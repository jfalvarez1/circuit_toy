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
    [CIRCUIT_NONE] = {"None", "None", "No circuit", TG_BASICS},
    [CIRCUIT_RC_LOWPASS] = {"RC Low Pass", "LP", "RC low-pass filter (fc=1.6kHz)", TG_FILTERS},
    [CIRCUIT_RC_HIGHPASS] = {"RC High Pass", "HP", "RC high-pass filter (fc=1.6kHz)", TG_FILTERS},
    [CIRCUIT_RL_LOWPASS] = {"RL Low Pass", "RL-LP", "RL low-pass filter", TG_FILTERS},
    [CIRCUIT_RL_HIGHPASS] = {"RL High Pass", "RL-HP", "RL high-pass filter", TG_FILTERS},
    [CIRCUIT_VOLTAGE_DIVIDER] = {"Voltage Divider", "Div", "Resistive voltage divider (1:1)", TG_BASICS},
    [CIRCUIT_INVERTING_AMP] = {"Inverting Amp", "Inv", "Inverting op-amp (gain=-10)", TG_OPAMPS},
    [CIRCUIT_NONINVERTING_AMP] = {"Non-Inv Amp", "NonI", "Non-inverting op-amp (gain=11)", TG_OPAMPS},
    [CIRCUIT_VOLTAGE_FOLLOWER] = {"Voltage Follower", "Fol", "Unity gain buffer", TG_OPAMPS},
    [CIRCUIT_HALFWAVE_RECT] = {"Half-Wave Rect", "HW", "Half-wave rectifier", TG_POWER_SUPPLY},
    [CIRCUIT_LED_WITH_RESISTOR] = {"LED + Resistor", "LED", "LED with current limiting resistor", TG_BASICS},
    // Transistor amplifiers
    [CIRCUIT_COMMON_EMITTER] = {"Common Emitter", "CE", "BJT common-emitter amplifier", TG_TRANSISTORS},
    [CIRCUIT_COMMON_SOURCE] = {"Common Source", "CS", "MOSFET common-source amplifier", TG_TRANSISTORS},
    [CIRCUIT_COMMON_DRAIN] = {"Source Follower", "SF", "MOSFET source follower (common-drain)", TG_TRANSISTORS},
    [CIRCUIT_MULTISTAGE_AMP] = {"Two-Stage Amp", "2Stg", "Two-stage BJT amplifier", TG_TRANSISTORS},
    // Additional transistor circuits
    [CIRCUIT_DIFFERENTIAL_PAIR] = {"Differential Pair", "Diff", "BJT differential amplifier", TG_TRANSISTORS},
    [CIRCUIT_CURRENT_MIRROR] = {"Current Mirror", "CMir", "BJT current mirror", TG_TRANSISTORS},
    [CIRCUIT_PUSH_PULL] = {"Push-Pull", "PP", "Complementary push-pull output stage", TG_TRANSISTORS},
    [CIRCUIT_CMOS_INVERTER] = {"CMOS Inverter", "CMOS", "CMOS logic inverter", TG_DIGITAL},
    // Additional op-amp circuits
    [CIRCUIT_INTEGRATOR] = {"Integrator", "Int", "Op-amp integrator circuit", TG_OPAMPS},
    [CIRCUIT_DIFFERENTIATOR] = {"Differentiator", "Dif", "Op-amp differentiator circuit", TG_OPAMPS},
    [CIRCUIT_SUMMING_AMP] = {"Summing Amp", "Sum", "Inverting summing amplifier", TG_OPAMPS},
    [CIRCUIT_COMPARATOR] = {"Comparator", "Cmp", "Op-amp voltage comparator", TG_OPAMPS},
    // Power supply / rectifier circuits
    [CIRCUIT_FULLWAVE_BRIDGE] = {"Bridge Rectifier", "Brdg", "Full-wave bridge rectifier with filter", TG_POWER_SUPPLY},
    [CIRCUIT_CENTERTAP_RECT] = {"Center-Tap Rect", "CTap", "Center-tap transformer rectifier", TG_POWER_SUPPLY},
    [CIRCUIT_AC_DC_SUPPLY] = {"AC-DC Supply", "ACDC", "Complete AC to DC power supply", TG_POWER_SUPPLY},
    [CIRCUIT_AC_DC_AMERICAN] = {"US 120V-12V", "US12", "American 120V/60Hz to 12V DC", TG_POWER_SUPPLY},
    // TI Analog Circuits - Amplifiers
    [CIRCUIT_DIFFERENCE_AMP] = {"Difference Amp", "DifA", "Op-amp difference amplifier (subtractor)", TG_OPAMPS},
    [CIRCUIT_TRANSIMPEDANCE] = {"Transimpedance", "TIA", "Transimpedance amplifier (I to V)", TG_OPAMPS},
    [CIRCUIT_INSTR_AMP] = {"Instr. Amp", "Inst", "Three op-amp instrumentation amplifier", TG_OPAMPS},
    // TI Analog Circuits - Filters
    [CIRCUIT_SALLEN_KEY_LP] = {"Sallen-Key LP", "S-K", "2nd order Sallen-Key low pass filter", TG_FILTERS},
    [CIRCUIT_BANDPASS_ACTIVE] = {"Active Bandpass", "BPF", "Active band pass filter", TG_FILTERS},
    [CIRCUIT_NOTCH_FILTER] = {"Notch Filter", "Notc", "Twin-T 60Hz notch filter", TG_FILTERS},
    // TI Analog Circuits - Signal Sources
    [CIRCUIT_WIEN_OSCILLATOR] = {"Wien Oscillator", "Wien", "Wien bridge sine wave oscillator", TG_OSCILLATORS},
    [CIRCUIT_CURRENT_SOURCE] = {"Current Source", "Isrc", "BJT constant current source", TG_TRANSISTORS},
    // TI Analog Circuits - Comparators/Detection
    [CIRCUIT_WINDOW_COMP] = {"Window Comp", "WCmp", "Window comparator (OV/UV detection)", TG_OPAMPS},
    [CIRCUIT_HYSTERESIS_COMP] = {"Schmitt Trigger", "Schm", "Comparator with hysteresis", TG_OPAMPS},
    // TI Analog Circuits - Power/Voltage
    [CIRCUIT_ZENER_REF] = {"Zener Reference", "Zref", "Zener diode voltage reference", TG_POWER_SUPPLY},
    [CIRCUIT_PRECISION_RECT] = {"Precision Rect", "PRec", "Precision full-wave rectifier", TG_POWER_SUPPLY},
    // Voltage Regulator Circuits
    [CIRCUIT_7805_REG] = {"7805 Regulator", "7805", "7805 fixed 5V regulator with filtering", TG_POWER_SUPPLY},
    [CIRCUIT_LM317_REG] = {"LM317 Adj Reg", "317", "LM317 adjustable regulator with voltage set", TG_POWER_SUPPLY},
    [CIRCUIT_TL431_REF] = {"TL431 Reference", "431", "TL431 precision shunt reference", TG_POWER_SUPPLY},
    // RLC Resonant Circuits
    [CIRCUIT_SERIES_RLC] = {"Series RLC", "sRLC", "Series RLC resonant circuit", TG_BASICS},
    [CIRCUIT_PARALLEL_RLC] = {"Parallel RLC", "pRLC", "Parallel RLC (tank) circuit", TG_BASICS},
    // Measurement & Detection Circuits
    [CIRCUIT_WHEATSTONE] = {"Wheatstone Bridge", "Whst", "Wheatstone bridge measurement circuit", TG_BASICS},
    [CIRCUIT_PEAK_DETECTOR] = {"Peak Detector", "Peak", "Op-amp peak detector circuit", TG_OPAMPS},
    // Signal Processing Circuits
    [CIRCUIT_CLAMPER] = {"Neg Clamper", "Clmp", "Negative clamper (DC restorer)", TG_POWER_SUPPLY},
    [CIRCUIT_PHASE_SHIFT_OSC] = {"Phase Shift Osc", "PhOsc", "RC phase shift oscillator (keep noise on)", TG_OSCILLATORS},
    [CIRCUIT_RC_BANDPASS] = {"RC Band-Pass", "RC BP", "Passive RC high-pass into low-pass", TG_FILTERS},
    [CIRCUIT_LC_LOWPASS] = {"LC Low-Pass", "LC LP", "2nd-order LC low-pass with load", TG_FILTERS},
    [CIRCUIT_ZENER_CLIPPER] = {"Zener Clipper", "ZClip", "Back-to-back zeners limit the swing", TG_POWER_SUPPLY},
    [CIRCUIT_VOLTAGE_DOUBLER] = {"Voltage Doubler", "Dblr", "Villard/Greinacher diode-capacitor doubler", TG_POWER_SUPPLY},
    [CIRCUIT_RELAXATION_OSC] = {"Relaxation Osc", "RelOsc", "Op-amp Schmitt + RC relaxation oscillator", TG_OSCILLATORS},
    [CIRCUIT_HALFWAVE_FILTERED] = {"HW Rect + Cap", "HW+C", "Half-wave rectifier with smoothing capacitor", TG_POWER_SUPPLY},
    [CIRCUIT_HV_345_LINE] = {"345 kV Line", "345kV", "100-mile 345 kV line, 600 MW load (per-phase)", TG_POWER_SYSTEMS},
    [CIRCUIT_HV_138_LINE_VAR] = {"138 kV Line + VAR", "138kV", "30-mile 138 kV line, lagging load, switchable cap bank", TG_POWER_SYSTEMS},
    [CIRCUIT_MV_FEEDER] = {"12.47 kV Feeder", "Feedr", "5-mile distribution feeder, 1 MW per phase", TG_POWER_SYSTEMS},
    [CIRCUIT_POLE_XFMR] = {"Pole Xfmr 120/240", "Pole", "7.2 kV to 240 V service transformer with a house load", TG_POWER_SYSTEMS},
    [CIRCUIT_GEN_GSU] = {"Generator + GSU", "GenSU", "18 kV generator, step-up to 345 kV, 600 MW", TG_POWER_SYSTEMS},
    [CIRCUIT_GRID_CHAIN] = {"Grid: 18 kV to 240 V", "Grid", "Generator to house through every voltage level", TG_POWER_SYSTEMS},
    [CIRCUIT_FERRANTI_LINE] = {"Ferranti (open line)", "Ferr", "200-mile 345 kV pi line, open end, switchable reactor", TG_POWER_SYSTEMS},
    [CIRCUIT_LINE_MODEL_LADDER] = {"Line Model Ladder", "Ladder", "Same line as R, R-L and pi: compare the load buses", TG_POWER_SYSTEMS},
    [CIRCUIT_DC_LINE_DROP] = {"Line Drop Basics", "Drop", "Battery, wire resistance, load: the simplest voltage drop", TG_BASICS},
    [CIRCUIT_PC_OVERCURRENT] = {"CT + 50/51 Overcurrent", "50/51", "CT 600:5, burden, rectify-hold, pickup comparator; pulsed fault", TG_POWER_SYSTEMS},
    [CIRCUIT_PC_DIFFERENTIAL] = {"87 Line Differential", "87L", "Two CTs in opposition: internal fault trips, through fault does not", TG_POWER_SYSTEMS},
    [CIRCUIT_PC_DISTANCE] = {"21 Distance Zone 1", "21Z1", "Replica impedance vs VT voltage: reach = 80 % of the line", TG_POWER_SYSTEMS},
    [CIRCUIT_PC_BREAKER_FAIL] = {"50BF Breaker Failure", "50BF", "TRIP AND current-present starts a 150 ms timer -> BFT", TG_POWER_SYSTEMS},
    [CIRCUIT_SIL_LOADING] = {"SIL Loading", "SIL", "200 mi 345 kV line at surge impedance load: flat voltage", TG_POWER_SYSTEMS},
    [CIRCUIT_SERIES_COMP] = {"Series Compensation", "SerC", "50 % series capacitor restores the voltage at 2 x SIL", TG_POWER_SYSTEMS},
    [CIRCUIT_HV_765_LINE] = {"765 kV Line (AEP)", "765kV", "300 mi six-bundle EHV line at ~2300 MW", TG_POWER_SYSTEMS},

    [CIRCUIT_TESLA_COIL] = {"Tesla Coil", "Tesla", "Spark-gap Tesla coil, 4x13 in toroid, streamer to a rod", TG_HIGH_VOLTAGE},
    [CIRCUIT_TESLA_COIL_BIG] = {"Tesla Coil (big top)", "TeslaB", "Retuned for an 8x24 in toroid: more energy, longer arc", TG_HIGH_VOLTAGE},
    [CIRCUIT_TESLA_COIL_DETUNED] = {"Tesla Coil (detuned)", "TeslaX", "Big toroid but the primary was not retuned: weak output", TG_HIGH_VOLTAGE},



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

// Helper to connect two component terminals with L-shaped orthogonal wires
// Routes: horizontal first (x1 to x2), then vertical (y1 to y2)
// This ensures all wires are strictly horizontal or vertical (no diagonals)
static void connect_terminals(Circuit *circuit, Component *c1, int t1, Component *c2, int t2) {
    float x1, y1, x2, y2;
    component_get_terminal_pos(c1, t1, &x1, &y1);
    component_get_terminal_pos(c2, t2, &x2, &y2);

    int n1 = circuit_find_or_create_node(circuit, x1, y1, 5.0f);
    int n2 = circuit_find_or_create_node(circuit, x2, y2, 5.0f);

    // Update node connections for components
    c1->node_ids[t1] = n1;
    c2->node_ids[t2] = n2;

    if (n1 == n2) return;  // Already same node

    // Check if already aligned (horizontal or vertical)
    float dx = x2 - x1;
    float dy = y2 - y1;
    if (fabsf(dx) < 1.0f || fabsf(dy) < 1.0f) {
        // Already aligned, single wire is orthogonal
        circuit_add_wire(circuit, n1, n2);
    } else {
        // Not aligned - create L-shaped route with corner at (x2, y1)
        // This goes: horizontal from (x1,y1) to (x2,y1), then vertical to (x2,y2)
        int corner = circuit_find_or_create_node(circuit, x2, y1, 5.0f);
        circuit_add_wire(circuit, n1, corner);
        circuit_add_wire(circuit, corner, n2);
    }
}

