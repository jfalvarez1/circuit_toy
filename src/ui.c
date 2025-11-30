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
    while (*text) {
        ui_draw_char(r, *text, x, y);
        x += 8;
        text++;
    }
}

void ui_init(UIState *ui) {
    memset(ui, 0, sizeof(UIState));

    // Set initial window dimensions
    ui->window_width = WINDOW_WIDTH;
    ui->window_height = WINDOW_HEIGHT;

    // Initialize properties panel width
    ui->properties_width = PROPERTIES_WIDTH;
    ui->props_resizing = false;

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

    // Speed slider
    ui->speed_slider = (Rect){btn_x + btn_w + 30, 15, 100, 20};
    ui->speed_value = 1.0f;

    // Initialize palette items
    int pal_y = TOOLBAR_HEIGHT + 30;
    int pal_h = 40;
    int col = 0;

    // Tools section
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_NONE, TOOL_SELECT, true, "Select", false, true
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_NONE, TOOL_WIRE, true, "Wire", false, false
    };
    col = 0;
    pal_y += pal_h + 10;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_NONE, TOOL_DELETE, true, "Delete", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_NONE, TOOL_PROBE, true, "Probe", false, false
    };

    // Sources section
    pal_y += pal_h + 20;
    col = 0;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_GROUND, TOOL_COMPONENT, false, "GND", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_DC_VOLTAGE, TOOL_COMPONENT, false, "DC V", false, false
    };
    col = 0;
    pal_y += pal_h + 5;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_AC_VOLTAGE, TOOL_COMPONENT, false, "AC V", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_DC_CURRENT, TOOL_COMPONENT, false, "DC I", false, false
    };

    // Waveform generators section
    col = 0;
    pal_y += pal_h + 5;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_SQUARE_WAVE, TOOL_COMPONENT, false, "Square", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_TRIANGLE_WAVE, TOOL_COMPONENT, false, "Tri", false, false
    };
    col = 0;
    pal_y += pal_h + 5;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_SAWTOOTH_WAVE, TOOL_COMPONENT, false, "Saw", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_NOISE_SOURCE, TOOL_COMPONENT, false, "Noise", false, false
    };

    // Passives section
    pal_y += pal_h + 20;
    col = 0;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_RESISTOR, TOOL_COMPONENT, false, "R", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_CAPACITOR, TOOL_COMPONENT, false, "C", false, false
    };
    col = 0;
    pal_y += pal_h + 5;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_CAPACITOR_ELEC, TOOL_COMPONENT, false, "Elec", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_INDUCTOR, TOOL_COMPONENT, false, "L", false, false
    };

    // Diodes section
    pal_y += pal_h + 5;
    col = 0;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_DIODE, TOOL_COMPONENT, false, "Diode", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_ZENER, TOOL_COMPONENT, false, "Zener", false, false
    };
    col = 0;
    pal_y += pal_h + 5;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_SCHOTTKY, TOOL_COMPONENT, false, "Schky", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_LED, TOOL_COMPONENT, false, "LED", false, false
    };

    // Semiconductors section
    pal_y += pal_h + 20;
    col = 0;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_NPN_BJT, TOOL_COMPONENT, false, "NPN", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_PNP_BJT, TOOL_COMPONENT, false, "PNP", false, false
    };
    col = 0;
    pal_y += pal_h + 5;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_NMOS, TOOL_COMPONENT, false, "NMOS", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_PMOS, TOOL_COMPONENT, false, "PMOS", false, false
    };
    col = 0;
    pal_y += pal_h + 5;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_OPAMP, TOOL_COMPONENT, false, "OpAmp", false, false
    };

    // Circuit templates section
    pal_y += pal_h + 25;
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
    }

    if (circuit) {
        ui->node_count = circuit->num_nodes;
        ui->component_count = circuit->num_components;
    }
}

