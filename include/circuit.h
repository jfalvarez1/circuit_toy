/**
 * Circuit Playground - Circuit Container
 */

#ifndef CIRCUIT_H
#define CIRCUIT_H

#include "types.h"
#include "component.h"

// Circuit node
typedef struct {
    int id;
    float x, y;
    double voltage;
    bool is_ground;
    int connection_count;
} Node;

// Wire segment
typedef struct {
    int id;
    int start_node_id;
    int end_node_id;
    Point2D points[16];  // Intermediate bend points
    int num_points;
    bool selected;
    double current;
} Wire;

// Voltage probe
typedef struct {
    int id;
    int node_id;
    float x, y;
    Color color;
    double voltage;
    int channel_num;    // Oscilloscope channel number (0-based)
    char label[8];      // Label like "CH1", "CH2", etc.
    bool selected;      // Whether probe is selected for deletion
} Probe;

// Undo action types
typedef enum {
    UNDO_ADD_COMPONENT,
    UNDO_REMOVE_COMPONENT,
    UNDO_ADD_WIRE,
    UNDO_REMOVE_WIRE,
    UNDO_MOVE_COMPONENT,
    /* A part changed in place: a value typed in, a model toggled, a part number cycled, a
       rotation. One record covers all of them, because what it stores is the part as it was.
       Add, delete and move each need their own because they change what is on the canvas or
       where; this one is for a part that stays exactly where it is and is different. */
    UNDO_EDIT_COMPONENT,
    /* Probes are edits too: putting one on a node and taking it off again are things a user
       does constantly, and neither could be taken back. */
    UNDO_ADD_PROBE,
    UNDO_REMOVE_PROBE,
    /* The whole canvas as it was. Clearing it, or picking a circuit from the palette - which
       clears the last one - changes everything at once, and putting that back piece by piece
       means reconstructing a netlist exactly: which node was which, which wire joined them.
       A file does that already and is checked template by template, so the record is a file. */
    UNDO_SNAPSHOT
} UndoActionType;

// Undo action
typedef struct {
    UndoActionType type;
    int id;
    Component *component_backup;  // For remove/move actions
    float old_x, old_y;           // For move actions
    int wire_start, wire_end;     // For wire actions: the nodes it joined
    /* ...and where those nodes were. Removing a wire can leave a node with nothing else on it,
       and those are cleaned up straight away - so by the time anyone undoes, the id recorded
       above may name a node that is gone. The position always still means something. */
    float wire_x0, wire_y0, wire_x1, wire_y1;
    /* Edits made as one act share a batch, and undo takes the whole batch back. Deleting a
       selection of thirty parts is one thing the user did, not thirty; without this it took
       thirty presses of Ctrl+Z to put it back. 0 means an edit that stands alone. */
    int batch;
    /* A probe is small and owns nothing, so the record carries the whole of it - node, position,
       colour, channel and the name, which is the one thing a user typed. */
    Probe probe_backup;
    char snapshot[264];           /* UNDO_SNAPSHOT: the circuit as it was, on disk */
} UndoAction;

/* Deep enough to hold a whole circuit. Clearing the canvas, or picking a circuit from the
   palette - which clears the last one - is recorded as one act made of one record per part,
   wire and probe, and a large template is a few hundred of those. At a hundred entries the
   oldest were shifted off the bottom and the act could only be half taken back. The stacks live
   on the heap with the circuit, so this costs memory nothing else was using. */
#define MAX_UNDO 4000

// Circuit structure
typedef struct Circuit {
    // Components
    Component *components[MAX_COMPONENTS];
    int num_components;
    int next_component_id;

    // Nodes
    Node nodes[MAX_NODES];
    int num_nodes;
    int next_node_id;
    int ground_node_id;

    // Wires
    Wire wires[MAX_WIRES];
    int num_wires;
    int next_wire_id;

    // Probes
    Probe probes[MAX_PROBES];
    int num_probes;

    // Node index map for simulation (node_id -> matrix index)
    int node_map[MAX_NODES];
    int num_matrix_nodes;

    // Clipboard for copy/paste
    Component *clipboard;
    float clipboard_offset_x;
    float clipboard_offset_y;

    // Undo stack
    UndoAction undo_stack[MAX_UNDO];
    int undo_count;

    // Redo stack
    UndoAction redo_stack[MAX_UNDO];
    int redo_count;

    /* Undo batching: while a batch is open every edit recorded joins it. */
    int undo_batch_current;
    int undo_batch_next;

    // Modified flag
    /* Set while something is being taken away that an undo record depends on: the nodes a
       deleted wire was joined to, or the undo stack itself across a recorded clear. Whatever
       would normally be swept up here is what the undo needs to find again. */
    bool undo_preserving;
    /* Set by the last undo or redo if what it put back was a whole circuit. The scope was set up
       for the circuit that has just been replaced, and its time base means nothing here. */
    bool undo_restored_circuit;
    bool modified;          // unsaved changes (cleared on save / new / load)
    bool topology_dirty;    // the STRUCTURE changed - a component or wire was added, moved or
                            // deleted - so the node map and the matrix have to be rebuilt.
                            // Editing a value does not set this: the stamps read the new number
                            // on the next step, so a running simulation can keep running.
} Circuit;