// Wire two nodes with an orthogonal route: straight if aligned, otherwise an L via (x_b, y_a)
static void wire_ortho(Circuit *circuit, int na, int nb) {
    Node *a = circuit_get_node(circuit, na), *b = circuit_get_node(circuit, nb);
    if (!a || !b || na == nb) { if (a && b && na != nb) circuit_add_wire(circuit, na, nb); return; }
    if (fabsf(a->x - b->x) < 1.0f || fabsf(a->y - b->y) < 1.0f) { circuit_add_wire(circuit, na, nb); return; }
    int corner = circuit_find_or_create_node(circuit, b->x, a->y, 5.0f);
    circuit_add_wire(circuit, na, corner);
    circuit_add_wire(circuit, corner, nb);
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
    // Frequency sweep 100 Hz -> 20000 Hz (log, 3 s each way, repeating) so the filter's
    // pass/stop behaviour is visible live; the readout under the source shows f.
    vsrc->props.ac_voltage.frequency_sweep.enabled = true;
    vsrc->props.ac_voltage.frequency_sweep.mode = SWEEP_LOG;
    vsrc->props.ac_voltage.frequency_sweep.start_value = 100;
    vsrc->props.ac_voltage.frequency_sweep.end_value = 20000;
    vsrc->props.ac_voltage.frequency_sweep.sweep_time = 3;
    vsrc->props.ac_voltage.frequency_sweep.repeat = true;
    vsrc->props.ac_voltage.frequency_sweep.bidirectional = true;

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
    // Frequency sweep 100 Hz -> 20000 Hz (log, 3 s each way, repeating) so the filter's
    // pass/stop behaviour is visible live; the readout under the source shows f.
    vsrc->props.ac_voltage.frequency_sweep.enabled = true;
    vsrc->props.ac_voltage.frequency_sweep.mode = SWEEP_LOG;
    vsrc->props.ac_voltage.frequency_sweep.start_value = 100;
    vsrc->props.ac_voltage.frequency_sweep.end_value = 20000;
    vsrc->props.ac_voltage.frequency_sweep.sweep_time = 3;
    vsrc->props.ac_voltage.frequency_sweep.repeat = true;
    vsrc->props.ac_voltage.frequency_sweep.bidirectional = true;

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
    // Frequency sweep 100 Hz -> 20000 Hz (log, 3 s each way, repeating) so the filter's
    // pass/stop behaviour is visible live; the readout under the source shows f.
    vsrc->props.ac_voltage.frequency_sweep.enabled = true;
    vsrc->props.ac_voltage.frequency_sweep.mode = SWEEP_LOG;
    vsrc->props.ac_voltage.frequency_sweep.start_value = 100;
    vsrc->props.ac_voltage.frequency_sweep.end_value = 20000;
    vsrc->props.ac_voltage.frequency_sweep.sweep_time = 3;
    vsrc->props.ac_voltage.frequency_sweep.repeat = true;
    vsrc->props.ac_voltage.frequency_sweep.bidirectional = true;

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
    // Frequency sweep 100 Hz -> 20000 Hz (log, 3 s each way, repeating) so the filter's
    // pass/stop behaviour is visible live; the readout under the source shows f.
    vsrc->props.ac_voltage.frequency_sweep.enabled = true;
    vsrc->props.ac_voltage.frequency_sweep.mode = SWEEP_LOG;
    vsrc->props.ac_voltage.frequency_sweep.start_value = 100;
    vsrc->props.ac_voltage.frequency_sweep.end_value = 20000;
    vsrc->props.ac_voltage.frequency_sweep.sweep_time = 3;
    vsrc->props.ac_voltage.frequency_sweep.repeat = true;
    vsrc->props.ac_voltage.frequency_sweep.bidirectional = true;

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
    wire_ortho(circuit, vcc_node, circuit_find_or_create_node(circuit, r1_top_x, vcc_y, 5.0f));
    wire_ortho(circuit, circuit_find_or_create_node(circuit, r1_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, r1_top_x, r1_top_y, 5.0f));
    r1->node_ids[0] = vcc_node;

    // Wire from Vcc line continuing right to Rc top (separate horizontal path)
    wire_ortho(circuit, circuit_find_or_create_node(circuit, r1_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc_top_x, vcc_y, 5.0f));
    wire_ortho(circuit, circuit_find_or_create_node(circuit, rc_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc_top_x, rc_top_y, 5.0f));
    rc->node_ids[0] = vcc_node;

    // === BASE BIAS NODE ===
    // Junction where R1 bottom, R2 top, Cin output, and BJT base connect
    int base_node = circuit_find_or_create_node(circuit, r1_bot_x, r1_bot_y, 5.0f);
    r1->node_ids[1] = base_node;

    // R2 top connects to same node (components are vertically aligned)
    r2->node_ids[0] = base_node;
    wire_ortho(circuit, base_node, circuit_find_or_create_node(circuit, r2_top_x, r2_top_y, 5.0f));

    // Cin output to base node (horizontal wire)
    wire_ortho(circuit, circuit_find_or_create_node(circuit, cin_out_x, cin_out_y, 5.0f), base_node);
    cin->node_ids[1] = base_node;

    // Base node to BJT base (horizontal wire)
    wire_ortho(circuit, base_node, circuit_find_or_create_node(circuit, base_x, base_y, 5.0f));
    npn->node_ids[0] = base_node;

    // === COLLECTOR NODE ===
    // Rc bottom and BJT collector connect
    int coll_node = circuit_find_or_create_node(circuit, coll_x, coll_y, 5.0f);
    npn->node_ids[1] = coll_node;

    // Rc bottom to collector (short vertical wire)
    wire_ortho(circuit, circuit_find_or_create_node(circuit, rc_bot_x, rc_bot_y, 5.0f), coll_node);
    rc->node_ids[1] = coll_node;

    // Collector to Cout (horizontal wire offset above collector)
    wire_ortho(circuit, coll_node, circuit_find_or_create_node(circuit, cout_in_x, coll_y, 5.0f));
    wire_ortho(circuit, circuit_find_or_create_node(circuit, cout_in_x, coll_y, 5.0f),
                     circuit_find_or_create_node(circuit, cout_in_x, cout_in_y, 5.0f));
    cout->node_ids[0] = coll_node;

    // === EMITTER NODE ===
    // BJT emitter to Re top
    int emit_node = circuit_find_or_create_node(circuit, emit_x, emit_y, 5.0f);
    npn->node_ids[2] = emit_node;
    wire_ortho(circuit, emit_node, circuit_find_or_create_node(circuit, re_top_x, re_top_y, 5.0f));
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
    Component *gnd_in = add_comp(circuit, COMP_GROUND, x - 100, y + 160, 0);

    // === INPUT COUPLING ===
    Component *cin = add_comp(circuit, COMP_CAPACITOR, x, y + 40, 0);
    cin->props.capacitor.capacitance = 10e-6;

    // === GATE BIAS DIVIDER ===
    // Rg1 (Vdd -> gate) and Rg2 (gate -> GND) set Vg = 12 * 330k / 1.33M = 2.98 V.
    // With Rs = 470 this biases Id ~ 2 mA (Vd ~ 7.5 V), squarely in saturation.
    // The coupling cap blocks the source's DC, so the divider is what turns the FET on.
    Component *rg1 = add_comp(circuit, COMP_RESISTOR, x + 80, y - 20, 90);
    rg1->props.resistor.resistance = 1000000.0;  // 1M
    Component *rg = add_comp(circuit, COMP_RESISTOR, x + 80, y + 80, 90);
    rg->props.resistor.resistance = 330000.0;    // 330k
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
    Component *label = add_comp(circuit, COMP_TEXT, x + 60, y - 160, 0);
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

    float rg1_top_x, rg1_top_y, rg1_bot_x, rg1_bot_y;
    component_get_terminal_pos(rg1, 0, &rg1_top_x, &rg1_top_y);
    component_get_terminal_pos(rg1, 1, &rg1_bot_x, &rg1_bot_y);

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

    // Wire from Vdd+ going right along the Vdd bus, with a tap for Rg1, then to Rd top
    int rg1_tap = circuit_find_or_create_node(circuit, rg1_top_x, vdd_y, 5.0f);
    circuit_add_wire(circuit, vdd_node, rg1_tap);
    circuit_add_wire(circuit, rg1_tap, circuit_find_or_create_node(circuit, rd_top_x, vdd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rd_top_x, vdd_y, 5.0f),
                     circuit_find_or_create_node(circuit, rd_top_x, rd_top_y, 5.0f));
    rd->node_ids[0] = vdd_node;

    // Rg1 top drops from the Vdd bus tap
    circuit_add_wire(circuit, rg1_tap, circuit_find_or_create_node(circuit, rg1_top_x, rg1_top_y, 5.0f));
    rg1->node_ids[0] = vdd_node;

    // === GATE NODE ===
    // Junction where Cin out, Rg1 bottom, Rg2 top, and NMOS gate connect
    int gate_node = circuit_find_or_create_node(circuit, rg_top_x, rg_top_y, 5.0f);
    rg->node_ids[0] = gate_node;
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rg1_bot_x, rg1_bot_y, 5.0f), gate_node);
    rg1->node_ids[1] = gate_node;
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

    return 14;
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
    Component *gnd_in = add_comp(circuit, COMP_GROUND, x - 100, y + 160, 0);

    // === INPUT COUPLING ===
    Component *cin = add_comp(circuit, COMP_CAPACITOR, x, y + 40, 0);
    cin->props.capacitor.capacitance = 10e-6;

    // === GATE BIAS DIVIDER ===
    // 1M/1M from Vdd sets Vg = 6 V, so Vs ~ 4 V and Id ~ 4 mA through Rs = 1k.
    // Without this the gate floats behind the coupling cap and the FET never turns on.
    Component *rg1 = add_comp(circuit, COMP_RESISTOR, x + 80, y - 20, 90);
    rg1->props.resistor.resistance = 1000000.0;  // 1M
    Component *rg2 = add_comp(circuit, COMP_RESISTOR, x + 80, y + 100, 90);
    rg2->props.resistor.resistance = 1000000.0;  // 1M
    Component *gnd_rg = add_comp(circuit, COMP_GROUND, x + 80, y + 160, 0);

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
    Component *label = add_comp(circuit, COMP_TEXT, x + 20, y - 160, 0);
    strncpy(label->props.text.text, "Source Follower (Gain~1)", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // === CONNECTIONS ===
    connect_terminals(circuit, vdd, 1, gnd_vdd, 0);
    connect_terminals(circuit, vin, 1, gnd_in, 0);
    connect_terminals(circuit, rs, 1, gnd_rs, 0);
    connect_terminals(circuit, rg2, 1, gnd_rg, 0);
    connect_terminals(circuit, vin, 0, cin, 0);

    // Get terminal positions
    float vdd_x, vdd_y;
    component_get_terminal_pos(vdd, 0, &vdd_x, &vdd_y);

    float cin_out_x, cin_out_y;
    component_get_terminal_pos(cin, 1, &cin_out_x, &cin_out_y);

    float rg1_top_x, rg1_top_y, rg1_bot_x, rg1_bot_y;
    component_get_terminal_pos(rg1, 0, &rg1_top_x, &rg1_top_y);
    component_get_terminal_pos(rg1, 1, &rg1_bot_x, &rg1_bot_y);
    float rg2_top_x, rg2_top_y;
    component_get_terminal_pos(rg2, 0, &rg2_top_x, &rg2_top_y);
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

    // Wire from Vdd+ along the bus (tap for Rg1) then down to drain
    int rg1_tap = circuit_find_or_create_node(circuit, rg1_top_x, vdd_y, 5.0f);
    circuit_add_wire(circuit, vdd_node, rg1_tap);
    circuit_add_wire(circuit, rg1_tap, circuit_find_or_create_node(circuit, drain_x, vdd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, drain_x, vdd_y, 5.0f),
                     circuit_find_or_create_node(circuit, drain_x, drain_y, 5.0f));
    nmos->node_ids[1] = vdd_node;

    // Rg1 top from the Vdd bus tap
    circuit_add_wire(circuit, rg1_tap, circuit_find_or_create_node(circuit, rg1_top_x, rg1_top_y, 5.0f));
    rg1->node_ids[0] = vdd_node;

    // === GATE NODE ===
    int gate_node = circuit_find_or_create_node(circuit, gate_x, gate_y, 5.0f);
    nmos->node_ids[0] = gate_node;

    // Cin output to gate (horizontal), with the bias divider tapping the gate wire midway
    int gate_tap = circuit_find_or_create_node(circuit, rg1_bot_x, cin_out_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cin_out_x, cin_out_y, 5.0f), gate_tap);
    circuit_add_wire(circuit, gate_tap, gate_node);
    cin->node_ids[1] = gate_node;
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, rg1_bot_x, rg1_bot_y, 5.0f), gate_tap);
    rg1->node_ids[1] = gate_node;
    circuit_add_wire(circuit, gate_tap, circuit_find_or_create_node(circuit, rg2_top_x, rg2_top_y, 5.0f));
    rg2->node_ids[0] = gate_node;
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

    return 13;
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
    wire_ortho(circuit, vcc_node, circuit_find_or_create_node(circuit, r1a_top_x, vcc_y, 5.0f));
    wire_ortho(circuit, circuit_find_or_create_node(circuit, r1a_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, r1a_top_x, r1a_top_y, 5.0f));
    r1a->node_ids[0] = vcc_node;

    // Continue bus to Rc1 top
    wire_ortho(circuit, circuit_find_or_create_node(circuit, r1a_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc1_top_x, vcc_y, 5.0f));
    wire_ortho(circuit, circuit_find_or_create_node(circuit, rc1_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc1_top_x, rc1_top_y, 5.0f));
    rc1->node_ids[0] = vcc_node;

    // Continue bus to R1b top
    wire_ortho(circuit, circuit_find_or_create_node(circuit, rc1_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, r1b_top_x, vcc_y, 5.0f));
    wire_ortho(circuit, circuit_find_or_create_node(circuit, r1b_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, r1b_top_x, r1b_top_y, 5.0f));
    r1b->node_ids[0] = vcc_node;

    // Continue bus to Rc2 top
    wire_ortho(circuit, circuit_find_or_create_node(circuit, r1b_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc2_top_x, vcc_y, 5.0f));
    wire_ortho(circuit, circuit_find_or_create_node(circuit, rc2_top_x, vcc_y, 5.0f),
                     circuit_find_or_create_node(circuit, rc2_top_x, rc2_top_y, 5.0f));
    rc2->node_ids[0] = vcc_node;

    // === STAGE 1 BASE NODE ===
    int base1_node = circuit_find_or_create_node(circuit, r1a_bot_x, r1a_bot_y, 5.0f);
    r1a->node_ids[1] = base1_node;
    r2a->node_ids[0] = base1_node;
    wire_ortho(circuit, base1_node, circuit_find_or_create_node(circuit, r2a_top_x, r2a_top_y, 5.0f));

    // C1 out to base node (horizontal)
    wire_ortho(circuit, circuit_find_or_create_node(circuit, c1_out_x, c1_out_y, 5.0f), base1_node);
    c1->node_ids[1] = base1_node;

    // Base node to Q1 base (horizontal)
    wire_ortho(circuit, base1_node, circuit_find_or_create_node(circuit, base1_x, base1_y, 5.0f));
    q1->node_ids[0] = base1_node;

    // === STAGE 1 COLLECTOR NODE ===
    int coll1_node = circuit_find_or_create_node(circuit, coll1_x, coll1_y, 5.0f);
    q1->node_ids[1] = coll1_node;

    // Rc1 bottom to collector
    wire_ortho(circuit, circuit_find_or_create_node(circuit, rc1_bot_x, rc1_bot_y, 5.0f), coll1_node);
    rc1->node_ids[1] = coll1_node;

    // Collector to C2 input (horizontal then vertical)
    wire_ortho(circuit, coll1_node, circuit_find_or_create_node(circuit, c2_in_x, coll1_y, 5.0f));
    wire_ortho(circuit, circuit_find_or_create_node(circuit, c2_in_x, coll1_y, 5.0f),
                     circuit_find_or_create_node(circuit, c2_in_x, c2_in_y, 5.0f));
    c2->node_ids[0] = coll1_node;

    // === STAGE 1 EMITTER NODE ===
    int emit1_node = circuit_find_or_create_node(circuit, emit1_x, emit1_y, 5.0f);
    q1->node_ids[2] = emit1_node;
    wire_ortho(circuit, emit1_node, circuit_find_or_create_node(circuit, re1_top_x, re1_top_y, 5.0f));
    re1->node_ids[0] = emit1_node;

    // === STAGE 2 BASE NODE ===
    int base2_node = circuit_find_or_create_node(circuit, r1b_bot_x, r1b_bot_y, 5.0f);
    r1b->node_ids[1] = base2_node;
    r2b->node_ids[0] = base2_node;
    wire_ortho(circuit, base2_node, circuit_find_or_create_node(circuit, r2b_top_x, r2b_top_y, 5.0f));

    // C2 out to base node (horizontal)
    wire_ortho(circuit, circuit_find_or_create_node(circuit, c2_out_x, c2_out_y, 5.0f), base2_node);
    c2->node_ids[1] = base2_node;

    // Base node to Q2 base (horizontal)
    wire_ortho(circuit, base2_node, circuit_find_or_create_node(circuit, base2_x, base2_y, 5.0f));
    q2->node_ids[0] = base2_node;

    // === STAGE 2 COLLECTOR NODE ===
    int coll2_node = circuit_find_or_create_node(circuit, coll2_x, coll2_y, 5.0f);
    q2->node_ids[1] = coll2_node;

    // Rc2 bottom to collector
    wire_ortho(circuit, circuit_find_or_create_node(circuit, rc2_bot_x, rc2_bot_y, 5.0f), coll2_node);
    rc2->node_ids[1] = coll2_node;

    // Collector to C3 input (horizontal then vertical)
    wire_ortho(circuit, coll2_node, circuit_find_or_create_node(circuit, c3_in_x, coll2_y, 5.0f));
    wire_ortho(circuit, circuit_find_or_create_node(circuit, c3_in_x, coll2_y, 5.0f),
                     circuit_find_or_create_node(circuit, c3_in_x, c3_in_y, 5.0f));
    c3->node_ids[0] = coll2_node;

    // === STAGE 2 EMITTER NODE ===
    int emit2_node = circuit_find_or_create_node(circuit, emit2_x, emit2_y, 5.0f);
    q2->node_ids[2] = emit2_node;
    wire_ortho(circuit, emit2_node, circuit_find_or_create_node(circuit, re2_top_x, re2_top_y, 5.0f));
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
    // Input sources on far left/right (DC-biased), base resistors feed into bases

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

    // Base resistors - horizontal orientation. The pair is direct-coupled: the input
    // sources carry a 6 V DC offset that biases both bases (single-supply, tail to GND),
    // so Ve ~ 5.3 V, Itail ~ 0.53 mA and each collector sits near 10.8 V.
    // (A capacitor-coupled input would leave the bases with no DC path and both BJTs off.)
    Component *cin1 = add_comp(circuit, COMP_RESISTOR, x - 40, y + 40, 0);
    cin1->props.resistor.resistance = 1000.0;
    Component *cin2 = add_comp(circuit, COMP_RESISTOR, x + 240, y + 40, 180);
    cin2->props.resistor.resistance = 1000.0;

    // AC input sources - on far left and right, DC-biased at 6 V, anti-phase
    Component *vin1 = add_comp(circuit, COMP_AC_VOLTAGE, x - 100, y + 80, 0);
    vin1->props.ac_voltage.amplitude = 0.05;
    vin1->props.ac_voltage.frequency = 1000.0;
    vin1->props.ac_voltage.offset = 6.0;
    Component *vin2 = add_comp(circuit, COMP_AC_VOLTAGE, x + 300, y + 80, 0);
    vin2->props.ac_voltage.amplitude = 0.05;
    vin2->props.ac_voltage.frequency = 1000.0;
    vin2->props.ac_voltage.phase = 180.0;
    vin2->props.ac_voltage.offset = 6.0;
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
    // Class-B complementary emitter follower.
    //
    //   Vcc+ ----+---- Q1.C              Q2 is rotated 180 so its EMITTER faces up and
    //            |                       sits directly under Q1's emitter. The previous
    //   Vin --+--+-- Q1.B  Q1.E --+--- Rload   layout ran the Vee wire straight through
    //         |                   |            Q2's emitter terminal and shorted the
    //         +--------- Q2.B     +-- Q2.E     output node to Vee.
    //                        Q2.C --- Vee-
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x - 60, y - 80, 0);
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 12.0;
    // Negative rail: its + terminal feeds the PNP collector node, so -12 V puts that
    // node at -12 V relative to the shared ground.
    // Placed between the base drop (x-20) and Q2 so its wire to Q2.C never crosses anything;
    // it gets its own ground symbol directly below.
    Component *vee = add_comp(circuit, COMP_DC_VOLTAGE, x + 20, y + 140, 0);
    vee->props.dc_voltage.voltage = -12.0;
    Component *gnd_vee = add_comp(circuit, COMP_GROUND, x + 20, y + 200, 0);

    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x - 140, y + 20, 0);
    vin->props.ac_voltage.amplitude = 5.0;
    vin->props.ac_voltage.frequency = 1000.0;

    // Single ground at bottom-left (all supply/source returns bus to it)
    Component *gnd = add_comp(circuit, COMP_GROUND, x - 60, y + 200, 0);

    Component *q1 = add_comp(circuit, COMP_NPN_BJT, x + 60, y - 20, 0);     // B left, C top-right, E bottom-right
    q1->props.bjt.bf = 100;
    Component *q2 = add_comp(circuit, COMP_PNP_BJT, x + 120, y + 60, 180);  // B right, E top-left, C bottom-left
    q2->props.bjt.bf = 100;

    // Load on the right, its top terminal on the output rail (y), bottom to its own ground
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 240, y + 40, 90);
    rload->props.resistor.resistance = 100.0;
    Component *gnd_load = add_comp(circuit, COMP_GROUND, x + 240, y + 100, 0);

    Component *label = add_comp(circuit, COMP_TEXT, x + 20, y - 160, 0);
    strncpy(label->props.text.text, "Push-Pull Output", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // --- terminal positions ---
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
    float rload_top_x, rload_top_y;
    component_get_terminal_pos(rload, 0, &rload_top_x, &rload_top_y);

    int gnd_node = circuit_find_or_create_node(circuit, gnd_x, gnd_y, 5.0f);
    gnd->node_ids[0] = gnd_node;
    float gnd_bus_x = vcc_neg_x - 40;  // vertical return bus left of the sources

    // --- Vcc+ -> Q1 collector (right along the top, then down) ---
    int vcc_node = circuit_find_or_create_node(circuit, vcc_pos_x, vcc_pos_y, 5.0f);
    vcc->node_ids[0] = vcc_node;
    int c1_corner = circuit_find_or_create_node(circuit, coll1_x, vcc_pos_y, 5.0f);
    int c1_node = circuit_find_or_create_node(circuit, coll1_x, coll1_y, 5.0f);
    circuit_add_wire(circuit, vcc_node, c1_corner);
    circuit_add_wire(circuit, c1_corner, c1_node);
    q1->node_ids[1] = vcc_node;

    // --- Vcc- -> ground bus ---
    int vcc_neg_node = circuit_find_or_create_node(circuit, vcc_neg_x, vcc_neg_y, 5.0f);
    int bus_top = circuit_find_or_create_node(circuit, gnd_bus_x, vcc_neg_y, 5.0f);
    int bus_bot = circuit_find_or_create_node(circuit, gnd_bus_x, gnd_y, 5.0f);
    circuit_add_wire(circuit, vcc_neg_node, bus_top);
    circuit_add_wire(circuit, bus_top, bus_bot);
    circuit_add_wire(circuit, bus_bot, gnd_node);
    vcc->node_ids[1] = gnd_node;

    // --- Vee+ -> Q2 collector: right along y+100, then up into the collector lead ---
    int vee_node = circuit_find_or_create_node(circuit, vee_pos_x, vee_pos_y, 5.0f);
    vee->node_ids[0] = vee_node;
    int c2_node = circuit_find_or_create_node(circuit, coll2_x, coll2_y, 5.0f);
    int c2_corner = circuit_find_or_create_node(circuit, coll2_x, vee_pos_y, 5.0f);
    circuit_add_wire(circuit, vee_node, c2_corner);
    circuit_add_wire(circuit, c2_corner, c2_node);
    q2->node_ids[1] = vee_node;

    // --- Vee- -> its own ground ---
    connect_terminals(circuit, vee, 1, gnd_vee, 0);

    // --- Vin+ -> both bases ---
    // Bus along the input row to a split point, then: straight on to Q1.B; and down,
    // under Q2, up its right side to Q2.B (base is on the right after the 180 rotation).
    int base_node = circuit_find_or_create_node(circuit, vin_pos_x, vin_pos_y, 5.0f);
    vin->node_ids[0] = base_node;
    float split_x = x - 20;
    float under_y = y + 240;      // below the Vee ground symbol
    float right_x = base2_x + 40;
    int split = circuit_find_or_create_node(circuit, split_x, vin_pos_y, 5.0f);
    int b1_node = circuit_find_or_create_node(circuit, base1_x, base1_y, 5.0f);
    circuit_add_wire(circuit, base_node, split);
    circuit_add_wire(circuit, split, b1_node);
    int d1 = circuit_find_or_create_node(circuit, split_x, under_y, 5.0f);
    int d2 = circuit_find_or_create_node(circuit, right_x, under_y, 5.0f);
    int d3 = circuit_find_or_create_node(circuit, right_x, base2_y, 5.0f);
    int b2_node = circuit_find_or_create_node(circuit, base2_x, base2_y, 5.0f);
    circuit_add_wire(circuit, split, d1);
    circuit_add_wire(circuit, d1, d2);
    circuit_add_wire(circuit, d2, d3);
    circuit_add_wire(circuit, d3, b2_node);
    q1->node_ids[0] = base_node;
    q2->node_ids[0] = base_node;

    // --- Vin- -> ground bus ---
    int vin_neg_node = circuit_find_or_create_node(circuit, vin_neg_x, vin_neg_y, 5.0f);
    int bus_vin = circuit_find_or_create_node(circuit, gnd_bus_x, vin_neg_y, 5.0f);
    circuit_add_wire(circuit, vin_neg_node, bus_vin);
    circuit_add_wire(circuit, bus_vin, bus_bot);
    vin->node_ids[1] = gnd_node;

    // --- Output: Q1.E and Q2.E joined, then right to the load ---
    int out_node = circuit_find_or_create_node(circuit, emit1_x, emit1_y, 5.0f);
    int e_corner = circuit_find_or_create_node(circuit, emit1_x, emit2_y, 5.0f);
    int e2_node = circuit_find_or_create_node(circuit, emit2_x, emit2_y, 5.0f);
    circuit_add_wire(circuit, out_node, e_corner);
    circuit_add_wire(circuit, e_corner, e2_node);
    int load_corner = circuit_find_or_create_node(circuit, rload_top_x, emit1_y, 5.0f);
    int load_top = circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f);
    circuit_add_wire(circuit, out_node, load_corner);
    circuit_add_wire(circuit, load_corner, load_top);
    q1->node_ids[2] = out_node;
    q2->node_ids[2] = out_node;
    rload->node_ids[0] = out_node;
    connect_terminals(circuit, rload, 1, gnd_load, 0);

    return 10;
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
    Component *r1a = add_comp(circuit, COMP_RESISTOR, x + 240, y - 100, 90);   // bottom terminal lands on the inv junction at y-60
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

    // V1 -> op1 non-inverting input via a corner at x+60 (the old horizontal-first route ran
    // straight through op1's inverting terminal and shorted the two inputs together)
    {
        int n_v1 = circuit_find_or_create_node(circuit, v1_pos_x, v1_pos_y, 5.0f);
        int k1 = circuit_find_or_create_node(circuit, x + 60, v1_pos_y, 5.0f);
        int k2 = circuit_find_or_create_node(circuit, x + 60, op1_noninv_y, 5.0f);
        int n_ni = circuit_find_or_create_node(circuit, op1_noninv_x, op1_noninv_y, 5.0f);
        circuit_add_wire(circuit, n_v1, k1);
        circuit_add_wire(circuit, k1, k2);
        circuit_add_wire(circuit, k2, n_ni);
    }

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
    // Frequency sweep 100 Hz -> 20000 Hz (log, 3 s each way, repeating) so the filter's
    // pass/stop behaviour is visible live; the readout under the source shows f.
    vsrc->props.ac_voltage.frequency_sweep.enabled = true;
    vsrc->props.ac_voltage.frequency_sweep.mode = SWEEP_LOG;
    vsrc->props.ac_voltage.frequency_sweep.start_value = 100;
    vsrc->props.ac_voltage.frequency_sweep.end_value = 20000;
    vsrc->props.ac_voltage.frequency_sweep.sweep_time = 3;
    vsrc->props.ac_voltage.frequency_sweep.repeat = true;
    vsrc->props.ac_voltage.frequency_sweep.bidirectional = true;
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
    wire_ortho(circuit, vcc_node, vcc_rail);
    vcc->node_ids[0] = vcc_node;

    int cdec_top_node = circuit_find_or_create_node(circuit, cdec_top_x, cdec_top_y, 5.0f);
    int corner_cdec = circuit_find_or_create_node(circuit, cdec_top_x, y - 120, 5.0f);
    wire_ortho(circuit, vcc_rail, corner_cdec);
    wire_ortho(circuit, corner_cdec, cdec_top_node);
    c_dec->node_ids[0] = cdec_top_node;

    Component *gnd_cdec = add_comp(circuit, COMP_GROUND, x + 60, y - 20, 0);
    connect_terminals(circuit, c_dec, 1, gnd_cdec, 0);

    // === INPUT SECTION (left) ===
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 40, 0);
    vsrc->props.ac_voltage.amplitude = 1.0;
    vsrc->props.ac_voltage.frequency = 1000.0;
    // Frequency sweep 100 Hz -> 20000 Hz (log, 3 s each way, repeating) so the filter's
    // pass/stop behaviour is visible live; the readout under the source shows f.
    vsrc->props.ac_voltage.frequency_sweep.enabled = true;
    vsrc->props.ac_voltage.frequency_sweep.mode = SWEEP_LOG;
    vsrc->props.ac_voltage.frequency_sweep.start_value = 100;
    vsrc->props.ac_voltage.frequency_sweep.end_value = 20000;
    vsrc->props.ac_voltage.frequency_sweep.sweep_time = 3;
    vsrc->props.ac_voltage.frequency_sweep.repeat = true;
    vsrc->props.ac_voltage.frequency_sweep.bidirectional = true;
    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, y + 100, 0);
    connect_terminals(circuit, vsrc, 1, gnd1, 0);

    // === FILTER NETWORK ===
    // Input resistor R1
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 140, y + 20, 0);
    r1->props.resistor.resistance = 10000.0;

    // Feedback capacitor C1 (input to inverting)
    Component *c1 = add_comp(circuit, COMP_CAPACITOR, x + 60, y + 20, 0);   // series input cap: (20,20)-(100,20), right end = R1 left
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
    // source -> (x+20) -> C1 -> R1 -> inverting input (C1 right end sits on R1's left end)
    int vsrc_corner = circuit_find_or_create_node(circuit, x + 20, vsrc_pos_y, 5.0f);
    int c1_left_node = circuit_find_or_create_node(circuit, x + 20, r1_left_y, 5.0f);
    int r1_left_node = circuit_find_or_create_node(circuit, r1_left_x, r1_left_y, 5.0f);
    wire_ortho(circuit, vsrc_node, vsrc_corner);
    wire_ortho(circuit, vsrc_corner, c1_left_node);
    vsrc->node_ids[0] = vsrc_node;
    c1->node_ids[0] = c1_left_node;
    c1->node_ids[1] = r1_left_node;
    r1->node_ids[0] = r1_left_node;

    // R1 to junction (inverting input area)
    float r1_right_x, r1_right_y;
    component_get_terminal_pos(r1, 1, &r1_right_x, &r1_right_y);
    float opamp_inv_x, opamp_inv_y;
    component_get_terminal_pos(opamp, 0, &opamp_inv_x, &opamp_inv_y);

    int inv_junc = circuit_find_or_create_node(circuit, r1_right_x, opamp_inv_y, 5.0f);
    int r1_right_node = circuit_find_or_create_node(circuit, r1_right_x, r1_right_y, 5.0f);
    wire_ortho(circuit, r1_right_node, inv_junc);
    r1->node_ids[1] = r1_right_node;

    // Junction to op-amp inverting
    int opamp_inv_node = circuit_find_or_create_node(circuit, opamp_inv_x, opamp_inv_y, 5.0f);
    wire_ortho(circuit, inv_junc, opamp_inv_node);
    opamp->node_ids[0] = opamp_inv_node;

    // C1 from junction up, then across
    float c1_left_x, c1_left_y;
    component_get_terminal_pos(c1, 0, &c1_left_x, &c1_left_y);
    float c1_right_x, c1_right_y;
    component_get_terminal_pos(c1, 1, &c1_right_x, &c1_right_y);


    // R2 and C2 from junction up to feedback level
    float r2_left_x, r2_left_y;
    component_get_terminal_pos(r2, 0, &r2_left_x, &r2_left_y);
    float c2_left_x, c2_left_y;
    component_get_terminal_pos(c2, 0, &c2_left_x, &c2_left_y);

    int corner2 = circuit_find_or_create_node(circuit, opamp_inv_x, r2_left_y, 5.0f);
    int r2_left_node = circuit_find_or_create_node(circuit, r2_left_x, r2_left_y, 5.0f);
    int c2_left_node = circuit_find_or_create_node(circuit, c2_left_x, c2_left_y, 5.0f);
    wire_ortho(circuit, inv_junc, corner2);
    wire_ortho(circuit, corner2, r2_left_node);
    int corner_c2 = circuit_find_or_create_node(circuit, opamp_inv_x, c2_left_y, 5.0f);
    wire_ortho(circuit, corner2, corner_c2);
    wire_ortho(circuit, corner_c2, c2_left_node);
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
    wire_ortho(circuit, opamp_noninv_node, corner3);
    wire_ortho(circuit, corner3, gnd2_node);
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
    wire_ortho(circuit, out_node, out_junc);
    wire_ortho(circuit, out_junc, corner5);
    wire_ortho(circuit, corner5, r2_right_node);
    r2->node_ids[1] = r2_right_node;

    int c2_right_node = circuit_find_or_create_node(circuit, c2_right_x, c2_right_y, 5.0f);
    int corner6 = circuit_find_or_create_node(circuit, opamp_out_x + 20, c2_right_y, 5.0f);
    wire_ortho(circuit, corner5, corner6);
    wire_ortho(circuit, corner6, c2_right_node);
    c2->node_ids[1] = c2_right_node;


    // Output to load resistor
    int rload_top_node = circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f);
    int load_corner = circuit_find_or_create_node(circuit, rload_top_x, opamp_out_y, 5.0f);
    wire_ortho(circuit, out_junc, load_corner);
    wire_ortho(circuit, load_corner, rload_top_node);
    rload->node_ids[0] = rload_top_node;

    return 14;
}

// Twin-T Notch Filter (60Hz rejection)
static int place_notch_filter(Circuit *circuit, float x, float y) {
    // Twin-T: T1 = R - R with 2C to ground from its middle, T2 = C - C with R/2 to ground
    // from its middle; both T outputs join. The two paths cancel exactly at
    // f0 = 1/(2*pi*R*C) = 60 Hz (R = 26.5k, C = 100n). Source sweeps 10-300 Hz.
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 40, 0);              // +(0,0) -(0,80)
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 1.0;
    vsrc->props.ac_voltage.frequency = 60.0;
    vsrc->props.ac_voltage.frequency_sweep.enabled = true;
    vsrc->props.ac_voltage.frequency_sweep.mode = SWEEP_LOG;
    vsrc->props.ac_voltage.frequency_sweep.start_value = 10;
    vsrc->props.ac_voltage.frequency_sweep.end_value = 300;
    vsrc->props.ac_voltage.frequency_sweep.sweep_time = 4;
    vsrc->props.ac_voltage.frequency_sweep.repeat = true;
    vsrc->props.ac_voltage.frequency_sweep.bidirectional = true;
    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, y + 100, 0);
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 100, y - 40, 0);            // (60,-40)-(140,-40)
    r1->props.resistor.resistance = 26525.0;
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 220, y - 40, 0);            // (180,-40)-(260,-40)
    r2->props.resistor.resistance = 26525.0;
    Component *ca = add_comp(circuit, COMP_CAPACITOR, x + 160, y, 90);               // (160,-40)-(160,40)  2C
    ca->props.capacitor.capacitance = 200e-9;
    Component *gnd_a = add_comp(circuit, COMP_GROUND, x + 160, y + 60, 0);           // terminal (160,40)
    Component *c2 = add_comp(circuit, COMP_CAPACITOR, x + 100, y + 100, 0);          // (60,100)-(140,100)
    c2->props.capacitor.capacitance = 100e-9;
    Component *c3 = add_comp(circuit, COMP_CAPACITOR, x + 220, y + 100, 0);          // (180,100)-(260,100)
    c3->props.capacitor.capacitance = 100e-9;
    Component *rb = add_comp(circuit, COMP_RESISTOR, x + 160, y + 160, 90);          // (160,120)-(160,200)  R/2
    rb->props.resistor.resistance = 13262.0;
    Component *gnd_b = add_comp(circuit, COMP_GROUND, x + 160, y + 220, 0);          // terminal (160,200)
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 340, y + 140, 90);       // (340,100)-(340,180)
    rload->props.resistor.resistance = 10000.0;
    Component *gnd_l = add_comp(circuit, COMP_GROUND, x + 340, y + 200, 0);          // terminal (340,180)
    Component *label = add_comp(circuit, COMP_TEXT, x + 80, y - 100, 0);
    strncpy(label->props.text.text, "Twin-T Notch (60 Hz)", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

#define NN(cx, cy) circuit_find_or_create_node(circuit, (cx), (cy), 5.0f)
#define NW(a_, b_) circuit_add_wire(circuit, (a_), (b_))
    connect_terminals(circuit, vsrc, 1, gnd1, 0);
    connect_terminals(circuit, ca, 1, gnd_a, 0);
    connect_terminals(circuit, rb, 1, gnd_b, 0);
    connect_terminals(circuit, rload, 1, gnd_l, 0);
    // input junction at (60,0) feeds both T's
    int vin = NN(x, y), j_in = NN(x + 60, y), r1l = NN(x + 60, y - 40), c2l = NN(x + 60, y + 100);
    NW(vin, j_in); NW(j_in, r1l); NW(j_in, c2l);
    // T1 middle (160,-40): R1 right, R2 left, Ca top
    int A = NN(x + 160, y - 40), r1r = NN(x + 140, y - 40), r2l = NN(x + 180, y - 40);
    NW(r1r, A); NW(A, r2l);
    // T2 middle (160,100): C2 right, C3 left, Rb top
    int B = NN(x + 160, y + 100), c2r = NN(x + 140, y + 100), c3l = NN(x + 180, y + 100), rbt = NN(x + 160, y + 120);
    NW(c2r, B); NW(B, c3l); NW(B, rbt);
    // outputs join at x+300 and drive the load
    int r2r = NN(x + 260, y - 40), c3r = NN(x + 260, y + 100), o_t = NN(x + 300, y - 40), o_b = NN(x + 300, y + 100), ld = NN(x + 340, y + 100);
    NW(r2r, o_t); NW(o_t, o_b); NW(c3r, o_b); NW(o_b, ld);
    r1->node_ids[0] = r1l; r1->node_ids[1] = r1r; r2->node_ids[0] = r2l; r2->node_ids[1] = r2r;
    ca->node_ids[0] = A; c2->node_ids[0] = c2l; c2->node_ids[1] = c2r; c3->node_ids[0] = c3l; c3->node_ids[1] = c3r;
    rb->node_ids[0] = rbt; rload->node_ids[0] = ld; vsrc->node_ids[0] = vin;
#undef NN
#undef NW
    return 13;
}
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
    rf->props.resistor.resistance = 22000.0; // Rf: gain 3.2 (> 3 needed) so oscillation grows

    // Rg is horizontal, left of the inverting junction. (A vertical Rg at x+120 put its
    // ground terminal exactly on the Wien network's corner node at (x+120, y+60) and
    // grounded the non-inverting input, so the oscillator could never start.)
    Component *rg = add_comp(circuit, COMP_RESISTOR, x + 60, y + 20, 0);
    rg->props.resistor.resistance = 10000.0; // Rg, gain = 1 + Rf/Rg = 3.2
    Component *gnd_rg = add_comp(circuit, COMP_GROUND, x + 20, y + 80, 0);
    connect_terminals(circuit, rg, 0, gnd_rg, 0);

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

    // Start-up kick: a single short pulse in series with R2's ground return injects a
    // small perturbation into the Wien network. A noiseless ideal loop sitting at exactly
    // 0 V has nothing to amplify and would otherwise stay silent forever.
    Component *kick = add_comp(circuit, COMP_PULSE_SOURCE, x + 120, y + 200, 0);
    kick->props.pulse_source.v_low = 0.0;
    kick->props.pulse_source.v_high = 0.5;
    kick->props.pulse_source.pulse_width = 50e-6;
    kick->props.pulse_source.period = 100.0;      // effectively one-shot
    Component *gnd_r2 = add_comp(circuit, COMP_GROUND, x + 120, y + 260, 0);
    Component *gnd_c2 = add_comp(circuit, COMP_GROUND, x + 160, y + 180, 0);
    connect_terminals(circuit, r2, 1, kick, 0);
    connect_terminals(circuit, kick, 1, gnd_r2, 0);
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

    // Rg right end to the inverting junction at x+120, then on to the inverting input
    float rg_top_x, rg_top_y;
    component_get_terminal_pos(rg, 1, &rg_top_x, &rg_top_y);
    int rg_top_node = circuit_find_or_create_node(circuit, rg_top_x, rg_top_y, 5.0f);
    int corner_inv = circuit_find_or_create_node(circuit, x + 120, opamp_inv_y, 5.0f);
    circuit_add_wire(circuit, rg_top_node, corner_inv);
    circuit_add_wire(circuit, corner_inv, inv_node);
    rg->node_ids[1] = rg_top_node;

    // Rf left to inverting junction
    float rf_left_x, rf_left_y;
    component_get_terminal_pos(rf, 0, &rf_left_x, &rf_left_y);
    float rf_right_x, rf_right_y;
    component_get_terminal_pos(rf, 1, &rf_right_x, &rf_right_y);
    int rf_left_node = circuit_find_or_create_node(circuit, rf_left_x, rf_left_y, 5.0f);
    int corner_rf = circuit_find_or_create_node(circuit, x + 120, rf_left_y, 5.0f);
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
    int corner_fb1 = circuit_find_or_create_node(circuit, c1_right_x, y + 300, 5.0f);
    int corner_fb2 = circuit_find_or_create_node(circuit, x + 80, y + 300, 5.0f);
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
    return 17;
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
    Component *comp_lo = add_comp(circuit, COMP_OPAMP, x + 280, y + 60, 0);
    comp_lo->props.opamp.ideal = true;
    // Output summing resistors: each comparator drives the LED node through 10k, so the
    // LED only lights when BOTH outputs are high (inside the window). Tying two push-pull
    // op-amp outputs directly together would make them fight.
    Component *r_hi = add_comp(circuit, COMP_RESISTOR, x + 360, y - 60, 90);
    r_hi->props.resistor.resistance = 10000.0;
    Component *r_lo = add_comp(circuit, COMP_RESISTOR, x + 360, y + 20, 90);
    r_lo->props.resistor.resistance = 10000.0;

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

    // Outputs summed through R_hi / R_lo into the LED node (wired-AND behaviour):
    // both high -> ~2.9 mA into the LED; either low -> net current negative, LED off.
    float comp_hi_out_x, comp_hi_out_y;
    component_get_terminal_pos(comp_hi, 2, &comp_hi_out_x, &comp_hi_out_y);
    float comp_lo_out_x, comp_lo_out_y;
    component_get_terminal_pos(comp_lo, 2, &comp_lo_out_x, &comp_lo_out_y);
    float rpu_bot_x, rpu_bot_y;
    component_get_terminal_pos(rpu, 1, &rpu_bot_x, &rpu_bot_y);
    float led_top_x, led_top_y;
    component_get_terminal_pos(led, 0, &led_top_x, &led_top_y);
    float r_hi_top_x, r_hi_top_y, r_hi_bot_x, r_hi_bot_y;
    component_get_terminal_pos(r_hi, 0, &r_hi_top_x, &r_hi_top_y);
    component_get_terminal_pos(r_hi, 1, &r_hi_bot_x, &r_hi_bot_y);
    float r_lo_top_x, r_lo_top_y, r_lo_bot_x, r_lo_bot_y;
    component_get_terminal_pos(r_lo, 0, &r_lo_top_x, &r_lo_top_y);
    component_get_terminal_pos(r_lo, 1, &r_lo_bot_x, &r_lo_bot_y);

    // The junction sits at R_hi bottom == R_lo top
    int out_junc = circuit_find_or_create_node(circuit, r_hi_bot_x, r_hi_bot_y, 5.0f);
    r_hi->node_ids[1] = out_junc;
    r_lo->node_ids[0] = circuit_find_or_create_node(circuit, r_lo_top_x, r_lo_top_y, 5.0f);

    // comp_hi out -> R_hi top (straight horizontal), comp_lo out -> R_lo bottom (straight)
    int hi_out_node = circuit_find_or_create_node(circuit, comp_hi_out_x, comp_hi_out_y, 5.0f);
    int r_hi_top_node = circuit_find_or_create_node(circuit, r_hi_top_x, r_hi_top_y, 5.0f);
    circuit_add_wire(circuit, hi_out_node, r_hi_top_node);
    comp_hi->node_ids[2] = hi_out_node;
    r_hi->node_ids[0] = r_hi_top_node;

    int lo_out_node = circuit_find_or_create_node(circuit, comp_lo_out_x, comp_lo_out_y, 5.0f);
    int r_lo_bot_node = circuit_find_or_create_node(circuit, r_lo_bot_x, r_lo_bot_y, 5.0f);
    circuit_add_wire(circuit, lo_out_node, r_lo_bot_node);
    comp_lo->node_ids[2] = lo_out_node;
    r_lo->node_ids[1] = r_lo_bot_node;
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

    return 18;  // incl. c_dec, gnd_dec, r_hi, r_lo
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
    vin->props.ac_voltage.offset = 6.0;    // swing around the 6 V reference on the - input

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

    // Zener diode (5.1V reference). Rotation 270 puts the cathode (terminal 1) on top,
    // toward Rs/+12V, and the anode (terminal 0) at the bottom toward ground, so the
    // diode operates in reverse breakdown as a reference (not forward-biased at 0.6 V).
    Component *zener = add_comp(circuit, COMP_ZENER, x + 80, y, 270);
    zener->props.zener.vz = 5.1;
    zener->props.zener.rz = 10.0;

    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 80, y + 60, 0);

    // Load resistor
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 160, y, 90);
    rload->props.resistor.resistance = 10000.0;

    Component *gnd3 = add_comp(circuit, COMP_GROUND, x + 160, y + 60, 0);

    // Ground connections
    connect_terminals(circuit, vcc, 1, gnd1, 0);
    connect_terminals(circuit, zener, 0, gnd2, 0);   // anode to ground
    connect_terminals(circuit, rload, 1, gnd3, 0);

    // Vcc to Rs top
    float vcc_pos_x, vcc_pos_y;
    component_get_terminal_pos(vcc, 0, &vcc_pos_x, &vcc_pos_y);
    float rs_top_x, rs_top_y;
    component_get_terminal_pos(rs, 0, &rs_top_x, &rs_top_y);

    wire_L_shape(circuit, vcc_pos_x, vcc_pos_y, rs_top_x, rs_top_y, false);

    // Rs to Zener junction and load
    float rs_bot_x, rs_bot_y;
    component_get_terminal_pos(rs, 1, &rs_bot_x, &rs_bot_y);
    float zener_top_x, zener_top_y;
    component_get_terminal_pos(zener, 1, &zener_top_x, &zener_top_y);   // cathode
    float rload_top_x, rload_top_y;
    component_get_terminal_pos(rload, 0, &rload_top_x, &rload_top_y);

    int ref_junc = circuit_find_or_create_node(circuit, rs_bot_x, rs_bot_y, 5.0f);
    rs->node_ids[1] = ref_junc;

    int zener_top_node = circuit_find_or_create_node(circuit, zener_top_x, zener_top_y, 5.0f);
    circuit_add_wire(circuit, ref_junc, zener_top_node);
    zener->node_ids[1] = zener_top_node;

    int rload_top_node = circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f);
    int corner1 = circuit_find_or_create_node(circuit, rload_top_x, rs_bot_y, 5.0f);
    circuit_add_wire(circuit, ref_junc, corner1);
    circuit_add_wire(circuit, corner1, rload_top_node);
    rload->node_ids[0] = rload_top_node;

    return 6;
}

