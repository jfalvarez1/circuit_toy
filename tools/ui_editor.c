/**
 * Circuit Playground - UI Layout Editor
 *
 * Pixel-accurate visual representation of the main application UI.
 * Shows exact positions of all UI elements for reference.
 * Drag and drop elements, resize panels, and save positions to JSON.
 */

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

// Window dimensions - EXACTLY matching the main app from types.h
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define TOOLBAR_HEIGHT 50
#define PALETTE_WIDTH 160
#define PROPERTIES_WIDTH 420
#define STATUSBAR_HEIGHT 24

// Derived values
#define CANVAS_X PALETTE_WIDTH
#define CANVAS_Y TOOLBAR_HEIGHT
#define CANVAS_WIDTH (WINDOW_WIDTH - PALETTE_WIDTH - PROPERTIES_WIDTH)
#define CANVAS_HEIGHT (WINDOW_HEIGHT - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT)
#define PROPS_X (WINDOW_WIDTH - PROPERTIES_WIDTH)

// Synthwave colors - exact match from types.h
#define SYNTH_BG_DARK_R 26
#define SYNTH_BG_DARK_G 26
#define SYNTH_BG_DARK_B 46

#define SYNTH_BG_MID_R 40
#define SYNTH_BG_MID_G 42
#define SYNTH_BG_MID_B 54

#define SYNTH_PINK_R 255
#define SYNTH_PINK_G 0
#define SYNTH_PINK_B 128

#define SYNTH_CYAN_R 0
#define SYNTH_CYAN_G 255
#define SYNTH_CYAN_B 255

#define SYNTH_PURPLE_R 139
#define SYNTH_PURPLE_G 92
#define SYNTH_PURPLE_B 246

#define SYNTH_YELLOW_R 255
#define SYNTH_YELLOW_G 255
#define SYNTH_YELLOW_B 0

#define SYNTH_ORANGE_R 255
#define SYNTH_ORANGE_G 165
#define SYNTH_ORANGE_B 0

#define SYNTH_GREEN_R 0
#define SYNTH_GREEN_G 255
#define SYNTH_GREEN_B 128

// 8x8 bitmap font (same as real app)
static const unsigned char font8x8[95][8] = {
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

// Draw a character
static void draw_char(SDL_Renderer *r, char c, int x, int y) {
    if (c < 32 || c > 126) c = '?';
    const unsigned char *glyph = font8x8[c - 32];
    for (int row = 0; row < 8; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (1 << col)) {
                SDL_RenderDrawPoint(r, x + col, y + row);
            }
        }
    }
}

// Draw text string
static void draw_text(SDL_Renderer *r, const char *text, int x, int y) {
    if (!text) return;
    while (*text) {
        draw_char(r, *text, x, y);
        x += 8;
        text++;
    }
}

// Set color helper
static void set_rgb(SDL_Renderer *r, int red, int green, int blue) {
    SDL_SetRenderDrawColor(r, red, green, blue, 255);
}

// Draw filled rect
static void fill_rect(SDL_Renderer *r, int x, int y, int w, int h) {
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}

// Draw outlined rect
static void draw_rect(SDL_Renderer *r, int x, int y, int w, int h) {
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderDrawRect(r, &rect);
}

// Palette category names (exact from ui.c)
static const char *category_names[] = {
    "Tools", "Sources", "Waveforms", "Passives", "Diodes", "BJT", "FET",
    "Thyristors", "Op-Amps", "Ctrl Sources", "Switches", "Transformers",
    "Logic Gates", "Digital ICs", "Mixed Signal", "Regulators", "Display",
    "Measurement", "Circuits"
};
#define NUM_CATEGORIES 19

// Component items per category (sample - first few)
static const char *tools_items[] = {"Select", "Wire", "Delete", "Probe", "Text"};
static const char *sources_items[] = {"GND", "DC V", "AC V", "DC I", "AC I", "Clock"};
static const char *passives_items[] = {"R", "C", "Elec", "L", "Pot", "Xtal", "Fuse"};
static const char *diodes_items[] = {"Diode", "Zener", "Schky", "LED"};

// Toolbar button names
static const char *toolbar_buttons[] = {"Run", "Pause", "Step", "Reset", "Clear", "Save", "Load", "SVG"};
#define NUM_TOOLBAR_BUTTONS 8

// Element types for the layout editor
typedef enum {
    ELEM_TOOLBAR,
    ELEM_PALETTE,
    ELEM_CANVAS,
    ELEM_OSCILLOSCOPE,
    ELEM_PROPERTIES,
    ELEM_STATUSBAR,
    ELEM_SCOPE_CONTROLS,
    ELEM_SCOPE_CHANNELS,
    ELEM_TYPE_COUNT
} ElementType;

