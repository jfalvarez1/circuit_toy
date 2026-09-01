/**
 * Circuit Playground - Component Implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>   /* toupper, in component_apply_part */
#include "component.h"
#include "logic.h"   /* logic_init_component, for the counter */

// Global environment state (affects LDR and thermistor components)
Vector *g_stamp_prev_step = NULL;
/* Set while a component is being stamped only to read a current back out of it, not to advance
   the simulation. A stamp that remembers something - a MOSFET's gate-capacitor charge, say -
   must not remember anything from one of these: the display recovers terminal currents by
   re-stamping every component once a frame, and that would overwrite the device's memory of the
   step that actually happened with values worked out from the linearisation. */
bool g_stamp_read_only = false;

EnvironmentState g_environment = {
    .light_level = 0.5,     // Default: medium light (0.0=dark to 1.0=bright)
    .temperature = 25.0     // Default: room temperature (°C)
};

// Global sub-circuit library
SubCircuitLibrary g_subcircuit_library = {
    .count = 0,
    .next_id = 1
};

// Helper: Calculate LED saturation current (Is) to achieve target Vf at 20mA
// Uses: Vf = n*Vt * ln(I/Is + 1), solving for Is
static double led_calc_is_for_vf(double target_vf, double n, double vt) {
    double typical_current = 0.020;  // 20 mA
    double exp_term = exp(target_vf / (n * vt));
    double is = typical_current / (exp_term - 1.0);
    return (is > 1e-30) ? is : 1e-30;  // Clamp to avoid numerical issues
}

// Get LED parameters based on color
static void led_get_color_params(LEDColor color, double *vf, double *max_current, double *wavelength) {
    switch (color) {
        case LED_COLOR_INFRARED:
            *vf = 1.4; *max_current = 0.050; *wavelength = 850; break;
        case LED_COLOR_RED:
            *vf = 2.0; *max_current = 0.030; *wavelength = 630; break;
        case LED_COLOR_ORANGE:
            *vf = 2.1; *max_current = 0.030; *wavelength = 610; break;
        case LED_COLOR_YELLOW:
            *vf = 2.1; *max_current = 0.030; *wavelength = 590; break;
        case LED_COLOR_GREEN_STANDARD:
            *vf = 2.1; *max_current = 0.030; *wavelength = 565; break;
        case LED_COLOR_GREEN_PURE:
            *vf = 3.2; *max_current = 0.030; *wavelength = 525; break;
        case LED_COLOR_BLUE:
            *vf = 3.4; *max_current = 0.030; *wavelength = 470; break;
        case LED_COLOR_WHITE:
            *vf = 3.3; *max_current = 0.030; *wavelength = 0; break;  // No single wavelength
        case LED_COLOR_UV:
            *vf = 3.5; *max_current = 0.030; *wavelength = 395; break;
        default:  // Default to RED
            *vf = 2.0; *max_current = 0.030; *wavelength = 630; break;
    }
}

// Component type information table
// NOTE: Terminal positions must be multiples of GRID_SIZE (10) for proper grid alignment
// Array is sized to COMP_TYPE_COUNT to ensure all component types have entries
static const ComponentTypeInfo component_info[COMP_TYPE_COUNT] = {
    [COMP_NONE] = { "None", "?", 0, {}, 0, 0, {} },

    [COMP_GROUND] = {
        "Ground", "GND", 1,
        {{ 0, -20, "GND" }},
        40, 40,
        {}
    },

    [COMP_DC_VOLTAGE] = {
        "DC Voltage", "V", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .dc_voltage = {
            .voltage = 5.0,
            .r_series = 0.001,
            .ideal = true
        }}
    },

    [COMP_ARB_SOURCE] = {
        "Arb Source", "ARB", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .arb_source = {
            .table = 0,
            .period = 0.01,
            .amplitude = 1.0,
            .offset = 0.0
        }}
    },

    [COMP_AC_VOLTAGE] = {
        "AC Voltage", "~V", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .ac_voltage = {
            .amplitude = 5.0,
            .frequency = 60.0,
            .phase = 0.0,
            .offset = 0.0,
            .r_series = 0.001,
            .ideal = true
        }}
    },

    [COMP_DC_CURRENT] = {
        "DC Current", "I", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .dc_current = {
            .current = 0.001,
            .r_parallel = 1e9,
            .ideal = true
        }}
    },

    [COMP_RESISTOR] = {
        "Resistor", "R", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 20,
        { .resistor = {
            .resistance = 1000.0,
            .tolerance = 5.0,
            .power_rating = 0.25,
            .power_dissipated = 0.0,
            .temp_coeff = 100.0,    // 100 ppm/°C (typical for carbon film)
            .temp = 25.0,           // Room temperature
            .ideal = true
        }}
    },

    [COMP_LOAD_HP] = {
        "HP Load", "R", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 20,
        { .resistor = {
            .resistance = 1000.0,
            .tolerance = 5.0,
            .power_rating = 1e12,
            .power_dissipated = 0.0,
            .temp_coeff = 100.0,    // 100 ppm/°C (typical for carbon film)
            .temp = 25.0,           // Room temperature
            .ideal = true,
            .high_power = true
        }}
    },

    [COMP_CAPACITOR] = {
        "Capacitor", "C", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 30,
        { .capacitor = {
            .capacitance = 1e-6,
            .voltage = 0.0,
            .esr = 0.01,            // 10 mOhm ESR
            .esl = 1e-9,            // 1 nH ESL
            .leakage = 1e9,         // 1 GOhm leakage
            .v_half = 0.0,          // no DC-bias capacitance loss unless a part model asks for it
            .ideal = true
        }}
    },

    [COMP_CAPACITOR_ELEC] = {
        "Electrolytic Cap", "Ce", 2,
        {{ -40, 0, "+" }, { 40, 0, "-" }},
        80, 30,
        { .capacitor_elec = {
            .capacitance = 100e-6,
            .voltage = 0.0,
            .max_voltage = 25.0,
            .esr = 0.1,             // Higher ESR for electrolytics
            .leakage = 1e6,         // Lower leakage resistance
            .ideal = true
        }}
    },

    [COMP_INDUCTOR] = {
        "Inductor", "L", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 20,
        { .inductor = {
            .inductance = 1e-3,
            .current = 0.0,
            .dcr = 0.1,             // 100 mOhm DC resistance
            .r_parallel = 1e6,      // Core loss resistance
            .i_sat = 1.0,           // 1A saturation current
            .ideal = true
        }}
    },

    [COMP_DIODE] = {
        "Diode", "D", 2,
        {{ -40, 0, "A" }, { 40, 0, "K" }},
        80, 20,
        { .diode = {
            .is = 1e-12,            // 1 pA saturation current
            .vt = 0.026,            // 26 mV thermal voltage
            .n = 1.0,               // Ideality factor
            .bv = 100.0,            // 100V reverse breakdown
            .ibv = 1e-10,           // Breakdown current
            .cjo = 1e-12,           // 1 pF junction capacitance
            .ideal = false          // Shockley by default; ideal = the 0.7 V switch model
        }}
    },

    [COMP_ZENER] = {
        "Zener Diode", "DZ", 2,
        {{ -40, 0, "A" }, { 40, 0, "K" }},
        80, 20,
        { .zener = {
            .is = 1e-12,
            .vt = 0.026,
            .n = 1.0,
            .vz = 5.1,              // 5.1V Zener voltage
            .rz = 5.0,              // 5 Ohm Zener impedance
            .iz_test = 5e-3,        // 5 mA test current
            .ideal = true
        }}
    },

    [COMP_SCHOTTKY] = {
        "Schottky Diode", "DS", 2,
        {{ -40, 0, "A" }, { 40, 0, "K" }},
        80, 20,
        { .schottky = {
            .is = 1e-8,             // Higher saturation current
            .vt = 0.026,
            .n = 1.05,              // Slightly higher ideality
            .vf = 0.3,              // 0.3V typical forward voltage
            .cjo = 5e-12,           // 5 pF junction capacitance
            .ideal = true
        }}
    },

    [COMP_LED] = {
        "LED", "LED", 2,
        {{ -40, 0, "A" }, { 40, 0, "K" }},
        80, 20,
        { .led = {
            .is = 5e-20,            // Calculated for Vf=2.0V @ 20mA (RED default)
            .vt = 0.026,
            .n = 2.0,               // Higher ideality for LED
            .vf = 2.0,              // 2.0V forward voltage (red)
            .max_current = 0.030,   // 30 mA max (standard 5mm LED)
            .wavelength = 630,      // Red (630 nm)
            .current = 0.0,
            .ideal = false,         // Use Shockley model
            .color = LED_COLOR_RED
        }}
    },

    [COMP_NPN_BJT] = {
        "NPN BJT", "Q", 3,
        {{ -20, 0, "B" }, { 20, -20, "C" }, { 20, 20, "E" }},
        60, 60,
        { .bjt = {
            .bf = 100.0,        // Forward current gain (beta)
            .is = 1e-14,        // Saturation current
            .vaf = 100.0,       // Forward Early voltage
            .nf = 1.0,          // Forward emission coefficient
            .br = 1.0,          // Reverse current gain
            .var = 100.0,       // Reverse Early voltage
            .nr = 1.0,          // Reverse emission coefficient
            .ise = 0.0,         // B-E leakage saturation current
            .isc = 0.0,         // B-C leakage saturation current
            .temp = 300.0,      // Temperature (K)
            .ideal = true       // Use ideal (simplified) model
        }}
    },

    [COMP_PNP_BJT] = {
        "PNP BJT", "Q", 3,
        {{ -20, 0, "B" }, { 20, -20, "C" }, { 20, 20, "E" }},
        60, 60,
        { .bjt = {
            .bf = 100.0,
            .is = 1e-14,
            .vaf = 100.0,
            .nf = 1.0,
            .br = 1.0,
            .var = 100.0,
            .nr = 1.0,
            .ise = 0.0,
            .isc = 0.0,
            .temp = 300.0,
            .ideal = true
        }}
    },

    [COMP_NMOS] = {
        "NMOS", "M", 3,
        {{ -20, 0, "G" }, { 20, -20, "D" }, { 20, 20, "S" }},
        60, 60,
        { .mosfet = {
            .vth = 0.7,         // Threshold voltage (V)
            .kp = 110e-6,       // Transconductance parameter (A/V²)
            .lambda = 0.04,     // Channel length modulation (1/V)
            .w = 10e-6,         // Channel width (m) - 10um
            .l = 1e-6,          // Channel length (m) - 1um
            .tox = 10e-9,       // Gate oxide thickness (m) - 10nm
            .gamma = 0.4,       // Body effect coefficient (V^0.5)
            .phi = 0.65,        // Surface potential (V)
            .nsub = 1e15,       // Substrate doping (1/cm³)
            .cgso = 1e-10,      // Gate-source overlap capacitance (F/m)
            .cgdo = 1e-10,      // Gate-drain overlap capacitance (F/m)
            .cgbo = 1e-10,      // Gate-body overlap capacitance (F/m)
            .cj = 1e-4,         // Junction capacitance (F/m²)
            .vgs_prev = 0.0,    // Previous Vgs
            .vgd_prev = 0.0,    // Previous Vgd
            .i_cgs = 0.0,       // Gate-source capacitor current
            .i_cgd = 0.0,       // Gate-drain capacitor current
            .temp = 300.0,      // Temperature (K)
            .ideal = true       // Use ideal (simplified) model
        }}
    },

    [COMP_PMOS] = {
        "PMOS", "M", 3,
        {{ -20, 0, "G" }, { 20, -20, "D" }, { 20, 20, "S" }},
        60, 60,
        { .mosfet = {
            .vth = -0.7,        // Threshold voltage (V) - negative for PMOS
            .kp = 50e-6,        // Transconductance parameter (A/V²) - lower for PMOS
            .lambda = 0.04,     // Channel length modulation (1/V)
            .w = 10e-6,         // Channel width (m)
            .l = 1e-6,          // Channel length (m)
            .tox = 10e-9,       // Gate oxide thickness (m)
            .gamma = 0.4,       // Body effect coefficient (V^0.5)
            .phi = 0.65,        // Surface potential (V)
            .nsub = 1e15,       // Substrate doping (1/cm³)
            .cgso = 1e-10,      // Gate-source overlap capacitance (F/m)
            .cgdo = 1e-10,      // Gate-drain overlap capacitance (F/m)
            .cgbo = 1e-10,      // Gate-body overlap capacitance (F/m)
            .cj = 1e-4,         // Junction capacitance (F/m²)
            .vgs_prev = 0.0,    // Previous Vgs
            .vgd_prev = 0.0,    // Previous Vgd
            .i_cgs = 0.0,       // Gate-source capacitor current
            .i_cgd = 0.0,       // Gate-drain capacitor current
            .temp = 300.0,      // Temperature (K)
            .ideal = true       // Use ideal (simplified) model
        }}
    },

    [COMP_OPAMP] = {
        "Op-Amp", "U", 3,
        {{ -40, -20, "-" }, { -40, 20, "+" }, { 40, 0, "OUT" }},
        80, 60,
        { .opamp = {
            .gain = 100000.0,       // 100 dB open-loop gain
            .voffset = 0.0,         // No input offset
            .vmax = 15.0,           // +15V rail
            .vmin = -15.0,          // -15V rail
            .gbw = 1e6,             // 1 MHz gain-bandwidth product
            .slew_rate = 0.5,       // 0.5 V/us slew rate
            .r_in = 1e12,           // 1 TOhm input impedance
            .r_out = 75.0,          // 75 Ohm output impedance
            .i_bias = 1e-12,        // 1 pA input bias current
            .cmrr = 90.0,           // 90 dB CMRR
            .rail_to_rail = false,  // Not rail-to-rail
            .ideal = true           // Ideal mode by default
        }}
    },

    [COMP_OPAMP_FLIPPED] = {
        "OpAmp(flipped)", "U", 3,
        {{ -40, -20, "+" }, { -40, 20, "-" }, { 40, 0, "OUT" }},  // + on top, - on bottom
        80, 60,
        { .opamp = {
            .gain = 100000.0,       // 100 dB open-loop gain
            .voffset = 0.0,         // No input offset
            .vmax = 15.0,           // +15V rail
            .vmin = -15.0,          // -15V rail
            .gbw = 1e6,             // 1 MHz gain-bandwidth product
            .slew_rate = 0.5,       // 0.5 V/us slew rate
            .r_in = 1e12,           // 1 TOhm input impedance
            .r_out = 75.0,          // 75 Ohm output impedance
            .i_bias = 1e-12,        // 1 pA input bias current
            .cmrr = 90.0,           // 90 dB CMRR
            .rail_to_rail = false,  // Not rail-to-rail
            .ideal = true           // Ideal mode by default
        }}
    },

    // Waveform generators
    [COMP_SQUARE_WAVE] = {
        "Square Wave", "SQ", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .square_wave = {
            .amplitude = 5.0,
            .frequency = 1000.0,
            .phase = 0.0,
            .offset = 0.0,
            .duty = 0.5,
            .rise_time = 1e-9,
            .fall_time = 1e-9,
            .r_series = 0.001,
            .ideal = true
        }}
    },

    [COMP_TRIANGLE_WAVE] = {
        "Triangle Wave", "TRI", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .triangle_wave = {
            .amplitude = 5.0,
            .frequency = 1000.0,
            .phase = 0.0,
            .offset = 0.0,
            .r_series = 0.001,
            .ideal = true
        }}
    },

    [COMP_SAWTOOTH_WAVE] = {
        "Sawtooth Wave", "SAW", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .sawtooth_wave = {
            .amplitude = 5.0,
            .frequency = 1000.0,
            .phase = 0.0,
            .offset = 0.0,
            .r_series = 0.001,
            .ideal = true
        }}
    },

    [COMP_NOISE_SOURCE] = {
        "Noise Source", "N", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .noise_source = {
            .amplitude = 1.0,
            .seed = 12345,
            .bandwidth = 1e6,
            .r_series = 0.001,
            .ideal = true
        }}
    },

    [COMP_TEXT] = {
        "Text", "T", 0,  // No terminals
        {},
        80, 20,  // Default width/height for hit detection
        { .text = {
            .text = "Label",
            .font_size = 2,  // Normal size
            .color = 0xFFFFFFFF  // White
        }}
    },

    [COMP_SPST_SWITCH] = {
        "SPST Switch", "SW", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 30,
        { .switch_spst = {
            .closed = false,        // Default open
            .r_on = 0.01,           // 10 mOhm on-resistance
            .r_off = 1e15,          // 1 POhm off-resistance (must be >> 1/GMIN for proper isolation)
            .momentary = false,
            .default_closed = false
        }}
    },

    [COMP_SPDT_SWITCH] = {
        "SPDT Switch", "SW", 3,
        {{ -40, 0, "C" }, { 40, -20, "A" }, { 40, 20, "B" }},  // Common, A, B
        80, 50,
        { .switch_spdt = {
            .position = 0,          // Default to A
            .r_on = 0.01,
            .r_off = 1e15,          // Must be >> 1/GMIN for proper isolation
            .momentary = false,
            .default_pos = 0
        }}
    },

    [COMP_PUSH_BUTTON] = {
        "Push Button", "PB", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 30,
        { .push_button = {
            .pressed = false,
            .r_on = 0.01,
            .r_off = 1e15               // Must be >> 1/GMIN for proper isolation
        }}
    },

    [COMP_TRANSFORMER] = {
        "Transformer", "T", 4,
        {{ -50, -20, "P1" }, { -50, 20, "P2" }, { 50, -20, "S1" }, { 50, 20, "S2" }},
        100, 60,
        { .transformer = {
            .l_primary = 10e-3,         // 10mH primary
            .turns_ratio = 1.0,         // 1:1 ratio
            .coupling = 0.99,           // 99% coupling
            .r_primary = 0.1,           // 100 mOhm DCR
            .r_secondary = 0.1,         // 100 mOhm DCR
            .n_primary = 100,           // 100 turns primary
            .n_secondary = 100,         // 100 turns secondary
            .ideal = true,              // Ideal model
            .center_tap = false
        }}
    },

    [COMP_TRANSFORMER_CT] = {
        "Transformer CT", "T", 5,
        {{ -50, -20, "P1" }, { -50, 20, "P2" }, { 50, -30, "S1" }, { 50, 0, "CT" }, { 50, 30, "S2" }},
        100, 80,
        { .transformer = {
            .l_primary = 10e-3,         // 10mH primary
            .turns_ratio = 1.0,         // 1:1 ratio (full secondary)
            .coupling = 0.99,           // 99% coupling
            .r_primary = 0.1,           // 100 mOhm DCR
            .r_secondary = 0.1,         // 100 mOhm DCR
            .n_primary = 100,           // 100 turns primary
            .n_secondary = 100,         // 100 turns secondary (50+50)
            .ideal = true,              // Ideal model
            .center_tap = true
        }}
    },

    // === ADDITIONAL PASSIVE COMPONENTS ===

    [COMP_POTENTIOMETER] = {
        "Potentiometer", "POT", 3,
        {{ -40, 0, "1" }, { 40, 0, "2" }, { 0, -20, "W" }},
        80, 40,
        { .potentiometer = {
            .resistance = 10000.0,      // 10k total resistance
            .wiper_pos = 0.5,           // Center position
            .tolerance = 20.0,
            .taper = 0,                 // Linear
            .ideal = true
        }}
    },

    [COMP_PHOTORESISTOR] = {
        "Photoresistor", "LDR", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 30,
        { .photoresistor = {
            .r_dark = 1e6,              // 1 MOhm in darkness
            .r_light = 100.0,           // 100 Ohm in bright light
            .light_level = 0.5,         // Medium light
            .gamma = 0.7,
            .ideal = true
        }}
    },

    [COMP_THERMISTOR] = {
        "Thermistor", "TH", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 30,
        { .thermistor = {
            .r_25 = 10000.0,            // 10k at 25°C
            .beta = 3950.0,             // Typical NTC beta
            .temp = 25.0,               // Room temperature
            .type = 0,                  // NTC
            .ideal = true
        }}
    },

    [COMP_MEMRISTOR] = {
        "Memristor", "MR", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 20,
        { .resistor = {
            .resistance = 1000.0,
            .tolerance = 10.0,
            .ideal = true
        }}
    },

    [COMP_FUSE] = {
        "Fuse", "F", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 20,
        { .fuse = {
            .rating = 1.0,              // 1A rating
            .resistance = 0.01,         // 10 mOhm cold resistance
            .i2t = 1.0,                 // I²t rating (A²s) - typical for 1A fast-blow
            .i2t_accumulated = 0.0,     // Accumulated energy starts at 0
            .current = 0.0,             // No current initially
            .blow_time = -1.0,          // Not blown (-1 means never blown)
            .blown = false,
            .ideal = true
        }}
    },

    [COMP_CRYSTAL] = {
        "Crystal", "Y", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 30,
        { .crystal = {
            /* A 100 kHz teaching crystal: high enough Q (2 pi f Ls / Rs = 314) to behave like
               quartz, low enough to integrate at the time steps these circuits run at. A real
               32.768 kHz watch part is Ls ~ 7900 H, Rs ~ 40k, Q ~ 40000. */
            .ls = 100e-3,               // 100 mH motional inductance
            .cs = 25.33e-12,            // with Ls gives fs = 100.0 kHz
            .rs = 200.0,                // Q = 314
            .cp = 33e-12,               // holder capacitance - a real HC-49 is 3..30 pF
            .ideal = false
        }}
    },

    [COMP_SPARK_GAP] = {
        "Spark Gap", "SG", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 30,
        { .spark_gap = {
            .gap_mm = 1.0,              // 3 kV breakdown
            .r_on = 1.0,
            .hold_current = 0.01,
            .quench_time = 2e-6,
            .conducting = false,
            .last_conduct_time = 0.0
        }}
    },

    [COMP_DELAY_LINE] = {
        "Delay Line", "TD", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 30,
        { .delay_line = {
            .z0 = 50.0,                 // RG-58 and most of the world
            .delay = 5e-9,              // about a metre of coax at 0.66 c
            .loss_db = 0.0,
            .ideal = true,
            .hist = NULL, .hist_t = NULL, .cap = 0, .head = 0, .count = 0
        }}
    },

    [COMP_TLINE] = {
        "Transmission Line", "TL", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 50,   /* the drawn body is 64 x 44: two masts and a catenary need the room */
        { .tline = {
            .length_mi = 100.0,
            .r_per_mi = 0.06,           // 345 kV twin Drake
            .x_per_mi = 0.55,
            .b_us_per_mi = 8.0,
            .model = 2
        }}
    },

    [COMP_SOURCE_3PH] = {
        "3-Phase Source", "G", 4,
        {{ 40, -20, "A" }, { 40, 0, "B" }, { 40, 20, "C" }, { 0, 40, "N" }},
        80, 80,
        { .source_3ph = {
            .v_peak = 392.0,            // 277 V rms L-N (480 V system)
            .frequency = 60.0,
            .phase = 0.0,
            .r_series = 0.001,
            .l_series = 0.0
        }}
    },

    [COMP_TOROID] = {
        "Toroid", "TL", 1,
        {{ 0, 40, "1" }},
        120, 90,
        { .toroid = {
            .major_in = 13.0,           // 4 x 13 inch: ~14.5 pF
            .minor_in = 4.0,
            .voltage = 0.0
        }}
    },

    // === ADDITIONAL SOURCES ===

    [COMP_AC_CURRENT] = {
        "AC Current", "~I", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .ac_current = {
            .amplitude = 0.001,         // 1mA peak
            .frequency = 60.0,
            .phase = 0.0,
            .offset = 0.0,
            .r_parallel = 1e9,
            .ideal = true
        }}
    },

    [COMP_CLOCK] = {
        "Clock", "CLK", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .clock = {
            .frequency = 1000.0,        // 1 kHz
            .v_low = 0.0,
            .v_high = 5.0,
            .duty = 0.5,
            .ideal = true
        }}
    },

    [COMP_VADC_SOURCE] = {
        "Variable DC", "VDC", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .dc_voltage = {
            .voltage = 5.0,
            .r_series = 0.001,
            .ideal = true
        }}
    },

    [COMP_AM_SOURCE] = {
        "AM Source", "AM", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .ac_voltage = {
            .amplitude = 5.0,
            .frequency = 1000.0,
            .phase = 0.0,
            .offset = 0.0,
            .ideal = true
        }}
    },

    [COMP_FM_SOURCE] = {
        "FM Source", "FM", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .ac_voltage = {
            .amplitude = 5.0,
            .frequency = 1000.0,
            .phase = 0.0,
            .offset = 0.0,
            .ideal = true
        }}
    },

    [COMP_BATTERY] = {
        "Battery", "BAT", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .battery = {
            .nominal_voltage = 1.5,       // AA battery
            .capacity_mah = 2500.0,       // Typical AA capacity
            .internal_r = 0.1,            // 100mOhm internal resistance
            .charge_state = 1.0,          // Fully charged
            .charge_coulombs = 9000.0,    // 2500mAh * 3.6 = 9000 C
            .current_draw = 0.0,
            .v_cutoff = 0.9,              // Cutoff voltage
            .discharged = false,
            .ideal = false                // Non-ideal by default for discharge
        }}
    },

    [COMP_PULSE_SOURCE] = {
        "Pulse Source", "PLS", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .pulse_source = {
            .v_low = 0.0,
            .v_high = 5.0,
            .delay = 0.0,
            .rise_time = 1e-9,
            .fall_time = 1e-9,
            .pulse_width = 0.0005,      // 500us
            .period = 0.001,            // 1ms
            .r_series = 0.001,
            .ideal = true
        }}
    },

    [COMP_PWM_SOURCE] = {
        "PWM Source", "PWM", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .pwm_source = {
            .amplitude = 5.0,
            .frequency = 1000.0,        // 1 kHz PWM
            .duty = 0.5,
            .offset = 0.0,
            .r_series = 0.001,
            .ideal = true
        }}
    },

    [COMP_PWL_SOURCE] = {
        "PWL Source", "PWL", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .pwl_source = {
            .times = {0.0, 0.001, 0.002, 0.003, 0.004},  // Example: 0, 1ms, 2ms, 3ms, 4ms
            .values = {0.0, 5.0, 5.0, 0.0, 0.0},        // Step up to 5V, hold, step down
            .num_points = 5,
            .repeat = true,
            .repeat_period = 0.0,       // Auto from last time point
            .r_series = 0.001,
            .ideal = true
        }}
    },

    [COMP_EXPR_SOURCE] = {
        "Expr Source", "V(t)", 2,
        {{ 0, -40, "+" }, { 0, 40, "-" }},
        40, 80,
        { .expr_source = {
            .expression = "5*sin(2*pi*60*t)",  // Default: 60Hz sine wave
            .r_series = 0.001,
            .cached_value = 0.0,
            .cache_time = -1.0,
            .ideal = true
        }}
    },

    // === ADDITIONAL DIODES ===

    [COMP_VARACTOR] = {
        "Varactor", "DV", 2,
        {{ -40, 0, "A" }, { 40, 0, "K" }},
        80, 20,
        { .diode = {
            .is = 1e-14,
            .vt = 0.026,
            .n = 1.0,
            .cjo = 50e-12,              // 50pF zero-bias capacitance
            .ideal = true
        }}
    },

    [COMP_TUNNEL_DIODE] = {
        "Tunnel Diode", "DT", 2,
        {{ -40, 0, "A" }, { 40, 0, "K" }},
        80, 20,
        { .diode = {
            .is = 1e-6,                 // Higher saturation current
            .vt = 0.026,
            .n = 1.0,
            .ideal = true
        }}
    },

    [COMP_PHOTODIODE] = {
        "Photodiode", "PD", 2,
        {{ -40, 0, "A" }, { 40, 0, "K" }},
        80, 20,
        { .diode = {
            .is = 1e-9,                 // Photocurrent at full illumination
            .vt = 0.026,
            .n = 1.0,
            .ideal = true
        }}
    },

    // === ADDITIONAL TRANSISTORS ===

    [COMP_NPN_DARLINGTON] = {
        "NPN Darlington", "QD", 3,
        {{ -20, 0, "B" }, { 20, -20, "C" }, { 20, 20, "E" }},
        60, 60,
        { .bjt = {
            .bf = 10000.0,              // Very high beta (100 x 100)
            .is = 1e-14,
            .vaf = 100.0,
            .nf = 1.0,
            .br = 1.0,
            .var = 100.0,
            .nr = 1.0,
            .temp = 300.0,
            .ideal = true
        }}
    },

    [COMP_PNP_DARLINGTON] = {
        "PNP Darlington", "QD", 3,
        {{ -20, 0, "B" }, { 20, -20, "C" }, { 20, 20, "E" }},
        60, 60,
        { .bjt = {
            .bf = 10000.0,
            .is = 1e-14,
            .vaf = 100.0,
            .nf = 1.0,
            .br = 1.0,
            .var = 100.0,
            .nr = 1.0,
            .temp = 300.0,
            .ideal = true
        }}
    },

    [COMP_NJFET] = {
        "N-JFET", "J", 3,
        {{ -20, 0, "G" }, { 20, -20, "D" }, { 20, 20, "S" }},
        60, 60,
        { .jfet = {
            .idss = 10e-3,              // 10mA IDSS
            .vp = -2.0,                 // -2V pinch-off
            .lambda = 0.01,
            .beta = 2.5e-3,             // IDSS / Vp^2
            .temp = 300.0,
            .ideal = true
        }}
    },

    [COMP_PJFET] = {
        "P-JFET", "J", 3,
        {{ -20, 0, "G" }, { 20, -20, "D" }, { 20, 20, "S" }},
        60, 60,
        { .jfet = {
            .idss = 10e-3,
            .vp = 2.0,                  // +2V pinch-off for P-channel
            .lambda = 0.01,
            .beta = 2.5e-3,
            .temp = 300.0,
            .ideal = true
        }}
    },

    // === THYRISTORS ===

    [COMP_SCR] = {
        "SCR", "SCR", 3,
        {{ -20, 0, "G" }, { 20, -20, "A" }, { 20, 20, "K" }},
        60, 60,
        { .scr = {
            .vgt = 0.7,
            .igt = 10e-3,
            .ih = 10e-3,
            .vf = 1.5,
            .on = false,
            .ideal = true
        }}
    },

    [COMP_DIAC] = {
        "DIAC", "DC", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 20,
        { .diac = {
            .vbo = 30.0,                // 30V breakover
            .vf = 2.0,
            .ideal = true
        }}
    },

    [COMP_TRIAC] = {
        "TRIAC", "TR", 3,
        {{ -20, 0, "G" }, { 20, -20, "MT1" }, { 20, 20, "MT2" }},
        60, 60,
        { .triac = {
            .vgt = 1.0,
            .igt = 25e-3,
            .ih = 25e-3,
            .vf = 1.5,
            .on = false,
            .ideal = true
        }}
    },

    [COMP_UJT] = {
        "UJT", "UJT", 3,
        {{ -20, 0, "E" }, { 20, -20, "B2" }, { 20, 20, "B1" }},
        60, 60,
        { .bjt = {
            .bf = 50.0,
            .is = 1e-12,
            .ideal = true
        }}
    },

    // === OP-AMPS & AMPLIFIERS ===

    [COMP_OPAMP_REAL] = {
        "Real Op-Amp", "U", 3,
        {{ -40, -20, "-" }, { -40, 20, "+" }, { 40, 0, "OUT" }},
        80, 60,
        { .opamp = {
            .gain = 100000.0,
            .voffset = 1e-3,            // 1mV offset
            .vmax = 15.0,
            .vmin = -15.0,
            .gbw = 1e6,
            .slew_rate = 0.5,
            .r_in = 1e6,                // 1 MOhm (finite)
            .r_out = 75.0,
            .i_bias = 100e-9,           // 100nA bias current
            .cmrr = 80.0,
            .rail_to_rail = false,
            .ideal = false
        }}
    },

    [COMP_OTA] = {
        "OTA", "OTA", 4,
        {{ -40, -20, "-" }, { -40, 20, "+" }, { 40, 0, "OUT" }, { 0, 30, "Iabc" }},
        80, 70,
        { .opamp = {
            .gain = 1000.0,             // Transconductance based
            .vmax = 15.0,
            .vmin = -15.0,
            .ideal = true
        }}
    },

    [COMP_CCII_PLUS] = {
        "CCII+", "CCII", 3,
        {{ -40, 0, "X" }, { 0, -30, "Y" }, { 40, 0, "Z" }},
        80, 60,
        { .controlled_source = {
            .gain = 1.0,
            .ideal = true
        }}
    },

    [COMP_CCII_MINUS] = {
        "CCII-", "CCII", 3,
        {{ -40, 0, "X" }, { 0, -30, "Y" }, { 40, 0, "Z" }},
        80, 60,
        { .controlled_source = {
            .gain = -1.0,
            .ideal = true
        }}
    },

    // === CONTROLLED SOURCES ===

    [COMP_VCVS] = {
        "VCVS", "E", 4,
        {{ -40, -20, "+" }, { -40, 20, "-" }, { 40, -20, "+" }, { 40, 20, "-" }},
        80, 60,
        { .controlled_source = {
            .gain = 1.0,                // V/V
            .ideal = true
        }}
    },

    [COMP_VCCS] = {
        "VCCS", "G", 4,
        {{ -40, -20, "+" }, { -40, 20, "-" }, { 40, -20, "+" }, { 40, 20, "-" }},
        80, 60,
        { .controlled_source = {
            .gain = 0.001,              // A/V (1 mS)
            .ideal = true
        }}
    },

    [COMP_CCVS] = {
        "CCVS", "H", 4,
        {{ -40, -20, "+" }, { -40, 20, "-" }, { 40, -20, "+" }, { 40, 20, "-" }},
        80, 60,
        { .controlled_source = {
            .gain = 1000.0,             // V/A (1k transresistance)
            .r_in = 0.001,              // Sensing resistance
            .ideal = true
        }}
    },

    [COMP_CCCS] = {
        "CCCS", "F", 4,
        {{ -40, -20, "+" }, { -40, 20, "-" }, { 40, -20, "+" }, { 40, 20, "-" }},
        80, 60,
        { .controlled_source = {
            .gain = 1.0,                // A/A
            .r_in = 0.001,
            .ideal = true
        }}
    },

    // === ADDITIONAL SWITCHES ===

    [COMP_DPDT_SWITCH] = {
        "DPDT Switch", "SW", 4,
        {{ -40, -20, "C1" }, { -40, 20, "C2" }, { 40, -20, "A" }, { 40, 20, "B" }},
        80, 60,
        { .switch_spdt = {
            .position = 0,
            .r_on = 0.01,
            .r_off = 1e9,
            .momentary = false,
            .default_pos = 0
        }}
    },

    [COMP_RELAY] = {
        "Relay", "K", 4,
        {{ -40, -20, "C+" }, { -40, 20, "C-" }, { 40, -20, "NO" }, { 40, 20, "COM" }},
        80, 60,
        { .relay = {
            .v_coil = 12.0,
            .r_coil = 200.0,
            .l_coil = 0.1,              // 100mH coil inductance (for kickback)
            .i_pickup = 0.05,           // 50mA pickup (pull-in)
            .i_dropout = 0.01,          // 10mA dropout (release)
            .r_contact_on = 0.1,
            .r_contact_off = 1e9,
            .i_coil = 0.0,              // Initial coil current
            .energized = false,
            .ideal = false              // Non-ideal by default for kickback
        }}
    },

    [COMP_ANALOG_SWITCH] = {
        "Analog Switch", "ASW", 3,
        {{ -40, 0, "IN" }, { 40, 0, "OUT" }, { 0, 20, "CTL" }},
        80, 40,
        { .analog_switch = {
            .v_on = 2.5,
            .v_off = 0.8,
            .r_on = 100.0,
            .r_off = 1e9,
            .state = false,
            .ideal = true
        }}
    },

    // === LOGIC GATES ===

    [COMP_LOGIC_INPUT] = {
        "Logic Input", "IN", 1,
        {{ 20, 0, "OUT" }},
        40, 30,
        { .logic_input = {
            .state = false,
            .v_low = 0.0,
            .v_high = 5.0,
            .r_out = 100.0
        }}
    },

    [COMP_LOGIC_OUTPUT] = {
        "Logic Output", "OUT", 1,
        {{ -20, 0, "IN" }},
        40, 30,
        { .logic_output = {
            .v_threshold = 2.5,
            .state = false
        }}
    },

    [COMP_NOT_GATE] = {
        "NOT Gate", "NOT", 2,
        {{ -40, 0, "IN" }, { 40, 0, "OUT" }},
        80, 40,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .prop_delay = 10e-9,
            .num_inputs = 1,
            .state = false,
            .ideal = true
        }}
    },

    [COMP_AND_GATE] = {
        "AND Gate", "AND", 3,
        {{ -40, -20, "A" }, { -40, 20, "B" }, { 40, 0, "OUT" }},
        80, 50,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .prop_delay = 10e-9,
            .num_inputs = 2,
            .state = false,
            .ideal = true
        }}
    },

    [COMP_OR_GATE] = {
        "OR Gate", "OR", 3,
        {{ -40, -20, "A" }, { -40, 20, "B" }, { 40, 0, "OUT" }},
        80, 50,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .prop_delay = 10e-9,
            .num_inputs = 2,
            .state = false,
            .ideal = true
        }}
    },

    [COMP_NAND_GATE] = {
        "NAND Gate", "NAND", 3,
        {{ -40, -20, "A" }, { -40, 20, "B" }, { 40, 0, "OUT" }},
        80, 50,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .prop_delay = 10e-9,
            .num_inputs = 2,
            .state = false,
            .ideal = true
        }}
    },

    [COMP_NOR_GATE] = {
        "NOR Gate", "NOR", 3,
        {{ -40, -20, "A" }, { -40, 20, "B" }, { 40, 0, "OUT" }},
        80, 50,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .prop_delay = 10e-9,
            .num_inputs = 2,
            .state = false,
            .ideal = true
        }}
    },

    [COMP_XOR_GATE] = {
        "XOR Gate", "XOR", 3,
        {{ -40, -20, "A" }, { -40, 20, "B" }, { 40, 0, "OUT" }},
        80, 50,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .prop_delay = 10e-9,
            .num_inputs = 2,
            .state = false,
            .ideal = true
        }}
    },

    [COMP_XNOR_GATE] = {
        "XNOR Gate", "XNOR", 3,
        {{ -40, -20, "A" }, { -40, 20, "B" }, { 40, 0, "OUT" }},
        80, 50,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .prop_delay = 10e-9,
            .num_inputs = 2,
            .state = false,
            .ideal = true
        }}
    },

    [COMP_BUFFER] = {
        "Buffer", "BUF", 2,
        {{ -40, 0, "IN" }, { 40, 0, "OUT" }},
        80, 40,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .prop_delay = 5e-9,
            .num_inputs = 1,
            .state = false,
            .ideal = true
        }}
    },

    [COMP_TRISTATE_BUF] = {
        "Tri-State Buf", "TRI", 3,
        {{ -40, 0, "IN" }, { 40, 0, "OUT" }, { 0, 20, "EN" }},
        80, 50,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .prop_delay = 10e-9,
            .num_inputs = 2,
            .state = false,
            .ideal = true
        }}
    },

    [COMP_SCHMITT_INV] = {
        "Schmitt Inv", "SINV", 2,
        {{ -40, 0, "IN" }, { 40, 0, "OUT" }},
        80, 40,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,         // Will use hysteresis in simulation
            .r_out = 100.0,
            .prop_delay = 15e-9,
            .num_inputs = 1,
            .state = false,
            .ideal = true
        }}
    },

    [COMP_SCHMITT_BUF] = {
        "Schmitt Buf", "SBUF", 2,
        {{ -40, 0, "IN" }, { 40, 0, "OUT" }},
        80, 40,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .prop_delay = 15e-9,
            .num_inputs = 1,
            .state = false,
            .ideal = true
        }}
    },

    // === DIGITAL ICs ===

    [COMP_D_FLIPFLOP] = {
        "D Flip-Flop", "DFF", 4,
        {{ -40, -20, "D" }, { -40, 20, "CLK" }, { 40, -20, "Q" }, { 40, 20, "Qn" }},
        80, 60,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .state = false,
            .ideal = true
        }}
    },

    [COMP_JK_FLIPFLOP] = {
        "JK Flip-Flop", "JKFF", 5,
        {{ -40, -20, "J" }, { -40, 0, "CLK" }, { -40, 20, "K" }, { 40, -20, "Q" }, { 40, 20, "Qn" }},
        80, 70,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .state = false,
            .ideal = true
        }}
    },

    [COMP_T_FLIPFLOP] = {
        "T Flip-Flop", "TFF", 3,
        {{ -40, 0, "T" }, { 40, -20, "Q" }, { 40, 20, "Qn" }},
        80, 50,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .state = false,
            .ideal = true
        }}
    },

    [COMP_SR_LATCH] = {
        "SR Latch", "SR", 4,
        {{ -40, -20, "S" }, { -40, 20, "R" }, { 40, -20, "Q" }, { 40, 20, "Qn" }},
        80, 60,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .state = false,
            .ideal = true
        }}
    },

    /* Four bits, a reset and a carry, so it can actually drive a BCD decoder and chain into the
       next digit. It used to be two output pins with no logic behind them at all - nothing ever
       incremented it. */
    [COMP_COUNTER] = {
        /* Q0..Q3 sit on the same 40 px pitch as the BCD decoder's A..D, so a counter feeding a
           decoder is four straight wires. CARRY goes on the bottom edge, out of their way. */
        "Counter", "CNT", 7,
        {{ -40, -40, "CLK" }, { -40, 40, "RST" },
         { 40, -60, "Q0" }, { 40, -20, "Q1" }, { 40, 20, "Q2" }, { 40, 60, "Q3" }, { 0, 70, "CY" }},
        80, 140,
        { .counter = {
            .modulus = 10,          // a decade counter by default: 0..9
            .count = 0,
            .wrapped = false,
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .ideal = true
        }}
    },

    [COMP_SHIFT_REG] = {
        "Shift Register", "SR", 4,
        {{ -40, -20, "DIN" }, { -40, 20, "CLK" }, { 40, -20, "Q0" }, { 40, 20, "Q1" }},
        80, 60,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .state = false,
            .ideal = true
        }}
    },

    [COMP_MUX_2TO1] = {
        "2:1 Mux", "MUX", 4,
        {{ -40, -20, "A" }, { -40, 20, "B" }, { 0, 30, "SEL" }, { 40, 0, "Y" }},
        80, 70,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .state = false,
            .ideal = true
        }}
    },

    [COMP_DEMUX_1TO2] = {
        "1:2 Demux", "DEMUX", 4,
        {{ -40, 0, "IN" }, { 0, 30, "SEL" }, { 40, -20, "Y0" }, { 40, 20, "Y1" }},
        80, 70,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .state = false,
            .ideal = true
        }}
    },

    [COMP_DECODER] = {
        "Decoder", "DEC", 4,
        {{ -40, 0, "IN" }, { 40, -20, "Y0" }, { 40, 0, "Y1" }, { 40, 20, "Y2" }},
        80, 70,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .state = false,
            .ideal = true
        }}
    },

    [COMP_BCD_DECODER] = {
        "BCD Decoder", "7447", 11,
        {{ -40, -60, "A" }, { -40, -20, "B" }, { -40, 20, "C" }, { -40, 60, "D" },
         { 40, -60, "a" }, { 40, -40, "b" }, { 40, -20, "c" }, { 40, 0, "d" },
         { 40, 20, "e" }, { 40, 40, "f" }, { 40, 60, "g" }},
        80, 140,
        { .bcd_decoder = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .active_low = true,
            .blanking = false,
            .lamp_test = false,
            .ideal = true
        }}
    },

    [COMP_HALF_ADDER] = {
        "Half Adder", "HA", 4,
        {{ -40, -20, "A" }, { -40, 20, "B" }, { 40, -20, "S" }, { 40, 20, "C" }},
        80, 60,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .state = false,
            .ideal = true
        }}
    },

    [COMP_FULL_ADDER] = {
        "Full Adder", "FA", 5,
        {{ -40, -20, "A" }, { -40, 0, "B" }, { -40, 20, "Cin" }, { 40, -20, "S" }, { 40, 20, "Cout" }},
        80, 70,
        { .logic_gate = {
            .v_low = 0.0,
            .v_high = 5.0,
            .v_threshold = 2.5,
            .r_out = 100.0,
            .state = false,
            .ideal = true
        }}
    },

    // === MIXED SIGNAL ===

    [COMP_555_TIMER] = {
        "555 Timer", "555", 5,
        {{ -40, -20, "VCC" }, { -40, 20, "GND" }, { 0, -30, "TRG" }, { 0, 30, "THR" }, { 40, 0, "OUT" }},
        80, 80,
        { .timer_555 = {
            .r1 = 10000.0,
            .r2 = 10000.0,
            .c = 10e-6,
            .mode = 0,                  // Astable
            .vcc = 5.0,
            .output = false,
            .cap_voltage = 0.0,
            .ideal = true
        }}
    },

    [COMP_DAC] = {
        "DAC", "DAC", 4,
        {{ -40, -20, "D0" }, { -40, 20, "D1" }, { 40, 0, "OUT" }, { 0, 30, "REF" }},
        80, 70,
        { .controlled_source = {
            .gain = 1.0,
            .ideal = true
        }}
    },

    [COMP_ADC] = {
        "ADC", "ADC", 4,
        {{ -40, 0, "IN" }, { 0, 30, "REF" }, { 40, -20, "D0" }, { 40, 20, "D1" }},
        80, 70,
        { .controlled_source = {
            .gain = 1.0,
            .ideal = true
        }}
    },

    [COMP_VCO] = {
        "VCO", "VCO", 3,
        {{ -40, 0, "VIN" }, { 40, 0, "OUT" }, { 0, 30, "GND" }},
        80, 60,
        { .ac_voltage = {
            .amplitude = 5.0,
            .frequency = 1000.0,
            .ideal = true
        }}
    },

    [COMP_PLL] = {
        "PLL", "PLL", 4,
        {{ -40, -20, "IN" }, { -40, 20, "REF" }, { 40, -20, "OUT" }, { 40, 20, "LOCK" }},
        80, 70,
        { .controlled_source = {
            .gain = 1.0,
            .ideal = true
        }}
    },

    [COMP_MONOSTABLE] = {
        "Monostable", "MONO", 3,
        {{ -40, 0, "TRG" }, { 40, 0, "Q" }, { 0, 30, "Qn" }},
        80, 60,
        { .timer_555 = {
            .r1 = 10000.0,
            .c = 1e-6,
            .mode = 1,                  // Monostable
            .vcc = 5.0,
            .ideal = true
        }}
    },

    [COMP_OPTOCOUPLER] = {
        "Optocoupler", "OC", 4,
        {{ -40, -20, "A" }, { -40, 20, "K" }, { 40, -20, "C" }, { 40, 20, "E" }},
        80, 60,
        { .bjt = {
            .bf = 100.0,
            .is = 1e-14,
            .ideal = true
        }}
    },

    // === VOLTAGE REGULATORS ===

    [COMP_LM317] = {
        "LM317", "LM317", 3,
        {{ -40, 0, "IN" }, { 40, 0, "OUT" }, { 0, 30, "ADJ" }},
        80, 60,
        { .dc_voltage = {
            .voltage = 1.25,            // Reference voltage
            .r_series = 0.1,
            .ideal = true
        }}
    },

    [COMP_7805] = {
        "7805", "7805", 3,
        {{ -40, 0, "IN" }, { 40, 0, "OUT" }, { 0, 30, "GND" }},
        80, 60,
        { .dc_voltage = {
            .voltage = 5.0,             // Fixed 5V output
            .r_series = 0.1,
            .ideal = true
        }}
    },

    [COMP_TL431] = {
        "TL431", "TL431", 3,
        {{ -40, 0, "K" }, { 40, 0, "A" }, { 0, 30, "REF" }},
        80, 60,
        { .zener = {
            .vz = 2.5,                  // 2.5V reference
            .rz = 0.2,
            .ideal = true
        }}
    },

    // === DISPLAY/OUTPUT ===

    [COMP_LAMP] = {
        "Lamp", "LP", 2,
        {{ -40, 0, "+" }, { 40, 0, "-" }},
        80, 30,
        { .lamp = {
            .power_rating = 5.0,        // 5W lamp
            .voltage_rating = 12.0,
            .r_cold = 10.0,
            .r_hot = 29.0,              // Hot resistance = V²/P
            .brightness = 0.0,
            .ideal = true
        }}
    },

    [COMP_7SEG_DISPLAY] = {
        "7-Seg Display", "7SEG", 9,
        {{ -40, -40, "a" }, { -40, -20, "b" }, { -40, 0, "c" }, { -40, 20, "d" }, { -40, 40, "COM" },
         { 40, -40, "e" }, { 40, -20, "f" }, { 40, 0, "g" }, { 40, 20, "DP" }},
        80, 100,
        { .seven_seg = {
            .vf = 2.0,
            .max_current = 0.02,
            .common_cathode = true,
            .segments = 0,
            .ideal = true
        }}
    },

    [COMP_LED_ARRAY] = {
        "LED Array", "BAR", 9,
        {{ -70, -30, "1" }, { -50, -30, "2" }, { -30, -30, "3" }, { -10, -30, "4" },
         { 10, -30, "5" }, { 30, -30, "6" }, { 50, -30, "7" }, { 70, -30, "8" },
         { 0, 30, "COM" }},
        160, 60,
        { .led_array = {
            .is = 5e-20,  // Saturation current (tuned for Vf=2.0V @ 20mA, RED default)
            .n = 2.0,     // Ideality factor
            .vf = 2.0,
            .max_current = 0.030,  // 30 mA max per segment (standard 5mm LED)
            .currents = {0, 0, 0, 0, 0, 0, 0, 0},
            .failed = {false, false, false, false, false, false, false, false},
            .color = LED_COLOR_RED  // Default to RED (classic bar graph displays often use red/green)
        }}
    },

    [COMP_LED_MATRIX] = {
        "LED Matrix 8x8", "DOT", 16,
        {{ -60, -52, "R0" }, { -60, -37, "R1" }, { -60, -22, "R2" }, { -60, -7, "R3" },
         { -60, 8, "R4" }, { -60, 23, "R5" }, { -60, 38, "R6" }, { -60, 53, "R7" },
         { 60, -52, "C0" }, { 60, -37, "C1" }, { 60, -22, "C2" }, { 60, -7, "C3" },
         { 60, 8, "C4" }, { 60, 23, "C5" }, { 60, 38, "C6" }, { 60, 53, "C7" }},
        120, 130,
        { .led_matrix = {
            .pixel_state = {0, 0, 0, 0, 0, 0, 0, 0},
            .vf = 2.0,
            .if_max = 0.02,
            .color = 0,
            .common_cathode = true
        }}
    },

    [COMP_DC_MOTOR] = {
        "DC Motor", "M", 2,
        {{ -40, 0, "+" }, { 40, 0, "-" }},
        80, 50,
        { .dc_motor = {
            .r_armature = 1.0,          // Armature resistance (Ohm)
            .l_armature = 1e-3,         // Armature inductance (H)
            .kv = 0.01,                 // Back-EMF constant (V/rad/s)
            .kt = 0.01,                 // Torque constant (Nm/A) - usually equal to kv
            .j_rotor = 1e-5,            // Rotor inertia (kg*m^2)
            .b_friction = 1e-6,         // Viscous friction (Nm*s/rad)
            .omega = 0.0,               // Initial angular velocity
            .current = 0.0,             // Initial current
            .torque_load = 0.0,         // External load torque
            .v_bemf = 0.0,              // Back-EMF (calculated)
            .ideal = false              // Full physics model by default
        }}
    },

    // === WIRELESS ===

    [COMP_ANTENNA_TX] = {
        "Antenna TX", "TX", 2,
        {{ -40, 0, "+" }, { 40, 0, "-" }},
        80, 40,
        { .antenna = {
            .channel = 0,               // Channel 0 by default
            .r_series = 50.0,           // 50 ohm (standard RF impedance)
            .voltage = 0.0,
            .gain = 1.0,
            .ideal = false
        }}
    },

    [COMP_ANTENNA_RX] = {
        "Antenna RX", "RX", 2,
        {{ -40, 0, "+" }, { 40, 0, "-" }},
        80, 40,
        { .antenna = {
            .channel = 0,               // Channel 0 by default
            .r_series = 50.0,           // 50 ohm (standard RF impedance)
            .voltage = 0.0,
            .gain = 1.0,
            .ideal = false
        }}
    },

    // === WIRING ===

    [COMP_BUS] = {
        "Bus", "BUS", 2,
        {{ -40, 0, "A" }, { 40, 0, "B" }},
        80, 30,
        { .bus = {
            .width = 8,                 // 8-bit bus by default
            .name = "DATA",
            .bus_id = 0
        }}
    },

    [COMP_BUS_TAP] = {
        "Bus Tap", "TAP", 2,
        {{ -20, 0, "BUS" }, { 20, 0, "SIG" }},
        40, 30,
        { .bus_tap = {
            .bus_id = 0,
            .tap_index = 0,
            .signal_name = "D0"
        }}
    },

    // === SUB-CIRCUITS ===

    [COMP_PIN] = {
        "Pin Marker", "P", 1,  // One terminal for connection
        {{ 40, 0, "P" }},      // Terminal at right side (40, 0)
        60, 30,
        { .pin = {
            .pin_number = 1,
            .pin_name = ""
        }}
    },

    [COMP_SUBCIRCUIT] = {
        "Sub-Circuit", "IC", 4,  // Default 4 pins, dynamically adjusted
        {{ -40, -20, "1" }, { -40, 20, "2" }, { 40, -20, "3" }, { 40, 20, "4" }},
        80, 60,
        { .subcircuit = {
            .def_id = -1,           // No definition yet
            .name = "U1"
        }}
    },

    // === MEASUREMENT ===

    [COMP_VOLTMETER] = {
        "Voltmeter", "VM", 2,
        {{ -40, 0, "+" }, { 40, 0, "-" }},
        80, 40,
        { .voltmeter = {
            .r_in = 10e6,               // 10 MOhm input resistance
            .reading = 0.0,
            .ideal = true
        }}
    },

    [COMP_AMMETER] = {
        "Ammeter", "AM", 2,
        {{ -40, 0, "+" }, { 40, 0, "-" }},
        80, 40,
        { .ammeter = {
            .r_shunt = 0.01,            // 10 mOhm shunt
            .reading = 0.0,
            .ideal = true
        }}
    },

    [COMP_WATTMETER] = {
        "Wattmeter", "WM", 4,
        {{ -40, -20, "V+" }, { -40, 20, "V-" }, { 40, -20, "I+" }, { 40, 20, "I-" }},
        80, 60,
        { .voltmeter = {
            .r_in = 10e6,
            .reading = 0.0,
            .ideal = true
        }}
    },

    [COMP_TEST_POINT] = {
        "Test Point", "TP", 1,
        {{ 0, 0, "TP" }},
        20, 20,
        { .voltmeter = {
            .r_in = 1e12,
            .reading = 0.0,
            .ideal = true
        }}
    },

    [COMP_LABEL] = {
        "Label", "LBL", 1,
        {{ 0, 0, "N" }},
        40, 20,
        { .text = {
            .text = "Node",
            .font_size = 2,
            .color = 0xFFFFFFFF
        }}
    },
};

