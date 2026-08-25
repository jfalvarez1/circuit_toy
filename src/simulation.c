/**
 * Circuit Playground - Simulation Engine Implementation
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "simulation.h"
#include "logic.h"
#include "component.h"

// External subcircuit library
extern SubCircuitLibrary g_subcircuit_library;

// Global counter for allocating subcircuit internal node indices during stamping
int g_subcircuit_internal_node_offset = 0;

// Helper to count internal nodes needed for a subcircuit definition
// Returns the max internal node ID found (excluding pin nodes)
static int subcircuit_count_internal_nodes(SubCircuitDef *def) {
    if (!def || !def->component_data || def->num_components == 0) {
        return 0;
    }

    int max_node_id = 0;
    Component *internal_comps = (Component *)def->component_data;

    // Find max node ID used by internal components
    for (int i = 0; i < def->num_components; i++) {
        Component *ic = &internal_comps[i];
        for (int t = 0; t < ic->num_terminals && t < MAX_TERMINALS; t++) {
            if (ic->node_ids[t] > max_node_id) {
                max_node_id = ic->node_ids[t];
            }
        }
    }

    return max_node_id;
}

// GMIN - minimum conductance added from each node to ground
// This stabilizes floating nodes and prevents singular matrices
// Equivalent to 1 TΩ resistance to ground
#define GMIN 1e-12

// Forward declarations
static void simulation_clamp_opamps(Circuit *circuit, Vector *solution);

Simulation *simulation_create(Circuit *circuit) {
    Simulation *sim = calloc(1, sizeof(Simulation));
    if (!sim) return NULL;

    sim->circuit = circuit;
    sim->state = SIM_STOPPED;
    sim->time = 0;
    sim->time_step = DEFAULT_TIME_STEP;
    sim->history_target_span = 0.1;  // until the scope says otherwise
    sim->speed = 1.0;

    // Initialize adaptive time-stepping (disabled by default - needs more tuning)
    sim->adaptive_enabled = false;
    sim->dt_target = DEFAULT_TIME_STEP;
    sim->dt_actual = DEFAULT_TIME_STEP;
    sim->error_estimate = 0.0;
    sim->step_rejections = 0;
    sim->total_step_rejections = 0;
    sim->adaptive_factor = 1.0;
    sim->saved_solution = NULL;

    return sim;
}

void simulation_free(Simulation *sim) {
    if (!sim) return;

    if (sim->solution) {
        vector_free(sim->solution);
    }
    if (sim->prev_solution) {
        vector_free(sim->prev_solution);
    }
    if (sim->saved_solution) {
        vector_free(sim->saved_solution);
    }

    free(sim);
}

void simulation_start(Simulation *sim) {
    if (sim) {
        sim->state = SIM_RUNNING;
    }
}

void simulation_pause(Simulation *sim) {
    if (sim) {
        sim->state = SIM_PAUSED;
    }
}

void simulation_stop(Simulation *sim) {
    if (sim) {
        sim->state = SIM_STOPPED;
    }
}

void simulation_reset(Simulation *sim) {
    if (!sim) return;

    sim->state = SIM_STOPPED;
    sim->time = 0;

    if (sim->solution) {
        vector_free(sim->solution);
        sim->solution = NULL;
    }
    if (sim->prev_solution) {
        vector_free(sim->prev_solution);
        sim->prev_solution = NULL;
        if (sim->prev_step_solution) vector_free(sim->prev_step_solution);
        sim->prev_step_solution = NULL;
        if (sim->last_linearization) vector_free(sim->last_linearization);
        sim->last_linearization = NULL;
    }
    if (sim->saved_solution) {
        vector_free(sim->saved_solution);
        sim->saved_solution = NULL;
    }

    sim->history_count = 0;
    sim->history_start = 0;
    sim->history_decimate_counter = 0;
    sim->history_decimate_factor = 0;  // 0 means "not yet calculated" - will be set when dt is valid
    sim->has_error = false;
    sim->error_msg[0] = '\0';

    // Reset adaptive time-stepping state
    sim->dt_actual = sim->time_step;
    sim->dt_target = sim->time_step;
    sim->error_estimate = 0.0;
    sim->step_rejections = 0;
    sim->total_step_rejections = 0;
    sim->adaptive_factor = 1.0;

    // Reset node voltages and component state
    if (sim->circuit) {
        for (int i = 0; i < sim->circuit->num_nodes; i++) {
            sim->circuit->nodes[i].voltage = 0;
        }
        for (int i = 0; i < sim->circuit->num_probes; i++) {
            sim->circuit->probes[i].voltage = 0;
        }

        // Reset component state variables (fuses, capacitors, etc.)
        for (int i = 0; i < sim->circuit->num_components; i++) {
            Component *comp = sim->circuit->components[i];
            if (!comp) continue;

            switch (comp->type) {
                case COMP_FUSE:
                    // Reset fuse to intact state
                    comp->props.fuse.blown = false;
                    comp->props.fuse.i2t_accumulated = 0.0;
                    comp->props.fuse.current = 0.0;
                    comp->props.fuse.blow_time = -1.0;
                    break;

                case COMP_CAPACITOR:
                case COMP_CAPACITOR_ELEC:
                    // Reset capacitor voltage
                    comp->props.capacitor.voltage = 0.0;
                    break;

                case COMP_INDUCTOR:
                    // Reset inductor current
                    comp->props.inductor.current = 0.0;
                    break;

                case COMP_BATTERY:
                    // Reset battery charge to full
                    comp->props.battery.charge_state = 1.0;  // Full charge
                    comp->props.battery.charge_coulombs = comp->props.battery.capacity_mah * 3.6;
                    comp->props.battery.discharged = false;
                    break;

                case COMP_LED_ARRAY:
                    // Reset LED Array - repair all burned out segments
                    for (int seg = 0; seg < 8; seg++) {
                        comp->props.led_array.failed[seg] = false;
                        comp->props.led_array.currents[seg] = 0.0;
                    }
                    break;

                default:
                    break;
            }
        }
    }
}

static void simulation_set_error(Simulation *sim, const char *msg) {
    if (sim) {
        strncpy(sim->error_msg, msg, sizeof(sim->error_msg) - 1);
        sim->error_msg[sizeof(sim->error_msg) - 1] = '\0';
        sim->has_error = true;
    }
}

// NOTE: The BFS function nodes_connected_via_wires was removed because it caused
// false positives in short circuit detection for parallel resistor circuits.
// The union-find based node_map check is sufficient and more accurate.

// Detect short circuits: voltage sources with both terminals at same node
// Returns true if a short circuit was detected
static bool simulation_detect_short_circuit(Simulation *sim) {
    if (!sim || !sim->circuit) return false;

    Circuit *circuit = sim->circuit;
    sim->has_short_circuit = false;
    sim->short_circuit_count = 0;

    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        if (!comp) continue;

        // Check voltage sources (DC, AC, battery) for shorted terminals
        bool is_voltage_source = (comp->type == COMP_DC_VOLTAGE ||
                                   comp->type == COMP_AC_VOLTAGE ||
                                   comp->type == COMP_BATTERY);

        if (is_voltage_source && comp->num_terminals >= 2) {
            int n0 = comp->node_ids[0];
            int n1 = comp->node_ids[1];
            bool is_shorted = false;

            // Method 1: Check if both terminals map to the same node via node_map
            if (n0 >= 0 && n1 >= 0 && n0 < MAX_NODES && n1 < MAX_NODES) {
                int mapped0 = circuit->node_map[n0];
                int mapped1 = circuit->node_map[n1];

                // Short circuit if both terminals at same node (including both at ground)
                if (mapped0 == mapped1) {
                    is_shorted = true;
                }
            }

            // Note: Method 2 (BFS through wires) was removed because it caused false positives
            // for valid parallel resistor circuits. The node_map check above should be sufficient
            // since it uses union-find which properly handles transitive wire connections.

            if (is_shorted) {
                // Short circuit detected!
                sim->has_short_circuit = true;
                if (sim->short_circuit_count < 8) {
                    sim->short_circuit_comp_ids[sim->short_circuit_count++] = comp->id;
                }
            }
        }
    }

    return sim->has_short_circuit;
}

// Detect open circuit current sources by checking for excessive node voltages
// An open current source has no path for current, causing voltage to go to infinity
static bool simulation_detect_open_current_source(Simulation *sim) {
    if (!sim || !sim->circuit) return false;

    Circuit *circuit = sim->circuit;
    sim->has_open_circuit = false;
    sim->open_circuit_count = 0;
    const double OPEN_VOLTAGE_THRESHOLD = 1e6;  // 1MV indicates open circuit

    // Check all nodes for excessive voltage
    for (int i = 0; i < circuit->num_nodes; i++) {
        Node *node = &circuit->nodes[i];
        if (node->id <= 0) continue;  // Skip invalid/unused nodes

        if (fabs(node->voltage) > OPEN_VOLTAGE_THRESHOLD) {
            // Excessive voltage - find current sources connected to this node
            for (int j = 0; j < circuit->num_components; j++) {
                Component *comp = circuit->components[j];
                if (!comp) continue;

                bool is_current_source = (comp->type == COMP_DC_CURRENT ||
                                          comp->type == COMP_AC_CURRENT);

                if (is_current_source && comp->num_terminals >= 2) {
                    // Check if this current source is connected to the high-voltage node
                    int n0 = comp->node_ids[0];
                    int n1 = comp->node_ids[1];

                    if (n0 == node->id || n1 == node->id) {
                        // Open circuit current source detected!
                        sim->has_open_circuit = true;
                        if (sim->open_circuit_count < 8) {
                            // Check if not already added
                            bool already_added = false;
                            for (int k = 0; k < sim->open_circuit_count; k++) {
                                if (sim->open_circuit_comp_ids[k] == comp->id) {
                                    already_added = true;
                                    break;
                                }
                            }
                            if (!already_added) {
                                sim->open_circuit_comp_ids[sim->open_circuit_count++] = comp->id;
                            }
                        }
                    }
                }
            }
        }
    }

    return sim->has_open_circuit;
}

// Detect excessive current indicating a short circuit (e.g., no resistance in path)
// Check ammeter readings after simulation converges - current > 100A indicates short
// Also identify voltage sources connected to high-current ammeters
static bool simulation_detect_excessive_current(Simulation *sim) {
    if (!sim || !sim->circuit) return false;

    Circuit *circuit = sim->circuit;
    const double SHORT_CURRENT_THRESHOLD = 100.0;  // 100A threshold for short detection

    // First pass: find ammeters with excessive current
    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        if (!comp) continue;

        if (comp->type == COMP_AMMETER) {
            double current = fabs(comp->props.ammeter.reading);

            if (current > SHORT_CURRENT_THRESHOLD) {
                // Excessive current detected - this is a short circuit
                sim->has_short_circuit = true;

                // Add the ammeter to the short circuit list
                if (sim->short_circuit_count < 8) {
                    sim->short_circuit_comp_ids[sim->short_circuit_count++] = comp->id;
                }

                // Find voltage sources in the circuit and add them too
                // (they're the source of the excessive current)
                for (int j = 0; j < circuit->num_components; j++) {
                    Component *src = circuit->components[j];
                    if (!src) continue;

                    bool is_voltage_source = (src->type == COMP_DC_VOLTAGE ||
                                               src->type == COMP_AC_VOLTAGE ||
                                               src->type == COMP_BATTERY);

                    if (is_voltage_source && sim->short_circuit_count < 8) {
                        // Check if this source isn't already in the list
                        bool already_added = false;
                        for (int k = 0; k < sim->short_circuit_count; k++) {
                            if (sim->short_circuit_comp_ids[k] == src->id) {
                                already_added = true;
                                break;
                            }
                        }
                        if (!already_added) {
                            sim->short_circuit_comp_ids[sim->short_circuit_count++] = src->id;
                        }
                    }
                }
            }
        }
    }

    return sim->has_short_circuit;
}

bool simulation_dc_analysis(Simulation *sim) {
    if (!sim || !sim->circuit) {
        simulation_set_error(sim, "No circuit");
        return false;
    }

    Circuit *circuit = sim->circuit;

    if (circuit->num_components == 0) {
        simulation_set_error(sim, "No components in circuit");
        return false;
    }

    // Check for ground
    bool has_ground = false;
    for (int i = 0; i < circuit->num_components; i++) {
        if (circuit->components[i]->type == COMP_GROUND) {
            has_ground = true;
            // Set the ground node
            Node *gnd_node = circuit_get_node(circuit, circuit->components[i]->node_ids[0]);
            if (gnd_node) {
                circuit_set_ground(circuit, gnd_node->id);
            }
            break;
        }
    }

    if (!has_ground) {
        simulation_set_error(sim, "No ground reference in circuit");
        return false;
    }

    // Build node map
    circuit_build_node_map(circuit);

    // Check for short circuits before proceeding
    if (simulation_detect_short_circuit(sim)) {
        simulation_set_error(sim, "SHORT! Voltage source terminals shorted");
        return false;
    }

    int num_nodes = circuit->num_matrix_nodes;
    if (num_nodes == 0) {
        simulation_set_error(sim, "No nodes to solve");
        return false;
    }

    // Count voltage variables
    int num_volt_vars = 0;
    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        if (comp->needs_voltage_var) {
            comp->voltage_var_idx = num_volt_vars;
            num_volt_vars += component_aux_count(comp);   // 1 for most, 3 for the three-phase source
        }
    }

    // Count subcircuit internal nodes needed
    // Each subcircuit instance needs space for internal nodes not exposed as pins
    int num_subcircuit_internal = 0;
    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        if (comp->type == COMP_SUBCIRCUIT) {
            // Find the definition
            for (int d = 0; d < g_subcircuit_library.count; d++) {
                if (g_subcircuit_library.defs[d].id == comp->props.subcircuit.def_id) {
                    int max_internal = subcircuit_count_internal_nodes(&g_subcircuit_library.defs[d]);
                    num_subcircuit_internal += max_internal + 1;  // +1 for safety
                    break;
                }
            }
        }
    }

    int matrix_size = num_nodes + num_volt_vars + num_subcircuit_internal;
    sim->solution_size = matrix_size;

    // Store base offset for subcircuit internal nodes (after voltage variables)
    g_subcircuit_internal_node_offset = num_nodes + num_volt_vars;

    // Iterative solution for nonlinear components
    Vector *solution = vector_create(matrix_size);
    if (!solution) {
        simulation_set_error(sim, "Memory allocation failed");
        return false;
    }

    bool converged = false;

    for (int iter = 0; iter < MAX_ITERATIONS; iter++) {
        Matrix *A = matrix_create(matrix_size, matrix_size);
        Vector *b = vector_create(matrix_size);

        if (!A || !b) {
            matrix_free(A);
            vector_free(b);
            vector_free(solution);
            simulation_set_error(sim, "Memory allocation failed");
            return false;
        }

        // Clear wireless state for antenna TX/RX pairs
        memset(&g_wireless, 0, sizeof(g_wireless));

        // Reset subcircuit internal node offset for this iteration
        // (subcircuit stamping increments this, so we must reset each pass)
        g_subcircuit_internal_node_offset = num_nodes + num_volt_vars;

        // Stamp all components
        // Use large dt for DC analysis so capacitors → open circuit, inductors → short circuit
        double dc_dt = 1e9;  // Very large dt for steady-state DC behavior
        for (int i = 0; i < circuit->num_components; i++) {
            // Pass NULL for first iteration so components use initial guess instead of zeros
            Vector *prev_sol = (iter == 0) ? NULL : solution;
            component_stamp(circuit->components[i], A, b,
                           circuit->node_map, num_nodes,
                           0, prev_sol, dc_dt);
        }

        // Add GMIN (minimum conductance) from each node to ground
        // This stabilizes floating nodes and prevents singular matrices
        for (int i = 0; i < num_nodes; i++) {
            matrix_add(A, i, i, GMIN);
        }

        // Solve
        Vector *new_solution = linear_solve(A, b);
        matrix_free(A);
        vector_free(b);

        if (!new_solution) {
            vector_free(solution);
            simulation_set_error(sim, "Matrix solver failed");
            return false;
        }

        // Check convergence
        double max_diff = 0;
        for (int i = 0; i < matrix_size; i++) {
            double diff = fabs(vector_get(new_solution, i) - vector_get(solution, i));
            if (diff > max_diff) max_diff = diff;
        }

        vector_free(solution);
        solution = new_solution;

        if (max_diff < CONVERGENCE_TOL) {
            converged = true;
            break;
        }
    }

    if (!converged) {
        // Still use the solution, but warn
        simulation_set_error(sim, "Warning: solution may not have converged");
    }

    // Store solution
    if (sim->solution) vector_free(sim->solution);
    if (sim->prev_solution) vector_free(sim->prev_solution);

    sim->solution = solution;
    sim->prev_solution = vector_clone(solution);

    // Apply post-solve clamping as safety net
    simulation_clamp_opamps(circuit, sim->solution);

    // Capacitors carry no current at the operating point; swept sources restart their phase
    for (int i = 0; i < circuit->num_components; i++) {
        circuit->components[i]->trap_i_prev = 0.0;
        circuit->components[i]->tline_ic_prev[0] = circuit->components[i]->tline_ic_prev[1] = 0.0;
        if (circuit->components[i]->type == COMP_SPARK_GAP) circuit->components[i]->props.spark_gap.conducting = false;
        circuit->components[i]->sweep_phase = 0.0;
    }

    // Update circuit voltages, meter readings and the current-flow display
    circuit_update_voltages(circuit, solution);
    circuit_update_meter_readings(circuit);
    simulation_update_flow_display(sim);

    // Check for excessive current indicating short circuit (after meter readings updated)
    if (simulation_detect_excessive_current(sim)) {
        simulation_set_error(sim, "Short circuit: excessive current (>100A) detected!");
        return false;
    }

    // Check for open circuit current sources (excessive voltage)
    if (simulation_detect_open_current_source(sim)) {
        simulation_set_error(sim, "Open circuit: current source has no load path!");
        return false;
    }

    sim->has_error = false;
    return true;
}

// Helper function to perform a single Newton-Raphson solve iteration
// Returns the new solution vector, or NULL on failure
static Vector *simulation_solve_step(Simulation *sim, double dt) {
    Circuit *circuit = sim->circuit;
    int num_nodes = circuit->num_matrix_nodes;
    int matrix_size = sim->solution_size;

    Vector *current_solution = vector_clone(sim->solution);
    if (!current_solution) return NULL;

    for (int i = 0; i < circuit->num_components; i++) {
        circuit->components[i]->sat_last_rail = 0;
        circuit->components[i]->sat_flips = 0;
    }

    for (int iter = 0; iter < MAX_ITERATIONS; iter++) {
        Matrix *A = matrix_create(matrix_size, matrix_size);
        Vector *b = vector_create(matrix_size);

        if (!A || !b) {
            matrix_free(A);
            vector_free(b);
            vector_free(current_solution);
            return NULL;
        }

        // Clear wireless state for antenna TX/RX pairs
        memset(&g_wireless, 0, sizeof(g_wireless));

        // Reset subcircuit internal node offset for this iteration
        // Count voltage variables to compute base offset
        int num_volt_vars = 0;
        for (int i = 0; i < circuit->num_components; i++) {
            if (circuit->components[i]->needs_voltage_var) {
                num_volt_vars += component_aux_count(circuit->components[i]);
            }
        }
        g_subcircuit_internal_node_offset = num_nodes + num_volt_vars;

        // Stamp components
        for (int i = 0; i < circuit->num_components; i++) {
            component_stamp(circuit->components[i], A, b,
                           circuit->node_map, num_nodes,
                           sim->time, current_solution, dt);
        }

        // Add GMIN (minimum conductance) from each node to ground
        for (int i = 0; i < num_nodes; i++) {
            matrix_add(A, i, i, GMIN);
        }

        Vector *new_solution = linear_solve(A, b);
        matrix_free(A);
        vector_free(b);

        if (!new_solution) {
            vector_free(current_solution);
            return NULL;
        }

        // Check convergence
        double max_diff = 0;
        for (int i = 0; i < matrix_size; i++) {
            double diff = fabs(vector_get(new_solution, i) - vector_get(current_solution, i));
            if (diff > max_diff) max_diff = diff;
        }

        // Keep the linearization point of this solve: the display-side terminal currents
        // are evaluated there so they satisfy KCL exactly for the solved voltages.
        if (sim->last_linearization) vector_free(sim->last_linearization);
        sim->last_linearization = current_solution;
        current_solution = new_solution;

        if (max_diff < CONVERGENCE_TOL) {
            break;
        }
    }

    return current_solution;
}

// Estimate the local truncation error based on change in solution
// Returns the maximum relative change across all node voltages
static double simulation_estimate_error(Simulation *sim, Vector *new_solution) {
    if (!sim->prev_solution || !new_solution) return 0.0;

    int matrix_size = sim->solution_size;
    double max_rel_change = 0.0;

    for (int i = 0; i < matrix_size; i++) {
        double old_val = vector_get(sim->prev_solution, i);
        double new_val = vector_get(new_solution, i);
        double change = fabs(new_val - old_val);

        // Use relative error with a minimum reference to avoid division by near-zero
        double ref = fmax(fabs(old_val), fabs(new_val));
        if (ref < 1e-6) ref = 1e-6;  // Minimum reference voltage of 1µV

        double rel_change = change / ref;
        if (rel_change > max_rel_change) {
            max_rel_change = rel_change;
        }
    }

    return max_rel_change;
}

// Update thermal state for all components - calculates temperature rise and damage
static void thermal_update_components(Circuit *circuit, double dt, double sim_time) {
    if (!circuit) return;

    for (int i = 0; i < circuit->num_components; i++) {
        Component *c = circuit->components[i];
        if (!c) continue;

        // Skip components without thermal modeling
        if (c->thermal.max_temperature <= 0) continue;

        // Get power dissipation based on component type
        double power = 0.0;
        switch (c->type) {
            case COMP_RESISTOR:
                // P = V * I (already tracked in component)
                power = c->thermal.power_dissipated;
                break;
            case COMP_NPN_BJT:
            case COMP_PNP_BJT:
                power = c->thermal.power_dissipated;
                break;
            case COMP_NMOS:
            case COMP_PMOS:
                power = c->thermal.power_dissipated;
                break;
            case COMP_CAPACITOR:
                // Capacitors dissipate power through ESR
                power = c->thermal.power_dissipated;
                break;
            case COMP_LED:
                power = c->thermal.power_dissipated;
                break;
            case COMP_DIODE:
            case COMP_ZENER:
            case COMP_SCHOTTKY:
                power = c->thermal.power_dissipated;
                break;
            default:
                continue;  // No thermal model for this component
        }

        // Skip if already failed
        if (c->thermal.failed) {
            // Update smoke particles
            if (c->thermal.smoke_active) {
                for (int s = 0; s < c->thermal.num_smoke; s++) {
                    SmokeParticle *p = &c->thermal.smoke[s];
                    // Move particle upward with some randomness
                    p->vy -= 0.5f * (float)dt;  // Gravity affects rising smoke
                    p->x += p->vx * (float)dt;
                    p->y += p->vy * (float)dt;
                    p->life -= (float)dt * 0.5f;  // Decay over ~2 seconds
                    p->alpha = (uint8_t)(p->life * 200);
                    p->size += (float)dt * 2.0f;  // Expand as it rises
                }
                // Remove dead particles
                int alive = 0;
                for (int s = 0; s < c->thermal.num_smoke; s++) {
                    if (c->thermal.smoke[s].life > 0) {
                        if (alive != s) {
                            c->thermal.smoke[alive] = c->thermal.smoke[s];
                        }
                        alive++;
                    }
                }
                c->thermal.num_smoke = alive;
                if (alive == 0) {
                    c->thermal.smoke_active = false;
                }
            }
            continue;
        }

        // Store power for thermal visualization
        c->thermal.power_dissipated = power;

        // Calculate temperature change using thermal model
        // dT/dt = (P - (T - T_ambient) / R_thermal) / C_thermal
        double thermal_resistance = c->thermal.thermal_resistance;
        double thermal_mass = c->thermal.thermal_mass;
        // Use global environment temperature for ambient
        double ambient = g_environment.temperature;

        if (thermal_mass > 0) {
            double heat_in = power;  // Power dissipation heats up
            double heat_out = (c->thermal.temperature - ambient) / thermal_resistance;  // Cooling
            double dT = (heat_in - heat_out) * dt / thermal_mass;
            c->thermal.temperature += dT;

            // Clamp to reasonable range
            if (c->thermal.temperature < ambient) {
                c->thermal.temperature = ambient;
            }
        }

        // Calculate power rating based on component type
        double power_rating = 0.25;  // Default 1/4W for resistors
        switch (c->type) {
            case COMP_RESISTOR:
                power_rating = 0.25;  // 1/4W typical through-hole
                break;
            case COMP_NPN_BJT:
            case COMP_PNP_BJT:
                power_rating = 0.625;  // 625mW for small signal TO-92
                break;
            case COMP_LED:
                power_rating = 0.1;  // 100mW typical LED
                break;
            default:
                power_rating = 0.5;
                break;
        }

        // Accumulate damage if over temperature or power limit
        double damage_rate = 0.0;

        // Temperature-based damage
        if (c->thermal.temperature > c->thermal.max_temperature) {
            double over_temp = c->thermal.temperature - c->thermal.max_temperature;
            damage_rate = over_temp / 50.0;  // Full damage in ~50°C over limit
        }

        // Power-based damage (exceeding rated power)
        if (power > power_rating * c->thermal.damage_threshold) {
            double over_power = (power - power_rating) / power_rating;
            damage_rate = fmax(damage_rate, over_power * 0.5);  // Scale with overpower
        }

        // Accumulate damage over time
        if (damage_rate > 0) {
            c->thermal.damage += damage_rate * dt;

            // Component fails when damage reaches 1.0
            if (c->thermal.damage >= 1.0) {
                c->thermal.damage = 1.0;
                c->thermal.failed = true;
                c->thermal.failure_time = sim_time;
                c->thermal.smoke_active = true;

                // Spawn initial smoke particles
                c->thermal.num_smoke = MAX_SMOKE_PARTICLES;
                for (int s = 0; s < MAX_SMOKE_PARTICLES; s++) {
                    SmokeParticle *p = &c->thermal.smoke[s];
                    p->x = (float)(rand() % 20 - 10);  // Random offset
                    p->y = (float)(rand() % 10 - 5);
                    p->vx = (float)(rand() % 20 - 10) * 0.5f;
                    p->vy = (float)(rand() % 10 + 10) * -2.0f;  // Rise upward
                    p->life = 1.0f + (float)(rand() % 50) / 100.0f;
                    p->size = 3.0f + (float)(rand() % 5);
                    p->alpha = 200;
                }
            }
        }
    }
}

// Post-solve hard clamping for opamp outputs
// This clamps opamp outputs to their rail voltages after the solver completes.
// Unlike soft clamping during matrix assembly, this approach:
// 1. Lets the solver work without numerical instability
// 2. Hard clamps outputs regardless of how far they've diverged
// 3. Works correctly for high-gain positive feedback circuits (oscillators)
static void simulation_clamp_opamps(Circuit *circuit, Vector *solution) {
    if (!circuit || !solution) return;

    // Voltage-source auxiliary variables live after all node voltages
    int num_nodes = circuit->num_matrix_nodes;

    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];

        // All VCVS-style op-amp models: ideal, flipped-symbol and realistic
        if (comp->type != COMP_OPAMP && comp->type != COMP_OPAMP_FLIPPED &&
            comp->type != COMP_OPAMP_REAL) continue;

        // Output is terminal 2 for every op-amp symbol. node_ids[] holds circuit node IDs;
        // the solution vector is indexed by matrix index, so map through node_map.
        int out_id = comp->node_ids[2];
        if (out_id < 0 || out_id >= MAX_NODES) continue;
        int out_idx = circuit->node_map[out_id];
        if (out_idx <= 0) continue;  // Unconnected or ground

        // Get rail voltages from opamp properties
        double vmax = comp->props.opamp.vmax;
        double vmin = comp->props.opamp.vmin;
        if (!(vmax > vmin)) continue;  // Rails not configured

        // Get the opamp's voltage variable index (VCVS auxiliary variable)
        // This is stored after all nodes in the solution vector
        int volt_var_idx = num_nodes + comp->voltage_var_idx;
        if (comp->voltage_var_idx < 0 || volt_var_idx >= solution->size) continue;
        if (out_idx - 1 >= solution->size) continue;

        double v_out_var = vector_get(solution, out_idx - 1);
        // Clamp BOTH to the same rail value to keep them consistent
        double clamped_value = v_out_var;  // Use voltage variable as source of truth
        if (v_out_var > vmax) {
            clamped_value = vmax;
        } else if (v_out_var < vmin) {
            clamped_value = vmin;
        }

        // Apply clamping to BOTH the node and the voltage variable
        if (v_out_var > vmax || v_out_var < vmin) {
            vector_set(solution, volt_var_idx, clamped_value);
            vector_set(solution, out_idx - 1, clamped_value);
        }
    }
}

// Decimation factor that makes MAX_HISTORY samples cover history_target_span, limited so the
// fastest fixed-frequency source still gets >= 20 recorded samples per cycle.
static int simulation_compute_decimation(Simulation *sim) {
    Circuit *circuit = sim->circuit;
    double raw_span = MAX_HISTORY * sim->time_step;
    int factor = 1;
    if (sim->history_target_span > raw_span && sim->time_step > 0) {
        factor = (int)ceil(sim->history_target_span / raw_span);
        if (factor < 1) factor = 1;
        if (factor > 1000000) factor = 1000000;
    }

    double max_freq = 0;
    for (int i = 0; i < circuit->num_components; i++) {
        Component *c = circuit->components[i];
        if (!c) continue;
        double freq = 0;
        switch (c->type) {
            case COMP_AC_VOLTAGE:
                freq = c->props.ac_voltage.frequency;
                if (c->props.ac_voltage.frequency_sweep.enabled) {
                    if (c->props.ac_voltage.frequency_sweep.start_value > freq) freq = c->props.ac_voltage.frequency_sweep.start_value;
                    if (c->props.ac_voltage.frequency_sweep.end_value > freq) freq = c->props.ac_voltage.frequency_sweep.end_value;
                }
                break;
            case COMP_SQUARE_WAVE: freq = c->props.square_wave.frequency; break;
            case COMP_TRIANGLE_WAVE: freq = c->props.triangle_wave.frequency; break;
            case COMP_SAWTOOTH_WAVE: freq = c->props.sawtooth_wave.frequency; break;
            default: break;
        }
        if (freq > max_freq) max_freq = freq;
    }
    if (max_freq > 0) {
        double max_effective_dt = (1.0 / max_freq) / 20.0;
        int max_decimate = (int)(max_effective_dt / sim->time_step);
        if (max_decimate < 1) max_decimate = 1;
        if (factor > max_decimate) factor = max_decimate;
    }
    return factor;
}

void simulation_set_history_span(Simulation *sim, double span_seconds) {
    if (!sim || !sim->circuit || span_seconds <= 0) return;
    if (fabs(span_seconds - sim->history_target_span) < 1e-15) return;
    sim->history_target_span = span_seconds;
    if (sim->history_decimate_factor > 0 && sim->time_step >= 1e-9 &&
        simulation_compute_decimation(sim) != sim->history_decimate_factor) {
        sim->history_decimate_factor = 0;   // recompute (and reset history) on the next step
    }
}

// ---------------------------------------------------------------------------
// Current-flow display support
// ---------------------------------------------------------------------------

static void clear_row(Matrix *A, Vector *b, int row) {
    if (row < 0 || row >= A->rows) return;
    for (int j = 0; j < A->cols; j++) matrix_set(A, row, j, 0.0);
    vector_set(b, row, 0.0);
}

// Re-stamp each component alone at the converged solution and read its KCL residual:
//   r = A_k * x - b_k  ->  r[node row] = current leaving that node into the component.
// Storage elements use prev_step_solution as their memory, so the capacitor current is the
// true C*dv/dt of the last step. Aux (voltage-source) rows are ignored; a single grounded
// terminal gets the negated sum of the others (charge conservation).
void simulation_compute_terminal_currents(Simulation *sim) {
    if (!sim || !sim->circuit || !sim->solution) return;
    Circuit *circuit = sim->circuit;
    int M = sim->solution_size;
    int num_nodes = circuit->num_matrix_nodes;
    if (M <= 0 || sim->solution->size != M) return;

    Matrix *A = matrix_create(M, M);
    Vector *b = vector_create(M);
    if (!A || !b) { matrix_free(A); vector_free(b); return; }

    int num_volt_vars = 0;
    for (int i = 0; i < circuit->num_components; i++)
        if (circuit->components[i]->needs_voltage_var) num_volt_vars++;
    g_subcircuit_internal_node_offset = num_nodes + num_volt_vars;

    double dt = (sim->dt_actual > 0) ? sim->dt_actual : sim->time_step;
    if (!sim->prev_step_solution) dt = 1e9;      // DC operating point: storage elements idle
    g_stamp_prev_step = sim->prev_step_solution;

    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        for (int t = 0; t < MAX_TERMINALS; t++) comp->terminal_current[t] = 0.0;
        if (comp->type == COMP_GROUND || comp->type == COMP_TEXT || comp->type == COMP_LABEL ||
            comp->type == COMP_PIN || comp->type == COMP_SUBCIRCUIT || comp->num_terminals < 2)
            continue;

        if ((comp->type == COMP_CAPACITOR || comp->type == COMP_CAPACITOR_ELEC) && sim->prev_step_solution) {
            comp->terminal_current[0] = comp->trap_i_prev;
            comp->terminal_current[1] = -comp->trap_i_prev;
            continue;
        }
        if (comp->type == COMP_TLINE && sim->prev_step_solution) {
            // series current from the auxiliary variable plus the shunt-capacitor current at each end
            int r = num_nodes + comp->voltage_var_idx;
            double i_s = (r < M) ? vector_get(sim->solution, r) : 0.0;
            comp->terminal_current[0] = i_s + comp->tline_ic_prev[0];
            comp->terminal_current[1] = -i_s + comp->tline_ic_prev[1];
            continue;
        }

        // Rows this component can touch
        int rows[MAX_TERMINALS + 4]; int nrows = 0;
        for (int t = 0; t < comp->num_terminals; t++) {
            int id = comp->node_ids[t];
            int idx = (id >= 0 && id < MAX_NODES) ? circuit->node_map[id] : 0;
            if (idx > 0) rows[nrows++] = idx - 1;
        }
        if (comp->needs_voltage_var)
            for (int k = 0; k < component_aux_count(comp); k++) {   // only this component's own aux rows
                int r = num_nodes + comp->voltage_var_idx + k;
                if (r < M) rows[nrows++] = r;
            }
        for (int k = 0; k < nrows; k++) clear_row(A, b, rows[k]);

        Vector *lin = (sim->last_linearization && sim->last_linearization->size == M)
                      ? sim->last_linearization : sim->solution;
        component_stamp(comp, A, b, circuit->node_map, num_nodes, sim->time - dt, lin, dt);   /* the accepted step was stamped before time advanced */

        int ground_t = -1, ground_count = 0; double sum = 0.0;
        for (int t = 0; t < comp->num_terminals; t++) {
            int id = comp->node_ids[t];
            int idx = (id >= 0 && id < MAX_NODES) ? circuit->node_map[id] : 0;
            if (idx <= 0) { ground_t = t; ground_count++; continue; }
            // Terminals tied to the same matrix node (e.g. diode-connected BJT) share one row:
            // the row residual is the total current into the device from that node, so credit
            // it to the first such terminal only.
            int dup = 0;
            for (int u = 0; u < t; u++) {
                int id2 = comp->node_ids[u];
                int idx2 = (id2 >= 0 && id2 < MAX_NODES) ? circuit->node_map[id2] : 0;
                if (idx2 == idx) { dup = 1; break; }
            }
            if (dup) continue;
            double r = -vector_get(b, idx - 1);
            for (int j = 0; j < M; j++) {
                double a = matrix_get(A, idx - 1, j);
                if (a != 0.0) r += a * vector_get(sim->solution, j);
            }
            comp->terminal_current[t] = r;
            sum += r;
        }
        if (ground_count == 1) comp->terminal_current[ground_t] = -sum;

        for (int k = 0; k < nrows; k++) clear_row(A, b, rows[k]);
    }

    g_stamp_prev_step = NULL;
    matrix_free(A);
    vector_free(b);
}

