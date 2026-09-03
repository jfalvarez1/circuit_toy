/**
 * Circuit Playground - Input Handling
 */

#ifndef INPUT_H
#define INPUT_H

#include <SDL.h>
#include "types.h"
#include "circuit.h"
#include "render.h"
#include "ui.h"

// Mouse button state
typedef struct {
    bool down;
    int start_x, start_y;
    int current_x, current_y;
} MouseButton;

// Property being edited
typedef enum {
    PROP_NONE = 0,
    PROP_PROBE_NAME,    // the probe's label, which is also its oscilloscope channel name
    PROP_LINE_Z0,       // delay line: characteristic impedance
    PROP_LINE_DELAY,    // delay line: one-way propagation delay
    PROP_VALUE,         // Main value (resistance, capacitance, voltage, etc.)
    PROP_FREQUENCY,
    PROP_PHASE,
    PROP_OFFSET,
    PROP_DUTY,
    PROP_AMPLITUDE,
    PROP_IDEAL,         // Generic ideal mode toggle

    // Source parameters
    PROP_R_SERIES,      // Internal series resistance
    PROP_R_PARALLEL,    // Internal parallel resistance

    // Resistor parameters
    PROP_TEMP_COEFF,    // Temperature coefficient
    PROP_TEMP,          // Operating temperature

    // Capacitor parameters
    PROP_ESR,           // Equivalent Series Resistance
    PROP_ESL,           // Equivalent Series Inductance
    PROP_LEAKAGE,       // Leakage resistance
    PROP_MAX_VOLTAGE,   // Voltage rating

    // Inductor parameters
    PROP_DCR,           // DC resistance
    PROP_I_SAT,         // Saturation current

    // Diode parameters
    PROP_BV,            // Reverse breakdown voltage
    PROP_CJO,           // Junction capacitance
    PROP_VZ,            // Zener voltage
    PROP_RZ,            // Zener impedance

    // BJT parameters
    PROP_BJT_BETA,      // Forward current gain (BF)
    PROP_BJT_IS,        // Saturation current
    PROP_BJT_VAF,       // Early voltage
    PROP_BJT_NF,        // Forward emission coefficient
    PROP_BJT_IDEAL,     // Ideal mode toggle

    // MOSFET parameters
    PROP_MOS_VTH,       // Threshold voltage
    PROP_MOS_KP,        // Transconductance parameter
    PROP_MOS_LAMBDA,    // Channel length modulation
    PROP_MOS_W,         // Channel width
    PROP_MOS_L,         // Channel length
    PROP_MOS_WL,        // W/L ratio (keeps L, scales W)
    PROP_MOS_KN,        // Device transconductance Kn = Kp (W/L) (keeps Kp and L, scales W)
    PROP_MOS_TOX,       // Gate oxide thickness (sets Cox, and so u*Cox)
    PROP_MOS_TYPE,      // Enhancement <-> depletion (flips the sign of Vth)
    PROP_MOS_IDEAL,     // Ideal mode toggle
    PROP_PART,          // Named device: cycle 2N7000 / 2N7002 / ... / generic
    PROP_CAP_VHALF,     // Ceramic DC-bias: the voltage at which the capacitance has halved

    // LED parameters
    PROP_LED_COLOR,     // Color selector (cycle through presets)
    PROP_LED_VF,        // Forward voltage
    PROP_LED_IMAX,      // Max current
    PROP_LED_ARRAY_COLOR, // LED Array color selector

    // Op-Amp parameters
    PROP_OPAMP_GAIN,    // Open-loop gain
    PROP_OPAMP_GBW,     // Gain-bandwidth product
    PROP_OPAMP_SLEW,    // Slew rate
    PROP_OPAMP_RIN,     // Input impedance
    PROP_OPAMP_ROUT,    // Output impedance
    PROP_OPAMP_VMAX,    // Positive rail
    PROP_OPAMP_VMIN,    // Negative rail
    PROP_OPAMP_R2R,     // Rail-to-rail toggle
    PROP_OPAMP_IDEAL,   // Ideal mode toggle

    // Waveform parameters
    PROP_RISE_TIME,     // Rise time
    PROP_FALL_TIME,     // Fall time
    PROP_DELAY,         // Pulse delay time
    PROP_PULSE_WIDTH,   // Pulse width time
    PROP_PERIOD,        // Pulse period time
    PROP_BANDWIDTH,     // Noise bandwidth

    // Sweep parameters
    PROP_SWEEP_VOLTAGE_ENABLE,    // Toggle voltage sweep
    PROP_SWEEP_VOLTAGE_MODE,      // Sweep mode (linear/log/step)
    PROP_SWEEP_VOLTAGE_START,     // Start value
    PROP_SWEEP_VOLTAGE_END,       // End value
    PROP_SWEEP_VOLTAGE_TIME,      // Sweep time
    PROP_SWEEP_VOLTAGE_STEPS,     // Number of steps (for stepped mode)
    PROP_SWEEP_VOLTAGE_REPEAT,    // Repeat sweep
    PROP_SWEEP_AMP_ENABLE,        // Toggle amplitude sweep
    PROP_SWEEP_AMP_MODE,          // Sweep mode
    PROP_SWEEP_AMP_START,         // Start value
    PROP_SWEEP_AMP_END,           // End value
    PROP_SWEEP_AMP_TIME,          // Sweep time
    PROP_SWEEP_AMP_STEPS,         // Number of steps
    PROP_SWEEP_AMP_REPEAT,        // Repeat sweep
    PROP_SWEEP_FREQ_ENABLE,       // Toggle frequency sweep
    PROP_SWEEP_FREQ_MODE,         // Sweep mode
    PROP_SWEEP_FREQ_START,        // Start frequency
    PROP_SWEEP_FREQ_END,          // End frequency
    PROP_SWEEP_FREQ_TIME,         // Sweep time
    PROP_SWEEP_FREQ_STEPS,        // Number of steps
    PROP_SWEEP_FREQ_REPEAT,       // Repeat sweep

    // Transformer parameters
    PROP_TRANS_R_PRIMARY,         // Primary winding resistance
    PROP_TRANS_R_SECONDARY,       // Secondary winding resistance
    PROP_TRANS_L_PRIMARY,         // Primary inductance (H): the magnetising inductance
    PROP_TRANS_COUPLING,          // Coupling coefficient k (0..1): leakage is 1 - k

    // DC motor: the armature branch and the mechanical side
    PROP_MOTOR_R,                 // Armature resistance
    PROP_MOTOR_L,                 // Armature inductance
    PROP_MOTOR_KV,                // Back-EMF constant (V per rad/s)
    PROP_MOTOR_KT,                // Torque constant (Nm/A)
    PROP_MOTOR_J,                 // Rotor inertia
    PROP_MOTOR_B,                 // Viscous friction
    PROP_MOTOR_TLOAD,             // Load torque

    // Relay coil and contacts
    PROP_RELAY_R_COIL,
    PROP_RELAY_L_COIL,
    PROP_RELAY_I_PICKUP,
    PROP_RELAY_I_DROPOUT,
    PROP_RELAY_R_ON,
    PROP_RELAY_R_OFF,

    // Controlled sources: the gain is the part
    PROP_CS_RIN,                  // input resistance (current sensing for CCVS/CCCS)

    // Battery
    PROP_BATT_CAPACITY,           // mAh
    PROP_BATT_R,                  // internal resistance
    /* A pack is a chemistry and an arrangement. These four decide the voltage, the capacity, the
       cutoff and the internal resistance between them, so they are the rows worth having. */
    PROP_BATT_CHEMISTRY,          // toggle: steps through the seven chemistries
    PROP_BATT_SERIES,             // S: cells stacked for voltage
    PROP_BATT_PARALLEL,           // P: cells alongside for capacity and current
    PROP_BATT_CRATE,              // continuous discharge, in C

    // JFET
    PROP_JFET_LAMBDA,             // channel-length modulation

    // Photoresistor / thermistor shaping
    PROP_LDR_GAMMA,               // light sensitivity exponent

    // Text annotation parameters
    PROP_TEXT_CONTENT,            // Text content string
    PROP_TEXT_SIZE,               // Font size (1=small, 2=normal, 3=large)
    PROP_TEXT_BOLD,               // Bold toggle
    PROP_TEXT_ITALIC,             // Italic toggle
    PROP_TEXT_UNDERLINE,          // Underline toggle

    // High voltage
    PROP_SPARK_GAP_MM,            // Spark gap spacing (mm)
    PROP_SPARK_GAP_RON,           // Spark gap arc resistance
    PROP_TOROID_MAJOR,            // Toroid outer diameter (in)
    PROP_TOROID_MINOR,            // Toroid tube diameter (in)
    PROP_TLINE_LENGTH, PROP_TLINE_R, PROP_TLINE_X, PROP_TLINE_B, PROP_TLINE_MODEL,   // Transmission line
    PROP_3PH_V, PROP_3PH_F, PROP_3PH_PHASE, PROP_3PH_R, PROP_3PH_L,                   // Three-phase source

    // Bode plot parameters
    PROP_BODE_FREQ_START,         // Start frequency
    PROP_BODE_FREQ_STOP,          // Stop frequency
    PROP_BODE_NUM_POINTS,         // Number of frequency points

    // Potentiometer
    PROP_WIPER_POS,               // Wiper position (0-1)

    // Photoresistor
    PROP_R_DARK,                  // Resistance in darkness
    PROP_R_LIGHT,                 // Resistance in light
    PROP_LIGHT_LEVEL,             // Current light level (0-1)

    // Thermistor
    PROP_R_25,                    // Resistance at 25°C
    PROP_BETA,                    // Beta value

    // Fuse
    PROP_RATING,                  // Current rating

    // JFET
    PROP_IDSS,                    // Drain saturation current
    PROP_VP,                      // Pinch-off voltage

    // Controlled sources
    PROP_GAIN,                    // Gain factor

    // Thyristors
    PROP_VGT,                     // Gate trigger voltage
    PROP_IGT,                     // Gate trigger current
    PROP_IH,                      // Holding current
    PROP_VBO,                     // Breakover voltage

    // Logic
    PROP_V_LOW,                   // Logic low voltage
    PROP_V_HIGH,                  // Logic high voltage
    PROP_V_THRESHOLD,             // Logic threshold voltage
    PROP_STATE,                   // Logic state (toggle)

    // 555 Timer
    PROP_R1,                      // Timing resistor 1
    PROP_R2,                      // Timing resistor 2

    // Relay
    PROP_V_COIL,                  // Coil voltage
    PROP_R_COIL,                  // Coil resistance

    // Switch
    PROP_R_ON,                    // On-state resistance
    PROP_R_OFF,                   // Off-state resistance
    PROP_SWITCH_STATE,            // Switch state (toggle)

    // Lamp
    PROP_POWER_RATING,            // Power rating
    PROP_VOLTAGE_RATING,          // Voltage rating

    // Fuse reset
    PROP_RESET_FUSE,              // Reset blown fuse

    PROP_TYPE_COUNT               /* so an audit can walk every property's action code */
} PropertyType;

