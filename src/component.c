/**
 * Circuit Playground - Component Implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "component.h"

// Global environment state (affects LDR and thermistor components)
EnvironmentState g_environment = {
    .light_level = 0.5,     // Default: medium light (0.0=dark to 1.0=bright)
    .temperature = 25.0     // Default: room temperature (°C)
};

// Global sub-circuit library
SubCircuitLibrary g_subcircuit_library = {
    .count = 0,
    .next_id = 1
};

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
            .ideal = true
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
            .is = 1e-20,
            .vt = 0.026,
            .n = 2.0,               // Higher ideality for LED
            .vf = 2.0,              // 2.0V forward voltage (red)
            .max_current = 0.020,   // 20 mA max
            .wavelength = 620,      // Red (620 nm)
            .current = 0.0,
            .ideal = true
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
            .r_off = 1e9,           // 1 GOhm off-resistance
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
            .r_off = 1e9,
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
            .r_off = 1e9
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
        { .capacitor = {
            .capacitance = 20e-12,      // Equivalent series capacitance
            .ideal = true
        }}
    },

    [COMP_SPARK_GAP] = {
        "Spark Gap", "SG", 2,
        {{ -40, 0, "1" }, { 40, 0, "2" }},
        80, 30,
        { .zener = {
            .vz = 90.0,                 // Breakdown voltage
            .rz = 1.0,
            .ideal = true
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
        {{ -40, -15, "A" }, { -40, 15, "B" }, { 40, 0, "OUT" }},
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
        {{ -40, -15, "A" }, { -40, 15, "B" }, { 40, 0, "OUT" }},
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
        {{ -40, -15, "A" }, { -40, 15, "B" }, { 40, 0, "OUT" }},
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
        {{ -40, -15, "A" }, { -40, 15, "B" }, { 40, 0, "OUT" }},
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
        {{ -40, -15, "A" }, { -40, 15, "B" }, { 40, 0, "OUT" }},
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
        {{ -40, -15, "A" }, { -40, 15, "B" }, { 40, 0, "OUT" }},
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

    [COMP_COUNTER] = {
        "Counter", "CNT", 3,
        {{ -40, 0, "CLK" }, { 40, -20, "Q0" }, { 40, 20, "Q1" }},
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
            .is = 1e-12,  // Saturation current (same as LED)
            .n = 2.0,     // Ideality factor (same as LED)
            .vf = 2.0,
            .max_current = 0.02,
            .currents = {0, 0, 0, 0, 0, 0, 0, 0},
            .failed = {false, false, false, false, false, false, false, false},
            .color = 1  // Default to green (like classic bar graph displays)
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

    [COMP_SPEAKER] = {
        "Speaker", "SPK", 2,
        {{ -40, -10, "+" }, { -40, 10, "-" }},
        80, 50,
        { .speaker = {
            .impedance = 8.0,           // 8 ohm speaker
            .sensitivity = 90.0,        // 90 dB/W/m
            .max_power = 1.0,           // 1W max
            .power_dissipated = 0.0,
            .voltage = 0.0,
            .current = 0.0,
            .frequency = 0.0,
            .audio_enabled = true,
            .failed = false
        }}
    },

    [COMP_MICROPHONE] = {
        "Microphone", "MIC", 2,
        {{ 40, 0, "+" }, { -40, 0, "-" }},
        80, 40,
        { .microphone = {
            .amplitude = 5.0,           // 5V peak output
            .offset = 2.5,              // 2.5V DC offset (centered)
            .gain = 1.0,                // Unity gain
            .r_series = 1000.0,         // 1K output resistance
            .voltage = 0.0,
            .peak_level = 0.0,
            .enabled = true,
            .ideal = false
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

const ComponentTypeInfo *component_get_info(ComponentType type) {
    if (type >= 0 && type < COMP_TYPE_COUNT) {
        return &component_info[type];
    }
    return &component_info[COMP_NONE];
}

Component *component_create(ComponentType type, float x, float y) {
    if (type <= COMP_NONE || type >= COMP_TYPE_COUNT) {
        return NULL;
    }

    Component *comp = calloc(1, sizeof(Component));
    if (!comp) return NULL;

    const ComponentTypeInfo *info = component_get_info(type);

    comp->id = next_component_id++;
    comp->type = type;
    comp->x = x;
    comp->y = y;
    comp->rotation = 0;
    comp->num_terminals = info->num_terminals;
    comp->props = info->default_props;

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

    // Set default label
    snprintf(comp->label, MAX_LABEL_LEN, "%s%d", info->short_name, comp->id);

    // Determine if component needs voltage variable (voltage sources, inductors)
    comp->needs_voltage_var = (type == COMP_DC_VOLTAGE ||
                               type == COMP_AC_VOLTAGE ||
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
                               type == COMP_PWM_SOURCE);

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
            comp->thermal.max_temperature = 155.0;   // Typical resistor max temp
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
        case COMP_SPEAKER:
            comp->thermal.max_temperature = 120.0;
            comp->thermal.thermal_mass = 1.0;        // Larger thermal mass
            comp->thermal.thermal_resistance = 50.0;
            break;
        default:
            comp->thermal.max_temperature = 150.0;
            comp->thermal.thermal_mass = 0.1;
            comp->thermal.thermal_resistance = 100.0;
            break;
    }

    return comp;
}

void component_free(Component *comp) {
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

    // Apply rotation
    double rad = comp->rotation * M_PI / 180.0;
    double cos_r = cos(rad);
    double sin_r = sin(rad);

    *x = comp->x + dx * cos_r - dy * sin_r;
    *y = comp->y + dx * sin_r + dy * cos_r;
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
            vector_add(b, volt_idx, V);
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

            double V = amp * sin(2 * M_PI * freq * time + phase) + offset;
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

        case COMP_DC_CURRENT: {
            double I = comp->props.dc_current.current;
            // Apply current sweep if enabled
            I = sweep_get_value(&comp->props.dc_current.current_sweep, I, time);
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
        case COMP_CAPACITOR_ELEC: {
            // Backward Euler companion model for capacitor:
            // i_C = C * dv/dt ≈ C * (v - v_prev) / dt = Geq * v - Ieq
            // where Geq = C/dt and Ieq = C * v_prev / dt
            double C = (comp->type == COMP_CAPACITOR) ?
                       comp->props.capacitor.capacitance :
                       comp->props.capacitor_elec.capacitance;
            double Geq = C / dt;
            double Ieq = 0;

            if (prev_solution) {
                double v1 = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double v2 = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                Ieq = C * (v1 - v2) / dt;
            }

            STAMP_CONDUCTANCE(n[0], n[1], Geq);
            // Ieq represents the capacitor's "memory" current
            // Positive Ieq means capacitor was charged (n1 > n2), so it sources current at n1
            if (n[0] > 0) vector_add(b, n[0]-1, Ieq);
            if (n[1] > 0) vector_add(b, n[1]-1, -Ieq);
            break;
        }

        case COMP_INDUCTOR: {
            double L = comp->props.inductor.inductance;
            double Req = L / dt;
            double Veq = 0;
            int curr_idx = num_nodes + comp->voltage_var_idx;

            if (prev_solution && curr_idx < prev_solution->size) {
                double Iprev = vector_get(prev_solution, curr_idx);
                Veq = L * Iprev / dt;
            }

            if (n[0] > 0) {
                matrix_add(A, curr_idx, n[0]-1, 1);
                matrix_add(A, n[0]-1, curr_idx, 1);
            }
            if (n[1] > 0) {
                matrix_add(A, curr_idx, n[1]-1, -1);
                matrix_add(A, n[1]-1, curr_idx, -1);
            }
            matrix_add(A, curr_idx, curr_idx, -Req);
            vector_add(b, curr_idx, Veq);
            break;
        }

        case COMP_DIODE: {
            double Is = comp->props.diode.is;
            // Calculate thermal voltage from global environment temperature
            // Vt = k*T/q where k/q = 8.617e-5 V/K
            double Vt = 8.617e-5 * (g_environment.temperature + 273.15);
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
            double Gd = (Is / nVt) * expTerm;
            double Ieq = Id - Gd * Vd;

            STAMP_CONDUCTANCE(n[0], n[1], Gd);
            if (n[0] > 0) vector_add(b, n[0]-1, -Ieq);
            if (n[1] > 0) vector_add(b, n[1]-1, Ieq);
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

            double Gd, Ieq;
            if (Vd >= 0) {
                // Forward bias - normal diode behavior
                Vd = CLAMP(Vd, 0, 40*nVt);
                double expTerm = exp(Vd / nVt);
                double Id = Is * (expTerm - 1);
                Gd = (Is / nVt) * expTerm + 1e-12;
                Ieq = Id - Gd * Vd;
            } else {
                // Reverse bias - breakdown at Vz
                double Vrev = -Vd;
                if (Vrev > Vz) {
                    // In breakdown region
                    Gd = 1.0;  // Low impedance
                    Ieq = -(Vz * Gd - Gd * Vd);
                } else {
                    // Before breakdown
                    Gd = 1e-12;  // Very high impedance
                    Ieq = 0;
                }
            }

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
            if (prev_solution) {
                double vB = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double vC = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                double vE = (n[2] > 0) ? vector_get(prev_solution, n[2]-1) : 0;
                Vbe = sign * (vB - vE);
                Vbc = sign * (vB - vC);
                Vbe = CLAMP(Vbe, -5*nf*Vt, 40*nf*Vt);
                Vbc = CLAMP(Vbc, -5*nf*Vt, 40*nf*Vt);
            }

            double Gbe, Gbc, Gm, Ieq_be, Ieq_bc;

            if (ideal) {
                // Ideal Ebers-Moll model (simplified)
                double expBE = exp(Vbe / (nf * Vt));
                double Ibe = (Is / bf) * (expBE - 1);
                Gbe = (Is / (bf * nf * Vt)) * expBE + 1e-12;
                Ieq_be = Ibe - Gbe * Vbe;

                // Collector current - forward active
                double Ic = Is * (expBE - 1);
                Gm = (Is / (nf * Vt)) * expBE;

                // Simplified: ignore B-C junction for ideal mode
                Gbc = 1e-12;
                Ieq_bc = 0;
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
                    double Vce = Vbe - Vbc;
                    early_factor = 1.0 + Vce / Vaf;
                }
                double Ic_f = Is * (expBE - 1) * early_factor;
                double Ic_r = Is * (expBC - 1);
                Gm = (Is / (nf * Vt)) * expBE * early_factor;
            }

            // Apply sign for PNP
            Gbe *= 1;  // Conductance is always positive
            Gm *= sign;
            Ieq_be *= sign;
            Ieq_bc *= sign;

            // Stamp B-E junction
            STAMP_CONDUCTANCE(n[0], n[2], Gbe);
            if (n[0] > 0) vector_add(b, n[0]-1, -Ieq_be);
            if (n[2] > 0) vector_add(b, n[2]-1, Ieq_be);

            // Stamp B-C junction (for non-ideal mode)
            if (!ideal) {
                STAMP_CONDUCTANCE(n[0], n[1], Gbc);
                if (n[0] > 0) vector_add(b, n[0]-1, -Ieq_bc);
                if (n[1] > 0) vector_add(b, n[1]-1, Ieq_bc);
            }

            // Transconductance (collector current controlled by Vbe)
            if (n[1] > 0 && n[0] > 0) matrix_add(A, n[1]-1, n[0]-1, Gm);
            if (n[1] > 0 && n[2] > 0) matrix_add(A, n[1]-1, n[2]-1, -Gm);
            if (n[2] > 0 && n[0] > 0) matrix_add(A, n[2]-1, n[0]-1, -Gm);
            if (n[2] > 0 && n[2] > 0) matrix_add(A, n[2]-1, n[2]-1, Gm);
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

            double Vgs = 0, Vds = 0, Vsb = 0;
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

            // Equivalent current source: Ieq = Id - Gm*Vgs - Gds*Vds
            Ieq = Id - Gm * Vgs - Gds * Vds;

            // Apply sign for PMOS (currents flow opposite direction)
            Gm *= sign;
            Ieq *= sign;

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

                // Get current voltages for capacitor integration
                double vG = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double vD = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                double vS = (n[2] > 0) ? vector_get(prev_solution, n[2]-1) : 0;

                double Vgs_curr = vG - vS;
                double Vgd_curr = vG - vD;

                // Trapezoidal integration: G_eq = 2*C/dt, I_eq = G_eq*V_prev + I_prev
                double G_cgs = 2.0 * Cgs / dt;
                double G_cgd = 2.0 * Cgd / dt;

                // Equivalent currents from previous timestep
                double I_cgs_eq = G_cgs * comp->props.mosfet.vgs_prev + comp->props.mosfet.i_cgs;
                double I_cgd_eq = G_cgd * comp->props.mosfet.vgd_prev + comp->props.mosfet.i_cgd;

                // Stamp Cgs (between gate n[0] and source n[2])
                STAMP_CONDUCTANCE(n[0], n[2], G_cgs);
                if (n[0] > 0) vector_add(b, n[0]-1, -I_cgs_eq);
                if (n[2] > 0) vector_add(b, n[2]-1, I_cgs_eq);

                // Stamp Cgd (between gate n[0] and drain n[1])
                STAMP_CONDUCTANCE(n[0], n[1], G_cgd);
                if (n[0] > 0) vector_add(b, n[0]-1, -I_cgd_eq);
                if (n[1] > 0) vector_add(b, n[1]-1, I_cgd_eq);

                // Update state for next iteration
                comp->props.mosfet.i_cgs = G_cgs * Vgs_curr - I_cgs_eq;
                comp->props.mosfet.i_cgd = G_cgd * Vgd_curr - I_cgd_eq;
                comp->props.mosfet.vgs_prev = Vgs_curr;
                comp->props.mosfet.vgd_prev = Vgd_curr;
            }
            break;
        }

        case COMP_OPAMP: {
            double A_gain = comp->props.opamp.gain;
            int volt_idx = num_nodes + comp->voltage_var_idx;

            // VCVS model: Vout = A * (V+ - V-)
            // For COMP_OPAMP: n[0]="-", n[1]="+", n[2]="OUT"
            if (n[2] > 0) {
                matrix_add(A, volt_idx, n[2]-1, 1);
                matrix_add(A, n[2]-1, volt_idx, 1);
            }
            if (n[1] > 0) matrix_add(A, volt_idx, n[1]-1, -A_gain);
            if (n[0] > 0) matrix_add(A, volt_idx, n[0]-1, A_gain);
            break;
        }

        case COMP_OPAMP_FLIPPED: {
            double A_gain = comp->props.opamp.gain;
            int volt_idx = num_nodes + comp->voltage_var_idx;

            // VCVS model: Vout = A * (V+ - V-)
            // For COMP_OPAMP_FLIPPED: n[0]="+", n[1]="-", n[2]="OUT"
            if (n[2] > 0) {
                matrix_add(A, volt_idx, n[2]-1, 1);
                matrix_add(A, n[2]-1, volt_idx, 1);
            }
            if (n[0] > 0) matrix_add(A, volt_idx, n[0]-1, -A_gain);  // + input
            if (n[1] > 0) matrix_add(A, volt_idx, n[1]-1, A_gain);   // - input
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

            // Square wave: high for duty cycle, low otherwise
            double V = (t_norm < duty) ? (amp + offset) : (-amp + offset);
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
            // Transformer model using VCVS (voltage-controlled voltage source) approach
            // Terminals: P1 (n[0]), P2 (n[1]), S1 (n[2]), S2 (n[3])
            // Relationship: V_secondary = N * V_primary
            // where N = turns_ratio = N_secondary / N_primary
            double N = comp->props.transformer.turns_ratio;

            if (comp->props.transformer.ideal) {
                // Ideal transformer using VCVS with series resistance
                // V_s = N * V_p, modeled as current source that enforces voltage relationship
                // I = G_src * (V_s1 - V_s2 - N * (V_p1 - V_p2))
                double R_src = 1.0;  // 1 ohm series resistance for numerical stability
                double G_src = 1.0 / R_src;

                // Primary magnetizing resistance (high value for low magnetizing current)
                double R_mag = 10000.0;
                double G_mag = 1.0 / R_mag;
                STAMP_CONDUCTANCE(n[0], n[1], G_mag);

                // Secondary: VCVS that makes V_s = N * V_p
                // Stamp conductance between S1-S2
                STAMP_CONDUCTANCE(n[2], n[3], G_src);

                // Add VCCS terms: secondary voltage follows primary voltage
                if (n[2] > 0 && n[0] > 0) matrix_add(A, n[2]-1, n[0]-1, -G_src * N);
                if (n[2] > 0 && n[1] > 0) matrix_add(A, n[2]-1, n[1]-1, G_src * N);
                if (n[3] > 0 && n[0] > 0) matrix_add(A, n[3]-1, n[0]-1, G_src * N);
                if (n[3] > 0 && n[1] > 0) matrix_add(A, n[3]-1, n[1]-1, -G_src * N);
            } else {
                // Non-ideal transformer with winding resistances
                double R_p = comp->props.transformer.r_primary;
                double R_s = comp->props.transformer.r_secondary;
                double R_src = 1.0;  // Additional source resistance
                double G_src = 1.0 / R_src;

                // Primary winding resistance
                double G_p = 1.0 / R_p;
                STAMP_CONDUCTANCE(n[0], n[1], G_p);

                // Secondary: winding resistance + VCVS
                double G_s = 1.0 / R_s;
                STAMP_CONDUCTANCE(n[2], n[3], G_s);
                STAMP_CONDUCTANCE(n[2], n[3], G_src);

                // VCCS terms for voltage coupling
                if (n[2] > 0 && n[0] > 0) matrix_add(A, n[2]-1, n[0]-1, -G_src * N);
                if (n[2] > 0 && n[1] > 0) matrix_add(A, n[2]-1, n[1]-1, G_src * N);
                if (n[3] > 0 && n[0] > 0) matrix_add(A, n[3]-1, n[0]-1, G_src * N);
                if (n[3] > 0 && n[1] > 0) matrix_add(A, n[3]-1, n[1]-1, -G_src * N);
            }
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

            // Upper secondary (S1-CT): VCVS with series resistance
            // V_s1_ct = N_half * V_primary
            // Modeled as: I = G_src * (V_s1 - V_ct - N_half * (V_p1 - V_p2))
            // Expanding: I = G_src * V_s1 - G_src * V_ct - G_src * N_half * V_p1 + G_src * N_half * V_p2
            STAMP_CONDUCTANCE(n[2], n[3], G_src);
            // Add VCCS terms to make secondary follow primary
            if (n[2] > 0 && n[0] > 0) matrix_add(A, n[2]-1, n[0]-1, -G_src * N_half);
            if (n[2] > 0 && n[1] > 0) matrix_add(A, n[2]-1, n[1]-1, G_src * N_half);
            if (n[3] > 0 && n[0] > 0) matrix_add(A, n[3]-1, n[0]-1, G_src * N_half);
            if (n[3] > 0 && n[1] > 0) matrix_add(A, n[3]-1, n[1]-1, -G_src * N_half);

            // Lower secondary (CT-S2): VCVS with series resistance
            // V_ct_s2 = N_half * V_primary
            STAMP_CONDUCTANCE(n[3], n[4], G_src);
            if (n[3] > 0 && n[0] > 0) matrix_add(A, n[3]-1, n[0]-1, -G_src * N_half);
            if (n[3] > 0 && n[1] > 0) matrix_add(A, n[3]-1, n[1]-1, G_src * N_half);
            if (n[4] > 0 && n[0] > 0) matrix_add(A, n[4]-1, n[0]-1, G_src * N_half);
            if (n[4] > 0 && n[1] > 0) matrix_add(A, n[4]-1, n[1]-1, -G_src * N_half);

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
                    if (I_abs > comp->props.fuse.rating) {
                        // Accumulate i²t energy: integral of I² over time
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

        case COMP_CRYSTAL:
        case COMP_SPARK_GAP: {
            // Simplified: treat as capacitor/high resistance
            double G = 1e-12;  // Very high impedance
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
                // Add internal resistance term to voltage equation
                // Current flows from + to -, so I_source is in the positive direction
                matrix_add(A, volt_idx, volt_idx, R_int);
                vector_add(b, volt_idx, V_oc);
            }

            // Track discharge over time
            // dQ = I * dt, where Q is charge in coulombs
            if (!ideal && !discharged && prev_solution && dt > 0) {
                // Get current from voltage source variable
                double I_battery = vector_get(prev_solution, volt_idx);
                comp->props.battery.current_draw = fabs(I_battery);

                // Discharge: reduce charge_coulombs
                double dQ = fabs(I_battery) * dt;
                comp->props.battery.charge_coulombs -= dQ;

                if (comp->props.battery.charge_coulombs < 0) {
                    comp->props.battery.charge_coulombs = 0;
                }

                // Update SoC (charge_coulombs / initial_charge)
                double initial_charge = comp->props.battery.capacity_mah * 3.6;  // mAh to C
                comp->props.battery.charge_state = comp->props.battery.charge_coulombs / initial_charge;

                if (comp->props.battery.charge_state < 0.01) {
                    comp->props.battery.discharged = true;
                }
            }
            break;
        }

        case COMP_PULSE_SOURCE: {
            double v_low = comp->props.pulse_source.v_low;
            double v_high = comp->props.pulse_source.v_high;
            double delay = comp->props.pulse_source.delay;
            double pw = comp->props.pulse_source.pulse_width;
            double period = comp->props.pulse_source.period;

            double V = v_low;
            if (time >= delay) {
                double t_in_period = fmod(time - delay, period);
                if (t_in_period < pw) {
                    V = v_high;
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

            // Check previous output to determine saturation state
            double v_out_prev = 0;
            if (prev_solution && n[2] > 0) {
                v_out_prev = vector_get(prev_solution, n[2]-1);
            }

            // Determine operating region based on previous output
            bool saturated_high = (v_out_prev >= v_max * 0.99);
            bool saturated_low = (v_out_prev <= v_min * 0.99);

            if (saturated_high) {
                // Positive saturation: output clamped to v_max
                if (n[2] > 0) {
                    matrix_add(A, volt_idx, n[2]-1, 1.0);
                    matrix_add(A, n[2]-1, volt_idx, 1.0);
                }
                vector_add(b, volt_idx, v_max);
            } else if (saturated_low) {
                // Negative saturation: output clamped to v_min
                if (n[2] > 0) {
                    matrix_add(A, volt_idx, n[2]-1, 1.0);
                    matrix_add(A, n[2]-1, volt_idx, 1.0);
                }
                vector_add(b, volt_idx, v_min);
            } else {
                // Linear region: V_out = A * (V+ - V-)
                if (n[2] > 0) {
                    matrix_add(A, volt_idx, n[2]-1, 1.0);
                    matrix_add(A, n[2]-1, volt_idx, 1.0);
                }
                // n[0] is inverting (-), n[1] is non-inverting (+)
                if (n[1] > 0) matrix_add(A, volt_idx, n[1]-1, -A_gain);
                if (n[0] > 0) matrix_add(A, volt_idx, n[0]-1, A_gain);
            }

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

            STAMP_CONDUCTANCE(n[0], n[2], 1.0/R_ca);  // C1 to A
            STAMP_CONDUCTANCE(n[0], n[3], 1.0/R_cb);  // C1 to B
            STAMP_CONDUCTANCE(n[1], n[2], 1.0/R_cb);  // C2 to A (opposite)
            STAMP_CONDUCTANCE(n[1], n[3], 1.0/R_ca);  // C2 to B (opposite)
            break;
        }

        case COMP_RELAY: {
            // Relay with coil inductance and hysteresis
            // Terminals: 0=C+ (coil+), 1=C- (coil-), 2=NO, 3=COM
            double R_coil = comp->props.relay.r_coil;
            double L_coil = comp->props.relay.l_coil;
            double i_pickup = comp->props.relay.i_pickup;
            double i_dropout = comp->props.relay.i_dropout;
            double I_prev = comp->props.relay.i_coil;
            bool energized = comp->props.relay.energized;

            // --- Coil Circuit (R + L in series) ---
            if (comp->props.relay.ideal || L_coil < 1e-9) {
                // Ideal mode: just coil resistance, no inductance
                STAMP_CONDUCTANCE(n[0], n[1], 1.0/R_coil);

                // Estimate coil current from voltage for hysteresis
                double V_coil = 0;
                if (prev_solution) {
                    double v0 = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                    double v1 = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                    V_coil = v0 - v1;
                }
                I_prev = V_coil / R_coil;
                comp->props.relay.i_coil = I_prev;
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

                // Update coil current from solution for next iteration
                // (done after matrix solve, but we can estimate from voltage)
                if (prev_solution) {
                    double v0 = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                    double v1 = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                    double V_coil = v0 - v1;
                    // I = (V - V_eq) / R_eq = (V - L/dt * I_prev) / R_eq
                    double I_new = (V_coil + (L_coil / dt) * I_prev) / R_eq;
                    comp->props.relay.i_coil = I_new;
                    I_prev = I_new;  // Use updated current for hysteresis
                }
            }

            // --- Hysteresis Logic ---
            // Apply hysteresis: energize at pickup, de-energize at dropout
            double I_abs = fabs(I_prev);
            if (!energized && I_abs >= i_pickup) {
                // Pull-in: energize relay
                comp->props.relay.energized = true;
                energized = true;
            } else if (energized && I_abs <= i_dropout) {
                // Drop-out: de-energize relay
                comp->props.relay.energized = false;
                energized = false;
            }

            // --- Contact Circuit (NO to COM) ---
            double R_contact = energized ?
                              comp->props.relay.r_contact_on :
                              comp->props.relay.r_contact_off;
            STAMP_CONDUCTANCE(n[2], n[3], 1.0/R_contact);
            break;
        }

        case COMP_ANALOG_SWITCH: {
            // Analog switch: controlled by voltage on control terminal
            double v_ctl = 0;
            if (prev_solution && n[2] > 0) {
                v_ctl = vector_get(prev_solution, n[2]-1);
            }

            bool on = v_ctl >= comp->props.analog_switch.v_on;
            double R = on ? comp->props.analog_switch.r_on : comp->props.analog_switch.r_off;
            STAMP_CONDUCTANCE(n[0], n[1], 1.0/R);
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

        case COMP_D_FLIPFLOP:
        case COMP_JK_FLIPFLOP:
        case COMP_T_FLIPFLOP:
        case COMP_SR_LATCH:
        case COMP_COUNTER:
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
            // TL431: Programmable shunt regulator
            double v_ref = 2.5;
            // Acts like a zener at V_ref
            double Vd = 0;
            if (prev_solution) {
                double v1 = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double v2 = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                Vd = v1 - v2;
            }

            double G = (Vd > v_ref) ? 1.0 : 1e-12;
            STAMP_CONDUCTANCE(n[0], n[1], G);
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
                double Vd = 0.6;  // Initial guess
                if (prev_solution) {
                    double v1 = (n[i] > 0) ? vector_get(prev_solution, n[i]-1) : 0;
                    double v2 = (n[com] > 0) ? vector_get(prev_solution, n[com]-1) : 0;
                    Vd = CLAMP(v1 - v2, -5*nVt, 40*nVt);
                }

                // Shockley diode equation with Newton-Raphson companion model
                double expTerm = exp(Vd / nVt);
                double Id = Is * (expTerm - 1);
                double Gd = (Is / nVt) * expTerm + 1e-12;  // Dynamic conductance + GMIN
                double Ieq = Id - Gd * Vd;  // Equivalent current source

                // Store LED current for glow rendering
                comp->props.led_array.currents[i] = (Id > 0) ? Id : 0;

                // Check for overcurrent (burning)
                if (Id > max_I * 2.0 && Id > 0.001) {
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

            // Get previous state
            double omega_prev = comp->props.dc_motor.omega;
            double I_prev = comp->props.dc_motor.current;

            // Back-EMF voltage
            double V_bemf = kv * omega_prev;
            comp->props.dc_motor.v_bemf = V_bemf;

            // Update motor speed using Euler integration
            // Torque = kt * I, friction torque = b * omega
            double T_motor = kt * I_prev;
            double T_friction = b_f * omega_prev;
            double d_omega = (T_motor - T_friction - T_load) / J;
            double omega_new = omega_prev + d_omega * dt;
            if (omega_new < 0) omega_new = 0;  // Prevent negative rotation for simple DC motor
            comp->props.dc_motor.omega = omega_new;

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

            // Update current from solution (will be done after solve)
            if (prev_solution) {
                double v1 = (n[0] > 0) ? vector_get(prev_solution, n[0]-1) : 0;
                double v2 = (n[1] > 0) ? vector_get(prev_solution, n[1]-1) : 0;
                comp->props.dc_motor.current = (v1 - v2 - V_bemf) / Req + I_prev * L_a / (Req * dt);
            }
            break;
        }

        case COMP_SPEAKER: {
            // Speaker: Resistive load with audio output
            // Check if failed (thermal damage)
            if (comp->thermal.failed || comp->props.speaker.failed) {
                // Open circuit when failed
                STAMP_CONDUCTANCE(n[0], n[1], 1e-15);
                break;
            }

            double R = comp->props.speaker.impedance;
            double G = 1.0 / R;
            STAMP_CONDUCTANCE(n[0], n[1], G);

            // Calculate voltage and current for audio output and power tracking
            if (prev_solution) {
                double v1 = (n[0] > 0) ? vector_get(prev_solution, n[0] - 1) : 0.0;
                double v2 = (n[1] > 0) ? vector_get(prev_solution, n[1] - 1) : 0.0;
                double V = v1 - v2;
                double I = V / R;
                double P = V * I;  // Power dissipation

                comp->props.speaker.voltage = V;
                comp->props.speaker.current = I;
                comp->props.speaker.power_dissipated = fabs(P);

                // Feed voltage to audio buffer if audio is enabled
                if (comp->props.speaker.audio_enabled && g_audio.enabled) {
                    // Scale voltage to audio range [-1, 1]
                    // Assuming typical voltage range of ±10V maps to ±1
                    float sample = (float)(V / 10.0);
                    if (sample > 1.0f) sample = 1.0f;
                    if (sample < -1.0f) sample = -1.0f;

                    // Write to ring buffer
                    g_audio.buffer[g_audio.write_pos] = sample;
                    g_audio.write_pos = (g_audio.write_pos + 1) % AUDIO_BUFFER_SIZE;
                }
            }
            break;
        }

        case COMP_MICROPHONE: {
            // Microphone: Voltage source driven by audio input from system mic
            // Acts as a voltage source with value from microphone buffer
            double R_series = comp->props.microphone.ideal ? 1e-6 : comp->props.microphone.r_series;
            double G = 1.0 / R_series;

            // Get voltage from microphone input buffer
            double V_mic = comp->props.microphone.offset;  // DC offset

            if (comp->props.microphone.enabled && g_microphone.initialized && g_microphone.enabled) {
                // Read sample from microphone buffer
                float sample = g_microphone.current_voltage;

                // Scale audio sample [-1, 1] to voltage range
                double amplitude = comp->props.microphone.amplitude;
                double gain = comp->props.microphone.gain;
                V_mic = comp->props.microphone.offset + (sample * amplitude * gain);

                // Update peak level for visualization
                float abs_sample = sample < 0 ? -sample : sample;
                comp->props.microphone.peak_level = abs_sample;
            }

            comp->props.microphone.voltage = V_mic;

            // Stamp as voltage source with series resistance
            // V = V_mic, with series R
            // I = (V+ - V- - V_mic) / R
            // Stamp conductance
            STAMP_CONDUCTANCE(n[0], n[1], G);

            // Stamp current source for voltage
            double I_eq = V_mic * G;
            if (n[0] > 0) vector_add(b, n[0] - 1, I_eq);
            if (n[1] > 0) vector_add(b, n[1] - 1, -I_eq);
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
                if (internal_id > 0 && internal_id < MAX_NODES && external_id > 0) {
                    // Map internal node to the matrix index of the external node
                    node_remap[internal_id] = node_map[external_id];
                }
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

            // Iterate through internal components and stamp them
            for (int c_idx = 0; c_idx < def->num_components; c_idx++) {
                Component *ic = &internal_comps[c_idx];

                // Skip non-stampable components (wires are stored separately, not in component_data)
                if (ic->type == COMP_PIN || ic->type == COMP_LABEL ||
                    ic->type == COMP_TEST_POINT || ic->type == COMP_SUBCIRCUIT) {
                    continue;
                }

                // Create a temporary component with remapped node IDs
                Component temp_comp;
                memcpy(&temp_comp, ic, sizeof(Component));

                // Remap node IDs using the mapping table
                for (int t = 0; t < temp_comp.num_terminals && t < MAX_TERMINALS; t++) {
                    int orig_node = ic->node_ids[t];
                    if (orig_node > 0 && orig_node < MAX_NODES) {
                        int mapped = node_remap[orig_node];
                        temp_comp.node_ids[t] = (mapped >= 0) ? mapped : 0;
                    } else {
                        temp_comp.node_ids[t] = 0;  // Ground
                    }
                }

                // Stamp the remapped component directly using matrix indices
                // Note: We pass a dummy node_map since temp_comp already has matrix indices
                int dummy_node_map[MAX_NODES];
                for (int i = 0; i < MAX_NODES; i++) {
                    dummy_node_map[i] = i;  // Identity mapping
                }
                component_stamp(&temp_comp, A, b, dummy_node_map, num_nodes, time, prev_solution, dt);
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
        case COMP_DC_CURRENT:
            format_engineering(comp->props.dc_current.current, "A", buf, buf_size);
            break;
        case COMP_RESISTOR:
            format_engineering(comp->props.resistor.resistance, "Ohm", buf, buf_size);
            break;
        case COMP_CAPACITOR:
            format_engineering(comp->props.capacitor.capacitance, "F", buf, buf_size);
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
