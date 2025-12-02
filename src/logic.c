/**
 * Circuit Playground - Mixed-Signal Logic Solver Implementation
 *
 * This module provides efficient simulation of digital logic circuits by:
 * 1. Abstracting logic gates from the full MNA matrix solve
 * 2. Propagating boolean states (HIGH, LOW, FLOAT) based on input thresholds
 * 3. Automatically converting at analog/digital boundaries (ADC/DAC bridges)
 */

#include "logic.h"
#include "component.h"
#include "circuit.h"
#include "simulation.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// Logic Family Default Levels
// ============================================================================

static const LogicLevels LOGIC_LEVELS_TTL = {
    .v_il = 0.8,      // Input low threshold
    .v_ih = 2.0,      // Input high threshold
    .v_ol = 0.4,      // Output low voltage
    .v_oh = 2.4,      // Output high voltage (can go to 3.4V with pullup)
    .v_hyst = 0.0,    // No hysteresis
    .r_out = 50.0     // Typical TTL output impedance
};

static const LogicLevels LOGIC_LEVELS_CMOS_5V = {
    .v_il = 1.5,      // 30% of VCC
    .v_ih = 3.5,      // 70% of VCC
    .v_ol = 0.0,      // Rail-to-rail low
    .v_oh = 5.0,      // Rail-to-rail high
    .v_hyst = 0.0,    // No hysteresis
    .r_out = 100.0    // CMOS output impedance
};

static const LogicLevels LOGIC_LEVELS_CMOS_3V3 = {
    .v_il = 0.8,      // ~25% of VCC
    .v_ih = 2.0,      // ~60% of VCC
    .v_ol = 0.0,
    .v_oh = 3.3,
    .v_hyst = 0.0,
    .r_out = 100.0
};

static const LogicLevels LOGIC_LEVELS_LVCMOS = {
    .v_il = 0.35,     // ~20% of 1.8V
    .v_ih = 1.1,      // ~60% of 1.8V
    .v_ol = 0.0,
    .v_oh = 1.8,
    .v_hyst = 0.0,
    .r_out = 150.0
};

// Schmitt trigger levels (CMOS 5V with hysteresis)
static const LogicLevels LOGIC_LEVELS_SCHMITT = {
    .v_il = 1.5,      // Low threshold
    .v_ih = 3.5,      // High threshold
    .v_ol = 0.0,
    .v_oh = 5.0,
    .v_hyst = 0.8,    // Hysteresis amount
    .r_out = 100.0
};

// ============================================================================
// BCD to 7-Segment Lookup Table
// ============================================================================

// Segment arrangement:    a
//                       f   b
//                         g
//                       e   c
//                         d
//
// Bit mapping: bit0=a, bit1=b, bit2=c, bit3=d, bit4=e, bit5=f, bit6=g

static const uint8_t BCD_TO_7SEG[16] = {
    0x3F,  // 0: a,b,c,d,e,f    = 0011 1111
    0x06,  // 1: b,c            = 0000 0110
    0x5B,  // 2: a,b,d,e,g      = 0101 1011
    0x4F,  // 3: a,b,c,d,g      = 0100 1111
    0x66,  // 4: b,c,f,g        = 0110 0110
    0x6D,  // 5: a,c,d,f,g      = 0110 1101
    0x7D,  // 6: a,c,d,e,f,g    = 0111 1101
    0x07,  // 7: a,b,c          = 0000 0111
    0x7F,  // 8: a,b,c,d,e,f,g  = 0111 1111
    0x6F,  // 9: a,b,c,d,f,g    = 0110 1111
    0x00,  // 10: blank (invalid BCD)
    0x00,  // 11: blank
    0x00,  // 12: blank
    0x00,  // 13: blank
    0x00,  // 14: blank
    0x00   // 15: blank
};

// ============================================================================
// Logic Level Functions
// ============================================================================

LogicLevels logic_get_family_levels(LogicFamily family) {
    switch (family) {
        case LOGIC_FAMILY_TTL:
            return LOGIC_LEVELS_TTL;
        case LOGIC_FAMILY_CMOS_5V:
            return LOGIC_LEVELS_CMOS_5V;
        case LOGIC_FAMILY_CMOS_3V3:
            return LOGIC_LEVELS_CMOS_3V3;
        case LOGIC_FAMILY_LVCMOS:
            return LOGIC_LEVELS_LVCMOS;
        case LOGIC_FAMILY_CUSTOM:
        default:
            return LOGIC_LEVELS_CMOS_5V;  // Default to 5V CMOS
    }
}

// ============================================================================
// ADC/DAC Bridge Functions
// ============================================================================

