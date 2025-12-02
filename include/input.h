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
    PROP_MOS_IDEAL,     // Ideal mode toggle

    // LED parameters
    PROP_LED_COLOR,     // Color selector (cycle through presets)
    PROP_LED_VF,        // Forward voltage
    PROP_LED_IMAX,      // Max current

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

    // Text annotation parameters
    PROP_TEXT_CONTENT,            // Text content string
    PROP_TEXT_SIZE,               // Font size (1=small, 2=normal, 3=large)
    PROP_TEXT_BOLD,               // Bold toggle
    PROP_TEXT_ITALIC,             // Italic toggle
    PROP_TEXT_UNDERLINE,          // Underline toggle

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

    // Microphone
    PROP_MIC_ENABLED,             // Microphone capture enabled toggle
    PROP_MIC_GAIN,                // Microphone input gain
    PROP_MIC_AMPLITUDE,           // Output amplitude
    PROP_MIC_OFFSET               // DC offset
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
    bool is_dragging;
    Component *dragging_component;

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
void input_handle_text_input(InputState *input, const char *text);
void input_handle_text_key(InputState *input, SDL_Keycode key);

// Parse value with engineering notation (supports k, M, G, m, u, n, p suffixes)
double parse_engineering_value(const char *str);

#endif // INPUT_H
