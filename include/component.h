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
    struct { double voltage; } dc_voltage;
    struct { double amplitude; double frequency; double phase; double offset; } ac_voltage;
    struct { double current; } dc_current;
    struct { double resistance; double tolerance; double power_rating; double power_dissipated; } resistor;
    struct { double capacitance; double voltage; } capacitor;  // voltage = state
    struct { double capacitance; double voltage; double max_voltage; } capacitor_elec;  // Electrolytic
    struct { double inductance; double current; } inductor;    // current = state
    struct { double is; double vt; double n; } diode;
    struct { double is; double vt; double n; double vz; } zener;  // vz = breakdown voltage
    struct { double is; double vt; double n; } schottky;       // Lower Vf than standard diode
    struct { double is; double vt; double n; double vf; double max_current; double wavelength; double current; } led;  // vf=forward voltage, wavelength in nm for color
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
    struct { double gain; double voffset; double vmax; double vmin; } opamp;
    // Waveform generators
    struct { double amplitude; double frequency; double phase; double offset; double duty; } square_wave;
    struct { double amplitude; double frequency; double phase; double offset; } triangle_wave;
    struct { double amplitude; double frequency; double phase; double offset; } sawtooth_wave;
    struct { double amplitude; double seed; } noise_source;
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

#endif // COMPONENT_H