LogicState logic_voltage_to_state(double voltage, const LogicLevels *levels,
                                   LogicState prev_state) {
    // Handle hysteresis for Schmitt triggers
    if (levels->v_hyst > 0.0) {
        // If previously HIGH, use lower threshold to go LOW
        // If previously LOW, use higher threshold to go HIGH
        if (prev_state == LOGIC_HIGH) {
            double low_thresh = levels->v_ih - levels->v_hyst;
            if (voltage < low_thresh) {
                return LOGIC_LOW;
            }
            return LOGIC_HIGH;
        } else {
            double high_thresh = levels->v_il + levels->v_hyst;
            if (voltage > high_thresh) {
                return LOGIC_HIGH;
            }
            return LOGIC_LOW;
        }
    }

    // Standard thresholding without hysteresis
    if (voltage <= levels->v_il) {
        return LOGIC_LOW;
    } else if (voltage >= levels->v_ih) {
        return LOGIC_HIGH;
    } else {
        // In the undefined region - return previous state or X
        if (prev_state == LOGIC_HIGH || prev_state == LOGIC_LOW) {
            return prev_state;
        }
        return LOGIC_X;  // Unknown state
    }
}

double logic_state_to_voltage(LogicState state, const LogicLevels *levels) {
    switch (state) {
        case LOGIC_HIGH:
            return levels->v_oh;
        case LOGIC_LOW:
            return levels->v_ol;
        case LOGIC_Z:
            // High impedance - return midpoint (won't actually drive)
            return (levels->v_oh + levels->v_ol) / 2.0;
        case LOGIC_X:
        default:
            // Unknown - return midpoint
            return (levels->v_oh + levels->v_ol) / 2.0;
    }
}

// ============================================================================
// Component Initialization
// ============================================================================

void logic_init_component(Component *comp) {
    if (!comp) return;

    // Initialize logic state
    LogicGateState *ls = &comp->logic_state;
    memset(ls, 0, sizeof(LogicGateState));

    // Set defaults
    ls->family = LOGIC_FAMILY_CMOS_5V;
    ls->levels = logic_get_family_levels(ls->family);
    ls->is_logic_component = logic_is_logic_component(comp->type);

    // Initialize all inputs/outputs to unknown
    for (int i = 0; i < MAX_LOGIC_INPUTS; i++) {
        ls->inputs[i] = LOGIC_X;
        ls->prev_inputs[i] = LOGIC_X;
    }
    for (int i = 0; i < MAX_LOGIC_OUTPUTS; i++) {
        ls->outputs[i] = LOGIC_X;
        ls->prev_outputs[i] = LOGIC_X;
    }

    // Sequential logic initial state
    ls->q = LOGIC_LOW;
    ls->q_bar = LOGIC_HIGH;
    ls->sr_set = LOGIC_LOW;
    ls->sr_reset = LOGIC_LOW;

    // Schmitt trigger initial state
    ls->schmitt_state = false;

    // Set Schmitt trigger hysteresis levels
    if (comp->type == COMP_SCHMITT_INV || comp->type == COMP_SCHMITT_BUF) {
        ls->levels = LOGIC_LEVELS_SCHMITT;
    }
}

// ============================================================================
// Logic Component Detection
// ============================================================================

bool logic_is_logic_component(ComponentType type) {
    switch (type) {
        // Logic gates
        case COMP_NOT_GATE:
        case COMP_AND_GATE:
        case COMP_OR_GATE:
        case COMP_NAND_GATE:
        case COMP_NOR_GATE:
        case COMP_XOR_GATE:
        case COMP_XNOR_GATE:
        case COMP_BUFFER:
        case COMP_TRISTATE_BUF:
        case COMP_SCHMITT_INV:
        case COMP_SCHMITT_BUF:
        // Sequential logic
        case COMP_D_FLIPFLOP:
        case COMP_JK_FLIPFLOP:
        case COMP_T_FLIPFLOP:
        case COMP_SR_LATCH:
        // Digital ICs
        case COMP_COUNTER:
        case COMP_SHIFT_REG:
        case COMP_MUX_2TO1:
        case COMP_DEMUX_1TO2:
        case COMP_DECODER:
        case COMP_BCD_DECODER:
        case COMP_HALF_ADDER:
        case COMP_FULL_ADDER:
        // Logic I/O
        case COMP_LOGIC_INPUT:
        case COMP_LOGIC_OUTPUT:
            return true;
        default:
            return false;
    }
}

bool logic_is_sequential(ComponentType type) {
    switch (type) {
        case COMP_D_FLIPFLOP:
        case COMP_JK_FLIPFLOP:
        case COMP_T_FLIPFLOP:
        case COMP_SR_LATCH:
        case COMP_COUNTER:
        case COMP_SHIFT_REG:
            return true;
        default:
            return false;
    }
}

// ============================================================================
// Edge Detection
// ============================================================================

EdgeType logic_detect_edge(LogicState current, LogicState previous) {
    if (previous == LOGIC_LOW && current == LOGIC_HIGH) {
        return EDGE_RISING;
    } else if (previous == LOGIC_HIGH && current == LOGIC_LOW) {
        return EDGE_FALLING;
    }
    return EDGE_NONE;
}

