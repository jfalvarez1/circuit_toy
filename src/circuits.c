/**
 * Circuit Playground - Predefined Circuit Templates Implementation
 */

#include <stdio.h>
#include <string.h>
#include "circuits.h"
#include "component.h"

// Circuit template info table
static const CircuitTemplateInfo template_info[] = {
    [CIRCUIT_NONE] = {"None", "None", "No circuit"},
    [CIRCUIT_RC_LOWPASS] = {"RC Low Pass", "LP", "RC low-pass filter (fc=1.6kHz)"},
    [CIRCUIT_RC_HIGHPASS] = {"RC High Pass", "HP", "RC high-pass filter (fc=1.6kHz)"},
    [CIRCUIT_RL_LOWPASS] = {"RL Low Pass", "RL-LP", "RL low-pass filter"},
    [CIRCUIT_RL_HIGHPASS] = {"RL High Pass", "RL-HP", "RL high-pass filter"},
    [CIRCUIT_VOLTAGE_DIVIDER] = {"Voltage Divider", "Div", "Resistive voltage divider (1:1)"},
    [CIRCUIT_INVERTING_AMP] = {"Inverting Amp", "Inv", "Inverting op-amp (gain=-10)"},
    [CIRCUIT_NONINVERTING_AMP] = {"Non-Inv Amp", "NonI", "Non-inverting op-amp (gain=11)"},
    [CIRCUIT_VOLTAGE_FOLLOWER] = {"Voltage Follower", "Fol", "Unity gain buffer"},
    [CIRCUIT_HALFWAVE_RECT] = {"Half-Wave Rect", "HW", "Half-wave rectifier"},
    [CIRCUIT_LED_WITH_RESISTOR] = {"LED + Resistor", "LED", "LED with current limiting resistor"},
};

const CircuitTemplateInfo *circuit_template_get_info(CircuitTemplateType type) {
    if (type < 0 || type >= CIRCUIT_TYPE_COUNT) {
        return &template_info[CIRCUIT_NONE];
    }
    return &template_info[type];
}

// Helper to create and add a component
static Component *add_comp(Circuit *circuit, ComponentType type, float x, float y, int rotation) {
    Component *comp = component_create(type, x, y);
    if (!comp) return NULL;
    comp->rotation = rotation;
    circuit_add_component(circuit, comp);
    return comp;
}

// Helper to connect two component terminals with a wire
static void connect_terminals(Circuit *circuit, Component *c1, int t1, Component *c2, int t2) {
    float x1, y1, x2, y2;
    component_get_terminal_pos(c1, t1, &x1, &y1);
    component_get_terminal_pos(c2, t2, &x2, &y2);

    int n1 = circuit_find_or_create_node(circuit, x1, y1, 5.0f);
    int n2 = circuit_find_or_create_node(circuit, x2, y2, 5.0f);

    if (n1 != n2) {
        circuit_add_wire(circuit, n1, n2);
    }

    // Update node connections for components
    c1->node_ids[t1] = n1;
    c2->node_ids[t2] = n2;
}

// Helper to create a node at component terminal
static int create_node_at(Circuit *circuit, Component *comp, int terminal_idx) {
    float x, y;
    component_get_terminal_pos(comp, terminal_idx, &x, &y);
    int node_id = circuit_find_or_create_node(circuit, x, y, 5.0f);
    comp->node_ids[terminal_idx] = node_id;
    return node_id;
}

// RC Low Pass Filter: Vin --[R]--+--[C]-- GND
//                                |
//                               Vout
static int place_rc_lowpass(Circuit *circuit, float x, float y) {
    // AC voltage source
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 1.0;
    vsrc->props.ac_voltage.frequency = 1000.0;

    // Ground
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Resistor (1k for fc ~= 1.6kHz with 100nF)
    Component *res = add_comp(circuit, COMP_RESISTOR, x + 100, y, 0);
    res->props.resistor.resistance = 1000.0;

    // Capacitor (100nF)
    Component *cap = add_comp(circuit, COMP_CAPACITOR, x + 200, y + 50, 90);
    cap->props.capacitor.capacitance = 100e-9;

    // Ground for capacitor
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 200, y + 100, 0);

    // Connect source negative to ground
    connect_terminals(circuit, vsrc, 1, gnd, 0);

    // Connect source positive to resistor
    connect_terminals(circuit, vsrc, 0, res, 0);

    // Connect resistor to capacitor (output node)
    connect_terminals(circuit, res, 1, cap, 0);

    // Connect capacitor to ground
    connect_terminals(circuit, cap, 1, gnd2, 0);

    return 5;
}