// Precision Full-Wave Rectifier (Absolute Value Circuit)
static int place_precision_rect(Circuit *circuit, float x, float y) {
    // Two-op-amp precision full-wave rectifier (Sedra/Smith "absolute value" circuit).
    //
    //   Vin --R1--+--|-\           D2      P
    //             |  |  >--out1--|>|--+------R3(5k)--+--|-\
    //             +--|<|-- D1 ---+    |               |  |  >--out2 = -|Vin|
    //             |            R2(10k)+        Vin --R4(10k)--+--|+/
    //             +--------------------+                      Rf(10k) out2->inv2
    //
    // Stage 1 (op1): Vin > 0 -> out1 swings negative, D1 conducts and holds the inverting
    // input at 0, D2 is off so P = 0.  Vin < 0 -> out1 swings positive, D2 conducts and R2
    // closes the loop, so P = -Vin = |Vin|.  The diode drops sit inside the loop and vanish.
    // Stage 2 (op2) sums Vin (R4) and P (R3 = R/2): out2 = -(Vin + 2P) = -|Vin|.
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 60, 0);            // +(0,20) -(0,100)
    if (!vin) return 0;
    vin->props.ac_voltage.amplitude = 1.0;
    vin->props.ac_voltage.frequency = 100.0;
    Component *gnd_in = add_comp(circuit, COMP_GROUND, x, y + 120, 0);
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 80, y + 20, 0);          // (40,20)-(120,20)
    r1->props.resistor.resistance = 10000.0;
    Component *op1 = add_comp(circuit, COMP_OPAMP, x + 200, y + 40, 0);           // -(160,20) +(160,60) out(240,40)
    op1->props.opamp.ideal = true;
    Component *gnd_op1 = add_comp(circuit, COMP_GROUND, x + 140, y + 100, 0);     // terminal (140,80)
    Component *d1 = add_comp(circuit, COMP_DIODE, x + 200, y - 40, 0);            // A(160,-40) K(240,-40)
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 200, y - 100, 0);        // (160,-100)-(240,-100)
    r2->props.resistor.resistance = 10000.0;
    Component *d2 = add_comp(circuit, COMP_DIODE, x + 320, y + 40, 0);            // A(280,40) K(360,40) = P
    Component *r3 = add_comp(circuit, COMP_RESISTOR, x + 420, y + 60, 0);         // (380,60)-(460,60)
    r3->props.resistor.resistance = 5000.0;
    Component *r4 = add_comp(circuit, COMP_RESISTOR, x + 460, y - 40, 90);        // (460,-80)-(460,0)
    r4->props.resistor.resistance = 10000.0;
    Component *op2 = add_comp(circuit, COMP_OPAMP, x + 520, y + 80, 0);           // -(480,60) +(480,100) out(560,80)
    op2->props.opamp.ideal = true;
    Component *gnd_op2 = add_comp(circuit, COMP_GROUND, x + 460, y + 140, 0);     // terminal (460,120)
    Component *rf = add_comp(circuit, COMP_RESISTOR, x + 520, y - 20, 0);         // (480,-20)-(560,-20)
    rf->props.resistor.resistance = 10000.0;
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 620, y + 120, 90);    // (620,80)-(620,160)
    rload->props.resistor.resistance = 10000.0;
    Component *gnd_load = add_comp(circuit, COMP_GROUND, x + 620, y + 180, 0);
    Component *label = add_comp(circuit, COMP_TEXT, x + 180, y - 200, 0);
    strncpy(label->props.text.text, "Precision Full-Wave Rectifier", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

#define PN(cx, cy) circuit_find_or_create_node(circuit, (cx), (cy), 5.0f)
#define PW(a, b) circuit_add_wire(circuit, (a), (b))
    connect_terminals(circuit, vin, 1, gnd_in, 0);
    connect_terminals(circuit, rload, 1, gnd_load, 0);

    // Vin -> R1 (with a tap at x+20 for the R4 path)
    int n_vin = PN(x, y + 20), tap = PN(x + 20, y + 20), r1l = PN(x + 40, y + 20);
    PW(n_vin, tap); PW(tap, r1l);
    // R1 -> inverting input, with the feedback column at x+140
    int r1r = PN(x + 120, y + 20), fbcol = PN(x + 140, y + 20), inv1 = PN(x + 160, y + 20);
    PW(r1r, fbcol); PW(fbcol, inv1);
    // op1 + input to ground
    int ni1 = PN(x + 160, y + 60), g1c = PN(x + 140, y + 60), g1 = PN(x + 140, y + 80);
    PW(ni1, g1c); PW(g1c, g1);
    // D1: inverting node up the column to the anode; cathode round the right to out1
    int fb_d1 = PN(x + 140, y - 40), d1a = PN(x + 160, y - 40), d1k = PN(x + 240, y - 40);
    PW(fbcol, fb_d1); PW(fb_d1, d1a);
    int out1 = PN(x + 240, y + 40), o1r = PN(x + 260, y + 40), o1u = PN(x + 260, y - 40);
    PW(out1, o1r); PW(o1r, o1u); PW(o1u, d1k);
    // R2: from the feedback column top to P
    int fb_r2 = PN(x + 140, y - 100), r2l = PN(x + 160, y - 100), r2r = PN(x + 240, y - 100);
    PW(fb_d1, fb_r2); PW(fb_r2, r2l);
    int pcol_top = PN(x + 360, y - 100), P = PN(x + 360, y + 40);
    PW(r2r, pcol_top); PW(pcol_top, P);
    // D2: out1 -> P
    int d2a = PN(x + 280, y + 40);
    PW(o1r, d2a);
    // P -> R3 -> inv2 junction
    int p_dn = PN(x + 360, y + 60), r3l = PN(x + 380, y + 60), r3r = PN(x + 460, y + 60), inv2 = PN(x + 480, y + 60);
    PW(P, p_dn); PW(p_dn, r3l); PW(r3r, inv2);
    // Vin tap -> over the top -> R4 -> inv2 junction
    int tap_up = PN(x + 20, y - 160), r4top_c = PN(x + 460, y - 160), r4t = PN(x + 460, y - 80), r4b = PN(x + 460, y);
    PW(tap, tap_up); PW(tap_up, r4top_c); PW(r4top_c, r4t); PW(r4b, r3r);
    // op2 + input to ground
    int ni2 = PN(x + 480, y + 100), g2c = PN(x + 460, y + 100), g2 = PN(x + 460, y + 120);
    PW(ni2, g2c); PW(g2c, g2);
    // Rf: out2 -> right column -> above -> inv2 (via the Rf left drop at x+480)
    int out2 = PN(x + 560, y + 80), o2r = PN(x + 580, y + 80), o2u = PN(x + 580, y - 20), rfr = PN(x + 560, y - 20), rfl = PN(x + 480, y - 20);
    PW(out2, o2r); PW(o2r, o2u); PW(o2u, rfr); PW(rfl, inv2);
    // out2 -> load
    int ld = PN(x + 620, y + 80);
    PW(o2r, ld);
#undef PN
#undef PW
    return 16;
}
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
    wire_ortho(circuit, vin_pos_node, input_bus);
    vin->node_ids[0] = vin_pos_node;

    // Cin top to input bus
    int cin_top_node = circuit_find_or_create_node(circuit, cin_top_x, cin_top_y, 5.0f);
    int bus_to_cin = circuit_find_or_create_node(circuit, cin_top_x, y - 40, 5.0f);
    wire_ortho(circuit, input_bus, bus_to_cin);
    wire_ortho(circuit, bus_to_cin, cin_top_node);
    cin->node_ids[0] = cin_top_node;

    // Regulator IN to input bus
    int reg_in_node = circuit_find_or_create_node(circuit, reg_in_x, reg_in_y, 5.0f);
    int bus_to_reg = circuit_find_or_create_node(circuit, reg_in_x, y - 40, 5.0f);
    wire_ortho(circuit, bus_to_cin, bus_to_reg);
    wire_ortho(circuit, bus_to_reg, reg_in_node);
    reg->node_ids[0] = reg_in_node;

    // Output power rail from regulator OUT
    int reg_out_node = circuit_find_or_create_node(circuit, reg_out_x, reg_out_y, 5.0f);
    int output_bus = circuit_find_or_create_node(circuit, reg_out_x, y - 40, 5.0f);
    wire_ortho(circuit, reg_out_node, output_bus);
    reg->node_ids[1] = reg_out_node;

    // R1 top to output bus
    int r1_top_node = circuit_find_or_create_node(circuit, r1_top_x, r1_top_y, 5.0f);
    int bus_to_r1 = circuit_find_or_create_node(circuit, r1_top_x, y - 40, 5.0f);
    wire_ortho(circuit, output_bus, bus_to_r1);
    wire_ortho(circuit, bus_to_r1, r1_top_node);
    r1->node_ids[0] = r1_top_node;

    // Cout top to output bus
    int cout_top_node = circuit_find_or_create_node(circuit, cout_top_x, cout_top_y, 5.0f);
    int bus_to_cout = circuit_find_or_create_node(circuit, cout_top_x, y - 40, 5.0f);
    wire_ortho(circuit, bus_to_r1, bus_to_cout);
    wire_ortho(circuit, bus_to_cout, cout_top_node);
    cout->node_ids[0] = cout_top_node;

    // Rload top to output bus
    int rload_top_node = circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f);
    int bus_to_load = circuit_find_or_create_node(circuit, rload_top_x, y - 40, 5.0f);
    wire_ortho(circuit, bus_to_cout, bus_to_load);
    wire_ortho(circuit, bus_to_load, rload_top_node);
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
    wire_ortho(circuit, r1_bot_node, adj_junction);
    wire_ortho(circuit, adj_junction, reg_adj_node);

    // Connect R2 top to ADJ junction
    wire_ortho(circuit, adj_junction, r2_top_node);

    // Ground connections
    int gnd_in_node = circuit_find_or_create_node(circuit, gnd_in_x, gnd_in_y, 5.0f);
    int vin_neg_node = circuit_find_or_create_node(circuit, vin_neg_x, vin_neg_y, 5.0f);
    wire_ortho(circuit, vin_neg_node, gnd_in_node);
    vin->node_ids[1] = vin_neg_node;
    gnd_in->node_ids[0] = gnd_in_node;

    // Ground rail
    float gnd_y = y + 140;
    int gnd_rail = circuit_find_or_create_node(circuit, cin_bot_x, gnd_y, 5.0f);

    // Cin bottom to ground
    int cin_bot_node = circuit_find_or_create_node(circuit, cin_bot_x, cin_bot_y, 5.0f);
    wire_ortho(circuit, cin_bot_node, gnd_rail);
    cin->node_ids[1] = cin_bot_node;

    // Connect vin- to ground rail
    int corner_vin_gnd = circuit_find_or_create_node(circuit, vin_neg_x, gnd_y, 5.0f);
    wire_ortho(circuit, gnd_in_node, corner_vin_gnd);
    wire_ortho(circuit, corner_vin_gnd, gnd_rail);

    // R2 bottom to ground rail
    int r2_bot_node = circuit_find_or_create_node(circuit, r2_bot_x, r2_bot_y, 5.0f);
    int gnd_rail_r2 = circuit_find_or_create_node(circuit, r2_bot_x, gnd_y, 5.0f);
    wire_ortho(circuit, gnd_rail, gnd_rail_r2);
    wire_ortho(circuit, r2_bot_node, gnd_rail_r2);
    r2->node_ids[1] = r2_bot_node;

    // Cout bottom to ground rail
    int cout_bot_node = circuit_find_or_create_node(circuit, cout_bot_x, cout_bot_y, 5.0f);
    int gnd_rail_cout = circuit_find_or_create_node(circuit, cout_bot_x, gnd_y, 5.0f);
    wire_ortho(circuit, gnd_rail_r2, gnd_rail_cout);
    wire_ortho(circuit, cout_bot_node, gnd_rail_cout);
    cout->node_ids[1] = cout_bot_node;

    // Rload bottom and ground symbol
    int rload_bot_node = circuit_find_or_create_node(circuit, rload_bot_x, rload_bot_y, 5.0f);
    int gnd_load_node = circuit_find_or_create_node(circuit, gnd_load_x, gnd_load_y, 5.0f);
    int gnd_rail_load = circuit_find_or_create_node(circuit, rload_bot_x, gnd_y, 5.0f);
    wire_ortho(circuit, gnd_rail_cout, gnd_rail_load);
    wire_ortho(circuit, rload_bot_node, gnd_rail_load);
    rload->node_ids[1] = rload_bot_node;

    // Ground symbol connection
    wire_ortho(circuit, gnd_rail_cout, gnd_load_node);
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

// =============================================================================
// RLC RESONANT CIRCUITS
// =============================================================================

// Series RLC: Vin --[R]--[L]--[C]-- GND
// f0 = 1/(2*pi*sqrt(LC)) = 159Hz with L=10mH, C=100uF
static int place_series_rlc(Circuit *circuit, float x, float y) {
    // Layout: AC source on left, R-L-C in series horizontally, ground below C

    // AC voltage source (+ terminal at y when placed at y+40)
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 40, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 5.0;
    vsrc->props.ac_voltage.frequency = 159.0;  // Near resonance
    // Frequency sweep 50 Hz -> 500 Hz (log, 3 s each way): the resonance at 159 Hz shows as a peak
    vsrc->props.ac_voltage.frequency_sweep.enabled = true;
    vsrc->props.ac_voltage.frequency_sweep.mode = SWEEP_LOG;
    vsrc->props.ac_voltage.frequency_sweep.start_value = 30;
    vsrc->props.ac_voltage.frequency_sweep.end_value = 800;
    vsrc->props.ac_voltage.frequency_sweep.sweep_time = 3;
    vsrc->props.ac_voltage.frequency_sweep.repeat = true;
    vsrc->props.ac_voltage.frequency_sweep.bidirectional = true;

    // Ground for source (at y+100)
    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, y + 100, 0);

    // Resistor horizontal at y (x+60 to x+140)
    Component *res = add_comp(circuit, COMP_RESISTOR, x + 100, y, 0);
    res->props.resistor.resistance = 3.3;   // Q = sqrt(L/C)/R = 3: Vc peaks at 3*Vin = 15 V on resonance

    // Inductor horizontal at y (x+180 to x+260)
    Component *ind = add_comp(circuit, COMP_INDUCTOR, x + 220, y, 0);
    ind->props.inductor.inductance = 10e-3;  // 10mH

    // Capacitor vertical (rotation 90) at x+300
    Component *cap = add_comp(circuit, COMP_CAPACITOR, x + 300, y + 40, 90);
    cap->props.capacitor.capacitance = 100e-6;  // 100uF

    // Ground for capacitor
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 300, y + 100, 0);

    // Connect source negative to ground
    connect_terminals(circuit, vsrc, 1, gnd1, 0);

    // Connect source positive to resistor
    connect_terminals(circuit, vsrc, 0, res, 0);

    // Connect resistor to inductor
    connect_terminals(circuit, res, 1, ind, 0);

    // Connect inductor to capacitor top
    connect_terminals(circuit, ind, 1, cap, 0);

    // Connect capacitor to ground
    connect_terminals(circuit, cap, 1, gnd2, 0);

    return 6;
}

// Parallel RLC (Tank): Vin --+--[R]--+-- GND
//                           |       |
//                          [L]     [C]
//                           |       |
//                           +-------+
// f0 = 1/(2*pi*sqrt(LC)) = 159Hz
static int place_parallel_rlc(Circuit *circuit, float x, float y) {
    // Source drives the tank through R (1k). Tank impedance peaks at f0 = 1/(2*pi*sqrt(LC))
    // = 159 Hz, so the tank voltage is near the full source voltage there and small
    // elsewhere (Q = R*sqrt(C/L) = 100 with R = 1k). The source sweeps 30-800 Hz.
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 60, 0);             // +(0,20) -(0,100)
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 5.0;
    vsrc->props.ac_voltage.frequency = 159.0;
    vsrc->props.ac_voltage.frequency_sweep.enabled = true;
    vsrc->props.ac_voltage.frequency_sweep.mode = SWEEP_LOG;
    vsrc->props.ac_voltage.frequency_sweep.start_value = 30;
    vsrc->props.ac_voltage.frequency_sweep.end_value = 800;
    vsrc->props.ac_voltage.frequency_sweep.sweep_time = 3;
    vsrc->props.ac_voltage.frequency_sweep.repeat = true;
    vsrc->props.ac_voltage.frequency_sweep.bidirectional = true;
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    Component *res = add_comp(circuit, COMP_RESISTOR, x + 100, y + 20, 0);           // (60,20)-(140,20)
    res->props.resistor.resistance = 1000.0;
    Component *ind = add_comp(circuit, COMP_INDUCTOR, x + 180, y + 60, 90);          // (180,20)-(180,100)
    ind->props.inductor.inductance = 10e-3;
    Component *cap = add_comp(circuit, COMP_CAPACITOR, x + 260, y + 60, 90);         // (260,20)-(260,100)
    cap->props.capacitor.capacitance = 100e-6;
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 260, y + 140, 0);           // terminal (260,120)
    Component *label = add_comp(circuit, COMP_TEXT, x + 60, y - 40, 0);
    strncpy(label->props.text.text, "Parallel RLC (tank)", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    connect_terminals(circuit, vsrc, 1, gnd, 0);
    connect_terminals(circuit, vsrc, 0, res, 0);          // (0,20) -> (60,20)
    int tank = circuit_find_or_create_node(circuit, x + 180, y + 20, 5.0f);
    int r_r = circuit_find_or_create_node(circuit, x + 140, y + 20, 5.0f);
    int c_t = circuit_find_or_create_node(circuit, x + 260, y + 20, 5.0f);
    circuit_add_wire(circuit, r_r, tank);
    circuit_add_wire(circuit, tank, c_t);
    int l_b = circuit_find_or_create_node(circuit, x + 180, y + 100, 5.0f);
    int c_b = circuit_find_or_create_node(circuit, x + 260, y + 100, 5.0f);
    int g2 = circuit_find_or_create_node(circuit, x + 260, y + 120, 5.0f);
    circuit_add_wire(circuit, l_b, c_b);
    circuit_add_wire(circuit, c_b, g2);
    res->node_ids[1] = r_r; ind->node_ids[0] = tank; ind->node_ids[1] = l_b;
    cap->node_ids[0] = c_t; cap->node_ids[1] = c_b; gnd2->node_ids[0] = g2;
    return 7;
}
static int place_wheatstone_bridge(Circuit *circuit, float x, float y) {
    // Layout: DC source on left, diamond of resistors

    // DC voltage source (vertical). Its terminals sit at y+20 / y+100, clear of both
    // resistor rows (y and y+120) so no routing corner lands on a source terminal.
    Component *vsrc = add_comp(circuit, COMP_DC_VOLTAGE, x, y + 60, 0);
    if (!vsrc) return 0;
    vsrc->props.dc_voltage.voltage = 10.0;

    // Ground for source
    Component *gnd = add_comp(circuit, COMP_GROUND, x, y + 160, 0);

    // Bridge resistors - create diamond layout
    // R1 and R2 on top row (horizontal)
    // R3 and R4 on bottom row (horizontal)

    // Top-left resistor R1 (horizontal)
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 100, y, 0);
    r1->props.resistor.resistance = 1000.0;

    // Top-right resistor R2 (horizontal)
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 220, y, 0);
    r2->props.resistor.resistance = 1000.0;

    // Bottom-left resistor R3 (horizontal)
    Component *r3 = add_comp(circuit, COMP_RESISTOR, x + 100, y + 120, 0);
    r3->props.resistor.resistance = 1000.0;

    // Bottom-right resistor R4 (variable - use 1.1k to unbalance)
    Component *r4 = add_comp(circuit, COMP_RESISTOR, x + 220, y + 120, 0);
    r4->props.resistor.resistance = 1100.0;  // Slightly unbalanced

    // Get terminal positions
    float vsrc_pos_x, vsrc_pos_y, vsrc_neg_x, vsrc_neg_y;
    component_get_terminal_pos(vsrc, 0, &vsrc_pos_x, &vsrc_pos_y);
    component_get_terminal_pos(vsrc, 1, &vsrc_neg_x, &vsrc_neg_y);

    // Connect source to ground
    connect_terminals(circuit, vsrc, 1, gnd, 0);

    // Left side (source + connects to R1 left and R3 left)
    float r1_left_x, r1_left_y;
    component_get_terminal_pos(r1, 0, &r1_left_x, &r1_left_y);
    float r3_left_x, r3_left_y;
    component_get_terminal_pos(r3, 0, &r3_left_x, &r3_left_y);

    int node_vsrc_pos = circuit_find_or_create_node(circuit, vsrc_pos_x, vsrc_pos_y, 5.0f);
    vsrc->node_ids[0] = node_vsrc_pos;

    // Wire from vsrc+ up to top row height, then right to R1 left
    int corner_top_left = circuit_find_or_create_node(circuit, vsrc_pos_x, r1_left_y, 5.0f);
    circuit_add_wire(circuit, node_vsrc_pos, corner_top_left);

    int node_r1_left = circuit_find_or_create_node(circuit, r1_left_x, r1_left_y, 5.0f);
    r1->node_ids[0] = node_r1_left;
    circuit_add_wire(circuit, corner_top_left, node_r1_left);

    // Left rail: R1 left straight down to R3 left (both fed from vsrc+)
    int node_r3_left = circuit_find_or_create_node(circuit, r3_left_x, r3_left_y, 5.0f);
    r3->node_ids[0] = node_r3_left;
    circuit_add_wire(circuit, node_r1_left, node_r3_left);

    // Right side (R2 right and R4 right connect to ground rail)
    float r2_right_x, r2_right_y;
    component_get_terminal_pos(r2, 1, &r2_right_x, &r2_right_y);
    float r4_right_x, r4_right_y;
    component_get_terminal_pos(r4, 1, &r4_right_x, &r4_right_y);

    // Create right rail at x+300
    float rail_right_x = x + 300;

    int node_r2_right = circuit_find_or_create_node(circuit, r2_right_x, r2_right_y, 5.0f);
    r2->node_ids[1] = node_r2_right;
    int corner_top_right = circuit_find_or_create_node(circuit, rail_right_x, r2_right_y, 5.0f);
    circuit_add_wire(circuit, node_r2_right, corner_top_right);

    int node_r4_right = circuit_find_or_create_node(circuit, r4_right_x, r4_right_y, 5.0f);
    r4->node_ids[1] = node_r4_right;
    int corner_bot_right = circuit_find_or_create_node(circuit, rail_right_x, r4_right_y, 5.0f);
    circuit_add_wire(circuit, node_r4_right, corner_bot_right);

    // Connect right rail vertically
    circuit_add_wire(circuit, corner_top_right, corner_bot_right);

    // Connect right rail to ground
    float gnd_term_x, gnd_term_y;
    component_get_terminal_pos(gnd, 0, &gnd_term_x, &gnd_term_y);
    int node_gnd = circuit_find_or_create_node(circuit, gnd_term_x, gnd_term_y, 5.0f);
    gnd->node_ids[0] = node_gnd;

    int corner_gnd = circuit_find_or_create_node(circuit, rail_right_x, gnd_term_y, 5.0f);
    circuit_add_wire(circuit, corner_bot_right, corner_gnd);
    circuit_add_wire(circuit, corner_gnd, node_gnd);

    // Center connections: R1-R2 junction and R3-R4 junction (the bridge output)
    float r1_right_x, r1_right_y;
    component_get_terminal_pos(r1, 1, &r1_right_x, &r1_right_y);
    float r2_left_x, r2_left_y;
    component_get_terminal_pos(r2, 0, &r2_left_x, &r2_left_y);

    int node_r1_right = circuit_find_or_create_node(circuit, r1_right_x, r1_right_y, 5.0f);
    r1->node_ids[1] = node_r1_right;
    int node_r2_left = circuit_find_or_create_node(circuit, r2_left_x, r2_left_y, 5.0f);
    r2->node_ids[0] = node_r2_left;
    circuit_add_wire(circuit, node_r1_right, node_r2_left);

    float r3_right_x, r3_right_y;
    component_get_terminal_pos(r3, 1, &r3_right_x, &r3_right_y);
    float r4_left_x, r4_left_y;
    component_get_terminal_pos(r4, 0, &r4_left_x, &r4_left_y);

    int node_r3_right = circuit_find_or_create_node(circuit, r3_right_x, r3_right_y, 5.0f);
    r3->node_ids[1] = node_r3_right;
    int node_r4_left = circuit_find_or_create_node(circuit, r4_left_x, r4_left_y, 5.0f);
    r4->node_ids[0] = node_r4_left;
    circuit_add_wire(circuit, node_r3_right, node_r4_left);

    return 7;  // vsrc, gnd, r1, r2, r3, r4
}

// Peak Detector: Vin --[Op-Amp]--[D]--+--[C]-- GND
//                                     |
//                                    Vout
static int place_peak_detector(Circuit *circuit, float x, float y) {
    // Simple peak detector: op-amp buffer -> diode -> capacitor

    // AC voltage source (input signal)
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 60, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 5.0;
    vsrc->props.ac_voltage.frequency = 1000.0;   // 1 kHz carrier
    // Amplitude sweeps 1 V -> 5 V -> 1 V every second (bidirectional, repeating)
    vsrc->props.ac_voltage.amplitude_sweep.enabled = true;
    vsrc->props.ac_voltage.amplitude_sweep.mode = SWEEP_LINEAR;
    vsrc->props.ac_voltage.amplitude_sweep.start_value = 1.0;
    vsrc->props.ac_voltage.amplitude_sweep.end_value = 5.0;
    vsrc->props.ac_voltage.amplitude_sweep.sweep_time = 0.5;
    vsrc->props.ac_voltage.amplitude_sweep.repeat = true;
    vsrc->props.ac_voltage.amplitude_sweep.bidirectional = true;

    // Ground for source
    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, y + 140, 0);

    // Op-amp (voltage follower configuration)
    Component *opamp = add_comp(circuit, COMP_OPAMP, x + 140, y + 20, 0);

    // Diode (horizontal)
    Component *diode = add_comp(circuit, COMP_DIODE, x + 240, y + 20, 0);

    // Hold capacitor (vertical)
    Component *cap = add_comp(circuit, COMP_CAPACITOR, x + 320, y + 60, 90);
    cap->props.capacitor.capacitance = 1e-6;   // 1 uF (R*C with the bleed = 47 ms)

    // Ground for capacitor
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 320, y + 140, 0);
    // Bleed resistor so the held peak decays and the output follows a falling envelope
    Component *rbleed = add_comp(circuit, COMP_RESISTOR, x + 380, y + 60, 90);
    rbleed->props.resistor.resistance = 47000.0;
    Component *gnd3 = add_comp(circuit, COMP_GROUND, x + 380, y + 140, 0);

    // Get terminal positions
    float vsrc_pos_x, vsrc_pos_y, vsrc_neg_x, vsrc_neg_y;
    component_get_terminal_pos(vsrc, 0, &vsrc_pos_x, &vsrc_pos_y);
    component_get_terminal_pos(vsrc, 1, &vsrc_neg_x, &vsrc_neg_y);

    float opamp_inv_x, opamp_inv_y;      // Inverting input (-)
    float opamp_noninv_x, opamp_noninv_y; // Non-inverting input (+)
    float opamp_out_x, opamp_out_y;       // Output
    component_get_terminal_pos(opamp, 0, &opamp_inv_x, &opamp_inv_y);
    component_get_terminal_pos(opamp, 1, &opamp_noninv_x, &opamp_noninv_y);
    component_get_terminal_pos(opamp, 2, &opamp_out_x, &opamp_out_y);

    // Source negative to ground
    connect_terminals(circuit, vsrc, 1, gnd1, 0);

    // Source positive to op-amp non-inverting input
    int node_vsrc_pos = circuit_find_or_create_node(circuit, vsrc_pos_x, vsrc_pos_y, 5.0f);
    vsrc->node_ids[0] = node_vsrc_pos;

    int node_opamp_noninv = circuit_find_or_create_node(circuit, opamp_noninv_x, opamp_noninv_y, 5.0f);
    opamp->node_ids[1] = node_opamp_noninv;

    // L-shape: right then down
    int corner1 = circuit_find_or_create_node(circuit, opamp_noninv_x, vsrc_pos_y, 5.0f);
    circuit_add_wire(circuit, node_vsrc_pos, corner1);
    circuit_add_wire(circuit, corner1, node_opamp_noninv);

    // Op-amp output to diode anode
    float diode_a_x, diode_a_y, diode_c_x, diode_c_y;
    component_get_terminal_pos(diode, 0, &diode_a_x, &diode_a_y);  // Anode
    component_get_terminal_pos(diode, 1, &diode_c_x, &diode_c_y);  // Cathode

    int node_opamp_out = circuit_find_or_create_node(circuit, opamp_out_x, opamp_out_y, 5.0f);
    opamp->node_ids[2] = node_opamp_out;
    int node_diode_a = circuit_find_or_create_node(circuit, diode_a_x, diode_a_y, 5.0f);
    diode->node_ids[0] = node_diode_a;
    circuit_add_wire(circuit, node_opamp_out, node_diode_a);

    // Feedback: diode cathode back to op-amp inverting input
    int node_opamp_inv = circuit_find_or_create_node(circuit, opamp_inv_x, opamp_inv_y, 5.0f);
    opamp->node_ids[0] = node_opamp_inv;
    int node_diode_c = circuit_find_or_create_node(circuit, diode_c_x, diode_c_y, 5.0f);
    diode->node_ids[1] = node_diode_c;

    // L-shape feedback: from diode cathode down, then left, then up to inv input
    float feedback_y = y + 80;  // Below the op-amp
    int corner_fb1 = circuit_find_or_create_node(circuit, diode_c_x, feedback_y, 5.0f);
    circuit_add_wire(circuit, node_diode_c, corner_fb1);
    int corner_fb2 = circuit_find_or_create_node(circuit, opamp_inv_x, feedback_y, 5.0f);
    circuit_add_wire(circuit, corner_fb1, corner_fb2);
    circuit_add_wire(circuit, corner_fb2, node_opamp_inv);

    // Diode cathode to capacitor top
    float cap_top_x, cap_top_y, cap_bot_x, cap_bot_y;
    component_get_terminal_pos(cap, 0, &cap_top_x, &cap_top_y);
    component_get_terminal_pos(cap, 1, &cap_bot_x, &cap_bot_y);

    int node_cap_top = circuit_find_or_create_node(circuit, cap_top_x, cap_top_y, 5.0f);
    cap->node_ids[0] = node_cap_top;
    circuit_add_wire(circuit, node_diode_c, node_cap_top);

    // Capacitor to ground
    connect_terminals(circuit, cap, 1, gnd2, 0);
    connect_terminals(circuit, cap, 0, rbleed, 0);
    connect_terminals(circuit, rbleed, 1, gnd3, 0);

    return 8;  // vsrc, gnd1, opamp, diode, cap, gnd2, rbleed, gnd3
}

