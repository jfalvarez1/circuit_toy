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

// ============================================================================
// SVG Export Implementation
// ============================================================================

// Helper to apply rotation transformation to a point
static void svg_rotate_point(float *px, float *py, float cx, float cy, int rotation) {
    float x = *px - cx;
    float y = *py - cy;
    float angle = rotation * M_PI / 180.0f;
    float cosA = cosf(angle);
    float sinA = sinf(angle);
    *px = cx + x * cosA - y * sinA;
    *py = cy + x * sinA + y * cosA;
}

// Get component type name for SVG comments
static const char* svg_component_name(ComponentType type) {
    switch (type) {
        case COMP_RESISTOR: return "Resistor";
        case COMP_CAPACITOR: return "Capacitor";
        case COMP_CAPACITOR_ELEC: return "Electrolytic Capacitor";
        case COMP_INDUCTOR: return "Inductor";
        case COMP_DIODE: return "Diode";
        case COMP_LED: return "LED";
        case COMP_ZENER: return "Zener Diode";
        case COMP_NPN_BJT: return "NPN BJT";
        case COMP_PNP_BJT: return "PNP BJT";
        case COMP_NMOS: return "NMOS";
        case COMP_PMOS: return "PMOS";
        case COMP_OPAMP: return "Op-Amp";
        case COMP_DC_VOLTAGE: return "DC Voltage Source";
        case COMP_AC_VOLTAGE: return "AC Voltage Source";
        case COMP_GROUND: return "Ground";
        case COMP_FUSE: return "Fuse";
        case COMP_SPST_SWITCH: return "SPST Switch";
        case COMP_SPDT_SWITCH: return "SPDT Switch";
        case COMP_POTENTIOMETER: return "Potentiometer";
        case COMP_TRANSFORMER: return "Transformer";
        case COMP_555_TIMER: return "555 Timer";
        default: return "Component";
    }
}

// Write SVG line with rotation
static void svg_write_line(FILE *f, float cx, float cy, float x1, float y1, float x2, float y2, int rotation) {
    float px1 = cx + x1, py1 = cy + y1;
    float px2 = cx + x2, py2 = cy + y2;
    svg_rotate_point(&px1, &py1, cx, cy, rotation);
    svg_rotate_point(&px2, &py2, cx, cy, rotation);
    fprintf(f, "    <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\"/>\n", px1, py1, px2, py2);
}

// Write SVG circle with rotation
static void svg_write_circle(FILE *f, float cx, float cy, float dx, float dy, float r, int rotation) {
    float px = cx + dx, py = cy + dy;
    svg_rotate_point(&px, &py, cx, cy, rotation);
    fprintf(f, "    <circle cx=\"%.1f\" cy=\"%.1f\" r=\"%.1f\"/>\n", px, py, r);
}

// Write SVG arc path
static void svg_write_arc(FILE *f, float cx, float cy, float startX, float startY,
                          float endX, float endY, float r, int sweepFlag, int rotation) {
    float px1 = cx + startX, py1 = cy + startY;
    float px2 = cx + endX, py2 = cy + endY;
    svg_rotate_point(&px1, &py1, cx, cy, rotation);
    svg_rotate_point(&px2, &py2, cx, cy, rotation);
    fprintf(f, "    <path d=\"M%.1f,%.1f A%.1f,%.1f 0 0,%d %.1f,%.1f\"/>\n",
            px1, py1, r, r, sweepFlag, px2, py2);
}

// SVG component renderers
static void svg_render_resistor(FILE *f, float x, float y, int rotation) {
    // Leads
    svg_write_line(f, x, y, -40, 0, -28, 0, rotation);
    svg_write_line(f, x, y, 28, 0, 40, 0, rotation);
    // Zigzag
    int points[][2] = {{-28,0},{-21,-8},{-7,8},{7,-8},{21,8},{28,0}};
    for (int i = 0; i < 5; i++) {
        svg_write_line(f, x, y, points[i][0], points[i][1], points[i+1][0], points[i+1][1], rotation);
    }
}