// RC High Pass Filter: Vin --[C]--+--[R]-- GND
//                                 |
//                                Vout
static int place_rc_highpass(Circuit *circuit, float x, float y) {
    // AC voltage source
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 1.0;
    vsrc->props.ac_voltage.frequency = 1000.0;

    // Ground
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Capacitor (100nF)
    Component *cap = add_comp(circuit, COMP_CAPACITOR, x + 100, y, 0);
    cap->props.capacitor.capacitance = 100e-9;

    // Resistor (1k)
    Component *res = add_comp(circuit, COMP_RESISTOR, x + 200, y + 50, 90);
    res->props.resistor.resistance = 1000.0;

    // Ground for resistor
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 200, y + 100, 0);

    // Connect source negative to ground
    connect_terminals(circuit, vsrc, 1, gnd, 0);

    // Connect source positive to capacitor
    connect_terminals(circuit, vsrc, 0, cap, 0);

    // Connect capacitor to resistor (output node)
    connect_terminals(circuit, cap, 1, res, 0);

    // Connect resistor to ground
    connect_terminals(circuit, res, 1, gnd2, 0);

    return 5;
}

// RL Low Pass Filter
static int place_rl_lowpass(Circuit *circuit, float x, float y) {
    // AC voltage source
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 1.0;
    vsrc->props.ac_voltage.frequency = 1000.0;

    // Ground
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Inductor (10mH)
    Component *ind = add_comp(circuit, COMP_INDUCTOR, x + 100, y, 0);
    ind->props.inductor.inductance = 10e-3;

    // Resistor (100 ohm)
    Component *res = add_comp(circuit, COMP_RESISTOR, x + 200, y + 50, 90);
    res->props.resistor.resistance = 100.0;

    // Ground
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 200, y + 100, 0);

    connect_terminals(circuit, vsrc, 1, gnd, 0);
    connect_terminals(circuit, vsrc, 0, ind, 0);
    connect_terminals(circuit, ind, 1, res, 0);
    connect_terminals(circuit, res, 1, gnd2, 0);

    return 5;
}

// RL High Pass Filter
static int place_rl_highpass(Circuit *circuit, float x, float y) {
    // AC voltage source
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 1.0;
    vsrc->props.ac_voltage.frequency = 1000.0;

    // Ground
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Resistor (100 ohm)
    Component *res = add_comp(circuit, COMP_RESISTOR, x + 100, y, 0);
    res->props.resistor.resistance = 100.0;

    // Inductor (10mH)
    Component *ind = add_comp(circuit, COMP_INDUCTOR, x + 200, y + 50, 90);
    ind->props.inductor.inductance = 10e-3;

    // Ground
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 200, y + 100, 0);

    connect_terminals(circuit, vsrc, 1, gnd, 0);
    connect_terminals(circuit, vsrc, 0, res, 0);
    connect_terminals(circuit, res, 1, ind, 0);
    connect_terminals(circuit, ind, 1, gnd2, 0);

    return 5;
}

// Voltage Divider: Vcc --[R1]--+--[R2]-- GND
//                              |
//                             Vout
static int place_voltage_divider(Circuit *circuit, float x, float y) {
    // DC voltage source
    Component *vsrc = add_comp(circuit, COMP_DC_VOLTAGE, x, y, 0);
    if (!vsrc) return 0;
    vsrc->props.dc_voltage.voltage = 10.0;

    // Ground for source
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // R1 (top resistor)
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 100, y, 0);
    r1->props.resistor.resistance = 10000.0;

    // R2 (bottom resistor)
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 200, y + 50, 90);
    r2->props.resistor.resistance = 10000.0;

    // Ground for R2
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 200, y + 100, 0);

    connect_terminals(circuit, vsrc, 1, gnd, 0);
    connect_terminals(circuit, vsrc, 0, r1, 0);
    connect_terminals(circuit, r1, 1, r2, 0);
    connect_terminals(circuit, r2, 1, gnd2, 0);

    return 5;
}

