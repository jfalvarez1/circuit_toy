/**
 * Circuit Playground - Input Handling Implementation
 */

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "input.h"
#include "app.h"

void input_init(InputState *input) {
    memset(input, 0, sizeof(InputState));
    input->current_tool = TOOL_SELECT;
    input->placing_component = COMP_NONE;
    input->pending_ui_action = UI_ACTION_NONE;
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
                } else {
                    // Store simulation/file actions for app to process
                    input->pending_ui_action = action;
                }
                return true;
            }

            // Check if in canvas area (use dynamic canvas bounds)
            if (x < render->canvas_rect.x || x >= render->canvas_rect.x + render->canvas_rect.w ||
                y < render->canvas_rect.y || y >= render->canvas_rect.y + render->canvas_rect.h) {
                return false;
            }

            // Convert to world coordinates
            float wx, wy;
            render_screen_to_world(render, x - render->canvas_rect.x, y - render->canvas_rect.y, &wx, &wy);

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
                        // Search at raw world coordinates with generous threshold
                        // Don't snap - we want to find the nearest node to where user clicked
                        Node *node = circuit_find_node_at(circuit, wx, wy, 25);
                        if (node) {
                            circuit_add_probe(circuit, node->id, node->x, node->y);
                            ui_set_status(ui, "Probe placed");
                        } else {
                            ui_set_status(ui, "Click on a node or wire junction to place probe");
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

            // Check if in canvas area (use dynamic canvas bounds)
            if (x < render->canvas_rect.x || x >= render->canvas_rect.x + render->canvas_rect.w ||
                y < render->canvas_rect.y || y >= render->canvas_rect.y + render->canvas_rect.h) {
                return false;
            }

            float wx, wy;
            render_screen_to_world(render, x - render->canvas_rect.x, y - render->canvas_rect.y, &wx, &wy);

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

            if (x >= render->canvas_rect.x && x < render->canvas_rect.x + render->canvas_rect.w &&
                y >= render->canvas_rect.y && y < render->canvas_rect.y + render->canvas_rect.h) {
                float factor = event->wheel.y > 0 ? 1.1f : 0.9f;
                render_zoom(render, factor, x, y);
                return true;
            }
            break;
        }

        case SDL_KEYDOWN: {
            // Update modifier state first
            SDL_Keymod mod = SDL_GetModState();
            input->shift_down = (mod & KMOD_SHIFT) != 0;
            input->ctrl_down = (mod & KMOD_CTRL) != 0;
            input->alt_down = (mod & KMOD_ALT) != 0;

            // If editing a property, handle text editing keys
            if (input->editing_property) {
                SDL_Keycode key = event->key.keysym.sym;
                if (key == SDLK_ESCAPE) {
                    input_cancel_property_edit(input);
                } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
                    // Apply will be handled by app layer which has component access
                    input->pending_ui_action = 1000;  // Special action for property apply
                } else {
                    input_handle_text_key(input, key);
                }
                return true;
            }

            input_handle_key(input, event->key.keysym.sym, circuit, render);
            return true;
        }

        case SDL_TEXTINPUT: {
            if (input->editing_property) {
                input_handle_text_input(input, event->text.text);
                return true;
            }
            break;
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
            if (ctrl) {
                // Ctrl+R: Rotate selected component
                if (input->selected_component) {
                    component_rotate(input->selected_component);
                    circuit_update_component_nodes(circuit, input->selected_component);
                }
            } else {
                // R: Add resistor
                input_start_placing(input, COMP_RESISTOR);
            }
            break;

        case SDLK_l:
            if (!ctrl) {
                // L: Add inductor
                input_start_placing(input, COMP_INDUCTOR);
            }
            break;

        case SDLK_d:
            if (ctrl) {
                input_duplicate(input, circuit);
            } else {
                // D: Add diode
                input_start_placing(input, COMP_DIODE);
            }
            break;

        case SDLK_g:
            // G: Add ground
            if (!ctrl) {
                input_start_placing(input, COMP_GROUND);
            }
            break;

        case SDLK_i:
            // I: Toggle current flow arrows
            if (!ctrl) {
                render->show_current = !render->show_current;
            }
            break;

        case SDLK_w:
            // Wire tool
            if (!ctrl) {
                input_set_tool(input, TOOL_WIRE);
            }
            break;

        case SDLK_s:
            // Select tool (not Ctrl+S which would be save)
            if (!ctrl) {
                input_set_tool(input, TOOL_SELECT);
            }
            break;

        case SDLK_c:
            if (ctrl) {
                input_copy(input, circuit);
            } else {
                // C: Add capacitor
                input_start_placing(input, COMP_CAPACITOR);
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
                render_zoom(render, 1.2f, render->canvas_rect.x + render->canvas_rect.w/2,
                           render->canvas_rect.y + render->canvas_rect.h/2);
            } else if (input->selected_component) {
                // Increase component value
                Component *c = input->selected_component;
                switch (c->type) {
                    case COMP_DC_VOLTAGE:
                        c->props.dc_voltage.voltage *= 1.1;
                        break;
                    case COMP_AC_VOLTAGE:
                        c->props.ac_voltage.amplitude *= 1.1;
                        break;
                    case COMP_DC_CURRENT:
                        c->props.dc_current.current *= 1.1;
                        break;
                    case COMP_RESISTOR:
                        c->props.resistor.resistance *= 1.1;
                        break;
                    case COMP_CAPACITOR:
                        c->props.capacitor.capacitance *= 1.1;
                        break;
                    case COMP_INDUCTOR:
                        c->props.inductor.inductance *= 1.1;
                        break;
                    case COMP_SQUARE_WAVE:
                        c->props.square_wave.amplitude *= 1.1;
                        break;
                    case COMP_TRIANGLE_WAVE:
                        c->props.triangle_wave.amplitude *= 1.1;
                        break;
                    case COMP_SAWTOOTH_WAVE:
                        c->props.sawtooth_wave.amplitude *= 1.1;
                        break;
                    case COMP_NOISE_SOURCE:
                        c->props.noise_source.amplitude *= 1.1;
                        break;
                    default:
                        break;
                }
            }
            break;

        case SDLK_MINUS:
            if (ctrl) {
                render_zoom(render, 0.8f, render->canvas_rect.x + render->canvas_rect.w/2,
                           render->canvas_rect.y + render->canvas_rect.h/2);
            } else if (input->selected_component) {
                // Decrease component value
                Component *c = input->selected_component;
                switch (c->type) {
                    case COMP_DC_VOLTAGE:
                        c->props.dc_voltage.voltage /= 1.1;
                        break;
                    case COMP_AC_VOLTAGE:
                        c->props.ac_voltage.amplitude /= 1.1;
                        break;
                    case COMP_DC_CURRENT:
                        c->props.dc_current.current /= 1.1;
                        break;
                    case COMP_RESISTOR:
                        c->props.resistor.resistance /= 1.1;
                        if (c->props.resistor.resistance < 0.1)
                            c->props.resistor.resistance = 0.1;
                        break;
                    case COMP_CAPACITOR:
                        c->props.capacitor.capacitance /= 1.1;
                        break;
                    case COMP_INDUCTOR:
                        c->props.inductor.inductance /= 1.1;
                        break;
                    case COMP_SQUARE_WAVE:
                        c->props.square_wave.amplitude /= 1.1;
                        if (c->props.square_wave.amplitude < 0.001)
                            c->props.square_wave.amplitude = 0.001;
                        break;
                    case COMP_TRIANGLE_WAVE:
                        c->props.triangle_wave.amplitude /= 1.1;
                        if (c->props.triangle_wave.amplitude < 0.001)
                            c->props.triangle_wave.amplitude = 0.001;
                        break;
                    case COMP_SAWTOOTH_WAVE:
                        c->props.sawtooth_wave.amplitude /= 1.1;
                        if (c->props.sawtooth_wave.amplitude < 0.001)
                            c->props.sawtooth_wave.amplitude = 0.001;
                        break;
                    case COMP_NOISE_SOURCE:
                        c->props.noise_source.amplitude /= 1.1;
                        if (c->props.noise_source.amplitude < 0.001)
                            c->props.noise_source.amplitude = 0.001;
                        break;
                    default:
                        break;
                }
            }
            break;

        case SDLK_f:
            // Adjust frequency for waveform sources
            if (input->selected_component) {
                Component *c = input->selected_component;
                double *freq = NULL;

                switch (c->type) {
                    case COMP_AC_VOLTAGE:
                        freq = &c->props.ac_voltage.frequency;
                        break;
                    case COMP_SQUARE_WAVE:
                        freq = &c->props.square_wave.frequency;
                        break;
                    case COMP_TRIANGLE_WAVE:
                        freq = &c->props.triangle_wave.frequency;
                        break;
                    case COMP_SAWTOOTH_WAVE:
                        freq = &c->props.sawtooth_wave.frequency;
                        break;
                    default:
                        break;
                }

                if (freq) {
                    if (input->shift_down) {
                        // Shift+F decreases frequency
                        *freq /= 1.2;
                        if (*freq < 0.1) *freq = 0.1;
                    } else {
                        // F increases frequency
                        *freq *= 1.2;
                        if (*freq > 100000000) *freq = 100000000;  // 100MHz max
                    }
                }
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
    if (input->mouse_x >= render->canvas_rect.x && input->mouse_x < render->canvas_rect.x + render->canvas_rect.w &&
        input->mouse_y >= render->canvas_rect.y && input->mouse_y < render->canvas_rect.y + render->canvas_rect.h) {
        render_screen_to_world(render,
            input->mouse_x - render->canvas_rect.x,
            input->mouse_y - render->canvas_rect.y,
            &wx, &wy);
    } else {
        render_screen_to_world(render, render->canvas_rect.w/2, render->canvas_rect.h/2, &wx, &wy);
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

// Parse value with engineering notation (k, M, G, m, u/μ, n, p suffixes)
double parse_engineering_value(const char *str) {
    if (!str || !*str) return 0.0;

    char *endptr;
    double value = strtod(str, &endptr);

    // Skip whitespace
    while (*endptr == ' ') endptr++;

    // Parse suffix
    double multiplier = 1.0;
    switch (*endptr) {
        case 'T': case 't': multiplier = 1e12; break;
        case 'G': case 'g': multiplier = 1e9; break;
        case 'M': multiplier = 1e6; break;  // Keep M uppercase for Mega
        case 'k': case 'K': multiplier = 1e3; break;
        case 'm': multiplier = 1e-3; break;  // milli
        case 'u': case 'U': multiplier = 1e-6; break;  // micro (u for μ)
        case 'n': case 'N': multiplier = 1e-9; break;
        case 'p': case 'P': multiplier = 1e-12; break;
        case 'f': case 'F': multiplier = 1e-15; break;
        default: break;
    }

    return value * multiplier;
}

void input_start_property_edit(InputState *input, PropertyType prop, const char *initial_value) {
    if (!input) return;

    input->editing_property = true;
    input->editing_prop_type = prop;

    if (initial_value) {
        strncpy(input->input_buffer, initial_value, sizeof(input->input_buffer) - 1);
        input->input_buffer[sizeof(input->input_buffer) - 1] = '\0';
        input->input_len = strlen(input->input_buffer);
    } else {
        input->input_buffer[0] = '\0';
        input->input_len = 0;
    }
    input->input_cursor = input->input_len;

    // Start SDL text input
    SDL_StartTextInput();
}

void input_cancel_property_edit(InputState *input) {
    if (!input) return;

    input->editing_property = false;
    input->editing_prop_type = PROP_NONE;
    input->input_buffer[0] = '\0';
    input->input_len = 0;
    input->input_cursor = 0;

    SDL_StopTextInput();
}

bool input_apply_property_edit(InputState *input, Component *comp) {
    if (!input || !comp || !input->editing_property) return false;

    double value = parse_engineering_value(input->input_buffer);

    // Validate and apply based on property type and component
    bool applied = false;

    switch (input->editing_prop_type) {
        case PROP_VALUE:
        case PROP_AMPLITUDE:
            switch (comp->type) {
                case COMP_DC_VOLTAGE:
                    comp->props.dc_voltage.voltage = value;
                    applied = true;
                    break;
                case COMP_AC_VOLTAGE:
                    if (value > 0) { comp->props.ac_voltage.amplitude = value; applied = true; }
                    break;
                case COMP_DC_CURRENT:
                    comp->props.dc_current.current = value;
                    applied = true;
                    break;
                case COMP_RESISTOR:
                    if (value > 0) { comp->props.resistor.resistance = value; applied = true; }
                    break;
                case COMP_CAPACITOR:
                    if (value > 0) { comp->props.capacitor.capacitance = value; applied = true; }
                    break;
                case COMP_INDUCTOR:
                    if (value > 0) { comp->props.inductor.inductance = value; applied = true; }
                    break;
                case COMP_SQUARE_WAVE:
                    if (value > 0) { comp->props.square_wave.amplitude = value; applied = true; }
                    break;
                case COMP_TRIANGLE_WAVE:
                    if (value > 0) { comp->props.triangle_wave.amplitude = value; applied = true; }
                    break;
                case COMP_SAWTOOTH_WAVE:
                    if (value > 0) { comp->props.sawtooth_wave.amplitude = value; applied = true; }
                    break;
                case COMP_NOISE_SOURCE:
                    if (value > 0) { comp->props.noise_source.amplitude = value; applied = true; }
                    break;
                default:
                    break;
            }
            break;

        case PROP_FREQUENCY:
            if (value > 0 && value <= 1e9) {  // Max 1GHz
                switch (comp->type) {
                    case COMP_AC_VOLTAGE:
                        comp->props.ac_voltage.frequency = value;
                        applied = true;
                        break;
                    case COMP_SQUARE_WAVE:
                        comp->props.square_wave.frequency = value;
                        applied = true;
                        break;
                    case COMP_TRIANGLE_WAVE:
                        comp->props.triangle_wave.frequency = value;
                        applied = true;
                        break;
                    case COMP_SAWTOOTH_WAVE:
                        comp->props.sawtooth_wave.frequency = value;
                        applied = true;
                        break;
                    default:
                        break;
                }
            }
            break;

        case PROP_PHASE:
            switch (comp->type) {
                case COMP_AC_VOLTAGE:
                    comp->props.ac_voltage.phase = value;
                    applied = true;
                    break;
                case COMP_SQUARE_WAVE:
                    comp->props.square_wave.phase = value;
                    applied = true;
                    break;
                case COMP_TRIANGLE_WAVE:
                    comp->props.triangle_wave.phase = value;
                    applied = true;
                    break;
                case COMP_SAWTOOTH_WAVE:
                    comp->props.sawtooth_wave.phase = value;
                    applied = true;
                    break;
                default:
                    break;
            }
            break;

        case PROP_OFFSET:
            switch (comp->type) {
                case COMP_AC_VOLTAGE:
                    comp->props.ac_voltage.offset = value;
                    applied = true;
                    break;
                case COMP_SQUARE_WAVE:
                    comp->props.square_wave.offset = value;
                    applied = true;
                    break;
                case COMP_TRIANGLE_WAVE:
                    comp->props.triangle_wave.offset = value;
                    applied = true;
                    break;
                case COMP_SAWTOOTH_WAVE:
                    comp->props.sawtooth_wave.offset = value;
                    applied = true;
                    break;
                default:
                    break;
            }
            break;

        case PROP_DUTY:
            if (comp->type == COMP_SQUARE_WAVE && value >= 0 && value <= 100) {
                comp->props.square_wave.duty = value / 100.0;  // Convert percentage to fraction
                applied = true;
            }
            break;

        default:
            break;
    }

    input_cancel_property_edit(input);
    return applied;
}

void input_handle_text_input(InputState *input, const char *text) {
    if (!input || !input->editing_property || !text) return;

    int text_len = strlen(text);
    if (input->input_len + text_len >= sizeof(input->input_buffer) - 1) return;

    // Insert text at cursor position
    memmove(input->input_buffer + input->input_cursor + text_len,
            input->input_buffer + input->input_cursor,
            input->input_len - input->input_cursor + 1);
    memcpy(input->input_buffer + input->input_cursor, text, text_len);

    input->input_cursor += text_len;
    input->input_len += text_len;
}

void input_handle_text_key(InputState *input, SDL_Keycode key) {
    if (!input || !input->editing_property) return;

    switch (key) {
        case SDLK_BACKSPACE:
            if (input->input_cursor > 0) {
                memmove(input->input_buffer + input->input_cursor - 1,
                        input->input_buffer + input->input_cursor,
                        input->input_len - input->input_cursor + 1);
                input->input_cursor--;
                input->input_len--;
            }
            break;

        case SDLK_DELETE:
            if (input->input_cursor < input->input_len) {
                memmove(input->input_buffer + input->input_cursor,
                        input->input_buffer + input->input_cursor + 1,
                        input->input_len - input->input_cursor);
                input->input_len--;
            }
            break;

        case SDLK_LEFT:
            if (input->input_cursor > 0) input->input_cursor--;
            break;

        case SDLK_RIGHT:
            if (input->input_cursor < input->input_len) input->input_cursor++;
            break;

        case SDLK_HOME:
            input->input_cursor = 0;
            break;

        case SDLK_END:
            input->input_cursor = input->input_len;
            break;

        default:
            break;
    }
}
