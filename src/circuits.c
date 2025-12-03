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
    // Power supply / rectifier circuits
    [CIRCUIT_FULLWAVE_BRIDGE] = {"Bridge Rectifier", "Brdg", "Full-wave bridge rectifier with filter"},
    [CIRCUIT_CENTERTAP_RECT] = {"Center-Tap Rect", "CTap", "Center-tap transformer rectifier"},
    [CIRCUIT_AC_DC_SUPPLY] = {"AC-DC Supply", "ACDC", "Complete AC to DC power supply"},
    [CIRCUIT_AC_DC_AMERICAN] = {"US 120V-12V", "US12", "American 120V/60Hz to 12V DC"},
    // TI Analog Circuits - Amplifiers
    [CIRCUIT_DIFFERENCE_AMP] = {"Difference Amp", "Diff", "Op-amp difference amplifier (subtractor)"},
    [CIRCUIT_TRANSIMPEDANCE] = {"Transimpedance", "TIA", "Transimpedance amplifier (I to V)"},
    [CIRCUIT_INSTR_AMP] = {"Instr. Amp", "Inst", "Three op-amp instrumentation amplifier"},
    // TI Analog Circuits - Filters
    [CIRCUIT_SALLEN_KEY_LP] = {"Sallen-Key LP", "S-K", "2nd order Sallen-Key low pass filter"},
    [CIRCUIT_BANDPASS_ACTIVE] = {"Active Bandpass", "BPF", "Active band pass filter"},
    [CIRCUIT_NOTCH_FILTER] = {"Notch Filter", "Notc", "Twin-T 60Hz notch filter"},
    // TI Analog Circuits - Signal Sources
    [CIRCUIT_WIEN_OSCILLATOR] = {"Wien Oscillator", "Wien", "Wien bridge sine wave oscillator"},
    [CIRCUIT_CURRENT_SOURCE] = {"Current Source", "Isrc", "BJT constant current source"},
    // TI Analog Circuits - Comparators/Detection
    [CIRCUIT_WINDOW_COMP] = {"Window Comp", "WCmp", "Window comparator (OV/UV detection)"},
    [CIRCUIT_HYSTERESIS_COMP] = {"Schmitt Trigger", "Schm", "Comparator with hysteresis"},
    // TI Analog Circuits - Power/Voltage
    [CIRCUIT_ZENER_REF] = {"Zener Reference", "Zref", "Zener diode voltage reference"},
    [CIRCUIT_PRECISION_RECT] = {"Precision Rect", "PRec", "Precision full-wave rectifier"},
    // Voltage Regulator Circuits
    [CIRCUIT_7805_REG] = {"7805 Regulator", "7805", "7805 fixed 5V regulator with filtering"},
    [CIRCUIT_LM317_REG] = {"LM317 Adj Reg", "317", "LM317 adjustable regulator with voltage set"},
    [CIRCUIT_TL431_REF] = {"TL431 Reference", "431", "TL431 precision shunt reference"},
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
    // Non-inverting amplifier using flipped op-amp (+ on top, - on bottom)
    // All wires are Manhattan routed (horizontal and vertical only)
    //
    // COMP_OPAMP_FLIPPED terminal offsets: + at (-40,-20), - at (-40,+20), out at (40,0)
    // AC Voltage source terminal offsets: + at (0,-40), - at (0,+40)
    //
    // Layout (wide spacing to avoid overlaps):
    //
    //  vsrc (x-60)       Rf (x+140, y-20)
    //    |              /              \
    //   gnd          opamp-  ----  opamp out
    //  (x-60)           |              |
    //                  Ri              |
    //                   |              |
    //                 gnd2 (x+60)      |
    //                                  |
    //            vsrc+ ---- opamp+ ----+
    //

    // AC voltage source - moved further left for spacing
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x - 60, y + 40, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 0.5;
    vsrc->props.ac_voltage.frequency = 1000.0;
    // vsrc+ at (x-60, y+0), vsrc- at (x-60, y+80)

    // Ground for source
    Component *gnd = add_comp(circuit, COMP_GROUND, x - 60, y + 100, 0);
    // gnd terminal at (x-60, y+80)

    // Flipped Op-amp: position so + input aligns with vsrc+ Y level (y+0)
    // opamp center at (x+140, y+20) gives: + at (x+100, y+0), - at (x+100, y+40), out at (x+180, y+20)
    Component *opamp = add_comp(circuit, COMP_OPAMP_FLIPPED, x + 140, y + 20, 0);

    // Ri vertical resistor - below the - input, connect to ground
    // Place at x+100 (same X as opamp-) with rotation 90 for vertical
    Component *ri = add_comp(circuit, COMP_RESISTOR, x + 100, y + 80, 90);
    ri->props.resistor.resistance = 1000.0;
    // Ri top at (x+100, y+40), Ri bottom at (x+100, y+120)

    // Ground for Ri - below the resistor (well separated from gnd at x-60)
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 100, y + 160, 0);
    // gnd2 terminal at (x+100, y+140)

    // Rf horizontal feedback resistor - connect to - input and output
    // Place above the opamp for feedback routing
    Component *rf = add_comp(circuit, COMP_RESISTOR, x + 140, y - 20, 0);
    rf->props.resistor.resistance = 10000.0;
    // Rf left at (x+100, y-20), Rf right at (x+180, y-20)

    // Get terminal positions
    float vsrc_pos_x, vsrc_pos_y, vsrc_neg_x, vsrc_neg_y;
    component_get_terminal_pos(vsrc, 0, &vsrc_pos_x, &vsrc_pos_y);
    component_get_terminal_pos(vsrc, 1, &vsrc_neg_x, &vsrc_neg_y);
    float noninv_x, noninv_y;
    component_get_terminal_pos(opamp, 0, &noninv_x, &noninv_y);
    float inv_x, inv_y;
    component_get_terminal_pos(opamp, 1, &inv_x, &inv_y);
    float ri_top_x, ri_top_y, ri_bot_x, ri_bot_y;
    component_get_terminal_pos(ri, 0, &ri_top_x, &ri_top_y);
    component_get_terminal_pos(ri, 1, &ri_bot_x, &ri_bot_y);
    float rf_left_x, rf_left_y, rf_right_x, rf_right_y;
    component_get_terminal_pos(rf, 0, &rf_left_x, &rf_left_y);
    component_get_terminal_pos(rf, 1, &rf_right_x, &rf_right_y);
    float out_x, out_y;
    component_get_terminal_pos(opamp, 2, &out_x, &out_y);
    float gnd_x, gnd_y, gnd2_x, gnd2_y;
    component_get_terminal_pos(gnd, 0, &gnd_x, &gnd_y);
    component_get_terminal_pos(gnd2, 0, &gnd2_x, &gnd2_y);

    // Connect source negative to ground (direct vertical wire)
    int gnd_node = circuit_find_or_create_node(circuit, gnd_x, gnd_y, 5.0f);
    vsrc->node_ids[1] = gnd_node;
    gnd->node_ids[0] = gnd_node;
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vsrc_neg_x, vsrc_neg_y, 5.0f), gnd_node);

    // Connect vsrc+ to opamp+ (horizontal wire, both at same Y=y+0)
    int noninv_node = circuit_find_or_create_node(circuit, noninv_x, noninv_y, 5.0f);
    vsrc->node_ids[0] = noninv_node;
    opamp->node_ids[0] = noninv_node;
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vsrc_pos_x, vsrc_pos_y, 5.0f), noninv_node);

    // Create - input node (Ri top connects to opamp-)
    int inv_node = circuit_find_or_create_node(circuit, inv_x, inv_y, 5.0f);
    opamp->node_ids[1] = inv_node;
    ri->node_ids[0] = inv_node;
    // Direct vertical wire from ri_top to opamp- (same X)
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, ri_top_x, ri_top_y, 5.0f), inv_node);

    // Connect Ri bottom to ground2
    int gnd2_node = circuit_find_or_create_node(circuit, gnd2_x, gnd2_y, 5.0f);
    gnd2->node_ids[0] = gnd2_node;
    ri->node_ids[1] = gnd2_node;
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, ri_bot_x, ri_bot_y, 5.0f), gnd2_node);

    // Connect Rf left to - input: vertical wire UP from inv to rf_left
    // rf_left at (x+100, y-20), inv at (x+100, y+40) - same X
    rf->node_ids[0] = inv_node;
    circuit_add_wire(circuit, inv_node, circuit_find_or_create_node(circuit, rf_left_x, rf_left_y, 5.0f));

    // Connect Rf right to output: vertical wire DOWN from rf_right to out
    // rf_right at (x+180, y-20), out at (x+180, y+20) - same X
    int out_node = circuit_find_or_create_node(circuit, out_x, out_y, 5.0f);
    opamp->node_ids[2] = out_node;
    rf->node_ids[1] = out_node;
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rf_right_x, rf_right_y, 5.0f), out_node);

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
// === DIFFERENTIAL PAIR ===
static int place_differential_pair(Circuit *circuit, float x, float y) {
    // Clean differential pair layout with orthogonal wiring only
    // Layout: VCC at top, RC resistors below, Q1/Q2 transistors, RE tail resistor, ground at bottom
    // Input sources on far left/right, coupling capacitors feed into bases

    // Label at top
    Component *label = add_comp(circuit, COMP_TEXT, x + 60, y - 140, 0);
    if (!label) return 0;
    strncpy(label->props.text.text, "Differential Pair", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // VCC power supply - centered at top
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x + 100, y - 100, 0);
    vcc->props.dc_voltage.voltage = 12.0;

    // Collector resistors - RC1 for Q1, RC2 for Q2
    Component *rc1 = add_comp(circuit, COMP_RESISTOR, x + 60, y - 40, 90);
    rc1->props.resistor.resistance = 4700.0;
    Component *rc2 = add_comp(circuit, COMP_RESISTOR, x + 140, y - 40, 90);
    rc2->props.resistor.resistance = 4700.0;

    // NPN transistors - Q1 on left, Q2 on right (both facing inward)
    // Q1: normal orientation (0 deg) - base on left, collector on top-right, emitter on bottom-right
    // Q2: mirrored (180 deg) - base on right, collector on top-left, emitter on bottom-left
    Component *q1 = add_comp(circuit, COMP_NPN_BJT, x + 40, y + 40, 0);
    q1->props.bjt.bf = 100;
    Component *q2 = add_comp(circuit, COMP_NPN_BJT, x + 160, y + 40, 180);
    q2->props.bjt.bf = 100;

    // Tail resistor RE - connected to both emitters
    Component *re = add_comp(circuit, COMP_RESISTOR, x + 100, y + 100, 90);
    re->props.resistor.resistance = 10000.0;

    // Ground at bottom center
    Component *gnd = add_comp(circuit, COMP_GROUND, x + 100, y + 180, 0);

    // Input coupling capacitors - horizontal orientation
    Component *cin1 = add_comp(circuit, COMP_CAPACITOR, x - 40, y + 40, 0);
    cin1->props.capacitor.capacitance = 10e-6;
    Component *cin2 = add_comp(circuit, COMP_CAPACITOR, x + 240, y + 40, 180);
    cin2->props.capacitor.capacitance = 10e-6;

    // AC input sources - on far left and right
    Component *vin1 = add_comp(circuit, COMP_AC_VOLTAGE, x - 100, y + 80, 0);
    vin1->props.ac_voltage.amplitude = 0.05;
    vin1->props.ac_voltage.frequency = 1000.0;
    Component *vin2 = add_comp(circuit, COMP_AC_VOLTAGE, x + 300, y + 80, 0);
    vin2->props.ac_voltage.amplitude = 0.05;
    vin2->props.ac_voltage.frequency = 1000.0;
    vin2->props.ac_voltage.phase = 180.0;

    // Get terminal positions
    float vcc_pos_x, vcc_pos_y, vcc_neg_x, vcc_neg_y;
    component_get_terminal_pos(vcc, 0, &vcc_pos_x, &vcc_pos_y);
    component_get_terminal_pos(vcc, 1, &vcc_neg_x, &vcc_neg_y);

    float rc1_top_x, rc1_top_y, rc1_bot_x, rc1_bot_y;
    component_get_terminal_pos(rc1, 0, &rc1_top_x, &rc1_top_y);
    component_get_terminal_pos(rc1, 1, &rc1_bot_x, &rc1_bot_y);
    float rc2_top_x, rc2_top_y, rc2_bot_x, rc2_bot_y;
    component_get_terminal_pos(rc2, 0, &rc2_top_x, &rc2_top_y);
    component_get_terminal_pos(rc2, 1, &rc2_bot_x, &rc2_bot_y);

    float base1_x, base1_y, coll1_x, coll1_y, emit1_x, emit1_y;
    component_get_terminal_pos(q1, 0, &base1_x, &base1_y);
    component_get_terminal_pos(q1, 1, &coll1_x, &coll1_y);
    component_get_terminal_pos(q1, 2, &emit1_x, &emit1_y);
    float base2_x, base2_y, coll2_x, coll2_y, emit2_x, emit2_y;
    component_get_terminal_pos(q2, 0, &base2_x, &base2_y);
    component_get_terminal_pos(q2, 1, &coll2_x, &coll2_y);
    component_get_terminal_pos(q2, 2, &emit2_x, &emit2_y);

    float re_top_x, re_top_y, re_bot_x, re_bot_y;
    component_get_terminal_pos(re, 0, &re_top_x, &re_top_y);
    component_get_terminal_pos(re, 1, &re_bot_x, &re_bot_y);

    float gnd_x, gnd_y;
    component_get_terminal_pos(gnd, 0, &gnd_x, &gnd_y);

    float cin1_in_x, cin1_in_y, cin1_out_x, cin1_out_y;
    component_get_terminal_pos(cin1, 0, &cin1_in_x, &cin1_in_y);
    component_get_terminal_pos(cin1, 1, &cin1_out_x, &cin1_out_y);
    float cin2_in_x, cin2_in_y, cin2_out_x, cin2_out_y;
    component_get_terminal_pos(cin2, 0, &cin2_in_x, &cin2_in_y);
    component_get_terminal_pos(cin2, 1, &cin2_out_x, &cin2_out_y);

    float vin1_pos_x, vin1_pos_y, vin1_neg_x, vin1_neg_y;
    component_get_terminal_pos(vin1, 0, &vin1_pos_x, &vin1_pos_y);
    component_get_terminal_pos(vin1, 1, &vin1_neg_x, &vin1_neg_y);
    float vin2_pos_x, vin2_pos_y, vin2_neg_x, vin2_neg_y;
    component_get_terminal_pos(vin2, 0, &vin2_pos_x, &vin2_pos_y);
    component_get_terminal_pos(vin2, 1, &vin2_neg_x, &vin2_neg_y);

    // Create ground node
    int gnd_node = circuit_find_or_create_node(circuit, gnd_x, gnd_y, 5.0f);
    gnd->node_ids[0] = gnd_node;

    // === VCC POWER RAIL (horizontal line at top) ===
    float vcc_rail_y = vcc_pos_y;
    int vcc_node = circuit_find_or_create_node(circuit, vcc_pos_x, vcc_rail_y, 5.0f);
    vcc->node_ids[0] = vcc_node;

    // VCC- goes down to ground rail
    float gnd_rail_y = gnd_y;
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vcc_neg_x, vcc_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, vcc_neg_x, gnd_rail_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vcc_neg_x, gnd_rail_y, 5.0f), gnd_node);
    vcc->node_ids[1] = gnd_node;

    // VCC+ to RC1 top: go left then down
    circuit_add_wire(circuit, vcc_node, circuit_find_or_create_node(circuit, rc1_top_x, vcc_rail_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rc1_top_x, vcc_rail_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc1_top_x, rc1_top_y, 5.0f));
    rc1->node_ids[0] = vcc_node;

    // VCC+ to RC2 top: go right then down
    circuit_add_wire(circuit, vcc_node, circuit_find_or_create_node(circuit, rc2_top_x, vcc_rail_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rc2_top_x, vcc_rail_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc2_top_x, rc2_top_y, 5.0f));
    rc2->node_ids[0] = vcc_node;

    // === COLLECTOR CONNECTIONS ===
    // RC1 bottom to Q1 collector - go down then right to collector
    int coll1_node = circuit_find_or_create_node(circuit, coll1_x, coll1_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rc1_bot_x, rc1_bot_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc1_bot_x, coll1_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rc1_bot_x, coll1_y, 5.0f), coll1_node);
    rc1->node_ids[1] = coll1_node;
    q1->node_ids[1] = coll1_node;

    // RC2 bottom to Q2 collector - go down then left to collector
    int coll2_node = circuit_find_or_create_node(circuit, coll2_x, coll2_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rc2_bot_x, rc2_bot_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc2_bot_x, coll2_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rc2_bot_x, coll2_y, 5.0f), coll2_node);
    rc2->node_ids[1] = coll2_node;
    q2->node_ids[1] = coll2_node;

    // === EMITTER TAIL CONNECTION ===
    // Both emitters connect above RE, then drop down to RE top to avoid crossing the resistor
    // Use a horizontal wire above RE (at emit1_y level + some offset to clear transistor bodies)
    float emitter_bus_y = emit1_y + 15;  // Horizontal bus just below emitters

    // Tail node at RE top
    int tail_node = circuit_find_or_create_node(circuit, re_top_x, re_top_y, 5.0f);
    re->node_ids[0] = tail_node;

    // Create emitter bus node (where both emitters meet, directly above RE)
    int emitter_bus_node = circuit_find_or_create_node(circuit, re_top_x, emitter_bus_y, 5.0f);

    // Q1 emitter: down to bus level, then right to center
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, emit1_x, emit1_y, 5.0f),
                     circuit_find_or_create_node(circuit, emit1_x, emitter_bus_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, emit1_x, emitter_bus_y, 5.0f), emitter_bus_node);
    q1->node_ids[2] = emitter_bus_node;

    // Q2 emitter: down to bus level, then left to center
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, emit2_x, emit2_y, 5.0f),
                     circuit_find_or_create_node(circuit, emit2_x, emitter_bus_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, emit2_x, emitter_bus_y, 5.0f), emitter_bus_node);
    q2->node_ids[2] = emitter_bus_node;

    // Drop from emitter bus down to RE top (vertical wire, doesn't cross RE body)
    circuit_add_wire(circuit, emitter_bus_node, tail_node);

    // RE bottom to ground
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, re_bot_x, re_bot_y, 5.0f), gnd_node);
    re->node_ids[1] = gnd_node;

    // === INPUT COUPLING CAPACITOR TO BASE CONNECTIONS ===
    // Cin1 output to Q1 base - horizontal wire
    int base1_node = circuit_find_or_create_node(circuit, base1_x, base1_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cin1_out_x, cin1_out_y, 5.0f),
                     circuit_find_or_create_node(circuit, base1_x, cin1_out_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, base1_x, cin1_out_y, 5.0f), base1_node);
    cin1->node_ids[1] = base1_node;
    q1->node_ids[0] = base1_node;

    // Cin2 output to Q2 base - horizontal wire
    int base2_node = circuit_find_or_create_node(circuit, base2_x, base2_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cin2_out_x, cin2_out_y, 5.0f),
                     circuit_find_or_create_node(circuit, base2_x, cin2_out_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, base2_x, cin2_out_y, 5.0f), base2_node);
    cin2->node_ids[1] = base2_node;
    q2->node_ids[0] = base2_node;

    // === VIN1 TO CIN1 CONNECTION ===
    // Vin1+ up to capacitor input level, then right to cin1 input
    int vin1_node = circuit_find_or_create_node(circuit, vin1_pos_x, vin1_pos_y, 5.0f);
    vin1->node_ids[0] = vin1_node;
    circuit_add_wire(circuit, vin1_node, circuit_find_or_create_node(circuit, vin1_pos_x, cin1_in_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vin1_pos_x, cin1_in_y, 5.0f),
                     circuit_find_or_create_node(circuit, cin1_in_x, cin1_in_y, 5.0f));
    cin1->node_ids[0] = vin1_node;

    // Vin1- to ground: down then right to ground rail
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vin1_neg_x, vin1_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, vin1_neg_x, gnd_rail_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vin1_neg_x, gnd_rail_y, 5.0f), gnd_node);
    vin1->node_ids[1] = gnd_node;

    // === VIN2 TO CIN2 CONNECTION ===
    // Vin2+ up to capacitor input level, then left to cin2 input
    int vin2_node = circuit_find_or_create_node(circuit, vin2_pos_x, vin2_pos_y, 5.0f);
    vin2->node_ids[0] = vin2_node;
    circuit_add_wire(circuit, vin2_node, circuit_find_or_create_node(circuit, vin2_pos_x, cin2_in_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vin2_pos_x, cin2_in_y, 5.0f),
                     circuit_find_or_create_node(circuit, cin2_in_x, cin2_in_y, 5.0f));
    cin2->node_ids[0] = vin2_node;

    // Vin2- to ground: down then left to ground rail
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vin2_neg_x, vin2_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, vin2_neg_x, gnd_rail_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vin2_neg_x, gnd_rail_y, 5.0f), gnd_node);
    vin2->node_ids[1] = gnd_node;

    return 11;
}

// === CURRENT MIRROR ===
static int place_current_mirror(Circuit *circuit, float x, float y) {
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x, y - 80, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 12.0;

    // Single ground at bottom center
    Component *gnd = add_comp(circuit, COMP_GROUND, x + 100, y + 80, 0);

    Component *rref = add_comp(circuit, COMP_RESISTOR, x + 60, y - 60, 90);
    rref->props.resistor.resistance = 10000.0;

    Component *q1 = add_comp(circuit, COMP_NPN_BJT, x + 80, y, 0);
    q1->props.bjt.bf = 100;

    Component *q2 = add_comp(circuit, COMP_NPN_BJT, x + 160, y, 0);
    q2->props.bjt.bf = 100;

    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 180, y - 60, 90);
    rload->props.resistor.resistance = 1000.0;

    Component *label = add_comp(circuit, COMP_TEXT, x + 60, y - 120, 0);
    strncpy(label->props.text.text, "Current Mirror", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // Get positions
    float vcc_pos_x, vcc_pos_y, vcc_neg_x, vcc_neg_y;
    component_get_terminal_pos(vcc, 0, &vcc_pos_x, &vcc_pos_y);
    component_get_terminal_pos(vcc, 1, &vcc_neg_x, &vcc_neg_y);
    float gnd_x, gnd_y;
    component_get_terminal_pos(gnd, 0, &gnd_x, &gnd_y);
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
    float base2_x, base2_y, coll2_x, coll2_y, emit2_x, emit2_y;
    component_get_terminal_pos(q2, 0, &base2_x, &base2_y);
    component_get_terminal_pos(q2, 1, &coll2_x, &coll2_y);
    component_get_terminal_pos(q2, 2, &emit2_x, &emit2_y);

    int gnd_node = circuit_find_or_create_node(circuit, gnd_x, gnd_y, 5.0f);

    // Vcc routing
    int vcc_node = circuit_find_or_create_node(circuit, vcc_pos_x, vcc_pos_y, 5.0f);
    vcc->node_ids[0] = vcc_node;
    circuit_add_wire(circuit, vcc_node, circuit_find_or_create_node(circuit, rref_top_x, vcc_pos_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rref_top_x, vcc_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, rref_top_x, rref_top_y, 5.0f));
    rref->node_ids[0] = vcc_node;
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rref_top_x, vcc_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, rload_top_x, vcc_pos_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rload_top_x, vcc_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f));
    rload->node_ids[0] = vcc_node;

    // Vcc- to ground
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vcc_neg_x, vcc_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, vcc_neg_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vcc_neg_x, gnd_y, 5.0f), gnd_node);
    vcc->node_ids[1] = gnd_node;

    // Diode-connect Q1 - all wires use Manhattan routing (no diagonals)
    int base_node = circuit_find_or_create_node(circuit, base1_x, base1_y, 5.0f);

    // Rref bottom to collector: go RIGHT first, then DOWN to collector level
    // rref_bot is above and left of coll1, so: right to coll1_x, then down to coll1_y
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rref_bot_x, rref_bot_y, 5.0f),
                     circuit_find_or_create_node(circuit, coll1_x, rref_bot_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, coll1_x, rref_bot_y, 5.0f),
                     circuit_find_or_create_node(circuit, coll1_x, coll1_y, 5.0f));

    // Collector to base: go LEFT to base1_x (above transistor), then DOWN to base
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, coll1_x, coll1_y, 5.0f),
                     circuit_find_or_create_node(circuit, base1_x, coll1_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, base1_x, coll1_y, 5.0f), base_node);
    rref->node_ids[1] = base_node;
    q1->node_ids[0] = base_node;
    q1->node_ids[1] = base_node;

    // Q2 base to Q1 base - route BELOW transistors to avoid crossing bodies
    // Go down from base1, across below emitters, then up to base2
    float base_bus_y = emit1_y + 40;  // Route well below emitter level
    circuit_add_wire(circuit, base_node, circuit_find_or_create_node(circuit, base1_x, base_bus_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, base1_x, base_bus_y, 5.0f),
                     circuit_find_or_create_node(circuit, base2_x, base_bus_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, base2_x, base_bus_y, 5.0f),
                     circuit_find_or_create_node(circuit, base2_x, base2_y, 5.0f));
    q2->node_ids[0] = base_node;

    // Q1 emitter to ground
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, emit1_x, emit1_y, 5.0f),
                     circuit_find_or_create_node(circuit, emit1_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, emit1_x, gnd_y, 5.0f), gnd_node);
    q1->node_ids[2] = gnd_node;

    // Q2 emitter to ground
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, emit2_x, emit2_y, 5.0f),
                     circuit_find_or_create_node(circuit, emit2_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, emit2_x, gnd_y, 5.0f), gnd_node);
    q2->node_ids[2] = gnd_node;

    // Rload to Q2 collector
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rload_bot_x, rload_bot_y, 5.0f),
                     circuit_find_or_create_node(circuit, coll2_x, coll2_y, 5.0f));
    int coll2_node = circuit_find_or_create_node(circuit, coll2_x, coll2_y, 5.0f);
    rload->node_ids[1] = coll2_node;
    q2->node_ids[1] = coll2_node;

    return 8;
}

