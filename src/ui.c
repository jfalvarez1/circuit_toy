/**
 * Circuit Playground - UI System Implementation
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ui.h"
#include "input.h"
#include "circuits.h"
#include "analysis.h"

// Simple 8x8 bitmap font (same as render.c)
static const unsigned char ui_font8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // '!'
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // '"'
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // '#'
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, // '$'
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // '%'
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // '&'
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, // '''
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, // '('
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, // ')'
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // '*'
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, // '+'
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, // ','
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, // '-'
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, // '.'
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // '/'
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, // '0'
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, // '1'
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, // '2'
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, // '3'
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, // '4'
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, // '5'
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, // '6'
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, // '7'
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, // '8'
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, // '9'
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, // ':'
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06}, // ';'
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, // '<'
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, // '='
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // '>'
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, // '?'
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, // '@'
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, // 'A'
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, // 'B'
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, // 'C'
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, // 'D'
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, // 'E'
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, // 'F'
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, // 'G'
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, // 'H'
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 'I'
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, // 'J'
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, // 'K'
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, // 'L'
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, // 'M'
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, // 'N'
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // 'O'
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, // 'P'
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, // 'Q'
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, // 'R'
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, // 'S'
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 'T'
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, // 'U'
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, // 'V'
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // 'W'
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, // 'X'
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, // 'Y'
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, // 'Z'
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, // '['
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, // '\'
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, // ']'
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // '^'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // '_'
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // '`'
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, // 'a'
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, // 'b'
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, // 'c'
    {0x38,0x30,0x30,0x3e,0x33,0x33,0x6E,0x00}, // 'd'
    {0x00,0x00,0x1E,0x33,0x3f,0x03,0x1E,0x00}, // 'e'
    {0x1C,0x36,0x06,0x0f,0x06,0x06,0x0F,0x00}, // 'f'
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, // 'g'
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, // 'h'
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, // 'i'
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, // 'j'
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, // 'k'
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 'l'
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, // 'm'
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, // 'n'
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, // 'o'
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, // 'p'
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, // 'q'
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, // 'r'
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, // 's'
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, // 't'
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, // 'u'
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, // 'v'
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, // 'w'
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, // 'x'
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, // 'y'
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, // 'z'
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, // '{'
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // '|'
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, // '}'
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, // '~'
};

static void ui_draw_char(SDL_Renderer *r, char c, int x, int y) {
    if (c < 32 || c > 126) c = '?';
    const unsigned char *glyph = ui_font8x8[c - 32];
    for (int row = 0; row < 8; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (1 << col)) {
                SDL_RenderDrawPoint(r, x + col, y + row);
            }
        }
    }
}

static void ui_draw_text(SDL_Renderer *r, const char *text, int x, int y) {
    if (!text) return;  // Safety check for NULL
    while (*text) {
        ui_draw_char(r, *text, x, y);
        x += 8;
        text++;
    }
}

void ui_init(UIState *ui) {
    memset(ui, 0, sizeof(UIState));

    // Initialize collapsible categories
    ui->categories[PCAT_TOOLS] = (PaletteCategory){"Tools", false, 0};
    ui->categories[PCAT_SOURCES] = (PaletteCategory){"Sources", false, 0};
    ui->categories[PCAT_WAVEFORMS] = (PaletteCategory){"Waveforms", false, 0};
    ui->categories[PCAT_PASSIVES] = (PaletteCategory){"Passives", false, 0};
    ui->categories[PCAT_DIODES] = (PaletteCategory){"Diodes", false, 0};
    ui->categories[PCAT_BJT] = (PaletteCategory){"BJT", false, 0};
    ui->categories[PCAT_FET] = (PaletteCategory){"FET", false, 0};
    ui->categories[PCAT_THYRISTORS] = (PaletteCategory){"Thyristors", false, 0};
    ui->categories[PCAT_OPAMPS] = (PaletteCategory){"Op-Amps", false, 0};
    ui->categories[PCAT_CONTROLLED] = (PaletteCategory){"Ctrl Sources", false, 0};
    ui->categories[PCAT_SWITCHES] = (PaletteCategory){"Switches", false, 0};
    ui->categories[PCAT_TRANSFORMERS] = (PaletteCategory){"Transformers", false, 0};
    ui->categories[PCAT_LOGIC] = (PaletteCategory){"Logic Gates", false, 0};
    ui->categories[PCAT_DIGITAL] = (PaletteCategory){"Digital ICs", false, 0};
    ui->categories[PCAT_MIXED] = (PaletteCategory){"Mixed Signal", false, 0};
    ui->categories[PCAT_REGULATORS] = (PaletteCategory){"Regulators", false, 0};
    ui->categories[PCAT_DISPLAY] = (PaletteCategory){"Display", false, 0};
    ui->categories[PCAT_MEASUREMENT] = (PaletteCategory){"Measurement", false, 0};
    ui->categories[PCAT_CIRCUITS] = (PaletteCategory){"Circuits", false, 0};

    // Set initial window dimensions
    ui->window_width = WINDOW_WIDTH;
    ui->window_height = WINDOW_HEIGHT;

    // Initialize properties panel width
    ui->properties_width = PROPERTIES_WIDTH;
    ui->props_resizing = false;
    ui->properties_content_height = 200;  // Default height, updated dynamically

    // Initialize toolbar buttons
    int btn_x = 200;
    int btn_w = 60, btn_h = 30;

    ui->btn_run = (Button){{btn_x, 10, btn_w, btn_h}, "Run", "Start simulation (F5)", false, false, true, false};
    btn_x += btn_w + 10;
    ui->btn_pause = (Button){{btn_x, 10, btn_w, btn_h}, "Pause", "Pause simulation (F6)", false, false, false, false};
    btn_x += btn_w + 10;
    ui->btn_step = (Button){{btn_x, 10, btn_w, btn_h}, "Step", "Single step (F10)", false, false, true, false};
    btn_x += btn_w + 10;
    ui->btn_reset = (Button){{btn_x, 10, btn_w, btn_h}, "Reset", "Reset simulation (F4)", false, false, true, false};
    btn_x += btn_w + 30;
    ui->btn_clear = (Button){{btn_x, 10, btn_w, btn_h}, "Clear", "Clear circuit", false, false, true, false};
    btn_x += btn_w + 10;
    ui->btn_save = (Button){{btn_x, 10, btn_w, btn_h}, "Save", "Save circuit (Ctrl+S)", false, false, true, false};
    btn_x += btn_w + 10;
    ui->btn_load = (Button){{btn_x, 10, btn_w, btn_h}, "Load", "Load circuit (Ctrl+O)", false, false, true, false};
    btn_x += btn_w + 10;
    ui->btn_export_svg = (Button){{btn_x, 10, btn_w, btn_h}, "SVG", "Export as SVG", false, false, true, false};

    // Speed slider
    ui->speed_slider = (Rect){btn_x + btn_w + 30, 15, 100, 20};
    ui->speed_value = 1.0f;

    // Time step controls - positioned after speed slider text
    int ts_x = ui->speed_slider.x + 50 + ui->speed_slider.w + 60;  // After speed slider + text
    ui->timestep_display_x = ts_x;
    ui->btn_timestep_down = (Button){{ts_x + 55, 12, 20, 20}, "-", "Decrease time step", false, false, true, false};
    ui->btn_timestep_up = (Button){{ts_x + 77, 12, 20, 20}, "+", "Increase time step", false, false, true, false};
    ui->btn_timestep_auto = (Button){{ts_x + 100, 10, 40, 24}, "Auto", "Auto time step", false, false, true, false};
    ui->display_time_step = 1e-7;  // Default 100 nanoseconds (will be updated from simulation)

    // Environment sliders (positioned in status bar area - will be updated in ui_update_layout)
    // These control global light/temperature for LDR and thermistor components
    ui->env_light_slider = (Rect){0, 0, 80, 14};   // Will be positioned in render
    ui->env_temp_slider = (Rect){0, 0, 80, 14};    // Will be positioned in render
    ui->dragging_light = false;
    ui->dragging_temp = false;

    // Initialize palette items
    int pal_y = TOOLBAR_HEIGHT + 18;
    int pal_h = 35;
    int col = 0;

    // Helper macro to add a palette item
    #define ADD_TOOL(tool, lbl) do { \
        ui->palette_items[ui->num_palette_items++] = (PaletteItem){ \
            {10 + col*70, pal_y, 60, pal_h}, COMP_NONE, tool, true, lbl, false, (tool == TOOL_SELECT) \
        }; \
        col++; if (col >= 2) { col = 0; pal_y += pal_h + 3; } \
    } while(0)

    #define ADD_COMP(comp, lbl) do { \
        ui->palette_items[ui->num_palette_items++] = (PaletteItem){ \
            {10 + col*70, pal_y, 60, pal_h}, comp, TOOL_COMPONENT, false, lbl, false, false \
        }; \
        col++; if (col >= 2) { col = 0; pal_y += pal_h + 3; } \
    } while(0)

    #define NEW_SECTION() do { col = 0; pal_y += pal_h + 15; } while(0)

    // === TOOLS SECTION (index 0) ===
    pal_y += 4;
    ADD_TOOL(TOOL_SELECT, "Select");
    ADD_TOOL(TOOL_WIRE, "Wire");
    ADD_TOOL(TOOL_DELETE, "Delete");
    ADD_TOOL(TOOL_PROBE, "Probe");
    ADD_COMP(COMP_TEXT, "Text");

    // === SOURCES SECTION (index 5) ===
    NEW_SECTION();
    ADD_COMP(COMP_GROUND, "GND");
    ADD_COMP(COMP_DC_VOLTAGE, "DC V");
    ADD_COMP(COMP_AC_VOLTAGE, "AC V");
    ADD_COMP(COMP_DC_CURRENT, "DC I");
    ADD_COMP(COMP_AC_CURRENT, "AC I");
    ADD_COMP(COMP_CLOCK, "Clock");

    // === WAVEFORMS SECTION (index 11) ===
    NEW_SECTION();
    ADD_COMP(COMP_SQUARE_WAVE, "Square");
    ADD_COMP(COMP_TRIANGLE_WAVE, "Tri");
    ADD_COMP(COMP_SAWTOOTH_WAVE, "Saw");
    ADD_COMP(COMP_NOISE_SOURCE, "Noise");
    ADD_COMP(COMP_PULSE_SOURCE, "Pulse");
    ADD_COMP(COMP_PWM_SOURCE, "PWM");

    // === PASSIVES SECTION (index 17) ===
    NEW_SECTION();
    ADD_COMP(COMP_RESISTOR, "R");
    ADD_COMP(COMP_CAPACITOR, "C");
    ADD_COMP(COMP_CAPACITOR_ELEC, "Elec");
    ADD_COMP(COMP_INDUCTOR, "L");
    ADD_COMP(COMP_POTENTIOMETER, "Pot");
    ADD_COMP(COMP_CRYSTAL, "Xtal");
    ADD_COMP(COMP_FUSE, "Fuse");
    ADD_COMP(COMP_THERMISTOR, "Therm");

    // === DIODES SECTION (index 25) ===
    NEW_SECTION();
    ADD_COMP(COMP_DIODE, "Diode");
    ADD_COMP(COMP_ZENER, "Zener");
    ADD_COMP(COMP_SCHOTTKY, "Schky");
    ADD_COMP(COMP_LED, "LED");
    ADD_COMP(COMP_VARACTOR, "Varac");
    ADD_COMP(COMP_PHOTODIODE, "Photo");

    // === TRANSISTORS - BJT SECTION (index 31) ===
    NEW_SECTION();
    ADD_COMP(COMP_NPN_BJT, "NPN");
    ADD_COMP(COMP_PNP_BJT, "PNP");
    ADD_COMP(COMP_NPN_DARLINGTON, "NPN-D");
    ADD_COMP(COMP_PNP_DARLINGTON, "PNP-D");

    // === TRANSISTORS - FET SECTION (index 35) ===
    NEW_SECTION();
    ADD_COMP(COMP_NMOS, "NMOS");
    ADD_COMP(COMP_PMOS, "PMOS");
    ADD_COMP(COMP_NJFET, "NJFET");
    ADD_COMP(COMP_PJFET, "PJFET");

    // === THYRISTORS SECTION (index 39) ===
    NEW_SECTION();
    ADD_COMP(COMP_SCR, "SCR");
    ADD_COMP(COMP_DIAC, "DIAC");
    ADD_COMP(COMP_TRIAC, "TRIAC");
    ADD_COMP(COMP_UJT, "UJT");

    // === OP-AMPS & AMPLIFIERS SECTION (index 43) ===
    NEW_SECTION();
    ADD_COMP(COMP_OPAMP, "OpAmp");
    ADD_COMP(COMP_OPAMP_FLIPPED, "OpFlip");
    ADD_COMP(COMP_OPAMP_REAL, "OpReal");
    ADD_COMP(COMP_OTA, "OTA");

    // === CONTROLLED SOURCES SECTION (index 47) ===
    NEW_SECTION();
    ADD_COMP(COMP_VCVS, "VCVS");
    ADD_COMP(COMP_VCCS, "VCCS");
    ADD_COMP(COMP_CCVS, "CCVS");
    ADD_COMP(COMP_CCCS, "CCCS");

    // === SWITCHES SECTION (index 51) ===
    NEW_SECTION();
    ADD_COMP(COMP_SPST_SWITCH, "SPST");
    ADD_COMP(COMP_SPDT_SWITCH, "SPDT");
    ADD_COMP(COMP_DPDT_SWITCH, "DPDT");
    ADD_COMP(COMP_PUSH_BUTTON, "PushB");
    ADD_COMP(COMP_RELAY, "Relay");
    ADD_COMP(COMP_ANALOG_SWITCH, "AnaSw");

    // === TRANSFORMERS SECTION (index 57) ===
    NEW_SECTION();
    ADD_COMP(COMP_TRANSFORMER, "Xfmr");
    ADD_COMP(COMP_TRANSFORMER_CT, "XfmrCT");

    // === LOGIC GATES SECTION (index 59) ===
    NEW_SECTION();
    ADD_COMP(COMP_LOGIC_INPUT, "LogIn");
    ADD_COMP(COMP_LOGIC_OUTPUT, "LogOut");
    ADD_COMP(COMP_NOT_GATE, "NOT");
    ADD_COMP(COMP_AND_GATE, "AND");
    ADD_COMP(COMP_OR_GATE, "OR");
    ADD_COMP(COMP_NAND_GATE, "NAND");
    ADD_COMP(COMP_NOR_GATE, "NOR");
    ADD_COMP(COMP_XOR_GATE, "XOR");
    ADD_COMP(COMP_XNOR_GATE, "XNOR");
    ADD_COMP(COMP_BUFFER, "Buffer");

    // === DIGITAL ICS SECTION (index 69) ===
    NEW_SECTION();
    ADD_COMP(COMP_D_FLIPFLOP, "D-FF");
    ADD_COMP(COMP_JK_FLIPFLOP, "JK-FF");
    ADD_COMP(COMP_T_FLIPFLOP, "T-FF");
    ADD_COMP(COMP_SR_LATCH, "SR");
    ADD_COMP(COMP_555_TIMER, "555");
    ADD_COMP(COMP_COUNTER, "Cntr");
    ADD_COMP(COMP_SHIFT_REG, "ShReg");
    ADD_COMP(COMP_MUX_2TO1, "Mux");
    ADD_COMP(COMP_DECODER, "Decod");
    ADD_COMP(COMP_BCD_DECODER, "BCD");

    // === MIXED SIGNAL SECTION (index 79) ===
    NEW_SECTION();
    ADD_COMP(COMP_DAC, "DAC");
    ADD_COMP(COMP_ADC, "ADC");
    ADD_COMP(COMP_VCO, "VCO");
    ADD_COMP(COMP_PLL, "PLL");
    ADD_COMP(COMP_MONOSTABLE, "Mono");
    ADD_COMP(COMP_OPTOCOUPLER, "Opto");

    // === VOLTAGE REGULATORS SECTION (index 85) ===
    NEW_SECTION();
    ADD_COMP(COMP_LM317, "LM317");
    ADD_COMP(COMP_7805, "7805");
    ADD_COMP(COMP_TL431, "TL431");

    // === DISPLAY/OUTPUT SECTION (index 88) ===
    NEW_SECTION();
    ADD_COMP(COMP_7SEG_DISPLAY, "7Seg");
    ADD_COMP(COMP_LED_ARRAY, "LEDBar");
    ADD_COMP(COMP_DC_MOTOR, "Motor");
    ADD_COMP(COMP_SPEAKER, "Spkr");
    ADD_COMP(COMP_LAMP, "Lamp");

    // === MEASUREMENT SECTION (index 93) ===
    NEW_SECTION();
    ADD_COMP(COMP_VOLTMETER, "VMeter");
    ADD_COMP(COMP_AMMETER, "AMeter");
    ADD_COMP(COMP_WATTMETER, "WMeter");
    ADD_COMP(COMP_TEST_POINT, "TstPt");

    #undef ADD_TOOL
    #undef ADD_COMP
    #undef NEW_SECTION

    // === CIRCUITS SECTION ===
    pal_y += pal_h + 18;
    col = 0;
    ui->num_circuit_items = 0;
    ui->selected_circuit_type = -1;
    ui->placing_circuit = false;

    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_RC_LOWPASS, "RC LP", false, false
    };
    col++;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_RC_HIGHPASS, "RC HP", false, false
    };
    col = 0;
    pal_y += pal_h + 5;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_VOLTAGE_DIVIDER, "V Div", false, false
    };
    col++;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_HALFWAVE_RECT, "HW Rec", false, false
    };
    col = 0;
    pal_y += pal_h + 5;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_INVERTING_AMP, "Inv", false, false
    };
    col++;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_NONINVERTING_AMP, "NonI", false, false
    };
    col = 0;
    pal_y += pal_h + 5;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_VOLTAGE_FOLLOWER, "Follw", false, false
    };
    col++;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_LED_WITH_RESISTOR, "LED+R", false, false
    };
    // Transistor amplifiers
    col = 0;
    pal_y += pal_h + 5;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_COMMON_EMITTER, "CE", false, false
    };
    col++;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_COMMON_SOURCE, "CS", false, false
    };
    col = 0;
    pal_y += pal_h + 5;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_COMMON_DRAIN, "SF", false, false
    };
    col++;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_MULTISTAGE_AMP, "2Stg", false, false
    };
    // Additional transistor circuits
    col = 0;
    pal_y += pal_h + 5;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_DIFFERENTIAL_PAIR, "Diff", false, false
    };
    col++;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_CURRENT_MIRROR, "CMir", false, false
    };
    col = 0;
    pal_y += pal_h + 5;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_PUSH_PULL, "PP", false, false
    };
    col++;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_CMOS_INVERTER, "CMOS", false, false
    };
    // Additional op-amp circuits
    col = 0;
    pal_y += pal_h + 5;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_INTEGRATOR, "Intg", false, false
    };
    col++;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_DIFFERENTIATOR, "Difr", false, false
    };
    col = 0;
    pal_y += pal_h + 5;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_SUMMING_AMP, "Sum", false, false
    };
    col++;
    ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, CIRCUIT_COMPARATOR, "Cmp", false, false
    };

    // Calculate palette content height (from toolbar to last item + padding)
    ui->palette_content_height = pal_y + pal_h + 10 - TOOLBAR_HEIGHT;
    ui->palette_scroll_offset = 0;
    ui->palette_visible_height = WINDOW_HEIGHT - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT;
    ui->palette_scrolling = false;

    // Oscilloscope settings - larger default size for better visibility
    ui->scope_rect = (Rect){WINDOW_WIDTH - ui->properties_width + 10, 250, 330, 300};
    ui->scope_num_channels = 0;
    ui->scope_time_div = 0.001;   // 1ms per division
    ui->scope_volt_div = 1.0;     // 1V per division
    ui->scope_selected_channel = 0;
    ui->scope_paused = false;
    ui->scope_resizing = false;
    ui->scope_resize_edge = -1;

    // Initialize all channels with predefined colors
    for (int i = 0; i < MAX_PROBES; i++) {
        ui->scope_channels[i] = (ScopeChannel){false, PROBE_COLORS[i], i, 0.0};
    }

    // Oscilloscope control buttons - organized in rows for clarity
    // Row 1: Scale controls (V/div, T/div) + Autoset
    int scope_btn_y = ui->scope_rect.y + ui->scope_rect.h + 5;
    int scope_btn_w = 32, scope_btn_h = 22;
    int scope_btn_x = ui->scope_rect.x;
    int row_spacing = scope_btn_h + 4;

    ui->btn_scope_volt_up = (Button){{scope_btn_x, scope_btn_y, scope_btn_w, scope_btn_h}, "V+", "Increase V/div", false, false, true, false};
    scope_btn_x += scope_btn_w + 3;
    ui->btn_scope_volt_down = (Button){{scope_btn_x, scope_btn_y, scope_btn_w, scope_btn_h}, "V-", "Decrease V/div", false, false, true, false};
    scope_btn_x += scope_btn_w + 10;
    ui->btn_scope_time_up = (Button){{scope_btn_x, scope_btn_y, scope_btn_w, scope_btn_h}, "T+", "Increase time/div", false, false, true, false};
    scope_btn_x += scope_btn_w + 3;
    ui->btn_scope_time_down = (Button){{scope_btn_x, scope_btn_y, scope_btn_w, scope_btn_h}, "T-", "Decrease time/div", false, false, true, false};
    scope_btn_x += scope_btn_w + 10;
    ui->btn_scope_autoset = (Button){{scope_btn_x, scope_btn_y, 50, scope_btn_h}, "Autoset", "Auto-configure scope settings", false, false, true, false};

    // Row 2: Trigger controls
    scope_btn_y += row_spacing;
    scope_btn_x = ui->scope_rect.x;

    ui->btn_scope_trig_mode = (Button){{scope_btn_x, scope_btn_y, 45, scope_btn_h}, "AUTO", "Trigger mode (Auto/Normal/Single)", false, false, true, false};
    scope_btn_x += 48;
    ui->btn_scope_trig_edge = (Button){{scope_btn_x, scope_btn_y, 28, scope_btn_h}, "/\\", "Trigger edge (Rising/Falling/Both)", false, false, true, false};
    scope_btn_x += 31;
    ui->btn_scope_trig_ch = (Button){{scope_btn_x, scope_btn_y, 35, scope_btn_h}, "CH1", "Trigger channel", false, false, true, false};
    scope_btn_x += 38;
    ui->btn_scope_trig_up = (Button){{scope_btn_x, scope_btn_y, 24, scope_btn_h}, "L+", "Increase trigger level", false, false, true, false};
    scope_btn_x += 27;
    ui->btn_scope_trig_down = (Button){{scope_btn_x, scope_btn_y, 24, scope_btn_h}, "L-", "Decrease trigger level", false, false, true, false};

    // Row 3: Display modes and tools
    scope_btn_y += row_spacing;
    scope_btn_x = ui->scope_rect.x;

    ui->btn_scope_mode = (Button){{scope_btn_x, scope_btn_y, 35, scope_btn_h}, "Y-T", "Display mode (Y-T/X-Y)", false, false, true, false};
    scope_btn_x += 38;
    ui->btn_scope_cursor = (Button){{scope_btn_x, scope_btn_y, 35, scope_btn_h}, "CUR", "Toggle measurement cursors", false, false, true, false};
    scope_btn_x += 38;
    ui->btn_scope_fft = (Button){{scope_btn_x, scope_btn_y, 35, scope_btn_h}, "FFT", "Toggle FFT spectrum view", false, false, true, false};
    scope_btn_x += 38;
    ui->btn_scope_screenshot = (Button){{scope_btn_x, scope_btn_y, 35, scope_btn_h}, "CAP", "Capture screenshot (saves scope.bmp)", false, false, true, false};
    scope_btn_x += 38;
    ui->btn_bode = (Button){{scope_btn_x, scope_btn_y, 40, scope_btn_h}, "Bode", "Frequency response plot", false, false, true, false};
    scope_btn_x += 43;
    ui->btn_mc = (Button){{scope_btn_x, scope_btn_y, 25, scope_btn_h}, "MC", "Monte Carlo statistical analysis", false, false, true, false};
    scope_btn_x += 28;
    ui->btn_scope_popup = (Button){{scope_btn_x, scope_btn_y, 50, scope_btn_h}, "PopOut", "Pop out oscilloscope to separate window", false, false, true, false};

    // Initialize cursor state
    ui->scope_cursor_mode = false;
    ui->scope_cursor_drag = 0;
    ui->cursor1_time = 0.25;
    ui->cursor2_time = 0.75;
    ui->scope_fft_mode = false;

    // Initialize trigger settings
    ui->trigger_mode = TRIG_AUTO;
    ui->trigger_edge = TRIG_EDGE_RISING;
    ui->trigger_channel = 0;
    ui->trigger_level = 0.0;
    ui->trigger_armed = true;
    ui->triggered = false;
    ui->trigger_holdoff = 0.001;  // 1ms holdoff
    ui->dragging_trigger_level = false;

    // Initialize pop-out oscilloscope window
    ui->scope_popup_window = NULL;
    ui->scope_popup_renderer = NULL;
    ui->scope_popup_window_id = 0;
    ui->scope_popped_out = false;

    // Initialize trigger capture state
    ui->scope_capture_count = 0;
    ui->scope_capture_time = 0;
    ui->scope_capture_valid = false;
    ui->scope_last_trigger_time = 0;
    ui->scope_trigger_sample_idx = 0;

    // Initialize display mode
    ui->display_mode = SCOPE_MODE_YT;
    ui->xy_channel_x = 0;
    ui->xy_channel_y = 1;

    // Initialize Bode plot settings
    ui->show_bode_plot = false;
    ui->bode_rect = (Rect){PALETTE_WIDTH + 50, TOOLBAR_HEIGHT + 50, 400, 300};
    ui->bode_freq_start = 10.0;     // 10 Hz
    ui->bode_freq_stop = 100000.0;  // 100 kHz
    ui->bode_num_points = 50;
    ui->bode_resizing = false;
    ui->bode_resize_edge = -1;
    ui->bode_dragging = false;
    ui->bode_drag_start_x = 0;
    ui->bode_drag_start_y = 0;
    ui->bode_rect_start_x = 0;
    ui->bode_rect_start_y = 0;

    // Bode recalculate button (bounds updated in render function)
    ui->btn_bode_recalc = (Button){{0, 0, 70, 20}, "Recalc", "Recalculate frequency sweep", false, false, true, false};

    // Initialize parametric sweep panel
    ui->show_sweep_panel = false;
    ui->sweep_component_idx = -1;
    ui->sweep_param_type = 0;       // Value (resistance, capacitance, etc.)
    ui->sweep_start = 100.0;
    ui->sweep_end = 10000.0;
    ui->sweep_num_points = 20;
    ui->sweep_log_scale = true;

    // Initialize Monte Carlo panel
    ui->show_monte_carlo_panel = false;
    ui->monte_carlo_runs = 100;
    ui->monte_carlo_tolerance = 10.0;  // 10% tolerance

    strncpy(ui->status_message, "Ready", sizeof(ui->status_message));
}

void ui_update(UIState *ui, Circuit *circuit, Simulation *sim) {
    if (!ui) return;

    // Update button states based on simulation
    if (sim) {
        ui->btn_run.enabled = (sim->state != SIM_RUNNING);
        ui->btn_pause.enabled = (sim->state == SIM_RUNNING);
        ui->sim_time = sim->time;
        ui->display_time_step = sim->time_step;
        // Sync speed slider value to simulation speed
        sim->speed = (double)ui->speed_value;

        // Copy adaptive time-stepping status for UI display
        ui->adaptive_enabled = sim->adaptive_enabled;
        ui->adaptive_factor = sim->adaptive_factor;
        ui->step_rejections = sim->step_rejections;
        ui->error_estimate = sim->error_estimate;
    }

    if (circuit) {
        ui->node_count = circuit->num_nodes;
        ui->component_count = circuit->num_components;
    }
}

static void draw_button(SDL_Renderer *r, Button *btn) {
    // Background - synthwave colors
    if (btn->pressed) {
        SDL_SetRenderDrawColor(r, SYNTH_PINK, 0xff);
    } else if (btn->hovered && btn->enabled) {
        SDL_SetRenderDrawColor(r, SYNTH_BG_LIGHT, 0xff);
    } else {
        SDL_SetRenderDrawColor(r, SYNTH_BG_MID, 0xff);
    }
    SDL_Rect rect = {btn->bounds.x, btn->bounds.y, btn->bounds.w, btn->bounds.h};
    SDL_RenderFillRect(r, &rect);

    // Border - synthwave cyan/pink
    if (btn->enabled) {
        SDL_SetRenderDrawColor(r, SYNTH_CYAN, 0xff);
    } else {
        SDL_SetRenderDrawColor(r, SYNTH_TEXT_DARK, 0xff);
    }
    SDL_RenderDrawRect(r, &rect);

    // Label text (centered)
    if (btn->label) {
        int text_len = (int)strlen(btn->label);
        int text_x = btn->bounds.x + (btn->bounds.w - text_len * 8) / 2;
        int text_y = btn->bounds.y + (btn->bounds.h - 8) / 2;
        if (btn->enabled) {
            SDL_SetRenderDrawColor(r, SYNTH_TEXT, 0xff);
        } else {
            SDL_SetRenderDrawColor(r, SYNTH_TEXT_DARK, 0xff);
        }
        ui_draw_text(r, btn->label, text_x, text_y);
    }
}

static void draw_palette_item(SDL_Renderer *r, PaletteItem *item) {
    // Background - synthwave colors
    if (item->selected) {
        SDL_SetRenderDrawColor(r, SYNTH_PINK, 0x60);
    } else if (item->hovered) {
        SDL_SetRenderDrawColor(r, SYNTH_PURPLE, 0x40);
    } else {
        SDL_SetRenderDrawColor(r, SYNTH_BG_MID, 0xff);
    }
    SDL_Rect rect = {item->bounds.x, item->bounds.y, item->bounds.w, item->bounds.h};
    SDL_RenderFillRect(r, &rect);

    // Border - synthwave colors
    if (item->selected) {
        SDL_SetRenderDrawColor(r, SYNTH_PINK, 0xff);
    } else if (item->hovered) {
        SDL_SetRenderDrawColor(r, SYNTH_CYAN, 0xff);
    } else {
        SDL_SetRenderDrawColor(r, SYNTH_BORDER, 0xff);
    }
    SDL_RenderDrawRect(r, &rect);

    // Label text (centered)
    if (item->label) {
        int text_len = (int)strlen(item->label);
        int text_x = item->bounds.x + (item->bounds.w - text_len * 8) / 2;
        int text_y = item->bounds.y + (item->bounds.h - 8) / 2;
        if (item->selected) {
            SDL_SetRenderDrawColor(r, SYNTH_TEXT, 0xff);
        } else {
            SDL_SetRenderDrawColor(r, SYNTH_TEXT_DIM, 0xff);
        }
        ui_draw_text(r, item->label, text_x, text_y);
    }
}

static void draw_circuit_item(SDL_Renderer *r, CircuitPaletteItem *item) {
    // Background - synthwave green tint for circuit templates
    if (item->selected) {
        SDL_SetRenderDrawColor(r, SYNTH_GREEN, 0x40);
    } else if (item->hovered) {
        SDL_SetRenderDrawColor(r, SYNTH_GREEN, 0x20);
    } else {
        SDL_SetRenderDrawColor(r, SYNTH_BG_MID, 0xff);
    }
    SDL_Rect rect = {item->bounds.x, item->bounds.y, item->bounds.w, item->bounds.h};
    SDL_RenderFillRect(r, &rect);

    // Border
    if (item->selected) {
        SDL_SetRenderDrawColor(r, SYNTH_GREEN, 0xff);
    } else if (item->hovered) {
        SDL_SetRenderDrawColor(r, SYNTH_GREEN, 0xff);
    } else {
        SDL_SetRenderDrawColor(r, SYNTH_BORDER, 0xff);
    }
    SDL_RenderDrawRect(r, &rect);

    // Label text (centered)
    if (item->label) {
        int text_len = (int)strlen(item->label);
        int text_x = item->bounds.x + (item->bounds.w - text_len * 8) / 2;
        int text_y = item->bounds.y + (item->bounds.h - 8) / 2;
        if (item->selected) {
            SDL_SetRenderDrawColor(r, SYNTH_TEXT, 0xff);
        } else {
            SDL_SetRenderDrawColor(r, SYNTH_GREEN, 0xff);
        }
        ui_draw_text(r, item->label, text_x, text_y);
    }
}

void ui_render_toolbar(UIState *ui, SDL_Renderer *renderer) {
    // Toolbar background - synthwave dark purple
    SDL_SetRenderDrawColor(renderer, SYNTH_BG_MID, 0xff);
    SDL_Rect toolbar = {0, 0, ui->window_width, TOOLBAR_HEIGHT};
    SDL_RenderFillRect(renderer, &toolbar);

    // Title - hot pink
    SDL_SetRenderDrawColor(renderer, SYNTH_PINK, 0xff);
    ui_draw_text(renderer, "Circuit Playground", 10, 20);

    // Buttons
    draw_button(renderer, &ui->btn_run);
    draw_button(renderer, &ui->btn_pause);
    draw_button(renderer, &ui->btn_step);
    draw_button(renderer, &ui->btn_reset);
    draw_button(renderer, &ui->btn_clear);
    draw_button(renderer, &ui->btn_save);
    draw_button(renderer, &ui->btn_load);
    draw_button(renderer, &ui->btn_export_svg);

    // Speed slider label
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT, 0xff);
    ui_draw_text(renderer, "Speed:", ui->speed_slider.x, ui->speed_slider.y - 2);

    // Speed slider background
    int slider_x = ui->speed_slider.x + 50;
    SDL_SetRenderDrawColor(renderer, SYNTH_BG_DARK, 0xff);
    SDL_Rect slider_bg = {slider_x, ui->speed_slider.y, ui->speed_slider.w, ui->speed_slider.h};
    SDL_RenderFillRect(renderer, &slider_bg);
    SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0x80);
    SDL_RenderDrawRect(renderer, &slider_bg);

    // Speed slider fill (logarithmic scale: 1x to 100x)
    // Map speed_value (1-100) to slider position
    float log_pos = (log10f(ui->speed_value) / 2.0f);  // log10(100) = 2
    int fill_w = (int)(ui->speed_slider.w * log_pos);
    fill_w = CLAMP(fill_w, 0, ui->speed_slider.w);
    SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0xff);
    SDL_Rect slider_fill = {slider_x, ui->speed_slider.y, fill_w, ui->speed_slider.h};
    SDL_RenderFillRect(renderer, &slider_fill);

    // Speed value text
    char speed_text[16];
    if (ui->speed_value >= 10.0f) {
        snprintf(speed_text, sizeof(speed_text), "%.0fx", ui->speed_value);
    } else {
        snprintf(speed_text, sizeof(speed_text), "%.1fx", ui->speed_value);
    }
    SDL_SetRenderDrawColor(renderer, SYNTH_GREEN, 0xff);
    ui_draw_text(renderer, speed_text, slider_x + ui->speed_slider.w + 5, ui->speed_slider.y - 2);

    // Time step label and value
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT, 0xff);
    ui_draw_text(renderer, "dt:", ui->timestep_display_x, 17);

    // Format time step with appropriate units
    char dt_text[24];
    double dt = ui->display_time_step;
    if (dt >= 1e-3) {
        snprintf(dt_text, sizeof(dt_text), "%.1fms", dt * 1e3);
    } else if (dt >= 1e-6) {
        snprintf(dt_text, sizeof(dt_text), "%.1fus", dt * 1e6);
    } else {
        snprintf(dt_text, sizeof(dt_text), "%.0fns", dt * 1e9);
    }
    SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0xff);
    ui_draw_text(renderer, dt_text, ui->timestep_display_x + 24, 17);

    // Time step buttons
    draw_button(renderer, &ui->btn_timestep_down);
    draw_button(renderer, &ui->btn_timestep_up);
    draw_button(renderer, &ui->btn_timestep_auto);

    // Toolbar border
    SDL_SetRenderDrawColor(renderer, SYNTH_BORDER, 0xff);
    SDL_RenderDrawLine(renderer, 0, TOOLBAR_HEIGHT - 1, ui->window_width, TOOLBAR_HEIGHT - 1);
}

void ui_render_palette(UIState *ui, SDL_Renderer *renderer) {
    // Palette background - synthwave dark
    SDL_SetRenderDrawColor(renderer, SYNTH_BG_DARK, 0xff);
    SDL_Rect palette = {0, TOOLBAR_HEIGHT, PALETTE_WIDTH, ui->window_height - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT};
    SDL_RenderFillRect(renderer, &palette);

    // Set clipping rect for palette content (excluding scrollbar area)
    SDL_Rect clip = {0, TOOLBAR_HEIGHT, PALETTE_WIDTH - 10, ui->window_height - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT};
    SDL_RenderSetClipRect(renderer, &clip);

    int scroll_offset = ui->palette_scroll_offset;

    // Section mapping: maps start index to category ID
    typedef struct { int start_idx; int end_idx; PaletteCategoryID cat_id; } SectionMapping;
    SectionMapping sections[] = {
        {0, 4, PCAT_TOOLS},
        {5, 10, PCAT_SOURCES},
        {11, 16, PCAT_WAVEFORMS},
        {17, 24, PCAT_PASSIVES},
        {25, 30, PCAT_DIODES},
        {31, 34, PCAT_BJT},
        {35, 38, PCAT_FET},
        {39, 42, PCAT_THYRISTORS},
        {43, 46, PCAT_OPAMPS},
        {47, 50, PCAT_CONTROLLED},
        {51, 56, PCAT_SWITCHES},
        {57, 58, PCAT_TRANSFORMERS},
        {59, 68, PCAT_LOGIC},
        {69, 78, PCAT_DIGITAL},
        {79, 84, PCAT_MIXED},
        {85, 87, PCAT_REGULATORS},
        {88, 92, PCAT_DISPLAY},
        {93, 96, PCAT_MEASUREMENT}
    };
    int num_sections = sizeof(sections) / sizeof(sections[0]);

    // Calculate dynamic positions and draw
    int pal_h = 35;  // Item height
    int draw_y = TOOLBAR_HEIGHT + 4;  // Starting y position (content coords, not screen)
    int content_height = 4;  // Track total content height

    for (int s = 0; s < num_sections; s++) {
        if (sections[s].start_idx >= ui->num_palette_items) continue;

        PaletteCategoryID cat_id = sections[s].cat_id;
        PaletteCategory *cat = &ui->categories[cat_id];
        bool collapsed = cat->collapsed;

        // Store header y position for click detection (in content coords)
        cat->header_y = draw_y;

        // Draw header
        int header_screen_y = draw_y - scroll_offset;
        if (header_screen_y >= TOOLBAR_HEIGHT - 14 && header_screen_y < ui->window_height - STATUSBAR_HEIGHT) {
            // Draw collapse indicator
            SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0xff);
            if (collapsed) {
                // Draw right-pointing triangle (collapsed)
                int tx = 3, ty = header_screen_y + 2;
                SDL_RenderDrawLine(renderer, tx, ty, tx, ty + 6);
                SDL_RenderDrawLine(renderer, tx, ty, tx + 4, ty + 3);
                SDL_RenderDrawLine(renderer, tx, ty + 6, tx + 4, ty + 3);
            } else {
                // Draw down-pointing triangle (expanded)
                int tx = 2, ty = header_screen_y + 1;
                SDL_RenderDrawLine(renderer, tx, ty, tx + 6, ty);
                SDL_RenderDrawLine(renderer, tx, ty, tx + 3, ty + 4);
                SDL_RenderDrawLine(renderer, tx + 6, ty, tx + 3, ty + 4);
            }

            // Draw header text
            SDL_SetRenderDrawColor(renderer, SYNTH_PINK, 0xff);
            ui_draw_text(renderer, cat->name, 12, header_screen_y);
        }
        draw_y += 14;  // Header height
        content_height += 14;

        if (!collapsed) {
            // Draw items in this section
            int col = 0;
            for (int i = sections[s].start_idx; i <= sections[s].end_idx && i < ui->num_palette_items; i++) {
                PaletteItem *item = &ui->palette_items[i];

                // Update item bounds to dynamic position
                item->bounds.x = 10 + col * 70;
                item->bounds.y = draw_y;

                int screen_y = draw_y - scroll_offset;
                // Draw if visible
                if (screen_y + item->bounds.h >= TOOLBAR_HEIGHT && screen_y < ui->window_height - STATUSBAR_HEIGHT) {
                    int orig_y = item->bounds.y;
                    item->bounds.y = screen_y;
                    draw_palette_item(renderer, item);
                    item->bounds.y = orig_y;
                }

                col++;
                if (col >= 2) {
                    col = 0;
                    draw_y += pal_h + 3;
                    content_height += pal_h + 3;
                }
            }
            // Move to next row if we ended mid-row
            if (col > 0) {
                draw_y += pal_h + 3;
                content_height += pal_h + 3;
            }
            draw_y += 12;  // Section spacing
            content_height += 12;
        } else {
            draw_y += 4;  // Small spacing when collapsed
            content_height += 4;
        }
    }

    // Circuits section
    PaletteCategory *circuits_cat = &ui->categories[PCAT_CIRCUITS];
    if (ui->num_circuit_items > 0) {
        circuits_cat->header_y = draw_y;

        int header_screen_y = draw_y - scroll_offset;
        if (header_screen_y >= TOOLBAR_HEIGHT - 14 && header_screen_y < ui->window_height - STATUSBAR_HEIGHT) {
            // Draw collapse indicator
            SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0xff);
            if (circuits_cat->collapsed) {
                int tx = 3, ty = header_screen_y + 2;
                SDL_RenderDrawLine(renderer, tx, ty, tx, ty + 6);
                SDL_RenderDrawLine(renderer, tx, ty, tx + 4, ty + 3);
                SDL_RenderDrawLine(renderer, tx, ty + 6, tx + 4, ty + 3);
            } else {
                int tx = 2, ty = header_screen_y + 1;
                SDL_RenderDrawLine(renderer, tx, ty, tx + 6, ty);
                SDL_RenderDrawLine(renderer, tx, ty, tx + 3, ty + 4);
                SDL_RenderDrawLine(renderer, tx + 6, ty, tx + 3, ty + 4);
            }

            SDL_SetRenderDrawColor(renderer, SYNTH_PINK, 0xff);
            ui_draw_text(renderer, "Circuits", 12, header_screen_y);
        }
        draw_y += 14;
        content_height += 14;

        if (!circuits_cat->collapsed) {
            int col = 0;
            for (int i = 0; i < ui->num_circuit_items; i++) {
                CircuitPaletteItem *item = &ui->circuit_items[i];

                item->bounds.x = 10 + col * 70;
                item->bounds.y = draw_y;

                int screen_y = draw_y - scroll_offset;
                if (screen_y + item->bounds.h >= TOOLBAR_HEIGHT && screen_y < ui->window_height - STATUSBAR_HEIGHT) {
                    int orig_y = item->bounds.y;
                    item->bounds.y = screen_y;
                    draw_circuit_item(renderer, item);
                    item->bounds.y = orig_y;
                }

                col++;
                if (col >= 2) {
                    col = 0;
                    draw_y += pal_h + 5;
                    content_height += pal_h + 5;
                }
            }
            if (col > 0) {
                draw_y += pal_h + 5;
                content_height += pal_h + 5;
            }
        }
    }

    // Update content height for scrollbar
    ui->palette_content_height = content_height + 10;

    // Reset clipping
    SDL_RenderSetClipRect(renderer, NULL);

    // Draw scrollbar if content exceeds visible area
    if (ui->palette_content_height > ui->palette_visible_height) {
        int scrollbar_x = PALETTE_WIDTH - 8;
        int scrollbar_track_y = TOOLBAR_HEIGHT + 2;
        int scrollbar_track_h = ui->palette_visible_height - 4;

        // Draw track (darker background) - synthwave dark
        SDL_SetRenderDrawColor(renderer, SYNTH_BG_DARK, 0xff);
        SDL_Rect track = {scrollbar_x, scrollbar_track_y, 6, scrollbar_track_h};
        SDL_RenderFillRect(renderer, &track);

        // Calculate thumb position and size
        float visible_ratio = (float)ui->palette_visible_height / ui->palette_content_height;
        int thumb_h = (int)(scrollbar_track_h * visible_ratio);
        if (thumb_h < 20) thumb_h = 20;  // Minimum thumb size

        int max_scroll = ui->palette_content_height - ui->palette_visible_height;
        float scroll_ratio = (max_scroll > 0) ? (float)ui->palette_scroll_offset / max_scroll : 0;
        int thumb_y = scrollbar_track_y + (int)((scrollbar_track_h - thumb_h) * scroll_ratio);

        // Draw thumb - synthwave purple
        SDL_SetRenderDrawColor(renderer, SYNTH_PURPLE, 0xff);
        SDL_Rect thumb = {scrollbar_x, thumb_y, 6, thumb_h};
        SDL_RenderFillRect(renderer, &thumb);
    }

    // Border - synthwave border
    SDL_SetRenderDrawColor(renderer, SYNTH_BORDER, 0xff);
    SDL_RenderDrawLine(renderer, PALETTE_WIDTH - 1, TOOLBAR_HEIGHT, PALETTE_WIDTH - 1, ui->window_height - STATUSBAR_HEIGHT);
}

// Helper to draw an editable property field
static void draw_property_field(SDL_Renderer *renderer, int x, int y, int w,
                                const char *label, const char *value,
                                bool is_editing, const char *edit_buffer, int cursor_pos) {
    // Safety checks for NULL strings
    if (!value) value = "";
    if (!edit_buffer) edit_buffer = "";
    if (!label) label = "";

    // Label - synthwave text
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT, 0xff);
    ui_draw_text(renderer, label, x, y + 2);

    // Value box
    int value_x = x + 90;
    int box_w = w - 90;
    SDL_Rect box = {value_x, y, box_w, 14};

    if (is_editing) {
        // Editing - dark background with pink border
        SDL_SetRenderDrawColor(renderer, SYNTH_BG_DARK, 0xff);
        SDL_RenderFillRect(renderer, &box);
        SDL_SetRenderDrawColor(renderer, SYNTH_PINK, 0xff);
        SDL_RenderDrawRect(renderer, &box);

        // Draw input text
        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT, 0xff);
        ui_draw_text(renderer, edit_buffer, value_x + 2, y + 3);

        // Draw cursor
        int cursor_x = value_x + 2 + cursor_pos * 8;
        SDL_SetRenderDrawColor(renderer, SYNTH_PINK, 0xff);
        SDL_RenderDrawLine(renderer, cursor_x, y + 2, cursor_x, y + 12);
    } else {
        // Not editing - clickable field
        SDL_SetRenderDrawColor(renderer, SYNTH_BG_MID, 0xff);
        SDL_RenderFillRect(renderer, &box);
        SDL_SetRenderDrawColor(renderer, SYNTH_BORDER, 0xff);
        SDL_RenderDrawRect(renderer, &box);

        // Draw value - synthwave green
        SDL_SetRenderDrawColor(renderer, SYNTH_GREEN, 0xff);
        ui_draw_text(renderer, value, value_x + 2, y + 3);
    }
}

// Helper to draw sweep configuration section
// Returns the new prop_y position after drawing
static int draw_sweep_config(SDL_Renderer *renderer, UIState *ui, int x, int prop_y, int prop_w,
                             const char *label, SweepConfig *sweep,
                             int enable_prop, int mode_prop, int start_prop, int end_prop,
                             int time_prop, int steps_prop, int repeat_prop,
                             struct InputState *input, const char *unit) {
    char buf[64];

    // Sweep enable toggle - synthwave orange
    SDL_SetRenderDrawColor(renderer, SYNTH_ORANGE, 0xff);
    ui_draw_text(renderer, label, x + 10, prop_y + 2);
    if (sweep->enabled) {
        SDL_SetRenderDrawColor(renderer, SYNTH_GREEN, 0xff);
    } else {
        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DARK, 0xff);
    }
    ui_draw_text(renderer, sweep->enabled ? "[ON]" : "[OFF]", x + 100, prop_y + 2);
    ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, 40, 14};
    ui->properties[ui->num_properties].prop_type = enable_prop;
    ui->num_properties++;
    prop_y += 16;

    if (sweep->enabled) {
        // Mode selection
        const char *mode_names[] = {"Linear", "Log", "Step"};
        int mode_idx = (sweep->mode >= SWEEP_LINEAR && sweep->mode <= SWEEP_STEP) ? sweep->mode - 1 : 0;
        if (mode_idx < 0) mode_idx = 0;
        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DIM, 0xff);
        ui_draw_text(renderer, "  Mode:", x + 10, prop_y + 2);
        SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0xff);
        snprintf(buf, sizeof(buf), "[%s]", mode_names[mode_idx]);
        ui_draw_text(renderer, buf, x + 100, prop_y + 2);
        ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, 60, 14};
        ui->properties[ui->num_properties].prop_type = mode_prop;
        ui->num_properties++;
        prop_y += 16;

        // Start value
        bool edit_start = input && input->editing_property && input->editing_prop_type == start_prop;
        snprintf(buf, sizeof(buf), "%.3g %s", sweep->start_value, unit);
        draw_property_field(renderer, x + 10, prop_y, prop_w, "  Start:", buf,
                           edit_start, input ? input->input_buffer : "", input ? input->input_cursor : 0);
        ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
        ui->properties[ui->num_properties].prop_type = start_prop;
        ui->num_properties++;
        prop_y += 16;

        // End value
        bool edit_end = input && input->editing_property && input->editing_prop_type == end_prop;
        snprintf(buf, sizeof(buf), "%.3g %s", sweep->end_value, unit);
        draw_property_field(renderer, x + 10, prop_y, prop_w, "  End:", buf,
                           edit_end, input ? input->input_buffer : "", input ? input->input_cursor : 0);
        ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
        ui->properties[ui->num_properties].prop_type = end_prop;
        ui->num_properties++;
        prop_y += 16;

        // Sweep time
        bool edit_time = input && input->editing_property && input->editing_prop_type == time_prop;
        snprintf(buf, sizeof(buf), "%.3g s", sweep->sweep_time);
        draw_property_field(renderer, x + 10, prop_y, prop_w, "  Time:", buf,
                           edit_time, input ? input->input_buffer : "", input ? input->input_cursor : 0);
        ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
        ui->properties[ui->num_properties].prop_type = time_prop;
        ui->num_properties++;
        prop_y += 16;

        // Steps (only for step mode)
        if (sweep->mode == SWEEP_STEP) {
            bool edit_steps = input && input->editing_property && input->editing_prop_type == steps_prop;
            snprintf(buf, sizeof(buf), "%d", sweep->num_steps);
            draw_property_field(renderer, x + 10, prop_y, prop_w, "  Steps:", buf,
                               edit_steps, input ? input->input_buffer : "", input ? input->input_cursor : 0);
            ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
            ui->properties[ui->num_properties].prop_type = steps_prop;
            ui->num_properties++;
            prop_y += 16;
        }

        // Repeat toggle
        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DIM, 0xff);
        ui_draw_text(renderer, "  Repeat:", x + 10, prop_y + 2);
        if (sweep->repeat) {
            SDL_SetRenderDrawColor(renderer, SYNTH_GREEN, 0xff);
        } else {
            SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DARK, 0xff);
        }
        ui_draw_text(renderer, sweep->repeat ? "[Yes]" : "[No]", x + 100, prop_y + 2);
        ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, 40, 14};
        ui->properties[ui->num_properties].prop_type = repeat_prop;
        ui->num_properties++;
        prop_y += 16;
    }

    return prop_y;
}

void ui_render_properties(UIState *ui, SDL_Renderer *renderer, Component *selected, struct InputState *input) {
    int x = ui->window_width - ui->properties_width;
    int y = TOOLBAR_HEIGHT;

    // Calculate panel height based on scope position (fill space between toolbar and scope)
    // The scope label is drawn 18px above scope_rect, so leave 50px gap to avoid overlap
    int available_height = ui->scope_rect.y - y - 50;
    int panel_height = available_height > 100 ? available_height : 100;  // Minimum 100 but don't exceed available

    // Store visible height for scrollbar calculations
    ui->properties_visible_height = panel_height;

    // Draw resize handle on left edge - synthwave purple
    SDL_SetRenderDrawColor(renderer, SYNTH_PURPLE_DIM, 0xff);
    SDL_Rect resize_handle = {x - 3, y, 6, panel_height};
    SDL_RenderFillRect(renderer, &resize_handle);

    // Background - synthwave dark
    SDL_SetRenderDrawColor(renderer, SYNTH_BG_DARK, 0xff);
    SDL_Rect panel = {x, y, ui->properties_width, panel_height};
    SDL_RenderFillRect(renderer, &panel);

    // Set clipping rect to prevent content from overflowing into scope area
    // Leave room for scrollbar on right side
    SDL_Rect clip_rect = {x, y, ui->properties_width - 10, panel_height};
    SDL_RenderSetClipRect(renderer, &clip_rect);

    // Apply scroll offset to content
    int scroll_y = ui->properties_scroll_offset;
    int content_y = y - scroll_y;

    // Title - synthwave pink
    SDL_SetRenderDrawColor(renderer, SYNTH_PINK, 0xff);
    ui_draw_text(renderer, "Properties", x + 10, content_y + 10);

    // Get editing state from input
    bool editing_value = input && input->editing_property && input->editing_prop_type == 1;  // PROP_VALUE
    bool editing_freq = input && input->editing_property && input->editing_prop_type == 2;   // PROP_FREQUENCY
    bool editing_phase = input && input->editing_property && input->editing_prop_type == 3;  // PROP_PHASE
    bool editing_offset = input && input->editing_property && input->editing_prop_type == 4; // PROP_OFFSET
    bool editing_duty = input && input->editing_property && input->editing_prop_type == 5;   // PROP_DUTY
    const char *edit_buf = input ? input->input_buffer : "";
    int cursor = input ? input->input_cursor : 0;

    // Show selected component info
    if (selected) {
        SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
        ui_draw_text(renderer, "Component:", x + 10, content_y + 35);

        // Get component type name safely from component info
        const ComponentTypeInfo *info = component_get_info(selected->type);
        if (info && info->name) {
            SDL_SetRenderDrawColor(renderer, 0xc0, 0xc0, 0xc0, 0xff);
            ui_draw_text(renderer, info->name, x + 100, content_y + 35);
        }

        // Store property bounds for later reference
        ui->num_properties = 0;

        // Show component properties with clickable fields
        int prop_y = content_y + 55;
        int prop_w = ui->properties_width - 20;
        char buf[64];

        switch (selected->type) {
            case COMP_DC_VOLTAGE: {
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.dc_voltage.voltage);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Voltage:", buf,
                                   editing_value, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_VALUE;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.dc_voltage.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Non-ideal parameters (internal resistance)
                if (!selected->props.dc_voltage.ideal) {
                    bool edit_rs = input && input->editing_property && input->editing_prop_type == PROP_R_SERIES;
                    format_engineering(selected->props.dc_voltage.r_series, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "R_series:", buf, edit_rs, edit_buf, cursor);
                    ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    ui->properties[ui->num_properties].prop_type = PROP_R_SERIES;
                    ui->num_properties++;
                    prop_y += 18;
                }

                // Voltage sweep configuration
                prop_y += 4;  // Add spacing before sweep section
                prop_y = draw_sweep_config(renderer, ui, x, prop_y, prop_w,
                    "V Sweep:", &selected->props.dc_voltage.voltage_sweep,
                    PROP_SWEEP_VOLTAGE_ENABLE, PROP_SWEEP_VOLTAGE_MODE,
                    PROP_SWEEP_VOLTAGE_START, PROP_SWEEP_VOLTAGE_END,
                    PROP_SWEEP_VOLTAGE_TIME, PROP_SWEEP_VOLTAGE_STEPS,
                    PROP_SWEEP_VOLTAGE_REPEAT, input, "V");
                break;
            }

            case COMP_AC_VOLTAGE: {
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.ac_voltage.amplitude);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Amplitude:", buf,
                                   editing_value, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_VALUE;
                ui->num_properties++;

                prop_y += 18;
                snprintf(buf, sizeof(buf), "%.3g Hz", selected->props.ac_voltage.frequency);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Frequency:", buf,
                                   editing_freq, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_FREQUENCY;
                ui->num_properties++;

                prop_y += 18;
                snprintf(buf, sizeof(buf), "%.1f deg", selected->props.ac_voltage.phase);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Phase:", buf,
                                   editing_phase, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_PHASE;
                ui->num_properties++;

                prop_y += 18;
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.ac_voltage.offset);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Offset:", buf,
                                   editing_offset, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_OFFSET;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.ac_voltage.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Non-ideal parameters (internal resistance)
                if (!selected->props.ac_voltage.ideal) {
                    bool edit_rs = input && input->editing_property && input->editing_prop_type == PROP_R_SERIES;
                    format_engineering(selected->props.ac_voltage.r_series, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "R_series:", buf, edit_rs, edit_buf, cursor);
                    ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    ui->properties[ui->num_properties].prop_type = PROP_R_SERIES;
                    ui->num_properties++;
                    prop_y += 18;
                }

                // Amplitude sweep configuration
                prop_y += 4;
                prop_y = draw_sweep_config(renderer, ui, x, prop_y, prop_w,
                    "Amp Sweep:", &selected->props.ac_voltage.amplitude_sweep,
                    PROP_SWEEP_AMP_ENABLE, PROP_SWEEP_AMP_MODE,
                    PROP_SWEEP_AMP_START, PROP_SWEEP_AMP_END,
                    PROP_SWEEP_AMP_TIME, PROP_SWEEP_AMP_STEPS,
                    PROP_SWEEP_AMP_REPEAT, input, "V");

                // Frequency sweep configuration
                prop_y += 4;
                prop_y = draw_sweep_config(renderer, ui, x, prop_y, prop_w,
                    "Freq Sweep:", &selected->props.ac_voltage.frequency_sweep,
                    PROP_SWEEP_FREQ_ENABLE, PROP_SWEEP_FREQ_MODE,
                    PROP_SWEEP_FREQ_START, PROP_SWEEP_FREQ_END,
                    PROP_SWEEP_FREQ_TIME, PROP_SWEEP_FREQ_STEPS,
                    PROP_SWEEP_FREQ_REPEAT, input, "Hz");
                break;
            }

            case COMP_DC_CURRENT: {
                snprintf(buf, sizeof(buf), "%.3g A", selected->props.dc_current.current);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Current:", buf,
                                   editing_value, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_VALUE;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.dc_current.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Non-ideal parameters (internal parallel resistance)
                if (!selected->props.dc_current.ideal) {
                    bool edit_rp = input && input->editing_property && input->editing_prop_type == PROP_R_PARALLEL;
                    format_engineering(selected->props.dc_current.r_parallel, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "R_parallel:", buf, edit_rp, edit_buf, cursor);
                    ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    ui->properties[ui->num_properties].prop_type = PROP_R_PARALLEL;
                    ui->num_properties++;
                    prop_y += 18;
                }

                // Current sweep configuration (reuse voltage sweep props)
                prop_y += 4;
                prop_y = draw_sweep_config(renderer, ui, x, prop_y, prop_w,
                    "I Sweep:", &selected->props.dc_current.current_sweep,
                    PROP_SWEEP_VOLTAGE_ENABLE, PROP_SWEEP_VOLTAGE_MODE,
                    PROP_SWEEP_VOLTAGE_START, PROP_SWEEP_VOLTAGE_END,
                    PROP_SWEEP_VOLTAGE_TIME, PROP_SWEEP_VOLTAGE_STEPS,
                    PROP_SWEEP_VOLTAGE_REPEAT, input, "A");
                break;
            }

            case COMP_RESISTOR: {
                snprintf(buf, sizeof(buf), "%.3g Ohm", selected->props.resistor.resistance);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Resistance:", buf,
                                   editing_value, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_VALUE;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.resistor.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Non-ideal parameters (temperature effects)
                if (!selected->props.resistor.ideal) {
                    bool edit_tc = input && input->editing_property && input->editing_prop_type == PROP_TEMP_COEFF;
                    snprintf(buf, sizeof(buf), "%.0f ppm/C", selected->props.resistor.temp_coeff);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Temp Coef:", buf, edit_tc, edit_buf, cursor);
                    ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    ui->properties[ui->num_properties].prop_type = PROP_TEMP_COEFF;
                    ui->num_properties++;
                    prop_y += 18;
                }

                // Tolerance
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Tolerance:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                snprintf(buf, sizeof(buf), "%.1f%%", selected->props.resistor.tolerance);
                ui_draw_text(renderer, buf, x + 100, prop_y + 2);
                prop_y += 18;

                // Power dissipated
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Power:", x + 10, prop_y + 2);
                double pwr_ratio = selected->props.resistor.power_dissipated / selected->props.resistor.power_rating;
                if (pwr_ratio > 1.0) SDL_SetRenderDrawColor(renderer, 0xff, 0x40, 0x40, 0xff);
                else if (pwr_ratio > 0.8) SDL_SetRenderDrawColor(renderer, 0xff, 0xaa, 0x00, 0xff);
                else SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);
                snprintf(buf, sizeof(buf), "%.2fW/%.2fW", selected->props.resistor.power_dissipated, selected->props.resistor.power_rating);
                ui_draw_text(renderer, buf, x + 100, prop_y + 2);
                break;
            }

            case COMP_CAPACITOR: {
                snprintf(buf, sizeof(buf), "%.3g F", selected->props.capacitor.capacitance);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Capacitance:", buf,
                                   editing_value, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_VALUE;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.capacitor.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Non-ideal parameters (ESR, leakage)
                if (!selected->props.capacitor.ideal) {
                    bool edit_esr = input && input->editing_property && input->editing_prop_type == PROP_ESR;
                    format_engineering(selected->props.capacitor.esr, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "ESR:", buf, edit_esr, edit_buf, cursor);
                    ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    ui->properties[ui->num_properties].prop_type = PROP_ESR;
                    ui->num_properties++;
                    prop_y += 18;

                    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                    format_engineering(selected->props.capacitor.leakage, "Ohm", buf, sizeof(buf));
                    ui_draw_text(renderer, "Leakage:", x + 10, prop_y + 2);
                    ui_draw_text(renderer, buf, x + 100, prop_y + 2);
                }
                break;
            }

            case COMP_INDUCTOR: {
                snprintf(buf, sizeof(buf), "%.3g H", selected->props.inductor.inductance);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Inductance:", buf,
                                   editing_value, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_VALUE;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.inductor.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Non-ideal parameters (DCR)
                if (!selected->props.inductor.ideal) {
                    bool edit_dcr = input && input->editing_property && input->editing_prop_type == PROP_DCR;
                    format_engineering(selected->props.inductor.dcr, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "DCR:", buf, edit_dcr, edit_buf, cursor);
                    ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    ui->properties[ui->num_properties].prop_type = PROP_DCR;
                    ui->num_properties++;
                    prop_y += 18;

                    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                    snprintf(buf, sizeof(buf), "%.2f A", selected->props.inductor.i_sat);
                    ui_draw_text(renderer, "I_sat:", x + 10, prop_y + 2);
                    ui_draw_text(renderer, buf, x + 100, prop_y + 2);
                }
                break;
            }

            case COMP_SQUARE_WAVE:
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.square_wave.amplitude);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Amplitude:", buf,
                                   editing_value, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_VALUE;
                ui->num_properties++;

                prop_y += 18;
                snprintf(buf, sizeof(buf), "%.3g Hz", selected->props.square_wave.frequency);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Frequency:", buf,
                                   editing_freq, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_FREQUENCY;
                ui->num_properties++;

                prop_y += 18;
                snprintf(buf, sizeof(buf), "%.0f %%", selected->props.square_wave.duty * 100);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Duty:", buf,
                                   editing_duty, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_DUTY;
                ui->num_properties++;

                // Amplitude sweep
                prop_y += 22;
                prop_y = draw_sweep_config(renderer, ui, x, prop_y, prop_w,
                    "Amp Sweep:", &selected->props.square_wave.amplitude_sweep,
                    PROP_SWEEP_AMP_ENABLE, PROP_SWEEP_AMP_MODE,
                    PROP_SWEEP_AMP_START, PROP_SWEEP_AMP_END,
                    PROP_SWEEP_AMP_TIME, PROP_SWEEP_AMP_STEPS,
                    PROP_SWEEP_AMP_REPEAT, input, "V");

                // Frequency sweep
                prop_y += 4;
                prop_y = draw_sweep_config(renderer, ui, x, prop_y, prop_w,
                    "Freq Sweep:", &selected->props.square_wave.frequency_sweep,
                    PROP_SWEEP_FREQ_ENABLE, PROP_SWEEP_FREQ_MODE,
                    PROP_SWEEP_FREQ_START, PROP_SWEEP_FREQ_END,
                    PROP_SWEEP_FREQ_TIME, PROP_SWEEP_FREQ_STEPS,
                    PROP_SWEEP_FREQ_REPEAT, input, "Hz");
                break;

            case COMP_TRIANGLE_WAVE:
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.triangle_wave.amplitude);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Amplitude:", buf,
                                   editing_value, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_VALUE;
                ui->num_properties++;

                prop_y += 18;
                snprintf(buf, sizeof(buf), "%.3g Hz", selected->props.triangle_wave.frequency);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Frequency:", buf,
                                   editing_freq, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_FREQUENCY;
                ui->num_properties++;

                // Amplitude sweep
                prop_y += 22;
                prop_y = draw_sweep_config(renderer, ui, x, prop_y, prop_w,
                    "Amp Sweep:", &selected->props.triangle_wave.amplitude_sweep,
                    PROP_SWEEP_AMP_ENABLE, PROP_SWEEP_AMP_MODE,
                    PROP_SWEEP_AMP_START, PROP_SWEEP_AMP_END,
                    PROP_SWEEP_AMP_TIME, PROP_SWEEP_AMP_STEPS,
                    PROP_SWEEP_AMP_REPEAT, input, "V");

                // Frequency sweep
                prop_y += 4;
                prop_y = draw_sweep_config(renderer, ui, x, prop_y, prop_w,
                    "Freq Sweep:", &selected->props.triangle_wave.frequency_sweep,
                    PROP_SWEEP_FREQ_ENABLE, PROP_SWEEP_FREQ_MODE,
                    PROP_SWEEP_FREQ_START, PROP_SWEEP_FREQ_END,
                    PROP_SWEEP_FREQ_TIME, PROP_SWEEP_FREQ_STEPS,
                    PROP_SWEEP_FREQ_REPEAT, input, "Hz");
                break;

            case COMP_SAWTOOTH_WAVE:
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.sawtooth_wave.amplitude);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Amplitude:", buf,
                                   editing_value, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_VALUE;
                ui->num_properties++;

                prop_y += 18;
                snprintf(buf, sizeof(buf), "%.3g Hz", selected->props.sawtooth_wave.frequency);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Frequency:", buf,
                                   editing_freq, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_FREQUENCY;
                ui->num_properties++;

                // Amplitude sweep
                prop_y += 22;
                prop_y = draw_sweep_config(renderer, ui, x, prop_y, prop_w,
                    "Amp Sweep:", &selected->props.sawtooth_wave.amplitude_sweep,
                    PROP_SWEEP_AMP_ENABLE, PROP_SWEEP_AMP_MODE,
                    PROP_SWEEP_AMP_START, PROP_SWEEP_AMP_END,
                    PROP_SWEEP_AMP_TIME, PROP_SWEEP_AMP_STEPS,
                    PROP_SWEEP_AMP_REPEAT, input, "V");

                // Frequency sweep
                prop_y += 4;
                prop_y = draw_sweep_config(renderer, ui, x, prop_y, prop_w,
                    "Freq Sweep:", &selected->props.sawtooth_wave.frequency_sweep,
                    PROP_SWEEP_FREQ_ENABLE, PROP_SWEEP_FREQ_MODE,
                    PROP_SWEEP_FREQ_START, PROP_SWEEP_FREQ_END,
                    PROP_SWEEP_FREQ_TIME, PROP_SWEEP_FREQ_STEPS,
                    PROP_SWEEP_FREQ_REPEAT, input, "Hz");
                break;

            case COMP_NOISE_SOURCE:
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.noise_source.amplitude);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Amplitude:", buf,
                                   editing_value, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_VALUE;
                ui->num_properties++;

                // Amplitude sweep
                prop_y += 22;
                prop_y = draw_sweep_config(renderer, ui, x, prop_y, prop_w,
                    "Amp Sweep:", &selected->props.noise_source.amplitude_sweep,
                    PROP_SWEEP_AMP_ENABLE, PROP_SWEEP_AMP_MODE,
                    PROP_SWEEP_AMP_START, PROP_SWEEP_AMP_END,
                    PROP_SWEEP_AMP_TIME, PROP_SWEEP_AMP_STEPS,
                    PROP_SWEEP_AMP_REPEAT, input, "V");
                break;

            case COMP_DIODE: {
                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.diode.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                if (!selected->props.diode.ideal) {
                    // Saturation current
                    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                    format_engineering(selected->props.diode.is, "A", buf, sizeof(buf));
                    ui_draw_text(renderer, "Is:", x + 10, prop_y + 2);
                    ui_draw_text(renderer, buf, x + 100, prop_y + 2);
                    prop_y += 14;

                    // Ideality factor
                    snprintf(buf, sizeof(buf), "n=%.2f", selected->props.diode.n);
                    ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                    prop_y += 14;

                    // Breakdown voltage (editable)
                    bool edit_bv = input && input->editing_property && input->editing_prop_type == PROP_BV;
                    snprintf(buf, sizeof(buf), "%.1f V", selected->props.diode.bv);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "BV:", buf, edit_bv, edit_buf, cursor);
                    ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    ui->properties[ui->num_properties].prop_type = PROP_BV;
                    ui->num_properties++;
                }
                break;
            }

            case COMP_ZENER: {
                // Zener voltage (editable)
                bool edit_vz = input && input->editing_property && input->editing_prop_type == PROP_VZ;
                snprintf(buf, sizeof(buf), "%.2f V", selected->props.zener.vz);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Vz:", buf, edit_vz, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_VZ;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.zener.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                if (!selected->props.zener.ideal) {
                    // Zener impedance (editable)
                    bool edit_rz = input && input->editing_property && input->editing_prop_type == PROP_RZ;
                    format_engineering(selected->props.zener.rz, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Rz:", buf, edit_rz, edit_buf, cursor);
                    ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    ui->properties[ui->num_properties].prop_type = PROP_RZ;
                    ui->num_properties++;
                    prop_y += 18;

                    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                    format_engineering(selected->props.zener.is, "A", buf, sizeof(buf));
                    ui_draw_text(renderer, "Is:", x + 10, prop_y + 2);
                    ui_draw_text(renderer, buf, x + 100, prop_y + 2);
                }
                break;
            }

            case COMP_SCHOTTKY: {
                // Forward voltage (editable)
                bool edit_vf = input && input->editing_property && input->editing_prop_type == PROP_LED_VF;
                snprintf(buf, sizeof(buf), "%.2f V", selected->props.schottky.vf);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Vf:", buf, edit_vf, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_LED_VF;  // Reuse LED_VF for forward voltage
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.schottky.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                if (!selected->props.schottky.ideal) {
                    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                    format_engineering(selected->props.schottky.is, "A", buf, sizeof(buf));
                    ui_draw_text(renderer, "Is:", x + 10, prop_y + 2);
                    ui_draw_text(renderer, buf, x + 100, prop_y + 2);
                    prop_y += 14;
                    snprintf(buf, sizeof(buf), "n=%.2f", selected->props.schottky.n);
                    ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                }
                break;
            }

            case COMP_CAPACITOR_ELEC: {
                snprintf(buf, sizeof(buf), "%.3g F", selected->props.capacitor_elec.capacitance);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Capacitance:", buf,
                                   editing_value, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_VALUE;
                ui->num_properties++;
                prop_y += 18;

                // Max voltage (editable)
                bool edit_vmax = input && input->editing_property && input->editing_prop_type == PROP_MAX_VOLTAGE;
                snprintf(buf, sizeof(buf), "%.1f V", selected->props.capacitor_elec.max_voltage);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Max Voltage:", buf, edit_vmax, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_MAX_VOLTAGE;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.capacitor_elec.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                if (!selected->props.capacitor_elec.ideal) {
                    bool edit_esr = input && input->editing_property && input->editing_prop_type == PROP_ESR;
                    format_engineering(selected->props.capacitor_elec.esr, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "ESR:", buf, edit_esr, edit_buf, cursor);
                    ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    ui->properties[ui->num_properties].prop_type = PROP_ESR;
                    ui->num_properties++;
                }
                break;
            }

            case COMP_OPAMP: {
                // Open-loop gain (editable)
                bool edit_gain = input && input->editing_property && input->editing_prop_type == PROP_OPAMP_GAIN;
                format_engineering(selected->props.opamp.gain, "", buf, sizeof(buf));
                draw_property_field(renderer, x + 10, prop_y, prop_w, "A_OL:", buf, edit_gain, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_OPAMP_GAIN;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.opamp.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_OPAMP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Rail voltages (editable)
                bool edit_vmax = input && input->editing_property && input->editing_prop_type == PROP_OPAMP_VMAX;
                snprintf(buf, sizeof(buf), "%.1f V", selected->props.opamp.vmax);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "V+:", buf, edit_vmax, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_OPAMP_VMAX;
                ui->num_properties++;
                prop_y += 18;

                bool edit_vmin = input && input->editing_property && input->editing_prop_type == PROP_OPAMP_VMIN;
                snprintf(buf, sizeof(buf), "%.1f V", selected->props.opamp.vmin);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "V-:", buf, edit_vmin, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_OPAMP_VMIN;
                ui->num_properties++;
                prop_y += 18;

                if (!selected->props.opamp.ideal) {
                    // GBW (editable)
                    bool edit_gbw = input && input->editing_property && input->editing_prop_type == PROP_OPAMP_GBW;
                    format_engineering(selected->props.opamp.gbw, "Hz", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "GBW:", buf, edit_gbw, edit_buf, cursor);
                    ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    ui->properties[ui->num_properties].prop_type = PROP_OPAMP_GBW;
                    ui->num_properties++;
                    prop_y += 18;

                    // Slew rate (editable)
                    bool edit_slew = input && input->editing_property && input->editing_prop_type == PROP_OPAMP_SLEW;
                    snprintf(buf, sizeof(buf), "%.2f V/us", selected->props.opamp.slew_rate);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Slew:", buf, edit_slew, edit_buf, cursor);
                    ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    ui->properties[ui->num_properties].prop_type = PROP_OPAMP_SLEW;
                    ui->num_properties++;
                    prop_y += 18;

                    // Input/output impedance (read-only display)
                    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                    format_engineering(selected->props.opamp.r_in, "Ohm", buf, sizeof(buf));
                    ui_draw_text(renderer, "Rin:", x + 10, prop_y + 2);
                    ui_draw_text(renderer, buf, x + 100, prop_y + 2);
                    prop_y += 14;

                    format_engineering(selected->props.opamp.r_out, "Ohm", buf, sizeof(buf));
                    ui_draw_text(renderer, "Rout:", x + 10, prop_y + 2);
                    ui_draw_text(renderer, buf, x + 100, prop_y + 2);
                    prop_y += 14;

                    snprintf(buf, sizeof(buf), "CMRR: %.0f dB", selected->props.opamp.cmrr);
                    ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                }

                // Rail-to-rail toggle
                prop_y += 18;
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "R2R:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.opamp.rail_to_rail ? "[Yes]" : "[No]", x + 100, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_OPAMP_R2R;
                ui->num_properties++;
                break;
            }

            case COMP_LED: {
                // Color selector (click to cycle through presets)
                double wl = selected->props.led.wavelength;
                const char *color_name = "Red";
                if (wl >= 380 && wl < 440) color_name = "Violet";
                else if (wl >= 440 && wl < 490) color_name = "Blue";
                else if (wl >= 490 && wl < 510) color_name = "Cyan";
                else if (wl >= 510 && wl < 580) color_name = "Green";
                else if (wl >= 580 && wl < 600) color_name = "Yellow";
                else if (wl >= 600 && wl < 640) color_name = "Orange";
                else if (wl >= 640 && wl <= 780) color_name = "Red";
                else if (wl > 780) color_name = "IR";
                else if (wl == 0) color_name = "White";

                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Color:", x + 10, prop_y + 2);
                // Draw color selector with dropdown indicator
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                snprintf(buf, sizeof(buf), "[%s] v", color_name);
                ui_draw_text(renderer, buf, x + 100, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_LED_COLOR;
                ui->num_properties++;
                prop_y += 18;

                // Wavelength (read-only, shows actual value)
                SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                snprintf(buf, sizeof(buf), "(%.0f nm)", wl);
                ui_draw_text(renderer, buf, x + 100, prop_y + 2);
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.led.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Forward voltage (editable)
                bool editing_vf = input && input->editing_property && input->editing_prop_type == PROP_LED_VF;
                snprintf(buf, sizeof(buf), "%.2f V", selected->props.led.vf);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Fwd Voltage:", buf,
                                   editing_vf, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_LED_VF;
                ui->num_properties++;
                prop_y += 18;

                // Max current (editable)
                bool editing_imax = input && input->editing_property && input->editing_prop_type == PROP_LED_IMAX;
                snprintf(buf, sizeof(buf), "%.0f mA", selected->props.led.max_current * 1000);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Max Current:", buf,
                                   editing_imax, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_LED_IMAX;
                ui->num_properties++;
                prop_y += 18;

                // Actual current (read-only, with warning if overcurrent)
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Current:", x + 10, prop_y + 2);
                double curr_ratio = selected->props.led.current / selected->props.led.max_current;
                if (curr_ratio > 1.0) {
                    SDL_SetRenderDrawColor(renderer, 0xff, 0x40, 0x40, 0xff);  // Red - overcurrent!
                } else if (curr_ratio > 0.8) {
                    SDL_SetRenderDrawColor(renderer, 0xff, 0xaa, 0x00, 0xff);  // Orange - warning
                } else {
                    SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);  // Green - OK
                }
                snprintf(buf, sizeof(buf), "%.2f mA (%.0f%%)", selected->props.led.current * 1000,
                         curr_ratio * 100);
                ui_draw_text(renderer, buf, x + 100, prop_y + 2);
                break;
            }

            case COMP_NPN_BJT:
            case COMP_PNP_BJT: {
                // Beta (forward current gain)
                bool editing_beta = input && input->editing_property && input->editing_prop_type == PROP_BJT_BETA;
                snprintf(buf, sizeof(buf), "%.1f", selected->props.bjt.bf);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Beta (BF):", buf,
                                   editing_beta, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_BJT_BETA;
                ui->num_properties++;
                prop_y += 18;

                // Saturation current (Is)
                bool editing_is = input && input->editing_property && input->editing_prop_type == PROP_BJT_IS;
                format_engineering(selected->props.bjt.is, "A", buf, sizeof(buf));
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Is:", buf,
                                   editing_is, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_BJT_IS;
                ui->num_properties++;
                prop_y += 18;

                // Early voltage (VAF)
                bool editing_vaf = input && input->editing_property && input->editing_prop_type == PROP_BJT_VAF;
                snprintf(buf, sizeof(buf), "%.1f V", selected->props.bjt.vaf);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "VAF:", buf,
                                   editing_vaf, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_BJT_VAF;
                ui->num_properties++;
                prop_y += 18;

                // Ideal/Non-ideal mode toggle (read-only display, click to toggle)
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                if (selected->props.bjt.ideal) {
                    SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);
                    ui_draw_text(renderer, "[Ideal]", x + 100, prop_y + 2);
                } else {
                    SDL_SetRenderDrawColor(renderer, 0xff, 0xaa, 0x00, 0xff);
                    ui_draw_text(renderer, "[SPICE]", x + 100, prop_y + 2);
                }
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_BJT_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Additional SPICE params shown only in non-ideal mode
                if (!selected->props.bjt.ideal) {
                    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                    snprintf(buf, sizeof(buf), "NF=%.2f BR=%.1f", selected->props.bjt.nf, selected->props.bjt.br);
                    ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                    prop_y += 14;
                    snprintf(buf, sizeof(buf), "VAR=%.1fV T=%.0fK", selected->props.bjt.var, selected->props.bjt.temp);
                    ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                }
                break;
            }

            case COMP_NMOS:
            case COMP_PMOS: {
                // Threshold voltage (Vth)
                bool editing_vth = input && input->editing_property && input->editing_prop_type == PROP_MOS_VTH;
                snprintf(buf, sizeof(buf), "%.2f V", selected->props.mosfet.vth);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Vth:", buf,
                                   editing_vth, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_MOS_VTH;
                ui->num_properties++;
                prop_y += 18;

                // Transconductance parameter (Kp)
                bool editing_kp = input && input->editing_property && input->editing_prop_type == PROP_MOS_KP;
                format_engineering(selected->props.mosfet.kp, "A/V2", buf, sizeof(buf));
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Kp:", buf,
                                   editing_kp, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_MOS_KP;
                ui->num_properties++;
                prop_y += 18;

                // Channel width (W)
                bool editing_w = input && input->editing_property && input->editing_prop_type == PROP_MOS_W;
                format_engineering(selected->props.mosfet.w, "m", buf, sizeof(buf));
                draw_property_field(renderer, x + 10, prop_y, prop_w, "W:", buf,
                                   editing_w, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_MOS_W;
                ui->num_properties++;
                prop_y += 18;

                // Channel length (L)
                bool editing_l = input && input->editing_property && input->editing_prop_type == PROP_MOS_L;
                format_engineering(selected->props.mosfet.l, "m", buf, sizeof(buf));
                draw_property_field(renderer, x + 10, prop_y, prop_w, "L:", buf,
                                   editing_l, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_MOS_L;
                ui->num_properties++;
                prop_y += 18;

                // Ideal/Non-ideal mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                if (selected->props.mosfet.ideal) {
                    SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);
                    ui_draw_text(renderer, "[Ideal]", x + 100, prop_y + 2);
                } else {
                    SDL_SetRenderDrawColor(renderer, 0xff, 0xaa, 0x00, 0xff);
                    ui_draw_text(renderer, "[SPICE]", x + 100, prop_y + 2);
                }
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_MOS_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Additional SPICE params shown only in non-ideal mode
                if (!selected->props.mosfet.ideal) {
                    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                    snprintf(buf, sizeof(buf), "Lambda=%.3f", selected->props.mosfet.lambda);
                    ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                    prop_y += 14;
                    snprintf(buf, sizeof(buf), "Gamma=%.2f Phi=%.2fV", selected->props.mosfet.gamma, selected->props.mosfet.phi);
                    ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                }
                break;
            }

            case COMP_TEXT: {
                // Text content (editable)
                bool edit_text = input && input->editing_property && input->editing_prop_type == PROP_TEXT_CONTENT;
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Text:", selected->props.text.text,
                                   edit_text, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_TEXT_CONTENT;
                ui->num_properties++;
                prop_y += 18;

                // Font size selector
                const char *size_names[] = {"Small", "Normal", "Large"};
                int size_idx = selected->props.text.font_size - 1;
                if (size_idx < 0) size_idx = 0;
                if (size_idx > 2) size_idx = 2;
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Size:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                snprintf(buf, sizeof(buf), "[%s]", size_names[size_idx]);
                ui_draw_text(renderer, buf, x + 100, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, 60, 14};
                ui->properties[ui->num_properties].prop_type = PROP_TEXT_SIZE;
                ui->num_properties++;
                prop_y += 18;

                // Bold/Italic/Underline toggles on same row
                int toggle_x = x + 10;
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Style:", toggle_x, prop_y + 2);
                toggle_x += 50;

                // Bold toggle
                if (selected->props.text.bold) {
                    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                } else {
                    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                }
                ui_draw_text(renderer, "[B]", toggle_x, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){toggle_x, prop_y, 24, 14};
                ui->properties[ui->num_properties].prop_type = PROP_TEXT_BOLD;
                ui->num_properties++;
                toggle_x += 30;

                // Italic toggle
                if (selected->props.text.italic) {
                    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                } else {
                    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                }
                ui_draw_text(renderer, "[I]", toggle_x, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){toggle_x, prop_y, 24, 14};
                ui->properties[ui->num_properties].prop_type = PROP_TEXT_ITALIC;
                ui->num_properties++;
                toggle_x += 30;

                // Underline toggle
                if (selected->props.text.underline) {
                    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                } else {
                    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                }
                ui_draw_text(renderer, "[U]", toggle_x, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){toggle_x, prop_y, 24, 14};
                ui->properties[ui->num_properties].prop_type = PROP_TEXT_UNDERLINE;
                ui->num_properties++;
                break;
            }

            case COMP_FUSE: {
                // Rating (current in amps)
                snprintf(buf, sizeof(buf), "%.3g A", selected->props.fuse.rating);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Rating:", buf,
                                   editing_value, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_VALUE;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle (Ideal = instant blow, Real = i²t model)
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.fuse.ideal ? "[Ideal]" : "[i2t]", x + 100, prop_y + 2);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Show fuse status
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Status:", x + 10, prop_y + 2);
                if (selected->props.fuse.blown) {
                    SDL_SetRenderDrawColor(renderer, 0xff, 0x40, 0x40, 0xff);  // Red for blown
                    ui_draw_text(renderer, "BLOWN", x + 100, prop_y + 2);
                } else {
                    SDL_SetRenderDrawColor(renderer, SYNTH_GREEN, 0xff);
                    ui_draw_text(renderer, "OK", x + 100, prop_y + 2);
                }
                prop_y += 18;

                // Reset button (if blown)
                if (selected->props.fuse.blown) {
                    SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0xff);
                    ui_draw_text(renderer, "[Reset Fuse]", x + 10, prop_y + 2);
                    ui->properties[ui->num_properties].bounds = (Rect){x + 10, prop_y, 90, 14};
                    ui->properties[ui->num_properties].prop_type = PROP_RESET_FUSE;
                    ui->num_properties++;
                    prop_y += 18;
                }
                break;
            }

            default:
                break;
        }

        // Help text - synthwave dim text
        prop_y += 25;
        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DARK, 0xff);
        ui_draw_text(renderer, "Click value to edit", x + 10, prop_y);
        ui_draw_text(renderer, "Use k,M,m,u,n,p suffix", x + 10, prop_y + 12);

        // Track content height for scrollbar calculations (relative to panel start)
        ui->properties_content_height = prop_y + 30 - content_y;  // Include help text
    } else {
        // Reset num_properties when nothing is selected to avoid stale bounds
        ui->num_properties = 0;

        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DARK, 0xff);
        ui_draw_text(renderer, "No selection", x + 10, content_y + 35);
        ui_draw_text(renderer, "Click component", x + 10, content_y + 55);
        ui_draw_text(renderer, "to edit properties", x + 10, content_y + 70);

        // Minimal content height when nothing selected
        ui->properties_content_height = 100;
    }

    // Reset clipping before drawing scrollbar and border
    SDL_RenderSetClipRect(renderer, NULL);

    // Draw scrollbar if content exceeds visible area
    if (ui->properties_content_height > ui->properties_visible_height) {
        int scrollbar_x = ui->window_width - 8;
        int scrollbar_track_y = y + 2;
        int scrollbar_track_h = panel_height - 4;

        // Draw track (darker background) - synthwave dark
        SDL_SetRenderDrawColor(renderer, SYNTH_BG_DARK, 0xff);
        SDL_Rect track = {scrollbar_x, scrollbar_track_y, 6, scrollbar_track_h};
        SDL_RenderFillRect(renderer, &track);

        // Calculate thumb position and size
        float visible_ratio = (float)ui->properties_visible_height / ui->properties_content_height;
        int thumb_h = (int)(scrollbar_track_h * visible_ratio);
        if (thumb_h < 20) thumb_h = 20;  // Minimum thumb size

        int max_scroll = ui->properties_content_height - ui->properties_visible_height;
        float scroll_ratio = (max_scroll > 0) ? (float)ui->properties_scroll_offset / max_scroll : 0;
        int thumb_y = scrollbar_track_y + (int)((scrollbar_track_h - thumb_h) * scroll_ratio);

        // Draw thumb - synthwave purple
        SDL_SetRenderDrawColor(renderer, SYNTH_PURPLE, 0xff);
        SDL_Rect thumb = {scrollbar_x, thumb_y, 6, thumb_h};
        SDL_RenderFillRect(renderer, &thumb);
    }

    // Border - synthwave border
    SDL_SetRenderDrawColor(renderer, SYNTH_BORDER, 0xff);
    SDL_RenderDrawLine(renderer, x, y, x, ui->window_height - STATUSBAR_HEIGHT);
}

void ui_render_measurements(UIState *ui, SDL_Renderer *renderer, Simulation *sim) {
    int x = ui->window_width - ui->properties_width;
    // Position measurements panel below the oscilloscope
    int y = ui->scope_rect.y + ui->scope_rect.h + 5;

    // Background - synthwave dark
    SDL_SetRenderDrawColor(renderer, SYNTH_BG_DARK, 0xff);
    SDL_Rect panel = {x, y, ui->properties_width, 180};
    SDL_RenderFillRect(renderer, &panel);

    // Title - synthwave pink
    SDL_SetRenderDrawColor(renderer, SYNTH_PINK, 0xff);
    ui_draw_text(renderer, "Measurements", x + 10, y + 10);

    // Voltmeter
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT, 0xff);
    ui_draw_text(renderer, "Voltmeter:", x + 10, y + 35);
    char volt_str[32];
    snprintf(volt_str, sizeof(volt_str), "%.3f V", ui->voltmeter_value);
    SDL_SetRenderDrawColor(renderer, SYNTH_GREEN, 0xff);
    ui_draw_text(renderer, volt_str, x + 100, y + 35);

    // Ammeter
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT, 0xff);
    ui_draw_text(renderer, "Ammeter:", x + 10, y + 55);
    char amp_str[32];
    snprintf(amp_str, sizeof(amp_str), "%.3f mA", ui->ammeter_value * 1000.0);
    SDL_SetRenderDrawColor(renderer, SYNTH_ORANGE, 0xff);
    ui_draw_text(renderer, amp_str, x + 100, y + 55);
}

// Helper to format scope values with proper units
static void format_time_value(char *buf, size_t size, double val) {
    if (val >= 1.0) {
        snprintf(buf, size, "%.0fs", val);
    } else if (val >= 0.1) {
        snprintf(buf, size, "%.0fms", val * 1000);
    } else if (val >= 0.001) {
        snprintf(buf, size, "%.0fms", val * 1000);
    } else if (val >= 0.0001) {
        snprintf(buf, size, "%.0fus", val * 1000000);
    } else {
        snprintf(buf, size, "%.1fus", val * 1000000);
    }
}

static void format_volt_value(char *buf, size_t size, double val) {
    if (val >= 1.0) {
        snprintf(buf, size, "%.0fV", val);
    } else if (val >= 0.1) {
        snprintf(buf, size, "%.0fmV", val * 1000);
    } else {
        snprintf(buf, size, "%.1fmV", val * 1000);
    }
}

void ui_render_oscilloscope(UIState *ui, SDL_Renderer *renderer, Simulation *sim, void *analysis_ptr) {
    AnalysisState *analysis = (AnalysisState *)analysis_ptr;
    Rect *r = &ui->scope_rect;
    char buf[64];

    // Update button labels based on current settings
    static const char *trig_mode_labels[] = {"AUTO", "NORM", "SNGL"};
    static const char *trig_edge_labels[] = {"/\\", "\\/", "/\\\\/"};
    static const char *mode_labels[] = {"Y-T", "X-Y"};

    ui->btn_scope_trig_mode.label = trig_mode_labels[ui->trigger_mode];
    ui->btn_scope_trig_edge.label = trig_edge_labels[ui->trigger_edge];
    ui->btn_scope_mode.label = mode_labels[ui->display_mode];

    // Update trigger channel button label
    static char trig_ch_label[8];
    snprintf(trig_ch_label, sizeof(trig_ch_label), "CH%d", ui->trigger_channel + 1);
    ui->btn_scope_trig_ch.label = trig_ch_label;

    // Title bar with settings
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    ui_draw_text(renderer, "SCOPE", r->x, r->y - 18);

    // Show time/div in title area (only for Y-T mode)
    if (ui->display_mode == SCOPE_MODE_YT) {
        SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
        format_time_value(buf, sizeof(buf), ui->scope_time_div);
        ui_draw_text(renderer, buf, r->x + 55, r->y - 18);
        ui_draw_text(renderer, "/div", r->x + 100, r->y - 18);
    } else {
        SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
        ui_draw_text(renderer, "X-Y Mode", r->x + 55, r->y - 18);
    }

    // Background (dark green like classic scopes)
    SDL_SetRenderDrawColor(renderer, 0x00, 0x10, 0x00, 0xff);
    SDL_Rect bg = {r->x, r->y, r->w, r->h};
    SDL_RenderFillRect(renderer, &bg);

    // Draw resize grip at top-left corner (small diagonal lines)
    SDL_SetRenderDrawColor(renderer, 0x60, 0x60, 0x60, 0xff);
    for (int i = 0; i < 3; i++) {
        SDL_RenderDrawLine(renderer, r->x + 2 + i*3, r->y + 8, r->x + 8, r->y + 2 + i*3);
    }
    // Highlight grip if resizing
    if (ui->scope_resizing) {
        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0xff, 0xff);
        for (int i = 0; i < 3; i++) {
            SDL_RenderDrawLine(renderer, r->x + 2 + i*3, r->y + 8, r->x + 8, r->y + 2 + i*3);
        }
    }

    int div_x = r->w / 10;
    int div_y = r->h / 8;

    // Draw dotted subdivision lines (5 subdivisions per division)
    SDL_SetRenderDrawColor(renderer, 0x20, 0x30, 0x20, 0xff);
    for (int i = 0; i < 10; i++) {
        for (int sub = 1; sub < 5; sub++) {
            int x = r->x + i * div_x + (sub * div_x / 5);
            for (int y = r->y; y < r->y + r->h; y += 4) {
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
    }
    for (int i = 0; i < 8; i++) {
        for (int sub = 1; sub < 5; sub++) {
            int y = r->y + i * div_y + (sub * div_y / 5);
            for (int x = r->x; x < r->x + r->w; x += 4) {
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
    }

    // Main grid lines
    SDL_SetRenderDrawColor(renderer, 0x30, 0x50, 0x30, 0xff);
    for (int i = 0; i <= 10; i++) {
        SDL_RenderDrawLine(renderer, r->x + i * div_x, r->y, r->x + i * div_x, r->y + r->h);
    }
    for (int i = 0; i <= 8; i++) {
        SDL_RenderDrawLine(renderer, r->x, r->y + i * div_y, r->x + r->w, r->y + i * div_y);
    }

    // Center crosshair (brighter, with tick marks)
    SDL_SetRenderDrawColor(renderer, 0x50, 0x80, 0x50, 0xff);
    int center_x = r->x + r->w / 2;
    int center_y = r->y + r->h / 2;
    SDL_RenderDrawLine(renderer, center_x, r->y, center_x, r->y + r->h);
    SDL_RenderDrawLine(renderer, r->x, center_y, r->x + r->w, center_y);

    // Draw small tick marks on center lines
    for (int i = 0; i <= 10; i++) {
        int x = r->x + i * div_x;
        SDL_RenderDrawLine(renderer, x, center_y - 3, x, center_y + 3);
    }
    for (int i = 0; i <= 8; i++) {
        int y = r->y + i * div_y;
        SDL_RenderDrawLine(renderer, center_x - 3, y, center_x + 3, y);
    }

    // Draw voltage scale labels on Y-axis (left side)
    // 8 divisions total: 4 above center (positive) and 4 below (negative)
    SDL_SetRenderDrawColor(renderer, 0x60, 0x80, 0x60, 0xff);
    for (int i = 0; i <= 8; i++) {
        int y = r->y + i * div_y;
        // Calculate voltage value: top is +4*V/div, center is 0, bottom is -4*V/div
        double voltage = (4 - i) * ui->scope_volt_div;

        // Format voltage label
        char vlabel[16];
        if (fabs(voltage) < 0.001) {
            snprintf(vlabel, sizeof(vlabel), "0V");
        } else if (fabs(ui->scope_volt_div) >= 1.0) {
            snprintf(vlabel, sizeof(vlabel), "%+.0fV", voltage);
        } else if (fabs(ui->scope_volt_div) >= 0.1) {
            snprintf(vlabel, sizeof(vlabel), "%+.1fV", voltage);
        } else if (fabs(ui->scope_volt_div) >= 0.01) {
            snprintf(vlabel, sizeof(vlabel), "%+.0fmV", voltage * 1000);
        } else {
            snprintf(vlabel, sizeof(vlabel), "%+.1fmV", voltage * 1000);
        }

        // Draw tick mark on left edge
        SDL_RenderDrawLine(renderer, r->x, y, r->x + 5, y);

        // Draw label to the right of tick (inside scope area)
        ui_draw_text(renderer, vlabel, r->x + 7, y - 5);
    }

    // Draw traces for all active channels (based on probes)
    if (sim && sim->history_count > 1 && !ui->scope_paused) {
        double scale = (r->h / 8.0) / ui->scope_volt_div;

        // Check if FFT mode is enabled
        if (ui->scope_fft_mode && ui->display_mode == SCOPE_MODE_YT && analysis) {
            // FFT spectrum display
            // Draw frequency grid labels
            int num_decades = 4;  // 10Hz to 100kHz
            double freq_min = 10.0;
            double freq_max = 100000.0;

            SDL_SetRenderDrawColor(renderer, 0x40, 0x60, 0x40, 0xff);
            for (int d = 0; d <= num_decades; d++) {
                double freq = freq_min * pow(10, d);
                double x_frac = log10(freq / freq_min) / log10(freq_max / freq_min);
                int grid_x = r->x + (int)(x_frac * r->w);
                for (int y = r->y; y < r->y + r->h; y += 4) {
                    SDL_RenderDrawPoint(renderer, grid_x, y);
                }
            }

            // Draw dB scale labels (-60dB to 0dB)
            SDL_SetRenderDrawColor(renderer, 0x60, 0x60, 0x60, 0xff);
            ui_draw_text(renderer, "0dB", r->x + 3, r->y + 5);
            ui_draw_text(renderer, "-60dB", r->x + 3, r->y + r->h - 15);

            for (int ch = 0; ch < ui->scope_num_channels && ch < MAX_PROBES; ch++) {
                if (!ui->scope_channels[ch].enabled) continue;

                FFTResult *fft = &analysis->fft_results[ch];
                if (fft->num_bins < 2) continue;

                SDL_SetRenderDrawColor(renderer,
                    ui->scope_channels[ch].color.r,
                    ui->scope_channels[ch].color.g,
                    ui->scope_channels[ch].color.b, 0xff);

                int prev_x = -1, prev_y = -1;

                for (int k = 1; k < fft->num_bins; k++) {
                    double freq = fft->frequency[k];
                    if (freq < freq_min || freq > freq_max) continue;

                    // X: logarithmic frequency scale
                    double x_frac = log10(freq / freq_min) / log10(freq_max / freq_min);
                    int px = r->x + (int)(x_frac * r->w);

                    // Y: linear dB scale (-60dB to 0dB)
                    double db = fft->magnitude[k];
                    db = CLAMP(db, -60.0, 0.0);
                    double y_frac = (db + 60.0) / 60.0;  // 0 = -60dB (bottom), 1 = 0dB (top)
                    int py = r->y + r->h - (int)(y_frac * r->h);

                    if (prev_x >= 0) {
                        SDL_RenderDrawLine(renderer, prev_x, prev_y, px, py);
                    }
                    prev_x = px;
                    prev_y = py;
                }

                // Show THD and SNR info
                if (ch == 0 && fft->thd > 0) {
                    snprintf(buf, sizeof(buf), "THD:%.1f%%", fft->thd);
                    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0x00, 0xff);
                    ui_draw_text(renderer, buf, r->x + r->w - 70, r->y + 5);
                    snprintf(buf, sizeof(buf), "SNR:%.0fdB", fft->snr);
                    ui_draw_text(renderer, buf, r->x + r->w - 70, r->y + 17);
                    snprintf(buf, sizeof(buf), "F0:%.1fHz", fft->fundamental_freq);
                    ui_draw_text(renderer, buf, r->x + r->w - 70, r->y + 29);
                }
            }

            // Show FFT mode label
            SDL_SetRenderDrawColor(renderer, 0xff, 0x80, 0x00, 0xff);
            ui_draw_text(renderer, "FFT SPECTRUM", r->x + 3, r->y + r->h - 30);

        } else if (ui->display_mode == SCOPE_MODE_YT) {
            // Y-T mode: standard time-domain display with proper triggering
            // Calculate time window (10 divisions on the scope)
            double time_window = 10.0 * ui->scope_time_div;

            // Get more samples than needed to search for trigger points
            // We want at least 2x the display window to find triggers
            int samples_for_trigger = (int)(time_window * 3.0 / sim->time_step);
            if (samples_for_trigger < 100) samples_for_trigger = 100;
            if (samples_for_trigger > MAX_HISTORY) samples_for_trigger = MAX_HISTORY;

            // Get trigger channel data for trigger detection
            double trig_times[MAX_HISTORY];
            double trig_values[MAX_HISTORY];
            int trig_ch = ui->trigger_channel;
            if (trig_ch >= ui->scope_num_channels) trig_ch = 0;
            int trig_probe = ui->scope_channels[trig_ch].probe_idx;
            int trig_count = simulation_get_history(sim, trig_probe, trig_times, trig_values, samples_for_trigger);

            // Search for trigger point in the data
            int trigger_idx = -1;
            bool need_new_trigger = true;

            // For SINGLE mode, don't look for new trigger if already triggered
            if (ui->trigger_mode == TRIG_SINGLE && ui->triggered && ui->scope_capture_valid) {
                need_new_trigger = false;
            }

            // Check holdoff - don't trigger too frequently
            if (need_new_trigger && ui->scope_capture_valid && trig_count > 0) {
                double current_time = trig_times[trig_count - 1];
                if (current_time - ui->scope_last_trigger_time < ui->trigger_holdoff) {
                    need_new_trigger = false;
                }
            }

            if (need_new_trigger && trig_count > 10) {
                double level = ui->trigger_level;

                // Search for trigger crossing (look for edge in middle portion of data)
                // Start from 1/3 into the buffer so we have pre-trigger data
                int search_start = trig_count / 3;
                int search_end = trig_count - 10;

                for (int i = search_start; i < search_end; i++) {
                    bool triggered_here = false;
                    double v_prev = trig_values[i - 1];
                    double v_curr = trig_values[i];

                    switch (ui->trigger_edge) {
                        case TRIG_EDGE_RISING:
                            // Rising edge: previous below level, current at or above
                            if (v_prev < level && v_curr >= level) {
                                triggered_here = true;
                            }
                            break;
                        case TRIG_EDGE_FALLING:
                            // Falling edge: previous above level, current at or below
                            if (v_prev > level && v_curr <= level) {
                                triggered_here = true;
                            }
                            break;
                        case TRIG_EDGE_BOTH:
                            // Either edge
                            if ((v_prev < level && v_curr >= level) ||
                                (v_prev > level && v_curr <= level)) {
                                triggered_here = true;
                            }
                            break;
                    }

                    if (triggered_here) {
                        trigger_idx = i;
                        break;  // Use first trigger found
                    }
                }
            }

            // Handle trigger modes
            bool use_capture = false;

            if (trigger_idx >= 0) {
                // Found a trigger - capture the data
                ui->scope_last_trigger_time = trig_times[trigger_idx];
                ui->scope_trigger_sample_idx = trigger_idx;
                ui->triggered = true;

                // Calculate how many samples to display (for the time window)
                int display_samples = (int)(time_window / sim->time_step);
                if (display_samples < 2) display_samples = 2;
                if (display_samples > SCOPE_CAPTURE_SIZE - 10) display_samples = SCOPE_CAPTURE_SIZE - 10;

                // Capture data: start some samples before trigger for pre-trigger view
                // Put trigger point at about 20% from left edge
                int pre_trigger = display_samples / 5;
                int capture_start = trigger_idx - pre_trigger;
                if (capture_start < 0) capture_start = 0;
                int capture_end = capture_start + display_samples;
                if (capture_end > trig_count) capture_end = trig_count;

                ui->scope_capture_count = capture_end - capture_start;
                if (ui->scope_capture_count > SCOPE_CAPTURE_SIZE) ui->scope_capture_count = SCOPE_CAPTURE_SIZE;

                // Capture times (from trigger channel)
                for (int i = 0; i < ui->scope_capture_count; i++) {
                    ui->scope_capture_times[i] = trig_times[capture_start + i];
                }

                // Capture all channel values
                for (int ch = 0; ch < ui->scope_num_channels && ch < MAX_PROBES; ch++) {
                    double ch_times[MAX_HISTORY], ch_values[MAX_HISTORY];
                    int ch_probe = ui->scope_channels[ch].probe_idx;
                    int ch_count = simulation_get_history(sim, ch_probe, ch_times, ch_values, samples_for_trigger);

                    // Match samples by index (assumes same time base)
                    for (int i = 0; i < ui->scope_capture_count && (capture_start + i) < ch_count; i++) {
                        ui->scope_capture_values[ch][i] = ch_values[capture_start + i];
                    }
                }

                ui->scope_capture_time = sim->time;
                ui->scope_capture_valid = true;
                use_capture = true;
            } else {
                // No trigger found
                if (ui->trigger_mode == TRIG_AUTO) {
                    // AUTO mode: free-run, show latest data without triggering
                    // Use the most recent samples
                    int display_samples = (int)(time_window / sim->time_step);
                    if (display_samples < 2) display_samples = 2;
                    if (display_samples > trig_count) display_samples = trig_count;
                    if (display_samples > SCOPE_CAPTURE_SIZE - 10) display_samples = SCOPE_CAPTURE_SIZE - 10;

                    int capture_start = trig_count - display_samples;
                    if (capture_start < 0) capture_start = 0;

                    ui->scope_capture_count = trig_count - capture_start;
                    if (ui->scope_capture_count > SCOPE_CAPTURE_SIZE) ui->scope_capture_count = SCOPE_CAPTURE_SIZE;

                    for (int i = 0; i < ui->scope_capture_count; i++) {
                        ui->scope_capture_times[i] = trig_times[capture_start + i];
                    }

                    for (int ch = 0; ch < ui->scope_num_channels && ch < MAX_PROBES; ch++) {
                        double ch_times[MAX_HISTORY], ch_values[MAX_HISTORY];
                        int ch_probe = ui->scope_channels[ch].probe_idx;
                        int ch_count = simulation_get_history(sim, ch_probe, ch_times, ch_values, samples_for_trigger);

                        for (int i = 0; i < ui->scope_capture_count && (capture_start + i) < ch_count; i++) {
                            ui->scope_capture_values[ch][i] = ch_values[capture_start + i];
                        }
                    }

                    ui->scope_capture_time = sim->time;
                    ui->scope_capture_valid = true;
                    use_capture = true;
                } else if (ui->scope_capture_valid) {
                    // NORMAL/SINGLE mode: use previous captured data
                    use_capture = true;
                }
            }

            // Render the captured/current waveform
            if (use_capture && ui->scope_capture_count >= 2) {
                double t_start = ui->scope_capture_times[0];
                double t_end = ui->scope_capture_times[ui->scope_capture_count - 1];
                double t_span = t_end - t_start;
                if (t_span < 1e-12) t_span = time_window;

                for (int ch = 0; ch < ui->scope_num_channels && ch < MAX_PROBES; ch++) {
                    if (!ui->scope_channels[ch].enabled) continue;

                    SDL_SetRenderDrawColor(renderer,
                        ui->scope_channels[ch].color.r,
                        ui->scope_channels[ch].color.g,
                        ui->scope_channels[ch].color.b, 0xff);

                    double offset = ui->scope_channels[ch].offset;

                    for (int i = 1; i < ui->scope_capture_count; i++) {
                        double x_frac1 = (ui->scope_capture_times[i-1] - t_start) / t_span;
                        double x_frac2 = (ui->scope_capture_times[i] - t_start) / t_span;
                        int x1 = r->x + (int)(x_frac1 * r->w);
                        int x2 = r->x + (int)(x_frac2 * r->w);
                        int y1 = center_y - (int)((ui->scope_capture_values[ch][i-1] + offset) * scale);
                        int y2 = center_y - (int)((ui->scope_capture_values[ch][i] + offset) * scale);
                        x1 = CLAMP(x1, r->x, r->x + r->w);
                        x2 = CLAMP(x2, r->x, r->x + r->w);
                        y1 = CLAMP(y1, r->y, r->y + r->h);
                        y2 = CLAMP(y2, r->y, r->y + r->h);
                        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
                    }

                    // Draw ground reference arrow on left side (channel color)
                    int gnd_y = center_y - (int)(offset * scale);
                    gnd_y = CLAMP(gnd_y, r->y + 8, r->y + r->h - 8);
                    SDL_RenderDrawLine(renderer, r->x + 2, gnd_y, r->x + 8, gnd_y);
                    SDL_RenderDrawLine(renderer, r->x + 5, gnd_y - 3, r->x + 8, gnd_y);
                    SDL_RenderDrawLine(renderer, r->x + 5, gnd_y + 3, r->x + 8, gnd_y);
                }

                // Draw trigger point marker (small T on the waveform)
                if (trigger_idx >= 0 && ui->trigger_mode != TRIG_AUTO) {
                    // Show trigger indicator at ~20% from left
                    int trig_x = r->x + r->w / 5;
                    SDL_SetRenderDrawColor(renderer, 0xff, 0x80, 0x00, 0xff);  // Orange
                    // Small downward arrow at top
                    SDL_RenderDrawLine(renderer, trig_x, r->y + 2, trig_x, r->y + 8);
                    SDL_RenderDrawLine(renderer, trig_x - 3, r->y + 5, trig_x, r->y + 8);
                    SDL_RenderDrawLine(renderer, trig_x + 3, r->y + 5, trig_x, r->y + 8);
                }
            }

            // Draw trigger level line (dashed, in orange)
            if (ui->scope_num_channels > 0) {
                if (trig_ch < ui->scope_num_channels && ui->scope_channels[trig_ch].enabled) {
                    double trig_offset = ui->scope_channels[trig_ch].offset;
                    int trig_y = center_y - (int)((ui->trigger_level + trig_offset) * scale);
                    trig_y = CLAMP(trig_y, r->y, r->y + r->h);

                    SDL_SetRenderDrawColor(renderer, 0xff, 0x80, 0x00, 0xff);  // Orange
                    // Draw dashed line
                    for (int x = r->x; x < r->x + r->w; x += 8) {
                        SDL_RenderDrawLine(renderer, x, trig_y, MIN(x + 4, r->x + r->w), trig_y);
                    }
                    // Draw trigger level indicator on right side (arrow)
                    SDL_RenderDrawLine(renderer, r->x + r->w - 8, trig_y, r->x + r->w - 2, trig_y);
                    SDL_RenderDrawLine(renderer, r->x + r->w - 5, trig_y - 3, r->x + r->w - 2, trig_y);
                    SDL_RenderDrawLine(renderer, r->x + r->w - 5, trig_y + 3, r->x + r->w - 2, trig_y);
                }
            }

            // Show trigger status
            const char *trig_status = NULL;
            if (ui->trigger_mode == TRIG_SINGLE) {
                if (ui->triggered) {
                    trig_status = "TRIG'D";
                } else {
                    trig_status = "WAIT";
                }
            } else if (ui->trigger_mode == TRIG_NORMAL && !ui->scope_capture_valid) {
                trig_status = "WAIT";
            }

            if (trig_status) {
                SDL_SetRenderDrawColor(renderer, 0xff, 0x80, 0x00, 0xff);
                ui_draw_text(renderer, trig_status, r->x + r->w - 50, r->y + 5);
            }
        } else {
            // X-Y mode: Lissajous display
            // Channel X (xy_channel_x) on horizontal axis, Channel Y (xy_channel_y) on vertical axis
            if (ui->scope_num_channels >= 2 &&
                ui->xy_channel_x < ui->scope_num_channels &&
                ui->xy_channel_y < ui->scope_num_channels) {

                SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x00, 0xff);  // Green for X-Y trace

                double times_x[MAX_HISTORY], values_x[MAX_HISTORY];
                double times_y[MAX_HISTORY], values_y[MAX_HISTORY];

                int count_x = simulation_get_history(sim, ui->xy_channel_x, times_x, values_x, MAX_HISTORY);
                int count_y = simulation_get_history(sim, ui->xy_channel_y, times_y, values_y, MAX_HISTORY);
                int count = MIN(count_x, count_y);

                double offset_x = ui->scope_channels[ui->xy_channel_x].offset;
                double offset_y = ui->scope_channels[ui->xy_channel_y].offset;

                for (int i = 1; i < count; i++) {
                    int x1 = center_x + (int)((values_x[i-1] + offset_x) * scale);
                    int x2 = center_x + (int)((values_x[i] + offset_x) * scale);
                    int y1 = center_y - (int)((values_y[i-1] + offset_y) * scale);
                    int y2 = center_y - (int)((values_y[i] + offset_y) * scale);

                    x1 = CLAMP(x1, r->x, r->x + r->w);
                    x2 = CLAMP(x2, r->x, r->x + r->w);
                    y1 = CLAMP(y1, r->y, r->y + r->h);
                    y2 = CLAMP(y2, r->y, r->y + r->h);

                    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
                }
            } else {
                // Not enough channels for X-Y mode
                SDL_SetRenderDrawColor(renderer, 0x80, 0x60, 0x00, 0xff);
                ui_draw_text(renderer, "X-Y needs 2+ probes", r->x + 50, center_y);
            }
        }
    }

    // Draw measurement cursors if enabled (Y-T mode only)
    if (ui->scope_cursor_mode && ui->display_mode == SCOPE_MODE_YT) {
        // Cursor 1 (cyan dashed line)
        int cursor1_x = r->x + (int)(ui->cursor1_time * r->w);
        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0xff, 0xff);
        for (int y = r->y; y < r->y + r->h; y += 4) {
            SDL_RenderDrawLine(renderer, cursor1_x, y, cursor1_x, MIN(y + 2, r->y + r->h));
        }
        // Label
        ui_draw_text(renderer, "C1", cursor1_x - 6, r->y + 2);

        // Cursor 2 (magenta dashed line)
        int cursor2_x = r->x + (int)(ui->cursor2_time * r->w);
        SDL_SetRenderDrawColor(renderer, 0xff, 0x00, 0xff, 0xff);
        for (int y = r->y; y < r->y + r->h; y += 4) {
            SDL_RenderDrawLine(renderer, cursor2_x, y, cursor2_x, MIN(y + 2, r->y + r->h));
        }
        // Label
        ui_draw_text(renderer, "C2", cursor2_x - 6, r->y + 2);

        // Show delta time and frequency between cursors
        if (sim && sim->time_step > 0) {
            double time_window = 10.0 * ui->scope_time_div;
            double t1 = ui->cursor1_time * time_window;
            double t2 = ui->cursor2_time * time_window;
            double dt = fabs(t2 - t1);
            double freq = dt > 0 ? 1.0 / dt : 0;

            // Display cursor measurements in a box at top-right
            int meas_x = r->x + r->w - 90;
            int meas_y = r->y + 5;

            SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xc0);
            SDL_Rect meas_bg = {meas_x - 5, meas_y - 2, 90, 45};
            SDL_RenderFillRect(renderer, &meas_bg);
            SDL_SetRenderDrawColor(renderer, 0x60, 0x60, 0x60, 0xff);
            SDL_RenderDrawRect(renderer, &meas_bg);

            SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
            if (dt < 0.001) {
                snprintf(buf, sizeof(buf), "dt:%.1fus", dt * 1e6);
            } else if (dt < 1.0) {
                snprintf(buf, sizeof(buf), "dt:%.2fms", dt * 1000);
            } else {
                snprintf(buf, sizeof(buf), "dt:%.3fs", dt);
            }
            ui_draw_text(renderer, buf, meas_x, meas_y);
            meas_y += 12;

            if (freq > 0) {
                if (freq >= 1000) {
                    snprintf(buf, sizeof(buf), "f:%.2fkHz", freq / 1000);
                } else {
                    snprintf(buf, sizeof(buf), "f:%.1fHz", freq);
                }
                ui_draw_text(renderer, buf, meas_x, meas_y);
                meas_y += 12;
            }

            snprintf(buf, sizeof(buf), "1/dt");
            ui_draw_text(renderer, buf, meas_x, meas_y);
        }
    }

    // Border (scope bezel)
    SDL_SetRenderDrawColor(renderer, 0x40, 0x40, 0x40, 0xff);
    SDL_RenderDrawRect(renderer, &bg);
    SDL_Rect outer = {r->x - 1, r->y - 1, r->w + 2, r->h + 2};
    SDL_RenderDrawRect(renderer, &outer);

    // Draw control buttons (first row: V+, V-, T+, T-)
    draw_button(renderer, &ui->btn_scope_volt_up);
    draw_button(renderer, &ui->btn_scope_volt_down);
    draw_button(renderer, &ui->btn_scope_time_up);
    draw_button(renderer, &ui->btn_scope_time_down);

    // Draw trigger control buttons (second row)
    draw_button(renderer, &ui->btn_scope_trig_mode);
    draw_button(renderer, &ui->btn_scope_trig_edge);
    draw_button(renderer, &ui->btn_scope_trig_ch);
    draw_button(renderer, &ui->btn_scope_trig_up);
    draw_button(renderer, &ui->btn_scope_trig_down);
    draw_button(renderer, &ui->btn_scope_mode);
    draw_button(renderer, &ui->btn_scope_screenshot);

    // Cursor button with toggle state indicator
    ui->btn_scope_cursor.toggled = ui->scope_cursor_mode;
    draw_button(renderer, &ui->btn_scope_cursor);

    // FFT button with toggle state indicator
    ui->btn_scope_fft.toggled = ui->scope_fft_mode;
    draw_button(renderer, &ui->btn_scope_fft);

    // Autoset button
    draw_button(renderer, &ui->btn_scope_autoset);

    draw_button(renderer, &ui->btn_bode);

    // Monte Carlo button with toggle state indicator
    ui->btn_mc.toggled = ui->show_monte_carlo_panel;
    draw_button(renderer, &ui->btn_mc);

    // Pop-out button with toggle state indicator
    ui->btn_scope_popup.toggled = ui->scope_popped_out;
    draw_button(renderer, &ui->btn_scope_popup);

    // Display settings panel below buttons (3 rows of buttons = ~79px, so start at +100)
    int info_y = r->y + r->h + 100;

    // Time/div with label
    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
    ui_draw_text(renderer, "TIME", r->x, info_y);
    SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x00, 0xff);
    format_time_value(buf, sizeof(buf), ui->scope_time_div);
    ui_draw_text(renderer, buf, r->x + 40, info_y);

    // Volts/div with label
    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
    ui_draw_text(renderer, "VOLTS", r->x + 110, info_y);
    SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x00, 0xff);
    format_volt_value(buf, sizeof(buf), ui->scope_volt_div);
    ui_draw_text(renderer, buf, r->x + 160, info_y);

    // Channel info with voltage readings
    info_y += 15;
    for (int ch = 0; ch < ui->scope_num_channels && ch < MAX_PROBES; ch++) {
        if (!ui->scope_channels[ch].enabled) continue;

        SDL_SetRenderDrawColor(renderer,
            ui->scope_channels[ch].color.r,
            ui->scope_channels[ch].color.g,
            ui->scope_channels[ch].color.b, 0xff);

        // Get current voltage from probe
        double voltage = 0;
        if (sim && sim->circuit && ch < sim->circuit->num_probes) {
            voltage = sim->circuit->probes[ch].voltage;
        }

        snprintf(buf, sizeof(buf), "CH%d:%.2fV", ch + 1, voltage);
        ui_draw_text(renderer, buf, r->x + ch * 80, info_y);
    }

    // Show "No probes" message if no channels active
    if (ui->scope_num_channels == 0) {
        SDL_SetRenderDrawColor(renderer, 0x40, 0x60, 0x40, 0xff);
        ui_draw_text(renderer, "Place probes to", r->x + 70, r->y + r->h/2 - 12);
        ui_draw_text(renderer, "see waveforms", r->x + 75, r->y + r->h/2 + 4);
    }

    // Display waveform measurements panel (below channel readings, relative to scope_rect)
    if (analysis && ui->scope_num_channels > 0) {
        // Start measurements below the channel readings (info_y + 15 for spacing)
        int meas_y = info_y + 18;
        int meas_x = r->x;
        int col_width = r->w / 2;  // Split into 2 columns if we have room

        // Measurements header
        SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
        ui_draw_text(renderer, "MEASUREMENTS", meas_x, meas_y);
        meas_y += 14;

        for (int ch = 0; ch < ui->scope_num_channels && ch < MAX_PROBES; ch++) {
            if (!ui->scope_channels[ch].enabled) continue;
            if (!analysis->measurements[ch].valid) continue;

            WaveformMeasurements *m = &analysis->measurements[ch];

            // Channel header with color
            SDL_SetRenderDrawColor(renderer,
                ui->scope_channels[ch].color.r,
                ui->scope_channels[ch].color.g,
                ui->scope_channels[ch].color.b, 0xff);
            snprintf(buf, sizeof(buf), "CH%d:", ch + 1);
            ui_draw_text(renderer, buf, meas_x, meas_y);

            // Measurements in gray, laid out in two columns
            SDL_SetRenderDrawColor(renderer, 0x90, 0x90, 0x90, 0xff);
            int col1_x = meas_x + 35;
            int col2_x = meas_x + col_width;
            int line_y = meas_y;

            // Column 1: Vpp, Vrms, Vavg
            if (m->v_pp < 1.0) {
                snprintf(buf, sizeof(buf), "Vpp:%.0fmV", m->v_pp * 1000);
            } else {
                snprintf(buf, sizeof(buf), "Vpp:%.2fV", m->v_pp);
            }
            ui_draw_text(renderer, buf, col1_x, line_y);
            line_y += 11;

            if (m->v_rms < 1.0) {
                snprintf(buf, sizeof(buf), "Vrms:%.0fmV", m->v_rms * 1000);
            } else {
                snprintf(buf, sizeof(buf), "Vrms:%.2fV", m->v_rms);
            }
            ui_draw_text(renderer, buf, col1_x, line_y);
            line_y += 11;

            if (fabs(m->v_avg) < 1.0) {
                snprintf(buf, sizeof(buf), "Vavg:%.0fmV", m->v_avg * 1000);
            } else {
                snprintf(buf, sizeof(buf), "Vavg:%.2fV", m->v_avg);
            }
            ui_draw_text(renderer, buf, col1_x, line_y);

            // Column 2: Freq, Period, Duty
            line_y = meas_y;
            if (m->frequency > 0) {
                if (m->frequency >= 1000) {
                    snprintf(buf, sizeof(buf), "f:%.1fkHz", m->frequency / 1000);
                } else {
                    snprintf(buf, sizeof(buf), "f:%.1fHz", m->frequency);
                }
                ui_draw_text(renderer, buf, col2_x, line_y);
                line_y += 11;

                if (m->period < 0.001) {
                    snprintf(buf, sizeof(buf), "T:%.1fus", m->period * 1e6);
                } else if (m->period < 1.0) {
                    snprintf(buf, sizeof(buf), "T:%.2fms", m->period * 1000);
                } else {
                    snprintf(buf, sizeof(buf), "T:%.2fs", m->period);
                }
                ui_draw_text(renderer, buf, col2_x, line_y);
                line_y += 11;

                snprintf(buf, sizeof(buf), "D:%.0f%%", m->duty_cycle);
                ui_draw_text(renderer, buf, col2_x, line_y);
            }

            meas_y += 38;  // Space for each channel's measurements (3 rows + padding)
        }
    }
}

void ui_render_bode_plot(UIState *ui, SDL_Renderer *renderer, Simulation *sim) {
    if (!ui || !renderer || !ui->show_bode_plot) return;

    Rect *r = &ui->bode_rect;
    char buf[64];

    // Semi-transparent background
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0x16, 0x21, 0x3e, 0xe0);
    SDL_Rect panel = {r->x - 10, r->y - 25, r->w + 20, r->h + 145};
    SDL_RenderFillRect(renderer, &panel);

    // Border
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    SDL_RenderDrawRect(renderer, &panel);

    // Title
    ui_draw_text(renderer, "Bode Plot - Frequency Response", r->x, r->y - 20);

    // Plot background (black)
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
    SDL_Rect plot_area = {r->x, r->y, r->w, r->h};
    SDL_RenderFillRect(renderer, &plot_area);

    // Grid
    SDL_SetRenderDrawColor(renderer, 0x20, 0x40, 0x20, 0xff);

    // Horizontal grid lines (magnitude)
    int num_h_lines = 6;  // -60 to 0 dB in 10dB steps
    for (int i = 0; i <= num_h_lines; i++) {
        int y_pos = r->y + (i * r->h) / num_h_lines;
        SDL_RenderDrawLine(renderer, r->x, y_pos, r->x + r->w, y_pos);
    }

    // Vertical grid lines (frequency decades)
    double log_start = log10(ui->bode_freq_start);
    double log_stop = log10(ui->bode_freq_stop);
    int num_decades = (int)(log_stop - log_start);
    for (int i = 0; i <= num_decades; i++) {
        int x_pos = r->x + (i * r->w) / num_decades;
        SDL_RenderDrawLine(renderer, x_pos, r->y, x_pos, r->y + r->h);
    }

    // 0 dB line (reference)
    SDL_SetRenderDrawColor(renderer, 0x60, 0x60, 0x60, 0xff);
    int zero_db_y = r->y + r->h / 2;  // 0 dB at middle
    SDL_RenderDrawLine(renderer, r->x, zero_db_y, r->x + r->w, zero_db_y);

    // Plot frequency response data
    if (sim && sim->freq_response_count > 1) {
        // Magnitude plot (yellow)
        SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0x00, 0xff);

        double db_min = -60, db_max = 20;  // dB range
        double db_range = db_max - db_min;

        for (int i = 1; i < sim->freq_response_count; i++) {
            FreqResponsePoint *p0 = &sim->freq_response[i - 1];
            FreqResponsePoint *p1 = &sim->freq_response[i];

            // Calculate x positions (log scale)
            double x0_norm = (log10(p0->frequency) - log_start) / (log_stop - log_start);
            double x1_norm = (log10(p1->frequency) - log_start) / (log_stop - log_start);
            int x0 = r->x + (int)(x0_norm * r->w);
            int x1 = r->x + (int)(x1_norm * r->w);

            // Calculate y positions (linear dB scale, inverted)
            double y0_norm = 1.0 - (p0->magnitude_db - db_min) / db_range;
            double y1_norm = 1.0 - (p1->magnitude_db - db_min) / db_range;
            y0_norm = fmax(0, fmin(1, y0_norm));
            y1_norm = fmax(0, fmin(1, y1_norm));
            int y0 = r->y + (int)(y0_norm * r->h);
            int y1 = r->y + (int)(y1_norm * r->h);

            SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
        }

        // Find and draw -3dB point
        // Find the MAXIMUM gain across all frequencies (works for both low-pass and high-pass)
        double max_gain_db = sim->freq_response[0].magnitude_db;
        int max_gain_idx = 0;
        for (int i = 1; i < sim->freq_response_count; i++) {
            if (sim->freq_response[i].magnitude_db > max_gain_db) {
                max_gain_db = sim->freq_response[i].magnitude_db;
                max_gain_idx = i;
            }
        }
        double target_db = max_gain_db - 3.0;  // -3dB from maximum gain
        double cutoff_freq = 0;
        bool found_cutoff = false;

        // Search for the -3dB crossover point
        // For low-pass filters: max is at low freq, cutoff is where gain drops below target
        // For high-pass filters: max is at high freq, cutoff is where gain rises above target
        for (int i = 1; i < sim->freq_response_count; i++) {
            FreqResponsePoint *p0 = &sim->freq_response[i - 1];
            FreqResponsePoint *p1 = &sim->freq_response[i];

            // Check if the -3dB level is crossed between these points
            if ((p0->magnitude_db >= target_db && p1->magnitude_db < target_db) ||
                (p0->magnitude_db <= target_db && p1->magnitude_db > target_db)) {
                // Linear interpolation to find exact crossing frequency
                double t = (target_db - p0->magnitude_db) / (p1->magnitude_db - p0->magnitude_db);
                // Log interpolation for frequency
                double log_f0 = log10(p0->frequency);
                double log_f1 = log10(p1->frequency);
                cutoff_freq = pow(10, log_f0 + t * (log_f1 - log_f0));
                found_cutoff = true;
                break;
            }
        }

        // Draw -3dB indicator if found
        if (found_cutoff && cutoff_freq > 0) {
            // Calculate positions
            double x_norm = (log10(cutoff_freq) - log_start) / (log_stop - log_start);
            int cutoff_x = r->x + (int)(x_norm * r->w);

            double y_norm = 1.0 - (target_db - db_min) / db_range;
            y_norm = fmax(0, fmin(1, y_norm));
            int cutoff_y = r->y + (int)(y_norm * r->h);

            // Draw horizontal dashed line at -3dB level (orange)
            SDL_SetRenderDrawColor(renderer, 0xff, 0x80, 0x00, 0xff);
            for (int x = r->x; x < cutoff_x; x += 6) {
                int x_end = x + 3;
                if (x_end > cutoff_x) x_end = cutoff_x;
                SDL_RenderDrawLine(renderer, x, cutoff_y, x_end, cutoff_y);
            }

            // Draw vertical dashed line at cutoff frequency
            for (int y = cutoff_y; y < r->y + r->h; y += 6) {
                int y_end = y + 3;
                if (y_end > r->y + r->h) y_end = r->y + r->h;
                SDL_RenderDrawLine(renderer, cutoff_x, y, cutoff_x, y_end);
            }

            // Draw a marker dot at the -3dB point
            SDL_Rect marker = {cutoff_x - 3, cutoff_y - 3, 6, 6};
            SDL_RenderFillRect(renderer, &marker);

            // Label the cutoff frequency
            char fc_buf[32];
            if (cutoff_freq >= 1000000) {
                snprintf(fc_buf, sizeof(fc_buf), "fc=%.2fMHz", cutoff_freq / 1000000);
            } else if (cutoff_freq >= 1000) {
                snprintf(fc_buf, sizeof(fc_buf), "fc=%.2fkHz", cutoff_freq / 1000);
            } else {
                snprintf(fc_buf, sizeof(fc_buf), "fc=%.1fHz", cutoff_freq);
            }

            // Position label near the marker
            int label_x = cutoff_x + 5;
            int label_y = cutoff_y - 10;
            if (label_x + 80 > r->x + r->w) label_x = cutoff_x - 75;
            if (label_y < r->y) label_y = cutoff_y + 5;

            ui_draw_text(renderer, fc_buf, label_x, label_y);

            // Also show -3dB label
            ui_draw_text(renderer, "-3dB", r->x + 5, cutoff_y - 10);
        }

        // Phase plot (cyan)
        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0xff, 0xff);

        for (int i = 1; i < sim->freq_response_count; i++) {
            FreqResponsePoint *p0 = &sim->freq_response[i - 1];
            FreqResponsePoint *p1 = &sim->freq_response[i];

            // Calculate x positions (log scale)
            double x0_norm = (log10(p0->frequency) - log_start) / (log_stop - log_start);
            double x1_norm = (log10(p1->frequency) - log_start) / (log_stop - log_start);
            int x0 = r->x + (int)(x0_norm * r->w);
            int x1 = r->x + (int)(x1_norm * r->w);

            // Calculate y positions (phase: -180 to +180 deg)
            double y0_norm = 0.5 - p0->phase_deg / 360.0;
            double y1_norm = 0.5 - p1->phase_deg / 360.0;
            y0_norm = fmax(0, fmin(1, y0_norm));
            y1_norm = fmax(0, fmin(1, y1_norm));
            int y0 = r->y + (int)(y0_norm * r->h);
            int y1 = r->y + (int)(y1_norm * r->h);

            SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
        }
    }

    // Labels
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);

    // Y-axis labels (magnitude)
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0x00, 0xff);
    ui_draw_text(renderer, "20dB", r->x - 35, r->y - 3);
    ui_draw_text(renderer, "0dB", r->x - 28, r->y + r->h/2 - 3);
    ui_draw_text(renderer, "-60dB", r->x - 40, r->y + r->h - 3);

    // Y-axis labels (phase)
    SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0xff, 0xff);
    ui_draw_text(renderer, "180", r->x + r->w + 5, r->y - 3);
    ui_draw_text(renderer, "0", r->x + r->w + 5, r->y + r->h/2 - 3);
    ui_draw_text(renderer, "-180", r->x + r->w + 5, r->y + r->h - 3);

    // X-axis labels (frequency)
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
    int label_y = r->y + r->h + 5;

    snprintf(buf, sizeof(buf), "%.0fHz", ui->bode_freq_start);
    ui_draw_text(renderer, buf, r->x - 10, label_y);

    double mid_freq = sqrt(ui->bode_freq_start * ui->bode_freq_stop);
    if (mid_freq >= 1000000000) {
        snprintf(buf, sizeof(buf), "%.1fGHz", mid_freq / 1000000000);
    } else if (mid_freq >= 1000000) {
        snprintf(buf, sizeof(buf), "%.1fMHz", mid_freq / 1000000);
    } else if (mid_freq >= 1000) {
        snprintf(buf, sizeof(buf), "%.1fkHz", mid_freq / 1000);
    } else {
        snprintf(buf, sizeof(buf), "%.0fHz", mid_freq);
    }
    ui_draw_text(renderer, buf, r->x + r->w/2 - 20, label_y);

    if (ui->bode_freq_stop >= 1000000000) {
        snprintf(buf, sizeof(buf), "%.0fGHz", ui->bode_freq_stop / 1000000000);
    } else if (ui->bode_freq_stop >= 1000000) {
        snprintf(buf, sizeof(buf), "%.0fMHz", ui->bode_freq_stop / 1000000);
    } else if (ui->bode_freq_stop >= 1000) {
        snprintf(buf, sizeof(buf), "%.0fkHz", ui->bode_freq_stop / 1000);
    } else {
        snprintf(buf, sizeof(buf), "%.0fHz", ui->bode_freq_stop);
    }
    ui_draw_text(renderer, buf, r->x + r->w - 30, label_y);

    // Legend
    int legend_y = label_y + 15;
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0x00, 0xff);
    ui_draw_text(renderer, "Magnitude (dB)", r->x, legend_y);
    SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0xff, 0xff);
    ui_draw_text(renderer, "Phase (deg)", r->x + 120, legend_y);

    // Status
    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
    if (sim && sim->freq_sweep_running) {
        ui_draw_text(renderer, "Running frequency sweep...", r->x, legend_y + 15);
    } else if (sim && sim->freq_sweep_complete) {
        snprintf(buf, sizeof(buf), "%d points measured", sim->freq_response_count);
        ui_draw_text(renderer, buf, r->x, legend_y + 15);
    } else {
        ui_draw_text(renderer, "Click Bode to run sweep", r->x, legend_y + 15);
    }

    // Settings controls
    int settings_y = legend_y + 35;

    // Start frequency
    SDL_SetRenderDrawColor(renderer, 0xc0, 0xc0, 0xc0, 0xff);
    ui_draw_text(renderer, "Start:", r->x, settings_y);
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    if (ui->bode_freq_start >= 1000) {
        snprintf(buf, sizeof(buf), "[%.1fkHz]", ui->bode_freq_start / 1000);
    } else {
        snprintf(buf, sizeof(buf), "[%.0fHz]", ui->bode_freq_start);
    }
    ui_draw_text(renderer, buf, r->x + 50, settings_y);

    // Stop frequency
    SDL_SetRenderDrawColor(renderer, 0xc0, 0xc0, 0xc0, 0xff);
    ui_draw_text(renderer, "Stop:", r->x + 130, settings_y);
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    if (ui->bode_freq_stop >= 1000000000) {
        snprintf(buf, sizeof(buf), "[%.1fGHz]", ui->bode_freq_stop / 1000000000);
    } else if (ui->bode_freq_stop >= 1000000) {
        snprintf(buf, sizeof(buf), "[%.0fMHz]", ui->bode_freq_stop / 1000000);
    } else if (ui->bode_freq_stop >= 1000) {
        snprintf(buf, sizeof(buf), "[%.0fkHz]", ui->bode_freq_stop / 1000);
    } else {
        snprintf(buf, sizeof(buf), "[%.0fHz]", ui->bode_freq_stop);
    }
    ui_draw_text(renderer, buf, r->x + 175, settings_y);

    // Number of points
    SDL_SetRenderDrawColor(renderer, 0xc0, 0xc0, 0xc0, 0xff);
    ui_draw_text(renderer, "Points:", r->x + 270, settings_y);
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    snprintf(buf, sizeof(buf), "[%d]", ui->bode_num_points);
    ui_draw_text(renderer, buf, r->x + 320, settings_y);

    // Recalculate button (next row)
    int recalc_y = settings_y + 18;
    ui->btn_bode_recalc.bounds = (Rect){r->x, recalc_y, 70, 20};

    // Draw recalculate button
    SDL_Rect recalc_rect = {ui->btn_bode_recalc.bounds.x, ui->btn_bode_recalc.bounds.y,
                            ui->btn_bode_recalc.bounds.w, ui->btn_bode_recalc.bounds.h};

    // Button background (cyan when hovered, darker when pressed)
    if (ui->btn_bode_recalc.pressed) {
        SDL_SetRenderDrawColor(renderer, 0x00, 0x60, 0x80, 0xff);
    } else if (ui->btn_bode_recalc.hovered) {
        SDL_SetRenderDrawColor(renderer, 0x00, 0xa0, 0xd0, 0xff);
    } else {
        SDL_SetRenderDrawColor(renderer, 0x00, 0x80, 0xb0, 0xff);
    }
    SDL_RenderFillRect(renderer, &recalc_rect);

    // Button border
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    SDL_RenderDrawRect(renderer, &recalc_rect);

    // Button text
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
    ui_draw_text(renderer, "Recalc", r->x + 10, recalc_y + 4);

    // Bode cursor toggle button
    SDL_SetRenderDrawColor(renderer, ui->bode_cursor_active ? 0x00 : 0x80, ui->bode_cursor_active ? 0xff : 0x80, ui->bode_cursor_active ? 0x00 : 0x80, 0xff);
    ui_draw_text(renderer, ui->bode_cursor_active ? "[Cursor ON]" : "[Cursor]", r->x + 80, recalc_y);

    // Draw cursor if active and we have data
    if (ui->bode_cursor_active && sim && sim->freq_response_count > 1) {
        // Initialize cursor to center if not set
        if (ui->bode_cursor_freq <= 0) {
            ui->bode_cursor_freq = sqrt(ui->bode_freq_start * ui->bode_freq_stop);
        }

        // Clamp cursor to valid range
        if (ui->bode_cursor_freq < ui->bode_freq_start) ui->bode_cursor_freq = ui->bode_freq_start;
        if (ui->bode_cursor_freq > ui->bode_freq_stop) ui->bode_cursor_freq = ui->bode_freq_stop;

        // Calculate cursor x position (log scale)
        double log_cursor = log10(ui->bode_cursor_freq);
        double log_start = log10(ui->bode_freq_start);
        double log_stop = log10(ui->bode_freq_stop);
        double x_norm = (log_cursor - log_start) / (log_stop - log_start);
        int cursor_x = r->x + (int)(x_norm * r->w);

        // Draw vertical cursor line (green, dashed)
        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x00, 0xff);
        for (int y = r->y; y < r->y + r->h; y += 4) {
            int y_end = y + 2;
            if (y_end > r->y + r->h) y_end = r->y + r->h;
            SDL_RenderDrawLine(renderer, cursor_x, y, cursor_x, y_end);
        }

        // Interpolate magnitude and phase at cursor frequency
        double cursor_magnitude = 0;
        double cursor_phase = 0;

        for (int i = 1; i < sim->freq_response_count; i++) {
            FreqResponsePoint *p0 = &sim->freq_response[i - 1];
            FreqResponsePoint *p1 = &sim->freq_response[i];

            if (p0->frequency <= ui->bode_cursor_freq && p1->frequency >= ui->bode_cursor_freq) {
                // Log interpolation for frequency
                double log_f0 = log10(p0->frequency);
                double log_f1 = log10(p1->frequency);
                double t = (log_cursor - log_f0) / (log_f1 - log_f0);

                // Linear interpolation for magnitude and phase
                cursor_magnitude = p0->magnitude_db + t * (p1->magnitude_db - p0->magnitude_db);
                cursor_phase = p0->phase_deg + t * (p1->phase_deg - p0->phase_deg);
                break;
            }
        }

        // Store values for display
        ui->bode_cursor_magnitude = cursor_magnitude;
        ui->bode_cursor_phase = cursor_phase;

        // Draw magnitude marker (yellow dot)
        double db_min = -60.0;
        double db_max = 20.0;
        double db_range = db_max - db_min;
        double y_mag_norm = 1.0 - (cursor_magnitude - db_min) / db_range;
        y_mag_norm = fmax(0, fmin(1, y_mag_norm));
        int mag_y = r->y + (int)(y_mag_norm * r->h);

        SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0x00, 0xff);
        SDL_Rect mag_marker = {cursor_x - 4, mag_y - 4, 8, 8};
        SDL_RenderFillRect(renderer, &mag_marker);

        // Draw phase marker (cyan dot)
        double y_phase_norm = 0.5 - cursor_phase / 360.0;
        y_phase_norm = fmax(0, fmin(1, y_phase_norm));
        int phase_y = r->y + (int)(y_phase_norm * r->h);

        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0xff, 0xff);
        SDL_Rect phase_marker = {cursor_x - 4, phase_y - 4, 8, 8};
        SDL_RenderFillRect(renderer, &phase_marker);

        // Draw cursor info box
        int info_x = cursor_x + 10;
        int info_y = r->y + 10;
        // Flip to left side if too close to right edge
        if (info_x + 120 > r->x + r->w) {
            info_x = cursor_x - 130;
        }

        // Info box background
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0x10, 0x10, 0x30, 0xe0);
        SDL_Rect info_box = {info_x, info_y, 120, 55};
        SDL_RenderFillRect(renderer, &info_box);
        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x00, 0xff);
        SDL_RenderDrawRect(renderer, &info_box);

        // Cursor frequency
        char cursor_buf[48];
        if (ui->bode_cursor_freq >= 1000000000) {
            snprintf(cursor_buf, sizeof(cursor_buf), "f: %.3f GHz", ui->bode_cursor_freq / 1000000000);
        } else if (ui->bode_cursor_freq >= 1000000) {
            snprintf(cursor_buf, sizeof(cursor_buf), "f: %.3f MHz", ui->bode_cursor_freq / 1000000);
        } else if (ui->bode_cursor_freq >= 1000) {
            snprintf(cursor_buf, sizeof(cursor_buf), "f: %.3f kHz", ui->bode_cursor_freq / 1000);
        } else {
            snprintf(cursor_buf, sizeof(cursor_buf), "f: %.1f Hz", ui->bode_cursor_freq);
        }
        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x00, 0xff);
        ui_draw_text(renderer, cursor_buf, info_x + 5, info_y + 5);

        // Magnitude
        snprintf(cursor_buf, sizeof(cursor_buf), "Mag: %.2f dB", cursor_magnitude);
        SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0x00, 0xff);
        ui_draw_text(renderer, cursor_buf, info_x + 5, info_y + 20);

        // Phase
        snprintf(cursor_buf, sizeof(cursor_buf), "Phase: %.1f deg", cursor_phase);
        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0xff, 0xff);
        ui_draw_text(renderer, cursor_buf, info_x + 5, info_y + 35);

        // Draw drag hint
        SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
        ui_draw_text(renderer, "(drag to move)", info_x + 5, info_y + 48);
    }

    // Close button hint
    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
    ui_draw_text(renderer, "[ESC to close]", r->x + r->w - 100, recalc_y);
}

void ui_render_sweep_panel(UIState *ui, SDL_Renderer *renderer, void *analysis_ptr) {
    if (!ui || !renderer || !ui->show_sweep_panel) return;

    AnalysisState *analysis = (AnalysisState *)analysis_ptr;
    char buf[64];

    // Panel dimensions
    int panel_w = 350;
    int panel_h = 280;
    int panel_x = (ui->window_width - panel_w) / 2;
    int panel_y = (ui->window_height - panel_h) / 2;

    // Semi-transparent background
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0x16, 0x21, 0x3e, 0xf0);
    SDL_Rect panel = {panel_x, panel_y, panel_w, panel_h};
    SDL_RenderFillRect(renderer, &panel);

    // Border
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    SDL_RenderDrawRect(renderer, &panel);

    // Title
    ui_draw_text(renderer, "PARAMETRIC SWEEP ANALYSIS", panel_x + 10, panel_y + 10);

    int y = panel_y + 35;
    int label_x = panel_x + 15;
    int value_x = panel_x + 150;

    SDL_SetRenderDrawColor(renderer, 0x90, 0x90, 0x90, 0xff);

    // Component selection
    ui_draw_text(renderer, "Component:", label_x, y);
    if (ui->sweep_component_idx >= 0) {
        snprintf(buf, sizeof(buf), "#%d", ui->sweep_component_idx);
    } else {
        snprintf(buf, sizeof(buf), "[Select]");
    }
    ui_draw_text(renderer, buf, value_x, y);
    y += 20;

    // Parameter type
    ui_draw_text(renderer, "Parameter:", label_x, y);
    static const char *param_names[] = {"Value", "Frequency", "Phase", "Offset", "Duty"};
    if (ui->sweep_param_type < 5) {
        ui_draw_text(renderer, param_names[ui->sweep_param_type], value_x, y);
    }
    y += 20;

    // Start value
    ui_draw_text(renderer, "Start:", label_x, y);
    snprintf(buf, sizeof(buf), "%.3g", ui->sweep_start);
    ui_draw_text(renderer, buf, value_x, y);
    y += 20;

    // End value
    ui_draw_text(renderer, "End:", label_x, y);
    snprintf(buf, sizeof(buf), "%.3g", ui->sweep_end);
    ui_draw_text(renderer, buf, value_x, y);
    y += 20;

    // Number of points
    ui_draw_text(renderer, "Points:", label_x, y);
    snprintf(buf, sizeof(buf), "%d", ui->sweep_num_points);
    ui_draw_text(renderer, buf, value_x, y);
    y += 20;

    // Scale type
    ui_draw_text(renderer, "Scale:", label_x, y);
    ui_draw_text(renderer, ui->sweep_log_scale ? "Logarithmic" : "Linear", value_x, y);
    y += 25;

    // Sweep progress
    if (analysis && analysis->sweep.active) {
        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x00, 0xff);
        snprintf(buf, sizeof(buf), "Progress: %d/%d",
                 analysis->sweep.current_point, analysis->sweep.num_points);
        ui_draw_text(renderer, buf, label_x, y);
        y += 15;

        // Progress bar
        int bar_w = panel_w - 40;
        double progress = (double)analysis->sweep.current_point / analysis->sweep.num_points;
        SDL_Rect bar_bg = {label_x, y, bar_w, 10};
        SDL_SetRenderDrawColor(renderer, 0x30, 0x30, 0x30, 0xff);
        SDL_RenderFillRect(renderer, &bar_bg);
        SDL_Rect bar_fill = {label_x, y, (int)(bar_w * progress), 10};
        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x00, 0xff);
        SDL_RenderFillRect(renderer, &bar_fill);
    } else {
        SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0x00, 0xff);
        ui_draw_text(renderer, "[Select component to sweep]", label_x, y);
    }

    // Instructions
    y = panel_y + panel_h - 25;
    SDL_SetRenderDrawColor(renderer, 0x60, 0x60, 0x60, 0xff);
    ui_draw_text(renderer, "Click component, then ESC to close", label_x, y);
}

// Static button bounds for Monte Carlo panel (for click handling)
static Rect mc_btn_run, mc_btn_reset, mc_btn_runs_up, mc_btn_runs_down, mc_btn_tol_up, mc_btn_tol_down;
static Rect mc_panel_rect;

void ui_render_monte_carlo_panel(UIState *ui, SDL_Renderer *renderer, void *analysis_ptr) {
    if (!ui || !renderer || !ui->show_monte_carlo_panel) return;

    AnalysisState *analysis = (AnalysisState *)analysis_ptr;
    char buf[64];

    // Panel dimensions
    int panel_w = 350;
    int panel_h = 340;
    int panel_x = (ui->window_width - panel_w) / 2;
    int panel_y = (ui->window_height - panel_h) / 2;

    // Store panel rect for click handling
    mc_panel_rect = (Rect){panel_x, panel_y, panel_w, panel_h};

    // Semi-transparent background
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0x16, 0x21, 0x3e, 0xf0);
    SDL_Rect panel = {panel_x, panel_y, panel_w, panel_h};
    SDL_RenderFillRect(renderer, &panel);

    // Border
    SDL_SetRenderDrawColor(renderer, 0xff, 0x80, 0x00, 0xff);  // Orange for MC
    SDL_RenderDrawRect(renderer, &panel);

    // Title
    ui_draw_text(renderer, "MONTE CARLO ANALYSIS", panel_x + 10, panel_y + 10);

    int y = panel_y + 35;
    int label_x = panel_x + 15;
    int value_x = panel_x + 130;
    int btn_size = 18;

    SDL_SetRenderDrawColor(renderer, 0xc0, 0xc0, 0xc0, 0xff);

    // Number of runs with +/- buttons
    ui_draw_text(renderer, "Runs:", label_x, y);
    snprintf(buf, sizeof(buf), "%d", ui->monte_carlo_runs);
    ui_draw_text(renderer, buf, value_x, y);

    // +/- buttons for runs
    mc_btn_runs_down = (Rect){value_x + 50, y - 2, btn_size, btn_size};
    mc_btn_runs_up = (Rect){value_x + 75, y - 2, btn_size, btn_size};
    SDL_SetRenderDrawColor(renderer, 0x40, 0x40, 0x60, 0xff);
    SDL_Rect r1 = {mc_btn_runs_down.x, mc_btn_runs_down.y, mc_btn_runs_down.w, mc_btn_runs_down.h};
    SDL_Rect r2 = {mc_btn_runs_up.x, mc_btn_runs_up.y, mc_btn_runs_up.w, mc_btn_runs_up.h};
    SDL_RenderFillRect(renderer, &r1);
    SDL_RenderFillRect(renderer, &r2);
    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
    SDL_RenderDrawRect(renderer, &r1);
    SDL_RenderDrawRect(renderer, &r2);
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
    ui_draw_text(renderer, "-", mc_btn_runs_down.x + 5, mc_btn_runs_down.y + 2);
    ui_draw_text(renderer, "+", mc_btn_runs_up.x + 4, mc_btn_runs_up.y + 2);
    y += 22;

    // Tolerance with +/- buttons
    SDL_SetRenderDrawColor(renderer, 0xc0, 0xc0, 0xc0, 0xff);
    ui_draw_text(renderer, "Tolerance:", label_x, y);
    snprintf(buf, sizeof(buf), "%.0f%%", ui->monte_carlo_tolerance);
    ui_draw_text(renderer, buf, value_x, y);

    // +/- buttons for tolerance
    mc_btn_tol_down = (Rect){value_x + 50, y - 2, btn_size, btn_size};
    mc_btn_tol_up = (Rect){value_x + 75, y - 2, btn_size, btn_size};
    SDL_SetRenderDrawColor(renderer, 0x40, 0x40, 0x60, 0xff);
    SDL_Rect r3 = {mc_btn_tol_down.x, mc_btn_tol_down.y, mc_btn_tol_down.w, mc_btn_tol_down.h};
    SDL_Rect r4 = {mc_btn_tol_up.x, mc_btn_tol_up.y, mc_btn_tol_up.w, mc_btn_tol_up.h};
    SDL_RenderFillRect(renderer, &r3);
    SDL_RenderFillRect(renderer, &r4);
    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
    SDL_RenderDrawRect(renderer, &r3);
    SDL_RenderDrawRect(renderer, &r4);
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
    ui_draw_text(renderer, "-", mc_btn_tol_down.x + 5, mc_btn_tol_down.y + 2);
    ui_draw_text(renderer, "+", mc_btn_tol_up.x + 4, mc_btn_tol_up.y + 2);
    y += 25;

    // Run and Reset buttons
    int btn_w = 70;
    int btn_h = 24;
    mc_btn_run = (Rect){label_x, y, btn_w, btn_h};
    mc_btn_reset = (Rect){label_x + btn_w + 10, y, btn_w, btn_h};

    // Run button - green if not running, disabled if running
    bool is_running = analysis && analysis->monte_carlo.active && !analysis->monte_carlo.complete;
    if (is_running) {
        SDL_SetRenderDrawColor(renderer, 0x30, 0x30, 0x30, 0xff);
    } else {
        SDL_SetRenderDrawColor(renderer, 0x00, 0x80, 0x00, 0xff);
    }
    SDL_Rect run_rect = {mc_btn_run.x, mc_btn_run.y, mc_btn_run.w, mc_btn_run.h};
    SDL_RenderFillRect(renderer, &run_rect);
    SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x00, 0xff);
    SDL_RenderDrawRect(renderer, &run_rect);
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
    ui_draw_text(renderer, is_running ? "Running" : "RUN", mc_btn_run.x + 15, mc_btn_run.y + 6);

    // Reset button - orange
    SDL_SetRenderDrawColor(renderer, 0x80, 0x40, 0x00, 0xff);
    SDL_Rect reset_rect = {mc_btn_reset.x, mc_btn_reset.y, mc_btn_reset.w, mc_btn_reset.h};
    SDL_RenderFillRect(renderer, &reset_rect);
    SDL_SetRenderDrawColor(renderer, 0xff, 0x80, 0x00, 0xff);
    SDL_RenderDrawRect(renderer, &reset_rect);
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
    ui_draw_text(renderer, "RESET", mc_btn_reset.x + 12, mc_btn_reset.y + 6);
    y += 35;

    // Results
    if (analysis && analysis->monte_carlo.complete) {
        MonteCarloAnalysis *mc = &analysis->monte_carlo;

        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x00, 0xff);
        ui_draw_text(renderer, "RESULTS:", label_x, y);
        y += 18;

        SDL_SetRenderDrawColor(renderer, 0xc0, 0xc0, 0xc0, 0xff);

        snprintf(buf, sizeof(buf), "Mean: %.4g", mc->mean);
        ui_draw_text(renderer, buf, label_x, y);
        y += 14;

        snprintf(buf, sizeof(buf), "Std Dev: %.4g", mc->std_dev);
        ui_draw_text(renderer, buf, label_x, y);
        y += 14;

        snprintf(buf, sizeof(buf), "Min: %.4g", mc->min_val);
        ui_draw_text(renderer, buf, label_x, y);
        y += 14;

        snprintf(buf, sizeof(buf), "Max: %.4g", mc->max_val);
        ui_draw_text(renderer, buf, label_x, y);
        y += 14;

        snprintf(buf, sizeof(buf), "1%% Worst: %.4g", mc->percentile_1);
        ui_draw_text(renderer, buf, label_x, y);
        y += 14;

        snprintf(buf, sizeof(buf), "99%% Worst: %.4g", mc->percentile_99);
        ui_draw_text(renderer, buf, label_x, y);
        y += 20;

        // Draw histogram
        SDL_SetRenderDrawColor(renderer, 0x40, 0x40, 0x60, 0xff);
        int hist_x = label_x;
        int hist_y = y;
        int hist_w = panel_w - 40;
        int hist_h = 50;
        SDL_Rect hist_bg = {hist_x, hist_y, hist_w, hist_h};
        SDL_RenderFillRect(renderer, &hist_bg);

        // Simple histogram bins
        int num_bins = 20;
        int bin_counts[20] = {0};
        double bin_width = (mc->max_val - mc->min_val) / num_bins;
        if (bin_width > 0) {
            for (int i = 0; i < mc->num_results; i++) {
                int bin = (int)((mc->output_values[i] - mc->min_val) / bin_width);
                if (bin >= num_bins) bin = num_bins - 1;
                if (bin >= 0 && bin < num_bins) bin_counts[bin]++;
            }

            int max_count = 1;
            for (int i = 0; i < num_bins; i++) {
                if (bin_counts[i] > max_count) max_count = bin_counts[i];
            }

            SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
            for (int i = 0; i < num_bins; i++) {
                int bar_h = (bin_counts[i] * (hist_h - 2)) / max_count;
                int bar_x = hist_x + (i * hist_w) / num_bins;
                int bar_w = hist_w / num_bins - 1;
                SDL_Rect bar = {bar_x, hist_y + hist_h - bar_h, bar_w, bar_h};
                SDL_RenderFillRect(renderer, &bar);
            }
        }

    } else if (analysis && analysis->monte_carlo.active) {
        // Show progress
        MonteCarloAnalysis *mc = &analysis->monte_carlo;
        SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0x00, 0xff);
        snprintf(buf, sizeof(buf), "Running: %d/%d", mc->current_run, mc->num_runs);
        ui_draw_text(renderer, buf, label_x, y);
        y += 15;

        // Progress bar
        int bar_w = panel_w - 40;
        double progress = (double)mc->current_run / mc->num_runs;
        SDL_Rect bar_bg = {label_x, y, bar_w, 10};
        SDL_SetRenderDrawColor(renderer, 0x30, 0x30, 0x30, 0xff);
        SDL_RenderFillRect(renderer, &bar_bg);
        SDL_Rect bar_fill = {label_x, y, (int)(bar_w * progress), 10};
        SDL_SetRenderDrawColor(renderer, 0xff, 0x80, 0x00, 0xff);
        SDL_RenderFillRect(renderer, &bar_fill);

    } else {
        SDL_SetRenderDrawColor(renderer, 0x60, 0x60, 0x60, 0xff);
        ui_draw_text(renderer, "Configure and run simulation", label_x, y);
    }

    // Instructions
    y = panel_y + panel_h - 25;
    SDL_SetRenderDrawColor(renderer, 0x60, 0x60, 0x60, 0xff);
    ui_draw_text(renderer, "ESC to close", label_x, y);
}

void ui_render_statusbar(UIState *ui, SDL_Renderer *renderer) {
    int y = ui->window_height - STATUSBAR_HEIGHT;

    // Background - synthwave dark purple
    SDL_SetRenderDrawColor(renderer, SYNTH_BG_MID, 0xff);
    SDL_Rect bar = {0, y, ui->window_width, STATUSBAR_HEIGHT};
    SDL_RenderFillRect(renderer, &bar);

    // Status message
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DIM, 0xff);
    if (ui->status_message[0]) {
        ui_draw_text(renderer, ui->status_message, 10, y + 8);
    } else {
        ui_draw_text(renderer, "Ready - Press F1 for help", 10, y + 8);
    }

    // Adaptive time-stepping indicator
    if (ui->adaptive_enabled) {
        char dt_str[32];
        // Show factor relative to 1.0 (target)
        if (ui->adaptive_factor < 0.9) {
            // Struggling - orange/red for slow stepping
            snprintf(dt_str, sizeof(dt_str), "dt:%.1fx", ui->adaptive_factor);
            if (ui->step_rejections > 0) {
                SDL_SetRenderDrawColor(renderer, 0xff, 0x40, 0x40, 0xff);  // Red - step rejections
            } else {
                SDL_SetRenderDrawColor(renderer, SYNTH_ORANGE, 0xff);  // Orange - slowed down
            }
        } else if (ui->adaptive_factor > 1.5) {
            // Fast - green for speeded up
            snprintf(dt_str, sizeof(dt_str), "dt:%.1fx", ui->adaptive_factor);
            SDL_SetRenderDrawColor(renderer, SYNTH_GREEN, 0xff);
        } else {
            // Normal - cyan
            snprintf(dt_str, sizeof(dt_str), "dt:%.1fx", ui->adaptive_factor);
            SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0xff);
        }
        ui_draw_text(renderer, dt_str, ui->window_width - 350, y + 8);
    }

    // Time display - synthwave cyan
    char time_str[32];
    snprintf(time_str, sizeof(time_str), "t=%.3fs", ui->sim_time);
    SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0xff);
    ui_draw_text(renderer, time_str, ui->window_width - 250, y + 8);

    // Component/Node counts
    char count_str[32];
    snprintf(count_str, sizeof(count_str), "C:%d N:%d", ui->component_count, ui->node_count);
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DIM, 0xff);
    ui_draw_text(renderer, count_str, ui->window_width - 120, y + 8);

    // Environment sliders (Light and Temperature) - positioned center-left of status bar
    // Calculate right boundary of sliders area (leave room for right-side displays)
    int right_boundary = ui->window_width - 370;  // dt/time/count start at -350, add margin
    int env_x = 240;  // Start position for environment sliders
    int slider_y = y + 5;
    int slider_w = 70;
    int slider_h = 14;
    int text_w = 28;

    // Calculate how much space the sliders need
    // Lux: text_w + slider_w + value_text (~35) = ~133
    // Gap between sliders: 45
    // Temp: text_w + slider_w + value_text (~35) = ~133
    // Total: ~311 pixels
    int sliders_total_width = 311;

    // Only show sliders if there's enough room
    if (env_x + sliders_total_width > right_boundary) {
        // Not enough room - skip sliders or adjust
        // Hide sliders and just init bounds to offscreen
        ui->env_light_slider = (Rect){-100, -100, 0, 0};
        ui->env_temp_slider = (Rect){-100, -100, 0, 0};
    } else {

    // Light slider (for LDR components)
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DIM, 0xff);
    ui_draw_text(renderer, "Lux:", env_x, y + 8);

    // Update slider bounds for click detection
    ui->env_light_slider = (Rect){env_x + text_w, slider_y, slider_w, slider_h};

    // Light slider background
    SDL_SetRenderDrawColor(renderer, SYNTH_BG_DARK, 0xff);
    SDL_Rect light_bg = {env_x + text_w, slider_y, slider_w, slider_h};
    SDL_RenderFillRect(renderer, &light_bg);
    SDL_SetRenderDrawColor(renderer, SYNTH_YELLOW, 0x60);
    SDL_RenderDrawRect(renderer, &light_bg);

    // Light slider fill (0-100%)
    int light_fill = (int)(slider_w * g_environment.light_level);
    light_fill = CLAMP(light_fill, 0, slider_w);
    SDL_SetRenderDrawColor(renderer, SYNTH_YELLOW, 0xff);
    SDL_Rect light_fill_rect = {env_x + text_w, slider_y, light_fill, slider_h};
    SDL_RenderFillRect(renderer, &light_fill_rect);

    // Light value text
    char light_text[16];
    snprintf(light_text, sizeof(light_text), "%d%%", (int)(g_environment.light_level * 100));
    SDL_SetRenderDrawColor(renderer, SYNTH_YELLOW, 0xff);
    ui_draw_text(renderer, light_text, env_x + text_w + slider_w + 4, y + 8);

    // Temperature slider (for Thermistor components)
    int temp_x = env_x + text_w + slider_w + 45;
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DIM, 0xff);
    ui_draw_text(renderer, "Tmp:", temp_x, y + 8);

    // Update slider bounds for click detection
    ui->env_temp_slider = (Rect){temp_x + text_w, slider_y, slider_w, slider_h};

    // Temperature slider background
    SDL_SetRenderDrawColor(renderer, SYNTH_BG_DARK, 0xff);
    SDL_Rect temp_bg = {temp_x + text_w, slider_y, slider_w, slider_h};
    SDL_RenderFillRect(renderer, &temp_bg);
    SDL_SetRenderDrawColor(renderer, SYNTH_ORANGE, 0x60);
    SDL_RenderDrawRect(renderer, &temp_bg);

    // Temperature slider fill (map -40°C to 125°C to 0-1)
    // Normalize: (temp - min) / (max - min)
    double temp_min = -40.0, temp_max = 125.0;
    double temp_norm = (g_environment.temperature - temp_min) / (temp_max - temp_min);
    temp_norm = CLAMP(temp_norm, 0.0, 1.0);
    int temp_fill = (int)(slider_w * temp_norm);
    SDL_SetRenderDrawColor(renderer, SYNTH_ORANGE, 0xff);
    SDL_Rect temp_fill_rect = {temp_x + text_w, slider_y, temp_fill, slider_h};
    SDL_RenderFillRect(renderer, &temp_fill_rect);

    // Temperature value text
    char temp_text[16];
    snprintf(temp_text, sizeof(temp_text), "%.0fC", g_environment.temperature);
    SDL_SetRenderDrawColor(renderer, SYNTH_ORANGE, 0xff);
    ui_draw_text(renderer, temp_text, temp_x + text_w + slider_w + 4, y + 8);
    }  // End of else block (sliders have room)
}

void ui_render_shortcuts_dialog(UIState *ui, SDL_Renderer *renderer) {
    if (!ui->show_shortcuts_dialog) return;

    // Semi-transparent overlay
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xb0);
    SDL_Rect overlay = {0, 0, ui->window_width, ui->window_height};
    SDL_RenderFillRect(renderer, &overlay);

    // Dialog box - synthwave dark with pink border
    int dw = 350, dh = 320;
    int dx = (ui->window_width - dw) / 2;
    int dy = (ui->window_height - dh) / 2;

    SDL_SetRenderDrawColor(renderer, SYNTH_BG_MID, 0xff);
    SDL_Rect dialog = {dx, dy, dw, dh};
    SDL_RenderFillRect(renderer, &dialog);

    SDL_SetRenderDrawColor(renderer, SYNTH_PINK, 0xff);
    SDL_RenderDrawRect(renderer, &dialog);

    // Title - pink
    ui_draw_text(renderer, "Keyboard Shortcuts", dx + 20, dy + 15);

    // Shortcuts list
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DIM, 0xff);
    int line_y = dy + 45;
    int line_h = 18;

    ui_draw_text(renderer, "Escape    - Cancel/Deselect", dx + 20, line_y); line_y += line_h;
    ui_draw_text(renderer, "Delete    - Delete selected", dx + 20, line_y); line_y += line_h;
    ui_draw_text(renderer, "R         - Rotate component", dx + 20, line_y); line_y += line_h;
    ui_draw_text(renderer, "Ctrl+Z    - Undo", dx + 20, line_y); line_y += line_h;
    ui_draw_text(renderer, "Ctrl+C    - Copy", dx + 20, line_y); line_y += line_h;
    ui_draw_text(renderer, "Ctrl+X    - Cut", dx + 20, line_y); line_y += line_h;
    ui_draw_text(renderer, "Ctrl+V    - Paste", dx + 20, line_y); line_y += line_h;
    ui_draw_text(renderer, "Ctrl+D    - Duplicate", dx + 20, line_y); line_y += line_h;
    ui_draw_text(renderer, "Space     - Run/Pause sim", dx + 20, line_y); line_y += line_h;
    ui_draw_text(renderer, "G         - Place ground", dx + 20, line_y); line_y += line_h;
    ui_draw_text(renderer, "I         - Toggle current", dx + 20, line_y); line_y += line_h;
    ui_draw_text(renderer, "Scroll    - Zoom in/out", dx + 20, line_y); line_y += line_h;
    ui_draw_text(renderer, "Mid-drag  - Pan view", dx + 20, line_y); line_y += line_h;

    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DARK, 0xff);
    line_y += 10;
    ui_draw_text(renderer, "Press Escape or F1 to close", dx + 20, line_y);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// Helper: case-insensitive substring search
static bool str_contains_ci(const char *haystack, const char *needle) {
    if (!haystack || !needle || needle[0] == '\0') return true;
    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);
    if (nlen > hlen) return false;

    for (size_t i = 0; i <= hlen - nlen; i++) {
        bool match = true;
        for (size_t j = 0; j < nlen; j++) {
            char h = haystack[i + j];
            char n = needle[j];
            // Convert to lowercase
            if (h >= 'A' && h <= 'Z') h += 32;
            if (n >= 'A' && n <= 'Z') n += 32;
            if (h != n) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

// Update spotlight search results based on query
static void spotlight_update_results(UIState *ui) {
    ui->spotlight_num_results = 0;
    if (ui->spotlight_query[0] == '\0') {
        // Empty query - show popular components
        ui->spotlight_results[ui->spotlight_num_results++] = COMP_RESISTOR;
        ui->spotlight_results[ui->spotlight_num_results++] = COMP_CAPACITOR;
        ui->spotlight_results[ui->spotlight_num_results++] = COMP_INDUCTOR;
        ui->spotlight_results[ui->spotlight_num_results++] = COMP_DIODE;
        ui->spotlight_results[ui->spotlight_num_results++] = COMP_DC_VOLTAGE;
        ui->spotlight_results[ui->spotlight_num_results++] = COMP_AC_VOLTAGE;
        ui->spotlight_results[ui->spotlight_num_results++] = COMP_GROUND;
        ui->spotlight_results[ui->spotlight_num_results++] = COMP_NPN_BJT;
        ui->spotlight_results[ui->spotlight_num_results++] = COMP_OPAMP;
        ui->spotlight_results[ui->spotlight_num_results++] = COMP_LED;
        return;
    }

    // Search through all component types
    for (int i = 1; i < COMP_TYPE_COUNT && ui->spotlight_num_results < 32; i++) {
        const ComponentTypeInfo *info = component_get_info(i);
        if (info && info->name) {
            if (str_contains_ci(info->name, ui->spotlight_query) ||
                str_contains_ci(info->short_name, ui->spotlight_query)) {
                ui->spotlight_results[ui->spotlight_num_results++] = (ComponentType)i;
            }
        }
    }

    // Clamp selected index
    if (ui->spotlight_selected >= ui->spotlight_num_results) {
        ui->spotlight_selected = ui->spotlight_num_results > 0 ? ui->spotlight_num_results - 1 : 0;
    }
}

// Open spotlight search dialog
void ui_spotlight_open(UIState *ui) {
    ui->show_spotlight = true;
    ui->spotlight_query[0] = '\0';
    ui->spotlight_cursor = 0;
    ui->spotlight_selected = 0;
    spotlight_update_results(ui);
    SDL_StartTextInput();
}

// Close spotlight search dialog
void ui_spotlight_close(UIState *ui) {
    ui->show_spotlight = false;
    SDL_StopTextInput();
}

// Handle spotlight text input
void ui_spotlight_text_input(UIState *ui, const char *text) {
    if (!ui->show_spotlight) return;

    int len = (int)strlen(ui->spotlight_query);
    int text_len = (int)strlen(text);

    if (len + text_len < 63) {
        // Insert text at cursor position
        memmove(&ui->spotlight_query[ui->spotlight_cursor + text_len],
                &ui->spotlight_query[ui->spotlight_cursor],
                len - ui->spotlight_cursor + 1);
        memcpy(&ui->spotlight_query[ui->spotlight_cursor], text, text_len);
        ui->spotlight_cursor += text_len;
        spotlight_update_results(ui);
        ui->spotlight_selected = 0;  // Reset selection on new search
    }
}

// Handle spotlight key input
// Returns: component type to place, or COMP_NONE if no selection
ComponentType ui_spotlight_key(UIState *ui, SDL_Keycode key) {
    if (!ui->show_spotlight) return COMP_NONE;

    int len = (int)strlen(ui->spotlight_query);

    switch (key) {
        case SDLK_ESCAPE:
            ui_spotlight_close(ui);
            return COMP_NONE;

        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (ui->spotlight_num_results > 0) {
                ComponentType result = ui->spotlight_results[ui->spotlight_selected];
                ui_spotlight_close(ui);
                return result;
            }
            return COMP_NONE;

        case SDLK_UP:
            if (ui->spotlight_selected > 0) {
                ui->spotlight_selected--;
            }
            return COMP_NONE;

        case SDLK_DOWN:
            if (ui->spotlight_selected < ui->spotlight_num_results - 1) {
                ui->spotlight_selected++;
            }
            return COMP_NONE;

        case SDLK_BACKSPACE:
            if (ui->spotlight_cursor > 0) {
                memmove(&ui->spotlight_query[ui->spotlight_cursor - 1],
                        &ui->spotlight_query[ui->spotlight_cursor],
                        len - ui->spotlight_cursor + 1);
                ui->spotlight_cursor--;
                spotlight_update_results(ui);
            }
            return COMP_NONE;

        case SDLK_DELETE:
            if (ui->spotlight_cursor < len) {
                memmove(&ui->spotlight_query[ui->spotlight_cursor],
                        &ui->spotlight_query[ui->spotlight_cursor + 1],
                        len - ui->spotlight_cursor);
                spotlight_update_results(ui);
            }
            return COMP_NONE;

        case SDLK_LEFT:
            if (ui->spotlight_cursor > 0) {
                ui->spotlight_cursor--;
            }
            return COMP_NONE;

        case SDLK_RIGHT:
            if (ui->spotlight_cursor < len) {
                ui->spotlight_cursor++;
            }
            return COMP_NONE;

        case SDLK_HOME:
            ui->spotlight_cursor = 0;
            return COMP_NONE;

        case SDLK_END:
            ui->spotlight_cursor = len;
            return COMP_NONE;

        default:
            return COMP_NONE;
    }
}

// Render spotlight search dialog
void ui_render_spotlight(UIState *ui, SDL_Renderer *renderer) {
    if (!ui->show_spotlight) return;

    // Semi-transparent overlay
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xa0);
    SDL_Rect overlay = {0, 0, ui->window_width, ui->window_height};
    SDL_RenderFillRect(renderer, &overlay);

    // Dialog dimensions - centered, top-third of screen
    int dw = 450, max_results = 10;
    int dh = 50 + (ui->spotlight_num_results > 0 ? MIN(ui->spotlight_num_results, max_results) * 28 + 10 : 0);
    int dx = (ui->window_width - dw) / 2;
    int dy = ui->window_height / 5;

    // Dialog background - dark with cyan border (synthwave style)
    SDL_SetRenderDrawColor(renderer, SYNTH_BG_MID, 0xff);
    SDL_Rect dialog = {dx, dy, dw, dh};
    SDL_RenderFillRect(renderer, &dialog);

    // Outer glow effect
    SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0x60);
    SDL_Rect glow1 = {dx - 2, dy - 2, dw + 4, dh + 4};
    SDL_RenderDrawRect(renderer, &glow1);
    SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0x30);
    SDL_Rect glow2 = {dx - 4, dy - 4, dw + 8, dh + 8};
    SDL_RenderDrawRect(renderer, &glow2);

    // Main border
    SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0xff);
    SDL_RenderDrawRect(renderer, &dialog);

    // Search icon (magnifying glass approximation)
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DIM, 0xff);
    int icon_x = dx + 15, icon_y = dy + 18;
    // Circle
    for (int a = 0; a < 360; a += 30) {
        int cx = icon_x + (int)(6 * cos(a * M_PI / 180));
        int cy = icon_y + (int)(6 * sin(a * M_PI / 180));
        SDL_RenderDrawPoint(renderer, cx, cy);
    }
    // Handle
    SDL_RenderDrawLine(renderer, icon_x + 4, icon_y + 4, icon_x + 8, icon_y + 8);

    // Search input field background
    SDL_SetRenderDrawColor(renderer, SYNTH_BG_DARK, 0xff);
    SDL_Rect input_bg = {dx + 35, dy + 10, dw - 50, 28};
    SDL_RenderFillRect(renderer, &input_bg);
    SDL_SetRenderDrawColor(renderer, SYNTH_BORDER, 0xff);
    SDL_RenderDrawRect(renderer, &input_bg);

    // Query text
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT, 0xff);
    if (ui->spotlight_query[0] != '\0') {
        ui_draw_text(renderer, ui->spotlight_query, dx + 42, dy + 18);
    } else {
        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DARK, 0xff);
        ui_draw_text(renderer, "Search components...", dx + 42, dy + 18);
    }

    // Cursor (blinking)
    Uint32 tick = SDL_GetTicks();
    if ((tick / 500) % 2 == 0) {
        SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0xff);
        int cursor_x = dx + 42 + ui->spotlight_cursor * 8;
        SDL_RenderDrawLine(renderer, cursor_x, dy + 14, cursor_x, dy + 32);
    }

    // Results list
    if (ui->spotlight_num_results > 0) {
        int result_y = dy + 48;
        int visible = MIN(ui->spotlight_num_results, max_results);

        for (int i = 0; i < visible; i++) {
            ComponentType comp = ui->spotlight_results[i];
            const ComponentTypeInfo *info = component_get_info(comp);
            if (!info) continue;

            // Highlight selected item
            if (i == ui->spotlight_selected) {
                SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0x40);
                SDL_Rect sel_bg = {dx + 5, result_y - 2, dw - 10, 24};
                SDL_RenderFillRect(renderer, &sel_bg);
                SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0x80);
                SDL_RenderDrawRect(renderer, &sel_bg);
            }

            // Component prefix (like "R" for resistor)
            SDL_SetRenderDrawColor(renderer, SYNTH_PINK, 0xff);
            char prefix_str[8];
            snprintf(prefix_str, sizeof(prefix_str), "[%s]", info->short_name);
            ui_draw_text(renderer, prefix_str, dx + 15, result_y + 4);

            // Component name
            SDL_SetRenderDrawColor(renderer, i == ui->spotlight_selected ? SYNTH_TEXT : SYNTH_TEXT_DIM, 0xff);
            ui_draw_text(renderer, info->name, dx + 60, result_y + 4);

            result_y += 28;
        }

        // Show "more results" indicator if needed
        if (ui->spotlight_num_results > max_results) {
            SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DARK, 0xff);
            char more_str[32];
            snprintf(more_str, sizeof(more_str), "... +%d more", ui->spotlight_num_results - max_results);
            ui_draw_text(renderer, more_str, dx + 15, result_y + 4);
        }
    } else if (ui->spotlight_query[0] != '\0') {
        // No results message
        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DARK, 0xff);
        ui_draw_text(renderer, "No matching components", dx + 15, dy + 55);
    }

    // Hint at bottom
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DARK, 0xff);
    ui_draw_text(renderer, "Enter to select, Esc to close", dx + 10, dy + dh - 18);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// Synthwave gradient spectrum animation along window borders
void ui_render_neon_trim(UIState *ui, SDL_Renderer *renderer) {
    // Get time for animation
    static Uint32 start_time = 0;
    if (start_time == 0) start_time = SDL_GetTicks();
    double time = (SDL_GetTicks() - start_time) / 1000.0;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    int w = ui->window_width;
    int h = ui->window_height;
    int border_thickness = 3;

    // Total perimeter for position calculation
    int perimeter = 2 * w + 2 * h;

    // Chaser position (0-1, moves around the border over time)
    double chaser_speed = 0.25;  // Full loop every 4 seconds
    double chaser_pos = fmod(time * chaser_speed, 1.0);
    double chaser_length = 0.2;  // Length of bright trail (20% of perimeter)

    // Global pulse effect (brightness oscillation for whole border)
    double pulse = 0.7 + 0.3 * sin(time * 2.5);

    // Helper to get bright cyberpunk neon color at position
    // Cycles through: Hot pink -> Electric purple -> Neon cyan -> Electric blue -> back
    #define GET_SYNTH_COLOR(pos, out_r, out_g, out_b) do { \
        double _hue = fmod((pos) + time * 0.15, 1.0); \
        double _r, _g, _b; \
        /* Brighter, more saturated cyberpunk colors */ \
        if (_hue < 0.25) { \
            /* Hot pink to electric purple */ \
            double _t = _hue / 0.25; \
            _r = 1.0; _g = 0.1 * (1.0 - _t); _b = 0.5 + 0.5 * _t; \
        } else if (_hue < 0.5) { \
            /* Electric purple to neon cyan */ \
            double _t = (_hue - 0.25) / 0.25; \
            _r = 1.0 - _t * 0.8; _g = _t; _b = 1.0; \
        } else if (_hue < 0.75) { \
            /* Neon cyan to electric blue */ \
            double _t = (_hue - 0.5) / 0.25; \
            _r = 0.2 - _t * 0.2; _g = 1.0 - _t * 0.5; _b = 1.0; \
        } else { \
            /* Electric blue back to hot pink */ \
            double _t = (_hue - 0.75) / 0.25; \
            _r = _t; _g = 0.5 - _t * 0.4; _b = 1.0 - _t * 0.5; \
        } \
        /* Calculate distance from chaser head (with wraparound) */ \
        double _dist = (pos) - chaser_pos; \
        if (_dist < 0) _dist += 1.0; \
        /* Chaser brightness boost on top of pulse */ \
        double _chaser_boost = 0.0; \
        if (_dist < chaser_length) { \
            _chaser_boost = (1.0 - _dist / chaser_length) * 0.5; \
        } \
        double _brightness = pulse + _chaser_boost; \
        if (_brightness > 1.0) _brightness = 1.0; \
        out_r = (uint8_t)(_r * 255 * _brightness); \
        out_g = (uint8_t)(_g * 255 * _brightness); \
        out_b = (uint8_t)(_b * 255 * _brightness); \
    } while(0)

    // Draw top border (left to right)
    for (int x = 0; x < w; x++) {
        double pos = (double)x / perimeter;
        uint8_t r, g, b;
        GET_SYNTH_COLOR(pos, r, g, b);
        SDL_SetRenderDrawColor(renderer, r, g, b, 200);
        for (int t = 0; t < border_thickness; t++) {
            SDL_RenderDrawPoint(renderer, x, t);
        }
    }

    // Draw right border (top to bottom)
    for (int y = 0; y < h; y++) {
        double pos = (double)(w + y) / perimeter;
        uint8_t r, g, b;
        GET_SYNTH_COLOR(pos, r, g, b);
        SDL_SetRenderDrawColor(renderer, r, g, b, 200);
        for (int t = 0; t < border_thickness; t++) {
            SDL_RenderDrawPoint(renderer, w - 1 - t, y);
        }
    }

    // Draw bottom border (right to left)
    for (int x = w - 1; x >= 0; x--) {
        double pos = (double)(w + h + (w - 1 - x)) / perimeter;
        uint8_t r, g, b;
        GET_SYNTH_COLOR(pos, r, g, b);
        SDL_SetRenderDrawColor(renderer, r, g, b, 200);
        for (int t = 0; t < border_thickness; t++) {
            SDL_RenderDrawPoint(renderer, x, h - 1 - t);
        }
    }

    // Draw left border (bottom to top)
    for (int y = h - 1; y >= 0; y--) {
        double pos = (double)(2 * w + h + (h - 1 - y)) / perimeter;
        uint8_t r, g, b;
        GET_SYNTH_COLOR(pos, r, g, b);
        SDL_SetRenderDrawColor(renderer, r, g, b, 200);
        for (int t = 0; t < border_thickness; t++) {
            SDL_RenderDrawPoint(renderer, t, y);
        }
    }

    #undef GET_SYNTH_COLOR

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static bool point_in_rect(int x, int y, Rect *r) {
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

int ui_handle_click(UIState *ui, int x, int y, bool is_down) {
    if (!ui) return UI_ACTION_NONE;

    // Handle scope resizing
    if (is_down) {
        // Check if clicking on top edge of scope (resize handle)
        int top_edge = ui->scope_rect.y;
        int left_edge = ui->scope_rect.x;
        int right_edge = ui->scope_rect.x + ui->scope_rect.w;

        // Top edge drag zone (5 pixels)
        if (y >= top_edge - 5 && y <= top_edge + 5 &&
            x >= left_edge && x <= right_edge) {
            ui->scope_resizing = true;
            ui->scope_resize_edge = 0;  // top edge
            return UI_ACTION_NONE;
        }
        // Left edge drag zone (5 pixels)
        if (x >= left_edge - 5 && x <= left_edge + 5 &&
            y >= top_edge && y <= ui->scope_rect.y + ui->scope_rect.h) {
            ui->scope_resizing = true;
            ui->scope_resize_edge = 1;  // left edge
            return UI_ACTION_NONE;
        }
        // Properties panel left edge drag zone (5 pixels)
        int props_left_edge = ui->window_width - ui->properties_width;
        if (x >= props_left_edge - 5 && x <= props_left_edge + 5 &&
            y >= TOOLBAR_HEIGHT && y <= ui->window_height - STATUSBAR_HEIGHT) {
            ui->props_resizing = true;
            return UI_ACTION_NONE;
        }

        // Bode plot resizing and dragging
        if (ui->show_bode_plot) {
            Rect *br = &ui->bode_rect;
            int panel_x = br->x - 10;
            int panel_y = br->y - 25;
            int panel_w = br->w + 20;
            int panel_h = br->h + 145;

            // Check if clicking on edges for resizing (5 pixel zones)
            bool on_left = (x >= panel_x - 5 && x <= panel_x + 5 && y >= panel_y && y <= panel_y + panel_h);
            bool on_right = (x >= panel_x + panel_w - 5 && x <= panel_x + panel_w + 5 && y >= panel_y && y <= panel_y + panel_h);
            bool on_top = (y >= panel_y - 5 && y <= panel_y + 5 && x >= panel_x && x <= panel_x + panel_w);
            bool on_bottom = (y >= panel_y + panel_h - 5 && y <= panel_y + panel_h + 5 && x >= panel_x && x <= panel_x + panel_w);

            if (on_left) {
                ui->bode_resizing = true;
                ui->bode_resize_edge = 1;  // left
                return UI_ACTION_NONE;
            } else if (on_right) {
                ui->bode_resizing = true;
                ui->bode_resize_edge = 3;  // right
                return UI_ACTION_NONE;
            } else if (on_top) {
                ui->bode_resizing = true;
                ui->bode_resize_edge = 0;  // top
                return UI_ACTION_NONE;
            } else if (on_bottom) {
                ui->bode_resizing = true;
                ui->bode_resize_edge = 2;  // bottom
                return UI_ACTION_NONE;
            }

            // Check if clicking on title bar for dragging (top 25 pixels of panel)
            if (x >= panel_x && x <= panel_x + panel_w &&
                y >= panel_y && y <= panel_y + 20) {
                ui->bode_dragging = true;
                ui->bode_drag_start_x = x;
                ui->bode_drag_start_y = y;
                ui->bode_rect_start_x = br->x;
                ui->bode_rect_start_y = br->y;
                return UI_ACTION_NONE;
            }

            // Check if clicking on Bode plot settings
            int settings_y = br->y + br->h + 55;  // legend_y + 35

            // Start frequency control (x: br->x + 50 to br->x + 120)
            if (x >= br->x + 50 && x <= br->x + 120 &&
                y >= settings_y && y <= settings_y + 14) {
                // Cycle through start frequencies: 1, 10, 100, 1000 Hz
                if (ui->bode_freq_start <= 1.0) ui->bode_freq_start = 10.0;
                else if (ui->bode_freq_start <= 10.0) ui->bode_freq_start = 100.0;
                else if (ui->bode_freq_start <= 100.0) ui->bode_freq_start = 1000.0;
                else ui->bode_freq_start = 1.0;
                return UI_ACTION_NONE;
            }

            // Stop frequency control (x: br->x + 175 to br->x + 260)
            if (x >= br->x + 175 && x <= br->x + 260 &&
                y >= settings_y && y <= settings_y + 14) {
                // Cycle through stop frequencies: 10k, 100k, 1M, 10M, 100M, 1G Hz
                if (ui->bode_freq_stop <= 10000.0) ui->bode_freq_stop = 100000.0;
                else if (ui->bode_freq_stop <= 100000.0) ui->bode_freq_stop = 1000000.0;
                else if (ui->bode_freq_stop <= 1000000.0) ui->bode_freq_stop = 10000000.0;
                else if (ui->bode_freq_stop <= 10000000.0) ui->bode_freq_stop = 100000000.0;
                else if (ui->bode_freq_stop <= 100000000.0) ui->bode_freq_stop = 1000000000.0;
                else ui->bode_freq_stop = 10000.0;
                return UI_ACTION_NONE;
            }

            // Points control (x: br->x + 320 to br->x + 370)
            if (x >= br->x + 320 && x <= br->x + 370 &&
                y >= settings_y && y <= settings_y + 14) {
                // Cycle through number of points: 50, 100, 200, 500
                if (ui->bode_num_points <= 50) ui->bode_num_points = 100;
                else if (ui->bode_num_points <= 100) ui->bode_num_points = 200;
                else if (ui->bode_num_points <= 200) ui->bode_num_points = 500;
                else ui->bode_num_points = 50;
                return UI_ACTION_NONE;
            }

            // Recalculate button
            if (point_in_rect(x, y, &ui->btn_bode_recalc.bounds) && ui->btn_bode_recalc.enabled) {
                ui->btn_bode_recalc.pressed = true;
                return UI_ACTION_BODE_RECALC;
            }

            // Cursor toggle button (next to recalc)
            int recalc_y_btn = settings_y + 18;  // Same as recalc button
            if (x >= br->x + 80 && x <= br->x + 170 &&
                y >= recalc_y_btn && y <= recalc_y_btn + 14) {
                ui->bode_cursor_active = !ui->bode_cursor_active;
                return UI_ACTION_NONE;
            }

            // Cursor dragging in plot area
            if (ui->bode_cursor_active &&
                x >= br->x && x <= br->x + br->w &&
                y >= br->y && y <= br->y + br->h) {
                // Calculate frequency from x position
                double log_start = log10(ui->bode_freq_start);
                double log_stop = log10(ui->bode_freq_stop);
                double x_norm = (double)(x - br->x) / br->w;
                x_norm = CLAMP(x_norm, 0.0, 1.0);
                double log_freq = log_start + x_norm * (log_stop - log_start);
                ui->bode_cursor_freq = pow(10, log_freq);
                ui->bode_cursor_dragging = true;
                return UI_ACTION_NONE;
            }
        }

        // Monte Carlo panel button handling
        if (ui->show_monte_carlo_panel) {
            if (point_in_rect(x, y, &mc_btn_run)) return UI_ACTION_MC_RUN;
            if (point_in_rect(x, y, &mc_btn_reset)) return UI_ACTION_MC_RESET;
            if (point_in_rect(x, y, &mc_btn_runs_up)) return UI_ACTION_MC_RUNS_UP;
            if (point_in_rect(x, y, &mc_btn_runs_down)) return UI_ACTION_MC_RUNS_DOWN;
            if (point_in_rect(x, y, &mc_btn_tol_up)) return UI_ACTION_MC_TOL_UP;
            if (point_in_rect(x, y, &mc_btn_tol_down)) return UI_ACTION_MC_TOL_DOWN;
            // Check if click is within panel to consume it
            if (point_in_rect(x, y, &mc_panel_rect)) return UI_ACTION_NONE;
        }

        // Handle trigger level dragging - check if click is near trigger indicator
        // The trigger indicator is on the right edge of the scope (arrow at r->x + r->w - 10 to r->x + r->w)
        if (ui->display_mode == SCOPE_MODE_YT && ui->scope_num_channels > 0) {
            Rect *sr = &ui->scope_rect;
            int trig_ch = ui->trigger_channel;
            if (trig_ch < ui->scope_num_channels && ui->scope_channels[trig_ch].enabled) {
                // Calculate current trigger level Y position
                double scale = (sr->h / 8.0) / ui->scope_volt_div;
                int center_y = sr->y + sr->h / 2;
                double trig_offset = ui->scope_channels[trig_ch].offset;
                int trig_y = center_y - (int)((ui->trigger_level + trig_offset) * scale);
                trig_y = CLAMP(trig_y, sr->y, sr->y + sr->h);

                // Check if click is near the trigger level indicator (right edge, +/- 8 pixels vertically)
                if (x >= sr->x + sr->w - 15 && x <= sr->x + sr->w &&
                    y >= trig_y - 8 && y <= trig_y + 8) {
                    ui->dragging_trigger_level = true;
                    return UI_ACTION_NONE;
                }
            }
        }

        // Handle cursor positioning when cursor mode is enabled
        if (ui->scope_cursor_mode && ui->display_mode == SCOPE_MODE_YT) {
            Rect *sr = &ui->scope_rect;
            if (x >= sr->x && x <= sr->x + sr->w &&
                y >= sr->y && y <= sr->y + sr->h) {
                // Clicked inside scope area - position cursor
                double normalized_x = (double)(x - sr->x) / sr->w;
                normalized_x = CLAMP(normalized_x, 0.0, 1.0);

                // Determine which cursor to move (closest one, or alternate)
                double dist1 = fabs(normalized_x - ui->cursor1_time);
                double dist2 = fabs(normalized_x - ui->cursor2_time);

                if (dist1 <= dist2) {
                    ui->cursor1_time = normalized_x;
                    ui->scope_cursor_drag = 1;
                } else {
                    ui->cursor2_time = normalized_x;
                    ui->scope_cursor_drag = 2;
                }
                return UI_ACTION_NONE;
            }
        }
    } else {
        // Release resize
        if (ui->scope_resizing) {
            ui->scope_resizing = false;
            ui->scope_resize_edge = -1;
            ui_update_layout(ui);  // Update button positions
        }
        if (ui->props_resizing) {
            ui->props_resizing = false;
            ui_update_layout(ui);  // Update scope position within panel
        }
        // Release Bode plot resize/drag
        if (ui->bode_resizing) {
            ui->bode_resizing = false;
            ui->bode_resize_edge = -1;
        }
        if (ui->bode_dragging) {
            ui->bode_dragging = false;
        }
        // Release Bode cursor drag
        if (ui->bode_cursor_dragging) {
            ui->bode_cursor_dragging = false;
        }
        // Release cursor drag
        if (ui->scope_cursor_drag != 0) {
            ui->scope_cursor_drag = 0;
        }
        // Release trigger level drag
        if (ui->dragging_trigger_level) {
            ui->dragging_trigger_level = false;
        }
        // Release speed slider drag
        if (ui->dragging_speed) {
            ui->dragging_speed = false;
        }
        // Release environment slider drags
        if (ui->dragging_light) {
            ui->dragging_light = false;
        }
        if (ui->dragging_temp) {
            ui->dragging_temp = false;
        }
    }

    // Check toolbar buttons
    if (is_down) {
        if (point_in_rect(x, y, &ui->btn_run.bounds) && ui->btn_run.enabled) {
            ui->btn_run.pressed = true;
            return UI_ACTION_RUN;
        }
        if (point_in_rect(x, y, &ui->btn_pause.bounds) && ui->btn_pause.enabled) {
            return UI_ACTION_PAUSE;
        }
        if (point_in_rect(x, y, &ui->btn_step.bounds) && ui->btn_step.enabled) {
            return UI_ACTION_STEP;
        }
        if (point_in_rect(x, y, &ui->btn_reset.bounds) && ui->btn_reset.enabled) {
            return UI_ACTION_RESET;
        }
        if (point_in_rect(x, y, &ui->btn_clear.bounds) && ui->btn_clear.enabled) {
            return UI_ACTION_CLEAR;
        }
        if (point_in_rect(x, y, &ui->btn_save.bounds) && ui->btn_save.enabled) {
            return UI_ACTION_SAVE;
        }
        if (point_in_rect(x, y, &ui->btn_load.bounds) && ui->btn_load.enabled) {
            return UI_ACTION_LOAD;
        }
        if (point_in_rect(x, y, &ui->btn_export_svg.bounds) && ui->btn_export_svg.enabled) {
            return UI_ACTION_EXPORT_SVG;
        }

        // Check time step control buttons
        if (point_in_rect(x, y, &ui->btn_timestep_up.bounds) && ui->btn_timestep_up.enabled) {
            return UI_ACTION_TIMESTEP_UP;
        }
        if (point_in_rect(x, y, &ui->btn_timestep_down.bounds) && ui->btn_timestep_down.enabled) {
            return UI_ACTION_TIMESTEP_DOWN;
        }
        if (point_in_rect(x, y, &ui->btn_timestep_auto.bounds) && ui->btn_timestep_auto.enabled) {
            return UI_ACTION_TIMESTEP_AUTO;
        }

        // Check speed slider click
        int slider_x = ui->speed_slider.x + 50;
        Rect slider_bounds = {slider_x, ui->speed_slider.y, ui->speed_slider.w, ui->speed_slider.h};
        if (point_in_rect(x, y, &slider_bounds)) {
            // Map click position to speed value (logarithmic: 1x to 100x)
            float normalized = (float)(x - slider_x) / ui->speed_slider.w;
            normalized = CLAMP(normalized, 0.0f, 1.0f);
            // Convert from linear position to logarithmic scale
            // position 0 = 1x, position 1 = 100x (log scale)
            ui->speed_value = powf(10.0f, normalized * 2.0f);
            ui->speed_value = CLAMP(ui->speed_value, 1.0f, 100.0f);
            ui->dragging_speed = true;
            return UI_ACTION_NONE;  // Handled internally
        }

        // Check environment light slider click (in status bar)
        if (point_in_rect(x, y, &ui->env_light_slider)) {
            // Map click position to light level (0 to 1)
            float normalized = (float)(x - ui->env_light_slider.x) / ui->env_light_slider.w;
            normalized = CLAMP(normalized, 0.0f, 1.0f);
            g_environment.light_level = normalized;
            ui->dragging_light = true;
            return UI_ACTION_NONE;
        }

        // Check environment temperature slider click (in status bar)
        if (point_in_rect(x, y, &ui->env_temp_slider)) {
            // Map click position to temperature (-40°C to 125°C)
            float normalized = (float)(x - ui->env_temp_slider.x) / ui->env_temp_slider.w;
            normalized = CLAMP(normalized, 0.0f, 1.0f);
            g_environment.temperature = -40.0 + normalized * 165.0;  // -40 + (0-1) * 165 = -40 to 125
            ui->dragging_temp = true;
            return UI_ACTION_NONE;
        }

        // Check oscilloscope control buttons
        if (point_in_rect(x, y, &ui->btn_scope_volt_up.bounds) && ui->btn_scope_volt_up.enabled) {
            return UI_ACTION_SCOPE_VOLT_UP;
        }
        if (point_in_rect(x, y, &ui->btn_scope_volt_down.bounds) && ui->btn_scope_volt_down.enabled) {
            return UI_ACTION_SCOPE_VOLT_DOWN;
        }
        if (point_in_rect(x, y, &ui->btn_scope_time_up.bounds) && ui->btn_scope_time_up.enabled) {
            return UI_ACTION_SCOPE_TIME_UP;
        }
        if (point_in_rect(x, y, &ui->btn_scope_time_down.bounds) && ui->btn_scope_time_down.enabled) {
            return UI_ACTION_SCOPE_TIME_DOWN;
        }
        if (point_in_rect(x, y, &ui->btn_scope_trig_mode.bounds) && ui->btn_scope_trig_mode.enabled) {
            return UI_ACTION_SCOPE_TRIG_MODE;
        }
        if (point_in_rect(x, y, &ui->btn_scope_trig_edge.bounds) && ui->btn_scope_trig_edge.enabled) {
            return UI_ACTION_SCOPE_TRIG_EDGE;
        }
        if (point_in_rect(x, y, &ui->btn_scope_trig_ch.bounds) && ui->btn_scope_trig_ch.enabled) {
            return UI_ACTION_SCOPE_TRIG_CH;
        }
        if (point_in_rect(x, y, &ui->btn_scope_trig_up.bounds) && ui->btn_scope_trig_up.enabled) {
            return UI_ACTION_SCOPE_TRIG_UP;
        }
        if (point_in_rect(x, y, &ui->btn_scope_trig_down.bounds) && ui->btn_scope_trig_down.enabled) {
            return UI_ACTION_SCOPE_TRIG_DOWN;
        }
        if (point_in_rect(x, y, &ui->btn_scope_mode.bounds) && ui->btn_scope_mode.enabled) {
            return UI_ACTION_SCOPE_MODE;
        }
        if (point_in_rect(x, y, &ui->btn_scope_screenshot.bounds) && ui->btn_scope_screenshot.enabled) {
            return UI_ACTION_SCOPE_SCREENSHOT;
        }
        if (point_in_rect(x, y, &ui->btn_scope_cursor.bounds) && ui->btn_scope_cursor.enabled) {
            return UI_ACTION_CURSOR_TOGGLE;
        }
        if (point_in_rect(x, y, &ui->btn_scope_fft.bounds) && ui->btn_scope_fft.enabled) {
            return UI_ACTION_FFT_TOGGLE;
        }
        if (point_in_rect(x, y, &ui->btn_scope_autoset.bounds) && ui->btn_scope_autoset.enabled) {
            return UI_ACTION_SCOPE_AUTOSET;
        }
        if (point_in_rect(x, y, &ui->btn_bode.bounds) && ui->btn_bode.enabled) {
            return UI_ACTION_BODE_PLOT;
        }
        if (point_in_rect(x, y, &ui->btn_mc.bounds) && ui->btn_mc.enabled) {
            return UI_ACTION_MONTE_CARLO;
        }
        if (point_in_rect(x, y, &ui->btn_scope_popup.bounds) && ui->btn_scope_popup.enabled) {
            return UI_ACTION_SCOPE_POPUP;
        }

        // Check property fields for click-to-edit
        // Each property field stores its prop_type directly
        for (int i = 0; i < ui->num_properties && i < 16; i++) {
            if (point_in_rect(x, y, &ui->properties[i].bounds)) {
                return UI_ACTION_PROP_EDIT + ui->properties[i].prop_type;
            }
        }

        // Check palette category headers for collapse/expand (must be in palette area)
        if (x >= 0 && x < PALETTE_WIDTH - 10 && y >= TOOLBAR_HEIGHT && y < ui->window_height - STATUSBAR_HEIGHT) {
            int content_y = y + ui->palette_scroll_offset;  // Convert screen y to content y
            // Check all categories for header clicks
            for (int cat = 0; cat < PCAT_COUNT; cat++) {
                PaletteCategory *category = &ui->categories[cat];
                // Header_y is in content coords, header is 14 pixels tall
                if (content_y >= category->header_y && content_y < category->header_y + 14) {
                    // Toggle collapsed state
                    category->collapsed = !category->collapsed;
                    return UI_ACTION_NONE;  // Handled, no further action needed
                }
            }
        }

        // Check palette items (adjust y for scroll offset)
        int adjusted_y = y + ui->palette_scroll_offset;
        for (int i = 0; i < ui->num_palette_items; i++) {
            if (point_in_rect(x, adjusted_y, &ui->palette_items[i].bounds)) {
                // Verify item is visible (not clipped by scroll)
                int item_screen_y = ui->palette_items[i].bounds.y - ui->palette_scroll_offset;
                if (item_screen_y < TOOLBAR_HEIGHT || item_screen_y + ui->palette_items[i].bounds.h > ui->window_height - STATUSBAR_HEIGHT) {
                    continue;  // Item is scrolled out of view
                }
                // Deselect all palette and circuit items
                for (int j = 0; j < ui->num_palette_items; j++) {
                    ui->palette_items[j].selected = false;
                }
                for (int j = 0; j < ui->num_circuit_items; j++) {
                    ui->circuit_items[j].selected = false;
                }
                ui->palette_items[i].selected = true;
                ui->selected_palette_idx = i;
                ui->placing_circuit = false;
                ui->selected_circuit_type = -1;

                if (ui->palette_items[i].is_tool) {
                    return UI_ACTION_SELECT_TOOL + ui->palette_items[i].tool_type;
                } else {
                    return UI_ACTION_SELECT_COMP + ui->palette_items[i].comp_type;
                }
            }
        }

        // Check circuit template items (adjust y for scroll offset)
        for (int i = 0; i < ui->num_circuit_items; i++) {
            if (point_in_rect(x, adjusted_y, &ui->circuit_items[i].bounds)) {
                // Verify item is visible (not clipped by scroll)
                int item_screen_y = ui->circuit_items[i].bounds.y - ui->palette_scroll_offset;
                if (item_screen_y < TOOLBAR_HEIGHT || item_screen_y + ui->circuit_items[i].bounds.h > ui->window_height - STATUSBAR_HEIGHT) {
                    continue;  // Item is scrolled out of view
                }
                // Deselect all palette and circuit items
                for (int j = 0; j < ui->num_palette_items; j++) {
                    ui->palette_items[j].selected = false;
                }
                for (int j = 0; j < ui->num_circuit_items; j++) {
                    ui->circuit_items[j].selected = false;
                }
                ui->circuit_items[i].selected = true;
                ui->placing_circuit = true;
                ui->selected_circuit_type = ui->circuit_items[i].circuit_type;

                return UI_ACTION_SELECT_CIRCUIT + ui->circuit_items[i].circuit_type;
            }
        }
    } else {
        ui->btn_run.pressed = false;
        ui->btn_bode_recalc.pressed = false;
    }

    return UI_ACTION_NONE;
}

int ui_handle_motion(UIState *ui, int x, int y) {
    if (!ui) return UI_ACTION_NONE;

    // Handle scope resizing
    if (ui->scope_resizing) {
        if (ui->scope_resize_edge == 0) {
            // Resizing top edge - changes height and y position
            int new_y = y;
            int bottom = ui->scope_rect.y + ui->scope_rect.h;
            int new_height = bottom - new_y;

            // Minimum and maximum height constraints
            if (new_height >= 100 && new_height <= 500 && new_y >= TOOLBAR_HEIGHT + 200) {
                ui->scope_rect.y = new_y;
                ui->scope_rect.h = new_height;
            }
        } else if (ui->scope_resize_edge == 1) {
            // Resizing left edge - changes width and x position
            int new_x = x;
            int right = ui->scope_rect.x + ui->scope_rect.w;
            int new_width = right - new_x;

            // Minimum and maximum width constraints
            if (new_width >= 150 && new_width <= 400 && new_x >= ui->window_width - ui->properties_width) {
                ui->scope_rect.x = new_x;
                ui->scope_rect.w = new_width;
            }
        }
        return UI_ACTION_NONE;
    }

    // Handle Bode plot resizing
    if (ui->bode_resizing) {
        Rect *br = &ui->bode_rect;

        if (ui->bode_resize_edge == 0) {
            // Top edge - changes height and y position
            int new_y = y + 25;  // Account for title bar
            int bottom = br->y + br->h;
            int new_height = bottom - new_y;

            if (new_height >= 100 && new_height <= 400 && new_y >= TOOLBAR_HEIGHT + 50) {
                br->y = new_y;
                br->h = new_height;
            }
        } else if (ui->bode_resize_edge == 1) {
            // Left edge - changes width and x position
            int new_x = x + 10;  // Account for panel padding
            int right = br->x + br->w;
            int new_width = right - new_x;

            if (new_width >= 200 && new_width <= 600 && new_x >= 200) {
                br->x = new_x;
                br->w = new_width;
            }
        } else if (ui->bode_resize_edge == 2) {
            // Bottom edge - changes height only
            int new_height = y - br->y - 95;  // Account for controls below plot

            if (new_height >= 100 && new_height <= 400) {
                br->h = new_height;
            }
        } else if (ui->bode_resize_edge == 3) {
            // Right edge - changes width only
            int new_width = x - br->x - 10;  // Account for panel padding

            if (new_width >= 200 && new_width <= 600) {
                br->w = new_width;
            }
        }
        return UI_ACTION_NONE;
    }

    // Handle Bode plot dragging (move window)
    if (ui->bode_dragging) {
        int dx = x - ui->bode_drag_start_x;
        int dy = y - ui->bode_drag_start_y;

        // Calculate new position based on start position + delta
        int new_x = ui->bode_rect_start_x + dx;
        int new_y = ui->bode_rect_start_y + dy;

        // Keep within window bounds
        int panel_w = ui->bode_rect.w + 20;
        int panel_h = ui->bode_rect.h + 145;
        int panel_x = new_x - 10;
        int panel_y = new_y - 25;

        if (panel_x < 0) new_x = 10;
        if (panel_y < TOOLBAR_HEIGHT) new_y = TOOLBAR_HEIGHT + 25;
        if (panel_x + panel_w > ui->window_width)
            new_x = ui->window_width - panel_w + 10;
        if (panel_y + panel_h > ui->window_height)
            new_y = ui->window_height - panel_h + 25;

        ui->bode_rect.x = new_x;
        ui->bode_rect.y = new_y;

        return UI_ACTION_NONE;
    }

    // Handle Bode cursor dragging
    if (ui->bode_cursor_dragging && ui->show_bode_plot) {
        Rect *br = &ui->bode_rect;
        // Calculate frequency from x position
        double log_start = log10(ui->bode_freq_start);
        double log_stop = log10(ui->bode_freq_stop);
        double x_norm = (double)(x - br->x) / br->w;
        x_norm = CLAMP(x_norm, 0.0, 1.0);
        double log_freq = log_start + x_norm * (log_stop - log_start);
        ui->bode_cursor_freq = pow(10, log_freq);
        return UI_ACTION_NONE;
    }

    // Handle cursor dragging
    if (ui->scope_cursor_drag != 0) {
        Rect *sr = &ui->scope_rect;
        double normalized_x = (double)(x - sr->x) / sr->w;
        normalized_x = CLAMP(normalized_x, 0.0, 1.0);

        if (ui->scope_cursor_drag == 1) {
            ui->cursor1_time = normalized_x;
        } else {
            ui->cursor2_time = normalized_x;
        }
        return UI_ACTION_NONE;
    }

    // Handle trigger level dragging
    if (ui->dragging_trigger_level && ui->scope_num_channels > 0) {
        Rect *sr = &ui->scope_rect;
        int trig_ch = ui->trigger_channel;
        if (trig_ch < ui->scope_num_channels && ui->scope_channels[trig_ch].enabled) {
            // Calculate scale and center_y (same as in rendering)
            double scale = (sr->h / 8.0) / ui->scope_volt_div;
            int center_y = sr->y + sr->h / 2;
            double trig_offset = ui->scope_channels[trig_ch].offset;

            // Convert mouse Y position to trigger level voltage
            // From rendering: trig_y = center_y - (int)((trigger_level + trig_offset) * scale)
            // So: trigger_level = (center_y - y) / scale - trig_offset
            double new_level = (double)(center_y - y) / scale - trig_offset;

            // Clamp to reasonable voltage range based on volt_div (4 divisions = 4 * volt_div)
            double max_level = 4.0 * ui->scope_volt_div;
            ui->trigger_level = CLAMP(new_level, -max_level, max_level);
        }
        return UI_ACTION_NONE;
    }

    // Handle speed slider dragging
    if (ui->dragging_speed) {
        int slider_x = ui->speed_slider.x + 50;
        float normalized = (float)(x - slider_x) / ui->speed_slider.w;
        normalized = CLAMP(normalized, 0.0f, 1.0f);
        // Convert from linear position to logarithmic scale (1x to 100x)
        ui->speed_value = powf(10.0f, normalized * 2.0f);
        ui->speed_value = CLAMP(ui->speed_value, 1.0f, 100.0f);
        return UI_ACTION_NONE;
    }

    // Handle environment light slider dragging
    if (ui->dragging_light) {
        float normalized = (float)(x - ui->env_light_slider.x) / ui->env_light_slider.w;
        normalized = CLAMP(normalized, 0.0f, 1.0f);
        g_environment.light_level = normalized;
        return UI_ACTION_NONE;
    }

    // Handle environment temperature slider dragging
    if (ui->dragging_temp) {
        float normalized = (float)(x - ui->env_temp_slider.x) / ui->env_temp_slider.w;
        normalized = CLAMP(normalized, 0.0f, 1.0f);
        g_environment.temperature = -40.0 + normalized * 165.0;  // -40 to 125
        return UI_ACTION_NONE;
    }

    // Handle properties panel resizing
    if (ui->props_resizing) {
        int new_width = ui->window_width - x;

        // Minimum and maximum width constraints
        if (new_width >= 180 && new_width <= 450) {
            ui->properties_width = new_width;
            // Also update scope position to stay within panel
            ui->scope_rect.x = ui->window_width - ui->properties_width + 10;
            ui->scope_rect.w = ui->properties_width - 20;  // Fit scope width to panel
        }
        return UI_ACTION_NONE;
    }

    // Update button hover states
    ui->btn_run.hovered = point_in_rect(x, y, &ui->btn_run.bounds);
    ui->btn_pause.hovered = point_in_rect(x, y, &ui->btn_pause.bounds);
    ui->btn_step.hovered = point_in_rect(x, y, &ui->btn_step.bounds);
    ui->btn_reset.hovered = point_in_rect(x, y, &ui->btn_reset.bounds);
    ui->btn_clear.hovered = point_in_rect(x, y, &ui->btn_clear.bounds);
    ui->btn_save.hovered = point_in_rect(x, y, &ui->btn_save.bounds);
    ui->btn_load.hovered = point_in_rect(x, y, &ui->btn_load.bounds);
    ui->btn_export_svg.hovered = point_in_rect(x, y, &ui->btn_export_svg.bounds);
    ui->btn_timestep_up.hovered = point_in_rect(x, y, &ui->btn_timestep_up.bounds);
    ui->btn_timestep_down.hovered = point_in_rect(x, y, &ui->btn_timestep_down.bounds);
    ui->btn_timestep_auto.hovered = point_in_rect(x, y, &ui->btn_timestep_auto.bounds);

    // Update oscilloscope button hover states
    ui->btn_scope_volt_up.hovered = point_in_rect(x, y, &ui->btn_scope_volt_up.bounds);
    ui->btn_scope_volt_down.hovered = point_in_rect(x, y, &ui->btn_scope_volt_down.bounds);
    ui->btn_scope_time_up.hovered = point_in_rect(x, y, &ui->btn_scope_time_up.bounds);
    ui->btn_scope_time_down.hovered = point_in_rect(x, y, &ui->btn_scope_time_down.bounds);
    ui->btn_scope_trig_mode.hovered = point_in_rect(x, y, &ui->btn_scope_trig_mode.bounds);
    ui->btn_scope_trig_edge.hovered = point_in_rect(x, y, &ui->btn_scope_trig_edge.bounds);
    ui->btn_scope_trig_ch.hovered = point_in_rect(x, y, &ui->btn_scope_trig_ch.bounds);
    ui->btn_scope_trig_up.hovered = point_in_rect(x, y, &ui->btn_scope_trig_up.bounds);
    ui->btn_scope_trig_down.hovered = point_in_rect(x, y, &ui->btn_scope_trig_down.bounds);
    ui->btn_scope_mode.hovered = point_in_rect(x, y, &ui->btn_scope_mode.bounds);
    ui->btn_scope_screenshot.hovered = point_in_rect(x, y, &ui->btn_scope_screenshot.bounds);
    ui->btn_scope_cursor.hovered = point_in_rect(x, y, &ui->btn_scope_cursor.bounds);
    ui->btn_scope_fft.hovered = point_in_rect(x, y, &ui->btn_scope_fft.bounds);
    ui->btn_scope_autoset.hovered = point_in_rect(x, y, &ui->btn_scope_autoset.bounds);
    ui->btn_bode.hovered = point_in_rect(x, y, &ui->btn_bode.bounds);
    ui->btn_mc.hovered = point_in_rect(x, y, &ui->btn_mc.bounds);
    ui->btn_scope_popup.hovered = point_in_rect(x, y, &ui->btn_scope_popup.bounds);
    ui->btn_bode_recalc.hovered = ui->show_bode_plot && point_in_rect(x, y, &ui->btn_bode_recalc.bounds);

    // Update palette hover states (adjust y for scroll offset)
    int adjusted_y = y + ui->palette_scroll_offset;
    for (int i = 0; i < ui->num_palette_items; i++) {
        bool in_bounds = point_in_rect(x, adjusted_y, &ui->palette_items[i].bounds);
        // Also check if item is visible on screen
        if (in_bounds) {
            int item_screen_y = ui->palette_items[i].bounds.y - ui->palette_scroll_offset;
            if (item_screen_y < TOOLBAR_HEIGHT || item_screen_y + ui->palette_items[i].bounds.h > ui->window_height - STATUSBAR_HEIGHT) {
                in_bounds = false;  // Item is scrolled out of view
            }
        }
        ui->palette_items[i].hovered = in_bounds;
    }

    // Update circuit items hover states (adjust y for scroll offset)
    for (int i = 0; i < ui->num_circuit_items; i++) {
        bool in_bounds = point_in_rect(x, adjusted_y, &ui->circuit_items[i].bounds);
        // Also check if item is visible on screen
        if (in_bounds) {
            int item_screen_y = ui->circuit_items[i].bounds.y - ui->palette_scroll_offset;
            if (item_screen_y < TOOLBAR_HEIGHT || item_screen_y + ui->circuit_items[i].bounds.h > ui->window_height - STATUSBAR_HEIGHT) {
                in_bounds = false;  // Item is scrolled out of view
            }
        }
        ui->circuit_items[i].hovered = in_bounds;
    }

    return UI_ACTION_NONE;
}

void ui_set_status(UIState *ui, const char *msg) {
    if (ui && msg) {
        strncpy(ui->status_message, msg, sizeof(ui->status_message) - 1);
        ui->status_message[sizeof(ui->status_message) - 1] = '\0';
    }
}

void ui_update_measurements(UIState *ui, Simulation *sim, Circuit *circuit) {
    if (!ui) return;

    if (sim) {
        ui->sim_time = sim->time;
    }

    if (circuit) {
        ui->node_count = circuit->num_nodes;
        ui->component_count = circuit->num_components;
    }
}

void ui_update_scope_channels(UIState *ui, Circuit *circuit) {
    if (!ui || !circuit) return;

    // Update oscilloscope channels based on probes in circuit
    ui->scope_num_channels = circuit->num_probes;

    for (int i = 0; i < circuit->num_probes && i < MAX_PROBES; i++) {
        ui->scope_channels[i].enabled = true;
        ui->scope_channels[i].probe_idx = i;
        ui->scope_channels[i].color = PROBE_COLORS[i];

        // Update probe with channel info
        circuit->probes[i].channel_num = i;
        circuit->probes[i].color = PROBE_COLORS[i];
        snprintf(circuit->probes[i].label, sizeof(circuit->probes[i].label), "CH%d", i + 1);
    }

    // Disable unused channels
    for (int i = circuit->num_probes; i < MAX_PROBES; i++) {
        ui->scope_channels[i].enabled = false;
    }
}

void ui_update_layout(UIState *ui) {
    if (!ui) return;

    // Update palette visible height
    ui->palette_visible_height = ui->window_height - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT;

    // Clamp scroll offset to valid range
    int max_scroll = ui->palette_content_height - ui->palette_visible_height;
    if (max_scroll < 0) max_scroll = 0;
    if (ui->palette_scroll_offset > max_scroll) {
        ui->palette_scroll_offset = max_scroll;
    }
    if (ui->palette_scroll_offset < 0) {
        ui->palette_scroll_offset = 0;
    }

    // Update oscilloscope position (anchored to right side, vertically positioned based on height)
    ui->scope_rect.x = ui->window_width - ui->properties_width + 10;

    // Ensure scope is positioned below properties content
    int min_scope_y = TOOLBAR_HEIGHT + ui->properties_content_height + 25;
    if (ui->scope_rect.y < min_scope_y) {
        ui->scope_rect.y = min_scope_y;
    }

    // Keep y position reasonable or adjust if window is too small
    int max_scope_y = ui->window_height - STATUSBAR_HEIGHT - ui->scope_rect.h - 100;
    if (ui->scope_rect.y > max_scope_y && max_scope_y > min_scope_y) {
        ui->scope_rect.y = max_scope_y;
    }

    // Update oscilloscope control buttons - 3 row layout
    // Row 1: Scale controls (V/div, T/div) + Autoset
    int scope_btn_y = ui->scope_rect.y + ui->scope_rect.h + 5;
    int scope_btn_w = 32, scope_btn_h = 22;
    int scope_btn_x = ui->scope_rect.x;
    int row_spacing = scope_btn_h + 4;

    ui->btn_scope_volt_up.bounds = (Rect){scope_btn_x, scope_btn_y, scope_btn_w, scope_btn_h};
    scope_btn_x += scope_btn_w + 3;
    ui->btn_scope_volt_down.bounds = (Rect){scope_btn_x, scope_btn_y, scope_btn_w, scope_btn_h};
    scope_btn_x += scope_btn_w + 10;
    ui->btn_scope_time_up.bounds = (Rect){scope_btn_x, scope_btn_y, scope_btn_w, scope_btn_h};
    scope_btn_x += scope_btn_w + 3;
    ui->btn_scope_time_down.bounds = (Rect){scope_btn_x, scope_btn_y, scope_btn_w, scope_btn_h};
    scope_btn_x += scope_btn_w + 10;
    ui->btn_scope_autoset.bounds = (Rect){scope_btn_x, scope_btn_y, 50, scope_btn_h};

    // Row 2: Trigger controls
    scope_btn_y += row_spacing;
    scope_btn_x = ui->scope_rect.x;

    ui->btn_scope_trig_mode.bounds = (Rect){scope_btn_x, scope_btn_y, 45, scope_btn_h};
    scope_btn_x += 48;
    ui->btn_scope_trig_edge.bounds = (Rect){scope_btn_x, scope_btn_y, 28, scope_btn_h};
    scope_btn_x += 31;
    ui->btn_scope_trig_ch.bounds = (Rect){scope_btn_x, scope_btn_y, 35, scope_btn_h};
    scope_btn_x += 38;
    ui->btn_scope_trig_up.bounds = (Rect){scope_btn_x, scope_btn_y, 24, scope_btn_h};
    scope_btn_x += 27;
    ui->btn_scope_trig_down.bounds = (Rect){scope_btn_x, scope_btn_y, 24, scope_btn_h};

    // Row 3: Display modes and tools
    scope_btn_y += row_spacing;
    scope_btn_x = ui->scope_rect.x;

    ui->btn_scope_mode.bounds = (Rect){scope_btn_x, scope_btn_y, 35, scope_btn_h};
    scope_btn_x += 38;
    ui->btn_scope_cursor.bounds = (Rect){scope_btn_x, scope_btn_y, 35, scope_btn_h};
    scope_btn_x += 38;
    ui->btn_scope_fft.bounds = (Rect){scope_btn_x, scope_btn_y, 35, scope_btn_h};
    scope_btn_x += 38;
    ui->btn_scope_screenshot.bounds = (Rect){scope_btn_x, scope_btn_y, 35, scope_btn_h};
    scope_btn_x += 38;
    ui->btn_bode.bounds = (Rect){scope_btn_x, scope_btn_y, 40, scope_btn_h};
    scope_btn_x += 43;
    ui->btn_mc.bounds = (Rect){scope_btn_x, scope_btn_y, 25, scope_btn_h};
    scope_btn_x += 28;
    ui->btn_scope_popup.bounds = (Rect){scope_btn_x, scope_btn_y, 50, scope_btn_h};
}

// Oscilloscope autoset - automatically configure scope based on signal analysis
void ui_scope_autoset(UIState *ui, Simulation *sim) {
    if (!ui || !sim || ui->scope_num_channels == 0) return;

    // Standard volt/div and time/div values (1-2-5 sequence)
    static const double volt_divs[] = {0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0};
    static const int num_volt_divs = sizeof(volt_divs) / sizeof(volt_divs[0]);
    static const double time_divs[] = {1e-9, 2e-9, 5e-9, 10e-9, 20e-9, 50e-9, 100e-9, 200e-9, 500e-9,
                                       1e-6, 2e-6, 5e-6, 10e-6, 20e-6, 50e-6, 100e-6, 200e-6, 500e-6,
                                       1e-3, 2e-3, 5e-3, 10e-3, 20e-3, 50e-3, 100e-3, 200e-3, 500e-3,
                                       1.0, 2.0, 5.0};
    static const int num_time_divs = sizeof(time_divs) / sizeof(time_divs[0]);

    // Analyze all enabled channels to find signal characteristics
    double global_min = 1e9, global_max = -1e9;
    double estimated_period = 0;
    int samples_with_signal = 0;

    for (int ch = 0; ch < ui->scope_num_channels; ch++) {
        if (!ui->scope_channels[ch].enabled) continue;

        double times[MAX_HISTORY], values[MAX_HISTORY];
        int probe_idx = ui->scope_channels[ch].probe_idx;
        int count = simulation_get_history(sim, probe_idx, times, values, MAX_HISTORY);

        if (count < 10) continue;  // Need enough samples

        // Find min/max
        double ch_min = values[0], ch_max = values[0];
        for (int i = 1; i < count; i++) {
            if (values[i] < ch_min) ch_min = values[i];
            if (values[i] > ch_max) ch_max = values[i];
        }

        if (ch_min < global_min) global_min = ch_min;
        if (ch_max > global_max) global_max = ch_max;

        // Estimate period by finding zero crossings (or midpoint crossings)
        double midpoint = (ch_min + ch_max) / 2.0;
        double amplitude = ch_max - ch_min;

        // Only estimate period if there's significant signal
        if (amplitude > 0.01) {
            samples_with_signal++;

            // Find rising edge crossings
            int crossing_count = 0;
            double first_crossing = 0, last_crossing = 0;

            for (int i = 1; i < count; i++) {
                // Rising edge crossing
                if (values[i-1] < midpoint && values[i] >= midpoint) {
                    if (crossing_count == 0) {
                        first_crossing = times[i];
                    }
                    last_crossing = times[i];
                    crossing_count++;
                }
            }

            // Estimate period from crossings
            if (crossing_count >= 2) {
                double total_time = last_crossing - first_crossing;
                double period = total_time / (crossing_count - 1);
                if (period > 0 && (estimated_period == 0 || period < estimated_period)) {
                    estimated_period = period;  // Use shortest period found
                }
            }
        }
    }

    // If no signal found, use default settings
    if (global_max <= global_min || samples_with_signal == 0) {
        ui->scope_volt_div = 1.0;
        ui->scope_time_div = 0.001;  // 1ms/div
        ui->trigger_level = 0;
        return;
    }

    // Calculate appropriate volt/div
    // Want signal to fill about 60-80% of the vertical range (8 divisions)
    double signal_range = global_max - global_min;
    double target_range = signal_range * 1.4;  // Add some margin
    double ideal_volt_div = target_range / 8.0;

    // Find nearest standard value (round up)
    int volt_idx = 0;
    for (int i = 0; i < num_volt_divs; i++) {
        if (volt_divs[i] >= ideal_volt_div) {
            volt_idx = i;
            break;
        }
        volt_idx = i;  // Use largest if nothing fits
    }
    ui->scope_volt_div = volt_divs[volt_idx];

    // Calculate appropriate time/div
    if (estimated_period > 0) {
        // Show about 2-3 complete cycles (10 divisions total)
        double target_time = estimated_period * 2.5;
        double ideal_time_div = target_time / 10.0;

        // Find nearest standard value
        int time_idx = 0;
        for (int i = 0; i < num_time_divs; i++) {
            if (time_divs[i] >= ideal_time_div) {
                time_idx = i;
                break;
            }
            time_idx = i;
        }
        ui->scope_time_div = time_divs[time_idx];
    } else {
        // DC or very low frequency - use a reasonable default
        ui->scope_time_div = 0.01;  // 10ms/div
    }

    // Set trigger level to midpoint of signal
    ui->trigger_level = (global_min + global_max) / 2.0;

    // Reset channel offsets to center the signal
    double signal_center = (global_min + global_max) / 2.0;
    for (int ch = 0; ch < ui->scope_num_channels; ch++) {
        if (ui->scope_channels[ch].enabled) {
            ui->scope_channels[ch].offset = -signal_center;
        }
    }

    // Set trigger to rising edge and auto mode for good display
    ui->trigger_edge = TRIG_EDGE_RISING;
    ui->trigger_mode = TRIG_AUTO;
}

// Handle palette scroll (mouse wheel)
void ui_palette_scroll(UIState *ui, int delta) {
    if (!ui) return;

    // Only scroll if content exceeds visible area
    if (ui->palette_content_height <= ui->palette_visible_height) {
        return;
    }

    // Scroll amount per wheel notch (pixels)
    int scroll_amount = 40;
    ui->palette_scroll_offset -= delta * scroll_amount;

    // Clamp to valid range
    int max_scroll = ui->palette_content_height - ui->palette_visible_height;
    if (ui->palette_scroll_offset < 0) {
        ui->palette_scroll_offset = 0;
    }
    if (ui->palette_scroll_offset > max_scroll) {
        ui->palette_scroll_offset = max_scroll;
    }
}

// Check if point is in palette area
bool ui_point_in_palette(UIState *ui, int x, int y) {
    if (!ui) return false;
    return (x >= 0 && x < PALETTE_WIDTH &&
            y >= TOOLBAR_HEIGHT && y < ui->window_height - STATUSBAR_HEIGHT);
}

// Handle properties scroll (mouse wheel)
void ui_properties_scroll(UIState *ui, int delta) {
    if (!ui) return;

    // Only scroll if content exceeds visible area
    if (ui->properties_content_height <= ui->properties_visible_height) {
        return;
    }

    // Scroll amount per wheel notch (pixels)
    int scroll_amount = 40;
    ui->properties_scroll_offset -= delta * scroll_amount;

    // Clamp to valid range
    int max_scroll = ui->properties_content_height - ui->properties_visible_height;
    if (ui->properties_scroll_offset < 0) {
        ui->properties_scroll_offset = 0;
    }
    if (ui->properties_scroll_offset > max_scroll) {
        ui->properties_scroll_offset = max_scroll;
    }
}

// Check if point is in properties area
bool ui_point_in_properties(UIState *ui, int x, int y) {
    if (!ui) return false;
    int props_x = ui->window_width - ui->properties_width;
    int props_y_end = ui->scope_rect.y - 50;  // Match the gap in ui_render_properties
    return (x >= props_x && x < ui->window_width &&
            y >= TOOLBAR_HEIGHT && y < props_y_end);
}
