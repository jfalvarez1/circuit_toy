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

// ============================================
// SYNTHWAVE COLOR THEME
// ============================================
// Background colors
#define SYNTH_BG_DARK       0x0d, 0x02, 0x21  // Deep purple-black
#define SYNTH_BG_MID        0x1a, 0x0a, 0x2e  // Dark purple
#define SYNTH_BG_LIGHT      0x2d, 0x13, 0x4a  // Purple

// Accent colors
#define SYNTH_PINK          0xff, 0x29, 0x75  // Hot pink
#define SYNTH_PINK_DIM      0xc0, 0x20, 0x58  // Dimmed pink
#define SYNTH_CYAN          0x00, 0xff, 0xff  // Neon cyan
#define SYNTH_CYAN_DIM      0x00, 0xb0, 0xb0  // Dimmed cyan
#define SYNTH_PURPLE        0xbd, 0x00, 0xff  // Bright purple
#define SYNTH_PURPLE_DIM    0x8a, 0x00, 0xb8  // Dimmed purple
#define SYNTH_YELLOW        0xff, 0xf0, 0x00  // Neon yellow
#define SYNTH_ORANGE        0xff, 0x61, 0x00  // Neon orange
#define SYNTH_ORANGE_DIM    0xc0, 0x48, 0x00  // Dimmed orange
#define SYNTH_GREEN         0x00, 0xff, 0x9f  // Neon green

// Text colors
#define SYNTH_TEXT          0xff, 0xff, 0xff  // White
#define SYNTH_TEXT_DIM      0xc0, 0xb0, 0xd0  // Light purple-gray
#define SYNTH_TEXT_DARK     0x80, 0x70, 0x90  // Dark purple-gray

// Border colors
#define SYNTH_BORDER        0x4a, 0x1a, 0x6a  // Purple border
#define SYNTH_BORDER_LIGHT  0x7a, 0x2a, 0x9a  // Light purple border

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

// Palette category IDs
typedef enum {
    PCAT_TOOLS = 0,
    PCAT_SOURCES,
    PCAT_WAVEFORMS,
    PCAT_PASSIVES,
    PCAT_DIODES,
    PCAT_BJT,
    PCAT_FET,
    PCAT_THYRISTORS,
    PCAT_OPAMPS,
    PCAT_CONTROLLED,
    PCAT_SWITCHES,
    PCAT_TRANSFORMERS,
    PCAT_LOGIC,
    PCAT_DIGITAL,
    PCAT_MIXED,
    PCAT_REGULATORS,
    PCAT_DISPLAY,
    PCAT_MEASUREMENT,
    PCAT_CIRCUITS,
    PCAT_SUBCIRCUITS,   // User-defined subcircuits (Ctrl+G)
    PCAT_COUNT
} PaletteCategoryID;

// Palette category (collapsible)
typedef struct {
    const char *name;
    bool collapsed;
    int header_y;       // Y position of header (for click detection)
} PaletteCategory;

// Palette item
typedef struct {
    Rect bounds;
    ComponentType comp_type;
    ToolType tool_type;
    bool is_tool;
    const char *label;
    bool hovered;
    bool selected;
    PaletteCategoryID category;  // Which category this item belongs to
} PaletteItem;

// Circuit template palette item
typedef struct {
    Rect bounds;
    int circuit_type;  // CircuitTemplateType
    const char *label;
    bool hovered;
    bool selected;
} CircuitPaletteItem;

