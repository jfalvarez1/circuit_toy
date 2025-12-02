/**
 * Circuit Playground - Advanced Analysis Implementation
 */

#include "analysis.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdio.h>

// Simple random number generator for Monte Carlo
static unsigned int rand_seed = 12345;
static double rand_uniform(void) {
    rand_seed = rand_seed * 1103515245 + 12345;
    return (double)(rand_seed & 0x7fffffff) / (double)0x7fffffff;
}

// Box-Muller transform for Gaussian random numbers
static double rand_gaussian(double mean, double std_dev) {
    double u1 = rand_uniform();
    double u2 = rand_uniform();
    if (u1 < 1e-10) u1 = 1e-10;
    double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    return mean + z * std_dev;
}

void analysis_init(AnalysisState *state) {
    memset(state, 0, sizeof(AnalysisState));

    state->ambient_temperature = 25.0;  // Room temperature
    state->temperature_sim_enabled = false;

    state->sweep.active = false;
    state->sweep.complete = false;

    state->monte_carlo.active = false;
    state->monte_carlo.complete = false;

    state->fft_enabled = false;
    state->fft_window_type = 1;  // Hanning window default

    // Initialize math channels
    for (int i = 0; i < MAX_PROBES; i++) {
        analysis_math_init(&state->math_channels[i]);
        state->math_values[i] = 0.0;
    }

    // Initialize persistence mode
    state->persistence_enabled = false;
    state->persistence_frames = 8;     // Default 8 frames
    state->persistence_alpha = 0.85;   // Decay factor

    state->auto_measure = true;
    state->cursors_enabled = false;

    state->cursor1.active = false;
    state->cursor2.active = false;

    state->noise_analysis_enabled = false;
}

double analysis_apply_temperature(double base_value, double tempco_ppm,
                                  double ref_temp, double actual_temp) {
    // Temperature coefficient formula: R(T) = R(T0) * (1 + alpha * (T - T0))
    // where alpha = tempco_ppm / 1e6
    double delta_t = actual_temp - ref_temp;
    double alpha = tempco_ppm / 1e6;
    return base_value * (1.0 + alpha * delta_t);
}

double analysis_get_tempco(ComponentType type, int material_type) {
    switch (type) {
        case COMP_RESISTOR:
            switch (material_type) {
                case RESISTOR_CARBON:      return TEMPCO_RESISTOR_CARBON;
                case RESISTOR_METAL_FILM:  return TEMPCO_RESISTOR_METAL;
                case RESISTOR_WIREWOUND:   return TEMPCO_RESISTOR_WIREWOUND;
                case RESISTOR_THICK_FILM:  return 200;  // Typical thick film
                default: return TEMPCO_RESISTOR_CARBON;
            }
        case COMP_CAPACITOR:
            return TEMPCO_CAPACITOR_CERAMIC;
        case COMP_CAPACITOR_ELEC:
            return TEMPCO_CAPACITOR_ELECTROLYTIC;
        case COMP_DIODE:
        case COMP_LED:
        case COMP_ZENER:
        case COMP_SCHOTTKY:
            return TEMPCO_DIODE_VF;
        default:
            return 0;  // No temperature coefficient
    }
}

// Parametric sweep functions
void analysis_sweep_init(AnalysisState *state, int component_id,
                         SweepParamType param, double start, double end,
                         int num_points, bool log_scale) {
    state->sweep.active = true;
    state->sweep.component_id = component_id;
    state->sweep.param_type = param;
    state->sweep.start_value = start;
    state->sweep.end_value = end;
    state->sweep.num_points = (num_points > MAX_SWEEP_POINTS) ?
                              MAX_SWEEP_POINTS : num_points;
    state->sweep.log_scale = log_scale;
    state->sweep.num_results = 0;
    state->sweep.current_point = 0;
    state->sweep.complete = false;
}