// Inverting Amplifier:
//          Rf
//     +---/\/\/---+
//     |           |
// Vin-+--[Ri]--(-)\
//              (+)/---Vout
//               |
//              GND
static int place_inverting_amp(Circuit *circuit, float x, float y) {
    // AC voltage source
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 0.5;
    vsrc->props.ac_voltage.frequency = 1000.0;

    // Ground for source
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Input resistor Ri (1k)
    Component *ri = add_comp(circuit, COMP_RESISTOR, x + 100, y, 0);
    ri->props.resistor.resistance = 1000.0;

    // Op-amp
    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 250, y + 30, 0);

    // Feedback resistor Rf (10k for gain = -10)
    Component *rf = add_comp(circuit, COMP_RESISTOR, x + 250, y - 50, 0);
    rf->props.resistor.resistance = 10000.0;

    // Ground for non-inverting input
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 200, y + 100, 0);

    // Connect source negative to ground
    connect_terminals(circuit, vsrc, 1, gnd, 0);

    // Connect source positive to Ri
    connect_terminals(circuit, vsrc, 0, ri, 0);

    // Connect Ri to inverting input (and Rf left side)
    connect_terminals(circuit, ri, 1, opamp, 0);  // Inverting input

    // Connect Rf left to inverting input
    float ri_out_x, ri_out_y;
    component_get_terminal_pos(ri, 1, &ri_out_x, &ri_out_y);
    float rf_in_x, rf_in_y;
    component_get_terminal_pos(rf, 0, &rf_in_x, &rf_in_y);
    int node_inv = circuit_find_or_create_node(circuit, ri_out_x, ri_out_y, 5.0f);
    int node_rf_in = circuit_find_or_create_node(circuit, rf_in_x, rf_in_y, 5.0f);
    if (node_inv != node_rf_in) {
        circuit_add_wire(circuit, node_inv, node_rf_in);
    }
    rf->node_ids[0] = node_inv;

    // Connect non-inverting input to ground
    connect_terminals(circuit, opamp, 1, gnd2, 0);

    // Connect Rf right to output
    connect_terminals(circuit, rf, 1, opamp, 2);  // Output

    return 6;
}

// Non-Inverting Amplifier:
// Vin ---(+)\
//        (-)/---+---Vout
//          |    |
//         Ri    Rf
//          |    |
//         GND  GND
static int place_noninverting_amp(Circuit *circuit, float x, float y) {
    // AC voltage source
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 0.5;
    vsrc->props.ac_voltage.frequency = 1000.0;

    // Ground for source
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Op-amp
    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 150, y, 0);

    // Ri (feedback to ground) - 1k
    Component *ri = add_comp(circuit, COMP_RESISTOR, x + 100, y + 70, 90);
    ri->props.resistor.resistance = 1000.0;

    // Rf (feedback resistor) - 10k for gain = 11
    Component *rf = add_comp(circuit, COMP_RESISTOR, x + 150, y - 60, 0);
    rf->props.resistor.resistance = 10000.0;

    // Ground for Ri
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 100, y + 130, 0);

    // Connect source negative to ground
    connect_terminals(circuit, vsrc, 1, gnd, 0);

    // Connect source positive to non-inverting input
    connect_terminals(circuit, vsrc, 0, opamp, 1);

    // Connect Ri top to inverting input
    connect_terminals(circuit, ri, 0, opamp, 0);

    // Connect Ri bottom to ground
    connect_terminals(circuit, ri, 1, gnd2, 0);

    // Connect Rf left to inverting input (same node as Ri top)
    float ri_top_x, ri_top_y;
    component_get_terminal_pos(ri, 0, &ri_top_x, &ri_top_y);
    float rf_in_x, rf_in_y;
    component_get_terminal_pos(rf, 0, &rf_in_x, &rf_in_y);
    int node_inv = circuit_find_or_create_node(circuit, ri_top_x, ri_top_y, 5.0f);
    int node_rf_in = circuit_find_or_create_node(circuit, rf_in_x, rf_in_y, 5.0f);
    if (node_inv != node_rf_in) {
        circuit_add_wire(circuit, node_inv, node_rf_in);
    }
    rf->node_ids[0] = node_inv;

    // Connect Rf right to output
    connect_terminals(circuit, rf, 1, opamp, 2);

    return 6;
}

