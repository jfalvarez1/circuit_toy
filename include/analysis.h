/**
 * Circuit Playground - Advanced Analysis Tools
 *
 * Features:
 * - Temperature analysis with component temperature coefficients
 * - Parametric sweep analysis
 * - Monte Carlo statistical analysis
 * - FFT spectrum analysis
 * - Advanced waveform measurements
 */

#ifndef ANALYSIS_H
#define ANALYSIS_H

#include "types.h"
#include "circuit.h"
#include "simulation.h"
#include <stdbool.h>

// Maximum sweep/Monte Carlo points
#define MAX_SWEEP_POINTS 100
#define MAX_MONTE_CARLO_RUNS 1000
#define FFT_SIZE 1024
#define MAX_PERSISTENCE_FRAMES 32

// Math channel operations
typedef enum {
    MATH_NONE = 0,
    MATH_ADD,           // A + B
    MATH_SUBTRACT,      // A - B
    MATH_MULTIPLY,      // A * B
    MATH_DIVIDE,        // A / B
    MATH_DERIVATIVE,    // dA/dt
    MATH_INTEGRAL,      // ∫A dt
    MATH_ABS,           // |A|
    MATH_INVERT,        // -A
    MATH_LOG,           // log10(|A|)
    MATH_SQRT,          // sqrt(|A|)
    MATH_OP_COUNT
} MathOperation;

// Math channel configuration
typedef struct {
    bool enabled;
    MathOperation operation;
    int source_a;           // Source channel A (0-7)
    int source_b;           // Source channel B (for binary ops)
    double scale;           // Output scaling factor
    double offset;          // Output offset
    double integral_value;  // Running integral accumulator
} MathChannel;

// Temperature coefficients (ppm/°C typical values)
#define TEMPCO_RESISTOR_CARBON   1500    // Carbon film: 1500 ppm/°C
#define TEMPCO_RESISTOR_METAL    50      // Metal film: 50 ppm/°C
#define TEMPCO_RESISTOR_WIREWOUND 20     // Wirewound: 20 ppm/°C
#define TEMPCO_CAPACITOR_CERAMIC 30      // Ceramic NP0: ±30 ppm/°C
#define TEMPCO_CAPACITOR_ELECTROLYTIC 1000  // Electrolytic: ~1000 ppm/°C
#define TEMPCO_DIODE_VF -2000            // Diode Vf: -2mV/°C typical

// Resistor material types (affects temperature coefficient)
typedef enum {
    RESISTOR_CARBON = 0,
    RESISTOR_METAL_FILM,
    RESISTOR_WIREWOUND,
    RESISTOR_THICK_FILM,
    RESISTOR_MATERIAL_COUNT
} ResistorMaterial;

// Sweep parameter type
typedef enum {
    SWEEP_RESISTANCE = 0,
    SWEEP_CAPACITANCE,
    SWEEP_INDUCTANCE,
    SWEEP_VOLTAGE,
    SWEEP_FREQUENCY,
    SWEEP_TEMPERATURE,
    SWEEP_PARAM_COUNT
} SweepParamType;

// Sweep result
typedef struct {
    double param_value;       // Parameter value at this point
    double output_value;      // Measured output (voltage, current, etc.)
    double output_min;        // Min during this sweep point
    double output_max;        // Max during this sweep point
    double output_rms;        // RMS value
} SweepPoint;

// Parametric sweep configuration
typedef struct {
    bool active;
    int component_id;         // Component being swept
    SweepParamType param_type;
    double start_value;
    double end_value;
    int num_points;
    bool log_scale;           // Use logarithmic spacing

    // Results
    SweepPoint results[MAX_SWEEP_POINTS];
    int num_results;
    int current_point;        // Current sweep point (for progress)
    bool complete;
} ParametricSweep;

// Monte Carlo configuration
typedef struct {
    bool active;
    int num_runs;
    int current_run;
    bool use_component_tolerance;  // Use each component's tolerance
    double global_tolerance;       // Or use global tolerance (%)

    // Results
    double output_values[MAX_MONTE_CARLO_RUNS];
    int num_results;
    double mean;
    double std_dev;
    double min_val;
    double max_val;
    double percentile_1;      // 1% worst case
    double percentile_99;     // 99% worst case
    bool complete;
} MonteCarloAnalysis;

// FFT result
typedef struct {
    double frequency[FFT_SIZE / 2];
    double magnitude[FFT_SIZE / 2];   // dB
    double phase[FFT_SIZE / 2];       // degrees
    int num_bins;
    double fundamental_freq;
    double thd;               // Total Harmonic Distortion (%)
    double snr;               // Signal-to-Noise Ratio (dB)
} FFTResult;

// Waveform measurements
typedef struct {
    // Voltage measurements
    double v_min;
    double v_max;
    double v_pp;              // Peak-to-peak
    double v_avg;             // Average (DC)
    double v_rms;             // RMS
    double v_dc_offset;       // DC offset

    // Timing measurements
    double frequency;         // Hz
    double period;            // seconds
    double rise_time;         // 10% to 90%
    double fall_time;         // 90% to 10%
    double duty_cycle;        // %
    double pulse_width;       // seconds (high time)

    // Phase measurement (relative to reference channel)
    double phase;             // degrees

    // Power measurements
    double power_avg;         // Average power (W)
    double power_rms;         // RMS power

    bool valid;               // Measurements are valid
} WaveformMeasurements;

