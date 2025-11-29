/**
 * Circuit Playground - UI System Implementation
 */

#include <stdio.h>
#include <string.h>
#include "ui.h"

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
        {10 + col*70, pal_y, 60, pal_h}, COMP_INDUCTOR, TOOL_COMPONENT, false, "L", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_DIODE, TOOL_COMPONENT, false, "D", false, false
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

    // Oscilloscope settings
    ui->scope_rect = (Rect){WINDOW_WIDTH - PROPERTIES_WIDTH + 10, 400, 260, 180};
    ui->scope_channels[0] = (ScopeChannel){true, {0xff, 0xff, 0x00, 0xff}, 0, 0};
    ui->scope_channels[1] = (ScopeChannel){false, {0x00, 0xff, 0xff, 0xff}, 1, 0};
    ui->scope_time_div = 0.001;
    ui->scope_volt_div = 1.0;

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

void ui_render_toolbar(UIState *ui, SDL_Renderer *renderer) {
    // Toolbar background
    SDL_SetRenderDrawColor(renderer, 0x16, 0x21, 0x3e, 0xff);
    SDL_Rect toolbar = {0, 0, WINDOW_WIDTH, TOOLBAR_HEIGHT};
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

    // Speed slider background
    SDL_SetRenderDrawColor(renderer, 0x0f, 0x34, 0x60, 0xff);
    SDL_Rect slider_bg = {ui->speed_slider.x, ui->speed_slider.y, ui->speed_slider.w, ui->speed_slider.h};
    SDL_RenderFillRect(renderer, &slider_bg);

    // Speed slider fill
    int fill_w = (int)(ui->speed_slider.w * (ui->speed_value / 100.0f));
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    SDL_Rect slider_fill = {ui->speed_slider.x, ui->speed_slider.y, fill_w, ui->speed_slider.h};
    SDL_RenderFillRect(renderer, &slider_fill);

    // Toolbar border
    SDL_SetRenderDrawColor(renderer, 0x0f, 0x34, 0x60, 0xff);
    SDL_RenderDrawLine(renderer, 0, TOOLBAR_HEIGHT - 1, WINDOW_WIDTH, TOOLBAR_HEIGHT - 1);
}

void ui_render_palette(UIState *ui, SDL_Renderer *renderer) {
    // Palette background
    SDL_SetRenderDrawColor(renderer, 0x16, 0x21, 0x3e, 0xff);
    SDL_Rect palette = {0, TOOLBAR_HEIGHT, PALETTE_WIDTH, WINDOW_HEIGHT - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT};
    SDL_RenderFillRect(renderer, &palette);

    // Palette items
    for (int i = 0; i < ui->num_palette_items; i++) {
        draw_palette_item(renderer, &ui->palette_items[i]);
    }

    // Border
    SDL_SetRenderDrawColor(renderer, 0x0f, 0x34, 0x60, 0xff);
    SDL_RenderDrawLine(renderer, PALETTE_WIDTH - 1, TOOLBAR_HEIGHT, PALETTE_WIDTH - 1, WINDOW_HEIGHT - STATUSBAR_HEIGHT);
}