// Create/destroy circuit
Circuit *circuit_create(void);
void circuit_free(Circuit *circuit);
void circuit_clear(Circuit *circuit);
/* Clear the canvas without discarding the undo stack - for a clear that has just been recorded
   whole by circuit_push_snapshot_undo, where the stack is the only way back. */
void circuit_clear_after_snapshot(Circuit *circuit);
/* Record the whole circuit as it is now, so one Ctrl+Z brings all of it back. For the acts that
   replace everything: clearing the canvas, or picking a circuit from the palette. */
bool circuit_push_snapshot_undo(Circuit *circuit);

// Component operations
int circuit_add_component(Circuit *circuit, Component *comp);
void circuit_remove_component(Circuit *circuit, int comp_id);
Component *circuit_get_component(Circuit *circuit, int comp_id);
Component *circuit_find_component_at(Circuit *circuit, float x, float y);

// Node operations
int circuit_create_node(Circuit *circuit, float x, float y);
Node *circuit_get_node(Circuit *circuit, int node_id);
Node *circuit_find_node_at(Circuit *circuit, float x, float y, float threshold);
int circuit_find_or_create_node(Circuit *circuit, float x, float y, float threshold);
void circuit_set_ground(Circuit *circuit, int node_id);

// Wire operations
int circuit_add_wire(Circuit *circuit, int start_node_id, int end_node_id);
void circuit_remove_wire(Circuit *circuit, int wire_id);
/* Delete on a user's behalf: records what it destroyed on the undo stack, then removes it.
   The plain remove_ functions above do not record anything. */
void circuit_delete_component(Circuit *circuit, int comp_id);
void circuit_delete_wire(Circuit *circuit, int wire_id);
void circuit_delete_probe(Circuit *circuit, int probe_id);
/* Record a probe that has just been added, so undo can take it off again. */
void circuit_push_probe_undo(Circuit *circuit, int probe_id);
Wire *circuit_find_wire_at(Circuit *circuit, float x, float y, float threshold);
int circuit_split_wire_at(Circuit *circuit, Wire *wire, float x, float y);

// Node cleanup
void circuit_cleanup_orphaned_nodes(Circuit *circuit);

// Probe operations
int circuit_add_probe(Circuit *circuit, int node_id, float x, float y);
void circuit_remove_probe(Circuit *circuit, int probe_id);

// Build node map for simulation (handles wire connections)
void circuit_build_node_map(Circuit *circuit);

// Update node voltages from solution
void circuit_update_voltages(Circuit *circuit, Vector *solution);

// Update wire currents based on connected components
void circuit_update_wire_currents(Circuit *circuit);

// Update voltmeter and ammeter readings from current node voltages
void circuit_update_meter_readings(Circuit *circuit);

// Update component terminals after movement
void circuit_update_component_nodes(Circuit *circuit, Component *comp);
/* Find or make nodes at a component's terminal positions - what adding one does, and what
   putting a deleted one back needs. update_component_nodes above only moves existing ones. */
void circuit_reattach_component(Circuit *circuit, Component *comp);

// Copy/paste operations
void circuit_copy_component(Circuit *circuit, Component *comp);
void circuit_cut_component(Circuit *circuit, Component *comp);
Component *circuit_paste_component(Circuit *circuit, float x, float y);
Component *circuit_duplicate_component(Circuit *circuit, Component *comp);

// Selection operations
void circuit_select_all(Circuit *circuit);
void circuit_deselect_all(Circuit *circuit);
void circuit_delete_selected(Circuit *circuit);

// Undo/Redo operations
void circuit_push_undo(Circuit *circuit, UndoActionType type, int id, Component *backup, float old_x, float old_y);
/* Everything recorded between these two comes back on one Ctrl+Z, and goes again on one Ctrl+Y. */
/* Record a component as it is right now, before changing it in place. */
void circuit_push_edit_undo(Circuit *circuit, Component *comp);
void circuit_undo_batch_begin(Circuit *circuit);
void circuit_undo_batch_end(Circuit *circuit);
bool circuit_undo(Circuit *circuit);
bool circuit_redo(Circuit *circuit);
void circuit_clear_undo(Circuit *circuit);
void circuit_clear_redo(Circuit *circuit);

// Serialization
bool circuit_save(Circuit *circuit, const char *filename);
bool circuit_load(Circuit *circuit, const char *filename);

// Check if any component has an active sweep (voltage, frequency, or amplitude)
bool circuit_has_active_sweep(Circuit *circuit);

#endif // CIRCUIT_H