static void draw_button(SDL_Renderer *r, Button *btn) {
    // Background
    if (btn->pressed) {
        SDL_SetRenderDrawColor(r, 0x00, 0xd9, 0xff, 0xff);
    } else if (btn->hovered && btn->enabled) {
        SDL_SetRenderDrawColor(r, 0x0f, 0x34, 0x60, 0xff);
    } else {
        SDL_SetRenderDrawColor(r, 0x16, 0x21, 0x3e, 0xff);
    }
    SDL_Rect rect = {btn->bounds.x, btn->bounds.y, btn->bounds.w, btn->bounds.h};
    SDL_RenderFillRect(r, &rect);

    // Border
    if (btn->enabled) {
        SDL_SetRenderDrawColor(r, 0x00, 0xd9, 0xff, 0xff);
    } else {
        SDL_SetRenderDrawColor(r, 0x50, 0x50, 0x50, 0xff);
    }
    SDL_RenderDrawRect(r, &rect);

    // Label text (centered)
    if (btn->label) {
        int text_len = (int)strlen(btn->label);
        int text_x = btn->bounds.x + (btn->bounds.w - text_len * 8) / 2;
        int text_y = btn->bounds.y + (btn->bounds.h - 8) / 2;
        if (btn->enabled) {
            SDL_SetRenderDrawColor(r, 0xff, 0xff, 0xff, 0xff);
        } else {
            SDL_SetRenderDrawColor(r, 0x80, 0x80, 0x80, 0xff);
        }
        ui_draw_text(r, btn->label, text_x, text_y);
    }
}

static void draw_palette_item(SDL_Renderer *r, PaletteItem *item) {
    // Background
    if (item->selected) {
        SDL_SetRenderDrawColor(r, 0xe9, 0x45, 0x60, 0x40);
    } else if (item->hovered) {
        SDL_SetRenderDrawColor(r, 0x00, 0xd9, 0xff, 0x20);
    } else {
        SDL_SetRenderDrawColor(r, 0x0f, 0x34, 0x60, 0xff);
    }
    SDL_Rect rect = {item->bounds.x, item->bounds.y, item->bounds.w, item->bounds.h};
    SDL_RenderFillRect(r, &rect);

    // Border
    if (item->selected) {
        SDL_SetRenderDrawColor(r, 0xe9, 0x45, 0x60, 0xff);
    } else if (item->hovered) {
        SDL_SetRenderDrawColor(r, 0x00, 0xd9, 0xff, 0xff);
    } else {
        SDL_SetRenderDrawColor(r, 0x2a, 0x2a, 0x4e, 0xff);
    }
    SDL_RenderDrawRect(r, &rect);

    // Label text (centered)
    if (item->label) {
        int text_len = (int)strlen(item->label);
        int text_x = item->bounds.x + (item->bounds.w - text_len * 8) / 2;
        int text_y = item->bounds.y + (item->bounds.h - 8) / 2;
        if (item->selected) {
            SDL_SetRenderDrawColor(r, 0xff, 0xff, 0xff, 0xff);
        } else {
            SDL_SetRenderDrawColor(r, 0xc0, 0xc0, 0xc0, 0xff);
        }
        ui_draw_text(r, item->label, text_x, text_y);
    }
}

