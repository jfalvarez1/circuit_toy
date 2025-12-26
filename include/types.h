/**
 * Circuit Playground - Common Type Definitions
 *
 * TODO: Fix subcircuit functionality - needs proper implementation
 * TODO: Fix opamp oscillation models - add GBW dynamics for sustained oscillation
 */

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>

// Define M_PI if not available (MSVC)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Window dimensions
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define TOOLBAR_HEIGHT 50
#define PALETTE_WIDTH 160
#define PROPERTIES_WIDTH 420
#define STATUSBAR_HEIGHT 24

// Canvas area
#define CANVAS_X PALETTE_WIDTH
#define CANVAS_Y TOOLBAR_HEIGHT
#define CANVAS_WIDTH (WINDOW_WIDTH - PALETTE_WIDTH - PROPERTIES_WIDTH)
#define CANVAS_HEIGHT (WINDOW_HEIGHT - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT)

// Grid settings
#define GRID_SIZE 10
#define MAX_ZOOM 4.0f
#define MIN_ZOOM 0.25f

// Limits
#define MAX_COMPONENTS 1024
#define MAX_NODES 2048
#define MAX_WIRES 2048
#define MAX_PROBES 8
#define MAX_LABEL_LEN 32
#define MAX_HISTORY 10000

// Component types
typedef enum {
    COMP_NONE = 0,

    // === PASSIVE COMPONENTS ===
    COMP_GROUND,
    COMP_RESISTOR,
    COMP_CAPACITOR,
    COMP_CAPACITOR_ELEC,    // Electrolytic capacitor (polarized)
    COMP_INDUCTOR,
    COMP_POTENTIOMETER,     // Variable resistor (3 terminals)
    COMP_PHOTORESISTOR,     // Light-dependent resistor (LDR)
    COMP_THERMISTOR,        // Temperature-dependent resistor (NTC/PTC)
    COMP_MEMRISTOR,         // Memory resistor
    COMP_FUSE,              // Overcurrent protection
    COMP_CRYSTAL,           // Quartz crystal oscillator
    COMP_SPARK_GAP,         // Overvoltage protection

    // === VOLTAGE/CURRENT SOURCES ===
    COMP_DC_VOLTAGE,
    COMP_AC_VOLTAGE,
    COMP_DC_CURRENT,
    COMP_AC_CURRENT,        // AC current source
    COMP_CLOCK,             // Digital clock source
    COMP_VADC_SOURCE,       // Variable amplitude DC source
    COMP_AM_SOURCE,         // Amplitude modulated source
    COMP_FM_SOURCE,         // Frequency modulated source
    COMP_BATTERY,           // Battery with discharge model

    // === WAVEFORM GENERATORS ===
    COMP_SQUARE_WAVE,
    COMP_TRIANGLE_WAVE,
    COMP_SAWTOOTH_WAVE,
    COMP_NOISE_SOURCE,
    COMP_PULSE_SOURCE,      // Pulse generator with configurable width
    COMP_PWM_SOURCE,        // PWM signal generator
    COMP_PWL_SOURCE,        // Piecewise linear voltage source
    COMP_EXPR_SOURCE,       // Expression-based voltage source V(t)

    // === DIODES ===
    COMP_DIODE,
    COMP_ZENER,             // Zener diode
    COMP_SCHOTTKY,          // Schottky diode
    COMP_LED,               // Light-emitting diode
    COMP_VARACTOR,          // Variable capacitance diode
    COMP_TUNNEL_DIODE,      // Negative resistance diode
    COMP_PHOTODIODE,        // Light-sensitive diode

    // === TRANSISTORS - BJT ===
    COMP_NPN_BJT,
    COMP_PNP_BJT,
    COMP_NPN_DARLINGTON,    // NPN Darlington pair
    COMP_PNP_DARLINGTON,    // PNP Darlington pair

    // === TRANSISTORS - FET ===
    COMP_NMOS,
    COMP_PMOS,
    COMP_NJFET,             // N-channel JFET
    COMP_PJFET,             // P-channel JFET

    // === THYRISTORS ===
    COMP_SCR,               // Silicon controlled rectifier
    COMP_DIAC,              // Diode for alternating current
    COMP_TRIAC,             // Triode for alternating current
    COMP_UJT,               // Unijunction transistor

    // === OP-AMPS & AMPLIFIERS ===
    COMP_OPAMP,
    COMP_OPAMP_FLIPPED,     // Op-amp with + and - inputs swapped (+ on top, - on bottom)
    COMP_OPAMP_REAL,        // Op-amp with finite gain, bandwidth, input/output impedance
    COMP_OTA,               // Operational transconductance amplifier
    COMP_CCII_PLUS,         // Current conveyor II+
    COMP_CCII_MINUS,        // Current conveyor II-

    // === CONTROLLED SOURCES ===
    COMP_VCVS,              // Voltage-controlled voltage source
    COMP_VCCS,              // Voltage-controlled current source
    COMP_CCVS,              // Current-controlled voltage source
    COMP_CCCS,              // Current-controlled current source

    // === SWITCHES ===
    COMP_SPST_SWITCH,       // Single-pole single-throw switch
    COMP_SPDT_SWITCH,       // Single-pole double-throw switch
    COMP_DPDT_SWITCH,       // Double-pole double-throw switch
    COMP_PUSH_BUTTON,       // Momentary push button (normally open)
    COMP_RELAY,             // Electromechanical relay
    COMP_ANALOG_SWITCH,     // Voltage-controlled analog switch

    // === TRANSFORMERS ===
    COMP_TRANSFORMER,       // Two-winding transformer
    COMP_TRANSFORMER_CT,    // Center-tapped transformer (3 secondary terminals)

    // === LOGIC GATES ===
    COMP_LOGIC_INPUT,       // Logic high/low input
    COMP_LOGIC_OUTPUT,      // Logic output indicator
    COMP_NOT_GATE,          // Inverter
    COMP_AND_GATE,
    COMP_OR_GATE,
    COMP_NAND_GATE,
    COMP_NOR_GATE,
    COMP_XOR_GATE,
    COMP_XNOR_GATE,
    COMP_BUFFER,            // Non-inverting buffer
    COMP_TRISTATE_BUF,      // Tri-state buffer
    COMP_SCHMITT_INV,       // Schmitt trigger inverter
    COMP_SCHMITT_BUF,       // Schmitt trigger buffer

    // === DIGITAL ICS ===
    COMP_D_FLIPFLOP,        // D flip-flop
    COMP_JK_FLIPFLOP,       // JK flip-flop
    COMP_T_FLIPFLOP,        // T (toggle) flip-flop
    COMP_SR_LATCH,          // SR latch
    COMP_COUNTER,           // Binary counter
    COMP_SHIFT_REG,         // Shift register
    COMP_MUX_2TO1,          // 2-to-1 multiplexer
    COMP_DEMUX_1TO2,        // 1-to-2 demultiplexer
    COMP_DECODER,           // Binary decoder
    COMP_BCD_DECODER,       // BCD to 7-segment decoder (7447/74LS47)
    COMP_HALF_ADDER,        // Half adder
    COMP_FULL_ADDER,        // Full adder

    // === MIXED SIGNAL ===
    COMP_555_TIMER,         // 555 timer IC
    COMP_DAC,               // Digital-to-analog converter
    COMP_ADC,               // Analog-to-digital converter
    COMP_VCO,               // Voltage-controlled oscillator
    COMP_PLL,               // Phase-locked loop (simplified)
    COMP_MONOSTABLE,        // Monostable multivibrator (one-shot)
    COMP_OPTOCOUPLER,       // Optical isolator

    // === VOLTAGE REGULATORS ===
    COMP_LM317,             // Adjustable voltage regulator
    COMP_7805,              // 5V fixed regulator
    COMP_TL431,             // Programmable shunt regulator

    // === DISPLAY/OUTPUT ===
    COMP_LAMP,              // Indicator lamp
    COMP_7SEG_DISPLAY,      // 7-segment LED display
    COMP_LED_ARRAY,         // LED bar graph
    COMP_LED_MATRIX,        // 8x8 LED dot matrix display
    COMP_DC_MOTOR,          // DC motor

    // === WIRELESS ===
    COMP_ANTENNA_TX,        // Transmitter antenna
    COMP_ANTENNA_RX,        // Receiver antenna

    // === WIRING ===
    COMP_BUS,               // Wire bundle (multi-wire bus)
    COMP_BUS_TAP,           // Bus tap (extract single wire from bus)

    // === MEASUREMENT ===
    COMP_VOLTMETER,         // Voltage measurement point
    COMP_AMMETER,           // Current measurement point
    COMP_WATTMETER,         // Power measurement
    COMP_TEST_POINT,        // Test point marker

    // === ANNOTATION ===
    COMP_TEXT,
    COMP_LABEL,             // Named node label

    // === SUB-CIRCUITS ===
    COMP_PIN,               // Pin marker for subcircuit creation (has pin_number property)
    COMP_SUBCIRCUIT,        // User-defined sub-circuit / IC block

    COMP_TYPE_COUNT
} ComponentType;