static void svg_render_capacitor(FILE *f, float x, float y, int rotation) {
    svg_write_line(f, x, y, -40, 0, -6, 0, rotation);
    svg_write_line(f, x, y, -6, -14, -6, 14, rotation);
    svg_write_line(f, x, y, 6, -14, 6, 14, rotation);
    svg_write_line(f, x, y, 6, 0, 40, 0, rotation);
}

static void svg_render_capacitor_elec(FILE *f, float x, float y, int rotation) {
    svg_write_line(f, x, y, -40, 0, -6, 0, rotation);
    svg_write_line(f, x, y, -6, -14, -6, 14, rotation);
    // Curved plate for electrolytic
    svg_write_arc(f, x, y, 6, -14, 6, 14, 10, 1, rotation);
    svg_write_line(f, x, y, 6, 0, 40, 0, rotation);
    // + sign on positive side
    svg_write_line(f, x, y, -25, -8, -25, -2, rotation);
    svg_write_line(f, x, y, -28, -5, -22, -5, rotation);
}

static void svg_render_inductor(FILE *f, float x, float y, int rotation) {
    svg_write_line(f, x, y, -40, 0, -28, 0, rotation);
    svg_write_line(f, x, y, 28, 0, 40, 0, rotation);
    // 4 half-circle coils
    for (int i = 0; i < 4; i++) {
        float coil_cx = -21 + i * 14;
        svg_write_arc(f, x, y, coil_cx - 7, 0, coil_cx + 7, 0, 7, 0, rotation);
    }
}

static void svg_render_diode(FILE *f, float x, float y, int rotation) {
    svg_write_line(f, x, y, -40, 0, -10, 0, rotation);
    svg_write_line(f, x, y, 10, 0, 40, 0, rotation);
    // Triangle
    svg_write_line(f, x, y, -10, -12, -10, 12, rotation);
    svg_write_line(f, x, y, -10, -12, 10, 0, rotation);
    svg_write_line(f, x, y, -10, 12, 10, 0, rotation);
    // Bar
    svg_write_line(f, x, y, 10, -12, 10, 12, rotation);
}

static void svg_render_led(FILE *f, float x, float y, int rotation) {
    svg_render_diode(f, x, y, rotation);
    // Light arrows
    svg_write_line(f, x, y, -5, -18, 5, -28, rotation);
    svg_write_line(f, x, y, 5, -28, 2, -25, rotation);
    svg_write_line(f, x, y, 5, -28, 5, -24, rotation);
    svg_write_line(f, x, y, 5, -18, 15, -28, rotation);
    svg_write_line(f, x, y, 15, -28, 12, -25, rotation);
    svg_write_line(f, x, y, 15, -28, 15, -24, rotation);
}

static void svg_render_zener(FILE *f, float x, float y, int rotation) {
    svg_write_line(f, x, y, -40, 0, -10, 0, rotation);
    svg_write_line(f, x, y, 10, 0, 40, 0, rotation);
    // Triangle
    svg_write_line(f, x, y, -10, -12, -10, 12, rotation);
    svg_write_line(f, x, y, -10, -12, 10, 0, rotation);
    svg_write_line(f, x, y, -10, 12, 10, 0, rotation);
    // Zener bar with wings
    svg_write_line(f, x, y, 6, -16, 10, -12, rotation);
    svg_write_line(f, x, y, 10, -12, 10, 12, rotation);
    svg_write_line(f, x, y, 10, 12, 14, 16, rotation);
}

static void svg_render_npn_bjt(FILE *f, float x, float y, int rotation) {
    // Circle outline
    svg_write_circle(f, x, y, 5, 0, 25, rotation);
    // Base line
    svg_write_line(f, x, y, -40, 0, -10, 0, rotation);
    // Emitter bar
    svg_write_line(f, x, y, -10, -15, -10, 15, rotation);
    // Collector
    svg_write_line(f, x, y, -10, -8, 15, -25, rotation);
    svg_write_line(f, x, y, 15, -25, 15, -40, rotation);
    // Emitter with arrow
    svg_write_line(f, x, y, -10, 8, 15, 25, rotation);
    svg_write_line(f, x, y, 15, 25, 15, 40, rotation);
    // Arrow
    svg_write_line(f, x, y, 8, 18, 15, 25, rotation);
    svg_write_line(f, x, y, 5, 25, 15, 25, rotation);
}