void analysis_sweep_step(AnalysisState *state, Circuit *circuit,
                         Simulation *sim, int probe_idx) {
    if (!state->sweep.active || state->sweep.complete) return;

    ParametricSweep *sweep = &state->sweep;

    // Calculate current parameter value
    double param_val;
    if (sweep->log_scale && sweep->start_value > 0 && sweep->end_value > 0) {
        double log_start = log10(sweep->start_value);
        double log_end = log10(sweep->end_value);
        double log_val = log_start + (log_end - log_start) *
                         sweep->current_point / (sweep->num_points - 1);
        param_val = pow(10, log_val);
    } else {
        param_val = sweep->start_value +
                    (sweep->end_value - sweep->start_value) *
                    sweep->current_point / (sweep->num_points - 1);
    }

    // Store result (actual measurement would be done by caller)
    sweep->results[sweep->current_point].param_value = param_val;

    // Get measurement from simulation history
    if (sim && probe_idx >= 0 && probe_idx < MAX_PROBES) {
        double times[MAX_HISTORY], values[MAX_HISTORY];
        int count = simulation_get_history(sim, probe_idx, times, values, 100);

        if (count > 0) {
            double min_v = values[0], max_v = values[0];
            double sum = 0, sum_sq = 0;
            for (int i = 0; i < count; i++) {
                if (values[i] < min_v) min_v = values[i];
                if (values[i] > max_v) max_v = values[i];
                sum += values[i];
                sum_sq += values[i] * values[i];
            }
            sweep->results[sweep->current_point].output_min = min_v;
            sweep->results[sweep->current_point].output_max = max_v;
            sweep->results[sweep->current_point].output_value = (max_v + min_v) / 2;
            sweep->results[sweep->current_point].output_rms =
                sqrt(sum_sq / count);
        }
    }

    sweep->num_results = sweep->current_point + 1;
    sweep->current_point++;

    if (sweep->current_point >= sweep->num_points) {
        sweep->complete = true;
    }
}

void analysis_sweep_reset(AnalysisState *state) {
    state->sweep.active = false;
    state->sweep.complete = false;
    state->sweep.num_results = 0;
    state->sweep.current_point = 0;
}

// Monte Carlo functions
void analysis_monte_carlo_init(AnalysisState *state, int num_runs,
                               bool use_tolerance, double global_tol) {
    state->monte_carlo.active = true;
    state->monte_carlo.num_runs = (num_runs > MAX_MONTE_CARLO_RUNS) ?
                                  MAX_MONTE_CARLO_RUNS : num_runs;
    state->monte_carlo.current_run = 0;
    state->monte_carlo.use_component_tolerance = use_tolerance;
    state->monte_carlo.global_tolerance = global_tol;
    state->monte_carlo.num_results = 0;
    state->monte_carlo.complete = false;
}

void analysis_monte_carlo_run(AnalysisState *state, Circuit *circuit,
                              Simulation *sim, int probe_idx) {
    if (!state->monte_carlo.active || state->monte_carlo.complete) return;

    MonteCarloAnalysis *mc = &state->monte_carlo;

    // Get output value from simulation
    if (sim && probe_idx >= 0) {
        double times[MAX_HISTORY], values[MAX_HISTORY];
        int count = simulation_get_history(sim, probe_idx, times, values, 50);

        if (count > 0) {
            // Use RMS as the output metric
            double sum_sq = 0;
            for (int i = 0; i < count; i++) {
                sum_sq += values[i] * values[i];
            }
            mc->output_values[mc->current_run] = sqrt(sum_sq / count);
            mc->num_results = mc->current_run + 1;
        }
    }

    mc->current_run++;
    if (mc->current_run >= mc->num_runs) {
        mc->complete = true;
        analysis_monte_carlo_stats(state);
    }
}

static int compare_double(const void *a, const void *b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    return (da > db) - (da < db);
}

void analysis_monte_carlo_stats(AnalysisState *state) {
    MonteCarloAnalysis *mc = &state->monte_carlo;
    if (mc->num_results == 0) return;

    // Calculate mean
    double sum = 0;
    for (int i = 0; i < mc->num_results; i++) {
        sum += mc->output_values[i];
    }
    mc->mean = sum / mc->num_results;

    // Calculate standard deviation
    double sum_sq = 0;
    for (int i = 0; i < mc->num_results; i++) {
        double diff = mc->output_values[i] - mc->mean;
        sum_sq += diff * diff;
    }
    mc->std_dev = sqrt(sum_sq / mc->num_results);

    // Sort for min/max and percentiles
    double sorted[MAX_MONTE_CARLO_RUNS];
    memcpy(sorted, mc->output_values, mc->num_results * sizeof(double));
    qsort(sorted, mc->num_results, sizeof(double), compare_double);

    mc->min_val = sorted[0];
    mc->max_val = sorted[mc->num_results - 1];
    mc->percentile_1 = sorted[(int)(mc->num_results * 0.01)];
    mc->percentile_99 = sorted[(int)(mc->num_results * 0.99)];
}

void analysis_monte_carlo_reset(AnalysisState *state) {
    state->monte_carlo.active = false;
    state->monte_carlo.complete = false;
    state->monte_carlo.num_results = 0;
    state->monte_carlo.current_run = 0;
}

// ============================================
// MONTE CARLO COMPONENT VALUE MANIPULATION
// ============================================

