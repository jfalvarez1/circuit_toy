/**
 * Circuit Playground - Simulation Engine Implementation
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "simulation.h"

Simulation *simulation_create(Circuit *circuit) {
    Simulation *sim = calloc(1, sizeof(Simulation));
    if (!sim) return NULL;

    sim->circuit = circuit;
    sim->state = SIM_STOPPED;
    sim->time = 0;
    sim->time_step = DEFAULT_TIME_STEP;
    sim->speed = 1.0;

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

    sim->history_count = 0;
    sim->history_start = 0;
    sim->has_error = false;
    sim->error_msg[0] = '\0';

    // Reset node voltages
    if (sim->circuit) {
        for (int i = 0; i < sim->circuit->num_nodes; i++) {
            sim->circuit->nodes[i].voltage = 0;
        }
        for (int i = 0; i < sim->circuit->num_probes; i++) {
            sim->circuit->probes[i].voltage = 0;
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

        // Stamp all components
        // Use large dt for DC analysis so capacitors → open circuit, inductors → short circuit
        double dc_dt = 1e9;  // Very large dt for steady-state DC behavior
        for (int i = 0; i < circuit->num_components; i++) {
            component_stamp(circuit->components[i], A, b,
                           circuit->node_map, num_nodes,
                           0, solution, dc_dt);
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

    // Update circuit voltages
    circuit_update_voltages(circuit, solution);

    sim->has_error = false;
    return true;
}

bool simulation_step(Simulation *sim) {
    if (!sim || !sim->circuit) return false;

    Circuit *circuit = sim->circuit;

    // Ensure we have a solution
    if (!sim->solution) {
        if (!simulation_dc_analysis(sim)) {
            return false;
        }
    }

    int num_nodes = circuit->num_matrix_nodes;
    int matrix_size = sim->solution_size;

    // Iterative solve
    for (int iter = 0; iter < MAX_ITERATIONS; iter++) {
        Matrix *A = matrix_create(matrix_size, matrix_size);
        Vector *b = vector_create(matrix_size);

        if (!A || !b) {
            matrix_free(A);
            vector_free(b);
            simulation_set_error(sim, "Memory allocation failed");
            return false;
        }

        // Stamp components
        for (int i = 0; i < circuit->num_components; i++) {
            component_stamp(circuit->components[i], A, b,
                           circuit->node_map, num_nodes,
                           sim->time, sim->solution, sim->time_step);
        }

        Vector *new_solution = linear_solve(A, b);
        matrix_free(A);
        vector_free(b);

        if (!new_solution) {
            simulation_set_error(sim, "Matrix solver failed");
            return false;
        }

        // Check convergence
        double max_diff = 0;
        for (int i = 0; i < matrix_size; i++) {
            double diff = fabs(vector_get(new_solution, i) - vector_get(sim->solution, i));
            if (diff > max_diff) max_diff = diff;
        }

        vector_free(sim->solution);
        sim->solution = new_solution;

        if (max_diff < CONVERGENCE_TOL) {
            break;
        }
    }

    // Update for next step
    if (sim->prev_solution) vector_free(sim->prev_solution);
    sim->prev_solution = vector_clone(sim->solution);

    sim->time += sim->time_step;

    // Update circuit
    circuit_update_voltages(circuit, sim->solution);

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
    }
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