void ui_render_properties(UIState *ui, SDL_Renderer *renderer, Component *selected) {
    int x = WINDOW_WIDTH - PROPERTIES_WIDTH;
    int y = TOOLBAR_HEIGHT;

    // Background
    SDL_SetRenderDrawColor(renderer, 0x16, 0x21, 0x3e, 0xff);
    SDL_Rect panel = {x, y, PROPERTIES_WIDTH, 200};
    SDL_RenderFillRect(renderer, &panel);

    // Title
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    ui_draw_text(renderer, "Properties", x + 10, y + 10);

    // Show selected component info
    if (selected) {
        SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
        ui_draw_text(renderer, "Component:", x + 10, y + 35);

        // Component type name
        const char *type_names[] = {
            "None", "Ground", "DC Voltage", "AC Voltage", "DC Current",
            "Resistor", "Capacitor", "Inductor", "Diode",
            "NPN BJT", "PNP BJT", "NMOS", "PMOS", "Op-Amp"
        };
        if (selected->type < COMP_TYPE_COUNT) {
            SDL_SetRenderDrawColor(renderer, 0xc0, 0xc0, 0xc0, 0xff);
            ui_draw_text(renderer, type_names[selected->type], x + 100, y + 35);
        }

        // Show component properties
        int prop_y = y + 55;
        char buf[64];
        SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);

        switch (selected->type) {
            case COMP_DC_VOLTAGE:
                ui_draw_text(renderer, "Voltage:", x + 10, prop_y);
                snprintf(buf, sizeof(buf), "%.2f V", selected->props.dc_voltage.voltage);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);
                ui_draw_text(renderer, buf, x + 100, prop_y);
                SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                ui_draw_text(renderer, "[+/-] to adjust", x + 10, prop_y + 20);
                break;

            case COMP_AC_VOLTAGE:
                ui_draw_text(renderer, "Amplitude:", x + 10, prop_y);
                snprintf(buf, sizeof(buf), "%.2f V", selected->props.ac_voltage.amplitude);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);
                ui_draw_text(renderer, buf, x + 100, prop_y);

                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Frequency:", x + 10, prop_y + 18);
                snprintf(buf, sizeof(buf), "%.1f Hz", selected->props.ac_voltage.frequency);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);
                ui_draw_text(renderer, buf, x + 100, prop_y + 18);

                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Phase:", x + 10, prop_y + 36);
                snprintf(buf, sizeof(buf), "%.1f deg", selected->props.ac_voltage.phase);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);
                ui_draw_text(renderer, buf, x + 100, prop_y + 36);

                SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
                ui_draw_text(renderer, "Offset:", x + 10, prop_y + 54);
                snprintf(buf, sizeof(buf), "%.2f V", selected->props.ac_voltage.offset);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);
                ui_draw_text(renderer, buf, x + 100, prop_y + 54);

                SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                ui_draw_text(renderer, "[+/-] amplitude", x + 10, prop_y + 75);
                ui_draw_text(renderer, "[F/f] frequency", x + 10, prop_y + 90);
                break;

            case COMP_DC_CURRENT:
                ui_draw_text(renderer, "Current:", x + 10, prop_y);
                snprintf(buf, sizeof(buf), "%.3f mA", selected->props.dc_current.current * 1000.0);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);
                ui_draw_text(renderer, buf, x + 100, prop_y);
                SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                ui_draw_text(renderer, "[+/-] to adjust", x + 10, prop_y + 20);
                break;

            case COMP_RESISTOR:
                ui_draw_text(renderer, "Resistance:", x + 10, prop_y);
                if (selected->props.resistor.resistance >= 1e6)
                    snprintf(buf, sizeof(buf), "%.2f MOhm", selected->props.resistor.resistance / 1e6);
                else if (selected->props.resistor.resistance >= 1e3)
                    snprintf(buf, sizeof(buf), "%.2f kOhm", selected->props.resistor.resistance / 1e3);
                else
                    snprintf(buf, sizeof(buf), "%.1f Ohm", selected->props.resistor.resistance);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);
                ui_draw_text(renderer, buf, x + 100, prop_y);
                SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                ui_draw_text(renderer, "[+/-] to adjust", x + 10, prop_y + 20);
                break;

            case COMP_CAPACITOR:
                ui_draw_text(renderer, "Capacitance:", x + 10, prop_y);
                if (selected->props.capacitor.capacitance >= 1e-3)
                    snprintf(buf, sizeof(buf), "%.2f mF", selected->props.capacitor.capacitance * 1e3);
                else if (selected->props.capacitor.capacitance >= 1e-6)
                    snprintf(buf, sizeof(buf), "%.2f uF", selected->props.capacitor.capacitance * 1e6);
                else if (selected->props.capacitor.capacitance >= 1e-9)
                    snprintf(buf, sizeof(buf), "%.2f nF", selected->props.capacitor.capacitance * 1e9);
                else
                    snprintf(buf, sizeof(buf), "%.2f pF", selected->props.capacitor.capacitance * 1e12);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);
                ui_draw_text(renderer, buf, x + 100, prop_y);
                SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                ui_draw_text(renderer, "[+/-] to adjust", x + 10, prop_y + 20);
                break;

            case COMP_INDUCTOR:
                ui_draw_text(renderer, "Inductance:", x + 10, prop_y);
                if (selected->props.inductor.inductance >= 1.0)
                    snprintf(buf, sizeof(buf), "%.2f H", selected->props.inductor.inductance);
                else if (selected->props.inductor.inductance >= 1e-3)
                    snprintf(buf, sizeof(buf), "%.2f mH", selected->props.inductor.inductance * 1e3);
                else
                    snprintf(buf, sizeof(buf), "%.2f uH", selected->props.inductor.inductance * 1e6);
                SDL_SetRenderDrawColor(renderer, 0x00, 0xff, 0x88, 0xff);
                ui_draw_text(renderer, buf, x + 100, prop_y);
                SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
                ui_draw_text(renderer, "[+/-] to adjust", x + 10, prop_y + 20);
                break;

            default:
                break;
        }
    } else {
        SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
        ui_draw_text(renderer, "No selection", x + 10, y + 35);
        ui_draw_text(renderer, "Click component", x + 10, y + 55);
        ui_draw_text(renderer, "to edit properties", x + 10, y + 70);
    }

    // Border
    SDL_SetRenderDrawColor(renderer, 0x0f, 0x34, 0x60, 0xff);
    SDL_RenderDrawLine(renderer, x, y, x, WINDOW_HEIGHT - STATUSBAR_HEIGHT);
}

