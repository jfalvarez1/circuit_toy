/**
 * Circuit Playground - Circuit Container Implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>       /* GetCurrentProcessId, for the undo snapshot filenames */
#else
#include <unistd.h>        /* getpid, likewise */
#endif
#include "circuit.h"
#include "matrix.h"
#include "component.h"  // For COMP_GROUND type
#include "file_io.h"       // a whole-canvas undo record is a file check

// Forward declarations
static double get_mapped_voltage(Circuit *circuit, int node_id);

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

static void undo_action_release(UndoAction *a);   /* defined below; used by circuit_free */

void circuit_free(Circuit *circuit) {
    if (!circuit) return;

    /* the undo and redo stacks own component copies and snapshot files */
    for (int i = 0; i < circuit->undo_count; i++) undo_action_release(&circuit->undo_stack[i]);
    for (int i = 0; i < circuit->redo_count; i++) undo_action_release(&circuit->redo_stack[i]);
    circuit->undo_count = circuit->redo_count = 0;

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

/* A snapshot record owns a file. Dropping the record without deleting it leaves the circuit
   behind in the temporary directory for good. */
static void undo_action_release(UndoAction *a) {
    if (!a) return;
    if (a->component_backup) { component_free(a->component_backup); a->component_backup = NULL; }
    if (a->type == UNDO_SNAPSHOT && a->snapshot[0]) { remove(a->snapshot); a->snapshot[0] = 0; }
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

    /* Clearing the canvas normally means starting again, and a stack of records about a
       circuit that no longer exists is worse than none. The exception is a clear that has just
       been recorded whole - there the stack is the only way back, and keep_undo says so. */
    if (!circuit->undo_preserving) {
        circuit_clear_undo(circuit);
        /* ...and with no records left to confuse, the NODE ids may start again.

           "A session does not run out of them" is true of component ids and false of these:
           node_map is indexed BY node id and holds MAX_NODES entries, as does the union-find
           parent[] in circuit_build_node_map, so an id past 2048 writes off the end of both. It
           is reachable - --place-test clears and refills one Circuit 188 times and segfaulted on
           the sixtieth template.

           Only when the undo stack goes with it. That is what made restarting them unsafe: a
           record naming a part by number could not tell the old canvas's part from the new one's.
           Nothing here survives the clear - components, nodes, wires and probes are all gone
           above - so with the stack cleared too there is nothing left holding an old id.
           Component ids still keep counting, which is what that argument was really about. */
        circuit->next_node_id = 1;
    }

    circuit->modified = true;
    circuit->topology_dirty = true;
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
    circuit->topology_dirty = true;
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
            circuit->topology_dirty = true;
    circuit->topology_dirty = true;

            // Clean up orphaned nodes - unless an undo may want to reconnect to them
            if (!circuit->undo_preserving) circuit_cleanup_orphaned_nodes(circuit);
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
    /* The id is what node_map and the union-find parent[] are indexed by, and both hold
       MAX_NODES. num_nodes being under the limit does not bound the id: ids only ever climb, so
       a long session reaches 2048 while holding forty nodes. Refusing here is not a limit anyone
       will meet without clearing the canvas once, and it is the difference between stopping and
       writing off the end of the Circuit struct. */
    if (circuit->next_node_id >= MAX_NODES) return -1;

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

/* The node nearest a remembered position, or -1. Restoring a wire wants the node that is there
   now, not the first one that happens to be within reach: a component restored a moment earlier
   put its node at its terminal, which can sit a few pixels from where the wire's node was, and
   the first-match rule would either miss it - leaving two nodes where there was one - or, with a
   wider reach, grab a neighbour and weld two separate nets together. */
static int node_nearest(Circuit *circuit, float x, float y, float max_dist) {
    int best = -1;
    float best_d2 = max_dist * max_dist;
    for (int i = 0; i < circuit->num_nodes; i++) {
        float dx = circuit->nodes[i].x - x, dy = circuit->nodes[i].y - y;
        float d2 = dx * dx + dy * dy;
        if (d2 <= best_d2) { best_d2 = d2; best = circuit->nodes[i].id; }
    }
    return best;
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
    circuit->topology_dirty = true;
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
            circuit->topology_dirty = true;
    circuit->topology_dirty = true;

            // Clean up orphaned nodes - unless an undo may want to reconnect to them
            if (!circuit->undo_preserving) circuit_cleanup_orphaned_nodes(circuit);
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

    /* Store the original wire's ENDS AS POSITIONS, not as ids, and make the junction after the
       old wire is gone rather than before.

       circuit_remove_wire sweeps orphaned nodes. The junction node used to be created first, and
       at that moment it had no wire on it and no terminal - the definition of an orphan - so
       removing the old wire swept away the node the two halves were about to be attached to, and
       both were then wired to an id that no longer named anything. Splitting a wire is an
       ordinary edit: it happens when a wire is drawn onto another one, and when one is clicked to
       put a junction in it.

       The two original ends can be swept by the same call for the same reason, if that wire was
       the only thing holding them, so they are found again by position afterwards. */
    float sx = start->x, sy = start->y, ex = end->x, ey = end->y;
    int wire_id = wire->id;

    circuit_remove_wire(circuit, wire_id);

    int orig_start_id = circuit_find_or_create_node(circuit, sx, sy, 5.0f);
    int orig_end_id   = circuit_find_or_create_node(circuit, ex, ey, 5.0f);
    int new_node_id   = circuit_create_node(circuit, split_x, split_y);
    if (new_node_id < 0 || orig_start_id < 0 || orig_end_id < 0) return -1;

    // Create two new wires
    circuit_add_wire(circuit, orig_start_id, new_node_id);
    circuit_add_wire(circuit, new_node_id, orig_end_id);

    circuit->modified = true;
    circuit->topology_dirty = true;

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
    snprintf(probe->label, sizeof(probe->label), "CH%d", idx + 1);   /* renameable: see probe_label_is_default */

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
            /* Renumber. circuit_add_probe assigns id = num_probes + 1, so ids are index + 1 - an
               invariant the rest of the code relies on and this function used to break. Remove the
               middle of three and the array holds ids 1 and 3; add another and it is given id 3
               as well, because num_probes is 2. Then removing "probe 3" removes whichever of the
               two comes first, which is the wrong one half the time.

               Only the id. The label is renameable and belongs to the probe, and channel_num and
               the colour travel with it. */
            for (int j = 0; j < circuit->num_probes; j++) circuit->probes[j].id = j + 1;
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

    // ROBUST FIX: First, union nodes at the same physical position
    // This ensures that component terminals placed at the same (x,y) are
    // automatically electrically connected, like in real EDA tools.
    // This prevents issues with parallel resistors and other configurations
    // where terminals overlap but may not have explicit wires between them.
    // NOTE: Tolerance must match circuit_find_or_create_node threshold (10)
    // to handle 90-degree wire turns where user clicks create multiple nodes
    // at nearly the same corner position.
    const float POSITION_TOLERANCE = 10.0f;  // Match node find/create threshold
    for (int i = 0; i < circuit->num_nodes; i++) {
        Node *ni = &circuit->nodes[i];
        for (int j = i + 1; j < circuit->num_nodes; j++) {
            Node *nj = &circuit->nodes[j];
            float dx = ni->x - nj->x;
            float dy = ni->y - nj->y;
            // If nodes are at the same position (within tolerance), merge them
            if (dx * dx + dy * dy <= POSITION_TOLERANCE * POSITION_TOLERANCE) {
                uf_union(parent, ni->id, nj->id);
            }
        }
    }

    // Then union nodes connected by explicit wires
    for (int i = 0; i < circuit->num_wires; i++) {
        Wire *wire = &circuit->wires[i];
        uf_union(parent, wire->start_node_id, wire->end_node_id);
    }

    /* And nodes that carry the same net name, wherever they are. This is the third way of
       joining two terminals, after standing at the same point and having a wire drawn between
       them, and it is the one a written-down circuit uses: a table lists a part as connected to
       "vm1" and never says where vm1 is. Names are compared exactly and case-insensitively, so
       VM1 and vm1 are the same net and vm10 is not. */
    for (int i = 0; i < circuit->num_nodes; i++) {
        if (!circuit->nodes[i].name[0]) continue;
        for (int j = i + 1; j < circuit->num_nodes; j++) {
            if (!circuit->nodes[j].name[0]) continue;
            if (_stricmp(circuit->nodes[i].name, circuit->nodes[j].name) == 0)
                uf_union(parent, circuit->nodes[i].id, circuit->nodes[j].id);
        }
    }

    // CRITICAL FIX: Union ALL ground component terminals together
    // In real circuits, all ground symbols are electrically connected.
    // This ensures that separate GND symbols (GND2, GND3, etc.) all map
    // to the same node, which is essential for short circuit detection
    // when V+ connects to one ground and V- connects to another ground.
    int first_ground_node = -1;
    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        if (comp && comp->type == COMP_GROUND && comp->num_terminals > 0) {
            int ground_terminal_node = comp->node_ids[0];
            if (first_ground_node < 0) {
                first_ground_node = ground_terminal_node;
            } else {
                // Union this ground's terminal with the first ground's terminal
                uf_union(parent, first_ground_node, ground_terminal_node);
            }
        }
    }

    // Build node index map
    memset(circuit->node_map, 0, sizeof(circuit->node_map));
    int next_idx = 1;  // 0 is reserved for ground

    // Determine ground root - use first_ground_node if we found COMP_GROUND components,
    // otherwise fall back to the node marked with is_ground flag
    int ground_root = -1;
    if (first_ground_node >= 0) {
        ground_root = uf_find(parent, first_ground_node);
    } else {
        // Fallback: check for nodes marked with is_ground
        for (int i = 0; i < circuit->num_nodes; i++) {
            if (circuit->nodes[i].is_ground) {
                ground_root = uf_find(parent, circuit->nodes[i].id);
                break;
            }
        }
    }

    // Mark the ground root as index 0 (ground is always index 0)
    // This is already the default from memset, but we track it explicitly
    // to avoid assigning a non-zero index to the ground set

    // Assign indices to non-ground nodes
    for (int i = 0; i < circuit->num_nodes; i++) {
        Node *node = &circuit->nodes[i];
        int root = uf_find(parent, node->id);

        // Skip if this root is the ground root
        if (ground_root >= 0 && root == ground_root) {
            continue;  // node_map[root] stays 0 (ground)
        }

        // Assign new index if not already assigned
        if (circuit->node_map[root] == 0) {
            circuit->node_map[root] = next_idx++;
        }
    }

    // Map all nodes to their root's index
    for (int i = 0; i < circuit->num_nodes; i++) {
        int node_id = circuit->nodes[i].id;
        if (node_id < 0 || node_id >= MAX_NODES) continue;   /* see circuit_create_node */
        int root = uf_find(parent, node_id);
        if (root < 0 || root >= MAX_NODES) continue;
        circuit->node_map[node_id] = circuit->node_map[root];
    }

    circuit->num_matrix_nodes = next_idx - 1;
}

// Helper: Get voltage for a node_id using node_map for robust multi-instance support
// Note: Forward declaration for use in circuit_update_voltages
static double get_mapped_voltage_internal(Circuit *circuit, int node_id, Vector *solution) {
    if (!circuit || !solution || node_id < 0 || node_id >= MAX_NODES) return 0.0;

    int idx = circuit->node_map[node_id];
    if (idx == 0) return 0.0;  // Ground
    if (idx > 0 && idx <= solution->size) {
        return vector_get(solution, idx - 1);
    }
    return 0.0;
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

    // Calculate power dissipation for components and LED currents
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
        // Recalculate LED current from the final converged solution
        else if (comp->type == COMP_LED && comp->num_terminals >= 2) {
            Node *led_n1 = circuit_get_node(circuit, comp->node_ids[0]);
            Node *led_n2 = circuit_get_node(circuit, comp->node_ids[1]);
            double led_current = 0.0;

            if (led_n1 && led_n2) {
                // Evaluate the same Shockley model the solver stamped, at the converged
                // node voltages. (Reading the current off "the" series resistor is wrong
                // whenever more than one branch feeds the LED, e.g. wired-AND outputs.)
                double Vd = led_n1->voltage - led_n2->voltage;
                double Is = comp->props.led.is;
                double n = comp->props.led.n;
                double Vt = 8.617e-5 * (g_environment.temperature + 273.15);
                double nVt = n * Vt;

                if (Vd < -5 * nVt) Vd = -5 * nVt;
                if (Vd > 40 * nVt) Vd = 40 * nVt;

                double expTerm = exp(Vd / nVt);
                double Id = Is * (expTerm - 1);
                led_current = Id > 0 ? Id : 0;

                comp->props.led.current = led_current;
            }
        }
        // LED_ARRAY: Calculate currents from FINAL voltages (after MNA solve)
        else if (comp->type == COMP_LED_ARRAY && comp->num_terminals >= 9) {
            int com = 8;  // Common cathode terminal
            double Is = comp->props.led_array.is;
            double nn = comp->props.led_array.n;
            double Vt = 8.617e-5 * (g_environment.temperature + 273.15);
            double nVt = nn * Vt;

            for (int seg = 0; seg < 8; seg++) {
                if (comp->props.led_array.failed[seg]) {
                    comp->props.led_array.currents[seg] = 0;
                    continue;
                }

                // Get FINAL voltages from solution using the circuit's node voltage directly
                // The node voltage is already set in circuit_update_voltages() before we get here
                int anode_id = comp->node_ids[seg];
                int cathode_id = comp->node_ids[com];

                double v_anode = 0.0;
                double v_cathode = 0.0;

                // Direct node voltage lookup - nodes already have correct voltages
                Node *anode_node = circuit_get_node(circuit, anode_id);
                Node *cathode_node = circuit_get_node(circuit, cathode_id);

                if (anode_node) v_anode = anode_node->voltage;
                if (cathode_node) v_cathode = cathode_node->voltage;

                double Vd = v_anode - v_cathode;

                // Clamp voltage to prevent overflow
                if (Vd < -5.0 * nVt) Vd = -5.0 * nVt;
                if (Vd > 40.0 * nVt) Vd = 40.0 * nVt;

                // Shockley equation with final voltage
                if (Vd > 0) {
                    double expTerm = exp(Vd / nVt);
                    double Id = Is * (expTerm - 1.0);
                    comp->props.led_array.currents[seg] = (Id > 0) ? Id : 0;
                } else {
                    comp->props.led_array.currents[seg] = 0;
                }
            }
        }
    }
}