// ============================================================================
// Input Sampling (ADC Phase)
// ============================================================================

// Helper function to get node voltage safely
static double get_node_voltage(Circuit *circuit, int node_id) {
    if (node_id <= 0) return 0.0;
    Node *node = circuit_get_node(circuit, node_id);
    return node ? node->voltage : 0.0;
}

void logic_sample_inputs(Simulation *sim, Circuit *circuit) {
    if (!sim || !circuit) return;

    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        if (!comp || !comp->logic_state.is_logic_component) continue;

        LogicGateState *ls = &comp->logic_state;

        // Save previous inputs for edge detection
        for (int j = 0; j < MAX_LOGIC_INPUTS; j++) {
            ls->prev_inputs[j] = ls->inputs[j];
        }

        // Sample inputs from analog nodes using node_ids
        // The mapping depends on component type
        switch (comp->type) {
            case COMP_NOT_GATE:
            case COMP_BUFFER:
            case COMP_SCHMITT_INV:
            case COMP_SCHMITT_BUF:
                // Single input on node_ids[0]
                if (comp->node_ids[0] > 0) {
                    double v = get_node_voltage(circuit, comp->node_ids[0]);
                    ls->inputs[0] = logic_voltage_to_state(v, &ls->levels, ls->prev_inputs[0]);
                }
                break;

            case COMP_AND_GATE:
            case COMP_OR_GATE:
            case COMP_NAND_GATE:
            case COMP_NOR_GATE:
            case COMP_XOR_GATE:
            case COMP_XNOR_GATE:
                // Two inputs on node_ids[0] and node_ids[1]
                if (comp->node_ids[0] > 0) {
                    double v = get_node_voltage(circuit, comp->node_ids[0]);
                    ls->inputs[0] = logic_voltage_to_state(v, &ls->levels, ls->prev_inputs[0]);
                }
                if (comp->node_ids[1] > 0) {
                    double v = get_node_voltage(circuit, comp->node_ids[1]);
                    ls->inputs[1] = logic_voltage_to_state(v, &ls->levels, ls->prev_inputs[1]);
                }
                break;

            case COMP_D_FLIPFLOP:
                // D input on node_ids[0], CLK on node_ids[1]
                if (comp->node_ids[0] > 0) {
                    double v = get_node_voltage(circuit, comp->node_ids[0]);
                    ls->inputs[0] = logic_voltage_to_state(v, &ls->levels, ls->prev_inputs[0]);  // D
                }
                if (comp->node_ids[1] > 0) {
                    double v = get_node_voltage(circuit, comp->node_ids[1]);
                    ls->inputs[1] = logic_voltage_to_state(v, &ls->levels, ls->prev_inputs[1]);  // CLK
                }
                break;

            case COMP_JK_FLIPFLOP:
                // J on node_ids[0], K on node_ids[1], CLK on node_ids[2]
                if (comp->node_ids[0] > 0) {
                    double v = get_node_voltage(circuit, comp->node_ids[0]);
                    ls->inputs[0] = logic_voltage_to_state(v, &ls->levels, ls->prev_inputs[0]);  // J
                }
                if (comp->node_ids[1] > 0) {
                    double v = get_node_voltage(circuit, comp->node_ids[1]);
                    ls->inputs[1] = logic_voltage_to_state(v, &ls->levels, ls->prev_inputs[1]);  // K
                }
                if (comp->node_ids[2] > 0) {
                    double v = get_node_voltage(circuit, comp->node_ids[2]);
                    ls->inputs[2] = logic_voltage_to_state(v, &ls->levels, ls->prev_inputs[2]);  // CLK
                }
                break;

            case COMP_T_FLIPFLOP:
                // T on node_ids[0], CLK on node_ids[1]
                if (comp->node_ids[0] > 0) {
                    double v = get_node_voltage(circuit, comp->node_ids[0]);
                    ls->inputs[0] = logic_voltage_to_state(v, &ls->levels, ls->prev_inputs[0]);  // T
                }
                if (comp->node_ids[1] > 0) {
                    double v = get_node_voltage(circuit, comp->node_ids[1]);
                    ls->inputs[1] = logic_voltage_to_state(v, &ls->levels, ls->prev_inputs[1]);  // CLK
                }
                break;

            case COMP_SR_LATCH:
                // S on node_ids[0], R on node_ids[1]
                if (comp->node_ids[0] > 0) {
                    double v = get_node_voltage(circuit, comp->node_ids[0]);
                    ls->inputs[0] = logic_voltage_to_state(v, &ls->levels, ls->prev_inputs[0]);  // S
                }
                if (comp->node_ids[1] > 0) {
                    double v = get_node_voltage(circuit, comp->node_ids[1]);
                    ls->inputs[1] = logic_voltage_to_state(v, &ls->levels, ls->prev_inputs[1]);  // R
                }
                break;

            case COMP_BCD_DECODER:
                // 4 BCD inputs: D(MSB) on node_ids[0], C on node_ids[1], B on node_ids[2], A(LSB) on node_ids[3]
                for (int j = 0; j < 4; j++) {
                    if (comp->node_ids[j] > 0) {
                        double v = get_node_voltage(circuit, comp->node_ids[j]);
                        ls->inputs[j] = logic_voltage_to_state(v, &ls->levels, ls->prev_inputs[j]);
                    }
                }
                break;

            case COMP_MUX_2TO1:
                // Input A on node_ids[0], Input B on node_ids[1], Select on node_ids[2]
                for (int j = 0; j < 3; j++) {
                    if (comp->node_ids[j] > 0) {
                        double v = get_node_voltage(circuit, comp->node_ids[j]);
                        ls->inputs[j] = logic_voltage_to_state(v, &ls->levels, ls->prev_inputs[j]);
                    }
                }
                break;

            case COMP_HALF_ADDER:
            case COMP_FULL_ADDER:
                // Half adder: A on node_ids[0], B on node_ids[1]
                // Full adder: A on node_ids[0], B on node_ids[1], Cin on node_ids[2]
                for (int j = 0; j < 3; j++) {
                    if (comp->node_ids[j] > 0) {
                        double v = get_node_voltage(circuit, comp->node_ids[j]);
                        ls->inputs[j] = logic_voltage_to_state(v, &ls->levels, ls->prev_inputs[j]);
                    }
                }
                break;

            default:
                break;
        }
    }
}