void simulation_update_flow_display(Simulation *sim) {
    if (!sim || !sim->circuit) return;
    simulation_compute_terminal_currents(sim);
    circuit_update_wire_currents(sim->circuit);
}

bool simulation_step(Simulation *sim) {
    if (!sim || !sim->circuit) return false;

    Circuit *circuit = sim->circuit;

    // Ensure we have a solution (run DC analysis if needed)
    if (!sim->solution) {
        if (!simulation_dc_analysis(sim)) {
            return false;
        }
        // Initialize adaptive state after DC analysis
        sim->dt_target = sim->time_step;
        sim->dt_actual = sim->time_step;
    }

    // Reset per-frame rejection counter
    sim->step_rejections = 0;

    // Current time step to try
    double dt = sim->adaptive_enabled ? sim->dt_actual : sim->time_step;
    double dt_new = dt;

    // Maximum retries to prevent infinite loops
    int max_retries = 10;
    int retries = 0;

    while (retries < max_retries) {
        // Save the current solution in case we need to reject this step
        if (sim->saved_solution) {
            vector_free(sim->saved_solution);
        }
        sim->saved_solution = vector_clone(sim->solution);

        // Attempt a solve with current dt
        g_stamp_prev_step = sim->solution;   // memory terms: previous accepted step
        Vector *trial_solution = simulation_solve_step(sim, dt);
        g_stamp_prev_step = NULL;

        if (!trial_solution) {
            // Solver failed - halve dt and retry
            if (sim->adaptive_enabled) {
                dt *= ADAPTIVE_MIN_FACTOR;
                dt = fmax(dt, MIN_TIME_STEP);
                retries++;
                sim->step_rejections++;
                sim->total_step_rejections++;
                continue;
            } else {
                simulation_set_error(sim, "Matrix solver failed");
                return false;
            }
        }

        // Estimate error from solution change
        double error = simulation_estimate_error(sim, trial_solution);
        sim->error_estimate = error;

        if (sim->adaptive_enabled) {
            if (error > ADAPTIVE_ERROR_TOL) {
                // Error too large - reject step, halve dt, and retry
                vector_free(trial_solution);

                // Restore the saved solution
                vector_free(sim->solution);
                sim->solution = vector_clone(sim->saved_solution);

                // Reduce time step
                double factor = ADAPTIVE_SAFETY_FACTOR * sqrt(ADAPTIVE_ERROR_TOL / error);
                factor = fmax(factor, ADAPTIVE_MIN_FACTOR);
                dt *= factor;
                dt = fmax(dt, MIN_TIME_STEP);

                retries++;
                sim->step_rejections++;
                sim->total_step_rejections++;
                continue;
            }

            // Step accepted - compute new dt for next step
            if (error < ADAPTIVE_STEADY_THRESHOLD) {
                // Circuit is very steady - increase dt gradually
                dt_new = dt * ADAPTIVE_MAX_FACTOR;
            } else if (error < ADAPTIVE_ERROR_TOL * 0.5) {
                // Error is comfortably below tolerance - increase dt
                double factor = ADAPTIVE_SAFETY_FACTOR * sqrt(ADAPTIVE_ERROR_TOL / error);
                factor = fmin(factor, ADAPTIVE_MAX_FACTOR);
                dt_new = dt * factor;
            } else {
                // Error is close to tolerance - keep dt similar
                dt_new = dt;
            }

            // Clamp to valid range
            dt_new = fmax(dt_new, MIN_TIME_STEP);
            dt_new = fmin(dt_new, MAX_TIME_STEP);
            // Don't exceed the target/nominal time step by too much
            dt_new = fmin(dt_new, sim->dt_target * ADAPTIVE_MAX_FACTOR * 2.0);
        }

        // Accept the step; keep the old solution as the previous-step state
        if (sim->prev_step_solution) vector_free(sim->prev_step_solution);
        sim->prev_step_solution = sim->solution;
        sim->solution = trial_solution;

        // Swept sources: advance the accumulated phase by the instantaneous frequency
        for (int i = 0; i < circuit->num_components; i++) {
            Component *comp = circuit->components[i];
            if (comp->type == COMP_AC_VOLTAGE && comp->props.ac_voltage.frequency_sweep.enabled) {
                double f = sweep_get_value(&comp->props.ac_voltage.frequency_sweep,
                                           comp->props.ac_voltage.frequency, sim->time);
                comp->sweep_phase += 2.0 * M_PI * f * dt;
                if (comp->sweep_phase > 1e6) comp->sweep_phase = fmod(comp->sweep_phase, 2.0 * M_PI);
            }
        }

        // Spark gaps: switch state from the accepted solution (never inside Newton)
        for (int i = 0; i < circuit->num_components; i++) {
            Component *comp = circuit->components[i];
            if (comp->type != COMP_SPARK_GAP) continue;
            int n0 = circuit->node_map[comp->node_ids[0]], n1 = circuit->node_map[comp->node_ids[1]];
            double v = ((n0 > 0) ? vector_get(sim->solution, n0 - 1) : 0) - ((n1 > 0) ? vector_get(sim->solution, n1 - 1) : 0);
            if (!comp->props.spark_gap.conducting) {
                if (fabs(v) > spark_gap_breakdown(comp)) {
                    comp->props.spark_gap.conducting = true;
                    comp->props.spark_gap.last_conduct_time = sim->time;
                }
            } else {
                double i_arc = v / fmax(comp->props.spark_gap.r_on, 1e-3);
                if (fabs(i_arc) > comp->props.spark_gap.hold_current) comp->props.spark_gap.last_conduct_time = sim->time;
                else if (sim->time - comp->props.spark_gap.last_conduct_time > comp->props.spark_gap.quench_time)
                    comp->props.spark_gap.conducting = false;
            }
        }
        for (int i = 0; i < circuit->num_components; i++) {
            Component *comp = circuit->components[i];
            if (comp->type != COMP_TOROID) continue;
            int n0 = circuit->node_map[comp->node_ids[0]];
            comp->props.toroid.voltage = (n0 > 0) ? vector_get(sim->solution, n0 - 1) : 0;
        }

        // Transmission-line shunt capacitor state (theta method, see the TLINE stamp)
        for (int i = 0; i < circuit->num_components; i++) {
            Component *comp = circuit->components[i];
            if (comp->type != COMP_TLINE) continue;
            double R, L, Cend; tline_params(comp, &R, &L, &Cend);
            if (Cend <= 0) { comp->tline_ic_prev[0] = comp->tline_ic_prev[1] = 0; continue; }
            double Geq = Cend / (0.6 * dt);
            for (int e = 0; e < 2; e++) {
                int nn = circuit->node_map[comp->node_ids[e]];
                double vn = (nn > 0) ? vector_get(sim->solution, nn - 1) : 0, vp = (nn > 0) ? vector_get(sim->prev_step_solution, nn - 1) : 0;
                comp->tline_ic_prev[e] = Geq * (vn - vp) - (0.4 / 0.6) * comp->tline_ic_prev[e];
            }
        }

        // Trapezoidal capacitor state: i_new = (2C/dt)(v_new - v_prev) - i_prev
        for (int i = 0; i < circuit->num_components; i++) {
            Component *comp = circuit->components[i];
            if (comp->type != COMP_CAPACITOR && comp->type != COMP_CAPACITOR_ELEC && comp->type != COMP_TOROID) continue;
            double C = (comp->type == COMP_CAPACITOR) ? comp->props.capacitor.capacitance
                     : (comp->type == COMP_TOROID) ? toroid_capacitance(comp)
                                                      : comp->props.capacitor_elec.capacitance;
            int n0 = circuit->node_map[comp->node_ids[0]], n1 = (comp->type == COMP_TOROID) ? 0 : circuit->node_map[comp->node_ids[1]];
            double vn = ((n0 > 0) ? vector_get(sim->solution, n0 - 1) : 0) - ((n1 > 0) ? vector_get(sim->solution, n1 - 1) : 0);
            double vp = ((n0 > 0) ? vector_get(sim->prev_step_solution, n0 - 1) : 0) - ((n1 > 0) ? vector_get(sim->prev_step_solution, n1 - 1) : 0);
            comp->trap_i_prev = (C / (0.6 * dt)) * (vn - vp) - (0.4 / 0.6) * comp->trap_i_prev;   // theta = 0.6, see capacitor stamp
        }

        // Apply post-solve clamping as safety net (valid approach for educational simulators)
        // This prevents any remaining numerical drift from pushing outputs beyond rails
        simulation_clamp_opamps(circuit, sim->solution);

        break;
    }

    if (retries >= max_retries) {
        simulation_set_error(sim, "Adaptive stepping: too many retries");
        return false;
    }

    // Update for next step
    if (sim->prev_solution) vector_free(sim->prev_solution);
    sim->prev_solution = vector_clone(sim->solution);

    // Update adaptive state
    sim->dt_actual = dt;
    if (sim->adaptive_enabled) {
        sim->dt_actual = dt_new;  // Use new dt for next step
        sim->adaptive_factor = dt / sim->dt_target;
    } else {
        sim->adaptive_factor = 1.0;
    }

    sim->time += dt;

    // Update circuit voltages and meter readings (wire flow display is refreshed per frame)
    circuit_update_voltages(circuit, sim->solution);
    circuit_update_meter_readings(circuit);

    // Update thermal state for all components (magic smoke simulation)
    thermal_update_components(circuit, dt, sim->time);

    // Mixed-signal logic solver phase
    // 1. ADC: Sample analog node voltages and convert to logic states
    logic_sample_inputs(sim, circuit);
    // 2. Propagate logic through digital gates
    logic_propagate(circuit, sim->time, dt);
    // 3. DAC: Drive logic outputs to analog nodes
    logic_drive_outputs(sim, circuit);

    // History decimation: keep MAX_HISTORY samples spanning what the scope wants to show
    // (sim->history_target_span), but never fewer than ~20 samples per cycle of the fastest
    // fixed-frequency source. Recomputed only when the factor is invalidated (0), because
    // changing decimation mid-run makes sample spacing inconsistent.
    if (sim->history_decimate_factor == 0 && sim->time_step >= 1e-9) {
        sim->history_decimate_factor = simulation_compute_decimation(sim);

        // Reset history so all stored samples share one spacing
        sim->history_count = 0;
        sim->history_start = 0;
        sim->history_decimate_counter = 0;
    }

    // Skip history recording until decimation is calculated (factor > 0)
    if (sim->history_decimate_factor == 0) {
        return true;  // Continue simulation, but don't record invalid samples
    }

    // Record history only every N samples (decimation)
    sim->history_decimate_counter++;
    if (sim->history_decimate_counter >= sim->history_decimate_factor) {
        sim->history_decimate_counter = 0;

        int hist_idx = (sim->history_start + sim->history_count) % MAX_HISTORY;
        sim->history[hist_idx].time = sim->time;

        for (int i = 0; i < circuit->num_probes && i < MAX_PROBES; i++) {
            sim->history[hist_idx].values[i] = circuit->probes[i].voltage;
        }

        if (sim->history_count < MAX_HISTORY) {
            sim->history_count++;
        } else {
            sim->history_start = (sim->history_start + 1) % MAX_HISTORY;
        }
    }

    return true;
}