static void draw_circuit_item(SDL_Renderer *r, CircuitPaletteItem *item) {
    // Background - use a different color scheme for circuits (green tint)
    if (item->selected) {
        SDL_SetRenderDrawColor(r, 0x45, 0xe9, 0x60, 0x40);
    } else if (item->hovered) {
        SDL_SetRenderDrawColor(r, 0x00, 0xff, 0x88, 0x20);
    } else {
        SDL_SetRenderDrawColor(r, 0x0f, 0x34, 0x60, 0xff);
    }
    SDL_Rect rect = {item->bounds.x, item->bounds.y, item->bounds.w, item->bounds.h};
    SDL_RenderFillRect(r, &rect);

    // Border
    if (item->selected) {
        SDL_SetRenderDrawColor(r, 0x45, 0xe9, 0x60, 0xff);
    } else if (item->hovered) {
        SDL_SetRenderDrawColor(r, 0x00, 0xff, 0x88, 0xff);
    } else {
        SDL_SetRenderDrawColor(r, 0x2a, 0x4e, 0x2a, 0xff);
    }
    SDL_RenderDrawRect(r, &rect);

    // Label text (centered)
    if (item->label) {
        int text_len = (int)strlen(item->label);
        int text_x = item->bounds.x + (item->bounds.w - text_len * 8) / 2;
        int text_y = item->bounds.y + (item->bounds.h - 8) / 2;
        if (item->selected) {
            SDL_SetRenderDrawColor(r, 0xff, 0xff, 0xff, 0xff);
        } else {
            SDL_SetRenderDrawColor(r, 0x88, 0xff, 0x88, 0xff);
        }
        ui_draw_text(r, item->label, text_x, text_y);
    }
}

void ui_render_toolbar(UIState *ui, SDL_Renderer *renderer) {
    // Toolbar background
    SDL_SetRenderDrawColor(renderer, 0x16, 0x21, 0x3e, 0xff);
    SDL_Rect toolbar = {0, 0, ui->window_width, TOOLBAR_HEIGHT};
    SDL_RenderFillRect(renderer, &toolbar);

    // Title
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    ui_draw_text(renderer, "Circuit Playground", 10, 20);

    // Buttons
    draw_button(renderer, &ui->btn_run);
    draw_button(renderer, &ui->btn_pause);
    draw_button(renderer, &ui->btn_step);
    draw_button(renderer, &ui->btn_reset);
    draw_button(renderer, &ui->btn_clear);
    draw_button(renderer, &ui->btn_save);
    draw_button(renderer, &ui->btn_load);

    // Speed slider label
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
    ui_draw_text(renderer, "Speed:", ui->speed_slider.x, ui->speed_slider.y - 2);

    // Speed slider background
    int slider_x = ui->speed_slider.x + 50;
    SDL_SetRenderDrawColor(renderer, 0x0f, 0x34, 0x60, 0xff);
    SDL_Rect slider_bg = {slider_x, ui->speed_slider.y, ui->speed_slider.w, ui->speed_slider.h};
    SDL_RenderFillRect(renderer, &slider_bg);
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0x80);
    SDL_RenderDrawRect(renderer, &slider_bg);

    // Speed slider fill (logarithmic scale: 1x to 100x)
    // Map speed_value (1-100) to slider position
    float log_pos = (log10f(ui->speed_value) / 2.0f);  // log10(100) = 2
    int fill_w = (int)(ui->speed_slider.w * log_pos);
    fill_w = CLAMP(fill_w, 0, ui->speed_slider.w);
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    SDL_Rect slider_fill = {slider_x, ui->speed_slider.y, fill_w, ui->speed_slider.h};
    SDL_RenderFillRect(renderer, &slider_fill);

    // Speed value text
    char speed_text[16];
    if (ui->speed_value >= 10.0f) {
        snprintf(speed_text, sizeof(speed_text), "%.0fx", ui->speed_value);
    } else {
        snprintf(speed_text, sizeof(speed_text), "%.1fx", ui->speed_value);
    }
    SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);
    ui_draw_text(renderer, speed_text, slider_x + ui->speed_slider.w + 5, ui->speed_slider.y - 2);

    // Toolbar border
    SDL_SetRenderDrawColor(renderer, 0x0f, 0x34, 0x60, 0xff);
    SDL_RenderDrawLine(renderer, 0, TOOLBAR_HEIGHT - 1, ui->window_width, TOOLBAR_HEIGHT - 1);
}

