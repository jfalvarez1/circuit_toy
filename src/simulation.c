/**
 * Circuit Playground - Simulation Engine Implementation
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "simulation.h"
#include "logic.h"

// GMIN - minimum conductance added from each node to ground
// This stabilizes floating nodes and prevents singular matrices
// Equivalent to 1 TΩ resistance to ground
#define GMIN 1e-12

Simulation *simulation_create(Circuit *circuit) {
    Simulation *sim = calloc(1, sizeof(Simulation));
    if (!sim) return NULL;

    sim->circuit = circuit;
    sim->state = SIM_STOPPED;
    sim->time = 0;
    sim->time_step = DEFAULT_TIME_STEP;
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
    }
    if (sim->saved_solution) {
        vector_free(sim->saved_solution);
        sim->saved_solution = NULL;
    }

    sim->history_count = 0;
    sim->history_start = 0;
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
            comp->voltage_var_idx = num_volt_vars++;
        }
    }

    int matrix_size = num_nodes + num_volt_vars;
    sim->solution_size = matrix_size;

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

        // Stamp all components
        // Use large dt for DC analysis so capacitors → open circuit, inductors → short circuit
        double dc_dt = 1e9;  // Very large dt for steady-state DC behavior
        for (int i = 0; i < circuit->num_components; i++) {
            component_stamp(circuit->components[i], A, b,
                           circuit->node_map, num_nodes,
                           0, solution, dc_dt);
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

    // Update circuit voltages and wire currents
    circuit_update_voltages(circuit, solution);
    circuit_update_wire_currents(circuit);
    circuit_update_meter_readings(circuit);

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

        vector_free(current_solution);
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
        double ambient = c->thermal.ambient_temperature;

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
        Vector *trial_solution = simulation_solve_step(sim, dt);

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

        // Accept the step
        vector_free(sim->solution);
        sim->solution = trial_solution;
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

    // Update circuit voltages, wire currents, and meter readings
    circuit_update_voltages(circuit, sim->solution);
    circuit_update_wire_currents(circuit);
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

    // Record history
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

    return true;
}

void simulation_set_speed(Simulation *sim, double speed) {
    if (sim) {
        sim->speed = CLAMP(speed, 0.1, 100.0);
    }
}

void simulation_set_time_step(Simulation *sim, double dt) {
    if (sim) {
        sim->time_step = CLAMP(dt, MIN_TIME_STEP, MAX_TIME_STEP);
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

double simulation_auto_time_step(Simulation *sim) {
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
        if (max_freq <= 100) {
            dt = period / 50.0;   // 50 samples/period for very low frequencies
        } else if (max_freq <= 1000) {
            dt = period / 100.0;  // 100 samples/period for 100Hz-1kHz
        } else if (max_freq <= 10000) {
            dt = period / 200.0;  // 200 samples/period for 1kHz-10kHz (fixes triangular appearance)
        } else if (max_freq <= 100000) {
            dt = period / 200.0;  // 200 samples/period for 10kHz-100kHz
        } else {
            dt = period / 300.0;  // 300 samples/period for >100kHz
        }
    } else {
        // No AC signals, use default time step
        dt = DEFAULT_TIME_STEP;
    }

    // Clamp to valid range
    dt = CLAMP(dt, MIN_TIME_STEP, MAX_TIME_STEP);
    sim->time_step = dt;

    return dt;
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
