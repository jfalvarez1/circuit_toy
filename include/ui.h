/**
 * Circuit Playground - UI System
 */

#ifndef UI_H
#define UI_H

#include <SDL.h>
#include "types.h"
#include "circuit.h"
#include "simulation.h"
#include "render.h"

// Button state
typedef struct {
    Rect bounds;
    const char *label;
    const char *tooltip;
    bool hovered;
    bool pressed;
    bool enabled;
    bool toggled;
} Button;

// Palette item
typedef struct {
    Rect bounds;
    ComponentType comp_type;
    ToolType tool_type;
    bool is_tool;
    const char *label;
    bool hovered;
    bool selected;
} PaletteItem;

// Circuit template palette item
typedef struct {
    Rect bounds;
    int circuit_type;  // CircuitTemplateType
    const char *label;
    bool hovered;
    bool selected;
} CircuitPaletteItem;

// Property field
typedef struct {
    Rect bounds;
    char label[32];
    char value[64];
    char unit[8];
    bool editing;
    int cursor_pos;
    int prop_type;  // PropertyType enum value (PROP_VALUE, PROP_FREQUENCY, etc.)
} PropertyField;

// Oscilloscope channel
typedef struct {
    bool enabled;
    Color color;
    int probe_idx;
    double offset;      // vertical offset in volts
} ScopeChannel;

// Predefined probe colors for oscilloscope channels
static const Color PROBE_COLORS[MAX_PROBES] = {
    {0xff, 0xff, 0x00, 0xff},  // Yellow (CH1)
    {0x00, 0xff, 0xff, 0xff},  // Cyan (CH2)
    {0xff, 0x00, 0xff, 0xff},  // Magenta (CH3)
    {0x00, 0xff, 0x00, 0xff},  // Green (CH4)
    {0xff, 0x80, 0x00, 0xff},  // Orange (CH5)
    {0x80, 0x80, 0xff, 0xff},  // Light Blue (CH6)
    {0xff, 0x80, 0x80, 0xff},  // Pink (CH7)
    {0x80, 0xff, 0x80, 0xff},  // Light Green (CH8)
};

// UI state
typedef struct {
    // Current window dimensions (updated on resize)
    int window_width;
    int window_height;

    // Toolbar buttons
    Button btn_run;
    Button btn_pause;
    Button btn_step;
    Button btn_reset;
    Button btn_clear;
    Button btn_save;
    Button btn_load;

    // Speed slider
    Rect speed_slider;
    float speed_value;
    bool dragging_speed;

    // Component palette
    PaletteItem palette_items[32];
    int num_palette_items;
    int selected_palette_idx;

    // Circuit template palette
    CircuitPaletteItem circuit_items[16];
    int num_circuit_items;
    int selected_circuit_type;  // Currently selected circuit template (-1 = none)
    bool placing_circuit;       // True when placing a circuit template

    // Properties panel
    PropertyField properties[16];
    int num_properties;
    Component *editing_component;

    // Oscilloscope settings
    Rect scope_rect;
    ScopeChannel scope_channels[MAX_PROBES];
    int scope_num_channels;         // Number of active channels (from probes)
    double scope_time_div;          // Time per division (seconds)
    double scope_volt_div;          // Volts per division
    int scope_selected_channel;     // Currently selected channel for adjustment
    bool scope_paused;              // Freeze oscilloscope display

    // Scope resizing
    bool scope_resizing;            // Currently resizing scope panel
    int scope_resize_edge;          // Which edge is being dragged (0=top, 1=left)

    // Properties panel resizing
    int properties_width;           // Current width of properties panel
    bool props_resizing;            // Currently resizing properties panel

    // Oscilloscope control buttons
    Button btn_scope_volt_up;
    Button btn_scope_volt_down;
    Button btn_scope_time_up;
    Button btn_scope_time_down;
    Button btn_scope_trig_mode;      // Cycle through trigger modes
    Button btn_scope_trig_edge;      // Toggle trigger edge
    Button btn_scope_trig_ch;        // Cycle through trigger channel
    Button btn_scope_trig_up;        // Increase trigger level
    Button btn_scope_trig_down;      // Decrease trigger level
    Button btn_scope_mode;           // Toggle Y-T / X-Y mode
    Button btn_scope_screenshot;     // Capture scope display
    Button btn_scope_cursor;         // Toggle measurement cursors
    Button btn_scope_fft;            // Toggle FFT view

    // Cursor state
    bool scope_cursor_mode;          // Cursor mode active
    int scope_cursor_drag;           // Which cursor is being dragged (0=none, 1=cursor1, 2=cursor2)
    double cursor1_time;             // Cursor 1 time position (0-1 normalized)
    double cursor2_time;             // Cursor 2 time position (0-1 normalized)

    // FFT display state
    bool scope_fft_mode;             // FFT display active

    // Trigger settings
    TriggerMode trigger_mode;        // Auto, Normal, Single
    TriggerEdge trigger_edge;        // Rising, Falling, Both
    int trigger_channel;             // Channel used for trigger (0-based)
    double trigger_level;            // Trigger voltage level
    bool trigger_armed;              // Single-shot mode armed
    bool triggered;                  // Has triggered (for single-shot)
    double trigger_holdoff;          // Time to wait after trigger before re-arming

    // Display mode
    ScopeDisplayMode display_mode;   // Y-T or X-Y
    int xy_channel_x;                // X channel for X-Y mode (0-based)
    int xy_channel_y;                // Y channel for X-Y mode (0-based)

    // Measurements display
    double voltmeter_value;
    double ammeter_value;

    // Status bar
    char status_message[256];
    double sim_time;
    int node_count;
    int component_count;

    // Modal dialogs
    bool show_shortcuts_dialog;

    // Bode plot / frequency response
    bool show_bode_plot;            // Show Bode plot panel
    Button btn_bode;                // Button to run/toggle Bode plot
    Rect bode_rect;                 // Bode plot panel bounds
    double bode_freq_start;         // Start frequency (Hz)
    double bode_freq_stop;          // Stop frequency (Hz)
    int bode_num_points;            // Number of frequency points

    // Parametric sweep panel
    bool show_sweep_panel;          // Show sweep panel
    int sweep_component_idx;        // Selected component for sweep
    int sweep_param_type;           // Parameter to sweep (0=value, 1=freq, etc.)
    double sweep_start;             // Start value
    double sweep_end;               // End value
    int sweep_num_points;           // Number of sweep points
    bool sweep_log_scale;           // Use logarithmic scale

    // Monte Carlo panel
    bool show_monte_carlo_panel;    // Show Monte Carlo panel
    int monte_carlo_runs;           // Number of Monte Carlo runs
    double monte_carlo_tolerance;   // Tolerance percentage

    // Cursor info
    int cursor_x, cursor_y;
    float world_x, world_y;
} UIState;