void ui_render_palette(UIState *ui, SDL_Renderer *renderer) {
    // Palette background
    SDL_SetRenderDrawColor(renderer, 0x16, 0x21, 0x3e, 0xff);
    SDL_Rect palette = {0, TOOLBAR_HEIGHT, PALETTE_WIDTH, ui->window_height - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT};
    SDL_RenderFillRect(renderer, &palette);

    // Palette items
    for (int i = 0; i < ui->num_palette_items; i++) {
        draw_palette_item(renderer, &ui->palette_items[i]);
    }

    // Circuits section header
    if (ui->num_circuit_items > 0) {
        int header_y = ui->circuit_items[0].bounds.y - 18;
        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);
        ui_draw_text(renderer, "Circuits", 10, header_y);
    }

    // Circuit palette items
    for (int i = 0; i < ui->num_circuit_items; i++) {
        draw_circuit_item(renderer, &ui->circuit_items[i]);
    }

    // Border
    SDL_SetRenderDrawColor(renderer, 0x0f, 0x34, 0x60, 0xff);
    SDL_RenderDrawLine(renderer, PALETTE_WIDTH - 1, TOOLBAR_HEIGHT, PALETTE_WIDTH - 1, ui->window_height - STATUSBAR_HEIGHT);
}

// Helper to draw an editable property field
static void draw_property_field(SDL_Renderer *renderer, int x, int y, int w,
                                const char *label, const char *value,
                                bool is_editing, const char *edit_buffer, int cursor_pos) {
    // Label
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
    ui_draw_text(renderer, label, x, y + 2);

    // Value box
    int value_x = x + 90;
    int box_w = w - 90;
    SDL_Rect box = {value_x, y, box_w, 14};

    if (is_editing) {
        // Editing - dark background with cyan border
        SDL_SetRenderDrawColor(renderer, 0x10, 0x20, 0x30, 0xff);
        SDL_RenderFillRect(renderer, &box);
        SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
        SDL_RenderDrawRect(renderer, &box);

        // Draw input text
        SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
        ui_draw_text(renderer, edit_buffer, value_x + 2, y + 3);

        // Draw cursor
        int cursor_x = value_x + 2 + cursor_pos * 8;
        SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
        SDL_RenderDrawLine(renderer, cursor_x, y + 2, cursor_x, y + 12);
    } else {
        // Not editing - clickable field
        SDL_SetRenderDrawColor(renderer, 0x20, 0x30, 0x40, 0xff);
        SDL_RenderFillRect(renderer, &box);
        SDL_SetRenderDrawColor(renderer, 0x40, 0x50, 0x60, 0xff);
        SDL_RenderDrawRect(renderer, &box);

        // Draw value
        SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);
        ui_draw_text(renderer, value, value_x + 2, y + 3);
    }
}