// Helper to get primary value for a component
static double mc_get_component_value(Component *comp) {
    switch (comp->type) {
        case COMP_RESISTOR:
            return comp->props.resistor.resistance;
        case COMP_CAPACITOR:
        case COMP_CAPACITOR_ELEC:
            return comp->props.capacitor.capacitance;
        case COMP_INDUCTOR:
            return comp->props.inductor.inductance;
        case COMP_POTENTIOMETER:
            return comp->props.potentiometer.resistance;
        case COMP_DC_VOLTAGE:
            return comp->props.dc_voltage.voltage;
        case COMP_DC_CURRENT:
            return comp->props.dc_current.current;
        case COMP_DIODE:
        case COMP_SCHOTTKY:
            return comp->props.diode.is;  // Saturation current
        case COMP_ZENER:
            return comp->props.zener.vz;  // Zener voltage
        case COMP_LED:
            return comp->props.led.vf;    // Forward voltage
        case COMP_NPN_BJT:
        case COMP_PNP_BJT:
            return comp->props.bjt.bf;    // Beta
        case COMP_NMOS:
        case COMP_PMOS:
            return comp->props.mosfet.vth;  // Threshold voltage
        default:
            return 0.0;
    }
}

// Helper to set primary value for a component
static void mc_set_component_value(Component *comp, double value) {
    switch (comp->type) {
        case COMP_RESISTOR:
            comp->props.resistor.resistance = value;
            break;
        case COMP_CAPACITOR:
        case COMP_CAPACITOR_ELEC:
            comp->props.capacitor.capacitance = value;
            break;
        case COMP_INDUCTOR:
            comp->props.inductor.inductance = value;
            break;
        case COMP_POTENTIOMETER:
            comp->props.potentiometer.resistance = value;
            break;
        case COMP_DC_VOLTAGE:
            comp->props.dc_voltage.voltage = value;
            break;
        case COMP_DC_CURRENT:
            comp->props.dc_current.current = value;
            break;
        case COMP_DIODE:
        case COMP_SCHOTTKY:
            comp->props.diode.is = value;
            break;
        case COMP_ZENER:
            comp->props.zener.vz = value;
            break;
        case COMP_LED:
            comp->props.led.vf = value;
            break;
        case COMP_NPN_BJT:
        case COMP_PNP_BJT:
            comp->props.bjt.bf = value;
            break;
        case COMP_NMOS:
        case COMP_PMOS:
            comp->props.mosfet.vth = value;
            break;
        default:
            break;
    }
}

// Check if component should participate in Monte Carlo
static bool mc_component_has_value(ComponentType type) {
    switch (type) {
        case COMP_RESISTOR:
        case COMP_CAPACITOR:
        case COMP_CAPACITOR_ELEC:
        case COMP_INDUCTOR:
        case COMP_POTENTIOMETER:
        case COMP_DC_VOLTAGE:
        case COMP_DC_CURRENT:
        case COMP_DIODE:
        case COMP_SCHOTTKY:
        case COMP_ZENER:
        case COMP_LED:
        case COMP_NPN_BJT:
        case COMP_PNP_BJT:
        case COMP_NMOS:
        case COMP_PMOS:
            return true;
        default:
            return false;
    }
}

void analysis_mc_backup_values(Circuit *circuit, MCBackup *backup) {
    if (!circuit || !backup) return;

    backup->num_backed_up = 0;
    for (int i = 0; i < circuit->num_components && i < MAX_COMPONENTS; i++) {
        Component *comp = circuit->components[i];
        if (!comp) continue;

        if (mc_component_has_value(comp->type)) {
            backup->values[i] = mc_get_component_value(comp);
        } else {
            backup->values[i] = 0.0;
        }
        backup->num_backed_up = i + 1;
    }
}

void analysis_mc_restore_values(Circuit *circuit, MCBackup *backup) {
    if (!circuit || !backup) return;

    for (int i = 0; i < backup->num_backed_up && i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        if (!comp) continue;

        if (mc_component_has_value(comp->type)) {
            mc_set_component_value(comp, backup->values[i]);
        }
    }
}

void analysis_mc_randomize_values(Circuit *circuit, double tolerance_pct) {
    if (!circuit) return;

    // Standard deviation for Gaussian distribution
    // For ±tolerance%, we use 3-sigma = tolerance, so sigma = tolerance/3
    double sigma_factor = tolerance_pct / 100.0 / 3.0;

    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        if (!comp) continue;

        if (mc_component_has_value(comp->type)) {
            double base_value = mc_get_component_value(comp);
            if (base_value == 0.0) continue;

            // Apply Gaussian variation
            double variation = rand_gaussian(1.0, sigma_factor);
            double new_value = base_value * variation;

            // Ensure positive values for passive components
            if (new_value < 0) new_value = fabs(new_value);

            mc_set_component_value(comp, new_value);
        }
    }
}