// =============================================================================
// SIGNAL PROCESSING CIRCUITS
// =============================================================================

// Positive Clamper: Vin --[C]--+--[D]-- GND
//                              |
//                             [R] (load)
//                              |
//                             GND
static int place_clamper(Circuit *circuit, float x, float y) {
    // Clamper (DC Restorer) - shifts AC waveform so negative peaks clamp at -0.7V
    // Grid-based layout with explicit node placement for clean orthogonal wires
    //
    // Layout (all on grid, no diagonal wires):
    //
    //     x        x+80      x+140     x+200
    //     |          |          |         |
    //  y: [AC+]-----[C]--------[bus]-----[R]
    //      |                     |         |
    // y+40:[GND]                [D]        |
    //                            |         |
    // y+80:                    [GND]     [GND]
    //
    // The bus node connects: cap output, diode cathode, resistor top

    // Create explicit grid nodes for clean wiring
    float grid_y = y;           // Signal line
    float grid_y2 = y + 40;     // Middle row
    float grid_y3 = y + 80;     // Ground row

    // === COMPONENTS ===

    // AC voltage source (vertical, rotation 0: + at top, - at bottom)
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x, grid_y + 20, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 5.0;
    vsrc->props.ac_voltage.frequency = 1000.0;
    // Amplitude sweeps 1 V -> 5 V -> 1 V every second (bidirectional, repeating)
    vsrc->props.ac_voltage.amplitude_sweep.enabled = true;
    vsrc->props.ac_voltage.amplitude_sweep.mode = SWEEP_LINEAR;
    vsrc->props.ac_voltage.amplitude_sweep.start_value = 1.0;
    vsrc->props.ac_voltage.amplitude_sweep.end_value = 5.0;
    vsrc->props.ac_voltage.amplitude_sweep.sweep_time = 0.5;
    vsrc->props.ac_voltage.amplitude_sweep.repeat = true;
    vsrc->props.ac_voltage.amplitude_sweep.bidirectional = true;

    // Ground for AC source
    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, grid_y2 + 20, 0);

    // Coupling capacitor (horizontal, rotation 0)
    Component *cap = add_comp(circuit, COMP_CAPACITOR, x + 60, grid_y, 0);
    cap->props.capacitor.capacitance = 1e-6;  // 1uF - small enough for fast charging (τ=C/Gd ~33µs)

    // Clamping diode (vertical) - NEGATIVE clamper topology
    // Rotation 270: cathode at top (connects to signal bus), anode at bottom (to ground)
    // When signal goes negative, diode conducts and clamps output to ~-0.3V
    Component *diode = add_comp(circuit, COMP_DIODE, x + 140, grid_y2, 270);
    // Increase saturation current for sharper clamping behavior
    // Default Is=1e-12 is too small - results in soft exponential clamping
    diode->props.diode.is = 1e-9;  // 1nA - makes diode conduct more strongly

    // Load resistor (vertical, rotation 90: term0 at top)
    // Higher value reduces loading on capacitor for better clamping
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 200, grid_y2, 90);
    rload->props.resistor.resistance = 100000.0;  // 100k for reduced loading

    // Grounds for diode and resistor
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 140, grid_y3 + 20, 0);
    Component *gnd3 = add_comp(circuit, COMP_GROUND, x + 200, grid_y3 + 20, 0);

    // === WIRING (all straight horizontal or vertical lines) ===

    // Get terminal positions and create nodes
    float tx, ty;

    // Node at vsrc+ (should be at grid_y)
    component_get_terminal_pos(vsrc, 0, &tx, &ty);
    int n_vsrc_plus = circuit_find_or_create_node(circuit, tx, ty, 5.0f);
    vsrc->node_ids[0] = n_vsrc_plus;

    // Node at vsrc-
    component_get_terminal_pos(vsrc, 1, &tx, &ty);
    int n_vsrc_minus = circuit_find_or_create_node(circuit, tx, ty, 5.0f);
    vsrc->node_ids[1] = n_vsrc_minus;

    // Node at gnd1
    component_get_terminal_pos(gnd1, 0, &tx, &ty);
    int n_gnd1 = circuit_find_or_create_node(circuit, tx, ty, 5.0f);
    gnd1->node_ids[0] = n_gnd1;

    // Node at cap left
    component_get_terminal_pos(cap, 0, &tx, &ty);
    int n_cap_left = circuit_find_or_create_node(circuit, tx, ty, 5.0f);
    cap->node_ids[0] = n_cap_left;

    // Node at cap right
    component_get_terminal_pos(cap, 1, &tx, &ty);
    int n_cap_right = circuit_find_or_create_node(circuit, tx, ty, 5.0f);
    cap->node_ids[1] = n_cap_right;

    // Node at diode anode (bottom for rotation 270 - negative clamper)
    component_get_terminal_pos(diode, 0, &tx, &ty);
    int n_diode_anode = circuit_find_or_create_node(circuit, tx, ty, 5.0f);
    diode->node_ids[0] = n_diode_anode;

    // Node at diode cathode (top for rotation 270 - negative clamper)
    component_get_terminal_pos(diode, 1, &tx, &ty);
    int n_diode_cathode = circuit_find_or_create_node(circuit, tx, ty, 5.0f);
    diode->node_ids[1] = n_diode_cathode;

    // Node at resistor top
    component_get_terminal_pos(rload, 0, &tx, &ty);
    int n_rload_top = circuit_find_or_create_node(circuit, tx, ty, 5.0f);
    rload->node_ids[0] = n_rload_top;

    // Node at resistor bottom
    component_get_terminal_pos(rload, 1, &tx, &ty);
    int n_rload_bottom = circuit_find_or_create_node(circuit, tx, ty, 5.0f);
    rload->node_ids[1] = n_rload_bottom;

    // Nodes at grounds
    component_get_terminal_pos(gnd2, 0, &tx, &ty);
    int n_gnd2 = circuit_find_or_create_node(circuit, tx, ty, 5.0f);
    gnd2->node_ids[0] = n_gnd2;

    component_get_terminal_pos(gnd3, 0, &tx, &ty);
    int n_gnd3 = circuit_find_or_create_node(circuit, tx, ty, 5.0f);
    gnd3->node_ids[0] = n_gnd3;

    // Create bus node on signal line (horizontal bus connecting cap, diode, resistor)
    // Position it at the cap_right x, signal line y
    component_get_terminal_pos(cap, 1, &tx, &ty);  // Get cap right position
    int n_bus = circuit_find_or_create_node(circuit, x + 140, grid_y, 5.0f);  // Bus at diode column, signal row

    // === WIRES ===

    // vsrc- to gnd1 (vertical)
    wire_ortho(circuit, n_vsrc_minus, n_gnd1);

    // vsrc+ to cap_left (horizontal - they should be at same y)
    wire_ortho(circuit, n_vsrc_plus, n_cap_left);

    // cap_right to bus (horizontal)
    wire_ortho(circuit, n_cap_right, n_bus);

    // bus to diode_cathode (for negative clamper - diode conducts when signal goes negative)
    wire_ortho(circuit, n_bus, n_diode_cathode);

    // Create another bus node at resistor column for horizontal extension
    int n_bus2 = circuit_find_or_create_node(circuit, x + 200, grid_y, 5.0f);
    wire_ortho(circuit, n_bus, n_bus2);  // bus to bus2 (horizontal)
    wire_ortho(circuit, n_bus2, n_rload_top);  // bus2 to resistor (vertical)

    // diode_anode to gnd2 (vertical - for negative clamper, anode goes to ground)
    wire_ortho(circuit, n_diode_anode, n_gnd2);

    // rload_bottom to gnd3 (vertical)
    wire_ortho(circuit, n_rload_bottom, n_gnd3);

    return 7;
}

// RC Phase Shift Oscillator:
// Classic analog oscillator using inverting opamp + 3-stage RC feedback network
// Theory: Each RC stage provides 60° phase shift × 3 = 180°
//         Inverting opamp provides 180° → Total = 360° = positive feedback
// Frequency formula: f = 1/(2πRC√(2N)) where N = number of RC stages
// For N=3: f = 1/(2πRC√6) ≈ 6.5 Hz with R=10kΩ, C=1µF
// CRITICAL: Minimum gain requirement Av ≥ 29 for 3 stages
static int place_phase_shift_osc(Circuit *circuit, float x, float y) {
    // RC phase-shift oscillator on a split +/-5 V supply (op-amp rails).
    //
    //            Rf 40k
    //      +----/\/\/\----+
    //      |              |
    //  INV o---|\         |   C   C   C
    //          | >--------+---||--+--||--+--||--+---(back to INV)
    //  GND o---|/             R|   R|   R|      |
    //                          gnd gnd gnd     Ck--PLS (start-up kick)
    //
    // f = 1/(2*pi*sqrt(6)*R*C) = 6.5 kHz for R=1k, C=10n; gain Rf/R = 40 (>= 29 needed).
    // The + input is grounded so the DC operating point is 0 V and the loop is in its
    // linear region; the +/-5 V rails then limit the amplitude. (A single-supply version
    // would need the ladder's shunt resistors returned to the mid-supply bias node,
    // otherwise the DC feedback through Rf/R3 latches the output at a rail.)
    if (!circuit) return 0;

    // --- op-amp and feedback ---
    Component *u = add_comp(circuit, COMP_OPAMP_REAL, x, y - 20, 0);          // -(-40,-40) +(-40,0) OUT(40,-20)
    if (!u) return 0;
    u->props.opamp.gain = 1e6;
    u->props.opamp.gbw = 10e6;
    u->props.opamp.vmax = 5.0;
    u->props.opamp.vmin = -5.0;
    Component *rf = add_comp(circuit, COMP_RESISTOR, x, y - 120, 0);           // (-40,-120)-(40,-120)
    rf->props.resistor.resistance = 33000.0;   // gain 33: just above the 29 minimum, limits overdrive

    // --- non-inverting input to ground ---
    Component *gnd_p = add_comp(circuit, COMP_GROUND, x - 80, y + 40, 0);      // terminal (-80,20)

    // --- RC ladder, left to right along y-20, shunt resistors down to ground ---
    Component *c1 = add_comp(circuit, COMP_CAPACITOR, x + 120, y - 20, 0);    // (80,-20)-(160,-20)
    Component *r1 = add_comp(circuit, COMP_RESISTOR,  x + 160, y + 20, 90);   // (160,-20)-(160,60)
    Component *g1 = add_comp(circuit, COMP_GROUND,    x + 160, y + 80, 0);
    Component *c2 = add_comp(circuit, COMP_CAPACITOR, x + 200, y - 20, 0);    // (160,-20)-(240,-20)
    Component *r2 = add_comp(circuit, COMP_RESISTOR,  x + 240, y + 20, 90);
    Component *g2 = add_comp(circuit, COMP_GROUND,    x + 240, y + 80, 0);
    Component *c3 = add_comp(circuit, COMP_CAPACITOR, x + 280, y - 20, 0);    // (240,-20)-(320,-20)
    Component *r3 = add_comp(circuit, COMP_RESISTOR,  x + 320, y + 20, 90);
    Component *g3 = add_comp(circuit, COMP_GROUND,    x + 320, y + 80, 0);
    Component *caps[3] = {c1, c2, c3};
    Component *res[3] = {r1, r2, r3};
    for (int i = 0; i < 3; i++) {
        caps[i]->props.capacitor.capacitance = 1e-8;   // 10 nF
        res[i]->props.resistor.resistance = 1000.0;    // 1 k
    }

    // --- start-up kick: one short pulse through a coupling cap into the ladder end ---
    Component *ck = add_comp(circuit, COMP_CAPACITOR, x + 360, y - 20, 0);    // (320,-20)-(400,-20)
    ck->props.capacitor.capacitance = 1e-7;
    Component *pls = add_comp(circuit, COMP_PULSE_SOURCE, x + 400, y + 20, 0); // +(400,-20) -(400,60)
    pls->props.pulse_source.v_low = 0.0;
    pls->props.pulse_source.v_high = 1.0;
    pls->props.pulse_source.period = 100.0;
    pls->props.pulse_source.pulse_width = 0.0001;
    Component *gnd_k = add_comp(circuit, COMP_GROUND, x + 400, y + 80, 0);

    Component *label = add_comp(circuit, COMP_TEXT, x + 80, y - 220, 0);
    strncpy(label->props.text.text, "RC Phase-Shift Oscillator", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // --- terminal-aligned connections (all straight) ---
    connect_terminals(circuit, u, 1, gnd_p, 0);         // + input left to x-80, then down to ground
    connect_terminals(circuit, rf, 0, u, 0);            // Rf left down to - input
    connect_terminals(circuit, rf, 1, u, 2);            // Rf right down to output
    connect_terminals(circuit, u, 2, c1, 0);            // output -> ladder
    for (int i = 0; i < 3; i++) {
        connect_terminals(circuit, caps[i], 1, res[i], 0);   // ladder node = cap right = R top
        connect_terminals(circuit, res[i], 1, (Component *[]){g1, g2, g3}[i], 0);
        if (i < 2) connect_terminals(circuit, res[i], 0, caps[i + 1], 0);
    }
    connect_terminals(circuit, r3, 0, ck, 0);           // ladder end -> kick cap
    connect_terminals(circuit, ck, 1, pls, 0);
    connect_terminals(circuit, pls, 1, gnd_k, 0);

    // --- ladder end back to the inverting input: up, across the top, down ---
    float n3_x, n3_y, inv_x, inv_y;
    component_get_terminal_pos(r3, 0, &n3_x, &n3_y);
    component_get_terminal_pos(u, 0, &inv_x, &inv_y);
    int n3 = circuit_find_or_create_node(circuit, n3_x, n3_y, 5.0f);
    int inv = circuit_find_or_create_node(circuit, inv_x, inv_y, 5.0f);
    int k1 = circuit_find_or_create_node(circuit, n3_x, y - 160, 5.0f);
    int k2 = circuit_find_or_create_node(circuit, x - 80, y - 160, 5.0f);
    int k3 = circuit_find_or_create_node(circuit, x - 80, inv_y, 5.0f);
    circuit_add_wire(circuit, n3, k1);
    circuit_add_wire(circuit, k1, k2);
    circuit_add_wire(circuit, k2, k3);
    circuit_add_wire(circuit, k3, inv);

    return 16;
}

static int place_template_body(Circuit *circuit, CircuitTemplateType type, float x, float y) {
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
        // RLC Resonant Circuits
        case CIRCUIT_SERIES_RLC:
            return place_series_rlc(circuit, x, y);
        case CIRCUIT_PARALLEL_RLC:
            return place_parallel_rlc(circuit, x, y);
        // Measurement & Detection Circuits
        case CIRCUIT_WHEATSTONE:
            return place_wheatstone_bridge(circuit, x, y);
        case CIRCUIT_PEAK_DETECTOR:
            return place_peak_detector(circuit, x, y);
        // Signal Processing Circuits
        case CIRCUIT_CLAMPER:
            return place_clamper(circuit, x, y);
        case CIRCUIT_PHASE_SHIFT_OSC:
            return place_phase_shift_osc(circuit, x, y);
        case CIRCUIT_RC_BANDPASS:      return place_rc_bandpass(circuit, x, y);
        case CIRCUIT_LC_LOWPASS:       return place_lc_lowpass(circuit, x, y);
        case CIRCUIT_ZENER_CLIPPER:    return place_zener_clipper(circuit, x, y);
        case CIRCUIT_VOLTAGE_DOUBLER:  return place_voltage_doubler(circuit, x, y);
        case CIRCUIT_RELAXATION_OSC:   return place_relaxation_osc(circuit, x, y);
        case CIRCUIT_HALFWAVE_FILTERED:return place_halfwave_filtered(circuit, x, y);
        case CIRCUIT_HV_345_LINE:      return place_hv_345_line(circuit, x, y);
        case CIRCUIT_HV_138_LINE_VAR:  return place_hv_138_line_var(circuit, x, y);
        case CIRCUIT_MV_FEEDER:        return place_mv_feeder(circuit, x, y);
        case CIRCUIT_POLE_XFMR:        return place_pole_xfmr(circuit, x, y);
        case CIRCUIT_GEN_GSU:          return place_gen_gsu(circuit, x, y);
        case CIRCUIT_GRID_CHAIN:       return place_grid_chain(circuit, x, y);
        case CIRCUIT_FERRANTI_LINE:    return place_ferranti_line(circuit, x, y);
        case CIRCUIT_LINE_MODEL_LADDER: return place_line_model_ladder(circuit, x, y);
        case CIRCUIT_DC_LINE_DROP:     return place_dc_line_drop(circuit, x, y);
        case CIRCUIT_PC_OVERCURRENT:   return place_pc_overcurrent(circuit, x, y);
        case CIRCUIT_PC_DIFFERENTIAL:  return place_pc_differential(circuit, x, y);
        case CIRCUIT_PC_DISTANCE:      return place_pc_distance(circuit, x, y);
        case CIRCUIT_PC_BREAKER_FAIL:  return place_pc_breaker_fail(circuit, x, y);
        case CIRCUIT_SIL_LOADING:      return place_sil_loading(circuit, x, y);
        case CIRCUIT_SERIES_COMP:      return place_series_comp(circuit, x, y);
        case CIRCUIT_HV_765_LINE:      return place_hv_765_line(circuit, x, y);
        case CIRCUIT_TESLA_COIL:       return place_tesla_coil(circuit, x, y);
        case CIRCUIT_TESLA_COIL_BIG:   return place_tesla_coil_big(circuit, x, y);
        case CIRCUIT_TESLA_COIL_DETUNED: return place_tesla_coil_detuned(circuit, x, y);
        default:
            return 0;
    }
}

