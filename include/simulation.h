/**
 * Circuit Playground - Simulation Engine
 */

#ifndef SIMULATION_H
#define SIMULATION_H

#include "types.h"
#include "circuit.h"
#include "matrix.h"

// Simulation configuration
#define DEFAULT_TIME_STEP 1e-7    // 100 nanoseconds - good for observing transients
#define MIN_TIME_STEP 1e-9        // 1 nanosecond minimum
#define MAX_TIME_STEP 0.01        // 10 milliseconds maximum
#define MAX_ITERATIONS 50
#define CONVERGENCE_TOL 1e-9

// Oscilloscope history point
typedef struct {
    double time;
    double values[MAX_PROBES];
} HistoryPoint;

// Frequency response data point
typedef struct {
    double frequency;           // Hz
    double magnitude_db;        // dB (20*log10(Vout/Vin))
    double phase_deg;           // degrees
} FreqResponsePoint;

// Maximum points in frequency sweep
#define MAX_FREQ_POINTS 1000

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

    // Frequency response data
    FreqResponsePoint freq_response[MAX_FREQ_POINTS];
    int freq_response_count;
    double freq_start;              // Start frequency (Hz)
    double freq_stop;               // Stop frequency (Hz)
    int freq_source_node;           // Input voltage source node
    int freq_probe_node;            // Output probe node
    bool freq_sweep_running;        // Currently running sweep
    bool freq_sweep_complete;       // Sweep complete

    // Threading support for frequency sweep
    int freq_sweep_progress;        // Current point being processed (0 to num_points-1)
    int freq_sweep_total;           // Total number of points
    bool freq_sweep_cancel;         // Request to cancel sweep
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

// Auto-adjust time step based on circuit's highest frequency signal
// Returns the new time step that ensures adequate sampling (at least 50 samples/cycle)
double simulation_auto_time_step(Simulation *sim);

// Get results
double simulation_get_node_voltage(Simulation *sim, int node_id);
double simulation_get_probe_voltage(Simulation *sim, int probe_idx);

// History access
int simulation_get_history(Simulation *sim, int probe_idx,
                           double *times, double *values, int max_points);

// Error handling
const char *simulation_get_error(Simulation *sim);
void simulation_clear_error(Simulation *sim);

// Frequency response / Bode plot
// Run frequency sweep from start_freq to stop_freq (in Hz)
// Uses source_node as input reference, probe_node as output
bool simulation_freq_sweep(Simulation *sim, double start_freq, double stop_freq,
                           int source_node, int probe_node, int num_points);

// Cancel running frequency sweep
void simulation_cancel_freq_sweep(Simulation *sim);

// Get frequency response data
int simulation_get_freq_response(Simulation *sim, FreqResponsePoint *points, int max_points);

#endif // SIMULATION_H