// === PUSH-PULL OUTPUT ===
static int place_push_pull(Circuit *circuit, float x, float y) {
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x - 60, y - 80, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 12.0;

    Component *vee = add_comp(circuit, COMP_DC_VOLTAGE, x - 60, y + 120, 0);
    vee->props.dc_voltage.voltage = 12.0;

    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x - 140, y + 20, 0);
    vin->props.ac_voltage.amplitude = 5.0;
    vin->props.ac_voltage.frequency = 1000.0;

    // Single ground at bottom-left
    Component *gnd = add_comp(circuit, COMP_GROUND, x - 60, y + 200, 0);

    Component *q1 = add_comp(circuit, COMP_NPN_BJT, x + 60, y - 20, 0);
    q1->props.bjt.bf = 100;

    Component *q2 = add_comp(circuit, COMP_PNP_BJT, x + 60, y + 60, 0);
    q2->props.bjt.bf = 100;

    // Position resistor so its bottom terminal aligns with Q2 emitter level
    // This avoids wires crossing through the resistor body
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 140, y + 60, 90);
    rload->props.resistor.resistance = 100.0;

    Component *label = add_comp(circuit, COMP_TEXT, x + 20, y - 120, 0);
    strncpy(label->props.text.text, "Push-Pull Output", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // Get positions
    float vcc_pos_x, vcc_pos_y, vcc_neg_x, vcc_neg_y;
    component_get_terminal_pos(vcc, 0, &vcc_pos_x, &vcc_pos_y);
    component_get_terminal_pos(vcc, 1, &vcc_neg_x, &vcc_neg_y);
    float vee_pos_x, vee_pos_y, vee_neg_x, vee_neg_y;
    component_get_terminal_pos(vee, 0, &vee_pos_x, &vee_pos_y);
    component_get_terminal_pos(vee, 1, &vee_neg_x, &vee_neg_y);
    float vin_pos_x, vin_pos_y, vin_neg_x, vin_neg_y;
    component_get_terminal_pos(vin, 0, &vin_pos_x, &vin_pos_y);
    component_get_terminal_pos(vin, 1, &vin_neg_x, &vin_neg_y);
    float gnd_x, gnd_y;
    component_get_terminal_pos(gnd, 0, &gnd_x, &gnd_y);
    float base1_x, base1_y, coll1_x, coll1_y, emit1_x, emit1_y;
    component_get_terminal_pos(q1, 0, &base1_x, &base1_y);
    component_get_terminal_pos(q1, 1, &coll1_x, &coll1_y);
    component_get_terminal_pos(q1, 2, &emit1_x, &emit1_y);
    float base2_x, base2_y, coll2_x, coll2_y, emit2_x, emit2_y;
    component_get_terminal_pos(q2, 0, &base2_x, &base2_y);
    component_get_terminal_pos(q2, 1, &coll2_x, &coll2_y);
    component_get_terminal_pos(q2, 2, &emit2_x, &emit2_y);
    float rload_top_x, rload_top_y, rload_bot_x, rload_bot_y;
    component_get_terminal_pos(rload, 0, &rload_top_x, &rload_top_y);
    component_get_terminal_pos(rload, 1, &rload_bot_x, &rload_bot_y);

    int gnd_node = circuit_find_or_create_node(circuit, gnd_x, gnd_y, 5.0f);

    // Vcc to Q1 collector
    int vcc_node = circuit_find_or_create_node(circuit, vcc_pos_x, vcc_pos_y, 5.0f);
    vcc->node_ids[0] = vcc_node;
    circuit_add_wire(circuit, vcc_node, circuit_find_or_create_node(circuit, coll1_x, vcc_pos_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, coll1_x, vcc_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, coll1_x, coll1_y, 5.0f));
    q1->node_ids[1] = vcc_node;

    // Vcc- to ground: Route LEFT first to avoid passing through Vee, then DOWN
    float gnd_bus_x = vcc_neg_x - 40;  // Route to the left of voltage sources
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vcc_neg_x, vcc_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, gnd_bus_x, vcc_neg_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, gnd_bus_x, vcc_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, gnd_bus_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, gnd_bus_x, gnd_y, 5.0f), gnd_node);
    vcc->node_ids[1] = gnd_node;

    // Vee (negative supply) - Vee+ to Q2 collector
    int vee_node = circuit_find_or_create_node(circuit, vee_pos_x, vee_pos_y, 5.0f);
    vee->node_ids[0] = vee_node;
    circuit_add_wire(circuit, vee_node, circuit_find_or_create_node(circuit, coll2_x, vee_pos_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, coll2_x, vee_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, coll2_x, coll2_y, 5.0f));
    q2->node_ids[1] = vee_node;

    // Vee- to ground: Route to gnd_bus_x, then down
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vee_neg_x, vee_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, gnd_bus_x, vee_neg_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, gnd_bus_x, vee_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, gnd_bus_x, gnd_y, 5.0f));
    vee->node_ids[1] = gnd_node;

    // Vin to bases: Route RIGHT to base1_x, then split up and down to bases
    int base_node = circuit_find_or_create_node(circuit, vin_pos_x, vin_pos_y, 5.0f);
    vin->node_ids[0] = base_node;
    circuit_add_wire(circuit, base_node, circuit_find_or_create_node(circuit, base1_x, vin_pos_y, 5.0f));
    // Up to Q1 base
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, base1_x, vin_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, base1_x, base1_y, 5.0f));
    // Down to Q2 base
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, base1_x, vin_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, base2_x, vin_pos_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, base2_x, vin_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, base2_x, base2_y, 5.0f));
    q1->node_ids[0] = base_node;
    q2->node_ids[0] = base_node;

    // Vin- to ground: Route to gnd_bus_x
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vin_neg_x, vin_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, gnd_bus_x, vin_neg_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, gnd_bus_x, vin_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, gnd_bus_x, gnd_y, 5.0f));
    vin->node_ids[1] = gnd_node;

    // Emitters to output/load - use Manhattan routing
    int out_node = circuit_find_or_create_node(circuit, emit1_x, emit1_y, 5.0f);
    // Connect emit1 down to emit2
    circuit_add_wire(circuit, out_node, circuit_find_or_create_node(circuit, emit2_x, emit2_y, 5.0f));
    // Connect emit2 to rload_bot using L-shape: DOWN first to rload_bot_y, then RIGHT
    // This routes BELOW the resistor body instead of through it
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, emit2_x, emit2_y, 5.0f),
                     circuit_find_or_create_node(circuit, emit2_x, rload_bot_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, emit2_x, rload_bot_y, 5.0f),
                     circuit_find_or_create_node(circuit, rload_bot_x, rload_bot_y, 5.0f));
    q1->node_ids[2] = out_node;
    q2->node_ids[2] = out_node;
    rload->node_ids[1] = out_node;

    // Load top to ground - route RIGHT of resistor to avoid passing through it
    float rload_wire_x = rload_top_x + 30;
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f),
                     circuit_find_or_create_node(circuit, rload_wire_x, rload_top_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rload_wire_x, rload_top_y, 5.0f),
                     circuit_find_or_create_node(circuit, rload_wire_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rload_wire_x, gnd_y, 5.0f), gnd_node);
    rload->node_ids[0] = gnd_node;

    return 9;
}

// === CMOS INVERTER ===
static int place_cmos_inverter(Circuit *circuit, float x, float y) {
    Component *vdd = add_comp(circuit, COMP_DC_VOLTAGE, x - 40, y - 60, 0);
    if (!vdd) return 0;
    vdd->props.dc_voltage.voltage = 5.0;

    Component *vin = add_comp(circuit, COMP_SQUARE_WAVE, x - 120, y + 20, 0);
    vin->props.square_wave.amplitude = 2.5;
    vin->props.square_wave.offset = 2.5;
    vin->props.square_wave.frequency = 1000.0;

    // Single ground at bottom
    Component *gnd = add_comp(circuit, COMP_GROUND, x + 60, y + 120, 0);

    Component *q1 = add_comp(circuit, COMP_PMOS, x + 60, y - 20, 0);
    q1->props.mosfet.vth = -1.0;
    q1->props.mosfet.kp = 50e-6;

    Component *q2 = add_comp(circuit, COMP_NMOS, x + 60, y + 40, 0);
    q2->props.mosfet.vth = 1.0;
    q2->props.mosfet.kp = 110e-6;

    Component *cload = add_comp(circuit, COMP_CAPACITOR, x + 140, y + 20, 90);
    cload->props.capacitor.capacitance = 100e-12;

    Component *label = add_comp(circuit, COMP_TEXT, x + 20, y - 100, 0);
    strncpy(label->props.text.text, "CMOS Inverter", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // Get positions
    float vdd_pos_x, vdd_pos_y, vdd_neg_x, vdd_neg_y;
    component_get_terminal_pos(vdd, 0, &vdd_pos_x, &vdd_pos_y);
    component_get_terminal_pos(vdd, 1, &vdd_neg_x, &vdd_neg_y);
    float vin_pos_x, vin_pos_y, vin_neg_x, vin_neg_y;
    component_get_terminal_pos(vin, 0, &vin_pos_x, &vin_pos_y);
    component_get_terminal_pos(vin, 1, &vin_neg_x, &vin_neg_y);
    float gnd_x, gnd_y;
    component_get_terminal_pos(gnd, 0, &gnd_x, &gnd_y);
    float gate1_x, gate1_y, drain1_x, drain1_y, source1_x, source1_y;
    component_get_terminal_pos(q1, 0, &gate1_x, &gate1_y);
    component_get_terminal_pos(q1, 1, &drain1_x, &drain1_y);
    component_get_terminal_pos(q1, 2, &source1_x, &source1_y);
    float gate2_x, gate2_y, drain2_x, drain2_y, source2_x, source2_y;
    component_get_terminal_pos(q2, 0, &gate2_x, &gate2_y);
    component_get_terminal_pos(q2, 1, &drain2_x, &drain2_y);
    component_get_terminal_pos(q2, 2, &source2_x, &source2_y);
    float cload_top_x, cload_top_y, cload_bot_x, cload_bot_y;
    component_get_terminal_pos(cload, 0, &cload_top_x, &cload_top_y);
    component_get_terminal_pos(cload, 1, &cload_bot_x, &cload_bot_y);

    int gnd_node = circuit_find_or_create_node(circuit, gnd_x, gnd_y, 5.0f);

    // Vdd to PMOS source
    int vdd_node = circuit_find_or_create_node(circuit, vdd_pos_x, vdd_pos_y, 5.0f);
    vdd->node_ids[0] = vdd_node;
    circuit_add_wire(circuit, vdd_node, circuit_find_or_create_node(circuit, source1_x, vdd_pos_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, source1_x, vdd_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, source1_x, source1_y, 5.0f));
    q1->node_ids[2] = vdd_node;

    // Vdd- to ground
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vdd_neg_x, vdd_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, vdd_neg_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vdd_neg_x, gnd_y, 5.0f), gnd_node);
    vdd->node_ids[1] = gnd_node;

    // Vin to gates
    int gate_node = circuit_find_or_create_node(circuit, vin_pos_x, vin_pos_y, 5.0f);
    vin->node_ids[0] = gate_node;
    circuit_add_wire(circuit, gate_node, circuit_find_or_create_node(circuit, gate1_x, vin_pos_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, gate1_x, vin_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, gate1_x, gate1_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, gate1_x, vin_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, gate2_x, gate2_y, 5.0f));
    q1->node_ids[0] = gate_node;
    q2->node_ids[0] = gate_node;

    // Vin- to ground
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vin_neg_x, vin_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, vin_neg_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vin_neg_x, gnd_y, 5.0f), gnd_node);
    vin->node_ids[1] = gnd_node;

    // Output node (drains)
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
                     circuit_find_or_create_node(circuit, source2_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, source2_x, gnd_y, 5.0f), gnd_node);
    q2->node_ids[2] = gnd_node;

    // Load cap to ground
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cload_bot_x, cload_bot_y, 5.0f),
                     circuit_find_or_create_node(circuit, cload_bot_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cload_bot_x, gnd_y, 5.0f), gnd_node);
    cload->node_ids[1] = gnd_node;

    return 8;
}

// === INTEGRATOR ===
static int place_integrator(Circuit *circuit, float x, float y) {
    Component *vsrc = add_comp(circuit, COMP_SQUARE_WAVE, x - 40, y + 40, 0);
    if (!vsrc) return 0;
    vsrc->props.square_wave.amplitude = 1.0;
    vsrc->props.square_wave.frequency = 100.0;
    vsrc->props.square_wave.offset = 0.0;

    // Single ground at bottom-left
    Component *gnd = add_comp(circuit, COMP_GROUND, x - 40, y + 120, 0);

    Component *ri = add_comp(circuit, COMP_RESISTOR, x + 60, y, 0);
    ri->props.resistor.resistance = 10000.0;

    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 180, y + 20, 0);

    Component *cf = add_comp(circuit, COMP_CAPACITOR, x + 180, y - 40, 0);
    cf->props.capacitor.capacitance = 100e-9;

    Component *label = add_comp(circuit, COMP_TEXT, x + 80, y - 80, 0);
    strncpy(label->props.text.text, "Op-Amp Integrator", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // Get positions
    float vsrc_pos_x, vsrc_pos_y, vsrc_neg_x, vsrc_neg_y;
    component_get_terminal_pos(vsrc, 0, &vsrc_pos_x, &vsrc_pos_y);
    component_get_terminal_pos(vsrc, 1, &vsrc_neg_x, &vsrc_neg_y);
    float gnd_x, gnd_y;
    component_get_terminal_pos(gnd, 0, &gnd_x, &gnd_y);
    float ri_in_x, ri_in_y, ri_out_x, ri_out_y;
    component_get_terminal_pos(ri, 0, &ri_in_x, &ri_in_y);
    component_get_terminal_pos(ri, 1, &ri_out_x, &ri_out_y);
    float inv_x, inv_y, noninv_x, noninv_y, out_x, out_y;
    component_get_terminal_pos(opamp, 0, &inv_x, &inv_y);
    component_get_terminal_pos(opamp, 1, &noninv_x, &noninv_y);
    component_get_terminal_pos(opamp, 2, &out_x, &out_y);
    float cf_in_x, cf_in_y, cf_out_x, cf_out_y;
    component_get_terminal_pos(cf, 0, &cf_in_x, &cf_in_y);
    component_get_terminal_pos(cf, 1, &cf_out_x, &cf_out_y);

    int gnd_node = circuit_find_or_create_node(circuit, gnd_x, gnd_y, 5.0f);

    // Vsrc to Ri
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vsrc_pos_x, vsrc_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, ri_in_x, vsrc_pos_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, ri_in_x, vsrc_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, ri_in_x, ri_in_y, 5.0f));
    int vin_node = circuit_find_or_create_node(circuit, vsrc_pos_x, vsrc_pos_y, 5.0f);
    vsrc->node_ids[0] = vin_node;
    ri->node_ids[0] = vin_node;

    // Vsrc- to ground
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vsrc_neg_x, vsrc_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, vsrc_neg_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vsrc_neg_x, gnd_y, 5.0f), gnd_node);
    vsrc->node_ids[1] = gnd_node;

    // Op-amp + to ground
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, noninv_x, noninv_y, 5.0f),
                     circuit_find_or_create_node(circuit, noninv_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, noninv_x, gnd_y, 5.0f), gnd_node);
    opamp->node_ids[1] = gnd_node;

    // Ri to - input
    int inv_node = circuit_find_or_create_node(circuit, inv_x, inv_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, ri_out_x, ri_out_y, 5.0f), inv_node);
    ri->node_ids[1] = inv_node;
    opamp->node_ids[0] = inv_node;

    // Feedback cap
    circuit_add_wire(circuit, inv_node, circuit_find_or_create_node(circuit, inv_x, cf_in_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, inv_x, cf_in_y, 5.0f),
                     circuit_find_or_create_node(circuit, cf_in_x, cf_in_y, 5.0f));
    cf->node_ids[0] = inv_node;

    int out_node = circuit_find_or_create_node(circuit, out_x, out_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cf_out_x, cf_out_y, 5.0f),
                     circuit_find_or_create_node(circuit, out_x, cf_out_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, out_x, cf_out_y, 5.0f), out_node);
    cf->node_ids[1] = out_node;
    opamp->node_ids[2] = out_node;

    return 6;
}

// === DIFFERENTIATOR ===
static int place_differentiator(Circuit *circuit, float x, float y) {
    Component *vsrc = add_comp(circuit, COMP_TRIANGLE_WAVE, x - 40, y + 40, 0);
    if (!vsrc) return 0;
    vsrc->props.triangle_wave.amplitude = 1.0;
    vsrc->props.triangle_wave.frequency = 100.0;

    Component *gnd = add_comp(circuit, COMP_GROUND, x - 40, y + 120, 0);

    Component *ci = add_comp(circuit, COMP_CAPACITOR, x + 60, y, 0);
    ci->props.capacitor.capacitance = 100e-9;

    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 180, y + 20, 0);

    Component *rf = add_comp(circuit, COMP_RESISTOR, x + 180, y - 40, 0);
    rf->props.resistor.resistance = 10000.0;

    Component *label = add_comp(circuit, COMP_TEXT, x + 60, y - 80, 0);
    strncpy(label->props.text.text, "Op-Amp Differentiator", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // Get positions
    float vsrc_pos_x, vsrc_pos_y, vsrc_neg_x, vsrc_neg_y;
    component_get_terminal_pos(vsrc, 0, &vsrc_pos_x, &vsrc_pos_y);
    component_get_terminal_pos(vsrc, 1, &vsrc_neg_x, &vsrc_neg_y);
    float gnd_x, gnd_y;
    component_get_terminal_pos(gnd, 0, &gnd_x, &gnd_y);
    float ci_in_x, ci_in_y, ci_out_x, ci_out_y;
    component_get_terminal_pos(ci, 0, &ci_in_x, &ci_in_y);
    component_get_terminal_pos(ci, 1, &ci_out_x, &ci_out_y);
    float inv_x, inv_y, noninv_x, noninv_y, out_x, out_y;
    component_get_terminal_pos(opamp, 0, &inv_x, &inv_y);
    component_get_terminal_pos(opamp, 1, &noninv_x, &noninv_y);
    component_get_terminal_pos(opamp, 2, &out_x, &out_y);
    float rf_in_x, rf_in_y, rf_out_x, rf_out_y;
    component_get_terminal_pos(rf, 0, &rf_in_x, &rf_in_y);
    component_get_terminal_pos(rf, 1, &rf_out_x, &rf_out_y);

    int gnd_node = circuit_find_or_create_node(circuit, gnd_x, gnd_y, 5.0f);

    // Vsrc to Ci
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vsrc_pos_x, vsrc_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, ci_in_x, vsrc_pos_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, ci_in_x, vsrc_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, ci_in_x, ci_in_y, 5.0f));
    int vin_node = circuit_find_or_create_node(circuit, vsrc_pos_x, vsrc_pos_y, 5.0f);
    vsrc->node_ids[0] = vin_node;
    ci->node_ids[0] = vin_node;

    // Vsrc- to ground
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vsrc_neg_x, vsrc_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, vsrc_neg_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vsrc_neg_x, gnd_y, 5.0f), gnd_node);
    vsrc->node_ids[1] = gnd_node;

    // Op-amp + to ground
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, noninv_x, noninv_y, 5.0f),
                     circuit_find_or_create_node(circuit, noninv_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, noninv_x, gnd_y, 5.0f), gnd_node);
    opamp->node_ids[1] = gnd_node;

    // Ci to - input
    int inv_node = circuit_find_or_create_node(circuit, inv_x, inv_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, ci_out_x, ci_out_y, 5.0f), inv_node);
    ci->node_ids[1] = inv_node;
    opamp->node_ids[0] = inv_node;

    // Feedback resistor
    circuit_add_wire(circuit, inv_node, circuit_find_or_create_node(circuit, inv_x, rf_in_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, inv_x, rf_in_y, 5.0f),
                     circuit_find_or_create_node(circuit, rf_in_x, rf_in_y, 5.0f));
    rf->node_ids[0] = inv_node;

    int out_node = circuit_find_or_create_node(circuit, out_x, out_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rf_out_x, rf_out_y, 5.0f),
                     circuit_find_or_create_node(circuit, out_x, rf_out_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, out_x, rf_out_y, 5.0f), out_node);
    rf->node_ids[1] = out_node;
    opamp->node_ids[2] = out_node;

    return 6;
}

// === SUMMING AMP ===
// Redesigned with horizontal layout to avoid wire overlaps
static int place_summing_amp(Circuit *circuit, float x, float y) {
    // Three voltage sources - shifted LEFT by 20 so V+ terminals align with resistor centers
    // This creates straight vertical V+ to R connections with no diagonal wires
    Component *v1 = add_comp(circuit, COMP_DC_VOLTAGE, x - 100, y - 60, 0);
    if (!v1) return 0;
    v1->props.dc_voltage.voltage = 1.0;

    Component *v2 = add_comp(circuit, COMP_DC_VOLTAGE, x - 20, y - 60, 0);
    v2->props.dc_voltage.voltage = 2.0;

    Component *v3 = add_comp(circuit, COMP_DC_VOLTAGE, x + 60, y - 60, 0);
    v3->props.dc_voltage.voltage = 3.0;

    // Ground on left side - cleaner routing for V- terminals
    Component *gnd = add_comp(circuit, COMP_GROUND, x - 140, y + 100, 0);

    // Input resistors (vertical) - aligned with V+ terminals
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x - 80, y + 20, 90);
    r1->props.resistor.resistance = 10000.0;
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x, y + 20, 90);
    r2->props.resistor.resistance = 10000.0;
    Component *r3 = add_comp(circuit, COMP_RESISTOR, x + 80, y + 20, 90);
    r3->props.resistor.resistance = 10000.0;

    // Op-amp
    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 180, y + 80, 0);

    // Feedback resistor
    Component *rf = add_comp(circuit, COMP_RESISTOR, x + 180, y + 20, 0);
    rf->props.resistor.resistance = 10000.0;

    Component *label = add_comp(circuit, COMP_TEXT, x + 60, y - 100, 0);
    strncpy(label->props.text.text, "Summing Amp (Vout = -(V1+V2+V3))", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // Get positions
    float v1_pos_x, v1_pos_y, v1_neg_x, v1_neg_y;
    component_get_terminal_pos(v1, 0, &v1_pos_x, &v1_pos_y);
    component_get_terminal_pos(v1, 1, &v1_neg_x, &v1_neg_y);
    float v2_pos_x, v2_pos_y, v2_neg_x, v2_neg_y;
    component_get_terminal_pos(v2, 0, &v2_pos_x, &v2_pos_y);
    component_get_terminal_pos(v2, 1, &v2_neg_x, &v2_neg_y);
    float v3_pos_x, v3_pos_y, v3_neg_x, v3_neg_y;
    component_get_terminal_pos(v3, 0, &v3_pos_x, &v3_pos_y);
    component_get_terminal_pos(v3, 1, &v3_neg_x, &v3_neg_y);
    float gnd_x, gnd_y;
    component_get_terminal_pos(gnd, 0, &gnd_x, &gnd_y);
    float r1_top_x, r1_top_y, r1_bot_x, r1_bot_y;
    component_get_terminal_pos(r1, 0, &r1_top_x, &r1_top_y);
    component_get_terminal_pos(r1, 1, &r1_bot_x, &r1_bot_y);
    float r2_top_x, r2_top_y, r2_bot_x, r2_bot_y;
    component_get_terminal_pos(r2, 0, &r2_top_x, &r2_top_y);
    component_get_terminal_pos(r2, 1, &r2_bot_x, &r2_bot_y);
    float r3_top_x, r3_top_y, r3_bot_x, r3_bot_y;
    component_get_terminal_pos(r3, 0, &r3_top_x, &r3_top_y);
    component_get_terminal_pos(r3, 1, &r3_bot_x, &r3_bot_y);
    float inv_x, inv_y, noninv_x, noninv_y, out_x, out_y;
    component_get_terminal_pos(opamp, 0, &inv_x, &inv_y);
    component_get_terminal_pos(opamp, 1, &noninv_x, &noninv_y);
    component_get_terminal_pos(opamp, 2, &out_x, &out_y);
    float rf_in_x, rf_in_y, rf_out_x, rf_out_y;
    component_get_terminal_pos(rf, 0, &rf_in_x, &rf_in_y);
    component_get_terminal_pos(rf, 1, &rf_out_x, &rf_out_y);

    int gnd_node = circuit_find_or_create_node(circuit, gnd_x, gnd_y, 5.0f);

    // V+ routing: Go RIGHT first to exit voltage source column, then DOWN to resistor
    // This avoids the vertical wire passing through the voltage source body
    // V- routing: Go DOWN first to ground bus level (below resistors), then LEFT to ground
    // This avoids horizontal wires crossing through resistor columns

    // V1+ to R1: RIGHT to exit voltage source, DOWN to R1 top
    int v1_node = circuit_find_or_create_node(circuit, v1_pos_x, v1_pos_y, 5.0f);
    v1->node_ids[0] = v1_node;
    r1->node_ids[0] = v1_node;
    // Go RIGHT first to r1_top_x (which aligns with resistor), then DOWN
    circuit_add_wire(circuit, v1_node, circuit_find_or_create_node(circuit, r1_top_x, v1_pos_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r1_top_x, v1_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, r1_top_x, r1_top_y, 5.0f));

    // V1- to ground: DOWN to ground bus level, then LEFT to gnd
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, v1_neg_x, v1_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, v1_neg_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, v1_neg_x, gnd_y, 5.0f), gnd_node);
    v1->node_ids[1] = gnd_node;

    // V2+ to R2: RIGHT to exit voltage source, DOWN to R2 top
    int v2_node = circuit_find_or_create_node(circuit, v2_pos_x, v2_pos_y, 5.0f);
    v2->node_ids[0] = v2_node;
    r2->node_ids[0] = v2_node;
    circuit_add_wire(circuit, v2_node, circuit_find_or_create_node(circuit, r2_top_x, v2_pos_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r2_top_x, v2_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, r2_top_x, r2_top_y, 5.0f));

    // V2- to ground: DOWN to ground bus level, then LEFT to gnd
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, v2_neg_x, v2_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, v2_neg_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, v2_neg_x, gnd_y, 5.0f), gnd_node);
    v2->node_ids[1] = gnd_node;

    // V3+ to R3: RIGHT to exit voltage source, DOWN to R3 top
    int v3_node = circuit_find_or_create_node(circuit, v3_pos_x, v3_pos_y, 5.0f);
    v3->node_ids[0] = v3_node;
    r3->node_ids[0] = v3_node;
    circuit_add_wire(circuit, v3_node, circuit_find_or_create_node(circuit, r3_top_x, v3_pos_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r3_top_x, v3_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, r3_top_x, r3_top_y, 5.0f));

    // V3- to ground: DOWN to ground bus level, then LEFT to gnd
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, v3_neg_x, v3_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, v3_neg_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, v3_neg_x, gnd_y, 5.0f), gnd_node);
    v3->node_ids[1] = gnd_node;

    // Op-amp + to ground
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, noninv_x, noninv_y, 5.0f),
                     circuit_find_or_create_node(circuit, noninv_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, noninv_x, gnd_y, 5.0f), gnd_node);
    opamp->node_ids[1] = gnd_node;

    // All input resistors to - input via horizontal bus
    int inv_node = circuit_find_or_create_node(circuit, inv_x, inv_y, 5.0f);
    opamp->node_ids[0] = inv_node;
    float bus_y = r1_bot_y;
    // R1 to bus
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r1_bot_x, r1_bot_y, 5.0f),
                     circuit_find_or_create_node(circuit, r1_bot_x, bus_y, 5.0f));
    r1->node_ids[1] = inv_node;
    // R2 to bus
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r2_bot_x, r2_bot_y, 5.0f),
                     circuit_find_or_create_node(circuit, r2_bot_x, bus_y, 5.0f));
    r2->node_ids[1] = inv_node;
    // R3 to bus
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r3_bot_x, r3_bot_y, 5.0f),
                     circuit_find_or_create_node(circuit, r3_bot_x, bus_y, 5.0f));
    r3->node_ids[1] = inv_node;
    // Connect bus segments
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r1_bot_x, bus_y, 5.0f),
                     circuit_find_or_create_node(circuit, r2_bot_x, bus_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r2_bot_x, bus_y, 5.0f),
                     circuit_find_or_create_node(circuit, r3_bot_x, bus_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r3_bot_x, bus_y, 5.0f),
                     circuit_find_or_create_node(circuit, inv_x, bus_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, inv_x, bus_y, 5.0f), inv_node);

    // Feedback resistor
    circuit_add_wire(circuit, inv_node, circuit_find_or_create_node(circuit, inv_x, rf_in_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, inv_x, rf_in_y, 5.0f),
                     circuit_find_or_create_node(circuit, rf_in_x, rf_in_y, 5.0f));
    rf->node_ids[0] = inv_node;

    int out_node = circuit_find_or_create_node(circuit, out_x, out_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rf_out_x, rf_out_y, 5.0f),
                     circuit_find_or_create_node(circuit, out_x, rf_out_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, out_x, rf_out_y, 5.0f), out_node);
    rf->node_ids[1] = out_node;
    opamp->node_ids[2] = out_node;

    return 11;
}