static int next_component_id = 1;

static double g_arb[ARB_TABLES][ARB_MAX];
static int g_arb_n[ARB_TABLES];

void arb_table_set(int idx, const double *v, int n) {
    if (idx < 0 || idx >= ARB_TABLES || !v) return;
    if (n > ARB_MAX) n = ARB_MAX;
    for (int i = 0; i < n; i++) g_arb[idx][i] = v[i];
    g_arb_n[idx] = n;
}
int arb_table_len(int idx) { return (idx >= 0 && idx < ARB_TABLES) ? g_arb_n[idx] : 0; }

/* Tables 0 and 1 hold a built-in outline until a file is loaded, so the X-Y Plotter draws
   something the moment it is placed. The curve is the classic heart parametrisation. */
static void arb_default_tables(void) {
    static int done = 0;
    if (done) return;
    done = 1;
    static double xs[256], ys[256];
    for (int i = 0; i < 256; i++) {
        double t = 2.0 * M_PI * i / 256.0;
        double st = sin(t);
        xs[i] = 16.0 * st * st * st;
        ys[i] = 13.0 * cos(t) - 5.0 * cos(2 * t) - 2.0 * cos(3 * t) - cos(4 * t);
    }
    double xm = 0, ym = 0;
    for (int i = 0; i < 256; i++) { if (fabs(xs[i]) > xm) xm = fabs(xs[i]); if (fabs(ys[i]) > ym) ym = fabs(ys[i]); }
    for (int i = 0; i < 256; i++) { xs[i] /= xm; ys[i] /= ym; }
    arb_table_set(0, xs, 256);
    arb_table_set(1, ys, 256);
}

double arb_table_value(int idx, double phase) {
    /* Bounds first. "idx < 2" is true of every negative number, so this read of g_arb_n[idx]
       used to happen before anything had checked the index - and the index is a field in a saved
       file, which means a corrupted or hand-edited circuit could point it anywhere and take the
       whole app down. Everything below this line was already guarded; this line was not. */
    if (idx < 0 || idx >= ARB_TABLES) return 0.0;
    if (idx < 2 && g_arb_n[idx] == 0) arb_default_tables();
    int n = arb_table_len(idx);
    if (n <= 0) return 0.0;
    if (n == 1) return g_arb[idx][0];
    phase = phase - floor(phase);
    double fp = phase * n;
    int i = (int)fp;
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    int j = (i + 1) % n;                       /* wrap: the table is a closed loop */
    double f = fp - i;
    return g_arb[idx][i] * (1.0 - f) + g_arb[idx][j] * f;
}

int arb_load_xy_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    static double xs[ARB_MAX], ys[ARB_MAX];
    int n = 0;
    char line[256];
    while (n < ARB_MAX && fgets(line, sizeof line, f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == ';' || *p == '\n' || *p == '\r' || *p == 0) continue;
        double a, b2;
        for (char *q = p; *q; q++) if (*q == ',' || *q == ';') *q = ' ';
        if (sscanf(p, "%lf %lf", &a, &b2) == 2) { xs[n] = a; ys[n] = b2; n++; }
    }
    fclose(f);
    if (n < 2) return 0;
    /* normalise both axes to -1..1 so any input scale plots sensibly */
    double xmin = xs[0], xmax = xs[0], ymin = ys[0], ymax = ys[0];
    for (int i = 1; i < n; i++) {
        if (xs[i] < xmin) xmin = xs[i]; if (xs[i] > xmax) xmax = xs[i];
        if (ys[i] < ymin) ymin = ys[i]; if (ys[i] > ymax) ymax = ys[i];
    }
    double xr = (xmax - xmin) > 1e-12 ? (xmax - xmin) : 1.0;
    double yr = (ymax - ymin) > 1e-12 ? (ymax - ymin) : 1.0;
    for (int i = 0; i < n; i++) {
        xs[i] = 2.0 * (xs[i] - xmin) / xr - 1.0;
        ys[i] = 2.0 * (ys[i] - ymin) / yr - 1.0;
    }
    arb_table_set(0, xs, n);
    arb_table_set(1, ys, n);
    return n;
}

const char *component_search_keywords(ComponentType type) {
    switch (type) {
        case COMP_RESISTOR:        return "resistor res ohm";
        case COMP_LOAD_HP:         return "resistor load high power hp";
        case COMP_CAPACITOR:       return "capacitor cap farad";
        case COMP_CAPACITOR_ELEC:  return "capacitor electrolytic polar cap";
        case COMP_INDUCTOR:        return "inductor coil henry choke";
        case COMP_POTENTIOMETER:   return "potentiometer pot variable resistor trimmer";
        case COMP_DIODE:           return "diode rectifier";
        case COMP_SCHOTTKY:        return "diode schottky";
        case COMP_ZENER:           return "diode zener reference";
        case COMP_LED:             return "led light diode";
        case COMP_NPN_BJT:         return "transistor bjt npn bipolar";
        case COMP_PNP_BJT:         return "transistor bjt pnp bipolar";
        case COMP_NMOS:            return "transistor mosfet fet nmos n-channel 2n7000 2n7002 irf540n";
        case COMP_PMOS:            return "transistor mosfet fet pmos p-channel";
        case COMP_OPAMP: case COMP_OPAMP_FLIPPED: return "opamp op-amp amplifier operational";
        case COMP_DC_VOLTAGE:      return "battery dc voltage source supply";
        case COMP_AC_VOLTAGE:      return "ac voltage source sine generator";
        case COMP_ARB_SOURCE:      return "arbitrary waveform table xy plot data";
        case COMP_DC_CURRENT: case COMP_AC_CURRENT: return "current source";
        case COMP_GROUND:          return "ground gnd earth";
        case COMP_TRANSFORMER: case COMP_TRANSFORMER_CT: return "transformer xfmr coil";
        case COMP_TLINE:           return "transmission line power line tline";
        case COMP_SOURCE_3PH:      return "three phase 3ph generator grid";
        case COMP_SPARK_GAP:       return "spark gap arc";
        case COMP_TOROID:          return "toroid tesla top load";
        case COMP_SPST_SWITCH: case COMP_SPDT_SWITCH: case COMP_DPDT_SWITCH: return "switch";
        case COMP_ANALOG_SWITCH:   return "switch analog";
        case COMP_CRYSTAL:         return "crystal xtal quartz";
        case COMP_FUSE:            return "fuse";
        case COMP_THERMISTOR:      return "thermistor temperature ntc";
        case COMP_NOT_GATE: case COMP_AND_GATE: case COMP_OR_GATE: case COMP_NAND_GATE: case COMP_NOR_GATE: case COMP_XOR_GATE: case COMP_XNOR_GATE:
                                   return "logic gate digital";
        case COMP_VOLTMETER: case COMP_AMMETER: case COMP_WATTMETER: return "meter measure";
        default:                   return "";
    }
}


/* ======================= Named part models =======================
   Every number below is from the manufacturer's data sheet, and the comment says which line.
   The square-law MOSFET model is fitted at the datasheet's own R_DS(on) point, since that is
   the figure a design is built around: K = 1 / (R_DS(on) (V_GS,test - V_th)).  --part-test
   rebuilds each of these conditions and checks the model reproduces the number. */

