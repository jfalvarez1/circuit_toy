/**
 * Circuit Playground - UI System
 */

#ifndef UI_H
#define UI_H

#include <SDL2/SDL.h>
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

// Property field
typedef struct {
    Rect bounds;
    char label[32];
    char value[64];
    char unit[8];
    bool editing;
    int cursor_pos;
} PropertyField;

// Oscilloscope channel
typedef struct {
    bool enabled;
    Color color;
    int probe_idx;
    double offset;
} ScopeChannel;

// UI state
typedef struct {
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

    // Properties panel
    PropertyField properties[16];
    int num_properties;
    Component *editing_component;

    // Oscilloscope
    Rect scope_rect;
    ScopeChannel scope_channels[2];
    double scope_time_div;
    double scope_volt_div;

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

    // Cursor info
    int cursor_x, cursor_y;
    float world_x, world_y;
} UIState;

// Initialize UI
void ui_init(UIState *ui);

// Update UI state
void ui_update(UIState *ui, Circuit *circuit, Simulation *sim);

// Render UI elements
void ui_render_toolbar(UIState *ui, SDL_Renderer *renderer);
void ui_render_palette(UIState *ui, SDL_Renderer *renderer);
void ui_render_properties(UIState *ui, SDL_Renderer *renderer, Component *selected);
void ui_render_measurements(UIState *ui, SDL_Renderer *renderer, Simulation *sim);
void ui_render_oscilloscope(UIState *ui, SDL_Renderer *renderer, Simulation *sim);
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
#define UI_ACTION_SELECT_TOOL   100  // + tool index
#define UI_ACTION_SELECT_COMP   200  // + component type

// Set status message
void ui_set_status(UIState *ui, const char *msg);

// Update measurements
void ui_update_measurements(UIState *ui, Simulation *sim, Circuit *circuit);

#endif // UI_H