// Oscilloscope trigger modes
typedef enum {
    TRIG_AUTO = 0,      // Always triggers, free-running if no signal
    TRIG_NORMAL,        // Only triggers on valid edge
    TRIG_SINGLE         // Single shot - triggers once then stops
} TriggerMode;

// Oscilloscope trigger edge
typedef enum {
    TRIG_EDGE_RISING = 0,
    TRIG_EDGE_FALLING,
    TRIG_EDGE_BOTH
} TriggerEdge;

// Oscilloscope display mode
typedef enum {
    SCOPE_MODE_YT = 0,  // Normal time-domain
    SCOPE_MODE_XY       // X-Y mode (Lissajous)
} ScopeDisplayMode;

// Source sweep modes
typedef enum {
    SWEEP_NONE = 0,     // No sweep - constant value
    SWEEP_LINEAR,       // Linear sweep from start to end
    SWEEP_LOG,          // Logarithmic sweep (for frequency)
    SWEEP_STEP          // Step through discrete values
} SweepMode;

// Sweep configuration for a source parameter
typedef struct {
    bool enabled;           // Sweep is active
    SweepMode mode;         // Type of sweep
    double start_value;     // Starting value
    double end_value;       // Ending value
    double sweep_time;      // Time to complete one sweep (seconds)
    int num_steps;          // For stepped mode: number of discrete steps
    bool repeat;            // Repeat sweep when complete (otherwise hold at end)
    bool bidirectional;     // Sweep back and forth (triangle pattern)
} SweepConfig;

