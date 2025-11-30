/**
 * Circuit Playground - Component Implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "component.h"

// Component type information table
// NOTE: Terminal positions must be multiples of GRID_SIZE (20) for proper grid alignment
static const ComponentTypeInfo component_info[] = {
    [COMP_NONE] = { "None", "?", 0, {}, 0, 0, {} },

    [COMP_GROUND] = {
        "Ground", "GND", 1,
        {{ 0, -20, "GND" }},
        40, 40,
        {}
    },

    [COMP_DC_VOLTAGE] = {
        "DC Voltage", "V", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .dc_voltage = { 5.0 } }
    },

    [COMP_AC_VOLTAGE] = {
        "AC Voltage", "~V", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .ac_voltage = { 5.0, 60.0, 0.0, 0.0 } }
    },

    [COMP_DC_CURRENT] = {
        "DC Current", "I", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .dc_current = { 0.001 } }
    },

    [COMP_RESISTOR] = {
        "Resistor", "R", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 20,
        { .resistor = { 1000.0, 5.0, 0.25, 0.0 } }  // 1k, 5% tolerance, 1/4W rating, 0W dissipated
    },

    [COMP_CAPACITOR] = {
        "Capacitor", "C", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 30,
        { .capacitor = { 1e-6, 0.0 } }
    },

    [COMP_CAPACITOR_ELEC] = {
        "Electrolytic Cap", "Ce", 2,
        {{ -40, 0, "+" }, { 40, 0, "-" }},
        80, 30,
        { .capacitor_elec = { 100e-6, 0.0, 25.0 } }  // 100uF, 25V max
    },

    [COMP_INDUCTOR] = {
        "Inductor", "L", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 20,
        { .inductor = { 1e-3, 0.0 } }
    },

    [COMP_DIODE] = {
        "Diode", "D", 2,
        {{ -40, 0, "A" }, { 40, 0, "K" }},
        80, 20,
        { .diode = { 1e-12, 0.026, 1.0 } }  // Is=1pA, Vt=26mV, n=1
    },

    [COMP_ZENER] = {
        "Zener Diode", "DZ", 2,
        {{ -40, 0, "A" }, { 40, 0, "K" }},
        80, 20,
        { .zener = { 1e-12, 0.026, 1.0, 5.1 } }  // 5.1V Zener
    },

    [COMP_SCHOTTKY] = {
        "Schottky Diode", "DS", 2,
        {{ -40, 0, "A" }, { 40, 0, "K" }},
        80, 20,
        { .schottky = { 1e-8, 0.026, 1.05 } }  // Lower Vf (~0.3V)
    },

    [COMP_LED] = {
        "LED", "LED", 2,
        {{ -40, 0, "A" }, { 40, 0, "K" }},
        80, 20,
        { .led = { 1e-20, 0.026, 2.0, 620 } }  // Red LED (~2V Vf, 620nm)
    },

    [COMP_NPN_BJT] = {
        "NPN BJT", "Q", 3,
        {{ -20, 0, "B" }, { 20, -20, "C" }, { 20, 20, "E" }},
        60, 60,
        { .bjt = { 100.0, 1e-14, 100.0 } }
    },

    [COMP_PNP_BJT] = {
        "PNP BJT", "Q", 3,
        {{ -20, 0, "B" }, { 20, -20, "C" }, { 20, 20, "E" }},
        60, 60,
        { .bjt = { 100.0, 1e-14, 100.0 } }
    },

    [COMP_NMOS] = {
        "NMOS", "M", 3,
        {{ -20, 0, "G" }, { 20, -20, "D" }, { 20, 20, "S" }},
        60, 60,
        { .mosfet = { 0.001, 1.0, 0.01 } }
    },

    [COMP_PMOS] = {
        "PMOS", "M", 3,
        {{ -20, 0, "G" }, { 20, -20, "D" }, { 20, 20, "S" }},
        60, 60,
        { .mosfet = { 0.0005, -1.0, 0.01 } }
    },

    [COMP_OPAMP] = {
        "Op-Amp", "U", 3,
        {{ -40, -20, "-" }, { -40, 20, "+" }, { 40, 0, "OUT" }},
        80, 60,
        { .opamp = { 100000.0, 0.0, 15.0, -15.0 } }
    },

    // Waveform generators
    [COMP_SQUARE_WAVE] = {
        "Square Wave", "SQ", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .square_wave = { 5.0, 1000.0, 0.0, 0.0, 0.5 } }  // 5V, 1kHz, 50% duty
    },

    [COMP_TRIANGLE_WAVE] = {
        "Triangle Wave", "TRI", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .triangle_wave = { 5.0, 1000.0, 0.0, 0.0 } }  // 5V, 1kHz
    },

    [COMP_SAWTOOTH_WAVE] = {
        "Sawtooth Wave", "SAW", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .sawtooth_wave = { 5.0, 1000.0, 0.0, 0.0 } }  // 5V, 1kHz
    },

    [COMP_NOISE_SOURCE] = {
        "Noise Source", "N", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .noise_source = { 1.0, 12345 } }  // 1V amplitude, seed
    },
};

static int next_component_id = 1;

const ComponentTypeInfo *component_get_info(ComponentType type) {
    if (type >= 0 && type < COMP_TYPE_COUNT) {
        return &component_info[type];
    }
    return &component_info[COMP_NONE];
}

Component *component_create(ComponentType type, float x, float y) {
    if (type <= COMP_NONE || type >= COMP_TYPE_COUNT) {
        return NULL;
    }

    Component *comp = calloc(1, sizeof(Component));
    if (!comp) return NULL;

    const ComponentTypeInfo *info = component_get_info(type);

    comp->id = next_component_id++;
    comp->type = type;
    comp->x = x;
    comp->y = y;
    comp->rotation = 0;
    comp->num_terminals = info->num_terminals;
    comp->props = info->default_props;

    // Set default label
    snprintf(comp->label, MAX_LABEL_LEN, "%s%d", info->short_name, comp->id);

    // Determine if component needs voltage variable (voltage sources, inductors)
    comp->needs_voltage_var = (type == COMP_DC_VOLTAGE ||
                               type == COMP_AC_VOLTAGE ||
                               type == COMP_INDUCTOR ||
                               type == COMP_OPAMP ||
                               type == COMP_SQUARE_WAVE ||
                               type == COMP_TRIANGLE_WAVE ||
                               type == COMP_SAWTOOTH_WAVE ||
                               type == COMP_NOISE_SOURCE);

    return comp;
}

void component_free(Component *comp) {
    free(comp);
}

Component *component_clone(Component *comp) {
    if (!comp) return NULL;

    Component *clone = malloc(sizeof(Component));
    if (!clone) return NULL;

    memcpy(clone, comp, sizeof(Component));
    clone->id = next_component_id++;
    clone->selected = false;
    clone->highlighted = false;

    // Clear node connections
    for (int i = 0; i < MAX_TERMINALS; i++) {
        clone->node_ids[i] = 0;
    }

    return clone;
}

void component_rotate(Component *comp) {
    if (comp) {
        comp->rotation = (comp->rotation + 90) % 360;
    }
}

void component_get_terminal_pos(Component *comp, int terminal_idx, float *x, float *y) {
    if (!comp || terminal_idx < 0 || terminal_idx >= comp->num_terminals) {
        *x = 0; *y = 0;
        return;
    }

    const ComponentTypeInfo *info = component_get_info(comp->type);
    float dx = info->terminals[terminal_idx].dx;
    float dy = info->terminals[terminal_idx].dy;

    // Apply rotation
    double rad = comp->rotation * M_PI / 180.0;
    double cos_r = cos(rad);
    double sin_r = sin(rad);

    *x = comp->x + dx * cos_r - dy * sin_r;
    *y = comp->y + dx * sin_r + dy * cos_r;
}

bool component_contains_point(Component *comp, float px, float py) {
    if (!comp) return false;

    const ComponentTypeInfo *info = component_get_info(comp->type);

    // Transform point to component local coordinates
    double rad = -comp->rotation * M_PI / 180.0;
    double cos_r = cos(rad);
    double sin_r = sin(rad);

    float dx = px - comp->x;
    float dy = py - comp->y;

    float local_x = dx * cos_r - dy * sin_r;
    float local_y = dx * sin_r + dy * cos_r;

    float half_w = info->width / 2 + 5;
    float half_h = info->height / 2 + 5;

    return (fabs(local_x) <= half_w && fabs(local_y) <= half_h);
}

int component_get_terminal_at(Component *comp, float px, float py, float threshold) {
    if (!comp) return -1;

    for (int i = 0; i < comp->num_terminals; i++) {
        float tx, ty;
        component_get_terminal_pos(comp, i, &tx, &ty);

        float dx = px - tx;
        float dy = py - ty;
        if (sqrt(dx*dx + dy*dy) <= threshold) {
            return i;
        }
    }

    return -1;
}

// Stamping helper macros
#define STAMP_CONDUCTANCE(n1, n2, g) do { \
    if ((n1) > 0) matrix_add(A, (n1)-1, (n1)-1, (g)); \
    if ((n2) > 0) matrix_add(A, (n2)-1, (n2)-1, (g)); \
    if ((n1) > 0 && (n2) > 0) { \
        matrix_add(A, (n1)-1, (n2)-1, -(g)); \
        matrix_add(A, (n2)-1, (n1)-1, -(g)); \
    } \
} while(0)

void component_stamp(Component *comp, Matrix *A, Vector *b,
                     int *node_map, int num_nodes,
                     double time, Vector *prev_solution, double dt) {
    if (!comp || !A || !b || !node_map) return;

    // Get node indices
    int n[MAX_TERMINALS];
    for (int i = 0; i < comp->num_terminals; i++) {
        n[i] = (comp->node_ids[i] > 0) ? node_map[comp->node_ids[i]] : 0;
    }

    switch (comp->type) {
        case COMP_GROUND: {
            // Ground forces node to 0V
            if (n[0] > 0) {
                double g_large = 1e10;
                matrix_add(A, n[0]-1, n[0]-1, g_large);
            }
            break;
        }

        case COMP_DC_VOLTAGE: {
            double V = comp->props.dc_voltage.voltage;
            int volt_idx = num_nodes + comp->voltage_var_idx;

            // Voltage source stamp
            if (n[0] > 0) {
                matrix_add(A, volt_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, volt_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, volt_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, volt_idx, -1);
            }
            vector_add(b, volt_idx, V);
            break;
        }

        case COMP_AC_VOLTAGE: {
            double amp = comp->props.ac_voltage.amplitude;
            double freq = comp->props.ac_voltage.frequency;
            double phase = comp->props.ac_voltage.phase * M_PI / 180.0;
            double offset = comp->props.ac_voltage.offset;

            double V = amp * sin(2 * M_PI * freq * time + phase) + offset;
            int volt_idx = num_nodes + comp->voltage_var_idx;

            if (n[0] > 0) {
                matrix_add(A, volt_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, volt_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, volt_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, volt_idx, -1);
            }
            vector_add(b, volt_idx, V);
            break;
        }

        case COMP_DC_CURRENT: {
            double I = comp->props.dc_current.current;
            if (n[0] > 0) vector_add(b, n[0]-1, -I);
            if (n[1] > 0) vector_add(b, n[1]-1, I);
            break;
        }

        case COMP_RESISTOR: {
            double G = 1.0 / comp->props.resistor.resistance;
            STAMP_CONDUCTANCE(n[0], n[1], G);
            break;
        }

        case COMP_CAPACITOR:
        case COMP_CAPACITOR_ELEC: {
            // Backward Euler companion model for capacitor:
            // i_C = C * dv/dt ≈ C * (v - v_prev) / dt = Geq * v - Ieq
            // where Geq = C/dt and Ieq = C * v_prev / dt
            double C = (comp->type == COMP_CAPACITOR) ?
                       comp->props.capacitor.capacitance :
                       comp->props.capacitor_elec.capacitance;
            double Geq = C / dt;
            double Ieq = 0;

            if (prev_solution) {
                double v1 = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double v2 = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                Ieq = C * (v1 - v2) / dt;
            }

            STAMP_CONDUCTANCE(n[0], n[1], Geq);
            // Ieq represents the capacitor's "memory" current
            // Positive Ieq means capacitor was charged (n1 > n2), so it sources current at n1
            if (n[0] > 0) vector_add(b, n[0]-1, Ieq);
            if (n[1] > 0) vector_add(b, n[1]-1, -Ieq);
            break;
        }

        case COMP_INDUCTOR: {
            double L = comp->props.inductor.inductance;
            double Req = L / dt;
            double Veq = 0;
            int curr_idx = num_nodes + comp->voltage_var_idx;

            if (prev_solution && curr_idx < prev_solution->size) {
                double Iprev = vector_get(prev_solution, curr_idx);
                Veq = L * Iprev / dt;
            }

            if (n[0] > 0) {
                matrix_add(A, curr_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, curr_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, curr_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, curr_idx, -1);
            }
            matrix_add(A, curr_idx, curr_idx, -Req);
            vector_add(b, curr_idx, Veq);
            break;
        }

        case COMP_DIODE: {
            double Is = comp->props.diode.is;
            double Vt = comp->props.diode.vt;
            double nn = comp->props.diode.n;
            double nVt = nn * Vt;

            double Vd = 0.6;
            if (prev_solution) {
                double v1 = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double v2 = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                Vd = CLAMP(v1 - v2, -5*nVt, 40*nVt);
            }

            double expTerm = exp(Vd / nVt);
            double Id = Is * (expTerm - 1);
            double Gd = (Is / nVt) * expTerm;
            double Ieq = Id - Gd * Vd;

            STAMP_CONDUCTANCE(n[0], n[1], Gd);
            if (n[0] > 0) vector_add(b, n[0]-1, -Ieq);
            if (n[1] > 0) vector_add(b, n[1]-1, Ieq);
            break;
        }

        case COMP_ZENER: {
            // Zener diode - bidirectional conduction
            double Is = comp->props.zener.is;
            double Vt = comp->props.zener.vt;
            double nn = comp->props.zener.n;
            double Vz = comp->props.zener.vz;
            double nVt = nn * Vt;

            double Vd = 0.6;
            if (prev_solution) {
                double v1 = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double v2 = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                Vd = v1 - v2;
            }

            double Gd, Ieq;
            if (Vd >= 0) {
                // Forward bias - normal diode behavior
                Vd = CLAMP(Vd, 0, 40*nVt);
                double expTerm = exp(Vd / nVt);
                double Id = Is * (expTerm - 1);
                Gd = (Is / nVt) * expTerm + 1e-12;
                Ieq = Id - Gd * Vd;
            } else {
                // Reverse bias - breakdown at Vz
                double Vrev = -Vd;
                if (Vrev > Vz) {
                    // In breakdown region
                    Gd = 1.0;  // Low impedance
                    Ieq = -(Vz * Gd - Gd * Vd);
                } else {
                    // Before breakdown
                    Gd = 1e-12;  // Very high impedance
                    Ieq = 0;
                }
            }

            STAMP_CONDUCTANCE(n[0], n[1], Gd);
            if (n[0] > 0) vector_add(b, n[0]-1, -Ieq);
            if (n[1] > 0) vector_add(b, n[1]-1, Ieq);
            break;
        }

        case COMP_SCHOTTKY:
        case COMP_LED: {
            // Similar to regular diode but with different parameters
            double Is, Vt, nn;
            if (comp->type == COMP_SCHOTTKY) {
                Is = comp->props.schottky.is;
                Vt = comp->props.schottky.vt;
                nn = comp->props.schottky.n;
            } else {
                Is = comp->props.led.is;
                Vt = comp->props.led.vt;
                nn = comp->props.led.n;
            }
            double nVt = nn * Vt;

            double Vd = 0.6;
            if (prev_solution) {
                double v1 = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double v2 = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                Vd = CLAMP(v1 - v2, -5*nVt, 40*nVt);
            }

            double expTerm = exp(Vd / nVt);
            double Id = Is * (expTerm - 1);
            double Gd = (Is / nVt) * expTerm + 1e-12;
            double Ieq = Id - Gd * Vd;

            STAMP_CONDUCTANCE(n[0], n[1], Gd);
            if (n[0] > 0) vector_add(b, n[0]-1, -Ieq);
            if (n[1] > 0) vector_add(b, n[1]-1, Ieq);
            break;
        }

        // Simplified stamps for transistors and op-amp
        case COMP_NPN_BJT:
        case COMP_PNP_BJT: {
            // Simplified BJT model
            double beta = comp->props.bjt.beta;
            double Is = comp->props.bjt.is;
            double Vt = 0.026;

            double Vbe = 0.6;
            if (prev_solution) {
                double vB = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double vE = (n[2] > 0) ? vector_get(prev_solution, n[2]-1) : 0;
                Vbe = CLAMP(vB - vE, -5*Vt, 40*Vt);
            }

            double expBE = exp(Vbe / Vt);
            double Gbe = (Is / (beta * Vt)) * expBE;
            double Gm = (Is / Vt) * expBE;

            // B-E junction
            STAMP_CONDUCTANCE(n[0], n[2], Gbe);

            // Transconductance (simplified)
            if (n[1] > 0 && n[0] > 0) matrix_add(A, n[1]-1, n[0]-1, Gm);
            if (n[1] > 0 && n[2] > 0) matrix_add(A, n[1]-1, n[2]-1, -Gm);
            if (n[2] > 0 && n[0] > 0) matrix_add(A, n[2]-1, n[0]-1, -Gm);
            if (n[2] > 0 && n[2] > 0) matrix_add(A, n[2]-1, n[2]-1, Gm);
            break;
        }

        case COMP_NMOS:
        case COMP_PMOS: {
            // Simplified MOSFET square-law model
            double Kn = comp->props.mosfet.kn;
            double Vth = comp->props.mosfet.vth;

            double Vgs = 0, Vds = 0;
            if (prev_solution) {
                double vG = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double vD = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                double vS = (n[2] > 0) ? vector_get(prev_solution, n[2]-1) : 0;
                Vgs = vG - vS;
                Vds = vD - vS;
            }

            double Gds = 1e-12;
            double Gm = 0;

            if (Vgs > Vth) {
                if (Vds < Vgs - Vth) {
                    // Triode
                    Gm = Kn * Vds;
                    Gds = Kn * (Vgs - Vth - Vds);
                } else {
                    // Saturation
                    Gm = Kn * (Vgs - Vth);
                    Gds = 1e-12;
                }
            }

            Gds = MAX(Gds, 1e-12);

            // D-S conductance
            STAMP_CONDUCTANCE(n[1], n[2], Gds);

            // Transconductance
            if (n[1] > 0 && n[0] > 0) matrix_add(A, n[1]-1, n[0]-1, Gm);
            if (n[1] > 0 && n[2] > 0) matrix_add(A, n[1]-1, n[2]-1, -Gm);
            if (n[2] > 0 && n[0] > 0) matrix_add(A, n[2]-1, n[0]-1, -Gm);
            if (n[2] > 0 && n[2] > 0) matrix_add(A, n[2]-1, n[2]-1, Gm);
            break;
        }

        case COMP_OPAMP: {
            double A_gain = comp->props.opamp.gain;
            int volt_idx = num_nodes + comp->voltage_var_idx;

            // VCVS model: Vout = A * (V+ - V-)
            if (n[2] > 0) {
                matrix_add(A, volt_idx, n[2]-1, 1);
                matrix_add(A, n[2]-1, volt_idx, 1);
            }
            if (n[1] > 0) matrix_add(A, volt_idx, n[1]-1, -A_gain);
            if (n[0] > 0) matrix_add(A, volt_idx, n[0]-1, A_gain);
            break;
        }

        case COMP_SQUARE_WAVE: {
            double amp = comp->props.square_wave.amplitude;
            double freq = comp->props.square_wave.frequency;
            double phase = comp->props.square_wave.phase * M_PI / 180.0;
            double offset = comp->props.square_wave.offset;
            double duty = comp->props.square_wave.duty;

            // Calculate normalized position in period (0 to 1)
            double period = 1.0 / freq;
            double t_norm = fmod(time + phase / (2 * M_PI * freq), period) / period;
            if (t_norm < 0) t_norm += 1.0;

            // Square wave: high for duty cycle, low otherwise
            double V = (t_norm < duty) ? (amp + offset) : (-amp + offset);
            int volt_idx = num_nodes + comp->voltage_var_idx;

            if (n[0] > 0) {
                matrix_add(A, volt_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, volt_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, volt_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, volt_idx, -1);
            }
            vector_add(b, volt_idx, V);
            break;
        }

        case COMP_TRIANGLE_WAVE: {
            double amp = comp->props.triangle_wave.amplitude;
            double freq = comp->props.triangle_wave.frequency;
            double phase = comp->props.triangle_wave.phase * M_PI / 180.0;
            double offset = comp->props.triangle_wave.offset;

            // Calculate normalized position in period (0 to 1)
            double period = 1.0 / freq;
            double t_norm = fmod(time + phase / (2 * M_PI * freq), period) / period;
            if (t_norm < 0) t_norm += 1.0;

            // Triangle wave: rises for first half, falls for second half
            double V;
            if (t_norm < 0.5) {
                V = amp * (4.0 * t_norm - 1.0) + offset;
            } else {
                V = amp * (3.0 - 4.0 * t_norm) + offset;
            }
            int volt_idx = num_nodes + comp->voltage_var_idx;

            if (n[0] > 0) {
                matrix_add(A, volt_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, volt_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, volt_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, volt_idx, -1);
            }
            vector_add(b, volt_idx, V);
            break;
        }

        case COMP_SAWTOOTH_WAVE: {
            double amp = comp->props.sawtooth_wave.amplitude;
            double freq = comp->props.sawtooth_wave.frequency;
            double phase = comp->props.sawtooth_wave.phase * M_PI / 180.0;
            double offset = comp->props.sawtooth_wave.offset;

            // Calculate normalized position in period (0 to 1)
            double period = 1.0 / freq;
            double t_norm = fmod(time + phase / (2 * M_PI * freq), period) / period;
            if (t_norm < 0) t_norm += 1.0;

            // Sawtooth wave: linear ramp from -amp to +amp
            double V = amp * (2.0 * t_norm - 1.0) + offset;
            int volt_idx = num_nodes + comp->voltage_var_idx;

            if (n[0] > 0) {
                matrix_add(A, volt_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, volt_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, volt_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, volt_idx, -1);
            }
            vector_add(b, volt_idx, V);
            break;
        }

        case COMP_NOISE_SOURCE: {
            double amp = comp->props.noise_source.amplitude;
            // Simple pseudo-random noise using time-based seed
            // Uses a combination of sine functions at irrational ratios for pseudo-randomness
            double V = amp * (sin(time * 12345.6789) + sin(time * 9876.5432 + 1.234) +
                             sin(time * 5678.1234 + 2.345)) / 3.0;
            int volt_idx = num_nodes + comp->voltage_var_idx;

            if (n[0] > 0) {
                matrix_add(A, volt_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, volt_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, volt_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, volt_idx, -1);
            }
            vector_add(b, volt_idx, V);
            break;
        }

        default:
            break;
    }
}

void format_engineering(double value, const char *unit, char *buf, size_t buf_size) {
    static const struct { double exp; const char *prefix; } prefixes[] = {
        {1e12, "T"}, {1e9, "G"}, {1e6, "M"}, {1e3, "k"},
        {1, ""}, {1e-3, "m"}, {1e-6, "u"}, {1e-9, "n"}, {1e-12, "p"}
    };

    double abs_val = fabs(value);
    if (abs_val == 0) {
        snprintf(buf, buf_size, "0 %s", unit);
        return;
    }

    for (size_t i = 0; i < sizeof(prefixes)/sizeof(prefixes[0]); i++) {
        if (abs_val >= prefixes[i].exp * 0.999) {
            double scaled = value / prefixes[i].exp;
            if (fabs(scaled) < 10)
                snprintf(buf, buf_size, "%.2f %s%s", scaled, prefixes[i].prefix, unit);
            else if (fabs(scaled) < 100)
                snprintf(buf, buf_size, "%.1f %s%s", scaled, prefixes[i].prefix, unit);
            else
                snprintf(buf, buf_size, "%.0f %s%s", scaled, prefixes[i].prefix, unit);
            return;
        }
    }

    snprintf(buf, buf_size, "%.2e %s", value, unit);
}

void component_get_value_string(Component *comp, char *buf, size_t buf_size) {
    if (!comp || !buf) return;

    switch (comp->type) {
        case COMP_DC_VOLTAGE:
            format_engineering(comp->props.dc_voltage.voltage, "V", buf, buf_size);
            break;
        case COMP_AC_VOLTAGE:
            format_engineering(comp->props.ac_voltage.amplitude, "V", buf, buf_size);
            break;
        case COMP_DC_CURRENT:
            format_engineering(comp->props.dc_current.current, "A", buf, buf_size);
            break;
        case COMP_RESISTOR:
            format_engineering(comp->props.resistor.resistance, "Ohm", buf, buf_size);
            break;
        case COMP_CAPACITOR:
            format_engineering(comp->props.capacitor.capacitance, "F", buf, buf_size);
            break;
        case COMP_INDUCTOR:
            format_engineering(comp->props.inductor.inductance, "H", buf, buf_size);
            break;
        case COMP_SQUARE_WAVE:
            format_engineering(comp->props.square_wave.amplitude, "V", buf, buf_size);
            break;
        case COMP_TRIANGLE_WAVE:
            format_engineering(comp->props.triangle_wave.amplitude, "V", buf, buf_size);
            break;
        case COMP_SAWTOOTH_WAVE:
            format_engineering(comp->props.sawtooth_wave.amplitude, "V", buf, buf_size);
            break;
        case COMP_NOISE_SOURCE:
            format_engineering(comp->props.noise_source.amplitude, "V", buf, buf_size);
            break;
        default:
            buf[0] = '\0';
            break;
    }
}