// User subcircuit palette item
typedef struct {
    Rect bounds;
    int def_id;         // ID in g_subcircuit_library
    char label[32];     // Subcircuit name
    int num_pins;       // Number of pins
    bool hovered;
    bool selected;
} SubcircuitPaletteItem;

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
    Button btn_export_svg;

    // Speed slider
    Rect speed_slider;
    float speed_value;
    bool dragging_speed;

    // Time step controls
    Button btn_timestep_up;
    Button btn_timestep_down;
    Button btn_timestep_auto;
    int timestep_display_x;     // X position for time step display
    double display_time_step;   // Current time step for display (updated from simulation)

    // Component palette
    PaletteItem palette_items[128];  // Increased for many new components
    int num_palette_items;
    int selected_palette_idx;

    // Palette categories (collapsible)
    PaletteCategory categories[PCAT_COUNT];

    // Palette scrolling
    int palette_scroll_offset;      // Current scroll offset (pixels from top)
    int palette_content_height;     // Total height of palette content
    int palette_visible_height;     // Visible height of palette area
    bool palette_scrolling;         // Currently dragging scrollbar
    int palette_scroll_drag_start_y;     // Mouse Y when drag started
    int palette_scroll_drag_start_offset; // Scroll offset when drag started

    // Circuit template palette
    CircuitPaletteItem circuit_items[80];  // Must be >= CIRCUIT_TYPE_COUNT
    int num_circuit_items;
    int selected_circuit_type;  // Currently selected circuit template (-1 = none)
    bool placing_circuit;       // True when placing a circuit template

    // User subcircuit palette (from g_subcircuit_library)
    SubcircuitPaletteItem subcircuit_items[MAX_SUBCIRCUIT_DEFS];
    int num_subcircuit_items;
    int selected_subcircuit_def_id;  // Selected subcircuit definition ID (-1 = none)
    bool placing_subcircuit;         // True when placing a user subcircuit

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

    // Scope controls scrolling (for buttons/measurements when window is small)
    int scope_controls_scroll;          // Current scroll offset
    int scope_controls_content_height;  // Total height of scope controls content
    int scope_controls_visible_height;  // Visible height of scope controls area
    bool scope_controls_scrolling;      // Currently dragging scrollbar

    // Properties panel resizing and scrolling
    int properties_width;           // Current width of properties panel
    bool props_resizing;            // Currently resizing properties panel
    int properties_content_height;  // Height of properties content (for dynamic sizing)
    int properties_scroll_offset;   // Current scroll offset for properties panel
    int properties_visible_height;  // Visible height of properties area
    bool properties_scrolling;      // Currently dragging properties scrollbar

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
    Button btn_scope_autoset;        // Auto-configure scope settings
    Button btn_scope_popup;          // Pop out oscilloscope to separate window

    // Pop-out oscilloscope window
    SDL_Window *scope_popup_window;      // Separate window for oscilloscope
    SDL_Renderer *scope_popup_renderer;  // Renderer for popup window
    Uint32 scope_popup_window_id;        // Window ID for event handling
    bool scope_popped_out;               // Whether scope is popped out

    // Cursor state
    bool scope_cursor_mode;          // Cursor mode active
    int scope_cursor_drag;           // Which cursor is being dragged (0=none, 1=time1, 2=time2, 3=volt1, 4=volt2, 5=trigger)
    double cursor1_time;             // Cursor 1 time position (0-1 normalized)
    double cursor2_time;             // Cursor 2 time position (0-1 normalized)
    double cursor1_volt;             // Cursor 1 voltage position (0-1 normalized, 0.5 = center)
    double cursor2_volt;             // Cursor 2 voltage position (0-1 normalized, 0.5 = center)

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
    bool dragging_trigger_level;     // Currently dragging trigger level indicator
    double trigger_position;         // Horizontal trigger position (0.0=left, 1.0=right, 0.5=center)
    bool dragging_trigger_position;  // Currently dragging trigger position indicator

    // Triggered capture state (for stable display)
    // 1000 samples is enough - we subsample history when needed
    #define SCOPE_CAPTURE_SIZE 1000
    double scope_capture_times[SCOPE_CAPTURE_SIZE];     // Time values of captured data
    double scope_capture_values[MAX_PROBES][SCOPE_CAPTURE_SIZE];  // Voltage values per channel
    int scope_capture_count;                     // Number of captured samples
    double scope_capture_time;                   // Simulation time when captured
    bool scope_capture_valid;                    // Whether we have valid captured data
    double scope_last_trigger_time;              // Time of last trigger for holdoff
    int scope_trigger_sample_idx;                // Index of trigger point in capture buffer

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

    // Adaptive time-stepping status (for UI display)
    bool adaptive_enabled;
    double adaptive_factor;      // Current dt multiplier (1.0 = target)
    int step_rejections;         // Rejections this frame
    double error_estimate;       // Estimated error (0-1)

    // Modal dialogs
    bool show_shortcuts_dialog;

    // Bode plot / frequency response
    bool show_bode_plot;            // Show Bode plot panel
    Button btn_bode;                // Button to run/toggle Bode plot
    Button btn_bode_recalc;         // Button to recalculate Bode plot
    Button btn_mc;                  // Button to toggle Monte Carlo panel
    Rect bode_rect;                 // Bode plot panel bounds
    double bode_freq_start;         // Start frequency (Hz)
    double bode_freq_stop;          // Stop frequency (Hz)
    int bode_num_points;            // Number of frequency points
    bool bode_resizing;             // Currently resizing Bode plot
    int bode_resize_edge;           // Which edge is being dragged (0=top, 1=left, 2=bottom, 3=right)
    bool bode_dragging;             // Dragging the Bode plot window
    int bode_drag_start_x;          // Mouse X when drag started
    int bode_drag_start_y;          // Mouse Y when drag started
    int bode_rect_start_x;          // Rect X when drag started
    int bode_rect_start_y;          // Rect Y when drag started

    // Bode plot cursor
    bool bode_cursor_active;        // Cursor mode active for Bode plot
    double bode_cursor_freq;        // Cursor frequency (Hz)
    bool bode_cursor_dragging;      // Currently dragging the cursor
    double bode_cursor_magnitude;   // Magnitude at cursor position (dB)
    double bode_cursor_phase;       // Phase at cursor position (degrees)

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

    // Component spotlight/search (Ctrl+K)
    bool show_spotlight;                    // Show spotlight dialog
    char spotlight_query[64];               // Search query text
    int spotlight_cursor;                   // Cursor position in query
    ComponentType spotlight_results[32];    // Matching component types
    int spotlight_num_results;              // Number of matching results
    int spotlight_selected;                 // Currently highlighted result index

    // Environment sliders (for LDR and Thermistor)
    Rect env_light_slider;          // Light level slider bounds
    Rect env_temp_slider;           // Temperature slider bounds
    bool dragging_light;            // Currently dragging light slider
    bool dragging_temp;             // Currently dragging temperature slider

    // Cursor info
    int cursor_x, cursor_y;
    float world_x, world_y;

    // Node hover tooltip
    int hovered_node_id;            // ID of node currently being hovered (-1 if none)
    double hovered_node_voltage;    // Voltage at hovered node
    bool show_node_tooltip;         // Whether to show the tooltip

    // Component hover tooltip
    int hovered_comp_id;            // ID of component currently being hovered (-1 if none)
    double hovered_comp_voltage;    // Voltage drop across component (V+ - V-)
    double hovered_comp_current;    // Current through component (A)
    bool show_comp_tooltip;         // Whether to show the component tooltip

    // Subcircuit editor dialog (Ctrl+G to create from selection)
    bool show_subcircuit_dialog;            // Show the create subcircuit dialog
    char subcircuit_name[32];               // Name for the new subcircuit
    int subcircuit_name_cursor;             // Cursor position in name field
    int subcircuit_num_pins;                // Number of pins defined
    char subcircuit_pin_names[16][16];      // Pin names (max 16 pins)
    int subcircuit_selected_pin;            // Currently selected pin for editing
    int subcircuit_editing_field;           // 0=name, 1+=pin names
    int subcircuit_editing_def_id;          // -1 = creating new, >=0 = editing existing def
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
void ui_render_spotlight(UIState *ui, SDL_Renderer *renderer);
void ui_render_subcircuit_dialog(UIState *ui, SDL_Renderer *renderer);
void ui_render_neon_trim(UIState *ui, SDL_Renderer *renderer);