// Tool types
typedef enum {
    TOOL_SELECT = 0,
    TOOL_WIRE,
    TOOL_DELETE,
    TOOL_PROBE,
    TOOL_COMPONENT
} ToolType;

// Simulation state
typedef enum {
    SIM_STOPPED = 0,
    SIM_RUNNING,
    SIM_PAUSED
} SimState;

// ============================================================================
// Mixed-Signal / Digital Logic Types
// ============================================================================

// Logic state (3-state + unknown for mixed-signal simulation)
typedef enum {
    LOGIC_LOW = 0,      // Below low threshold (typically 0V)
    LOGIC_HIGH = 1,     // Above high threshold (typically VCC)
    LOGIC_Z = 2,        // High impedance (floating/tri-state)
    LOGIC_X = 3         // Unknown/conflict (multiple drivers or undefined)
} LogicState;

// Logic family (determines voltage levels and thresholds)
typedef enum {
    LOGIC_FAMILY_TTL = 0,    // TTL: VIL=0.8V, VIH=2.0V, VOL=0.4V, VOH=2.4V
    LOGIC_FAMILY_CMOS_5V,    // 5V CMOS: VIL=1.5V, VIH=3.5V, VOL=0V, VOH=5V
    LOGIC_FAMILY_CMOS_3V3,   // 3.3V CMOS: VIL=0.8V, VIH=2.0V, VOL=0V, VOH=3.3V
    LOGIC_FAMILY_LVCMOS,     // Low-voltage CMOS (1.8V)
    LOGIC_FAMILY_CUSTOM      // User-defined thresholds
} LogicFamily;

// Edge type for sequential logic
typedef enum {
    EDGE_NONE = 0,
    EDGE_RISING,
    EDGE_FALLING
} EdgeType;

