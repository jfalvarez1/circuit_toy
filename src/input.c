/**
 * Circuit Playground - Input Handling Implementation
 */

#include <string.h>
#include <math.h>
#include "input.h"
#include "app.h"

void input_init(InputState *input) {
    memset(input, 0, sizeof(InputState));
    input->current_tool = TOOL_SELECT;
    input->placing_component = COMP_NONE;
}

bool input_handle_event(InputState *input, SDL_Event *event,
                        Circuit *circuit, RenderContext *render,
                        UIState *ui) {
    if (!input || !event) return false;

    switch (event->type) {
        case SDL_MOUSEBUTTONDOWN: {
            int x = event->button.x;
            int y = event->button.y;

            // Check if click is in UI area first
            int action = ui_handle_click(ui, x, y, true);
            if (action != UI_ACTION_NONE) {
                // Handle UI action
                if (action >= UI_ACTION_SELECT_TOOL && action < UI_ACTION_SELECT_COMP) {
                    input_set_tool(input, action - UI_ACTION_SELECT_TOOL);
                } else if (action >= UI_ACTION_SELECT_COMP) {
                    input_start_placing(input, action - UI_ACTION_SELECT_COMP);
                }
                return true;
            }

            // Check if in canvas area
            if (x < CANVAS_X || x >= CANVAS_X + CANVAS_WIDTH ||
                y < CANVAS_Y || y >= CANVAS_Y + CANVAS_HEIGHT) {
                return false;
            }

            // Convert to world coordinates
            float wx, wy;
            render_screen_to_world(render, x - CANVAS_X, y - CANVAS_Y, &wx, &wy);

            if (event->button.button == SDL_BUTTON_LEFT) {
                input->left.down = true;
                input->left.start_x = x;
                input->left.start_y = y;

                switch (input->current_tool) {
                    case TOOL_SELECT: {
                        // Find component at click position
                        Component *comp = circuit_find_component_at(circuit, wx, wy);
                        if (comp) {
                            // Deselect previous
                            if (input->selected_component && input->selected_component != comp) {
                                input->selected_component->selected = false;
                            }
                            comp->selected = true;
                            input->selected_component = comp;
                            input->is_dragging = true;
                            input->dragging_component = comp;
                        } else {
                            // Deselect
                            if (input->selected_component) {
                                input->selected_component->selected = false;
                                input->selected_component = NULL;
                            }
                        }
                        break;
                    }

                    case TOOL_COMPONENT: {
                        if (input->placing_component != COMP_NONE) {
                            float snapped_x = snap_to_grid(wx);
                            float snapped_y = snap_to_grid(wy);

                            // Check for existing nodes near terminals before placing
                            Component *temp = component_create(input->placing_component, snapped_x, snapped_y);
                            if (temp) {
                                // Find existing nodes near each terminal before adding component
                                int existing_nodes[8] = {0};
                                for (int i = 0; i < temp->num_terminals && i < 8; i++) {
                                    float tx, ty;
                                    component_get_terminal_pos(temp, i, &tx, &ty);
                                    Node *existing = circuit_find_node_at(circuit, tx, ty, 10);
                                    existing_nodes[i] = existing ? existing->id : 0;
                                }

                                // Now add the component (this creates new nodes for terminals)
                                circuit_add_component(circuit, temp);

                                // Auto-wire: connect to existing nodes at terminal positions
                                for (int i = 0; i < temp->num_terminals && i < 8; i++) {
                                    if (existing_nodes[i] > 0 && temp->node_ids[i] != existing_nodes[i]) {
                                        // Create wire between new terminal node and existing node
                                        circuit_add_wire(circuit, temp->node_ids[i], existing_nodes[i]);
                                    }
                                }

                                // Push undo action for component placement
                                circuit_push_undo(circuit, UNDO_ADD_COMPONENT, temp->id, NULL, 0, 0);
                                ui_set_status(ui, "Component placed (Ctrl+Z to undo)");
                            }
                        }
                        break;
                    }

                    case TOOL_WIRE: {
                        float snapped_x = snap_to_grid(wx);
                        float snapped_y = snap_to_grid(wy);

                        if (!input->drawing_wire) {
                            // Start wire
                            int node_id = circuit_find_or_create_node(circuit, snapped_x, snapped_y, 10);
                            input->wire_start_node = node_id;
                            input->drawing_wire = true;
                            input->wire_preview_x = snapped_x;
                            input->wire_preview_y = snapped_y;
                        } else {
                            // End wire
                            int end_node_id = circuit_find_or_create_node(circuit, snapped_x, snapped_y, 10);
                            if (end_node_id != input->wire_start_node) {
                                int wire_id = circuit_add_wire(circuit, input->wire_start_node, end_node_id);
                                if (wire_id >= 0) {
                                    // Push undo action for wire placement
                                    UndoAction *action = &circuit->undo_stack[circuit->undo_count++];
                                    action->type = UNDO_ADD_WIRE;
                                    action->id = wire_id;
                                    action->wire_start = input->wire_start_node;
                                    action->wire_end = end_node_id;
                                    action->component_backup = NULL;
                                }
                                ui_set_status(ui, "Wire connected (Ctrl+Z to undo)");
                            }
                            input->drawing_wire = false;
                        }
                        break;
                    }

                    case TOOL_DELETE: {
                        // Delete component
                        Component *comp = circuit_find_component_at(circuit, wx, wy);
                        if (comp) {
                            if (input->selected_component == comp) {
                                input->selected_component = NULL;
                            }
                            circuit_remove_component(circuit, comp->id);
                            ui_set_status(ui, "Component deleted");
                            break;
                        }

                        // Delete wire
                        Wire *wire = circuit_find_wire_at(circuit, wx, wy, 5);
                        if (wire) {
                            circuit_remove_wire(circuit, wire->id);
                            ui_set_status(ui, "Wire deleted");
                        }
                        break;
                    }

                    case TOOL_PROBE: {
                        float snapped_x = snap_to_grid(wx);
                        float snapped_y = snap_to_grid(wy);
                        Node *node = circuit_find_node_at(circuit, snapped_x, snapped_y, 15);
                        if (node) {
                            circuit_add_probe(circuit, node->id, node->x, node->y);
                            ui_set_status(ui, "Probe placed");
                        }
                        break;
                    }
                }
            } else if (event->button.button == SDL_BUTTON_MIDDLE) {
                input->middle.down = true;
                input->middle.start_x = x;
                input->middle.start_y = y;
                input->is_panning = true;
            } else if (event->button.button == SDL_BUTTON_RIGHT) {
                // Cancel current action
                input_cancel_action(input);
            }

            return true;
        }

        case SDL_MOUSEBUTTONUP: {
            int button = event->button.button;
            if (button == SDL_BUTTON_LEFT) {
                // Auto-wire when dropping a component near other terminals
                if (input->is_dragging && input->dragging_component) {
                    Component *comp = input->dragging_component;
                    for (int i = 0; i < comp->num_terminals && i < 8; i++) {
                        float tx, ty;
                        component_get_terminal_pos(comp, i, &tx, &ty);

                        // Find existing nodes near this terminal (excluding component's own nodes)
                        for (int j = 0; j < circuit->num_nodes; j++) {
                            Node *existing = &circuit->nodes[j];
                            if (existing->id == comp->node_ids[i]) continue;

                            float dx = existing->x - tx;
                            float dy = existing->y - ty;
                            if (sqrt(dx*dx + dy*dy) <= 10) {
                                // Check if wire already exists
                                bool wire_exists = false;
                                for (int k = 0; k < circuit->num_wires; k++) {
                                    Wire *w = &circuit->wires[k];
                                    if ((w->start_node_id == comp->node_ids[i] && w->end_node_id == existing->id) ||
                                        (w->start_node_id == existing->id && w->end_node_id == comp->node_ids[i])) {
                                        wire_exists = true;
                                        break;
                                    }
                                }
                                if (!wire_exists) {
                                    circuit_add_wire(circuit, comp->node_ids[i], existing->id);
                                }
                            }
                        }
                    }
                }
                input->left.down = false;
                input->is_dragging = false;
                input->dragging_component = NULL;
            } else if (button == SDL_BUTTON_MIDDLE) {
                input->middle.down = false;
                input->is_panning = false;
            }
            ui_handle_click(ui, event->button.x, event->button.y, false);
            return true;
        }

        case SDL_MOUSEMOTION: {
            int x = event->motion.x;
            int y = event->motion.y;
            input->mouse_x = x;
            input->mouse_y = y;

            ui_handle_motion(ui, x, y);

            // Panning
            if (input->is_panning) {
                int dx = x - input->middle.start_x;
                int dy = y - input->middle.start_y;
                render_pan(render, dx, dy);
                input->middle.start_x = x;
                input->middle.start_y = y;
                return true;
            }

            // Check if in canvas area
            if (x < CANVAS_X || x >= CANVAS_X + CANVAS_WIDTH ||
                y < CANVAS_Y || y >= CANVAS_Y + CANVAS_HEIGHT) {
                return false;
            }

            float wx, wy;
            render_screen_to_world(render, x - CANVAS_X, y - CANVAS_Y, &wx, &wy);

            // Dragging component
            if (input->is_dragging && input->dragging_component) {
                float snapped_x = snap_to_grid(wx);
                float snapped_y = snap_to_grid(wy);
                input->dragging_component->x = snapped_x;
                input->dragging_component->y = snapped_y;
                circuit_update_component_nodes(circuit, input->dragging_component);
                return true;
            }

            // Wire preview
            if (input->drawing_wire) {
                input->wire_preview_x = snap_to_grid(wx);
                input->wire_preview_y = snap_to_grid(wy);
                return true;
            }

            // Highlight on hover
            if (input->current_tool == TOOL_SELECT) {
                for (int i = 0; i < circuit->num_components; i++) {
                    circuit->components[i]->highlighted =
                        component_contains_point(circuit->components[i], wx, wy);
                }
            }

            return true;
        }

        case SDL_MOUSEWHEEL: {
            int x = input->mouse_x;
            int y = input->mouse_y;

            if (x >= CANVAS_X && x < CANVAS_X + CANVAS_WIDTH &&
                y >= CANVAS_Y && y < CANVAS_Y + CANVAS_HEIGHT) {
                float factor = event->wheel.y > 0 ? 1.1f : 0.9f;
                render_zoom(render, factor, x, y);
                return true;
            }
            break;
        }

        case SDL_KEYDOWN: {
            input_handle_key(input, event->key.keysym.sym, circuit, render);

            // Update modifier state
            SDL_Keymod mod = SDL_GetModState();
            input->shift_down = (mod & KMOD_SHIFT) != 0;
            input->ctrl_down = (mod & KMOD_CTRL) != 0;
            input->alt_down = (mod & KMOD_ALT) != 0;
            return true;
        }

        case SDL_KEYUP: {
            SDL_Keymod mod = SDL_GetModState();
            input->shift_down = (mod & KMOD_SHIFT) != 0;
            input->ctrl_down = (mod & KMOD_CTRL) != 0;
            input->alt_down = (mod & KMOD_ALT) != 0;
            return true;
        }
    }

    return false;
}