typedef struct {
    int id;
    ElementType type;
    char name[64];
    int x, y;
    int width, height;
    bool visible;
    bool locked;
    bool resizable;
} UIElement;

#define MAX_ELEMENTS 16

typedef struct {
    UIElement elements[MAX_ELEMENTS];
    int num_elements;
    int selected_id;
    bool dragging;
    bool resizing;
    int resize_edge;
    int drag_offset_x, drag_offset_y;
    int mouse_x, mouse_y;
    bool show_grid;
    int grid_size;
    bool snap_to_grid;
    char filename[256];
    bool modified;
    int palette_scroll;  // Scroll offset for palette
} EditorState;

// Initialize editor with default layout
static void editor_init(EditorState *state) {
    memset(state, 0, sizeof(EditorState));
    state->selected_id = -1;
    state->show_grid = false;  // Grid off by default for cleaner view
    state->grid_size = 10;
    state->snap_to_grid = true;
    snprintf(state->filename, sizeof(state->filename), "ui_layout.json");

    int id = 0;

    // Toolbar
    state->elements[id++] = (UIElement){
        0, ELEM_TOOLBAR, "toolbar", 0, 0, WINDOW_WIDTH, TOOLBAR_HEIGHT, true, true, false
    };

    // Palette
    state->elements[id++] = (UIElement){
        1, ELEM_PALETTE, "palette", 0, TOOLBAR_HEIGHT, PALETTE_WIDTH,
        WINDOW_HEIGHT - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT, true, false, true
    };

    // Canvas
    state->elements[id++] = (UIElement){
        2, ELEM_CANVAS, "canvas", CANVAS_X, CANVAS_Y, CANVAS_WIDTH, CANVAS_HEIGHT, true, true, false
    };

    // Properties
    state->elements[id++] = (UIElement){
        3, ELEM_PROPERTIES, "properties", PROPS_X, TOOLBAR_HEIGHT,
        PROPERTIES_WIDTH, 200, true, false, true
    };

    // Oscilloscope (exact from ui.c: scope_rect = {WINDOW_WIDTH - props_width + 10, 250, 330, 300})
    state->elements[id++] = (UIElement){
        4, ELEM_OSCILLOSCOPE, "oscilloscope", PROPS_X + 10, 250, 330, 300, true, false, true
    };

    // Scope controls
    state->elements[id++] = (UIElement){
        5, ELEM_SCOPE_CONTROLS, "scope_controls", PROPS_X + 10, 555, 330, 100, true, false, false
    };

    // Status bar
    state->elements[id++] = (UIElement){
        6, ELEM_STATUSBAR, "statusbar", 0, WINDOW_HEIGHT - STATUSBAR_HEIGHT,
        WINDOW_WIDTH, STATUSBAR_HEIGHT, true, true, false
    };

    // Scope channels panel (below scope controls, showing CH1-CH8)
    state->elements[id++] = (UIElement){
        7, ELEM_SCOPE_CHANNELS, "scope_channels", PROPS_X + 10, 660, 330, 30, true, false, true
    };

    state->num_elements = id;
}