// Input state
typedef struct InputState {
    // Mouse state
    MouseButton left;
    MouseButton middle;
    MouseButton right;
    int mouse_x, mouse_y;
    int wheel_delta;

    // Keyboard modifiers
    bool shift_down;
    bool ctrl_down;
    bool alt_down;

    // Current tool
    ToolType current_tool;
    ComponentType placing_component;
    int placing_rotation;  // 0, 90, 180, 270 - rotation while placing

    // Interaction state
    bool is_panning;
    bool scope_panning;          // Middle-drag over the scope: pan time (horizontal) / offset (vertical)
    bool scope_trig_dragging;    // Left-drag over the scope screen: set the trigger level
    bool is_dragging;
    Component *dragging_component;
    float drag_start_x, drag_start_y;  // Component position when drag started (for undo)

    // Multi-selection drag state
    bool is_multi_dragging;                   // Currently dragging multiple components
    float multi_drag_start_x[64];             // Initial X positions for undo
    float multi_drag_start_y[64];             // Initial Y positions for undo
    float multi_drag_ref_x, multi_drag_ref_y; // Reference point (initial mouse position)

    // Wire drawing
    bool drawing_wire;
    int wire_start_node;
    float wire_preview_x, wire_preview_y;

    // Selection
    Component *selected_component;
    int selected_wire_idx;          // Index of selected wire (-1 = none)

    // Multi-selection (box select)
    bool box_selecting;             // Currently doing box selection
    float box_start_x, box_start_y; // Box selection start in world coords
    float box_end_x, box_end_y;     // Box selection end in world coords
    Component *multi_selected[64];  // Array of multi-selected components
    int multi_selected_count;       // Number of multi-selected components

    // Probe selection and dragging
    int dragging_probe_idx;         // Index of probe being dragged (-1 = none)
    int selected_probe_idx;         // Index of selected probe (-1 = none)

    // Text input for property editing
    bool editing_property;
    PropertyType editing_prop_type;
    char input_buffer[64];
    int input_cursor;
    int input_len;

    // Pending UI action (set by ui_handle_click, processed by app)
    int pending_ui_action;

    // Simulation state (set by app to prevent editing during simulation)
    bool sim_running;
    bool sim_paused;                // simulation started but paused: switches still toggle on click

    // Auto-start flag for oscillator circuits
    bool should_autostart_sim;
} InputState;