bool analysis_monte_carlo_step(AnalysisState *state, Circuit *circuit,
                               Simulation *sim, int probe_idx, MCBackup *backup) {
    if (!state || !circuit || !sim || !backup) return true;

    MonteCarloAnalysis *mc = &state->monte_carlo;

    if (!mc->active || mc->complete) return true;

    // First run: backup original values
    if (mc->current_run == 0) {
        analysis_mc_backup_values(circuit, backup);
    }

    // Restore original values before applying new random variation
    analysis_mc_restore_values(circuit, backup);

    // Apply random variation
    analysis_mc_randomize_values(circuit, mc->global_tolerance);

    // Reset simulation state for clean DC analysis
    simulation_reset(sim);

    // Run DC analysis to get steady-state operating point
    if (!simulation_dc_analysis(sim)) {
        // DC analysis failed, record zero or skip
        mc->output_values[mc->current_run] = 0.0;
    } else {
        // Get the voltage at the probe node
        if (probe_idx >= 0 && probe_idx < circuit->num_probes) {
            mc->output_values[mc->current_run] = circuit->probes[probe_idx].voltage;
        } else if (circuit->num_probes > 0) {
            // Use first probe if index invalid
            mc->output_values[mc->current_run] = circuit->probes[0].voltage;
        } else {
            // No probes, try to get a meaningful voltage
            mc->output_values[mc->current_run] = 0.0;
        }
    }

    mc->num_results = mc->current_run + 1;
    mc->current_run++;

    // Check if complete
    if (mc->current_run >= mc->num_runs) {
        mc->complete = true;

        // Restore original values
        analysis_mc_restore_values(circuit, backup);

        // Calculate statistics
        analysis_monte_carlo_stats(state);

        // Reset simulation back to normal
        simulation_reset(sim);

        return true;  // Complete
    }

    return false;  // Not complete, more runs needed
}

// FFT functions
void analysis_fft_window(double *samples, int num_samples, int window_type) {
    for (int i = 0; i < num_samples; i++) {
        double w = 1.0;
        double n = (double)i / (num_samples - 1);

        switch (window_type) {
            case 0:  // Rectangular
                w = 1.0;
                break;
            case 1:  // Hanning
                w = 0.5 * (1.0 - cos(2.0 * M_PI * n));
                break;
            case 2:  // Hamming
                w = 0.54 - 0.46 * cos(2.0 * M_PI * n);
                break;
            case 3:  // Blackman
                w = 0.42 - 0.5 * cos(2.0 * M_PI * n) +
                    0.08 * cos(4.0 * M_PI * n);
                break;
        }
        samples[i] *= w;
    }
}

// ============================================================================
// COOLEY-TUKEY RADIX-2 FFT - O(n log n) implementation
// ~100x faster than DFT for N=1024 (10,240 vs 1,048,576 operations)
// ============================================================================

// Pre-computed twiddle factors for FFT (computed once, reused)
static double fft_cos_table[FFT_SIZE];
static double fft_sin_table[FFT_SIZE];
static int fft_tables_initialized = 0;

// Bit-reversal permutation lookup table
static int fft_bit_rev[FFT_SIZE];

// Initialize FFT lookup tables (called once)
static void fft_init_tables(void) {
    if (fft_tables_initialized) return;

    int N = FFT_SIZE;

    // Pre-compute twiddle factors: W_N^k = e^(-2*pi*i*k/N) = cos - i*sin
    for (int k = 0; k < N; k++) {
        double angle = -2.0 * M_PI * k / N;
        fft_cos_table[k] = cos(angle);
        fft_sin_table[k] = sin(angle);
    }

    // Pre-compute bit-reversal indices
    int log2N = 0;
    for (int temp = N; temp > 1; temp >>= 1) log2N++;

    for (int i = 0; i < N; i++) {
        int rev = 0;
        int x = i;
        for (int j = 0; j < log2N; j++) {
            rev = (rev << 1) | (x & 1);
            x >>= 1;
        }
        fft_bit_rev[i] = rev;
    }

    fft_tables_initialized = 1;
}

// In-place Cooley-Tukey radix-2 decimation-in-time FFT
// real[] and imag[] must be arrays of size N (power of 2)
static void fft_transform(double *real, double *imag, int N) {
    // Ensure lookup tables are ready
    fft_init_tables();

    // Bit-reversal permutation
    for (int i = 0; i < N; i++) {
        int j = fft_bit_rev[i];
        if (i < j) {
            // Swap real[i] and real[j]
            double temp = real[i];
            real[i] = real[j];
            real[j] = temp;
            // Swap imag[i] and imag[j]
            temp = imag[i];
            imag[i] = imag[j];
            imag[j] = temp;
        }
    }

    // Cooley-Tukey iterative FFT
    for (int size = 2; size <= N; size *= 2) {
        int halfsize = size / 2;
        int step = N / size;  // Twiddle factor step

        for (int i = 0; i < N; i += size) {
            int k = 0;  // Twiddle index
            for (int j = i; j < i + halfsize; j++) {
                // Butterfly operation
                double cos_w = fft_cos_table[k];
                double sin_w = fft_sin_table[k];

                // t = W * x[j + halfsize]
                double t_re = cos_w * real[j + halfsize] - sin_w * imag[j + halfsize];
                double t_im = cos_w * imag[j + halfsize] + sin_w * real[j + halfsize];

                // x[j + halfsize] = x[j] - t
                real[j + halfsize] = real[j] - t_re;
                imag[j + halfsize] = imag[j] - t_im;

                // x[j] = x[j] + t
                real[j] += t_re;
                imag[j] += t_im;

                k += step;
            }
        }
    }
}