// === COMPARATOR ===
static int place_comparator(Circuit *circuit, float x, float y) {
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x - 60, y - 60, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 10.0;

    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x - 140, y + 80, 0);
    vin->props.ac_voltage.amplitude = 6.0;
    vin->props.ac_voltage.offset = 5.0;
    vin->props.ac_voltage.frequency = 100.0;

    // Single ground at bottom
    Component *gnd = add_comp(circuit, COMP_GROUND, x + 40, y + 160, 0);

    // Voltage divider for reference
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x, y - 40, 90);
    r1->props.resistor.resistance = 10000.0;
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x, y + 40, 90);
    r2->props.resistor.resistance = 10000.0;

    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 100, y + 40, 0);

    Component *rpu = add_comp(circuit, COMP_RESISTOR, x + 180, y - 20, 90);
    rpu->props.resistor.resistance = 10000.0;

    Component *label = add_comp(circuit, COMP_TEXT, x + 20, y - 100, 0);
    strncpy(label->props.text.text, "Voltage Comparator", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // Get positions
    float vcc_pos_x, vcc_pos_y, vcc_neg_x, vcc_neg_y;
    component_get_terminal_pos(vcc, 0, &vcc_pos_x, &vcc_pos_y);
    component_get_terminal_pos(vcc, 1, &vcc_neg_x, &vcc_neg_y);
    float vin_pos_x, vin_pos_y, vin_neg_x, vin_neg_y;
    component_get_terminal_pos(vin, 0, &vin_pos_x, &vin_pos_y);
    component_get_terminal_pos(vin, 1, &vin_neg_x, &vin_neg_y);
    float gnd_x, gnd_y;
    component_get_terminal_pos(gnd, 0, &gnd_x, &gnd_y);
    float r1_top_x, r1_top_y, r1_bot_x, r1_bot_y;
    component_get_terminal_pos(r1, 0, &r1_top_x, &r1_top_y);
    component_get_terminal_pos(r1, 1, &r1_bot_x, &r1_bot_y);
    float r2_top_x, r2_top_y, r2_bot_x, r2_bot_y;
    component_get_terminal_pos(r2, 0, &r2_top_x, &r2_top_y);
    component_get_terminal_pos(r2, 1, &r2_bot_x, &r2_bot_y);
    float inv_x, inv_y, noninv_x, noninv_y, out_x, out_y;
    component_get_terminal_pos(opamp, 0, &inv_x, &inv_y);
    component_get_terminal_pos(opamp, 1, &noninv_x, &noninv_y);
    component_get_terminal_pos(opamp, 2, &out_x, &out_y);
    float rpu_top_x, rpu_top_y, rpu_bot_x, rpu_bot_y;
    component_get_terminal_pos(rpu, 0, &rpu_top_x, &rpu_top_y);
    component_get_terminal_pos(rpu, 1, &rpu_bot_x, &rpu_bot_y);

    int gnd_node = circuit_find_or_create_node(circuit, gnd_x, gnd_y, 5.0f);

    // Vcc routing
    int vcc_node = circuit_find_or_create_node(circuit, vcc_pos_x, vcc_pos_y, 5.0f);
    vcc->node_ids[0] = vcc_node;
    circuit_add_wire(circuit, vcc_node, circuit_find_or_create_node(circuit, r1_top_x, vcc_pos_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r1_top_x, vcc_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, r1_top_x, r1_top_y, 5.0f));
    r1->node_ids[0] = vcc_node;
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r1_top_x, vcc_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, rpu_top_x, vcc_pos_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rpu_top_x, vcc_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, rpu_top_x, rpu_top_y, 5.0f));
    rpu->node_ids[0] = vcc_node;

    // Vcc- to ground
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vcc_neg_x, vcc_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, vcc_neg_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vcc_neg_x, gnd_y, 5.0f), gnd_node);
    vcc->node_ids[1] = gnd_node;

    // R1/R2 junction to + input
    int ref_node = circuit_find_or_create_node(circuit, r1_bot_x, r1_bot_y, 5.0f);
    circuit_add_wire(circuit, ref_node, circuit_find_or_create_node(circuit, r2_top_x, r2_top_y, 5.0f));
    r1->node_ids[1] = ref_node;
    r2->node_ids[0] = ref_node;
    circuit_add_wire(circuit, ref_node, circuit_find_or_create_node(circuit, noninv_x, r1_bot_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, noninv_x, r1_bot_y, 5.0f),
                     circuit_find_or_create_node(circuit, noninv_x, noninv_y, 5.0f));
    opamp->node_ids[1] = ref_node;

    // R2 to ground
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r2_bot_x, r2_bot_y, 5.0f),
                     circuit_find_or_create_node(circuit, r2_bot_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, r2_bot_x, gnd_y, 5.0f), gnd_node);
    r2->node_ids[1] = gnd_node;

    // Vin to - input
    int vin_node = circuit_find_or_create_node(circuit, vin_pos_x, vin_pos_y, 5.0f);
    vin->node_ids[0] = vin_node;
    circuit_add_wire(circuit, vin_node, circuit_find_or_create_node(circuit, inv_x, vin_pos_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, inv_x, vin_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, inv_x, inv_y, 5.0f));
    opamp->node_ids[0] = vin_node;

    // Vin- to ground
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vin_neg_x, vin_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, vin_neg_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vin_neg_x, gnd_y, 5.0f), gnd_node);
    vin->node_ids[1] = gnd_node;

    // Output with pull-up
    int out_node = circuit_find_or_create_node(circuit, out_x, out_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rpu_bot_x, rpu_bot_y, 5.0f),
                     circuit_find_or_create_node(circuit, out_x, rpu_bot_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, out_x, rpu_bot_y, 5.0f), out_node);
    rpu->node_ids[1] = out_node;
    opamp->node_ids[2] = out_node;

    return 10;
}

// Full-Wave Bridge Rectifier:
//      +--[D1]--+--[D3]--+
//      |        |        |
// AC~--+        +--+-----+--[C]--+--[R]--+
//      |        |  |     |       |       |
//      +--[D2]--+--[D4]--+       |       |
//                                GND    GND
static int place_fullwave_bridge(Circuit *circuit, float x, float y) {
    // Full-wave bridge rectifier with clean horizontal layout
    // All diodes pointing right in a 2x2 grid pattern
    // Layout:
    //                DC+ Rail
    //                   |
    //   D1 [>|]----+----+----[C]----[R]
    //       |      |              |     |
    //       +------+              |     |
    //       |   (junction)        |     |
    //       +------+              |     |
    //       |      |              |     |
    //   D2 [|<]----+----+----+----+-----+
    //                   |    |    |     |
    //                  GND  DC-  GND   GND
    //
    // D3 and D4 on right side form the other half

    // AC voltage source (60Hz, 12Vpp)
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 50, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 12.0;
    vsrc->props.ac_voltage.frequency = 60.0;

    // Ground for source
    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, y + 130, 0);

    // Bridge diodes - horizontal layout, all pointing right
    // Row 1: D1 and D3 (cathodes to DC+)
    Component *d1 = add_comp(circuit, COMP_DIODE, x + 120, y, 0);      // Top-left
    Component *d3 = add_comp(circuit, COMP_DIODE, x + 220, y, 0);      // Top-right
    // Row 2: D2 and D4 (anodes to DC-)
    Component *d2 = add_comp(circuit, COMP_DIODE, x + 120, y + 100, 180); // Bottom-left, flipped
    Component *d4 = add_comp(circuit, COMP_DIODE, x + 220, y + 100, 180); // Bottom-right, flipped

    // Filter capacitor (electrolytic, 100uF)
    Component *cap = add_comp(circuit, COMP_CAPACITOR_ELEC, x + 340, y + 50, 90);
    cap->props.capacitor_elec.capacitance = 100e-6;

    // Load resistor
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 410, y + 50, 90);
    rload->props.resistor.resistance = 1000.0;

    // Ground for output
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 340, y + 130, 0);
    Component *gnd3 = add_comp(circuit, COMP_GROUND, x + 410, y + 130, 0);

    // Connect source negative to ground
    connect_terminals(circuit, vsrc, 1, gnd1, 0);

    // Get all terminal positions
    float d1_a_x, d1_a_y, d1_k_x, d1_k_y;
    component_get_terminal_pos(d1, 0, &d1_a_x, &d1_a_y);  // anode (left)
    component_get_terminal_pos(d1, 1, &d1_k_x, &d1_k_y);  // cathode (right)

    float d2_a_x, d2_a_y, d2_k_x, d2_k_y;
    component_get_terminal_pos(d2, 0, &d2_a_x, &d2_a_y);  // anode (right after 180 flip)
    component_get_terminal_pos(d2, 1, &d2_k_x, &d2_k_y);  // cathode (left after 180 flip)

    float d3_a_x, d3_a_y, d3_k_x, d3_k_y;
    component_get_terminal_pos(d3, 0, &d3_a_x, &d3_a_y);  // anode (left)
    component_get_terminal_pos(d3, 1, &d3_k_x, &d3_k_y);  // cathode (right)

    float d4_a_x, d4_a_y, d4_k_x, d4_k_y;
    component_get_terminal_pos(d4, 0, &d4_a_x, &d4_a_y);  // anode (right after 180 flip)
    component_get_terminal_pos(d4, 1, &d4_k_x, &d4_k_y);  // cathode (left after 180 flip)

    float vsrc_pos_x, vsrc_pos_y;
    component_get_terminal_pos(vsrc, 0, &vsrc_pos_x, &vsrc_pos_y);

    // Left AC junction at D1 anode x position (aligned with terminal)
    int left_junc = circuit_find_or_create_node(circuit, d1_a_x, y + 50, 5.0f);

    // Wire from source to left junction (route above the source to avoid overlap)
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vsrc_pos_x, vsrc_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, vsrc_pos_x, y - 10, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vsrc_pos_x, y - 10, 5.0f),
                     circuit_find_or_create_node(circuit, d1_a_x, y - 10, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d1_a_x, y - 10, 5.0f), left_junc);
    vsrc->node_ids[0] = left_junc;

    // D1 anode to left junction (wire goes directly to terminal)
    circuit_add_wire(circuit, left_junc, circuit_find_or_create_node(circuit, d1_a_x, y + 50, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d1_a_x, y + 50, 5.0f), d1->node_ids[0]);

    // D2 cathode to left junction (wire goes directly to terminal)
    circuit_add_wire(circuit, left_junc, circuit_find_or_create_node(circuit, d2_k_x, y + 50, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d2_k_x, y + 50, 5.0f), d2->node_ids[1]);

    // Right AC junction at D3 anode x position (aligned with terminal)
    int right_junc = circuit_find_or_create_node(circuit, d3_a_x, y + 50, 5.0f);

    // D3 anode to right junction (direct vertical connection)
    circuit_add_wire(circuit, right_junc, d3->node_ids[0]);

    // D4 cathode to right junction (wire goes directly to terminal)
    circuit_add_wire(circuit, right_junc, circuit_find_or_create_node(circuit, d4_k_x, y + 50, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d4_k_x, y + 50, 5.0f), d4->node_ids[1]);

    // DC+ rail at top (connects D1/D3 cathodes to cap/load)
    int dc_plus = circuit_find_or_create_node(circuit, x + 280, y - 20, 5.0f);

    // D1 cathode to DC+ rail (use existing terminal node)
    circuit_add_wire(circuit, d1->node_ids[1], circuit_find_or_create_node(circuit, d1_k_x, y - 20, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d1_k_x, y - 20, 5.0f), dc_plus);

    // D3 cathode to DC+ rail (use existing terminal node)
    circuit_add_wire(circuit, d3->node_ids[1], circuit_find_or_create_node(circuit, d3_k_x, y - 20, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d3_k_x, y - 20, 5.0f),
                     circuit_find_or_create_node(circuit, d1_k_x, y - 20, 5.0f));

    // DC- rail at bottom (connects D2/D4 anodes to ground)
    int dc_minus = circuit_find_or_create_node(circuit, x + 280, y + 120, 5.0f);

    // D2 anode to DC- rail (use existing terminal node)
    circuit_add_wire(circuit, d2->node_ids[0], circuit_find_or_create_node(circuit, d2_a_x, y + 120, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d2_a_x, y + 120, 5.0f), dc_minus);

    // D4 anode to DC- rail (use existing terminal node)
    circuit_add_wire(circuit, d4->node_ids[0], circuit_find_or_create_node(circuit, d4_a_x, y + 120, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d4_a_x, y + 120, 5.0f),
                     circuit_find_or_create_node(circuit, d2_a_x, y + 120, 5.0f));

    // Connect DC+ to capacitor and load (use existing terminal nodes)
    circuit_add_wire(circuit, dc_plus, circuit_find_or_create_node(circuit, cap->x, y - 20, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cap->x, y - 20, 5.0f), cap->node_ids[0]);

    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cap->x, y - 20, 5.0f),
                     circuit_find_or_create_node(circuit, rload->x, y - 20, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rload->x, y - 20, 5.0f), rload->node_ids[0]);

    // Connect grounds
    connect_terminals(circuit, cap, 1, gnd2, 0);
    connect_terminals(circuit, rload, 1, gnd3, 0);

    // Connect DC- to ground rail
    circuit_add_wire(circuit, dc_minus, circuit_find_or_create_node(circuit, x + 340, y + 120, 5.0f));

    return 11;
}

// Center-Tap Transformer Rectifier with proper spacing
static int place_centertap_rectifier(Circuit *circuit, float x, float y) {
    // Center-tap uses 2 horizontal diodes side by side, both pointing right
    // Layout:
    //  AC ----[TRANS-CT]---- S1 --[>|]D1--+--[C]--[R]--
    //    |         |                     |      |    |
    //   GND       CT--------------------GND    GND  GND
    //              |                     |
    //             S2 --[>|]D2------------+

    // AC voltage source (60Hz)
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 50, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 120.0;
    vsrc->props.ac_voltage.frequency = 60.0;

    // Ground for source
    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, y + 130, 0);

    // Center-tap transformer (10:1 step down)
    Component *trans = add_comp(circuit, COMP_TRANSFORMER_CT, x + 120, y + 50, 0);
    trans->props.transformer.turns_ratio = 0.1;

    // Two horizontal diodes (both pointing right, cathodes to DC+)
    Component *d1 = add_comp(circuit, COMP_DIODE, x + 280, y + 20, 0);   // Top diode
    Component *d2 = add_comp(circuit, COMP_DIODE, x + 280, y + 80, 0);   // Bottom diode

    // Filter capacitor
    Component *cap = add_comp(circuit, COMP_CAPACITOR_ELEC, x + 420, y + 50, 90);
    cap->props.capacitor_elec.capacitance = 470e-6;

    // Load resistor
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 490, y + 50, 90);
    rload->props.resistor.resistance = 1000.0;

    // Grounds
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 420, y + 140, 0);
    Component *gnd3 = add_comp(circuit, COMP_GROUND, x + 490, y + 140, 0);

    // Connect source to ground
    connect_terminals(circuit, vsrc, 1, gnd1, 0);

    // Get terminal positions
    float vsrc_pos_x, vsrc_pos_y;
    component_get_terminal_pos(vsrc, 0, &vsrc_pos_x, &vsrc_pos_y);

    float p1_x, p1_y, p2_x, p2_y;
    component_get_terminal_pos(trans, 0, &p1_x, &p1_y);
    component_get_terminal_pos(trans, 1, &p2_x, &p2_y);

    float s1_x, s1_y, ct_x, ct_y, s2_x, s2_y;
    component_get_terminal_pos(trans, 2, &s1_x, &s1_y);
    component_get_terminal_pos(trans, 3, &ct_x, &ct_y);
    component_get_terminal_pos(trans, 4, &s2_x, &s2_y);

    float d1_a_x, d1_a_y, d1_k_x, d1_k_y;
    component_get_terminal_pos(d1, 0, &d1_a_x, &d1_a_y);
    component_get_terminal_pos(d1, 1, &d1_k_x, &d1_k_y);

    float d2_a_x, d2_a_y, d2_k_x, d2_k_y;
    component_get_terminal_pos(d2, 0, &d2_a_x, &d2_a_y);
    component_get_terminal_pos(d2, 1, &d2_k_x, &d2_k_y);

    // AC source to P1 (route above transformer)
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vsrc_pos_x, vsrc_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, vsrc_pos_x, y - 10, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vsrc_pos_x, y - 10, 5.0f),
                     circuit_find_or_create_node(circuit, p1_x, y - 10, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, p1_x, y - 10, 5.0f),
                     circuit_find_or_create_node(circuit, p1_x, p1_y, 5.0f));
    int prim_top = circuit_find_or_create_node(circuit, p1_x, p1_y, 5.0f);
    vsrc->node_ids[0] = prim_top;
    trans->node_ids[0] = prim_top;

    // Ground to P2 (route below transformer)
    float gnd1_x, gnd1_y;
    component_get_terminal_pos(gnd1, 0, &gnd1_x, &gnd1_y);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, gnd1_x, gnd1_y, 5.0f),
                     circuit_find_or_create_node(circuit, gnd1_x, y + 120, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, gnd1_x, y + 120, 5.0f),
                     circuit_find_or_create_node(circuit, p2_x, y + 120, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, p2_x, y + 120, 5.0f),
                     circuit_find_or_create_node(circuit, p2_x, p2_y, 5.0f));
    int prim_bot = circuit_find_or_create_node(circuit, p2_x, p2_y, 5.0f);
    trans->node_ids[1] = prim_bot;

    // S1 to D1 anode (horizontal wire at d1 height)
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, s1_x, s1_y, 5.0f),
                     circuit_find_or_create_node(circuit, x + 230, s1_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, x + 230, s1_y, 5.0f),
                     circuit_find_or_create_node(circuit, x + 230, d1_a_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, x + 230, d1_a_y, 5.0f),
                     circuit_find_or_create_node(circuit, d1_a_x, d1_a_y, 5.0f));
    int s1_node = circuit_find_or_create_node(circuit, d1_a_x, d1_a_y, 5.0f);
    trans->node_ids[2] = s1_node;
    d1->node_ids[0] = s1_node;

    // S2 to D2 anode (horizontal wire at d2 height)
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, s2_x, s2_y, 5.0f),
                     circuit_find_or_create_node(circuit, x + 230, s2_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, x + 230, s2_y, 5.0f),
                     circuit_find_or_create_node(circuit, x + 230, d2_a_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, x + 230, d2_a_y, 5.0f),
                     circuit_find_or_create_node(circuit, d2_a_x, d2_a_y, 5.0f));
    int s2_node = circuit_find_or_create_node(circuit, d2_a_x, d2_a_y, 5.0f);
    trans->node_ids[4] = s2_node;
    d2->node_ids[0] = s2_node;

    // DC+ rail (connects both diode cathodes to cap/load)
    int dc_plus = circuit_find_or_create_node(circuit, x + 370, y + 50, 5.0f);

    // D1 cathode to DC+ junction (use existing terminal node)
    circuit_add_wire(circuit, d1->node_ids[1], circuit_find_or_create_node(circuit, x + 370, d1_k_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, x + 370, d1_k_y, 5.0f), dc_plus);

    // D2 cathode to DC+ junction (use existing terminal node)
    circuit_add_wire(circuit, d2->node_ids[1], circuit_find_or_create_node(circuit, x + 370, d2_k_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, x + 370, d2_k_y, 5.0f), dc_plus);

    // DC+ to capacitor and load (use existing terminal nodes)
    circuit_add_wire(circuit, dc_plus, circuit_find_or_create_node(circuit, x + 370, y - 10, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, x + 370, y - 10, 5.0f),
                     circuit_find_or_create_node(circuit, cap->x, y - 10, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cap->x, y - 10, 5.0f), cap->node_ids[0]);

    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cap->x, y - 10, 5.0f),
                     circuit_find_or_create_node(circuit, rload->x, y - 10, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rload->x, y - 10, 5.0f), rload->node_ids[0]);

    // CT to ground rail (route below components)
    int ct_node = circuit_find_or_create_node(circuit, ct_x, ct_y, 5.0f);
    trans->node_ids[3] = ct_node;
    circuit_add_wire(circuit, ct_node, circuit_find_or_create_node(circuit, x + 210, ct_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, x + 210, ct_y, 5.0f),
                     circuit_find_or_create_node(circuit, x + 210, y + 130, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, x + 210, y + 130, 5.0f),
                     circuit_find_or_create_node(circuit, x + 420, y + 130, 5.0f));

    // Ground connections
    connect_terminals(circuit, cap, 1, gnd2, 0);
    connect_terminals(circuit, rload, 1, gnd3, 0);

    return 10;
}

// AC to DC Power Supply with Transformer and Bridge Rectifier (horizontal layout)
static int place_ac_dc_supply(Circuit *circuit, float x, float y) {
    // Horizontal 2x2 bridge with transformer:
    //                         DC+ rail
    //                            |
    // AC--[TRANS]--S1--[>|]D1---+---[>|]D3--S2
    //        |           |             |     |
    //       GND    left_junc         right_junc
    //                    |             |
    //              [|<]D2---+---[|<]D4--
    //                       |
    //                    DC- rail

    // AC voltage source
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 50, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 170.0;
    vsrc->props.ac_voltage.frequency = 60.0;

    // Ground for source
    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, y + 130, 0);

    // Step-down transformer (10:1)
    Component *trans = add_comp(circuit, COMP_TRANSFORMER, x + 100, y + 50, 0);
    trans->props.transformer.turns_ratio = 0.1;

    // Bridge rectifier diodes (horizontal 2x2 grid)
    // D1/D3 point right (0 deg), D2/D4 point left (180 deg)
    Component *d1 = add_comp(circuit, COMP_DIODE, x + 260, y, 0);         // Top-left
    Component *d3 = add_comp(circuit, COMP_DIODE, x + 360, y, 0);         // Top-right
    Component *d2 = add_comp(circuit, COMP_DIODE, x + 260, y + 100, 180); // Bottom-left
    Component *d4 = add_comp(circuit, COMP_DIODE, x + 360, y + 100, 180); // Bottom-right

    // Filter capacitor
    Component *cap = add_comp(circuit, COMP_CAPACITOR_ELEC, x + 500, y + 50, 90);
    cap->props.capacitor_elec.capacitance = 1000e-6;

    // Load resistor
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 570, y + 50, 90);
    rload->props.resistor.resistance = 100.0;

    // Grounds
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 500, y + 140, 0);
    Component *gnd3 = add_comp(circuit, COMP_GROUND, x + 570, y + 140, 0);

    // Source to ground
    connect_terminals(circuit, vsrc, 1, gnd1, 0);

    // Get terminal positions
    float vsrc_pos_x, vsrc_pos_y;
    component_get_terminal_pos(vsrc, 0, &vsrc_pos_x, &vsrc_pos_y);
    float p1_x, p1_y, p2_x, p2_y;
    component_get_terminal_pos(trans, 0, &p1_x, &p1_y);
    component_get_terminal_pos(trans, 1, &p2_x, &p2_y);
    float s1_x, s1_y, s2_x, s2_y;
    component_get_terminal_pos(trans, 2, &s1_x, &s1_y);
    component_get_terminal_pos(trans, 3, &s2_x, &s2_y);

    float d1_a_x, d1_a_y, d1_k_x, d1_k_y;
    component_get_terminal_pos(d1, 0, &d1_a_x, &d1_a_y);  // anode left
    component_get_terminal_pos(d1, 1, &d1_k_x, &d1_k_y);  // cathode right

    float d2_a_x, d2_a_y, d2_k_x, d2_k_y;
    component_get_terminal_pos(d2, 0, &d2_a_x, &d2_a_y);  // anode right (180 flip)
    component_get_terminal_pos(d2, 1, &d2_k_x, &d2_k_y);  // cathode left (180 flip)

    float d3_a_x, d3_a_y, d3_k_x, d3_k_y;
    component_get_terminal_pos(d3, 0, &d3_a_x, &d3_a_y);  // anode left
    component_get_terminal_pos(d3, 1, &d3_k_x, &d3_k_y);  // cathode right

    float d4_a_x, d4_a_y, d4_k_x, d4_k_y;
    component_get_terminal_pos(d4, 0, &d4_a_x, &d4_a_y);  // anode right (180 flip)
    component_get_terminal_pos(d4, 1, &d4_k_x, &d4_k_y);  // cathode left (180 flip)

    // Source to transformer primary (route above)
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vsrc_pos_x, vsrc_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, vsrc_pos_x, y - 20, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vsrc_pos_x, y - 20, 5.0f),
                     circuit_find_or_create_node(circuit, p1_x, y - 20, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, p1_x, y - 20, 5.0f),
                     circuit_find_or_create_node(circuit, p1_x, p1_y, 5.0f));
    int prim_top = circuit_find_or_create_node(circuit, p1_x, p1_y, 5.0f);
    vsrc->node_ids[0] = prim_top;
    trans->node_ids[0] = prim_top;

    // Ground to P2 (route below)
    float gnd1_x, gnd1_y;
    component_get_terminal_pos(gnd1, 0, &gnd1_x, &gnd1_y);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, gnd1_x, gnd1_y, 5.0f),
                     circuit_find_or_create_node(circuit, gnd1_x, y + 120, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, gnd1_x, y + 120, 5.0f),
                     circuit_find_or_create_node(circuit, p2_x, y + 120, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, p2_x, y + 120, 5.0f),
                     circuit_find_or_create_node(circuit, p2_x, p2_y, 5.0f));
    int prim_bot = circuit_find_or_create_node(circuit, p2_x, p2_y, 5.0f);
    trans->node_ids[1] = prim_bot;

    // Left AC junction (D1 anode / D2 cathode)
    int left_junc = circuit_find_or_create_node(circuit, x + 230, y + 50, 5.0f);

    // S1 to left junction (route down to y+50, then right)
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, s1_x, s1_y, 5.0f),
                     circuit_find_or_create_node(circuit, x + 190, s1_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, x + 190, s1_y, 5.0f),
                     circuit_find_or_create_node(circuit, x + 190, y + 50, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, x + 190, y + 50, 5.0f), left_junc);
    trans->node_ids[2] = left_junc;

    // D1 anode to left junction (wire goes directly to terminal)
    circuit_add_wire(circuit, left_junc, circuit_find_or_create_node(circuit, d1_a_x, y + 50, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d1_a_x, y + 50, 5.0f), d1->node_ids[0]);

    // D2 cathode to left junction (wire goes directly to terminal)
    circuit_add_wire(circuit, left_junc, circuit_find_or_create_node(circuit, d2_k_x, y + 50, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d2_k_x, y + 50, 5.0f), d2->node_ids[1]);

    // Right AC junction at D3 anode x position
    int right_junc = circuit_find_or_create_node(circuit, d3_a_x, y + 50, 5.0f);

    // S2 to right junction (route below diodes)
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, s2_x, s2_y, 5.0f),
                     circuit_find_or_create_node(circuit, x + 190, s2_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, x + 190, s2_y, 5.0f),
                     circuit_find_or_create_node(circuit, x + 190, y + 140, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, x + 190, y + 140, 5.0f),
                     circuit_find_or_create_node(circuit, d3_a_x, y + 140, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d3_a_x, y + 140, 5.0f), right_junc);
    trans->node_ids[3] = right_junc;

    // D3 anode to right junction (direct vertical connection)
    circuit_add_wire(circuit, right_junc, d3->node_ids[0]);

    // D4 cathode to right junction (wire goes directly to terminal)
    circuit_add_wire(circuit, right_junc, circuit_find_or_create_node(circuit, d4_k_x, y + 50, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d4_k_x, y + 50, 5.0f), d4->node_ids[1]);

    // DC+ rail (connects D1/D3 cathodes to cap/load)
    int dc_plus = circuit_find_or_create_node(circuit, x + 430, y - 20, 5.0f);

    // D1 cathode to DC+ rail
    circuit_add_wire(circuit, d1->node_ids[1], circuit_find_or_create_node(circuit, d1_k_x, y - 20, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d1_k_x, y - 20, 5.0f), dc_plus);

    // D3 cathode to DC+ rail
    circuit_add_wire(circuit, d3->node_ids[1], circuit_find_or_create_node(circuit, d3_k_x, y - 20, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d3_k_x, y - 20, 5.0f), dc_plus);

    // DC- rail (connects D2/D4 anodes to ground)
    int dc_minus = circuit_find_or_create_node(circuit, x + 430, y + 120, 5.0f);

    // D2 anode to DC- rail
    circuit_add_wire(circuit, d2->node_ids[0], circuit_find_or_create_node(circuit, d2_a_x, y + 120, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d2_a_x, y + 120, 5.0f), dc_minus);

    // D4 anode to DC- rail
    circuit_add_wire(circuit, d4->node_ids[0], circuit_find_or_create_node(circuit, d4_a_x, y + 120, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d4_a_x, y + 120, 5.0f), dc_minus);

    // DC+ to capacitor and load (use existing terminal nodes)
    circuit_add_wire(circuit, dc_plus, circuit_find_or_create_node(circuit, cap->x, y - 20, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cap->x, y - 20, 5.0f), cap->node_ids[0]);

    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cap->x, y - 20, 5.0f),
                     circuit_find_or_create_node(circuit, rload->x, y - 20, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rload->x, y - 20, 5.0f), rload->node_ids[0]);

    // Ground connections
    connect_terminals(circuit, cap, 1, gnd2, 0);
    connect_terminals(circuit, rload, 1, gnd3, 0);

    // DC- to ground rail
    circuit_add_wire(circuit, dc_minus, circuit_find_or_create_node(circuit, x + 500, y + 120, 5.0f));

    return 11;
}

