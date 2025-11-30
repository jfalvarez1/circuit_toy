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
    // Transistor amplifiers
    [CIRCUIT_COMMON_EMITTER] = {"Common Emitter", "CE", "BJT common-emitter amplifier"},
    [CIRCUIT_COMMON_SOURCE] = {"Common Source", "CS", "MOSFET common-source amplifier"},
    [CIRCUIT_COMMON_DRAIN] = {"Source Follower", "SF", "MOSFET source follower (common-drain)"},
    [CIRCUIT_MULTISTAGE_AMP] = {"Two-Stage Amp", "2Stg", "Two-stage BJT amplifier"},
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

// Common Emitter Amplifier:
//       Vcc
//        |
//       Rc
//        |
//  Cin --+-- Cout
//        |
//    R1--+--B  NPN  C--+
//        |      E     |
//       R2      |    Re
//        |     GND    |
//       GND          GND
//
static int place_common_emitter(Circuit *circuit, float x, float y) {
    // Vcc supply
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x + 160, y - 60, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 12.0;

    // Ground for Vcc (at bottom of source)
    Component *gnd_vcc = add_comp(circuit, COMP_GROUND, x + 160, y + 20, 0);

    // AC input source
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x - 40, y + 40, 0);
    vin->props.ac_voltage.amplitude = 0.1;  // 100mV input
    vin->props.ac_voltage.frequency = 1000.0;

    // Ground for input source
    Component *gnd_in = add_comp(circuit, COMP_GROUND, x - 40, y + 100, 0);

    // Input coupling capacitor (horizontal)
    Component *cin = add_comp(circuit, COMP_CAPACITOR, x + 40, y, 0);
    cin->props.capacitor.capacitance = 10e-6;  // 10uF

    // Bias resistors (R1 and R2 form voltage divider)
    // R1 from Vcc to base junction
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 120, y - 80, 90);
    r1->props.resistor.resistance = 47000.0;  // 47k

    // R2 from base junction to ground
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 120, y + 40, 90);
    r2->props.resistor.resistance = 10000.0;  // 10k

    // Ground for R2
    Component *gnd_r2 = add_comp(circuit, COMP_GROUND, x + 120, y + 100, 0);

    // NPN transistor - base at left, collector at top, emitter at bottom
    // With rotation 0: B at (-20,0), C at (0,-30), E at (0,30)
    Component *npn = add_comp(circuit, COMP_NPN_BJT, x + 180, y, 0);
    npn->props.bjt.bf = 100;  // Beta = 100

    // Collector resistor (Rc) - from Vcc to collector
    Component *rc = add_comp(circuit, COMP_RESISTOR, x + 180, y - 80, 90);
    rc->props.resistor.resistance = 2200.0;  // 2.2k

    // Emitter resistor (Re) - from emitter to ground
    Component *re = add_comp(circuit, COMP_RESISTOR, x + 180, y + 70, 90);
    re->props.resistor.resistance = 1000.0;  // 1k

    // Ground for Re
    Component *gnd_re = add_comp(circuit, COMP_GROUND, x + 180, y + 130, 0);

    // Output coupling capacitor
    Component *cout = add_comp(circuit, COMP_CAPACITOR, x + 260, y, 0);
    cout->props.capacitor.capacitance = 10e-6;

    // Text label for circuit
    Component *label = add_comp(circuit, COMP_TEXT, x + 80, y - 120, 0);
    strncpy(label->props.text.text, "Common Emitter Amp", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // Connections
    // Input source to ground
    connect_terminals(circuit, vin, 1, gnd_in, 0);
    // Vcc to ground
    connect_terminals(circuit, vcc, 1, gnd_vcc, 0);

    // Input source + to Cin
    connect_terminals(circuit, vin, 0, cin, 0);

    // Cin to base junction (and R1/R2 junction)
    float cin_out_x, cin_out_y;
    component_get_terminal_pos(cin, 1, &cin_out_x, &cin_out_y);
    float r1_bot_x, r1_bot_y;
    component_get_terminal_pos(r1, 1, &r1_bot_x, &r1_bot_y);
    float r2_top_x, r2_top_y;
    component_get_terminal_pos(r2, 0, &r2_top_x, &r2_top_y);
    float base_x, base_y;
    component_get_terminal_pos(npn, 0, &base_x, &base_y);

    // Create base junction node
    int base_node = circuit_find_or_create_node(circuit, r2_top_x, r2_top_y, 5.0f);
    r1->node_ids[1] = base_node;
    r2->node_ids[0] = base_node;

    // Wire from Cin to base junction
    wire_L_shape(circuit, cin_out_x, cin_out_y, r2_top_x, r2_top_y, true);
    cin->node_ids[1] = base_node;

    // Wire from base junction to BJT base
    wire_L_shape(circuit, r2_top_x, r2_top_y, base_x, base_y, true);
    npn->node_ids[0] = base_node;

    // R1 top to Vcc+
    float vcc_plus_x, vcc_plus_y;
    component_get_terminal_pos(vcc, 0, &vcc_plus_x, &vcc_plus_y);
    float r1_top_x, r1_top_y;
    component_get_terminal_pos(r1, 0, &r1_top_x, &r1_top_y);
    wire_L_shape(circuit, r1_top_x, r1_top_y, vcc_plus_x, vcc_plus_y, true);

    // Create Vcc node
    int vcc_node = circuit_find_or_create_node(circuit, vcc_plus_x, vcc_plus_y, 5.0f);
    r1->node_ids[0] = vcc_node;
    vcc->node_ids[0] = vcc_node;

    // Rc top to Vcc
    float rc_top_x, rc_top_y;
    component_get_terminal_pos(rc, 0, &rc_top_x, &rc_top_y);
    wire_L_shape(circuit, rc_top_x, rc_top_y, vcc_plus_x, vcc_plus_y, true);
    rc->node_ids[0] = vcc_node;

    // Rc bottom to collector
    float rc_bot_x, rc_bot_y;
    component_get_terminal_pos(rc, 1, &rc_bot_x, &rc_bot_y);
    float coll_x, coll_y;
    component_get_terminal_pos(npn, 1, &coll_x, &coll_y);
    wire_L_shape(circuit, rc_bot_x, rc_bot_y, coll_x, coll_y, false);
    int coll_node = circuit_find_or_create_node(circuit, coll_x, coll_y, 5.0f);
    rc->node_ids[1] = coll_node;
    npn->node_ids[1] = coll_node;

    // Collector to Cout
    float cout_in_x, cout_in_y;
    component_get_terminal_pos(cout, 0, &cout_in_x, &cout_in_y);
    wire_L_shape(circuit, coll_x, coll_y, cout_in_x, cout_in_y, true);
    cout->node_ids[0] = coll_node;

    // R2 bottom to ground
    connect_terminals(circuit, r2, 1, gnd_r2, 0);

    // Emitter to Re top
    float emit_x, emit_y;
    component_get_terminal_pos(npn, 2, &emit_x, &emit_y);
    float re_top_x, re_top_y;
    component_get_terminal_pos(re, 0, &re_top_x, &re_top_y);
    wire_L_shape(circuit, emit_x, emit_y, re_top_x, re_top_y, false);
    int emit_node = circuit_find_or_create_node(circuit, emit_x, emit_y, 5.0f);
    npn->node_ids[2] = emit_node;
    re->node_ids[0] = emit_node;

    // Re bottom to ground
    connect_terminals(circuit, re, 1, gnd_re, 0);

    return 14;
}

