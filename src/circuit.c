/**
 * Circuit Playground - Circuit Container Implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "circuit.h"

static int next_node_id = 1;
static int next_wire_id = 1;

Circuit *circuit_create(void) {
    Circuit *circuit = calloc(1, sizeof(Circuit));
    if (!circuit) return NULL;

    circuit->next_component_id = 1;
    circuit->next_node_id = 1;
    circuit->next_wire_id = 1;
    circuit->ground_node_id = 0;
    circuit->clipboard_offset_x = 20;
    circuit->clipboard_offset_y = 20;

    return circuit;
}

void circuit_free(Circuit *circuit) {
    if (!circuit) return;

    // Free all components
    for (int i = 0; i < circuit->num_components; i++) {
        component_free(circuit->components[i]);
    }

    // Free clipboard
    if (circuit->clipboard) {
        component_free(circuit->clipboard);
    }

    free(circuit);
}

void circuit_clear(Circuit *circuit) {
    if (!circuit) return;

    // Free all components
    for (int i = 0; i < circuit->num_components; i++) {
        component_free(circuit->components[i]);
        circuit->components[i] = NULL;
    }
    circuit->num_components = 0;

    // Clear nodes - zero out array to prevent stale data
    memset(circuit->nodes, 0, sizeof(circuit->nodes));
    circuit->num_nodes = 0;
    circuit->ground_node_id = 0;

    // Clear wires - zero out array
    memset(circuit->wires, 0, sizeof(circuit->wires));
    circuit->num_wires = 0;

    // Clear probes - zero out array
    memset(circuit->probes, 0, sizeof(circuit->probes));
    circuit->num_probes = 0;

    // Clear node map
    memset(circuit->node_map, 0, sizeof(circuit->node_map));
    circuit->num_matrix_nodes = 0;

    // Clear undo stack
    circuit_clear_undo(circuit);

    // Reset IDs
    circuit->next_component_id = 1;
    circuit->next_node_id = 1;
    circuit->next_wire_id = 1;

    circuit->modified = true;
}

int circuit_add_component(Circuit *circuit, Component *comp) {
    if (!circuit || !comp) return -1;
    if (circuit->num_components >= MAX_COMPONENTS) return -1;

    comp->id = circuit->next_component_id++;
    circuit->components[circuit->num_components++] = comp;

    // Create nodes for component terminals
    for (int i = 0; i < comp->num_terminals; i++) {
        float tx, ty;
        component_get_terminal_pos(comp, i, &tx, &ty);

        int node_id = circuit_find_or_create_node(circuit, tx, ty, 10);
        comp->node_ids[i] = node_id;
    }

    circuit->modified = true;
    return comp->id;
}

void circuit_remove_component(Circuit *circuit, int comp_id) {
    if (!circuit) return;

    for (int i = 0; i < circuit->num_components; i++) {
        if (circuit->components[i]->id == comp_id) {
            component_free(circuit->components[i]);

            // Shift remaining components
            for (int j = i; j < circuit->num_components - 1; j++) {
                circuit->components[j] = circuit->components[j + 1];
            }
            circuit->num_components--;
            circuit->components[circuit->num_components] = NULL;
            circuit->modified = true;

            // Clean up orphaned nodes
            circuit_cleanup_orphaned_nodes(circuit);
            return;
        }
    }
}

Component *circuit_get_component(Circuit *circuit, int comp_id) {
    if (!circuit) return NULL;

    for (int i = 0; i < circuit->num_components; i++) {
        if (circuit->components[i]->id == comp_id) {
            return circuit->components[i];
        }
    }
    return NULL;
}

Component *circuit_find_component_at(Circuit *circuit, float x, float y) {
    if (!circuit) return NULL;

    // Search in reverse order (top-most first)
    for (int i = circuit->num_components - 1; i >= 0; i--) {
        if (component_contains_point(circuit->components[i], x, y)) {
            return circuit->components[i];
        }
    }
    return NULL;
}

int circuit_create_node(Circuit *circuit, float x, float y) {
    if (!circuit || circuit->num_nodes >= MAX_NODES) return -1;

    Node *node = &circuit->nodes[circuit->num_nodes++];
    node->id = circuit->next_node_id++;
    node->x = x;
    node->y = y;
    node->voltage = 0;
    node->is_ground = false;
    node->connection_count = 0;

    return node->id;
}

Node *circuit_get_node(Circuit *circuit, int node_id) {
    if (!circuit) return NULL;

    for (int i = 0; i < circuit->num_nodes; i++) {
        if (circuit->nodes[i].id == node_id) {
            return &circuit->nodes[i];
        }
    }
    return NULL;
}

Node *circuit_find_node_at(Circuit *circuit, float x, float y, float threshold) {
    if (!circuit) return NULL;

    for (int i = 0; i < circuit->num_nodes; i++) {
        float dx = circuit->nodes[i].x - x;
        float dy = circuit->nodes[i].y - y;
        if (sqrt(dx*dx + dy*dy) <= threshold) {
            return &circuit->nodes[i];
        }
    }
    return NULL;
}

int circuit_find_or_create_node(Circuit *circuit, float x, float y, float threshold) {
    Node *node = circuit_find_node_at(circuit, x, y, threshold);
    if (node) return node->id;
    return circuit_create_node(circuit, x, y);
}

void circuit_set_ground(Circuit *circuit, int node_id) {
    if (!circuit) return;

    // Clear previous ground
    for (int i = 0; i < circuit->num_nodes; i++) {
        circuit->nodes[i].is_ground = false;
    }

    Node *node = circuit_get_node(circuit, node_id);
    if (node) {
        node->is_ground = true;
        circuit->ground_node_id = node_id;
    }
}

int circuit_add_wire(Circuit *circuit, int start_node_id, int end_node_id) {
    if (!circuit || circuit->num_wires >= MAX_WIRES) return -1;
    if (start_node_id == end_node_id) return -1;

    Wire *wire = &circuit->wires[circuit->num_wires++];
    wire->id = circuit->next_wire_id++;
    wire->start_node_id = start_node_id;
    wire->end_node_id = end_node_id;
    wire->num_points = 0;
    wire->selected = false;
    wire->current = 0;

    circuit->modified = true;
    return wire->id;
}

void circuit_remove_wire(Circuit *circuit, int wire_id) {
    if (!circuit) return;

    for (int i = 0; i < circuit->num_wires; i++) {
        if (circuit->wires[i].id == wire_id) {
            // Shift remaining wires
            for (int j = i; j < circuit->num_wires - 1; j++) {
                circuit->wires[j] = circuit->wires[j + 1];
            }
            circuit->num_wires--;

            // Zero out the last slot
            memset(&circuit->wires[circuit->num_wires], 0, sizeof(Wire));
            circuit->modified = true;

            // Clean up orphaned nodes
            circuit_cleanup_orphaned_nodes(circuit);
            return;
        }
    }
}

Wire *circuit_find_wire_at(Circuit *circuit, float x, float y, float threshold) {
    if (!circuit) return NULL;

    for (int i = 0; i < circuit->num_wires; i++) {
        Wire *wire = &circuit->wires[i];
        Node *start = circuit_get_node(circuit, wire->start_node_id);
        Node *end = circuit_get_node(circuit, wire->end_node_id);

        if (!start || !end) continue;

        // Check distance to wire segment
        float dx = end->x - start->x;
        float dy = end->y - start->y;
        float len_sq = dx*dx + dy*dy;

        if (len_sq == 0) continue;

        float t = ((x - start->x) * dx + (y - start->y) * dy) / len_sq;
        t = CLAMP(t, 0, 1);

        float proj_x = start->x + t * dx;
        float proj_y = start->y + t * dy;

        float dist = sqrt((x - proj_x)*(x - proj_x) + (y - proj_y)*(y - proj_y));
        if (dist <= threshold) {
            return wire;
        }
    }
    return NULL;
}

// Split a wire at a given point, creating a new node and two new wires
// Returns the new node ID, or -1 on failure
int circuit_split_wire_at(Circuit *circuit, Wire *wire, float x, float y) {
    if (!circuit || !wire) return -1;

    Node *start = circuit_get_node(circuit, wire->start_node_id);
    Node *end = circuit_get_node(circuit, wire->end_node_id);
    if (!start || !end) return -1;

    // Calculate the closest point on the wire to (x, y)
    float dx = end->x - start->x;
    float dy = end->y - start->y;
    float len_sq = dx*dx + dy*dy;

    if (len_sq == 0) return -1;

    float t = ((x - start->x) * dx + (y - start->y) * dy) / len_sq;
    t = CLAMP(t, 0.05f, 0.95f);  // Don't split too close to endpoints

    float split_x = start->x + t * dx;
    float split_y = start->y + t * dy;

    // Store original wire info before removing
    int orig_start_id = wire->start_node_id;
    int orig_end_id = wire->end_node_id;
    int wire_id = wire->id;

    // Create new node at split point
    int new_node_id = circuit_create_node(circuit, split_x, split_y);
    if (new_node_id < 0) return -1;

    // Remove original wire
    circuit_remove_wire(circuit, wire_id);

    // Create two new wires
    circuit_add_wire(circuit, orig_start_id, new_node_id);
    circuit_add_wire(circuit, new_node_id, orig_end_id);

    circuit->modified = true;

    return new_node_id;
}

// Check if a node is connected to any component or wire
static bool is_node_connected(Circuit *circuit, int node_id) {
    // Check components
    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        for (int j = 0; j < comp->num_terminals; j++) {
            if (comp->node_ids[j] == node_id) {
                return true;
            }
        }
    }

    // Check wires
    for (int i = 0; i < circuit->num_wires; i++) {
        if (circuit->wires[i].start_node_id == node_id ||
            circuit->wires[i].end_node_id == node_id) {
            return true;
        }
    }

    // Check probes
    for (int i = 0; i < circuit->num_probes; i++) {
        if (circuit->probes[i].node_id == node_id) {
            return true;
        }
    }

    return false;
}

// Remove a node by index and update all references
static void remove_node_by_index(Circuit *circuit, int index) {
    if (index < 0 || index >= circuit->num_nodes) return;

    int removed_id = circuit->nodes[index].id;

    // Clear ground if this was the ground node
    if (circuit->ground_node_id == removed_id) {
        circuit->ground_node_id = 0;
    }

    // Shift remaining nodes
    for (int i = index; i < circuit->num_nodes - 1; i++) {
        circuit->nodes[i] = circuit->nodes[i + 1];
    }
    circuit->num_nodes--;

    // Zero out the last slot
    memset(&circuit->nodes[circuit->num_nodes], 0, sizeof(Node));
}

// Clean up nodes that are no longer connected to anything
void circuit_cleanup_orphaned_nodes(Circuit *circuit) {
    if (!circuit) return;

    // Iterate in reverse to safely remove nodes
    for (int i = circuit->num_nodes - 1; i >= 0; i--) {
        int node_id = circuit->nodes[i].id;
        if (!is_node_connected(circuit, node_id)) {
            remove_node_by_index(circuit, i);
        }
    }
}

int circuit_add_probe(Circuit *circuit, int node_id, float x, float y) {
    if (!circuit || circuit->num_probes >= MAX_PROBES) return -1;

    static const Color probe_colors[] = {
        {0xff, 0xff, 0x00, 0xff},  // Yellow (CH1)
        {0x00, 0xff, 0xff, 0xff},  // Cyan (CH2)
        {0xff, 0x00, 0xff, 0xff},  // Magenta (CH3)
        {0x00, 0xff, 0x00, 0xff},  // Green (CH4)
        {0xff, 0x80, 0x00, 0xff},  // Orange (CH5)
        {0x80, 0x80, 0xff, 0xff},  // Light Blue (CH6)
        {0xff, 0x80, 0x80, 0xff},  // Pink (CH7)
        {0x80, 0xff, 0x80, 0xff},  // Light Green (CH8)
    };

    int idx = circuit->num_probes;
    Probe *probe = &circuit->probes[idx];
    probe->id = idx + 1;
    probe->node_id = node_id;
    probe->x = x;
    probe->y = y;
    probe->color = probe_colors[idx % 8];
    probe->voltage = 0;
    probe->channel_num = idx;
    snprintf(probe->label, sizeof(probe->label), "CH%d", idx + 1);

    circuit->num_probes++;
    return probe->id;
}

void circuit_remove_probe(Circuit *circuit, int probe_id) {
    if (!circuit) return;

    for (int i = 0; i < circuit->num_probes; i++) {
        if (circuit->probes[i].id == probe_id) {
            for (int j = i; j < circuit->num_probes - 1; j++) {
                circuit->probes[j] = circuit->probes[j + 1];
            }
            circuit->num_probes--;
            return;
        }
    }
}

// Union-Find helpers for building node map
static int uf_find(int *parent, int x) {
    if (parent[x] != x) {
        parent[x] = uf_find(parent, parent[x]);
    }
    return parent[x];
}

static void uf_union(int *parent, int x, int y) {
    int px = uf_find(parent, x);
    int py = uf_find(parent, y);
    if (px != py) {
        parent[px] = py;
    }
}

void circuit_build_node_map(Circuit *circuit) {
    if (!circuit) return;

    // Initialize union-find
    int parent[MAX_NODES];
    for (int i = 0; i < MAX_NODES; i++) {
        parent[i] = i;
    }

    // Union nodes connected by wires
    for (int i = 0; i < circuit->num_wires; i++) {
        Wire *wire = &circuit->wires[i];
        uf_union(parent, wire->start_node_id, wire->end_node_id);
    }

    // Build node index map
    memset(circuit->node_map, 0, sizeof(circuit->node_map));
    int next_idx = 1;  // 0 is reserved for ground

    // First, assign ground
    for (int i = 0; i < circuit->num_nodes; i++) {
        Node *node = &circuit->nodes[i];
        int root = uf_find(parent, node->id);
        Node *root_node = circuit_get_node(circuit, root);

        if (root_node && root_node->is_ground) {
            if (circuit->node_map[root] == 0) {
                circuit->node_map[root] = 0;  // Ground is index 0
            }
        }
    }

    // Then assign other nodes
    for (int i = 0; i < circuit->num_nodes; i++) {
        Node *node = &circuit->nodes[i];
        int root = uf_find(parent, node->id);

        if (circuit->node_map[root] == 0) {
            Node *root_node = circuit_get_node(circuit, root);
            if (!root_node || !root_node->is_ground) {
                circuit->node_map[root] = next_idx++;
            }
        }
    }

    // Map all nodes to their root's index
    for (int i = 0; i < circuit->num_nodes; i++) {
        int node_id = circuit->nodes[i].id;
        int root = uf_find(parent, node_id);
        circuit->node_map[node_id] = circuit->node_map[root];
    }

    circuit->num_matrix_nodes = next_idx - 1;
}

void circuit_update_voltages(Circuit *circuit, Vector *solution) {
    if (!circuit || !solution) return;

    for (int i = 0; i < circuit->num_nodes; i++) {
        Node *node = &circuit->nodes[i];
        int idx = circuit->node_map[node->id];

        if (idx == 0) {
            node->voltage = 0;
        } else if (idx > 0 && idx <= solution->size) {
            node->voltage = vector_get(solution, idx - 1);
        }
    }

    // Update probes
    for (int i = 0; i < circuit->num_probes; i++) {
        Node *node = circuit_get_node(circuit, circuit->probes[i].node_id);
        circuit->probes[i].voltage = node ? node->voltage : 0;
    }

    // Calculate power dissipation for components
    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        if (comp->type == COMP_RESISTOR && comp->num_terminals >= 2) {
            Node *n1 = circuit_get_node(circuit, comp->node_ids[0]);
            Node *n2 = circuit_get_node(circuit, comp->node_ids[1]);
            if (n1 && n2) {
                double v_diff = n1->voltage - n2->voltage;
                double R = comp->props.resistor.resistance;
                // P = V^2 / R
                comp->props.resistor.power_dissipated = (v_diff * v_diff) / R;
            }
        }
    }
}

// Helper: Calculate current through a 2-terminal component based on type and voltages
static double calculate_component_current(Component *comp, double v1, double v2) {
    double v_diff = v1 - v2;

    switch (comp->type) {
        case COMP_RESISTOR:
            if (comp->props.resistor.resistance > 0) {
                return v_diff / comp->props.resistor.resistance;
            }
            break;

        case COMP_DC_VOLTAGE:
            // Current through voltage source: use internal resistance
            if (comp->props.dc_voltage.r_series > 0) {
                return (v_diff - comp->props.dc_voltage.voltage) / comp->props.dc_voltage.r_series;
            }
            // For ideal source, estimate based on typical load
            return v_diff / 1000.0;  // Assume 1k load for visualization

        case COMP_AC_VOLTAGE:
            if (comp->props.ac_voltage.r_series > 0) {
                return v_diff / comp->props.ac_voltage.r_series;
            }
            return v_diff / 1000.0;

        case COMP_DC_CURRENT:
            // Current source: current is specified
            return comp->props.dc_current.current;

        case COMP_DIODE:
        case COMP_LED:
        case COMP_ZENER:
        case COMP_SCHOTTKY: {
            // Diode current: exponential model approximation
            // For visualization, use simplified I = (V - Vf) / Rd where Rd ~ 10-100 ohms
            double vf = (comp->type == COMP_LED) ? comp->props.led.vf : 0.7;
            if (v_diff > vf) {
                return (v_diff - vf) / 50.0;  // ~50 ohm dynamic resistance
            } else if (v_diff < -5.0 && comp->type == COMP_ZENER) {
                return (v_diff + comp->props.zener.vz) / comp->props.zener.rz;
            }
            return 0;
        }

        case COMP_CAPACITOR:
            // Capacitor: for DC, very small current (leakage)
            // For animation purposes, show small current proportional to voltage
            return v_diff * 1e-6;  // Small leakage current for visualization

        case COMP_INDUCTOR:
            // Inductor: current from stored state or estimate from DCR
            if (comp->props.inductor.dcr > 0) {
                return v_diff / comp->props.inductor.dcr;
            }
            return comp->props.inductor.current;

        case COMP_SPST_SWITCH:
            if (comp->props.switch_spst.closed) {
                return v_diff / comp->props.switch_spst.r_on;
            }
            return 0;

        case COMP_PUSH_BUTTON:
            if (comp->props.push_button.pressed) {
                return v_diff / comp->props.push_button.r_on;
            }
            return 0;

        default:
            // For unknown components, estimate based on voltage difference
            if (fabs(v_diff) > 0.001) {
                return v_diff / 1000.0;  // Assume 1k equivalent resistance
            }
            break;
    }
    return 0;
}

void circuit_update_wire_currents(Circuit *circuit) {
    if (!circuit) return;

    // For each wire, find connected components and calculate current
    for (int w = 0; w < circuit->num_wires; w++) {
        Wire *wire = &circuit->wires[w];
        double total_current = 0;
        int current_count = 0;

        // Get wire endpoint nodes
        Node *start_node = circuit_get_node(circuit, wire->start_node_id);
        Node *end_node = circuit_get_node(circuit, wire->end_node_id);
        if (!start_node || !end_node) {
            wire->current = 0;
            continue;
        }

        // Find components connected to start node
        for (int c = 0; c < circuit->num_components; c++) {
            Component *comp = circuit->components[c];
            if (!comp || comp->num_terminals < 2) continue;

            // Check if this component connects to the wire's start node
            for (int t = 0; t < comp->num_terminals; t++) {
                if (comp->node_ids[t] == wire->start_node_id) {
                    // Found a component connected to start node
                    // Get the voltage at the other terminal(s)
                    for (int t2 = 0; t2 < comp->num_terminals; t2++) {
                        if (t2 != t) {
                            Node *other_node = circuit_get_node(circuit, comp->node_ids[t2]);
                            if (other_node) {
                                double current = calculate_component_current(comp,
                                    start_node->voltage, other_node->voltage);
                                // Current flowing INTO the start node (toward the wire)
                                total_current += current;
                                current_count++;
                            }
                        }
                    }
                    break;
                }
            }
        }

        // If we found component currents, use the average magnitude
        if (current_count > 0) {
            wire->current = total_current / current_count;
        } else {
            // Fallback: no components found, use voltage difference estimation
            // This shouldn't happen in a properly connected circuit
            wire->current = 0;
        }
    }
}

void circuit_update_component_nodes(Circuit *circuit, Component *comp) {
    if (!circuit || !comp) return;

    for (int i = 0; i < comp->num_terminals; i++) {
        float tx, ty;
        component_get_terminal_pos(comp, i, &tx, &ty);

        Node *node = circuit_get_node(circuit, comp->node_ids[i]);
        if (node) {
            node->x = tx;
            node->y = ty;
        }
    }
}

void circuit_copy_component(Circuit *circuit, Component *comp) {
    if (!circuit || !comp) return;

    if (circuit->clipboard) {
        component_free(circuit->clipboard);
    }

    circuit->clipboard = component_clone(comp);
}

void circuit_cut_component(Circuit *circuit, Component *comp) {
    if (!circuit || !comp) return;

    circuit_copy_component(circuit, comp);
    circuit_remove_component(circuit, comp->id);
}

Component *circuit_paste_component(Circuit *circuit, float x, float y) {
    if (!circuit || !circuit->clipboard) return NULL;

    Component *pasted = component_clone(circuit->clipboard);
    if (!pasted) return NULL;

    pasted->x = x;
    pasted->y = y;

    circuit_add_component(circuit, pasted);
    return pasted;
}

Component *circuit_duplicate_component(Circuit *circuit, Component *comp) {
    if (!circuit || !comp) return NULL;

    Component *dup = component_clone(comp);
    if (!dup) return NULL;

    dup->x = comp->x + circuit->clipboard_offset_x;
    dup->y = comp->y + circuit->clipboard_offset_y;

    circuit_add_component(circuit, dup);
    return dup;
}

void circuit_select_all(Circuit *circuit) {
    if (!circuit) return;

    for (int i = 0; i < circuit->num_components; i++) {
        circuit->components[i]->selected = true;
    }
}

void circuit_deselect_all(Circuit *circuit) {
    if (!circuit) return;

    for (int i = 0; i < circuit->num_components; i++) {
        circuit->components[i]->selected = false;
    }
}

void circuit_delete_selected(Circuit *circuit) {
    if (!circuit) return;

    // Delete in reverse to avoid index issues
    for (int i = circuit->num_components - 1; i >= 0; i--) {
        if (circuit->components[i]->selected) {
            circuit_remove_component(circuit, circuit->components[i]->id);
        }
    }
}

// Undo operations
void circuit_push_undo(Circuit *circuit, UndoActionType type, int id, Component *backup, float old_x, float old_y) {
    if (!circuit) return;

    // Shift stack if full
    if (circuit->undo_count >= MAX_UNDO) {
        // Free the oldest backup if it exists
        if (circuit->undo_stack[0].component_backup) {
            component_free(circuit->undo_stack[0].component_backup);
        }
        // Shift everything down
        for (int i = 0; i < MAX_UNDO - 1; i++) {
            circuit->undo_stack[i] = circuit->undo_stack[i + 1];
        }
        circuit->undo_count = MAX_UNDO - 1;
    }

    UndoAction *action = &circuit->undo_stack[circuit->undo_count++];
    action->type = type;
    action->id = id;
    action->component_backup = backup;
    action->old_x = old_x;
    action->old_y = old_y;
}

bool circuit_undo(Circuit *circuit) {
    if (!circuit || circuit->undo_count == 0) return false;

    UndoAction *action = &circuit->undo_stack[--circuit->undo_count];

    switch (action->type) {
        case UNDO_ADD_COMPONENT:
            // Remove the component that was added
            circuit_remove_component(circuit, action->id);
            // Don't push this removal to undo stack
            if (circuit->undo_count > 0) {
                // Check if we just pushed an undo for this removal and remove it
                UndoAction *last = &circuit->undo_stack[circuit->undo_count - 1];
                if (last->type == UNDO_REMOVE_COMPONENT && last->id == action->id) {
                    if (last->component_backup) {
                        component_free(last->component_backup);
                    }
                    circuit->undo_count--;
                }
            }
            break;

        case UNDO_REMOVE_COMPONENT:
            // Re-add the component that was removed
            if (action->component_backup) {
                action->component_backup->id = action->id;
                circuit->components[circuit->num_components++] = action->component_backup;
                action->component_backup = NULL;  // Don't free it
            }
            break;

        case UNDO_MOVE_COMPONENT:
            // Move component back to old position
            for (int i = 0; i < circuit->num_components; i++) {
                if (circuit->components[i]->id == action->id) {
                    circuit->components[i]->x = action->old_x;
                    circuit->components[i]->y = action->old_y;
                    circuit_update_component_nodes(circuit, circuit->components[i]);
                    break;
                }
            }
            break;

        case UNDO_ADD_WIRE:
            circuit_remove_wire(circuit, action->id);
            break;

        case UNDO_REMOVE_WIRE:
            circuit_add_wire(circuit, action->wire_start, action->wire_end);
            break;
    }

    // Clean up backup if not used
    if (action->component_backup) {
        component_free(action->component_backup);
        action->component_backup = NULL;
    }

    circuit->modified = true;
    return true;
}

void circuit_clear_undo(Circuit *circuit) {
    if (!circuit) return;

    for (int i = 0; i < circuit->undo_count; i++) {
        if (circuit->undo_stack[i].component_backup) {
            component_free(circuit->undo_stack[i].component_backup);
            circuit->undo_stack[i].component_backup = NULL;
        }
    }
    circuit->undo_count = 0;
}
