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
        { .dc_voltage = {
            .voltage = 5.0,
            .r_series = 0.001,
            .ideal = true
        }}
    },

    [COMP_AC_VOLTAGE] = {
        "AC Voltage", "~V", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .ac_voltage = {
            .amplitude = 5.0,
            .frequency = 60.0,
            .phase = 0.0,
            .offset = 0.0,
            .r_series = 0.001,
            .ideal = true
        }}
    },

    [COMP_DC_CURRENT] = {
        "DC Current", "I", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .dc_current = {
            .current = 0.001,
            .r_parallel = 1e9,
            .ideal = true
        }}
    },

    [COMP_RESISTOR] = {
        "Resistor", "R", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 20,
        { .resistor = {
            .resistance = 1000.0,
            .tolerance = 5.0,
            .power_rating = 0.25,
            .power_dissipated = 0.0,
            .temp_coeff = 100.0,    // 100 ppm/°C (typical for carbon film)
            .temp = 25.0,           // Room temperature
            .ideal = true
        }}
    },

    [COMP_CAPACITOR] = {
        "Capacitor", "C", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 30,
        { .capacitor = {
            .capacitance = 1e-6,
            .voltage = 0.0,
            .esr = 0.01,            // 10 mOhm ESR
            .esl = 1e-9,            // 1 nH ESL
            .leakage = 1e9,         // 1 GOhm leakage
            .ideal = true
        }}
    },

    [COMP_CAPACITOR_ELEC] = {
        "Electrolytic Cap", "Ce", 2,
        {{ -40, 0, "+" }, { 40, 0, "-" }},
        80, 30,
        { .capacitor_elec = {
            .capacitance = 100e-6,
            .voltage = 0.0,
            .max_voltage = 25.0,
            .esr = 0.1,             // Higher ESR for electrolytics
            .leakage = 1e6,         // Lower leakage resistance
            .ideal = true
        }}
    },

    [COMP_INDUCTOR] = {
        "Inductor", "L", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 20,
        { .inductor = {
            .inductance = 1e-3,
            .current = 0.0,
            .dcr = 0.1,             // 100 mOhm DC resistance
            .r_parallel = 1e6,      // Core loss resistance
            .i_sat = 1.0,           // 1A saturation current
            .ideal = true
        }}
    },

    [COMP_DIODE] = {
        "Diode", "D", 2,
        {{ -40, 0, "A" }, { 40, 0, "K" }},
        80, 20,
        { .diode = {
            .is = 1e-12,            // 1 pA saturation current
            .vt = 0.026,            // 26 mV thermal voltage
            .n = 1.0,               // Ideality factor
            .bv = 100.0,            // 100V reverse breakdown
            .ibv = 1e-10,           // Breakdown current
            .cjo = 1e-12,           // 1 pF junction capacitance
            .ideal = true
        }}
    },

    [COMP_ZENER] = {
        "Zener Diode", "DZ", 2,
        {{ -40, 0, "A" }, { 40, 0, "K" }},
        80, 20,
        { .zener = {
            .is = 1e-12,
            .vt = 0.026,
            .n = 1.0,
            .vz = 5.1,              // 5.1V Zener voltage
            .rz = 5.0,              // 5 Ohm Zener impedance
            .iz_test = 5e-3,        // 5 mA test current
            .ideal = true
        }}
    },

    [COMP_SCHOTTKY] = {
        "Schottky Diode", "DS", 2,
        {{ -40, 0, "A" }, { 40, 0, "K" }},
        80, 20,
        { .schottky = {
            .is = 1e-8,             // Higher saturation current
            .vt = 0.026,
            .n = 1.05,              // Slightly higher ideality
            .vf = 0.3,              // 0.3V typical forward voltage
            .cjo = 5e-12,           // 5 pF junction capacitance
            .ideal = true
        }}
    },

    [COMP_LED] = {
        "LED", "LED", 2,
        {{ -40, 0, "A" }, { 40, 0, "K" }},
        80, 20,
        { .led = {
            .is = 1e-20,
            .vt = 0.026,
            .n = 2.0,               // Higher ideality for LED
            .vf = 2.0,              // 2.0V forward voltage (red)
            .max_current = 0.020,   // 20 mA max
            .wavelength = 620,      // Red (620 nm)
            .current = 0.0,
            .ideal = true
        }}
    },

    [COMP_NPN_BJT] = {
        "NPN BJT", "Q", 3,
        {{ -20, 0, "B" }, { 20, -20, "C" }, { 20, 20, "E" }},
        60, 60,
        { .bjt = {
            .bf = 100.0,        // Forward current gain (beta)
            .is = 1e-14,        // Saturation current
            .vaf = 100.0,       // Forward Early voltage
            .nf = 1.0,          // Forward emission coefficient
            .br = 1.0,          // Reverse current gain
            .var = 100.0,       // Reverse Early voltage
            .nr = 1.0,          // Reverse emission coefficient
            .ise = 0.0,         // B-E leakage saturation current
            .isc = 0.0,         // B-C leakage saturation current
            .temp = 300.0,      // Temperature (K)
            .ideal = true       // Use ideal (simplified) model
        }}
    },

    [COMP_PNP_BJT] = {
        "PNP BJT", "Q", 3,
        {{ -20, 0, "B" }, { 20, -20, "C" }, { 20, 20, "E" }},
        60, 60,
        { .bjt = {
            .bf = 100.0,
            .is = 1e-14,
            .vaf = 100.0,
            .nf = 1.0,
            .br = 1.0,
            .var = 100.0,
            .nr = 1.0,
            .ise = 0.0,
            .isc = 0.0,
            .temp = 300.0,
            .ideal = true
        }}
    },

    [COMP_NMOS] = {
        "NMOS", "M", 3,
        {{ -20, 0, "G" }, { 20, -20, "D" }, { 20, 20, "S" }},
        60, 60,
        { .mosfet = {
            .vth = 0.7,         // Threshold voltage (V)
            .kp = 110e-6,       // Transconductance parameter (A/V²)
            .lambda = 0.04,     // Channel length modulation (1/V)
            .w = 10e-6,         // Channel width (m) - 10um
            .l = 1e-6,          // Channel length (m) - 1um
            .tox = 10e-9,       // Gate oxide thickness (m) - 10nm
            .gamma = 0.4,       // Body effect coefficient (V^0.5)
            .phi = 0.65,        // Surface potential (V)
            .nsub = 1e15,       // Substrate doping (1/cm³)
            .temp = 300.0,      // Temperature (K)
            .ideal = true       // Use ideal (simplified) model
        }}
    },

    [COMP_PMOS] = {
        "PMOS", "M", 3,
        {{ -20, 0, "G" }, { 20, -20, "D" }, { 20, 20, "S" }},
        60, 60,
        { .mosfet = {
            .vth = -0.7,        // Threshold voltage (V) - negative for PMOS
            .kp = 50e-6,        // Transconductance parameter (A/V²) - lower for PMOS
            .lambda = 0.04,     // Channel length modulation (1/V)
            .w = 10e-6,         // Channel width (m)
            .l = 1e-6,          // Channel length (m)
            .tox = 10e-9,       // Gate oxide thickness (m)
            .gamma = 0.4,       // Body effect coefficient (V^0.5)
            .phi = 0.65,        // Surface potential (V)
            .nsub = 1e15,       // Substrate doping (1/cm³)
            .temp = 300.0,      // Temperature (K)
            .ideal = true       // Use ideal (simplified) model
        }}
    },

    [COMP_OPAMP] = {
        "Op-Amp", "U", 3,
        {{ -40, -20, "-" }, { -40, 20, "+" }, { 40, 0, "OUT" }},
        80, 60,
        { .opamp = {
            .gain = 100000.0,       // 100 dB open-loop gain
            .voffset = 0.0,         // No input offset
            .vmax = 15.0,           // +15V rail
            .vmin = -15.0,          // -15V rail
            .gbw = 1e6,             // 1 MHz gain-bandwidth product
            .slew_rate = 0.5,       // 0.5 V/us slew rate
            .r_in = 1e12,           // 1 TOhm input impedance
            .r_out = 75.0,          // 75 Ohm output impedance
            .i_bias = 1e-12,        // 1 pA input bias current
            .cmrr = 90.0,           // 90 dB CMRR
            .rail_to_rail = false,  // Not rail-to-rail
            .ideal = true           // Ideal mode by default
        }}
    },

    // Waveform generators
    [COMP_SQUARE_WAVE] = {
        "Square Wave", "SQ", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .square_wave = {
            .amplitude = 5.0,
            .frequency = 1000.0,
            .phase = 0.0,
            .offset = 0.0,
            .duty = 0.5,
            .rise_time = 1e-9,
            .fall_time = 1e-9,
            .r_series = 0.001,
            .ideal = true
        }}
    },

    [COMP_TRIANGLE_WAVE] = {
        "Triangle Wave", "TRI", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .triangle_wave = {
            .amplitude = 5.0,
            .frequency = 1000.0,
            .phase = 0.0,
            .offset = 0.0,
            .r_series = 0.001,
            .ideal = true
        }}
    },

    [COMP_SAWTOOTH_WAVE] = {
        "Sawtooth Wave", "SAW", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .sawtooth_wave = {
            .amplitude = 5.0,
            .frequency = 1000.0,
            .phase = 0.0,
            .offset = 0.0,
            .r_series = 0.001,
            .ideal = true
        }}
    },

    [COMP_NOISE_SOURCE] = {
        "Noise Source", "N", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .noise_source = {
            .amplitude = 1.0,
            .seed = 12345,
            .bandwidth = 1e6,
            .r_series = 0.001,
            .ideal = true
        }}
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

            // Store LED current for glow rendering
            if (comp->type == COMP_LED) {
                comp->props.led.current = Id > 0 ? Id : 0;
            }

            STAMP_CONDUCTANCE(n[0], n[1], Gd);
            if (n[0] > 0) vector_add(b, n[0]-1, -Ieq);
            if (n[1] > 0) vector_add(b, n[1]-1, Ieq);
            break;
        }

        // BJT transistor stamps (Gummel-Poon model)
        case COMP_NPN_BJT:
        case COMP_PNP_BJT: {
            double bf = comp->props.bjt.bf;      // Forward beta
            double Is = comp->props.bjt.is;      // Saturation current
            double Vaf = comp->props.bjt.vaf;    // Early voltage
            double nf = comp->props.bjt.nf;      // Emission coefficient
            double temp = comp->props.bjt.temp;  // Temperature
            bool ideal = comp->props.bjt.ideal;

            // Thermal voltage at temperature
            double Vt = 8.617e-5 * temp;  // k*T/q

            // For PNP, invert voltage polarities
            double sign = (comp->type == COMP_PNP_BJT) ? -1.0 : 1.0;

            double Vbe = 0.6 * sign;
            double Vbc = 0.0;
            if (prev_solution) {
                double vB = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double vC = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                double vE = (n[2] > 0) ? vector_get(prev_solution, n[2]-1) : 0;
                Vbe = sign * (vB - vE);
                Vbc = sign * (vB - vC);
                Vbe = CLAMP(Vbe, -5*nf*Vt, 40*nf*Vt);
                Vbc = CLAMP(Vbc, -5*nf*Vt, 40*nf*Vt);
            }

            double Gbe, Gbc, Gm, Ieq_be, Ieq_bc;

            if (ideal) {
                // Ideal Ebers-Moll model (simplified)
                double expBE = exp(Vbe / (nf * Vt));
                double Ibe = (Is / bf) * (expBE - 1);
                Gbe = (Is / (bf * nf * Vt)) * expBE + 1e-12;
                Ieq_be = Ibe - Gbe * Vbe;

                // Collector current - forward active
                double Ic = Is * (expBE - 1);
                Gm = (Is / (nf * Vt)) * expBE;

                // Simplified: ignore B-C junction for ideal mode
                Gbc = 1e-12;
                Ieq_bc = 0;
            } else {
                // Non-ideal Gummel-Poon model with Early effect
                double br = comp->props.bjt.br;
                double nr = comp->props.bjt.nr;
                double ise = comp->props.bjt.ise;
                double isc = comp->props.bjt.isc;

                // Forward B-E diode
                double expBE = exp(Vbe / (nf * Vt));
                double Ibe_main = (Is / bf) * (expBE - 1);
                double Ibe_leak = ise * (exp(Vbe / (2 * nf * Vt)) - 1);  // Low-level injection
                double Ibe = Ibe_main + Ibe_leak;
                Gbe = (Is / (bf * nf * Vt)) * expBE + 1e-12;
                Ieq_be = Ibe - Gbe * Vbe;

                // Reverse B-C diode
                double expBC = exp(Vbc / (nr * Vt));
                double Ibc_main = (Is / br) * (expBC - 1);
                double Ibc_leak = isc * (exp(Vbc / (2 * nr * Vt)) - 1);
                double Ibc = Ibc_main + Ibc_leak;
                Gbc = (Is / (br * nr * Vt)) * expBC + 1e-12;
                Ieq_bc = Ibc - Gbc * Vbc;

                // Collector current with Early effect
                double early_factor = 1.0;
                if (Vaf > 0) {
                    double Vce = Vbe - Vbc;
                    early_factor = 1.0 + Vce / Vaf;
                }
                double Ic_f = Is * (expBE - 1) * early_factor;
                double Ic_r = Is * (expBC - 1);
                Gm = (Is / (nf * Vt)) * expBE * early_factor;
            }

            // Apply sign for PNP
            Gbe *= 1;  // Conductance is always positive
            Gm *= sign;
            Ieq_be *= sign;
            Ieq_bc *= sign;

            // Stamp B-E junction
            STAMP_CONDUCTANCE(n[0], n[2], Gbe);
            if (n[0] > 0) vector_add(b, n[0]-1, -Ieq_be);
            if (n[2] > 0) vector_add(b, n[2]-1, Ieq_be);

            // Stamp B-C junction (for non-ideal mode)
            if (!ideal) {
                STAMP_CONDUCTANCE(n[0], n[1], Gbc);
                if (n[0] > 0) vector_add(b, n[0]-1, -Ieq_bc);
                if (n[1] > 0) vector_add(b, n[1]-1, Ieq_bc);
            }

            // Transconductance (collector current controlled by Vbe)
            if (n[1] > 0 && n[0] > 0) matrix_add(A, n[1]-1, n[0]-1, Gm);
            if (n[1] > 0 && n[2] > 0) matrix_add(A, n[1]-1, n[2]-1, -Gm);
            if (n[2] > 0 && n[0] > 0) matrix_add(A, n[2]-1, n[0]-1, -Gm);
            if (n[2] > 0 && n[2] > 0) matrix_add(A, n[2]-1, n[2]-1, Gm);
            break;
        }

        // MOSFET transistor stamps (Level 1 SPICE model)
        case COMP_NMOS:
        case COMP_PMOS: {
            double Vth = comp->props.mosfet.vth;
            double Kp = comp->props.mosfet.kp;
            double lambda = comp->props.mosfet.lambda;
            double W = comp->props.mosfet.w;
            double L = comp->props.mosfet.l;
            bool ideal = comp->props.mosfet.ideal;

            // Effective transconductance: K = Kp * W / L
            double K = Kp * (W / L);

            // For PMOS, work with absolute values and invert at end
            double sign = (comp->type == COMP_PMOS) ? -1.0 : 1.0;
            double Vth_eff = fabs(Vth);

            double Vgs = 0, Vds = 0, Vsb = 0;
            if (prev_solution) {
                double vG = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double vD = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                double vS = (n[2] > 0) ? vector_get(prev_solution, n[2]-1) : 0;

                if (comp->type == COMP_PMOS) {
                    // For PMOS: Vsg, Vsd (source and drain swapped in equations)
                    Vgs = vS - vG;
                    Vds = vS - vD;
                    Vsb = 0;  // Assume body tied to source
                } else {
                    Vgs = vG - vS;
                    Vds = vD - vS;
                    Vsb = 0;
                }
            }

            // Body effect (non-ideal mode only)
            double Vth_adj = Vth_eff;
            if (!ideal && Vsb > 0) {
                double gamma = comp->props.mosfet.gamma;
                double phi = comp->props.mosfet.phi;
                Vth_adj = Vth_eff + gamma * (sqrt(phi + Vsb) - sqrt(phi));
            }

            double Gds = 1e-12;  // Minimum conductance
            double Gm = 0;
            double Id = 0;
            double Ieq = 0;

            double Vov = Vgs - Vth_adj;  // Overdrive voltage

            if (Vov <= 0) {
                // Cutoff region
                Gds = 1e-12;
                Gm = 0;
                Id = 0;
            } else if (Vds < Vov) {
                // Triode (linear) region
                // Id = K * (Vov * Vds - Vds²/2) * (1 + lambda * Vds)
                double lambda_term = ideal ? 1.0 : (1.0 + lambda * Vds);
                Id = K * (Vov * Vds - 0.5 * Vds * Vds) * lambda_term;

                // Derivatives for Newton-Raphson linearization
                Gm = K * Vds * lambda_term;  // dId/dVgs
                Gds = K * (Vov - Vds) * lambda_term;  // dId/dVds
                if (!ideal) {
                    Gds += K * (Vov * Vds - 0.5 * Vds * Vds) * lambda;
                }
            } else {
                // Saturation region
                // Id = (K/2) * Vov² * (1 + lambda * Vds)
                double lambda_term = ideal ? 1.0 : (1.0 + lambda * Vds);
                Id = 0.5 * K * Vov * Vov * lambda_term;

                // Derivatives
                Gm = K * Vov * lambda_term;  // dId/dVgs
                Gds = ideal ? 1e-12 : (0.5 * K * Vov * Vov * lambda);  // dId/dVds (channel length modulation)
            }

            // Ensure minimum conductance
            Gds = MAX(Gds, 1e-12);
            Gm = MAX(Gm, 0);

            // Equivalent current source: Ieq = Id - Gm*Vgs - Gds*Vds
            Ieq = Id - Gm * Vgs - Gds * Vds;

            // Apply sign for PMOS (currents flow opposite direction)
            Gm *= sign;
            Ieq *= sign;

            // Stamp D-S conductance
            STAMP_CONDUCTANCE(n[1], n[2], Gds);

            // Stamp equivalent current source
            if (n[1] > 0) vector_add(b, n[1]-1, -Ieq);
            if (n[2] > 0) vector_add(b, n[2]-1, Ieq);

            // Transconductance (drain current controlled by Vgs)
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
