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

// Helper to create an L-shaped wire connection via an intermediate point
// This avoids diagonal wires by going horizontal then vertical (or vice versa)
static void wire_L_shape(Circuit *circuit, float x1, float y1, float x2, float y2, bool horiz_first) {
    int node1 = circuit_find_or_create_node(circuit, x1, y1, 5.0f);
    int node2 = circuit_find_or_create_node(circuit, x2, y2, 5.0f);

    if (x1 == x2 || y1 == y2) {
        // Already aligned, just add direct wire
        if (node1 != node2) {
            circuit_add_wire(circuit, node1, node2);
        }
    } else {
        // Create intermediate node for L-shape
        float mid_x, mid_y;
        if (horiz_first) {
            mid_x = x2;
            mid_y = y1;
        } else {
            mid_x = x1;
            mid_y = y2;
        }
        int mid_node = circuit_find_or_create_node(circuit, mid_x, mid_y, 5.0f);

        if (node1 != mid_node) {
            circuit_add_wire(circuit, node1, mid_node);
        }
        if (mid_node != node2) {
            circuit_add_wire(circuit, mid_node, node2);
        }
    }
}

// RC Low Pass Filter: Vin --[R]--+--[C]-- GND
//                                |
//                               Vout
static int place_rc_lowpass(Circuit *circuit, float x, float y) {
    // Layout: horizontal wire at y, source offset so + terminal aligns
    // Source + terminal is at dy=-40, so place source at y+40 to put + at y

    // AC voltage source (offset down so + terminal is at y)
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 40, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 1.0;
    vsrc->props.ac_voltage.frequency = 1000.0;

    // Ground for source (- terminal at y+80, ground terminal at y+100-20=y+80)
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Resistor horizontal at y (terminals at x+60 and x+140)
    Component *res = add_comp(circuit, COMP_RESISTOR, x + 100, y, 0);
    res->props.resistor.resistance = 1000.0;

    // Capacitor vertical (rotation 90) - top terminal at y, bottom at y+80
    // With rotation 90, terminals move from (±40, 0) to (0, ±40)
    Component *cap = add_comp(circuit, COMP_CAPACITOR, x + 180, y + 40, 90);
    cap->props.capacitor.capacitance = 100e-9;

    // Ground for capacitor (terminal at y+100-20=y+80, matches cap bottom)
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 180, y + 100, 0);

    // Connect source negative to ground
    connect_terminals(circuit, vsrc, 1, gnd, 0);

    // Connect source positive to resistor (both at y)
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
    // Layout: horizontal wire at y, source offset so + terminal aligns

    // AC voltage source (offset down so + terminal is at y)
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 40, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 1.0;
    vsrc->props.ac_voltage.frequency = 1000.0;

    // Ground for source
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Capacitor horizontal at y
    Component *cap = add_comp(circuit, COMP_CAPACITOR, x + 100, y, 0);
    cap->props.capacitor.capacitance = 100e-9;

    // Resistor vertical (rotation 90) - top terminal at y
    Component *res = add_comp(circuit, COMP_RESISTOR, x + 180, y + 40, 90);
    res->props.resistor.resistance = 1000.0;

    // Ground for resistor
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 180, y + 100, 0);

    // Connect source negative to ground
    connect_terminals(circuit, vsrc, 1, gnd, 0);

    // Connect source positive to capacitor (both at y)
    connect_terminals(circuit, vsrc, 0, cap, 0);

    // Connect capacitor to resistor (output node)
    connect_terminals(circuit, cap, 1, res, 0);

    // Connect resistor to ground
    connect_terminals(circuit, res, 1, gnd2, 0);

    return 5;
}

// RL Low Pass Filter
static int place_rl_lowpass(Circuit *circuit, float x, float y) {
    // Layout: horizontal wire at y, source offset so + terminal aligns

    // AC voltage source (offset down so + terminal is at y)
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 40, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 1.0;
    vsrc->props.ac_voltage.frequency = 1000.0;

    // Ground for source
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Inductor horizontal at y
    Component *ind = add_comp(circuit, COMP_INDUCTOR, x + 100, y, 0);
    ind->props.inductor.inductance = 10e-3;

    // Resistor vertical (rotation 90) - top terminal at y
    Component *res = add_comp(circuit, COMP_RESISTOR, x + 180, y + 40, 90);
    res->props.resistor.resistance = 100.0;

    // Ground for resistor
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 180, y + 100, 0);

    connect_terminals(circuit, vsrc, 1, gnd, 0);
    connect_terminals(circuit, vsrc, 0, ind, 0);
    connect_terminals(circuit, ind, 1, res, 0);
    connect_terminals(circuit, res, 1, gnd2, 0);

    return 5;
}

