/**
 * Circuit Playground - Input Handling Implementation
 */

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "input.h"
#include "app.h"
#include "circuits.h"

void input_init(InputState *input) {
    memset(input, 0, sizeof(InputState));
    input->current_tool = TOOL_SELECT;
    input->placing_component = COMP_NONE;
    input->pending_ui_action = UI_ACTION_NONE;
    input->dragging_probe_idx = -1;
    input->selected_probe_idx = -1;
    input->selected_wire_idx = -1;
    input->multi_selected_count = 0;
}

// Helper to find wire near a point
static int find_wire_at(Circuit *circuit, float wx, float wy, float threshold) {
    if (!circuit) return -1;

    for (int i = 0; i < circuit->num_wires; i++) {
        Wire *wire = &circuit->wires[i];
        Node *n1 = circuit_get_node(circuit, wire->start_node_id);
        Node *n2 = circuit_get_node(circuit, wire->end_node_id);
        if (!n1 || !n2) continue;

        // Calculate distance from point to line segment
        float dx = n2->x - n1->x;
        float dy = n2->y - n1->y;
        float len_sq = dx * dx + dy * dy;

        if (len_sq < 1.0f) continue;  // Skip very short wires

        // Parameter t for closest point on line
        float t = ((wx - n1->x) * dx + (wy - n1->y) * dy) / len_sq;
        t = fmaxf(0.0f, fminf(1.0f, t));

        // Closest point on segment
        float closest_x = n1->x + t * dx;
        float closest_y = n1->y + t * dy;

        float dist = sqrtf((wx - closest_x) * (wx - closest_x) + (wy - closest_y) * (wy - closest_y));
        if (dist <= threshold) {
            return i;
        }
    }
    return -1;
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
                } else if (action >= UI_ACTION_SELECT_COMP && action < UI_ACTION_SELECT_CIRCUIT) {
                    input_start_placing(input, action - UI_ACTION_SELECT_COMP);
                } else if (action >= UI_ACTION_SELECT_CIRCUIT && action < UI_ACTION_PROP_APPLY) {
                    // Circuit template selected - store action for app to process
                    // and set input to select tool for placement click
                    input_set_tool(input, TOOL_SELECT);
                    input->pending_ui_action = action;
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

                // Check if placing a circuit template
                if (ui->placing_circuit && ui->selected_circuit_type > 0) {
                    if (!input->sim_running) {
                        float snapped_x = snap_to_grid(wx);
                        float snapped_y = snap_to_grid(wy);

                        int count = circuit_place_template(circuit, ui->selected_circuit_type, snapped_x, snapped_y);
                        if (count > 0) {
                            const CircuitTemplateInfo *info = circuit_template_get_info(ui->selected_circuit_type);
                            char msg[128];
                            snprintf(msg, sizeof(msg), "Placed %s circuit (%d components)", info->name, count);
                            ui_set_status(ui, msg);
                            circuit->modified = true;
                        } else {
                            ui_set_status(ui, "Failed to place circuit");
                        }

                        // Deselect circuit template after placing
                        ui->placing_circuit = false;
                        ui->selected_circuit_type = -1;
                        for (int i = 0; i < ui->num_circuit_items; i++) {
                            ui->circuit_items[i].selected = false;
                        }
                    } else {
                        ui_set_status(ui, "Stop simulation to place circuits");
                    }
                    return true;
                }

                switch (input->current_tool) {
                    case TOOL_SELECT: {
                        // If already drawing a wire (started from terminal click), complete it
                        if (input->drawing_wire) {
                            if (input->sim_running) {
                                ui_set_status(ui, "Stop simulation to create wires");
                                input->drawing_wire = false;
                                break;
                            }
                            float snapped_x = snap_to_grid(wx);
                            float snapped_y = snap_to_grid(wy);
                            int end_node_id = circuit_find_or_create_node(circuit, snapped_x, snapped_y, 10);
                            if (end_node_id != input->wire_start_node) {
                                int wire_id = circuit_add_wire(circuit, input->wire_start_node, end_node_id);
                                if (wire_id >= 0) {
                                    circuit_push_undo(circuit, UNDO_ADD_WIRE, wire_id, NULL, input->wire_start_node, end_node_id);
                                }
                                ui_set_status(ui, "Wire connected (Ctrl+Z to undo)");
                            }
                            input->drawing_wire = false;
                            break;
                        }

                        // Check if click is near a node/terminal to start a wire
                        if (!input->sim_running) {
                            Node *node = circuit_find_node_at(circuit, wx, wy, 12);
                            if (node) {
                                // Start drawing wire from this node
                                input->wire_start_node = node->id;
                                input->drawing_wire = true;
                                input->wire_preview_x = node->x;
                                input->wire_preview_y = node->y;
                                ui_set_status(ui, "Click another terminal to connect wire");
                                break;
                            }
                        }

                        // Check for probe click (probes have visual priority)
                        bool found_probe = false;
                        for (int i = 0; i < circuit->num_probes; i++) {
                            Probe *probe = &circuit->probes[i];
                            float dx = probe->x - wx;
                            float dy = probe->y - wy;
                            // Check if click is near probe tip (within 15 units)
                            if (sqrt(dx*dx + dy*dy) < 15) {
                                // Clear previous selections
                                if (input->selected_component) {
                                    input->selected_component->selected = false;
                                    input->selected_component = NULL;
                                }
                                if (input->selected_wire_idx >= 0 && input->selected_wire_idx < circuit->num_wires) {
                                    circuit->wires[input->selected_wire_idx].selected = false;
                                }
                                input->selected_wire_idx = -1;
                                // Clear previous probe selection
                                if (input->selected_probe_idx >= 0 && input->selected_probe_idx < circuit->num_probes) {
                                    circuit->probes[input->selected_probe_idx].selected = false;
                                }
                                input->multi_selected_count = 0;
                                // Clear any property editing mode
                                input->editing_property = false;

                                // Select this probe
                                input->selected_probe_idx = i;
                                probe->selected = true;
                                input->dragging_probe_idx = i;
                                found_probe = true;
                                ui_set_status(ui, "Probe selected - drag to move, Delete to remove");
                                break;
                            }
                        }

                        if (!found_probe) {
                            // Find component at click position
                            Component *comp = circuit_find_component_at(circuit, wx, wy);
                            if (comp) {
                                // Clear previous selections
                                if (input->selected_component && input->selected_component != comp) {
                                    input->selected_component->selected = false;
                                }
                                // Clear wire selection
                                if (input->selected_wire_idx >= 0 && input->selected_wire_idx < circuit->num_wires) {
                                    circuit->wires[input->selected_wire_idx].selected = false;
                                }
                                input->selected_wire_idx = -1;
                                // Clear probe selection
                                if (input->selected_probe_idx >= 0 && input->selected_probe_idx < circuit->num_probes) {
                                    circuit->probes[input->selected_probe_idx].selected = false;
                                }
                                input->selected_probe_idx = -1;
                                input->multi_selected_count = 0;

                                comp->selected = true;
                                input->selected_component = comp;
                                // Auto-pause simulation when starting to drag
                                if (input->sim_running) {
                                    input->pending_ui_action = UI_ACTION_PAUSE;
                                    ui_set_status(ui, "Simulation paused - drag component to move");
                                }
                                input->is_dragging = true;
                                input->dragging_component = comp;
                            } else {
                                // Check for wire click
                                int wire_idx = find_wire_at(circuit, wx, wy, 8.0f);
                                if (wire_idx >= 0) {
                                    // Clear previous selections
                                    if (input->selected_component) {
                                        input->selected_component->selected = false;
                                        input->selected_component = NULL;
                                    }
                                    // Clear previous wire selection
                                    if (input->selected_wire_idx >= 0 && input->selected_wire_idx < circuit->num_wires) {
                                        circuit->wires[input->selected_wire_idx].selected = false;
                                    }
                                    // Clear probe selection
                                    if (input->selected_probe_idx >= 0 && input->selected_probe_idx < circuit->num_probes) {
                                        circuit->probes[input->selected_probe_idx].selected = false;
                                    }
                                    input->selected_probe_idx = -1;
                                    input->multi_selected_count = 0;

                                    input->selected_wire_idx = wire_idx;
                                    circuit->wires[wire_idx].selected = true;
                                    ui_set_status(ui, "Wire selected - press Delete to remove");
                                } else {
                                    // Empty space - start box selection or deselect
                                    if (input->selected_component) {
                                        input->selected_component->selected = false;
                                        input->selected_component = NULL;
                                    }
                                    // Clear wire selection
                                    if (input->selected_wire_idx >= 0 && input->selected_wire_idx < circuit->num_wires) {
                                        circuit->wires[input->selected_wire_idx].selected = false;
                                    }
                                    input->selected_wire_idx = -1;
                                    // Clear probe selection
                                    if (input->selected_probe_idx >= 0 && input->selected_probe_idx < circuit->num_probes) {
                                        circuit->probes[input->selected_probe_idx].selected = false;
                                    }
                                    input->selected_probe_idx = -1;

                                    // Clear multi-selection
                                    for (int i = 0; i < input->multi_selected_count; i++) {
                                        if (input->multi_selected[i]) {
                                            input->multi_selected[i]->selected = false;
                                        }
                                    }
                                    input->multi_selected_count = 0;

                                    // Start box selection
                                    input->box_selecting = true;
                                    input->box_start_x = wx;
                                    input->box_start_y = wy;
                                    input->box_end_x = wx;
                                    input->box_end_y = wy;
                                }
                            }
                        }
                        break;
                    }

                    case TOOL_COMPONENT: {
                        // Don't allow placing components while simulation running
                        if (input->sim_running) break;
                        if (input->placing_component != COMP_NONE) {
                            float snapped_x = snap_to_grid(wx);
                            float snapped_y = snap_to_grid(wy);

                            // Check for existing nodes near terminals before placing
                            Component *temp = component_create(input->placing_component, snapped_x, snapped_y);
                            if (temp) {
                                // Apply placing rotation
                                temp->rotation = input->placing_rotation;

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
                        // Don't allow drawing wires while simulation running
                        if (input->sim_running) break;
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
                        // Don't allow deletion while simulation running
                        if (input->sim_running) {
                            ui_set_status(ui, "Stop simulation to delete components");
                            break;
                        }
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

                // Finalize probe dragging - connect to nearest node
                if (input->dragging_probe_idx >= 0 && input->dragging_probe_idx < circuit->num_probes) {
                    Probe *probe = &circuit->probes[input->dragging_probe_idx];

                    // Find nearest node to probe position
                    Node *nearest = circuit_find_node_at(circuit, probe->x, probe->y, 30);
                    if (nearest) {
                        // Snap probe to node and update connection
                        probe->x = nearest->x;
                        probe->y = nearest->y;
                        probe->node_id = nearest->id;
                        ui_set_status(ui, "Probe connected to node");
                    } else {
                        ui_set_status(ui, "Probe not near any node - voltage reading may be incorrect");
                    }
                    input->dragging_probe_idx = -1;
                }

                // Complete box selection
                if (input->box_selecting) {
                    input->box_selecting = false;

                    // Calculate box bounds (handle any drag direction)
                    float min_x = fminf(input->box_start_x, input->box_end_x);
                    float max_x = fmaxf(input->box_start_x, input->box_end_x);
                    float min_y = fminf(input->box_start_y, input->box_end_y);
                    float max_y = fmaxf(input->box_start_y, input->box_end_y);

                    // Only select if box is at least 5 pixels in size
                    if (max_x - min_x > 5 || max_y - min_y > 5) {
                        input->multi_selected_count = 0;

                        // Find components within box
                        for (int i = 0; i < circuit->num_components && input->multi_selected_count < 64; i++) {
                            Component *comp = circuit->components[i];
                            // Check if component center is within box
                            if (comp->x >= min_x && comp->x <= max_x &&
                                comp->y >= min_y && comp->y <= max_y) {
                                comp->selected = true;
                                input->multi_selected[input->multi_selected_count++] = comp;
                            }
                        }

                        if (input->multi_selected_count > 0) {
                            char msg[64];
                            snprintf(msg, sizeof(msg), "Selected %d components - press Delete to remove", input->multi_selected_count);
                            ui_set_status(ui, msg);
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

            // Dragging probe
            if (input->dragging_probe_idx >= 0 && input->dragging_probe_idx < circuit->num_probes) {
                Probe *probe = &circuit->probes[input->dragging_probe_idx];
                probe->x = wx;
                probe->y = wy;
                return true;
            }

            // Wire preview
            if (input->drawing_wire) {
                input->wire_preview_x = snap_to_grid(wx);
                input->wire_preview_y = snap_to_grid(wy);
                return true;
            }

            // Box selection
            if (input->box_selecting) {
                input->box_end_x = wx;
                input->box_end_y = wy;
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

            // Check if mouse is in palette area (left sidebar) - scroll palette
            if (ui_point_in_palette(ui, x, y)) {
                ui_palette_scroll(ui, event->wheel.y);
                return true;
            }

            // Check if mouse is in properties panel area - scroll properties
            if (ui_point_in_properties(ui, x, y)) {
                ui_properties_scroll(ui, event->wheel.y);
                return true;
            }

            // Check if mouse is in properties panel (right side, below scroll area)
            // and there's a selected component - adjust value with wheel
            if (x >= render->canvas_rect.x + render->canvas_rect.w && input->selected_component) {
                Component *c = input->selected_component;
                double factor = event->wheel.y > 0 ? 1.05 : 0.95;  // 5% per tick
                if (input->shift_down) factor = event->wheel.y > 0 ? 1.2 : 0.8;  // 20% with shift

                // Adjust primary value based on component type
                switch (c->type) {
                    case COMP_DC_VOLTAGE:
                        c->props.dc_voltage.voltage *= factor;
                        break;
                    case COMP_AC_VOLTAGE:
                        c->props.ac_voltage.amplitude *= factor;
                        break;
                    case COMP_DC_CURRENT:
                        c->props.dc_current.current *= factor;
                        break;
                    case COMP_RESISTOR:
                        c->props.resistor.resistance *= factor;
                        if (c->props.resistor.resistance < 0.1) c->props.resistor.resistance = 0.1;
                        break;
                    case COMP_CAPACITOR:
                        c->props.capacitor.capacitance *= factor;
                        if (c->props.capacitor.capacitance < 1e-15) c->props.capacitor.capacitance = 1e-15;
                        break;
                    case COMP_INDUCTOR:
                        c->props.inductor.inductance *= factor;
                        if (c->props.inductor.inductance < 1e-12) c->props.inductor.inductance = 1e-12;
                        break;
                    case COMP_SQUARE_WAVE:
                        c->props.square_wave.amplitude *= factor;
                        break;
                    case COMP_TRIANGLE_WAVE:
                        c->props.triangle_wave.amplitude *= factor;
                        break;
                    case COMP_SAWTOOTH_WAVE:
                        c->props.sawtooth_wave.amplitude *= factor;
                        break;
                    case COMP_NOISE_SOURCE:
                        c->props.noise_source.amplitude *= factor;
                        break;
                    default:
                        break;
                }
                ui_set_status(ui, "Value adjusted (Shift+wheel for larger steps)");
                return true;
            }

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

            // Close panels on ESC
            if (event->key.keysym.sym == SDLK_ESCAPE && ui) {
                if (ui->show_bode_plot) {
                    ui->show_bode_plot = false;
                    return true;
                }
                if (ui->show_sweep_panel) {
                    ui->show_sweep_panel = false;
                    return true;
                }
                if (ui->show_monte_carlo_panel) {
                    ui->show_monte_carlo_panel = false;
                    return true;
                }
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
                // Ctrl+R: Rotate selected component or component being placed
                if (input->placing_component != COMP_NONE) {
                    // Rotate component being placed (before clicking)
                    input->placing_rotation = (input->placing_rotation + 90) % 360;
                } else if (input->selected_component) {
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

        case SDLK_v:
            if (ctrl) {
                input_paste(input, circuit, render);
            } else if (input->shift_down) {
                // Shift+V: Add AC voltage source
                input_start_placing(input, COMP_AC_VOLTAGE);
            } else {
                // V: Add DC voltage source
                input_start_placing(input, COMP_DC_VOLTAGE);
            }
            break;

        case SDLK_x:
            if (ctrl) {
                input_cut(input, circuit);
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

        case SDLK_SPACE:
            // Toggle switch state when a switch is selected
            if (input->selected_component) {
                Component *c = input->selected_component;
                switch (c->type) {
                    case COMP_SPST_SWITCH:
                        c->props.switch_spst.closed = !c->props.switch_spst.closed;
                        break;
                    case COMP_SPDT_SWITCH:
                        c->props.switch_spdt.position = (c->props.switch_spdt.position == 0) ? 1 : 0;
                        break;
                    case COMP_PUSH_BUTTON:
                        c->props.push_button.pressed = !c->props.push_button.pressed;
                        break;
                    default:
                        break;
                }
            }
            break;

        case SDLK_t:
            // T: Add SPST switch (toggle switch)
            if (!ctrl) {
                input_start_placing(input, COMP_SPST_SWITCH);
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
    input->placing_rotation = 0;
}

void input_cancel_action(InputState *input) {
    if (!input) return;

    input->drawing_wire = false;
    input->is_dragging = false;
    input->dragging_component = NULL;
    input->box_selecting = false;
}

void input_delete_selected(InputState *input, Circuit *circuit) {
    if (!input || !circuit) return;

    // Delete multi-selected components first
    if (input->multi_selected_count > 0) {
        for (int i = 0; i < input->multi_selected_count; i++) {
            if (input->multi_selected[i]) {
                circuit_remove_component(circuit, input->multi_selected[i]->id);
            }
        }
        input->multi_selected_count = 0;
        input->selected_component = NULL;
        return;
    }

    // Delete single selected component
    if (input->selected_component) {
        circuit_remove_component(circuit, input->selected_component->id);
        input->selected_component = NULL;
        return;
    }

    // Delete selected wire
    if (input->selected_wire_idx >= 0 && input->selected_wire_idx < circuit->num_wires) {
        circuit->wires[input->selected_wire_idx].selected = false;
        int wire_id = circuit->wires[input->selected_wire_idx].id;
        circuit_remove_wire(circuit, wire_id);
        input->selected_wire_idx = -1;
        return;
    }

    // Delete selected probe
    if (input->selected_probe_idx >= 0 && input->selected_probe_idx < circuit->num_probes) {
        circuit->probes[input->selected_probe_idx].selected = false;
        int probe_id = circuit->probes[input->selected_probe_idx].id;
        circuit_remove_probe(circuit, probe_id);
        input->selected_probe_idx = -1;
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
                case COMP_CAPACITOR_ELEC:
                    if (value > 0) { comp->props.capacitor_elec.capacitance = value; applied = true; }
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
                case COMP_LED:
                    // Wavelength in nm (380-780 visible, 0 = white, >780 = IR)
                    if (value >= 0 && value <= 1000) {
                        comp->props.led.wavelength = value;
                        // Update forward voltage based on color
                        if (value >= 380 && value < 440) {
                            comp->props.led.vf = 2.8;  // Violet
                            comp->props.led.max_current = 0.020;
                        } else if (value >= 440 && value < 510) {
                            comp->props.led.vf = 3.2;  // Blue/Cyan
                            comp->props.led.max_current = 0.020;
                        } else if (value >= 510 && value < 580) {
                            comp->props.led.vf = 2.2;  // Green
                            comp->props.led.max_current = 0.020;
                        } else if (value >= 580 && value < 600) {
                            comp->props.led.vf = 2.1;  // Yellow
                            comp->props.led.max_current = 0.020;
                        } else if (value >= 600 && value < 640) {
                            comp->props.led.vf = 2.0;  // Orange
                            comp->props.led.max_current = 0.020;
                        } else if (value >= 640 && value <= 780) {
                            comp->props.led.vf = 1.8;  // Red
                            comp->props.led.max_current = 0.020;
                        } else if (value > 780) {
                            comp->props.led.vf = 1.4;  // IR
                            comp->props.led.max_current = 0.050;
                        } else if (value == 0) {
                            comp->props.led.vf = 3.2;  // White
                            comp->props.led.max_current = 0.020;
                        }
                        applied = true;
                    }
                    break;
                case COMP_TRANSFORMER:
                case COMP_TRANSFORMER_CT:
                    if (value > 0 && value <= 1000) {
                        comp->props.transformer.turns_ratio = value;
                        applied = true;
                    }
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

        // BJT parameters
        case PROP_BJT_BETA:
            if ((comp->type == COMP_NPN_BJT || comp->type == COMP_PNP_BJT) && value > 0 && value <= 10000) {
                comp->props.bjt.bf = value;
                applied = true;
            }
            break;

        case PROP_BJT_IS:
            if ((comp->type == COMP_NPN_BJT || comp->type == COMP_PNP_BJT) && value > 0 && value <= 1e-6) {
                comp->props.bjt.is = value;
                applied = true;
            }
            break;

        case PROP_BJT_VAF:
            if ((comp->type == COMP_NPN_BJT || comp->type == COMP_PNP_BJT) && value > 0 && value <= 1000) {
                comp->props.bjt.vaf = value;
                applied = true;
            }
            break;

        case PROP_BJT_IDEAL:
            // Toggle ideal mode for BJT (doesn't use value, just toggles)
            if (comp->type == COMP_NPN_BJT || comp->type == COMP_PNP_BJT) {
                comp->props.bjt.ideal = !comp->props.bjt.ideal;
                applied = true;
            }
            break;

        // MOSFET parameters
        case PROP_MOS_VTH:
            if (comp->type == COMP_NMOS || comp->type == COMP_PMOS) {
                // Allow negative values for PMOS, typical range -5V to 5V
                if (value >= -10 && value <= 10) {
                    comp->props.mosfet.vth = value;
                    applied = true;
                }
            }
            break;

        case PROP_MOS_KP:
            if ((comp->type == COMP_NMOS || comp->type == COMP_PMOS) && value > 0 && value <= 1) {
                comp->props.mosfet.kp = value;
                applied = true;
            }
            break;

        case PROP_MOS_W:
            if ((comp->type == COMP_NMOS || comp->type == COMP_PMOS) && value > 0 && value <= 1) {
                comp->props.mosfet.w = value;
                applied = true;
            }
            break;

        case PROP_MOS_L:
            if ((comp->type == COMP_NMOS || comp->type == COMP_PMOS) && value > 0 && value <= 1) {
                comp->props.mosfet.l = value;
                applied = true;
            }
            break;

        case PROP_MOS_IDEAL:
            // Toggle ideal mode for MOSFET (doesn't use value, just toggles)
            if (comp->type == COMP_NMOS || comp->type == COMP_PMOS) {
                comp->props.mosfet.ideal = !comp->props.mosfet.ideal;
                applied = true;
            }
            break;

        // LED parameters
        case PROP_LED_COLOR: {
            // Cycle through LED color presets
            if (comp->type == COMP_LED) {
                // Color presets: Red, Orange, Yellow, Green, Cyan, Blue, Violet, White, IR
                double wl = comp->props.led.wavelength;
                // Determine current color and move to next
                if (wl >= 640 && wl <= 780) {
                    // Red -> Orange
                    comp->props.led.wavelength = 620;
                    comp->props.led.vf = 2.0;
                } else if (wl >= 600 && wl < 640) {
                    // Orange -> Yellow
                    comp->props.led.wavelength = 590;
                    comp->props.led.vf = 2.1;
                } else if (wl >= 580 && wl < 600) {
                    // Yellow -> Green
                    comp->props.led.wavelength = 550;
                    comp->props.led.vf = 2.2;
                } else if (wl >= 510 && wl < 580) {
                    // Green -> Cyan
                    comp->props.led.wavelength = 500;
                    comp->props.led.vf = 3.0;
                } else if (wl >= 490 && wl < 510) {
                    // Cyan -> Blue
                    comp->props.led.wavelength = 470;
                    comp->props.led.vf = 3.2;
                } else if (wl >= 440 && wl < 490) {
                    // Blue -> Violet
                    comp->props.led.wavelength = 420;
                    comp->props.led.vf = 2.8;
                } else if (wl >= 380 && wl < 440) {
                    // Violet -> White
                    comp->props.led.wavelength = 0;
                    comp->props.led.vf = 3.2;
                } else if (wl == 0) {
                    // White -> IR
                    comp->props.led.wavelength = 850;
                    comp->props.led.vf = 1.4;
                    comp->props.led.max_current = 0.050;
                } else {
                    // IR or unknown -> Red
                    comp->props.led.wavelength = 660;
                    comp->props.led.vf = 1.8;
                    comp->props.led.max_current = 0.020;
                }
                applied = true;
            }
            break;
        }

        case PROP_LED_VF:
            if (comp->type == COMP_LED && value > 0 && value <= 10) {
                comp->props.led.vf = value;
                applied = true;
            }
            break;

        case PROP_LED_IMAX:
            // Value entered in mA, stored in A
            if (comp->type == COMP_LED && value > 0 && value <= 1000) {
                comp->props.led.max_current = value / 1000.0;  // Convert mA to A
                applied = true;
            }
            break;

        // Source internal resistance
        case PROP_R_SERIES:
            if (value >= 0 && value <= 1e9) {
                if (comp->type == COMP_DC_VOLTAGE) {
                    comp->props.dc_voltage.r_series = value;
                    applied = true;
                } else if (comp->type == COMP_AC_VOLTAGE) {
                    comp->props.ac_voltage.r_series = value;
                    applied = true;
                }
            }
            break;

        case PROP_R_PARALLEL:
            if (comp->type == COMP_DC_CURRENT && value > 0 && value <= 1e12) {
                comp->props.dc_current.r_parallel = value;
                applied = true;
            }
            break;

        // Resistor temperature coefficient
        case PROP_TEMP_COEFF:
            if (comp->type == COMP_RESISTOR && value >= -10000 && value <= 100000) {
                comp->props.resistor.temp_coeff = value;
                applied = true;
            }
            break;

        // Capacitor ESR
        case PROP_ESR:
            if (value >= 0 && value <= 1e6) {
                if (comp->type == COMP_CAPACITOR) {
                    comp->props.capacitor.esr = value;
                    applied = true;
                } else if (comp->type == COMP_CAPACITOR_ELEC) {
                    comp->props.capacitor_elec.esr = value;
                    applied = true;
                }
            }
            break;

        // Inductor DCR
        case PROP_DCR:
            if (comp->type == COMP_INDUCTOR && value >= 0 && value <= 1e6) {
                comp->props.inductor.dcr = value;
                applied = true;
            }
            break;

        // Transformer winding resistances
        case PROP_TRANS_R_PRIMARY:
            if ((comp->type == COMP_TRANSFORMER || comp->type == COMP_TRANSFORMER_CT) && value >= 0 && value <= 1e6) {
                comp->props.transformer.r_primary = value;
                applied = true;
            }
            break;

        case PROP_TRANS_R_SECONDARY:
            if ((comp->type == COMP_TRANSFORMER || comp->type == COMP_TRANSFORMER_CT) && value >= 0 && value <= 1e6) {
                comp->props.transformer.r_secondary = value;
                applied = true;
            }
            break;

        // Diode breakdown voltage
        case PROP_BV:
            if (comp->type == COMP_DIODE && value > 0 && value <= 10000) {
                comp->props.diode.bv = value;
                applied = true;
            }
            break;

        // Zener parameters
        case PROP_VZ:
            if (comp->type == COMP_ZENER && value > 0 && value <= 1000) {
                comp->props.zener.vz = value;
                applied = true;
            }
            break;

        case PROP_RZ:
            if (comp->type == COMP_ZENER && value >= 0 && value <= 1e6) {
                comp->props.zener.rz = value;
                applied = true;
            }
            break;

        // Electrolytic capacitor max voltage
        case PROP_MAX_VOLTAGE:
            if (comp->type == COMP_CAPACITOR_ELEC && value > 0 && value <= 10000) {
                comp->props.capacitor_elec.max_voltage = value;
                applied = true;
            }
            break;

        // Op-Amp parameters
        case PROP_OPAMP_GAIN:
            if ((comp->type == COMP_OPAMP || comp->type == COMP_OPAMP_FLIPPED) && value > 0 && value <= 1e12) {
                comp->props.opamp.gain = value;
                applied = true;
            }
            break;

        case PROP_OPAMP_GBW:
            if ((comp->type == COMP_OPAMP || comp->type == COMP_OPAMP_FLIPPED) && value > 0 && value <= 1e12) {
                comp->props.opamp.gbw = value;
                applied = true;
            }
            break;

        case PROP_OPAMP_SLEW:
            if ((comp->type == COMP_OPAMP || comp->type == COMP_OPAMP_FLIPPED) && value > 0 && value <= 1e6) {
                comp->props.opamp.slew_rate = value;
                applied = true;
            }
            break;

        case PROP_OPAMP_VMAX:
            if ((comp->type == COMP_OPAMP || comp->type == COMP_OPAMP_FLIPPED) && value >= -1000 && value <= 1000) {
                comp->props.opamp.vmax = value;
                applied = true;
            }
            break;

        case PROP_OPAMP_VMIN:
            if ((comp->type == COMP_OPAMP || comp->type == COMP_OPAMP_FLIPPED) && value >= -1000 && value <= 1000) {
                comp->props.opamp.vmin = value;
                applied = true;
            }
            break;

        // Sweep value properties
        case PROP_SWEEP_VOLTAGE_START:
        case PROP_SWEEP_VOLTAGE_END:
        case PROP_SWEEP_VOLTAGE_TIME:
        case PROP_SWEEP_VOLTAGE_STEPS:
        case PROP_SWEEP_AMP_START:
        case PROP_SWEEP_AMP_END:
        case PROP_SWEEP_AMP_TIME:
        case PROP_SWEEP_AMP_STEPS:
        case PROP_SWEEP_FREQ_START:
        case PROP_SWEEP_FREQ_END:
        case PROP_SWEEP_FREQ_TIME:
        case PROP_SWEEP_FREQ_STEPS: {
            SweepConfig *sweep = NULL;
            int base_prop = 0;
            PropertyType prop = input->editing_prop_type;

            // Find the right sweep config based on property type and component
            if (prop >= PROP_SWEEP_VOLTAGE_START && prop <= PROP_SWEEP_VOLTAGE_STEPS) {
                base_prop = PROP_SWEEP_VOLTAGE_START;
                if (comp->type == COMP_DC_VOLTAGE) sweep = &comp->props.dc_voltage.voltage_sweep;
                else if (comp->type == COMP_DC_CURRENT) sweep = &comp->props.dc_current.current_sweep;
            } else if (prop >= PROP_SWEEP_AMP_START && prop <= PROP_SWEEP_AMP_STEPS) {
                base_prop = PROP_SWEEP_AMP_START;
                if (comp->type == COMP_AC_VOLTAGE) sweep = &comp->props.ac_voltage.amplitude_sweep;
                else if (comp->type == COMP_SQUARE_WAVE) sweep = &comp->props.square_wave.amplitude_sweep;
                else if (comp->type == COMP_TRIANGLE_WAVE) sweep = &comp->props.triangle_wave.amplitude_sweep;
                else if (comp->type == COMP_SAWTOOTH_WAVE) sweep = &comp->props.sawtooth_wave.amplitude_sweep;
                else if (comp->type == COMP_NOISE_SOURCE) sweep = &comp->props.noise_source.amplitude_sweep;
            } else if (prop >= PROP_SWEEP_FREQ_START && prop <= PROP_SWEEP_FREQ_STEPS) {
                base_prop = PROP_SWEEP_FREQ_START;
                if (comp->type == COMP_AC_VOLTAGE) sweep = &comp->props.ac_voltage.frequency_sweep;
                else if (comp->type == COMP_SQUARE_WAVE) sweep = &comp->props.square_wave.frequency_sweep;
                else if (comp->type == COMP_TRIANGLE_WAVE) sweep = &comp->props.triangle_wave.frequency_sweep;
                else if (comp->type == COMP_SAWTOOTH_WAVE) sweep = &comp->props.sawtooth_wave.frequency_sweep;
            }

            if (sweep) {
                int offset = prop - base_prop;
                switch (offset) {
                    case 0: // START
                        sweep->start_value = value;
                        applied = true;
                        break;
                    case 1: // END
                        sweep->end_value = value;
                        applied = true;
                        break;
                    case 2: // TIME
                        if (value > 0) {
                            sweep->sweep_time = value;
                            applied = true;
                        }
                        break;
                    case 3: // STEPS
                        if (value >= 2 && value <= 1000) {
                            sweep->num_steps = (int)value;
                            applied = true;
                        }
                        break;
                }
            }
            break;
        }

        // Also handle Schottky Vf using LED_VF property type
        // (already handled in PROP_LED_VF - but we need schottky handling)

        case PROP_TEXT_CONTENT:
            if (comp->type == COMP_TEXT) {
                // Copy text content (no numeric parsing needed)
                strncpy(comp->props.text.text, input->input_buffer, sizeof(comp->props.text.text) - 1);
                comp->props.text.text[sizeof(comp->props.text.text) - 1] = '\0';
                applied = true;
            }
            break;

        case PROP_TEXT_SIZE:
            // Cycle through font sizes: 1 -> 2 -> 3 -> 1
            if (comp->type == COMP_TEXT) {
                comp->props.text.font_size = (comp->props.text.font_size % 3) + 1;
                applied = true;
            }
            break;

        case PROP_TEXT_BOLD:
            // Toggle bold
            if (comp->type == COMP_TEXT) {
                comp->props.text.bold = !comp->props.text.bold;
                applied = true;
            }
            break;

        case PROP_TEXT_ITALIC:
            // Toggle italic
            if (comp->type == COMP_TEXT) {
                comp->props.text.italic = !comp->props.text.italic;
                applied = true;
            }
            break;

        case PROP_TEXT_UNDERLINE:
            // Toggle underline
            if (comp->type == COMP_TEXT) {
                comp->props.text.underline = !comp->props.text.underline;
                applied = true;
            }
            break;

        default:
            // Handle special cases for reused property types
            if (input->editing_prop_type == PROP_LED_VF && comp->type == COMP_SCHOTTKY) {
                if (value > 0 && value <= 10) {
                    comp->props.schottky.vf = value;
                    applied = true;
                }
            }
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
            // Character input is handled by SDL_TEXTINPUT event
            // Only SDL_TEXTINPUT should add characters to avoid double-input
            break;
    }
}