// Subcircuit editor functions
void ui_subcircuit_dialog_open(UIState *ui, int num_selected, int detected_pins, char detected_names[][16]);
void ui_subcircuit_dialog_open_edit(UIState *ui, int def_id);  // Open dialog to edit existing subcircuit
void ui_subcircuit_dialog_close(UIState *ui);
void ui_subcircuit_dialog_text_input(UIState *ui, const char *text);
bool ui_subcircuit_dialog_key(UIState *ui, SDL_Keycode key);
bool ui_subcircuit_dialog_click(UIState *ui, int mouse_x, int mouse_y);

// Spotlight search functions
void ui_spotlight_open(UIState *ui);
void ui_spotlight_close(UIState *ui);
void ui_spotlight_text_input(UIState *ui, const char *text);
ComponentType ui_spotlight_key(UIState *ui, SDL_Keycode key);
ComponentType ui_spotlight_click(UIState *ui, int mouse_x, int mouse_y);

// Handle UI events
// Returns: -1 = not handled, 0+ = action ID
int ui_handle_click(UIState *ui, int x, int y, bool is_down);
int ui_handle_right_click(UIState *ui, int x, int y);  // Handle right-click on palette items
int ui_handle_motion(UIState *ui, int x, int y, bool popup_mode);

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
#define UI_ACTION_SCOPE_AUTOSET    27
#define UI_ACTION_BODE_PLOT     22
#define UI_ACTION_BODE_RECALC   28   // Recalculate Bode plot with current settings
#define UI_ACTION_CURSOR_TOGGLE 23   // Toggle cursor mode
#define UI_ACTION_FFT_TOGGLE    24   // Toggle FFT view
#define UI_ACTION_SWEEP_PANEL   25   // Toggle parametric sweep panel
#define UI_ACTION_MONTE_CARLO   26   // Toggle Monte Carlo panel
#define UI_ACTION_TIMESTEP_UP   29   // Increase time step
#define UI_ACTION_TIMESTEP_DOWN 30   // Decrease time step
#define UI_ACTION_TIMESTEP_AUTO 31   // Auto-adjust time step
#define UI_ACTION_SCOPE_POPUP   32   // Pop out oscilloscope to separate window
#define UI_ACTION_SPOTLIGHT     33   // Open component spotlight search (Ctrl+K)
#define UI_ACTION_EXPORT_SVG    34   // Export circuit to SVG file
#define UI_ACTION_MC_RUN        35   // Start Monte Carlo analysis
#define UI_ACTION_MC_RUNS_UP    36   // Increase MC runs
#define UI_ACTION_MC_RUNS_DOWN  37   // Decrease MC runs
#define UI_ACTION_MC_TOL_UP     38   // Increase MC tolerance
#define UI_ACTION_MC_TOL_DOWN   39   // Decrease MC tolerance
#define UI_ACTION_MC_RESET      40   // Reset MC results
#define UI_ACTION_CREATE_SUBCIRCUIT 41   // Create subcircuit from selection (Ctrl+G)
#define UI_ACTION_EDIT_SUBCIRCUIT   42   // Edit existing subcircuit (right-click in palette)
#define UI_ACTION_SELECT_TOOL   100  // + tool index
#define UI_ACTION_SELECT_COMP   200  // + component type (supports up to 300 component types)
#define UI_ACTION_SELECT_CIRCUIT 500 // + circuit template type
#define UI_ACTION_SELECT_SUBCIRCUIT 600 // + subcircuit definition id
#define UI_ACTION_PROP_APPLY    1000 // Apply property text edit
#define UI_ACTION_PROP_EDIT     1100 // + property type (PROP_VALUE, PROP_FREQUENCY, etc.)