// Helper: Get voltage for a node_id using node_map for robust multi-instance support
static double get_mapped_voltage(Circuit *circuit, int node_id) {
    if (!circuit || node_id < 0 || node_id >= MAX_NODES) return 0.0;

    // First try direct lookup
    Node *node = circuit_get_node(circuit, node_id);
    if (node) return node->voltage;

    // Fallback: find voltage via node_map (for nodes connected via wires)
    int target_idx = circuit->node_map[node_id];
    if (target_idx == 0) return 0.0;  // Ground

    // Find any node with the same mapped index
    for (int i = 0; i < circuit->num_nodes; i++) {
        if (circuit->node_map[circuit->nodes[i].id] == target_idx) {
            return circuit->nodes[i].voltage;
        }
    }
    return 0.0;
}

// Extended precision version for ammeter calculations where tiny voltage drops matter
// long double provides ~18-19 decimal digits vs ~15-16 for double
static long double get_mapped_voltage_extended(Circuit *circuit, int node_id) {
    if (!circuit || node_id < 0 || node_id >= MAX_NODES) return 0.0L;

    // First try direct lookup
    Node *node = circuit_get_node(circuit, node_id);
    if (node) return (long double)node->voltage;

    // Fallback: find voltage via node_map (for nodes connected via wires)
    int target_idx = circuit->node_map[node_id];
    if (target_idx == 0) return 0.0L;  // Ground

    // Find any node with the same mapped index
    for (int i = 0; i < circuit->num_nodes; i++) {
        if (circuit->node_map[circuit->nodes[i].id] == target_idx) {
            return (long double)circuit->nodes[i].voltage;
        }
    }

    return 0.0L;
}

