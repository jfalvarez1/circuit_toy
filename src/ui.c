/**
 * Circuit Playground - UI System Implementation
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <math.h>
#include "ui.h"

/* Defined below, next to the panel it serves; declared here because draw_sweep_config
   emits rows before that point. */
static PropertyField *ui_prop_slot(UIState *ui);
#include "input.h"
#include "circuits.h"
#include "analysis.h"
#include "version.h"   /* the version shown under the title */

static void ui_volt_readout(char *out, size_t n, double v);   // defined with the scope layout helpers
static void scope_button_list(UIState *ui, Button *out[SCOPE_BTN_N]);
void ui_update_layout(UIState *ui);

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
    /* Everything except Tools starts collapsed. With 180-odd templates and twenty component
       categories an all-open palette is a wall of buttons; a closed one is a table of contents. */
    ui->categories[PCAT_TOOLS] = (PaletteCategory){"Tools", false, 0};
    ui->categories[PCAT_SOURCES] = (PaletteCategory){"Sources", true, 0};
    ui->categories[PCAT_WAVEFORMS] = (PaletteCategory){"Waveforms", true, 0};
    ui->categories[PCAT_PASSIVES] = (PaletteCategory){"Passives", true, 0};
    ui->categories[PCAT_DIODES] = (PaletteCategory){"Diodes", true, 0};
    ui->categories[PCAT_BJT] = (PaletteCategory){"BJT", true, 0};
    ui->categories[PCAT_FET] = (PaletteCategory){"FET", true, 0};
    ui->categories[PCAT_THYRISTORS] = (PaletteCategory){"Thyristors", true, 0};
    ui->categories[PCAT_OPAMPS] = (PaletteCategory){"Op-Amps", true, 0};
    ui->categories[PCAT_CONTROLLED] = (PaletteCategory){"Ctrl Sources", true, 0};
    ui->categories[PCAT_SWITCHES] = (PaletteCategory){"Switches", true, 0};
    ui->categories[PCAT_TRANSFORMERS] = (PaletteCategory){"Transformers", true, 0};
    ui->categories[PCAT_LOGIC] = (PaletteCategory){"Logic Gates", true, 0};
    ui->categories[PCAT_DIGITAL] = (PaletteCategory){"Digital ICs", true, 0};
    ui->categories[PCAT_MIXED] = (PaletteCategory){"Mixed Signal", true, 0};
    ui->categories[PCAT_REGULATORS] = (PaletteCategory){"Regulators", true, 0};
    ui->categories[PCAT_DISPLAY] = (PaletteCategory){"Display", true, 0};
    ui->categories[PCAT_WIRELESS] = (PaletteCategory){"Wireless", true, 0};
    ui->categories[PCAT_MEASUREMENT] = (PaletteCategory){"Measurement", true, 0};
    ui->categories[PCAT_SUBPARTS] = (PaletteCategory){"Sub-circuit / Bus", true, 0};
    ui->categories[PCAT_CIRCUITS] = (PaletteCategory){"Circuits", true, 0};
    ui->categories[PCAT_SUBCIRCUITS] = (PaletteCategory){"My Circuits", true, 0};

    // Set initial window dimensions
    ui->window_width = WINDOW_WIDTH;
    ui->window_height = WINDOW_HEIGHT;

    // Initialize properties panel width
    ui->properties_width = PROPERTIES_WIDTH;
    ui->props_resizing = false;
    ui->properties_content_height = 200;  // Default height, updated dynamically

    /* 160, not 200. "Circuit Playground" ends at about x = 145 and the version sits under it, so
       the old start left 55 px of nothing at the very moment the right-hand end of the toolbar had
       run out of window. */
    // Initialize toolbar buttons
    int btn_x = 160;
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
    btn_x += btn_w + 10;
    ui->btn_screenshot = (Button){{btn_x, 10, 35, btn_h}, "Scr", "Screenshot (F12)", false, false, true, false};
    btn_x += 35 + 10;
    /* Canvas zoom on the toolbar. The wheel and +/- already do this; a trackpad that
       reports no wheel, or a hand already on the mouse, has nothing to reach for. */
    ui->btn_zoom_out = (Button){{btn_x, 10, 24, btn_h}, "-", "Zoom out (-)", false, false, true, false};
    btn_x += 24 + 4;
    ui->btn_zoom_in  = (Button){{btn_x, 10, 24, btn_h}, "+", "Zoom in (+)", false, false, true, false};
    btn_x += 24 + 4;
    ui->btn_zoom_fit = (Button){{btn_x, 10, 32, btn_h}, "Fit", "Zoom to fit the whole circuit", false, false, true, false};
    btn_x += 32 + 10;
    ui->btn_import_spice = (Button){{btn_x, 10, 46, btn_h}, "SPICE", "Import a vendor .SUBCKT model", false, false, true, false};

    // Speed slider. The x and w here are placeholders; ui_layout_toolbar_right sets them from
    // the window width, at start-up and again on every resize.
    ui->speed_slider = (Rect){0, 15, 100, 20};
    ui->speed_label_w = 52;
    ui->speed_value = 1.0f;

    // Time step controls - right-aligned with the speed slider, see ui_layout_toolbar_right
    ui->btn_timestep_down = (Button){{0, 12, 20, 20}, "-", "Decrease time step", false, false, true, false};
    ui->btn_timestep_up = (Button){{0, 12, 20, 20}, "+", "Increase time step", false, false, true, false};
    ui->btn_timestep_auto = (Button){{0, 10, 40, 24}, "Auto", "Auto time step", false, false, true, false};
    ui->btn_update = (Button){{0, 0, 0, 0}, "Update", "A newer release is available - click to download and install", false, false, true, false};
    ui->display_time_step = 1e-7;  // Default 100 nanoseconds (will be updated from simulation)
    /* Position the right-hand group now. ui_update_layout runs on resize, but nothing calls it at
       start-up, so without this the placeholders above would be what the first frame draws. */
    ui_layout_toolbar_right(ui);

    // Environment sliders (positioned in status bar area - will be updated in ui_update_layout)
    // These control global light/temperature for LDR and thermistor components
    ui->env_light_slider = (Rect){0, 0, 80, 14};   // Will be positioned in render
    ui->env_temp_slider = (Rect){0, 0, 80, 14};    // Will be positioned in render
    ui->brightness_slider = (Rect){-100, -100, 0, 0};
    ui->brightness = 1.0f;
    ui->dragging_light = false;
    ui->dragging_temp = false;

    // Initialize palette items
    int pal_y = TOOLBAR_HEIGHT + 18;
    int pal_h = 35;
    int col = 0;

    // Helper macros to add a palette item. Every item records the category it was added
    // under (cur_cat), so the layout and hit-test never depend on list positions.
    PaletteCategoryID cur_cat = PCAT_TOOLS;
    #define ADD_TOOL(tool, lbl) do { \
        ui->palette_items[ui->num_palette_items++] = (PaletteItem){ \
            {10 + col*70, pal_y, 60, pal_h}, COMP_NONE, tool, true, lbl, false, (tool == TOOL_SELECT), cur_cat \
        }; \
        col++; if (col >= 2) { col = 0; pal_y += pal_h + 3; } \
    } while(0)

    #define ADD_COMP(comp, lbl) do { \
        ui->palette_items[ui->num_palette_items++] = (PaletteItem){ \
            {10 + col*70, pal_y, 60, pal_h}, comp, TOOL_COMPONENT, false, lbl, false, false, cur_cat \
        }; \
        col++; if (col >= 2) { col = 0; pal_y += pal_h + 3; } \
    } while(0)

    #define NEW_SECTION(cat) do { col = 0; pal_y += pal_h + 15; cur_cat = (cat); } while(0)

    // === TOOLS SECTION (index 0) ===
    pal_y += 4;
    ADD_TOOL(TOOL_SELECT, "Select");
    ADD_TOOL(TOOL_WIRE, "Wire");
    ADD_TOOL(TOOL_DELETE, "Delete");
    ADD_TOOL(TOOL_PROBE, "Probe");
    ADD_TOOL(TOOL_PAN, "Pan");
    ADD_COMP(COMP_TEXT, "Text");

    // === SOURCES SECTION (index 5) ===
    NEW_SECTION(PCAT_SOURCES);
    ADD_COMP(COMP_GROUND, "GND");
    ADD_COMP(COMP_DC_VOLTAGE, "DC V");
    ADD_COMP(COMP_AC_VOLTAGE, "AC V");
    ADD_COMP(COMP_ARB_SOURCE, "ARB");
    ADD_COMP(COMP_DC_CURRENT, "DC I");
    ADD_COMP(COMP_AC_CURRENT, "AC I");
    ADD_COMP(COMP_CLOCK, "Clock");

    // === WAVEFORMS SECTION (index 11) ===
    NEW_SECTION(PCAT_WAVEFORMS);
    ADD_COMP(COMP_SQUARE_WAVE, "Square");
    ADD_COMP(COMP_TRIANGLE_WAVE, "Tri");
    ADD_COMP(COMP_SAWTOOTH_WAVE, "Saw");
    ADD_COMP(COMP_NOISE_SOURCE, "Noise");
    ADD_COMP(COMP_PULSE_SOURCE, "Pulse");
    ADD_COMP(COMP_PWM_SOURCE, "PWM");

    // === PASSIVES SECTION (index 17) ===
    NEW_SECTION(PCAT_PASSIVES);
    ADD_COMP(COMP_RESISTOR, "R");
    ADD_COMP(COMP_LOAD_HP, "R_HP");
    ADD_COMP(COMP_CAPACITOR, "C");
    ADD_COMP(COMP_CAPACITOR_ELEC, "Elec");
    ADD_COMP(COMP_INDUCTOR, "L");
    ADD_COMP(COMP_POTENTIOMETER, "Pot");
    ADD_COMP(COMP_CRYSTAL, "Xtal");
    ADD_COMP(COMP_FUSE, "Fuse");
    ADD_COMP(COMP_THERMISTOR, "Therm");
    ADD_COMP(COMP_SPARK_GAP, "Spark");
    ADD_COMP(COMP_TOROID, "Toroid");
    ADD_COMP(COMP_TLINE, "TLine");
    ADD_COMP(COMP_DELAY_LINE, "Delay");
    ADD_COMP(COMP_SOURCE_3PH, "3ph~");

    // === DIODES SECTION (index 25) ===
    NEW_SECTION(PCAT_DIODES);
    ADD_COMP(COMP_DIODE, "Diode");
    ADD_COMP(COMP_ZENER, "Zener");
    ADD_COMP(COMP_SCHOTTKY, "Schky");
    ADD_COMP(COMP_LED, "LED");
    ADD_COMP(COMP_VARACTOR, "Varac");
    ADD_COMP(COMP_PHOTODIODE, "Photo");

    // === TRANSISTORS - BJT SECTION (index 31) ===
    NEW_SECTION(PCAT_BJT);
    ADD_COMP(COMP_NPN_BJT, "NPN");
    ADD_COMP(COMP_PNP_BJT, "PNP");
    ADD_COMP(COMP_NPN_DARLINGTON, "NPN-D");
    ADD_COMP(COMP_PNP_DARLINGTON, "PNP-D");

    // === TRANSISTORS - FET SECTION (index 35) ===
    NEW_SECTION(PCAT_FET);
    ADD_COMP(COMP_NMOS, "NMOS");
    ADD_COMP(COMP_PMOS, "PMOS");
    ADD_COMP(COMP_NJFET, "NJFET");
    ADD_COMP(COMP_PJFET, "PJFET");

    // === THYRISTORS SECTION (index 39) ===
    NEW_SECTION(PCAT_THYRISTORS);
    ADD_COMP(COMP_SCR, "SCR");
    ADD_COMP(COMP_DIAC, "DIAC");
    ADD_COMP(COMP_TRIAC, "TRIAC");
    ADD_COMP(COMP_UJT, "UJT");

    // === OP-AMPS & AMPLIFIERS SECTION (index 43) ===
    NEW_SECTION(PCAT_OPAMPS);
    ADD_COMP(COMP_OPAMP, "OpAmp");
    ADD_COMP(COMP_OPAMP_FLIPPED, "OpFlip");
    ADD_COMP(COMP_OPAMP_REAL, "OpReal");
    ADD_COMP(COMP_OTA, "OTA");

    // === CONTROLLED SOURCES SECTION (index 47) ===
    NEW_SECTION(PCAT_CONTROLLED);
    ADD_COMP(COMP_VCVS, "VCVS");
    ADD_COMP(COMP_VCCS, "VCCS");
    ADD_COMP(COMP_CCVS, "CCVS");
    ADD_COMP(COMP_CCCS, "CCCS");

    // === SWITCHES SECTION (index 51) ===
    NEW_SECTION(PCAT_SWITCHES);
    ADD_COMP(COMP_SPST_SWITCH, "SPST");
    ADD_COMP(COMP_SPDT_SWITCH, "SPDT");
    ADD_COMP(COMP_DPDT_SWITCH, "DPDT");
    ADD_COMP(COMP_PUSH_BUTTON, "PushB");
    ADD_COMP(COMP_RELAY, "Relay");
    ADD_COMP(COMP_ANALOG_SWITCH, "AnaSw");

    // === TRANSFORMERS SECTION (index 57) ===
    NEW_SECTION(PCAT_TRANSFORMERS);
    ADD_COMP(COMP_TRANSFORMER, "Xfmr");
    ADD_COMP(COMP_TRANSFORMER_CT, "XfmrCT");

    // === LOGIC GATES SECTION (index 59) ===
    NEW_SECTION(PCAT_LOGIC);
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
    NEW_SECTION(PCAT_DIGITAL);
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
    NEW_SECTION(PCAT_MIXED);
    ADD_COMP(COMP_DAC, "DAC");
    ADD_COMP(COMP_ADC, "ADC");
    ADD_COMP(COMP_VCO, "VCO");
    ADD_COMP(COMP_PLL, "PLL");
    ADD_COMP(COMP_MONOSTABLE, "Mono");
    ADD_COMP(COMP_OPTOCOUPLER, "Opto");

    // === VOLTAGE REGULATORS SECTION (index 85) ===
    NEW_SECTION(PCAT_REGULATORS);
    ADD_COMP(COMP_LM317, "LM317");
    ADD_COMP(COMP_7805, "7805");
    ADD_COMP(COMP_TL431, "TL431");

    // === DISPLAY/OUTPUT SECTION (index 88) ===
    NEW_SECTION(PCAT_DISPLAY);
    ADD_COMP(COMP_7SEG_DISPLAY, "7Seg");
    ADD_COMP(COMP_LED_ARRAY, "LEDBar");
    ADD_COMP(COMP_LED_MATRIX, "8x8");
    ADD_COMP(COMP_DC_MOTOR, "Motor");

    // === WIRELESS SECTION ===
    // TX and RX are not displays. TX publishes the voltage across itself on a channel number
    // and RX, set to the same channel, reproduces it - see the Wireless Link template.
    NEW_SECTION(PCAT_WIRELESS);
    ADD_COMP(COMP_ANTENNA_TX, "TX");
    ADD_COMP(COMP_ANTENNA_RX, "RX");

    // === SUB-CIRCUITS SECTION ===
    NEW_SECTION(PCAT_SUBPARTS);
    ADD_COMP(COMP_PIN, "Pin");
    ADD_COMP(COMP_SUBCIRCUIT, "IC");
    ADD_COMP(COMP_BUS, "Bus");
    ADD_COMP(COMP_BUS_TAP, "Tap");
    ADD_COMP(COMP_LAMP, "Lamp");

    // === MEASUREMENT SECTION (index 93) ===
    NEW_SECTION(PCAT_MEASUREMENT);
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

    for (int g = 0; g < TG_COUNT; g++) ui->circuit_group_collapsed[g] = true;

    // User subcircuits (dynamically updated from g_subcircuit_library)
    ui->num_subcircuit_items = 0;
    ui->selected_subcircuit_def_id = -1;
    ui->placing_subcircuit = false;
    ui->subcircuit_editing_def_id = -1;  // -1 = creating new

    // The Circuits palette is generated from circuits.c: every template appears, grouped by
    // TemplateGroup, labelled with its short_name. Bounds are laid out in ui_render_palette.
    for (int g = 0; g < TG_COUNT; g++) {
        for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
            const CircuitTemplateInfo *info = circuit_template_get_info((CircuitTemplateType)t);
            if (!info || (int)info->group != g) continue;
            if (ui->num_circuit_items >= (int)(sizeof ui->circuit_items / sizeof ui->circuit_items[0])) break;
            ui->circuit_items[ui->num_circuit_items++] = (CircuitPaletteItem){
                {0, 0, 60, pal_h}, t, info->short_name, false, false, g
            };
        }
    }
    (void)col;

    // Calculate palette content height (from toolbar to last item + padding)
    ui->palette_content_height = pal_y + pal_h + 10 - TOOLBAR_HEIGHT;
    ui->palette_scroll_offset = 0;
    ui->palette_visible_height = WINDOW_HEIGHT - TOOLBAR_HEIGHT - PALETTE_TOP_H - STATUSBAR_HEIGHT;
    ui->palette_scrolling = false;

    // Oscilloscope settings - larger default size for better visibility. The scope starts
    // higher and taller than the properties list needs; the clamp in ui_update_layout still
    // shrinks it on a small window so its three button rows always clear the status bar.
    ui->scope_rect = (Rect){WINDOW_WIDTH - ui->properties_width + 10, 220, 330, 375};
    ui->scope_default_h = ui->scope_rect.h;
    ui->scope_num_channels = 0;
    ui->scope_time_div = 0.001;   // 1ms per division
    ui->scope_volt_div = 1.0;     // 1V per division
    ui->scope_selected_channel = 0;
    ui->scope_paused = false;
    ui->scope_resizing = false;
    ui->scope_resize_edge = -1;
    ui->scope_controls_scroll = 0;
    ui->scope_controls_content_height = 0;
    ui->scope_controls_visible_height = 0;
    ui->scope_controls_scrolling = false;

    // Initialize all channels with predefined colors
    for (int i = 0; i < MAX_PROBES; i++) {
        ui->scope_channels[i] = (ScopeChannel){false, PROBE_COLORS[i], i, 0.0};
    }

    // Oscilloscope control buttons - initialized with default positions
    // Actual positions are recalculated in ui_update_layout with auto-wrapping
    int scope_btn_y = ui->scope_rect.y + ui->scope_rect.h + 5;
    int scope_btn_w = 32, scope_btn_h = 22;
    int scope_btn_x = ui->scope_rect.x;
    int row_spacing = scope_btn_h + 4;

    // Row 1: Scale controls
    ui->btn_scope_volt_up = (Button){{scope_btn_x, scope_btn_y, scope_btn_w, scope_btn_h}, "V+", "Increase V/div", false, false, true, false};
    scope_btn_x += scope_btn_w + 3;
    ui->btn_scope_volt_down = (Button){{scope_btn_x, scope_btn_y, scope_btn_w, scope_btn_h}, "V-", "Decrease V/div", false, false, true, false};
    scope_btn_x += scope_btn_w + 10;
    ui->btn_scope_time_up = (Button){{scope_btn_x, scope_btn_y, scope_btn_w, scope_btn_h}, "T+", "Increase time/div", false, false, true, false};
    scope_btn_x += scope_btn_w + 3;
    ui->btn_scope_time_down = (Button){{scope_btn_x, scope_btn_y, scope_btn_w, scope_btn_h}, "T-", "Decrease time/div", false, false, true, false};
    scope_btn_x += scope_btn_w + 10;
    ui->scope_scale_all = true;
    ui->btn_scope_ch_all = (Button){{0, 0, 0, 0}, "ALL", "V+/V- and the wheel move every channel", false, false, true};
    for (int ch = 0; ch < MAX_PROBES; ch++)
        ui->btn_scope_ch[ch] = (Button){{0, 0, 0, 0}, "", "Give this channel its own volts/div", false, false, true};
    ui->btn_scope_autoset = (Button){{scope_btn_x, scope_btn_y, 60, scope_btn_h}, "Autoset", "Auto-configure scope settings", false, false, true, false};

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
    ui->btn_scope_stack = (Button){{scope_btn_x, scope_btn_y, 40, scope_btn_h}, "Stack", "Stacked view: one band per channel (toggle overlay)", false, false, true, false};
    ui->btn_scope_ac = (Button){{0, 0, 0, 0}, "AC", "AC coupling: draw each trace minus its DC level (readouts stay DC)", false, false, true, false};
    ui->btn_scope_fit = (Button){{0, 0, 0, 0}, "Fit", "Fit (stacked view): scale every band to its own signal, centred on its mean", false, false, true, false};
    scope_btn_x += 43;
    ui->btn_scope_track = (Button){{scope_btn_x, scope_btn_y, 32, scope_btn_h}, "Trk", "Track a sweeping source: time/div follows its frequency (~3 cycles per screen)", false, false, true, false};
    scope_btn_x += 35;
    ui->btn_scope_screenshot = (Button){{scope_btn_x, scope_btn_y, 35, scope_btn_h}, "CAP", "Capture screenshot (saves scope.bmp)", false, false, true, false};
    scope_btn_x += 38;
    ui->btn_bode = (Button){{scope_btn_x, scope_btn_y, 40, scope_btn_h}, "Bode", "Frequency response plot", false, false, true, false};
    scope_btn_x += 43;
    ui->btn_mc = (Button){{scope_btn_x, scope_btn_y, 25, scope_btn_h}, "MC", "Monte Carlo statistical analysis", false, false, true, false};
    scope_btn_x += 28;
    ui->btn_scope_popup = (Button){{scope_btn_x, scope_btn_y, 50, scope_btn_h}, "PopOut", "Pop out oscilloscope to separate window", false, false, true, false};
    ui->btn_scope_tab[0] = (Button){{0, 0, 0, 0}, "Display", "Y-T/X-Y mode, capture", false, false, true, false};
    ui->btn_scope_tab[1] = (Button){{0, 0, 0, 0}, "Trigger", "Trigger mode, edge, source, level", false, false, true, false};
    ui->btn_scope_tab[2] = (Button){{0, 0, 0, 0}, "Analysis", "FFT, Bode plot, Monte Carlo", false, false, true, false};
    ui->scope_ctl_tab = 0;

    // Initialize cursor state
    ui->scope_cursor_mode = false;
    ui->scope_cursor_drag = 0;
    ui->cursor1_time = 0.25;
    ui->cursor2_time = 0.75;
    ui->cursor1_volt = 0.35;
    ui->cursor2_volt = 0.65;
    ui->scope_cursor_type = 0;
    ui->scope_cursor_active = 1;
    ui->scope_cursor_linked = false;
    ui->cursor_a_channel = -1;
    ui->cursor_b_channel = -1;
    ui->scope_view_t0 = 0.0;
    ui->scope_view_span = 0.0;
    ui->scope_fft_mode = false;
    ui->scope_stacked = false;
    ui->scope_track_sweep = false;
    ui->scope_extra_w = 0;
    ui->scope_user_sized = false;

    // Initialize trigger settings
    ui->trigger_mode = TRIG_AUTO;
    ui->trigger_edge = TRIG_EDGE_RISING;
    ui->trigger_channel = 0;
    ui->trigger_level = 0.0;
    ui->trigger_armed = true;
    ui->triggered = false;
    ui->trigger_holdoff = 0.001;  // 1ms holdoff
    ui->dragging_trigger_level = false;
    ui->trigger_position = 0.5;  // Center of screen
    ui->dragging_trigger_position = false;

    // Initialize pop-out oscilloscope window
    ui->scope_knob_active = -1;
    ui->scope_knob_hover = -1;
    ui->scope_knob_last_x = 0;
    ui->scope_knob_last_y = 0;
    ui->scope_panel_active = false;
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
    // Lay everything out once for the initial window size (otherwise the scope buttons keep
    // their bootstrap positions and the info rows are drawn at y = 0 until the first resize)
    ui_update_layout(ui);
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

        // Extract voltmeter and ammeter readings from circuit components
        for (int i = 0; i < circuit->num_components; i++) {
            Component *comp = circuit->components[i];
            if (!comp) continue;
            if (comp->type == COMP_VOLTMETER) {
                ui->voltmeter_value = comp->props.voltmeter.reading;
            } else if (comp->type == COMP_AMMETER) {
                ui->ammeter_value = comp->props.ammeter.reading;
            }
        }
    }
}

static void draw_button(SDL_Renderer *r, Button *btn) {
    if (btn->bounds.w <= 0 || btn->bounds.h <= 0) return;   // hidden (inactive scope tab)
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
    SDL_Rect rect = {item->bounds.x, item->bounds.y, item->bounds.w, item->bounds.h};

    // Neon glow effect for selected items
    if (item->selected) {
        // Animation time using SDL_GetTicks (convert to seconds)
        double anim_time = SDL_GetTicks() / 1000.0;
        double pulse = 0.5 + 0.5 * sin(anim_time * 3.0);

        // Enable additive blending for neon glow
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);

        // Draw outer glow layers (pink/magenta)
        for (int layer = 5; layer >= 1; layer--) {
            int expand = layer * 2;
            uint8_t alpha = (uint8_t)(pulse * (15 + (6 - layer) * 18));
            if (alpha > 255) alpha = 255;

            SDL_SetRenderDrawColor(r, SYNTH_PINK, alpha);
            SDL_Rect glow_rect = {
                rect.x - expand,
                rect.y - expand,
                rect.w + expand * 2,
                rect.h + expand * 2
            };
            SDL_RenderDrawRect(r, &glow_rect);
        }

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    }

    // Background - synthwave colors
    if (item->selected) {
        SDL_SetRenderDrawColor(r, SYNTH_PINK, 0x60);
    } else if (item->hovered) {
        SDL_SetRenderDrawColor(r, SYNTH_PURPLE, 0x40);
    } else {
        SDL_SetRenderDrawColor(r, SYNTH_BG_MID, 0xff);
    }
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
    SDL_Rect rect = {item->bounds.x, item->bounds.y, item->bounds.w, item->bounds.h};

    // Neon glow effect for selected items (green theme for circuits)
    if (item->selected) {
        // Animation time using SDL_GetTicks (convert to seconds)
        double anim_time = SDL_GetTicks() / 1000.0;
        double pulse = 0.5 + 0.5 * sin(anim_time * 3.0);

        // Enable additive blending for neon glow
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_ADD);

        // Draw outer glow layers (green)
        for (int layer = 5; layer >= 1; layer--) {
            int expand = layer * 2;
            uint8_t alpha = (uint8_t)(pulse * (15 + (6 - layer) * 18));
            if (alpha > 255) alpha = 255;

            SDL_SetRenderDrawColor(r, SYNTH_GREEN, alpha);
            SDL_Rect glow_rect = {
                rect.x - expand,
                rect.y - expand,
                rect.w + expand * 2,
                rect.h + expand * 2
            };
            SDL_RenderDrawRect(r, &glow_rect);
        }

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    }

    // Background - synthwave green tint for circuit templates
    if (item->selected) {
        SDL_SetRenderDrawColor(r, SYNTH_GREEN, 0x40);
    } else if (item->hovered) {
        SDL_SetRenderDrawColor(r, SYNTH_GREEN, 0x20);
    } else {
        SDL_SetRenderDrawColor(r, SYNTH_BG_MID, 0xff);
    }
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

    // Title - hot pink, with the version dim underneath it. The window title carries it too,
    // which is what the taskbar shows; this is for when the window is already in front of you.
    SDL_SetRenderDrawColor(renderer, SYNTH_PINK, 0xff);
    ui_draw_text(renderer, "Circuit Playground", 10, 20);
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DIM, 0xff);
    ui_draw_text(renderer, "v" APP_VERSION, 10, 32);

    // Buttons
    draw_button(renderer, &ui->btn_run);
    draw_button(renderer, &ui->btn_pause);
    draw_button(renderer, &ui->btn_step);
    draw_button(renderer, &ui->btn_reset);
    draw_button(renderer, &ui->btn_clear);
    draw_button(renderer, &ui->btn_save);
    draw_button(renderer, &ui->btn_load);
    draw_button(renderer, &ui->btn_export_svg);
    draw_button(renderer, &ui->btn_screenshot);
    draw_button(renderer, &ui->btn_zoom_out);
    draw_button(renderer, &ui->btn_zoom_in);
    draw_button(renderer, &ui->btn_zoom_fit);
    draw_button(renderer, &ui->btn_import_spice);

    // Speed slider label - dropped on a narrow window so the controls right of it still fit
    if (ui->speed_label_w > 0) {
        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT, 0xff);
        ui_draw_text(renderer, "Speed:", ui->speed_slider.x, ui->speed_slider.y - 2);
    }

    // Speed slider background
    int slider_x = ui->speed_slider.x + ui->speed_label_w;
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
    /* Amber when the stepper is not keeping up with what the slider asked for.
     *
     * The speed control asks the stepper to advance delta_time * speed of circuit time each
     * frame. It never changes dt - every step is a full step at the same accuracy - so turning it
     * up cannot make an answer wrong. What it can do is fail: the frame gives up after 12 ms of
     * wall clock so the interface stays alive, and on a heavy circuit that means fewer steps than
     * were asked for. app->sim_realtime_ratio has measured exactly that all along, and the field
     * it is copied into carries the comment "shown next to speed" - and nothing drew it. So the
     * toolbar showed the request as though it were the fact, and a circuit crawling at a twentieth
     * of real time looked identical to one keeping up.
     *
     * The number itself does not fit: between the speed text and the dt label there is room for
     * about four characters, and the first attempt at "(0.04x)" was drawn straight across
     * "dt:200ps". So the signal is the colour, which costs no space and answers the question that
     * matters - am I seeing this in real time or not. app.c parks the ratio at 1.0 while the
     * simulation is not running, so a pause does not leave the toolbar amber. */
    if (ui->sim_realtime_ratio < 0.95) SDL_SetRenderDrawColor(renderer, 0xff, 0xa0, 0x30, 0xff);
    else                               SDL_SetRenderDrawColor(renderer, SYNTH_GREEN, 0xff);
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
    } else if (dt >= 1e-9) {
        snprintf(dt_text, sizeof(dt_text), "%.1fns", dt * 1e9);
    } else {
        /* the floor is 10 ps, and "%.0fns" showed a 200 ps step as "dt:0ns" */
        snprintf(dt_text, sizeof(dt_text), "%.0fps", dt * 1e12);
    }
    SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0xff);
    ui_draw_text(renderer, dt_text, ui->timestep_display_x + 24, 17);

    // Time step buttons
    draw_button(renderer, &ui->btn_timestep_down);
    draw_button(renderer, &ui->btn_timestep_up);
    draw_button(renderer, &ui->btn_timestep_auto);
    if (ui->btn_update.bounds.w > 0) { ui->btn_update.toggled = true; draw_button(renderer, &ui->btn_update); }

    // Toolbar border
    SDL_SetRenderDrawColor(renderer, SYNTH_BORDER, 0xff);
    SDL_RenderDrawLine(renderer, 0, TOOLBAR_HEIGHT - 1, ui->window_width, TOOLBAR_HEIGHT - 1);
}

// Case-insensitive substring match of the palette filter against a label (empty filter = match)
static bool palette_filter_match(const UIState *ui, const char *label, const char *label2, const char *label3) {
    if (!ui->palette_filter[0]) return true;
    char f[32], l[96];
    size_t i;
    for (i = 0; ui->palette_filter[i] && i < sizeof f - 1; i++) f[i] = (char)tolower((unsigned char)ui->palette_filter[i]);
    f[i] = 0;
    for (int pass = 0; pass < 3; pass++) {
        const char *src = pass == 2 ? label3 : pass ? label2 : label;
        if (!src) continue;
        for (i = 0; src[i] && i < sizeof l - 1; i++) l[i] = (char)tolower((unsigned char)src[i]);
        l[i] = 0;
        if (strstr(l, f)) return true;
    }
    return false;
}

// Tab strip + filter box at the top of the left panel
/* A palette section header, drawn as something you can obviously press: a filled bar across the
   panel with a border and a triangle, rather than a line of coloured text that happens to be
   clickable. PAL_HEADER_H is the row it occupies - the draw, the layout and the hit test all
   take it from here, so a taller header cannot drift away from where the clicks land. */
static void draw_palette_header(SDL_Renderer *renderer, int screen_y, const char *label,
                                bool collapsed, bool accent, int indent) {
    SDL_Rect bar = { 2 + indent, screen_y, PALETTE_WIDTH - 14 - indent, PAL_HEADER_H - 3 };
    SDL_SetRenderDrawColor(renderer, collapsed ? 0x1c : 0x2e, collapsed ? 0x10 : 0x16,
                                     collapsed ? 0x32 : 0x50, 0xff);
    SDL_RenderFillRect(renderer, &bar);
    SDL_SetRenderDrawColor(renderer, accent ? SYNTH_PINK : SYNTH_BORDER_LIGHT, 0xff);
    SDL_RenderDrawRect(renderer, &bar);

    /* the triangle: right when it is closed, down when it is open */
    int tx = bar.x + 7, ty = bar.y + bar.h / 2;
    SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0xff);
    if (collapsed) {
        for (int i = 0; i < 5; i++)
            SDL_RenderDrawLine(renderer, tx + i, ty - 4 + i, tx + i, ty + 4 - i);
    } else {
        for (int i = 0; i < 5; i++)
            SDL_RenderDrawLine(renderer, tx - 4 + i, ty + i - 2, tx + 4 - i, ty + i - 2);
    }

    SDL_SetRenderDrawColor(renderer, accent ? SYNTH_PINK : SYNTH_TEXT, 0xff);
    /* The bar is the label's whole world now. "Interview: instrumentation & scope" is 34
       characters at 8 px each - two hundred and seventy pixels in a hundred and thirty pixel
       panel - and it used to run straight out over the canvas. Anything that does not fit is
       cut and marked with a pair of dots so it reads as shortened rather than as a typo. */
    int text_x = bar.x + 16;
    int room = (bar.x + bar.w - 2 - text_x) / 8;
    if (room < 4) room = 4;
    if ((int)strlen(label) <= room) {
        ui_draw_text(renderer, label, text_x, bar.y + (bar.h - 8) / 2);
    } else {
        char cut[64];
        int keep = room - 2; if (keep > (int)sizeof cut - 3) keep = (int)sizeof cut - 3;
        memcpy(cut, label, (size_t)keep);
        cut[keep] = '.'; cut[keep + 1] = '.'; cut[keep + 2] = '\0';
        ui_draw_text(renderer, cut, text_x, bar.y + (bar.h - 8) / 2);
    }
}