// American 120V/60Hz to 12V DC Power Supply (horizontal 2x2 bridge)
static int place_ac_dc_american(Circuit *circuit, float x, float y) {
    // Same layout as place_ac_dc_supply but with different capacitor and emphasis on 120V->12V

    // 120V AC source (170V peak for 120V RMS)
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 50, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 170.0;
    vsrc->props.ac_voltage.frequency = 60.0;

    // Ground for source
    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, y + 130, 0);

    // Step-down transformer (10:1 for ~12V output)
    Component *trans = add_comp(circuit, COMP_TRANSFORMER, x + 100, y + 50, 0);
    trans->props.transformer.turns_ratio = 0.1;

    // Bridge rectifier diodes (horizontal 2x2 grid)
    Component *d1 = add_comp(circuit, COMP_DIODE, x + 260, y, 0);         // Top-left
    Component *d3 = add_comp(circuit, COMP_DIODE, x + 360, y, 0);         // Top-right
    Component *d2 = add_comp(circuit, COMP_DIODE, x + 260, y + 100, 180); // Bottom-left
    Component *d4 = add_comp(circuit, COMP_DIODE, x + 360, y + 100, 180); // Bottom-right

    // Large filter capacitor (2200uF typical for power supply)
    Component *cap = add_comp(circuit, COMP_CAPACITOR_ELEC, x + 500, y + 50, 90);
    cap->props.capacitor_elec.capacitance = 2200e-6;
    cap->props.capacitor_elec.max_voltage = 25.0;

    // Load resistor (100 ohm = ~120mA at 12V)
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 570, y + 50, 90);
    rload->props.resistor.resistance = 100.0;

    // Grounds
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 500, y + 140, 0);
    Component *gnd3 = add_comp(circuit, COMP_GROUND, x + 570, y + 140, 0);

    // Source to ground
    connect_terminals(circuit, vsrc, 1, gnd1, 0);

    // Get terminal positions
    float vsrc_pos_x, vsrc_pos_y;
    component_get_terminal_pos(vsrc, 0, &vsrc_pos_x, &vsrc_pos_y);
    float p1_x, p1_y, p2_x, p2_y;
    component_get_terminal_pos(trans, 0, &p1_x, &p1_y);
    component_get_terminal_pos(trans, 1, &p2_x, &p2_y);
    float s1_x, s1_y, s2_x, s2_y;
    component_get_terminal_pos(trans, 2, &s1_x, &s1_y);
    component_get_terminal_pos(trans, 3, &s2_x, &s2_y);

    float d1_a_x, d1_a_y, d1_k_x, d1_k_y;
    component_get_terminal_pos(d1, 0, &d1_a_x, &d1_a_y);
    component_get_terminal_pos(d1, 1, &d1_k_x, &d1_k_y);

    float d2_a_x, d2_a_y, d2_k_x, d2_k_y;
    component_get_terminal_pos(d2, 0, &d2_a_x, &d2_a_y);
    component_get_terminal_pos(d2, 1, &d2_k_x, &d2_k_y);

    float d3_a_x, d3_a_y, d3_k_x, d3_k_y;
    component_get_terminal_pos(d3, 0, &d3_a_x, &d3_a_y);
    component_get_terminal_pos(d3, 1, &d3_k_x, &d3_k_y);

    float d4_a_x, d4_a_y, d4_k_x, d4_k_y;
    component_get_terminal_pos(d4, 0, &d4_a_x, &d4_a_y);
    component_get_terminal_pos(d4, 1, &d4_k_x, &d4_k_y);

    // Source to transformer primary (route above)
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vsrc_pos_x, vsrc_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, vsrc_pos_x, y - 20, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vsrc_pos_x, y - 20, 5.0f),
                     circuit_find_or_create_node(circuit, p1_x, y - 20, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, p1_x, y - 20, 5.0f),
                     circuit_find_or_create_node(circuit, p1_x, p1_y, 5.0f));
    int prim_top = circuit_find_or_create_node(circuit, p1_x, p1_y, 5.0f);
    vsrc->node_ids[0] = prim_top;
    trans->node_ids[0] = prim_top;

    // Ground to P2 (route below)
    float gnd1_x, gnd1_y;
    component_get_terminal_pos(gnd1, 0, &gnd1_x, &gnd1_y);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, gnd1_x, gnd1_y, 5.0f),
                     circuit_find_or_create_node(circuit, gnd1_x, y + 120, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, gnd1_x, y + 120, 5.0f),
                     circuit_find_or_create_node(circuit, p2_x, y + 120, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, p2_x, y + 120, 5.0f),
                     circuit_find_or_create_node(circuit, p2_x, p2_y, 5.0f));
    int prim_bot = circuit_find_or_create_node(circuit, p2_x, p2_y, 5.0f);
    trans->node_ids[1] = prim_bot;

    // Left AC junction (D1 anode / D2 cathode)
    int left_junc = circuit_find_or_create_node(circuit, x + 230, y + 50, 5.0f);

    // S1 to left junction
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, s1_x, s1_y, 5.0f),
                     circuit_find_or_create_node(circuit, x + 190, s1_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, x + 190, s1_y, 5.0f),
                     circuit_find_or_create_node(circuit, x + 190, y + 50, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, x + 190, y + 50, 5.0f), left_junc);
    trans->node_ids[2] = left_junc;

    // D1 anode to left junction (wire goes directly to terminal)
    circuit_add_wire(circuit, left_junc, circuit_find_or_create_node(circuit, d1_a_x, y + 50, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d1_a_x, y + 50, 5.0f), d1->node_ids[0]);

    // D2 cathode to left junction (wire goes directly to terminal)
    circuit_add_wire(circuit, left_junc, circuit_find_or_create_node(circuit, d2_k_x, y + 50, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d2_k_x, y + 50, 5.0f), d2->node_ids[1]);

    // Right AC junction at D3 anode x position
    int right_junc = circuit_find_or_create_node(circuit, d3_a_x, y + 50, 5.0f);

    // S2 to right junction (route below diodes)
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, s2_x, s2_y, 5.0f),
                     circuit_find_or_create_node(circuit, x + 190, s2_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, x + 190, s2_y, 5.0f),
                     circuit_find_or_create_node(circuit, x + 190, y + 140, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, x + 190, y + 140, 5.0f),
                     circuit_find_or_create_node(circuit, d3_a_x, y + 140, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d3_a_x, y + 140, 5.0f), right_junc);
    trans->node_ids[3] = right_junc;

    // D3 anode to right junction (direct vertical connection)
    circuit_add_wire(circuit, right_junc, d3->node_ids[0]);

    // D4 cathode to right junction (wire goes directly to terminal)
    circuit_add_wire(circuit, right_junc, circuit_find_or_create_node(circuit, d4_k_x, y + 50, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d4_k_x, y + 50, 5.0f), d4->node_ids[1]);

    // DC+ rail (connects D1/D3 cathodes to cap/load)
    int dc_plus = circuit_find_or_create_node(circuit, x + 430, y - 20, 5.0f);

    // D1 cathode to DC+ rail
    circuit_add_wire(circuit, d1->node_ids[1], circuit_find_or_create_node(circuit, d1_k_x, y - 20, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d1_k_x, y - 20, 5.0f), dc_plus);

    // D3 cathode to DC+ rail
    circuit_add_wire(circuit, d3->node_ids[1], circuit_find_or_create_node(circuit, d3_k_x, y - 20, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d3_k_x, y - 20, 5.0f), dc_plus);

    // DC- rail (connects D2/D4 anodes to ground)
    int dc_minus = circuit_find_or_create_node(circuit, x + 430, y + 120, 5.0f);

    // D2 anode to DC- rail
    circuit_add_wire(circuit, d2->node_ids[0], circuit_find_or_create_node(circuit, d2_a_x, y + 120, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d2_a_x, y + 120, 5.0f), dc_minus);

    // D4 anode to DC- rail
    circuit_add_wire(circuit, d4->node_ids[0], circuit_find_or_create_node(circuit, d4_a_x, y + 120, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, d4_a_x, y + 120, 5.0f), dc_minus);

    // DC+ to capacitor and load (use existing terminal nodes)
    circuit_add_wire(circuit, dc_plus, circuit_find_or_create_node(circuit, cap->x, y - 20, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cap->x, y - 20, 5.0f), cap->node_ids[0]);

    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cap->x, y - 20, 5.0f),
                     circuit_find_or_create_node(circuit, rload->x, y - 20, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rload->x, y - 20, 5.0f), rload->node_ids[0]);

    // Ground connections
    connect_terminals(circuit, cap, 1, gnd2, 0);
    connect_terminals(circuit, rload, 1, gnd3, 0);

    // DC- to ground rail
    circuit_add_wire(circuit, dc_minus, circuit_find_or_create_node(circuit, x + 500, y + 120, 5.0f));

    return 11;
}

// =============================================================================
// TI ANALOG CIRCUITS
// =============================================================================

// Difference Amplifier (Subtractor):
// Vout = (V2 - V1) * Rf/R1
// Layout: Power rails at top/bottom, inputs on left, output on right
static int place_difference_amp(Circuit *circuit, float x, float y) {
    // === POWER SUPPLY SECTION (top) ===
    // +12V supply at top
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x, y - 100, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 12.0;

    Component *gnd_vcc = add_comp(circuit, COMP_GROUND, x, y - 40, 0);
    connect_terminals(circuit, vcc, 1, gnd_vcc, 0);

    // Decoupling capacitor on positive rail
    Component *c_dec = add_comp(circuit, COMP_CAPACITOR, x + 60, y - 80, 90);
    c_dec->props.capacitor.capacitance = 0.1e-6; // 0.1uF

    // Connect decoupling cap: top to VCC+, bottom to ground
    float vcc_pos_x, vcc_pos_y;
    component_get_terminal_pos(vcc, 0, &vcc_pos_x, &vcc_pos_y);
    float cdec_top_x, cdec_top_y, cdec_bot_x, cdec_bot_y;
    component_get_terminal_pos(c_dec, 0, &cdec_top_x, &cdec_top_y);
    component_get_terminal_pos(c_dec, 1, &cdec_bot_x, &cdec_bot_y);

    int vcc_rail = circuit_find_or_create_node(circuit, vcc_pos_x, y - 120, 5.0f);
    int vcc_node = circuit_find_or_create_node(circuit, vcc_pos_x, vcc_pos_y, 5.0f);
    circuit_add_wire(circuit, vcc_node, vcc_rail);
    vcc->node_ids[0] = vcc_node;

    int cdec_top_node = circuit_find_or_create_node(circuit, cdec_top_x, cdec_top_y, 5.0f);
    int corner_cdec = circuit_find_or_create_node(circuit, cdec_top_x, y - 120, 5.0f);
    circuit_add_wire(circuit, vcc_rail, corner_cdec);
    circuit_add_wire(circuit, corner_cdec, cdec_top_node);
    c_dec->node_ids[0] = cdec_top_node;

    Component *gnd_cdec = add_comp(circuit, COMP_GROUND, x + 60, y - 20, 0);
    connect_terminals(circuit, c_dec, 1, gnd_cdec, 0);

    // === INPUT SECTION (left side) ===
    // AC source for V1 (input signal)
    Component *v1 = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 40, 0);
    v1->props.ac_voltage.amplitude = 1.0;
    v1->props.ac_voltage.frequency = 1000.0;
    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, y + 100, 0);
    connect_terminals(circuit, v1, 1, gnd1, 0);

    // DC source for V2 (reference input)
    Component *v2 = add_comp(circuit, COMP_DC_VOLTAGE, x, y + 180, 0);
    v2->props.dc_voltage.voltage = 0.5;
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x, y + 240, 0);
    connect_terminals(circuit, v2, 1, gnd2, 0);

    // === OP-AMP AND RESISTOR NETWORK (center) ===
    // R1 (V1 to inverting input)
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 140, y + 20, 0);
    r1->props.resistor.resistance = 10000.0;

    // R2 (V2 to non-inverting input)
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 140, y + 140, 0);
    r2->props.resistor.resistance = 10000.0;

    // Op-amp
    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 280, y + 60, 0);
    opamp->props.opamp.gain = 100000.0;
    opamp->props.opamp.ideal = true;

    // Rf (feedback resistor)
    Component *rf = add_comp(circuit, COMP_RESISTOR, x + 280, y - 20, 0);
    rf->props.resistor.resistance = 10000.0;

    // R3 (non-inverting to ground)
    Component *r3 = add_comp(circuit, COMP_RESISTOR, x + 220, y + 180, 90);
    r3->props.resistor.resistance = 10000.0;
    Component *gnd3 = add_comp(circuit, COMP_GROUND, x + 220, y + 240, 0);
    connect_terminals(circuit, r3, 1, gnd3, 0);

    // === OUTPUT SECTION (right side) ===
    // Output load resistor
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 400, y + 100, 90);
    rload->props.resistor.resistance = 10000.0;
    Component *gnd_load = add_comp(circuit, COMP_GROUND, x + 400, y + 160, 0);
    connect_terminals(circuit, rload, 1, gnd_load, 0);

    // === WIRING ===
    // V1 to R1 (up then right)
    float v1_pos_x, v1_pos_y;
    component_get_terminal_pos(v1, 0, &v1_pos_x, &v1_pos_y);
    float r1_left_x, r1_left_y;
    component_get_terminal_pos(r1, 0, &r1_left_x, &r1_left_y);

    int v1_node = circuit_find_or_create_node(circuit, v1_pos_x, v1_pos_y, 5.0f);
    int v1_corner = circuit_find_or_create_node(circuit, v1_pos_x, r1_left_y, 5.0f);
    int r1_left_node = circuit_find_or_create_node(circuit, r1_left_x, r1_left_y, 5.0f);
    circuit_add_wire(circuit, v1_node, v1_corner);
    circuit_add_wire(circuit, v1_corner, r1_left_node);
    v1->node_ids[0] = v1_node;
    r1->node_ids[0] = r1_left_node;

    // V2 to R2 (up then right)
    float v2_pos_x, v2_pos_y;
    component_get_terminal_pos(v2, 0, &v2_pos_x, &v2_pos_y);
    float r2_left_x, r2_left_y;
    component_get_terminal_pos(r2, 0, &r2_left_x, &r2_left_y);

    int v2_node = circuit_find_or_create_node(circuit, v2_pos_x, v2_pos_y, 5.0f);
    int v2_corner = circuit_find_or_create_node(circuit, v2_pos_x, r2_left_y, 5.0f);
    int r2_left_node = circuit_find_or_create_node(circuit, r2_left_x, r2_left_y, 5.0f);
    circuit_add_wire(circuit, v2_node, v2_corner);
    circuit_add_wire(circuit, v2_corner, r2_left_node);
    v2->node_ids[0] = v2_node;
    r2->node_ids[0] = r2_left_node;

    // R1 to inverting junction (continues to Rf)
    float r1_right_x, r1_right_y;
    component_get_terminal_pos(r1, 1, &r1_right_x, &r1_right_y);
    float opamp_inv_x, opamp_inv_y;
    component_get_terminal_pos(opamp, 0, &opamp_inv_x, &opamp_inv_y);
    float rf_left_x, rf_left_y;
    component_get_terminal_pos(rf, 0, &rf_left_x, &rf_left_y);

    int inv_junc = circuit_find_or_create_node(circuit, r1_right_x, r1_right_y, 5.0f);
    r1->node_ids[1] = inv_junc;

    // Junction down to op-amp inverting
    int inv_corner = circuit_find_or_create_node(circuit, r1_right_x, opamp_inv_y, 5.0f);
    int opamp_inv_node = circuit_find_or_create_node(circuit, opamp_inv_x, opamp_inv_y, 5.0f);
    circuit_add_wire(circuit, inv_junc, inv_corner);
    circuit_add_wire(circuit, inv_corner, opamp_inv_node);
    opamp->node_ids[0] = opamp_inv_node;

    // Junction up to Rf left
    int rf_corner = circuit_find_or_create_node(circuit, r1_right_x, rf_left_y, 5.0f);
    int rf_left_node = circuit_find_or_create_node(circuit, rf_left_x, rf_left_y, 5.0f);
    circuit_add_wire(circuit, inv_junc, rf_corner);
    circuit_add_wire(circuit, rf_corner, rf_left_node);
    rf->node_ids[0] = rf_left_node;

    // R2 to non-inverting junction (continues to R3)
    float r2_right_x, r2_right_y;
    component_get_terminal_pos(r2, 1, &r2_right_x, &r2_right_y);
    float opamp_noninv_x, opamp_noninv_y;
    component_get_terminal_pos(opamp, 1, &opamp_noninv_x, &opamp_noninv_y);
    float r3_top_x, r3_top_y;
    component_get_terminal_pos(r3, 0, &r3_top_x, &r3_top_y);

    int noninv_junc = circuit_find_or_create_node(circuit, r2_right_x, r2_right_y, 5.0f);
    r2->node_ids[1] = noninv_junc;

    // Junction up to op-amp non-inverting
    int noninv_corner = circuit_find_or_create_node(circuit, r2_right_x, opamp_noninv_y, 5.0f);
    int opamp_noninv_node = circuit_find_or_create_node(circuit, opamp_noninv_x, opamp_noninv_y, 5.0f);
    circuit_add_wire(circuit, noninv_junc, noninv_corner);
    circuit_add_wire(circuit, noninv_corner, opamp_noninv_node);
    opamp->node_ids[1] = opamp_noninv_node;

    // Junction down to R3 top
    int r3_corner = circuit_find_or_create_node(circuit, r2_right_x, r3_top_y, 5.0f);
    int r3_top_node = circuit_find_or_create_node(circuit, r3_top_x, r3_top_y, 5.0f);
    circuit_add_wire(circuit, noninv_junc, r3_corner);
    circuit_add_wire(circuit, r3_corner, r3_top_node);
    r3->node_ids[0] = r3_top_node;

    // Rf right to output junction
    float rf_right_x, rf_right_y;
    component_get_terminal_pos(rf, 1, &rf_right_x, &rf_right_y);
    float opamp_out_x, opamp_out_y;
    component_get_terminal_pos(opamp, 2, &opamp_out_x, &opamp_out_y);
    float rload_top_x, rload_top_y;
    component_get_terminal_pos(rload, 0, &rload_top_x, &rload_top_y);

    int out_junc = circuit_find_or_create_node(circuit, opamp_out_x + 40, opamp_out_y, 5.0f);
    int opamp_out_node = circuit_find_or_create_node(circuit, opamp_out_x, opamp_out_y, 5.0f);
    circuit_add_wire(circuit, opamp_out_node, out_junc);
    opamp->node_ids[2] = opamp_out_node;

    // Rf right down to output level
    int rf_right_node = circuit_find_or_create_node(circuit, rf_right_x, rf_right_y, 5.0f);
    int rf_out_corner = circuit_find_or_create_node(circuit, rf_right_x, opamp_out_y, 5.0f);
    circuit_add_wire(circuit, rf_right_node, rf_out_corner);
    circuit_add_wire(circuit, rf_out_corner, out_junc);
    rf->node_ids[1] = rf_right_node;

    // Output to load resistor
    int rload_top_node = circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f);
    int load_corner = circuit_find_or_create_node(circuit, rload_top_x, opamp_out_y, 5.0f);
    circuit_add_wire(circuit, out_junc, load_corner);
    circuit_add_wire(circuit, load_corner, rload_top_node);
    rload->node_ids[0] = rload_top_node;

    return 17;
}

// Transimpedance Amplifier (Current to Voltage Converter):
// Vout = -Iin * Rf
// Layout: Power rails at top, current source on left, output on right
static int place_transimpedance(Circuit *circuit, float x, float y) {
    // === POWER SUPPLY SECTION (top) ===
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x, y - 100, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 12.0;
    Component *gnd_vcc = add_comp(circuit, COMP_GROUND, x, y - 40, 0);
    connect_terminals(circuit, vcc, 1, gnd_vcc, 0);

    // Decoupling capacitor
    Component *c_dec = add_comp(circuit, COMP_CAPACITOR, x + 60, y - 80, 90);
    c_dec->props.capacitor.capacitance = 0.1e-6;

    float vcc_pos_x, vcc_pos_y;
    component_get_terminal_pos(vcc, 0, &vcc_pos_x, &vcc_pos_y);
    float cdec_top_x, cdec_top_y;
    component_get_terminal_pos(c_dec, 0, &cdec_top_x, &cdec_top_y);

    int vcc_rail = circuit_find_or_create_node(circuit, vcc_pos_x, y - 120, 5.0f);
    int vcc_node = circuit_find_or_create_node(circuit, vcc_pos_x, vcc_pos_y, 5.0f);
    circuit_add_wire(circuit, vcc_node, vcc_rail);
    vcc->node_ids[0] = vcc_node;

    int cdec_top_node = circuit_find_or_create_node(circuit, cdec_top_x, cdec_top_y, 5.0f);
    int corner_cdec = circuit_find_or_create_node(circuit, cdec_top_x, y - 120, 5.0f);
    circuit_add_wire(circuit, vcc_rail, corner_cdec);
    circuit_add_wire(circuit, corner_cdec, cdec_top_node);
    c_dec->node_ids[0] = cdec_top_node;

    Component *gnd_cdec = add_comp(circuit, COMP_GROUND, x + 60, y - 20, 0);
    connect_terminals(circuit, c_dec, 1, gnd_cdec, 0);

    // === INPUT SECTION (left side) ===
    // Current source to simulate photodiode
    Component *isrc = add_comp(circuit, COMP_DC_CURRENT, x, y + 40, 0);
    isrc->props.dc_current.current = 0.001; // 1mA
    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, y + 100, 0);
    connect_terminals(circuit, isrc, 1, gnd1, 0);

    // === OP-AMP SECTION (center) ===
    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 200, y + 20, 0);
    opamp->props.opamp.gain = 100000.0;
    opamp->props.opamp.ideal = true;

    // Feedback resistor
    Component *rf = add_comp(circuit, COMP_RESISTOR, x + 200, y - 40, 0);
    rf->props.resistor.resistance = 10000.0; // 10k: 1mA * 10k = 10V

    // Non-inverting input to ground
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 160, y + 100, 0);

    // === OUTPUT SECTION (right side) ===
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 340, y + 60, 90);
    rload->props.resistor.resistance = 10000.0;
    Component *gnd_load = add_comp(circuit, COMP_GROUND, x + 340, y + 120, 0);
    connect_terminals(circuit, rload, 1, gnd_load, 0);

    // === WIRING ===
    // Current source to inverting junction
    float isrc_pos_x, isrc_pos_y;
    component_get_terminal_pos(isrc, 0, &isrc_pos_x, &isrc_pos_y);
    float opamp_inv_x, opamp_inv_y;
    component_get_terminal_pos(opamp, 0, &opamp_inv_x, &opamp_inv_y);
    float rf_left_x, rf_left_y;
    component_get_terminal_pos(rf, 0, &rf_left_x, &rf_left_y);

    int inv_junc = circuit_find_or_create_node(circuit, rf_left_x, opamp_inv_y, 5.0f);

    int isrc_node = circuit_find_or_create_node(circuit, isrc_pos_x, isrc_pos_y, 5.0f);
    int isrc_corner = circuit_find_or_create_node(circuit, isrc_pos_x, opamp_inv_y, 5.0f);
    circuit_add_wire(circuit, isrc_node, isrc_corner);
    circuit_add_wire(circuit, isrc_corner, inv_junc);
    isrc->node_ids[0] = isrc_node;

    // Junction to op-amp inverting
    int opamp_inv_node = circuit_find_or_create_node(circuit, opamp_inv_x, opamp_inv_y, 5.0f);
    circuit_add_wire(circuit, inv_junc, opamp_inv_node);
    opamp->node_ids[0] = opamp_inv_node;

    // Junction up to Rf left
    int rf_left_node = circuit_find_or_create_node(circuit, rf_left_x, rf_left_y, 5.0f);
    circuit_add_wire(circuit, inv_junc, rf_left_node);
    rf->node_ids[0] = rf_left_node;

    // Op-amp non-inverting to ground
    float opamp_noninv_x, opamp_noninv_y;
    component_get_terminal_pos(opamp, 1, &opamp_noninv_x, &opamp_noninv_y);
    float gnd2_x, gnd2_y;
    component_get_terminal_pos(gnd2, 0, &gnd2_x, &gnd2_y);

    int opamp_noninv_node = circuit_find_or_create_node(circuit, opamp_noninv_x, opamp_noninv_y, 5.0f);
    int gnd2_corner = circuit_find_or_create_node(circuit, gnd2_x, opamp_noninv_y, 5.0f);
    int gnd2_node = circuit_find_or_create_node(circuit, gnd2_x, gnd2_y, 5.0f);
    circuit_add_wire(circuit, opamp_noninv_node, gnd2_corner);
    circuit_add_wire(circuit, gnd2_corner, gnd2_node);
    opamp->node_ids[1] = opamp_noninv_node;
    gnd2->node_ids[0] = gnd2_node;

    // Rf right to output junction
    float rf_right_x, rf_right_y;
    component_get_terminal_pos(rf, 1, &rf_right_x, &rf_right_y);
    float opamp_out_x, opamp_out_y;
    component_get_terminal_pos(opamp, 2, &opamp_out_x, &opamp_out_y);
    float rload_top_x, rload_top_y;
    component_get_terminal_pos(rload, 0, &rload_top_x, &rload_top_y);

    int out_junc = circuit_find_or_create_node(circuit, opamp_out_x + 40, opamp_out_y, 5.0f);
    int opamp_out_node = circuit_find_or_create_node(circuit, opamp_out_x, opamp_out_y, 5.0f);
    circuit_add_wire(circuit, opamp_out_node, out_junc);
    opamp->node_ids[2] = opamp_out_node;

    int rf_right_node = circuit_find_or_create_node(circuit, rf_right_x, rf_right_y, 5.0f);
    int rf_out_corner = circuit_find_or_create_node(circuit, rf_right_x, opamp_out_y, 5.0f);
    circuit_add_wire(circuit, rf_right_node, rf_out_corner);
    circuit_add_wire(circuit, rf_out_corner, out_junc);
    rf->node_ids[1] = rf_right_node;

    // Output to load resistor
    int rload_top_node = circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f);
    int load_corner = circuit_find_or_create_node(circuit, rload_top_x, opamp_out_y, 5.0f);
    circuit_add_wire(circuit, out_junc, load_corner);
    circuit_add_wire(circuit, load_corner, rload_top_node);
    rload->node_ids[0] = rload_top_node;

    return 11;
}