// Logic timing parameters
typedef struct {
    double prop_delay_lh;    // Propagation delay low-to-high (seconds)
    double prop_delay_hl;    // Propagation delay high-to-low (seconds)
    double rise_time;        // Output rise time (seconds)
    double fall_time;        // Output fall time (seconds)
} LogicTiming;

// Logic level configuration (ADC/DAC bridge thresholds)
typedef struct {
    double v_il;             // Input low threshold (max voltage for LOW)
    double v_ih;             // Input high threshold (min voltage for HIGH)
    double v_ol;             // Output low voltage
    double v_oh;             // Output high voltage
    double v_hyst;           // Schmitt trigger hysteresis (0 = no hysteresis)
    double r_out;            // Output source impedance
} LogicLevels;

// Maximum number of logic inputs/outputs per component
#define MAX_LOGIC_INPUTS 8
#define MAX_LOGIC_OUTPUTS 8

// ============================================================================
// Logic Gate State Structure (for mixed-signal simulation)
// ============================================================================

// Per-component logic state (stored in each logic component)
typedef struct {
    // Current input states (sampled from analog nodes)
    LogicState inputs[MAX_LOGIC_INPUTS];
    LogicState prev_inputs[MAX_LOGIC_INPUTS];  // For edge detection

    // Current output states (driven to analog nodes)
    LogicState outputs[MAX_LOGIC_OUTPUTS];
    LogicState prev_outputs[MAX_LOGIC_OUTPUTS];

    // Sequential logic internal state
    LogicState q;           // Flip-flop Q output
    LogicState q_bar;       // Flip-flop Q-bar output
    LogicState sr_set;      // SR latch set state
    LogicState sr_reset;    // SR latch reset state

    // Schmitt trigger state (for hysteresis)
    bool schmitt_state;     // Current output level (used for hysteresis)

    // Timing state
    double last_change_time;    // Time of last output change
    bool output_pending;        // Output change is pending (propagation delay)

    // Logic level configuration
    LogicLevels levels;
    LogicFamily family;

    // Flags
    bool is_logic_component;    // True if this component uses logic abstraction
    bool outputs_dirty;         // Outputs need to be re-propagated
} LogicGateState;

// 2D Point
typedef struct {
    float x;
    float y;
} Point2D;

// Integer point (for grid)
typedef struct {
    int x;
    int y;
} Point2Di;

// Rectangle
typedef struct {
    int x, y, w, h;
} Rect;

// Color (RGBA)
typedef struct {
    uint8_t r, g, b, a;
} Color;

// Predefined colors
#define COLOR_BG         (Color){0x1a, 0x1a, 0x2e, 0xff}
#define COLOR_BG_DARK    (Color){0x16, 0x21, 0x3e, 0xff}
#define COLOR_ACCENT     (Color){0x00, 0xd9, 0xff, 0xff}
#define COLOR_ACCENT2    (Color){0xe9, 0x45, 0x60, 0xff}
#define COLOR_TEXT       (Color){0xff, 0xff, 0xff, 0xff}
#define COLOR_TEXT_DIM   (Color){0xb0, 0xb0, 0xb0, 0xff}
#define COLOR_GRID       (Color){0x2a, 0x2a, 0x4e, 0xff}
#define COLOR_SUCCESS    (Color){0x00, 0xff, 0x88, 0xff}
#define COLOR_WARNING    (Color){0xff, 0xaa, 0x00, 0xff}
#define COLOR_DANGER     (Color){0xff, 0x44, 0x44, 0xff}
#define COLOR_WIRE       (Color){0x00, 0xd9, 0xff, 0xff}

// Utility macros
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi) (MIN(MAX(x, lo), hi))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Snap to grid (handles negative values correctly using round)
static inline int snap_to_grid(float val) {
    return (int)round(val / GRID_SIZE) * GRID_SIZE;
}

// ============================================================================
// Global Environment Settings
// ============================================================================
// These affect LDR (photoresistor) and thermistor components globally

typedef struct {
    double light_level;     // Global light level (0.0=dark to 1.0=bright), default: 0.5
    double temperature;     // Global ambient temperature (°C), default: 25.0
} EnvironmentState;

// Global environment instance (defined in component.c)
extern EnvironmentState g_environment;