// Optimized FFT computation using Cooley-Tukey algorithm
void analysis_fft_compute(AnalysisState *state, double *samples,
                          int num_samples, double sample_rate, int channel) {
    if (channel < 0 || channel >= MAX_PROBES) return;
    if (num_samples <= 0 || num_samples > FFT_SIZE) return;

    FFTResult *fft = &state->fft_results[channel];

    // Apply window function to windowed copy
    double windowed[FFT_SIZE];
    memcpy(windowed, samples, num_samples * sizeof(double));
    analysis_fft_window(windowed, num_samples, state->fft_window_type);

    // Zero-pad to FFT_SIZE
    for (int i = num_samples; i < FFT_SIZE; i++) {
        windowed[i] = 0;
    }

    // Prepare real and imaginary arrays for FFT
    static double fft_real[FFT_SIZE];
    static double fft_imag[FFT_SIZE];

    memcpy(fft_real, windowed, FFT_SIZE * sizeof(double));
    memset(fft_imag, 0, FFT_SIZE * sizeof(double));  // Input is real-only

    // Perform FFT transform - O(n log n)
    fft_transform(fft_real, fft_imag, FFT_SIZE);

    // Extract magnitude and phase from complex output
    int N = FFT_SIZE;
    fft->num_bins = N / 2;

    for (int k = 0; k < N / 2; k++) {
        double mag = sqrt(fft_real[k] * fft_real[k] + fft_imag[k] * fft_imag[k]) / N;
        double phase_rad = atan2(fft_imag[k], fft_real[k]);

        fft->frequency[k] = (double)k * sample_rate / N;
        fft->magnitude[k] = (mag > 1e-10) ? 20.0 * log10(mag) : -200.0;
        fft->phase[k] = phase_rad * 180.0 / M_PI;
    }

    // Find fundamental frequency (largest bin above DC, skip bin 0)
    double max_mag = -1000;
    int max_bin = 1;
    for (int k = 1; k < N / 2; k++) {
        if (fft->magnitude[k] > max_mag) {
            max_mag = fft->magnitude[k];
            max_bin = k;
        }
    }
    fft->fundamental_freq = fft->frequency[max_bin];

    // Calculate THD and SNR
    fft->thd = analysis_calculate_thd(fft);
    fft->snr = analysis_calculate_snr(fft);
}

double analysis_calculate_thd(FFTResult *fft) {
    // Find fundamental (largest peak)
    double max_mag_linear = 0;
    int fund_bin = 1;
    for (int k = 1; k < fft->num_bins; k++) {
        double mag_linear = pow(10, fft->magnitude[k] / 20.0);
        if (mag_linear > max_mag_linear) {
            max_mag_linear = mag_linear;
            fund_bin = k;
        }
    }

    if (max_mag_linear < 1e-10) return 0;

    // Sum harmonics (2nd through 10th)
    double harmonic_sum_sq = 0;
    for (int h = 2; h <= 10; h++) {
        int harm_bin = fund_bin * h;
        if (harm_bin < fft->num_bins) {
            double mag_linear = pow(10, fft->magnitude[harm_bin] / 20.0);
            harmonic_sum_sq += mag_linear * mag_linear;
        }
    }

    return 100.0 * sqrt(harmonic_sum_sq) / max_mag_linear;
}

double analysis_calculate_snr(FFTResult *fft) {
    // Find signal power (fundamental)
    double max_mag_linear = 0;
    int fund_bin = 1;
    for (int k = 1; k < fft->num_bins; k++) {
        double mag_linear = pow(10, fft->magnitude[k] / 20.0);
        if (mag_linear > max_mag_linear) {
            max_mag_linear = mag_linear;
            fund_bin = k;
        }
    }

    double signal_power = max_mag_linear * max_mag_linear;

    // Sum noise (everything except fundamental and nearby bins)
    double noise_power = 0;
    int exclude_width = 3;  // Exclude bins near fundamental
    for (int k = 1; k < fft->num_bins; k++) {
        if (abs(k - fund_bin) > exclude_width) {
            double mag_linear = pow(10, fft->magnitude[k] / 20.0);
            noise_power += mag_linear * mag_linear;
        }
    }

    if (noise_power < 1e-20) return 100.0;  // Very high SNR

    return 10.0 * log10(signal_power / noise_power);
}