// One-paragraph "how it works" note placed under every example circuit.
static const char *const template_notes[CIRCUIT_TYPE_COUNT][6] = {
    [CIRCUIT_RC_LOWPASS] = {"RC LOW-PASS FILTER: R in series, C to ground. The cap cannot change", "voltage instantly, so fast wiggles are shorted away and slow ones pass.", "Corner fc = 1/(2*pi*R*C) = 1.59 kHz here; at fc the output is 0.707x and", "lags 45 deg. Try: raise the source frequency 10x and watch Vout shrink.", "PROBE: auto-placed on input and output. The source sweeps 100 Hz-20 kHz: Vout is full",
 "below 1.59 kHz and shrinks above it. Read f under the source; use 200 us/div."},
    [CIRCUIT_RC_HIGHPASS] = {"RC HIGH-PASS FILTER: C in series, R to ground. The cap blocks DC and slow", "changes but passes fast ones. fc = 1/(2*pi*R*C) = 1.59 kHz; below fc the", "output falls 20 dB/decade and leads the input in phase.", "Try: add a DC offset to the source - the output stays centred on 0 V.", "PROBE: auto-placed. Source sweeps 100 Hz-20 kHz: Vout is tiny at 100 Hz and grows",
 "to the full input above 1.59 kHz. Read f under the source; use 200 us/div."},
    [CIRCUIT_RL_LOWPASS] = {"RL LOW-PASS: L in series, R to ground. An inductor resists changes in", "current, so high-frequency current (and the drop across R) is small.", "fc = R/(2*pi*L) = 1.59 kHz. Same response shape as the RC low-pass.", "PROBE: auto-placed. Source sweeps 100 Hz-20 kHz: output falls past 1.59 kHz."},
    [CIRCUIT_RL_HIGHPASS] = {"RL HIGH-PASS: R in series, L to ground. At low frequency the inductor is", "a short and pulls the output to 0; at high frequency it is open and the", "output follows the input. fc = R/(2*pi*L) = 1.59 kHz.", "PROBE: auto-placed. Source sweeps 100 Hz-20 kHz: output rises past 1.59 kHz."},
    [CIRCUIT_VOLTAGE_DIVIDER] = {"VOLTAGE DIVIDER: two resistors in series share the supply in proportion", "to their resistance: Vout = Vin * R2/(R1+R2) = 10 V * 10k/20k = 5 V.", "Same current (0.5 mA) flows through both. Loading the output with a third", "resistor lowers Vout - that is why dividers are for references, not power.", "PROBE: the R1-R2 junction. Expect a flat 5.00 V line (use the VM readout too)."},
    [CIRCUIT_INVERTING_AMP] = {"INVERTING AMPLIFIER: the op-amp holds its - input at the + input (0 V, a", "'virtual ground'). Input current Vin/Rin must flow through Rf, so", "Vout = -Rf/Rin * Vin = -10x. Output is 5 Vpk and 180 deg out of phase.", "Try: Rf = 100k -> gain 100 and the output clips at the +/-15 V rails.", "PROBE: input (0.5 Vpk) and op-amp OUT. Expect 5 Vpk, inverted."},
    [CIRCUIT_NONINVERTING_AMP] = {"NON-INVERTING AMPLIFIER: feedback divider Rg/Rf returns a fraction of", "Vout to the - input; the op-amp drives Vout until that equals Vin.", "Gain = 1 + Rf/Rg = 11, in phase, very high input impedance.", "PROBE: input (0.5 Vpk) and op-amp OUT. Expect 5.5 Vpk, in phase."},
    [CIRCUIT_VOLTAGE_FOLLOWER] = {"VOLTAGE FOLLOWER (unity buffer): output wired straight to the - input,", "so Vout = Vin exactly. Gain 1, but it draws no current from the source", "and can drive a heavy load - an impedance converter.", "PROBE: input and OUT. The two traces sit exactly on top of each other."},
    [CIRCUIT_HALFWAVE_RECT] = {"HALF-WAVE RECTIFIER: the diode conducts only when the anode is ~0.7 V", "above the cathode, so only positive half-cycles reach the load.", "Peak out = 5 - 0.7 = 4.3 V, average = Vpk/pi = 1.4 V, ripple at 60 Hz.", "Try: add a capacitor across the load to smooth it into DC.", "PROBE: source + and the diode-R junction. Expect only the positive humps."},
    [CIRCUIT_LED_WITH_RESISTOR] = {"LED WITH SERIES RESISTOR: the resistor sets the current, the LED sets the", "voltage (~1.9 V for red). I = (5 - Vf)/330 = 9.4 mA. Without the resistor", "the LED would draw amps and burn. Try: cycle the LED colour (Vf changes).", "PROBE: the R-LED junction. Expect ~1.9 V; LED glows; current view shows 9 mA."},
    [CIRCUIT_COMMON_EMITTER] = {"COMMON-EMITTER AMPLIFIER: R1/R2 bias the base at 2.1 V so the transistor", "sits mid-way (Ve 1.4 V, Ic 1.4 mA, Vc 9 V). A small base wiggle steers", "Ic, which drops across Rc: gain ~ -Rc/(Re+re) ~ -2, inverted.", "Coupling caps pass the AC but keep the bias DC where it is.", "PROBE: base (2.1 V DC + tiny wiggle) and collector (9 V, inverted, ~2x)."},
    [CIRCUIT_COMMON_SOURCE] = {"COMMON-SOURCE (MOSFET) AMPLIFIER: 1M/330k bias the gate at 3 V, above", "Vth = 1.5 V, so ~2.7 mA flows. Gate voltage controls drain current", "(square law); the swing across Rd is the inverted output. Rs adds", "negative feedback that stabilises the bias point.", "PROBE: gate (3 V DC) and drain. Expect an inverted swing around 6 V."},
    [CIRCUIT_COMMON_DRAIN] = {"SOURCE FOLLOWER: the drain is tied to Vdd, the output is taken at the", "source. Vs tracks Vg minus Vgs, so gain is just under 1 with no", "inversion - a buffer with high input and low output impedance.", "PROBE: gate (6 V) and source. Same shape, 1.7 V lower, not inverted."},
    [CIRCUIT_MULTISTAGE_AMP] = {"TWO-STAGE AMPLIFIER: two identical common-emitter stages in cascade.", "Each inverts and gives ~4.6x, so overall ~21x and back in phase.", "Interstage cap C2 passes the signal but isolates the two bias networks.", "PROBE: input (10 mVpk) and the second collector. Expect ~0.2 Vpk, in phase."},
    [CIRCUIT_DIFFERENTIAL_PAIR] = {"DIFFERENTIAL PAIR: both bases sit at 6 V; the shared tail resistor", "carries 0.54 mA. A difference between the inputs steers that current", "from one transistor to the other, so the collectors swing in opposite", "directions. Equal (common-mode) input changes cancel - the op-amp front end.", "PROBE: both collectors. Two mirror-image swings around 10.75 V."},
    [CIRCUIT_CURRENT_MIRROR] = {"CURRENT MIRROR: Q1 is diode-connected, so Rref sets Iref = (12-0.65)/10k", "= 1.1 mA and fixes Vbe. Q2 shares the same Vbe and copies the current", "into its own load regardless of that load's resistance (up to compliance).", "PROBE: the Rload-Q2 collector node. Expect ~10.9 V; change Rload, I stays 1.1 mA."},
    [CIRCUIT_PUSH_PULL] = {"PUSH-PULL (CLASS B) OUTPUT: NPN sources current on positive swings, PNP", "sinks it on negative swings; each is an emitter follower (gain ~1).", "Neither conducts within +/-0.7 V of zero, so you see crossover", "distortion - the flat step at the zero crossing. Bias diodes would fix it.", "PROBE: input (5 Vpk) and the output node. Same sine with a flat notch at 0 V."},
    [CIRCUIT_CMOS_INVERTER] = {"CMOS INVERTER: PMOS on top, NMOS below, gates tied together. Input low", "turns the PMOS on -> output pulled to Vdd; input high turns the NMOS on", "-> output pulled to 0 V. Only one conducts at a time, so almost no", "static current. The 100 pF load sets the edge speed.", "PROBE: gate input and the drain output. Expect the square wave inverted."},
    [CIRCUIT_INTEGRATOR] = {"OP-AMP INTEGRATOR: input current Vin/R must flow into the capacitor, so", "Vout ramps at -Vin/(R*C). A square wave in becomes a triangle out.", "Any DC offset also integrates, so a real integrator needs a bleed", "resistor across C - watch this one drift toward the rail.", "PROBE: square input and OUT. Expect a triangle that slowly drifts to a rail."},
    [CIRCUIT_DIFFERENTIATOR] = {"OP-AMP DIFFERENTIATOR: the capacitor passes current proportional to", "dVin/dt, which flows through R: Vout = -R*C*dVin/dt. A triangle in", "becomes a square out (+/-0.4 V here). Edges produce sharp spikes.", "PROBE: triangle input and OUT. Expect a +/-0.4 V square wave."},
    [CIRCUIT_SUMMING_AMP] = {"SUMMING AMPLIFIER: each input pushes Vi/Ri into the virtual ground; the", "currents add and all flow through Rf. With equal resistors", "Vout = -(V1+V2+V3) = -6 V. The basis of audio mixers and DACs.", "PROBE: op-amp OUT. Expect a flat -6 V line."},
    [CIRCUIT_COMPARATOR] = {"COMPARATOR: no feedback, so the op-amp slams to a rail depending on which", "input is higher. The 10k/10k divider sets a 5 V threshold; the input", "sine (5 V +/- 6 V) crosses it twice per cycle -> a 100 Hz square wave.", "PROBE: sine input and OUT. Expect a +/-15 V square wave switching at 5 V."},
    [CIRCUIT_FULLWAVE_BRIDGE] = {"FULL-WAVE BRIDGE RECTIFIER: on each half-cycle a diagonal pair of diodes", "conducts, so both halves reach the load with the same polarity. Peak", "12 - 1.4 = 10.6 V; the cap holds the peak and ripples at 120 Hz by about", "I/(f*C) = 0.9 V. Bigger C or lighter load -> less ripple.", "PROBE: the load (cap) node. Expect ~10 V DC with 0.9 V sawtooth ripple."},
    [CIRCUIT_CENTERTAP_RECT] = {"CENTER-TAP RECTIFIER: the transformer steps 120 V down to 12 V across the", "whole secondary; the tap is ground, so each half gives 6 Vpk. Each diode", "conducts on alternate half-cycles -> full-wave DC with only one diode drop.", "PROBE: the load node. Expect ~5.3 V DC with small 120 Hz ripple."},
    [CIRCUIT_AC_DC_SUPPLY] = {"AC-DC POWER SUPPLY: transformer (10:1) -> bridge rectifier -> reservoir", "capacitor. 170 Vpk becomes 17 Vpk, minus two diode drops = 15.6 V DC", "with ~1.3 V of 120 Hz ripple into 100 ohm. This is the front end of", "every linear supply; a regulator would follow.", "PROBE: transformer secondary (17 Vpk sine) and the load node (~15 V DC)."},
    [CIRCUIT_AC_DC_AMERICAN] = {"120 V / 60 Hz TO 12 V DC: same as the AC-DC supply with a 2200 uF", "reservoir (ripple ~0.6 V). Note the cap is rated 25 V - raise the turns", "ratio and watch the peak exceed the rating.", "PROBE: the load node. Expect ~15 V DC with ~0.6 V ripple at 120 Hz."},
    [CIRCUIT_DIFFERENCE_AMP] = {"DIFFERENCE AMPLIFIER: four equal resistors make Vout = V2 - V1. Anything", "common to both inputs cancels; only the difference is amplified (x1).", "Try: mismatch one resistor to 11k and see common-mode leak through.", "PROBE: OUT. Expect the 1 Vpk sine shifted by the 0.5 V DC input."},
    [CIRCUIT_TRANSIMPEDANCE] = {"TRANSIMPEDANCE AMPLIFIER: converts a current into a voltage. The input", "current cannot enter the op-amp, so it all flows through Rf:", "Vout = -I * Rf = 1 mA * 10k = 10 V. Used with photodiodes.", "PROBE: OUT. Expect a flat 10 V (1 mA x 10k). Change the current source."},
    [CIRCUIT_INSTR_AMP] = {"INSTRUMENTATION AMPLIFIER: two input buffers share gain resistor Rg", "(gain 1 + 2R/Rg = 21), then a difference amp subtracts. Very high input", "impedance on both inputs and excellent common-mode rejection.", "PROBE: the final op-amp OUT. Expect ~2.1 Vpk (gain 21) on a -1 V offset."},
    [CIRCUIT_SALLEN_KEY_LP] = {"SALLEN-KEY LOW-PASS: a 2nd-order active filter - two RC sections with the", "op-amp bootstrapping the first cap for a sharper knee. fc = 1/(2*pi*R*C)", "= 1.59 kHz, rolls off 40 dB/decade. Try the Bode tool.", "PROBE: auto-placed. Source sweeps 100 Hz-20 kHz: 2nd-order roll-off above 1.59 kHz."},
    [CIRCUIT_BANDPASS_ACTIVE] = {"ACTIVE BAND-PASS: a high-pass RC into the op-amp and a low-pass RC in the", "feedback. Only frequencies near f0 = 1/(2*pi*R*C) = 1.59 kHz pass;", "sweep the source or use the Bode tool to see the peak.", "PROBE: auto-placed. Source sweeps 100 Hz-20 kHz: output peaks near 1.6 kHz."},
    [CIRCUIT_NOTCH_FILTER] = {"TWIN-T NOTCH: two T networks (R-R-C and C-C-R/2) whose outputs cancel", "exactly at f = 1/(2*pi*R*C) = 60 Hz. Everything else passes. The classic", "hum filter - detune one resistor and the notch fills in.", "PROBE: auto-placed on the source and the load. Sweeping 10-300 Hz the output vanishes at 60 Hz."},
    [CIRCUIT_WIEN_OSCILLATOR] = {"WIEN BRIDGE OSCILLATOR: the series/parallel RC network has zero phase", "shift and 1/3 gain at f = 1/(2*pi*R*C) = 1.59 kHz; the amplifier gives", "a little over 3x, so that frequency builds up until the rails limit it.", "A tiny pulse kicks it off (an ideal loop at exactly 0 V never starts).", "PROBE: op-amp OUT. Expect a 1.6 kHz sine growing to the +/-15 V rails."},
    [CIRCUIT_CURRENT_SOURCE] = {"CONSTANT-CURRENT SOURCE: the divider fixes the base at 2.16 V, so the", "emitter sits at 1.46 V and Re sets I = 3.1 mA. The collector delivers", "that current to any load up to the compliance limit - change Rload and", "the current stays put.", "PROBE: the load top node. Expect 3.1 V (3.1 mA x 1k); change Rload -> V scales."},
    [CIRCUIT_WINDOW_COMP] = {"WINDOW COMPARATOR: two comparators check Vin against 3.33 V and 1.67 V", "from a 3-resistor divider. Both outputs go high only inside the window;", "the summing resistors light the LED only then. Move Vin outside to test.", "PROBE: both comparator outputs (+15 V inside window). LED on at 2.5 V input."},
    [CIRCUIT_HYSTERESIS_COMP] = {"SCHMITT TRIGGER: the - input sits at a 6 V reference; the input (6 V +/- 3 V)", "is compared to it. Positive feedback (Rf 100k / R 10k) moves the threshold", "+/-1.4 V after each switch, so the output flips only past the far threshold.", "PROBE: sine input and OUT. Square wave switching at 6 +/- 1.4 V, not at 6 V."},
    [CIRCUIT_ZENER_REF] = {"ZENER REFERENCE: the zener is reverse-biased through Rs; once it reaches", "breakdown it holds ~5.1 V while Rs absorbs the rest of the 12 V.", "Extra current only changes Vz by I*Rz. Load lightly or it drops out.", "PROBE: the zener cathode node. Expect a flat 5.1 V. Change Vin 8-20 V."},
    [CIRCUIT_PRECISION_RECT] = {"PRECISION RECTIFIER: D1/D2 sit inside op1's feedback loop, so their 0.7 V", "drop is divided by the loop gain: P = |Vin| for Vin < 0, 0 otherwise. op2 sums", "Vin (10k) and P (5k): out = -(Vin + 2P) = -|Vin|. Works down to millivolts.", "PROBE: input and op2 OUT. Expect -|sin|: every half-cycle negative, 1 Vpk."},
    [CIRCUIT_7805_REG] = {"7805 REGULATOR: 9 V in, 5.00 V out regardless of load or small input", "changes. Needs at least ~7 V in (dropout). The small caps stabilise it.", "Try: lower Vin to 6 V and watch the output follow.", "PROBE: OUT pin node. Expect a flat 5.00 V. Drop Vin below 7 V to see dropout."},
    [CIRCUIT_LM317_REG] = {"LM317 ADJUSTABLE REGULATOR: it keeps 1.25 V between OUT and ADJ, so", "Vout = 1.25 * (1 + R2/R1) = 1.25 * (1 + 720/240) = 5.0 V. Change R2 to", "set any voltage up to the input minus dropout.", "PROBE: OUT node. Expect 5.0 V. Change R2 (720) to 1.2k -> 7.5 V."},
    [CIRCUIT_TL431_REF] = {"TL431 SHUNT REFERENCE: with REF tied to the cathode it regulates its", "own cathode at 2.50 V, shunting whatever current Rs supplies beyond", "the load. A programmable zener - add a divider on REF for other voltages.", "PROBE: the cathode node. Expect a flat 2.50 V."},
    [CIRCUIT_SERIES_RLC] = {"SERIES RLC: at resonance f0 = 1/(2*pi*sqrt(L*C)) = 159 Hz the reactances", "cancel and only R (3.3 ohm) limits the current: 1.5 A. Q = sqrt(L/C)/R = 3, so", "the capacitor voltage magnifies to Q*Vin = 15 V right at f0 and falls either side.", "PROBE: auto-placed on the source and across C. Source sweeps 30-800 Hz: Vc peaks at 159 Hz."},
    [CIRCUIT_PARALLEL_RLC] = {"PARALLEL RLC (TANK): at f0 = 159 Hz the tank impedance is maximum, so", "almost the whole source voltage appears across it. Q = R*sqrt(C/L) = 100:", "shift the source a few Hz and the output collapses.", "PROBE: auto-placed: source vs the tank (one net across the top). Sweeping 50-500 Hz, the",
 "tank voltage is tiny off-resonance and rises toward the full 5 V only around 159 Hz."},
    [CIRCUIT_WHEATSTONE] = {"WHEATSTONE BRIDGE: two dividers side by side. With R4 = 1.1k the right", "midpoint sits at 5.24 V vs 5.00 V on the left - a 0.24 V imbalance that", "measures the unknown resistor. Set R4 = 1k and the bridge nulls.", "PROBE: both bridge midpoints. Expect 5.00 V and 5.24 V (0.24 V imbalance)."},
    [CIRCUIT_PEAK_DETECTOR] = {"PEAK DETECTOR: the op-amp charges C through the diode whenever Vin", "exceeds the stored voltage; when Vin falls the diode blocks and C holds the", "peak. The 47k bleed (R*C = 47 ms) lets it decay, so the output rides the", "envelope of the 1 kHz carrier whose amplitude sweeps 1 V -> 5 V -> 1 V each second.", "PROBE: input and the cap node at 50 ms/div: the cap traces the envelope 1..5 V."},
    [CIRCUIT_CLAMPER] = {"CLAMPER (DC RESTORER): the cap charges to the negative peak through the", "diode, then acts as a 5 V battery in series with the signal. The whole", "sine is shifted so its bottom sits at ~-0.7 V. R*C >> period keeps it.", "PROBE: input and the cap-diode node at 50 ms/div: bottom pinned at -0.7 V, top follows 2A."},
    [CIRCUIT_PHASE_SHIFT_OSC] = {"RC PHASE-SHIFT OSCILLATOR: three RC sections each shift 60 deg at", "f = 1/(2*pi*sqrt(6)*R*C) = 6.5 kHz, totalling 180 deg; the inverting", "amplifier adds the other 180 deg. Gain must exceed 29 (Rf/R = 33 here).", "Split +/-5 V rails limit the swing; a pulse through Ck starts it.", "PROBE: op-amp OUT. Expect ~6.5 kHz clipped sine, +/-5 V. Set dt 1 us."},
    [CIRCUIT_RC_BANDPASS] = {"RC BAND-PASS: C1/R1 form a high-pass (fc1 = 1/(2*pi*R1*C1) = 800 Hz), then R2/C2", "a low-pass (fc2 = 3.2 kHz). Only the band between passes; the response peaks near", "sqrt(fc1*fc2) = 1.6 kHz at about 0.8x and falls 20 dB/decade on both sides.", "PROBE: auto-placed. Source sweeps 100 Hz-20 kHz: output rises, peaks ~1.6 kHz, falls."},
    [CIRCUIT_LC_LOWPASS] = {"LC LOW-PASS: series L, shunt C, load R. Second order: f0 = 1/(2*pi*sqrt(LC)) =", "1.59 kHz and it rolls off 40 dB/decade above f0 (twice as steep as RC). The load", "sets the damping: Q = R*sqrt(C/L) = 1 here, so only a slight peak near f0.", "PROBE: auto-placed. Sweep 100 Hz-20 kHz: flat, slight hump at 1.6 kHz, then steep fall."},
    [CIRCUIT_ZENER_CLIPPER] = {"ZENER CLIPPER: two 5.1 V zeners back to back. Below ~5.8 V neither conducts and", "the output follows the input; above it one zener breaks down while the other is", "forward biased, clamping the output at +/-(Vz + 0.7). R limits the zener current.", "PROBE: auto-placed. Amplitude sweeps 1-10 V: the tops flatten once the input passes 5.8 V."},
    [CIRCUIT_VOLTAGE_DOUBLER] = {"VOLTAGE DOUBLER: on negative half-cycles D1 charges C1 to Vpk (clamper); on positive", "ones the clamped waveform swings to 2*Vpk and D2 charges C2 to it (peak detector).", "Vout = 2*Vpk - 2*0.7 V. Each extra diode/cap stage adds another Vpk (Cockcroft-Walton).", "PROBE: auto-placed on source and C2. Amplitude sweeps 1-5 V: output tracks 2*A - 1.4."},
    [CIRCUIT_RELAXATION_OSC] = {"RELAXATION OSCILLATOR: the op-amp is a Schmitt trigger (R1/R2 feed half of Vout", "to +). C charges through R toward the rail until V(C) crosses the threshold, the", "output flips and C charges the other way. f = 1/(2RC*ln((1+b)/(1-b))) = 455 Hz.", "PROBE: auto-placed on OUT (square, +/-15 V). Also probe C: a triangle-ish exponential."},
    [CIRCUIT_HALFWAVE_FILTERED] = {"HALF-WAVE + SMOOTHING CAP: the diode charges C to the peak; between peaks the", "load drains it, giving a sawtooth ripple dV = I/(f*C) = (Vdc/R)/(60*100u) ~ 1.5 V.", "Bigger C or lighter load -> less ripple; this is the simplest DC supply.", "PROBE: auto-placed on the cap. Amplitude sweeps 2-10 V: DC follows Vpk - 0.7 with ripple."},
    [CIRCUIT_HV_345_LINE] = {"345 kV TRANSMISSION LINE (ERCOT/CREZ class): single-phase equivalent of 100 miles of", "twin-Drake conductor: R = 0.06 ohm/mi, X = 0.55 ohm/mi -> 6 ohm + 145.9 mH. Source is the", "phase-to-neutral peak 345k/sqrt3*sqrt2 = 281.7 kV; the 198.4 ohm load draws 600 MW (3-ph).", "Expect I = 941 A rms, load 186.7 kV rms (264 kVpk): a 6.3 % drop, mostly I*X, lagging.", "PROBE: auto-placed at both ends. Compare amplitudes (281.7 vs 264 kVpk) and the phase lag."},
    [CIRCUIT_HV_138_LINE_VAR] = {"138 kV LINE + VAR SUPPORT: 30 miles of single Drake (3.9 ohm + 57.3 mH) feeding 90 MW at", "power factor 0.9 lagging (171.5 ohm + 0.22 H). The reactive current through the line X", "drops the bus to about 74 kV rms (-6.7 %). Close SW: the 6.1 uF capacitor bank supplies", "the load's VARs locally and the bus recovers to ~78 kV. This is what substation cap banks do.", "PROBE: auto-placed on the source and the load bus. Toggle SW while running (100 kV/div)."},
    [CIRCUIT_MV_FEEDER] = {"12.47 kV DISTRIBUTION FEEDER: 7.2 kV phase-to-neutral (10.18 kVpk), 5 miles of 1/0 ACSR", "(1.53 ohm + 8.22 mH), 1 MW per phase (51.84 ohm). Expect 134.5 A, 6,973 V rms at the end", "(-3.2 %). Utilities keep feeders within +/-5 % (ANSI C84.1) with regulators and cap banks.", "PROBE: auto-placed. The far end sits ~3 % below the substation; add load to see it sag."},
    [CIRCUIT_POLE_XFMR] = {"POLE TRANSFORMER: 7.2 kV feeder phase to a 240 V service (turns ratio 30:1). A 25 kVA", "can serves a few houses; this one feeds a 5 kW load (11.5 ohm). The secondary is really", "center-tapped (two 120 V halves); here it is drawn as the full 240 V winding.", "Current scales the other way: 20.8 A on the house side is only 0.7 A on the 7.2 kV side.", "PROBE: auto-placed on the 7.2 kV side and the 240 V side (100 V/div vs 5 kV/div!)."},
    [CIRCUIT_GEN_GSU] = {"GENERATOR + GSU: an 18 kV (line-to-line) machine, 14.7 kVpk per phase, with X'' = 0.15 pu", "on 700 MVA (0.184 mH) behind its terminals. The generator step-up transformer (1:19.17)", "lifts it to the 345 kV bus, which feeds 600 MW (198.4 ohm per phase).", "Referred to 345 kV the machine reactance is 25 ohm; at unity pf the bus stays within 1 %.", "PROBE: auto-placed on the 18 kV terminals and the 345 kV bus - note the 19x scale change."},
    [CIRCUIT_GRID_CHAIN] = {"WHOLE GRID IN ONE LINE: generator 18 kV -> GSU -> 100 mi of 345 kV -> 345/138 kV auto", "-> 30 mi of 138 kV -> 138/12.47 kV substation -> 5 mi feeder -> pole transformer -> a house.", "Each transformer trades voltage for current; each line drops I*Z. With only this one house", "the lines are unloaded, so the 240 V end sits at ~239 V; scale the loads up to see the sag.", "PROBE: probe any bus: 14.7 kVpk, 282 kVpk, 113 kVpk, 10.2 kVpk, 339 Vpk left to right."},
    [CIRCUIT_FERRANTI_LINE] = {"FERRANTI EFFECT: a long unloaded line is a capacitor; its charging current flowing through", "the series inductance RAISES the receiving-end voltage. 200 mi of 345 kV as a pi section:", "12 ohm + 291.8 mH with 2.12 uF at each end, open end (10 Mohm). Expect +9.9 % (309.6 kVpk).", "Close SW to connect a 3.54 H shunt reactor: it absorbs the charging VARs and cancels the rise.", "PROBE: auto-placed on both ends. The far end is HIGHER than the source until SW closes."},
    [CIRCUIT_TESLA_COIL] = {"TESLA COIL: the NST (120 V -> 9 kV rms, current-limited by the 10 ohm) charges C1 = 25 nF.", "Near each peak the 3.2 mm gap breaks down (~9.6 kV) and C1 rings with L1 = 29 uH at", "f1 = 1/(2 pi sqrt(L1 C1)) = 186 kHz. The T-model (L1(1-k), k L1, 1:32, L2(1-k), k = 0.2)", "couples it to the 30 mH secondary, which resonates with the toroid (14.5 pF) + 10 pF self-C", "at the same 186 kHz, so the 1.2 J in C1 pumps the toroid to ~150 kV (E = 1/2 C V^2, minus losses).", "PROBE: auto-placed on the toroid. At 10 us/div you see one ring burst; arcs show on the gaps."},
    [CIRCUIT_TESLA_COIL_BIG] = {"BIG TOROID: an 8x24 in toroid is 26.5 pF (Bert Pool: C = (1.2781 - d/D) 2.8 sqrt(pi (D-d) d/4)),", "so the secondary now resonates at 152 kHz. The primary was RETUNED with C1 = 38 nF to match.", "More C1 at the same 9.6 kV means more energy per bang (1.75 J vs 1.15 J): the toroid reaches a", "higher voltage and the streamer jumps a 45 mm gap (135 kV in this simple 3 kV/mm model).", "Click the toroid and edit D / d: watch C and the resonance move. Compare with Tesla (detuned).", "PROBE: auto-placed on the toroid; also probe C1 (12.7 kV sawtooth) and the primary node."},
    [CIRCUIT_TESLA_COIL_DETUNED] = {"DETUNED COIL: same 8x24 in toroid (secondary at 152 kHz) but the primary has an 18 nF", "cap, ringing at 220 kHz. The two resonators are 45 % apart, so energy sloshes back", "into the primary instead of building up on the toroid: peak voltage is far lower and the", "40 mm rod gap never fires. Tuning the primary (C1 or the tap on L1) is the first thing a", "coiler does. Fix it here: click C1 and set 38 nF, or shrink the toroid to 4x13 in.", "PROBE: auto-placed on the toroid. Compare the envelope with the tuned coils."},
    [CIRCUIT_LINE_MODEL_LADDER] = {"LINE MODEL LADDER: one 138 kV source feeds three copies of a 30-mile line into equal", "90 MW loads. Row 1 keeps only the conductor resistance (R = 0.13 ohm/mi x 30 = 3.9 ohm).", "Row 2 adds the series reactance (X = 0.72 ohm/mi -> 57 mH): the drop grows and lags.", "Row 3 is the nominal pi: 6 uS/mi of charging susceptance as C/2 at each end (Ferranti).", "Click any line: length, R/mi, X/mi, B/mi and the model number are editable properties.", "PROBE: the three load buses (auto probe on row 2). Row 1 ~110.7 kVpk, row 2 ~110.1 kVpk."},
    [CIRCUIT_DC_LINE_DROP] = {"LINE DROP BASICS: the wire is a resistor too. I = V/(R_wire + R_load) = 12/11 = 1.09 A,", "so the load only sees 10.9 V and the wire burns 1.2 W. Longer wire = more ohms (R = rho L/A):", "double the length and the drop doubles. Everything in the power examples is this idea", "plus reactance (X) and charging (B) - open Line Model Ladder to see those added one by one.", "PROBE: auto-placed on the load; also probe the battery to compare 12 V vs 10.9 V."},
    [CIRCUIT_PC_OVERCURRENT] = {"50/51 OVERCURRENT: the CT (600:5, N = 120) turns 600 A of feeder current into 5 A through the", "1 ohm burden = 7.07 Vpk. Diode + 10 uF hold the peak; the comparator trips above 8 V, i.e. when", "the primary exceeds 738 A rms (the 50 element; hold tau = 100 ms). The fault switch (5.3 ohm)", "closes at t = 40 ms and raises the current to 1200 A: TRIP goes high. AEP feeds every BES", "breaker from redundant A/B relays; a 51 time curve is just a longer RC delay on V_hold.", "PROBE: TRIP is auto-probed; also probe the burden top (7 V -> 14 V) and the pulse source."},
    [CIRCUIT_PC_DIFFERENTIAL] = {"87 LINE DIFFERENTIAL: the two 120:1 CTs bracket the protected zone and are wired in opposition", "so their secondary currents circulate: with equal current at both ends (load, or a THROUGH fault", "beyond CT2 at t = 240 ms) nothing flows in the 1 ohm differential burden. An INTERNAL fault", "(2 ohm at t = 100 ms) makes I1 = 2828 A but I2 = 257 A: (I1-I2)/120 = 21 A rms -> 30 Vpk on R_d,", "far above the 1 V pickup (hold tau 22 ms). AEP prefers 87L over direct fiber, distance as backup.", "PROBE: TRIP auto-probed; probe R_d (top) to see the two faults treated differently."},
    [CIRCUIT_PC_DISTANCE] = {"21 DISTANCE, ZONE 1: the relay compares |I| x Z_set (CT 400:1 into a 3.35 ohm replica, equal to", "80 % of the 50-mile line impedance referred through the VT ratio) against |V| from the VT (2875:1).", "|I Z_set| > |V| means the apparent impedance V/I is inside the reach -> TRIP. The 40 % fault", "(t = 100 ms): V_sec 38 V, I Z_set 76 V -> trip. The 100 % fault (t = 240 ms): 52 V vs 42 V -> no trip", "(zone 2, with a 0.3 s timer, covers it). Zone 1 is set short of the far end so it never overreaches.", "PROBE: TRIP auto-probed; also probe the two peak-detector caps (|V| below-left, |I Z| top)."},
    [CIRCUIT_PC_BREAKER_FAIL] = {"50BF BREAKER FAILURE: when a relay trips a breaker (TRIP pulse at 50 ms) the current must vanish", "within ~5 cycles. START = TRIP AND current-still-present charges C through R (tau = 150 ms); if", "the current is still there when the timer expires, BFT = timer AND current trips the adjacent", "breakers. Here the current pulse stays on (stuck breaker): BFT fires at ~200 ms. Set the 50BF", "pulse width to 83 ms (healthy breaker): C only reaches 5 V of the 7.6 V (0.632 x 12) threshold.", "PROBE: BFT auto-probed; probe the capacitor to see the timer ramp reset when START drops."},
    [CIRCUIT_SIL_LOADING] = {"SURGE IMPEDANCE LOADING: a line loaded with its characteristic impedance Zc = sqrt(L/C) = 283 ohm", "absorbs exactly the VARs it generates: the voltage profile is flat (Vr/Vs = 0.996 here) and", "the angle is small. P_SIL = V^2/Zc = 345^2/283 = 420 MW. Close SW (2 x SIL, 141 ohm): the line", "now needs VARs it cannot supply and the far end sags to 0.80. St. Clair: ~2 SIL is OK at 100 mi,", "~1 SIL at 300 mi - long lines are limited by voltage and stability, not by conductor heating.", "PROBE: auto-placed on both ends: nearly equal at SIL, 20 % apart after closing SW."},
    [CIRCUIT_SERIES_COMP] = {"SERIES COMPENSATION: the line reactance X = 120 ohm (200 mi) limits how much power can flow", "before the far end sags. A capacitor with Xc = 60 ohm (44 uF) in the middle cancels half of it:", "at 2 x SIL the receiving end rises from 0.80 to about 0.90. Close SW to bypass the capacitor", "and watch the drop return. AEP uses series caps on long 765/345 kV paths (with protection", "against subsynchronous resonance and MOV bypass on faults).", "PROBE: auto-placed on both ends; the source-side probe barely moves, the load end jumps."},
    [CIRCUIT_HV_765_LINE] = {"765 kV (AEP's backbone since 1969): 300 miles of six-conductor bundle, R = 0.02, X = 0.53 ohm/mi,", "B = 8.5 uS/mi (bundling lowers X and raises B). Zc = sqrt(0.53/8.5e-6) = 250 ohm, so", "SIL = 765^2/250 = 2340 MW - about 6 x a 345 kV circuit with half the losses per MW. Loaded at", "SIL the 300-mile profile stays flat (0.99). One nominal pi for 300 mi is coarse: for accuracy", "split it into 3 x 100 mi sections (place three TLine parts) - the pi model is exact only per section.", "PROBE: auto-placed on both ends; 200 kV/div. Try 2 x SIL (125 ohm) and 600 mi."},
};


// ---------------------------------------------------------------------------------------
// Additional teaching circuits (2026-08). All use straight, terminal-aligned wiring.
// ---------------------------------------------------------------------------------------
#define TN(cx, cy) circuit_find_or_create_node(circuit, (cx), (cy), 5.0f)
#define TW(a_, b_) circuit_add_wire(circuit, (a_), (b_))

static void set_freq_sweep(Component *v, double f0, double f1, double t) {
    v->props.ac_voltage.frequency_sweep.enabled = true;
    v->props.ac_voltage.frequency_sweep.mode = SWEEP_LOG;
    v->props.ac_voltage.frequency_sweep.start_value = f0;
    v->props.ac_voltage.frequency_sweep.end_value = f1;
    v->props.ac_voltage.frequency_sweep.sweep_time = t;
    v->props.ac_voltage.frequency_sweep.repeat = true;
    v->props.ac_voltage.frequency_sweep.bidirectional = true;
}
static void set_amp_sweep(Component *v, double a0, double a1, double t) {
    v->props.ac_voltage.amplitude_sweep.enabled = true;
    v->props.ac_voltage.amplitude_sweep.mode = SWEEP_LINEAR;
    v->props.ac_voltage.amplitude_sweep.start_value = a0;
    v->props.ac_voltage.amplitude_sweep.end_value = a1;
    v->props.ac_voltage.amplitude_sweep.sweep_time = t;
    v->props.ac_voltage.amplitude_sweep.repeat = true;
    v->props.ac_voltage.amplitude_sweep.bidirectional = true;
}
static Component *add_label(Circuit *circuit, float x, float y, const char *text) {
    Component *l = add_comp(circuit, COMP_TEXT, x, y, 0);
    if (l) { strncpy(l->props.text.text, text, sizeof(l->props.text.text)-1); l->props.text.font_size = 1; }
    return l;
}

// RC band-pass: C1/R1 high-pass (fc 800 Hz) followed by R2/C2 low-pass (fc 3.2 kHz)
static int place_rc_bandpass(Circuit *circuit, float x, float y) {
    Component *v = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 60, 0);            // +(0,20) -(0,100)
    if (!v) return 0;
    v->props.ac_voltage.amplitude = 1.0; v->props.ac_voltage.frequency = 1600.0;
    set_freq_sweep(v, 100, 20000, 3);
    Component *g0 = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    Component *c1 = add_comp(circuit, COMP_CAPACITOR, x + 60, y + 20, 0);        // (20,20)-(100,20)
    c1->props.capacitor.capacitance = 200e-9;
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 100, y + 60, 90);       // (100,20)-(100,100)
    r1->props.resistor.resistance = 1000.0;
    Component *g1 = add_comp(circuit, COMP_GROUND, x + 100, y + 120, 0);
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 160, y + 20, 0);        // (120,20)-(200,20)
    r2->props.resistor.resistance = 10000.0;
    Component *c2 = add_comp(circuit, COMP_CAPACITOR, x + 200, y + 60, 90);      // (200,20)-(200,100)
    c2->props.capacitor.capacitance = 5e-9;
    Component *g2 = add_comp(circuit, COMP_GROUND, x + 200, y + 120, 0);
    add_label(circuit, x + 40, y - 40, "RC Band-Pass");
    connect_terminals(circuit, v, 1, g0, 0);
    connect_terminals(circuit, v, 0, c1, 0);
    connect_terminals(circuit, r1, 1, g1, 0);
    connect_terminals(circuit, c2, 1, g2, 0);
    int n1 = TN(x + 100, y + 20), r2l = TN(x + 120, y + 20), n2 = TN(x + 200, y + 20);
    TW(n1, r2l);
    c1->node_ids[1] = n1; r1->node_ids[0] = n1; r2->node_ids[0] = r2l; r2->node_ids[1] = n2; c2->node_ids[0] = n2;
    return 9;
}

// LC low-pass: L series, C shunt, R load. f0 = 1/(2*pi*sqrt(LC)) = 1.59 kHz, Q = R*sqrt(C/L) = 1
static int place_lc_lowpass(Circuit *circuit, float x, float y) {
    Component *v = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 60, 0);
    if (!v) return 0;
    v->props.ac_voltage.amplitude = 1.0; v->props.ac_voltage.frequency = 1000.0;
    set_freq_sweep(v, 100, 20000, 3);
    Component *g0 = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    Component *l = add_comp(circuit, COMP_INDUCTOR, x + 60, y + 20, 0);          // (20,20)-(100,20)
    l->props.inductor.inductance = 10e-3;
    Component *c = add_comp(circuit, COMP_CAPACITOR, x + 100, y + 60, 90);       // (100,20)-(100,100)
    c->props.capacitor.capacitance = 1e-6;
    Component *g1 = add_comp(circuit, COMP_GROUND, x + 100, y + 120, 0);
    Component *r = add_comp(circuit, COMP_RESISTOR, x + 180, y + 60, 90);        // (180,20)-(180,100)
    r->props.resistor.resistance = 100.0;
    Component *g2 = add_comp(circuit, COMP_GROUND, x + 180, y + 120, 0);
    add_label(circuit, x + 30, y - 40, "LC Low-Pass (2nd order)");
    connect_terminals(circuit, v, 1, g0, 0);
    connect_terminals(circuit, v, 0, l, 0);
    connect_terminals(circuit, c, 1, g1, 0);
    connect_terminals(circuit, r, 1, g2, 0);
    int n1 = TN(x + 100, y + 20), rt = TN(x + 180, y + 20);
    TW(n1, rt);
    l->node_ids[1] = n1; c->node_ids[0] = n1; r->node_ids[0] = rt;
    return 8;
}

// Zener clipper: 1k series, two 5.1 V zeners back to back clamp at about +/-5.8 V
static int place_zener_clipper(Circuit *circuit, float x, float y) {
    Component *v = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 60, 0);
    if (!v) return 0;
    v->props.ac_voltage.amplitude = 10.0; v->props.ac_voltage.frequency = 1000.0;
    set_amp_sweep(v, 1.0, 10.0, 1.0);
    Component *g0 = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    Component *r = add_comp(circuit, COMP_RESISTOR, x + 60, y + 20, 0);          // (20,20)-(100,20)
    r->props.resistor.resistance = 1000.0;
    Component *z1 = add_comp(circuit, COMP_ZENER, x + 100, y + 60, 270);         // K top (100,20), A bottom (100,100)
    z1->props.zener.vz = 5.1;
    Component *z2 = add_comp(circuit, COMP_ZENER, x + 100, y + 140, 90);         // A top (100,100), K bottom (100,180)
    z2->props.zener.vz = 5.1;
    Component *g1 = add_comp(circuit, COMP_GROUND, x + 100, y + 200, 0);         // terminal (100,180)
    add_label(circuit, x + 30, y - 40, "Zener Clipper (limiter)");
    connect_terminals(circuit, v, 1, g0, 0);
    connect_terminals(circuit, v, 0, r, 0);
    int n = TN(x + 100, y + 20), mid = TN(x + 100, y + 100), gb = TN(x + 100, y + 180);
    r->node_ids[1] = n; z1->node_ids[1] = n; z1->node_ids[0] = mid; z2->node_ids[0] = mid; z2->node_ids[1] = gb; g1->node_ids[0] = gb;
    return 7;
}

