/**
 * Circuit Playground - File I/O Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "file_io.h"

static char error_message[256] = "";

const char *file_get_error(void) {
    return error_message;
}

static void set_error(const char *msg) {
    strncpy(error_message, msg, sizeof(error_message) - 1);
    error_message[sizeof(error_message) - 1] = '\0';
}

bool file_save_circuit(Circuit *circuit, const char *filename) {
    if (!circuit || !filename) {
        set_error("Invalid arguments");
        return false;
    }

    FILE *f = fopen(filename, "wb");
    if (!f) {
        set_error("Failed to open file for writing");
        return false;
    }

    // Write magic number and version
    uint32_t magic = CIRCUIT_FILE_MAGIC;
    uint32_t version = CIRCUIT_FILE_VERSION;
    fwrite(&magic, sizeof(magic), 1, f);
    fwrite(&version, sizeof(version), 1, f);

    // Write component count
    fwrite(&circuit->num_components, sizeof(int), 1, f);

    // Write each component
    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        fwrite(&comp->type, sizeof(ComponentType), 1, f);
        fwrite(&comp->x, sizeof(float), 1, f);
        fwrite(&comp->y, sizeof(float), 1, f);
        fwrite(&comp->rotation, sizeof(int), 1, f);
        fwrite(comp->label, MAX_LABEL_LEN, 1, f);
        fwrite(&comp->props, sizeof(ComponentProps), 1, f);
    }

    // Write node count
    fwrite(&circuit->num_nodes, sizeof(int), 1, f);

    // Write nodes
    for (int i = 0; i < circuit->num_nodes; i++) {
        Node *node = &circuit->nodes[i];
        fwrite(&node->id, sizeof(int), 1, f);
        fwrite(&node->x, sizeof(float), 1, f);
        fwrite(&node->y, sizeof(float), 1, f);
        fwrite(&node->is_ground, sizeof(bool), 1, f);
    }

    // Write wire count
    fwrite(&circuit->num_wires, sizeof(int), 1, f);

    // Write wires
    for (int i = 0; i < circuit->num_wires; i++) {
        Wire *wire = &circuit->wires[i];
        fwrite(&wire->start_node_id, sizeof(int), 1, f);
        fwrite(&wire->end_node_id, sizeof(int), 1, f);
    }

    fclose(f);
    return true;
}

bool file_load_circuit(Circuit *circuit, const char *filename) {
    if (!circuit || !filename) {
        set_error("Invalid arguments");
        return false;
    }

    FILE *f = fopen(filename, "rb");
    if (!f) {
        set_error("Failed to open file for reading");
        return false;
    }

    // Read and verify magic number
    uint32_t magic, version;
    fread(&magic, sizeof(magic), 1, f);
    if (magic != CIRCUIT_FILE_MAGIC) {
        set_error("Invalid file format");
        fclose(f);
        return false;
    }

    fread(&version, sizeof(version), 1, f);
    if (version > CIRCUIT_FILE_VERSION) {
        set_error("File version not supported");
        fclose(f);
        return false;
    }

    // Clear current circuit
    circuit_clear(circuit);

    // Read component count
    int num_components;
    fread(&num_components, sizeof(int), 1, f);

    // Read components
    for (int i = 0; i < num_components; i++) {
        ComponentType type;
        float x, y;
        int rotation;
        char label[MAX_LABEL_LEN];
        ComponentProps props;

        fread(&type, sizeof(ComponentType), 1, f);
        fread(&x, sizeof(float), 1, f);
        fread(&y, sizeof(float), 1, f);
        fread(&rotation, sizeof(int), 1, f);
        fread(label, MAX_LABEL_LEN, 1, f);
        fread(&props, sizeof(ComponentProps), 1, f);

        Component *comp = component_create(type, x, y);
        if (comp) {
            comp->rotation = rotation;
            strncpy(comp->label, label, MAX_LABEL_LEN);
            comp->props = props;
            circuit_add_component(circuit, comp);
        }
    }

    // Read node count
    fread(&circuit->num_nodes, sizeof(int), 1, f);

    // Read nodes
    for (int i = 0; i < circuit->num_nodes; i++) {
        Node *node = &circuit->nodes[i];
        fread(&node->id, sizeof(int), 1, f);
        fread(&node->x, sizeof(float), 1, f);
        fread(&node->y, sizeof(float), 1, f);
        fread(&node->is_ground, sizeof(bool), 1, f);

        if (node->is_ground) {
            circuit->ground_node_id = node->id;
        }
    }

    // Read wire count
    fread(&circuit->num_wires, sizeof(int), 1, f);

    // Read wires
    for (int i = 0; i < circuit->num_wires; i++) {
        Wire *wire = &circuit->wires[i];
        fread(&wire->start_node_id, sizeof(int), 1, f);
        fread(&wire->end_node_id, sizeof(int), 1, f);
        wire->id = circuit->next_wire_id++;
    }

    fclose(f);
    circuit->modified = false;
    return true;
}

bool file_export_json(Circuit *circuit, const char *filename) {
    if (!circuit || !filename) {
        set_error("Invalid arguments");
        return false;
    }

    FILE *f = fopen(filename, "w");
    if (!f) {
        set_error("Failed to open file for writing");
        return false;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"version\": %d,\n", CIRCUIT_FILE_VERSION);

    // Components
    fprintf(f, "  \"components\": [\n");
    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"type\": %d,\n", comp->type);
        fprintf(f, "      \"x\": %.2f,\n", comp->x);
        fprintf(f, "      \"y\": %.2f,\n", comp->y);
        fprintf(f, "      \"rotation\": %d,\n", comp->rotation);
        fprintf(f, "      \"label\": \"%s\"\n", comp->label);
        fprintf(f, "    }%s\n", i < circuit->num_components - 1 ? "," : "");
    }
    fprintf(f, "  ],\n");

    // Nodes
    fprintf(f, "  \"nodes\": [\n");
    for (int i = 0; i < circuit->num_nodes; i++) {
        Node *node = &circuit->nodes[i];
        fprintf(f, "    {\"id\": %d, \"x\": %.2f, \"y\": %.2f, \"ground\": %s}%s\n",
                node->id, node->x, node->y,
                node->is_ground ? "true" : "false",
                i < circuit->num_nodes - 1 ? "," : "");
    }
    fprintf(f, "  ],\n");

    // Wires
    fprintf(f, "  \"wires\": [\n");
    for (int i = 0; i < circuit->num_wires; i++) {
        Wire *wire = &circuit->wires[i];
        fprintf(f, "    {\"start\": %d, \"end\": %d}%s\n",
                wire->start_node_id, wire->end_node_id,
                i < circuit->num_wires - 1 ? "," : "");
    }
    fprintf(f, "  ]\n");

    fprintf(f, "}\n");

    fclose(f);
    return true;
}

bool file_import_json(Circuit *circuit, const char *filename) {
    // Simplified JSON parser - for production use a proper JSON library
    if (!circuit || !filename) {
        set_error("Invalid arguments");
        return false;
    }

    FILE *f = fopen(filename, "r");
    if (!f) {
        set_error("Failed to open file for reading");
        return false;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer) {
        set_error("Memory allocation failed");
        fclose(f);
        return false;
    }

    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);

    // Very basic JSON parsing
    // In production, use a proper JSON library like cJSON

    circuit_clear(circuit);

    // Parse components (simplified)
    char *ptr = strstr(buffer, "\"components\"");
    if (ptr) {
        // Find each component object
        while ((ptr = strstr(ptr, "\"type\":")) != NULL) {
            int type;
            float x, y;
            int rotation;

            if (sscanf(ptr, "\"type\": %d", &type) == 1) {
                char *x_ptr = strstr(ptr, "\"x\":");
                char *y_ptr = strstr(ptr, "\"y\":");
                char *rot_ptr = strstr(ptr, "\"rotation\":");

                if (x_ptr && sscanf(x_ptr, "\"x\": %f", &x) == 1 &&
                    y_ptr && sscanf(y_ptr, "\"y\": %f", &y) == 1) {

                    rotation = 0;
                    if (rot_ptr) sscanf(rot_ptr, "\"rotation\": %d", &rotation);

                    Component *comp = component_create(type, x, y);
                    if (comp) {
                        comp->rotation = rotation;
                        circuit_add_component(circuit, comp);
                    }
                }
            }
            ptr++;
        }
    }

    // Parse nodes
    ptr = strstr(buffer, "\"nodes\"");
    if (ptr) {
        while ((ptr = strstr(ptr, "\"id\":")) != NULL) {
            int id;
            float x, y;
            bool is_ground = false;

            if (sscanf(ptr, "\"id\": %d", &id) == 1) {
                char *x_ptr = strstr(ptr, "\"x\":");
                char *y_ptr = strstr(ptr, "\"y\":");
                char *gnd_ptr = strstr(ptr, "\"ground\":");

                if (x_ptr && sscanf(x_ptr, "\"x\": %f", &x) == 1 &&
                    y_ptr && sscanf(y_ptr, "\"y\": %f", &y) == 1) {

                    if (gnd_ptr && strstr(gnd_ptr, "true")) {
                        is_ground = true;
                    }

                    int node_id = circuit_create_node(circuit, x, y);
                    if (is_ground) {
                        circuit_set_ground(circuit, node_id);
                    }
                }
            }
            ptr++;
        }
    }

    // Parse wires
    ptr = strstr(buffer, "\"wires\"");
    if (ptr) {
        while ((ptr = strstr(ptr, "\"start\":")) != NULL) {
            int start, end;

            if (sscanf(ptr, "\"start\": %d", &start) == 1) {
                char *end_ptr = strstr(ptr, "\"end\":");
                if (end_ptr && sscanf(end_ptr, "\"end\": %d", &end) == 1) {
                    circuit_add_wire(circuit, start, end);
                }
            }
            ptr++;
        }
    }

    free(buffer);
    circuit->modified = false;
    return true;
}
