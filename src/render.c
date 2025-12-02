/**
 * Circuit Playground - Rendering System Implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "render.h"

// Forward declarations for new component symbols
void render_fuse(RenderContext *ctx, float x, float y, int rotation);
void render_crystal(RenderContext *ctx, float x, float y, int rotation);
void render_spark_gap(RenderContext *ctx, float x, float y, int rotation);
void render_potentiometer(RenderContext *ctx, float x, float y, int rotation);
void render_photoresistor(RenderContext *ctx, float x, float y, int rotation);
void render_thermistor(RenderContext *ctx, float x, float y, int rotation);
void render_memristor(RenderContext *ctx, float x, float y, int rotation);
void render_varactor(RenderContext *ctx, float x, float y, int rotation);
void render_tunnel_diode(RenderContext *ctx, float x, float y, int rotation);
void render_photodiode(RenderContext *ctx, float x, float y, int rotation);
void render_scr(RenderContext *ctx, float x, float y, int rotation);
void render_diac(RenderContext *ctx, float x, float y, int rotation);
void render_triac(RenderContext *ctx, float x, float y, int rotation);
void render_ujt(RenderContext *ctx, float x, float y, int rotation);
void render_njfet(RenderContext *ctx, float x, float y, int rotation);
void render_pjfet(RenderContext *ctx, float x, float y, int rotation);
void render_darlington_npn(RenderContext *ctx, float x, float y, int rotation);
void render_darlington_pnp(RenderContext *ctx, float x, float y, int rotation);
void render_opamp_real(RenderContext *ctx, float x, float y, int rotation);
void render_ota(RenderContext *ctx, float x, float y, int rotation);
void render_ccii(RenderContext *ctx, float x, float y, int rotation, bool is_plus);
void render_vcvs(RenderContext *ctx, float x, float y, int rotation);
void render_vccs(RenderContext *ctx, float x, float y, int rotation);
void render_ccvs(RenderContext *ctx, float x, float y, int rotation);
void render_cccs(RenderContext *ctx, float x, float y, int rotation);
void render_dpdt_switch(RenderContext *ctx, float x, float y, int rotation, int position);
void render_relay(RenderContext *ctx, float x, float y, int rotation, bool energized);
void render_analog_switch(RenderContext *ctx, float x, float y, int rotation, bool closed);
void render_lamp(RenderContext *ctx, float x, float y, int rotation);
void render_speaker(RenderContext *ctx, float x, float y, int rotation);
void render_dc_motor(RenderContext *ctx, float x, float y, int rotation);
void render_voltmeter(RenderContext *ctx, float x, float y, int rotation);
void render_ammeter(RenderContext *ctx, float x, float y, int rotation);
void render_wattmeter(RenderContext *ctx, float x, float y, int rotation);
void render_ac_current_source(RenderContext *ctx, float x, float y, int rotation);
void render_clock_source(RenderContext *ctx, float x, float y, int rotation);
void render_pulse_source(RenderContext *ctx, float x, float y, int rotation);
void render_pwm_source(RenderContext *ctx, float x, float y, int rotation);
void render_not_gate(RenderContext *ctx, float x, float y, int rotation);
void render_and_gate(RenderContext *ctx, float x, float y, int rotation);
void render_or_gate(RenderContext *ctx, float x, float y, int rotation);
void render_nand_gate(RenderContext *ctx, float x, float y, int rotation);
void render_nor_gate(RenderContext *ctx, float x, float y, int rotation);
void render_xor_gate(RenderContext *ctx, float x, float y, int rotation);
void render_xnor_gate(RenderContext *ctx, float x, float y, int rotation);
void render_buffer(RenderContext *ctx, float x, float y, int rotation);
void render_555_timer(RenderContext *ctx, float x, float y, int rotation);
void render_logic_input(RenderContext *ctx, float x, float y, int rotation, bool high);
void render_logic_output(RenderContext *ctx, float x, float y, int rotation, bool high);
void render_d_flipflop(RenderContext *ctx, float x, float y, int rotation);
void render_vco(RenderContext *ctx, float x, float y, int rotation);
void render_optocoupler(RenderContext *ctx, float x, float y, int rotation);
void render_test_point(RenderContext *ctx, float x, float y, int rotation);
void render_7seg_display(RenderContext *ctx, float x, float y, int rotation);
void render_led_array(RenderContext *ctx, float x, float y, int rotation);
void render_bcd_decoder(RenderContext *ctx, float x, float y, int rotation);

// Simple 8x8 bitmap font (ASCII 32-126)
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

// Forward declarations for component render functions
void render_zener(RenderContext *ctx, float x, float y, int rotation);
void render_schottky(RenderContext *ctx, float x, float y, int rotation);
void render_led(RenderContext *ctx, float x, float y, int rotation);
void render_capacitor_elec(RenderContext *ctx, float x, float y, int rotation);
void render_spst_switch(RenderContext *ctx, float x, float y, int rotation, bool closed);
void render_spdt_switch(RenderContext *ctx, float x, float y, int rotation, int position);
void render_push_button(RenderContext *ctx, float x, float y, int rotation, bool pressed);
void render_transformer(RenderContext *ctx, float x, float y, int rotation);
void render_transformer_ct(RenderContext *ctx, float x, float y, int rotation);

// Draw a single character using bitmap font
static void draw_char(SDL_Renderer *renderer, char c, int x, int y) {
    if (c < 32 || c > 126) c = '?';
    const unsigned char *glyph = font8x8[c - 32];

    for (int row = 0; row < 8; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (1 << col)) {
                SDL_RenderDrawPoint(renderer, x + col, y + row);
            }
        }
    }
}

// Draw a scaled character using bitmap font
static void draw_char_scaled(SDL_Renderer *renderer, char c, int x, int y, int scale, bool bold, bool italic) {
    if (c < 32 || c > 126) c = '?';
    const unsigned char *glyph = font8x8[c - 32];

    for (int row = 0; row < 8; row++) {
        unsigned char bits = glyph[row];
        // Italic: shift pixels right based on row (from bottom to top)
        int italic_shift = italic ? (7 - row) / 3 : 0;
        for (int col = 0; col < 8; col++) {
            if (bits & (1 << col)) {
                SDL_Rect rect = {x + (col + italic_shift) * scale, y + row * scale, scale, scale};
                SDL_RenderFillRect(renderer, &rect);
                // Bold: draw extra pixel to the right
                if (bold) {
                    SDL_Rect bold_rect = {x + (col + italic_shift + 1) * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRect(renderer, &bold_rect);
                }
            }
        }
    }
}

RenderContext *render_create(SDL_Renderer *renderer) {
    RenderContext *ctx = calloc(1, sizeof(RenderContext));
    if (!ctx) return NULL;

    ctx->renderer = renderer;
    // Align initial offset to grid for proper grid line alignment
    ctx->offset_x = (CANVAS_WIDTH / 2 / GRID_SIZE) * GRID_SIZE;
    ctx->offset_y = (CANVAS_HEIGHT / 2 / GRID_SIZE) * GRID_SIZE;
    ctx->zoom = 1.0f;
    ctx->show_grid = true;
    ctx->snap_to_grid = true;

    ctx->canvas_rect = (Rect){CANVAS_X, CANVAS_Y, CANVAS_WIDTH, CANVAS_HEIGHT};
    ctx->show_current = true;  // Show current flow by default

    // Initialize animation timing
    ctx->animation_time = 0.0;
    ctx->last_frame_time = (double)SDL_GetTicks() / 1000.0;

    return ctx;
}

void render_free(RenderContext *ctx) {
    free(ctx);
}

void render_screen_to_world(RenderContext *ctx, int sx, int sy, float *wx, float *wy) {
    *wx = (sx - ctx->offset_x) / ctx->zoom;
    *wy = (sy - ctx->offset_y) / ctx->zoom;
}

void render_world_to_screen(RenderContext *ctx, float wx, float wy, int *sx, int *sy) {
    *sx = (int)(wx * ctx->zoom + ctx->offset_x) + ctx->canvas_rect.x;
    *sy = (int)(wy * ctx->zoom + ctx->offset_y) + ctx->canvas_rect.y;
}

void render_pan(RenderContext *ctx, int dx, int dy) {
    ctx->offset_x += dx;
    ctx->offset_y += dy;
}

void render_zoom(RenderContext *ctx, float factor, int center_x, int center_y) {
    float wx, wy;
    render_screen_to_world(ctx, center_x - ctx->canvas_rect.x, center_y - ctx->canvas_rect.y, &wx, &wy);

    ctx->zoom *= factor;
    ctx->zoom = CLAMP(ctx->zoom, MIN_ZOOM, MAX_ZOOM);

    // Adjust offset to keep mouse position fixed
    ctx->offset_x = (center_x - ctx->canvas_rect.x) - wx * ctx->zoom;
    ctx->offset_y = (center_y - ctx->canvas_rect.y) - wy * ctx->zoom;
}

void render_reset_view(RenderContext *ctx) {
    // Align offset to grid for proper grid line alignment
    ctx->offset_x = (ctx->canvas_rect.w / 2 / GRID_SIZE) * GRID_SIZE;
    ctx->offset_y = (ctx->canvas_rect.h / 2 / GRID_SIZE) * GRID_SIZE;
    ctx->zoom = 1.0f;
}

void render_set_color(RenderContext *ctx, Color color) {
    SDL_SetRenderDrawColor(ctx->renderer, color.r, color.g, color.b, color.a);
}

void render_draw_line(RenderContext *ctx, float x1, float y1, float x2, float y2) {
    int sx1, sy1, sx2, sy2;
    render_world_to_screen(ctx, x1, y1, &sx1, &sy1);
    render_world_to_screen(ctx, x2, y2, &sx2, &sy2);
    SDL_RenderDrawLine(ctx->renderer, sx1, sy1, sx2, sy2);
}

void render_draw_rect(RenderContext *ctx, float x, float y, float w, float h) {
    int sx, sy;
    render_world_to_screen(ctx, x, y, &sx, &sy);
    SDL_Rect rect = {sx, sy, (int)(w * ctx->zoom), (int)(h * ctx->zoom)};
    SDL_RenderDrawRect(ctx->renderer, &rect);
}

void render_fill_rect(RenderContext *ctx, float x, float y, float w, float h) {
    int sx, sy;
    render_world_to_screen(ctx, x, y, &sx, &sy);
    SDL_Rect rect = {sx, sy, (int)(w * ctx->zoom), (int)(h * ctx->zoom)};
    SDL_RenderFillRect(ctx->renderer, &rect);
}

void render_draw_circle(RenderContext *ctx, float cx, float cy, float r) {
    int sx, sy;
    render_world_to_screen(ctx, cx, cy, &sx, &sy);
    int sr = (int)(r * ctx->zoom);

    // Simple circle drawing algorithm
    for (int angle = 0; angle < 360; angle += 5) {
        float rad1 = angle * M_PI / 180;
        float rad2 = (angle + 5) * M_PI / 180;
        int x1 = sx + (int)(sr * cos(rad1));
        int y1 = sy + (int)(sr * sin(rad1));
        int x2 = sx + (int)(sr * cos(rad2));
        int y2 = sy + (int)(sr * sin(rad2));
        SDL_RenderDrawLine(ctx->renderer, x1, y1, x2, y2);
    }
}

void render_fill_circle(RenderContext *ctx, float cx, float cy, float r) {
    int sx, sy;
    render_world_to_screen(ctx, cx, cy, &sx, &sy);
    int sr = (int)(r * ctx->zoom);

    for (int dy = -sr; dy <= sr; dy++) {
        int dx = (int)sqrt(sr*sr - dy*dy);
        SDL_RenderDrawLine(ctx->renderer, sx - dx, sy + dy, sx + dx, sy + dy);
    }
}

void render_draw_line_screen(RenderContext *ctx, int x1, int y1, int x2, int y2) {
    SDL_RenderDrawLine(ctx->renderer, x1, y1, x2, y2);
}

void render_draw_rect_screen(RenderContext *ctx, int x, int y, int w, int h) {
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderDrawRect(ctx->renderer, &rect);
}

void render_fill_rect_screen(RenderContext *ctx, int x, int y, int w, int h) {
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(ctx->renderer, &rect);
}

// Text rendering using bitmap font
void render_draw_text(RenderContext *ctx, const char *text, int x, int y, Color color) {
    if (!text || !*text) return;  // Safety check for NULL or empty string
    SDL_SetRenderDrawColor(ctx->renderer, color.r, color.g, color.b, color.a);
    int cx = x;
    while (*text) {
        draw_char(ctx->renderer, *text, cx, y);
        cx += 8;
        text++;
    }
}

void render_draw_text_small(RenderContext *ctx, const char *text, int x, int y, Color color) {
    // Use same font for now (could scale down if needed)
    render_draw_text(ctx, text, x, y, color);
}

// Styled text rendering with size and formatting
void render_draw_text_styled(RenderContext *ctx, const char *text, int x, int y, Color color,
                             int font_size, bool bold, bool italic, bool underline) {
    if (!text || !*text) return;
    SDL_SetRenderDrawColor(ctx->renderer, color.r, color.g, color.b, color.a);

    // Scale based on font_size: 1=small(1x), 2=normal(2x), 3=large(3x)
    int scale = (font_size < 1) ? 1 : (font_size > 3) ? 3 : font_size;
    int char_width = 8 * scale + (bold ? scale : 0);  // Extra width for bold
    int char_height = 8 * scale;

    int cx = x;
    int text_start_x = x;
    while (*text) {
        draw_char_scaled(ctx->renderer, *text, cx, y, scale, bold, italic);
        cx += char_width;
        text++;
    }

    // Draw underline if enabled
    if (underline) {
        int underline_y = y + char_height + scale;
        SDL_Rect underline_rect = {text_start_x, underline_y, cx - text_start_x, scale};
        SDL_RenderFillRect(ctx->renderer, &underline_rect);
    }
}

void render_grid(RenderContext *ctx) {
    render_set_color(ctx, COLOR_GRID);

    // Calculate visible area
    float left, top, right, bottom;
    render_screen_to_world(ctx, 0, 0, &left, &top);
    render_screen_to_world(ctx, ctx->canvas_rect.w, ctx->canvas_rect.h, &right, &bottom);

    // Use floor() to handle negative coordinates correctly
    int start_x = ((int)floor(left / GRID_SIZE) - 1) * GRID_SIZE;
    int start_y = ((int)floor(top / GRID_SIZE) - 1) * GRID_SIZE;
    int end_x = ((int)ceil(right / GRID_SIZE) + 1) * GRID_SIZE;
    int end_y = ((int)ceil(bottom / GRID_SIZE) + 1) * GRID_SIZE;

    // Vertical lines
    for (int x = start_x; x <= end_x; x += GRID_SIZE) {
        render_draw_line(ctx, x, start_y, x, end_y);
    }

    // Horizontal lines
    for (int y = start_y; y <= end_y; y += GRID_SIZE) {
        render_draw_line(ctx, start_x, y, end_x, y);
    }

    // Origin marker
    render_set_color(ctx, (Color){0x3a, 0x3a, 0x5e, 0xff});
    render_draw_line(ctx, -20, 0, 20, 0);
    render_draw_line(ctx, 0, -20, 0, 20);
}

void render_component(RenderContext *ctx, Component *comp) {
    if (!comp) return;

    // Set color based on state
    if (comp->selected) {
        render_set_color(ctx, COLOR_ACCENT2);
    } else if (comp->highlighted) {
        render_set_color(ctx, COLOR_ACCENT);
    } else {
        render_set_color(ctx, COLOR_TEXT);
    }

    // Draw based on type
    switch (comp->type) {
        case COMP_GROUND:
            render_ground(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_DC_VOLTAGE:
            render_voltage_source(ctx, comp->x, comp->y, comp->rotation, false);
            break;
        case COMP_AC_VOLTAGE:
            render_voltage_source(ctx, comp->x, comp->y, comp->rotation, true);
            break;
        case COMP_DC_CURRENT:
            render_current_source(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_RESISTOR:
            // Color based on power dissipation vs rating
            if (!comp->selected && !comp->highlighted) {
                double pwr_ratio = comp->props.resistor.power_dissipated / comp->props.resistor.power_rating;
                if (pwr_ratio > 1.5) {
                    render_set_color(ctx, (Color){0xff, 0x20, 0x20, 0xff});  // Bright red - burning!
                } else if (pwr_ratio > 1.0) {
                    render_set_color(ctx, (Color){0xff, 0x60, 0x00, 0xff});  // Red-orange - overheating
                } else if (pwr_ratio > 0.8) {
                    render_set_color(ctx, (Color){0xff, 0xaa, 0x00, 0xff});  // Orange - warning
                }
            }
            render_resistor(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_CAPACITOR:
            render_capacitor(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_CAPACITOR_ELEC:
            render_capacitor_elec(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_INDUCTOR:
            render_inductor(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_DIODE:
            render_diode(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_ZENER:
            render_zener(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_SCHOTTKY:
            render_schottky(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_LED: {
            // Get LED color from wavelength
            double wl = comp->props.led.wavelength;
            uint8_t r = 255, g = 0, b = 0;  // Default red
            if (wl >= 380 && wl < 440) {        // Violet
                r = 148; g = 0; b = 211;
            } else if (wl >= 440 && wl < 490) { // Blue
                r = 0; g = 0; b = 255;
            } else if (wl >= 490 && wl < 510) { // Cyan
                r = 0; g = 255; b = 255;
            } else if (wl >= 510 && wl < 580) { // Green
                r = 0; g = 255; b = 0;
            } else if (wl >= 580 && wl < 600) { // Yellow
                r = 255; g = 255; b = 0;
            } else if (wl >= 600 && wl < 640) { // Orange
                r = 255; g = 165; b = 0;
            } else if (wl >= 640 && wl <= 780) { // Red
                r = 255; g = 0; b = 0;
            } else if (wl > 780) {              // Infrared (show as dark red)
                r = 139; g = 0; b = 0;
            }

            // Draw glow if LED has current
            double current = comp->props.led.current;
            if (current > 1e-6) {  // Threshold for visible glow
                // Calculate glow intensity based on current (max ~20mA)
                double intensity = fmin(1.0, current / 0.015);  // Full brightness at 15mA
                uint8_t alpha = (uint8_t)(intensity * 200);

                // Draw glow circles (use SDL directly for filled circle approximation)
                SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_ADD);
                // Get screen coordinates for component center
                int scr_x, scr_y;
                render_world_to_screen(ctx, comp->x, comp->y, &scr_x, &scr_y);
                for (int radius = 20; radius >= 5; radius -= 3) {
                    uint8_t glow_alpha = (uint8_t)(alpha * (1.0 - (radius - 5) / 15.0));
                    SDL_SetRenderDrawColor(ctx->renderer, r, g, b, glow_alpha);
                    int screen_radius = (int)(radius * ctx->zoom);
                    // Draw filled circle approximation
                    for (int dy = -screen_radius; dy <= screen_radius; dy++) {
                        int dx = (int)sqrt(screen_radius * screen_radius - dy * dy);
                        SDL_RenderDrawLine(ctx->renderer, scr_x - dx, scr_y + dy, scr_x + dx, scr_y + dy);
                    }
                }
                SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
            }

            // Set LED symbol color based on wavelength
            if (!comp->selected && !comp->highlighted) {
                render_set_color(ctx, (Color){r, g, b, 0xff});
            }
            render_led(ctx, comp->x, comp->y, comp->rotation);
            break;
        }
        case COMP_NPN_BJT:
            render_bjt(ctx, comp->x, comp->y, comp->rotation, false, "NPN");
            break;
        case COMP_PNP_BJT:
            render_bjt(ctx, comp->x, comp->y, comp->rotation, true, "PNP");
            break;
        case COMP_NMOS:
            render_mosfet(ctx, comp->x, comp->y, comp->rotation, false, "NMOS");
            break;
        case COMP_PMOS:
            render_mosfet(ctx, comp->x, comp->y, comp->rotation, true, "PMOS");
            break;
        case COMP_OPAMP:
            render_opamp(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_OPAMP_FLIPPED:
            render_opamp_flipped(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_SQUARE_WAVE:
            render_square_wave(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_TRIANGLE_WAVE:
            render_triangle_wave(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_SAWTOOTH_WAVE:
            render_sawtooth_wave(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_NOISE_SOURCE:
            render_noise_source(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_SPST_SWITCH:
            render_spst_switch(ctx, comp->x, comp->y, comp->rotation,
                              comp->props.switch_spst.closed);
            break;
        case COMP_SPDT_SWITCH:
            render_spdt_switch(ctx, comp->x, comp->y, comp->rotation,
                              comp->props.switch_spdt.position);
            break;
        case COMP_PUSH_BUTTON:
            render_push_button(ctx, comp->x, comp->y, comp->rotation,
                              comp->props.push_button.pressed);
            break;
        case COMP_TRANSFORMER:
            render_transformer(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_TRANSFORMER_CT:
            render_transformer_ct(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_TEXT: {
            // Render text annotation with formatting
            int sx, sy;
            render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            // Extract color from packed RGBA
            uint32_t c = comp->props.text.color;
            Color text_color = {
                (c >> 24) & 0xFF,
                (c >> 16) & 0xFF,
                (c >> 8) & 0xFF,
                c & 0xFF
            };
            // Use selection color if selected
            if (comp->selected) {
                text_color = COLOR_ACCENT2;
            }
            render_draw_text_styled(ctx, comp->props.text.text, sx, sy, text_color,
                                    comp->props.text.font_size,
                                    comp->props.text.bold,
                                    comp->props.text.italic,
                                    comp->props.text.underline);
            break;
        }
        // === NEW COMPONENT SYMBOLS ===
        case COMP_FUSE:
            render_fuse(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_CRYSTAL:
            render_crystal(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_SPARK_GAP:
            render_spark_gap(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_POTENTIOMETER:
            render_potentiometer(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_PHOTORESISTOR:
            render_photoresistor(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_THERMISTOR:
            render_thermistor(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_MEMRISTOR:
            render_memristor(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_VARACTOR:
            render_varactor(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_TUNNEL_DIODE:
            render_tunnel_diode(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_PHOTODIODE:
            render_photodiode(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_SCR:
            render_scr(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_DIAC:
            render_diac(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_TRIAC:
            render_triac(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_UJT:
            render_ujt(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_NJFET:
            render_njfet(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_PJFET:
            render_pjfet(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_NPN_DARLINGTON:
            render_darlington_npn(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_PNP_DARLINGTON:
            render_darlington_pnp(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_OPAMP_REAL:
            render_opamp_real(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_OTA:
            render_ota(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_CCII_PLUS:
            render_ccii(ctx, comp->x, comp->y, comp->rotation, true);
            break;
        case COMP_CCII_MINUS:
            render_ccii(ctx, comp->x, comp->y, comp->rotation, false);
            break;
        case COMP_VCVS:
            render_vcvs(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_VCCS:
            render_vccs(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_CCVS:
            render_ccvs(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_CCCS:
            render_cccs(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_DPDT_SWITCH:
            render_dpdt_switch(ctx, comp->x, comp->y, comp->rotation, 0);
            break;
        case COMP_RELAY:
            render_relay(ctx, comp->x, comp->y, comp->rotation, false);
            break;
        case COMP_ANALOG_SWITCH:
            render_analog_switch(ctx, comp->x, comp->y, comp->rotation, false);
            break;
        case COMP_LAMP:
            render_lamp(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_SPEAKER:
            render_speaker(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_DC_MOTOR:
            render_dc_motor(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_VOLTMETER:
            render_voltmeter(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_AMMETER:
            render_ammeter(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_WATTMETER:
            render_wattmeter(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_AC_CURRENT:
            render_ac_current_source(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_CLOCK:
            render_clock_source(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_PULSE_SOURCE:
            render_pulse_source(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_PWM_SOURCE:
            render_pwm_source(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_NOT_GATE:
            render_not_gate(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_AND_GATE:
            render_and_gate(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_OR_GATE:
            render_or_gate(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_NAND_GATE:
            render_nand_gate(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_NOR_GATE:
            render_nor_gate(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_XOR_GATE:
            render_xor_gate(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_XNOR_GATE:
            render_xnor_gate(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_BUFFER:
            render_buffer(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_555_TIMER:
            render_555_timer(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_LOGIC_INPUT:
            render_logic_input(ctx, comp->x, comp->y, comp->rotation,
                              comp->props.logic_input.state);
            break;
        case COMP_LOGIC_OUTPUT:
            render_logic_output(ctx, comp->x, comp->y, comp->rotation, false);
            break;
        case COMP_D_FLIPFLOP: {
            render_d_flipflop(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "D", sx - 4, sy - 4, COLOR_TEXT);
            break;
        }
        case COMP_VCO: {
            render_vco(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "VCO", sx - 10, sy - 4, COLOR_TEXT);
            break;
        }
        case COMP_OPTOCOUPLER: {
            render_optocoupler(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "OC", sx - 8, sy - 4, COLOR_TEXT);
            break;
        }
        case COMP_TEST_POINT:
            render_test_point(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_7SEG_DISPLAY:
            render_7seg_display(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_LED_ARRAY:
            render_led_array(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_BCD_DECODER:
            render_bcd_decoder(ctx, comp->x, comp->y, comp->rotation);
            break;

        // Tristate and Schmitt - use buffer with indicator
        case COMP_TRISTATE_BUF:
            render_buffer(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_SCHMITT_INV:
        case COMP_SCHMITT_BUF:
            render_buffer(ctx, comp->x, comp->y, comp->rotation);
            break;

        // Digital ICs - use D flip-flop style box with label
        case COMP_JK_FLIPFLOP: {
            render_d_flipflop(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "JK", sx - 8, sy - 4, COLOR_TEXT);
            break;
        }
        case COMP_T_FLIPFLOP: {
            render_d_flipflop(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "T", sx - 4, sy - 4, COLOR_TEXT);
            break;
        }
        case COMP_SR_LATCH: {
            render_d_flipflop(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "SR", sx - 8, sy - 4, COLOR_TEXT);
            break;
        }
        case COMP_COUNTER: {
            render_d_flipflop(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "CNT", sx - 10, sy - 4, COLOR_TEXT);
            break;
        }
        case COMP_SHIFT_REG: {
            render_d_flipflop(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "SR", sx - 8, sy - 4, COLOR_TEXT);
            break;
        }
        case COMP_MUX_2TO1: {
            render_d_flipflop(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "MUX", sx - 10, sy - 4, COLOR_TEXT);
            break;
        }
        case COMP_DEMUX_1TO2: {
            render_d_flipflop(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "DMX", sx - 10, sy - 4, COLOR_TEXT);
            break;
        }
        case COMP_DECODER: {
            render_d_flipflop(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "DEC", sx - 10, sy - 4, COLOR_TEXT);
            break;
        }
        case COMP_HALF_ADDER: {
            render_d_flipflop(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "HA", sx - 8, sy - 4, COLOR_TEXT);
            break;
        }
        case COMP_FULL_ADDER: {
            render_d_flipflop(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "FA", sx - 8, sy - 4, COLOR_TEXT);
            break;
        }
        case COMP_DAC: {
            render_d_flipflop(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "DAC", sx - 10, sy - 4, COLOR_TEXT);
            break;
        }
        case COMP_ADC: {
            render_d_flipflop(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "ADC", sx - 10, sy - 4, COLOR_TEXT);
            break;
        }
        case COMP_PLL: {
            render_d_flipflop(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "PLL", sx - 10, sy - 4, COLOR_TEXT);
            break;
        }
        case COMP_MONOSTABLE: {
            render_d_flipflop(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "MONO", sx - 14, sy - 4, COLOR_TEXT);
            break;
        }

        // Voltage regulators - use 555-style box with label
        case COMP_LM317: {
            render_555_timer(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "317", sx - 10, sy - 4, COLOR_TEXT);
            break;
        }
        case COMP_7805: {
            render_555_timer(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "7805", sx - 14, sy - 4, COLOR_TEXT);
            break;
        }
        case COMP_TL431: {
            render_555_timer(ctx, comp->x, comp->y, comp->rotation);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            render_draw_text_small(ctx, "431", sx - 10, sy - 4, COLOR_TEXT);
            break;
        }

        // Variable/modulated voltage sources
        case COMP_VADC_SOURCE:
        case COMP_AM_SOURCE:
        case COMP_FM_SOURCE:
            render_voltage_source(ctx, comp->x, comp->y, comp->rotation, false);
            break;

        // Label - display text
        case COMP_LABEL: {
            const char *text = comp->props.text.text;
            render_set_color(ctx, COLOR_ACCENT);
            render_draw_text(ctx, text, (int)comp->x - 15, (int)comp->y - 8, COLOR_ACCENT);
            break;
        }

        default:
            break;
    }

    // Draw terminals
    render_set_color(ctx, COLOR_ACCENT);
    for (int i = 0; i < comp->num_terminals; i++) {
        float tx, ty;
        component_get_terminal_pos(comp, i, &tx, &ty);
        render_fill_circle(ctx, tx, ty, 3);
    }

    // Draw warning icon for overloaded components
    bool show_warning = false;
    double overload_ratio = 0.0;

    if (comp->type == COMP_RESISTOR) {
        if (comp->props.resistor.power_rating > 0) {
            overload_ratio = comp->props.resistor.power_dissipated / comp->props.resistor.power_rating;
            show_warning = (overload_ratio > 1.0);
        }
    } else if (comp->type == COMP_LED) {
        if (comp->props.led.max_current > 0 && comp->props.led.current > 0) {
            overload_ratio = comp->props.led.current / comp->props.led.max_current;
            show_warning = (overload_ratio > 1.0);
        }
    }

    if (show_warning) {
        // Draw warning triangle with exclamation mark at top-right of component
        float icon_x = comp->x + 18;
        float icon_y = comp->y - 15;

        // Warning color - more red based on overload severity
        uint8_t r_col = 255;
        uint8_t g_col = (overload_ratio > 2.0) ? 50 : (overload_ratio > 1.5) ? 100 : 180;
        uint8_t b_col = 0;

        // Animated pulse effect
        double pulse = 0.7 + 0.3 * sin(ctx->sim_time * 8.0);
        r_col = (uint8_t)(r_col * pulse);
        g_col = (uint8_t)(g_col * pulse);

        render_set_color(ctx, (Color){r_col, g_col, b_col, 255});

        // Draw warning triangle
        float tri_size = 10;
        float tx1 = icon_x;                    // Top point
        float ty1 = icon_y - tri_size;
        float tx2 = icon_x - tri_size * 0.866; // Bottom left
        float ty2 = icon_y + tri_size * 0.5;
        float tx3 = icon_x + tri_size * 0.866; // Bottom right
        float ty3 = icon_y + tri_size * 0.5;

        render_draw_line(ctx, tx1, ty1, tx2, ty2);
        render_draw_line(ctx, tx2, ty2, tx3, ty3);
        render_draw_line(ctx, tx3, ty3, tx1, ty1);

        // Draw inner triangle for fill effect
        float inner = 0.7;
        float ix1 = icon_x;
        float iy1 = icon_y - tri_size * inner;
        float ix2 = icon_x - tri_size * 0.866 * inner;
        float iy2 = icon_y + tri_size * 0.5 * inner;
        float ix3 = icon_x + tri_size * 0.866 * inner;
        float iy3 = icon_y + tri_size * 0.5 * inner;

        render_draw_line(ctx, ix1, iy1, ix2, iy2);
        render_draw_line(ctx, ix2, iy2, ix3, iy3);
        render_draw_line(ctx, ix3, iy3, ix1, iy1);

        // Draw exclamation mark in center (black for contrast)
        render_set_color(ctx, (Color){0, 0, 0, 255});
        render_draw_line(ctx, icon_x, icon_y - 5, icon_x, icon_y + 1);
        render_fill_circle(ctx, icon_x, icon_y + 4, 1);

        // If severely overloaded (>200%), add second warning indicator
        if (overload_ratio > 2.0) {
            render_set_color(ctx, (Color){255, 0, 0, 255});
            // Draw X mark
            float xx = icon_x + 12;
            float xy = icon_y;
            render_draw_line(ctx, xx - 4, xy - 4, xx + 4, xy + 4);
            render_draw_line(ctx, xx - 4, xy + 4, xx + 4, xy - 4);
        }
    }
}

void render_wire(RenderContext *ctx, Wire *wire, Circuit *circuit) {
    if (!wire || !circuit) return;

    Node *start = circuit_get_node(circuit, wire->start_node_id);
    Node *end = circuit_get_node(circuit, wire->end_node_id);
    if (!start || !end) return;

    if (wire->selected) {
        render_set_color(ctx, COLOR_ACCENT2);
    } else {
        render_set_color(ctx, COLOR_WIRE);
    }

    render_draw_line(ctx, start->x, start->y, end->x, end->y);

    // Draw animated current flow particles (cyan dots flowing along wires)
    if (ctx->show_current && ctx->sim_running) {
        // Use wire current from simulation (sign indicates direction)
        // Positive current: flows from start_node to end_node
        // Negative current: flows from end_node to start_node
        double current = wire->current;
        double abs_current = fabs(current);

        // Show particles if any measurable current
        if (abs_current > 1e-6) {  // 1µA threshold for visibility
            // Direction based on current sign (conventional current flow)
            float from_x, from_y, to_x, to_y;
            if (current > 0) {
                // Positive current: flows from start to end
                from_x = start->x; from_y = start->y;
                to_x = end->x; to_y = end->y;
            } else {
                // Negative current: flows from end to start
                from_x = end->x; from_y = end->y;
                to_x = start->x; to_y = start->y;
            }

            float dx = to_x - from_x;
            float dy = to_y - from_y;
            float len = sqrt(dx*dx + dy*dy);

            if (len > 5) {  // Draw on wires >= 5 pixels
                // Normalize direction
                dx /= len;
                dy /= len;

                // Animation speed based on current magnitude (logarithmic scale for better visibility)
                // Base speed is comfortable viewing speed, scaled by current
                double log_current = log10(abs_current + 1e-9);  // Range: roughly -9 to 1 for µA to A
                double speed_factor = 0.3 + (log_current + 9.0) * 0.15;  // Map to 0.3 - 1.8 range
                if (speed_factor < 0.2) speed_factor = 0.2;
                if (speed_factor > 3.0) speed_factor = 3.0;

                // Use real-time animation_time for smooth, consistent motion
                double anim_phase = fmod(ctx->animation_time * speed_factor, 1.0);

                // Particle spacing based on wire length - about 20 pixels apart
                int num_particles = (int)(len / 20) + 1;
                if (num_particles > 8) num_particles = 8;
                if (num_particles < 1) num_particles = 1;
                float particle_spacing = 1.0f / (num_particles + 1);

                // Cyan particles (synthwave theme) - brighter for higher current
                uint8_t base_intensity = 180;
                uint8_t intensity = (uint8_t)(base_intensity + fmin(log_current + 6.0, 3.0) * 25);

                for (int i = 0; i < num_particles; i++) {
                    // Position along wire (0 to 1), continuously animated
                    float t = fmod(anim_phase + (i + 1) * particle_spacing, 1.0f);

                    float particle_x = from_x + dx * len * t;
                    float particle_y = from_y + dy * len * t;

                    // Draw glowing particle with cyan color (synthwave theme)
                    // Outer glow
                    render_set_color(ctx, (Color){0x00, 0xff, 0xff, 0x30});
                    render_fill_circle(ctx, particle_x, particle_y, 3);

                    // Middle glow
                    render_set_color(ctx, (Color){0x00, 0xff, 0xff, 0x60});
                    render_fill_circle(ctx, particle_x, particle_y, 2);

                    // Inner bright core
                    render_set_color(ctx, (Color){0x00, intensity, intensity, 0xff});
                    render_fill_circle(ctx, particle_x, particle_y, 1.5f);

                    // White center for extra pop
                    render_set_color(ctx, (Color){0xff, 0xff, 0xff, intensity});
                    render_fill_circle(ctx, particle_x, particle_y, 0.8f);
                }
            }
        }
    }
}

void render_node(RenderContext *ctx, Node *node, bool show_voltage) {
    if (!node) return;

    if (node->is_ground) {
        render_set_color(ctx, (Color){0x88, 0x88, 0x88, 0xff});
    } else {
        render_set_color(ctx, COLOR_ACCENT);
    }

    render_fill_circle(ctx, node->x, node->y, 4);
}

void render_probe(RenderContext *ctx, Probe *probe, int index) {
    if (!probe) return;

    // Probe dimensions (in world coordinates)
    float tip_x = probe->x;
    float tip_y = probe->y;

    // Probe extends diagonally up-left from tip
    float handle_dx = -25;  // Handle offset X
    float handle_dy = -35;  // Handle offset Y

    // Draw selection highlight if selected
    if (probe->selected) {
        render_set_color(ctx, COLOR_ACCENT2);  // Bright highlight
        // Draw glowing outline around the probe area
        float grip_x = tip_x + handle_dx * 0.6f;
        float grip_y = tip_y + handle_dy * 0.6f;
        render_draw_circle(ctx, grip_x, grip_y, 12);
        render_draw_circle(ctx, tip_x, tip_y, 8);
        // Draw highlight line along handle
        for (int i = -4; i <= 4; i++) {
            render_draw_line(ctx, tip_x + 6 + i*0.3f, tip_y + 6 + i*0.3f,
                            tip_x + handle_dx + i*1.5f, tip_y + handle_dy + i*1.5f);
        }
    }

    // Draw probe cable (wire going off to the side)
    render_set_color(ctx, (Color){0x40, 0x40, 0x40, 0xff});
    render_draw_line(ctx, tip_x + handle_dx - 5, tip_y + handle_dy - 5,
                     tip_x + handle_dx - 25, tip_y + handle_dy - 15);
    render_draw_line(ctx, tip_x + handle_dx - 25, tip_y + handle_dy - 15,
                     tip_x + handle_dx - 35, tip_y + handle_dy - 10);

    // Draw probe body/handle (elongated shape)
    render_set_color(ctx, probe->color);

    // Main handle body - thick line from tip toward handle
    for (int i = -2; i <= 2; i++) {
        render_draw_line(ctx, tip_x + 4 + i*0.5f, tip_y + 4 + i*0.5f,
                        tip_x + handle_dx + i, tip_y + handle_dy + i);
    }

    // Handle grip area (wider section)
    float grip_x = tip_x + handle_dx * 0.6f;
    float grip_y = tip_y + handle_dy * 0.6f;
    render_fill_circle(ctx, grip_x, grip_y, 6);
    render_fill_circle(ctx, tip_x + handle_dx * 0.4f, tip_y + handle_dy * 0.4f, 5);
    render_fill_circle(ctx, tip_x + handle_dx * 0.8f, tip_y + handle_dy * 0.8f, 5);

    // Handle end cap
    render_fill_circle(ctx, tip_x + handle_dx, tip_y + handle_dy, 4);

    // Draw metallic probe tip (pointed)
    render_set_color(ctx, (Color){0xcc, 0xcc, 0xcc, 0xff});  // Silver/metallic
    // Tip triangle
    render_draw_line(ctx, tip_x, tip_y, tip_x + 3, tip_y + 5);
    render_draw_line(ctx, tip_x, tip_y, tip_x - 3, tip_y + 5);
    render_draw_line(ctx, tip_x - 3, tip_y + 5, tip_x + 3, tip_y + 5);
    // Tip shaft
    render_draw_line(ctx, tip_x - 2, tip_y + 5, tip_x + 2, tip_y + 5);
    render_draw_line(ctx, tip_x - 2, tip_y + 5, tip_x - 1, tip_y + 8);
    render_draw_line(ctx, tip_x + 2, tip_y + 5, tip_x + 1, tip_y + 8);

    // Contact point indicator (small bright dot at tip)
    render_set_color(ctx, (Color){0xff, 0xff, 0x00, 0xff});  // Yellow contact point
    render_fill_circle(ctx, tip_x, tip_y, 2);

    // Draw outline around handle for visibility
    render_set_color(ctx, COLOR_TEXT);
    render_draw_circle(ctx, grip_x, grip_y, 7);

    // Draw channel label near handle
    int sx, sy;
    render_world_to_screen(ctx, tip_x + handle_dx - 10, tip_y + handle_dy + 5, &sx, &sy);
    if (probe->label[0]) {
        render_draw_text(ctx, probe->label, sx - 8, sy, probe->color);
    } else {
        char buf[16];
        snprintf(buf, sizeof(buf), "CH%d", index + 1);
        render_draw_text(ctx, buf, sx - 12, sy, probe->color);
    }

    // Draw voltage reading near the tip
    char volt_str[16];
    snprintf(volt_str, sizeof(volt_str), "%.2fV", probe->voltage);
    render_world_to_screen(ctx, tip_x + 10, tip_y + 10, &sx, &sy);
    render_draw_text(ctx, volt_str, sx, sy, probe->color);
}

// Draw current flow animation through two-terminal components
static void render_component_current_flow(RenderContext *ctx, Component *comp, Circuit *circuit) {
    if (!comp || !circuit || !ctx->show_current || !ctx->sim_running) return;

    // Only handle two-terminal components for now
    if (comp->num_terminals != 2) return;

    // Skip components that don't conduct current in a visible path
    if (comp->type == COMP_GROUND || comp->type == COMP_TEXT ||
        comp->type == COMP_TEST_POINT) return;

    // Get terminal positions
    float t0_x, t0_y, t1_x, t1_y;
    component_get_terminal_pos(comp, 0, &t0_x, &t0_y);
    component_get_terminal_pos(comp, 1, &t1_x, &t1_y);

    // Get node voltages to determine current direction
    double v0 = 0.0, v1 = 0.0;
    if (comp->node_ids[0] >= 0) {
        for (int i = 0; i < circuit->num_nodes; i++) {
            if (circuit->nodes[i].id == comp->node_ids[0]) {
                v0 = circuit->nodes[i].voltage;
                break;
            }
        }
    }
    if (comp->node_ids[1] >= 0) {
        for (int i = 0; i < circuit->num_nodes; i++) {
            if (circuit->nodes[i].id == comp->node_ids[1]) {
                v1 = circuit->nodes[i].voltage;
                break;
            }
        }
    }

    // Estimate current based on component type
    double current = 0.0;
    double v_diff = v0 - v1;

    switch (comp->type) {
        case COMP_RESISTOR:
            if (comp->props.resistor.resistance > 0)
                current = v_diff / comp->props.resistor.resistance;
            break;
        case COMP_CAPACITOR:
        case COMP_CAPACITOR_ELEC:
            // Capacitor current is proportional to dV/dt, estimate from voltage difference
            current = v_diff / 1000.0;  // Rough approximation
            break;
        case COMP_INDUCTOR:
            current = comp->props.inductor.current;
            break;
        case COMP_DIODE:
        case COMP_ZENER:
        case COMP_SCHOTTKY:
        case COMP_LED:
            // For diodes, use exponential approximation
            if (v_diff > 0.3) current = v_diff / 100.0;  // Forward biased
            break;
        case COMP_FUSE:
            if (!comp->props.fuse.blown)
                current = v_diff / comp->props.fuse.resistance;
            break;
        default:
            // Generic estimation based on voltage difference
            current = v_diff / 1000.0;
            break;
    }

    double abs_current = fabs(current);

    // Threshold for visibility
    if (abs_current < 1e-6) return;

    // Determine direction (conventional current: high to low voltage)
    float from_x, from_y, to_x, to_y;
    if (v0 > v1) {
        from_x = t0_x; from_y = t0_y;
        to_x = t1_x; to_y = t1_y;
    } else {
        from_x = t1_x; from_y = t1_y;
        to_x = t0_x; to_y = t0_y;
    }

    float dx = to_x - from_x;
    float dy = to_y - from_y;
    float len = sqrt(dx*dx + dy*dy);

    if (len < 5) return;  // Too short

    // Normalize direction
    dx /= len;
    dy /= len;

    // Animation speed based on current magnitude
    double log_current = log10(abs_current + 1e-9);
    double speed_factor = 0.3 + (log_current + 9.0) * 0.15;
    if (speed_factor < 0.2) speed_factor = 0.2;
    if (speed_factor > 3.0) speed_factor = 3.0;

    // Use real-time animation_time for smooth motion
    double anim_phase = fmod(ctx->animation_time * speed_factor, 1.0);

    // Particle spacing - about 15 pixels apart for components
    int num_particles = (int)(len / 15) + 1;
    if (num_particles > 6) num_particles = 6;
    if (num_particles < 1) num_particles = 1;
    float particle_spacing = 1.0f / (num_particles + 1);

    // Cyan particles - brighter for higher current
    uint8_t base_intensity = 180;
    uint8_t intensity = (uint8_t)(base_intensity + fmin(log_current + 6.0, 3.0) * 25);

    for (int i = 0; i < num_particles; i++) {
        float t = fmod(anim_phase + (i + 1) * particle_spacing, 1.0f);

        float particle_x = from_x + dx * len * t;
        float particle_y = from_y + dy * len * t;

        // Draw glowing particle
        render_set_color(ctx, (Color){0x00, 0xff, 0xff, 0x30});
        render_fill_circle(ctx, particle_x, particle_y, 2.5f);

        render_set_color(ctx, (Color){0x00, 0xff, 0xff, 0x60});
        render_fill_circle(ctx, particle_x, particle_y, 1.5f);

        render_set_color(ctx, (Color){0x00, intensity, intensity, 0xff});
        render_fill_circle(ctx, particle_x, particle_y, 1);

        render_set_color(ctx, (Color){0xff, 0xff, 0xff, intensity});
        render_fill_circle(ctx, particle_x, particle_y, 0.5f);
    }
}

void render_circuit(RenderContext *ctx, Circuit *circuit) {
    if (!circuit) return;

    // Draw wires first
    for (int i = 0; i < circuit->num_wires; i++) {
        render_wire(ctx, &circuit->wires[i], circuit);
    }

    // Draw nodes
    for (int i = 0; i < circuit->num_nodes; i++) {
        render_node(ctx, &circuit->nodes[i], ctx->show_voltages);
    }

    // Draw components
    for (int i = 0; i < circuit->num_components; i++) {
        render_component(ctx, circuit->components[i]);
    }

    // Draw current flow animation through components (after components so particles appear on top)
    for (int i = 0; i < circuit->num_components; i++) {
        render_component_current_flow(ctx, circuit->components[i], circuit);
    }

    // Draw probes
    for (int i = 0; i < circuit->num_probes; i++) {
        render_probe(ctx, &circuit->probes[i], i);
    }
}

// Component shape rendering functions
// NOTE: All dimensions are scaled to match grid-aligned terminal positions (multiples of 20)

// Helper to rotate a point (dx, dy) around origin by rotation degrees (0, 90, 180, 270)
static void rotate_point(float dx, float dy, int rotation, float *rx, float *ry) {
    switch (rotation % 360) {
        case 0:
        default:
            *rx = dx;
            *ry = dy;
            break;
        case 90:
            *rx = -dy;
            *ry = dx;
            break;
        case 180:
            *rx = -dx;
            *ry = -dy;
            break;
        case 270:
            *rx = dy;
            *ry = -dx;
            break;
    }
}

// Helper to draw a rotated line
static void render_draw_line_rotated(RenderContext *ctx, float cx, float cy,
                                      float x1, float y1, float x2, float y2, int rotation) {
    float rx1, ry1, rx2, ry2;
    rotate_point(x1, y1, rotation, &rx1, &ry1);
    rotate_point(x2, y2, rotation, &rx2, &ry2);
    render_draw_line(ctx, cx + rx1, cy + ry1, cx + rx2, cy + ry2);
}

// Helper to draw a rotated circle (circle doesn't change, just position)
static void render_draw_circle_rotated(RenderContext *ctx, float cx, float cy,
                                        float dx, float dy, float r, int rotation) {
    float rx, ry;
    rotate_point(dx, dy, rotation, &rx, &ry);
    render_draw_circle(ctx, cx + rx, cy + ry, r);
}

void render_ground(RenderContext *ctx, float x, float y, int rotation) {
    // Terminal at (0, -20)
    render_draw_line_rotated(ctx, x, y, 0, -20, 0, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -15, 0, 15, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -10, 6, 10, 6, rotation);
    render_draw_line_rotated(ctx, x, y, -5, 12, 5, 12, rotation);
}

void render_voltage_source(RenderContext *ctx, float x, float y, int rotation, bool is_ac) {
    // Terminals at (0, -40) and (0, 40)
    render_draw_circle(ctx, x, y, 18);
    render_draw_line_rotated(ctx, x, y, 0, -18, 0, -40, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 18, 0, 40, rotation);

    if (is_ac) {
        // Sine wave symbol (rotated)
        for (int i = 0; i < 20; i++) {
            float dx1 = -10 + i;
            float dx2 = -10 + i + 1;
            float dy1 = 8 * sin((i / 20.0) * 2 * M_PI);
            float dy2 = 8 * sin(((i + 1) / 20.0) * 2 * M_PI);
            render_draw_line_rotated(ctx, x, y, dx1, dy1, dx2, dy2, rotation);
        }
    } else {
        // + and - symbols
        render_draw_line_rotated(ctx, x, y, -5, -8, 5, -8, rotation);
        render_draw_line_rotated(ctx, x, y, 0, -13, 0, -3, rotation);
        render_draw_line_rotated(ctx, x, y, -5, 8, 5, 8, rotation);
    }
}

void render_current_source(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (0, -40) and (0, 40)
    render_draw_circle(ctx, x, y, 18);
    render_draw_line_rotated(ctx, x, y, 0, -18, 0, -40, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 18, 0, 40, rotation);
    // Arrow
    render_draw_line_rotated(ctx, x, y, 0, 10, 0, -10, rotation);
    render_draw_line_rotated(ctx, x, y, -5, -5, 0, -10, rotation);
    render_draw_line_rotated(ctx, x, y, 5, -5, 0, -10, rotation);
}

void render_resistor(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -28, 0, rotation);
    // Zigzag scaled to fit 80px width
    int points[][2] = {{-28,0},{-21,-8},{-7,8},{7,-8},{21,8},{28,0}};
    for (int i = 0; i < 5; i++) {
        render_draw_line_rotated(ctx, x, y, points[i][0], points[i][1],
                                 points[i+1][0], points[i+1][1], rotation);
    }
    render_draw_line_rotated(ctx, x, y, 28, 0, 40, 0, rotation);
}

void render_capacitor(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -6, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -6, -14, -6, 14, rotation);
    render_draw_line_rotated(ctx, x, y, 6, -14, 6, 14, rotation);
    render_draw_line_rotated(ctx, x, y, 6, 0, 40, 0, rotation);
}

void render_inductor(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -28, 0, rotation);
    // Coils scaled to fit
    for (int i = 0; i < 4; i++) {
        float coil_cx = -21 + i * 14;
        for (int a = 180; a <= 360; a += 15) {
            float r = a * M_PI / 180;
            float r2 = (a + 15) * M_PI / 180;
            float dx1 = coil_cx + 7*cos(r);
            float dy1 = 7*sin(r);
            float dx2 = coil_cx + 7*cos(r2);
            float dy2 = 7*sin(r2);
            render_draw_line_rotated(ctx, x, y, dx1, dy1, dx2, dy2, rotation);
        }
    }
    render_draw_line_rotated(ctx, x, y, 28, 0, 40, 0, rotation);
}

void render_diode(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -10, 0, rotation);
    // Triangle
    render_draw_line_rotated(ctx, x, y, -10, -12, -10, 12, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -12, 10, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -10, 12, 10, 0, rotation);
    // Bar
    render_draw_line_rotated(ctx, x, y, 10, -12, 10, 12, rotation);
    render_draw_line_rotated(ctx, x, y, 10, 0, 40, 0, rotation);
}

void render_zener(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -10, 0, rotation);
    // Triangle
    render_draw_line_rotated(ctx, x, y, -10, -12, -10, 12, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -12, 10, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -10, 12, 10, 0, rotation);
    // Zener bar with bent ends (Z shape)
    render_draw_line_rotated(ctx, x, y, 10, -12, 10, 12, rotation);
    render_draw_line_rotated(ctx, x, y, 10, -12, 6, -12, rotation);   // Top bend
    render_draw_line_rotated(ctx, x, y, 10, 12, 14, 12, rotation);    // Bottom bend
    render_draw_line_rotated(ctx, x, y, 10, 0, 40, 0, rotation);
}

void render_schottky(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -10, 0, rotation);
    // Triangle
    render_draw_line_rotated(ctx, x, y, -10, -12, -10, 12, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -12, 10, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -10, 12, 10, 0, rotation);
    // Schottky bar with S-shaped ends
    render_draw_line_rotated(ctx, x, y, 10, -12, 10, 12, rotation);
    render_draw_line_rotated(ctx, x, y, 10, -12, 6, -12, rotation);   // Top horizontal
    render_draw_line_rotated(ctx, x, y, 6, -12, 6, -8, rotation);     // Top vertical
    render_draw_line_rotated(ctx, x, y, 10, 12, 14, 12, rotation);    // Bottom horizontal
    render_draw_line_rotated(ctx, x, y, 14, 12, 14, 8, rotation);     // Bottom vertical
    render_draw_line_rotated(ctx, x, y, 10, 0, 40, 0, rotation);
}

void render_led(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -10, 0, rotation);
    // Triangle (filled appearance with multiple lines)
    render_draw_line_rotated(ctx, x, y, -10, -12, -10, 12, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -12, 10, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -10, 12, 10, 0, rotation);
    // Bar
    render_draw_line_rotated(ctx, x, y, 10, -12, 10, 12, rotation);
    render_draw_line_rotated(ctx, x, y, 10, 0, 40, 0, rotation);
    // Light arrows (emission)
    render_draw_line_rotated(ctx, x, y, 2, -16, 8, -22, rotation);
    render_draw_line_rotated(ctx, x, y, 6, -16, 8, -22, rotation);
    render_draw_line_rotated(ctx, x, y, 8, -22, 5, -20, rotation);
    render_draw_line_rotated(ctx, x, y, 8, -22, 8, -18, rotation);
    render_draw_line_rotated(ctx, x, y, 8, -12, 14, -18, rotation);
    render_draw_line_rotated(ctx, x, y, 12, -12, 14, -18, rotation);
    render_draw_line_rotated(ctx, x, y, 14, -18, 11, -16, rotation);
    render_draw_line_rotated(ctx, x, y, 14, -18, 14, -14, rotation);
}

void render_capacitor_elec(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    // Positive side (left) - straight line
    render_draw_line_rotated(ctx, x, y, -40, 0, -5, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -5, -15, -5, 15, rotation);
    // Negative side (right) - curved plate (shown as multiple lines)
    render_draw_line_rotated(ctx, x, y, 5, -15, 5, -10, rotation);
    render_draw_line_rotated(ctx, x, y, 5, -10, 7, -5, rotation);
    render_draw_line_rotated(ctx, x, y, 7, -5, 7, 5, rotation);
    render_draw_line_rotated(ctx, x, y, 7, 5, 5, 10, rotation);
    render_draw_line_rotated(ctx, x, y, 5, 10, 5, 15, rotation);
    render_draw_line_rotated(ctx, x, y, 5, 0, 40, 0, rotation);
    // Plus sign near positive terminal
    render_draw_line_rotated(ctx, x, y, -25, -8, -19, -8, rotation);  // horizontal
    render_draw_line_rotated(ctx, x, y, -22, -11, -22, -5, rotation); // vertical
}

void render_bjt(RenderContext *ctx, float x, float y, int rotation, bool is_pnp, const char *label) {
    // Terminals: B at (-20, 0), C at (20, -20), E at (20, 20)
    render_draw_circle(ctx, x, y, 18);
    render_draw_line_rotated(ctx, x, y, -20, 0, -5, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -5, -10, -5, 10, rotation);
    render_draw_line_rotated(ctx, x, y, -5, -5, 12, -15, rotation);
    render_draw_line_rotated(ctx, x, y, 12, -15, 20, -20, rotation);
    render_draw_line_rotated(ctx, x, y, -5, 5, 12, 15, rotation);
    render_draw_line_rotated(ctx, x, y, 12, 15, 20, 20, rotation);

    // Draw arrow on emitter to distinguish NPN/PNP
    // Arrow is on the line from (-5, 5) to (12, 15)
    // Arrow direction: NPN = outward (away from base), PNP = inward (toward base)
    if (is_pnp) {
        // PNP: Arrow pointing toward base (at emitter near base)
        render_draw_line_rotated(ctx, x, y, 0, 7, -3, 10, rotation);
        render_draw_line_rotated(ctx, x, y, 0, 7, 3, 11, rotation);
    } else {
        // NPN: Arrow pointing outward (at emitter away from base)
        render_draw_line_rotated(ctx, x, y, 8, 12, 5, 8, rotation);
        render_draw_line_rotated(ctx, x, y, 8, 12, 11, 9, rotation);
    }

    // Draw type label
    if (label) {
        int sx, sy;
        render_world_to_screen(ctx, x, y - 28, &sx, &sy);
        Color label_color = {0x00, 0xff, 0xff, 0xff};  // Cyan
        render_draw_text(ctx, label, sx - 10, sy, label_color);
    }
}

void render_mosfet(RenderContext *ctx, float x, float y, int rotation, bool is_pmos, const char *label) {
    // Terminals: G at (-20, 0), D at (20, -20), S at (20, 20)
    render_draw_line_rotated(ctx, x, y, -20, 0, -8, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -8, -10, -8, 10, rotation);
    render_draw_line_rotated(ctx, x, y, -3, -12, -3, -4, rotation);
    render_draw_line_rotated(ctx, x, y, -3, -2, -3, 2, rotation);  // Center segment (body)
    render_draw_line_rotated(ctx, x, y, -3, 4, -3, 12, rotation);
    render_draw_line_rotated(ctx, x, y, -3, -8, 12, -8, rotation);
    render_draw_line_rotated(ctx, x, y, 12, -8, 12, -20, rotation);
    render_draw_line_rotated(ctx, x, y, 12, -20, 20, -20, rotation);
    render_draw_line_rotated(ctx, x, y, -3, 8, 12, 8, rotation);
    render_draw_line_rotated(ctx, x, y, 12, 8, 12, 20, rotation);
    render_draw_line_rotated(ctx, x, y, 12, 20, 20, 20, rotation);

    // Body connection line
    render_draw_line_rotated(ctx, x, y, -3, 0, 12, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 12, 0, 12, 8, rotation);

    // Arrow on body to distinguish NMOS/PMOS
    if (is_pmos) {
        // PMOS: Arrow pointing outward (away from channel) - on the body line
        render_draw_line_rotated(ctx, x, y, 3, 0, 0, -3, rotation);
        render_draw_line_rotated(ctx, x, y, 3, 0, 0, 3, rotation);
    } else {
        // NMOS: Arrow pointing inward (toward channel) - on the body line
        render_draw_line_rotated(ctx, x, y, 6, 0, 9, -3, rotation);
        render_draw_line_rotated(ctx, x, y, 6, 0, 9, 3, rotation);
    }

    // Draw type label
    if (label) {
        int sx, sy;
        render_world_to_screen(ctx, x, y - 28, &sx, &sy);
        Color label_color = {0x00, 0xff, 0xff, 0xff};  // Cyan
        render_draw_text(ctx, label, sx - 15, sy, label_color);
    }
}

void render_opamp(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals: - at (-40, -20), + at (-40, 20), OUT at (40, 0)
    // Triangle
    render_draw_line_rotated(ctx, x, y, -25, -30, -25, 30, rotation);
    render_draw_line_rotated(ctx, x, y, -25, -30, 30, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -25, 30, 30, 0, rotation);
    // Inputs
    render_draw_line_rotated(ctx, x, y, -40, -20, -25, -20, rotation);
    render_draw_line_rotated(ctx, x, y, -40, 20, -25, 20, rotation);
    // Output
    render_draw_line_rotated(ctx, x, y, 30, 0, 40, 0, rotation);

    // Draw - symbol for inverting input (top input at y=-20)
    render_draw_line_rotated(ctx, x, y, -20, -20, -12, -20, rotation);

    // Draw + symbol for non-inverting input (bottom input at y=+20)
    render_draw_line_rotated(ctx, x, y, -20, 20, -12, 20, rotation);  // horizontal
    render_draw_line_rotated(ctx, x, y, -16, 16, -16, 24, rotation);  // vertical
}

void render_opamp_flipped(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals: + at (-40, -20), - at (-40, 20), OUT at (40, 0)
    // Triangle (same as regular opamp)
    render_draw_line_rotated(ctx, x, y, -25, -30, -25, 30, rotation);
    render_draw_line_rotated(ctx, x, y, -25, -30, 30, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -25, 30, 30, 0, rotation);
    // Inputs
    render_draw_line_rotated(ctx, x, y, -40, -20, -25, -20, rotation);
    render_draw_line_rotated(ctx, x, y, -40, 20, -25, 20, rotation);
    // Output
    render_draw_line_rotated(ctx, x, y, 30, 0, 40, 0, rotation);

    // Draw + symbol for non-inverting input (TOP input at y=-20)
    render_draw_line_rotated(ctx, x, y, -20, -20, -12, -20, rotation);  // horizontal
    render_draw_line_rotated(ctx, x, y, -16, -24, -16, -16, rotation);  // vertical

    // Draw - symbol for inverting input (BOTTOM input at y=+20)
    render_draw_line_rotated(ctx, x, y, -20, 20, -12, 20, rotation);
}

void render_square_wave(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (0, -40) and (0, 40), same as voltage sources
    render_draw_circle(ctx, x, y, 18);
    render_draw_line_rotated(ctx, x, y, 0, -18, 0, -40, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 18, 0, 40, rotation);

    // Square wave symbol inside circle
    render_draw_line_rotated(ctx, x, y, -10, 6, -10, -6, rotation);   // left edge up
    render_draw_line_rotated(ctx, x, y, -10, -6, -3, -6, rotation);   // top left
    render_draw_line_rotated(ctx, x, y, -3, -6, -3, 6, rotation);     // down
    render_draw_line_rotated(ctx, x, y, -3, 6, 3, 6, rotation);       // bottom middle
    render_draw_line_rotated(ctx, x, y, 3, 6, 3, -6, rotation);       // up
    render_draw_line_rotated(ctx, x, y, 3, -6, 10, -6, rotation);     // top right
    render_draw_line_rotated(ctx, x, y, 10, -6, 10, 6, rotation);     // right edge down
}

void render_triangle_wave(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (0, -40) and (0, 40)
    render_draw_circle(ctx, x, y, 18);
    render_draw_line_rotated(ctx, x, y, 0, -18, 0, -40, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 18, 0, 40, rotation);

    // Triangle wave symbol inside circle
    render_draw_line_rotated(ctx, x, y, -10, 6, -5, -6, rotation);    // up slope
    render_draw_line_rotated(ctx, x, y, -5, -6, 0, 6, rotation);      // down slope
    render_draw_line_rotated(ctx, x, y, 0, 6, 5, -6, rotation);       // up slope
    render_draw_line_rotated(ctx, x, y, 5, -6, 10, 6, rotation);      // down slope
}

void render_sawtooth_wave(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (0, -40) and (0, 40)
    render_draw_circle(ctx, x, y, 18);
    render_draw_line_rotated(ctx, x, y, 0, -18, 0, -40, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 18, 0, 40, rotation);

    // Sawtooth wave symbol inside circle
    render_draw_line_rotated(ctx, x, y, -10, 6, -3, -6, rotation);    // ramp up
    render_draw_line_rotated(ctx, x, y, -3, -6, -3, 6, rotation);     // drop down
    render_draw_line_rotated(ctx, x, y, -3, 6, 4, -6, rotation);      // ramp up
    render_draw_line_rotated(ctx, x, y, 4, -6, 4, 6, rotation);       // drop down
    render_draw_line_rotated(ctx, x, y, 4, 6, 10, -2, rotation);      // partial ramp
}

void render_noise_source(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (0, -40) and (0, 40)
    render_draw_circle(ctx, x, y, 18);
    render_draw_line_rotated(ctx, x, y, 0, -18, 0, -40, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 18, 0, 40, rotation);

    // Random-looking noise symbol inside circle
    render_draw_line_rotated(ctx, x, y, -10, 0, -8, -4, rotation);
    render_draw_line_rotated(ctx, x, y, -8, -4, -6, 6, rotation);
    render_draw_line_rotated(ctx, x, y, -6, 6, -4, -2, rotation);
    render_draw_line_rotated(ctx, x, y, -4, -2, -2, 4, rotation);
    render_draw_line_rotated(ctx, x, y, -2, 4, 0, -6, rotation);
    render_draw_line_rotated(ctx, x, y, 0, -6, 2, 3, rotation);
    render_draw_line_rotated(ctx, x, y, 2, 3, 4, -4, rotation);
    render_draw_line_rotated(ctx, x, y, 4, -4, 6, 5, rotation);
    render_draw_line_rotated(ctx, x, y, 6, 5, 8, -3, rotation);
    render_draw_line_rotated(ctx, x, y, 8, -3, 10, 1, rotation);
}

void render_spst_switch(RenderContext *ctx, float x, float y, int rotation, bool closed) {
    // Terminals at (-40, 0) and (40, 0)
    // Draw terminal leads
    render_draw_line_rotated(ctx, x, y, -40, 0, -15, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 15, 0, 40, 0, rotation);

    // Draw switch contacts (small circles at pivot points)
    render_draw_circle_rotated(ctx, x, y, -15, 0, 3, rotation);
    render_draw_circle_rotated(ctx, x, y, 15, 0, 3, rotation);

    // Draw the actuator (switch blade)
    if (closed) {
        // Closed: horizontal blade connecting the contacts
        render_draw_line_rotated(ctx, x, y, -15, 0, 15, 0, rotation);
    } else {
        // Open: blade tilted up at 30 degrees
        render_draw_line_rotated(ctx, x, y, -15, 0, 12, -12, rotation);
    }
}

void render_spdt_switch(RenderContext *ctx, float x, float y, int rotation, int position) {
    // Terminals: Common at (-40, 0), A at (40, -20), B at (40, 20)
    // Draw terminal leads
    render_draw_line_rotated(ctx, x, y, -40, 0, -15, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 15, -20, 40, -20, rotation);
    render_draw_line_rotated(ctx, x, y, 15, 20, 40, 20, rotation);

    // Draw switch contacts
    render_draw_circle_rotated(ctx, x, y, -15, 0, 3, rotation);
    render_draw_circle_rotated(ctx, x, y, 15, -20, 3, rotation);
    render_draw_circle_rotated(ctx, x, y, 15, 20, 3, rotation);

    // Draw the actuator pointing to selected terminal
    if (position == 0) {
        // Position A (up)
        render_draw_line_rotated(ctx, x, y, -15, 0, 12, -18, rotation);
    } else {
        // Position B (down)
        render_draw_line_rotated(ctx, x, y, -15, 0, 12, 18, rotation);
    }
}

void render_push_button(RenderContext *ctx, float x, float y, int rotation, bool pressed) {
    // Terminals at (-40, 0) and (40, 0)
    // Draw terminal leads
    render_draw_line_rotated(ctx, x, y, -40, 0, -15, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 15, 0, 40, 0, rotation);

    // Draw the button outline (rectangle)
    render_draw_line_rotated(ctx, x, y, -15, -8, 15, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -15, 8, 15, 8, rotation);
    render_draw_line_rotated(ctx, x, y, -15, -8, -15, 8, rotation);
    render_draw_line_rotated(ctx, x, y, 15, -8, 15, 8, rotation);

    // Draw the actuator inside
    if (pressed) {
        // Pressed: horizontal line (contacts closed)
        render_draw_line_rotated(ctx, x, y, -12, 0, 12, 0, rotation);
        // Draw button pushed down
        render_draw_line_rotated(ctx, x, y, -8, -8, -8, -3, rotation);
        render_draw_line_rotated(ctx, x, y, 8, -8, 8, -3, rotation);
        render_draw_line_rotated(ctx, x, y, -8, -3, 8, -3, rotation);
    } else {
        // Not pressed: gap in middle
        render_draw_line_rotated(ctx, x, y, -12, 0, -3, 0, rotation);
        render_draw_line_rotated(ctx, x, y, 3, 0, 12, 0, rotation);
        // Draw button in normal position
        render_draw_line_rotated(ctx, x, y, -8, -8, -8, -6, rotation);
        render_draw_line_rotated(ctx, x, y, 8, -8, 8, -6, rotation);
        render_draw_line_rotated(ctx, x, y, -8, -6, 8, -6, rotation);
    }
}

void render_transformer(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals: P1 at (-50, -20), P2 at (-50, 20), S1 at (50, -20), S2 at (50, 20)

    // Primary winding leads
    render_draw_line_rotated(ctx, x, y, -50, -20, -30, -20, rotation);
    render_draw_line_rotated(ctx, x, y, -50, 20, -30, 20, rotation);

    // Primary coil (4 half-circles on left side)
    for (int i = 0; i < 4; i++) {
        float coil_cy = -15 + i * 10;
        for (int a = 90; a <= 270; a += 15) {
            float r1 = a * M_PI / 180;
            float r2 = (a + 15) * M_PI / 180;
            float dx1 = -20 + 5 * cos(r1);
            float dy1 = coil_cy + 5 * sin(r1);
            float dx2 = -20 + 5 * cos(r2);
            float dy2 = coil_cy + 5 * sin(r2);
            render_draw_line_rotated(ctx, x, y, dx1, dy1, dx2, dy2, rotation);
        }
    }

    // Core lines (two vertical lines in the middle)
    render_draw_line_rotated(ctx, x, y, -8, -25, -8, 25, rotation);
    render_draw_line_rotated(ctx, x, y, 8, -25, 8, 25, rotation);

    // Secondary coil (4 half-circles on right side)
    for (int i = 0; i < 4; i++) {
        float coil_cy = -15 + i * 10;
        for (int a = -90; a <= 90; a += 15) {
            float r1 = a * M_PI / 180;
            float r2 = (a + 15) * M_PI / 180;
            float dx1 = 20 + 5 * cos(r1);
            float dy1 = coil_cy + 5 * sin(r1);
            float dx2 = 20 + 5 * cos(r2);
            float dy2 = coil_cy + 5 * sin(r2);
            render_draw_line_rotated(ctx, x, y, dx1, dy1, dx2, dy2, rotation);
        }
    }

    // Secondary winding leads
    render_draw_line_rotated(ctx, x, y, 30, -20, 50, -20, rotation);
    render_draw_line_rotated(ctx, x, y, 30, 20, 50, 20, rotation);
}

void render_transformer_ct(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals: P1 at (-50, -20), P2 at (-50, 20), S1 at (50, -30), CT at (50, 0), S2 at (50, 30)

    // Primary winding leads
    render_draw_line_rotated(ctx, x, y, -50, -20, -30, -20, rotation);
    render_draw_line_rotated(ctx, x, y, -50, 20, -30, 20, rotation);

    // Primary coil (4 half-circles on left side)
    for (int i = 0; i < 4; i++) {
        float coil_cy = -15 + i * 10;
        for (int a = 90; a <= 270; a += 15) {
            float r1 = a * M_PI / 180;
            float r2 = (a + 15) * M_PI / 180;
            float dx1 = -20 + 5 * cos(r1);
            float dy1 = coil_cy + 5 * sin(r1);
            float dx2 = -20 + 5 * cos(r2);
            float dy2 = coil_cy + 5 * sin(r2);
            render_draw_line_rotated(ctx, x, y, dx1, dy1, dx2, dy2, rotation);
        }
    }

    // Core lines (two vertical lines in the middle)
    render_draw_line_rotated(ctx, x, y, -8, -35, -8, 35, rotation);
    render_draw_line_rotated(ctx, x, y, 8, -35, 8, 35, rotation);

    // Secondary coil - top half (2 half-circles)
    for (int i = 0; i < 2; i++) {
        float coil_cy = -25 + i * 10;
        for (int a = -90; a <= 90; a += 15) {
            float r1 = a * M_PI / 180;
            float r2 = (a + 15) * M_PI / 180;
            float dx1 = 20 + 5 * cos(r1);
            float dy1 = coil_cy + 5 * sin(r1);
            float dx2 = 20 + 5 * cos(r2);
            float dy2 = coil_cy + 5 * sin(r2);
            render_draw_line_rotated(ctx, x, y, dx1, dy1, dx2, dy2, rotation);
        }
    }

    // Center tap connection
    render_draw_line_rotated(ctx, x, y, 25, 0, 50, 0, rotation);

    // Secondary coil - bottom half (2 half-circles)
    for (int i = 0; i < 2; i++) {
        float coil_cy = 15 + i * 10;
        for (int a = -90; a <= 90; a += 15) {
            float r1 = a * M_PI / 180;
            float r2 = (a + 15) * M_PI / 180;
            float dx1 = 20 + 5 * cos(r1);
            float dy1 = coil_cy + 5 * sin(r1);
            float dx2 = 20 + 5 * cos(r2);
            float dy2 = coil_cy + 5 * sin(r2);
            render_draw_line_rotated(ctx, x, y, dx1, dy1, dx2, dy2, rotation);
        }
    }

    // Secondary winding leads
    render_draw_line_rotated(ctx, x, y, 25, -30, 50, -30, rotation);
    render_draw_line_rotated(ctx, x, y, 25, 30, 50, 30, rotation);
}

void render_ghost_component(RenderContext *ctx, Component *comp) {
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    render_set_color(ctx, (Color){0xff, 0xff, 0xff, 0x80});
    render_component(ctx, comp);
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_NONE);
}

void render_wire_preview(RenderContext *ctx, float x1, float y1, float x2, float y2) {
    render_set_color(ctx, COLOR_WARNING);
    // Dashed line effect
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrt(dx*dx + dy*dy);
    if (len < 1) return;

    dx /= len; dy /= len;
    float pos = 0;
    bool draw = true;

    while (pos < len) {
        float seg_len = MIN(5, len - pos);
        if (draw) {
            render_draw_line(ctx, x1 + dx*pos, y1 + dy*pos,
                            x1 + dx*(pos+seg_len), y1 + dy*(pos+seg_len));
        }
        pos += 5;
        draw = !draw;
    }
}

void render_selection_box(RenderContext *ctx, float x1, float y1, float x2, float y2) {
    // Draw dashed rectangle for selection box
    render_set_color(ctx, COLOR_ACCENT);

    // Normalize coordinates
    float min_x = fminf(x1, x2);
    float max_x = fmaxf(x1, x2);
    float min_y = fminf(y1, y2);
    float max_y = fmaxf(y1, y2);

    // Draw the four edges with dashed lines
    float dash_len = 5.0f;

    // Top edge
    for (float x = min_x; x < max_x; x += dash_len * 2) {
        float x_end = fminf(x + dash_len, max_x);
        render_draw_line(ctx, x, min_y, x_end, min_y);
    }
    // Bottom edge
    for (float x = min_x; x < max_x; x += dash_len * 2) {
        float x_end = fminf(x + dash_len, max_x);
        render_draw_line(ctx, x, max_y, x_end, max_y);
    }
    // Left edge
    for (float y = min_y; y < max_y; y += dash_len * 2) {
        float y_end = fminf(y + dash_len, max_y);
        render_draw_line(ctx, min_x, y, min_x, y_end);
    }
    // Right edge
    for (float y = min_y; y < max_y; y += dash_len * 2) {
        float y_end = fminf(y + dash_len, max_y);
        render_draw_line(ctx, max_x, y, max_x, y_end);
    }

    // Fill with semi-transparent color
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ctx->renderer, 0, 217, 255, 40);  // Light cyan, very transparent

    int sx1, sy1, sx2, sy2;
    render_world_to_screen(ctx, min_x, min_y, &sx1, &sy1);
    render_world_to_screen(ctx, max_x, max_y, &sx2, &sy2);

    SDL_Rect rect = {sx1, sy1, sx2 - sx1, sy2 - sy1};
    SDL_RenderFillRect(ctx->renderer, &rect);
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_NONE);
}

// ============================================================================
// NEW COMPONENT SYMBOLS
// ============================================================================

// Fuse - rectangle with S-curve inside
void render_fuse(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -20, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 20, 0, 40, 0, rotation);
    // Rectangle body
    render_draw_line_rotated(ctx, x, y, -20, -8, 20, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -20, 8, 20, 8, rotation);
    render_draw_line_rotated(ctx, x, y, -20, -8, -20, 8, rotation);
    render_draw_line_rotated(ctx, x, y, 20, -8, 20, 8, rotation);
    // S-curve fuse element inside
    render_draw_line_rotated(ctx, x, y, -15, 0, -10, -4, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -4, -5, 4, rotation);
    render_draw_line_rotated(ctx, x, y, -5, 4, 0, -4, rotation);
    render_draw_line_rotated(ctx, x, y, 0, -4, 5, 4, rotation);
    render_draw_line_rotated(ctx, x, y, 5, 4, 10, -4, rotation);
    render_draw_line_rotated(ctx, x, y, 10, -4, 15, 0, rotation);
}

// Crystal oscillator - rectangle between two capacitor plates
void render_crystal(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -15, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 15, 0, 40, 0, rotation);
    // Left plate (vertical line)
    render_draw_line_rotated(ctx, x, y, -15, -12, -15, 12, rotation);
    // Crystal body (rectangle)
    render_draw_line_rotated(ctx, x, y, -10, -8, 10, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -10, 8, 10, 8, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -8, -10, 8, rotation);
    render_draw_line_rotated(ctx, x, y, 10, -8, 10, 8, rotation);
    // Right plate (vertical line)
    render_draw_line_rotated(ctx, x, y, 15, -12, 15, 12, rotation);
}

// Spark gap - two angled electrodes with gap
void render_spark_gap(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -15, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 15, 0, 40, 0, rotation);
    // Left electrode (angled)
    render_draw_line_rotated(ctx, x, y, -15, 0, -8, -10, rotation);
    render_draw_line_rotated(ctx, x, y, -15, 0, -8, 10, rotation);
    render_draw_line_rotated(ctx, x, y, -8, -10, -5, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -8, 10, -5, 0, rotation);
    // Right electrode (angled)
    render_draw_line_rotated(ctx, x, y, 15, 0, 8, -10, rotation);
    render_draw_line_rotated(ctx, x, y, 15, 0, 8, 10, rotation);
    render_draw_line_rotated(ctx, x, y, 8, -10, 5, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 8, 10, 5, 0, rotation);
}

// Potentiometer - resistor with arrow (wiper)
void render_potentiometer(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0), (40, 0), and (0, -20) for wiper
    // Draw resistor body
    render_draw_line_rotated(ctx, x, y, -40, 0, -28, 0, rotation);
    int points[][2] = {{-28,0},{-21,-8},{-7,8},{7,-8},{21,8},{28,0}};
    for (int i = 0; i < 5; i++) {
        render_draw_line_rotated(ctx, x, y, points[i][0], points[i][1],
                                 points[i+1][0], points[i+1][1], rotation);
    }
    render_draw_line_rotated(ctx, x, y, 28, 0, 40, 0, rotation);
    // Wiper arrow pointing down at resistor
    render_draw_line_rotated(ctx, x, y, 0, -20, 0, -5, rotation);
    render_draw_line_rotated(ctx, x, y, -4, -10, 0, -5, rotation);
    render_draw_line_rotated(ctx, x, y, 4, -10, 0, -5, rotation);
}

// Photoresistor (LDR) - resistor with light arrows
void render_photoresistor(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    // Draw resistor body
    render_draw_line_rotated(ctx, x, y, -40, 0, -28, 0, rotation);
    int points[][2] = {{-28,0},{-21,-8},{-7,8},{7,-8},{21,8},{28,0}};
    for (int i = 0; i < 5; i++) {
        render_draw_line_rotated(ctx, x, y, points[i][0], points[i][1],
                                 points[i+1][0], points[i+1][1], rotation);
    }
    render_draw_line_rotated(ctx, x, y, 28, 0, 40, 0, rotation);
    // Light arrows pointing at resistor (from top-left)
    render_draw_line_rotated(ctx, x, y, -20, -20, -10, -10, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -10, -13, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -10, -8, -13, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -20, 0, -10, rotation);
    render_draw_line_rotated(ctx, x, y, 0, -10, -3, -8, rotation);
    render_draw_line_rotated(ctx, x, y, 0, -10, 2, -13, rotation);
}

// Thermistor - resistor with T symbol
void render_thermistor(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    // Draw resistor body
    render_draw_line_rotated(ctx, x, y, -40, 0, -28, 0, rotation);
    int points[][2] = {{-28,0},{-21,-8},{-7,8},{7,-8},{21,8},{28,0}};
    for (int i = 0; i < 5; i++) {
        render_draw_line_rotated(ctx, x, y, points[i][0], points[i][1],
                                 points[i+1][0], points[i+1][1], rotation);
    }
    render_draw_line_rotated(ctx, x, y, 28, 0, 40, 0, rotation);
    // Diagonal line through (temperature coefficient indicator)
    render_draw_line_rotated(ctx, x, y, -25, 15, 25, -15, rotation);
    // Small "t" or temperature mark
    render_draw_line_rotated(ctx, x, y, 20, -12, 20, -5, rotation);
    render_draw_line_rotated(ctx, x, y, 17, -9, 23, -9, rotation);
}

// Memristor - rectangle with thick black band on one side
void render_memristor(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -20, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 20, 0, 40, 0, rotation);
    // Rectangle body
    render_draw_line_rotated(ctx, x, y, -20, -10, 20, -10, rotation);
    render_draw_line_rotated(ctx, x, y, -20, 10, 20, 10, rotation);
    render_draw_line_rotated(ctx, x, y, -20, -10, -20, 10, rotation);
    render_draw_line_rotated(ctx, x, y, 20, -10, 20, 10, rotation);
    // Filled band on left side (multiple lines to simulate fill)
    for (int i = -9; i <= 9; i++) {
        render_draw_line_rotated(ctx, x, y, -20, i, -10, i, rotation);
    }
}

// Varactor - diode with capacitor symbol
void render_varactor(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -10, 0, rotation);
    // Diode triangle
    render_draw_line_rotated(ctx, x, y, -10, -12, -10, 12, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -12, 10, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -10, 12, 10, 0, rotation);
    // Cathode bar
    render_draw_line_rotated(ctx, x, y, 10, -12, 10, 12, rotation);
    // Extra line for capacitor effect
    render_draw_line_rotated(ctx, x, y, 15, -12, 15, 12, rotation);
    render_draw_line_rotated(ctx, x, y, 15, 0, 40, 0, rotation);
}

// Tunnel diode - diode with cathode bends
void render_tunnel_diode(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -10, 0, rotation);
    // Triangle
    render_draw_line_rotated(ctx, x, y, -10, -12, -10, 12, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -12, 10, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -10, 12, 10, 0, rotation);
    // Cathode bar with bends on both ends (inward)
    render_draw_line_rotated(ctx, x, y, 10, -12, 10, 12, rotation);
    render_draw_line_rotated(ctx, x, y, 10, -12, 14, -12, rotation);
    render_draw_line_rotated(ctx, x, y, 14, -12, 14, -8, rotation);
    render_draw_line_rotated(ctx, x, y, 10, 12, 14, 12, rotation);
    render_draw_line_rotated(ctx, x, y, 14, 12, 14, 8, rotation);
    render_draw_line_rotated(ctx, x, y, 10, 0, 40, 0, rotation);
}

// Photodiode - diode with light arrows pointing at it
void render_photodiode(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -10, 0, rotation);
    // Triangle
    render_draw_line_rotated(ctx, x, y, -10, -12, -10, 12, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -12, 10, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -10, 12, 10, 0, rotation);
    // Bar
    render_draw_line_rotated(ctx, x, y, 10, -12, 10, 12, rotation);
    render_draw_line_rotated(ctx, x, y, 10, 0, 40, 0, rotation);
    // Light arrows pointing at diode (incoming light)
    render_draw_line_rotated(ctx, x, y, -20, -22, -8, -14, rotation);
    render_draw_line_rotated(ctx, x, y, -8, -14, -12, -14, rotation);
    render_draw_line_rotated(ctx, x, y, -8, -14, -8, -18, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -22, 2, -14, rotation);
    render_draw_line_rotated(ctx, x, y, 2, -14, -2, -14, rotation);
    render_draw_line_rotated(ctx, x, y, 2, -14, 2, -18, rotation);
}

// SCR (Silicon Controlled Rectifier) - diode with gate
void render_scr(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals: Anode at (-40, 0), Cathode at (40, 0), Gate at (0, 20)
    render_draw_line_rotated(ctx, x, y, -40, 0, -10, 0, rotation);
    // Triangle (pointing right)
    render_draw_line_rotated(ctx, x, y, -10, -12, -10, 12, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -12, 10, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -10, 12, 10, 0, rotation);
    // Bar
    render_draw_line_rotated(ctx, x, y, 10, -12, 10, 12, rotation);
    render_draw_line_rotated(ctx, x, y, 10, 0, 40, 0, rotation);
    // Gate connection from cathode
    render_draw_line_rotated(ctx, x, y, 0, 6, 0, 20, rotation);
}

// DIAC - two back-to-back diodes
void render_diac(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -15, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 15, 0, 40, 0, rotation);
    // First diode (pointing right)
    render_draw_line_rotated(ctx, x, y, -15, -10, -15, 10, rotation);
    render_draw_line_rotated(ctx, x, y, -15, -10, 0, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -15, 10, 0, 0, rotation);
    // Second diode (pointing left)
    render_draw_line_rotated(ctx, x, y, 15, -10, 15, 10, rotation);
    render_draw_line_rotated(ctx, x, y, 15, -10, 0, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 15, 10, 0, 0, rotation);
    // Center bar
    render_draw_line_rotated(ctx, x, y, 0, -10, 0, 10, rotation);
}

// TRIAC - bidirectional thyristor with gate
void render_triac(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals: MT1 at (-40, 0), MT2 at (40, 0), Gate at (0, 20)
    render_draw_line_rotated(ctx, x, y, -40, 0, -15, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 15, 0, 40, 0, rotation);
    // First triangle (pointing right)
    render_draw_line_rotated(ctx, x, y, -15, -10, -15, 10, rotation);
    render_draw_line_rotated(ctx, x, y, -15, -10, 0, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -15, 10, 0, 0, rotation);
    // Second triangle (pointing left)
    render_draw_line_rotated(ctx, x, y, 15, -10, 15, 10, rotation);
    render_draw_line_rotated(ctx, x, y, 15, -10, 0, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 15, 10, 0, 0, rotation);
    // Center bar
    render_draw_line_rotated(ctx, x, y, 0, -10, 0, 10, rotation);
    // Gate terminal
    render_draw_line_rotated(ctx, x, y, -8, 5, 0, 20, rotation);
}

// UJT (Unijunction Transistor)
void render_ujt(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals: E at (-20, 0), B1 at (20, 20), B2 at (20, -20)
    // Vertical bar (base region)
    render_draw_line_rotated(ctx, x, y, 0, -15, 0, 15, rotation);
    // Connections to B1 and B2
    render_draw_line_rotated(ctx, x, y, 0, -15, 20, -20, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 15, 20, 20, rotation);
    // Emitter with arrow
    render_draw_line_rotated(ctx, x, y, -20, 0, -5, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -5, 0, 0, 5, rotation);
    // Arrow on emitter
    render_draw_line_rotated(ctx, x, y, -3, 3, 0, 5, rotation);
    render_draw_line_rotated(ctx, x, y, -5, 0, -3, 3, rotation);
}

// N-channel JFET
void render_njfet(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals: G at (-20, 0), D at (20, -20), S at (20, 20)
    // Channel (vertical bar)
    render_draw_line_rotated(ctx, x, y, 0, -15, 0, 15, rotation);
    // Drain and Source connections
    render_draw_line_rotated(ctx, x, y, 0, -15, 0, -20, rotation);
    render_draw_line_rotated(ctx, x, y, 0, -20, 20, -20, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 15, 0, 20, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 20, 20, 20, rotation);
    // Gate with arrow pointing inward (N-channel)
    render_draw_line_rotated(ctx, x, y, -20, 0, -5, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -5, 0, 0, 0, rotation);
    // Arrow pointing toward channel
    render_draw_line_rotated(ctx, x, y, -8, -3, -5, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -8, 3, -5, 0, rotation);
    // Type label
    int sx, sy;
    render_world_to_screen(ctx, x, y - 28, &sx, &sy);
    Color label_color = {0x00, 0xff, 0xff, 0xff};
    render_draw_text(ctx, "NJFET", sx - 15, sy, label_color);
}

// P-channel JFET
void render_pjfet(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals: G at (-20, 0), D at (20, -20), S at (20, 20)
    // Channel (vertical bar)
    render_draw_line_rotated(ctx, x, y, 0, -15, 0, 15, rotation);
    // Drain and Source connections
    render_draw_line_rotated(ctx, x, y, 0, -15, 0, -20, rotation);
    render_draw_line_rotated(ctx, x, y, 0, -20, 20, -20, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 15, 0, 20, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 20, 20, 20, rotation);
    // Gate with arrow pointing outward (P-channel)
    render_draw_line_rotated(ctx, x, y, -20, 0, 0, 0, rotation);
    // Arrow pointing away from channel
    render_draw_line_rotated(ctx, x, y, -12, -3, -8, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -12, 3, -8, 0, rotation);
    // Type label
    int sx, sy;
    render_world_to_screen(ctx, x, y - 28, &sx, &sy);
    Color label_color = {0x00, 0xff, 0xff, 0xff};
    render_draw_text(ctx, "PJFET", sx - 15, sy, label_color);
}

// Darlington transistor (NPN)
void render_darlington_npn(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals: B at (-20, 0), C at (20, -20), E at (20, 20)
    render_draw_circle(ctx, x, y, 22);
    // First BJT (smaller, left)
    render_draw_line_rotated(ctx, x, y, -20, 0, -10, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -6, -10, 6, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -3, -2, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -10, 3, -2, 8, rotation);
    // Second BJT (connected to first)
    render_draw_line_rotated(ctx, x, y, -2, 8, 5, 8, rotation);
    render_draw_line_rotated(ctx, x, y, 5, 2, 5, 14, rotation);
    render_draw_line_rotated(ctx, x, y, 5, 5, 12, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 5, 11, 12, 16, rotation);
    // Collector connection (from first BJT)
    render_draw_line_rotated(ctx, x, y, -2, -8, 12, -8, rotation);
    render_draw_line_rotated(ctx, x, y, 12, -8, 12, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 12, 0, 20, -20, rotation);
    // Emitter connection (from second BJT)
    render_draw_line_rotated(ctx, x, y, 12, 16, 20, 20, rotation);
    // Arrow on final emitter
    render_draw_line_rotated(ctx, x, y, 10, 14, 8, 11, rotation);
    render_draw_line_rotated(ctx, x, y, 10, 14, 13, 12, rotation);
    // Type label
    int sx, sy;
    render_world_to_screen(ctx, x, y - 32, &sx, &sy);
    Color label_color = {0x00, 0xff, 0xff, 0xff};
    render_draw_text(ctx, "NPN-D", sx - 15, sy, label_color);
}

// Darlington transistor (PNP)
void render_darlington_pnp(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals: B at (-20, 0), C at (20, -20), E at (20, 20)
    render_draw_circle(ctx, x, y, 22);
    // First BJT (smaller, left)
    render_draw_line_rotated(ctx, x, y, -20, 0, -10, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -6, -10, 6, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -3, -2, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -10, 3, -2, 8, rotation);
    // Second BJT (connected to first)
    render_draw_line_rotated(ctx, x, y, -2, 8, 5, 8, rotation);
    render_draw_line_rotated(ctx, x, y, 5, 2, 5, 14, rotation);
    render_draw_line_rotated(ctx, x, y, 5, 5, 12, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 5, 11, 12, 16, rotation);
    // Collector connection (from first BJT)
    render_draw_line_rotated(ctx, x, y, -2, -8, 12, -8, rotation);
    render_draw_line_rotated(ctx, x, y, 12, -8, 12, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 12, 0, 20, -20, rotation);
    // Emitter connection (from second BJT)
    render_draw_line_rotated(ctx, x, y, 12, 16, 20, 20, rotation);
    // Arrow on emitter pointing inward (PNP)
    render_draw_line_rotated(ctx, x, y, 7, 10, 9, 7, rotation);
    render_draw_line_rotated(ctx, x, y, 7, 10, 4, 8, rotation);
    // Type label
    int sx, sy;
    render_world_to_screen(ctx, x, y - 32, &sx, &sy);
    Color label_color = {0x00, 0xff, 0xff, 0xff};
    render_draw_text(ctx, "PNP-D", sx - 15, sy, label_color);
}

// Real op-amp (with finite gain indicator)
void render_opamp_real(RenderContext *ctx, float x, float y, int rotation) {
    // Same as regular op-amp but with "A" inside
    render_draw_line_rotated(ctx, x, y, -25, -30, -25, 30, rotation);
    render_draw_line_rotated(ctx, x, y, -25, -30, 30, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -25, 30, 30, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -40, -20, -25, -20, rotation);
    render_draw_line_rotated(ctx, x, y, -40, 20, -25, 20, rotation);
    render_draw_line_rotated(ctx, x, y, 30, 0, 40, 0, rotation);
    // - and + symbols
    render_draw_line_rotated(ctx, x, y, -20, -20, -12, -20, rotation);
    render_draw_line_rotated(ctx, x, y, -20, 20, -12, 20, rotation);
    render_draw_line_rotated(ctx, x, y, -16, 16, -16, 24, rotation);
    // "A" symbol inside to indicate finite gain
    render_draw_line_rotated(ctx, x, y, -5, 8, 0, -2, rotation);
    render_draw_line_rotated(ctx, x, y, 0, -2, 5, 8, rotation);
    render_draw_line_rotated(ctx, x, y, -2, 4, 2, 4, rotation);
}

// OTA (Operational Transconductance Amplifier)
void render_ota(RenderContext *ctx, float x, float y, int rotation) {
    // Same as op-amp but with "gm" or diamond inside
    render_draw_line_rotated(ctx, x, y, -25, -30, -25, 30, rotation);
    render_draw_line_rotated(ctx, x, y, -25, -30, 30, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -25, 30, 30, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -40, -20, -25, -20, rotation);
    render_draw_line_rotated(ctx, x, y, -40, 20, -25, 20, rotation);
    render_draw_line_rotated(ctx, x, y, 30, 0, 40, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -20, -20, -12, -20, rotation);
    render_draw_line_rotated(ctx, x, y, -20, 20, -12, 20, rotation);
    render_draw_line_rotated(ctx, x, y, -16, 16, -16, 24, rotation);
    // Small diamond inside
    render_draw_line_rotated(ctx, x, y, 0, -6, 6, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 6, 0, 0, 6, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 6, -6, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -6, 0, 0, -6, rotation);
}

// CCII (Current Conveyor) - generic symbol
void render_ccii(RenderContext *ctx, float x, float y, int rotation, bool is_plus) {
    // Box with terminals
    render_draw_line_rotated(ctx, x, y, -25, -25, 25, -25, rotation);
    render_draw_line_rotated(ctx, x, y, -25, 25, 25, 25, rotation);
    render_draw_line_rotated(ctx, x, y, -25, -25, -25, 25, rotation);
    render_draw_line_rotated(ctx, x, y, 25, -25, 25, 25, rotation);
    // Terminals: X at (-40, 0), Y at (0, -40), Z at (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -25, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 0, -40, 0, -25, rotation);
    render_draw_line_rotated(ctx, x, y, 25, 0, 40, 0, rotation);
    // Labels inside (simplified)
    // X label position
    render_draw_line_rotated(ctx, x, y, -20, -3, -17, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -17, 0, -20, 3, rotation);
    render_draw_line_rotated(ctx, x, y, -17, 0, -14, -3, rotation);
    render_draw_line_rotated(ctx, x, y, -17, 0, -14, 3, rotation);
    // + or - for polarity
    if (is_plus) {
        render_draw_line_rotated(ctx, x, y, 15, -3, 15, 3, rotation);
    }
    render_draw_line_rotated(ctx, x, y, 12, 0, 18, 0, rotation);
}

// VCVS (Voltage-Controlled Voltage Source) - diamond
void render_vcvs(RenderContext *ctx, float x, float y, int rotation) {
    // Diamond shape with + - inside
    render_draw_line_rotated(ctx, x, y, 0, -20, 20, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 20, 0, 0, 20, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 20, -20, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -20, 0, 0, -20, rotation);
    // Terminals at corners
    render_draw_line_rotated(ctx, x, y, 0, -20, 0, -40, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 20, 0, 40, rotation);
    // + and - inside
    render_draw_line_rotated(ctx, x, y, -3, -8, 3, -8, rotation);
    render_draw_line_rotated(ctx, x, y, 0, -11, 0, -5, rotation);
    render_draw_line_rotated(ctx, x, y, -3, 8, 3, 8, rotation);
}

// VCCS (Voltage-Controlled Current Source) - diamond with arrow
void render_vccs(RenderContext *ctx, float x, float y, int rotation) {
    // Diamond shape
    render_draw_line_rotated(ctx, x, y, 0, -20, 20, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 20, 0, 0, 20, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 20, -20, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -20, 0, 0, -20, rotation);
    // Terminals
    render_draw_line_rotated(ctx, x, y, 0, -20, 0, -40, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 20, 0, 40, rotation);
    // Current arrow inside
    render_draw_line_rotated(ctx, x, y, 0, 10, 0, -10, rotation);
    render_draw_line_rotated(ctx, x, y, -4, -5, 0, -10, rotation);
    render_draw_line_rotated(ctx, x, y, 4, -5, 0, -10, rotation);
}

// CCVS (Current-Controlled Voltage Source) - diamond
void render_ccvs(RenderContext *ctx, float x, float y, int rotation) {
    // Diamond with + - and "r" indicator
    render_draw_line_rotated(ctx, x, y, 0, -20, 20, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 20, 0, 0, 20, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 20, -20, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -20, 0, 0, -20, rotation);
    render_draw_line_rotated(ctx, x, y, 0, -20, 0, -40, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 20, 0, 40, rotation);
    // + and -
    render_draw_line_rotated(ctx, x, y, -3, -8, 3, -8, rotation);
    render_draw_line_rotated(ctx, x, y, 0, -11, 0, -5, rotation);
    render_draw_line_rotated(ctx, x, y, -3, 8, 3, 8, rotation);
    // Small "r" mark
    render_draw_line_rotated(ctx, x, y, 8, -2, 8, 2, rotation);
    render_draw_line_rotated(ctx, x, y, 8, -2, 10, -2, rotation);
}

// CCCS (Current-Controlled Current Source) - diamond with arrow
void render_cccs(RenderContext *ctx, float x, float y, int rotation) {
    // Diamond shape
    render_draw_line_rotated(ctx, x, y, 0, -20, 20, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 20, 0, 0, 20, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 20, -20, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -20, 0, 0, -20, rotation);
    render_draw_line_rotated(ctx, x, y, 0, -20, 0, -40, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 20, 0, 40, rotation);
    // Arrow
    render_draw_line_rotated(ctx, x, y, 0, 10, 0, -10, rotation);
    render_draw_line_rotated(ctx, x, y, -4, -5, 0, -10, rotation);
    render_draw_line_rotated(ctx, x, y, 4, -5, 0, -10, rotation);
    // Beta mark
    render_draw_line_rotated(ctx, x, y, 8, -2, 8, 4, rotation);
    render_draw_line_rotated(ctx, x, y, 8, 0, 11, -2, rotation);
    render_draw_line_rotated(ctx, x, y, 8, 2, 11, 4, rotation);
}

// DPDT Switch
void render_dpdt_switch(RenderContext *ctx, float x, float y, int rotation, int position) {
    // 6 terminals: two poles, each with common and two throws
    // Draw two SPDT switches side by side
    // Left pole
    render_draw_line_rotated(ctx, x, y, -50, -20, -30, -20, rotation);
    render_draw_line_rotated(ctx, x, y, -50, 20, -30, 20, rotation);
    render_draw_circle_rotated(ctx, x, y, -30, -20, 3, rotation);
    render_draw_circle_rotated(ctx, x, y, -30, 20, 3, rotation);
    render_draw_circle_rotated(ctx, x, y, -10, 0, 3, rotation);
    render_draw_line_rotated(ctx, x, y, -10, 0, -30, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -30, 0, -50, 0, rotation);
    // Left switch blade
    if (position == 0) {
        render_draw_line_rotated(ctx, x, y, -10, 0, -28, -18, rotation);
    } else {
        render_draw_line_rotated(ctx, x, y, -10, 0, -28, 18, rotation);
    }
    // Right pole
    render_draw_line_rotated(ctx, x, y, 50, -20, 30, -20, rotation);
    render_draw_line_rotated(ctx, x, y, 50, 20, 30, 20, rotation);
    render_draw_circle_rotated(ctx, x, y, 30, -20, 3, rotation);
    render_draw_circle_rotated(ctx, x, y, 30, 20, 3, rotation);
    render_draw_circle_rotated(ctx, x, y, 10, 0, 3, rotation);
    render_draw_line_rotated(ctx, x, y, 10, 0, 30, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 30, 0, 50, 0, rotation);
    // Right switch blade
    if (position == 0) {
        render_draw_line_rotated(ctx, x, y, 10, 0, 28, -18, rotation);
    } else {
        render_draw_line_rotated(ctx, x, y, 10, 0, 28, 18, rotation);
    }
    // Mechanical linkage (dashed line between poles)
    render_draw_line_rotated(ctx, x, y, -10, -8, 10, -8, rotation);
}

// Relay - coil with switch
void render_relay(RenderContext *ctx, float x, float y, int rotation, bool energized) {
    // Coil on left side
    render_draw_line_rotated(ctx, x, y, -50, -20, -30, -20, rotation);
    render_draw_line_rotated(ctx, x, y, -50, 20, -30, 20, rotation);
    // Coil rectangle
    render_draw_line_rotated(ctx, x, y, -30, -15, -10, -15, rotation);
    render_draw_line_rotated(ctx, x, y, -30, 15, -10, 15, rotation);
    render_draw_line_rotated(ctx, x, y, -30, -15, -30, 15, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -15, -10, 15, rotation);
    // Coil windings inside
    for (int i = -12; i <= 12; i += 6) {
        render_draw_line_rotated(ctx, x, y, -28, i, -12, i, rotation);
    }
    // Switch on right side
    render_draw_line_rotated(ctx, x, y, 50, -20, 30, -20, rotation);
    render_draw_line_rotated(ctx, x, y, 50, 20, 30, 20, rotation);
    render_draw_circle_rotated(ctx, x, y, 30, -20, 3, rotation);
    render_draw_circle_rotated(ctx, x, y, 30, 20, 3, rotation);
    render_draw_circle_rotated(ctx, x, y, 10, 0, 3, rotation);
    render_draw_line_rotated(ctx, x, y, 10, 0, -5, 0, rotation);
    // Switch blade
    if (energized) {
        render_draw_line_rotated(ctx, x, y, 10, 0, 28, 18, rotation);  // NO contact closed
    } else {
        render_draw_line_rotated(ctx, x, y, 10, 0, 28, -18, rotation);  // NC contact closed
    }
    // Dashed line showing magnetic coupling
    render_draw_line_rotated(ctx, x, y, -5, -10, -5, 10, rotation);
}

// Analog Switch
void render_analog_switch(RenderContext *ctx, float x, float y, int rotation, bool closed) {
    // Terminals at (-40, 0) and (40, 0), control at (0, -20)
    render_draw_line_rotated(ctx, x, y, -40, 0, -15, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 15, 0, 40, 0, rotation);
    // Switch contacts
    render_draw_circle_rotated(ctx, x, y, -15, 0, 3, rotation);
    render_draw_circle_rotated(ctx, x, y, 15, 0, 3, rotation);
    // Switch blade
    if (closed) {
        render_draw_line_rotated(ctx, x, y, -15, 0, 15, 0, rotation);
    } else {
        render_draw_line_rotated(ctx, x, y, -15, 0, 10, -10, rotation);
    }
    // Control input (arrow pointing at switch)
    render_draw_line_rotated(ctx, x, y, 0, -20, 0, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -3, -12, 0, -8, rotation);
    render_draw_line_rotated(ctx, x, y, 3, -12, 0, -8, rotation);
}

// Lamp - circle with X inside
void render_lamp(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -15, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 15, 0, 40, 0, rotation);
    // Circle
    render_draw_circle(ctx, x, y, 15);
    // X inside
    render_draw_line_rotated(ctx, x, y, -10, -10, 10, 10, rotation);
    render_draw_line_rotated(ctx, x, y, -10, 10, 10, -10, rotation);
}

// Speaker - rectangle with cone
void render_speaker(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, -10) and (-40, 10)
    render_draw_line_rotated(ctx, x, y, -40, -10, -20, -10, rotation);
    render_draw_line_rotated(ctx, x, y, -40, 10, -20, 10, rotation);
    // Voice coil box
    render_draw_line_rotated(ctx, x, y, -20, -10, -10, -10, rotation);
    render_draw_line_rotated(ctx, x, y, -20, 10, -10, 10, rotation);
    render_draw_line_rotated(ctx, x, y, -20, -10, -20, 10, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -10, -10, 10, rotation);
    // Cone (trapezoid)
    render_draw_line_rotated(ctx, x, y, -10, -10, 20, -20, rotation);
    render_draw_line_rotated(ctx, x, y, -10, 10, 20, 20, rotation);
    render_draw_line_rotated(ctx, x, y, 20, -20, 20, 20, rotation);
}

// DC Motor - circle with M
void render_dc_motor(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -18, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 18, 0, 40, 0, rotation);
    // Circle
    render_draw_circle(ctx, x, y, 18);
    // M inside (stylized)
    render_draw_line_rotated(ctx, x, y, -8, 8, -8, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -8, -8, 0, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 0, 8, -8, rotation);
    render_draw_line_rotated(ctx, x, y, 8, -8, 8, 8, rotation);
}

// Voltmeter - circle with V
void render_voltmeter(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -18, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 18, 0, 40, 0, rotation);
    // Circle
    render_draw_circle(ctx, x, y, 18);
    // V inside
    render_draw_line_rotated(ctx, x, y, -8, -10, 0, 10, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 10, 8, -10, rotation);
}

// Ammeter - circle with A
void render_ammeter(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -18, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 18, 0, 40, 0, rotation);
    // Circle
    render_draw_circle(ctx, x, y, 18);
    // A inside
    render_draw_line_rotated(ctx, x, y, -8, 10, 0, -10, rotation);
    render_draw_line_rotated(ctx, x, y, 0, -10, 8, 10, rotation);
    render_draw_line_rotated(ctx, x, y, -5, 3, 5, 3, rotation);
}

// Wattmeter - circle with W
void render_wattmeter(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -18, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 18, 0, 40, 0, rotation);
    render_draw_circle(ctx, x, y, 18);
    // W inside
    render_draw_line_rotated(ctx, x, y, -10, -8, -5, 8, rotation);
    render_draw_line_rotated(ctx, x, y, -5, 8, 0, -2, rotation);
    render_draw_line_rotated(ctx, x, y, 0, -2, 5, 8, rotation);
    render_draw_line_rotated(ctx, x, y, 5, 8, 10, -8, rotation);
}

// AC Current Source
void render_ac_current_source(RenderContext *ctx, float x, float y, int rotation) {
    // Circle with sine wave and arrow
    render_draw_circle(ctx, x, y, 18);
    render_draw_line_rotated(ctx, x, y, 0, -18, 0, -40, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 18, 0, 40, rotation);
    // Sine wave
    for (int i = 0; i < 16; i++) {
        float dx1 = -8 + i;
        float dx2 = -8 + i + 1;
        float dy1 = 5 * sin((i / 16.0) * 2 * M_PI);
        float dy2 = 5 * sin(((i + 1) / 16.0) * 2 * M_PI);
        render_draw_line_rotated(ctx, x, y, dx1, dy1, dx2, dy2, rotation);
    }
}

// Clock source
void render_clock_source(RenderContext *ctx, float x, float y, int rotation) {
    render_draw_circle(ctx, x, y, 18);
    render_draw_line_rotated(ctx, x, y, 0, -18, 0, -40, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 18, 0, 40, rotation);
    // Clock wave (digital square with edges)
    render_draw_line_rotated(ctx, x, y, -10, 5, -10, -5, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -5, -5, -5, rotation);
    render_draw_line_rotated(ctx, x, y, -5, -5, -5, 5, rotation);
    render_draw_line_rotated(ctx, x, y, -5, 5, 0, 5, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 5, 0, -5, rotation);
    render_draw_line_rotated(ctx, x, y, 0, -5, 5, -5, rotation);
    render_draw_line_rotated(ctx, x, y, 5, -5, 5, 5, rotation);
    render_draw_line_rotated(ctx, x, y, 5, 5, 10, 5, rotation);
}

// Pulse source
void render_pulse_source(RenderContext *ctx, float x, float y, int rotation) {
    render_draw_circle(ctx, x, y, 18);
    render_draw_line_rotated(ctx, x, y, 0, -18, 0, -40, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 18, 0, 40, rotation);
    // Single pulse
    render_draw_line_rotated(ctx, x, y, -10, 5, -5, 5, rotation);
    render_draw_line_rotated(ctx, x, y, -5, 5, -5, -5, rotation);
    render_draw_line_rotated(ctx, x, y, -5, -5, 5, -5, rotation);
    render_draw_line_rotated(ctx, x, y, 5, -5, 5, 5, rotation);
    render_draw_line_rotated(ctx, x, y, 5, 5, 10, 5, rotation);
}

// PWM source
void render_pwm_source(RenderContext *ctx, float x, float y, int rotation) {
    render_draw_circle(ctx, x, y, 18);
    render_draw_line_rotated(ctx, x, y, 0, -18, 0, -40, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 18, 0, 40, rotation);
    // PWM pattern (varying duty cycle)
    render_draw_line_rotated(ctx, x, y, -12, 5, -10, 5, rotation);
    render_draw_line_rotated(ctx, x, y, -10, 5, -10, -5, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -5, -6, -5, rotation);
    render_draw_line_rotated(ctx, x, y, -6, -5, -6, 5, rotation);
    render_draw_line_rotated(ctx, x, y, -6, 5, -4, 5, rotation);
    render_draw_line_rotated(ctx, x, y, -4, 5, -4, -5, rotation);
    render_draw_line_rotated(ctx, x, y, -4, -5, 2, -5, rotation);
    render_draw_line_rotated(ctx, x, y, 2, -5, 2, 5, rotation);
    render_draw_line_rotated(ctx, x, y, 2, 5, 4, 5, rotation);
    render_draw_line_rotated(ctx, x, y, 4, 5, 4, -5, rotation);
    render_draw_line_rotated(ctx, x, y, 4, -5, 12, -5, rotation);
}

// Logic gate helper - draw gate body outline
static void render_gate_body(RenderContext *ctx, float x, float y, int rotation) {
    // Left side (straight)
    render_draw_line_rotated(ctx, x, y, -15, -15, -15, 15, rotation);
    // Top
    render_draw_line_rotated(ctx, x, y, -15, -15, 10, -15, rotation);
    // Bottom
    render_draw_line_rotated(ctx, x, y, -15, 15, 10, 15, rotation);
}

// NOT gate (inverter)
void render_not_gate(RenderContext *ctx, float x, float y, int rotation) {
    // Triangle
    render_draw_line_rotated(ctx, x, y, -20, -15, -20, 15, rotation);
    render_draw_line_rotated(ctx, x, y, -20, -15, 10, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -20, 15, 10, 0, rotation);
    // Inversion bubble
    render_draw_circle_rotated(ctx, x, y, 15, 0, 5, rotation);
    // Input/output leads
    render_draw_line_rotated(ctx, x, y, -40, 0, -20, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 20, 0, 40, 0, rotation);
}

// AND gate
void render_and_gate(RenderContext *ctx, float x, float y, int rotation) {
    // Left side
    render_draw_line_rotated(ctx, x, y, -15, -15, -15, 15, rotation);
    // Top
    render_draw_line_rotated(ctx, x, y, -15, -15, 5, -15, rotation);
    // Bottom
    render_draw_line_rotated(ctx, x, y, -15, 15, 5, 15, rotation);
    // Curved right side (arc)
    for (int a = -90; a <= 90; a += 10) {
        float r1 = a * M_PI / 180;
        float r2 = (a + 10) * M_PI / 180;
        float dx1 = 5 + 15 * cos(r1);
        float dy1 = 15 * sin(r1);
        float dx2 = 5 + 15 * cos(r2);
        float dy2 = 15 * sin(r2);
        render_draw_line_rotated(ctx, x, y, dx1, dy1, dx2, dy2, rotation);
    }
    // Inputs
    render_draw_line_rotated(ctx, x, y, -40, -8, -15, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -40, 8, -15, 8, rotation);
    // Output
    render_draw_line_rotated(ctx, x, y, 20, 0, 40, 0, rotation);
}

// OR gate
void render_or_gate(RenderContext *ctx, float x, float y, int rotation) {
    // Curved left side
    for (int a = -30; a <= 30; a += 10) {
        float r1 = a * M_PI / 180;
        float r2 = (a + 10) * M_PI / 180;
        float dx1 = -25 + 20 * cos(r1);
        float dy1 = 20 * sin(r1);
        float dx2 = -25 + 20 * cos(r2);
        float dy2 = 20 * sin(r2);
        render_draw_line_rotated(ctx, x, y, dx1, dy1, dx2, dy2, rotation);
    }
    // Top curve
    for (int a = 180; a <= 240; a += 10) {
        float r1 = a * M_PI / 180;
        float r2 = (a + 10) * M_PI / 180;
        float dx1 = 20 + 25 * cos(r1);
        float dy1 = -30 + 25 * sin(r1);
        float dx2 = 20 + 25 * cos(r2);
        float dy2 = -30 + 25 * sin(r2);
        render_draw_line_rotated(ctx, x, y, dx1, dy1, dx2, dy2, rotation);
    }
    // Bottom curve
    for (int a = 120; a <= 180; a += 10) {
        float r1 = a * M_PI / 180;
        float r2 = (a + 10) * M_PI / 180;
        float dx1 = 20 + 25 * cos(r1);
        float dy1 = 30 + 25 * sin(r1);
        float dx2 = 20 + 25 * cos(r2);
        float dy2 = 30 + 25 * sin(r2);
        render_draw_line_rotated(ctx, x, y, dx1, dy1, dx2, dy2, rotation);
    }
    // Point
    render_draw_line_rotated(ctx, x, y, 7, -9, 20, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 7, 9, 20, 0, rotation);
    // Inputs
    render_draw_line_rotated(ctx, x, y, -40, -8, -10, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -40, 8, -10, 8, rotation);
    // Output
    render_draw_line_rotated(ctx, x, y, 20, 0, 40, 0, rotation);
}

// NAND gate
void render_nand_gate(RenderContext *ctx, float x, float y, int rotation) {
    // AND gate body
    render_draw_line_rotated(ctx, x, y, -15, -15, -15, 15, rotation);
    render_draw_line_rotated(ctx, x, y, -15, -15, 5, -15, rotation);
    render_draw_line_rotated(ctx, x, y, -15, 15, 5, 15, rotation);
    for (int a = -90; a <= 90; a += 10) {
        float r1 = a * M_PI / 180;
        float r2 = (a + 10) * M_PI / 180;
        float dx1 = 5 + 15 * cos(r1);
        float dy1 = 15 * sin(r1);
        float dx2 = 5 + 15 * cos(r2);
        float dy2 = 15 * sin(r2);
        render_draw_line_rotated(ctx, x, y, dx1, dy1, dx2, dy2, rotation);
    }
    // Inversion bubble
    render_draw_circle_rotated(ctx, x, y, 25, 0, 5, rotation);
    // Inputs/output
    render_draw_line_rotated(ctx, x, y, -40, -8, -15, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -40, 8, -15, 8, rotation);
    render_draw_line_rotated(ctx, x, y, 30, 0, 40, 0, rotation);
}

// NOR gate
void render_nor_gate(RenderContext *ctx, float x, float y, int rotation) {
    // OR gate body (simplified)
    render_draw_line_rotated(ctx, x, y, -15, -15, 5, -15, rotation);
    render_draw_line_rotated(ctx, x, y, -15, 15, 5, 15, rotation);
    render_draw_line_rotated(ctx, x, y, -15, -15, -15, 15, rotation);
    render_draw_line_rotated(ctx, x, y, 5, -15, 15, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 5, 15, 15, 0, rotation);
    // Inversion bubble
    render_draw_circle_rotated(ctx, x, y, 20, 0, 5, rotation);
    // Inputs/output
    render_draw_line_rotated(ctx, x, y, -40, -8, -15, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -40, 8, -15, 8, rotation);
    render_draw_line_rotated(ctx, x, y, 25, 0, 40, 0, rotation);
}

// XOR gate
void render_xor_gate(RenderContext *ctx, float x, float y, int rotation) {
    // OR shape with extra curve
    render_draw_line_rotated(ctx, x, y, -18, -15, -18, 15, rotation);
    render_draw_line_rotated(ctx, x, y, -15, -15, -15, 15, rotation);
    render_draw_line_rotated(ctx, x, y, -15, -15, 5, -15, rotation);
    render_draw_line_rotated(ctx, x, y, -15, 15, 5, 15, rotation);
    render_draw_line_rotated(ctx, x, y, 5, -15, 20, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 5, 15, 20, 0, rotation);
    // Inputs/output
    render_draw_line_rotated(ctx, x, y, -40, -8, -15, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -40, 8, -15, 8, rotation);
    render_draw_line_rotated(ctx, x, y, 20, 0, 40, 0, rotation);
}

// XNOR gate
void render_xnor_gate(RenderContext *ctx, float x, float y, int rotation) {
    // XOR body
    render_draw_line_rotated(ctx, x, y, -18, -15, -18, 15, rotation);
    render_draw_line_rotated(ctx, x, y, -15, -15, -15, 15, rotation);
    render_draw_line_rotated(ctx, x, y, -15, -15, 5, -15, rotation);
    render_draw_line_rotated(ctx, x, y, -15, 15, 5, 15, rotation);
    render_draw_line_rotated(ctx, x, y, 5, -15, 15, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 5, 15, 15, 0, rotation);
    // Inversion bubble
    render_draw_circle_rotated(ctx, x, y, 20, 0, 5, rotation);
    // Inputs/output
    render_draw_line_rotated(ctx, x, y, -40, -8, -15, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -40, 8, -15, 8, rotation);
    render_draw_line_rotated(ctx, x, y, 25, 0, 40, 0, rotation);
}

// Buffer
void render_buffer(RenderContext *ctx, float x, float y, int rotation) {
    // Triangle (no bubble)
    render_draw_line_rotated(ctx, x, y, -20, -15, -20, 15, rotation);
    render_draw_line_rotated(ctx, x, y, -20, -15, 15, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -20, 15, 15, 0, rotation);
    // Leads
    render_draw_line_rotated(ctx, x, y, -40, 0, -20, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 15, 0, 40, 0, rotation);
}

// 555 Timer IC
void render_555_timer(RenderContext *ctx, float x, float y, int rotation) {
    // IC package (rectangle)
    render_draw_line_rotated(ctx, x, y, -30, -30, 30, -30, rotation);
    render_draw_line_rotated(ctx, x, y, -30, 30, 30, 30, rotation);
    render_draw_line_rotated(ctx, x, y, -30, -30, -30, 30, rotation);
    render_draw_line_rotated(ctx, x, y, 30, -30, 30, 30, rotation);
    // Pin 1 notch
    render_draw_circle_rotated(ctx, x, y, -25, -25, 3, rotation);
    // Terminals (8-pin DIP style)
    render_draw_line_rotated(ctx, x, y, -40, -20, -30, -20, rotation);  // Pin 1 GND
    render_draw_line_rotated(ctx, x, y, -40, -7, -30, -7, rotation);    // Pin 2 TRIG
    render_draw_line_rotated(ctx, x, y, -40, 7, -30, 7, rotation);      // Pin 3 OUT
    render_draw_line_rotated(ctx, x, y, -40, 20, -30, 20, rotation);    // Pin 4 RESET
    render_draw_line_rotated(ctx, x, y, 30, -20, 40, -20, rotation);    // Pin 8 VCC
    render_draw_line_rotated(ctx, x, y, 30, -7, 40, -7, rotation);      // Pin 7 DIS
    render_draw_line_rotated(ctx, x, y, 30, 7, 40, 7, rotation);        // Pin 6 THR
    render_draw_line_rotated(ctx, x, y, 30, 20, 40, 20, rotation);      // Pin 5 CV
    // "555" label inside
    int sx, sy;
    render_world_to_screen(ctx, x, y, &sx, &sy);
    render_draw_text_small(ctx, "555", sx - 10, sy - 4, COLOR_TEXT);
}

// Logic input (switch/level)
void render_logic_input(RenderContext *ctx, float x, float y, int rotation, bool high) {
    // Box
    render_draw_line_rotated(ctx, x, y, -15, -15, 15, -15, rotation);
    render_draw_line_rotated(ctx, x, y, -15, 15, 15, 15, rotation);
    render_draw_line_rotated(ctx, x, y, -15, -15, -15, 15, rotation);
    render_draw_line_rotated(ctx, x, y, 15, -15, 15, 15, rotation);
    // Output lead
    render_draw_line_rotated(ctx, x, y, 15, 0, 40, 0, rotation);
    // H or L inside
    if (high) {
        render_draw_line_rotated(ctx, x, y, -8, -8, -8, 8, rotation);
        render_draw_line_rotated(ctx, x, y, 8, -8, 8, 8, rotation);
        render_draw_line_rotated(ctx, x, y, -8, 0, 8, 0, rotation);
    } else {
        render_draw_line_rotated(ctx, x, y, -5, -8, -5, 8, rotation);
        render_draw_line_rotated(ctx, x, y, -5, 8, 5, 8, rotation);
    }
}

// Logic output (LED indicator)
void render_logic_output(RenderContext *ctx, float x, float y, int rotation, bool high) {
    // Circle
    render_draw_circle(ctx, x, y, 12);
    // Input lead
    render_draw_line_rotated(ctx, x, y, -40, 0, -12, 0, rotation);
    // Fill if high (draw concentric circles to simulate fill)
    if (high) {
        for (int r = 10; r > 0; r -= 2) {
            render_draw_circle(ctx, x, y, r);
        }
    }
}

// D Flip-Flop
void render_d_flipflop(RenderContext *ctx, float x, float y, int rotation) {
    // Rectangle
    render_draw_line_rotated(ctx, x, y, -25, -25, 25, -25, rotation);
    render_draw_line_rotated(ctx, x, y, -25, 25, 25, 25, rotation);
    render_draw_line_rotated(ctx, x, y, -25, -25, -25, 25, rotation);
    render_draw_line_rotated(ctx, x, y, 25, -25, 25, 25, rotation);
    // Inputs: D, CLK
    render_draw_line_rotated(ctx, x, y, -40, -15, -25, -15, rotation);
    render_draw_line_rotated(ctx, x, y, -40, 15, -25, 15, rotation);
    // Clock symbol (triangle)
    render_draw_line_rotated(ctx, x, y, -25, 12, -20, 15, rotation);
    render_draw_line_rotated(ctx, x, y, -25, 18, -20, 15, rotation);
    // Outputs: Q, Q'
    render_draw_line_rotated(ctx, x, y, 25, -15, 40, -15, rotation);
    render_draw_line_rotated(ctx, x, y, 25, 15, 40, 15, rotation);
    // Q' inversion bubble
    render_draw_circle_rotated(ctx, x, y, 28, 15, 3, rotation);
}

// VCO (Voltage-Controlled Oscillator)
void render_vco(RenderContext *ctx, float x, float y, int rotation) {
    // Circle
    render_draw_circle(ctx, x, y, 18);
    render_draw_line_rotated(ctx, x, y, -18, 0, -40, 0, rotation);  // Control input
    render_draw_line_rotated(ctx, x, y, 18, 0, 40, 0, rotation);   // Output
    // Sine wave with arrow
    for (int i = 0; i < 12; i++) {
        float dx1 = -6 + i;
        float dx2 = -6 + i + 1;
        float dy1 = 5 * sin((i / 12.0) * 2 * M_PI);
        float dy2 = 5 * sin(((i + 1) / 12.0) * 2 * M_PI);
        render_draw_line_rotated(ctx, x, y, dx1, dy1, dx2, dy2, rotation);
    }
    // Arrow indicating variable
    render_draw_line_rotated(ctx, x, y, -10, -12, 5, -12, rotation);
    render_draw_line_rotated(ctx, x, y, 2, -15, 5, -12, rotation);
    render_draw_line_rotated(ctx, x, y, 2, -9, 5, -12, rotation);
}

// Optocoupler
void render_optocoupler(RenderContext *ctx, float x, float y, int rotation) {
    // Box outline
    render_draw_line_rotated(ctx, x, y, -30, -25, 30, -25, rotation);
    render_draw_line_rotated(ctx, x, y, -30, 25, 30, 25, rotation);
    render_draw_line_rotated(ctx, x, y, -30, -25, -30, 25, rotation);
    render_draw_line_rotated(ctx, x, y, 30, -25, 30, 25, rotation);
    // LED on left (simplified diode)
    render_draw_line_rotated(ctx, x, y, -40, -15, -20, -15, rotation);
    render_draw_line_rotated(ctx, x, y, -40, 15, -20, 15, rotation);
    render_draw_line_rotated(ctx, x, y, -20, -10, -20, 10, rotation);
    render_draw_line_rotated(ctx, x, y, -20, -10, -10, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -20, 10, -10, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -10, -10, -10, 10, rotation);
    // Light arrows
    render_draw_line_rotated(ctx, x, y, -5, -5, 5, -5, rotation);
    render_draw_line_rotated(ctx, x, y, 2, -8, 5, -5, rotation);
    render_draw_line_rotated(ctx, x, y, 2, -2, 5, -5, rotation);
    render_draw_line_rotated(ctx, x, y, -5, 5, 5, 5, rotation);
    render_draw_line_rotated(ctx, x, y, 2, 2, 5, 5, rotation);
    render_draw_line_rotated(ctx, x, y, 2, 8, 5, 8, rotation);
    // Phototransistor on right (simplified)
    render_draw_line_rotated(ctx, x, y, 10, 0, 10, -10, rotation);
    render_draw_line_rotated(ctx, x, y, 10, 0, 10, 10, rotation);
    render_draw_line_rotated(ctx, x, y, 10, -5, 20, -15, rotation);
    render_draw_line_rotated(ctx, x, y, 10, 5, 20, 15, rotation);
    render_draw_line_rotated(ctx, x, y, 20, -15, 40, -15, rotation);
    render_draw_line_rotated(ctx, x, y, 20, 15, 40, 15, rotation);
}

// Test point marker
void render_test_point(RenderContext *ctx, float x, float y, int rotation) {
    // Small circle with cross
    render_draw_circle(ctx, x, y, 8);
    render_draw_line_rotated(ctx, x, y, -5, 0, 5, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 0, -5, 0, 5, rotation);
    // Lead
    render_draw_line_rotated(ctx, x, y, 0, 8, 0, 20, rotation);
}

// 7-segment display
// Component size: 80x100, terminals: a,b,c,d,COM on left (-40), e,f,g,DP on right (40)
void render_7seg_display(RenderContext *ctx, float x, float y, int rotation) {
    // DIP-style IC package
    // Outer rectangle
    render_draw_line_rotated(ctx, x, y, -30, -45, 30, -45, rotation);  // Top
    render_draw_line_rotated(ctx, x, y, 30, -45, 30, 45, rotation);    // Right
    render_draw_line_rotated(ctx, x, y, 30, 45, -30, 45, rotation);    // Bottom
    render_draw_line_rotated(ctx, x, y, -30, 45, -30, -45, rotation);  // Left

    // Notch at top (DIP-style)
    render_draw_circle(ctx, x, y - 45, 5);

    // Left side lead lines to terminals at x=-40 (a, b, c, d, COM)
    render_draw_line_rotated(ctx, x, y, -30, -40, -40, -40, rotation);  // a
    render_draw_line_rotated(ctx, x, y, -30, -20, -40, -20, rotation);  // b
    render_draw_line_rotated(ctx, x, y, -30, 0, -40, 0, rotation);      // c
    render_draw_line_rotated(ctx, x, y, -30, 20, -40, 20, rotation);    // d
    render_draw_line_rotated(ctx, x, y, -30, 40, -40, 40, rotation);    // COM

    // Right side lead lines to terminals at x=40 (e, f, g, DP)
    render_draw_line_rotated(ctx, x, y, 30, -40, 40, -40, rotation);    // e
    render_draw_line_rotated(ctx, x, y, 30, -20, 40, -20, rotation);    // f
    render_draw_line_rotated(ctx, x, y, 30, 0, 40, 0, rotation);        // g
    render_draw_line_rotated(ctx, x, y, 30, 20, 40, 20, rotation);      // DP

    // 7-segment pattern inside (digit "8" outline)
    // Top horizontal (segment a)
    render_draw_line_rotated(ctx, x, y, -15, -35, 15, -35, rotation);
    // Top-left vertical (segment f)
    render_draw_line_rotated(ctx, x, y, -15, -35, -15, -5, rotation);
    // Top-right vertical (segment b)
    render_draw_line_rotated(ctx, x, y, 15, -35, 15, -5, rotation);
    // Middle horizontal (segment g)
    render_draw_line_rotated(ctx, x, y, -15, -5, 15, -5, rotation);
    // Bottom-left vertical (segment e)
    render_draw_line_rotated(ctx, x, y, -15, -5, -15, 25, rotation);
    // Bottom-right vertical (segment c)
    render_draw_line_rotated(ctx, x, y, 15, -5, 15, 25, rotation);
    // Bottom horizontal (segment d)
    render_draw_line_rotated(ctx, x, y, -15, 25, 15, 25, rotation);
    // Decimal point (segment DP)
    render_fill_circle(ctx, x + 20, y + 25, 3);

    // Label
    int sx, sy;
    render_world_to_screen(ctx, x, y, &sx, &sy);
    Color label_color = {0x00, 0xff, 0xff, 0xff};
    render_draw_text_small(ctx, "7SEG", sx - 10, sy - 5, label_color);
}

// LED array (bar graph)
// Component size: 80x30, terminals at (-40, 0) and (40, 0)
void render_led_array(RenderContext *ctx, float x, float y, int rotation) {
    // Lead lines to terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -30, 0, rotation);  // Left lead
    render_draw_line_rotated(ctx, x, y, 30, 0, 40, 0, rotation);    // Right lead

    // Outer rectangle (sized to fit within component bounds)
    render_draw_line_rotated(ctx, x, y, -30, -12, 30, -12, rotation);  // Top
    render_draw_line_rotated(ctx, x, y, 30, -12, 30, 12, rotation);    // Right
    render_draw_line_rotated(ctx, x, y, 30, 12, -30, 12, rotation);    // Bottom
    render_draw_line_rotated(ctx, x, y, -30, 12, -30, -12, rotation);  // Left

    // LED segments (8 bars)
    for (int i = 0; i < 8; i++) {
        float bx = -26 + i * 7;
        render_draw_line_rotated(ctx, x, y, bx, -8, bx + 5, -8, rotation);
        render_draw_line_rotated(ctx, x, y, bx + 5, -8, bx + 5, 8, rotation);
        render_draw_line_rotated(ctx, x, y, bx + 5, 8, bx, 8, rotation);
        render_draw_line_rotated(ctx, x, y, bx, 8, bx, -8, rotation);
    }
    // Label
    int sx, sy;
    render_world_to_screen(ctx, x, y - 22, &sx, &sy);
    Color label_color = {0x00, 0xff, 0xff, 0xff};
    render_draw_text(ctx, "LED ARR", sx - 20, sy, label_color);
}

// BCD to 7-segment decoder (7447/74LS47 style)
// Component size: 80x140, terminals: A,B,C,D inputs on left, a-g outputs on right
void render_bcd_decoder(RenderContext *ctx, float x, float y, int rotation) {
    // DIP-style IC package
    // Outer rectangle
    render_draw_line_rotated(ctx, x, y, -30, -65, 30, -65, rotation);  // Top
    render_draw_line_rotated(ctx, x, y, 30, -65, 30, 65, rotation);    // Right
    render_draw_line_rotated(ctx, x, y, 30, 65, -30, 65, rotation);    // Bottom
    render_draw_line_rotated(ctx, x, y, -30, 65, -30, -65, rotation);  // Left

    // Notch at top (DIP-style)
    render_draw_circle(ctx, x, y - 65, 5);

    // Left side lead lines to terminals (inputs: A, B, C, D)
    render_draw_line_rotated(ctx, x, y, -30, -60, -40, -60, rotation);  // A
    render_draw_line_rotated(ctx, x, y, -30, -20, -40, -20, rotation);  // B
    render_draw_line_rotated(ctx, x, y, -30, 20, -40, 20, rotation);    // C
    render_draw_line_rotated(ctx, x, y, -30, 60, -40, 60, rotation);    // D

    // Right side lead lines to terminals (outputs: a, b, c, d, e, f, g)
    render_draw_line_rotated(ctx, x, y, 30, -60, 40, -60, rotation);    // a
    render_draw_line_rotated(ctx, x, y, 30, -40, 40, -40, rotation);    // b
    render_draw_line_rotated(ctx, x, y, 30, -20, 40, -20, rotation);    // c
    render_draw_line_rotated(ctx, x, y, 30, 0, 40, 0, rotation);        // d
    render_draw_line_rotated(ctx, x, y, 30, 20, 40, 20, rotation);      // e
    render_draw_line_rotated(ctx, x, y, 30, 40, 40, 40, rotation);      // f
    render_draw_line_rotated(ctx, x, y, 30, 60, 40, 60, rotation);      // g

    // Draw input labels on left side
    int sx, sy;
    Color label_color = {0x00, 0xff, 0xff, 0xff};

    render_world_to_screen(ctx, x - 25, y - 60, &sx, &sy);
    render_draw_text_small(ctx, "A", sx, sy - 3, label_color);
    render_world_to_screen(ctx, x - 25, y - 20, &sx, &sy);
    render_draw_text_small(ctx, "B", sx, sy - 3, label_color);
    render_world_to_screen(ctx, x - 25, y + 20, &sx, &sy);
    render_draw_text_small(ctx, "C", sx, sy - 3, label_color);
    render_world_to_screen(ctx, x - 25, y + 60, &sx, &sy);
    render_draw_text_small(ctx, "D", sx, sy - 3, label_color);

    // Draw output labels on right side
    render_world_to_screen(ctx, x + 18, y - 60, &sx, &sy);
    render_draw_text_small(ctx, "a", sx, sy - 3, label_color);
    render_world_to_screen(ctx, x + 18, y - 40, &sx, &sy);
    render_draw_text_small(ctx, "b", sx, sy - 3, label_color);
    render_world_to_screen(ctx, x + 18, y - 20, &sx, &sy);
    render_draw_text_small(ctx, "c", sx, sy - 3, label_color);
    render_world_to_screen(ctx, x + 18, y, &sx, &sy);
    render_draw_text_small(ctx, "d", sx, sy - 3, label_color);
    render_world_to_screen(ctx, x + 18, y + 20, &sx, &sy);
    render_draw_text_small(ctx, "e", sx, sy - 3, label_color);
    render_world_to_screen(ctx, x + 18, y + 40, &sx, &sy);
    render_draw_text_small(ctx, "f", sx, sy - 3, label_color);
    render_world_to_screen(ctx, x + 18, y + 60, &sx, &sy);
    render_draw_text_small(ctx, "g", sx, sy - 3, label_color);

    // IC label in center
    render_world_to_screen(ctx, x, y - 5, &sx, &sy);
    render_draw_text_small(ctx, "7447", sx - 10, sy, label_color);
    render_world_to_screen(ctx, x, y + 10, &sx, &sy);
    render_draw_text_small(ctx, "BCD", sx - 8, sy, label_color);
}
