/**
 * Circuit Playground - Component Definitions
 */

#ifndef COMPONENT_H
#define COMPONENT_H

#include "types.h"
#include "matrix.h"

// Maximum terminals per component
#define MAX_TERMINALS 16

/* The programmable block: fourteen I/O pins and a ground, which is fifteen terminals and fits
   MAX_TERMINALS. D2..D13 and A0..A1 - D0 and D1 are left off because on the board they are the
   serial port, and every sketch that uses Serial would fight whatever was wired to them. */
#define MCU_PINS     15
#define MCU_GND_PIN  14
#define MCU_SRC_MAX  1024
/* Programmable blocks on one sheet. More than a handful is a different program than this. */
#define MCU_MAX_BLOCKS 8

// Terminal definition
typedef struct {
    float dx;      // Offset from component center
    float dy;
    char name[8];  // Terminal name (e.g., "+", "-", "B", "C", "E")
} TerminalDef;

// Component properties union
typedef union {
    // DC Voltage Source
    struct {
        double voltage;         // Output voltage (V)
        double r_series;        // Internal series resistance (Ohm), default: 0.001
        bool ideal;             // Ideal mode (zero internal resistance)
        SweepConfig voltage_sweep;  // Voltage sweep (stepped or ramped)
    } dc_voltage;

    // AC Voltage Source
    struct {
        double amplitude;       // Peak amplitude (V)
        double frequency;       // Frequency (Hz)
        double phase;           // Phase (degrees)
        double offset;          // DC offset (V)
        double r_series;        // Internal series resistance (Ohm), default: 0.001
        bool ideal;             // Ideal mode (zero internal resistance)
        SweepConfig amplitude_sweep;   // Amplitude sweep
        SweepConfig frequency_sweep;   // Frequency sweep
    } ac_voltage;

    // DC Current Source
    struct {
        double current;         // Output current (A)
        double r_parallel;      // Internal parallel resistance (Ohm), default: 1e9
        bool ideal;             // Ideal mode (infinite internal resistance)
        SweepConfig current_sweep;  // Current sweep (stepped or ramped)
    } dc_current;

    // Resistor
    struct {
        double resistance;      // Resistance (Ohm)
        double tolerance;       // Tolerance (%)
        double power_rating;    // Max power dissipation (W)
        bool high_power;        // HP load: box symbol, no thermal warning (power-system loads, fault resistors)
        double power_dissipated; // Current power dissipation (W)
        double temp_coeff;      // Temperature coefficient (ppm/°C), default: 100
        double temp;            // Operating temperature (°C), default: 25
        bool ideal;             // Ideal mode (no temperature effects)
    } resistor;

    // Capacitor
    struct {
        double capacitance;     // Capacitance (F)
        double voltage;         // Initial condition: the voltage the capacitor starts the run at
                                // (0 = start from the operating point, which is the usual case).
                                // A switching converter started from a flat capacitor rings for
                                // milliseconds before it means anything; this is how a template
                                // says "start it where it will settle".
        double esr;             // Equivalent Series Resistance (Ohm), default: 0.01
        double esl;             // Equivalent Series Inductance (H), default: 1e-9
        double leakage;         // Leakage resistance (Ohm), default: 1e9
        double v_half;          // DC bias at which the capacitance has halved (0 = no bias loss,
                                // which is right for C0G/NP0; a 6.3 V X5R halves near 2 V)
        bool ideal;             // Ideal mode (no parasitics)
    } capacitor;

    // Electrolytic Capacitor
    struct {
        double capacitance;     // Capacitance (F)
        double voltage;         // Current voltage (state variable)
        double max_voltage;     // Voltage rating (V)
        double esr;             // ESR (Ohm), typically higher than film caps
        double leakage;         // Leakage resistance (Ohm)
        bool ideal;             // Ideal mode
    } capacitor_elec;

    // Inductor
    struct {
        double inductance;      // Inductance (H)
        double current;         // Current (state variable)
        double dcr;             // DC resistance (Ohm), default: 0.1
        double r_parallel;      // Parallel resistance for core losses (Ohm), default: 1e6
        double i_sat;           // Saturation current (A), default: 1.0
        bool ideal;             // Ideal mode (no DCR, no saturation)
    } inductor;

    // Standard Diode
    struct {
        double is;              // Saturation current (A)
        double vt;              // Thermal voltage (V), ~0.026 at room temp
        double n;               // Ideality factor
        double bv;              // Reverse breakdown voltage (V), default: 100
        double ibv;             // Current at breakdown (A), default: 1e-10
        double cjo;             // Zero-bias junction capacitance (F), default: 1e-12
        bool ideal;             // Ideal mode (simple Vf drop)
    } diode;

    // Zener Diode
    struct {
        double is;              // Saturation current (A)
        double vt;              // Thermal voltage (V)
        double n;               // Ideality factor
        double vz;              // Zener breakdown voltage (V)
        double rz;              // Zener impedance (Ohm), default: 5
        double iz_test;         // Test current for Vz (A), default: 5e-3
        bool ideal;             // Ideal mode (perfect clamping at Vz)
    } zener;

    // Schottky Diode
    struct {
        double is;              // Saturation current (A) - typically higher than Si
        double vt;              // Thermal voltage (V)
        double n;               // Ideality factor - typically 1.0-1.1
        double vf;              // Typical forward voltage (V), default: 0.3
        double cjo;             // Junction capacitance (F)
        bool ideal;             // Ideal mode
    } schottky;

    // LED
    struct {
        double is;              // Saturation current (A)
        double vt;              // Thermal voltage (V)
        double n;               // Ideality factor
        double vf;              // Forward voltage (V)
        double max_current;     // Maximum forward current (A)
        double wavelength;      // Wavelength (nm) for color
        double current;         // Actual current (calculated)
        bool ideal;             // Ideal mode (fixed Vf drop)
        int color;              // Color index: 0=Red, 1=Green, 2=Blue, 3=Yellow, 4=Orange, 5=White
    } led;
    // BJT transistor (NPN/PNP) - Gummel-Poon model parameters
    struct {
        // Basic DC parameters
        double bf;         // BF - Forward current gain (beta), default: 100
        double is;         // IS - Saturation current (A), default: 1e-14
        double vaf;        // VAF - Forward Early voltage (V), default: 100
        double nf;         // NF - Forward emission coefficient, default: 1.0

        // Reverse parameters
        double br;         // BR - Reverse current gain, default: 1.0
        double var;        // VAR - Reverse Early voltage (V), default: 100
        double nr;         // NR - Reverse emission coefficient, default: 1.0

        // Leakage currents
        double ise;        // ISE - B-E leakage saturation current (A), default: 0
        double isc;        // ISC - B-C leakage saturation current (A), default: 0

        /* Charge storage. Without these a transistor has no frequency limit at all: f_T is
           infinite, a common-emitter stage holds its midband gain to a gigahertz, and the
           Miller effect - which is a whole lesson on its own - cannot be demonstrated because
           there is no C_bc to multiply. TF is what sets f_T (f_T = 1/(2 pi TF) at high current);
           CJE and CJC are the depletion capacitances that dominate at low current and at
           reverse bias. Zero by default, so a part that has not been given a data sheet
           behaves exactly as it did before. */
        double tf;         // TF  - Forward transit time (s), default: 0 (no charge storage)
        double tr;         // TR  - Reverse transit time (s), default: 0
        double cje;        // CJE - B-E zero-bias depletion capacitance (F), default: 0
        double cjc;        // CJC - B-C zero-bias depletion capacitance (F), default: 0
        double vje;        // VJE - B-E junction potential (V), default: 0.75
        double vjc;        // VJC - B-C junction potential (V), default: 0.75

        // Temperature
        double temp;       // Operating temperature (K), default: 300

        /* Live operating point, written by the stamp and read by the properties panel and the
           bias warning. A small-signal answer taken about a transistor that is saturated or cut
           off is not a small answer, it is a wrong one, and nothing in a waveform says so. */
        double op_vbe, op_vce, op_ic, op_ib, op_gm;
        int op_region;     // 0 cutoff, 1 active, 2 saturated, 3 reverse-active

        // Mode
        bool ideal;        // Use ideal (simplified) model, default: true
    } bjt;

    // MOSFET transistor (NMOS/PMOS) - Level 1 SPICE model parameters
    struct {
        // Basic parameters
        double vth;        // VTO - Threshold voltage (V), NMOS: 0.7, PMOS: -0.7
        double kp;         // KP - Transconductance parameter (A/V²), default: 110e-6
        double lambda;     // LAMBDA - Channel length modulation (1/V), default: 0.04

        // Physical dimensions
        double w;          // W - Channel width (m), default: 10e-6
        double l;          // L - Channel length (m), default: 1e-6
        double tox;        // TOX - Gate oxide thickness (m), default: 10e-9

        // Body effect parameters
        double gamma;      // GAMMA - Body effect coefficient (V^0.5), default: 0.4
        double phi;        // PHI - Surface potential (V), default: 0.65
        double nsub;       // NSUB - Substrate doping (1/cm³), default: 1e15

        // Gate capacitance parameters
        double cgso;       // CGSO - Gate-source overlap capacitance (F/m), default: 1e-10
        double cgdo;       // CGDO - Gate-drain overlap capacitance (F/m), default: 1e-10
        double cgbo;       // CGBO - Gate-body overlap capacitance (F/m), default: 1e-10
        double cj;         // CJ - Junction capacitance (F/m²), default: 1e-4

        // State variables for capacitor integration
        // Operating point, refreshed every solve so the properties panel can show it
        double op_vgs, op_vds, op_id, op_gm;   // V, V, A, A/V
        int op_region;          // 0 cutoff, 1 triode, 2 saturation
        double vgs_prev;   // Previous Vgs for capacitor integration
        double vgd_prev;   // Previous Vgd for capacitor integration
        double i_cgs;      // Gate-source capacitor current
        double i_cgd;      // Gate-drain capacitor current

        // Temperature
        double temp;       // Operating temperature (K), default: 300

        // Mode
        bool ideal;        // Use ideal (simplified) model, default: true
    } mosfet;
    /* Quartz crystal: the motional arm (Ls, Cs, Rs in series - the mechanical resonance seen
       electrically) in parallel with Cp, the capacitance of the plates and the holder. The
       series resonance is 1/(2 pi sqrt(Ls Cs)) and the Q is 2 pi f Ls / Rs, which for a real
       part is tens of thousands: that is what makes a crystal a crystal, and what makes it
       hard to integrate without damping it numerically (see the stamp). */
    struct {
        double ls;              // motional inductance (H)
        double cs;              // motional capacitance (F)
        double rs;              // motional resistance / ESR (Ohm)
        double cp;              // shunt (holder) capacitance (F)
        bool ideal;             // Ideal mode: lossless motional arm
    } crystal;

    // Op-Amp
    struct {
        double gain;            // Open-loop DC gain, default: 100000 (100dB)
        double voffset;         // Input offset voltage (V), default: 0
        double vmax;            // Positive rail voltage (V), default: 15
        double vmin;            // Negative rail voltage (V), default: -15
        double gbw;             // Gain-bandwidth product (Hz), default: 1e6
        double slew_rate;       // Slew rate (V/us), default: 0.5
        double r_in;            // Input impedance (Ohm), default: 1e12
        double r_out;           // Output impedance (Ohm), default: 75
        double i_bias;          // Input bias current (A), default: 1e-12
        double cmrr;            // Common-mode rejection ratio (dB), default: 90
        bool rail_to_rail;      // Rail-to-rail output capability
        bool ideal;             // Ideal mode (infinite gain, bandwidth, etc.)
        double prev_output;     // Previous output voltage (for GBW dynamics)
    } opamp;
    // Waveform generators
    struct {
        double amplitude;       // Peak amplitude (V)
        double frequency;       // Frequency (Hz)
        double phase;           // Phase (degrees)
        double offset;          // DC offset (V)
        double duty;            // Duty cycle (0-1)
        double rise_time;       // Rise time (s), default: 1e-9
        double fall_time;       // Fall time (s), default: 1e-9
        double r_series;        // Output resistance (Ohm)
        bool ideal;             // Ideal mode (zero rise/fall, zero output R)
        SweepConfig amplitude_sweep;   // Amplitude sweep
        SweepConfig frequency_sweep;   // Frequency sweep
    } square_wave;

    struct {
        double amplitude;       // Peak amplitude (V)
        double frequency;       // Frequency (Hz)
        double phase;           // Phase (degrees)
        double offset;          // DC offset (V)
        double r_series;        // Output resistance (Ohm)
        bool ideal;             // Ideal mode
        SweepConfig amplitude_sweep;   // Amplitude sweep
        SweepConfig frequency_sweep;   // Frequency sweep
    } triangle_wave;

    struct {
        double amplitude;       // Peak amplitude (V)
        double frequency;       // Frequency (Hz)
        double phase;           // Phase (degrees)
        double offset;          // DC offset (V)
        double r_series;        // Output resistance (Ohm)
        bool ideal;             // Ideal mode
        SweepConfig amplitude_sweep;   // Amplitude sweep
        SweepConfig frequency_sweep;   // Frequency sweep
    } sawtooth_wave;

    struct {
        double amplitude;       // RMS amplitude (V)
        double seed;            // Random seed
        double bandwidth;       // Noise bandwidth (Hz), default: 1e6
        double r_series;        // Output resistance (Ohm)
        bool ideal;             // Ideal mode
        SweepConfig amplitude_sweep;   // Amplitude sweep
    } noise_source;

    // Text annotation
    struct {
        char text[128];         // Text content
        int font_size;          // Font size (1=small, 2=normal, 3=large)
        uint32_t color;         // Text color (RGBA packed)
        bool bold;              // Bold text
        bool italic;            // Italic text
        bool underline;         // Underlined text
    } text;

    // Potentiometer (variable resistor with 3 terminals)
    struct {
        double resistance;      // Total resistance (Ohm)
        double wiper_pos;       // Wiper position (0.0 to 1.0)
        double tolerance;       // Tolerance (%)
        int taper;              // 0=linear, 1=logarithmic
        bool ideal;             // Ideal mode
    } potentiometer;

    // Photoresistor (LDR - Light Dependent Resistor)
    struct {
        double r_dark;          // Resistance in darkness (Ohm), default: 1e6
        double r_light;         // Resistance in bright light (Ohm), default: 100
        double light_level;     // Light level (0.0=dark to 1.0=bright)
        double gamma;           // Light sensitivity exponent, default: 0.7
        bool ideal;             // Ideal mode
    } photoresistor;

    // Thermistor (NTC or PTC)
    struct {
        double r_25;            // Resistance at 25°C (Ohm)
        double beta;            // Beta value (K), default: 3950 for NTC
        double temp;            // Operating temperature (°C)
        int type;               // 0=NTC, 1=PTC
        bool ideal;             // Ideal mode
    } thermistor;

    // Fuse
    struct {
        double rating;          // Current rating (A)
        double resistance;      // Cold resistance (Ohm), default: 0.01
        double i2t;             // I²t rating for time-current characteristic (A²s)
        double i2t_accumulated; // Accumulated I²t energy (A²s) - state variable
        double current;         // Current through fuse (A) - for display/animation
        double blow_time;       // Simulation time when fuse blew (for animation)
        bool blown;             // Current state
        bool ideal;             // Ideal mode (instant blow at rating)
    } fuse;

    // AC Current Source
    struct {
        double amplitude;       // Peak amplitude (A)
        double frequency;       // Frequency (Hz)
        double phase;           // Phase (degrees)
        double offset;          // DC offset (A)
        double r_parallel;      // Internal parallel resistance (Ohm)
        bool ideal;             // Ideal mode
    } ac_current;

    // Clock Source (digital)
    struct {
        double frequency;       // Frequency (Hz)
        double v_low;           // Low voltage (V), default: 0
        double v_high;          // High voltage (V), default: 5
        double duty;            // Duty cycle (0-1), default: 0.5
        bool ideal;             // Ideal mode
    } clock;

    // Pulse Source
    struct {
        double v_low;           // Low voltage (V)
        double v_high;          // High voltage (V)
        double delay;           // Initial delay (s)
        double rise_time;       // Rise time (s)
        double fall_time;       // Fall time (s)
        double pulse_width;     // Pulse width (s)
        double period;          // Period (s)
        double r_series;        // Output resistance
        bool ideal;             // Ideal mode
    } pulse_source;

    // PWM Source
    struct {
        double amplitude;       // Amplitude (V)
        double frequency;       // PWM frequency (Hz)
        double duty;            // Duty cycle (0-1)
        double offset;          // DC offset (V)
        double r_series;        // Output resistance
        bool ideal;             // Ideal mode
    } pwm_source;

    // JFET (N-channel and P-channel)
    struct {
        double idss;            // Drain saturation current (A), default: 10e-3
        double vp;              // Pinch-off voltage (V), default: -2 for N, +2 for P
        double lambda;          // Channel length modulation (1/V), default: 0.01
        double beta;            // Transconductance parameter (A/V²)
        double temp;            // Temperature (K)
        bool ideal;             // Ideal mode
    } jfet;

    // Controlled Sources (VCVS, VCCS, CCVS, CCCS)
    struct {
        double gain;            // Gain (V/V for VCVS, A/V for VCCS, V/A for CCVS, A/A for CCCS)
        double r_in;            // Input resistance (for current sensing in CCVS/CCCS)
        bool ideal;             // Ideal mode
    } controlled_source;

    // SCR (Silicon Controlled Rectifier)
    struct {
        double vgt;             // Gate trigger voltage (V), default: 0.7
        double igt;             // Gate trigger current (A), default: 10e-3
        double ih;              // Holding current (A), default: 10e-3
        double vf;              // Forward voltage drop (V)
        bool on;                // Current state (latched on)
        bool ideal;             // Ideal mode
    } scr;

    // TRIAC
    struct {
        double vgt;             // Gate trigger voltage (V)
        double igt;             // Gate trigger current (A)
        double ih;              // Holding current (A)
        double vf;              // Forward voltage drop (V)
        bool on;                // Current state
        bool ideal;             // Ideal mode
    } triac;

    // DIAC
    struct {
        double vbo;             // Breakover voltage (V), default: 30
        double vf;              // Forward voltage drop after breakover (V)
        bool ideal;             // Ideal mode
    } diac;

    // Logic gates
    struct {
        double v_low;           // Low output voltage (V), default: 0
        double v_high;          // High output voltage (V), default: 5
        double v_threshold;     // Input threshold (V), default: 2.5
        double r_out;           // Output resistance (Ohm)
        double prop_delay;      // Propagation delay (s)
        int num_inputs;         // Number of inputs (for AND, OR, etc.), default: 2
        bool state;             // Current output state (for simulation)
        bool ideal;             // Ideal mode
    } logic_gate;

    // Logic input (manual high/low toggle)
    struct {
        bool state;             // Current state (0=low, 1=high)
        double v_low;           // Low voltage (V)
        double v_high;          // High voltage (V)
        double r_out;           // Output resistance
    } logic_input;

    // Logic output (LED indicator)
    struct {
        double v_threshold;     // Threshold voltage (V)
        bool state;             // Current state (display only)
    } logic_output;

    // 555 Timer
    struct {
        double r1;              // Timing resistor 1 (Ohm)
        double r2;              // Timing resistor 2 (Ohm)
        double c;               // Timing capacitor (F)
        int mode;               // 0=astable, 1=monostable
        double vcc;             // Supply voltage (V)
        bool output;            // Current output state
        double cap_voltage;     // Internal capacitor voltage (state)
        bool ideal;             // Ideal mode
    } timer_555;

    // Relay (with coil inductance and hysteresis)
    struct {
        double v_coil;          // Coil voltage rating (V)
        double r_coil;          // Coil resistance (Ohm)
        double l_coil;          // Coil inductance (H), for inductive kickback
        double i_pickup;        // Pickup current threshold (A)
        double i_dropout;       // Dropout current threshold (A)
        double r_contact_on;    // Contact on-resistance (Ohm)
        double r_contact_off;   // Contact off-resistance (Ohm)
        double i_coil;          // Current coil current (A) - state variable
        bool energized;         // Current contact state
        bool ideal;             // Ideal mode (no inductance)
    } relay;

    // Analog Switch (voltage controlled)
    struct {
        double v_on;            // Control voltage for on (V)
        double v_off;           // Control voltage for off (V)
        double r_on;            // On-resistance (Ohm)
        double r_off;           // Off-resistance (Ohm)
        bool state;             // Current state (tracks the control voltage unless manual is set)
        bool manual;            // Manual override: click the switch to force it; shift-click returns it to the control signal
        bool ideal;             // Ideal mode
    } analog_switch;

    /* Logic-driven DPDT changeover. Both poles throw together on the control input: with it
       low each common sits on its NC throw, with it high on its NO throw. Break-before-make is
       inherent rather than modelled - exactly one throw per pole is ever stamped at r_on and the
       other at r_off, so there is no instant at which both are connected. */
    struct {
        double v_on;            // Control threshold: at or above this, the poles are thrown (V)
        double r_on;            // Resistance of the closed throw (Ohm)
        double r_off;           // Resistance of the open throw (Ohm)
        bool thrown;            // Where the poles are now, for the symbol to draw
        bool ideal;             // Ideal: zero on-resistance, infinite off
    } dpdt_driven;

    // Arbitrary waveform source: replays one of the global sample tables
    struct {
        int table;              // which global table (0..ARB_TABLES-1)
        double period;          // seconds for one pass through the table
        double amplitude;       // table values are scaled by this
        double offset;          // added after scaling (V)
    } arb_source;

    // Voltmeter
    struct {
        double r_in;            // Input resistance (Ohm), default: 10e6
        double reading;         // Current reading (V)
        bool ideal;             // Ideal mode (infinite resistance)
    } voltmeter;

    // Ammeter
    struct {
        double r_shunt;         // Shunt resistance (Ohm), default: 0.01
        double reading;         // Current reading (A)
        bool ideal;             // Ideal mode (zero resistance)
    } ammeter;

    // Lamp (incandescent)
    struct {
        double power_rating;    // Power rating (W)
        double voltage_rating;  // Voltage rating (V)
        double r_cold;          // Cold resistance (Ohm)
        double r_hot;           // Hot resistance (Ohm)
        double brightness;      // Current brightness (0-1)
        bool ideal;             // Ideal mode (constant resistance)
    } lamp;

    // SPST Switch (Single-Pole Single-Throw)
    struct {
        bool closed;            // Switch state: true=closed (conducting), false=open
        double r_on;            // On-state resistance (Ohm), default: 0.01
        double r_off;           // Off-state resistance (Ohm), default: 1e9
        bool momentary;         // If true, returns to default state when released
        bool default_closed;    // Default state for momentary switches
    } switch_spst;

    // SPDT Switch (Single-Pole Double-Throw)
    struct {
        int position;           // 0=terminal A, 1=terminal B
        double r_on;            // On-state resistance (Ohm)
        double r_off;           // Off-state resistance (Ohm)
        bool momentary;         // If true, returns to default position
        int default_pos;        // Default position for momentary
    } switch_spdt;

    // Push Button (Momentary, normally open)
    struct {
        bool pressed;           // Currently pressed
        double r_on;            // On-state resistance (Ohm)
        double r_off;           // Off-state resistance (Ohm)
    } push_button;

    // Transformer (coupled inductors)
    struct {
        double l_primary;       // Primary inductance (H), default: 10e-3
        double turns_ratio;     // Secondary/Primary turns ratio (N2/N1), default: 1.0
        double coupling;        // Coupling coefficient (0-1), default: 0.99
        double r_primary;       // Primary winding resistance (Ohm), default: 0.1
        double r_secondary;     // Secondary winding resistance (Ohm), default: 0.1
        int n_primary;          // Number of primary turns (for display), default: 100
        int n_secondary;        // Number of secondary turns (for display), default: 100
        bool ideal;             // Ideal mode (perfect coupling, no resistance)
        bool center_tap;        // Has center tap on secondary
    } transformer;

    // 7-Segment Display (common cathode/anode)
    struct {
        double vf;              // Forward voltage per segment (V)
        double max_current;     // Max current per segment (A)
        bool common_cathode;    // true=common cathode, false=common anode
        uint8_t segments;       // Active segments bitmask (a=bit0, b=bit1, ..., g=bit6, dp=bit7)
        double currents[8];     // Per-segment current (A), order a,b,c,d,e,f,g,dp - drives the glow
        bool ideal;             // Ideal mode
    } seven_seg;

    // Binary / BCD counter with a settable modulus and a carry out
    struct {
        int modulus;            // counts 0 .. modulus-1, then wraps (10 for a BCD digit)
        int count;              // where it is now
        bool wrapped;           // has it rolled over yet? CARRY stays low until it has, so a
                                // chain of these does not clock itself once at power-on
        double v_low;           // output low level (V)
        double v_high;          // output high level (V)
        double v_threshold;     // input threshold (V)
        double r_out;           // output resistance (Ohm)
        bool ideal;
    } counter;

    // BCD to 7-Segment Decoder (like 7447/74LS47)
    struct {
        double v_low;           // Low output voltage (V)
        double v_high;          // High output voltage (V)
        double v_threshold;     // Input threshold (V)
        bool active_low;        // true=active low outputs (like 7447)
        bool blanking;          // Blanking input state
        bool lamp_test;         // Lamp test input state
        bool ideal;             // Ideal mode
    } bcd_decoder;

    // PWL (Piecewise Linear) Source - up to 32 time-value pairs
    struct {
        double times[32];       // Time points (s)
        double values[32];      // Voltage values (V) at each time point
        int num_points;         // Number of defined points (max 32)
        bool repeat;            // Repeat waveform after last point
        double repeat_period;   // Period for repeat mode (0 = auto from last time)
        double r_series;        // Output resistance (Ohm)
        bool ideal;             // Ideal mode (zero output resistance)
    } pwl_source;

    // Expression-based Source V(t)
    struct {
        char expression[256];   // Math expression string (e.g., "3*sin(2*pi*60*t)+0.1*rand()")
        double r_series;        // Output resistance (Ohm)
        double cached_value;    // Last computed value (for efficiency)
        double cache_time;      // Time of cached value
        bool ideal;             // Ideal mode
    } expr_source;

    // DC Motor (armature model with back-EMF)
    struct {
        double r_armature;      // Armature resistance (Ohm), default: 1.0
        double l_armature;      // Armature inductance (H), default: 1e-3
        double kv;              // Back-EMF constant (V/rad/s), default: 0.01
        double kt;              // Torque constant (Nm/A), default: 0.01
        double j_rotor;         // Rotor moment of inertia (kg*m^2), default: 1e-5
        double b_friction;      // Viscous friction coefficient (Nm*s/rad), default: 1e-6
        double omega;           // Angular velocity state (rad/s)
        double current;         // Armature current state (A)
        double torque_load;     // External load torque (Nm)
        double v_bemf;          // Back-EMF voltage (calculated)
        bool ideal;             // Ideal mode (no inductance, no friction)
    } dc_motor;

    // Battery with discharge model
    struct {
        double nominal_voltage;  // Nominal voltage (V), default: 1.5 (AA)
        double capacity_mah;     // Capacity in mAh, default: 2500
        double internal_r;       // Internal resistance (Ohm), default: 0.1
        double charge_state;     // State of charge (0.0=empty, 1.0=full)
        double charge_coulombs;  // Remaining charge in coulombs (C = mAh * 3.6)
        double current_draw;     // Tracked current draw (A), for discharge calculation
        double v_cutoff;         // Cutoff voltage (V), default: 0.9
        bool discharged;         // Battery is depleted
        bool ideal;              // Ideal mode (no internal resistance, no discharge)
        /* What it is made of and how it is wired up. A pack is cells in series for voltage and
           in parallel for capacity and current: 2S is 7.4 V of LiPo, 6 lead-acid cells is a car
           battery, 4S LiFePO4 is 12.8 V and drops into the same hole. The three numbers below
           DERIVE nominal_voltage, capacity_mah, internal_r and v_cutoff - see
           component_battery_refresh - so setting the chemistry sets the physics with it. */
        int chemistry;           // BatteryChemistry
        int cells_series;        // S: cells stacked for voltage, default 1
        int cells_parallel;      // P: cells alongside for capacity and current, default 1
        /* Continuous discharge rating, in multiples of capacity per hour. A 2200 mAh pack at
           25C will give 55 A; the same cell at 1C will give 2.2 A and sag badly trying. It is
           what separates a pack built for a drone from one built for a torch, and it is why
           internal resistance is derived from it rather than typed in separately. */
        double c_rating;
        double cell_capacity_mah; // per cell; capacity_mah is this times P
    } battery;

    // Speaker/Buzzer (audio output)
    struct {
        double impedance;       // Speaker impedance (Ohm), default: 8
        double sensitivity;     // Sensitivity (dB/W/m), default: 90
        double max_power;       // Max power rating (W), default: 1.0
        double power_dissipated; // Current power (W)
        double voltage;         // Current voltage across speaker
        double current;         // Current through speaker
        double frequency;       // Detected dominant frequency (Hz)
        bool audio_enabled;     // Output to SDL audio
        bool failed;            // Speaker is damaged
    } speaker;

    // Microphone (audio input source)
    struct {
        double amplitude;       // Output amplitude (V peak), default: 5.0
        double offset;          // DC offset (V), default: 2.5
        double gain;            // Input gain multiplier, default: 1.0
        double r_series;        // Output resistance (Ohm), default: 1000
        double voltage;         // Current output voltage (calculated)
        double peak_level;      // Current peak audio level (0-1)
        bool enabled;           // Microphone capture enabled
        bool ideal;             // Ideal mode (zero output resistance)
    } microphone;

    // Antenna (TX/RX pair for wireless signal transmission)
    struct {
        int channel;            // Wireless channel (0-15)
        double r_series;        // Series resistance (Ohm), default: 50
        double voltage;         // Current voltage (TX: measured, RX: received)
        double gain;            // Signal gain multiplier, default: 1.0
        bool ideal;             // Ideal mode (zero resistance)
    } antenna;

    // LED Dot Matrix (8x8 LED display with common cathode)
    struct {
        uint8_t pixel_state[8];  // 8 bytes, each bit represents one LED in a row
        double vf;               // Forward voltage per LED (default: 2.0V)
        double if_max;           // Max forward current per LED (default: 20mA)
        uint8_t color;           // LED color (0=red, 1=green, 2=blue, 3=yellow, 4=white)
        bool common_cathode;     // True = common cathode, False = common anode
    } led_matrix;

    // LED Bar Graph Array (8 individual LEDs with common cathode)
    struct {
        double is;               // Saturation current (A) - same as LED
        double n;                // Ideality factor - same as LED
        double vf;               // Forward voltage per LED (default: 2.0V)
        double max_current;      // Max forward current per LED (default: 20mA)
        double currents[8];      // Current through each LED segment
        bool failed[8];          // Failed (burned) state for each LED
        int color;               // LED color (0=red, 1=green, 2=blue, 3=yellow, 4=orange, 5=white)
    } led_array;

    // Bus (wire bundle - groups multiple wires together)
    struct {
        int width;              // Bus width (number of wires), default: 8
        char name[16];          // Bus name (e.g., "DATA", "ADDR")
        int bus_id;             // Bus identifier for matching bus/tap pairs
    } bus;

    // Bus Tap (extracts a single wire from a bus)
    struct {
        int bus_id;             // Bus identifier to connect to
        int tap_index;          // Which wire to tap (0 to width-1)
        char signal_name[16];   // Signal name (e.g., "D0", "A7")
    } bus_tap;

    // Pin marker for subcircuit creation
    struct {
        int pin_number;         // Pin number (1-16)
        char pin_name[16];      // Pin name (e.g., "VCC", "IN1", "OUT")
    } pin;

    /* A block that runs Arduino-shaped code and drives its pins from it. The code lives here as
       text and nothing else: Component is cloned with a flat memcpy for undo, copy and paste,
       so a pointer to a compiled program in here would be shared between copies and freed
       twice. The compiled program lives in the Simulation, keyed by component id, and this
       struct carries only what the stamp needs - what each pin is being driven to right now.

       1 KB of source, which is a deliberate limit rather than an arbitrary one. Blink is 120
       characters and a sketch that reads a divider and switches an output is under 400. This
       block drives pins in response to pin states and time; a sketch that does not fit is one
       that has grown into being about the language rather than about the circuit. */
    struct {
        char source[MCU_SRC_MAX];
        double vcc;             // supply the outputs drive to, and what analogRead measures against
        double r_out;           // series resistance of a driven pin (a real port pin is ~25 ohm)
        double r_in;            // a pin left as an input: high impedance, not infinite
        double r_pullup;        // INPUT_PULLUP, or a digitalWrite HIGH to a pin still in INPUT
        double pwm_hz;          // analogWrite carrier (490 Hz on most Arduino pins)
        /* Runtime, written once per accepted step by the simulation. pin_level is 0..1 of vcc
           and is the INSTANTANEOUS level, so a PWM pin is a real square wave on the scope
           rather than its average. */
        double pin_level[MCU_PINS];
        unsigned char pin_drive[MCU_PINS];   // 0 input, 1 output, 2 input with pull-up
        bool compiled;          // the source compiled cleanly
        char status[64];        // the compile error, or the last Serial.print
    } mcu;

    // Sub-circuit instance (user-defined IC block)
    struct {
        int def_id;             // Reference to SubCircuitDef in g_subcircuit_library
        char name[32];          // Instance name (e.g., "U1", "IC2")
        /* Live copy of the definition's components for THIS instance. The definition is a
           template; two blocks of the same type each need their own capacitor voltages and
           inductor currents, so the copies live here and persist between steps. Rebuilt when
           the instance is first stamped or the definition changes; never serialized (a saved
           circuit stores def_id, and the instance is rebuilt on load). */
        void *inst_data;
        int inst_count;
        int inst_def_id;        // which definition inst_data was built from
    } subcircuit;
    // Spark gap: open until |V| exceeds the breakdown of the air gap (3 kV/mm), then a low
    // resistance arc that persists until the current has stayed below hold_current for
    // quench_time. State changes only between accepted steps (see simulation.c).
    struct {
        double gap_mm;          // Electrode spacing (mm); breakdown = 3000 V/mm * gap_mm
        double r_on;            // Arc resistance while conducting (Ohm)
        double hold_current;    // Arc extinguishes below this current (A) ...
        double quench_time;     // ... after this long (s)
        bool conducting;        // Current state (runtime)
        double last_conduct_time;  // Last time |I| > hold_current (runtime)
    } spark_gap;

    // Toroid topload: capacitance to ground from the toroid dimensions (Bert Pool formula)
    struct {
        double major_in;        // Outer diameter D (inches)
        double minor_in;        // Tube (cross-section) diameter d (inches)
        double voltage;         // Terminal voltage after the last accepted step (runtime, for the corona display)
    } toroid;
    // Transmission line (single-phase equivalent) built from per-mile data at 60 Hz:
    //   R = r_per_mi * length, L = x_per_mi * length / (2 pi 60), C_end = b * length / (2 pi 60) / 2
    // model: 0 = resistance only, 1 = series R-L, 2 = nominal pi (R-L with C/2 shunt at each end)
    struct {
        double length_mi;
        double r_per_mi;        // Ohm per mile
        double x_per_mi;        // Ohm per mile (inductive reactance at 60 Hz)
        double b_us_per_mi;     // micro-siemens per mile (shunt charging susceptance at 60 Hz)
        int model;
    } tline;
    /* Signal transmission line: a real one, with a propagation delay rather than a lumped
       approximation of one. Both ports are referred to circuit ground - a coax with its shield
       on the ground plane, which is what every template that needs this is drawing.
       The model is Bergeron's: at each end the line looks like Z0 in series with a source
       carrying what the far end launched one delay ago,
           E1(t) = v2(t - T) + Z0 i2(t - T),   E2(t) = v1(t - T) + Z0 i1(t - T)
       which is exact for a lossless line at any time step - the delay is in the history, not
       in the number of sections, so 5 ns of cable does not need 5 ns of solver resolution. */
    struct {
        double z0;              // characteristic impedance (Ohm)
        double delay;           // one-way propagation delay (s)
        double loss_db;         // one-way loss (dB), applied to the launched wave
        bool ideal;             // lossless: ignore loss_db
        double *hist;           // ring buffer, 2 * cap: port 0 then port 1 (v + Z0 i)
        double *hist_t;         // sample times, cap entries
        int cap, head, count;   // ring state; head is where the next sample goes
    } delay_line;
    // Three-phase source / generator block (per-phase peak voltage, common neutral)
    struct {
        double v_peak;          // phase-to-neutral peak (V)
        double frequency;       // Hz
        double phase;           // phase A angle (deg); B = -120, C = +120 from it
        double r_series;        // per-phase series resistance (Ohm)
        double l_series;        // per-phase series inductance (H), e.g. a generator's X''
    } source_3ph;
} ComponentProps;