// RL High Pass Filter
static int place_rl_highpass(Circuit *circuit, float x, float y) {
    // Layout: horizontal wire at y, source offset so + terminal aligns

    // AC voltage source (offset down so + terminal is at y)
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 40, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 1.0;
    vsrc->props.ac_voltage.frequency = 1000.0;

    // Ground for source
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Resistor horizontal at y
    Component *res = add_comp(circuit, COMP_RESISTOR, x + 100, y, 0);
    res->props.resistor.resistance = 100.0;

    // Inductor vertical (rotation 90) - top terminal at y
    Component *ind = add_comp(circuit, COMP_INDUCTOR, x + 180, y + 40, 90);
    ind->props.inductor.inductance = 10e-3;

    // Ground for inductor
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 180, y + 100, 0);

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
    // Layout: horizontal wire at y, source offset so + terminal aligns

    // DC voltage source (offset down so + terminal is at y)
    Component *vsrc = add_comp(circuit, COMP_DC_VOLTAGE, x, y + 40, 0);
    if (!vsrc) return 0;
    vsrc->props.dc_voltage.voltage = 10.0;

    // Ground for source
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // R1 horizontal at y
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 100, y, 0);
    r1->props.resistor.resistance = 10000.0;

    // R2 vertical (rotation 90) - top terminal at y
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 180, y + 40, 90);
    r2->props.resistor.resistance = 10000.0;

    // Ground for R2
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 180, y + 100, 0);

    connect_terminals(circuit, vsrc, 1, gnd, 0);
    connect_terminals(circuit, vsrc, 0, r1, 0);
    connect_terminals(circuit, r1, 1, r2, 0);
    connect_terminals(circuit, r2, 1, gnd2, 0);

    return 5;
}

// Inverting Amplifier:
//              +-------- Rf --------+
//              |                    |
// Vin ---[Ri]--+--(-)\              |
//                 (+)/--------+-----+--- Vout
//                  |          |
//                 GND       (output)
//
// Clean orthogonal layout with no diagonal wires
static int place_inverting_amp(Circuit *circuit, float x, float y) {
    // Layout plan (all wires horizontal or vertical):
    // - Source + at y, connects horizontally to Ri
    // - Ri output at junction node, connects to op-amp - input
    // - Rf above, connects junction to output via vertical wires
    // - + input connects down to ground

    // AC voltage source (offset so + terminal is at y)
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 40, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 0.5;
    vsrc->props.ac_voltage.frequency = 1000.0;

    // Ground for source
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Input resistor Ri horizontal at y
    Component *ri = add_comp(circuit, COMP_RESISTOR, x + 80, y, 0);
    ri->props.resistor.resistance = 1000.0;
    // Ri terminals: (x+40, y) and (x+120, y)

    // Op-amp positioned so - input aligns horizontally with Ri output
    // Op-amp - input at (opamp_x - 40, opamp_y - 20)
    // Want - input at (x+160, y), so opamp at (x+200, y+20)
    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 200, y + 20, 0);
    // Op-amp terminals: - at (x+160, y), + at (x+160, y+40), out at (x+240, y+20)

    // Feedback resistor Rf horizontal, above the signal path
    // Place at y-40 so it's above the op-amp
    Component *rf = add_comp(circuit, COMP_RESISTOR, x + 200, y - 40, 0);
    rf->props.resistor.resistance = 10000.0;
    // Rf terminals: (x+160, y-40) and (x+240, y-40)

    // Ground for + input
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 160, y + 60, 0);
    // Ground terminal at (x+160, y+40)

    // Connect source negative to ground
    connect_terminals(circuit, vsrc, 1, gnd, 0);

    // Connect source positive to Ri (horizontal at y)
    connect_terminals(circuit, vsrc, 0, ri, 0);

    // Connect Ri output to op-amp - input (horizontal at y)
    // Both at y level, so just connect directly
    connect_terminals(circuit, ri, 1, opamp, 0);

    // The junction node at - input needs to connect to Rf
    // Rf left terminal is at (x+160, y-40), - input is at (x+160, y)
    // This is a vertical connection - perfect!
    float inv_x, inv_y;
    component_get_terminal_pos(opamp, 0, &inv_x, &inv_y);
    float rf_left_x, rf_left_y;
    component_get_terminal_pos(rf, 0, &rf_left_x, &rf_left_y);

    // Create vertical wire from - input up to Rf left
    wire_L_shape(circuit, inv_x, inv_y, rf_left_x, rf_left_y, false);

    // Set Rf node connection
    int inv_node = circuit_find_or_create_node(circuit, inv_x, inv_y, 5.0f);
    rf->node_ids[0] = inv_node;

    // Connect Rf right to output via vertical wire
    // Rf right at (x+240, y-40), output at (x+240, y+20)
    float rf_right_x, rf_right_y;
    component_get_terminal_pos(rf, 1, &rf_right_x, &rf_right_y);
    float out_x, out_y;
    component_get_terminal_pos(opamp, 2, &out_x, &out_y);

    // Vertical wire from Rf right down to output
    wire_L_shape(circuit, rf_right_x, rf_right_y, out_x, out_y, false);

    // Set Rf and opamp output node connection
    int out_node = circuit_find_or_create_node(circuit, out_x, out_y, 5.0f);
    rf->node_ids[1] = out_node;
    opamp->node_ids[2] = out_node;

    // Connect + input to ground (vertical)
    connect_terminals(circuit, opamp, 1, gnd2, 0);

    return 6;
}

