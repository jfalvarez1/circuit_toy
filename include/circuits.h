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
    CIRCUIT_RC_BANDPASS,
    CIRCUIT_LC_LOWPASS,
    CIRCUIT_ZENER_CLIPPER,
    CIRCUIT_VOLTAGE_DOUBLER,
    CIRCUIT_RELAXATION_OSC,
    CIRCUIT_HALFWAVE_FILTERED,
    CIRCUIT_HV_345_LINE,
    CIRCUIT_HV_138_LINE_VAR,
    CIRCUIT_MV_FEEDER,
    CIRCUIT_POLE_XFMR,
    CIRCUIT_GEN_GSU,
    CIRCUIT_GRID_CHAIN,
    CIRCUIT_FERRANTI_LINE,
    CIRCUIT_TESLA_COIL,
    CIRCUIT_TESLA_COIL_BIG,
    CIRCUIT_TESLA_COIL_DETUNED,
    CIRCUIT_LINE_MODEL_LADDER,
    CIRCUIT_DC_LINE_DROP,
    CIRCUIT_PC_OVERCURRENT,
    CIRCUIT_PC_DIFFERENTIAL,
    CIRCUIT_PC_DISTANCE,
    CIRCUIT_PC_BREAKER_FAIL,
    CIRCUIT_SIL_LOADING,
    CIRCUIT_SERIES_COMP,
    CIRCUIT_HV_765_LINE,
    CIRCUIT_3PH_Y_BALANCED,
    CIRCUIT_3PH_UNBALANCED,
    CIRCUIT_3PH_345_LINE,
    CIRCUIT_3PH_RECTIFIER,
    CIRCUIT_SCHMITT_BISTABLE,
    CIRCUIT_TRI_SQUARE_GEN,
    CIRCUIT_FUNCTION_GEN,
    CIRCUIT_COLPITTS,
    CIRCUIT_RING_OSC,
    CIRCUIT_HARTLEY,
    CIRCUIT_CLAPP,
    CIRCUIT_THEVENIN,
    CIRCUIT_SUPERPOSITION,
    CIRCUIT_RC_STEP,
    CIRCUIT_RL_STEP,
    CIRCUIT_RLC_RING,
    CIRCUIT_RLC_DAMPING,
    CIRCUIT_OPAMP_SAT,
    CIRCUIT_SINGLE_TUNED_AMP,
    CIRCUIT_COMMON_BASE,
    CIRCUIT_DARLINGTON,
    CIRCUIT_SR_LATCH,
    CIRCUIT_POWER_PLANT,
    CIRCUIT_SUBSTATION,
    CIRCUIT_IO_PUSH_PULL,
    CIRCUIT_IO_OPEN_DRAIN,
    CIRCUIT_IO_OPEN_COLLECTOR,
    CIRCUIT_IO_I2C_BUS,
    CIRCUIT_IO_I2C_LEVEL,
    CIRCUIT_IO_INPUT_DEBOUNCE,
    CIRCUIT_IO_LOW_SIDE,
    CIRCUIT_IO_HIGH_SIDE,
    CIRCUIT_IO_SPI,
    CIRCUIT_IO_UART,
    CIRCUIT_IO_RS485,
    CIRCUIT_IO_SPMI,
    CIRCUIT_TX_69KV,
    CIRCUIT_TX_LADDER,
    CIRCUIT_TX_WIND,
    CIRCUIT_TX_PLANT,
    CIRCUIT_RES_SERVICE,
    CIRCUIT_RES_BRANCH,
    CIRCUIT_RES_ACSTART,
    CIRCUIT_RES_SOLAR,
    CIRCUIT_COM_480Y,
    CIRCUIT_COM_208Y,
    CIRCUIT_COM_PFC,
    CIRCUIT_COM_ATS,
    CIRCUIT_GS_N1,
    CIRCUIT_GS_IBR,
    CIRCUIT_GS_BOLD,
    CIRCUIT_GS_DERATE,
    CIRCUIT_GS_FACRATE,
    CIRCUIT_GS_KRON,
    CIRCUIT_GS_RX,
    CIRCUIT_GS_GOVERNOR,
    CIRCUIT_GS_PIDS,
    CIRCUIT_MOS_IDVGS,
    CIRCUIT_MOS_IDVDS,
    CIRCUIT_MOS_TUNED,
    CIRCUIT_MOS_CG,
    CIRCUIT_MOS_CASCODE,
    CIRCUIT_MOS_DIFF,
    CIRCUIT_MOS_MIRROR,
    CIRCUIT_CMOS_INV,
    CIRCUIT_CMOS_NAND,
    CIRCUIT_CMOS_TGATE,
    CIRCUIT_XY_LISSAJOUS,
    CIRCUIT_XY_PLOTTER,
    CIRCUIT_HW_BUCK,
    CIRCUIT_HW_BOOST,
    CIRCUIT_HW_BUCKBOOST,
    CIRCUIT_HW_CUK,
    CIRCUIT_HW_INTERLEAVED,
    CIRCUIT_HW_PDN,
    CIRCUIT_HW_CAPS,
    CIRCUIT_HW_MATCH,
    CIRCUIT_HW_REFLECT,
    CIRCUIT_HW_LOOP,
    CIRCUIT_HW_CCM_DCM,
    /* Ideal vs real: the same circuit twice, one part swapped for its non-ideal model */
    CIRCUIT_ID_SOURCE,
    CIRCUIT_ID_DIODE,
    CIRCUIT_ID_CAP,
    CIRCUIT_ID_IND,
    CIRCUIT_ID_OPAMP,
    CIRCUIT_ID_BJT,
    CIRCUIT_ID_MOSFET,
    CIRCUIT_ID_OPAMP_ERR,
    CIRCUIT_PARTS_MOSFET,
    CIRCUIT_CAP_DCBIAS,
    CIRCUIT_NE555_ASTABLE,
    CIRCUIT_PIERCE,
    /* Interview prep: the questions a hardware interview actually asks. Nothing here
       repeats a circuit that already exists elsewhere in the list - where the ground is
       already covered, the notes name the template that covers it. */
    CIRCUIT_IV_PROBE_COMP,
    CIRCUIT_IV_PROBE_LOADING,
    CIRCUIT_IV_GROUND_LEAD,
    CIRCUIT_IV_SCOPE_INPUT_Z,
    CIRCUIT_IV_AC_COUPLING,
    CIRCUIT_IV_SHUNT_SENSE,
    CIRCUIT_IV_KELVIN,
    CIRCUIT_IV_BUCK_NODES,
    CIRCUIT_IV_LDO_VS_BUCK,
    CIRCUIT_IV_BOOTSTRAP,
    CIRCUIT_IV_TERMINATION,
    CIRCUIT_IV_PULLUP_SIZING,
    CIRCUIT_IV_GROUND_BOUNCE,
    CIRCUIT_IV_CROSSTALK,
    CIRCUIT_IV_ESD_CLAMP,
    CIRCUIT_IV_CAP_ENERGY,
    CIRCUIT_IV_MILLER,
    CIRCUIT_IV_SWITCH_CHOICE,
    CIRCUIT_IV_INRUSH,
    CIRCUIT_TLINE_REAL,
    CIRCUIT_SEVENSEG_TEST,
    CIRCUIT_WIRELESS_LINK,
    CIRCUIT_BCD_COUNTER,
    CIRCUIT_DIGITAL_CLOCK,
    /* Battery monitoring and electronic load - the circuits of a senior-design BMI:
       a constant-current sink, a two-stage LiPo charger, an NTC cutout and a cell simulator */
    CIRCUIT_BMI_ELOAD_CC,
    CIRCUIT_BMI_ELOAD_CR,
    CIRCUIT_BMI_ELOAD_CV,
    CIRCUIT_BMI_THERMAL_CUTOUT,
    CIRCUIT_BMI_SUPERCAP,
    CIRCUIT_TYPE_COUNT
} CircuitTemplateType;