// Instrumentation Amplifier (Three Op-Amp):
// High CMRR differential amplifier
static int place_instr_amp(Circuit *circuit, float x, float y) {
    // Three op-amp instrumentation amplifier
    // First stage: two unity gain buffers for high input impedance
    // Second stage: difference amplifier

    // Input sources
    Component *v1 = add_comp(circuit, COMP_AC_VOLTAGE, x, y - 20, 0);
    if (!v1) return 0;
    v1->props.ac_voltage.amplitude = 0.1;
    v1->props.ac_voltage.frequency = 1000.0;

    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, y + 40, 0);

    Component *v2 = add_comp(circuit, COMP_DC_VOLTAGE, x, y + 160, 0);
    v2->props.dc_voltage.voltage = 0.05;

    Component *gnd2 = add_comp(circuit, COMP_GROUND, x, y + 220, 0);

    // First stage op-amps (buffers with gain set by Rg)
    Component *op1 = add_comp(circuit, COMP_OPAMP, x + 160, y - 40, 0);
    op1->props.opamp.ideal = true;

    Component *op2 = add_comp(circuit, COMP_OPAMP, x + 160, y + 120, 0);
    op2->props.opamp.ideal = true;

    // Gain resistor Rg between the two first-stage outputs
    Component *rg = add_comp(circuit, COMP_RESISTOR, x + 240, y + 40, 90);
    rg->props.resistor.resistance = 1000.0; // Gain = 1 + 2*R/Rg

    // Feedback resistors for first stage
    Component *r1a = add_comp(circuit, COMP_RESISTOR, x + 240, y - 80, 90);
    r1a->props.resistor.resistance = 10000.0;

    Component *r1b = add_comp(circuit, COMP_RESISTOR, x + 240, y + 160, 90);
    r1b->props.resistor.resistance = 10000.0;

    // Second stage (difference amplifier)
    Component *op3 = add_comp(circuit, COMP_OPAMP, x + 400, y + 40, 0);
    op3->props.opamp.ideal = true;

    Component *r2a = add_comp(circuit, COMP_RESISTOR, x + 320, y - 20, 0);
    r2a->props.resistor.resistance = 10000.0;

    Component *r2b = add_comp(circuit, COMP_RESISTOR, x + 320, y + 100, 0);
    r2b->props.resistor.resistance = 10000.0;

    Component *rf = add_comp(circuit, COMP_RESISTOR, x + 400, y - 20, 0);
    rf->props.resistor.resistance = 10000.0;

    Component *r3 = add_comp(circuit, COMP_RESISTOR, x + 360, y + 140, 90);
    r3->props.resistor.resistance = 10000.0;

    Component *gnd3 = add_comp(circuit, COMP_GROUND, x + 360, y + 200, 0);

    // Connect input sources to ground
    connect_terminals(circuit, v1, 1, gnd1, 0);
    connect_terminals(circuit, v2, 1, gnd2, 0);

    // Connect R3 to ground
    connect_terminals(circuit, r3, 1, gnd3, 0);

    // V1 to op1 non-inverting
    float v1_pos_x, v1_pos_y;
    component_get_terminal_pos(v1, 0, &v1_pos_x, &v1_pos_y);
    float op1_noninv_x, op1_noninv_y;
    component_get_terminal_pos(op1, 1, &op1_noninv_x, &op1_noninv_y);

    wire_L_shape(circuit, v1_pos_x, v1_pos_y, op1_noninv_x, op1_noninv_y, true);

    // V2 to op2 non-inverting
    float v2_pos_x, v2_pos_y;
    component_get_terminal_pos(v2, 0, &v2_pos_x, &v2_pos_y);
    float op2_noninv_x, op2_noninv_y;
    component_get_terminal_pos(op2, 1, &op2_noninv_x, &op2_noninv_y);

    wire_L_shape(circuit, v2_pos_x, v2_pos_y, op2_noninv_x, op2_noninv_y, true);

    // Op1 inverting to R1a and Rg
    float op1_inv_x, op1_inv_y;
    component_get_terminal_pos(op1, 0, &op1_inv_x, &op1_inv_y);
    float r1a_bot_x, r1a_bot_y;
    component_get_terminal_pos(r1a, 1, &r1a_bot_x, &r1a_bot_y);
    float rg_top_x, rg_top_y;
    component_get_terminal_pos(rg, 0, &rg_top_x, &rg_top_y);

    int op1_inv_node = circuit_find_or_create_node(circuit, op1_inv_x, op1_inv_y, 5.0f);
    int junc1 = circuit_find_or_create_node(circuit, r1a_bot_x, op1_inv_y, 5.0f);
    circuit_add_wire(circuit, op1_inv_node, junc1);
    wire_L_shape(circuit, r1a_bot_x, op1_inv_y, r1a_bot_x, r1a_bot_y, false);
    wire_L_shape(circuit, r1a_bot_x, op1_inv_y, rg_top_x, rg_top_y, false);

    // Op2 inverting to R1b and Rg
    float op2_inv_x, op2_inv_y;
    component_get_terminal_pos(op2, 0, &op2_inv_x, &op2_inv_y);
    float r1b_top_x, r1b_top_y;
    component_get_terminal_pos(r1b, 0, &r1b_top_x, &r1b_top_y);
    float rg_bot_x, rg_bot_y;
    component_get_terminal_pos(rg, 1, &rg_bot_x, &rg_bot_y);

    int op2_inv_node = circuit_find_or_create_node(circuit, op2_inv_x, op2_inv_y, 5.0f);
    int junc2 = circuit_find_or_create_node(circuit, r1b_top_x, op2_inv_y, 5.0f);
    circuit_add_wire(circuit, op2_inv_node, junc2);
    wire_L_shape(circuit, r1b_top_x, op2_inv_y, r1b_top_x, r1b_top_y, false);
    wire_L_shape(circuit, r1b_top_x, op2_inv_y, rg_bot_x, rg_bot_y, false);

    // Op1 output to R1a top and R2a left
    float op1_out_x, op1_out_y;
    component_get_terminal_pos(op1, 2, &op1_out_x, &op1_out_y);
    float r1a_top_x, r1a_top_y;
    component_get_terminal_pos(r1a, 0, &r1a_top_x, &r1a_top_y);
    float r2a_left_x, r2a_left_y;
    component_get_terminal_pos(r2a, 0, &r2a_left_x, &r2a_left_y);

    int op1_out_node = circuit_find_or_create_node(circuit, op1_out_x, op1_out_y, 5.0f);
    wire_L_shape(circuit, op1_out_x, op1_out_y, r1a_top_x, r1a_top_y, false);
    wire_L_shape(circuit, op1_out_x, op1_out_y, r2a_left_x, r2a_left_y, true);

    // Op2 output to R1b bottom and R2b left
    float op2_out_x, op2_out_y;
    component_get_terminal_pos(op2, 2, &op2_out_x, &op2_out_y);
    float r1b_bot_x, r1b_bot_y;
    component_get_terminal_pos(r1b, 1, &r1b_bot_x, &r1b_bot_y);
    float r2b_left_x, r2b_left_y;
    component_get_terminal_pos(r2b, 0, &r2b_left_x, &r2b_left_y);

    int op2_out_node = circuit_find_or_create_node(circuit, op2_out_x, op2_out_y, 5.0f);
    wire_L_shape(circuit, op2_out_x, op2_out_y, r1b_bot_x, r1b_bot_y, false);
    wire_L_shape(circuit, op2_out_x, op2_out_y, r2b_left_x, r2b_left_y, true);

    // R2a right to op3 inverting and Rf left
    float r2a_right_x, r2a_right_y;
    component_get_terminal_pos(r2a, 1, &r2a_right_x, &r2a_right_y);
    float op3_inv_x, op3_inv_y;
    component_get_terminal_pos(op3, 0, &op3_inv_x, &op3_inv_y);
    float rf_left_x, rf_left_y;
    component_get_terminal_pos(rf, 0, &rf_left_x, &rf_left_y);

    int junc3 = circuit_find_or_create_node(circuit, r2a_right_x, r2a_right_y, 5.0f);
    wire_L_shape(circuit, r2a_right_x, r2a_right_y, op3_inv_x, op3_inv_y, false);
    wire_L_shape(circuit, r2a_right_x, r2a_right_y, rf_left_x, rf_left_y, false);

    // R2b right to op3 non-inverting and R3 top
    float r2b_right_x, r2b_right_y;
    component_get_terminal_pos(r2b, 1, &r2b_right_x, &r2b_right_y);
    float op3_noninv_x, op3_noninv_y;
    component_get_terminal_pos(op3, 1, &op3_noninv_x, &op3_noninv_y);
    float r3_top_x, r3_top_y;
    component_get_terminal_pos(r3, 0, &r3_top_x, &r3_top_y);

    int junc4 = circuit_find_or_create_node(circuit, r2b_right_x, r2b_right_y, 5.0f);
    wire_L_shape(circuit, r2b_right_x, r2b_right_y, op3_noninv_x, op3_noninv_y, false);
    wire_L_shape(circuit, r2b_right_x, r2b_right_y, r3_top_x, r3_top_y, false);

    // Rf right to op3 output
    float rf_right_x, rf_right_y;
    component_get_terminal_pos(rf, 1, &rf_right_x, &rf_right_y);
    float op3_out_x, op3_out_y;
    component_get_terminal_pos(op3, 2, &op3_out_x, &op3_out_y);

    wire_L_shape(circuit, rf_right_x, rf_right_y, op3_out_x, op3_out_y, false);

    return 17;
}

// Sallen-Key Low Pass Filter (2nd Order):
// Unity gain version with fc = 1/(2*pi*R*C)
// Layout: Power rail at top, input left, output right
static int place_sallen_key_lp(Circuit *circuit, float x, float y) {
    // === POWER SUPPLY (top) ===
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x, y - 100, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 12.0;
    Component *gnd_vcc = add_comp(circuit, COMP_GROUND, x, y - 40, 0);
    connect_terminals(circuit, vcc, 1, gnd_vcc, 0);

    // Decoupling capacitor
    Component *c_dec = add_comp(circuit, COMP_CAPACITOR, x + 60, y - 80, 90);
    c_dec->props.capacitor.capacitance = 0.1e-6;

    float vcc_pos_x, vcc_pos_y;
    component_get_terminal_pos(vcc, 0, &vcc_pos_x, &vcc_pos_y);
    float cdec_top_x, cdec_top_y;
    component_get_terminal_pos(c_dec, 0, &cdec_top_x, &cdec_top_y);

    int vcc_rail = circuit_find_or_create_node(circuit, vcc_pos_x, y - 120, 5.0f);
    int vcc_node = circuit_find_or_create_node(circuit, vcc_pos_x, vcc_pos_y, 5.0f);
    circuit_add_wire(circuit, vcc_node, vcc_rail);
    vcc->node_ids[0] = vcc_node;

    int cdec_top_node = circuit_find_or_create_node(circuit, cdec_top_x, cdec_top_y, 5.0f);
    int corner_cdec = circuit_find_or_create_node(circuit, cdec_top_x, y - 120, 5.0f);
    circuit_add_wire(circuit, vcc_rail, corner_cdec);
    circuit_add_wire(circuit, corner_cdec, cdec_top_node);
    c_dec->node_ids[0] = cdec_top_node;

    Component *gnd_cdec = add_comp(circuit, COMP_GROUND, x + 60, y - 20, 0);
    connect_terminals(circuit, c_dec, 1, gnd_cdec, 0);

    // === INPUT SECTION (left) ===
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 40, 0);
    vsrc->props.ac_voltage.amplitude = 1.0;
    vsrc->props.ac_voltage.frequency = 1000.0;
    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, y + 100, 0);
    connect_terminals(circuit, vsrc, 1, gnd1, 0);

    // === FILTER NETWORK ===
    // R1 (series input)
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 140, y + 20, 0);
    r1->props.resistor.resistance = 10000.0;

    // R2 (series)
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 240, y + 20, 0);
    r2->props.resistor.resistance = 10000.0;

    // C1 (from R1-R2 junction to output - feedback)
    Component *c1 = add_comp(circuit, COMP_CAPACITOR, x + 190, y - 40, 0);
    c1->props.capacitor.capacitance = 10e-9;

    // C2 (from R2 output to ground)
    Component *c2 = add_comp(circuit, COMP_CAPACITOR, x + 320, y + 60, 90);
    c2->props.capacitor.capacitance = 10e-9;
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 320, y + 120, 0);
    connect_terminals(circuit, c2, 1, gnd2, 0);

    // === OP-AMP (unity gain buffer) ===
    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 400, y + 20, 0);
    opamp->props.opamp.ideal = true;

    // === OUTPUT SECTION (right) ===
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 500, y + 60, 90);
    rload->props.resistor.resistance = 10000.0;
    Component *gnd_load = add_comp(circuit, COMP_GROUND, x + 500, y + 120, 0);
    connect_terminals(circuit, rload, 1, gnd_load, 0);

    // === WIRING ===
    // Source to R1
    float vsrc_pos_x, vsrc_pos_y;
    component_get_terminal_pos(vsrc, 0, &vsrc_pos_x, &vsrc_pos_y);
    float r1_left_x, r1_left_y;
    component_get_terminal_pos(r1, 0, &r1_left_x, &r1_left_y);

    int vsrc_node = circuit_find_or_create_node(circuit, vsrc_pos_x, vsrc_pos_y, 5.0f);
    int vsrc_corner = circuit_find_or_create_node(circuit, vsrc_pos_x, r1_left_y, 5.0f);
    int r1_left_node = circuit_find_or_create_node(circuit, r1_left_x, r1_left_y, 5.0f);
    circuit_add_wire(circuit, vsrc_node, vsrc_corner);
    circuit_add_wire(circuit, vsrc_corner, r1_left_node);
    vsrc->node_ids[0] = vsrc_node;
    r1->node_ids[0] = r1_left_node;

    // R1 to R2 junction
    float r1_right_x, r1_right_y;
    component_get_terminal_pos(r1, 1, &r1_right_x, &r1_right_y);
    float r2_left_x, r2_left_y;
    component_get_terminal_pos(r2, 0, &r2_left_x, &r2_left_y);

    int junc1 = circuit_find_or_create_node(circuit, r1_right_x, r1_right_y, 5.0f);
    int r2_left_node = circuit_find_or_create_node(circuit, r2_left_x, r2_left_y, 5.0f);
    circuit_add_wire(circuit, junc1, r2_left_node);
    r1->node_ids[1] = junc1;
    r2->node_ids[0] = r2_left_node;

    // C1 left to junction (up from junction)
    float c1_left_x, c1_left_y;
    component_get_terminal_pos(c1, 0, &c1_left_x, &c1_left_y);

    int corner1 = circuit_find_or_create_node(circuit, r1_right_x, c1_left_y, 5.0f);
    int c1_left_node = circuit_find_or_create_node(circuit, c1_left_x, c1_left_y, 5.0f);
    circuit_add_wire(circuit, junc1, corner1);
    circuit_add_wire(circuit, corner1, c1_left_node);
    c1->node_ids[0] = c1_left_node;

    // R2 to op-amp non-inverting and C2
    float r2_right_x, r2_right_y;
    component_get_terminal_pos(r2, 1, &r2_right_x, &r2_right_y);
    float opamp_noninv_x, opamp_noninv_y;
    component_get_terminal_pos(opamp, 1, &opamp_noninv_x, &opamp_noninv_y);
    float c2_top_x, c2_top_y;
    component_get_terminal_pos(c2, 0, &c2_top_x, &c2_top_y);

    int junc2 = circuit_find_or_create_node(circuit, r2_right_x, r2_right_y, 5.0f);
    r2->node_ids[1] = junc2;

    // To C2
    int c2_top_node = circuit_find_or_create_node(circuit, c2_top_x, c2_top_y, 5.0f);
    int c2_corner = circuit_find_or_create_node(circuit, c2_top_x, r2_right_y, 5.0f);
    circuit_add_wire(circuit, junc2, c2_corner);
    circuit_add_wire(circuit, c2_corner, c2_top_node);
    c2->node_ids[0] = c2_top_node;

    // To op-amp non-inverting
    int opamp_noninv_node = circuit_find_or_create_node(circuit, opamp_noninv_x, opamp_noninv_y, 5.0f);
    int noninv_corner = circuit_find_or_create_node(circuit, c2_top_x, opamp_noninv_y, 5.0f);
    circuit_add_wire(circuit, c2_corner, noninv_corner);
    circuit_add_wire(circuit, noninv_corner, opamp_noninv_node);
    opamp->node_ids[1] = opamp_noninv_node;

    // Op-amp inverting to output (unity gain feedback)
    float opamp_inv_x, opamp_inv_y;
    component_get_terminal_pos(opamp, 0, &opamp_inv_x, &opamp_inv_y);
    float opamp_out_x, opamp_out_y;
    component_get_terminal_pos(opamp, 2, &opamp_out_x, &opamp_out_y);
    float rload_top_x, rload_top_y;
    component_get_terminal_pos(rload, 0, &rload_top_x, &rload_top_y);

    int opamp_inv_node = circuit_find_or_create_node(circuit, opamp_inv_x, opamp_inv_y, 5.0f);
    int opamp_out_node = circuit_find_or_create_node(circuit, opamp_out_x, opamp_out_y, 5.0f);
    int out_junc = circuit_find_or_create_node(circuit, opamp_out_x + 20, opamp_out_y, 5.0f);
    int feedback_corner = circuit_find_or_create_node(circuit, opamp_out_x + 20, opamp_inv_y, 5.0f);
    circuit_add_wire(circuit, opamp_out_node, out_junc);
    circuit_add_wire(circuit, out_junc, feedback_corner);
    circuit_add_wire(circuit, feedback_corner, opamp_inv_node);
    opamp->node_ids[0] = opamp_inv_node;
    opamp->node_ids[2] = opamp_out_node;

    // C1 right to output junction
    float c1_right_x, c1_right_y;
    component_get_terminal_pos(c1, 1, &c1_right_x, &c1_right_y);

    int c1_right_node = circuit_find_or_create_node(circuit, c1_right_x, c1_right_y, 5.0f);
    int c1_out_corner = circuit_find_or_create_node(circuit, opamp_out_x + 20, c1_right_y, 5.0f);
    circuit_add_wire(circuit, c1_right_node, c1_out_corner);
    circuit_add_wire(circuit, c1_out_corner, out_junc);
    c1->node_ids[1] = c1_right_node;

    // Output to load resistor
    int rload_top_node = circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f);
    int load_corner = circuit_find_or_create_node(circuit, rload_top_x, opamp_out_y, 5.0f);
    circuit_add_wire(circuit, out_junc, load_corner);
    circuit_add_wire(circuit, load_corner, rload_top_node);
    rload->node_ids[0] = rload_top_node;

    return 15;
}

// Active Band Pass Filter (Multiple Feedback topology)
// Layout: Power rail at top, input left, output right
static int place_bandpass_active(Circuit *circuit, float x, float y) {
    // === POWER SUPPLY (top) ===
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x, y - 100, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 12.0;
    Component *gnd_vcc = add_comp(circuit, COMP_GROUND, x, y - 40, 0);
    connect_terminals(circuit, vcc, 1, gnd_vcc, 0);

    // Decoupling capacitor
    Component *c_dec = add_comp(circuit, COMP_CAPACITOR, x + 60, y - 80, 90);
    c_dec->props.capacitor.capacitance = 0.1e-6;

    float vcc_pos_x, vcc_pos_y;
    component_get_terminal_pos(vcc, 0, &vcc_pos_x, &vcc_pos_y);
    float cdec_top_x, cdec_top_y;
    component_get_terminal_pos(c_dec, 0, &cdec_top_x, &cdec_top_y);

    int vcc_rail = circuit_find_or_create_node(circuit, vcc_pos_x, y - 120, 5.0f);
    int vcc_node = circuit_find_or_create_node(circuit, vcc_pos_x, vcc_pos_y, 5.0f);
    circuit_add_wire(circuit, vcc_node, vcc_rail);
    vcc->node_ids[0] = vcc_node;

    int cdec_top_node = circuit_find_or_create_node(circuit, cdec_top_x, cdec_top_y, 5.0f);
    int corner_cdec = circuit_find_or_create_node(circuit, cdec_top_x, y - 120, 5.0f);
    circuit_add_wire(circuit, vcc_rail, corner_cdec);
    circuit_add_wire(circuit, corner_cdec, cdec_top_node);
    c_dec->node_ids[0] = cdec_top_node;

    Component *gnd_cdec = add_comp(circuit, COMP_GROUND, x + 60, y - 20, 0);
    connect_terminals(circuit, c_dec, 1, gnd_cdec, 0);

    // === INPUT SECTION (left) ===
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 40, 0);
    vsrc->props.ac_voltage.amplitude = 1.0;
    vsrc->props.ac_voltage.frequency = 1000.0;
    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, y + 100, 0);
    connect_terminals(circuit, vsrc, 1, gnd1, 0);

    // === FILTER NETWORK ===
    // Input resistor R1
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 140, y + 20, 0);
    r1->props.resistor.resistance = 10000.0;

    // Feedback capacitor C1 (input to inverting)
    Component *c1 = add_comp(circuit, COMP_CAPACITOR, x + 240, y - 40, 0);
    c1->props.capacitor.capacitance = 10e-9;

    // Feedback resistor R2 (inverting to output)
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 320, y - 80, 0);
    r2->props.resistor.resistance = 10000.0;

    // Capacitor C2 (parallel with R2)
    Component *c2 = add_comp(circuit, COMP_CAPACITOR, x + 320, y - 120, 0);
    c2->props.capacitor.capacitance = 10e-9;

    // Op-amp
    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 320, y + 20, 0);
    opamp->props.opamp.ideal = true;

    // Non-inverting to ground
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 280, y + 80, 0);

    // === OUTPUT SECTION (right) ===
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 440, y + 60, 90);
    rload->props.resistor.resistance = 10000.0;
    Component *gnd_load = add_comp(circuit, COMP_GROUND, x + 440, y + 120, 0);
    connect_terminals(circuit, rload, 1, gnd_load, 0);

    // === WIRING ===
    // Source to R1
    float vsrc_pos_x, vsrc_pos_y;
    component_get_terminal_pos(vsrc, 0, &vsrc_pos_x, &vsrc_pos_y);
    float r1_left_x, r1_left_y;
    component_get_terminal_pos(r1, 0, &r1_left_x, &r1_left_y);

    int vsrc_node = circuit_find_or_create_node(circuit, vsrc_pos_x, vsrc_pos_y, 5.0f);
    int vsrc_corner = circuit_find_or_create_node(circuit, vsrc_pos_x, r1_left_y, 5.0f);
    int r1_left_node = circuit_find_or_create_node(circuit, r1_left_x, r1_left_y, 5.0f);
    circuit_add_wire(circuit, vsrc_node, vsrc_corner);
    circuit_add_wire(circuit, vsrc_corner, r1_left_node);
    vsrc->node_ids[0] = vsrc_node;
    r1->node_ids[0] = r1_left_node;

    // R1 to junction (inverting input area)
    float r1_right_x, r1_right_y;
    component_get_terminal_pos(r1, 1, &r1_right_x, &r1_right_y);
    float opamp_inv_x, opamp_inv_y;
    component_get_terminal_pos(opamp, 0, &opamp_inv_x, &opamp_inv_y);

    int inv_junc = circuit_find_or_create_node(circuit, r1_right_x, opamp_inv_y, 5.0f);
    int r1_right_node = circuit_find_or_create_node(circuit, r1_right_x, r1_right_y, 5.0f);
    circuit_add_wire(circuit, r1_right_node, inv_junc);
    r1->node_ids[1] = r1_right_node;

    // Junction to op-amp inverting
    int opamp_inv_node = circuit_find_or_create_node(circuit, opamp_inv_x, opamp_inv_y, 5.0f);
    circuit_add_wire(circuit, inv_junc, opamp_inv_node);
    opamp->node_ids[0] = opamp_inv_node;

    // C1 from junction up, then across
    float c1_left_x, c1_left_y;
    component_get_terminal_pos(c1, 0, &c1_left_x, &c1_left_y);
    float c1_right_x, c1_right_y;
    component_get_terminal_pos(c1, 1, &c1_right_x, &c1_right_y);

    int corner1 = circuit_find_or_create_node(circuit, r1_right_x, c1_left_y, 5.0f);
    int c1_left_node = circuit_find_or_create_node(circuit, c1_left_x, c1_left_y, 5.0f);
    circuit_add_wire(circuit, inv_junc, corner1);
    circuit_add_wire(circuit, corner1, c1_left_node);
    c1->node_ids[0] = c1_left_node;

    // R2 and C2 from junction up to feedback level
    float r2_left_x, r2_left_y;
    component_get_terminal_pos(r2, 0, &r2_left_x, &r2_left_y);
    float c2_left_x, c2_left_y;
    component_get_terminal_pos(c2, 0, &c2_left_x, &c2_left_y);

    int corner2 = circuit_find_or_create_node(circuit, opamp_inv_x, r2_left_y, 5.0f);
    int r2_left_node = circuit_find_or_create_node(circuit, r2_left_x, r2_left_y, 5.0f);
    int c2_left_node = circuit_find_or_create_node(circuit, c2_left_x, c2_left_y, 5.0f);
    circuit_add_wire(circuit, inv_junc, corner2);
    circuit_add_wire(circuit, corner2, r2_left_node);
    int corner_c2 = circuit_find_or_create_node(circuit, opamp_inv_x, c2_left_y, 5.0f);
    circuit_add_wire(circuit, corner2, corner_c2);
    circuit_add_wire(circuit, corner_c2, c2_left_node);
    r2->node_ids[0] = r2_left_node;
    c2->node_ids[0] = c2_left_node;

    // Op-amp non-inverting to ground
    float opamp_noninv_x, opamp_noninv_y;
    component_get_terminal_pos(opamp, 1, &opamp_noninv_x, &opamp_noninv_y);
    float gnd2_x, gnd2_y;
    component_get_terminal_pos(gnd2, 0, &gnd2_x, &gnd2_y);

    int opamp_noninv_node = circuit_find_or_create_node(circuit, opamp_noninv_x, opamp_noninv_y, 5.0f);
    int gnd2_node = circuit_find_or_create_node(circuit, gnd2_x, gnd2_y, 5.0f);
    int corner3 = circuit_find_or_create_node(circuit, gnd2_x, opamp_noninv_y, 5.0f);
    circuit_add_wire(circuit, opamp_noninv_node, corner3);
    circuit_add_wire(circuit, corner3, gnd2_node);
    opamp->node_ids[1] = opamp_noninv_node;
    gnd2->node_ids[0] = gnd2_node;

    // Output to R2, C2, C1 right, and load
    float opamp_out_x, opamp_out_y;
    component_get_terminal_pos(opamp, 2, &opamp_out_x, &opamp_out_y);
    float r2_right_x, r2_right_y;
    component_get_terminal_pos(r2, 1, &r2_right_x, &r2_right_y);
    float c2_right_x, c2_right_y;
    component_get_terminal_pos(c2, 1, &c2_right_x, &c2_right_y);
    float rload_top_x, rload_top_y;
    component_get_terminal_pos(rload, 0, &rload_top_x, &rload_top_y);

    int out_node = circuit_find_or_create_node(circuit, opamp_out_x, opamp_out_y, 5.0f);
    opamp->node_ids[2] = out_node;

    int out_junc = circuit_find_or_create_node(circuit, opamp_out_x + 20, opamp_out_y, 5.0f);
    int corner5 = circuit_find_or_create_node(circuit, opamp_out_x + 20, r2_right_y, 5.0f);
    int r2_right_node = circuit_find_or_create_node(circuit, r2_right_x, r2_right_y, 5.0f);
    circuit_add_wire(circuit, out_node, out_junc);
    circuit_add_wire(circuit, out_junc, corner5);
    circuit_add_wire(circuit, corner5, r2_right_node);
    r2->node_ids[1] = r2_right_node;

    int c2_right_node = circuit_find_or_create_node(circuit, c2_right_x, c2_right_y, 5.0f);
    int corner6 = circuit_find_or_create_node(circuit, opamp_out_x + 20, c2_right_y, 5.0f);
    circuit_add_wire(circuit, corner5, corner6);
    circuit_add_wire(circuit, corner6, c2_right_node);
    c2->node_ids[1] = c2_right_node;

    int c1_right_node = circuit_find_or_create_node(circuit, c1_right_x, c1_right_y, 5.0f);
    int corner7 = circuit_find_or_create_node(circuit, opamp_out_x + 20, c1_right_y, 5.0f);
    circuit_add_wire(circuit, corner6, corner7);
    circuit_add_wire(circuit, corner7, c1_right_node);
    c1->node_ids[1] = c1_right_node;

    // Output to load resistor
    int rload_top_node = circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f);
    int load_corner = circuit_find_or_create_node(circuit, rload_top_x, opamp_out_y, 5.0f);
    circuit_add_wire(circuit, out_junc, load_corner);
    circuit_add_wire(circuit, load_corner, rload_top_node);
    rload->node_ids[0] = rload_top_node;

    return 14;
}