// Waveform measurements
void analysis_measure_waveform(WaveformMeasurements *meas,
                               double *times, double *values, int count) {
    if (count < 2) {
        meas->valid = false;
        return;
    }

    // Min, max, peak-to-peak
    meas->v_min = values[0];
    meas->v_max = values[0];
    double sum = 0, sum_sq = 0;

    for (int i = 0; i < count; i++) {
        if (values[i] < meas->v_min) meas->v_min = values[i];
        if (values[i] > meas->v_max) meas->v_max = values[i];
        sum += values[i];
        sum_sq += values[i] * values[i];
    }

    meas->v_pp = meas->v_max - meas->v_min;
    meas->v_avg = sum / count;
    meas->v_rms = sqrt(sum_sq / count);
    meas->v_dc_offset = meas->v_avg;

    // Frequency measurement
    meas->frequency = analysis_measure_frequency(times, values, count);
    if (meas->frequency > 0) {
        meas->period = 1.0 / meas->frequency;
    } else {
        meas->period = 0;
    }

    // Rise/fall time
    analysis_measure_rise_fall_time(times, values, count,
                                    &meas->rise_time, &meas->fall_time);

    // Duty cycle (time above midpoint / period)
    double midpoint = (meas->v_max + meas->v_min) / 2.0;
    double high_time = 0;
    for (int i = 1; i < count; i++) {
        if (values[i] > midpoint && values[i-1] > midpoint) {
            high_time += times[i] - times[i-1];
        }
    }
    double total_time = times[count-1] - times[0];
    meas->duty_cycle = (total_time > 0) ? 100.0 * high_time / total_time : 50.0;
    meas->pulse_width = high_time;

    meas->valid = true;
}

double analysis_measure_frequency(double *times, double *values, int count) {
    if (count < 4) return 0;

    // Find zero crossings (or midpoint crossings)
    double midpoint = 0;
    for (int i = 0; i < count; i++) {
        midpoint += values[i];
    }
    midpoint /= count;

    // Count rising zero crossings
    int crossings = 0;
    double first_crossing = -1, last_crossing = -1;

    for (int i = 1; i < count; i++) {
        if (values[i-1] < midpoint && values[i] >= midpoint) {
            // Rising edge crossing
            // Interpolate for more accuracy
            double frac = (midpoint - values[i-1]) / (values[i] - values[i-1]);
            double crossing_time = times[i-1] + frac * (times[i] - times[i-1]);

            if (first_crossing < 0) {
                first_crossing = crossing_time;
            }
            last_crossing = crossing_time;
            crossings++;
        }
    }

    if (crossings < 2) return 0;

    // Frequency = (number of cycles) / (time span)
    double time_span = last_crossing - first_crossing;
    if (time_span <= 0) return 0;

    return (crossings - 1) / time_span;
}

double analysis_measure_rms(double *values, int count) {
    if (count <= 0) return 0;

    double sum_sq = 0;
    for (int i = 0; i < count; i++) {
        sum_sq += values[i] * values[i];
    }
    return sqrt(sum_sq / count);
}

double analysis_measure_phase(double *times1, double *values1,
                              double *times2, double *values2, int count) {
    if (count < 4) return 0;

    // Find first rising zero crossing of each signal
    double mid1 = 0, mid2 = 0;
    for (int i = 0; i < count; i++) {
        mid1 += values1[i];
        mid2 += values2[i];
    }
    mid1 /= count;
    mid2 /= count;

    double cross1 = -1, cross2 = -1;

    for (int i = 1; i < count && (cross1 < 0 || cross2 < 0); i++) {
        if (cross1 < 0 && values1[i-1] < mid1 && values1[i] >= mid1) {
            double frac = (mid1 - values1[i-1]) / (values1[i] - values1[i-1]);
            cross1 = times1[i-1] + frac * (times1[i] - times1[i-1]);
        }
        if (cross2 < 0 && values2[i-1] < mid2 && values2[i] >= mid2) {
            double frac = (mid2 - values2[i-1]) / (values2[i] - values2[i-1]);
            cross2 = times2[i-1] + frac * (times2[i] - times2[i-1]);
        }
    }

    if (cross1 < 0 || cross2 < 0) return 0;

    // Measure frequency to convert time difference to phase
    double freq = analysis_measure_frequency(times1, values1, count);
    if (freq <= 0) return 0;

    double period = 1.0 / freq;
    double time_diff = cross2 - cross1;

    // Normalize to -180 to +180 degrees
    double phase = 360.0 * time_diff / period;
    while (phase > 180) phase -= 360;
    while (phase < -180) phase += 360;

    return phase;
}