// ============================================================================
// Thermal & Failure State (for destructive component failure / magic smoke)
// ============================================================================

// Maximum smoke particles per component
#define MAX_SMOKE_PARTICLES 8

// Smoke particle for visual effect
typedef struct {
    float x, y;           // Position relative to component
    float vx, vy;         // Velocity
    float life;           // Remaining lifetime (0-1)
    float size;           // Particle size
    uint8_t alpha;        // Current alpha
} SmokeParticle;

// Thermal state for a component (tracks temperature and failure)
typedef struct {
    double temperature;           // Current temperature (°C)
    double ambient_temperature;   // Ambient temperature (°C), default 25
    double power_dissipated;      // Current power dissipation (W)
    double thermal_mass;          // Thermal mass/capacity (J/°C)
    double thermal_resistance;    // Thermal resistance to ambient (°C/W)
    double max_temperature;       // Maximum safe temperature (°C)
    double damage;                // Accumulated thermal damage (0-1, 1=failed)
    double damage_threshold;      // Power rating multiplier where damage starts
    double failure_time;          // Simulation time when component failed (-1 if intact)
    bool failed;                  // Component has failed (magic smoke released)
    bool smoke_active;            // Smoke particles are active
    SmokeParticle smoke[MAX_SMOKE_PARTICLES];  // Smoke particles
    int num_smoke;                // Active smoke particle count
} ThermalState;

// ============================================================================
// SUB-CIRCUIT / IC DEFINITION
// ============================================================================

// Maximum components/wires in a sub-circuit
#define MAX_SUBCIRCUIT_COMPONENTS 64
#define MAX_SUBCIRCUIT_WIRES 128
#define MAX_SUBCIRCUIT_PINS 16
#define MAX_SUBCIRCUIT_DEFS 32

// Pin definition for a sub-circuit (external connection point)
typedef struct {
    char name[16];          // Pin name (e.g., "VCC", "GND", "IN1", "OUT")
    int internal_node_id;   // Internal node ID this pin connects to
    int side;               // 0=left, 1=right, 2=top, 3=bottom
    int position;           // Position along the side (0 = first)
} SubCircuitPin;

// Forward declaration - full definition in circuit.h
struct Wire;
struct Component;

// Sub-circuit definition (template)
typedef struct {
    int id;                                     // Unique definition ID
    char name[32];                              // Sub-circuit name (e.g., "Half Adder")
    char description[128];                      // Optional description

    // Internal circuit (stored as copies, not pointers to avoid dangling refs)
    int num_components;
    int num_wires;
    int num_pins;

    // Component data stored as bytes (serialized)
    // When instantiating, we deserialize and create fresh components
    void *component_data;                       // Serialized component array
    void *wire_data;                            // Serialized wire array
    size_t component_data_size;
    size_t wire_data_size;

    // Pin definitions (exposed terminals)
    SubCircuitPin pins[MAX_SUBCIRCUIT_PINS];

    // Bounding box for internal circuit (for rendering preview)
    float internal_width;
    float internal_height;

    // Visual size of the IC block
    float block_width;
    float block_height;

    // Number of unique internal nodes (for matrix sizing during simulation)
    // This is the count of internal nodes EXCLUDING those exposed as pins
    int num_internal_nodes;
} SubCircuitDef;

// Global sub-circuit library
typedef struct {
    SubCircuitDef defs[MAX_SUBCIRCUIT_DEFS];
    int count;
    int next_id;
} SubCircuitLibrary;

// Global subcircuit library (defined in component.c)
extern SubCircuitLibrary g_subcircuit_library;

// ============================================================================
// WIRELESS ANTENNA STATE
// ============================================================================

// Number of wireless channels available
#define WIRELESS_CHANNEL_COUNT 16

// Wireless channel state - stores TX voltages for each channel
typedef struct {
    double voltage[WIRELESS_CHANNEL_COUNT];   // Voltage being transmitted on each channel
    int tx_count[WIRELESS_CHANNEL_COUNT];     // Number of TX antennas on each channel
} WirelessState;

// Global wireless state (defined in simulation.c)
extern WirelessState g_wireless;

#endif // TYPES_H