// Draw the toolbar exactly as in the real app
static void draw_toolbar(SDL_Renderer *r) {
    // Background
    set_rgb(r, 45, 45, 65);
    fill_rect(r, 0, 0, WINDOW_WIDTH, TOOLBAR_HEIGHT);

    // Bottom border
    set_rgb(r, SYNTH_PINK_R, SYNTH_PINK_G, SYNTH_PINK_B);
    SDL_RenderDrawLine(r, 0, TOOLBAR_HEIGHT - 1, WINDOW_WIDTH, TOOLBAR_HEIGHT - 1);

    // Title
    set_rgb(r, SYNTH_CYAN_R, SYNTH_CYAN_G, SYNTH_CYAN_B);
    draw_text(r, "Circuit Playground", 10, 18);

    // Buttons starting at x=200 (from ui.c: btn_x = 200)
    int btn_x = 200;
    int btn_w = 60, btn_h = 30;

    for (int i = 0; i < NUM_TOOLBAR_BUTTONS; i++) {
        // Button background
        set_rgb(r, 55, 55, 75);
        fill_rect(r, btn_x, 10, btn_w, btn_h);

        // Button border
        set_rgb(r, 100, 100, 120);
        draw_rect(r, btn_x, 10, btn_w, btn_h);

        // Button text
        set_rgb(r, 200, 200, 210);
        int text_x = btn_x + (btn_w - (int)strlen(toolbar_buttons[i]) * 8) / 2;
        draw_text(r, toolbar_buttons[i], text_x, 21);

        btn_x += btn_w + 10;
        if (i == 3) btn_x += 20;  // Gap after Reset
    }

    // Speed slider area (after SVG button)
    int speed_x = btn_x + 30;
    set_rgb(r, 150, 150, 160);
    draw_text(r, "Speed:", speed_x, 18);

    // Slider track
    set_rgb(r, 40, 40, 60);
    fill_rect(r, speed_x + 55, 15, 100, 20);
    set_rgb(r, 80, 80, 100);
    draw_rect(r, speed_x + 55, 15, 100, 20);

    // Slider fill (50%)
    set_rgb(r, SYNTH_CYAN_R, SYNTH_CYAN_G, SYNTH_CYAN_B);
    fill_rect(r, speed_x + 55, 15, 50, 20);

    // Speed value
    set_rgb(r, SYNTH_CYAN_R, SYNTH_CYAN_G, SYNTH_CYAN_B);
    draw_text(r, "1.0x", speed_x + 160, 18);

    // Time step controls (from ui.c: ts_x = ui->speed_slider.x + 50 + 100 + 60)
    int ts_x = speed_x + 55 + 100 + 60;
    set_rgb(r, 150, 150, 160);
    draw_text(r, "dt:", ts_x, 18);

    set_rgb(r, SYNTH_CYAN_R, SYNTH_CYAN_G, SYNTH_CYAN_B);
    draw_text(r, "100ns", ts_x + 28, 18);

    // - button
    set_rgb(r, 55, 55, 75);
    fill_rect(r, ts_x + 75, 12, 20, 20);
    set_rgb(r, 100, 100, 120);
    draw_rect(r, ts_x + 75, 12, 20, 20);
    set_rgb(r, 200, 200, 210);
    draw_text(r, "-", ts_x + 81, 16);

    // + button
    set_rgb(r, 55, 55, 75);
    fill_rect(r, ts_x + 97, 12, 20, 20);
    set_rgb(r, 100, 100, 120);
    draw_rect(r, ts_x + 97, 12, 20, 20);
    set_rgb(r, 200, 200, 210);
    draw_text(r, "+", ts_x + 103, 16);

    // Auto button
    set_rgb(r, 55, 55, 75);
    fill_rect(r, ts_x + 120, 10, 40, 24);
    set_rgb(r, SYNTH_GREEN_R, SYNTH_GREEN_G, SYNTH_GREEN_B);
    draw_rect(r, ts_x + 120, 10, 40, 24);
    draw_text(r, "Auto", ts_x + 124, 17);
}