void analysis_measure_rise_fall_time(double *times, double *values, int count,
                                     double *rise_time, double *fall_time) {
    *rise_time = 0;
    *fall_time = 0;

    if (count < 4) return;

    // Find min and max
    double v_min = values[0], v_max = values[0];
    for (int i = 0; i < count; i++) {
        if (values[i] < v_min) v_min = values[i];
        if (values[i] > v_max) v_max = values[i];
    }

    double v_10 = v_min + 0.1 * (v_max - v_min);
    double v_90 = v_min + 0.9 * (v_max - v_min);

    // Find first rising edge
    double t_10_rise = -1, t_90_rise = -1;
    for (int i = 1; i < count; i++) {
        if (t_10_rise < 0 && values[i-1] < v_10 && values[i] >= v_10) {
            double frac = (v_10 - values[i-1]) / (values[i] - values[i-1]);
            t_10_rise = times[i-1] + frac * (times[i] - times[i-1]);
        }
        if (t_10_rise >= 0 && t_90_rise < 0 && values[i-1] < v_90 && values[i] >= v_90) {
            double frac = (v_90 - values[i-1]) / (values[i] - values[i-1]);
            t_90_rise = times[i-1] + frac * (times[i] - times[i-1]);
            break;
        }
    }

    if (t_10_rise >= 0 && t_90_rise >= 0) {
        *rise_time = t_90_rise - t_10_rise;
    }

    // Find first falling edge
    double t_90_fall = -1, t_10_fall = -1;
    for (int i = 1; i < count; i++) {
        if (t_90_fall < 0 && values[i-1] > v_90 && values[i] <= v_90) {
            double frac = (v_90 - values[i-1]) / (values[i] - values[i-1]);
            t_90_fall = times[i-1] + frac * (times[i] - times[i-1]);
        }
        if (t_90_fall >= 0 && t_10_fall < 0 && values[i-1] > v_10 && values[i] <= v_10) {
            double frac = (v_10 - values[i-1]) / (values[i] - values[i-1]);
            t_10_fall = times[i-1] + frac * (times[i] - times[i-1]);
            break;
        }
    }

    if (t_90_fall >= 0 && t_10_fall >= 0) {
        *fall_time = t_10_fall - t_90_fall;
    }
}

// Cursor functions
double analysis_cursor_delta_time(AnalysisState *state) {
    if (!state->cursor1.active || !state->cursor2.active) return 0;
    return state->cursor2.time - state->cursor1.time;
}

double analysis_cursor_delta_value(AnalysisState *state) {
    if (!state->cursor1.active || !state->cursor2.active) return 0;
    return state->cursor2.value - state->cursor1.value;
}

double analysis_cursor_frequency(AnalysisState *state) {
    double dt = analysis_cursor_delta_time(state);
    if (fabs(dt) < 1e-12) return 0;
    return 1.0 / fabs(dt);
}

// Noise analysis
double analysis_estimate_noise_floor(double *values, int count) {
    if (count < 10) return 0;

    // Use median absolute deviation for robust noise estimation
    // First, compute differences (approximates derivative/noise)
    double diffs[MAX_HISTORY];
    for (int i = 1; i < count && i < MAX_HISTORY; i++) {
        diffs[i-1] = fabs(values[i] - values[i-1]);
    }
    int num_diffs = count - 1;
    if (num_diffs > MAX_HISTORY - 1) num_diffs = MAX_HISTORY - 1;

    // Sort to find median
    qsort(diffs, num_diffs, sizeof(double), compare_double);
    double median_diff = diffs[num_diffs / 2];

    // MAD-based noise estimate (scaled by 1.4826 for Gaussian)
    double noise_rms = median_diff * 1.4826 / sqrt(2.0);

    // Convert to dBV
    if (noise_rms < 1e-12) return -240;
    return 20.0 * log10(noise_rms);
}

double analysis_calculate_snr_from_signal(double *signal, double *noise, int count) {
    if (count <= 0) return 0;

    double signal_power = 0, noise_power = 0;
    for (int i = 0; i < count; i++) {
        signal_power += signal[i] * signal[i];
        noise_power += noise[i] * noise[i];
    }

    if (noise_power < 1e-20) return 100.0;
    return 10.0 * log10(signal_power / noise_power);
}

// ============================================
// MATH CHANNEL OPERATIONS
// ============================================

void analysis_math_init(MathChannel *math) {
    math->enabled = false;
    math->operation = MATH_NONE;
    math->source_a = 0;
    math->source_b = 1;
    math->scale = 1.0;
    math->offset = 0.0;
    math->integral_value = 0.0;
}

