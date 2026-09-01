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
/* How many matrix rows a placed block needs for the nodes inside it - INCLUDING the nodes
   inside any block nested in it, which each allocate their own. Counting only this level left
   the matrix too small, and indices past the end were dropped by the bounds check in
   matrix_add: those nodes silently behaved like ground, so a capacitor model nested twice read
   as a short (0.026 ohm where two 100 nF in parallel should be 7.96). */
/* Advance the capacitors inside a block - and inside any block nested in it. Without the
   recursion a nested capacitor's stored voltage stays at zero, its companion source with it,
   and the part behaves as a near short: a vendor capacitor model instantiated twice measured
   0.09 ohm where two 100 nF in parallel should be 7.96. */
static void subcircuit_advance_caps(Component *blk, Vector *now, Vector *prev, double dt) {
    Component *inner = NULL;
    int n = component_subcircuit_instance(blk, &inner);
    for (int i = 0; i < n; i++) {
        Component *comp = &inner[i];
        if (comp->type == COMP_SUBCIRCUIT) { subcircuit_advance_caps(comp, now, prev, dt); continue; }
        if (comp->type != COMP_CAPACITOR && comp->type != COMP_CAPACITOR_ELEC) continue;
        int n0 = comp->node_ids[0], n1 = comp->node_ids[1];
        double vn = ((n0 > 0) ? vector_get(now, n0 - 1) : 0) - ((n1 > 0) ? vector_get(now, n1 - 1) : 0);
        double vp = ((n0 > 0) ? vector_get(prev, n0 - 1) : 0) - ((n1 > 0) ? vector_get(prev, n1 - 1) : 0);
        CapCompanion cc = component_cap_companion(comp, dt, true, vp);
        double i_prev = comp->trap_i_prev;
        double i_new = cc.G * vn - cc.Ieq;
        if (cc.Geq > 0) comp->cap_vc += (i_new + cc.K * i_prev) / cc.Geq;
        comp->trap_i_prev = i_new;
    }
}

/* Record what a component's companion state is as this step's solve begins, so that reading a
   terminal current back afterwards - which re-stamps with g_stamp_read_only set, after the state
   has been advanced - reproduces the stamp that actually happened.

   It has to reach inside subcircuit blocks. A block's internal parts live on its own instance
   array, not on circuit->components, and subcircuit_advance_caps advances their cap_vc and
   trap_i_prev every accepted step. Snapshotting only the top level left those internals with
   cap_vc_solve = 0 for ever, so a charged capacitor inside a user block was re-stamped as though
   it were empty: a block's reported pin current came out 94x wrong on a 5 mA circuit. That was a
   regression introduced with the snapshot itself - before it, the read used cap_vc directly,
   which was one step stale but broadly right. */
static void snapshot_companion_state(Component *comp) {
    if (!comp) return;
    comp->trap_i_solve = comp->trap_i_prev;
    comp->cap_vc_solve = comp->cap_vc;
    if (comp->type == COMP_DC_MOTOR) {
        comp->state_w_solve = comp->props.dc_motor.omega;
        comp->state_i_solve = comp->props.dc_motor.current;
    } else if (comp->type == COMP_RELAY) {
        comp->state_i_solve = comp->props.relay.i_coil;
    } else if (comp->type == COMP_SUBCIRCUIT) {
        Component *inner = NULL;
        int n = component_subcircuit_instance(comp, &inner);
        for (int i = 0; i < n; i++) snapshot_companion_state(&inner[i]);   /* nested blocks too */
    }
}

/* Start a block's storage elements from the operating point, nested ones included. */
static void subcircuit_seed_state(Component *blk, Vector *solution) {
    Component *inner = NULL;
    int n = component_subcircuit_instance(blk, &inner);
    for (int i = 0; i < n; i++) {
        Component *comp = &inner[i];
        comp->trap_i_prev = 0.0;
        comp->tline_ic_prev[0] = comp->tline_ic_prev[1] = 0.0;
        if (comp->type == COMP_SUBCIRCUIT) { subcircuit_seed_state(comp, solution); continue; }
        if (comp->type == COMP_CAPACITOR || comp->type == COMP_CAPACITOR_ELEC) {
            int a2 = comp->node_ids[0], b2 = comp->node_ids[1];
            comp->cap_vc = ((a2 > 0) ? vector_get(solution, a2 - 1) : 0)
                         - ((b2 > 0) ? vector_get(solution, b2 - 1) : 0);
        }
    }
}

static int subcircuit_count_internal_nodes_depth(SubCircuitDef *def, int depth) {
    if (!def || !def->component_data || def->num_components == 0) return 0;
    if (depth >= SUBCIRCUIT_MAX_DEPTH) return 0;

    int max_node_id = 0, nested = 0;
    Component *internal_comps = (Component *)def->component_data;

    for (int i = 0; i < def->num_components; i++) {
        Component *ic = &internal_comps[i];
        for (int t = 0; t < ic->num_terminals && t < MAX_TERMINALS; t++) {
            if (ic->node_ids[t] > max_node_id) max_node_id = ic->node_ids[t];
        }
        if (ic->type == COMP_SUBCIRCUIT) {
            SubCircuitDef *sub = subcircuit_find_def(ic->props.subcircuit.def_id);
            nested += subcircuit_count_internal_nodes_depth(sub, depth + 1) + 1;
        }
    }
    return max_node_id + nested;
}

static int subcircuit_count_internal_nodes(SubCircuitDef *def) {
    return subcircuit_count_internal_nodes_depth(def, 0);
}

// GMIN - minimum conductance added from each node to ground
// This stabilizes floating nodes and prevents singular matrices
// Equivalent to 1 TΩ resistance to ground
#define GMIN 1e-12