// Component structure
/* The largest storage values the solver stays conditioned at. A capacitor's companion is a
   conductance of C/dt, and dt goes down to 10 ps, so a big enough C puts a number in the matrix
   that swamps every real conductance in the circuit and the solve comes apart: --stress-test
   measures a clean solve at 1e6 F and a runaway to -4e5 V in a twelve volt circuit at 1e9. The
   same for an inductor at 1e9 H and above.

   A megafarad and a megahenry are already absurd - the largest supercapacitors made are a few
   thousand farads - so this refuses nothing anyone would ask for, and it refuses it at the point
   the value is typed rather than letting the scope show a number that is not a number. */
#define MAX_CAPACITANCE 1e6
#define MAX_INDUCTANCE  1e6

typedef struct Component {
    int id;
    ComponentType type;
    float x, y;
    int rotation;       // 0, 90, 180, 270
    bool selected;
    bool highlighted;
    char label[MAX_LABEL_LEN];
    char part[16];      // Named device ("2N7000", "LM358", ...) when one has been applied; "" = generic

    // Terminals and connections
    int num_terminals;
    int node_ids[MAX_TERMINALS];  // Connected node IDs

    // For voltage sources/inductors - index of current variable
    int voltage_var_idx;
    bool needs_voltage_var;
    double terminal_current[MAX_TERMINALS];  // Current entering each terminal (A), from the last solve
    FlowState flow;                          // where its flow dots are; display only, never saved
    double trap_i_prev;
    /* What the companion state was when the current step's solve stamped it. Terminal currents
       are recovered after the step by re-stamping each device alone, and by then trap_i_prev and
       cap_vc have been advanced to the next step's values - so the re-stamp reproduced a stamp
       that never happened, and the difference landed in the current-flow display as a KCL gap.
       On the Pierce oscillator that gap was 6.8 uA. Read these instead when g_stamp_read_only. */
    double trap_i_solve;
    double cap_vc_solve;
    /* the same idea for parts whose state lives in their props - a motor's speed and armature
       current, a relay's coil current */
    double state_w_solve;
    double state_i_solve;                      // Capacitors: current (terminal 0 -> 1) at the end of the last step (trapezoidal state)
    double cap_vc;                           // Capacitors: voltage across the ideal C itself (terminal voltage minus ESR/ESL drops)
    double tline_ic_prev[2];   // transmission line: shunt-capacitor currents at each end after the last accepted step (theta method)
    int sat_last_rail;                       // Op-amps: rail chosen in the previous Newton iteration (+1/-1/0)
    int sat_flips;
    int slew_latch;                          // Op-amps: -1/+1 while the output is slew-limited this step, 0 free
    double mos_vds_lin;                      // MOSFETs: the V_DS the last stamp linearised at.
                                             // Kept apart from props.mosfet.op_vds, which is the
                                             // real terminal voltage the properties panel shows -
                                             // the two differ while Newton limiting is catching up.                           // Op-amps: rail flip-flops seen in this solve (>=2 -> use the linear stamp)
    double sweep_phase;                      // AC sources with a frequency sweep: accumulated phase (rad)

    // Properties
    ComponentProps props;

    // Mixed-signal logic state (for digital components)
    LogicGateState logic_state;

    // Thermal state (for power dissipation / magic smoke)
    ThermalState thermal;
} Component;