void simulation_set_speed(Simulation *sim, double speed) {
    if (sim) {
        sim->speed = CLAMP(speed, 0.1, 100.0);
    }
}

void simulation_set_time_step(Simulation *sim, double dt) {
    if (sim) {
        double new_dt = CLAMP(dt, MIN_TIME_STEP, MAX_TIME_STEP);
        if (new_dt != sim->time_step) {
            // Sample spacing changes: re-derive history decimation on the next step
            sim->history_decimate_factor = 0;
        }
        sim->time_step = new_dt;
        // Also update target for adaptive stepping
        sim->dt_target = sim->time_step;
        sim->dt_actual = sim->time_step;
    }
}

// Adaptive time-stepping control
void simulation_enable_adaptive(Simulation *sim, bool enable) {
    if (sim) {
        sim->adaptive_enabled = enable;
        if (!enable) {
            // Reset to fixed stepping
            sim->dt_actual = sim->time_step;
            sim->adaptive_factor = 1.0;
        }
    }
}

bool simulation_is_adaptive_enabled(Simulation *sim) {
    return sim ? sim->adaptive_enabled : false;
}

double simulation_get_adaptive_factor(Simulation *sim) {
    return sim ? sim->adaptive_factor : 1.0;
}

int simulation_get_step_rejections(Simulation *sim) {
    return sim ? sim->step_rejections : 0;
}

