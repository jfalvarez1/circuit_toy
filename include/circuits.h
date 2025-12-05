/**
 * Circuit Playground - Predefined Circuit Templates
 */

#ifndef CIRCUITS_H
#define CIRCUITS_H

#include "types.h"
#include "circuit.h"

// Circuit template types
typedef enum {
    CIRCUIT_NONE = 0,
    CIRCUIT_RC_LOWPASS,
    CIRCUIT_RC_HIGHPASS,
    CIRCUIT_RL_LOWPASS,
    CIRCUIT_RL_HIGHPASS,
    CIRCUIT_VOLTAGE_DIVIDER,
    CIRCUIT_INVERTING_AMP,
    CIRCUIT_NONINVERTING_AMP,
    CIRCUIT_VOLTAGE_FOLLOWER,
    CIRCUIT_HALFWAVE_RECT,
    CIRCUIT_LED_WITH_RESISTOR,
    // Transistor amplifiers
    CIRCUIT_COMMON_EMITTER,
    CIRCUIT_COMMON_SOURCE,
    CIRCUIT_COMMON_DRAIN,
    CIRCUIT_MULTISTAGE_AMP,
    // Additional transistor circuits
    CIRCUIT_DIFFERENTIAL_PAIR,
    CIRCUIT_CURRENT_MIRROR,
    CIRCUIT_PUSH_PULL,
    CIRCUIT_CMOS_INVERTER,
    // Additional op-amp circuits
    CIRCUIT_INTEGRATOR,
    CIRCUIT_DIFFERENTIATOR,
    CIRCUIT_SUMMING_AMP,
    CIRCUIT_COMPARATOR,
    // Power supply / rectifier circuits
    CIRCUIT_FULLWAVE_BRIDGE,    // Full-wave bridge rectifier
    CIRCUIT_CENTERTAP_RECT,     // Center-tap rectifier with transformer
    CIRCUIT_AC_DC_SUPPLY,       // AC to DC power supply with transformer
    CIRCUIT_AC_DC_AMERICAN,     // American 120V/60Hz to 12V DC
    // TI Analog Circuits - Amplifiers
    CIRCUIT_DIFFERENCE_AMP,     // Difference amplifier (subtractor)
    CIRCUIT_TRANSIMPEDANCE,     // Transimpedance amplifier (I to V)
    CIRCUIT_INSTR_AMP,          // Instrumentation amplifier (3 op-amp)
    // TI Analog Circuits - Filters
    CIRCUIT_SALLEN_KEY_LP,      // Sallen-Key low pass (2nd order)
    CIRCUIT_BANDPASS_ACTIVE,    // Active band pass filter
    CIRCUIT_NOTCH_FILTER,       // Twin-T notch filter
    // TI Analog Circuits - Signal Sources
    CIRCUIT_WIEN_OSCILLATOR,    // Wien bridge sine oscillator
    CIRCUIT_CURRENT_SOURCE,     // Constant current source (BJT)
    // TI Analog Circuits - Comparators/Detection
    CIRCUIT_WINDOW_COMP,        // Window comparator
    CIRCUIT_HYSTERESIS_COMP,    // Schmitt trigger (comparator with hysteresis)
    // TI Analog Circuits - Power/Voltage
    CIRCUIT_ZENER_REF,          // Zener voltage reference
    CIRCUIT_PRECISION_RECT,     // Precision full-wave rectifier
    // Voltage Regulator Circuits
    CIRCUIT_7805_REG,           // 7805 fixed 5V regulator circuit
    CIRCUIT_LM317_REG,          // LM317 adjustable regulator circuit
    CIRCUIT_TL431_REF,          // TL431 precision shunt reference circuit
    // RLC Resonant Circuits
    CIRCUIT_SERIES_RLC,         // Series RLC resonant circuit
    CIRCUIT_PARALLEL_RLC,       // Parallel RLC (tank) circuit
    // Measurement & Detection Circuits
    CIRCUIT_WHEATSTONE,         // Wheatstone bridge
    CIRCUIT_PEAK_DETECTOR,      // Peak detector with op-amp
    // Signal Processing Circuits
    CIRCUIT_CLAMPER,            // Positive clamper (DC restorer)
    CIRCUIT_PHASE_SHIFT_OSC,    // RC phase shift oscillator
    CIRCUIT_TYPE_COUNT
} CircuitTemplateType;

// Circuit template info
typedef struct {
    const char *name;
    const char *short_name;
    const char *description;
} CircuitTemplateInfo;

// Get info for circuit template type
const CircuitTemplateInfo *circuit_template_get_info(CircuitTemplateType type);

// Place a circuit template at the given position
// Returns number of components added, or 0 on failure
int circuit_place_template(Circuit *circuit, CircuitTemplateType type, float x, float y);

#endif // CIRCUITS_H