// Component type info
typedef struct {
    const char *name;
    const char *short_name;
    int num_terminals;
    TerminalDef terminals[MAX_TERMINALS];
    float width;
    float height;
    ComponentProps default_props;
} ComponentTypeInfo;

// Get info for component type
const ComponentTypeInfo *component_get_info(ComponentType type);
// Extra search words for the Spotlight / palette filter ("mosfet", "coil", "transistor" ...); "" if none
const char *component_search_keywords(ComponentType type);

// Global sample tables for COMP_ARB_SOURCE. Two of them (0 = X, 1 = Y) back the X-Y plotter:
// load a file of coordinate pairs and the scope draws the shape in X-Y mode.
#define ARB_TABLES 4
#define ARB_MAX 2048
void arb_table_set(int idx, const double *v, int n);
int  arb_table_len(int idx);
double arb_table_value(int idx, double phase);   // phase 0..1, linearly interpolated
// Load a two-column text file ("x y" per line, blank/# lines ignored) into tables 0 and 1.
// Returns the number of points, or 0 on failure.
int arb_load_xy_file(const char *path);

// Create a new component
Component *component_create(ComponentType type, float x, float y);

// Free component
void component_free(Component *comp);

// Clone component
Component *component_clone(Component *comp);

// Rotate component
void component_rotate(Component *comp);

