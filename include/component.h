/**
 * Circuit Playground - Component Definitions
 */

#ifndef COMPONENT_H
#define COMPONENT_H

#include "types.h"
#include "matrix.h"

// Maximum terminals per component
#define MAX_TERMINALS 4

// Terminal definition
typedef struct {
    float dx;      // Offset from component center
    float dy;
    char name[8];  // Terminal name (e.g., "+", "-", "B", "C", "E")
} TerminalDef;

// Component properties union
typedef union {
    // DC Voltage Source
    struct {
        double voltage;         // Output voltage (V)
        double r_series;        // Internal series resistance (Ohm), default: 0.001
        bool ideal;             // Ideal mode (zero internal resistance)
        SweepConfig voltage_sweep;  // Voltage sweep (stepped or ramped)
    } dc_voltage;

    // AC Voltage Source
    struct {
        double amplitude;       // Peak amplitude (V)
        double frequency;       // Frequency (Hz)
        double phase;           // Phase (degrees)
        double offset;          // DC offset (V)
        double r_series;        // Internal series resistance (Ohm), default: 0.001
        bool ideal;             // Ideal mode (zero internal resistance)
        SweepConfig amplitude_sweep;   // Amplitude sweep
        SweepConfig frequency_sweep;   // Frequency sweep
    } ac_voltage;

    // DC Current Source
    struct {
        double current;         // Output current (A)
        double r_parallel;      // Internal parallel resistance (Ohm), default: 1e9
        bool ideal;             // Ideal mode (infinite internal resistance)
        SweepConfig current_sweep;  // Current sweep (stepped or ramped)
    } dc_current;

    // Resistor
    struct {
        double resistance;      // Resistance (Ohm)
        double tolerance;       // Tolerance (%)
        double power_rating;    // Max power dissipation (W)
        double power_dissipated; // Current power dissipation (W)
        double temp_coeff;      // Temperature coefficient (ppm/°C), default: 100
        double temp;            // Operating temperature (°C), default: 25
        bool ideal;             // Ideal mode (no temperature effects)
    } resistor;

    // Capacitor
    struct {
        double capacitance;     // Capacitance (F)
        double voltage;         // Current voltage (state variable)
        double esr;             // Equivalent Series Resistance (Ohm), default: 0.01
        double esl;             // Equivalent Series Inductance (H), default: 1e-9
        double leakage;         // Leakage resistance (Ohm), default: 1e9
        bool ideal;             // Ideal mode (no parasitics)
    } capacitor;

    // Electrolytic Capacitor
    struct {
        double capacitance;     // Capacitance (F)
        double voltage;         // Current voltage (state variable)
        double max_voltage;     // Voltage rating (V)
        double esr;             // ESR (Ohm), typically higher than film caps
        double leakage;         // Leakage resistance (Ohm)
        bool ideal;             // Ideal mode
    } capacitor_elec;

    // Inductor
    struct {
        double inductance;      // Inductance (H)
        double current;         // Current (state variable)
        double dcr;             // DC resistance (Ohm), default: 0.1
        double r_parallel;      // Parallel resistance for core losses (Ohm), default: 1e6
        double i_sat;           // Saturation current (A), default: 1.0
        bool ideal;             // Ideal mode (no DCR, no saturation)
    } inductor;

    // Standard Diode
    struct {
        double is;              // Saturation current (A)
        double vt;              // Thermal voltage (V), ~0.026 at room temp
        double n;               // Ideality factor
        double bv;              // Reverse breakdown voltage (V), default: 100
        double ibv;             // Current at breakdown (A), default: 1e-10
        double cjo;             // Zero-bias junction capacitance (F), default: 1e-12
        bool ideal;             // Ideal mode (simple Vf drop)
    } diode;

    // Zener Diode
    struct {
        double is;              // Saturation current (A)
        double vt;              // Thermal voltage (V)
        double n;               // Ideality factor
        double vz;              // Zener breakdown voltage (V)
        double rz;              // Zener impedance (Ohm), default: 5
        double iz_test;         // Test current for Vz (A), default: 5e-3
        bool ideal;             // Ideal mode (perfect clamping at Vz)
    } zener;

    // Schottky Diode
    struct {
        double is;              // Saturation current (A) - typically higher than Si
        double vt;              // Thermal voltage (V)
        double n;               // Ideality factor - typically 1.0-1.1
        double vf;              // Typical forward voltage (V), default: 0.3
        double cjo;             // Junction capacitance (F)
        bool ideal;             // Ideal mode
    } schottky;

    // LED
    struct {
        double is;              // Saturation current (A)
        double vt;              // Thermal voltage (V)
        double n;               // Ideality factor
        double vf;              // Forward voltage (V)
        double max_current;     // Maximum forward current (A)
        double wavelength;      // Wavelength (nm) for color
        double current;         // Actual current (calculated)
        bool ideal;             // Ideal mode (fixed Vf drop)
    } led;
    // BJT transistor (NPN/PNP) - Gummel-Poon model parameters
    struct {
        // Basic DC parameters
        double bf;         // BF - Forward current gain (beta), default: 100
        double is;         // IS - Saturation current (A), default: 1e-14
        double vaf;        // VAF - Forward Early voltage (V), default: 100
        double nf;         // NF - Forward emission coefficient, default: 1.0

        // Reverse parameters
        double br;         // BR - Reverse current gain, default: 1.0
        double var;        // VAR - Reverse Early voltage (V), default: 100
        double nr;         // NR - Reverse emission coefficient, default: 1.0

        // Leakage currents
        double ise;        // ISE - B-E leakage saturation current (A), default: 0
        double isc;        // ISC - B-C leakage saturation current (A), default: 0

        // Temperature
        double temp;       // Operating temperature (K), default: 300

        // Mode
        bool ideal;        // Use ideal (simplified) model, default: true
    } bjt;

    // MOSFET transistor (NMOS/PMOS) - Level 1 SPICE model parameters
    struct {
        // Basic parameters
        double vth;        // VTO - Threshold voltage (V), NMOS: 0.7, PMOS: -0.7
        double kp;         // KP - Transconductance parameter (A/V²), default: 110e-6
        double lambda;     // LAMBDA - Channel length modulation (1/V), default: 0.04

        // Physical dimensions
        double w;          // W - Channel width (m), default: 10e-6
        double l;          // L - Channel length (m), default: 1e-6
        double tox;        // TOX - Gate oxide thickness (m), default: 10e-9

        // Body effect parameters
        double gamma;      // GAMMA - Body effect coefficient (V^0.5), default: 0.4
        double phi;        // PHI - Surface potential (V), default: 0.65
        double nsub;       // NSUB - Substrate doping (1/cm³), default: 1e15

        // Temperature
        double temp;       // Operating temperature (K), default: 300

        // Mode
        bool ideal;        // Use ideal (simplified) model, default: true
    } mosfet;
    // Op-Amp
    struct {
        double gain;            // Open-loop DC gain, default: 100000 (100dB)
        double voffset;         // Input offset voltage (V), default: 0
        double vmax;            // Positive rail voltage (V), default: 15
        double vmin;            // Negative rail voltage (V), default: -15
        double gbw;             // Gain-bandwidth product (Hz), default: 1e6
        double slew_rate;       // Slew rate (V/us), default: 0.5
        double r_in;            // Input impedance (Ohm), default: 1e12
        double r_out;           // Output impedance (Ohm), default: 75
        double i_bias;          // Input bias current (A), default: 1e-12
        double cmrr;            // Common-mode rejection ratio (dB), default: 90
        bool rail_to_rail;      // Rail-to-rail output capability
        bool ideal;             // Ideal mode (infinite gain, bandwidth, etc.)
    } opamp;
    // Waveform generators
    struct {
        double amplitude;       // Peak amplitude (V)
        double frequency;       // Frequency (Hz)
        double phase;           // Phase (degrees)
        double offset;          // DC offset (V)
        double duty;            // Duty cycle (0-1)
        double rise_time;       // Rise time (s), default: 1e-9
        double fall_time;       // Fall time (s), default: 1e-9
        double r_series;        // Output resistance (Ohm)
        bool ideal;             // Ideal mode (zero rise/fall, zero output R)
        SweepConfig amplitude_sweep;   // Amplitude sweep
        SweepConfig frequency_sweep;   // Frequency sweep
    } square_wave;

    struct {
        double amplitude;       // Peak amplitude (V)
        double frequency;       // Frequency (Hz)
        double phase;           // Phase (degrees)
        double offset;          // DC offset (V)
        double r_series;        // Output resistance (Ohm)
        bool ideal;             // Ideal mode
        SweepConfig amplitude_sweep;   // Amplitude sweep
        SweepConfig frequency_sweep;   // Frequency sweep
    } triangle_wave;

    struct {
        double amplitude;       // Peak amplitude (V)
        double frequency;       // Frequency (Hz)
        double phase;           // Phase (degrees)
        double offset;          // DC offset (V)
        double r_series;        // Output resistance (Ohm)
        bool ideal;             // Ideal mode
        SweepConfig amplitude_sweep;   // Amplitude sweep
        SweepConfig frequency_sweep;   // Frequency sweep
    } sawtooth_wave;

    struct {
        double amplitude;       // RMS amplitude (V)
        double seed;            // Random seed
        double bandwidth;       // Noise bandwidth (Hz), default: 1e6
        double r_series;        // Output resistance (Ohm)
        bool ideal;             // Ideal mode
        SweepConfig amplitude_sweep;   // Amplitude sweep
    } noise_source;
} ComponentProps;