// Palette grouping of the templates (the Circuits palette is generated from this)
typedef enum {
    TG_BASICS = 0,      // dividers, RLC, Ohm's law
    TG_FILTERS,         // passive and active filters
    TG_OPAMPS,          // op-amp building blocks
    TG_TRANSISTORS,     // BJT / MOSFET stages
    TG_OSCILLATORS,
    TG_POWER_SUPPLY,    // rectifiers, regulators, clampers, references
    TG_DIGITAL,         // logic-level circuits
    TG_POWER_SYSTEMS,   // transmission / distribution examples
    TG_HIGH_VOLTAGE,    // Tesla coils
    TG_TRANSIENTS,      // step responses (Agarwal & Lang ch. 10/12)
    TG_IC_IO,           // IC output / input structures, buses and drivers (GPIO, I2C, SPI, UART, RS-485, SPMI)
    TG_BUILDING,        // Residential and commercial services (ANSI C84.1, NEC, IEEE 1547)
    TG_GRID_STD,        // Reliability standards and simulation methods (NERC TPL/PRC/FAC, ERCOT BAL/NOGRR, CIP-014)
    TG_HARDWARE,        // Hardware engineering: switching converters, PDN, signal integrity, loop stability
    TG_IDEAL,           // Ideal vs real: the same circuit with a textbook part and with its real model
    TG_IV_MEAS,         // Interview prep: instrumentation, probing and scope setup
    TG_IV_FUND,         // Interview prep: analog fundamentals
    TG_IV_POWER,        // Interview prep: converters and power delivery
    TG_IV_SI,           // Interview prep: I/O, termination and signal integrity
    TG_BMI,             // Battery monitoring and electronic load: sink, charger, cutout, simulator
    TG_COUNT
} TemplateGroup;