static void svg_render_pnp_bjt(FILE *f, float x, float y, int rotation) {
    // Circle outline
    svg_write_circle(f, x, y, 5, 0, 25, rotation);
    // Base line
    svg_write_line(f, x, y, -40, 0, -10, 0, rotation);
    // Emitter bar
    svg_write_line(f, x, y, -10, -15, -10, 15, rotation);
    // Collector
    svg_write_line(f, x, y, -10, -8, 15, -25, rotation);
    svg_write_line(f, x, y, 15, -25, 15, -40, rotation);
    // Emitter
    svg_write_line(f, x, y, -10, 8, 15, 25, rotation);
    svg_write_line(f, x, y, 15, 25, 15, 40, rotation);
    // Arrow pointing inward
    svg_write_line(f, x, y, -10, 8, -3, 11, rotation);
    svg_write_line(f, x, y, -10, 8, -7, 15, rotation);
}

static void svg_render_nmos(FILE *f, float x, float y, int rotation) {
    // Gate
    svg_write_line(f, x, y, -40, 0, -15, 0, rotation);
    svg_write_line(f, x, y, -15, -20, -15, 20, rotation);
    // Channel
    svg_write_line(f, x, y, -8, -20, -8, 20, rotation);
    // Drain
    svg_write_line(f, x, y, -8, -15, 20, -15, rotation);
    svg_write_line(f, x, y, 20, -15, 20, -40, rotation);
    // Source
    svg_write_line(f, x, y, -8, 15, 20, 15, rotation);
    svg_write_line(f, x, y, 20, 15, 20, 40, rotation);
    // Body
    svg_write_line(f, x, y, -8, 0, 20, 0, rotation);
    svg_write_line(f, x, y, 20, 0, 20, 15, rotation);
    // Arrow
    svg_write_line(f, x, y, 10, 0, 15, -4, rotation);
    svg_write_line(f, x, y, 10, 0, 15, 4, rotation);
}

static void svg_render_pmos(FILE *f, float x, float y, int rotation) {
    // Gate
    svg_write_line(f, x, y, -40, 0, -18, 0, rotation);
    svg_write_circle(f, x, y, -16, 0, 3, rotation);
    svg_write_line(f, x, y, -12, -20, -12, 20, rotation);
    // Channel
    svg_write_line(f, x, y, -5, -20, -5, 20, rotation);
    // Drain
    svg_write_line(f, x, y, -5, -15, 20, -15, rotation);
    svg_write_line(f, x, y, 20, -15, 20, -40, rotation);
    // Source
    svg_write_line(f, x, y, -5, 15, 20, 15, rotation);
    svg_write_line(f, x, y, 20, 15, 20, 40, rotation);
    // Body
    svg_write_line(f, x, y, -5, 0, 20, 0, rotation);
    svg_write_line(f, x, y, 20, 0, 20, 15, rotation);
    // Arrow pointing out
    svg_write_line(f, x, y, 5, 0, 0, -4, rotation);
    svg_write_line(f, x, y, 5, 0, 0, 4, rotation);
}

static void svg_render_opamp(FILE *f, float x, float y, int rotation) {
    // Triangle body
    svg_write_line(f, x, y, -30, -35, -30, 35, rotation);
    svg_write_line(f, x, y, -30, -35, 30, 0, rotation);
    svg_write_line(f, x, y, -30, 35, 30, 0, rotation);
    // Inputs
    svg_write_line(f, x, y, -40, -20, -30, -20, rotation);  // + (non-inverting)
    svg_write_line(f, x, y, -40, 20, -30, 20, rotation);   // - (inverting)
    // Output
    svg_write_line(f, x, y, 30, 0, 40, 0, rotation);
    // + sign
    svg_write_line(f, x, y, -25, -20, -19, -20, rotation);
    svg_write_line(f, x, y, -22, -23, -22, -17, rotation);
    // - sign
    svg_write_line(f, x, y, -25, 20, -19, 20, rotation);
}