// Voltage doubler (Villard/Greinacher): C1 + D1 clamp, D2 + C2 peak detect -> ~2*Vpk - 1.4 V
static int place_voltage_doubler(Circuit *circuit, float x, float y) {
    Component *v = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 60, 0);
    if (!v) return 0;
    v->props.ac_voltage.amplitude = 5.0; v->props.ac_voltage.frequency = 1000.0;
    set_amp_sweep(v, 1.0, 5.0, 1.0);
    Component *g0 = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    Component *c1 = add_comp(circuit, COMP_CAPACITOR, x + 60, y + 20, 0);        // (20,20)-(100,20)
    c1->props.capacitor.capacitance = 1e-6;
    Component *d1 = add_comp(circuit, COMP_DIODE, x + 100, y + 60, 270);         // K top (100,20), A bottom (100,100)
    Component *g1 = add_comp(circuit, COMP_GROUND, x + 100, y + 120, 0);         // terminal (100,100)
    Component *d2 = add_comp(circuit, COMP_DIODE, x + 160, y + 20, 0);           // A (120,20), K (200,20)
    Component *c2 = add_comp(circuit, COMP_CAPACITOR, x + 200, y + 60, 90);      // (200,20)-(200,100)
    c2->props.capacitor.capacitance = 1e-6;
    Component *g2 = add_comp(circuit, COMP_GROUND, x + 200, y + 120, 0);
    Component *r = add_comp(circuit, COMP_RESISTOR, x + 280, y + 60, 90);        // (280,20)-(280,100)
    r->props.resistor.resistance = 100000.0;
    Component *g3 = add_comp(circuit, COMP_GROUND, x + 280, y + 120, 0);
    add_label(circuit, x + 40, y - 40, "Voltage Doubler");
    connect_terminals(circuit, v, 1, g0, 0);
    connect_terminals(circuit, v, 0, c1, 0);
    connect_terminals(circuit, d1, 0, g1, 0);
    connect_terminals(circuit, c2, 1, g2, 0);
    connect_terminals(circuit, r, 1, g3, 0);
    int A = TN(x + 100, y + 20), d2a = TN(x + 120, y + 20), out = TN(x + 200, y + 20), rt = TN(x + 280, y + 20);
    TW(A, d2a); TW(out, rt);
    c1->node_ids[1] = A; d1->node_ids[1] = A; d2->node_ids[0] = d2a; d2->node_ids[1] = out; c2->node_ids[0] = out; r->node_ids[0] = rt;
    return 11;
}

// Op-amp relaxation oscillator: Schmitt trigger (beta = 0.5) charging C through R
// f = 1/(2*R*C*ln((1+beta)/(1-beta))) = 1/(2*1e-3*ln3) = 455 Hz
static int place_relaxation_osc(Circuit *circuit, float x, float y) {
    Component *u = add_comp(circuit, COMP_OPAMP, x + 200, y + 40, 0);           // -(160,20) +(160,60) out(240,40)
    if (!u) return 0;
    u->props.opamp.ideal = true;
    Component *rc = add_comp(circuit, COMP_RESISTOR, x + 200, y - 40, 0);        // (160,-40)-(240,-40)
    rc->props.resistor.resistance = 10000.0;
    Component *c = add_comp(circuit, COMP_CAPACITOR, x + 120, y + 60, 90);       // (120,20)-(120,100)
    c->props.capacitor.capacitance = 100e-9;
    Component *gc = add_comp(circuit, COMP_GROUND, x + 120, y + 120, 0);
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 300, y + 80, 90);       // (300,40)-(300,120)
    r1->props.resistor.resistance = 10000.0;
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 140, y + 160, 90);      // (140,120)-(140,200)
    r2->props.resistor.resistance = 10000.0;
    Component *g2 = add_comp(circuit, COMP_GROUND, x + 140, y + 220, 0);
    Component *kick = add_comp(circuit, COMP_PULSE_SOURCE, x + 60, y + 180, 0);  // +(60,140) -(60,220): tiny start-up nudge on C
    kick->props.pulse_source.v_low = 0.0; kick->props.pulse_source.v_high = 0.1;
    kick->props.pulse_source.pulse_width = 20e-6; kick->props.pulse_source.period = 100.0;
    Component *rk = add_comp(circuit, COMP_RESISTOR, x + 60, y + 100, 90);       // (60,60)-(60,140)
    rk->props.resistor.resistance = 100000.0;
    Component *gk = add_comp(circuit, COMP_GROUND, x + 60, y + 240, 0);          // terminal (60,220)
    add_label(circuit, x + 100, y - 100, "Relaxation Oscillator (455 Hz)");
    connect_terminals(circuit, c, 1, gc, 0);
    connect_terminals(circuit, r2, 1, g2, 0);
    connect_terminals(circuit, kick, 1, gk, 0);
    connect_terminals(circuit, rk, 1, kick, 0);
    int inv = TN(x + 160, y + 20), ct = TN(x + 120, y + 20), rkt = TN(x + 60, y + 60), rkc = TN(x + 60, y + 20);
    TW(inv, ct); TW(ct, rkc); TW(rkc, rkt);
    int out = TN(x + 240, y + 40), oc = TN(x + 260, y + 40), ou = TN(x + 260, y - 40), rcr = TN(x + 240, y - 40), rcl = TN(x + 160, y - 40);
    TW(out, oc); TW(oc, ou); TW(ou, rcr); TW(rcl, inv);
    int r1t = TN(x + 300, y + 40), r1b = TN(x + 300, y + 120), pl = TN(x + 140, y + 120), pc = TN(x + 140, y + 60), ni = TN(x + 160, y + 60);
    TW(oc, r1t); TW(r1b, pl); TW(pl, pc); TW(pc, ni);
    u->node_ids[0] = inv; u->node_ids[1] = ni; u->node_ids[2] = out;
    rc->node_ids[0] = rcl; rc->node_ids[1] = rcr; c->node_ids[0] = ct; r1->node_ids[0] = r1t; r1->node_ids[1] = r1b; r2->node_ids[0] = pl;
    rk->node_ids[0] = rkt;
    return 11;
}

// Half-wave rectifier with smoothing capacitor: DC ~ Vpk - 0.7 with ripple I/(f*C)
static int place_halfwave_filtered(Circuit *circuit, float x, float y) {
    Component *v = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 60, 0);
    if (!v) return 0;
    v->props.ac_voltage.amplitude = 10.0; v->props.ac_voltage.frequency = 60.0;
    set_amp_sweep(v, 2.0, 10.0, 1.0);
    Component *g0 = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    Component *d = add_comp(circuit, COMP_DIODE, x + 60, y + 20, 0);             // A (20,20), K (100,20)
    Component *c = add_comp(circuit, COMP_CAPACITOR, x + 100, y + 60, 90);       // (100,20)-(100,100)
    c->props.capacitor.capacitance = 100e-6;
    Component *g1 = add_comp(circuit, COMP_GROUND, x + 100, y + 120, 0);
    Component *r = add_comp(circuit, COMP_RESISTOR, x + 180, y + 60, 90);        // (180,20)-(180,100)
    r->props.resistor.resistance = 1000.0;
    Component *g2 = add_comp(circuit, COMP_GROUND, x + 180, y + 120, 0);
    add_label(circuit, x + 20, y - 40, "Half-Wave Rectifier + Smoothing Cap");
    connect_terminals(circuit, v, 1, g0, 0);
    connect_terminals(circuit, v, 0, d, 0);
    connect_terminals(circuit, c, 1, g1, 0);
    connect_terminals(circuit, r, 1, g2, 0);
    int n = TN(x + 100, y + 20), rt = TN(x + 180, y + 20);
    TW(n, rt);
    d->node_ids[1] = n; c->node_ids[0] = n; r->node_ids[0] = rt;
    return 8;
}
#undef TN
#undef TW


// ---------------------------------------------------------------------------------------
// Power-system examples (Texas / ERCOT numbers, single-phase equivalent, 60 Hz).
// Sources are peak phase-to-neutral voltages; lines are series R-L; loads are resistors
// sized from MW per phase. See docs/RESEARCH_TEXAS_GRID.md for the derivations.
// ---------------------------------------------------------------------------------------
#define TN(cx, cy) circuit_find_or_create_node(circuit, (cx), (cy), 5.0f)
#define TW(a_, b_) circuit_add_wire(circuit, (a_), (b_))

// source -> transmission line -> node -> load, all on one row: returns the load node id
static Component *add_tline(Circuit *circuit, float x, float y, int rot, double len_mi, double r, double xr, double b_us, int model) {
    Component *t = add_comp(circuit, COMP_TLINE, x, y, rot);
    t->props.tline.length_mi = len_mi; t->props.tline.r_per_mi = r; t->props.tline.x_per_mi = xr;
    t->props.tline.b_us_per_mi = b_us; t->props.tline.model = model;
    return t;
}
static int chain_line_load(Circuit *circuit, float x, float y, Component *src, double len_mi, double r, double xr, double b_us, int model,
                           double Rload, Component **line_out, Component **load_out) {
    Component *t = add_tline(circuit, x + 130, y + 20, 0, len_mi, r, xr, b_us, model);     // (90,20)-(170,20)
    Component *ld = add_comp(circuit, COMP_RESISTOR, x + 280, y + 60, 90);                // (280,20)-(280,100)
    ld->props.resistor.resistance = Rload;
    Component *g = add_comp(circuit, COMP_GROUND, x + 280, y + 120, 0);
    int sp = TN(x, y + 20), tl = TN(x + 90, y + 20), tr = TN(x + 170, y + 20), n = TN(x + 280, y + 20);
    TW(sp, tl); TW(tr, n);
    connect_terminals(circuit, ld, 1, g, 0);
    src->node_ids[0] = sp; t->node_ids[0] = tl; t->node_ids[1] = tr; ld->node_ids[0] = n;
    if (line_out) *line_out = t; if (load_out) *load_out = ld;
    return n;
}

static Component *ac_source(Circuit *circuit, float x, float y, double vpk) {
    Component *v = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 60, 0);              // +(0,20) -(0,100)
    if (!v) return NULL;
    v->props.ac_voltage.amplitude = vpk; v->props.ac_voltage.frequency = 60.0;
    Component *g = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    connect_terminals(circuit, v, 1, g, 0);
    return v;
}

// 345 kV, 100 mi twin-Drake, 600 MW at unity pf: Vpk 281.7 kV, R 6, L 145.9 mH, Rload 198.4
static int place_hv_345_line(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 281700.0); if (!v) return 0;
    chain_line_load(circuit, x, y, v, 100.0, 0.06, 0.55, 8.0, 1, 198.4, NULL, NULL);   // R-L model (click the line: model 2 = pi)
    add_label(circuit, x + 20, y - 40, "345 kV line, 100 mi, 600 MW");
    return 5;
}

// 138 kV, 30 mi single Drake, 90 MW at pf 0.9 lag; switchable 6.1 uF (per phase) cap bank
static int place_hv_138_line_var(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 112700.0); if (!v) return 0;
    Component *tl = add_tline(circuit, x + 130, y + 20, 0, 30.0, 0.13, 0.72, 6.0, 1);   // (90,20)-(170,20) single Drake
    Component *ld = add_comp(circuit, COMP_RESISTOR, x + 280, y + 60, 90);        // (280,20)-(280,100)
    ld->props.resistor.resistance = 171.5;
    Component *xl = add_comp(circuit, COMP_INDUCTOR, x + 280, y + 140, 90);       // (280,100)-(280,180)
    xl->props.inductor.inductance = 0.22;
    Component *g = add_comp(circuit, COMP_GROUND, x + 280, y + 200, 0);
    Component *sw = add_comp(circuit, COMP_SPST_SWITCH, x + 380, y + 60, 90);     // (380,20)-(380,100)
    sw->props.switch_spst.closed = false;
    Component *cb = add_comp(circuit, COMP_CAPACITOR, x + 380, y + 140, 90);     // (380,100)-(380,180)
    cb->props.capacitor.capacitance = 6.1e-6;
    Component *gc = add_comp(circuit, COMP_GROUND, x + 380, y + 200, 0);
    connect_terminals(circuit, xl, 1, g, 0);
    connect_terminals(circuit, cb, 1, gc, 0);
    int sp = TN(x, y + 20), tll = TN(x + 90, y + 20), lr = TN(x + 170, y + 20), n = TN(x + 280, y + 20), swt = TN(x + 380, y + 20);
    TW(sp, tll); TW(lr, n); TW(n, swt);
    int mid = TN(x + 280, y + 100), swb = TN(x + 380, y + 100);
    v->node_ids[0] = sp; tl->node_ids[0] = tll; tl->node_ids[1] = lr; ld->node_ids[0] = n; ld->node_ids[1] = mid; xl->node_ids[0] = mid;
    sw->node_ids[0] = swt; sw->node_ids[1] = swb; cb->node_ids[0] = swb;
    add_label(circuit, x + 20, y - 40, "138 kV line, 30 mi, 90 MW pf 0.9 - close SW for the cap bank");
    return 10;
}

// 12.47 kV feeder (7.2 kV L-N), 5 mi, 1 MW per phase: Vpk 10.18 kV, R 1.53, L 8.22 mH, Rload 51.84
static int place_mv_feeder(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 10182.0); if (!v) return 0;
    chain_line_load(circuit, x, y, v, 5.0, 0.306, 0.62, 0.0, 1, 51.84, NULL, NULL);      // 1/0 ACSR: 0.306 + j0.62 ohm/mi
    add_label(circuit, x + 20, y - 40, "12.47 kV feeder, 5 mi, 1 MW/phase");
    return 5;
}

// Pole transformer 7.2 kV -> 240 V (N = 1/30), 5 kW house load (11.52 ohm)
static int place_pole_xfmr(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 10182.0); if (!v) return 0;
    Component *t = add_comp(circuit, COMP_TRANSFORMER, x + 150, y + 20, 0);      // P1(100,0) P2(100,40) S1(200,0) S2(200,40)
    t->props.transformer.turns_ratio = 1.0 / 30.0;
    Component *gp = add_comp(circuit, COMP_GROUND, x + 100, y + 80, 0);          // terminal (100,60)
    Component *house = add_comp(circuit, COMP_RESISTOR, x + 280, y + 60, 90);    // (280,20)-(280,100)
    house->props.resistor.resistance = 11.52;
    Component *gs = add_comp(circuit, COMP_GROUND, x + 200, y + 80, 0);          // S2 (200,40) -> (200,60)
    Component *gh = add_comp(circuit, COMP_GROUND, x + 280, y + 120, 0);
    add_label(circuit, x + 20, y - 40, "Pole transformer 7.2 kV : 240 V, 5 kW house");
    int vp = TN(x, y + 20), p1 = TN(x + 100, y), c1 = TN(x + 60, y + 20), c2 = TN(x + 60, y);
    TW(vp, c1); TW(c1, c2); TW(c2, p1);
    int p2 = TN(x + 100, y + 40), gpt = TN(x + 100, y + 60);
    TW(p2, gpt);
    int s1 = TN(x + 200, y), ht = TN(x + 280, y + 20), hc = TN(x + 280, y);
    TW(s1, hc); TW(hc, ht);
    int s2 = TN(x + 200, y + 40), gst = TN(x + 200, y + 60);
    TW(s2, gst);
    connect_terminals(circuit, house, 1, gh, 0);
    t->node_ids[0] = p1; t->node_ids[1] = p2; t->node_ids[2] = s1; t->node_ids[3] = s2;
    gp->node_ids[0] = gpt; gs->node_ids[0] = gst; house->node_ids[0] = ht; v->node_ids[0] = vp;
    return 7;
}

// Generator (18 kV L-L -> 14.7 kV pk L-N) with X'' = 0.15 pu on 700 MVA, GSU 1:19.17 to 345 kV, 600 MW
static int place_gen_gsu(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 14697.0); if (!v) return 0;
    Component *xd = add_comp(circuit, COMP_INDUCTOR, x + 60, y + 20, 0);         // (20,20)-(100,20)
    xd->props.inductor.inductance = 0.184e-3;
    Component *t = add_comp(circuit, COMP_TRANSFORMER, x + 190, y + 20, 0);      // P1(140,0) P2(140,40) S1(240,0) S2(240,40)
    t->props.transformer.turns_ratio = 19.17;
    Component *gp = add_comp(circuit, COMP_GROUND, x + 140, y + 80, 0);
    Component *gs = add_comp(circuit, COMP_GROUND, x + 240, y + 80, 0);
    Component *ld = add_comp(circuit, COMP_RESISTOR, x + 320, y + 60, 90);       // (320,20)-(320,100)
    ld->props.resistor.resistance = 198.4;
    Component *gl = add_comp(circuit, COMP_GROUND, x + 320, y + 120, 0);
    add_label(circuit, x + 20, y - 40, "Generator 18 kV + GSU -> 345 kV bus, 600 MW");
    connect_terminals(circuit, v, 0, xd, 0);
    int xr = TN(x + 100, y + 20), c = TN(x + 120, y + 20), c2 = TN(x + 120, y), p1 = TN(x + 140, y);
    TW(xr, c); TW(c, c2); TW(c2, p1);
    int p2 = TN(x + 140, y + 40), gpt = TN(x + 140, y + 60); TW(p2, gpt);
    int s1 = TN(x + 240, y), lc = TN(x + 320, y), lt = TN(x + 320, y + 20); TW(s1, lc); TW(lc, lt);
    int s2 = TN(x + 240, y + 40), gst = TN(x + 240, y + 60); TW(s2, gst);
    connect_terminals(circuit, ld, 1, gl, 0);
    xd->node_ids[1] = xr; t->node_ids[0] = p1; t->node_ids[1] = p2; t->node_ids[2] = s1; t->node_ids[3] = s2;
    gp->node_ids[0] = gpt; gs->node_ids[0] = gst; ld->node_ids[0] = lt;
    return 8;
}

// Full chain: gen -> GSU -> 345 kV line -> 345/138 -> 138 kV line -> 138/12.47 -> feeder -> pole -> house
static int place_grid_chain(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 14697.0); if (!v) return 0;
    float cx = x;                   // running x of the "bus" node on row y
    int bus = TN(x, y + 20);
    int count = 2;
    // helper lambdas as macros
#define SERIES_LINE(LEN, RR, XX, BB) do { \
        Component *t_ = add_tline(circuit, cx + 80, y + 20, 0, (LEN), (RR), (XX), (BB), 1); \
        int a_ = TN(cx + 40, y + 20); TW(bus, a_); t_->node_ids[0] = a_; \
        bus = TN(cx + 120, y + 20); t_->node_ids[1] = bus; cx += 120; count += 1; } while (0)
#define XFMR(NN) do { \
        Component *t_ = add_comp(circuit, COMP_TRANSFORMER, cx + 90, y + 20, 0); t_->props.transformer.turns_ratio = (NN); \
        int p1_ = TN(cx + 40, y), c1_ = TN(cx + 20, y + 20), c2_ = TN(cx + 20, y); TW(bus, c1_); TW(c1_, c2_); TW(c2_, p1_); \
        int p2_ = TN(cx + 40, y + 40), gp_ = TN(cx + 40, y + 60); TW(p2_, gp_); \
        Component *g1_ = add_comp(circuit, COMP_GROUND, cx + 40, y + 80, 0); g1_->node_ids[0] = gp_; \
        int s1_ = TN(cx + 140, y), s2_ = TN(cx + 140, y + 40), gs_ = TN(cx + 140, y + 60); TW(s2_, gs_); \
        Component *g2_ = add_comp(circuit, COMP_GROUND, cx + 140, y + 80, 0); g2_->node_ids[0] = gs_; \
        int o1_ = TN(cx + 160, y), o2_ = TN(cx + 160, y + 20); TW(s1_, o1_); TW(o1_, o2_); \
        t_->node_ids[0] = p1_; t_->node_ids[1] = p2_; t_->node_ids[2] = s1_; t_->node_ids[3] = s2_; \
        bus = o2_; cx += 160; count += 3; } while (0)
    XFMR(19.17);                       // GSU 18 kV -> 345 kV
    SERIES_LINE(100.0, 0.06, 0.55, 8.0);   // 345 kV, 100 mi twin Drake
    XFMR(0.4);                             // 345/138 kV autotransformer
    SERIES_LINE(30.0, 0.13, 0.72, 6.0);    // 138 kV, 30 mi Drake
    XFMR(1.0 / 11.07);                     // 138 kV -> 12.47 kV (7.2 kV L-N)
    SERIES_LINE(5.0, 0.306, 0.62, 0.0);    // 5-mile feeder, 1/0 ACSR
    XFMR(1.0 / 30.0);                      // pole transformer -> 240 V
#undef SERIES_LINE
#undef XFMR
    Component *house = add_comp(circuit, COMP_RESISTOR, cx + 40, y + 60, 90);    // (cx+40,20)-(cx+40,100)
    house->props.resistor.resistance = 11.52;
    Component *gh = add_comp(circuit, COMP_GROUND, cx + 40, y + 120, 0);
    int ht = TN(cx + 40, y + 20); TW(bus, ht); house->node_ids[0] = ht;
    connect_terminals(circuit, house, 1, gh, 0);
    add_label(circuit, x + 200, y - 60, "Grid chain: generator -> 345 kV -> 138 kV -> 12.47 kV -> 240 V house");
    return count + 2;
}

// Ferranti effect: 200-mi 345 kV line as a pi section, open at the far end (10 MOhm), with a
// switchable 3.54 H shunt reactor. Receiving end rises to about +9.9 %.
static int place_ferranti_line(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 281700.0); if (!v) return 0;
    Component *tl = add_tline(circuit, x + 130, y + 20, 0, 200.0, 0.06, 0.55, 8.0, 2);   // pi model: 12 ohm + 291.8 mH, 2.12 uF each end
    Component *open_load = add_comp(circuit, COMP_RESISTOR, x + 280, y + 60, 90);       // (280,20)-(280,100)
    open_load->props.resistor.resistance = 1e7;
    Component *g3 = add_comp(circuit, COMP_GROUND, x + 280, y + 120, 0);
    Component *sw = add_comp(circuit, COMP_SPST_SWITCH, x + 380, y + 60, 90);           // (380,20)-(380,100)
    sw->props.switch_spst.closed = false;
    Component *xr = add_comp(circuit, COMP_INDUCTOR, x + 380, y + 140, 90);             // (380,100)-(380,180)
    xr->props.inductor.inductance = 3.54;
    Component *g4 = add_comp(circuit, COMP_GROUND, x + 380, y + 200, 0);
    add_label(circuit, x + 20, y - 40, "Ferranti rise: 345 kV, 200 mi pi line, open end - close SW for the shunt reactor");
    int sp = TN(x, y + 20), tll = TN(x + 90, y + 20), tlr = TN(x + 170, y + 20), ot = TN(x + 280, y + 20), swt = TN(x + 380, y + 20), swb = TN(x + 380, y + 100);
    TW(sp, tll); TW(tlr, ot); TW(ot, swt);
    connect_terminals(circuit, open_load, 1, g3, 0);
    connect_terminals(circuit, xr, 1, g4, 0);
    v->node_ids[0] = sp; tl->node_ids[0] = tll; tl->node_ids[1] = tlr; open_load->node_ids[0] = ot;
    sw->node_ids[0] = swt; sw->node_ids[1] = swb; xr->node_ids[0] = swb;
    return 8;
}

// Realism ladder: the same 138 kV, 30-mile line three ways from one source, same 90 MW load:
//   row 1 resistance only, row 2 R-L (reactance), row 3 nominal pi (adds line charging).
static int place_line_model_ladder(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 112700.0); if (!v) return 0;
    int sp = TN(x, y + 20);
    v->node_ids[0] = sp;
    const char *names[3] = { "model 0: R only", "model 1: R + jX", "model 2: pi (R, X, B)" };
    int prev = sp;
    for (int row = 0; row < 3; row++) {
        float ry = y + 20 + row * 140;                                   // bus row y
        int bl = TN(x + 60, ry);
        TW(prev, bl); prev = bl;
        Component *t = add_tline(circuit, x + 130, ry, 0, 30.0, 0.13, 0.72, 6.0, row);   // (90,ry)-(170,ry)
        Component *ld = add_comp(circuit, COMP_RESISTOR, x + 280, ry + 40, 90);          // (280,ry)-(280,ry+80)
        ld->props.resistor.resistance = 211.6;
        Component *g = add_comp(circuit, COMP_GROUND, x + 280, ry + 100, 0);
        int tl = TN(x + 90, ry), tr = TN(x + 170, ry), n = TN(x + 280, ry);
        TW(bl, tl); TW(tr, n);
        connect_terminals(circuit, ld, 1, g, 0);
        t->node_ids[0] = tl; t->node_ids[1] = tr; ld->node_ids[0] = n;
        add_label(circuit, x + 100, ry - 30, names[row]);
    }
    add_label(circuit, x + 20, y - 60, "Line model ladder: 138 kV, 30 mi, 90 MW - probe the three load buses");
    return 13;
}

// Fundamentals: a battery, the wire's own resistance and the load. V_load = V R_load / (R_wire + R_load).
static int place_dc_line_drop(Circuit *circuit, float x, float y) {
    Component *v = add_comp(circuit, COMP_DC_VOLTAGE, x, y + 60, 0);                   // +(0,20) -(0,100)
    if (!v) return 0;
    v->props.dc_voltage.voltage = 12.0;
    Component *g0 = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    Component *rw = add_comp(circuit, COMP_RESISTOR, x + 100, y + 20, 0);               // (60,20)-(140,20) wire
    rw->props.resistor.resistance = 1.0;
    Component *rl = add_comp(circuit, COMP_RESISTOR, x + 200, y + 60, 90);              // (200,20)-(200,100) load
    rl->props.resistor.resistance = 10.0;
    Component *g1 = add_comp(circuit, COMP_GROUND, x + 200, y + 120, 0);
    Component *rw2 = add_comp(circuit, COMP_RESISTOR, x + 100, y + 120, 0);             // (60,120)-(140,120) return wire (drawn, grounded both ends)
    rw2->props.resistor.resistance = 1.0;
    add_label(circuit, x + 20, y - 40, "Line drop basics: 12 V, 1 ohm wire, 10 ohm load -> 10.9 V (I = 1.09 A)");
    add_label(circuit, x + 30, y + 160, "(return conductor shown for the picture; both ends are the 0 V reference)");
    connect_terminals(circuit, v, 1, g0, 0);
    connect_terminals(circuit, rl, 1, g1, 0);
    int sp = TN(x, y + 20), wl = TN(x + 60, y + 20), wr = TN(x + 140, y + 20), lt = TN(x + 200, y + 20);
    TW(sp, wl); TW(wr, lt);
    int gl = TN(x + 60, y + 120), gr = TN(x + 140, y + 120), gnd0 = TN(x, y + 120), gnd1 = TN(x + 200, y + 120);
    TW(gnd0, gl); TW(gr, gnd1);
    v->node_ids[0] = sp; rw->node_ids[0] = wl; rw->node_ids[1] = wr; rl->node_ids[0] = lt; rw2->node_ids[0] = gl; rw2->node_ids[1] = gr;
    return 7;
}
#undef TN
#undef TW