// Draw the palette exactly as in the real app
static void draw_palette(SDL_Renderer *r, int scroll_offset) {
    // Background
    set_rgb(r, 35, 38, 48);
    fill_rect(r, 0, TOOLBAR_HEIGHT, PALETTE_WIDTH, WINDOW_HEIGHT - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT);

    // Right border
    set_rgb(r, 60, 60, 80);
    SDL_RenderDrawLine(r, PALETTE_WIDTH - 1, TOOLBAR_HEIGHT, PALETTE_WIDTH - 1, WINDOW_HEIGHT - STATUSBAR_HEIGHT);

    // Draw categories with items
    int y = TOOLBAR_HEIGHT + 8 - scroll_offset;
    int pal_h = 35;  // Item height

    // Helper to draw a category
    #define DRAW_CATEGORY(name, items, count) do { \
        /* Category header */ \
        if (y >= TOOLBAR_HEIGHT - 10 && y < WINDOW_HEIGHT - STATUSBAR_HEIGHT) { \
            set_rgb(r, SYNTH_CYAN_R, SYNTH_CYAN_G, SYNTH_CYAN_B); \
            draw_text(r, "> ", 5, y); \
            draw_text(r, name, 17, y); \
        } \
        y += 16; \
        /* Items in 2 columns */ \
        int col = 0; \
        for (int i = 0; i < count; i++) { \
            int item_y = y; \
            if (item_y >= TOOLBAR_HEIGHT - pal_h && item_y < WINDOW_HEIGHT - STATUSBAR_HEIGHT) { \
                int item_x = 10 + col * 70; \
                /* Item background */ \
                set_rgb(r, 50, 52, 64); \
                fill_rect(r, item_x, item_y, 60, pal_h); \
                /* Item border */ \
                set_rgb(r, 70, 70, 90); \
                draw_rect(r, item_x, item_y, 60, pal_h); \
                /* Item text */ \
                set_rgb(r, 200, 200, 210); \
                draw_text(r, items[i], item_x + 4, item_y + 14); \
            } \
            col++; \
            if (col >= 2) { col = 0; y += pal_h + 3; } \
        } \
        if (col != 0) y += pal_h + 3; \
        y += 8; /* Gap between categories */ \
    } while(0)

    // Draw several categories
    DRAW_CATEGORY("Tools", tools_items, 5);
    DRAW_CATEGORY("Sources", sources_items, 6);
    DRAW_CATEGORY("Passives", passives_items, 7);
    DRAW_CATEGORY("Diodes", diodes_items, 4);

    // More categories (just headers for space)
    const char *more_cats[] = {"BJT", "FET", "Thyristors", "Op-Amps", "Switches", "Logic Gates", "Circuits"};
    for (int c = 0; c < 7; c++) {
        if (y >= TOOLBAR_HEIGHT - 10 && y < WINDOW_HEIGHT - STATUSBAR_HEIGHT) {
            set_rgb(r, SYNTH_CYAN_R, SYNTH_CYAN_G, SYNTH_CYAN_B);
            draw_text(r, "> ", 5, y);
            draw_text(r, more_cats[c], 17, y);
        }
        y += 16;
        // Placeholder items
        for (int i = 0; i < 2; i++) {
            if (y >= TOOLBAR_HEIGHT - pal_h && y < WINDOW_HEIGHT - STATUSBAR_HEIGHT) {
                set_rgb(r, 50, 52, 64);
                fill_rect(r, 10, y, 60, pal_h);
                fill_rect(r, 80, y, 60, pal_h);
                set_rgb(r, 70, 70, 90);
                draw_rect(r, 10, y, 60, pal_h);
                draw_rect(r, 80, y, 60, pal_h);
            }
            y += pal_h + 3;
        }
        y += 8;
    }

    #undef DRAW_CATEGORY

    // Scrollbar
    int visible_height = WINDOW_HEIGHT - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT;
    int content_height = 1200;  // Approximate
    if (content_height > visible_height) {
        int sb_x = PALETTE_WIDTH - 8;
        int sb_h = visible_height * visible_height / content_height;
        int sb_y = TOOLBAR_HEIGHT + scroll_offset * visible_height / content_height;

        set_rgb(r, 40, 42, 54);
        fill_rect(r, sb_x, TOOLBAR_HEIGHT, 6, visible_height);
        set_rgb(r, 80, 80, 100);
        fill_rect(r, sb_x, sb_y, 6, sb_h);
    }
}

// Draw the canvas
static void draw_canvas(SDL_Renderer *r) {
    // Background
    set_rgb(r, 20, 22, 30);
    fill_rect(r, CANVAS_X, CANVAS_Y, CANVAS_WIDTH, CANVAS_HEIGHT);

    // Grid
    set_rgb(r, 40, 42, 54);
    for (int x = CANVAS_X; x < CANVAS_X + CANVAS_WIDTH; x += 20) {
        SDL_RenderDrawLine(r, x, CANVAS_Y, x, CANVAS_Y + CANVAS_HEIGHT);
    }
    for (int y = CANVAS_Y; y < CANVAS_Y + CANVAS_HEIGHT; y += 20) {
        SDL_RenderDrawLine(r, CANVAS_X, y, CANVAS_X + CANVAS_WIDTH, y);
    }

    // "Circuit Playground" watermark in center
    set_rgb(r, 50, 52, 64);
    int cx = CANVAS_X + CANVAS_WIDTH / 2 - 70;
    int cy = CANVAS_Y + CANVAS_HEIGHT / 2;
    draw_text(r, "Circuit Canvas", cx, cy);
}