// Forward declarations
static void simulation_clamp_opamps(Circuit *circuit, Vector *solution, double dt);

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
    if (sim) { sim->retune_countdown = 600; sim->retune_done = 0; sim->retune_looks = 0; }
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
                /* The crystal too. Its motional arm is an L-C-R branch and it keeps its state in
                   the same two fields - cap_vc for the motional capacitor's voltage and
                   trap_i_prev for the branch current - which this switch cleared only for the two
                   capacitor types. So Reset left a crystal ringing: a Pierce oscillator pressed
                   Reset came straight back at full amplitude instead of starting from nothing and
                   building up, which is the thing the template exists to show. */
                case COMP_CRYSTAL:
                    /* The stored state is cap_vc; props.capacitor.voltage is the initial
                       condition the circuit was built with, so a reset must not erase it. */
                    comp->cap_vc = 0.0;
                    comp->trap_i_prev = 0.0;
                    break;

                case COMP_INDUCTOR:
                    // Reset inductor current
                    comp->props.inductor.current = 0.0;
                    break;

                /* The relay's coil current and the motor's rotor are advanced once per accepted
                   step now, and the stamps only read them - so these fields ARE the initial
                   condition of the next run. Before that change the relay recomputed i_coil from
                   the node voltages on every stamp and the motor drove omega to its steady state
                   within one step, so a stale value corrected itself. Without this, Reset
                   restarts a motor at full speed with its relay already pulled in. */
                case COMP_RELAY:
                    comp->props.relay.i_coil = 0.0;
                    comp->props.relay.energized = false;
                    break;

                case COMP_DC_MOTOR:
                    comp->props.dc_motor.omega = 0.0;
                    comp->props.dc_motor.current = 0.0;
                    comp->props.dc_motor.v_bemf = 0.0;
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
        /* not gated on needs_voltage_var: a subcircuit needs rows for whatever is inside it */
        int aux = component_aux_count(comp);   // 1 for most, 3 for a three-phase source
        if (aux > 0) {
            comp->voltage_var_idx = num_volt_vars;
            num_volt_vars += aux;
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
    /* +1 because these are NODE ids, and a node id addresses matrix row id-1 (0 is ground).
       Starting at num_nodes + num_volt_vars put the first internal node on the LAST auxiliary
       row, on top of whatever voltage source or inductor owned it. */
    g_subcircuit_internal_node_offset = num_nodes + num_volt_vars + 1;

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
        /* +1 because these are NODE ids, and a node id addresses matrix row id-1 (0 is ground).
       Starting at num_nodes + num_volt_vars put the first internal node on the LAST auxiliary
       row, on top of whatever voltage source or inductor owned it. */
    g_subcircuit_internal_node_offset = num_nodes + num_volt_vars + 1;

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
    simulation_clamp_opamps(circuit, sim->solution, 0.0);   /* operating point: no slew limit yet */

    /* The relay's coil current and armature, seeded from the operating point. The advance that
       maintains them runs only on an accepted transient step, so without this the DC result says
       the coil carries V/R while props.relay.i_coil is still zero and the contacts are open -
       an operating point that contradicts its own solve, and the initial condition the transient
       then starts from. */
    bool armature_moved = false;
    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        if (!comp || comp->type != COMP_RELAY) continue;
        double R_coil = comp->props.relay.r_coil;
        if (!(R_coil > 0)) continue;
        int a = circuit->node_map[comp->node_ids[0]], b2 = circuit->node_map[comp->node_ids[1]];
        double v0 = (a > 0) ? vector_get(sim->solution, a - 1) : 0;
        double v1 = (b2 > 0) ? vector_get(sim->solution, b2 - 1) : 0;
        double I = (v0 - v1) / R_coil;
        comp->props.relay.i_coil = I;
        bool now = fabs(I) >= comp->props.relay.i_pickup;
        if (now != comp->props.relay.energized) armature_moved = true;
        comp->props.relay.energized = now;
    }

    /* And if one moved, solve again. Seeding the state was only half of it: the voltages above
       were computed with the contacts where they used to be, so an operating point that ends
       with a relay newly energized is describing a circuit that no longer exists - which is how
       an over-current trip could report itself tripped and its load still drawing current.

       Three passes is the cap. A protection whose contact removes the very current that closed
       it never settles, because there is no operating point to find: that is a relaxation
       oscillator and the transient is where it belongs. Stopping leaves the last consistent
       pair rather than looping. */
    if (armature_moved && sim->dc_relay_pass < 3) {
        sim->dc_relay_pass++;
        bool again = simulation_dc_analysis(sim);
        sim->dc_relay_pass--;
        return again;
    }

    // Capacitors carry no current at the operating point; swept sources restart their phase
    for (int i = 0; i < circuit->num_components; i++) {
        Component *cc_ = circuit->components[i];
        if (cc_->type == COMP_CAPACITOR || cc_->type == COMP_CAPACITOR_ELEC || cc_->type == COMP_TOROID) {
            /* with no current flowing, the capacitor holds the whole terminal voltage - unless
               it has been given an initial condition, which is what a converter template uses
               to start its transfer and output capacitors where they will settle instead of
               ringing its way there over several milliseconds */
            int a_ = circuit->node_map[cc_->node_ids[0]];
            int b_ = (cc_->type == COMP_TOROID) ? 0 : circuit->node_map[cc_->node_ids[1]];
            double ic_ = (cc_->type == COMP_CAPACITOR)      ? cc_->props.capacitor.voltage :
                         (cc_->type == COMP_CAPACITOR_ELEC) ? cc_->props.capacitor_elec.voltage : 0.0;
            cc_->cap_vc = (ic_ != 0.0) ? ic_
                        : ((a_ > 0) ? vector_get(solution, a_ - 1) : 0) - ((b_ > 0) ? vector_get(solution, b_ - 1) : 0);
        }
        circuit->components[i]->trap_i_prev = 0.0;
        circuit->components[i]->tline_ic_prev[0] = circuit->components[i]->tline_ic_prev[1] = 0.0;
        subcircuit_seed_state(circuit->components[i], solution);   /* nested blocks included */
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
        for (int i = 0; i < circuit->num_components; i++)
            num_volt_vars += component_aux_count(circuit->components[i]);
        /* +1 because these are NODE ids, and a node id addresses matrix row id-1 (0 is ground).
       Starting at num_nodes + num_volt_vars put the first internal node on the LAST auxiliary
       row, on top of whatever voltage source or inductor owned it. */
    g_subcircuit_internal_node_offset = num_nodes + num_volt_vars + 1;

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
static void simulation_clamp_opamps(Circuit *circuit, Vector *solution, double dt) {
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

        /* The slew-rate limit lives in the stamp (opamp_slew_pin), so the solved node and its
           branch currents stay consistent; here we only carry the output forward. */
        // Apply clamping to BOTH the node and the voltage variable
        if (clamped_value != v_out_var) {
            vector_set(solution, volt_var_idx, clamped_value);
            vector_set(solution, out_idx - 1, clamped_value);
        }
        comp->props.opamp.prev_output = clamped_value;   // one update per accepted solve; the stamp slews from it
        comp->slew_latch = 0;                            // the next step decides afresh
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
    if (sim->history_decimate_factor > 0 && sim->time_step >= MIN_TIME_STEP &&
        simulation_compute_decimation(sim) != sim->history_decimate_factor) {
        sim->history_prev_factor = sim->history_decimate_factor;
        sim->history_decimate_factor = 0;   // recompute on the next step, keeping what is recorded
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
        num_volt_vars += component_aux_count(circuit->components[i]);
    /* +1 because these are NODE ids, and a node id addresses matrix row id-1 (0 is ground).
       Starting at num_nodes + num_volt_vars put the first internal node on the LAST auxiliary
       row, on top of whatever voltage source or inductor owned it. */
    g_subcircuit_internal_node_offset = num_nodes + num_volt_vars + 1;

    double dt = (sim->dt_actual > 0) ? sim->dt_actual : sim->time_step;
    if (!sim->prev_step_solution) dt = 1e9;      // DC operating point: storage elements idle
    g_stamp_prev_step = sim->prev_step_solution;

    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        for (int t = 0; t < MAX_TERMINALS; t++) comp->terminal_current[t] = 0.0;
        /* A subcircuit is included: stamping the block stamps everything inside it, so the
           residual at a pin's row is the current entering that pin, and the flow animation
           carries on through the block instead of stopping dead at its edge. */
        if (comp->type == COMP_GROUND || comp->type == COMP_TEXT || comp->type == COMP_LABEL ||
            comp->type == COMP_PIN || comp->num_terminals < 2)
            continue;

        if ((comp->type == COMP_CAPACITOR || comp->type == COMP_CAPACITOR_ELEC) && sim->prev_step_solution) {
            double i_cap = comp->trap_i_prev;
            double leak = (comp->type == COMP_CAPACITOR) ? (comp->props.capacitor.ideal ? 0 : comp->props.capacitor.leakage)
                                                         : (comp->props.capacitor_elec.ideal ? 0 : comp->props.capacitor_elec.leakage);
            if (leak > 0) {
                int a_ = circuit->node_map[comp->node_ids[0]], b_ = circuit->node_map[comp->node_ids[1]];
                double v_ = ((a_ > 0) ? vector_get(sim->solution, a_ - 1) : 0) - ((b_ > 0) ? vector_get(sim->solution, b_ - 1) : 0);
                i_cap += v_ / leak;
            }
            comp->terminal_current[0] = i_cap;
            comp->terminal_current[1] = -i_cap;
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

        /* Rows this component can touch: its terminals, and its own auxiliary rows.

           The auxiliary rows are no longer collected into the array. rows[] held
           MAX_TERMINALS + 4, and component_aux_count() for a SUBCIRCUIT is the number of parts
           INSIDE it that need a row - a voltage source, a battery, an inductor, an op-amp, a
           transformer, each counts one - so a block with five of them overflowed a stack array.
           The guard there was "if (r < M)", which bounds the row's VALUE against the matrix size
           and says nothing about the index being written.

           They are contiguous, so they can be cleared where they are counted and never stored.
           That is the whole fix: an array that cannot overflow because nothing unbounded goes
           into it. */
        int rows[MAX_TERMINALS]; int nrows = 0;
        for (int t = 0; t < comp->num_terminals && nrows < MAX_TERMINALS; t++) {
            int id = comp->node_ids[t];
            int idx = (id >= 0 && id < MAX_NODES) ? circuit->node_map[id] : 0;
            if (idx > 0) rows[nrows++] = idx - 1;
        }
        int naux = component_aux_count(comp);   /* a subcircuit's rows belong to what is inside it */
        for (int k = 0; k < nrows; k++) clear_row(A, b, rows[k]);
        for (int k = 0; k < naux; k++) {
            int r = num_nodes + comp->voltage_var_idx + k;
            if (r >= 0 && r < M) clear_row(A, b, r);
        }

        Vector *lin = (sim->last_linearization && sim->last_linearization->size == M)
                      ? sim->last_linearization : sim->solution;
        g_stamp_read_only = true;    /* reading a current out, not advancing the circuit */
        component_stamp(comp, A, b, circuit->node_map, num_nodes, sim->time - dt, lin, dt);   /* the accepted step was stamped before time advanced */
        g_stamp_read_only = false;

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
        for (int k = 0; k < naux; k++) {
            int r = num_nodes + comp->voltage_var_idx + k;
            if (r >= 0 && r < M) clear_row(A, b, r);
        }
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

    /* Remember the companion state this step is about to solve with. It is read back afterwards,
       when the terminal currents are recovered by re-stamping each device on its own - and by
       then the accepted step has advanced trap_i_prev and cap_vc to the next step's values.
       Taken here rather than just before the advance so that a re-stamp during the step, which
       the step-rejection path does, sees this step's state and not the last one's. */
    for (int i = 0; i < circuit->num_components; i++) snapshot_companion_state(circuit->components[i]);

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

        /* A circuit that makes its own frequency gets its step from the display until it has run
           long enough to be measured - about twenty samples a cycle, which draws a waveform
           perfectly well and times one badly. Where the period is set by when a threshold is
           crossed rather than by an R and a C, the step quantises the crossing: the Function
           Generator and the Triangle/Square generator ran 10 % fast that way and the Ring
           Oscillator 9.4 %, and every audit agreed with them because the audits used a finer step
           than the app did.
           So once there is enough history to measure, look, and tighten to the hundred samples a
           cycle a circuit with a real source would have been given. Twice at most, and only ever
           finer: a measurement must not be able to coarsen the step that produced it, or the two
           chase each other. */
        if (sim->retune_done < 2 && sim->retune_looks < 60 && --sim->retune_countdown <= 0) {
            sim->retune_countdown = 600;
            sim->retune_looks++;
            double want = simulation_accuracy_time_step(sim);
            /* A fifth finer at least, or it is not worth the steps it costs. Nothing to measure
               yet is not an answer - a crystal takes a thousand times longer to start than a
               comparator, and giving up the first time the history is still flat would leave
               exactly the slow oscillators unrefined. The look budget ends it instead. */
            if (want > 0 && want < sim->time_step * 0.8) {
                sim->retune_done++;
                simulation_set_time_step(sim, want);
            }
        }

        /* A motor's rotor and its armature current, advanced once here rather than once per
           Newton iteration inside the stamp. Explicit Euler on the mechanical side, using the
           torque from the current at the start of the step, which is what the stamp assumed when
           it was doing this itself - the difference is only that it now happens once. */
        for (int i = 0; i < circuit->num_components; i++) {
            Component *comp = circuit->components[i];
            if (!comp || comp->type != COMP_DC_MOTOR) continue;
            double R_a = comp->props.dc_motor.r_armature;
            double L_a = comp->props.dc_motor.l_armature;
            double kv = comp->props.dc_motor.kv, kt = comp->props.dc_motor.kt;
            double J = comp->props.dc_motor.j_rotor, b_f = comp->props.dc_motor.b_friction;
            double T_load = comp->props.dc_motor.torque_load;
            if (!(J > 0) || !(dt > 0)) continue;
            double omega_prev = comp->props.dc_motor.omega;
            double I_prev = comp->props.dc_motor.current;
            double V_bemf = kv * omega_prev;
            double Req = R_a + L_a / dt;

            int a = circuit->node_map[comp->node_ids[0]], b2 = circuit->node_map[comp->node_ids[1]];
            double v1 = (a > 0) ? vector_get(sim->solution, a - 1) : 0;
            double v2 = (b2 > 0) ? vector_get(sim->solution, b2 - 1) : 0;
            double I_new = (Req > 0) ? (v1 - v2 - V_bemf) / Req + I_prev * L_a / (Req * dt) : 0;

            double d_omega = (kt * I_prev - b_f * omega_prev - T_load) / J;
            double omega_new = omega_prev + d_omega * dt;
            if (omega_new < 0) omega_new = 0;
            comp->props.dc_motor.omega = omega_new;
            comp->props.dc_motor.current = I_new;
        }

        /* A battery's remaining charge. Integrated once per accepted step, from the accepted
           solution, for the same reason as the two below: it used to be counted inside the stamp
           and the DC operating point's 1e9 pseudo-step emptied it before the run began. */
        for (int i = 0; i < circuit->num_components; i++) {
            Component *comp = circuit->components[i];
            if (!comp || comp->type != COMP_BATTERY) continue;
            if (comp->props.battery.ideal || comp->props.battery.discharged) continue;
            double cap_mah = comp->props.battery.capacity_mah;
            if (!(cap_mah > 0) || !(dt > 0)) continue;
            int vi = circuit->num_matrix_nodes + comp->voltage_var_idx;
            if (vi < 0 || vi >= sim->solution_size) continue;
            double I = fabs(vector_get(sim->solution, vi));
            comp->props.battery.current_draw = I;
            comp->props.battery.charge_coulombs -= I * dt;
            if (comp->props.battery.charge_coulombs < 0) comp->props.battery.charge_coulombs = 0;
            comp->props.battery.charge_state = comp->props.battery.charge_coulombs / (cap_mah * 3.6);
            if (comp->props.battery.charge_state < 0.01) comp->props.battery.discharged = true;
        }

        /* A relay's coil current, and the pull-in/drop-out decision made from it - once per
           accepted step, for the same reason as the motor above it. Hysteresis belongs here too:
           energizing is a decision about a step's converged current, not about a Newton iterate. */
        for (int i = 0; i < circuit->num_components; i++) {
            Component *comp = circuit->components[i];
            if (!comp || comp->type != COMP_RELAY) continue;
            double R_coil = comp->props.relay.r_coil;
            double L_coil = comp->props.relay.l_coil;
            if (!(R_coil > 0)) continue;
            int a = circuit->node_map[comp->node_ids[0]], b2 = circuit->node_map[comp->node_ids[1]];
            double v0 = (a > 0) ? vector_get(sim->solution, a - 1) : 0;
            double v1 = (b2 > 0) ? vector_get(sim->solution, b2 - 1) : 0;
            double V_coil = v0 - v1;
            double I_new;
            if (comp->props.relay.ideal || L_coil < 1e-9 || !(dt > 0)) {
                I_new = V_coil / R_coil;
            } else {
                double R_eq = R_coil + L_coil / dt;
                I_new = (V_coil + (L_coil / dt) * comp->props.relay.i_coil) / R_eq;
            }
            comp->props.relay.i_coil = I_new;
            double I_abs = fabs(I_new);
            if (!comp->props.relay.energized && I_abs >= comp->props.relay.i_pickup)
                comp->props.relay.energized = true;
            else if (comp->props.relay.energized && I_abs <= comp->props.relay.i_dropout)
                comp->props.relay.energized = false;
        }

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

        /* Trapezoidal capacitor state: i_new = (2C/dt)(v_new - v_prev) - i_prev.
           Components inside a subcircuit are advanced the same way - their state lives on the
           block's own copies, and without this pass an internal capacitor never charges. Their
           node_ids already hold matrix indices, so they skip the node_map. */
        for (int sub = 0; sub < circuit->num_components; sub++)
            subcircuit_advance_caps(circuit->components[sub], sim->solution, sim->prev_step_solution, dt);

        /* Delay line: record what each end launched into the cable this step. The far end
           will read it back one propagation delay from now, which is the whole model - so this
           has to happen once per ACCEPTED step, with the solution that was accepted.
             i_k = (v_k - E_k) / Z0 is the current flowing into the line at port k, where E_k is
           the wave that arrived there; the launched wave is v_k + Z0 i_k. */
        for (int i = 0; i < circuit->num_components; i++) {
            Component *comp = circuit->components[i];
            if (comp->type != COMP_DELAY_LINE) continue;
            double z0 = comp->props.delay_line.z0, td = comp->props.delay_line.delay;
            if (z0 <= 0) continue;
            int m0 = (comp->node_ids[0] >= 0 && comp->node_ids[0] < MAX_NODES) ? circuit->node_map[comp->node_ids[0]] : 0;
            int m1 = (comp->node_ids[1] >= 0 && comp->node_ids[1] < MAX_NODES) ? circuit->node_map[comp->node_ids[1]] : 0;
            double v0 = (m0 > 0 && m0 - 1 < (int)sim->solution->size) ? vector_get(sim->solution, m0 - 1) : 0.0;
            double v1 = (m1 > 0 && m1 - 1 < (int)sim->solution->size) ? vector_get(sim->solution, m1 - 1) : 0.0;
            double e0 = (td > 0) ? delay_line_history(comp, 1, sim->time + dt - td) : 0.0;
            double e1 = (td > 0) ? delay_line_history(comp, 0, sim->time + dt - td) : 0.0;
            if (!comp->props.delay_line.ideal && comp->props.delay_line.loss_db > 0) {
                double a = pow(10.0, -comp->props.delay_line.loss_db / 20.0);
                e0 *= a; e1 *= a;
            }
            double i0 = (v0 - e0) / z0, i1 = (v1 - e1) / z0;
            comp->terminal_current[0] = i0;
            comp->terminal_current[1] = i1;
            /* the accepted solution belongs to the END of the step; sim->time is still at its start */
            delay_line_record(comp, sim->time + dt, v0, i0, v1, i1);
        }

        /* Crystal: the motional capacitor's voltage and the holder capacitor's current, both
           carried forward from the arm current the solve just produced. */
        for (int i = 0; i < circuit->num_components; i++) {
            Component *comp = circuit->components[i];
            if (comp->type != COMP_CRYSTAL) continue;
            int idx = circuit->num_matrix_nodes + comp->voltage_var_idx;
            if (idx >= (int)sim->solution->size) continue;
            double i_now = vector_get(sim->solution, idx);
            double i_was = vector_get(sim->prev_step_solution, idx);
            double Cs = comp->props.crystal.cs;
            if (Cs > 0) comp->cap_vc += (dt / (2.0 * Cs)) * (i_now + i_was);   /* trapezoidal */
            double Cp = comp->props.crystal.cp;
            if (Cp > 0) {
                int a2 = circuit->node_map[comp->node_ids[0]], b2 = circuit->node_map[comp->node_ids[1]];
                double vn = ((a2 > 0) ? vector_get(sim->solution, a2 - 1) : 0)
                          - ((b2 > 0) ? vector_get(sim->solution, b2 - 1) : 0);
                double vp = ((a2 > 0) ? vector_get(sim->prev_step_solution, a2 - 1) : 0)
                          - ((b2 > 0) ? vector_get(sim->prev_step_solution, b2 - 1) : 0);
                comp->trap_i_prev = (Cp / (0.6 * dt)) * (vn - vp) - (0.4 / 0.6) * comp->trap_i_prev;
            }
        }
        for (int i = 0; i < circuit->num_components; i++) {
            Component *comp = circuit->components[i];
            if (comp->type != COMP_CAPACITOR && comp->type != COMP_CAPACITOR_ELEC && comp->type != COMP_TOROID) continue;
            double C = (comp->type == COMP_CAPACITOR) ? comp->props.capacitor.capacitance
                     : (comp->type == COMP_TOROID) ? toroid_capacitance(comp)
                                                      : comp->props.capacitor_elec.capacitance;
            (void)C;
            int n0 = circuit->node_map[comp->node_ids[0]], n1 = (comp->type == COMP_TOROID) ? 0 : circuit->node_map[comp->node_ids[1]];
            double vn = ((n0 > 0) ? vector_get(sim->solution, n0 - 1) : 0) - ((n1 > 0) ? vector_get(sim->solution, n1 - 1) : 0);
            double vp = ((n0 > 0) ? vector_get(sim->prev_step_solution, n0 - 1) : 0) - ((n1 > 0) ? vector_get(sim->prev_step_solution, n1 - 1) : 0);
            /* Same companion the stamp used, so the state and the matrix can never disagree.
               i = G v - Ieq is the branch current; the capacitor's own voltage then follows from
               the theta-method relation. In ideal mode this is the old one-liner exactly. */
            CapCompanion cc = component_cap_companion(comp, dt, true, vp);
            double i_prev = comp->trap_i_prev;
            double i_new = cc.G * vn - cc.Ieq;
            comp->cap_vc += (i_new + cc.K * i_prev) / cc.Geq;
            comp->trap_i_prev = i_new;
        }

        // Apply post-solve clamping as safety net (valid approach for educational simulators)
        // This prevents any remaining numerical drift from pushing outputs beyond rails
        simulation_clamp_opamps(circuit, sim->solution, dt);

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
    /* >= MIN_TIME_STEP, not a hard 1e-9: a step below the old floor would leave the factor at
       zero forever and nothing would ever be recorded, which is a blank scope. */
    if (sim->history_decimate_factor == 0 && sim->time_step >= MIN_TIME_STEP) {
        int prev = sim->history_prev_factor;
        sim->history_decimate_factor = simulation_compute_decimation(sim);
        sim->history_prev_factor = 0;

        /* Widening the scope's time/div asks for a coarser spacing, and this used to throw the
           recorded history away and start again - so the trace vanished and came back as the
           buffer refilled, which is what a wider T+ looked like from the front. Every sample
           carries its own timestamp, so the samples already recorded are still good: thin them
           to the new spacing and keep them. Only a finer spacing, which cannot be reconstructed
           from coarse samples, starts over. */
        int keep_every = (prev > 0 && sim->history_decimate_factor > prev)
                             ? sim->history_decimate_factor / prev : 0;
        if (keep_every > 1 && sim->history_count > 1) {
            int kept_n = (sim->history_count + keep_every - 1) / keep_every;
            HistoryPoint *kept = malloc((size_t)kept_n * sizeof *kept);
            if (kept) {
                int w = 0;
                for (int i = 0; i < sim->history_count && w < kept_n; i += keep_every)
                    kept[w++] = sim->history[(sim->history_start + i) % MAX_HISTORY];
                memcpy(sim->history, kept, (size_t)w * sizeof *kept);
                free(kept);
                sim->history_start = 0;
                sim->history_count = w;
            } else {
                sim->history_count = 0;
                sim->history_start = 0;
            }
        } else if (keep_every != 1) {
            // Finer spacing than what is recorded: start over so all samples share one spacing
            sim->history_count = 0;
            sim->history_start = 0;
        }
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

/* Two node ids on the same net? Uses the solver's own node map, which has already merged wires and
   coincident terminals, and falls back to identity if it has not been built. */
static bool nets_equal(Circuit *c, int a, int b) {
    if (a == b) return true;
    if (!c || a < 0 || b < 0 || a >= MAX_NODES || b >= MAX_NODES) return false;
    return c->node_map[a] == c->node_map[b];
}

/* The fastest natural time constant in the circuit, or 0 if nothing here has one.

   The step rule above asks only what the SOURCES do. That is fine until a circuit's own dynamics
   are faster than anything driving it: Hot-Plug Inrush charges 1 mF through 50 mohm of connector -
   a 50 us event - from a DC rail through a switch, so there is no frequency and no pulse width
   anywhere in the netlist to notice. The step came from the display rule at ~125 us and stepped
   straight over the inrush the template is named after, and the rail sag it reported moved 8 % when
   the step was divided by eight.

   The resistance a capacitor works against is, properly, the Thevenin resistance seen at its
   terminals, which needs a solve of its own. What is used instead is the smallest series
   resistance sharing a net with it. "Shares a net with" is not "is in series with", so this is an
   approximation, and the shape of the rule at the call site - only where no source sets the pace,
   only where the circuit has not measured its own period, and never for a circuit that can
   resonate - is entirely about keeping that approximation somewhere it cannot do harm.

   Nets, not node ids, because a template joins its parts with wires and two ids on one net are the
   same piece of copper. node_map is built by the time anything asks for a step. */
/* What a part looks like as a series resistance, or 0 if it is not one.

   Not only COMP_RESISTOR. Hot-Plug Inrush is the case that proves it: its "50 mohm of connector"
   is an analog switch's r_on and its supply's is a source r_series, so a scan for resistors found
   nothing but the load and concluded the circuit was slow. The parasitic resistances ARE the
   circuit here - they are the only thing between 12 V and an empty capacitor. */
static double component_series_ohms(const Component *r) {
    if (!r) return 0;
    switch (r->type) {
        case COMP_RESISTOR:       return r->props.resistor.resistance;
        case COMP_ANALOG_SWITCH:  return r->props.analog_switch.ideal ? 0 : r->props.analog_switch.r_on;
        case COMP_DC_VOLTAGE:     return r->props.dc_voltage.ideal ? 0 : r->props.dc_voltage.r_series;
        case COMP_AC_VOLTAGE:     return r->props.ac_voltage.ideal ? 0 : r->props.ac_voltage.r_series;
        default:                  return 0;
    }
}

static double circuit_fastest_time_constant(Circuit *c) {
    if (!c) return 0;

    /* Nothing at all for a circuit that can resonate. R times C is the scale on which a capacitor
       settles; it is not the scale on which an LC tank or a crystal does anything, and the two are
       often orders apart. Guessing an RC for an oscillator and stepping to it broke Colpitts,
       Hartley and Pierce outright - the tank's own period is the only right answer there, and the
       measured-period rule above is what supplies it. The circuits this rule exists for - a
       hot-plug, a switched charge transfer, an RC that settles - have no inductor in them. */
    for (int i = 0; i < c->num_components; i++) {
        Component *k = c->components[i];
        if (!k) continue;
        if (k->type == COMP_INDUCTOR || k->type == COMP_CRYSTAL ||
            k->type == COMP_TRANSFORMER) return 0;
    }

    double best = 0;
    for (int i = 0; i < c->num_components; i++) {
        Component *s = c->components[i];
        if (!s || s->type != COMP_CAPACITOR) continue;
        double val = s->props.capacitor.capacitance;
        if (!(val > 0)) continue;

        double r_small = 0;
        for (int j = 0; j < c->num_components; j++) {
            Component *r = c->components[j];
            if (!r) continue;
            double rv = component_series_ohms(r);
            if (!(rv > 0)) continue;
            int touches = 0;
            for (int a = 0; a < 2 && !touches; a++)
                for (int b = 0; b < 2 && !touches; b++)
                    if (nets_equal(c, s->node_ids[a], r->node_ids[b])) touches = 1;
            if (!touches) continue;
            if (r_small <= 0 || rv < r_small) r_small = rv;
        }

        double tau = (r_small > 0) ? r_small * val : 0;
        if (tau > 0 && (best <= 0 || tau < best)) best = tau;
    }
    return best;
}

/* Set by --no-rc-step. Only so the before and after can be measured on one binary; the rule is
   on in every build that ships. */
bool g_rc_step_disabled = false;

double simulation_accuracy_time_step(Simulation *sim) {
    if (!sim || !sim->circuit) return DEFAULT_TIME_STEP;

    // Find the highest frequency signal in the circuit
    double max_freq = 0;
    /* ...and the narrowest thing any stimulus actually does, which a frequency does not describe */
    double min_pulse = 0;

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
            case COMP_PULSE_SOURCE: {
                // repetitive pulses count as a periodic source (start-up kicks with a huge period do not)
                if (c->props.pulse_source.period > 0 && c->props.pulse_source.period < 10.0) freq = 1.0 / c->props.pulse_source.period;
                /* but the pulse itself has to be sampled however rarely it comes round */
                double pw = c->props.pulse_source.pulse_width;
                if (pw > 0 && (min_pulse <= 0 || pw < min_pulse)) min_pulse = pw;
                break;
            }
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

        /* ...except that a circuit with no source may still have a frequency: it makes its own.
           A relaxation oscillator, a ring, an LC tank - nothing in the netlist says how fast, so
           the step fell through to the display's rule, which is about twenty samples a division.
           Twenty samples a cycle is enough to draw a waveform and not nearly enough to time one:
           when the period is set by *when a threshold is crossed* rather than by an RC, the step
           quantises the crossing, and the Function Generator and the Triangle/Square generator
           were both shown running 10 % fast, the Ring Oscillator 9.4 %. The audits agreed with
           them, because the audits used a finer step than the app did.

           The period cannot be known before the circuit runs. It can be measured once it has, so
           measure it, and hold the step to the hundred samples a cycle a known source would get.
           Only ever tightening: a measurement that came out slow must not be able to coarsen the
           step that produced it, which is how a loop like this oscillates. */
        double self_period = 0;
        for (int p = 0; p < sim->circuit->num_probes && p < MAX_PROBES; p++) {
            SignalCharacter sc;
            simulation_characterise(sim, p, &sc);
            if (sc.cls != SIGNAL_PERIODIC || sc.period <= 0) continue;
            if (self_period <= 0 || sc.period < self_period) self_period = sc.period;
        }
        if (self_period > 0) {
            double self_dt = self_period / 100.0;
            if (self_dt < dt) dt = self_dt;
        }

        /* And ten samples across the circuit's own fastest natural time constant - see
           circuit_fastest_time_constant for what that is and why a scan for resistors is not
           enough to find it.

           Only in this branch, where there is no periodic source. That is not timidity, it is the
           measurement: applied to every circuit the rule tightens the step on templates whose pace
           is already set by a source, because a non-ideal source carries a default 1 mohm and a
           power supply's 1000 uF against 1 mohm is a 1 us time constant. Resolving that on a 60 Hz
           circuit means a 100 ns step where 333 us would do - three thousand times the work to
           resolve a start-up transient nobody is looking at. The audit battery went from 224
           seconds to over fifteen minutes and was still running.

           When a source sets the pace, the source and display rules already give a step that
           resolves what is on screen. When nothing does - a DC circuit, a hot-plug, a switched
           charge transfer - the circuit's own dynamics are the only thing there is, and they were
           being ignored entirely. */
        /* ...and only if the circuit has not already told us its period by running. An LC or
           crystal oscillator IS its own source, and the measurement above is a far better answer
           than any guess from an R and a C: applied to those, this rule broke Colpitts, Hartley
           and Pierce outright. The reason is the same approximation that makes it useful on a
           hot-plug - "shares a net with" is not "is in series with" - and on an oscillator with a
           1 M bias resistor touching the tank it collapses the step to the floor and the thing
           never starts. Measured period first; this only where there is none. */
        if (!g_rc_step_disabled && self_period <= 0) {
            double tau = circuit_fastest_time_constant(sim->circuit);
            if (tau > 0) {
                double dt_rc = tau / 10.0;
                if (dt_rc < dt) dt = dt_rc;
            }
        }
    }

    /* Five samples across the narrowest pulse, however rare that pulse is. The relaxation
       oscillator is started by a single 20 us kick with a hundred-second period: it counts as no
       frequency at all, the step came out at 50 us, and the kick fell between two samples. It
       only ever started because the first sample happened to land on its leading edge. */
    if (min_pulse > 0) {
        double dt_pulse = min_pulse / 5.0;
        if (dt_pulse < dt) dt = dt_pulse;
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

    // Snap down to the 1-2-5 series, then clamp
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

/* See the comment in simulation.h. The classification is made from the recorded history rather
   than from the netlist on purpose: what a circuit is doing is a property of the run, and a
   template that has quietly stopped oscillating should be reported as static, not excused because
   its netlist contains a crystal. */
void simulation_characterise(Simulation *sim, int probe_idx, SignalCharacter *out) {
    if (!out) return;
    memset(out, 0, sizeof *out);
    out->cls = SIGNAL_STATIC;
    if (!sim) return;

    static double th[MAX_HISTORY], tv[MAX_HISTORY];
    int n = simulation_get_history(sim, probe_idx, th, tv, MAX_HISTORY);
    out->samples = n;
    if (n < 4) return;

    double lo = tv[0], hi = tv[0], sum = 0;
    for (int i = 0; i < n; i++) {
        if (tv[i] < lo) lo = tv[i];
        if (tv[i] > hi) hi = tv[i];
        sum += tv[i];
    }
    double span = hi - lo;
    out->amplitude = span / 2.0;
    out->dc = sum / n;

    /* "Does not move" has to be relative to the size of the thing: a 5 V rail wobbling by a
       millivolt is static, and a millivolt signal swinging by a millivolt is not. */
    double scale = fabs(hi) > fabs(lo) ? fabs(hi) : fabs(lo);
    /* Relative to the size of the thing, and never below a microvolt in absolute terms. The
       X-Y Plotter idles at about 1e-10 V of solver residue, which is not a signal by any measure
       this program can display, and calling it one made its class depend on the time step. */
    if (span <= 1e-6 * (scale + 1e-12) + 1e-6) return;

    /* Rising crossings of the mid-level, interpolated, and the intervals between them. A periodic
       signal has intervals that agree; a sweep or a staircase has intervals that march. */
    double mid = 0.5 * (lo + hi);
    static double xt[MAX_HISTORY];
    int nx = 0;
    for (int i = 1; i < n && nx < MAX_HISTORY; i++) {
        if (tv[i - 1] < mid && tv[i] >= mid) {
            double d = tv[i] - tv[i - 1];
            double frac = (d != 0.0) ? (mid - tv[i - 1]) / d : 0.0;
            xt[nx++] = th[i - 1] + frac * (th[i] - th[i - 1]);
        }
    }
    out->cycles = nx > 0 ? nx - 1 : 0;

    if (nx < 3) {
        out->cls = SIGNAL_ONESHOT;      /* it moved, and crossed its own middle once or not at all */
    } else {
        /* Judge the settled part, not the start-up. An oscillator building up has intervals that
           genuinely disagree - it is changing - and reading the whole record at once made the
           Hartley and the Clapp come out "stepped" at one step and "periodic" at another, which
           is a statement about where the run happened to stop rather than about the circuit. Take
           a provisional period from the median interval, use it to find where the envelope stopped
           changing, and judge from the crossings after that. Every circuit settles at its own rate;
           this is how a suite asks each one on its own terms rather than on a fixed schedule. */
        static double iv[MAX_HISTORY];
        for (int i = 1; i < nx; i++) iv[i - 1] = xt[i] - xt[i - 1];
        int niv = nx - 1;
        for (int i = 1; i < niv; i++) {          /* insertion sort: niv is small and nearly sorted */
            double k = iv[i]; int j = i - 1;
            while (j >= 0 && iv[j] > k) { iv[j + 1] = iv[j]; j--; }
            iv[j + 1] = k;
        }
        double prov = iv[niv / 2];

        double t_from = th[0];
        if (prov > 0) {
            double w = 3.0 * prov;
            double f_lo = 1e300, f_hi = -1e300;
            for (int i = n - 1; i >= 0 && th[n - 1] - th[i] <= w; i--) {
                if (tv[i] < f_lo) f_lo = tv[i];
                if (tv[i] > f_hi) f_hi = tv[i];
            }
            double tol = 0.02 * (f_hi - f_lo) + 1e-12;
            for (int i = 0; i < n; i++) {
                double w_lo = 1e300, w_hi = -1e300;
                for (int j = i; j < n && th[j] - th[i] <= w; j++) {
                    if (tv[j] < w_lo) w_lo = tv[j];
                    if (tv[j] > w_hi) w_hi = tv[j];
                }
                if (fabs(w_lo - f_lo) <= tol && fabs(w_hi - f_hi) <= tol) { t_from = th[i]; break; }
            }
        }
        out->settle_time = t_from;

        int s0 = 0;
        while (s0 < nx && xt[s0] < t_from) s0++;
        if (nx - s0 < 3) s0 = 0;        /* nothing settled to speak of: judge what there is */

        double mean_iv = (xt[nx - 1] - xt[s0]) / (double)(nx - 1 - s0);
        double worst = 0;
        for (int i = s0 + 1; i < nx; i++) {
            double e = fabs((xt[i] - xt[i - 1]) - mean_iv);
            if (e > worst) worst = e;
        }
        /* 5 % of the mean interval: loose enough for an adaptive step landing crossings on
           different samples, tight enough that a sweep does not pass as periodic. */
        if (mean_iv > 0 && worst <= 0.05 * mean_iv) {
            out->cls = SIGNAL_PERIODIC;
            out->period = mean_iv;
            out->frequency = 1.0 / mean_iv;
        } else {
            out->cls = SIGNAL_STEPPED;
        }

        /* the amplitude and level of the settled part, which is what a reader means by them */
        double slo = 1e300, shi = -1e300, ssum = 0; int sn = 0;
        for (int i = 0; i < n; i++) {
            if (th[i] < t_from) continue;
            if (tv[i] < slo) slo = tv[i];
            if (tv[i] > shi) shi = tv[i];
            ssum += tv[i]; sn++;
        }
        if (sn > 1) { out->amplitude = (shi - slo) / 2.0; out->dc = ssum / sn; }
    }

    /* When it settled: the first moment from which the envelope already looks like the envelope at
       the end. A few cycles make the comparison window for something periodic, a tenth of the
       record for anything else. This is what a suite should wait for instead of a fixed number of
       divisions - a crystal taking milliseconds to start and an RC taking microseconds both get
       what they need, and neither holds up the run. */
    {
        double w = (out->cls == SIGNAL_PERIODIC && out->period > 0)
                   ? 3.0 * out->period : (th[n - 1] - th[0]) / 10.0;
        if (w <= 0) { out->settle_time = th[n - 1]; return; }

        double f_lo = 1e300, f_hi = -1e300;
        for (int i = n - 1; i >= 0 && th[n - 1] - th[i] <= w; i--) {
            if (tv[i] < f_lo) f_lo = tv[i];
            if (tv[i] > f_hi) f_hi = tv[i];
        }
        double tol = 0.02 * (f_hi - f_lo) + 1e-12;
        out->settle_time = th[n - 1];
        for (int i = 0; i < n; i++) {
            double w_lo = 1e300, w_hi = -1e300;
            for (int j = i; j < n && th[j] - th[i] <= w; j++) {
                if (tv[j] < w_lo) w_lo = tv[j];
                if (tv[j] > w_hi) w_hi = tv[j];
            }
            if (fabs(w_lo - f_lo) <= tol && fabs(w_hi - f_hi) <= tol) { out->settle_time = th[i]; break; }
        }
    }
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

    /* Everything the sweep is about to move, so the live circuit gets it back. This runs on the
       user's own Simulation, not a copy, so anything left where the sweep put it is the running
       circuit quietly disturbed. */
    double orig_freq = ac_source->props.ac_voltage.frequency;
    bool orig_fsweep = ac_source->props.ac_voltage.frequency_sweep.enabled;
    double orig_time = sim->time;
    double orig_step = sim->time_step;
    Vector *keep_solution = sim->solution ? vector_clone(sim->solution) : NULL;
    Vector *keep_prev = sim->prev_solution ? vector_clone(sim->prev_solution) : NULL;

    /* And the source's OWN frequency sweep goes off for the duration. The stamp reads
       sweep_get_value(&frequency_sweep, freq, time), so while that is enabled the source ignores
       the frequency this analysis is setting and plays whatever its own sweep says instead - and
       the RC Low Pass, the first template in the app, ships with it enabled. Every point of its
       Bode plot was measured at the wrong frequency. */
    ac_source->props.ac_voltage.frequency_sweep.enabled = false;
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
            /* simulation_step advances sim->time by dt itself. Adding it again here ran the clock
               at 2 dt per solve: the companion models integrated one dt while the source's phase
               and the analytic reference below moved two, so magnitude and phase were both read
               off a circuit being driven at twice the rate the integrator thought. */
            simulation_step(sim);

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
            /* Negated. The output of a low-pass LAGS its input, so the output's rising zero
               crossing happens later and out - in is positive - but a lag is a negative phase.
               The plot read +85 degrees where an RC at twelve times its corner is -85, and the
               sign was wrong at every point that had any phase in it at all. */
            double time_diff = out_zero_cross_time - in_zero_cross_time;
            phase_deg = -(time_diff / period) * 360.0;
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

    // Restore everything the sweep moved
    ac_source->props.ac_voltage.frequency = orig_freq;
    ac_source->props.ac_voltage.frequency_sweep.enabled = orig_fsweep;
    sim->time = orig_time;
    sim->time_step = orig_step;
    if (keep_solution) {
        if (sim->solution) vector_free(sim->solution);
        sim->solution = keep_solution;
    }
    if (keep_prev) {
        if (sim->prev_solution) vector_free(sim->prev_solution);
        sim->prev_solution = keep_prev;
    }

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