// ---------------------------------------------------------------------------------------
// Spark-gap Tesla coil. NST (120 V -> 9 kV rms) charges C1; the gap fires near the peak and
// the primary tank rings at f1 = 1/(2 pi sqrt(L1 C1)). Coupling k = 0.2 is modelled with the
// T-equivalent: L1(1-k) series, k L1 shunt, ideal 1:sqrt(L2/L1), L2(1-k) series. The
// secondary resonates with the toroid + self capacitance. A second gap to a grounded rod is
// the streamer. See docs/RESEARCH_TEXAS_GRID.md (not Texas, but the same modelling notes).
// ---------------------------------------------------------------------------------------
#define TN(cx, cy) circuit_find_or_create_node(circuit, (cx), (cy), 5.0f)
#define TW(a_, b_) circuit_add_wire(circuit, (a_), (b_))
static int place_tesla(Circuit *circuit, float x, float y, double c1, double toroid_D, double toroid_d, double rod_gap_mm, const char *title) {
    const double L1 = 29e-6, L2 = 30e-3, k = 0.2;
    // NST -> junction A. Gap: A -> ground. Tank: A -> C1 -> R -> L1(1-k) -> nc -> k L1 -> ground,
    // so the RF loop C1-L1-gap closes on itself and the NST (281 kOhm referred) only recharges C1.
    Component *v = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 60, 0);              // +(0,20) -(0,100)
    if (!v) return 0;
    v->props.ac_voltage.amplitude = 170.0; v->props.ac_voltage.frequency = 60.0;
    Component *g0 = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    Component *rn = add_comp(circuit, COMP_RESISTOR, x + 60, y + 20, 0);          // (20,20)-(100,20) NST current limit (10 ohm -> 56 k on the HV side)
    rn->props.resistor.resistance = 10.0;                                        // 56 kOhm referred: tau = 1.4 ms into 25 nF
    Component *nst = add_comp(circuit, COMP_TRANSFORMER, x + 150, y + 20, 0);     // P1(100,0) P2(100,40) S1(200,0) S2(200,40)
    nst->props.transformer.turns_ratio = 75.0;
    Component *gp = add_comp(circuit, COMP_GROUND, x + 100, y + 80, 0);
    Component *gs = add_comp(circuit, COMP_GROUND, x + 200, y + 80, 0);
    Component *gap = add_comp(circuit, COMP_SPARK_GAP, x + 240, y + 40, 90);     // (240,0)-(240,80)
    gap->props.spark_gap.gap_mm = 3.2; gap->props.spark_gap.r_on = 1.0; gap->props.spark_gap.hold_current = 20.0; gap->props.spark_gap.quench_time = 1e-6;
    Component *ggap = add_comp(circuit, COMP_GROUND, x + 240, y + 100, 0);
    Component *c1c = add_comp(circuit, COMP_CAPACITOR, x + 300, y, 0);           // (260,0)-(340,0)
    c1c->props.capacitor.capacitance = c1;
    Component *rl = add_comp(circuit, COMP_RESISTOR, x + 400, y, 0);             // (360,0)-(440,0) primary loss
    rl->props.resistor.resistance = 2.0;
    Component *l1a = add_comp(circuit, COMP_INDUCTOR, x + 500, y, 0);            // (460,0)-(540,0)
    l1a->props.inductor.inductance = L1 * (1 - k);
    Component *lk = add_comp(circuit, COMP_INDUCTOR, x + 540, y + 40, 90);       // (540,0)-(540,80)
    lk->props.inductor.inductance = L1 * k;
    Component *gk = add_comp(circuit, COMP_GROUND, x + 540, y + 100, 0);
    Component *t = add_comp(circuit, COMP_TRANSFORMER, x + 640, y + 20, 0);      // P1(590,0) P2(590,40) S1(690,0) S2(690,40)
    t->props.transformer.turns_ratio = sqrt(L2 / L1);
    Component *gtp = add_comp(circuit, COMP_GROUND, x + 590, y + 80, 0);
    Component *gts = add_comp(circuit, COMP_GROUND, x + 690, y + 80, 0);
    Component *l2 = add_comp(circuit, COMP_INDUCTOR, x + 750, y, 0);             // (710,0)-(790,0)
    l2->props.inductor.inductance = L2 * (1 - k);
    Component *rs = add_comp(circuit, COMP_RESISTOR, x + 850, y, 0);             // (810,0)-(890,0) coil resistance
    rs->props.resistor.resistance = 50.0;
    Component *cs = add_comp(circuit, COMP_CAPACITOR, x + 890, y + 40, 90);      // (890,0)-(890,80) coil self-C
    cs->props.capacitor.capacitance = 10e-12;
    Component *gcs = add_comp(circuit, COMP_GROUND, x + 890, y + 100, 0);
    Component *top = add_comp(circuit, COMP_TOROID, x + 990, y - 40, 0);         // terminal (990,0)
    top->props.toroid.major_in = toroid_D; top->props.toroid.minor_in = toroid_d;
    Component *rod = add_comp(circuit, COMP_SPARK_GAP, x + 1090, y + 40, 90);    // (1090,0)-(1090,80) streamer to the rod
    rod->props.spark_gap.gap_mm = rod_gap_mm; rod->props.spark_gap.r_on = 2e5; rod->props.spark_gap.hold_current = 0.05; rod->props.spark_gap.quench_time = 20e-6;
    Component *grod = add_comp(circuit, COMP_GROUND, x + 1090, y + 100, 0);
    add_label(circuit, x + 300, y - 110, title);
    add_label(circuit, x + 1050, y + 120, "grounded rod");
    // wiring
    connect_terminals(circuit, v, 1, g0, 0);
    connect_terminals(circuit, v, 0, rn, 0);
    int p1 = TN(x + 100, y), rnr = TN(x + 100, y + 20); TW(rnr, p1);
    int p2 = TN(x + 100, y + 40), gpt = TN(x + 100, y + 60); TW(p2, gpt);
    int s1 = TN(x + 200, y), na = TN(x + 240, y), c1l = TN(x + 260, y); TW(s1, na); TW(na, c1l);
    int s2 = TN(x + 200, y + 40), gst = TN(x + 200, y + 60); TW(s2, gst);
    connect_terminals(circuit, gap, 1, ggap, 0);
    connect_terminals(circuit, c1c, 1, rl, 0);
    connect_terminals(circuit, rl, 1, l1a, 0);
    int nc = TN(x + 540, y), tp1 = TN(x + 590, y); TW(nc, tp1);
    connect_terminals(circuit, lk, 1, gk, 0);
    int tp2 = TN(x + 590, y + 40), gtpt = TN(x + 590, y + 60); TW(tp2, gtpt);
    int ts1 = TN(x + 690, y), l2l = TN(x + 710, y); TW(ts1, l2l);
    int ts2 = TN(x + 690, y + 40), gtst = TN(x + 690, y + 60); TW(ts2, gtst);
    connect_terminals(circuit, l2, 1, rs, 0);
    int ne = TN(x + 890, y), topt = TN(x + 990, y), rodt = TN(x + 1090, y); TW(ne, topt); TW(topt, rodt);
    connect_terminals(circuit, cs, 1, gcs, 0);
    connect_terminals(circuit, rod, 1, grod, 0);
    rn->node_ids[1] = rnr; nst->node_ids[0] = p1; nst->node_ids[1] = p2; nst->node_ids[2] = s1; nst->node_ids[3] = s2;
    gp->node_ids[0] = gpt; gs->node_ids[0] = gst; gap->node_ids[0] = na; c1c->node_ids[0] = c1l;
    l1a->node_ids[1] = nc; lk->node_ids[0] = nc; t->node_ids[0] = tp1; t->node_ids[1] = tp2; t->node_ids[2] = ts1; t->node_ids[3] = ts2;
    gtp->node_ids[0] = gtpt; gts->node_ids[0] = gtst; l2->node_ids[0] = l2l; rs->node_ids[1] = ne; cs->node_ids[0] = ne;
    top->node_ids[0] = topt; rod->node_ids[0] = rodt;
    return 25;
}
static int place_tesla_coil(Circuit *circuit, float x, float y)         { return place_tesla(circuit, x, y, 25e-9, 13.0, 4.0, 40.0, "Spark-gap Tesla coil: 4x13 in toroid, primary tuned to 186 kHz"); }
static int place_tesla_coil_big(Circuit *circuit, float x, float y)     { return place_tesla(circuit, x, y, 38e-9, 24.0, 8.0, 45.0, "Tesla coil with an 8x24 in toroid: C1 retuned to 152 kHz, rod at 45 mm"); }
static int place_tesla_coil_detuned(Circuit *circuit, float x, float y) { return place_tesla(circuit, x, y, 18e-9, 24.0, 8.0, 40.0, "DETUNED: big toroid (152 kHz) but the primary rings at 220 kHz"); }
#undef TN
#undef TW


// ---------------------------------------------------------------------------------------
// Protection & control examples (AEP-style practice, see docs/RESEARCH_AEP_PC.md). Faults
// are applied by an analog switch driven by a pulse source so every demo runs by itself:
// pre-fault -> fault -> relay decision on one scope screen.
// ---------------------------------------------------------------------------------------
#define TN(cx, cy) circuit_find_or_create_node(circuit, (cx), (cy), 5.0f)
#define TW(a_, b_) circuit_add_wire(circuit, (a_), (b_))

// horizontal analog switch at (x,y): IN (x-40,y) OUT (x+40,y) CTL (x,y+20); pulse source below it
static Component *fault_switch(Circuit *circuit, float x, float y, double delay, double width, double period) {
    Component *sw = add_comp(circuit, COMP_ANALOG_SWITCH, x, y, 0);
    sw->props.analog_switch.r_on = 0.3; sw->props.analog_switch.r_off = 1e9; sw->props.analog_switch.v_on = 2.5;
    Component *pl = add_comp(circuit, COMP_PULSE_SOURCE, x, y + 80, 0);         // +(x,y+40) -(x,y+120)
    pl->props.pulse_source.v_low = 0; pl->props.pulse_source.v_high = 5.0;
    pl->props.pulse_source.delay = delay; pl->props.pulse_source.pulse_width = width; pl->props.pulse_source.period = period;
    Component *g = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    int ctl = TN(x, y + 20), pp = TN(x, y + 40);
    TW(ctl, pp);
    sw->node_ids[2] = ctl; pl->node_ids[0] = pp;
    connect_terminals(circuit, pl, 1, g, 0);
    return sw;
}
// diode -> hold capacitor (10 uF) + bleed (100 k): peak detector from node at (x0,y) to node at (x0+160,y)
static int peak_hold(Circuit *circuit, float x0, float y, double C, double R) {
    Component *d = add_comp(circuit, COMP_DIODE, x0 + 60, y, 0);                // (x0+20,y)-(x0+100,y)
    Component *c = add_comp(circuit, COMP_CAPACITOR, x0 + 100, y + 40, 90);     // (x0+100,y)-(x0+100,y+80)
    c->props.capacitor.capacitance = C;
    Component *gc = add_comp(circuit, COMP_GROUND, x0 + 100, y + 100, 0);
    Component *r = add_comp(circuit, COMP_RESISTOR, x0 + 160, y + 40, 90);      // (x0+160,y)-(x0+160,y+80)
    r->props.resistor.resistance = R;
    Component *gr = add_comp(circuit, COMP_GROUND, x0 + 160, y + 100, 0);
    int a = TN(x0, y), da = TN(x0 + 20, y), k = TN(x0 + 100, y), rt = TN(x0 + 160, y);
    TW(a, da); TW(k, rt);
    d->node_ids[0] = da; d->node_ids[1] = k; c->node_ids[0] = k; r->node_ids[0] = rt;
    connect_terminals(circuit, c, 1, gc, 0);
    connect_terminals(circuit, r, 1, gr, 0);
    return rt;
}
// ideal op-amp comparator at (x,y): -(x-40,y-20) +(x-40,y+20) out(x+40,y); DC reference vref wired to the - input
static Component *comparator_with_ref(Circuit *circuit, float x, float y, double vref, int plus_node) {
    Component *u = add_comp(circuit, COMP_OPAMP, x, y, 0);
    u->props.opamp.ideal = false;                 // open-loop comparator: finite gain + rails (ideal = virtual short)
    u->props.opamp.gain = 1e5;
    Component *v = add_comp(circuit, COMP_DC_VOLTAGE, x + 100, y - 100, 0);      // +(x+100,y-140) -(x+100,y-60)
    v->props.dc_voltage.voltage = vref;
    Component *g = add_comp(circuit, COMP_GROUND, x + 100, y - 40, 0);           // terminal (x+100,y-60)
    int minus = TN(x - 40, y - 20), vp = TN(x + 100, y - 140), c1 = TN(x - 60, y - 140), c2 = TN(x - 60, y - 20);
    TW(vp, c1); TW(c1, c2); TW(c2, minus);
    // output load so TRIP is a real node with a wire (100 k to ground, right of the op-amp)
    Component *rl = add_comp(circuit, COMP_RESISTOR, x + 80, y + 40, 90);        // (x+80,y)-(x+80,y+80)
    rl->props.resistor.resistance = 100e3;
    Component *gl = add_comp(circuit, COMP_GROUND, x + 80, y + 100, 0);
    int out = TN(x + 40, y), lt = TN(x + 80, y);
    TW(out, lt);
    rl->node_ids[0] = lt;
    connect_terminals(circuit, rl, 1, gl, 0);
    u->node_ids[0] = minus; u->node_ids[1] = plus_node; u->node_ids[2] = out;
    v->node_ids[0] = vp;
    connect_terminals(circuit, v, 1, g, 0);
    return u;
}

// 5.1: 7.97 kV rms feeder, 13.3 ohm load (600 A), CT 600:5 into 1 ohm, rectify-hold, 8 V pickup (738 A)
static int place_pc_overcurrent(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 11270.0); if (!v) return 0;
    Component *ct = add_comp(circuit, COMP_TRANSFORMER, x + 150, y + 40, 0);     // P1(100,20) P2(100,60) S1(200,20) S2(200,60)
    ct->props.transformer.turns_ratio = 120.0;
    Component *gs2 = add_comp(circuit, COMP_GROUND, x + 200, y + 80, 0);
    Component *rb = add_comp(circuit, COMP_RESISTOR, x + 260, y + 60, 90);       // burden (260,20)-(260,100)
    rb->props.resistor.resistance = 1.0;
    Component *gb = add_comp(circuit, COMP_GROUND, x + 260, y + 120, 0);
    Component *ld = add_comp(circuit, COMP_RESISTOR, x + 200, y + 180, 90);      // load (200,140)-(200,220)
    ld->props.resistor.resistance = 13.3;
    Component *gl = add_comp(circuit, COMP_GROUND, x + 200, y + 240, 0);
    Component *rf = add_comp(circuit, COMP_RESISTOR, x + 300, y + 180, 90);      // fault R (300,140)-(300,220)
    rf->props.resistor.resistance = 5.0;
    Component *sw = fault_switch(circuit, x + 340, y + 220, 0.040, 0.060, 0.200); // IN (300,220) OUT (380,220)
    Component *gsw = add_comp(circuit, COMP_GROUND, x + 380, y + 240, 0);
    int hold = peak_hold(circuit, x + 260, y + 20, 10e-6, 10e3);                 // tau 100 ms -> (420,20)
    int plus = TN(x + 560, y + 60), c1 = TN(x + 480, y + 20), c2 = TN(x + 480, y + 60);
    TW(hold, c1); TW(c1, c2); TW(c2, plus);
    Component *u = comparator_with_ref(circuit, x + 600, y + 40, 8.0, plus);     // -(560,20) +(560,60) out(640,40)
    (void)u;
    add_label(circuit, x + 20, y - 60, "CT + 50/51 overcurrent: 600 A normal, fault 1200 A at t = 40-100 ms (repeats)");
    add_label(circuit, x + 700, y + 30, "TRIP");
    // wiring
    int sp = TN(x, y + 20), p1 = TN(x + 100, y + 20); TW(sp, p1);
    int p2 = TN(x + 100, y + 60), b0 = TN(x + 100, y + 140), bl = TN(x + 200, y + 140), br = TN(x + 300, y + 140);
    TW(p2, b0); TW(b0, bl); TW(bl, br);
    int s1 = TN(x + 200, y + 20), rbt = TN(x + 260, y + 20); TW(s1, rbt);
    int s2 = TN(x + 200, y + 60);
    int rfb = TN(x + 300, y + 220), swo = TN(x + 380, y + 220);
    v->node_ids[0] = sp; ct->node_ids[0] = p1; ct->node_ids[1] = p2; ct->node_ids[2] = s1; ct->node_ids[3] = s2;
    gs2->node_ids[0] = s2; rb->node_ids[0] = rbt; ld->node_ids[0] = bl; rf->node_ids[0] = br; rf->node_ids[1] = rfb;
    sw->node_ids[0] = rfb; sw->node_ids[1] = swo; gsw->node_ids[0] = swo;
    connect_terminals(circuit, rb, 1, gb, 0);
    connect_terminals(circuit, ld, 1, gl, 0);
    return 22;
}

// 5.2: two 120:1 CTs in opposition across a 1 ohm differential burden; internal fault at 40-100 ms,
// through fault beyond CT2 at 240-300 ms (period 400 ms)
static int place_pc_differential(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 11270.0); if (!v) return 0;
    Component *rs = add_comp(circuit, COMP_RESISTOR, x + 60, y + 20, 0);         // (20,20)-(100,20) source R
    rs->props.resistor.resistance = 1.0;
    Component *ct1 = add_comp(circuit, COMP_TRANSFORMER, x + 150, y + 40, 0);    // P1(100,20) P2(100,60) S1(200,20) S2(200,60)
    ct1->props.transformer.turns_ratio = 120.0;
    Component *ct2 = add_comp(circuit, COMP_TRANSFORMER, x + 470, y + 160, 0);   // P1(420,140) P2(420,180) S1(520,140) S2(520,180)
    ct2->props.transformer.turns_ratio = 120.0;
    Component *ld = add_comp(circuit, COMP_RESISTOR, x + 500, y + 280, 90);      // (500,240)-(500,320)
    ld->props.resistor.resistance = 20.0;
    Component *gl = add_comp(circuit, COMP_GROUND, x + 500, y + 340, 0);
    Component *rfi = add_comp(circuit, COMP_RESISTOR, x + 250, y + 180, 90);     // internal fault R (250,140)-(250,220)
    rfi->props.resistor.resistance = 2.0;
    Component *swi = fault_switch(circuit, x + 290, y + 220, 0.100, 0.060, 0.400); // IN (250,220) OUT (330,220)
    Component *gi = add_comp(circuit, COMP_GROUND, x + 330, y + 240, 0);
    Component *rfe = add_comp(circuit, COMP_RESISTOR, x + 600, y + 280, 90);     // external fault R (600,240)-(600,320)
    rfe->props.resistor.resistance = 2.0;
    Component *swe = fault_switch(circuit, x + 640, y + 320, 0.240, 0.060, 0.400); // IN (600,320) OUT (680,320)
    Component *ge = add_comp(circuit, COMP_GROUND, x + 680, y + 340, 0);
    Component *gb = add_comp(circuit, COMP_GROUND, x + 300, y + 120, 0);         // B node ground, terminal (300,100)
    Component *rd = add_comp(circuit, COMP_RESISTOR, x + 620, y + 60, 90);       // differential burden (620,20)-(620,100)
    rd->props.resistor.resistance = 1.0;
    Component *gd = add_comp(circuit, COMP_GROUND, x + 620, y + 120, 0);
    int hold = peak_hold(circuit, x + 620, y + 20, 2.2e-6, 10e3);                // tau 22 ms -> (780,20)
    int plus = TN(x + 860, y + 60), c1 = TN(x + 800, y + 20), c2 = TN(x + 800, y + 60);
    TW(hold, c1); TW(c1, c2); TW(c2, plus);
    comparator_with_ref(circuit, x + 900, y + 40, 1.0, plus);                   // -(860,20) +(860,60) out(940,40)
    add_label(circuit, x + 20, y - 60, "87 line differential: internal fault (100-160 ms) trips, through fault (240-300 ms) does not");
    add_label(circuit, x + 1000, y + 30, "TRIP");
    // primary path
    connect_terminals(circuit, v, 0, rs, 0);
    int p1 = TN(x + 100, y + 20), p2 = TN(x + 100, y + 60), z0 = TN(x + 100, y + 140), zf = TN(x + 250, y + 140), z1 = TN(x + 420, y + 140);
    TW(p2, z0); TW(z0, zf); TW(zf, z1);
    int q2 = TN(x + 420, y + 180), l0 = TN(x + 420, y + 240), lt = TN(x + 500, y + 240), et = TN(x + 600, y + 240);
    TW(q2, l0); TW(l0, lt); TW(lt, et);
    rs->node_ids[1] = p1; ct1->node_ids[0] = p1; ct1->node_ids[1] = p2; ct2->node_ids[0] = z1; ct2->node_ids[1] = q2;
    ld->node_ids[0] = lt; rfi->node_ids[0] = zf; rfe->node_ids[0] = et;
    int rfib = TN(x + 250, y + 220), swio = TN(x + 330, y + 220); rfi->node_ids[1] = rfib; swi->node_ids[0] = rfib; swi->node_ids[1] = swio; gi->node_ids[0] = swio;
    int rfeb = TN(x + 600, y + 320), sweo = TN(x + 680, y + 320); rfe->node_ids[1] = rfeb; swe->node_ids[0] = rfeb; swe->node_ids[1] = sweo; ge->node_ids[0] = sweo;
    connect_terminals(circuit, ld, 1, gl, 0);
    // secondaries: A = CT1 S1 + CT2 S2 -> R_d ; B = CT1 S2 + CT2 S1 -> ground
    int s1a = TN(x + 200, y + 20), a1 = TN(x + 560, y + 20), a2 = TN(x + 560, y + 180), s2b = TN(x + 520, y + 180), rdt = TN(x + 620, y + 20);
    TW(s1a, a1); TW(a1, a2); TW(a2, s2b); TW(a1, rdt);
    int s2a = TN(x + 200, y + 60), b1 = TN(x + 200, y + 100), b2 = TN(x + 300, y + 100), b3 = TN(x + 540, y + 100), b4 = TN(x + 540, y + 140), s1b = TN(x + 520, y + 140);
    TW(s2a, b1); TW(b1, b2); TW(b2, b3); TW(b3, b4); TW(b4, s1b);
    ct1->node_ids[2] = s1a; ct1->node_ids[3] = s2a; ct2->node_ids[2] = s1b; ct2->node_ids[3] = s2b;
    gb->node_ids[0] = b2; rd->node_ids[0] = rdt;
    connect_terminals(circuit, rd, 1, gd, 0);
    return 30;
}

// 5.3: 345 kV, 50 mi line as 20 + 30 mi, faults at 40 % (t 40-100 ms) and 100 % (t 240-300 ms);
// VT 2875:1, CT 400:1 into a 3.35 ohm replica (= 80 % reach); trip if |I Z_set| > |V|
static int place_pc_distance(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 281400.0); if (!v) return 0;
    Component *ls = add_comp(circuit, COMP_INDUCTOR, x + 60, y + 20, 0);         // (20,20)-(100,20) source X = 10 ohm
    ls->props.inductor.inductance = 26.5e-3;
    Component *ct = add_comp(circuit, COMP_TRANSFORMER, x + 150, y + 40, 0);     // P1(100,20) P2(100,60) S1(200,20) S2(200,60)
    ct->props.transformer.turns_ratio = 400.0;
    Component *gs2 = add_comp(circuit, COMP_GROUND, x + 200, y + 80, 0);
    Component *rrep = add_comp(circuit, COMP_RESISTOR, x + 320, y + 60, 90);     // replica (320,20)-(320,100)
    rrep->props.resistor.resistance = 3.35;
    Component *grep_ = add_comp(circuit, COMP_GROUND, x + 320, y + 120, 0);
    int vi = peak_hold(circuit, x + 320, y + 20, 2.2e-6, 10e3);                  // tau 22 ms -> (480,20)
    Component *vt = add_comp(circuit, COMP_TRANSFORMER, x + 150, y + 220, 180);  // rotated: P1(200,240) P2(200,200) S1(100,240) S2(100,200)
    vt->props.transformer.turns_ratio = 1.0 / 2875.0;
    Component *gvt = add_comp(circuit, COMP_GROUND, x + 200, y + 260, 0);        // P1 (200,240)
    Component *gvs = add_comp(circuit, COMP_GROUND, x + 100, y + 220, 0);        // S2 (100,200)
    Component *seg1 = add_tline(circuit, x + 340, y + 140, 0, 20.0, 0.06, 0.60, 0.0, 1);   // (300,140)-(380,140)
    Component *seg2 = add_tline(circuit, x + 520, y + 140, 0, 30.0, 0.06, 0.60, 0.0, 1);   // (480,140)-(560,140)
    Component *sw1 = fault_switch(circuit, x + 440, y + 200, 0.100, 0.060, 0.400); // IN (400,200) OUT (480,200)
    Component *g1 = add_comp(circuit, COMP_GROUND, x + 480, y + 220, 0);
    Component *sw2 = fault_switch(circuit, x + 640, y + 200, 0.240, 0.060, 0.400); // IN (600,200) OUT (680,200)
    Component *g2 = add_comp(circuit, COMP_GROUND, x + 680, y + 220, 0);
    Component *ld = add_comp(circuit, COMP_RESISTOR, x + 760, y + 180, 90);      // (760,140)-(760,220)
    ld->props.resistor.resistance = 500.0;
    Component *gl = add_comp(circuit, COMP_GROUND, x + 760, y + 240, 0);
    // VT secondary peak detector, routed below-left, then a long loop to the comparator's - input at the top
    int vv = peak_hold(circuit, x + 80, y + 320, 2.2e-6, 10e3);                  // (80,320) -> (240,320)
    int vsec = TN(x + 100, y + 240), vs1 = TN(x + 40, y + 240), vs2 = TN(x + 40, y + 320), vs3 = TN(x + 80, y + 320);
    TW(vsec, vs1); TW(vs1, vs2); TW(vs2, vs3);
    Component *u = add_comp(circuit, COMP_OPAMP, x + 640, y - 40, 0);            // -(600,-60) +(600,-20) out(680,-40)
    u->props.opamp.ideal = false; u->props.opamp.gain = 1e5;
    int plus = TN(x + 600, y - 20), i1 = TN(x + 560, y + 20), i2 = TN(x + 560, y - 20);
    TW(vi, i1); TW(i1, i2); TW(i2, plus);
    int minus = TN(x + 600, y - 60), m1 = TN(x + 240, y + 460), m2 = TN(x - 60, y + 460), m3 = TN(x - 60, y - 60);
    TW(vv, m1); TW(m1, m2); TW(m2, m3); TW(m3, minus);
    int uout = TN(x + 680, y - 40), ult = TN(x + 720, y - 40);
    TW(uout, ult);
    Component *url = add_comp(circuit, COMP_RESISTOR, x + 720, y, 90);           // (720,-40)-(720,40)
    url->props.resistor.resistance = 100e3;
    Component *ugl = add_comp(circuit, COMP_GROUND, x + 720, y + 60, 0);
    url->node_ids[0] = ult;
    connect_terminals(circuit, url, 1, ugl, 0);
    u->node_ids[0] = minus; u->node_ids[1] = plus; u->node_ids[2] = uout;
    add_label(circuit, x + 20, y - 120, "21 distance, zone 1 (80 % reach): fault at 40 % trips, fault at 100 % does not");
    add_label(circuit, x + 740, y - 60, "TRIP");
    add_label(circuit, x + 330, y - 20, "|I| x Z_set");
    add_label(circuit, x + 90, y + 290, "|V| (VT)");
    // wiring
    connect_terminals(circuit, v, 0, ls, 0);
    int p1 = TN(x + 100, y + 20), p2 = TN(x + 100, y + 60), r0 = TN(x + 100, y + 140), r1 = TN(x + 200, y + 140), r2 = TN(x + 300, y + 140);
    TW(p2, r0); TW(r0, r1); TW(r1, r2);
    int vtp2 = TN(x + 200, y + 200); TW(r1, vtp2);
    int vtp1 = TN(x + 200, y + 240), vts2 = TN(x + 100, y + 200);
    ls->node_ids[1] = p1; ct->node_ids[0] = p1; ct->node_ids[1] = p2;
    vt->node_ids[0] = vtp1; vt->node_ids[1] = vtp2; vt->node_ids[2] = vsec; vt->node_ids[3] = vts2;
    gvt->node_ids[0] = vtp1; gvs->node_ids[0] = vts2;
    int s1 = TN(x + 200, y + 20), s2 = TN(x + 200, y + 60), rt = TN(x + 320, y + 20); TW(s1, rt);
    ct->node_ids[2] = s1; ct->node_ids[3] = s2; gs2->node_ids[0] = s2; rrep->node_ids[0] = rt;
    connect_terminals(circuit, rrep, 1, grep_, 0);
    int e1 = TN(x + 380, y + 140), mid = TN(x + 400, y + 140), e2 = TN(x + 480, y + 140), f1 = TN(x + 400, y + 200);
    TW(e1, mid); TW(mid, e2); TW(mid, f1);
    seg1->node_ids[0] = r2; seg1->node_ids[1] = e1; seg2->node_ids[0] = e2;
    int e3 = TN(x + 560, y + 140), end = TN(x + 600, y + 140), f2 = TN(x + 600, y + 200), lt = TN(x + 760, y + 140);
    TW(e3, end); TW(end, f2); TW(end, lt);
    seg2->node_ids[1] = e3; ld->node_ids[0] = lt;
    int o1 = TN(x + 480, y + 200), o2 = TN(x + 680, y + 200);
    sw1->node_ids[0] = f1; sw1->node_ids[1] = o1; g1->node_ids[0] = o1;
    sw2->node_ids[0] = f2; sw2->node_ids[1] = o2; g2->node_ids[0] = o2;
    connect_terminals(circuit, ld, 1, gl, 0);
    return 34;
}

// 5.8: breaker failure: START = TRIP AND 50BF -> 150 ms timer -> BFT = timer AND 50BF
static int place_pc_breaker_fail(Circuit *circuit, float x, float y) {
    // gate inputs sit at +/-15 px, so gates are placed at y+35 / y+65: their terminals land on the 10 px grid
    Component *trip = add_comp(circuit, COMP_PULSE_SOURCE, x, y + 60, 0);        // +(0,20) -(0,100)
    if (!trip) return 0;
    trip->props.pulse_source.v_low = 0; trip->props.pulse_source.v_high = 5; trip->props.pulse_source.delay = 0.050; trip->props.pulse_source.pulse_width = 0.300; trip->props.pulse_source.period = 0.600;
    Component *g0 = add_comp(circuit, COMP_GROUND, x, y + 120, 0);
    Component *cur = add_comp(circuit, COMP_PULSE_SOURCE, x, y + 220, 0);        // +(0,180) -(0,260)
    cur->props.pulse_source.v_low = 0; cur->props.pulse_source.v_high = 5; cur->props.pulse_source.delay = 0.050; cur->props.pulse_source.pulse_width = 0.300; cur->props.pulse_source.period = 0.600;
    Component *g1 = add_comp(circuit, COMP_GROUND, x, y + 280, 0);
    Component *and1 = add_comp(circuit, COMP_AND_GATE, x + 120, y + 35, 0);      // A(80,20) B(80,50) OUT(160,35)->node (160,40)
    Component *rt = add_comp(circuit, COMP_RESISTOR, x + 220, y + 40, 0);        // (180,40)-(260,40)
    rt->props.resistor.resistance = 10e3;
    Component *ct = add_comp(circuit, COMP_CAPACITOR, x + 260, y + 80, 90);      // (260,40)-(260,120)
    ct->props.capacitor.capacitance = 15e-6;
    Component *gc = add_comp(circuit, COMP_GROUND, x + 260, y + 140, 0);
    int plus = TN(x + 360, y + 80), ctop = TN(x + 260, y + 40), c1 = TN(x + 300, y + 40), c2 = TN(x + 300, y + 80);
    TW(ctop, c1); TW(c1, c2); TW(c2, plus);
    Component *u = comparator_with_ref(circuit, x + 400, y + 60, 3.16, plus);   // -(360,40) +(360,80) out(440,60), load at (480,60..140)
    Component *and2 = add_comp(circuit, COMP_AND_GATE, x + 600, y + 65, 0);      // A(560,50) B(560,80) OUT(640,65)->node (640,70)
    Component *rl = add_comp(circuit, COMP_RESISTOR, x + 700, y + 110, 90);      // (700,70)-(700,150)
    rl->props.resistor.resistance = 100e3;
    Component *gl = add_comp(circuit, COMP_GROUND, x + 700, y + 170, 0);
    add_label(circuit, x + 20, y - 40, "50BF: TRIP at 50 ms, breaker current still present -> BFT 150 ms later (stuck breaker)");
    add_label(circuit, x + 720, y + 60, "BFT");
    add_label(circuit, x + 20, y + 310, "TRIP (top pulse) and 50BF current detector (bottom pulse); shorten the 50BF pulse to 83 ms for a healthy breaker");
    // wiring
    int tp = TN(x, y + 20), ga = TN(x + 80, y + 20); TW(tp, ga);
    int cp = TN(x, y + 180), b1 = TN(x + 60, y + 180), b2 = TN(x + 60, y + 50), gb = TN(x + 80, y + 50); TW(cp, b1); TW(b1, b2); TW(b2, gb);
    int o1 = TN(x + 160, y + 40), rl0 = TN(x + 180, y + 40); TW(o1, rl0);
    trip->node_ids[0] = tp; cur->node_ids[0] = cp;
    connect_terminals(circuit, trip, 1, g0, 0);
    connect_terminals(circuit, cur, 1, g1, 0);
    and1->node_ids[0] = ga; and1->node_ids[1] = gb; and1->node_ids[2] = o1;
    rt->node_ids[0] = rl0; rt->node_ids[1] = ctop; ct->node_ids[0] = ctop;
    connect_terminals(circuit, ct, 1, gc, 0);
    int uo = TN(x + 440, y + 60), j1 = TN(x + 520, y + 60), j2 = TN(x + 520, y + 50), a2a = TN(x + 560, y + 50);
    TW(uo, j1); TW(j1, j2); TW(j2, a2a);
    int k1 = TN(x + 540, y + 180), k2 = TN(x + 540, y + 80), a2b = TN(x + 560, y + 80);
    TW(b1, k1); TW(k1, k2); TW(k2, a2b);
    (void)u;
    int bft = TN(x + 640, y + 70), lt = TN(x + 700, y + 70); TW(bft, lt);
    and2->node_ids[0] = a2a; and2->node_ids[1] = a2b; and2->node_ids[2] = bft;
    rl->node_ids[0] = lt;
    connect_terminals(circuit, rl, 1, gl, 0);
    return 17;
}