// Draw the oscilloscope
static void draw_oscilloscope(SDL_Renderer *r, int x, int y, int w, int h) {
    // Dark background
    set_rgb(r, 10, 20, 30);
    fill_rect(r, x, y, w, h);

    // Grid (10 vertical, 8 horizontal divisions)
    set_rgb(r, 40, 80, 80);
    int div_x = w / 10;
    int div_y = h / 8;

    for (int i = 0; i <= 10; i++) {
        int lx = x + i * div_x;
        SDL_RenderDrawLine(r, lx, y, lx, y + h);
    }
    for (int i = 0; i <= 8; i++) {
        int ly = y + i * div_y;
        SDL_RenderDrawLine(r, x, ly, x + w, ly);
    }

    // Center crosshair (brighter)
    set_rgb(r, 60, 120, 120);
    SDL_RenderDrawLine(r, x + w/2, y, x + w/2, y + h);
    SDL_RenderDrawLine(r, x, y + h/2, x + w, y + h/2);

    // Simulated waveform (green)
    set_rgb(r, 0, 255, 128);
    int prev_wy = y + h/2;
    for (int i = 0; i < w; i += 2) {
        int wave_y = y + h/2 + (int)(sin((double)i * 0.06) * h/4);
        SDL_RenderDrawLine(r, x + i - 2, prev_wy, x + i, wave_y);
        prev_wy = wave_y;
    }

    // Second channel (cyan, smaller amplitude, different freq)
    set_rgb(r, 0, 255, 255);
    prev_wy = y + h/2;
    for (int i = 0; i < w; i += 2) {
        int wave_y = y + h/2 + (int)(sin((double)i * 0.12 + 1.5) * h/6);
        SDL_RenderDrawLine(r, x + i - 2, prev_wy, x + i, wave_y);
        prev_wy = wave_y;
    }

    // Border
    set_rgb(r, SYNTH_CYAN_R, SYNTH_CYAN_G, SYNTH_CYAN_B);
    draw_rect(r, x, y, w, h);

    // V/div label
    set_rgb(r, 150, 150, 160);
    draw_text(r, "1V/div", x + 5, y + 5);

    // T/div label
    draw_text(r, "1ms/div", x + w - 65, y + 5);

    // Channel labels
    set_rgb(r, 0, 255, 128);
    draw_text(r, "CH1", x + 5, y + h - 12);
    set_rgb(r, 0, 255, 255);
    draw_text(r, "CH2", x + 40, y + h - 12);
}

// Draw scope controls
static void draw_scope_controls(SDL_Renderer *r, int x, int y, int w, int h) {
    // Background
    set_rgb(r, 35, 38, 48);
    fill_rect(r, x, y, w, h);

    // Border
    set_rgb(r, 60, 60, 80);
    draw_rect(r, x, y, w, h);

    // V/div buttons
    set_rgb(r, 150, 150, 160);
    draw_text(r, "V/div:", x + 5, y + 8);

    int btn_y = y + 22;
    const char *vdiv_opts[] = {"1mV", "10mV", "100mV", "1V", "10V"};
    for (int i = 0; i < 5; i++) {
        set_rgb(r, 50, 52, 64);
        fill_rect(r, x + 5 + i * 32, btn_y, 30, 18);
        set_rgb(r, i == 3 ? SYNTH_CYAN_R : 80, i == 3 ? SYNTH_CYAN_G : 80, i == 3 ? SYNTH_CYAN_B : 100);
        draw_rect(r, x + 5 + i * 32, btn_y, 30, 18);
        set_rgb(r, 180, 180, 190);
        draw_text(r, vdiv_opts[i], x + 7 + i * 32, btn_y + 5);
    }

    // T/div buttons
    set_rgb(r, 150, 150, 160);
    draw_text(r, "T/div:", x + 5, y + 48);

    btn_y = y + 62;
    const char *tdiv_opts[] = {"1us", "10us", "1ms", "10ms", "1s"};
    for (int i = 0; i < 5; i++) {
        set_rgb(r, 50, 52, 64);
        fill_rect(r, x + 5 + i * 32, btn_y, 30, 18);
        set_rgb(r, i == 2 ? SYNTH_CYAN_R : 80, i == 2 ? SYNTH_CYAN_G : 80, i == 2 ? SYNTH_CYAN_B : 100);
        draw_rect(r, x + 5 + i * 32, btn_y, 30, 18);
        set_rgb(r, 180, 180, 190);
        draw_text(r, tdiv_opts[i], x + 7 + i * 32, btn_y + 5);
    }

    // Trigger mode
    set_rgb(r, 150, 150, 160);
    draw_text(r, "Trig: Auto", x + 180, y + 8);

    // MC (Monte Carlo) button
    set_rgb(r, 50, 52, 64);
    fill_rect(r, x + 280, y + 5, 40, 20);
    set_rgb(r, SYNTH_PURPLE_R, SYNTH_PURPLE_G, SYNTH_PURPLE_B);
    draw_rect(r, x + 280, y + 5, 40, 20);
    draw_text(r, "MC", x + 290, y + 11);

    // Bode button
    set_rgb(r, 50, 52, 64);
    fill_rect(r, x + 230, y + 5, 45, 20);
    set_rgb(r, SYNTH_ORANGE_R, SYNTH_ORANGE_G, SYNTH_ORANGE_B);
    draw_rect(r, x + 230, y + 5, 45, 20);
    draw_text(r, "Bode", x + 237, y + 11);
}

