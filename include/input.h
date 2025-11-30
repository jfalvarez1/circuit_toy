/**
 * Circuit Playground - Input Handling
 */

#ifndef INPUT_H
#define INPUT_H

#include <SDL.h>
#include "types.h"
#include "circuit.h"
#include "render.h"
#include "ui.h"

// Mouse button state
typedef struct {
    bool down;
    int start_x, start_y;
    int current_x, current_y;
} MouseButton;

// Property being edited
typedef enum {
    PROP_NONE = 0,
    PROP_VALUE,         // Main value (resistance, capacitance, voltage, etc.)
    PROP_FREQUENCY,
    PROP_PHASE,
    PROP_OFFSET,
    PROP_DUTY,
    PROP_AMPLITUDE
} PropertyType;

// Input state
typedef struct InputState {
    // Mouse state
    MouseButton left;
    MouseButton middle;
    MouseButton right;
    int mouse_x, mouse_y;
    int wheel_delta;

    // Keyboard modifiers
    bool shift_down;
    bool ctrl_down;
    bool alt_down;

    // Current tool
    ToolType current_tool;
    ComponentType placing_component;
    int placing_rotation;  // 0, 90, 180, 270 - rotation while placing

    // Interaction state
    bool is_panning;
    bool is_dragging;
    Component *dragging_component;

    // Wire drawing
    bool drawing_wire;
    int wire_start_node;
    float wire_preview_x, wire_preview_y;

    // Selection
    Component *selected_component;

    // Probe dragging
    int dragging_probe_idx;         // Index of probe being dragged (-1 = none)

    // Text input for property editing
    bool editing_property;
    PropertyType editing_prop_type;
    char input_buffer[64];
    int input_cursor;
    int input_len;

    // Pending UI action (set by ui_handle_click, processed by app)
    int pending_ui_action;
} InputState;

// Initialize input state
void input_init(InputState *input);

// Process SDL event
// Returns true if event was handled
bool input_handle_event(InputState *input, SDL_Event *event,
                        Circuit *circuit, RenderContext *render,
                        UIState *ui);

// Handle keyboard shortcut
void input_handle_key(InputState *input, SDL_Keycode key,
                      Circuit *circuit, RenderContext *render);

// Update cursor for current state
void input_update_cursor(InputState *input);

// Set current tool
void input_set_tool(InputState *input, ToolType tool);

// Start placing a component
void input_start_placing(InputState *input, ComponentType type);

// Cancel current action
void input_cancel_action(InputState *input);

// Delete selected component
void input_delete_selected(InputState *input, Circuit *circuit);

// Copy/cut/paste operations
void input_copy(InputState *input, Circuit *circuit);
void input_cut(InputState *input, Circuit *circuit);
void input_paste(InputState *input, Circuit *circuit, RenderContext *render);
void input_duplicate(InputState *input, Circuit *circuit);

// Property text editing
void input_start_property_edit(InputState *input, PropertyType prop, const char *initial_value);
void input_cancel_property_edit(InputState *input);
bool input_apply_property_edit(InputState *input, Component *comp);
void input_handle_text_input(InputState *input, const char *text);
void input_handle_text_key(InputState *input, SDL_Keycode key);

// Parse value with engineering notation (supports k, M, G, m, u, n, p suffixes)
double parse_engineering_value(const char *str);

#endif // INPUT_H