// ============================================================================
// Logic Propagation
// ============================================================================

int logic_propagate(Circuit *circuit, double time, double dt) {
    if (!circuit) return 0;

    int changes = 0;

    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        if (!comp || !comp->logic_state.is_logic_component) continue;

        if (logic_propagate_component(comp, time)) {
            changes++;
        }
    }

    return changes;
}

bool logic_propagate_component(Component *comp, double time) {
    if (!comp) return false;

    LogicGateState *ls = &comp->logic_state;

    // Save previous outputs
    for (int i = 0; i < MAX_LOGIC_OUTPUTS; i++) {
        ls->prev_outputs[i] = ls->outputs[i];
    }

    // Propagate based on component type
    switch (comp->type) {
        case COMP_NOT_GATE:
            logic_propagate_not(comp);
            break;
        case COMP_AND_GATE:
            logic_propagate_and(comp);
            break;
        case COMP_OR_GATE:
            logic_propagate_or(comp);
            break;
        case COMP_NAND_GATE:
            logic_propagate_nand(comp);
            break;
        case COMP_NOR_GATE:
            logic_propagate_nor(comp);
            break;
        case COMP_XOR_GATE:
            logic_propagate_xor(comp);
            break;
        case COMP_XNOR_GATE:
            logic_propagate_xnor(comp);
            break;
        case COMP_BUFFER:
            logic_propagate_buffer(comp);
            break;
        case COMP_SCHMITT_INV:
            logic_propagate_schmitt_inv(comp);
            break;
        case COMP_SCHMITT_BUF:
            logic_propagate_schmitt_buf(comp);
            break;
        case COMP_D_FLIPFLOP:
            logic_propagate_d_flipflop(comp, time);
            break;
        case COMP_JK_FLIPFLOP:
            logic_propagate_jk_flipflop(comp, time);
            break;
        case COMP_T_FLIPFLOP:
            logic_propagate_t_flipflop(comp, time);
            break;
        case COMP_SR_LATCH:
            logic_propagate_sr_latch(comp);
            break;
        case COMP_BCD_DECODER:
            logic_propagate_bcd_decoder(comp);
            break;
        case COMP_DECODER:
            logic_propagate_decoder(comp);
            break;
        case COMP_MUX_2TO1:
            logic_propagate_mux(comp);
            break;
        case COMP_DEMUX_1TO2:
            logic_propagate_demux(comp);
            break;
        case COMP_HALF_ADDER:
            logic_propagate_half_adder(comp);
            break;
        case COMP_FULL_ADDER:
            logic_propagate_full_adder(comp);
            break;
        default:
            break;
    }

    // Check if any output changed
    for (int i = 0; i < MAX_LOGIC_OUTPUTS; i++) {
        if (ls->outputs[i] != ls->prev_outputs[i]) {
            ls->outputs_dirty = true;
            ls->last_change_time = time;
            return true;
        }
    }

    return false;
}

// ============================================================================
// Combinational Gate Propagation
// ============================================================================