// Twin-T Notch Filter (60Hz rejection)
static int place_notch_filter(Circuit *circuit, float x, float y) {
    // AC source
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 40, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 1.0;
    vsrc->props.ac_voltage.frequency = 60.0; // 60Hz notch

    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Twin-T network: R-C-R path on top, C-R-C path on bottom
    // Top path: R1-C1-R2
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 100, y - 40, 0);
    r1->props.resistor.resistance = 26525.0; // For 60Hz notch

    Component *c1 = add_comp(circuit, COMP_CAPACITOR, x + 200, y - 40, 0);
    c1->props.capacitor.capacitance = 100e-9;

    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 300, y - 40, 0);
    r2->props.resistor.resistance = 26525.0;

    // Bottom path: C2-R3-C3
    Component *c2 = add_comp(circuit, COMP_CAPACITOR, x + 100, y + 40, 0);
    c2->props.capacitor.capacitance = 100e-9;

    Component *r3 = add_comp(circuit, COMP_RESISTOR, x + 200, y + 40, 0);
    r3->props.resistor.resistance = 13262.0; // Half of R1/R2

    Component *c3 = add_comp(circuit, COMP_CAPACITOR, x + 300, y + 40, 0);
    c3->props.capacitor.capacitance = 100e-9;

    // Center connection to ground (through R4 for adjustable Q)
    Component *r4 = add_comp(circuit, COMP_RESISTOR, x + 200, y + 100, 90);
    r4->props.resistor.resistance = 10000.0;

    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 200, y + 160, 0);

    // Load resistor (10kΩ standard output load)
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 400, y + 40, 90);
    rload->props.resistor.resistance = 10000.0;

    Component *gnd3 = add_comp(circuit, COMP_GROUND, x + 400, y + 100, 0);

    // Connect source to ground
    connect_terminals(circuit, vsrc, 1, gnd1, 0);

    // R4 to ground
    connect_terminals(circuit, r4, 1, gnd2, 0);

    // Rload to ground
    connect_terminals(circuit, rload, 1, gnd3, 0);

    // Input junction
    float vsrc_pos_x, vsrc_pos_y;
    component_get_terminal_pos(vsrc, 0, &vsrc_pos_x, &vsrc_pos_y);

    int input_junc = circuit_find_or_create_node(circuit, x + 60, y, 5.0f);

    // Source to input junction
    int vsrc_node = circuit_find_or_create_node(circuit, vsrc_pos_x, vsrc_pos_y, 5.0f);
    int corner1 = circuit_find_or_create_node(circuit, vsrc_pos_x, y, 5.0f);
    circuit_add_wire(circuit, vsrc_node, corner1);
    circuit_add_wire(circuit, corner1, input_junc);
    vsrc->node_ids[0] = vsrc_node;

    // Input to top path (R1)
    float r1_left_x, r1_left_y;
    component_get_terminal_pos(r1, 0, &r1_left_x, &r1_left_y);
    int r1_left_node = circuit_find_or_create_node(circuit, r1_left_x, r1_left_y, 5.0f);
    int corner2 = circuit_find_or_create_node(circuit, x + 60, r1_left_y, 5.0f);
    circuit_add_wire(circuit, input_junc, corner2);
    circuit_add_wire(circuit, corner2, r1_left_node);
    r1->node_ids[0] = r1_left_node;

    // Input to bottom path (C2)
    float c2_left_x, c2_left_y;
    component_get_terminal_pos(c2, 0, &c2_left_x, &c2_left_y);
    int c2_left_node = circuit_find_or_create_node(circuit, c2_left_x, c2_left_y, 5.0f);
    int corner3 = circuit_find_or_create_node(circuit, x + 60, c2_left_y, 5.0f);
    circuit_add_wire(circuit, input_junc, corner3);
    circuit_add_wire(circuit, corner3, c2_left_node);
    c2->node_ids[0] = c2_left_node;

    // Top path connections
    connect_terminals(circuit, r1, 1, c1, 0);
    connect_terminals(circuit, c1, 1, r2, 0);

    // Bottom path connections
    connect_terminals(circuit, c2, 1, r3, 0);
    connect_terminals(circuit, r3, 1, c3, 0);

    // Center junction (C1-R2 junction and R3 center to R4)
    float c1_right_x, c1_right_y;
    component_get_terminal_pos(c1, 1, &c1_right_x, &c1_right_y);
    float r3_left_x, r3_left_y;
    component_get_terminal_pos(r3, 0, &r3_left_x, &r3_left_y);
    float r3_right_x, r3_right_y;
    component_get_terminal_pos(r3, 1, &r3_right_x, &r3_right_y);
    float r4_top_x, r4_top_y;
    component_get_terminal_pos(r4, 0, &r4_top_x, &r4_top_y);

    // Connect R3 center (average of left/right) to R4 and C1 junction
    // Actually, Twin-T: C1-R2 junction connects to R3 midpoint, which goes to ground via R4
    // Let's connect C1 right to R4 top
    int c1_right_node = circuit_find_or_create_node(circuit, c1_right_x, c1_right_y, 5.0f);
    int r4_top_node = circuit_find_or_create_node(circuit, r4_top_x, r4_top_y, 5.0f);
    int center_junc = circuit_find_or_create_node(circuit, r4_top_x, c1_right_y, 5.0f);
    circuit_add_wire(circuit, c1_right_node, center_junc);
    circuit_add_wire(circuit, center_junc, r4_top_node);
    c1->node_ids[1] = c1_right_node;
    r4->node_ids[0] = r4_top_node;

    // R3 right (which is same as C3 left junction) connects to center too
    int r3_right_node = circuit_find_or_create_node(circuit, r3_right_x, r3_right_y, 5.0f);
    int corner4 = circuit_find_or_create_node(circuit, r4_top_x, r3_right_y, 5.0f);
    circuit_add_wire(circuit, r3_right_node, corner4);
    circuit_add_wire(circuit, corner4, r4_top_node);
    r3->node_ids[1] = r3_right_node;

    // Output junction
    int output_junc = circuit_find_or_create_node(circuit, x + 360, y, 5.0f);

    // R2 right to output
    float r2_right_x, r2_right_y;
    component_get_terminal_pos(r2, 1, &r2_right_x, &r2_right_y);
    int r2_right_node = circuit_find_or_create_node(circuit, r2_right_x, r2_right_y, 5.0f);
    int corner5 = circuit_find_or_create_node(circuit, x + 360, r2_right_y, 5.0f);
    circuit_add_wire(circuit, r2_right_node, corner5);
    circuit_add_wire(circuit, corner5, output_junc);
    r2->node_ids[1] = r2_right_node;

    // C3 right to output
    float c3_right_x, c3_right_y;
    component_get_terminal_pos(c3, 1, &c3_right_x, &c3_right_y);
    int c3_right_node = circuit_find_or_create_node(circuit, c3_right_x, c3_right_y, 5.0f);
    int corner6 = circuit_find_or_create_node(circuit, x + 360, c3_right_y, 5.0f);
    circuit_add_wire(circuit, c3_right_node, corner6);
    circuit_add_wire(circuit, corner6, output_junc);
    c3->node_ids[1] = c3_right_node;

    // Output to load
    float rload_top_x, rload_top_y;
    component_get_terminal_pos(rload, 0, &rload_top_x, &rload_top_y);
    int rload_top_node = circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f);
    circuit_add_wire(circuit, output_junc, rload_top_node);
    rload->node_ids[0] = rload_top_node;

    return 12;
}

// Wien Bridge Oscillator
static int place_wien_oscillator(Circuit *circuit, float x, float y) {
    // Wien bridge with op-amp and amplitude limiting
    // Oscillation frequency: f = 1/(2*pi*R*C)

    // === POWER SUPPLY SECTION (top) ===
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x, y - 100, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 12.0;

    Component *gnd_vcc = add_comp(circuit, COMP_GROUND, x, y - 40, 0);

    // Connect power supply
    connect_terminals(circuit, vcc, 1, gnd_vcc, 0);

    // Decoupling capacitor
    Component *c_dec = add_comp(circuit, COMP_CAPACITOR, x + 60, y - 80, 90);
    c_dec->props.capacitor.capacitance = 0.1e-6;  // 0.1uF decoupling

    // Wire decoupling cap to power rail
    float vcc_pos_x, vcc_pos_y;
    component_get_terminal_pos(vcc, 0, &vcc_pos_x, &vcc_pos_y);
    float cdec_top_x, cdec_top_y;
    component_get_terminal_pos(c_dec, 0, &cdec_top_x, &cdec_top_y);
    float cdec_bot_x, cdec_bot_y;
    component_get_terminal_pos(c_dec, 1, &cdec_bot_x, &cdec_bot_y);

    int vcc_node = circuit_find_or_create_node(circuit, vcc_pos_x, vcc_pos_y, 5.0f);
    int vcc_rail = circuit_find_or_create_node(circuit, vcc_pos_x, y - 120, 5.0f);
    circuit_add_wire(circuit, vcc_node, vcc_rail);
    vcc->node_ids[0] = vcc_node;

    int cdec_top_node = circuit_find_or_create_node(circuit, cdec_top_x, cdec_top_y, 5.0f);
    int corner_vcc = circuit_find_or_create_node(circuit, cdec_top_x, y - 120, 5.0f);
    circuit_add_wire(circuit, vcc_rail, corner_vcc);
    circuit_add_wire(circuit, corner_vcc, cdec_top_node);
    c_dec->node_ids[0] = cdec_top_node;

    // Decoupling cap ground
    Component *gnd_dec = add_comp(circuit, COMP_GROUND, x + 60, y - 20, 0);
    connect_terminals(circuit, c_dec, 1, gnd_dec, 0);

    // === OP-AMP SECTION ===
    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 200, y + 40, 0);
    opamp->props.opamp.ideal = true;

    // Negative feedback network (gain = 3 for oscillation)
    Component *rf = add_comp(circuit, COMP_RESISTOR, x + 200, y - 20, 0);
    rf->props.resistor.resistance = 20000.0; // Rf

    Component *rg = add_comp(circuit, COMP_RESISTOR, x + 120, y + 20, 90);
    rg->props.resistor.resistance = 10000.0; // Rg, gain = 1 + Rf/Rg = 3

    Component *gnd_rg = add_comp(circuit, COMP_GROUND, x + 120, y + 80, 0);
    connect_terminals(circuit, rg, 1, gnd_rg, 0);

    // Wien bridge network (positive feedback for oscillation)
    // Series RC from output
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 300, y + 60, 0);
    r1->props.resistor.resistance = 10000.0;

    Component *c1 = add_comp(circuit, COMP_CAPACITOR, x + 380, y + 60, 0);
    c1->props.capacitor.capacitance = 10e-9; // ~1.6kHz

    // Parallel RC to ground
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 120, y + 120, 90);
    r2->props.resistor.resistance = 10000.0;

    Component *c2 = add_comp(circuit, COMP_CAPACITOR, x + 160, y + 120, 90);
    c2->props.capacitor.capacitance = 10e-9;

    Component *gnd_r2 = add_comp(circuit, COMP_GROUND, x + 120, y + 180, 0);
    Component *gnd_c2 = add_comp(circuit, COMP_GROUND, x + 160, y + 180, 0);
    connect_terminals(circuit, r2, 1, gnd_r2, 0);
    connect_terminals(circuit, c2, 1, gnd_c2, 0);

    // === OUTPUT SECTION (right side) ===
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 440, y + 100, 90);
    rload->props.resistor.resistance = 10000.0;  // 10kΩ output load

    Component *gnd_load = add_comp(circuit, COMP_GROUND, x + 440, y + 160, 0);
    connect_terminals(circuit, rload, 1, gnd_load, 0);

    // === WIRING ===
    // Op-amp terminals
    float opamp_inv_x, opamp_inv_y;
    component_get_terminal_pos(opamp, 0, &opamp_inv_x, &opamp_inv_y);
    float opamp_noninv_x, opamp_noninv_y;
    component_get_terminal_pos(opamp, 1, &opamp_noninv_x, &opamp_noninv_y);
    float opamp_out_x, opamp_out_y;
    component_get_terminal_pos(opamp, 2, &opamp_out_x, &opamp_out_y);

    int inv_node = circuit_find_or_create_node(circuit, opamp_inv_x, opamp_inv_y, 5.0f);
    int noninv_node = circuit_find_or_create_node(circuit, opamp_noninv_x, opamp_noninv_y, 5.0f);
    int out_node = circuit_find_or_create_node(circuit, opamp_out_x, opamp_out_y, 5.0f);
    opamp->node_ids[0] = inv_node;
    opamp->node_ids[1] = noninv_node;
    opamp->node_ids[2] = out_node;

    // Rg top to inverting input
    float rg_top_x, rg_top_y;
    component_get_terminal_pos(rg, 0, &rg_top_x, &rg_top_y);
    int rg_top_node = circuit_find_or_create_node(circuit, rg_top_x, rg_top_y, 5.0f);
    int corner_inv = circuit_find_or_create_node(circuit, rg_top_x, opamp_inv_y, 5.0f);
    circuit_add_wire(circuit, rg_top_node, corner_inv);
    circuit_add_wire(circuit, corner_inv, inv_node);
    rg->node_ids[0] = rg_top_node;

    // Rf left to inverting junction
    float rf_left_x, rf_left_y;
    component_get_terminal_pos(rf, 0, &rf_left_x, &rf_left_y);
    float rf_right_x, rf_right_y;
    component_get_terminal_pos(rf, 1, &rf_right_x, &rf_right_y);
    int rf_left_node = circuit_find_or_create_node(circuit, rf_left_x, rf_left_y, 5.0f);
    int corner_rf = circuit_find_or_create_node(circuit, rg_top_x, rf_left_y, 5.0f);
    circuit_add_wire(circuit, corner_inv, corner_rf);
    circuit_add_wire(circuit, corner_rf, rf_left_node);
    rf->node_ids[0] = rf_left_node;

    // Rf right to output
    int rf_right_node = circuit_find_or_create_node(circuit, rf_right_x, rf_right_y, 5.0f);
    int corner_rf_out = circuit_find_or_create_node(circuit, rf_right_x, opamp_out_y, 5.0f);
    circuit_add_wire(circuit, rf_right_node, corner_rf_out);
    circuit_add_wire(circuit, corner_rf_out, out_node);
    rf->node_ids[1] = rf_right_node;

    // R1 from output
    float r1_left_x, r1_left_y;
    component_get_terminal_pos(r1, 0, &r1_left_x, &r1_left_y);
    int r1_left_node = circuit_find_or_create_node(circuit, r1_left_x, r1_left_y, 5.0f);
    int corner_r1 = circuit_find_or_create_node(circuit, opamp_out_x, r1_left_y, 5.0f);
    circuit_add_wire(circuit, out_node, corner_r1);
    circuit_add_wire(circuit, corner_r1, r1_left_node);
    r1->node_ids[0] = r1_left_node;

    // R1 to C1
    connect_terminals(circuit, r1, 1, c1, 0);

    // C1 back to non-inverting input junction
    float c1_right_x, c1_right_y;
    component_get_terminal_pos(c1, 1, &c1_right_x, &c1_right_y);
    int c1_right_node = circuit_find_or_create_node(circuit, c1_right_x, c1_right_y, 5.0f);
    c1->node_ids[1] = c1_right_node;

    // Feedback path from C1 to non-inverting input (wrap around)
    int corner_fb1 = circuit_find_or_create_node(circuit, c1_right_x, y + 200, 5.0f);
    int corner_fb2 = circuit_find_or_create_node(circuit, x + 80, y + 200, 5.0f);
    int corner_fb3 = circuit_find_or_create_node(circuit, x + 80, opamp_noninv_y, 5.0f);
    circuit_add_wire(circuit, c1_right_node, corner_fb1);
    circuit_add_wire(circuit, corner_fb1, corner_fb2);
    circuit_add_wire(circuit, corner_fb2, corner_fb3);
    circuit_add_wire(circuit, corner_fb3, noninv_node);

    // Parallel RC (R2, C2) from non-inverting junction to ground
    float r2_top_x, r2_top_y;
    component_get_terminal_pos(r2, 0, &r2_top_x, &r2_top_y);
    float c2_top_x, c2_top_y;
    component_get_terminal_pos(c2, 0, &c2_top_x, &c2_top_y);

    int r2_top_node = circuit_find_or_create_node(circuit, r2_top_x, r2_top_y, 5.0f);
    int c2_top_node = circuit_find_or_create_node(circuit, c2_top_x, c2_top_y, 5.0f);

    // Connect non-inverting to parallel RC
    int corner_rc = circuit_find_or_create_node(circuit, r2_top_x, opamp_noninv_y, 5.0f);
    circuit_add_wire(circuit, noninv_node, corner_rc);
    circuit_add_wire(circuit, corner_rc, r2_top_node);
    r2->node_ids[0] = r2_top_node;

    int corner_c2 = circuit_find_or_create_node(circuit, c2_top_x, opamp_noninv_y, 5.0f);
    circuit_add_wire(circuit, corner_rc, corner_c2);
    circuit_add_wire(circuit, corner_c2, c2_top_node);
    c2->node_ids[0] = c2_top_node;

    // Output to load resistor
    float rload_top_x, rload_top_y;
    component_get_terminal_pos(rload, 0, &rload_top_x, &rload_top_y);
    int rload_top_node = circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f);
    int corner_load = circuit_find_or_create_node(circuit, rload_top_x, opamp_out_y, 5.0f);
    circuit_add_wire(circuit, out_node, corner_load);
    circuit_add_wire(circuit, corner_load, rload_top_node);
    rload->node_ids[0] = rload_top_node;

    return 16;
}

// BJT Constant Current Source
static int place_current_source(Circuit *circuit, float x, float y) {
    // Simple current source using BJT and voltage reference
    // I_out = (Vref - Vbe) / Re

    // === POWER SUPPLY SECTION ===
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x, y - 40, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 12.0;

    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, y + 20, 0);

    // Decoupling capacitor
    Component *c_dec = add_comp(circuit, COMP_CAPACITOR, x + 40, y - 80, 90);
    c_dec->props.capacitor.capacitance = 0.1e-6;  // 0.1uF decoupling

    Component *gnd_dec = add_comp(circuit, COMP_GROUND, x + 40, y - 20, 0);

    // Reference voltage divider
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 80, y - 80, 90);
    r1->props.resistor.resistance = 10000.0;

    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 80, y, 90);
    r2->props.resistor.resistance = 2200.0;

    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 80, y + 60, 0);

    // NPN transistor
    Component *q1 = add_comp(circuit, COMP_NPN_BJT, x + 160, y - 40, 0);
    q1->props.bjt.bf = 100;

    // Emitter resistor (sets current)
    Component *re = add_comp(circuit, COMP_RESISTOR, x + 180, y + 20, 90);
    re->props.resistor.resistance = 470.0; // ~2mA with Vref ~1.8V

    Component *gnd3 = add_comp(circuit, COMP_GROUND, x + 180, y + 80, 0);

    // Load resistor (collector load)
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 180, y - 100, 90);
    rload->props.resistor.resistance = 1000.0;

    // Connect Vcc to ground
    connect_terminals(circuit, vcc, 1, gnd1, 0);

    // R2 to ground
    connect_terminals(circuit, r2, 1, gnd2, 0);

    // Re to ground
    connect_terminals(circuit, re, 1, gnd3, 0);

    // Decoupling cap to ground
    connect_terminals(circuit, c_dec, 1, gnd_dec, 0);

    // Vcc+ to R1 top and Rload top
    float vcc_pos_x, vcc_pos_y;
    component_get_terminal_pos(vcc, 0, &vcc_pos_x, &vcc_pos_y);
    float r1_top_x, r1_top_y;
    component_get_terminal_pos(r1, 0, &r1_top_x, &r1_top_y);
    float rload_top_x, rload_top_y;
    component_get_terminal_pos(rload, 0, &rload_top_x, &rload_top_y);
    float cdec_top_x, cdec_top_y;
    component_get_terminal_pos(c_dec, 0, &cdec_top_x, &cdec_top_y);

    int vcc_rail = circuit_find_or_create_node(circuit, vcc_pos_x, y - 120, 5.0f);
    int vcc_node = circuit_find_or_create_node(circuit, vcc_pos_x, vcc_pos_y, 5.0f);
    circuit_add_wire(circuit, vcc_node, vcc_rail);
    vcc->node_ids[0] = vcc_node;

    // Decoupling cap to power rail
    int cdec_top_node = circuit_find_or_create_node(circuit, cdec_top_x, cdec_top_y, 5.0f);
    int corner_dec = circuit_find_or_create_node(circuit, cdec_top_x, y - 120, 5.0f);
    circuit_add_wire(circuit, vcc_rail, corner_dec);
    circuit_add_wire(circuit, corner_dec, cdec_top_node);
    c_dec->node_ids[0] = cdec_top_node;

    int r1_top_node = circuit_find_or_create_node(circuit, r1_top_x, r1_top_y, 5.0f);
    int corner1 = circuit_find_or_create_node(circuit, r1_top_x, y - 120, 5.0f);
    circuit_add_wire(circuit, corner_dec, corner1);
    circuit_add_wire(circuit, corner1, r1_top_node);
    r1->node_ids[0] = r1_top_node;

    int rload_top_node = circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f);
    int corner2 = circuit_find_or_create_node(circuit, rload_top_x, y - 120, 5.0f);
    circuit_add_wire(circuit, corner1, corner2);
    circuit_add_wire(circuit, corner2, rload_top_node);
    rload->node_ids[0] = rload_top_node;

    // R1-R2 junction to base
    float r1_bot_x, r1_bot_y;
    component_get_terminal_pos(r1, 1, &r1_bot_x, &r1_bot_y);
    float r2_top_x, r2_top_y;
    component_get_terminal_pos(r2, 0, &r2_top_x, &r2_top_y);
    float base_x, base_y;
    component_get_terminal_pos(q1, 0, &base_x, &base_y);

    int bias_junc = circuit_find_or_create_node(circuit, r1_bot_x, r1_bot_y, 5.0f);
    int r2_top_node = circuit_find_or_create_node(circuit, r2_top_x, r2_top_y, 5.0f);
    circuit_add_wire(circuit, bias_junc, r2_top_node);
    r1->node_ids[1] = bias_junc;
    r2->node_ids[0] = r2_top_node;

    int base_node = circuit_find_or_create_node(circuit, base_x, base_y, 5.0f);
    circuit_add_wire(circuit, bias_junc, base_node);
    q1->node_ids[0] = base_node;

    // Collector to Rload
    float coll_x, coll_y;
    component_get_terminal_pos(q1, 1, &coll_x, &coll_y);
    float rload_bot_x, rload_bot_y;
    component_get_terminal_pos(rload, 1, &rload_bot_x, &rload_bot_y);

    int coll_node = circuit_find_or_create_node(circuit, coll_x, coll_y, 5.0f);
    int rload_bot_node = circuit_find_or_create_node(circuit, rload_bot_x, rload_bot_y, 5.0f);
    circuit_add_wire(circuit, coll_node, rload_bot_node);
    q1->node_ids[1] = coll_node;
    rload->node_ids[1] = rload_bot_node;

    // Emitter to Re
    float emit_x, emit_y;
    component_get_terminal_pos(q1, 2, &emit_x, &emit_y);
    float re_top_x, re_top_y;
    component_get_terminal_pos(re, 0, &re_top_x, &re_top_y);

    int emit_node = circuit_find_or_create_node(circuit, emit_x, emit_y, 5.0f);
    int re_top_node = circuit_find_or_create_node(circuit, re_top_x, re_top_y, 5.0f);
    circuit_add_wire(circuit, emit_node, re_top_node);
    q1->node_ids[2] = emit_node;
    re->node_ids[0] = re_top_node;

    return 11;  // vcc, gnd1, c_dec, gnd_dec, r1, r2, gnd2, q1, re, gnd3, rload
}

// Window Comparator (Overvoltage/Undervoltage detection)
static int place_window_comp(Circuit *circuit, float x, float y) {
    // Two comparators: one for high threshold, one for low threshold
    // Output goes low if input is outside window

    // Input voltage (to be monitored)
    Component *vin = add_comp(circuit, COMP_DC_VOLTAGE, x, y + 40, 0);
    if (!vin) return 0;
    vin->props.dc_voltage.voltage = 2.5; // Mid-range

    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Reference voltage supply
    Component *vref = add_comp(circuit, COMP_DC_VOLTAGE, x + 80, y - 100, 0);
    vref->props.dc_voltage.voltage = 5.0;

    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 80, y - 40, 0);

    // Decoupling capacitor for power supply
    Component *c_dec = add_comp(circuit, COMP_CAPACITOR, x + 120, y - 80, 90);
    c_dec->props.capacitor.capacitance = 0.1e-6;  // 0.1uF decoupling

    Component *gnd_dec = add_comp(circuit, COMP_GROUND, x + 120, y - 20, 0);

    // Voltage divider for thresholds
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 160, y - 140, 90);
    r1->props.resistor.resistance = 10000.0;

    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 160, y - 60, 90);
    r2->props.resistor.resistance = 10000.0;

    Component *r3 = add_comp(circuit, COMP_RESISTOR, x + 160, y + 20, 90);
    r3->props.resistor.resistance = 10000.0;

    Component *gnd3 = add_comp(circuit, COMP_GROUND, x + 160, y + 80, 0);

    // High comparator (input > high threshold -> output low)
    Component *comp_hi = add_comp(circuit, COMP_OPAMP, x + 280, y - 100, 0);
    comp_hi->props.opamp.ideal = true;

    // Low comparator (input < low threshold -> output low)
    Component *comp_lo = add_comp(circuit, COMP_OPAMP, x + 280, y + 40, 0);
    comp_lo->props.opamp.ideal = true;

    // Pull-up resistor for output
    Component *rpu = add_comp(circuit, COMP_RESISTOR, x + 400, y - 60, 90);
    rpu->props.resistor.resistance = 10000.0;

    // LED indicator
    Component *led = add_comp(circuit, COMP_LED, x + 400, y + 20, 90);

    Component *gnd4 = add_comp(circuit, COMP_GROUND, x + 400, y + 80, 0);

    // Ground connections
    connect_terminals(circuit, vin, 1, gnd1, 0);
    connect_terminals(circuit, vref, 1, gnd2, 0);
    connect_terminals(circuit, c_dec, 1, gnd_dec, 0);
    connect_terminals(circuit, r3, 1, gnd3, 0);
    connect_terminals(circuit, led, 1, gnd4, 0);

    // Vref to R1 top and decoupling cap
    float vref_pos_x, vref_pos_y;
    component_get_terminal_pos(vref, 0, &vref_pos_x, &vref_pos_y);
    float r1_top_x, r1_top_y;
    component_get_terminal_pos(r1, 0, &r1_top_x, &r1_top_y);
    float cdec_top_x, cdec_top_y;
    component_get_terminal_pos(c_dec, 0, &cdec_top_x, &cdec_top_y);

    // Create power rail at y - 160
    int vref_node = circuit_find_or_create_node(circuit, vref_pos_x, vref_pos_y, 5.0f);
    int vref_rail = circuit_find_or_create_node(circuit, vref_pos_x, y - 160, 5.0f);
    circuit_add_wire(circuit, vref_node, vref_rail);
    vref->node_ids[0] = vref_node;

    // Decoupling cap to rail
    int cdec_node = circuit_find_or_create_node(circuit, cdec_top_x, cdec_top_y, 5.0f);
    int corner_dec = circuit_find_or_create_node(circuit, cdec_top_x, y - 160, 5.0f);
    circuit_add_wire(circuit, vref_rail, corner_dec);
    circuit_add_wire(circuit, corner_dec, cdec_node);
    c_dec->node_ids[0] = cdec_node;

    // R1 to rail
    int r1_top_node = circuit_find_or_create_node(circuit, r1_top_x, r1_top_y, 5.0f);
    int corner_r1 = circuit_find_or_create_node(circuit, r1_top_x, y - 160, 5.0f);
    circuit_add_wire(circuit, corner_dec, corner_r1);
    circuit_add_wire(circuit, corner_r1, r1_top_node);
    r1->node_ids[0] = r1_top_node;

    // Divider chain
    connect_terminals(circuit, r1, 1, r2, 0);
    connect_terminals(circuit, r2, 1, r3, 0);

    // High threshold (R1-R2 junction) to comp_hi non-inverting
    float r1_bot_x, r1_bot_y;
    component_get_terminal_pos(r1, 1, &r1_bot_x, &r1_bot_y);
    float comp_hi_noninv_x, comp_hi_noninv_y;
    component_get_terminal_pos(comp_hi, 1, &comp_hi_noninv_x, &comp_hi_noninv_y);

    wire_L_shape(circuit, r1_bot_x, r1_bot_y, comp_hi_noninv_x, comp_hi_noninv_y, true);

    // Low threshold (R2-R3 junction) to comp_lo inverting
    float r2_bot_x, r2_bot_y;
    component_get_terminal_pos(r2, 1, &r2_bot_x, &r2_bot_y);
    float comp_lo_inv_x, comp_lo_inv_y;
    component_get_terminal_pos(comp_lo, 0, &comp_lo_inv_x, &comp_lo_inv_y);

    wire_L_shape(circuit, r2_bot_x, r2_bot_y, comp_lo_inv_x, comp_lo_inv_y, true);

    // Input to both comparators
    float vin_pos_x, vin_pos_y;
    component_get_terminal_pos(vin, 0, &vin_pos_x, &vin_pos_y);
    float comp_hi_inv_x, comp_hi_inv_y;
    component_get_terminal_pos(comp_hi, 0, &comp_hi_inv_x, &comp_hi_inv_y);
    float comp_lo_noninv_x, comp_lo_noninv_y;
    component_get_terminal_pos(comp_lo, 1, &comp_lo_noninv_x, &comp_lo_noninv_y);

    int vin_node = circuit_find_or_create_node(circuit, vin_pos_x, vin_pos_y, 5.0f);
    int input_junc = circuit_find_or_create_node(circuit, x + 60, y, 5.0f);
    int corner1 = circuit_find_or_create_node(circuit, vin_pos_x, y, 5.0f);
    circuit_add_wire(circuit, vin_node, corner1);
    circuit_add_wire(circuit, corner1, input_junc);
    vin->node_ids[0] = vin_node;

    // To comp_hi inverting
    wire_L_shape(circuit, x + 60, y, comp_hi_inv_x, comp_hi_inv_y, false);

    // To comp_lo non-inverting
    wire_L_shape(circuit, x + 60, y, comp_lo_noninv_x, comp_lo_noninv_y, false);

    // Outputs wired-OR (both must be high for LED to light)
    // Simplified: connect outputs together through pull-up
    float comp_hi_out_x, comp_hi_out_y;
    component_get_terminal_pos(comp_hi, 2, &comp_hi_out_x, &comp_hi_out_y);
    float comp_lo_out_x, comp_lo_out_y;
    component_get_terminal_pos(comp_lo, 2, &comp_lo_out_x, &comp_lo_out_y);
    float rpu_bot_x, rpu_bot_y;
    component_get_terminal_pos(rpu, 1, &rpu_bot_x, &rpu_bot_y);
    float led_top_x, led_top_y;
    component_get_terminal_pos(led, 0, &led_top_x, &led_top_y);

    int out_junc = circuit_find_or_create_node(circuit, x + 360, y - 20, 5.0f);

    wire_L_shape(circuit, comp_hi_out_x, comp_hi_out_y, x + 360, y - 20, true);
    wire_L_shape(circuit, comp_lo_out_x, comp_lo_out_y, x + 360, y - 20, true);

    int rpu_bot_node = circuit_find_or_create_node(circuit, rpu_bot_x, rpu_bot_y, 5.0f);
    circuit_add_wire(circuit, out_junc, rpu_bot_node);
    rpu->node_ids[1] = rpu_bot_node;

    int led_top_node = circuit_find_or_create_node(circuit, led_top_x, led_top_y, 5.0f);
    circuit_add_wire(circuit, out_junc, led_top_node);
    led->node_ids[0] = led_top_node;

    // Pull-up to Vref
    float rpu_top_x, rpu_top_y;
    component_get_terminal_pos(rpu, 0, &rpu_top_x, &rpu_top_y);

    wire_L_shape(circuit, rpu_top_x, rpu_top_y, vref_pos_x, vref_pos_y, false);

    return 16;  // Added c_dec and gnd_dec
}