// Get terminal world positions
void component_get_terminal_pos(Component *comp, int terminal_idx, float *x, float *y);

// High-voltage helpers
double toroid_capacitance(const Component *comp);      // Farads, from props.toroid dimensions
#define SUBCIRCUIT_MAX_DEPTH 8   /* blocks inside blocks: deep enough for real hierarchy, and a
                                    stop for a definition that manages to contain itself */
extern int g_subcircuit_depth;
SubCircuitDef *subcircuit_find_def(int def_id);        // NULL if the id is not in the library
void subcircuit_release_instance(Component *comp);     // free a block's live copies, not the block
/* The live internal components of a placed subcircuit, after it has been stamped at least
   once: their node_ids hold MATRIX INDICES, not circuit node ids, and their state (capacitor
   charge, inductor current) belongs to this block. Returns the count, 0 if there is none. */
int component_subcircuit_instance(Component *comp, Component **out);

/* ---------------------------------------------------------------------------------------
   Named parts. A schematic says 2N7000, not "an NMOS with V_th = 2.1 V", so each entry here
   carries the datasheet parameters for a real device and the one-line spec it was taken from.
   Applying one sets the model parameters and the part name; the name is what the canvas and
   the properties panel show. The parts are checked against their data sheets by
   `template_smoke --part-test`, which builds the datasheet's own test condition.
   --------------------------------------------------------------------------------------- */