// Draw scope channels panel (CH1-CH8 indicators)
static void draw_scope_channels(SDL_Renderer *r, int x, int y, int w, int h) {
    // Background
    set_rgb(r, 35, 38, 48);
    fill_rect(r, x, y, w, h);

    // Border
    set_rgb(r, 60, 60, 80);
    draw_rect(r, x, y, w, h);

    // Channel indicators
    const int ch_colors[][3] = {
        {0, 255, 128},   // CH1 - green
        {0, 255, 255},   // CH2 - cyan
        {255, 128, 0},   // CH3 - orange
        {255, 0, 255},   // CH4 - magenta
        {255, 255, 0},   // CH5 - yellow
        {128, 128, 255}, // CH6 - light blue
        {255, 128, 128}, // CH7 - light red
        {128, 255, 128}, // CH8 - light green
    };

    char ch_label[8];
    for (int i = 0; i < 8; i++) {
        int cx = x + 5 + i * 40;
        set_rgb(r, ch_colors[i][0], ch_colors[i][1], ch_colors[i][2]);
        snprintf(ch_label, sizeof(ch_label), "CH%d", i + 1);
        draw_text(r, ch_label, cx, y + 10);
    }
}

// Draw properties panel
static void draw_properties(SDL_Renderer *r, int x, int y, int w, int h) {
    // Background
    set_rgb(r, 38, 40, 52);
    fill_rect(r, x, y, w, h);

    // Left border
    set_rgb(r, 60, 60, 80);
    SDL_RenderDrawLine(r, x, y, x, y + h);

    // Title
    set_rgb(r, SYNTH_PINK_R, SYNTH_PINK_G, SYNTH_PINK_B);
    draw_text(r, "Properties", x + 10, y + 10);

    // Separator
    set_rgb(r, 60, 60, 80);
    SDL_RenderDrawLine(r, x + 10, y + 25, x + w - 20, y + 25);

    // Sample properties
    set_rgb(r, 150, 150, 160);
    draw_text(r, "No component selected", x + 10, y + 40);
    draw_text(r, "Click a component to", x + 10, y + 55);
    draw_text(r, "view its properties.", x + 10, y + 70);
}

// Draw status bar exactly as in the real app
static void draw_statusbar(SDL_Renderer *r) {
    int y = WINDOW_HEIGHT - STATUSBAR_HEIGHT;

    // Background
    set_rgb(r, 30, 32, 42);
    fill_rect(r, 0, y, WINDOW_WIDTH, STATUSBAR_HEIGHT);

    // Top border
    set_rgb(r, 60, 60, 80);
    SDL_RenderDrawLine(r, 0, y, WINDOW_WIDTH, y);

    // Status message (left side)
    set_rgb(r, 120, 120, 140);
    draw_text(r, "Ready - Press F1 for help", 10, y + 8);

    // Lux slider - shifted right for better spacing
    int env_x = 350;
    set_rgb(r, 120, 120, 140);
    draw_text(r, "Lux:", env_x, y + 8);

    // Slider background
    set_rgb(r, 35, 38, 48);
    fill_rect(r, env_x + 32, y + 5, 70, 14);
    set_rgb(r, SYNTH_YELLOW_R, SYNTH_YELLOW_G, SYNTH_YELLOW_B);
    draw_rect(r, env_x + 32, y + 5, 70, 14);

    // Slider fill (50%)
    fill_rect(r, env_x + 32, y + 5, 35, 14);

    // Lux value
    draw_text(r, "50%", env_x + 106, y + 8);

    // Temperature slider (from ui.c: temp_x = env_x + 28 + 70 + 45)
    int temp_x = env_x + 28 + 70 + 45;
    set_rgb(r, 120, 120, 140);
    draw_text(r, "Tmp:", temp_x, y + 8);

    // Slider background
    set_rgb(r, 35, 38, 48);
    fill_rect(r, temp_x + 32, y + 5, 70, 14);
    set_rgb(r, SYNTH_ORANGE_R, SYNTH_ORANGE_G, SYNTH_ORANGE_B);
    draw_rect(r, temp_x + 32, y + 5, 70, 14);

    // Slider fill (~40% for 25C on -40 to 125 range)
    fill_rect(r, temp_x + 32, y + 5, 28, 14);

    // Temp value
    draw_text(r, "25C", temp_x + 106, y + 8);

    // dt indicator (from ui.c: WINDOW_WIDTH - 350)
    set_rgb(r, SYNTH_CYAN_R, SYNTH_CYAN_G, SYNTH_CYAN_B);
    draw_text(r, "dt:1.0x", WINDOW_WIDTH - 350, y + 8);

    // Time (from ui.c: WINDOW_WIDTH - 250)
    draw_text(r, "t=0.000s", WINDOW_WIDTH - 250, y + 8);

    // Component/Node counts (from ui.c: WINDOW_WIDTH - 120)
    set_rgb(r, 120, 120, 140);
    draw_text(r, "C:0 N:0", WINDOW_WIDTH - 120, y + 8);
}