// Non-Inverting Amplifier:
//                +-------- Rf --------+
//                |                    |
// Vin -----(+)\  |                    |
//          (-)/--+--------------------+--- Vout
//           |
//          Ri
//           |
//          GND
//
// Clean orthogonal layout with no diagonal wires
static int place_noninverting_amp(Circuit *circuit, float x, float y) {
    // Layout plan:
    // - Source + connects horizontally to op-amp + input
    // - Op-amp - input connects to junction
    // - From junction: Ri goes down to ground, Rf goes up then right to output

    // AC voltage source (offset so + terminal is at y)
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 40, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 0.5;
    vsrc->props.ac_voltage.frequency = 1000.0;

    // Ground for source
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Op-amp positioned so + input aligns with source + terminal
    // Op-amp + input at (opamp_x - 40, opamp_y + 20)
    // Want + input at y level, so opamp_y + 20 = y, opamp_y = y - 20
    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 160, y - 20, 0);
    // Op-amp terminals: - at (x+120, y-40), + at (x+120, y), out at (x+200, y-20)

    // Ri vertical, from - input down to ground
    // - input is at (x+120, y-40)
    // Place Ri so its top terminal connects to - input
    // With rotation 90, terminal 0 is at (ri_x, ri_y - 40)
    // Want (ri_x, ri_y - 40) = (x+120, y-40), so ri_y = y
    Component *ri = add_comp(circuit, COMP_RESISTOR, x + 120, y, 90);
    ri->props.resistor.resistance = 1000.0;
    // Ri terminals: top at (x+120, y-40), bottom at (x+120, y+40)

    // Ground for Ri bottom
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 120, y + 60, 0);
    // Ground terminal at (x+120, y+40)

    // Rf horizontal, above the op-amp connecting - input to output
    // - input is at (x+120, y-40), output is at (x+200, y-20)
    // Place Rf at y-60 level (above - input)
    Component *rf = add_comp(circuit, COMP_RESISTOR, x + 160, y - 60, 0);
    rf->props.resistor.resistance = 10000.0;
    // Rf terminals: left at (x+120, y-60), right at (x+200, y-60)

    // Connect source negative to ground
    connect_terminals(circuit, vsrc, 1, gnd, 0);

    // Connect source positive to + input (horizontal at y)
    connect_terminals(circuit, vsrc, 0, opamp, 1);

    // Connect Ri top to - input (should be same point or close)
    float inv_x, inv_y;
    component_get_terminal_pos(opamp, 0, &inv_x, &inv_y);
    float ri_top_x, ri_top_y;
    component_get_terminal_pos(ri, 0, &ri_top_x, &ri_top_y);

    int inv_node = circuit_find_or_create_node(circuit, inv_x, inv_y, 5.0f);
    ri->node_ids[0] = inv_node;
    opamp->node_ids[0] = inv_node;

    // If Ri top and - input aren't at same position, add wire
    if (fabs(ri_top_x - inv_x) > 1 || fabs(ri_top_y - inv_y) > 1) {
        wire_L_shape(circuit, ri_top_x, ri_top_y, inv_x, inv_y, false);
    }

    // Connect Ri bottom to ground
    connect_terminals(circuit, ri, 1, gnd2, 0);

    // Connect Rf left to - input junction (vertical wire from y-40 to y-60)
    float rf_left_x, rf_left_y;
    component_get_terminal_pos(rf, 0, &rf_left_x, &rf_left_y);
    wire_L_shape(circuit, inv_x, inv_y, rf_left_x, rf_left_y, false);
    rf->node_ids[0] = inv_node;

    // Connect Rf right to output (vertical wire from y-60 to y-20)
    float rf_right_x, rf_right_y;
    component_get_terminal_pos(rf, 1, &rf_right_x, &rf_right_y);
    float out_x, out_y;
    component_get_terminal_pos(opamp, 2, &out_x, &out_y);

    wire_L_shape(circuit, rf_right_x, rf_right_y, out_x, out_y, false);

    int out_node = circuit_find_or_create_node(circuit, out_x, out_y, 5.0f);
    rf->node_ids[1] = out_node;
    opamp->node_ids[2] = out_node;

    return 6;
}