// Initialize UI
void ui_init(UIState *ui);

// Update UI state
void ui_update(UIState *ui, Circuit *circuit, Simulation *sim);

// Forward declaration for InputState (defined in input.h)
struct InputState;

// Forward declaration for AnalysisState (defined in analysis.h)
typedef struct AnalysisState AnalysisState_fwd;

// Render UI elements
void ui_render_toolbar(UIState *ui, SDL_Renderer *renderer);
void ui_render_palette(UIState *ui, SDL_Renderer *renderer);
void ui_render_properties(UIState *ui, SDL_Renderer *renderer, Component *selected, struct InputState *input);
void ui_render_measurements(UIState *ui, SDL_Renderer *renderer, Simulation *sim);
void ui_render_oscilloscope(UIState *ui, SDL_Renderer *renderer, Simulation *sim, void *analysis);
void ui_render_bode_plot(UIState *ui, SDL_Renderer *renderer, Simulation *sim);
void ui_render_sweep_panel(UIState *ui, SDL_Renderer *renderer, void *analysis);
void ui_render_monte_carlo_panel(UIState *ui, SDL_Renderer *renderer, void *analysis);
void ui_render_statusbar(UIState *ui, SDL_Renderer *renderer);
void ui_render_shortcuts_dialog(UIState *ui, SDL_Renderer *renderer);

// Handle UI events
// Returns: -1 = not handled, 0+ = action ID
int ui_handle_click(UIState *ui, int x, int y, bool is_down);
int ui_handle_motion(UIState *ui, int x, int y);

// UI action IDs
#define UI_ACTION_NONE          -1
#define UI_ACTION_RUN           1
#define UI_ACTION_PAUSE         2
#define UI_ACTION_STEP          3
#define UI_ACTION_RESET         4
#define UI_ACTION_CLEAR         5
#define UI_ACTION_SAVE          6
#define UI_ACTION_LOAD          7
#define UI_ACTION_SCOPE_VOLT_UP    10
#define UI_ACTION_SCOPE_VOLT_DOWN  11
#define UI_ACTION_SCOPE_TIME_UP    12
#define UI_ACTION_SCOPE_TIME_DOWN  13
#define UI_ACTION_SCOPE_PAUSE      14
#define UI_ACTION_SCOPE_TRIG_MODE  15
#define UI_ACTION_SCOPE_TRIG_EDGE  16
#define UI_ACTION_SCOPE_TRIG_CH    17
#define UI_ACTION_SCOPE_MODE       18
#define UI_ACTION_SCOPE_TRIG_UP    19
#define UI_ACTION_SCOPE_TRIG_DOWN  20
#define UI_ACTION_SCOPE_SCREENSHOT 21
#define UI_ACTION_BODE_PLOT     22
#define UI_ACTION_CURSOR_TOGGLE 23   // Toggle cursor mode
#define UI_ACTION_FFT_TOGGLE    24   // Toggle FFT view
#define UI_ACTION_SWEEP_PANEL   25   // Toggle parametric sweep panel
#define UI_ACTION_MONTE_CARLO   26   // Toggle Monte Carlo panel
#define UI_ACTION_SELECT_TOOL   100  // + tool index
#define UI_ACTION_SELECT_COMP   200  // + component type
#define UI_ACTION_SELECT_CIRCUIT 300 // + circuit template type
#define UI_ACTION_PROP_APPLY    1000 // Apply property text edit
#define UI_ACTION_PROP_EDIT     1100 // + property type (PROP_VALUE, PROP_FREQUENCY, etc.)

// Set status message
void ui_set_status(UIState *ui, const char *msg);

// Update measurements
void ui_update_measurements(UIState *ui, Simulation *sim, Circuit *circuit);

// Update oscilloscope channels from circuit probes
void ui_update_scope_channels(UIState *ui, Circuit *circuit);

// Update UI layout after window resize
void ui_update_layout(UIState *ui);

#endif // UI_H
