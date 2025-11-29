/**
 * Circuit Playground - Simulation Engine
 */

#ifndef SIMULATION_H
#define SIMULATION_H

#include "types.h"
#include "circuit.h"
#include "matrix.h"

// Simulation configuration
#define DEFAULT_TIME_STEP 1e-6
#define MIN_TIME_STEP 1e-9
#define MAX_TIME_STEP 0.01
#define MAX_ITERATIONS 50
#define CONVERGENCE_TOL 1e-9

// Oscilloscope history point
typedef struct {
    double time;
    double values[MAX_PROBES];
} HistoryPoint;

// Simulation engine
typedef struct {
    Circuit *circuit;

    // State
    SimState state;
    double time;
    double time_step;
    double speed;  // Speed multiplier

    // Solution vectors
    Vector *solution;
    Vector *prev_solution;
    int solution_size;

    // Convergence tracking
    int iteration_count;
    bool converged;

    // History for oscilloscope
    HistoryPoint history[MAX_HISTORY];
    int history_count;
    int history_start;  // Circular buffer start

    // Error message
    char error_msg[256];
    bool has_error;
} Simulation;

// Create/destroy simulation
Simulation *simulation_create(Circuit *circuit);
void simulation_free(Simulation *sim);

// Control
void simulation_start(Simulation *sim);
void simulation_pause(Simulation *sim);
void simulation_stop(Simulation *sim);
void simulation_reset(Simulation *sim);

// Run DC analysis (operating point)
bool simulation_dc_analysis(Simulation *sim);

// Run single time step
bool simulation_step(Simulation *sim);

// Set simulation parameters
void simulation_set_speed(Simulation *sim, double speed);
void simulation_set_time_step(Simulation *sim, double dt);

// Get results
double simulation_get_node_voltage(Simulation *sim, int node_id);
double simulation_get_probe_voltage(Simulation *sim, int probe_idx);

// History access
int simulation_get_history(Simulation *sim, int probe_idx,
                           double *times, double *values, int max_points);

// Error handling
const char *simulation_get_error(Simulation *sim);
void simulation_clear_error(Simulation *sim);

#endif // SIMULATION_H