// 5.6: 200 mi 345 kV pi line loaded at SIL (283 ohm); close SW to add a second 283 ohm -> 2 x SIL
static int place_sil_loading(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 281700.0); if (!v) return 0;
    Component *tl = add_tline(circuit, x + 130, y + 20, 0, 200.0, 0.06, 0.60, 7.5, 2);   // (90,20)-(170,20)
    Component *ld = add_comp(circuit, COMP_RESISTOR, x + 280, y + 60, 90);       // (280,20)-(280,100)
    ld->props.resistor.resistance = 283.0;
    Component *gl = add_comp(circuit, COMP_GROUND, x + 280, y + 120, 0);
    Component *sw = add_comp(circuit, COMP_SPST_SWITCH, x + 380, y + 60, 90);    // (380,20)-(380,100)
    sw->props.switch_spst.closed = false;
    Component *l2 = add_comp(circuit, COMP_RESISTOR, x + 380, y + 140, 90);      // (380,100)-(380,180)
    l2->props.resistor.resistance = 283.0;
    Component *g2 = add_comp(circuit, COMP_GROUND, x + 380, y + 200, 0);
    add_label(circuit, x + 20, y - 40, "SIL loading: 200 mi 345 kV line into Zc = 283 ohm (420 MW) - close SW for 2 x SIL");
    int sp = TN(x, y + 20), tll = TN(x + 90, y + 20), tlr = TN(x + 170, y + 20), lt = TN(x + 280, y + 20), swt = TN(x + 380, y + 20), swb = TN(x + 380, y + 100);
    TW(sp, tll); TW(tlr, lt); TW(lt, swt);
    v->node_ids[0] = sp; tl->node_ids[0] = tll; tl->node_ids[1] = tlr; ld->node_ids[0] = lt; sw->node_ids[0] = swt; sw->node_ids[1] = swb; l2->node_ids[0] = swb;
    connect_terminals(circuit, ld, 1, gl, 0);
    connect_terminals(circuit, l2, 1, g2, 0);
    return 8;
}

// 5.5: two 100 mi pi sections with a 50 % series capacitor in the middle, 2 x SIL load; SW bypasses the cap
static int place_series_comp(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 281700.0); if (!v) return 0;
    Component *t1 = add_tline(circuit, x + 130, y + 20, 0, 100.0, 0.06, 0.60, 7.5, 2);   // (90,20)-(170,20)
    Component *c = add_comp(circuit, COMP_CAPACITOR, x + 250, y + 20, 0);        // (210,20)-(290,20)
    c->props.capacitor.capacitance = 4.4210e-05;
    Component *sw = add_comp(circuit, COMP_SPST_SWITCH, x + 250, y - 40, 0);     // bypass (210,-40)-(290,-40)
    sw->props.switch_spst.closed = false;
    Component *t2 = add_tline(circuit, x + 370, y + 20, 0, 100.0, 0.06, 0.60, 7.5, 2);   // (330,20)-(410,20)
    Component *ld = add_comp(circuit, COMP_RESISTOR, x + 480, y + 60, 90);       // (480,20)-(480,100)
    ld->props.resistor.resistance = 141.5;
    Component *gl = add_comp(circuit, COMP_GROUND, x + 480, y + 120, 0);
    add_label(circuit, x + 20, y - 90, "Series compensation: 50 % of X_line in a capacitor, load 2 x SIL - close SW to bypass it");
    int sp = TN(x, y + 20), a = TN(x + 90, y + 20), b = TN(x + 170, y + 20), cl = TN(x + 210, y + 20), cr = TN(x + 290, y + 20), d = TN(x + 330, y + 20), e = TN(x + 410, y + 20), lt = TN(x + 480, y + 20);
    int swl = TN(x + 210, y - 40), swr = TN(x + 290, y - 40);
    TW(sp, a); TW(b, cl); TW(cr, d); TW(e, lt); TW(cl, swl); TW(cr, swr);
    v->node_ids[0] = sp; t1->node_ids[0] = a; t1->node_ids[1] = b; c->node_ids[0] = cl; c->node_ids[1] = cr;
    sw->node_ids[0] = swl; sw->node_ids[1] = swr; t2->node_ids[0] = d; t2->node_ids[1] = e; ld->node_ids[0] = lt;
    connect_terminals(circuit, ld, 1, gl, 0);
    return 8;
}

// AEP 765 kV: 300 mi six-conductor bundle (0.02 + j0.53 ohm/mi, 8.5 uS/mi) at SIL (250 ohm, ~2300 MW)
static int place_hv_765_line(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 624600.0); if (!v) return 0;
    chain_line_load(circuit, x, y, v, 300.0, 0.02, 0.53, 8.5, 2, 250.0, NULL, NULL);
    add_label(circuit, x + 20, y - 40, "765 kV, 300 mi, six-bundle, loaded at SIL (2340 MW)");
    return 5;
}
#undef TN
#undef TW

// Output node to probe for each template (component type, ordinal among that type, terminal)
typedef struct { ComponentType ct; int ord, term; } TemplateProbeSpec;
static const TemplateProbeSpec template_output[CIRCUIT_TYPE_COUNT] = {
    [CIRCUIT_RC_LOWPASS]       = { COMP_CAPACITOR, 0, 0 },
    [CIRCUIT_RC_HIGHPASS]      = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_RL_LOWPASS]       = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_RL_HIGHPASS]      = { COMP_INDUCTOR, 0, 0 },
    [CIRCUIT_VOLTAGE_DIVIDER]  = { COMP_RESISTOR, 1, 0 },
    [CIRCUIT_INVERTING_AMP]    = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_NONINVERTING_AMP] = { COMP_OPAMP_FLIPPED, 0, 2 },
    [CIRCUIT_VOLTAGE_FOLLOWER] = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_HALFWAVE_RECT]    = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_LED_WITH_RESISTOR]= { COMP_LED, 0, 0 },
    [CIRCUIT_COMMON_EMITTER]   = { COMP_NPN_BJT, 0, 1 },
    [CIRCUIT_COMMON_SOURCE]    = { COMP_NMOS, 0, 1 },
    [CIRCUIT_COMMON_DRAIN]     = { COMP_NMOS, 0, 2 },
    [CIRCUIT_MULTISTAGE_AMP]   = { COMP_NPN_BJT, 1, 1 },
    [CIRCUIT_DIFFERENTIAL_PAIR]= { COMP_NPN_BJT, 0, 1 },
    [CIRCUIT_CURRENT_MIRROR]   = { COMP_NPN_BJT, 1, 1 },
    [CIRCUIT_PUSH_PULL]        = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_CMOS_INVERTER]    = { COMP_NMOS, 0, 1 },
    [CIRCUIT_INTEGRATOR]       = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_DIFFERENTIATOR]   = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_SUMMING_AMP]      = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_COMPARATOR]       = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_FULLWAVE_BRIDGE]  = { COMP_CAPACITOR_ELEC, 0, 0 },
    [CIRCUIT_CENTERTAP_RECT]   = { COMP_CAPACITOR_ELEC, 0, 0 },
    [CIRCUIT_AC_DC_SUPPLY]     = { COMP_CAPACITOR_ELEC, 0, 0 },
    [CIRCUIT_AC_DC_AMERICAN]   = { COMP_CAPACITOR_ELEC, 0, 0 },
    [CIRCUIT_DIFFERENCE_AMP]   = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_TRANSIMPEDANCE]   = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_INSTR_AMP]        = { COMP_OPAMP, 2, 2 },
    [CIRCUIT_SALLEN_KEY_LP]    = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_BANDPASS_ACTIVE]  = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_NOTCH_FILTER]     = { COMP_RESISTOR, 3, 0 },
    [CIRCUIT_WIEN_OSCILLATOR]  = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_CURRENT_SOURCE]   = { COMP_NPN_BJT, 0, 1 },
    [CIRCUIT_WINDOW_COMP]      = { COMP_LED, 0, 0 },
    [CIRCUIT_HYSTERESIS_COMP]  = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_ZENER_REF]        = { COMP_ZENER, 0, 1 },
    [CIRCUIT_PRECISION_RECT]   = { COMP_OPAMP, 1, 2 },
    [CIRCUIT_7805_REG]         = { COMP_7805, 0, 1 },
    [CIRCUIT_LM317_REG]        = { COMP_LM317, 0, 1 },
    [CIRCUIT_TL431_REF]        = { COMP_TL431, 0, 0 },
    [CIRCUIT_SERIES_RLC]       = { COMP_CAPACITOR, 0, 0 },
    [CIRCUIT_PARALLEL_RLC]     = { COMP_INDUCTOR, 0, 0 },
    [CIRCUIT_WHEATSTONE]       = { COMP_RESISTOR, 3, 0 },
    [CIRCUIT_PEAK_DETECTOR]    = { COMP_CAPACITOR, 0, 0 },
    [CIRCUIT_CLAMPER]          = { COMP_DIODE, 0, 1 },
    [CIRCUIT_PHASE_SHIFT_OSC]  = { COMP_OPAMP_REAL, 0, 2 },
    [CIRCUIT_RC_BANDPASS]      = { COMP_CAPACITOR, 1, 0 },
    [CIRCUIT_LC_LOWPASS]       = { COMP_CAPACITOR, 0, 0 },
    [CIRCUIT_ZENER_CLIPPER]    = { COMP_RESISTOR, 0, 1 },
    [CIRCUIT_VOLTAGE_DOUBLER]  = { COMP_CAPACITOR, 1, 0 },
    [CIRCUIT_RELAXATION_OSC]   = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_HALFWAVE_FILTERED]= { COMP_CAPACITOR, 0, 0 },
    [CIRCUIT_HV_345_LINE]      = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_HV_138_LINE_VAR]  = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_MV_FEEDER]        = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_POLE_XFMR]        = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_GEN_GSU]          = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_GRID_CHAIN]       = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_FERRANTI_LINE]    = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_LINE_MODEL_LADDER] = { COMP_RESISTOR, 1, 0 },
    [CIRCUIT_DC_LINE_DROP]     = { COMP_RESISTOR, 1, 0 },
    [CIRCUIT_PC_OVERCURRENT]   = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_PC_DIFFERENTIAL]  = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_PC_DISTANCE]      = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_PC_BREAKER_FAIL]  = { COMP_AND_GATE, 1, 2 },
    [CIRCUIT_SIL_LOADING]      = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_SERIES_COMP]      = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_HV_765_LINE]      = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_TESLA_COIL]       = { COMP_TOROID, 0, 0 },
    [CIRCUIT_TESLA_COIL_BIG]   = { COMP_TOROID, 0, 0 },
    [CIRCUIT_TESLA_COIL_DETUNED] = { COMP_TOROID, 0, 0 },
};

// Scope time/div that shows the interesting behaviour of each template
static const double template_time_div[CIRCUIT_TYPE_COUNT] = {
    [CIRCUIT_RC_LOWPASS] = 200e-6, [CIRCUIT_RC_HIGHPASS] = 200e-6, [CIRCUIT_RL_LOWPASS] = 200e-6,
    [CIRCUIT_RL_HIGHPASS] = 200e-6, [CIRCUIT_VOLTAGE_DIVIDER] = 1e-3, [CIRCUIT_INVERTING_AMP] = 200e-6,
    [CIRCUIT_NONINVERTING_AMP] = 200e-6, [CIRCUIT_VOLTAGE_FOLLOWER] = 200e-6, [CIRCUIT_HALFWAVE_RECT] = 5e-3,
    [CIRCUIT_LED_WITH_RESISTOR] = 1e-3, [CIRCUIT_COMMON_EMITTER] = 200e-6, [CIRCUIT_COMMON_SOURCE] = 200e-6,
    [CIRCUIT_COMMON_DRAIN] = 200e-6, [CIRCUIT_MULTISTAGE_AMP] = 200e-6, [CIRCUIT_DIFFERENTIAL_PAIR] = 200e-6,
    [CIRCUIT_CURRENT_MIRROR] = 1e-3, [CIRCUIT_PUSH_PULL] = 200e-6, [CIRCUIT_CMOS_INVERTER] = 200e-6,
    [CIRCUIT_INTEGRATOR] = 2e-3, [CIRCUIT_DIFFERENTIATOR] = 2e-3, [CIRCUIT_SUMMING_AMP] = 1e-3,
    [CIRCUIT_COMPARATOR] = 2e-3, [CIRCUIT_FULLWAVE_BRIDGE] = 5e-3, [CIRCUIT_CENTERTAP_RECT] = 5e-3,
    [CIRCUIT_AC_DC_SUPPLY] = 5e-3, [CIRCUIT_AC_DC_AMERICAN] = 5e-3, [CIRCUIT_DIFFERENCE_AMP] = 200e-6,
    [CIRCUIT_TRANSIMPEDANCE] = 1e-3, [CIRCUIT_INSTR_AMP] = 200e-6, [CIRCUIT_SALLEN_KEY_LP] = 200e-6,
    [CIRCUIT_BANDPASS_ACTIVE] = 200e-6, [CIRCUIT_NOTCH_FILTER] = 5e-3, [CIRCUIT_WIEN_OSCILLATOR] = 500e-6,
    [CIRCUIT_CURRENT_SOURCE] = 1e-3, [CIRCUIT_WINDOW_COMP] = 1e-3, [CIRCUIT_HYSTERESIS_COMP] = 2e-3,
    [CIRCUIT_ZENER_REF] = 1e-3, [CIRCUIT_PRECISION_RECT] = 2e-3, [CIRCUIT_7805_REG] = 1e-3,
    [CIRCUIT_LM317_REG] = 1e-3, [CIRCUIT_TL431_REF] = 1e-3, [CIRCUIT_SERIES_RLC] = 2e-3,
    [CIRCUIT_PARALLEL_RLC] = 2e-3, [CIRCUIT_WHEATSTONE] = 1e-3, [CIRCUIT_PEAK_DETECTOR] = 50e-3,
    [CIRCUIT_CLAMPER] = 50e-3, [CIRCUIT_PHASE_SHIFT_OSC] = 50e-6,
    [CIRCUIT_RC_BANDPASS] = 200e-6, [CIRCUIT_LC_LOWPASS] = 200e-6, [CIRCUIT_ZENER_CLIPPER] = 50e-3,
    [CIRCUIT_VOLTAGE_DOUBLER] = 50e-3, [CIRCUIT_RELAXATION_OSC] = 1e-3, [CIRCUIT_HALFWAVE_FILTERED] = 50e-3,
    [CIRCUIT_HV_345_LINE] = 5e-3, [CIRCUIT_HV_138_LINE_VAR] = 5e-3, [CIRCUIT_MV_FEEDER] = 5e-3, [CIRCUIT_POLE_XFMR] = 5e-3,
    [CIRCUIT_GEN_GSU] = 5e-3, [CIRCUIT_GRID_CHAIN] = 5e-3, [CIRCUIT_FERRANTI_LINE] = 5e-3,
    [CIRCUIT_TESLA_COIL] = 10e-6, [CIRCUIT_TESLA_COIL_BIG] = 10e-6, [CIRCUIT_TESLA_COIL_DETUNED] = 10e-6,
    [CIRCUIT_LINE_MODEL_LADDER] = 5e-3, [CIRCUIT_DC_LINE_DROP] = 1e-3,
    [CIRCUIT_PC_OVERCURRENT] = 10e-3, [CIRCUIT_PC_DIFFERENTIAL] = 20e-3, [CIRCUIT_PC_DISTANCE] = 20e-3, [CIRCUIT_PC_BREAKER_FAIL] = 20e-3,
    [CIRCUIT_SIL_LOADING] = 5e-3, [CIRCUIT_SERIES_COMP] = 5e-3, [CIRCUIT_HV_765_LINE] = 5e-3,
};

// Scope volts/div preset (0 = leave as is)
static const double template_volt_div[CIRCUIT_TYPE_COUNT] = {
    [CIRCUIT_RC_LOWPASS] = 0.5, [CIRCUIT_RC_HIGHPASS] = 0.5, [CIRCUIT_RL_LOWPASS] = 0.5, [CIRCUIT_RL_HIGHPASS] = 0.5,
    [CIRCUIT_SALLEN_KEY_LP] = 0.5, [CIRCUIT_BANDPASS_ACTIVE] = 0.5, [CIRCUIT_NOTCH_FILTER] = 0.5,
    [CIRCUIT_VOLTAGE_FOLLOWER] = 0.5, [CIRCUIT_INVERTING_AMP] = 2.0, [CIRCUIT_NONINVERTING_AMP] = 2.0,
    [CIRCUIT_DIFFERENTIATOR] = 0.5, [CIRCUIT_INSTR_AMP] = 1.0, [CIRCUIT_DIFFERENCE_AMP] = 0.5,
    [CIRCUIT_PRECISION_RECT] = 0.5, [CIRCUIT_PEAK_DETECTOR] = 2.0, [CIRCUIT_CLAMPER] = 5.0,
    [CIRCUIT_COMPARATOR] = 5.0, [CIRCUIT_HYSTERESIS_COMP] = 5.0, [CIRCUIT_WIEN_OSCILLATOR] = 5.0,
    [CIRCUIT_PHASE_SHIFT_OSC] = 2.0, [CIRCUIT_CMOS_INVERTER] = 2.0, [CIRCUIT_SERIES_RLC] = 2.0,
    [CIRCUIT_PARALLEL_RLC] = 2.0, [CIRCUIT_FULLWAVE_BRIDGE] = 5.0, [CIRCUIT_AC_DC_SUPPLY] = 5.0,
    [CIRCUIT_AC_DC_AMERICAN] = 5.0, [CIRCUIT_CENTERTAP_RECT] = 2.0, [CIRCUIT_HALFWAVE_RECT] = 2.0,
    [CIRCUIT_RC_BANDPASS] = 0.5, [CIRCUIT_LC_LOWPASS] = 0.5, [CIRCUIT_ZENER_CLIPPER] = 5.0,
    [CIRCUIT_VOLTAGE_DOUBLER] = 2.0, [CIRCUIT_RELAXATION_OSC] = 5.0, [CIRCUIT_HALFWAVE_FILTERED] = 5.0,
    [CIRCUIT_HV_345_LINE] = 100e3, [CIRCUIT_HV_138_LINE_VAR] = 50e3, [CIRCUIT_MV_FEEDER] = 5e3, [CIRCUIT_POLE_XFMR] = 100.0,
    [CIRCUIT_GEN_GSU] = 100e3, [CIRCUIT_GRID_CHAIN] = 100.0, [CIRCUIT_FERRANTI_LINE] = 100e3,
    [CIRCUIT_TESLA_COIL] = 100e3, [CIRCUIT_TESLA_COIL_BIG] = 100e3, [CIRCUIT_TESLA_COIL_DETUNED] = 100e3,
    [CIRCUIT_LINE_MODEL_LADDER] = 50e3, [CIRCUIT_DC_LINE_DROP] = 5.0,
    [CIRCUIT_PC_OVERCURRENT] = 5.0, [CIRCUIT_PC_DIFFERENTIAL] = 5.0, [CIRCUIT_PC_DISTANCE] = 5.0, [CIRCUIT_PC_BREAKER_FAIL] = 2.0,
    [CIRCUIT_SIL_LOADING] = 100e3, [CIRCUIT_SERIES_COMP] = 100e3, [CIRCUIT_HV_765_LINE] = 200e3,
};

// Demonstration contract per template (see DemoKind in circuits.h)
static const TemplateDemo template_demo[CIRCUIT_TYPE_COUNT] = {
    [CIRCUIT_RC_LOWPASS]       = { DEMO_LOWPASS, 1591.5 },
    [CIRCUIT_RC_HIGHPASS]      = { DEMO_HIGHPASS, 1591.5 },
    [CIRCUIT_RL_LOWPASS]       = { DEMO_LOWPASS, 1591.5 },
    [CIRCUIT_RL_HIGHPASS]      = { DEMO_HIGHPASS, 1591.5 },
    [CIRCUIT_VOLTAGE_DIVIDER]  = { DEMO_DC, 0 },
    [CIRCUIT_INVERTING_AMP]    = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_NONINVERTING_AMP] = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_VOLTAGE_FOLLOWER] = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_HALFWAVE_RECT]    = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_LED_WITH_RESISTOR]= { DEMO_DC, 0 },
    [CIRCUIT_COMMON_EMITTER]   = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_COMMON_SOURCE]    = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_COMMON_DRAIN]     = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_MULTISTAGE_AMP]   = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_DIFFERENTIAL_PAIR]= { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_CURRENT_MIRROR]   = { DEMO_DC, 0 },
    [CIRCUIT_PUSH_PULL]        = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_CMOS_INVERTER]    = { DEMO_SWITCH, 1000 },
    [CIRCUIT_INTEGRATOR]       = { DEMO_WAVEFORM, 100 },
    [CIRCUIT_DIFFERENTIATOR]   = { DEMO_WAVEFORM, 100 },
    [CIRCUIT_SUMMING_AMP]      = { DEMO_DC, 0 },
    [CIRCUIT_COMPARATOR]       = { DEMO_SWITCH, 100 },
    [CIRCUIT_FULLWAVE_BRIDGE]  = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_CENTERTAP_RECT]   = { DEMO_DC, 0 },
    [CIRCUIT_AC_DC_SUPPLY]     = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_AC_DC_AMERICAN]   = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_DIFFERENCE_AMP]   = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_TRANSIMPEDANCE]   = { DEMO_DC, 0 },
    [CIRCUIT_INSTR_AMP]        = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_SALLEN_KEY_LP]    = { DEMO_LOWPASS, 1591.5 },
    [CIRCUIT_BANDPASS_ACTIVE]  = { DEMO_BANDPASS, 1591.5 },
    [CIRCUIT_NOTCH_FILTER]     = { DEMO_NOTCH, 60 },
    [CIRCUIT_WIEN_OSCILLATOR]  = { DEMO_OSC, 1591.5 },
    [CIRCUIT_CURRENT_SOURCE]   = { DEMO_DC, 0 },
    [CIRCUIT_WINDOW_COMP]      = { DEMO_DC, 0 },
    [CIRCUIT_HYSTERESIS_COMP]  = { DEMO_SWITCH, 100 },
    [CIRCUIT_ZENER_REF]        = { DEMO_DC, 0 },
    [CIRCUIT_PRECISION_RECT]   = { DEMO_WAVEFORM, 100 },
    [CIRCUIT_7805_REG]         = { DEMO_DC, 0 },
    [CIRCUIT_LM317_REG]        = { DEMO_DC, 0 },
    [CIRCUIT_TL431_REF]        = { DEMO_DC, 0 },
    [CIRCUIT_SERIES_RLC]       = { DEMO_BANDPASS, 159.15 },
    [CIRCUIT_PARALLEL_RLC]     = { DEMO_BANDPASS, 159.15 },
    [CIRCUIT_WHEATSTONE]       = { DEMO_DC, 0 },
    [CIRCUIT_PEAK_DETECTOR]    = { DEMO_ENVELOPE, 1000 },
    [CIRCUIT_CLAMPER]          = { DEMO_ENVELOPE, 1000 },
    [CIRCUIT_PHASE_SHIFT_OSC]  = { DEMO_OSC, 6497 },
    [CIRCUIT_RC_BANDPASS]      = { DEMO_BANDPASS, 1600 },
    [CIRCUIT_LC_LOWPASS]       = { DEMO_LOWPASS, 1591.5 },
    [CIRCUIT_ZENER_CLIPPER]    = { DEMO_LIMITER, 1000 },
    [CIRCUIT_VOLTAGE_DOUBLER]  = { DEMO_ENVELOPE, 1000 },
    [CIRCUIT_RELAXATION_OSC]   = { DEMO_OSC, 455 },
    [CIRCUIT_HALFWAVE_FILTERED]= { DEMO_ENVELOPE, 60 },
    [CIRCUIT_HV_345_LINE]      = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_HV_138_LINE_VAR]  = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_MV_FEEDER]        = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_POLE_XFMR]        = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_GEN_GSU]          = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_GRID_CHAIN]       = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_FERRANTI_LINE]    = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_TESLA_COIL]       = { DEMO_OSC, 186e3 },
    [CIRCUIT_TESLA_COIL_BIG]   = { DEMO_OSC, 152e3 },
    [CIRCUIT_TESLA_COIL_DETUNED] = { DEMO_OSC, 152e3 },
    [CIRCUIT_LINE_MODEL_LADDER] = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_DC_LINE_DROP]     = { DEMO_DC, 0 },
    [CIRCUIT_PC_OVERCURRENT]   = { DEMO_SWITCH, 30 },   // run 6/f = 200 ms: window 100-200 ms sees TRIP drop at 100 ms
    [CIRCUIT_PC_DIFFERENTIAL]  = { DEMO_SWITCH, 20 },   // 300 ms: window 150-300 sees the release after the 100-160 ms fault
    [CIRCUIT_PC_DISTANCE]      = { DEMO_SWITCH, 20 },
    [CIRCUIT_PC_BREAKER_FAIL]  = { DEMO_SWITCH, 5 },    // 1.2 s: BFT pulses at 200-350 ms and 800-950 ms
    [CIRCUIT_SIL_LOADING]      = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_SERIES_COMP]      = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_HV_765_LINE]      = { DEMO_WAVEFORM, 60 },
};

const TemplateDemo *circuit_template_demo(CircuitTemplateType type) {
    static const TemplateDemo none = { DEMO_NONE, 0 };
    if (type <= CIRCUIT_NONE || type >= CIRCUIT_TYPE_COUNT) return &none;
    return &template_demo[type];
}

double circuit_template_scope_volt_div(CircuitTemplateType type) {
    if (type <= CIRCUIT_NONE || type >= CIRCUIT_TYPE_COUNT) return 0.0;
    return template_volt_div[type];
}

bool circuit_template_output_spec(CircuitTemplateType type, ComponentType *ct, int *ord, int *term) {
    if (type <= CIRCUIT_NONE || type >= CIRCUIT_TYPE_COUNT || !template_output[type].ct) return false;
    *ct = template_output[type].ct; *ord = template_output[type].ord; *term = template_output[type].term;
    return true;
}

double circuit_template_scope_time_div(CircuitTemplateType type) {
    if (type <= CIRCUIT_NONE || type >= CIRCUIT_TYPE_COUNT) return 0.0;
    return template_time_div[type];
}

static Component *nth_of_type(Circuit *circuit, int first, ComponentType ct, int ord) {
    int k = 0;
    for (int i = first; i < circuit->num_components; i++)
        if (circuit->components[i]->type == ct) { if (k == ord) return circuit->components[i]; k++; }
    return NULL;
}

static void probe_component_terminal(Circuit *circuit, Component *c, int term) {
    if (!c || term < 0 || term >= c->num_terminals) return;
    Node *n = circuit_get_node(circuit, c->node_ids[term]);
    if (!n) return;
    for (int i = 0; i < circuit->num_probes; i++)
        if (circuit->probes[i].node_id == n->id) return;   // already probed
    circuit_add_probe(circuit, n->id, n->x, n->y);
}

int circuit_place_template(Circuit *circuit, CircuitTemplateType type, float x, float y) {
    if (!circuit) return 0;
    int first = circuit->num_components;
    int count = place_template_body(circuit, type, x, y);
    if (count <= 0 || type <= CIRCUIT_NONE || type >= CIRCUIT_TYPE_COUNT) return count;
    if (!template_notes[type][0]) return count;

    // Bounding box of what was just placed (include node positions for wire corners)
    float min_x = 1e9f, max_y = -1e9f;
    for (int i = first; i < circuit->num_components; i++) {
        Component *c = circuit->components[i];
        const ComponentTypeInfo *info = component_get_info(c->type);
        float hw = info ? info->width / 2.0f : 40.0f, hh = info ? info->height / 2.0f : 40.0f;
        if (c->x - hw < min_x) min_x = c->x - hw;
        if (c->y + hh > max_y) max_y = c->y + hh;
        for (int t = 0; t < c->num_terminals; t++) {
            Node *n = circuit_get_node(circuit, c->node_ids[t]);
            if (n && n->y > max_y) max_y = n->y;
        }
    }
    for (int i = 0; i < circuit->num_nodes; i++) {
        Node *n = &circuit->nodes[i];
        if (n->y > max_y && n->y < max_y + 200 && n->x > min_x - 200) max_y = n->y;
    }
    if (min_x > 1e8f) { min_x = x - 100; max_y = y + 200; }

    // Auto-place probes: CH1 on the input source (+ terminal), CH2 on the output node,
    // so the scope shows the circuit working the moment it is run.
    {
        static const ComponentType src_types[] = { COMP_AC_VOLTAGE, COMP_SQUARE_WAVE, COMP_TRIANGLE_WAVE,
                                                   COMP_PULSE_SOURCE, COMP_DC_CURRENT, COMP_DC_VOLTAGE };
        Component *src = NULL;
        for (unsigned k = 0; k < sizeof src_types / sizeof src_types[0] && !src; k++)
            src = nth_of_type(circuit, first, src_types[k], 0);
        const TemplateProbeSpec *spec = &template_output[type];
        Component *out = spec->ct ? nth_of_type(circuit, first, spec->ct, spec->ord) : NULL;
        bool osc = (type == CIRCUIT_WIEN_OSCILLATOR || type == CIRCUIT_PHASE_SHIFT_OSC);
        if (src && !osc) probe_component_terminal(circuit, src, 0);
        if (out) probe_component_terminal(circuit, out, spec->term);
    }

    float ty = max_y + 60.0f;
    for (int l = 0; l < 6 && template_notes[type][l]; l++) {
        Component *txt = add_comp(circuit, COMP_TEXT, min_x, ty + l * 16.0f, 0);
        if (!txt) break;
        strncpy(txt->props.text.text, template_notes[type][l], sizeof(txt->props.text.text) - 1);
        txt->props.text.font_size = 1;
        count++;
    }
    return count;
}

const char *circuit_template_group_name(TemplateGroup g) {
    static const char *names[TG_COUNT] = {
        "Basics", "Filters", "Op-amps", "Transistors", "Oscillators", "Power supplies", "Digital", "Power systems", "High voltage"
    };
    return (g >= 0 && g < TG_COUNT) ? names[g] : "?";
}