// Schmitt Trigger (Comparator with Hysteresis)
static int place_hysteresis_comp(Circuit *circuit, float x, float y) {
    // Non-inverting Schmitt trigger with positive feedback
    // Clean layout: power top-left, input left, op-amp center, output right

    // === POWER SUPPLY SECTION (top-left) ===
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x, y - 100, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 12.0;

    Component *gnd_vcc = add_comp(circuit, COMP_GROUND, x, y - 40, 0);
    connect_terminals(circuit, vcc, 1, gnd_vcc, 0);

    // Decoupling capacitor (near power supply, right of VCC)
    Component *c_dec = add_comp(circuit, COMP_CAPACITOR, x + 60, y - 80, 90);
    c_dec->props.capacitor.capacitance = 0.1e-6;  // 0.1uF

    Component *gnd_dec = add_comp(circuit, COMP_GROUND, x + 60, y - 20, 0);
    connect_terminals(circuit, c_dec, 1, gnd_dec, 0);

    // Wire decoupling cap to power rail
    float vcc_pos_x, vcc_pos_y;
    component_get_terminal_pos(vcc, 0, &vcc_pos_x, &vcc_pos_y);
    float cdec_top_x, cdec_top_y;
    component_get_terminal_pos(c_dec, 0, &cdec_top_x, &cdec_top_y);

    int vcc_node = circuit_find_or_create_node(circuit, vcc_pos_x, vcc_pos_y, 5.0f);
    int vcc_rail = circuit_find_or_create_node(circuit, vcc_pos_x, y - 120, 5.0f);
    circuit_add_wire(circuit, vcc_node, vcc_rail);
    vcc->node_ids[0] = vcc_node;

    int cdec_top_node = circuit_find_or_create_node(circuit, cdec_top_x, cdec_top_y, 5.0f);
    int corner_vcc = circuit_find_or_create_node(circuit, cdec_top_x, y - 120, 5.0f);
    circuit_add_wire(circuit, vcc_rail, corner_vcc);
    circuit_add_wire(circuit, corner_vcc, cdec_top_node);
    c_dec->node_ids[0] = cdec_top_node;

    // === INPUT SECTION (left side) ===
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 60, 0);
    vin->props.ac_voltage.amplitude = 3.0;
    vin->props.ac_voltage.frequency = 100.0;

    Component *gnd_in = add_comp(circuit, COMP_GROUND, x, y + 120, 0);
    connect_terminals(circuit, vin, 1, gnd_in, 0);

    // Input resistor (horizontal)
    Component *rin = add_comp(circuit, COMP_RESISTOR, x + 100, y + 20, 0);
    rin->props.resistor.resistance = 10000.0;

    // === OP-AMP SECTION (center) ===
    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 280, y + 20, 0);
    opamp->props.opamp.ideal = true;

    // Positive feedback resistor (above op-amp, sets hysteresis)
    Component *rf = add_comp(circuit, COMP_RESISTOR, x + 280, y - 40, 0);
    rf->props.resistor.resistance = 100000.0;  // 100kΩ for hysteresis

    // === REFERENCE DIVIDER (to the left of inverting input, vertical stack) ===
    // Position divider far enough left to avoid crossing the op-amp
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 180, y - 40, 90);  // Top resistor
    r1->props.resistor.resistance = 10000.0;

    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 180, y + 40, 90);  // Bottom resistor
    r2->props.resistor.resistance = 10000.0;

    Component *gnd_ref = add_comp(circuit, COMP_GROUND, x + 180, y + 100, 0);
    connect_terminals(circuit, r2, 1, gnd_ref, 0);

    // === OUTPUT SECTION (right side) ===
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 400, y + 60, 90);
    rload->props.resistor.resistance = 10000.0;

    Component *gnd_load = add_comp(circuit, COMP_GROUND, x + 400, y + 120, 0);
    connect_terminals(circuit, rload, 1, gnd_load, 0);

    // === WIRING ===

    // Input source to Rin
    float vin_pos_x, vin_pos_y;
    component_get_terminal_pos(vin, 0, &vin_pos_x, &vin_pos_y);
    float rin_left_x, rin_left_y;
    component_get_terminal_pos(rin, 0, &rin_left_x, &rin_left_y);

    wire_L_shape(circuit, vin_pos_x, vin_pos_y, rin_left_x, rin_left_y, true);

    // Rin right to non-inverting input
    float rin_right_x, rin_right_y;
    component_get_terminal_pos(rin, 1, &rin_right_x, &rin_right_y);
    float opamp_noninv_x, opamp_noninv_y;
    component_get_terminal_pos(opamp, 1, &opamp_noninv_x, &opamp_noninv_y);

    int rin_right_node = circuit_find_or_create_node(circuit, rin_right_x, rin_right_y, 5.0f);
    int noninv_junc = circuit_find_or_create_node(circuit, rin_right_x + 20, opamp_noninv_y, 5.0f);
    int corner_in = circuit_find_or_create_node(circuit, rin_right_x + 20, rin_right_y, 5.0f);
    circuit_add_wire(circuit, rin_right_node, corner_in);
    circuit_add_wire(circuit, corner_in, noninv_junc);
    rin->node_ids[1] = rin_right_node;

    int opamp_noninv_node = circuit_find_or_create_node(circuit, opamp_noninv_x, opamp_noninv_y, 5.0f);
    circuit_add_wire(circuit, noninv_junc, opamp_noninv_node);
    opamp->node_ids[1] = opamp_noninv_node;

    // Rf left to non-inverting junction (positive feedback)
    float rf_left_x, rf_left_y;
    component_get_terminal_pos(rf, 0, &rf_left_x, &rf_left_y);

    int rf_left_node = circuit_find_or_create_node(circuit, rf_left_x, rf_left_y, 5.0f);
    int corner_rf = circuit_find_or_create_node(circuit, rin_right_x + 20, rf_left_y, 5.0f);
    circuit_add_wire(circuit, noninv_junc, corner_rf);
    circuit_add_wire(circuit, corner_rf, rf_left_node);
    rf->node_ids[0] = rf_left_node;

    // Rf right to output
    float rf_right_x, rf_right_y;
    component_get_terminal_pos(rf, 1, &rf_right_x, &rf_right_y);
    float opamp_out_x, opamp_out_y;
    component_get_terminal_pos(opamp, 2, &opamp_out_x, &opamp_out_y);

    int rf_right_node = circuit_find_or_create_node(circuit, rf_right_x, rf_right_y, 5.0f);
    int out_node = circuit_find_or_create_node(circuit, opamp_out_x, opamp_out_y, 5.0f);
    int corner_out_top = circuit_find_or_create_node(circuit, rf_right_x, opamp_out_y, 5.0f);
    circuit_add_wire(circuit, rf_right_node, corner_out_top);
    circuit_add_wire(circuit, corner_out_top, out_node);
    rf->node_ids[1] = rf_right_node;
    opamp->node_ids[2] = out_node;

    // Power rail to R1 top
    float r1_top_x, r1_top_y;
    component_get_terminal_pos(r1, 0, &r1_top_x, &r1_top_y);

    int r1_top_node = circuit_find_or_create_node(circuit, r1_top_x, r1_top_y, 5.0f);
    int corner_pwr = circuit_find_or_create_node(circuit, r1_top_x, y - 120, 5.0f);
    circuit_add_wire(circuit, corner_vcc, corner_pwr);
    circuit_add_wire(circuit, corner_pwr, r1_top_node);
    r1->node_ids[0] = r1_top_node;

    // R1-R2 junction to inverting input
    float r1_bot_x, r1_bot_y;
    component_get_terminal_pos(r1, 1, &r1_bot_x, &r1_bot_y);
    float r2_top_x, r2_top_y;
    component_get_terminal_pos(r2, 0, &r2_top_x, &r2_top_y);
    float opamp_inv_x, opamp_inv_y;
    component_get_terminal_pos(opamp, 0, &opamp_inv_x, &opamp_inv_y);

    int ref_junc = circuit_find_or_create_node(circuit, r1_bot_x, r1_bot_y, 5.0f);
    int r2_top_node = circuit_find_or_create_node(circuit, r2_top_x, r2_top_y, 5.0f);
    circuit_add_wire(circuit, ref_junc, r2_top_node);
    r1->node_ids[1] = ref_junc;
    r2->node_ids[0] = r2_top_node;

    int opamp_inv_node = circuit_find_or_create_node(circuit, opamp_inv_x, opamp_inv_y, 5.0f);
    int corner_inv = circuit_find_or_create_node(circuit, r1_bot_x, opamp_inv_y, 5.0f);
    circuit_add_wire(circuit, ref_junc, corner_inv);
    circuit_add_wire(circuit, corner_inv, opamp_inv_node);
    opamp->node_ids[0] = opamp_inv_node;

    // Output to load resistor
    float rload_top_x, rload_top_y;
    component_get_terminal_pos(rload, 0, &rload_top_x, &rload_top_y);

    int rload_top_node = circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f);
    int corner_load = circuit_find_or_create_node(circuit, rload_top_x, opamp_out_y, 5.0f);
    circuit_add_wire(circuit, out_node, corner_load);
    circuit_add_wire(circuit, corner_load, rload_top_node);
    rload->node_ids[0] = rload_top_node;

    // Total: vcc, gnd_vcc, c_dec, gnd_dec, vin, gnd_in, rin, opamp, rf, r1, r2, gnd_ref, rload, gnd_load
    return 14;
}

// Zener Voltage Reference
static int place_zener_ref(Circuit *circuit, float x, float y) {
    // Simple Zener reference with current limiting resistor

    // Power supply
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x, y - 40, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 12.0;

    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, y + 20, 0);

    // Current limiting resistor
    Component *rs = add_comp(circuit, COMP_RESISTOR, x + 80, y - 80, 90);
    rs->props.resistor.resistance = 1000.0; // Limits Zener current

    // Zener diode (5.1V reference)
    Component *zener = add_comp(circuit, COMP_ZENER, x + 80, y, 90);
    zener->props.zener.vz = 5.1;
    zener->props.zener.rz = 10.0;

    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 80, y + 60, 0);

    // Load resistor
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 160, y, 90);
    rload->props.resistor.resistance = 10000.0;

    Component *gnd3 = add_comp(circuit, COMP_GROUND, x + 160, y + 60, 0);

    // Ground connections
    connect_terminals(circuit, vcc, 1, gnd1, 0);
    connect_terminals(circuit, zener, 1, gnd2, 0);
    connect_terminals(circuit, rload, 1, gnd3, 0);

    // Vcc to Rs top
    float vcc_pos_x, vcc_pos_y;
    component_get_terminal_pos(vcc, 0, &vcc_pos_x, &vcc_pos_y);
    float rs_top_x, rs_top_y;
    component_get_terminal_pos(rs, 0, &rs_top_x, &rs_top_y);

    wire_L_shape(circuit, vcc_pos_x, vcc_pos_y, rs_top_x, rs_top_y, true);

    // Rs to Zener junction and load
    float rs_bot_x, rs_bot_y;
    component_get_terminal_pos(rs, 1, &rs_bot_x, &rs_bot_y);
    float zener_top_x, zener_top_y;
    component_get_terminal_pos(zener, 0, &zener_top_x, &zener_top_y);
    float rload_top_x, rload_top_y;
    component_get_terminal_pos(rload, 0, &rload_top_x, &rload_top_y);

    int ref_junc = circuit_find_or_create_node(circuit, rs_bot_x, rs_bot_y, 5.0f);
    rs->node_ids[1] = ref_junc;

    int zener_top_node = circuit_find_or_create_node(circuit, zener_top_x, zener_top_y, 5.0f);
    circuit_add_wire(circuit, ref_junc, zener_top_node);
    zener->node_ids[0] = zener_top_node;

    int rload_top_node = circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f);
    int corner1 = circuit_find_or_create_node(circuit, rload_top_x, rs_bot_y, 5.0f);
    circuit_add_wire(circuit, ref_junc, corner1);
    circuit_add_wire(circuit, corner1, rload_top_node);
    rload->node_ids[0] = rload_top_node;

    return 6;
}

// Precision Full-Wave Rectifier (Absolute Value Circuit)
static int place_precision_rect(Circuit *circuit, float x, float y) {
    // Two op-amp precision full-wave rectifier (absolute value circuit)
    //
    // LAYOUT DESIGN - Separate rows with clearance:
    //   Row -80:  R2 feedback, D1 (above op1)
    //   Row -40:  Output wire routing channel (EMPTY - no components)
    //   Row 0:    R1 input resistor only (LEFT section)
    //   Row +20:  op1 center
    //   Row +60:  D2, R3, op2 center, Rload (MAIN signal path)
    //   Row +120: R4 direct input path
    //   Row +160: R5 feedback for op2
    //   Row +200: Grounds
    //
    // Key routing rules:
    //   - D1 feedback goes UP to -80, then back down OUTSIDE D2
    //   - Output wire goes at -40 level (above everything) to reach Rload
    //   - R4 path runs well below all op-amps

    // === INPUT SECTION (left side) ===
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 60, 0);
    if (!vin) return 0;
    vin->props.ac_voltage.amplitude = 1.0;
    vin->props.ac_voltage.frequency = 100.0;

    Component *gnd_in = add_comp(circuit, COMP_GROUND, x, y + 120, 0);
    connect_terminals(circuit, vin, 1, gnd_in, 0);

    // === FIRST STAGE: Half-wave rectifier ===
    // R1: Input resistor (horizontal, at y level)
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 120, y, 0);
    r1->props.resistor.resistance = 10000.0;

    // Op1: First op-amp (centered at y + 20)
    Component *op1 = add_comp(circuit, COMP_OPAMP, x + 240, y + 20, 0);
    op1->props.opamp.ideal = true;

    // R2: Feedback resistor for op1 (above op1 at y - 80)
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 240, y - 80, 0);
    r2->props.resistor.resistance = 10000.0;

    // D1: Feedback diode (at same level as R2, y - 80)
    Component *d1 = add_comp(circuit, COMP_DIODE, x + 340, y - 80, 0);

    // Ground for op1 non-inverting (well below op1)
    Component *gnd_op1 = add_comp(circuit, COMP_GROUND, x + 220, y + 100, 0);

    // === SECOND STAGE: Summing amplifier ===
    // D2: Output diode from op1 (at y + 60, the main signal row)
    Component *d2 = add_comp(circuit, COMP_DIODE, x + 360, y + 60, 0);

    // R3: From D2 output to op2 (at y + 60)
    Component *r3 = add_comp(circuit, COMP_RESISTOR, x + 460, y + 60, 0);
    r3->props.resistor.resistance = 10000.0;

    // Op2: Second op-amp (centered at y + 60)
    Component *op2 = add_comp(circuit, COMP_OPAMP, x + 580, y + 60, 0);
    op2->props.opamp.ideal = true;

    // R4: Direct input path (well below at y + 120)
    Component *r4 = add_comp(circuit, COMP_RESISTOR, x + 460, y + 120, 0);
    r4->props.resistor.resistance = 5000.0;

    // R5: Feedback resistor for op2 (below op2 at y + 160)
    Component *r5 = add_comp(circuit, COMP_RESISTOR, x + 580, y + 160, 0);
    r5->props.resistor.resistance = 10000.0;

    // Ground for op2 non-inverting
    Component *gnd_op2 = add_comp(circuit, COMP_GROUND, x + 560, y + 140, 0);

    // === OUTPUT SECTION ===
    // Rload: vertical resistor, positioned so top terminal is at y - 40 (routing channel)
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 700, y - 20, 90);
    rload->props.resistor.resistance = 10000.0;

    Component *gnd_load = add_comp(circuit, COMP_GROUND, x + 700, y + 60, 0);
    connect_terminals(circuit, rload, 1, gnd_load, 0);

    // === WIRING ===

    // --- Input to R1 ---
    float vin_x, vin_y;
    component_get_terminal_pos(vin, 0, &vin_x, &vin_y);
    float r1_left_x, r1_left_y;
    component_get_terminal_pos(r1, 0, &r1_left_x, &r1_left_y);

    int vin_node = circuit_find_or_create_node(circuit, vin_x, vin_y, 5.0f);
    int input_junc = circuit_find_or_create_node(circuit, vin_x, r1_left_y, 5.0f);
    int r1_left_node = circuit_find_or_create_node(circuit, r1_left_x, r1_left_y, 5.0f);
    circuit_add_wire(circuit, vin_node, input_junc);
    circuit_add_wire(circuit, input_junc, r1_left_node);
    vin->node_ids[0] = vin_node;
    r1->node_ids[0] = r1_left_node;

    // --- R1 to op1 inverting ---
    float r1_right_x, r1_right_y;
    component_get_terminal_pos(r1, 1, &r1_right_x, &r1_right_y);
    float op1_inv_x, op1_inv_y;
    component_get_terminal_pos(op1, 0, &op1_inv_x, &op1_inv_y);

    int r1_right_node = circuit_find_or_create_node(circuit, r1_right_x, r1_right_y, 5.0f);
    r1->node_ids[1] = r1_right_node;

    // Junction for feedback connections (to the left of op1)
    int inv1_junc = circuit_find_or_create_node(circuit, r1_right_x + 20, op1_inv_y, 5.0f);
    int op1_inv_node = circuit_find_or_create_node(circuit, op1_inv_x, op1_inv_y, 5.0f);

    // Route: R1 right -> right 20px -> down to inv level -> right to op1 inv
    int corner_r1_h = circuit_find_or_create_node(circuit, r1_right_x + 20, r1_right_y, 5.0f);
    circuit_add_wire(circuit, r1_right_node, corner_r1_h);
    circuit_add_wire(circuit, corner_r1_h, inv1_junc);
    circuit_add_wire(circuit, inv1_junc, op1_inv_node);
    op1->node_ids[0] = op1_inv_node;

    // --- R2 feedback: left connects to inv1_junc, right connects to D1 anode ---
    float r2_left_x, r2_left_y;
    component_get_terminal_pos(r2, 0, &r2_left_x, &r2_left_y);
    float r2_right_x, r2_right_y;
    component_get_terminal_pos(r2, 1, &r2_right_x, &r2_right_y);

    int r2_left_node = circuit_find_or_create_node(circuit, r2_left_x, r2_left_y, 5.0f);
    int r2_right_node = circuit_find_or_create_node(circuit, r2_right_x, r2_right_y, 5.0f);
    r2->node_ids[0] = r2_left_node;
    r2->node_ids[1] = r2_right_node;

    // Route from inv1_junc UP to R2 level
    int corner_fb1 = circuit_find_or_create_node(circuit, r1_right_x + 20, r2_left_y, 5.0f);
    circuit_add_wire(circuit, inv1_junc, corner_fb1);
    circuit_add_wire(circuit, corner_fb1, r2_left_node);

    // --- D1: anode from R2 right, cathode to op1 output ---
    float d1_anode_x, d1_anode_y;
    component_get_terminal_pos(d1, 0, &d1_anode_x, &d1_anode_y);
    float d1_cath_x, d1_cath_y;
    component_get_terminal_pos(d1, 1, &d1_cath_x, &d1_cath_y);

    int d1_anode_node = circuit_find_or_create_node(circuit, d1_anode_x, d1_anode_y, 5.0f);
    int d1_cath_node = circuit_find_or_create_node(circuit, d1_cath_x, d1_cath_y, 5.0f);
    circuit_add_wire(circuit, r2_right_node, d1_anode_node);
    d1->node_ids[0] = d1_anode_node;
    d1->node_ids[1] = d1_cath_node;

    // Op1 output
    float op1_out_x, op1_out_y;
    component_get_terminal_pos(op1, 2, &op1_out_x, &op1_out_y);
    int op1_out_node = circuit_find_or_create_node(circuit, op1_out_x, op1_out_y, 5.0f);
    op1->node_ids[2] = op1_out_node;

    // D1 cathode to op1 output - route DOWN and LEFT, staying RIGHT of D2
    // Path: D1 cath (x+360, y-80) -> down to y-40 -> left to op1_out_x+20 -> down to op1_out_y -> left to op1_out
    float feedback_route_y = y - 40;  // Routing channel above main signal path
    int d1_corner1 = circuit_find_or_create_node(circuit, d1_cath_x, feedback_route_y, 5.0f);
    int d1_corner2 = circuit_find_or_create_node(circuit, op1_out_x + 20, feedback_route_y, 5.0f);
    int d1_corner3 = circuit_find_or_create_node(circuit, op1_out_x + 20, op1_out_y, 5.0f);
    circuit_add_wire(circuit, d1_cath_node, d1_corner1);
    circuit_add_wire(circuit, d1_corner1, d1_corner2);
    circuit_add_wire(circuit, d1_corner2, d1_corner3);
    circuit_add_wire(circuit, d1_corner3, op1_out_node);

    // --- Op1 non-inverting to ground ---
    float op1_noninv_x, op1_noninv_y;
    component_get_terminal_pos(op1, 1, &op1_noninv_x, &op1_noninv_y);
    float gnd_op1_x, gnd_op1_y;
    component_get_terminal_pos(gnd_op1, 0, &gnd_op1_x, &gnd_op1_y);

    int op1_noninv_node = circuit_find_or_create_node(circuit, op1_noninv_x, op1_noninv_y, 5.0f);
    int gnd_op1_node = circuit_find_or_create_node(circuit, gnd_op1_x, gnd_op1_y, 5.0f);
    int corner_gnd1 = circuit_find_or_create_node(circuit, gnd_op1_x, op1_noninv_y, 5.0f);
    circuit_add_wire(circuit, op1_noninv_node, corner_gnd1);
    circuit_add_wire(circuit, corner_gnd1, gnd_op1_node);
    op1->node_ids[1] = op1_noninv_node;
    gnd_op1->node_ids[0] = gnd_op1_node;

    // --- Op1 output to D2 anode ---
    // Route: op1_out -> down to D2 level (y+60) -> right to D2 anode
    float d2_anode_x, d2_anode_y;
    component_get_terminal_pos(d2, 0, &d2_anode_x, &d2_anode_y);
    float d2_cath_x, d2_cath_y;
    component_get_terminal_pos(d2, 1, &d2_cath_x, &d2_cath_y);

    int d2_anode_node = circuit_find_or_create_node(circuit, d2_anode_x, d2_anode_y, 5.0f);
    int d2_cath_node = circuit_find_or_create_node(circuit, d2_cath_x, d2_cath_y, 5.0f);
    d2->node_ids[0] = d2_anode_node;
    d2->node_ids[1] = d2_cath_node;

    // Route from op1 output down to D2 level, then right to D2
    int op1_to_d2_corner = circuit_find_or_create_node(circuit, op1_out_x, d2_anode_y, 5.0f);
    circuit_add_wire(circuit, op1_out_node, op1_to_d2_corner);
    circuit_add_wire(circuit, op1_to_d2_corner, d2_anode_node);

    // --- D2 cathode to R3 left ---
    float r3_left_x, r3_left_y;
    component_get_terminal_pos(r3, 0, &r3_left_x, &r3_left_y);
    int r3_left_node = circuit_find_or_create_node(circuit, r3_left_x, r3_left_y, 5.0f);
    circuit_add_wire(circuit, d2_cath_node, r3_left_node);
    r3->node_ids[0] = r3_left_node;

    // --- R3 right to op2 inverting ---
    float r3_right_x, r3_right_y;
    component_get_terminal_pos(r3, 1, &r3_right_x, &r3_right_y);
    float op2_inv_x, op2_inv_y;
    component_get_terminal_pos(op2, 0, &op2_inv_x, &op2_inv_y);

    int r3_right_node = circuit_find_or_create_node(circuit, r3_right_x, r3_right_y, 5.0f);
    int inv2_junc = circuit_find_or_create_node(circuit, r3_right_x + 20, op2_inv_y, 5.0f);
    int op2_inv_node = circuit_find_or_create_node(circuit, op2_inv_x, op2_inv_y, 5.0f);
    r3->node_ids[1] = r3_right_node;
    op2->node_ids[0] = op2_inv_node;

    // Route: R3 right -> right 20px corner -> to op2 inv (should be same Y level)
    int corner_r3 = circuit_find_or_create_node(circuit, r3_right_x + 20, r3_right_y, 5.0f);
    circuit_add_wire(circuit, r3_right_node, corner_r3);
    circuit_add_wire(circuit, corner_r3, inv2_junc);
    circuit_add_wire(circuit, inv2_junc, op2_inv_node);

    // --- R4: Direct input to op2 (bypasses first stage) ---
    float r4_left_x, r4_left_y;
    component_get_terminal_pos(r4, 0, &r4_left_x, &r4_left_y);
    float r4_right_x, r4_right_y;
    component_get_terminal_pos(r4, 1, &r4_right_x, &r4_right_y);

    int r4_left_node = circuit_find_or_create_node(circuit, r4_left_x, r4_left_y, 5.0f);
    int r4_right_node = circuit_find_or_create_node(circuit, r4_right_x, r4_right_y, 5.0f);
    r4->node_ids[0] = r4_left_node;
    r4->node_ids[1] = r4_right_node;

    // Route from input junction down to R4 level, then right to R4
    int corner_r4_in = circuit_find_or_create_node(circuit, vin_x, r4_left_y, 5.0f);
    circuit_add_wire(circuit, input_junc, corner_r4_in);
    circuit_add_wire(circuit, corner_r4_in, r4_left_node);

    // R4 right to inv2 junction
    int corner_r4_out = circuit_find_or_create_node(circuit, r3_right_x + 20, r4_right_y, 5.0f);
    circuit_add_wire(circuit, r4_right_node, corner_r4_out);
    circuit_add_wire(circuit, corner_r4_out, inv2_junc);

    // --- R5 feedback for op2 ---
    float r5_left_x, r5_left_y;
    component_get_terminal_pos(r5, 0, &r5_left_x, &r5_left_y);
    float r5_right_x, r5_right_y;
    component_get_terminal_pos(r5, 1, &r5_right_x, &r5_right_y);

    int r5_left_node = circuit_find_or_create_node(circuit, r5_left_x, r5_left_y, 5.0f);
    int r5_right_node = circuit_find_or_create_node(circuit, r5_right_x, r5_right_y, 5.0f);
    r5->node_ids[0] = r5_left_node;
    r5->node_ids[1] = r5_right_node;

    // Route from inv2 junction down to R5 level
    int corner_fb2 = circuit_find_or_create_node(circuit, r3_right_x + 20, r5_left_y, 5.0f);
    circuit_add_wire(circuit, inv2_junc, corner_fb2);
    circuit_add_wire(circuit, corner_fb2, r5_left_node);

    // --- Op2 output ---
    float op2_out_x, op2_out_y;
    component_get_terminal_pos(op2, 2, &op2_out_x, &op2_out_y);
    int op2_out_node = circuit_find_or_create_node(circuit, op2_out_x, op2_out_y, 5.0f);
    op2->node_ids[2] = op2_out_node;

    // R5 right to op2 output
    int corner_r5_out = circuit_find_or_create_node(circuit, r5_right_x, op2_out_y, 5.0f);
    circuit_add_wire(circuit, r5_right_node, corner_r5_out);
    circuit_add_wire(circuit, corner_r5_out, op2_out_node);

    // --- Op2 non-inverting to ground ---
    float op2_noninv_x, op2_noninv_y;
    component_get_terminal_pos(op2, 1, &op2_noninv_x, &op2_noninv_y);
    float gnd_op2_x, gnd_op2_y;
    component_get_terminal_pos(gnd_op2, 0, &gnd_op2_x, &gnd_op2_y);

    int op2_noninv_node = circuit_find_or_create_node(circuit, op2_noninv_x, op2_noninv_y, 5.0f);
    int gnd_op2_node = circuit_find_or_create_node(circuit, gnd_op2_x, gnd_op2_y, 5.0f);
    int corner_gnd2 = circuit_find_or_create_node(circuit, gnd_op2_x, op2_noninv_y, 5.0f);
    circuit_add_wire(circuit, op2_noninv_node, corner_gnd2);
    circuit_add_wire(circuit, corner_gnd2, gnd_op2_node);
    op2->node_ids[1] = op2_noninv_node;
    gnd_op2->node_ids[0] = gnd_op2_node;

    // --- Output to load resistor ---
    // Route: op2 out -> UP to routing channel (y-40) -> RIGHT to rload_top_x -> DOWN to rload top
    float rload_top_x, rload_top_y;
    component_get_terminal_pos(rload, 0, &rload_top_x, &rload_top_y);

    int rload_top_node = circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f);
    rload->node_ids[0] = rload_top_node;

    // Route output ABOVE all components via the y-40 routing channel
    float output_route_y = y - 40;
    int out_corner1 = circuit_find_or_create_node(circuit, op2_out_x, output_route_y, 5.0f);
    int out_corner2 = circuit_find_or_create_node(circuit, rload_top_x, output_route_y, 5.0f);
    circuit_add_wire(circuit, op2_out_node, out_corner1);
    circuit_add_wire(circuit, out_corner1, out_corner2);
    circuit_add_wire(circuit, out_corner2, rload_top_node);

    // Components: vin, gnd_in, r1, op1, r2, d1, gnd_op1,
    //             d2, r3, op2, r4, r5, gnd_op2, rload, gnd_load
    return 15;
}