// Common Source Amplifier (NMOS):
//       Vdd
//        |
//       Rd
//        |
//  Cin --+-- Cout
//        |
//       G   NMOS  D--+
//            S       |
//            |      Rs
//           GND      |
//                   GND
//
static int place_common_source(Circuit *circuit, float x, float y) {
    // Vdd supply
    Component *vdd = add_comp(circuit, COMP_DC_VOLTAGE, x + 160, y - 60, 0);
    if (!vdd) return 0;
    vdd->props.dc_voltage.voltage = 12.0;

    // Ground for Vdd
    Component *gnd_vdd = add_comp(circuit, COMP_GROUND, x + 160, y + 20, 0);

    // AC input source with DC bias
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x - 40, y + 40, 0);
    vin->props.ac_voltage.amplitude = 0.1;
    vin->props.ac_voltage.frequency = 1000.0;
    vin->props.ac_voltage.offset = 2.0;  // DC bias for gate

    // Ground for input source
    Component *gnd_in = add_comp(circuit, COMP_GROUND, x - 40, y + 100, 0);

    // Input coupling capacitor
    Component *cin = add_comp(circuit, COMP_CAPACITOR, x + 40, y, 0);
    cin->props.capacitor.capacitance = 10e-6;

    // Gate bias resistor to ground (provides DC return path)
    Component *rg = add_comp(circuit, COMP_RESISTOR, x + 100, y + 40, 90);
    rg->props.resistor.resistance = 1000000.0;  // 1M

    // Ground for Rg
    Component *gnd_rg = add_comp(circuit, COMP_GROUND, x + 100, y + 100, 0);

    // NMOS transistor - gate at left, drain at top, source at bottom
    // With rotation 0: G at (-20,0), D at (0,-30), S at (0,30)
    Component *nmos = add_comp(circuit, COMP_NMOS, x + 160, y, 0);
    nmos->props.mosfet.vth = 1.5;
    nmos->props.mosfet.kp = 0.01;

    // Drain resistor (Rd)
    Component *rd = add_comp(circuit, COMP_RESISTOR, x + 160, y - 80, 90);
    rd->props.resistor.resistance = 2200.0;

    // Source resistor (Rs) with bypass cap
    Component *rs = add_comp(circuit, COMP_RESISTOR, x + 160, y + 70, 90);
    rs->props.resistor.resistance = 470.0;

    // Ground for Rs
    Component *gnd_rs = add_comp(circuit, COMP_GROUND, x + 160, y + 130, 0);

    // Output coupling capacitor
    Component *cout = add_comp(circuit, COMP_CAPACITOR, x + 240, y, 0);
    cout->props.capacitor.capacitance = 10e-6;

    // Text label
    Component *label = add_comp(circuit, COMP_TEXT, x + 80, y - 120, 0);
    strncpy(label->props.text.text, "Common Source Amp", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // Connections (similar to common emitter)
    connect_terminals(circuit, vin, 1, gnd_in, 0);
    connect_terminals(circuit, vdd, 1, gnd_vdd, 0);
    connect_terminals(circuit, vin, 0, cin, 0);

    // Gate junction
    float cin_out_x, cin_out_y;
    component_get_terminal_pos(cin, 1, &cin_out_x, &cin_out_y);
    float rg_top_x, rg_top_y;
    component_get_terminal_pos(rg, 0, &rg_top_x, &rg_top_y);
    float gate_x, gate_y;
    component_get_terminal_pos(nmos, 0, &gate_x, &gate_y);

    int gate_node = circuit_find_or_create_node(circuit, rg_top_x, rg_top_y, 5.0f);
    rg->node_ids[0] = gate_node;
    wire_L_shape(circuit, cin_out_x, cin_out_y, rg_top_x, rg_top_y, true);
    cin->node_ids[1] = gate_node;
    wire_L_shape(circuit, rg_top_x, rg_top_y, gate_x, gate_y, true);
    nmos->node_ids[0] = gate_node;

    // Rg to ground
    connect_terminals(circuit, rg, 1, gnd_rg, 0);

    // Vdd to Rd
    float vdd_plus_x, vdd_plus_y;
    component_get_terminal_pos(vdd, 0, &vdd_plus_x, &vdd_plus_y);
    float rd_top_x, rd_top_y;
    component_get_terminal_pos(rd, 0, &rd_top_x, &rd_top_y);
    wire_L_shape(circuit, rd_top_x, rd_top_y, vdd_plus_x, vdd_plus_y, true);
    int vdd_node = circuit_find_or_create_node(circuit, vdd_plus_x, vdd_plus_y, 5.0f);
    rd->node_ids[0] = vdd_node;
    vdd->node_ids[0] = vdd_node;

    // Rd to drain
    float rd_bot_x, rd_bot_y;
    component_get_terminal_pos(rd, 1, &rd_bot_x, &rd_bot_y);
    float drain_x, drain_y;
    component_get_terminal_pos(nmos, 1, &drain_x, &drain_y);
    wire_L_shape(circuit, rd_bot_x, rd_bot_y, drain_x, drain_y, false);
    int drain_node = circuit_find_or_create_node(circuit, drain_x, drain_y, 5.0f);
    rd->node_ids[1] = drain_node;
    nmos->node_ids[1] = drain_node;

    // Drain to Cout
    float cout_in_x, cout_in_y;
    component_get_terminal_pos(cout, 0, &cout_in_x, &cout_in_y);
    wire_L_shape(circuit, drain_x, drain_y, cout_in_x, cout_in_y, true);
    cout->node_ids[0] = drain_node;

    // Source to Rs
    float source_x, source_y;
    component_get_terminal_pos(nmos, 2, &source_x, &source_y);
    float rs_top_x, rs_top_y;
    component_get_terminal_pos(rs, 0, &rs_top_x, &rs_top_y);
    wire_L_shape(circuit, source_x, source_y, rs_top_x, rs_top_y, false);
    int source_node = circuit_find_or_create_node(circuit, source_x, source_y, 5.0f);
    nmos->node_ids[2] = source_node;
    rs->node_ids[0] = source_node;

    // Rs to ground
    connect_terminals(circuit, rs, 1, gnd_rs, 0);

    return 13;
}