// Voltage Follower (Unity Gain Buffer):
//                  +-------+
//                  |       |
// Vin --------(+)\ |       |
//             (-)/--+------+--- Vout
//              |
//             GND (source)
//
// Clean orthogonal layout - feedback goes up, over, and down
static int place_voltage_follower(Circuit *circuit, float x, float y) {
    // AC voltage source (offset so + terminal is at y)
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 40, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 1.0;
    vsrc->props.ac_voltage.frequency = 1000.0;

    // Ground for source
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Op-amp positioned so + input aligns with source + terminal
    // Op-amp + input at (opamp_x - 40, opamp_y + 20)
    // Want + input at y, so opamp_y + 20 = y, opamp_y = y - 20
    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 160, y - 20, 0);
    // Op-amp terminals: - at (x+120, y-40), + at (x+120, y), out at (x+200, y-20)

    // Connect source negative to ground
    connect_terminals(circuit, vsrc, 1, gnd, 0);

    // Connect source positive to + input (horizontal at y)
    connect_terminals(circuit, vsrc, 0, opamp, 1);

    // Connect output to - input via L-shaped feedback wire
    // - input at (x+120, y-40), output at (x+200, y-20)
    // Route: output -> up to y-60 -> left to x+120 -> down to - input
    float inv_x, inv_y;
    component_get_terminal_pos(opamp, 0, &inv_x, &inv_y);
    float out_x, out_y;
    component_get_terminal_pos(opamp, 2, &out_x, &out_y);

    // Create feedback path with intermediate nodes (goes above the op-amp)
    float fb_y = y - 60;  // Feedback wire level (above op-amp)

    // Create nodes
    int inv_node = circuit_find_or_create_node(circuit, inv_x, inv_y, 5.0f);
    int out_node = circuit_find_or_create_node(circuit, out_x, out_y, 5.0f);
    int fb_left = circuit_find_or_create_node(circuit, inv_x, fb_y, 5.0f);
    int fb_right = circuit_find_or_create_node(circuit, out_x, fb_y, 5.0f);

    // Wire: - input up to feedback level
    circuit_add_wire(circuit, inv_node, fb_left);
    // Wire: horizontal along feedback level
    circuit_add_wire(circuit, fb_left, fb_right);
    // Wire: feedback level down to output
    circuit_add_wire(circuit, fb_right, out_node);

    // Set node connections
    opamp->node_ids[0] = inv_node;
    opamp->node_ids[2] = out_node;

    return 3;
}

// Half-Wave Rectifier:
// Vin ---[D]---+---[R]--- GND
//              |
//             Vout
static int place_halfwave_rectifier(Circuit *circuit, float x, float y) {
    // Layout: horizontal wire at y, source offset so + terminal aligns

    // AC voltage source (offset down so + terminal is at y)
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 40, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 5.0;
    vsrc->props.ac_voltage.frequency = 60.0;

    // Ground for source
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Diode horizontal at y
    Component *diode = add_comp(circuit, COMP_DIODE, x + 100, y, 0);

    // Load resistor vertical (rotation 90) - top terminal at y
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 180, y + 40, 90);
    rload->props.resistor.resistance = 1000.0;

    // Ground for load
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 180, y + 100, 0);

    // Connect source negative to ground
    connect_terminals(circuit, vsrc, 1, gnd, 0);

    // Connect source positive to diode anode (both at y)
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
    // Layout: horizontal wire at y, source offset so + terminal aligns

    // DC voltage source (5V, offset down so + terminal is at y)
    Component *vsrc = add_comp(circuit, COMP_DC_VOLTAGE, x, y + 40, 0);
    if (!vsrc) return 0;
    vsrc->props.dc_voltage.voltage = 5.0;

    // Ground for source
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Current limiting resistor horizontal at y (for ~10mA: (5-2)/0.01 = 300 ohm)
    Component *res = add_comp(circuit, COMP_RESISTOR, x + 100, y, 0);
    res->props.resistor.resistance = 330.0;

    // LED horizontal at y
    Component *led = add_comp(circuit, COMP_LED, x + 200, y, 0);

    // Ground for LED - place it below LED cathode
    // LED cathode is at x+240, y, so we need ground there
    // But ground terminal is at dy=-20, so place ground at (x+240, y+20)
    // to connect via short wire, or better: use vertical segment
    // Actually for a clean layout, bend the LED output down to ground
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 240, y + 40, 0);

    // Connect source negative to ground
    connect_terminals(circuit, vsrc, 1, gnd, 0);

    // Connect source positive to resistor (both at y)
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