void ui_render_properties(UIState *ui, SDL_Renderer *renderer, Component *selected, struct InputState *input) {
    int x = ui->window_width - ui->properties_width;
    int y = TOOLBAR_HEIGHT;

    // Draw resize handle on left edge
    SDL_SetRenderDrawColor(renderer, 0x40, 0x60, 0x80, 0xff);
    SDL_Rect resize_handle = {x - 3, y, 6, 200};
    SDL_RenderFillRect(renderer, &resize_handle);

    // Background
    SDL_SetRenderDrawColor(renderer, 0x16, 0x21, 0x3e, 0xff);
    SDL_Rect panel = {x, y, ui->properties_width, 200};
    SDL_RenderFillRect(renderer, &panel);

    // Title
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    ui_draw_text(renderer, "Properties", x + 10, y + 10);

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
        ui_draw_text(renderer, "Component:", x + 10, y + 35);

        // Component type name (must match ComponentType enum order)
        const char *type_names[] = {
            "None", "Ground", "DC Voltage", "AC Voltage", "DC Current",
            "Resistor", "Capacitor", "Elec. Cap", "Inductor", "Diode",
            "Zener", "Schottky", "LED",
            "NPN BJT", "PNP BJT", "NMOS", "PMOS", "Op-Amp",
            "Square Wave", "Triangle Wave", "Sawtooth", "Noise"
        };
        if (selected->type < COMP_TYPE_COUNT) {
            SDL_SetRenderDrawColor(renderer, 0xc0, 0xc0, 0xc0, 0xff);
            ui_draw_text(renderer, type_names[selected->type], x + 100, y + 35);
        }

        // Store property bounds for later reference
        ui->num_properties = 0;

        // Show component properties with clickable fields
        int prop_y = y + 55;
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
                }
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
                }
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
                }
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
                ui->properties[0].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[0].prop_type = PROP_VALUE;

                prop_y += 18;
                snprintf(buf, sizeof(buf), "%.3g Hz", selected->props.square_wave.frequency);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Frequency:", buf,
                                   editing_freq, edit_buf, cursor);
                ui->properties[1].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[1].prop_type = PROP_FREQUENCY;

                prop_y += 18;
                snprintf(buf, sizeof(buf), "%.0f %%", selected->props.square_wave.duty * 100);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Duty:", buf,
                                   editing_duty, edit_buf, cursor);
                ui->properties[2].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[2].prop_type = PROP_DUTY;
                ui->num_properties = 3;
                break;

            case COMP_TRIANGLE_WAVE:
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.triangle_wave.amplitude);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Amplitude:", buf,
                                   editing_value, edit_buf, cursor);
                ui->properties[0].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[0].prop_type = PROP_VALUE;

                prop_y += 18;
                snprintf(buf, sizeof(buf), "%.3g Hz", selected->props.triangle_wave.frequency);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Frequency:", buf,
                                   editing_freq, edit_buf, cursor);
                ui->properties[1].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[1].prop_type = PROP_FREQUENCY;
                ui->num_properties = 2;
                break;

            case COMP_SAWTOOTH_WAVE:
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.sawtooth_wave.amplitude);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Amplitude:", buf,
                                   editing_value, edit_buf, cursor);
                ui->properties[0].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[0].prop_type = PROP_VALUE;

                prop_y += 18;
                snprintf(buf, sizeof(buf), "%.3g Hz", selected->props.sawtooth_wave.frequency);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Frequency:", buf,
                                   editing_freq, edit_buf, cursor);
                ui->properties[1].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[1].prop_type = PROP_FREQUENCY;
                ui->num_properties = 2;
                break;

            case COMP_NOISE_SOURCE:
                snprintf(buf, sizeof(buf), "%.3g V", selected->props.noise_source.amplitude);
                draw_property_field(renderer, x + 10, prop_y, prop_w, "Amplitude:", buf,
                                   editing_value, edit_buf, cursor);
                ui->properties[ui->num_properties].bounds = (Rect){x + 100, prop_y, prop_w - 90, 14};
                ui->properties[ui->num_properties].prop_type = PROP_VALUE;
                ui->num_properties++;
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

            default:
                break;
        }

        // Help text
        prop_y += 25;
        SDL_SetRenderDrawColor(renderer, 0x60, 0x60, 0x60, 0xff);
        ui_draw_text(renderer, "Click value to edit", x + 10, prop_y);
        ui_draw_text(renderer, "Use k,M,m,u,n,p suffix", x + 10, prop_y + 12);
    } else {
        // Reset num_properties when nothing is selected to avoid stale bounds
        ui->num_properties = 0;

        SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
        ui_draw_text(renderer, "No selection", x + 10, y + 35);
        ui_draw_text(renderer, "Click component", x + 10, y + 55);
        ui_draw_text(renderer, "to edit properties", x + 10, y + 70);
    }

    // Border
    SDL_SetRenderDrawColor(renderer, 0x0f, 0x34, 0x60, 0xff);
    SDL_RenderDrawLine(renderer, x, y, x, ui->window_height - STATUSBAR_HEIGHT);
}