// Draw the full editor view
static void draw_editor(SDL_Renderer *r, EditorState *state) {
    // Clear
    set_rgb(r, SYNTH_BG_DARK_R, SYNTH_BG_DARK_G, SYNTH_BG_DARK_B);
    SDL_RenderClear(r);

    // Draw all UI areas in proper order
    draw_canvas(r);
    draw_palette(r, state->palette_scroll);
    draw_properties(r, PROPS_X, TOOLBAR_HEIGHT, PROPERTIES_WIDTH, 200);
    draw_oscilloscope(r, PROPS_X + 10, 250, 330, 300);
    draw_scope_controls(r, PROPS_X + 10, 555, 330, 100);
    draw_scope_channels(r, state->elements[7].x, state->elements[7].y,
                        state->elements[7].width, state->elements[7].height);
    draw_toolbar(r);
    draw_statusbar(r);

    // Selection highlight for selected element
    if (state->selected_id >= 0) {
        UIElement *e = &state->elements[state->selected_id];
        set_rgb(r, SYNTH_PINK_R, SYNTH_PINK_G, SYNTH_PINK_B);
        draw_rect(r, e->x - 2, e->y - 2, e->width + 4, e->height + 4);
        draw_rect(r, e->x - 1, e->y - 1, e->width + 2, e->height + 2);

        // Resize handles if resizable
        if (e->resizable) {
            set_rgb(r, SYNTH_CYAN_R, SYNTH_CYAN_G, SYNTH_CYAN_B);
            fill_rect(r, e->x + e->width/2 - 3, e->y - 3, 6, 6);
            fill_rect(r, e->x + e->width/2 - 3, e->y + e->height - 3, 6, 6);
            fill_rect(r, e->x - 3, e->y + e->height/2 - 3, 6, 6);
            fill_rect(r, e->x + e->width - 3, e->y + e->height/2 - 3, 6, 6);
        }
    }

    // Help overlay
    set_rgb(r, 30, 30, 50);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 30, 30, 50, 230);
    SDL_Rect help_bg = {WINDOW_WIDTH - 170, 55, 165, 95};
    SDL_RenderFillRect(r, &help_bg);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    set_rgb(r, SYNTH_PURPLE_R, SYNTH_PURPLE_G, SYNTH_PURPLE_B);
    draw_rect(r, WINDOW_WIDTH - 170, 55, 165, 95);

    set_rgb(r, 180, 180, 190);
    draw_text(r, "UI Layout Editor", WINDOW_WIDTH - 165, 60);
    draw_text(r, "Click to select", WINDOW_WIDTH - 165, 78);
    draw_text(r, "Drag to move", WINDOW_WIDTH - 165, 93);
    draw_text(r, "S = Save JSON", WINDOW_WIDTH - 165, 108);
    draw_text(r, "ESC = Deselect", WINDOW_WIDTH - 165, 123);
    draw_text(r, "Scroll = Palette", WINDOW_WIDTH - 165, 138);

    // Selected element info
    if (state->selected_id >= 0) {
        UIElement *e = &state->elements[state->selected_id];
        char info[128];
        snprintf(info, sizeof(info), "Selected: %s @ (%d,%d) %dx%d", e->name, e->x, e->y, e->width, e->height);

        set_rgb(r, 40, 40, 60);
        fill_rect(r, CANVAS_X + 5, CANVAS_Y + 5, 300, 18);
        set_rgb(r, SYNTH_CYAN_R, SYNTH_CYAN_G, SYNTH_CYAN_B);
        draw_text(r, info, CANVAS_X + 10, CANVAS_Y + 10);
    }

    // Modified indicator
    if (state->modified) {
        set_rgb(r, SYNTH_PINK_R, SYNTH_PINK_G, SYNTH_PINK_B);
        draw_text(r, "* Modified", WINDOW_WIDTH - 90, 40);
    }

    SDL_RenderPresent(r);
}