void logic_propagate_not(Component *comp) {
    LogicGateState *ls = &comp->logic_state;

    switch (ls->inputs[0]) {
        case LOGIC_LOW:
            ls->outputs[0] = LOGIC_HIGH;
            break;
        case LOGIC_HIGH:
            ls->outputs[0] = LOGIC_LOW;
            break;
        default:
            ls->outputs[0] = LOGIC_X;
            break;
    }
}

void logic_propagate_buffer(Component *comp) {
    LogicGateState *ls = &comp->logic_state;
    ls->outputs[0] = ls->inputs[0];
}

void logic_propagate_and(Component *comp) {
    LogicGateState *ls = &comp->logic_state;
    LogicState a = ls->inputs[0];
    LogicState b = ls->inputs[1];

    // AND: output HIGH only if both inputs HIGH
    if (a == LOGIC_HIGH && b == LOGIC_HIGH) {
        ls->outputs[0] = LOGIC_HIGH;
    } else if (a == LOGIC_LOW || b == LOGIC_LOW) {
        ls->outputs[0] = LOGIC_LOW;
    } else {
        ls->outputs[0] = LOGIC_X;
    }
}

void logic_propagate_or(Component *comp) {
    LogicGateState *ls = &comp->logic_state;
    LogicState a = ls->inputs[0];
    LogicState b = ls->inputs[1];

    // OR: output HIGH if either input HIGH
    if (a == LOGIC_HIGH || b == LOGIC_HIGH) {
        ls->outputs[0] = LOGIC_HIGH;
    } else if (a == LOGIC_LOW && b == LOGIC_LOW) {
        ls->outputs[0] = LOGIC_LOW;
    } else {
        ls->outputs[0] = LOGIC_X;
    }
}

void logic_propagate_nand(Component *comp) {
    LogicGateState *ls = &comp->logic_state;
    LogicState a = ls->inputs[0];
    LogicState b = ls->inputs[1];

    // NAND: inverted AND
    if (a == LOGIC_HIGH && b == LOGIC_HIGH) {
        ls->outputs[0] = LOGIC_LOW;
    } else if (a == LOGIC_LOW || b == LOGIC_LOW) {
        ls->outputs[0] = LOGIC_HIGH;
    } else {
        ls->outputs[0] = LOGIC_X;
    }
}

void logic_propagate_nor(Component *comp) {
    LogicGateState *ls = &comp->logic_state;
    LogicState a = ls->inputs[0];
    LogicState b = ls->inputs[1];

    // NOR: inverted OR
    if (a == LOGIC_HIGH || b == LOGIC_HIGH) {
        ls->outputs[0] = LOGIC_LOW;
    } else if (a == LOGIC_LOW && b == LOGIC_LOW) {
        ls->outputs[0] = LOGIC_HIGH;
    } else {
        ls->outputs[0] = LOGIC_X;
    }
}

void logic_propagate_xor(Component *comp) {
    LogicGateState *ls = &comp->logic_state;
    LogicState a = ls->inputs[0];
    LogicState b = ls->inputs[1];

    // XOR: output HIGH if inputs are different
    if ((a == LOGIC_HIGH || a == LOGIC_LOW) && (b == LOGIC_HIGH || b == LOGIC_LOW)) {
        ls->outputs[0] = (a != b) ? LOGIC_HIGH : LOGIC_LOW;
    } else {
        ls->outputs[0] = LOGIC_X;
    }
}

void logic_propagate_xnor(Component *comp) {
    LogicGateState *ls = &comp->logic_state;
    LogicState a = ls->inputs[0];
    LogicState b = ls->inputs[1];

    // XNOR: output HIGH if inputs are same
    if ((a == LOGIC_HIGH || a == LOGIC_LOW) && (b == LOGIC_HIGH || b == LOGIC_LOW)) {
        ls->outputs[0] = (a == b) ? LOGIC_HIGH : LOGIC_LOW;
    } else {
        ls->outputs[0] = LOGIC_X;
    }
}

// ============================================================================
// Schmitt Trigger Propagation (with Hysteresis)
// ============================================================================

void logic_propagate_schmitt_inv(Component *comp) {
    LogicGateState *ls = &comp->logic_state;

    // Hysteresis is already handled in logic_voltage_to_state
    // Just invert the input
    switch (ls->inputs[0]) {
        case LOGIC_LOW:
            ls->outputs[0] = LOGIC_HIGH;
            ls->schmitt_state = true;
            break;
        case LOGIC_HIGH:
            ls->outputs[0] = LOGIC_LOW;
            ls->schmitt_state = false;
            break;
        default:
            // Keep previous state during uncertainty
            ls->outputs[0] = ls->schmitt_state ? LOGIC_HIGH : LOGIC_LOW;
            break;
    }
}