// Component structure
typedef struct {
    int id;
    ComponentType type;
    float x, y;
    int rotation;       // 0, 90, 180, 270
    bool selected;
    bool highlighted;
    char label[MAX_LABEL_LEN];

    // Terminals and connections
    int num_terminals;
    int node_ids[MAX_TERMINALS];  // Connected node IDs

    // For voltage sources/inductors - index of current variable
    int voltage_var_idx;
    bool needs_voltage_var;

    // Properties
    ComponentProps props;
} Component;

// Component type info
typedef struct {
    const char *name;
    const char *short_name;
    int num_terminals;
    TerminalDef terminals[MAX_TERMINALS];
    float width;
    float height;
    ComponentProps default_props;
} ComponentTypeInfo;

// Get info for component type
const ComponentTypeInfo *component_get_info(ComponentType type);

// Create a new component
Component *component_create(ComponentType type, float x, float y);

// Free component
void component_free(Component *comp);

// Clone component
Component *component_clone(Component *comp);

// Rotate component
void component_rotate(Component *comp);

// Get terminal world positions
void component_get_terminal_pos(Component *comp, int terminal_idx, float *x, float *y);

// Check if point is inside component
bool component_contains_point(Component *comp, float px, float py);

// Check if point is near a terminal
int component_get_terminal_at(Component *comp, float px, float py, float threshold);

// Stamp component into MNA matrix
void component_stamp(Component *comp, Matrix *A, Vector *b,
                     int *node_map, int num_nodes,
                     double time, Vector *prev_solution, double dt);

// Get display value string
void component_get_value_string(Component *comp, char *buf, size_t buf_size);

// Format engineering notation
void format_engineering(double value, const char *unit, char *buf, size_t buf_size);

// Calculate current sweep value based on time
// Returns the base_value if sweep is disabled, otherwise the swept value
double sweep_get_value(const SweepConfig *sweep, double base_value, double time);

#endif // COMPONENT_H