typedef struct {
    const char *part;            // "2N7000"
    ComponentType type;          // which symbol it is
    const char *summary;         // the line shown in the properties panel
    void (*apply)(Component *c); // datasheet parameters
} PartModel;

int  component_part_count(void);
const PartModel *component_part_at(int i);
// Parts that fit this component type, in table order; returns how many and fills idx[] (max n).
int  component_parts_for(ComponentType type, int *idx, int n);
// Apply by name (case-insensitive) or by table index. Returns false if it does not fit.
bool component_apply_part(Component *c, const char *part);

/* Battery chemistry. A pack derives its nominal voltage, capacity, cutoff and internal
   resistance from what it is made of and how it is wired - call component_battery_refresh after
   changing chemistry, S, P, the C rating or the per-cell capacity. */
const char *component_battery_chemistry_name(int chem);
double component_battery_cell_ocv(int chem, double soc);
double component_battery_nominal_cell(int chem);
/* The terminal voltage with nothing drawn, and how to set a pack to one. Setting the nominal is
   NOT the same thing: a full cell sits above its nominal and a flat one below. */
double component_battery_open_circuit(const Component *c);

/* The seven parts that share props.logic_gate. Written once here rather than as the same
   seven-way || at each of the places that has to ask - the properties panel offers them one set
   of rows, and every row's apply case has to agree about who it applies to. */