double analysis_math_compute(MathChannel *math, double val_a, double val_b,
                             double prev_val_a, double dt) {
    if (!math->enabled) return 0.0;

    double result = 0.0;

    switch (math->operation) {
        case MATH_NONE:
            result = val_a;
            break;

        case MATH_ADD:
            result = val_a + val_b;
            break;

        case MATH_SUBTRACT:
            result = val_a - val_b;
            break;

        case MATH_MULTIPLY:
            result = val_a * val_b;
            break;

        case MATH_DIVIDE:
            result = (fabs(val_b) > 1e-12) ? val_a / val_b : 0.0;
            break;

        case MATH_DERIVATIVE:
            // dA/dt - numerical derivative
            result = (dt > 1e-15) ? (val_a - prev_val_a) / dt : 0.0;
            break;

        case MATH_INTEGRAL:
            // Trapezoidal integration
            math->integral_value += 0.5 * (val_a + prev_val_a) * dt;
            result = math->integral_value;
            break;

        case MATH_ABS:
            result = fabs(val_a);
            break;

        case MATH_INVERT:
            result = -val_a;
            break;

        case MATH_LOG:
            result = (fabs(val_a) > 1e-12) ? log10(fabs(val_a)) : -12.0;
            break;

        case MATH_SQRT:
            result = (val_a >= 0) ? sqrt(val_a) : -sqrt(-val_a);
            break;

        default:
            result = val_a;
            break;
    }

    return result * math->scale + math->offset;
}

void analysis_math_update_all(AnalysisState *state, double *channel_values,
                              double *prev_values, double dt) {
    for (int i = 0; i < MAX_PROBES; i++) {
        MathChannel *math = &state->math_channels[i];
        if (!math->enabled) {
            state->math_values[i] = 0.0;
            continue;
        }

        int src_a = math->source_a;
        int src_b = math->source_b;

        // Bounds check
        if (src_a < 0 || src_a >= MAX_PROBES) src_a = 0;
        if (src_b < 0 || src_b >= MAX_PROBES) src_b = 0;

        double val_a = channel_values[src_a];
        double val_b = channel_values[src_b];
        double prev_a = prev_values ? prev_values[src_a] : val_a;

        state->math_values[i] = analysis_math_compute(math, val_a, val_b, prev_a, dt);
    }
}

// ============================================
// ENHANCED CURSOR MEASUREMENTS
// ============================================

double analysis_cursor_slew_rate(AnalysisState *state) {
    // Slew rate = dV/dt between cursors (V/s)
    double dt = analysis_cursor_delta_time(state);
    double dv = analysis_cursor_delta_value(state);

    if (fabs(dt) < 1e-15) return 0.0;
    return dv / dt;
}

// ============================================
// CSV EXPORT FUNCTIONS
// ============================================

bool analysis_export_csv(const char *filename, double *times,
                         double values[][1024], int num_channels, int num_points) {
    if (!filename || !times || num_channels <= 0 || num_points <= 0) {
        return false;
    }

    FILE *fp = fopen(filename, "w");
    if (!fp) return false;

    // Write header
    fprintf(fp, "Time(s)");
    for (int ch = 0; ch < num_channels; ch++) {
        fprintf(fp, ",CH%d(V)", ch + 1);
    }
    fprintf(fp, "\n");

    // Write data rows
    for (int i = 0; i < num_points; i++) {
        fprintf(fp, "%.9e", times[i]);
        for (int ch = 0; ch < num_channels; ch++) {
            fprintf(fp, ",%.9e", values[ch][i]);
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
    return true;
}

bool analysis_export_measurements_csv(const char *filename,
                                      WaveformMeasurements *meas, int num_channels) {
    if (!filename || !meas || num_channels <= 0) {
        return false;
    }

    FILE *fp = fopen(filename, "w");
    if (!fp) return false;

    // Write header
    fprintf(fp, "Channel,Vmin(V),Vmax(V),Vpp(V),Vavg(V),Vrms(V),Freq(Hz),Period(s),RiseTime(s),FallTime(s),DutyCycle(%%)\n");

    // Write measurements for each channel
    for (int ch = 0; ch < num_channels; ch++) {
        if (!meas[ch].valid) continue;

        fprintf(fp, "CH%d,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.6e,%.2f\n",
                ch + 1,
                meas[ch].v_min,
                meas[ch].v_max,
                meas[ch].v_pp,
                meas[ch].v_avg,
                meas[ch].v_rms,
                meas[ch].frequency,
                meas[ch].period,
                meas[ch].rise_time,
                meas[ch].fall_time,
                meas[ch].duty_cycle);
    }

    fclose(fp);
    return true;
}
