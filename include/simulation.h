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
/* 1 ns was the floor, and it is the reason the transmission-line templates looked untriggered:
   at 5 ns a division the scope asks for a step of 250 ps and got 1 ns, so each division of the
   screen was five samples. The trace was a zigzag and the trigger point could only land on one
   of five places in a division, which reads as jitter. 10 ps lets a 5 ns/div screen have its
   twenty samples a division like every other circuit. */
#define MIN_TIME_STEP 1e-11       // 10 picoseconds minimum
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

// Adaptive time-stepping configuration
#define ADAPTIVE_ERROR_TOL 0.05       // 5% relative error tolerance
#define ADAPTIVE_SAFETY_FACTOR 0.9    // Safety factor for step sizing
#define ADAPTIVE_MIN_FACTOR 0.5       // Minimum step reduction factor
#define ADAPTIVE_MAX_FACTOR 2.0       // Maximum step increase factor
#define ADAPTIVE_STEADY_THRESHOLD 0.001  // Threshold for "steady" circuit (0.1%)

// Simulation engine
typedef struct Simulation {
    Circuit *circuit;

    // State
    SimState state;
    double time;
    double time_step;
    double speed;  // Speed multiplier

    // Adaptive time-stepping
    bool adaptive_enabled;          // Enable adaptive stepping
    double dt_target;               // Target/nominal time step
    double dt_actual;               // Actual time step used this iteration
    double error_estimate;          // Estimated local truncation error
    int step_rejections;            // Number of rejected steps (for UI)
    int total_step_rejections;      // Total rejections since start
    double adaptive_factor;         // Current step size multiplier (for UI)
    Vector *saved_solution;         // Saved solution for step rejection/retry

    // Solution vectors
    Vector *solution;
    Vector *prev_solution;
    Vector *prev_step_solution;     // Converged solution of the previous accepted time step
    Vector *last_linearization;     // Newton iterate the final linear solve was linearized at
    int solution_size;

    // Convergence tracking
    int iteration_count;
    bool converged;

    // History for oscilloscope
    HistoryPoint history[MAX_HISTORY];
    int history_count;
    int history_start;  // Circular buffer start

    // Adaptive decimation for history (ensures history covers long time spans)
    int history_decimate_counter;   // Counter for decimation
    int history_decimate_factor;    // Current decimation factor (record every Nth sample)
    /* A circuit with no source has no frequency until it makes one, so the step it starts with
       cannot know how fine it needs to be. These count the steps until the next look and record
       how many times the look has tightened it, so it settles instead of chasing itself. */
    int retune_countdown;
    int retune_done;
    int retune_looks;

    int history_prev_factor;        // the factor before an invalidation, so what is already
                                    // recorded can be thinned to the new spacing instead of
                                    // thrown away (a wider time/div used to blank the scope)
    double history_target_span;     // Seconds of history the scope wants to see (drives decimation)

    // Error message
    char error_msg[256];
    bool has_error;

    // Short circuit detection
    bool has_short_circuit;
    int short_circuit_comp_ids[8];  // Component IDs involved in short
    int short_circuit_count;        // Number of components in short

    // Open circuit detection (for current sources with no load path)
    bool has_open_circuit;
    int open_circuit_comp_ids[8];   // Component IDs with open circuit
    int open_circuit_count;         // Number of open circuit components

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

// Display-side current flow: exact per-terminal currents for every component (from the
// last converged solution) and a per-net flow solve for the wires. Call once per frame.
void simulation_compute_terminal_currents(Simulation *sim);
void simulation_update_flow_display(Simulation *sim);
// Tell the recorder how many seconds of history the display needs (e.g. 20 x time/div).
// Decimation is re-derived; history is only reset if the factor actually changes.
void simulation_set_history_span(Simulation *sim, double span_seconds);

// Auto-adjust time step based on circuit's highest frequency signal
// Returns the new time step that ensures adequate sampling (at least 50 samples/cycle)
double simulation_auto_time_step(Simulation *sim);
// Pure variants (no side effects):
//  - accuracy step: what auto would pick from the fastest fixed-frequency source
//  - scope step: dt that gives ~50 samples per scope division, never coarser than the
//    accuracy step, snapped down to the 1-2-5 series, clamped to [MIN,MAX]_TIME_STEP
double simulation_accuracy_time_step(Simulation *sim);
double simulation_scope_time_step(Simulation *sim, double scope_time_div);

// Adaptive time-stepping control
void simulation_enable_adaptive(Simulation *sim, bool enable);
bool simulation_is_adaptive_enabled(Simulation *sim);

// Get adaptive stepping statistics for UI display
double simulation_get_adaptive_factor(Simulation *sim);  // Current dt multiplier (1.0 = target)
int simulation_get_step_rejections(Simulation *sim);     // Rejections this frame
double simulation_get_error_estimate(Simulation *sim);   // Estimated error (0-1)

// Get results
double simulation_get_node_voltage(Simulation *sim, int node_id);
double simulation_get_probe_voltage(Simulation *sim, int probe_idx);

// History access
int simulation_get_history(Simulation *sim, int probe_idx,
                           double *times, double *values, int max_points);

/* ---------------------------------------------------------------------------------------
 * What a probe is actually doing, measured rather than assumed.
 *
 * The audits used to judge all 187 templates by one rule: run for thirty divisions, expect a
 * repeating waveform, expect it to have settled. That rule is wrong for most of them. A crystal
 * oscillator needs milliseconds to start and a comparator needs microseconds. A curve tracer has
 * no frequency at all - it is a staircase, and asking what its period is has no answer. A bias
 * network never moves. A one-shot happens once. Judged by one rule, a circuit doing exactly what
 * it should looks broken, and one that has quietly stopped oscillating looks fine.
 *
 * So a suite asks the run what it is looking at, and judges it on those terms.
 * ------------------------------------------------------------------------------------- */
typedef enum {
    SIGNAL_STATIC,      /* does not move: a bias point, a rail, a divider */
    SIGNAL_PERIODIC,    /* repeats: an oscillator, a converter, anything a source drives */
    SIGNAL_ONESHOT,     /* moves once and stops: a step response, a discharge, a monostable */
    SIGNAL_STEPPED      /* moves repeatedly but not periodically: a sweep, a curve tracer */
} SignalClass;

typedef struct {
    SignalClass cls;
    double period;        /* seconds; 0 unless SIGNAL_PERIODIC */
    double frequency;     /* 1/period, or 0 */
    double amplitude;     /* half the peak-to-peak of the record */
    double dc;            /* the level it sits on */
    double settle_time;   /* when the envelope first matched its final value, seconds */
    int cycles;           /* whole cycles in the history */
    int samples;          /* points the history held */
} SignalCharacter;

/* Characterise one probe's recorded history. Reads only what is already there - no stepping and
   no side effects - so a suite may call it whenever it likes. */
void simulation_characterise(Simulation *sim, int probe_idx, SignalCharacter *out);

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