static void part_2n7000(Component *c) {
    c->props.mosfet.vth = 2.1;        /* V_GS(th) 0.8 - 3.0 V, 2.1 typ */
    c->props.mosfet.kp = 0.105;       /* R_DS(on) 1.2 ohm typ at V_GS = 10 V, I_D = 0.5 A */
    c->props.mosfet.w = 1e-6; c->props.mosfet.l = 1e-6;
    c->props.mosfet.lambda = 0.02;
    c->props.mosfet.ideal = false;
}
static void part_2n7002(Component *c) {
    c->props.mosfet.vth = 1.6;        /* V_GS(th) 1.0 - 2.5 V */
    c->props.mosfet.kp = 0.060;       /* R_DS(on) 2 ohm typ at V_GS = 10 V */
    c->props.mosfet.w = 1e-6; c->props.mosfet.l = 1e-6;
    c->props.mosfet.lambda = 0.02;
    c->props.mosfet.ideal = false;
}
static void part_irf540n(Component *c) {
    c->props.mosfet.vth = 4.0;        /* V_GS(th) 2 - 4 V */
    c->props.mosfet.kp = 3.8;         /* R_DS(on) 44 mohm max at V_GS = 10 V, I_D = 17 A */
    c->props.mosfet.w = 1e-6; c->props.mosfet.l = 1e-6;
    c->props.mosfet.lambda = 0.005;
    c->props.mosfet.ideal = false;
}
static void part_bs250(Component *c) {
    c->props.mosfet.vth = -3.0;       /* P-channel, V_GS(th) -1 to -3.5 V */
    c->props.mosfet.kp = 0.0143;      /* R_DS(on) 10 ohm typ at V_GS = -10 V (14 ohm max) */
    c->props.mosfet.w = 1e-6; c->props.mosfet.l = 1e-6;
    c->props.mosfet.lambda = 0.02;
    c->props.mosfet.ideal = false;
}
static void part_irf9540n(Component *c) {
    c->props.mosfet.vth = -3.0;       /* P-channel, V_GS(th) -2 to -4 V */
    c->props.mosfet.kp = 0.71;        /* R_DS(on) 0.2 ohm max at V_GS = -10 V, I_D = -14 A */
    c->props.mosfet.w = 1e-6; c->props.mosfet.l = 1e-6;
    c->props.mosfet.lambda = 0.01;
    c->props.mosfet.ideal = false;
}
static void part_2n3904(Component *c) {
    c->props.bjt.bf = 200;            /* h_FE 100 - 300 at I_C = 10 mA */
    c->props.bjt.is = 6.7e-15;        /* gives V_BE ~ 0.66 V at 10 mA */
    c->props.bjt.vaf = 74;
    c->props.bjt.nf = 1.0;
    c->props.bjt.ideal = false;
}
static void part_bc547b(Component *c) {
    c->props.bjt.bf = 290;            /* B grade: h_FE 200 - 450 at I_C = 2 mA */
    c->props.bjt.is = 7.0e-15;
    c->props.bjt.vaf = 63;
    c->props.bjt.nf = 1.0;
    c->props.bjt.ideal = false;
}
static void part_2n3906(Component *c) {
    c->props.bjt.bf = 180;            /* h_FE 100 - 300 at I_C = 10 mA */
    c->props.bjt.is = 1.4e-14;
    c->props.bjt.vaf = 100;
    c->props.bjt.nf = 1.0;
    c->props.bjt.ideal = false;
}
static void part_1n4148(Component *c) {
    c->props.diode.is = 2.52e-9;      /* the standard 1N4148 SPICE model */
    c->props.diode.n = 1.752;
    c->props.diode.bv = 100.0;        /* V_R 100 V */
    c->props.diode.ideal = false;     /* Shockley: the soft knee is the whole point of this part */
}
static void part_1n4001(Component *c) {
    c->props.diode.is = 1.4e-9;       /* fitted to V_F = 1.0 V typ at 1 A (1.1 V max) */
    c->props.diode.n = 1.9;
    c->props.diode.bv = 50.0;
    c->props.diode.ideal = false;
}
static void part_1n4733a(Component *c) {
    c->props.zener.vz = 5.1;          /* 5.1 V, 1 W, I_ZT = 49 mA */
    c->props.zener.rz = 7.0;          /* Z_ZT 7 ohm */
    c->props.zener.ideal = false;
}
static void part_x5r_10u(Component *c) {
    c->props.capacitor.capacitance = 10e-6;   /* 10 uF 6.3 V X5R, 0805 */
    c->props.capacitor.esr = 5e-3;            /* 5 mohm at 100 kHz */
    c->props.capacitor.esl = 0.5e-9;
    c->props.capacitor.leakage = 1e8;
    c->props.capacitor.v_half = 2.0;          /* down to half its value near 2 V of bias */
    c->props.capacitor.ideal = false;
}
static void part_c0g_10n(Component *c) {
    c->props.capacitor.capacitance = 10e-9;   /* 10 nF C0G/NP0: class I, no bias loss */
    c->props.capacitor.esr = 10e-3;
    c->props.capacitor.esl = 0.4e-9;
    c->props.capacitor.leakage = 1e10;
    c->props.capacitor.v_half = 0.0;
    c->props.capacitor.ideal = false;
}
static void part_alum_100u(Component *c) {
    c->props.capacitor_elec.capacitance = 100e-6;  /* 100 uF 25 V aluminium electrolytic */
    c->props.capacitor_elec.esr = 0.5;             /* the figure that decides a supply's ripple */
    c->props.capacitor_elec.leakage = 1e5;
    c->props.capacitor_elec.ideal = false;
}
static void part_lm358(Component *c) {
    c->props.opamp.gain = 100000;     /* A_VOL 100 V/mV */
    c->props.opamp.gbw = 1e6;         /* 1 MHz */
    c->props.opamp.slew_rate = 0.5;   /* 0.5 V/us */
    c->props.opamp.voffset = 2e-3;    /* V_IO 2 mV typ, 7 mV max */
    c->props.opamp.i_bias = 45e-9;    /* I_IB 45 nA typ */
    c->props.opamp.cmrr = 85;         /* 85 dB typ */
    c->props.opamp.r_out = 75;
    c->props.opamp.rail_to_rail = false;
    c->props.opamp.ideal = false;
}
static void part_lm741(Component *c) {
    c->props.opamp.gain = 200000;     /* A_VOL 200 V/mV typ */
    c->props.opamp.gbw = 1e6;
    c->props.opamp.slew_rate = 0.5;
    c->props.opamp.voffset = 1e-3;    /* V_IO 1 mV typ */
    c->props.opamp.i_bias = 80e-9;    /* I_IB 80 nA typ */
    c->props.opamp.cmrr = 90;
    c->props.opamp.r_out = 75;
    c->props.opamp.rail_to_rail = false;
    c->props.opamp.ideal = false;
}
static void part_tl072(Component *c) {
    c->props.opamp.gain = 200000;     /* A_VD 200 V/mV typ */
    c->props.opamp.gbw = 3e6;         /* 3 MHz */
    c->props.opamp.slew_rate = 13.0;  /* 13 V/us */
    c->props.opamp.voffset = 3e-3;    /* V_IO 3 mV typ */
    c->props.opamp.i_bias = 65e-12;   /* JFET inputs: 65 pA */
    c->props.opamp.cmrr = 100;
    c->props.opamp.r_out = 75;
    c->props.opamp.rail_to_rail = false;
    c->props.opamp.ideal = false;
}
static void part_mcp6001(Component *c) {
    c->props.opamp.gain = 100000;     /* A_OL 112 dB typ */
    c->props.opamp.gbw = 1e6;         /* 1 MHz */
    c->props.opamp.slew_rate = 0.6;   /* 0.6 V/us */
    c->props.opamp.voffset = 4.5e-3;  /* V_OS +/- 4.5 mV max */
    c->props.opamp.i_bias = 1e-12;    /* 1 pA */
    c->props.opamp.cmrr = 76;
    c->props.opamp.r_out = 75;
    c->props.opamp.rail_to_rail = true;   /* the reason to pick it */
    c->props.opamp.ideal = false;
}
/* The regulator symbols reuse the source / zener property structs, so the datasheet figure
   goes into the field the stamp actually reads. */
static void part_lm317(Component *c) {
    c->props.dc_voltage.voltage = 1.25;   /* V_REF 1.25 V (1.20 - 1.30 over line and load) */
    c->props.dc_voltage.r_series = 0.1;
}
static void part_lm7805(Component *c) {
    c->props.dc_voltage.voltage = 5.0;    /* V_O 4.8 - 5.2 V */
    c->props.dc_voltage.r_series = 0.1;
}
static void part_tl431(Component *c) {
    c->props.zener.vz = 2.495;            /* V_ref 2.495 V, +/- 1 % (A grade) */
    c->props.zener.rz = 0.2;
}

static const PartModel g_parts[] = {
    { "2N7000",  COMP_NMOS,    "logic-level NMOS, V_th 2.1 V, R_DS(on) 1.2 ohm at V_GS 10 V", part_2n7000 },
    { "2N7002",  COMP_NMOS,    "SOT-23 NMOS, V_th 1.6 V, R_DS(on) 2 ohm at V_GS 10 V",        part_2n7002 },
    { "IRF540N", COMP_NMOS,    "power NMOS, V_th 4 V, R_DS(on) 44 mohm at V_GS 10 V",         part_irf540n },
    { "BS250",   COMP_PMOS,    "P-channel, V_th -3 V, R_DS(on) 14 ohm at V_GS -10 V",         part_bs250 },
    { "IRF9540N",COMP_PMOS,    "power P-channel, V_th -3 V, R_DS(on) 0.2 ohm at V_GS -10 V",  part_irf9540n },
    { "2N3904",  COMP_NPN_BJT, "general-purpose NPN, h_FE 200 at 10 mA, V_AF 74 V",           part_2n3904 },
    { "BC547B",  COMP_NPN_BJT, "small-signal NPN, h_FE 290 (B grade), V_AF 63 V",             part_bc547b },
    { "2N3906",  COMP_PNP_BJT, "general-purpose PNP, h_FE 180 at 10 mA",                      part_2n3906 },
    { "1N4148",  COMP_DIODE,   "small-signal silicon, V_F 0.72 V at 5 mA, V_R 100 V",         part_1n4148 },
    { "1N4001",  COMP_DIODE,   "1 A rectifier, V_F 1.1 V at 1 A, V_R 50 V",                   part_1n4001 },
    { "1N4733A", COMP_ZENER,   "5.1 V 1 W zener, Z_ZT 7 ohm at 49 mA",                        part_1n4733a },
    { "X5R 10uF", COMP_CAPACITOR, "10 uF 6.3 V X5R: 5 mohm ESR, and half its value at 2 V bias", part_x5r_10u },
    { "C0G 10nF", COMP_CAPACITOR, "10 nF C0G/NP0: class I, no capacitance lost to DC bias",      part_c0g_10n },
    { "Alu 100uF", COMP_CAPACITOR_ELEC, "100 uF 25 V aluminium, ESR 0.5 ohm",                    part_alum_100u },
    { "LM358",   COMP_OPAMP,   "dual op-amp, 1 MHz, 0.5 V/us, V_os 2 mV, I_B 45 nA",          part_lm358 },
    { "LM741",   COMP_OPAMP,   "the classic, 1 MHz, 0.5 V/us, V_os 1 mV, I_B 80 nA",          part_lm741 },
    { "TL072",   COMP_OPAMP,   "JFET input, 3 MHz, 13 V/us, I_B 65 pA",                       part_tl072 },
    { "MCP6001", COMP_OPAMP,   "rail-to-rail CMOS, 1 MHz, 0.6 V/us, I_B 1 pA",                part_mcp6001 },
    { "LM317",   COMP_LM317,   "adjustable regulator, V_ref 1.25 V",                          part_lm317 },
    { "LM7805",  COMP_7805,    "fixed 5 V regulator, 4.8 - 5.2 V",                            part_lm7805 },
    { "TL431",   COMP_TL431,   "programmable shunt reference, V_ref 2.495 V",                 part_tl431 },
};

int component_part_count(void) { return (int)(sizeof g_parts / sizeof g_parts[0]); }

const PartModel *component_part_at(int i) {
    if (i < 0 || i >= component_part_count()) return NULL;
    return &g_parts[i];
}

int component_parts_for(ComponentType type, int *idx, int n) {
    int found = 0;
    for (int i = 0; i < component_part_count(); i++) {
        if (g_parts[i].type != type) continue;
        if (idx && found < n) idx[found] = i;
        found++;
    }
    return found;
}

bool component_apply_part_idx(Component *c, int idx) {
    const PartModel *m = component_part_at(idx);
    if (!c || !m || m->type != c->type) return false;
    m->apply(c);
    snprintf(c->part, sizeof c->part, "%s", m->part);
    return true;
}

bool component_apply_part(Component *c, const char *part) {
    if (!c || !part) return false;
    for (int i = 0; i < component_part_count(); i++) {
        if (g_parts[i].type != c->type) continue;
        const char *a = g_parts[i].part, *b = part;
        while (*a && *b && toupper((unsigned char)*a) == toupper((unsigned char)*b)) { a++; b++; }
        if (!*a && !*b) return component_apply_part_idx(c, i);
    }
    return false;
}

void component_cycle_part(Component *c) {
    if (!c) return;
    int idx[16];
    int n = component_parts_for(c->type, idx, 16);
    if (n <= 0) return;
    if (n > 16) n = 16;
    int cur = -1;                                  /* -1 = generic */
    for (int i = 0; i < n; i++)
        if (c->part[0] && !strcmp(component_part_at(idx[i])->part, c->part)) { cur = i; break; }
    /* The operating point the panel shows lives in the same union as the model parameters, so
       restoring defaults would blank it - and since the simulation is paused while you click,
       nothing would refresh it and every device would read 0. Carry it across. */
    double op_vgs = c->props.mosfet.op_vgs, op_vds = c->props.mosfet.op_vds;
    double op_id = c->props.mosfet.op_id, op_gm = c->props.mosfet.op_gm;
    int op_region = c->props.mosfet.op_region;
    bool is_mos = (c->type == COMP_NMOS || c->type == COMP_PMOS);

    int next = cur + 1;
    if (next >= n) {                               /* wrap back to the generic component */
        c->part[0] = '\0';
        const ComponentTypeInfo *info = component_get_info(c->type);
        if (info) c->props = info->default_props;
    } else {
        component_apply_part_idx(c, idx[next]);
    }
    if (is_mos) {
        c->props.mosfet.op_vgs = op_vgs; c->props.mosfet.op_vds = op_vds;
        c->props.mosfet.op_id = op_id;   c->props.mosfet.op_gm = op_gm;
        c->props.mosfet.op_region = op_region;
    }
}

int g_subcircuit_depth = 0;      /* how deep the current stamp is inside nested blocks */

SubCircuitDef *subcircuit_find_def(int def_id) {
    for (int i = 0; i < g_subcircuit_library.count; i++)
        if (g_subcircuit_library.defs[i].id == def_id) return &g_subcircuit_library.defs[i];
    return NULL;
}

/* Give this instance its own copies of the definition's components, so their state (a
   capacitor's charge, an inductor's current) belongs to the block rather than to the template
   every block shares. Rebuilt only when the definition behind the block changes. */
static Component *subcircuit_instance(Component *comp, SubCircuitDef *def, int *count) {
    if (comp->props.subcircuit.inst_data && comp->props.subcircuit.inst_def_id == def->id &&
        comp->props.subcircuit.inst_count == def->num_components) {
        *count = comp->props.subcircuit.inst_count;
        return (Component *)comp->props.subcircuit.inst_data;
    }
    subcircuit_release_instance(comp);
    comp->props.subcircuit.inst_data = malloc((size_t)def->num_components * sizeof(Component));
    if (!comp->props.subcircuit.inst_data) { *count = 0; return NULL; }
    memcpy(comp->props.subcircuit.inst_data, def->component_data,
           (size_t)def->num_components * sizeof(Component));
    Component *arr = (Component *)comp->props.subcircuit.inst_data;
    for (int i = 0; i < def->num_components; i++) {
        arr[i].trap_i_prev = 0; arr[i].cap_vc = 0;
        arr[i].tline_ic_prev[0] = arr[i].tline_ic_prev[1] = 0;
        arr[i].sat_last_rail = 0; arr[i].sat_flips = 0; arr[i].slew_latch = 0;
        arr[i].mos_vds_lin = 0;
        if (arr[i].type == COMP_SUBCIRCUIT) {
            /* never share the template's pointer - the nested block builds its own copies the
               first time it is stamped, so its state belongs to this instance too */
            arr[i].props.subcircuit.inst_data = NULL;
            arr[i].props.subcircuit.inst_count = 0;
            arr[i].props.subcircuit.inst_def_id = 0;
        }
    }
    comp->props.subcircuit.inst_count = def->num_components;
    comp->props.subcircuit.inst_def_id = def->id;
    *count = def->num_components;
    return arr;
}

int component_subcircuit_instance(Component *comp, Component **out) {
    if (!comp || comp->type != COMP_SUBCIRCUIT || !comp->props.subcircuit.inst_data) return 0;
    if (out) *out = (Component *)comp->props.subcircuit.inst_data;
    return comp->props.subcircuit.inst_count;
}

const ComponentTypeInfo *component_get_info(ComponentType type) {
    if (type >= 0 && type < COMP_TYPE_COUNT) {
        return &component_info[type];
    }
    return &component_info[COMP_NONE];
}

double toroid_capacitance(const Component *comp) {
    // Bert Pool's toroid formula, D and d in inches, result in pF:
    //   C = (1 + (0.2781 - d/D)) * 2.8 * sqrt(pi * (D - d) * d / 4)
    double D = comp->props.toroid.major_in, d = comp->props.toroid.minor_in;
    if (D < 0.5) D = 0.5;
    if (d < 0.1) d = 0.1;
    if (d > D * 0.9) d = D * 0.9;
    double pf = (1.0 + (0.2781 - d / D)) * 2.8 * sqrt(M_PI * (D - d) * d / 4.0);
    return pf * 1e-12;
}

void tline_params(const Component *comp, double *R, double *L, double *C_end) {
    const double w = 2.0 * M_PI * 60.0;
    double len = fmax(comp->props.tline.length_mi, 1e-3);
    *R = fmax(comp->props.tline.r_per_mi, 0.0) * len;
    *L = (comp->props.tline.model >= 1) ? fmax(comp->props.tline.x_per_mi, 0.0) * len / w : 0.0;
    *C_end = (comp->props.tline.model >= 2) ? fmax(comp->props.tline.b_us_per_mi, 0.0) * 1e-6 * len / w / 2.0 : 0.0;
}

/* How many auxiliary matrix rows a component needs. A subcircuit needs one for every internal
   component that needs one - a voltage source or an inductor inside the block has to have a row
   of its own, or it stamps into somebody else's and reads as 0 V. */
/* A block inside a block is counted too, so nesting reserves the rows the whole tree needs.
   `depth` stops a definition that somehow contains itself from recursing for ever. */
static int component_aux_count_depth(const Component *comp, int depth) {
    if (!comp) return 0;
    if (comp->type == COMP_SOURCE_3PH) return 3;
    /* one current per half-secondary: without them there is nothing to reflect into the
       primary, and the winding is a free source - see the stamp */
    if (comp->type == COMP_TRANSFORMER_CT) return 2;
    if (comp->type == COMP_SUBCIRCUIT) {
        if (depth >= SUBCIRCUIT_MAX_DEPTH) return 0;
        SubCircuitDef *def = subcircuit_find_def(comp->props.subcircuit.def_id);
        if (!def || !def->component_data) return 0;
        const Component *arr = (const Component *)def->component_data;
        int n = 0;
        for (int i = 0; i < def->num_components; i++) {
            if (arr[i].type == COMP_PIN || arr[i].type == COMP_LABEL ||
                arr[i].type == COMP_TEST_POINT) continue;
            n += component_aux_count_depth(&arr[i], depth + 1);
        }
        return n;
    }
    return comp->needs_voltage_var ? 1 : 0;
}

int component_aux_count(const Component *comp) {
    return component_aux_count_depth(comp, 0);
}

double spark_gap_breakdown(const Component *comp) {
    return 3000.0 * comp->props.spark_gap.gap_mm;   // ~30 kV/cm in air at 1 atm
}

static void delay_line_alloc(Component *comp);   /* defined with component_free, below */

/* What port `port` launched at time t, interpolated from the ring buffer. Before the line has
   that much history it has launched nothing, so the far end sees zero - which is exactly right
   for a line that starts at rest. */
double delay_line_history(const Component *comp, int port, double t) {
    if (!comp || comp->type != COMP_DELAY_LINE) return 0.0;
    int cap = comp->props.delay_line.cap, n = comp->props.delay_line.count;
    if (cap <= 0 || n <= 0) return 0.0;
    const double *ht = comp->props.delay_line.hist_t;
    const double *hv = comp->props.delay_line.hist + (port ? cap : 0);
    int head = comp->props.delay_line.head;
    int oldest = (n < cap) ? 0 : head;                 /* ring: oldest sample */
    if (t <= ht[oldest]) return 0.0;                   /* before the line was running */
    /* walk back from the newest until we straddle t */
    for (int k = 1; k <= n; k++) {
        int i = (head - k + cap) % cap;                /* newest first */
        if (ht[i] <= t) {
            int j = (i + 1) % cap;                     /* the sample after it */
            if (k == 1) return hv[i];                  /* t is at or past the newest */
            double dt = ht[j] - ht[i];
            if (dt <= 0) return hv[i];
            double f = (t - ht[i]) / dt;
            return hv[i] + f * (hv[j] - hv[i]);
        }
    }
    return 0.0;
}

/* Record what each port launched this step: v + Z0 i, the wave leaving into the line. */
void delay_line_record(Component *comp, double t, double v0, double i0, double v1, double i1) {
    if (!comp || comp->type != COMP_DELAY_LINE) return;
    int cap = comp->props.delay_line.cap;
    if (cap <= 0) return;
    double z0 = comp->props.delay_line.z0;
    int h = comp->props.delay_line.head;
    comp->props.delay_line.hist_t[h] = t;
    comp->props.delay_line.hist[h] = v0 + z0 * i0;
    comp->props.delay_line.hist[cap + h] = v1 + z0 * i1;
    comp->props.delay_line.head = (h + 1) % cap;
    if (comp->props.delay_line.count < cap) comp->props.delay_line.count++;
}


Component *component_create(ComponentType type, float x, float y) {
    if (type <= COMP_NONE || type >= COMP_TYPE_COUNT) {
        return NULL;
    }

    Component *comp = calloc(1, sizeof(Component));
    if (!comp) return NULL;

    const ComponentTypeInfo *info = component_get_info(type);

    comp->id = next_component_id++;
    comp->type = (type == COMP_LOAD_HP) ? COMP_RESISTOR : type;   // HP load is a resistor flavour
    comp->x = x;
    comp->y = y;
    comp->rotation = 0;
    comp->num_terminals = info->num_terminals;
    comp->props = info->default_props;
    if (comp->type == COMP_DELAY_LINE) delay_line_alloc(comp);

    /* Wake the mixed-signal logic engine up for the counter. logic_init_component had no caller
       anywhere in the program, so is_logic_component was false for everything and the whole
       sample -> propagate -> drive pass in logic.c did nothing; every digital part that works
       today does so because its own stamp reads the input voltages directly. The counter cannot
       work that way - it is sequential, and a stamp runs many times per time step, so it would
       count once per Newton iteration. Only the counter is switched on here: flipping the rest
       over would change how every existing logic template behaves and belongs on its own. */
    if (comp->type == COMP_COUNTER) {
        logic_init_component(comp);
        comp->props = info->default_props;   /* logic_init must not stamp on the counter state */
    }

    // Special initialization for text component (char array needs explicit copy)
    if (type == COMP_TEXT) {
        strncpy(comp->props.text.text, "Label", sizeof(comp->props.text.text) - 1);
        comp->props.text.text[sizeof(comp->props.text.text) - 1] = '\0';
        comp->props.text.font_size = 2;
        comp->props.text.color = 0xFFFFFFFF;
    }

    // Special initialization for PIN component - auto-increment pin number
    if (type == COMP_PIN) {
        static int next_pin_number = 1;
        comp->props.pin.pin_number = next_pin_number;
        snprintf(comp->props.pin.pin_name, sizeof(comp->props.pin.pin_name), "P%d", next_pin_number);
        next_pin_number++;
    }

    // Special initialization for LED component - calculate Is based on color
    if (type == COMP_LED) {
        double vf, max_current, wavelength;
        led_get_color_params(comp->props.led.color, &vf, &max_current, &wavelength);
        comp->props.led.vf = vf;
        comp->props.led.max_current = max_current;
        comp->props.led.wavelength = wavelength;
        comp->props.led.is = led_calc_is_for_vf(vf, comp->props.led.n, comp->props.led.vt);
    }

    // Special initialization for LED_ARRAY component - calculate Is based on color
    if (type == COMP_LED_ARRAY) {
        double vf, max_current, wavelength;
        led_get_color_params(comp->props.led_array.color, &vf, &max_current, &wavelength);
        comp->props.led_array.vf = vf;
        comp->props.led_array.max_current = max_current;
        comp->props.led_array.is = led_calc_is_for_vf(vf, comp->props.led_array.n, 0.026);
    }

    // Set default label
    snprintf(comp->label, MAX_LABEL_LEN, "%s%d", info->short_name, comp->id);

    // Determine if component needs voltage variable (voltage sources, inductors)
    comp->needs_voltage_var = (type == COMP_CRYSTAL ||     /* the motional arm's current */
                               type == COMP_DC_VOLTAGE ||
                               type == COMP_AC_VOLTAGE ||
                               type == COMP_ARB_SOURCE ||
                               type == COMP_INDUCTOR ||
                               type == COMP_OPAMP ||
                               type == COMP_OPAMP_FLIPPED ||
                               type == COMP_OPAMP_REAL ||
                               type == COMP_OTA ||
                               type == COMP_SQUARE_WAVE ||
                               type == COMP_TRIANGLE_WAVE ||
                               type == COMP_SAWTOOTH_WAVE ||
                               type == COMP_NOISE_SOURCE ||
                               type == COMP_CLOCK ||
                               type == COMP_VADC_SOURCE ||
                               type == COMP_AM_SOURCE ||
                               type == COMP_FM_SOURCE ||
                               type == COMP_BATTERY ||
                               type == COMP_PULSE_SOURCE ||
                               type == COMP_PWM_SOURCE ||
                               type == COMP_TRANSFORMER ||
                               type == COMP_TRANSFORMER_CT ||
                               type == COMP_TLINE ||
                               type == COMP_SOURCE_3PH);

    // Initialize thermal state for components that can fail
    comp->thermal.temperature = 25.0;           // Room temperature
    comp->thermal.ambient_temperature = 25.0;
    comp->thermal.power_dissipated = 0.0;
    comp->thermal.damage = 0.0;
    comp->thermal.damage_threshold = 1.5;       // Start damage at 150% power rating
    comp->thermal.failure_time = -1.0;
    comp->thermal.failed = false;
    comp->thermal.smoke_active = false;
    comp->thermal.num_smoke = 0;

    // Component-specific thermal parameters
    switch (type) {
        case COMP_RESISTOR:
            comp->thermal.max_temperature = comp->props.resistor.high_power ? 0.0 : 155.0;   // HP loads: no thermal limit
            comp->thermal.thermal_mass = 0.1;        // Small thermal mass
            comp->thermal.thermal_resistance = 100.0; // °C/W to ambient
            break;
        case COMP_NPN_BJT:
        case COMP_PNP_BJT:
            comp->thermal.max_temperature = 150.0;   // Junction temp limit
            comp->thermal.thermal_mass = 0.05;
            comp->thermal.thermal_resistance = 200.0;
            break;
        case COMP_NMOS:
        case COMP_PMOS:
            comp->thermal.max_temperature = 175.0;
            comp->thermal.thermal_mass = 0.05;
            comp->thermal.thermal_resistance = 150.0;
            break;
        case COMP_CAPACITOR:
        case COMP_CAPACITOR_ELEC:
            comp->thermal.max_temperature = 105.0;   // Electrolytic cap limit
            comp->thermal.thermal_mass = 0.2;
            comp->thermal.thermal_resistance = 80.0;
            break;
        case COMP_LED:
            comp->thermal.max_temperature = 100.0;
            comp->thermal.thermal_mass = 0.02;
            comp->thermal.thermal_resistance = 250.0;
            break;
        default:
            comp->thermal.max_temperature = 150.0;
            comp->thermal.thermal_mass = 0.1;
            comp->thermal.thermal_resistance = 100.0;
            break;
    }

    return comp;
}

void component_update_led_color(Component *comp) {
    if (!comp) return;

    if (comp->type == COMP_LED) {
        // Clamp color to valid range
        if (comp->props.led.color < 0 || comp->props.led.color >= LED_COLOR_COUNT) {
            comp->props.led.color = LED_COLOR_RED;
        }

        double vf, max_current, wavelength;
        led_get_color_params(comp->props.led.color, &vf, &max_current, &wavelength);
        comp->props.led.vf = vf;
        comp->props.led.max_current = max_current;
        comp->props.led.wavelength = wavelength;
        comp->props.led.is = led_calc_is_for_vf(vf, comp->props.led.n, comp->props.led.vt);
    } else if (comp->type == COMP_LED_ARRAY) {
        // Clamp color to valid range
        if (comp->props.led_array.color < 0 || comp->props.led_array.color >= LED_COLOR_COUNT) {
            comp->props.led_array.color = LED_COLOR_RED;
        }

        double vf, max_current, wavelength;
        led_get_color_params(comp->props.led_array.color, &vf, &max_current, &wavelength);
        comp->props.led_array.vf = vf;
        comp->props.led_array.max_current = max_current;
        comp->props.led_array.is = led_calc_is_for_vf(vf, comp->props.led_array.n, 0.026);
    }
}

/* Release a block's instance copies WITHOUT freeing the component itself - the nested ones
   live inside an array, so component_free()'s free(comp) would be freeing an array element. */
void subcircuit_release_instance(Component *comp) {
    if (!comp || comp->type != COMP_SUBCIRCUIT || !comp->props.subcircuit.inst_data) return;
    Component *inner = (Component *)comp->props.subcircuit.inst_data;
    for (int i = 0; i < comp->props.subcircuit.inst_count; i++)
        subcircuit_release_instance(&inner[i]);          /* frees the tree, not the array */
    free(comp->props.subcircuit.inst_data);
    comp->props.subcircuit.inst_data = NULL;
    comp->props.subcircuit.inst_count = 0;
    comp->props.subcircuit.inst_def_id = 0;
}

/* The delay line carries a history buffer, because its whole model is "what the far end
   launched one delay ago". It is the only component with a heap allocation of its own besides
   a subcircuit instance, so it gets the same treatment: allocated on create, freed on free,
   and a clone starts with its own empty one rather than sharing. */
#define DELAY_LINE_CAP 8192
static void delay_line_alloc(Component *comp);
static void delay_line_release(Component *comp);

static void delay_line_alloc(Component *comp) {
    if (!comp || comp->type != COMP_DELAY_LINE) return;
    comp->props.delay_line.hist = calloc(2 * DELAY_LINE_CAP, sizeof(double));
    comp->props.delay_line.hist_t = calloc(DELAY_LINE_CAP, sizeof(double));
    comp->props.delay_line.cap = comp->props.delay_line.hist && comp->props.delay_line.hist_t
                                 ? DELAY_LINE_CAP : 0;
    comp->props.delay_line.head = 0;
    comp->props.delay_line.count = 0;
}

static void delay_line_release(Component *comp) {
    if (!comp || comp->type != COMP_DELAY_LINE) return;
    free(comp->props.delay_line.hist);
    free(comp->props.delay_line.hist_t);
    comp->props.delay_line.hist = NULL;
    comp->props.delay_line.hist_t = NULL;
    comp->props.delay_line.cap = comp->props.delay_line.head = comp->props.delay_line.count = 0;
}

void component_adopt_props(Component *comp, const ComponentProps *p) {
    if (!comp || !p) return;
    /* Props are a plain union, so copying one wholesale over a live component also copies any
       pointer inside it. A delay line owns two heap buffers that way. Loading a file used to
       do exactly this: the component's own buffers leaked and it inherited the addresses the
       *saving* process had, which are then freed a second time when both circuits go away -
       a double free that took the whole run down two templates later. Copy the values, keep
       the buffers this component already owns. */
    double *hist = NULL, *hist_t = NULL;
    int cap = 0, head = 0, count = 0;
    if (comp->type == COMP_DELAY_LINE) {
        hist = comp->props.delay_line.hist;
        hist_t = comp->props.delay_line.hist_t;
        cap = comp->props.delay_line.cap;
        head = comp->props.delay_line.head;
        count = comp->props.delay_line.count;
    }
    comp->props = *p;
    if (comp->type == COMP_DELAY_LINE) {
        comp->props.delay_line.hist = hist;
        comp->props.delay_line.hist_t = hist_t;
        comp->props.delay_line.cap = cap;
        comp->props.delay_line.head = head;
        comp->props.delay_line.count = count;
    }
    if (comp->type == COMP_SUBCIRCUIT) {
        /* The other one that owns memory through the union. It is rebuilt from the definition
           on the next solve, so start it empty rather than pointing at whatever the array was
           at in the process that wrote the file. */
        comp->props.subcircuit.inst_data = NULL;
        comp->props.subcircuit.inst_count = 0;
        comp->props.subcircuit.inst_def_id = 0;
    }
    if (comp->type == COMP_ARB_SOURCE) {
        /* Not a pointer, but the same kind of trust: an index into a fixed set of tables, read
           straight out of a file. Out of range it names no table, and the value readout would
           print it. Bring it back inside. */
        int tbl = comp->props.arb_source.table;
        if (tbl < 0 || tbl >= ARB_TABLES) comp->props.arb_source.table = 0;
    }
}