void ui_render_measurements(UIState *ui, SDL_Renderer *renderer, Simulation *sim) {
    int x = WINDOW_WIDTH - PROPERTIES_WIDTH;
    int y = TOOLBAR_HEIGHT + 210;

    // Background
    SDL_SetRenderDrawColor(renderer, 0x16, 0x21, 0x3e, 0xff);
    SDL_Rect panel = {x, y, PROPERTIES_WIDTH, 180};
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

void ui_render_oscilloscope(UIState *ui, SDL_Renderer *renderer, Simulation *sim) {
    Rect *r = &ui->scope_rect;

    // Background (black)
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
    SDL_Rect bg = {r->x, r->y, r->w, r->h};
    SDL_RenderFillRect(renderer, &bg);

    // Grid
    SDL_SetRenderDrawColor(renderer, 0x33, 0x33, 0x33, 0xff);
    int div_x = r->w / 10;
    int div_y = r->h / 8;
    for (int i = 0; i <= 10; i++) {
        SDL_RenderDrawLine(renderer, r->x + i * div_x, r->y, r->x + i * div_x, r->y + r->h);
    }
    for (int i = 0; i <= 8; i++) {
        SDL_RenderDrawLine(renderer, r->x, r->y + i * div_y, r->x + r->w, r->y + i * div_y);
    }

    // Center lines (brighter)
    SDL_SetRenderDrawColor(renderer, 0x55, 0x55, 0x55, 0xff);
    SDL_RenderDrawLine(renderer, r->x + r->w / 2, r->y, r->x + r->w / 2, r->y + r->h);
    SDL_RenderDrawLine(renderer, r->x, r->y + r->h / 2, r->x + r->w, r->y + r->h / 2);

    // Draw traces if simulation has data
    if (sim && sim->history_count > 1) {
        for (int ch = 0; ch < 2; ch++) {
            if (!ui->scope_channels[ch].enabled) continue;

            SDL_SetRenderDrawColor(renderer,
                ui->scope_channels[ch].color.r,
                ui->scope_channels[ch].color.g,
                ui->scope_channels[ch].color.b, 0xff);

            double times[MAX_HISTORY];
            double values[MAX_HISTORY];
            int count = simulation_get_history(sim, ch, times, values, r->w);

            int center_y = r->y + r->h / 2;
            double scale = (r->h / 8.0) / ui->scope_volt_div;

            for (int i = 1; i < count; i++) {
                int x1 = r->x + (i - 1);
                int x2 = r->x + i;
                int y1 = center_y - (int)(values[i-1] * scale);
                int y2 = center_y - (int)(values[i] * scale);
                y1 = CLAMP(y1, r->y, r->y + r->h);
                y2 = CLAMP(y2, r->y, r->y + r->h);
                SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
            }
        }
    }

    // Border
    SDL_SetRenderDrawColor(renderer, 0x0f, 0x34, 0x60, 0xff);
    SDL_RenderDrawRect(renderer, &bg);
}

void ui_render_statusbar(UIState *ui, SDL_Renderer *renderer) {
    int y = WINDOW_HEIGHT - STATUSBAR_HEIGHT;

    // Background
    SDL_SetRenderDrawColor(renderer, 0x0f, 0x34, 0x60, 0xff);
    SDL_Rect bar = {0, y, WINDOW_WIDTH, STATUSBAR_HEIGHT};
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
    ui_draw_text(renderer, time_str, WINDOW_WIDTH - 250, y + 8);

    // Component/Node counts
    char count_str[32];
    snprintf(count_str, sizeof(count_str), "C:%d N:%d", ui->component_count, ui->node_count);
    SDL_SetRenderDrawColor(renderer, 0xb0, 0xb0, 0xb0, 0xff);
    ui_draw_text(renderer, count_str, WINDOW_WIDTH - 120, y + 8);
}

void ui_render_shortcuts_dialog(UIState *ui, SDL_Renderer *renderer) {
    if (!ui->show_shortcuts_dialog) return;

    // Semi-transparent overlay
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xb0);
    SDL_Rect overlay = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(renderer, &overlay);

    // Dialog box
    int dw = 350, dh = 320;
    int dx = (WINDOW_WIDTH - dw) / 2;
    int dy = (WINDOW_HEIGHT - dh) / 2;

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
    ui_draw_text(renderer, "G         - Toggle grid", dx + 20, line_y); line_y += line_h;
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

        // Check palette items
        for (int i = 0; i < ui->num_palette_items; i++) {
            if (point_in_rect(x, y, &ui->palette_items[i].bounds)) {
                // Deselect all
                for (int j = 0; j < ui->num_palette_items; j++) {
                    ui->palette_items[j].selected = false;
                }
                ui->palette_items[i].selected = true;
                ui->selected_palette_idx = i;

                if (ui->palette_items[i].is_tool) {
                    return UI_ACTION_SELECT_TOOL + ui->palette_items[i].tool_type;
                } else {
                    return UI_ACTION_SELECT_COMP + ui->palette_items[i].comp_type;
                }
            }
        }
    } else {
        ui->btn_run.pressed = false;
    }

    return UI_ACTION_NONE;
}

int ui_handle_motion(UIState *ui, int x, int y) {
    if (!ui) return UI_ACTION_NONE;

    // Update button hover states
    ui->btn_run.hovered = point_in_rect(x, y, &ui->btn_run.bounds);
    ui->btn_pause.hovered = point_in_rect(x, y, &ui->btn_pause.bounds);
    ui->btn_step.hovered = point_in_rect(x, y, &ui->btn_step.bounds);
    ui->btn_reset.hovered = point_in_rect(x, y, &ui->btn_reset.bounds);
    ui->btn_clear.hovered = point_in_rect(x, y, &ui->btn_clear.bounds);
    ui->btn_save.hovered = point_in_rect(x, y, &ui->btn_save.bounds);
    ui->btn_load.hovered = point_in_rect(x, y, &ui->btn_load.bounds);

    // Update palette hover states
    for (int i = 0; i < ui->num_palette_items; i++) {
        ui->palette_items[i].hovered = point_in_rect(x, y, &ui->palette_items[i].bounds);
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