static void svg_render_dc_voltage(FILE *f, float x, float y, int rotation) {
    svg_write_line(f, x, y, 0, -40, 0, -15, rotation);
    svg_write_circle(f, x, y, 0, 0, 15, rotation);
    svg_write_line(f, x, y, 0, 15, 0, 40, rotation);
    // + sign at top
    svg_write_line(f, x, y, -4, -8, 4, -8, rotation);
    svg_write_line(f, x, y, 0, -12, 0, -4, rotation);
    // - sign at bottom
    svg_write_line(f, x, y, -4, 8, 4, 8, rotation);
}

static void svg_render_ac_voltage(FILE *f, float x, float y, int rotation) {
    svg_write_line(f, x, y, 0, -40, 0, -15, rotation);
    svg_write_circle(f, x, y, 0, 0, 15, rotation);
    svg_write_line(f, x, y, 0, 15, 0, 40, rotation);
    // Sine wave symbol inside
    svg_write_arc(f, x, y, -8, 0, 0, -6, 5, 1, rotation);
    svg_write_arc(f, x, y, 0, -6, 8, 0, 5, 0, rotation);
}

static void svg_render_ground(FILE *f, float x, float y, int rotation) {
    svg_write_line(f, x, y, 0, -40, 0, 0, rotation);
    svg_write_line(f, x, y, -15, 0, 15, 0, rotation);
    svg_write_line(f, x, y, -10, 6, 10, 6, rotation);
    svg_write_line(f, x, y, -5, 12, 5, 12, rotation);
}

static void svg_render_fuse(FILE *f, float x, float y, int rotation) {
    svg_write_line(f, x, y, -40, 0, -20, 0, rotation);
    // Fuse body rectangle outline
    svg_write_line(f, x, y, -20, -8, 20, -8, rotation);
    svg_write_line(f, x, y, -20, 8, 20, 8, rotation);
    svg_write_line(f, x, y, -20, -8, -20, 8, rotation);
    svg_write_line(f, x, y, 20, -8, 20, 8, rotation);
    // Fuse element
    svg_write_line(f, x, y, -15, 0, 15, 0, rotation);
    svg_write_line(f, x, y, 20, 0, 40, 0, rotation);
}

static void svg_render_switch_spst(FILE *f, float x, float y, int rotation) {
    svg_write_line(f, x, y, -40, 0, -15, 0, rotation);
    svg_write_circle(f, x, y, -12, 0, 3, rotation);
    svg_write_circle(f, x, y, 12, 0, 3, rotation);
    svg_write_line(f, x, y, 15, 0, 40, 0, rotation);
    // Switch arm (open position)
    svg_write_line(f, x, y, -12, 0, 10, -12, rotation);
}

static void svg_render_potentiometer(FILE *f, float x, float y, int rotation) {
    svg_render_resistor(f, x, y, rotation);
    // Wiper arrow
    svg_write_line(f, x, y, 0, 40, 0, 10, rotation);
    svg_write_line(f, x, y, -5, 15, 0, 10, rotation);
    svg_write_line(f, x, y, 5, 15, 0, 10, rotation);
}

static void svg_render_transformer(FILE *f, float x, float y, int rotation) {
    // Primary coils
    for (int i = 0; i < 4; i++) {
        float coil_cy = -21 + i * 14;
        svg_write_arc(f, x, y, -10, coil_cy - 7, -10, coil_cy + 7, 7, 1, rotation);
    }
    // Core lines
    svg_write_line(f, x, y, -3, -28, -3, 28, rotation);
    svg_write_line(f, x, y, 3, -28, 3, 28, rotation);
    // Secondary coils
    for (int i = 0; i < 4; i++) {
        float coil_cy = -21 + i * 14;
        svg_write_arc(f, x, y, 10, coil_cy - 7, 10, coil_cy + 7, 7, 0, rotation);
    }
    // Leads
    svg_write_line(f, x, y, -10, -28, -10, -40, rotation);
    svg_write_line(f, x, y, -10, 28, -10, 40, rotation);
    svg_write_line(f, x, y, 10, -28, 10, -40, rotation);
    svg_write_line(f, x, y, 10, 28, 10, 40, rotation);
}