static inline bool component_is_logic_gate(ComponentType t) {
    return t == COMP_NOT_GATE || t == COMP_AND_GATE || t == COMP_OR_GATE ||
           t == COMP_NAND_GATE || t == COMP_NOR_GATE || t == COMP_XOR_GATE ||
           t == COMP_XNOR_GATE;
}
void   component_battery_set_open_circuit(Component *c, double v_target);
double component_battery_charge_voltage(int chem);
double component_battery_float_voltage(int chem);
void   component_battery_refresh(Component *c);
double component_battery_max_current(const Component *c);
bool component_apply_part_idx(Component *c, int idx);
// Next part for this component's type, wrapping through "" (generic) - drives the panel row.
void component_cycle_part(Component *c);

/* Capacitor companion model for one step. The branch is  C in series with ESR and ESL,
   with the leakage resistance in parallel across the whole branch:

       i = G * v_branch - Ieq        (terminal 0 -> terminal 1, excluding leakage)
       G_leak = 1/R_leak             (stamped in parallel, 0 in ideal mode)

   `ideal` (the default) zeroes ESR, ESL and leakage, so an ideal capacitor stamps exactly
   the theta-method companion it always did. */
typedef struct {
    double G;        // branch conductance to stamp
    double Ieq;      // branch Norton current to stamp
    double G_leak;   // parallel leakage conductance (0 when ideal)
    double Geq;      // C / (theta dt): the ideal capacitor part alone
    double K;        // (1 - theta) / theta, 0 at the operating point
} CapCompanion;

