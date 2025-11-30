/**
 * Circuit Playground - Common Type Definitions
 */

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>

// Define M_PI if not available (MSVC)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Window dimensions
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define TOOLBAR_HEIGHT 50
#define PALETTE_WIDTH 160
#define PROPERTIES_WIDTH 350
#define STATUSBAR_HEIGHT 24

// Canvas area
#define CANVAS_X PALETTE_WIDTH
#define CANVAS_Y TOOLBAR_HEIGHT
#define CANVAS_WIDTH (WINDOW_WIDTH - PALETTE_WIDTH - PROPERTIES_WIDTH)
#define CANVAS_HEIGHT (WINDOW_HEIGHT - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT)

// Grid settings
#define GRID_SIZE 20
#define MAX_ZOOM 4.0f
#define MIN_ZOOM 0.25f

// Limits
#define MAX_COMPONENTS 1024
#define MAX_NODES 2048
#define MAX_WIRES 2048
#define MAX_PROBES 8
#define MAX_LABEL_LEN 32
#define MAX_HISTORY 10000

// Component types
typedef enum {
    COMP_NONE = 0,
    COMP_GROUND,
    COMP_DC_VOLTAGE,
    COMP_AC_VOLTAGE,
    COMP_DC_CURRENT,
    COMP_RESISTOR,
    COMP_CAPACITOR,
    COMP_CAPACITOR_ELEC,    // Electrolytic capacitor (polarized)
    COMP_INDUCTOR,
    COMP_DIODE,
    COMP_ZENER,             // Zener diode
    COMP_SCHOTTKY,          // Schottky diode
    COMP_LED,               // Light-emitting diode
    COMP_NPN_BJT,
    COMP_PNP_BJT,
    COMP_NMOS,
    COMP_PMOS,
    COMP_OPAMP,
    // Waveform generators
    COMP_SQUARE_WAVE,
    COMP_TRIANGLE_WAVE,
    COMP_SAWTOOTH_WAVE,
    COMP_NOISE_SOURCE,
    COMP_TYPE_COUNT
} ComponentType;

// Oscilloscope trigger modes
typedef enum {
    TRIG_AUTO = 0,      // Always triggers, free-running if no signal
    TRIG_NORMAL,        // Only triggers on valid edge
    TRIG_SINGLE         // Single shot - triggers once then stops
} TriggerMode;

// Oscilloscope trigger edge
typedef enum {
    TRIG_EDGE_RISING = 0,
    TRIG_EDGE_FALLING,
    TRIG_EDGE_BOTH
} TriggerEdge;

// Oscilloscope display mode
typedef enum {
    SCOPE_MODE_YT = 0,  // Normal time-domain
    SCOPE_MODE_XY       // X-Y mode (Lissajous)
} ScopeDisplayMode;

// Source sweep modes
typedef enum {
    SWEEP_NONE = 0,     // No sweep - constant value
    SWEEP_LINEAR,       // Linear sweep from start to end
    SWEEP_LOG,          // Logarithmic sweep (for frequency)
    SWEEP_STEP          // Step through discrete values
} SweepMode;

// Sweep configuration for a source parameter
typedef struct {
    bool enabled;           // Sweep is active
    SweepMode mode;         // Type of sweep
    double start_value;     // Starting value
    double end_value;       // Ending value
    double sweep_time;      // Time to complete one sweep (seconds)
    int num_steps;          // For stepped mode: number of discrete steps
    bool repeat;            // Repeat sweep when complete (otherwise hold at end)
    bool bidirectional;     // Sweep back and forth (triangle pattern)
} SweepConfig;

// Tool types
typedef enum {
    TOOL_SELECT = 0,
    TOOL_WIRE,
    TOOL_DELETE,
    TOOL_PROBE,
    TOOL_COMPONENT
} ToolType;

// Simulation state
typedef enum {
    SIM_STOPPED = 0,
    SIM_RUNNING,
    SIM_PAUSED
} SimState;

// 2D Point
typedef struct {
    float x;
    float y;
} Point2D;

// Integer point (for grid)
typedef struct {
    int x;
    int y;
} Point2Di;

// Rectangle
typedef struct {
    int x, y, w, h;
} Rect;

// Color (RGBA)
typedef struct {
    uint8_t r, g, b, a;
} Color;

// Predefined colors
#define COLOR_BG         (Color){0x1a, 0x1a, 0x2e, 0xff}
#define COLOR_BG_DARK    (Color){0x16, 0x21, 0x3e, 0xff}
#define COLOR_ACCENT     (Color){0x00, 0xd9, 0xff, 0xff}
#define COLOR_ACCENT2    (Color){0xe9, 0x45, 0x60, 0xff}
#define COLOR_TEXT       (Color){0xff, 0xff, 0xff, 0xff}
#define COLOR_TEXT_DIM   (Color){0xb0, 0xb0, 0xb0, 0xff}
#define COLOR_GRID       (Color){0x2a, 0x2a, 0x4e, 0xff}
#define COLOR_SUCCESS    (Color){0x00, 0xff, 0x88, 0xff}
#define COLOR_WARNING    (Color){0xff, 0xaa, 0x00, 0xff}
#define COLOR_DANGER     (Color){0xff, 0x44, 0x44, 0xff}
#define COLOR_WIRE       (Color){0x00, 0xd9, 0xff, 0xff}

// Utility macros
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi) (MIN(MAX(x, lo), hi))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Snap to grid (handles negative values correctly using round)
static inline int snap_to_grid(float val) {
    return (int)round(val / GRID_SIZE) * GRID_SIZE;
}

#endif // TYPES_H