double simulation_get_error_estimate(Simulation *sim) {
    return sim ? sim->error_estimate : 0.0;
}

double simulation_accuracy_time_step(Simulation *sim) {
    if (!sim || !sim->circuit) return DEFAULT_TIME_STEP;

    // Find the highest frequency signal in the circuit
    double max_freq = 0;

    for (int i = 0; i < sim->circuit->num_components; i++) {
        Component *c = sim->circuit->components[i];
        if (!c) continue;

        double freq = 0;
        switch (c->type) {
            case COMP_AC_VOLTAGE:
                freq = c->props.ac_voltage.frequency;
                if (c->props.ac_voltage.frequency_sweep.enabled) {
                    // Instantaneous sweep frequency: dt follows the sweep (re-synced whenever
                    // the tracked time/div steps), instead of paying for the top end all the time
                    freq = sweep_get_value(&c->props.ac_voltage.frequency_sweep, freq, sim->time);
                }
                break;
            case COMP_SQUARE_WAVE:
                freq = c->props.square_wave.frequency;
                break;
            case COMP_TRIANGLE_WAVE:
                freq = c->props.triangle_wave.frequency;
                break;
            case COMP_SAWTOOTH_WAVE:
                freq = c->props.sawtooth_wave.frequency;
                break;
            case COMP_SOURCE_3PH:
                freq = c->props.source_3ph.frequency;
                break;
            case COMP_PULSE_SOURCE:
                // repetitive pulses count as a periodic source (start-up kicks with a huge period do not)
                if (c->props.pulse_source.period > 0 && c->props.pulse_source.period < 10.0) freq = 1.0 / c->props.pulse_source.period;
                break;
            default:
                break;
        }

        if (freq > max_freq) {
            max_freq = freq;
        }
    }

    // Calculate time step to ensure smooth waveform visualization
    // More samples per period = smoother sine waves (avoiding triangular appearance)
    double dt;
    if (max_freq > 0) {
        double period = 1.0 / max_freq;

        // Use progressively more samples at higher frequencies for smooth curves
        // With theta-trapezoidal integration 100 samples per period keeps the amplitude and
        // phase error well under 1%; the old 200-300 tiers only made high-frequency
        // circuits crawl in real time.
        if (max_freq <= 100) {
            dt = period / 50.0;
        } else {
            dt = period / 100.0;
        }
    } else {
        // No periodic source at all (DC, kick-started oscillators): nothing here constrains dt,
        // so let the scope's time/div rule decide (it used to force the 100 ns default, which
        // made DC and pulse-only circuits crawl through 2 million steps per screen).
        dt = MAX_TIME_STEP;
    }

    // Clamp to valid range
    return CLAMP(dt, MIN_TIME_STEP, MAX_TIME_STEP);
}

