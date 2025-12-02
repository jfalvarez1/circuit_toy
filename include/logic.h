/**
 * Circuit Playground - Mixed-Signal Logic Solver
 *
 * This module provides efficient simulation of digital logic circuits by:
 * 1. Abstracting logic gates from the full MNA matrix solve
 * 2. Propagating boolean states (HIGH, LOW, FLOAT) based on input thresholds
 * 3. Automatically inserting ADC/DAC bridges at analog/digital boundaries
 *
 * The mixed-signal architecture significantly improves performance for circuits
 * containing many logic gates, flip-flops, and other digital ICs.
 */

#ifndef LOGIC_H
#define LOGIC_H

#include "types.h"

// Forward declarations with typedef for opaque pointer usage in header
// These will be fully defined when the respective headers are included
typedef struct Component Component;
typedef struct Circuit Circuit;
typedef struct Simulation Simulation;

// Note: LogicGateState is defined in types.h to avoid circular dependencies

// ============================================================================
// Logic Solver Interface
// ============================================================================

/**
 * Initialize logic state for a component.
 * Called when a logic component is created.
 */
void logic_init_component(Component *comp);

/**
 * Get default logic levels for a logic family.
 */
LogicLevels logic_get_family_levels(LogicFamily family);

/**
 * Convert analog voltage to logic state using the given thresholds.
 * Implements ADC bridge functionality.
 *
 * @param voltage Input voltage from analog node
 * @param levels Logic level thresholds
 * @param prev_state Previous state (for hysteresis in Schmitt triggers)
 * @return Logic state (HIGH, LOW, or X if in undefined region)
 */
LogicState logic_voltage_to_state(double voltage, const LogicLevels *levels,
                                   LogicState prev_state);

/**
 * Convert logic state to output voltage.
 * Implements DAC bridge functionality.
 *
 * @param state Logic state to convert
 * @param levels Logic level configuration
 * @return Output voltage (v_oh for HIGH, v_ol for LOW, undefined for Z/X)
 */
double logic_state_to_voltage(LogicState state, const LogicLevels *levels);

/**
 * Sample input voltages from analog nodes and convert to logic states.
 * This is the ADC phase of mixed-signal simulation.
 *
 * @param sim Simulation context (contains node voltages)
 * @param circuit Circuit containing components
 */
void logic_sample_inputs(Simulation *sim, Circuit *circuit);

/**
 * Propagate logic states through all logic gates.
 * This bypasses the MNA matrix and uses fast boolean logic.
 *
 * @param circuit Circuit containing logic components
 * @param time Current simulation time
 * @param dt Time step
 * @return Number of gates that changed output
 */
int logic_propagate(Circuit *circuit, double time, double dt);

/**
 * Propagate a single logic component.
 * Called by logic_propagate for each logic gate.
 *
 * @param comp Logic component to propagate
 * @param time Current simulation time
 * @return true if output changed
 */
bool logic_propagate_component(Component *comp, double time);

/**
 * Drive logic outputs to analog nodes.
 * This is the DAC phase of mixed-signal simulation.
 * Logic outputs are stamped as voltage sources with source impedance.
 *
 * @param sim Simulation context
 * @param circuit Circuit containing components
 */
void logic_drive_outputs(Simulation *sim, Circuit *circuit);

/**
 * Check if a component type is a logic gate that uses the logic abstraction.
 */
bool logic_is_logic_component(ComponentType type);

/**
 * Check if a component type is a sequential logic element (has state).
 */
bool logic_is_sequential(ComponentType type);

/**
 * Detect edge on a logic input (for flip-flops).
 */
EdgeType logic_detect_edge(LogicState current, LogicState previous);

// ============================================================================
// Specific Logic Gate Propagation Functions
// ============================================================================

// Combinational gates
void logic_propagate_not(Component *comp);
void logic_propagate_and(Component *comp);
void logic_propagate_or(Component *comp);
void logic_propagate_nand(Component *comp);
void logic_propagate_nor(Component *comp);
void logic_propagate_xor(Component *comp);
void logic_propagate_xnor(Component *comp);
void logic_propagate_buffer(Component *comp);

// Schmitt triggers (with hysteresis)
void logic_propagate_schmitt_inv(Component *comp);
void logic_propagate_schmitt_buf(Component *comp);

// Sequential logic
void logic_propagate_d_flipflop(Component *comp, double time);
void logic_propagate_jk_flipflop(Component *comp, double time);
void logic_propagate_t_flipflop(Component *comp, double time);
void logic_propagate_sr_latch(Component *comp);

// Decoders and multiplexers
void logic_propagate_bcd_decoder(Component *comp);
void logic_propagate_decoder(Component *comp);
void logic_propagate_mux(Component *comp);
void logic_propagate_demux(Component *comp);

// Arithmetic
void logic_propagate_half_adder(Component *comp);
void logic_propagate_full_adder(Component *comp);

// ============================================================================
// 7-Segment Decoder Truth Table
// ============================================================================

/**
 * BCD to 7-segment decoder lookup table.
 * Returns segment bitmask (a=bit0, b=bit1, ..., g=bit6).
 * Returns 0x00 for invalid BCD inputs (10-15).
 */
uint8_t logic_bcd_to_7seg(int bcd_value);

/**
 * Get segment pattern for a 7-segment display based on 4 BCD inputs.
 */
uint8_t logic_decode_7seg(LogicState d, LogicState c, LogicState b, LogicState a);

// ============================================================================
// Debug Utilities
// ============================================================================

/**
 * Get string representation of logic state.
 */
const char* logic_state_str(LogicState state);

/**
 * Print logic component state for debugging.
 */
void logic_debug_component(const Component *comp);

#endif // LOGIC_H