// Circuit template info
typedef struct {
    const char *name;
    const char *short_name;
    const char *description;
    TemplateGroup group;
} CircuitTemplateInfo;

const char *circuit_template_group_name(TemplateGroup g);

// Get info for circuit template type
const CircuitTemplateInfo *circuit_template_get_info(CircuitTemplateType type);

// Place a circuit template at the given position
// Returns number of components added, or 0 on failure
int circuit_place_template(Circuit *circuit, CircuitTemplateType type, float x, float y);

// Suggested scope time/div for a template (0 = no preference)
double circuit_template_scope_time_div(CircuitTemplateType type);
double circuit_template_scope_volt_div(CircuitTemplateType type);
// Scope view preset: SCOPE_FLAG_STACK | SCOPE_FLAG_FIT for amplifiers whose in/out ride on different DC levels
#define SCOPE_FLAG_AC    1
#define SCOPE_FLAG_STACK 2
#define SCOPE_FLAG_FIT   4
/* Arm a single-shot trigger. For a template whose whole event happens once: the switch closes, the
   charge moves, and from then on the scope in AUTO mode free-runs over a settled circuit and draws
   flat lines. A person loading The Two-Capacitor Problem sees the 10 ms transfer flash past in the
   first moments and then two straight traces for as long as they look at it. SNGL catches the
   event and holds it, which is exactly what the mode is for and what a person would reach for on a
   bench. */
#define SCOPE_FLAG_SINGLE 8
int circuit_template_scope_flags(CircuitTemplateType type);

// HARD RULE: every template declares how it demonstrates itself, and template_smoke
// --demo-test checks that the stimulus actually shows it (see tools/template_smoke.c).
typedef enum {
    DEMO_NONE = 0,     // not allowed for a shipped template
    DEMO_LOWPASS,      // frequency sweep must bracket f_char; output amplitude falls above it
    DEMO_HIGHPASS,     // ... output amplitude rises above it
    DEMO_BANDPASS,     // ... output peaks around it (also resonance peaks)
    DEMO_NOTCH,        // ... output dips around it
    DEMO_ENVELOPE,     // amplitude sweep: output level follows the input amplitude
    DEMO_LIMITER,      // amplitude sweep: output stops growing once the input exceeds a level
    DEMO_WAVEFORM,     // periodic stimulus: output is a clearly varying waveform
    DEMO_SWITCH,       // comparator-like: output swings rail to rail
    DEMO_DC,           // steady DC output (value checked by --probe-test)
    DEMO_OSC           // self-oscillates (checked by --osc-test)
} DemoKind;
typedef struct { DemoKind kind; double f_char; } TemplateDemo;
const TemplateDemo *circuit_template_demo(CircuitTemplateType type);
// Designated output node of a template (component type / ordinal / terminal); false if none
bool circuit_template_output_spec(CircuitTemplateType type, ComponentType *ct, int *ord, int *term);

#endif // CIRCUITS_H
