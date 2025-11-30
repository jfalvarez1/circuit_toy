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