void component_free(Component *comp) {
    subcircuit_release_instance(comp);
    delay_line_release(comp);
    free(comp);
}

Component *component_clone(Component *comp) {
    if (!comp) return NULL;

    Component *clone = malloc(sizeof(Component));
    if (!clone) return NULL;

    memcpy(clone, comp, sizeof(Component));
    clone->id = next_component_id++;
    clone->selected = false;
    clone->highlighted = false;
    if (clone->type == COMP_DELAY_LINE) delay_line_alloc(clone);   /* its own history, not the original's */
    if (clone->type == COMP_SUBCIRCUIT) {
        /* the copy must not share the original's instance array, or both would free it */
        clone->props.subcircuit.inst_data = NULL;
        clone->props.subcircuit.inst_count = 0;
        clone->props.subcircuit.inst_def_id = 0;
    }

    // Clear node connections
    for (int i = 0; i < MAX_TERMINALS; i++) {
        clone->node_ids[i] = 0;
    }

    return clone;
}

void component_rotate(Component *comp) {
    if (comp) {
        comp->rotation = (comp->rotation + 90) % 360;
    }
}

void component_get_terminal_pos(Component *comp, int terminal_idx, float *x, float *y) {
    if (!comp || terminal_idx < 0 || terminal_idx >= comp->num_terminals) {
        *x = 0; *y = 0;
        return;
    }

    const ComponentTypeInfo *info = component_get_info(comp->type);
    float dx, dy;

    // Special handling for subcircuits - calculate from definition's pin positions
    if (comp->type == COMP_SUBCIRCUIT) {
        // Find the subcircuit definition
        SubCircuitDef *def = NULL;
        for (int i = 0; i < g_subcircuit_library.count; i++) {
            if (g_subcircuit_library.defs[i].id == comp->props.subcircuit.def_id) {
                def = &g_subcircuit_library.defs[i];
                break;
            }
        }

        if (def && terminal_idx < def->num_pins) {
            SubCircuitPin *pin = &def->pins[terminal_idx];
            // Use the definition's actual block size, not the default ComponentTypeInfo size
            float half_w = def->block_width / 2.0f;
            float half_h = def->block_height / 2.0f;

            switch (pin->side) {
                case 0:  // Left
                    dx = -half_w - 10;
                    dy = -half_h + 20 + pin->position * 20;
                    break;
                case 1:  // Right
                    dx = half_w + 10;
                    dy = -half_h + 20 + pin->position * 20;
                    break;
                case 2:  // Top
                    dx = -half_w + 20 + pin->position * 20;
                    dy = -half_h - 10;
                    break;
                case 3:  // Bottom
                    dx = -half_w + 20 + pin->position * 20;
                    dy = half_h + 10;
                    break;
                default:
                    dx = info->terminals[terminal_idx].dx;
                    dy = info->terminals[terminal_idx].dy;
                    break;
            }
        } else {
            // Fallback to default terminal positions
            dx = info->terminals[terminal_idx].dx;
            dy = info->terminals[terminal_idx].dy;
        }
    } else {
        dx = info->terminals[terminal_idx].dx;
        dy = info->terminals[terminal_idx].dy;
    }

    // Apply rotation using integer math for 90-degree increments
    // This avoids floating-point precision issues with cos/sin
    float rx, ry;
    int rot = ((comp->rotation % 360) + 360) % 360;  // Normalize to 0-359
    switch (rot) {
        case 0:
            rx = dx;
            ry = dy;
            break;
        case 90:
            rx = -dy;
            ry = dx;
            break;
        case 180:
            rx = -dx;
            ry = -dy;
            break;
        case 270:
            rx = dy;
            ry = -dx;
            break;
        default: {
            // Fallback to trig for non-90-degree rotations (shouldn't happen)
            double rad = comp->rotation * M_PI / 180.0;
            rx = dx * cos(rad) - dy * sin(rad);
            ry = dx * sin(rad) + dy * cos(rad);
            break;
        }
    }

    // Round to grid to ensure proper alignment
    *x = (float)snap_to_grid(comp->x + rx);
    *y = (float)snap_to_grid(comp->y + ry);
}

bool component_contains_point(Component *comp, float px, float py) {
    if (!comp) return false;

    const ComponentTypeInfo *info = component_get_info(comp->type);

    // Transform point to component local coordinates
    double rad = -comp->rotation * M_PI / 180.0;
    double cos_r = cos(rad);
    double sin_r = sin(rad);

    float dx = px - comp->x;
    float dy = py - comp->y;

    float local_x = dx * cos_r - dy * sin_r;
    float local_y = dx * sin_r + dy * cos_r;

    float half_w = info->width / 2 + 5;
    float half_h = info->height / 2 + 5;

    return (fabs(local_x) <= half_w && fabs(local_y) <= half_h);
}

int component_get_terminal_at(Component *comp, float px, float py, float threshold) {
    if (!comp) return -1;

    for (int i = 0; i < comp->num_terminals; i++) {
        float tx, ty;
        component_get_terminal_pos(comp, i, &tx, &ty);

        float dx = px - tx;
        float dy = py - ty;
        if (sqrt(dx*dx + dy*dy) <= threshold) {
            return i;
        }
    }

    return -1;
}

// Stamping helper macros
#define STAMP_CONDUCTANCE(n1, n2, g) do { \
    if ((n1) > 0) matrix_add(A, (n1)-1, (n1)-1, (g)); \
    if ((n2) > 0) matrix_add(A, (n2)-1, (n2)-1, (g)); \
    if ((n1) > 0 && (n2) > 0) { \
        matrix_add(A, (n1)-1, (n2)-1, -(g)); \
        matrix_add(A, (n2)-1, (n1)-1, -(g)); \
    } \
} while(0)

// Calculate sweep value at given time
double sweep_get_value(const SweepConfig *sweep, double base_value, double time) {
    if (!sweep || !sweep->enabled || sweep->sweep_time <= 0) {
        return base_value;
    }

    double progress;
    if (sweep->repeat) {
        // Repeating sweep
        if (sweep->bidirectional) {
            // Triangle pattern: 0->1->0->1->...
            double cycle_time = sweep->sweep_time * 2.0;
            double t_in_cycle = fmod(time, cycle_time);
            if (t_in_cycle < sweep->sweep_time) {
                progress = t_in_cycle / sweep->sweep_time;
            } else {
                progress = 1.0 - (t_in_cycle - sweep->sweep_time) / sweep->sweep_time;
            }
        } else {
            // Sawtooth pattern: 0->1, 0->1, ...
            progress = fmod(time, sweep->sweep_time) / sweep->sweep_time;
        }
    } else {
        // One-shot sweep
        if (time >= sweep->sweep_time) {
            if (sweep->bidirectional) {
                return base_value;  // Return to start
            }
            return sweep->end_value;  // Hold at end
        }
        progress = time / sweep->sweep_time;
        if (sweep->bidirectional && time >= sweep->sweep_time / 2.0) {
            progress = 1.0 - (time - sweep->sweep_time / 2.0) / (sweep->sweep_time / 2.0);
        }
    }

    double result;
    switch (sweep->mode) {
        case SWEEP_LINEAR:
            result = sweep->start_value + progress * (sweep->end_value - sweep->start_value);
            break;

        case SWEEP_LOG:
            // Logarithmic sweep (useful for frequency)
            if (sweep->start_value > 0 && sweep->end_value > 0) {
                double log_start = log10(sweep->start_value);
                double log_end = log10(sweep->end_value);
                result = pow(10.0, log_start + progress * (log_end - log_start));
            } else {
                result = sweep->start_value + progress * (sweep->end_value - sweep->start_value);
            }
            break;

        case SWEEP_STEP: {
            // Stepped discrete values
            int num_steps = (sweep->num_steps > 1) ? sweep->num_steps : 2;
            int step_idx = (int)(progress * num_steps);
            if (step_idx >= num_steps) step_idx = num_steps - 1;
            double step_size = (sweep->end_value - sweep->start_value) / (num_steps - 1);
            result = sweep->start_value + step_idx * step_size;
            break;
        }

        default:
            result = base_value;
            break;
    }

    return result;
}

// Stamp an op-amp output (VCVS auxiliary row) with rail saturation.
//   plus_idx / minus_idx / out_idx are matrix indices (0 = ground), volt_idx is the row of
//   the auxiliary current variable. Returns true if the output was stamped saturated.
/* Real op-amps roll off. The open-loop gain has one pole at f_p = GBW / A_OL, i.e.
       tau dVout/dt + Vout = A (V+ - V-),   tau = A / (2 pi GBW)
   Backward Euler puts that straight into the output row the VCVS already owns:
       (1 + tau/dt) Vout - A V+ + A V- = (tau/dt) Vout_prev
   so the closed-loop bandwidth (GBW / closed-loop gain) falls out of the solve instead of
   being imposed. Ideal mode (the default, and what every op-amp template uses) skips it, and
   so does the operating point, where the algebraic answer is the right starting condition.
   The slew-rate limit is applied after the solve, in simulation_clamp_opamps(). */
/* ------------------------------------------------------------------------------------
   Real op-amp: one-pole roll-off, slew rate and rails, all imposed inside the solve.

     tau dVout/dt + Vout = A (V+ - V-),   tau = A / (2 pi GBW)
   Backward Euler on the output row the VCVS already owns:
     (1 + tau/dt) Vout - A V+ + A V- = (tau/dt) Vout_prev

   The output can then only travel SR x dt per step and can never pass a rail, so instead of
   the algebraic saturation test (which assumes V_out = A verr instantly and therefore fires
   the moment the output lags its input at all) the limits are applied to the OUTPUT STATE:
   if the last Newton iterate wants to go past the window [prev - SR dt, prev + SR dt] clipped
   to the rails, the output is stamped as a source sitting on that edge. The choice latches for
   the rest of the solve so Newton cannot oscillate between the free and pinned stamps.

   Ideal mode - the default, and what every op-amp template uses - skips all of this and keeps
   the algebraic VCVS with its rail hysteresis. So does the operating point, where the
   algebraic answer is the right initial state for the integration. */
static bool opamp_stamp_dynamic(Component *comp, Matrix *A, Vector *b, Vector *prev_solution,
                                int plus_idx, int minus_idx, int out_idx, int volt_idx, double dt) {
    if (!comp || comp->props.opamp.ideal) return false;
    double gain = comp->props.opamp.gain, gbw = comp->props.opamp.gbw;
    if (gain <= 0 || gbw <= 0) return false;
    /* The input errors and the output resistance are there at DC as much as in the transient -
       an offset voltage is exactly what a DC amplifier gets wrong - so the operating point
       uses this path too. Only the pole and the slew limit are transient-only: neither means
       anything without a previous step. */
    bool trans = (g_stamp_prev_step != NULL) && dt > 0;

    if (out_idx > 0) {
        matrix_add(A, volt_idx, out_idx-1, 1.0);
        matrix_add(A, out_idx-1, volt_idx, 1.0);
    }

    double prev = comp->props.opamp.prev_output;
    double k = trans ? (gain / (2.0 * M_PI * gbw)) / dt : 0.0;    /* tau / dt */
    double sr = trans ? comp->props.opamp.slew_rate : 0.0;
    double hi = 1e30, lo = -1e30;
    if (sr > 0) { hi = prev + sr * 1e6 * dt; lo = prev - sr * 1e6 * dt; }
    double vmax = comp->props.opamp.vmax, vmin = comp->props.opamp.vmin;
    /* A part that is not rail-to-rail cannot drive its output all the way to the supply;
       1.5 V of headroom is the classic bipolar figure (a 741 on +/-15 V swings +/-13.5). */
    if (vmax > vmin && !comp->props.opamp.rail_to_rail) {
        double head = 1.5;
        if (vmax - vmin > 4 * head) { vmax -= head; vmin += head; }
    }
    if (vmax > vmin) { if (hi > vmax) hi = vmax; if (lo < vmin) lo = vmin; }

    /* Input-side error sources. These are what decide a DC amplifier's accuracy, and they are
       what the datasheet's front page is about:
         - offset voltage: the input pair is never perfectly matched, so the part behaves as if
           a small battery sat in series with one input;
         - bias current: real inputs draw current, which becomes a voltage error across
           whatever resistance the source presents (and cancels if both sides see the same);
         - CMRR: a common-mode voltage leaks through with gain A / 10^(CMRR_dB/20);
         - input resistance: a finite differential resistance loads the source. */
    double acm = (comp->props.opamp.cmrr > 0) ? gain / pow(10.0, comp->props.opamp.cmrr / 20.0) : 0.0;
    double ib = comp->props.opamp.i_bias;
    if (ib > 0) {
        if (plus_idx > 0)  vector_add(b, plus_idx-1,  -ib);
        if (minus_idx > 0) vector_add(b, minus_idx-1, -ib);
    }
    if (comp->props.opamp.r_in > 0 && comp->props.opamp.r_in < 1e15) {
        double g_in = 1.0 / comp->props.opamp.r_in;
        STAMP_CONDUCTANCE(plus_idx, minus_idx, g_in);
    }

    if (trans && !comp->slew_latch && prev_solution && out_idx > 0) {
        double vo_it = vector_get(prev_solution, out_idx-1);
        if (vo_it > hi) comp->slew_latch = 1;
        else if (vo_it < lo) comp->slew_latch = -1;
    }
    if (trans && comp->slew_latch) {
        vector_add(b, volt_idx, comp->slew_latch > 0 ? hi : lo);   /* on the limit, and no further */
        return true;
    }
    /* Free: the pole decides how far the output gets this step. The row is
         (1 + tau/dt) Vout - (A + Acm/2) V+ + (A - Acm/2) V- - r_out I = A Voffset + (tau/dt) Vprev
       which is Vout = A(Vd + Voffset) + Acm Vcm rolled off by the pole, driving the load
       through the output resistance. */
    if (plus_idx > 0)  matrix_add(A, volt_idx, plus_idx-1,  -(gain + acm / 2.0));
    if (minus_idx > 0) matrix_add(A, volt_idx, minus_idx-1,  (gain - acm / 2.0));
    if (out_idx > 0)   matrix_add(A, volt_idx, out_idx-1,    k);
    /* Output resistance: the aux variable is the current leaving the output node into the
       part, so a negative coefficient here makes the terminal droop as it sources current. */
    if (comp->props.opamp.r_out > 0) matrix_add(A, volt_idx, volt_idx, -comp->props.opamp.r_out);
    vector_add(b, volt_idx, k * prev + gain * comp->props.opamp.voffset);
    return true;
}

static bool opamp_stamp_output(Component *comp, Matrix *A, Vector *b, Vector *prev_solution,
                               int plus_idx, int minus_idx, int out_idx, int volt_idx,
                               double gain, double vmax, double vmin) {
    // Coupling between the auxiliary current variable and the output node
    if (out_idx > 0) {
        matrix_add(A, volt_idx, out_idx-1, 1.0);
        matrix_add(A, out_idx-1, volt_idx, 1.0);
    }

    bool saturated = false, forced_linear = false;
    double rail = 0.0;
    if (prev_solution && vmax > vmin) {
        double vp = (plus_idx > 0)  ? vector_get(prev_solution, plus_idx-1)  : 0.0;
        double vm = (minus_idx > 0) ? vector_get(prev_solution, minus_idx-1) : 0.0;
        double vo = (out_idx > 0)   ? vector_get(prev_solution, out_idx-1)   : 0.0;
        double predicted = gain * (vp - vm);
        // Hysteresis: once the previous iterate sits on a rail, stay there unless the
        // prediction comes back inside by a margin. Prevents Newton from 2-cycling
        // between the linear and saturated stamps right at the rail boundary.
        double margin = 0.01 * (vmax - vmin);
        bool at_max = (vo >= vmax - 1e-6), at_min = (vo <= vmin + 1e-6);
        if (predicted >= vmax || (at_max && predicted >= vmax - margin)) { saturated = true; rail = vmax; }
        else if (predicted <= vmin || (at_min && predicted <= vmin + margin)) { saturated = true; rail = vmin; }

        // A rail-to-rail flip-flop between Newton iterations means neither rail is a
        // consistent solution (e.g. a feedback loop whose true answer is in the linear
        // region). After two flips fall back to the linear stamp for this solve, which
        // Newton can then converge on.
        int rail_sign = saturated ? ((rail == vmax) ? 1 : -1) : 0;
        if (rail_sign != 0 && comp->sat_last_rail != 0 && rail_sign != comp->sat_last_rail) comp->sat_flips++;
        comp->sat_last_rail = rail_sign;
        if (comp->sat_flips >= 2) { saturated = false; forced_linear = true; }
    }

    if (saturated) {
        // Output pinned to the rail: V_out = rail (no dependence on the inputs)
        vector_add(b, volt_idx, rail);
    } else {
        /* The fall-back above drops the rail stamp, and a plain linear stamp has nothing left
           to keep the output inside the supplies: an opened feedback loop then reports gain
           times the input difference, which for a gain of 1e5 is megavolts out of a part
           running on 15. Lower the gain just enough to land on the rail instead. It stays
           linear, so Newton still has something it can converge on, and the answer stays
           somewhere the op-amp could actually go. Only the fall-back path is touched; a
           normally-behaved op-amp stamps its full gain as before. */
        double g_eff = gain;
        if (forced_linear && prev_solution && vmax > vmin) {
            double vp = (plus_idx > 0)  ? vector_get(prev_solution, plus_idx-1)  : 0.0;
            double vm = (minus_idx > 0) ? vector_get(prev_solution, minus_idx-1) : 0.0;
            double dv = vp - vm;
            if (dv > 1e-12 && gain * dv > vmax)      g_eff = vmax / dv;
            else if (dv < -1e-12 && gain * dv < vmin) g_eff = vmin / dv;
            if (g_eff < 1.0) g_eff = 1.0;
        }
        // Linear region: V_out - gain*V+ + gain*V- = 0
        if (plus_idx > 0)  matrix_add(A, volt_idx, plus_idx-1,  -g_eff);
        if (minus_idx > 0) matrix_add(A, volt_idx, minus_idx-1,  g_eff);
    }
    return saturated;
}

/* Newton limiting for the MOSFET linearisation point.

   A device with a large K - a power MOSFET is several amps per volt squared - can be thrown by
   one linear solve to a V_DS hundreds of volts from anything its load line allows. The square
   law then reports a current in the hundreds of amps, the next iteration answers with a bigger
   overshoot, and Newton settles on a point that satisfies no KCL at all (the drain of a 2N7000
   switching a 100 ohm load came out at -42 V). SPICE limits how far the linearisation point may
   move per iteration; this is the same idea in its simplest useful form. It cannot change where
   Newton converges - the fixed point is unchanged - only how it gets there. */
static double mos_limit(double vnew, double vold) {
    double step = 2.0 + 0.5 * fabs(vold);
    if (vnew > vold + step) return vold + step;
    if (vnew < vold - step) return vold - step;
    return vnew;
}

/* Capacitor branch companion: theta-method C, plus ESR / ESL / leakage when the part is
   not in ideal mode. Derivation (i = terminal 0 -> 1, K = (1-theta)/theta):

     v_C  = v_C,prev + (i + K i_prev) / Geq          theta-method on the capacitor
     v_L  = R_L (i - i_prev),  R_L = ESL/dt          backward Euler on the parasitic inductance
     v_br = v_C + i ESR + v_L
   =>  i = (v_br - E) / R_tot,   R_tot = 1/Geq + ESR + R_L,   E = v_C,prev + K i_prev/Geq - R_L i_prev

   At the operating point there is no history, so K = 0 and v_C,prev is the terminal voltage:
   the expression collapses to the plain companion. */
CapCompanion component_cap_companion(const Component *comp, double dt, bool trans, double v_prev) {
    CapCompanion cc = { 0, 0, 0, 0, 0 };
    if (!comp || dt <= 0) return cc;
    double C = (comp->type == COMP_CAPACITOR)   ? comp->props.capacitor.capacitance :
               (comp->type == COMP_TOROID)      ? toroid_capacitance(comp)
                                                : comp->props.capacitor_elec.capacitance;
    if (C <= 0) return cc;
    /* DC bias. A class-II ceramic (X5R, X7R) loses capacitance as the voltage across it rises -
       a 10 uF 6.3 V part can be down to a third of its marked value at 5 V, which is why a rail
       decoupled with "22 uF" can ripple like 6 uF. The one parameter is the bias at which it has
       halved. This is the effective capacitance at the operating point rather than a proper
       Q(V) integration, so it is a teaching model, not a charge-conserving one; class-I parts
       (C0G/NP0) leave v_half at 0 and are unaffected, as they are in life. */
    if (comp->type == COMP_CAPACITOR && !comp->props.capacitor.ideal && comp->props.capacitor.v_half > 0) {
        double vb = fabs(comp->cap_vc);
        C = C / (1.0 + vb / comp->props.capacitor.v_half);
    }
    const double THETA = 0.6;
    cc.Geq = trans ? C / (THETA * dt) : C / dt;
    cc.K   = trans ? (1.0 - THETA) / THETA : 0.0;

    double esr = 0, esl = 0, leak = 0;
    if (comp->type == COMP_CAPACITOR && !comp->props.capacitor.ideal) {
        esr = comp->props.capacitor.esr; esl = comp->props.capacitor.esl; leak = comp->props.capacitor.leakage;
    } else if (comp->type == COMP_CAPACITOR_ELEC && !comp->props.capacitor_elec.ideal) {
        esr = comp->props.capacitor_elec.esr; leak = comp->props.capacitor_elec.leakage;
    }
    if (esr < 0) esr = 0;
    if (esl < 0) esl = 0;
    /* Initial condition. At the operating point a capacitor that was given a starting voltage
       is stamped as a stiff source of that value, so the NODES come out consistent with it. .
       Seeding only the stored state would leave the first transient step with a companion that
       believes 24 V while the node sits at 0, and the branch current that implies is enormous. */
    double ic = (comp->type == COMP_CAPACITOR)      ? comp->props.capacitor.voltage :
                (comp->type == COMP_CAPACITOR_ELEC) ? comp->props.capacitor_elec.voltage : 0.0;
    if (!trans && ic != 0.0) {
        cc.G = 1000.0;                        /* 1 mohm: stiff enough to hold it, soft enough to solve */
        cc.Ieq = cc.G * ic;
        cc.Geq = cc.G; cc.K = 0; cc.G_leak = 0;
        return cc;
    }
    double R_L = trans ? esl / dt : 0.0;      /* a parasitic inductance has no operating-point drop */
    /* the state this step's solve used - see trap_i_solve in component.h */
    double st_i  = g_stamp_read_only ? comp->trap_i_solve : comp->trap_i_prev;
    double st_vc = g_stamp_read_only ? comp->cap_vc_solve : comp->cap_vc;
    double i_prev = trans ? st_i : 0.0;
    double vc_prev = trans ? st_vc : v_prev;

    double Rtot = 1.0 / cc.Geq + esr + R_L;
    double E = vc_prev + cc.K * i_prev / cc.Geq - R_L * i_prev;
    cc.G = 1.0 / Rtot;
    cc.Ieq = cc.G * E;
    cc.G_leak = (leak > 0) ? 1.0 / leak : 0.0;
    return cc;
}

/* A junction capacitance across a diode, integrated trapezoidally like any other capacitor.
   Every diode in this program has carried a cjo in its properties - 1 pF for a signal diode,
   5 pF for a Schottky, 50 pF for a varactor - and nothing has ever read it, so a reverse-biased
   junction carried no displacement current at all. On a 60 Hz rectifier that is invisible; on a
   switching node moving volts per nanosecond it is microamps that no terminal reported and that
   the flow display then had to spread across the net.

   The capacitance is taken at zero bias rather than as the usual Cjo/(1 - V/Vj)^m: the bias
   dependence matters for a varactor's tuning and not for the charge that has to be accounted
   for here, and a constant is one thing to be right about rather than three. */
/* A conductance from a resistance, with a floor.

   Dividing straight by a resistance is how a zero a user typed reaches the matrix as an infinity,
   and one infinity makes every node in the circuit come back NaN - which the scope then draws and
   the measurements panel then prints. --stress-test found it on the analog switch, where an
   on-resistance of zero is a perfectly reasonable thing to ask for and produced "NaN at node 1"
   across five templates.

   1e-9 ohm is the floor the ammeter stamp already used for the same reason, kept here so there is
   one number rather than two. The condition is written so that a NaN or a negative also lands on
   the floor rather than sailing through. */
static double conductance_of(double R) {
    if (!(R > 1e-9)) R = 1e-9;
    return 1.0 / R;
}

static void stamp_junction_cap(Component *comp, Matrix *A, Vector *b, const int *n,
                               double cjo, double dt) {
    (void)comp;
    if (cjo <= 0 || dt <= 0 || !g_stamp_prev_step) return;   /* DC: a capacitor is an open */
    /* Backward Euler, and deliberately so. A trapezoidal companion needs the branch current from
       the last accepted step as well as its voltage, and that is state - state which is advanced
       once per step for the capacitors the solver knows about and would be advanced once per
       Newton iteration if it lived in here. One order of accuracy is a fair price for a term
       that exists to account for picoamp-scale charge rather than to shape a waveform. */
    double a_ = (n[0] > 0) ? vector_get(g_stamp_prev_step, n[0] - 1) : 0;
    double k_ = (n[1] > 0) ? vector_get(g_stamp_prev_step, n[1] - 1) : 0;
    double v_prev = a_ - k_;
    double G = cjo / dt;
    double Ieq = G * v_prev;
    STAMP_CONDUCTANCE(n[0], n[1], G);
    /* +Ieq at the anode, the same way an ordinary capacitor and the crystal's holder
       capacitance are stamped. Written the other way round the companion does not remember the
       charge, it injects it: the branch carries C(v + v_prev)/dt instead of C(v - v_prev)/dt.
       KCL still closes, because the residual only has to agree with whatever was stamped - it is
       the waveforms that are wrong, and the Function Generator's shaper said so. */
    if (n[0] > 0) vector_add(b, n[0] - 1, Ieq);
    if (n[1] > 0) vector_add(b, n[1] - 1, -Ieq);
}