// dt = step, trans = a previous step exists (transient); v_prev = terminal voltage last step
CapCompanion component_cap_companion(const Component *comp, double dt, bool trans, double v_prev);
double spark_gap_breakdown(const Component *comp);     // Volts, from props.spark_gap.gap_mm
void tline_params(const Component *comp, double *R, double *L, double *C_end);   // lumped values from the per-mile data
int component_aux_count(const Component *comp);        // number of MNA auxiliary (current) variables the component needs

// Check if point is inside component
bool component_contains_point(Component *comp, float px, float py);

// Check if point is near a terminal
int component_get_terminal_at(Component *comp, float px, float py, float threshold);

// Stamp component into MNA matrix
// Previous *time-step* solution for companion models (capacitor/inductor memory terms).
// The solver sets this before Newton iteration; prev_solution passed to component_stamp is
// the current Newton iterate, which must NOT be used as the storage element's memory.
extern Vector *g_stamp_prev_step;
/* True while a stamp is being taken apart for its currents rather than to advance time: a
   stateful device must not update its memory during one. */
extern bool g_stamp_read_only;

/* Delay line: what a port launched at time t, and the per-step record of both ports. */
double delay_line_history(const Component *comp, int port, double t);
void   delay_line_record(Component *comp, double t, double v0, double i0, double v1, double i1);

void component_stamp(Component *comp, Matrix *A, Vector *b,
                     int *node_map, int num_nodes,
                     double time, Vector *prev_solution, double dt);

// Get display value string
/* Copy a saved props union onto a live component without stealing the heap buffers it owns
   (the delay line's history). Use this instead of comp->props = p anywhere props come from a
   file, the clipboard or another component. */
void component_adopt_props(Component *comp, const ComponentProps *p);
void component_get_value_string(Component *comp, char *buf, size_t buf_size);

// Update LED parameters based on color (for both LED and LED_ARRAY)
void component_update_led_color(Component *comp);

// Format engineering notation
void format_engineering(double value, const char *unit, char *buf, size_t buf_size);

// Calculate current sweep value based on time
// Returns the base_value if sweep is disabled, otherwise the swept value
double sweep_get_value(const SweepConfig *sweep, double base_value, double time);

#endif // COMPONENT_H