void logic_propagate_schmitt_buf(Component *comp) {
    LogicGateState *ls = &comp->logic_state;

    // Non-inverting Schmitt trigger
    switch (ls->inputs[0]) {
        case LOGIC_LOW:
            ls->outputs[0] = LOGIC_LOW;
            ls->schmitt_state = false;
            break;
        case LOGIC_HIGH:
            ls->outputs[0] = LOGIC_HIGH;
            ls->schmitt_state = true;
            break;
        default:
            // Keep previous state during uncertainty
            ls->outputs[0] = ls->schmitt_state ? LOGIC_HIGH : LOGIC_LOW;
            break;
    }
}

// ============================================================================
// Sequential Logic Propagation
// ============================================================================

void logic_propagate_d_flipflop(Component *comp, double time) {
    LogicGateState *ls = &comp->logic_state;

    // D flip-flop: On rising clock edge, Q = D
    LogicState d = ls->inputs[0];
    EdgeType clk_edge = logic_detect_edge(ls->inputs[1], ls->prev_inputs[1]);

    if (clk_edge == EDGE_RISING) {
        // Latch D input to Q on rising edge
        if (d == LOGIC_HIGH) {
            ls->q = LOGIC_HIGH;
            ls->q_bar = LOGIC_LOW;
        } else if (d == LOGIC_LOW) {
            ls->q = LOGIC_LOW;
            ls->q_bar = LOGIC_HIGH;
        }
        // If D is unknown, Q becomes unknown
        else {
            ls->q = LOGIC_X;
            ls->q_bar = LOGIC_X;
        }
    }

    // Output Q and Q_bar
    ls->outputs[0] = ls->q;
    ls->outputs[1] = ls->q_bar;
}

void logic_propagate_jk_flipflop(Component *comp, double time) {
    LogicGateState *ls = &comp->logic_state;

    // JK flip-flop: On rising clock edge
    // J=0, K=0: Hold
    // J=0, K=1: Reset (Q=0)
    // J=1, K=0: Set (Q=1)
    // J=1, K=1: Toggle

    LogicState j = ls->inputs[0];
    LogicState k = ls->inputs[1];
    EdgeType clk_edge = logic_detect_edge(ls->inputs[2], ls->prev_inputs[2]);

    if (clk_edge == EDGE_RISING) {
        if (j == LOGIC_LOW && k == LOGIC_LOW) {
            // Hold - no change
        } else if (j == LOGIC_LOW && k == LOGIC_HIGH) {
            // Reset
            ls->q = LOGIC_LOW;
            ls->q_bar = LOGIC_HIGH;
        } else if (j == LOGIC_HIGH && k == LOGIC_LOW) {
            // Set
            ls->q = LOGIC_HIGH;
            ls->q_bar = LOGIC_LOW;
        } else if (j == LOGIC_HIGH && k == LOGIC_HIGH) {
            // Toggle
            if (ls->q == LOGIC_HIGH) {
                ls->q = LOGIC_LOW;
                ls->q_bar = LOGIC_HIGH;
            } else {
                ls->q = LOGIC_HIGH;
                ls->q_bar = LOGIC_LOW;
            }
        }
    }

    ls->outputs[0] = ls->q;
    ls->outputs[1] = ls->q_bar;
}

void logic_propagate_t_flipflop(Component *comp, double time) {
    LogicGateState *ls = &comp->logic_state;

    // T flip-flop: On rising clock edge, if T=1, toggle Q
    LogicState t = ls->inputs[0];
    EdgeType clk_edge = logic_detect_edge(ls->inputs[1], ls->prev_inputs[1]);

    if (clk_edge == EDGE_RISING && t == LOGIC_HIGH) {
        // Toggle
        if (ls->q == LOGIC_HIGH) {
            ls->q = LOGIC_LOW;
            ls->q_bar = LOGIC_HIGH;
        } else {
            ls->q = LOGIC_HIGH;
            ls->q_bar = LOGIC_LOW;
        }
    }

    ls->outputs[0] = ls->q;
    ls->outputs[1] = ls->q_bar;
}

void logic_propagate_sr_latch(Component *comp) {
    LogicGateState *ls = &comp->logic_state;

    // SR latch: Level-sensitive
    // S=0, R=0: Hold
    // S=0, R=1: Reset (Q=0)
    // S=1, R=0: Set (Q=1)
    // S=1, R=1: Invalid (both outputs LOW or undefined)

    LogicState s = ls->inputs[0];
    LogicState r = ls->inputs[1];

    if (s == LOGIC_LOW && r == LOGIC_LOW) {
        // Hold - no change
    } else if (s == LOGIC_LOW && r == LOGIC_HIGH) {
        // Reset
        ls->q = LOGIC_LOW;
        ls->q_bar = LOGIC_HIGH;
    } else if (s == LOGIC_HIGH && r == LOGIC_LOW) {
        // Set
        ls->q = LOGIC_HIGH;
        ls->q_bar = LOGIC_LOW;
    } else if (s == LOGIC_HIGH && r == LOGIC_HIGH) {
        // Invalid state - both outputs LOW (as per many implementations)
        ls->q = LOGIC_LOW;
        ls->q_bar = LOGIC_LOW;
    }

    ls->outputs[0] = ls->q;
    ls->outputs[1] = ls->q_bar;
}