// Set status message
void ui_set_status(UIState *ui, const char *msg);

// Update measurements
void ui_update_measurements(UIState *ui, Simulation *sim, Circuit *circuit);

// Update oscilloscope channels from circuit probes
void ui_update_scope_channels(UIState *ui, Circuit *circuit);

// Oscilloscope autoset - automatically configure scope based on signal
void ui_scope_autoset(UIState *ui, Simulation *sim);

// Update UI layout after window resize
void ui_update_layout(UIState *ui);

// Handle palette scroll (mouse wheel)
void ui_palette_scroll(UIState *ui, int delta);

// Handle properties scroll (mouse wheel)
void ui_properties_scroll(UIState *ui, int delta);

// Check if point is in palette area
bool ui_point_in_palette(UIState *ui, int x, int y);

// Check if point is in properties area
bool ui_point_in_properties(UIState *ui, int x, int y);

// Check if point is in scope controls area
bool ui_point_in_scope_controls(UIState *ui, int x, int y);

// Scroll scope controls area
void ui_scope_controls_scroll(UIState *ui, int direction);

// Popup scope coordinate handling for input events
// Stores saved coordinates for scope rect and buttons
typedef struct {
    Rect scope_rect;
    Rect btn_volt_up, btn_volt_down, btn_time_up, btn_time_down;
    Rect btn_autoset, btn_trig_mode, btn_trig_edge, btn_trig_ch;
    Rect btn_trig_up, btn_trig_down, btn_mode, btn_cursor;
    Rect btn_fft, btn_screenshot, btn_bode, btn_mc;
} ScopeCoordsBackup;

// Setup popup scope coordinates for input handling
// Returns backup of original coordinates
ScopeCoordsBackup ui_setup_popup_scope_coords(UIState *ui);

// Restore original scope coordinates from backup
void ui_restore_popup_scope_coords(UIState *ui, const ScopeCoordsBackup *backup);

#endif // UI_H