static void draw_palette_tabs(UIState *ui, SDL_Renderer *renderer) {
    /* Two buttons, not two words. They are the first thing anyone touches and they were 18 px
       of text with the label centred on the wrong glyph width, so the whole strip read as a
       caption rather than a control. */
    const char *names[LTAB_COUNT] = { "Parts", "Circuits" };
    int tab_w = (PALETTE_WIDTH - 14) / LTAB_COUNT;
    for (int t = 0; t < LTAB_COUNT; t++) {
        SDL_Rect r = { 2 + t * (tab_w + 2), TOOLBAR_HEIGHT + 3, tab_w, 25 };
        bool active = (ui->left_tab == t);
        SDL_SetRenderDrawColor(renderer, active ? 0x4a : 0x18, active ? 0x22 : 0x0e,
                                         active ? 0x70 : 0x28, 0xff);
        SDL_RenderFillRect(renderer, &r);
        /* the active one gets a brighter border and a thicker underline, so which tab you are
           on is legible at a glance rather than by comparing two dark fills */
        SDL_SetRenderDrawColor(renderer, active ? SYNTH_CYAN : SYNTH_BORDER_LIGHT, 0xff);
        SDL_RenderDrawRect(renderer, &r);
        if (active) {
            SDL_Rect under = { r.x + 1, r.y + r.h - 3, r.w - 2, 2 };
            SDL_RenderFillRect(renderer, &under);
        }
        int tx = r.x + (r.w - (int)strlen(names[t]) * 8) / 2;     /* the font advances 8 */
        SDL_SetRenderDrawColor(renderer, active ? SYNTH_CYAN : SYNTH_TEXT_DIM, 0xff);
        ui_draw_text(renderer, names[t], tx, r.y + (r.h - 8) / 2);
    }
    // filter box
    SDL_Rect box = { 4, TOOLBAR_HEIGHT + 31, PALETTE_WIDTH - 18, 19 };
    SDL_SetRenderDrawColor(renderer, 0x0c, 0x08, 0x18, 0xff);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, ui->palette_filter_active ? SYNTH_CYAN : SYNTH_BORDER, 0xff);
    SDL_RenderDrawRect(renderer, &box);
    if (ui->palette_filter[0]) {
        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT, 0xff);
        ui_draw_text(renderer, ui->palette_filter, box.x + 4, box.y + 4);
        if (ui->palette_filter_active && (SDL_GetTicks() / 500) % 2 == 0) {
            int cx = box.x + 4 + (int)strlen(ui->palette_filter) * 6;
            SDL_RenderDrawLine(renderer, cx, box.y + 3, cx, box.y + 13);
        }
    } else {
        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DIM, 0xff);
        ui_draw_text(renderer, ui->palette_filter_active ? "type to filter" : "filter...", box.x + 4, box.y + 4);
    }
}