void component_stamp(Component *comp, Matrix *A, Vector *b,
                     int *node_map, int num_nodes,
                     double time, Vector *prev_solution, double dt) {
    if (!comp || !A || !b || !node_map) return;

    // Get node indices
    int n[MAX_TERMINALS];
    for (int i = 0; i < comp->num_terminals; i++) {
        n[i] = (comp->node_ids[i] > 0) ? node_map[comp->node_ids[i]] : 0;
    }

    switch (comp->type) {
        case COMP_GROUND: {
            // Ground forces node to 0V
            if (n[0] > 0) {
                double g_large = 1e10;
                matrix_add(A, n[0]-1, n[0]-1, g_large);
            }
            break;
        }

        case COMP_DC_VOLTAGE: {
            double V = comp->props.dc_voltage.voltage;
            // Apply voltage sweep if enabled
            V = sweep_get_value(&comp->props.dc_voltage.voltage_sweep, V, time);
            int volt_idx = num_nodes + comp->voltage_var_idx;

            // Voltage source stamp
            if (n[0] > 0) {
                matrix_add(A, volt_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, volt_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, volt_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, volt_idx, -1);
            }
            /* Real sources have an internal resistance: v0 - v1 - R i = V, so the terminal
               voltage sags with the load. Ideal mode (the default) leaves it out. */
            if (!comp->props.dc_voltage.ideal && comp->props.dc_voltage.r_series > 0)
                matrix_add(A, volt_idx, volt_idx, -comp->props.dc_voltage.r_series);
            vector_add(b, volt_idx, V);
            break;
        }

        case COMP_ARB_SOURCE: {
            /* replay a sample table: one pass per period, linearly interpolated and wrapped */
            double per = comp->props.arb_source.period > 1e-12 ? comp->props.arb_source.period : 1e-3;
            double ph = time / per;
            double V = comp->props.arb_source.amplitude * arb_table_value(comp->props.arb_source.table, ph)
                     + comp->props.arb_source.offset;
            int vi = num_nodes + comp->voltage_var_idx;
            if (n[0] > 0) { matrix_add(A, vi, n[0]-1, 1); matrix_add(A, n[0]-1, vi, 1); }
            if (n[1] > 0) { matrix_add(A, vi, n[1]-1, -1); matrix_add(A, n[1]-1, vi, -1); }
            vector_add(b, vi, V);
            break;
        }

        case COMP_AC_VOLTAGE: {
            double amp = comp->props.ac_voltage.amplitude;
            double freq = comp->props.ac_voltage.frequency;
            double phase = comp->props.ac_voltage.phase * M_PI / 180.0;
            double offset = comp->props.ac_voltage.offset;

            // Apply amplitude and frequency sweeps if enabled
            amp = sweep_get_value(&comp->props.ac_voltage.amplitude_sweep, amp, time);
            freq = sweep_get_value(&comp->props.ac_voltage.frequency_sweep, freq, time);

            // A swept frequency must be integrated into phase (phi += 2*pi*f*dt per step, see
            // simulation_step); sin(2*pi*f(t)*t) would chirp at f + t*df/dt instead of f.
            double V;
            if (comp->props.ac_voltage.frequency_sweep.enabled)
                V = amp * sin(comp->sweep_phase + phase) + offset;
            else
                V = amp * sin(2 * M_PI * freq * time + phase) + offset;
            int volt_idx = num_nodes + comp->voltage_var_idx;

            if (n[0] > 0) {
                matrix_add(A, volt_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, volt_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, volt_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, volt_idx, -1);
            }
            if (!comp->props.ac_voltage.ideal && comp->props.ac_voltage.r_series > 0)
                matrix_add(A, volt_idx, volt_idx, -comp->props.ac_voltage.r_series);
            vector_add(b, volt_idx, V);
            break;
        }

        case COMP_DC_CURRENT: {
            double I = comp->props.dc_current.current;
            // Apply current sweep if enabled
            I = sweep_get_value(&comp->props.dc_current.current_sweep, I, time);
            /* A real current source is a Norton pair: the shunt resistance carries some of the
               current once the compliance voltage rises. Ideal mode (the default) omits it. */
            if (!comp->props.dc_current.ideal && comp->props.dc_current.r_parallel > 0)
                STAMP_CONDUCTANCE(n[0], n[1], conductance_of(comp->props.dc_current.r_parallel));
            if (n[0] > 0) vector_add(b, n[0]-1, -I);
            if (n[1] > 0) vector_add(b, n[1]-1, I);
            break;
        }

        case COMP_RESISTOR: {
            double R_base = comp->props.resistor.resistance;
            double R = R_base;

            // Apply temperature coefficient only in non-ideal mode
            // R(T) = R_base * (1 + alpha * (T - T_ref))
            // where alpha = temp_coeff / 1e6 (ppm to fraction), T_ref = 25°C
            if (!comp->props.resistor.ideal) {
                double alpha = comp->props.resistor.temp_coeff / 1e6;  // ppm/°C to fraction
                double dT = g_environment.temperature - 25.0;  // Delta from reference temp
                R = R_base * (1.0 + alpha * dT);
            }

            if (R < 0.001) R = 0.001;  // Minimum resistance to avoid divide by zero
            double G = 1.0 / R;
            STAMP_CONDUCTANCE(n[0], n[1], G);
            break;
        }

        case COMP_CAPACITOR:
        case COMP_CAPACITOR_ELEC:
        case COMP_TOROID: {
            // Backward Euler companion model for capacitor:
            // i_C = C * dv/dt ≈ C * (v - v_prev) / dt = Geq * v - Ieq
            // where Geq = C/dt and Ieq = C * v_prev / dt
            // (a toroid is a one-terminal capacitor to ground: n[1] is forced to 0)
            double C = (comp->type == COMP_CAPACITOR) ? comp->props.capacitor.capacitance :
                       (comp->type == COMP_TOROID) ? toroid_capacitance(comp) :
                       comp->props.capacitor_elec.capacitance;
            if (comp->type == COMP_TOROID) n[1] = 0;
            // Trapezoidal companion (SPICE default): i = (2C/dt)(v - v_prev) - i_prev.
            // Half the numerical damping of backward Euler, which matters for oscillators.
            // Backward Euler (Geq = C/dt, Ieq = C v_prev/dt) is kept for the DC operating point,
            // where there is no previous step.
            // theta-method: theta = 0.5 is pure trapezoidal (undamped, rings at Nyquist after a
            // slope discontinuity), theta = 1 is backward Euler (heavily damped). 0.6 kills the
            // ringing within a few steps while keeping oscillators alive at coarse dt.
            bool trap = (g_stamp_prev_step != NULL);
            Vector *mem = g_stamp_prev_step ? g_stamp_prev_step : prev_solution;
            double v_prev = 0;
            if (mem) {
                double v1 = (n[0] > 0) ? vector_get(mem, n[0]-1) : 0;
                double v2 = (n[1] > 0) ? vector_get(mem, n[1]-1) : 0;
                v_prev = v1 - v2;
            }
            /* ESR / ESL / leakage are folded in here; in ideal mode (the default) this is the
               plain theta-method companion. (void)C keeps the shared type dispatch above. */
            (void)C;
            CapCompanion cc = component_cap_companion(comp, dt, trap && mem, v_prev);
            if (!mem) cc.Ieq = 0;

            STAMP_CONDUCTANCE(n[0], n[1], cc.G);
            if (cc.G_leak > 0) STAMP_CONDUCTANCE(n[0], n[1], cc.G_leak);
            // Ieq represents the capacitor's "memory" current
            // Positive Ieq means capacitor was charged (n1 > n2), so it sources current at n1
            if (n[0] > 0) vector_add(b, n[0]-1, cc.Ieq);
            if (n[1] > 0) vector_add(b, n[1]-1, -cc.Ieq);
            break;
        }

        case COMP_INDUCTOR: {
            double L = comp->props.inductor.inductance;
            int curr_idx = num_nodes + comp->voltage_var_idx;
            // theta method (theta = 0.6, like the capacitor): backward Euler alone damps high-Q
            // LC resonators by ~(w dt)^2/2 per step, which killed crystal-class circuits.
            //   theta V_n + (1-theta) V_prev = L (I_n - I_prev)/dt
            //   -> V_n - (L/(theta dt)) I_n = -K V_prev - (L/(theta dt)) I_prev,  K = (1-theta)/theta
            const double TH = 0.6, K = (1.0 - TH) / TH;
            Vector *mem = g_stamp_prev_step ? g_stamp_prev_step : prev_solution;
            bool trans = (mem != NULL) && curr_idx < (int)mem->size;
            double Req = trans ? L / (TH * dt) : L / dt;
            /* Winding resistance. Without it an inductor is lossless, so any L-C loop it sits in
               rings for ever - which is what made the switching converters run away. The equation
               becomes V = L di/dt + R_dcr i, i.e. one more term on the current variable. */
            double Rdcr = comp->props.inductor.ideal ? 0.0 : comp->props.inductor.dcr;
            if (Rdcr < 0) Rdcr = 0;
            double Veq = 0;
            if (trans) {
                double Iprev = vector_get(mem, curr_idx);
                double v0p = (n[0] > 0) ? vector_get(mem, n[0]-1) : 0, v1p = (n[1] > 0) ? vector_get(mem, n[1]-1) : 0;
                /* The theta method integrates the INDUCTIVE voltage, so the resistive part of
                   last step's terminal voltage has to come off first. Leaving it in adds a
                   spurious +K R I_prev to the row, which cancels part of the DCR: a branch set
                   to zeta = 0.30 then rings as if it were 0.21. */
                Veq = -K * ((v0p - v1p) - Rdcr * Iprev) - Req * Iprev;
            }

            if (n[0] > 0) {
                matrix_add(A, curr_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, curr_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, curr_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, curr_idx, -1);
            }
            matrix_add(A, curr_idx, curr_idx, -(Req + Rdcr));   /* the history term above uses the inductive part only */
            vector_add(b, curr_idx, Veq);
            break;
        }

        case COMP_DIODE: {
            if (comp->props.diode.ideal) {
                /* Textbook "ideal diode with a 0.7 V drop": a switch in series with a battery.
                   Off it is a 1 MOhm leak, on it is 0.7 V behind 1 ohm - no exponential knee,
                   which is exactly the difference the Ideal vs Real Diode template shows. */
                const double Vf = 0.7, Ron = 1.0, Roff = 1e6;
                double Vd_i = 0;
                if (prev_solution) {
                    double a_ = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                    double k_ = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                    Vd_i = a_ - k_;
                }
                double Gi, Ii;
                if (Vd_i > Vf) { Gi = 1.0 / Ron; Ii = -Vf / Ron; }   /* i = (Vd - Vf)/Ron */
                else           { Gi = 1.0 / Roff; Ii = 0; }          /* i = Vd / Roff */
                STAMP_CONDUCTANCE(n[0], n[1], Gi);
                if (n[0] > 0) vector_add(b, n[0]-1, -Ii);
                if (n[1] > 0) vector_add(b, n[1]-1, Ii);
                break;
            }
            double Is = comp->props.diode.is;
            // Calculate thermal voltage from global environment temperature
            // Vt = k*T/q where k/q = 8.617e-5 V/K
            double Vt = 8.617e-5 * (g_environment.temperature + 273.15);
            double nn = comp->props.diode.n;
            double nVt = nn * Vt;

            double Vd = 0.6;
            double v1_raw = 0, v2_raw = 0;
            if (prev_solution) {
                v1_raw = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                v2_raw = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                double Vd_raw = v1_raw - v2_raw;
                // Use wider clamp range for reverse bias to allow proper blocking
                // Forward: limit to 40*nVt (~1V) to prevent overflow
                // Reverse: allow up to -100V for proper blocking behavior
                Vd = CLAMP(Vd_raw, -100.0, 40*nVt);
            }

            double expTerm = exp(Vd / nVt);
            double Id = Is * (expTerm - 1);
            // Gd is conductance - add minimum conductance to prevent singularities
            double Gd = (Is / nVt) * expTerm;
            if (Gd < 1e-12) Gd = 1e-12;  // Minimum conductance for stability
            double Ieq = Id - Gd * Vd;

            STAMP_CONDUCTANCE(n[0], n[1], Gd);
            if (n[0] > 0) vector_add(b, n[0]-1, -Ieq);
            if (n[1] > 0) vector_add(b, n[1]-1, Ieq);
            stamp_junction_cap(comp, A, b, n, comp->props.diode.cjo, dt);
            break;
        }

        case COMP_ZENER: {
            // Zener diode - bidirectional conduction
            double Is = comp->props.zener.is;
            // Calculate thermal voltage from global environment temperature
            double Vt = 8.617e-5 * (g_environment.temperature + 273.15);
            double nn = comp->props.zener.n;
            double Vz = comp->props.zener.vz;
            double nVt = nn * Vt;

            double Vd = 0.6;
            if (prev_solution) {
                double v1 = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double v2 = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                Vd = v1 - v2;
            }

            // Smooth model: forward Shockley diode plus an exponential reverse-breakdown
            // branch centred on -Vz. Both branches are continuous, so Newton-Raphson
            // converges instead of chattering between "off" and a 1 S short.
            //   I(Vd) = Is*(exp(Vd/nVt) - 1) - Iz0*exp(-(Vd + Vz)/Vs)
            // Iz0 = 1 mA is the knee current at Vd = -Vz; Vs sets the knee sharpness so the
            // dynamic resistance near a few mA is about Rz (ideal mode: very sharp knee).
            const double Iz0 = 1e-3;
            double Rz = comp->props.zener.rz;
            double Vs = comp->props.zener.ideal ? 0.002 : CLAMP(Rz * 5e-3, 0.005, 0.5);

            Vd = CLAMP(Vd, -(Vz + 20.0 * Vs), 40.0 * nVt);   // e^20*Iz0 ~ 0.5 A keeps Gd sane
            double expF = exp(Vd / nVt);
            double expB = exp(-(Vd + Vz) / Vs);
            double Id = Is * (expF - 1.0) - Iz0 * expB;
            double Gd = (Is / nVt) * expF + (Iz0 / Vs) * expB + 1e-12;
            double Ieq = Id - Gd * Vd;
            STAMP_CONDUCTANCE(n[0], n[1], Gd);
            if (n[0] > 0) vector_add(b, n[0]-1, -Ieq);
            if (n[1] > 0) vector_add(b, n[1]-1, Ieq);
            break;
        }

        case COMP_SCHOTTKY:
        case COMP_LED: {
            // Similar to regular diode but with different parameters
            double Is, nn;
            if (comp->type == COMP_SCHOTTKY) {
                Is = comp->props.schottky.is;
                nn = comp->props.schottky.n;
            } else {
                Is = comp->props.led.is;
                nn = comp->props.led.n;
            }
            // Calculate thermal voltage from global environment temperature
            double Vt = 8.617e-5 * (g_environment.temperature + 273.15);
            double nVt = nn * Vt;

            double Vd = 0.6;
            if (prev_solution) {
                double v1 = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double v2 = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                Vd = CLAMP(v1 - v2, -5*nVt, 40*nVt);
            }

            double expTerm = exp(Vd / nVt);
            double Id = Is * (expTerm - 1);
            double Gd = (Is / nVt) * expTerm + 1e-12;
            double Ieq = Id - Gd * Vd;

            // Store LED current for glow rendering
            if (comp->type == COMP_LED) {
                comp->props.led.current = Id > 0 ? Id : 0;
            }

            STAMP_CONDUCTANCE(n[0], n[1], Gd);
            if (n[0] > 0) vector_add(b, n[0]-1, -Ieq);
            if (n[1] > 0) vector_add(b, n[1]-1, Ieq);
            /* A Schottky's junction capacitance is five times a signal diode's and it is the
               part that sits across a switching node, which is where it matters. An LED has no
               cjo of its own to read. */
            if (comp->type == COMP_SCHOTTKY)
                stamp_junction_cap(comp, A, b, n, comp->props.schottky.cjo, dt);
            break;
        }

        // BJT transistor stamps (Gummel-Poon model)
        case COMP_NPN_BJT:
        case COMP_PNP_BJT: {
            double bf = comp->props.bjt.bf;      // Forward beta
            double Is = comp->props.bjt.is;      // Saturation current
            double Vaf = comp->props.bjt.vaf;    // Early voltage
            double nf = comp->props.bjt.nf;      // Emission coefficient
            bool ideal = comp->props.bjt.ideal;

            // Calculate thermal voltage from global environment temperature
            // Vt = k*T/q where k/q = 8.617e-5 V/K, T must be in Kelvin
            double Vt = 8.617e-5 * (g_environment.temperature + 273.15);

            // For PNP, invert voltage polarities
            double sign = (comp->type == COMP_PNP_BJT) ? -1.0 : 1.0;

            double Vbe = 0.6 * sign;
            double Vbc = 0.0;
            /* The junction voltages above are clamped to keep exp() finite, which caps Vbc at
               about -0.13 V. The Early factor needs the REAL collector-emitter voltage, so it is
               taken straight from the node voltages - reading it back out of the clamped Vbc
               would cap V_CE near 0.8 V and shrink the effect to a tenth of its size. */
            double Vce_real = 0.2 * sign;
            if (prev_solution) {
                double vB = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double vC = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                double vE = (n[2] > 0) ? vector_get(prev_solution, n[2]-1) : 0;
                Vbe = sign * (vB - vE);
                Vbc = sign * (vB - vC);
                Vce_real = sign * (vC - vE);
                Vbe = CLAMP(Vbe, -5*nf*Vt, 40*nf*Vt);
                Vbc = CLAMP(Vbc, -5*nf*Vt, 40*nf*Vt);
            }

            double Gbe, Gbc, Gm, Gmr, Ieq_be, Ieq_bc, Ieq_c;

            if (ideal) {
                // Ideal Ebers-Moll model (simplified)
                double expBE = exp(Vbe / (nf * Vt));
                double Ibe = (Is / bf) * (expBE - 1);
                Gbe = (Is / (bf * nf * Vt)) * expBE + 1e-12;
                Ieq_be = Ibe - Gbe * Vbe;

                // Base-collector junction (reverse diode, beta_R = br): without it a saturated
                // transistor keeps sinking beta_F x I_B and drags its collector below ground.
                double br = comp->props.bjt.br > 0 ? comp->props.bjt.br : 1.0;
                double expBC = exp(Vbc / (nf * Vt));
                double Ibc = (Is / br) * (expBC - 1);
                Gbc = (Is / (br * nf * Vt)) * expBC + 1e-12;
                Ieq_bc = Ibc - Gbc * Vbc;

                // Collector current (Ebers-Moll transport): forward minus reverse
                double Ic = Is * (expBE - 1) - Is * (expBC - 1);
                Gm = (Is / (nf * Vt)) * expBE;
                Gmr = -(Is / (nf * Vt)) * expBC;
                Ieq_c = Ic - Gm * Vbe - Gmr * Vbc;
            } else {
                // Non-ideal Gummel-Poon model with Early effect
                double br = comp->props.bjt.br;
                double nr = comp->props.bjt.nr;
                double ise = comp->props.bjt.ise;
                double isc = comp->props.bjt.isc;

                // Forward B-E diode
                double expBE = exp(Vbe / (nf * Vt));
                double Ibe_main = (Is / bf) * (expBE - 1);
                double Ibe_leak = ise * (exp(Vbe / (2 * nf * Vt)) - 1);  // Low-level injection
                double Ibe = Ibe_main + Ibe_leak;
                Gbe = (Is / (bf * nf * Vt)) * expBE + 1e-12;
                Ieq_be = Ibe - Gbe * Vbe;

                // Reverse B-C diode
                double expBC = exp(Vbc / (nr * Vt));
                double Ibc_main = (Is / br) * (expBC - 1);
                double Ibc_leak = isc * (exp(Vbc / (2 * nr * Vt)) - 1);
                double Ibc = Ibc_main + Ibc_leak;
                Gbc = (Is / (br * nr * Vt)) * expBC + 1e-12;
                Ieq_bc = Ibc - Gbc * Vbc;

                // Collector current with Early effect
                double early_factor = 1.0;
                if (Vaf > 0) {
                    early_factor = 1.0 + Vce_real / Vaf;
                    if (early_factor < 0.1) early_factor = 0.1;   /* never let it change sign */
                }
                double Ic_f = Is * (expBE - 1) * early_factor;
                double Ic_r = Is * (expBC - 1);
                double Ic = Ic_f - Ic_r;
                Gm = (Is / (nf * Vt)) * expBE * early_factor;       // dIc/dVbe
                Gmr = -(Is / (nr * Vt)) * expBC;                     // dIc/dVbc
                Ieq_c = Ic - Gm * Vbe - Gmr * Vbc;
            }

            // Apply sign for PNP. Conductances/transconductances are the same for both
            // polarities (the matrix entries are derivatives w.r.t. node voltages); only
            // the Newton equivalent current sources flip with the device polarity.
            Ieq_be *= sign;
            Ieq_bc *= sign;
            Ieq_c *= sign;
            // Stamp B-E junction
            STAMP_CONDUCTANCE(n[0], n[2], Gbe);
            if (n[0] > 0) vector_add(b, n[0]-1, -Ieq_be);
            if (n[2] > 0) vector_add(b, n[2]-1, Ieq_be);

            // Stamp B-C junction (both modes: it is what makes saturation clamp V_CE)
            {
                STAMP_CONDUCTANCE(n[0], n[1], Gbc);
                if (n[0] > 0) vector_add(b, n[0]-1, -Ieq_bc);
                if (n[1] > 0) vector_add(b, n[1]-1, Ieq_bc);
            }

            // Collector current Ic = Gm*Vbe + Gmr*Vbc + Ieq_c flows C -> E through the device.
            // Transconductance (collector current controlled by Vbe)
            if (n[1] > 0 && n[0] > 0) matrix_add(A, n[1]-1, n[0]-1, Gm);
            if (n[1] > 0 && n[2] > 0) matrix_add(A, n[1]-1, n[2]-1, -Gm);
            if (n[2] > 0 && n[0] > 0) matrix_add(A, n[2]-1, n[0]-1, -Gm);
            if (n[2] > 0)             matrix_add(A, n[2]-1, n[2]-1, Gm);
            // Reverse transconductance (Vbc dependence, non-ideal mode only)
            if (Gmr != 0) {
                if (n[1] > 0 && n[0] > 0) matrix_add(A, n[1]-1, n[0]-1, Gmr);
                if (n[1] > 0)             matrix_add(A, n[1]-1, n[1]-1, -Gmr);
                if (n[2] > 0 && n[0] > 0) matrix_add(A, n[2]-1, n[0]-1, -Gmr);
                if (n[2] > 0 && n[1] > 0) matrix_add(A, n[2]-1, n[1]-1, Gmr);
            }
            // Newton equivalent current source for the collector current
            if (n[1] > 0) vector_add(b, n[1]-1, -Ieq_c);
            if (n[2] > 0) vector_add(b, n[2]-1, Ieq_c);
            break;
        }

        // MOSFET transistor stamps (Level 1 SPICE model)
        case COMP_NMOS:
        case COMP_PMOS: {
            double Vth = comp->props.mosfet.vth;
            double Kp = comp->props.mosfet.kp;
            double lambda = comp->props.mosfet.lambda;
            double W = comp->props.mosfet.w;
            double L = comp->props.mosfet.l;
            bool ideal = comp->props.mosfet.ideal;

            // Temperature effects (non-ideal mode)
            // Reference temperature is 25°C (298.15K)
            if (!ideal) {
                double T = g_environment.temperature + 273.15;  // Current temp in Kelvin
                double T0 = 298.15;  // Reference temp (25°C) in Kelvin
                double dT_C = g_environment.temperature - 25.0;  // Delta in Celsius

                // Vth decreases ~2mV/°C (typical for silicon MOSFETs)
                Vth = Vth - 0.002 * dT_C;

                // Mobility decreases with temperature: Kp(T) = Kp(T0) * (T0/T)^1.5
                Kp = Kp * pow(T0 / T, 1.5);
            }

            // Effective transconductance: K = Kp * W / L
            double K = Kp * (W / L);

            // For PMOS, work with absolute values and invert at end
            double sign = (comp->type == COMP_PMOS) ? -1.0 : 1.0;
            double Vth_eff = fabs(Vth);

            double Vgs = 0, Vds = 0, Vsb = 0, Vds_terminal = 0;
            if (prev_solution) {
                double vG = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double vD = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                double vS = (n[2] > 0) ? vector_get(prev_solution, n[2]-1) : 0;

                if (comp->type == COMP_PMOS) {
                    // For PMOS: Vsg, Vsd (source and drain swapped in equations)
                    Vgs = vS - vG;
                    Vds = vS - vD;
                    Vsb = 0;  // Assume body tied to source
                } else {
                    Vgs = vG - vS;
                    Vds = vD - vS;
                    Vsb = 0;
                }
                /* Limit only V_DS (see mos_limit). V_GS is almost always held by a source, and
                   limiting it stalls the solve instead: the device stays in cutoff while the
                   node voltages sit still, and the convergence test - which watches the node
                   voltages - calls that converged.
                   The limiter remembers its own point: op_vds is what the properties panel
                   shows, and it has to be the real terminal voltage. Feeding the panel the
                   linearisation point instead made a device sitting in cutoff read 5 V when its
                   drain was actually at 10 - the solve stops as soon as the nodes stop moving,
                   which can be before the limiter has walked all the way out. */
                Vds_terminal = Vds;
                Vds = mos_limit(Vds, comp->mos_vds_lin);
                comp->mos_vds_lin = Vds;
            }

            // Body effect (non-ideal mode only)
            double Vth_adj = Vth_eff;
            if (!ideal && Vsb > 0) {
                double gamma = comp->props.mosfet.gamma;
                double phi = comp->props.mosfet.phi;
                Vth_adj = Vth_eff + gamma * (sqrt(phi + Vsb) - sqrt(phi));
            }

            double Gds = 1e-12;  // Minimum conductance
            double Gm = 0;
            double Id = 0;
            double Ieq = 0;

            double Vov = Vgs - Vth_adj;  // Overdrive voltage

            if (Vov <= 0) {
                // Cutoff region
                Gds = 1e-12;
                Gm = 0;
                Id = 0;
            } else if (Vds < Vov) {
                // Triode (linear) region
                // Id = K * (Vov * Vds - Vds²/2) * (1 + lambda * Vds)
                double lambda_term = ideal ? 1.0 : (1.0 + lambda * Vds);
                Id = K * (Vov * Vds - 0.5 * Vds * Vds) * lambda_term;

                // Derivatives for Newton-Raphson linearization
                Gm = K * Vds * lambda_term;  // dId/dVgs
                Gds = K * (Vov - Vds) * lambda_term;  // dId/dVds
                if (!ideal) {
                    Gds += K * (Vov * Vds - 0.5 * Vds * Vds) * lambda;
                }
            } else {
                // Saturation region
                // Id = (K/2) * Vov² * (1 + lambda * Vds)
                double lambda_term = ideal ? 1.0 : (1.0 + lambda * Vds);
                Id = 0.5 * K * Vov * Vov * lambda_term;

                // Derivatives
                Gm = K * Vov * lambda_term;  // dId/dVgs
                Gds = ideal ? 1e-12 : (0.5 * K * Vov * Vov * lambda);  // dId/dVds (channel length modulation)
            }

            // Ensure minimum conductance
            Gds = MAX(Gds, 1e-12);
            Gm = MAX(Gm, 0);

            // Cache the operating point for the properties panel (Vov <= 0 cutoff, Vds < Vov triode)
            comp->props.mosfet.op_vgs = Vgs; comp->props.mosfet.op_vds = Vds_terminal;
            comp->props.mosfet.op_id = Id;   comp->props.mosfet.op_gm = Gm;
            comp->props.mosfet.op_region = (Vov <= 0) ? 0 : (Vds < Vov ? 1 : 2);

            // Equivalent current source: Ieq = Id - Gm*Vgs - Gds*Vds
            Ieq = Id - Gm * Vgs - Gds * Vds;

            // Apply sign for PMOS: the drain current flows S->D instead of D->S, which
            // flips the Newton equivalent current source. The Gm/Gds matrix entries are
            // derivatives w.r.t. node voltages and are identical for both polarities
            // (Vgs/Vds were already taken with PMOS sign above).
            (void)sign;
            Ieq *= (comp->type == COMP_PMOS) ? -1.0 : 1.0;

            // Stamp D-S conductance
            STAMP_CONDUCTANCE(n[1], n[2], Gds);

            // Stamp equivalent current source
            if (n[1] > 0) vector_add(b, n[1]-1, -Ieq);
            if (n[2] > 0) vector_add(b, n[2]-1, Ieq);

            // Transconductance (drain current controlled by Vgs)
            if (n[1] > 0 && n[0] > 0) matrix_add(A, n[1]-1, n[0]-1, Gm);
            if (n[1] > 0 && n[2] > 0) matrix_add(A, n[1]-1, n[2]-1, -Gm);
            if (n[2] > 0 && n[0] > 0) matrix_add(A, n[2]-1, n[0]-1, -Gm);
            if (n[2] > 0 && n[2] > 0) matrix_add(A, n[2]-1, n[2]-1, Gm);

            // Gate capacitance model (non-ideal mode only)
            if (!ideal && dt > 0) {
                double cgso = comp->props.mosfet.cgso;
                double cgdo = comp->props.mosfet.cgdo;
                double tox = comp->props.mosfet.tox;

                // Oxide capacitance per unit area (epsilon_ox ≈ 3.9 * epsilon_0)
                const double epsilon_ox = 3.9 * 8.854e-12;  // F/m
                double Cox = epsilon_ox / tox;  // F/m²

                // Total gate capacitances depend on operating region
                double Cgs, Cgd;

                if (Vov <= 0) {
                    // Cutoff: only overlap capacitances
                    Cgs = cgso * W;
                    Cgd = cgdo * W;
                } else if (Vds < Vov) {
                    // Triode: channel capacitance distributed between G-S and G-D
                    double Cch = Cox * W * L;  // Total channel capacitance
                    double x = Vds / Vov;
                    // Meyer's model approximation
                    Cgs = cgso * W + Cch * (1.0 - (x*x - x + 1.0) / (3.0 * (1.0 - x + 1e-12)));
                    Cgd = cgdo * W + Cch * (1.0 - (1.0 - x + x*x) / (3.0 * (1.0 - x + 1e-12)));
                } else {
                    // Saturation: most channel capacitance goes to G-S
                    double Cch = Cox * W * L;
                    Cgs = cgso * W + (2.0/3.0) * Cch;
                    Cgd = cgdo * W;  // Only overlap in saturation
                }

                /* Backward Euler against the previous accepted step, the same companion the
                   diode's junction capacitance uses, and for the same reason.

                   This was a trapezoidal companion carrying its own state on the component -
                   vgs_prev, i_cgs - and updating it inside the stamp. A stamp runs once per Newton
                   iteration, not once per accepted step, so "the previous timestep" became "the
                   previous iterate": as Newton converged, vgs_prev approached Vgs_curr and the
                   recurrence I_eq = 2 G V(k-1) - I_eq(k-1) alternated instead of settling. A
                   MOSFET holding a DC gate bias drew a large steady current into its gate - 24 mA
                   on a 2 pF gate biased at 3 V, where the displacement current is 12.6 uA. No
                   conservation check could see it, because a terminal current is read back out of
                   the same stamp that produced it, and --flow-test skips gate nodes outright.
                   --dvdt-test is the check that can: it compares against C dv/dt worked out
                   outside the solver.

                   Stateless, so there is nothing to advance and nothing to advance at the wrong
                   time; and +Ieq on the gate, the way an ordinary capacitor and the crystal's
                   holder capacitance are stamped. It was -Ieq here. */
                if (g_stamp_prev_step) {
                    double pG = (n[0] > 0) ? vector_get(g_stamp_prev_step, n[0]-1) : 0;
                    double pD = (n[1] > 0) ? vector_get(g_stamp_prev_step, n[1]-1) : 0;
                    double pS = (n[2] > 0) ? vector_get(g_stamp_prev_step, n[2]-1) : 0;

                    double G_cgs = Cgs / dt;
                    double G_cgd = Cgd / dt;
                    double I_cgs_eq = G_cgs * (pG - pS);
                    double I_cgd_eq = G_cgd * (pG - pD);

                    STAMP_CONDUCTANCE(n[0], n[2], G_cgs);
                    if (n[0] > 0) vector_add(b, n[0]-1, I_cgs_eq);
                    if (n[2] > 0) vector_add(b, n[2]-1, -I_cgs_eq);

                    STAMP_CONDUCTANCE(n[0], n[1], G_cgd);
                    if (n[0] > 0) vector_add(b, n[0]-1, I_cgd_eq);
                    if (n[1] > 0) vector_add(b, n[1]-1, -I_cgd_eq);
                }
            }
            break;
        }

        case COMP_OPAMP: {
            // VCVS model with rail saturation: Vout = clamp(A * (V+ - V-), vmin, vmax)
            // For COMP_OPAMP: n[0]="-", n[1]="+", n[2]="OUT"
            int volt_idx = num_nodes + comp->voltage_var_idx;
            if (!opamp_stamp_dynamic(comp, A, b, prev_solution, n[1], n[0], n[2], volt_idx, dt))
                opamp_stamp_output(comp, A, b, prev_solution, n[1], n[0], n[2], volt_idx,
                                   comp->props.opamp.gain,
                                   comp->props.opamp.vmax, comp->props.opamp.vmin);
            break;
        }

        case COMP_OPAMP_FLIPPED: {
            // Same model as COMP_OPAMP; only the symbol's input order differs.
            // For COMP_OPAMP_FLIPPED: n[0]="+", n[1]="-", n[2]="OUT"
            int volt_idx = num_nodes + comp->voltage_var_idx;
            if (!opamp_stamp_dynamic(comp, A, b, prev_solution, n[0], n[1], n[2], volt_idx, dt))
                opamp_stamp_output(comp, A, b, prev_solution, n[0], n[1], n[2], volt_idx,
                                   comp->props.opamp.gain,
                                   comp->props.opamp.vmax, comp->props.opamp.vmin);
            break;
        }

        case COMP_SQUARE_WAVE: {
            double amp = comp->props.square_wave.amplitude;
            double freq = comp->props.square_wave.frequency;
            double phase = comp->props.square_wave.phase * M_PI / 180.0;
            double offset = comp->props.square_wave.offset;
            double duty = comp->props.square_wave.duty;

            // Apply amplitude and frequency sweeps if enabled
            amp = sweep_get_value(&comp->props.square_wave.amplitude_sweep, amp, time);
            freq = sweep_get_value(&comp->props.square_wave.frequency_sweep, freq, time);

            // Calculate normalized position in period (0 to 1)
            double period = 1.0 / freq;
            double t_norm = fmod(time + phase / (2 * M_PI * freq), period) / period;
            if (t_norm < 0) t_norm += 1.0;

            /* Square wave with the edges it says it has: rise_time and fall_time were carried
               and never used, so the edge was a step and its di/dt was whatever the time step
               made it. At 1 ns against a millisecond period this is invisible; on the templates
               that live in nanoseconds it is the whole answer. */
            double hi = amp + offset, lo = -amp + offset;
            double t_in = t_norm * period;
            double sq_tr = comp->props.square_wave.rise_time;
            double sq_tf = comp->props.square_wave.fall_time;
            if (sq_tr < 0) sq_tr = 0;
            if (sq_tf < 0) sq_tf = 0;
            double high_time = duty * period, low_time = period - high_time;
            if (sq_tr > high_time * 0.5) sq_tr = high_time * 0.5;
            if (sq_tf > low_time * 0.5) sq_tf = low_time * 0.5;

            double V;
            if (sq_tr > 0 && t_in < sq_tr)            V = lo + (hi - lo) * (t_in / sq_tr);
            else if (t_in < high_time)                V = hi;
            else if (sq_tf > 0 && t_in < high_time + sq_tf)
                                                      V = hi - (hi - lo) * ((t_in - high_time) / sq_tf);
            else                                      V = lo;
            int volt_idx = num_nodes + comp->voltage_var_idx;

            if (n[0] > 0) {
                matrix_add(A, volt_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, volt_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, volt_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, volt_idx, -1);
            }
            vector_add(b, volt_idx, V);
            break;
        }

        case COMP_TRIANGLE_WAVE: {
            double amp = comp->props.triangle_wave.amplitude;
            double freq = comp->props.triangle_wave.frequency;
            double phase = comp->props.triangle_wave.phase * M_PI / 180.0;
            double offset = comp->props.triangle_wave.offset;

            // Apply amplitude and frequency sweeps if enabled
            amp = sweep_get_value(&comp->props.triangle_wave.amplitude_sweep, amp, time);
            freq = sweep_get_value(&comp->props.triangle_wave.frequency_sweep, freq, time);

            // Calculate normalized position in period (0 to 1)
            double period = 1.0 / freq;
            double t_norm = fmod(time + phase / (2 * M_PI * freq), period) / period;
            if (t_norm < 0) t_norm += 1.0;

            // Triangle wave: rises for first half, falls for second half
            double V;
            if (t_norm < 0.5) {
                V = amp * (4.0 * t_norm - 1.0) + offset;
            } else {
                V = amp * (3.0 - 4.0 * t_norm) + offset;
            }
            int volt_idx = num_nodes + comp->voltage_var_idx;

            if (n[0] > 0) {
                matrix_add(A, volt_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, volt_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, volt_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, volt_idx, -1);
            }
            vector_add(b, volt_idx, V);
            break;
        }

        case COMP_SAWTOOTH_WAVE: {
            double amp = comp->props.sawtooth_wave.amplitude;
            double freq = comp->props.sawtooth_wave.frequency;
            double phase = comp->props.sawtooth_wave.phase * M_PI / 180.0;
            double offset = comp->props.sawtooth_wave.offset;

            // Apply amplitude and frequency sweeps if enabled
            amp = sweep_get_value(&comp->props.sawtooth_wave.amplitude_sweep, amp, time);
            freq = sweep_get_value(&comp->props.sawtooth_wave.frequency_sweep, freq, time);

            // Calculate normalized position in period (0 to 1)
            double period = 1.0 / freq;
            double t_norm = fmod(time + phase / (2 * M_PI * freq), period) / period;
            if (t_norm < 0) t_norm += 1.0;

            // Sawtooth wave: linear ramp from -amp to +amp
            double V = amp * (2.0 * t_norm - 1.0) + offset;
            int volt_idx = num_nodes + comp->voltage_var_idx;

            if (n[0] > 0) {
                matrix_add(A, volt_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, volt_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, volt_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, volt_idx, -1);
            }
            vector_add(b, volt_idx, V);
            break;
        }

        case COMP_NOISE_SOURCE: {
            double amp = comp->props.noise_source.amplitude;
            // Apply amplitude sweep if enabled
            amp = sweep_get_value(&comp->props.noise_source.amplitude_sweep, amp, time);
            // Simple pseudo-random noise using time-based seed
            // Uses a combination of sine functions at irrational ratios for pseudo-randomness
            double V = amp * (sin(time * 12345.6789) + sin(time * 9876.5432 + 1.234) +
                             sin(time * 5678.1234 + 2.345)) / 3.0;
            int volt_idx = num_nodes + comp->voltage_var_idx;

            if (n[0] > 0) {
                matrix_add(A, volt_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, volt_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, volt_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, volt_idx, -1);
            }
            vector_add(b, volt_idx, V);
            break;
        }

        case COMP_SPST_SWITCH: {
            // SPST switch: simple variable resistance between terminals
            // When closed: low resistance (r_on)
            // When open: high resistance (r_off)
            double R = comp->props.switch_spst.closed ?
                       comp->props.switch_spst.r_on :
                       comp->props.switch_spst.r_off;
            double G = 1.0 / R;
            STAMP_CONDUCTANCE(n[0], n[1], G);
            break;
        }

        case COMP_SPDT_SWITCH: {
            // SPDT switch: Common terminal connects to either A or B
            // Terminal 0 = Common, Terminal 1 = A, Terminal 2 = B
            double r_on = comp->props.switch_spdt.r_on;
            double r_off = comp->props.switch_spdt.r_off;
            int pos = comp->props.switch_spdt.position;

            // Common to A
            double R_ca = (pos == 0) ? r_on : r_off;
            double G_ca = 1.0 / R_ca;
            STAMP_CONDUCTANCE(n[0], n[1], G_ca);

            // Common to B
            double R_cb = (pos == 1) ? r_on : r_off;
            double G_cb = 1.0 / R_cb;
            STAMP_CONDUCTANCE(n[0], n[2], G_cb);
            break;
        }

        case COMP_PUSH_BUTTON: {
            // Push button: momentary switch, normally open
            double R = comp->props.push_button.pressed ?
                       comp->props.push_button.r_on :
                       comp->props.push_button.r_off;
            double G = 1.0 / R;
            STAMP_CONDUCTANCE(n[0], n[1], G);
            break;
        }

        case COMP_TRANSFORMER: {
            // Ideal transformer with one auxiliary variable i_s (current entering S1 and
            // leaving S2 through the secondary winding):
            //   V_s1 - V_s2 = N (V_p1 - V_p2)          (aux row)
            //   I_p1 = -N i_s, I_p2 = +N i_s           (power balance: V_p I_p + V_s I_s = 0)
            // Terminals: P1 (n[0]), P2 (n[1]), S1 (n[2]), S2 (n[3]); N = N_sec / N_pri.
            // Non-ideal mode adds the winding resistances referred to the secondary and a
            // finite magnetizing branch; ideal mode keeps only a tiny magnetizing conductance.
            double N = comp->props.transformer.turns_ratio;
            int k = num_nodes + comp->voltage_var_idx;
            double R_ref = 0.0, G_mag = 1e-9;
            if (!comp->props.transformer.ideal) {
                R_ref = comp->props.transformer.r_secondary + N * N * comp->props.transformer.r_primary;
                G_mag = 1e-6;
            }
            // aux row: V_s1 - V_s2 - N V_p1 + N V_p2 - R_ref i_s = 0
            if (n[2] > 0) matrix_add(A, k, n[2]-1, 1.0);
            if (n[3] > 0) matrix_add(A, k, n[3]-1, -1.0);
            if (n[0] > 0) matrix_add(A, k, n[0]-1, -N);
            if (n[1] > 0) matrix_add(A, k, n[1]-1, N);
            matrix_add(A, k, k, -R_ref - 1e-9);
            // secondary KCL: i_s leaves S1 into the device, returns at S2
            if (n[2] > 0) matrix_add(A, n[2]-1, k, 1.0);
            if (n[3] > 0) matrix_add(A, n[3]-1, k, -1.0);
            // primary KCL: reflected current -N i_s leaves P1, +N i_s leaves P2
            if (n[0] > 0) matrix_add(A, n[0]-1, k, -N);
            if (n[1] > 0) matrix_add(A, n[1]-1, k, N);
            STAMP_CONDUCTANCE(n[0], n[1], G_mag);
            break;
        }

        case COMP_TRANSFORMER_CT: {
            // Center-tapped transformer
            // Terminals: P1 (n[0]), P2 (n[1]), S1 (n[2]), CT (n[3]), S2 (n[4])
            //
            // For turns ratio N (e.g., 0.1 means 10:1 step-down):
            // V(S1-CT) = N/2 * V(P1-P2)  (upper half secondary)
            // V(CT-S2) = N/2 * V(P1-P2)  (lower half secondary)
            // V(S1-S2) = N * V(P1-P2)    (full secondary)
            //
            // Use voltage-controlled voltage source model with source resistance:
            // V_s = N * V_p, with small series resistance for stability
            double N = comp->props.transformer.turns_ratio;
            double N_half = N / 2.0;

            // Source resistance for secondary windings (provides numerical stability)
            double R_src = 1.0;  // 1 ohm series resistance
            double G_src = 1.0 / R_src;

            // Primary magnetizing inductance modeled as resistance for DC stability
            double R_mag = 10000.0;  // High resistance (low magnetizing current)
            double G_mag = 1.0 / R_mag;
            STAMP_CONDUCTANCE(n[0], n[1], G_mag);

            /* Two auxiliary currents, one per half-secondary, stamped the way COMP_TRANSFORMER
               stamps its one:

                   V_s1 - V_ct - N_half (V_p1 - V_p2) - R_w i1 = 0      (aux row k)
                   V_ct - V_s2 - N_half (V_p1 - V_p2) - R_w i2 = 0      (aux row k+1)
                   I_p1 = -N_half (i1 + i2),  I_p2 = +N_half (i1 + i2)

               The last line is the whole point and it was missing. This part used to be stamped
               as a pair of voltage-controlled sources with a series conductance: the secondary
               followed the primary correctly, and nothing carried the secondary's current back.
               A transformer that does not reflect its load is a free source, and this one was -
               measured, the primary drew 0.012 A with a 10 k load on the secondary and 0.012 A
               with 10 ohm, while the two-winding part next to it went up by a factor of 999.

               R_w is the same referred winding resistance the two-winding part uses, and it also
               keeps the aux row off a zero pivot when the windings are ideal. */
            int k = num_nodes + comp->voltage_var_idx;
            double R_w = N_half * N_half * 1e-3 + 1e-3;

            /* upper half: S1 (n[2]) to CT (n[3]) */
            if (n[2] > 0) matrix_add(A, k, n[2]-1, 1.0);
            if (n[3] > 0) matrix_add(A, k, n[3]-1, -1.0);
            if (n[0] > 0) matrix_add(A, k, n[0]-1, -N_half);
            if (n[1] > 0) matrix_add(A, k, n[1]-1, N_half);
            matrix_add(A, k, k, -R_w);
            if (n[2] > 0) matrix_add(A, n[2]-1, k, 1.0);
            if (n[3] > 0) matrix_add(A, n[3]-1, k, -1.0);

            /* lower half: CT (n[3]) to S2 (n[4]) */
            if (n[3] > 0) matrix_add(A, k+1, n[3]-1, 1.0);
            if (n[4] > 0) matrix_add(A, k+1, n[4]-1, -1.0);
            if (n[0] > 0) matrix_add(A, k+1, n[0]-1, -N_half);
            if (n[1] > 0) matrix_add(A, k+1, n[1]-1, N_half);
            matrix_add(A, k+1, k+1, -R_w);
            if (n[3] > 0) matrix_add(A, n[3]-1, k+1, 1.0);
            if (n[4] > 0) matrix_add(A, n[4]-1, k+1, -1.0);

            /* and both halves reflected into the primary */
            if (n[0] > 0) { matrix_add(A, n[0]-1, k, -N_half); matrix_add(A, n[0]-1, k+1, -N_half); }
            if (n[1] > 0) { matrix_add(A, n[1]-1, k,  N_half); matrix_add(A, n[1]-1, k+1,  N_half); }

            (void)G_src;
            break;
        }

        // === ADDITIONAL PASSIVE COMPONENTS ===

        case COMP_POTENTIOMETER: {
            // Potentiometer: 3-terminal variable resistor
            // Terminal 0 and 1 are the ends, terminal 2 is the wiper
            double R_total = comp->props.potentiometer.resistance;
            double pos = comp->props.potentiometer.wiper_pos;
            pos = CLAMP(pos, 0.001, 0.999);  // Avoid zero resistance

            double R_low = R_total * pos;           // Resistance from terminal 0 to wiper
            double R_high = R_total * (1.0 - pos);  // Resistance from wiper to terminal 1

            double G_low = 1.0 / R_low;
            double G_high = 1.0 / R_high;

            STAMP_CONDUCTANCE(n[0], n[2], G_low);   // Terminal 0 to wiper
            STAMP_CONDUCTANCE(n[2], n[1], G_high);  // Wiper to terminal 1
            break;
        }

        case COMP_PHOTORESISTOR: {
            // Photoresistor: resistance varies with light level
            // Use global environment light level for all LDRs
            double R_dark = comp->props.photoresistor.r_dark;
            double R_light = comp->props.photoresistor.r_light;
            double light = g_environment.light_level;  // Use global light level
            double gamma = comp->props.photoresistor.gamma;

            // Logarithmic response to light
            double R = R_dark * pow(R_light / R_dark, pow(light, gamma));
            double G = 1.0 / R;
            STAMP_CONDUCTANCE(n[0], n[1], G);
            break;
        }

        case COMP_THERMISTOR: {
            // Thermistor: resistance varies with temperature
            // Use global environment temperature for all thermistors
            double R_25 = comp->props.thermistor.r_25;
            double beta = comp->props.thermistor.beta;
            double T = g_environment.temperature + 273.15;  // Use global temperature, convert to Kelvin
            double T_25 = 298.15;  // 25°C in Kelvin

            double R;
            if (comp->props.thermistor.type == 0) {
                // NTC: resistance decreases with temperature
                R = R_25 * exp(beta * (1.0/T - 1.0/T_25));
            } else {
                // PTC: resistance increases with temperature (simplified)
                R = R_25 * exp(-beta * (1.0/T - 1.0/T_25));
            }
            double G = 1.0 / R;
            STAMP_CONDUCTANCE(n[0], n[1], G);
            break;
        }

        case COMP_MEMRISTOR: {
            // Simplified memristor: acts like resistor
            double G = 1.0 / comp->props.resistor.resistance;
            STAMP_CONDUCTANCE(n[0], n[1], G);
            break;
        }

        case COMP_FUSE: {
            // Fuse with i²t protection model
            // When not blown: accumulate i²t energy, blow when it exceeds rating
            // When blown: very high resistance (open circuit)

            if (!comp->props.fuse.blown) {
                // Calculate current from previous solution voltages
                double V1 = (n[0] > 0 && prev_solution) ? vector_get(prev_solution, n[0]-1) : 0.0;
                double V2 = (n[1] > 0 && prev_solution) ? vector_get(prev_solution, n[1]-1) : 0.0;
                double Vdiff = V1 - V2;
                double R = comp->props.fuse.resistance;
                double I = Vdiff / R;
                double I_abs = fabs(I);

                // Store current for display/animation
                comp->props.fuse.current = I_abs;

                if (comp->props.fuse.ideal) {
                    // Ideal mode: instant blow when current exceeds rating
                    if (I_abs > comp->props.fuse.rating) {
                        comp->props.fuse.blown = true;
                        comp->props.fuse.blow_time = time;
                    }
                } else {
                    // Realistic mode: i²t accumulation
                    // Only accumulate when current exceeds rating (pre-arcing region)
                    if (I_abs > comp->props.fuse.rating && !g_stamp_read_only) {
                        /* i2t decides when the fuse blows, so it is integration state and not a
                           readout like props.fuse.current above it. A stamp runs once per Newton
                           iteration and once more whenever the current-flow display reads a
                           terminal current back, so counting here scaled the accumulated energy
                           by the iteration count - measured at exactly 2x on a DC circuit with
                           the display on. The read-only guard is the same one the other companion
                           state uses; the remaining per-iteration multiple is bounded by Newton
                           and is far smaller than the decade-wide i2t spread of real fuses. */
                        double i2t_increment = I_abs * I_abs * dt;
                        comp->props.fuse.i2t_accumulated += i2t_increment;

                        // Check if accumulated energy exceeds i²t rating
                        if (comp->props.fuse.i2t_accumulated >= comp->props.fuse.i2t) {
                            comp->props.fuse.blown = true;
                            comp->props.fuse.blow_time = time;
                        }
                    } else {
                        // Below rating: slowly cool down (dissipate accumulated energy)
                        // This simulates thermal recovery when overcurrent is removed
                        double cooling_rate = 0.1; // 10% per second
                        comp->props.fuse.i2t_accumulated *= (1.0 - cooling_rate * dt);
                        if (comp->props.fuse.i2t_accumulated < 0.001) {
                            comp->props.fuse.i2t_accumulated = 0.0;
                        }
                    }
                }
            } else {
                // Blown fuse: no current
                comp->props.fuse.current = 0.0;
            }

            // Stamp conductance: low when intact, very high resistance when blown
            double R = comp->props.fuse.blown ? 1e9 : comp->props.fuse.resistance;
            double G = 1.0 / R;
            STAMP_CONDUCTANCE(n[0], n[1], G);
            break;
        }

        case COMP_CRYSTAL: {
            /* Motional arm Ls-Rs-Cs in series, with Cp across the whole thing.

               The arm is integrated with the TRAPEZOIDAL rule (theta = 0.5) rather than the
               theta = 0.6 used elsewhere. That 0.6 exists to damp the ringing a slope
               discontinuity leaves behind, and it costs a little amplitude every step - which
               is exactly what a resonator with a Q in the hundreds cannot afford. Damping a
               crystal numerically is how it fails to start.

                 i_n (1/a + Rs + b) = v_n + v_prev + i_prev (1/a - Rs - b) - 2 vc_prev
                 a = dt/(2 Ls),  b = dt/(2 Cs),  vc = the motional capacitor's own voltage

               so the auxiliary row carries -(1/a + Rs + b) on the current and the history on
               the right. vc is advanced after the step in simulation.c. */
            double Ls = comp->props.crystal.ls, Cs = comp->props.crystal.cs;
            double Rs = comp->props.crystal.ideal ? 0.0 : comp->props.crystal.rs;
            double Cp = comp->props.crystal.cp;
            if (Ls <= 0 || Cs <= 0) break;
            int idx = num_nodes + comp->voltage_var_idx;
            Vector *mem = g_stamp_prev_step ? g_stamp_prev_step : prev_solution;
            bool trans = (mem != NULL) && dt > 0 && idx < (int)mem->size;

            if (n[0] > 0) { matrix_add(A, idx, n[0]-1, 1.0);  matrix_add(A, n[0]-1, idx, 1.0); }
            if (n[1] > 0) { matrix_add(A, idx, n[1]-1, -1.0); matrix_add(A, n[1]-1, idx, -1.0); }

            if (trans) {
                double ka = dt / (2.0 * Ls), kb = dt / (2.0 * Cs);
                double i_prev = vector_get(mem, idx);
                double v0p = (n[0] > 0) ? vector_get(mem, n[0]-1) : 0;
                double v1p = (n[1] > 0) ? vector_get(mem, n[1]-1) : 0;
                matrix_add(A, idx, idx, -(1.0 / ka + Rs + kb));
                vector_add(b, idx, -(v0p - v1p) - (1.0 / ka - Rs - kb) * i_prev
                                   + 2.0 * (g_stamp_read_only ? comp->cap_vc_solve : comp->cap_vc));
            } else {
                /* operating point: the arm is an open circuit (its capacitor blocks DC), so a
                   large resistance keeps the row solvable without passing current */
                matrix_add(A, idx, idx, -1e12);
            }

            /* the holder capacitance sits across the part and is an ordinary capacitor */
            if (Cp > 0 && trans) {
                double Geq = Cp / (0.6 * dt);
                double v0p = (n[0] > 0) ? vector_get(mem, n[0]-1) : 0;
                double v1p = (n[1] > 0) ? vector_get(mem, n[1]-1) : 0;
                double Ieq = Geq * (v0p - v1p)
                           + (0.4 / 0.6) * (g_stamp_read_only ? comp->trap_i_solve : comp->trap_i_prev);
                STAMP_CONDUCTANCE(n[0], n[1], Geq);
                if (n[0] > 0) vector_add(b, n[0]-1, Ieq);
                if (n[1] > 0) vector_add(b, n[1]-1, -Ieq);
            }
            break;
        }

        case COMP_SOURCE_3PH: {
            // Three voltage sources A, B, C referenced to N, each with its own current variable
            // (voltage_var_idx + k) and a small series R (+ optional L, backward Euler) per phase.
            double w = 2.0 * M_PI * comp->props.source_3ph.frequency;
            double Rs = fmax(comp->props.source_3ph.r_series, 1e-6);
            double Ls = fmax(comp->props.source_3ph.l_series, 0.0);
            Vector *mem = g_stamp_prev_step ? g_stamp_prev_step : prev_solution;
            for (int k = 0; k < 3; k++) {
                int idx = num_nodes + comp->voltage_var_idx + k;
                double ph = (comp->props.source_3ph.phase - 120.0 * k) * M_PI / 180.0;   // A, B = -120, C = -240 (= +120)
                double V = comp->props.source_3ph.v_peak * sin(w * time + ph);
                double Lterm = (mem && dt > 0) ? Ls / dt : 0.0;
                double iprev = (mem && dt > 0 && idx < (int)mem->size) ? vector_get(mem, idx) : 0.0;
                if (n[k] > 0) { matrix_add(A, idx, n[k]-1, 1.0);  matrix_add(A, n[k]-1, idx, 1.0); }
                if (n[3] > 0) { matrix_add(A, idx, n[3]-1, -1.0); matrix_add(A, n[3]-1, idx, -1.0); }
                matrix_add(A, idx, idx, -(Rs + Lterm));
                vector_add(b, idx, V - Lterm * iprev);
            }
            break;
        }

        case COMP_DELAY_LINE: {
            /* Bergeron's method. Each end of a lossless line looks like Z0 in series with a
               source carrying whatever the OTHER end launched one delay ago:

                   E1(t) = v2(t - T) + Z0 i2(t - T)
                   E2(t) = v1(t - T) + Z0 i1(t - T)

               In Norton form that is a conductance 1/Z0 to ground and a current source E/Z0
               into the node - two stamps, no auxiliary row, and exact for any time step. The
               delay lives in the history buffer, so a 5 ns cable does not need the solver to
               take 5 ns steps; it only needs enough steps to resolve the edge you are sending
               down it.

               Both ports are referred to ground: this is a coax with its shield on the ground
               plane, which is what a bench cable and a board trace over a plane both are. */
            double z0 = comp->props.delay_line.z0;
            double td = comp->props.delay_line.delay;
            if (z0 <= 0) break;
            double g = 1.0 / z0;

            /* The wave arriving now was launched one delay ago at the far end. At the operating
               point (dt = 0) there is no history at all, so the line is just 2 x Z0 to ground -
               a matched, quiet cable, which is the right DC starting state. */
            double e1 = 0.0, e2 = 0.0;
            if (dt > 0 && td > 0) {
                double now = time + dt;   /* the stamp is called for the step being solved */
                e1 = delay_line_history(comp, 1, now - td);   /* what port 2 launched */
                e2 = delay_line_history(comp, 0, now - td);   /* what port 1 launched */
                if (!comp->props.delay_line.ideal && comp->props.delay_line.loss_db > 0) {
                    double a = pow(10.0, -comp->props.delay_line.loss_db / 20.0);
                    e1 *= a; e2 *= a;
                }
            }

            if (n[0] > 0) {
                matrix_add(A, n[0]-1, n[0]-1, g);
                vector_add(b, n[0]-1, e1 * g);
            }
            if (n[1] > 0) {
                matrix_add(A, n[1]-1, n[1]-1, g);
                vector_add(b, n[1]-1, e2 * g);
            }
            break;
        }

        case COMP_TLINE: {
            // Series R-L through the auxiliary current i, shunt C/2 at each end, both with the same
            // theta method as the capacitor (theta = 0.6: trapezoidal accuracy, slight damping).
            //   aux row: V - (R + L/(th dt)) i = -((1-th)/th) V_prev + (((1-th)/th) R - L/(th dt)) i_prev
            //   caps:    i_c = Geq v - Ieq, Geq = C/(th dt), Ieq = Geq v_prev + ((1-th)/th) i_c_prev
            double R, L, Cend;
            tline_params(comp, &R, &L, &Cend);
            const double TH = 0.6, K = (1.0 - TH) / TH;
            int k = num_nodes + comp->voltage_var_idx;
            Vector *mem = g_stamp_prev_step ? g_stamp_prev_step : prev_solution;
            bool transient = (mem != NULL) && dt > 0;
            double Lterm = transient ? L / (TH * dt) : 0.0;
            double i_prev = (transient && k < (int)mem->size) ? vector_get(mem, k) : 0.0;
            double v1p = (transient && n[0] > 0) ? vector_get(mem, n[0]-1) : 0.0;
            double v2p = (transient && n[1] > 0) ? vector_get(mem, n[1]-1) : 0.0;
            if (n[0] > 0) { matrix_add(A, k, n[0]-1, 1.0);  matrix_add(A, n[0]-1, k, 1.0); }
            if (n[1] > 0) { matrix_add(A, k, n[1]-1, -1.0); matrix_add(A, n[1]-1, k, -1.0); }
            matrix_add(A, k, k, -(R + Lterm + 1e-9));
            if (transient) vector_add(b, k, -K * (v1p - v2p) + (K * R - Lterm) * i_prev);
            if (transient && Cend > 0) {
                double Geq = Cend / (TH * dt);
                double vp[2] = { v1p, v2p };
                for (int e = 0; e < 2; e++) {
                    if (n[e] <= 0) continue;
                    double Ieq = Geq * vp[e] + K * comp->tline_ic_prev[e];
                    matrix_add(A, n[e]-1, n[e]-1, Geq);
                    vector_add(b, n[e]-1, Ieq);
                }
            }
            break;
        }

        case COMP_SPARK_GAP: {
            // Hysteretic arc: open (1 pS) until breakdown, then r_on. The state is only
            // switched between accepted steps so Newton always sees a fixed conductance.
            double G = comp->props.spark_gap.conducting ? 1.0 / fmax(comp->props.spark_gap.r_on, 1e-3) : 1e-12;
            STAMP_CONDUCTANCE(n[0], n[1], G);
            break;
        }

        // === ADDITIONAL SOURCES ===

        case COMP_AC_CURRENT: {
            double amp = comp->props.ac_current.amplitude;
            double freq = comp->props.ac_current.frequency;
            double phase = comp->props.ac_current.phase * M_PI / 180.0;
            double offset = comp->props.ac_current.offset;

            double I = amp * sin(2 * M_PI * freq * time + phase) + offset;
            if (n[0] > 0) vector_add(b, n[0]-1, -I);
            if (n[1] > 0) vector_add(b, n[1]-1, I);
            break;
        }

        case COMP_CLOCK: {
            double freq = comp->props.clock.frequency;
            double duty = comp->props.clock.duty;
            double v_low = comp->props.clock.v_low;
            double v_high = comp->props.clock.v_high;

            double period = 1.0 / freq;
            double t_norm = fmod(time, period) / period;
            double V = (t_norm < duty) ? v_high : v_low;
            int volt_idx = num_nodes + comp->voltage_var_idx;

            if (n[0] > 0) {
                matrix_add(A, volt_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, volt_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, volt_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, volt_idx, -1);
            }
            vector_add(b, volt_idx, V);
            break;
        }

        case COMP_VADC_SOURCE:
        case COMP_AM_SOURCE:
        case COMP_FM_SOURCE: {
            // Variable/modulated sources - treat as DC/AC for basic simulation
            double V = comp->props.dc_voltage.voltage;
            int volt_idx = num_nodes + comp->voltage_var_idx;

            if (n[0] > 0) {
                matrix_add(A, volt_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, volt_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, volt_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, volt_idx, -1);
            }
            vector_add(b, volt_idx, V);
            break;
        }

        case COMP_BATTERY: {
            // Battery with discharge model
            // Model: V_out = V_nominal * f(SoC) - I * R_internal
            // where f(SoC) is a voltage curve based on state of charge

            double V_nom = comp->props.battery.nominal_voltage;
            double R_int = comp->props.battery.internal_r;
            double SoC = comp->props.battery.charge_state;
            double V_cutoff = comp->props.battery.v_cutoff;
            bool discharged = comp->props.battery.discharged;
            bool ideal = comp->props.battery.ideal;

            // Voltage curve: V = V_nom * (0.9 + 0.1 * SoC) with cutoff
            // This gives roughly 90% voltage at empty, 100% at full
            double V_oc;  // Open-circuit voltage
            if (discharged) {
                V_oc = V_cutoff * 0.8;  // Dead battery
            } else {
                // Simple linear discharge curve
                V_oc = V_nom * (0.85 + 0.15 * SoC);
                if (V_oc < V_cutoff) {
                    comp->props.battery.discharged = true;
                    V_oc = V_cutoff * 0.8;
                }
            }

            int volt_idx = num_nodes + comp->voltage_var_idx;

            if (ideal) {
                // Ideal voltage source (no internal resistance)
                if (n[0] > 0) {
                    matrix_add(A, volt_idx, n[0]-1, 1);
                    matrix_add(A, n[0]-1, volt_idx, 1);
                }
                if (n[1] > 0) {
                    matrix_add(A, volt_idx, n[1]-1, -1);
                    matrix_add(A, n[1]-1, volt_idx, -1);
                }
                vector_add(b, volt_idx, V_oc);
            } else {
                // Non-ideal: voltage source with series resistance
                // Stamp as voltage source in series with resistor
                // V = V_oc - I * R_int
                // Using voltage source equation: V(n+) - V(n-) - I*R = V_oc
                // Rearranged: V(n+) - V(n-) + I*R_int = V_oc
                if (n[0] > 0) {
                    matrix_add(A, volt_idx, n[0]-1, 1);
                    matrix_add(A, n[0]-1, volt_idx, 1);
                }
                if (n[1] > 0) {
                    matrix_add(A, volt_idx, n[1]-1, -1);
                    matrix_add(A, n[1]-1, volt_idx, -1);
                }
                /* NEGATIVE R_int, as COMP_DC_VOLTAGE stamps -r_series a thousand lines above.
                   The algebra in the comment is right and the convention it assumes is not: this
                   solver's auxiliary current runs the other way, so +R_int made the internal
                   resistance push instead of drop. A 12 V battery with 1 ohm internal across a
                   3 ohm load read 18 V - half again its own open-circuit voltage - and a 1.5 V AA
                   read 3.0. --flow-test could not see it, because a device wired backwards is
                   still consistent with itself when its terminal current is recovered by
                   re-stamping it alone; --sign-test measures it against the divider instead. */
                matrix_add(A, volt_idx, volt_idx, -R_int);
                vector_add(b, volt_idx, V_oc);
            }

            /* The coulomb count used to live here, and a stamp is the wrong place for an
               integrator. It ran once per Newton iteration, once more whenever the current-flow
               display read a terminal current back, and - fatally - once during the DC operating
               point, where dt is the pseudo-step 1e9 that makes capacitors look like opens. A
               default AA across 100 ohm therefore lost 0.015 A x 1e9 s of charge before the first
               transient step: every Run began with a flat battery reading 0.72 V instead of 1.5 V.
               It is advanced once per accepted step in simulation.c now, with the rest of the
               companions. Only the display current is recorded here, and not while reading. */
            if (!ideal && !discharged && prev_solution && !g_stamp_read_only && dt > 0 && dt < 1e6)
                comp->props.battery.current_draw = fabs(vector_get(prev_solution, volt_idx));
            break;
        }

        case COMP_PULSE_SOURCE: {
            double v_low = comp->props.pulse_source.v_low;
            double v_high = comp->props.pulse_source.v_high;
            double delay = comp->props.pulse_source.delay;
            double pw = comp->props.pulse_source.pulse_width;
            double period = comp->props.pulse_source.period;

            /* A real driver's edge takes time, and this one carries the numbers for it -
               rise_time and fall_time, 1 ns by default - but stepped instantly anyway. An
               instantaneous edge has no defined di/dt, so every circuit that answers to an edge
               rate answered to the time step instead: the ground-bounce template's spike doubled
               each time the step was halved and never settled. With the edge given its own
               duration the answer is the circuit's, not the solver's. */
            double tr = comp->props.pulse_source.rise_time;
            double tf = comp->props.pulse_source.fall_time;
            if (tr < 0) tr = 0;
            if (tf < 0) tf = 0;
            /* an edge can never eat its own half of the period, or a slow setting turns the
               pulse into a triangle */
            if (tr > pw * 0.5) tr = pw * 0.5;
            if (period - pw > 0 && tf > (period - pw) * 0.5) tf = (period - pw) * 0.5;

            double V = v_low;
            if (time >= delay) {
                double t_in_period = fmod(time - delay, period);
                if (tr > 0 && t_in_period < tr)
                    V = v_low + (v_high - v_low) * (t_in_period / tr);
                else if (t_in_period < pw)
                    V = v_high;
                else if (tf > 0 && t_in_period < pw + tf)
                    V = v_high - (v_high - v_low) * ((t_in_period - pw) / tf);
            }
            int volt_idx = num_nodes + comp->voltage_var_idx;

            if (n[0] > 0) {
                matrix_add(A, volt_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, volt_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, volt_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, volt_idx, -1);
            }
            vector_add(b, volt_idx, V);
            break;
        }

        case COMP_PWM_SOURCE: {
            double amp = comp->props.pwm_source.amplitude;
            double freq = comp->props.pwm_source.frequency;
            double duty = comp->props.pwm_source.duty;
            double offset = comp->props.pwm_source.offset;

            double period = 1.0 / freq;
            double t_norm = fmod(time, period) / period;
            double V = (t_norm < duty) ? (amp + offset) : offset;
            int volt_idx = num_nodes + comp->voltage_var_idx;

            if (n[0] > 0) {
                matrix_add(A, volt_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, volt_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, volt_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, volt_idx, -1);
            }
            vector_add(b, volt_idx, V);
            break;
        }

        case COMP_PWL_SOURCE: {
            // Piecewise linear source - interpolate between time-value pairs
            int num_pts = comp->props.pwl_source.num_points;
            double V = 0.0;

            if (num_pts > 0) {
                double t = time;

                // Handle repeat mode
                if (comp->props.pwl_source.repeat && num_pts > 1) {
                    double period = comp->props.pwl_source.repeat_period;
                    if (period <= 0) {
                        period = comp->props.pwl_source.times[num_pts - 1];
                    }
                    if (period > 0) {
                        t = fmod(time, period);
                    }
                }

                // Find the segment and interpolate
                if (t <= comp->props.pwl_source.times[0]) {
                    V = comp->props.pwl_source.values[0];
                } else if (t >= comp->props.pwl_source.times[num_pts - 1]) {
                    V = comp->props.pwl_source.values[num_pts - 1];
                } else {
                    // Binary search for segment
                    for (int i = 0; i < num_pts - 1; i++) {
                        double t1 = comp->props.pwl_source.times[i];
                        double t2 = comp->props.pwl_source.times[i + 1];
                        if (t >= t1 && t < t2) {
                            double v1 = comp->props.pwl_source.values[i];
                            double v2 = comp->props.pwl_source.values[i + 1];
                            // Linear interpolation
                            double alpha = (t - t1) / (t2 - t1);
                            V = v1 + alpha * (v2 - v1);
                            break;
                        }
                    }
                }
            }

            int volt_idx = num_nodes + comp->voltage_var_idx;
            if (n[0] > 0) {
                matrix_add(A, volt_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, volt_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, volt_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, volt_idx, -1);
            }
            vector_add(b, volt_idx, V);
            break;
        }

        case COMP_EXPR_SOURCE: {
            // Expression-based source - parse and evaluate V(t)
            // For now, implement common built-in functions
            double V = 0.0;
            const char *expr = comp->props.expr_source.expression;

            // Simple expression parser for common patterns
            // Pattern: "A*sin(2*pi*F*t)" or "A*sin(2*pi*F*t)+B*rand()"
            double amp = 1.0, freq = 60.0, offset = 0.0;
            double noise_amp = 0.0;

            // Try to parse sine wave pattern: "A*sin(2*pi*F*t)"
            if (sscanf(expr, "%lf*sin(2*pi*%lf*t)", &amp, &freq) == 2) {
                V = amp * sin(2.0 * M_PI * freq * time);
            }
            // Try pattern with offset: "A*sin(2*pi*F*t)+C"
            else if (sscanf(expr, "%lf*sin(2*pi*%lf*t)+%lf", &amp, &freq, &offset) == 3) {
                V = amp * sin(2.0 * M_PI * freq * time) + offset;
            }
            // Try pattern with noise: "A*sin(2*pi*F*t)+N*rand()"
            else if (sscanf(expr, "%lf*sin(2*pi*%lf*t)+%lf*rand()", &amp, &freq, &noise_amp) == 3) {
                V = amp * sin(2.0 * M_PI * freq * time);
                V += noise_amp * ((double)rand() / RAND_MAX * 2.0 - 1.0);
            }
            // Simple constant
            else if (sscanf(expr, "%lf", &V) != 1) {
                V = 0.0;  // Default if parsing fails
            }

            int volt_idx = num_nodes + comp->voltage_var_idx;
            if (n[0] > 0) {
                matrix_add(A, volt_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, volt_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, volt_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, volt_idx, -1);
            }
            vector_add(b, volt_idx, V);
            break;
        }

        // === ADDITIONAL DIODES ===

        case COMP_VARACTOR:
        case COMP_TUNNEL_DIODE:
        case COMP_PHOTODIODE: {
            // Simplified diode model
            double Is = comp->props.diode.is;
            double Vt = comp->props.diode.vt;
            double nn = comp->props.diode.n;
            double nVt = nn * Vt;

            double Vd = 0.6;
            if (prev_solution) {
                double v1 = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double v2 = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                Vd = CLAMP(v1 - v2, -5*nVt, 40*nVt);
            }

            double expTerm = exp(Vd / nVt);
            double Id = Is * (expTerm - 1);
            double Gd = (Is / nVt) * expTerm + 1e-12;
            double Ieq = Id - Gd * Vd;

            STAMP_CONDUCTANCE(n[0], n[1], Gd);
            if (n[0] > 0) vector_add(b, n[0]-1, -Ieq);
            if (n[1] > 0) vector_add(b, n[1]-1, Ieq);
            break;
        }

        // === ADDITIONAL TRANSISTORS ===

        case COMP_NPN_DARLINGTON:
        case COMP_PNP_DARLINGTON: {
            // Darlington pair - same as BJT but with higher beta
            double bf = comp->props.bjt.bf;
            double Is = comp->props.bjt.is;
            double nf = comp->props.bjt.nf;
            double temp = comp->props.bjt.temp;
            double Vt = 8.617e-5 * temp;

            double sign = (comp->type == COMP_PNP_DARLINGTON) ? -1.0 : 1.0;

            double Vbe = 0.6 * sign;
            if (prev_solution) {
                double vB = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double vE = (n[2] > 0) ? vector_get(prev_solution, n[2]-1) : 0;
                Vbe = sign * (vB - vE);
                Vbe = CLAMP(Vbe, -5*nf*Vt, 40*nf*Vt);
            }

            double expBE = exp(Vbe / (nf * Vt));
            double Ibe = (Is / bf) * (expBE - 1);
            double Gbe = (Is / (bf * nf * Vt)) * expBE + 1e-12;
            double Ieq_be = Ibe - Gbe * Vbe;
            double Gm = (Is / (nf * Vt)) * expBE * sign;

            Ieq_be *= sign;

            STAMP_CONDUCTANCE(n[0], n[2], Gbe);
            if (n[0] > 0) vector_add(b, n[0]-1, -Ieq_be);
            if (n[2] > 0) vector_add(b, n[2]-1, Ieq_be);

            if (n[1] > 0 && n[0] > 0) matrix_add(A, n[1]-1, n[0]-1, Gm);
            if (n[1] > 0 && n[2] > 0) matrix_add(A, n[1]-1, n[2]-1, -Gm);
            if (n[2] > 0 && n[0] > 0) matrix_add(A, n[2]-1, n[0]-1, -Gm);
            if (n[2] > 0 && n[2] > 0) matrix_add(A, n[2]-1, n[2]-1, Gm);
            break;
        }

        case COMP_NJFET:
        case COMP_PJFET: {
            // JFET model (simplified Shichman-Hodges)
            double Idss = comp->props.jfet.idss;
            double Vp = comp->props.jfet.vp;
            double lambda = comp->props.jfet.lambda;

            double sign = (comp->type == COMP_PJFET) ? -1.0 : 1.0;
            double Vp_abs = fabs(Vp);

            double Vgs = 0, Vds = 0;
            if (prev_solution) {
                double vG = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double vD = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                double vS = (n[2] > 0) ? vector_get(prev_solution, n[2]-1) : 0;

                if (comp->type == COMP_PJFET) {
                    Vgs = vS - vG;
                    Vds = vS - vD;
                } else {
                    Vgs = vG - vS;
                    Vds = vD - vS;
                }
            }

            double Gds = 1e-12;
            double Gm = 0;
            double Id = 0;

            if (Vgs <= -Vp_abs) {
                // Cutoff
                Gds = 1e-12;
                Id = 0;
            } else if (Vds < Vgs + Vp_abs) {
                // Triode/Linear region
                double Vov = Vgs + Vp_abs;
                Id = Idss * (2 * Vov * Vds / (Vp_abs * Vp_abs) - Vds * Vds / (Vp_abs * Vp_abs));
                Gm = 2 * Idss * Vds / (Vp_abs * Vp_abs);
                Gds = 2 * Idss * (Vov - Vds) / (Vp_abs * Vp_abs);
            } else {
                // Saturation region
                double Vov = Vgs + Vp_abs;
                Id = Idss * (Vov * Vov) / (Vp_abs * Vp_abs) * (1 + lambda * Vds);
                Gm = 2 * Idss * Vov / (Vp_abs * Vp_abs) * (1 + lambda * Vds);
                Gds = lambda * Idss * (Vov * Vov) / (Vp_abs * Vp_abs);
            }

            Gds = MAX(Gds, 1e-12);
            double Ieq = Id - Gm * Vgs - Gds * Vds;

            Gm *= sign;
            Ieq *= sign;

            STAMP_CONDUCTANCE(n[1], n[2], Gds);
            if (n[1] > 0) vector_add(b, n[1]-1, -Ieq);
            if (n[2] > 0) vector_add(b, n[2]-1, Ieq);

            if (n[1] > 0 && n[0] > 0) matrix_add(A, n[1]-1, n[0]-1, Gm);
            if (n[1] > 0 && n[2] > 0) matrix_add(A, n[1]-1, n[2]-1, -Gm);
            if (n[2] > 0 && n[0] > 0) matrix_add(A, n[2]-1, n[0]-1, -Gm);
            if (n[2] > 0 && n[2] > 0) matrix_add(A, n[2]-1, n[2]-1, Gm);
            break;
        }

        // === THYRISTORS ===

        case COMP_SCR: {
            // SCR: acts as diode when triggered, open circuit otherwise
            double R = comp->props.scr.on ? 0.1 : 1e9;  // Low/high resistance
            double G = 1.0 / R;
            STAMP_CONDUCTANCE(n[1], n[2], G);  // Anode to Kathode
            break;
        }

        case COMP_DIAC: {
            // DIAC: conducts when voltage exceeds breakover
            double Vbo = comp->props.diac.vbo;
            double Vd = 0;
            if (prev_solution) {
                double v1 = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double v2 = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                Vd = v1 - v2;
            }

            double R = (fabs(Vd) > Vbo) ? 1.0 : 1e9;
            double G = 1.0 / R;
            STAMP_CONDUCTANCE(n[0], n[1], G);
            break;
        }

        case COMP_TRIAC: {
            // TRIAC: bidirectional SCR
            double R = comp->props.triac.on ? 0.1 : 1e9;
            double G = 1.0 / R;
            STAMP_CONDUCTANCE(n[1], n[2], G);  // MT1 to MT2
            break;
        }

        case COMP_UJT: {
            // UJT: simplified as resistor divider
            double G = 1.0 / 1000.0;  // 1k default
            STAMP_CONDUCTANCE(n[1], n[2], G);  // B2 to B1
            break;
        }

        // === OP-AMPS & AMPLIFIERS ===

        case COMP_OPAMP_REAL: {
            // Real op-amp with finite parameters
            // Standard VCVS model with output saturation (piecewise-linear)
            // NOTE: Backward Euler integration inherently damps oscillations.
            // For oscillator circuits, keep the noise perturbation source connected.
            double A_gain = comp->props.opamp.gain;
            double r_in = comp->props.opamp.r_in;
            double r_out = comp->props.opamp.r_out;
            double v_max = comp->props.opamp.vmax;
            double v_min = comp->props.opamp.vmin;
            int volt_idx = num_nodes + comp->voltage_var_idx;

            // Input resistance between + and - inputs
            double G_in = 1.0 / r_in;
            STAMP_CONDUCTANCE(n[0], n[1], G_in);

            // SIMPLIFIED MODEL FOR EDUCATIONAL SIMULATOR
            // Research conclusion: High-gain (1e6) oscillators are fundamentally
            // incompatible with MNA simulators using direct solves.
            //
            // Best practice for oscillator circuits:
            // 1. Use moderate gain (50-200) instead of 1e6
            // 2. Post-solve hard clamping to rails (see simulation.c)
            // 3. This provides stable, predictable oscillation

            // For oscillators: use reduced gain for numerical stability
            // Theoretical minimum for 3-stage RC phase shift = 29
            // Use 150× to provide 5× safety margin
            // Full open-loop gain: rail saturation is solved inside Newton now, so the old
            // "cap the gain at 150" workaround is no longer needed (it starved oscillators).
            double A_effective = A_gain;

            // VCVS with rail saturation solved self-consistently (piecewise-linear).
            // n[0] is inverting (-), n[1] is non-inverting (+)
            opamp_stamp_output(comp, A, b, prev_solution, n[1], n[0], n[2], volt_idx,
                               A_effective, v_max, v_min);
            // Output resistance
            double G_out = 1.0 / r_out;
            if (n[2] > 0) {
                matrix_add(A, n[2]-1, n[2]-1, G_out);
            }
            break;
        }

        case COMP_OTA: {
            // OTA: transconductance amplifier
            double gm = comp->props.opamp.gain;  // Transconductance
            int volt_idx = num_nodes + comp->voltage_var_idx;

            if (n[2] > 0) {
                matrix_add(A, volt_idx, n[2]-1, 1);
                matrix_add(A, n[2]-1, volt_idx, 1);
            }
            if (n[1] > 0) matrix_add(A, volt_idx, n[1]-1, -gm);
            if (n[0] > 0) matrix_add(A, volt_idx, n[0]-1, gm);
            break;
        }

        case COMP_CCII_PLUS:
        case COMP_CCII_MINUS: {
            // Current conveyor: simplified model
            double gain = comp->props.controlled_source.gain;
            double G = 1.0 / 100.0;  // 100 ohm
            STAMP_CONDUCTANCE(n[0], n[1], G);
            break;
        }

        // === CONTROLLED SOURCES ===

        case COMP_VCVS: {
            // Voltage-controlled voltage source
            double gain = comp->props.controlled_source.gain;
            double G_src = 1.0;  // Source conductance

            // Control input (n[0], n[1]) is high-impedance
            double G_in = 1e-12;
            STAMP_CONDUCTANCE(n[0], n[1], G_in);

            // Output (n[2], n[3]) follows control voltage times gain
            STAMP_CONDUCTANCE(n[2], n[3], G_src);
            if (n[2] > 0 && n[0] > 0) matrix_add(A, n[2]-1, n[0]-1, -G_src * gain);
            if (n[2] > 0 && n[1] > 0) matrix_add(A, n[2]-1, n[1]-1, G_src * gain);
            if (n[3] > 0 && n[0] > 0) matrix_add(A, n[3]-1, n[0]-1, G_src * gain);
            if (n[3] > 0 && n[1] > 0) matrix_add(A, n[3]-1, n[1]-1, -G_src * gain);
            break;
        }

        case COMP_VCCS: {
            // Voltage-controlled current source
            double gm = comp->props.controlled_source.gain;  // Transconductance (A/V)

            // Control input is high-impedance
            double G_in = 1e-12;
            STAMP_CONDUCTANCE(n[0], n[1], G_in);

            // Output current proportional to control voltage
            if (n[2] > 0 && n[0] > 0) matrix_add(A, n[2]-1, n[0]-1, gm);
            if (n[2] > 0 && n[1] > 0) matrix_add(A, n[2]-1, n[1]-1, -gm);
            if (n[3] > 0 && n[0] > 0) matrix_add(A, n[3]-1, n[0]-1, -gm);
            if (n[3] > 0 && n[1] > 0) matrix_add(A, n[3]-1, n[1]-1, gm);
            break;
        }

        case COMP_CCVS: {
            // Current-controlled voltage source
            double rm = comp->props.controlled_source.gain;  // Transresistance (V/A)
            double r_sense = comp->props.controlled_source.r_in;

            // Current sensing through low resistance
            double G_sense = 1.0 / r_sense;
            STAMP_CONDUCTANCE(n[0], n[1], G_sense);

            // Output voltage proportional to sensed current
            double G_src = 1.0;
            STAMP_CONDUCTANCE(n[2], n[3], G_src);
            // I_sense = G_sense * (V0 - V1), V_out = rm * I_sense
            if (n[2] > 0 && n[0] > 0) matrix_add(A, n[2]-1, n[0]-1, -G_src * rm * G_sense);
            if (n[2] > 0 && n[1] > 0) matrix_add(A, n[2]-1, n[1]-1, G_src * rm * G_sense);
            if (n[3] > 0 && n[0] > 0) matrix_add(A, n[3]-1, n[0]-1, G_src * rm * G_sense);
            if (n[3] > 0 && n[1] > 0) matrix_add(A, n[3]-1, n[1]-1, -G_src * rm * G_sense);
            break;
        }

        case COMP_CCCS: {
            // Current-controlled current source
            double gain = comp->props.controlled_source.gain;  // Current gain (A/A)
            double r_sense = comp->props.controlled_source.r_in;

            // Current sensing
            double G_sense = 1.0 / r_sense;
            STAMP_CONDUCTANCE(n[0], n[1], G_sense);

            // Output current proportional to sensed current
            if (n[2] > 0 && n[0] > 0) matrix_add(A, n[2]-1, n[0]-1, gain * G_sense);
            if (n[2] > 0 && n[1] > 0) matrix_add(A, n[2]-1, n[1]-1, -gain * G_sense);
            if (n[3] > 0 && n[0] > 0) matrix_add(A, n[3]-1, n[0]-1, -gain * G_sense);
            if (n[3] > 0 && n[1] > 0) matrix_add(A, n[3]-1, n[1]-1, gain * G_sense);
            break;
        }

        // === SWITCHES ===

        case COMP_DPDT_SWITCH: {
            // DPDT: two SPDT switches ganged together
            double r_on = comp->props.switch_spdt.r_on;
            double r_off = comp->props.switch_spdt.r_off;
            int pos = comp->props.switch_spdt.position;

            double R_ca = (pos == 0) ? r_on : r_off;
            double R_cb = (pos == 1) ? r_on : r_off;

            STAMP_CONDUCTANCE(n[0], n[2], conductance_of(R_ca));  // C1 to A
            STAMP_CONDUCTANCE(n[0], n[3], conductance_of(R_cb));  // C1 to B
            STAMP_CONDUCTANCE(n[1], n[2], conductance_of(R_cb));  // C2 to A (opposite)
            STAMP_CONDUCTANCE(n[1], n[3], conductance_of(R_ca));  // C2 to B (opposite)
            break;
        }

        case COMP_RELAY: {
            // Relay with coil inductance and hysteresis
            // Terminals: 0=C+ (coil+), 1=C- (coil-), 2=NO, 3=COM
            double R_coil = comp->props.relay.r_coil;
            double L_coil = comp->props.relay.l_coil;
            double i_pickup = comp->props.relay.i_pickup;
            double i_dropout = comp->props.relay.i_dropout;
            /* Read the state; do not advance it. The coil current and the armature are advanced
               once per accepted step, in simulation.c, next to every other companion - measured
               first, as the motor was: switched onto its 12 V supply, the default coil reached
               63 %% of V/R in 10.5 us where tau = L/R says 500 us. A relay that pulls in with no
               delay makes every delay circuit built on one a lie. */
            double I_prev = g_stamp_read_only ? comp->state_i_solve : comp->props.relay.i_coil;
            bool energized = comp->props.relay.energized;

            // --- Coil Circuit (R + L in series) ---
            if (comp->props.relay.ideal || L_coil < 1e-9) {
                // Ideal mode: just coil resistance, no inductance
                STAMP_CONDUCTANCE(n[0], n[1], conductance_of(R_coil));

            } else {
                // Non-ideal: R + L companion model
                // V = I*R + L*dI/dt -> V = I*R_eq + V_eq
                // where R_eq = R + L/dt, V_eq = L/dt * I_prev
                double R_eq = R_coil + L_coil / dt;
                double G = 1.0 / R_eq;
                STAMP_CONDUCTANCE(n[0], n[1], G);

                // Current source for inductor history (I_eq = I_prev * L/dt / R_eq)
                double I_eq = (L_coil / dt) * I_prev / R_eq;
                if (n[0] > 0) vector_add(b, n[0]-1, -I_eq);
                if (n[1] > 0) vector_add(b, n[1]-1, I_eq);

            }
            (void)i_pickup; (void)i_dropout;   /* hysteresis moved to the per-step advance */

            // --- Contact Circuit (NO to COM) ---
            double R_contact = energized ?
                              comp->props.relay.r_contact_on :
                              comp->props.relay.r_contact_off;
            STAMP_CONDUCTANCE(n[2], n[3], conductance_of(R_contact));
            break;
        }

        case COMP_ANALOG_SWITCH: {
            // Analog switch: controlled by voltage on control terminal
            double v_ctl = 0;
            if (prev_solution && n[2] > 0) {
                v_ctl = vector_get(prev_solution, n[2]-1);
            }

            bool on = comp->props.analog_switch.manual ? comp->props.analog_switch.state
                                                       : (v_ctl >= comp->props.analog_switch.v_on);
            comp->props.analog_switch.state = on;   // so a manual click starts from the state it is in
            double R = on ? comp->props.analog_switch.r_on : comp->props.analog_switch.r_off;
            STAMP_CONDUCTANCE(n[0], n[1], conductance_of(R));
            break;
        }

        // === LOGIC GATES ===

        case COMP_LOGIC_INPUT: {
            // Logic input: voltage source
            double V = comp->props.logic_input.state ?
                      comp->props.logic_input.v_high :
                      comp->props.logic_input.v_low;
            double R_out = comp->props.logic_input.r_out;

            // Model as voltage source with series resistance
            double G = 1.0 / R_out;
            if (n[0] > 0) {
                matrix_add(A, n[0]-1, n[0]-1, G);
                vector_add(b, n[0]-1, G * V);
            }
            break;
        }

        case COMP_LOGIC_OUTPUT: {
            // Logic output: high-impedance input (just observes voltage)
            double G = 1e-12;
            if (n[0] > 0) {
                matrix_add(A, n[0]-1, n[0]-1, G);
            }
            break;
        }

        case COMP_NOT_GATE:
        case COMP_BUFFER:
        case COMP_SCHMITT_INV:
        case COMP_SCHMITT_BUF: {
            // Single-input logic gate
            double v_th = comp->props.logic_gate.v_threshold;
            double v_low = comp->props.logic_gate.v_low;
            double v_high = comp->props.logic_gate.v_high;
            double r_out = comp->props.logic_gate.r_out;

            double v_in = 0;
            if (prev_solution && n[0] > 0) {
                v_in = vector_get(prev_solution, n[0]-1);
            }

            bool input_high = v_in >= v_th;
            bool output_high;

            if (comp->type == COMP_NOT_GATE || comp->type == COMP_SCHMITT_INV) {
                output_high = !input_high;
            } else {
                output_high = input_high;
            }

            double V_out = output_high ? v_high : v_low;
            double G = 1.0 / r_out;

            // High-impedance input
            if (n[0] > 0) matrix_add(A, n[0]-1, n[0]-1, 1e-12);

            // Output as voltage source with resistance
            if (n[1] > 0) {
                matrix_add(A, n[1]-1, n[1]-1, G);
                vector_add(b, n[1]-1, G * V_out);
            }
            break;
        }

        case COMP_AND_GATE:
        case COMP_NAND_GATE: {
            double v_th = comp->props.logic_gate.v_threshold;
            double v_low = comp->props.logic_gate.v_low;
            double v_high = comp->props.logic_gate.v_high;
            double r_out = comp->props.logic_gate.r_out;

            double v_a = 0, v_b = 0;
            if (prev_solution) {
                if (n[0] > 0) v_a = vector_get(prev_solution, n[0]-1);
                if (n[1] > 0) v_b = vector_get(prev_solution, n[1]-1);
            }

            bool a_high = v_a >= v_th;
            bool b_high = v_b >= v_th;
            bool result = a_high && b_high;
            if (comp->type == COMP_NAND_GATE) result = !result;

            double V_out = result ? v_high : v_low;
            double G = 1.0 / r_out;

            // High-impedance inputs
            if (n[0] > 0) matrix_add(A, n[0]-1, n[0]-1, 1e-12);
            if (n[1] > 0) matrix_add(A, n[1]-1, n[1]-1, 1e-12);

            // Output
            if (n[2] > 0) {
                matrix_add(A, n[2]-1, n[2]-1, G);
                vector_add(b, n[2]-1, G * V_out);
            }
            break;
        }

        case COMP_OR_GATE:
        case COMP_NOR_GATE: {
            double v_th = comp->props.logic_gate.v_threshold;
            double v_low = comp->props.logic_gate.v_low;
            double v_high = comp->props.logic_gate.v_high;
            double r_out = comp->props.logic_gate.r_out;

            double v_a = 0, v_b = 0;
            if (prev_solution) {
                if (n[0] > 0) v_a = vector_get(prev_solution, n[0]-1);
                if (n[1] > 0) v_b = vector_get(prev_solution, n[1]-1);
            }

            bool a_high = v_a >= v_th;
            bool b_high = v_b >= v_th;
            bool result = a_high || b_high;
            if (comp->type == COMP_NOR_GATE) result = !result;

            double V_out = result ? v_high : v_low;
            double G = 1.0 / r_out;

            if (n[0] > 0) matrix_add(A, n[0]-1, n[0]-1, 1e-12);
            if (n[1] > 0) matrix_add(A, n[1]-1, n[1]-1, 1e-12);

            if (n[2] > 0) {
                matrix_add(A, n[2]-1, n[2]-1, G);
                vector_add(b, n[2]-1, G * V_out);
            }
            break;
        }

        case COMP_XOR_GATE:
        case COMP_XNOR_GATE: {
            double v_th = comp->props.logic_gate.v_threshold;
            double v_low = comp->props.logic_gate.v_low;
            double v_high = comp->props.logic_gate.v_high;
            double r_out = comp->props.logic_gate.r_out;

            double v_a = 0, v_b = 0;
            if (prev_solution) {
                if (n[0] > 0) v_a = vector_get(prev_solution, n[0]-1);
                if (n[1] > 0) v_b = vector_get(prev_solution, n[1]-1);
            }

            bool a_high = v_a >= v_th;
            bool b_high = v_b >= v_th;
            bool result = a_high != b_high;  // XOR
            if (comp->type == COMP_XNOR_GATE) result = !result;

            double V_out = result ? v_high : v_low;
            double G = 1.0 / r_out;

            if (n[0] > 0) matrix_add(A, n[0]-1, n[0]-1, 1e-12);
            if (n[1] > 0) matrix_add(A, n[1]-1, n[1]-1, 1e-12);

            if (n[2] > 0) {
                matrix_add(A, n[2]-1, n[2]-1, G);
                vector_add(b, n[2]-1, G * V_out);
            }
            break;
        }

        case COMP_TRISTATE_BUF: {
            // Tri-state buffer: output can be high-impedance
            double v_th = comp->props.logic_gate.v_threshold;
            double v_low = comp->props.logic_gate.v_low;
            double v_high = comp->props.logic_gate.v_high;
            double r_out = comp->props.logic_gate.r_out;

            double v_in = 0, v_en = 0;
            if (prev_solution) {
                if (n[0] > 0) v_in = vector_get(prev_solution, n[0]-1);
                if (n[2] > 0) v_en = vector_get(prev_solution, n[2]-1);
            }

            bool enabled = v_en >= v_th;
            bool input_high = v_in >= v_th;

            if (n[0] > 0) matrix_add(A, n[0]-1, n[0]-1, 1e-12);
            if (n[2] > 0) matrix_add(A, n[2]-1, n[2]-1, 1e-12);

            if (enabled) {
                double V_out = input_high ? v_high : v_low;
                double G = 1.0 / r_out;
                if (n[1] > 0) {
                    matrix_add(A, n[1]-1, n[1]-1, G);
                    vector_add(b, n[1]-1, G * V_out);
                }
            } else {
                // High impedance output
                if (n[1] > 0) matrix_add(A, n[1]-1, n[1]-1, 1e-12);
            }
            break;
        }

        // === DIGITAL ICs - Simplified behavioral models ===

        case COMP_COUNTER: {
            /* CLK and RST are high impedance; Q0..Q3 and CARRY are driven from the count that
               logic_propagate_counter maintains. Five independent outputs, so this cannot use
               the shared two-output block below. */
            double v_high = comp->props.counter.v_high;
            double v_low = comp->props.counter.v_low;
            double G = 1.0 / (comp->props.counter.r_out > 0 ? comp->props.counter.r_out : 100.0);
            for (int i = 0; i < 2; i++)
                if (n[i] > 0) matrix_add(A, n[i]-1, n[i]-1, 1e-12);

            int mod = comp->props.counter.modulus > 0 ? comp->props.counter.modulus : 10;
            int cnt = comp->props.counter.count % mod;
            for (int bit = 0; bit < 4; bit++) {
                double V = (cnt & (1 << bit)) ? v_high : v_low;
                int t = 2 + bit;
                if (n[t] > 0) { matrix_add(A, n[t]-1, n[t]-1, G); vector_add(b, n[t]-1, G * V); }
            }
            double V_cy = (cnt == 0 && comp->props.counter.wrapped) ? v_high : v_low;
            if (n[6] > 0) { matrix_add(A, n[6]-1, n[6]-1, G); vector_add(b, n[6]-1, G * V_cy); }
            break;
        }

        case COMP_D_FLIPFLOP:
        case COMP_JK_FLIPFLOP:
        case COMP_T_FLIPFLOP:
        case COMP_SR_LATCH:
        case COMP_SHIFT_REG:
        case COMP_MUX_2TO1:
        case COMP_DEMUX_1TO2:
        case COMP_DECODER:
        case COMP_HALF_ADDER:
        case COMP_FULL_ADDER: {
            // Simplified: treat outputs as voltage sources based on state
            double v_high = comp->props.logic_gate.v_high;
            double v_low = comp->props.logic_gate.v_low;
            double r_out = comp->props.logic_gate.r_out;
            double G = 1.0 / r_out;

            // High-impedance inputs
            for (int i = 0; i < comp->num_terminals - 2; i++) {
                if (n[i] > 0) matrix_add(A, n[i]-1, n[i]-1, 1e-12);
            }

            // Outputs (last two terminals typically)
            double V_out = comp->props.logic_gate.state ? v_high : v_low;
            int out1 = comp->num_terminals - 2;
            int out2 = comp->num_terminals - 1;

            if (n[out1] > 0) {
                matrix_add(A, n[out1]-1, n[out1]-1, G);
                vector_add(b, n[out1]-1, G * V_out);
            }
            if (n[out2] > 0) {
                matrix_add(A, n[out2]-1, n[out2]-1, G);
                vector_add(b, n[out2]-1, G * (v_high - V_out + v_low));  // Complement
            }
            break;
        }

        case COMP_BCD_DECODER: {
            // BCD to 7-segment decoder (like 7447)
            // Terminals: 0-3 = A,B,C,D (BCD inputs), 4-10 = a,b,c,d,e,f,g (segment outputs)
            double v_high = comp->props.bcd_decoder.v_high;
            double v_low = comp->props.bcd_decoder.v_low;
            double v_thresh = comp->props.bcd_decoder.v_threshold;
            bool active_low = comp->props.bcd_decoder.active_low;
            double r_out = 100.0;
            double G = 1.0 / r_out;

            // Read BCD inputs (high-impedance)
            int bcd_value = 0;
            for (int i = 0; i < 4; i++) {
                if (n[i] > 0) {
                    matrix_add(A, n[i]-1, n[i]-1, 1e-12);
                    // Read input voltage from previous solution
                    if (prev_solution && prev_solution->data[n[i]-1] > v_thresh) {
                        bcd_value |= (1 << i);
                    }
                }
            }

            // BCD to 7-segment lookup table (common cathode: 1=on, 0=off)
            // Segments: a, b, c, d, e, f, g (bits 0-6)
            static const uint8_t seg_table[16] = {
                0x3F, // 0: abcdef
                0x06, // 1: bc
                0x5B, // 2: abdeg
                0x4F, // 3: abcdg
                0x66, // 4: bcfg
                0x6D, // 5: acdfg
                0x7D, // 6: acdefg
                0x07, // 7: abc
                0x7F, // 8: abcdefg
                0x6F, // 9: abcdfg
                0x77, // A: abcefg
                0x7C, // b: cdefg
                0x39, // C: adef
                0x5E, // d: bcdeg
                0x79, // E: adefg
                0x71  // F: aefg
            };

            uint8_t segments = seg_table[bcd_value & 0x0F];

            // Set outputs (terminals 4-10)
            for (int i = 0; i < 7; i++) {
                int term_idx = 4 + i;
                if (n[term_idx] > 0) {
                    bool seg_on = (segments >> i) & 1;
                    if (active_low) seg_on = !seg_on;
                    double V_out = seg_on ? v_high : v_low;
                    matrix_add(A, n[term_idx]-1, n[term_idx]-1, G);
                    vector_add(b, n[term_idx]-1, G * V_out);
                }
            }
            break;
        }

        // === MIXED SIGNAL ===

        case COMP_555_TIMER: {
            // 555 Timer: functional model with trigger/threshold comparators
            // Terminals: VCC(0), GND(1), TRG(2), THR(3), OUT(4)
            double r_out = 100.0;
            double G_out = 1.0 / r_out;
            double G_in = 1e-7;  // High impedance inputs (10M)

            // Get voltages from solution vector (for state update)
            double v_vcc = 0, v_gnd = 0, v_trig = 0, v_thresh = 0;
            if (prev_solution) {
                if (n[0] > 0) v_vcc = vector_get(prev_solution, n[0] - 1);
                if (n[1] > 0) v_gnd = vector_get(prev_solution, n[1] - 1);
                if (n[2] > 0) v_trig = vector_get(prev_solution, n[2] - 1);
                if (n[3] > 0) v_thresh = vector_get(prev_solution, n[3] - 1);
            }

            // Calculate supply voltage relative to GND
            double vcc = v_vcc - v_gnd;
            if (vcc < 0.5) vcc = comp->props.timer_555.vcc;  // Fallback

            // 555 internal comparator thresholds
            double v_trig_thresh = vcc / 3.0;    // Trigger threshold (1/3 VCC)
            double v_upper_thresh = 2.0 * vcc / 3.0;  // Upper threshold (2/3 VCC)

            // Relative voltages (referenced to GND)
            double v_trig_rel = v_trig - v_gnd;
            double v_thresh_rel = v_thresh - v_gnd;

            // Update internal flip-flop state based on comparators
            // TRIGGER < 1/3 VCC: SET output HIGH
            // THRESHOLD > 2/3 VCC: RESET output LOW
            if (v_trig_rel < v_trig_thresh) {
                comp->props.timer_555.output = true;  // SET
            }
            if (v_thresh_rel > v_upper_thresh) {
                comp->props.timer_555.output = false;  // RESET
            }

            // Output voltage based on flip-flop state
            double v_out = comp->props.timer_555.output ? (vcc - 0.3 + v_gnd) : (0.1 + v_gnd);

            // VCC input - small conductance to ground for stability
            if (n[0] > 0) matrix_add(A, n[0]-1, n[0]-1, G_in);

            // GND input - small conductance
            if (n[1] > 0) matrix_add(A, n[1]-1, n[1]-1, G_in);

            // TRIGGER input - high impedance to GND
            if (n[2] > 0) matrix_add(A, n[2]-1, n[2]-1, G_in);

            // THRESHOLD input - high impedance to GND
            if (n[3] > 0) matrix_add(A, n[3]-1, n[3]-1, G_in);

            // Output - voltage source behavior (low impedance output)
            if (n[4] > 0) {
                matrix_add(A, n[4]-1, n[4]-1, G_out);
                vector_add(b, n[4]-1, G_out * v_out);
            }
            break;
        }

        case COMP_DAC:
        case COMP_ADC:
        case COMP_VCO:
        case COMP_PLL:
        case COMP_MONOSTABLE: {
            // Simplified: output based on input
            double r_out = 100.0;
            double G = 1.0 / r_out;

            // High-impedance inputs
            for (int i = 0; i < comp->num_terminals - 1; i++) {
                if (n[i] > 0) matrix_add(A, n[i]-1, n[i]-1, 1e-12);
            }

            // Output
            int out_idx = comp->num_terminals - 1;
            if (comp->type == COMP_DAC) out_idx = 2;  // DAC output is terminal 2

            double V_out = 2.5;  // Default mid-rail
            if (n[out_idx] > 0) {
                matrix_add(A, n[out_idx]-1, n[out_idx]-1, G);
                vector_add(b, n[out_idx]-1, G * V_out);
            }
            break;
        }

        case COMP_OPTOCOUPLER: {
            // Optocoupler: LED on input, phototransistor on output
            // Input side (LED)
            double Is = 1e-20;
            double Vt = 0.026;
            double Vd = 0.6;
            if (prev_solution) {
                double v1 = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double v2 = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                Vd = CLAMP(v1 - v2, -0.5, 2.0);
            }
            double expTerm = exp(Vd / (2 * Vt));
            double Gd = (Is / (2 * Vt)) * expTerm + 1e-12;
            double Id = Is * (expTerm - 1);
            double Ieq = Id - Gd * Vd;

            STAMP_CONDUCTANCE(n[0], n[1], Gd);
            if (n[0] > 0) vector_add(b, n[0]-1, -Ieq);
            if (n[1] > 0) vector_add(b, n[1]-1, Ieq);

            // Output side: current source proportional to LED current
            double I_out = Id * 0.5;  // Current transfer ratio ~50%
            if (n[2] > 0) vector_add(b, n[2]-1, -I_out);
            if (n[3] > 0) vector_add(b, n[3]-1, I_out);
            break;
        }

        // === VOLTAGE REGULATORS ===

        case COMP_LM317: {
            // LM317: Adjustable voltage regulator
            // Vout = Vadj + 1.25V (reference voltage between OUT and ADJ)
            // Dropout voltage ~3V (Vin must be at least Vout + 3V)
            double v_ref = 1.25;
            double v_dropout = 2.5;  // Minimum IN-OUT voltage
            double r_out = 0.1;      // Low output impedance
            double G_out = 1.0 / r_out;
            double G_in = 1e-9;      // Input draws very little current at no load

            // Get voltages from solution vector
            double v_in = 0, v_adj = 0;
            if (prev_solution) {
                if (n[0] > 0) v_in = vector_get(prev_solution, n[0] - 1);
                if (n[2] > 0) v_adj = vector_get(prev_solution, n[2] - 1);
            }

            // Calculate desired output voltage (ADJ + 1.25V)
            double v_out_desired = v_adj + v_ref;

            // Check dropout condition: if Vin < Vout + dropout, regulator can't maintain output
            double v_out_max = v_in - v_dropout;
            double V_out = (v_out_desired < v_out_max) ? v_out_desired : v_out_max;
            if (V_out < 0) V_out = 0;  // Can't output negative

            // Input connection - small conductance for bias current
            if (n[0] > 0) matrix_add(A, n[0]-1, n[0]-1, G_in);

            // ADJ pin - very high impedance (draws ~50uA typically)
            if (n[2] > 0) matrix_add(A, n[2]-1, n[2]-1, 1e-12);

            // Output - voltage source behavior
            if (n[1] > 0) {
                matrix_add(A, n[1]-1, n[1]-1, G_out);
                vector_add(b, n[1]-1, G_out * V_out);
            }
            break;
        }

        case COMP_7805: {
            // 7805: Fixed 5V voltage regulator
            // Requires minimum ~7V input (2V dropout)
            // Terminals: IN(0), OUT(1), GND(2)
            double v_reg = 5.0;       // Regulated output voltage
            double v_dropout = 2.0;   // Minimum IN-OUT voltage
            double r_out = 0.1;       // Low output impedance
            double G_out = 1.0 / r_out;
            double G_in = 1e-9;       // Input draws very little current at no load

            // Get voltages from solution vector
            double v_in = 0, v_gnd = 0;
            if (prev_solution) {
                if (n[0] > 0) v_in = vector_get(prev_solution, n[0] - 1);
                if (n[2] > 0) v_gnd = vector_get(prev_solution, n[2] - 1);
            }

            // Calculate voltage relative to GND pin
            double v_in_rel = v_in - v_gnd;

            // Check dropout condition: need at least 7V (5V + 2V dropout)
            double V_out;
            if (v_in_rel >= v_reg + v_dropout) {
                // Normal regulation - output 5V above GND
                V_out = v_gnd + v_reg;
            } else if (v_in_rel > 0) {
                // Dropout - output follows input minus dropout
                V_out = v_in - v_dropout;
                if (V_out < v_gnd) V_out = v_gnd;  // Can't go below GND
            } else {
                // No input voltage
                V_out = v_gnd;
            }

            // Input connection - small conductance for bias current
            if (n[0] > 0) matrix_add(A, n[0]-1, n[0]-1, G_in);

            // GND pin - connection point
            if (n[2] > 0) matrix_add(A, n[2]-1, n[2]-1, 1e-12);

            // Output - voltage source behavior
            if (n[1] > 0) {
                matrix_add(A, n[1]-1, n[1]-1, G_out);
                vector_add(b, n[1]-1, G_out * V_out);
            }
            break;
        }

        case COMP_TL431: {
            // TL431: programmable shunt regulator. Terminals: n[0]=K (cathode), n[1]=A (anode),
            // n[2]=REF. The cathode current is a steep, smooth function of (V_REF - V_A) around
            // the 2.495 V internal reference, which behaves like a VCCS from K to A:
            //   I_ka = I0 * exp((V_ref - V_A - VREF) / VS)
            // with VS = 10 mV (~1/VS = 100 S transconductance near the knee). Ideal mode uses
            // a sharper knee; non-ideal adds the datasheet ~1 uA/2 uA REF bias current.
            const double VREF = 2.495;
            const double I0 = 1e-3;
            double VS = comp->props.zener.ideal ? 0.005 : 0.010;

            double Vr = VREF;  // Assume at the knee if no previous solution (good initial guess)
            if (prev_solution) {
                double vA = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                double vR = (n[2] > 0) ? vector_get(prev_solution, n[2]-1) : 0;
                Vr = vR - vA;
            }
            double xexp = (Vr - VREF) / VS;
            if (xexp > 20.0) xexp = 20.0;    // ~0.5 A cap: keeps exp() sane
            if (xexp < -40.0) xexp = -40.0;
            double Ika = I0 * exp(xexp);
            double Gr = Ika / VS;             // dI_ka / dV_ref
            double Ieq = Ika - Gr * Vr;

            // K->A current controlled by (V_REF - V_A): KCL rows K (+I) and A (-I)
            if (n[0] > 0 && n[2] > 0) matrix_add(A, n[0]-1, n[2]-1, Gr);
            if (n[0] > 0 && n[1] > 0) matrix_add(A, n[0]-1, n[1]-1, -Gr);
            if (n[1] > 0 && n[2] > 0) matrix_add(A, n[1]-1, n[2]-1, -Gr);
            if (n[1] > 0)             matrix_add(A, n[1]-1, n[1]-1, Gr);
            if (n[0] > 0) vector_add(b, n[0]-1, -Ieq);
            if (n[1] > 0) vector_add(b, n[1]-1, Ieq);

            // Small leakage K-A and REF input conductance keep the matrix well-conditioned
            STAMP_CONDUCTANCE(n[0], n[1], 1e-6);
            STAMP_CONDUCTANCE(n[2], n[1], comp->props.zener.ideal ? 1e-12 : 1e-6);
            break;
        }

        // === DISPLAY/OUTPUT ===

        case COMP_LAMP: {
            // Lamp: temperature-dependent resistance
            double R = comp->props.lamp.r_cold;  // Simplified: use cold resistance
            double G = 1.0 / R;
            STAMP_CONDUCTANCE(n[0], n[1], G);
            break;
        }

        case COMP_7SEG_DISPLAY: {
            // 7-segment display: terminals 0-3=a,b,c,d, 4=COM, 5-8=e,f,g,DP
            // Each segment is a diode from segment pin to COM
            double Is = 1e-20;
            // Calculate thermal voltage from global environment temperature
            double Vt = 8.617e-5 * (g_environment.temperature + 273.15);
            double nn = 2.0;
            double nVt = nn * Vt;
            int com = 4;  // COM is terminal 4

            // Segment terminals: 0,1,2,3 (a,b,c,d) and 5,6,7,8 (e,f,g,DP)
            int seg_terminals[] = {0, 1, 2, 3, 5, 6, 7, 8};
            for (int j = 0; j < 8; j++) {
                int i = seg_terminals[j];
                double Vd = 0.6;
                if (prev_solution) {
                    double v1 = (n[i] > 0) ? vector_get(prev_solution, n[i]-1) : 0;
                    double v2 = (n[com] > 0) ? vector_get(prev_solution, n[com]-1) : 0;
                    Vd = CLAMP(v1 - v2, -1, 3);
                }
                double expTerm = exp(Vd / nVt);
                double Gd = (Is / nVt) * expTerm + 1e-12;
                double Id = Is * (expTerm - 1);
                double Ieq = Id - Gd * Vd;

                /* Keep the segment current so the symbol can actually light up. Without this the
                   display drew the same dead outline whatever it was driven with. */
                comp->props.seven_seg.currents[j] = Id;
                if (Id > 0.0001) comp->props.seven_seg.segments |= (uint8_t)(1u << j);
                else             comp->props.seven_seg.segments &= (uint8_t)~(1u << j);

                STAMP_CONDUCTANCE(n[i], n[com], Gd);
                if (n[i] > 0) vector_add(b, n[i]-1, -Ieq);
                if (n[com] > 0) vector_add(b, n[com]-1, Ieq);
            }
            break;
        }

        case COMP_LED_ARRAY: {
            // LED bar graph array: 8 individual LEDs with common cathode
            // Terminals 0-7 are anodes, terminal 8 is common cathode
            // Each segment uses same Shockley diode model as COMP_LED
            double Is = comp->props.led_array.is;
            double nn = comp->props.led_array.n;
            double Vt = 8.617e-5 * (g_environment.temperature + 273.15);
            double nVt = nn * Vt;
            int com = 8;  // Common cathode terminal index
            double max_I = comp->props.led_array.max_current;

            for (int i = 0; i < 8; i++) {
                // Skip burned LEDs (open circuit)
                if (comp->props.led_array.failed[i]) {
                    comp->props.led_array.currents[i] = 0;
                    continue;
                }

                // Calculate diode voltage from previous solution
                // Use typical LED forward voltage (1.8V) as initial guess to prevent overcurrent spikes
                // This gives a good starting point for Newton-Raphson iteration
                double Vd = 1.8;
                if (prev_solution) {
                    double v1 = (n[i] > 0) ? vector_get(prev_solution, n[i]-1) : 0;
                    double v2 = (n[com] > 0) ? vector_get(prev_solution, n[com]-1) : 0;
                    Vd = v1 - v2;
                }

                // Clamp to prevent exp() overflow
                if (Vd < -5.0 * nVt) Vd = -5.0 * nVt;
                if (Vd > 40.0 * nVt) Vd = 40.0 * nVt;

                // Shockley diode equation with Newton-Raphson companion model
                double expTerm = exp(Vd / nVt);
                double Id = Is * (expTerm - 1);
                double Gd = (Is / nVt) * expTerm + 1e-12;  // Dynamic conductance + GMIN
                double Ieq = Id - Gd * Vd;  // Equivalent current source

                // Note: Current is now calculated in circuit.c after MNA solve using final voltages
                // This ensures accurate current display and brightness rendering

                // Check for overcurrent (burning) - only during transient simulation, not DC analysis
                // DC analysis uses large dt (1e9), transient uses small dt (<1s typically)
                if (dt < 1.0 && Id > max_I * 2.0 && Id > 0.001) {
                    comp->props.led_array.failed[i] = true;
                }

                // Stamp the companion model: conductance + current source
                STAMP_CONDUCTANCE(n[i], n[com], Gd);
                if (n[i] > 0) vector_add(b, n[i]-1, -Ieq);
                if (n[com] > 0) vector_add(b, n[com]-1, Ieq);
            }
            break;
        }

        case COMP_LED_MATRIX: {
            // LED Matrix 8x8: rows R0-R7 (terminals 0-7) are anodes
            // columns C0-C7 (terminals 8-15) are cathodes
            // Each LED(r,c) is connected between row r and column c
            double Is = 1e-20;
            // Calculate thermal voltage from global environment temperature
            double Vt = 8.617e-5 * (g_environment.temperature + 273.15);
            double nn = 2.0;
            double nVt = nn * Vt;

            // Model 64 LEDs (8x8 matrix)
            for (int row = 0; row < 8; row++) {
                for (int col = 0; col < 8; col++) {
                    int anode = row;       // Row terminal (anode)
                    int cathode = 8 + col; // Column terminal (cathode)

                    double Vd = 0.6;
                    if (prev_solution) {
                        double v1 = (n[anode] > 0) ? vector_get(prev_solution, n[anode]-1) : 0;
                        double v2 = (n[cathode] > 0) ? vector_get(prev_solution, n[cathode]-1) : 0;
                        Vd = CLAMP(v1 - v2, -1, 3);
                    }
                    double expTerm = exp(Vd / nVt);
                    double Gd = (Is / nVt) * expTerm + 1e-12;
                    double Id = Is * (expTerm - 1);
                    double Ieq = Id - Gd * Vd;

                    STAMP_CONDUCTANCE(n[anode], n[cathode], Gd);
                    if (n[anode] > 0) vector_add(b, n[anode]-1, -Ieq);
                    if (n[cathode] > 0) vector_add(b, n[cathode]-1, Ieq);
                }
            }
            break;
        }

        case COMP_DC_MOTOR: {
            // DC Motor: R_a + L_a in series with back-EMF voltage source
            // V = I*R_a + L_a*dI/dt + V_bemf where V_bemf = kv * omega
            // Motor dynamics: J*d(omega)/dt = kt*I - b*omega - T_load
            double R_a = comp->props.dc_motor.r_armature;
            double L_a = comp->props.dc_motor.l_armature;
            double kv = comp->props.dc_motor.kv;
            double kt = comp->props.dc_motor.kt;
            double J = comp->props.dc_motor.j_rotor;
            double b_f = comp->props.dc_motor.b_friction;
            double T_load = comp->props.dc_motor.torque_load;

            /* Read the state; do not advance it. The rotor's speed and the armature current are
               advanced once per accepted step, in simulation.c, next to every other companion.

               They used to be integrated here, and a stamp runs once per Newton iteration. Each
               iteration therefore advanced the rotor, and the iteration only stops when nothing is
               changing any more - which for a motor is its steady state. So a time step did not
               advance the motor by dt, it drove it to where it was heading: 63 % of final speed in
               a single step of 49.5 us where the mechanical time constant is 99 ms. The final
               speed was exactly right, which is why nothing noticed - d_omega is zero at steady
               state however many times it is added, so every check that looked at where a motor
               ends up was satisfied. --dvdt-test times the getting there instead. */
            double omega_prev = g_stamp_read_only ? comp->state_w_solve : comp->props.dc_motor.omega;
            double I_prev     = g_stamp_read_only ? comp->state_i_solve : comp->props.dc_motor.current;

            // Back-EMF voltage
            double V_bemf = kv * omega_prev;
            if (!g_stamp_read_only) comp->props.dc_motor.v_bemf = V_bemf;
            (void)kt; (void)J; (void)b_f; (void)T_load;

            // Stamp armature circuit: R_a + L_a with back-EMF
            // Use companion model: V = I*R_eq + V_eq where R_eq = R_a + L_a/dt
            double Req = R_a + L_a / dt;
            double G = 1.0 / Req;
            STAMP_CONDUCTANCE(n[0], n[1], G);

            // Back-EMF acts as voltage source in series (reduces current)
            // Current source equivalent for back-EMF: I_bemf = V_bemf / Req
            double I_bemf = V_bemf / Req;
            // Also add previous inductor current contribution
            double I_L_prev = (L_a / dt) * I_prev / Req;
            double I_eq = I_bemf - I_L_prev;

            if (n[0] > 0) vector_add(b, n[0]-1, -I_eq);
            if (n[1] > 0) vector_add(b, n[1]-1, I_eq);

            break;
        }

        // === WIRELESS ===

        case COMP_ANTENNA_TX: {
            // TX Antenna: Measures voltage across terminals and broadcasts on channel
            // Acts as a high-impedance voltmeter that stores voltage to wireless channel
            double R_series = comp->props.antenna.ideal ? 1e-6 : comp->props.antenna.r_series;
            double G = 1.0 / R_series;

            // Stamp as high-impedance load
            STAMP_CONDUCTANCE(n[0], n[1], G);

            // Read voltage from previous solution and broadcast to channel
            if (prev_solution) {
                double v1 = (n[0] > 0) ? vector_get(prev_solution, n[0] - 1) : 0.0;
                double v2 = (n[1] > 0) ? vector_get(prev_solution, n[1] - 1) : 0.0;
                double v_diff = (v1 - v2) * comp->props.antenna.gain;
                comp->props.antenna.voltage = v_diff;

                // Contribute to wireless channel (will be averaged if multiple TX)
                int ch = comp->props.antenna.channel;
                if (ch >= 0 && ch < WIRELESS_CHANNEL_COUNT) {
                    g_wireless.voltage[ch] += v_diff;
                    g_wireless.tx_count[ch]++;
                }
            }
            break;
        }

        case COMP_ANTENNA_RX: {
            // RX Antenna: Receives voltage from wireless channel and outputs it
            // Acts as a voltage source with the received signal
            double R_series = comp->props.antenna.ideal ? 1e-6 : comp->props.antenna.r_series;
            double G = 1.0 / R_series;

            // Get voltage from wireless channel
            int ch = comp->props.antenna.channel;
            double V_rx = 0.0;
            if (ch >= 0 && ch < WIRELESS_CHANNEL_COUNT && g_wireless.tx_count[ch] > 0) {
                // Average voltage from all TX on this channel
                V_rx = (g_wireless.voltage[ch] / g_wireless.tx_count[ch]) * comp->props.antenna.gain;
            }
            comp->props.antenna.voltage = V_rx;

            // Stamp as voltage source with series resistance
            STAMP_CONDUCTANCE(n[0], n[1], G);

            // Stamp current source for received voltage
            double I_eq = V_rx * G;
            if (n[0] > 0) vector_add(b, n[0] - 1, I_eq);
            if (n[1] > 0) vector_add(b, n[1] - 1, -I_eq);
            break;
        }

        // === WIRING ===

        case COMP_BUS:
        case COMP_BUS_TAP: {
            // Bus and Bus Tap: essentially short circuits connecting terminals
            // Very low resistance to pass signals through
            double G = 1e6;  // 1 micro-ohm resistance
            STAMP_CONDUCTANCE(n[0], n[1], G);
            break;
        }

        // === MEASUREMENT ===

        case COMP_VOLTMETER: {
            // Voltmeter: high-impedance (minimal loading on circuit)
            // Readings are calculated in circuit_update_meter_readings() after solve
            double G = comp->props.voltmeter.ideal ? 1e-12 : (1.0 / comp->props.voltmeter.r_in);
            STAMP_CONDUCTANCE(n[0], n[1], G);
            break;
        }

        case COMP_AMMETER: {
            // Ammeter: low-impedance shunt (series element)
            // Use 1uOhm (1e-6) for ideal - acts as effective short circuit
            // Extended precision is used in circuit_update_meter_readings() to handle
            // the tiny voltage drops across such low resistances
            // Readings are calculated in circuit_update_meter_readings() after solve
            double R = comp->props.ammeter.ideal ? 1e-6 : comp->props.ammeter.r_shunt;
            if (R < 1e-9) R = 1e-9;  // Absolute minimum for numerical stability
            double G = 1.0 / R;
            STAMP_CONDUCTANCE(n[0], n[1], G);
            break;
        }

        case COMP_WATTMETER: {
            // Wattmeter: voltage sensing (high-Z) and current sensing (low-Z)
            // Readings are calculated in circuit_update_meter_readings() after solve
            double G_v = 1e-12;  // Voltage input (high impedance)
            double R_i = 0.001;  // Current shunt (1mOhm, low impedance)
            double G_i = 1.0 / R_i;
            STAMP_CONDUCTANCE(n[0], n[1], G_v);
            STAMP_CONDUCTANCE(n[2], n[3], G_i);
            break;
        }

        case COMP_TEST_POINT:
        case COMP_LABEL: {
            // Test point/Label: just a node marker, infinite impedance
            if (n[0] > 0) matrix_add(A, n[0]-1, n[0]-1, 1e-15);
            break;
        }

        case COMP_SUBCIRCUIT: {
            // Subcircuit: expand and stamp internal components
            // Find the subcircuit definition
            SubCircuitDef *def = NULL;
            for (int i = 0; i < g_subcircuit_library.count; i++) {
                if (g_subcircuit_library.defs[i].id == comp->props.subcircuit.def_id) {
                    def = &g_subcircuit_library.defs[i];
                    break;
                }
            }

            if (!def || !def->component_data || def->num_components == 0) {
                break;  // No definition found or empty
            }

            // External global counter for allocating subcircuit internal node indices
            extern int g_subcircuit_internal_node_offset;

            // Create node remapping table: internal_node_id -> matrix index
            // -1 means not yet assigned, 0 means ground
            int node_remap[MAX_NODES];
            for (int i = 0; i < MAX_NODES; i++) {
                node_remap[i] = -1;  // Not yet assigned
            }

            // First, map pin internal nodes to external circuit nodes
            for (int i = 0; i < def->num_pins && i < comp->num_terminals; i++) {
                int internal_id = def->pins[i].internal_node_id;
                int external_id = comp->node_ids[i];
                if (internal_id <= 0 || internal_id >= MAX_NODES) continue;
                /* external_id 0 is ground, and a pin tied to ground means the internal node IS
                   ground - index 0. Skipping it left the node unmapped, so the loop below gave
                   it a fresh internal index and the inside of the block floated: a divider
                   inside another block came out at the supply rail instead of a third of it. */
                node_remap[internal_id] = (external_id > 0) ? node_map[external_id] : 0;
            }

            // Then, allocate matrix indices for non-pin internal nodes
            // Collect all internal node IDs used by components
            Component *internal_comps = (Component *)def->component_data;
            for (int c_idx = 0; c_idx < def->num_components; c_idx++) {
                Component *ic = &internal_comps[c_idx];
                for (int t = 0; t < ic->num_terminals && t < MAX_TERMINALS; t++) {
                    int orig_node = ic->node_ids[t];
                    if (orig_node > 0 && orig_node < MAX_NODES && node_remap[orig_node] == -1) {
                        // This internal node hasn't been assigned yet - allocate new index
                        node_remap[orig_node] = g_subcircuit_internal_node_offset++;
                    }
                }
            }

            /* Stamp this instance's own components. They keep their state between steps, and
               each one that needs an auxiliary row gets one out of the block reserved for this
               block (component_aux_count told the solver how many to set aside). */
            int inst_n = 0;
            Component *inst = subcircuit_instance(comp, def, &inst_n);
            if (!inst) break;

            int dummy_node_map[MAX_NODES];
            for (int i = 0; i < MAX_NODES; i++) dummy_node_map[i] = i;   /* identity: already indices */

            int aux_used = 0;
            for (int c_idx = 0; c_idx < inst_n; c_idx++) {
                Component *ic = &inst[c_idx];
                const Component *src_ic = &internal_comps[c_idx];

                /* A block inside a block is stamped like anything else: this case expands it,
                   and its pins arrive already carrying matrix indices, which is exactly what
                   the identity node map below hands back. */
                if (ic->type == COMP_PIN || ic->type == COMP_LABEL ||
                    ic->type == COMP_TEST_POINT) {
                    continue;
                }
                if (ic->type == COMP_SUBCIRCUIT && g_subcircuit_depth >= SUBCIRCUIT_MAX_DEPTH) {
                    continue;                      /* a definition that contains itself */
                }

                /* remap from the DEFINITION's node ids (the instance's were overwritten with
                   matrix indices on the previous pass, and the indices move every solve) */
                for (int t = 0; t < ic->num_terminals && t < MAX_TERMINALS; t++) {
                    int orig_node = src_ic->node_ids[t];
                    if (orig_node > 0 && orig_node < MAX_NODES) {
                        int mapped = node_remap[orig_node];
                        ic->node_ids[t] = (mapped >= 0) ? mapped : 0;
                    } else {
                        ic->node_ids[t] = 0;   // Ground
                    }
                }

                int need = component_aux_count(ic);   /* a nested block asks for its whole tree */
                if (need > 0) {
                    ic->voltage_var_idx = comp->voltage_var_idx + aux_used;
                    aux_used += need;
                }

                g_subcircuit_depth++;
                component_stamp(ic, A, b, dummy_node_map, num_nodes, time, prev_solution, dt);
                g_subcircuit_depth--;
            }
            break;
        }

        default:
            break;
    }
}