void input_handle_key(InputState *input, SDL_Keycode key,
                      Circuit *circuit, RenderContext *render) {
    bool ctrl = input->ctrl_down;

    switch (key) {
        case SDLK_ESCAPE:
            input_cancel_action(input);
            input_set_tool(input, TOOL_SELECT);
            break;

        case SDLK_DELETE:
        case SDLK_BACKSPACE:
            input_delete_selected(input, circuit);
            break;

        case SDLK_r:
            // Rotate selected component
            if (input->selected_component) {
                component_rotate(input->selected_component);
                circuit_update_component_nodes(circuit, input->selected_component);
            }
            break;

        case SDLK_g:
            // Toggle grid
            if (!ctrl) {
                render->show_grid = !render->show_grid;
            }
            break;

        case SDLK_c:
            if (ctrl) {
                input_copy(input, circuit);
            }
            break;

        case SDLK_x:
            if (ctrl) {
                input_cut(input, circuit);
            }
            break;

        case SDLK_v:
            if (ctrl) {
                input_paste(input, circuit, render);
            }
            break;

        case SDLK_d:
            if (ctrl) {
                input_duplicate(input, circuit);
            }
            break;

        case SDLK_a:
            if (ctrl) {
                circuit_select_all(circuit);
            }
            break;

        case SDLK_z:
            if (ctrl) {
                circuit_undo(circuit);
                input->selected_component = NULL;
            }
            break;

        case SDLK_0:
            if (ctrl) {
                render_reset_view(render);
            }
            break;

        case SDLK_PLUS:
        case SDLK_EQUALS:
            if (ctrl) {
                render_zoom(render, 1.2f, CANVAS_X + CANVAS_WIDTH/2, CANVAS_Y + CANVAS_HEIGHT/2);
            }
            break;

        case SDLK_MINUS:
            if (ctrl) {
                render_zoom(render, 0.8f, CANVAS_X + CANVAS_WIDTH/2, CANVAS_Y + CANVAS_HEIGHT/2);
            }
            break;

        case SDLK_F1:
            // Toggle shortcuts dialog (handled by app)
            break;

        default:
            break;
    }
}