void ui_render_palette(UIState *ui, SDL_Renderer *renderer) {
    // Palette background - synthwave dark
    SDL_SetRenderDrawColor(renderer, SYNTH_BG_DARK, 0xff);
    SDL_Rect palette = {0, TOOLBAR_HEIGHT, PALETTE_WIDTH, ui->window_height - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT};
    SDL_RenderFillRect(renderer, &palette);

    draw_palette_tabs(ui, renderer);

    // Set clipping rect for palette content (below the tab strip, excluding scrollbar area)
    SDL_Rect clip = {0, TOOLBAR_HEIGHT + PALETTE_TOP_H, PALETTE_WIDTH - 10, ui->window_height - TOOLBAR_HEIGHT - PALETTE_TOP_H - STATUSBAR_HEIGHT};
    SDL_RenderSetClipRect(renderer, &clip);

    int scroll_offset = ui->palette_scroll_offset;

    // Component sections are the categories in enum order up to (not including) Circuits;
    // every item is drawn under item->category (see ui_init).
    int num_sections = (int)PCAT_CIRCUITS;

    // Calculate dynamic positions and draw
    int pal_h = 35;  // Item height
    int draw_y = TOOLBAR_HEIGHT + PALETTE_TOP_H + 4;  // Starting y position (content coords, not screen)
    int content_height = 4;  // Track total content height

    for (int s = 0; s < num_sections && ui->left_tab == LTAB_PARTS; s++) {
        PaletteCategoryID cat_id = (PaletteCategoryID)s;
        PaletteCategory *cat = &ui->categories[cat_id];
        bool collapsed = cat->collapsed;

        // Store header y position for click detection (in content coords)
        cat->header_y = draw_y;

        // Draw header
        int header_screen_y = draw_y - scroll_offset;
        if (header_screen_y >= TOOLBAR_HEIGHT - 14 && header_screen_y < ui->window_height - STATUSBAR_HEIGHT) {
            draw_palette_header(renderer, header_screen_y, cat->name, collapsed, false, 0);
        }
        draw_y += PAL_HEADER_H;
        content_height += PAL_HEADER_H;

        if (!collapsed) {
            // Draw items in this section
            int col = 0;
            for (int i = 0; i < ui->num_palette_items; i++) {
                PaletteItem *item = &ui->palette_items[i];
                if (item->category != cat_id) continue;
                if (!palette_filter_match(ui, item->label, item->is_tool ? NULL : component_get_info(item->comp_type)->name, item->is_tool ? NULL : component_search_keywords(item->comp_type))) {
                    item->bounds.w = 0;   // filtered out: unreachable by hit-test
                    continue;
                }
                item->bounds.w = 60;

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
    if (ui->left_tab == LTAB_CIRCUITS && ui->num_circuit_items > 0) {
        circuits_cat->header_y = draw_y;

        int header_screen_y = draw_y - scroll_offset;
        if (header_screen_y >= TOOLBAR_HEIGHT - 14 && header_screen_y < ui->window_height - STATUSBAR_HEIGHT) {
            draw_palette_header(renderer, header_screen_y, "Circuits", circuits_cat->collapsed, true, 0);
        }
        draw_y += PAL_HEADER_H;
        content_height += PAL_HEADER_H;

        if (!circuits_cat->collapsed) {
            int col = 0, cur_group = -1;
            for (int g = 0; g < 16; g++) ui->circuit_group_header_y[g] = 0;
            for (int i = 0; i < ui->num_circuit_items; i++) {
                CircuitPaletteItem *item = &ui->circuit_items[i];

                if (item->group != cur_group) {
                    // group sub-header (click to collapse the group)
                    if (col > 0) { draw_y += pal_h + 5; content_height += pal_h + 5; col = 0; }
                    cur_group = item->group;
                    ui->circuit_group_header_y[cur_group] = draw_y;
                    int hy = draw_y - scroll_offset;
                    if (hy >= TOOLBAR_HEIGHT - PAL_HEADER_H && hy < ui->window_height - STATUSBAR_HEIGHT) {
                        /* the same pressable bar as every other section, indented one step so it
                           still reads as living inside Circuits */
                        draw_palette_header(renderer, hy, circuit_template_group_name((TemplateGroup)cur_group),
                                            ui->circuit_group_collapsed[cur_group], false, 6);
                    }
                    draw_y += PAL_HEADER_H; content_height += PAL_HEADER_H;
                }
                if (ui->circuit_group_collapsed[cur_group]) { item->bounds.w = 0; item->bounds.h = 0; continue; }
                {
                    const CircuitTemplateInfo *tinfo = circuit_template_get_info((CircuitTemplateType)item->circuit_type);
                    if (!palette_filter_match(ui, item->label, tinfo ? tinfo->name : NULL, tinfo ? tinfo->description : NULL)) { item->bounds.w = 0; item->bounds.h = 0; continue; }
                }

                item->bounds.x = 10 + col * 70;
                item->bounds.y = draw_y;
                item->bounds.w = 60;
                item->bounds.h = pal_h;

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

    // My Circuits (user subcircuits) section
    PaletteCategory *subcircuits_cat = &ui->categories[PCAT_SUBCIRCUITS];

    // Update subcircuit items from g_subcircuit_library (shared with ui_update_layout)
    ui_sync_subcircuit_items(ui);

    if (ui->left_tab == LTAB_CIRCUITS && ui->num_subcircuit_items > 0) {
        subcircuits_cat->header_y = draw_y;

        int header_screen_y = draw_y - scroll_offset;
        if (header_screen_y >= TOOLBAR_HEIGHT - 14 && header_screen_y < ui->window_height - STATUSBAR_HEIGHT) {
            // Draw collapse indicator
            draw_palette_header(renderer, header_screen_y, "My Circuits", subcircuits_cat->collapsed, true, 0);
        }
        draw_y += PAL_HEADER_H;
        content_height += PAL_HEADER_H;

        if (!subcircuits_cat->collapsed) {
            int col = 0;
            for (int i = 0; i < ui->num_subcircuit_items; i++) {
                SubcircuitPaletteItem *item = &ui->subcircuit_items[i];

                item->bounds.x = 10 + col * 70;
                item->bounds.y = draw_y;

                int screen_y = draw_y - scroll_offset;
                if (screen_y + item->bounds.h >= TOOLBAR_HEIGHT && screen_y < ui->window_height - STATUSBAR_HEIGHT) {
                    // Draw subcircuit item
                    Rect bounds = {item->bounds.x, screen_y, item->bounds.w, item->bounds.h};

                    // Background
                    if (item->selected) {
                        SDL_SetRenderDrawColor(renderer, SYNTH_ORANGE_DIM, 0xff);
                    } else if (item->hovered) {
                        SDL_SetRenderDrawColor(renderer, 0x40, 0x30, 0x20, 0xff);
                    } else {
                        SDL_SetRenderDrawColor(renderer, SYNTH_BG_DARK, 0xff);
                    }
                    SDL_Rect bg = {bounds.x, bounds.y, bounds.w, bounds.h};
                    SDL_RenderFillRect(renderer, &bg);

                    // Border
                    SDL_SetRenderDrawColor(renderer, item->selected ? SYNTH_ORANGE : SYNTH_BORDER, 0xff);
                    SDL_RenderDrawRect(renderer, &bg);

                    // Subcircuit icon (IC chip with notch)
                    int icon_x = bounds.x + bounds.w / 2;
                    int icon_y = bounds.y + 12;
                    SDL_SetRenderDrawColor(renderer, SYNTH_ORANGE, 0xff);

                    // Draw DIP-style IC body
                    SDL_Rect icon = {icon_x - 10, icon_y - 7, 20, 14};
                    SDL_RenderDrawRect(renderer, &icon);

                    // Draw notch at top (IC orientation indicator)
                    SDL_RenderDrawLine(renderer, icon_x - 3, icon_y - 7, icon_x, icon_y - 4);
                    SDL_RenderDrawLine(renderer, icon_x, icon_y - 4, icon_x + 3, icon_y - 7);

                    // Draw 3 pins on each side
                    for (int p = 0; p < 3; p++) {
                        // Left pins
                        SDL_RenderDrawLine(renderer, icon_x - 14, icon_y - 4 + p * 4, icon_x - 10, icon_y - 4 + p * 4);
                        // Right pins
                        SDL_RenderDrawLine(renderer, icon_x + 10, icon_y - 4 + p * 4, icon_x + 14, icon_y - 4 + p * 4);
                    }

                    // Label (truncate if needed)
                    char short_label[10];
                    strncpy(short_label, item->label, 9);
                    short_label[9] = '\0';
                    int text_x = bounds.x + (bounds.w - strlen(short_label) * 6) / 2;
                    int text_y = bounds.y + bounds.h - 12;
                    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT, 0xff);
                    ui_draw_text(renderer, short_label, text_x, text_y);
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
    } else {
        // Show hint when no subcircuits
        subcircuits_cat->header_y = draw_y;
        int header_screen_y = draw_y - scroll_offset;
        if (header_screen_y >= TOOLBAR_HEIGHT - 14 && header_screen_y < ui->window_height - STATUSBAR_HEIGHT) {
            SDL_SetRenderDrawColor(renderer, SYNTH_ORANGE, 0xff);
            ui_draw_text(renderer, "My Circuits", 12, header_screen_y);

            // Hint text
            SDL_SetRenderDrawColor(renderer, 0x60, 0x60, 0x60, 0xff);
            ui_draw_text(renderer, "(Ctrl+G to create)", 12, header_screen_y + 14);
        }
        draw_y += 30;
        content_height += 30;
    }

    // Update content height for scrollbar
    ui->palette_content_height = content_height + 10;

    // Reset clipping
    SDL_RenderSetClipRect(renderer, NULL);

    // Draw scrollbar if content exceeds visible area
    if (ui->palette_content_height > ui->palette_visible_height) {
        int scrollbar_x = PALETTE_WIDTH - 8;
        int scrollbar_track_y = TOOLBAR_HEIGHT + PALETTE_TOP_H + 2;
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
    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, 40, 14};
    (*ui_prop_slot(ui)).prop_type = enable_prop;
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
        (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, 60, 14};
        (*ui_prop_slot(ui)).prop_type = mode_prop;
        ui->num_properties++;
        prop_y += 16;

        // Start value
        bool edit_start = input && input->editing_property && input->editing_prop_type == start_prop;
        snprintf(buf, sizeof(buf), "%.3g %s", sweep->start_value, unit);
        draw_property_field(renderer, x + 10, prop_y, prop_w, "  Start:", buf,
                           edit_start, input ? input->input_buffer : "", input ? input->input_cursor : 0);
        (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
        (*ui_prop_slot(ui)).prop_type = start_prop;
        ui->num_properties++;
        prop_y += 16;

        // End value
        bool edit_end = input && input->editing_property && input->editing_prop_type == end_prop;
        snprintf(buf, sizeof(buf), "%.3g %s", sweep->end_value, unit);
        draw_property_field(renderer, x + 10, prop_y, prop_w, "  End:", buf,
                           edit_end, input ? input->input_buffer : "", input ? input->input_cursor : 0);
        (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
        (*ui_prop_slot(ui)).prop_type = end_prop;
        ui->num_properties++;
        prop_y += 16;

        // Sweep time
        bool edit_time = input && input->editing_property && input->editing_prop_type == time_prop;
        snprintf(buf, sizeof(buf), "%.3g s", sweep->sweep_time);
        draw_property_field(renderer, x + 10, prop_y, prop_w, "  Time:", buf,
                           edit_time, input ? input->input_buffer : "", input ? input->input_cursor : 0);
        (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
        (*ui_prop_slot(ui)).prop_type = time_prop;
        ui->num_properties++;
        prop_y += 16;

        // Steps (only for step mode)
        if (sweep->mode == SWEEP_STEP) {
            bool edit_steps = input && input->editing_property && input->editing_prop_type == steps_prop;
            snprintf(buf, sizeof(buf), "%d", sweep->num_steps);
            draw_property_field(renderer, x + 10, prop_y, prop_w, "  Steps:", buf,
                               edit_steps, input ? input->input_buffer : "", input ? input->input_cursor : 0);
            (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
            (*ui_prop_slot(ui)).prop_type = steps_prop;
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
        (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, 40, 14};
        (*ui_prop_slot(ui)).prop_type = repeat_prop;
        ui->num_properties++;
        prop_y += 16;
    }

    return prop_y;
}

/* What the scope calls a channel: the probe's own label if it has one, CHn otherwise. */
const char *ui_channel_name(const UIState *ui, int ch) {
    static char fallback[8];
    if (ui && ch >= 0 && ch < MAX_PROBES && ui->scope_channels[ch].name[0]) return ui->scope_channels[ch].name;
    snprintf(fallback, sizeof fallback, "CH%d", ch + 1);
    return fallback;
}

/* "CH3" and "" are defaults; anything else is a name somebody typed. */
bool probe_label_is_default(const char *label) {
    if (!label || !label[0]) return true;
    if (label[0] != 'C' || label[1] != 'H') return false;
    for (const char *p = label + 2; *p; p++) if (*p < '0' || *p > '9') return false;
    return label[2] != '\0';
}

/* The slot the next property row goes in, or a bin if the panel has run out.

   Every row in this panel is emitted as
       ui->properties[ui->num_properties].bounds = ...;
       ui->properties[ui->num_properties].prop_type = ...;
       ui->num_properties++;
   at 157 places, and not one of them tested num_properties against the size of the array. An AC
   voltage source with both its amplitude and frequency sweeps enabled emits 17 rows into 16 slots,
   and the fields that follow the array are num_properties itself and editing_component - so row 16
   set num_properties to a screen coordinate and editing_component to a pair of rectangle
   dimensions, and the next store went wherever that coordinate pointed. Two clicks on the panel
   reach it, and it repeats every frame the panel is drawn.

   Writes past the end land in the bin and are dropped. Nothing reads the bin. */
static PropertyField *ui_prop_slot(UIState *ui) {
    static PropertyField overflow_bin;
    if (!ui || ui->num_properties < 0 || ui->num_properties >= UI_MAX_PROPERTY_ROWS)
        return &overflow_bin;
    return &ui->properties[ui->num_properties];
}

void ui_render_properties(UIState *ui, SDL_Renderer *renderer, Component *selected, struct InputState *input) {
    int x = ui->window_width - ui->properties_width;
    int y = TOOLBAR_HEIGHT;

    // Calculate panel height based on scope position (fill space between toolbar and scope)
    // The scope label is drawn 18px above scope_rect, so leave 50px gap to avoid overlap
    int available_height = ui->scope_rect.y - y - 50;
    int panel_height = available_height > 100 ? available_height : 100;  // Minimum 100 but don't exceed available
    if (ui->properties_collapsed) {
        // header only: "> Properties (name)"; click it to expand
        panel_height = 26;
        ui->properties_visible_height = panel_height;
        ui->properties_content_height = panel_height;
        SDL_SetRenderDrawColor(renderer, SYNTH_BG_DARK, 0xff);
        SDL_Rect panel = {x, y, ui->properties_width, panel_height};
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, SYNTH_PINK, 0xff);
        char hdr[96];
        snprintf(hdr, sizeof hdr, "> Properties%s%s", selected ? ": " : "", selected ? selected->label : "");
        ui_draw_text(renderer, hdr, x + 10, y + 8);
        ui->num_properties = 0;
        return;
    }

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

    // Title - synthwave pink (click to collapse)
    SDL_SetRenderDrawColor(renderer, SYNTH_PINK, 0xff);
    ui_draw_text(renderer, "v Properties", x + 10, content_y + 10);

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

                /* Named device. A schematic says 2N7000, not "an NMOS with V_th = 2.1 V", so
           anything with datasheet models in the library gets a row to pick one; the
           summary underneath is the line the parameters were taken from. */
        if (component_parts_for(selected->type, NULL, 0) > 0) {
            SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
            ui_draw_text(renderer, "Part:", x + 10, prop_y + 2);
            SDL_SetRenderDrawColor(renderer, 0xff, 0xd7, 0x4a, 0xff);
            ui_draw_text(renderer, selected->part[0] ? selected->part : "[generic]", x + 100, prop_y + 2);
            (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
            (*ui_prop_slot(ui)).prop_type = PROP_PART;
            ui->num_properties++;
            prop_y += 16;
            if (selected->part[0]) {
                const char *sum = NULL;
                for (int q = 0; q < component_part_count(); q++) {
            const PartModel *m = component_part_at(q);
            if (m && m->type == selected->type && !strcmp(m->part, selected->part)) { sum = m->summary; break; }
                }
                if (sum) {
            SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
            char line[64];
            int len = (int)strlen(sum), per = (prop_w - 16) / 6;
            if (per < 8) per = 8;
            for (int off = 0; off < len && off < 3 * per; off += per) {
                snprintf(line, sizeof line, "%.*s", per, sum + off);
                ui_draw_text(renderer, line, x + 10, prop_y + 2);
                prop_y += 11;
            }
                }
            }
            prop_y += 4;
        }

        switch (selected->type) {
            case COMP_DC_VOLTAGE: {
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.dc_voltage.voltage);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Voltage:", buf,
                                   editing_value, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_VALUE;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.dc_voltage.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Non-ideal parameters (internal resistance)
                if (!selected->props.dc_voltage.ideal) {
                    bool edit_rs = input && input->editing_property && input->editing_prop_type == PROP_R_SERIES;
                    format_engineering(selected->props.dc_voltage.r_series, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "R_series:", buf, edit_rs, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_R_SERIES;
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
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_VALUE;
                ui->num_properties++;

                prop_y += 18;
                snprintf(buf, sizeof(buf), "%.3g Hz", selected->props.ac_voltage.frequency);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Frequency:", buf,
                                   editing_freq, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_FREQUENCY;
                ui->num_properties++;

                prop_y += 18;
                snprintf(buf, sizeof(buf), "%.1f deg", selected->props.ac_voltage.phase);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Phase:", buf,
                                   editing_phase, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_PHASE;
                ui->num_properties++;

                prop_y += 18;
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.ac_voltage.offset);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Offset:", buf,
                                   editing_offset, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_OFFSET;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.ac_voltage.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Non-ideal parameters (internal resistance)
                if (!selected->props.ac_voltage.ideal) {
                    bool edit_rs = input && input->editing_property && input->editing_prop_type == PROP_R_SERIES;
                    format_engineering(selected->props.ac_voltage.r_series, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "R_series:", buf, edit_rs, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_R_SERIES;
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
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_VALUE;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.dc_current.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Non-ideal parameters (internal parallel resistance)
                if (!selected->props.dc_current.ideal) {
                    bool edit_rp = input && input->editing_property && input->editing_prop_type == PROP_R_PARALLEL;
                    format_engineering(selected->props.dc_current.r_parallel, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "R_parallel:", buf, edit_rp, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_R_PARALLEL;
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
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_VALUE;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.resistor.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Non-ideal parameters (temperature effects)
                if (!selected->props.resistor.ideal) {
                    bool edit_tc = input && input->editing_property && input->editing_prop_type == PROP_TEMP_COEFF;
                    snprintf(buf, sizeof(buf), "%.0f ppm/C", selected->props.resistor.temp_coeff);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Temp Coef:", buf, edit_tc, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_TEMP_COEFF;
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
                if (selected->props.resistor.high_power) { char pw[24]; format_engineering(selected->props.resistor.power_dissipated, "W", pw, sizeof pw); snprintf(buf, sizeof(buf), "%s (HP load, no limit)", pw); }
                else snprintf(buf, sizeof(buf), "%.2fW/%.2fW", selected->props.resistor.power_dissipated, selected->props.resistor.power_rating);
                ui_draw_text(renderer, buf, x + 100, prop_y + 2);
                break;
            }

            case COMP_CAPACITOR: {
                snprintf(buf, sizeof(buf), "%.3g F", selected->props.capacitor.capacitance);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Capacitance:", buf,
                                   editing_value, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_VALUE;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.capacitor.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Non-ideal parameters (ESR, leakage)
                if (!selected->props.capacitor.ideal) {
                    bool edit_vh = input && input->editing_property && input->editing_prop_type == PROP_CAP_VHALF;
                    if (selected->props.capacitor.v_half > 0)
                        snprintf(buf, sizeof(buf), "%.3g V", selected->props.capacitor.v_half);
                    else
                        snprintf(buf, sizeof(buf), "none (C0G)");
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Bias 1/2:", buf, edit_vh, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_CAP_VHALF;
                    ui->num_properties++;
                    prop_y += 18;
                }
                if (!selected->props.capacitor.ideal) {
                    bool edit_esr = input && input->editing_property && input->editing_prop_type == PROP_ESR;
                    format_engineering(selected->props.capacitor.esr, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "ESR:", buf, edit_esr, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_ESR;
                    ui->num_properties++;
                    prop_y += 18;

                    /* Leakage was drawn as grey read-only text and the series inductance was
                       not drawn at all, though the stamp reads both. A number a user can see and
                       cannot change is the most annoying kind of number. */
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_LEAKAGE;
                    format_engineering(selected->props.capacitor.leakage, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Leakage:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_LEAKAGE;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_ESL;
                    format_engineering(selected->props.capacitor.esl, "H", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "ESL:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_ESL;
                    ui->num_properties++;
                    prop_y += 18;
                }
                }
                break;
            }

            /* The transformer had no property panel at all, which made its turns ratio - the
               one number the part is about - uneditable, along with a magnetising inductance,
               a coupling coefficient and two winding resistances the simulation reads every
               step. Two of those already had working apply handlers with no way to reach them.
               Found by counting: --prop-gap lists the types that offer the panel nothing. */
            /* A DC motor: the armature branch the stamp reads, and the mechanical side the
               per-step advance reads. Its physics was corrected on 2026-08-30 and none of it
               could be configured - a motor could not be given a load torque or a different
               rotor, which is most of what a motor is for. */
            /* Six parts that offered the properties panel nothing at all. Every value below is
               read by the solver on every step - a wiper position, a light level, a pinch-off
               voltage, a battery's internal resistance, a switch's contact resistances - and
               none of them could be set. --prop-gap is what lists them. */
            case COMP_POTENTIOMETER: {
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_VALUE;
                    format_engineering(selected->props.potentiometer.resistance, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Resistance:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_VALUE;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_WIPER_POS;
                    snprintf(buf, sizeof(buf), "%.3f", selected->props.potentiometer.wiper_pos);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Wiper (0-1):", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_WIPER_POS;
                    ui->num_properties++;
                    prop_y += 18;
                }
                break;
            }

            case COMP_PHOTORESISTOR: {
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_R_DARK;
                    format_engineering(selected->props.photoresistor.r_dark, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "R dark:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_R_DARK;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_R_LIGHT;
                    format_engineering(selected->props.photoresistor.r_light, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "R light:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_R_LIGHT;
                    ui->num_properties++;
                    prop_y += 18;
                }
                /* No per-part light level: the photoresistor's stamp reads the global
                   environment - the Lux slider in the status bar - and not
                   props.photoresistor.light_level, so a row here sets a field nothing reads. */
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_LDR_GAMMA;
                    snprintf(buf, sizeof(buf), "%.3f", selected->props.photoresistor.gamma);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Gamma:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_LDR_GAMMA;
                    ui->num_properties++;
                    prop_y += 18;
                }
                break;
            }

            case COMP_THERMISTOR: {
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_R_25;
                    format_engineering(selected->props.thermistor.r_25, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "R at 25C:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_R_25;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_BETA;
                    snprintf(buf, sizeof(buf), "%.0f", selected->props.thermistor.beta);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Beta (K):", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_BETA;
                    ui->num_properties++;
                    prop_y += 18;
                }
                break;
            }

            case COMP_NJFET: case COMP_PJFET: {
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_IDSS;
                    format_engineering(selected->props.jfet.idss, "A", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Idss:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_IDSS;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_VP;
                    snprintf(buf, sizeof(buf), "%.3f V", selected->props.jfet.vp);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Vp:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_VP;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_JFET_LAMBDA;
                    snprintf(buf, sizeof(buf), "%.4f", selected->props.jfet.lambda);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Lambda:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_JFET_LAMBDA;
                    ui->num_properties++;
                    prop_y += 18;
                }
                break;
            }

            case COMP_BATTERY: {
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_VALUE;
                    format_engineering(selected->props.battery.nominal_voltage, "V", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Voltage:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_VALUE;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_BATT_CAPACITY;
                    snprintf(buf, sizeof(buf), "%.0f", selected->props.battery.capacity_mah);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Capacity mAh:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_BATT_CAPACITY;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_BATT_R;
                    format_engineering(selected->props.battery.internal_r, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Internal R:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_BATT_R;
                    ui->num_properties++;
                    prop_y += 18;
                }
                break;
            }

            case COMP_SPST_SWITCH: case COMP_SPDT_SWITCH:
            case COMP_DPDT_SWITCH: case COMP_PUSH_BUTTON: {
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_R_ON;
                    format_engineering(selected->props.switch_spst.r_on, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Contact on:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_R_ON;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_R_OFF;
                    format_engineering(selected->props.switch_spst.r_off, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Contact off:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_R_OFF;
                    ui->num_properties++;
                    prop_y += 18;
                }
                break;
            }

            case COMP_DC_MOTOR: {
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_MOTOR_R;
                    format_engineering(selected->props.dc_motor.r_armature, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "R armature:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_MOTOR_R;
                    ui->num_properties++;
                    prop_y += 18;
                }
                /* No Model toggle and no ideal/real split: no stamp reads props.dc_motor.ideal,
                   so it changed nothing while hiding the armature inductance and the friction,
                   both of which the solver uses on every step. Always shown now. */
                {
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_MOTOR_L;
                    format_engineering(selected->props.dc_motor.l_armature, "H", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "L armature:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_MOTOR_L;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_MOTOR_B;
                    snprintf(buf, sizeof(buf), "%.4g", selected->props.dc_motor.b_friction);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Friction b:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_MOTOR_B;
                    ui->num_properties++;
                    prop_y += 18;
                }
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_MOTOR_KV;
                    snprintf(buf, sizeof(buf), "%.4g", selected->props.dc_motor.kv);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Kv (V.s/rad):", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_MOTOR_KV;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_MOTOR_KT;
                    snprintf(buf, sizeof(buf), "%.4g", selected->props.dc_motor.kt);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Kt (Nm/A):", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_MOTOR_KT;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_MOTOR_J;
                    snprintf(buf, sizeof(buf), "%.4g", selected->props.dc_motor.j_rotor);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Inertia J:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_MOTOR_J;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_MOTOR_TLOAD;
                    snprintf(buf, sizeof(buf), "%.4g", selected->props.dc_motor.torque_load);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Load torque:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_MOTOR_TLOAD;
                    ui->num_properties++;
                    prop_y += 18;
                }
                break;
            }

            /* A relay: the coil that decides when it pulls in, and the contacts it closes.
               The pickup and dropout currents are the whole behaviour of the part. */
            case COMP_RELAY: {
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_RELAY_R_COIL;
                    format_engineering(selected->props.relay.r_coil, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Coil R:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_RELAY_R_COIL;
                    ui->num_properties++;
                    prop_y += 18;
                }
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.relay.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;
                if (!selected->props.relay.ideal) {
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_RELAY_L_COIL;
                    format_engineering(selected->props.relay.l_coil, "H", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Coil L:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_RELAY_L_COIL;
                    ui->num_properties++;
                    prop_y += 18;
                }
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_RELAY_I_PICKUP;
                    format_engineering(selected->props.relay.i_pickup, "A", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Pickup I:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_RELAY_I_PICKUP;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_RELAY_I_DROPOUT;
                    format_engineering(selected->props.relay.i_dropout, "A", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Dropout I:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_RELAY_I_DROPOUT;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_RELAY_R_ON;
                    format_engineering(selected->props.relay.r_contact_on, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Contact on:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_RELAY_R_ON;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_RELAY_R_OFF;
                    format_engineering(selected->props.relay.r_contact_off, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Contact off:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_RELAY_R_OFF;
                    ui->num_properties++;
                    prop_y += 18;
                }
                break;
            }

            /* The four controlled sources. PROP_GAIN sat in the property enum with nothing on
               either end of it - no row, no handler - so the gain of a VCVS could not be set,
               and the gain is the entire part. */
            case COMP_VCVS: case COMP_VCCS: case COMP_CCVS: case COMP_CCCS: {
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_GAIN;
                    snprintf(buf, sizeof(buf), "%.4g", selected->props.controlled_source.gain);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, (selected->type == COMP_VCVS ? "Gain (V/V):" : selected->type == COMP_VCCS ? "Gain (A/V):" : selected->type == COMP_CCVS ? "Gain (V/A):" : "Gain (A/A):"), buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_GAIN;
                    ui->num_properties++;
                    prop_y += 18;
                }
                /* likewise: props.controlled_source.ideal is read by no stamp */
                /* R sense only where it is read: the CCVS and the CCCS sense their control
                   current through props.controlled_source.r_in (component.c:4943 and 4963); the
                   VCVS and the VCCS hardcode their input conductance and would ignore it. */
                if (selected->type == COMP_CCVS || selected->type == COMP_CCCS) {
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_CS_RIN;
                    format_engineering(selected->props.controlled_source.r_in, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "R input:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_CS_RIN;
                    ui->num_properties++;
                    prop_y += 18;
                }
                }
                break;
            }

            case COMP_TRANSFORMER:
            case COMP_TRANSFORMER_CT: {
                snprintf(buf, sizeof(buf), "%.4g : 1", selected->props.transformer.turns_ratio);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Turns N2/N1:", buf,
                                   editing_value, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_VALUE;
                ui->num_properties++;
                prop_y += 18;

                /* The centre-tapped transformer stops here: its stamp reads only the turns
                   ratio and hardcodes its source and magnetising resistances, so the model
                   toggle and the winding resistances below would all be inert for it. The
                   two-winding transformer does read them. */
                if (selected->type == COMP_TRANSFORMER_CT) break;

                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.transformer.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                /* No L primary and no coupling row. The transformer model is an ideal turns
                   ratio plus winding resistance: no stamp reads props.transformer.l_primary or
                   .coupling. Rows for both were added on 2026-08-30 and took a value that went
                   nowhere, which is worse than having no row - the panel said the model had a
                   magnetising inductance when it does not. A pre-release review caught it. */
                if (!selected->props.transformer.ideal) {
                    bool edit_rp = input && input->editing_property && input->editing_prop_type == PROP_TRANS_R_PRIMARY;
                    format_engineering(selected->props.transformer.r_primary, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "R primary:", buf,
                                       edit_rp, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_TRANS_R_PRIMARY;
                    ui->num_properties++;
                    prop_y += 18;

                    bool edit_rs = input && input->editing_property && input->editing_prop_type == PROP_TRANS_R_SECONDARY;
                    format_engineering(selected->props.transformer.r_secondary, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "R secondary:", buf,
                                       edit_rs, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_TRANS_R_SECONDARY;
                    ui->num_properties++;
                    prop_y += 18;
                }
                break;
            }

            case COMP_INDUCTOR: {
                snprintf(buf, sizeof(buf), "%.3g H", selected->props.inductor.inductance);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Inductance:", buf,
                                   editing_value, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_VALUE;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.inductor.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Non-ideal parameters (DCR)
                if (!selected->props.inductor.ideal) {
                    bool edit_dcr = input && input->editing_property && input->editing_prop_type == PROP_DCR;
                    format_engineering(selected->props.inductor.dcr, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "DCR:", buf, edit_dcr, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_DCR;
                    ui->num_properties++;
                    prop_y += 18;

                    /* The saturation current decides where the core gives up, and it was grey
                       read-only text. */
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_I_SAT;
                    format_engineering(selected->props.inductor.i_sat, "A", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "I_sat:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_I_SAT;
                    ui->num_properties++;
                    prop_y += 18;
                }
                }
                break;
            }

            case COMP_SQUARE_WAVE:
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.square_wave.amplitude);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Amplitude:", buf,
                                   editing_value, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_VALUE;
                ui->num_properties++;

                prop_y += 18;
                snprintf(buf, sizeof(buf), "%.3g Hz", selected->props.square_wave.frequency);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Frequency:", buf,
                                   editing_freq, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_FREQUENCY;
                ui->num_properties++;

                prop_y += 18;
                snprintf(buf, sizeof(buf), "%.0f %%", selected->props.square_wave.duty * 100);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Duty:", buf,
                                   editing_duty, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_DUTY;
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
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_VALUE;
                ui->num_properties++;

                prop_y += 18;
                snprintf(buf, sizeof(buf), "%.3g Hz", selected->props.triangle_wave.frequency);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Frequency:", buf,
                                   editing_freq, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_FREQUENCY;
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
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_VALUE;
                ui->num_properties++;

                prop_y += 18;
                snprintf(buf, sizeof(buf), "%.3g Hz", selected->props.sawtooth_wave.frequency);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Frequency:", buf,
                                   editing_freq, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_FREQUENCY;
                ui->num_properties++;

                // Phase
                prop_y += 18;
                bool edit_saw_phase = input && input->editing_property && input->editing_prop_type == PROP_PHASE;
                snprintf(buf, sizeof(buf), "%.1f deg", selected->props.sawtooth_wave.phase);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Phase:", buf,
                                   edit_saw_phase, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_PHASE;
                ui->num_properties++;

                // Offset
                prop_y += 18;
                bool edit_saw_offset = input && input->editing_property && input->editing_prop_type == PROP_OFFSET;
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.sawtooth_wave.offset);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Offset:", buf,
                                   edit_saw_offset, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_OFFSET;
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
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_VALUE;
                ui->num_properties++;

                // Bandwidth
                prop_y += 18;
                bool edit_noise_bw = input && input->editing_property && input->editing_prop_type == PROP_BANDWIDTH;
                format_engineering(selected->props.noise_source.bandwidth, "Hz", buf, sizeof(buf));
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Bandwidth:", buf,
                                   edit_noise_bw, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_BANDWIDTH;
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

            case COMP_CLOCK: {
                // Frequency
                bool edit_clock_freq = input && input->editing_property && input->editing_prop_type == PROP_FREQUENCY;
                snprintf(buf, sizeof(buf), "%.3g Hz", selected->props.clock.frequency);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Frequency:", buf,
                                   edit_clock_freq, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_FREQUENCY;
                ui->num_properties++;

                // V_high
                prop_y += 18;
                bool edit_v_high = input && input->editing_property && input->editing_prop_type == PROP_V_HIGH;
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.clock.v_high);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "V_high:", buf,
                                   edit_v_high, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_V_HIGH;
                ui->num_properties++;

                // V_low
                prop_y += 18;
                bool edit_v_low = input && input->editing_property && input->editing_prop_type == PROP_V_LOW;
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.clock.v_low);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "V_low:", buf,
                                   edit_v_low, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_V_LOW;
                ui->num_properties++;

                // Duty cycle
                prop_y += 18;
                bool edit_clock_duty = input && input->editing_property && input->editing_prop_type == PROP_DUTY;
                snprintf(buf, sizeof(buf), "%.0f %%", selected->props.clock.duty * 100);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Duty:", buf,
                                   edit_clock_duty, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_DUTY;
                ui->num_properties++;
                break;
            }

            case COMP_PULSE_SOURCE: {
                // V_low
                bool edit_pulse_v_low = input && input->editing_property && input->editing_prop_type == PROP_V_LOW;
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.pulse_source.v_low);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "V_low:", buf,
                                   edit_pulse_v_low, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_V_LOW;
                ui->num_properties++;

                // V_high
                prop_y += 18;
                bool edit_pulse_v_high = input && input->editing_property && input->editing_prop_type == PROP_V_HIGH;
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.pulse_source.v_high);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "V_high:", buf,
                                   edit_pulse_v_high, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_V_HIGH;
                ui->num_properties++;

                // Delay
                prop_y += 18;
                bool edit_pulse_delay = input && input->editing_property && input->editing_prop_type == PROP_DELAY;
                format_engineering(selected->props.pulse_source.delay, "s", buf, sizeof(buf));
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Delay:", buf,
                                   edit_pulse_delay, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_DELAY;
                ui->num_properties++;

                // Rise time
                prop_y += 18;
                bool edit_pulse_rise = input && input->editing_property && input->editing_prop_type == PROP_RISE_TIME;
                format_engineering(selected->props.pulse_source.rise_time, "s", buf, sizeof(buf));
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Rise:", buf,
                                   edit_pulse_rise, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_RISE_TIME;
                ui->num_properties++;

                // Fall time
                prop_y += 18;
                bool edit_pulse_fall = input && input->editing_property && input->editing_prop_type == PROP_FALL_TIME;
                format_engineering(selected->props.pulse_source.fall_time, "s", buf, sizeof(buf));
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Fall:", buf,
                                   edit_pulse_fall, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_FALL_TIME;
                ui->num_properties++;

                // Pulse width
                prop_y += 18;
                bool edit_pulse_width = input && input->editing_property && input->editing_prop_type == PROP_PULSE_WIDTH;
                format_engineering(selected->props.pulse_source.pulse_width, "s", buf, sizeof(buf));
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Width:", buf,
                                   edit_pulse_width, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_PULSE_WIDTH;
                ui->num_properties++;

                // Period
                prop_y += 18;
                bool edit_pulse_period = input && input->editing_property && input->editing_prop_type == PROP_PERIOD;
                format_engineering(selected->props.pulse_source.period, "s", buf, sizeof(buf));
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Period:", buf,
                                   edit_pulse_period, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_PERIOD;
                ui->num_properties++;
                break;
            }

            case COMP_PWM_SOURCE: {
                // Amplitude
                bool edit_pwm_amp = input && input->editing_property && input->editing_prop_type == PROP_VALUE;
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.pwm_source.amplitude);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Amplitude:", buf,
                                   edit_pwm_amp, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_VALUE;
                ui->num_properties++;

                // Frequency
                prop_y += 18;
                bool edit_pwm_freq = input && input->editing_property && input->editing_prop_type == PROP_FREQUENCY;
                snprintf(buf, sizeof(buf), "%.3g Hz", selected->props.pwm_source.frequency);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Frequency:", buf,
                                   edit_pwm_freq, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_FREQUENCY;
                ui->num_properties++;

                // Duty cycle
                prop_y += 18;
                bool edit_pwm_duty = input && input->editing_property && input->editing_prop_type == PROP_DUTY;
                snprintf(buf, sizeof(buf), "%.0f %%", selected->props.pwm_source.duty * 100);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Duty:", buf,
                                   edit_pwm_duty, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_DUTY;
                ui->num_properties++;

                // Offset
                prop_y += 18;
                bool edit_pwm_offset = input && input->editing_property && input->editing_prop_type == PROP_OFFSET;
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.pwm_source.offset);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Offset:", buf,
                                   edit_pwm_offset, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_OFFSET;
                ui->num_properties++;
                break;
            }

            case COMP_DIODE: {
                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.diode.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_IDEAL;
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
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_BV;
                    ui->num_properties++;
                    prop_y += 18;

                    /* The junction capacitance. It was a default value nothing read until its
                       stamp was written on 2026-08-30, and it still could not be set. */
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_CJO;
                    format_engineering(selected->props.diode.cjo, "F", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Cjo:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_CJO;
                    ui->num_properties++;
                    prop_y += 18;
                }
                }
                break;
            }

            case COMP_SOURCE_3PH: {
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_3PH_V;
                    snprintf(buf, sizeof(buf), "%.4g V", selected->props.source_3ph.v_peak);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Vpk L-N:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_3PH_V;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_3PH_F;
                    snprintf(buf, sizeof(buf), "%.4g Hz", selected->props.source_3ph.frequency);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Freq:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_3PH_F;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_3PH_PHASE;
                    snprintf(buf, sizeof(buf), "%.1f deg", selected->props.source_3ph.phase);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Phase A:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_3PH_PHASE;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_3PH_R;
                    snprintf(buf, sizeof(buf), "%.4g Ohm", selected->props.source_3ph.r_series);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "R/phase:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_3PH_R;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_3PH_L;
                    snprintf(buf, sizeof(buf), "%.4g H", selected->props.source_3ph.l_series);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "L/phase:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_3PH_L;
                    ui->num_properties++;
                    prop_y += 18;
                }
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                snprintf(buf, sizeof(buf), "L-L rms %.4g V", selected->props.source_3ph.v_peak / 1.41421356 * 1.7320508);
                ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                prop_y += 18;
                break;
            }

            case COMP_DELAY_LINE: {
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_LINE_Z0;
                    snprintf(buf, sizeof(buf), "%.4g Ohm", selected->props.delay_line.z0);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Z0:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_LINE_Z0;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_LINE_DELAY;
                    double td = selected->props.delay_line.delay;
                    if (td >= 1e-6)      snprintf(buf, sizeof(buf), "%.4g us", td * 1e6);
                    else if (td >= 1e-9) snprintf(buf, sizeof(buf), "%.4g ns", td * 1e9);
                    else                 snprintf(buf, sizeof(buf), "%.4g ps", td * 1e12);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Delay:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_LINE_DELAY;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {   /* a length is easier to picture than a delay: 0.66 c is ordinary coax */
                    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DARK, 0xff);
                    double metres = selected->props.delay_line.delay * 2.0e8;
                    snprintf(buf, sizeof(buf), "~%.3g m of coax", metres);
                    ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                    prop_y += 16;
                }
                break;
            }
            case COMP_TLINE: {
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_TLINE_LENGTH;
                    snprintf(buf, sizeof(buf), "%.4g mi", selected->props.tline.length_mi);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Length:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_TLINE_LENGTH;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_TLINE_R;
                    snprintf(buf, sizeof(buf), "%.4g Ohm", selected->props.tline.r_per_mi);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "R/mi:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_TLINE_R;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_TLINE_X;
                    snprintf(buf, sizeof(buf), "%.4g Ohm", selected->props.tline.x_per_mi);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "X/mi:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_TLINE_X;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_TLINE_B;
                    snprintf(buf, sizeof(buf), "%.4g uS", selected->props.tline.b_us_per_mi);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "B/mi:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_TLINE_B;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    bool ed = input && input->editing_property && input->editing_prop_type == PROP_TLINE_MODEL;
                    snprintf(buf, sizeof(buf), "%d (0=R 1=RL 2=pi)", selected->props.tline.model);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Model:", buf, ed, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_TLINE_MODEL;
                    ui->num_properties++;
                    prop_y += 18;
                }
                {
                    double R, L, Cend; tline_params(selected, &R, &L, &Cend);
                    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                    snprintf(buf, sizeof(buf), "R=%.3g  L=%.3gmH  C/2=%.3guF", R, L * 1e3, Cend * 1e6);
                    ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                    prop_y += 18;
                }
                break;
            }

            case COMP_SPARK_GAP: {
                bool e1 = input && input->editing_property && input->editing_prop_type == PROP_SPARK_GAP_MM;
                snprintf(buf, sizeof(buf), "%.2f mm (%.3g kV)", selected->props.spark_gap.gap_mm, spark_gap_breakdown(selected) / 1e3);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Gap:", buf, e1, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_SPARK_GAP_MM;
                ui->num_properties++;
                prop_y += 18;
                bool e2 = input && input->editing_property && input->editing_prop_type == PROP_SPARK_GAP_RON;
                snprintf(buf, sizeof(buf), "%.3g Ohm", selected->props.spark_gap.r_on);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "R arc:", buf, e2, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_SPARK_GAP_RON;
                ui->num_properties++;
                prop_y += 18;
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.spark_gap.conducting ? "State: ARC" : "State: open", x + 10, prop_y + 2);
                prop_y += 18;
                break;
            }

            case COMP_TOROID: {
                bool e1 = input && input->editing_property && input->editing_prop_type == PROP_TOROID_MAJOR;
                snprintf(buf, sizeof(buf), "%.1f in", selected->props.toroid.major_in);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Outer D:", buf, e1, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_TOROID_MAJOR;
                ui->num_properties++;
                prop_y += 18;
                bool e2 = input && input->editing_property && input->editing_prop_type == PROP_TOROID_MINOR;
                snprintf(buf, sizeof(buf), "%.1f in", selected->props.toroid.minor_in);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Tube d:", buf, e2, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_TOROID_MINOR;
                ui->num_properties++;
                prop_y += 18;
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                snprintf(buf, sizeof(buf), "C = %.1f pF   V = %.3g kV", toroid_capacitance(selected) * 1e12, selected->props.toroid.voltage / 1e3);
                ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                prop_y += 18;
                break;
            }

            case COMP_ZENER: {
                // Zener voltage (editable)
                bool edit_vz = input && input->editing_property && input->editing_prop_type == PROP_VZ;
                snprintf(buf, sizeof(buf), "%.2f V", selected->props.zener.vz);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Vz:", buf, edit_vz, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_VZ;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.zener.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                if (!selected->props.zener.ideal) {
                    // Zener impedance (editable)
                    bool edit_rz = input && input->editing_property && input->editing_prop_type == PROP_RZ;
                    format_engineering(selected->props.zener.rz, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Rz:", buf, edit_rz, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_RZ;
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
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_LED_VF;  // Reuse LED_VF for forward voltage
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.schottky.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_IDEAL;
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
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_VALUE;
                ui->num_properties++;
                prop_y += 18;

                // Max voltage (editable)
                bool edit_vmax = input && input->editing_property && input->editing_prop_type == PROP_MAX_VOLTAGE;
                snprintf(buf, sizeof(buf), "%.1f V", selected->props.capacitor_elec.max_voltage);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Max Voltage:", buf, edit_vmax, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_MAX_VOLTAGE;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.capacitor_elec.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                if (!selected->props.capacitor_elec.ideal) {
                    bool edit_esr = input && input->editing_property && input->editing_prop_type == PROP_ESR;
                    format_engineering(selected->props.capacitor_elec.esr, "Ohm", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "ESR:", buf, edit_esr, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_ESR;
                    ui->num_properties++;
                }
                break;
            }

            case COMP_OPAMP: {
                // Open-loop gain (editable)
                bool edit_gain = input && input->editing_property && input->editing_prop_type == PROP_OPAMP_GAIN;
                format_engineering(selected->props.opamp.gain, "", buf, sizeof(buf));
                draw_property_field(renderer, x + 10, prop_y, prop_w, "A_OL:", buf, edit_gain, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_OPAMP_GAIN;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.opamp.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_OPAMP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Rail voltages (editable)
                bool edit_vmax = input && input->editing_property && input->editing_prop_type == PROP_OPAMP_VMAX;
                snprintf(buf, sizeof(buf), "%.1f V", selected->props.opamp.vmax);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "V+:", buf, edit_vmax, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_OPAMP_VMAX;
                ui->num_properties++;
                prop_y += 18;

                bool edit_vmin = input && input->editing_property && input->editing_prop_type == PROP_OPAMP_VMIN;
                snprintf(buf, sizeof(buf), "%.1f V", selected->props.opamp.vmin);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "V-:", buf, edit_vmin, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_OPAMP_VMIN;
                ui->num_properties++;
                prop_y += 18;

                if (!selected->props.opamp.ideal) {
                    // GBW (editable)
                    bool edit_gbw = input && input->editing_property && input->editing_prop_type == PROP_OPAMP_GBW;
                    format_engineering(selected->props.opamp.gbw, "Hz", buf, sizeof(buf));
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "GBW:", buf, edit_gbw, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_OPAMP_GBW;
                    ui->num_properties++;
                    prop_y += 18;

                    // Slew rate (editable)
                    bool edit_slew = input && input->editing_property && input->editing_prop_type == PROP_OPAMP_SLEW;
                    snprintf(buf, sizeof(buf), "%.2f V/us", selected->props.opamp.slew_rate);
                    draw_property_field(renderer, x + 10, prop_y, prop_w, "Slew:", buf, edit_slew, edit_buf, cursor);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_OPAMP_SLEW;
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
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_OPAMP_R2R;
                ui->num_properties++;
                break;
            }

            case COMP_LED: {
                // Color selector (click to cycle through presets)
                const char *color_names[] = {"IR", "Red", "Orange", "Yellow", "Green", "Emerald", "Blue", "White", "UV"};
                int color_idx = selected->props.led.color % LED_COLOR_COUNT;

                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Color:", x + 10, prop_y + 2);
                // Draw color selector with dropdown indicator
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                snprintf(buf, sizeof(buf), "[%s] v", color_names[color_idx]);
                ui_draw_text(renderer, buf, x + 100, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_LED_COLOR;
                ui->num_properties++;
                prop_y += 18;

                // Forward voltage & wavelength (read-only)
                SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                snprintf(buf, sizeof(buf), "Vf: %.2fV (%.0fnm)", selected->props.led.vf, selected->props.led.wavelength);
                ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                prop_y += 18;

                // Model mode toggle
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.led.ideal ? "[Ideal]" : "[Real]", x + 100, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_IDEAL;
                ui->num_properties++;
                prop_y += 18;

                // Forward voltage (editable)
                bool editing_vf = input && input->editing_property && input->editing_prop_type == PROP_LED_VF;
                snprintf(buf, sizeof(buf), "%.2f V", selected->props.led.vf);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Fwd Voltage:", buf,
                                   editing_vf, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_LED_VF;
                ui->num_properties++;
                prop_y += 18;

                // Max current (editable)
                bool editing_imax = input && input->editing_property && input->editing_prop_type == PROP_LED_IMAX;
                snprintf(buf, sizeof(buf), "%.0f mA", selected->props.led.max_current * 1000);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Max Current:", buf,
                                   editing_imax, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_LED_IMAX;
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

            case COMP_LED_ARRAY: {
                // LED Array color selector
                const char *color_names[] = {"IR", "Red", "Orange", "Yellow", "Green", "Emerald", "Blue", "White", "UV"};
                int color_idx = selected->props.led_array.color % LED_COLOR_COUNT;

                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Color:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                snprintf(buf, sizeof(buf), "[%s] v", color_names[color_idx]);
                ui_draw_text(renderer, buf, x + 100, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_LED_ARRAY_COLOR;
                ui->num_properties++;
                prop_y += 18;

                // Forward voltage (read-only, based on color)
                SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                snprintf(buf, sizeof(buf), "Vf: %.2f V", selected->props.led_array.vf);
                ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                prop_y += 18;

                // Max current per LED
                snprintf(buf, sizeof(buf), "Max I: %.0f mA", selected->props.led_array.max_current * 1000);
                ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                break;
            }

            case COMP_NPN_BJT:
            case COMP_PNP_BJT: {
                // Beta (forward current gain)
                bool editing_beta = input && input->editing_property && input->editing_prop_type == PROP_BJT_BETA;
                snprintf(buf, sizeof(buf), "%.1f", selected->props.bjt.bf);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Beta (BF):", buf,
                                   editing_beta, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_BJT_BETA;
                ui->num_properties++;
                prop_y += 18;

                // Saturation current (Is)
                bool editing_is = input && input->editing_property && input->editing_prop_type == PROP_BJT_IS;
                format_engineering(selected->props.bjt.is, "A", buf, sizeof(buf));
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Is:", buf,
                                   editing_is, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_BJT_IS;
                ui->num_properties++;
                prop_y += 18;

                // Early voltage (VAF)
                bool editing_vaf = input && input->editing_property && input->editing_prop_type == PROP_BJT_VAF;
                snprintf(buf, sizeof(buf), "%.1f V", selected->props.bjt.vaf);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "VAF:", buf,
                                   editing_vaf, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_BJT_VAF;
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
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_BJT_IDEAL;
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
                    prop_y += 14;
                    if (selected->props.bjt.tf > 0 || selected->props.bjt.cje > 0) {
                        char t1[24], t2[24], t3[24];
                        format_engineering(selected->props.bjt.tf, "s", t1, sizeof t1);
                        format_engineering(selected->props.bjt.cje, "F", t2, sizeof t2);
                        format_engineering(selected->props.bjt.cjc, "F", t3, sizeof t3);
                        snprintf(buf, sizeof(buf), "TF %s CJE %s CJC %s", t1, t2, t3);
                        ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                        prop_y += 14;
                    }
                }

                /* Where the device is actually working. A reader who has just built a stage and
                   is looking at a flat or tiny waveform is nearly always looking at a bias
                   problem, and nothing in the waveform says so - the solver linearises happily
                   about a transistor that is hard on or hard off and reports the result. */
                {
                    static const char *regions[4] = { "CUT OFF", "ACTIVE", "SATURATED", "REVERSE ACTIVE" };
                    int rg = selected->props.bjt.op_region;
                    if (rg < 0 || rg > 3) rg = 0;
                    prop_y += 4;
                    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                    ui_draw_text(renderer, "Operating point", x + 10, prop_y + 2);
                    prop_y += 16;
                    if (rg == 1) SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);
                    else         SDL_SetRenderDrawColor(renderer, 0xff, 0x60, 0x40, 0xff);
                    ui_draw_text(renderer, regions[rg], x + 10, prop_y + 2);
                    prop_y += 16;
                    SDL_SetRenderDrawColor(renderer, 0x00, 0xcc, 0xff, 0xff);
                    char v1[24], v2[24], v3[24];
                    format_engineering(selected->props.bjt.op_vbe, "V", v1, sizeof v1);
                    format_engineering(selected->props.bjt.op_vce, "V", v2, sizeof v2);
                    snprintf(buf, sizeof(buf), "Vbe %s  Vce %s", v1, v2);
                    ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                    prop_y += 14;
                    format_engineering(selected->props.bjt.op_ic, "A", v1, sizeof v1);
                    format_engineering(selected->props.bjt.op_ib, "A", v2, sizeof v2);
                    format_engineering(selected->props.bjt.op_gm, "S", v3, sizeof v3);
                    snprintf(buf, sizeof(buf), "Ic %s  Ib %s", v1, v2);
                    ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                    prop_y += 14;
                    snprintf(buf, sizeof(buf), "gm %s", v3);
                    ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                    prop_y += 14;
                    /* The sentence that is the whole point of showing any of this. */
                    if (rg != 1) {
                        SDL_SetRenderDrawColor(renderer, 0xff, 0x60, 0x40, 0xff);
                        ui_draw_text(renderer, rg == 2 ? "Saturated: it cannot amplify."
                                                       : "Not conducting: it cannot amplify.",
                                     x + 10, prop_y + 2);
                        prop_y += 14;
                        ui_draw_text(renderer, "Any gain measured here is", x + 10, prop_y + 2);
                        prop_y += 14;
                        ui_draw_text(renderer, "meaningless. Fix the bias first.", x + 10, prop_y + 2);
                        prop_y += 14;
                    }
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
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_MOS_VTH;
                ui->num_properties++;
                prop_y += 18;

                // Transconductance parameter (Kp)
                bool editing_kp = input && input->editing_property && input->editing_prop_type == PROP_MOS_KP;
                format_engineering(selected->props.mosfet.kp, "A/V2", buf, sizeof(buf));
                draw_property_field(renderer, x + 10, prop_y, prop_w, "u*Cox:", buf,
                                   editing_kp, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_MOS_KP;
                ui->num_properties++;
                prop_y += 18;

                // Channel width (W)
                bool editing_w = input && input->editing_property && input->editing_prop_type == PROP_MOS_W;
                format_engineering(selected->props.mosfet.w, "m", buf, sizeof(buf));
                draw_property_field(renderer, x + 10, prop_y, prop_w, "W:", buf,
                                   editing_w, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_MOS_W;
                ui->num_properties++;
                prop_y += 18;

                // Channel length (L)
                bool editing_l = input && input->editing_property && input->editing_prop_type == PROP_MOS_L;
                format_engineering(selected->props.mosfet.l, "m", buf, sizeof(buf));
                draw_property_field(renderer, x + 10, prop_y, prop_w, "L:", buf,
                                   editing_l, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_MOS_L;
                ui->num_properties++;
                prop_y += 18;

                // W/L ratio - the form textbooks actually quote
                bool editing_wl = input && input->editing_property && input->editing_prop_type == PROP_MOS_WL;
                snprintf(buf, sizeof(buf), "%.4g", selected->props.mosfet.l > 0 ? selected->props.mosfet.w / selected->props.mosfet.l : 0);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "W/L:", buf, editing_wl, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_MOS_WL;
                ui->num_properties++;
                prop_y += 18;

                // Device transconductance Kn = Kp (W/L): the other form a problem may give you
                bool editing_kn = input && input->editing_property && input->editing_prop_type == PROP_MOS_KN;
                format_engineering(selected->props.mosfet.kp * (selected->props.mosfet.l > 0 ? selected->props.mosfet.w / selected->props.mosfet.l : 0), "A/V2", buf, sizeof(buf));
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Kn:", buf, editing_kn, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_MOS_KN;
                ui->num_properties++;
                prop_y += 18;

                // Channel-length modulation
                bool editing_lam = input && input->editing_property && input->editing_prop_type == PROP_MOS_LAMBDA;
                snprintf(buf, sizeof(buf), "%.4g /V", selected->props.mosfet.lambda);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "lambda:", buf, editing_lam, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_MOS_LAMBDA;
                ui->num_properties++;
                prop_y += 18;

                // Gate oxide: t_ox sets Cox = eps_ox / t_ox, and u*Cox follows at constant mobility
                bool editing_tox = input && input->editing_property && input->editing_prop_type == PROP_MOS_TOX;
                format_engineering(selected->props.mosfet.tox, "m", buf, sizeof(buf));
                draw_property_field(renderer, x + 10, prop_y, prop_w, "tox:", buf, editing_tox, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_MOS_TOX;
                ui->num_properties++;
                prop_y += 18;
                {
                    double cox = selected->props.mosfet.tox > 0 ? 3.45e-11 / selected->props.mosfet.tox : 0;
                    char c1[24], c2[24];
                    format_engineering(cox, "F/m2", c1, sizeof c1);
                    format_engineering(cox > 0 ? selected->props.mosfet.kp / cox : 0, "m2/Vs", c2, sizeof c2);
                    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                    snprintf(buf, sizeof(buf), "Cox %s", c1);
                    ui_draw_text(renderer, buf, x + 10, prop_y + 2); prop_y += 14;
                    snprintf(buf, sizeof(buf), "u   %s", c2);
                    ui_draw_text(renderer, buf, x + 10, prop_y + 2); prop_y += 16;
                }

                // Enhancement or depletion mode: click to flip the sign of Vth
                {
                    bool depl = (selected->type == COMP_NMOS) ? (selected->props.mosfet.vth < 0) : (selected->props.mosfet.vth > 0);
                    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                    ui_draw_text(renderer, "Type:", x + 10, prop_y + 2);
                    SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);
                    ui_draw_text(renderer, depl ? "[Depletion]" : "[Enhancement]", x + 100, prop_y + 2);
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_MOS_TYPE;
                    ui->num_properties++;
                    prop_y += 18;
                }

                // Live operating point: region, Vgs, Vds, Id, gm, and Vov = Vgs - Vth
                {
                    static const char *regions[3] = { "CUTOFF", "TRIODE", "SATURATION" };
                    int rg = selected->props.mosfet.op_region;
                    if (rg < 0 || rg > 2) rg = 0;
                    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                    ui_draw_text(renderer, "Operating point", x + 10, prop_y + 2);
                    prop_y += 16;
                    SDL_SetRenderDrawColor(renderer, rg == 2 ? 0x00 : 0xff, rg == 2 ? 0xff : 0xc0, 0x66, 0xff);
                    ui_draw_text(renderer, regions[rg], x + 10, prop_y + 2);
                    prop_y += 16;
                    SDL_SetRenderDrawColor(renderer, 0x00, 0xcc, 0xff, 0xff);
                    char v1[24], v2[24];
                    format_engineering(selected->props.mosfet.op_vgs, "V", v1, sizeof v1);
                    format_engineering(selected->props.mosfet.op_vds, "V", v2, sizeof v2);
                    snprintf(buf, sizeof(buf), "Vgs %s  Vds %s", v1, v2);
                    ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                    prop_y += 16;
                    format_engineering(selected->props.mosfet.op_id, "A", v1, sizeof v1);
                    format_engineering(selected->props.mosfet.op_gm, "A/V", v2, sizeof v2);
                    snprintf(buf, sizeof(buf), "Id %s  gm %s", v1, v2);
                    ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                    prop_y += 16;
                    format_engineering(selected->props.mosfet.op_vgs - fabs(selected->props.mosfet.vth), "V", v1, sizeof v1);
                    snprintf(buf, sizeof(buf), "Vov %s", v1);
                    ui_draw_text(renderer, buf, x + 10, prop_y + 2);
                    prop_y += 18;
                }

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
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_MOS_IDEAL;
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
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_TEXT_CONTENT;
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
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, 60, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_TEXT_SIZE;
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
                (*ui_prop_slot(ui)).bounds = (Rect){toggle_x, prop_y, 24, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_TEXT_BOLD;
                ui->num_properties++;
                toggle_x += 30;

                // Italic toggle
                if (selected->props.text.italic) {
                    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                } else {
                    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                }
                ui_draw_text(renderer, "[I]", toggle_x, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){toggle_x, prop_y, 24, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_TEXT_ITALIC;
                ui->num_properties++;
                toggle_x += 30;

                // Underline toggle
                if (selected->props.text.underline) {
                    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                } else {
                    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                }
                ui_draw_text(renderer, "[U]", toggle_x, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){toggle_x, prop_y, 24, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_TEXT_UNDERLINE;
                ui->num_properties++;
                break;
            }

            case COMP_FUSE: {
                // Rating (current in amps)
                snprintf(buf, sizeof(buf), "%.3g A", selected->props.fuse.rating);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Rating:", buf,
                                   editing_value, edit_buf, cursor);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_VALUE;
                ui->num_properties++;
                prop_y += 18;

                // Model mode toggle (Ideal = instant blow, Real = i²t model)
                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Model:", x + 10, prop_y + 2);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
                ui_draw_text(renderer, selected->props.fuse.ideal ? "[Ideal]" : "[i2t]", x + 100, prop_y + 2);
                (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                (*ui_prop_slot(ui)).prop_type = PROP_IDEAL;
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
                    (*ui_prop_slot(ui)).bounds = (Rect){x + 10, prop_y, 90, 14};
                    (*ui_prop_slot(ui)).prop_type = PROP_RESET_FUSE;
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
    } else if (ui->selected_probe) {
        /* A selected probe gets one editable field: its name. That name is what the scope
           labels the channel with, so "Vout" beats "CH2" on a schematic with eight of them. */
        ui->num_properties = 0;
        Probe *pr = ui->selected_probe;
        int prop_y = content_y + 6;
        int prop_w = ui->properties_width - 20;
        SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0xff);
        char hdr[64];
        snprintf(hdr, sizeof hdr, "Probe on channel %d", pr->channel_num + 1);
        ui_draw_text(renderer, hdr, x + 10, prop_y);
        prop_y += 20;

        bool editing = input->editing_property && input->editing_prop_type == PROP_PROBE_NAME;
        draw_property_field(renderer, x + 10, prop_y, prop_w, "Name:", pr->label,
                            editing, input->input_buffer, input->input_cursor);
        (*ui_prop_slot(ui)).bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
        (*ui_prop_slot(ui)).prop_type = PROP_PROBE_NAME;
        ui->num_properties++;
        prop_y += 22;

        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DIM, 0xff);
        char vbuf[48];
        snprintf(vbuf, sizeof vbuf, "Reading: %.4g V", pr->voltage);
        ui_draw_text(renderer, vbuf, x + 10, prop_y); prop_y += 16;
        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DARK, 0xff);
        ui_draw_text(renderer, "Up to 7 characters.", x + 10, prop_y); prop_y += 14;
        ui_draw_text(renderer, "Blank restores CHn.", x + 10, prop_y); prop_y += 14;
        ui->properties_content_height = prop_y + 10 - content_y;
    } else {
        // Reset num_properties when nothing is selected to avoid stale bounds
        ui->num_properties = 0;

        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DARK, 0xff);
        ui_draw_text(renderer, "No selection", x + 10, content_y + 35);
        ui_draw_text(renderer, "Click component", x + 10, content_y + 55);
        ui_draw_text(renderer, "or probe", x + 10, content_y + 70);
        ui_draw_text(renderer, "to edit properties", x + 10, content_y + 85);

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
    // Render voltmeter/ammeter readings inline in the status bar
    // Position right after env sliders (Lux/Temp end around 711)
    int y = ui->window_height - STATUSBAR_HEIGHT;
    int x = 720;  // Right after Lux/Temp sliders

    /* The meters sit at a fixed x while the readouts to their right are placed from the window
       edge, so on a narrow window they run straight into them: at 1000 px wide, VM landed on the
       t= readout and AM on the component count, all three overlapping in the same strip. The
       status bar already drops its Lux/Temp sliders when the room runs out; the meters do the
       same now. Found by unpacking the release zip and running it at 1000x700 - --layout-test
       checks one large size, so nothing else was going to see it. */
    if (x + 180 > ui->window_width - 370) return;

    // Voltmeter
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DIM, 0xff);
    ui_draw_text(renderer, "VM:", x, y + 8);
    char volt_str[32];
    snprintf(volt_str, sizeof(volt_str), "%.3fV", ui->voltmeter_value);
    SDL_SetRenderDrawColor(renderer, SYNTH_GREEN, 0xff);
    ui_draw_text(renderer, volt_str, x + 28, y + 8);

    // Ammeter
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DIM, 0xff);
    ui_draw_text(renderer, "AM:", x + 90, y + 8);
    char amp_str[32];
    snprintf(amp_str, sizeof(amp_str), "%.3fmA", ui->ammeter_value * 1000.0);
    SDL_SetRenderDrawColor(renderer, SYNTH_ORANGE, 0xff);
    ui_draw_text(renderer, amp_str, x + 118, y + 8);
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
    } else if (val >= 1e-6) {
        snprintf(buf, size, "%.1fus", val * 1000000);
    } else if (val >= 1e-7) {
        snprintf(buf, size, "%.0fns", val * 1e9);
    } else {
        snprintf(buf, size, "%.0fns", val * 1e9);
    }
}

// Engineering-style readouts for the cursor box (3 significant digits, signed)
static void fmt_time_eng(char *buf, size_t size, double t) {
    double a = fabs(t);
    if (a >= 1.0)        snprintf(buf, size, "%.3gs", t);
    else if (a >= 1e-3)  snprintf(buf, size, "%.3gms", t * 1e3);
    else if (a >= 1e-6)  snprintf(buf, size, "%.3gus", t * 1e6);
    else                 snprintf(buf, size, "%.3gns", t * 1e9);
}
static void fmt_volt_eng(char *buf, size_t size, double v) {
    double a = fabs(v);
    if (a >= 1.0)        snprintf(buf, size, "%.3gV", v);
    else if (a >= 1e-3)  snprintf(buf, size, "%.3gmV", v * 1e3);
    else if (a >= 1e-6)  snprintf(buf, size, "%.3guV", v * 1e6);
    else                 snprintf(buf, size, "0V");
}
static void fmt_freq_eng(char *buf, size_t size, double f) {
    if (f >= 1e6)        snprintf(buf, size, "%.3gMHz", f / 1e6);
    else if (f >= 1e3)   snprintf(buf, size, "%.3gkHz", f / 1e3);
    else                 snprintf(buf, size, "%.3gHz", f);
}

// Linear interpolation of the captured trace of channel ch at absolute time t.
// Returns false when t is outside the captured span.
static bool scope_value_at(UIState *ui, int ch, double t, double *out) {
    int n = ui->scope_capture_count;
    if (ch < 0 || ch >= MAX_PROBES || n < 2) return false;
    if (t < ui->scope_capture_times[0] || t > ui->scope_capture_times[n - 1]) return false;
    int lo = 0, hi = n - 1;
    while (hi - lo > 1) {
        int mid = (lo + hi) / 2;
        if (ui->scope_capture_times[mid] <= t) lo = mid; else hi = mid;
    }
    double t0 = ui->scope_capture_times[lo], t1 = ui->scope_capture_times[hi];
    double v0 = ui->scope_capture_values[ch][lo], v1 = ui->scope_capture_values[ch][hi];
    double f = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0;
    *out = v0 + (v1 - v0) * f;
    return true;
}

// Gated measurements of channel ch between absolute times ta..tb (Tek "gate to cursors")
static bool scope_gated_stats(UIState *ui, int ch, double ta, double tb,
                              double *vmin, double *vmax, double *vmean, double *vrms) {
    if (ta > tb) { double t = ta; ta = tb; tb = t; }
    int n = ui->scope_capture_count, cnt = 0;
    double mn = 1e300, mx = -1e300, sum = 0, sq = 0;
    for (int i = 0; i < n; i++) {
        double t = ui->scope_capture_times[i];
        if (t < ta || t > tb) continue;
        double v = ui->scope_capture_values[ch][i];
        if (v < mn) mn = v; if (v > mx) mx = v; sum += v; sq += v * v; cnt++;
    }
    if (cnt < 2) return false;
    *vmin = mn; *vmax = mx; *vmean = sum / cnt; *vrms = sqrt(sq / cnt);
    return true;
}

/* The volts/div this channel is drawn at. Channels start at 0, meaning "follow the main
   knob", so a scope nobody has touched behaves exactly as it always did. */
double ui_channel_volt_div(const UIState *ui, int ch) {
    if (!ui) return 1.0;
    if (ch >= 0 && ch < MAX_PROBES && ui->scope_channels[ch].volt_div > 0)
        return ui->scope_channels[ch].volt_div;
    return ui->scope_volt_div;
}

// Band geometry of channel ch (matches the Y-T plotting code, incl. stacked view)
static void scope_channel_frame(UIState *ui, Rect *r, int ch, int *top, int *h, int *center, double *scale) {
    *top = r->y; *h = r->h; *center = r->y + r->h / 2;
    *scale = (r->h / 8.0) / ui_channel_volt_div(ui, ch);
    if (!ui->scope_stacked) return;
    int n_en = 0, idx = 0;
    for (int c = 0; c < ui->scope_num_channels && c < MAX_PROBES; c++) {
        if (!ui->scope_channels[c].enabled) continue;
        if (c == ch) idx = n_en;
        n_en++;
    }
    if (n_en < 2) return;
    *top = r->y + (idx * r->h) / n_en;
    *h = r->y + ((idx + 1) * r->h) / n_en - *top;
    *center = *top + *h / 2;
    *scale = (*h / 8.0) / ui_channel_volt_div(ui, ch);
}

static void format_volt_value(char *buf, size_t size, double val) {
    if (val >= 1e6) {
        snprintf(buf, size, "%.3gMV", val / 1e6);
    } else if (val >= 1e3) {
        snprintf(buf, size, "%.3gkV", val / 1e3);
    } else if (val >= 1.0) {
        snprintf(buf, size, "%.0fV", val);
    } else if (val >= 0.1) {
        snprintf(buf, size, "%.0fmV", val * 1000);
    } else {
        snprintf(buf, size, "%.1fmV", val * 1000);
    }
}

// Cardinal spline interpolation with tension for smooth waveform rendering
// Returns interpolated value between p1 and p2 at parameter t (0 to 1)
// p0 and p3 are the neighboring points used to calculate the curve tangents
// Tension controls overshoot: 0 = Catmull-Rom (max smoothness, can overshoot)
//                             0.5 = balanced (smooth with reduced overshoot)
//                             1 = linear (no overshoot, not smooth)
#define SPLINE_TENSION 0.5

static double cardinal_spline_interp(double p0, double p1, double p2, double p3, double t) {
    double t2 = t * t;
    double t3 = t2 * t;

    // Cardinal spline with tension parameter
    double s = (1.0 - SPLINE_TENSION) / 2.0;

    double a = -s*p0 + (2.0-s)*p1 + (s-2.0)*p2 + s*p3;
    double b = 2.0*s*p0 + (s-3.0)*p1 + (3.0-2.0*s)*p2 - s*p3;
    double c = -s*p0 + s*p2;
    double d = p1;

    double result = a*t3 + b*t2 + c*t + d;

    // Clamp to prevent excessive overshoot beyond the range of all 4 control points
    // This prevents wild oscillations while still allowing smooth curves at peaks
    double all_min = p0;
    if (p1 < all_min) all_min = p1;
    if (p2 < all_min) all_min = p2;
    if (p3 < all_min) all_min = p3;

    double all_max = p0;
    if (p1 > all_max) all_max = p1;
    if (p2 > all_max) all_max = p2;
    if (p3 > all_max) all_max = p3;

    // Allow 15% margin beyond the sample range for smooth peak interpolation
    double range = all_max - all_min;
    double margin = range * 0.15;
    if (margin < 0.001) margin = 0.001;  // Minimum margin for flat signals

    if (result < all_min - margin) result = all_min - margin;
    if (result > all_max + margin) result = all_max + margin;

    return result;
}

// Number of interpolation subdivisions per sample segment
// Higher = smoother curves, but more CPU usage
#define SCOPE_INTERP_SUBDIVISIONS 8

/* The level a channel centres itself on when it is AC-coupled or drawn in a fitted band.
 *
 * The mean of the captured window is the obvious answer and it is the wrong one. The window is a
 * sliding, ragged fraction of a cycle - 250 samples one frame, 225 the next, because the capture
 * takes whatever history falls inside the time base - and the mean of a partial cycle depends on
 * which partial cycle it is. So the centre moved every frame while the waveform itself, being
 * trigger-anchored, stood perfectly still: the Common Emitter's collector read 9.0861 V one frame
 * and 9.1005 V the next, and at 235 px per volt the whole trace bounced 3.4 px up and down for
 * ever. The extremes never moved at all. It was only ever the mean.
 *
 * Average over a whole number of cycles instead, delimited by rising crossings of the mid-level -
 * that is the DC component a coupling capacitor would actually remove, and it does not depend on
 * where the window happens to end. With no cycle to find (DC, a one-shot, a ramp) fall back to
 * the midpoint of the extremes: stable, and the centre the fitted scale already assumes, since it
 * takes its volts per division from (hi - lo) / 2.
 */
static double ui_scope_dc_level(const double *v, int n) {
    /* Where a channel puts its zero line when it is AC-coupled or drawn in a fitted band: the
       midpoint of the extremes, and nothing cleverer, because stability is the property this has
       to have and the extremes are the stable thing about a captured window.

       Two other estimators were tried and both moved the trace.

       The plain mean of the window is what was here originally. The window is a ragged fraction of
       a cycle whose length changes from frame to frame - 250 samples, then 225, because the
       capture takes whatever history falls inside the time base - so the mean of it changes too,
       and the Common Emitter's trace bounced 3.4 px under a waveform that was standing perfectly
       still.

       Averaging over whole cycles between mid-level crossings fixes that and is the right DC in
       principle. But it only works while the window holds two crossings, and a window that drifts
       in and out of holding a second one switches between two different quantities: an average of
       a waveform and an average of two points. On the MOSFET Transfer Curves that was 0.0943 V
       against 0.0328 V, back and forth, and the trace jumped 14 px with it. Falling back to the
       window mean instead of the midpoint made the join continuous and put the original bounce
       back on every one-shot. There is no continuous join between those two quantities, so this
       does not attempt one.

       The midpoint is stable in every regime that was measured - periodic, one-shot, DC, stepped,
       clipped - because lo and hi do not move while the window slides across a settled waveform.
       It is also what the fitted band already assumes: it takes its volts per division from
       (hi - lo) / 2, so centring on (hi + lo) / 2 puts the waveform symmetrically in its band.

       What it gives up: on an asymmetric waveform an AC-coupled view centres on the middle of the
       swing rather than on the true mean, so a 10 %% duty pulse sits centred rather than mostly
       below the line the way a coupling capacitor would put it. That is a difference in where a
       trace is drawn, not in what it says, and it buys a display that holds still. */
    if (!v || n <= 0) return 0.0;
    double lo = v[0], hi = v[0];
    for (int i = 1; i < n; i++) { if (v[i] < lo) lo = v[i]; if (v[i] > hi) hi = v[i]; }
    return 0.5 * (lo + hi);
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
    snprintf(trig_ch_label, sizeof(trig_ch_label), "%s", ui_channel_name(ui, ui->trigger_channel));
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
    /* Not while the channels are stacked, with or without Fit.
       A stacked channel is drawn with eight divisions inside its OWN band - ch_scale is
       (band_h / 8) / volts-per-division - so with two channels the screen carries sixteen
       divisions of signal while this axis labels eight, and its 0 V line falls between the two
       bands rather than at either channel's zero. Every number on it is wrong by the number of
       channels. Fit was already excluded for the related reason that each band picks its own
       scale; the rest of stacked is no better off. The volts per division is in the readout row
       under the scope, and each band carries its own label when Fit is on. */
    for (int i = 0; i <= 8 && !ui->scope_stacked; i++) {   // Fit: every band has its ownscale (tag per band)
        int y = r->y + i * div_y;
        // Calculate voltage value: top is +4*V/div, center is 0, bottom is -4*V/div
        double voltage = (4 - i) * ui->scope_volt_div;

        // Format voltage label
        char vlabel[16];
        if (fabs(voltage) < 0.001) {
            snprintf(vlabel, sizeof(vlabel), "0V");
        } else if (fabs(ui->scope_volt_div) >= 1e3) {
            snprintf(vlabel, sizeof(vlabel), "%+.3gkV", voltage / 1e3);
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

            // Always request all available history - the simulation uses adaptive decimation
            // so we can't predict how many samples cover a given time span
            int samples_for_trigger = MAX_HISTORY;

            // Get trigger channel data for trigger detection
            double trig_times[MAX_HISTORY];
            double trig_values[MAX_HISTORY];
            int trig_ch = ui->trigger_channel;
            if (trig_ch >= ui->scope_num_channels) trig_ch = 0;
            int trig_probe = ui->scope_channels[trig_ch].probe_idx;
            int trig_count = 0;

            // Get trigger data (simulation_get_history returns 0 for invalid probes)
            if (trig_probe >= 0) {
                trig_count = simulation_get_history(sim, trig_probe, trig_times, trig_values, samples_for_trigger);
            }

            // If trigger channel has no data, find any channel with valid data
            // This ensures DC signals display even when trigger source probe was removed
            if (trig_count < 2) {
                for (int ch = 0; ch < ui->scope_num_channels && ch < MAX_PROBES; ch++) {
                    if (!ui->scope_channels[ch].enabled) continue;
                    int ch_probe = ui->scope_channels[ch].probe_idx;
                    if (ch_probe < 0) continue;
                    int ch_count = simulation_get_history(sim, ch_probe, trig_times, trig_values, samples_for_trigger);
                    if (ch_count >= 2) {
                        trig_count = ch_count;
                        trig_ch = ch;
                        trig_probe = ch_probe;
                        break;
                    }
                }
            }

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

                // Search BACKWARDS from end of buffer to find the MOST RECENT trigger edge
                // This matches real oscilloscope behavior (Tektronix style)
                // We need enough samples after the trigger for post-trigger display
                // trigger_position determines where trigger appears on screen:
                // - 0.0 = left edge (all post-trigger)
                // - 0.5 = center (50% pre, 50% post)
                // - 1.0 = right edge (all pre-trigger)
                trigger_idx = ui_scope_find_trigger(ui, trig_times, trig_values, trig_count,
                                                   time_window, level);
            }

            // Handle trigger modes
            bool use_capture = false;

            if (trigger_idx >= 0) {
                // Found a trigger - capture the data CENTERED around the trigger point.
                // Interpolate the crossing between the two straddling samples: with 15-20 samples
                // per division a sample-quantised trigger point jitters the whole trace by up to
                // one step every refresh.
                double trigger_time = trig_times[trigger_idx];
                {
                    double v0 = trig_values[trigger_idx - 1], v1 = trig_values[trigger_idx];
                    double t0 = trig_times[trigger_idx - 1], t1 = trig_times[trigger_idx];
                    if (v1 != v0) {
                        double fr = (ui->trigger_level - v0) / (v1 - v0);
                        if (fr < 0) fr = 0; if (fr > 1) fr = 1;
                        trigger_time = t0 + fr * (t1 - t0);
                    }
                }
                ui->scope_last_trigger_time = trigger_time;
                ui->scope_trigger_sample_idx = trigger_idx;
                ui->triggered = true;

                // Calculate time window centered around trigger based on trigger_position
                // trigger_position is where trigger appears on screen (0.0=left, 0.5=center, 1.0=right)
                // Pre-trigger time = trigger_position * time_window
                // Post-trigger time = (1 - trigger_position) * time_window
                double pre_trigger_time = ui->trigger_position * time_window;
                double post_trigger_time = (1.0 - ui->trigger_position) * time_window;

                // Add 25% margin for display
                double t_start = trigger_time - pre_trigger_time * 1.25;
                double t_end = trigger_time + post_trigger_time * 1.25;

                // Find first sample index at or after t_start
                int window_start = 0;
                for (int i = 0; i < trig_count; i++) {
                    if (trig_times[i] >= t_start) {
                        window_start = i;
                        break;
                    }
                }

                // Find last sample index at or before t_end
                int window_end = trig_count - 1;
                for (int i = trig_count - 1; i >= 0; i--) {
                    if (trig_times[i] <= t_end) {
                        window_end = i;
                        break;
                    }
                }

                int window_samples = window_end - window_start + 1;

                // Only subsample if needed to fit in capture buffer
                int subsample = 1;
                if (window_samples > SCOPE_CAPTURE_SIZE - 10) {
                    subsample = (window_samples + SCOPE_CAPTURE_SIZE - 11) / (SCOPE_CAPTURE_SIZE - 10);
                    if (subsample < 1) subsample = 1;
                }

                int capture_samples = (window_samples + subsample - 1) / subsample;
                if (capture_samples > SCOPE_CAPTURE_SIZE - 10) capture_samples = SCOPE_CAPTURE_SIZE - 10;

                ui->scope_capture_count = capture_samples;

                // Capture times with subsampling from window start
                for (int i = 0; i < ui->scope_capture_count; i++) {
                    int src_idx = window_start + i * subsample;
                    if (src_idx > window_end) src_idx = window_end;
                    ui->scope_capture_times[i] = trig_times[src_idx];
                }

                // Capture all channel values with subsampling from window start
                for (int ch = 0; ch < ui->scope_num_channels && ch < MAX_PROBES; ch++) {
                    double ch_times[MAX_HISTORY], ch_values[MAX_HISTORY];
                    int ch_probe = ui->scope_channels[ch].probe_idx;
                    int ch_count = simulation_get_history(sim, ch_probe, ch_times, ch_values, samples_for_trigger);

                    // Calculate corresponding window start for this channel
                    int ch_window_start = 0;
                    for (int i = 0; i < ch_count; i++) {
                        if (ch_times[i] >= t_start) {
                            ch_window_start = i;
                            break;
                        }
                    }

                    for (int i = 0; i < ui->scope_capture_count; i++) {
                        int src_idx = ch_window_start + i * subsample;
                        if (src_idx < ch_count) {
                            ui->scope_capture_values[ch][i] = ch_values[src_idx];
                        }
                    }
                }

                ui->scope_capture_time = sim->time;
                ui->scope_capture_valid = true;
                use_capture = true;
            } else {
                // No trigger found
                if (ui->trigger_mode == TRIG_AUTO) {
                    // AUTO mode: free-run, show latest data without triggering
                    // (NORMAL mode should hold last capture, not free-run)

                    // Find samples covering the visible time window (+ 50% margin)
                    double target_time_span = time_window * 1.5;
                    double t_end = trig_times[trig_count - 1];
                    double t_start = t_end - target_time_span;

                    // Find first sample index at or after t_start
                    int window_start = 0;
                    for (int i = 0; i < trig_count; i++) {
                        if (trig_times[i] >= t_start) {
                            window_start = i;
                            break;
                        }
                    }

                    int window_samples = trig_count - window_start;

                    // Only subsample if needed to fit in capture buffer
                    int subsample = 1;
                    if (window_samples > SCOPE_CAPTURE_SIZE - 10) {
                        subsample = (window_samples + SCOPE_CAPTURE_SIZE - 11) / (SCOPE_CAPTURE_SIZE - 10);
                        if (subsample < 1) subsample = 1;
                    }

                    int capture_samples = (window_samples + subsample - 1) / subsample;
                    if (capture_samples > SCOPE_CAPTURE_SIZE - 10) capture_samples = SCOPE_CAPTURE_SIZE - 10;

                    ui->scope_capture_count = capture_samples;

                    // Capture times with subsampling from window start
                    for (int i = 0; i < ui->scope_capture_count; i++) {
                        int src_idx = window_start + i * subsample;
                        if (src_idx >= trig_count) src_idx = trig_count - 1;
                        ui->scope_capture_times[i] = trig_times[src_idx];
                    }

                    // Capture all channel values with subsampling from window start
                    for (int ch = 0; ch < ui->scope_num_channels && ch < MAX_PROBES; ch++) {
                        double ch_times[MAX_HISTORY], ch_values[MAX_HISTORY];
                        int ch_probe = ui->scope_channels[ch].probe_idx;
                        int ch_count = simulation_get_history(sim, ch_probe, ch_times, ch_values, samples_for_trigger);

                        // Calculate corresponding window start for this channel
                        int ch_window_start = 0;
                        for (int i = 0; i < ch_count; i++) {
                            if (ch_times[i] >= t_start) {
                                ch_window_start = i;
                                break;
                            }
                        }

                        for (int i = 0; i < ui->scope_capture_count; i++) {
                            int src_idx = ch_window_start + i * subsample;
                            if (src_idx < ch_count) {
                                ui->scope_capture_values[ch][i] = ch_values[src_idx];
                            }
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

            /* Nothing new to draw, but something was drawn before: hold it, the way a bench scope
               holds its last sweep. The recorder can be briefly empty - a change of time/div
               re-derives its sample spacing - and blanking the screen for those frames is what
               made the display flicker on the way to a wider window. */
            if (!use_capture && trig_count < 2 && ui->scope_capture_valid &&
                ui->scope_capture_count >= 2)
                use_capture = true;

            // Render the captured/current waveform
            if (use_capture && ui->scope_capture_count >= 2) {
                double t_start = ui->scope_capture_times[0];
                double t_end = ui->scope_capture_times[ui->scope_capture_count - 1];
                double t_span = t_end - t_start;

                // Always use time_window for proper scaling relative to time/div setting
                // This ensures the grid divisions match the time/div setting exactly
                double display_time_span = time_window;
                double t_reference;

                // Check if we have enough data to fill the time window
                // Use 90% threshold to account for sampling granularity
                if (t_span >= time_window * 0.9) {
                    // When triggered (NORMAL/SINGLE mode), position waveform so trigger
                    // appears at the trigger_position (horizontal trigger position)
                    // AUTO mode included: a found trigger anchors the trace; only when none was
                    // found does AUTO free-run (that is what AUTO means on a real scope)
                    /* The remembered trigger time anchors the trace, whether or not this
                       particular frame ran a search. It often does not: the holdoff suppresses
                       one, and SINGLE stops after the first. Requiring a fresh trigger_idx meant
                       those frames fell through to the free-running branch below and the trace
                       slid sideways by however far the simulation had advanced - which is what
                       "it does not trigger properly" looks like, on every template, one frame in
                       two. The trigger only stops anchoring when it ages out of the buffer. */
                    if (ui->triggered &&
                        ui->scope_last_trigger_time >= t_start && ui->scope_last_trigger_time <= t_end) {
                        // Position the trigger point at trigger_position on screen
                        // t_reference is the time at x=0, trigger is at trigger_position
                        t_reference = ui->scope_last_trigger_time - (ui->trigger_position * time_window);
                    } else {
                        // AUTO mode or no valid trigger - anchor to right edge
                        t_reference = t_end - time_window;
                    }
                } else {
                    // Not enough data yet - anchor to left edge
                    // Data starts at x=0 and grows rightward as simulation runs
                    t_reference = t_start;
                }

                // Remember the drawn window so cursor readouts map screen -> time
                ui->scope_view_t0 = t_reference;
                ui->scope_view_span = display_time_span;

                // Stacked view: give every enabled channel its own horizontal band with its
                // own zero line and 8 divisions, so identical signals can be told apart.
                int n_enabled = 0;
                for (int ch = 0; ch < ui->scope_num_channels && ch < MAX_PROBES; ch++)
                    if (ui->scope_channels[ch].enabled) n_enabled++;
                bool stacked = ui->scope_stacked && n_enabled > 1;
                bool fit = stacked && ui->scope_stack_fit;
                int band_index = 0;
                for (int ch = 0; ch < MAX_PROBES; ch++) { ui->scope_ch_shift[ch] = 0; ui->scope_ch_scale[ch] = scale; ui->scope_band_vdiv[ch] = ui->scope_volt_div; }

                for (int ch = 0; ch < ui->scope_num_channels && ch < MAX_PROBES; ch++) {
                    if (!ui->scope_channels[ch].enabled) continue;

                    // Per-channel drawing frame (whole scope, or this channel's band)
                    int band_y = r->y, band_h = r->h;
                    int ch_center = center_y;
                    double ch_scale = (r->h / 8.0) / ui_channel_volt_div(ui, ch);
                    if (stacked) {
                        band_y = r->y + (band_index * r->h) / n_enabled;
                        band_h = r->y + ((band_index + 1) * r->h) / n_enabled - band_y;
                        ch_center = band_y + band_h / 2;
                        ch_scale = (band_h / 8.0) / ui_channel_volt_div(ui, ch);
                        // Band separator and zero line
                        if (band_index > 0) {
                            SDL_SetRenderDrawColor(renderer, 0x50, 0x60, 0x50, 0xff);
                            SDL_RenderDrawLine(renderer, r->x, band_y, r->x + r->w, band_y);
                        }
                        SDL_SetRenderDrawColor(renderer, 0x30, 0x48, 0x30, 0xff);
                        SDL_RenderDrawLine(renderer, r->x, ch_center, r->x + r->w, ch_center);
                        // Channel tag in its own colour
                        char tag[24];
                        snprintf(tag, sizeof(tag), "%s", ui_channel_name(ui, ch));
                        if (fit) {
                            /* A channel the user has scaled by hand keeps that scale; the band
                               still centres itself on the channel's own mean, which is what
                               keeps a 5 V rail on the screen while you zoom into its ripple. */
                            double manual = ui->scope_channels[ch].volt_div;
                            double v_lo = ui->scope_capture_values[ch][0], v_hi = v_lo;
                            for (int i = 1; i < ui->scope_capture_count; i++) { double v = ui->scope_capture_values[ch][i]; if (v < v_lo) v_lo = v; if (v > v_hi) v_hi = v; }
                            double amp = (v_hi - v_lo) / 2.0; if (amp < 0.01) amp = 0.01;   // DC / tiny: 10 mV floor keeps the band readable
                            double vd = amp / 3.0;   // ~6 of the 8 divisions
                            double dec = pow(10.0, floor(log10(vd))); double m = vd / dec;
                            vd = (m <= 1.0 ? 1.0 : m <= 2.0 ? 2.0 : m <= 5.0 ? 5.0 : 10.0) * dec;
                            if (manual > 0) vd = manual;
                            ui->scope_band_vdiv[ch] = vd;
                            ch_scale = (band_h / 8.0) / vd;
                            char vs[16]; format_volt_value(vs, sizeof vs, vd);
                            snprintf(tag, sizeof(tag), "%s %s/div", ui_channel_name(ui, ch), vs);
                        }
                        SDL_SetRenderDrawColor(renderer,
                            ui->scope_channels[ch].color.r,
                            ui->scope_channels[ch].color.g,
                            ui->scope_channels[ch].color.b, 0xff);
                        /* Right-aligned by what the tag actually measures, not by a fixed 34 or
                           110 px. Those two numbers were the width of "IN" and of "IN 50mV/div",
                           and any longer channel name ran off the graticule: the Termination
                           template's "SERIES" and "PARALLEL" bands were drawn as "SERI" and
                           "PARA" with the rest past the edge. 8 px a glyph, as everywhere else
                           in this panel. */
                        int tag_w = (int)strlen(tag) * 8;
                        ui_draw_text(renderer, tag, r->x + r->w - tag_w - 6, band_y + 3);
                    }
                    band_index++;

                    SDL_SetRenderDrawColor(renderer,
                        ui->scope_channels[ch].color.r,
                        ui->scope_channels[ch].color.g,
                        ui->scope_channels[ch].color.b, 0xff);

                    double offset = ui->scope_channels[ch].offset;

                    // Check if this is a DC signal (very low variance)
                    double v_min = ui->scope_capture_values[ch][0];
                    double v_max = ui->scope_capture_values[ch][0];
                    double v_sum = 0;
                    for (int i = 0; i < ui->scope_capture_count; i++) {
                        double v = ui->scope_capture_values[ch][i];
                        if (v < v_min) v_min = v;
                        if (v > v_max) v_max = v;
                        v_sum += v;
                    }
                    double v_avg = v_sum / ui->scope_capture_count;
                    double v_range = v_max - v_min;
                    bool is_dc = (v_range < 0.01);  // Less than 10mV variation = DC
                    /* AC view / fitted band: centre on the channel's own DC level, taken over
                       whole cycles rather than over the ragged captured window - ui_scope_dc_level. */
                    if (ui->scope_ac_coupling || fit)
                        offset -= ui_scope_dc_level(ui->scope_capture_values[ch], ui->scope_capture_count);
                    else if (stacked && v_min >= -0.05 * ui->scope_volt_div) offset -= 3.0 * ui->scope_volt_div;   // unipolar (logic) signal: 0 V one division above the band bottom (negative shifts the trace down)
                    ui->scope_ch_shift[ch] = offset - ui->scope_channels[ch].offset;
                    ui->scope_ch_center[ch] = ch_center;
                    ui->scope_ch_scale[ch] = ch_scale;
                    /* SCOPE_DEBUG=1 prints what each band decided: where it is, what it is
                       scaled at and what it centred on. Reading this off the screen is guesswork
                       - it is how the "the buck's ripple goes flat" report was settled. */
                    static int dbg = -1;
                    if (dbg < 0) dbg = getenv("SCOPE_DEBUG") ? 1 : 0;
                    if (dbg)
                        fprintf(stderr, "ch%d band_y=%d band_h=%d centre=%d scale=%.4g vd=%.5g "
                                "avg=%.5g lo=%.5g hi=%.5g off=%.5g n=%d\n", ch, band_y, band_h,
                                ch_center, ch_scale, ui->scope_band_vdiv[ch], v_avg, v_min, v_max,
                                offset, ui->scope_capture_count);

                    // Calculate x range for the captured data
                    double x_frac_start = (ui->scope_capture_times[0] - t_reference) / display_time_span;
                    double x_frac_end = (ui->scope_capture_times[ui->scope_capture_count - 1] - t_reference) / display_time_span;
                    int x_start = r->x + (int)(x_frac_start * r->w);
                    int x_end = r->x + (int)(x_frac_end * r->w);
                    x_start = CLAMP(x_start, r->x, r->x + r->w);
                    x_end = CLAMP(x_end, r->x, r->x + r->w);

                    if (is_dc && ui->scope_capture_count >= 2) {
                        // For DC signals, draw a horizontal line at the average voltage
                        // DC signals should ALWAYS span the full visible width since the value is constant
                        int y_dc = ch_center - (int)((v_avg + offset) * ch_scale);
                        y_dc = CLAMP(y_dc, band_y, band_y + band_h);

                        // Always draw DC line across full scope width
                        // DC voltage is constant, so there's no reason to limit the line length
                        SDL_RenderDrawLine(renderer, r->x, y_dc, r->x + r->w, y_dc);
                    } else {
                        // Simple linear waveform rendering - accurate amplitude display
                        for (int i = 1; i < ui->scope_capture_count; i++) {
                            double x_frac1 = (ui->scope_capture_times[i-1] - t_reference) / display_time_span;
                            double x_frac2 = (ui->scope_capture_times[i] - t_reference) / display_time_span;
                            int x1 = r->x + (int)(x_frac1 * r->w);
                            int x2 = r->x + (int)(x_frac2 * r->w);
                            int y1 = ch_center - (int)((ui->scope_capture_values[ch][i-1] + offset) * ch_scale);
                            int y2 = ch_center - (int)((ui->scope_capture_values[ch][i] + offset) * ch_scale);
                            x1 = CLAMP(x1, r->x, r->x + r->w);
                            x2 = CLAMP(x2, r->x, r->x + r->w);
                            y1 = CLAMP(y1, band_y, band_y + band_h);
                            y2 = CLAMP(y2, band_y, band_y + band_h);
                            SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
                        }
                    }

                    // Draw ground reference arrow on left side (channel color)
                    int gnd_y = ch_center - (int)(offset * ch_scale);
                    gnd_y = CLAMP(gnd_y, band_y + 8, band_y + band_h - 8);
                    SDL_RenderDrawLine(renderer, r->x + 2, gnd_y, r->x + 8, gnd_y);
                    SDL_RenderDrawLine(renderer, r->x + 5, gnd_y - 3, r->x + 8, gnd_y);
                    SDL_RenderDrawLine(renderer, r->x + 5, gnd_y + 3, r->x + 8, gnd_y);
                }

                // Draw trigger point marker (small T on the waveform)
                if (trigger_idx >= 0) {
                    // Show trigger indicator at the trigger_position (AUTO too: it tells you the level worked)
                    int trig_x = r->x + (int)(ui->trigger_position * r->w);
                    trig_x = CLAMP(trig_x, r->x, r->x + r->w);
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
                    int t_center = center_y, t_top = r->y, t_h = r->h;
                    double t_scale = (r->h / 8.0) / ui_channel_volt_div(ui, trig_ch);
                    if (ui->scope_stacked) {
                        int n_en = 0, idx = 0;
                        for (int ch = 0; ch < ui->scope_num_channels && ch < MAX_PROBES; ch++) {
                            if (!ui->scope_channels[ch].enabled) continue;
                            if (ch == trig_ch) idx = n_en;
                            n_en++;
                        }
                        if (n_en > 1) {
                            t_top = r->y + (idx * r->h) / n_en;
                            t_h = r->y + ((idx + 1) * r->h) / n_en - t_top;
                            t_center = t_top + t_h / 2;
                            t_scale = (t_h / 8.0) / ui_channel_volt_div(ui, trig_ch);
                        }
                    }
                    if (ui->scope_stacked && ui->scope_stack_fit) t_scale = ui->scope_ch_scale[trig_ch];
                    int trig_y = t_center - (int)((ui->trigger_level + trig_offset + ui->scope_ch_shift[trig_ch]) * t_scale);
                    trig_y = CLAMP(trig_y, t_top, t_top + t_h);

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

            // Draw trigger position line (vertical dashed, in cyan)
            {
                int trig_pos_x = r->x + (int)(ui->trigger_position * r->w);
                trig_pos_x = CLAMP(trig_pos_x, r->x, r->x + r->w);

                SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0xff, 0xff);  // Cyan
                // Draw dashed vertical line
                for (int y = r->y; y < r->y + r->h; y += 8) {
                    SDL_RenderDrawLine(renderer, trig_pos_x, y, trig_pos_x, MIN(y + 4, r->y + r->h));
                }
                // Draw trigger position indicator at bottom (down arrow)
                SDL_RenderDrawLine(renderer, trig_pos_x, r->y + r->h - 8, trig_pos_x, r->y + r->h - 2);
                SDL_RenderDrawLine(renderer, trig_pos_x - 3, r->y + r->h - 5, trig_pos_x, r->y + r->h - 2);
                SDL_RenderDrawLine(renderer, trig_pos_x + 3, r->y + r->h - 5, trig_pos_x, r->y + r->h - 2);
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

    // Measurement cursors (Y-T mode only)
    //   type 1  waveform cursors: two time cursors that read the source trace (a, b, delta,
    //           1/dt, dV/dt) plus gated Vpp / mean / RMS between them
    //   type 2  screen cursors: independent vertical (time) and horizontal (amplitude) bars
    if (ui->scope_cursor_mode && ui->scope_cursor_type > 0 && ui->display_mode == SCOPE_MODE_YT) {
        bool wave = (ui->scope_cursor_type == 1);
        // Source channels: cursor a defaults to the trigger channel, b to a's channel;
        // keys 1-8 (with cursors on) bind the active cursor to a channel.
        int src = ui->cursor_a_channel;
        if (src < 0 || src >= ui->scope_num_channels || !ui->scope_channels[src].enabled) src = ui->trigger_channel;
        if (src < 0 || src >= ui->scope_num_channels || !ui->scope_channels[src].enabled) {
            src = -1;
            for (int c = 0; c < ui->scope_num_channels && c < MAX_PROBES; c++)
                if (ui->scope_channels[c].enabled) { src = c; break; }
        }
        int src_b = ui->cursor_b_channel;
        if (src_b < 0 || src_b >= ui->scope_num_channels || !ui->scope_channels[src_b].enabled) src_b = src;
        /* The trace is drawn at centre - (v + offset + shift) * scale, where the shift is the
           AC/fitted centring and the scale comes from the band's own volts per division. The
           render records all three per channel precisely so this can be inverted; reading only
           the manual offset and an unfitted scale put the markers on the wrong volts whenever AC
           coupling or Fit was on - which is the scope's default for a stacked template. Falls
           back to the geometric frame before the first render has recorded anything. */
        int f_top, f_h, f_center; double f_scale;
        scope_channel_frame(ui, r, src >= 0 ? src : 0, &f_top, &f_h, &f_center, &f_scale);
        double src_offset = (src >= 0) ? ui->scope_channels[src].offset : 0.0;
        if (src >= 0 && src < MAX_PROBES && ui->scope_ch_scale[src] > 0) {
            f_center = ui->scope_ch_center[src];
            f_scale  = ui->scope_ch_scale[src];
            src_offset += ui->scope_ch_shift[src];
        }
        int fb_top, fb_h, fb_center; double fb_scale;
        scope_channel_frame(ui, r, src_b >= 0 ? src_b : 0, &fb_top, &fb_h, &fb_center, &fb_scale);
        double srcb_offset = (src_b >= 0) ? ui->scope_channels[src_b].offset : 0.0;
        if (src_b >= 0 && src_b < MAX_PROBES && ui->scope_ch_scale[src_b] > 0) {
            fb_center = ui->scope_ch_center[src_b];
            fb_scale  = ui->scope_ch_scale[src_b];
            srcb_offset += ui->scope_ch_shift[src_b];
        }

        // --- vertical (time) cursors a and b ---
        int ax = r->x + (int)(ui->cursor1_time * r->w);
        int bx = r->x + (int)(ui->cursor2_time * r->w);
        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0xff, 0xff);
        for (int y = r->y; y < r->y + r->h; y += 4)
            SDL_RenderDrawLine(renderer, ax, y, ax, MIN(y + 2, r->y + r->h));
        {
            char tag[12]; snprintf(tag, sizeof tag, "a%s%d", ui->scope_cursor_active == 1 ? "*" : ":", src + 1);
            if (src >= 0) SDL_SetRenderDrawColor(renderer, ui->scope_channels[src].color.r, ui->scope_channels[src].color.g, ui->scope_channels[src].color.b, 0xff);
            ui_draw_text(renderer, tag, ax - 8, r->y + 2);
        }
        SDL_SetRenderDrawColor(renderer, 0xff, 0x00, 0xff, 0xff);
        for (int y = r->y; y < r->y + r->h; y += 4)
            SDL_RenderDrawLine(renderer, bx, y, bx, MIN(y + 2, r->y + r->h));
        {
            char tag[12]; snprintf(tag, sizeof tag, "b%s%d", ui->scope_cursor_active == 2 ? "*" : ":", src_b + 1);
            if (src_b >= 0) SDL_SetRenderDrawColor(renderer, ui->scope_channels[src_b].color.r, ui->scope_channels[src_b].color.g, ui->scope_channels[src_b].color.b, 0xff);
            ui_draw_text(renderer, tag, bx - 8, r->y + 2);
        }

        double span = ui->scope_view_span > 0 ? ui->scope_view_span : 10.0 * ui->scope_time_div;
        double ta = ui->scope_view_t0 + ui->cursor1_time * span;
        double tb = ui->scope_view_t0 + ui->cursor2_time * span;
        double dt = tb - ta;

        // --- readout box (top-right) ---
        char line[16][40]; int nl = 0;
        char t1[24], t2[24], v1[24], v2[24];
        fmt_time_eng(t1, sizeof t1, ta - ui->scope_view_t0);
        fmt_time_eng(t2, sizeof t2, tb - ui->scope_view_t0);
        double va = 0, vb = 0; bool ha = false, hb = false;

        if (wave) {
            if (src >= 0) ha = scope_value_at(ui, src, ta, &va);
            if (src_b >= 0) hb = scope_value_at(ui, src_b, tb, &vb);
            if (src_b != src) snprintf(line[nl++], 40, "WAVE a:CH%d b:CH%d%s", src + 1, src_b + 1, ui->scope_cursor_linked ? " LNK" : "");
            else snprintf(line[nl++], 40, "WAVE CH%d%s", src + 1, ui->scope_cursor_linked ? " LINK" : "");
            if (ha) fmt_volt_eng(v1, sizeof v1, va); else snprintf(v1, sizeof v1, "--");
            if (hb) fmt_volt_eng(v2, sizeof v2, vb); else snprintf(v2, sizeof v2, "--");
            snprintf(line[nl++], 40, "a %s %s", t1, v1);
            snprintf(line[nl++], 40, "b %s %s", t2, v2);
            char dts[24]; fmt_time_eng(dts, sizeof dts, dt);
            snprintf(line[nl++], 40, "dt %s", dts);
            if (fabs(dt) > 0) { char fs[24]; fmt_freq_eng(fs, sizeof fs, 1.0 / fabs(dt)); snprintf(line[nl++], 40, "1/dt %s", fs); }
            if (ha && hb) {
                char dv[24]; fmt_volt_eng(dv, sizeof dv, vb - va);
                snprintf(line[nl++], 40, "dV %s", dv);
                if (fabs(dt) > 0) {
                    double slope = (vb - va) / dt;
                    if (fabs(slope) >= 1e3) snprintf(line[nl++], 40, "dV/dt %.3gV/ms", slope / 1e3);
                    else snprintf(line[nl++], 40, "dV/dt %.3gV/s", slope);
                }
            }
            double gmin, gmax, gmean, grms;
            if (src >= 0 && scope_gated_stats(ui, src, ta, tb, &gmin, &gmax, &gmean, &grms)) {
                char b1[24], b2[24], b3[24];
                fmt_volt_eng(b1, sizeof b1, gmax - gmin); fmt_volt_eng(b2, sizeof b2, gmean); fmt_volt_eng(b3, sizeof b3, grms);
                snprintf(line[nl++], 40, "gated a-b:");
                snprintf(line[nl++], 40, " Vpp %s", b1);
                snprintf(line[nl++], 40, " mean %s", b2);
                snprintf(line[nl++], 40, " rms %s", b3);
            }
            // Markers on the trace at the cursor positions
            if (ha) {
                int y = f_center - (int)((va + src_offset) * f_scale);
                y = CLAMP(y, f_top, f_top + f_h);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0xff, 0xff);
                SDL_Rect m = {ax - 3, y - 3, 7, 7}; SDL_RenderDrawRect(renderer, &m);
            }
            if (hb) {
                int y = fb_center - (int)((vb + srcb_offset) * fb_scale);
                y = CLAMP(y, fb_top, fb_top + fb_h);
                SDL_SetRenderDrawColor(renderer, 0xff, 0x00, 0xff, 0xff);
                SDL_Rect m = {bx - 3, y - 3, 7, 7}; SDL_RenderDrawRect(renderer, &m);
            }
        } else {
            // Screen cursors: horizontal amplitude bars read in the source channel's frame
            int ay = f_top + (int)(ui->cursor1_volt * f_h);
            int by = fb_top + (int)(ui->cursor2_volt * fb_h);
            va = (f_center - ay) / f_scale - src_offset;
            vb = (fb_center - by) / fb_scale - srcb_offset;
            SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0xff, 0xff);
            for (int x = r->x; x < r->x + r->w; x += 4) SDL_RenderDrawLine(renderer, x, ay, MIN(x + 2, r->x + r->w), ay);
            SDL_SetRenderDrawColor(renderer, 0xff, 0x00, 0xff, 0xff);
            for (int x = r->x; x < r->x + r->w; x += 4) SDL_RenderDrawLine(renderer, x, by, MIN(x + 2, r->x + r->w), by);
            if (src_b != src) snprintf(line[nl++], 40, "SCRN a:CH%d b:CH%d%s", src + 1, src_b + 1, ui->scope_cursor_linked ? " LNK" : "");
            else snprintf(line[nl++], 40, "SCREEN CH%d%s", src + 1, ui->scope_cursor_linked ? " LINK" : "");
            fmt_volt_eng(v1, sizeof v1, va); fmt_volt_eng(v2, sizeof v2, vb);
            snprintf(line[nl++], 40, "a %s %s", t1, v1);
            snprintf(line[nl++], 40, "b %s %s", t2, v2);
            char dts[24], dv[24]; fmt_time_eng(dts, sizeof dts, dt); fmt_volt_eng(dv, sizeof dv, vb - va);
            snprintf(line[nl++], 40, "dt %s  dV %s", dts, dv);
            if (fabs(dt) > 0) { char fs[24]; fmt_freq_eng(fs, sizeof fs, 1.0 / fabs(dt)); snprintf(line[nl++], 40, "1/dt %s", fs); }
        }

        int box_w = 150, box_h = nl * 12 + 6;
        int meas_x = r->x + r->w - box_w - 4;
        int meas_y = r->y + 14;
        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xc0);
        SDL_Rect meas_bg = {meas_x - 3, meas_y - 3, box_w, box_h};
        SDL_RenderFillRect(renderer, &meas_bg);
        SDL_SetRenderDrawColor(renderer, 0x60, 0x60, 0x60, 0xff);
        SDL_RenderDrawRect(renderer, &meas_bg);
        for (int i = 0; i < nl; i++) {
            if (i == 0) SDL_SetRenderDrawColor(renderer, 0xff, 0xc0, 0x40, 0xff);
            else if (line[i][0] == 'a') SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0xff, 0xff);
            else if (line[i][0] == 'b') SDL_SetRenderDrawColor(renderer, 0xff, 0x60, 0xff, 0xff);
            else SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
            ui_draw_text(renderer, line[i], meas_x, meas_y + i * 12);
        }
    }

    // Border (scope bezel)
    SDL_SetRenderDrawColor(renderer, 0x40, 0x40, 0x40, 0xff);
    SDL_RenderDrawRect(renderer, &bg);
    // Resize grip: the top and left edges can be dragged (top-left corner ticks)
    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x90, 0xff);
    for (int k = 0; k < 3; k++) {
        SDL_RenderDrawLine(renderer, r->x + 2, r->y + 4 + k * 4, r->x + 8, r->y + 4 + k * 4);
        SDL_RenderDrawLine(renderer, r->x + 4 + k * 4, r->y + 2, r->x + 4 + k * 4, r->y + 8);
    }
    SDL_Rect outer = {r->x - 1, r->y - 1, r->w + 2, r->h + 2};
    SDL_RenderDrawRect(renderer, &outer);

    // Draw control buttons (first row: V+, V-, T+, T-)
    draw_button(renderer, &ui->btn_scope_volt_up);
    draw_button(renderer, &ui->btn_scope_volt_down);
    draw_button(renderer, &ui->btn_scope_time_up);
    draw_button(renderer, &ui->btn_scope_time_down);
    /* the channel the vertical controls act on: ALL, or one channel by its own name */
    ui->btn_scope_ch_all.toggled = ui->scope_scale_all;
    draw_button(renderer, &ui->btn_scope_ch_all);
    for (int ch = 0; ch < MAX_PROBES; ch++) {
        if (ui->btn_scope_ch[ch].bounds.w <= 0) continue;
        if (ch >= ui->scope_num_channels || !ui->scope_channels[ch].enabled) continue;
        ui->btn_scope_ch[ch].label = ui_channel_name(ui, ch);
        ui->btn_scope_ch[ch].toggled = !ui->scope_scale_all && ui->scope_selected_channel == ch;
        draw_button(renderer, &ui->btn_scope_ch[ch]);
    }
    for (int t = 0; t < 3; t++) {
        ui->btn_scope_tab[t].toggled = (ui->scope_ctl_tab == t);
        draw_button(renderer, &ui->btn_scope_tab[t]);
    }

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

    // Stacked / overlay view toggle
    ui->btn_scope_stack.toggled = ui->scope_stacked;
    draw_button(renderer, &ui->btn_scope_stack);
    ui->btn_scope_ac.toggled = ui->scope_ac_coupling;
    draw_button(renderer, &ui->btn_scope_ac);
    ui->btn_scope_fit.toggled = ui->scope_stack_fit;
    ui->btn_scope_fit.enabled = ui->scope_stacked;
    draw_button(renderer, &ui->btn_scope_fit);
    ui->btn_scope_track.toggled = ui->scope_track_sweep;
    draw_button(renderer, &ui->btn_scope_track);

    // Autoset button
    draw_button(renderer, &ui->btn_scope_autoset);

    draw_button(renderer, &ui->btn_bode);

    // Monte Carlo button with toggle state indicator
    ui->btn_mc.toggled = ui->show_monte_carlo_panel;
    draw_button(renderer, &ui->btn_mc);

    // Pop-out button with toggle state indicator
    ui->btn_scope_popup.toggled = ui->scope_popped_out;
    draw_button(renderer, &ui->btn_scope_popup);

    // Display settings panel below buttons (TIME/VOLTS info, channel readings, measurements)
    // Buttons are positioned BEFORE this section (scope -> buttons -> info -> measurements)
    int info_y = ui->scope_buttons_bottom + 8;   // just below the last button row (already scrolled)

    /* A row is drawn only if it fits whole. These readouts are laid out downwards from the button
       block and the block scrolls, so the last one used to be sliced through the middle by the
       status bar - the top half of "TIME 200us  VOLTS per-ch  TRIG ..." above the bar and nothing
       below it. Clipping alone does not fix that, it just decides where the cut lands. A row that
       does not fit is left out, the panel scrolls to reach it, and the edge stays clean.
       From the renderer, not ui->window_height: the popped-out scope renders through here into a
       window of its own, and that one has no status bar. */
    int out_w = 0, out_h = 0;
    SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
    const int panel_bottom = out_h - (ui->scope_popped_out ? 0 : STATUSBAR_HEIGHT);
    const int ROW_H = 12;

    if (info_y + ROW_H <= panel_bottom) {
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
    if (ui->scope_stacked && ui->scope_stack_fit) snprintf(buf, sizeof buf, "per-ch");
    else if (ui->scope_ac_coupling) { size_t n = strlen(buf); snprintf(buf + n, sizeof buf - n, " AC"); }
    ui_draw_text(renderer, buf, r->x + 160, info_y);

    // Trigger readout: channel, level, edge, mode
    {
        static const char *modes[] = { "AUTO", "NORM", "SNGL" };
        static const char *edges[] = { "/", "\\", "X" };
        char lv[24];
        if (fabs(ui->trigger_level) >= 1000) snprintf(lv, sizeof lv, "%.3gkV", ui->trigger_level / 1e3);
        else if (fabs(ui->trigger_level) < 0.1 && ui->trigger_level != 0) snprintf(lv, sizeof lv, "%.0fmV", ui->trigger_level * 1e3);
        else snprintf(lv, sizeof lv, "%.2fV", ui->trigger_level);
        SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
        ui_draw_text(renderer, "TRIG", r->x + 230, info_y);
        SDL_SetRenderDrawColor(renderer, 0xff, 0x80, 0x00, 0xff);
        snprintf(buf, sizeof(buf), "%s %s %s %s", ui_channel_name(ui, ui->trigger_channel), lv, edges[ui->trigger_edge % 3], modes[ui->trigger_mode % 3]);
        ui_draw_text(renderer, buf, r->x + 270, info_y);
    }
    }   /* if the TIME / VOLTS / TRIG row fits */

    // Channel info with voltage readings
    info_y += 15;
    /* Advance by what was actually drawn, not by a fixed 80 px pitch. A fixed pitch worked for
       "IN:" and "OUT:" and ran channels into each other the day one was named "SW OUT" or there
       were four of them - the MOSFET curve tracer's row read "OUT:0.18VDEV2:17.6mDEV3:80.5mV".
       Wraps to a second line when the row is full; everything below already positions itself
       relative to info_y, so the measurements move down with it. */
    {
        int rx = r->x;
        for (int ch = 0; ch < ui->scope_num_channels && ch < MAX_PROBES; ch++) {
            if (!ui->scope_channels[ch].enabled) continue;

            double voltage = 0;
            if (sim && sim->circuit && ch < sim->circuit->num_probes) {
                voltage = sim->circuit->probes[ch].voltage;
            }
            { char vs[32]; ui_volt_readout(vs, sizeof vs, voltage); snprintf(buf, sizeof(buf), "%s:%s", ui_channel_name(ui, ch), vs); }

            int w = (int)strlen(buf) * 8;
            if (rx > r->x && rx + w > r->x + r->w) { rx = r->x; info_y += 13; }
            if (info_y + ROW_H > panel_bottom) break;
            SDL_SetRenderDrawColor(renderer,
                ui->scope_channels[ch].color.r,
                ui->scope_channels[ch].color.g,
                ui->scope_channels[ch].color.b, 0xff);
            ui_draw_text(renderer, buf, rx, info_y);
            rx += w + 10;
        }
    }

    // Show "No probes" message if no channels active
    if (ui->scope_num_channels == 0) {
        SDL_SetRenderDrawColor(renderer, 0x40, 0x60, 0x40, 0xff);
        ui_draw_text(renderer, "Place probes to", r->x + 70, r->y + r->h/2 - 12);
        ui_draw_text(renderer, "see waveforms", r->x + 75, r->y + r->h/2 + 4);
    }

    // Display waveform measurements panel (below channel readings, relative to scope_rect)
    if (analysis && ui->scope_num_channels > 0 && info_y + 18 + ROW_H <= panel_bottom) {
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
            snprintf(buf, sizeof(buf), "%s:", ui_channel_name(ui, ch));
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
                { char vs[32]; ui_volt_readout(vs, sizeof vs, m->v_pp); snprintf(buf, sizeof(buf), "Vpp:%s", vs); }
            }
            ui_draw_text(renderer, buf, col1_x, line_y);
            line_y += 11;

            if (m->v_rms < 1.0) {
                snprintf(buf, sizeof(buf), "Vrms:%.0fmV", m->v_rms * 1000);
            } else {
                { char vs[32]; ui_volt_readout(vs, sizeof vs, m->v_rms); snprintf(buf, sizeof(buf), "Vrms:%s", vs); }
            }
            ui_draw_text(renderer, buf, col1_x, line_y);
            line_y += 11;

            if (fabs(m->v_avg) < 1.0) {
                snprintf(buf, sizeof(buf), "Vavg:%.0fmV", m->v_avg * 1000);
            } else {
                { char vs[32]; ui_volt_readout(vs, sizeof vs, m->v_avg); snprintf(buf, sizeof(buf), "Vavg:%s", vs); }
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

    /* The scale the trace is drawn on, declared here so the grid and the 0 dB line are drawn on
       the SAME one. They were not: the trace maps -60 dB to +20 dB across the height, which puts
       0 dB a quarter of the way down, while the reference line below was drawn at r->h / 2 and
       labelled 0 dB. By the plot's own mapping that line is -20 dB. The grid said "-60 to 0 dB in
       10dB steps" in a comment and drew six even intervals across 80 dB, which is 13.3 dB each. */
    const double db_min = -60.0, db_max = 20.0;
    const double db_range = db_max - db_min;
    #define BODE_DB_Y(db) (r->y + (int)((1.0 - ((db) - db_min) / db_range) * r->h))

    // Horizontal grid lines (magnitude), every 10 dB across the range actually plotted
    for (double db = db_min; db <= db_max + 0.001; db += 10.0) {
        int y_pos = BODE_DB_Y(db);
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

    // 0 dB line (reference), from the same mapping the trace uses
    SDL_SetRenderDrawColor(renderer, 0x60, 0x60, 0x60, 0xff);
    int zero_db_y = BODE_DB_Y(0.0);
    SDL_RenderDrawLine(renderer, r->x, zero_db_y, r->x + r->w, zero_db_y);
    ui_draw_text(renderer, "0 dB", r->x + 3, zero_db_y - 10);

    // Plot frequency response data
    if (sim && sim->freq_response_count > 1) {
        // Magnitude plot (yellow)
        SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0x00, 0xff);

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
    int env_x = 400;  // Start position for environment sliders (moved right for better spacing)
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
        ui->brightness_slider = (Rect){-100, -100, 0, 0};
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

    // Screen brightness slider (25 % .. 100 %), persisted in settings.json; F3 / F4 step it
    int brt_x = ui->window_width - 400;   // right of the VM/AM readouts (end ~905), left of the t= readout (w - 250)
    if (brt_x >= 920) {
        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DIM, 0xff);
        ui_draw_text(renderer, "Brt:", brt_x, y + 8);
        ui->brightness_slider = (Rect){brt_x + text_w, slider_y, slider_w, slider_h};
        SDL_SetRenderDrawColor(renderer, SYNTH_BG_DARK, 0xff);
        SDL_Rect bb = {brt_x + text_w, slider_y, slider_w, slider_h};
        SDL_RenderFillRect(renderer, &bb);
        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT, 0x60);
        SDL_RenderDrawRect(renderer, &bb);
        int bf = (int)(slider_w * (ui->brightness - 0.25f) / 0.75f);
        bf = CLAMP(bf, 0, slider_w);
        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT, 0xff);
        SDL_Rect bfr = {brt_x + text_w, slider_y, bf, slider_h};
        SDL_RenderFillRect(renderer, &bfr);
        char bt[16]; snprintf(bt, sizeof bt, "%d%%", (int)(ui->brightness * 100 + 0.5f));
        ui_draw_text(renderer, bt, brt_x + text_w + slider_w + 4, y + 8);
    } else {
        ui->brightness_slider = (Rect){-100, -100, 0, 0};
    }
    }  // End of else block (sliders have room)
}

void ui_set_brightness(UIState *ui, float b) {
    ui->brightness = CLAMP(b, 0.25f, 1.0f);
    char msg[64]; snprintf(msg, sizeof msg, "Brightness %d%% (F3 / F4, saved on exit)", (int)(ui->brightness * 100 + 0.5f));
    ui_set_status(ui, msg);
}

void ui_render_brightness(UIState *ui, SDL_Renderer *renderer, int w, int h) {
    if (ui->brightness >= 0.995f) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, (Uint8)((1.0f - ui->brightness) * 255.0f));
    SDL_Rect all = {0, 0, w, h};
    SDL_RenderFillRect(renderer, &all);
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
                str_contains_ci(info->short_name, ui->spotlight_query) ||
                str_contains_ci(component_search_keywords((ComponentType)i), ui->spotlight_query)) {
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

// Handle mouse click on spotlight search dialog
// Returns selected ComponentType if a result was clicked, COMP_NONE otherwise
ComponentType ui_spotlight_click(UIState *ui, int mouse_x, int mouse_y) {
    if (!ui->show_spotlight) return COMP_NONE;
    if (ui->spotlight_num_results <= 0) return COMP_NONE;

    // Dialog dimensions - must match ui_render_spotlight
    int dw = 450, max_results = 10;
    int dx = (ui->window_width - dw) / 2;
    int dy = ui->window_height / 5;
    int results_start_y = dy + 48;
    int item_height = 28;
    int visible = MIN(ui->spotlight_num_results, max_results);

    // Check if click is within the results area
    int results_area_x = dx + 5;
    int results_area_w = dw - 10;

    if (mouse_x >= results_area_x && mouse_x < results_area_x + results_area_w) {
        // Calculate which result item was clicked
        int rel_y = mouse_y - results_start_y + 2;  // +2 to account for -2 offset in render
        if (rel_y >= 0) {
            int clicked_index = rel_y / item_height;
            if (clicked_index >= 0 && clicked_index < visible) {
                // Select and confirm the clicked item
                ui->spotlight_selected = clicked_index;
                ComponentType result = ui->spotlight_results[clicked_index];
                ui_spotlight_close(ui);
                return result;
            }
        }
    }

    // Click was outside results - check if outside dialog entirely to close
    int dh = 50 + (ui->spotlight_num_results > 0 ? visible * 28 + 10 : 0);
    if (mouse_x < dx || mouse_x >= dx + dw || mouse_y < dy || mouse_y >= dy + dh) {
        ui_spotlight_close(ui);
    }

    return COMP_NONE;
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

// ============================================================================
// SUBCIRCUIT DIALOG FUNCTIONS
// ============================================================================

// Open the subcircuit creation dialog
void ui_subcircuit_dialog_open(UIState *ui, int num_selected, int detected_pins, char detected_names[][16]) {
    ui->show_subcircuit_dialog = true;
    ui->subcircuit_name[0] = '\0';
    ui->subcircuit_name_cursor = 0;
    ui->subcircuit_editing_field = 0;  // Start with name field
    ui->subcircuit_selected_pin = 0;
    ui->subcircuit_editing_def_id = -1;  // Creating new, not editing

    // Use detected pins if available, otherwise fall back to default
    if (detected_pins > 0) {
        ui->subcircuit_num_pins = MIN(detected_pins, 16);
        // Copy detected pin names
        for (int i = 0; i < ui->subcircuit_num_pins; i++) {
            if (detected_names && detected_names[i][0] != '\0') {
                strncpy(ui->subcircuit_pin_names[i], detected_names[i], sizeof(ui->subcircuit_pin_names[i]) - 1);
                ui->subcircuit_pin_names[i][sizeof(ui->subcircuit_pin_names[i]) - 1] = '\0';
            } else {
                snprintf(ui->subcircuit_pin_names[i], sizeof(ui->subcircuit_pin_names[i]), "P%d", i + 1);
            }
        }
        // Initialize remaining slots with defaults
        for (int i = ui->subcircuit_num_pins; i < 16; i++) {
            snprintf(ui->subcircuit_pin_names[i], sizeof(ui->subcircuit_pin_names[i]), "P%d", i + 1);
        }
    } else {
        // Fall back to default behavior
        ui->subcircuit_num_pins = (num_selected > 0) ? MIN(num_selected * 2, 8) : 4;
        for (int i = 0; i < 16; i++) {
            snprintf(ui->subcircuit_pin_names[i], sizeof(ui->subcircuit_pin_names[i]), "P%d", i + 1);
        }
    }
    SDL_StartTextInput();
}

// Open the subcircuit dialog to edit an existing subcircuit definition
void ui_subcircuit_dialog_open_edit(UIState *ui, int def_id) {
    // Find the subcircuit definition
    SubCircuitDef *def = NULL;
    for (int i = 0; i < g_subcircuit_library.count; i++) {
        if (g_subcircuit_library.defs[i].id == def_id) {
            def = &g_subcircuit_library.defs[i];
            break;
        }
    }

    if (!def) return;  // Definition not found

    ui->show_subcircuit_dialog = true;
    ui->subcircuit_editing_def_id = def_id;  // Editing existing
    ui->subcircuit_editing_field = 0;  // Start with name field
    ui->subcircuit_selected_pin = 0;

    // Copy name from definition
    strncpy(ui->subcircuit_name, def->name, sizeof(ui->subcircuit_name) - 1);
    ui->subcircuit_name[sizeof(ui->subcircuit_name) - 1] = '\0';
    ui->subcircuit_name_cursor = (int)strlen(ui->subcircuit_name);

    // Copy pin count and names from definition
    ui->subcircuit_num_pins = def->num_pins;
    for (int i = 0; i < def->num_pins && i < 16; i++) {
        strncpy(ui->subcircuit_pin_names[i], def->pins[i].name, sizeof(ui->subcircuit_pin_names[i]) - 1);
        ui->subcircuit_pin_names[i][sizeof(ui->subcircuit_pin_names[i]) - 1] = '\0';
    }
    // Initialize remaining slots with defaults
    for (int i = def->num_pins; i < 16; i++) {
        snprintf(ui->subcircuit_pin_names[i], sizeof(ui->subcircuit_pin_names[i]), "P%d", i + 1);
    }
    SDL_StartTextInput();
}

// Close the subcircuit creation dialog
void ui_subcircuit_dialog_close(UIState *ui) {
    ui->show_subcircuit_dialog = false;
    SDL_StopTextInput();
}

// Handle text input for subcircuit dialog
void ui_subcircuit_dialog_text_input(UIState *ui, const char *text) {
    if (!ui->show_subcircuit_dialog) return;

    if (ui->subcircuit_editing_field == 0) {
        // Editing name
        size_t len = strlen(ui->subcircuit_name);
        size_t text_len = strlen(text);
        if (len + text_len < sizeof(ui->subcircuit_name) - 1) {
            // Insert at cursor position
            memmove(&ui->subcircuit_name[ui->subcircuit_name_cursor + text_len],
                   &ui->subcircuit_name[ui->subcircuit_name_cursor],
                   len - ui->subcircuit_name_cursor + 1);
            memcpy(&ui->subcircuit_name[ui->subcircuit_name_cursor], text, text_len);
            ui->subcircuit_name_cursor += (int)text_len;
        }
    } else {
        // Editing pin name
        int pin_idx = ui->subcircuit_editing_field - 1;
        if (pin_idx >= 0 && pin_idx < ui->subcircuit_num_pins) {
            size_t len = strlen(ui->subcircuit_pin_names[pin_idx]);
            size_t text_len = strlen(text);
            if (len + text_len < sizeof(ui->subcircuit_pin_names[pin_idx]) - 1) {
                strcat(ui->subcircuit_pin_names[pin_idx], text);
            }
        }
    }
}

// Handle key events for subcircuit dialog
// Returns true if a subcircuit should be created (Enter pressed with valid name)
bool ui_subcircuit_dialog_key(UIState *ui, SDL_Keycode key) {
    if (!ui->show_subcircuit_dialog) return false;

    if (key == SDLK_ESCAPE) {
        ui_subcircuit_dialog_close(ui);
        return false;
    }

    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        // Confirm creation if name is not empty
        if (ui->subcircuit_name[0] != '\0') {
            ui_subcircuit_dialog_close(ui);  // Close dialog before returning
            return true;  // Signal to create the subcircuit
        }
        return false;
    }

    if (key == SDLK_TAB) {
        // Cycle through fields (name, then pin names)
        ui->subcircuit_editing_field++;
        if (ui->subcircuit_editing_field > ui->subcircuit_num_pins) {
            ui->subcircuit_editing_field = 0;
        }
        return false;
    }

    if (key == SDLK_UP) {
        if (ui->subcircuit_editing_field > 0) {
            ui->subcircuit_editing_field--;
        }
        return false;
    }

    if (key == SDLK_DOWN) {
        if (ui->subcircuit_editing_field < ui->subcircuit_num_pins) {
            ui->subcircuit_editing_field++;
        }
        return false;
    }

    if (key == SDLK_BACKSPACE) {
        if (ui->subcircuit_editing_field == 0) {
            // Editing name
            if (ui->subcircuit_name_cursor > 0) {
                size_t len = strlen(ui->subcircuit_name);
                memmove(&ui->subcircuit_name[ui->subcircuit_name_cursor - 1],
                       &ui->subcircuit_name[ui->subcircuit_name_cursor],
                       len - ui->subcircuit_name_cursor + 1);
                ui->subcircuit_name_cursor--;
            }
        } else {
            // Editing pin name
            int pin_idx = ui->subcircuit_editing_field - 1;
            if (pin_idx >= 0 && pin_idx < ui->subcircuit_num_pins) {
                size_t len = strlen(ui->subcircuit_pin_names[pin_idx]);
                if (len > 0) {
                    ui->subcircuit_pin_names[pin_idx][len - 1] = '\0';
                }
            }
        }
        return false;
    }

    // Adjust number of pins with +/-
    if (key == SDLK_PLUS || key == SDLK_KP_PLUS || key == SDLK_EQUALS) {
        if (ui->subcircuit_num_pins < 16) {
            ui->subcircuit_num_pins++;
        }
        return false;
    }

    if (key == SDLK_MINUS || key == SDLK_KP_MINUS) {
        if (ui->subcircuit_num_pins > 2) {
            ui->subcircuit_num_pins--;
        }
        return false;
    }

    return false;
}

// Handle mouse click on subcircuit dialog
// Returns true if dialog should close
bool ui_subcircuit_dialog_click(UIState *ui, int mouse_x, int mouse_y) {
    if (!ui->show_subcircuit_dialog) return false;

    // Dialog dimensions
    int dw = 400, dh = 350;
    int dx = (ui->window_width - dw) / 2;
    int dy = (ui->window_height - dh) / 2;

    // Check if click is outside dialog to close
    if (mouse_x < dx || mouse_x >= dx + dw || mouse_y < dy || mouse_y >= dy + dh) {
        ui_subcircuit_dialog_close(ui);
        return false;
    }

    // Check name field click (y = dy + 45 to dy + 73)
    if (mouse_y >= dy + 45 && mouse_y < dy + 73) {
        ui->subcircuit_editing_field = 0;
        return false;
    }

    // Check pin name field clicks (2 column layout starting at y = dy + 130)
    int pin_start_y = dy + 130;
    int pin_height = 24;
    int pin_col_width = dw / 2 - 20;  // Same as rendering
    for (int i = 0; i < ui->subcircuit_num_pins; i++) {
        int col = i % 2;
        int row = i / 2;
        int px = dx + 15 + col * pin_col_width;
        int py = pin_start_y + row * pin_height;
        // Check if click is within this pin's input field area
        if (mouse_x >= px && mouse_x < px + 140 &&
            mouse_y >= py && mouse_y < py + pin_height) {
            ui->subcircuit_editing_field = i + 1;
            return false;
        }
    }

    // Check Create button (bottom right)
    int btn_x = dx + dw - 90;
    int btn_y = dy + dh - 45;
    if (mouse_x >= btn_x && mouse_x < btn_x + 80 &&
        mouse_y >= btn_y && mouse_y < btn_y + 30) {
        if (ui->subcircuit_name[0] != '\0') {
            ui_subcircuit_dialog_close(ui);  // Close dialog before returning
            return true;  // Create subcircuit
        }
    }

    // Check Cancel button (bottom left)
    btn_x = dx + 10;
    if (mouse_x >= btn_x && mouse_x < btn_x + 80 &&
        mouse_y >= btn_y && mouse_y < btn_y + 30) {
        ui_subcircuit_dialog_close(ui);
        return false;
    }

    return false;
}

// Render subcircuit creation dialog
void ui_render_subcircuit_dialog(UIState *ui, SDL_Renderer *renderer) {
    if (!ui->show_subcircuit_dialog) return;

    // Semi-transparent overlay
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xb0);
    SDL_Rect overlay = {0, 0, ui->window_width, ui->window_height};
    SDL_RenderFillRect(renderer, &overlay);

    // Dialog dimensions
    int dw = 400, dh = 350;
    int dx = (ui->window_width - dw) / 2;
    int dy = (ui->window_height - dh) / 2;

    // Dialog background
    SDL_SetRenderDrawColor(renderer, SYNTH_BG_MID, 0xff);
    SDL_Rect dialog = {dx, dy, dw, dh};
    SDL_RenderFillRect(renderer, &dialog);

    // Outer glow effect
    SDL_SetRenderDrawColor(renderer, SYNTH_PURPLE, 0x60);
    SDL_Rect glow1 = {dx - 2, dy - 2, dw + 4, dh + 4};
    SDL_RenderDrawRect(renderer, &glow1);
    SDL_SetRenderDrawColor(renderer, SYNTH_PURPLE, 0x30);
    SDL_Rect glow2 = {dx - 4, dy - 4, dw + 8, dh + 8};
    SDL_RenderDrawRect(renderer, &glow2);

    // Main border
    SDL_SetRenderDrawColor(renderer, SYNTH_PURPLE, 0xff);
    SDL_RenderDrawRect(renderer, &dialog);

    // Title - different for create vs edit mode
    SDL_SetRenderDrawColor(renderer, SYNTH_PINK, 0xff);
    bool is_editing = (ui->subcircuit_editing_def_id >= 0);
    ui_draw_text(renderer, is_editing ? "EDIT SUBCIRCUIT" : "CREATE SUBCIRCUIT", dx + 120, dy + 15);

    // Name label and field
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DIM, 0xff);
    ui_draw_text(renderer, "Name:", dx + 15, dy + 50);

    // Name input field
    bool name_focused = (ui->subcircuit_editing_field == 0);
    SDL_SetRenderDrawColor(renderer, SYNTH_BG_DARK, 0xff);
    SDL_Rect name_bg = {dx + 70, dy + 45, dw - 90, 28};
    SDL_RenderFillRect(renderer, &name_bg);
    SDL_SetRenderDrawColor(renderer, name_focused ? SYNTH_CYAN : SYNTH_BORDER, 0xff);
    SDL_RenderDrawRect(renderer, &name_bg);

    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT, 0xff);
    if (ui->subcircuit_name[0] != '\0') {
        ui_draw_text(renderer, ui->subcircuit_name, dx + 78, dy + 53);
    } else {
        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DARK, 0xff);
        ui_draw_text(renderer, "Enter IC name...", dx + 78, dy + 53);
    }

    // Blinking cursor for name field
    if (name_focused && (SDL_GetTicks() / 500) % 2 == 0) {
        SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0xff);
        int cursor_x = dx + 78 + ui->subcircuit_name_cursor * 8;
        SDL_RenderDrawLine(renderer, cursor_x, dy + 49, cursor_x, dy + 69);
    }

    // Number of pins section
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DIM, 0xff);
    char pins_str[32];
    snprintf(pins_str, sizeof(pins_str), "Pins: %d  (+/- to adjust)", ui->subcircuit_num_pins);
    ui_draw_text(renderer, pins_str, dx + 15, dy + 85);

    // Pin names section
    SDL_SetRenderDrawColor(renderer, SYNTH_PINK, 0xff);
    ui_draw_text(renderer, "Pin Names:", dx + 15, dy + 110);

    int pin_start_y = dy + 130;
    int pin_col_width = dw / 2 - 20;

    for (int i = 0; i < ui->subcircuit_num_pins && i < 16; i++) {
        int col = i % 2;
        int row = i / 2;
        int px = dx + 15 + col * pin_col_width;
        int py = pin_start_y + row * 24;

        bool pin_focused = (ui->subcircuit_editing_field == i + 1);

        // Pin number label
        char pin_label[8];
        snprintf(pin_label, sizeof(pin_label), "%d:", i + 1);
        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DIM, 0xff);
        ui_draw_text(renderer, pin_label, px, py + 4);

        // Pin name input
        SDL_SetRenderDrawColor(renderer, SYNTH_BG_DARK, 0xff);
        SDL_Rect pin_bg = {px + 25, py, 110, 20};
        SDL_RenderFillRect(renderer, &pin_bg);
        SDL_SetRenderDrawColor(renderer, pin_focused ? SYNTH_CYAN : SYNTH_BORDER, 0xff);
        SDL_RenderDrawRect(renderer, &pin_bg);

        SDL_SetRenderDrawColor(renderer, SYNTH_TEXT, 0xff);
        ui_draw_text(renderer, ui->subcircuit_pin_names[i], px + 30, py + 4);

        // Cursor for focused pin
        if (pin_focused && (SDL_GetTicks() / 500) % 2 == 0) {
            SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0xff);
            int cursor_x = px + 30 + (int)strlen(ui->subcircuit_pin_names[i]) * 8;
            SDL_RenderDrawLine(renderer, cursor_x, py + 2, cursor_x, py + 18);
        }
    }

    // Buttons at bottom
    int btn_y = dy + dh - 45;

    // Cancel button
    SDL_SetRenderDrawColor(renderer, SYNTH_BG_LIGHT, 0xff);
    SDL_Rect cancel_btn = {dx + 10, btn_y, 80, 30};
    SDL_RenderFillRect(renderer, &cancel_btn);
    SDL_SetRenderDrawColor(renderer, SYNTH_BORDER_LIGHT, 0xff);
    SDL_RenderDrawRect(renderer, &cancel_btn);
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT, 0xff);
    ui_draw_text(renderer, "Cancel", dx + 25, btn_y + 10);

    // Create/Save button - different text for edit mode
    bool can_create = (ui->subcircuit_name[0] != '\0');
    SDL_SetRenderDrawColor(renderer, can_create ? SYNTH_PURPLE_DIM : SYNTH_BG_LIGHT, 0xff);
    SDL_Rect create_btn = {dx + dw - 90, btn_y, 80, 30};
    SDL_RenderFillRect(renderer, &create_btn);
    SDL_SetRenderDrawColor(renderer, can_create ? SYNTH_PURPLE : SYNTH_BORDER, 0xff);
    SDL_RenderDrawRect(renderer, &create_btn);
    SDL_SetRenderDrawColor(renderer, can_create ? SYNTH_TEXT : SYNTH_TEXT_DARK, 0xff);
    ui_draw_text(renderer, is_editing ? "Save" : "Create", is_editing ? (dx + dw - 67) : (dx + dw - 75), btn_y + 10);

    // Hint at bottom - different text for edit mode
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT_DARK, 0xff);
    ui_draw_text(renderer, is_editing ? "Tab: next field  Enter: save  Esc: cancel" : "Tab: next field  Enter: create  Esc: cancel", dx + 60, dy + dh - 15);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// Synthwave gradient spectrum animation along window borders
// Two smooth chasers on opposite sides with thick glowing bars
void ui_render_neon_trim(UIState *ui, SDL_Renderer *renderer) {
    // Get time for animation
    static Uint32 start_time = 0;
    if (start_time == 0) start_time = SDL_GetTicks();
    double time = (SDL_GetTicks() - start_time) / 1000.0;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    int w = ui->window_width;
    int h = ui->window_height;
    int border_thickness = 5;  // Thicker bars

    // Total perimeter for position calculation
    int perimeter = 2 * w + 2 * h;

    // Two chasers on opposite sides, smooth motion
    double chaser_speed = 0.15;  // Slower for smoother flow (full loop every ~6.7 seconds)
    double chaser1_pos = fmod(time * chaser_speed, 1.0);
    double chaser2_pos = fmod(time * chaser_speed + 0.5, 1.0);  // Opposite side (180 degrees)
    double chaser_length = 0.15;  // Length of bright trail (15% of perimeter)

    // Smooth easing function for gradient falloff
    #define SMOOTH_FALLOFF(x) ((x) * (x) * (3.0 - 2.0 * (x)))

    // Global pulse effect (subtle brightness oscillation)
    double pulse = 0.6 + 0.15 * sin(time * 1.5);

    // Helper to get bright cyberpunk neon color at position with two chasers
    #define GET_SYNTH_COLOR(pos, out_r, out_g, out_b) do { \
        double _hue = fmod((pos) + time * 0.1, 1.0); \
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
        /* Calculate distance from both chasers (with wraparound) */ \
        double _dist1 = (pos) - chaser1_pos; \
        if (_dist1 < 0) _dist1 += 1.0; \
        double _dist2 = (pos) - chaser2_pos; \
        if (_dist2 < 0) _dist2 += 1.0; \
        /* Smooth chaser brightness boost with falloff */ \
        double _chaser_boost = 0.0; \
        if (_dist1 < chaser_length) { \
            double _t = 1.0 - _dist1 / chaser_length; \
            _chaser_boost = SMOOTH_FALLOFF(_t) * 0.6; \
        } \
        if (_dist2 < chaser_length) { \
            double _t = 1.0 - _dist2 / chaser_length; \
            _chaser_boost += SMOOTH_FALLOFF(_t) * 0.6; \
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
        SDL_SetRenderDrawColor(renderer, r, g, b, 220);
        for (int t = 0; t < border_thickness; t++) {
            SDL_RenderDrawPoint(renderer, x, t);
        }
    }

    // Draw right border (top to bottom)
    for (int y = 0; y < h; y++) {
        double pos = (double)(w + y) / perimeter;
        uint8_t r, g, b;
        GET_SYNTH_COLOR(pos, r, g, b);
        SDL_SetRenderDrawColor(renderer, r, g, b, 220);
        for (int t = 0; t < border_thickness; t++) {
            SDL_RenderDrawPoint(renderer, w - 1 - t, y);
        }
    }

    // Draw bottom border (right to left)
    for (int x = w - 1; x >= 0; x--) {
        double pos = (double)(w + h + (w - 1 - x)) / perimeter;
        uint8_t r, g, b;
        GET_SYNTH_COLOR(pos, r, g, b);
        SDL_SetRenderDrawColor(renderer, r, g, b, 220);
        for (int t = 0; t < border_thickness; t++) {
            SDL_RenderDrawPoint(renderer, x, h - 1 - t);
        }
    }

    // Draw left border (bottom to top)
    for (int y = h - 1; y >= 0; y--) {
        double pos = (double)(2 * w + h + (h - 1 - y)) / perimeter;
        uint8_t r, g, b;
        GET_SYNTH_COLOR(pos, r, g, b);
        SDL_SetRenderDrawColor(renderer, r, g, b, 220);
        for (int t = 0; t < border_thickness; t++) {
            SDL_RenderDrawPoint(renderer, t, y);
        }
    }

    #undef GET_SYNTH_COLOR
    #undef SMOOTH_FALLOFF

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static bool point_in_rect(int x, int y, Rect *r) {
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

static bool is_palette_item_in_collapsed_category(UIState *ui, int item_idx) {
    if (item_idx < 0 || item_idx >= ui->num_palette_items) return true;
    PaletteCategoryID cat = ui->palette_items[item_idx].category;
    if (cat < 0 || cat >= PCAT_COUNT) return true;
    return ui->categories[cat].collapsed;
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

            // Consume all other clicks within the Bode panel to prevent component placement
            if (x >= panel_x && x <= panel_x + panel_w &&
                y >= panel_y && y <= panel_y + panel_h) {
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

        // Handle trigger position and level dragging in scope area
        if (ui->display_mode == SCOPE_MODE_YT && ui->scope_num_channels > 0) {
            Rect *sr = &ui->scope_rect;

            // Check trigger position FIRST (vertical cyan line) - before trigger level
            // because trigger level detection spans the entire width
            int trig_pos_x = sr->x + (int)(ui->trigger_position * sr->w);
            trig_pos_x = CLAMP(trig_pos_x, sr->x, sr->x + sr->w);

            // Check if click is near the trigger position line (entire height, +/- 5 pixels horizontally)
            // Or on the bottom edge indicator (+/- 8 pixels horizontally)
            bool on_trig_pos_line = (y >= sr->y && y <= sr->y + sr->h &&
                                     x >= trig_pos_x - 5 && x <= trig_pos_x + 5);
            bool on_trig_pos_arrow = (y >= sr->y + sr->h - 15 && y <= sr->y + sr->h &&
                                      x >= trig_pos_x - 8 && x <= trig_pos_x + 8);
            if (on_trig_pos_line || on_trig_pos_arrow) {
                ui->dragging_trigger_position = true;
                return UI_ACTION_NONE;
            }

            // Now check trigger level (horizontal yellow line)
            int trig_ch = ui->trigger_channel;
            if (trig_ch < ui->scope_num_channels && ui->scope_channels[trig_ch].enabled) {
                /* Where the trigger line is actually drawn, which is inside the trigger
                   channel's own band, at that channel's own volts-per-division, with the AC and
                   Fit centring applied. This used the full height, the middle of the screen and
                   the global V/div, so with the channels stacked - the default for most templates
                   - the line you can grab was nowhere near the line you can see. The renderer
                   records the three numbers per channel for exactly this; the cursor overlay was
                   fixed the same way. */
                double scale = (sr->h / 8.0) / ui->scope_volt_div;
                int center_y = sr->y + sr->h / 2;
                double trig_offset = ui->scope_channels[trig_ch].offset;
                if (trig_ch >= 0 && trig_ch < MAX_PROBES && ui->scope_ch_scale[trig_ch] > 0) {
                    center_y = ui->scope_ch_center[trig_ch];
                    scale = ui->scope_ch_scale[trig_ch];
                    trig_offset += ui->scope_ch_shift[trig_ch];
                }
                int trig_y = center_y - (int)((ui->trigger_level + trig_offset) * scale);
                trig_y = CLAMP(trig_y, sr->y, sr->y + sr->h);

                // Check if click is near the trigger level line (entire width, +/- 5 pixels vertically)
                // Or on the right edge indicator (+/- 8 pixels vertically)
                bool on_trigger_line = (x >= sr->x && x <= sr->x + sr->w &&
                                        y >= trig_y - 5 && y <= trig_y + 5);
                bool on_trigger_arrow = (x >= sr->x + sr->w - 15 && x <= sr->x + sr->w &&
                                         y >= trig_y - 8 && y <= trig_y + 8);
                if (on_trigger_line || on_trigger_arrow) {
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
                // Clicked inside scope area - grab the nearest cursor bar
                double normalized_x = (double)(x - sr->x) / sr->w;
                normalized_x = CLAMP(normalized_x, 0.0, 1.0);
                double normalized_y = (double)(y - sr->y) / sr->h;
                normalized_y = CLAMP(normalized_y, 0.0, 1.0);

                double d1 = fabs(normalized_x - ui->cursor1_time) * sr->w;
                double d2 = fabs(normalized_x - ui->cursor2_time) * sr->w;
                int pick = (d1 <= d2) ? 1 : 2;
                double dbest = (d1 <= d2) ? d1 : d2;
                if (ui->scope_cursor_type == 2) {
                    // Horizontal bars live in the source channel's band; compare in pixels
                    int f_top, f_h, f_center; double f_scale;
                    int src = (ui->cursor_a_channel >= 0) ? ui->cursor_a_channel : ui->trigger_channel;
                    if (src < 0 || src >= ui->scope_num_channels) src = 0;
                    int src_b = (ui->cursor_b_channel >= 0) ? ui->cursor_b_channel : src;
                    if (src_b < 0 || src_b >= ui->scope_num_channels) src_b = src;
                    scope_channel_frame(ui, sr, src, &f_top, &f_h, &f_center, &f_scale);
                    int fb_top, fb_h, fb_center; double fb_scale;
                    scope_channel_frame(ui, sr, src_b, &fb_top, &fb_h, &fb_center, &fb_scale);
                    double d3 = fabs((double)y - (f_top + ui->cursor1_volt * f_h));
                    double d4 = fabs((double)y - (fb_top + ui->cursor2_volt * fb_h));
                    if (d3 < dbest) { pick = 3; dbest = d3; }
                    if (d4 < dbest) { pick = 4; dbest = d4; }
                    if (pick == 3) ui->cursor1_volt = CLAMP((double)(y - f_top) / f_h, 0.0, 1.0);
                    if (pick == 4) ui->cursor2_volt = CLAMP((double)(y - fb_top) / fb_h, 0.0, 1.0);
                }
                if (pick == 1) ui->cursor1_time = normalized_x;
                else if (pick == 2) ui->cursor2_time = normalized_x;
                ui->scope_cursor_drag = pick;
                ui->scope_cursor_active = (pick == 1 || pick == 3) ? 1 : 2;
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
        // Release trigger position drag
        if (ui->dragging_trigger_position) {
            ui->dragging_trigger_position = false;
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
        ui->dragging_brightness = false;
        // Release palette scrollbar drag
        if (ui->palette_scrolling) {
            ui->palette_scrolling = false;
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
        if (point_in_rect(x, y, &ui->btn_zoom_out.bounds) && ui->btn_zoom_out.enabled) return UI_ACTION_ZOOM_OUT;
        if (point_in_rect(x, y, &ui->btn_zoom_in.bounds)  && ui->btn_zoom_in.enabled)  return UI_ACTION_ZOOM_IN;
        if (point_in_rect(x, y, &ui->btn_zoom_fit.bounds) && ui->btn_zoom_fit.enabled) return UI_ACTION_ZOOM_FIT;
        if (point_in_rect(x, y, &ui->btn_import_spice.bounds) && ui->btn_import_spice.enabled) return UI_ACTION_IMPORT_SPICE;
        if (point_in_rect(x, y, &ui->btn_screenshot.bounds) && ui->btn_screenshot.enabled) {
            return UI_ACTION_SCREENSHOT;
        }

        // Check time step control buttons
        if (point_in_rect(x, y, &ui->btn_timestep_up.bounds) && ui->btn_timestep_up.enabled) {
            return UI_ACTION_TIMESTEP_UP;
        }
        if (point_in_rect(x, y, &ui->btn_timestep_down.bounds) && ui->btn_timestep_down.enabled) {
            return UI_ACTION_TIMESTEP_DOWN;
        }
        if (ui->btn_update.bounds.w > 0 && point_in_rect(x, y, &ui->btn_update.bounds)) {
            return UI_ACTION_UPDATE;
        }
        if (point_in_rect(x, y, &ui->btn_timestep_auto.bounds) && ui->btn_timestep_auto.enabled) {
            return UI_ACTION_TIMESTEP_AUTO;
        }

        // Check speed slider click
        int slider_x = ui->speed_slider.x + ui->speed_label_w;
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

        if (point_in_rect(x, y, &ui->brightness_slider)) {
            float normalized = (float)(x - ui->brightness_slider.x) / ui->brightness_slider.w;
            ui_set_brightness(ui, 0.25f + CLAMP(normalized, 0.0f, 1.0f) * 0.75f);
            ui->dragging_brightness = true;
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
        for (int t = 0; t < 3; t++) {
            if (point_in_rect(x, y, &ui->btn_scope_tab[t].bounds)) { ui->scope_ctl_tab = t; return UI_ACTION_NONE; }
        }
        if (point_in_rect(x, y, &ui->btn_scope_volt_up.bounds) && ui->btn_scope_volt_up.enabled) {
            return UI_ACTION_SCOPE_VOLT_UP;
        }
        if (point_in_rect(x, y, &ui->btn_scope_volt_down.bounds) && ui->btn_scope_volt_down.enabled) {
            return UI_ACTION_SCOPE_VOLT_DOWN;
        }
        /* which channel the vertical controls move: the ALL chip, or a channel by name */
        if (ui->btn_scope_ch_all.bounds.w > 0 && point_in_rect(x, y, &ui->btn_scope_ch_all.bounds))
            return UI_ACTION_SCOPE_CH_SEL;
        for (int ch = 0; ch < MAX_PROBES && ch < ui->scope_num_channels; ch++)
            if (ui->scope_channels[ch].enabled && ui->btn_scope_ch[ch].bounds.w > 0 &&
                point_in_rect(x, y, &ui->btn_scope_ch[ch].bounds))
                return UI_ACTION_SCOPE_CH_SEL + 1 + ch;
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
        if (point_in_rect(x, y, &ui->btn_scope_stack.bounds) && ui->btn_scope_stack.enabled) {
            return UI_ACTION_SCOPE_STACK;
        }
        if (point_in_rect(x, y, &ui->btn_scope_ac.bounds) && ui->btn_scope_ac.enabled) return UI_ACTION_SCOPE_AC;
        if (point_in_rect(x, y, &ui->btn_scope_fit.bounds) && ui->btn_scope_fit.enabled) return UI_ACTION_SCOPE_FIT;
        if (point_in_rect(x, y, &ui->btn_scope_track.bounds) && ui->btn_scope_track.enabled) {
            return UI_ACTION_SCOPE_TRACK;
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

        // Properties header: click to collapse / expand
        {
            Rect hdr = {ui->window_width - ui->properties_width + 4, TOOLBAR_HEIGHT + 2, ui->properties_width - 16, 24};
            if (point_in_rect(x, y, &hdr)) { ui->properties_collapsed = !ui->properties_collapsed; return UI_ACTION_NONE; }
        }

        // Check property fields for click-to-edit
        // Each property field stores its prop_type directly
        for (int i = 0; i < ui->num_properties && i < UI_MAX_PROPERTY_ROWS; i++) {
            if (point_in_rect(x, y, &ui->properties[i].bounds)) {
                return UI_ACTION_PROP_EDIT + ui->properties[i].prop_type;
            }
        }

        // Tab strip and filter box at the top of the left panel
        if (x >= 0 && x < PALETTE_WIDTH && y >= TOOLBAR_HEIGHT && y < TOOLBAR_HEIGHT + PALETTE_TOP_H) {
            if (y < TOOLBAR_HEIGHT + 30) {    /* the tab buttons are 25 tall from +3 */
                int tab = (x < PALETTE_WIDTH / 2) ? LTAB_PARTS : LTAB_CIRCUITS;
                if (tab != ui->left_tab) {
                    ui->palette_scroll_per_tab[ui->left_tab] = ui->palette_scroll_offset;
                    ui->left_tab = tab;
                    ui->palette_scroll_offset = ui->palette_scroll_per_tab[tab];
                }
                ui->palette_filter_active = false;
            } else {
                ui->palette_filter_active = true;
            }
            return UI_ACTION_NONE;
        }
        ui->palette_filter_active = false;

        // Check palette scrollbar click (scrollbar is at x = PALETTE_WIDTH - 8, width 6)
        if (ui->palette_content_height > ui->palette_visible_height) {
            int scrollbar_x = PALETTE_WIDTH - 8;
            int scrollbar_track_y = TOOLBAR_HEIGHT + PALETTE_TOP_H + 2;
            int scrollbar_track_h = ui->palette_visible_height - 4;

            // Check if clicking on scrollbar track area
            if (x >= scrollbar_x - 2 && x < PALETTE_WIDTH &&
                y >= scrollbar_track_y && y < scrollbar_track_y + scrollbar_track_h) {

                // Calculate thumb position and size (same as in render)
                float visible_ratio = (float)ui->palette_visible_height / ui->palette_content_height;
                int thumb_h = (int)(scrollbar_track_h * visible_ratio);
                if (thumb_h < 20) thumb_h = 20;

                int max_scroll = ui->palette_content_height - ui->palette_visible_height;
                float scroll_ratio = (max_scroll > 0) ? (float)ui->palette_scroll_offset / max_scroll : 0;
                int thumb_y = scrollbar_track_y + (int)((scrollbar_track_h - thumb_h) * scroll_ratio);

                // Check if clicking on thumb (drag) or track (jump)
                if (y >= thumb_y && y < thumb_y + thumb_h) {
                    // Clicking on thumb - start drag
                    ui->palette_scrolling = true;
                    ui->palette_scroll_drag_start_y = y;
                    ui->palette_scroll_drag_start_offset = ui->palette_scroll_offset;
                } else {
                    // Clicking on track - jump to position
                    // Calculate scroll position based on click position in track
                    int available_track = scrollbar_track_h - thumb_h;
                    if (available_track > 0 && max_scroll > 0) {
                        // Center the thumb on the click position
                        int target_thumb_y = y - thumb_h / 2;
                        if (target_thumb_y < scrollbar_track_y) target_thumb_y = scrollbar_track_y;
                        if (target_thumb_y > scrollbar_track_y + available_track)
                            target_thumb_y = scrollbar_track_y + available_track;

                        float new_scroll_ratio = (float)(target_thumb_y - scrollbar_track_y) / available_track;
                        ui->palette_scroll_offset = (int)(new_scroll_ratio * max_scroll);
                        if (ui->palette_scroll_offset < 0) ui->palette_scroll_offset = 0;
                        if (ui->palette_scroll_offset > max_scroll) ui->palette_scroll_offset = max_scroll;
                    }
                }
                return UI_ACTION_NONE;
            }
        }

        // Check palette category headers for collapse/expand (must be in palette area)
        if (x >= 0 && x < PALETTE_WIDTH - 10 && y >= TOOLBAR_HEIGHT + PALETTE_TOP_H && y < ui->window_height - STATUSBAR_HEIGHT) {
            int content_y = y + ui->palette_scroll_offset;  // Convert screen y to content y
            // Check the active tab's categories for header clicks
            for (int cat = 0; cat < PCAT_COUNT; cat++) {
                bool on_tab = (ui->left_tab == LTAB_PARTS) ? (cat < PCAT_CIRCUITS) : (cat >= PCAT_CIRCUITS);
                if (!on_tab) continue;
                PaletteCategory *category = &ui->categories[cat];
                // Header_y is in content coords, header is 14 pixels tall
                if (content_y >= category->header_y && content_y < category->header_y + PAL_HEADER_H) {
                    // Toggle collapsed state
                    category->collapsed = !category->collapsed;
                    return UI_ACTION_NONE;  // Handled, no further action needed
                }
            }
        }

        // Check palette items (adjust y for scroll offset)
        int adjusted_y = y + ui->palette_scroll_offset;
        for (int i = 0; i < ui->num_palette_items && ui->left_tab == LTAB_PARTS; i++) {
            // Skip items in collapsed categories (their bounds are stale) and filtered-out items
            if (is_palette_item_in_collapsed_category(ui, i) || ui->palette_items[i].bounds.w <= 0) {
                continue;
            }
            if (point_in_rect(x, adjusted_y, &ui->palette_items[i].bounds)) {
                // Verify item is visible (not clipped by scroll)
                int item_screen_y = ui->palette_items[i].bounds.y - ui->palette_scroll_offset;
                if (item_screen_y < TOOLBAR_HEIGHT + PALETTE_TOP_H - 8 || item_screen_y + ui->palette_items[i].bounds.h > ui->window_height - STATUSBAR_HEIGHT) {
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
        // Skip if Circuits category is collapsed (bounds are stale)
        if (ui->left_tab == LTAB_CIRCUITS && !ui->categories[PCAT_CIRCUITS].collapsed) {
        for (int g = 0; g < TG_COUNT; g++) {
            if (ui->circuit_group_header_y[g] <= 0) continue;
            /* the bar as draw_palette_header lays it out at indent 6, so the whole box presses */
            Rect hr = {8, ui->circuit_group_header_y[g], PALETTE_WIDTH - 20, PAL_HEADER_H - 3};
            int hy = hr.y - ui->palette_scroll_offset;
            if (hy < TOOLBAR_HEIGHT - 12 || hy > ui->window_height - STATUSBAR_HEIGHT) continue;
            if (point_in_rect(x, adjusted_y, &hr)) {
                ui->circuit_group_collapsed[g] = !ui->circuit_group_collapsed[g];
                return UI_ACTION_NONE;
            }
        }
        for (int i = 0; i < ui->num_circuit_items; i++) {
            if (ui->circuit_items[i].bounds.w <= 0) continue;   // collapsed group
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
        }  // end if (!collapsed)

        // Check user subcircuit items (adjust y for scroll offset)
        for (int i = 0; i < ui->num_subcircuit_items && ui->left_tab == LTAB_CIRCUITS; i++) {
            if (point_in_rect(x, adjusted_y, &ui->subcircuit_items[i].bounds)) {
                // Verify item is visible (not clipped by scroll)
                int item_screen_y = ui->subcircuit_items[i].bounds.y - ui->palette_scroll_offset;
                if (item_screen_y < TOOLBAR_HEIGHT || item_screen_y + ui->subcircuit_items[i].bounds.h > ui->window_height - STATUSBAR_HEIGHT) {
                    continue;  // Item is scrolled out of view
                }
                // Deselect all palette, circuit, and subcircuit items
                for (int j = 0; j < ui->num_palette_items; j++) {
                    ui->palette_items[j].selected = false;
                }
                for (int j = 0; j < ui->num_circuit_items; j++) {
                    ui->circuit_items[j].selected = false;
                }
                for (int j = 0; j < ui->num_subcircuit_items; j++) {
                    ui->subcircuit_items[j].selected = false;
                }
                ui->subcircuit_items[i].selected = true;
                ui->placing_subcircuit = true;
                ui->placing_circuit = false;
                ui->selected_subcircuit_def_id = ui->subcircuit_items[i].def_id;

                return UI_ACTION_SELECT_SUBCIRCUIT + ui->subcircuit_items[i].def_id;
            }
        }
    } else {
        ui->btn_run.pressed = false;
        ui->btn_bode_recalc.pressed = false;
    }

    return UI_ACTION_NONE;
}

// Handle right-click on palette items (for editing subcircuits)
int ui_handle_right_click(UIState *ui, int x, int y) {
    if (!ui) return UI_ACTION_NONE;

    // Only check palette area
    if (!ui_point_in_palette(ui, x, y)) return UI_ACTION_NONE;

    // Adjust y for scroll offset
    int adjusted_y = y + ui->palette_scroll_offset;

    // Check user subcircuit items for right-click (edit)
    for (int i = 0; i < ui->num_subcircuit_items; i++) {
        if (point_in_rect(x, adjusted_y, &ui->subcircuit_items[i].bounds)) {
            // Verify item is visible (not clipped by scroll)
            int item_screen_y = ui->subcircuit_items[i].bounds.y - ui->palette_scroll_offset;
            if (item_screen_y < TOOLBAR_HEIGHT || item_screen_y + ui->subcircuit_items[i].bounds.h > ui->window_height - STATUSBAR_HEIGHT) {
                continue;  // Item is scrolled out of view
            }
            // Open edit dialog for this subcircuit
            ui_subcircuit_dialog_open_edit(ui, ui->subcircuit_items[i].def_id);
            return UI_ACTION_EDIT_SUBCIRCUIT;
        }
    }

    return UI_ACTION_NONE;
}

int ui_handle_motion(UIState *ui, int x, int y, bool popup_mode) {
    if (!ui) return UI_ACTION_NONE;

    // Handle scope resizing
    if (ui->scope_resizing) {
        if (ui->scope_resize_edge == 0) {
            // Resizing top edge - changes height and y position
            int new_y = y;
            int bottom = ui->scope_rect.y + ui->scope_rect.h;
            int new_height = bottom - new_y;

            // Height: anything from 100 px up to the space between the toolbar and the
            // control rows at the bottom (the scope may cover the properties list)
            int max_height = ui->window_height - STATUSBAR_HEIGHT - TOOLBAR_HEIGHT - 140;
            if (new_height >= 100 && new_height <= max_height && new_y >= TOOLBAR_HEIGHT + 30) {
                ui->scope_rect.y = new_y;
                ui->scope_rect.h = new_height;
                ui->scope_user_sized = true;
            }
        } else if (ui->scope_resize_edge == 1) {
            // Resizing left edge - changes width and x position
            int new_x = x;
            int right = ui->scope_rect.x + ui->scope_rect.w;
            int new_width = right - new_x;

            // Width: the scope may grow leftwards over the canvas (up to the palette)
            if (new_width >= 150 && new_x >= PALETTE_WIDTH + 20) {
                ui->scope_rect.x = new_x;
                ui->scope_rect.w = new_width;
                int panel_x = ui->window_width - ui->properties_width + 10;
                ui->scope_extra_w = (panel_x - new_x > 0) ? (panel_x - new_x) : 0;
                ui->scope_user_sized = true;
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

    // Handle palette scrollbar dragging
    if (ui->palette_scrolling) {
        // Calculate scroll position based on mouse Y delta
        int scrollbar_track_h = ui->palette_visible_height - 4;
        int max_scroll = ui->palette_content_height - ui->palette_visible_height;
        if (max_scroll > 0 && scrollbar_track_h > 0) {
            // Calculate thumb size to determine how much track space is available
            float visible_ratio = (float)ui->palette_visible_height / ui->palette_content_height;
            int thumb_h = (int)(scrollbar_track_h * visible_ratio);
            if (thumb_h < 20) thumb_h = 20;
            int available_track = scrollbar_track_h - thumb_h;

            if (available_track > 0) {
                // Convert mouse delta to scroll delta
                int mouse_delta = y - ui->palette_scroll_drag_start_y;
                float scroll_ratio = (float)mouse_delta / available_track;
                int new_offset = ui->palette_scroll_drag_start_offset + (int)(scroll_ratio * max_scroll);

                // Clamp to valid range
                if (new_offset < 0) new_offset = 0;
                if (new_offset > max_scroll) new_offset = max_scroll;
                ui->palette_scroll_offset = new_offset;
            }
        }
        return UI_ACTION_NONE;
    }

    // Handle cursor dragging
    if (ui->scope_cursor_drag != 0) {
        Rect *sr = &ui->scope_rect;
        double normalized_x = (double)(x - sr->x) / sr->w;
        normalized_x = CLAMP(normalized_x, 0.0, 1.0);

        if (ui->scope_cursor_drag == 1) {
            if (ui->scope_cursor_linked) {
                double delta = ui->cursor2_time - ui->cursor1_time;
                ui->cursor2_time = CLAMP(normalized_x + delta, 0.0, 1.0);
            }
            ui->cursor1_time = normalized_x;
        } else if (ui->scope_cursor_drag == 2) {
            ui->cursor2_time = normalized_x;
        } else {
            int f_top, f_h, f_center; double f_scale;
            int src = (ui->cursor_a_channel >= 0) ? ui->cursor_a_channel : ui->trigger_channel;
            if (src < 0 || src >= ui->scope_num_channels) src = 0;
            if (ui->scope_cursor_drag == 4 && ui->cursor_b_channel >= 0 && ui->cursor_b_channel < ui->scope_num_channels) src = ui->cursor_b_channel;
            scope_channel_frame(ui, sr, src, &f_top, &f_h, &f_center, &f_scale);
            double fy = CLAMP((double)(y - f_top) / f_h, 0.0, 1.0);
            if (ui->scope_cursor_drag == 3) ui->cursor1_volt = fy; else ui->cursor2_volt = fy;
        }
        return UI_ACTION_NONE;
    }

    // Handle trigger level dragging
    if (ui->dragging_trigger_level && ui->scope_num_channels > 0) {
        Rect *sr = &ui->scope_rect;
        int trig_ch = ui->trigger_channel;
        if (trig_ch < ui->scope_num_channels && ui->scope_channels[trig_ch].enabled) {
            // Calculate scale and center_y (the same three the renderer used - see the hit-test)
            double scale = (sr->h / 8.0) / ui->scope_volt_div;
            int center_y = sr->y + sr->h / 2;
            double trig_offset = ui->scope_channels[trig_ch].offset;
            double vdiv = ui->scope_volt_div;
            if (trig_ch >= 0 && trig_ch < MAX_PROBES && ui->scope_ch_scale[trig_ch] > 0) {
                center_y = ui->scope_ch_center[trig_ch];
                scale = ui->scope_ch_scale[trig_ch];
                trig_offset += ui->scope_ch_shift[trig_ch];
                vdiv = ui_channel_volt_div(ui, trig_ch);
            }

            // Convert mouse Y position to trigger level voltage
            // From rendering: trig_y = center_y - (int)((trigger_level + trig_offset) * scale)
            // So: trigger_level = (center_y - y) / scale - trig_offset
            double new_level = (double)(center_y - y) / scale - trig_offset;

            // Clamp to reasonable voltage range based on volt_div (4 divisions = 4 * volt_div)
            double max_level = 4.0 * vdiv;
            ui->trigger_level = CLAMP(new_level, -max_level, max_level);
        }
        return UI_ACTION_NONE;
    }

    // Handle trigger position dragging
    if (ui->dragging_trigger_position) {
        Rect *sr = &ui->scope_rect;
        // Convert mouse X position to trigger position (0.0 to 1.0)
        double new_pos = (double)(x - sr->x) / sr->w;
        ui->trigger_position = CLAMP(new_pos, 0.0, 1.0);
        return UI_ACTION_NONE;
    }

    // Handle speed slider dragging
    if (ui->dragging_speed) {
        int slider_x = ui->speed_slider.x + ui->speed_label_w;
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

    if (ui->dragging_brightness) {
        float normalized = (float)(x - ui->brightness_slider.x) / ui->brightness_slider.w;
        ui_set_brightness(ui, 0.25f + CLAMP(normalized, 0.0f, 1.0f) * 0.75f);
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
            ui->scope_rect.x = ui->window_width - ui->properties_width + 10 - ui->scope_extra_w;
            ui->scope_rect.w = ui->properties_width - 20 + ui->scope_extra_w;
        }
        return UI_ACTION_NONE;
    }

    // Update button hover states (skip if in popup mode)
    if (!popup_mode) {
        ui->btn_run.hovered = point_in_rect(x, y, &ui->btn_run.bounds);
        ui->btn_pause.hovered = point_in_rect(x, y, &ui->btn_pause.bounds);
        ui->btn_step.hovered = point_in_rect(x, y, &ui->btn_step.bounds);
        ui->btn_reset.hovered = point_in_rect(x, y, &ui->btn_reset.bounds);
        ui->btn_clear.hovered = point_in_rect(x, y, &ui->btn_clear.bounds);
        ui->btn_save.hovered = point_in_rect(x, y, &ui->btn_save.bounds);
        ui->btn_load.hovered = point_in_rect(x, y, &ui->btn_load.bounds);
        ui->btn_export_svg.hovered = point_in_rect(x, y, &ui->btn_export_svg.bounds);
        ui->btn_screenshot.hovered = point_in_rect(x, y, &ui->btn_screenshot.bounds);
        ui->btn_zoom_out.hovered = point_in_rect(x, y, &ui->btn_zoom_out.bounds);
        ui->btn_zoom_in.hovered  = point_in_rect(x, y, &ui->btn_zoom_in.bounds);
        ui->btn_zoom_fit.hovered = point_in_rect(x, y, &ui->btn_zoom_fit.bounds);
        ui->btn_import_spice.hovered = point_in_rect(x, y, &ui->btn_import_spice.bounds);
        ui->btn_timestep_up.hovered = point_in_rect(x, y, &ui->btn_timestep_up.bounds);
        ui->btn_timestep_down.hovered = point_in_rect(x, y, &ui->btn_timestep_down.bounds);
        ui->btn_timestep_auto.hovered = point_in_rect(x, y, &ui->btn_timestep_auto.bounds);
        ui->btn_update.hovered = ui->btn_update.bounds.w > 0 && point_in_rect(x, y, &ui->btn_update.bounds);
    }

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
    ui->btn_scope_stack.hovered = point_in_rect(x, y, &ui->btn_scope_stack.bounds);
    ui->btn_scope_ac.hovered = point_in_rect(x, y, &ui->btn_scope_ac.bounds);
    ui->btn_scope_fit.hovered = point_in_rect(x, y, &ui->btn_scope_fit.bounds);
    ui->btn_scope_track.hovered = point_in_rect(x, y, &ui->btn_scope_track.bounds);
    ui->btn_scope_autoset.hovered = point_in_rect(x, y, &ui->btn_scope_autoset.bounds);
    ui->btn_bode.hovered = point_in_rect(x, y, &ui->btn_bode.bounds);
    ui->btn_mc.hovered = point_in_rect(x, y, &ui->btn_mc.bounds);
    ui->btn_scope_popup.hovered = point_in_rect(x, y, &ui->btn_scope_popup.bounds);
    ui->btn_bode_recalc.hovered = ui->show_bode_plot && point_in_rect(x, y, &ui->btn_bode_recalc.bounds);

    // Hover tooltip: whichever button or palette item is under the mouse
    {
        const char *tip = NULL;
        char tipbuf[160];
        Button *tb[] = { &ui->btn_run, &ui->btn_pause, &ui->btn_step, &ui->btn_reset, &ui->btn_clear, &ui->btn_save, &ui->btn_load,
                         &ui->btn_export_svg, &ui->btn_screenshot, &ui->btn_zoom_out, &ui->btn_zoom_in, &ui->btn_zoom_fit, &ui->btn_import_spice, &ui->btn_timestep_up, &ui->btn_timestep_down, &ui->btn_timestep_auto, &ui->btn_update };
        for (unsigned i = 0; i < sizeof tb / sizeof tb[0] && !tip; i++)
            if (!popup_mode && tb[i]->bounds.w > 0 && point_in_rect(x, y, &tb[i]->bounds)) tip = tb[i]->tooltip;
        Button *sb[SCOPE_BTN_N]; scope_button_list(ui, sb);
        for (int i = 0; i < SCOPE_BTN_N && !tip; i++)
            if (sb[i]->bounds.w > 0 && point_in_rect(x, y, &sb[i]->bounds)) tip = sb[i]->tooltip;
        if (!tip && !popup_mode && x < PALETTE_WIDTH && y >= TOOLBAR_HEIGHT + PALETTE_TOP_H) {
            int ay = y + ui->palette_scroll_offset;
            for (int i = 0; i < ui->num_palette_items && !tip && ui->left_tab == LTAB_PARTS; i++) {
                PaletteItem *it = &ui->palette_items[i];
                if (it->bounds.w > 0 && !is_palette_item_in_collapsed_category(ui, i) && point_in_rect(x, ay, &it->bounds)) {
                    if (it->is_tool) tip = it->label;
                    else { const ComponentTypeInfo *ci = component_get_info(it->comp_type); tip = ci ? ci->name : it->label; }
                }
            }
            for (int i = 0; i < ui->num_circuit_items && !tip && ui->left_tab == LTAB_CIRCUITS; i++) {
                CircuitPaletteItem *it = &ui->circuit_items[i];
                if (it->bounds.w > 0 && point_in_rect(x, ay, &it->bounds)) {
                    const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)it->circuit_type);
                    if (ti) { snprintf(tipbuf, sizeof tipbuf, "%s - %s", ti->name, ti->description); tip = tipbuf; }
                }
            }
        }
        if (!tip) {
            ui->hover_text[0] = 0;
        } else if (strcmp(tip, ui->hover_text) != 0) {
            strncpy(ui->hover_text, tip, sizeof ui->hover_text - 1);
            ui->hover_text[sizeof ui->hover_text - 1] = 0;
            ui->hover_since = SDL_GetTicks();
        }
        ui->hover_x = x; ui->hover_y = y;
    }

    // Update palette hover states (skip if in popup mode)
    if (!popup_mode) {
        int adjusted_y = y + ui->palette_scroll_offset;
        for (int i = 0; i < ui->num_palette_items; i++) {
            // Skip items in collapsed categories (their bounds are stale)
            if (is_palette_item_in_collapsed_category(ui, i)) {
                ui->palette_items[i].hovered = false;
                continue;
            }
            bool in_bounds = point_in_rect(x, adjusted_y, &ui->palette_items[i].bounds);
            // Also check if item is visible on screen
            if (in_bounds) {
                int item_screen_y = ui->palette_items[i].bounds.y - ui->palette_scroll_offset;
                if (item_screen_y < TOOLBAR_HEIGHT + PALETTE_TOP_H - 8 || item_screen_y + ui->palette_items[i].bounds.h > ui->window_height - STATUSBAR_HEIGHT) {
                    in_bounds = false;  // Item is scrolled out of view
                }
            }
            ui->palette_items[i].hovered = in_bounds;
        }

        // Update circuit items hover states (adjust y for scroll offset)
        // Skip if Circuits category is collapsed (bounds are stale)
        if (!ui->categories[PCAT_CIRCUITS].collapsed) {
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
        } else {
            // Clear hover states when collapsed
            for (int i = 0; i < ui->num_circuit_items; i++) {
                ui->circuit_items[i].hovered = false;
            }
        }
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

    int names_changed = 0;
    for (int i = 0; i < circuit->num_probes && i < MAX_PROBES; i++) {
        ui->scope_channels[i].enabled = true;
        ui->scope_channels[i].probe_idx = i;
        ui->scope_channels[i].color = PROBE_COLORS[i];

        // Update probe with channel info. The label is only renumbered while it is still a
        // default one - a probe the user has named "Vout" keeps that name when probes are
        // added, removed or reordered.
        circuit->probes[i].channel_num = i;
        circuit->probes[i].color = PROBE_COLORS[i];
        if (probe_label_is_default(circuit->probes[i].label))
            snprintf(circuit->probes[i].label, sizeof(circuit->probes[i].label), "CH%d", i + 1);
        if (strcmp(ui->scope_channels[i].name, circuit->probes[i].label) != 0) names_changed = 1;
        snprintf(ui->scope_channels[i].name, sizeof ui->scope_channels[i].name, "%s", circuit->probes[i].label);
    }

    // Disable unused channels
    for (int i = circuit->num_probes; i < MAX_PROBES; i++) {
        ui->scope_channels[i].enabled = false;
    }

    /* The channel chips are sized from these names, but they are laid out in ui_update_layout,
       which runs when the window is sized - before any template has attached its probes. So a
       chip sized for "CH2" was later drawn holding "SW OUT", and the LDO vs Switcher's chip row
       read "LDOSW OUT". When a name actually changes, re-run the same layout call the resize
       path makes; the guard on scope_rect.w skips the call that arrives before first layout. */
    if (names_changed && ui->scope_rect.w > 0)
        ui_layout_scope_buttons(ui, ui->scope_rect.x,
                                ui->scope_rect.y + ui->scope_rect.h + 5 - ui->scope_controls_scroll,
                                ui->scope_rect.x + ui->scope_rect.w);
}

static void scope_button_list(UIState *ui, Button *out[SCOPE_BTN_N]) {
    Button *l[SCOPE_BTN_N] = {
        &ui->btn_scope_volt_up, &ui->btn_scope_volt_down, &ui->btn_scope_time_up, &ui->btn_scope_time_down,
        &ui->btn_scope_autoset, &ui->btn_scope_cursor, &ui->btn_scope_stack, &ui->btn_scope_track, &ui->btn_scope_popup,
        &ui->btn_scope_tab[0], &ui->btn_scope_tab[1], &ui->btn_scope_tab[2],
        &ui->btn_scope_mode, &ui->btn_scope_screenshot,
        &ui->btn_scope_trig_mode, &ui->btn_scope_trig_edge, &ui->btn_scope_trig_ch, &ui->btn_scope_trig_up, &ui->btn_scope_trig_down,
        &ui->btn_scope_fft, &ui->btn_bode, &ui->btn_mc, &ui->btn_scope_ac, &ui->btn_scope_fit,
    };
    memcpy(out, l, sizeof l);
}

void ui_scope_buttons(UIState *ui, Button *out[]) { scope_button_list(ui, out); }

void ui_render_tooltip(UIState *ui, SDL_Renderer *renderer) {
    if (!ui->hover_text[0] || SDL_GetTicks() - ui->hover_since < 450) return;
    int len = (int)strlen(ui->hover_text);
    int w = len * 6 + 12, h = 18;
    int tx = ui->hover_x + 14, ty = ui->hover_y + 18;
    if (tx + w > ui->window_width - 4) tx = ui->window_width - 4 - w;
    if (ty + h > ui->window_height - 4) ty = ui->hover_y - h - 6;
    if (tx < 2) tx = 2;
    SDL_Rect box = {tx, ty, w, h};
    SDL_SetRenderDrawColor(renderer, 0x10, 0x0a, 0x22, 0xf0);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, SYNTH_CYAN, 0xff);
    SDL_RenderDrawRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, SYNTH_TEXT, 0xff);
    ui_draw_text(renderer, ui->hover_text, tx + 6, ty + 5);
}

// Voltage readout with kV/mV scaling (scope channel and measurement rows)
static void ui_volt_readout(char *out, size_t n, double v) {
    if (fabs(v) >= 1000.0) snprintf(out, n, "%.4gkV", v / 1e3);
    else if (fabs(v) < 0.1 && v != 0.0) snprintf(out, n, "%.1fmV", v * 1e3);
    else snprintf(out, n, "%.2fV", v);
}

void ui_layout_scope_buttons(UIState *ui, int x0, int y0, int max_x) {
    const int h = 22, gap = 3, row = 26;
    int x = x0, y = y0;
    #define PUT(btn, w) do { \
        if (x + (w) > max_x && x > x0) { y += row; x = x0; } \
        (btn)->bounds = (Rect){x, y, (w), h}; x += (w) + gap; } while (0)
    #define HIDE(btn) ((btn)->bounds = (Rect){0, 0, 0, 0})
    // primary row: the controls used every minute
    PUT(&ui->btn_scope_volt_up, 30); PUT(&ui->btn_scope_volt_down, 30); x += 4;
    PUT(&ui->btn_scope_time_up, 30); PUT(&ui->btn_scope_time_down, 30); x += 4;
    PUT(&ui->btn_scope_autoset, 52); x += 4;
    PUT(&ui->btn_scope_cursor, 34); PUT(&ui->btn_scope_stack, 40); PUT(&ui->btn_scope_track, 30); PUT(&ui->btn_scope_popup, 46);
    /* Which channel the vertical controls move. Named after the probe, so the row reads
       ALL IN OUT rather than ALL CH1 CH2. */
    y += row; x = x0;
    /* Laid out for every channel the scope can have, because this runs when the window is sized
       and not per frame - at that point the circuit's channel count is not known yet. Drawing
       and hit-testing skip the chips of channels that do not exist. */
    PUT(&ui->btn_scope_ch_all, 34);
    for (int ch = 0; ch < MAX_PROBES; ch++) {
        const char *nm = ui_channel_name(ui, ch);
        int w = (int)strlen(nm) * 8 + 10;
        if (w < 30) w = 30;
        if (w > 74) w = 74;
        PUT(&ui->btn_scope_ch[ch], w);
    }

    // tab strip
    y += row; x = x0;
    for (int t = 0; t < 3; t++) PUT(&ui->btn_scope_tab[t], 62);
    // active tab row; everything else is hidden (zero bounds never hit-test or draw)
    y += row; x = x0;
    HIDE(&ui->btn_scope_mode); HIDE(&ui->btn_scope_screenshot); HIDE(&ui->btn_scope_ac); HIDE(&ui->btn_scope_fit);
    HIDE(&ui->btn_scope_trig_mode); HIDE(&ui->btn_scope_trig_edge); HIDE(&ui->btn_scope_trig_ch); HIDE(&ui->btn_scope_trig_up); HIDE(&ui->btn_scope_trig_down);
    HIDE(&ui->btn_scope_fft); HIDE(&ui->btn_bode); HIDE(&ui->btn_mc);
    switch (ui->scope_ctl_tab) {
        case 0: PUT(&ui->btn_scope_mode, 40); PUT(&ui->btn_scope_ac, 30); PUT(&ui->btn_scope_fit, 34); PUT(&ui->btn_scope_screenshot, 40); break;
        case 1: PUT(&ui->btn_scope_trig_mode, 45); PUT(&ui->btn_scope_trig_edge, 28); PUT(&ui->btn_scope_trig_ch, 35); PUT(&ui->btn_scope_trig_up, 24); PUT(&ui->btn_scope_trig_down, 24); break;
        default: PUT(&ui->btn_scope_fft, 35); PUT(&ui->btn_bode, 40); PUT(&ui->btn_mc, 25); break;
    }
    #undef PUT
    #undef HIDE
    ui->scope_buttons_bottom = y + h;
}

/* The subcircuit palette list is state, not drawing: it has to exist as soon as a definition
   does, so that placing an imported model does not depend on a frame having been rendered
   first. ui_render_palette() calls this too, and it is idempotent. */
void ui_sync_subcircuit_items(UIState *ui) {
    if (!ui) return;
    ui->num_subcircuit_items = 0;
    for (int i = 0; i < g_subcircuit_library.count && ui->num_subcircuit_items < MAX_SUBCIRCUIT_DEFS; i++) {
        SubCircuitDef *def = &g_subcircuit_library.defs[i];
        if (def->id < 0) continue;
        SubcircuitPaletteItem *item = &ui->subcircuit_items[ui->num_subcircuit_items];
        item->def_id = def->id;
        strncpy(item->label, def->name, sizeof(item->label) - 1);
        item->label[sizeof(item->label) - 1] = '\0';
        item->num_pins = def->num_pins;
        item->hovered = false;
        item->selected = (ui->selected_subcircuit_def_id == def->id);
        item->bounds.w = 60;
        item->bounds.h = 35;
        ui->num_subcircuit_items++;
    }
}

/* The speed slider, the dt readout and the three time-step buttons, laid out from the RIGHT edge
   of the window.

   They used to be at absolute offsets from the left, computed once at start-up from a 1280-wide
   window that no longer had room for them: the dt value was cut off mid-character at "dt:10.0",
   and [-] [+] [Auto] sat at x = 1292 to 1377 - past the edge of a 1280 px window, so at the
   DEFAULT size three controls were invisible and unclickable, and resizing the window never moved
   them because nothing re-laid the toolbar. Anchoring to the right edge fixes both: they are
   always on screen, and they follow a resize.

   There is not room for everything at 1280, so the group gives ground in a fixed order - the
   slider narrows to 56 px first, and only if that is still not enough does the "Speed:" label go
   (the slider keeps its tooltip). Nothing is ever dropped or clipped. */
void ui_layout_toolbar_right(UIState *ui) {
    if (!ui) return;
    const int SLIDER_MAX = 100, SLIDER_MIN = 56, LABEL_W = 52;
    /* everything in the group except the label and the slider: the gap and the speed number, the
       gap before "dt:", the label and its value, then the three buttons with their gaps */
    const int FIXED = 5 + 34 + 8 + 24 + 52 + 5 + (20 + 2 + 20 + 3 + 40);

    int right = ui->window_width - 10;
    int left_limit = ui->btn_import_spice.bounds.x + ui->btn_import_spice.bounds.w + 12;
    int avail = right - left_limit;

    int label_w = LABEL_W, slider_w = SLIDER_MAX;
    if (label_w + slider_w + FIXED > avail) slider_w = avail - label_w - FIXED;
    if (slider_w < SLIDER_MIN) { slider_w = SLIDER_MIN; label_w = 0; }
    if (slider_w > SLIDER_MAX) slider_w = SLIDER_MAX;
    if (label_w + slider_w + FIXED > avail) {   /* narrower than anything sensible: stop shrinking
                                                   and let it sit left of where it would clip */
        label_w = 0;
        slider_w = SLIDER_MIN;
    }

    int gx = right - (label_w + slider_w + FIXED);
    /* Clamped to the window, NOT to the end of the button strip. Below about 1200 px there is no
       room for both, and the strip is at fixed offsets from the left; pushing the group right to
       clear it is what put these controls off the edge in the first place. Overlapping a button
       is ugly, and a control past the window edge cannot be clicked at all - so the group stays on
       screen and sits over the strip if it must. At 1280 and up the two do not meet. */
    (void)left_limit;
    if (gx < 4) gx = 4;

    ui->speed_label_w = label_w;
    ui->speed_slider.x = gx;
    ui->speed_slider.w = slider_w;

    int ts_x = gx + label_w + slider_w + 5 + 34 + 8;
    ui->timestep_display_x = ts_x;
    int bx = ts_x + 24 + 52 + 5;
    ui->btn_timestep_down.bounds = (Rect){bx, 12, 20, 20};
    ui->btn_timestep_up.bounds   = (Rect){bx + 22, 12, 20, 20};
    ui->btn_timestep_auto.bounds = (Rect){bx + 44, 10, 40, 24};
}

void ui_update_layout(UIState *ui) {
    ui_sync_subcircuit_items(ui);
    if (!ui) return;

    ui_layout_toolbar_right(ui);

    // Update palette visible height
    ui->palette_visible_height = ui->window_height - TOOLBAR_HEIGHT - PALETTE_TOP_H - STATUSBAR_HEIGHT;

    // Clamp scroll offset to valid range
    int max_scroll = ui->palette_content_height - ui->palette_visible_height;
    if (max_scroll < 0) max_scroll = 0;
    if (ui->palette_scroll_offset > max_scroll) {
        ui->palette_scroll_offset = max_scroll;
    }
    if (ui->palette_scroll_offset < 0) {
        ui->palette_scroll_offset = 0;
    }

    // Update oscilloscope position (anchored to the right panel; a user-resized scope may
    // extend left over the canvas by scope_extra_w and may cover the properties list)
    if (ui->scope_extra_w > ui->window_width - ui->properties_width - PALETTE_WIDTH - 30)
        ui->scope_extra_w = ui->window_width - ui->properties_width - PALETTE_WIDTH - 30;
    if (ui->scope_extra_w < 0) ui->scope_extra_w = 0;
    ui->scope_rect.x = ui->window_width - ui->properties_width + 10 - ui->scope_extra_w;
    ui->scope_rect.w = ui->properties_width - 20 + ui->scope_extra_w;

    // Ensure scope is positioned below properties content (unless the user sized it)
    int min_scope_y = (ui->scope_user_sized || ui->properties_collapsed) ? TOOLBAR_HEIGHT + 30 : TOOLBAR_HEIGHT + ui->properties_content_height + 25;
    if (ui->properties_collapsed && !ui->scope_user_sized) min_scope_y = TOOLBAR_HEIGHT + 60;
    if (ui->scope_rect.y < min_scope_y) {
        ui->scope_rect.y = min_scope_y;
    }

    // Keep y position reasonable or adjust if window is too small
    int max_scope_y = ui->window_height - STATUSBAR_HEIGHT - ui->scope_rect.h - 100;
    if (ui->scope_rect.y > max_scope_y && max_scope_y > min_scope_y) {
        ui->scope_rect.y = max_scope_y;
    }
    /* Unless the user sized it, the scope shrinks so its button rows always fit above the status
       bar. Four rows now: the channel strip that says which channel the vertical controls move
       sits between the scale row and the tabs. */
    if (!ui->scope_user_sized) {
        /* Four button rows, and then the TIME / VOLTS / TRIG row under them - 8 px of gap, 12 of
           glyph, 4 of air. Reserving only the buttons left that row hanging over the status bar,
           where it was drawn sliced in half; it is now simply dropped when it does not fit, so
           without this the settings a scope user reads most were the ones that disappeared. */
        int need_below = 5 + 4 * 26 + 8 + 24;
        int max_h = ui->window_height - STATUSBAR_HEIGHT - ui->scope_rect.y - need_below;
        int h = ui->scope_default_h > 0 ? ui->scope_default_h : ui->scope_rect.h;
        if (h > max_h) h = max_h;
        if (h < 120) h = 120;
        ui->scope_rect.h = h;
    }

    // Scope control buttons (one shared layout for the main and the pop-out window)
    ui_layout_scope_buttons(ui, ui->scope_rect.x, ui->scope_rect.y + ui->scope_rect.h + 5 - ui->scope_controls_scroll,
                            ui->scope_rect.x + ui->scope_rect.w);
    int scope_btn_h = 22;
    int scope_btn_y = ui->scope_buttons_bottom - scope_btn_h;

    // Calculate scope controls content and visible heights for scrolling
    // Content: buttons (3 rows) + info section + measurements
    // The final scope_btn_y is at the last row, add button height to get bottom of buttons
    int buttons_end_y = scope_btn_y + scope_btn_h;
    int buttons_height = buttons_end_y - (ui->scope_rect.y + ui->scope_rect.h + 5);

    // Info section: +100 offset, then +15 for channel readings, +18 for measurements, +14 header + 38*channels
    int info_height = 8 + 15 + 18 + 14 + (ui->scope_num_channels * 38);

    // Total content height (from bottom of scope)
    ui->scope_controls_content_height = buttons_height + info_height;

    // Visible height: from bottom of scope to status bar
    ui->scope_controls_visible_height = ui->window_height - STATUSBAR_HEIGHT - (ui->scope_rect.y + ui->scope_rect.h + 5);
    if (ui->scope_controls_visible_height < 50) ui->scope_controls_visible_height = 50;

    // Clamp scroll offset
    int scope_max_scroll = ui->scope_controls_content_height - ui->scope_controls_visible_height;
    if (scope_max_scroll < 0) scope_max_scroll = 0;
    if (ui->scope_controls_scroll > scope_max_scroll) ui->scope_controls_scroll = scope_max_scroll;
    if (ui->scope_controls_scroll < 0) ui->scope_controls_scroll = 0;
}

// Oscilloscope autoset - automatically configure scope based on signal analysis
/* A circuit's scope preset only means anything against a clean starting state. Autoset and the
   knobs both write per-channel offsets, per-channel scales and a trigger, and those belong to the
   circuit that was on the screen when they were set: carried into the next circuit, a trigger
   level of 9 V from an amplifier sits above everything a logic template ever reaches, so it never
   fires and the trace never appears. */
void ui_scope_reset_for_template(UIState *ui) {
    for (int ch = 0; ch < MAX_PROBES; ch++) {
        ui->scope_channels[ch].offset = 0.0;
        ui->scope_channels[ch].volt_div = 0.0;      /* follow the main setting again */
    }
    ui->trigger_channel = 0;
    ui->trigger_level = 0.0;
    ui->trigger_position = 0.5;
    ui->scope_capture_valid = false;
}

/* One 1-2-5 step of the vertical scale, on whatever the ALL / channel chips point at. With ALL
   selected it moves the shared scale and drops the per-channel overrides, so everything follows
   it again; with a channel selected only that channel moves, which is how a 5 V rail and a 50 mV
   ripple end up legible on the same screen. */
void ui_scope_volt_step(UIState *ui, int dir) {
    static const double steps[] = {
        0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0,
        100.0, 200.0, 500.0, 1e3, 2e3, 5e3, 10e3, 20e3, 50e3, 100e3, 200e3, 500e3
    };
    const int n = (int)(sizeof steps / sizeof steps[0]);
    if (!ui || dir == 0) return;
    int ch = ui->scope_scale_all ? -1 : ui->scope_selected_channel;
    if (ch >= MAX_PROBES) ch = -1;
    /* Start from what the channel is actually showing. In a fitted band that is the scale the
       fit chose, not the shared one - stepping from the shared scale jumped a 10 mV ripple
       straight to volts per division. */
    double cur;
    if (ch < 0) cur = ui->scope_volt_div;
    else if (ui->scope_channels[ch].volt_div > 0) cur = ui->scope_channels[ch].volt_div;
    else if (ui->scope_stacked && ui->scope_stack_fit && ui->scope_band_vdiv[ch] > 0)
        cur = ui->scope_band_vdiv[ch];
    else cur = ui_channel_volt_div(ui, ch);
    double next = cur;
    if (dir > 0) {
        for (int i = 0; i < n - 1; i++) if (cur <= steps[i] * 1.01) { next = steps[i + 1]; break; }
    } else {
        for (int i = n - 1; i > 0; i--) if (cur >= steps[i] * 0.99) { next = steps[i - 1]; break; }
    }
    if (ch >= 0) {
        ui->scope_channels[ch].volt_div = next;
    } else {
        ui->scope_volt_div = next;
        for (int i = 0; i < MAX_PROBES; i++) ui->scope_channels[i].volt_div = 0.0;
    }
    /* The one-shot autoscale would otherwise overwrite this a frame later. The per-band fit is
       left alone: it is what centres each band on its own channel, and the scale set here is
       used in place of the fitted one. */
    ui->scope_auto_vdiv_pending = false;
    ui->scope_capture_valid = false;
}

/* The most recent edge in the recorded history, or -1. This is what decides whether a trace
   stands still or crawls, so --trig-test runs exactly this rather than its own copy. */
int ui_scope_find_trigger(const UIState *ui, const double *times, const double *values, int count,
                          double time_window, double level) {
    if (!ui || !times || !values || count <= 10) return -1;
    double span = times[count - 1] - times[0];
    if (span <= 0) return -1;
    /* enough samples after the trigger to fill the part of the screen that comes after it */
    double post_trigger_ratio = 1.0 - ui->trigger_position;
    int min_post_samples = (int)(time_window / span * count * post_trigger_ratio);
    if (min_post_samples < 5) min_post_samples = 5;
    int search_end = count - min_post_samples;
    int search_start = count / 10;      /* don't search too far back */
    if (search_start < 1) search_start = 1;

    for (int i = search_end; i >= search_start; i--) {
        double v_prev = values[i - 1], v_curr = values[i];
        bool rise = (v_prev < level && v_curr >= level);
        bool fall = (v_prev > level && v_curr <= level);
        switch (ui->trigger_edge) {
            case TRIG_EDGE_RISING:  if (rise) return i; break;
            case TRIG_EDGE_FALLING: if (fall) return i; break;
            default:                if (rise || fall) return i; break;
        }
    }
    return -1;
}

/* Point the trigger at something that actually crosses it. A level of 0 V never fires on a
   rectified, pulsed or DC-offset output, and the display free-runs and crawls. The channel with
   the largest swing wins - the current one keeps its job if it still swings a tenth as much -
   and the level goes at the middle of that channel's range, which every cycle has to cross. */
void ui_scope_autotrigger(UIState *ui, Simulation *sim) {
    if (!ui || !sim) return;
    static double t_buf[MAX_HISTORY], v_buf[MAX_HISTORY];
    double lo[MAX_PROBES], hi[MAX_PROBES];
    int nch = ui->scope_num_channels < MAX_PROBES ? ui->scope_num_channels : MAX_PROBES;
    int best = -1;
    double best_swing = 0;
    for (int ch = 0; ch < nch; ch++) {
        lo[ch] = 1e300; hi[ch] = -1e300;
        if (!ui->scope_channels[ch].enabled) continue;
        int n = simulation_get_history(sim, ui->scope_channels[ch].probe_idx, t_buf, v_buf, MAX_HISTORY);
        for (int i = 0; i < n; i++) {
            if (v_buf[i] < lo[ch]) lo[ch] = v_buf[i];
            if (v_buf[i] > hi[ch]) hi[ch] = v_buf[i];
        }
        if (n < 2 || hi[ch] < lo[ch]) continue;
        double sw = hi[ch] - lo[ch];
        if (sw > best_swing) { best_swing = sw; best = ch; }
    }
    int cur = ui->trigger_channel;
    if (cur >= 0 && cur < nch && hi[cur] >= lo[cur] && (hi[cur] - lo[cur]) > 0.1 * best_swing)
        best = cur;
    if (best >= 0 && (hi[best] - lo[best]) > 1e-6) {
        ui->trigger_channel = best;
        ui->trigger_level = 0.5 * (lo[best] + hi[best]);
        ui->scope_capture_valid = false;
    }
}

UIActionKind ui_action_kind(int action, int *index) {
    int idx = 0;
    UIActionKind k = UIA_NONE;
    if (action <= UI_ACTION_NONE) k = UIA_NONE;
    else if (action < UI_ACTION_SELECT_TOOL) { k = UIA_SIMPLE; idx = action; }
    else if (action < UI_ACTION_SELECT_COMP) { k = UIA_TOOL; idx = action - UI_ACTION_SELECT_TOOL; }
    else if (action < UI_ACTION_SELECT_COMP + 300) { k = UIA_COMP; idx = action - UI_ACTION_SELECT_COMP; }
    else if (action == UI_ACTION_PROP_APPLY) k = UIA_PROP_APPLY;
    else if (action >= UI_ACTION_PROP_EDIT && action < UI_ACTION_PROP_EDIT + UI_ACTION_PROP_EDIT_MAX) {
        k = UIA_PROP_EDIT; idx = action - UI_ACTION_PROP_EDIT;
    } else if (action >= UI_ACTION_SELECT_CIRCUIT &&
               action < UI_ACTION_SELECT_CIRCUIT + UI_ACTION_SELECT_CIRCUIT_MAX) {
        k = UIA_CIRCUIT; idx = action - UI_ACTION_SELECT_CIRCUIT;
    } else if (action >= UI_ACTION_SELECT_SUBCIRCUIT && action < UI_ACTION_SELECT_SUBCIRCUIT + 1000) {
        k = UIA_SUBCIRCUIT; idx = action - UI_ACTION_SELECT_SUBCIRCUIT;
    } else if (action >= UI_ACTION_SCOPE_CH_SEL &&
               action < UI_ACTION_SCOPE_CH_SEL + UI_ACTION_SCOPE_CH_SEL_MAX) {
        k = UIA_SCOPE_CH; idx = action - UI_ACTION_SCOPE_CH_SEL;
    }
    if (index) *index = idx;
    return k;
}

/* Placing a template sets the scope up for it. Kept here rather than inline in the app so the
   headless audit drives the same code the app does instead of a copy that can drift from it. */
void ui_scope_apply_template_preset(UIState *ui, CircuitTemplateType type) {
    ui_scope_reset_for_template(ui);
    double td = circuit_template_scope_time_div(type);
    if (td > 0) ui->scope_time_div = td;
    double vd = circuit_template_scope_volt_div(type);
    if (vd > 0) ui->scope_volt_div = vd;
    int fl = circuit_template_scope_flags(type);
    ui->scope_ac_coupling = (fl & SCOPE_FLAG_AC) != 0;
    ui->trigger_mode = (fl & SCOPE_FLAG_SINGLE) ? TRIG_SINGLE : TRIG_AUTO;
    ui->scope_stacked     = (fl & SCOPE_FLAG_STACK) != 0;
    ui->scope_stack_fit   = (fl & SCOPE_FLAG_FIT) != 0;
    ui->scope_auto_vdiv_pending = true;   /* refine V/div from real data once it flows */
}

void ui_scope_autoset(UIState *ui, Simulation *sim) {
    if (!ui || !sim || ui->scope_num_channels == 0) return;

    // Standard volt/div and time/div values (1-2-5 sequence)
    static const double volt_divs[] = {0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0, 200.0, 500.0, 1e3, 2e3, 5e3, 10e3, 20e3, 50e3, 100e3, 200e3, 500e3};
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

    // If no signal found, check if we have DC (amplitude ~0 but signal present)
    // For DC signals, global_min ≈ global_max, so we need to scale based on DC level
    if (global_max <= global_min || samples_with_signal == 0) {
        // Check if we actually have data (DC signal case)
        if (global_min < 1e9 && global_max > -1e9) {
            // We have data but no AC content - this is a DC signal
            // Scale volt/div based on the DC level so the signal fits on screen
            // The scope shows ±4 divisions from center (8 total), so signal must fit in 4 divs
            double dc_level = fmax(fabs(global_min), fabs(global_max));
            if (dc_level > 0.001) {
                // Find the smallest volt/div that keeps the DC level within 3.5 divisions
                // (leaving 0.5 division margin from the edge for better visibility)
                // Formula: dc_level / volt_div <= 3.5, so volt_div >= dc_level / 3.5
                double min_volt_div = dc_level / 3.5;
                int volt_idx = 0;
                for (int i = 0; i < num_volt_divs; i++) {
                    if (volt_divs[i] >= min_volt_div) {
                        volt_idx = i;
                        break;
                    }
                    volt_idx = i;
                }
                ui->scope_volt_div = volt_divs[volt_idx];
            } else {
                ui->scope_volt_div = 1.0;
            }
            ui->scope_time_div = 0.001;  // 1ms/div
            ui->trigger_level = (global_min + global_max) / 2.0;
            return;
        }
        // No data at all - use defaults
        ui->scope_volt_div = 1.0;
        ui->scope_time_div = 0.001;  // 1ms/div
        ui->trigger_level = 0;
        return;
    }

    /* Volts/div has to satisfy two things, and this used to check only the first: the signal has
       to fit the screen's height, AND it has to fit where it actually sits. The screen is eight
       divisions centred on zero, so a 0-5 V square wave scaled from its 5 V span alone lands at
       1 V/div and runs five divisions above centre - half of it off the top, which is what
       "autoset does not match what I see" looks like. Take whichever of the two is coarser. */
    double signal_range = global_max - global_min;
    double by_span = signal_range * 1.4 / 8.0;
    double excursion = fmax(fabs(global_max), fabs(global_min));
    double by_offset = excursion / 3.5;            /* half a division of margin at the edge */
    double ideal_volt_div = fmax(by_span, by_offset);

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

    /* Trigger on a channel from that channel's own numbers.
       The level used to be the midpoint of every channel taken together while the trigger stayed
       on CH1, so an amplifier - a 100 mV input and a 9 V output - got a level of about 4.5 V on
       a channel that never leaves +/-0.1 V. It cannot fire, and the trace free-runs and slides.
       Twenty templates were doing this. Pick the channel with the most swing, and take the level
       from the middle of that channel. */
    {
        int best = -1;
        double best_swing = 0, best_mid = 0;
        for (int ch = 0; ch < ui->scope_num_channels; ch++) {
            if (!ui->scope_channels[ch].enabled) continue;
            double times[MAX_HISTORY], values[MAX_HISTORY];
            int n = simulation_get_history(sim, ui->scope_channels[ch].probe_idx, times, values, MAX_HISTORY);
            if (n < 10) continue;
            double lo = values[0], hi = values[0];
            for (int i = 1; i < n; i++) { if (values[i] < lo) lo = values[i]; if (values[i] > hi) hi = values[i]; }
            if (hi - lo > best_swing) { best_swing = hi - lo; best = ch; best_mid = (lo + hi) / 2.0; }
        }
        if (best >= 0 && best_swing > 1e-9) {
            ui->trigger_channel = best;
            ui->trigger_level = best_mid;
        } else {
            ui->trigger_level = (global_min + global_max) / 2.0;   /* nothing moves: harmless */
        }
    }

    /* Where each trace sits.
       This used to push every channel down by the midpoint of all of them together. In the
       per-channel view - which is what the amplifier templates open in - the display is already
       centring each band on that channel's own mean, so the two shifts add: a common emitter's
       output, 200 mV of signal on a 9 V rail, ended up about fourteen volts from its band at
       100 mV/div, which is a hundred and thirty divisions away. The screen went blank and the
       readouts kept updating, because the numbers were fine and only the drawing was off.

       In the per-channel view the bands do their own centring, so the right offset is none. In
       the plain view each channel is centred on its own mean, which is what puts a trace on the
       screen whatever DC it rides on. */
    bool per_channel = ui->scope_stacked && ui->scope_stack_fit;
    for (int ch = 0; ch < ui->scope_num_channels; ch++) {
        if (!ui->scope_channels[ch].enabled) continue;
        if (per_channel) { ui->scope_channels[ch].offset = 0.0; continue; }
        double times[MAX_HISTORY], values[MAX_HISTORY];
        int n = simulation_get_history(sim, ui->scope_channels[ch].probe_idx, times, values, MAX_HISTORY);
        if (n < 10) { ui->scope_channels[ch].offset = 0.0; continue; }
        double lo = values[0], hi = values[0];
        for (int i = 1; i < n; i++) { if (values[i] < lo) lo = values[i]; if (values[i] > hi) hi = values[i]; }
        ui->scope_channels[ch].offset = -(lo + hi) / 2.0;
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

// Check if point is in scope controls area (below the scope, buttons/info/measurements)
bool ui_point_in_scope_controls(UIState *ui, int x, int y) {
    if (!ui) return false;
    // Scope controls are in the right panel, below the oscilloscope
    int scope_controls_x = ui->scope_rect.x;
    int scope_controls_y = ui->scope_rect.y + ui->scope_rect.h + 5;
    int scope_controls_w = ui->scope_rect.w;
    int scope_controls_h = ui->scope_controls_visible_height;
    return (x >= scope_controls_x && x < scope_controls_x + scope_controls_w &&
            y >= scope_controls_y && y < scope_controls_y + scope_controls_h);
}

// Scroll scope controls area
void ui_scope_controls_scroll(UIState *ui, int direction) {
    if (!ui) return;
    int max_scroll = ui->scope_controls_content_height - ui->scope_controls_visible_height;
    if (max_scroll < 0) max_scroll = 0;

    ui->scope_controls_scroll -= direction * 20;  // 20px per scroll tick

    if (ui->scope_controls_scroll < 0) ui->scope_controls_scroll = 0;
    if (ui->scope_controls_scroll > max_scroll) ui->scope_controls_scroll = max_scroll;
}

// Pop-out scope: swap in the popup window's rect and button layout, returning the main
// window's coordinates so they can be restored after the event / render.

/* ======================= Pop-out bench-scope front panel =======================
   A real scope is a screen in a bezel with a column of knobs beside it. The pop-out window
   draws that, and every knob moves a value the docked controls also reach: there is no second
   copy of the state, so the two views can never disagree. */

#define SCOPE_PANEL_W   250      /* right-hand column of knobs */

static const char *knob_label(const UIState *ui, int k) {
    (void)ui;
    switch (k) {
        case KNOB_VOLTS:     return "VOLTS/DIV";
        case KNOB_TIME:      return "TIME/DIV";
        case KNOB_POSITION:  return "POSITION";
        case KNOB_TRIGGER:   return "TRIG LEVEL";
        case KNOB_INTENSITY: return "INTENSITY";
        default:             return "CHANNEL";
    }
}

/* 0..1 along the knob's travel, for the pointer angle */
static double knob_fraction(UIState *ui, int k) {
    switch (k) {
        case KNOB_VOLTS: {
            double v = ui_channel_volt_div(ui, ui->scope_selected_channel);
            if (v <= 0) return 0.5;
            double f = (log10(v) + 3.0) / 8.7;            /* 1 mV .. 500 kV per division */
            return f < 0 ? 0 : (f > 1 ? 1 : f);
        }
        case KNOB_TIME: {
            double t = ui->scope_time_div;
            if (t <= 0) return 0.5;
            double f = (log10(t) + 9.0) / 11.0;           /* 1 ns .. 100 s per division */
            return f < 0 ? 0 : (f > 1 ? 1 : f);
        }
        case KNOB_POSITION: {
            int ch = ui->scope_selected_channel;
            if (ch < 0 || ch >= MAX_PROBES) return 0.5;
            double span = ui->scope_volt_div * 4.0;
            if (span <= 0) return 0.5;
            double f = 0.5 + ui->scope_channels[ch].offset / (2.0 * span);
            return f < 0 ? 0 : (f > 1 ? 1 : f);
        }
        case KNOB_TRIGGER: {
            double span = ui->scope_volt_div * 4.0;
            if (span <= 0) return 0.5;
            double f = 0.5 + ui->trigger_level / (2.0 * span);
            return f < 0 ? 0 : (f > 1 ? 1 : f);
        }
        case KNOB_INTENSITY:
            return (ui->brightness - 0.25) / 0.75;
        default: {
            int n = ui->scope_num_channels > 0 ? ui->scope_num_channels : 1;
            if (n < 2) return 0.5;
            int ch = ui->scope_selected_channel;
            if (ch < 0) ch = 0;
            return (double)ch / (double)(n - 1);
        }
    }
}

static void knob_value_text(UIState *ui, int k, char *buf, size_t n) {
    switch (k) {
        case KNOB_VOLTS: {
            double v = ui_channel_volt_div(ui, ui->scope_selected_channel);
            if (v >= 1000)     snprintf(buf, n, "%.4g kV", v / 1000.0);
            else if (v >= 1)   snprintf(buf, n, "%.4g V", v);
            else               snprintf(buf, n, "%.4g mV", v * 1000.0);
            break;
        }
        case KNOB_TIME: {
            double t = ui->scope_time_div;
            if (t >= 1)         snprintf(buf, n, "%.4g s", t);
            else if (t >= 1e-3) snprintf(buf, n, "%.4g ms", t * 1e3);
            else if (t >= 1e-6) snprintf(buf, n, "%.4g us", t * 1e6);
            else                snprintf(buf, n, "%.4g ns", t * 1e9);
            break;
        }
        case KNOB_POSITION: {
            int ch = ui->scope_selected_channel;
            double o = (ch >= 0 && ch < MAX_PROBES) ? ui->scope_channels[ch].offset : 0.0;
            snprintf(buf, n, "%+.3g V", o);
            break;
        }
        case KNOB_TRIGGER:   snprintf(buf, n, "%+.3g V", ui->trigger_level); break;
        case KNOB_INTENSITY: snprintf(buf, n, "%d %%", (int)(ui->brightness * 100.0 + 0.5)); break;
        default:             snprintf(buf, n, "%s", ui_channel_name(ui, ui->scope_selected_channel)); break;
    }
}

void ui_layout_scope_panel(UIState *ui, int win_w, int win_h) {
    if (!ui) return;
    int px = win_w - SCOPE_PANEL_W + 14;
    int py = 74;                                   /* below the panel's name plate */
    int cell_w = (SCOPE_PANEL_W - 34) / 2, cell_h = 104;
    for (int i = 0; i < KNOB_COUNT; i++) {
        int col = i % 2, row = i / 2;
        ScopeKnob *k = &ui->scope_knobs[i];
        k->bounds = (Rect){ px + col * cell_w, py + row * cell_h, cell_w - 6, cell_h - 8 };
        k->r = 25;
        k->cx = k->bounds.x + k->bounds.w / 2;
        k->cy = k->bounds.y + 16 + k->r;
    }
    /* The INPUTS list: one row per channel showing what it is on, and a click target so you can
       point the vertical section at an input without turning the CHANNEL knob to it. With eight
       possible probes a knob each is a wall of knobs; one section and a selector is how a bench
       scope does it, and it reads better. */
    int sy = py + ((KNOB_COUNT + 1) / 2) * cell_h + 10 + 92 + 32;
    for (int ch = 0; ch < MAX_PROBES; ch++)
        ui->scope_input_rows[ch] = (Rect){ px, sy + ch * 18, SCOPE_PANEL_W - 34, 16 };
    (void)win_h;
}

static void knob_ring(SDL_Renderer *r, int cx, int cy, int rad, Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    int prev_x = cx + rad, prev_y = cy;
    for (int a = 6; a <= 360; a += 6) {
        double t = a * M_PI / 180.0;
        int x = cx + (int)(rad * cos(t)), y = cy + (int)(rad * sin(t));
        SDL_RenderDrawLine(r, prev_x, prev_y, x, y);
        prev_x = x; prev_y = y;
    }
}

void ui_render_scope_panel(UIState *ui, SDL_Renderer *renderer) {
    if (!ui || !renderer || !ui->scope_panel_active) return;
    int win_w = 0, win_h = 0;
    SDL_GetRendererOutputSize(renderer, &win_w, &win_h);

    /* instrument body */
    SDL_SetRenderDrawColor(renderer, 0x1b, 0x16, 0x28, 0xff);
    SDL_Rect body = { win_w - SCOPE_PANEL_W, 0, SCOPE_PANEL_W, win_h };
    SDL_RenderFillRect(renderer, &body);
    SDL_SetRenderDrawColor(renderer, 0x39, 0x2b, 0x5c, 0xff);
    SDL_RenderDrawLine(renderer, body.x, 0, body.x, win_h);

    /* name plate */
    SDL_SetRenderDrawColor(renderer, 0xe6, 0xdc, 0xff, 0xff);
    ui_draw_text(renderer, "CIRCUIT PLAYGROUND", body.x + 16, 16);
    SDL_SetRenderDrawColor(renderer, 0xff, 0x2d, 0x95, 0xff);
    ui_draw_text(renderer, "8-CHANNEL OSCILLOSCOPE", body.x + 16, 34);
    SDL_SetRenderDrawColor(renderer, 0x39, 0x2b, 0x5c, 0xff);
    SDL_RenderDrawLine(renderer, body.x + 12, 56, win_w - 12, 56);

    /* screen bezel: a bright inner lip and a darker outer one, so the graticule reads as a
       recessed CRT rather than a rectangle of the background */
    SDL_Rect bez = { ui->scope_rect.x - 6, ui->scope_rect.y - 6,
                     ui->scope_rect.w + 12, ui->scope_rect.h + 12 };
    SDL_SetRenderDrawColor(renderer, 0x6a, 0x52, 0x9c, 0xff);
    SDL_RenderDrawRect(renderer, &bez);
    for (int i = 1; i <= 3; i++) {
        SDL_Rect o = { bez.x - i, bez.y - i, bez.w + 2 * i, bez.h + 2 * i };
        SDL_SetRenderDrawColor(renderer, 0x39 - i * 6, 0x2b - i * 4, 0x5c - i * 8, 0xff);
        SDL_RenderDrawRect(renderer, &o);
    }

    /* the INPUTS list: what every channel is on, and which one the knobs are driving */
    {
        int hy = ui->scope_input_rows[0].y - 20;
        SDL_SetRenderDrawColor(renderer, 0x39, 0x2b, 0x5c, 0xff);
        SDL_RenderDrawLine(renderer, body.x + 12, hy - 6, win_w - 12, hy - 6);
        SDL_SetRenderDrawColor(renderer, 0xb9, 0xa6, 0xe6, 0xff);
        ui_draw_text(renderer, "INPUTS - CLICK TO DRIVE", body.x + 16, hy);
        for (int ch = 0; ch < MAX_PROBES; ch++) {
            Rect r = ui->scope_input_rows[ch];
            bool live = ch < ui->scope_num_channels && ui->scope_channels[ch].enabled;
            bool sel = live && ch == ui->scope_selected_channel;
            if (sel) {
                SDL_SetRenderDrawColor(renderer, 0x2a, 0x20, 0x44, 0xff);
                SDL_Rect fill = { r.x - 2, r.y - 1, r.w + 4, r.h };
                SDL_RenderFillRect(renderer, &fill);
                Color pc = PROBE_COLORS[ch];
                SDL_SetRenderDrawColor(renderer, pc.r, pc.g, pc.b, 0xff);
                SDL_RenderDrawRect(renderer, &fill);
            }
            char line[40];
            if (live) {
                double vd = ui_channel_volt_div(ui, ch);
                char vs[16];
                if (vd >= 1000)    snprintf(vs, sizeof vs, "%.4g kV", vd / 1000.0);
                else if (vd >= 1)  snprintf(vs, sizeof vs, "%.4g V", vd);
                else               snprintf(vs, sizeof vs, "%.4g mV", vd * 1000.0);
                snprintf(line, sizeof line, "%-6s %8s%s", ui_channel_name(ui, ch), vs,
                         ui->scope_channels[ch].volt_div > 0 ? "" : "  (main)");
                Color pc = PROBE_COLORS[ch];
                SDL_SetRenderDrawColor(renderer, pc.r, pc.g, pc.b, 0xff);
            } else {
                snprintf(line, sizeof line, "%-6s        --", ui_channel_name(ui, ch));
                SDL_SetRenderDrawColor(renderer, 0x50, 0x46, 0x6c, 0xff);
            }
            ui_draw_text(renderer, line, r.x + 4, r.y + 2);
        }
    }

    for (int i = 0; i < KNOB_COUNT; i++) {
        ScopeKnob *k = &ui->scope_knobs[i];
        bool active = (ui->scope_knob_active == i), hot = active || (ui->scope_knob_hover == i);

        /* Sliders: the same control drawn as a track and a handle, for anyone who would rather
           see where a setting sits in its travel than read it off a pointer. Drag is identical -
           right or up turns it up - so nothing else in the panel changes. */
        if (ui->scope_sliders) {
            int half = k->r + 10, tx = k->cx - half, tw = half * 2, ty = k->cy - 3;
            SDL_SetRenderDrawColor(renderer, 0x1c, 0x14, 0x30, 0xff);
            SDL_Rect track = { tx, ty, tw, 7 };
            SDL_RenderFillRect(renderer, &track);
            Color rail = hot ? (Color){0x00, 0xff, 0xd5, 0xff} : (Color){0x6a, 0x52, 0x9c, 0xff};
            int rsel = ui->scope_selected_channel;
            if ((i == KNOB_VOLTS || i == KNOB_POSITION || i == KNOB_CHANNEL) && !hot &&
                rsel >= 0 && rsel < MAX_PROBES && rsel < ui->scope_num_channels &&
                ui->scope_channels[rsel].enabled)
                rail = PROBE_COLORS[rsel];
            SDL_SetRenderDrawColor(renderer, rail.r, rail.g, rail.b, 0xff);
            SDL_RenderDrawRect(renderer, &track);
            for (int t = 0; t <= 4; t++) {
                int gx = tx + tw * t / 4;
                SDL_RenderDrawLine(renderer, gx, ty + 9, gx, ty + 12);
            }
            int hx = tx + (int)(knob_fraction(ui, i) * (tw - 10));
            Color hcol = hot ? (Color){0x00, 0xff, 0xd5, 0xff} : (Color){0xff, 0xd7, 0x4a, 0xff};
            SDL_SetRenderDrawColor(renderer, hcol.r, hcol.g, hcol.b, 0xff);
            SDL_Rect handle = { hx, ty - 6, 10, 19 };
            SDL_RenderFillRect(renderer, &handle);
            SDL_SetRenderDrawColor(renderer, 0x1c, 0x14, 0x30, 0xff);
            SDL_RenderDrawRect(renderer, &handle);
            goto knob_caption;
        }

        /* body: a filled disc, brighter when the pointer is on it */
        Color face = hot ? (Color){0x3a, 0x2c, 0x60, 0xff} : (Color){0x2a, 0x20, 0x44, 0xff};
        SDL_SetRenderDrawColor(renderer, face.r, face.g, face.b, 0xff);
        for (int dy = -k->r; dy <= k->r; dy++) {
            int dx = (int)sqrt((double)(k->r * k->r - dy * dy));
            SDL_RenderDrawLine(renderer, k->cx - dx, k->cy + dy, k->cx + dx, k->cy + dy);
        }
        /* the vertical section wears the colour of the channel it is driving */
        Color ring = hot ? (Color){0x00, 0xff, 0xd5, 0xff} : (Color){0x6a, 0x52, 0x9c, 0xff};
        int sel_ch = ui->scope_selected_channel;
        bool vertical = (i == KNOB_VOLTS || i == KNOB_POSITION || i == KNOB_CHANNEL);
        if (vertical && !hot && sel_ch >= 0 && sel_ch < MAX_PROBES &&
            sel_ch < ui->scope_num_channels && ui->scope_channels[sel_ch].enabled)
            ring = PROBE_COLORS[sel_ch];
        knob_ring(renderer, k->cx, k->cy, k->r, ring);

        /* travel ticks: 270 degrees, 7 o'clock round to 5 o'clock */
        SDL_SetRenderDrawColor(renderer, 0x9c, 0x82, 0xd8, 0xff);
        for (int t = 0; t <= 8; t++) {
            double a = (135.0 + 270.0 * t / 8.0) * M_PI / 180.0;
            int x0 = k->cx + (int)((k->r + 3) * cos(a)), y0 = k->cy + (int)((k->r + 3) * sin(a));
            int x1 = k->cx + (int)((k->r + 6) * cos(a)), y1 = k->cy + (int)((k->r + 6) * sin(a));
            SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
        }

        /* pointer */
        double f = knob_fraction(ui, i);
        double a = (135.0 + 270.0 * f) * M_PI / 180.0;
        Color ptr = hot ? (Color){0x00, 0xff, 0xd5, 0xff} : (Color){0xff, 0xd7, 0x4a, 0xff};
        SDL_SetRenderDrawColor(renderer, ptr.r, ptr.g, ptr.b, 0xff);
        for (int w = -1; w <= 1; w++)
            SDL_RenderDrawLine(renderer, k->cx + w, k->cy,
                               k->cx + w + (int)((k->r - 4) * cos(a)), k->cy + (int)((k->r - 4) * sin(a)));

    knob_caption:
        /* label above, value below */
        {
        const char *lab = knob_label(ui, i);
        SDL_SetRenderDrawColor(renderer, 0xb9, 0xa6, 0xe6, 0xff);
        ui_draw_text(renderer, lab, k->cx - (int)(strlen(lab) * 4), k->bounds.y + 2);
        char val[32]; knob_value_text(ui, i, val, sizeof(val));
        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0xd5, 0xff);
        ui_draw_text(renderer, val, k->cx - (int)(strlen(val) * 4), k->cy + k->r + 8);
        }
    }

    /* status plate: the settings that are switches rather than knobs */
    int sy = 74 + 3 * 104 + 10;
    SDL_SetRenderDrawColor(renderer, 0x39, 0x2b, 0x5c, 0xff);
    SDL_Rect plate = { body.x + 14, sy, SCOPE_PANEL_W - 28, 92 };
    SDL_RenderDrawRect(renderer, &plate);
    const char *tm = ui->trigger_mode == TRIG_AUTO ? "AUTO" :
                     ui->trigger_mode == TRIG_NORMAL ? "NORM" : "SINGLE";
    const char *te = ui->trigger_edge == TRIG_EDGE_RISING ? "RISING" :
                     ui->trigger_edge == TRIG_EDGE_FALLING ? "FALLING" : "BOTH";
    char line[64];
    SDL_SetRenderDrawColor(renderer, 0xb9, 0xa6, 0xe6, 0xff);
    snprintf(line, sizeof line, "TRIG  %s %s", tm, te);
    ui_draw_text(renderer, line, plate.x + 8, sy + 8);
    snprintf(line, sizeof line, "MODE  %s %s", ui->display_mode == SCOPE_MODE_XY ? "X-Y" : "Y-T",
             ui->scope_ac_coupling ? "AC" : "DC");
    ui_draw_text(renderer, line, plate.x + 8, sy + 24);
    snprintf(line, sizeof line, "VIEW  %s", ui->scope_stacked ? "STACKED" : "OVERLAY");
    ui_draw_text(renderer, line, plate.x + 8, sy + 40);
    if (ui->scope_paused) SDL_SetRenderDrawColor(renderer, 0xff, 0xd7, 0x4a, 0xff);
    else                  SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0xd5, 0xff);
    snprintf(line, sizeof line, "%s   %d CH", ui->scope_paused ? "HOLD" : "RUN ", ui->scope_num_channels);
    ui_draw_text(renderer, line, plate.x + 8, sy + 60);

    {   /* KNOBS / SLIDERS sits in the plate's own corner, beside the RUN line, so it cannot
           land on top of anything; the hint goes under everything as it always did. */
        ui->scope_style_btn = (Rect){ plate.x + plate.w - 84, sy + 54, 76, 20 };
        SDL_Rect sb = { ui->scope_style_btn.x, ui->scope_style_btn.y,
                        ui->scope_style_btn.w, ui->scope_style_btn.h };
        SDL_SetRenderDrawColor(renderer, 0x2a, 0x20, 0x44, 0xff);
        SDL_RenderFillRect(renderer, &sb);
        SDL_SetRenderDrawColor(renderer, 0x6a, 0x52, 0x9c, 0xff);
        SDL_RenderDrawRect(renderer, &sb);
        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0xd5, 0xff);
        ui_draw_text(renderer, ui->scope_sliders ? "SLIDERS" : " KNOBS", sb.x + 6, sb.y + 6);

        const ScopeKnob *last = &ui->scope_knobs[KNOB_COUNT - 1];
        SDL_SetRenderDrawColor(renderer, 0x7d, 0x6d, 0xa8, 0xff);
        ui_draw_text(renderer, ui->scope_sliders ? "drag a slider left / right"
                                                 : "drag a knob up / down",
                     body.x + 16, last->bounds.y + last->bounds.h + 8);
    }
}

/* Which INPUTS row is under the pointer, or -1. Clicking one points the vertical section at
   that channel - the same thing the CHANNEL knob does, without counting detents. */
int ui_scope_input_row_at(UIState *ui, int x, int y) {
    if (!ui || !ui->scope_panel_active) return -1;
    for (int ch = 0; ch < MAX_PROBES; ch++) {
        if (ch >= ui->scope_num_channels || !ui->scope_channels[ch].enabled) continue;
        Rect r = ui->scope_input_rows[ch];
        if (r.w <= 0) continue;
        if (x >= r.x && x < r.x + r.w && y >= r.y - 1 && y < r.y + r.h) return ch;
    }
    return -1;
}

int ui_scope_knob_at(UIState *ui, int x, int y) {
    if (!ui || !ui->scope_panel_active) return -1;
    for (int i = 0; i < KNOB_COUNT; i++) {
        ScopeKnob *k = &ui->scope_knobs[i];
        if (ui->scope_sliders) {
            /* the track and its handle, which is a wider target than the disc it replaces */
            int half = k->r + 12;
            if (x >= k->cx - half && x <= k->cx + half && y >= k->cy - 12 && y <= k->cy + 14)
                return i;
            continue;
        }
        int dx = x - k->cx, dy = y - k->cy;
        if (dx * dx + dy * dy <= (k->r + 6) * (k->r + 6)) return i;
    }
    return -1;
}

int ui_scope_knob_drag_xy(UIState *ui, int knob, int dx, int dy) {
    if (!ui || knob < 0 || knob >= KNOB_COUNT) return 0;
    ScopeKnob *k = &ui->scope_knobs[knob];
    /* Right or up turns it up, left or down turns it down. Vertical alone is what a real knob
       does under a finger, but on a screen the hand wants to go the way the value goes, and
       either axis reading the same way means neither is wrong. */
    double up = (double)dx - (double)dy;
    switch (knob) {
        case KNOB_VOLTS: {
            /* One vertical section, driving whichever input the CHANNEL knob is on. Writing to
               that channel's own volts/div is what makes the section per-channel: the others
               keep following the main setting until their turn comes. */
            k->detent += up;
            const double STEP = 14.0;
            int dir = (k->detent >= STEP) ? 1 : (k->detent <= -STEP) ? -1 : 0;
            if (!dir) return 0;
            k->detent = 0;
            static const double steps[] = { 1e-3, 2e-3, 5e-3, 1e-2, 2e-2, 5e-2, 0.1, 0.2, 0.5,
                                            1, 2, 5, 10, 20, 50, 100, 200, 500, 1e3, 2e3, 5e3,
                                            1e4, 2e4, 5e4, 1e5, 2e5, 5e5 };
            const int n = (int)(sizeof steps / sizeof steps[0]);
            int ch = ui->scope_selected_channel;
            double cur = ui_channel_volt_div(ui, ch);
            int i = 0;
            while (i < n - 1 && steps[i] < cur * 0.99) i++;
            i += (dir > 0) ? 1 : -1;
            if (i < 0) i = 0;
            if (i > n - 1) i = n - 1;
            if (ch >= 0 && ch < MAX_PROBES) ui->scope_channels[ch].volt_div = steps[i];
            else                            ui->scope_volt_div = steps[i];
            return 0;
        }
        case KNOB_TIME:
        case KNOB_CHANNEL: {
            k->detent += up;
            const double STEP = 14.0;              /* pixels per detent */
            if (k->detent >= STEP) {
                k->detent = 0;
                return knob == KNOB_TIME ? UI_ACTION_SCOPE_TIME_UP : UI_ACTION_SCOPE_TRIG_CH;
            }
            if (k->detent <= -STEP) {
                k->detent = 0;
                return knob == KNOB_TIME ? UI_ACTION_SCOPE_TIME_DOWN : UI_ACTION_SCOPE_TRIG_CH;
            }
            return 0;
        }
        case KNOB_POSITION: {
            int ch = ui->scope_selected_channel;
            if (ch < 0 || ch >= MAX_PROBES) return 0;
            double span = ui_channel_volt_div(ui, ch) * 4.0;
            double v = ui->scope_channels[ch].offset + up * span / 120.0;
            if (v >  2 * span) v =  2 * span;
            if (v < -2 * span) v = -2 * span;
            ui->scope_channels[ch].offset = v;
            return 0;
        }
        case KNOB_TRIGGER: {
            double span = ui_channel_volt_div(ui, ui->trigger_channel) * 4.0;
            double v = ui->trigger_level + up * span / 120.0;
            if (v >  2 * span) v =  2 * span;
            if (v < -2 * span) v = -2 * span;
            ui->trigger_level = v;
            return 0;
        }
        default:
            ui_set_brightness(ui, ui->brightness + (float)(up / 300.0));
            return 0;
    }
}

ScopeCoordsBackup ui_setup_popup_scope_coords(UIState *ui) {
    ScopeCoordsBackup backup = {0};
    if (!ui || !ui->scope_popped_out || !ui->scope_popup_window) return backup;
    Button *list[SCOPE_BTN_N];
    scope_button_list(ui, list);
    backup.scope_rect = ui->scope_rect;
    backup.buttons_bottom = ui->scope_buttons_bottom;
    for (int i = 0; i < SCOPE_BTN_N; i++) backup.b[i] = list[i]->bounds;
    int popup_w, popup_h;
    SDL_GetWindowSize(ui->scope_popup_window, &popup_w, &popup_h);
    /* front panel down the right, screen and its button rows on the left */
    int screen_w = popup_w - SCOPE_PANEL_W - 36;
    if (screen_w < 260) screen_w = 260;            /* a very narrow window keeps a usable screen */
    ui->scope_rect = (Rect){18, 30, screen_w, popup_h - 130};
    if (ui->scope_rect.h < 120) ui->scope_rect.h = 120;
    ui->scope_panel_active = true;
    ui_layout_scope_panel(ui, popup_w, popup_h);
    ui_layout_scope_buttons(ui, ui->scope_rect.x, ui->scope_rect.y + ui->scope_rect.h + 5, ui->scope_rect.x + ui->scope_rect.w);
    return backup;
}

void ui_restore_popup_scope_coords(UIState *ui, const ScopeCoordsBackup *backup) {
    if (!ui || !backup) return;
    ui->scope_panel_active = false;
    Button *list[SCOPE_BTN_N];
    scope_button_list(ui, list);
    ui->scope_rect = backup->scope_rect;
    ui->scope_buttons_bottom = backup->buttons_bottom;
    for (int i = 0; i < SCOPE_BTN_N; i++) list[i]->bounds = backup->b[i];
}

/* the old one-axis entry point, kept for the headless layout checks */
int ui_scope_knob_drag(UIState *ui, int knob, int dy) {
    return ui_scope_knob_drag_xy(ui, knob, 0, dy);
}
