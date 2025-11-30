/**
 * Circuit Playground - Predefined Circuit Templates Implementation
 *
 * =============================================================================
 * CIRCUIT BUILDING DOCUMENTATION
 * =============================================================================
 *
 * This file contains predefined circuit templates. Follow these guidelines
 * when creating new circuits to ensure clean, non-overlapping layouts.
 *
 * -----------------------------------------------------------------------------
 * COMPONENT TERMINAL POSITIONS (relative to component center, before rotation)
 * -----------------------------------------------------------------------------
 *
 * 2-Terminal Components (horizontal orientation):
 *   - Resistor, Capacitor, Inductor, Diode: terminals at (-40, 0) and (40, 0)
 *   - Voltage/Current Sources: terminals at (0, -40) [+] and (0, 40) [-]
 *   - Ground: single terminal at (0, -20)
 *
 * 3-Terminal Components:
 *   - BJT (NPN/PNP): Base at (-20, 0), Collector at (20, -20), Emitter at (20, 20)
 *   - MOSFET (NMOS/PMOS): Gate at (-20, 0), Drain at (20, -20), Source at (20, 20)
 *   - Op-Amp: Inverting(-) at (-40, -20), Non-inverting(+) at (-40, 20), Output at (40, 0)
 *
 * Rotation effects (90-degree increments):
 *   - Rotation transforms terminal positions: (dx, dy) -> (dx*cos - dy*sin, dx*sin + dy*cos)
 *   - rotation=0:   terminals at original positions
 *   - rotation=90:  swap x/y and negate new x (rotate clockwise)
 *   - rotation=180: negate both x and y
 *   - rotation=270: swap x/y and negate new y
 *
 * Example: Resistor at rotation=90 has terminals at (0, -40) and (0, 40)
 *
 * -----------------------------------------------------------------------------
 * WIRE ROUTING RULES (Non-overlapping)
 * -----------------------------------------------------------------------------
 *
 * 1. ONLY use horizontal and vertical wires - never diagonal
 *
 * 2. For each connection between two points that aren't aligned:
 *    - Create an L-shaped path using an intermediate corner node
 *    - Use circuit_add_wire() for each straight segment
 *    - Create corner nodes with circuit_find_or_create_node()
 *
 * 3. When multiple components connect to the same node:
 *    - Each component gets its own wire path to the junction
 *    - Don't share wire segments between different connections
 *    - Route wires on separate horizontal/vertical tracks
 *
 * 4. Power bus pattern (for Vcc/Vdd):
 *    - Create a horizontal bus at the top (y = vcc_y)
 *    - Drop vertical wires down from the bus to each component
 *    - Example:
 *        Vcc+ --+----+----+----+
 *               |    |    |    |
 *              R1   R2   R3   R4
 *
 * 5. Ground connections:
 *    - Use direct connect_terminals() for simple ground connections
 *    - For complex circuits, use explicit wire routing
 *
 * -----------------------------------------------------------------------------
 * CIRCUIT BUILDING PATTERN
 * -----------------------------------------------------------------------------
 *
 * static int place_example_circuit(Circuit *circuit, float x, float y) {
 *     // 1. CREATE COMPONENTS at grid-aligned positions
 *     Component *vsrc = add_comp(circuit, COMP_DC_VOLTAGE, x, y + 40, 0);
 *     Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 100, y, 0);
 *     // ... etc
 *
 *     // 2. SET COMPONENT PROPERTIES
 *     vsrc->props.dc_voltage.voltage = 12.0;
 *     r1->props.resistor.resistance = 1000.0;
 *
 *     // 3. SIMPLE DIRECT CONNECTIONS (for aligned terminals)
 *     connect_terminals(circuit, vsrc, 1, gnd, 0);
 *
 *     // 4. GET TERMINAL POSITIONS for complex routing
 *     float tx, ty;
 *     component_get_terminal_pos(r1, 0, &tx, &ty);
 *
 *     // 5. CREATE EXPLICIT WIRE PATHS for non-trivial routing
 *     // Junction node where multiple components connect
 *     int junction = circuit_find_or_create_node(circuit, jx, jy, 5.0f);
 *
 *     // Wire from component A to junction (horizontal)
 *     circuit_add_wire(circuit, circuit_find_or_create_node(circuit, ax, ay, 5.0f), junction);
 *
 *     // Wire from junction to component B (with L-shape)
 *     // First horizontal segment to corner
 *     int corner = circuit_find_or_create_node(circuit, bx, jy, 5.0f);
 *     circuit_add_wire(circuit, junction, corner);
 *     // Then vertical segment to destination
 *     circuit_add_wire(circuit, corner, circuit_find_or_create_node(circuit, bx, by, 5.0f));
 *
 *     // 6. SET NODE IDs on components
 *     r1->node_ids[0] = junction;
 *
 *     // 7. Return component count
 *     return num_components;
 * }
 *
 * -----------------------------------------------------------------------------
 * HELPER FUNCTIONS
 * -----------------------------------------------------------------------------
 *
 * add_comp(circuit, type, x, y, rotation)
 *   - Creates and adds a component to the circuit
 *   - Returns NULL on failure
 *
 * connect_terminals(circuit, comp1, terminal1, comp2, terminal2)
 *   - Connects two terminals with automatic wire routing
 *   - Best for simple, aligned connections
 *
 * create_node_at(circuit, comp, terminal_idx)
 *   - Creates a node at a component's terminal position
 *   - Returns the node ID
 *
 * wire_L_shape(circuit, x1, y1, x2, y2, horiz_first)
 *   - Creates an L-shaped wire path (USE SPARINGLY - prefer explicit routing)
 *   - horiz_first: if true, goes horizontal then vertical
 *
 * circuit_add_wire(circuit, node1_id, node2_id)
 *   - Adds a single wire segment between two nodes
 *   - PREFERRED for explicit routing control
 *
 * circuit_find_or_create_node(circuit, x, y, threshold)
 *   - Finds existing node or creates new one at position
 *   - Returns node ID
 *
 * component_get_terminal_pos(comp, terminal_idx, &x, &y)
 *   - Gets world position of a terminal (accounts for rotation)
 *
 * =============================================================================
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
    // Additional transistor circuits
    [CIRCUIT_DIFFERENTIAL_PAIR] = {"Differential Pair", "Diff", "BJT differential amplifier"},
    [CIRCUIT_CURRENT_MIRROR] = {"Current Mirror", "CMir", "BJT current mirror"},
    [CIRCUIT_PUSH_PULL] = {"Push-Pull", "PP", "Complementary push-pull output stage"},
    [CIRCUIT_CMOS_INVERTER] = {"CMOS Inverter", "CMOS", "CMOS logic inverter"},
    // Additional op-amp circuits
    [CIRCUIT_INTEGRATOR] = {"Integrator", "Int", "Op-amp integrator circuit"},
    [CIRCUIT_DIFFERENTIATOR] = {"Differentiator", "Dif", "Op-amp differentiator circuit"},
    [CIRCUIT_SUMMING_AMP] = {"Summing Amp", "Sum", "Inverting summing amplifier"},
    [CIRCUIT_COMPARATOR] = {"Comparator", "Cmp", "Op-amp voltage comparator"},
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
    // Common Emitter Amplifier with voltage divider bias
    // Layout designed for clean, non-overlapping wire routing
    //
    //    Vcc+ ----+-------- Rc --------+
    //            |                     |
    //           R1                 Collector
    //            |                     |
    //    Vin--Cin+--------- Base --NPN--+-- Cout
    //            |                     |
    //           R2                 Emitter
    //            |                     |
    //           GND         Re        GND
    //                        |
    //                       GND

    // === POWER SUPPLY (left column) ===
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x - 100, y - 80, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 12.0;
    Component *gnd_vcc = add_comp(circuit, COMP_GROUND, x - 100, y, 0);

    // === INPUT SOURCE (left column, below) ===
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x - 100, y + 80, 0);
    vin->props.ac_voltage.amplitude = 0.1;
    vin->props.ac_voltage.frequency = 1000.0;
    Component *gnd_in = add_comp(circuit, COMP_GROUND, x - 100, y + 160, 0);

    // === INPUT COUPLING (horizontal) ===
    Component *cin = add_comp(circuit, COMP_CAPACITOR, x, y + 40, 0);
    cin->props.capacitor.capacitance = 10e-6;

    // === BIAS NETWORK (column at x+80) ===
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 80, y - 40, 90);  // Upper bias
    r1->props.resistor.resistance = 47000.0;
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 80, y + 80, 90);  // Lower bias
    r2->props.resistor.resistance = 10000.0;
    Component *gnd_r2 = add_comp(circuit, COMP_GROUND, x + 80, y + 140, 0);

    // === TRANSISTOR (center-right) ===
    // BJT terminals: B at (-20,0), C at (20,-20), E at (20,20)
    Component *npn = add_comp(circuit, COMP_NPN_BJT, x + 160, y + 40, 0);
    npn->props.bjt.bf = 100;

    // === COLLECTOR RESISTOR (above transistor) ===
    Component *rc = add_comp(circuit, COMP_RESISTOR, x + 180, y - 40, 90);
    rc->props.resistor.resistance = 2200.0;

    // === EMITTER RESISTOR (below transistor) ===
    Component *re = add_comp(circuit, COMP_RESISTOR, x + 180, y + 100, 90);
    re->props.resistor.resistance = 1000.0;
    Component *gnd_re = add_comp(circuit, COMP_GROUND, x + 180, y + 160, 0);

    // === OUTPUT COUPLING ===
    Component *cout = add_comp(circuit, COMP_CAPACITOR, x + 260, y + 20, 0);
    cout->props.capacitor.capacitance = 10e-6;

    // Label
    Component *label = add_comp(circuit, COMP_TEXT, x + 60, y - 100, 0);
    strncpy(label->props.text.text, "Common Emitter Amp", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // === CONNECTIONS ===
    // Direct ground connections
    connect_terminals(circuit, vcc, 1, gnd_vcc, 0);
    connect_terminals(circuit, vin, 1, gnd_in, 0);
    connect_terminals(circuit, r2, 1, gnd_r2, 0);
    connect_terminals(circuit, re, 1, gnd_re, 0);

    // Input signal to coupling cap
    connect_terminals(circuit, vin, 0, cin, 0);

    // Get terminal positions
    float vcc_x, vcc_y;
    component_get_terminal_pos(vcc, 0, &vcc_x, &vcc_y);
    float r1_top_x, r1_top_y, r1_bot_x, r1_bot_y;
    component_get_terminal_pos(r1, 0, &r1_top_x, &r1_top_y);
    component_get_terminal_pos(r1, 1, &r1_bot_x, &r1_bot_y);
    float r2_top_x, r2_top_y;
    component_get_terminal_pos(r2, 0, &r2_top_x, &r2_top_y);
    float cin_out_x, cin_out_y;
    component_get_terminal_pos(cin, 1, &cin_out_x, &cin_out_y);
    float base_x, base_y, coll_x, coll_y, emit_x, emit_y;
    component_get_terminal_pos(npn, 0, &base_x, &base_y);
    component_get_terminal_pos(npn, 1, &coll_x, &coll_y);
    component_get_terminal_pos(npn, 2, &emit_x, &emit_y);
    float rc_top_x, rc_top_y, rc_bot_x, rc_bot_y;
    component_get_terminal_pos(rc, 0, &rc_top_x, &rc_top_y);
    component_get_terminal_pos(rc, 1, &rc_bot_x, &rc_bot_y);
    float re_top_x, re_top_y;
    component_get_terminal_pos(re, 0, &re_top_x, &re_top_y);
    float cout_in_x, cout_in_y;
    component_get_terminal_pos(cout, 0, &cout_in_x, &cout_in_y);

    // === VCC NODE ===
    // Create Vcc node and connect Vcc+, R1 top, and Rc top
    int vcc_node = circuit_find_or_create_node(circuit, vcc_x, vcc_y, 5.0f);
    vcc->node_ids[0] = vcc_node;

    // Wire from Vcc+ going right to R1 top (horizontal at y=-120)
    circuit_add_wire(circuit, vcc_node, circuit_find_or_create_node(circuit, r1_top_x, vcc_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r1_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, r1_top_x, r1_top_y, 5.0f));
    r1->node_ids[0] = vcc_node;

    // Wire from Vcc line continuing right to Rc top (separate horizontal path)
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r1_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc_top_x, vcc_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rc_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc_top_x, rc_top_y, 5.0f));
    rc->node_ids[0] = vcc_node;

    // === BASE BIAS NODE ===
    // Junction where R1 bottom, R2 top, Cin output, and BJT base connect
    int base_node = circuit_find_or_create_node(circuit, r1_bot_x, r1_bot_y, 5.0f);
    r1->node_ids[1] = base_node;

    // R2 top connects to same node (components are vertically aligned)
    r2->node_ids[0] = base_node;
    circuit_add_wire(circuit, base_node, circuit_find_or_create_node(circuit, r2_top_x, r2_top_y, 5.0f));

    // Cin output to base node (horizontal wire)
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cin_out_x, cin_out_y, 5.0f), base_node);
    cin->node_ids[1] = base_node;

    // Base node to BJT base (horizontal wire)
    circuit_add_wire(circuit, base_node, circuit_find_or_create_node(circuit, base_x, base_y, 5.0f));
    npn->node_ids[0] = base_node;

    // === COLLECTOR NODE ===
    // Rc bottom and BJT collector connect
    int coll_node = circuit_find_or_create_node(circuit, coll_x, coll_y, 5.0f);
    npn->node_ids[1] = coll_node;

    // Rc bottom to collector (short vertical wire)
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rc_bot_x, rc_bot_y, 5.0f), coll_node);
    rc->node_ids[1] = coll_node;

    // Collector to Cout (horizontal wire offset above collector)
    circuit_add_wire(circuit, coll_node, circuit_find_or_create_node(circuit, cout_in_x, coll_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cout_in_x, coll_y, 5.0f),
                     circuit_find_or_create_node(circuit, cout_in_x, cout_in_y, 5.0f));
    cout->node_ids[0] = coll_node;

    // === EMITTER NODE ===
    // BJT emitter to Re top
    int emit_node = circuit_find_or_create_node(circuit, emit_x, emit_y, 5.0f);
    npn->node_ids[2] = emit_node;
    circuit_add_wire(circuit, emit_node, circuit_find_or_create_node(circuit, re_top_x, re_top_y, 5.0f));
    re->node_ids[0] = emit_node;

    return 14;
}

// Common Source Amplifier (NMOS):
// Layout with non-overlapping wire routing
//
//    Vdd+ --------- Rd --------+
//                              |
//                          Drain
//                              |
//    Vin--Cin--+--- Gate --NMOS--+-- Cout
//              |               |
//             Rg           Source
//              |               |
//             GND      Rs     GND
//                       |
//                      GND
//
static int place_common_source(Circuit *circuit, float x, float y) {
    // === POWER SUPPLY ===
    Component *vdd = add_comp(circuit, COMP_DC_VOLTAGE, x - 100, y - 80, 0);
    if (!vdd) return 0;
    vdd->props.dc_voltage.voltage = 12.0;
    Component *gnd_vdd = add_comp(circuit, COMP_GROUND, x - 100, y, 0);

    // === INPUT SOURCE ===
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x - 100, y + 80, 0);
    vin->props.ac_voltage.amplitude = 0.1;
    vin->props.ac_voltage.frequency = 1000.0;
    vin->props.ac_voltage.offset = 2.0;
    Component *gnd_in = add_comp(circuit, COMP_GROUND, x - 100, y + 160, 0);

    // === INPUT COUPLING ===
    Component *cin = add_comp(circuit, COMP_CAPACITOR, x, y + 40, 0);
    cin->props.capacitor.capacitance = 10e-6;

    // === GATE BIAS RESISTOR ===
    Component *rg = add_comp(circuit, COMP_RESISTOR, x + 80, y + 80, 90);
    rg->props.resistor.resistance = 1000000.0;  // 1M
    Component *gnd_rg = add_comp(circuit, COMP_GROUND, x + 80, y + 140, 0);

    // === TRANSISTOR ===
    // NMOS terminals: G at (-20,0), D at (20,-20), S at (20,20)
    Component *nmos = add_comp(circuit, COMP_NMOS, x + 160, y + 40, 0);
    nmos->props.mosfet.vth = 1.5;
    nmos->props.mosfet.kp = 0.01;

    // === DRAIN RESISTOR ===
    Component *rd = add_comp(circuit, COMP_RESISTOR, x + 180, y - 40, 90);
    rd->props.resistor.resistance = 2200.0;

    // === SOURCE RESISTOR ===
    Component *rs = add_comp(circuit, COMP_RESISTOR, x + 180, y + 100, 90);
    rs->props.resistor.resistance = 470.0;
    Component *gnd_rs = add_comp(circuit, COMP_GROUND, x + 180, y + 160, 0);

    // === OUTPUT COUPLING ===
    Component *cout = add_comp(circuit, COMP_CAPACITOR, x + 260, y + 20, 0);
    cout->props.capacitor.capacitance = 10e-6;

    // Label
    Component *label = add_comp(circuit, COMP_TEXT, x + 60, y - 100, 0);
    strncpy(label->props.text.text, "Common Source Amp", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // === CONNECTIONS ===
    connect_terminals(circuit, vdd, 1, gnd_vdd, 0);
    connect_terminals(circuit, vin, 1, gnd_in, 0);
    connect_terminals(circuit, rg, 1, gnd_rg, 0);
    connect_terminals(circuit, rs, 1, gnd_rs, 0);
    connect_terminals(circuit, vin, 0, cin, 0);

    // Get terminal positions
    float vdd_x, vdd_y;
    component_get_terminal_pos(vdd, 0, &vdd_x, &vdd_y);
    float cin_out_x, cin_out_y;
    component_get_terminal_pos(cin, 1, &cin_out_x, &cin_out_y);
    float rg_top_x, rg_top_y;
    component_get_terminal_pos(rg, 0, &rg_top_x, &rg_top_y);
    float gate_x, gate_y, drain_x, drain_y, source_x, source_y;
    component_get_terminal_pos(nmos, 0, &gate_x, &gate_y);
    component_get_terminal_pos(nmos, 1, &drain_x, &drain_y);
    component_get_terminal_pos(nmos, 2, &source_x, &source_y);
    float rd_top_x, rd_top_y, rd_bot_x, rd_bot_y;
    component_get_terminal_pos(rd, 0, &rd_top_x, &rd_top_y);
    component_get_terminal_pos(rd, 1, &rd_bot_x, &rd_bot_y);
    float rs_top_x, rs_top_y;
    component_get_terminal_pos(rs, 0, &rs_top_x, &rs_top_y);
    float cout_in_x, cout_in_y;
    component_get_terminal_pos(cout, 0, &cout_in_x, &cout_in_y);

    // === VDD NODE ===
    int vdd_node = circuit_find_or_create_node(circuit, vdd_x, vdd_y, 5.0f);
    vdd->node_ids[0] = vdd_node;

    // Wire from Vdd+ going right to Rd top (horizontal bus at Vdd level)
    circuit_add_wire(circuit, vdd_node, circuit_find_or_create_node(circuit, rd_top_x, vdd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rd_top_x, vdd_y, 5.0f),
                     circuit_find_or_create_node(circuit, rd_top_x, rd_top_y, 5.0f));
    rd->node_ids[0] = vdd_node;

    // === GATE NODE ===
    // Junction where Cin out, Rg top, and NMOS gate connect
    int gate_node = circuit_find_or_create_node(circuit, rg_top_x, rg_top_y, 5.0f);
    rg->node_ids[0] = gate_node;

    // Cin output to gate node (horizontal)
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cin_out_x, cin_out_y, 5.0f),
                     circuit_find_or_create_node(circuit, rg_top_x, cin_out_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rg_top_x, cin_out_y, 5.0f), gate_node);
    cin->node_ids[1] = gate_node;

    // Gate node to NMOS gate (horizontal)
    circuit_add_wire(circuit, gate_node, circuit_find_or_create_node(circuit, gate_x, gate_y, 5.0f));
    nmos->node_ids[0] = gate_node;

    // === DRAIN NODE ===
    int drain_node = circuit_find_or_create_node(circuit, drain_x, drain_y, 5.0f);
    nmos->node_ids[1] = drain_node;

    // Rd bottom to drain
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rd_bot_x, rd_bot_y, 5.0f), drain_node);
    rd->node_ids[1] = drain_node;

    // Drain to Cout (horizontal then vertical)
    circuit_add_wire(circuit, drain_node, circuit_find_or_create_node(circuit, cout_in_x, drain_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cout_in_x, drain_y, 5.0f),
                     circuit_find_or_create_node(circuit, cout_in_x, cout_in_y, 5.0f));
    cout->node_ids[0] = drain_node;

    // === SOURCE NODE ===
    int source_node = circuit_find_or_create_node(circuit, source_x, source_y, 5.0f);
    nmos->node_ids[2] = source_node;
    circuit_add_wire(circuit, source_node, circuit_find_or_create_node(circuit, rs_top_x, rs_top_y, 5.0f));
    rs->node_ids[0] = source_node;

    return 13;
}

// Common Drain (Source Follower):
// Layout with non-overlapping wire routing
//
//    Vdd+ ---------- Drain
//                      |
//    Vin--Cin-- Gate --NMOS
//                      |
//                   Source --+-- Cout
//                      |
//                     Rs
//                      |
//                     GND
//
static int place_common_drain(Circuit *circuit, float x, float y) {
    // === POWER SUPPLY ===
    Component *vdd = add_comp(circuit, COMP_DC_VOLTAGE, x - 100, y - 80, 0);
    if (!vdd) return 0;
    vdd->props.dc_voltage.voltage = 12.0;
    Component *gnd_vdd = add_comp(circuit, COMP_GROUND, x - 100, y, 0);

    // === INPUT SOURCE ===
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x - 100, y + 80, 0);
    vin->props.ac_voltage.amplitude = 1.0;
    vin->props.ac_voltage.frequency = 1000.0;
    vin->props.ac_voltage.offset = 6.0;
    Component *gnd_in = add_comp(circuit, COMP_GROUND, x - 100, y + 160, 0);

    // === INPUT COUPLING ===
    Component *cin = add_comp(circuit, COMP_CAPACITOR, x, y + 40, 0);
    cin->props.capacitor.capacitance = 10e-6;

    // === TRANSISTOR ===
    // NMOS terminals: G at (-20,0), D at (20,-20), S at (20,20)
    Component *nmos = add_comp(circuit, COMP_NMOS, x + 140, y + 40, 0);
    nmos->props.mosfet.vth = 1.5;
    nmos->props.mosfet.kp = 0.02;

    // === SOURCE RESISTOR ===
    Component *rs = add_comp(circuit, COMP_RESISTOR, x + 160, y + 100, 90);
    rs->props.resistor.resistance = 1000.0;
    Component *gnd_rs = add_comp(circuit, COMP_GROUND, x + 160, y + 160, 0);

    // === OUTPUT COUPLING ===
    Component *cout = add_comp(circuit, COMP_CAPACITOR, x + 240, y + 60, 0);
    cout->props.capacitor.capacitance = 10e-6;

    // Label
    Component *label = add_comp(circuit, COMP_TEXT, x + 20, y - 100, 0);
    strncpy(label->props.text.text, "Source Follower (Gain~1)", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // === CONNECTIONS ===
    connect_terminals(circuit, vdd, 1, gnd_vdd, 0);
    connect_terminals(circuit, vin, 1, gnd_in, 0);
    connect_terminals(circuit, rs, 1, gnd_rs, 0);
    connect_terminals(circuit, vin, 0, cin, 0);

    // Get terminal positions
    float vdd_x, vdd_y;
    component_get_terminal_pos(vdd, 0, &vdd_x, &vdd_y);
    float cin_out_x, cin_out_y;
    component_get_terminal_pos(cin, 1, &cin_out_x, &cin_out_y);
    float gate_x, gate_y, drain_x, drain_y, source_x, source_y;
    component_get_terminal_pos(nmos, 0, &gate_x, &gate_y);
    component_get_terminal_pos(nmos, 1, &drain_x, &drain_y);
    component_get_terminal_pos(nmos, 2, &source_x, &source_y);
    float rs_top_x, rs_top_y;
    component_get_terminal_pos(rs, 0, &rs_top_x, &rs_top_y);
    float cout_in_x, cout_in_y;
    component_get_terminal_pos(cout, 0, &cout_in_x, &cout_in_y);

    // === VDD NODE (Drain) ===
    int vdd_node = circuit_find_or_create_node(circuit, vdd_x, vdd_y, 5.0f);
    vdd->node_ids[0] = vdd_node;

    // Wire from Vdd+ to drain (horizontal then vertical)
    circuit_add_wire(circuit, vdd_node, circuit_find_or_create_node(circuit, drain_x, vdd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, drain_x, vdd_y, 5.0f),
                     circuit_find_or_create_node(circuit, drain_x, drain_y, 5.0f));
    nmos->node_ids[1] = vdd_node;

    // === GATE NODE ===
    int gate_node = circuit_find_or_create_node(circuit, gate_x, gate_y, 5.0f);
    nmos->node_ids[0] = gate_node;

    // Cin output to gate (horizontal)
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cin_out_x, cin_out_y, 5.0f),
                     circuit_find_or_create_node(circuit, gate_x, cin_out_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, gate_x, cin_out_y, 5.0f), gate_node);
    cin->node_ids[1] = gate_node;

    // === SOURCE NODE ===
    int source_node = circuit_find_or_create_node(circuit, source_x, source_y, 5.0f);
    nmos->node_ids[2] = source_node;

    // Source to Rs (vertical)
    circuit_add_wire(circuit, source_node, circuit_find_or_create_node(circuit, rs_top_x, rs_top_y, 5.0f));
    rs->node_ids[0] = source_node;

    // Source to Cout (separate path: horizontal then vertical)
    circuit_add_wire(circuit, source_node, circuit_find_or_create_node(circuit, cout_in_x, source_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cout_in_x, source_y, 5.0f),
                     circuit_find_or_create_node(circuit, cout_in_x, cout_in_y, 5.0f));
    cout->node_ids[0] = source_node;

    return 10;
}

// Two-Stage BJT Amplifier (CE-CE cascade):
// Layout with non-overlapping wire routing
// Vcc bus runs horizontally at top connecting all upper resistors
//
//    Vcc+--+------+------+------+
//         |      |      |      |
//        R1a    Rc1    R1b    Rc2
//         |      |      |      |
//    C1---+--Q1--+--C2--+--Q2--+--C3
//         |      |      |      |
//        R2a    Re1    R2b    Re2
//         |      |      |      |
//        GND    GND    GND    GND
//
static int place_multistage_amp(Circuit *circuit, float x, float y) {
    // === POWER SUPPLY ===
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x - 80, y - 100, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 12.0;
    Component *gnd_vcc = add_comp(circuit, COMP_GROUND, x - 80, y - 20, 0);

    // === INPUT SOURCE ===
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x - 80, y + 60, 0);
    vin->props.ac_voltage.amplitude = 0.01;
    vin->props.ac_voltage.frequency = 1000.0;
    Component *gnd_in = add_comp(circuit, COMP_GROUND, x - 80, y + 140, 0);

    // === INPUT COUPLING ===
    Component *c1 = add_comp(circuit, COMP_CAPACITOR, x, y + 20, 0);
    c1->props.capacitor.capacitance = 10e-6;

    // === STAGE 1 BIAS ===
    Component *r1a = add_comp(circuit, COMP_RESISTOR, x + 60, y - 40, 90);
    r1a->props.resistor.resistance = 47000.0;
    Component *r2a = add_comp(circuit, COMP_RESISTOR, x + 60, y + 60, 90);
    r2a->props.resistor.resistance = 10000.0;
    Component *gnd_r2a = add_comp(circuit, COMP_GROUND, x + 60, y + 120, 0);

    // === STAGE 1 TRANSISTOR ===
    Component *q1 = add_comp(circuit, COMP_NPN_BJT, x + 120, y + 20, 0);
    q1->props.bjt.bf = 100;

    // === STAGE 1 RESISTORS ===
    Component *rc1 = add_comp(circuit, COMP_RESISTOR, x + 140, y - 40, 90);
    rc1->props.resistor.resistance = 4700.0;
    Component *re1 = add_comp(circuit, COMP_RESISTOR, x + 140, y + 80, 90);
    re1->props.resistor.resistance = 1000.0;
    Component *gnd_re1 = add_comp(circuit, COMP_GROUND, x + 140, y + 140, 0);

    // === INTERSTAGE COUPLING ===
    Component *c2 = add_comp(circuit, COMP_CAPACITOR, x + 200, y, 0);
    c2->props.capacitor.capacitance = 10e-6;

    // === STAGE 2 BIAS ===
    Component *r1b = add_comp(circuit, COMP_RESISTOR, x + 260, y - 40, 90);
    r1b->props.resistor.resistance = 47000.0;
    Component *r2b = add_comp(circuit, COMP_RESISTOR, x + 260, y + 60, 90);
    r2b->props.resistor.resistance = 10000.0;
    Component *gnd_r2b = add_comp(circuit, COMP_GROUND, x + 260, y + 120, 0);

    // === STAGE 2 TRANSISTOR ===
    Component *q2 = add_comp(circuit, COMP_NPN_BJT, x + 320, y + 20, 0);
    q2->props.bjt.bf = 100;

    // === STAGE 2 RESISTORS ===
    Component *rc2 = add_comp(circuit, COMP_RESISTOR, x + 340, y - 40, 90);
    rc2->props.resistor.resistance = 4700.0;
    Component *re2 = add_comp(circuit, COMP_RESISTOR, x + 340, y + 80, 90);
    re2->props.resistor.resistance = 1000.0;
    Component *gnd_re2 = add_comp(circuit, COMP_GROUND, x + 340, y + 140, 0);

    // === OUTPUT COUPLING ===
    Component *c3 = add_comp(circuit, COMP_CAPACITOR, x + 400, y, 0);
    c3->props.capacitor.capacitance = 10e-6;

    // Label
    Component *label = add_comp(circuit, COMP_TEXT, x + 100, y - 120, 0);
    strncpy(label->props.text.text, "Two-Stage CE Amplifier", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // === GROUND CONNECTIONS ===
    connect_terminals(circuit, vcc, 1, gnd_vcc, 0);
    connect_terminals(circuit, vin, 1, gnd_in, 0);
    connect_terminals(circuit, r2a, 1, gnd_r2a, 0);
    connect_terminals(circuit, re1, 1, gnd_re1, 0);
    connect_terminals(circuit, r2b, 1, gnd_r2b, 0);
    connect_terminals(circuit, re2, 1, gnd_re2, 0);
    connect_terminals(circuit, vin, 0, c1, 0);

    // Get terminal positions
    float vcc_x, vcc_y;
    component_get_terminal_pos(vcc, 0, &vcc_x, &vcc_y);
    float r1a_top_x, r1a_top_y, r1a_bot_x, r1a_bot_y;
    component_get_terminal_pos(r1a, 0, &r1a_top_x, &r1a_top_y);
    component_get_terminal_pos(r1a, 1, &r1a_bot_x, &r1a_bot_y);
    float r2a_top_x, r2a_top_y;
    component_get_terminal_pos(r2a, 0, &r2a_top_x, &r2a_top_y);
    float rc1_top_x, rc1_top_y, rc1_bot_x, rc1_bot_y;
    component_get_terminal_pos(rc1, 0, &rc1_top_x, &rc1_top_y);
    component_get_terminal_pos(rc1, 1, &rc1_bot_x, &rc1_bot_y);
    float re1_top_x, re1_top_y;
    component_get_terminal_pos(re1, 0, &re1_top_x, &re1_top_y);
    float r1b_top_x, r1b_top_y, r1b_bot_x, r1b_bot_y;
    component_get_terminal_pos(r1b, 0, &r1b_top_x, &r1b_top_y);
    component_get_terminal_pos(r1b, 1, &r1b_bot_x, &r1b_bot_y);
    float r2b_top_x, r2b_top_y;
    component_get_terminal_pos(r2b, 0, &r2b_top_x, &r2b_top_y);
    float rc2_top_x, rc2_top_y, rc2_bot_x, rc2_bot_y;
    component_get_terminal_pos(rc2, 0, &rc2_top_x, &rc2_top_y);
    component_get_terminal_pos(rc2, 1, &rc2_bot_x, &rc2_bot_y);
    float re2_top_x, re2_top_y;
    component_get_terminal_pos(re2, 0, &re2_top_x, &re2_top_y);
    float c1_out_x, c1_out_y;
    component_get_terminal_pos(c1, 1, &c1_out_x, &c1_out_y);
    float c2_in_x, c2_in_y, c2_out_x, c2_out_y;
    component_get_terminal_pos(c2, 0, &c2_in_x, &c2_in_y);
    component_get_terminal_pos(c2, 1, &c2_out_x, &c2_out_y);
    float c3_in_x, c3_in_y;
    component_get_terminal_pos(c3, 0, &c3_in_x, &c3_in_y);
    float base1_x, base1_y, coll1_x, coll1_y, emit1_x, emit1_y;
    component_get_terminal_pos(q1, 0, &base1_x, &base1_y);
    component_get_terminal_pos(q1, 1, &coll1_x, &coll1_y);
    component_get_terminal_pos(q1, 2, &emit1_x, &emit1_y);
    float base2_x, base2_y, coll2_x, coll2_y, emit2_x, emit2_y;
    component_get_terminal_pos(q2, 0, &base2_x, &base2_y);
    component_get_terminal_pos(q2, 1, &coll2_x, &coll2_y);
    component_get_terminal_pos(q2, 2, &emit2_x, &emit2_y);

    // === VCC BUS (horizontal at vcc_y level) ===
    int vcc_node = circuit_find_or_create_node(circuit, vcc_x, vcc_y, 5.0f);
    vcc->node_ids[0] = vcc_node;

    // Vcc to R1a top: horizontal to R1a x, then down to R1a top
    circuit_add_wire(circuit, vcc_node, circuit_find_or_create_node(circuit, r1a_top_x, vcc_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r1a_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, r1a_top_x, r1a_top_y, 5.0f));
    r1a->node_ids[0] = vcc_node;

    // Continue bus to Rc1 top
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r1a_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc1_top_x, vcc_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rc1_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc1_top_x, rc1_top_y, 5.0f));
    rc1->node_ids[0] = vcc_node;

    // Continue bus to R1b top
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rc1_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, r1b_top_x, vcc_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r1b_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, r1b_top_x, r1b_top_y, 5.0f));
    r1b->node_ids[0] = vcc_node;

    // Continue bus to Rc2 top
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r1b_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc2_top_x, vcc_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rc2_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc2_top_x, rc2_top_y, 5.0f));
    rc2->node_ids[0] = vcc_node;

    // === STAGE 1 BASE NODE ===
    int base1_node = circuit_find_or_create_node(circuit, r1a_bot_x, r1a_bot_y, 5.0f);
    r1a->node_ids[1] = base1_node;
    r2a->node_ids[0] = base1_node;
    circuit_add_wire(circuit, base1_node, circuit_find_or_create_node(circuit, r2a_top_x, r2a_top_y, 5.0f));

    // C1 out to base node (horizontal)
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, c1_out_x, c1_out_y, 5.0f), base1_node);
    c1->node_ids[1] = base1_node;

    // Base node to Q1 base (horizontal)
    circuit_add_wire(circuit, base1_node, circuit_find_or_create_node(circuit, base1_x, base1_y, 5.0f));
    q1->node_ids[0] = base1_node;

    // === STAGE 1 COLLECTOR NODE ===
    int coll1_node = circuit_find_or_create_node(circuit, coll1_x, coll1_y, 5.0f);
    q1->node_ids[1] = coll1_node;

    // Rc1 bottom to collector
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rc1_bot_x, rc1_bot_y, 5.0f), coll1_node);
    rc1->node_ids[1] = coll1_node;

    // Collector to C2 input (horizontal then vertical)
    circuit_add_wire(circuit, coll1_node, circuit_find_or_create_node(circuit, c2_in_x, coll1_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, c2_in_x, coll1_y, 5.0f),
                     circuit_find_or_create_node(circuit, c2_in_x, c2_in_y, 5.0f));
    c2->node_ids[0] = coll1_node;

    // === STAGE 1 EMITTER NODE ===
    int emit1_node = circuit_find_or_create_node(circuit, emit1_x, emit1_y, 5.0f);
    q1->node_ids[2] = emit1_node;
    circuit_add_wire(circuit, emit1_node, circuit_find_or_create_node(circuit, re1_top_x, re1_top_y, 5.0f));
    re1->node_ids[0] = emit1_node;

    // === STAGE 2 BASE NODE ===
    int base2_node = circuit_find_or_create_node(circuit, r1b_bot_x, r1b_bot_y, 5.0f);
    r1b->node_ids[1] = base2_node;
    r2b->node_ids[0] = base2_node;
    circuit_add_wire(circuit, base2_node, circuit_find_or_create_node(circuit, r2b_top_x, r2b_top_y, 5.0f));

    // C2 out to base node (horizontal)
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, c2_out_x, c2_out_y, 5.0f), base2_node);
    c2->node_ids[1] = base2_node;

    // Base node to Q2 base (horizontal)
    circuit_add_wire(circuit, base2_node, circuit_find_or_create_node(circuit, base2_x, base2_y, 5.0f));
    q2->node_ids[0] = base2_node;

    // === STAGE 2 COLLECTOR NODE ===
    int coll2_node = circuit_find_or_create_node(circuit, coll2_x, coll2_y, 5.0f);
    q2->node_ids[1] = coll2_node;

    // Rc2 bottom to collector
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rc2_bot_x, rc2_bot_y, 5.0f), coll2_node);
    rc2->node_ids[1] = coll2_node;

    // Collector to C3 input (horizontal then vertical)
    circuit_add_wire(circuit, coll2_node, circuit_find_or_create_node(circuit, c3_in_x, coll2_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, c3_in_x, coll2_y, 5.0f),
                     circuit_find_or_create_node(circuit, c3_in_x, c3_in_y, 5.0f));
    c3->node_ids[0] = coll2_node;

    // === STAGE 2 EMITTER NODE ===
    int emit2_node = circuit_find_or_create_node(circuit, emit2_x, emit2_y, 5.0f);
    q2->node_ids[2] = emit2_node;
    circuit_add_wire(circuit, emit2_node, circuit_find_or_create_node(circuit, re2_top_x, re2_top_y, 5.0f));
    re2->node_ids[0] = emit2_node;

    return 23;
}

// BJT Differential Pair:
// Classic differential amplifier with tail current source
//
//        Vcc+
//         |
//    +----+----+
//    |         |
//   Rc1       Rc2
//    |         |
//    +--Vout1  +--Vout2
//    |         |
//   C1        C2
//    |    |    |
//  Q1-B   |   B-Q2
//    E    |    E
//     \   |   /
//      \  |  /
//       \ | /
//        \|/
//         Re (tail current)
//         |
//        GND
//
static int place_differential_pair(Circuit *circuit, float x, float y) {
    // === POWER SUPPLY ===
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x - 80, y - 100, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 12.0;
    Component *gnd_vcc = add_comp(circuit, COMP_GROUND, x - 80, y - 20, 0);

    // === INPUT SOURCES ===
    Component *vin1 = add_comp(circuit, COMP_AC_VOLTAGE, x - 80, y + 60, 0);
    vin1->props.ac_voltage.amplitude = 0.05;
    vin1->props.ac_voltage.frequency = 1000.0;
    Component *gnd_in1 = add_comp(circuit, COMP_GROUND, x - 80, y + 140, 0);

    Component *vin2 = add_comp(circuit, COMP_AC_VOLTAGE, x + 200, y + 60, 0);
    vin2->props.ac_voltage.amplitude = 0.05;
    vin2->props.ac_voltage.frequency = 1000.0;
    vin2->props.ac_voltage.phase = 180.0;  // 180 degrees out of phase
    Component *gnd_in2 = add_comp(circuit, COMP_GROUND, x + 200, y + 140, 0);

    // === INPUT COUPLING CAPACITORS ===
    Component *cin1 = add_comp(circuit, COMP_CAPACITOR, x, y + 20, 0);
    cin1->props.capacitor.capacitance = 10e-6;
    Component *cin2 = add_comp(circuit, COMP_CAPACITOR, x + 120, y + 20, 0);
    cin2->props.capacitor.capacitance = 10e-6;

    // === TRANSISTORS ===
    // BJT: B at (-20,0), C at (20,-20), E at (20,20)
    Component *q1 = add_comp(circuit, COMP_NPN_BJT, x + 60, y + 20, 0);
    q1->props.bjt.bf = 100;
    Component *q2 = add_comp(circuit, COMP_NPN_BJT, x + 100, y + 20, 180);  // Mirrored
    q2->props.bjt.bf = 100;

    // === COLLECTOR RESISTORS ===
    Component *rc1 = add_comp(circuit, COMP_RESISTOR, x + 80, y - 60, 90);
    rc1->props.resistor.resistance = 4700.0;
    Component *rc2 = add_comp(circuit, COMP_RESISTOR, x + 120, y - 60, 90);
    rc2->props.resistor.resistance = 4700.0;

    // === TAIL RESISTOR ===
    Component *re = add_comp(circuit, COMP_RESISTOR, x + 100, y + 80, 90);
    re->props.resistor.resistance = 10000.0;
    Component *gnd_re = add_comp(circuit, COMP_GROUND, x + 100, y + 140, 0);

    // Label
    Component *label = add_comp(circuit, COMP_TEXT, x + 40, y - 120, 0);
    strncpy(label->props.text.text, "Differential Pair", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // === CONNECTIONS ===
    connect_terminals(circuit, vcc, 1, gnd_vcc, 0);
    connect_terminals(circuit, vin1, 1, gnd_in1, 0);
    connect_terminals(circuit, vin2, 1, gnd_in2, 0);
    connect_terminals(circuit, re, 1, gnd_re, 0);
    connect_terminals(circuit, vin1, 0, cin1, 0);
    connect_terminals(circuit, vin2, 0, cin2, 0);

    // Get terminal positions
    float vcc_x, vcc_y;
    component_get_terminal_pos(vcc, 0, &vcc_x, &vcc_y);
    float cin1_out_x, cin1_out_y;
    component_get_terminal_pos(cin1, 1, &cin1_out_x, &cin1_out_y);
    float cin2_out_x, cin2_out_y;
    component_get_terminal_pos(cin2, 1, &cin2_out_x, &cin2_out_y);
    float rc1_top_x, rc1_top_y, rc1_bot_x, rc1_bot_y;
    component_get_terminal_pos(rc1, 0, &rc1_top_x, &rc1_top_y);
    component_get_terminal_pos(rc1, 1, &rc1_bot_x, &rc1_bot_y);
    float rc2_top_x, rc2_top_y, rc2_bot_x, rc2_bot_y;
    component_get_terminal_pos(rc2, 0, &rc2_top_x, &rc2_top_y);
    component_get_terminal_pos(rc2, 1, &rc2_bot_x, &rc2_bot_y);
    float re_top_x, re_top_y;
    component_get_terminal_pos(re, 0, &re_top_x, &re_top_y);
    float base1_x, base1_y, coll1_x, coll1_y, emit1_x, emit1_y;
    component_get_terminal_pos(q1, 0, &base1_x, &base1_y);
    component_get_terminal_pos(q1, 1, &coll1_x, &coll1_y);
    component_get_terminal_pos(q1, 2, &emit1_x, &emit1_y);
    float base2_x, base2_y, coll2_x, coll2_y, emit2_x, emit2_y;
    component_get_terminal_pos(q2, 0, &base2_x, &base2_y);
    component_get_terminal_pos(q2, 1, &coll2_x, &coll2_y);
    component_get_terminal_pos(q2, 2, &emit2_x, &emit2_y);

    // === VCC BUS ===
    int vcc_node = circuit_find_or_create_node(circuit, vcc_x, vcc_y, 5.0f);
    vcc->node_ids[0] = vcc_node;
    circuit_add_wire(circuit, vcc_node, circuit_find_or_create_node(circuit, rc1_top_x, vcc_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rc1_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc1_top_x, rc1_top_y, 5.0f));
    rc1->node_ids[0] = vcc_node;
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rc1_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc2_top_x, vcc_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rc2_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc2_top_x, rc2_top_y, 5.0f));
    rc2->node_ids[0] = vcc_node;

    // === BASE CONNECTIONS ===
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cin1_out_x, cin1_out_y, 5.0f),
                     circuit_find_or_create_node(circuit, base1_x, base1_y, 5.0f));
    cin1->node_ids[1] = circuit_find_or_create_node(circuit, base1_x, base1_y, 5.0f);
    q1->node_ids[0] = cin1->node_ids[1];

    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cin2_out_x, cin2_out_y, 5.0f),
                     circuit_find_or_create_node(circuit, base2_x, base2_y, 5.0f));
    cin2->node_ids[1] = circuit_find_or_create_node(circuit, base2_x, base2_y, 5.0f);
    q2->node_ids[0] = cin2->node_ids[1];

    // === COLLECTOR CONNECTIONS ===
    int coll1_node = circuit_find_or_create_node(circuit, coll1_x, coll1_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rc1_bot_x, rc1_bot_y, 5.0f), coll1_node);
    rc1->node_ids[1] = coll1_node;
    q1->node_ids[1] = coll1_node;

    int coll2_node = circuit_find_or_create_node(circuit, coll2_x, coll2_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rc2_bot_x, rc2_bot_y, 5.0f), coll2_node);
    rc2->node_ids[1] = coll2_node;
    q2->node_ids[1] = coll2_node;

    // === EMITTER TAIL ===
    int tail_node = circuit_find_or_create_node(circuit, re_top_x, re_top_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, emit1_x, emit1_y, 5.0f),
                     circuit_find_or_create_node(circuit, emit1_x, re_top_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, emit1_x, re_top_y, 5.0f), tail_node);
    q1->node_ids[2] = tail_node;
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, emit2_x, emit2_y, 5.0f),
                     circuit_find_or_create_node(circuit, emit2_x, re_top_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, emit2_x, re_top_y, 5.0f), tail_node);
    q2->node_ids[2] = tail_node;
    re->node_ids[0] = tail_node;

    return 15;
}

// BJT Current Mirror:
// Reference current sets output current
//
//    Vcc+
//     |
//    Rref
//     |
//  +--+--+
//  |     |
// Q1    Q2 (output)
//  |     |
// GND   Load
//
static int place_current_mirror(Circuit *circuit, float x, float y) {
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x, y - 80, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 12.0;
    Component *gnd_vcc = add_comp(circuit, COMP_GROUND, x, y, 0);

    // Reference resistor
    Component *rref = add_comp(circuit, COMP_RESISTOR, x + 60, y - 60, 90);
    rref->props.resistor.resistance = 10000.0;  // Sets ~1mA reference current

    // Reference transistor Q1 (diode-connected)
    Component *q1 = add_comp(circuit, COMP_NPN_BJT, x + 80, y, 0);
    q1->props.bjt.bf = 100;
    Component *gnd_q1 = add_comp(circuit, COMP_GROUND, x + 100, y + 40, 0);

    // Output transistor Q2
    Component *q2 = add_comp(circuit, COMP_NPN_BJT, x + 160, y, 0);
    q2->props.bjt.bf = 100;

    // Load resistor (to see the mirrored current)
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 180, y - 60, 90);
    rload->props.resistor.resistance = 1000.0;

    Component *gnd_q2 = add_comp(circuit, COMP_GROUND, x + 180, y + 40, 0);

    // Label
    Component *label = add_comp(circuit, COMP_TEXT, x + 60, y - 120, 0);
    strncpy(label->props.text.text, "Current Mirror", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // === CONNECTIONS ===
    connect_terminals(circuit, vcc, 1, gnd_vcc, 0);
    connect_terminals(circuit, q2, 2, gnd_q2, 0);

    float vcc_x, vcc_y;
    component_get_terminal_pos(vcc, 0, &vcc_x, &vcc_y);
    float rref_top_x, rref_top_y, rref_bot_x, rref_bot_y;
    component_get_terminal_pos(rref, 0, &rref_top_x, &rref_top_y);
    component_get_terminal_pos(rref, 1, &rref_bot_x, &rref_bot_y);
    float rload_top_x, rload_top_y, rload_bot_x, rload_bot_y;
    component_get_terminal_pos(rload, 0, &rload_top_x, &rload_top_y);
    component_get_terminal_pos(rload, 1, &rload_bot_x, &rload_bot_y);
    float base1_x, base1_y, coll1_x, coll1_y, emit1_x, emit1_y;
    component_get_terminal_pos(q1, 0, &base1_x, &base1_y);
    component_get_terminal_pos(q1, 1, &coll1_x, &coll1_y);
    component_get_terminal_pos(q1, 2, &emit1_x, &emit1_y);
    float base2_x, base2_y, coll2_x, coll2_y;
    component_get_terminal_pos(q2, 0, &base2_x, &base2_y);
    component_get_terminal_pos(q2, 1, &coll2_x, &coll2_y);
    float gnd_q1_x, gnd_q1_y;
    component_get_terminal_pos(gnd_q1, 0, &gnd_q1_x, &gnd_q1_y);

    // Vcc to Rref and Rload
    int vcc_node = circuit_find_or_create_node(circuit, vcc_x, vcc_y, 5.0f);
    vcc->node_ids[0] = vcc_node;
    circuit_add_wire(circuit, vcc_node, circuit_find_or_create_node(circuit, rref_top_x, vcc_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rref_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rref_top_x, rref_top_y, 5.0f));
    rref->node_ids[0] = vcc_node;
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rref_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rload_top_x, vcc_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rload_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f));
    rload->node_ids[0] = vcc_node;

    // Diode-connect Q1 (base to collector)
    int base_node = circuit_find_or_create_node(circuit, base1_x, base1_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rref_bot_x, rref_bot_y, 5.0f),
                     circuit_find_or_create_node(circuit, coll1_x, coll1_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, coll1_x, coll1_y, 5.0f), base_node);
    rref->node_ids[1] = base_node;
    q1->node_ids[0] = base_node;
    q1->node_ids[1] = base_node;

    // Connect Q2 base to Q1 base
    circuit_add_wire(circuit, base_node, circuit_find_or_create_node(circuit, base2_x, base2_y, 5.0f));
    q2->node_ids[0] = base_node;

    // Q1 emitter to ground
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, emit1_x, emit1_y, 5.0f),
                     circuit_find_or_create_node(circuit, gnd_q1_x, gnd_q1_y, 5.0f));
    q1->node_ids[2] = circuit_find_or_create_node(circuit, gnd_q1_x, gnd_q1_y, 5.0f);

    // Rload to Q2 collector
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rload_bot_x, rload_bot_y, 5.0f),
                     circuit_find_or_create_node(circuit, coll2_x, coll2_y, 5.0f));
    rload->node_ids[1] = circuit_find_or_create_node(circuit, coll2_x, coll2_y, 5.0f);
    q2->node_ids[1] = rload->node_ids[1];

    return 9;
}

// Push-Pull Output Stage:
// Complementary NPN/PNP for high current drive
//
//      Vcc+
//       |
//      Q1 (NPN)
//    B--|  E
//       |--|--Output
//    B--|  E
//      Q2 (PNP)
//       |
//      Vcc-
//
static int place_push_pull(Circuit *circuit, float x, float y) {
    // Positive supply
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x, y - 80, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 12.0;
    Component *gnd_vcc = add_comp(circuit, COMP_GROUND, x, y, 0);

    // Negative supply
    Component *vee = add_comp(circuit, COMP_DC_VOLTAGE, x, y + 120, 0);
    vee->props.dc_voltage.voltage = 12.0;
    Component *gnd_vee = add_comp(circuit, COMP_GROUND, x, y + 200, 0);

    // Input signal
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x - 80, y + 60, 0);
    vin->props.ac_voltage.amplitude = 5.0;
    vin->props.ac_voltage.frequency = 1000.0;
    Component *gnd_in = add_comp(circuit, COMP_GROUND, x - 80, y + 140, 0);

    // NPN transistor (top)
    Component *q1 = add_comp(circuit, COMP_NPN_BJT, x + 80, y - 20, 0);
    q1->props.bjt.bf = 100;

    // PNP transistor (bottom)
    Component *q2 = add_comp(circuit, COMP_PNP_BJT, x + 80, y + 60, 0);
    q2->props.bjt.bf = 100;

    // Load resistor
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 160, y + 20, 90);
    rload->props.resistor.resistance = 100.0;
    Component *gnd_load = add_comp(circuit, COMP_GROUND, x + 160, y + 80, 0);

    // Label
    Component *label = add_comp(circuit, COMP_TEXT, x + 40, y - 120, 0);
    strncpy(label->props.text.text, "Push-Pull Output", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // === CONNECTIONS ===
    connect_terminals(circuit, vcc, 1, gnd_vcc, 0);
    connect_terminals(circuit, vee, 1, gnd_vee, 0);
    connect_terminals(circuit, vin, 1, gnd_in, 0);
    connect_terminals(circuit, rload, 1, gnd_load, 0);

    float vcc_x, vcc_y;
    component_get_terminal_pos(vcc, 0, &vcc_x, &vcc_y);
    float vee_x, vee_y;
    component_get_terminal_pos(vee, 0, &vee_x, &vee_y);
    float vin_x, vin_y;
    component_get_terminal_pos(vin, 0, &vin_x, &vin_y);
    float base1_x, base1_y, coll1_x, coll1_y, emit1_x, emit1_y;
    component_get_terminal_pos(q1, 0, &base1_x, &base1_y);
    component_get_terminal_pos(q1, 1, &coll1_x, &coll1_y);
    component_get_terminal_pos(q1, 2, &emit1_x, &emit1_y);
    float base2_x, base2_y, coll2_x, coll2_y, emit2_x, emit2_y;
    component_get_terminal_pos(q2, 0, &base2_x, &base2_y);
    component_get_terminal_pos(q2, 1, &coll2_x, &coll2_y);
    component_get_terminal_pos(q2, 2, &emit2_x, &emit2_y);
    float rload_top_x, rload_top_y;
    component_get_terminal_pos(rload, 0, &rload_top_x, &rload_top_y);

    // Vcc to Q1 collector
    int vcc_node = circuit_find_or_create_node(circuit, vcc_x, vcc_y, 5.0f);
    circuit_add_wire(circuit, vcc_node, circuit_find_or_create_node(circuit, coll1_x, vcc_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, coll1_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, coll1_x, coll1_y, 5.0f));
    vcc->node_ids[0] = vcc_node;
    q1->node_ids[1] = vcc_node;

    // Vee to Q2 collector
    int vee_node = circuit_find_or_create_node(circuit, vee_x, vee_y, 5.0f);
    circuit_add_wire(circuit, vee_node, circuit_find_or_create_node(circuit, coll2_x, vee_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, coll2_x, vee_y, 5.0f),
                     circuit_find_or_create_node(circuit, coll2_x, coll2_y, 5.0f));
    vee->node_ids[0] = vee_node;
    q2->node_ids[1] = vee_node;

    // Input to both bases
    int base_node = circuit_find_or_create_node(circuit, vin_x, vin_y, 5.0f);
    circuit_add_wire(circuit, base_node, circuit_find_or_create_node(circuit, base1_x, vin_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, base1_x, vin_y, 5.0f),
                     circuit_find_or_create_node(circuit, base1_x, base1_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, base1_x, vin_y, 5.0f),
                     circuit_find_or_create_node(circuit, base2_x, base2_y, 5.0f));
    vin->node_ids[0] = base_node;
    q1->node_ids[0] = base_node;
    q2->node_ids[0] = base_node;

    // Emitters to output/load
    int out_node = circuit_find_or_create_node(circuit, emit1_x, emit1_y, 5.0f);
    circuit_add_wire(circuit, out_node, circuit_find_or_create_node(circuit, emit2_x, emit2_y, 5.0f));
    circuit_add_wire(circuit, out_node, circuit_find_or_create_node(circuit, rload_top_x, emit1_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rload_top_x, emit1_y, 5.0f),
                     circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f));
    q1->node_ids[2] = out_node;
    q2->node_ids[2] = out_node;
    rload->node_ids[0] = out_node;

    return 11;
}

// CMOS Inverter:
// Basic digital logic gate
//
//    Vdd+
//     |
//    Q1 (PMOS)
//  G--|  D
//     |--|--Output
//  G--|  D
//    Q2 (NMOS)
//     |
//    GND
//
static int place_cmos_inverter(Circuit *circuit, float x, float y) {
    Component *vdd = add_comp(circuit, COMP_DC_VOLTAGE, x, y - 60, 0);
    if (!vdd) return 0;
    vdd->props.dc_voltage.voltage = 5.0;
    Component *gnd_vdd = add_comp(circuit, COMP_GROUND, x, y + 20, 0);

    // Input (square wave for digital signal)
    Component *vin = add_comp(circuit, COMP_SQUARE_WAVE, x - 80, y + 20, 0);
    vin->props.square_wave.amplitude = 2.5;
    vin->props.square_wave.offset = 2.5;  // 0V to 5V square wave
    vin->props.square_wave.frequency = 1000.0;
    Component *gnd_in = add_comp(circuit, COMP_GROUND, x - 80, y + 100, 0);

    // PMOS (top) - G at (-20,0), D at (20,-20), S at (20,20)
    Component *q1 = add_comp(circuit, COMP_PMOS, x + 80, y - 20, 0);
    q1->props.mosfet.vth = -1.0;
    q1->props.mosfet.kp = 50e-6;

    // NMOS (bottom)
    Component *q2 = add_comp(circuit, COMP_NMOS, x + 80, y + 40, 0);
    q2->props.mosfet.vth = 1.0;
    q2->props.mosfet.kp = 110e-6;

    Component *gnd_q2 = add_comp(circuit, COMP_GROUND, x + 100, y + 80, 0);

    // Load capacitor (to see the switching behavior)
    Component *cload = add_comp(circuit, COMP_CAPACITOR, x + 160, y + 20, 90);
    cload->props.capacitor.capacitance = 100e-12;  // 100pF
    Component *gnd_cload = add_comp(circuit, COMP_GROUND, x + 160, y + 80, 0);

    // Label
    Component *label = add_comp(circuit, COMP_TEXT, x + 40, y - 100, 0);
    strncpy(label->props.text.text, "CMOS Inverter", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // === CONNECTIONS ===
    connect_terminals(circuit, vdd, 1, gnd_vdd, 0);
    connect_terminals(circuit, vin, 1, gnd_in, 0);
    connect_terminals(circuit, cload, 1, gnd_cload, 0);

    float vdd_x, vdd_y;
    component_get_terminal_pos(vdd, 0, &vdd_x, &vdd_y);
    float vin_x, vin_y;
    component_get_terminal_pos(vin, 0, &vin_x, &vin_y);
    float gate1_x, gate1_y, drain1_x, drain1_y, source1_x, source1_y;
    component_get_terminal_pos(q1, 0, &gate1_x, &gate1_y);
    component_get_terminal_pos(q1, 1, &drain1_x, &drain1_y);
    component_get_terminal_pos(q1, 2, &source1_x, &source1_y);
    float gate2_x, gate2_y, drain2_x, drain2_y, source2_x, source2_y;
    component_get_terminal_pos(q2, 0, &gate2_x, &gate2_y);
    component_get_terminal_pos(q2, 1, &drain2_x, &drain2_y);
    component_get_terminal_pos(q2, 2, &source2_x, &source2_y);
    float cload_top_x, cload_top_y;
    component_get_terminal_pos(cload, 0, &cload_top_x, &cload_top_y);
    float gnd_q2_x, gnd_q2_y;
    component_get_terminal_pos(gnd_q2, 0, &gnd_q2_x, &gnd_q2_y);

    // Vdd to PMOS source
    int vdd_node = circuit_find_or_create_node(circuit, vdd_x, vdd_y, 5.0f);
    circuit_add_wire(circuit, vdd_node, circuit_find_or_create_node(circuit, source1_x, vdd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, source1_x, vdd_y, 5.0f),
                     circuit_find_or_create_node(circuit, source1_x, source1_y, 5.0f));
    vdd->node_ids[0] = vdd_node;
    q1->node_ids[2] = vdd_node;

    // Input to both gates
    int gate_node = circuit_find_or_create_node(circuit, vin_x, vin_y, 5.0f);
    circuit_add_wire(circuit, gate_node, circuit_find_or_create_node(circuit, gate1_x, vin_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, gate1_x, vin_y, 5.0f),
                     circuit_find_or_create_node(circuit, gate1_x, gate1_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, gate1_x, vin_y, 5.0f),
                     circuit_find_or_create_node(circuit, gate2_x, gate2_y, 5.0f));
    vin->node_ids[0] = gate_node;
    q1->node_ids[0] = gate_node;
    q2->node_ids[0] = gate_node;

    // Output node (drains connected)
    int out_node = circuit_find_or_create_node(circuit, drain1_x, drain1_y, 5.0f);
    circuit_add_wire(circuit, out_node, circuit_find_or_create_node(circuit, drain2_x, drain2_y, 5.0f));
    circuit_add_wire(circuit, out_node, circuit_find_or_create_node(circuit, cload_top_x, drain1_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cload_top_x, drain1_y, 5.0f),
                     circuit_find_or_create_node(circuit, cload_top_x, cload_top_y, 5.0f));
    q1->node_ids[1] = out_node;
    q2->node_ids[1] = out_node;
    cload->node_ids[0] = out_node;

    // NMOS source to ground
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, source2_x, source2_y, 5.0f),
                     circuit_find_or_create_node(circuit, gnd_q2_x, gnd_q2_y, 5.0f));
    q2->node_ids[2] = circuit_find_or_create_node(circuit, gnd_q2_x, gnd_q2_y, 5.0f);

    return 10;
}

// Op-Amp Integrator:
//              +---- C ----+
//              |           |
// Vin ---[R]---+--(-)\     |
//                  (+)/----+--- Vout
//                   |
//                  GND
//
static int place_integrator(Circuit *circuit, float x, float y) {
    // Input source
    Component *vsrc = add_comp(circuit, COMP_SQUARE_WAVE, x, y + 40, 0);
    if (!vsrc) return 0;
    vsrc->props.square_wave.amplitude = 1.0;
    vsrc->props.square_wave.frequency = 100.0;
    vsrc->props.square_wave.offset = 0.0;
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Input resistor
    Component *ri = add_comp(circuit, COMP_RESISTOR, x + 80, y, 0);
    ri->props.resistor.resistance = 10000.0;

    // Op-amp
    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 200, y + 20, 0);

    // Feedback capacitor
    Component *cf = add_comp(circuit, COMP_CAPACITOR, x + 200, y - 40, 0);
    cf->props.capacitor.capacitance = 100e-9;

    // Ground for + input
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 160, y + 60, 0);

    // Label
    Component *label = add_comp(circuit, COMP_TEXT, x + 100, y - 80, 0);
    strncpy(label->props.text.text, "Op-Amp Integrator", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // === CONNECTIONS ===
    connect_terminals(circuit, vsrc, 1, gnd, 0);
    connect_terminals(circuit, vsrc, 0, ri, 0);
    connect_terminals(circuit, opamp, 1, gnd2, 0);

    // Get positions
    float inv_x, inv_y;
    component_get_terminal_pos(opamp, 0, &inv_x, &inv_y);
    float out_x, out_y;
    component_get_terminal_pos(opamp, 2, &out_x, &out_y);
    float cf_left_x, cf_left_y, cf_right_x, cf_right_y;
    component_get_terminal_pos(cf, 0, &cf_left_x, &cf_left_y);
    component_get_terminal_pos(cf, 1, &cf_right_x, &cf_right_y);
    float ri_right_x, ri_right_y;
    component_get_terminal_pos(ri, 1, &ri_right_x, &ri_right_y);

    // Ri to - input
    int inv_node = circuit_find_or_create_node(circuit, inv_x, inv_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, ri_right_x, ri_right_y, 5.0f), inv_node);
    ri->node_ids[1] = inv_node;
    opamp->node_ids[0] = inv_node;

    // Feedback cap: - input to output via top path
    circuit_add_wire(circuit, inv_node, circuit_find_or_create_node(circuit, inv_x, cf_left_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, inv_x, cf_left_y, 5.0f),
                     circuit_find_or_create_node(circuit, cf_left_x, cf_left_y, 5.0f));
    cf->node_ids[0] = inv_node;

    int out_node = circuit_find_or_create_node(circuit, out_x, out_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cf_right_x, cf_right_y, 5.0f),
                     circuit_find_or_create_node(circuit, out_x, cf_right_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, out_x, cf_right_y, 5.0f), out_node);
    cf->node_ids[1] = out_node;
    opamp->node_ids[2] = out_node;

    return 7;
}

// Op-Amp Differentiator:
//              +---- R ----+
//              |           |
// Vin ---[C]---+--(-)\     |
//                  (+)/----+--- Vout
//                   |
//                  GND
//
static int place_differentiator(Circuit *circuit, float x, float y) {
    // Input source (triangle wave for smooth differentiation)
    Component *vsrc = add_comp(circuit, COMP_TRIANGLE_WAVE, x, y + 40, 0);
    if (!vsrc) return 0;
    vsrc->props.triangle_wave.amplitude = 1.0;
    vsrc->props.triangle_wave.frequency = 100.0;
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Input capacitor
    Component *ci = add_comp(circuit, COMP_CAPACITOR, x + 80, y, 0);
    ci->props.capacitor.capacitance = 100e-9;

    // Op-amp
    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 200, y + 20, 0);

    // Feedback resistor
    Component *rf = add_comp(circuit, COMP_RESISTOR, x + 200, y - 40, 0);
    rf->props.resistor.resistance = 10000.0;

    // Ground for + input
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 160, y + 60, 0);

    // Label
    Component *label = add_comp(circuit, COMP_TEXT, x + 80, y - 80, 0);
    strncpy(label->props.text.text, "Op-Amp Differentiator", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // === CONNECTIONS ===
    connect_terminals(circuit, vsrc, 1, gnd, 0);
    connect_terminals(circuit, vsrc, 0, ci, 0);
    connect_terminals(circuit, opamp, 1, gnd2, 0);

    // Get positions
    float inv_x, inv_y;
    component_get_terminal_pos(opamp, 0, &inv_x, &inv_y);
    float out_x, out_y;
    component_get_terminal_pos(opamp, 2, &out_x, &out_y);
    float rf_left_x, rf_left_y, rf_right_x, rf_right_y;
    component_get_terminal_pos(rf, 0, &rf_left_x, &rf_left_y);
    component_get_terminal_pos(rf, 1, &rf_right_x, &rf_right_y);
    float ci_right_x, ci_right_y;
    component_get_terminal_pos(ci, 1, &ci_right_x, &ci_right_y);

    // Ci to - input
    int inv_node = circuit_find_or_create_node(circuit, inv_x, inv_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, ci_right_x, ci_right_y, 5.0f), inv_node);
    ci->node_ids[1] = inv_node;
    opamp->node_ids[0] = inv_node;

    // Feedback resistor: - input to output via top path
    circuit_add_wire(circuit, inv_node, circuit_find_or_create_node(circuit, inv_x, rf_left_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, inv_x, rf_left_y, 5.0f),
                     circuit_find_or_create_node(circuit, rf_left_x, rf_left_y, 5.0f));
    rf->node_ids[0] = inv_node;

    int out_node = circuit_find_or_create_node(circuit, out_x, out_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rf_right_x, rf_right_y, 5.0f),
                     circuit_find_or_create_node(circuit, out_x, rf_right_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, out_x, rf_right_y, 5.0f), out_node);
    rf->node_ids[1] = out_node;
    opamp->node_ids[2] = out_node;

    return 7;
}

// Summing Amplifier (3 inputs):
//              +--------- Rf --------+
//              |                     |
// V1 ---[R1]---+                     |
// V2 ---[R2]---+---(-)\              |
// V3 ---[R3]---+   (+)/-----+--------+--- Vout
//                   |
//                  GND
//
static int place_summing_amp(Circuit *circuit, float x, float y) {
    // Three input sources
    Component *v1 = add_comp(circuit, COMP_DC_VOLTAGE, x - 80, y - 40, 0);
    if (!v1) return 0;
    v1->props.dc_voltage.voltage = 1.0;
    Component *gnd1 = add_comp(circuit, COMP_GROUND, x - 80, y + 40, 0);

    Component *v2 = add_comp(circuit, COMP_DC_VOLTAGE, x - 80, y + 80, 0);
    v2->props.dc_voltage.voltage = 2.0;
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x - 80, y + 160, 0);

    Component *v3 = add_comp(circuit, COMP_DC_VOLTAGE, x - 80, y + 200, 0);
    v3->props.dc_voltage.voltage = 3.0;
    Component *gnd3 = add_comp(circuit, COMP_GROUND, x - 80, y + 280, 0);

    // Input resistors
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 20, y - 80, 0);
    r1->props.resistor.resistance = 10000.0;
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 20, y + 40, 0);
    r2->props.resistor.resistance = 10000.0;
    Component *r3 = add_comp(circuit, COMP_RESISTOR, x + 20, y + 160, 0);
    r3->props.resistor.resistance = 10000.0;

    // Op-amp
    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 180, y + 60, 0);

    // Feedback resistor
    Component *rf = add_comp(circuit, COMP_RESISTOR, x + 180, y, 0);
    rf->props.resistor.resistance = 10000.0;

    // Ground for + input
    Component *gnd_op = add_comp(circuit, COMP_GROUND, x + 140, y + 100, 0);

    // Label
    Component *label = add_comp(circuit, COMP_TEXT, x + 60, y - 120, 0);
    strncpy(label->props.text.text, "Summing Amp (Vout = -(V1+V2+V3))", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // === CONNECTIONS ===
    connect_terminals(circuit, v1, 1, gnd1, 0);
    connect_terminals(circuit, v2, 1, gnd2, 0);
    connect_terminals(circuit, v3, 1, gnd3, 0);
    connect_terminals(circuit, v1, 0, r1, 0);
    connect_terminals(circuit, v2, 0, r2, 0);
    connect_terminals(circuit, v3, 0, r3, 0);
    connect_terminals(circuit, opamp, 1, gnd_op, 0);

    // Get positions
    float inv_x, inv_y;
    component_get_terminal_pos(opamp, 0, &inv_x, &inv_y);
    float out_x, out_y;
    component_get_terminal_pos(opamp, 2, &out_x, &out_y);
    float rf_left_x, rf_left_y, rf_right_x, rf_right_y;
    component_get_terminal_pos(rf, 0, &rf_left_x, &rf_left_y);
    component_get_terminal_pos(rf, 1, &rf_right_x, &rf_right_y);
    float r1_right_x, r1_right_y;
    component_get_terminal_pos(r1, 1, &r1_right_x, &r1_right_y);
    float r2_right_x, r2_right_y;
    component_get_terminal_pos(r2, 1, &r2_right_x, &r2_right_y);
    float r3_right_x, r3_right_y;
    component_get_terminal_pos(r3, 1, &r3_right_x, &r3_right_y);

    // Junction node at - input
    int inv_node = circuit_find_or_create_node(circuit, inv_x, inv_y, 5.0f);
    opamp->node_ids[0] = inv_node;

    // Connect all input resistors to junction via vertical bus
    float bus_x = inv_x - 20;  // Vertical bus position
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r1_right_x, r1_right_y, 5.0f),
                     circuit_find_or_create_node(circuit, bus_x, r1_right_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, bus_x, r1_right_y, 5.0f),
                     circuit_find_or_create_node(circuit, bus_x, inv_y, 5.0f));
    r1->node_ids[1] = inv_node;

    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r2_right_x, r2_right_y, 5.0f),
                     circuit_find_or_create_node(circuit, bus_x, r2_right_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, bus_x, r2_right_y, 5.0f),
                     circuit_find_or_create_node(circuit, bus_x, inv_y, 5.0f));
    r2->node_ids[1] = inv_node;

    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r3_right_x, r3_right_y, 5.0f),
                     circuit_find_or_create_node(circuit, bus_x, r3_right_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, bus_x, r3_right_y, 5.0f),
                     circuit_find_or_create_node(circuit, bus_x, inv_y, 5.0f));
    r3->node_ids[1] = inv_node;

    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, bus_x, inv_y, 5.0f), inv_node);

    // Feedback resistor
    circuit_add_wire(circuit, inv_node, circuit_find_or_create_node(circuit, inv_x, rf_left_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, inv_x, rf_left_y, 5.0f),
                     circuit_find_or_create_node(circuit, rf_left_x, rf_left_y, 5.0f));
    rf->node_ids[0] = inv_node;

    int out_node = circuit_find_or_create_node(circuit, out_x, out_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rf_right_x, rf_right_y, 5.0f),
                     circuit_find_or_create_node(circuit, out_x, rf_right_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, out_x, rf_right_y, 5.0f), out_node);
    rf->node_ids[1] = out_node;
    opamp->node_ids[2] = out_node;

    return 14;
}

// Comparator:
// Compares two voltages and outputs high or low
//
// Vref ---+---(+)\
//              (-)/-----+--- Vout
// Vin ---+
//
static int place_comparator(Circuit *circuit, float x, float y) {
    // Reference voltage (voltage divider)
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x - 80, y - 60, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 10.0;
    Component *gnd_vcc = add_comp(circuit, COMP_GROUND, x - 80, y + 20, 0);

    Component *r1 = add_comp(circuit, COMP_RESISTOR, x, y - 40, 90);
    r1->props.resistor.resistance = 10000.0;
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x, y + 40, 90);
    r2->props.resistor.resistance = 10000.0;
    Component *gnd_r2 = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Input signal (sine wave crossing the reference)
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x - 80, y + 140, 0);
    vin->props.ac_voltage.amplitude = 6.0;
    vin->props.ac_voltage.offset = 5.0;  // Oscillates around 5V reference
    vin->props.ac_voltage.frequency = 100.0;
    Component *gnd_in = add_comp(circuit, COMP_GROUND, x - 80, y + 220, 0);

    // Op-amp as comparator
    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 120, y + 40, 0);

    // Pull-up resistor for output (single supply operation)
    Component *rpu = add_comp(circuit, COMP_RESISTOR, x + 200, y - 20, 90);
    rpu->props.resistor.resistance = 10000.0;

    // Label
    Component *label = add_comp(circuit, COMP_TEXT, x + 40, y - 100, 0);
    strncpy(label->props.text.text, "Voltage Comparator", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // === CONNECTIONS ===
    connect_terminals(circuit, vcc, 1, gnd_vcc, 0);
    connect_terminals(circuit, r2, 1, gnd_r2, 0);
    connect_terminals(circuit, vin, 1, gnd_in, 0);

    float vcc_x, vcc_y;
    component_get_terminal_pos(vcc, 0, &vcc_x, &vcc_y);
    float r1_top_x, r1_top_y, r1_bot_x, r1_bot_y;
    component_get_terminal_pos(r1, 0, &r1_top_x, &r1_top_y);
    component_get_terminal_pos(r1, 1, &r1_bot_x, &r1_bot_y);
    float r2_top_x, r2_top_y;
    component_get_terminal_pos(r2, 0, &r2_top_x, &r2_top_y);
    float vin_x, vin_y;
    component_get_terminal_pos(vin, 0, &vin_x, &vin_y);
    float inv_x, inv_y, noninv_x, noninv_y, out_x, out_y;
    component_get_terminal_pos(opamp, 0, &inv_x, &inv_y);
    component_get_terminal_pos(opamp, 1, &noninv_x, &noninv_y);
    component_get_terminal_pos(opamp, 2, &out_x, &out_y);
    float rpu_top_x, rpu_top_y, rpu_bot_x, rpu_bot_y;
    component_get_terminal_pos(rpu, 0, &rpu_top_x, &rpu_top_y);
    component_get_terminal_pos(rpu, 1, &rpu_bot_x, &rpu_bot_y);

    // Vcc to R1 and pull-up
    int vcc_node = circuit_find_or_create_node(circuit, vcc_x, vcc_y, 5.0f);
    circuit_add_wire(circuit, vcc_node, circuit_find_or_create_node(circuit, r1_top_x, vcc_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r1_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, r1_top_x, r1_top_y, 5.0f));
    vcc->node_ids[0] = vcc_node;
    r1->node_ids[0] = vcc_node;

    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r1_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rpu_top_x, vcc_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rpu_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rpu_top_x, rpu_top_y, 5.0f));
    rpu->node_ids[0] = vcc_node;

    // Reference voltage junction (R1/R2 divider) to + input
    int ref_node = circuit_find_or_create_node(circuit, r1_bot_x, r1_bot_y, 5.0f);
    circuit_add_wire(circuit, ref_node, circuit_find_or_create_node(circuit, r2_top_x, r2_top_y, 5.0f));
    r1->node_ids[1] = ref_node;
    r2->node_ids[0] = ref_node;

    circuit_add_wire(circuit, ref_node, circuit_find_or_create_node(circuit, noninv_x, r1_bot_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, noninv_x, r1_bot_y, 5.0f),
                     circuit_find_or_create_node(circuit, noninv_x, noninv_y, 5.0f));
    opamp->node_ids[1] = ref_node;

    // Input signal to - input
    int vin_node = circuit_find_or_create_node(circuit, vin_x, vin_y, 5.0f);
    circuit_add_wire(circuit, vin_node, circuit_find_or_create_node(circuit, inv_x, vin_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, inv_x, vin_y, 5.0f),
                     circuit_find_or_create_node(circuit, inv_x, inv_y, 5.0f));
    vin->node_ids[0] = vin_node;
    opamp->node_ids[0] = vin_node;

    // Output with pull-up
    int out_node = circuit_find_or_create_node(circuit, out_x, out_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rpu_bot_x, rpu_bot_y, 5.0f),
                     circuit_find_or_create_node(circuit, out_x, rpu_bot_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, out_x, rpu_bot_y, 5.0f), out_node);
    rpu->node_ids[1] = out_node;
    opamp->node_ids[2] = out_node;

    return 12;
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
        case CIRCUIT_DIFFERENTIAL_PAIR:
            return place_differential_pair(circuit, x, y);
        case CIRCUIT_CURRENT_MIRROR:
            return place_current_mirror(circuit, x, y);
        case CIRCUIT_PUSH_PULL:
            return place_push_pull(circuit, x, y);
        case CIRCUIT_CMOS_INVERTER:
            return place_cmos_inverter(circuit, x, y);
        case CIRCUIT_INTEGRATOR:
            return place_integrator(circuit, x, y);
        case CIRCUIT_DIFFERENTIATOR:
            return place_differentiator(circuit, x, y);
        case CIRCUIT_SUMMING_AMP:
            return place_summing_amp(circuit, x, y);
        case CIRCUIT_COMPARATOR:
            return place_comparator(circuit, x, y);
        default:
            return 0;
    }
}