double simulation_auto_time_step(Simulation *sim) {
    if (!sim || !sim->circuit) return DEFAULT_TIME_STEP;
    double dt = simulation_accuracy_time_step(sim);
    simulation_set_time_step(sim, dt);
    return dt;
}

double simulation_scope_time_step(Simulation *sim, double scope_time_div) {
    if (!sim || scope_time_div <= 0) return DEFAULT_TIME_STEP;
    double display_dt = scope_time_div / 20.0;         // ~20 samples per division (200 per screen)
    double accuracy_dt = simulation_accuracy_time_step(sim);
    double dt = display_dt < accuracy_dt ? display_dt : accuracy_dt;
    dt = CLAMP(dt, MIN_TIME_STEP, MAX_TIME_STEP);

    // Snap down to the 1-2-5 series (stays >= MIN_TIME_STEP because MIN is itself 1e-9)
    double decade = pow(10.0, floor(log10(dt)));
    double m = dt / decade;
    double snapped = (m >= 5.0) ? 5.0 : (m >= 2.0) ? 2.0 : 1.0;
    return CLAMP(snapped * decade, MIN_TIME_STEP, MAX_TIME_STEP);
}

double simulation_get_node_voltage(Simulation *sim, int node_id) {
    if (!sim || !sim->circuit) return 0;

    Node *node = circuit_get_node(sim->circuit, node_id);
    return node ? node->voltage : 0;
}