void circuit_update_meter_readings(Circuit *circuit) {
    if (!circuit) return;

    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        if (!comp) continue;

        if (comp->type == COMP_VOLTMETER) {
            // Voltmeter: read voltage difference using node_map for multi-instance support
            double v1 = get_mapped_voltage(circuit, comp->node_ids[0]);
            double v2 = get_mapped_voltage(circuit, comp->node_ids[1]);
            comp->props.voltmeter.reading = v1 - v2;
        }
        else if (comp->type == COMP_AMMETER) {
            // Ammeter: calculate current from voltage drop across shunt resistance
            // Use extended precision (long double) to handle tiny voltage drops
            // across very low ammeter resistance (1µΩ for ideal ammeter)
            long double v1 = get_mapped_voltage_extended(circuit, comp->node_ids[0]);
            long double v2 = get_mapped_voltage_extended(circuit, comp->node_ids[1]);

            // Use the same shunt resistance as in stamp function
            // Ideal ammeter uses 1uOhm (1e-6) to act as effective short circuit
            long double R = comp->props.ammeter.ideal ? 1e-6L : (long double)comp->props.ammeter.r_shunt;
            if (R < 1e-9L) R = 1e-9L;  // Minimum resistance for numerical stability

            // I = V_drop / R_shunt (all in extended precision)
            long double current = (v1 - v2) / R;
            comp->props.ammeter.reading = (double)current;
        }
        else if (comp->type == COMP_WATTMETER) {
            // Wattmeter: P = V * I
            // Voltage across V+ to V- (terminals 0, 1)
            double vp = get_mapped_voltage(circuit, comp->node_ids[0]);
            double vn = get_mapped_voltage(circuit, comp->node_ids[1]);
            double voltage = vp - vn;

            // Current through I+ to I- (terminals 2, 3)
            // Use extended precision for current measurement across low-R shunt
            long double vi1 = get_mapped_voltage_extended(circuit, comp->node_ids[2]);
            long double vi2 = get_mapped_voltage_extended(circuit, comp->node_ids[3]);
            long double R_i = 0.001L;  // 1mOhm shunt
            long double current = (vi1 - vi2) / R_i;

            comp->props.voltmeter.reading = voltage * (double)current;
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

// BFS-based current flow tracing from sources to ground
// This properly traces current direction through all wires in the path
void circuit_update_wire_currents(Circuit *circuit) {
    // Physical wire currents. Each electrical net (matrix node) is a small graph of circuit
    // nodes joined by zero-resistance wires. The component terminal currents (see
    // simulation_compute_terminal_currents) are the demands at the nodes; ground symbols
    // absorb whatever the net does not return. Wire flows are the minimum-norm solution of
    // the conservation equations (a unit-conductance Laplacian solve), which is exact for
    // tree-shaped nets (the usual case) and splits evenly across parallel wires.
    if (!circuit) return;
    for (int w = 0; w < circuit->num_wires; w++) circuit->wires[w].current = 0;
    if (circuit->num_nodes == 0) return;

    // Which nodes hold ground symbols?
    static unsigned char has_ground[MAX_NODES];
    memset(has_ground, 0, sizeof(has_ground));
    for (int c = 0; c < circuit->num_components; c++) {
        Component *comp = circuit->components[c];
        if (comp && comp->type == COMP_GROUND && comp->num_terminals >= 1) {
            int id = comp->node_ids[0];
            if (id >= 0 && id < MAX_NODES) has_ground[id] = 1;
        }
    }

    // Demand at every circuit node: current leaving the node into components
    static double demand[MAX_NODES];
    memset(demand, 0, sizeof(demand));
    for (int c = 0; c < circuit->num_components; c++) {
        Component *comp = circuit->components[c];
        if (!comp || comp->type == COMP_GROUND) continue;
        for (int t = 0; t < comp->num_terminals; t++) {
            int id = comp->node_ids[t];
            if (id >= 0 && id < MAX_NODES) demand[id] += comp->terminal_current[t];
        }
    }

    // Solve one connected wire island at a time. A net (matrix node) can consist of several
    // islands that are only joined through ground symbols; each island must balance on its own.
    static int local[MAX_NODES];
    static int island_of[MAX_NODES];
    for (int i = 0; i < MAX_NODES; i++) island_of[i] = -1;

    for (int start_i = 0; start_i < circuit->num_nodes; start_i++) {
        int seed = circuit->nodes[start_i].id;
        if (seed < 0 || seed >= MAX_NODES || island_of[seed] >= 0) continue;

        // BFS over wires
        int ids[MAX_NODES]; int k = 0, head = 0;
        ids[k++] = seed; island_of[seed] = start_i; local[seed] = 0;
        while (head < k) {
            int id = ids[head++];
            for (int w = 0; w < circuit->num_wires; w++) {
                Wire *wire = &circuit->wires[w];
                int other = -1;
                if (wire->start_node_id == id) other = wire->end_node_id;
                else if (wire->end_node_id == id) other = wire->start_node_id;
                if (other < 0 || other >= MAX_NODES || island_of[other] >= 0) continue;
                island_of[other] = start_i;
                local[other] = k;
                ids[k++] = other;
            }
        }
        if (k < 2) continue;

        // Island balance: whatever the components do not return goes into the island's ground
        // symbols; an island without ground (floating) spreads numerical residue evenly.
        double total = 0; int n_gnd = 0;
        for (int i = 0; i < k; i++) { total += demand[ids[i]]; if (has_ground[ids[i]]) n_gnd++; }
        for (int i = 0; i < k; i++) {
            if (n_gnd > 0) { if (has_ground[ids[i]]) demand[ids[i]] -= total / n_gnd; }
            else demand[ids[i]] -= total / k;
        }

        // Unit-conductance Laplacian with local node 0 as reference
        int n = k - 1;
        Matrix *L = matrix_create(n, n);
        Vector *rhs = vector_create(n);
        if (!L || !rhs) { matrix_free(L); vector_free(rhs); continue; }
        for (int w = 0; w < circuit->num_wires; w++) {
            Wire *wire = &circuit->wires[w];
            int a = wire->start_node_id, bn = wire->end_node_id;
            if (a < 0 || a >= MAX_NODES || bn < 0 || bn >= MAX_NODES) continue;
            if (island_of[a] != start_i || island_of[bn] != start_i) continue;
            int la = local[a], lb = local[bn];
            if (la > 0) matrix_add(L, la - 1, la - 1, 1.0);
            if (lb > 0) matrix_add(L, lb - 1, lb - 1, 1.0);
            if (la > 0 && lb > 0) { matrix_add(L, la - 1, lb - 1, -1.0); matrix_add(L, lb - 1, la - 1, -1.0); }
        }
        // Conservation: sum over wires of flow into node = demand  ->  L*phi = -demand
        for (int i = 1; i < k; i++) vector_set(rhs, i - 1, -demand[ids[i]]);
        Vector *phi = linear_solve(L, rhs);
        if (phi) {
            for (int w = 0; w < circuit->num_wires; w++) {
                Wire *wire = &circuit->wires[w];
                int a = wire->start_node_id, bn = wire->end_node_id;
                if (a < 0 || a >= MAX_NODES || bn < 0 || bn >= MAX_NODES) continue;
                if (island_of[a] != start_i || island_of[bn] != start_i) continue;
                double pa = (local[a] > 0) ? vector_get(phi, local[a] - 1) : 0.0;
                double pb = (local[bn] > 0) ? vector_get(phi, local[bn] - 1) : 0.0;
                wire->current = pa - pb;   // positive = start -> end
            }
            vector_free(phi);
        }
        matrix_free(L);
        vector_free(rhs);
    }
}

/* Give a component's terminals nodes at the positions they are actually at, the way adding one
   does. Restoring an undone delete needs this: the ids it was carrying were cleaned up with it,
   and ids get reused, so what they name now may belong to another part entirely. */
void circuit_reattach_component(Circuit *circuit, Component *comp) {
    if (!circuit || !comp) return;
    for (int i = 0; i < comp->num_terminals; i++) {
        float tx, ty;
        component_get_terminal_pos(comp, i, &tx, &ty);
        comp->node_ids[i] = circuit_find_or_create_node(circuit, tx, ty, 10);
    }
    circuit->topology_dirty = true;
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

// Helper to push action to redo stack
static void circuit_push_redo_internal(Circuit *circuit, UndoActionType type, int id, Component *backup, float old_x, float old_y, int wire_start, int wire_end) {
    if (!circuit) return;

    // Shift stack if full
    if (circuit->redo_count >= MAX_UNDO) {
        // Free the oldest backup if it exists
        undo_action_release(&circuit->redo_stack[0]);
        // Shift everything down
        for (int i = 0; i < MAX_UNDO - 1; i++) {
            circuit->redo_stack[i] = circuit->redo_stack[i + 1];
        }
        circuit->redo_count = MAX_UNDO - 1;
    }

    UndoAction *action = &circuit->redo_stack[circuit->redo_count++];
    action->type = type;
    action->id = id;
    action->component_backup = backup;
    action->old_x = old_x;
    action->old_y = old_y;
    action->wire_start = wire_start;
    action->wire_end = wire_end;
}

// Clear redo stack
void circuit_clear_redo(Circuit *circuit) {
    if (!circuit) return;

    for (int i = 0; i < circuit->redo_count; i++) undo_action_release(&circuit->redo_stack[i]);
    circuit->redo_count = 0;
}

// Undo/Redo operations
void circuit_push_undo(Circuit *circuit, UndoActionType type, int id, Component *backup, float old_x, float old_y) {
    if (!circuit) return;

    // Clear redo stack when a new action is pushed
    circuit_clear_redo(circuit);

    // Shift stack if full
    if (circuit->undo_count >= MAX_UNDO) {
        // Free the oldest backup if it exists
        undo_action_release(&circuit->undo_stack[0]);
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
    /* For a wire, the two coordinates are the nodes it joins - that is the convention the redo
       path already used, reading them back out of old_x and old_y. They were never copied into
       the fields undo reads, so a slot recycled from an older action carried whatever endpoints
       that one had. Nothing hit it, because nothing ever recorded a wire being deleted. */
    action->wire_start = (type == UNDO_ADD_WIRE || type == UNDO_REMOVE_WIRE) ? (int)old_x : 0;
    action->wire_end   = (type == UNDO_ADD_WIRE || type == UNDO_REMOVE_WIRE) ? (int)old_y : 0;
    action->batch = circuit->undo_batch_current;
}

/* Where a snapshot goes: beside the settings, in a file named for its place on the stack. They
   are small, there are at most a few, and they are cleaned up when the stack is. */
/* This process's own id, so two copies of the program do not write each other's undo files.
   The serial below starts at zero in every process, so a second window wrote circuit_undo_0.cpg
   over the first window's - and the first window's Ctrl+Z then restored the second window's
   circuit. Two windows is an ordinary thing to have open. */
static unsigned long snapshot_owner(void) {
#ifdef _WIN32
    return (unsigned long)GetCurrentProcessId();
#else
    return (unsigned long)getpid();
#endif
}

static void snapshot_path_for(char *out, size_t n, int serial) {
    /* The system's temporary directory, which is what it is for. Not SDL's preferences path:
       this file is compiled into the headless audit tool too, which has no SDL. */
    const char *dir = getenv("TEMP");
    if (!dir) dir = getenv("TMPDIR");
    if (!dir) dir = getenv("TMP");
    unsigned long who = snapshot_owner();
    if (dir && dir[0]) snprintf(out, n, "%s/circuit_undo_%lu_%d.cpg", dir, who, serial);
    else snprintf(out, n, "circuit_undo_%lu_%d.cpg", who, serial);
}

bool circuit_push_snapshot_undo(Circuit *circuit) {
    if (!circuit) return false;
    static int serial = 0;
    char path[264];
    snapshot_path_for(path, sizeof path, serial++);
    if (!file_save_circuit(circuit, path)) return false;
    circuit_push_undo(circuit, UNDO_SNAPSHOT, 0, NULL, 0, 0);
    if (circuit->undo_count > 0)
        snprintf(circuit->undo_stack[circuit->undo_count - 1].snapshot, 264, "%s", path);
    return true;
}

/* Clear the canvas without discarding the undo stack. Clearing normally means starting again,
   and records about a circuit that no longer exists are worse than none - but a clear that has
   just been recorded whole is the one case where the stack is the only way back. */
void circuit_clear_after_snapshot(Circuit *circuit) {
    if (!circuit) return;
    circuit->undo_preserving = true;
    circuit_clear(circuit);
    circuit->undo_preserving = false;
}

void circuit_push_probe_undo(Circuit *circuit, int probe_id) {
    if (!circuit) return;
    for (int i = 0; i < circuit->num_probes; i++) {
        if (circuit->probes[i].id != probe_id) continue;
        circuit_push_undo(circuit, UNDO_ADD_PROBE, probe_id, NULL, circuit->probes[i].x,
                          circuit->probes[i].y);
        if (circuit->undo_count > 0)
            circuit->undo_stack[circuit->undo_count - 1].probe_backup = circuit->probes[i];
        return;
    }
}

void circuit_delete_probe(Circuit *circuit, int probe_id) {
    if (!circuit) return;
    for (int i = 0; i < circuit->num_probes; i++) {
        if (circuit->probes[i].id != probe_id) continue;
        circuit_push_undo(circuit, UNDO_REMOVE_PROBE, probe_id, NULL, circuit->probes[i].x,
                          circuit->probes[i].y);
        if (circuit->undo_count > 0)
            circuit->undo_stack[circuit->undo_count - 1].probe_backup = circuit->probes[i];
        circuit_remove_probe(circuit, probe_id);
        return;
    }
}

/* Which probe is the recorded one? A probe's id is its position in the list plus one, so it
   changes under any probe that is added or removed before it - an id noted before an edit names
   a different probe afterwards, or none. What identifies a probe is the node it is on and where
   it sits. */
static int probe_like(Circuit *circuit, const Probe *p) {
    if (!circuit || !p) return -1;
    for (int i = 0; i < circuit->num_probes; i++)
        if (circuit->probes[i].node_id == p->node_id &&
            fabsf(circuit->probes[i].x - p->x) < 0.5f &&
            fabsf(circuit->probes[i].y - p->y) < 0.5f)
            return circuit->probes[i].id;
    return -1;
}

/* Put a recorded probe back on the circuit, name and channel and all. */
static void circuit_restore_probe(Circuit *circuit, const Probe *p) {
    if (!circuit || !p || circuit->num_probes >= MAX_PROBES) return;
    int id = circuit_add_probe(circuit, p->node_id, p->x, p->y);
    if (id <= 0) return;
    Probe *back = &circuit->probes[circuit->num_probes - 1];
    int keep_id = back->id;
    *back = *p;
    back->id = keep_id;
}

void circuit_push_edit_undo(Circuit *circuit, Component *comp) {
    if (!circuit || !comp) return;
    circuit_push_undo(circuit, UNDO_EDIT_COMPONENT, comp->id, component_clone(comp), comp->x, comp->y);
}

void circuit_undo_batch_begin(Circuit *circuit) {
    if (!circuit || circuit->undo_batch_current) return;   /* already inside one */
    circuit->undo_batch_current = ++circuit->undo_batch_next;
}

void circuit_undo_batch_end(Circuit *circuit) {
    if (circuit) circuit->undo_batch_current = 0;
}

/* Delete, and be able to take it back. circuit_remove_component and circuit_remove_wire do what
   they say and nothing else: they free the thing. Every caller that deletes on a user's behalf
   has to record what it destroyed first, and none of them did - so Ctrl+Z could bring back a
   part you moved or added, and nothing you deleted. These two do the recording and then the
   removing, and they are what the delete tool, the Delete key and the audit all call. */
void circuit_delete_component(Circuit *circuit, int comp_id) {
    if (!circuit) return;
    Component *victim = NULL;
    for (int i = 0; i < circuit->num_components; i++)
        if (circuit->components[i]->id == comp_id) { victim = circuit->components[i]; break; }
    if (!victim) return;
    circuit_push_undo(circuit, UNDO_REMOVE_COMPONENT, comp_id, component_clone(victim), 0, 0);
    /* Keep the nodes it was on, for the same reason a recorded wire delete does: a wire deleted
       in the same act remembers which nodes it joined by id, and sweeping them up here is what
       leaves that wire with nowhere to go - its two ends resolve to one node and it cannot come
       back at all. An empty node is inert. */
    circuit->undo_preserving = true;
    circuit_remove_component(circuit, comp_id);
    circuit->undo_preserving = false;
}

void circuit_delete_wire(Circuit *circuit, int wire_id) {
    if (!circuit) return;
    for (int i = 0; i < circuit->num_wires; i++) {
        if (circuit->wires[i].id == wire_id) {
            int sn = circuit->wires[i].start_node_id, en = circuit->wires[i].end_node_id;
            Node *ns = circuit_get_node(circuit, sn), *ne = circuit_get_node(circuit, en);
            circuit_push_undo(circuit, UNDO_REMOVE_WIRE, wire_id, NULL, (float)sn, (float)en);
            if (circuit->undo_count > 0) {
                UndoAction *rec = &circuit->undo_stack[circuit->undo_count - 1];
                rec->wire_x0 = ns ? ns->x : 0; rec->wire_y0 = ns ? ns->y : 0;
                rec->wire_x1 = ne ? ne->x : 0; rec->wire_y1 = ne ? ne->y : 0;
            }
            circuit_remove_wire(circuit, wire_id);
            return;
        }
    }
}

static bool circuit_undo_one(Circuit *circuit) {
    if (!circuit || circuit->undo_count == 0) return false;

    UndoAction *action = &circuit->undo_stack[--circuit->undo_count];
    const int batch_of_this = action->batch;
    const int redo_before = circuit->redo_count;

    switch (action->type) {
        case UNDO_ADD_COMPONENT: {
            // Remove the component that was added - first backup for redo
            Component *backup = NULL;
            for (int i = 0; i < circuit->num_components; i++) {
                if (circuit->components[i]->id == action->id) {
                    backup = component_clone(circuit->components[i]);
                    break;
                }
            }
            // Push to redo stack (redo will re-add it)
            circuit_push_redo_internal(circuit, UNDO_REMOVE_COMPONENT, action->id, backup, 0, 0, 0, 0);
            // Remove the component
            circuit_remove_component(circuit, action->id);
            // Remove the undo entry that circuit_remove_component just pushed
            if (circuit->undo_count > 0) {
                UndoAction *last = &circuit->undo_stack[circuit->undo_count - 1];
                if (last->type == UNDO_REMOVE_COMPONENT && last->id == action->id) {
                    if (last->component_backup) {
                        component_free(last->component_backup);
                    }
                    circuit->undo_count--;
                }
            }
            break;
        }

        case UNDO_REMOVE_COMPONENT:
            // Re-add the component that was removed
            if (action->component_backup) {
                action->component_backup->id = action->id;
                circuit->components[circuit->num_components++] = action->component_backup;
                /* and put it back on the circuit. Removing it cleaned up any node left with
                   nothing on it, so the ids it is carrying may name nothing - or, since ids are
                   reused, may name a node that now belongs to another part. Its terminals are
                   where they always were, and the nodes come from those. */
                circuit_reattach_component(circuit, action->component_backup);
                // Push to redo stack (redo will remove it again)
                circuit_push_redo_internal(circuit, UNDO_ADD_COMPONENT, action->id, NULL, 0, 0, 0, 0);
                action->component_backup = NULL;  // Don't free it
            }
            break;

        case UNDO_SNAPSHOT: {
            /* Put the recorded circuit back, and record the one being replaced so redo can
               return to it. Loading clears the undo stack, so the record is taken out of the way
               first and the stacks are restored around it. */
            char here[264];
            snprintf(here, sizeof here, "%s", action->snapshot);
            char now[264];
            static int redo_serial = 100000;
            snapshot_path_for(now, sizeof now, redo_serial++);
            bool have_now = file_save_circuit(circuit, now);

            /* Loading a circuit clears the canvas, and clearing it normally throws the undo
               stack away with it - which would take this record and every other one, and the
               files they own, with it. undo_preserving says the stack is the way back. */
            circuit->undo_preserving = true;
            bool loaded = file_load_circuit(circuit, here);
            circuit->undo_preserving = false;
            circuit->undo_restored_circuit = loaded;
            remove(here);
            action->snapshot[0] = 0;
            if (loaded && have_now) {
                circuit_push_redo_internal(circuit, UNDO_SNAPSHOT, 0, NULL, 0, 0, 0, 0);
                if (circuit->redo_count > 0)
                    snprintf(circuit->redo_stack[circuit->redo_count - 1].snapshot, 264, "%s", now);
            } else if (!loaded && have_now) {
                remove(now);
            }
            break;
        }

        case UNDO_ADD_PROBE:
            circuit_push_redo_internal(circuit, UNDO_REMOVE_PROBE, action->id, NULL,
                                       action->old_x, action->old_y, 0, 0);
            if (circuit->redo_count > 0)
                circuit->redo_stack[circuit->redo_count - 1].probe_backup = action->probe_backup;
            {   int pid = probe_like(circuit, &action->probe_backup);
                if (pid > 0) circuit_remove_probe(circuit, pid); }
            break;

        case UNDO_REMOVE_PROBE:
            circuit_restore_probe(circuit, &action->probe_backup);
            circuit_push_redo_internal(circuit, UNDO_ADD_PROBE,
                                       circuit->num_probes > 0 ? circuit->probes[circuit->num_probes - 1].id : 0,
                                       NULL, action->old_x, action->old_y, 0, 0);
            if (circuit->redo_count > 0)
                circuit->redo_stack[circuit->redo_count - 1].probe_backup = action->probe_backup;
            break;

        case UNDO_EDIT_COMPONENT: {
            /* Swap: the recorded part becomes the live one, and what was live is recorded for
               the redo. Properties are adopted rather than assigned, because two of them own
               memory through the union and a raw copy would hand this part someone else's
               buffers. */
            Component *live = NULL;
            for (int i = 0; i < circuit->num_components; i++)
                if (circuit->components[i]->id == action->id) { live = circuit->components[i]; break; }
            if (live && action->component_backup) {
                Component *was = component_clone(live);
                Component *b = action->component_backup;
                component_adopt_props(live, &b->props);
                live->rotation = b->rotation;
                live->x = b->x;
                live->y = b->y;
                snprintf(live->part, sizeof live->part, "%s", b->part);
                /* The part never left, so it still owns its nodes: move those back to where its
                   terminals are now, the way rotating or dragging does. Finding or making nodes
                   instead would hand it fresh ones and leave whatever it was wired to sitting on
                   the old ones - the connection would be lost by the undo. */
                circuit_update_component_nodes(circuit, live);
                circuit_push_redo_internal(circuit, UNDO_EDIT_COMPONENT, action->id, was,
                                           live->x, live->y, 0, 0);
            }
            break;
        }

        case UNDO_MOVE_COMPONENT:
            // Move component back to old position
            for (int i = 0; i < circuit->num_components; i++) {
                if (circuit->components[i]->id == action->id) {
                    float cur_x = circuit->components[i]->x;
                    float cur_y = circuit->components[i]->y;
                    circuit->components[i]->x = action->old_x;
                    circuit->components[i]->y = action->old_y;
                    circuit_update_component_nodes(circuit, circuit->components[i]);
                    // Push to redo stack (redo will move back to current position)
                    circuit_push_redo_internal(circuit, UNDO_MOVE_COMPONENT, action->id, NULL, cur_x, cur_y, 0, 0);
                    break;
                }
            }
            break;

        case UNDO_ADD_WIRE: {
            /* The wire is still here, so its nodes still are: note where they sit before it
               goes, or the redo has nothing to attach to once they are cleaned up. */
            float wx0 = 0, wy0 = 0, wx1 = 0, wy1 = 0;
            for (int i = 0; i < circuit->num_wires; i++) {
                if (circuit->wires[i].id != action->id) continue;
                Node *n0 = circuit_get_node(circuit, circuit->wires[i].start_node_id);
                Node *n1 = circuit_get_node(circuit, circuit->wires[i].end_node_id);
                if (n0) { wx0 = n0->x; wy0 = n0->y; }
                if (n1) { wx1 = n1->x; wy1 = n1->y; }
                break;
            }
            circuit_push_redo_internal(circuit, UNDO_REMOVE_WIRE, action->id, NULL, 0, 0,
                                       (int)action->old_x, (int)action->old_y);
            if (circuit->redo_count > 0) {
                UndoAction *r = &circuit->redo_stack[circuit->redo_count - 1];
                r->wire_x0 = wx0; r->wire_y0 = wy0; r->wire_x1 = wx1; r->wire_y1 = wy1;
            }
            circuit_remove_wire(circuit, action->id);
            break;
        }

        case UNDO_REMOVE_WIRE: {
            /* The nodes this wire joined may have gone with it: a node nothing else touches is
               cleaned up the moment the wire leaves. Put them back where they were and rejoin
               those - otherwise the wire returns attached to nothing and the circuit settles
               differently than it did before the delete.

               The position recorded is the node's own, so it is matched exactly. The ten pixels
               a component's terminal is allowed to reach for is wrong here in both directions:
               too small and the wire misses a node that is still there, too large and it grabs a
               neighbour and two nets that were separate become one. */
            if (!circuit_get_node(circuit, action->wire_start)) {
                int n0 = node_nearest(circuit, action->wire_x0, action->wire_y0, 10.0f);
                action->wire_start = (n0 > 0) ? n0
                    : circuit_create_node(circuit, action->wire_x0, action->wire_y0);
            }
            if (!circuit_get_node(circuit, action->wire_end)) {
                int n1 = node_nearest(circuit, action->wire_x1, action->wire_y1, 10.0f);
                action->wire_end = (n1 > 0) ? n1
                    : circuit_create_node(circuit, action->wire_x1, action->wire_y1);
            }
            int wire_id = circuit_add_wire(circuit, action->wire_start, action->wire_end);
            // Push to redo stack (redo will remove it)
            circuit_push_redo_internal(circuit, UNDO_ADD_WIRE, wire_id, NULL,
                                       (float)action->wire_start, (float)action->wire_end, 0, 0);
            break;
        }
    }

    // Clean up backup if not used
    if (action->component_backup) {
        component_free(action->component_backup);
        action->component_backup = NULL;
    }

    /* Whatever this left on the redo stack belongs to the same act, so one press of redo takes
       all of it forward again. */
    for (int i = redo_before; i < circuit->redo_count; i++)
        circuit->redo_stack[i].batch = batch_of_this;

    circuit->modified = true;
    circuit->topology_dirty = true;
    return true;
}

/* One press takes back one act. Edits recorded inside a batch are one act however many entries
   they left on the stack, so the whole run of them comes back together. */
bool circuit_undo(Circuit *circuit) {
    if (!circuit || circuit->undo_count == 0) return false;
    int batch = circuit->undo_stack[circuit->undo_count - 1].batch;
    if (!circuit_undo_one(circuit)) return false;
    while (batch != 0 && circuit->undo_count > 0 &&
           circuit->undo_stack[circuit->undo_count - 1].batch == batch) {
        if (!circuit_undo_one(circuit)) break;
    }
    return true;
}

static bool circuit_redo_one(Circuit *circuit) {
    if (!circuit || circuit->redo_count == 0) return false;

    UndoAction *action = &circuit->redo_stack[--circuit->redo_count];
    const int batch_of_this = action->batch;
    const int undo_before = circuit->undo_count;

    switch (action->type) {
        case UNDO_ADD_COMPONENT: {
            // Re-remove the component - first backup for undo
            Component *backup = NULL;
            for (int i = 0; i < circuit->num_components; i++) {
                if (circuit->components[i]->id == action->id) {
                    backup = component_clone(circuit->components[i]);
                    break;
                }
            }
            // Remove the component (don't use circuit_remove_component to avoid pushing to undo)
            for (int i = 0; i < circuit->num_components; i++) {
                if (circuit->components[i]->id == action->id) {
                    component_free(circuit->components[i]);
                    // Shift remaining components
                    for (int j = i; j < circuit->num_components - 1; j++) {
                        circuit->components[j] = circuit->components[j + 1];
                    }
                    circuit->num_components--;
                    break;
                }
            }
            // Push to undo stack directly (undo will re-add it)
            if (circuit->undo_count < MAX_UNDO) {
                UndoAction *undo = &circuit->undo_stack[circuit->undo_count++];
                undo->type = UNDO_ADD_COMPONENT;
                undo->id = action->id;
                undo->component_backup = backup;
                undo->old_x = 0;
                undo->old_y = 0;
            } else if (backup) {
                component_free(backup);
            }
            break;
        }

        case UNDO_REMOVE_COMPONENT:
            // Re-add the component
            if (action->component_backup) {
                action->component_backup->id = action->id;
                circuit->components[circuit->num_components++] = action->component_backup;
                circuit_reattach_component(circuit, action->component_backup);
                // Push to undo stack (undo will remove it)
                if (circuit->undo_count < MAX_UNDO) {
                    UndoAction *undo = &circuit->undo_stack[circuit->undo_count++];
                    undo->type = UNDO_REMOVE_COMPONENT;
                    undo->id = action->id;
                    undo->component_backup = NULL;
                    undo->old_x = 0;
                    undo->old_y = 0;
                }
                action->component_backup = NULL;  // Don't free it
            }
            break;

        /* On the redo stack a type names what the undo did, and redo does the opposite - the
           same convention the component cases above use. UNDO_ADD_PROBE here means the undo put
           a probe back, so redoing takes it off again. */
        case UNDO_SNAPSHOT: {
            char here[264];
            snprintf(here, sizeof here, "%s", action->snapshot);
            char now[264];
            static int undo_serial = 200000;
            snapshot_path_for(now, sizeof now, undo_serial++);
            bool have_now = file_save_circuit(circuit, now);

            circuit->undo_preserving = true;
            bool loaded = file_load_circuit(circuit, here);
            circuit->undo_preserving = false;
            circuit->undo_restored_circuit = loaded;
            remove(here);
            action->snapshot[0] = 0;
            if (loaded && have_now && circuit->undo_count < MAX_UNDO) {
                UndoAction *undo = &circuit->undo_stack[circuit->undo_count++];
                memset(undo, 0, sizeof *undo);
                undo->type = UNDO_SNAPSHOT;
                snprintf(undo->snapshot, sizeof undo->snapshot, "%s", now);
            } else if (!loaded && have_now) {
                remove(now);
            }
            break;
        }

        case UNDO_ADD_PROBE:
            if (circuit->undo_count < MAX_UNDO) {
                UndoAction *undo = &circuit->undo_stack[circuit->undo_count++];
                memset(undo, 0, sizeof *undo);
                undo->type = UNDO_REMOVE_PROBE;
                undo->id = action->id;
                undo->probe_backup = action->probe_backup;
            }
            {   int pid = probe_like(circuit, &action->probe_backup);
                if (pid > 0) circuit_remove_probe(circuit, pid); }
            break;

        case UNDO_REMOVE_PROBE:
            circuit_restore_probe(circuit, &action->probe_backup);
            if (circuit->undo_count < MAX_UNDO && circuit->num_probes > 0) {
                UndoAction *undo = &circuit->undo_stack[circuit->undo_count++];
                memset(undo, 0, sizeof *undo);
                undo->type = UNDO_ADD_PROBE;
                undo->id = circuit->probes[circuit->num_probes - 1].id;
                undo->probe_backup = action->probe_backup;
            }
            break;

        case UNDO_EDIT_COMPONENT: {
            Component *live = NULL;
            for (int i = 0; i < circuit->num_components; i++)
                if (circuit->components[i]->id == action->id) { live = circuit->components[i]; break; }
            if (live && action->component_backup) {
                Component *was = component_clone(live);
                Component *b = action->component_backup;
                component_adopt_props(live, &b->props);
                live->rotation = b->rotation;
                live->x = b->x;
                live->y = b->y;
                snprintf(live->part, sizeof live->part, "%s", b->part);
                /* The part never left, so it still owns its nodes: move those back to where its
                   terminals are now, the way rotating or dragging does. Finding or making nodes
                   instead would hand it fresh ones and leave whatever it was wired to sitting on
                   the old ones - the connection would be lost by the undo. */
                circuit_update_component_nodes(circuit, live);
                if (circuit->undo_count < MAX_UNDO) {
                    UndoAction *undo = &circuit->undo_stack[circuit->undo_count++];
                    memset(undo, 0, sizeof *undo);
                    undo->type = UNDO_EDIT_COMPONENT;
                    undo->id = action->id;
                    undo->component_backup = was;
                } else {
                    component_free(was);
                }
            }
            break;
        }

        case UNDO_MOVE_COMPONENT:
            // Move component to the redo position
            for (int i = 0; i < circuit->num_components; i++) {
                if (circuit->components[i]->id == action->id) {
                    float cur_x = circuit->components[i]->x;
                    float cur_y = circuit->components[i]->y;
                    circuit->components[i]->x = action->old_x;
                    circuit->components[i]->y = action->old_y;
                    circuit_update_component_nodes(circuit, circuit->components[i]);
                    // Push to undo stack (undo will move back)
                    if (circuit->undo_count < MAX_UNDO) {
                        UndoAction *undo = &circuit->undo_stack[circuit->undo_count++];
                        undo->type = UNDO_MOVE_COMPONENT;
                        undo->id = action->id;
                        undo->component_backup = NULL;
                        undo->old_x = cur_x;
                        undo->old_y = cur_y;
                    }
                    break;
                }
            }
            break;

        case UNDO_ADD_WIRE: {
            // Remove the wire
            int wire_start = (int)action->old_x;
            int wire_end = (int)action->old_y;
            circuit_remove_wire(circuit, action->id);
            // Push to undo stack (undo will re-add it)
            if (circuit->undo_count < MAX_UNDO) {
                UndoAction *undo = &circuit->undo_stack[circuit->undo_count++];
                undo->type = UNDO_ADD_WIRE;
                undo->id = action->id;
                undo->component_backup = NULL;
                undo->old_x = (float)wire_start;
                undo->old_y = (float)wire_end;
                undo->wire_start = wire_start;
                undo->wire_end = wire_end;
            }
            break;
        }

        case UNDO_REMOVE_WIRE: {
            if (!circuit_get_node(circuit, action->wire_start)) {
                int n0 = node_nearest(circuit, action->wire_x0, action->wire_y0, 10.0f);
                action->wire_start = (n0 > 0) ? n0
                    : circuit_create_node(circuit, action->wire_x0, action->wire_y0);
            }
            if (!circuit_get_node(circuit, action->wire_end)) {
                int n1 = node_nearest(circuit, action->wire_x1, action->wire_y1, 10.0f);
                action->wire_end = (n1 > 0) ? n1
                    : circuit_create_node(circuit, action->wire_x1, action->wire_y1);
            }
            int wire_id = circuit_add_wire(circuit, action->wire_start, action->wire_end);
            // Push to undo stack (undo will remove it)
            if (circuit->undo_count < MAX_UNDO) {
                UndoAction *undo = &circuit->undo_stack[circuit->undo_count++];
                undo->type = UNDO_REMOVE_WIRE;
                undo->id = wire_id;
                undo->component_backup = NULL;
                undo->old_x = (float)action->wire_start;
                undo->old_y = (float)action->wire_end;
                undo->wire_start = action->wire_start;
                undo->wire_end = action->wire_end;
            }
            break;
        }
    }

    // Clean up backup if not used
    if (action->component_backup) {
        component_free(action->component_backup);
        action->component_backup = NULL;
    }

    /* the same, back the other way: what this put on the undo stack is one act */
    for (int i = undo_before; i < circuit->undo_count; i++)
        circuit->undo_stack[i].batch = batch_of_this;

    circuit->modified = true;
    circuit->topology_dirty = true;
    return true;
}

/* One press puts one act forward again, batch and all. */
bool circuit_redo(Circuit *circuit) {
    if (!circuit || circuit->redo_count == 0) return false;
    int batch = circuit->redo_stack[circuit->redo_count - 1].batch;
    if (!circuit_redo_one(circuit)) return false;
    while (batch != 0 && circuit->redo_count > 0 &&
           circuit->redo_stack[circuit->redo_count - 1].batch == batch) {
        if (!circuit_redo_one(circuit)) break;
    }
    return true;
}

void circuit_clear_undo(Circuit *circuit) {
    if (!circuit) return;

    for (int i = 0; i < circuit->undo_count; i++) undo_action_release(&circuit->undo_stack[i]);
    circuit->undo_count = 0;
    /* The redo stack goes too. It did not, and its records outlived the circuit they described:
       open a file and press Ctrl+Y and the app replayed an action belonging to the circuit that
       was on the canvas before, naming parts by numbers that now mean something else. Its only
       caller is circuit_clear, which is throwing the whole circuit away - there is no reading of
       that where a redo of the old one still makes sense. */
    for (int i = 0; i < circuit->redo_count; i++) undo_action_release(&circuit->redo_stack[i]);
    circuit->redo_count = 0;
    /* Deleting a part or a wire leaves the nodes it was on in place, because an undo may want to
       reconnect to exactly those. Once no undo can, they are litter: this is the moment to sweep
       them up, and the only moment where it is certainly safe. */
    circuit_cleanup_orphaned_nodes(circuit);
}

bool circuit_has_active_sweep(Circuit *circuit) {
    if (!circuit) return false;

    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        if (!comp) continue;

        switch (comp->type) {
            case COMP_DC_VOLTAGE:
                if (comp->props.dc_voltage.voltage_sweep.enabled)
                    return true;
                break;
            case COMP_AC_VOLTAGE:
                if (comp->props.ac_voltage.amplitude_sweep.enabled ||
                    comp->props.ac_voltage.frequency_sweep.enabled)
                    return true;
                break;
            case COMP_DC_CURRENT:
                if (comp->props.dc_current.current_sweep.enabled)
                    return true;
                break;
            case COMP_SQUARE_WAVE:
                if (comp->props.square_wave.amplitude_sweep.enabled ||
                    comp->props.square_wave.frequency_sweep.enabled)
                    return true;
                break;
            case COMP_TRIANGLE_WAVE:
                if (comp->props.triangle_wave.amplitude_sweep.enabled ||
                    comp->props.triangle_wave.frequency_sweep.enabled)
                    return true;
                break;
            case COMP_SAWTOOTH_WAVE:
                if (comp->props.sawtooth_wave.amplitude_sweep.enabled ||
                    comp->props.sawtooth_wave.frequency_sweep.enabled)
                    return true;
                break;
            case COMP_NOISE_SOURCE:
                if (comp->props.noise_source.amplitude_sweep.enabled)
                    return true;
                break;
            default:
                break;
        }
    }
    return false;
}

/* Turn a subcircuit definition back into a circuit you can look at.

   A definition stores parts and the internal node each terminal sits on; it does not store
   wires, because it never needed them - the solver works from node numbers. A drawing does need
   them, so every terminal is placed at its own position and one wire is run between consecutive
   terminals that share an internal node. That is a star per net rather than the routing someone
   would draw by hand, but it is the true topology, which is what the question "what is inside
   this block" is asking. */
/* Rewrite every run of overlapping wires that lie along one line into a single chain.

   The idiom that produces them is everywhere in the template builders and reads perfectly
   sensibly: four parts that return to the same ground node each get a wire to it, and if they
   are all on the same row those four wires lie on top of one another. What is drawn is one
   line with three others hidden under it - a reader cannot see where anything joins, and
   whether two of them meet is not on the drawing at all.

   The fix is the bus that was meant: take every wire on a given row, collect the points where
   something attaches, and lay one segment between each neighbouring pair. Same net, same
   junctions, one line. Only whole runs that actually overlap are touched; two wires that merely
   share an endpoint end-to-end are already a chain and are left alone.

   Applied after a template is placed rather than inside circuit_add_wire, so that drawing a
   wire by hand still does exactly what the hand said. */
void circuit_tidy_collinear_wires(Circuit *circuit) {
    if (!circuit || circuit->num_wires < 2) return;
    /* Every rebuild restarts the scan, so a case this cannot settle would spin forever - and
       one did, until the points below were deduped at the radius nodes actually merge at. The
       cap is what stops a hang being the failure mode of a drawing tidy-up. */
    /* An A/B switch, because the only way to see what this pass changed is to run the same
       template with it off and diff the nets. */
    if (getenv("CT_NO_TIDY")) return;
    int budget = 400;
    /* Nothing here is a user edit: it is the same net, drawn properly. Two consequences, and
       both were bugs. The deletions must not reach the undo stack, or one Ctrl+Z after opening
       a template starts pulling apart wires the user never drew. And orphan cleanup must be
       held off while a run is torn down and relaid, because it sweeps up a node that is briefly
       unused - and if that node is ground, remove_node_by_index clears ground_node_id and the
       whole circuit floats. That is what made the CMOS inverter stop swinging. */
    bool prev_preserving = circuit->undo_preserving;
    circuit->undo_preserving = true;

    for (int pass = 0; pass < 2; pass++) {          /* 0 = horizontal runs, 1 = vertical */
        for (int i = 0; i < circuit->num_wires; i++) {
            Node *a = circuit_get_node(circuit, circuit->wires[i].start_node_id);
            Node *b = circuit_get_node(circuit, circuit->wires[i].end_node_id);
            if (!a || !b) continue;
            bool horiz = fabsf(a->y - b->y) <= 0.5f, vert = fabsf(a->x - b->x) <= 0.5f;
            if (pass == 0 && (!horiz || vert)) continue;      /* skip verticals and points */
            if (pass == 1 && (!vert || horiz)) continue;
            float line = (pass == 0) ? a->y : a->x;

            /* every wire on this same line, and the span they cover together */
            int member[MAX_WIRES], nm = 0;
            float lo = 1e9f, hi = -1e9f;
            for (int j = 0; j < circuit->num_wires; j++) {
                Node *c = circuit_get_node(circuit, circuit->wires[j].start_node_id);
                Node *d = circuit_get_node(circuit, circuit->wires[j].end_node_id);
                if (!c || !d) continue;
                if (pass == 0) {
                    if (fabsf(c->y - d->y) > 0.5f || fabsf(c->x - d->x) <= 0.5f) continue;
                    if (fabsf(c->y - line) > 0.5f) continue;
                } else {
                    if (fabsf(c->x - d->x) > 0.5f || fabsf(c->y - d->y) <= 0.5f) continue;
                    if (fabsf(c->x - line) > 0.5f) continue;
                }
                float p = (pass == 0) ? c->x : c->y, q = (pass == 0) ? d->x : d->y;
                if (nm < MAX_WIRES) member[nm++] = j;
                if (fminf(p, q) < lo) lo = fminf(p, q);
                if (fmaxf(p, q) > hi) hi = fmaxf(p, q);
            }
            if (nm < 2) continue;
            (void)lo; (void)hi;

            /* Work one connected run at a time, never across a gap.

               A row can carry two quite separate buses with clear space between them, and
               chaining every point on the row would lay a wire across that space and join two
               nets that have nothing to do with each other. So: find the group of wires that
               actually touch or overlap each other, and rebuild only that. */
            bool rebuilt = false;
            for (int seed = 0; seed < nm; seed++) {
                int grp[MAX_WIRES], ng = 0;
                float glo, ghi;
                {
                    Node *c = circuit_get_node(circuit, circuit->wires[member[seed]].start_node_id);
                    Node *d = circuit_get_node(circuit, circuit->wires[member[seed]].end_node_id);
                    float p = (pass == 0) ? c->x : c->y, q = (pass == 0) ? d->x : d->y;
                    glo = fminf(p, q); ghi = fmaxf(p, q);
                }
                grp[ng++] = member[seed];
                bool grew = true;
                while (grew) {                        /* absorb anything touching the run */
                    grew = false;
                    for (int k = 0; k < nm; k++) {
                        bool have = false;
                        for (int g = 0; g < ng; g++) if (grp[g] == member[k]) have = true;
                        if (have) continue;
                        Node *c = circuit_get_node(circuit, circuit->wires[member[k]].start_node_id);
                        Node *d = circuit_get_node(circuit, circuit->wires[member[k]].end_node_id);
                        float p = (pass == 0) ? c->x : c->y, q = (pass == 0) ? d->x : d->y;
                        float klo = fminf(p, q), khi = fmaxf(p, q);
                        if (khi < glo - 0.5f || klo > ghi + 0.5f) continue;   /* disjoint */
                        /* ...and only when it is provably the same net already, which means
                           sharing a node with a wire the run already holds. Lying on top of
                           one another is not connection: wires join only through shared nodes,
                           so two different nets can be drawn down the same line - and the CMOS
                           inverter draws exactly that, the rail and the output overlapping on
                           one vertical. Chaining those shorted the output to VDD and the gate
                           stopped swinging. Redrawing a net must never change what is joined to
                           what; an overlap between two nets is a layout fault to be moved apart,
                           not a bus to be tidied. */
                        bool shares = false;
                        int ks = circuit->wires[member[k]].start_node_id;
                        int ke = circuit->wires[member[k]].end_node_id;
                        for (int g = 0; g < ng && !shares; g++) {
                            int gs = circuit->wires[grp[g]].start_node_id;
                            int ge = circuit->wires[grp[g]].end_node_id;
                            if (ks == gs || ks == ge || ke == gs || ke == ge) shares = true;
                        }
                        if (!shares) continue;
                        if (ng < MAX_WIRES) grp[ng++] = member[k];
                        if (klo < glo) glo = klo;
                        if (khi > ghi) ghi = khi;
                        grew = true;
                    }
                }
                if (ng < 2) continue;

                /* Does this run actually double back on itself? Length laid against length
                   covered - equal means the wires meet end to end and are already a chain. */
                float laid = 0;
                for (int g = 0; g < ng; g++) {
                    Node *c = circuit_get_node(circuit, circuit->wires[grp[g]].start_node_id);
                    Node *d = circuit_get_node(circuit, circuit->wires[grp[g]].end_node_id);
                    float p = (pass == 0) ? c->x : c->y, q = (pass == 0) ? d->x : d->y;
                    laid += fabsf(q - p);
                }
                if (laid <= (ghi - glo) + 0.5f) continue;

                float pt[2 * MAX_WIRES]; int np = 0;
                for (int g = 0; g < ng && np + 2 <= (int)(sizeof pt / sizeof pt[0]); g++) {
                    Node *c = circuit_get_node(circuit, circuit->wires[grp[g]].start_node_id);
                    Node *d = circuit_get_node(circuit, circuit->wires[grp[g]].end_node_id);
                    pt[np++] = (pass == 0) ? c->x : c->y;
                    pt[np++] = (pass == 0) ? d->x : d->y;
                }
                for (int p1 = 0; p1 < np; p1++)                  /* sort, then unique */
                    for (int p2 = p1 + 1; p2 < np; p2++)
                        if (pt[p2] < pt[p1]) { float t = pt[p1]; pt[p1] = pt[p2]; pt[p2] = t; }
                /* Deduped at the radius circuit_find_or_create_node merges at, not at half a
                   pixel. Two points 3 px apart are one node to it, so planning a segment
                   between them lays nothing - and the run comes back unchanged on the next
                   scan, which is a loop rather than a fixed point. */
                int nu = 0;
                for (int p1 = 0; p1 < np; p1++)
                    if (nu == 0 || pt[p1] - pt[nu - 1] > 5.0f) pt[nu++] = pt[p1];
                if (nu < 2) continue;

                /* Sort the group so the highest wire index goes first: deleting shuffles the
                   list down, and taking them in any other order removes the wrong ones. */
                for (int a1 = 0; a1 < ng; a1++)
                    for (int a2 = a1 + 1; a2 < ng; a2++)
                        if (grp[a2] > grp[a1]) { int t = grp[a1]; grp[a1] = grp[a2]; grp[a2] = t; }
                for (int g = 0; g < ng; g++)
                    circuit_remove_wire(circuit, circuit->wires[grp[g]].id);

                for (int k = 0; k + 1 < nu; k++) {
                    int n1, n2;
                    if (pass == 0) {
                        n1 = circuit_find_or_create_node(circuit, pt[k],     line, 5.0f);
                        n2 = circuit_find_or_create_node(circuit, pt[k + 1], line, 5.0f);
                    } else {
                        n1 = circuit_find_or_create_node(circuit, line, pt[k],     5.0f);
                        n2 = circuit_find_or_create_node(circuit, line, pt[k + 1], 5.0f);
                    }
                    if (n1 > 0 && n2 > 0 && n1 != n2) circuit_add_wire(circuit, n1, n2);
                }
                rebuilt = true;
                if (--budget <= 0) { circuit->undo_preserving = prev_preserving; return; }
                break;                                /* the list moved; rescan from the top */
            }
            /* Only when something actually moved. Restarting the scan unconditionally is an
               infinite loop and was one: every wire on the sheet sent it back to the beginning
               whether or not anything had been rebuilt. */
            if (rebuilt) i = -1;
        }
    }
    circuit->undo_preserving = prev_preserving;
    circuit->topology_dirty = true;
}

Circuit *circuit_from_subcircuit_def(int def_id, char *name_out, size_t name_size) {
    SubCircuitDef *def = subcircuit_find_def(def_id);
    if (!def || def->num_components <= 0 || !def->component_data) return NULL;
    Circuit *c = circuit_create();
    if (!c) return NULL;
    if (name_out && name_size) snprintf(name_out, name_size, "%s", def->name);

    const Component *src = (const Component *)def->component_data;
    /* The last terminal seen on each net, so a net of three parts is drawn as a chain of short
       hops rather than as three long ones back to whichever terminal happened to come first. */
    enum { VIEW_MAX_NET = 256 };
    int last_node[VIEW_MAX_NET];
    float last_x[VIEW_MAX_NET], last_y[VIEW_MAX_NET];
    for (int i = 0; i < VIEW_MAX_NET; i++) last_node[i] = 0;

    for (int i = 0; i < def->num_components; i++) {
        Component *p = component_create(src[i].type, src[i].x, src[i].y);
        if (!p) continue;
        p->props = src[i].props;
        /* rotation before the add: circuit_add_component makes the nodes from the terminal
           positions, and those depend on it */
        p->rotation = src[i].rotation;
        p->num_terminals = src[i].num_terminals;
        snprintf(p->label, sizeof p->label, "%s", src[i].label);
        snprintf(p->part, sizeof p->part, "%s", src[i].part);
        if (p->type == COMP_SUBCIRCUIT) {          /* a block inside a block keeps no instance */
            p->props.subcircuit.inst_data = NULL;
            p->props.subcircuit.inst_count = 0;
            p->props.subcircuit.inst_def_id = 0;
        }
        if (circuit_add_component(c, p) < 0) { free(p); continue; }
        for (int t = 0; t < p->num_terminals && t < MAX_TERMINALS; t++) {
            int internal = src[i].node_ids[t];
            if (internal <= 0 || internal >= VIEW_MAX_NET) continue;
            int nid = p->node_ids[t];
            float tx, ty;
            component_get_terminal_pos(p, t, &tx, &ty);
            if (last_node[internal] == 0) {
                last_node[internal] = nid; last_x[internal] = tx; last_y[internal] = ty;
                continue;
            }
            if (last_node[internal] != nid) {
                /* An elbow rather than a straight line between the two terminals. A definition
                   stores no wires, so these are invented - and a schematic drawn with diagonals
                   reads as a pile of string. Across, then down: two segments when the terminals
                   do not already share a row or a column, one when they do. */
                float ax = last_x[internal], ay = last_y[internal];
                if (fabsf(ax - tx) > 0.5f && fabsf(ay - ty) > 0.5f) {
                    int elbow = circuit_find_or_create_node(c, tx, ay, 5.0f);
                    circuit_add_wire(c, last_node[internal], elbow);
                    circuit_add_wire(c, elbow, nid);
                } else {
                    circuit_add_wire(c, last_node[internal], nid);
                }
            }
            last_node[internal] = nid; last_x[internal] = tx; last_y[internal] = ty;
        }
    }
    return c;
}
