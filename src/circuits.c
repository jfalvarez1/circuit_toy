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
#include <stdlib.h>
#include <string.h>
#include "circuits.h"
#include "label.h"   /* label_wrap: the notes are laid out at the width they are drawn */
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
    [CIRCUIT_3PH_Y_BALANCED] = {"3-Phase Y Balanced", "3phY", "Three 120-degree sources, Y load: neutral carries nothing", TG_POWER_SYSTEMS},
    [CIRCUIT_3PH_UNBALANCED] = {"3-Phase Unbalanced", "3phUn", "Unequal Y loads: neutral current and neutral shift", TG_POWER_SYSTEMS},
    [CIRCUIT_3PH_345_LINE] = {"3-Phase 345 kV Line", "3ph345", "Three per-phase lines from the 345 kV example", TG_POWER_SYSTEMS},
    [CIRCUIT_3PH_RECTIFIER] = {"3-Phase 6-Pulse Rect", "6Pulse", "Three-phase diode bridge: 360 Hz ripple, 1.35 x V_LL", TG_POWER_SYSTEMS},
    [CIRCUIT_SCHMITT_BISTABLE] = {"Bistable (Schmitt)", "Schmit", "Inverting op-amp bistable: +/-7.5 V thresholds, hysteresis loop", TG_OSCILLATORS},
    [CIRCUIT_TRI_SQUARE_GEN] = {"Triangle/Square Gen", "TriSq", "Bistable + integrator: 5 kHz triangle and square", TG_OSCILLATORS},
    [CIRCUIT_FUNCTION_GEN] = {"Function Generator", "FuncGn", "Triangle -> 3-breakpoint diode shaper -> sine; R sets f, thresholds set A", TG_OSCILLATORS},
    [CIRCUIT_COLPITTS] = {"Colpitts (MOSFET)", "Colpit", "LC tank C1-C2 capacitive divider, 712 kHz", TG_OSCILLATORS},
    [CIRCUIT_RING_OSC] = {"Ring Oscillator", "Ring", "Five inverters with RC delay stages, ~145 kHz", TG_OSCILLATORS},
    [CIRCUIT_HARTLEY] = {"Hartley (MOSFET)", "Hartly", "Tapped-inductor tank L1 + L2 with C: 503 kHz", TG_OSCILLATORS},
    [CIRCUIT_CLAPP] = {"Clapp (MOSFET)", "Clapp", "Colpitts with a small series cap setting f: 1.744 MHz", TG_OSCILLATORS},
    [CIRCUIT_THEVENIN] = {"Thevenin Equivalent", "Thev", "Divider + series R seen by a load: Vth 6 V, Rth 2.2 k", TG_BASICS},
    [CIRCUIT_SUPERPOSITION] = {"Superposition", "Super", "Two voltage sources + a current source: responses add", TG_BASICS},
    [CIRCUIT_RC_STEP] = {"RC Step Response", "RCstp", "63 % at one time constant, 10-90 % rise = 2.2 tau", TG_TRANSIENTS},
    [CIRCUIT_RL_STEP] = {"RL Step Response", "RLstp", "Inductor current rises with tau = L/R", TG_TRANSIENTS},
    [CIRCUIT_RLC_RING] = {"RLC Step (Ringing)", "RLCst", "Underdamped series RLC: 90 % overshoot, 199 us period", TG_TRANSIENTS},
    [CIRCUIT_RLC_DAMPING] = {"RLC Damping Ladder", "Damp", "Same L, C with R = 20 / 632 / 2000: under, critical, over", TG_TRANSIENTS},
    [CIRCUIT_OPAMP_SAT] = {"Op-Amp Saturation", "Sat", "Gain -10 clips at the rails; the virtual short is lost", TG_OPAMPS},
    [CIRCUIT_SINGLE_TUNED_AMP] = {"Single-Tuned Amplifier", "Tuned", "CE stage with an LC tank load: gain peaks at f0 = 100 kHz", TG_TRANSISTORS},
    [CIRCUIT_COMMON_BASE] = {"Common Base", "CB", "Non-inverting, low input resistance, gain g_m R_C", TG_TRANSISTORS},
    [CIRCUIT_DARLINGTON] = {"Darlington Follower", "Darl", "beta^2 input resistance: a 100k source still drives 100 ohm", TG_TRANSISTORS},
    [CIRCUIT_SR_LATCH] = {"SR Latch (NOR)", "SRlat", "Cross-coupled NOR gates remember S and R pulses", TG_DIGITAL},
    [CIRCUIT_POWER_PLANT] = {"Power Plant (3-phase)", "Plant", "3-phase generator, GSU bank, breakers, 345 kV line, load", TG_POWER_SYSTEMS},
    [CIRCUIT_SUBSTATION] = {"Transmission Substation", "Substn", "345 kV lines, breakers, 345/138 autos, feeders, cap banks", TG_POWER_SYSTEMS},
    [CIRCUIT_IO_PUSH_PULL] = {"Push-Pull Output", "PPout", "CMOS totem-pole GPIO: PMOS sources, NMOS sinks, 1 MHz into 20 pF", TG_IC_IO},
    [CIRCUIT_IO_OPEN_DRAIN] = {"Open-Drain + Pull-up", "OD", "Pin only pulls low; 4.7k pull-up makes the slow RC rise", TG_IC_IO},
    [CIRCUIT_IO_OPEN_COLLECTOR] = {"Open-Collector Level Shift", "OC", "3.3 V logic drives a 5 V line through an NPN (inverting)", TG_IC_IO},
    [CIRCUIT_IO_I2C_BUS] = {"I2C Bus (wired-AND)", "I2C", "Master and slave open-drain on one SDA, 4.7k / 200 pF", TG_IC_IO},
    [CIRCUIT_IO_I2C_LEVEL] = {"I2C Level Shifter", "I2Clv", "One NMOS, gate at 3.3 V, pull-ups both sides: 3.3 V <-> 5 V", TG_IC_IO},
    [CIRCUIT_IO_INPUT_DEBOUNCE] = {"GPIO Input + Debounce", "Btn", "Pull-up, button to ground, RC debounce, inverter", TG_IC_IO},
    [CIRCUIT_IO_LOW_SIDE] = {"Low-side Switch + Flyback", "LoSw", "NMOS sinks a relay coil; flyback diode clamps the spike", TG_IC_IO},
    [CIRCUIT_IO_HIGH_SIDE] = {"High-side PMOS Switch", "HiSw", "3.3 V logic -> NPN -> PMOS gate: load switched from the 12 V rail", TG_IC_IO},
    [CIRCUIT_IO_SPI] = {"SPI Lines", "SPI", "SCLK 10 MHz / MOSI 5 MHz, 33 ohm series termination, 200 pF cable", TG_IC_IO},
    [CIRCUIT_IO_UART] = {"UART 5 V <-> 3.3 V", "UART", "Divider one way, direct the other way (TTL V_IH = 2 V)", TG_IC_IO},
    [CIRCUIT_IO_RS485] = {"RS-485 Differential Link", "RS485", "A/B antiphase, 120 ohm both ends, common-mode noise rejected", TG_IC_IO},
    [CIRCUIT_IO_SPMI] = {"SPMI Bus (1.8 V)", "SPMI", "MIPI two-wire: 1.8 V SCLK 5 MHz + SDATA, 33 ohm into 15 pF", TG_IC_IO},
    [CIRCUIT_TX_69KV] = {"69 kV Subtransmission", "69kV", "AEP Texas 69 kV: 20 mi 336 ACSR into 20 MVA at 0.95 pf", TG_POWER_SYSTEMS},
    [CIRCUIT_TX_LADDER] = {"Texas Voltage Ladder", "TXLad", "345 / 138 / 69 / 12.47 kV and the 240 V service in one canvas", TG_POWER_SYSTEMS},
    [CIRCUIT_TX_WIND] = {"CREZ Wind Collector", "Wind", "34.5 kV strings -> collector -> 345 kV GSU -> ERCOT grid", TG_POWER_SYSTEMS},
    [CIRCUIT_TX_PLANT] = {"13.8 kV Industrial Service", "13k8", "13.8/4.16 kV plant transformer, motor bus, 480 V shop", TG_POWER_SYSTEMS},
    [CIRCUIT_RES_SERVICE] = {"240/120 V Service", "Split", "Centre-tapped pole transformer, unbalanced legs, neutral", TG_BUILDING},
    [CIRCUIT_RES_BRANCH] = {"120 V Branch Circuits", "Branch", "#14 vs #10, 100 ft, 12 A: the NEC 3 % voltage drop", TG_BUILDING},
    [CIRCUIT_RES_ACSTART] = {"AC Compressor Start", "ACstart", "LRA 104 A on a weak service: a 10 % flicker dip", TG_BUILDING},
    [CIRCUIT_RES_SOLAR] = {"Rooftop Solar Backfeed", "Solar", "7.6 kW export raises the PCC (IEEE 1547 / C84.1)", TG_BUILDING},
    [CIRCUIT_COM_480Y] = {"480Y/277 V Service", "480Y", "3-phase 30 hp motor plus 277 V lighting", TG_BUILDING},
    [CIRCUIT_COM_208Y] = {"208Y/120 V Panel", "208Y", "Unbalanced 20/12/6 A and the shared neutral (NEC 220.61)", TG_BUILDING},
    [CIRCUIT_COM_PFC] = {"Power Factor Correction", "PFC", "0.75 -> 0.95 pf with a switched 478 uF bank", TG_BUILDING},
    [CIRCUIT_COM_ATS] = {"Standby Generator Transfer", "ATS", "Utility drops, the generator picks the load up (NEC 700)", TG_BUILDING},
    [CIRCUIT_GS_N1] = {"N-1 Contingency", "N-1", "Two 345 kV circuits: open one and watch the P0 envelope break", TG_GRID_STD},
    [CIRCUIT_GS_IBR] = {"IBR Ride-Through", "IBR", "PRC-029-1 / NOGRR-245: a 150 ms fault at the POI", TG_GRID_STD},
    [CIRCUIT_GS_BOLD] = {"AEP BOLD vs Conventional", "BOLD", "Compact phasing lowers Zc and raises SIL by 62 %", TG_GRID_STD},
    [CIRCUIT_GS_DERATE] = {"Extreme Temperature Derating", "Derate", "TPL-008-1: conductor R rises with the Tmp slider", TG_GRID_STD},
    [CIRCUIT_GS_FACRATE] = {"Facility Rating (limiting element)", "FacRt", "FAC-008-5: the CT, not the conductor, sets the rating", TG_GRID_STD},
    [CIRCUIT_GS_KRON] = {"Kron Reduction (Y to delta)", "Kron", "Eliminating an interior bus leaves the boundary identical", TG_GRID_STD},
    [CIRCUIT_GS_RX] = {"R/X Ratio and Decoupling", "R/X", "Why fast decoupled power flow diverges on feeders", TG_GRID_STD},
    [CIRCUIT_GS_GOVERNOR] = {"Governor Droop & Swing Equation", "Gov", "BAL-001-TRE-2 frequency nadir on an op-amp patch", TG_GRID_STD},
    [CIRCUIT_GS_PIDS] = {"Supervised Alarm Loop", "PIDS", "CIP-014-2: four states on one pair into the RTU", TG_GRID_STD},
    [CIRCUIT_MOS_IDVGS] = {"MOSFET Transfer Curves", "IdVgs", "One gate ramp, three devices: Vth and kn compared", TG_TRANSISTORS},
    [CIRCUIT_MOS_IDVDS] = {"MOSFET Output Curves", "IdVds", "Drain sweep at three gate voltages: triode to saturation", TG_TRANSISTORS},
    [CIRCUIT_MOS_TUNED] = {"MOSFET Tuned Amplifier", "MTund", "Common-source stage with the same 100 kHz LC tank", TG_TRANSISTORS},
    [CIRCUIT_MOS_CG] = {"Common Gate (MOSFET)", "CG", "Signal into the source, output in phase, low R_in", TG_TRANSISTORS},
    [CIRCUIT_MOS_CASCODE] = {"Cascode (MOSFET)", "Casc", "CS under CG: high gain, almost no Miller effect", TG_TRANSISTORS},
    [CIRCUIT_MOS_DIFF] = {"MOSFET Differential Pair", "MDiff", "Tail resistor sets the current, gates steer it", TG_TRANSISTORS},
    [CIRCUIT_MOS_MIRROR] = {"MOSFET Current Mirror", "MMirr", "Diode-connected reference copied by a matched device", TG_TRANSISTORS},
    [CIRCUIT_CMOS_INV] = {"CMOS Inverter (VTC)", "CMOSi", "Sweep the gates and read the transfer characteristic", TG_DIGITAL},
    [CIRCUIT_CMOS_NAND] = {"CMOS NAND (transistor level)", "CMOSn", "PMOS in parallel, NMOS in series", TG_DIGITAL},
    [CIRCUIT_CMOS_TGATE] = {"Transmission Gate", "TGate", "Complementary pair vs a lone NMOS pass transistor", TG_DIGITAL},
    [CIRCUIT_XY_LISSAJOUS] = {"Lissajous Figures", "Lissa", "Two sines into X-Y: the figure counts the frequency ratio", TG_BASICS},
    [CIRCUIT_XY_PLOTTER] = {"X-Y Plotter (upload)", "XYplt", "Replay a file of coordinates through two arb sources", TG_BASICS},
    [CIRCUIT_HW_BUCK] = {"Buck Converter", "Buck", "Vout = D Vin: 12 V to 6 V at 100 kHz", TG_HARDWARE},
    [CIRCUIT_HW_BOOST] = {"Boost Converter", "Boost", "Vout = Vin/(1-D): 5 V to 10 V", TG_HARDWARE},
    [CIRCUIT_HW_BUCKBOOST] = {"Buck-Boost Converter", "BuckB", "Inverted output, above or below the input", TG_HARDWARE},
    [CIRCUIT_HW_CUK] = {"Cuk Converter", "Cuk", "Capacitive transfer: both currents continuous", TG_HARDWARE},
    [CIRCUIT_HW_INTERLEAVED] = {"Two-Phase Interleaved Buck", "2Ph", "180 deg phases cancel ripple (the CLVR idea)", TG_HARDWARE},
    [CIRCUIT_HW_PDN] = {"Power Delivery Network", "PDN", "Bulk, ceramic and plane inductance against a load step", TG_HARDWARE},
    [CIRCUIT_HW_CAPS] = {"Input vs Output Capacitance", "Ccomp", "Same cap, two places, very different result", TG_HARDWARE},
    [CIRCUIT_HW_MATCH] = {"Impedance Matching", "Zmatch", "5 / 50 / 500 ohm on a 50 ohm source", TG_HARDWARE},
    [CIRCUIT_HW_REFLECT] = {"Signal Reflections", "Refl", "Artificial 50 ohm line, terminated or not", TG_HARDWARE},
    [CIRCUIT_HW_LOOP] = {"Loop Stability & Phase Margin", "Loop", "The same stage with and without compensation", TG_HARDWARE},
    [CIRCUIT_ID_SOURCE] = {"Ideal vs Real Source", "IdSrc", "Internal resistance: the terminal voltage sags", TG_IDEAL},
    [CIRCUIT_ID_DIODE] = {"Ideal vs Real Diode", "IdDio", "0.7 V brick wall against the Shockley knee", TG_IDEAL},
    [CIRCUIT_ID_CAP] = {"Ideal vs Real Capacitor", "IdCap", "ESR turns the ripple triangle into a square step", TG_IDEAL},
    [CIRCUIT_ID_IND] = {"Ideal vs Real Inductor", "IdInd", "Winding resistance damps the ring", TG_IDEAL},
    [CIRCUIT_ID_OPAMP] = {"Ideal vs Real Op-Amp", "IdOA", "Gain-bandwidth and slew rate against infinity", TG_IDEAL},
    [CIRCUIT_ID_BJT] = {"Ideal vs Real BJT", "IdBJT", "The Early effect moves the operating point", TG_IDEAL},
    [CIRCUIT_ID_MOSFET] = {"Ideal vs Real MOSFET", "IdMOS", "Channel-length modulation is not a rounding error", TG_IDEAL},
    [CIRCUIT_ID_OPAMP_ERR] = {"Op-Amp Error Sources", "OAerr", "Offset and bias current at DC, and how to cancel them", TG_IDEAL},
    [CIRCUIT_PARTS_MOSFET] = {"Named Parts: MOSFET Switches", "Parts", "2N7000 / 2N7002 / IRF540N doing the same job", TG_IDEAL},
    [CIRCUIT_CAP_DCBIAS] = {"Ceramic DC Bias", "Cbias", "The same 10 uF X5R at 0, 2 and 5 V of bias", TG_IDEAL},
    [CIRCUIT_NE555_ASTABLE] = {"555 Astable", "555", "The 555 as a block, built from its own comparators and latch", TG_OSCILLATORS},
    [CIRCUIT_PIERCE] = {"Pierce Crystal Oscillator", "Pierce", "A real quartz model: it only oscillates between fs and fp", TG_OSCILLATORS},
    [CIRCUIT_IV_PROBE_COMP] = {"Probe Compensation", "PrbCmp", "Under, correct and over, on the same 1 kHz square", TG_IV_MEAS},
    [CIRCUIT_IV_PROBE_LOADING] = {"Probe Loading (1x vs 10x)", "PrbLd", "The probe is part of the circuit you are measuring", TG_IV_MEAS},
    [CIRCUIT_IV_GROUND_LEAD] = {"Ground Lead Ringing", "GndLd", "6 inch clip vs spring tip: 119 MHz of ring that is not real", TG_IV_MEAS},
    [CIRCUIT_IV_SCOPE_INPUT_Z] = {"Scope Input: 1 M vs 50 ohm", "InpZ", "The unterminated cable reads twice the amplitude", TG_IV_MEAS},
    [CIRCUIT_IV_AC_COUPLING] = {"AC Coupling: 200 mV on 12 V", "ACcpl", "Finding ripple you cannot see at 5 V/div", TG_IV_MEAS},
    [CIRCUIT_IV_SHUNT_SENSE] = {"Current Sense: High vs Low Side", "Isense", "Burden voltage, ground lift and common mode", TG_IV_MEAS},
    [CIRCUIT_IV_KELVIN] = {"4-Wire (Kelvin) Sensing", "Kelvin", "10 mohm read as 110 mohm, and how to fix it", TG_IV_MEAS},
    [CIRCUIT_IV_BUCK_NODES] = {"Discrete Buck, Node by Node", "BuckN", "Real PMOS and gate drive: what every node does", TG_IV_POWER},
    [CIRCUIT_IV_LDO_VS_BUCK] = {"LDO vs Switcher", "LDOsw", "12 V to 5 V at 1 A: 7 W of heat, or 0.5 W", TG_IV_POWER},
    [CIRCUIT_IV_BOOTSTRAP] = {"Bootstrap High-Side Drive", "Boot", "Why an N-channel high side cannot run at 100 %", TG_IV_POWER},
    [CIRCUIT_IV_TERMINATION] = {"Termination: none / series / parallel", "Term", "One line, three ways of ending it", TG_IV_SI},
    [CIRCUIT_IV_PULLUP_SIZING] = {"Pull-up Sizing", "PullUp", "10k / 4.7k / 1k against 400 pF of bus", TG_IV_SI},
    [CIRCUIT_IV_GROUND_BOUNCE] = {"Ground Bounce", "Bounce", "A shared return lifts a quiet pin by a volt", TG_IV_SI},
    [CIRCUIT_IV_CROSSTALK] = {"Crosstalk", "Xtalk", "Same coupled charge, two victim impedances", TG_IV_SI},
    [CIRCUIT_IV_ESD_CLAMP] = {"ESD Clamp Diodes", "ESD", "6 V into a 3.3 V pin, through 1 k and through 220 k", TG_IV_SI},
    [CIRCUIT_IV_CAP_ENERGY] = {"The Two-Capacitor Problem", "CapE", "Half the energy leaves, whatever the resistance", TG_IV_FUND},
    [CIRCUIT_IV_MILLER] = {"The Miller Effect", "Miller", "10 pF of C_gd becomes 110 pF at the input", TG_IV_FUND},
    [CIRCUIT_IV_SWITCH_CHOICE] = {"BJT or MOSFET as a Switch", "SwSel", "V_CE(sat) against R_DS(on), and what each costs", TG_IV_FUND},
    [CIRCUIT_IV_INRUSH] = {"Hot-Plug Inrush", "Inrush", "An empty capacitor is a short circuit", TG_IV_FUND},
    [CIRCUIT_TLINE_REAL] = {"Transmission Line (real delay)", "TLdly", "One 5 ns cable, three terminations, actual propagation", TG_HARDWARE},
    [CIRCUIT_SEVENSEG_TEST] = {"7-Segment Segment Test", "7Seg", "Every segment on its own switch", TG_DIGITAL},
    [CIRCUIT_WIRELESS_LINK] = {"Wireless Link (TX/RX)", "Wless", "Antenna pair on a shared channel, 50 ohm both ends", TG_IC_IO},
    [CIRCUIT_BCD_COUNTER] = {"BCD Counter to 7-Segment", "Count", "Clock, decade counter, decoder, digit", TG_DIGITAL},
    [CIRCUIT_DIGITAL_CLOCK] = {"Digital Clock (HH:MM:SS)", "Clock", "Six digits, carry chained, reset at 24", TG_DIGITAL},






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
/* Defined further down with the other layout helpers; the amplifier builders above the
   definition need it, so it is declared here. */
static void cap_output_load(Circuit *circuit, Component *cap, double r_load);

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
    Component *label = add_comp(circuit, COMP_TEXT, x + 60, y - 160, 0);
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

    cap_output_load(circuit, cout, 100e3);   /* the far side of the output cap is the output */
    return 16;
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

    cap_output_load(circuit, cout, 100e3);
    return 16;
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

    cap_output_load(circuit, cout, 100e3);
    return 15;
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
    Component *label = add_comp(circuit, COMP_TEXT, x + 100, y - 180, 0);
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

    cap_output_load(circuit, c3, 100e3);   /* stage 2's output cap had nothing on its far side */
    return 25;
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
    Component *label = add_comp(circuit, COMP_TEXT, x - 160, y - 220, 0);
    if (!label) return 0;
    strncpy(label->props.text.text, "Differential Pair", sizeof(label->props.text.text)-1);
    label->props.text.font_size = 2;

    // VCC power supply - off to the right, so its return can run down the outside edge
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x + 280, y - 100, 0);
    vcc->props.dc_voltage.voltage = 12.0;

    // Collector resistors - RC1 for Q1, RC2 for Q2
    Component *rc1 = add_comp(circuit, COMP_RESISTOR, x + 60, y - 40, 90);
    rc1->props.resistor.resistance = 4700.0;
    Component *rc2 = add_comp(circuit, COMP_RESISTOR, x + 220, y - 40, 90);
    rc2->props.resistor.resistance = 4700.0;

    // NPN transistors - both upright, so both read the same way: base left, collector at the
    // top, emitter at the bottom. Q2 used to be placed at 180 degrees to face the second input
    // source, but rotating an NPN by 180 turns it upside down - collector at the bottom and
    // emitter at the top - which forced RC2's wire to run down past Q2's own emitter terminal.
    Component *q1 = add_comp(circuit, COMP_NPN_BJT, x + 40, y + 40, 0);
    q1->props.bjt.bf = 100;
    Component *q2 = add_comp(circuit, COMP_NPN_BJT, x + 200, y + 40, 0);
    q2->props.bjt.bf = 100;

    // Tail resistor RE - centred between the two emitters, below both bodies
    Component *re = add_comp(circuit, COMP_RESISTOR, x + 140, y + 120, 90);
    re->props.resistor.resistance = 10000.0;

    // Ground on the bottom rail
    Component *gnd = add_comp(circuit, COMP_GROUND, x + 40, y + 240, 0);

    // Base resistors - horizontal orientation. The pair is direct-coupled: the input
    // sources carry a 6 V DC offset that biases both bases (single-supply, tail to GND),
    // so Ve ~ 5.3 V, Itail ~ 0.53 mA and each collector sits near 10.8 V.
    // (A capacitor-coupled input would leave the bases with no DC path and both BJTs off.)
    // Both inputs enter from the left now that both transistors face the same way: Q1's on the
    // base row, Q2's on a clear row above the collector resistors, dropping in between them.
    Component *cin1 = add_comp(circuit, COMP_RESISTOR, x - 60, y + 40, 0);
    cin1->props.resistor.resistance = 1000.0;
    Component *cin2 = add_comp(circuit, COMP_RESISTOR, x - 60, y - 110, 0);
    cin2->props.resistor.resistance = 1000.0;

    // AC input sources - DC-biased at 6 V, anti-phase
    Component *vin1 = add_comp(circuit, COMP_AC_VOLTAGE, x - 160, y + 80, 0);
    vin1->props.ac_voltage.amplitude = 0.01;   // 10 mV each, antiphase: 20 mV differential keeps the pair linear (tanh saturates past ~4 V_T)
    vin1->props.ac_voltage.frequency = 1000.0;
    vin1->props.ac_voltage.offset = 6.0;
    Component *vin2 = add_comp(circuit, COMP_AC_VOLTAGE, x - 260, y - 70, 0);
    vin2->props.ac_voltage.amplitude = 0.01;
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

    // VCC- runs down the right-hand edge and joins the bottom rail at RE's column. The rail is
    // drawn as a chain of segments between the columns that land on it, never as several
    // full-width wires stacked on top of each other.
    float gnd_rail_y = gnd_y;
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vcc_neg_x, vcc_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, vcc_neg_x, gnd_rail_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vcc_neg_x, gnd_rail_y, 5.0f),
                     circuit_find_or_create_node(circuit, re_bot_x, gnd_rail_y, 5.0f));
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
    // The bus runs along RE's own top terminal row. Putting it a fixed 15 px below the
    // emitters instead dropped it inside RE's body, so both emitter wires arrived by
    // running horizontally through the resistor.
    float emitter_bus_y = re_top_y;

    // Tail node at RE top - the emitters meet on it directly
    int tail_node = circuit_find_or_create_node(circuit, re_top_x, re_top_y, 5.0f);
    re->node_ids[0] = tail_node;
    int emitter_bus_node = tail_node;

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

    // No drop needed: the bus already is RE's top terminal.

    // RE bottom down to the rail, then left to the ground symbol
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, re_bot_x, re_bot_y, 5.0f),
                     circuit_find_or_create_node(circuit, re_bot_x, gnd_rail_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, re_bot_x, gnd_rail_y, 5.0f), gnd_node);
    re->node_ids[1] = gnd_node;

    // === INPUT COUPLING CAPACITOR TO BASE CONNECTIONS ===
    // Cin1 output to Q1 base - horizontal wire
    int base1_node = circuit_find_or_create_node(circuit, base1_x, base1_y, 5.0f);
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cin1_out_x, cin1_out_y, 5.0f),
                     circuit_find_or_create_node(circuit, base1_x, cin1_out_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, base1_x, cin1_out_y, 5.0f), base1_node);
    cin1->node_ids[1] = base1_node;
    q1->node_ids[0] = base1_node;

    // Cin2 output to Q2 base - across above the collector resistors, then down the clear
    // column between the two transistors and in on the base row.
    int base2_node = circuit_find_or_create_node(circuit, base2_x, base2_y, 5.0f);
    float drop_x = base1_x + 80.0f;   // between Q1's body and Q2's
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, cin2_out_x, cin2_out_y, 5.0f),
                     circuit_find_or_create_node(circuit, drop_x, cin2_out_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, drop_x, cin2_out_y, 5.0f),
                     circuit_find_or_create_node(circuit, drop_x, base2_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, drop_x, base2_y, 5.0f), base2_node);
    cin2->node_ids[1] = base2_node;
    q2->node_ids[0] = base2_node;

    // === VIN1 TO CIN1 CONNECTION ===
    // Each source sits on its base resistor's own row, so the feed is a single wire
    int vin1_node = circuit_find_or_create_node(circuit, vin1_pos_x, vin1_pos_y, 5.0f);
    vin1->node_ids[0] = vin1_node;
    circuit_add_wire(circuit, vin1_node, circuit_find_or_create_node(circuit, cin1_in_x, cin1_in_y, 5.0f));
    cin1->node_ids[0] = vin1_node;

    // Vin1- down to the bottom rail, then right to the ground symbol
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vin1_neg_x, vin1_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, vin1_neg_x, gnd_rail_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vin1_neg_x, gnd_rail_y, 5.0f), gnd_node);
    vin1->node_ids[1] = gnd_node;

    // === VIN2 TO CIN2 CONNECTION ===
    int vin2_node = circuit_find_or_create_node(circuit, vin2_pos_x, vin2_pos_y, 5.0f);
    vin2->node_ids[0] = vin2_node;
    circuit_add_wire(circuit, vin2_node, circuit_find_or_create_node(circuit, cin2_in_x, cin2_in_y, 5.0f));
    cin2->node_ids[0] = vin2_node;

    // Vin2- down the outside and along the rail as far as Vin1's column
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vin2_neg_x, vin2_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, vin2_neg_x, gnd_rail_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vin2_neg_x, gnd_rail_y, 5.0f),
                     circuit_find_or_create_node(circuit, vin1_neg_x, gnd_rail_y, 5.0f));
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

    Component *label = add_comp(circuit, COMP_TEXT, x + 60, y - 180, 0);
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

    Component *label = add_comp(circuit, COMP_TEXT, x + 20, y - 160, 0);
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

    Component *label = add_comp(circuit, COMP_TEXT, x - 40, y - 180, 0);
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

    Component *label = add_comp(circuit, COMP_TEXT, x + 20, y - 160, 0);
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

    // Vcc- to ground. The three returns (Vcc-, R2, Vin-) hand off along the rail from left to
    // right instead of each drawing its own full-width wire to the ground symbol - otherwise
    // three wires lie on top of one another and a single crossing gets reported three times.
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vcc_neg_x, vcc_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, vcc_neg_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vcc_neg_x, gnd_y, 5.0f),
                     circuit_find_or_create_node(circuit, r2_bot_x, gnd_y, 5.0f));
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

    // Vin to - input. The straight shot along vin_pos_y is the divider's own row: it ran
    // clean through R2's body. Go out to the left of the source, under the divider, and up
    // into the inverting pin between the divider and the op-amp.
    float in_left   = vin_pos_x - 60.0f;
    float in_bus_y  = gnd_y + 40.0f;      // under the ground rail, which nothing else reaches
    float in_up_x   = r2_bot_x + 20.0f;   // between the divider column and the ground symbol
    int vin_node = circuit_find_or_create_node(circuit, vin_pos_x, vin_pos_y, 5.0f);
    vin->node_ids[0] = vin_node;
    circuit_add_wire(circuit, vin_node, circuit_find_or_create_node(circuit, in_left, vin_pos_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, in_left, vin_pos_y, 5.0f),
                     circuit_find_or_create_node(circuit, in_left, in_bus_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, in_left, in_bus_y, 5.0f),
                     circuit_find_or_create_node(circuit, in_up_x, in_bus_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, in_up_x, in_bus_y, 5.0f),
                     circuit_find_or_create_node(circuit, in_up_x, inv_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, in_up_x, inv_y, 5.0f),
                     circuit_find_or_create_node(circuit, inv_x, inv_y, 5.0f));
    opamp->node_ids[0] = vin_node;

    // Vin- to ground - joins the rail at Vcc-'s column, not all the way across
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vin_neg_x, vin_neg_y, 5.0f),
                     circuit_find_or_create_node(circuit, vin_neg_x, gnd_y, 5.0f));
    circuit_add_wire(circuit, circuit_find_or_create_node(circuit, vin_neg_x, gnd_y, 5.0f),
                     circuit_find_or_create_node(circuit, vcc_neg_x, gnd_y, 5.0f));
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
    Component *trans = add_comp(circuit, COMP_TRANSFORMER_CT, x + 160, y + 50, 0);
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
    rload->props.resistor.resistance = 100.0; rload->props.resistor.power_rating = 5.0;   // ~2.5 W at 16 V: 5 W load resistor

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
    rload->props.resistor.resistance = 100.0; rload->props.resistor.power_rating = 5.0;   // ~2.5 W at 16 V: 5 W load resistor

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

    int vcc_rail = circuit_find_or_create_node(circuit, vcc_pos_x, y - 160, 5.0f);   /* above the source body, not across it */
    int vcc_node = circuit_find_or_create_node(circuit, vcc_pos_x, vcc_pos_y, 5.0f);
    circuit_add_wire(circuit, vcc_node, vcc_rail);
    vcc->node_ids[0] = vcc_node;

    int cdec_top_node = circuit_find_or_create_node(circuit, cdec_top_x, cdec_top_y, 5.0f);
    int corner_cdec = circuit_find_or_create_node(circuit, cdec_top_x, y - 160, 5.0f);
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
    /* turn at R1's column on the source's own row: turning at (v1_pos_x, r1_left_y) puts
       the corner inside the source symbol, and the wire out of it runs through the body */
    int v1_corner = circuit_find_or_create_node(circuit, r1_left_x, v1_pos_y, 5.0f);
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

    int vcc_rail = circuit_find_or_create_node(circuit, vcc_pos_x, y - 160, 5.0f);   /* above the source body, not across it */
    int vcc_node = circuit_find_or_create_node(circuit, vcc_pos_x, vcc_pos_y, 5.0f);
    circuit_add_wire(circuit, vcc_node, vcc_rail);
    vcc->node_ids[0] = vcc_node;

    int cdec_top_node = circuit_find_or_create_node(circuit, cdec_top_x, cdec_top_y, 5.0f);
    int corner_cdec = circuit_find_or_create_node(circuit, cdec_top_x, y - 160, 5.0f);
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

    int vcc_rail = circuit_find_or_create_node(circuit, vcc_pos_x, y - 160, 5.0f);   /* above the source body, not across it */
    int vcc_node = circuit_find_or_create_node(circuit, vcc_pos_x, vcc_pos_y, 5.0f);
    circuit_add_wire(circuit, vcc_node, vcc_rail);
    vcc->node_ids[0] = vcc_node;

    int cdec_top_node = circuit_find_or_create_node(circuit, cdec_top_x, cdec_top_y, 5.0f);
    int corner_cdec = circuit_find_or_create_node(circuit, cdec_top_x, y - 160, 5.0f);
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
    /* Hangs below R2's row rather than straddling it, so the wire from R2 to the op-amp's +
       input passes over clear air instead of through the capacitor. */
    Component *c2 = add_comp(circuit, COMP_CAPACITOR, x + 320, y + 120, 90);
    c2->props.capacitor.capacitance = 10e-9;
    Component *gnd2 = add_comp(circuit, COMP_GROUND, x + 320, y + 180, 0);
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
    int vsrc_corner = circuit_find_or_create_node(circuit, r1_left_x, vsrc_pos_y, 5.0f);
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

    /* R1 and C1 overlap in x - C1's left terminal is above R1's body - so neither a turn at
       R1's column (through C1) nor one at C1's column (through R1) works. Step up out of R1's
       row first, then across, then up into the terminal. */
    float step_y = (r1_right_y + c1_left_y) / 2 + 5.0f;
    int step_a = circuit_find_or_create_node(circuit, r1_right_x, step_y, 5.0f);
    int step_b = circuit_find_or_create_node(circuit, c1_left_x, step_y, 5.0f);
    int c1_left_node = circuit_find_or_create_node(circuit, c1_left_x, c1_left_y, 5.0f);
    circuit_add_wire(circuit, junc1, step_a);
    circuit_add_wire(circuit, step_a, step_b);
    circuit_add_wire(circuit, step_b, c1_left_node);
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
    /* Along C2's top terminal row and then across, rather than turning at (c2_x, r2_y) - that
       corner sits between C2's plates, so the wire left the capacitor through its own side. */
    int c2_top_node = circuit_find_or_create_node(circuit, c2_top_x, c2_top_y, 5.0f);
    int c2_corner = circuit_find_or_create_node(circuit, r2_right_x, c2_top_y, 5.0f);
    circuit_add_wire(circuit, junc2, c2_corner);
    circuit_add_wire(circuit, c2_corner, c2_top_node);
    c2->node_ids[0] = c2_top_node;

    /* To the op-amp's + input, straight along R2's own row. This used to turn at C2's column
       and travel the last stretch at C2's terminal height, which put it through the capacitor;
       both branches leave R2's junction instead, and C2 hangs below the row. */
    int opamp_noninv_node = circuit_find_or_create_node(circuit, opamp_noninv_x, opamp_noninv_y, 5.0f);
    int noninv_corner = circuit_find_or_create_node(circuit, r2_right_x, opamp_noninv_y, 5.0f);
    circuit_add_wire(circuit, junc2, noninv_corner);
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

    int vcc_rail = circuit_find_or_create_node(circuit, vcc_pos_x, y - 160, 5.0f);   /* above the source body, not across it */
    int vcc_node = circuit_find_or_create_node(circuit, vcc_pos_x, vcc_pos_y, 5.0f);
    wire_ortho(circuit, vcc_node, vcc_rail);
    vcc->node_ids[0] = vcc_node;

    int cdec_top_node = circuit_find_or_create_node(circuit, cdec_top_x, cdec_top_y, 5.0f);
    int corner_cdec = circuit_find_or_create_node(circuit, cdec_top_x, y - 160, 5.0f);
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
    int vcc_rail = circuit_find_or_create_node(circuit, vcc_pos_x, y - 160, 5.0f);   /* above the source body, not across it */
    circuit_add_wire(circuit, vcc_node, vcc_rail);
    vcc->node_ids[0] = vcc_node;

    int cdec_top_node = circuit_find_or_create_node(circuit, cdec_top_x, cdec_top_y, 5.0f);
    int corner_vcc = circuit_find_or_create_node(circuit, cdec_top_x, y - 160, 5.0f);
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
    Component *c_dec = add_comp(circuit, COMP_CAPACITOR, x + 40, y - 140, 90);
    c_dec->props.capacitor.capacitance = 0.1e-6;  // 0.1uF decoupling

    Component *gnd_dec = add_comp(circuit, COMP_GROUND, x + 40, y - 80, 0);

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

    int vcc_rail = circuit_find_or_create_node(circuit, vcc_pos_x, y - 160, 5.0f);   /* above the source body, not across it */
    int vcc_node = circuit_find_or_create_node(circuit, vcc_pos_x, vcc_pos_y, 5.0f);
    circuit_add_wire(circuit, vcc_node, vcc_rail);
    vcc->node_ids[0] = vcc_node;

    // Decoupling cap to power rail
    int cdec_top_node = circuit_find_or_create_node(circuit, cdec_top_x, cdec_top_y, 5.0f);
    int corner_dec = circuit_find_or_create_node(circuit, cdec_top_x, y - 160, 5.0f);
    circuit_add_wire(circuit, vcc_rail, corner_dec);
    circuit_add_wire(circuit, corner_dec, cdec_top_node);
    c_dec->node_ids[0] = cdec_top_node;

    int r1_top_node = circuit_find_or_create_node(circuit, r1_top_x, r1_top_y, 5.0f);
    int corner1 = circuit_find_or_create_node(circuit, r1_top_x, y - 160, 5.0f);
    circuit_add_wire(circuit, corner_dec, corner1);
    circuit_add_wire(circuit, corner1, r1_top_node);
    r1->node_ids[0] = r1_top_node;

    int rload_top_node = circuit_find_or_create_node(circuit, rload_top_x, rload_top_y, 5.0f);
    int corner2 = circuit_find_or_create_node(circuit, rload_top_x, y - 160, 5.0f);
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

    /* The rail runs at y-200, clear above the top of R1. At y-160 it was inside R1's body -
       the divider's top resistor reaches up to y-174 - so the rail arrived in the middle of it
       instead of at its terminal, and everything else that used that row went through it too. */
    int vref_node = circuit_find_or_create_node(circuit, vref_pos_x, vref_pos_y, 5.0f);
    int vref_rail = circuit_find_or_create_node(circuit, vref_pos_x, y - 200, 5.0f);
    circuit_add_wire(circuit, vref_node, vref_rail);
    vref->node_ids[0] = vref_node;

    // Decoupling cap to rail
    int cdec_node = circuit_find_or_create_node(circuit, cdec_top_x, cdec_top_y, 5.0f);
    int corner_dec = circuit_find_or_create_node(circuit, cdec_top_x, y - 200, 5.0f);
    circuit_add_wire(circuit, vref_rail, corner_dec);
    circuit_add_wire(circuit, corner_dec, cdec_node);
    c_dec->node_ids[0] = cdec_node;

    // R1 to rail
    int r1_top_node = circuit_find_or_create_node(circuit, r1_top_x, r1_top_y, 5.0f);
    int corner_r1 = circuit_find_or_create_node(circuit, r1_top_x, y - 200, 5.0f);
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

    /* Down and around to both comparator inputs. Going straight across meant crossing the
       reference divider at the height of its own resistors, and the reference supply with it -
       the monitored input ran through both of them. */
    int in_down  = circuit_find_or_create_node(circuit, x + 60, y + 140, 5.0f);
    int in_right = circuit_find_or_create_node(circuit, x + 220, y + 140, 5.0f);
    int in_lo    = circuit_find_or_create_node(circuit, x + 220, comp_lo_noninv_y, 5.0f);
    int in_hi    = circuit_find_or_create_node(circuit, x + 220, comp_hi_inv_y, 5.0f);
    circuit_add_wire(circuit, input_junc, in_down);
    circuit_add_wire(circuit, in_down, in_right);
    circuit_add_wire(circuit, in_right, in_lo);
    circuit_add_wire(circuit, in_lo, circuit_find_or_create_node(circuit, comp_lo_noninv_x, comp_lo_noninv_y, 5.0f));
    circuit_add_wire(circuit, in_lo, in_hi);
    circuit_add_wire(circuit, in_hi, circuit_find_or_create_node(circuit, comp_hi_inv_x, comp_hi_inv_y, 5.0f));

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

    /* Up to the rail and along it, joining at the divider's column. Straight across at the
       pull-up's own height went through R1. */
    int rpu_top_node = circuit_find_or_create_node(circuit, rpu_top_x, rpu_top_y, 5.0f);
    int rpu_rail = circuit_find_or_create_node(circuit, rpu_top_x, y - 200, 5.0f);
    circuit_add_wire(circuit, rpu_top_node, rpu_rail);
    circuit_add_wire(circuit, rpu_rail, corner_r1);
    rpu->node_ids[0] = rpu_top_node;

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
    int vcc_rail = circuit_find_or_create_node(circuit, vcc_pos_x, y - 160, 5.0f);   /* above the source body, not across it */
    circuit_add_wire(circuit, vcc_node, vcc_rail);
    vcc->node_ids[0] = vcc_node;

    int cdec_top_node = circuit_find_or_create_node(circuit, cdec_top_x, cdec_top_y, 5.0f);
    int corner_vcc = circuit_find_or_create_node(circuit, cdec_top_x, y - 160, 5.0f);
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

    /* === REFERENCE DIVIDER, out on the left ===
       It used to stand at x+180, which is between the input resistor and the op-amp - the one
       lane both the input wire and the feedback wire have to use to reach the pins. Both ran
       straight through its two resistors. Out here the only thing it meets is a single crossing
       with the non-inverting column, which is what a reference wire crossing a signal looks
       like on paper too. */
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x - 80, y - 40, 90);  // Top resistor
    r1->props.resistor.resistance = 10000.0;

    Component *r2 = add_comp(circuit, COMP_RESISTOR, x - 80, y + 40, 90);  // Bottom resistor
    r2->props.resistor.resistance = 10000.0;

    Component *gnd_ref = add_comp(circuit, COMP_GROUND, x - 80, y + 100, 0);
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
    int corner_pwr = circuit_find_or_create_node(circuit, r1_top_x, y - 160, 5.0f);
    /* from the supply end of the rail, not the decoupling end: the divider is on the far side
       now, and running from corner_vcc would lay this wire on top of the rail it already has */
    circuit_add_wire(circuit, vcc_rail, corner_pwr);
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

    /* Out along the divider's own junction first, then up the op-amp's column. Turning at
       the divider instead - (r1_bot_x, opamp_inv_y) - put this wire inside both resistors. */
    int opamp_inv_node = circuit_find_or_create_node(circuit, opamp_inv_x, opamp_inv_y, 5.0f);
    int corner_inv = circuit_find_or_create_node(circuit, opamp_inv_x, r1_bot_y, 5.0f);
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
    rload->props.resistor.resistance = 50.0; rload->props.resistor.power_rating = 1.0;    // 0.5 W at 5 V / 100 mA: 1 W part

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

    /* R2 sits a body-length below R1, not overlapping it: at y + 110 the two symbols shared
       twenty pixels and the wires to the ADJ junction ran across R2's body. */
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 140, y + 140, 90);
    r2->props.resistor.resistance = 720.0;

    // Output filter capacitor
    Component *cout = add_comp(circuit, COMP_CAPACITOR, x + 200, y + 30, 90);
    cout->props.capacitor.capacitance = 1.0e-6;  // 1uF

    // Load resistor
    Component *rload = add_comp(circuit, COMP_RESISTOR, x + 260, y + 30, 90);
    rload->props.resistor.resistance = 100.0;

    Component *gnd_load = add_comp(circuit, COMP_GROUND, x + 200, y + 210, 0);

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
    float gnd_y = y + 200;   /* below R2, not through the middle of it */
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
    int corner_top_left = circuit_find_or_create_node(circuit, r1_left_x, vsrc_pos_y, 5.0f);
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
    Component *gnd1 = add_comp(circuit, COMP_GROUND, x, grid_y2 + 60, 0);   // 40 px lower: clear of the source body

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
    /* Gain 33 against the 29 a phase-shift loop needs to start. Nothing controls the amplitude
       after that - no lamp, no JFET, no diode limiter - so the oscillation grows until the rails
       stop it, and the steady state is a lightly clipped sine. That is what this circuit does as
       drawn; a limiter across Rf is what would make it a clean one. */
    rf->props.resistor.resistance = 33000.0;

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

static int place_io_push_pull(Circuit *circuit, float x, float y);
static int place_io_open_drain(Circuit *circuit, float x, float y);
static int place_io_open_collector(Circuit *circuit, float x, float y);
static int place_io_i2c_bus(Circuit *circuit, float x, float y);
static int place_io_i2c_level(Circuit *circuit, float x, float y);
static int place_io_input_debounce(Circuit *circuit, float x, float y);
static int place_io_low_side(Circuit *circuit, float x, float y);
static int place_io_high_side(Circuit *circuit, float x, float y);
static int place_io_spi(Circuit *circuit, float x, float y);
static int place_io_uart(Circuit *circuit, float x, float y);
static int place_io_rs485(Circuit *circuit, float x, float y);
static int place_io_spmi(Circuit *circuit, float x, float y);
static int place_tx_69kv(Circuit *circuit, float x, float y);
static int place_tx_ladder(Circuit *circuit, float x, float y);
static int place_tx_wind(Circuit *circuit, float x, float y);
static int place_tx_plant(Circuit *circuit, float x, float y);
static int place_res_service(Circuit *circuit, float x, float y);
static int place_res_branch(Circuit *circuit, float x, float y);
static int place_res_acstart(Circuit *circuit, float x, float y);
static int place_res_solar(Circuit *circuit, float x, float y);
static int place_com_480y(Circuit *circuit, float x, float y);
static int place_com_208y(Circuit *circuit, float x, float y);
static int place_com_pfc(Circuit *circuit, float x, float y);
static int place_com_ats(Circuit *circuit, float x, float y);
static int place_gs_n1(Circuit *circuit, float x, float y);
static int place_gs_ibr(Circuit *circuit, float x, float y);
static int place_gs_bold(Circuit *circuit, float x, float y);
static int place_gs_derate(Circuit *circuit, float x, float y);
static int place_gs_facrate(Circuit *circuit, float x, float y);
static int place_gs_kron(Circuit *circuit, float x, float y);
static int place_gs_rx(Circuit *circuit, float x, float y);
static int place_gs_governor(Circuit *circuit, float x, float y);
static int place_gs_pids(Circuit *circuit, float x, float y);
static int place_mos_idvgs(Circuit *circuit, float x, float y);
static int place_mos_idvds(Circuit *circuit, float x, float y);
static int place_mos_tuned(Circuit *circuit, float x, float y);
static int place_mos_cg(Circuit *circuit, float x, float y);
static int place_mos_cascode(Circuit *circuit, float x, float y);
static int place_mos_diff(Circuit *circuit, float x, float y);
static int place_mos_mirror(Circuit *circuit, float x, float y);
static int place_cmos_inv(Circuit *circuit, float x, float y);
static int place_cmos_nand(Circuit *circuit, float x, float y);
static int place_cmos_tgate(Circuit *circuit, float x, float y);
static int place_xy_lissajous(Circuit *circuit, float x, float y);
static int place_xy_plotter(Circuit *circuit, float x, float y);
static int place_hw_buck(Circuit *circuit, float x, float y);
static int place_hw_boost(Circuit *circuit, float x, float y);
static int place_hw_buckboost(Circuit *circuit, float x, float y);
static int place_hw_cuk(Circuit *circuit, float x, float y);
static int place_hw_interleaved(Circuit *circuit, float x, float y);
static int place_hw_pdn(Circuit *circuit, float x, float y);
static int place_hw_caps(Circuit *circuit, float x, float y);
static int place_hw_match(Circuit *circuit, float x, float y);
static int place_hw_reflect(Circuit *circuit, float x, float y);
static int place_hw_loop(Circuit *circuit, float x, float y);
static int place_id_source(Circuit *circuit, float x, float y);
static int place_id_diode(Circuit *circuit, float x, float y);
static int place_id_cap(Circuit *circuit, float x, float y);
static int place_id_ind(Circuit *circuit, float x, float y);
static int place_id_opamp(Circuit *circuit, float x, float y);
static int place_id_bjt(Circuit *circuit, float x, float y);
static int place_id_mosfet(Circuit *circuit, float x, float y);
static int place_id_opamp_err(Circuit *circuit, float x, float y);
static int place_parts_mosfet(Circuit *circuit, float x, float y);
static int place_cap_dcbias(Circuit *circuit, float x, float y);
static int place_ne555_astable(Circuit *circuit, float x, float y);
static int place_tesla_coil(Circuit *circuit, float x, float y);
static int place_tesla_coil_big(Circuit *circuit, float x, float y);
/* Every builder is declared before the dispatch switch below uses it. Without these the
   compiler declares them implicitly (MSVC C4013) and cannot check a single argument. */
static int place_iv_probe_comp(Circuit *circuit, float x, float y);
static int place_iv_probe_loading(Circuit *circuit, float x, float y);
static int place_iv_ground_lead(Circuit *circuit, float x, float y);
static int place_iv_scope_input_z(Circuit *circuit, float x, float y);
static int place_iv_ac_coupling(Circuit *circuit, float x, float y);
static int place_iv_shunt_sense(Circuit *circuit, float x, float y);
static int place_iv_kelvin(Circuit *circuit, float x, float y);
static int place_iv_buck_nodes(Circuit *circuit, float x, float y);
static int place_iv_ldo_vs_buck(Circuit *circuit, float x, float y);
static int place_iv_bootstrap(Circuit *circuit, float x, float y);
static int place_iv_termination(Circuit *circuit, float x, float y);
static int place_iv_pullup_sizing(Circuit *circuit, float x, float y);
static int place_iv_ground_bounce(Circuit *circuit, float x, float y);
static int place_iv_crosstalk(Circuit *circuit, float x, float y);
static int place_iv_esd_clamp(Circuit *circuit, float x, float y);
static int place_iv_cap_energy(Circuit *circuit, float x, float y);
static int place_iv_miller(Circuit *circuit, float x, float y);
static int place_iv_switch_choice(Circuit *circuit, float x, float y);
static int place_iv_inrush(Circuit *circuit, float x, float y);
static int place_tline_real(Circuit *circuit, float x, float y);
static int place_sevenseg_test(Circuit *circuit, float x, float y);
static int place_wireless_link(Circuit *circuit, float x, float y);
static int place_bcd_counter(Circuit *circuit, float x, float y);
static int place_pierce(Circuit *circuit, float x, float y);
static int place_digital_clock(Circuit *circuit, float x, float y);
static int place_3ph_345_line(Circuit *circuit, float x, float y);
static int place_3ph_rectifier(Circuit *circuit, float x, float y);
static int place_3ph_unbalanced(Circuit *circuit, float x, float y);
static int place_3ph_y_balanced(Circuit *circuit, float x, float y);
static int place_7805_reg(Circuit *circuit, float x, float y);
static int place_ac_dc_american(Circuit *circuit, float x, float y);
static int place_ac_dc_supply(Circuit *circuit, float x, float y);
static int place_bandpass_active(Circuit *circuit, float x, float y);
static int place_centertap_rectifier(Circuit *circuit, float x, float y);
static int place_clamper(Circuit *circuit, float x, float y);
static int place_clapp(Circuit *circuit, float x, float y);
static int place_cmos_inverter(Circuit *circuit, float x, float y);
static int place_colpitts(Circuit *circuit, float x, float y);
static int place_common_base(Circuit *circuit, float x, float y);
static int place_common_drain(Circuit *circuit, float x, float y);
static int place_common_emitter(Circuit *circuit, float x, float y);
static int place_common_source(Circuit *circuit, float x, float y);
static int place_comparator(Circuit *circuit, float x, float y);
static int place_current_mirror(Circuit *circuit, float x, float y);
static int place_current_source(Circuit *circuit, float x, float y);
static int place_darlington(Circuit *circuit, float x, float y);
static int place_dc_line_drop(Circuit *circuit, float x, float y);
static int place_difference_amp(Circuit *circuit, float x, float y);
static int place_differential_pair(Circuit *circuit, float x, float y);
static int place_differentiator(Circuit *circuit, float x, float y);
static int place_ferranti_line(Circuit *circuit, float x, float y);
static int place_fullwave_bridge(Circuit *circuit, float x, float y);
static int place_function_gen(Circuit *circuit, float x, float y);
static int place_gen_gsu(Circuit *circuit, float x, float y);
static int place_grid_chain(Circuit *circuit, float x, float y);
static int place_halfwave_filtered(Circuit *circuit, float x, float y);
static int place_halfwave_rectifier(Circuit *circuit, float x, float y);
static int place_hartley(Circuit *circuit, float x, float y);
static int place_hv_138_line_var(Circuit *circuit, float x, float y);
static int place_hv_345_line(Circuit *circuit, float x, float y);
static int place_hv_765_line(Circuit *circuit, float x, float y);
static int place_hysteresis_comp(Circuit *circuit, float x, float y);
static int place_instr_amp(Circuit *circuit, float x, float y);
static int place_integrator(Circuit *circuit, float x, float y);
static int place_inverting_amp(Circuit *circuit, float x, float y);
static int place_lc_lowpass(Circuit *circuit, float x, float y);
static int place_led_with_resistor(Circuit *circuit, float x, float y);
static int place_line_model_ladder(Circuit *circuit, float x, float y);
static int place_lm317_reg(Circuit *circuit, float x, float y);
static int place_multistage_amp(Circuit *circuit, float x, float y);
static int place_mv_feeder(Circuit *circuit, float x, float y);
static int place_noninverting_amp(Circuit *circuit, float x, float y);
static int place_notch_filter(Circuit *circuit, float x, float y);
static int place_opamp_sat(Circuit *circuit, float x, float y);
static int place_parallel_rlc(Circuit *circuit, float x, float y);
static int place_pc_breaker_fail(Circuit *circuit, float x, float y);
static int place_pc_differential(Circuit *circuit, float x, float y);
static int place_pc_distance(Circuit *circuit, float x, float y);
static int place_pc_overcurrent(Circuit *circuit, float x, float y);
static int place_peak_detector(Circuit *circuit, float x, float y);
static int place_phase_shift_osc(Circuit *circuit, float x, float y);
static int place_pole_xfmr(Circuit *circuit, float x, float y);
static int place_power_plant(Circuit *circuit, float x, float y);
static int place_precision_rect(Circuit *circuit, float x, float y);
static int place_push_pull(Circuit *circuit, float x, float y);
static int place_rc_bandpass(Circuit *circuit, float x, float y);
static int place_rc_highpass(Circuit *circuit, float x, float y);
static int place_rc_lowpass(Circuit *circuit, float x, float y);
static int place_rc_step(Circuit *circuit, float x, float y);
static int place_relaxation_osc(Circuit *circuit, float x, float y);
static int place_ring_osc(Circuit *circuit, float x, float y);
static int place_rl_highpass(Circuit *circuit, float x, float y);
static int place_rl_lowpass(Circuit *circuit, float x, float y);
static int place_rl_step(Circuit *circuit, float x, float y);
static int place_rlc_damping(Circuit *circuit, float x, float y);
static int place_rlc_ring(Circuit *circuit, float x, float y);
static int place_sallen_key_lp(Circuit *circuit, float x, float y);
static int place_schmitt_bistable(Circuit *circuit, float x, float y);
static int place_series_comp(Circuit *circuit, float x, float y);
static int place_series_rlc(Circuit *circuit, float x, float y);
static int place_sil_loading(Circuit *circuit, float x, float y);
static int place_single_tuned_amp(Circuit *circuit, float x, float y);
static int place_sr_latch(Circuit *circuit, float x, float y);
static int place_substation(Circuit *circuit, float x, float y);
static int place_summing_amp(Circuit *circuit, float x, float y);
static int place_superposition(Circuit *circuit, float x, float y);
static int place_tesla_coil_detuned(Circuit *circuit, float x, float y);
static int place_thevenin(Circuit *circuit, float x, float y);
static int place_tl431_ref(Circuit *circuit, float x, float y);
static int place_transimpedance(Circuit *circuit, float x, float y);
static int place_tri_square_gen(Circuit *circuit, float x, float y);
static int place_voltage_divider(Circuit *circuit, float x, float y);
static int place_voltage_doubler(Circuit *circuit, float x, float y);
static int place_voltage_follower(Circuit *circuit, float x, float y);
static int place_wheatstone_bridge(Circuit *circuit, float x, float y);
static int place_wien_oscillator(Circuit *circuit, float x, float y);
static int place_window_comp(Circuit *circuit, float x, float y);
static int place_zener_clipper(Circuit *circuit, float x, float y);
static int place_zener_ref(Circuit *circuit, float x, float y);

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
        case CIRCUIT_3PH_Y_BALANCED:   return place_3ph_y_balanced(circuit, x, y);
        case CIRCUIT_3PH_UNBALANCED:   return place_3ph_unbalanced(circuit, x, y);
        case CIRCUIT_3PH_345_LINE:     return place_3ph_345_line(circuit, x, y);
        case CIRCUIT_3PH_RECTIFIER:    return place_3ph_rectifier(circuit, x, y);
        case CIRCUIT_SCHMITT_BISTABLE: return place_schmitt_bistable(circuit, x, y);
        case CIRCUIT_TRI_SQUARE_GEN:   return place_tri_square_gen(circuit, x, y);
        case CIRCUIT_FUNCTION_GEN:     return place_function_gen(circuit, x, y);
        case CIRCUIT_COLPITTS:         return place_colpitts(circuit, x, y);
        case CIRCUIT_RING_OSC:         return place_ring_osc(circuit, x, y);
        case CIRCUIT_HARTLEY:          return place_hartley(circuit, x, y);
        case CIRCUIT_CLAPP:            return place_clapp(circuit, x, y);
        case CIRCUIT_THEVENIN:         return place_thevenin(circuit, x, y);
        case CIRCUIT_SUPERPOSITION:    return place_superposition(circuit, x, y);
        case CIRCUIT_RC_STEP:          return place_rc_step(circuit, x, y);
        case CIRCUIT_RL_STEP:          return place_rl_step(circuit, x, y);
        case CIRCUIT_RLC_RING:         return place_rlc_ring(circuit, x, y);
        case CIRCUIT_RLC_DAMPING:      return place_rlc_damping(circuit, x, y);
        case CIRCUIT_OPAMP_SAT:        return place_opamp_sat(circuit, x, y);
        case CIRCUIT_SINGLE_TUNED_AMP: return place_single_tuned_amp(circuit, x, y);
        case CIRCUIT_COMMON_BASE:      return place_common_base(circuit, x, y);
        case CIRCUIT_DARLINGTON:       return place_darlington(circuit, x, y);
        case CIRCUIT_SR_LATCH:         return place_sr_latch(circuit, x, y);
        case CIRCUIT_POWER_PLANT:      return place_power_plant(circuit, x, y);
        case CIRCUIT_SUBSTATION:       return place_substation(circuit, x, y);
        case CIRCUIT_IO_PUSH_PULL:     return place_io_push_pull(circuit, x, y);
        case CIRCUIT_IO_OPEN_DRAIN:    return place_io_open_drain(circuit, x, y);
        case CIRCUIT_IO_OPEN_COLLECTOR: return place_io_open_collector(circuit, x, y);
        case CIRCUIT_IO_I2C_BUS:       return place_io_i2c_bus(circuit, x, y);
        case CIRCUIT_IO_I2C_LEVEL:     return place_io_i2c_level(circuit, x, y);
        case CIRCUIT_IO_INPUT_DEBOUNCE: return place_io_input_debounce(circuit, x, y);
        case CIRCUIT_IO_LOW_SIDE:      return place_io_low_side(circuit, x, y);
        case CIRCUIT_IO_HIGH_SIDE:     return place_io_high_side(circuit, x, y);
        case CIRCUIT_IO_SPI:           return place_io_spi(circuit, x, y);
        case CIRCUIT_IO_UART:          return place_io_uart(circuit, x, y);
        case CIRCUIT_IO_RS485:         return place_io_rs485(circuit, x, y);
        case CIRCUIT_IO_SPMI:          return place_io_spmi(circuit, x, y);
        case CIRCUIT_TX_69KV:          return place_tx_69kv(circuit, x, y);
        case CIRCUIT_TX_LADDER:        return place_tx_ladder(circuit, x, y);
        case CIRCUIT_TX_WIND:          return place_tx_wind(circuit, x, y);
        case CIRCUIT_TX_PLANT:         return place_tx_plant(circuit, x, y);
        case CIRCUIT_RES_SERVICE:      return place_res_service(circuit, x, y);
        case CIRCUIT_RES_BRANCH:       return place_res_branch(circuit, x, y);
        case CIRCUIT_RES_ACSTART:      return place_res_acstart(circuit, x, y);
        case CIRCUIT_RES_SOLAR:        return place_res_solar(circuit, x, y);
        case CIRCUIT_COM_480Y:         return place_com_480y(circuit, x, y);
        case CIRCUIT_COM_208Y:         return place_com_208y(circuit, x, y);
        case CIRCUIT_COM_PFC:          return place_com_pfc(circuit, x, y);
        case CIRCUIT_COM_ATS:          return place_com_ats(circuit, x, y);
        case CIRCUIT_GS_N1:            return place_gs_n1(circuit, x, y);
        case CIRCUIT_GS_IBR:           return place_gs_ibr(circuit, x, y);
        case CIRCUIT_GS_BOLD:          return place_gs_bold(circuit, x, y);
        case CIRCUIT_GS_DERATE:        return place_gs_derate(circuit, x, y);
        case CIRCUIT_GS_FACRATE:       return place_gs_facrate(circuit, x, y);
        case CIRCUIT_GS_KRON:          return place_gs_kron(circuit, x, y);
        case CIRCUIT_GS_RX:            return place_gs_rx(circuit, x, y);
        case CIRCUIT_GS_GOVERNOR:      return place_gs_governor(circuit, x, y);
        case CIRCUIT_GS_PIDS:          return place_gs_pids(circuit, x, y);
        case CIRCUIT_MOS_IDVGS:    return place_mos_idvgs(circuit, x, y);
        case CIRCUIT_MOS_IDVDS:    return place_mos_idvds(circuit, x, y);
        case CIRCUIT_MOS_TUNED:          return place_mos_tuned(circuit, x, y);
        case CIRCUIT_MOS_CG:             return place_mos_cg(circuit, x, y);
        case CIRCUIT_MOS_CASCODE:        return place_mos_cascode(circuit, x, y);
        case CIRCUIT_MOS_DIFF:           return place_mos_diff(circuit, x, y);
        case CIRCUIT_MOS_MIRROR:         return place_mos_mirror(circuit, x, y);
        case CIRCUIT_CMOS_INV:           return place_cmos_inv(circuit, x, y);
        case CIRCUIT_CMOS_NAND:          return place_cmos_nand(circuit, x, y);
        case CIRCUIT_CMOS_TGATE:         return place_cmos_tgate(circuit, x, y);
        case CIRCUIT_XY_LISSAJOUS:  return place_xy_lissajous(circuit, x, y);
        case CIRCUIT_XY_PLOTTER:    return place_xy_plotter(circuit, x, y);
        case CIRCUIT_HW_BUCK:            return place_hw_buck(circuit, x, y);
        case CIRCUIT_HW_BOOST:           return place_hw_boost(circuit, x, y);
        case CIRCUIT_HW_BUCKBOOST:       return place_hw_buckboost(circuit, x, y);
        case CIRCUIT_HW_CUK:             return place_hw_cuk(circuit, x, y);
        case CIRCUIT_HW_INTERLEAVED:     return place_hw_interleaved(circuit, x, y);
        case CIRCUIT_HW_PDN:             return place_hw_pdn(circuit, x, y);
        case CIRCUIT_HW_CAPS:            return place_hw_caps(circuit, x, y);
        case CIRCUIT_HW_MATCH:           return place_hw_match(circuit, x, y);
        case CIRCUIT_HW_REFLECT:         return place_hw_reflect(circuit, x, y);
        case CIRCUIT_HW_LOOP:            return place_hw_loop(circuit, x, y);
        case CIRCUIT_ID_SOURCE:          return place_id_source(circuit, x, y);
        case CIRCUIT_ID_DIODE:           return place_id_diode(circuit, x, y);
        case CIRCUIT_ID_CAP:             return place_id_cap(circuit, x, y);
        case CIRCUIT_ID_IND:             return place_id_ind(circuit, x, y);
        case CIRCUIT_ID_OPAMP:           return place_id_opamp(circuit, x, y);
        case CIRCUIT_ID_BJT:             return place_id_bjt(circuit, x, y);
        case CIRCUIT_ID_MOSFET:          return place_id_mosfet(circuit, x, y);
        case CIRCUIT_ID_OPAMP_ERR:       return place_id_opamp_err(circuit, x, y);
        case CIRCUIT_PARTS_MOSFET:       return place_parts_mosfet(circuit, x, y);
        case CIRCUIT_CAP_DCBIAS:         return place_cap_dcbias(circuit, x, y);
        case CIRCUIT_NE555_ASTABLE:      return place_ne555_astable(circuit, x, y);
        case CIRCUIT_PIERCE:             return place_pierce(circuit, x, y);
        case CIRCUIT_IV_PROBE_COMP:      return place_iv_probe_comp(circuit, x, y);
        case CIRCUIT_IV_PROBE_LOADING:   return place_iv_probe_loading(circuit, x, y);
        case CIRCUIT_IV_GROUND_LEAD:     return place_iv_ground_lead(circuit, x, y);
        case CIRCUIT_IV_SCOPE_INPUT_Z:   return place_iv_scope_input_z(circuit, x, y);
        case CIRCUIT_IV_AC_COUPLING:     return place_iv_ac_coupling(circuit, x, y);
        case CIRCUIT_IV_SHUNT_SENSE:     return place_iv_shunt_sense(circuit, x, y);
        case CIRCUIT_IV_KELVIN:          return place_iv_kelvin(circuit, x, y);
        case CIRCUIT_IV_BUCK_NODES:      return place_iv_buck_nodes(circuit, x, y);
        case CIRCUIT_IV_LDO_VS_BUCK:     return place_iv_ldo_vs_buck(circuit, x, y);
        case CIRCUIT_IV_BOOTSTRAP:       return place_iv_bootstrap(circuit, x, y);
        case CIRCUIT_IV_TERMINATION:     return place_iv_termination(circuit, x, y);
        case CIRCUIT_IV_PULLUP_SIZING:   return place_iv_pullup_sizing(circuit, x, y);
        case CIRCUIT_IV_GROUND_BOUNCE:   return place_iv_ground_bounce(circuit, x, y);
        case CIRCUIT_IV_CROSSTALK:       return place_iv_crosstalk(circuit, x, y);
        case CIRCUIT_IV_ESD_CLAMP:       return place_iv_esd_clamp(circuit, x, y);
        case CIRCUIT_IV_CAP_ENERGY:      return place_iv_cap_energy(circuit, x, y);
        case CIRCUIT_IV_MILLER:          return place_iv_miller(circuit, x, y);
        case CIRCUIT_IV_SWITCH_CHOICE:   return place_iv_switch_choice(circuit, x, y);
        case CIRCUIT_IV_INRUSH:          return place_iv_inrush(circuit, x, y);
        case CIRCUIT_TLINE_REAL:         return place_tline_real(circuit, x, y);
        case CIRCUIT_SEVENSEG_TEST:      return place_sevenseg_test(circuit, x, y);
        case CIRCUIT_WIRELESS_LINK:      return place_wireless_link(circuit, x, y);
        case CIRCUIT_BCD_COUNTER:        return place_bcd_counter(circuit, x, y);
        case CIRCUIT_DIGITAL_CLOCK:      return place_digital_clock(circuit, x, y);
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
    [CIRCUIT_MULTISTAGE_AMP] = {"TWO-STAGE AMPLIFIER: two identical common-emitter stages in cascade.", "Each inverts; ~4.7x unloaded, ~13x overall once stage 2 loads stage 1 - and back in phase.", "Interstage cap C2 passes the signal but isolates the two bias networks.", "PROBE: input (10 mVpk) and the second collector. Expect ~0.13 Vpk, in phase."},
    [CIRCUIT_DIFFERENTIAL_PAIR] = {"DIFFERENTIAL PAIR: both bases sit at 6 V; the shared tail resistor", "carries 0.54 mA. A difference between the inputs steers that current", "from one transistor to the other, so the collectors swing in opposite", "directions (gain g_m R_C / 2 = 25 per side). Common-mode changes cancel.", "PROBE: base (10 mV) and both collectors: mirror-image 0.5 V swings around 10.75 V.", "Raise the inputs to 50 mV and the pair saturates - the outputs turn into rounded squares."},
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
    [CIRCUIT_PC_OVERCURRENT] = {"50/51 OVERCURRENT: the CT (600:5, N = 120) turns 600 A of feeder current into 5 A through the", "1 ohm burden = 7.07 Vpk. Diode + 10 uF hold the peak; the comparator trips above 8 V, i.e. when", "the primary exceeds 738 A rms (the 50 element; hold tau = 100 ms). The fault switch (13 ohm)", "closes at t = 40 ms and raises the current to 1200 A: TRIP goes high. AEP feeds every BES", "breaker from redundant A/B relays; a 51 time curve is just a longer RC delay on V_hold.", "PROBE: TRIP is auto-probed; also probe the burden top (7 V -> 14 V) and the pulse source."},
    [CIRCUIT_PC_DIFFERENTIAL] = {"87 LINE DIFFERENTIAL: the two 120:1 CTs bracket the protected zone and are wired in opposition", "so their secondary currents circulate: with equal current at both ends (load, or a THROUGH fault", "beyond CT2 at t = 240 ms) nothing flows in the 1 ohm differential burden. An INTERNAL fault", "(2 ohm at t = 100 ms) makes I1 = 2828 A but I2 = 257 A: (I1-I2)/120 = 21 A rms -> 30 Vpk on R_d,", "far above the 1 V pickup (hold tau 22 ms). AEP prefers 87L over direct fiber, distance as backup.", "PROBE: TRIP auto-probed; probe R_d (top) to see the two faults treated differently."},
    [CIRCUIT_PC_DISTANCE] = {"21 DISTANCE, ZONE 1: the relay compares |I| x Z_set (CT 400:1 into a 3.35 ohm replica, equal to", "80 % of the 50-mile line impedance referred through the VT ratio) against |V| from the VT (2875:1).", "|I Z_set| > |V| means the apparent impedance V/I is inside the reach -> TRIP. The 40 % fault", "(t = 100 ms): V_sec 38 V, I Z_set 76 V -> trip. The 100 % fault (t = 240 ms): 52 V vs 42 V -> no trip", "(zone 2, with a 0.3 s timer, covers it). Zone 1 is set short of the far end so it never overreaches.", "PROBE: TRIP auto-probed; also probe the two peak-detector caps (|V| below-left, |I Z| top)."},
    [CIRCUIT_PC_BREAKER_FAIL] = {"50BF BREAKER FAILURE: when a relay trips a breaker (TRIP pulse at 50 ms) the current must vanish", "within ~5 cycles. START = TRIP AND current-still-present charges C through R (tau = 150 ms); if", "the current is still there when the timer expires, BFT = timer AND current trips the adjacent", "breakers. Here the current pulse stays on (stuck breaker): BFT fires at ~200 ms. Set the 50BF", "pulse width to 83 ms (healthy breaker): C only reaches 5 V of the 7.6 V (0.632 x 12) threshold.", "PROBE: BFT auto-probed; probe the capacitor to see the timer ramp reset when START drops."},
    [CIRCUIT_SIL_LOADING] = {"SURGE IMPEDANCE LOADING: a line loaded with its characteristic impedance Zc = sqrt(L/C) = 283 ohm", "absorbs exactly the VARs it generates: the voltage profile is flat (Vr/Vs = 0.996 here) and", "the angle is small. P_SIL = V^2/Zc = 345^2/283 = 420 MW. Close SW (2 x SIL, 141 ohm): the line", "now needs VARs it cannot supply and the far end sags to 0.80. St. Clair: ~2 SIL is OK at 100 mi,", "~1 SIL at 300 mi - long lines are limited by voltage and stability, not by conductor heating.", "PROBE: auto-placed on both ends: nearly equal at SIL, 20 % apart after closing SW."},
    [CIRCUIT_SERIES_COMP] = {"SERIES COMPENSATION: the line reactance X = 120 ohm (200 mi) limits how much power can flow", "before the far end sags. A capacitor with Xc = 60 ohm (44 uF) in the middle cancels half of it:", "at 2 x SIL the receiving end rises from 0.80 to about 0.90. Close SW to bypass the capacitor", "and watch the drop return. AEP uses series caps on long 765/345 kV paths (with protection", "against subsynchronous resonance and MOV bypass on faults).", "PROBE: auto-placed on both ends; the source-side probe barely moves, the load end jumps."},
    [CIRCUIT_HV_765_LINE] = {"765 kV (AEP's backbone since 1969): 300 miles of six-conductor bundle, R = 0.02, X = 0.53 ohm/mi,", "B = 8.5 uS/mi (bundling lowers X and raises B). Zc = sqrt(0.53/8.5e-6) = 250 ohm, so", "SIL = 765^2/250 = 2340 MW - about 6 x a 345 kV circuit with half the losses per MW. Loaded at", "SIL the 300-mile profile stays flat (0.99). One nominal pi for 300 mi is coarse: for accuracy", "split it into 3 x 100 mi sections (place three TLine parts) - the pi model is exact only per section.", "PROBE: auto-placed on both ends; 200 kV/div. Try 2 x SIL (125 ohm) and 600 mi."},
    [CIRCUIT_3PH_Y_BALANCED] = {"THREE-PHASE Y, BALANCED: three equal sources 120 deg apart (277 V rms = 480 V line-line)", "feeding equal loads. The three load currents also sum to zero at every instant, so the neutral", "carries nothing (probe it: ~0 V across the 1 ohm neutral resistor) and the total power is", "constant, not pulsating - that is why motors and generators are three-phase. Line-line", "voltage = sqrt(3) x line-neutral; phase order A-B-C sets the direction a motor turns.", "PROBE: A (source), B and C loads, and the neutral node (~0). Use Stack to separate them."},
    [CIRCUIT_3PH_UNBALANCED] = {"THREE-PHASE Y, UNBALANCED: loads 10 / 20 / 40 ohm. The phase currents no longer cancel;", "their sum (I_A + I_B + I_C) returns through the neutral. With 1 ohm of neutral impedance the", "neutral point shifts (probe it: tens of volts at 60 Hz) and the lightly loaded phases see a", "higher voltage than the heavily loaded one - the classic 'lost neutral' hazard in a service.", "Set the neutral resistor to 1 mohm (solid neutral) or 1 Mohm (open neutral) and compare.", "PROBE: A (source), B and C load nodes and the neutral. Compare load amplitudes per phase."},
    [CIRCUIT_3PH_345_LINE] = {"THREE-PHASE 345 kV LINE: the single-phase 345 kV example done for all three phases (100 mi", "of twin Drake per phase, 198.4 ohm per phase = 600 MW). Everything in a balanced system can be", "computed per phase and multiplied by three - the reason the other power examples are", "single-phase equivalents. Each phase drops the same 6.3 % (281.7 -> 264 kVpk) 120 deg apart.", "Unbalance one load and the neutral (1 ohm) shows the zero-sequence current.", "PROBE: load nodes A, B (auto) and C, 100 kV/div; Stack view separates the three phases."},
    [CIRCUIT_3PH_RECTIFIER] = {"SIX-PULSE RECTIFIER: three phases into a diode bridge. The plus bus always follows the", "highest phase and the minus bus the lowest, so V+ - V- = line-line peak x cos(0..30 deg):", "DC = 1.35 x V_LL(rms) = 2.34 x V_LN(rms) (here ~ 280 V), ripple only 4 % at 6 x 60 = 360 Hz -", "far smoother than single-phase bridges; this is the front end of every VFD and HVDC pole.", "Each diode conducts 120 deg per cycle. Add a capacitor across the load and watch the pulses shrink.", "PROBE: plus bus (auto), minus bus (extra) and phase A. The load sees their difference."},
    [CIRCUIT_SCHMITT_BISTABLE] = {"BISTABLE MULTIVIBRATOR (S&S 18.4): positive feedback R1/R2 makes the op-amp a latch. With the", "output at +15 V the + input sits at +7.5 V, so the input must rise ABOVE 7.5 V before the output", "snaps to -15 V; it then has to fall below -7.5 V to snap back. That hysteresis (15 V wide) is", "why a Schmitt trigger ignores noise near the threshold. V_TH = L+ R1/(R1+R2), V_TL = L- R1/(R1+R2).", "Try the X-Y (Y-T button) view: output vs input draws the hysteresis loop.", "PROBE: triangle input and OUT (auto). Two clean edges per input cycle, at +/-7.5 V exactly."},
    [CIRCUIT_TRI_SQUARE_GEN] = {"TRIANGLE / SQUARE GENERATOR (S&S 18.5.2): a non-inverting bistable (thresholds +/-15 R1/R2 =", "+/-7.5 V) drives an integrator; the integrator ramps at 15 V/RC until it reaches a threshold,", "the bistable flips and the ramp reverses. f = R2/(4 R C R1) = 20k/(4 x 10k x 10n x 10k) = 5 kHz.", "Frequency: R or C. Amplitude: R1/R2 (or the supply). The rails cancel out of f: that is the trick.", "A 50 us kick on the bistable input breaks the perfect 0 V equilibrium at start-up.", "PROBE: triangle (auto) and square (extra probe). Stack view shows the square edges at the peaks."},
    [CIRCUIT_FUNCTION_GEN] = {"FUNCTION GENERATOR (S&S 18.8.2): the triangle from the generator above feeds R_in and a", "piecewise-linear diode network. Below 2.6 V nothing conducts (slope 1); above it the 22k branch", "to +2.0 V loads the node (slope 0.69); above 4.3 V the 5.6k branch to +3.7 V (slope 0.31).", "Mirror branches handle the negative half. Three breakpoints turn the triangle into a ~5 V sine", "(THD a few %). Frequency: edit R (10k) or C; amplitude: R2 of the bistable, then re-scale the bias V.", "PROBE: sine output (auto) and triangle (extra). Try the FFT button: 3rd harmonic > 30 dB down."},
    [CIRCUIT_COLPITTS] = {"COLPITTS (S&S 18.3.1): the tank is L with a capacitive divider C1-C2; the divider feeds back a", "fraction C1/C2 of the drain swing to the gate, so oscillation needs g_m R_tank > C2/C1 (= 1 here).", "f = 1/(2 pi sqrt(L C1C2/(C1+C2))) = 1/(2 pi sqrt(100u x 0.5n)) = 712 kHz. The 1 mH RFC is an open", "circuit at RF but passes the DC drain current; the 10 nF coupling cap keeps 12 V off the gate;", "1M/1M bias the gate at 6 V. Amplitude limits when the MOSFET cuts off each cycle (class C).", "PROBE: drain (auto). 500 ns/div. A 50 ns kick starts it; edit C1 to 2 nF -> 616 kHz."},
    [CIRCUIT_RING_OSC] = {"RING OSCILLATOR: an odd number of inverters in a loop can never settle - each stage inverts,", "so the signal returns inverted and the ring keeps flipping. Period = 2 N t_pd. Real gates delay by", "their own capacitance; here each stage has R 1k / C 1n, and a gate flips when its RC reaches the", "2.5 V threshold: t ~ 0.69 RC = 0.7 us -> f ~ 1/(2 x 5 x 0.7 us) ~ 145 kHz. Edit any C to retune.", "Probe several stages: five squares, each shifted by one fifth of a half period.", "PROBE: last stage (auto). 2 us/div. The 2 us kick pulse on the first RC breaks the symmetry."},
    [CIRCUIT_HARTLEY] = {"HARTLEY (S&S 18.3.1): the Colpitts with the roles swapped - a tapped inductor (L1 = L2 = 50 uH,", "tap at Vdd = AC ground; L1 is the drain load) and C = 1 nF. f = 1/(2 pi sqrt((L1 + L2) C)) = 503 kHz;", "start-up needs g_m R_tank > L1/L2 (= 1). A 220 nF cap keeps the 6 V gate bias off L2.", "It runs at 534 kHz, 6 % above the ideal: the tap is only an AC ground through the supply and", "the device capacitances sit across part of the tank. PROBE: drain, AC-coupled, 500 ns/div."},
    [CIRCUIT_CLAPP] = {"CLAPP: a Colpitts whose L has a small series capacitor C3 = 100 pF. The effective tank C is", "1/(1/C1 + 1/C2 + 1/C3) = 83 pF, dominated by C3, so f = 1/(2 pi sqrt(L C3_eff)) = 1.744 MHz and the", "transistor capacitances (which sit across C1, C2) hardly pull the frequency: better stability.", "C1 = C2 = 1 nF swamp the device; the price is a weaker feedback fraction, so g_m must be higher.", "PROBE: drain (auto), 200 ns/div. Change C3 to 47 pF -> 2.4 MHz."},
    [CIRCUIT_THEVENIN] = {"THEVENIN EQUIVALENT (A&L 3.6): everything left of the load is replaced by V_th (the open-circuit", "voltage, 10 x 3k/(2k+3k) = 6 V) in series with R_th (sources zeroed: 2k||3k + 1k = 2.2k).", "Then V_L = V_th R_L/(R_L + R_th) = 3.0 V, and the load gets the most power when R_L = R_th (4.1 mW).", "Norton: I_N = V_th/R_th = 2.73 mA. Edit R_L: 1k -> 1.875 V, open -> 6 V.", "PROBE: load node (auto): 3.00 V. The battery probe reads 10 V - the divider does the rest."},
    [CIRCUIT_SUPERPOSITION] = {"SUPERPOSITION (A&L 3.5): with linear elements the node voltage is the sum of each source acting", "alone (voltage sources shorted, current sources opened). 12 V alone: 12 x (4k||4k)/(4k + 4k||4k) = 4 V;", "6 V alone: 2 V; 1 mA alone into 4k||4k||4k = 1.33 V. Total 7.33 V. Zero one source and watch.", "Try it: set the current source to 0 -> 6.00 V; set V1 to 0 -> 3.33 V.", "PROBE: node N (auto). Turn the 1 mA source into an AC source to see the ripple ride on the DC."},
    [CIRCUIT_RC_STEP] = {"RC STEP RESPONSE (A&L 10.1): V_C = 5 (1 - e^(-t/tau)), tau = RC = 1 ms. At t = tau the capacitor", "has 63 % (3.16 V); at 2.2 tau it passed 90 %; after 5 tau it is settled. Discharge is the mirror.", "Use the cursors: put A on the edge and B where V_C = 3.16 V - the readout says 1.00 ms.", "Double R or C and tau doubles; the 100 Hz square (5 ms half period = 5 tau) just settles.", "PROBE: square input and capacitor (auto)."},
    [CIRCUIT_RL_STEP] = {"RL STEP RESPONSE (A&L 10.2): the inductor opposes changes in current, so i_L rises as", "(V/R)(1 - e^(-t/tau)) with tau = L/R = 100 us. The resistor voltage IS the current (100 ohm x i_L):", "5 V final = 50 mA, 3.16 V (31.6 mA) at t = tau. The inductor voltage jumps to 5 V then decays.", "Interchange with the RC case: here the CURRENT is the state that cannot jump.", "PROBE: square input and resistor (auto). Probe the L-R junction for the inductor voltage."},
    [CIRCUIT_RLC_RING] = {"SERIES RLC STEP (A&L 12.2): w0 = 1/sqrt(LC) = 31.6 krad/s (5.03 kHz), alpha = R/2L = 1000/s,", "zeta = alpha/w0 = 0.032 -> underdamped. The capacitor overshoots to 5(1 + e^(-pi zeta/sqrt(1-zeta^2)))", "= 9.5 V and rings at the damped frequency with an envelope tau = 1/alpha = 1 ms (Q = 16).", "Raise R and the ringing dies: 632 ohm is critical, 2k overdamped (see RLC Damping Ladder).", "PROBE: square input and capacitor (auto). Cursors on two peaks: period 199 us."},
    [CIRCUIT_RLC_DAMPING] = {"DAMPING LADDER (A&L 12.2-12.3): same L = 10 mH, C = 100 nF, three values of R. R = 20 ohm rings", "(zeta 0.03); R = 632 ohm = 2 sqrt(L/C) is critically damped - the fastest rise with no overshoot", "(4.0 V at 3/w0 = 95 us); R = 2k is overdamped: two real roots, the slow one tau = 195 us.", "The rows share the same square input; compare the three capacitor voltages in Stack view.", "PROBE: the critical-row capacitor (auto) plus the underdamped and overdamped rows (extra)."},
    [CIRCUIT_OPAMP_SAT] = {"OP-AMP SATURATION (S&S 2.8, A&L 15.5): the inverting amplifier wants -10 x 2 Vpk = 20 Vpk, but", "the output can only reach the +/-15 V rails. While clipped the feedback loop is broken: the", "inverting input is no longer a virtual ground - it follows (v_i R2 + v_o R1)/(R1 + R2), 0.45 V at", "the input peak. Clipping starts at |v_i| = 1.5 V; lower the input to 1 V and the sine is clean.", "PROBE: output (auto) and the inverting input (extra probe): watch it leave 0 V while clipped."},
    [CIRCUIT_SINGLE_TUNED_AMP] = {"SINGLE-TUNED AMPLIFIER: a common-emitter stage whose collector load is a parallel LC tank (L 1 mH,", "C 2.53 nF, Rq 10k). At resonance f0 = 1/(2 pi sqrt(LC)) = 100 kHz the tank impedance is just Rq,", "so the gain peaks at g_m (Rq || RL) ~ 500; off resonance L or C shorts the collector and the", "gain collapses. Bandwidth = f0/Q with Q = Rq / (2 pi f0 L) = 16 -> ~6 kHz. This is the IF stage of", "every classic radio; Rq (or the coil loss) sets the selectivity, C tunes the station.", "PROBE: input (10 mV) and output (auto). The sweep 20-500 kHz shows the narrow peak; use Trk."},
    [CIRCUIT_COMMON_BASE] = {"COMMON BASE (S&S 7.3.5): the base is AC-grounded (10 uF), the signal enters the emitter and", "leaves at the collector IN PHASE. Input resistance is only r_e = V_T/I_E = 25 ohm (I_E = 1 mA),", "gain A_v = g_m R_C = 40 mA/V x 4.7k = +188. Low R_in and no Miller effect make it the natural", "RF / cascode partner; it is a lousy voltage buffer. Add 50 ohm in series with the source: 1/3 lost.", "PROBE: input (10 mV) and output (auto): ~1.9 V peak, same polarity as the input."},
    [CIRCUIT_DARLINGTON] = {"DARLINGTON FOLLOWER (S&S 7.3.7): two emitter followers in cascade multiply the current gain,", "so R_in ~ beta1 beta2 R_E = 100 x 100 x 100 = 1 M. Even through a 100k source resistor the", "output still follows the 1 Vpk input (0.91 Vpk); a single transistor (beta R_E = 10k) would", "only pass 0.09 Vpk. The price: two V_BE drops (6 V in -> 4.6 V out) and slower turn-off.", "PROBE: input (6 V + 1 Vpk) and the emitter output (auto)."},
    [CIRCUIT_SR_LATCH] = {"SR LATCH (S&S 15.1.1): two NOR gates feed each other. S = 1 forces Q = 1 (Qbar = 0); when S", "returns to 0 the cross-coupling keeps Q = 1: memory. R = 1 forces Q = 0. Both = 1 is forbidden", "(both outputs 0, and the result after release depends on which drops last).", "S pulses at 0.2 ms, R at 0.6 ms (1 ms period): Q is a 0.4 ms-wide pulse every millisecond.", "PROBE: S (auto), Q (auto), Qbar and R (extra probes) - use Stack view. S feeds the Qbar gate, R the Q gate."},
    [CIRCUIT_POWER_PLANT] = {"POWER PLANT: the 3-phase block is a synchronous generator (18 kV line-line, 14.7 kVpk per phase)", "behind its subtransient reactance X'' (0.184 mH per phase). Three single-phase GSU transformers", "(1:19.17) lift it to 345 kV; breakers connect the 100-mile lines that deliver 600 MW (198 ohm/phase).", "Open one breaker: that phase's load drops and the neutral currents no longer cancel.", "Real plants: 500-1300 MW units, 13.8-24 kV terminals, GSU 300-700 MVA at 10-14 % impedance.", "PROBE: generator phase A (auto), the three 345 kV load buses (auto + extra). 100 kV/div, Stack."},
    [CIRCUIT_SUBSTATION] = {"TRANSMISSION SUBSTATION: the 3-phase block stands for the 345 kV grid. Each phase: 50 mi of", "345 kV line -> breaker -> 345/138 kV autotransformer (0.4) -> 138 kV bus -> 30 mi feeder ->", "90 MW at pf 0.9 (171.5 ohm + 0.22 H). The lagging load drags the far bus down ~7 %; close the", "cap-bank switches (6.1 uF per phase) and it recovers. Open a breaker to drop one phase.", "AEP practice: breaker-and-a-half bus, redundant A/B relaying, cap banks switched by voltage.", "PROBE: grid phase A (auto), the three 138 kV feeder buses (auto + extra). 50 kV/div, Stack."},
    [CIRCUIT_IO_PUSH_PULL] = {"PUSH-PULL (CMOS) OUTPUT: this is what a GPIO pin is inside. Input HIGH turns the NMOS on", "(output pulled to 0 V) and the PMOS off; input LOW does the opposite, so the output is", "actively driven both ways - fast edges into 20 pF, no pull-up needed, but two push-pull", "pins on one wire fight (shoot-through). The stage inverts.", "PROBE: input pulse (1 MHz) and the output across the 10k / 20 pF load: clean inverted 0-3.3 V."},
    [CIRCUIT_IO_OPEN_DRAIN] = {"OPEN-DRAIN OUTPUT: only an NMOS to ground. The pin can pull LOW hard, but HIGH is", "made by the external 4.7k pull-up charging the 100 pF line: tau = 470 ns, so the rising", "edge is slow and the falling edge is fast. That asymmetry is the signature of I2C, reset", "lines and interrupt pins. Raise C or R and watch the rise stretch; several open-drain", "pins can share the wire (wired-AND). PROBE: input (200 kHz) and the drain / pull-up node."},
    [CIRCUIT_IO_OPEN_COLLECTOR] = {"OPEN-COLLECTOR LEVEL SHIFT: a 3.3 V pin drives the NPN base through 1k (2.6 mA), the", "collector is pulled up to a different rail (5 V) through 4.7k. Output LOW = Vce(sat)", "~0.1 V, HIGH = 5 V, and it is INVERTED. The classic way to drive a 5 V (or 12 V or 24 V)", "input from a low-voltage MCU; the pull-up and the line capacitance set the rise time.", "PROBE: 3.3 V input (100 kHz) and the 5 V collector node."},
    [CIRCUIT_IO_I2C_BUS] = {"I2C SDA, WIRED-AND: master (50 kHz, 30 %) and slave (20 kHz, 20 %, delayed) are both", "open-drain on the same 3.3 V line. The line is LOW whenever EITHER device pulls; it is", "HIGH only when both release - that is how ACK and clock stretching work. The 4.7k pull-up", "against 200 pF of bus gives ~1 us rising edges (standard mode allows 1 us at 100 kHz).", "PROBE: master pulse, slave pulse (extra) and SDA. Stack view shows the AND."},
    [CIRCUIT_IO_I2C_LEVEL] = {"I2C LEVEL SHIFTER (NXP AN10441): one NMOS with its gate on the 3.3 V rail, source on", "the 3.3 V bus, drain on the 5 V bus, pull-ups on both sides. Idle: both sides high, the", "MOSFET is off (Vgs = 0). When the 3.3 V side is pulled low Vgs = 3.3 V, the MOSFET turns", "on and drags the 5 V side down too; release and both pull-ups restore their own level.", "It works in both directions (the body diode pulls the low side when the 5 V side drops).", "PROBE: driver pulse, 3.3 V side (extra) and the 5 V side: same timing, two levels."},
    [CIRCUIT_IO_INPUT_DEBOUNCE] = {"GPIO INPUT: the 10k pull-up idles the pin at 3.3 V; pressing the button (the analog switch,", "driven by the 50 Hz pulse: 10 ms press, 10 ms release) shorts it to 0 V. Real contacts", "bounce for ~1 ms, so 10k + 100 nF low-passes the pin before the inverter (threshold 1.65 V):", "discharge tau 1 ms, recharge tau 2 ms (through both 10k) - a bounce never reaches the", "threshold. Cost: ~1-2 ms of latency; the pin itself ramps because the RC loads it.", "PROBE: button, pin (extra), RC node (extra) and the inverter output: sharp / slow / delayed."},
    [CIRCUIT_IO_LOW_SIDE] = {"LOW-SIDE SWITCH: the load (relay coil: 10 mH + 50 ohm) hangs from the 12 V rail and the", "NMOS pulls its bottom end to ground - the simplest way an MCU drives a coil, motor or LED", "string. Gate needs 5 V for a plain NMOS (logic-level part). At turn-off the coil current", "(240 mA) has nowhere to go: without the diode the drain would fly to kilovolts and kill", "the MOSFET; the flyback diode clamps it at 12.7 V and the current decays with L/R = 200 us.", "PROBE: gate pulse (500 Hz) and the drain: 0 V on, 12.7 V clamp then 12 V off."},
    [CIRCUIT_IO_HIGH_SIDE] = {"HIGH-SIDE SWITCH: the load sits between the PMOS and ground, so the switch is in the 12 V", "rail. A 3.3 V pin cannot reach 12 V, so an NPN level shifter pulls the PMOS gate down:", "logic HIGH -> NPN on -> gate 0 V -> Vgs = -12 V -> PMOS on -> load sees 12 V. Logic LOW ->", "10k pull-up parks the gate at 12 V -> PMOS off. Two inversions = non-inverting overall.", "Used for switched power rails, LED strips, 'ignition' style loads.", "PROBE: logic pulse, PMOS gate (extra) and the load: 0 / 12 V in phase with the input."},
    [CIRCUIT_IO_SPI] = {"SPI: push-pull drivers, SCLK 10 MHz and MOSI 5 MHz (a 1010 pattern), each through a 33 ohm", "series termination into 200 pF of ribbon cable. tau = 6.6 ns rounds every edge; at 10 MHz", "the clock still reaches the rails, at 50 MHz it would not - that is why long SPI cables fail", "and why the series resistor sits at the driver (it damps the reflection a real cable adds).", "PROBE: SCLK source, SCLK at the load, MOSI source and MOSI at the load (extras). Stack view."},
    [CIRCUIT_IO_UART] = {"UART BETWEEN 5 V AND 3.3 V PARTS: TX of the 5 V device goes through a 1k / 2k divider, so the", "3.3 V RX pin sees 3.33 V (never 5 V straight in - the ESD diode would conduct). The other", "direction needs nothing: a 5 V TTL input takes anything above V_IH = 2 V as HIGH, so the", "3.3 V TX drives it directly (the inverter here is the 5 V input stage). Idle level is HIGH.", "PROBE: 5 V TX, 3.3 V RX node, 3.3 V TX (extra) and the 5 V receiver output (extra)."},
    [CIRCUIT_IO_RS485] = {"RS-485 (differential, half-duplex): A carries the data, B its complement (the inverter),", "120 ohm at both ends of the pair so the line is matched. The receiver is a comparator on", "A - B: it only needs a 200 mV difference. 1 V of 100 kHz noise is injected into BOTH wires", "(ground shift, coupling) - each wire wobbles, the difference does not, and the output is a", "clean 0 / 5 V copy of the data. That is why every long industrial bus is differential.", "PROBE: data, A at the far end (extra), B at the far end (extra), receiver output. Stack."},
    [CIRCUIT_IO_SPMI] = {"SPMI (MIPI System Power Management Interface): the two-wire 1.8 V bus between an SoC and", "its PMIC. SCLK (5 MHz here, up to 26 MHz) and SDATA are push-pull, 33 ohm into 15 pF of", "on-board load. Bus arbitration and the SDATA bus-keeper are protocol level (not modelled);", "the electrical lesson is the 1.8 V swing: V_IH ~ 1.2 V leaves ~0.6 V of noise margin, so", "ground bounce and crosstalk that a 5 V bus shrugs off will corrupt this one.", "PROBE: SCLK source, SDATA at the load and SCLK at the load (extra). 50 ns/div."},
    [CIRCUIT_TX_69KV] = {"AEP TEXAS 69 kV SUBTRANSMISSION: 20 miles of 336.4 ACSR (Linnet) is 6.1 + j15 ohm.", "The load is 20 MVA three-phase at 0.95 pf lagging - AEP's design power factor - so per phase", "226 ohm in series with 197 mH. The receiving bus lands at 0.957 pu, inside the 0.95-1.05 pu", "ERCOT Planning Guide / NERC TPL-001 system-normal band with almost nothing to spare.", "PROBE: sending bus (source) and the load bus. Both are 60 Hz sines; read the peaks.", "TRY: 25 MVA -> 0.946 pu, a steady-state violation. Model 1 -> 2 gives the line its charging."},
    [CIRCUIT_TX_LADDER] = {"TEXAS VOLTAGE LADDER: every level a Texas electron passes through. 345 kV (ERCOT's highest", "transmission voltage) -> 138 kV -> 69 kV subtransmission -> 12.47 kV distribution -> the 240 V", "service, with a tap load at each level (300 / 100 / 15 / 3 MW and a 10 kW house). The 69/12.47 kV", "substation transformer carries its LTC 8 steps up (+5 %, 0.625 % per step) - without it the house", "sits at 112 V, below ANSI C84.1 Range A. Buses: 0.99 / 0.97 / 0.96 / 0.99 pu, service 117.7 V.", "PROBE: 345 kV source, the house, and the 138 / 69 / 12.47 kV buses. Use Stack + Fit."},
    [CIRCUIT_TX_WIND] = {"CREZ WIND COLLECTOR: the Texas build-out that put 345 kV lines out to the Panhandle. Two", "34.5 kV turbine strings (each behind its pad transformer, 1 + j2 ohm) feed a 6 mi collector", "cable, a 34.5/345 kV GSU and 30 mi of 345 kV line into the ERCOT grid. The strings run 6 deg", "ahead of the grid: that angle is what pushes ~50 MW out, and it lifts the collector bus above", "the grid - the voltage-rise problem every collector system has. ERCOT wants 0.95 pf at the POI.", "PROBE: string A, the collector bus, the 345 kV POI. TRY: open the string-B switch."},
    [CIRCUIT_TX_PLANT] = {"AEP 13.8 kV INDUSTRIAL SERVICE: a 13.8 kV primary (2 mi of 4/0) into the plant's 13.8/4.16 kV", "transformer. The 4.16 kV bus carries a 2500 hp motor group (0.88 pf) and feeds a 4160/480 V", "shop transformer with 300 kVA of 0.95 pf load. This is the standard industrial ladder: medium", "voltage for the big motors, 480 V for everything else.", "PROBE: 13.8 kV source, the 480 V shop bus, and the 4.16 kV motor bus.", "TRY: the motor R down (more load) and watch both downstream buses sag together."},
    [CIRCUIT_RES_SERVICE] = {"240/120 V RESIDENTIAL SERVICE: the pole transformer is two 7200:120 windings in series and", "the joint is the grounded neutral, so L1 and L2 are 120 V either side of it and 240 V apart.", "1.2 kW on L1, 600 W on L2, a 4.8 kW range across 240 V. The neutral carries only the", "difference of the two 120 V legs (10 A - 5 A = 5 A), which is why it can be smaller.", "PROBE: primary, L1, L2 and the range. L1 and L2 are 180 deg apart - that is the 240 V.", "TRY: neutral 0.02 -> 5 ohm (a corroded splice): L1 collapses, L2 rises past 126 V."},
    [CIRCUIT_RES_BRANCH] = {"120 V BRANCH CIRCUITS: the same 12 A load (80 % of a 15 A circuit) at the end of 100 ft of", "#14 copper and of 100 ft of #10. Both conductors count, so #14 is 2 x 100 x 2.525/1000 =", "0.505 ohm and #10 is 0.20 ohm. The loads land at 114.2 V (4.8 %) and 117.7 V (2.0 %). NEC 210.19(A)", "informs 3 % on a branch and 5 % overall - the #14 run misses it, the #10 run passes.", "PROBE: the panel, the #14 load and the #10 load. Read the peaks: 169.7 / 161 / 166 V.", "TRY: halve the load current, or run the #14 circuit only 50 ft, and it comes into compliance."},
    [CIRCUIT_RES_ACSTART] = {"AC COMPRESSOR START: a 5-ton condenser draws its locked-rotor current (104 A at 0.5 pf) for", "the first fraction of a second. On a long rural service (0.20 + j0.12 ohm of transformer and", "conductor) the panel falls from 333 to 310 Vpk - a 7 % dip to 219 V (109.6 V per leg) - and", "every lamp dims. IEEE 1453 and AEP distribution practice allow roughly 3 % for a rare start.", "PROBE: utility, the panel and the motor branch. The contactor closes at 50 ms.", "TRY: service R 0.20 -> 0.05 ohm (bigger transformer / conductor), or a soft starter."},
    [CIRCUIT_RES_SOLAR] = {"ROOFTOP SOLAR BACKFEED: a 7.6 kW inverter pushes 31.7 A back through the 0.25 ohm service.", "Current out of the house instead of into it means the voltage at the point of common coupling", "rises rather than sags: +7.9 V, so about 124 V per leg. IEEE 1547-2018 and the ERCOT DG rules", "require the PCC to stay inside ANSI C84.1 Range A (114-126 V) while exporting.", "PROBE: utility, the PCC and the house load. Compare the PCC peak with the utility peak.", "TRY: double the inverter current - the PCC passes 126 V, where volt-var / volt-watt kicks in."},
    [CIRCUIT_COM_480Y] = {"480Y/277 V COMMERCIAL SERVICE: the standard American commercial system. Each phase is 277 V", "to neutral, 480 V phase to phase. A 30 hp motor (0.85 pf) sits across all three phases and", "6 kW of 277 V lighting hangs on phase A - NEC 210.6(C) allows 277 V luminaires. Phase A is", "loaded more heavily, so it sits slightly lower than B and C.", "PROBE: phase A source and the three phase buses. Stack shows the 120 deg spacing.", "TRY: Range A for a 480 V service is 456-504 V line-to-line, i.e. 263-291 V here."},
    [CIRCUIT_COM_208Y] = {"208Y/120 V PANEL: three 120 V branches loaded 20 / 12 / 6 A. Because the phases are 120 deg", "apart the neutral carries the vector unbalance, not the sum: sqrt(a^2+b^2+c^2-ab-bc-ca) =", "12.2 A, not 38 A. That is why NEC 220.61 lets the neutral be sized on the unbalance.", "The 0.05 ohm shared neutral lifts the panel neutral slightly, so the loaded phase sags and", "the light one rises. PROBE: source A and the three branch buses.", "TRY: neutral 0.05 -> 1 ohm and the three 120 V branches spread apart badly."},
    [CIRCUIT_COM_PFC] = {"POWER FACTOR CORRECTION: a 33 kVA motor at 0.75 pf lagging on a 277 V phase draws 120 A,", "but only 25 kW of that is real. Close the switch and a 478 uF bank supplies 13.8 kvar", "locally: the same kW now needs 95 A, a 21 % smaller supply current. The scope watches a", "0.05 ohm shunt in the supply return, so 50 mV on CH1 is one amp.", "ERCOT / AEP tariffs price reactive power and NERC VAR-001 makes it a planning obligation.", "TRY: 956 uF over-corrects into a leading pf and the current climbs again."},
    [CIRCUIT_COM_ATS] = {"STANDBY GENERATOR TRANSFER: the utility contactor opens at 50 ms (an outage) and the", "generator contactor closes at 70 ms, so the life-safety load is dead for 20 ms. This is an", "open-transition transfer - the two sources are never closed together, which is what keeps a", "generator from being back-fed into the utility. NEC 700 gives emergency systems 10 s to", "transfer, NEC 701 gives legally required standby 60 s; NFPA 110 Type 10 is the 10 s class.", "PROBE: utility, the load bus and the generator. TRY: overlap the contactors and watch them fight."},
    [CIRCUIT_GS_N1] = {"N-1 CONTINGENCY (NERC TPL-001-5.1): two 200 mi 345 kV circuits feed one 340 MW bus.", "System intact (category P0) the parallel pair is 6 + j27.5 ohm and the bus sits at 0.972 pu,", "inside the 0.95-1.05 pu envelope AEP files under FERC Form 715. Open the breaker and the", "single remaining circuit doubles the impedance: 0.925 pu - below the P0 floor, but inside the", "0.92-1.05 pu post-contingency envelope, and the 4.8 % deviation is under the 8 % threshold that", "would force a documented engineering review. PROBE: source and the load bus."},
    [CIRCUIT_GS_IBR] = {"INVERTER RIDE-THROUGH (NERC PRC-029-1, ERCOT NOGRR-245, IEEE 2800-2022): a fault 100 ms in", "holds the point of interconnection near 0.3 pu for 150 ms. Under the old settings-based", "PRC-024-3 an inverter was allowed to trip on its own relay curve; from October 2026 the", "performance-based rule asks whether the plant actually stayed on, kept injecting current", "through the sag, and restored its pre-disturbance power within 1.0 s of recovery.", "PROBE: grid, the POI and the inverter branch. TRY: open the inverter breaker (the old behaviour)."},
    [CIRCUIT_GS_BOLD] = {"AEP BOLD: the same 150 mi 345 kV corridor at 600 MW, built twice. A conventional double-circuit", "tower gives 0.06 + j0.55 ohm/mi and 8 uS/mi, so Zc = sqrt(L/C) = 262 ohm and SIL = V^2/Zc =", "454 MW. BOLD's arch crossarm compacts the phases into a tight triangle, which raises the line's", "capacitance and lowers its inductance: Zc falls to 162 ohm and SIL rises to 735 MW (+62 %),", "with 40 % lower I^2R losses. Carrying the transfer naturally means no series capacitors - and", "therefore no sub-synchronous resonance risk. PROBE: source and both receiving buses. Stack."},
    [CIRCUIT_GS_DERATE] = {"EXTREME TEMPERATURE (NERC TPL-008-1, PUCT 16 TAC 25.55): weather-related outages are up 67 %", "since 2000, so planners now build cases that couple a benchmark temperature with load growth", "and equipment derating. This 20 mi 12.47 kV feeder uses a real aluminium conductor coefficient", "(4030 ppm/degC): 6.0 ohm at 25 degC, 7.2 ohm at 75 degC. Drag the Tmp slider in the status bar", "and watch the bus fall; close the switch to add the summer air-conditioning block on top.", "PROBE: source and the feeder bus. The two effects arrive together, which is the point."},
    [CIRCUIT_GS_FACRATE] = {"FACILITY RATING (NERC FAC-008-5): a circuit's rating is set by its MOST LIMITING ELEMENT, not", "by the conductor. Four series elements each carry their own rating: line conductor 800 kW,", "breaker 20 kW, CT 4 kW, buswork 25 kW (the simulator compares instantaneous power, so it holds", "twice those as peak limits). At 400 A everything is inside its rating; close the switch to push", "500 A and only the CT crosses 100 % - so 500 A is past the path rating even though the conductor", "sits at 63 % and could carry far more. PROBE: the source and the load bus; watch the CT label."},
    [CIRCUIT_GS_KRON] = {"KRON REDUCTION: bulk grid models eliminate zero-injection buses with Y_red = Y_aa -", "Y_ab Y_bb^-1 Y_ba, the Schur complement of the admittance matrix. For a single interior node", "that is exactly the Y-to-delta transform: three 10 ohm arms become three 30 ohm arms", "(R12 = Ra + Rb + Ra Rb / Rc). The two halves here are driven identically and loaded", "identically, and their load voltages match digit for digit - the 'effective resistance", "invariance' that makes the reduction exact. PROBE: both load buses; they overlay perfectly."},
    [CIRCUIT_GS_RX] = {"R/X AND FAST DECOUPLED POWER FLOW: FDPF (Stott-Alsac 1974) assumes lines are almost purely", "inductive, so active power moves the angle and reactive power moves the magnitude, and the", "Jacobian splits into two constant matrices. The top branch is transmission (1 + j11 ohm,", "R/X = 0.09) and the bottom is a distribution feeder (11 + j7.3 ohm, R/X = 1.5). Close each", "reactive block in turn: on the transmission branch the vars dominate the magnitude change; on", "the feeder watts and vars move it about equally - the cross-coupling that makes FDPF diverge."},
    [CIRCUIT_GS_GOVERNOR] = {"GOVERNOR DROOP AND THE SWING EQUATION (NERC BAL-001-TRE-2): ERCOT is an electrical island, so", "it has to arrest its own frequency excursions. This is the standard analog-computer patch:", "U1 integrates 2H/f0 dDf/dt = Pm - Pe - D Df/f0 (H = 4 s, D = 1 pu), U2 inverts the sign, and", "U3 is the 5 % droop with the 0.3 s steam-chest lag. Scale: 1 V = 1 Hz, 1 V = 0.1 pu of power.", "A 0.05 pu load step at 0.2 s gives the nadir, then recovery to -0.05/(1/R + D) = -0.143 Hz.", "PROBE: the load step and the frequency deviation. UFLS starts at 59.3 Hz, load resources 59.7."},
    [CIRCUIT_GS_PIDS] = {"SUPERVISED PERIMETER ZONE (NERC CIP-014-2, layers 2 'detect' and 5 'communicate'): a fence", "sensor is reported to the substation RTU as a dry contact on one twisted pair. The pair is", "supervised so that a cut or a short cannot look like 'all clear': the 5.6k end-of-line resistor", "and the 2.2k zone resistor give four distinct levels at the RTU input - normal 8.5 V, alarm", "9.2 V, cable cut 12 V, short 0 V. The contact opens at 4 s for 3 s. Passive loops and fibre are", "used here because ordinary wireless sensors false-alarm in the EMI around energised HV plant."},
    [CIRCUIT_MOS_IDVGS] = {"MOSFET TRANSFER CURVES: a single 0-4 V triangle drives three gates, and every device sits", "over its own 1 ohm source sense resistor, so each probed node reads I_D directly (1 V = 1 A).", "The three parts are a 2N7000 (Vth 2.1 V, k_n 105 mA/V2), a 2N7002 (1.6 V, 60 mA/V2) and a", "textbook 1 um NMOS (0.7 V, 1.1 mA/V2) - each leaves cutoff at its own threshold and climbs with", "its own k_n. Press the scope's Y-T button for X-Y and CH1 becomes the x axis: real I_D-V_GS curves.", "TRY: select a device and edit Vth, W/L or Kn - the curve moves while you type."},
    [CIRCUIT_MOS_IDVDS] = {"MOSFET OUTPUT CHARACTERISTICS: one 0-6 V triangle sweeps the drains of three identical", "2N7000-class devices held at V_GS = 2.5, 3.0 and 3.5 V. Each source sense resistor is 2 ohm, so", "the probes read 2 x I_D. In X-Y mode (the Y-T button) with the sweep on CH1 you get the classic", "family: a steep triode slope while V_DS < V_OV, then the knee, then the flat saturation region", "whose height goes as (V_GS - Vth)^2 - which is why the 3.5 V curve sits far above the 2.5 V one.", "TRY: raise lambda and saturation stops being flat; that tilt is channel-length modulation."},
    [CIRCUIT_MOS_TUNED] = {"MOSFET SINGLE-TUNED AMPLIFIER: the MOSFET counterpart of the BJT tuned stage. A common-source", "device (1M/330k gate bias, 470 ohm source bypassed by 10 uF) drives an L-C-Rq tank: 1 mH,", "2.53 nF and 10 k, so f0 = 1/(2 pi sqrt(LC)) = 100 kHz. Away from resonance the tank impedance", "collapses and so does the gain; at f0 the gain is g_m (Rq || RL). The source sweeps 20-500 kHz.", "PROBE: input and output. Turn on Trk so the time base follows the sweep.", "TRY: change C to move f0; raise Rq for a narrower, taller peak (higher Q)."},
    [CIRCUIT_MOS_CG] = {"COMMON GATE: the signal drives the SOURCE and the gate is held at AC ground by a 10 uF cap.", "Input resistance is only 1/g_m - a few tens of ohms - so it loads a source badly, but there is", "no Miller capacitance and the output at the drain is IN PHASE with the input. That combination", "makes it the RF input stage and the top half of a cascode.", "PROBE: the input at the source and the output at the drain: same polarity, gain g_m R_D.", "TRY: raise the source resistor and watch the input divide down before it ever reaches the device."},
    [CIRCUIT_MOS_CASCODE] = {"CASCODE: a common-source device with a common-gate device stacked on its drain. The lower", "device sees an almost constant drain voltage because the upper one holds that node, so its", "gate-drain capacitance is never multiplied by the gain - the Miller effect that limits a plain", "common-source stage practically disappears, and the bandwidth goes up with it.", "The pair still delivers the full g_m R_D gain because the upper device passes the current on.", "PROBE: input and the top drain. TRY: compare with the plain Common Source at the same drive."},
    [CIRCUIT_MOS_DIFF] = {"MOSFET DIFFERENTIAL PAIR: both gates sit at 3 V and share a 2.2 k tail resistor. The two", "sources are driven in antiphase (20 mV each), so the tail current is steered from one device", "to the other and the drains swing in opposite directions with gain g_m R_D / 2. Anything", "common to both gates moves the tail node instead and cancels - the front end of every op-amp.", "PROBE: one gate and both drains: two mirror-image swings.", "TRY: drive both gates in phase (set one phase to 0) and watch the output collapse."},
    [CIRCUIT_MOS_MIRROR] = {"MOSFET CURRENT MIRROR: M1 has its gate tied to its drain, so it is forced into saturation and", "the 10 k reference resistor sets its current. M2 shares the same gate-source voltage, so it", "carries the same current into whatever load it is given - within the compliance limit set by", "keeping M2 in saturation (V_DS > V_GS - Vth).", "PROBE: the reference node and the mirrored drain.", "TRY: change the load resistor - the mirrored current barely moves until M2 leaves saturation."},
    [CIRCUIT_CMOS_INV] = {"CMOS INVERTER: a PMOS pull-up and an NMOS pull-down sharing a gate. A 0-5 V triangle on the", "input walks the pair through all five regions, and the output is the voltage transfer", "characteristic: flat at 5 V while the NMOS is off, a steep near-vertical transition where both", "devices are saturated, then flat at 0 V. Press Y-T for X-Y and CH1 becomes the x axis: the VTC", "appears directly. Both devices conduct only in that narrow middle band, which is the crossbar", "current - the reason CMOS burns power while switching and almost none while it sits still."},
    [CIRCUIT_CMOS_NAND] = {"CMOS NAND AT THE TRANSISTOR LEVEL: two PMOS devices in PARALLEL from the rail and two NMOS", "devices in SERIES to ground, all four gates driven by the two inputs. Either input low turns", "on a PMOS and pulls the output up; only both inputs high can turn both NMOS devices on and", "pull it down. That is the NAND function falling straight out of the topology - and the series", "NMOS stack is why a NAND is slower falling than rising, and why wide gates get slow.", "PROBE: A, B and the output. A runs at 1 kHz and B at 500 Hz, so all four input codes appear."},
    [CIRCUIT_CMOS_TGATE] = {"TRANSMISSION GATE vs a LONE NMOS: the same 0-5 V ramp is passed by a complementary pair (NMOS", "gate at Vdd, PMOS gate at ground) and by a single NMOS. The pair passes the whole rail: the", "NMOS carries the low end well and the PMOS takes over at the top. The lone NMOS stops one", "threshold short - it cannot pull its source above Vdd - Vth, so the top of the ramp is clipped", "and its on-resistance rises badly as the signal approaches that limit.", "PROBE: the input and both outputs. That threshold drop is why CMOS switches come in pairs."},
    [CIRCUIT_XY_LISSAJOUS] = {"LISSAJOUS FIGURES: two independent sine sources, each into its own 10 k load, so CH1 is the x", "axis and CH2 the y axis once you press the scope's Y-T button to reach X-Y mode. The shape", "counts the ratio for you: the number of horizontal tangents divided by the number of vertical", "ones is f_y / f_x. 1:1 draws a straight line at 0 deg, a circle at 90 deg and an ellipse in", "between; 1:2 is a figure of eight; 2:3 is the classic pretzel. Before frequency counters this", "was how you measured an unknown frequency against a reference. TRY: edit CH2's frequency/phase."},
    [CIRCUIT_XY_PLOTTER] = {"X-Y PLOTTER: two arbitrary-waveform sources (ARB) replay sample tables 0 and 1 - one for x, one", "for y - so in X-Y mode the scope draws whatever shape those tables describe. Load your own with", "  circuit-playground.exe --xy points.txt", "where the file holds 'x y' pairs one per line (commas and semicolons also work, # lines are", "ignored, up to 2048 points). Both axes are normalised to -1..1 and then scaled by each source's", "amplitude, so any units plot sensibly. The period property sets how fast the shape is traced."},
    [CIRCUIT_HW_BUCK] = {"BUCK CONVERTER: an ideal switch chops the 12 V input at 100 kHz with 50 % duty, the diode", "freewheels the inductor current while the switch is open, and L-C average the result:", "Vout = D x Vin = 6 V. Inductor ripple is (Vin - Vout) D / (L fsw) = 300 mA and the output", "ripple that leaves behind is dI / (8 C fsw) = 3.8 mV.", "PROBE: the switch node (a 0-12 V square) and the output (a quiet 6 V).", "TRY: duty 0.5 -> 0.25 halves the output; L 100 -> 10 uH pushes it into discontinuous mode."},
    [CIRCUIT_HW_BOOST] = {"BOOST CONVERTER: the switch grounds the inductor to charge it, then opens so the inductor's", "collapsing field drives current through the diode into the output. Vout = Vin/(1-D) = 10 V at", "D = 0.5 - the output can only ever sit above the input. The input current is continuous but the", "output current arrives in pulses, which is why a boost needs far more output capacitance than a", "buck of the same power. PROBE: the switch node and the output.", "TRY: D 0.5 -> 0.75 gives 20 V; watch the input current stay smooth while the output pulses."},
    [CIRCUIT_HW_BUCKBOOST] = {"BUCK-BOOST: the inductor is the only path from input to output, and it is connected the other", "way round when it discharges - so the output is INVERTED: Vout = -D/(1-D) x Vin = -12 V at", "D = 0.5. Below D = 0.5 it steps down, above it steps up, which is what makes it useful when the", "input can be either side of the output. Neither the input nor the output current is continuous,", "so both ends need filtering. PROBE: the switch node and the negative output.", "TRY: D -> 0.67 for -24 V, D -> 0.33 for -6 V."},
    [CIRCUIT_HW_CUK] = {"CUK CONVERTER: energy crosses from input to output through the 47 uF capacitor rather than", "through the inductor, and there is an inductor on BOTH sides. That makes the input and the", "output current continuous - the quietest of the basic topologies, at the cost of a capacitor", "that has to carry the full transfer current. Like the buck-boost the output is inverted:", "Vout = -D/(1-D) x Vin = -12 V at D = 0.5. NOTE: this model settles on the right mean but its", "ripple is coarser than a real Cuk - the transfer loop wants a finer step than the preset. See ROADMAP."},
    [CIRCUIT_HW_INTERLEAVED] = {"TWO-PHASE INTERLEAVED BUCK: two 100 kHz buck stages sharing one output, switched 180 degrees", "apart. Each carries half the load, and because their inductor ripples are out of phase they", "partly cancel: the output sees ripple at 200 kHz with a much smaller amplitude than one phase", "would give. That is why every CPU and GPU rail is multiphase. Coupling the two inductors into", "one magnetic part - a coupled-inductor voltage regulator (CLVR) - cancels more of it again and", "shrinks the magnetics. TRY: set phase B's pulse delay to 0 and watch the ripple double."},
    [CIRCUIT_HW_PDN] = {"POWER DELIVERY NETWORK: a 1.8 V rail reaches the die through 20 mOhm of resistance and 2 nH of", "plane and via inductance, with 100 uF of bulk (20 mOhm ESR) and 1 uF of ceramic (5 mOhm) along", "the way. A 0.9 A load step every 80 us asks the network what it can actually deliver.", "The first nanoseconds belong to the ceramic - the plane inductance blocks everything upstream -", "then the bulk takes over, and only later the regulator. That is the whole reason decoupling is a", "hierarchy. TRY: delete the ceramic (sharp spike) or raise the plane inductance to 20 nH."},
    [CIRCUIT_HW_CAPS] = {"INPUT vs OUTPUT CAPACITANCE: two identical rails, the same 1 A load step, and the same 100 uF -", "the only difference is which side of the 1 uH lead inductance the capacitor sits on. On the", "source side it is useless against the step: the inductance stands between it and the load. On", "the load side it holds the rail up until the source can respond. This is why decoupling goes AT", "the part it feeds, and why an input capacitor's job is to keep the SOURCE quiet, not the load.", "PROBE: both rails. The two traces are the same experiment with one thing moved."},
    [CIRCUIT_ID_SOURCE] = {"IDEAL vs REAL SOURCE: three copies of a 5 V supply into a load. The ideal one has no",
        "internal resistance, so its terminal voltage is 5.000 V whatever it feeds. The other two are the",
        "same 5 V behind r = 200 ohm: into 1k that divider gives 5 x 1000/1200 = 4.167 V, and into 100 ohm",
        "only 5 x 100/300 = 1.667 V. The source has not changed - the LOAD decided the terminal voltage.",
        "This is Thevenin from the source's side: every real supply, battery and signal generator is a",
        "voltage behind a resistance. PROBE: all three loads. Set r_series or tick Ideal to move between them."},
    [CIRCUIT_ID_DIODE] = {"IDEAL vs REAL DIODE: the same 1 Vpk 1 kHz half-wave rectifier twice. The ideal diode is a",
        "switch behind a 0.7 V battery: nothing at all until 0.7 V, then a hard 0.7 V drop, so the peak is",
        "1.0 - 0.7 = 0.30 V. The real one is Shockley, I = Is(exp(V/nVt) - 1): it is already passing",
        "hundreds of microamps at 0.5 V, so it turns on softly around 0.52 V and the peak reaches ~0.48 V.",
        "The ideal model is the one to use for a mental estimate; it is wrong exactly where the interesting",
        "circuits live (log amps, temperature sensors, small-signal detectors). PROBE: both loads."},
    [CIRCUIT_ID_CAP] = {"IDEAL vs REAL CAPACITOR (ESR): a +/-5 V 20 kHz square through 100 ohm into 5 uF. The current is",
        "a 50 mA square, so the ideal capacitor integrates it into a triangle: dV = I(T/2)/C = 250 mVpp.",
        "A real capacitor is C in series with its ESR, and that resistance turns the current square straight",
        "into a voltage square ON TOP of the triangle: +/-25 mV at 0.5 ohm, +/-100 mV at 2 ohm.",
        "This is why a supply's output ripple barely improves when you add capacitance but collapses when",
        "you pick a low-ESR part. ESL does the same to the fast edges. PROBE: all three capacitors."},
    [CIRCUIT_ID_IND] = {"IDEAL vs REAL INDUCTOR (DCR): a 5 V step into a series R-L-C, 10 ohm + 10 mH + 1 uF, so",
        "f0 = 1/(2 pi sqrt(LC)) = 1592 Hz. With an ideal inductor the only loss is the 10 ohm resistor:",
        "zeta = (R/2) sqrt(C/L) = 0.05 and the capacitor overshoots to 1.85 x 5 V = 9.3 V, ringing for many",
        "cycles. Give the winding its real 50 ohm DCR and zeta = 0.30: the peak drops to 6.9 V and the ring",
        "is gone in three cycles. An ideal inductor makes any L-C loop ring for ever, which is exactly how",
        "switching converters run away in a simulator that ignores DCR. PROBE: both capacitors."},
    [CIRCUIT_ID_OPAMP] = {"IDEAL vs REAL OP-AMP: three non-inverting stages, gain 1 + 9k/1k = 10, at 100 kHz. The ideal",
        "op-amp has infinite gain and bandwidth, so 50 mVpk comes out as 500 mVpk however fast you drive it.",
        "A real part has a gain-bandwidth product: at Acl = 10 a 1 MHz GBW leaves 100 kHz of bandwidth, so",
        "at exactly 100 kHz the output is 3 dB down - 354 mV, not 500. Row 3 asks the same part for 5 V at",
        "100 kHz, which needs 3.1 V/us; it can only do 0.5 V/us, so the sine comes out as a TRIANGLE about",
        "1.25 V tall. Bandwidth is a small-signal limit, slew rate a large-signal one. PROBE: all three."},
    [CIRCUIT_ID_BJT] = {"IDEAL vs REAL BJT (Early effect): the same fixed-bias stage twice. 1.13 M from 12 V sets",
        "I_B = (12 - 0.7)/1.13M = 10 uA, and beta = 100 gives I_C = 1 mA, so V_C = 12 - 1 mA x 4.7k = 7.3 V.",
        "That is the textbook answer, and the ideal model gives it. A real transistor's collector current",
        "also rises with V_CE - the Early effect, I_C = I_S exp(V_BE/V_T)(1 + V_CE/V_AF) - so with V_AF = 80 V",
        "the current is ~9 % higher and the collector sits lower. It is the same effect that sets a stage's",
        "output resistance r_o = V_AF/I_C, so it is not a rounding error. PROBE: both collectors."},
    [CIRCUIT_TLINE_REAL] = {"TRANSMISSION LINE WITH A REAL DELAY: one 50 ohm, 5 ns cable, ended three ways. The",
        "Delay Line part is not a ladder of Ls and Cs approximating a cable - it records what each end",
        "launches and hands it to the other end one delay later, which is Bergeron's method and is exact",
        "for a lossless line at any time step. So the delay is a property of the cable, not of the solver:",
        "the far end sits at exactly zero for 5 ns and then steps. Matched, it arrives once and stays;",
        "shorted it comes back inverted at 10 ns; open it doubles. PROBE: the far end of the matched line."},
    [CIRCUIT_SEVENSEG_TEST] = {"7-SEGMENT SEGMENT TEST: the eight segments of a common-cathode display, each on its own",
        "switch and its own 150 ohm resistor from a 5 V rail. All eight are closed on load, so the digit",
        "reads 8 with its decimal point; open any switch and that one segment goes out and the rest do not",
        "care. Each segment is an ordinary LED to the shared cathode - about 2.2 V forward at 19 mA - so",
        "the brightness on screen follows the current through it, and dropping the rail dims all eight together.",
        "PROBE: segment a's pin, which sits at the forward drop while it is lit and near zero when it is not."},
    [CIRCUIT_WIRELESS_LINK] = {"WIRELESS LINK: what the TX and RX antenna parts actually do. TX measures the voltage",
        "across its own two terminals and publishes it on a channel number; RX, set to the same channel,",
        "becomes a source of that voltage. There is no distance, no path loss and no carrier - the channel",
        "is just a name the two of them share, and changing either one silences the link. Both present",
        "50 ohm, so a 50 ohm load gets half of what was sent: 2 V in, 1 V across the load.",
        "PROBE: the load resistor. Set both channels to 1 and it goes quiet; set them both back and it returns."},
    [CIRCUIT_BCD_COUNTER] = {"BCD COUNTER DRIVING A 7-SEGMENT DISPLAY: a clock, a decade counter, a decoder and a",
        "digit. Every rising clock edge steps the counter 0,1,2..9 and back to 0; the decoder turns",
        "that four-bit value into the seven segments that spell it. This is the real test of the",
        "display, because ten counts light every segment in a different combination - a segment that",
        "is stuck or mis-wired reads as a wrong digit rather than as a dark bar you have to hunt for.",
        "PROBE: segment a, which is lit for every digit except 1 and 4. CARRY is the pin that chains digits."},
    [CIRCUIT_DIGITAL_CLOCK] = {"DIGITAL CLOCK: six digits of the same counter-decoder-display block, one pulse a second",
        "into the right-hand one, and every digit clocking the one to its left when it rolls over. The",
        "moduli do most of the work - 10 and 6 give 0-59 twice over - but the hours would run to 29 on",
        "10 and 3, so an AND gate watching for tens=2 with units=4 resets both hour digits the instant",
        "they read 24. That gate is the entire difference between a chain of counters and a clock.",
        "PROBE: segment a of the seconds digit. Raise the speed to watch the minutes and hours turn over."},
    [CIRCUIT_IV_CAP_ENERGY] = {"THE TWO-CAPACITOR PROBLEM: 100 uF charged to 10 V is switched onto an equal empty one.",
        "Charge is conserved, so both settle at 5 V. Energy is not: 1/2 C V^2 was 5 mJ and is now 2 x 1/2 x",
        "100 uF x 25 = 2.5 mJ. Half has gone - and it goes for ANY resistance, including the 1 ohm copy that",
        "finishes in 100 us, because the integral of i^2 R dt does not depend on R: a smaller R raises the",
        "current by exactly as much as it shortens the time. At R = 0 it leaves as radiation instead. The",
        "same algebra says charging any cap from a fixed voltage wastes half of what the source delivered."},
    [CIRCUIT_IV_MILLER] = {"THE MILLER EFFECT: a capacitor between the input and the output of an inverting stage is",
        "not C at the input. While the input rises by v the drain falls by A v, so the charge that has to be",
        "supplied is C (1 + A) v - the input sees C (1 + A). Here a harmless-looking 10 pF of C_gd on a stage",
        "with a gain of 12 becomes 130 pF, and against a 10 k source that is a pole at 122 kHz: at 1 MHz the",
        "second stage is down to a seventh of the first, which still has all of it. This is why gain and",
        "bandwidth trade against each other in one stage, and why a cascode holds the drain still to stop it."},
    [CIRCUIT_IV_SWITCH_CHOICE] = {"BJT OR MOSFET AS A SWITCH: same 12 V, same 100 ohm load, same 5 V of logic. The 2N3904",
        "saturates at 0.07 V here whatever the current, and costs 9 mA of base current for as long as it is",
        "on. The 2N7000 drops I x R_DS(on), and at V_GS = 5 V that is about 3.4 ohm - 0.41 V, not the 0.14 V",
        "the 1.2 ohm on the data sheet would suggest, because 1.2 ohm is quoted at V_GS = 10 V. So the BJT",
        "wins on drop here and loses badly at high current, costs standby current and nothing per edge; the",
        "MOSFET is the other way round. 'Logic level' is a specification, and 5 V is not 10 V."},
    [CIRCUIT_IV_INRUSH] = {"HOT-PLUG INRUSH: an empty capacitor is a short circuit, and the only thing between it and",
        "12 V is whatever series resistance happens to exist. With 50 mohm of connector and wiring that is",
        "240 A for the first 50 us. The energy is not the problem - 1/2 C V^2 = 72 mJ either way - the peak",
        "current is: 240 A through a connector rated for 5 A pits the contacts every time, and the rail sag",
        "browns out everything else on the board. 4.7 ohm in series makes it 2.5 A and charges the cap in",
        "20 ms. That is an inrush limiter: an NTC that falls to a few tenths once warm, or a ramped MOSFET."},
    [CIRCUIT_IV_TERMINATION] = {"TERMINATION: the same 3.3 V driver into the same 50 ohm line, ended three ways.",
        "NONE: the far end is open, the step reflects in full, and the driver sees it back one round trip",
        "later - 10 ns here. SERIES 33 ohm at the source: the driver's own 25 ohm plus 33 matches the line,",
        "so whatever comes back is absorbed there; the far end still shows the full 3.3 V, because incident",
        "plus reflected is exactly that. PARALLEL 50 ohm at the load: nothing reflects at all, but the",
        "receiver only sees 2.2 V and the driver holds 44 mA the whole time it is high. INTERVIEW: the trade."},
    [CIRCUIT_IV_PULLUP_SIZING] = {"PULL-UP SIZING: one open-drain bus with 400 pF on it, three pull-ups. An open-drain pin",
        "can only pull down, so the rising edge is the pull-up charging that capacitance: 10 % to 90 % takes",
        "2.2 R C. 10 k gives 8.8 us and sinks 330 uA; 4.7 k gives 4.1 us and 700 uA; 1 k gives 880 ns and",
        "3.3 mA. The whole choice is that trade, and I2C specifies both ends of it - 300 ns maximum rise for",
        "fast mode, 3 mA maximum sink - which is also why the specification caps the bus at 400 pF, because",
        "with more than that no resistor satisfies both. INTERVIEW: 'why 4.7 k?' It is a compromise, not a law."},
    [CIRCUIT_IV_GROUND_BOUNCE] = {"GROUND BOUNCE: a driver charging 100 pF to 3.3 V through 10 ohm pulls 330 mA for about a",
        "nanosecond, and all of it goes home through the 5 nH of bond wire and via that every pin on the die",
        "shares. L di/dt swings that local ground 2.2 V pk-pk. Nothing is broken - but every other signal",
        "on the die is referred to the lifted ground, so a pin that is holding LOW appears to pulse, and a",
        "receiver with a 0.8 V threshold may believe it. The fixes are all the same fix: fewer ways for the",
        "current to be shared - more ground pins, shorter returns, a plane instead of a trace, slower edges."},
    [CIRCUIT_IV_CROSSTALK] = {"CROSSTALK: one aggressor edge, 2 pF of coupling, two victims that differ only in what holds",
        "them. The coupled charge is identical: C_m dV = 2 pF x 3.3 V = 6.6 pC. Into a victim with 5 pF of",
        "its own capacitance and only a 10 k pull-down to drain it, that is 3.3 x 2/7 = 0.94 V - a logic",
        "level - and it takes 70 ns to bleed away. Into a 10 ohm driver the same charge is gone in 70 ps.",
        "nanosecond. The lesson is not 'avoid coupling': it is that coupling becomes a fault only when the",
        "victim's impedance lets it. Drive your quiet nets, and never leave an input floating."},
    [CIRCUIT_IV_ESD_CLAMP] = {"ESD CLAMPS: every CMOS input has a diode to each rail, and they exist to survive a strike -",
        "not to be a voltage clamp you design around. Drive 6 V into a 3.3 V pin through 1 k and the pin sits",
        "at 4.0 V with 2.7 mA flowing INTO the 3.3 V rail: more than the 10 uA to 20 mA of injection a data",
        "sheet allows, and on a lightly loaded board that current alone can pull the whole supply up. This is",
        "how a live signal back-powers a board that is switched off, through one input pin. Through 220 k the",
        "same 6 V injects 12 uA, the pin still reads a solid high, and the only cost is bandwidth."},
    [CIRCUIT_IV_BUCK_NODES] = {"DISCRETE BUCK, NODE BY NODE: 12 V, 50 % duty at 50 kHz, 5.5 V out, built from real parts",
        "instead of an ideal switch: an IRF9540N, an NPN to drive it and a Schottky to catch the current.",
        "GATE sits at 12 V and the NPN pulls it to 0.2 V, so Vgs = -11.8 V and the PMOS turns on. The 1 k",
        "gate resistor is why the edges are not instant, and it burns 12 mA whenever the NPN is on. SWITCH",
        "NODE is an 11.3 V square that drops to -0.4 V when the Schottky freewheels - the one hard-switching",
        "node here. L turns that square into a 270 mA triangle; C turns the triangle into 7 mV of ripple."},
    [CIRCUIT_IV_LDO_VS_BUCK] = {"LDO vs SWITCHER: the same job - 12 V in, 5 V out, 1 A - done both ways, with an ammeter",
        "in each input so you can read what each one takes from the rail. The linear regulator draws the",
        "same 1 A it delivers and turns the other 7 V into 7 W of heat: efficiency is just Vout/Vin = 42 %.",
        "The switcher moves power, not voltage, so 5 W out of 12 V is about 440 mA in - around 90 %, no",
        "heatsink, at the cost of an inductor and 100 kHz of ripple. INTERVIEW: 'when would you still use",
        "an LDO?' Small drop, small current, or a load that hates ripple - and often a buck then an LDO."},
    [CIRCUIT_IV_BOOTSTRAP] = {"BOOTSTRAP HIGH-SIDE DRIVE: an N-channel on the high side needs its gate ABOVE the rail,",
        "because its source is the switch node. C_boot floats on that node: while SW is low the diode",
        "charges it to 11.5 V from the 12 V supply, and when SW rises to 12 V the whole capacitor rides up,",
        "putting BOOT at 23.5 V - still 11.5 V above the source, which is what the gate needs. The catch is",
        "in the second copy: hold SW high and the diode never conducts again, the driver's own current",
        "empties the cap, and the high side turns itself off. Hence a maximum duty cycle, never 100 %."},
    [CIRCUIT_IV_PROBE_COMP] = {"PROBE COMPENSATION: a 10x probe is a 9M/1M divider, and a resistive divider is only",
        "flat if the stray capacitance across each half divides the same way. The trimmer sets the",
        "probe's own C so that 9M x Cp = 1M x 15 pF -> 1.67 pF. Under-compensated the edge rounds",
        "with tau = 14 us; over-compensated it overshoots to 0.9 V and decays. Both are the probe,",
        "not the circuit. Compensate on the CAL output in 10x - in 1x the trimmer is not in the",
        "path. INTERVIEW: 'the scope shows ringing on a slow signal' - check this first."},
    [CIRCUIT_IV_PROBE_LOADING] = {"PROBE LOADING: the probe is a capacitor you attach to the node. A 1 MHz square out of",
        "10 k sees 5 pF of board stray on its own: tau = 50 ns. Hang a 1x probe (1M || 100 pF) on it",
        "and tau becomes 1.05 us - the square is a triangle and the circuit never changed. A 10x probe",
        "is 12 pF: tau = 170 ns, loaded but usable. That is the whole reason 10x is the default and",
        "why you do not leave a probe in 1x for the bigger picture. INTERVIEW: 'why does the edge get",
        "slower when you probe it' - and 'what would you use instead' (active or FET probe)."},
    [CIRCUIT_IV_GROUND_LEAD] = {"GROUND LEAD RINGING: the signal goes down the probe and the return comes back through",
        "the ground lead, and that loop has inductance. With the probe tip's 12 pF it is an LC tank at",
        "f = 1/(2 pi sqrt(L C)). A 6 inch clip is about 150 nH: 119 MHz, right in the band you came to",
        "measure, so every fast edge appears to ring. A half-inch spring tip is 15 nH: 375 MHz, above",
        "the probe's own bandwidth, and the ring disappears. Nothing on the board changed. INTERVIEW:",
        "'you see 100 MHz ringing on a 3.3 V edge - is it real?' Move the ground and find out."},
    [CIRCUIT_IV_SCOPE_INPUT_Z] = {"SCOPE INPUT IMPEDANCE: a generator marked 1 V is 1 V INTO 50 OHMS - internally it is a",
        "2 V source behind 50 ohms. Terminate the cable in the scope's 50 ohm input and you read 1 V.",
        "Leave the scope on 1 M and the far end of the cable is open: the step reflects, adds to itself",
        "and you read exactly twice what it is sending - 2 V for a generator set to 1. The",
        "cable is a real 50 ohm line with 5 ns of propagation delay, so the reflection arrives when it",
        "arrives. INTERVIEW: 'the generator reads double' - and 'when may you NOT use 50 ohms?'"},
    [CIRCUIT_IV_AC_COUPLING] = {"AC COUPLING: 200 mVpp of ripple on a 12 V rail. DC-coupled you must fit 12 V on screen, so",
        "at 5 V/div the ripple is a twentieth of a division and simply is not there. AC coupling puts",
        "the scope's own 0.1 uF in series with its 1 M input - a high-pass at 1.6 Hz - which throws the",
        "DC away so the gain can go to 50 mV/div and 200 mVpp becomes four divisions. The cost is that",
        "the DC level is gone and anything under a few Hz is attenuated and phase-shifted. INTERVIEW:",
        "'how would you measure 20 mV of ripple on a 12 V rail?' AC couple, and limit the bandwidth."},
    [CIRCUIT_IV_SHUNT_SENSE] = {"CURRENT SENSE: 1 A through 100 mohm is 100 mV either way, but where the shunt goes decides",
        "everything else. LOW SIDE (in the return) is single-ended and cheap, but the load's ground now",
        "sits 100 mV above real ground and a short from the load to ground is invisible - no current",
        "flows in the shunt. HIGH SIDE keeps the load grounded and sees that short, but the 100 mV rides",
        "on 12 V of common mode, so it needs a difference amp and the CMRR of that amp becomes your",
        "accuracy. Both pay the same burden voltage. INTERVIEW: asked at TI and Apple, almost verbatim."},
    [CIRCUIT_IV_KELVIN] = {"4-WIRE (KELVIN) SENSING: 1 A forced through a 10 mohm shunt whose leads are 50 mohm each.",
        "Measure at the connector and you read 110 mV: 110 mohm, eleven times the part, and the part is",
        "the smallest thing in the measurement. Land two more wires directly on the resistor body and",
        "feed them to a 10 M input: they carry no current, so their own 50 mohm drops nothing, and the",
        "meter reads the 10 mV that is really across the part. This is why a milliohm meter and an LCR",
        "bridge have four terminals, and why sense pads sit inside force pads. INTERVIEW: NI classic."},
    [CIRCUIT_PIERCE] = {"PIERCE CRYSTAL OSCILLATOR: an inverting amplifier with a pi network - C2, the crystal",
        "and C1 - closing the loop. The crystal is one component with a real quartz model: a motional",
        "arm (Ls 100 mH, Cs 25.33 pF, Rs 200) resonating at fs = 100.0 kHz with a Q of 314, in parallel",
        "with the 33 pF of its holder. Between fs and the parallel resonance fp the arm looks INDUCTIVE,",
        "and that band is the only place the pi network gives the 180 deg the inverter needs - so it",
        "settles at 100.5 kHz, pulled off fs by the load caps. PROBE: op-amp output, a clipped square."},
    [CIRCUIT_NE555_ASTABLE] = {"555 ASTABLE: the 555 here is a real subcircuit, not a box - inside it are the three 5k",
        "divider resistors that set 1/3 and 2/3 of the supply, the two comparators that watch",
        "TRIGGER and THRESHOLD against them, the NOR latch they drive, and the discharge transistor.",
        "Outside, C charges from V+ through R_A + R_B and discharges through R_B into the DISCH pin,",
        "so it oscillates between 1/3 and 2/3 of the supply: f = 1.44/((R_A + 2 R_B) C) = 4.8 kHz",
        "with 10k, 10k and 10 nF. Open the block with Ctrl+G's library to see the parts. PROBE: OUT."},
    [CIRCUIT_CAP_DCBIAS] = {"CERAMIC DC BIAS: three copies of the same 10 uF 6.3 V X5R, each fed the same 25 mA",
        "ripple current, each sitting on a different DC bias. A class-II ceramic loses capacitance",
        "as the voltage across it rises - this one is down to half at 2 V - so the ripple grows:",
        "62 mVpp with no bias, 125 mV at 2 V, 219 mV at 5 V. The part is still marked 10 uF.",
        "This is why a rail decoupled with '22 uF' can ripple like 6 uF, and why a designer picks",
        "a bigger case size or a higher voltage rating. C0G/NP0 parts do not do this. PROBE: all three."},
    [CIRCUIT_PARTS_MOSFET] = {"NAMED PARTS: the same low-side switch three times, with three real MOSFETs from the",
        "library. Each gate is held at 10 V - the voltage their data sheets specify R_DS(on) at -",
        "so the drop across each device is 12 V x R_DS(on) / (R_load + R_DS(on)): 143 mV for the",
        "2N7000 (1.2 ohm), 233 mV for the 2N7002 (2 ohm) and 5 mV for the IRF540N (44 mohm).",
        "CLOSE THE SWITCH to halve the first load: twice the current, twice the drop, four times",
        "the heat in the device. Pick a part in the properties panel. PROBE: all three drains."},
    [CIRCUIT_ID_OPAMP_ERR] = {"OP-AMP ERROR SOURCES AT DC: two x100 non-inverting stages with their inputs grounded",
        "through a source resistance, so whatever comes out is error. The ideal part gives 0.000 V; the",
        "real one has V_os = 1 mV and I_B = 100 nA, and the bias current has to flow through whatever",
        "the input sees: V_out = 100 (V_os - I_B (R_s - R1||Rf)) = 100 (1 mV - 9.9 mV) = -0.89 V.",
        "CLOSE THE SWITCH to short 99k of it: R_s becomes 1k, almost exactly the 990 ohm the other input",
        "sees, the bias errors cancel and only the offset is left (+0.10 V). PROBE: both outputs."},
    [CIRCUIT_ID_MOSFET] = {"IDEAL vs REAL MOSFET (channel-length modulation): common source, V_GS = 3 V, V_th = 1.5 V,",
        "K = u_Cox(W/L) = 2 mA/V^2. The square law gives I_D = K V_ov^2/2 = 2.25 mA and V_D = 12 - 2.2k x I_D",
        "= 7.05 V, and with lambda = 0 that is exactly what the left device does. Switch lambda to 0.05 /V and",
        "I_D picks up a (1 + lambda V_DS) term: the operating point moves to 5.65 V, a 1.4 V shift.",
        "lambda is also what gives the device a finite output resistance r_o = 1/(lambda I_D) - without it the",
        "saturation curves are flat and a current mirror would be perfect. PROBE: both drains."},
    [CIRCUIT_HW_MATCH] = {"IMPEDANCE MATCHING: one 2 Vpk source behind 50 ohm, feeding 5 ohm, 50 ohm and 500 ohm.", "The load voltage rises with R_L, but the load POWER does not: V^2/R gives 33 mW, 20 mW and", "3.3 mW... wait - work it through and the matched 50 ohm load takes the most, because power is", "(V_s R_L / (R_s + R_L))^2 / R_L, which peaks at R_L = R_s.", "Maximum power transfer is a match, not the biggest or smallest load. Maximum voltage would want", "an open circuit and maximum current a short; neither delivers power. PROBE: all three loads."},
    [CIRCUIT_HW_REFLECT] = {"SIGNAL REFLECTIONS: a 50 ohm line with 400 ns of one-way delay, driven from 50 ohm. The",
        "line is a real one - Bergeron's method, with the delay carried in the part's own history - so the",
        "far end sits at exactly zero for 400 ns and then steps, which an L-C ladder cannot do. With the",
        "far-end switch OPEN the line is unterminated: the edge runs to the end, reflects with the same",
        "sign and comes back, so the driver end shows the classic staircase and the far end doubles the",
        "incident step. CLOSE the switch for a matched 50 ohm end and the reflection disappears."},
    [CIRCUIT_HW_LOOP] = {"LOOP STABILITY AND PHASE MARGIN: two identical inverting stages (gain -10) driving 1 nF through", "1 k. The load pole and the amplifier's own pole both sit inside the loop, so by the time the", "loop gain reaches unity the phase has fallen close to -180 degrees: little phase margin, and the", "step response rings. The lower copy has 100 pF across the feedback resistor, which adds a zero,", "returns phase before crossover and damps the ringing - at the cost of bandwidth.", "PROBE: both outputs on the same step. Use the Bode button to see the margin directly."},
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
    add_label(circuit, x + 20, y - 80, "Ferranti rise: 345 kV, 200 mi pi line, open end - close SW for the shunt reactor");
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
        add_label(circuit, x + 80, ry - 70, names[row]);   /* right of the bus at x+60, in the gap above the row it names */
    }
    add_label(circuit, x + 20, y - 100, "Line model ladder: 138 kV, 30 mi, 90 MW - probe the three load buses");
    return 13;
}

// Fundamentals: a battery, the wire's own resistance and the load. V_load = V R_load / (R_wire + R_load).
static int place_dc_line_drop(Circuit *circuit, float x, float y) {
    Component *v = add_comp(circuit, COMP_DC_VOLTAGE, x, y + 60, 0);                   // +(0,20) -(0,100)
    if (!v) return 0;
    v->props.dc_voltage.voltage = 12.0;
    Component *g0 = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    Component *rw = add_comp(circuit, COMP_RESISTOR, x + 100, y + 20, 0);               // (60,20)-(140,20) wire
    rw->props.resistor.resistance = 1.0; rw->props.resistor.power_rating = 5.0;         // 1.2 W in the wire: a 5 W part
    Component *rl = add_comp(circuit, COMP_RESISTOR, x + 200, y + 60, 90);              // (200,20)-(200,100) load
    rl->props.resistor.resistance = 10.0; rl->props.resistor.power_rating = 25.0;       // 12 W load: 25 W wirewound
    Component *g1 = add_comp(circuit, COMP_GROUND, x + 200, y + 120, 0);
    Component *rw2 = add_comp(circuit, COMP_RESISTOR, x + 100, y + 120, 0);             // (60,120)-(140,120) return wire (drawn, grounded both ends)
    rw2->props.resistor.resistance = 1.0; rw2->props.resistor.power_rating = 5.0;
    add_label(circuit, x + 20, y - 40, "Line drop basics: 12 V, 1 ohm wire, 10 ohm load -> 10.9 V (I = 1.09 A)");
    add_label(circuit, x + 30, y + 160, "(return conductor shown for the picture; both ends are the 0 V reference)");
    connect_terminals(circuit, v, 1, g0, 0);
    connect_terminals(circuit, rl, 1, g1, 0);
    int sp = TN(x, y + 20), wl = TN(x + 60, y + 20), wr = TN(x + 140, y + 20), lt = TN(x + 200, y + 20);
    TW(sp, wl); TW(wr, lt);
    /* the ground symbols' terminals are 20 px above their origin: at y + 100, not y + 120.
       gnd1 used to be the latter, which is outside the 5 px merge, so the return conductor's
       far end sat on a node of its own with nothing else on it. */
    int gl = TN(x + 60, y + 120), gr = TN(x + 140, y + 120), gnd0 = TN(x, y + 120), gnd1 = TN(x + 200, y + 100);
    TW(gnd0, gl); TW(gr, TN(x + 140, y + 100)); TW(TN(x + 140, y + 100), gnd1);   /* up to the ground terminal, orthogonally */
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
    u->props.opamp.ideal = true;                  // open-loop comparator: finite gain (1e5) + hard rails, algebraic
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
    rf->props.resistor.resistance = 13.0;                                        // 13 || 13.3 = 6.6 ohm -> 1200 A rms
    Component *sw = fault_switch(circuit, x + 340, y + 220, 0.040, 0.060, 0.200); // IN (300,220) OUT (380,220)
    Component *gsw = add_comp(circuit, COMP_GROUND, x + 380, y + 240, 0);
    int hold = peak_hold(circuit, x + 260, y + 20, 10e-6, 10e3);                 // tau 100 ms -> (420,20)
    int plus = TN(x + 560, y + 60), c1 = TN(x + 480, y + 20), c2 = TN(x + 480, y + 60);
    TW(hold, c1); TW(c1, c2); TW(c2, plus);
    Component *u = comparator_with_ref(circuit, x + 600, y + 40, 8.0, plus);     // -(560,20) +(560,60) out(640,40)
    (void)u;
    add_label(circuit, x + 20, y - 150, "CT + 50/51 overcurrent: 600 A normal, fault 1200 A at t = 40-100 ms (repeats)");
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
    add_label(circuit, x + 20, y - 110, "87 line differential: internal fault (100-160 ms) trips, through fault (240-300 ms) does not");
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
    Component *rrep = add_comp(circuit, COMP_RESISTOR, x + 260, y + 60, 90);     // replica (260,20)-(260,100)
    rrep->props.resistor.resistance = 3.35;
    Component *grep_ = add_comp(circuit, COMP_GROUND, x + 260, y + 120, 0);     // clear of the line at y+140
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
    u->props.opamp.ideal = true; u->props.opamp.gain = 1e5;   /* algebraic model, finite gain: see sat_opamp */
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
    int s1 = TN(x + 200, y + 20), s2 = TN(x + 200, y + 60), rt = TN(x + 260, y + 20), rj = TN(x + 320, y + 20);
    TW(s1, rt); TW(rt, rj);   // the replica resistor moved left, clear of the line below
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
    Component *trip = add_comp(circuit, COMP_PULSE_SOURCE, x, y + 60, 0);        // +(0,20) -(0,100)
    if (!trip) return 0;
    trip->props.pulse_source.v_low = 0; trip->props.pulse_source.v_high = 5; trip->props.pulse_source.delay = 0.050; trip->props.pulse_source.pulse_width = 0.300; trip->props.pulse_source.period = 0.600;
    Component *g0 = add_comp(circuit, COMP_GROUND, x, y + 120, 0);
    Component *cur = add_comp(circuit, COMP_PULSE_SOURCE, x, y + 220, 0);        // +(0,180) -(0,260)
    cur->props.pulse_source.v_low = 0; cur->props.pulse_source.v_high = 5; cur->props.pulse_source.delay = 0.050; cur->props.pulse_source.pulse_width = 0.300; cur->props.pulse_source.period = 0.600;
    Component *g1 = add_comp(circuit, COMP_GROUND, x, y + 280, 0);
    Component *and1 = add_comp(circuit, COMP_AND_GATE, x + 120, y + 40, 0);      // A(80,20) B(80,60) OUT(160,40)
    Component *rt = add_comp(circuit, COMP_RESISTOR, x + 220, y + 40, 0);        // (180,40)-(260,40)
    rt->props.resistor.resistance = 10e3;
    Component *ct = add_comp(circuit, COMP_CAPACITOR, x + 260, y + 80, 90);      // (260,40)-(260,120)
    ct->props.capacitor.capacitance = 15e-6;
    Component *gc = add_comp(circuit, COMP_GROUND, x + 260, y + 140, 0);
    int plus = TN(x + 360, y + 80), ctop = TN(x + 260, y + 40), c1 = TN(x + 300, y + 40), c2 = TN(x + 300, y + 80);
    TW(ctop, c1); TW(c1, c2); TW(c2, plus);
    Component *u = comparator_with_ref(circuit, x + 400, y + 60, 3.16, plus);   // -(360,40) +(360,80) out(440,60), load at (480,60..140)
    Component *and2 = add_comp(circuit, COMP_AND_GATE, x + 600, y + 70, 0);      // A(560,50) B(560,90) OUT(640,70)
    Component *rl = add_comp(circuit, COMP_RESISTOR, x + 700, y + 110, 90);      // (700,70)-(700,150)
    rl->props.resistor.resistance = 100e3;
    Component *gl = add_comp(circuit, COMP_GROUND, x + 700, y + 170, 0);
    add_label(circuit, x + 20, y - 140, "50BF: TRIP at 50 ms, breaker current still present -> BFT 150 ms later (stuck breaker)");
    add_label(circuit, x + 720, y + 60, "BFT");
    add_label(circuit, x + 20, y + 310, "TRIP (top pulse) and 50BF current detector (bottom pulse); shorten the 50BF pulse to 83 ms for a healthy breaker");
    // wiring
    int tp = TN(x, y + 20), ga = TN(x + 80, y + 20); TW(tp, ga);
    int cp = TN(x, y + 180), b1 = TN(x + 60, y + 180), b2 = TN(x + 60, y + 60), gb = TN(x + 80, y + 60); TW(cp, b1); TW(b1, b2); TW(b2, gb);
    int o1 = TN(x + 160, y + 40), rl0 = TN(x + 180, y + 40); TW(o1, rl0);
    trip->node_ids[0] = tp; cur->node_ids[0] = cp;
    connect_terminals(circuit, trip, 1, g0, 0);
    connect_terminals(circuit, cur, 1, g1, 0);
    and1->node_ids[0] = ga; and1->node_ids[1] = gb; and1->node_ids[2] = o1;
    rt->node_ids[0] = rl0; rt->node_ids[1] = ctop; ct->node_ids[0] = ctop;
    connect_terminals(circuit, ct, 1, gc, 0);
    int uo = TN(x + 440, y + 60), j1 = TN(x + 520, y + 60), j2 = TN(x + 520, y + 50), a2a = TN(x + 560, y + 50);
    TW(uo, j1); TW(j1, j2); TW(j2, a2a);
    int k1 = TN(x + 540, y + 180), k2 = TN(x + 540, y + 90), a2b = TN(x + 560, y + 90);
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
    add_label(circuit, x + 20, y - 80, "SIL loading: 200 mi 345 kV line into Zc = 283 ohm (420 MW) - close SW for 2 x SIL");
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


// ---------------------------------------------------------------------------------------
// Three-phase examples: three AC sources at 0 / -120 / +120 degrees, one row per phase.
// ---------------------------------------------------------------------------------------
#define TN(cx, cy) circuit_find_or_create_node(circuit, (cx), (cy), 5.0f)
#define TW(a_, b_) circuit_add_wire(circuit, (a_), (b_))
static Component *ph_source(Circuit *circuit, float x, float y, double vpk, double deg) {
    Component *v = ac_source(circuit, x, y, vpk);
    if (v) v->props.ac_voltage.phase = deg;
    return v;
}
// rows at y, y+140, y+280: source -> series R -> load R -> neutral bus at x+300 -> R_n -> ground
static int place_3ph_y(Circuit *circuit, float x, float y, const double *loads, const char *title) {
    static const double deg[3] = { 0, -120, 120 };
    int bus_prev = -1;
    for (int k = 0; k < 3; k++) {
        float ry = y + k * 140;
        Component *v = ph_source(circuit, x, ry, 392.0, deg[k]); if (!v) return 0;
        Component *rl = add_comp(circuit, COMP_RESISTOR, x + 100, ry + 20, 0);   // (60,20)-(140,20)
        rl->props.resistor.resistance = 0.5;
        Component *ld = add_comp(circuit, COMP_RESISTOR, x + 220, ry + 20, 0);   // (180,20)-(260,20)
        ld->props.resistor.resistance = loads[k];
        int sp = TN(x, ry + 20), a = TN(x + 60, ry + 20), b = TN(x + 140, ry + 20), c = TN(x + 180, ry + 20), d = TN(x + 260, ry + 20), bus = TN(x + 300, ry + 20);
        TW(sp, a); TW(b, c); TW(d, bus);
        if (bus_prev >= 0) TW(bus_prev, bus);
        bus_prev = bus;
        v->node_ids[0] = sp; rl->node_ids[0] = a; rl->node_ids[1] = b; ld->node_ids[0] = c; ld->node_ids[1] = d;
    }
    Component *rn = add_comp(circuit, COMP_RESISTOR, x + 300, y + 340, 90);     // (300,300)-(300,380)
    rn->props.resistor.resistance = 1.0;
    Component *gn = add_comp(circuit, COMP_GROUND, x + 300, y + 400, 0);
    rn->node_ids[0] = bus_prev;
    connect_terminals(circuit, rn, 1, gn, 0);
    add_label(circuit, x + 20, y - 40, title);
    add_label(circuit, x + 370, y + 330, "neutral (1 ohm to ground)");
    return 11;
}
static int place_3ph_y_balanced(Circuit *circuit, float x, float y) {
    static const double loads[3] = { 10.0, 10.0, 10.0 };
    return place_3ph_y(circuit, x, y, loads, "Balanced Y: 277 V rms per phase (480 V line-line), 10 ohm loads, 0.5 ohm lines");
}
static int place_3ph_unbalanced(Circuit *circuit, float x, float y) {
    static const double loads[3] = { 10.0, 20.0, 40.0 };
    return place_3ph_y(circuit, x, y, loads, "Unbalanced Y: loads 10 / 20 / 40 ohm - the neutral carries the difference");
}
// three per-phase 345 kV lines (100 mi, R-L) into 198.4 ohm loads, neutral through 1 ohm
static int place_3ph_345_line(Circuit *circuit, float x, float y) {
    static const double deg[3] = { 0, -120, 120 };
    int bus_prev = -1;
    for (int k = 0; k < 3; k++) {
        float ry = y + k * 140;
        Component *v = ph_source(circuit, x, ry, 281700.0, deg[k]); if (!v) return 0;
        Component *tl = add_tline(circuit, x + 130, ry + 20, 0, 100.0, 0.06, 0.55, 8.0, 1);   // (90,20)-(170,20)
        Component *ld = add_comp(circuit, COMP_RESISTOR, x + 280, ry + 20, 0);   // (240,20)-(320,20)
        ld->props.resistor.resistance = 198.4;
        int sp = TN(x, ry + 20), a = TN(x + 90, ry + 20), b = TN(x + 170, ry + 20), c = TN(x + 200, ry + 20), d = TN(x + 280, ry + 20), bus = TN(x + 320, ry + 20);
        TW(sp, a); TW(b, c); TW(d, bus);
        if (bus_prev >= 0) TW(bus_prev, bus);
        bus_prev = bus;
        v->node_ids[0] = sp; tl->node_ids[0] = a; tl->node_ids[1] = b; ld->node_ids[0] = c; ld->node_ids[1] = d;
    }
    Component *rn = add_comp(circuit, COMP_RESISTOR, x + 320, y + 340, 90);
    rn->props.resistor.resistance = 1.0;
    Component *gn = add_comp(circuit, COMP_GROUND, x + 320, y + 400, 0);
    rn->node_ids[0] = bus_prev;
    connect_terminals(circuit, rn, 1, gn, 0);
    add_label(circuit, x + 20, y - 80, "Three-phase 345 kV, 100 mi per phase, 600 MW: every phase drops the same 6 %");
    return 11;
}
// six-pulse bridge: phase columns at x+100/200/300, plus bus y=0, minus bus y=260 (neutral grounded)
static int place_3ph_rectifier(Circuit *circuit, float x, float y) {
    static const double deg[3] = { 0, -120, 120 };
    int plus_prev = -1, minus_prev = -1;
    for (int k = 0; k < 3; k++) {
        float cx = x + 100 + k * 100;
        Component *du = add_comp(circuit, COMP_DIODE, cx, y + 60, 270);          // A (cx,100) K (cx,20)
        Component *v = add_comp(circuit, COMP_AC_VOLTAGE, cx, y + 160, 0);       // +(cx,120) -(cx,200)
        if (!v) return 0;
        v->props.ac_voltage.amplitude = 170.0; v->props.ac_voltage.frequency = 60.0; v->props.ac_voltage.phase = deg[k];
        Component *g = add_comp(circuit, COMP_GROUND, cx, y + 220, 0);
        Component *dl = add_comp(circuit, COMP_DIODE, cx + 40, y + 180, 270);    // K (cx+40,140) A (cx+40,220)
        int kt = TN(cx, y + 20), plus = TN(cx, y), an = TN(cx, y + 100), vp = TN(cx, y + 120);
        TW(kt, plus); TW(an, vp);
        int j = TN(cx + 40, y + 120), kl = TN(cx + 40, y + 140), al = TN(cx + 40, y + 220), minus = TN(cx + 40, y + 260);
        TW(vp, j); TW(j, kl); TW(al, minus);
        if (plus_prev >= 0) TW(plus_prev, plus);
        if (minus_prev >= 0) TW(minus_prev, minus);
        plus_prev = plus; minus_prev = minus;
        du->node_ids[0] = an; du->node_ids[1] = kt; v->node_ids[0] = vp; dl->node_ids[0] = al; dl->node_ids[1] = kl;
        connect_terminals(circuit, v, 1, g, 0);
    }
    Component *ld = add_comp(circuit, COMP_RESISTOR, x + 440, y + 130, 90);     // (440,90)-(440,170)
    ld->props.resistor.resistance = 100.0;
    int p1 = TN(x + 440, y), p2 = TN(x + 440, y + 90), m1 = TN(x + 440, y + 260), m2 = TN(x + 440, y + 170);
    TW(plus_prev, p1); TW(p1, p2); TW(minus_prev, m1); TW(m1, m2);
    ld->node_ids[0] = p2; ld->node_ids[1] = m2;
    add_label(circuit, x + 40, y - 40, "Six-pulse rectifier: 170 Vpk per phase -> plus bus follows the highest phase, minus bus the lowest");
    add_label(circuit, x + 510, y + 120, "load 100 ohm (V+ - V-)");
    return 13;
}
#undef TN
#undef TW


// ---------------------------------------------------------------------------------------
// Sedra & Smith ch. 18 signal generators (docs/RESEARCH_OSCILLATORS.md). Bistables use the
// finite-gain saturating op-amp (ideal = virtual short would not work with positive feedback).
// ---------------------------------------------------------------------------------------
#define TN(cx, cy) circuit_find_or_create_node(circuit, (cx), (cy), 5.0f)
#define TW(a_, b_) circuit_add_wire(circuit, (a_), (b_))
static Component *sat_opamp(Circuit *circuit, float x, float y) {
    Component *u = add_comp(circuit, COMP_OPAMP, x, y, 0);
    /* Open-loop / comparator use: finite gain (1e5, not a virtual short) and hard rails.
       `ideal` stays true so the part keeps the algebraic model: these templates are about
       saturation and switching thresholds, not about a 741's GBW and slew rate. The
       Ideal vs Real Op-Amp template is where the dynamic model is the point. */
    u->props.opamp.ideal = true; u->props.opamp.gain = 1e5;
    return u;
}
// ground the op-amp terminal at (tx,ty) via a wire to (gx,ty) and a ground symbol there
static void gnd_at(Circuit *circuit, int node, float gx, float gy) {
    Component *g = add_comp(circuit, COMP_GROUND, gx, gy + 20, 0);
    int gt = TN(gx, gy);
    if (gt != node) TW(node, gt);
    g->node_ids[0] = gt;
}

// 18.4: inverting bistable, R1 = R2 = 10k -> thresholds +/- 7.5 V, driven by a 10 V 100 Hz triangle
static int place_schmitt_bistable(Circuit *circuit, float x, float y) {
    Component *v = add_comp(circuit, COMP_TRIANGLE_WAVE, x, y + 60, 0);         // +(0,20) -(0,100)
    if (!v) return 0;
    v->props.triangle_wave.amplitude = 10.0; v->props.triangle_wave.frequency = 100.0;
    Component *g0 = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    Component *u = sat_opamp(circuit, x + 200, y + 40);                          // -(160,20) +(160,60) out(240,40)
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 220, y + 100, 0);       // (180,100)-(260,100) out -> +
    r1->props.resistor.resistance = 10e3;
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 120, y + 100, 90);      // (120,60)-(120,140) + -> gnd
    r2->props.resistor.resistance = 10e3;
    Component *g2 = add_comp(circuit, COMP_GROUND, x + 120, y + 160, 0);
    Component *rl = add_comp(circuit, COMP_RESISTOR, x + 320, y + 80, 90);       // (320,40)-(320,120) load
    rl->props.resistor.resistance = 100e3;
    Component *gl = add_comp(circuit, COMP_GROUND, x + 320, y + 140, 0);
    add_label(circuit, x + 20, y - 40, "Bistable multivibrator (inverting Schmitt): flips at +/-7.5 V, output +/-15 V");
    int in = TN(x, y + 20), minus = TN(x + 160, y + 20); TW(in, minus);
    int out = TN(x + 240, y + 40), o1 = TN(x + 280, y + 40), o2 = TN(x + 280, y + 100), r1r = TN(x + 260, y + 100), r1l = TN(x + 180, y + 100), p1 = TN(x + 160, y + 100), plus = TN(x + 160, y + 60), r2t = TN(x + 120, y + 60);
    TW(out, o1); TW(o1, o2); TW(o2, r1r); TW(r1l, p1); TW(p1, plus); TW(plus, r2t);
    int lt = TN(x + 320, y + 40); TW(o1, lt);
    v->node_ids[0] = in; u->node_ids[0] = minus; u->node_ids[1] = plus; u->node_ids[2] = out;
    r1->node_ids[0] = r1l; r1->node_ids[1] = r1r; r2->node_ids[0] = r2t; rl->node_ids[0] = lt;
    connect_terminals(circuit, v, 1, g0, 0);
    connect_terminals(circuit, r2, 1, g2, 0);
    connect_terminals(circuit, rl, 1, gl, 0);
    return 9;
}

// 18.5.2: non-inverting bistable (R1 10k, R2 20k) + inverting integrator (R 10k, C 10 nF): f = R2/(4 R C R1) = 5 kHz
// returns the integrator output node; *u2_out receives the integrator output node id
static int place_tri_square_core(Circuit *circuit, float x, float y, int *tri_node) {
    Component *u1 = sat_opamp(circuit, x + 200, y + 40);                         // -(160,20) +(160,60) out(240,40)
    if (!u1) return 0;
    Component *kick = add_comp(circuit, COMP_PULSE_SOURCE, x + 100, y + 20, 90); // rot 90: +(140,20) -(60,20)
    kick->props.pulse_source.v_low = 0; kick->props.pulse_source.v_high = 0.5; kick->props.pulse_source.pulse_width = 50e-6; kick->props.pulse_source.period = 100.0;
    Component *gk = add_comp(circuit, COMP_GROUND, x + 20, y + 40, 0);          // terminal (20,20): clear of the pulse body
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 100, y + 60, 0);        // (60,60)-(140,60) tri -> +
    r1->props.resistor.resistance = 10e3;
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 220, y + 120, 0);       // (180,120)-(260,120) out -> +
    r2->props.resistor.resistance = 20e3;
    Component *r = add_comp(circuit, COMP_RESISTOR, x + 320, y + 120, 0);        // (280,120)-(360,120) out -> integ -
    r->props.resistor.resistance = 10e3;
    Component *u2 = sat_opamp(circuit, x + 400, y + 140);                        // -(360,120) +(360,160) out(440,140); finite gain integrates fine
    Component *c = add_comp(circuit, COMP_CAPACITOR, x + 420, y + 60, 0);        // (380,60)-(460,60)
    c->props.capacitor.capacitance = 10e-9;
    Component *g2 = add_comp(circuit, COMP_GROUND, x + 340, y + 180, 0);         // integrator + at (360,160) -> (340,160)
    int minus1 = TN(x + 160, y + 20), kp = TN(x + 140, y + 20), kn = TN(x + 60, y + 20);
    TW(kp, minus1);
    kick->node_ids[0] = kp; kick->node_ids[1] = kn;
    { int gkn = TN(x + 20, y + 20); TW(kn, gkn); gk->node_ids[0] = gkn; }
    int plus1 = TN(x + 160, y + 60), r1r = TN(x + 140, y + 60), r1l = TN(x + 60, y + 60);
    TW(r1r, plus1);
    int out1 = TN(x + 240, y + 40), o1 = TN(x + 280, y + 40), o2 = TN(x + 280, y + 120), r2r = TN(x + 260, y + 120), r2l = TN(x + 180, y + 120), p2 = TN(x + 160, y + 120);
    TW(out1, o1); TW(o1, o2); TW(o2, r2r); TW(r2l, p2); TW(p2, plus1);
    int minus2 = TN(x + 360, y + 120);
    int cl = TN(x + 380, y + 60), c1 = TN(x + 360, y + 60), cr = TN(x + 460, y + 60), c2 = TN(x + 480, y + 60), out2 = TN(x + 440, y + 140), o3 = TN(x + 480, y + 140);
    TW(cl, c1); TW(c1, minus2); TW(cr, c2); TW(c2, o3); TW(out2, o3);
    int plus2 = TN(x + 360, y + 160), g2t = TN(x + 340, y + 160); TW(plus2, g2t); g2->node_ids[0] = g2t;
    int f1 = TN(x + 480, y + 220), f2 = TN(x + 60, y + 220); TW(o3, f1); TW(f1, f2); TW(f2, r1l);
    u1->node_ids[0] = minus1; u1->node_ids[1] = plus1; u1->node_ids[2] = out1;
    r1->node_ids[0] = r1l; r1->node_ids[1] = r1r; r2->node_ids[0] = r2l; r2->node_ids[1] = r2r; r->node_ids[0] = o2; r->node_ids[1] = minus2;
    u2->node_ids[0] = minus2; u2->node_ids[1] = plus2; u2->node_ids[2] = out2; c->node_ids[0] = cl; c->node_ids[1] = cr;
    *tri_node = o3;
    return 10;
}
static int place_tri_square_gen(Circuit *circuit, float x, float y) {
    int tri; int n = place_tri_square_core(circuit, x, y, &tri);
    if (!n) return 0;
    add_label(circuit, x + 20, y - 40, "Triangle / square generator: f = R2 / (4 R C R1) = 5 kHz, triangle +/-7.5 V, square +/-15 V");
    return n;
}
// 18.8.2: + R_in 10k and a 3-breakpoint diode shaper (22k to +/-2.0 V, 5.6k to +/-3.7 V)
static int place_function_gen(Circuit *circuit, float x, float y) {
    int tri; int n = place_tri_square_core(circuit, x, y, &tri);
    if (!n) return 0;
    Component *rin = add_comp(circuit, COMP_RESISTOR, x + 540, y + 140, 0);      // (500,140)-(580,140)
    rin->props.resistor.resistance = 10e3;
    int o3 = TN(x + 480, y + 140), ril = TN(x + 500, y + 140), sn = TN(x + 580, y + 140), s2 = TN(x + 600, y + 140);
    TW(o3, ril); TW(sn, s2);
    rin->node_ids[0] = ril; rin->node_ids[1] = sn;
    struct { double r, vb; int neg; } br[4] = { {22e3, 2.0, 0}, {5.6e3, 3.7, 0}, {22e3, -2.0, 1}, {5.6e3, -3.7, 1} };
    int prev = s2;
    for (int k = 0; k < 4; k++) {
        /* 80 px columns, not 60: each branch's bias source carries a voltage label ("3.7V")
           drawn beside its circle, and at 60 px it was drawn across the neighbouring source. */
        float bx = x + 600 + k * 80;
        int top = TN(bx, y + 140);
        if (k) TW(prev, top);
        prev = top;
        Component *rb = add_comp(circuit, COMP_RESISTOR, bx, y + 180, 90);      // (bx,140)-(bx,220)
        rb->props.resistor.resistance = br[k].r;
        Component *d = add_comp(circuit, COMP_DIODE, bx, y + 260, br[k].neg ? 270 : 90);   // 90: A top (bx,220) K bottom (bx,300); 270: K top, A bottom
        Component *vb = add_comp(circuit, COMP_DC_VOLTAGE, bx, y + 340, 0);      // +(bx,300) -(bx,380)
        vb->props.dc_voltage.voltage = br[k].vb;
        Component *g = add_comp(circuit, COMP_GROUND, bx, y + 400, 0);
        int mid = TN(bx, y + 220), bot = TN(bx, y + 300);
        rb->node_ids[0] = top; rb->node_ids[1] = mid;
        if (br[k].neg) { d->node_ids[1] = mid; d->node_ids[0] = bot; } else { d->node_ids[0] = mid; d->node_ids[1] = bot; }
        vb->node_ids[0] = bot;
        connect_terminals(circuit, vb, 1, g, 0);
    }
    add_label(circuit, x + 20, y - 40, "Function generator: triangle -> R_in -> diode breakpoints -> ~5 V sine. Edit R (f) or R2 (amplitude)");
    add_label(circuit, x + 590, y + 110, "sine out");
    return n + 13;
}

// 18.3.1: common-source Colpitts, L 100 uH, C1 = C2 = 1 nF -> f = 1/(2 pi sqrt(L C1C2/(C1+C2))) = 712 kHz
static int place_lc_core(Circuit *circuit, float x, float y, int hartley, double cc_val, const char *title) {
    Component *vdd = add_comp(circuit, COMP_DC_VOLTAGE, x + 100, y - 80, 0);    // +(100,-120) -(100,-40)
    if (!vdd) return 0;
    vdd->props.dc_voltage.voltage = 12.0;
    Component *gv = add_comp(circuit, COMP_GROUND, x + 100, y - 20, 0);
    Component *m = add_comp(circuit, COMP_NMOS, x + 200, y + 100, 0);           // G(180,100) D(220,80) S(220,120)
    Component *gs = add_comp(circuit, COMP_GROUND, x + 220, y + 140, 0);
    Component *rfc = add_comp(circuit, COMP_INDUCTOR, x + 220, y, 90);           // (220,-40)-(220,40): RFC, or L1 of the Hartley (tap = Vdd = AC ground)
    rfc->props.inductor.inductance = hartley ? 50e-6 : 1e-3;
    // tank: Colpitts = C1 (drain-gnd), L (series), C2 (gate-gnd); Hartley = L1, C, L2 (gate side via 10 nF)
    Component *c1 = NULL, *g1 = NULL;
    if (!hartley) {
        c1 = add_comp(circuit, COMP_CAPACITOR, x + 300, y + 80, 90);             // (300,40)-(300,120)
        c1->props.capacitor.capacitance = 1e-9;
        g1 = add_comp(circuit, COMP_GROUND, x + 300, y + 140, 0);
    }
    Component *l = add_comp(circuit, hartley ? COMP_CAPACITOR : COMP_INDUCTOR, x + 360, y + 40, 0);      // (320,40)-(400,40)
    if (hartley) l->props.capacitor.capacitance = 1e-9; else l->props.inductor.inductance = 100e-6;
    Component *cc = add_comp(circuit, COMP_CAPACITOR, x + 440, y + 40, 0);       // (400,40)-(480,40)
    cc->props.capacitor.capacitance = cc_val;
    Component *c2 = add_comp(circuit, hartley ? COMP_INDUCTOR : COMP_CAPACITOR, x + 100, y + 140, 90);   // (100,100)-(100,180)
    if (hartley) c2->props.inductor.inductance = 50e-6; else c2->props.capacitor.capacitance = 1e-9;
    Component *kick = add_comp(circuit, COMP_PULSE_SOURCE, x + 100, y + 220, 0); // +(100,180) -(100,260)
    kick->props.pulse_source.v_low = 0; kick->props.pulse_source.v_high = 0.3; kick->props.pulse_source.pulse_width = 50e-9; kick->props.pulse_source.period = 100.0;
    Component *gk = add_comp(circuit, COMP_GROUND, x + 100, y + 280, 0);
    Component *rt = add_comp(circuit, COMP_RESISTOR, x + 60, y - 80, 90);        // (60,-120)-(60,-40)
    rt->props.resistor.resistance = 1e6;
    Component *rb = add_comp(circuit, COMP_RESISTOR, x + 60, y + 140, 90);       // (60,100)-(60,180)
    rb->props.resistor.resistance = 1e6;
    Component *gb = add_comp(circuit, COMP_GROUND, x + 60, y + 200, 0);
    add_label(circuit, x + 20, y - 160, title);
    int vp = TN(x + 100, y - 120), rt0 = TN(x + 60, y - 120), rail = TN(x + 220, y - 120), rfct = TN(x + 220, y - 40);
    TW(vp, rt0); TW(vp, rail); TW(rail, rfct);
    int rfcb = TN(x + 220, y + 40), d = TN(x + 220, y + 80), tank = TN(x + 300, y + 40), lt = TN(x + 320, y + 40);
    TW(rfcb, d); TW(rfcb, tank); TW(tank, lt);
    int lr = TN(x + 400, y + 40), ccr = TN(x + 480, y + 40), f1 = TN(x + 480, y + 180), f2 = TN(x + 140, y + 180), f3 = TN(x + 140, y + 100), gate = TN(x + 180, y + 100), gn = TN(x + 100, y + 100), rb0 = TN(x + 60, y + 100), rt1 = TN(x + 60, y - 40);
    TW(ccr, f1); TW(f1, f2); TW(f2, f3); TW(f3, gate);
    if (hartley) {
        // gate node -> 10 nF -> L2 top (gn): keeps the 6 V bias off the inductor; bias joins the gate at (60,20)
        Component *cg = add_comp(circuit, COMP_CAPACITOR, x + 100, y + 60, 90);  // (100,20)-(100,100)
        cg->props.capacitor.capacitance = 10e-9;
        int cgt = TN(x + 100, y + 20), b1 = TN(x + 140, y + 20), mid = TN(x + 60, y + 20);
        TW(f3, b1); TW(b1, cgt); TW(rt1, mid); TW(mid, rb0); TW(mid, cgt);
        cg->node_ids[0] = cgt; cg->node_ids[1] = gn;
    } else {
        TW(f3, gn); TW(gn, rb0); TW(rt1, rb0);
    }
    vdd->node_ids[0] = vp; rt->node_ids[0] = rt0; rt->node_ids[1] = rt1; rfc->node_ids[0] = rfct; rfc->node_ids[1] = rfcb;
    m->node_ids[0] = gate; m->node_ids[1] = d; if (c1) c1->node_ids[0] = tank; l->node_ids[0] = lt; l->node_ids[1] = lr; cc->node_ids[0] = lr; cc->node_ids[1] = ccr;
    c2->node_ids[0] = gn; rb->node_ids[0] = rb0;
    int c2b = TN(x + 100, y + 180); c2->node_ids[1] = c2b; kick->node_ids[0] = c2b;
    connect_terminals(circuit, vdd, 1, gv, 0);
    connect_terminals(circuit, m, 2, gs, 0);
    if (c1) connect_terminals(circuit, c1, 1, g1, 0);
    connect_terminals(circuit, kick, 1, gk, 0);
    connect_terminals(circuit, rb, 1, gb, 0);
    return 15;
}
static int place_colpitts(Circuit *circuit, float x, float y) {
    return place_lc_core(circuit, x, y, 0, 10e-9, "Colpitts (common source): tank L 100 uH with C1 = C2 = 1 nF -> 712 kHz; 1 mH RFC feeds the drain");
}
static int place_hartley(Circuit *circuit, float x, float y) {
    return place_lc_core(circuit, x, y, 1, 220e-9, "Hartley (common source): L1 = L2 = 50 uH with the tap at Vdd (AC ground), C = 1 nF -> 503 kHz");
}
static int place_clapp(Circuit *circuit, float x, float y) {
    return place_lc_core(circuit, x, y, 0, 100e-12, "Clapp: Colpitts with 100 pF in series with L - the small cap sets f = 1.744 MHz");
}

// Pierce with a "teaching crystal": Ls 100 mH, Cs 25.33 pF (f_s = 100.0 kHz), Rs 200 (Q ~ 314), Cp 1 nF;
// inverting op-amp stage (gain -100) + 100 ohm drive R + C2 / crystal / C1 pi network. f = f_s (1 + Cs/(2(Cp + C_L))) = 100.63 kHz
static int place_pierce(Circuit *circuit, float x, float y);
static int place_pierce(Circuit *circuit, float x, float y) {
    Component *u = sat_opamp(circuit, x + 200, y + 40);                          // -(160,20) +(160,60) out(240,40)
    if (!u) return 0;
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 100, y + 20, 0);        // (60,20)-(140,20)
    r1->props.resistor.resistance = 22e3;                                     // 100k, not 10k: at 100 kHz a 10k input would load the pi network down below unity loop gain
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 200, y - 40, 0);        // (160,-40)-(240,-40)
    r2->props.resistor.resistance = 10e6;                                      // gain -100, well past the network's loss
    Component *gp = add_comp(circuit, COMP_GROUND, x + 140, y + 80, 0);         // + input grounded at (140,60)
    Component *ro = add_comp(circuit, COMP_RESISTOR, x + 320, y + 40, 0);        // (280,40)-(360,40): output R (with C3: the extra pole a real inverter has)
    ro->props.resistor.resistance = 220.0;
    Component *c3 = add_comp(circuit, COMP_CAPACITOR, x + 360, y + 80, 90);      // (360,40)-(360,120)
    c3->props.capacitor.capacitance = 100e-12;                                 // just the inverter's own output capacitance - 2.2 nF here was a 72 kHz pole sitting on top of the crystal
    Component *g3 = add_comp(circuit, COMP_GROUND, x + 360, y + 140, 0);
    Component *rd = add_comp(circuit, COMP_RESISTOR, x + 400, y + 40, 0);        // (360,40)-(440,40)
    rd->props.resistor.resistance = 100.0;
    Component *c2 = add_comp(circuit, COMP_CAPACITOR, x + 440, y + 80, 90);      // (440,40)-(440,120)
    c2->props.capacitor.capacitance = 4.7e-9;                                 // C2 of the pi network: with C1 the pair sets the load capacitance the crystal is pulled by
    Component *kick = add_comp(circuit, COMP_PULSE_SOURCE, x + 440, y + 160, 0); // +(440,120) -(440,200): start-up kick under C2
    kick->props.pulse_source.v_low = 0; kick->props.pulse_source.v_high = 0.5; kick->props.pulse_source.pulse_width = 2e-6; kick->props.pulse_source.period = 100.0;
    Component *gk = add_comp(circuit, COMP_GROUND, x + 440, y + 220, 0);
    /* One crystal, not four parts pretending to be one: the component integrates its motional
       arm trapezoidally, which is what keeps a Q of 314 alive at these time steps. */
    Component *xtal = add_comp(circuit, COMP_CRYSTAL, x + 580, y + 40, 0);       // (540,40)-(620,40)
    xtal->props.crystal.ls = 100e-3;
    xtal->props.crystal.cs = 2.5330e-11;    /* fs = 100.0 kHz */
    xtal->props.crystal.rs = 200.0;         /* Q = 2 pi fs Ls / Rs = 314 */
    xtal->props.crystal.cp = 33e-12;        /* holder capacitance. This one matters: at 1 nF it shunts
                                               the motional arm hard enough that the loop locks onto the
                                               parallel resonance instead, or dies - which is exactly the
                                               reason a data sheet gives you C0 and a maximum load. */
    xtal->props.crystal.ideal = false;
    Component *c1 = add_comp(circuit, COMP_CAPACITOR, x + 780, y + 80, 90);      // (780,40)-(780,120)
    c1->props.capacitor.capacitance = 4.7e-9;                                 // C1 of the pi network
    Component *g1 = add_comp(circuit, COMP_GROUND, x + 780, y + 140, 0);
    add_label(circuit, x + 20, y - 100, "Pierce crystal oscillator: f_s = 100 kHz, Q = 314, C1 = C2 = 4.7 nF - it runs just above f_s, where the crystal looks inductive");
    int minus = TN(x + 160, y + 20), r1r = TN(x + 140, y + 20), r1l = TN(x + 60, y + 20), plus = TN(x + 160, y + 60), gpt = TN(x + 140, y + 60);
    TW(r1r, minus); TW(plus, gpt); gp->node_ids[0] = gpt;
    int out = TN(x + 240, y + 40), o1 = TN(x + 280, y + 40), o2 = TN(x + 280, y - 40), r2r = TN(x + 240, y - 40), r2l = TN(x + 160, y - 40);
    TW(out, o1); TW(o1, o2); TW(o2, r2r); TW(r2l, minus);
    int n3 = TN(x + 360, y + 40), na = TN(x + 440, y + 40), lsl = TN(x + 460, y + 40), rsr = TN(x + 700, y + 40), nb = TN(x + 740, y + 40), c1t = TN(x + 780, y + 40);
    TW(na, lsl); TW(rsr, nb); TW(nb, c1t);
    int xl = TN(x + 540, y + 40), xr = TN(x + 620, y + 40);
    TW(lsl, xl); TW(xr, rsr);
    xtal->node_ids[0] = xl; xtal->node_ids[1] = xr;
    int f1 = TN(x + 740, y + 260), f2 = TN(x + 20, y + 260), f3 = TN(x + 20, y + 20);
    TW(nb, f1); TW(f1, f2); TW(f2, f3); TW(f3, r1l);
    u->node_ids[0] = minus; u->node_ids[1] = plus; u->node_ids[2] = out;
    r1->node_ids[0] = r1l; r1->node_ids[1] = r1r; r2->node_ids[0] = r2l; r2->node_ids[1] = r2r;
    ro->node_ids[0] = o1; ro->node_ids[1] = n3; c3->node_ids[0] = n3; rd->node_ids[0] = n3; rd->node_ids[1] = na; c2->node_ids[0] = na; int c2b = TN(x + 440, y + 120); c2->node_ids[1] = c2b; kick->node_ids[0] = c2b;
    c1->node_ids[0] = c1t;
    connect_terminals(circuit, c3, 1, g3, 0);
    connect_terminals(circuit, kick, 1, gk, 0);
    connect_terminals(circuit, c1, 1, g1, 0);
    return 14;
}

// ring oscillator: five inverters, each followed by R 1k / C 1 nF -> f ~ 1/(2 N 0.69 RC) ~ 145 kHz
static int place_ring_osc(Circuit *circuit, float x, float y) {
    int first_in = -1, prev = -1, kick_node = -1;
    for (int k = 0; k < 5; k++) {
        float gx = x + 100 + k * 160;
        Component *g = add_comp(circuit, COMP_NOT_GATE, gx, y + 20, 0);         // IN (gx-40,20) OUT (gx+40,20)
        if (!g) return 0;
        Component *r = add_comp(circuit, COMP_RESISTOR, gx + 80, y + 20, 0);     // (gx+40,20)-(gx+120,20)
        r->props.resistor.resistance = 1e3;
        Component *c = add_comp(circuit, COMP_CAPACITOR, gx + 120, y + 60, 90);  // (gx+120,20)-(gx+120,100)
        c->props.capacitor.capacitance = 1e-9;
        int in = TN(gx - 40, y + 20), out = TN(gx + 40, y + 20), nd = TN(gx + 120, y + 20);
        if (k == 0) first_in = in; else TW(prev, in);
        g->node_ids[0] = in; g->node_ids[1] = out; r->node_ids[0] = out; r->node_ids[1] = nd; c->node_ids[0] = nd;
        if (k == 0) {
            Component *kick = add_comp(circuit, COMP_PULSE_SOURCE, gx + 120, y + 140, 0);   // +(gx+120,100) -(gx+120,180)
            kick->props.pulse_source.v_low = 0; kick->props.pulse_source.v_high = 3.0; kick->props.pulse_source.pulse_width = 2e-6; kick->props.pulse_source.period = 100.0;
            Component *gk = add_comp(circuit, COMP_GROUND, gx + 120, y + 200, 0);
            kick_node = TN(gx + 120, y + 100); c->node_ids[1] = kick_node; kick->node_ids[0] = kick_node;
            connect_terminals(circuit, kick, 1, gk, 0);
        } else {
            Component *gc = add_comp(circuit, COMP_GROUND, gx + 120, y + 120, 0);
            connect_terminals(circuit, c, 1, gc, 0);
        }
        prev = nd;
    }
    int f1 = TN(x + 880, y + 20), f2 = TN(x + 880, y - 40), f3 = TN(x + 40, y - 40), f4 = TN(x + 40, y + 20);
    TW(prev, f1); TW(f1, f2); TW(f2, f3); TW(f3, f4); TW(f4, first_in);
    add_label(circuit, x + 20, y - 80, "Ring oscillator: odd number of inverters; each RC adds ~0.69 RC = 0.7 us -> f ~ 1/(2 x 5 x 0.7 us) ~ 145 kHz");
    return 18;
}
#undef TN
#undef TW


// ---------------------------------------------------------------------------------------
// Textbook basics and transients (Agarwal & Lang ch. 3, 10, 12; Sedra & Smith app. D/E).
// ---------------------------------------------------------------------------------------
#define TN(cx, cy) circuit_find_or_create_node(circuit, (cx), (cy), 5.0f)
#define TW(a_, b_) circuit_add_wire(circuit, (a_), (b_))
static Component *square_source(Circuit *circuit, float x, float y, double f) {
    Component *v = add_comp(circuit, COMP_SQUARE_WAVE, x, y + 60, 0);            // +(0,20) -(0,100)
    if (!v) return NULL;
    v->props.square_wave.amplitude = 2.5; v->props.square_wave.offset = 2.5; v->props.square_wave.frequency = f; v->props.square_wave.duty = 0.5;
    Component *g = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    connect_terminals(circuit, v, 1, g, 0);
    return v;
}
// source(0,20) -> series part A (horizontal at x+60) -> series part B (x+160) -> node (x+220) -> shunt part (vertical) -> gnd
static int series_series_shunt(Circuit *circuit, float x, float y, Component *src, Component *a, Component *b, Component *sh) {
    int sp = TN(x, y + 20), al = TN(x + 20, y + 20), ar = TN(x + 100, y + 20), bl = TN(x + 120, y + 20), br = TN(x + 200, y + 20), n = TN(x + 220, y + 20);
    TW(sp, al); if (b) { TW(ar, bl); TW(br, n); } else TW(ar, n);
    src->node_ids[0] = sp; a->node_ids[0] = al; a->node_ids[1] = ar;
    if (b) { b->node_ids[0] = bl; b->node_ids[1] = br; }
    sh->node_ids[0] = n;
    Component *g = add_comp(circuit, COMP_GROUND, x + 220, y + 120, 0);
    connect_terminals(circuit, sh, 1, g, 0);
    return n;
}

static int place_thevenin(Circuit *circuit, float x, float y) {
    Component *v = add_comp(circuit, COMP_DC_VOLTAGE, x, y + 60, 0); if (!v) return 0;   // +(0,20) -(0,100)
    v->props.dc_voltage.voltage = 10.0;
    Component *g0 = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 100, y + 60, 90);       // (100,20)-(100,100)
    r1->props.resistor.resistance = 2e3;
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 100, y + 140, 90);      // (100,100)-(100,180)
    r2->props.resistor.resistance = 3e3;
    Component *g2 = add_comp(circuit, COMP_GROUND, x + 100, y + 200, 0);
    Component *r3 = add_comp(circuit, COMP_RESISTOR, x + 160, y + 100, 0);       // (120,100)-(200,100)
    r3->props.resistor.resistance = 1e3;
    Component *rl = add_comp(circuit, COMP_RESISTOR, x + 240, y + 140, 90);      // (240,100)-(240,180)
    rl->props.resistor.resistance = 2.2e3;
    Component *gl = add_comp(circuit, COMP_GROUND, x + 240, y + 200, 0);
    add_label(circuit, x + 20, y - 40, "Thevenin: V_th = 10 x 3/(2+3) = 6 V, R_th = 2k||3k + 1k = 2.2k -> V_L = 6 x 2.2/4.4 = 3 V");
    int sp = TN(x, y + 20), r1t = TN(x + 100, y + 20), tap = TN(x + 100, y + 100), r3l = TN(x + 120, y + 100), r3r = TN(x + 200, y + 100), lt = TN(x + 240, y + 100);
    TW(sp, r1t); TW(tap, r3l); TW(r3r, lt);
    v->node_ids[0] = sp; r1->node_ids[0] = r1t; r1->node_ids[1] = tap; r2->node_ids[0] = tap; r3->node_ids[0] = r3l; r3->node_ids[1] = r3r; rl->node_ids[0] = lt;
    connect_terminals(circuit, v, 1, g0, 0);
    connect_terminals(circuit, r2, 1, g2, 0);
    connect_terminals(circuit, rl, 1, gl, 0);
    return 8;
}

static int place_superposition(Circuit *circuit, float x, float y) {
    Component *v1 = add_comp(circuit, COMP_DC_VOLTAGE, x, y + 60, 0); if (!v1) return 0;
    v1->props.dc_voltage.voltage = 12.0;
    Component *g1 = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 80, y + 20, 0);         // (40,20)-(120,20)
    r1->props.resistor.resistance = 4e3;
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 240, y + 20, 0);        // (200,20)-(280,20)
    r2->props.resistor.resistance = 4e3;
    Component *v2 = add_comp(circuit, COMP_DC_VOLTAGE, x + 320, y + 60, 0);      // +(320,20) -(320,100)
    v2->props.dc_voltage.voltage = 6.0;
    Component *g2 = add_comp(circuit, COMP_GROUND, x + 320, y + 140, 0);
    Component *r3 = add_comp(circuit, COMP_RESISTOR, x + 160, y + 60, 90);       // (160,20)-(160,100)
    r3->props.resistor.resistance = 4e3;
    Component *g3 = add_comp(circuit, COMP_GROUND, x + 160, y + 120, 0);
    Component *i1 = add_comp(circuit, COMP_DC_CURRENT, x + 120, y + 100, 180);   // rotated: -(120,60) top, +(120,140) bottom: injects into the node
    i1->props.dc_current.current = 1e-3;
    Component *g4 = add_comp(circuit, COMP_GROUND, x + 120, y + 160, 0);
    add_label(circuit, x + 20, y - 40, "Superposition: V_N = 12/3 + 6/3 + 1 mA x (4k||4k||4k) = 4 + 2 + 1.33 = 7.33 V");
    int sp = TN(x, y + 20), r1l = TN(x + 40, y + 20), n = TN(x + 120, y + 20), r3t = TN(x + 160, y + 20), r2l = TN(x + 200, y + 20), r2r = TN(x + 280, y + 20), v2p = TN(x + 320, y + 20);
    TW(sp, r1l); TW(n, r3t); TW(r3t, r2l); TW(r2r, v2p);
    int it = TN(x + 120, y + 60); TW(n, it);
    v1->node_ids[0] = sp; r1->node_ids[0] = r1l; r1->node_ids[1] = n; r2->node_ids[0] = r2l; r2->node_ids[1] = r2r; v2->node_ids[0] = v2p;
    r3->node_ids[0] = r3t; i1->node_ids[1] = it;
    connect_terminals(circuit, v1, 1, g1, 0);
    connect_terminals(circuit, v2, 1, g2, 0);
    connect_terminals(circuit, r3, 1, g3, 0);
    connect_terminals(circuit, i1, 0, g4, 0);
    return 10;
}

static int place_rc_step(Circuit *circuit, float x, float y) {
    Component *v = square_source(circuit, x, y, 100.0); if (!v) return 0;
    Component *r = add_comp(circuit, COMP_RESISTOR, x + 60, y + 20, 0); r->props.resistor.resistance = 10e3;
    Component *c = add_comp(circuit, COMP_CAPACITOR, x + 220, y + 60, 90); c->props.capacitor.capacitance = 100e-9;
    series_series_shunt(circuit, x, y, v, r, NULL, c);
    add_label(circuit, x + 20, y - 40, "RC step: tau = RC = 1 ms; V_C(tau) = 63 % of 5 V = 3.16 V; 10-90 % rise = 2.2 tau");
    return 5;
}
static int place_rl_step(Circuit *circuit, float x, float y) {
    Component *v = square_source(circuit, x, y, 1000.0); if (!v) return 0;
    Component *l = add_comp(circuit, COMP_INDUCTOR, x + 60, y + 20, 0); l->props.inductor.inductance = 10e-3;
    Component *r = add_comp(circuit, COMP_RESISTOR, x + 220, y + 60, 90); r->props.resistor.resistance = 100.0; r->props.resistor.power_rating = 0.5;   // 0.25 W peak: 1/2 W part
    series_series_shunt(circuit, x, y, v, l, NULL, r);
    add_label(circuit, x + 20, y - 40, "RL step: tau = L/R = 100 us; the resistor voltage = 100 x i_L rises to 5 V (50 mA), 63 % at tau");
    return 5;
}
static int place_rlc_ring(Circuit *circuit, float x, float y) {
    Component *v = square_source(circuit, x, y, 200.0); if (!v) return 0;
    Component *r = add_comp(circuit, COMP_RESISTOR, x + 60, y + 20, 0); r->props.resistor.resistance = 20.0;
    Component *l = add_comp(circuit, COMP_INDUCTOR, x + 160, y + 20, 0); l->props.inductor.inductance = 10e-3;
    Component *c = add_comp(circuit, COMP_CAPACITOR, x + 220, y + 60, 90); c->props.capacitor.capacitance = 100e-9;
    series_series_shunt(circuit, x, y, v, r, l, c);
    add_label(circuit, x + 20, y - 40, "Series RLC step: w0 = 1/sqrt(LC) (5.03 kHz), zeta = R/(2 sqrt(L/C)) = 0.03 -> 90 % overshoot, rings for ~1 ms");
    return 6;
}
static int place_rlc_damping(Circuit *circuit, float x, float y) {
    static const double rs[3] = { 20.0, 632.0, 2000.0 };
    static const char *names[3] = { "R = 20: underdamped (zeta 0.03)", "R = 632 = 2 sqrt(L/C): critical", "R = 2k: overdamped (slow root tau 195 us)" };
    for (int k = 0; k < 3; k++) {
        float ry = y + k * 140;
        Component *v = square_source(circuit, x, ry, 200.0); if (!v) return 0;
        Component *r = add_comp(circuit, COMP_RESISTOR, x + 60, ry + 20, 0); r->props.resistor.resistance = rs[k];
        Component *l = add_comp(circuit, COMP_INDUCTOR, x + 160, ry + 20, 0); l->props.inductor.inductance = 10e-3;
        Component *c = add_comp(circuit, COMP_CAPACITOR, x + 220, ry + 60, 90); c->props.capacitor.capacitance = 100e-9;
        series_series_shunt(circuit, x, ry, v, r, l, c);
        add_label(circuit, x + 330, ry + 50, names[k]);
    }
    add_label(circuit, x + 20, y - 40, "Damping ladder: same L = 10 mH, C = 100 nF; only R changes the shape of the step response");
    return 18;
}

static int place_opamp_sat(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 2.0); if (!v) return 0;
    v->props.ac_voltage.frequency = 1000.0;
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 80, y + 20, 0);         // (40,20)-(120,20)
    r1->props.resistor.resistance = 10e3;
    Component *u = add_comp(circuit, COMP_OPAMP, x + 200, y + 40, 0);           // -(160,20) +(160,60) out(240,40)
    u->props.opamp.ideal = true; u->props.opamp.gain = 1e5;   /* algebraic model, finite gain: see sat_opamp */
    Component *g = add_comp(circuit, COMP_GROUND, x + 140, y + 80, 0);          // + at (160,60) -> (140,60)
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 200, y - 40, 0);        // (160,-40)-(240,-40)
    r2->props.resistor.resistance = 100e3;
    Component *rl = add_comp(circuit, COMP_RESISTOR, x + 320, y + 80, 90);       // (320,40)-(320,120)
    rl->props.resistor.resistance = 10e3;
    Component *gl = add_comp(circuit, COMP_GROUND, x + 320, y + 140, 0);
    add_label(circuit, x + 20, y - 100, "Op-amp saturation: gain -10 with 2 Vpk in wants +/-20 V but the rails stop it at +/-15 V; the - input leaves 0 V");
    int sp = TN(x, y + 20), r1l = TN(x + 40, y + 20), minus = TN(x + 160, y + 20), plus = TN(x + 160, y + 60), gt = TN(x + 140, y + 60);
    TW(sp, r1l); TW(plus, gt);
    int out = TN(x + 240, y + 40), o1 = TN(x + 280, y + 40), o2 = TN(x + 280, y - 40), r2r = TN(x + 240, y - 40), r2l = TN(x + 160, y - 40), lt = TN(x + 320, y + 40);
    TW(out, o1); TW(o1, o2); TW(o2, r2r); TW(r2l, minus); TW(o1, lt);
    v->node_ids[0] = sp; r1->node_ids[0] = r1l; r1->node_ids[1] = minus; u->node_ids[0] = minus; u->node_ids[1] = plus; u->node_ids[2] = out;
    g->node_ids[0] = gt; r2->node_ids[0] = r2l; r2->node_ids[1] = r2r; rl->node_ids[0] = lt;
    connect_terminals(circuit, rl, 1, gl, 0);
    return 8;
}
#undef TN
#undef TW


// ---------------------------------------------------------------------------------------
// Batch 5: tuned / CB / Darlington amplifiers, an SR latch, and two three-phase "plants".
// ---------------------------------------------------------------------------------------
#define TN(cx, cy) circuit_find_or_create_node(circuit, (cx), (cy), 5.0f)
#define TW(a_, b_) circuit_add_wire(circuit, (a_), (b_))
/* An output coupling capacitor with nothing on the far side is not an output: the pin has no
   node at all, so the schematic shows a lead going nowhere and the node cannot be probed. This
   gives it the load a real stage would drive - high enough not to move the gain, real enough to
   be a circuit. Placed from the capacitor's own terminal position, so it follows the layout. */
static void cap_output_load(Circuit *circuit, Component *cap, double r_load) {
    if (!cap) return;
    float ox, oy;
    component_get_terminal_pos(cap, 1, &ox, &oy);
    int out = circuit_find_or_create_node(circuit, ox, oy, 5.0f);
    cap->node_ids[1] = out;
    Component *rl = add_comp(circuit, COMP_RESISTOR, ox + 80, oy + 60, 90);   /* (ox+80,oy+20)-(ox+80,oy+100) */
    if (!rl) return;
    rl->props.resistor.resistance = r_load;
    int corner = circuit_find_or_create_node(circuit, ox + 80, oy, 5.0f);
    int rt = circuit_find_or_create_node(circuit, ox + 80, oy + 20, 5.0f);
    int rb = circuit_find_or_create_node(circuit, ox + 80, oy + 100, 5.0f);
    circuit_add_wire(circuit, out, corner);
    circuit_add_wire(circuit, corner, rt);
    rl->node_ids[0] = rt; rl->node_ids[1] = rb;
    Component *g = add_comp(circuit, COMP_GROUND, ox + 80, oy + 160, 0);
    if (!g) return;
    int gt = circuit_find_or_create_node(circuit, ox + 80, oy + 140, 5.0f);
    circuit_add_wire(circuit, rb, gt);
    g->node_ids[0] = gt;
}

static Component *dc_rail(Circuit *circuit, float x, float y, double v) {   // DC source at (x,y+40): +(x,y) -(x,y+80) -> gnd
    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x, y + 40, 0);
    vcc->props.dc_voltage.voltage = v;
    Component *g = add_comp(circuit, COMP_GROUND, x, y + 120, 0);
    connect_terminals(circuit, vcc, 1, g, 0);
    return vcc;
}
static Component *vres(Circuit *circuit, float x, float y, double r) {      // vertical resistor, terminals (x,y-40)/(x,y+40)
    Component *c = add_comp(circuit, COMP_RESISTOR, x, y, 90); c->props.resistor.resistance = r; return c;
}
static Component *hres(Circuit *circuit, float x, float y, double r) {
    Component *c = add_comp(circuit, COMP_RESISTOR, x, y, 0); c->props.resistor.resistance = r; return c;
}
static Component *vcap(Circuit *circuit, float x, float y, double c) {
    Component *k = add_comp(circuit, COMP_CAPACITOR, x, y, 90); k->props.capacitor.capacitance = c; return k;
}
static Component *hcap(Circuit *circuit, float x, float y, double c) {
    Component *k = add_comp(circuit, COMP_CAPACITOR, x, y, 0); k->props.capacitor.capacitance = c; return k;
}
static void gnd_below(Circuit *circuit, Component *c, int term, float x, float y) {   // ground symbol at (x,y), terminal (x,y-20)
    Component *g = add_comp(circuit, COMP_GROUND, x, y, 0);
    connect_terminals(circuit, c, term, g, 0);
}

// Single-tuned amplifier: CE stage (R1 47k, R2 10k, Re 1k || 10 uF), collector load L 1 mH || C 2.53 nF || Rq 10k
static int place_single_tuned_amp(Circuit *circuit, float x, float y) {
    Component *vcc = dc_rail(circuit, x - 180, y - 100, 12.0); if (!vcc) return 0;         // +(-180,-100): clear of the input cap
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x - 100, y + 80, 0);              // +(-100,40) -(-100,120)
    vin->props.ac_voltage.amplitude = 0.01; vin->props.ac_voltage.frequency = 100059.9;
    set_freq_sweep(vin, 20e3, 500e3, 0.5);                                               // fast sweep: 100 kHz needs sub-us steps
    Component *gi = add_comp(circuit, COMP_GROUND, x - 100, y + 140, 0);
    connect_terminals(circuit, vin, 1, gi, 0);
    Component *cc = hcap(circuit, x, y + 40, 10e-9);                                       // (-40,40)-(40,40)
    Component *r1 = vres(circuit, x + 100, y - 60, 47e3);                                  // (100,-100)-(100,-20)
    Component *r2 = vres(circuit, x + 100, y + 60, 10e3);                                  // (100,20)-(100,100)
    gnd_below(circuit, r2, 1, x + 100, y + 120);
    Component *q = add_comp(circuit, COMP_NPN_BJT, x + 200, y, 0);                        // B(180,0) C(220,-20) E(220,20)
    Component *re = vres(circuit, x + 220, y + 80, 1e3);                                   // (220,40)-(220,120)
    gnd_below(circuit, re, 1, x + 220, y + 140);
    Component *ce = vcap(circuit, x + 280, y + 60, 10e-6);                                 // (280,20)-(280,100)
    gnd_below(circuit, ce, 1, x + 280, y + 120);
    Component *l = add_comp(circuit, COMP_INDUCTOR, x + 300, y - 60, 90); l->props.inductor.inductance = 1e-3;   // (300,-100)-(300,-20)
    Component *ct = vcap(circuit, x + 360, y - 60, 2.53e-9);                               // (360,-100)-(360,-20)
    Component *rq = vres(circuit, x + 440, y - 60, 10e3);                                  // (440,-100)-(440,-20)
    Component *co = hcap(circuit, x + 480, y - 20, 10e-9);                                 // (440,-20)-(520,-20)
    Component *rl = vres(circuit, x + 560, y + 20, 100e3);                                 // (560,-20)-(560,60)
    gnd_below(circuit, rl, 1, x + 560, y + 80);
    add_label(circuit, x - 80, y - 160, "Single-tuned amplifier: gain g_m (Rq || RL) only near f0 = 1/(2 pi sqrt(LC)) = 100 kHz; sweep 20-500 kHz");
    int rail0 = TN(x, y - 100), rail1 = TN(x + 100, y - 100), rail2 = TN(x + 300, y - 100), rail3 = TN(x + 360, y - 100), rail4 = TN(x + 420, y - 100);
    TW(rail0, rail1); TW(rail1, rail2); TW(rail2, rail3); TW(rail3, rail4);
    int r1b = TN(x + 100, y - 20), bn = TN(x + 100, y), r2t = TN(x + 100, y + 20), base = TN(x + 180, y);
    TW(r1b, bn); TW(bn, r2t); TW(bn, base);
    int sp = TN(x - 100, y + 40), ccl = TN(x - 40, y + 40), ccr = TN(x + 40, y + 40), j1 = TN(x + 60, y + 40), j2 = TN(x + 60, y);
    TW(sp, ccl); TW(ccr, j1); TW(j1, j2); TW(j2, bn);
    int col = TN(x + 220, y - 20), lb = TN(x + 300, y - 20), ctb = TN(x + 360, y - 20), rqb = TN(x + 420, y - 20), col2 = TN(x + 440, y - 20);
    TW(col, lb); TW(lb, ctb); TW(ctb, rqb); TW(rqb, col2);
    int e = TN(x + 220, y + 20), cet = TN(x + 280, y + 20), ret = TN(x + 220, y + 40); TW(e, cet); TW(e, ret);
    { int vt = TN(x - 180, y - 100); TW(vt, rail0); vcc->node_ids[0] = vt; } r1->node_ids[0] = rail1; r1->node_ids[1] = r1b; r2->node_ids[0] = r2t; q->node_ids[0] = base; q->node_ids[1] = col; q->node_ids[2] = e;
    re->node_ids[0] = ret; ce->node_ids[0] = cet; l->node_ids[0] = rail2; l->node_ids[1] = lb; ct->node_ids[0] = rail3; ct->node_ids[1] = ctb; rq->node_ids[0] = rail4; rq->node_ids[1] = rqb;
    co->node_ids[0] = col2; co->node_ids[1] = TN(x + 520, y - 20);
    { int rlt = TN(x + 560, y - 20); TW(co->node_ids[1], rlt); rl->node_ids[0] = rlt; } vin->node_ids[0] = sp; cc->node_ids[0] = ccl; cc->node_ids[1] = ccr;
    return 16;
}

// Common base: base held at 3.75 V (22k/10k, bypassed), Re 3k (1 mA), Rc 4.7k; input into the emitter via 10 uF
static int place_common_base(Circuit *circuit, float x, float y) {
    Component *vcc = dc_rail(circuit, x, y - 100, 12.0); if (!vcc) return 0;               // +(0,-100)
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x - 100, y + 200, 0);             // +(-100,160) -(-100,240)
    vin->props.ac_voltage.amplitude = 0.01; vin->props.ac_voltage.frequency = 10e3;
    Component *gi = add_comp(circuit, COMP_GROUND, x - 100, y + 260, 0);
    connect_terminals(circuit, vin, 1, gi, 0);
    Component *r1 = vres(circuit, x + 100, y - 60, 22e3);                                  // (100,-100)-(100,-20)
    Component *r2 = vres(circuit, x + 100, y + 60, 10e3);                                  // (100,20)-(100,100)
    gnd_below(circuit, r2, 1, x + 100, y + 120);
    Component *cb = vcap(circuit, x + 40, y + 60, 10e-6);                                  // (40,20)-(40,100) base bypass
    gnd_below(circuit, cb, 1, x + 40, y + 120);
    Component *q = add_comp(circuit, COMP_NPN_BJT, x + 200, y, 0);                        // B(180,0) C(220,-20) E(220,20)
    Component *re = vres(circuit, x + 220, y + 100, 3e3);                                  // (220,60)-(220,140)
    gnd_below(circuit, re, 1, x + 220, y + 160);
    Component *ci = vcap(circuit, x + 160, y + 120, 10e-6);                                // (160,80)-(160,160) input coupling
    Component *rc = vres(circuit, x + 300, y - 60, 4.7e3);                                 // (300,-100)-(300,-20)
    Component *co = hcap(circuit, x + 380, y - 20, 10e-6);                                 // (340,-20)-(420,-20)
    Component *rl = vres(circuit, x + 420, y + 20, 100e3);                                 // (420,-20)-(420,60)
    gnd_below(circuit, rl, 1, x + 420, y + 80);
    add_label(circuit, x - 80, y - 160, "Common base: input at the emitter (R_in = r_e = 25 ohm), output in phase, A_v = g_m R_C = +188");
    int rail0 = TN(x, y - 100), rail1 = TN(x + 100, y - 100), rail2 = TN(x + 300, y - 100); TW(rail0, rail1); TW(rail1, rail2);
    int r1b = TN(x + 100, y - 20), bn = TN(x + 100, y), r2t = TN(x + 100, y + 20), base = TN(x + 180, y), cbt = TN(x + 40, y + 20), cbj = TN(x + 40, y);
    TW(r1b, bn); TW(bn, r2t); TW(bn, base); TW(bn, cbj); TW(cbj, cbt);
    int sp = TN(x - 100, y + 160), s1 = TN(x + 160, y + 160), cit = TN(x + 160, y + 80), cij = TN(x + 160, y + 60), e = TN(x + 220, y + 20), ej = TN(x + 220, y + 60);
    TW(sp, s1); TW(cit, cij); TW(cij, ej); TW(e, ej);
    int col = TN(x + 220, y - 20), rcb = TN(x + 300, y - 20), col2 = TN(x + 340, y - 20); TW(col, rcb); TW(rcb, col2);
    vcc->node_ids[0] = rail0; r1->node_ids[0] = rail1; r1->node_ids[1] = r1b; r2->node_ids[0] = r2t; cb->node_ids[0] = cbt;
    q->node_ids[0] = base; q->node_ids[1] = col; q->node_ids[2] = e; re->node_ids[0] = ej; ci->node_ids[0] = cit; ci->node_ids[1] = s1;
    rc->node_ids[0] = rail2; rc->node_ids[1] = rcb; co->node_ids[0] = col2; co->node_ids[1] = TN(x + 420, y - 20); rl->node_ids[0] = co->node_ids[1]; vin->node_ids[0] = sp;
    return 15;
}

// Darlington emitter follower driven through 100k; Re 100 ohm; 6 V DC + 1 Vpk input
static int place_darlington(Circuit *circuit, float x, float y) {
    Component *vcc = dc_rail(circuit, x, y - 100, 12.0); if (!vcc) return 0;               // +(0,-100)
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 100, 0);                   // +(0,60) -(0,140)
    vin->props.ac_voltage.amplitude = 1.0; vin->props.ac_voltage.frequency = 1000.0; vin->props.ac_voltage.offset = 6.0;
    Component *gi = add_comp(circuit, COMP_GROUND, x, y + 160, 0);
    connect_terminals(circuit, vin, 1, gi, 0);
    Component *rs = hres(circuit, x + 100, y + 60, 100e3);                                 // (60,60)-(140,60)
    Component *q1 = add_comp(circuit, COMP_NPN_BJT, x + 200, y, 0);                       // B(180,0) C(220,-20) E(220,20)
    Component *q2 = add_comp(circuit, COMP_NPN_BJT, x + 280, y + 60, 0);                  // B(260,60) C(300,40) E(300,80)
    Component *re = vres(circuit, x + 300, y + 120, 100.0); re->props.resistor.power_rating = 1.0;   // 46 mA emitter current: 1 W part
    gnd_below(circuit, re, 1, x + 300, y + 180);
    add_label(circuit, x - 40, y - 160, "Darlington follower: R_in ~ beta^2 R_E = 1 M, so a 100k source loses only 9 %; two V_BE drops (4.6 V DC out)");
    int rail0 = TN(x, y - 100), rail1 = TN(x + 220, y - 100), rail2 = TN(x + 300, y - 100), c1 = TN(x + 220, y - 20), c2 = TN(x + 300, y + 40);
    TW(rail0, rail1); TW(rail1, rail2); TW(rail1, c1); TW(rail2, c2);
    int sp = TN(x, y + 60), rsl = TN(x + 60, y + 60), rsr = TN(x + 140, y + 60), j1 = TN(x + 160, y + 60), j2 = TN(x + 160, y), b1 = TN(x + 180, y);
    TW(sp, rsl); TW(rsr, j1); TW(j1, j2); TW(j2, b1);
    int e1 = TN(x + 220, y + 20), e1j = TN(x + 220, y + 60), b2 = TN(x + 260, y + 60), e2 = TN(x + 300, y + 80);
    TW(e1, e1j); TW(e1j, b2);
    vcc->node_ids[0] = rail0; vin->node_ids[0] = sp; rs->node_ids[0] = rsl; rs->node_ids[1] = rsr;
    q1->node_ids[0] = b1; q1->node_ids[1] = c1; q1->node_ids[2] = e1; q2->node_ids[0] = b2; q2->node_ids[1] = c2; q2->node_ids[2] = e2; re->node_ids[0] = e2;
    return 9;
}

// SR latch: two NOR gates cross-coupled; S and R pulses 50 us wide at 0.2 / 0.6 ms (1 ms period)
static int place_sr_latch(Circuit *circuit, float x, float y) {
    Component *g1 = add_comp(circuit, COMP_NOR_GATE, x + 200, y + 40, 0);                 // A(160,20)=S B(160,60)=Q  OUT(240,40) = Qbar
    if (!g1) return 0;
    Component *g2 = add_comp(circuit, COMP_NOR_GATE, x + 200, y + 160, 0);                // A(160,140)=Qbar B(160,180)=R OUT(240,160) = Q
    Component *ps = add_comp(circuit, COMP_PULSE_SOURCE, x + 60, y - 20, 0);              // +(60,-60) -(60,20)
    ps->props.pulse_source.v_low = 0; ps->props.pulse_source.v_high = 5; ps->props.pulse_source.delay = 0.2e-3; ps->props.pulse_source.pulse_width = 50e-6; ps->props.pulse_source.period = 1e-3;
    Component *gs = add_comp(circuit, COMP_GROUND, x + 60, y + 40, 0);
    Component *pr = add_comp(circuit, COMP_PULSE_SOURCE, x + 40, y + 190, 0);             // +(40,150) -(40,230)
    pr->props.pulse_source.v_low = 0; pr->props.pulse_source.v_high = 5; pr->props.pulse_source.delay = 0.6e-3; pr->props.pulse_source.pulse_width = 50e-6; pr->props.pulse_source.period = 1e-3;
    Component *gr = add_comp(circuit, COMP_GROUND, x + 40, y + 250, 0);
    Component *lq = vres(circuit, x + 340, y + 200, 100e3);                                // (340,160)-(340,240) load on Q
    gnd_below(circuit, lq, 1, x + 340, y + 260);
    add_label(circuit, x, y - 110, "SR latch: S sets Q (0.2 ms), R resets it (0.6 ms); with S = R = 0 the cross-coupling holds the state");
    add_label(circuit, x + 250, y + 25, "Qbar"); add_label(circuit, x + 250, y + 145, "Q");
    int spp = TN(x + 60, y - 60), s1 = TN(x + 140, y - 60), s2 = TN(x + 140, y + 20), a1 = TN(x + 160, y + 20);
    TW(spp, s1); TW(s1, s2); TW(s2, a1);
    // Qbar (G1 out) -> G2 A: right, down, left, into (160,140)
    int qb = TN(x + 240, y + 40), q1 = TN(x + 280, y + 40), q2 = TN(x + 280, y + 100), q3 = TN(x + 130, y + 100), q4 = TN(x + 130, y + 140), a2 = TN(x + 160, y + 140);
    TW(qb, q1); TW(q1, q2); TW(q2, q3); TW(q3, q4); TW(q4, a2);
    // Q (G2 out) -> G1 B: right, down below everything, far left, up, into (160,60)
    int q = TN(x + 240, y + 160), n1 = TN(x + 300, y + 160), n2 = TN(x + 300, y + 300), n3 = TN(x, y + 300), n4 = TN(x, y + 60), b1 = TN(x + 160, y + 60), lqt = TN(x + 340, y + 160);
    TW(q, n1); TW(n1, n2); TW(n2, n3); TW(n3, n4); TW(n4, b1); TW(n1, lqt);
    int rpp = TN(x + 40, y + 150), r1 = TN(x + 110, y + 150), r2 = TN(x + 110, y + 180), b2 = TN(x + 160, y + 180);
    TW(rpp, r1); TW(r1, r2); TW(r2, b2);
    g1->node_ids[0] = a1; g1->node_ids[1] = b1; g1->node_ids[2] = qb; g2->node_ids[0] = a2; g2->node_ids[1] = b2; g2->node_ids[2] = q;
    ps->node_ids[0] = spp; pr->node_ids[0] = rpp; lq->node_ids[0] = lqt;
    connect_terminals(circuit, ps, 1, gs, 0);
    connect_terminals(circuit, pr, 1, gr, 0);
    return 8;
}

// three-phase fan-out from a SOURCE_3PH at (x+60,y+60): rows at ry[k]; returns the row start node ids in out[3]
static Component *three_phase_fanout(Circuit *circuit, float x, float y, double vpk, double lseries, const float ry[3], int out[3]) {
    Component *g = add_comp(circuit, COMP_SOURCE_3PH, x + 60, y + 60, 0);                 // A(100,40) B(100,60) C(100,80) N(60,100)
    if (!g) return NULL;
    g->props.source_3ph.v_peak = vpk; g->props.source_3ph.l_series = lseries;
    Component *gn = add_comp(circuit, COMP_GROUND, x + 60, y + 120, 0);
    int a = TN(x + 100, y + 40), b = TN(x + 100, y + 60), c = TN(x + 100, y + 80), nn = TN(x + 60, y + 100);
    g->node_ids[0] = a; g->node_ids[1] = b; g->node_ids[2] = c; g->node_ids[3] = nn; gn->node_ids[0] = nn;
    int a1 = TN(x + 120, y + 40), a2 = TN(x + 120, ry[0]), a3 = TN(x + 160, ry[0]); TW(a, a1); TW(a1, a2); TW(a2, a3);
    int b1 = TN(x + 140, y + 60), b2 = TN(x + 140, ry[1]), b3 = TN(x + 160, ry[1]); TW(b, b1); TW(b1, b2); TW(b2, b3);
    int c1 = TN(x + 120, y + 80), c2 = TN(x + 120, ry[2]), c3 = TN(x + 160, ry[2]); TW(c, c1); TW(c1, c2); TW(c2, c3);   // x+160,y+80 is transformer B's P2
    out[0] = a3; out[1] = b3; out[2] = c3;
    return g;
}
// transformer at (tx, ry+20): P1 (tx-50,ry) P2 (tx-50,ry+40)->gnd, S1 (tx+50,ry) S2 (tx+50,ry+40)->gnd; returns S1 node
static int xfmr_row(Circuit *circuit, float tx, float ry, double n_ratio, int in_node) {
    Component *t = add_comp(circuit, COMP_TRANSFORMER, tx, ry + 20, 0);
    t->props.transformer.turns_ratio = n_ratio;
    int p1 = TN(tx - 50, ry), p2 = TN(tx - 50, ry + 40), s1 = TN(tx + 50, ry), s2 = TN(tx + 50, ry + 40);
    if (in_node != p1) TW(in_node, p1);
    Component *g1 = add_comp(circuit, COMP_GROUND, tx - 50, ry + 80, 0), *g2 = add_comp(circuit, COMP_GROUND, tx + 50, ry + 80, 0);
    int gp = TN(tx - 50, ry + 60), gs = TN(tx + 50, ry + 60); TW(p2, gp); TW(s2, gs);
    g1->node_ids[0] = gp; g2->node_ids[0] = gs;
    t->node_ids[0] = p1; t->node_ids[1] = p2; t->node_ids[2] = s1; t->node_ids[3] = s2;
    return s1;
}

// Power plant: 18 kV generator (X'' 0.184 mH per phase) -> GSU bank 1:19.17 -> breakers -> 3 x 100 mi 345 kV lines -> 600 MW load
static int place_power_plant(Circuit *circuit, float x, float y) {
    static const float ry[3] = { -100, 40, 180 };
    float rows[3] = { y + ry[0], y + ry[1], y + ry[2] };
    int start[3];
    Component *g = three_phase_fanout(circuit, x, y, 14697.0, 0.184e-3, rows, start);
    if (!g) return 0;
    for (int k = 0; k < 3; k++) {
        float r = rows[k];
        int s1 = xfmr_row(circuit, x + 210, r, 19.17, start[k]);                          // P1 (160,r) S1 (260,r)
        Component *br = add_comp(circuit, COMP_SPST_SWITCH, x + 300, r, 0);               // (260,r)-(340,r) breaker
        br->props.switch_spst.closed = true;
        Component *tl = add_tline(circuit, x + 380, r, 0, 100.0, 0.06, 0.55, 8.0, 1);      // (340,r)-(420,r)
        Component *ld = vres(circuit, x + 460, r + 40, 198.4);                             // (460,r)-(460,r+80)
        gnd_below(circuit, ld, 1, x + 460, r + 100);
        int bl = TN(x + 340, r), tr = TN(x + 420, r), lt = TN(x + 460, r);
        TW(tr, lt);
        br->node_ids[0] = s1; br->node_ids[1] = bl; tl->node_ids[0] = bl; tl->node_ids[1] = tr; ld->node_ids[0] = lt;
    }
    add_label(circuit, x + 20, y - 240, "Power plant: 18 kV generator (X'' = 0.15 pu) -> GSU bank 18/345 kV -> 345 kV breakers -> 100 mi lines -> 600 MW");
    add_label(circuit, x + 280, y - 190, "open a breaker: that phase's load drops, the others keep going (unbalanced)");
    return 22;
}

// Transmission substation: 345 kV grid -> 3 x 50 mi lines -> breakers -> 345/138 kV autos -> 138 kV bus ->
// 30 mi feeders into 90 MW pf 0.9 loads, with a switchable 6.1 uF cap bank per phase on the far bus
static int place_substation(Circuit *circuit, float x, float y) {
    static const float ry[3] = { -140, 80, 300 };
    float rows[3] = { y + ry[0], y + ry[1], y + ry[2] };
    int start[3];
    Component *g = three_phase_fanout(circuit, x, y, 281700.0, 0.0, rows, start);
    if (!g) return 0;
    for (int k = 0; k < 3; k++) {
        float r = rows[k];
        Component *tl = add_tline(circuit, x + 200, r, 0, 50.0, 0.06, 0.55, 8.0, 1);       // (160,r)-(240,r)
        Component *br = add_comp(circuit, COMP_SPST_SWITCH, x + 280, r, 0);               // (240,r)-(320,r)
        br->props.switch_spst.closed = true;
        int brl = TN(x + 240, r), brr = TN(x + 320, r);
        tl->node_ids[0] = start[k]; tl->node_ids[1] = brl; br->node_ids[0] = brl; br->node_ids[1] = brr;
        int s1 = xfmr_row(circuit, x + 370, r, 0.4, brr);                                  // P1 (320,r) S1 (420,r)
        Component *fd = add_tline(circuit, x + 480, r, 0, 30.0, 0.13, 0.72, 6.0, 1);       // (440,r)-(520,r)
        int fl = TN(x + 440, r), fr = TN(x + 520, r), lt = TN(x + 560, r), ct = TN(x + 620, r);
        TW(s1, fl); TW(fr, lt); TW(lt, ct);
        fd->node_ids[0] = fl; fd->node_ids[1] = fr;
        Component *rl = vres(circuit, x + 560, r + 40, 171.5);                             // (560,r)-(560,r+80)
        Component *ll = add_comp(circuit, COMP_INDUCTOR, x + 560, r + 120, 90); ll->props.inductor.inductance = 0.22;   // (560,r+80)-(560,r+160)
        gnd_below(circuit, ll, 1, x + 560, r + 180);
        int mid = TN(x + 560, r + 80);
        rl->node_ids[0] = lt; rl->node_ids[1] = mid; ll->node_ids[0] = mid;
        Component *sw = add_comp(circuit, COMP_SPST_SWITCH, x + 620, r + 40, 90);         // (620,r)-(620,r+80)
        sw->props.switch_spst.closed = false;
        Component *cb = vcap(circuit, x + 620, r + 120, 6.1e-6);                           // (620,r+80)-(620,r+160)
        gnd_below(circuit, cb, 1, x + 620, r + 180);
        int swb = TN(x + 620, r + 80);
        sw->node_ids[0] = ct; sw->node_ids[1] = swb; cb->node_ids[0] = swb;
    }
    add_label(circuit, x + 20, y - 290, "Transmission substation: 345 kV grid -> 50 mi lines -> breakers -> 345/138 kV autotransformers -> 138 kV bus");
    add_label(circuit, x + 20, y - 230, "-> 30 mi feeders into 90 MW pf 0.9 loads. Close the cap-bank switches: the far bus recovers ~5 %");
    return 34;
}
#undef TN
#undef TW

// Output node to probe for each template (component type, ordinal among that type, terminal)
/* A probe the template places. `name` is what it is called on the schematic and on the scope -
   "CH1" says which trace it is but not what it is on, which is the thing you actually want to
   know when you look at a circuit you did not draw. Left NULL, a name is derived from the part
   and terminal it sits on, so no probe is ever unnamed. */
typedef struct { ComponentType ct; int ord, term; const char *name; } TemplateProbeSpec;
// ---------------------------------------------------------------------------------------
// IC I/O and drivers: what a GPIO pin is made of and how it talks to the outside world.
// Push-pull / open-drain / open-collector outputs, wired-AND buses, level shifting, input
// pull-ups and debouncing, low- and high-side load switches, SPI/UART/RS-485/SPMI signalling.
// Every builder keeps a 20 px minimum between distinct nodes (10 px auto-merge).
// ---------------------------------------------------------------------------------------
#define TN(cx, cy) circuit_find_or_create_node(circuit, (cx), (cy), 5.0f)
#define TW(a_, b_) circuit_add_wire(circuit, (a_), (b_))
// logic pulse source at (x,y+40): +(x,y) -(x,y+80) -> ground
static Component *logic_pulse(Circuit *circuit, float x, float y, double v, double f, double duty, double delay) {
    Component *p = add_comp(circuit, COMP_PULSE_SOURCE, x, y + 40, 0);
    if (!p) return NULL;
    p->props.pulse_source.v_low = 0; p->props.pulse_source.v_high = v; p->props.pulse_source.delay = delay;
    p->props.pulse_source.period = 1.0 / f; p->props.pulse_source.pulse_width = duty / f;
    p->props.pulse_source.rise_time = p->props.pulse_source.fall_time = 0.01 / f;   // 1 % of the period: realistic GPIO edges, and the BJT stages stay solvable
    Component *g = add_comp(circuit, COMP_GROUND, x, y + 120, 0);
    connect_terminals(circuit, p, 1, g, 0);
    return p;
}
static Component *io_nmos(Circuit *circuit, float x, float y, double w) {   // G(x-20,y) D(x+20,y-20) S(x+20,y+20), source grounded below
    Component *m = add_comp(circuit, COMP_NMOS, x, y, 0); m->props.mosfet.w = w;
    gnd_below(circuit, m, 2, x + 20, y + 40);
    return m;
}

// 1. Push-pull (CMOS totem-pole) output: PMOS to 3.3 V, NMOS to ground, one input drives both gates
static int place_io_push_pull(Circuit *circuit, float x, float y) {
    Component *vdd = dc_rail(circuit, x, y - 140, 3.3); if (!vdd) return 0;               // +(0,-140)
    Component *in = logic_pulse(circuit, x, y + 40, 3.3, 1e6, 0.5, 0);                     // +(0,40)
    Component *rin = hres(circuit, x + 100, y + 40, 100.0);                                // (60,40)-(140,40)
    Component *mp = add_comp(circuit, COMP_PMOS, x + 240, y - 40, 0); mp->props.mosfet.w = 200e-6;   // G(220,-40) D(260,-60) S(260,-20)
    Component *mn = io_nmos(circuit, x + 240, y + 120, 100e-6);                             // G(220,120) D(260,100) S(260,140)
    Component *rl = vres(circuit, x + 420, y + 60, 10e3);                                  // (420,20)-(420,100)
    gnd_below(circuit, rl, 1, x + 420, y + 120);
    Component *cl = vcap(circuit, x + 500, y + 60, 20e-12);                                // (500,20)-(500,100)
    gnd_below(circuit, cl, 1, x + 500, y + 120);
    add_label(circuit, x - 40, y - 200, "Push-pull (CMOS) output: PMOS sources, NMOS sinks - drives both ways, never leave two of them fighting on one wire");
    int rail0 = TN(x, y - 140), rail1 = TN(x + 300, y - 140), ps = TN(x + 260, y - 20), psj = TN(x + 300, y - 20);
    TW(rail0, rail1); TW(rail1, psj); TW(psj, ps);
    int sp = TN(x, y + 40), rl_ = TN(x + 60, y + 40), rr = TN(x + 140, y + 40), gb = TN(x + 180, y + 40), gp = TN(x + 180, y - 40), gn = TN(x + 180, y + 120), gpt = TN(x + 220, y - 40), gnt = TN(x + 220, y + 120);
    TW(sp, rl_); TW(rr, gb); TW(gb, gp); TW(gb, gn); TW(gp, gpt); TW(gn, gnt);
    int pd = TN(x + 260, y - 60), ob0 = TN(x + 340, y - 60), ob1 = TN(x + 340, y + 100), nd = TN(x + 260, y + 100), out = TN(x + 340, y + 20), lt = TN(x + 420, y + 20), ct = TN(x + 500, y + 20);
    TW(pd, ob0); TW(ob0, out); TW(out, ob1); TW(ob1, nd); TW(out, lt); TW(lt, ct);
    vdd->node_ids[0] = rail0; in->node_ids[0] = sp; rin->node_ids[0] = rl_; rin->node_ids[1] = rr;
    mp->node_ids[0] = gpt; mp->node_ids[1] = pd; mp->node_ids[2] = ps; mn->node_ids[0] = gnt; mn->node_ids[1] = nd;
    rl->node_ids[0] = lt; cl->node_ids[0] = ct;
    return 12;
}

// 2. Open-drain output with an external pull-up: fast fall through the NMOS, RC rise through the pull-up
static int place_io_open_drain(Circuit *circuit, float x, float y) {
    Component *vdd = dc_rail(circuit, x, y - 140, 3.3); if (!vdd) return 0;
    Component *in = logic_pulse(circuit, x, y + 40, 3.3, 200e3, 0.5, 0);
    Component *rin = hres(circuit, x + 100, y + 40, 100.0);                                // (60,40)-(140,40)
    Component *mn = io_nmos(circuit, x + 240, y + 40, 100e-6);                              // G(220,40) D(260,20) S(260,60)
    Component *rpu = vres(circuit, x + 340, y - 60, 4.7e3);                                // (340,-100)-(340,-20)
    Component *cl = vcap(circuit, x + 420, y + 20, 100e-12);                               // (420,-20)-(420,60)
    gnd_below(circuit, cl, 1, x + 420, y + 80);
    add_label(circuit, x - 40, y - 200, "Open-drain output + 4.7k pull-up: the pin can only pull LOW; the resistor makes the HIGH (tau = R C = 470 ns)");
    int rail0 = TN(x, y - 140), rail1 = TN(x + 340, y - 140), rt = TN(x + 340, y - 100); TW(rail0, rail1); TW(rail1, rt);
    int sp = TN(x, y + 40), rl_ = TN(x + 60, y + 40), rr = TN(x + 140, y + 40), g = TN(x + 220, y + 40); TW(sp, rl_); TW(rr, g);
    int d = TN(x + 260, y + 20), dj = TN(x + 340, y + 20), out = TN(x + 340, y - 20), ct = TN(x + 420, y - 20); TW(d, dj); TW(dj, out); TW(out, ct);
    vdd->node_ids[0] = rail0; in->node_ids[0] = sp; rin->node_ids[0] = rl_; rin->node_ids[1] = rr; mn->node_ids[0] = g; mn->node_ids[1] = d;
    rpu->node_ids[0] = rt; rpu->node_ids[1] = out; cl->node_ids[0] = ct;
    return 10;
}

// 3. Open-collector NPN with the pull-up on a different rail: 3.3 V logic drives a 5 V line (inverted)
static int place_io_open_collector(Circuit *circuit, float x, float y) {
    Component *v5 = dc_rail(circuit, x, y - 140, 5.0); if (!v5) return 0;
    Component *in = logic_pulse(circuit, x, y + 40, 3.3, 100e3, 0.5, 0);
    Component *rb = hres(circuit, x + 100, y + 40, 1e3);                                   // (60,40)-(140,40)
    Component *q = add_comp(circuit, COMP_NPN_BJT, x + 240, y + 40, 0);                   // B(220,40) C(260,20) E(260,60)
    gnd_below(circuit, q, 2, x + 260, y + 80);
    Component *rpu = vres(circuit, x + 340, y - 60, 4.7e3);                                // (340,-100)-(340,-20)
    Component *cl = vres(circuit, x + 420, y + 20, 100e3);                                 // 5 V receiver input
    gnd_below(circuit, cl, 1, x + 420, y + 80);
    add_label(circuit, x - 40, y - 200, "Open-collector level shift: a 3.3 V pin drives a 5 V line through an NPN (1k base) - output is inverted, 0 / 5 V");
    int rail0 = TN(x, y - 140), rail1 = TN(x + 340, y - 140), rt = TN(x + 340, y - 100); TW(rail0, rail1); TW(rail1, rt);
    int sp = TN(x, y + 40), rl_ = TN(x + 60, y + 40), rr = TN(x + 140, y + 40), b = TN(x + 220, y + 40); TW(sp, rl_); TW(rr, b);
    int c = TN(x + 260, y + 20), cj = TN(x + 340, y + 20), out = TN(x + 340, y - 20), ct = TN(x + 420, y - 20); TW(c, cj); TW(cj, out); TW(out, ct);
    v5->node_ids[0] = rail0; in->node_ids[0] = sp; rb->node_ids[0] = rl_; rb->node_ids[1] = rr; q->node_ids[0] = b; q->node_ids[1] = c;
    rpu->node_ids[0] = rt; rpu->node_ids[1] = out; cl->node_ids[0] = ct;
    return 10;
}

// 4. I2C SDA: two open-drain devices on one pulled-up wire (wired-AND) - whoever pulls, the line is low
static int place_io_i2c_bus(Circuit *circuit, float x, float y) {
    Component *vdd = dc_rail(circuit, x, y - 140, 3.3); if (!vdd) return 0;
    Component *ma = logic_pulse(circuit, x, y + 40, 3.3, 50e3, 0.3, 0);                    // master, +(0,40)
    Component *ra = hres(circuit, x + 100, y + 40, 100.0);
    Component *mna = io_nmos(circuit, x + 240, y + 40, 100e-6);                             // D(260,20)
    Component *sb = logic_pulse(circuit, x, y + 240, 3.3, 20e3, 0.2, 15e-6);               // slave, +(0,240)
    Component *rb_ = hres(circuit, x + 100, y + 240, 100.0);
    Component *mnb = io_nmos(circuit, x + 240, y + 240, 100e-6);                            // D(260,220)
    Component *rpu = vres(circuit, x + 340, y - 60, 4.7e3);                                // (340,-100)-(340,-20)
    Component *cb = vcap(circuit, x + 420, y + 20, 200e-12);                               // bus capacitance
    gnd_below(circuit, cb, 1, x + 420, y + 80);
    add_label(circuit, x - 40, y - 200, "I2C SDA (wired-AND): master and slave are both open-drain; the 4.7k pull-up + 200 pF bus set the rise time (~1 us)");
    int rail0 = TN(x, y - 140), rail1 = TN(x + 340, y - 140), rt = TN(x + 340, y - 100); TW(rail0, rail1); TW(rail1, rt);
    int spa = TN(x, y + 40), ral = TN(x + 60, y + 40), rar = TN(x + 140, y + 40), ga = TN(x + 220, y + 40); TW(spa, ral); TW(rar, ga);
    int spb = TN(x, y + 240), rbl = TN(x + 60, y + 240), rbr = TN(x + 140, y + 240), gb = TN(x + 220, y + 240); TW(spb, rbl); TW(rbr, gb);
    int da = TN(x + 260, y + 20), daj = TN(x + 340, y + 20), db = TN(x + 260, y + 220), dbj = TN(x + 340, y + 220), sda = TN(x + 340, y - 20), ct = TN(x + 420, y - 20);
    TW(da, daj); TW(db, dbj); TW(sda, daj); TW(daj, dbj); TW(sda, ct);
    vdd->node_ids[0] = rail0; ma->node_ids[0] = spa; ra->node_ids[0] = ral; ra->node_ids[1] = rar; mna->node_ids[0] = ga; mna->node_ids[1] = da;
    sb->node_ids[0] = spb; rb_->node_ids[0] = rbl; rb_->node_ids[1] = rbr; mnb->node_ids[0] = gb; mnb->node_ids[1] = db;
    rpu->node_ids[0] = rt; rpu->node_ids[1] = sda; cb->node_ids[0] = ct;
    return 15;
}

// 5. Bidirectional I2C level shifter: one NMOS, gate at 3.3 V, pull-ups on both sides
static int place_io_i2c_level(Circuit *circuit, float x, float y) {
    Component *v33 = dc_rail(circuit, x, y - 140, 3.3); if (!v33) return 0;                // +(0,-140)
    Component *in = logic_pulse(circuit, x, y + 40, 3.3, 100e3, 0.5, 0);
    Component *rin = hres(circuit, x + 100, y + 40, 100.0);
    Component *mdrv = io_nmos(circuit, x + 240, y + 40, 100e-6);                            // D(260,20)
    Component *rpul = vres(circuit, x + 340, y - 60, 4.7e3);                               // (340,-100)-(340,-20) low side pull-up
    Component *cl = vcap(circuit, x + 400, y + 20, 50e-12);                                // (400,-20)-(400,60)
    gnd_below(circuit, cl, 1, x + 400, y + 80);
    Component *msh = add_comp(circuit, COMP_NMOS, x + 500, y + 20, 0); msh->props.mosfet.w = 100e-6;   // G(480,20) D(520,0) S(520,40)
    Component *rpuh = vres(circuit, x + 600, y - 60, 4.7e3);                               // (600,-100)-(600,-20) high side pull-up
    Component *v5 = dc_rail(circuit, x + 700, y - 140, 5.0);                               // +(700,-140)
    Component *ch = vcap(circuit, x + 660, y + 20, 50e-12);                                // (660,-20)-(660,60)
    gnd_below(circuit, ch, 1, x + 660, y + 80);
    add_label(circuit, x - 40, y - 200, "I2C level shifter: NMOS gate at 3.3 V; pull the 3.3 V side low and the 5 V side follows (either side can drive, pull-ups both sides)");
    int rail0 = TN(x, y - 140), rail1 = TN(x + 340, y - 140), rlt = TN(x + 340, y - 100); TW(rail0, rail1); TW(rail1, rlt);
    int sp = TN(x, y + 40), rl_ = TN(x + 60, y + 40), rr = TN(x + 140, y + 40), g = TN(x + 220, y + 40); TW(sp, rl_); TW(rr, g);
    int d = TN(x + 260, y + 20), dj = TN(x + 340, y + 20), low = TN(x + 340, y - 20), clt = TN(x + 400, y - 20), lj = TN(x + 440, y - 20), lj2 = TN(x + 440, y + 40), ssh = TN(x + 520, y + 40);
    TW(d, dj); TW(dj, low); TW(low, clt); TW(clt, lj); TW(lj, lj2); TW(lj2, ssh);
    int gsh = TN(x + 480, y + 20), gj = TN(x + 480, y - 100); TW(gsh, gj); TW(gj, rlt);           // gate tied to the 3.3 V rail
    int dsh = TN(x + 520, y), hj = TN(x + 600, y), high = TN(x + 600, y - 20), rht = TN(x + 600, y - 100), rail5 = TN(x + 700, y - 140), rail5j = TN(x + 600, y - 140), cht = TN(x + 660, y - 20);
    TW(dsh, hj); TW(hj, high); TW(rail5, rail5j); TW(rail5j, rht); TW(high, cht);
    v33->node_ids[0] = rail0; in->node_ids[0] = sp; rin->node_ids[0] = rl_; rin->node_ids[1] = rr; mdrv->node_ids[0] = g; mdrv->node_ids[1] = d;
    rpul->node_ids[0] = rlt; rpul->node_ids[1] = low; cl->node_ids[0] = clt; msh->node_ids[0] = gsh; msh->node_ids[1] = dsh; msh->node_ids[2] = ssh;
    rpuh->node_ids[0] = rht; rpuh->node_ids[1] = high; v5->node_ids[0] = rail5; ch->node_ids[0] = cht;
    return 16;
}

// 6. GPIO input: pull-up, push-button to ground, RC debounce, Schmitt-style inverter
static int place_io_input_debounce(Circuit *circuit, float x, float y) {
    Component *vdd = dc_rail(circuit, x, y - 140, 3.3); if (!vdd) return 0;
    Component *rpu = vres(circuit, x + 200, y - 60, 10e3);                                 // (200,-100)-(200,-20)
    Component *sw = add_comp(circuit, COMP_ANALOG_SWITCH, x + 260, y + 20, 0);            // IN(220,20) OUT(300,20) CTL(260,40)
    sw->props.analog_switch.v_on = 2.5; sw->props.analog_switch.v_off = 0.8; sw->props.analog_switch.r_on = 1.0; sw->props.analog_switch.ideal = false;
    gnd_below(circuit, sw, 1, x + 300, y + 40);
    Component *btn = logic_pulse(circuit, x + 260, y + 100, 3.3, 50.0, 0.5, 0);            // "press" = high for 10 ms, +(260,100)
    Component *rd = hres(circuit, x + 400, y - 20, 10e3);                                  // (360,-20)-(440,-20)
    Component *cd = vcap(circuit, x + 440, y + 20, 100e-9);                                // (440,-20)-(440,60)
    gnd_below(circuit, cd, 1, x + 440, y + 80);
    Component *inv = add_comp(circuit, COMP_NOT_GATE, x + 540, y - 20, 0);                // IN(500,-20) OUT(580,-20)
    inv->props.logic_gate.v_high = 3.3; inv->props.logic_gate.v_threshold = 1.65;
    Component *rl = vres(circuit, x + 620, y + 20, 100e3);                                 // (620,-20)-(620,60)
    gnd_below(circuit, rl, 1, x + 620, y + 80);
    add_label(circuit, x - 40, y - 200, "GPIO input: 10k pull-up idles the pin HIGH, the button shorts it LOW; 10k + 100 nF (tau 1 ms) debounces before the inverter");
    int rail0 = TN(x, y - 140), rail1 = TN(x + 200, y - 140), rt = TN(x + 200, y - 100); TW(rail0, rail1); TW(rail1, rt);
    int pin = TN(x + 200, y - 20), pj = TN(x + 200, y + 20), swin = TN(x + 220, y + 20), ctl = TN(x + 260, y + 40), bp = TN(x + 260, y + 100);
    TW(pin, pj); TW(pj, swin); TW(ctl, bp);
    int rdl = TN(x + 360, y - 20), rdr = TN(x + 440, y - 20), gi = TN(x + 500, y - 20), go = TN(x + 580, y - 20), lt = TN(x + 620, y - 20);
    TW(pin, rdl); TW(rdr, gi); TW(go, lt);
    vdd->node_ids[0] = rail0; rpu->node_ids[0] = rt; rpu->node_ids[1] = pin; sw->node_ids[0] = swin; sw->node_ids[2] = ctl; btn->node_ids[0] = bp;
    rd->node_ids[0] = rdl; rd->node_ids[1] = rdr; cd->node_ids[0] = rdr; inv->node_ids[0] = gi; inv->node_ids[1] = go; rl->node_ids[0] = lt;
    return 13;
}

// 7. Low-side NMOS switch on an inductive load (relay coil) with a flyback diode
static int place_io_low_side(Circuit *circuit, float x, float y) {
    Component *v12 = dc_rail(circuit, x, y - 220, 12.0); if (!v12) return 0;               // +(0,-220)
    Component *in = logic_pulse(circuit, x, y + 40, 5.0, 500.0, 0.5, 0);
    Component *rg = hres(circuit, x + 100, y + 40, 100.0);
    Component *mn = io_nmos(circuit, x + 240, y + 40, 1e-3);                                // D(260,20)
    Component *rcoil = vres(circuit, x + 340, y - 180, 50.0); rcoil->props.resistor.power_rating = 5.0;   // (340,-220)-(340,-140); 2.9 W coil resistance
    Component *lcoil = add_comp(circuit, COMP_INDUCTOR, x + 340, y - 100, 90); lcoil->props.inductor.inductance = 10e-3;   // (340,-140)-(340,-60)
    Component *dfb = add_comp(circuit, COMP_DIODE, x + 420, y - 140, 270);                // A(420,-100) K(420,-180)
    add_label(circuit, x - 40, y - 280, "Low-side switch: 5 V gate drive, NMOS sinks the coil current; the flyback diode clamps the turn-off spike to 12.7 V (delete it: kV!)");
    int rail0 = TN(x, y - 220), rail1 = TN(x + 340, y - 220), rail2 = TN(x + 420, y - 220), kt = TN(x + 420, y - 180); TW(rail0, rail1); TW(rail1, rail2); TW(rail2, kt);
    int sp = TN(x, y + 40), rl_ = TN(x + 60, y + 40), rr = TN(x + 140, y + 40), g = TN(x + 220, y + 40); TW(sp, rl_); TW(rr, g);
    int lb = TN(x + 340, y - 60), dj = TN(x + 340, y + 20), d = TN(x + 260, y + 20), at = TN(x + 420, y - 100), aj = TN(x + 420, y - 60);
    TW(lb, dj); TW(dj, d); TW(at, aj); TW(aj, lb);
    v12->node_ids[0] = rail0; in->node_ids[0] = sp; rg->node_ids[0] = rl_; rg->node_ids[1] = rr; mn->node_ids[0] = g; mn->node_ids[1] = d;
    rcoil->node_ids[0] = rail1; rcoil->node_ids[1] = TN(x + 340, y - 140); lcoil->node_ids[0] = rcoil->node_ids[1]; lcoil->node_ids[1] = lb;
    dfb->node_ids[0] = at; dfb->node_ids[1] = kt;
    return 10;
}

// 8. High-side PMOS switch driven from 3.3 V logic through an NPN level shifter
static int place_io_high_side(Circuit *circuit, float x, float y) {
    Component *v12 = dc_rail(circuit, x, y - 140, 12.0); if (!v12) return 0;               // +(0,-140)
    Component *in = logic_pulse(circuit, x, y + 100, 3.3, 500.0, 0.5, 0);                  // +(0,100)
    Component *rb = hres(circuit, x + 100, y + 100, 10e3);                                 // (60,100)-(140,100)
    Component *q = add_comp(circuit, COMP_NPN_BJT, x + 180, y + 100, 0);                  // B(160,100) C(200,80) E(200,120)
    gnd_below(circuit, q, 2, x + 200, y + 140);
    Component *rgate = vres(circuit, x + 200, y - 40, 10e3);                               // (200,-80)-(200,0)
    Component *mp = add_comp(circuit, COMP_PMOS, x + 300, y - 20, 0); mp->props.mosfet.w = 1e-3;   // G(280,-20) D(320,-40) S(320,0)
    Component *rload = vres(circuit, x + 420, y + 40, 100.0); rload->props.resistor.power_rating = 3.0;    // (420,0)-(420,80); 1.44 W at 12 V
    gnd_below(circuit, rload, 1, x + 420, y + 100);
    add_label(circuit, x - 40, y - 200, "High-side switch: logic HIGH turns the NPN on, pulls the PMOS gate to 0 V (Vgs = -12), the load gets the full 12 V rail");
    int rail0 = TN(x, y - 140), rail1 = TN(x + 200, y - 140), rail2 = TN(x + 360, y - 140), rgt = TN(x + 200, y - 80), sj = TN(x + 360, y), s = TN(x + 320, y);
    TW(rail0, rail1); TW(rail1, rail2); TW(rail1, rgt); TW(rail2, sj); TW(sj, s);
    int sp = TN(x, y + 100), rbl = TN(x + 60, y + 100), rbr = TN(x + 140, y + 100), b = TN(x + 160, y + 100); TW(sp, rbl); TW(rbr, b);
    int rgb = TN(x + 200, y), c = TN(x + 200, y + 80), gj = TN(x + 240, y + 80), gj2 = TN(x + 240, y - 20), g = TN(x + 280, y - 20);
    TW(rgb, c); TW(c, gj); TW(gj, gj2); TW(gj2, g);
    int d = TN(x + 320, y - 40), dj = TN(x + 420, y - 40), lt = TN(x + 420, y); TW(d, dj); TW(dj, lt);
    v12->node_ids[0] = rail0; in->node_ids[0] = sp; rb->node_ids[0] = rbl; rb->node_ids[1] = rbr; q->node_ids[0] = b; q->node_ids[1] = c;
    rgate->node_ids[0] = rgt; rgate->node_ids[1] = rgb; mp->node_ids[0] = g; mp->node_ids[1] = d; mp->node_ids[2] = s; rload->node_ids[0] = lt;
    return 10;
}

// two push-pull lines through series resistors into a cable/trace capacitance (SPI, SPMI)
static int io_two_lines(Circuit *circuit, float x, float y, double v, double f1, double f2, double delay2, double rs, double cline) {
    Component *a = logic_pulse(circuit, x, y + 40, v, f1, 0.5, 0); if (!a) return 0;      // +(0,40)
    Component *ra = hres(circuit, x + 100, y + 40, rs); ra->props.resistor.power_rating = 0.5;
    Component *ca = vcap(circuit, x + 200, y + 80, cline);                                 // (200,40)-(200,120)
    gnd_below(circuit, ca, 1, x + 200, y + 140);
    Component *b = logic_pulse(circuit, x, y + 240, v, f2, 0.5, delay2);                   // +(0,240)
    Component *rb_ = hres(circuit, x + 100, y + 240, rs); rb_->props.resistor.power_rating = 0.5;
    Component *cb = vcap(circuit, x + 200, y + 280, cline);
    gnd_below(circuit, cb, 1, x + 200, y + 340);
    int spa = TN(x, y + 40), ral = TN(x + 60, y + 40), rar = TN(x + 140, y + 40), cat = TN(x + 200, y + 40); TW(spa, ral); TW(rar, cat);
    int spb = TN(x, y + 240), rbl = TN(x + 60, y + 240), rbr = TN(x + 140, y + 240), cbt = TN(x + 200, y + 240); TW(spb, rbl); TW(rbr, cbt);
    a->node_ids[0] = spa; ra->node_ids[0] = ral; ra->node_ids[1] = rar; ca->node_ids[0] = cat;
    b->node_ids[0] = spb; rb_->node_ids[0] = rbl; rb_->node_ids[1] = rbr; cb->node_ids[0] = cbt;
    return 10;
}
// 9. SPI: SCLK 10 MHz and MOSI 5 MHz push-pull, 33 ohm series termination into 200 pF of ribbon cable
static int place_io_spi(Circuit *circuit, float x, float y) {
    int n = io_two_lines(circuit, x, y, 3.3, 10e6, 5e6, 0, 33.0, 200e-12);
    add_label(circuit, x - 40, y - 40, "SPI: SCLK 10 MHz / MOSI 5 MHz push-pull, 33 ohm series termination, 200 pF cable -> tau 6.6 ns rounds the edges");
    add_label(circuit, x - 40, y + 400, "Try: 1 nF (long cable) and the clock never reaches 3.3 V; 0 ohm and a real trace would ring");
    return n;
}
// 12. SPMI (MIPI): 1.8 V two-wire, SCLK 5 MHz push-pull, SDATA 2.5 MHz, 15 pF on-board loads
static int place_io_spmi(Circuit *circuit, float x, float y) {
    int n = io_two_lines(circuit, x, y, 1.8, 5e6, 2.5e6, 50e-9, 33.0, 15e-12);
    add_label(circuit, x - 40, y - 40, "SPMI (MIPI): 1.8 V SCLK 5 MHz and SDATA 2.5 MHz, push-pull through 33 ohm into 15 pF (PMIC on the same board)");
    add_label(circuit, x - 40, y + 400, "1.8 V logic: V_IH is only ~1.2 V, so 0.3 V of noise or ground bounce already matters; keep the return path short");
    return n;
}

// 10. UART between 5 V and 3.3 V parts: a divider one way, direct the other way (5 V V_IH = 2 V)
static int place_io_uart(Circuit *circuit, float x, float y) {
    Component *tx5 = logic_pulse(circuit, x, y + 40, 5.0, 4800.0, 0.5, 0); if (!tx5) return 0;   // +(0,40): 9600 baud alternating bits
    Component *rt = hres(circuit, x + 100, y + 40, 1e3);                                   // (60,40)-(140,40)
    Component *rbt = vres(circuit, x + 140, y + 80, 2e3);                                  // (140,40)-(140,120)
    gnd_below(circuit, rbt, 1, x + 140, y + 140);
    Component *crx = vcap(circuit, x + 220, y + 80, 10e-12);                               // (220,40)-(220,120) RX pin
    gnd_below(circuit, crx, 1, x + 220, y + 140);
    Component *tx33 = logic_pulse(circuit, x, y + 240, 3.3, 4800.0, 0.5, 52e-6);          // +(0,240)
    Component *rx5 = add_comp(circuit, COMP_NOT_GATE, x + 180, y + 240, 0);               // IN(140,240) OUT(220,240): 5 V logic input
    rx5->props.logic_gate.v_high = 5.0; rx5->props.logic_gate.v_threshold = 2.0;
    Component *rl = vres(circuit, x + 260, y + 280, 100e3);                                // (260,240)-(260,320)
    gnd_below(circuit, rl, 1, x + 260, y + 320);
    add_label(circuit, x - 40, y - 40, "UART 5 V <-> 3.3 V: TX(5 V) -> 1k/2k divider -> 3.33 V at the 3.3 V RX pin (never feed 5 V straight into a 3.3 V pin)");
    add_label(circuit, x - 40, y + 380, "The other way needs nothing: a 5 V TTL input takes anything above V_IH = 2 V as HIGH, so 3.3 V TX drives it directly");
    int sp = TN(x, y + 40), rl_ = TN(x + 60, y + 40), rr = TN(x + 140, y + 40), ct = TN(x + 220, y + 40); TW(sp, rl_); TW(rr, ct);
    int sp2 = TN(x, y + 240), gi = TN(x + 140, y + 240), go = TN(x + 220, y + 240), lt = TN(x + 260, y + 240); TW(sp2, gi); TW(go, lt);
    tx5->node_ids[0] = sp; rt->node_ids[0] = rl_; rt->node_ids[1] = rr; rbt->node_ids[0] = rr; crx->node_ids[0] = ct;
    tx33->node_ids[0] = sp2; rx5->node_ids[0] = gi; rx5->node_ids[1] = go; rl->node_ids[0] = lt;
    return 11;
}

// 11. RS-485 differential link: A and B driven in antiphase, 120 ohm at both ends, common-mode noise cancels at the receiver
static int place_io_rs485(Circuit *circuit, float x, float y) {
    Component *data = logic_pulse(circuit, x, y + 40, 5.0, 500e3, 0.5, 0); if (!data) return 0;   // +(0,40) = A driver
    Component *invb = add_comp(circuit, COMP_NOT_GATE, x + 100, y + 120, 0);              // IN(60,120) OUT(140,120) = B driver
    invb->props.logic_gate.v_high = 5.0; invb->props.logic_gate.v_threshold = 2.5; invb->props.logic_gate.r_out = 10.0;
    Component *rla = hres(circuit, x + 240, y + 40, 10.0);                                 // (200,40)-(280,40)
    Component *rlb = hres(circuit, x + 240, y + 120, 10.0);                                // (200,120)-(280,120)
    Component *rt0 = vres(circuit, x + 180, y + 80, 120.0);                                // (180,40)-(180,120) driver-end termination
    Component *rt = vres(circuit, x + 420, y + 80, 120.0);                                 // (420,40)-(420,120) far-end termination
    Component *na = add_comp(circuit, COMP_AC_VOLTAGE, x + 320, y, 0);                    // +(320,-40) -(320,40): noise into A
    na->props.ac_voltage.amplitude = 1.0; na->props.ac_voltage.frequency = 100e3;
    Component *nb = add_comp(circuit, COMP_AC_VOLTAGE, x + 340, y + 160, 0);              // +(340,120) -(340,200): same noise into B
    nb->props.ac_voltage.amplitude = 1.0; nb->props.ac_voltage.frequency = 100e3;
    Component *rx = sat_opamp(circuit, x + 520, y + 80);                                  // -(480,60) +(480,100) OUT(560,80)
    rx->props.opamp.slew_rate = 1000.0; rx->props.opamp.gbw = 1e9; rx->props.opamp.vmax = 5.0; rx->props.opamp.vmin = 0.0;
    Component *rl = vres(circuit, x + 600, y + 120, 10e3);                                 // (600,80)-(600,160)
    gnd_below(circuit, rl, 1, x + 600, y + 180);
    add_label(circuit, x - 40, y - 100, "RS-485: A = data, B = NOT data, 120 ohm at both ends; 1 V of common-mode noise rides on BOTH wires and the receiver (A-B) ignores it");
    int sp = TN(x, y + 40), ij = TN(x + 60, y + 40), gi = TN(x + 60, y + 120), t0a = TN(x + 180, y + 40), ral = TN(x + 200, y + 40);
    TW(sp, ij); TW(ij, gi); TW(ij, t0a); TW(t0a, ral);
    int go = TN(x + 140, y + 120), t0b = TN(x + 180, y + 120), rbl = TN(x + 200, y + 120); TW(go, t0b); TW(t0b, rbl);
    int rar = TN(x + 280, y + 40), nam = TN(x + 320, y + 40), nap = TN(x + 320, y - 40), aj = TN(x + 360, y - 40), aj2 = TN(x + 360, y + 40), ta = TN(x + 420, y + 40);
    TW(rar, nam); TW(nap, aj); TW(aj, aj2); TW(aj2, ta);
    int rbr = TN(x + 280, y + 120), bj = TN(x + 300, y + 120), bj2 = TN(x + 300, y + 200), nbm = TN(x + 340, y + 200), nbp = TN(x + 340, y + 120), tb = TN(x + 420, y + 120);
    TW(rbr, bj); TW(bj, bj2); TW(bj2, nbm); TW(nbp, tb);
    int ap = TN(x + 460, y + 40), ap2 = TN(x + 460, y + 100), rxp = TN(x + 480, y + 100), bm = TN(x + 440, y + 120), bm2 = TN(x + 440, y + 60), rxm = TN(x + 480, y + 60), out = TN(x + 560, y + 80), lt = TN(x + 600, y + 80);
    TW(ta, ap); TW(ap, ap2); TW(ap2, rxp); TW(tb, bm); TW(bm, bm2); TW(bm2, rxm); TW(out, lt);
    data->node_ids[0] = sp; invb->node_ids[0] = gi; invb->node_ids[1] = go; rla->node_ids[0] = ral; rla->node_ids[1] = rar; rlb->node_ids[0] = rbl; rlb->node_ids[1] = rbr;
    rt0->node_ids[0] = t0a; rt0->node_ids[1] = t0b; rt->node_ids[0] = ta; rt->node_ids[1] = tb;
    na->node_ids[0] = nap; na->node_ids[1] = nam; nb->node_ids[0] = nbp; nb->node_ids[1] = nbm;
    rx->node_ids[0] = rxm; rx->node_ids[1] = rxp; rx->node_ids[2] = out; rl->node_ids[0] = lt;
    return 12;
}
#undef TN
#undef TW

// ---------------------------------------------------------------------------------------
// Texas voltage levels, and the residential / commercial services at the end of the line.
//
// Standards the numbers are sized against (see docs/RESEARCH_ERCOT_STANDARDS.md):
//   ERCOT Planning Guide 4 / Nodal Operating Guide 2 - transmission bus 0.95-1.05 pu system
//     normal (NERC TPL-001-5.1 P0), 0.90-1.10 pu post-contingency (P1-P7).
//   NERC PRC-023 relay loadability (150 % of the highest emergency rating), VAR-001 reactive
//     planning, PRC-006 / ERCOT UFLS 59.3 / 58.9 / 58.5 Hz, PRC-024 ride-through.
//   AEP Texas: 345 / 138 / 69 kV transmission and subtransmission, 34.5 / 12.47 kV distribution,
//     LTC +/-10 % in 32 steps of 0.625 %, 0.95 pf design point at the delivery point.
//   ANSI C84.1 Range A: service 114-126 V, utilization 110-125 V (120 V base); 456-504 V for 480 V.
//   NEC 210.19(A)/215.2(A) informational notes: 3 % branch, 5 % feeder + branch voltage drop.
//   IEEE 1547-2018 / ERCOT DG: the PCC stays inside ANSI C84.1 Range A while exporting.
//   IEEE 1453 / AEP distribution planning: ~3 % voltage dip for infrequent motor starts.
//
// Everything transmission-side is a single-phase (phase-to-neutral) equivalent at 60 Hz.
// ---------------------------------------------------------------------------------------
#define TN(cx, cy) circuit_find_or_create_node(circuit, (cx), (cy), 5.0f)
#define TW(a_, b_) circuit_add_wire(circuit, (a_), (b_))

// horizontal transmission line: terminals (x-40,y) and (x+40,y); returns the far node
static int line_seg(Circuit *circuit, float x, float y, int in, double mi, double r, double xr, double b_us, int model) {
    Component *t = add_tline(circuit, x, y, 0, mi, r, xr, b_us, model);
    int l = TN(x - 40, y), rr = TN(x + 40, y);
    if (in >= 0 && in != l) TW(in, l);
    t->node_ids[0] = l; t->node_ids[1] = rr;
    return rr;
}
// horizontal resistance (conductor / service drop): terminals (x-40,y),(x+40,y)
static int res_seg(Circuit *circuit, float x, float y, int in, double R) {
    Component *r = hres(circuit, x, y, R);
    int l = TN(x - 40, y), rr = TN(x + 40, y);
    if (in >= 0 && in != l) TW(in, l);
    r->node_ids[0] = l; r->node_ids[1] = rr;
    return rr;
}
// series R-L shunt load from node `n` at (x,y) down to ground (L = 0 -> resistor only)
static Component *rl_load(Circuit *circuit, float x, float y, int n, double R, double L) {
    Component *r = add_comp(circuit, COMP_RESISTOR, x, y + 40, 90);          // (x,y)-(x,y+80)
    r->props.resistor.resistance = R;
    int t = TN(x, y), m = TN(x, y + 80);
    if (n >= 0 && n != t) TW(n, t);
    r->node_ids[0] = t;
    if (L > 0) {
        Component *l = add_comp(circuit, COMP_INDUCTOR, x, y + 120, 90);     // (x,y+80)-(x,y+160)
        l->props.inductor.inductance = L;
        int bt = TN(x, y + 160);
        Component *g = add_comp(circuit, COMP_GROUND, x, y + 180, 0);
        r->node_ids[1] = m; l->node_ids[0] = m; l->node_ids[1] = bt; g->node_ids[0] = bt;
    } else {
        Component *g = add_comp(circuit, COMP_GROUND, x, y + 100, 0);        // terminal (x,y+80)
        r->node_ids[1] = m; g->node_ids[0] = m;
    }
    return r;
}

// 1. AEP Texas 69 kV subtransmission: 20 mi of 336 ACSR into 20 MVA at 0.95 pf lag
static int place_tx_69kv(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 56340.0); if (!v) return 0;      // 69 kV L-L -> 39.84 kV L-N -> 56.34 kVpk
    int sp = TN(x, y + 20);
    v->node_ids[0] = sp;
    int bus = line_seg(circuit, x + 160, y + 20, sp, 20.0, 0.306, 0.75, 5.4, 1);   // 336.4 ACSR Linnet
    int b2 = TN(x + 300, y + 20); TW(bus, b2);
    rl_load(circuit, x + 300, y + 20, b2, 226.2, 197.1e-3);                  // 20 MVA 3ph at 0.95 pf lag
    add_label(circuit, x - 40, y - 60, "AEP Texas 69 kV subtransmission: 20 mi 336 ACSR (6.1 + j15 ohm), 20 MVA at 0.95 pf -> 0.957 pu (ERCOT limit 0.95)");
    return 8;
}

// 2. Texas voltage ladder: 345 -> 138 -> 69 -> 12.47 kV -> 240 V, a tap load at every level
static int place_tx_ladder(Circuit *circuit, float x, float y) {
    // per row: [0] line mi, r, x, b; [1] tap R; [2] transformer ratio
    Component *v = ac_source(circuit, x, y, 281700.0); if (!v) return 0;     // 345 kV L-L -> 199.2 kV L-N
    int in = TN(x, y + 20); v->node_ids[0] = in;
    static const double mi[4] = { 30, 20, 10, 3 };
    static const double rr[4] = { 0.06, 0.13, 0.306, 0.30 };
    static const double xx[4] = { 0.55, 0.72, 0.75, 0.65 };
    static const double bb[4] = { 8.0, 6.0, 5.4, 0.0 };
    static const double tap[4] = { 396.8, 190.4, 317.4, 51.84 };             // 300 / 100 / 15 / 3 MW three-phase
    static const double ratio[4] = { 0.4, 0.5, 0.189735, 0.0333333 };        // 345/138, 138/69, 69/12.47 with LTC +5 %, 7.2 kV/240 V
    static const char *lbl[4] = { "345 kV bus (0.99 pu)", "138 kV bus (0.97 pu)", "69 kV bus (0.96 pu)", "12.47 kV bus (0.99 pu, LTC +5 %)" };
    for (int k = 0; k < 4; k++) {
        float ry = y + 20 + k * 260;
        int bus = line_seg(circuit, x + 160, ry, in, mi[k], rr[k], xx[k], bb[k], 1);
        int b2 = TN(x + 300, ry); TW(bus, b2);
        rl_load(circuit, x + 300, ry, b2, tap[k], 0);
        add_label(circuit, x + 120, ry - 60, lbl[k]);   /* the line label now sits above its symbol */
        int sec = xfmr_row(circuit, x + 520, ry, ratio[k], b2);              // P1(x+470,ry) S1(x+570,ry)
        if (k < 3) {                                                        // carry the secondary down to the next row
            int d1 = TN(x + 620, ry), d2 = TN(x + 620, ry + 220), d3 = TN(x, ry + 220), d4 = TN(x, ry + 260);
            TW(sec, d1); TW(d1, d2); TW(d2, d3); TW(d3, d4);
            in = d4;
        } else {
            int serv = res_seg(circuit, x + 660, ry, sec, 0.05);             // 100 ft 4/0 AL service drop
            int h = TN(x + 740, ry); TW(serv, h);
            rl_load(circuit, x + 740, ry, h, 5.76, 0);                       // 10 kW house at 240 V
            add_label(circuit, x + 640, ry - 30, "240 V service (117.7 V per leg)");
        }
    }
    add_label(circuit, x - 40, y - 110, "TEXAS VOLTAGE LADDER - 345 / 138 / 69 / 12.47 kV and the 240 V service, each with its own tap load");
    return 40;
}

// 3. CREZ wind collector: two 34.5 kV strings -> 6 mi collector -> 34.5/345 kV GSU -> 30 mi 345 kV to the grid
static int place_tx_wind(Circuit *circuit, float x, float y) {
    Component *g1 = ac_source(circuit, x, y, 29300.0); if (!g1) return 0;    // string A behind its pad transformer, +3.5 %
    g1->props.ac_voltage.phase = 6.0;
    Component *g2 = ac_source(circuit, x, y + 200, 29300.0);
    g2->props.ac_voltage.phase = 6.0;
    int s1 = TN(x, y + 20), s2 = TN(x, y + 220);
    g1->node_ids[0] = s1; g2->node_ids[0] = s2;
    int a1 = res_seg(circuit, x + 100, y + 20, s1, 1.0);                     // string A pad transformer + cable
    Component *la = add_comp(circuit, COMP_INDUCTOR, x + 200, y + 20, 0); la->props.inductor.inductance = 5.3e-3;
    int al = TN(x + 160, y + 20), ar = TN(x + 240, y + 20); TW(a1, al);
    la->node_ids[0] = al; la->node_ids[1] = ar;
    Component *sw = add_comp(circuit, COMP_SPST_SWITCH, x + 320, y + 220, 0);   // string B disconnect (closed)
    sw->props.switch_spst.closed = true;
    int b1 = res_seg(circuit, x + 100, y + 220, s2, 1.0);
    Component *lb = add_comp(circuit, COMP_INDUCTOR, x + 200, y + 220, 0); lb->props.inductor.inductance = 5.3e-3;
    int bl = TN(x + 160, y + 220), br = TN(x + 240, y + 220); TW(b1, bl);
    lb->node_ids[0] = bl; lb->node_ids[1] = br;
    int swl = TN(x + 280, y + 220), swr = TN(x + 360, y + 220); TW(br, swl);
    sw->node_ids[0] = swl; sw->node_ids[1] = swr;
    int coll = TN(x + 420, y + 20), cb = TN(x + 420, y + 220);              // 34.5 kV collector bus
    TW(ar, coll); TW(swr, cb); TW(cb, coll);
    int cf = line_seg(circuit, x + 520, y + 20, coll, 6.0, 0.15, 0.12, 0.0, 0);   // 6 mi 1000 kcmil collector cable
    int gsu = xfmr_row(circuit, x + 700, y + 20, 10.0, cf);                 // 34.5 -> 345 kV GSU (P1 x+650, S1 x+750)
    int poi = line_seg(circuit, x + 860, y + 20, gsu, 30.0, 0.06, 0.55, 8.0, 1);  // 30 mi 345 kV to the POI
    Component *grid = ac_source(circuit, x + 1000, y + 60, 281700.0);       // ERCOT 345 kV bus, + terminal (x+1000,y+80)
    int gp = TN(x + 1000, y + 80), pj = TN(x + 940, y + 20), pj2 = TN(x + 940, y + 80);
    TW(poi, pj); TW(pj, pj2); TW(pj2, gp);
    grid->node_ids[0] = gp;
    add_label(circuit, x - 40, y - 60, "CREZ WIND COLLECTOR: two 34.5 kV strings -> 6 mi collector -> 34.5/345 kV GSU -> 30 mi to the ERCOT 345 kV grid");
    add_label(circuit, x + 260, y + 300, "Open the string-B switch (or set the string phase to 0 deg) and the export - and the collector voltage rise - collapse");
    return 20;
}

// 4. AEP 13.8 kV industrial service: 13.8/4.16 kV plant transformer, motor bus + 480 V shop feeder
static int place_tx_plant(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 11267.0); if (!v) return 0;      // 13.8 kV L-L -> 7.97 kV L-N -> 11.27 kVpk
    int sp = TN(x, y + 20); v->node_ids[0] = sp;
    int feed = res_seg(circuit, x + 120, y + 20, sp, 1.2);                   // 2 mi 4/0 primary
    int bus = TN(x + 200, y + 20); TW(feed, bus);
    int mv = xfmr_row(circuit, x + 340, y + 20, 0.30145, bus);              // 13.8/4.16 kV (4.16/13.8)
    int m2 = TN(x + 460, y + 20); TW(mv, m2);
    rl_load(circuit, x + 460, y + 20, m2, 5.77, 3.06e-3);                    // 2500 hp motor bus, 0.88 pf
    int lv = xfmr_row(circuit, x + 640, y + 20, 0.11538, m2);               // 4.16 kV -> 480 V shop
    int l2 = res_seg(circuit, x + 760, y + 20, lv, 0.02);                   // 200 ft of 480 V feeder
    rl_load(circuit, x + 840, y + 20, l2, 2.56, 0.84e-3);                    // 300 kVA shop load at 0.95 pf
    add_label(circuit, x - 40, y - 60, "AEP 13.8 kV INDUSTRIAL SERVICE: 13.8/4.16 kV plant transformer, 2500 hp motor bus, 4160/480 V shop feeder");
    return 16;
}

// 5. 240/120 V residential service: centre-tapped pole transformer, unbalanced legs, neutral conductor
static int place_res_service(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 10182.0); if (!v) return 0;      // 12.47Y/7.2 kV primary, 7.2 kV L-N
    int pri = TN(x, y + 20); v->node_ids[0] = pri;
    Component *ta = add_comp(circuit, COMP_TRANSFORMER, x + 200, y + 40, 0);   // P1(150,20) P2(150,60) S1(250,20) S2(250,60)
    Component *tb = add_comp(circuit, COMP_TRANSFORMER, x + 200, y + 200, 0);  // P1(150,180) P2(150,220) S1(250,180) S2(250,220)
    ta->props.transformer.turns_ratio = 0.0166667; tb->props.transformer.turns_ratio = 0.0166667;
    int p1a = TN(x + 150, y + 20), p2a = TN(x + 150, y + 60), p1b = TN(x + 150, y + 180), p2b = TN(x + 150, y + 220);
    TW(pri, p1a); TW(p1a, p1b);
    Component *gp = add_comp(circuit, COMP_GROUND, x + 110, y + 120, 0);     // primary neutral, terminal (110,100)
    int pn = TN(x + 110, y + 100); TW(p2a, TN(x + 110, y + 60)); TW(TN(x + 110, y + 60), pn); TW(pn, TN(x + 110, y + 220)); TW(TN(x + 110, y + 220), p2b);
    gp->node_ids[0] = pn;
    int s1a = TN(x + 250, y + 20), s2a = TN(x + 250, y + 60), s1b = TN(x + 250, y + 180), s2b = TN(x + 250, y + 220);
    ta->node_ids[0] = p1a; ta->node_ids[1] = p2a; ta->node_ids[2] = s1a; ta->node_ids[3] = s2a;
    tb->node_ids[0] = p1b; tb->node_ids[1] = p2b; tb->node_ids[2] = s1b; tb->node_ids[3] = s2b;
    int ct = TN(x + 300, y + 120);                                           // centre tap = system neutral
    TW(s2a, TN(x + 300, y + 60)); TW(TN(x + 300, y + 60), ct); TW(ct, TN(x + 300, y + 180)); TW(TN(x + 300, y + 180), s1b);
    int l1 = res_seg(circuit, x + 400, y + 20, s1a, 0.02);                   // 100 ft 4/0 AL triplex, L1
    int l2 = res_seg(circuit, x + 400, y + 220, s2b, 0.02);                  // L2
    int nn = res_seg(circuit, x + 400, y + 120, ct, 0.02);                   // neutral conductor
    Component *gn = add_comp(circuit, COMP_GROUND, x + 440, y + 160, 0);     // grounded neutral at the panel, terminal (440,140)
    int np = TN(x + 440, y + 120), ng = TN(x + 440, y + 140); TW(nn, np); TW(np, ng);
    gn->node_ids[0] = ng;
    Component *r1 = add_comp(circuit, COMP_RESISTOR, x + 520, y + 60, 90);   // L1-N 1.2 kW: (520,20)-(520,100)
    r1->props.resistor.resistance = 12.0;
    int r1t = TN(x + 520, y + 20), r1b = TN(x + 520, y + 100);
    TW(l1, r1t);
    r1->node_ids[0] = r1t; r1->node_ids[1] = r1b;
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 520, y + 160, 90);  // L2-N 600 W: (520,120)-(520,200)
    r2->props.resistor.resistance = 24.0;
    int r2t = TN(x + 520, y + 120), r2b = TN(x + 520, y + 200);
    TW(r1b, r2t); TW(np, r2t);                                               // both returns land on the neutral
    int l2j = TN(x + 520, y + 220); TW(r2b, l2j); TW(l2j, l2);
    r2->node_ids[0] = r2t; r2->node_ids[1] = r2b;
    Component *r3 = add_comp(circuit, COMP_RESISTOR, x + 620, y + 120, 90);  // 240 V range 4.8 kW: (620,80)-(620,160)
    r3->props.resistor.resistance = 12.0;
    int r3t = TN(x + 620, y + 80), r3b = TN(x + 620, y + 160), r3j = TN(x + 620, y + 20), r3k = TN(x + 620, y + 220);
    TW(r1t, r3j); TW(r3j, r3t);
    TW(r3b, r3k); TW(r3k, l2j);
    r3->node_ids[0] = r3t; r3->node_ids[1] = r3b;
    add_label(circuit, x - 40, y - 60, "240/120 V RESIDENTIAL SERVICE: centre-tapped pole transformer, 1.2 kW on L1, 600 W on L2, 4.8 kW range across 240 V");
    add_label(circuit, x + 360, y + 300, "Raise the neutral conductor to 5 ohm (a corroded connection): L1 collapses and L2 rises past 126 V - the open-neutral failure");
    return 18;
}

// 6. 120 V branch circuits: 100 ft of #14 and 100 ft of #10 feeding the same 12 A load (NEC 210.19(A))
static int place_res_branch(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 169.71); if (!v) return 0;       // 120 V rms
    int sp = TN(x, y + 20); v->node_ids[0] = sp;
    int p14 = res_seg(circuit, x + 140, y + 20, sp, 0.505);                  // #14 Cu, 100 ft, both conductors
    int n14 = TN(x + 240, y + 20); TW(p14, n14);
    rl_load(circuit, x + 240, y + 20, n14, 10.0, 0);                         // 12 A load
    int t1 = TN(x + 100, y + 20), b2 = TN(x + 100, y + 220); TW(t1, b2);    // tap the panel, not the source's return
    int p10 = res_seg(circuit, x + 140, y + 220, b2, 0.200);                 // #10 Cu, 100 ft
    int n10 = TN(x + 240, y + 220); TW(p10, n10);
    rl_load(circuit, x + 240, y + 220, n10, 10.0, 0);
    add_label(circuit, x - 40, y - 60, "120 V BRANCH CIRCUITS (NEC 210.19(A)): the same 12 A load 100 ft away on #14 (0.505 ohm) and on #10 (0.20 ohm)");
    add_label(circuit, x + 100, y + 340, "#14: 114.2 V at the load = 4.8 % drop, over the 3 % guideline. #10: 117.7 V = 2.0 %, compliant");
    return 10;
}

// 7. AC compressor start: 5-ton unit, LRA 104 A, on a long rural 240 V service (IEEE 1453 flicker)
static int place_res_acstart(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 339.41); if (!v) return 0;       // 240 V rms
    int sp = TN(x, y + 20); v->node_ids[0] = sp;
    int sr = res_seg(circuit, x + 120, y + 20, sp, 0.20);                    // transformer + 200 ft service
    Component *ls = add_comp(circuit, COMP_INDUCTOR, x + 220, y + 20, 0); ls->props.inductor.inductance = 0.318e-3;
    int ll = TN(x + 180, y + 20), lr = TN(x + 260, y + 20); TW(sr, ll);
    ls->node_ids[0] = ll; ls->node_ids[1] = lr;
    int bus = TN(x + 320, y + 20); TW(lr, bus);                              // house panel
    rl_load(circuit, x + 320, y + 20, bus, 28.8, 0);                         // 2 kW of lighting / receptacles
    Component *sw = fault_switch(circuit, x + 460, y + 20, 0.050, 0.120, 0.400);   // IN(420,20) OUT(500,20): compressor contactor
    TW(bus, TN(x + 420, y + 20)); sw->node_ids[0] = TN(x + 420, y + 20);
    int mtr = TN(x + 500, y + 20); sw->node_ids[1] = mtr;
    int m2 = TN(x + 560, y + 20); TW(mtr, m2);
    rl_load(circuit, x + 560, y + 20, m2, 1.15, 5.30e-3);                    // locked rotor: 104 A at 0.5 pf
    add_label(circuit, x - 40, y - 60, "AC COMPRESSOR START: 5-ton unit (LRA 104 A) on a long rural 240 V service (0.20 + j0.12 ohm)");
    add_label(circuit, x + 120, y + 300, "The contactor closes at 50 ms: the panel sags 7 % (333 -> 310 Vpk), past the ~3 % flicker limit (IEEE 1453 / AEP planning).");
    add_label(circuit, x + 120, y + 330, "Fix it: service R 0.20 -> 0.05 ohm (bigger transformer and conductor) or a soft starter (raise the motor R).");
    return 14;
}

// 8. Rooftop solar backfeed: 7.6 kW inverter exporting into the service impedance (IEEE 1547 / ANSI C84.1)
static int place_res_solar(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 339.41); if (!v) return 0;       // utility 240 V
    int sp = TN(x, y + 20); v->node_ids[0] = sp;
    int sr = res_seg(circuit, x + 140, y + 20, sp, 0.25);                    // 25 kVA transformer + 150 ft drop
    int pcc = TN(x + 240, y + 20); TW(sr, pcc);                              // point of common coupling
    rl_load(circuit, x + 240, y + 20, pcc, 57.6, 0);                         // 1 kW house load
    Component *inv = add_comp(circuit, COMP_AC_CURRENT, x + 400, y + 100, 180);   // rotated: -(400,60) +(400,140) injects up
    inv->props.ac_current.amplitude = 44.8; inv->props.ac_current.frequency = 60.0;   // 7.6 kW at unity pf
    Component *gi = add_comp(circuit, COMP_GROUND, x + 400, y + 160, 0);
    int it = TN(x + 400, y + 60), ib = TN(x + 400, y + 140), ig = TN(x + 400, y + 160);
    TW(pcc, TN(x + 400, y + 20)); TW(TN(x + 400, y + 20), it); TW(ib, ig);
    inv->node_ids[0] = ib; inv->node_ids[1] = it; gi->node_ids[0] = ig;
    add_label(circuit, x - 40, y - 60, "ROOFTOP SOLAR BACKFEED (IEEE 1547 / ERCOT DG): a 7.6 kW inverter exports 31.7 A through a 0.25 ohm service");
    add_label(circuit, x + 60, y + 260, "Export raises the PCC by I x Z = 7.9 V (124 V per leg): inside ANSI C84.1 Range A, but only just.");
    add_label(circuit, x + 60, y + 290, "Double the inverter current (or the service resistance) and the PCC passes 126 V - where 1547 volt-var / volt-watt takes over.");
    return 11;
}

// 9. 480Y/277 V commercial service: 3-phase motor plus 277 V lighting
static int place_com_480y(Circuit *circuit, float x, float y) {
    static const float ry[3] = { 40, 260, 480 };   // R-L loads are 160 px tall: keep the rows clear
    int out[3];
    Component *g = three_phase_fanout(circuit, x, y, 391.9, 0.0, ry, out);   // 277 V L-N
    if (!g) return 0;
    static const char *ph[3] = { "A", "B", "C" };
    for (int k = 0; k < 3; k++) {
        float py = y + ry[k];
        int f = res_seg(circuit, x + 240, py, out[k], 0.03);                 // 200 ft feeder per phase
        int bus = TN(x + 320, py); TW(f, bus);
        rl_load(circuit, x + 320, py, bus, 3.62, 6.42e-3);                   // 30 hp motor, 0.85 pf, per phase
        add_label(circuit, x + 270, py - 30, ph[k]);
        if (k == 0) {                                                        // 277 V lighting on phase A only
            int lt = TN(x + 480, py); TW(bus, TN(x + 440, py)); TW(TN(x + 440, py), lt);
            rl_load(circuit, x + 480, py, lt, 12.8, 0);                      // 6 kW of 277 V lighting
        }
    }
    add_label(circuit, x - 40, y - 60, "480Y/277 V COMMERCIAL SERVICE: 30 hp motor across the three phases plus 6 kW of 277 V lighting on A (NEC 210.6(C))");
    add_label(circuit, x + 200, y + 680, "ANSI C84.1 Range A for a 480 V service is 456-504 V line-to-line (263-291 V line-to-neutral).");
    return 24;
}

// 10. 208Y/120 V panel: unbalanced loads and the neutral conductor (NEC 220.61)
static int place_com_208y(Circuit *circuit, float x, float y) {
    static const float ry[3] = { 40, 180, 320 };
    int out[3];
    Component *g = three_phase_fanout(circuit, x, y, 169.71, 0.0, ry, out);  // 120 V L-N
    if (!g) return 0;
    static const double load[3] = { 6.0, 10.0, 20.0 };                       // 20 A, 12 A, 6 A
    static const char *ph[3] = { "A 20 A", "B 12 A", "C 6 A" };
    int neutral = TN(x + 520, y + 180);
    for (int k = 0; k < 3; k++) {
        float py = y + ry[k];
        int f = res_seg(circuit, x + 240, py, out[k], 0.05);                 // branch conductors
        int bus = TN(x + 320, py); TW(f, bus);
        Component *r = add_comp(circuit, COMP_RESISTOR, x + 400, py, 0);     // (360,py)-(440,py)
        r->props.resistor.resistance = load[k];
        int rl_ = TN(x + 360, py), rr_ = TN(x + 440, py); TW(bus, rl_);
        r->node_ids[0] = rl_; r->node_ids[1] = rr_;
        int j = TN(x + 480, py), j2 = TN(x + 520, py); TW(rr_, j); TW(j, j2); if (j2 != neutral) TW(j2, neutral);
        add_label(circuit, x + 340, py - 30, ph[k]);
    }
    Component *rn = add_comp(circuit, COMP_RESISTOR, x + 600, y + 180, 0);   // shared neutral back to the transformer
    rn->props.resistor.resistance = 0.05;
    int nl = TN(x + 560, y + 180), nr = TN(x + 640, y + 180); TW(neutral, nl);
    rn->node_ids[0] = nl; rn->node_ids[1] = nr;
    Component *gn = add_comp(circuit, COMP_GROUND, x + 680, y + 200, 0);
    int ng = TN(x + 680, y + 180), ng2 = TN(x + 680, y + 200); TW(nr, ng); TW(ng, ng2);
    gn->node_ids[0] = ng2;
    add_label(circuit, x - 40, y - 60, "208Y/120 V PANEL: 20 / 12 / 6 A on A / B / C. The neutral carries the unbalance (~12.2 A), not the sum (NEC 220.61)");
    add_label(circuit, x + 200, y + 480, "Raise the neutral conductor and the panel neutral shifts: the lightly loaded phase rises, the heavy one sags.");
    return 22;
}

// 11. Power factor correction: 277 V commercial motor with a switchable capacitor bank
static int place_com_pfc(Circuit *circuit, float x, float y) {
    Component *v = add_comp(circuit, COMP_AC_VOLTAGE, x, y + 60, 0);        // +(x,y+20) -(x,y+100)
    if (!v) return 0;
    v->props.ac_voltage.amplitude = 391.9; v->props.ac_voltage.frequency = 60.0;   // 277 V L-N
    Component *sense = add_comp(circuit, COMP_RESISTOR, x, y + 160, 90);     // supply-return shunt: (x,y+120)-(x,y+200)
    sense->props.resistor.resistance = 0.05;
    Component *gs = add_comp(circuit, COMP_GROUND, x, y + 220, 0);
    int sp = TN(x, y + 20), vm = TN(x, y + 100), st = TN(x, y + 120), sb = TN(x, y + 200);
    TW(vm, st);
    v->node_ids[0] = sp; v->node_ids[1] = vm;
    sense->node_ids[0] = st; sense->node_ids[1] = sb; gs->node_ids[0] = sb;
    int bus = TN(x + 240, y + 20); TW(sp, bus);
    rl_load(circuit, x + 240, y + 20, bus, 1.727, 4.04e-3);                  // 33 kVA at 0.75 pf lag
    Component *sw = add_comp(circuit, COMP_SPST_SWITCH, x + 420, y + 20, 0); // capacitor bank switch (open)
    sw->props.switch_spst.closed = false;
    int swl = TN(x + 380, y + 20), swr = TN(x + 460, y + 20); TW(bus, swl);
    sw->node_ids[0] = swl; sw->node_ids[1] = swr;
    Component *c = add_comp(circuit, COMP_CAPACITOR, x + 520, y + 60, 90);   // (520,20)-(520,100)
    c->props.capacitor.capacitance = 478e-6;
    int ct = TN(x + 520, y + 20), cb = TN(x + 520, y + 100); TW(swr, ct);
    Component *gc = add_comp(circuit, COMP_GROUND, x + 520, y + 120, 0);
    c->node_ids[0] = ct; c->node_ids[1] = cb; gc->node_ids[0] = cb;
    add_label(circuit, x - 40, y - 60, "POWER FACTOR CORRECTION: 33 kVA motor at 0.75 pf on 277 V; close the switch for 478 uF (13.8 kvar) -> 0.95 pf");
    add_label(circuit, x + 60, y + 300, "CH1 is the 0.05 ohm supply shunt (50 mV per amp): the line current falls from 120 A to 95 A for the same kW.");
    add_label(circuit, x + 60, y + 330, "ERCOT / AEP tariffs and NERC VAR-001 both push the delivery point to 0.95; the kvar comes from the bank, not the line.");
    return 12;
}

// 12. Standby generator transfer switch (NEC 700 / NFPA 110): utility drops, the generator picks the load up
static int place_com_ats(Circuit *circuit, float x, float y) {
    Component *u = ac_source(circuit, x, y, 339.41); if (!u) return 0;       // utility 240 V
    int up = TN(x, y + 20); u->node_ids[0] = up;
    Component *swu = fault_switch(circuit, x + 200, y + 20, 0.0, 0.050, 1.0);      // utility contactor: closed 0-50 ms
    TW(up, TN(x + 160, y + 20)); swu->node_ids[0] = TN(x + 160, y + 20);
    Component *gsrc = ac_source(circuit, x, y + 240, 339.41);               // standby generator
    int gp = TN(x, y + 260); gsrc->node_ids[0] = gp;
    Component *swg = fault_switch(circuit, x + 200, y + 260, 0.070, 0.300, 1.0);   // generator contactor: closed from 70 ms
    TW(gp, TN(x + 160, y + 260)); swg->node_ids[0] = TN(x + 160, y + 260);
    int bus = TN(x + 340, y + 20), gb = TN(x + 340, y + 260);
    swu->node_ids[1] = TN(x + 240, y + 20); TW(TN(x + 240, y + 20), bus);
    swg->node_ids[1] = TN(x + 240, y + 260); TW(TN(x + 240, y + 260), gb); TW(gb, bus);
    int f = res_seg(circuit, x + 440, y + 20, bus, 0.05);
    int lb = TN(x + 520, y + 20); TW(f, lb);
    rl_load(circuit, x + 520, y + 20, lb, 11.52, 0);                         // 5 kW life-safety load
    add_label(circuit, x - 40, y - 60, "STANDBY GENERATOR TRANSFER (NEC 700 / NFPA 110): the utility contactor opens at 50 ms, the generator closes at 70 ms");
    add_label(circuit, x + 100, y + 420, "The load is dead for 20 ms (open transition). NEC 700 allows 10 s for emergency systems, 60 s for legally required standby.");
    return 16;
}
#undef TN
#undef TW

// ---------------------------------------------------------------------------------------
// Reliability standards and simulation methods, built from four utility technical reports
// (see docs/RESEARCH_GRID_STANDARDS.md):
//   NERC TPL-001-5.1 contingency voltage envelopes, TPL-008-1 extreme temperature,
//   PRC-029-1 / ERCOT NOGRR-245 / IEEE 2800 inverter ride-through, FAC-008-5 facility
//   ratings, AEP BOLD line geometry, BAL-001-TRE-2 governor droop and the swing equation,
//   Kron reduction and the R/X limits of fast decoupled power flow, and the CIP-014-2
//   supervised perimeter alarm loop.
// ---------------------------------------------------------------------------------------
#define TN(cx, cy) circuit_find_or_create_node(circuit, (cx), (cy), 5.0f)
#define TW(a_, b_) circuit_add_wire(circuit, (a_), (b_))

// 1. NERC TPL-001-5.1: two parallel 345 kV lines; open one and the bus leaves the P0 envelope
static int place_gs_n1(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 281700.0); if (!v) return 0;
    int sp = TN(x, y + 20); v->node_ids[0] = sp;
    int tap = TN(x + 60, y + 20); TW(sp, tap);                                      // shared tap for both circuits
    int a = line_seg(circuit, x + 160, y + 20, tap, 200.0, 0.06, 0.55, 8.0, 1);     // circuit 1
    Component *sw = add_comp(circuit, COMP_SPST_SWITCH, x + 300, y + 180, 0);       // circuit 2 breaker (closed)
    sw->props.switch_spst.closed = true;
    int b1 = TN(x + 60, y + 180); TW(tap, b1);
    int b = line_seg(circuit, x + 160, y + 180, b1, 200.0, 0.06, 0.55, 8.0, 1);      // circuit 2
    int swl = TN(x + 260, y + 180), swr = TN(x + 340, y + 180); TW(b, swl);
    sw->node_ids[0] = swl; sw->node_ids[1] = swr;
    int bus = TN(x + 400, y + 20), bj = TN(x + 400, y + 180); TW(a, bus); TW(swr, bj); TW(bj, bus);
    rl_load(circuit, x + 400, y + 20, bus, 350.0, 0);                                // 340 MW three-phase
    add_label(circuit, x - 40, y - 60, "NERC TPL-001-5.1: two 200 mi 345 kV circuits into one load bus. Open the breaker to run the P1 (N-1) case.");
    add_label(circuit, x + 60, y + 320, "Both in service (P0): 0.972 pu - inside 0.95-1.05. One out (P1): 0.925 pu - below the P0 floor but");
    add_label(circuit, x + 60, y + 350, "inside the 0.92-1.05 post-contingency envelope. The 4.8 % deviation is under the 8 % review threshold.");
    return 12;
}

// 2. PRC-029-1 / ERCOT NOGRR-245: a fault drags the POI down; the inverter must keep injecting
static int place_gs_ibr(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 281700.0); if (!v) return 0;             // 345 kV grid
    int sp = TN(x, y + 20); v->node_ids[0] = sp;
    int poi = res_seg(circuit, x + 120, y + 20, sp, 20.0);                           // grid source impedance
    int bus = TN(x + 220, y + 20); TW(poi, bus);
    rl_load(circuit, x + 220, y + 20, bus, 400.0, 0);                                // local load
    Component *fs = fault_switch(circuit, x + 380, y + 20, 0.100, 0.150, 1.0);       // 150 ms fault at 100 ms
    TW(bus, TN(x + 340, y + 20)); fs->node_ids[0] = TN(x + 340, y + 20);
    int fo = TN(x + 420, y + 20); fs->node_ids[1] = fo;
    Component *rf = add_comp(circuit, COMP_RESISTOR, x + 460, y + 60, 90);           // fault impedance
    rf->props.resistor.resistance = 8.0;
    int rft = TN(x + 460, y + 20), rfb = TN(x + 460, y + 100); TW(fo, rft);
    Component *gf = add_comp(circuit, COMP_GROUND, x + 460, y + 120, 0);
    rf->node_ids[0] = rft; rf->node_ids[1] = rfb; gf->node_ids[0] = rfb;
    Component *trip = add_comp(circuit, COMP_SPST_SWITCH, x + 620, y + 20, 0);       // inverter breaker (closed)
    trip->props.switch_spst.closed = true;
    int tl = TN(x + 580, y + 20), tr = TN(x + 660, y + 20);
    int tap0 = TN(x + 280, y + 20), tap1 = TN(x + 280, y + 220), tap2 = TN(x + 560, y + 220), tap3 = TN(x + 560, y + 20);
    TW(bus, tap0); TW(tap0, tap1); TW(tap1, tap2); TW(tap2, tap3); TW(tap3, tl);   // route clear of the fault switch
    trip->node_ids[0] = tl; trip->node_ids[1] = tr;
    Component *inv = add_comp(circuit, COMP_AC_CURRENT, x + 720, y + 100, 180);      // -(720,60) +(720,140)
    inv->props.ac_current.amplitude = 400.0; inv->props.ac_current.frequency = 60.0;
    Component *gi = add_comp(circuit, COMP_GROUND, x + 720, y + 160, 0);
    int it = TN(x + 720, y + 60), ib = TN(x + 720, y + 140), ig = TN(x + 720, y + 160);
    TW(tr, TN(x + 720, y + 20)); TW(TN(x + 720, y + 20), it); TW(ib, ig);
    inv->node_ids[0] = ib; inv->node_ids[1] = it; gi->node_ids[0] = ig;
    add_label(circuit, x - 40, y - 60, "IBR RIDE-THROUGH (NERC PRC-029-1 / ERCOT NOGRR-245 / IEEE 2800): a 150 ms fault at 100 ms drags the POI to ~0.3 pu");
    add_label(circuit, x + 60, y + 300, "0.90-1.10 pu is the continuous envelope; below 0.90 the inverter must ride through and inject current,");
    add_label(circuit, x + 60, y + 330, "and must be back to its pre-disturbance real power within 1.0 s. Open the inverter breaker for the non-compliant case.");
    return 16;
}

// 3. AEP BOLD: compacted phase spacing lowers Zc = sqrt(L/C) and raises SIL by ~60 %
static int place_gs_bold(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 281700.0); if (!v) return 0;
    int sp = TN(x, y + 20); v->node_ids[0] = sp;
    int tap = TN(x + 60, y + 20); TW(sp, tap);
    int a = line_seg(circuit, x + 200, y + 20, tap, 150.0, 0.06, 0.55, 8.0, 2);      // conventional: Zc 262 ohm
    int ba = TN(x + 320, y + 20); TW(a, ba);
    rl_load(circuit, x + 320, y + 20, ba, 198.4, 0);                                 // 600 MW
    int d1 = TN(x + 60, y + 260); TW(tap, d1);
    int b = line_seg(circuit, x + 200, y + 260, d1, 150.0, 0.036, 0.38, 14.5, 2);    // BOLD: Zc 162 ohm
    int bb = TN(x + 320, y + 260); TW(b, bb);
    rl_load(circuit, x + 320, y + 260, bb, 198.4, 0);
    add_label(circuit, x - 40, y - 120, "AEP BOLD (Breakthrough Overhead Line Design): the same 150 mi 345 kV corridor at 600 MW, twice");
    add_label(circuit, x + 120, y - 70, "conventional: 0.06 + j0.55 ohm/mi, 8 uS/mi  ->  Zc = sqrt(L/C) = 262 ohm, SIL = 345^2/Zc = 454 MW");
    add_label(circuit, x + 120, y + 170, "BOLD: compact triangular phasing raises C and lowers L  ->  Zc = 162 ohm, SIL = 735 MW (+62 %), losses -40 %");
    add_label(circuit, x + 60, y + 420, "Because BOLD carries the transfer naturally it needs no series capacitors - and so has no sub-synchronous resonance risk.");
    return 14;
}

// 4. NERC TPL-008-1 / PUCT 25.55: conductor resistance rises with temperature (drag the Tmp slider)
static int place_gs_derate(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 10182.0); if (!v) return 0;              // 12.47 kV feeder
    int sp = TN(x, y + 20); v->node_ids[0] = sp;
    Component *rc = hres(circuit, x + 140, y + 20, 6.0);                             // 20 mi of 1/0 ACSR
    rc->props.resistor.ideal = false;                                                // enable the temperature model
    rc->props.resistor.temp_coeff = 4030.0;                                          // aluminium: 0.00403 /degC
    int cl = TN(x + 100, y + 20), cr = TN(x + 180, y + 20); TW(sp, cl);
    rc->node_ids[0] = cl; rc->node_ids[1] = cr;
    Component *lx = add_comp(circuit, COMP_INDUCTOR, x + 260, y + 20, 0); lx->props.inductor.inductance = 34.5e-3;
    int ll = TN(x + 220, y + 20), lr = TN(x + 300, y + 20); TW(cr, ll);
    lx->node_ids[0] = ll; lx->node_ids[1] = lr;
    int bus = TN(x + 360, y + 20); TW(lr, bus);
    rl_load(circuit, x + 360, y + 20, bus, 150.0, 0);                                // 1 MW base feeder load
    Component *sw = add_comp(circuit, COMP_SPST_SWITCH, x + 500, y + 20, 0);         // summer peak block (open)
    sw->props.switch_spst.closed = false;
    int swl = TN(x + 460, y + 20), swr = TN(x + 540, y + 20); TW(bus, swl);
    sw->node_ids[0] = swl; sw->node_ids[1] = swr;
    rl_load(circuit, x + 600, y + 20, swr, 150.0, 0);                                // the summer air-conditioning block
    add_label(circuit, x - 40, y - 60, "EXTREME TEMPERATURE (NERC TPL-008-1 / PUCT 25.55): the conductor carries a real 4030 ppm/degC aluminium coefficient");
    add_label(circuit, x + 60, y + 240, "Drag the Tmp slider in the status bar: at 25 degC the conductor is 6.0 ohm, at 75 degC it is 7.2 ohm (+20 %).");
    add_label(circuit, x + 60, y + 270, "Close the switch for the summer-peak air-conditioning block - hotter conductor and heavier load arrive together.");
    return 14;
}

// 5. NERC FAC-008-5: the rating of a path is the rating of its most limiting element
static int place_gs_facrate(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 112670.0); if (!v) return 0;             // 138 kV
    int sp = TN(x, y + 20); v->node_ids[0] = sp;
    static const double R[4]   = { 2.0, 0.05, 0.02, 0.03 };
    static const double rate[4] = { 1600e3, 40e3, 8e3, 50e3 };   // peak limits: the simulator compares instantaneous P, which is 2x the average for a sine
    static const char *nm[4] = { "line conductor 800 kW", "breaker 20 kW", "CT 4 kW", "buswork 25 kW" };   // average ratings, shown to the reader
    int n = sp;
    for (int k = 0; k < 4; k++) {
        Component *r = hres(circuit, x + 140 + k * 160, y + 20, R[k]);
        r->props.resistor.power_rating = rate[k];                                    // explicit: the post-pass leaves these alone
        int l = TN(x + 100 + k * 160, y + 20), rr = TN(x + 180 + k * 160, y + 20);
        if (n != l) TW(n, l);
        r->node_ids[0] = l; r->node_ids[1] = rr;
        add_label(circuit, x + 100 + k * 160, y - 20 - (k % 2) * 26, nm[k]);
        n = rr;
    }
    int bus = TN(x + 800, y + 20); TW(n, bus);
    rl_load(circuit, x + 800, y + 20, bus, 199.0, 0);                                // 400 A
    Component *sw = add_comp(circuit, COMP_SPST_SWITCH, x + 900, y + 20, 0);         // extra block (open)
    sw->props.switch_spst.closed = false;
    int swl = TN(x + 860, y + 20), swr = TN(x + 940, y + 20); TW(bus, swl);
    sw->node_ids[0] = swl; sw->node_ids[1] = swr;
    rl_load(circuit, x + 1000, y + 20, swr, 799.0, 0);                               // closing the switch takes the path to 500 A
    add_label(circuit, x - 40, y - 80, "FACILITY RATING (NERC FAC-008-5): four elements in one path, each with its own rating");
    add_label(circuit, x + 60, y + 240, "At 400 A everything is inside its rating. Close the switch (500 A) and only the CT crosses 100 % - it is the");
    add_label(circuit, x + 60, y + 270, "most limiting element, so it sets the rating of the whole path no matter how strong the conductor is.");
    return 16;
}

// 6. Kron reduction: eliminating an interior node is exactly the Y -> delta transform
static int place_gs_kron(Circuit *circuit, float x, float y) {
    // -- Y network: source -> Ra -> interior -> Rb -> load 1, interior -> Rc -> load 2
    Component *v1 = ac_source(circuit, x, y, 169.71); if (!v1) return 0;
    int s1 = TN(x, y + 20); v1->node_ids[0] = s1;
    int ia = res_seg(circuit, x + 140, y + 20, s1, 10.0);                            // Ra
    int mid = TN(x + 220, y + 20); TW(ia, mid);                                      // the interior (zero-injection) node
    int b1 = res_seg(circuit, x + 320, y + 20, mid, 10.0);                           // Rb
    int l1 = TN(x + 400, y + 20); TW(b1, l1);
    rl_load(circuit, x + 400, y + 20, l1, 40.0, 0);
    int mj = TN(x + 220, y + 140); TW(mid, mj);
    int c1 = res_seg(circuit, x + 320, y + 140, mj, 10.0);                           // Rc
    int l2 = TN(x + 400, y + 140); TW(c1, l2);
    rl_load(circuit, x + 400, y + 140, l2, 25.0, 0);
    add_label(circuit, x + 60, y - 20, "Y network: the interior node is a zero-injection bus");
    // -- the Kron-reduced (delta) equivalent, driven identically
    Component *v2 = ac_source(circuit, x, y + 320, 169.71);
    int s2 = TN(x, y + 340); v2->node_ids[0] = s2;
    int s2b = TN(x + 60, y + 340), s2c = TN(x + 60, y + 460); TW(s2, s2b); TW(s2b, s2c);
    int d1 = res_seg(circuit, x + 200, y + 340, s2b, 30.0);                          // R12 = Ra + Rb + Ra Rb / Rc
    int m1 = TN(x + 280, y + 340); TW(d1, m1);
    rl_load(circuit, x + 280, y + 340, m1, 40.0, 0);
    int d2 = res_seg(circuit, x + 200, y + 460, s2c, 30.0);                          // R13
    int m2 = TN(x + 280, y + 460); TW(d2, m2);
    rl_load(circuit, x + 280, y + 460, m2, 25.0, 0);
    Component *r23 = add_comp(circuit, COMP_RESISTOR, x + 400, y + 400, 90);         // R23 between the two boundary buses
    r23->props.resistor.resistance = 30.0;
    int rt = TN(x + 400, y + 360), rb = TN(x + 400, y + 440);
    TW(m1, TN(x + 400, y + 340)); TW(TN(x + 400, y + 340), rt);
    TW(rb, TN(x + 400, y + 460)); TW(TN(x + 400, y + 460), m2);
    r23->node_ids[0] = rt; r23->node_ids[1] = rb;
    add_label(circuit, x + 60, y + 300, "Kron-reduced equivalent: the interior node is gone, the boundary sees the same network");
    add_label(circuit, x - 40, y - 60, "KRON REDUCTION: Y_red = Y_aa - Y_ab Y_bb^-1 Y_ba is the Schur complement - for one interior node it is the Y-to-delta transform");
    add_label(circuit, x + 60, y + 600, "Both halves are driven identically: the two load voltages match to the last digit, which is what 'effective resistance invariance' means.");
    return 22;
}

// 7. Fast decoupled power flow assumes R/X << 1: watch it break on a distribution feeder
static int place_gs_rx(Circuit *circuit, float x, float y) {
    Component *v = ac_source(circuit, x, y, 169.71); if (!v) return 0;
    int sp = TN(x, y + 20); v->node_ids[0] = sp;
    int rxtap = TN(x + 60, y + 20); TW(sp, rxtap);
    Component *rt = hres(circuit, x + 140, y + 20, 1.0);                             // transmission-like: R/X = 0.09
    int tl = TN(x + 100, y + 20), tr = TN(x + 180, y + 20); TW(rxtap, tl);
    rt->node_ids[0] = tl; rt->node_ids[1] = tr;
    Component *lt = add_comp(circuit, COMP_INDUCTOR, x + 260, y + 20, 0); lt->props.inductor.inductance = 29.0e-3;
    int ll = TN(x + 220, y + 20), lr = TN(x + 300, y + 20); TW(tr, ll);
    lt->node_ids[0] = ll; lt->node_ids[1] = lr;
    int bt = TN(x + 360, y + 20); TW(lr, bt);
    rl_load(circuit, x + 360, y + 20, bt, 200.0, 0);
    Component *sw1 = add_comp(circuit, COMP_SPST_SWITCH, x + 480, y + 20, 0);        // reactive block (open)
    sw1->props.switch_spst.closed = false;
    int s1l = TN(x + 440, y + 20), s1r = TN(x + 520, y + 20); TW(bt, s1l);
    sw1->node_ids[0] = s1l; sw1->node_ids[1] = s1r;
    rl_load(circuit, x + 580, y + 20, s1r, 0.1, 32.0e-3);                            // mostly reactive
    int d1 = TN(x + 60, y + 300); TW(rxtap, d1);
    Component *rd = hres(circuit, x + 140, y + 300, 11.0);                           // distribution-like: R/X = 1.5
    int dl = TN(x + 100, y + 300), dr = TN(x + 180, y + 300); TW(d1, dl);
    rd->node_ids[0] = dl; rd->node_ids[1] = dr;
    Component *ld = add_comp(circuit, COMP_INDUCTOR, x + 260, y + 300, 0); ld->props.inductor.inductance = 19.4e-3;
    int l2l = TN(x + 220, y + 300), l2r = TN(x + 300, y + 300); TW(dr, l2l);
    ld->node_ids[0] = l2l; ld->node_ids[1] = l2r;
    int bd = TN(x + 360, y + 300); TW(l2r, bd);
    rl_load(circuit, x + 360, y + 300, bd, 200.0, 0);
    Component *sw2 = add_comp(circuit, COMP_SPST_SWITCH, x + 480, y + 300, 0);
    sw2->props.switch_spst.closed = false;
    int s2l = TN(x + 440, y + 300), s2r = TN(x + 520, y + 300); TW(bd, s2l);
    sw2->node_ids[0] = s2l; sw2->node_ids[1] = s2r;
    rl_load(circuit, x + 580, y + 300, s2r, 0.1, 32.0e-3);
    add_label(circuit, x - 40, y - 60, "R/X AND FAST DECOUPLED POWER FLOW: the top branch is transmission (1 + j11 ohm, R/X = 0.09), the bottom");
    add_label(circuit, x + 60, y - 20, "is a distribution feeder (11 + j7.3 ohm, R/X = 1.5). Both carry the same 12 ohm load.");
    add_label(circuit, x + 60, y + 560, "FDPF assumes R/X ~ 0 so that P moves angle and Q moves magnitude. Close each reactive block in turn:");
    add_label(circuit, x + 60, y + 590, "on the transmission branch the extra vars move the magnitude much more than the watts do; on the feeder the");
    add_label(circuit, x + 60, y + 620, "two effects are comparable, which is the coupling that makes the decoupled Jacobian diverge below 100 kV.");
    return 24;
}

// inverting op-amp stage: Rin from in_node (input and output share y so stages chain with one wire),
// Rf and optional Cf in feedback, + input grounded. Returns the output node.
static int inv_stage(Circuit *circuit, float x, float y, int in_node, double rin, double rf, double cf) {
    Component *u = sat_opamp(circuit, x, y);                                         // -(x-40,y-20) +(x-40,y+20) OUT(x+40,y)
    if (!u) return -1;
    Component *ri = add_comp(circuit, COMP_RESISTOR, x - 100, y, 0);                 // (x-140,y)-(x-60,y)
    ri->props.resistor.resistance = rin;
    Component *rff = add_comp(circuit, COMP_RESISTOR, x - 20, y - 100, 0);           // (x-60,y-100)-(x+20,y-100)
    rff->props.resistor.resistance = rf;
    int inl = TN(x - 140, y), sj = TN(x - 60, y), sj2 = TN(x - 60, y - 20), minus = TN(x - 40, y - 20);
    int fl = TN(x - 60, y - 100), fr = TN(x + 20, y - 100), out = TN(x + 40, y);
    int oj2 = TN(x + 80, y - 100), oj = TN(x + 80, y);
    if (in_node >= 0 && in_node != inl) TW(in_node, inl);
    TW(sj, sj2); TW(sj2, minus); TW(fl, sj2); TW(fr, oj2); TW(oj2, oj); TW(oj, out);
    ri->node_ids[0] = inl; ri->node_ids[1] = sj; rff->node_ids[0] = fl; rff->node_ids[1] = fr;
    if (cf > 0) {
        Component *cc = add_comp(circuit, COMP_CAPACITOR, x - 20, y - 160, 0);       // (x-60,y-160)-(x+20,y-160)
        cc->props.capacitor.capacitance = cf;
        int cl = TN(x - 60, y - 160), cr = TN(x + 20, y - 160);
        TW(cl, fl); TW(cr, fr);
        cc->node_ids[0] = cl; cc->node_ids[1] = cr;
    }
    Component *g = add_comp(circuit, COMP_GROUND, x - 40, y + 60, 0);                // terminal (x-40,y+40)
    int gp = TN(x - 40, y + 20), gt = TN(x - 40, y + 40);
    TW(gp, gt);
    u->node_ids[0] = minus; u->node_ids[1] = gp; u->node_ids[2] = out; g->node_ids[0] = gt;
    return out;
}

// 8. BAL-001-TRE-2: the swing equation and a droop governor as an op-amp analog computer
static int place_gs_governor(Circuit *circuit, float x, float y) {
    // 1 V = 1 Hz of frequency deviation, 1 V = 0.1 pu of power
    Component *step = add_comp(circuit, COMP_PULSE_SOURCE, x, y + 60, 0);            // +(x,y+20) -(x,y+100)
    if (!step) return 0;
    step->props.pulse_source.v_low = 0; step->props.pulse_source.v_high = 0.5;       // 0.05 pu load step
    step->props.pulse_source.delay = 0.2; step->props.pulse_source.pulse_width = 8.0; step->props.pulse_source.period = 20.0;
    step->props.pulse_source.rise_time = 1e-3; step->props.pulse_source.fall_time = 1e-3;
    Component *gs = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    int sp = TN(x, y + 20); step->node_ids[0] = sp;
    connect_terminals(circuit, step, 1, gs, 0);
    int s0 = TN(x, y), in0 = TN(x + 160, y); TW(sp, s0); TW(s0, in0);
    // swing equation: 2H/f0 dDf/dt = Pm - Pe - D Df/f0   (H = 4 s, D = 1 pu, f0 = 60)
    int df = inv_stage(circuit, x + 300, y, in0, 133e3, 800e3, 10e-6);               // integrator with damping
    if (df < 0) return 0;
    Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 200, y - 60, 0);            // the governor's summing input
    r2->props.resistor.resistance = 133e3;
    int r2l = TN(x + 160, y - 60), r2r = TN(x + 240, y - 60);
    r2->node_ids[0] = r2l; r2->node_ids[1] = r2r;
    TW(r2r, TN(x + 240, y));                                                         // straight down onto the summing junction
    int nf = inv_stage(circuit, x + 620, y, df, 100e3, 100e3, 0);                    // unity inverter -> -Df
    if (nf < 0) return 0;
    int pm = inv_stage(circuit, x + 940, y, nf, 90e3, 300e3, 1e-6);                  // 5 % droop + 0.3 s steam chest
    if (pm < 0) return 0;
    int f1 = TN(x + 980, y + 180), f2 = TN(x - 100, y + 180), f3 = TN(x - 100, y - 60);
    TW(pm, f1); TW(f1, f2); TW(f2, f3); TW(f3, r2l);
    add_label(circuit, x - 40, y - 280, "GOVERNOR DROOP AND THE SWING EQUATION (NERC BAL-001-TRE-2): an analog-computer patch of the ERCOT frequency response");
    add_label(circuit, x + 60, y + 200, "1 V = 1 Hz of deviation, 1 V = 0.1 pu of power. U1 integrates 2H/f0 dDf/dt = Pm - Pe - D Df/f0 (H = 4 s, D = 1);");
    add_label(circuit, x + 60, y + 230, "U2 inverts the sign; U3 is the 5 % droop with the 0.3 s steam-chest lag, fed back into U1's summing junction.");
    add_label(circuit, x + 60, y + 260, "A 0.05 pu load step at 0.2 s gives the nadir, then recovery to -0.05/(1/R + D) = -0.143 Hz (59.857 Hz).");
    add_label(circuit, x + 60, y + 290, "TRY: droop 5 -> 10 % (U3 input 90k -> 180k) doubles the deviation; H 4 -> 2 s (U1 cap 10 -> 5 uF) deepens the nadir.");
    return 26;
}

// 9. CIP-014-2: a supervised fence-zone loop reporting to the substation RTU
static int place_gs_pids(Circuit *circuit, float x, float y) {
    Component *v = dc_rail(circuit, x, y, 12.0); if (!v) return 0;                   // +(x,y) RTU supply
    int rail = TN(x, y);
    Component *rp = hres(circuit, x + 140, y, 2.2e3);                                // RTU input pull-up
    int pl = TN(x + 100, y), pr = TN(x + 180, y); TW(rail, pl);
    rp->node_ids[0] = pl; rp->node_ids[1] = pr;
    int di = TN(x + 240, y); TW(pr, di);                                             // the RTU digital input
    Component *rin = vres(circuit, x + 240, y + 100, 100e3);                          // RTU input impedance
    gnd_below(circuit, rin, 1, x + 240, y + 180);
    rin->node_ids[0] = TN(x + 240, y + 60); TW(di, rin->node_ids[0]);
    Component *cut = add_comp(circuit, COMP_SPST_SWITCH, x + 340, y, 0);             // fibre / cable integrity (closed)
    cut->props.switch_spst.closed = true;
    int cl = TN(x + 300, y), cr = TN(x + 380, y); TW(di, cl);
    cut->node_ids[0] = cl; cut->node_ids[1] = cr;
    Component *rz = hres(circuit, x + 460, y, 2.2e3);                                 // zone resistor, shorted by the contact
    int zl = TN(x + 420, y), zr = TN(x + 500, y); TW(cr, zl);
    rz->node_ids[0] = zl; rz->node_ids[1] = zr;
    Component *zone = fault_switch(circuit, x + 460, y + 120, 0.0, 4.0, 7.0);        // closed 0-4 s, open (alarm) 4-7 s
    zone->props.analog_switch.r_on = 1.0;
    TW(zl, TN(x + 420, y + 120)); zone->node_ids[0] = TN(x + 420, y + 120);
    TW(TN(x + 500, y + 120), zr); zone->node_ids[1] = TN(x + 500, y + 120);
    Component *eol = vres(circuit, x + 560, y + 100, 5.6e3);                          // end-of-line supervision resistor
    int et = TN(x + 560, y + 60), eb = TN(x + 560, y + 140);
    TW(zr, TN(x + 560, y)); TW(TN(x + 560, y), et);
    Component *ge = add_comp(circuit, COMP_GROUND, x + 560, y + 160, 0);
    eol->node_ids[0] = et; eol->node_ids[1] = eb; ge->node_ids[0] = eb;
    add_label(circuit, x - 40, y - 80, "SUPERVISED PERIMETER ZONE (NERC CIP-014-2 layers 2 and 5): a fence sensor reported to the substation RTU");
    add_label(circuit, x + 60, y + 290, "One wire carries four states, so a cut or a short cannot be mistaken for 'all clear':");
    add_label(circuit, x + 60, y + 330, "  normal (contact shorts the zone resistor, 5.6k end-of-line) = 8.5 V     alarm (contact opens, 2.2k + 5.6k) = 9.2 V");
    add_label(circuit, x + 60, y + 370, "  cable cut (open the integrity switch) = 12 V                            short across the pair = 0 V");
    add_label(circuit, x + 60, y + 410, "The contact opens at 4 s for 3 s. Passive loops and fibre are used because wireless sensors fail in substation EMI.");
    return 18;
}
#undef TN
#undef TW

// ---------------------------------------------------------------------------------------
// MOSFET curve tracers. Each device sits over a small source-sense resistor, so the probed
// node is I_D times that resistance - press the scope's Y-T button for X-Y and the traces
// become real transfer and output characteristics.
// ---------------------------------------------------------------------------------------
#define TN(cx, cy) circuit_find_or_create_node(circuit, (cx), (cy), 5.0f)
#define TW(a_, b_) circuit_add_wire(circuit, (a_), (b_))

// one device: gate on gate_node, drain through rd to the rail, source through rs to ground
static Component *curve_device(Circuit *circuit, float x, float y, int gate_node, int rail_node,
                               double vth, double kn, double rd, double rs) {
    Component *m = add_comp(circuit, COMP_NMOS, x, y, 0);                    // G(x-20,y) D(x+20,y-20) S(x+20,y+20)
    if (!m) return NULL;
    m->props.mosfet.vth = vth;
    m->props.mosfet.l = 1e-6;
    m->props.mosfet.w = (kn / m->props.mosfet.kp) * m->props.mosfet.l;       // W/L set from the device k_n
    m->props.mosfet.ideal = false;
    Component *rdr = add_comp(circuit, COMP_RESISTOR, x + 40, y - 100, 90);  // (x+40,y-140)-(x+40,y-60)
    rdr->props.resistor.resistance = rd;
    rdr->props.resistor.power_rating = 5.0;                                  // a curve tracer's load resistor is a real 5 W part
    Component *rsr = add_comp(circuit, COMP_RESISTOR, x + 40, y + 100, 90);  // (x+40,y+60)-(x+40,y+140)
    rsr->props.resistor.resistance = rs;
    Component *g = add_comp(circuit, COMP_GROUND, x + 40, y + 160, 0);
    int gt = TN(x - 20, y), d = TN(x + 20, y - 20), sN = TN(x + 20, y + 20);
    int dj = TN(x + 40, y - 20), dt = TN(x + 40, y - 60), rt = TN(x + 40, y - 140);
    int sj = TN(x + 40, y + 20), st = TN(x + 40, y + 60), sb = TN(x + 40, y + 140), gnd = TN(x + 40, y + 160);
    TW(gate_node, gt); TW(d, dj); TW(dj, dt); TW(rt, rail_node);
    TW(sN, sj); TW(sj, st); TW(sb, gnd);
    m->node_ids[0] = gt; m->node_ids[1] = d; m->node_ids[2] = sN;
    rdr->node_ids[0] = rt; rdr->node_ids[1] = dt;
    rsr->node_ids[0] = st; rsr->node_ids[1] = sb;
    g->node_ids[0] = gnd;
    return m;
}

// 1. Transfer curves: one gate ramp, three different devices, each over its own sense resistor
static int place_mos_idvgs(Circuit *circuit, float x, float y) {
    Component *vdd = dc_rail(circuit, x, y - 260, 10.0); if (!vdd) return 0;   // +(x,y-260)
    int rail = TN(x, y - 260);
    Component *sweep = add_comp(circuit, COMP_TRIANGLE_WAVE, x, y + 60, 0);    // +(x,y+20) -(x,y+100)
    sweep->props.triangle_wave.amplitude = 2.0; sweep->props.triangle_wave.offset = 2.0;
    sweep->props.triangle_wave.frequency = 100.0;
    Component *gg = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    int gate = TN(x, y + 20); sweep->node_ids[0] = gate;
    connect_terminals(circuit, sweep, 1, gg, 0);
    static const double vth[3] = { 2.1,   1.6,   0.7 };
    static const double kn[3]  = { 0.105, 0.060, 1.1e-3 };
    static const double rd[3]  = { 22.0,  33.0,  1000.0 };
    static const char *nm[3] = { "2N7000: Vth 2.1 V, kn 105 mA/V2", "2N7002: Vth 1.6 V, kn 60 mA/V2", "1 um NMOS: Vth 0.7 V, kn 1.1 mA/V2" };
    int gb0 = TN(x + 60, y + 20), gb1 = TN(x + 60, y + 200);                  // gate bus runs below the devices
    TW(gate, gb0); TW(gb0, gb1);
    int gj = gb1;
    for (int k = 0; k < 3; k++) {
        float dx = x + 200 + k * 220;
        int gn = TN(dx - 60, y + 200), gu = TN(dx - 60, y), gt = TN(dx - 20, y);
        TW(gj, gn); TW(gn, gu); TW(gu, gt);
        int rn = TN(dx + 40, y - 260); TW(rail, rn);
        curve_device(circuit, dx, y, gt, rn, vth[k], kn[k], rd[k], 1.0);
        add_label(circuit, x - 80, y + 240 + k * 30, nm[k]);
        gj = gn;
        rail = rn;
    }
    add_label(circuit, x - 40, y - 320, "MOSFET TRANSFER CURVES: one 0-4 V gate ramp into three devices; each source sense resistor makes the probe read I_D");
    add_label(circuit, x - 40, y + 420, "Press the scope's Y-T button for X-Y and CH1 (the gate) becomes the x axis: these are I_D vs V_GS curves.");
    add_label(circuit, x - 40, y + 330, "Each channel is 1 ohm x I_D, so 1 V = 1 A. Watch each device leave cutoff at its own Vth and rise with its own kn.");
    add_label(circuit, x - 40, y + 360, "TRY: select a device and edit Vth, W/L or Kn in the properties panel - the curve moves as you type.");
    return 20;
}

// 2. Output characteristics: one drain sweep, the same device at three gate voltages
static int place_mos_idvds(Circuit *circuit, float x, float y) {
    Component *sweep = add_comp(circuit, COMP_TRIANGLE_WAVE, x, y - 200, 0);   // +(x,y-240) -(x,y-160)
    if (!sweep) return 0;
    sweep->props.triangle_wave.amplitude = 3.0; sweep->props.triangle_wave.offset = 3.0;
    sweep->props.triangle_wave.frequency = 100.0;
    Component *sg = add_comp(circuit, COMP_GROUND, x, y - 120, 0);
    int rail = TN(x, y - 240); sweep->node_ids[0] = rail;
    connect_terminals(circuit, sweep, 1, sg, 0);
    static const double vgs[3] = { 2.5, 3.0, 3.5 };
    static const char *nm[3] = { "Vgs 2.5 V", "Vgs 3.0 V", "Vgs 3.5 V" };
    for (int k = 0; k < 3; k++) {
        float dx = x + 220 + k * 220;
        Component *vg = add_comp(circuit, COMP_DC_VOLTAGE, dx - 60, y + 60, 0);   // +(dx-60,y+20) -(dx-60,y+100)
        vg->props.dc_voltage.voltage = vgs[k];
        Component *gg = add_comp(circuit, COMP_GROUND, dx - 60, y + 140, 0);
        int gp = TN(dx - 60, y + 20), gu = TN(dx - 60, y), gt = TN(dx - 20, y);
        vg->node_ids[0] = gp; TW(gp, gu); TW(gu, gt);
        connect_terminals(circuit, vg, 1, gg, 0);
        int rn = TN(dx + 40, y - 240); TW(rail, rn);
        curve_device(circuit, dx, y, gt, rn, 2.1, 0.105, 0.001, 2.0);             // drain straight onto the sweep
        add_label(circuit, dx - 60, y + 200, nm[k]);
        rail = rn;
    }
    add_label(circuit, x - 40, y - 300, "MOSFET OUTPUT CHARACTERISTICS: one 0-6 V drain sweep, the same 2N7000-class device held at three gate voltages");
    add_label(circuit, x - 40, y + 260, "Press Y-T for X-Y with CH1 (the sweep) as the x axis and you get the classic I_D vs V_DS family:");
    add_label(circuit, x - 40, y + 290, "a steep triode slope up to V_DS = V_OV, then the flat saturation region. Each channel is 2 ohm x I_D.");
    add_label(circuit, x - 40, y + 320, "TRY: raise lambda on a device and its saturation region stops being flat - that is channel-length modulation.");
    return 22;
}
#undef TN
#undef TW

// ---------------------------------------------------------------------------------------
// MOSFET amplifier family and CMOS logic at the transistor level. Shared geometry: the gate
// line is y, the NMOS sits at (x,y) with G(x-20,y) D(x+20,y-20) S(x+20,y+20), the supply rail
// runs at y-180 and ground returns are at y+160 or below.
// ---------------------------------------------------------------------------------------
#define TN(cx, cy) circuit_find_or_create_node(circuit, (cx), (cy), 5.0f)
#define TW(a_, b_) circuit_add_wire(circuit, (a_), (b_))

// gate bias divider from rail to ground, gate taken at (gx, y); returns the gate node
static int mos_gate_bias(Circuit *circuit, float gx, float y, float rail_y, int rail, double r1, double r2) {
    Component *rg1 = add_comp(circuit, COMP_RESISTOR, gx, y - 100, 90);   // (gx,y-140)-(gx,y-60)
    rg1->props.resistor.resistance = r1;
    Component *rg2 = add_comp(circuit, COMP_RESISTOR, gx, y + 60, 90);    // (gx,y+20)-(gx,y+100)
    rg2->props.resistor.resistance = r2;
    Component *g = add_comp(circuit, COMP_GROUND, gx, y + 120, 0);        // terminal (gx,y+100)
    int t1 = TN(gx, y - 140), b1 = TN(gx, y - 60), gate = TN(gx, y), t2 = TN(gx, y + 20), b2 = TN(gx, y + 100);
    int up = TN(gx, rail_y); TW(rail, up); TW(up, t1);   // along the rail, then down: no diagonal
    TW(b1, gate); TW(gate, t2);
    rg1->node_ids[0] = t1; rg1->node_ids[1] = b1; rg2->node_ids[0] = t2; rg2->node_ids[1] = b2; g->node_ids[0] = b2;
    return gate;
}
// source resistor (optionally bypassed) from the device source at (sx,y+20) down to ground
static void mos_source_leg(Circuit *circuit, float sx, float y, int src_node, double rs, double cbyp) {
    Component *r = add_comp(circuit, COMP_RESISTOR, sx, y + 100, 90);     // (sx,y+60)-(sx,y+140)
    r->props.resistor.resistance = rs;
    Component *g = add_comp(circuit, COMP_GROUND, sx, y + 160, 0);        // terminal (sx,y+140)
    int t = TN(sx, y + 60), b = TN(sx, y + 140), j = TN(sx, y + 20);
    TW(src_node, j); TW(j, t);
    r->node_ids[0] = t; r->node_ids[1] = b; g->node_ids[0] = b;
    if (cbyp > 0) {
        Component *c = add_comp(circuit, COMP_CAPACITOR, sx + 80, y + 100, 90);
        c->props.capacitor.capacitance = cbyp;
        Component *g2 = add_comp(circuit, COMP_GROUND, sx + 80, y + 160, 0);
        int ct = TN(sx + 80, y + 60), cb = TN(sx + 80, y + 140);
        TW(t, ct);
        c->node_ids[0] = ct; c->node_ids[1] = cb; g2->node_ids[0] = cb;
    }
}
static Component *mos_dev(Circuit *circuit, float x, float y, double vth, double kp) {
    Component *m = add_comp(circuit, COMP_NMOS, x, y, 0);
    if (m) { m->props.mosfet.vth = vth; m->props.mosfet.kp = kp; m->props.mosfet.ideal = false; }
    return m;
}

// 1. MOSFET single-tuned amplifier: the MOSFET counterpart of the BJT one, LC tank drain load
static int place_mos_tuned(Circuit *circuit, float x, float y) {
    Component *vdd = dc_rail(circuit, x - 320, y - 180, 12.0); if (!vdd) return 0;   // +(-320,-180)
    int rail = TN(x - 320, y - 180), rgb = TN(x - 60, y - 180); TW(rail, rgb);   // chained taps
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x - 260, y + 60, 0);        // +(-260,20) -(-260,100)
    vin->props.ac_voltage.amplitude = 0.01; vin->props.ac_voltage.frequency = 100059.9;
    set_freq_sweep(vin, 20e3, 500e3, 0.5);
    Component *gi = add_comp(circuit, COMP_GROUND, x - 260, y + 140, 0);
    connect_terminals(circuit, vin, 1, gi, 0);
    int sp = TN(x - 260, y + 20), s1 = TN(x - 200, y + 20), s2 = TN(x - 200, y); TW(sp, s1); TW(s1, s2);
    vin->node_ids[0] = sp;
    Component *cin = add_comp(circuit, COMP_CAPACITOR, x - 160, y, 0);              // (-200,0)-(-120,0)
    cin->props.capacitor.capacitance = 10e-9;
    int cl = TN(x - 200, y), cr = TN(x - 120, y);
    cin->node_ids[0] = cl; cin->node_ids[1] = cr;
    int gate = mos_gate_bias(circuit, x - 60, y, y - 180, rgb, 1e6, 330e3);
    TW(cr, gate);
    Component *m = mos_dev(circuit, x, y, 1.5, 0.01); if (!m) return 0;
    int d = TN(x + 20, y - 20), dj = TN(x + 40, y - 20), tankb = TN(x + 40, y - 60);
    TW(d, dj); TW(dj, tankb);
    m->node_ids[0] = gate; m->node_ids[1] = d; m->node_ids[2] = TN(x + 20, y + 20);
    static const double val[3] = { 1e-3, 2.53e-9, 10e3 };
    int rprev = rgb;
    for (int k = 0; k < 3; k++) {                                                   // L, C, Rq in parallel
        float px = x + 40 + k * 80;   /* 60 was narrower than the tank capacitor's own label */
        Component *c = add_comp(circuit, k == 0 ? COMP_INDUCTOR : (k == 1 ? COMP_CAPACITOR : COMP_RESISTOR), px, y - 100, 90);
        if (k == 0) c->props.inductor.inductance = val[0];
        else if (k == 1) c->props.capacitor.capacitance = val[1];
        else c->props.resistor.resistance = val[2];
        int t = TN(px, y - 140), b = TN(px, y - 60), rn = TN(px, y - 180);
        TW(rprev, rn); TW(rn, t); TW(b, tankb);
        rprev = rn;
        c->node_ids[0] = t; c->node_ids[1] = b;
    }
    mos_source_leg(circuit, x + 40, y, TN(x + 20, y + 20), 470.0, 10e-6);
    Component *co = add_comp(circuit, COMP_CAPACITOR, x + 280, y - 60, 0);          // (240,-60)-(320,-60)
    co->props.capacitor.capacitance = 10e-9;
    int col = TN(x + 240, y - 60), cor = TN(x + 320, y - 60); TW(tankb, TN(x + 200, y - 60)); TW(TN(x + 200, y - 60), col);
    co->node_ids[0] = col; co->node_ids[1] = cor;
    Component *rl = add_comp(circuit, COMP_RESISTOR, x + 360, y - 20, 90);          // (360,-60)-(360,20)
    rl->props.resistor.resistance = 100e3;
    Component *gl = add_comp(circuit, COMP_GROUND, x + 360, y + 40, 0);
    int rt = TN(x + 360, y - 60), rb = TN(x + 360, y + 20); TW(cor, rt);
    rl->node_ids[0] = rt; rl->node_ids[1] = rb; gl->node_ids[0] = rb;
    add_label(circuit, x - 320, y - 260, "MOSFET single-tuned amplifier: the same 100 kHz tank as the BJT version, driven by a common-source stage");
    return 20;
}

// 2. Common gate: signal into the source, gate held at AC ground, output at the drain
static int place_mos_cg(Circuit *circuit, float x, float y) {
    Component *vdd = dc_rail(circuit, x - 320, y - 180, 12.0); if (!vdd) return 0;
    int rail = TN(x - 320, y - 180), rail2 = TN(x + 40, y - 180); TW(rail, rail2);
    int gate = mos_gate_bias(circuit, x - 60, y, y - 180, rail, 1e6, 470e3);
    TW(gate, TN(x - 20, y));
    Component *cg = add_comp(circuit, COMP_CAPACITOR, x - 140, y + 60, 90);         // (-140,20)-(-140,100) gate bypass
    cg->props.capacitor.capacitance = 10e-6;
    Component *gg = add_comp(circuit, COMP_GROUND, x - 140, y + 120, 0);
    int ct = TN(x - 140, y + 20), cb = TN(x - 140, y + 100);
    TW(gate, TN(x - 140, y)); TW(TN(x - 140, y), ct);
    cg->node_ids[0] = ct; cg->node_ids[1] = cb; gg->node_ids[0] = cb;
    Component *m = mos_dev(circuit, x, y, 1.5, 0.01); if (!m) return 0;
    int d = TN(x + 20, y - 20), sN = TN(x + 20, y + 20);
    m->node_ids[0] = gate; m->node_ids[1] = d; m->node_ids[2] = sN;
    Component *rd = add_comp(circuit, COMP_RESISTOR, x + 40, y - 100, 90);
    rd->props.resistor.resistance = 2.2e3;
    int rt = TN(x + 40, y - 140), rb = TN(x + 40, y - 60);
    TW(rt, rail2); TW(d, TN(x + 40, y - 20)); TW(TN(x + 40, y - 20), rb);
    rd->node_ids[0] = rt; rd->node_ids[1] = rb;
    mos_source_leg(circuit, x + 40, y, sN, 3.3e3, 0);   // 0.65 mA keeps V_DS ~9 V, well into saturation
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x + 200, y + 220, 0);       // +(200,180) -(200,260)
    vin->props.ac_voltage.amplitude = 0.02; vin->props.ac_voltage.frequency = 10e3;
    Component *gvi = add_comp(circuit, COMP_GROUND, x + 200, y + 300, 0);
    connect_terminals(circuit, vin, 1, gvi, 0);
    Component *ci = add_comp(circuit, COMP_CAPACITOR, x + 120, y + 180, 0);         // (80,180)-(160,180)
    ci->props.capacitor.capacitance = 10e-6;
    int il = TN(x + 80, y + 180), ir = TN(x + 160, y + 180), vp = TN(x + 200, y + 180);
    TW(ir, vp); TW(il, TN(x + 40, y + 180)); TW(TN(x + 40, y + 180), TN(x + 40, y + 60));
    ci->node_ids[0] = il; ci->node_ids[1] = ir; vin->node_ids[0] = vp;
    Component *co = add_comp(circuit, COMP_CAPACITOR, x + 160, y - 60, 0);          // (120,-60)-(200,-60)
    co->props.capacitor.capacitance = 10e-6;
    int ol = TN(x + 120, y - 60), orr = TN(x + 200, y - 60);
    TW(rb, ol);
    co->node_ids[0] = ol; co->node_ids[1] = orr;
    Component *rl = add_comp(circuit, COMP_RESISTOR, x + 240, y - 20, 90);
    rl->props.resistor.resistance = 100e3;
    Component *gl = add_comp(circuit, COMP_GROUND, x + 240, y + 40, 0);
    int lt = TN(x + 240, y - 60), lb = TN(x + 240, y + 20); TW(orr, lt);
    rl->node_ids[0] = lt; rl->node_ids[1] = lb; gl->node_ids[0] = lb;
    add_label(circuit, x - 320, y - 260, "COMMON GATE: the signal drives the SOURCE, the gate is an AC ground, the output leaves the drain IN PHASE");
    return 18;
}

// 3. Cascode: a common-source device with a common-gate device stacked on its drain
static int place_mos_cascode(Circuit *circuit, float x, float y) {
    Component *vdd = dc_rail(circuit, x - 320, y - 320, 12.0); if (!vdd) return 0;
    int rail = TN(x - 320, y - 320), rail2 = TN(x + 40, y - 320); TW(rail, rail2);
    Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x - 260, y + 60, 0);
    vin->props.ac_voltage.amplitude = 0.01; vin->props.ac_voltage.frequency = 10e3;
    Component *gi = add_comp(circuit, COMP_GROUND, x - 260, y + 140, 0);
    connect_terminals(circuit, vin, 1, gi, 0);
    int sp = TN(x - 260, y + 20), s2 = TN(x - 200, y + 20); TW(sp, s2); TW(s2, TN(x - 200, y));
    vin->node_ids[0] = sp;
    Component *cin = add_comp(circuit, COMP_CAPACITOR, x - 160, y, 0);
    cin->props.capacitor.capacitance = 10e-6;
    int cl = TN(x - 200, y), cr = TN(x - 120, y);
    cin->node_ids[0] = cl; cin->node_ids[1] = cr;
    int gate1 = mos_gate_bias(circuit, x - 60, y, y - 320, rail, 1e6, 330e3);
    TW(cr, gate1);
    Component *m1 = mos_dev(circuit, x, y, 1.5, 0.01); if (!m1) return 0;           // common source
    m1->props.mosfet.ideal = true;
    int d1 = TN(x + 20, y - 20), s1n = TN(x + 20, y + 20);
    m1->node_ids[0] = gate1; m1->node_ids[1] = d1; m1->node_ids[2] = s1n;
    mos_source_leg(circuit, x + 40, y, s1n, 2.2e3, 10e-6);   // 0.62 mA leaves both devices saturated
    Component *m2 = mos_dev(circuit, x, y - 160, 1.5, 0.01);                        // common gate on top
    if (m2) m2->props.mosfet.ideal = true;
    int d2 = TN(x + 20, y - 180), s2n = TN(x + 20, y - 140);
    TW(d1, TN(x + 40, y - 20)); TW(TN(x + 40, y - 20), TN(x + 40, y - 140)); TW(TN(x + 40, y - 140), s2n);
    int gate2 = mos_gate_bias(circuit, x - 160, y - 160, y - 320, rail, 470e3, 1e6);
    TW(gate2, TN(x - 20, y - 160));
    m2->node_ids[0] = gate2; m2->node_ids[1] = d2; m2->node_ids[2] = s2n;
    Component *rd = add_comp(circuit, COMP_RESISTOR, x + 40, y - 260, 90);          // (40,-300)-(40,-220)
    rd->props.resistor.resistance = 3.3e3;
    int rt = TN(x + 40, y - 300), rb = TN(x + 40, y - 220);
    TW(rt, rail2); TW(d2, TN(x + 40, y - 180)); TW(TN(x + 40, y - 180), rb);
    rd->node_ids[0] = rt; rd->node_ids[1] = rb;
    Component *co = add_comp(circuit, COMP_CAPACITOR, x + 160, y - 220, 0);
    co->props.capacitor.capacitance = 10e-6;
    int ol = TN(x + 120, y - 220), orr = TN(x + 200, y - 220); TW(rb, TN(x + 80, y - 220)); TW(TN(x + 80, y - 220), ol);
    co->node_ids[0] = ol; co->node_ids[1] = orr;
    Component *rl = add_comp(circuit, COMP_RESISTOR, x + 240, y - 180, 90);
    rl->props.resistor.resistance = 100e3;
    Component *gl = add_comp(circuit, COMP_GROUND, x + 240, y - 120, 0);
    int lt = TN(x + 240, y - 220), lb = TN(x + 240, y - 140); TW(orr, lt);
    rl->node_ids[0] = lt; rl->node_ids[1] = lb; gl->node_ids[0] = lb;
    add_label(circuit, x - 320, y - 400, "CASCODE: a common-source device under a common-gate device. The lower drain barely moves, so Miller feedback nearly vanishes");
    return 22;
}

// 4. MOSFET differential pair
static int place_mos_diff(Circuit *circuit, float x, float y) {
    Component *vdd = dc_rail(circuit, x - 260, y - 180, 12.0); if (!vdd) return 0;
    int rail = TN(x - 260, y - 180), railA = TN(x + 40, y - 180), railB = TN(x + 240, y - 180);
    TW(rail, railA); TW(railA, railB);
    int tail = TN(x + 140, y + 60);
    for (int k = 0; k < 2; k++) {
        float dx = x + k * 200;
        Component *m = mos_dev(circuit, dx, y, 1.5, 0.01); if (!m) return 0;
        int d = TN(dx + 20, y - 20), sN = TN(dx + 20, y + 20), gt = TN(dx - 20, y);
        Component *rd = add_comp(circuit, COMP_RESISTOR, dx + 40, y - 100, 90);
        rd->props.resistor.resistance = 2.2e3;
        int rt = TN(dx + 40, y - 140), rb = TN(dx + 40, y - 60);
        TW(rt, TN(dx + 40, y - 180)); TW(d, TN(dx + 40, y - 20)); TW(TN(dx + 40, y - 20), rb);
        rd->node_ids[0] = rt; rd->node_ids[1] = rb;
        m->node_ids[1] = d; m->node_ids[2] = sN;
        TW(sN, TN(dx + 20, y + 60)); TW(TN(dx + 20, y + 60), tail);
        /* Each source sits outside its own half of the pair. Both used to be placed to the left
           of their transistor, which put the second one between the two devices - directly on
           the row the sources share to reach the tail, so that wire ran through its body. */
        float sx2 = (k == 0) ? dx - 120 : dx + 120;
        Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, sx2, y + 60, 0);        // +(sx2,y+20)
        vin->props.ac_voltage.amplitude = 0.02; vin->props.ac_voltage.frequency = 1000.0;
        vin->props.ac_voltage.phase = k ? 180.0 : 0.0; vin->props.ac_voltage.offset = 3.0;
        Component *gv = add_comp(circuit, COMP_GROUND, sx2, y + 140, 0);
        connect_terminals(circuit, vin, 1, gv, 0);
        int vp = TN(sx2, y + 20), vj = TN(sx2, y);
        TW(vp, vj); TW(vj, gt);
        vin->node_ids[0] = vp; m->node_ids[0] = gt;
    }
    Component *rt2 = add_comp(circuit, COMP_RESISTOR, x + 140, y + 140, 90);        // tail resistor
    rt2->props.resistor.resistance = 2.2e3;
    Component *gt2 = add_comp(circuit, COMP_GROUND, x + 140, y + 200, 0);
    int tt = TN(x + 140, y + 100), tb = TN(x + 140, y + 180); TW(tail, tt);
    rt2->node_ids[0] = tt; rt2->node_ids[1] = tb; gt2->node_ids[0] = tb;
    add_label(circuit, x - 260, y - 260, "MOSFET DIFFERENTIAL PAIR: the tail resistor sets the current, a difference between the gates steers it one way or the other");
    return 20;
}

// 5. MOSFET current mirror: a diode-connected reference device copied into a load
static int place_mos_mirror(Circuit *circuit, float x, float y) {
    Component *vdd = dc_rail(circuit, x - 260, y - 180, 12.0); if (!vdd) return 0;
    int rail = TN(x - 260, y - 180), railA = TN(x - 60, y - 180), railB = TN(x + 200, y - 180);
    TW(rail, railA); TW(railA, railB);
    Component *rref = add_comp(circuit, COMP_RESISTOR, x - 60, y - 100, 90);
    rref->props.resistor.resistance = 10e3;
    int rt = TN(x - 60, y - 140), rb = TN(x - 60, y - 60); TW(rt, railA);
    rref->node_ids[0] = rt; rref->node_ids[1] = rb;
    Component *m1 = mos_dev(circuit, x, y, 1.5, 0.01); if (!m1) return 0;
    int d1 = TN(x + 20, y - 20), s1 = TN(x + 20, y + 20), g1 = TN(x - 20, y);
    /* straight into the drain; it used to overshoot to x+40 and come back, which drew the wire
       out through the far side of the transistor */
    TW(rb, TN(x - 60, y - 20)); TW(TN(x - 60, y - 20), d1);
    TW(TN(x - 60, y - 20), TN(x - 60, y)); TW(TN(x - 60, y), g1);                   // diode connection
    m1->node_ids[0] = g1; m1->node_ids[1] = d1; m1->node_ids[2] = s1;
    Component *gs1 = add_comp(circuit, COMP_GROUND, x + 20, y + 60, 0);
    gs1->node_ids[0] = s1;
    Component *m2 = mos_dev(circuit, x + 200, y, 1.5, 0.01);
    int d2 = TN(x + 220, y - 20), s2 = TN(x + 220, y + 20), g2 = TN(x + 180, y);
    TW(g1, TN(x - 20, y + 100)); TW(TN(x - 20, y + 100), TN(x + 180, y + 100)); TW(TN(x + 180, y + 100), g2);
    m2->node_ids[0] = g2; m2->node_ids[1] = d2; m2->node_ids[2] = s2;
    Component *gs2 = add_comp(circuit, COMP_GROUND, x + 220, y + 60, 0);
    gs2->node_ids[0] = s2;
    Component *rl = add_comp(circuit, COMP_RESISTOR, x + 240, y - 100, 90);
    rl->props.resistor.resistance = 4.7e3;
    int lt = TN(x + 240, y - 140), lb = TN(x + 240, y - 60);
    TW(lt, TN(x + 240, y - 180)); TW(TN(x + 240, y - 180), railB); TW(d2, TN(x + 240, y - 20)); TW(TN(x + 240, y - 20), lb);
    rl->node_ids[0] = lt; rl->node_ids[1] = lb;
    add_label(circuit, x - 260, y - 260, "MOSFET CURRENT MIRROR: M1 is diode-connected so Rref sets its current; M2 shares the gate-source voltage and copies it");
    return 14;
}

// 6. CMOS inverter: sweep the input and read the voltage transfer characteristic
static int place_cmos_inv(Circuit *circuit, float x, float y) {
    Component *vdd = dc_rail(circuit, x - 200, y - 180, 5.0); if (!vdd) return 0;
    int rail = TN(x - 200, y - 180), railA = TN(x + 20, y - 180); TW(rail, railA);
    Component *sweep = add_comp(circuit, COMP_TRIANGLE_WAVE, x - 200, y + 60, 0);   // +(-200,20) -(-200,100)
    sweep->props.triangle_wave.amplitude = 2.5; sweep->props.triangle_wave.offset = 2.5;
    sweep->props.triangle_wave.frequency = 1000.0;
    Component *gs = add_comp(circuit, COMP_GROUND, x - 200, y + 140, 0);
    int inn = TN(x - 200, y + 20); sweep->node_ids[0] = inn;
    connect_terminals(circuit, sweep, 1, gs, 0);
    int ij = TN(x - 200, y), gp = TN(x - 20, y - 80), gn = TN(x - 20, y + 80);
    TW(inn, ij); TW(ij, TN(x - 100, y)); TW(TN(x - 100, y), TN(x - 100, y - 80)); TW(TN(x - 100, y - 80), gp);
    TW(TN(x - 100, y), TN(x - 100, y + 80)); TW(TN(x - 100, y + 80), gn);
    Component *mp = add_comp(circuit, COMP_PMOS, x, y - 80, 0);                     // G(-20,-80) D(20,-100) S(20,-60)
    mp->props.mosfet.vth = -1.0; mp->props.mosfet.kp = 0.005; mp->props.mosfet.ideal = true;
    Component *mn = add_comp(circuit, COMP_NMOS, x, y + 80, 0);                     // G(-20,80) D(20,60) S(20,100)
    mn->props.mosfet.vth = 1.0; mn->props.mosfet.kp = 0.01; mn->props.mosfet.ideal = true;
    int pd = TN(x + 20, y - 100), ps = TN(x + 20, y - 60), nd = TN(x + 20, y + 60), ns = TN(x + 20, y + 100);
    TW(ps, TN(x + 60, y - 60)); TW(TN(x + 60, y - 60), TN(x + 60, y - 180)); TW(TN(x + 60, y - 180), railA);
    int out = TN(x + 20, y);
    /* A stub from the drain out to (-20,-120) used to hang here. It connected to nothing and it
       was drawn straight back through the PMOS it came from. */
    TW(pd, out); TW(out, nd);
    Component *gnd = add_comp(circuit, COMP_GROUND, x + 20, y + 140, 0);
    gnd->node_ids[0] = ns;
    mp->node_ids[0] = gp; mp->node_ids[1] = pd; mp->node_ids[2] = ps;
    mn->node_ids[0] = gn; mn->node_ids[1] = nd; mn->node_ids[2] = ns;
    Component *cl = add_comp(circuit, COMP_CAPACITOR, x + 140, y + 40, 90);         // (140,0)-(140,80)
    cl->props.capacitor.capacitance = 20e-12;
    Component *gc = add_comp(circuit, COMP_GROUND, x + 140, y + 140, 0);
    int clt = TN(x + 140, y), clb = TN(x + 140, y + 80), gct = TN(x + 140, y + 120);
    TW(out, clt); TW(clb, gct);
    cl->node_ids[0] = clt; cl->node_ids[1] = clb; gc->node_ids[0] = gct;
    add_label(circuit, x - 200, y - 260, "CMOS INVERTER: a 0-5 V triangle on the gates. Press Y-T for X-Y and CH1 becomes the x axis: that is the VTC");
    add_label(circuit, x - 200, y + 200, "Both devices conduct only in the narrow middle band - that is the crossbar current, and why CMOS burns power only while switching.");
    return 14;
}

// 7. CMOS NAND: two PMOS in parallel above, two NMOS in series below
static int place_cmos_nand(Circuit *circuit, float x, float y) {
    Component *vdd = dc_rail(circuit, x - 260, y - 200, 5.0); if (!vdd) return 0;
    int rail = TN(x - 260, y - 200), railA = TN(x + 60, y - 200); TW(rail, railA);
    Component *pa = logic_pulse(circuit, x - 260, y + 60, 5.0, 1e3, 0.5, 0);        // A
    Component *pb = logic_pulse(circuit, x - 260, y + 260, 5.0, 500.0, 0.5, 0);     // B, half the rate
    int an = TN(x - 260, y + 60), bn = TN(x - 260, y + 260);
    pa->node_ids[0] = an; pb->node_ids[0] = bn;
    Component *mp1 = add_comp(circuit, COMP_PMOS, x, y - 120, 0);                   // G(-20,-120) D(20,-140) S(20,-100)
    Component *mp2 = add_comp(circuit, COMP_PMOS, x + 120, y - 120, 0);
    Component *mn1 = add_comp(circuit, COMP_NMOS, x, y + 40, 0);                    // G(-20,40) D(20,20) S(20,60)
    Component *mn2 = add_comp(circuit, COMP_NMOS, x, y + 160, 0);
    mp1->props.mosfet.vth = -1.0; mp2->props.mosfet.vth = -1.0;
    mp1->props.mosfet.kp = 0.005; mp2->props.mosfet.kp = 0.005;
    mn1->props.mosfet.kp = 0.01; mn2->props.mosfet.kp = 0.01;
    mp1->props.mosfet.ideal = mp2->props.mosfet.ideal = mn1->props.mosfet.ideal = mn2->props.mosfet.ideal = true;   // logic demo: the plain square-law model converges cleanly
    int out = TN(x + 20, y - 40);
    int p1d = TN(x + 20, y - 140), p1s = TN(x + 20, y - 100), p2d = TN(x + 140, y - 140), p2s = TN(x + 140, y - 100);
    TW(p1s, TN(x + 60, y - 100)); TW(TN(x + 60, y - 100), railA); TW(p2s, TN(x + 140, y - 200)); TW(TN(x + 140, y - 200), railA);
    TW(p1d, out); TW(p2d, TN(x + 140, y - 40)); TW(TN(x + 140, y - 40), out);
    int n1d = TN(x + 20, y + 20), n1s = TN(x + 20, y + 60), n2d = TN(x + 20, y + 140), n2s = TN(x + 20, y + 180);
    TW(out, n1d); TW(n1s, n2d);
    Component *gnd = add_comp(circuit, COMP_GROUND, x + 20, y + 220, 0);
    gnd->node_ids[0] = n2s;
    Component *rbl = add_comp(circuit, COMP_RESISTOR, x + 100, y + 100, 90);        // (100,60)-(100,140): keeps the
    rbl->props.resistor.resistance = 1e6;                                           // series-stack mid node defined
    Component *gbl = add_comp(circuit, COMP_GROUND, x + 100, y + 180, 0);
    int bt = TN(x + 100, y + 60), bb = TN(x + 100, y + 140), bg = TN(x + 100, y + 160);
    TW(n1s, bt); TW(bb, bg);
    rbl->node_ids[0] = bt; rbl->node_ids[1] = bb; gbl->node_ids[0] = bg;
    int ga1 = TN(x - 20, y - 120), ga2 = TN(x - 20, y + 40), gb1 = TN(x - 20, y + 160), gb2 = TN(x + 100, y - 120);
    TW(an, TN(x - 160, y + 60)); TW(TN(x - 160, y + 60), TN(x - 160, y - 120)); TW(TN(x - 160, y - 120), ga1);
    TW(TN(x - 160, y + 60), TN(x - 160, y + 40)); TW(TN(x - 160, y + 40), ga2);
    TW(bn, TN(x - 200, y + 260)); TW(TN(x - 200, y + 260), TN(x - 200, y + 160)); TW(TN(x - 200, y + 160), gb1);
    TW(TN(x - 200, y + 260), TN(x - 200, y - 260)); TW(TN(x - 200, y - 260), TN(x + 100, y - 260)); TW(TN(x + 100, y - 260), gb2);
    mp1->node_ids[0] = ga1; mp1->node_ids[1] = p1d; mp1->node_ids[2] = p1s;
    mp2->node_ids[0] = gb2; mp2->node_ids[1] = p2d; mp2->node_ids[2] = p2s;
    mn1->node_ids[0] = ga2; mn1->node_ids[1] = n1d; mn1->node_ids[2] = n1s;
    mn2->node_ids[0] = gb1; mn2->node_ids[1] = n2d; mn2->node_ids[2] = n2s;
    Component *rl = add_comp(circuit, COMP_RESISTOR, x + 240, y - 40, 90);
    rl->props.resistor.resistance = 100e3;
    Component *gl = add_comp(circuit, COMP_GROUND, x + 240, y + 40, 0);
    int lt = TN(x + 240, y - 80), lb = TN(x + 240, y), lg = TN(x + 240, y + 20);
    /* Up to the load resistor's own top terminal. Running along y-40 to x+240 arrived at the
       middle of the resistor body, because that is the resistor's centre, not a pin. */
    TW(TN(x + 140, y - 40), TN(x + 200, y - 40));
    TW(TN(x + 200, y - 40), TN(x + 200, y - 80));
    TW(TN(x + 200, y - 80), lt); TW(lb, lg);
    rl->node_ids[0] = lt; rl->node_ids[1] = lb; gl->node_ids[0] = lg;
    Component *cld = add_comp(circuit, COMP_CAPACITOR, x + 320, y - 40, 90);        // (320,-80)-(320,0)
    cld->props.capacitor.capacitance = 20e-12;
    Component *gcl = add_comp(circuit, COMP_GROUND, x + 320, y + 40, 0);
    int ct2 = TN(x + 320, y - 80), cb2 = TN(x + 320, y), cg2 = TN(x + 320, y + 20);
    TW(lt, ct2); TW(cb2, cg2);
    cld->node_ids[0] = ct2; cld->node_ids[1] = cb2; gcl->node_ids[0] = cg2;
    add_label(circuit, x - 260, y - 300, "CMOS NAND: the PMOS pair is in PARALLEL (either input low pulls the output up), the NMOS pair in SERIES (both must be high to pull it down)");
    return 20;
}

// 8. Transmission gate against a lone NMOS pass transistor
static int place_cmos_tgate(Circuit *circuit, float x, float y) {
    Component *vdd = dc_rail(circuit, x - 300, y - 200, 5.0); if (!vdd) return 0;
    int rail = TN(x - 300, y - 200);
    Component *vin = add_comp(circuit, COMP_TRIANGLE_WAVE, x - 300, y + 60, 0);
    vin->props.triangle_wave.amplitude = 2.5; vin->props.triangle_wave.offset = 2.5;
    vin->props.triangle_wave.frequency = 1000.0;
    Component *gi = add_comp(circuit, COMP_GROUND, x - 300, y + 140, 0);
    int sp = TN(x - 300, y + 20); vin->node_ids[0] = sp;
    connect_terminals(circuit, vin, 1, gi, 0);
    int sig = TN(x - 200, y + 20); TW(sp, sig);
    // -- transmission gate: NMOS gate at Vdd, PMOS gate at ground, in parallel
    Component *mn = add_comp(circuit, COMP_NMOS, x, y, 90);                          // rot 90: G(0,-20) D(-20,20) S(20,20)
    Component *mp = add_comp(circuit, COMP_PMOS, x, y + 120, 90);
    mn->props.mosfet.kp = 0.01; mp->props.mosfet.vth = -1.0; mp->props.mosfet.kp = 0.005;
    mn->props.mosfet.ideal = mp->props.mosfet.ideal = true;
    int ns = TN(x - 20, y + 20), nd = TN(x + 20, y + 20), ng = TN(x, y - 20);   // rot 90: S left, D right
    int ps = TN(x - 20, y + 140), pd = TN(x + 20, y + 140), pg = TN(x, y + 100);
    TW(sig, TN(x - 60, y + 20)); TW(TN(x - 60, y + 20), ns);
    TW(TN(x - 60, y + 20), TN(x - 60, y + 140)); TW(TN(x - 60, y + 140), ps);
    int tout = TN(x + 80, y + 20); TW(nd, tout); TW(pd, TN(x + 80, y + 140)); TW(TN(x + 80, y + 140), tout);
    TW(ng, TN(x, y - 200)); TW(TN(x, y - 200), rail);                                // NMOS gate to Vdd
    Component *gpg = add_comp(circuit, COMP_GROUND, x, y + 180, 0);
    gpg->node_ids[0] = pg;                                                           // PMOS gate to ground
    mn->node_ids[0] = ng; mn->node_ids[1] = nd; mn->node_ids[2] = ns;
    mp->node_ids[0] = pg; mp->node_ids[1] = pd; mp->node_ids[2] = ps;
    Component *rl1 = add_comp(circuit, COMP_RESISTOR, x + 140, y + 60, 90);
    rl1->props.resistor.resistance = 100e3;
    Component *gl1 = add_comp(circuit, COMP_GROUND, x + 140, y + 120, 0);
    int l1t = TN(x + 140, y + 20), l1b = TN(x + 140, y + 100); TW(tout, l1t);
    rl1->node_ids[0] = l1t; rl1->node_ids[1] = l1b; gl1->node_ids[0] = l1b;
    Component *cg1 = add_comp(circuit, COMP_CAPACITOR, x + 220, y + 60, 90);        // (220,20)-(220,100)
    cg1->props.capacitor.capacitance = 20e-12;
    Component *ggc1 = add_comp(circuit, COMP_GROUND, x + 220, y + 140, 0);
    int cc1t = TN(x + 220, y + 20), cc1b = TN(x + 220, y + 100), cc1g = TN(x + 220, y + 120);
    TW(tout, cc1t); TW(cc1b, cc1g);
    cg1->node_ids[0] = cc1t; cg1->node_ids[1] = cc1b; ggc1->node_ids[0] = cc1g;
    // -- lone NMOS pass transistor for comparison
    Component *mn2 = add_comp(circuit, COMP_NMOS, x, y + 320, 90);
    mn2->props.mosfet.kp = 0.01; mn2->props.mosfet.ideal = true;
    int n2s = TN(x - 20, y + 340), n2d = TN(x + 20, y + 340), n2g = TN(x, y + 300);
    TW(TN(x - 60, y + 140), TN(x - 60, y + 340)); TW(TN(x - 60, y + 340), n2s);
    TW(n2g, TN(x - 120, y + 300)); TW(TN(x - 120, y + 300), TN(x - 120, y - 200)); TW(TN(x - 120, y - 200), rail);
    mn2->node_ids[0] = n2g; mn2->node_ids[1] = n2d; mn2->node_ids[2] = n2s;
    Component *rl2 = add_comp(circuit, COMP_RESISTOR, x + 140, y + 380, 90);
    rl2->props.resistor.resistance = 100e3;
    Component *gl2 = add_comp(circuit, COMP_GROUND, x + 140, y + 440, 0);
    int l2t = TN(x + 140, y + 340), l2b = TN(x + 140, y + 420); TW(n2d, TN(x + 80, y + 340)); TW(TN(x + 80, y + 340), l2t);
    rl2->node_ids[0] = l2t; rl2->node_ids[1] = l2b; gl2->node_ids[0] = l2b;
    Component *cg2 = add_comp(circuit, COMP_CAPACITOR, x + 220, y + 380, 90);       // (220,340)-(220,420)
    cg2->props.capacitor.capacitance = 20e-12;
    Component *ggc2 = add_comp(circuit, COMP_GROUND, x + 220, y + 460, 0);
    int cc2t = TN(x + 220, y + 340), cc2b = TN(x + 220, y + 420), cc2g = TN(x + 220, y + 440);
    TW(l2t, cc2t); TW(cc2b, cc2g);
    cg2->node_ids[0] = cc2t; cg2->node_ids[1] = cc2b; ggc2->node_ids[0] = cc2g;
    add_label(circuit, x - 300, y - 280, "TRANSMISSION GATE vs a LONE NMOS: both pass the same 0-5 V ramp");
    add_label(circuit, x - 300, y + 500, "The complementary pair passes the whole rail; the single NMOS stops one threshold below Vdd - that is why CMOS switches come in pairs.");
    return 18;
}
#undef TN
#undef TW

// ---------------------------------------------------------------------------------------
// X-Y mode: Lissajous figures from two sources, and a plotter that replays an uploaded
// table of coordinates through two arbitrary-waveform sources.
// ---------------------------------------------------------------------------------------
#define TN(cx, cy) circuit_find_or_create_node(circuit, (cx), (cy), 5.0f)
#define TW(a_, b_) circuit_add_wire(circuit, (a_), (b_))

// a source driving its own load, probed at the load: one scope channel
static Component *xy_channel(Circuit *circuit, float x, float y, ComponentType t) {
    Component *v = add_comp(circuit, t, x, y + 60, 0);                    // +(x,y+20) -(x,y+100)
    if (!v) return NULL;
    Component *g = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    connect_terminals(circuit, v, 1, g, 0);
    Component *r = add_comp(circuit, COMP_RESISTOR, x + 140, y + 60, 90); // (140,20)-(140,100)
    r->props.resistor.resistance = 10e3;
    Component *g2 = add_comp(circuit, COMP_GROUND, x + 140, y + 160, 0);
    int sp = TN(x, y + 20), rt = TN(x + 140, y + 20), rb = TN(x + 140, y + 100), gt = TN(x + 140, y + 140);
    TW(sp, rt); TW(rb, gt);
    v->node_ids[0] = sp; r->node_ids[0] = rt; r->node_ids[1] = rb; g2->node_ids[0] = gt;
    return v;
}

// 1. Lissajous: two sine sources whose ratio and phase you edit
static int place_xy_lissajous(Circuit *circuit, float x, float y) {
    Component *vx = xy_channel(circuit, x, y, COMP_AC_VOLTAGE); if (!vx) return 0;
    vx->props.ac_voltage.amplitude = 5.0; vx->props.ac_voltage.frequency = 1000.0;
    Component *vy = xy_channel(circuit, x, y + 260, COMP_AC_VOLTAGE);
    vy->props.ac_voltage.amplitude = 5.0; vy->props.ac_voltage.frequency = 2000.0;
    vy->props.ac_voltage.phase = 90.0;
    add_label(circuit, x - 40, y - 60, "LISSAJOUS: CH1 is 1 kHz, CH2 is 2 kHz at 90 deg. Press the scope's Y-T button for X-Y.");
    add_label(circuit, x - 40, y + 460, "The figure counts the ratio: horizontal tangents / vertical tangents = f_y / f_x. 1:1 gives a line, circle or");
    add_label(circuit, x - 40, y + 490, "ellipse depending on phase; 1:2 a figure of eight; 2:3 the classic pretzel. Edit CH2's frequency and phase and watch.");
    return 10;
}

// 2. X-Y plotter: two arbitrary sources replaying an uploaded coordinate table
static int place_xy_plotter(Circuit *circuit, float x, float y) {
    Component *vx = xy_channel(circuit, x, y, COMP_ARB_SOURCE); if (!vx) return 0;
    vx->props.arb_source.table = 0; vx->props.arb_source.period = 0.01; vx->props.arb_source.amplitude = 5.0;
    Component *vy = xy_channel(circuit, x, y + 260, COMP_ARB_SOURCE);
    vy->props.arb_source.table = 1; vy->props.arb_source.period = 0.01; vy->props.arb_source.amplitude = 5.0;
    add_label(circuit, x - 40, y - 60, "X-Y PLOTTER: two arbitrary-waveform sources replay a table of coordinates. Press Y-T for X-Y mode.");
    add_label(circuit, x - 40, y + 460, "Load your own shape with  circuit-playground.exe --xy points.txt  - a text file of 'x y' pairs, one per line");
    add_label(circuit, x - 40, y + 490, "(commas or semicolons work too, # comments are ignored). Both axes are normalised to -1..1 and scaled by the");
    add_label(circuit, x - 40, y + 520, "source amplitude, so any units plot sensibly. Without a file both sources hold the built-in demo outline.");
    return 10;
}
#undef TN
#undef TW

// ---------------------------------------------------------------------------------------
// Hardware engineering: switching converters, power delivery, signal integrity and loop
// stability. The converters use an ideal switch (COMP_ANALOG_SWITCH driven by a PWM pulse)
// the way a textbook does, so the waveforms are the converter's, not a gate driver's.
// ---------------------------------------------------------------------------------------
#define TN(cx, cy) circuit_find_or_create_node(circuit, (cx), (cy), 5.0f)
#define TW(a_, b_) circuit_add_wire(circuit, (a_), (b_))

// PWM-driven ideal switch between (x-40,y) and (x+40,y); duty 0..1 at f_sw
static Component *pwm_switch(Circuit *circuit, float x, float y, double fsw, double duty) {
    Component *sw = add_comp(circuit, COMP_ANALOG_SWITCH, x, y, 0);       // IN(x-40,y) OUT(x+40,y) CTL(x,y+20)
    if (!sw) return NULL;
    sw->props.analog_switch.r_on = 0.05; sw->props.analog_switch.r_off = 1e9;
    sw->props.analog_switch.v_on = 2.5; sw->props.analog_switch.ideal = false;
    Component *p = add_comp(circuit, COMP_PULSE_SOURCE, x, y + 100, 0);   // +(x,y+60) -(x,y+140)
    p->props.pulse_source.v_low = 0; p->props.pulse_source.v_high = 5.0;
    p->props.pulse_source.period = 1.0 / fsw; p->props.pulse_source.pulse_width = duty / fsw;
    p->props.pulse_source.rise_time = p->props.pulse_source.fall_time = 0.002 / fsw;
    Component *g = add_comp(circuit, COMP_GROUND, x, y + 180, 0);
    int ctl = TN(x, y + 20), pp = TN(x, y + 60);
    TW(ctl, pp);
    sw->node_ids[2] = ctl; p->node_ids[0] = pp;
    connect_terminals(circuit, p, 1, g, 0);
    return sw;
}
// vertical inductor at (x,y): terminals (x,y-40),(x,y+40)
static Component *vind(Circuit *circuit, float x, float y, double l) {
    Component *c = add_comp(circuit, COMP_INDUCTOR, x, y, 90); c->props.inductor.inductance = l;
    c->props.inductor.dcr = 0.2; c->props.inductor.ideal = false;    /* real windings lose a little: without it an L-C loop rings for ever */
    return c;
}
static Component *hind(Circuit *circuit, float x, float y, double l) {
    Component *c = add_comp(circuit, COMP_INDUCTOR, x, y, 0); c->props.inductor.inductance = l;
    c->props.inductor.dcr = 0.2; c->props.inductor.ideal = false;
    return c;
}
// output filter cap + load from node `n` at (x,y) to ground; returns the load component
static Component *out_stage(Circuit *circuit, float x, float y, int n, double c_out, double r_load, double v0) {
    Component *c = add_comp(circuit, COMP_CAPACITOR, x, y + 60, 90);      // (x,y+20)-(x,y+100)
    c->props.capacitor.capacitance = c_out;
    c->props.capacitor.voltage = v0;   /* start near steady state: a cold start rings for many ms */
    Component *gc = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    int ct = TN(x, y + 20), cb = TN(x, y + 100), cg = TN(x, y + 120);
    TW(n, ct); TW(cb, cg);
    c->node_ids[0] = ct; c->node_ids[1] = cb; gc->node_ids[0] = cg;
    Component *r = add_comp(circuit, COMP_RESISTOR, x + 100, y + 60, 90); // (x+100,y+20)-(x+100,y+100)
    r->props.resistor.resistance = r_load;
    Component *gr = add_comp(circuit, COMP_GROUND, x + 100, y + 140, 0);
    int rt = TN(x + 100, y + 20), rb = TN(x + 100, y + 100), rg = TN(x + 100, y + 120);
    TW(ct, rt); TW(rb, rg);
    r->node_ids[0] = rt; r->node_ids[1] = rb; gr->node_ids[0] = rg;
    return r;
}

// 1. Buck: switch chops Vin, the diode freewheels, L-C averages. Vout = D Vin
static int place_hw_buck(Circuit *circuit, float x, float y) {
    Component *vin = dc_rail(circuit, x, y, 12.0); if (!vin) return 0;      // +(x,y)
    int in = TN(x, y);
    Component *sw = pwm_switch(circuit, x + 140, y, 100e3, 0.5);
    int si = TN(x + 100, y), so = TN(x + 180, y); TW(in, si);
    sw->node_ids[0] = si; sw->node_ids[1] = so;
    int node_sw = TN(x + 240, y); TW(so, node_sw);
    Component *d = add_comp(circuit, COMP_DIODE, x + 240, y + 80, 90);     // A(240,40) K(240,120) -> cathode up
    int da = TN(x + 240, y + 40), dk = TN(x + 240, y + 120);
    Component *gd = add_comp(circuit, COMP_GROUND, x + 240, y + 160, 0);
    TW(node_sw, da);
    d->node_ids[0] = dk; d->node_ids[1] = da;                              // K to the switch node, A to ground
    gd->node_ids[0] = dk;
    Component *l = hind(circuit, x + 340, y, 22e-6);                       // (300,y)-(380,y)
    int ll = TN(x + 300, y), lr = TN(x + 380, y); TW(node_sw, ll);
    l->node_ids[0] = ll; l->node_ids[1] = lr;
    int out = TN(x + 440, y); TW(lr, out);
    out_stage(circuit, x + 440, y, out, 47e-6, 6.0, 6.0);
    add_label(circuit, x - 40, y - 80, "BUCK CONVERTER: Vout = D x Vin = 0.5 x 12 = 6 V at 100 kHz. The switch chops, the diode freewheels, L and C average");
    add_label(circuit, x - 40, y + 220, "Inductor ripple dI = (Vin-Vout) D / (L fsw) = 300 mA; output ripple dV = dI / (8 C fsw) = 3.8 mV.");
    add_label(circuit, x - 40, y + 250, "TRY: duty 0.5 -> 0.25 halves Vout. Drop L to 10 uH and the inductor current goes discontinuous.");
    return 16;
}

// 2. Boost: Vout = Vin / (1 - D)
static int place_hw_boost(Circuit *circuit, float x, float y) {
    Component *vin = dc_rail(circuit, x, y, 5.0); if (!vin) return 0;
    int in = TN(x, y);
    Component *l = hind(circuit, x + 100, y, 22e-6);                       // (60,y)-(140,y)
    int ll = TN(x + 60, y), lr = TN(x + 140, y); TW(in, ll);
    l->node_ids[0] = ll; l->node_ids[1] = lr;
    int node_sw = TN(x + 200, y); TW(lr, node_sw);
    Component *sw = pwm_switch(circuit, x + 200, y + 80, 100e3, 0.5);      // IN(160,80) OUT(240,80)
    int si = TN(x + 160, y + 80), so = TN(x + 240, y + 80);
    TW(node_sw, TN(x + 160, y)); TW(TN(x + 160, y), si);
    Component *gs = add_comp(circuit, COMP_GROUND, x + 280, y + 100, 0);
    TW(so, TN(x + 280, y + 80)); gs->node_ids[0] = TN(x + 280, y + 80);
    sw->node_ids[0] = si; sw->node_ids[1] = so;
    Component *d = add_comp(circuit, COMP_DIODE, x + 320, y, 0);           // A(280,y) K(360,y)
    int da = TN(x + 280, y), dk = TN(x + 360, y); TW(node_sw, da);
    d->node_ids[0] = da; d->node_ids[1] = dk;
    int out = TN(x + 420, y); TW(dk, out);
    out_stage(circuit, x + 420, y, out, 47e-6, 20.0, 10.0);
    add_label(circuit, x - 40, y - 80, "BOOST CONVERTER: Vout = Vin / (1 - D) = 5 / 0.5 = 10 V. The switch charges L from the input, then L dumps into the output through the diode");
    add_label(circuit, x - 40, y + 320, "The output can only ever be ABOVE the input, and the input current is continuous while the output current is not -");
    add_label(circuit, x - 40, y + 350, "which is why a boost needs far more output capacitance than a buck. TRY: duty 0.5 -> 0.75 gives 20 V.");
    return 16;
}

// 3. Buck-boost: Vout = -D/(1-D) Vin
static int place_hw_buckboost(Circuit *circuit, float x, float y) {
    Component *vin = dc_rail(circuit, x, y, 12.0); if (!vin) return 0;
    int in = TN(x, y);
    Component *sw = pwm_switch(circuit, x + 140, y, 100e3, 0.5);
    int si = TN(x + 100, y), so = TN(x + 180, y); TW(in, si);
    sw->node_ids[0] = si; sw->node_ids[1] = so;
    int node_sw = TN(x + 240, y); TW(so, node_sw);
    Component *l = add_comp(circuit, COMP_INDUCTOR, x + 240, y + 80, 90);  // (240,40)-(240,120)
    l->props.inductor.inductance = 22e-6;
    int lt = TN(x + 240, y + 40), lb = TN(x + 240, y + 120);
    Component *gl = add_comp(circuit, COMP_GROUND, x + 240, y + 160, 0);
    TW(node_sw, lt); TW(lb, TN(x + 240, y + 140)); gl->node_ids[0] = TN(x + 240, y + 140);
    l->node_ids[0] = lt; l->node_ids[1] = lb;
    Component *d = add_comp(circuit, COMP_DIODE, x + 340, y, 180);         // rotated: K(300,y) A(380,y)
    int dk = TN(x + 300, y), da = TN(x + 380, y); TW(node_sw, dk);
    d->node_ids[0] = da; d->node_ids[1] = dk;
    int out = TN(x + 440, y); TW(da, out);
    out_stage(circuit, x + 440, y, out, 47e-6, 20.0, -12.0);
    add_label(circuit, x - 40, y - 80, "BUCK-BOOST: Vout = -D/(1-D) x Vin = -12 V at D = 0.5. The output is INVERTED, and can be above or below the input");
    add_label(circuit, x - 40, y + 220, "The inductor is the only energy path: it charges from the input, then discharges into the output with the opposite");
    add_label(circuit, x - 40, y + 250, "polarity. Neither the input nor the output current is continuous. TRY: D 0.5 -> 0.67 gives -24 V.");
    return 16;
}

// 4. Cuk: capacitive energy transfer, both currents continuous, inverted output
static int place_hw_cuk(Circuit *circuit, float x, float y) {
    Component *vin = dc_rail(circuit, x, y, 12.0); if (!vin) return 0;
    int in = TN(x, y);
    Component *l1 = hind(circuit, x + 100, y, 47e-6);
    l1->props.inductor.dcr = 0.1; l1->props.inductor.ideal = false;
    int l1l = TN(x + 60, y), l1r = TN(x + 140, y); TW(in, l1l);
    l1->node_ids[0] = l1l; l1->node_ids[1] = l1r;
    int node_a = TN(x + 200, y); TW(l1r, node_a);
    Component *sw = pwm_switch(circuit, x + 200, y + 80, 100e3, 0.5);
    int si = TN(x + 160, y + 80), so = TN(x + 240, y + 80);
    TW(node_a, TN(x + 160, y)); TW(TN(x + 160, y), si);
    Component *gs = add_comp(circuit, COMP_GROUND, x + 280, y + 100, 0);
    TW(so, TN(x + 280, y + 80)); gs->node_ids[0] = TN(x + 280, y + 80);
    sw->node_ids[0] = si; sw->node_ids[1] = so;
    Component *c1 = add_comp(circuit, COMP_CAPACITOR, x + 300, y, 0);      // (260,y)-(340,y) transfer cap
    c1->props.capacitor.capacitance = 220e-6;
    /* No initial condition on the transfer capacitor. Its DC level is a slow free integrator
       here - nothing in the loop forces the volt-second balance quickly - so pre-charging it to
       the theoretical Vin + |Vout| = 24 V does not settle it faster, it settles it somewhere
       else (15.5 V out instead of 13.3). See docs/ROADMAP.md; the output capacitors of the
       other converters do take their initial condition, and it changes only their startup. */
    int c1l = TN(x + 260, y), c1r = TN(x + 340, y); TW(node_a, c1l);
    c1->node_ids[0] = c1l; c1->node_ids[1] = c1r;
    Component *resr = hres(circuit, x + 400, y, 0.2);                      // (360,y)-(440,y): the cap's ESR
    int esl = TN(x + 360, y), esr2 = TN(x + 440, y); TW(c1r, esl);
    resr->node_ids[0] = esl; resr->node_ids[1] = esr2;
    int node_b = TN(x + 500, y); TW(esr2, node_b);
    Component *d = add_comp(circuit, COMP_DIODE, x + 500, y + 80, 90);     // rot 90: A(500,40) K(500,120)
    int da2 = TN(x + 500, y + 40), dk2 = TN(x + 500, y + 120);
    Component *gd = add_comp(circuit, COMP_GROUND, x + 500, y + 160, 0);
    TW(node_b, da2); TW(dk2, TN(x + 500, y + 140)); gd->node_ids[0] = TN(x + 500, y + 140);
    d->node_ids[0] = da2; d->node_ids[1] = dk2;   // anode on the switch node, cathode to ground
    Component *l2 = hind(circuit, x + 600, y, 47e-6);
    l2->props.inductor.dcr = 0.1; l2->props.inductor.ideal = false;
    int l2l = TN(x + 560, y), l2r = TN(x + 640, y); TW(node_b, l2l);
    l2->node_ids[0] = l2l; l2->node_ids[1] = l2r;
    int out = TN(x + 700, y); TW(l2r, out);
    out_stage(circuit, x + 700, y, out, 100e-6, 20.0, -12.0);   /* 2 ms of output filter: the converter settles inside the visible window */
    add_label(circuit, x - 40, y - 80, "CUK CONVERTER: energy moves through the 47 uF transfer capacitor instead of an inductor, so BOTH the input and");
    add_label(circuit, x - 40, y - 50, "output currents are continuous - the quietest of the basic topologies. Vout = -D/(1-D) x Vin = -12 V at D = 0.5.");
    add_label(circuit, x - 40, y + 300, "The 0.5 ohm with C1 is its ESR; with no real loss the C1-L2 loop rings away at start-up. TRY: C1 -> 1 uF.");
    return 22;
}

// 5. Two-phase interleaved buck: the ripple the CLVR idea is built on
static int place_hw_interleaved(Circuit *circuit, float x, float y) {
    Component *vin = dc_rail(circuit, x, y - 120, 12.0); if (!vin) return 0;
    int in = TN(x, y - 120);
    int out = TN(x + 520, y);
    for (int k = 0; k < 2; k++) {
        float py = y + k * 220;
        Component *sw = pwm_switch(circuit, x + 140, py, 100e3, 0.5);
        int si = TN(x + 100, py), so = TN(x + 180, py);
        TW(in, TN(x + 100, y - 120)); TW(TN(x + 100, y - 120), TN(x + 100, py));
        sw->node_ids[0] = si; sw->node_ids[1] = so;
        if (k) {   /* phase B runs 180 degrees later */
            Component *p = NULL;
            for (int i = circuit->num_components - 1; i >= 0 && !p; i--)
                if (circuit->components[i]->type == COMP_PULSE_SOURCE) p = circuit->components[i];
            if (p) p->props.pulse_source.delay = 5e-6;
        }
        int nsw = TN(x + 240, py); TW(so, nsw);
        Component *d = add_comp(circuit, COMP_DIODE, x + 240, py + 80, 90);
        int da = TN(x + 240, py + 40), dk = TN(x + 240, py + 120);
        Component *gd = add_comp(circuit, COMP_GROUND, x + 240, py + 160, 0);
        TW(nsw, da);
        d->node_ids[0] = dk; d->node_ids[1] = da; gd->node_ids[0] = dk;
        Component *l = hind(circuit, x + 340, py, 22e-6);
        int ll = TN(x + 300, py), lr = TN(x + 380, py); TW(nsw, ll);
        l->node_ids[0] = ll; l->node_ids[1] = lr;
        TW(lr, TN(x + 440, py)); TW(TN(x + 440, py), TN(x + 440, y)); TW(TN(x + 440, y), out);
    }
    out_stage(circuit, x + 520, y, out, 47e-6, 3.0, 6.0);
    add_label(circuit, x - 40, y - 200, "TWO-PHASE INTERLEAVED BUCK: two 100 kHz buck stages 180 degrees out of phase into one output");
    add_label(circuit, x - 40, y + 460, "Each phase carries half the current, and their ripples partly cancel, so the output ripple is far smaller than one");
    add_label(circuit, x - 40, y + 490, "phase alone would give at 200 kHz effective. Coupling the two inductors (a CLVR) cancels more still and shrinks them.");
    add_label(circuit, x - 40, y + 520, "TRY: set phase B's pulse delay to 0 so both switch together - the ripple doubles.");
    return 26;
}

// 6. Power delivery network: bulk, ceramic and plane inductance against a load step
static int place_hw_pdn(Circuit *circuit, float x, float y) {
    Component *vin = dc_rail(circuit, x, y, 1.8); if (!vin) return 0;
    int in = TN(x, y);
    Component *rp = hres(circuit, x + 100, y, 0.02);                       // regulator + plane resistance
    int rl = TN(x + 60, y), rr = TN(x + 140, y); TW(in, rl);
    rp->node_ids[0] = rl; rp->node_ids[1] = rr;
    Component *lp = hind(circuit, x + 200, y, 2e-9);                       // 2 nH of plane/via inductance
    int lpl = TN(x + 160, y), lpr = TN(x + 240, y); TW(rr, lpl);
    lp->node_ids[0] = lpl; lp->node_ids[1] = lpr;
    int rail = TN(x + 300, y); TW(lpr, rail);
    /* bulk 100 uF with 20 mOhm ESR, then a 1 uF ceramic with 5 mOhm, right at the die */
    static const double cval[2] = { 100e-6, 1e-6 };
    static const double esr[2]  = { 0.02, 0.005 };
    for (int k = 0; k < 2; k++) {
        float px = x + 300 + k * 120;
        Component *c = add_comp(circuit, COMP_CAPACITOR, px, y + 80, 90);   // (px,y+40)-(px,y+120)
        c->props.capacitor.capacitance = cval[k];
        Component *r = add_comp(circuit, COMP_RESISTOR, px, y + 180, 90);   // (px,y+140)-(px,y+220)
        r->props.resistor.resistance = esr[k];
        Component *g = add_comp(circuit, COMP_GROUND, px, y + 260, 0);
        int ct = TN(px, y + 40), cb = TN(px, y + 120), rt = TN(px, y + 140), rb = TN(px, y + 220), gt = TN(px, y + 240);
        int tap = TN(px, y); TW(rail, tap); TW(tap, ct); TW(cb, rt); TW(rb, gt);
        c->node_ids[0] = ct; c->node_ids[1] = cb; r->node_ids[0] = rt; r->node_ids[1] = rb; g->node_ids[0] = gt;
    }
    /* the load: a steady 0.9 A plus a switched 0.9 A step */
    Component *rdc = add_comp(circuit, COMP_RESISTOR, x + 560, y + 80, 90);
    rdc->props.resistor.resistance = 2.0;
    Component *gdc = add_comp(circuit, COMP_GROUND, x + 560, y + 160, 0);
    int dt = TN(x + 560, y + 40), db = TN(x + 560, y + 120), dg = TN(x + 560, y + 140);
    int dtap = TN(x + 560, y); TW(rail, dtap); TW(dtap, dt); TW(db, dg);
    rdc->node_ids[0] = dt; rdc->node_ids[1] = db; gdc->node_ids[0] = dg;
    Component *sw = fault_switch(circuit, x + 660, y, 20e-6, 20e-6, 80e-6);  // IN(620,y) OUT(700,y)
    TW(rail, TN(x + 620, y)); sw->node_ids[0] = TN(x + 620, y);
    Component *rstep = add_comp(circuit, COMP_RESISTOR, x + 740, y + 80, 90);
    rstep->props.resistor.resistance = 2.0;
    Component *gst = add_comp(circuit, COMP_GROUND, x + 740, y + 160, 0);
    int st = TN(x + 740, y + 40), sb = TN(x + 740, y + 120), sg = TN(x + 740, y + 140);
    TW(TN(x + 700, y), TN(x + 740, y)); TW(TN(x + 740, y), st); TW(sb, sg);
    sw->node_ids[1] = TN(x + 700, y);
    rstep->node_ids[0] = st; rstep->node_ids[1] = sb; gst->node_ids[0] = sg;
    add_label(circuit, x - 40, y - 80, "POWER DELIVERY NETWORK: a 1.8 V rail through 20 mOhm and 2 nH of plane into 100 uF bulk (20 mOhm ESR)");
    add_label(circuit, x - 40, y - 50, "and 1 uF of ceramic (5 mOhm) at the die. A 0.9 A load step every 80 us shows what the rail actually does.");
    add_label(circuit, x - 40, y + 320, "The first nanoseconds are the ceramic's job (the plane inductance blocks everything else), then the bulk takes over,");
    add_label(circuit, x - 40, y + 350, "and finally the regulator. TRY: delete the ceramic and the step edge gets a sharp spike; raise the plane L to 20 nH.");
    return 22;
}

// 7. Input vs output capacitance: the same 1 A step with the cap in each place
static int place_hw_caps(Circuit *circuit, float x, float y) {
    for (int k = 0; k < 2; k++) {
        float py = y + k * 300;
        Component *v = dc_rail(circuit, x, py, 5.0); if (!v) return 0;
        int in = TN(x, py);
        Component *r = hres(circuit, x + 100, py, 0.5);                    // source/wiring impedance
        int rl = TN(x + 60, py), rr = TN(x + 140, py); TW(in, rl);
        r->node_ids[0] = rl; r->node_ids[1] = rr;
        Component *l = hind(circuit, x + 200, py, 1e-6);                   // 1 uH of lead inductance
        int ll = TN(x + 160, py), lr = TN(x + 240, py); TW(rr, ll);
        l->node_ids[0] = ll; l->node_ids[1] = lr;
        int rail = TN(x + 300, py); TW(lr, rail);
        float cx = k ? x + 300 : x + 60;                                   /* output side vs input side */
        Component *c = add_comp(circuit, COMP_CAPACITOR, cx, py + 80, 90);
        c->props.capacitor.capacitance = 100e-6;
        Component *g = add_comp(circuit, COMP_GROUND, cx, py + 160, 0);
        int ct = TN(cx, py + 40), cb = TN(cx, py + 120), gt = TN(cx, py + 140);
        TW(k ? rail : rl, ct); TW(cb, gt);
        c->node_ids[0] = ct; c->node_ids[1] = cb; g->node_ids[0] = gt;
        Component *sw = fault_switch(circuit, x + 400, py, 50e-6, 50e-6, 200e-6);
        TW(rail, TN(x + 360, py)); sw->node_ids[0] = TN(x + 360, py);
        Component *rs = add_comp(circuit, COMP_RESISTOR, x + 480, py + 80, 90);
        rs->props.resistor.resistance = 5.0;
        Component *gs = add_comp(circuit, COMP_GROUND, x + 480, py + 160, 0);
        int st = TN(x + 480, py + 40), sb = TN(x + 480, py + 120), sg = TN(x + 480, py + 140);
        TW(TN(x + 440, py), TN(x + 480, py)); TW(TN(x + 480, py), st); TW(sb, sg);
        sw->node_ids[1] = TN(x + 440, py);
        rs->node_ids[0] = st; rs->node_ids[1] = sb; gs->node_ids[0] = sg;
        add_label(circuit, x - 40, py - 50, k ? "OUTPUT capacitance: the 100 uF sits at the load, after the 1 uH lead"
                                              : "INPUT capacitance: the same 100 uF sits at the source, before the lead");
    }
    add_label(circuit, x - 40, y - 110, "INPUT vs OUTPUT CAPACITANCE: identical rails, identical 1 A load step, the only difference is which side of the");
    add_label(circuit, x - 40, y + 520, "lead inductance the 100 uF sits on. At the source it can do nothing about the step - the 1 uH is between it and the");
    add_label(circuit, x - 40, y + 550, "load. At the load it holds the rail up. This is why decoupling goes AT the part, and why input caps are for the source.");
    return 26;
}

// 8. Impedance matching: a source into three loads, one of them matched
static int place_hw_match(Circuit *circuit, float x, float y) {
    static const double rl[3] = { 5.0, 50.0, 500.0 };
    static const char *nm[3] = { "R_L = 5 ohm (too low)", "R_L = 50 ohm (matched)", "R_L = 500 ohm (too high)" };
    for (int k = 0; k < 3; k++) {
        float py = y + k * 220;
        Component *v = add_comp(circuit, COMP_AC_VOLTAGE, x, py + 60, 0);   // +(x,py+20)
        if (!v) return 0;
        v->props.ac_voltage.amplitude = 2.0; v->props.ac_voltage.frequency = 1e6;
        Component *g = add_comp(circuit, COMP_GROUND, x, py + 140, 0);
        connect_terminals(circuit, v, 1, g, 0);
        int sp = TN(x, py + 20);
        v->node_ids[0] = sp;
        Component *rs = hres(circuit, x + 140, py + 20, 50.0);              // source resistance
        int sl = TN(x + 100, py + 20), sr = TN(x + 180, py + 20); TW(sp, sl);
        rs->node_ids[0] = sl; rs->node_ids[1] = sr;
        int node = TN(x + 240, py + 20); TW(sr, node);
        Component *r = add_comp(circuit, COMP_RESISTOR, x + 240, py + 80, 90);
        r->props.resistor.resistance = rl[k];
        Component *g2 = add_comp(circuit, COMP_GROUND, x + 240, py + 160, 0);
        int rt = TN(x + 240, py + 40), rb = TN(x + 240, py + 120), gt = TN(x + 240, py + 140);
        TW(node, rt); TW(rb, gt);
        r->node_ids[0] = rt; r->node_ids[1] = rb; g2->node_ids[0] = gt;
        add_label(circuit, x + 300, py + 20, nm[k]);
    }
    add_label(circuit, x - 40, y - 60, "IMPEDANCE MATCHING: the same 2 Vpk source behind 50 ohm feeding 5, 50 and 500 ohm");
    add_label(circuit, x - 40, y + 680, "Power in the load is V^2/R after the divider: 0.033 W, 0.020 W, 0.0033 W - the MATCHED load takes the most power");
    add_label(circuit, x - 40, y + 710, "even though it does not have the highest voltage across it. Maximum power transfer is R_L = R_S, not R_L as large as");
    add_label(circuit, x - 40, y + 740, "possible (that maximises voltage) nor as small as possible (that maximises current).");
    return 22;
}

// 9. Reflections on an artificial (lumped LC) line with a switchable termination
static int place_hw_reflect(Circuit *circuit, float x, float y) {
    Component *src = add_comp(circuit, COMP_PULSE_SOURCE, x, y + 60, 0);    // +(x,y+20) -(x,y+100)
    if (!src) return 0;
    src->props.pulse_source.v_low = 0; src->props.pulse_source.v_high = 5.0;
    src->props.pulse_source.period = 4e-6; src->props.pulse_source.pulse_width = 2e-6;
    src->props.pulse_source.rise_time = src->props.pulse_source.fall_time = 20e-9;
    Component *g = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    int sp = TN(x, y + 20); src->node_ids[0] = sp;
    connect_terminals(circuit, src, 1, g, 0);
    Component *rs = hres(circuit, x + 100, y + 20, 50.0);                   // source termination
    int sl = TN(x + 60, y + 20), sr = TN(x + 140, y + 20); TW(sp, sl);
    rs->node_ids[0] = sl; rs->node_ids[1] = sr;
    /* One 50 ohm, 400 ns line. This used to be eight L-C sections making an artificial one;
       the Delay Line carries the delay in its history instead, so the far end sits at exactly
       zero for 400 ns and then steps, rather than starting to move with the driver. */
    Component *line = add_comp(circuit, COMP_DELAY_LINE, x + 400, y + 20, 0);   // (360,20)-(440,20)
    line->props.delay_line.z0 = 50.0;
    line->props.delay_line.delay = 400e-9;
    line->props.delay_line.ideal = true;
    int near = TN(x + 360, y + 20), farend = TN(x + 440, y + 20);
    TW(sr, near);
    line->node_ids[0] = near; line->node_ids[1] = farend;
    int n = farend;

    Component *sw = add_comp(circuit, COMP_SPST_SWITCH, x + 560, y + 20, 0);   // termination switch (open = reflect)
    sw->props.switch_spst.closed = false;
    int swl = TN(x + 520, y + 20), swr = TN(x + 600, y + 20); TW(n, swl);
    sw->node_ids[0] = swl; sw->node_ids[1] = swr;
    Component *rt = add_comp(circuit, COMP_RESISTOR, x + 660, y + 80, 90);
    rt->props.resistor.resistance = 50.0;
    Component *gt2 = add_comp(circuit, COMP_GROUND, x + 660, y + 160, 0);
    int tt = TN(x + 660, y + 40), tb = TN(x + 660, y + 120), tg = TN(x + 660, y + 140);
    TW(swr, TN(x + 660, y + 20)); TW(TN(x + 660, y + 20), tt); TW(tb, tg);
    rt->node_ids[0] = tt; rt->node_ids[1] = tb; gt2->node_ids[0] = tg;
    add_label(circuit, x - 40, y - 60, "SIGNAL REFLECTIONS: a 50 ohm line with 400 ns of real propagation delay, driven through 50 ohm");
    add_label(circuit, x - 40, y + 240, "With the termination switch OPEN the far end is unterminated: the edge reflects back with the same sign, and the");
    add_label(circuit, x - 40, y + 270, "source end shows the classic staircase. CLOSE the switch for a matched 50 ohm end and the reflection disappears.");
    add_label(circuit, x - 40, y + 300, "PROBE: the driver end and the far end. TRY: make the termination 10 ohm for a negative (inverted)");
    add_label(circuit, x - 40, y + 330, "reflection, or edit the line: Z0 and the delay are both properties of the part, not of the drawing.");
    return 7;
}

// 10. Loop stability: the same amplifier with and without a compensation capacitor
static int place_hw_loop(Circuit *circuit, float x, float y) {
    for (int k = 0; k < 2; k++) {
        float py = y + k * 320;
        Component *v = add_comp(circuit, COMP_SQUARE_WAVE, x, py + 60, 0);  // +(x,py+20)
        if (!v) return 0;
        v->props.square_wave.amplitude = 0.5; v->props.square_wave.offset = 0.5;
        v->props.square_wave.frequency = 2e3; v->props.square_wave.duty = 0.5;
        Component *g = add_comp(circuit, COMP_GROUND, x, py + 140, 0);
        int sp = TN(x, py + 20); v->node_ids[0] = sp;
        connect_terminals(circuit, v, 1, g, 0);
        Component *u = sat_opamp(circuit, x + 260, py + 20);                // -(220,0) +(220,40) out(300,20)
        Component *rin = hres(circuit, x + 140, py, 10e3);                  // (100,py)-(180,py)
        int il = TN(x + 100, py), ir = TN(x + 180, py), minus = TN(x + 220, py);
        TW(sp, TN(x + 60, py + 20)); TW(TN(x + 60, py + 20), TN(x + 60, py)); TW(TN(x + 60, py), il);
        TW(ir, minus);
        rin->node_ids[0] = il; rin->node_ids[1] = ir;
        Component *rf = hres(circuit, x + 260, py - 100, 100e3);            // (220,py-100)-(300,py-100)
        int fl = TN(x + 220, py - 100), fr = TN(x + 300, py - 100), out = TN(x + 300, py + 20);
        TW(fl, TN(x + 220, py - 60)); TW(TN(x + 220, py - 60), minus);
        TW(fr, TN(x + 340, py - 100)); TW(TN(x + 340, py - 100), TN(x + 340, py + 20)); TW(TN(x + 340, py + 20), out);
        rf->node_ids[0] = fl; rf->node_ids[1] = fr;
        Component *gp = add_comp(circuit, COMP_GROUND, x + 220, py + 100, 0);
        int plus = TN(x + 220, py + 40), pg = TN(x + 220, py + 80);
        TW(plus, pg); gp->node_ids[0] = pg;
        u->node_ids[0] = minus; u->node_ids[1] = plus; u->node_ids[2] = out;
        /* the load the loop has to drive: 1 nF, which adds the extra pole */
        Component *rl2 = hres(circuit, x + 420, py + 20, 1e3);
        int rll = TN(x + 380, py + 20), rlr = TN(x + 460, py + 20); TW(out, rll);
        rl2->node_ids[0] = rll; rl2->node_ids[1] = rlr;
        Component *cl = add_comp(circuit, COMP_CAPACITOR, x + 520, py + 80, 90);
        cl->props.capacitor.capacitance = 1e-9;
        Component *gc = add_comp(circuit, COMP_GROUND, x + 520, py + 160, 0);
        int ct = TN(x + 520, py + 40), cb = TN(x + 520, py + 120), gt = TN(x + 520, py + 140);
        TW(rlr, TN(x + 520, py + 20)); TW(TN(x + 520, py + 20), ct); TW(cb, gt);
        cl->node_ids[0] = ct; cl->node_ids[1] = cb; gc->node_ids[0] = gt;
        if (k) {   /* compensated: a feedback capacitor across Rf adds a zero and lifts the phase margin */
            Component *cf = add_comp(circuit, COMP_CAPACITOR, x + 260, py - 180, 0);
            cf->props.capacitor.capacitance = 100e-12;
            int cfl = TN(x + 220, py - 180), cfr = TN(x + 300, py - 180);
            TW(cfl, fl); TW(cfr, fr);
            cf->node_ids[0] = cfl; cf->node_ids[1] = cfr;
        }
        add_label(circuit, x + 560, py + 20, k ? "compensated: 100 pF across Rf" : "uncompensated: rings on every edge");
    }
    add_label(circuit, x - 40, y - 240, "LOOP STABILITY AND PHASE MARGIN: two identical x10 inverting stages driving 1 nF, one with a feedback capacitor");
    add_label(circuit, x - 40, y + 560, "The load capacitance and the amplifier's own pole put two poles inside the loop, so the phase reaches -180 deg near");
    add_label(circuit, x - 40, y + 590, "the crossover and the step response rings. 100 pF across the feedback resistor adds a zero, pulls the phase back and");
    add_label(circuit, x - 40, y + 620, "the ringing disappears - at the cost of bandwidth. Use the Bode button on each to compare the two loops directly.");
    return 30;
}

/* ======================= Ideal vs real component models =======================
   Each builder places the same circuit two or three times and swaps ONE part for its
   non-ideal model, so both traces sit on the scope together. Every number quoted in the
   notes is a hand calculation that --probe-test checks against the solver. */

// 1. A source is a voltage BEHIND a resistance
static int place_id_source(Circuit *circuit, float x, float y) {
    static const double rl[3] = { 1000.0, 1000.0, 100.0 };
    static const int idl[3] = { 1, 0, 0 };
    static const char *nm[3] = { "IDEAL 5 V, 1k load -> 5.000 V",
                                 "REAL 5 V (r = 200 ohm), 1k load -> 4.167 V",
                                 "REAL 5 V (r = 200 ohm), 100 ohm load -> 1.667 V" };
    for (int k = 0; k < 3; k++) {
        float py = y + k * 220;
        Component *v = add_comp(circuit, COMP_DC_VOLTAGE, x, py + 60, 0);       // +(x,py+20) -(x,py+100)
        if (!v) return 0;
        v->props.dc_voltage.voltage = 5.0;
        v->props.dc_voltage.ideal = idl[k] ? true : false;
        v->props.dc_voltage.r_series = 200.0;
        Component *g = add_comp(circuit, COMP_GROUND, x, py + 140, 0);
        connect_terminals(circuit, v, 1, g, 0);
        int sp = TN(x, py + 20); v->node_ids[0] = sp;
        int node = TN(x + 240, py + 20); TW(sp, node);
        Component *r = add_comp(circuit, COMP_RESISTOR, x + 240, py + 80, 90);  // (240,py+40)-(240,py+120)
        r->props.resistor.resistance = rl[k];
        Component *g2 = add_comp(circuit, COMP_GROUND, x + 240, py + 160, 0);
        int rt = TN(x + 240, py + 40), rb = TN(x + 240, py + 120), gt = TN(x + 240, py + 140);
        TW(node, rt); TW(rb, gt);
        r->node_ids[0] = rt; r->node_ids[1] = rb; g2->node_ids[0] = gt;
        add_label(circuit, x + 300, py + 20, nm[k]);
    }
    add_label(circuit, x - 40, y - 60, "IDEAL vs REAL SOURCE: one 5 V supply three times - ideal, then twice behind its 200 ohm internal resistance");
    add_label(circuit, x - 40, y + 660, "A real supply, battery or signal generator is a voltage BEHIND a resistance, so the LOAD decides the terminal");
    add_label(circuit, x - 40, y + 690, "voltage: 5 x 1000/1200 = 4.167 V into 1k, but only 5 x 100/300 = 1.667 V into 100 ohm. Tick Ideal on the second");
    add_label(circuit, x - 40, y + 720, "source, or set its r_series to 0, and it climbs back to a flat 5.000 V whatever you hang on it.");
    return 12;
}

// 2. The 0.7 V brick wall against the Shockley knee
static int place_id_diode(Circuit *circuit, float x, float y) {
    static const int idl[2] = { 1, 0 };
    static const char *nm[2] = { "IDEAL diode: dead below 0.7 V, then a hard 0.7 V drop -> peak 0.30 V",
                                 "REAL diode (Shockley): soft knee near 0.52 V -> peak ~0.48 V" };
    for (int k = 0; k < 2; k++) {
        float py = y + k * 220;
        Component *v = add_comp(circuit, COMP_AC_VOLTAGE, x, py + 60, 0);
        if (!v) return 0;
        v->props.ac_voltage.amplitude = 1.0; v->props.ac_voltage.frequency = 1000.0;
        Component *g = add_comp(circuit, COMP_GROUND, x, py + 140, 0);
        connect_terminals(circuit, v, 1, g, 0);
        int sp = TN(x, py + 20); v->node_ids[0] = sp;
        Component *d = add_comp(circuit, COMP_DIODE, x + 140, py + 20, 0);      // A(100,py+20) K(180,py+20)
        d->props.diode.ideal = idl[k] ? true : false;
        int da = TN(x + 100, py + 20), dk = TN(x + 180, py + 20); TW(sp, da);
        d->node_ids[0] = da; d->node_ids[1] = dk;
        int node = TN(x + 240, py + 20); TW(dk, node);
        Component *r = add_comp(circuit, COMP_RESISTOR, x + 240, py + 80, 90);
        r->props.resistor.resistance = 1000.0;
        Component *g2 = add_comp(circuit, COMP_GROUND, x + 240, py + 160, 0);
        int rt = TN(x + 240, py + 40), rb = TN(x + 240, py + 120), gt = TN(x + 240, py + 140);
        TW(node, rt); TW(rb, gt);
        r->node_ids[0] = rt; r->node_ids[1] = rb; g2->node_ids[0] = gt;
        add_label(circuit, x + 300, py + 20, nm[k]);
    }
    add_label(circuit, x - 40, y - 60, "IDEAL vs REAL DIODE: the same 1 Vpk half-wave rectifier, one with the switch-plus-battery model, one with Shockley");
    add_label(circuit, x - 40, y + 440, "The ideal model is a switch behind a 0.7 V battery, so a 1 V peak leaves 0.30 V. The real junction passes hundreds");
    add_label(circuit, x - 40, y + 470, "of microamps well below 0.7 V, turns on around 0.52 V and leaves ~0.48 V - 60 % more. Use the ideal model for a");
    add_label(circuit, x - 40, y + 500, "mental estimate; it is wrong exactly where log amps, temperature sensors and small-signal detectors live.");
    return 10;
}

// 3. ESR: the resistance that turns ripple into a square
static int place_id_cap(Circuit *circuit, float x, float y) {
    static const double esr[3] = { 0.0, 0.5, 2.0 };
    static const char *nm[3] = { "IDEAL 5 uF: pure triangle, 250 mVpp",
                                 "REAL 5 uF, ESR = 0.5 ohm: a +/-25 mV square rides on the triangle",
                                 "REAL 5 uF, ESR = 2 ohm: +/-100 mV - now the ESR dominates the ripple" };
    for (int k = 0; k < 3; k++) {
        float py = y + k * 220;
        Component *v = add_comp(circuit, COMP_SQUARE_WAVE, x, py + 60, 0);
        if (!v) return 0;
        v->props.square_wave.amplitude = 5.0; v->props.square_wave.frequency = 20e3;
        v->props.square_wave.duty = 0.5;
        Component *g = add_comp(circuit, COMP_GROUND, x, py + 140, 0);
        connect_terminals(circuit, v, 1, g, 0);
        int sp = TN(x, py + 20); v->node_ids[0] = sp;
        Component *rs = hres(circuit, x + 140, py + 20, 100.0);
        rs->props.resistor.power_rating = 1.0;      /* 10 Vpp across 100 ohm: a real 1 W part */
        int sl = TN(x + 100, py + 20), sr = TN(x + 180, py + 20); TW(sp, sl);
        rs->node_ids[0] = sl; rs->node_ids[1] = sr;
        int node = TN(x + 240, py + 20); TW(sr, node);
        Component *cc = add_comp(circuit, COMP_CAPACITOR, x + 240, py + 80, 90);
        cc->props.capacitor.capacitance = 5e-6;    /* RC = 0.5 ms, so the mean settles inside the run */
        cc->props.capacitor.ideal = (k == 0);
        cc->props.capacitor.esr = esr[k];
        cc->props.capacitor.esl = 0.0;              /* ESR only, so the step is exactly I x ESR */
        cc->props.capacitor.leakage = 1e9;
        Component *g2 = add_comp(circuit, COMP_GROUND, x + 240, py + 160, 0);
        int rt = TN(x + 240, py + 40), rb = TN(x + 240, py + 120), gt = TN(x + 240, py + 140);
        TW(node, rt); TW(rb, gt);
        cc->node_ids[0] = rt; cc->node_ids[1] = rb; g2->node_ids[0] = gt;
        add_label(circuit, x + 300, py + 20, nm[k]);
    }
    add_label(circuit, x - 40, y - 60, "IDEAL vs REAL CAPACITOR: a +/-5 V 20 kHz square through 100 ohm charges 5 uF - so the current is a +/-50 mA square");
    add_label(circuit, x - 40, y + 660, "An ideal capacitor integrates that square into a triangle: dV = I(T/2)/C = 250 mVpp. A real one is C in SERIES with");
    add_label(circuit, x - 40, y + 690, "its ESR, and the resistance copies the current square straight onto the output. That is why a supply's ripple barely");
    add_label(circuit, x - 40, y + 720, "improves when you add capacitance but collapses when you pick a low-ESR part. ESL does the same to the fast edges.");
    return 15;
}

// 4. DCR: the winding resistance that stops an L-C loop ringing for ever
static int place_id_ind(Circuit *circuit, float x, float y) {
    static const int idl[2] = { 1, 0 };
    static const char *nm[2] = { "IDEAL inductor: only the 10 ohm damps it, zeta = 0.05, peak 9.3 V",
                                 "REAL inductor, DCR = 50 ohm: zeta = 0.30, peak 6.9 V, ring gone in 3 cycles" };
    for (int k = 0; k < 2; k++) {
        float py = y + k * 260;
        Component *v = add_comp(circuit, COMP_PULSE_SOURCE, x, py + 60, 0);
        if (!v) return 0;
        v->props.pulse_source.v_low = 0; v->props.pulse_source.v_high = 5.0;
        v->props.pulse_source.period = 8e-3; v->props.pulse_source.pulse_width = 3e-3;
        v->props.pulse_source.rise_time = v->props.pulse_source.fall_time = 1e-6;
        Component *g = add_comp(circuit, COMP_GROUND, x, py + 140, 0);
        connect_terminals(circuit, v, 1, g, 0);
        int sp = TN(x, py + 20); v->node_ids[0] = sp;
        Component *rs = hres(circuit, x + 140, py + 20, 10.0);
        int sl = TN(x + 100, py + 20), sr = TN(x + 180, py + 20); TW(sp, sl);
        rs->node_ids[0] = sl; rs->node_ids[1] = sr;
        Component *l = add_comp(circuit, COMP_INDUCTOR, x + 280, py + 20, 0);   // (240,py+20)-(320,py+20)
        l->props.inductor.inductance = 10e-3;
        l->props.inductor.ideal = idl[k] ? true : false;
        l->props.inductor.dcr = 50.0;
        int ll = TN(x + 240, py + 20), lr = TN(x + 320, py + 20); TW(sr, ll);
        l->node_ids[0] = ll; l->node_ids[1] = lr;
        int node = TN(x + 400, py + 20); TW(lr, node);
        Component *cc = add_comp(circuit, COMP_CAPACITOR, x + 400, py + 80, 90);
        cc->props.capacitor.capacitance = 1e-6;
        Component *g2 = add_comp(circuit, COMP_GROUND, x + 400, py + 160, 0);
        int rt = TN(x + 400, py + 40), rb = TN(x + 400, py + 120), gt = TN(x + 400, py + 140);
        TW(node, rt); TW(rb, gt);
        cc->node_ids[0] = rt; cc->node_ids[1] = rb; g2->node_ids[0] = gt;
        add_label(circuit, x + 460, py + 20, nm[k]);
    }
    add_label(circuit, x - 40, y - 60, "IDEAL vs REAL INDUCTOR: a 5 V step into 10 ohm + 10 mH + 1 uF, so f0 = 1/(2 pi sqrt(LC)) = 1592 Hz");
    add_label(circuit, x - 40, y + 480, "Damping is zeta = (R/2) sqrt(C/L). With an ideal inductor only the 10 ohm resistor counts: zeta = 0.05 and the");
    add_label(circuit, x - 40, y + 510, "capacitor overshoots to 1.85 x 5 V = 9.3 V, ringing for many cycles. Give the winding its real 50 ohm DCR and");
    add_label(circuit, x - 40, y + 540, "zeta = 0.30: the peak falls to 6.9 V and the ring is gone in three. A lossless inductor is how a switching");
    add_label(circuit, x - 40, y + 570, "converter runs away in a simulator - the L-C loop it lives in has nothing to dissipate the energy.");
    return 12;
}

// 5. Gain-bandwidth product and slew rate against an infinite op-amp
static int place_id_opamp(Circuit *circuit, float x, float y) {
    static const int idl[3] = { 1, 0, 0 };
    static const double amp[3] = { 0.05, 0.05, 0.5 };
    static const char *nm[3] = { "IDEAL: 50 mV in, 500 mV out, at any frequency you like",
                                 "REAL (GBW 1 MHz): at Acl = 10 the bandwidth is 100 kHz, so 354 mV - 3 dB down",
                                 "REAL, 500 mV in: 5 V at 100 kHz needs 3.1 V/us, the part slews at 0.5 - a TRIANGLE" };
    for (int k = 0; k < 3; k++) {
        float py = y + k * 300;
        Component *v = add_comp(circuit, COMP_AC_VOLTAGE, x, py + 100, 0);      // +(x,py+60) -(x,py+140)
        if (!v) return 0;
        v->props.ac_voltage.amplitude = amp[k]; v->props.ac_voltage.frequency = 100e3;
        Component *g = add_comp(circuit, COMP_GROUND, x, py + 180, 0);
        connect_terminals(circuit, v, 1, g, 0);
        int sp = TN(x, py + 60); v->node_ids[0] = sp;
        Component *u = add_comp(circuit, COMP_OPAMP, x + 240, py + 40, 0);      // -(200,py+20) +(200,py+60) out(280,py+40)
        u->props.opamp.ideal = idl[k] ? true : false;
        u->props.opamp.gain = 1e5; u->props.opamp.gbw = 1e6; u->props.opamp.slew_rate = 0.5;
        int plus = TN(x + 200, py + 60); TW(sp, plus);        /* clear corridor: nothing sits on py+60 */
        int minus = TN(x + 200, py + 20);
        /* The gain network lives in its own column to the LEFT of the source, so the wire that
           feeds the + input never has to cross R1 (running the input along a resistor's own
           line reads - correctly - as a wire straight through the part). */
        Component *r1 = add_comp(circuit, COMP_RESISTOR, x - 100, py + 80, 90);  // (-100,py+40)-(-100,py+120)
        r1->props.resistor.resistance = 1000.0;
        Component *g1 = add_comp(circuit, COMP_GROUND, x - 100, py + 160, 0);
        int mleft = TN(x - 100, py + 20);
        int r1t = TN(x - 100, py + 40), r1b = TN(x - 100, py + 120), g1t = TN(x - 100, py + 140);
        TW(minus, mleft); TW(mleft, r1t); TW(r1b, g1t);
        r1->node_ids[0] = r1t; r1->node_ids[1] = r1b; g1->node_ids[0] = g1t;
        /* feedback over the top: out -> right -> up -> left through R2 -> down the same column */
        Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 235, py - 40, 0);   // (195,py-40)-(275,py-40)
        r2->props.resistor.resistance = 9000.0;
        int out = TN(x + 280, py + 40), oright = TN(x + 320, py + 40), otop = TN(x + 320, py - 40);
        int r2r = TN(x + 275, py - 40), r2l = TN(x + 195, py - 40), fbl = TN(x - 100, py - 40);
        TW(out, oright); TW(oright, otop); TW(otop, r2r); TW(r2l, fbl); TW(fbl, mleft);
        r2->node_ids[0] = r2l; r2->node_ids[1] = r2r;
        u->node_ids[0] = minus; u->node_ids[1] = plus; u->node_ids[2] = out;
        int lnode = TN(x + 400, py + 40); TW(oright, lnode);
        Component *rl = add_comp(circuit, COMP_RESISTOR, x + 400, py + 100, 90);  // (400,py+60)-(400,py+140)
        rl->props.resistor.resistance = 10000.0;
        Component *gl = add_comp(circuit, COMP_GROUND, x + 400, py + 180, 0);
        int lt = TN(x + 400, py + 60), lb = TN(x + 400, py + 140), glt = TN(x + 400, py + 160);
        TW(lnode, lt); TW(lb, glt);
        rl->node_ids[0] = lt; rl->node_ids[1] = lb; gl->node_ids[0] = glt;
        add_label(circuit, x + 470, py + 40, nm[k]);
    }
    add_label(circuit, x - 140, y - 100, "IDEAL vs REAL OP-AMP: three non-inverting stages, gain 1 + 9k/1k = 10, all driven at 100 kHz");
    add_label(circuit, x - 140, y + 800, "An ideal op-amp has infinite gain and infinite bandwidth, so 50 mV in is 500 mV out however fast you drive it. A real");
    add_label(circuit, x - 140, y + 840, "part has a gain-bandwidth PRODUCT: 1 MHz at a closed-loop gain of 10 leaves 100 kHz of bandwidth, so at exactly");
    add_label(circuit, x - 140, y + 880, "100 kHz the output is 3 dB down. Row 3 asks the same part for 5 V at 100 kHz, which needs 2 pi f V = 3.1 V/us; it");
    add_label(circuit, x - 140, y + 920, "can only manage 0.5, so the sine leaves as a triangle. Bandwidth is small-signal, slew rate is large-signal.");
    return 24;
}

// 6. The Early effect moves the operating point
static int place_id_bjt(Circuit *circuit, float x, float y) {
    static const int idl[2] = { 1, 0 };
    static const char *nm[2] = { "IDEAL BJT (no Early effect): I_C = beta I_B = 1.00 mA, V_C = 7.30 V",
                                 "REAL BJT (V_AF = 80 V): I_C climbs with V_CE, so the collector settles lower" };
    for (int k = 0; k < 2; k++) {
        float py = y + k * 320;
        Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x, py + 60, 0);     // +(x,py+20)
        if (!vcc) return 0;
        vcc->props.dc_voltage.voltage = 12.0;
        Component *g0 = add_comp(circuit, COMP_GROUND, x, py + 140, 0);
        connect_terminals(circuit, vcc, 1, g0, 0);
        int rail = TN(x, py + 20); vcc->node_ids[0] = rail;
        int rbtop = TN(x + 140, py + 20), rctop = TN(x + 340, py + 20);
        TW(rail, rbtop); TW(rbtop, rctop);
        Component *rb = add_comp(circuit, COMP_RESISTOR, x + 140, py + 80, 90); // (140,py+40)-(140,py+120)
        rb->props.resistor.resistance = 1.13e6;
        Component *rc = add_comp(circuit, COMP_RESISTOR, x + 340, py + 80, 90);
        rc->props.resistor.resistance = 4700.0;
        int rbt = TN(x + 140, py + 40), rbb = TN(x + 140, py + 120);
        int rct = TN(x + 340, py + 40), rcb = TN(x + 340, py + 120);
        TW(rbtop, rbt); TW(rctop, rct);
        rb->node_ids[0] = rbt; rb->node_ids[1] = rbb;
        rc->node_ids[0] = rct; rc->node_ids[1] = rcb;
        Component *q = add_comp(circuit, COMP_NPN_BJT, x + 260, py + 160, 0);   // B(240,py+160) C(280,py+140) E(280,py+180)
        q->props.bjt.bf = 100.0; q->props.bjt.vaf = 80.0;
        q->props.bjt.ideal = idl[k] ? true : false;
        int base = TN(x + 240, py + 160), bdrop = TN(x + 140, py + 160);
        TW(rbb, bdrop); TW(bdrop, base);
        int coll = TN(x + 280, py + 140), cdrop = TN(x + 340, py + 140);
        TW(rcb, cdrop); TW(cdrop, coll);
        int emit = TN(x + 280, py + 180), eb = TN(x + 280, py + 220);
        TW(emit, eb);
        Component *ge = add_comp(circuit, COMP_GROUND, x + 280, py + 240, 0);
        ge->node_ids[0] = eb;
        q->node_ids[0] = base; q->node_ids[1] = coll; q->node_ids[2] = emit;
        add_label(circuit, x + 420, py + 140, nm[k]);
    }
    add_label(circuit, x - 40, y - 60, "IDEAL vs REAL BJT: the same fixed-bias stage twice. 1.13 M sets I_B = (12 - 0.7)/1.13M = 10 uA, beta = 100");
    add_label(circuit, x - 40, y + 620, "The textbook answer is I_C = beta I_B = 1 mA and V_C = 12 - 1 mA x 4.7k = 7.30 V, and that is what the ideal");
    add_label(circuit, x - 40, y + 650, "model gives. A real transistor's collector current also rises with V_CE - the Early effect, the (1 + V_CE/V_AF)");
    add_label(circuit, x - 40, y + 680, "term - so with V_AF = 80 V it draws ~9 % more and the collector sits lower. The same slope is the stage's output");
    add_label(circuit, x - 40, y + 710, "resistance r_o = V_AF/I_C, so it decides the gain of every current-source-loaded amplifier you will build.");
    return 12;
}

// 7. Channel-length modulation
static int place_id_mosfet(Circuit *circuit, float x, float y) {
    static const int idl[2] = { 1, 0 };
    static const double lam[2] = { 0.0, 0.05 };
    static const char *nm[2] = { "IDEAL (lambda = 0): I_D = K V_ov^2 / 2 = 2.25 mA, V_D = 7.05 V",
                                 "REAL (lambda = 0.05 /V): the (1 + lambda V_DS) term pulls V_D down to 5.65 V" };
    for (int k = 0; k < 2; k++) {
        float py = y + k * 340;
        Component *vdd = add_comp(circuit, COMP_DC_VOLTAGE, x, py + 60, 0);
        if (!vdd) return 0;
        vdd->props.dc_voltage.voltage = 12.0;
        Component *g0 = add_comp(circuit, COMP_GROUND, x, py + 140, 0);
        connect_terminals(circuit, vdd, 1, g0, 0);
        int rail = TN(x, py + 20); vdd->node_ids[0] = rail;
        int gtop = TN(x + 140, py + 20), rdtop = TN(x + 340, py + 20);
        TW(rail, gtop); TW(gtop, rdtop);
        Component *rg1 = add_comp(circuit, COMP_RESISTOR, x + 140, py + 80, 90);   // (140,py+40)-(140,py+120)
        rg1->props.resistor.resistance = 900e3;
        Component *rg2 = add_comp(circuit, COMP_RESISTOR, x + 140, py + 200, 90);  // (140,py+160)-(140,py+240)
        rg2->props.resistor.resistance = 300e3;
        Component *rd = add_comp(circuit, COMP_RESISTOR, x + 340, py + 80, 90);
        rd->props.resistor.resistance = 2200.0;
        int g1t = TN(x + 140, py + 40), g1b = TN(x + 140, py + 120);
        int g2t = TN(x + 140, py + 160), g2b = TN(x + 140, py + 240);
        int rdt = TN(x + 340, py + 40), rdb = TN(x + 340, py + 120);
        TW(gtop, g1t); TW(rdtop, rdt); TW(g1b, g2t);
        rg1->node_ids[0] = g1t; rg1->node_ids[1] = g1b;
        rg2->node_ids[0] = g2t; rg2->node_ids[1] = g2b;
        rd->node_ids[0] = rdt; rd->node_ids[1] = rdb;
        Component *gg = add_comp(circuit, COMP_GROUND, x + 140, py + 260, 0);
        gg->node_ids[0] = g2b;
        Component *m = add_comp(circuit, COMP_NMOS, x + 260, py + 160, 0);         // G(240,py+160) D(280,py+140) S(280,py+180)
        m->props.mosfet.vth = 1.5; m->props.mosfet.kp = 2e-3;
        m->props.mosfet.w = 1e-6; m->props.mosfet.l = 1e-6;    /* W/L = 1, so K = u_Cox(W/L) = 2 mA/V^2 */
        m->props.mosfet.lambda = lam[k];
        m->props.mosfet.ideal = idl[k] ? true : false;
        int gate = TN(x + 240, py + 160); TW(g2t, gate);
        int drain = TN(x + 280, py + 140), ddrop = TN(x + 340, py + 140);
        TW(rdb, ddrop); TW(ddrop, drain);
        int src = TN(x + 280, py + 180), sb = TN(x + 280, py + 220);
        TW(src, sb);
        Component *gs = add_comp(circuit, COMP_GROUND, x + 280, py + 240, 0);
        gs->node_ids[0] = sb;
        m->node_ids[0] = gate; m->node_ids[1] = drain; m->node_ids[2] = src;
        add_label(circuit, x + 420, py + 140, nm[k]);
    }
    add_label(circuit, x - 40, y - 60, "IDEAL vs REAL MOSFET: common source, 900k/300k biases the gate at 3.0 V, V_th = 1.5 V, K = u_Cox(W/L) = 2 mA/V^2");
    add_label(circuit, x - 40, y + 660, "The square law gives I_D = K V_ov^2/2 = 2.25 mA and V_D = 12 - 2.2k x I_D = 7.05 V, which is exactly what the");
    add_label(circuit, x - 40, y + 690, "lambda = 0 device does. Switch lambda to 0.05 /V and the drain current picks up a (1 + lambda V_DS) factor: the");
    add_label(circuit, x - 40, y + 720, "operating point moves to 5.65 V, a 1.4 V shift. lambda is also what gives the device a finite output resistance");
    add_label(circuit, x - 40, y + 750, "r_o = 1/(lambda I_D); without it the saturation curves are flat and a current mirror would be perfect.");
    return 16;
}


// 8. What a real op-amp gets wrong at DC, and the resistor that fixes half of it
static int place_id_opamp_err(Circuit *circuit, float x, float y) {
    for (int k = 0; k < 2; k++) {
        float py = y + k * 380;
        bool real = (k == 1);
        Component *u = add_comp(circuit, COMP_OPAMP, x + 240, py + 40, 0);   // -(200,py+20) +(200,py+60) out(280,py+40)
        if (!u) return 0;
        u->props.opamp.ideal = !real;
        u->props.opamp.gain = 1e5;
        u->props.opamp.voffset = 1e-3;      /* 1 mV input offset, a jellybean bipolar part */
        u->props.opamp.i_bias = 100e-9;     /* 100 nA into each input */
        int minus = TN(x + 200, py + 20), plus = TN(x + 200, py + 60);

        /* gain network: R1 to ground, Rf from the output, both in the left column */
        Component *r1 = add_comp(circuit, COMP_RESISTOR, x - 100, py + 80, 90);   // (-100,py+40)-(-100,py+120)
        r1->props.resistor.resistance = 1000.0;
        Component *g1 = add_comp(circuit, COMP_GROUND, x - 100, py + 160, 0);
        int mleft = TN(x - 100, py + 20), r1t = TN(x - 100, py + 40), r1b = TN(x - 100, py + 120), g1t = TN(x - 100, py + 140);
        TW(minus, mleft); TW(mleft, r1t); TW(r1b, g1t);
        r1->node_ids[0] = r1t; r1->node_ids[1] = r1b; g1->node_ids[0] = g1t;
        Component *rf = add_comp(circuit, COMP_RESISTOR, x + 235, py - 40, 0);    // (195,py-40)-(275,py-40)
        rf->props.resistor.resistance = 99000.0;                                  /* 1 + 99k/1k = x100 */
        int out = TN(x + 280, py + 40), oright = TN(x + 320, py + 40), otop = TN(x + 320, py - 40);
        int rfr = TN(x + 275, py - 40), rfl = TN(x + 195, py - 40), fbl = TN(x - 100, py - 40);
        TW(out, oright); TW(oright, otop); TW(otop, rfr); TW(rfl, fbl); TW(fbl, mleft);
        rf->node_ids[0] = rfl; rf->node_ids[1] = rfr;
        u->node_ids[0] = minus; u->node_ids[1] = plus; u->node_ids[2] = out;

        /* source resistance on the + input: 99k + 1k to ground, with a switch across the 99k */
        int ptap = TN(x + 120, py + 60), ptop = TN(x + 120, py + 100);
        TW(plus, ptap); TW(ptap, ptop);
        Component *rs_big = add_comp(circuit, COMP_RESISTOR, x + 120, py + 140, 90);  // (120,py+100)-(120,py+180)
        rs_big->props.resistor.resistance = 99000.0;
        Component *rs_small = add_comp(circuit, COMP_RESISTOR, x + 120, py + 220, 90); // (120,py+180)-(120,py+260)
        rs_small->props.resistor.resistance = 1000.0;
        Component *gs = add_comp(circuit, COMP_GROUND, x + 120, py + 280, 0);
        int mid = TN(x + 120, py + 180), sbot = TN(x + 120, py + 260);
        rs_big->node_ids[0] = ptop; rs_big->node_ids[1] = mid;
        rs_small->node_ids[0] = mid; rs_small->node_ids[1] = sbot;
        gs->node_ids[0] = sbot;
        if (real) {
            /* only the real stage gets the matching switch - it is the only one the error
               depends on, and it keeps exactly one switch in the template for --switch-test */
            Component *sw = add_comp(circuit, COMP_SPST_SWITCH, x + 40, py + 140, 90);  // (40,py+100)-(40,py+180)
            sw->props.switch_spst.closed = false;
            int swt = TN(x + 40, py + 100), swb = TN(x + 40, py + 180);
            TW(ptop, swt); TW(swb, mid);
            sw->node_ids[0] = swt; sw->node_ids[1] = swb;
            add_label(circuit, x + 360, py + 100, "CLOSE to short the 99k: R_s = 1k matches R1||Rf = 990 ohm");
        }
        add_label(circuit, x + 360, py + 40,
                  real ? "REAL: V_os = 1 mV, I_B = 100 nA -> -0.89 V out of a grounded input"
                       : "IDEAL: no offset, no bias current -> 0.000 V");
    }
    add_label(circuit, x - 180, y - 100, "OP-AMP ERROR SOURCES: two x100 non-inverting stages with the input grounded through a source resistance");
    add_label(circuit, x - 180, y + 700, "Everything at the output is error. V_out = 100 (V_os - I_B (R_s - R1||Rf)): the offset contributes 100 mV and the");
    add_label(circuit, x - 180, y + 730, "bias current 990 mV, in opposite directions, so the real stage sits at -0.89 V with nothing at its input. Close the");
    add_label(circuit, x - 180, y + 760, "switch and R_s becomes 1k - almost exactly the 990 ohm the inverting input sees - so the two bias-current drops");
    add_label(circuit, x - 180, y + 790, "cancel and only the offset is left. Matching the DC resistance at both inputs is a free 10x accuracy improvement.");
    return 15;
}


// 9. The same low-side switch built with three real devices from the part library
static int place_parts_mosfet(Circuit *circuit, float x, float y) {
    static const char *parts[3] = { "2N7000", "2N7002", "IRF540N" };
    static const char *nm[3] = {
        "2N7000: R_DS(on) 1.2 ohm at V_GS = 10 V -> 143 mV across the switch",
        "2N7002: R_DS(on) 2 ohm -> 233 mV, and it is the same silicon in a smaller package",
        "IRF540N: R_DS(on) 44 mohm -> 5 mV; a power part earns its size in the drop"
    };
    for (int k = 0; k < 3; k++) {
        float py = y + k * 360;
        Component *vdd = add_comp(circuit, COMP_DC_VOLTAGE, x, py + 60, 0);   // +(x,py+20)
        if (!vdd) return 0;
        vdd->props.dc_voltage.voltage = 12.0;
        Component *g0 = add_comp(circuit, COMP_GROUND, x, py + 140, 0);
        connect_terminals(circuit, vdd, 1, g0, 0);
        int rail = TN(x, py + 20), rt = TN(x + 240, py + 20);
        vdd->node_ids[0] = rail; TW(rail, rt);

        Component *rl = add_comp(circuit, COMP_RESISTOR, x + 240, py + 60, 90);   // (240,py+20)-(240,py+100)
        rl->props.resistor.resistance = 100.0;
        rl->props.resistor.power_rating = 5.0;                                    /* 1.4 W in this circuit */
        int rb = TN(x + 240, py + 100), rdrop = TN(x + 240, py + 140);
        rl->node_ids[0] = rt; rl->node_ids[1] = rb;
        TW(rb, rdrop);

        Component *m = add_comp(circuit, COMP_NMOS, x + 320, py + 160, 0);   // G(300,py+160) D(340,py+140) S(340,py+180)
        component_apply_part(m, parts[k]);                                   /* the data sheet model, by name */
        int drain = TN(x + 340, py + 140), gate = TN(x + 300, py + 160), src = TN(x + 340, py + 180);
        TW(rdrop, drain);
        m->node_ids[0] = gate; m->node_ids[1] = drain; m->node_ids[2] = src;
        int sb = TN(x + 340, py + 220);
        TW(src, sb);
        Component *gm = add_comp(circuit, COMP_GROUND, x + 340, py + 240, 0);
        gm->node_ids[0] = sb;

        /* gate held at the data sheet's 10 V test condition */
        Component *vg = add_comp(circuit, COMP_DC_VOLTAGE, x + 140, py + 240, 0);  // +(140,py+200) -(140,py+280)
        vg->props.dc_voltage.voltage = 10.0;
        Component *gg = add_comp(circuit, COMP_GROUND, x + 140, py + 320, 0);
        connect_terminals(circuit, vg, 1, gg, 0);
        int gp = TN(x + 140, py + 200), gu = TN(x + 140, py + 160);
        vg->node_ids[0] = gp;
        TW(gp, gu); TW(gu, gate);

        if (k == 0) {
            /* a second load the switch can bring in: twice the current through the same device */
            Component *r2 = add_comp(circuit, COMP_RESISTOR, x + 460, py + 60, 90);  // (460,py+20)-(460,py+100)
            r2->props.resistor.resistance = 100.0;
            r2->props.resistor.power_rating = 5.0;
            Component *sw = add_comp(circuit, COMP_SPST_SWITCH, x + 460, py + 160, 90); // (460,py+120)-(460,py+200)
            sw->props.switch_spst.closed = false;
            int r2t = TN(x + 460, py + 20), r2b = TN(x + 460, py + 100);
            int swt = TN(x + 460, py + 120), swb = TN(x + 460, py + 200), sj = TN(x + 460, py + 220);
            TW(rt, r2t); TW(r2b, swt); TW(swb, sj);
            int dj = TN(x + 400, py + 220), dd = TN(x + 400, py + 140);
            TW(sj, dj); TW(dj, dd); TW(dd, drain);
            r2->node_ids[0] = r2t; r2->node_ids[1] = r2b;
            sw->node_ids[0] = swt; sw->node_ids[1] = swb;
            add_label(circuit, x + 570, py + 160, "CLOSE: a second 100 ohm load, so twice the current");
        }
        add_label(circuit, x + 520, py + 60, nm[k]);
    }
    add_label(circuit, x - 40, y - 60, "NAMED PARTS: the same low-side switch with three real MOSFETs, each at the 10 V gate drive its data sheet specifies");
    add_label(circuit, x - 40, y + 1080, "F2 shows each part number on the canvas, and the properties panel's Part row swaps a device for another in the");
    add_label(circuit, x - 40, y + 1110, "library - the parameters come from the data sheet, and template_smoke --part-test rebuilds each one's test");
    add_label(circuit, x - 40, y + 1140, "condition to check it. The drop across a closed switch is I x R_DS(on): the 2N7000 loses 143 mV and dissipates");
    add_label(circuit, x - 40, y + 1170, "17 mW, the IRF540N loses 5 mV. Close the switch to double the current and watch the drop double with it.");
    return 24;
}


// 10. What a class-II ceramic actually gives you once there is voltage across it
static int place_cap_dcbias(Circuit *circuit, float x, float y) {
    static const double bias[3] = { 0.0, 2.0, 5.0 };
    static const char *nm[3] = {
        "no bias: the full 10 uF, 62 mVpp of ripple",
        "2 V bias: half the capacitance, twice the ripple",
        "5 V bias: 2.9 uF left of the marked 10 uF - 219 mVpp"
    };
    for (int k = 0; k < 3; k++) {
        float py = y + k * 300;
        Component *v = add_comp(circuit, COMP_SQUARE_WAVE, x, py + 60, 0);   // +(x,py+20)
        if (!v) return 0;
        v->props.square_wave.amplitude = 0.5; v->props.square_wave.frequency = 20e3;
        v->props.square_wave.duty = 0.5;
        Component *g0 = add_comp(circuit, COMP_GROUND, x, py + 140, 0);
        connect_terminals(circuit, v, 1, g0, 0);
        int sp = TN(x, py + 20); v->node_ids[0] = sp;
        Component *rs = hres(circuit, x + 140, py + 20, 20.0);               /* 0.5 V / 20 ohm = 25 mA */
        int sl = TN(x + 100, py + 20), sr = TN(x + 180, py + 20); TW(sp, sl);
        rs->node_ids[0] = sl; rs->node_ids[1] = sr;
        int node = TN(x + 240, py + 20); TW(sr, node);

        Component *cc = add_comp(circuit, COMP_CAPACITOR, x + 240, py + 80, 90);  // (240,py+40)-(240,py+120)
        component_apply_part(cc, "X5R 10uF");                                /* 10 uF, half at 2 V */
        int ct = TN(x + 240, py + 40), cb = TN(x + 240, py + 120);
        TW(node, ct);
        cc->node_ids[0] = ct; cc->node_ids[1] = cb;

        /* the bias sits under the capacitor, and is a short to the ripple */
        Component *vb = add_comp(circuit, COMP_DC_VOLTAGE, x + 240, py + 180, 0);  // +(240,py+140) -(240,py+220)
        vb->props.dc_voltage.voltage = bias[k];
        Component *gb = add_comp(circuit, COMP_GROUND, x + 240, py + 260, 0);
        connect_terminals(circuit, vb, 1, gb, 0);
        int bp = TN(x + 240, py + 140); vb->node_ids[0] = bp;
        TW(cb, bp);
        add_label(circuit, x + 320, py + 20, nm[k]);
    }
    add_label(circuit, x - 40, y - 60, "CERAMIC DC BIAS: the same 10 uF X5R three times, same 25 mA ripple current, three different DC biases");
    add_label(circuit, x - 40, y + 900, "A class-II ceramic (X5R, X7R) is a ferroelectric, and its permittivity falls as the field across it rises. This part");
    add_label(circuit, x - 40, y + 930, "is down to half its marked value at 2 V and under a third at 5 V, so the ripple on the same current triples - the");
    add_label(circuit, x - 40, y + 960, "capacitor is still marked 10 uF. Set 'Bias 1/2' to 0 in the properties panel for a class-I part (C0G/NP0), which");
    add_label(circuit, x - 40, y + 990, "does not do this at all; that is what you buy when the value has to be the value.");
    return 18;
}


/* ======================= The 555, as a subcircuit =======================
   The block placed by the 555 template is a real definition: three 5k resistors setting 1/3
   and 2/3 of the supply, two comparators watching TRIGGER and THRESHOLD against them, the NOR
   latch they drive, and the discharge transistor. It is registered once and reused, so it
   behaves like any block a user builds with Ctrl+G - including being visible in the library.

   Internal node ids: 1 VCC, 2 TRIG, 3 THRES, 5 DISCH, 6 GND, 7 = 2/3 V+, 8 = 1/3 V+,
   9 SET, 10 RESET, 11 Q (this is the OUT pin), 12 QBAR (drives the discharge transistor). */
static int ne555_def_id(void) {
    for (int i = 0; i < g_subcircuit_library.count; i++)
        if (!strcmp(g_subcircuit_library.defs[i].name, "NE555")) return g_subcircuit_library.defs[i].id;
    if (g_subcircuit_library.count >= MAX_SUBCIRCUIT_DEFS) return 0;

    static const struct { ComponentType t; float x, y; int a, b, c2; } spec[8] = {
        { COMP_RESISTOR, 100, 60,  1, 7, -1 },      /* R1  V+ -> 2/3 */
        { COMP_RESISTOR, 100, 180, 7, 8, -1 },      /* R2  2/3 -> 1/3 */
        { COMP_RESISTOR, 100, 300, 8, 6, -1 },      /* R3  1/3 -> GND */
        { COMP_OPAMP,    300, 100, 7, 3, 10 },      /* comparator A: -=2/3, +=THRES, out=RESET */
        { COMP_OPAMP,    300, 240, 2, 8, 9  },      /* comparator B: -=TRIG, +=1/3, out=SET */
        { COMP_NOR_GATE, 480, 120, 9, 11, 12 },     /* QBAR = NOR(SET, Q) */
        { COMP_NOR_GATE, 480, 240, 12, 10, 11 },    /* Q    = NOR(QBAR, RESET) */
        { COMP_NMOS,     660, 240, 12, 5, 6 },      /* discharge: gate QBAR, drain DISCH */
    };
    const int NPARTS = 8;

    SubCircuitDef *def = &g_subcircuit_library.defs[g_subcircuit_library.count];
    memset(def, 0, sizeof *def);
    def->component_data = calloc((size_t)NPARTS, sizeof(Component));
    if (!def->component_data) return 0;
    Component *parts = (Component *)def->component_data;

    for (int i = 0; i < NPARTS; i++) {
        Component *proto = component_create(spec[i].t, spec[i].x, spec[i].y);
        if (!proto) { free(def->component_data); def->component_data = NULL; return 0; }
        parts[i] = *proto;
        free(proto);
        Component *p = &parts[i];
        if (spec[i].t == COMP_RESISTOR) { p->props.resistor.resistance = 5000.0; p->props.resistor.power_rating = 1.0; }
        if (spec[i].t == COMP_OPAMP) {
            /* a comparator, and its rails are the logic levels the gates expect */
            p->props.opamp.ideal = true; p->props.opamp.gain = 1e5;
            p->props.opamp.vmax = 5.0; p->props.opamp.vmin = 0.0;
        }
        if (spec[i].t == COMP_NMOS) {
            p->props.mosfet.vth = 1.0; p->props.mosfet.kp = 0.05;
            p->props.mosfet.w = 1e-6; p->props.mosfet.l = 1e-6; p->props.mosfet.ideal = false;
        }
        p->node_ids[0] = spec[i].a;
        p->node_ids[1] = spec[i].b;
        if (spec[i].c2 >= 0) p->node_ids[2] = spec[i].c2;
        snprintf(p->label, sizeof p->label, "%c%d",
                 (spec[i].t == COMP_RESISTOR) ? 'R' : (spec[i].t == COMP_OPAMP) ? 'U' :
                 (spec[i].t == COMP_NOR_GATE) ? 'G' : 'M', i + 1);
    }

    def->id = ++g_subcircuit_library.next_id;
    snprintf(def->name, sizeof def->name, "NE555");
    def->component_data_size = (size_t)NPARTS * sizeof(Component);
    def->num_components = NPARTS;

    static const struct { const char *name; int node; } pins[6] = {
        { "V+", 1 }, { "GND", 6 }, { "TRIG", 2 }, { "THRES", 3 }, { "OUT", 11 }, { "DISCH", 5 }
    };
    def->num_pins = 6;
    for (int i = 0; i < 6; i++) {
        snprintf(def->pins[i].name, sizeof def->pins[i].name, "%s", pins[i].name);
        def->pins[i].internal_node_id = pins[i].node;
        def->pins[i].side = (i < 3) ? 0 : 1;
        def->pins[i].position = i % 3;
    }
    def->num_internal_nodes = 6;          /* 7, 8, 9, 10, 11, 12 */
    def->internal_width = 760; def->internal_height = 400;
    def->block_width = 120; def->block_height = 120;
    g_subcircuit_library.count++;
    return def->id;
}

// 11. The classic astable, with the 555 as a block that really contains a 555
static int place_ne555_astable(Circuit *circuit, float x, float y) {
    int def_id = ne555_def_id();
    if (!def_id) return 0;

    Component *vcc = add_comp(circuit, COMP_DC_VOLTAGE, x, y + 60, 0);        // +(x,y+20)
    if (!vcc) return 0;
    vcc->props.dc_voltage.voltage = 5.0;
    Component *g0 = add_comp(circuit, COMP_GROUND, x, y + 140, 0);
    connect_terminals(circuit, vcc, 1, g0, 0);
    int rail = TN(x, y + 20), rt = TN(x + 160, y + 20); vcc->node_ids[0] = rail; TW(rail, rt);

    Component *ra = add_comp(circuit, COMP_RESISTOR, x + 160, y + 80, 90);    // (160,y+40)-(160,y+120)
    ra->props.resistor.resistance = 10e3;
    int rab = TN(x + 160, y + 120);
    ra->node_ids[0] = TN(x + 160, y + 40); ra->node_ids[1] = rab;
    TW(rt, ra->node_ids[0]);

    Component *rb = add_comp(circuit, COMP_RESISTOR, x + 160, y + 200, 90);   // (160,y+160)-(160,y+240)
    rb->props.resistor.resistance = 10e3;
    int rbt = TN(x + 160, y + 160), rbb = TN(x + 160, y + 240);
    rb->node_ids[0] = rbt; rb->node_ids[1] = rbb;
    TW(rab, rbt);

    Component *ct = add_comp(circuit, COMP_CAPACITOR, x + 160, y + 320, 90);  // (160,y+280)-(160,y+360)
    ct->props.capacitor.capacitance = 10e-9;
    int ctt = TN(x + 160, y + 280), ctb = TN(x + 160, y + 360);
    ct->node_ids[0] = ctt; ct->node_ids[1] = ctb;
    TW(rbb, ctt);
    Component *gc = add_comp(circuit, COMP_GROUND, x + 160, y + 400, 0);
    gc->node_ids[0] = ctb;

    /* the block: V+, GND, TRIG, THRES, OUT, DISCH */
    Component *ic = add_comp(circuit, COMP_SUBCIRCUIT, x + 400, y + 200, 0);
    ic->props.subcircuit.def_id = def_id;
    ic->num_terminals = 6;
    snprintf(ic->props.subcircuit.name, sizeof ic->props.subcircuit.name, "NE555");
    int gnd_ic = TN(x + 400, y + 420), out_n = TN(x + 620, y + 200);
    Component *gi = add_comp(circuit, COMP_GROUND, x + 400, y + 440, 0);
    gi->node_ids[0] = gnd_ic;
    /* V+ from the rail, TRIG and THRES both on the capacitor, DISCH between R_A and R_B */
    int vp = TN(x + 320, y + 20); TW(rt, vp);
    int trig_n = ctt;
    ic->node_ids[0] = vp;        /* V+   */
    ic->node_ids[1] = gnd_ic;    /* GND  */
    ic->node_ids[2] = trig_n;    /* TRIG */
    ic->node_ids[3] = trig_n;    /* THRES tied to TRIG, which is what makes it astable */
    ic->node_ids[4] = out_n;     /* OUT  */
    ic->node_ids[5] = rab;       /* DISCH, between R_A and R_B */

    Component *rl = add_comp(circuit, COMP_RESISTOR, x + 700, y + 260, 90);   // (700,y+220)-(700,y+300)
    rl->props.resistor.resistance = 10e3;
    int lt = TN(x + 700, y + 220), lb = TN(x + 700, y + 300);
    rl->node_ids[0] = lt; rl->node_ids[1] = lb;
    TW(out_n, TN(x + 700, y + 200)); TW(TN(x + 700, y + 200), lt);
    Component *gl = add_comp(circuit, COMP_GROUND, x + 700, y + 340, 0);
    gl->node_ids[0] = lb;

    add_label(circuit, x - 40, y - 60, "555 ASTABLE: the block is a real subcircuit - comparators, divider, NOR latch and discharge transistor inside");
    add_label(circuit, x - 40, y + 500, "C charges from V+ through R_A + R_B until THRESHOLD reaches 2/3 of the supply, then the latch flips, the");
    add_label(circuit, x - 40, y + 530, "discharge transistor pulls the DISCH pin down and C empties through R_B alone until TRIGGER falls to 1/3.");
    add_label(circuit, x - 40, y + 560, "f = 1.44/((R_A + 2 R_B) C) = 4.8 kHz here, and the duty cycle is (R_A + R_B)/(R_A + 2 R_B) = 67 %.");
    return 10;
}

/* =====================================================================================
 * Interview prep: instrumentation and the oscilloscope.
 *
 * These are the measurement questions - the ones where the circuit is fine and the
 * answer on the screen is wrong because of how it was measured. Every one of them is a
 * real interview question at a company that builds or tests hardware, and every one is
 * a mistake that costs a day in the lab the first time you make it.
 *
 * Nothing here duplicates an existing template: the signal-integrity templates
 * (Signal Reflections, Impedance Matching, SPI Lines) are about what the BOARD does,
 * and these are about what the INSTRUMENT does to what the board did.
 * =================================================================================== */

/* A 10x probe and a scope input, as a network: 9M in parallel with the compensation
   trimmer, feeding the scope's 1M in parallel with its 15 pF. The divider is flat only
   when 9M * Ccomp = 1M * 15 pF, i.e. Ccomp = 1.67 pF. */
static void probe_channel(Circuit *circuit, float x, float y, double ccomp, int src_node, const char *tag) {
    Component *rp = hres(circuit, x + 80, y, 9e6);                      // (40,0)-(120,0)
    Component *cp = hcap(circuit, x + 80, y - 80, ccomp);               // (40,-80)-(120,-80)
    int pl = TN(x + 40, y), pr = TN(x + 120, y);
    int cl = TN(x + 40, y - 80), cr = TN(x + 120, y - 80);
    TW(src_node, pl); TW(pl, cl); TW(pr, cr);
    rp->node_ids[0] = pl; rp->node_ids[1] = pr;
    cp->node_ids[0] = cl; cp->node_ids[1] = cr;

    int tip = TN(x + 200, y);
    TW(pr, tip);
    Component *rin = vres(circuit, x + 200, y + 100, 1e6);              // (200,60)-(200,140)
    Component *cin = vcap(circuit, x + 280, y + 100, 15e-12);           // (280,60)-(280,140)
    int rt = TN(x + 200, y + 60), rb = TN(x + 200, y + 140);
    int ct = TN(x + 280, y + 60), cb = TN(x + 280, y + 140);
    TW(tip, rt); TW(rt, ct); TW(rb, cb);
    rin->node_ids[0] = rt; rin->node_ids[1] = rb;
    cin->node_ids[0] = ct; cin->node_ids[1] = cb;
    Component *g = add_comp(circuit, COMP_GROUND, x + 200, y + 200, 0);
    g->node_ids[0] = TN(x + 200, y + 180);
    TW(rb, TN(x + 200, y + 180));
    add_label(circuit, x + 380, y + 100, tag);
}

static int place_iv_probe_comp(Circuit *circuit, float x, float y) {
    /* the scope's own CAL output: 1 kHz square, 0..5 V, out of about 1 k */
    static const double ccomp[3] = { 0.8e-12, 1.67e-12, 3.3e-12 };
    static const char *tag[3] = {
        "UNDER-compensated (0.8 pF): the edge is rounded, tau = 14 us - you would report a slow driver that is not slow",
        "CORRECT (1.67 pF): 9M x 1.67p = 1M x 15p, so the divider is 1/10 at every frequency and the top is flat",
        "OVER-compensated (3.3 pF): the edge overshoots to 0.9 V and decays - you would report ringing that is not there"
    };
    for (int k = 0; k < 3; k++) {
        float py = y + k * 320;
        Component *cal = add_comp(circuit, COMP_SQUARE_WAVE, x, py + 60, 0);   // +(x,py+20) -(x,py+100)
        if (!cal) return 0;
        cal->props.square_wave.amplitude = 2.5; cal->props.square_wave.offset = 2.5;
        cal->props.square_wave.frequency = 1000.0; cal->props.square_wave.duty = 0.5;
        Component *gc = add_comp(circuit, COMP_GROUND, x, py + 160, 0);
        connect_terminals(circuit, cal, 1, gc, 0);
        int sp = TN(x, py + 20); cal->node_ids[0] = sp;
        Component *rsrc = hres(circuit, x + 100, py + 20, 1000.0);             // (60,20)-(140,20)
        int sl = TN(x + 60, py + 20), sr = TN(x + 140, py + 20);
        TW(sp, sl); rsrc->node_ids[0] = sl; rsrc->node_ids[1] = sr;
        probe_channel(circuit, x + 180, py + 20, ccomp[k], sr, tag[k]);
    }
    add_label(circuit, x - 40, y - 120, "PROBE COMPENSATION: the same 1 kHz CAL square through three 10x probes - the only difference is the trimmer");
    add_label(circuit, x - 40, y + 1000, "A 10x probe is a 9M/1M divider, and a divider made of resistors is only flat if the strays across them divide the");
    add_label(circuit, x - 40, y + 1030, "same way. The trimmer sets the probe's own capacitance so that 9M x Cp equals 1M x 15 pF. Compensate on the");
    add_label(circuit, x - 40, y + 1060, "CAL output before you trust an edge - and do it in 10x, never 1x: in 1x the trimmer is not in the path at all.");
    add_label(circuit, x - 40, y + 1090, "ALSO SEE: Probe Loading (what the probe takes from the node) and Ground Lead Ringing (what the return adds).");
    return 24;
}

static int place_iv_probe_loading(Circuit *circuit, float x, float y) {
    /* 3.3 V, 1 MHz square out of 10 k with 5 pF of board stray: 50 ns natural edge */
    static const double cprobe[3] = { 0.0, 100e-12, 12e-12 };
    static const double rprobe[3] = { 0.0, 1e6, 10e6 };
    static const char *tag[3] = {
        "no probe: 10 k into the 5 pF of the board alone, tau = 50 ns - this is the edge that is really there",
        "1x probe (1M || 100 pF): tau = 1.05 us. The 1 MHz square is now a triangle. The circuit did not change",
        "10x probe (10M || 12 pF): tau = 170 ns. Still loaded, but you can see an edge - this is why 10x is the default"
    };
    for (int k = 0; k < 3; k++) {
        float py = y + k * 260;
        Component *src = add_comp(circuit, COMP_SQUARE_WAVE, x, py + 60, 0);
        if (!src) return 0;
        src->props.square_wave.amplitude = 1.65; src->props.square_wave.offset = 1.65;
        src->props.square_wave.frequency = 1e6; src->props.square_wave.duty = 0.5;
        Component *gs = add_comp(circuit, COMP_GROUND, x, py + 160, 0);
        connect_terminals(circuit, src, 1, gs, 0);
        int sp = TN(x, py + 20); src->node_ids[0] = sp;
        Component *rs = hres(circuit, x + 100, py + 20, 10e3);
        int sl = TN(x + 60, py + 20), sr = TN(x + 140, py + 20);
        TW(sp, sl); rs->node_ids[0] = sl; rs->node_ids[1] = sr;
        int node = TN(x + 220, py + 20); TW(sr, node);

        Component *cstray = vcap(circuit, x + 220, py + 80, 5e-12);            // (220,20)-(220,140) wait: 40 tall each way
        int st = TN(x + 220, py + 40), sb = TN(x + 220, py + 120);
        TW(node, st); cstray->node_ids[0] = st; cstray->node_ids[1] = sb;
        Component *g1 = add_comp(circuit, COMP_GROUND, x + 220, py + 180, 0);
        g1->node_ids[0] = TN(x + 220, py + 160); TW(sb, TN(x + 220, py + 160));

        if (cprobe[k] > 0) {
            Component *rp = vres(circuit, x + 320, py + 80, rprobe[k]);
            Component *cp = vcap(circuit, x + 400, py + 80, cprobe[k]);
            int rt = TN(x + 320, py + 40), rb = TN(x + 320, py + 120);
            int ct = TN(x + 400, py + 40), cb = TN(x + 400, py + 120);
            TW(node, TN(x + 320, py + 20)); TW(TN(x + 320, py + 20), rt); TW(rt, ct); TW(rb, cb);
            rp->node_ids[0] = rt; rp->node_ids[1] = rb;
            cp->node_ids[0] = ct; cp->node_ids[1] = cb;
            Component *g2 = add_comp(circuit, COMP_GROUND, x + 320, py + 180, 0);
            g2->node_ids[0] = TN(x + 320, py + 160); TW(rb, TN(x + 320, py + 160));
        }
        add_label(circuit, x + 480, py + 80, tag[k]);
    }
    add_label(circuit, x - 40, y - 60, "PROBE LOADING: one 1 MHz driver out of 10 k, measured three ways. The probe is part of the circuit");
    add_label(circuit, x - 40, y + 820, "A probe is a capacitor you solder onto the node. On a 50 ohm node nobody notices; on a 10 k node a 1x probe's");
    add_label(circuit, x - 40, y + 850, "100 pF is twenty times the board's own stray and the edge you came to measure is the probe's edge. Rule of");
    add_label(circuit, x - 40, y + 880, "thumb: the probe must be small against the node's own C, or high against its R at the frequency of interest.");
    add_label(circuit, x - 40, y + 910, "ALSO SEE: Probe Compensation, and Ideal vs Real Capacitor for what else a real part brings with it.");
    return 20;
}

static int place_iv_ground_lead(Circuit *circuit, float x, float y) {
    /* 12 pF of probe tip resonating with the inductance of the return path */
    static const double lead[2] = { 150e-9, 15e-9 };
    static const char *tag[2] = {
        "6 inch ground clip, 150 nH: rings at 119 MHz. Every fast edge you probe will now have this on it",
        "half inch spring tip, 15 nH: 375 MHz, above the probe's own bandwidth - the ring is simply not there"
    };
    for (int k = 0; k < 2; k++) {
        float py = y + k * 300;
        Component *src = add_comp(circuit, COMP_PULSE_SOURCE, x, py + 60, 0);
        if (!src) return 0;
        src->props.pulse_source.v_low = 0; src->props.pulse_source.v_high = 3.3;
        src->props.pulse_source.pulse_width = 60e-9; src->props.pulse_source.period = 120e-9;
        src->props.pulse_source.rise_time = 1e-9; src->props.pulse_source.fall_time = 1e-9;
        Component *gs = add_comp(circuit, COMP_GROUND, x, py + 160, 0);
        connect_terminals(circuit, src, 1, gs, 0);
        int sp = TN(x, py + 20); src->node_ids[0] = sp;
        Component *rs = hres(circuit, x + 100, py + 20, 50.0);
        int sl = TN(x + 60, py + 20), sr = TN(x + 140, py + 20);
        TW(sp, sl); rs->node_ids[0] = sl; rs->node_ids[1] = sr;
        int tip = TN(x + 220, py + 20); TW(sr, tip);

        Component *ctip = vcap(circuit, x + 220, py + 80, 12e-12);
        int ct = TN(x + 220, py + 40), cb = TN(x + 220, py + 120);
        TW(tip, ct); ctip->node_ids[0] = ct; ctip->node_ids[1] = cb;
        Component *lg = add_comp(circuit, COMP_INDUCTOR, x + 220, py + 180, 90);   // (220,140)-(220,220)
        lg->props.inductor.inductance = lead[k];
        lg->props.inductor.dcr = 1.0;
        int lt = TN(x + 220, py + 140), lb = TN(x + 220, py + 220);
        TW(cb, lt); lg->node_ids[0] = lt; lg->node_ids[1] = lb;
        Component *g2 = add_comp(circuit, COMP_GROUND, x + 220, py + 260, 0);
        g2->node_ids[0] = TN(x + 220, py + 240); TW(lb, TN(x + 220, py + 240));
        add_label(circuit, x + 300, py + 120, tag[k]);
    }
    add_label(circuit, x - 40, y - 60, "GROUND LEAD RINGING: the same 1 ns edge into the same probe tip, with two different return paths");
    add_label(circuit, x - 40, y + 640, "The signal goes down the probe and the return comes back through the ground lead, and that loop has inductance.");
    add_label(circuit, x - 40, y + 670, "With the probe's 12 pF it is an LC tank: f = 1/(2 pi sqrt(L C)). A 6 inch clip puts it right in the band you are");
    add_label(circuit, x - 40, y + 700, "trying to measure, so every edge appears to ring. Nothing on the board changed - shorten the return and it stops.");
    add_label(circuit, x - 40, y + 730, "ALSO SEE: Signal Reflections and SPI Lines, where the ringing is real and the board is what has to change.");
    return 14;
}

static int place_iv_scope_input_z(Circuit *circuit, float x, float y) {
    /* a 50 ohm generator into 3 ft of 50 ohm coax (five LC sections), read two ways */
    static const double rterm[2] = { 1e6, 50.0 };
    static const char *tag[2] = {
        "1 M input: the cable end is open, so the 1 V launched doubles to 2 V. The generator is not putting out 2 V",
        "50 ohm input: matched, 1 Vpk, flat. This is the setting the generator's amplitude is calibrated into"
    };
    for (int k = 0; k < 2; k++) {
        float py = y + k * 340;
        Component *src = add_comp(circuit, COMP_PULSE_SOURCE, x, py + 60, 0);
        if (!src) return 0;
        src->props.pulse_source.v_low = 0; src->props.pulse_source.v_high = 2.0;   /* 2 V open-circuit = 1 V into 50 */
        src->props.pulse_source.pulse_width = 40e-9; src->props.pulse_source.period = 100e-9;
        src->props.pulse_source.rise_time = 1e-9; src->props.pulse_source.fall_time = 1e-9;
        Component *gs = add_comp(circuit, COMP_GROUND, x, py + 160, 0);
        connect_terminals(circuit, src, 1, gs, 0);
        int sp = TN(x, py + 20); src->node_ids[0] = sp;
        Component *rout = hres(circuit, x + 100, py + 20, 50.0);
        int sl = TN(x + 60, py + 20), sr = TN(x + 140, py + 20);
        TW(sp, sl); rout->node_ids[0] = sl; rout->node_ids[1] = sr;

        /* 3 ft of RG-58: one Delay Line, Z0 = 50 ohm and 5 ns one way. It used to be five L-C
           sections standing in for the cable; the part carries the delay itself, so the far end
           stays at zero until the wave gets there instead of ramping with the driver. */
        Component *cable = add_comp(circuit, COMP_DELAY_LINE, x + 300, py + 20, 0);   // (260,20)-(340,20)
        cable->props.delay_line.z0 = 50.0;
        cable->props.delay_line.delay = 5e-9;
        cable->props.delay_line.ideal = true;
        int ca = TN(x + 260, py + 20), cb2 = TN(x + 340, py + 20);
        TW(sr, ca);
        cable->node_ids[0] = ca; cable->node_ids[1] = cb2;
        int prev = cb2;
        Component *rin = vres(circuit, x + 460, py + 80, rterm[k]);
        int rt = TN(x + 460, py + 40), rb = TN(x + 460, py + 120);
        TW(prev, TN(x + 420, py + 20)); TW(TN(x + 420, py + 20), TN(x + 460, py + 20)); TW(TN(x + 460, py + 20), rt);
        rin->node_ids[0] = rt; rin->node_ids[1] = rb;
        Component *g2 = add_comp(circuit, COMP_GROUND, x + 460, py + 180, 0);
        g2->node_ids[0] = TN(x + 460, py + 160); TW(rb, TN(x + 460, py + 160));
        add_label(circuit, x + 540, py + 80, tag[k]);
    }
    add_label(circuit, x - 40, y - 60, "SCOPE INPUT: 1 M or 50 ohm. Same generator, same cable, two different answers");
    add_label(circuit, x - 40, y + 740, "A signal generator marked 1 V is 1 V into 50 ohms - it is a 2 V source behind 50 ohms. Terminate the cable in");
    add_label(circuit, x - 40, y + 770, "50 ohms and you read 1 V. Leave the scope on 1 M and the far end is open: the step reflects, adds to itself and");
    add_label(circuit, x - 40, y + 800, "you read exactly twice what it is sending. The classic 'my generator is out of calibration' bug report.");
    add_label(circuit, x - 40, y + 830, "ALSO SEE: Signal Reflections and Impedance Matching - the same physics, in the board rather than the bench.");
    return 12;
}

static int place_iv_ac_coupling(Circuit *circuit, float x, float y) {
    /* 12 V rail with 40 mVpp of 100 kHz ripple on it, read DC-coupled and AC-coupled */
    Component *rail = add_comp(circuit, COMP_DC_VOLTAGE, x, y + 60, 0);     // +(x,y+20) -(x,y+100)
    if (!rail) return 0;
    rail->props.dc_voltage.voltage = 12.0;
    Component *gr = add_comp(circuit, COMP_GROUND, x, y + 160, 0);
    connect_terminals(circuit, rail, 1, gr, 0);
    int rp = TN(x, y + 20); rail->node_ids[0] = rp;
    Component *ripple = add_comp(circuit, COMP_AC_VOLTAGE, x + 100, y + 20, 0);   // (60,20)-(140,20), in series
    ripple->props.ac_voltage.amplitude = 0.1; ripple->props.ac_voltage.frequency = 100e3;
    int a = TN(x + 60, y + 20), b = TN(x + 140, y + 20);
    TW(rp, a); ripple->node_ids[0] = a; ripple->node_ids[1] = b;
    int node = TN(x + 220, y + 20); TW(b, node);
    Component *rload = vres(circuit, x + 220, y + 100, 1000.0);
    int lt = TN(x + 220, y + 60), lb = TN(x + 220, y + 140);
    TW(node, lt); rload->node_ids[0] = lt; rload->node_ids[1] = lb;
    Component *gl = add_comp(circuit, COMP_GROUND, x + 220, y + 200, 0);
    gl->node_ids[0] = TN(x + 220, y + 180); TW(lb, TN(x + 220, y + 180));

    /* DC-coupled channel: straight to a 1 M input */
    Component *rdc = vres(circuit, x + 360, y + 100, 1e6);
    int dt = TN(x + 360, y + 60), db = TN(x + 360, y + 140);
    TW(node, TN(x + 360, y + 20)); TW(TN(x + 360, y + 20), dt);
    rdc->node_ids[0] = dt; rdc->node_ids[1] = db;
    Component *gd = add_comp(circuit, COMP_GROUND, x + 360, y + 200, 0);
    gd->node_ids[0] = TN(x + 360, y + 180); TW(db, TN(x + 360, y + 180));

    /* AC-coupled channel: the scope's own 0.1 uF blocking cap ahead of the same 1 M */
    Component *cblk = hcap(circuit, x + 520, y + 20, 0.1e-6);      // (480,20)-(560,20)
    int cl = TN(x + 480, y + 20), cr = TN(x + 560, y + 20);
    TW(node, cl); cblk->node_ids[0] = cl; cblk->node_ids[1] = cr;
    Component *rac = vres(circuit, x + 640, y + 100, 1e6);
    int at = TN(x + 640, y + 60), ab = TN(x + 640, y + 140);
    TW(cr, TN(x + 640, y + 20)); TW(TN(x + 640, y + 20), at);
    rac->node_ids[0] = at; rac->node_ids[1] = ab;
    Component *ga = add_comp(circuit, COMP_GROUND, x + 640, y + 200, 0);
    ga->node_ids[0] = TN(x + 640, y + 180); TW(ab, TN(x + 640, y + 180));

    add_label(circuit, x - 40, y - 60, "AC COUPLING: 200 mVpp of ripple sitting on a 12 V rail - one node, two channels");
    add_label(circuit, x + 300, y + 260, "DC-coupled: 12.00 V. At 5 V/div the ripple is a twentieth of a division");
    add_label(circuit, x + 300, y + 290, "AC-coupled: 0 V mean. Now 50 mV/div fits, and 200 mVpp is four divisions");
    add_label(circuit, x - 40, y + 340, "TRY IT: on the DC-coupled trace, work out what V/div you would need to see 200 mV on 12 V. You cannot - the rail");
    add_label(circuit, x - 40, y + 370, "would be 60 divisions off screen before the ripple is one. AC coupling is a 0.1 uF cap into the 1 M input, a");
    add_label(circuit, x - 40, y + 400, "high-pass at 1.6 Hz: it throws the DC away and keeps everything above it, so the gain can be turned all the way up.");
    add_label(circuit, x - 40, y + 430, "The cost: you can no longer read the DC level, and anything slower than a couple of Hz is attenuated or shifted.");
    add_label(circuit, x - 40, y + 460, "ALSO SEE: Power Delivery Network, where finding millivolts of ripple on a rail is the whole job.");
    return 12;
}

static int place_iv_shunt_sense(Circuit *circuit, float x, float y) {
    /* the same 12 V, 1 A load measured with a 100 mohm shunt, low side and high side */
    Component *v1 = dc_rail(circuit, x, y, 12.0); if (!v1) return 0;
    int rail1 = TN(x, y);
    Component *rl1 = vres(circuit, x + 160, y + 220, 11.9);   // (160,180)-(160,260): 1 A with the shunt in series
    rl1->props.resistor.power_rating = 15.0;                 /* a 12 W load, so rate it like one */
    int l1t = TN(x + 160, y + 180), l1b = TN(x + 160, y + 260);
    TW(rail1, TN(x + 160, y)); TW(TN(x + 160, y), l1t);
    rl1->node_ids[0] = l1t; rl1->node_ids[1] = l1b;
    Component *sh1 = vres(circuit, x + 160, y + 340, 0.1);    // (160,300)-(160,380): the LOW-SIDE shunt
    int s1t = TN(x + 160, y + 300), s1b = TN(x + 160, y + 380);
    TW(l1b, s1t); sh1->node_ids[0] = s1t; sh1->node_ids[1] = s1b;
    Component *g1 = add_comp(circuit, COMP_GROUND, x + 160, y + 440, 0);
    g1->node_ids[0] = TN(x + 160, y + 420); TW(s1b, TN(x + 160, y + 420));
    add_label(circuit, x + 240, y + 400, "LOW SIDE: 100 mV across the shunt, referred to ground - a single-ended input reads it directly.");
    add_label(circuit, x + 240, y + 430, "The price: the load's 'ground' now sits 100 mV up, and every other signal it shares is offset by that.");

    /* high side: same shunt at the top of the rail, read by a difference amp */
    float hx = x + 900;
    Component *v2 = dc_rail(circuit, hx, y, 12.0);
    int rail2 = TN(hx, y);
    Component *sh2 = hres(circuit, hx + 100, y + 100, 0.1);   // (60,100)-(140,100): HIGH-SIDE shunt
    int s2l = TN(hx + 60, y + 100), s2r = TN(hx + 140, y + 100);
    TW(rail2, TN(hx + 60, y)); TW(TN(hx + 60, y), s2l);
    sh2->node_ids[0] = s2l; sh2->node_ids[1] = s2r;
    Component *rl2 = vres(circuit, hx + 200, y + 220, 11.9);
    rl2->props.resistor.power_rating = 15.0;
    int l2t = TN(hx + 200, y + 180), l2b = TN(hx + 200, y + 260);
    TW(s2r, TN(hx + 200, y + 100)); TW(TN(hx + 200, y + 100), l2t);
    rl2->node_ids[0] = l2t; rl2->node_ids[1] = l2b;
    Component *g2 = add_comp(circuit, COMP_GROUND, hx + 200, y + 320, 0);
    g2->node_ids[0] = TN(hx + 200, y + 300); TW(l2b, TN(hx + 200, y + 300));

    /* difference amp, gain 20: 100 mV of difference on a 12 V common mode -> 2 V out */
    Component *u = add_comp(circuit, COMP_OPAMP, hx + 500, y + 140, 0);   // -(460,120) +(460,160) out(540,140)
    u->props.opamp.ideal = true; u->props.opamp.gain = 1e5;
    Component *rin1 = hres(circuit, hx + 340, y + 120, 10e3);
    Component *rin2 = hres(circuit, hx + 340, y + 160, 10e3);
    Component *rfb = hres(circuit, hx + 500, y + 40, 200e3);
    Component *rgn = vres(circuit, hx + 420, y + 240, 200e3);
    int minus = TN(hx + 460, y + 120), plus = TN(hx + 460, y + 160);
    int i1l = TN(hx + 300, y + 120), i1r = TN(hx + 380, y + 120);
    int i2l = TN(hx + 300, y + 160), i2r = TN(hx + 380, y + 160);
    /* load side to the inverting input, rail side to the non-inverting one, so a current
       flowing INTO the load comes out of the amplifier positive */
    TW(s2r, TN(hx + 140, y + 120)); TW(TN(hx + 140, y + 120), i1l);
    TW(s2l, TN(hx + 60, y + 160)); TW(TN(hx + 60, y + 160), i2l);
    TW(i1r, minus); TW(i2r, plus);
    rin1->node_ids[0] = i1l; rin1->node_ids[1] = i1r;
    rin2->node_ids[0] = i2l; rin2->node_ids[1] = i2r;
    int out = TN(hx + 540, y + 140), o1 = TN(hx + 580, y + 140), o2 = TN(hx + 580, y + 40), fr = TN(hx + 540, y + 40), fl = TN(hx + 460, y + 40);
    TW(out, o1); TW(o1, o2); TW(o2, fr); TW(fl, minus);
    rfb->node_ids[0] = fl; rfb->node_ids[1] = fr;
    int gt = TN(hx + 420, y + 200), gb = TN(hx + 420, y + 280);
    TW(plus, TN(hx + 420, y + 160)); TW(TN(hx + 420, y + 160), gt);
    rgn->node_ids[0] = gt; rgn->node_ids[1] = gb;
    Component *g3 = add_comp(circuit, COMP_GROUND, hx + 420, y + 340, 0);
    g3->node_ids[0] = TN(hx + 420, y + 320); TW(gb, TN(hx + 420, y + 320));
    u->node_ids[0] = minus; u->node_ids[1] = plus; u->node_ids[2] = out;
    add_label(circuit, hx + 620, y + 140, "HIGH SIDE: the load keeps a real ground, and a short to ground still shows up as current.");
    add_label(circuit, hx + 620, y + 170, "The price: 100 mV of signal riding on 12 V of common mode - it takes a difference amp, and the");
    add_label(circuit, hx + 620, y + 200, "CMRR of that amp is now your accuracy. Gain 20 here: 100 mV of shunt becomes 2 V at the output.");

    add_label(circuit, x - 40, y - 100, "CURRENT SENSE: high side or low side. Same 1 A, same 100 mohm shunt, two different sets of problems");
    add_label(circuit, x - 40, y + 560, "Interview answer: low side is cheap and single-ended but lifts the load's ground and cannot see a short to");
    add_label(circuit, x - 40, y + 590, "ground; high side keeps the ground clean and catches that short, but needs common-mode rejection at the rail");
    add_label(circuit, x - 40, y + 620, "voltage. Either way the shunt costs you burden voltage - 100 mV here - which is why it is not 1 ohm.");
    add_label(circuit, x - 40, y + 650, "ALSO SEE: Difference Amp and Instrumentation Amp for the amplifier, and 4-Wire Sensing for the shunt itself.");
    return 20;
}

static int place_iv_kelvin(Circuit *circuit, float x, float y) {
    /* 1 A forced through a 10 mohm shunt that has 50 mohm of lead each side */
    Component *isrc = add_comp(circuit, COMP_DC_CURRENT, x, y + 100, 0);   // (x,y+60)-(x,y+140)
    if (!isrc) return 0;
    isrc->props.dc_current.current = 1.0;
    int itop = TN(x, y + 60), ibot = TN(x, y + 140);
    isrc->node_ids[0] = ibot; isrc->node_ids[1] = itop;   /* source pushes 1 A up through the chain */
    Component *gi = add_comp(circuit, COMP_GROUND, x, y + 200, 0);
    gi->node_ids[0] = TN(x, y + 180); TW(ibot, TN(x, y + 180));

    Component *lead1 = hres(circuit, x + 140, y + 60, 0.05);      // (100,60)-(180,60)
    Component *rsh = hres(circuit, x + 300, y + 60, 0.010);       // (260,60)-(340,60): the part under test
    Component *lead2 = hres(circuit, x + 460, y + 60, 0.05);      // (420,60)-(500,60)
    int a = TN(x + 100, y + 60), b = TN(x + 180, y + 60), c = TN(x + 260, y + 60), d = TN(x + 340, y + 60), e = TN(x + 420, y + 60), f = TN(x + 500, y + 60);
    TW(itop, a); TW(b, c); TW(d, e);
    lead1->node_ids[0] = a; lead1->node_ids[1] = b;
    rsh->node_ids[0] = c; rsh->node_ids[1] = d;
    lead2->node_ids[0] = e; lead2->node_ids[1] = f;
    Component *gf = add_comp(circuit, COMP_GROUND, x + 560, y + 60, 0);
    gf->node_ids[0] = TN(x + 540, y + 60); TW(f, TN(x + 540, y + 60));

    /* 2-wire: measure at the connector, outside both leads */
    Component *m2 = vres(circuit, x + 100, y + 220, 10e6);
    int m2t = TN(x + 100, y + 180), m2b = TN(x + 100, y + 260);
    TW(a, TN(x + 100, y + 100)); TW(TN(x + 100, y + 100), m2t);
    m2->node_ids[0] = m2t; m2->node_ids[1] = m2b;
    Component *g2 = add_comp(circuit, COMP_GROUND, x + 100, y + 320, 0);
    g2->node_ids[0] = TN(x + 100, y + 300); TW(m2b, TN(x + 100, y + 300));

    /* 4-wire: two more leads land on the body of the part and go to a differential input.
       They have the same 50 mohm, but they carry no current, so they drop nothing - and the
       input has to be differential, because neither sense point is at ground. */
    Component *s1 = vres(circuit, x + 260, y + 200, 0.05);        // sense lead from the shunt's high side
    Component *s2 = vres(circuit, x + 340, y + 200, 0.05);        // sense lead from its low side
    int s1t = TN(x + 260, y + 160), s1b = TN(x + 260, y + 240);
    int s2t = TN(x + 340, y + 160), s2b = TN(x + 340, y + 240);
    TW(c, TN(x + 260, y + 100)); TW(TN(x + 260, y + 100), s1t);
    TW(d, TN(x + 340, y + 100)); TW(TN(x + 340, y + 100), s2t);
    s1->node_ids[0] = s1t; s1->node_ids[1] = s1b;
    s2->node_ids[0] = s2t; s2->node_ids[1] = s2b;

    Component *u = add_comp(circuit, COMP_OPAMP, x + 500, y + 360, 0);   // -(460,340) +(460,380) out(540,360)
    u->props.opamp.ideal = true; u->props.opamp.gain = 1e5;
    Component *ri1 = hres(circuit, x + 380, y + 340, 100e3);
    Component *ri2 = hres(circuit, x + 380, y + 380, 100e3);
    Component *rf = hres(circuit, x + 500, y + 260, 100e3);
    Component *rg = vres(circuit, x + 420, y + 460, 100e3);
    int minus = TN(x + 460, y + 340), plus = TN(x + 460, y + 380);
    int i1l = TN(x + 340, y + 340), i1r = TN(x + 420, y + 340);
    int i2l = TN(x + 340, y + 380), i2r = TN(x + 420, y + 380);
    TW(s2b, TN(x + 340, y + 300)); TW(TN(x + 340, y + 300), i1l);
    TW(s1b, TN(x + 260, y + 380)); TW(TN(x + 260, y + 380), i2l);
    TW(i1r, minus); TW(i2r, plus);
    ri1->node_ids[0] = i1l; ri1->node_ids[1] = i1r;
    ri2->node_ids[0] = i2l; ri2->node_ids[1] = i2r;
    int out = TN(x + 540, y + 360), o1 = TN(x + 580, y + 360), o2 = TN(x + 580, y + 260), fr = TN(x + 540, y + 260), fl = TN(x + 460, y + 260);
    TW(out, o1); TW(o1, o2); TW(o2, fr); TW(fl, minus);
    rf->node_ids[0] = fl; rf->node_ids[1] = fr;
    int gt = TN(x + 420, y + 420), gb = TN(x + 420, y + 500);
    TW(plus, TN(x + 420, y + 380)); TW(TN(x + 420, y + 380), gt);
    rg->node_ids[0] = gt; rg->node_ids[1] = gb;
    Component *g4 = add_comp(circuit, COMP_GROUND, x + 420, y + 560, 0);
    g4->node_ids[0] = TN(x + 420, y + 540); TW(gb, TN(x + 420, y + 540));
    u->node_ids[0] = minus; u->node_ids[1] = plus; u->node_ids[2] = out;

    add_label(circuit, x - 40, y - 60, "4-WIRE (KELVIN) SENSING: 1 A through a 10 mohm shunt with 50 mohm of lead at each end");
    add_label(circuit, x + 620, y + 220, "2-wire: 110 mV at the connector -> 110 mohm. The leads are ten times the part");
    add_label(circuit, x + 620, y + 360, "4-wire: the difference amp reads 10 mV -> 10 mohm. The sense leads carry no");
    add_label(circuit, x + 620, y + 390, "current, so their own 50 mohm drops nothing - and the reading is the part");
    add_label(circuit, x - 40, y + 620, "The force leads carry the current and the sense leads carry none, so IR drop in the sense path is zero and what");
    add_label(circuit, x - 40, y + 650, "the meter sees is the part. This is why a milliohm meter, an LCR bridge and a good shunt all have four terminals,");
    add_label(circuit, x - 40, y + 680, "and why a current-sense resistor is laid out with its sense pads inside its force pads.");
    add_label(circuit, x - 40, y + 710, "ALSO SEE: High-side vs Low-side Current Sense, and Line Drop Basics for the same IR drop at another scale.");
    return 22;
}

/* =====================================================================================
 * Interview prep: converters and power delivery.
 *
 * Buck Converter already shows that Vout = D Vin with an ideal switch. These four are the
 * questions asked ABOUT that circuit: draw me the waveform at every node of a DISCRETE
 * one, what happens at light load, why not just use a linear regulator, and how does the
 * high-side gate get above the rail.
 * =================================================================================== */

static int place_iv_buck_nodes(Circuit *circuit, float x, float y) {
    Component *vin = dc_rail(circuit, x, y, 12.0); if (!vin) return 0;      // +(x,y)
    int in = TN(x, y);

    /* high-side PMOS: source on the rail, gate held off by Rg, pulled down by an NPN.
       This is the discrete version of what a controller IC does inside. */
    Component *m = add_comp(circuit, COMP_PMOS, x + 300, y + 40, 0);        // G(280,40) D(320,20) S(320,60)
    component_apply_part(m, "IRF9540N");   /* a real power P-channel. The generic PMOS default is a
                                             small-signal device - R_DS(on) near 180 ohm - and it
                                             cannot switch an amp, which is the whole template. */
    m->props.mosfet.cgso = m->props.mosfet.cgdo = m->props.mosfet.cgbo = 0.0;
    /* Gate charge off: the displacement current through C_gs and C_gd is not reported as a
       terminal current, so leaving it on makes the gate net fail a KCL audit by the very
       current the audit cannot see. The gate-charge story is in the notes; the waveforms this
       template is about - switch node, inductor, output - do not depend on it. */
    int gate = TN(x + 280, y + 40), drain = TN(x + 320, y + 20), source = TN(x + 320, y + 60);
    TW(in, TN(x + 320, y)); TW(TN(x + 320, y), source);
    Component *rg = vres(circuit, x + 240, y - 40, 1000.0);                 // (240,-80)-(240,0)
    int rgt = TN(x + 240, y - 80), rgb = TN(x + 240, y);
    TW(in, TN(x, y - 120)); TW(TN(x, y - 120), TN(x + 240, y - 120)); TW(TN(x + 240, y - 120), rgt);
    TW(rgb, TN(x + 240, y + 40)); TW(TN(x + 240, y + 40), gate);
    rg->node_ids[0] = rgt; rg->node_ids[1] = rgb;
    rg->props.resistor.power_rating = 0.5;      /* 12 V across 1 k every time the NPN pulls it down */

    Component *q = add_comp(circuit, COMP_NPN_BJT, x + 200, y + 160, 0);    // B(180,160) C(220,140) E(220,180)
    int qb = TN(x + 180, y + 160), qc = TN(x + 220, y + 140), qe = TN(x + 220, y + 180);
    TW(gate, TN(x + 280, y + 140)); TW(TN(x + 280, y + 140), qc);
    Component *gq = add_comp(circuit, COMP_GROUND, x + 220, y + 240, 0);
    gq->node_ids[0] = TN(x + 220, y + 220); TW(qe, TN(x + 220, y + 220));
    Component *rb = hres(circuit, x + 120, y + 160, 1000.0);                // (80,160)-(160,160)
    int rbl = TN(x + 80, y + 160), rbr = TN(x + 160, y + 160);
    TW(rbr, qb); rb->node_ids[0] = rbl; rb->node_ids[1] = rbr;
    Component *pwm = add_comp(circuit, COMP_PULSE_SOURCE, x, y + 220, 0);   // +(x,y+180) -(x,y+260)
    pwm->props.pulse_source.v_low = 0; pwm->props.pulse_source.v_high = 5.0;
    pwm->props.pulse_source.period = 20e-6; pwm->props.pulse_source.pulse_width = 10e-6;
    pwm->props.pulse_source.rise_time = pwm->props.pulse_source.fall_time = 1e-6;   /* a discrete driver has real edges: 1 us of a 20 us period */
    int pp = TN(x, y + 180);
    pwm->node_ids[0] = pp;
    TW(pp, TN(x, y + 160)); TW(TN(x, y + 160), rbl);
    Component *gp = add_comp(circuit, COMP_GROUND, x, y + 320, 0);
    connect_terminals(circuit, pwm, 1, gp, 0);
    m->node_ids[0] = gate; m->node_ids[1] = drain; m->node_ids[2] = source;

    /* switching node, catch diode, and the L-C that averages it */
    int sw = TN(x + 400, y + 20); TW(drain, sw);
    Component *d = add_comp(circuit, COMP_SCHOTTKY, x + 400, y + 100, 90);  // A(400,60) K(400,140)
    int da = TN(x + 400, y + 60), dk = TN(x + 400, y + 140);
    TW(sw, da);
    d->node_ids[0] = dk; d->node_ids[1] = da;                               // cathode up to SW, anode to ground
    Component *gd = add_comp(circuit, COMP_GROUND, x + 400, y + 200, 0);
    gd->node_ids[0] = TN(x + 400, y + 180); TW(dk, TN(x + 400, y + 180));
    Component *l = hind(circuit, x + 520, y + 20, 220e-6);                  // (480,20)-(560,20)
    l->props.inductor.current = 0.9;   /* start at the current it will settle at: a cold start is a
                                          10 A L-C inrush that has nothing to do with the lesson */
    int ll = TN(x + 480, y + 20), lr = TN(x + 560, y + 20);
    TW(sw, ll); l->node_ids[0] = ll; l->node_ids[1] = lr;
    int out = TN(x + 640, y + 20); TW(lr, out);
    Component *ldb = out_stage(circuit, x + 640, y + 20, out, 100e-6, 6.0, 0.0);
    ldb->props.resistor.power_rating = 30.0;    /* 5 W steady - and it rings to 12 V before the L-C settles */

    add_label(circuit, x - 40, y - 200, "DISCRETE BUCK, NODE BY NODE: 12 V in, 50 % duty at 50 kHz, 5.5 V out - and every node has its own answer");
    add_label(circuit, x + 60, y + 380, "GATE: sits at 12 V (off). The NPN pulls it to 0.2 V to turn the PMOS on - Vgs = -11.8 V.");
    add_label(circuit, x + 60, y + 410, "SWITCH NODE: an 11.3 V square when the PMOS conducts, and -0.4 V when the Schottky freewheels.");
    add_label(circuit, x + 60, y + 440, "It is the only truly hard-switching node in the circuit, and the only one worth an EMI argument.");
    add_label(circuit, x + 60, y + 470, "INDUCTOR: the square, integrated. Current ramps up at (Vin-Vout)/L and down at Vout/L: a triangle.");
    add_label(circuit, x + 60, y + 500, "OUTPUT: the triangle, integrated again. dI = (Vin-Vout) D/(L f) = 270 mA; dV = dI/(8 C f) = 7 mV.");
    add_label(circuit, x + 60, y + 530, "ALSO SEE: Buck Converter for the ideal-switch version, and Input vs Output Capacitance.");
    return 20;
}

static int place_iv_ldo_vs_buck(Circuit *circuit, float x, float y) {
    /* 12 V in, 5 V out, 1 A. The same job done two ways, with the input current measured */
    Component *v1 = dc_rail(circuit, x, y, 12.0); if (!v1) return 0;
    int in1 = TN(x, y);
    Component *sh1 = add_comp(circuit, COMP_AMMETER, x + 100, y, 0);       // (60,0)-(140,0): input current
    int s1l = TN(x + 60, y), s1r = TN(x + 140, y);
    TW(in1, s1l); sh1->node_ids[0] = s1l; sh1->node_ids[1] = s1r;
    Component *cin1 = vcap(circuit, x + 170, y + 80, 10e-6);               // (170,40)-(170,120): the input cap every regulator needs
    int ci1t = TN(x + 170, y + 40), ci1b = TN(x + 170, y + 120);
    TW(s1r, TN(x + 170, y)); TW(TN(x + 170, y), ci1t);
    cin1->node_ids[0] = ci1t; cin1->node_ids[1] = ci1b;
    Component *gci1 = add_comp(circuit, COMP_GROUND, x + 170, y + 180, 0);
    gci1->node_ids[0] = TN(x + 170, y + 160); TW(ci1b, TN(x + 170, y + 160));
    Component *reg = add_comp(circuit, COMP_7805, x + 240, y, 0);          // IN(200,0) OUT(280,0) GND(240,30)
    int ri = TN(x + 200, y), ro = TN(x + 280, y), rgn = TN(x + 240, y + 30);
    TW(s1r, ri);
    reg->node_ids[0] = ri; reg->node_ids[1] = ro; reg->node_ids[2] = rgn;
    Component *gr = add_comp(circuit, COMP_GROUND, x + 240, y + 100, 0);
    gr->node_ids[0] = TN(x + 240, y + 80); TW(rgn, TN(x + 240, y + 80));
    int o1 = TN(x + 360, y); TW(ro, o1);
    Component *ld1 = out_stage(circuit, x + 360, y, o1, 10e-6, 5.0, 5.0);
    ld1->props.resistor.power_rating = 10.0;      /* a 5 W load */

    /* the same 5 V from a switcher: 12 V chopped at 42 % duty */
    float by = y + 360;
    Component *v2 = dc_rail(circuit, x, by, 12.0);
    int in2 = TN(x, by);
    Component *sh2 = add_comp(circuit, COMP_AMMETER, x + 100, by, 0);
    int s2l = TN(x + 60, by), s2r = TN(x + 140, by);
    TW(in2, s2l); sh2->node_ids[0] = s2l; sh2->node_ids[1] = s2r;
    /* the switcher's input capacitor. Without it the 100 mohm sense resistor carries the full
       triangular switch current instead of the average, which is exactly the mistake the
       Input vs Output Capacitance template is about. */
    Component *cin2 = vcap(circuit, x + 170, by + 80, 10e-6);
    int ci2t = TN(x + 170, by + 40), ci2b = TN(x + 170, by + 120);
    TW(s2r, TN(x + 170, by)); TW(TN(x + 170, by), ci2t);
    cin2->node_ids[0] = ci2t; cin2->node_ids[1] = ci2b;
    Component *gci2 = add_comp(circuit, COMP_GROUND, x + 170, by + 180, 0);
    gci2->node_ids[0] = TN(x + 170, by + 160); TW(ci2b, TN(x + 170, by + 160));
    Component *sw = pwm_switch(circuit, x + 240, by, 100e3, 0.44);
    int si = TN(x + 200, by), so = TN(x + 280, by); TW(s2r, si);
    sw->node_ids[0] = si; sw->node_ids[1] = so;
    int node_sw = TN(x + 340, by); TW(so, node_sw);
    Component *d = add_comp(circuit, COMP_SCHOTTKY, x + 340, by + 80, 90);
    int da = TN(x + 340, by + 40), dk = TN(x + 340, by + 120);
    TW(node_sw, da);
    d->node_ids[0] = dk; d->node_ids[1] = da;
    Component *gd = add_comp(circuit, COMP_GROUND, x + 340, by + 180, 0);
    gd->node_ids[0] = TN(x + 340, by + 160); TW(dk, TN(x + 340, by + 160));
    Component *l = hind(circuit, x + 440, by, 47e-6);
    int ll = TN(x + 400, by), lr = TN(x + 480, by); TW(node_sw, ll);
    l->node_ids[0] = ll; l->node_ids[1] = lr;
    int o2 = TN(x + 560, by); TW(lr, o2);
    Component *ld2 = out_stage(circuit, x + 560, by, o2, 220e-6, 5.0, 5.0);
    ld2->props.resistor.power_rating = 15.0;   /* 4.8 W steady, and it rings a little on the way there */

    add_label(circuit, x - 40, y - 80, "LDO vs SWITCHER: 12 V in, 5 V out, 1 A out. Read the two input ammeters and compare them");
    add_label(circuit, x + 560, y + 60, "LINEAR: 1 A in for 1 A out. The other 7 V leaves as 7 W of heat - 42 %");
    add_label(circuit, x + 760, by + 60, "SWITCHING: 440 mA in for the same 1 A out - about 90 %, and no heatsink");
    add_label(circuit, x - 40, by + 320, "ALSO SEE: 7805 Regulator, LM317 Adj Reg, Buck Converter, Input vs Output Capacitance.");
    return 30;
}

static int place_iv_bootstrap(Circuit *circuit, float x, float y) {
    /* A high-side N-channel gate has to go ABOVE the rail. The bootstrap capacitor is how,
       and this shows what it costs: it only refills while the switch node is low. */
    static const double duty[2] = { 0.5, 0.999 };
    static const double period[2] = { 10e-6, 100e-3 };  /* the second one simply never comes back down */
    static const char *tag[2] = {
        "SWITCHING at 50 %: every time SW goes low the diode refills C_boot to 11.5 V, so BOOT rides",
        "STUCK ON: SW never goes low, the diode never conducts, and the driver's own current drains"
    };
    for (int k = 0; k < 2; k++) {
        float py = y + k * 320;
        Component *rail = dc_rail(circuit, x, py, 12.0); if (!rail) return 0;
        int vdd = TN(x, py);
        Component *d = add_comp(circuit, COMP_DIODE, x + 160, py, 0);      // A(120,0) K(200,0)
        int da = TN(x + 120, py), dk = TN(x + 200, py);
        TW(vdd, da); d->node_ids[0] = da; d->node_ids[1] = dk;
        int boot = TN(x + 280, py); TW(dk, boot);

        Component *cb = vcap(circuit, x + 280, py + 100, 100e-9);          // (280,60)-(280,140)
        int cbt = TN(x + 280, py + 60), cbb = TN(x + 280, py + 140);
        TW(boot, cbt); cb->node_ids[0] = cbt; cb->node_ids[1] = cbb;
        /* the driver's quiescent current, as the load that empties the cap */
        Component *rq = vres(circuit, x + 380, py + 100, 2e3);
        int rqt = TN(x + 380, py + 60), rqb = TN(x + 380, py + 140);
        TW(cbt, rqt); TW(cbb, rqb);
        rq->node_ids[0] = rqt; rq->node_ids[1] = rqb;

        /* the switch node itself, driven as the power stage would drive it */
        Component *swn = add_comp(circuit, COMP_PULSE_SOURCE, x + 280, py + 220, 0);  // +(280,180) -(280,260)
        swn->props.pulse_source.v_low = 0; swn->props.pulse_source.v_high = 12.0;
        swn->props.pulse_source.period = period[k];
        swn->props.pulse_source.pulse_width = duty[k] * period[k];
        swn->props.pulse_source.rise_time = swn->props.pulse_source.fall_time = 1e-6;
        int swp = TN(x + 280, py + 180);
        swn->node_ids[0] = swp;
        TW(cbb, swp);
        Component *gs = add_comp(circuit, COMP_GROUND, x + 280, py + 320, 0);
        connect_terminals(circuit, swn, 1, gs, 0);
        add_label(circuit, x + 460, py + 100, tag[k]);
    }
    add_label(circuit, x - 40, y - 80, "BOOTSTRAP HIGH-SIDE DRIVE: C_boot floats on the switch node so the gate can go above the 12 V rail");
    add_label(circuit, x + 460, y + 130, "to 23.5 V while SW is high. The gate always has 11.5 V above its own source.");
    add_label(circuit, x + 460, y + 450, "it to nothing. BOOT falls to SW, Vgs goes to zero and the high-side turns itself off.");
    add_label(circuit, x - 40, y + 700, "This is why an N-channel high-side driver has a maximum duty cycle, and why a bootstrapped buck cannot");
    add_label(circuit, x - 40, y + 730, "run at 100 %: it has to put the switch node on the floor often enough to refill the capacitor. A design that");
    add_label(circuit, x - 40, y + 760, "must reach 100 % needs a charge pump or an isolated supply instead. INTERVIEW: asked whenever a candidate");
    add_label(circuit, x - 40, y + 790, "says 'N-channel on top' - the follow-up is always 'so how do you drive its gate?'");
    add_label(circuit, x - 40, y + 820, "ALSO SEE: High-side PMOS Switch, which sidesteps the whole problem, and Discrete Buck, Node by Node.");
    return 16;
}

/* =====================================================================================
 * Interview prep: I/O, termination and signal integrity.
 *
 * Signal Reflections shows a line terminated or not; SPI Lines and RS-485 show real buses.
 * These five are the questions asked about them: what SERIES termination does that parallel
 * does not, how you pick a pull-up, and the two ways a neighbouring signal ruins yours.
 * =================================================================================== */

/* A 3.3 V CMOS driver into a metre of 50 ohm coax: one Delay Line part, 5 ns one way. This
   used to be five L-C sections approximating the same cable; the part carries the delay in its
   own history instead, so the far end stays at exactly zero until the wave arrives and the
   time step no longer has to be short against a section. Returns the far-end node. */
static int fast_line(Circuit *circuit, float x, float y, int src, double rs_val, Component **rs_out) {
    Component *rs = hres(circuit, x + 60, y, rs_val);
    int sl = TN(x + 20, y), sr = TN(x + 100, y);
    TW(src, sl); rs->node_ids[0] = sl; rs->node_ids[1] = sr;
    if (rs_out) *rs_out = rs;
    Component *ln = add_comp(circuit, COMP_DELAY_LINE, x + 220, y, 0);   // (180,0)-(260,0)
    ln->props.delay_line.z0 = 50.0;
    ln->props.delay_line.delay = 5e-9;
    ln->props.delay_line.ideal = true;
    int a = TN(x + 180, y), b = TN(x + 260, y);
    TW(sr, a);
    ln->node_ids[0] = a; ln->node_ids[1] = b;
    return b;
}

static int place_iv_termination(Circuit *circuit, float x, float y) {
    /* one driver, one line, three ways of ending it */
    static const double rser[3] = { 25.0, 58.0, 25.0 };   /* driver alone, driver + 33 series, driver alone */
    static const double rpar[3] = { 0.0,  0.0,  50.0 };
    static const char *tag[3] = {
        "NONE: the far end is open, so the 2.2 V the driver launched arrives and doubles to 4.4 V, and the",
        "SERIES 33 ohm at the SOURCE: 25 + 33 = 58 ohm matches the line, so the reflection that comes",
        "PARALLEL 50 ohm at the LOAD: nothing reflects at all, the cleanest edge of the three - and the"
    };
    for (int k = 0; k < 3; k++) {
        float py = y + k * 320;
        Component *drv = add_comp(circuit, COMP_PULSE_SOURCE, x, py + 60, 0);   // +(x,py+20) -(x,py+100)
        if (!drv) return 0;
        drv->props.pulse_source.v_low = 0; drv->props.pulse_source.v_high = 3.3;
        drv->props.pulse_source.pulse_width = 20e-9; drv->props.pulse_source.period = 40e-9;
        drv->props.pulse_source.rise_time = drv->props.pulse_source.fall_time = 200e-12;   /* 200 ps: far shorter than the 1.5 ns the line takes, which is when termination starts to matter */
        Component *gd = add_comp(circuit, COMP_GROUND, x, py + 160, 0);
        connect_terminals(circuit, drv, 1, gd, 0);
        int sp = TN(x, py + 20); drv->node_ids[0] = sp;
        int far = fast_line(circuit, x, py + 20, sp, rser[k], NULL);
        int rx = TN(x + 1060, py + 20); TW(far, rx);
        if (rpar[k] > 0) {
            Component *rt = vres(circuit, x + 1060, py + 80, rpar[k]);
            int rtt = TN(x + 1060, py + 40), rtb = TN(x + 1060, py + 120);
            TW(rx, rtt); rt->node_ids[0] = rtt; rt->node_ids[1] = rtb;
            Component *gt = add_comp(circuit, COMP_GROUND, x + 1060, py + 180, 0);
            gt->node_ids[0] = TN(x + 1060, py + 160); TW(rtb, TN(x + 1060, py + 160));
        }
        Component *crx = vcap(circuit, x + 1160, py + 80, 10e-12);   /* the receiver's input pin */
        int ct = TN(x + 1160, py + 40), cb = TN(x + 1160, py + 120);
        TW(rx, TN(x + 1160, py + 20)); TW(TN(x + 1160, py + 20), ct);
        crx->node_ids[0] = ct; crx->node_ids[1] = cb;
        Component *gr = add_comp(circuit, COMP_GROUND, x + 1160, py + 180, 0);
        gr->node_ids[0] = TN(x + 1160, py + 160); TW(cb, TN(x + 1160, py + 160));
        add_label(circuit, x + 1240, py + 60, tag[k]);
    }
    add_label(circuit, x - 40, y - 60, "TERMINATION: the same 3.3 V driver into the same 50 ohm line, ended three ways");
    add_label(circuit, x + 1240, y + 420, "back is absorbed there. The far end still doubles the step, which is exactly right: the");
    add_label(circuit, x + 1240, y + 460, "receiver is high impedance, so the incident half plus the reflected half make the full 3.3 V.");
    add_label(circuit, x + 1240, y + 750, "receiver only ever sees 3.3 x 50/75 = 2.2 V, and the driver holds 44 mA the whole time it is high.");
    add_label(circuit, x - 40, y + 1000, "The interview answer is the trade: SERIES costs nothing at DC and one line delay of latency, and it works");
    add_label(circuit, x - 40, y + 1030, "for exactly one receiver at the far end. PARALLEL works for a bus with receivers along it, and costs DC");
    add_label(circuit, x - 40, y + 1060, "current and amplitude forever. NONE is fine when the edge is slow next to the round trip - the real rule is");
    add_label(circuit, x - 40, y + 1090, "terminate when the rise time is shorter than about twice the one-way delay, which here is 10 ns.");
    add_label(circuit, x - 40, y + 1120, "ALSO SEE: Signal Reflections, Impedance Matching, SPI Lines, RS-485 Differential Link.");
    return 42;
}

static int place_iv_pullup_sizing(Circuit *circuit, float x, float y) {
    /* the same open-drain bus, three pull-ups, 400 pF of bus capacitance */
    static const double rp[3] = { 10e3, 4.7e3, 1e3 };
    static const char *tag[3] = {
        "10 k: rise time 2.2 R C = 8.8 us. Only 330 uA of sink current, but the bus cannot run at 400 kHz",
        "4.7 k: 4.1 us, 700 uA. The I2C default, and it is a compromise, not a magic number",
        "1 k: 880 ns, 3.3 mA. Fast enough for fast-mode, but check the sink spec: 3 mA is a lot for some parts"
    };
    for (int k = 0; k < 3; k++) {
        float py = y + k * 560;
        Component *rail = dc_rail(circuit, x, py, 3.3); if (!rail) return 0;
        int vdd = TN(x, py);
        Component *r = vres(circuit, x + 200, py + 60, rp[k]);            // (200,py+20)-(200,py+100)
        int rt = TN(x + 200, py + 20), rb = TN(x + 200, py + 100);
        TW(vdd, TN(x + 200, py)); TW(TN(x + 200, py), rt);
        r->node_ids[0] = rt; r->node_ids[1] = rb;
        int bus = TN(x + 200, py + 140); TW(rb, bus);

        Component *cb = vcap(circuit, x + 320, py + 200, 400e-12);        // (320,py+160)-(320,py+240)
        int ct = TN(x + 320, py + 160), cbm = TN(x + 320, py + 240);
        TW(bus, TN(x + 320, py + 140)); TW(TN(x + 320, py + 140), ct);
        cb->node_ids[0] = ct; cb->node_ids[1] = cbm;
        Component *gc = add_comp(circuit, COMP_GROUND, x + 320, py + 300, 0);
        gc->node_ids[0] = TN(x + 320, py + 280); TW(cbm, TN(x + 320, py + 280));

        /* the open-drain device that pulls the bus down */
        Component *m = add_comp(circuit, COMP_NMOS, x + 460, py + 160, 0);  // G(440,160) D(480,140) S(480,180)
        component_apply_part(m, "2N7000");
        m->props.mosfet.cgso = m->props.mosfet.cgdo = m->props.mosfet.cgbo = m->props.mosfet.cj = 0.0;
        /* gate charge off: this template is about the pull-up, and the gate's displacement
           current is not reported as a terminal current, so leaving it on makes the bus net
           fail a KCL audit by exactly the current the audit cannot see */
        int gate = TN(x + 440, py + 160), drain = TN(x + 480, py + 140), srcn = TN(x + 480, py + 180);
        TW(TN(x + 320, py + 140), TN(x + 480, py + 140));   /* through the junction, not across it: a wire that passes over a node makes the flow display guess */
        m->node_ids[0] = gate; m->node_ids[1] = drain; m->node_ids[2] = srcn;
        Component *gm = add_comp(circuit, COMP_GROUND, x + 480, py + 240, 0);
        gm->node_ids[0] = TN(x + 480, py + 220); TW(srcn, TN(x + 480, py + 220));
        Component *pw = add_comp(circuit, COMP_PULSE_SOURCE, x + 340, py + 380, 0);  // +(340,340) -(340,420)
        pw->props.pulse_source.v_low = 0; pw->props.pulse_source.v_high = 5.0;
        pw->props.pulse_source.period = 40e-6; pw->props.pulse_source.pulse_width = 20e-6;
        pw->props.pulse_source.rise_time = pw->props.pulse_source.fall_time = 1e-6;
        int pp = TN(x + 340, py + 340); pw->node_ids[0] = pp;
        TW(pp, TN(x + 340, py + 320)); TW(TN(x + 340, py + 320), TN(x + 440, py + 320)); TW(TN(x + 440, py + 320), gate);
        Component *gp = add_comp(circuit, COMP_GROUND, x + 340, py + 480, 0);
        connect_terminals(circuit, pw, 1, gp, 0);
        add_label(circuit, x + 560, py + 160, tag[k]);
    }
    add_label(circuit, x - 40, y - 60, "PULL-UP SIZING: one open-drain bus with 400 pF on it, three pull-ups, 25 kHz of switching");
    add_label(circuit, x - 40, y + 1720, "An open-drain pin can only pull down. The rise is the pull-up charging the bus capacitance, so the whole");
    add_label(circuit, x - 40, y + 1750, "choice is a trade between rise time (2.2 R C to get from 10 % to 90 %) and the current the pin has to sink");
    add_label(circuit, x - 40, y + 1780, "while it holds the bus low. I2C specifies both ends of it: 300 ns maximum rise for fast mode, 3 mA maximum");
    add_label(circuit, x - 40, y + 1810, "sink. With 400 pF of bus you cannot satisfy both, which is why the specification also caps the bus at 400 pF.");
    add_label(circuit, x - 40, y + 1840, "ALSO SEE: I2C Bus (wired-AND), Open-Drain + Pull-up, I2C Level Shifter.");
    return 33;
}

static int place_iv_ground_bounce(Circuit *circuit, float x, float y) {
    /* one driver switching hard, one quiet line, one shared return inductance */
    /* The driver is a pulse source, so there is no separate rail to draw - one used to be
       placed here and its + terminal connected to nothing at all. */
    Component *drv = add_comp(circuit, COMP_PULSE_SOURCE, x + 160, y + 60, 0);   // +(160,20) -(160,100)
    if (!drv) return 0;
    drv->props.pulse_source.v_low = 0; drv->props.pulse_source.v_high = 3.3;
    drv->props.pulse_source.pulse_width = 20e-9; drv->props.pulse_source.period = 50e-9;
    drv->props.pulse_source.rise_time = drv->props.pulse_source.fall_time = 1e-9;
    int dp = TN(x + 160, y + 20), dn = TN(x + 160, y + 100);
    drv->node_ids[0] = dp; drv->node_ids[1] = dn;
    Component *rout = hres(circuit, x + 280, y + 20, 10.0);       // (240,20)-(320,20)
    rout->props.resistor.power_rating = 1.0;
    int rl = TN(x + 240, y + 20), rr = TN(x + 320, y + 20);
    TW(dp, rl); rout->node_ids[0] = rl; rout->node_ids[1] = rr;
    int load = TN(x + 400, y + 20); TW(rr, load);
    Component *cl = vcap(circuit, x + 400, y + 80, 100e-12);      // (400,40)-(400,120)
    int clt = TN(x + 400, y + 40), clb = TN(x + 400, y + 120);
    TW(load, clt); cl->node_ids[0] = clt; cl->node_ids[1] = clb;

    /* The load capacitor returns to the BOARD ground - it is off-chip. The driver returns to the
       chip's own ground, and that reaches the board through one 5 nH bond wire. So the charging
       current has to cross the bond wire, which is the entire point. */
    Component *gload = add_comp(circuit, COMP_GROUND, x + 480, y + 180, 0);
    gload->node_ids[0] = TN(x + 480, y + 160);
    TW(clb, TN(x + 480, y + 120)); TW(TN(x + 480, y + 120), TN(x + 480, y + 160));
    int localgnd = TN(x + 400, y + 200);
    TW(dn, TN(x + 160, y + 200)); TW(TN(x + 160, y + 200), localgnd);
    Component *lb = add_comp(circuit, COMP_INDUCTOR, x + 400, y + 280, 90);   // (400,240)-(400,320)
    lb->props.inductor.inductance = 5e-9; lb->props.inductor.dcr = 0.05;
    int lt = TN(x + 400, y + 240), lbm = TN(x + 400, y + 320);
    TW(localgnd, lt); lb->node_ids[0] = lt; lb->node_ids[1] = lbm;
    Component *gb = add_comp(circuit, COMP_GROUND, x + 400, y + 380, 0);
    gb->node_ids[0] = TN(x + 400, y + 360); TW(lbm, TN(x + 400, y + 360));

    /* the quiet output: a pin held low, referred to the SAME local ground */
    Component *rq = vres(circuit, x + 620, y + 120, 50.0);        // (620,80)-(620,160)
    rq->props.resistor.power_rating = 1.0;
    int rqt = TN(x + 620, y + 80), rqb = TN(x + 620, y + 160);
    TW(rqb, TN(x + 620, y + 200)); TW(TN(x + 620, y + 200), localgnd);
    rq->node_ids[0] = rqt; rq->node_ids[1] = rqb;
    Component *cq = vcap(circuit, x + 760, y + 120, 10e-12);      // (760,80)-(760,160)
    int cqt = TN(x + 760, y + 80), cqb = TN(x + 760, y + 160);
    TW(rqt, TN(x + 760, y + 80)); cq->node_ids[0] = cqt; cq->node_ids[1] = cqb;
    Component *gq = add_comp(circuit, COMP_GROUND, x + 760, y + 220, 0);
    gq->node_ids[0] = TN(x + 760, y + 200); TW(cqb, TN(x + 760, y + 200));

    add_label(circuit, x - 40, y - 60, "GROUND BOUNCE: one driver switching 100 pF in a nanosecond, and one quiet pin sharing its return");
    add_label(circuit, x + 840, y + 100, "The quiet pin is 'low'. Measured against the board's ground it is not:");
    add_label(circuit, x + 840, y + 130, "it moves with the local ground, because that is what it is referred to.");
    add_label(circuit, x + 480, y + 280, "5 nH of bond wire and via: the local ground swings 2.2 V pk-pk.");
    add_label(circuit, x - 40, y + 460, "Nothing here is broken. The driver charges 100 pF to 3.3 V through 10 ohm, so 330 mA flows for about a");
    add_label(circuit, x - 40, y + 490, "nanosecond, and all of it goes home through the shared 5 nH: L di/dt swings the local ground 2.2 V pk-pk.");
    add_label(circuit, x - 40, y + 520, "Every other signal on that die is referred to the lifted ground, so a pin that is holding low appears to");
    add_label(circuit, x - 40, y + 550, "pulse - and a receiver with a 0.8 V threshold may believe it. The fixes are all the same fix: fewer ways for");
    add_label(circuit, x - 40, y + 580, "the current to be shared. More ground pins, shorter returns, a plane instead of a trace, slower edges.");
    add_label(circuit, x - 40, y + 610, "ALSO SEE: Power Delivery Network for the same story on the supply side, and Ground Lead Ringing for the");
    add_label(circuit, x - 40, y + 640, "measurement version of it - where the inductance you are fighting is in your own probe.");
    return 13;
}

static int place_iv_crosstalk(Circuit *circuit, float x, float y) {
    /* one aggressor, two victims: the difference is what drives the victim */
    static const double rv[2] = { 10e3, 10.0 };
    static const char *tag[2] = {
        "WEAK victim (10 k pull-down): 2 pF of coupling against 5 pF of victim capacitance is a divider.",
        "STRONG victim (10 ohm driver): the same charge arrives and is swallowed in 70 ps. No glitch,"
    };
    for (int k = 0; k < 2; k++) {
        float py = y + k * 320;
        Component *agg = add_comp(circuit, COMP_PULSE_SOURCE, x, py + 60, 0);
        if (!agg) return 0;
        agg->props.pulse_source.v_low = 0; agg->props.pulse_source.v_high = 3.3;
        agg->props.pulse_source.pulse_width = 20e-9; agg->props.pulse_source.period = 50e-9;
        agg->props.pulse_source.rise_time = agg->props.pulse_source.fall_time = 1e-9;
        Component *ga = add_comp(circuit, COMP_GROUND, x, py + 160, 0);
        connect_terminals(circuit, agg, 1, ga, 0);
        int ap = TN(x, py + 20); agg->node_ids[0] = ap;
        Component *ra = hres(circuit, x + 100, py + 20, 25.0);
        int al = TN(x + 60, py + 20), ar = TN(x + 140, py + 20);
        TW(ap, al); ra->node_ids[0] = al; ra->node_ids[1] = ar;
        int agn = TN(x + 240, py + 20); TW(ar, agn);
        Component *ca = vcap(circuit, x + 240, py + 80, 5e-12);           /* the aggressor's own trace C */
        int cat = TN(x + 240, py + 40), cab = TN(x + 240, py + 120);
        TW(agn, cat); ca->node_ids[0] = cat; ca->node_ids[1] = cab;
        Component *gca = add_comp(circuit, COMP_GROUND, x + 240, py + 180, 0);
        gca->node_ids[0] = TN(x + 240, py + 160); TW(cab, TN(x + 240, py + 160));

        /* the coupling between the two traces: 2 pF of running side by side */
        Component *cm = hcap(circuit, x + 360, py + 20, 2e-12);           // (320,20)-(400,20)
        int cml = TN(x + 320, py + 20), cmr = TN(x + 400, py + 20);
        TW(agn, cml); cm->node_ids[0] = cml; cm->node_ids[1] = cmr;
        int vic = TN(x + 500, py + 20); TW(cmr, vic);

        Component *cv = vcap(circuit, x + 500, py + 80, 5e-12);
        int cvt = TN(x + 500, py + 40), cvb = TN(x + 500, py + 120);
        TW(vic, cvt); cv->node_ids[0] = cvt; cv->node_ids[1] = cvb;
        Component *gcv = add_comp(circuit, COMP_GROUND, x + 500, py + 180, 0);
        gcv->node_ids[0] = TN(x + 500, py + 160); TW(cvb, TN(x + 500, py + 160));
        Component *rvv = vres(circuit, x + 620, py + 80, rv[k]);
        int rvt = TN(x + 620, py + 40), rvb = TN(x + 620, py + 120);
        TW(vic, TN(x + 620, py + 20)); TW(TN(x + 620, py + 20), rvt);
        rvv->node_ids[0] = rvt; rvv->node_ids[1] = rvb;
        Component *grv = add_comp(circuit, COMP_GROUND, x + 620, py + 180, 0);
        grv->node_ids[0] = TN(x + 620, py + 160); TW(rvb, TN(x + 620, py + 160));
        add_label(circuit, x + 700, py + 80, tag[k]);
    }
    add_label(circuit, x - 40, y - 60, "CROSSTALK: one aggressor edge, 2 pF of coupling, and two victims that differ only in what holds them");
    add_label(circuit, x + 700, y + 110, "The victim jumps 3.3 x 2/7 = 0.94 V and takes 10 k x 7 pF = 70 ns to bleed away.");
    add_label(circuit, x + 700, y + 140, "That is a logic level, on a line nothing is driving. This is how a floating input");
    add_label(circuit, x + 700, y + 170, "or a slow reset line picks up its neighbour and glitches.");
    add_label(circuit, x + 700, y + 430, "and nothing to debug. The lesson is not 'avoid coupling' - it is that coupling only");
    add_label(circuit, x + 700, y + 460, "becomes a fault when the victim's impedance lets it become one.");
    add_label(circuit, x - 40, y + 700, "The coupled charge is the same in both copies: C_m dV = 2 pF x 3.3 V = 6.6 pC. What differs is where it");
    add_label(circuit, x - 40, y + 730, "goes. Into 5 pF of victim capacitance with only 10 k to drain it, that charge is nearly a volt for 70 ns.");
    add_label(circuit, x - 40, y + 760, "Into a 10 ohm driver it is gone before the aggressor edge has finished. Hence: drive your quiet nets, do");
    add_label(circuit, x - 40, y + 790, "not leave inputs floating, and give the aggressor a return path close to it so its field stays local.");
    add_label(circuit, x - 40, y + 820, "ALSO SEE: GPIO Input + Debounce (a real pull-up on a real input), Signal Reflections.");
    return 24;
}

static int place_iv_esd_clamp(Circuit *circuit, float x, float y) {
    /* an input pin with rail clamps, overdriven two ways */
    static const double rlim[2] = { 1e3, 220e3 };
    static const char *tag[2] = {
        "1 k series: the pin clamps at 4.0 V and 2.7 mA flows into the 3.3 V rail. That is more than the",
        "220 k series: the same 6 V outside, 12 uA in. Under every data sheet's injection limit, and the"
    };
    for (int k = 0; k < 2; k++) {
        float py = y + k * 360;
        Component *rail = dc_rail(circuit, x, py, 3.3); if (!rail) return 0;
        int vdd = TN(x, py);
        Component *ext = add_comp(circuit, COMP_DC_VOLTAGE, x + 160, py + 220, 0);   // +(160,180) -(160,260)
        ext->props.dc_voltage.voltage = 6.0;
        Component *ge = add_comp(circuit, COMP_GROUND, x + 160, py + 320, 0);
        connect_terminals(circuit, ext, 1, ge, 0);
        int ep = TN(x + 160, py + 180); ext->node_ids[0] = ep;
        Component *rl = vres(circuit, x + 160, py + 100, rlim[k]);       // (160,60)-(160,140)
        int rlt = TN(x + 160, py + 60), rlb = TN(x + 160, py + 140);
        TW(ep, rlb); rl->node_ids[0] = rlt; rl->node_ids[1] = rlb;
        int pin = TN(x + 300, py + 60); TW(rlt, pin);

        /* the two clamp diodes every CMOS input has: pin to VDD, ground to pin */
        Component *d1 = add_comp(circuit, COMP_DIODE, x + 300, py - 40, 90);   // A(300,-80) K(300,0) -> anode down
        int d1a = TN(x + 300, py - 80), d1k = TN(x + 300, py);
        d1->node_ids[0] = d1k; d1->node_ids[1] = d1a;                    /* anode at the pin, cathode at VDD */
        TW(pin, TN(x + 300, py + 20)); TW(TN(x + 300, py + 20), d1k);
        /* py - 120 lands inside the copy above: run the rail just above this copy instead */
        TW(d1a, TN(x, py - 80)); TW(TN(x, py - 80), vdd);
        Component *d2 = add_comp(circuit, COMP_DIODE, x + 420, py + 120, 90);  // A(420,80) K(420,160)
        int d2a = TN(x + 420, py + 80), d2k = TN(x + 420, py + 160);
        d2->node_ids[0] = d2k; d2->node_ids[1] = d2a;                    /* anode at ground, cathode at the pin */
        TW(pin, TN(x + 420, py + 60)); TW(TN(x + 420, py + 60), d2a);
        Component *gd2 = add_comp(circuit, COMP_GROUND, x + 420, py + 220, 0);
        gd2->node_ids[0] = TN(x + 420, py + 200); TW(d2k, TN(x + 420, py + 200));
        Component *rin = vres(circuit, x + 560, py + 120, 1e6);          /* the gate the pin drives */
        int rit = TN(x + 560, py + 80), rib = TN(x + 560, py + 160);
        TW(pin, TN(x + 560, py + 60)); TW(TN(x + 560, py + 60), rit);
        rin->node_ids[0] = rit; rin->node_ids[1] = rib;
        Component *gri = add_comp(circuit, COMP_GROUND, x + 560, py + 220, 0);
        gri->node_ids[0] = TN(x + 560, py + 200); TW(rib, TN(x + 560, py + 200));
        add_label(circuit, x + 660, py + 120, tag[k]);
    }
    add_label(circuit, x - 40, y - 200, "ESD CLAMPS: 6 V applied to a 3.3 V input, through 1 k and through 220 k");
    add_label(circuit, x + 660, y + 150, "10 uA to 20 mA a data sheet allows, and it is being sourced INTO the rail:");
    add_label(circuit, x + 660, y + 180, "on a lightly loaded board that alone can pull the whole 3.3 V supply up.");
    add_label(circuit, x + 660, y + 510, "pin still reads a solid high. The resistor costs bandwidth, and nothing else.");
    add_label(circuit, x - 40, y + 760, "Every CMOS input has a diode to each rail. They exist to survive an ESD strike, not to be a voltage");
    add_label(circuit, x - 40, y + 790, "clamp you design around: hold one on and you are pushing current into the supply, which is why a signal");
    add_label(circuit, x - 40, y + 820, "that is live while a board is off can back-power the whole thing through one input pin. INTERVIEW: 'what");
    add_label(circuit, x - 40, y + 850, "happens if you drive 5 V into a 3.3 V input?' - and the follow-up, 'so how do you make that safe?'");
    add_label(circuit, x - 40, y + 880, "ALSO SEE: UART 5 V <-> 3.3 V and I2C Level Shifter for the ways you are supposed to do it.");
    return 20;
}

/* =====================================================================================
 * Interview prep: analog fundamentals.
 *
 * The four questions that come up whatever the job is. Thevenin Equivalent, RC Step
 * Response and Cascode already cover the textbook versions; these are the ones where the
 * textbook answer and the interview answer are different.
 * =================================================================================== */

static int place_iv_cap_energy(Circuit *circuit, float x, float y) {
    /* C1 charged to 10 V, switched onto an equal empty C2. Twice, through very different
       resistances, to make the point that the resistance is not what decides the loss. */
    static const double rt[2] = { 1.0, 100.0 };
    static const char *tag[2] = {
        "1 ohm: the transfer takes 100 us. Both end at 5 V, and 2.5 mJ of the 5 mJ has gone",
        "100 ohm: 10 ms instead - and exactly the same 2.5 mJ has gone. R sets the time, not the loss"
    };
    for (int k = 0; k < 2; k++) {
        float py = y + k * 320;
        Component *c1 = vcap(circuit, x, py + 100, 100e-6);              // (x,py+60)-(x,py+140)
        if (!c1) return 0;
        c1->props.capacitor.voltage = 10.0;                             /* charged before the switch closes */
        int c1t = TN(x, py + 60), c1b = TN(x, py + 140);
        c1->node_ids[0] = c1t; c1->node_ids[1] = c1b;
        Component *g1 = add_comp(circuit, COMP_GROUND, x, py + 200, 0);
        g1->node_ids[0] = TN(x, py + 180); TW(c1b, TN(x, py + 180));

        /* An NMOS, turned so its channel lies along the row and its gate hangs below: the drain
           on the left where the charged capacitor is, the source on the right. K is set so the
           channel is rt[k] at the middle of the transfer, where V_gs is about 12.5 V. */
        Component *sw = add_comp(circuit, COMP_NMOS, x + 180, py + 40, 270);   // D(160,20) S(200,20) G(180,60)
        sw->props.mosfet.vth = 1.0;
        sw->props.mosfet.kp = 1.0 / (11.5 * rt[k]);
        sw->props.mosfet.lambda = 0.0;
        sw->props.mosfet.ideal = true;              /* square law, no parasitics: this is the switch */
        int si = TN(x + 160, py + 20), so = TN(x + 200, py + 20), ctl = TN(x + 180, py + 60);
        TW(c1t, TN(x, py + 20)); TW(TN(x, py + 20), si);
        sw->node_ids[1] = si; sw->node_ids[2] = so; sw->node_ids[0] = ctl;   /* D, S, G */
        Component *pw = add_comp(circuit, COMP_PULSE_SOURCE, x + 180, py + 140, 0);  // +(180,100) -(180,180)
        /* 15 V, not 5: an NMOS pass transistor stops conducting when its source reaches
           V_gate - V_th, so a gate at the rail would leave the transfer unfinished. This is
           what a gate driver or a bootstrap is for. */
        pw->props.pulse_source.v_low = 0; pw->props.pulse_source.v_high = 15.0;
        pw->props.pulse_source.delay = 20e-3;      /* close it well after the scope has started */
        pw->props.pulse_source.rise_time = pw->props.pulse_source.fall_time = 1e-6;
        pw->props.pulse_source.pulse_width = 10.0; pw->props.pulse_source.period = 100.0;
        int pp = TN(x + 180, py + 100); pw->node_ids[0] = pp;
        TW(ctl, pp);   /* gate straight down to the pulse source */
        Component *gp = add_comp(circuit, COMP_GROUND, x + 180, py + 240, 0);
        connect_terminals(circuit, pw, 1, gp, 0);

        Component *c2 = vcap(circuit, x + 340, py + 100, 100e-6);
        int c2t = TN(x + 340, py + 60), c2b = TN(x + 340, py + 140);
        TW(so, TN(x + 340, py + 20)); TW(TN(x + 340, py + 20), c2t);
        c2->node_ids[0] = c2t; c2->node_ids[1] = c2b;
        Component *g2 = add_comp(circuit, COMP_GROUND, x + 340, py + 200, 0);
        g2->node_ids[0] = TN(x + 340, py + 180); TW(c2b, TN(x + 340, py + 180));
        add_label(circuit, x + 420, py + 100, tag[k]);
    }
    add_label(circuit, x - 40, y - 60, "THE TWO-CAPACITOR PROBLEM: 100 uF charged to 10 V, switched onto an equal empty one at t = 20 ms");
    add_label(circuit, x - 40, y + 700, "Before: 1/2 C V^2 = 1/2 x 100 uF x 100 = 5 mJ. After: charge is conserved, so both sit at 5 V, and the");
    add_label(circuit, x - 40, y + 730, "energy is 2 x 1/2 x 100 uF x 25 = 2.5 mJ. Half of it is gone, and it is gone for ANY resistance, including");
    add_label(circuit, x - 40, y + 760, "one so small the transfer takes microseconds: the integral of i^2 R dt is independent of R, because a");
    add_label(circuit, x - 40, y + 790, "smaller R makes the current bigger by exactly as much as it shortens the time. Make R zero and the energy");
    add_label(circuit, x - 40, y + 820, "still leaves - as radiation from a loop carrying an enormous current. The same algebra says a capacitor");
    add_label(circuit, x - 40, y + 850, "charged from a fixed voltage source always wastes half of what the source delivered, which is why a");
    add_label(circuit, x - 40, y + 880, "switching pre-regulator exists at all. INTERVIEW: asked at TI, and the follow-up is 'where did it go?'");
    add_label(circuit, x - 40, y + 910, "ALSO SEE: RC Step Response, and Input vs Output Capacitance.");
    return 16;
}

static int place_iv_miller(Circuit *circuit, float x, float y) {
    /* the same common-source stage twice; the second one has its C_gd made explicit */
    static const double cgd[2] = { 0.0, 10e-12 };
    static const char *tag[2] = {
        "NO C_gd: the 10 k source sees only the gate capacitance, and the stage does its full gain at 1 MHz",
        "10 pF from drain to gate: the input sees 10 pF x (1 + 12) = 130 pF, and 10 k x 130 pF rolls off at"
    };
    for (int k = 0; k < 2; k++) {
        float py = y + k * 400;
        Component *vdd = dc_rail(circuit, x + 300, py - 160, 12.0); if (!vdd) return 0;
        int rail = TN(x + 300, py - 160);
        Component *vin = add_comp(circuit, COMP_AC_VOLTAGE, x, py + 60, 0);   // +(x,py+20) -(x,py+100)
        vin->props.ac_voltage.amplitude = 0.05; vin->props.ac_voltage.frequency = 1e6;
        Component *gi = add_comp(circuit, COMP_GROUND, x, py + 160, 0);
        connect_terminals(circuit, vin, 1, gi, 0);
        int sp = TN(x, py + 20); vin->node_ids[0] = sp;
        Component *rs = hres(circuit, x + 100, py + 20, 10e3);               /* the source impedance that Miller works against */
        int rsl = TN(x + 60, py + 20), rsr = TN(x + 140, py + 20);
        TW(sp, rsl); rs->node_ids[0] = rsl; rs->node_ids[1] = rsr;
        /* the signal has to be coupled in: without this the 10 k source drags the bias divider
           down to a tenth of a volt and the stage is simply off */
        Component *cc = hcap(circuit, x + 180, py + 20, 100e-9);             // (140,20)-(220,20)
        int ccl = TN(x + 140, py + 20), ccr = TN(x + 220, py + 20);
        TW(rsr, ccl); cc->node_ids[0] = ccl; cc->node_ids[1] = ccr;
        int gate = ccr;

        /* bias: 1M / 330k from the rail, so the gate sits at 3 V */
        Component *rb1 = vres(circuit, x + 220, py - 60, 1e6);
        int b1t = TN(x + 220, py - 100), b1b = TN(x + 220, py - 20);
        TW(rail, TN(x + 220, py - 160)); TW(TN(x + 220, py - 160), b1t); TW(b1b, gate);
        rb1->node_ids[0] = b1t; rb1->node_ids[1] = b1b;
        Component *rb2 = vres(circuit, x + 220, py + 100, 330e3);
        int b2t = TN(x + 220, py + 60), b2b = TN(x + 220, py + 140);
        TW(gate, b2t); rb2->node_ids[0] = b2t; rb2->node_ids[1] = b2b;
        Component *gb = add_comp(circuit, COMP_GROUND, x + 220, py + 200, 0);
        gb->node_ids[0] = TN(x + 220, py + 180); TW(b2b, TN(x + 220, py + 180));

        Component *m = add_comp(circuit, COMP_NMOS, x + 320, py + 20, 0);    // G(300,20) D(340,0) S(340,40)
        m->props.mosfet.vth = 1.5; m->props.mosfet.kp = 3.8e-3;
        m->props.mosfet.w = 1e-6; m->props.mosfet.l = 1e-6;   /* W/L = 1: the stamp multiplies kp by W/L, and
                                                                 the default 10 um device would be ten times
                                                                 this transconductance and sit in triode */
        m->props.mosfet.lambda = 0.02; m->props.mosfet.ideal = false;
        int mg = TN(x + 300, py + 20), md = TN(x + 340, py), ms = TN(x + 340, py + 40);
        int gtap = TN(x + 260, py + 20);          /* the C_gd tap, on the gate run */
        TW(gate, gtap); TW(gtap, mg);
        m->node_ids[0] = mg; m->node_ids[1] = md; m->node_ids[2] = ms;
        Component *gs = add_comp(circuit, COMP_GROUND, x + 340, py + 100, 0);
        gs->node_ids[0] = TN(x + 340, py + 80); TW(ms, TN(x + 340, py + 80));
        Component *rd = vres(circuit, x + 340, py - 80, 2200.0);             // (340,-120)-(340,-40)
        int rdt = TN(x + 340, py - 120), rdb = TN(x + 340, py - 40);
        TW(rail, TN(x + 340, py - 160)); TW(TN(x + 340, py - 160), rdt);
        TW(rdb, md);
        rd->node_ids[0] = rdt; rd->node_ids[1] = rdb;

        if (cgd[k] > 0) {
            Component *cm = add_comp(circuit, COMP_CAPACITOR, x + 300, py - 200, 0);   // (260,-200)-(340,-200)
            cm->props.capacitor.capacitance = cgd[k];
            int cml = TN(x + 260, py - 200), cmr = TN(x + 340, py - 200);
            /* tap the gate one column right of the bias resistors: running up x + 220 would
               take this wire straight over the 1M, which is where the bias divider lives */
            TW(gtap, TN(x + 260, py - 200)); TW(TN(x + 260, py - 200), cml);
            TW(md, TN(x + 400, py)); TW(TN(x + 400, py), TN(x + 400, py - 200)); TW(TN(x + 400, py - 200), cmr);
            cm->node_ids[0] = cml; cm->node_ids[1] = cmr;
        }
        add_label(circuit, x + 480, py + 20, tag[k]);
    }
    add_label(circuit, x - 40, y - 300, "THE MILLER EFFECT: two identical common-source stages driven at 1 MHz. The second has a 10 pF C_gd");
    add_label(circuit, x + 480, y + 450, "122 kHz, so at 1 MHz most of the signal never reaches the gate at all.");
    add_label(circuit, x - 40, y + 700, "A capacitor between the input and the output of an inverting stage is not a capacitor of C to the input:");
    add_label(circuit, x - 40, y + 730, "while the input moves up by v the far end moves DOWN by A v, so the charge that has to be supplied is");
    add_label(circuit, x - 40, y + 760, "C (1 + A) v. The input sees C (1 + A), and with a gain of 10 a harmless 10 pF becomes 110 pF. That is why");
    add_label(circuit, x - 40, y + 790, "gain and bandwidth trade against each other in a single stage, and why the cascode exists: putting a");
    add_label(circuit, x - 40, y + 820, "common-gate device above the common-source one holds the drain still, so A across C_gd is about 1.");
    add_label(circuit, x - 40, y + 850, "ALSO SEE: Cascode (MOSFET) for the fix, and Common Source for the stage without the argument.");
    return 22;
}

static int place_iv_switch_choice(Circuit *circuit, float x, float y) {
    /* the same 100 ohm load switched by a saturated BJT and by a MOSFET */
    Component *v1 = dc_rail(circuit, x, y, 12.0); if (!v1) return 0;
    int rail1 = TN(x, y);
    Component *rl1 = vres(circuit, x + 200, y + 80, 100.0);        // (200,40)-(200,120)
    rl1->props.resistor.power_rating = 5.0;
    int l1t = TN(x + 200, y + 40), l1b = TN(x + 200, y + 120);
    TW(rail1, TN(x + 200, y)); TW(TN(x + 200, y), l1t);
    rl1->node_ids[0] = l1t; rl1->node_ids[1] = l1b;
    Component *q = add_comp(circuit, COMP_NPN_BJT, x + 220, y + 200, 0);   // B(200,200) C(240,180) E(240,220)
    component_apply_part(q, "2N3904");
    int qb = TN(x + 200, y + 200), qc = TN(x + 240, y + 180), qe = TN(x + 240, y + 220);
    TW(l1b, TN(x + 200, y + 160)); TW(TN(x + 200, y + 160), TN(x + 240, y + 160)); TW(TN(x + 240, y + 160), qc);
    q->node_ids[0] = qb; q->node_ids[1] = qc; q->node_ids[2] = qe;
    Component *gq = add_comp(circuit, COMP_GROUND, x + 240, y + 280, 0);
    gq->node_ids[0] = TN(x + 240, y + 260); TW(qe, TN(x + 240, y + 260));
    Component *rb = hres(circuit, x + 100, y + 200, 470.0);        // (60,200)-(140,200)
    int rbl = TN(x + 60, y + 200), rbr = TN(x + 140, y + 200);
    TW(rbr, qb); rb->node_ids[0] = rbl; rb->node_ids[1] = rbr;
    Component *d1 = dc_rail(circuit, x - 100, y + 200, 5.0);
    TW(TN(x - 100, y + 200), rbl);

    /* the same job with a logic-level MOSFET */
    float mx = x + 700;
    Component *v2 = dc_rail(circuit, mx, y, 12.0);
    int rail2 = TN(mx, y);
    Component *rl2 = vres(circuit, mx + 200, y + 80, 100.0);
    rl2->props.resistor.power_rating = 5.0;
    int l2t = TN(mx + 200, y + 40), l2b = TN(mx + 200, y + 120);
    TW(rail2, TN(mx + 200, y)); TW(TN(mx + 200, y), l2t);
    rl2->node_ids[0] = l2t; rl2->node_ids[1] = l2b;
    Component *m = add_comp(circuit, COMP_NMOS, mx + 220, y + 200, 0);
    component_apply_part(m, "2N7000");
    int mg = TN(mx + 200, y + 200), md = TN(mx + 240, y + 180), ms = TN(mx + 240, y + 220);
    TW(l2b, TN(mx + 200, y + 160)); TW(TN(mx + 200, y + 160), TN(mx + 240, y + 160)); TW(TN(mx + 240, y + 160), md);
    m->node_ids[0] = mg; m->node_ids[1] = md; m->node_ids[2] = ms;
    Component *gm = add_comp(circuit, COMP_GROUND, mx + 240, y + 280, 0);
    gm->node_ids[0] = TN(mx + 240, y + 260); TW(ms, TN(mx + 240, y + 260));
    Component *rg = hres(circuit, mx + 100, y + 200, 470.0);
    int rgl = TN(mx + 60, y + 200), rgr = TN(mx + 140, y + 200);
    TW(rgr, mg); rg->node_ids[0] = rgl; rg->node_ids[1] = rgr;
    Component *d2 = dc_rail(circuit, mx - 100, y + 200, 5.0);
    TW(TN(mx - 100, y + 200), rgl);
    (void)d1; (void)d2;

    add_label(circuit, x - 40, y - 100, "BJT OR MOSFET AS A SWITCH: the same 12 V, 100 ohm load, the same 5 V of logic to drive it");
    add_label(circuit, x + 320, y + 360, "2N3904 saturated: V_CE(sat) about 0.2 V, so the load gets 11.8 V.");
    add_label(circuit, x + 320, y + 390, "It costs 9 mA of base current the whole time it is on, forever.");
    add_label(circuit, mx + 320, y + 180, "2N7000 at V_GS = 5 V: about 3.4 ohm, so 0.41 V. The data sheet's 1.2 ohm");
    add_label(circuit, mx + 320, y + 210, "is at V_GS = 10 V - and the gate takes current only while it changes.");
    add_label(circuit, x - 40, y + 420, "The interview answer is not 'MOSFETs are better'. A saturated BJT holds a fixed 0.2 V whatever the current,");
    add_label(circuit, x - 40, y + 450, "so at low current it wins on drop and at high current it loses badly - the MOSFET's drop is I x R_DS(on) and");
    add_label(circuit, x - 40, y + 480, "falls with the current. The BJT costs continuous base current, which matters on a battery; the MOSFET costs");
    add_label(circuit, x - 40, y + 510, "gate charge per edge, which matters at 100 kHz. And 'logic level' is a real specification: a 2N7000 needs");
    add_label(circuit, x - 40, y + 540, "V_GS = 4.5 V to be properly on, and an IRF540N would still be half off at 5 V.");
    add_label(circuit, x - 40, y + 570, "ALSO SEE: Named Parts: MOSFET Switches, Low-side Switch + Flyback, Common Emitter.");
    return 20;
}

static int place_tline_real(Circuit *circuit, float x, float y) {
    /* One driver, one 5 ns cable, three ways of ending it - the same experiment as the
       Termination template, but with a line that actually delays rather than five L-C
       sections pretending to. The difference is visible: here the far end stays at zero
       for a full 5 ns and then steps, instead of ramping the moment the driver moves. */
    static const double rl[3] = { 50.0, 0.0, 1e6 };     /* matched, short, open */
    static const char *tag[3] = {
        "MATCHED (50 ohm at the far end): the launched half arrives once, 5 ns later, and stays. Nothing reflects",
        "SHORTED: the far end cannot move, so it reflects with -1 and drives the source end back to zero at 10 ns",
        "OPEN: the far end reflects with +1 and doubles to the full 2 V; the source end sees it arrive at 10 ns"
    };
    for (int k = 0; k < 3; k++) {
        float py = y + k * 260;
        Component *drv = add_comp(circuit, COMP_PULSE_SOURCE, x, py + 60, 0);   // +(x,py+20) -(x,py+100)
        if (!drv) return 0;
        drv->props.pulse_source.v_low = 0; drv->props.pulse_source.v_high = 2.0;
        drv->props.pulse_source.pulse_width = 40e-9; drv->props.pulse_source.period = 80e-9;
        drv->props.pulse_source.rise_time = drv->props.pulse_source.fall_time = 250e-12;
        Component *gd = add_comp(circuit, COMP_GROUND, x, py + 160, 0);
        connect_terminals(circuit, drv, 1, gd, 0);
        int sp = TN(x, py + 20); drv->node_ids[0] = sp;

        Component *rs = hres(circuit, x + 100, py + 20, 50.0);      // (60,20)-(140,20)
        int sl = TN(x + 60, py + 20), sr = TN(x + 140, py + 20);
        TW(sp, sl); rs->node_ids[0] = sl; rs->node_ids[1] = sr;

        Component *ln = add_comp(circuit, COMP_DELAY_LINE, x + 280, py + 20, 0);   // (240,20)-(320,20)
        ln->props.delay_line.z0 = 50.0;
        ln->props.delay_line.delay = 5e-9;      /* about a metre of coax */
        ln->props.delay_line.ideal = true;
        int near = TN(x + 240, py + 20), far = TN(x + 320, py + 20);
        TW(sr, near);
        ln->node_ids[0] = near; ln->node_ids[1] = far;

        int rx = TN(x + 420, py + 20); TW(far, rx);
        Component *rt = vres(circuit, x + 420, py + 80, rl[k] > 0 ? rl[k] : 0.001);
        rt->props.resistor.power_rating = 5.0;
        int rtt = TN(x + 420, py + 40), rtb = TN(x + 420, py + 120);
        TW(rx, rtt); rt->node_ids[0] = rtt; rt->node_ids[1] = rtb;
        Component *gt = add_comp(circuit, COMP_GROUND, x + 420, py + 180, 0);
        gt->node_ids[0] = TN(x + 420, py + 160); TW(rtb, TN(x + 420, py + 160));
        add_label(circuit, x + 500, py + 60, tag[k]);
    }
    add_label(circuit, x - 40, y - 60, "TRANSMISSION LINE WITH A REAL DELAY: one 50 ohm, 5 ns cable, ended three ways");
    add_label(circuit, x - 40, y + 780, "The Delay Line part is not a ladder of Ls and Cs: it carries what each end launched and hands it to the other");
    add_label(circuit, x - 40, y + 810, "end one delay later (Bergeron's method), so the delay is a property of the cable rather than of the time step.");
    add_label(circuit, x - 40, y + 840, "Watch the far end: it sits at exactly zero for 5 ns and then steps. A lumped approximation cannot do that -");
    add_label(circuit, x - 40, y + 870, "it starts moving the instant the driver does, and it needs the solver to take steps far shorter than the delay.");
    add_label(circuit, x - 40, y + 900, "ALSO SEE: Signal Reflections and Termination, which use the ladder, and Scope Input: 1 M vs 50 ohm.");
    return 21;
}

static int place_iv_inrush(Circuit *circuit, float x, float y) {
    /* the same bulk capacitor hot-plugged onto the same supply, with and without a limiter */
    static const double rlim[2] = { 0.0, 4.7 };
    static const char *tag[2] = {
        "STRAIGHT IN: 50 mohm of connector and wiring is all that limits it. 12 / 0.05 = 240 A for the",
        "THROUGH 4.7 ohm: 2.5 A peak, and the cap is charged in 20 ms. That is what an inrush limiter is -"
    };
    for (int k = 0; k < 2; k++) {
        float py = y + k * 300;
        Component *v = add_comp(circuit, COMP_DC_VOLTAGE, x, py + 60, 0);   // +(x,py+20) -(x,py+100)
        if (!v) return 0;
        v->props.dc_voltage.voltage = 12.0;
        v->props.dc_voltage.r_series = 0.01;      /* the supply's own output impedance */
        v->props.dc_voltage.ideal = false;
        Component *gv = add_comp(circuit, COMP_GROUND, x, py + 160, 0);
        connect_terminals(circuit, v, 1, gv, 0);
        int sp = TN(x, py + 20); v->node_ids[0] = sp;

        /* the connector itself. Closing it IS the hot plug, and its on-resistance is the only
           thing between 12 V and an empty capacitor. */
        Component *sw = add_comp(circuit, COMP_ANALOG_SWITCH, x + 160, py + 20, 0);  // IN(120,20) OUT(200,20) CTL(160,40)
        sw->props.analog_switch.r_on = (rlim[k] > 0) ? rlim[k] : 0.05;
        sw->props.analog_switch.r_off = 1e9; sw->props.analog_switch.v_on = 2.5;
        sw->props.analog_switch.ideal = false;
        int si = TN(x + 120, py + 20), so = TN(x + 200, py + 20), ctl = TN(x + 160, py + 40);
        TW(sp, si);
        sw->node_ids[0] = si; sw->node_ids[1] = so; sw->node_ids[2] = ctl;
        Component *pw = add_comp(circuit, COMP_PULSE_SOURCE, x + 160, py + 140, 0);  // +(160,100) -(160,180)
        pw->props.pulse_source.v_low = 0; pw->props.pulse_source.v_high = 5.0;
        pw->props.pulse_source.delay = 2e-3;
        pw->props.pulse_source.rise_time = pw->props.pulse_source.fall_time = 1e-6;
        pw->props.pulse_source.pulse_width = 10.0; pw->props.pulse_source.period = 100.0;
        int pp = TN(x + 160, py + 100); pw->node_ids[0] = pp;
        TW(ctl, TN(x + 160, py + 80)); TW(TN(x + 160, py + 80), pp);
        Component *gpw = add_comp(circuit, COMP_GROUND, x + 160, py + 240, 0);
        connect_terminals(circuit, pw, 1, gpw, 0);
        int bulk = TN(x + 300, py + 20); TW(so, bulk);
        Component *cb = vcap(circuit, x + 300, py + 100, 1000e-6);
        int cbt = TN(x + 300, py + 60), cbb = TN(x + 300, py + 140);
        TW(bulk, cbt); cb->node_ids[0] = cbt; cb->node_ids[1] = cbb;
        Component *gc = add_comp(circuit, COMP_GROUND, x + 300, py + 200, 0);
        gc->node_ids[0] = TN(x + 300, py + 180); TW(cbb, TN(x + 300, py + 180));
        Component *rload = vres(circuit, x + 420, py + 100, 100.0);
        int rlt = TN(x + 420, py + 60), rlb = TN(x + 420, py + 140);
        TW(bulk, TN(x + 420, py + 20)); TW(TN(x + 420, py + 20), rlt);
        rload->props.resistor.power_rating = 5.0;
        rload->node_ids[0] = rlt; rload->node_ids[1] = rlb;
        Component *gl = add_comp(circuit, COMP_GROUND, x + 420, py + 200, 0);
        gl->node_ids[0] = TN(x + 420, py + 180); TW(rlb, TN(x + 420, py + 180));
        add_label(circuit, x + 520, py + 100, tag[k]);
    }
    add_label(circuit, x - 40, y - 60, "HOT-PLUG INRUSH: 1000 uF of bulk capacitance meeting a 12 V supply, twice");
    add_label(circuit, x + 520, y + 130, "first 50 us, which is what welds a connector pin and browns out everything");
    add_label(circuit, x + 520, y + 160, "else on the rail. The capacitor is charged in 250 us, which nobody asked for.");
    add_label(circuit, x + 520, y + 430, "usually an NTC thermistor, so it is 4.7 ohm cold and a few tenths once current");
    add_label(circuit, x + 520, y + 460, "has warmed it, or a MOSFET whose gate is ramped by an RC (a hot-swap controller).");
    add_label(circuit, x - 40, y + 640, "An empty capacitor is a short circuit, and the only thing between 12 V and that short is whatever series");
    add_label(circuit, x - 40, y + 670, "resistance happens to exist. The energy is not the problem - it is 1/2 C V^2 = 72 mJ either way - the");
    add_label(circuit, x - 40, y + 700, "problem is the peak current and how briefly it flows: 240 A through a connector rated for 5 A pits the");
    add_label(circuit, x - 40, y + 730, "contacts a little every time, and the rail sag takes everything else on the board down with it.");
    add_label(circuit, x - 40, y + 760, "ALSO SEE: Power Delivery Network, and The Two-Capacitor Problem for where the other half of the energy goes.");
    return 15;
}

/* === 7-SEGMENT SEGMENT TEST ===
   One switch and one resistor per segment, so every one of the eight can be lit and blanked by
   hand. The display had no template at all before this, which is how it kept a renderer that
   was never handed the segment currents and therefore lit nothing, whatever it was driven with.
   a,b,c,d leave the package on the left and e,f,g,DP on the right, so each side fans out to its
   own column of drivers: the pin nearest the top turns first and travels furthest up, which is
   what keeps the eight feeds from crossing one another. */
static int place_sevenseg_test(Circuit *circuit, float x, float y) {
    Component *disp = add_comp(circuit, COMP_7SEG_DISPLAY, x, y, 0);
    if (!disp) return 0;
    disp->props.seven_seg.common_cathode = true;

    add_label(circuit, x - 150, y - 320, "7-SEGMENT SEGMENT TEST - every segment on its own switch");

    /* terminal index on the package, and which side of it that pin comes out on */
    static const struct { int term; int side; const char *name; } seg[8] = {
        { 0, -1, "a" }, { 1, -1, "b" }, { 2, -1, "c" }, { 3, -1, "d" },
        { 5, +1, "e" }, { 6, +1, "f" }, { 7, +1, "g" }, { 8, +1, "DP" },
    };

    Component *gnd = add_comp(circuit, COMP_GROUND, x - 40, y + 160, 0);
    int gnd_node = TN(x - 40, y + 140);
    gnd->node_ids[0] = gnd_node;

    /* COM straight down to ground - common cathode, so every segment returns through it */
    float com_x, com_y;
    component_get_terminal_pos(disp, 4, &com_x, &com_y);
    TW(TN(com_x, com_y), gnd_node);
    disp->node_ids[4] = gnd_node;

    for (int i = 0; i < 8; i++) {
        int s = seg[i].side;
        int k = i % 4;                              /* position within its side, top to bottom */
        float px, py;
        component_get_terminal_pos(disp, seg[i].term, &px, &py);
        float tx = x + s * (80.0f + 20.0f * k);     /* turn column: innermost pin turns first */
        float ry = y - 180.0f + 40.0f * k;          /* driver row: innermost pin goes highest */

        Component *r = add_comp(circuit, COMP_RESISTOR, x + s * 280.0f, ry, 0);
        r->props.resistor.resistance = 150.0;       /* (5 - 2.2) / 150 = 19 mA, a normal segment */
        Component *sw = add_comp(circuit, COMP_SPST_SWITCH, x + s * 420.0f, ry, 0);
        sw->props.switch_spst.closed = true;        /* all eight lit on load; open one to blank it */

        int pin = TN(px, py);
        disp->node_ids[seg[i].term] = pin;
        TW(pin, TN(tx, py));
        TW(TN(tx, py), TN(tx, ry));
        TW(TN(tx, ry), TN(x + s * 240.0f, ry));

        int r_in = (s < 0) ? 1 : 0;                 /* the resistor terminal facing the display */
        r->node_ids[r_in] = TN(x + s * 240.0f, ry);
        r->node_ids[1 - r_in] = TN(x + s * 320.0f, ry);
        TW(TN(x + s * 320.0f, ry), TN(x + s * 380.0f, ry));

        int sw_in = (s < 0) ? 1 : 0;
        sw->node_ids[sw_in] = TN(x + s * 380.0f, ry);
        sw->node_ids[1 - sw_in] = TN(x + s * 460.0f, ry);
        TW(TN(x + s * 460.0f, ry), TN(x + s * 540.0f, ry));

        add_label(circuit, tx - 12.0f, ry - 24.0f, seg[i].name);   /* top of this pin's turn column, above its driver row */

        /* Rail column, drawn as a chain between the rows that tap it. A node sitting on top of
           a wire is not a connection here - only wire ends and coincident nodes are - so the
           rail has to be built one segment at a time. */
        if (k > 0) TW(TN(x + s * 540.0f, ry - 40.0f), TN(x + s * 540.0f, ry));
    }

    /* The two rails join over the top of the display */
    TW(TN(x - 540, y - 180), TN(x - 540, y - 260));
    TW(TN(x - 540, y - 260), TN(x + 540, y - 260));
    TW(TN(x + 540, y - 260), TN(x + 540, y - 180));

    /* 5 V supply at the foot of the left rail */
    Component *v5 = add_comp(circuit, COMP_DC_VOLTAGE, x - 540, y + 20, 0);
    v5->props.dc_voltage.voltage = 5.0;
    int vpos = TN(x - 540, y - 20);
    v5->node_ids[0] = vpos;
    TW(vpos, TN(x - 540, y - 60));
    TW(TN(x - 540, y + 60), TN(x - 540, y + 140));
    TW(TN(x - 540, y + 140), gnd_node);
    v5->node_ids[1] = gnd_node;

    add_label(circuit, x - 150, y + 210, "Open any switch and that segment goes out. All eight closed lights the 8 and the point.");
    add_label(circuit, x - 150, y + 240, "Each segment is a diode to the common cathode: 150 ohm sets 19 mA, and the glow follows the current.");
    return 12;
}

/* One digit: a BCD decoder at (dx,dy) and a 7-segment display to the right of it, wired
   together and to the counter that feeds it. Six of these make a clock, which is why it is a
   helper rather than another 200 lines of duplicated wiring.

   a,b,c,d come out of the left of the display and e,f,g out of the right, so a-d run straight
   across on a descending staircase of turn columns (each one turns further left than the one
   above it, which is what stops them meeting) and e,f,g wrap underneath and come back up on the
   far side. The wrap costs a few crossings; three wires reversing around a body cannot avoid it. */
static Component *digit_block(Circuit *circuit, float dx, float dy, Component *cnt) {
    Component *dec = add_comp(circuit, COMP_BCD_DECODER, dx, dy, 0);
    if (!dec) return NULL;
    dec->props.bcd_decoder.active_low = false;   /* drive the segment high to light it */
    Component *disp = add_comp(circuit, COMP_7SEG_DISPLAY, dx + 300, dy, 0);
    if (!disp) return NULL;
    disp->props.seven_seg.common_cathode = true;

    /* counter Q0..Q3 straight into A..D - same pitch, same rows */
    for (int b = 0; b < 4; b++) {
        float qx, qy, ax, ay;
        component_get_terminal_pos(cnt, 2 + b, &qx, &qy);
        component_get_terminal_pos(dec, b, &ax, &ay);
        int q = TN(qx, qy), a = TN(ax, ay);
        TW(q, a);
        cnt->node_ids[2 + b] = q;
        dec->node_ids[b] = q;
    }

    /* COM of the display to the ground the rest of the circuit already uses */
    float comx, comy;
    component_get_terminal_pos(disp, 4, &comx, &comy);
    int com = TN(comx, comy);
    disp->node_ids[4] = com;
    TW(com, TN(comx, dy + 200));

    /* segments a,b,c,d: decoder rows -60,-40,-20,0 into display rows -40,-20,0,+20 */
    static const int left_seg[4] = { 0, 1, 2, 3 };
    for (int i = 0; i < 4; i++) {
        float ox, oy, px, py;
        component_get_terminal_pos(dec, 4 + i, &ox, &oy);          /* decoder a,b,c,d */
        component_get_terminal_pos(disp, left_seg[i], &px, &py);   /* display a,b,c,d */
        float turn = dx + 220.0f - 20.0f * i;                      /* descending staircase */
        int o = TN(ox, oy), p = TN(px, py);
        TW(o, TN(turn, oy));
        TW(TN(turn, oy), TN(turn, py));
        TW(TN(turn, py), p);
        dec->node_ids[4 + i] = o;
        disp->node_ids[left_seg[i]] = o;
    }

    /* segments e,f,g: down, under the display, and back up the far side */
    static const int right_seg[3] = { 5, 6, 7 };
    for (int i = 0; i < 3; i++) {
        float ox, oy, px, py;
        component_get_terminal_pos(dec, 8 + i, &ox, &oy);           /* decoder e,f,g */
        component_get_terminal_pos(disp, right_seg[i], &px, &py);   /* display e,f,g */
        /* Clear of the decoder body, clear of the a-d staircase, under the display and back up
           on the far side. e keeps to the right of f and g the whole way, which is the order
           their pins are in at both ends. */
        float drop = dx + 100.0f - 20.0f * i;
        float bus  = dy + 90.0f + 20.0f * i;
        float rise = dx + 420.0f - 20.0f * i;
        int o = TN(ox, oy), p = TN(px, py);
        TW(o, TN(drop, oy));
        TW(TN(drop, oy), TN(drop, bus));
        TW(TN(drop, bus), TN(rise, bus));
        TW(TN(rise, bus), TN(rise, py));
        TW(TN(rise, py), p);
        dec->node_ids[8 + i] = o;
        disp->node_ids[right_seg[i]] = o;
    }
    return disp;
}

/* === BCD COUNTER DRIVING A 7-SEGMENT DISPLAY === */
static int place_bcd_counter(Circuit *circuit, float x, float y) {
    Component *clk = add_comp(circuit, COMP_CLOCK, x - 200, y, 0);
    if (!clk) return 0;
    clk->props.clock.frequency = 2.0;            /* two digits a second: watchable */
    clk->props.clock.v_high = 5.0;

    Component *cnt = add_comp(circuit, COMP_COUNTER, x, y, 0);
    cnt->props.counter.modulus = 10;

    Component *gnd = add_comp(circuit, COMP_GROUND, x - 200, y + 220, 0);
    int gnd_node = TN(x - 200, y + 200);
    gnd->node_ids[0] = gnd_node;

    /* clock into CLK on its own row */
    int ck = TN(x - 200, y - 40);
    clk->node_ids[0] = ck;
    TW(ck, TN(x - 40, y - 40));
    cnt->node_ids[0] = ck;

    /* clock return and RST both to the bottom rail - RST tied low so it free-runs */
    TW(TN(x - 200, y + 40), gnd_node);
    clk->node_ids[1] = gnd_node;
    TW(TN(x - 40, y + 40), TN(x - 40, y + 200));
    TW(TN(x - 40, y + 200), gnd_node);
    cnt->node_ids[1] = gnd_node;

    Component *disp = digit_block(circuit, x + 200, y, cnt);
    /* the display's common cathode comes down to the same rail */
    float comx, comy;
    component_get_terminal_pos(disp, 4, &comx, &comy);
    TW(TN(comx, y + 200), TN(x - 40, y + 200));

    /* CARRY lights the decimal point: it goes high for one count in ten, so the point marks
       every rollover. It is also the pin you would feed into the next digit's clock, and it
       gives both otherwise unused pins - the carry and the DP - something to do. */
    Component *rdp = add_comp(circuit, COMP_RESISTOR, x + 300, y + 160, 0);
    rdp->props.resistor.resistance = 150.0;
    int cy = TN(x, y + 70);
    cnt->node_ids[6] = cy;
    TW(cy, TN(x, y + 160));
    TW(TN(x, y + 160), TN(x + 260, y + 160));
    rdp->node_ids[0] = TN(x + 260, y + 160);
    rdp->node_ids[1] = TN(x + 340, y + 160);
    float dpx, dpy;
    component_get_terminal_pos(disp, 8, &dpx, &dpy);
    float dp_rise = x + 660;                 /* outside the e,f,g risers */
    TW(TN(x + 340, y + 160), TN(dp_rise, y + 160));
    TW(TN(dp_rise, y + 160), TN(dp_rise, dpy));
    TW(TN(dp_rise, dpy), TN(dpx, dpy));
    disp->node_ids[8] = TN(x + 340, y + 160);

    add_label(circuit, x - 220, y - 200, "BCD COUNTER DRIVING A 7-SEGMENT DISPLAY");
    add_label(circuit, x - 220, y + 250, "The counter steps 0-9 on every rising clock edge and the decoder turns each value into");
    add_label(circuit, x - 220, y + 280, "the seven segments that spell it. Watch it run: every segment is exercised over ten counts,");
    add_label(circuit, x - 220, y + 310, "which is what makes this the honest test of the display - a stuck segment shows up as a digit");
    add_label(circuit, x - 220, y + 340, "that reads wrong, not as a dark bar you have to look for.");
    return 14;
}

/* === DIGITAL CLOCK, HH:MM:SS ===
   Six digits, each one a counter, a decoder and a display. One second in, and each digit clocks
   the one to its left when it rolls over - which is all a digital clock is. The moduli do the
   work: 10 and 6 give 0-59 for seconds and minutes, and the hours would run 0-29 on 10 and 3, so
   an AND gate watching for 2 and 4 resets both hour digits at 24:00:00. That gate is the whole
   difference between a counter chain and a clock. */
static int place_digital_clock(Circuit *circuit, float x, float y) {
    /* left to right: HH tens, HH ones, MM tens, MM ones, SS tens, SS ones */
    static const int modulus[6] = { 3, 10, 6, 10, 6, 10 };
    static const char *const digit_name[6] = { "H10", "H1", "M10", "M1", "S10", "S1" };
    Component *cnt[6];
    Component *disp[6];
    const float pitch = 760.0f;
    const float carry_bus = y + 320.0f;      /* below every digit's own wiring */
    const float gnd_rail  = y + 420.0f;

    Component *gnd = add_comp(circuit, COMP_GROUND, x - 260, gnd_rail + 20.0f, 0);
    if (!gnd) return 0;
    int gnd_node = TN(x - 260, gnd_rail);
    gnd->node_ids[0] = gnd_node;

    for (int i = 0; i < 6; i++) {
        float cx = x + i * pitch;
        cnt[i] = add_comp(circuit, COMP_COUNTER, cx, y, 0);
        if (!cnt[i]) return 0;
        cnt[i]->props.counter.modulus = modulus[i];
        disp[i] = digit_block(circuit, cx + 200.0f, y, cnt[i]);
        if (!disp[i]) return 0;

        /* every display's common cathode down to the bottom rail */
        float comx, comy;
        component_get_terminal_pos(disp[i], 4, &comx, &comy);
        TW(TN(comx, y + 200), TN(comx, gnd_rail));
        TW(TN(comx, gnd_rail), TN(cx - 140, gnd_rail));
        disp[i]->node_ids[4] = gnd_node;

        /* the DP of every digit is off: tie it to the cathode it sits above */
        float dpx, dpy;
        component_get_terminal_pos(disp[i], 8, &dpx, &dpy);
        TW(TN(dpx, dpy), TN(dpx + 40.0f, dpy));
        TW(TN(dpx + 40.0f, dpy), TN(dpx + 40.0f, y + 240));
        TW(TN(dpx + 40.0f, y + 240), TN(comx, y + 240));
        TW(TN(comx, y + 240), TN(comx, y + 200));
        disp[i]->node_ids[8] = gnd_node;

        add_label(circuit, cx - 40, y - 160, digit_name[i]);
    }

    /* the bottom rail, chained left to right between the columns that land on it */
    for (int i = 0; i < 6; i++) {
        float cx = x + i * pitch;
        TW(TN(cx - 140, gnd_rail), TN(i == 0 ? x - 260 : cx - pitch - 140, gnd_rail));
    }

    /* one second in, at the right-hand end */
    Component *src = add_comp(circuit, COMP_CLOCK, x + 5 * pitch + 800.0f, y, 0);
    src->props.clock.frequency = 1.0;
    src->props.clock.v_high = 5.0;
    int tick = TN(x + 5 * pitch + 800.0f, y - 40);
    src->node_ids[0] = tick;
    TW(TN(x + 5 * pitch + 800.0f, y + 40), TN(x + 5 * pitch + 800.0f, gnd_rail));
    TW(TN(x + 5 * pitch + 800.0f, gnd_rail), TN(x + 5 * pitch - 140, gnd_rail));
    src->node_ids[1] = gnd_node;

    /* seconds' units take the tick; every other digit takes the carry of the one to its right */
    for (int i = 5; i >= 0; i--) {
        float cx = x + i * pitch;
        float clk_x = cx - 40.0f, clk_y = y - 40.0f;
        float riser = cx - 100.0f;
        if (i == 5) {
            /* over the top of the digit: straight along this row would run the tick through the
               last decoder and display */
            float top = y - 380.0f, src_x = x + 5 * pitch + 800.0f;
            TW(tick, TN(src_x, top));
            TW(TN(src_x, top), TN(cx - 100.0f, top));
            TW(TN(cx - 100.0f, top), TN(cx - 100.0f, clk_y));
            TW(TN(cx - 100.0f, clk_y), TN(clk_x, clk_y));
            cnt[i]->node_ids[0] = tick;
        } else {
            float sx2 = cx + pitch;                     /* the digit to the right */
            int cy = TN(sx2, y + 70);
            cnt[i + 1]->node_ids[6] = cy;
            TW(cy, TN(sx2, carry_bus));
            TW(TN(sx2, carry_bus), TN(riser, carry_bus));
            TW(TN(riser, carry_bus), TN(riser, clk_y));
            TW(TN(riser, clk_y), TN(clk_x, clk_y));
            cnt[i]->node_ids[0] = cy;
        }
    }

    /* RST: the four seconds and minutes digits never need one - their modulus already wraps
       them - so they sit on the rail. */
    for (int i = 2; i < 6; i++) {
        float cx = x + i * pitch;
        TW(TN(cx - 40, y + 40), TN(cx - 180, y + 40));
        TW(TN(cx - 180, y + 40), TN(cx - 180, gnd_rail));
        TW(TN(cx - 180, gnd_rail), TN(cx - 140, gnd_rail));
        cnt[i]->node_ids[1] = gnd_node;
    }

    /* The hours run 0..29 on their own: mod 10 and mod 3. This gate is what makes them a clock -
       tens = 2 (Q1) and units = 4 (Q2) means the display just reached 24, and both are reset. */
    Component *rst = add_comp(circuit, COMP_AND_GATE, x + pitch + 340.0f, y - 260.0f, 0);
    int tens_q1 = TN(x + 40, y - 20);          /* HH tens Q1: weight 2 */
    int ones_q2 = TN(x + pitch + 40, y + 20);  /* HH ones Q2: weight 4 */
    TW(tens_q1, TN(x + 100, y - 20));
    TW(TN(x + 100, y - 20), TN(x + 100, y - 280));
    TW(TN(x + 100, y - 280), TN(x + pitch + 300.0f, y - 280));
    rst->node_ids[0] = tens_q1;
    TW(ones_q2, TN(x + pitch + 100, y + 20));
    TW(TN(x + pitch + 100, y + 20), TN(x + pitch + 100, y - 240));
    TW(TN(x + pitch + 100, y - 240), TN(x + pitch + 300.0f, y - 240));
    rst->node_ids[1] = ones_q2;

    int rst_out = TN(x + pitch + 380.0f, y - 260.0f);
    rst->node_ids[2] = rst_out;
    TW(rst_out, TN(x + pitch + 380.0f, y - 320.0f));
    TW(TN(x + pitch + 380.0f, y - 320.0f), TN(x - 180, y - 320.0f));
    TW(TN(x - 180, y - 320.0f), TN(x - 180, y + 40));
    TW(TN(x - 180, y + 40), TN(x - 40, y + 40));
    cnt[0]->node_ids[1] = rst_out;
    TW(TN(x - 180, y - 320.0f), TN(x + pitch - 180, y - 320.0f));
    TW(TN(x + pitch - 180, y - 320.0f), TN(x + pitch - 180, y + 40));
    TW(TN(x + pitch - 180, y + 40), TN(x + pitch - 40, y + 40));
    cnt[1]->node_ids[1] = rst_out;

    /* The hours' carry is where a day counter would go. Brought out to a marked pin rather than
       left hanging, so the schematic says what it is instead of looking forgotten. */
    Component *day = add_comp(circuit, COMP_PIN, x - 160, y + 160, 0);
    int day_node = TN(x - 120, y + 160);
    day->node_ids[0] = day_node;
    strncpy(day->props.pin.pin_name, "DAY", sizeof(day->props.pin.pin_name) - 1);
    int h_cy = TN(x, y + 70);
    cnt[0]->node_ids[6] = h_cy;
    TW(h_cy, TN(x, y + 160));
    TW(TN(x, y + 160), day_node);

    /* Only the title and the per-digit names go on the canvas. The explanation lives in the
       notes panel, and printing it here as well put two paragraphs on top of each other. */
    add_label(circuit, x - 260, y - 420, "DIGITAL CLOCK - HH : MM : SS");
    add_label(circuit, x + 2 * pitch + 640, y - 420, "1 Hz in");
    return 20;
}

/* === WIRELESS LINK (TX/RX) ===
   What the two antenna parts actually do, which is not obvious from the symbols: TX measures the
   voltage across itself and publishes it on a channel, RX becomes a source of whatever is on that
   channel. Both present 50 ohm, so a matched load gets half the transmitted amplitude. */
static int place_wireless_link(Circuit *circuit, float x, float y) {
    Component *vsrc = add_comp(circuit, COMP_AC_VOLTAGE, x - 160, y + 40, 0);
    if (!vsrc) return 0;
    vsrc->props.ac_voltage.amplitude = 2.0;
    vsrc->props.ac_voltage.frequency = 1000.0;

    Component *tx = add_comp(circuit, COMP_ANTENNA_TX, x - 40, y, 0);
    tx->props.antenna.channel = 0;
    Component *rx = add_comp(circuit, COMP_ANTENNA_RX, x + 280, y, 0);
    rx->props.antenna.channel = 0;

    Component *rl = add_comp(circuit, COMP_RESISTOR, x + 400, y + 60, 90);
    rl->props.resistor.resistance = 50.0;

    Component *gnd = add_comp(circuit, COMP_GROUND, x - 160, y + 140, 0);
    int gnd_node = TN(x - 160, y + 120);
    gnd->node_ids[0] = gnd_node;

    /* Source across the transmitter */
    int src_hi = TN(x - 160, y);
    vsrc->node_ids[0] = src_hi;
    TW(src_hi, TN(x - 80, y));
    tx->node_ids[0] = src_hi;

    TW(TN(x - 160, y + 80), gnd_node);
    vsrc->node_ids[1] = gnd_node;

    /* Transmitter's cold end down to the ground rail */
    TW(TN(x, y), TN(x, y + 120));
    TW(TN(x, y + 120), gnd_node);
    tx->node_ids[1] = gnd_node;

    /* Receiver: cold end to ground, hot end into the matched load */
    TW(TN(x + 240, y), TN(x + 180, y));
    TW(TN(x + 180, y), TN(x + 180, y + 120));
    TW(TN(x + 180, y + 120), TN(x, y + 120));
    rx->node_ids[0] = gnd_node;

    int out = TN(x + 320, y);
    rx->node_ids[1] = out;
    TW(out, TN(x + 400, y));
    TW(TN(x + 400, y), TN(x + 400, y + 26));
    rl->node_ids[0] = out;
    TW(TN(x + 400, y + 94), TN(x + 400, y + 120));
    TW(TN(x + 400, y + 120), TN(x + 180, y + 120));
    rl->node_ids[1] = gnd_node;

    add_label(circuit, x - 200, y - 120, "WIRELESS LINK - TX publishes on a channel, RX reproduces it");
    add_label(circuit, x - 200, y + 200, "TX reads the voltage across itself and puts it on channel 0. RX on the same channel becomes");
    add_label(circuit, x - 200, y + 230, "a source of it. Both are 50 ohm, so the matched load sees half the amplitude: 2 V in, 1 V out.");
    add_label(circuit, x - 200, y + 260, "Change either channel number and the link goes quiet - that is all the channel is, a name they share.");
    return 12;
}

#undef TN
#undef TW

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
    [CIRCUIT_3PH_Y_BALANCED]   = { COMP_RESISTOR, 3, 0 },   // phase B load
    [CIRCUIT_3PH_UNBALANCED]   = { COMP_RESISTOR, 6, 0, "NEUT" },   /* the neutral shift, not an output */
    [CIRCUIT_3PH_345_LINE]     = { COMP_RESISTOR, 1, 0 },   // phase B load
    [CIRCUIT_3PH_RECTIFIER]    = { COMP_RESISTOR, 0, 0 },   // plus bus
    [CIRCUIT_SCHMITT_BISTABLE] = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_TRI_SQUARE_GEN]   = { COMP_OPAMP, 1, 2 },      // triangle
    [CIRCUIT_FUNCTION_GEN]     = { COMP_RESISTOR, 3, 1 },   // shaper output
    [CIRCUIT_COLPITTS]         = { COMP_NMOS, 0, 1 },       // drain
    [CIRCUIT_RING_OSC]         = { COMP_NOT_GATE, 4, 1 },
    [CIRCUIT_HARTLEY]          = { COMP_NMOS, 0, 1 },
    [CIRCUIT_CLAPP]            = { COMP_NMOS, 0, 1 },
    [CIRCUIT_THEVENIN]         = { COMP_RESISTOR, 3, 0 },
    [CIRCUIT_SUPERPOSITION]    = { COMP_RESISTOR, 0, 1 },
    [CIRCUIT_RC_STEP]          = { COMP_CAPACITOR, 0, 0 },
    [CIRCUIT_RL_STEP]          = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_RLC_RING]         = { COMP_CAPACITOR, 0, 0 },
    [CIRCUIT_RLC_DAMPING]      = { COMP_CAPACITOR, 1, 0 },   // critical row
    [CIRCUIT_OPAMP_SAT]        = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_SINGLE_TUNED_AMP] = { COMP_RESISTOR, 4, 0 },
    [CIRCUIT_COMMON_BASE]      = { COMP_RESISTOR, 4, 0 },
    [CIRCUIT_DARLINGTON]       = { COMP_RESISTOR, 1, 0 },
    [CIRCUIT_SR_LATCH]         = { COMP_NOR_GATE, 1, 2 },   // Q
    [CIRCUIT_POWER_PLANT]      = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_SUBSTATION]       = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_IO_PUSH_PULL]     = { COMP_RESISTOR, 1, 0 },
    [CIRCUIT_IO_OPEN_DRAIN]    = { COMP_RESISTOR, 1, 1 },
    [CIRCUIT_IO_OPEN_COLLECTOR]= { COMP_RESISTOR, 1, 1 },
    [CIRCUIT_IO_I2C_BUS]       = { COMP_RESISTOR, 2, 1 },
    [CIRCUIT_IO_I2C_LEVEL]     = { COMP_RESISTOR, 2, 1 },
    [CIRCUIT_IO_INPUT_DEBOUNCE]= { COMP_NOT_GATE, 0, 1 },
    [CIRCUIT_IO_LOW_SIDE]      = { COMP_INDUCTOR, 0, 1 },
    [CIRCUIT_IO_HIGH_SIDE]     = { COMP_RESISTOR, 2, 0 },
    [CIRCUIT_IO_SPI]           = { COMP_CAPACITOR, 0, 0 },
    [CIRCUIT_IO_UART]          = { COMP_RESISTOR, 1, 0 },
    [CIRCUIT_IO_RS485]         = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_IO_SPMI]          = { COMP_CAPACITOR, 1, 0 },
    [CIRCUIT_TX_69KV]          = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_TX_LADDER]        = { COMP_RESISTOR, 5, 0 },
    [CIRCUIT_TX_WIND]          = { COMP_TLINE, 0, 0 },        // 34.5 kV collector bus
    [CIRCUIT_TX_PLANT]         = { COMP_RESISTOR, 3, 0 },      // 480 V shop bus
    [CIRCUIT_RES_SERVICE]      = { COMP_RESISTOR, 3, 0 },
    [CIRCUIT_RES_BRANCH]       = { COMP_RESISTOR, 1, 0 },
    [CIRCUIT_RES_ACSTART]      = { COMP_RESISTOR, 1, 0 },      // house panel
    [CIRCUIT_RES_SOLAR]        = { COMP_RESISTOR, 1, 0 },
    [CIRCUIT_COM_480Y]         = { COMP_RESISTOR, 1, 0 },
    [CIRCUIT_COM_208Y]         = { COMP_RESISTOR, 1, 0 },
    [CIRCUIT_COM_PFC]          = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_COM_ATS]          = { COMP_RESISTOR, 1, 0 },
    [CIRCUIT_GS_N1]            = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_GS_IBR]           = { COMP_RESISTOR, 1, 0 },
    [CIRCUIT_GS_BOLD]          = { COMP_RESISTOR, 0, 0 },      // the conventional line
    [CIRCUIT_GS_DERATE]        = { COMP_RESISTOR, 1, 0 },
    [CIRCUIT_GS_FACRATE]       = { COMP_RESISTOR, 4, 0 },
    [CIRCUIT_GS_KRON]          = { COMP_RESISTOR, 2, 0 },
    [CIRCUIT_GS_RX]            = { COMP_RESISTOR, 1, 0 },
    [CIRCUIT_GS_GOVERNOR]      = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_GS_PIDS]          = { COMP_RESISTOR, 1, 0 },
    [CIRCUIT_MOS_IDVGS]        = { COMP_RESISTOR, 1, 0 },      // the first device's sense resistor
    [CIRCUIT_MOS_IDVDS]        = { COMP_RESISTOR, 5, 0 },      // the Vgs 3.5 V device carries the most current
    [CIRCUIT_MOS_TUNED]        = { COMP_RESISTOR, 4, 0 },
    [CIRCUIT_MOS_CG]           = { COMP_RESISTOR, 4, 0 },
    [CIRCUIT_MOS_CASCODE]      = { COMP_RESISTOR, 6, 0 },
    [CIRCUIT_MOS_DIFF]         = { COMP_RESISTOR, 0, 1 },
    [CIRCUIT_MOS_MIRROR]       = { COMP_RESISTOR, 1, 1 },
    [CIRCUIT_CMOS_INV]         = { COMP_CAPACITOR, 0, 0 },
    [CIRCUIT_CMOS_NAND]        = { COMP_RESISTOR, 1, 0 },      // the load, not the stack bleed
    [CIRCUIT_CMOS_TGATE]       = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_XY_LISSAJOUS]     = { COMP_RESISTOR, 1, 0 },
    [CIRCUIT_XY_PLOTTER]       = { COMP_RESISTOR, 1, 0 },
    [CIRCUIT_HW_BUCK]          = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_HW_BOOST]         = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_HW_BUCKBOOST]     = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_HW_CUK]           = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_HW_INTERLEAVED]   = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_HW_PDN]           = { COMP_RESISTOR, 3, 0 },
    [CIRCUIT_HW_CAPS]          = { COMP_RESISTOR, 2, 0 },
    [CIRCUIT_HW_MATCH]         = { COMP_RESISTOR, 3, 0 },
    [CIRCUIT_HW_REFLECT]       = { COMP_DELAY_LINE, 0, 1 },   /* the far end of the line */
    [CIRCUIT_HW_LOOP]          = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_ID_SOURCE]        = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_ID_DIODE]         = { COMP_RESISTOR, 0, 0 },
    [CIRCUIT_ID_CAP]           = { COMP_CAPACITOR, 0, 0 },
    [CIRCUIT_ID_IND]           = { COMP_CAPACITOR, 0, 0 },
    [CIRCUIT_ID_OPAMP]         = { COMP_RESISTOR, 2, 0 },
    [CIRCUIT_ID_BJT]           = { COMP_RESISTOR, 1, 1 },
    [CIRCUIT_ID_MOSFET]        = { COMP_RESISTOR, 2, 1 },
    [CIRCUIT_ID_OPAMP_ERR]     = { COMP_OPAMP, 1, 2 },
    [CIRCUIT_PARTS_MOSFET]     = { COMP_NMOS, 0, 1 },
    [CIRCUIT_CAP_DCBIAS]       = { COMP_CAPACITOR, 0, 0 },
    [CIRCUIT_NE555_ASTABLE]    = { COMP_RESISTOR, 2, 0 },
    [CIRCUIT_PIERCE]           = { COMP_OPAMP, 0, 2 },
    [CIRCUIT_IV_PROBE_COMP]    = { COMP_RESISTOR, 5, 0 },     /* the correctly compensated channel's scope input */
    [CIRCUIT_IV_PROBE_LOADING] = { COMP_CAPACITOR, 0, 0 },    /* the unprobed node */
    [CIRCUIT_IV_GROUND_LEAD]   = { COMP_CAPACITOR, 0, 0 },    /* probe tip, 6 inch clip */
    [CIRCUIT_IV_SCOPE_INPUT_Z] = { COMP_RESISTOR, 1, 0 },     /* the 1 M input at the end of the cable */
    [CIRCUIT_IV_AC_COUPLING]   = { COMP_RESISTOR, 2, 0 },     /* the AC-coupled channel */
    [CIRCUIT_IV_SHUNT_SENSE]   = { COMP_OPAMP, 0, 2 },        /* high-side difference amp output */
    [CIRCUIT_IV_KELVIN]        = { COMP_OPAMP, 0, 2 },        /* the 4-wire differential reading */
    [CIRCUIT_IV_BUCK_NODES]    = { COMP_RESISTOR, 2, 0 },     /* the output, after L and C */
    /* Resistor 0 is the linear regulator's load, resistor 1 the switcher's. The output probe was
       on 1, so the whole point of the circuit - the two 5 V rails side by side - had a probe on
       one half and nothing on the other. Both are probed now. */
    [CIRCUIT_IV_LDO_VS_BUCK]   = { COMP_RESISTOR, 0, 0, "LDO" },
    [CIRCUIT_IV_BOOTSTRAP]     = { COMP_CAPACITOR, 0, 0 },    /* BOOT, riding on the switch node */
    [CIRCUIT_IV_TERMINATION]   = { COMP_CAPACITOR, 0, 0 },    /* unterminated receiver: the overshoot */
    [CIRCUIT_IV_PULLUP_SIZING] = { COMP_CAPACITOR, 0, 0 },    /* the 10 k bus */
    [CIRCUIT_IV_GROUND_BOUNCE] = { COMP_INDUCTOR, 0, 0 },     /* the local ground, above the bond wire */
    [CIRCUIT_IV_CROSSTALK]     = { COMP_CAPACITOR, 2, 0 },    /* the weakly held victim */
    [CIRCUIT_IV_ESD_CLAMP]     = { COMP_DIODE, 0, 0 },        /* the pin, clamped one diode above the rail */
    [CIRCUIT_IV_CAP_ENERGY]    = { COMP_CAPACITOR, 1, 0 },    /* C2 in the fast copy: 0 V, then 5 V */
    [CIRCUIT_IV_MILLER]        = { COMP_NMOS, 0, 1 },         /* the drain of the stage without C_gd */
    [CIRCUIT_IV_SWITCH_CHOICE] = { COMP_NPN_BJT, 0, 1 },      /* the saturated collector */
    [CIRCUIT_IV_INRUSH]        = { COMP_CAPACITOR, 0, 0 },    /* the bulk cap, straight in */
    [CIRCUIT_TLINE_REAL]       = { COMP_RESISTOR, 1, 0 },     /* the matched far end */
    [CIRCUIT_SEVENSEG_TEST]    = { COMP_7SEG_DISPLAY, 0, 0 }, /* segment a's pin: its forward drop */
    [CIRCUIT_WIRELESS_LINK]    = { COMP_RESISTOR, 0, 0 },     /* across the matched load */
    [CIRCUIT_BCD_COUNTER]      = { COMP_7SEG_DISPLAY, 0, 0 }, /* segment a: on for 0,2,3,5,6,7,8,9 */
    [CIRCUIT_DIGITAL_CLOCK]    = { COMP_7SEG_DISPLAY, 5, 0 }, /* the seconds digit, segment a */
    [CIRCUIT_TESLA_COIL]       = { COMP_TOROID, 0, 0 },
    [CIRCUIT_TESLA_COIL_BIG]   = { COMP_TOROID, 0, 0 },
    [CIRCUIT_TESLA_COIL_DETUNED] = { COMP_TOROID, 0, 0 },
};

// Extra probes (up to 3) for templates that need more than input + output on the scope
static const TemplateProbeSpec template_extra_probes[CIRCUIT_TYPE_COUNT][3] = {
    [CIRCUIT_3PH_Y_BALANCED]   = { { COMP_RESISTOR, 5, 0, "PH C" }, { COMP_RESISTOR, 6, 0, "NEUT" } },      // phase C load, neutral (A = source probe)
    [CIRCUIT_DIFFERENTIAL_PAIR] = { { COMP_NPN_BJT, 1, 1, "VC2" } },                                // Q2 collector: the mirror image
    [CIRCUIT_3PH_UNBALANCED]   = { { COMP_RESISTOR, 3, 0, "PH B" }, { COMP_RESISTOR, 5, 0, "PH C" } },
    [CIRCUIT_3PH_345_LINE]     = { { COMP_RESISTOR, 0, 0, "PH A" }, { COMP_RESISTOR, 2, 0, "PH C" } },      // phase A, C loads
    [CIRCUIT_3PH_RECTIFIER]    = { { COMP_DIODE, 5, 0, "V-" } },                                   // minus bus
    [CIRCUIT_TRI_SQUARE_GEN]   = { { COMP_OPAMP, 0, 2, "SQUARE" } },                                   // square
    [CIRCUIT_FUNCTION_GEN]     = { { COMP_OPAMP, 1, 2, "TRI" } },                                   // triangle
    [CIRCUIT_RLC_DAMPING]      = { { COMP_CAPACITOR, 0, 0, "UNDER" }, { COMP_CAPACITOR, 2, 0, "OVER" } },      // under / over rows
    [CIRCUIT_OPAMP_SAT]        = { { COMP_OPAMP, 0, 0, "V-" } },                                   // inverting input
    [CIRCUIT_SR_LATCH]         = { { COMP_NOR_GATE, 0, 2, "QBAR" }, { COMP_PULSE_SOURCE, 1, 0, "R" } },   // Qbar, R
    [CIRCUIT_POWER_PLANT]      = { { COMP_RESISTOR, 1, 0, "PH B" }, { COMP_RESISTOR, 2, 0, "PH C" } },      // phases B, C
    [CIRCUIT_SUBSTATION]       = { { COMP_RESISTOR, 1, 0, "PH B" }, { COMP_RESISTOR, 2, 0, "PH C" } },
    [CIRCUIT_IO_I2C_BUS]       = { { COMP_PULSE_SOURCE, 1, 0, "SLAVE" } },                             // slave driver
    [CIRCUIT_IO_I2C_LEVEL]     = { { COMP_RESISTOR, 1, 1, "3V3" } },                                 // 3.3 V side
    [CIRCUIT_IO_INPUT_DEBOUNCE]= { { COMP_RESISTOR, 0, 1, "PIN" }, { COMP_CAPACITOR, 0, 0, "RC" } },       // pin, RC node
    [CIRCUIT_IO_HIGH_SIDE]     = { { COMP_RESISTOR, 1, 1, "PGATE" } },                                 // PMOS gate
    [CIRCUIT_IO_SPI]           = { { COMP_PULSE_SOURCE, 1, 0, "MOSI" }, { COMP_CAPACITOR, 1, 0, "MOSIRX" } },   // MOSI source, MOSI at load
    [CIRCUIT_IO_UART]          = { { COMP_PULSE_SOURCE, 1, 0, "TX3V3" }, { COMP_NOT_GATE, 0, 1, "RX5V" } },    // 3.3 V TX, 5 V receiver out
    [CIRCUIT_IO_RS485]         = { { COMP_RESISTOR, 3, 0, "A FAR" }, { COMP_RESISTOR, 3, 1, "B FAR" } },        // A, B at the far termination
    [CIRCUIT_IO_SPMI]          = { { COMP_CAPACITOR, 0, 0, "BUS" } },
    [CIRCUIT_IV_LDO_VS_BUCK]   = { { COMP_RESISTOR, 1, 0, "SW OUT" } },   /* the switcher's rail */
    /* the digit templates: the clock that drives them, so the scope shows the input that makes
       the count advance next to the segment it lights */
    [CIRCUIT_BCD_COUNTER]      = { { COMP_CLOCK, 0, 0, "CLK" } },
    [CIRCUIT_DIGITAL_CLOCK]    = { { COMP_CLOCK, 0, 0, "CLK" } },                                // SCLK at load
    [CIRCUIT_TX_LADDER]        = { { COMP_RESISTOR, 1, 0, "138KV" }, { COMP_RESISTOR, 2, 0, "69KV" }, { COMP_RESISTOR, 3, 0, "12KV" } },   // 138 / 69 / 12.47 kV buses
    [CIRCUIT_TX_WIND]          = { { COMP_TLINE, 1, 0, "345KV" }, { COMP_TLINE, 1, 1, "POI" } },               // 345 kV sending end, POI
    [CIRCUIT_TX_PLANT]         = { { COMP_RESISTOR, 1, 0, "4160V" } },                                 // 4.16 kV motor bus
    [CIRCUIT_RES_SERVICE]      = { { COMP_RESISTOR, 4, 1, "L2" }, { COMP_RESISTOR, 2, 0, "NEUT" } },        // L2 at the panel, the neutral conductor
    [CIRCUIT_RES_BRANCH]       = { { COMP_RESISTOR, 3, 0, "BR10" } },                                 // #10 load
    [CIRCUIT_RES_ACSTART]      = { { COMP_RESISTOR, 2, 0, "MOTOR" } },                                 // motor branch
    [CIRCUIT_COM_480Y]         = { { COMP_RESISTOR, 4, 0, "PH B" }, { COMP_RESISTOR, 6, 0, "PH C" } },        // phase B, phase C buses
    [CIRCUIT_COM_208Y]         = { { COMP_RESISTOR, 3, 0, "PH B" }, { COMP_RESISTOR, 5, 0, "PH C" } },        // phase B, C branch buses
    [CIRCUIT_COM_ATS]          = { { COMP_AC_VOLTAGE, 1, 0, "GEN" } },                               // the standby generator
    [CIRCUIT_GS_BOLD]          = { { COMP_RESISTOR, 1, 0, "BOLD" } },                                 // the BOLD receiving bus
    [CIRCUIT_GS_KRON]          = { { COMP_RESISTOR, 6, 0, "DELTA" }, { COMP_RESISTOR, 4, 0, "SEC" } },        // the delta-side load (overlays the Y one) and the second Y load
    [CIRCUIT_GS_RX]            = { { COMP_RESISTOR, 4, 0, "FEEDER" } },                                 // the feeder bus
    [CIRCUIT_GS_IBR]           = { { COMP_RESISTOR, 2, 0, "FAULT" } },                                 // the fault branch
    [CIRCUIT_MOS_IDVGS]        = { { COMP_RESISTOR, 3, 0, "DEV2" }, { COMP_RESISTOR, 5, 0, "DEV3" } },        // the other two devices
    [CIRCUIT_MOS_IDVDS]        = { { COMP_RESISTOR, 1, 0, "DEV2" }, { COMP_RESISTOR, 3, 0, "DEV3" } },
    [CIRCUIT_MOS_DIFF]         = { { COMP_RESISTOR, 1, 1, "OUT2" } },
    [CIRCUIT_CMOS_TGATE]       = { { COMP_RESISTOR, 1, 0, "PASS" } },
    [CIRCUIT_XY_LISSAJOUS]     = { { COMP_RESISTOR, 0, 0, "Y" } },
    [CIRCUIT_XY_PLOTTER]       = { { COMP_RESISTOR, 0, 0, "Y" } },
    [CIRCUIT_HW_CAPS]          = { { COMP_RESISTOR, 6, 0, "RAIL" } },
    [CIRCUIT_HW_MATCH]         = { { COMP_RESISTOR, 1, 0, "NEAR" }, { COMP_RESISTOR, 5, 0, "FAR" } },
    [CIRCUIT_HW_REFLECT]       = { { COMP_DELAY_LINE, 0, 0, "DRIVER" } },   /* and the driver end */
    [CIRCUIT_HW_LOOP]          = { { COMP_OPAMP, 1, 2, "COMP" } },
    [CIRCUIT_HW_PDN]           = { { COMP_RESISTOR, 0, 0, "PDN" } },
    /* ideal vs real: the whole point is seeing both models at once, so every copy is probed */
    [CIRCUIT_ID_SOURCE]        = { { COMP_RESISTOR, 1, 0, "MODEL2" }, { COMP_RESISTOR, 2, 0, "MODEL3" } },
    [CIRCUIT_ID_DIODE]         = { { COMP_RESISTOR, 1, 0, "MODEL2" } },
    [CIRCUIT_ID_CAP]           = { { COMP_CAPACITOR, 1, 0, "MODEL2" }, { COMP_CAPACITOR, 2, 0, "MODEL3" } },
    [CIRCUIT_ID_IND]           = { { COMP_CAPACITOR, 1, 0, "MODEL2" } },
    [CIRCUIT_ID_OPAMP]         = { { COMP_RESISTOR, 5, 0, "MODEL2" }, { COMP_RESISTOR, 8, 0, "MODEL3" } },
    [CIRCUIT_ID_BJT]           = { { COMP_RESISTOR, 3, 1, "MODEL2" } },
    [CIRCUIT_ID_MOSFET]        = { { COMP_RESISTOR, 5, 1, "MODEL2" } },
    [CIRCUIT_ID_OPAMP_ERR]     = { { COMP_OPAMP, 0, 2, "OUT2" } },
    [CIRCUIT_PARTS_MOSFET]     = { { COMP_NMOS, 1, 1, "DRAIN2" }, { COMP_NMOS, 2, 1, "DRAIN3" } },
    [CIRCUIT_CAP_DCBIAS]       = { { COMP_CAPACITOR, 1, 0, "CAP2" }, { COMP_CAPACITOR, 2, 0, "CAP3" } },
    [CIRCUIT_NE555_ASTABLE]    = { { COMP_CAPACITOR, 0, 0, "VCAP" } },
    [CIRCUIT_CMOS_NAND]        = { { COMP_PULSE_SOURCE, 1, 0, "B IN" } },
    // multi-input circuits: every input on its own channel
    [CIRCUIT_SUMMING_AMP]      = { { COMP_DC_VOLTAGE, 1, 0, "V2" }, { COMP_DC_VOLTAGE, 2, 0, "V3" } },    // V2, V3 (V1 = source probe)
    [CIRCUIT_DIFFERENCE_AMP]   = { { COMP_DC_VOLTAGE, 1, 0, "V2" } },                               // V2 (0.5 V DC)
    [CIRCUIT_INSTR_AMP]        = { { COMP_DC_VOLTAGE, 0, 0, "V2" } },                               // V2 (50 mV DC)
    [CIRCUIT_SUPERPOSITION]    = { { COMP_DC_VOLTAGE, 1, 0, "V6" } },                               // the 6 V source
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
    [CIRCUIT_3PH_Y_BALANCED] = 5e-3, [CIRCUIT_3PH_UNBALANCED] = 5e-3, [CIRCUIT_3PH_345_LINE] = 5e-3, [CIRCUIT_3PH_RECTIFIER] = 5e-3,
    [CIRCUIT_SCHMITT_BISTABLE] = 2e-3, [CIRCUIT_TRI_SQUARE_GEN] = 100e-6, [CIRCUIT_FUNCTION_GEN] = 100e-6, [CIRCUIT_COLPITTS] = 500e-9, [CIRCUIT_RING_OSC] = 2e-6,
    [CIRCUIT_HARTLEY] = 500e-9, [CIRCUIT_CLAPP] = 100e-9,
    [CIRCUIT_THEVENIN] = 1e-3, [CIRCUIT_SUPERPOSITION] = 1e-3, [CIRCUIT_RC_STEP] = 1e-3, [CIRCUIT_RL_STEP] = 100e-6, [CIRCUIT_RLC_RING] = 50e-6, [CIRCUIT_RLC_DAMPING] = 100e-6, [CIRCUIT_OPAMP_SAT] = 200e-6,
    [CIRCUIT_SINGLE_TUNED_AMP] = 5e-6, [CIRCUIT_COMMON_BASE] = 20e-6, [CIRCUIT_DARLINGTON] = 200e-6, [CIRCUIT_SR_LATCH] = 200e-6, [CIRCUIT_POWER_PLANT] = 5e-3, [CIRCUIT_SUBSTATION] = 5e-3,
    [CIRCUIT_IO_PUSH_PULL] = 200e-9, [CIRCUIT_IO_OPEN_DRAIN] = 1e-6, [CIRCUIT_IO_OPEN_COLLECTOR] = 2e-6, [CIRCUIT_IO_I2C_BUS] = 10e-6,
    [CIRCUIT_IO_I2C_LEVEL] = 2e-6, [CIRCUIT_IO_INPUT_DEBOUNCE] = 5e-3, [CIRCUIT_IO_LOW_SIDE] = 500e-6, [CIRCUIT_IO_HIGH_SIDE] = 500e-6,
    [CIRCUIT_IO_SPI] = 50e-9, [CIRCUIT_IO_UART] = 100e-6, [CIRCUIT_IO_RS485] = 500e-9, [CIRCUIT_IO_SPMI] = 50e-9,
    [CIRCUIT_TX_69KV] = 5e-3, [CIRCUIT_TX_LADDER] = 5e-3, [CIRCUIT_TX_WIND] = 5e-3, [CIRCUIT_TX_PLANT] = 5e-3,
    [CIRCUIT_RES_SERVICE] = 5e-3, [CIRCUIT_RES_BRANCH] = 5e-3, [CIRCUIT_RES_ACSTART] = 20e-3, [CIRCUIT_RES_SOLAR] = 5e-3,
    [CIRCUIT_COM_480Y] = 5e-3, [CIRCUIT_COM_208Y] = 5e-3, [CIRCUIT_COM_PFC] = 5e-3, [CIRCUIT_COM_ATS] = 20e-3,
    [CIRCUIT_GS_N1] = 5e-3, [CIRCUIT_GS_IBR] = 50e-3, [CIRCUIT_GS_BOLD] = 5e-3, [CIRCUIT_GS_DERATE] = 5e-3,
    [CIRCUIT_GS_FACRATE] = 5e-3, [CIRCUIT_GS_KRON] = 5e-3, [CIRCUIT_GS_RX] = 5e-3, [CIRCUIT_GS_GOVERNOR] = 0.5,
    [CIRCUIT_GS_PIDS] = 1.0,
    [CIRCUIT_MOS_IDVGS] = 1e-3, [CIRCUIT_MOS_IDVDS] = 1e-3,
    [CIRCUIT_MOS_TUNED] = 2e-6, [CIRCUIT_MOS_CG] = 20e-6, [CIRCUIT_MOS_CASCODE] = 20e-6,
    [CIRCUIT_MOS_DIFF] = 200e-6, [CIRCUIT_MOS_MIRROR] = 1e-3, [CIRCUIT_CMOS_INV] = 200e-6,
    [CIRCUIT_CMOS_NAND] = 500e-6, [CIRCUIT_CMOS_TGATE] = 200e-6,
    [CIRCUIT_XY_LISSAJOUS] = 200e-6, [CIRCUIT_XY_PLOTTER] = 1e-3,
    [CIRCUIT_HW_BUCK] = 5e-6, [CIRCUIT_HW_BOOST] = 5e-6, [CIRCUIT_HW_BUCKBOOST] = 5e-6, [CIRCUIT_HW_CUK] = 5e-6,
    [CIRCUIT_HW_INTERLEAVED] = 5e-6, [CIRCUIT_HW_PDN] = 20e-6, [CIRCUIT_HW_CAPS] = 50e-6,
    [CIRCUIT_HW_MATCH] = 500e-9, [CIRCUIT_HW_REFLECT] = 500e-9, [CIRCUIT_HW_LOOP] = 100e-6,
    [CIRCUIT_ID_SOURCE] = 1e-3, [CIRCUIT_ID_DIODE] = 200e-6, [CIRCUIT_ID_CAP] = 10e-6,
    [CIRCUIT_ID_IND] = 200e-6, [CIRCUIT_ID_OPAMP] = 2e-6, [CIRCUIT_ID_BJT] = 1e-3, [CIRCUIT_ID_MOSFET] = 1e-3,
    [CIRCUIT_ID_OPAMP_ERR] = 1e-3, [CIRCUIT_PARTS_MOSFET] = 1e-3, [CIRCUIT_CAP_DCBIAS] = 10e-6, [CIRCUIT_NE555_ASTABLE] = 100e-6, [CIRCUIT_PIERCE] = 5e-6,
    [CIRCUIT_IV_PROBE_COMP] = 200e-6, [CIRCUIT_IV_PROBE_LOADING] = 200e-9,
    [CIRCUIT_IV_GROUND_LEAD] = 20e-9, [CIRCUIT_IV_SCOPE_INPUT_Z] = 20e-9,
    [CIRCUIT_IV_AC_COUPLING] = 2e-6, [CIRCUIT_IV_SHUNT_SENSE] = 1e-3, [CIRCUIT_IV_KELVIN] = 1e-3,
    [CIRCUIT_IV_BUCK_NODES] = 2e-6,
    [CIRCUIT_IV_LDO_VS_BUCK] = 2e-6, [CIRCUIT_IV_BOOTSTRAP] = 2e-6,
    [CIRCUIT_IV_TERMINATION] = 5e-9, [CIRCUIT_IV_PULLUP_SIZING] = 5e-6,
    [CIRCUIT_IV_GROUND_BOUNCE] = 5e-9, [CIRCUIT_IV_CROSSTALK] = 5e-9, [CIRCUIT_IV_ESD_CLAMP] = 1e-3,
    [CIRCUIT_IV_CAP_ENERGY] = 5e-3, [CIRCUIT_IV_MILLER] = 200e-9,
    [CIRCUIT_IV_SWITCH_CHOICE] = 1e-3, [CIRCUIT_IV_INRUSH] = 5e-3, [CIRCUIT_TLINE_REAL] = 20e-9,
    [CIRCUIT_SEVENSEG_TEST] = 1e-3, [CIRCUIT_WIRELESS_LINK] = 200e-6, [CIRCUIT_BCD_COUNTER] = 0.1, [CIRCUIT_DIGITAL_CLOCK] = 1.0,
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
    [CIRCUIT_VOLTAGE_DOUBLER] = 5.0, [CIRCUIT_RELAXATION_OSC] = 5.0, [CIRCUIT_HALFWAVE_FILTERED] = 5.0,
    [CIRCUIT_HV_345_LINE] = 100e3, [CIRCUIT_HV_138_LINE_VAR] = 50e3, [CIRCUIT_MV_FEEDER] = 5e3, [CIRCUIT_POLE_XFMR] = 100.0,
    [CIRCUIT_GEN_GSU] = 100e3, [CIRCUIT_GRID_CHAIN] = 100.0, [CIRCUIT_FERRANTI_LINE] = 100e3,
    [CIRCUIT_TESLA_COIL] = 100e3, [CIRCUIT_TESLA_COIL_BIG] = 100e3, [CIRCUIT_TESLA_COIL_DETUNED] = 100e3,
    [CIRCUIT_LINE_MODEL_LADDER] = 50e3, [CIRCUIT_DC_LINE_DROP] = 5.0,
    [CIRCUIT_PC_OVERCURRENT] = 5.0, [CIRCUIT_PC_DIFFERENTIAL] = 5.0, [CIRCUIT_PC_DISTANCE] = 5.0, [CIRCUIT_PC_BREAKER_FAIL] = 2.0,
    [CIRCUIT_SIL_LOADING] = 100e3, [CIRCUIT_SERIES_COMP] = 100e3, [CIRCUIT_HV_765_LINE] = 200e3,
    [CIRCUIT_3PH_Y_BALANCED] = 100.0, [CIRCUIT_3PH_UNBALANCED] = 100.0, [CIRCUIT_3PH_345_LINE] = 100e3, [CIRCUIT_3PH_RECTIFIER] = 50.0,
    [CIRCUIT_SCHMITT_BISTABLE] = 5.0, [CIRCUIT_TRI_SQUARE_GEN] = 5.0, [CIRCUIT_FUNCTION_GEN] = 2.0, [CIRCUIT_COLPITTS] = 10.0, [CIRCUIT_RING_OSC] = 2.0,
    [CIRCUIT_HARTLEY] = 10.0, [CIRCUIT_CLAPP] = 5.0,
    [CIRCUIT_THEVENIN] = 2.0, [CIRCUIT_SUPERPOSITION] = 2.0, [CIRCUIT_RC_STEP] = 2.0, [CIRCUIT_RL_STEP] = 2.0, [CIRCUIT_RLC_RING] = 5.0, [CIRCUIT_RLC_DAMPING] = 5.0, [CIRCUIT_OPAMP_SAT] = 5.0,
    [CIRCUIT_SINGLE_TUNED_AMP] = 2.0, [CIRCUIT_COMMON_BASE] = 1.0, [CIRCUIT_DARLINGTON] = 2.0, [CIRCUIT_SR_LATCH] = 2.0, [CIRCUIT_POWER_PLANT] = 100e3, [CIRCUIT_SUBSTATION] = 50e3,
    [CIRCUIT_IO_PUSH_PULL] = 1.0, [CIRCUIT_IO_OPEN_DRAIN] = 1.0, [CIRCUIT_IO_OPEN_COLLECTOR] = 2.0, [CIRCUIT_IO_I2C_BUS] = 1.0,
    [CIRCUIT_IO_I2C_LEVEL] = 2.0, [CIRCUIT_IO_INPUT_DEBOUNCE] = 1.0, [CIRCUIT_IO_LOW_SIDE] = 5.0, [CIRCUIT_IO_HIGH_SIDE] = 5,
    [CIRCUIT_IO_SPI] = 1.0, [CIRCUIT_IO_UART] = 1.0, [CIRCUIT_IO_RS485] = 2.0, [CIRCUIT_IO_SPMI] = 0.5,
    [CIRCUIT_TX_69KV] = 20e3, [CIRCUIT_TX_LADDER] = 100e3, [CIRCUIT_TX_WIND] = 100e3, [CIRCUIT_TX_PLANT] = 5e3,
    [CIRCUIT_RES_SERVICE] = 100.0, [CIRCUIT_RES_BRANCH] = 50.0, [CIRCUIT_RES_ACSTART] = 200.0, [CIRCUIT_RES_SOLAR] = 100.0,
    [CIRCUIT_COM_480Y] = 100.0, [CIRCUIT_COM_208Y] = 50.0, [CIRCUIT_COM_PFC] = 5.0, [CIRCUIT_COM_ATS] = 100.0,
    [CIRCUIT_GS_N1] = 100e3, [CIRCUIT_GS_IBR] = 100e3, [CIRCUIT_GS_BOLD] = 100e3, [CIRCUIT_GS_DERATE] = 5e3,
    [CIRCUIT_GS_FACRATE] = 50e3, [CIRCUIT_GS_KRON] = 50.0, [CIRCUIT_GS_RX] = 50.0, [CIRCUIT_GS_GOVERNOR] = 0.1,
    [CIRCUIT_GS_PIDS] = 5,
    [CIRCUIT_MOS_IDVGS] = 0.1, [CIRCUIT_MOS_IDVDS] = 0.1,
    [CIRCUIT_MOS_TUNED] = 2.0, [CIRCUIT_MOS_CG] = 1.0, [CIRCUIT_MOS_CASCODE] = 1.0,
    [CIRCUIT_MOS_DIFF] = 5.0, [CIRCUIT_MOS_MIRROR] = 5.0, [CIRCUIT_CMOS_INV] = 2.0,
    [CIRCUIT_CMOS_NAND] = 2.0, [CIRCUIT_CMOS_TGATE] = 2.0,
    [CIRCUIT_XY_LISSAJOUS] = 2.0, [CIRCUIT_XY_PLOTTER] = 2.0,
    [CIRCUIT_HW_BUCK] = 2.0, [CIRCUIT_HW_BOOST] = 5.0, [CIRCUIT_HW_BUCKBOOST] = 5.0, [CIRCUIT_HW_CUK] = 10,
    [CIRCUIT_HW_INTERLEAVED] = 2.0, [CIRCUIT_HW_PDN] = 0.5, [CIRCUIT_HW_CAPS] = 1.0,
    [CIRCUIT_HW_MATCH] = 0.5, [CIRCUIT_HW_REFLECT] = 2, [CIRCUIT_HW_LOOP] = 5,
    [CIRCUIT_ID_SOURCE] = 2, [CIRCUIT_ID_DIODE] = 0.2, [CIRCUIT_ID_CAP] = 0.1,
    [CIRCUIT_ID_IND] = 5, [CIRCUIT_ID_OPAMP] = 0.5, [CIRCUIT_ID_BJT] = 2.0, [CIRCUIT_ID_MOSFET] = 2.0,
    [CIRCUIT_ID_OPAMP_ERR] = 0.5, [CIRCUIT_PARTS_MOSFET] = 0.1, [CIRCUIT_CAP_DCBIAS] = 0.05, [CIRCUIT_NE555_ASTABLE] = 1.0, [CIRCUIT_PIERCE] = 5.0,
    [CIRCUIT_IV_PROBE_COMP] = 0.2, [CIRCUIT_IV_PROBE_LOADING] = 1.0,
    [CIRCUIT_IV_GROUND_LEAD] = 1.0, [CIRCUIT_IV_SCOPE_INPUT_Z] = 0.5,
    [CIRCUIT_IV_AC_COUPLING] = 0.05, [CIRCUIT_IV_SHUNT_SENSE] = 0.5, [CIRCUIT_IV_KELVIN] = 0.05,
    [CIRCUIT_IV_BUCK_NODES] = 2.0,
    [CIRCUIT_IV_LDO_VS_BUCK] = 2.0, [CIRCUIT_IV_BOOTSTRAP] = 10,
    [CIRCUIT_IV_TERMINATION] = 2, [CIRCUIT_IV_PULLUP_SIZING] = 1.0,
    [CIRCUIT_IV_GROUND_BOUNCE] = 0.5, [CIRCUIT_IV_CROSSTALK] = 0.5, [CIRCUIT_IV_ESD_CLAMP] = 1.0,
    [CIRCUIT_IV_CAP_ENERGY] = 2.0, [CIRCUIT_IV_MILLER] = 0.5,
    [CIRCUIT_IV_SWITCH_CHOICE] = 2.0, [CIRCUIT_IV_INRUSH] = 5, [CIRCUIT_TLINE_REAL] = 0.5,
    [CIRCUIT_SEVENSEG_TEST] = 1.0, [CIRCUIT_WIRELESS_LINK] = 0.5, [CIRCUIT_BCD_COUNTER] = 1.0, [CIRCUIT_DIGITAL_CLOCK] = 1.0,
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
    [CIRCUIT_3PH_Y_BALANCED]   = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_3PH_UNBALANCED]   = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_3PH_345_LINE]     = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_3PH_RECTIFIER]    = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_SCHMITT_BISTABLE] = { DEMO_SWITCH, 100 },
    [CIRCUIT_TRI_SQUARE_GEN]   = { DEMO_OSC, 5000 },
    [CIRCUIT_FUNCTION_GEN]     = { DEMO_OSC, 5000 },
    [CIRCUIT_COLPITTS]         = { DEMO_OSC, 712e3 },
    [CIRCUIT_RING_OSC]         = { DEMO_OSC, 145e3 },
    [CIRCUIT_HARTLEY]          = { DEMO_OSC, 503292 },
    [CIRCUIT_CLAPP]            = { DEMO_OSC, 1743455 },
    [CIRCUIT_THEVENIN]         = { DEMO_DC, 0 },
    [CIRCUIT_SUPERPOSITION]    = { DEMO_DC, 0 },
    [CIRCUIT_RC_STEP]          = { DEMO_WAVEFORM, 100 },
    [CIRCUIT_RL_STEP]          = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_RLC_RING]         = { DEMO_WAVEFORM, 5030 },   // the ring frequency: 6 cycles of ringing in the demo window
    [CIRCUIT_RLC_DAMPING]      = { DEMO_WAVEFORM, 5030 },
    [CIRCUIT_OPAMP_SAT]        = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_SINGLE_TUNED_AMP] = { DEMO_BANDPASS, 100060 },
    [CIRCUIT_COMMON_BASE]      = { DEMO_WAVEFORM, 10000 },
    [CIRCUIT_DARLINGTON]       = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_SR_LATCH]         = { DEMO_SWITCH, 1000 },
    [CIRCUIT_POWER_PLANT]      = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_SUBSTATION]       = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_IO_PUSH_PULL]     = { DEMO_WAVEFORM, 1e6 },
    [CIRCUIT_IO_OPEN_DRAIN]    = { DEMO_WAVEFORM, 200e3 },
    [CIRCUIT_IO_OPEN_COLLECTOR]= { DEMO_WAVEFORM, 100e3 },
    [CIRCUIT_IO_I2C_BUS]       = { DEMO_WAVEFORM, 20e3 },
    [CIRCUIT_IO_I2C_LEVEL]     = { DEMO_WAVEFORM, 100e3 },
    [CIRCUIT_IO_INPUT_DEBOUNCE]= { DEMO_WAVEFORM, 50 },
    [CIRCUIT_IO_LOW_SIDE]      = { DEMO_WAVEFORM, 500 },
    [CIRCUIT_IO_HIGH_SIDE]     = { DEMO_WAVEFORM, 500 },
    [CIRCUIT_IO_SPI]           = { DEMO_WAVEFORM, 5e6 },
    [CIRCUIT_IO_UART]          = { DEMO_WAVEFORM, 4800 },
    [CIRCUIT_IO_RS485]         = { DEMO_WAVEFORM, 500e3 },
    [CIRCUIT_IO_SPMI]          = { DEMO_WAVEFORM, 2.5e6 },
    [CIRCUIT_TX_69KV]          = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_TX_LADDER]        = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_TX_WIND]          = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_TX_PLANT]         = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_RES_SERVICE]      = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_RES_BRANCH]       = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_RES_ACSTART]      = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_RES_SOLAR]        = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_COM_480Y]         = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_COM_208Y]         = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_COM_PFC]          = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_COM_ATS]          = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_GS_N1]            = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_GS_IBR]           = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_GS_BOLD]          = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_GS_DERATE]        = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_GS_FACRATE]       = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_GS_KRON]          = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_GS_RX]            = { DEMO_WAVEFORM, 60 },
    [CIRCUIT_GS_GOVERNOR]      = { DEMO_WAVEFORM, 0.5 },
    [CIRCUIT_GS_PIDS]          = { DEMO_WAVEFORM, 0.2 },
    [CIRCUIT_MOS_IDVGS]        = { DEMO_WAVEFORM, 100 },
    [CIRCUIT_MOS_IDVDS]        = { DEMO_WAVEFORM, 100 },
    [CIRCUIT_MOS_TUNED]        = { DEMO_BANDPASS, 100e3 },
    [CIRCUIT_MOS_CG]           = { DEMO_WAVEFORM, 10e3 },
    [CIRCUIT_MOS_CASCODE]      = { DEMO_WAVEFORM, 10e3 },
    [CIRCUIT_MOS_DIFF]         = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_MOS_MIRROR]       = { DEMO_DC, 0 },
    [CIRCUIT_CMOS_INV]         = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_CMOS_NAND]        = { DEMO_WAVEFORM, 500 },
    [CIRCUIT_CMOS_TGATE]       = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_XY_LISSAJOUS]     = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_XY_PLOTTER]       = { DEMO_WAVEFORM, 100 },
    [CIRCUIT_HW_BUCK]          = { DEMO_DC, 0 },
    [CIRCUIT_HW_BOOST]         = { DEMO_DC, 0 },
    [CIRCUIT_HW_BUCKBOOST]     = { DEMO_DC, 0 },
    [CIRCUIT_HW_CUK]           = { DEMO_WAVEFORM, 100e3 },   // settles to the right mean but still ripples: see ROADMAP
    [CIRCUIT_HW_INTERLEAVED]   = { DEMO_DC, 0 },
    [CIRCUIT_HW_PDN]           = { DEMO_DC, 0 },
    [CIRCUIT_HW_CAPS]          = { DEMO_DC, 0 },
    [CIRCUIT_HW_MATCH]         = { DEMO_WAVEFORM, 1e6 },
    [CIRCUIT_HW_REFLECT]       = { DEMO_WAVEFORM, 250e3 },
    [CIRCUIT_HW_LOOP]          = { DEMO_WAVEFORM, 2e3 },
    [CIRCUIT_ID_SOURCE]        = { DEMO_DC, 0 },
    [CIRCUIT_ID_DIODE]         = { DEMO_WAVEFORM, 1e3 },
    [CIRCUIT_ID_CAP]           = { DEMO_WAVEFORM, 20e3 },   /* the ripple rides the 20 kHz square */
    [CIRCUIT_ID_IND]           = { DEMO_WAVEFORM, 1592 },   /* the ring, not the 125 Hz pulse rate */
    [CIRCUIT_ID_OPAMP]         = { DEMO_WAVEFORM, 100e3 },
    [CIRCUIT_ID_BJT]           = { DEMO_DC, 0 },
    [CIRCUIT_ID_MOSFET]        = { DEMO_DC, 0 },
    [CIRCUIT_ID_OPAMP_ERR]     = { DEMO_DC, 0 },
    [CIRCUIT_PARTS_MOSFET]     = { DEMO_DC, 0 },
    [CIRCUIT_CAP_DCBIAS]       = { DEMO_WAVEFORM, 20e3 },
    [CIRCUIT_NE555_ASTABLE]    = { DEMO_OSC, 4800 },
    [CIRCUIT_PIERCE]           = { DEMO_OSC, 100000 },
    [CIRCUIT_IV_PROBE_COMP]    = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_IV_PROBE_LOADING] = { DEMO_WAVEFORM, 1000000 },
    [CIRCUIT_IV_GROUND_LEAD]   = { DEMO_WAVEFORM, 8333333 },
    [CIRCUIT_IV_SCOPE_INPUT_Z] = { DEMO_WAVEFORM, 10000000 },
    [CIRCUIT_IV_AC_COUPLING]   = { DEMO_WAVEFORM, 100000 },
    [CIRCUIT_IV_SHUNT_SENSE]   = { DEMO_DC, 0 },
    [CIRCUIT_IV_KELVIN]        = { DEMO_DC, 0 },
    [CIRCUIT_IV_BUCK_NODES]    = { DEMO_DC, 0 },
    [CIRCUIT_IV_LDO_VS_BUCK]   = { DEMO_DC, 0 },
    [CIRCUIT_IV_BOOTSTRAP]     = { DEMO_WAVEFORM, 100000 },
    [CIRCUIT_IV_TERMINATION]   = { DEMO_WAVEFORM, 25000000 },
    [CIRCUIT_IV_PULLUP_SIZING] = { DEMO_WAVEFORM, 25000 },
    [CIRCUIT_IV_GROUND_BOUNCE] = { DEMO_WAVEFORM, 20000000 },
    [CIRCUIT_IV_CROSSTALK]     = { DEMO_WAVEFORM, 20000000 },
    [CIRCUIT_IV_ESD_CLAMP]     = { DEMO_DC, 0 },
    [CIRCUIT_IV_CAP_ENERGY]    = { DEMO_DC, 0 },
    [CIRCUIT_IV_MILLER]        = { DEMO_WAVEFORM, 1000000 },
    [CIRCUIT_IV_SWITCH_CHOICE] = { DEMO_DC, 0 },
    [CIRCUIT_IV_INRUSH]        = { DEMO_DC, 0 },
    [CIRCUIT_TLINE_REAL]       = { DEMO_WAVEFORM, 12500000 },
    [CIRCUIT_SEVENSEG_TEST]    = { DEMO_DC, 0 },
    [CIRCUIT_WIRELESS_LINK]    = { DEMO_WAVEFORM, 1000 },
    [CIRCUIT_BCD_COUNTER]      = { DEMO_WAVEFORM, 2 },
    [CIRCUIT_DIGITAL_CLOCK]    = { DEMO_WAVEFORM, 1 },
};

const TemplateDemo *circuit_template_demo(CircuitTemplateType type) {
    static const TemplateDemo none = { DEMO_NONE, 0 };
    if (type <= CIRCUIT_NONE || type >= CIRCUIT_TYPE_COUNT) return &none;
    return &template_demo[type];
}

// Amplifier stages whose input and output sit on different DC levels: stacked + fitted bands
// (each band centred on its own mean and scaled to its own swing) so a 10 mV input and a
// 250 mV output riding on 6 V are both readable.
static const int template_scope_flags[CIRCUIT_TYPE_COUNT] = {
    [CIRCUIT_COMMON_EMITTER] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT, [CIRCUIT_COMMON_SOURCE] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_COMMON_DRAIN] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT, [CIRCUIT_MULTISTAGE_AMP] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_DIFFERENTIAL_PAIR] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT, [CIRCUIT_SINGLE_TUNED_AMP] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_COMMON_BASE] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT, [CIRCUIT_DARLINGTON] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_IO_I2C_BUS] = SCOPE_FLAG_STACK, [CIRCUIT_IO_I2C_LEVEL] = SCOPE_FLAG_STACK, [CIRCUIT_IO_INPUT_DEBOUNCE] = SCOPE_FLAG_STACK,
    [CIRCUIT_IO_HIGH_SIDE] = SCOPE_FLAG_STACK, [CIRCUIT_IO_SPI] = SCOPE_FLAG_STACK, [CIRCUIT_IO_UART] = SCOPE_FLAG_STACK,
    [CIRCUIT_IO_RS485] = SCOPE_FLAG_STACK, [CIRCUIT_IO_SPMI] = SCOPE_FLAG_STACK,
    [CIRCUIT_TX_LADDER] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT, [CIRCUIT_TX_PLANT] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_TX_WIND] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT, [CIRCUIT_RES_SERVICE] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_RES_BRANCH] = SCOPE_FLAG_STACK, [CIRCUIT_RES_SOLAR] = SCOPE_FLAG_STACK,
    [CIRCUIT_RES_ACSTART] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT, [CIRCUIT_COM_PFC] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_COM_480Y] = SCOPE_FLAG_STACK, [CIRCUIT_COM_208Y] = SCOPE_FLAG_STACK, [CIRCUIT_COM_ATS] = SCOPE_FLAG_STACK,
    [CIRCUIT_GS_BOLD] = SCOPE_FLAG_STACK, [CIRCUIT_GS_KRON] = SCOPE_FLAG_STACK, [CIRCUIT_GS_RX] = SCOPE_FLAG_STACK,
    [CIRCUIT_GS_IBR] = SCOPE_FLAG_STACK, [CIRCUIT_GS_GOVERNOR] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_MOS_IDVGS] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT, [CIRCUIT_MOS_IDVDS] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_MOS_TUNED] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT, [CIRCUIT_MOS_CG] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_MOS_CASCODE] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT, [CIRCUIT_MOS_DIFF] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_CMOS_INV] = SCOPE_FLAG_STACK, [CIRCUIT_CMOS_NAND] = SCOPE_FLAG_STACK, [CIRCUIT_CMOS_TGATE] = SCOPE_FLAG_STACK,
    /* A converter's output ripple is what the circuit is about, and it is tens of millivolts on a
       rail of several volts: three hundredths of a division on one shared scale, which reads as
       "this buck has no ripple". Each channel gets its own fitted band, so the rail's ripple
       fills its band and the switching node keeps its own. --probe-audit's RIPPLE flag finds
       these: a trace on the screen whose every movement is thinner than a tenth of a division. */
    [CIRCUIT_HW_BUCK] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_HW_BOOST] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_HW_BUCKBOOST] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_HW_CUK] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_HW_INTERLEAVED] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_CENTERTAP_RECT] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_AC_DC_AMERICAN] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_GS_PIDS] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_MOS_MIRROR] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_HW_PDN] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT, [CIRCUIT_HW_CAPS] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_HW_MATCH] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT, [CIRCUIT_HW_REFLECT] = SCOPE_FLAG_STACK,
    [CIRCUIT_HW_LOOP] = SCOPE_FLAG_STACK,
    [CIRCUIT_ID_SOURCE] = SCOPE_FLAG_STACK, [CIRCUIT_ID_DIODE] = SCOPE_FLAG_STACK,
    [CIRCUIT_ID_CAP] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT, [CIRCUIT_ID_IND] = SCOPE_FLAG_STACK,
    [CIRCUIT_ID_OPAMP] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_ID_BJT] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT, [CIRCUIT_ID_MOSFET] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_ID_OPAMP_ERR] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_PARTS_MOSFET] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_CAP_DCBIAS] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_NE555_ASTABLE] = SCOPE_FLAG_STACK,
    [CIRCUIT_PIERCE] = SCOPE_FLAG_AC,
    [CIRCUIT_IV_PROBE_COMP] = SCOPE_FLAG_STACK,
    [CIRCUIT_IV_PROBE_LOADING] = SCOPE_FLAG_STACK,
    [CIRCUIT_IV_GROUND_LEAD] = SCOPE_FLAG_STACK,
    [CIRCUIT_IV_SCOPE_INPUT_Z] = SCOPE_FLAG_STACK,
    [CIRCUIT_IV_AC_COUPLING] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_IV_BUCK_NODES] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_IV_LDO_VS_BUCK] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_IV_BOOTSTRAP] = SCOPE_FLAG_STACK,
    [CIRCUIT_IV_TERMINATION] = SCOPE_FLAG_STACK,
    [CIRCUIT_IV_PULLUP_SIZING] = SCOPE_FLAG_STACK,
    [CIRCUIT_IV_GROUND_BOUNCE] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_IV_CROSSTALK] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_IV_CAP_ENERGY] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_IV_MILLER] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_IV_INRUSH] = SCOPE_FLAG_STACK,
    [CIRCUIT_TLINE_REAL] = SCOPE_FLAG_STACK,
    /* The neutral shift is the whole point of the unbalanced Y, and it is tens of volts beside
       phases of four hundred: on one shared scale it is a fifth of a division and invisible.
       Each channel gets its own band so the small one is readable next to the large ones. */
    [CIRCUIT_3PH_UNBALANCED] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_SEVENSEG_TEST] = SCOPE_FLAG_FIT, [CIRCUIT_WIRELESS_LINK] = SCOPE_FLAG_FIT,
    [CIRCUIT_BCD_COUNTER] = SCOPE_FLAG_FIT, [CIRCUIT_DIGITAL_CLOCK] = SCOPE_FLAG_FIT,
    /* LC oscillators swing about a 12 V rail: AC-couple them so the tank waveform is centred */
    [CIRCUIT_COLPITTS] = SCOPE_FLAG_AC, [CIRCUIT_HARTLEY] = SCOPE_FLAG_AC, [CIRCUIT_CLAPP] = SCOPE_FLAG_AC,
    [CIRCUIT_SINGLE_TUNED_AMP] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT,
    [CIRCUIT_SUMMING_AMP] = SCOPE_FLAG_STACK, [CIRCUIT_DIFFERENCE_AMP] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT, [CIRCUIT_INSTR_AMP] = SCOPE_FLAG_STACK | SCOPE_FLAG_FIT, [CIRCUIT_SUPERPOSITION] = SCOPE_FLAG_STACK,
};
int circuit_template_scope_flags(CircuitTemplateType type) {
    if (type <= CIRCUIT_NONE || type >= CIRCUIT_TYPE_COUNT) return 0;
    return template_scope_flags[type];
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

/* What a probe is sitting on, in the seven characters a probe label holds. Every part that has
   named terminals says which one; anything else is a node in the middle of the circuit. */
static const char *derived_probe_name(ComponentType ct, int term) {
    switch (ct) {
        case COMP_NPN_BJT: case COMP_PNP_BJT:
        case COMP_NPN_DARLINGTON: case COMP_PNP_DARLINGTON:
            return term == 0 ? "BASE" : term == 1 ? "COLL" : "EMIT";
        case COMP_NMOS: case COMP_PMOS: case COMP_NJFET: case COMP_PJFET:
            return term == 0 ? "GATE" : term == 1 ? "DRAIN" : "SRC";
        case COMP_DIODE: case COMP_ZENER: case COMP_LED: case COMP_SCHOTTKY:
            return term == 0 ? "ANODE" : "CATH";
        case COMP_CAPACITOR: return "VCAP";
        case COMP_INDUCTOR:  return "VL";
        case COMP_OPAMP:     return term == 2 ? "OUT" : term == 0 ? "V-" : "V+";
        case COMP_AND_GATE: case COMP_OR_GATE: case COMP_NAND_GATE: case COMP_NOR_GATE:
        case COMP_XOR_GATE: case COMP_NOT_GATE:
            return term >= 2 ? "Q" : term == 0 ? "A" : "B";
        case COMP_TRANSFORMER: return term < 2 ? "PRI" : "SEC";
        case COMP_RESISTOR:  return "VR";
        default:             return "NODE";
    }
}

/* Place a probe and give it a name. Names have to be unique inside a circuit, because they are
   also the scope's channel names: two traces both called VCAP cannot be told apart. */
static void probe_named(Circuit *circuit, Component *c, int term, const char *name) {
    if (!c || term < 0 || term >= c->num_terminals) return;
    Node *n = circuit_get_node(circuit, c->node_ids[term]);
    if (!n) return;
    for (int i = 0; i < circuit->num_probes; i++)
        if (circuit->probes[i].node_id == n->id) return;   // already probed
    int id = circuit_add_probe(circuit, n->id, n->x, n->y);   /* returns the probe's id, = index+1 */
    int idx = id - 1;
    if (id <= 0 || idx >= circuit->num_probes) return;
    Probe *p = &circuit->probes[idx];
    const char *want = (name && name[0]) ? name : derived_probe_name(c->type, term);
    for (int attempt = 0; attempt < 9; attempt++) {
        char cand[8];
        if (attempt == 0) snprintf(cand, sizeof cand, "%s", want);
        else snprintf(cand, sizeof cand, "%.*s%d", (int)sizeof cand - 2, want, attempt + 1);
        bool taken = false;
        for (int i = 0; i < circuit->num_probes; i++)
            if (i != idx && strcmp(circuit->probes[i].label, cand) == 0) { taken = true; break; }
        if (!taken) { snprintf(p->label, sizeof p->label, "%s", cand); return; }
    }
}

static void probe_component_terminal(Circuit *circuit, Component *c, int term) {
    probe_named(circuit, c, term, NULL);
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

    // Power-system / high-voltage templates: loads, line, fault and even CT-burden resistors
    // dissipate 100s of W to MW. Make them all high-power loads (box symbol, no thermal warning);
    // low-voltage templates keep the normal resistor and its overload warning (--burn-test enforces).
    {
        const CircuitTemplateInfo *tinfo = circuit_template_get_info(type);
        if (tinfo && (tinfo->group == TG_POWER_SYSTEMS || tinfo->group == TG_HIGH_VOLTAGE || tinfo->group == TG_BUILDING || tinfo->group == TG_GRID_STD ||
                      tinfo->group == TG_HARDWARE)) {
            // Building services: a resistor is either a load (>= 1 ohm: an appliance, a motor, a
            // lighting circuit - draw it as a load box) or a conductor / shunt (< 1 ohm: keep the
            // resistor symbol, but it is wire, not a 1/4 W part, so it has no thermal limit either).
            bool building = (tinfo->group == TG_BUILDING || tinfo->group == TG_GRID_STD || tinfo->group == TG_HARDWARE);
            for (int i = first; i < circuit->num_components; i++) {
                Component *c = circuit->components[i];
                if (c->type == COMP_RESISTOR && c->props.resistor.power_rating <= 0.2501) {   // an explicit rating (FAC-008) is left alone
                    if (!building || c->props.resistor.resistance >= 1.0) c->props.resistor.high_power = true;
                    c->props.resistor.power_rating = 1e12;
                    c->thermal.max_temperature = 0.0;
                }
            }
        }
    }

    // Auto-place probes: CH1 on the input source (+ terminal), CH2 on the output node,
    // so the scope shows the circuit working the moment it is run.
    {
        static const ComponentType src_types[] = { COMP_AC_VOLTAGE, COMP_SOURCE_3PH, COMP_SQUARE_WAVE, COMP_TRIANGLE_WAVE,
                                                   COMP_PULSE_SOURCE, COMP_DC_CURRENT, COMP_DC_VOLTAGE };
        Component *src = NULL;
        for (unsigned k = 0; k < sizeof src_types / sizeof src_types[0] && !src; k++)
            src = nth_of_type(circuit, first, src_types[k], 0);
        const TemplateProbeSpec *spec = &template_output[type];
        Component *out = spec->ct ? nth_of_type(circuit, first, spec->ct, spec->ord) : NULL;
        const TemplateDemo *dm = &template_demo[type];
        bool osc = (dm->kind == DEMO_OSC);   // oscillators: the only 'source' is a start-up kick - do not probe it
        if (src && !osc) {
            // probe the source terminal that is not ground (a rotated current source has its '+' on ground)
            int st = 0;
            for (int i = 0; i < circuit->num_components; i++)
                if (circuit->components[i]->type == COMP_GROUND && circuit->components[i]->node_ids[0] == src->node_ids[0]) st = 1;
            probe_named(circuit, src, st, "IN");
        }
        if (out) probe_named(circuit, out, spec->term, spec->name ? spec->name : "OUT");
        for (int e = 0; e < 3; e++) {
            const TemplateProbeSpec *xs = &template_extra_probes[type][e];
            if (!xs->ct) continue;
            Component *xc = nth_of_type(circuit, first, xs->ct, xs->ord);
            if (xc) probe_named(circuit, xc, xs->term, xs->name);
        }
    }

    /* Each note is drawn wrapped, so the next one starts below however many lines this one
       took. A fixed 16 px step printed them on top of each other the moment they wrapped. */
    float ty = max_y + 60.0f;
    for (int l = 0; l < 6 && template_notes[type][l]; l++) {
        Component *txt = add_comp(circuit, COMP_TEXT, min_x, ty, 0);
        if (!txt) break;
        strncpy(txt->props.text.text, template_notes[type][l], sizeof(txt->props.text.text) - 1);
        txt->props.text.font_size = 1;
        int st[CANVAS_TEXT_MAX_LINES], ln[CANVAS_TEXT_MAX_LINES];
        int nl = label_wrap(template_notes[type][l], CANVAS_TEXT_WRAP, st, ln, CANVAS_TEXT_MAX_LINES);
        ty += (float)(nl > 0 ? nl : 1) * (CANVAS_TEXT_PX + 2) + 4.0f;
        count++;
    }
    return count;
}

const char *circuit_template_group_name(TemplateGroup g) {
    static const char *names[TG_COUNT] = {
        "Basics", "Filters", "Op-amps", "Transistors", "Oscillators", "Power supplies", "Digital", "Power systems", "High voltage", "Transients", "IC I/O & drivers", "Residential & commercial", "Grid standards & methods", "Hardware engineering", "Ideal vs real models",
        "Interview: instrumentation & scope", "Interview: fundamentals",
        "Interview: power & converters", "Interview: I/O & signal integrity"
    };
    return (g >= 0 && g < TG_COUNT) ? names[g] : "?";
}