// Initialize input state
void input_init(InputState *input);

// Process SDL event
// Returns true if event was handled
bool input_handle_event(InputState *input, SDL_Event *event,
                        Circuit *circuit, RenderContext *render,
                        UIState *ui);

// Handle keyboard shortcut
void input_handle_key(InputState *input, SDL_Keycode key,
                      Circuit *circuit, RenderContext *render);

// Update cursor for current state
void input_update_cursor(InputState *input);

// Set current tool
void input_set_tool(InputState *input, ToolType tool);

// Start placing a component
void input_start_placing(InputState *input, ComponentType type);

// Cancel current action
void input_cancel_action(InputState *input);
/* Drop every pointer into the circuit - call whenever the circuit is replaced wholesale */
void input_forget_circuit(InputState *input);

// Delete selected component
void input_delete_selected(InputState *input, Circuit *circuit);

// Copy/cut/paste operations
void input_copy(InputState *input, Circuit *circuit);
void input_cut(InputState *input, Circuit *circuit);
void input_paste(InputState *input, Circuit *circuit, RenderContext *render);
void input_duplicate(InputState *input, Circuit *circuit);

// Property text editing
void input_start_property_edit(InputState *input, PropertyType prop, const char *initial_value);
void input_cancel_property_edit(InputState *input);
bool input_apply_property_edit(InputState *input, Component *comp);
/* True for the panel rows that are clicked rather than typed into (model toggles, the
   part picker, the sweep switches): those never reach the apply switch. */
bool property_is_toggle(int prop_type);
void input_handle_text_input(InputState *input, const char *text);
void input_handle_text_key(InputState *input, SDL_Keycode key);

// Parse value with engineering notation (supports k, M, G, m, u, n, p suffixes)
double parse_engineering_value(const char *str);

#endif // INPUT_H