static void svg_render_generic_ic(FILE *f, float x, float y, int rotation, const char *label) {
    // IC box
    svg_write_line(f, x, y, -25, -30, 25, -30, rotation);
    svg_write_line(f, x, y, -25, 30, 25, 30, rotation);
    svg_write_line(f, x, y, -25, -30, -25, 30, rotation);
    svg_write_line(f, x, y, 25, -30, 25, 30, rotation);
    // Notch
    svg_write_arc(f, x, y, -5, -30, 5, -30, 5, 1, rotation);
}

// Main component renderer dispatch
static void svg_render_component(FILE *f, Component *comp) {
    float x = comp->x;
    float y = comp->y;
    int rot = comp->rotation;

    fprintf(f, "  <!-- %s: %s -->\n", svg_component_name(comp->type), comp->label[0] ? comp->label : "unlabeled");
    fprintf(f, "  <g class=\"component\" data-type=\"%d\">\n", comp->type);

    switch (comp->type) {
        case COMP_RESISTOR:
            svg_render_resistor(f, x, y, rot);
            break;
        case COMP_CAPACITOR:
            svg_render_capacitor(f, x, y, rot);
            break;
        case COMP_CAPACITOR_ELEC:
            svg_render_capacitor_elec(f, x, y, rot);
            break;
        case COMP_INDUCTOR:
            svg_render_inductor(f, x, y, rot);
            break;
        case COMP_DIODE:
        case COMP_SCHOTTKY:
            svg_render_diode(f, x, y, rot);
            break;
        case COMP_LED:
            svg_render_led(f, x, y, rot);
            break;
        case COMP_ZENER:
            svg_render_zener(f, x, y, rot);
            break;
        case COMP_NPN_BJT:
            svg_render_npn_bjt(f, x, y, rot);
            break;
        case COMP_PNP_BJT:
            svg_render_pnp_bjt(f, x, y, rot);
            break;
        case COMP_NMOS:
            svg_render_nmos(f, x, y, rot);
            break;
        case COMP_PMOS:
            svg_render_pmos(f, x, y, rot);
            break;
        case COMP_OPAMP:
        case COMP_OPAMP_FLIPPED:
            svg_render_opamp(f, x, y, rot);
            break;
        case COMP_DC_VOLTAGE:
        case COMP_DC_CURRENT:
            svg_render_dc_voltage(f, x, y, rot);
            break;
        case COMP_AC_VOLTAGE:
        case COMP_AC_CURRENT:
            svg_render_ac_voltage(f, x, y, rot);
            break;
        case COMP_GROUND:
            svg_render_ground(f, x, y, rot);
            break;
        case COMP_FUSE:
            svg_render_fuse(f, x, y, rot);
            break;
        case COMP_SPST_SWITCH:
        case COMP_PUSH_BUTTON:
            svg_render_switch_spst(f, x, y, rot);
            break;
        case COMP_POTENTIOMETER:
            svg_render_potentiometer(f, x, y, rot);
            break;
        case COMP_TRANSFORMER:
            svg_render_transformer(f, x, y, rot);
            break;
        case COMP_555_TIMER:
            svg_render_generic_ic(f, x, y, rot, "555");
            break;
        default:
            // Generic box for unsupported components
            svg_write_line(f, x, y, -20, -20, 20, -20, rot);
            svg_write_line(f, x, y, -20, 20, 20, 20, rot);
            svg_write_line(f, x, y, -20, -20, -20, 20, rot);
            svg_write_line(f, x, y, 20, -20, 20, 20, rot);
            break;
    }

    // Add label if present
    if (comp->label[0]) {
        float lx = x, ly = y + 35;
        svg_rotate_point(&lx, &ly, x, y, rot);
        fprintf(f, "    <text x=\"%.1f\" y=\"%.1f\" text-anchor=\"middle\" class=\"label\">%s</text>\n",
                lx, ly, comp->label);
    }

    fprintf(f, "  </g>\n");
}