// Common Drain (Source Follower):
//       Vdd
//        |
//   Vin--G   NMOS  D
//            S
//            |
//           Rs--Vout
//            |
//           GND
//
static int place_common_drain(Circuit *circuit, float x, float y) {
    // Vdd supply
    Component *vdd = add_comp(circuit, COMP_DC_VOLTAGE, x + 160, y - 100, 0);
    if (!vdd) return 0;
    vdd->props.dc_voltage.voltage = 12.0;

    // Ground for Vdd
    Component *gnd_vdd = add_comp(circuit, COMP_GROUND, x + 160, y - 20, 0);

    // AC input source with DC bias
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x - 40, y + 40, 0);
    vin->props.ac_voltage.amplitude = 1.0;
    vin->props.ac_voltage.frequency = 1000.0;
    vin->props.ac_voltage.offset = 6.0;  // DC bias

    // Ground for input source
    Component *gnd_in = add_comp(circuit, COMP_GROUND, x - 40, y + 100, 0);

    // Input coupling capacitor
    Component *cin = add_comp(circuit, COMP_CAPACITOR, x + 40, y, 0);
    cin->props.capacitor.capacitance = 10e-6;

    // NMOS transistor - drain connected directly to Vdd
    Component *nmos = add_comp(circuit, COMP_NMOS, x + 160, y, 0);
    nmos->props.mosfet.vth = 1.5;
    nmos->props.mosfet.kp = 0.02;

    // Source resistor (Rs) - load resistor
    Component *rs = add_comp(circuit, COMP_RESISTOR, x + 160, y + 70, 90);
    rs->props.resistor.resistance = 1000.0;

    // Ground for Rs
    Component *gnd_rs = add_comp(circuit, COMP_GROUND, x + 160, y + 130, 0);

    // Output coupling capacitor (from source)
    Component *cout = add_comp(circuit, COMP_CAPACITOR, x + 240, y + 30, 0);
    cout->props.capacitor.capacitance = 10e-6;

    // Text label
    Component *label = add_comp(circuit, COMP_TEXT, x + 60, y - 120, 0);
    strncpy(label->props.text.text, "Source Follower (Gain~1)", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // Connections
    connect_terminals(circuit, vin, 1, gnd_in, 0);
    connect_terminals(circuit, vdd, 1, gnd_vdd, 0);
    connect_terminals(circuit, vin, 0, cin, 0);

    // Cin to gate
    float cin_out_x, cin_out_y;
    component_get_terminal_pos(cin, 1, &cin_out_x, &cin_out_y);
    float gate_x, gate_y;
    component_get_terminal_pos(nmos, 0, &gate_x, &gate_y);
    wire_L_shape(circuit, cin_out_x, cin_out_y, gate_x, gate_y, true);
    int gate_node = circuit_find_or_create_node(circuit, gate_x, gate_y, 5.0f);
    cin->node_ids[1] = gate_node;
    nmos->node_ids[0] = gate_node;

    // Vdd+ to drain
    float vdd_plus_x, vdd_plus_y;
    component_get_terminal_pos(vdd, 0, &vdd_plus_x, &vdd_plus_y);
    float drain_x, drain_y;
    component_get_terminal_pos(nmos, 1, &drain_x, &drain_y);
    wire_L_shape(circuit, vdd_plus_x, vdd_plus_y, drain_x, drain_y, false);
    int vdd_node = circuit_find_or_create_node(circuit, vdd_plus_x, vdd_plus_y, 5.0f);
    vdd->node_ids[0] = vdd_node;
    nmos->node_ids[1] = vdd_node;

    // Source to Rs
    float source_x, source_y;
    component_get_terminal_pos(nmos, 2, &source_x, &source_y);
    float rs_top_x, rs_top_y;
    component_get_terminal_pos(rs, 0, &rs_top_x, &rs_top_y);
    wire_L_shape(circuit, source_x, source_y, rs_top_x, rs_top_y, false);
    int source_node = circuit_find_or_create_node(circuit, source_x, source_y, 5.0f);
    nmos->node_ids[2] = source_node;
    rs->node_ids[0] = source_node;

    // Source to Cout (output taken from source)
    float cout_in_x, cout_in_y;
    component_get_terminal_pos(cout, 0, &cout_in_x, &cout_in_y);
    wire_L_shape(circuit, source_x, source_y, cout_in_x, cout_in_y, true);
    cout->node_ids[0] = source_node;

    // Rs to ground
    connect_terminals(circuit, rs, 1, gnd_rs, 0);

    return 10;
}

