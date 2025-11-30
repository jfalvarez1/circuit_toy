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
    UNDO_MOVE_COMPONENT
} UndoActionType;

// Undo action
typedef struct {
    UndoActionType type;
    int id;
    Component *component_backup;  // For remove/move actions
    float old_x, old_y;           // For move actions
    int wire_start, wire_end;     // For wire actions
} UndoAction;

#define MAX_UNDO 100

// Circuit structure
typedef struct {
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

    // Modified flag
    bool modified;
} Circuit;

// Create/destroy circuit
Circuit *circuit_create(void);
void circuit_free(Circuit *circuit);
void circuit_clear(Circuit *circuit);

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
Wire *circuit_find_wire_at(Circuit *circuit, float x, float y, float threshold);

// Node cleanup
void circuit_cleanup_orphaned_nodes(Circuit *circuit);

// Probe operations
int circuit_add_probe(Circuit *circuit, int node_id, float x, float y);
void circuit_remove_probe(Circuit *circuit, int probe_id);

// Build node map for simulation (handles wire connections)
void circuit_build_node_map(Circuit *circuit);

// Update node voltages from solution
void circuit_update_voltages(Circuit *circuit, Vector *solution);

// Update component terminals after movement
void circuit_update_component_nodes(Circuit *circuit, Component *comp);

// Copy/paste operations
void circuit_copy_component(Circuit *circuit, Component *comp);
void circuit_cut_component(Circuit *circuit, Component *comp);
Component *circuit_paste_component(Circuit *circuit, float x, float y);
Component *circuit_duplicate_component(Circuit *circuit, Component *comp);

// Selection operations
void circuit_select_all(Circuit *circuit);
void circuit_deselect_all(Circuit *circuit);
void circuit_delete_selected(Circuit *circuit);

// Undo operations
void circuit_push_undo(Circuit *circuit, UndoActionType type, int id, Component *backup, float old_x, float old_y);
bool circuit_undo(Circuit *circuit);
void circuit_clear_undo(Circuit *circuit);

// Serialization
bool circuit_save(Circuit *circuit, const char *filename);
bool circuit_load(Circuit *circuit, const char *filename);

#endif // CIRCUIT_H