void ui_render_measurements(UIState *ui, SDL_Renderer *renderer, Simulation *sim) {
    int x = ui->window_width - ui->properties_width;
    int y = TOOLBAR_HEIGHT + 210;

    // Background
    SDL_SetRenderDrawColor(renderer, 0x16, 0x21, 0x3e, 0xff);
    SDL_Rect panel = {x, y, ui->properties_width, 180};
    SDL_RenderFillRect(renderer, &panel);

    // Title
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    ui_draw_text(renderer, "Measurements", x + 10, y + 10);

    // Voltmeter
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
    ui_draw_text(renderer, "Voltmeter:", x + 10, y + 35);
    char volt_str[32];
    snprintf(volt_str, sizeof(volt_str), "%.3f V", ui->voltmeter_value);
    SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);
    ui_draw_text(renderer, volt_str, x + 100, y + 35);

    // Ammeter
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
    ui_draw_text(renderer, "Ammeter:", x + 10, y + 55);
    char amp_str[32];
    snprintf(amp_str, sizeof(amp_str), "%.3f mA", ui->ammeter_value * 1000.0);
    SDL_SetRenderDrawColor(renderer, 0xff, 0xaa, 0x00, 0xff);
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
                if (display_samples > MAX_HISTORY / 2) display_samples = MAX_HISTORY / 2;

                // Capture data: start some samples before trigger for pre-trigger view
                // Put trigger point at about 20% from left edge
                int pre_trigger = display_samples / 5;
                int capture_start = trigger_idx - pre_trigger;
                if (capture_start < 0) capture_start = 0;
                int capture_end = capture_start + display_samples;
                if (capture_end > trig_count) capture_end = trig_count;

                ui->scope_capture_count = capture_end - capture_start;

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

                    int capture_start = trig_count - display_samples;
                    if (capture_start < 0) capture_start = 0;

                    ui->scope_capture_count = trig_count - capture_start;

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

    // Show trigger level next to mode button
    if (ui->trigger_mode != TRIG_AUTO) {
        int trig_info_x = ui->btn_scope_mode.bounds.x + ui->btn_scope_mode.bounds.w + 10;
        int trig_info_y = ui->btn_scope_mode.bounds.y + 6;
        SDL_SetRenderDrawColor(renderer, 0xff, 0x80, 0x00, 0xff);  // Orange
        snprintf(buf, sizeof(buf), "T:%.1fV", ui->trigger_level);
        ui_draw_text(renderer, buf, trig_info_x, trig_info_y);
    }

    // Display settings panel below buttons (3 rows of buttons = ~79px, so start at +85)
    int info_y = r->y + r->h + 85;

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

    // Display waveform measurements panel (right side of scope)
    if (analysis && ui->scope_num_channels > 0) {
        int meas_x = r->x + r->w + 15;
        int meas_y = r->y;

        // Measurements header
        SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
        ui_draw_text(renderer, "MEASUREMENTS", meas_x, meas_y);
        meas_y += 18;

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
            meas_y += 14;

            // Measurements in gray
            SDL_SetRenderDrawColor(renderer, 0x90, 0x90, 0x90, 0xff);

            // Vpp
            if (m->v_pp < 1.0) {
                snprintf(buf, sizeof(buf), "Vpp: %.1fmV", m->v_pp * 1000);
            } else {
                snprintf(buf, sizeof(buf), "Vpp: %.2fV", m->v_pp);
            }
            ui_draw_text(renderer, buf, meas_x + 8, meas_y);
            meas_y += 12;

            // Vrms
            if (m->v_rms < 1.0) {
                snprintf(buf, sizeof(buf), "Vrms:%.1fmV", m->v_rms * 1000);
            } else {
                snprintf(buf, sizeof(buf), "Vrms:%.2fV", m->v_rms);
            }
            ui_draw_text(renderer, buf, meas_x + 8, meas_y);
            meas_y += 12;

            // Vavg (DC offset)
            if (fabs(m->v_avg) < 1.0) {
                snprintf(buf, sizeof(buf), "Vavg:%.1fmV", m->v_avg * 1000);
            } else {
                snprintf(buf, sizeof(buf), "Vavg:%.2fV", m->v_avg);
            }
            ui_draw_text(renderer, buf, meas_x + 8, meas_y);
            meas_y += 12;

            // Frequency
            if (m->frequency > 0) {
                if (m->frequency >= 1000) {
                    snprintf(buf, sizeof(buf), "Freq:%.2fkHz", m->frequency / 1000);
                } else {
                    snprintf(buf, sizeof(buf), "Freq:%.1fHz", m->frequency);
                }
                ui_draw_text(renderer, buf, meas_x + 8, meas_y);
                meas_y += 12;

                // Period
                if (m->period < 0.001) {
                    snprintf(buf, sizeof(buf), "T: %.1fus", m->period * 1e6);
                } else if (m->period < 1.0) {
                    snprintf(buf, sizeof(buf), "T: %.2fms", m->period * 1000);
                } else {
                    snprintf(buf, sizeof(buf), "T: %.2fs", m->period);
                }
                ui_draw_text(renderer, buf, meas_x + 8, meas_y);
                meas_y += 12;

                // Duty cycle
                snprintf(buf, sizeof(buf), "Duty:%.1f%%", m->duty_cycle);
                ui_draw_text(renderer, buf, meas_x + 8, meas_y);
                meas_y += 12;
            }

            // Rise/fall time if measured
            if (m->rise_time > 0) {
                if (m->rise_time < 0.001) {
                    snprintf(buf, sizeof(buf), "Rise:%.1fus", m->rise_time * 1e6);
                } else {
                    snprintf(buf, sizeof(buf), "Rise:%.2fms", m->rise_time * 1000);
                }
                ui_draw_text(renderer, buf, meas_x + 8, meas_y);
                meas_y += 12;
            }
            if (m->fall_time > 0) {
                if (m->fall_time < 0.001) {
                    snprintf(buf, sizeof(buf), "Fall:%.1fus", m->fall_time * 1e6);
                } else {
                    snprintf(buf, sizeof(buf), "Fall:%.2fms", m->fall_time * 1000);
                }
                ui_draw_text(renderer, buf, meas_x + 8, meas_y);
                meas_y += 12;
            }

            meas_y += 6;  // Space between channels
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
    SDL_Rect panel = {r->x - 10, r->y - 25, r->w + 20, r->h + 120};
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
    if (mid_freq >= 1000) {
        snprintf(buf, sizeof(buf), "%.1fkHz", mid_freq / 1000);
    } else {
        snprintf(buf, sizeof(buf), "%.0fHz", mid_freq);
    }
    ui_draw_text(renderer, buf, r->x + r->w/2 - 20, label_y);

    if (ui->bode_freq_stop >= 1000) {
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

    // Close button hint
    ui_draw_text(renderer, "[ESC to close]", r->x + r->w - 100, legend_y + 15);
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

void ui_render_monte_carlo_panel(UIState *ui, SDL_Renderer *renderer, void *analysis_ptr) {
    if (!ui || !renderer || !ui->show_monte_carlo_panel) return;

    AnalysisState *analysis = (AnalysisState *)analysis_ptr;
    char buf[64];

    // Panel dimensions
    int panel_w = 350;
    int panel_h = 300;
    int panel_x = (ui->window_width - panel_w) / 2;
    int panel_y = (ui->window_height - panel_h) / 2;

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
    int value_x = panel_x + 150;

    SDL_SetRenderDrawColor(renderer, 0x90, 0x90, 0x90, 0xff);

    // Number of runs
    ui_draw_text(renderer, "Runs:", label_x, y);
    snprintf(buf, sizeof(buf), "%d", ui->monte_carlo_runs);
    ui_draw_text(renderer, buf, value_x, y);
    y += 20;

    // Tolerance
    ui_draw_text(renderer, "Tolerance:", label_x, y);
    snprintf(buf, sizeof(buf), "%.1f%%", ui->monte_carlo_tolerance);
    ui_draw_text(renderer, buf, value_x, y);
    y += 25;

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

    // Background
    SDL_SetRenderDrawColor(renderer, 0x0f, 0x34, 0x60, 0xff);
    SDL_Rect bar = {0, y, ui->window_width, STATUSBAR_HEIGHT};
    SDL_RenderFillRect(renderer, &bar);

    // Status message
    SDL_SetRenderDrawColor(renderer, 0xb0, 0xb0, 0xb0, 0xff);
    if (ui->status_message[0]) {
        ui_draw_text(renderer, ui->status_message, 10, y + 8);
    } else {
        ui_draw_text(renderer, "Ready - Press F1 for help", 10, y + 8);
    }

    // Time display
    char time_str[32];
    snprintf(time_str, sizeof(time_str), "t=%.3fs", ui->sim_time);
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    ui_draw_text(renderer, time_str, ui->window_width - 250, y + 8);

    // Component/Node counts
    char count_str[32];
    snprintf(count_str, sizeof(count_str), "C:%d N:%d", ui->component_count, ui->node_count);
    SDL_SetRenderDrawColor(renderer, 0xb0, 0xb0, 0xb0, 0xff);
    ui_draw_text(renderer, count_str, ui->window_width - 120, y + 8);
}

void ui_render_shortcuts_dialog(UIState *ui, SDL_Renderer *renderer) {
    if (!ui->show_shortcuts_dialog) return;

    // Semi-transparent overlay
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xb0);
    SDL_Rect overlay = {0, 0, ui->window_width, ui->window_height};
    SDL_RenderFillRect(renderer, &overlay);

    // Dialog box
    int dw = 350, dh = 320;
    int dx = (ui->window_width - dw) / 2;
    int dy = (ui->window_height - dh) / 2;

    SDL_SetRenderDrawColor(renderer, 0x16, 0x21, 0x3e, 0xff);
    SDL_Rect dialog = {dx, dy, dw, dh};
    SDL_RenderFillRect(renderer, &dialog);

    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    SDL_RenderDrawRect(renderer, &dialog);

    // Title
    ui_draw_text(renderer, "Keyboard Shortcuts", dx + 20, dy + 15);

    // Shortcuts list
    SDL_SetRenderDrawColor(renderer, 0xc0, 0xc0, 0xc0, 0xff);
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

    SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
    line_y += 10;
    ui_draw_text(renderer, "Press Escape or F1 to close", dx + 20, line_y);

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
        // Release cursor drag
        if (ui->scope_cursor_drag != 0) {
            ui->scope_cursor_drag = 0;
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

        // Check property fields for click-to-edit
        // Each property field stores its prop_type directly
        for (int i = 0; i < ui->num_properties && i < 16; i++) {
            if (point_in_rect(x, y, &ui->properties[i].bounds)) {
                return UI_ACTION_PROP_EDIT + ui->properties[i].prop_type;
            }
        }

        // Check palette items
        for (int i = 0; i < ui->num_palette_items; i++) {
            if (point_in_rect(x, y, &ui->palette_items[i].bounds)) {
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

        // Check circuit template items
        for (int i = 0; i < ui->num_circuit_items; i++) {
            if (point_in_rect(x, y, &ui->circuit_items[i].bounds)) {
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

    // Update palette hover states
    for (int i = 0; i < ui->num_palette_items; i++) {
        ui->palette_items[i].hovered = point_in_rect(x, y, &ui->palette_items[i].bounds);
    }

    // Update circuit items hover states
    for (int i = 0; i < ui->num_circuit_items; i++) {
        ui->circuit_items[i].hovered = point_in_rect(x, y, &ui->circuit_items[i].bounds);
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

    // Update oscilloscope position (anchored to right side, vertically positioned based on height)
    ui->scope_rect.x = ui->window_width - ui->properties_width + 10;
    // Keep y position reasonable or adjust if window is too small
    int max_scope_y = ui->window_height - STATUSBAR_HEIGHT - ui->scope_rect.h - 100;
    if (ui->scope_rect.y > max_scope_y && max_scope_y > TOOLBAR_HEIGHT + 200) {
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
