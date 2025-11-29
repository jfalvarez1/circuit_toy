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

// Input state
typedef struct {
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

#endif // INPUT_H
