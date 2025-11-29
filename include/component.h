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
    struct { double resistance; } resistor;
    struct { double capacitance; double voltage; } capacitor;  // voltage = state
    struct { double inductance; double current; } inductor;    // current = state
    struct { double is; double vt; double n; } diode;
    struct { double beta; double is; double va; } bjt;
    struct { double kn; double vth; double lambda; } mosfet;
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