// Measurement cursor
typedef struct {
    bool active;
    double time;              // Time position
    double value;             // Measured value at cursor
    int channel;              // Which channel
} MeasurementCursor;

// Analysis state
typedef struct {
    // Global temperature setting
    double ambient_temperature;   // °C (default 25°C)
    bool temperature_sim_enabled;

    // Parametric sweep
    ParametricSweep sweep;

    // Monte Carlo
    MonteCarloAnalysis monte_carlo;

    // FFT
    FFTResult fft_results[MAX_PROBES];
    bool fft_enabled;
    int fft_window_type;      // 0=rectangular, 1=Hanning, 2=Hamming, 3=Blackman

    // Math channels (computed from probe channels)
    MathChannel math_channels[MAX_PROBES];
    double math_values[MAX_PROBES];  // Current computed math channel values

    // Persistence mode (phosphor-like decay)
    bool persistence_enabled;
    int persistence_frames;          // Number of frames to persist (1-32)
    double persistence_alpha;        // Decay factor per frame (0.0-1.0)

    // Measurements
    WaveformMeasurements measurements[MAX_PROBES];
    bool auto_measure;        // Continuously update measurements

    // Cursors
    MeasurementCursor cursor1;
    MeasurementCursor cursor2;
    bool cursors_enabled;

    // Noise analysis
    double noise_floor_dbv;   // Estimated noise floor
    bool noise_analysis_enabled;
} AnalysisState;

// Initialize analysis state
void analysis_init(AnalysisState *state);

// Temperature-adjusted component values
double analysis_apply_temperature(double base_value, double tempco_ppm,
                                  double ref_temp, double actual_temp);

// Get temperature coefficient for component type
double analysis_get_tempco(ComponentType type, int material_type);

// Parametric sweep
void analysis_sweep_init(AnalysisState *state, int component_id,
                         SweepParamType param, double start, double end,
                         int num_points, bool log_scale);
void analysis_sweep_step(AnalysisState *state, Circuit *circuit,
                         Simulation *sim, int probe_idx);
void analysis_sweep_reset(AnalysisState *state);

// Monte Carlo analysis
void analysis_monte_carlo_init(AnalysisState *state, int num_runs,
                               bool use_tolerance, double global_tol);
void analysis_monte_carlo_run(AnalysisState *state, Circuit *circuit,
                              Simulation *sim, int probe_idx);
void analysis_monte_carlo_stats(AnalysisState *state);
void analysis_monte_carlo_reset(AnalysisState *state);

// Monte Carlo component value manipulation
// Backup arrays for component values during MC analysis
typedef struct {
    double values[MAX_COMPONENTS];    // Original component primary values
    int num_backed_up;
} MCBackup;

// Save original component values before MC run
void analysis_mc_backup_values(Circuit *circuit, MCBackup *backup);

// Restore original component values after MC run
void analysis_mc_restore_values(Circuit *circuit, MCBackup *backup);

// Apply random Gaussian variation to component values
// tolerance_pct: tolerance percentage (e.g., 10.0 for 10%)
void analysis_mc_randomize_values(Circuit *circuit, double tolerance_pct);

// Complete Monte Carlo analysis - runs all iterations
// Returns true when complete
bool analysis_monte_carlo_step(AnalysisState *state, Circuit *circuit,
                               Simulation *sim, int probe_idx, MCBackup *backup);

// FFT analysis
void analysis_fft_compute(AnalysisState *state, double *samples,
                          int num_samples, double sample_rate, int channel);
void analysis_fft_window(double *samples, int num_samples, int window_type);
double analysis_calculate_thd(FFTResult *fft);
double analysis_calculate_snr(FFTResult *fft);

// Waveform measurements
void analysis_measure_waveform(WaveformMeasurements *meas,
                               double *times, double *values, int count);
double analysis_measure_frequency(double *times, double *values, int count);
double analysis_measure_rms(double *values, int count);
double analysis_measure_phase(double *times1, double *values1,
                              double *times2, double *values2, int count);
void analysis_measure_rise_fall_time(double *times, double *values, int count,
                                     double *rise_time, double *fall_time);

// Cursor measurements
double analysis_cursor_delta_time(AnalysisState *state);
double analysis_cursor_delta_value(AnalysisState *state);
double analysis_cursor_frequency(AnalysisState *state);

// Noise analysis
double analysis_estimate_noise_floor(double *values, int count);
double analysis_calculate_snr_from_signal(double *signal, double *noise, int count);

// Math channel operations
void analysis_math_init(MathChannel *math);
double analysis_math_compute(MathChannel *math, double val_a, double val_b,
                             double prev_val_a, double dt);
void analysis_math_update_all(AnalysisState *state, double *channel_values,
                              double *prev_values, double dt);

// Enhanced cursor measurements
double analysis_cursor_slew_rate(AnalysisState *state);  // V/s between cursors

// CSV export
bool analysis_export_csv(const char *filename, double *times,
                         double values[][1024], int num_channels, int num_points);
bool analysis_export_measurements_csv(const char *filename,
                                      WaveformMeasurements *meas, int num_channels);

#endif // ANALYSIS_H