// Voltage Follower (Unity Gain Buffer):
// Vin ---(+)\
//        (-)/---Vout
//          |
//          +----+ (feedback)
static int place_voltage_follower(Circuit *circuit, float x, float y) {
    // AC voltage source
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 1.0;
    vsrc->props.ac_voltage.frequency = 1000.0;

    // Ground for source
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Op-amp
    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 150, y, 0);

    // Connect source negative to ground
    connect_terminals(circuit, vsrc, 1, gnd, 0);

    // Connect source positive to non-inverting input
    connect_terminals(circuit, vsrc, 0, opamp, 1);

    // Connect output to inverting input (unity feedback)
    connect_terminals(circuit, opamp, 2, opamp, 0);

    return 3;
}

// Half-Wave Rectifier:
// Vin ---[D]---+---[R]--- GND
//              |
//             Vout
static int place_halfwave_rectifier(Circuit *circuit, float x, float y) {
    // AC voltage source
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 5.0;
    vsrc->props.ac_voltage.frequency = 60.0;

    // Ground for source
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Diode
    Component *diode = add_comp(circuit, COMP_DIODE, x + 100, y, 0);

    // Load resistor
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 200, y + 50, 90);
    rload->props.resistor.resistance = 1000.0;

    // Ground for load
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 200, y + 100, 0);

    // Connect source negative to ground
    connect_terminals(circuit, vsrc, 1, gnd, 0);

    // Connect source positive to diode anode
    connect_terminals(circuit, vsrc, 0, diode, 0);

    // Connect diode cathode to load
    connect_terminals(circuit, diode, 1, rload, 0);

    // Connect load to ground
    connect_terminals(circuit, rload, 1, gnd2, 0);

    return 5;
}

// LED with Current Limiting Resistor:
// Vcc ---[R]---[LED]--- GND
static int place_led_with_resistor(Circuit *circuit, float x, float y) {
    // DC voltage source (5V)
    Component *vsrc = add_comp(circuit, COMP_DC_VOLTAGE, x, y, 0);
    if (!vsrc) return 0;
    vsrc->props.dc_voltage.voltage = 5.0;

    // Ground for source
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Current limiting resistor (for ~10mA: (5-2)/0.01 = 300 ohm)
    Component *res = add_comp(circuit, COMP_RESISTOR, x + 100, y, 0);
    res->props.resistor.resistance = 330.0;

    // LED
    Component *led = add_comp(circuit, COMP_LED, x + 200, y, 0);

    // Ground for LED
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 250, y + 80, 0);

    // Connect source negative to ground
    connect_terminals(circuit, vsrc, 1, gnd, 0);

    // Connect source positive to resistor
    connect_terminals(circuit, vsrc, 0, res, 0);

    // Connect resistor to LED anode
    connect_terminals(circuit, res, 1, led, 0);

    // Connect LED cathode to ground
    connect_terminals(circuit, led, 1, gnd2, 0);

    return 5;
}

int circuit_place_template(Circuit *circuit, CircuitTemplateType type, float x, float y) {
    if (!circuit) return 0;

    switch (type) {
        case CIRCUIT_RC_LOWPASS:
            return place_rc_lowpass(circuit, x, y);
        case CIRCUIT_RC_HIGHPASS:
            return place_rc_highpass(circuit, x, y);
        case CIRCUIT_RL_LOWPASS:
            return place_rl_lowpass(circuit, x, y);
        case CIRCUIT_RL_HIGHPASS:
            return place_rl_highpass(circuit, x, y);
        case CIRCUIT_VOLTAGE_DIVIDER:
            return place_voltage_divider(circuit, x, y);
        case CIRCUIT_INVERTING_AMP:
            return place_inverting_amp(circuit, x, y);
        case CIRCUIT_NONINVERTING_AMP:
            return place_noninverting_amp(circuit, x, y);
        case CIRCUIT_VOLTAGE_FOLLOWER:
            return place_voltage_follower(circuit, x, y);
        case CIRCUIT_HALFWAVE_RECT:
            return place_halfwave_rectifier(circuit, x, y);
        case CIRCUIT_LED_WITH_RESISTOR:
            return place_led_with_resistor(circuit, x, y);
        default:
            return 0;
    }
}