double simulation_get_probe_voltage(Simulation *sim, int probe_idx) {
    if (!sim || !sim->circuit || probe_idx < 0 || probe_idx >= sim->circuit->num_probes) {
        return 0;
    }
    return sim->circuit->probes[probe_idx].voltage;
}

int simulation_get_history(Simulation *sim, int probe_idx,
                           double *times, double *values, int max_points) {
    if (!sim || probe_idx < 0 || probe_idx >= MAX_PROBES) return 0;
    // Also check against actual number of probes in circuit to avoid stale data
    if (sim->circuit && probe_idx >= sim->circuit->num_probes) return 0;

    int count = MIN(sim->history_count, max_points);

    for (int i = 0; i < count; i++) {
        int idx = (sim->history_start + sim->history_count - count + i) % MAX_HISTORY;
        times[i] = sim->history[idx].time;
        values[i] = sim->history[idx].values[probe_idx];
    }

    return count;
}

const char *simulation_get_error(Simulation *sim) {
    return sim ? sim->error_msg : "No simulation";
}

void simulation_clear_error(Simulation *sim) {
    if (sim) {
        sim->has_error = false;
        sim->error_msg[0] = '\0';
    }
}

// Frequency response / Bode plot implementation
bool simulation_freq_sweep(Simulation *sim, double start_freq, double stop_freq,
                           int source_node, int probe_node, int num_points) {
    if (!sim || !sim->circuit) {
        simulation_set_error(sim, "No circuit");
        return false;
    }

    if (num_points > MAX_FREQ_POINTS) {
        num_points = MAX_FREQ_POINTS;
    }

    // Find an AC voltage source to use for excitation
    Component *ac_source = NULL;
    for (int i = 0; i < sim->circuit->num_components; i++) {
        Component *comp = sim->circuit->components[i];
        if (comp->type == COMP_AC_VOLTAGE) {
            ac_source = comp;
            break;
        }
    }

    if (!ac_source) {
        simulation_set_error(sim, "No AC voltage source found for frequency sweep");
        return false;
    }

    // Save original frequency
    double orig_freq = ac_source->props.ac_voltage.frequency;
    double amplitude = ac_source->props.ac_voltage.amplitude;

    sim->freq_start = start_freq;
    sim->freq_stop = stop_freq;
    sim->freq_source_node = source_node;
    sim->freq_probe_node = probe_node;
    sim->freq_sweep_running = true;
    sim->freq_sweep_complete = false;
    sim->freq_sweep_cancel = false;
    sim->freq_response_count = 0;
    sim->freq_sweep_progress = 0;
    sim->freq_sweep_total = num_points;

    // Generate logarithmically spaced frequencies
    double log_start = log10(start_freq);
    double log_stop = log10(stop_freq);
    double log_step = (log_stop - log_start) / (num_points - 1);

    for (int i = 0; i < num_points; i++) {
        // Check for cancellation request
        if (sim->freq_sweep_cancel) {
            ac_source->props.ac_voltage.frequency = orig_freq;
            sim->freq_sweep_running = false;
            sim->freq_sweep_complete = false;
            return false;
        }

        // Update progress
        sim->freq_sweep_progress = i;

        double freq = pow(10.0, log_start + i * log_step);

        // Set source frequency
        ac_source->props.ac_voltage.frequency = freq;

        // Calculate time step and simulation duration
        double period = 1.0 / freq;
        double dt = period / 100.0;  // 100 samples per period
        if (dt < MIN_TIME_STEP) dt = MIN_TIME_STEP;
        if (dt > MAX_TIME_STEP) dt = MAX_TIME_STEP;

        // Simulate for several cycles to reach steady state
        int num_cycles = 10;
        double total_time = num_cycles * period;
        int num_steps = (int)(total_time / dt);

        // Track min/max values for last 2 cycles
        double in_min = 1e30, in_max = -1e30;
        double out_min = 1e30, out_max = -1e30;

        // Track zero crossings for phase measurement
        double in_zero_cross_time = 0;
        double out_zero_cross_time = 0;
        double prev_in = 0, prev_out = 0;
        bool found_in_zero = false, found_out_zero = false;

        // Reset simulation state
        sim->time = 0;
        if (sim->solution) {
            for (int j = 0; j < sim->solution_size; j++) {
                vector_set(sim->solution, j, 0);
            }
        }
        if (sim->prev_solution) {
            for (int j = 0; j < sim->solution_size; j++) {
                vector_set(sim->prev_solution, j, 0);
            }
        }

        // Run simulation
        double measure_start = (num_cycles - 2) * period;
        for (int step = 0; step < num_steps; step++) {
            sim->time_step = dt;
            simulation_step(sim);
            sim->time += dt;

            // Get input (AC source output) and output voltages
            double t = sim->time;
            double in_voltage = amplitude * sin(2 * M_PI * freq * t + ac_source->props.ac_voltage.phase);
            double out_voltage = simulation_get_node_voltage(sim, probe_node);

            // Only measure during last 2 cycles
            if (t >= measure_start) {
                if (in_voltage < in_min) in_min = in_voltage;
                if (in_voltage > in_max) in_max = in_voltage;
                if (out_voltage < out_min) out_min = out_voltage;
                if (out_voltage > out_max) out_max = out_voltage;

                // Detect rising zero crossings for phase measurement
                if (!found_in_zero && prev_in <= 0 && in_voltage > 0) {
                    in_zero_cross_time = t - dt * prev_in / (in_voltage - prev_in);
                    found_in_zero = true;
                }
                if (!found_out_zero && prev_out <= 0 && out_voltage > 0) {
                    out_zero_cross_time = t - dt * prev_out / (out_voltage - prev_out);
                    found_out_zero = true;
                }
            }
            prev_in = in_voltage;
            prev_out = out_voltage;
        }

        // Calculate magnitude and phase
        double in_pp = in_max - in_min;
        double out_pp = out_max - out_min;

        double magnitude_ratio = (in_pp > 1e-12) ? (out_pp / in_pp) : 0;
        double magnitude_db = (magnitude_ratio > 1e-12) ? 20.0 * log10(magnitude_ratio) : -120.0;

        // Phase in degrees
        double phase_deg = 0;
        if (found_in_zero && found_out_zero) {
            double time_diff = out_zero_cross_time - in_zero_cross_time;
            phase_deg = (time_diff / period) * 360.0;
            // Normalize to -180 to +180
            while (phase_deg > 180) phase_deg -= 360;
            while (phase_deg < -180) phase_deg += 360;
        }

        // Store result
        sim->freq_response[sim->freq_response_count].frequency = freq;
        sim->freq_response[sim->freq_response_count].magnitude_db = magnitude_db;
        sim->freq_response[sim->freq_response_count].phase_deg = phase_deg;
        sim->freq_response_count++;
    }

    // Restore original frequency
    ac_source->props.ac_voltage.frequency = orig_freq;

    sim->freq_sweep_running = false;
    sim->freq_sweep_complete = true;

    return true;
}

int simulation_get_freq_response(Simulation *sim, FreqResponsePoint *points, int max_points) {
    if (!sim || !points) return 0;

    int count = MIN(sim->freq_response_count, max_points);
    for (int i = 0; i < count; i++) {
        points[i] = sim->freq_response[i];
    }

    return count;
}

void simulation_cancel_freq_sweep(Simulation *sim) {
    if (sim) {
        sim->freq_sweep_cancel = true;
    }
}