bool file_export_svg(Circuit *circuit, const char *filename) {
    if (!circuit || !filename) {
        set_error("Invalid arguments");
        return false;
    }

    FILE *f = fopen(filename, "w");
    if (!f) {
        set_error("Failed to open file for writing");
        return false;
    }

    // Calculate bounding box
    float min_x = 1e9f, min_y = 1e9f, max_x = -1e9f, max_y = -1e9f;

    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        float margin = 50;  // Component size margin
        if (comp->x - margin < min_x) min_x = comp->x - margin;
        if (comp->y - margin < min_y) min_y = comp->y - margin;
        if (comp->x + margin > max_x) max_x = comp->x + margin;
        if (comp->y + margin > max_y) max_y = comp->y + margin;
    }

    for (int i = 0; i < circuit->num_nodes; i++) {
        Node *node = &circuit->nodes[i];
        if (node->x < min_x) min_x = node->x;
        if (node->y < min_y) min_y = node->y;
        if (node->x > max_x) max_x = node->x;
        if (node->y > max_y) max_y = node->y;
    }

    // Add padding
    float padding = 40;
    min_x -= padding;
    min_y -= padding;
    max_x += padding;
    max_y += padding;

    float width = max_x - min_x;
    float height = max_y - min_y;

    // Ensure minimum size
    if (width < 200) width = 200;
    if (height < 200) height = 200;

    // Write SVG header
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"%.0f %.0f %.0f %.0f\" width=\"%.0f\" height=\"%.0f\">\n",
            min_x, min_y, width, height, width, height);

    // Style definitions
    fprintf(f, "  <defs>\n");
    fprintf(f, "    <style>\n");
    fprintf(f, "      line, path, circle { stroke: #00d9ff; stroke-width: 2; fill: none; }\n");
    fprintf(f, "      .wire { stroke: #00d9ff; stroke-width: 2; }\n");
    fprintf(f, "      .junction { fill: #00d9ff; }\n");
    fprintf(f, "      .label { font-family: Arial, sans-serif; font-size: 12px; fill: #ffffff; }\n");
    fprintf(f, "      .component circle { fill: none; }\n");
    fprintf(f, "      text { font-family: Arial, sans-serif; font-size: 10px; fill: #b0b0b0; }\n");
    fprintf(f, "    </style>\n");
    fprintf(f, "  </defs>\n");

    // Background
    fprintf(f, "  <rect x=\"%.0f\" y=\"%.0f\" width=\"%.0f\" height=\"%.0f\" fill=\"#1a1a2e\"/>\n",
            min_x, min_y, width, height);

    // Render wires
    fprintf(f, "\n  <!-- Wires -->\n");
    for (int i = 0; i < circuit->num_wires; i++) {
        Wire *wire = &circuit->wires[i];
        Node *start = circuit_get_node(circuit, wire->start_node_id);
        Node *end = circuit_get_node(circuit, wire->end_node_id);
        if (start && end) {
            fprintf(f, "  <line x1=\"%.1f\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" class=\"wire\"/>\n",
                    start->x, start->y, end->x, end->y);
        }
    }

    // Render junction points (nodes with multiple connections)
    fprintf(f, "\n  <!-- Junction points -->\n");
    for (int i = 0; i < circuit->num_nodes; i++) {
        Node *node = &circuit->nodes[i];
        // Count connections to this node
        int connections = 0;
        for (int j = 0; j < circuit->num_wires; j++) {
            Wire *wire = &circuit->wires[j];
            if (wire->start_node_id == node->id || wire->end_node_id == node->id) {
                connections++;
            }
        }
        // Draw junction dot if 3+ connections
        if (connections >= 3) {
            fprintf(f, "  <circle cx=\"%.1f\" cy=\"%.1f\" r=\"4\" class=\"junction\"/>\n",
                    node->x, node->y);
        }
    }

    // Render components
    fprintf(f, "\n  <!-- Components -->\n");
    for (int i = 0; i < circuit->num_components; i++) {
        svg_render_component(f, circuit->components[i]);
    }

    fprintf(f, "</svg>\n");
    fclose(f);
    return true;
}