// ============================================================================
// BCD to 7-Segment Decoder
// ============================================================================

uint8_t logic_bcd_to_7seg(int bcd_value) {
    if (bcd_value < 0 || bcd_value > 15) {
        return 0x00;  // Blank for invalid
    }
    return BCD_TO_7SEG[bcd_value];
}

uint8_t logic_decode_7seg(LogicState d, LogicState c, LogicState b, LogicState a) {
    // Convert logic states to BCD value
    int bcd = 0;

    // Check for valid inputs
    if (d != LOGIC_HIGH && d != LOGIC_LOW) return 0x00;
    if (c != LOGIC_HIGH && c != LOGIC_LOW) return 0x00;
    if (b != LOGIC_HIGH && b != LOGIC_LOW) return 0x00;
    if (a != LOGIC_HIGH && a != LOGIC_LOW) return 0x00;

    if (d == LOGIC_HIGH) bcd |= 8;
    if (c == LOGIC_HIGH) bcd |= 4;
    if (b == LOGIC_HIGH) bcd |= 2;
    if (a == LOGIC_HIGH) bcd |= 1;

    return logic_bcd_to_7seg(bcd);
}

void logic_propagate_bcd_decoder(Component *comp) {
    LogicGateState *ls = &comp->logic_state;

    // Inputs: D(MSB), C, B, A(LSB) on inputs[0..3]
    uint8_t segments = logic_decode_7seg(
        ls->inputs[0],  // D
        ls->inputs[1],  // C
        ls->inputs[2],  // B
        ls->inputs[3]   // A
    );

    // Outputs: segments a-g on outputs[0..6]
    for (int i = 0; i < 7; i++) {
        ls->outputs[i] = (segments & (1 << i)) ? LOGIC_HIGH : LOGIC_LOW;
    }
}

void logic_propagate_decoder(Component *comp) {
    LogicGateState *ls = &comp->logic_state;

    // Simple 2-to-4 decoder
    // Inputs: A on inputs[0], B on inputs[1]
    // Outputs: Y0-Y3 on outputs[0..3]

    // All outputs LOW by default
    for (int i = 0; i < 4; i++) {
        ls->outputs[i] = LOGIC_LOW;
    }

    LogicState a = ls->inputs[0];
    LogicState b = ls->inputs[1];

    if (a == LOGIC_LOW && b == LOGIC_LOW) {
        ls->outputs[0] = LOGIC_HIGH;  // Y0
    } else if (a == LOGIC_HIGH && b == LOGIC_LOW) {
        ls->outputs[1] = LOGIC_HIGH;  // Y1
    } else if (a == LOGIC_LOW && b == LOGIC_HIGH) {
        ls->outputs[2] = LOGIC_HIGH;  // Y2
    } else if (a == LOGIC_HIGH && b == LOGIC_HIGH) {
        ls->outputs[3] = LOGIC_HIGH;  // Y3
    }
}

// ============================================================================
// Multiplexer/Demultiplexer
// ============================================================================

void logic_propagate_mux(Component *comp) {
    LogicGateState *ls = &comp->logic_state;

    // 2-to-1 MUX
    // inputs[0] = A (selected when SEL=0)
    // inputs[1] = B (selected when SEL=1)
    // inputs[2] = SEL

    LogicState sel = ls->inputs[2];

    if (sel == LOGIC_LOW) {
        ls->outputs[0] = ls->inputs[0];  // Output A
    } else if (sel == LOGIC_HIGH) {
        ls->outputs[0] = ls->inputs[1];  // Output B
    } else {
        ls->outputs[0] = LOGIC_X;
    }
}

void logic_propagate_demux(Component *comp) {
    LogicGateState *ls = &comp->logic_state;

    // 1-to-2 DEMUX
    // inputs[0] = Data
    // inputs[1] = SEL
    // outputs[0] = Y0 (active when SEL=0)
    // outputs[1] = Y1 (active when SEL=1)

    LogicState data = ls->inputs[0];
    LogicState sel = ls->inputs[1];

    if (sel == LOGIC_LOW) {
        ls->outputs[0] = data;
        ls->outputs[1] = LOGIC_LOW;
    } else if (sel == LOGIC_HIGH) {
        ls->outputs[0] = LOGIC_LOW;
        ls->outputs[1] = data;
    } else {
        ls->outputs[0] = LOGIC_X;
        ls->outputs[1] = LOGIC_X;
    }
}

// ============================================================================
// Arithmetic
// ============================================================================