void format_engineering(double value, const char *unit, char *buf, size_t buf_size) {
    static const struct { double exp; const char *prefix; } prefixes[] = {
        {1e12, "T"}, {1e9, "G"}, {1e6, "M"}, {1e3, "k"},
        {1, ""}, {1e-3, "m"}, {1e-6, "u"}, {1e-9, "n"}, {1e-12, "p"}
    };

    double abs_val = fabs(value);
    if (abs_val == 0) {
        snprintf(buf, buf_size, "0 %s", unit);
        return;
    }

    for (size_t i = 0; i < sizeof(prefixes)/sizeof(prefixes[0]); i++) {
        if (abs_val >= prefixes[i].exp * 0.999) {
            double scaled = value / prefixes[i].exp;
            if (fabs(scaled) < 10)
                snprintf(buf, buf_size, "%.2f %s%s", scaled, prefixes[i].prefix, unit);
            else if (fabs(scaled) < 100)
                snprintf(buf, buf_size, "%.1f %s%s", scaled, prefixes[i].prefix, unit);
            else
                snprintf(buf, buf_size, "%.0f %s%s", scaled, prefixes[i].prefix, unit);
            return;
        }
    }

    snprintf(buf, buf_size, "%.2e %s", value, unit);
}

void component_get_value_string(Component *comp, char *buf, size_t buf_size) {
    if (!comp || !buf) return;

    switch (comp->type) {
        case COMP_DC_VOLTAGE:
            format_engineering(comp->props.dc_voltage.voltage, "V", buf, buf_size);
            break;
        case COMP_AC_VOLTAGE:
            format_engineering(comp->props.ac_voltage.amplitude, "V", buf, buf_size);
            break;
        case COMP_ARB_SOURCE:
            snprintf(buf, buf_size, "tbl%d %d pts", comp->props.arb_source.table, arb_table_len(comp->props.arb_source.table));
            break;
        case COMP_DC_CURRENT:
            format_engineering(comp->props.dc_current.current, "A", buf, buf_size);
            break;
        case COMP_RESISTOR:
            format_engineering(comp->props.resistor.resistance, "Ohm", buf, buf_size);
            break;
        case COMP_CAPACITOR:
            format_engineering(comp->props.capacitor.capacitance, "F", buf, buf_size);
            break;
        case COMP_TOROID:
            format_engineering(toroid_capacitance(comp), "F", buf, buf_size);
            break;
        case COMP_SOURCE_3PH:
            format_engineering(comp->props.source_3ph.v_peak, "V", buf, buf_size);
            break;
        case COMP_TLINE:
            snprintf(buf, buf_size, "%.4g mi %s", comp->props.tline.length_mi,
                     comp->props.tline.model == 0 ? "R" : comp->props.tline.model == 1 ? "RL" : "pi");
            break;
        case COMP_SPARK_GAP:
            format_engineering(spark_gap_breakdown(comp), "V", buf, buf_size);
            break;
        case COMP_INDUCTOR:
            format_engineering(comp->props.inductor.inductance, "H", buf, buf_size);
            break;
        case COMP_SQUARE_WAVE:
            format_engineering(comp->props.square_wave.amplitude, "V", buf, buf_size);
            break;
        case COMP_TRIANGLE_WAVE:
            format_engineering(comp->props.triangle_wave.amplitude, "V", buf, buf_size);
            break;
        case COMP_SAWTOOTH_WAVE:
            format_engineering(comp->props.sawtooth_wave.amplitude, "V", buf, buf_size);
            break;
        case COMP_NOISE_SOURCE:
            format_engineering(comp->props.noise_source.amplitude, "V", buf, buf_size);
            break;
        default:
            buf[0] = '\0';
            break;
    }
}