// 7805 Fixed 5V Regulator Circuit
// Basic power supply with input/output filtering
static int place_7805_reg(Circuit *circuit, float x, float y) {
    // Input voltage source (9V)
    Component *vin = add_comp(circuit, COMP_DC_VOLTAGE, x - 100, y, 0);
    if (!vin) return 0;
    vin->props.dc_voltage.voltage = 9.0;

    Component *gnd_in = add_comp(circuit, COMP_GROUND, x - 100, y + 60, 0);

    // Input filter capacitor
    Component *cin = add_comp(circuit, COMP_CAPACITOR, x, y + 30, 90);
    cin->props.capacitor.capacitance = 0.33e-6;  // 0.33uF

    // 7805 regulator - positioned horizontally
    Component *reg = add_comp(circuit, COMP_7805, x + 80, y, 0);

    // Output filter capacitor
    Component *cout = add_comp(circuit, COMP_CAPACITOR, x + 160, y + 30, 90);
    cout->props.capacitor.capacitance = 0.1e-6;  // 0.1uF

    // Load resistor (50 ohms for 100mA at 5V)
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 240, y + 30, 90);
    rload->props.resistor.resistance = 50.0;

    Component *gnd_load = add_comp(circuit, COMP_GROUND, x + 240, y + 90, 0);

    // Get terminal positions
    float vin_pos_x, vin_pos_y, vin_neg_x, vin_neg_y;
    component_get_terminal_pos(vin, 0, &vin_pos_x, &vin_pos_y);
    component_get_terminal_pos(vin, 1, &vin_neg_x, &vin_neg_y);

    float gnd_in_x, gnd_in_y;
    component_get_terminal_pos(gnd_in, 0, &gnd_in_x, &gnd_in_y);

    float cin_top_x, cin_top_y, cin_bot_x, cin_bot_y;
    component_get_terminal_pos(cin, 0, &cin_top_x, &cin_top_y);
    component_get_terminal_pos(cin, 1, &cin_bot_x, &cin_bot_y);

    float reg_in_x, reg_in_y, reg_out_x, reg_out_y, reg_gnd_x, reg_gnd_y;
    component_get_terminal_pos(reg, 0, &reg_in_x, &reg_in_y);
    component_get_terminal_pos(reg, 1, &reg_out_x, &reg_out_y);
    component_get_terminal_pos(reg, 2, &reg_gnd_x, &reg_gnd_y);

    float cout_top_x, cout_top_y, cout_bot_x, cout_bot_y;
    component_get_terminal_pos(cout, 0, &cout_top_x, &cout_top_y);
    component_get_terminal_pos(cout, 1, &cout_bot_x, &cout_bot_y);

    float rload_top_x, rload_top_y, rload_bot_x, rload_bot_y;
    component_get_terminal_pos(rload, 0, &rload_top_x, &rload_top_y);
    component_get_terminal_pos(rload, 1, &rload_bot_x, &rload_bot_y);

    float gnd_load_x, gnd_load_y;
    component_get_terminal_pos(gnd_load, 0, &gnd_load_x, &gnd_load_y);

    // Connect Vin+ to input bus (top rail)
    int vin_pos_node = circuit_find_or_create_node(circuit, vin_pos_x, vin_pos_y, 5.0f);
    int input_bus = circuit_find_or_create_node(circuit, vin_pos_x, y - 40, 5.0f);
    circuit_add_wire(circuit, vin_pos_node, input_bus);
    vin->node_ids[0] = vin_pos_node;

    // Input bus to Cin top
    int cin_top_node = circuit_find_or_create_node(circuit, cin_top_x, cin_top_y, 5.0f);
    int bus_to_cin = circuit_find_or_create_node(circuit, cin_top_x, y - 40, 5.0f);
    circuit_add_wire(circuit, input_bus, bus_to_cin);
    circuit_add_wire(circuit, bus_to_cin, cin_top_node);
    cin->node_ids[0] = cin_top_node;

    // Input bus to regulator IN
    int reg_in_node = circuit_find_or_create_node(circuit, reg_in_x, reg_in_y, 5.0f);
    int bus_to_reg = circuit_find_or_create_node(circuit, reg_in_x, y - 40, 5.0f);
    circuit_add_wire(circuit, bus_to_cin, bus_to_reg);
    int corner_reg_in = circuit_find_or_create_node(circuit, reg_in_x, reg_in_y, 5.0f);
    circuit_add_wire(circuit, bus_to_reg, corner_reg_in);
    reg->node_ids[0] = reg_in_node;

    // Regulator OUT to output bus
    int reg_out_node = circuit_find_or_create_node(circuit, reg_out_x, reg_out_y, 5.0f);
    int output_bus = circuit_find_or_create_node(circuit, reg_out_x, y - 40, 5.0f);
    circuit_add_wire(circuit, reg_out_node, output_bus);
    reg->node_ids[1] = reg_out_node;

    // Output bus to Cout top
    int cout_top_node = circuit_find_or_create_node(circuit, cout_top_x, cout_top_y, 5.0f);
    int bus_to_cout = circuit_find_or_create_node(circuit, cout_top_x, y - 40, 5.0f);
    circuit_add_wire(circuit, output_bus, bus_to_cout);
    circuit_add_wire(circuit, bus_to_cout, cout_top_node);
    cout->node_ids[0] = cout_top_node;

    // Output bus to load
    int rload_top_node = circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f);
    int bus_to_load = circuit_find_or_create_node(circuit, rload_top_x, y - 40, 5.0f);
    circuit_add_wire(circuit, bus_to_cout, bus_to_load);
    circuit_add_wire(circuit, bus_to_load, rload_top_node);
    rload->node_ids[0] = rload_top_node;

    // Ground rail
    int gnd_in_node = circuit_find_or_create_node(circuit, gnd_in_x, gnd_in_y, 5.0f);
    int vin_neg_node = circuit_find_or_create_node(circuit, vin_neg_x, vin_neg_y, 5.0f);
    circuit_add_wire(circuit, vin_neg_node, gnd_in_node);
    vin->node_ids[1] = vin_neg_node;
    gnd_in->node_ids[0] = gnd_in_node;

    // Cin bottom to ground
    int cin_bot_node = circuit_find_or_create_node(circuit, cin_bot_x, cin_bot_y, 5.0f);
    int gnd_rail = circuit_find_or_create_node(circuit, cin_bot_x, y + 80, 5.0f);
    circuit_add_wire(circuit, cin_bot_node, gnd_rail);
    cin->node_ids[1] = cin_bot_node;

    // Connect Vin- to ground rail
    int corner_vin_gnd = circuit_find_or_create_node(circuit, vin_neg_x, y + 80, 5.0f);
    circuit_add_wire(circuit, gnd_in_node, corner_vin_gnd);
    circuit_add_wire(circuit, corner_vin_gnd, gnd_rail);

    // Regulator GND to ground rail
    int reg_gnd_node = circuit_find_or_create_node(circuit, reg_gnd_x, reg_gnd_y, 5.0f);
    int gnd_rail_reg = circuit_find_or_create_node(circuit, reg_gnd_x, y + 80, 5.0f);
    circuit_add_wire(circuit, gnd_rail, gnd_rail_reg);
    circuit_add_wire(circuit, reg_gnd_node, gnd_rail_reg);
    reg->node_ids[2] = reg_gnd_node;

    // Cout bottom to ground rail
    int cout_bot_node = circuit_find_or_create_node(circuit, cout_bot_x, cout_bot_y, 5.0f);
    int gnd_rail_cout = circuit_find_or_create_node(circuit, cout_bot_x, y + 80, 5.0f);
    circuit_add_wire(circuit, gnd_rail_reg, gnd_rail_cout);
    circuit_add_wire(circuit, cout_bot_node, gnd_rail_cout);
    cout->node_ids[1] = cout_bot_node;

    // Load resistor bottom to ground
    int rload_bot_node = circuit_find_or_create_node(circuit, rload_bot_x, rload_bot_y, 5.0f);
    int gnd_load_node = circuit_find_or_create_node(circuit, gnd_load_x, gnd_load_y, 5.0f);
    circuit_add_wire(circuit, rload_bot_node, gnd_load_node);
    rload->node_ids[1] = rload_bot_node;
    gnd_load->node_ids[0] = gnd_load_node;

    // Connect ground rail to load ground
    int gnd_rail_load = circuit_find_or_create_node(circuit, rload_bot_x, y + 80, 5.0f);
    circuit_add_wire(circuit, gnd_rail_cout, gnd_rail_load);
    circuit_add_wire(circuit, gnd_rail_load, gnd_load_node);

    return 8;  // vin, gnd_in, cin, reg, cout, rload, gnd_load
}

// LM317 Adjustable Regulator Circuit
// Vout = 1.25V * (1 + R2/R1) with R1=240 ohm, R2=720 ohm -> Vout ~= 5V
static int place_lm317_reg(Circuit *circuit, float x, float y) {
    // Input voltage source (12V)
    Component *vin = add_comp(circuit, COMP_DC_VOLTAGE, x - 100, y, 0);
    if (!vin) return 0;
    vin->props.dc_voltage.voltage = 12.0;

    Component *gnd_in = add_comp(circuit, COMP_GROUND, x - 100, y + 60, 0);

    // Input filter capacitor
    Component *cin = add_comp(circuit, COMP_CAPACITOR, x, y + 30, 90);
    cin->props.capacitor.capacitance = 0.1e-6;  // 0.1uF

    // LM317 regulator
    Component *reg = add_comp(circuit, COMP_LM317, x + 80, y, 0);

    // R1 (between OUT and ADJ) - 240 ohm
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 140, y + 50, 90);
    r1->props.resistor.resistance = 240.0;

    // R2 (between ADJ and GND) - 720 ohm for ~5V output
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 140, y + 110, 90);
    r2->props.resistor.resistance = 720.0;

    // Output filter capacitor
    Component *cout = add_comp(circuit, COMP_CAPACITOR, x + 200, y + 30, 90);
    cout->props.capacitor.capacitance = 1.0e-6;  // 1uF

    // Load resistor
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 260, y + 30, 90);
    rload->props.resistor.resistance = 100.0;

    Component *gnd_load = add_comp(circuit, COMP_GROUND, x + 200, y + 150, 0);

    // Get terminal positions
    float vin_pos_x, vin_pos_y, vin_neg_x, vin_neg_y;
    component_get_terminal_pos(vin, 0, &vin_pos_x, &vin_pos_y);
    component_get_terminal_pos(vin, 1, &vin_neg_x, &vin_neg_y);

    float gnd_in_x, gnd_in_y;
    component_get_terminal_pos(gnd_in, 0, &gnd_in_x, &gnd_in_y);

    float cin_top_x, cin_top_y, cin_bot_x, cin_bot_y;
    component_get_terminal_pos(cin, 0, &cin_top_x, &cin_top_y);
    component_get_terminal_pos(cin, 1, &cin_bot_x, &cin_bot_y);

    float reg_in_x, reg_in_y, reg_out_x, reg_out_y, reg_adj_x, reg_adj_y;
    component_get_terminal_pos(reg, 0, &reg_in_x, &reg_in_y);
    component_get_terminal_pos(reg, 1, &reg_out_x, &reg_out_y);
    component_get_terminal_pos(reg, 2, &reg_adj_x, &reg_adj_y);

    float r1_top_x, r1_top_y, r1_bot_x, r1_bot_y;
    component_get_terminal_pos(r1, 0, &r1_top_x, &r1_top_y);
    component_get_terminal_pos(r1, 1, &r1_bot_x, &r1_bot_y);

    float r2_top_x, r2_top_y, r2_bot_x, r2_bot_y;
    component_get_terminal_pos(r2, 0, &r2_top_x, &r2_top_y);
    component_get_terminal_pos(r2, 1, &r2_bot_x, &r2_bot_y);

    float cout_top_x, cout_top_y, cout_bot_x, cout_bot_y;
    component_get_terminal_pos(cout, 0, &cout_top_x, &cout_top_y);
    component_get_terminal_pos(cout, 1, &cout_bot_x, &cout_bot_y);

    float rload_top_x, rload_top_y, rload_bot_x, rload_bot_y;
    component_get_terminal_pos(rload, 0, &rload_top_x, &rload_top_y);
    component_get_terminal_pos(rload, 1, &rload_bot_x, &rload_bot_y);

    float gnd_load_x, gnd_load_y;
    component_get_terminal_pos(gnd_load, 0, &gnd_load_x, &gnd_load_y);

    // Input power rail
    int vin_pos_node = circuit_find_or_create_node(circuit, vin_pos_x, vin_pos_y, 5.0f);
    int input_bus = circuit_find_or_create_node(circuit, vin_pos_x, y - 40, 5.0f);
    circuit_add_wire(circuit, vin_pos_node, input_bus);
    vin->node_ids[0] = vin_pos_node;

    // Cin top to input bus
    int cin_top_node = circuit_find_or_create_node(circuit, cin_top_x, cin_top_y, 5.0f);
    int bus_to_cin = circuit_find_or_create_node(circuit, cin_top_x, y - 40, 5.0f);
    circuit_add_wire(circuit, input_bus, bus_to_cin);
    circuit_add_wire(circuit, bus_to_cin, cin_top_node);
    cin->node_ids[0] = cin_top_node;

    // Regulator IN to input bus
    int reg_in_node = circuit_find_or_create_node(circuit, reg_in_x, reg_in_y, 5.0f);
    int bus_to_reg = circuit_find_or_create_node(circuit, reg_in_x, y - 40, 5.0f);
    circuit_add_wire(circuit, bus_to_cin, bus_to_reg);
    circuit_add_wire(circuit, bus_to_reg, reg_in_node);
    reg->node_ids[0] = reg_in_node;

    // Output power rail from regulator OUT
    int reg_out_node = circuit_find_or_create_node(circuit, reg_out_x, reg_out_y, 5.0f);
    int output_bus = circuit_find_or_create_node(circuit, reg_out_x, y - 40, 5.0f);
    circuit_add_wire(circuit, reg_out_node, output_bus);
    reg->node_ids[1] = reg_out_node;

    // R1 top to output bus
    int r1_top_node = circuit_find_or_create_node(circuit, r1_top_x, r1_top_y, 5.0f);
    int bus_to_r1 = circuit_find_or_create_node(circuit, r1_top_x, y - 40, 5.0f);
    circuit_add_wire(circuit, output_bus, bus_to_r1);
    circuit_add_wire(circuit, bus_to_r1, r1_top_node);
    r1->node_ids[0] = r1_top_node;

    // Cout top to output bus
    int cout_top_node = circuit_find_or_create_node(circuit, cout_top_x, cout_top_y, 5.0f);
    int bus_to_cout = circuit_find_or_create_node(circuit, cout_top_x, y - 40, 5.0f);
    circuit_add_wire(circuit, bus_to_r1, bus_to_cout);
    circuit_add_wire(circuit, bus_to_cout, cout_top_node);
    cout->node_ids[0] = cout_top_node;

    // Rload top to output bus
    int rload_top_node = circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f);
    int bus_to_load = circuit_find_or_create_node(circuit, rload_top_x, y - 40, 5.0f);
    circuit_add_wire(circuit, bus_to_cout, bus_to_load);
    circuit_add_wire(circuit, bus_to_load, rload_top_node);
    rload->node_ids[0] = rload_top_node;

    // R1 bottom to ADJ and R2 top (feedback junction)
    int r1_bot_node = circuit_find_or_create_node(circuit, r1_bot_x, r1_bot_y, 5.0f);
    int reg_adj_node = circuit_find_or_create_node(circuit, reg_adj_x, reg_adj_y, 5.0f);
    int r2_top_node = circuit_find_or_create_node(circuit, r2_top_x, r2_top_y, 5.0f);
    r1->node_ids[1] = r1_bot_node;
    r2->node_ids[0] = r2_top_node;
    reg->node_ids[2] = reg_adj_node;

    // Connect R1 bottom to ADJ
    int adj_junction = circuit_find_or_create_node(circuit, reg_adj_x, r1_bot_y, 5.0f);
    circuit_add_wire(circuit, r1_bot_node, adj_junction);
    circuit_add_wire(circuit, adj_junction, reg_adj_node);

    // Connect R2 top to ADJ junction
    circuit_add_wire(circuit, adj_junction, r2_top_node);

    // Ground connections
    int gnd_in_node = circuit_find_or_create_node(circuit, gnd_in_x, gnd_in_y, 5.0f);
    int vin_neg_node = circuit_find_or_create_node(circuit, vin_neg_x, vin_neg_y, 5.0f);
    circuit_add_wire(circuit, vin_neg_node, gnd_in_node);
    vin->node_ids[1] = vin_neg_node;
    gnd_in->node_ids[0] = gnd_in_node;

    // Ground rail
    float gnd_y = y + 140;
    int gnd_rail = circuit_find_or_create_node(circuit, cin_bot_x, gnd_y, 5.0f);

    // Cin bottom to ground
    int cin_bot_node = circuit_find_or_create_node(circuit, cin_bot_x, cin_bot_y, 5.0f);
    circuit_add_wire(circuit, cin_bot_node, gnd_rail);
    cin->node_ids[1] = cin_bot_node;

    // Connect vin- to ground rail
    int corner_vin_gnd = circuit_find_or_create_node(circuit, vin_neg_x, gnd_y, 5.0f);
    circuit_add_wire(circuit, gnd_in_node, corner_vin_gnd);
    circuit_add_wire(circuit, corner_vin_gnd, gnd_rail);

    // R2 bottom to ground rail
    int r2_bot_node = circuit_find_or_create_node(circuit, r2_bot_x, r2_bot_y, 5.0f);
    int gnd_rail_r2 = circuit_find_or_create_node(circuit, r2_bot_x, gnd_y, 5.0f);
    circuit_add_wire(circuit, gnd_rail, gnd_rail_r2);
    circuit_add_wire(circuit, r2_bot_node, gnd_rail_r2);
    r2->node_ids[1] = r2_bot_node;

    // Cout bottom to ground rail
    int cout_bot_node = circuit_find_or_create_node(circuit, cout_bot_x, cout_bot_y, 5.0f);
    int gnd_rail_cout = circuit_find_or_create_node(circuit, cout_bot_x, gnd_y, 5.0f);
    circuit_add_wire(circuit, gnd_rail_r2, gnd_rail_cout);
    circuit_add_wire(circuit, cout_bot_node, gnd_rail_cout);
    cout->node_ids[1] = cout_bot_node;

    // Rload bottom and ground symbol
    int rload_bot_node = circuit_find_or_create_node(circuit, rload_bot_x, rload_bot_y, 5.0f);
    int gnd_load_node = circuit_find_or_create_node(circuit, gnd_load_x, gnd_load_y, 5.0f);
    int gnd_rail_load = circuit_find_or_create_node(circuit, rload_bot_x, gnd_y, 5.0f);
    circuit_add_wire(circuit, gnd_rail_cout, gnd_rail_load);
    circuit_add_wire(circuit, rload_bot_node, gnd_rail_load);
    rload->node_ids[1] = rload_bot_node;

    // Ground symbol connection
    circuit_add_wire(circuit, gnd_rail_cout, gnd_load_node);
    gnd_load->node_ids[0] = gnd_load_node;

    return 10;  // vin, gnd_in, cin, reg, r1, r2, cout, rload, gnd_load
}

// TL431 Precision Shunt Reference Circuit
// Used as a precision 2.5V reference with external resistor setting
static int place_tl431_ref(Circuit *circuit, float x, float y) {
    // Input voltage source (5V)
    Component *vin = add_comp(circuit, COMP_DC_VOLTAGE, x - 80, y, 0);
    if (!vin) return 0;
    vin->props.dc_voltage.voltage = 5.0;

    Component *gnd_in = add_comp(circuit, COMP_GROUND, x - 80, y + 60, 0);

    // Series resistor (limits current through TL431)
    Component *rs = add_comp(circuit, COMP_RESISTOR, x + 20, y - 40, 0);
    rs->props.resistor.resistance = 470.0;  // 470 ohm

    // TL431 shunt reference
    Component *ref = add_comp(circuit, COMP_TL431, x + 100, y + 20, 0);

    // Load resistor to demonstrate voltage reference
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 180, y + 20, 90);
    rload->props.resistor.resistance = 1000.0;  // 1k ohm

    Component *gnd_load = add_comp(circuit, COMP_GROUND, x + 180, y + 100, 0);

    // Get terminal positions
    float vin_pos_x, vin_pos_y, vin_neg_x, vin_neg_y;
    component_get_terminal_pos(vin, 0, &vin_pos_x, &vin_pos_y);
    component_get_terminal_pos(vin, 1, &vin_neg_x, &vin_neg_y);

    float gnd_in_x, gnd_in_y;
    component_get_terminal_pos(gnd_in, 0, &gnd_in_x, &gnd_in_y);

    float rs_left_x, rs_left_y, rs_right_x, rs_right_y;
    component_get_terminal_pos(rs, 0, &rs_left_x, &rs_left_y);
    component_get_terminal_pos(rs, 1, &rs_right_x, &rs_right_y);

    // TL431: K(0)=cathode, A(1)=anode, REF(2)=reference
    float ref_k_x, ref_k_y, ref_a_x, ref_a_y, ref_ref_x, ref_ref_y;
    component_get_terminal_pos(ref, 0, &ref_k_x, &ref_k_y);
    component_get_terminal_pos(ref, 1, &ref_a_x, &ref_a_y);
    component_get_terminal_pos(ref, 2, &ref_ref_x, &ref_ref_y);

    float rload_top_x, rload_top_y, rload_bot_x, rload_bot_y;
    component_get_terminal_pos(rload, 0, &rload_top_x, &rload_top_y);
    component_get_terminal_pos(rload, 1, &rload_bot_x, &rload_bot_y);

    float gnd_load_x, gnd_load_y;
    component_get_terminal_pos(gnd_load, 0, &gnd_load_x, &gnd_load_y);

    // Vin+ to Rs left
    int vin_pos_node = circuit_find_or_create_node(circuit, vin_pos_x, vin_pos_y, 5.0f);
    int rs_left_node = circuit_find_or_create_node(circuit, rs_left_x, rs_left_y, 5.0f);
    vin->node_ids[0] = vin_pos_node;
    rs->node_ids[0] = rs_left_node;

    int corner_vin_rs = circuit_find_or_create_node(circuit, vin_pos_x, rs_left_y, 5.0f);
    circuit_add_wire(circuit, vin_pos_node, corner_vin_rs);
    circuit_add_wire(circuit, corner_vin_rs, rs_left_node);

    // Rs right to TL431 cathode (K) and Rload top
    int rs_right_node = circuit_find_or_create_node(circuit, rs_right_x, rs_right_y, 5.0f);
    int ref_k_node = circuit_find_or_create_node(circuit, ref_k_x, ref_k_y, 5.0f);
    int rload_top_node = circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f);
    rs->node_ids[1] = rs_right_node;
    ref->node_ids[0] = ref_k_node;
    rload->node_ids[0] = rload_top_node;

    // Output junction (cathode node is the reference voltage output)
    int output_junction = circuit_find_or_create_node(circuit, ref_k_x, rs_right_y, 5.0f);
    circuit_add_wire(circuit, rs_right_node, output_junction);
    circuit_add_wire(circuit, output_junction, ref_k_node);

    // Rload top to output junction
    int corner_load = circuit_find_or_create_node(circuit, rload_top_x, rs_right_y, 5.0f);
    circuit_add_wire(circuit, output_junction, corner_load);
    circuit_add_wire(circuit, corner_load, rload_top_node);

    // TL431 REF connected to cathode for 2.5V reference mode
    int ref_ref_node = circuit_find_or_create_node(circuit, ref_ref_x, ref_ref_y, 5.0f);
    ref->node_ids[2] = ref_ref_node;

    // Connect REF to cathode for basic 2.5V shunt mode
    int corner_ref_k = circuit_find_or_create_node(circuit, ref_k_x, ref_ref_y, 5.0f);
    circuit_add_wire(circuit, ref_ref_node, corner_ref_k);
    circuit_add_wire(circuit, corner_ref_k, ref_k_node);

    // Ground connections
    int gnd_in_node = circuit_find_or_create_node(circuit, gnd_in_x, gnd_in_y, 5.0f);
    int vin_neg_node = circuit_find_or_create_node(circuit, vin_neg_x, vin_neg_y, 5.0f);
    circuit_add_wire(circuit, vin_neg_node, gnd_in_node);
    vin->node_ids[1] = vin_neg_node;
    gnd_in->node_ids[0] = gnd_in_node;

    // Ground rail
    float gnd_y = y + 90;
    int gnd_rail = circuit_find_or_create_node(circuit, vin_neg_x, gnd_y, 5.0f);
    circuit_add_wire(circuit, gnd_in_node, gnd_rail);

    // TL431 anode to ground
    int ref_a_node = circuit_find_or_create_node(circuit, ref_a_x, ref_a_y, 5.0f);
    ref->node_ids[1] = ref_a_node;
    int gnd_rail_ref = circuit_find_or_create_node(circuit, ref_a_x, gnd_y, 5.0f);
    circuit_add_wire(circuit, gnd_rail, gnd_rail_ref);
    circuit_add_wire(circuit, ref_a_node, gnd_rail_ref);

    // Rload bottom to ground
    int rload_bot_node = circuit_find_or_create_node(circuit, rload_bot_x, rload_bot_y, 5.0f);
    int gnd_load_node = circuit_find_or_create_node(circuit, gnd_load_x, gnd_load_y, 5.0f);
    circuit_add_wire(circuit, rload_bot_node, gnd_load_node);
    rload->node_ids[1] = rload_bot_node;
    gnd_load->node_ids[0] = gnd_load_node;

    // Connect ground rail to load ground
    int gnd_rail_load = circuit_find_or_create_node(circuit, rload_bot_x, gnd_y, 5.0f);
    circuit_add_wire(circuit, gnd_rail_ref, gnd_rail_load);
    circuit_add_wire(circuit, gnd_rail_load, gnd_load_node);

    return 7;  // vin, gnd_in, rs, ref, rload, gnd_load
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
        case CIRCUIT_FULLWAVE_BRIDGE:
            return place_fullwave_bridge(circuit, x, y);
        case CIRCUIT_CENTERTAP_RECT:
            return place_centertap_rectifier(circuit, x, y);
        case CIRCUIT_AC_DC_SUPPLY:
            return place_ac_dc_supply(circuit, x, y);
        case CIRCUIT_AC_DC_AMERICAN:
            return place_ac_dc_american(circuit, x, y);
        // TI Analog Circuits - Amplifiers
        case CIRCUIT_DIFFERENCE_AMP:
            return place_difference_amp(circuit, x, y);
        case CIRCUIT_TRANSIMPEDANCE:
            return place_transimpedance(circuit, x, y);
        case CIRCUIT_INSTR_AMP:
            return place_instr_amp(circuit, x, y);
        // TI Analog Circuits - Filters
        case CIRCUIT_SALLEN_KEY_LP:
            return place_sallen_key_lp(circuit, x, y);
        case CIRCUIT_BANDPASS_ACTIVE:
            return place_bandpass_active(circuit, x, y);
        case CIRCUIT_NOTCH_FILTER:
            return place_notch_filter(circuit, x, y);
        // TI Analog Circuits - Signal Sources
        case CIRCUIT_WIEN_OSCILLATOR:
            return place_wien_oscillator(circuit, x, y);
        case CIRCUIT_CURRENT_SOURCE:
            return place_current_source(circuit, x, y);
        // TI Analog Circuits - Comparators/Detection
        case CIRCUIT_WINDOW_COMP:
            return place_window_comp(circuit, x, y);
        case CIRCUIT_HYSTERESIS_COMP:
            return place_hysteresis_comp(circuit, x, y);
        // TI Analog Circuits - Power/Voltage
        case CIRCUIT_ZENER_REF:
            return place_zener_ref(circuit, x, y);
        case CIRCUIT_PRECISION_RECT:
            return place_precision_rect(circuit, x, y);
        // Voltage Regulator Circuits
        case CIRCUIT_7805_REG:
            return place_7805_reg(circuit, x, y);
        case CIRCUIT_LM317_REG:
            return place_lm317_reg(circuit, x, y);
        case CIRCUIT_TL431_REF:
            return place_tl431_ref(circuit, x, y);
        default:
            return 0;
    }
}
