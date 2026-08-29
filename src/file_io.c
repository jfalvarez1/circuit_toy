/**
 * Circuit Playground - File I/O Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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
        /* which node each terminal is on (format 2). Without this the connections that are not
           re-derivable from where the terminals sit are simply lost. */
        fwrite(&comp->num_terminals, sizeof(int), 1, f);
        fwrite(comp->node_ids, sizeof(int), MAX_TERMINALS, f);
        fwrite(comp->part, sizeof comp->part, 1, f);   /* the named device, if one was applied */
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

    // Write probe count
    fwrite(&circuit->num_probes, sizeof(int), 1, f);

    // Write probes
    for (int i = 0; i < circuit->num_probes; i++) {
        Probe *probe = &circuit->probes[i];
        fwrite(&probe->id, sizeof(int), 1, f);
        fwrite(&probe->node_id, sizeof(int), 1, f);
        fwrite(&probe->x, sizeof(float), 1, f);
        fwrite(&probe->y, sizeof(float), 1, f);
        fwrite(&probe->channel_num, sizeof(int), 1, f);
        fwrite(probe->label, 8, 1, f);  // Label is char[8]
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

        int saved_terminals = 0;
        int saved_nodes[MAX_TERMINALS];
        for (int k = 0; k < MAX_TERMINALS; k++) saved_nodes[k] = -1;
        char part[16] = "";
        if (version >= 2) {
            fread(&saved_terminals, sizeof(int), 1, f);
            fread(saved_nodes, sizeof(int), MAX_TERMINALS, f);
            fread(part, sizeof part, 1, f);
            part[sizeof part - 1] = 0;
        }

        Component *comp = component_create(type, x, y);
        if (comp) {
            comp->rotation = rotation;
            strncpy(comp->label, label, MAX_LABEL_LEN);
            component_adopt_props(comp, &props);
            if (version >= 2) memcpy(comp->part, part, sizeof comp->part);
            circuit_add_component(circuit, comp);
            /* After, not before: circuit_add_component assigns every terminal to whatever node
               sits at its position, which is exactly the guess this is here to replace. The
               node table read below is the one these ids refer to. */
            if (version >= 2) {
                int n = saved_terminals < comp->num_terminals ? saved_terminals : comp->num_terminals;
                for (int k = 0; k < n; k++) comp->node_ids[k] = saved_nodes[k];
            }
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

    // Read probe count (if available in file)
    int num_probes = 0;
    if (fread(&num_probes, sizeof(int), 1, f) == 1 && num_probes > 0 && num_probes < MAX_PROBES) {
        // Read probes
        for (int i = 0; i < num_probes; i++) {
            Probe *probe = &circuit->probes[circuit->num_probes];
            fread(&probe->id, sizeof(int), 1, f);
            fread(&probe->node_id, sizeof(int), 1, f);
            fread(&probe->x, sizeof(float), 1, f);
            fread(&probe->y, sizeof(float), 1, f);
            fread(&probe->channel_num, sizeof(int), 1, f);
            fread(probe->label, 8, 1, f);

            // Set probe color based on channel
            if (probe->channel_num == 0) probe->color = (Color){0xff, 0xff, 0x00, 0xff};  // Yellow
            else if (probe->channel_num == 1) probe->color = (Color){0x00, 0xff, 0xff, 0xff};  // Cyan
            else if (probe->channel_num == 2) probe->color = (Color){0xff, 0x00, 0xff, 0xff};  // Magenta
            else probe->color = (Color){0xff, 0xff, 0xff, 0xff};  // White

            probe->selected = false;
            circuit->num_probes++;
        }
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
        fprintf(f, "      \"label\": \"%s\"", comp->label);

        // Add component-specific properties
        bool has_props = false;
        if (comp->type == COMP_RESISTOR) {
            fprintf(f, ",\n");
            fprintf(f, "      \"properties\": {\n");
            fprintf(f, "        \"resistance\": %.6e,\n", comp->props.resistor.resistance);
            fprintf(f, "        \"high_power\": %d\n", comp->props.resistor.high_power ? 1 : 0);
            fprintf(f, "      }");
            has_props = true;
        } else if (comp->type == COMP_CAPACITOR) {
            fprintf(f, ",\n");
            fprintf(f, "      \"properties\": {\n");
            fprintf(f, "        \"capacitance\": %.6e\n", comp->props.capacitor.capacitance);
            fprintf(f, "      }");
            has_props = true;
        } else if (comp->type == COMP_SPARK_GAP) {
            fprintf(f, ",\n");
            fprintf(f, "      \"properties\": {\n");
            fprintf(f, "        \"gap_mm\": %.6e,\n", comp->props.spark_gap.gap_mm);
            fprintf(f, "        \"r_on\": %.6e\n", comp->props.spark_gap.r_on);
            fprintf(f, "      }");
            has_props = true;
        } else if (comp->type == COMP_SOURCE_3PH) {
            fprintf(f, ",\n");
            fprintf(f, "      \"properties\": {\n");
            fprintf(f, "        \"v_peak\": %.6e,\n", comp->props.source_3ph.v_peak);
            fprintf(f, "        \"frequency\": %.6e,\n", comp->props.source_3ph.frequency);
            fprintf(f, "        \"phase\": %.6e,\n", comp->props.source_3ph.phase);
            fprintf(f, "        \"r_series\": %.6e,\n", comp->props.source_3ph.r_series);
            fprintf(f, "        \"l_series\": %.6e\n", comp->props.source_3ph.l_series);
            fprintf(f, "      }");
            has_props = true;
        } else if (comp->type == COMP_TLINE) {
            fprintf(f, ",\n");
            fprintf(f, "      \"properties\": {\n");
            fprintf(f, "        \"length_mi\": %.6e,\n", comp->props.tline.length_mi);
            fprintf(f, "        \"r_per_mi\": %.6e,\n", comp->props.tline.r_per_mi);
            fprintf(f, "        \"x_per_mi\": %.6e,\n", comp->props.tline.x_per_mi);
            fprintf(f, "        \"b_us_per_mi\": %.6e,\n", comp->props.tline.b_us_per_mi);
            fprintf(f, "        \"model\": %d\n", comp->props.tline.model);
            fprintf(f, "      }");
            has_props = true;
        } else if (comp->type == COMP_TOROID) {
            fprintf(f, ",\n");
            fprintf(f, "      \"properties\": {\n");
            fprintf(f, "        \"major_in\": %.6e,\n", comp->props.toroid.major_in);
            fprintf(f, "        \"minor_in\": %.6e\n", comp->props.toroid.minor_in);
            fprintf(f, "      }");
            has_props = true;
        } else if (comp->type == COMP_OPAMP || comp->type == COMP_OPAMP_FLIPPED || comp->type == COMP_OPAMP_REAL) {
            fprintf(f, ",\n");
            fprintf(f, "      \"properties\": {\n");
            fprintf(f, "        \"gain\": %.6e,\n", comp->props.opamp.gain);
            fprintf(f, "        \"gbw\": %.6e,\n", comp->props.opamp.gbw);
            fprintf(f, "        \"voffset\": %.6e\n", comp->props.opamp.voffset);
            fprintf(f, "      }");
            has_props = true;
        } else if (comp->type == COMP_AC_VOLTAGE) {
            fprintf(f, ",\n");
            fprintf(f, "      \"properties\": {\n");
            fprintf(f, "        \"amplitude\": %.6e,\n", comp->props.ac_voltage.amplitude);
            fprintf(f, "        \"frequency\": %.6e,\n", comp->props.ac_voltage.frequency);
            fprintf(f, "        \"offset\": %.6e\n", comp->props.ac_voltage.offset);
            fprintf(f, "      }");
            has_props = true;
        } else if (comp->type == COMP_SPST_SWITCH) {
            fprintf(f, ",\n");
            fprintf(f, "      \"properties\": {\n");
            fprintf(f, "        \"closed\": %s\n", comp->props.switch_spst.closed ? "true" : "false");
            fprintf(f, "      }");
            has_props = true;
        } else if (comp->type == COMP_LED_ARRAY) {
            fprintf(f, ",\n");
            fprintf(f, "      \"properties\": {\n");
            fprintf(f, "        \"color\": %d\n", comp->props.led_array.color);
            fprintf(f, "      }");
            has_props = true;
        } else if (comp->type == COMP_PULSE_SOURCE) {
            fprintf(f, ",\n");
            fprintf(f, "      \"properties\": {\n");
            fprintf(f, "        \"v_low\": %.6e,\n", comp->props.pulse_source.v_low);
            fprintf(f, "        \"v_high\": %.6e,\n", comp->props.pulse_source.v_high);
            fprintf(f, "        \"period\": %.6e,\n", comp->props.pulse_source.period);
            fprintf(f, "        \"pulse_width\": %.6e\n", comp->props.pulse_source.pulse_width);
            fprintf(f, "      }");
            has_props = true;
        }

        /* The readable fields above are a handful per type, chosen by hand, and everything they
           leave out came back as the component's default: a 10 mH inductor reloaded as 1 mH, a
           12 V supply as 5 V. Rather than hand-write every field of every part - which is how
           it drifted in the first place - the whole property block goes out as bytes alongside
           them, with the terminals' nodes and the part number. The readable fields stay for
           anything reading these files; the state is what the app loads. */
        fprintf(f, ",\n      \"state\": \"");
        const unsigned char *pb = (const unsigned char *)&comp->props;
        for (size_t k = 0; k < sizeof comp->props; k++) fprintf(f, "%02x", pb[k]);
        fprintf(f, "\",\n      \"part\": \"%s\",\n      \"terminals\": [", comp->part);
        for (int k = 0; k < comp->num_terminals; k++)
            fprintf(f, "%d%s", comp->node_ids[k], k < comp->num_terminals - 1 ? ", " : "");
        fprintf(f, "]");

        fprintf(f, "\n    }%s\n", i < circuit->num_components - 1 ? "," : "");
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
    fprintf(f, "  ],\n");

    // Probes
    fprintf(f, "  \"probes\": [\n");
    for (int i = 0; i < circuit->num_probes; i++) {
        Probe *probe = &circuit->probes[i];
        fprintf(f, "    {\"id\": %d, \"node_id\": %d, \"x\": %.2f, \"y\": %.2f, \"channel\": %d, \"label\": \"%s\"}%s\n",
                probe->id, probe->node_id, probe->x, probe->y,
                probe->channel_num, probe->label,
                i < circuit->num_probes - 1 ? "," : "");
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

                        // Parse component-specific properties
                        char *props_ptr = strstr(ptr, "\"properties\":");
                        if (props_ptr) {
                            // Limit search to this component's object (before next "type": or end)
                            char *next_comp = strstr(ptr + 1, "\"type\":");

                            if (comp->type == COMP_RESISTOR) {
                                char *res_ptr = strstr(props_ptr, "\"resistance\":");
                                if (res_ptr && (!next_comp || res_ptr < next_comp)) {
                                    double resistance;
                                    if (sscanf(res_ptr, "\"resistance\": %lf", &resistance) == 1) {
                                        comp->props.resistor.resistance = resistance;
                                    }
                                    char *hp_ptr = strstr(props_ptr, "\"high_power\":");
                                    if (hp_ptr && (!next_comp || hp_ptr < next_comp)) {
                                        int hp = 0;
                                        if (sscanf(hp_ptr + 13, " %d", &hp) == 1 && hp) { comp->props.resistor.high_power = true; comp->props.resistor.power_rating = 1e12; comp->thermal.max_temperature = 0.0; }
                                    }
                                }
                            } else if (comp->type == COMP_SOURCE_3PH) {
                                const char *keys[5] = { "\"v_peak\":", "\"frequency\":", "\"phase\":", "\"r_series\":", "\"l_series\":" };
                                double *dst[5] = { &comp->props.source_3ph.v_peak, &comp->props.source_3ph.frequency, &comp->props.source_3ph.phase, &comp->props.source_3ph.r_series, &comp->props.source_3ph.l_series };
                                for (int k = 0; k < 5; k++) {
                                    char *kp = strstr(props_ptr, keys[k]);
                                    if (kp && (!next_comp || kp < next_comp)) { double val; if (sscanf(kp + strlen(keys[k]), " %lf", &val) == 1) *dst[k] = val; }
                                }
                            } else if (comp->type == COMP_SPARK_GAP || comp->type == COMP_TOROID || comp->type == COMP_TLINE) {
                                const char *keys[5]; double *dst[5]; int nk = 0; double model_tmp = comp->props.tline.model;
                                if (comp->type == COMP_SPARK_GAP) {
                                    keys[0] = "\"gap_mm\":"; keys[1] = "\"r_on\":";
                                    dst[0] = &comp->props.spark_gap.gap_mm; dst[1] = &comp->props.spark_gap.r_on; nk = 2;
                                } else if (comp->type == COMP_TOROID) {
                                    keys[0] = "\"major_in\":"; keys[1] = "\"minor_in\":";
                                    dst[0] = &comp->props.toroid.major_in; dst[1] = &comp->props.toroid.minor_in; nk = 2;
                                } else {
                                    keys[0] = "\"length_mi\":"; keys[1] = "\"r_per_mi\":"; keys[2] = "\"x_per_mi\":"; keys[3] = "\"b_us_per_mi\":"; keys[4] = "\"model\":";
                                    dst[0] = &comp->props.tline.length_mi; dst[1] = &comp->props.tline.r_per_mi; dst[2] = &comp->props.tline.x_per_mi;
                                    dst[3] = &comp->props.tline.b_us_per_mi; dst[4] = &model_tmp; nk = 5;
                                }
                                for (int k = 0; k < nk; k++) {
                                    char *kp = strstr(props_ptr, keys[k]);
                                    if (kp && (!next_comp || kp < next_comp)) { double val; if (sscanf(kp + strlen(keys[k]), " %lf", &val) == 1) *dst[k] = val; }
                                }
                                if (comp->type == COMP_TLINE) comp->props.tline.model = (int)model_tmp;
                            } else if (comp->type == COMP_CAPACITOR) {
                                char *cap_ptr = strstr(props_ptr, "\"capacitance\":");
                                if (cap_ptr && (!next_comp || cap_ptr < next_comp)) {
                                    double capacitance;
                                    if (sscanf(cap_ptr, "\"capacitance\": %lf", &capacitance) == 1) {
                                        comp->props.capacitor.capacitance = capacitance;
                                    }
                                }
                            } else if (comp->type == COMP_OPAMP || comp->type == COMP_OPAMP_FLIPPED || comp->type == COMP_OPAMP_REAL) {
                                char *gain_ptr = strstr(props_ptr, "\"gain\":");
                                char *gbw_ptr = strstr(props_ptr, "\"gbw\":");
                                char *voff_ptr = strstr(props_ptr, "\"voffset\":");
                                if (gain_ptr && (!next_comp || gain_ptr < next_comp)) {
                                    double gain;
                                    if (sscanf(gain_ptr, "\"gain\": %lf", &gain) == 1) {
                                        comp->props.opamp.gain = gain;
                                    }
                                }
                                if (gbw_ptr && (!next_comp || gbw_ptr < next_comp)) {
                                    double gbw;
                                    if (sscanf(gbw_ptr, "\"gbw\": %lf", &gbw) == 1) {
                                        comp->props.opamp.gbw = gbw;
                                    }
                                }
                                if (voff_ptr && (!next_comp || voff_ptr < next_comp)) {
                                    double voffset;
                                    if (sscanf(voff_ptr, "\"voffset\": %lf", &voffset) == 1) {
                                        comp->props.opamp.voffset = voffset;
                                    }
                                }
                            } else if (comp->type == COMP_AC_VOLTAGE) {
                                char *amp_ptr = strstr(props_ptr, "\"amplitude\":");
                                char *freq_ptr = strstr(props_ptr, "\"frequency\":");
                                char *off_ptr = strstr(props_ptr, "\"offset\":");
                                if (amp_ptr && (!next_comp || amp_ptr < next_comp)) {
                                    double amplitude;
                                    if (sscanf(amp_ptr, "\"amplitude\": %lf", &amplitude) == 1) {
                                        comp->props.ac_voltage.amplitude = amplitude;
                                    }
                                }
                                if (freq_ptr && (!next_comp || freq_ptr < next_comp)) {
                                    double frequency;
                                    if (sscanf(freq_ptr, "\"frequency\": %lf", &frequency) == 1) {
                                        comp->props.ac_voltage.frequency = frequency;
                                    }
                                }
                                if (off_ptr && (!next_comp || off_ptr < next_comp)) {
                                    double offset;
                                    if (sscanf(off_ptr, "\"offset\": %lf", &offset) == 1) {
                                        comp->props.ac_voltage.offset = offset;
                                    }
                                }
                            } else if (comp->type == COMP_SPST_SWITCH) {
                                char *closed_ptr = strstr(props_ptr, "\"closed\":");
                                if (closed_ptr && (!next_comp || closed_ptr < next_comp)) {
                                    if (strstr(closed_ptr, "true")) {
                                        comp->props.switch_spst.closed = true;
                                    } else {
                                        comp->props.switch_spst.closed = false;
                                    }
                                }
                            } else if (comp->type == COMP_LED_ARRAY) {
                                char *color_ptr = strstr(props_ptr, "\"color\":");
                                if (color_ptr && (!next_comp || color_ptr < next_comp)) {
                                    int color;
                                    if (sscanf(color_ptr, "\"color\": %d", &color) == 1) {
                                        comp->props.led_array.color = color;
                                        component_update_led_color(comp);  // Update Vf, Is, etc.
                                    }
                                }
                            }
                        }

                        /* The full property block, if the file carries one. It wins over the
                           readable fields above: those are a hand-picked few and everything
                           they omit would otherwise stay at the component's default. */
                        char *end_of_comp = strstr(ptr + 1, "\"type\":");
                        char *state_ptr = strstr(ptr, "\"state\": \"");
                        if (state_ptr && (!end_of_comp || state_ptr < end_of_comp)) {
                            const char *hex = state_ptr + strlen("\"state\": \"");
                            ComponentProps p;
                            unsigned char *pb = (unsigned char *)&p;
                            size_t k = 0;
                            for (; k < sizeof p; k++) {
                                unsigned v;
                                if (!isxdigit((unsigned char)hex[2*k]) || !isxdigit((unsigned char)hex[2*k+1])) break;
                                if (sscanf(hex + 2*k, "%2x", &v) != 1) break;
                                pb[k] = (unsigned char)v;
                            }
                            if (k == sizeof p) component_adopt_props(comp, &p);
                        }
                        char *part_ptr = strstr(ptr, "\"part\": \"");
                        if (part_ptr && (!end_of_comp || part_ptr < end_of_comp)) {
                            const char *q = part_ptr + strlen("\"part\": \"");
                            size_t n = 0;
                            while (q[n] && q[n] != '"' && n < sizeof comp->part - 1) n++;
                            memcpy(comp->part, q, n);
                            comp->part[n] = 0;
                        }

                        circuit_add_component(circuit, comp);

                        /* and which node each terminal is on - after the add, which assigns them
                           by position and would otherwise overwrite what the file says */
                        char *term_ptr = strstr(ptr, "\"terminals\": [");
                        if (term_ptr && (!end_of_comp || term_ptr < end_of_comp)) {
                            const char *q = term_ptr + strlen("\"terminals\": [");
                            for (int k = 0; k < comp->num_terminals; k++) {
                                int id = 0;
                                while (*q == ' ' || *q == ',') q++;
                                if (sscanf(q, "%d", &id) != 1) break;
                                comp->node_ids[k] = id;
                                while (*q && *q != ',' && *q != ']') q++;
                            }
                        }
                    }
                }
            }
            ptr++;
        }
    }

    /* Parse nodes.
       The file's node ids are the ones its wires and its components' terminals refer to, so
       they have to survive. This used to call circuit_create_node, which hands out a fresh id
       and leaves every reference in the file pointing at something else - on top of the nodes
       the components had already created by position, which are replaced here. */
    ptr = strstr(buffer, "\"nodes\"");
    if (ptr) {
        circuit->num_nodes = 0;
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

                    if (circuit->num_nodes < MAX_NODES) {
                        Node *n = &circuit->nodes[circuit->num_nodes++];
                        memset(n, 0, sizeof *n);
                        n->id = id;
                        n->x = x;
                        n->y = y;
                        n->is_ground = is_ground;
                        if (is_ground) circuit->ground_node_id = id;
                        if (id >= circuit->next_node_id) circuit->next_node_id = id + 1;
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

    // Parse probes
    ptr = strstr(buffer, "\"probes\"");
    if (ptr) {
        while ((ptr = strstr(ptr, "\"id\":")) != NULL) {
            // Check we're still in probes section, not in components/nodes
            char *next_section = strstr(ptr + 1, "\"components\"");
            char *next_nodes = strstr(ptr + 1, "\"nodes\"");
            char *next_wires = strstr(ptr + 1, "\"wires\"");

            // Simplified check - if we hit closing brace of probes array, stop
            char *probe_end = strstr(ptr, "]");

            int id, node_id, channel;
            float x, y;
            char label[8];

            if (sscanf(ptr, "\"id\": %d", &id) == 1) {
                char *node_ptr = strstr(ptr, "\"node_id\":");
                char *x_ptr = strstr(ptr, "\"x\":");
                char *y_ptr = strstr(ptr, "\"y\":");
                char *ch_ptr = strstr(ptr, "\"channel\":");
                char *lbl_ptr = strstr(ptr, "\"label\":");

                if (node_ptr && x_ptr && y_ptr && ch_ptr && lbl_ptr &&
                    (!probe_end || (node_ptr < probe_end && x_ptr < probe_end))) {

                    if (sscanf(node_ptr, "\"node_id\": %d", &node_id) == 1 &&
                        sscanf(x_ptr, "\"x\": %f", &x) == 1 &&
                        sscanf(y_ptr, "\"y\": %f", &y) == 1 &&
                        sscanf(ch_ptr, "\"channel\": %d", &channel) == 1) {

                        // Parse label string
                        char *quote1 = strchr(lbl_ptr, '"');
                        if (quote1) {
                            quote1++;  // Skip opening quote
                            char *quote2 = strchr(quote1, '"');
                            if (quote2) {
                                int len = quote2 - quote1;
                                if (len > 7) len = 7;
                                strncpy(label, quote1, len);
                                label[len] = '\0';

                                // Add probe to circuit
                                if (circuit->num_probes < MAX_PROBES) {
                                    Probe *probe = &circuit->probes[circuit->num_probes++];
                                    probe->id = id;
                                    probe->node_id = node_id;
                                    probe->x = x;
                                    probe->y = y;
                                    probe->channel_num = channel;
                                    strncpy(probe->label, label, 7);
                                    probe->label[7] = '\0';
                                    probe->selected = false;
                                }
                            }
                        }
                    }
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
        case COMP_OPAMP_REAL: return "Op-Amp (real)";
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
        case COMP_OPAMP_REAL:
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
        case COMP_TEXT: {
            // Text annotation: emit the text itself instead of a placeholder box
            int fs = comp->props.text.font_size;
            float px = fs <= 1 ? 12.0f : (fs == 2 ? 18.0f : 24.0f);
            fprintf(f, "    <text x=\"%.1f\" y=\"%.1f\" text-anchor=\"middle\" class=\"label\" font-size=\"%.0f\"%s>",
                    x, y + px * 0.35f, px, comp->props.text.bold ? " font-weight=\"bold\"" : "");
            for (const char *c = comp->props.text.text; *c; c++) {
                if (*c == '<') fputs("&lt;", f);
                else if (*c == '>') fputs("&gt;", f);
                else if (*c == '&') fputs("&amp;", f);
                else fputc(*c, f);
            }
            fprintf(f, "</text>\n");
            fprintf(f, "  </g>\n");
            return;
        }
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