void logic_propagate_half_adder(Component *comp) {
    LogicGateState *ls = &comp->logic_state;

    // Half adder: Sum = A XOR B, Carry = A AND B
    LogicState a = ls->inputs[0];
    LogicState b = ls->inputs[1];

    // Sum (XOR)
    if ((a == LOGIC_HIGH || a == LOGIC_LOW) && (b == LOGIC_HIGH || b == LOGIC_LOW)) {
        ls->outputs[0] = (a != b) ? LOGIC_HIGH : LOGIC_LOW;
        // Carry (AND)
        ls->outputs[1] = (a == LOGIC_HIGH && b == LOGIC_HIGH) ? LOGIC_HIGH : LOGIC_LOW;
    } else {
        ls->outputs[0] = LOGIC_X;
        ls->outputs[1] = LOGIC_X;
    }
}

void logic_propagate_full_adder(Component *comp) {
    LogicGateState *ls = &comp->logic_state;

    // Full adder: Sum = A XOR B XOR Cin, Cout = (A AND B) OR (Cin AND (A XOR B))
    LogicState a = ls->inputs[0];
    LogicState b = ls->inputs[1];
    LogicState cin = ls->inputs[2];

    if ((a == LOGIC_HIGH || a == LOGIC_LOW) &&
        (b == LOGIC_HIGH || b == LOGIC_LOW) &&
        (cin == LOGIC_HIGH || cin == LOGIC_LOW)) {

        int a_val = (a == LOGIC_HIGH) ? 1 : 0;
        int b_val = (b == LOGIC_HIGH) ? 1 : 0;
        int cin_val = (cin == LOGIC_HIGH) ? 1 : 0;

        int sum = a_val ^ b_val ^ cin_val;
        int cout = (a_val & b_val) | (cin_val & (a_val ^ b_val));

        ls->outputs[0] = sum ? LOGIC_HIGH : LOGIC_LOW;
        ls->outputs[1] = cout ? LOGIC_HIGH : LOGIC_LOW;
    } else {
        ls->outputs[0] = LOGIC_X;
        ls->outputs[1] = LOGIC_X;
    }
}

// ============================================================================
// Output Driving (DAC Phase)
// ============================================================================

void logic_drive_outputs(Simulation *sim, Circuit *circuit) {
    if (!sim || !circuit) return;

    for (int i = 0; i < circuit->num_components; i++) {
        Component *comp = circuit->components[i];
        if (!comp || !comp->logic_state.is_logic_component) continue;

        LogicGateState *ls = &comp->logic_state;

        // Drive output nodes based on component type
        // Store the logic state in the component's props for MNA stamping
        switch (comp->type) {
            case COMP_NOT_GATE:
            case COMP_BUFFER:
            case COMP_SCHMITT_INV:
            case COMP_SCHMITT_BUF:
                // Store the output state for rendering and MNA stamping
                comp->props.logic_gate.state = (ls->outputs[0] == LOGIC_HIGH);
                break;

            case COMP_AND_GATE:
            case COMP_OR_GATE:
            case COMP_NAND_GATE:
            case COMP_NOR_GATE:
            case COMP_XOR_GATE:
            case COMP_XNOR_GATE:
                // Store the output state
                comp->props.logic_gate.state = (ls->outputs[0] == LOGIC_HIGH);
                break;

            case COMP_D_FLIPFLOP:
            case COMP_JK_FLIPFLOP:
            case COMP_T_FLIPFLOP:
            case COMP_SR_LATCH:
                // Q on output node - store state for rendering and MNA stamping
                comp->props.logic_gate.state = (ls->q == LOGIC_HIGH);
                break;

            case COMP_BCD_DECODER:
                // 7 segment outputs - these will be used by rendering
                // The outputs are stored in ls->outputs[0..6] for segments a-g
                break;

            default:
                break;
        }
    }
}

// ============================================================================
// Debug Utilities
// ============================================================================

const char* logic_state_str(LogicState state) {
    switch (state) {
        case LOGIC_LOW:  return "LOW";
        case LOGIC_HIGH: return "HIGH";
        case LOGIC_Z:    return "Z";
        case LOGIC_X:    return "X";
        default:         return "?";
    }
}

void logic_debug_component(const Component *comp) {
    if (!comp || !comp->logic_state.is_logic_component) return;

    const LogicGateState *ls = &comp->logic_state;

    printf("Logic Component [%d] Type=%d\n", comp->id, comp->type);
    printf("  Inputs: ");
    for (int i = 0; i < MAX_LOGIC_INPUTS && i < 4; i++) {
        printf("%s ", logic_state_str(ls->inputs[i]));
    }
    printf("\n");
    printf("  Outputs: ");
    for (int i = 0; i < MAX_LOGIC_OUTPUTS && i < 4; i++) {
        printf("%s ", logic_state_str(ls->outputs[i]));
    }
    printf("\n");
    if (logic_is_sequential(comp->type)) {
        printf("  Q=%s Q_bar=%s\n", logic_state_str(ls->q), logic_state_str(ls->q_bar));
    }
}