void input_set_tool(InputState *input, ToolType tool) {
    if (!input) return;

    input_cancel_action(input);
    input->current_tool = tool;
    input->placing_component = COMP_NONE;
}

void input_start_placing(InputState *input, ComponentType type) {
    if (!input) return;

    input_cancel_action(input);
    input->current_tool = TOOL_COMPONENT;
    input->placing_component = type;
}

void input_cancel_action(InputState *input) {
    if (!input) return;

    input->drawing_wire = false;
    input->is_dragging = false;
    input->dragging_component = NULL;
}

void input_delete_selected(InputState *input, Circuit *circuit) {
    if (!input || !circuit) return;

    if (input->selected_component) {
        circuit_remove_component(circuit, input->selected_component->id);
        input->selected_component = NULL;
    }
}

void input_copy(InputState *input, Circuit *circuit) {
    if (!input || !circuit || !input->selected_component) return;

    circuit_copy_component(circuit, input->selected_component);
}

void input_cut(InputState *input, Circuit *circuit) {
    if (!input || !circuit || !input->selected_component) return;

    circuit_cut_component(circuit, input->selected_component);
    input->selected_component = NULL;
}

void input_paste(InputState *input, Circuit *circuit, RenderContext *render) {
    if (!input || !circuit || !circuit->clipboard) return;

    // Paste at current mouse position or center of canvas
    float wx, wy;
    if (input->mouse_x >= CANVAS_X && input->mouse_x < CANVAS_X + CANVAS_WIDTH &&
        input->mouse_y >= CANVAS_Y && input->mouse_y < CANVAS_Y + CANVAS_HEIGHT) {
        render_screen_to_world(render,
            input->mouse_x - CANVAS_X,
            input->mouse_y - CANVAS_Y,
            &wx, &wy);
    } else {
        render_screen_to_world(render, CANVAS_WIDTH/2, CANVAS_HEIGHT/2, &wx, &wy);
    }

    float snapped_x = snap_to_grid(wx);
    float snapped_y = snap_to_grid(wy);

    Component *pasted = circuit_paste_component(circuit, snapped_x, snapped_y);
    if (pasted) {
        // Deselect previous and select pasted
        if (input->selected_component) {
            input->selected_component->selected = false;
        }
        pasted->selected = true;
        input->selected_component = pasted;
    }
}

void input_duplicate(InputState *input, Circuit *circuit) {
    if (!input || !circuit || !input->selected_component) return;

    Component *dup = circuit_duplicate_component(circuit, input->selected_component);
    if (dup) {
        input->selected_component->selected = false;
        dup->selected = true;
        input->selected_component = dup;
    }
}

void input_update_cursor(InputState *input) {
    // SDL cursor management would go here
    // For now, using default cursor
}