// Two-Stage BJT Amplifier (CE-CE cascade):
// Two common-emitter stages cascaded for higher gain
static int place_multistage_amp(Circuit *circuit, float x, float y) {
    // Vcc supply
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x + 200, y - 80, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 12.0;

    // Ground for Vcc
    Component *gnd_vcc = add_comp(circuit, COMP_GROUND, x + 200, y, 0);

    // AC input source
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x - 60, y + 40, 0);
    vin->props.ac_voltage.amplitude = 0.01;  // 10mV - small input due to high gain
    vin->props.ac_voltage.frequency = 1000.0;

    // Ground for input source
    Component *gnd_in = add_comp(circuit, COMP_GROUND, x - 60, y + 100, 0);

    // Input coupling cap
    Component *c1 = add_comp(circuit, COMP_CAPACITOR, x + 20, y, 0);
    c1->props.capacitor.capacitance = 10e-6;

    // === STAGE 1 ===
    // Bias resistors
    Component *r1a = add_comp(circuit, COMP_RESISTOR, x + 80, y - 80, 90);
    r1a->props.resistor.resistance = 47000.0;

    Component *r2a = add_comp(circuit, COMP_RESISTOR, x + 80, y + 40, 90);
    r2a->props.resistor.resistance = 10000.0;

    Component *gnd_r2a = add_comp(circuit, COMP_GROUND, x + 80, y + 100, 0);

    // First transistor
    Component *q1 = add_comp(circuit, COMP_NPN_BJT, x + 140, y, 0);
    q1->props.bjt.bf = 100;

    // Collector resistor
    Component *rc1 = add_comp(circuit, COMP_RESISTOR, x + 140, y - 80, 90);
    rc1->props.resistor.resistance = 4700.0;

    // Emitter resistor
    Component *re1 = add_comp(circuit, COMP_RESISTOR, x + 140, y + 70, 90);
    re1->props.resistor.resistance = 1000.0;

    Component *gnd_re1 = add_comp(circuit, COMP_GROUND, x + 140, y + 130, 0);

    // Interstage coupling cap
    Component *c2 = add_comp(circuit, COMP_CAPACITOR, x + 220, y, 0);
    c2->props.capacitor.capacitance = 10e-6;

    // === STAGE 2 ===
    Component *r1b = add_comp(circuit, COMP_RESISTOR, x + 280, y - 80, 90);
    r1b->props.resistor.resistance = 47000.0;

    Component *r2b = add_comp(circuit, COMP_RESISTOR, x + 280, y + 40, 90);
    r2b->props.resistor.resistance = 10000.0;

    Component *gnd_r2b = add_comp(circuit, COMP_GROUND, x + 280, y + 100, 0);

    Component *q2 = add_comp(circuit, COMP_NPN_BJT, x + 340, y, 0);
    q2->props.bjt.bf = 100;

    Component *rc2 = add_comp(circuit, COMP_RESISTOR, x + 340, y - 80, 90);
    rc2->props.resistor.resistance = 4700.0;

    Component *re2 = add_comp(circuit, COMP_RESISTOR, x + 340, y + 70, 90);
    re2->props.resistor.resistance = 1000.0;

    Component *gnd_re2 = add_comp(circuit, COMP_GROUND, x + 340, y + 130, 0);

    // Output coupling cap
    Component *c3 = add_comp(circuit, COMP_CAPACITOR, x + 420, y, 0);
    c3->props.capacitor.capacitance = 10e-6;

    // Text label
    Component *label = add_comp(circuit, COMP_TEXT, x + 140, y - 140, 0);
    strncpy(label->props.text.text, "Two-Stage CE Amplifier", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // Connections
    connect_terminals(circuit, vin, 1, gnd_in, 0);
    connect_terminals(circuit, vcc, 1, gnd_vcc, 0);
    connect_terminals(circuit, vin, 0, c1, 0);
    connect_terminals(circuit, r2a, 1, gnd_r2a, 0);
    connect_terminals(circuit, re1, 1, gnd_re1, 0);
    connect_terminals(circuit, r2b, 1, gnd_r2b, 0);
    connect_terminals(circuit, re2, 1, gnd_re2, 0);

    // Get Vcc node
    float vcc_plus_x, vcc_plus_y;
    component_get_terminal_pos(vcc, 0, &vcc_plus_x, &vcc_plus_y);
    int vcc_node = circuit_find_or_create_node(circuit, vcc_plus_x, vcc_plus_y, 5.0f);
    vcc->node_ids[0] = vcc_node;

    // Stage 1 connections
    // C1 to base junction
    float c1_out_x, c1_out_y;
    component_get_terminal_pos(c1, 1, &c1_out_x, &c1_out_y);
    float r2a_top_x, r2a_top_y;
    component_get_terminal_pos(r2a, 0, &r2a_top_x, &r2a_top_y);
    int base1_node = circuit_find_or_create_node(circuit, r2a_top_x, r2a_top_y, 5.0f);

    wire_L_shape(circuit, c1_out_x, c1_out_y, r2a_top_x, r2a_top_y, true);
    c1->node_ids[1] = base1_node;
    r1a->node_ids[1] = base1_node;
    r2a->node_ids[0] = base1_node;

    float base1_x, base1_y;
    component_get_terminal_pos(q1, 0, &base1_x, &base1_y);
    wire_L_shape(circuit, r2a_top_x, r2a_top_y, base1_x, base1_y, true);
    q1->node_ids[0] = base1_node;

    // R1a to Vcc
    float r1a_top_x, r1a_top_y;
    component_get_terminal_pos(r1a, 0, &r1a_top_x, &r1a_top_y);
    wire_L_shape(circuit, r1a_top_x, r1a_top_y, vcc_plus_x, vcc_plus_y, true);
    r1a->node_ids[0] = vcc_node;

    // Rc1 to Vcc
    float rc1_top_x, rc1_top_y;
    component_get_terminal_pos(rc1, 0, &rc1_top_x, &rc1_top_y);
    wire_L_shape(circuit, rc1_top_x, rc1_top_y, vcc_plus_x, vcc_plus_y, true);
    rc1->node_ids[0] = vcc_node;

    // Rc1 to collector
    float rc1_bot_x, rc1_bot_y;
    component_get_terminal_pos(rc1, 1, &rc1_bot_x, &rc1_bot_y);
    float coll1_x, coll1_y;
    component_get_terminal_pos(q1, 1, &coll1_x, &coll1_y);
    wire_L_shape(circuit, rc1_bot_x, rc1_bot_y, coll1_x, coll1_y, false);
    int coll1_node = circuit_find_or_create_node(circuit, coll1_x, coll1_y, 5.0f);
    rc1->node_ids[1] = coll1_node;
    q1->node_ids[1] = coll1_node;

    // Emitter to Re1
    float emit1_x, emit1_y;
    component_get_terminal_pos(q1, 2, &emit1_x, &emit1_y);
    float re1_top_x, re1_top_y;
    component_get_terminal_pos(re1, 0, &re1_top_x, &re1_top_y);
    wire_L_shape(circuit, emit1_x, emit1_y, re1_top_x, re1_top_y, false);
    int emit1_node = circuit_find_or_create_node(circuit, emit1_x, emit1_y, 5.0f);
    q1->node_ids[2] = emit1_node;
    re1->node_ids[0] = emit1_node;

    // Interstage: collector1 to C2
    float c2_in_x, c2_in_y;
    component_get_terminal_pos(c2, 0, &c2_in_x, &c2_in_y);
    wire_L_shape(circuit, coll1_x, coll1_y, c2_in_x, c2_in_y, true);
    c2->node_ids[0] = coll1_node;

    // Stage 2 - same pattern
    float c2_out_x, c2_out_y;
    component_get_terminal_pos(c2, 1, &c2_out_x, &c2_out_y);
    float r2b_top_x, r2b_top_y;
    component_get_terminal_pos(r2b, 0, &r2b_top_x, &r2b_top_y);
    int base2_node = circuit_find_or_create_node(circuit, r2b_top_x, r2b_top_y, 5.0f);

    wire_L_shape(circuit, c2_out_x, c2_out_y, r2b_top_x, r2b_top_y, true);
    c2->node_ids[1] = base2_node;
    r1b->node_ids[1] = base2_node;
    r2b->node_ids[0] = base2_node;

    float base2_x, base2_y;
    component_get_terminal_pos(q2, 0, &base2_x, &base2_y);
    wire_L_shape(circuit, r2b_top_x, r2b_top_y, base2_x, base2_y, true);
    q2->node_ids[0] = base2_node;

    // R1b to Vcc
    float r1b_top_x, r1b_top_y;
    component_get_terminal_pos(r1b, 0, &r1b_top_x, &r1b_top_y);
    wire_L_shape(circuit, r1b_top_x, r1b_top_y, vcc_plus_x, vcc_plus_y, true);
    r1b->node_ids[0] = vcc_node;

    // Rc2 to Vcc
    float rc2_top_x, rc2_top_y;
    component_get_terminal_pos(rc2, 0, &rc2_top_x, &rc2_top_y);
    wire_L_shape(circuit, rc2_top_x, rc2_top_y, vcc_plus_x, vcc_plus_y, true);
    rc2->node_ids[0] = vcc_node;

    // Rc2 to collector
    float rc2_bot_x, rc2_bot_y;
    component_get_terminal_pos(rc2, 1, &rc2_bot_x, &rc2_bot_y);
    float coll2_x, coll2_y;
    component_get_terminal_pos(q2, 1, &coll2_x, &coll2_y);
    wire_L_shape(circuit, rc2_bot_x, rc2_bot_y, coll2_x, coll2_y, false);
    int coll2_node = circuit_find_or_create_node(circuit, coll2_x, coll2_y, 5.0f);
    rc2->node_ids[1] = coll2_node;
    q2->node_ids[1] = coll2_node;

    // Emitter to Re2
    float emit2_x, emit2_y;
    component_get_terminal_pos(q2, 2, &emit2_x, &emit2_y);
    float re2_top_x, re2_top_y;
    component_get_terminal_pos(re2, 0, &re2_top_x, &re2_top_y);
    wire_L_shape(circuit, emit2_x, emit2_y, re2_top_x, re2_top_y, false);
    int emit2_node = circuit_find_or_create_node(circuit, emit2_x, emit2_y, 5.0f);
    q2->node_ids[2] = emit2_node;
    re2->node_ids[0] = emit2_node;

    // Output: collector2 to C3
    float c3_in_x, c3_in_y;
    component_get_terminal_pos(c3, 0, &c3_in_x, &c3_in_y);
    wire_L_shape(circuit, coll2_x, coll2_y, c3_in_x, c3_in_y, true);
    c3->node_ids[0] = coll2_node;

    return 23;
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
        case CIRCUIT_COMMON_EMITTER:
            return place_common_emitter(circuit, x, y);
        case CIRCUIT_COMMON_SOURCE:
            return place_common_source(circuit, x, y);
        case CIRCUIT_COMMON_DRAIN:
            return place_common_drain(circuit, x, y);
        case CIRCUIT_MULTISTAGE_AMP:
            return place_multistage_amp(circuit, x, y);
        default:
            return 0;
    }
}