// Save layout to JSON
static void save_layout(EditorState *state) {
    FILE *f = fopen(state->filename, "w");
    if (!f) {
        printf("Failed to save layout\n");
        return;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"version\": 1,\n");
    fprintf(f, "  \"window\": {\"width\": %d, \"height\": %d},\n", WINDOW_WIDTH, WINDOW_HEIGHT);
    fprintf(f, "  \"constants\": {\n");
    fprintf(f, "    \"TOOLBAR_HEIGHT\": %d,\n", TOOLBAR_HEIGHT);
    fprintf(f, "    \"PALETTE_WIDTH\": %d,\n", PALETTE_WIDTH);
    fprintf(f, "    \"PROPERTIES_WIDTH\": %d,\n", PROPERTIES_WIDTH);
    fprintf(f, "    \"STATUSBAR_HEIGHT\": %d\n", STATUSBAR_HEIGHT);
    fprintf(f, "  },\n");
    fprintf(f, "  \"elements\": [\n");

    const char *type_names[] = {"toolbar", "palette", "canvas", "oscilloscope", "properties", "statusbar", "scope_controls", "scope_channels"};

    for (int i = 0; i < state->num_elements; i++) {
        UIElement *e = &state->elements[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"type\": \"%s\",\n", type_names[e->type]);
        fprintf(f, "      \"name\": \"%s\",\n", e->name);
        fprintf(f, "      \"x\": %d, \"y\": %d,\n", e->x, e->y);
        fprintf(f, "      \"width\": %d, \"height\": %d,\n", e->width, e->height);
        fprintf(f, "      \"locked\": %s,\n", e->locked ? "true" : "false");
        fprintf(f, "      \"resizable\": %s\n", e->resizable ? "true" : "false");
        fprintf(f, "    }%s\n", (i < state->num_elements - 1) ? "," : "");
    }

    fprintf(f, "  ]\n}\n");
    fclose(f);

    state->modified = false;
    printf("Saved layout to %s\n", state->filename);
}

// Check if point in element
static bool point_in_element(UIElement *e, int x, int y) {
    return x >= e->x && x < e->x + e->width && y >= e->y && y < e->y + e->height;
}

// Find element at point
static int find_element_at(EditorState *state, int x, int y) {
    for (int i = state->num_elements - 1; i >= 0; i--) {
        if (state->elements[i].visible && point_in_element(&state->elements[i], x, y)) {
            return i;
        }
    }
    return -1;
}

// Snap to grid
static int snap(int val, int grid, bool enabled) {
    if (!enabled) return val;
    return ((val + grid / 2) / grid) * grid;
}

// Handle events
static void handle_event(EditorState *state, SDL_Event *ev) {
    switch (ev->type) {
        case SDL_MOUSEBUTTONDOWN:
            if (ev->button.button == SDL_BUTTON_LEFT) {
                int id = find_element_at(state, ev->button.x, ev->button.y);
                state->selected_id = id;
                if (id >= 0 && !state->elements[id].locked) {
                    state->dragging = true;
                    state->drag_offset_x = ev->button.x - state->elements[id].x;
                    state->drag_offset_y = ev->button.y - state->elements[id].y;
                }
            }
            break;

        case SDL_MOUSEBUTTONUP:
            state->dragging = false;
            state->resizing = false;
            break;

        case SDL_MOUSEMOTION:
            state->mouse_x = ev->motion.x;
            state->mouse_y = ev->motion.y;
            if (state->dragging && state->selected_id >= 0) {
                UIElement *e = &state->elements[state->selected_id];
                if (!e->locked) {
                    e->x = snap(ev->motion.x - state->drag_offset_x, state->grid_size, state->snap_to_grid);
                    e->y = snap(ev->motion.y - state->drag_offset_y, state->grid_size, state->snap_to_grid);
                    state->modified = true;
                }
            }
            break;

        case SDL_MOUSEWHEEL:
            // Scroll palette
            state->palette_scroll -= ev->wheel.y * 30;
            if (state->palette_scroll < 0) state->palette_scroll = 0;
            if (state->palette_scroll > 800) state->palette_scroll = 800;
            break;

        case SDL_KEYDOWN:
            switch (ev->key.keysym.sym) {
                case SDLK_s:
                    save_layout(state);
                    break;
                case SDLK_g:
                    state->show_grid = !state->show_grid;
                    break;
                case SDLK_ESCAPE:
                    state->selected_id = -1;
                    state->dragging = false;
                    break;
            }
            break;
    }
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Circuit Playground - UI Layout Editor (Exact 1280x720)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    EditorState state;
    editor_init(&state);

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            else handle_event(&state, &event);
        }

        draw_editor(renderer, &state);
        SDL_Delay(16);
    }

    if (state.modified) {
        save_layout(&state);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
