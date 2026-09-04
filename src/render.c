/**
 * Circuit Playground - Rendering System Implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "render.h"
#include "label.h"
/* Last, because it redefines the two SDL colour calls everything draws through. */
#include "style.h"

// Forward declarations for new component symbols
void render_fuse(RenderContext *ctx, float x, float y, int rotation, bool blown, double heat_level);
void render_crystal(RenderContext *ctx, float x, float y, int rotation);
void render_spark_gap(RenderContext *ctx, float x, float y, int rotation);
static void render_draw_line_rotated(RenderContext *ctx, float cx, float cy,
                                     float x1, float y1, float x2, float y2, int rotation);
static void render_draw_circle_rotated(RenderContext *ctx, float cx, float cy,
                                       float dx, float dy, float r, int rotation);
void render_load_hp(RenderContext *ctx, float x, float y, int rotation);
void render_battery(RenderContext *ctx, float x, float y, int rotation);
static void render_component_value(RenderContext *ctx, Component *comp) {
    char buf[96];
    float lx, ly;
    if (!render_component_value_label(comp, buf, sizeof buf, &lx, &ly)) return;
    int sx, sy; render_world_to_screen(ctx, lx, ly, &sx, &sy);
    render_draw_text_small(ctx, buf, sx, sy, (Color){0xa0, 0xb4, 0xc8, 0xff});
}
void render_toroid(RenderContext *ctx, Component *comp);
void render_tline(RenderContext *ctx, Component *comp);
void render_source_3ph(RenderContext *ctx, Component *comp);
static void render_arc_between(RenderContext *ctx, float x1, float y1, float x2, float y2, float intensity);
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
void render_dpdt_driven(RenderContext *ctx, float x, float y, int rotation, bool thrown);
void render_delay_line(RenderContext *ctx, Component *comp);
void render_lamp(RenderContext *ctx, float x, float y, int rotation);
void render_speaker(RenderContext *ctx, float x, float y, int rotation);
void render_microphone(RenderContext *ctx, float x, float y, int rotation);
void render_antenna_tx(RenderContext *ctx, float x, float y, int rotation);
void render_antenna_rx(RenderContext *ctx, float x, float y, int rotation);
void render_bus(RenderContext *ctx, float x, float y, int rotation, int width);
void render_bus_tap(RenderContext *ctx, float x, float y, int rotation);
void render_led_matrix(RenderContext *ctx, float x, float y, int rotation, uint8_t *pixel_state, uint8_t color_idx);
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
void render_regulator_box(RenderContext *ctx, float x, float y, int rotation);
void render_logic_input(RenderContext *ctx, float x, float y, int rotation, bool high);
void render_logic_output(RenderContext *ctx, float x, float y, int rotation, bool high);
void render_d_flipflop(RenderContext *ctx, float x, float y, int rotation);
void render_vco(RenderContext *ctx, float x, float y, int rotation);
void render_optocoupler(RenderContext *ctx, float x, float y, int rotation);
void render_test_point(RenderContext *ctx, float x, float y, int rotation);
void render_7seg_display(RenderContext *ctx, float x, float y, int rotation,
                         const double *currents, double max_current);
void render_led_array(RenderContext *ctx, float x, float y, int rotation,
                      double *currents, bool *failed, double max_current, int color_idx);
void render_bcd_decoder(RenderContext *ctx, float x, float y, int rotation);
void render_counter(RenderContext *ctx, float x, float y, int rotation, int count, int modulus);
void render_subcircuit(RenderContext *ctx, float x, float y, int rotation, int def_id, const char *name);

// Neon glow effect helpers (for selected items)
void render_neon_glow_component(RenderContext *ctx, float x, float y, float size);
void render_neon_glow_wire(RenderContext *ctx, float x1, float y1, float x2, float y2);
void render_neon_chaser_rect(RenderContext *ctx, float x, float y, float size);
void render_neon_chaser_line(RenderContext *ctx, float x1, float y1, float x2, float y2);

// Simple 8x8 bitmap font (ASCII 32-126)
static const unsigned char font8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // '!'
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // '"'
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // '#'
    {0x18,0x7C,0x06,0x3C,0x60,0x3E,0x18,0x00}, // '$'
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // '%'
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // '&'
    {0x18,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, // '''
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // '('
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // ')'
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // '*'
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // '+'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x0C}, // ','
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // '-'
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // '.'
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // '/'
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, // '0'
    {0x18,0x1C,0x18,0x18,0x18,0x18,0x7E,0x00}, // '1'
    {0x3C,0x66,0x60,0x38,0x0C,0x66,0x7E,0x00}, // '2'
    {0x3C,0x66,0x60,0x38,0x60,0x66,0x3C,0x00}, // '3'
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, // '4'
    {0x7E,0x06,0x3E,0x60,0x60,0x66,0x3C,0x00}, // '5'
    {0x38,0x0C,0x06,0x3E,0x66,0x66,0x3C,0x00}, // '6'
    {0x7E,0x66,0x60,0x30,0x18,0x18,0x18,0x00}, // '7'
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, // '8'
    {0x3C,0x66,0x66,0x7C,0x60,0x30,0x1C,0x00}, // '9'
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, // ':'
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x0C}, // ';'
    {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, // '<'
    {0x00,0x00,0x7E,0x00,0x00,0x7E,0x00,0x00}, // '='
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // '>'
    {0x3C,0x66,0x60,0x30,0x18,0x00,0x18,0x00}, // '?'
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, // '@'
    {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00}, // 'A'
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, // 'B'
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, // 'C'
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, // 'D'
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, // 'E'
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, // 'F'
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, // 'G'
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, // 'H'
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 'I'
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, // 'J'
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, // 'K'
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, // 'L'
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, // 'M'
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, // 'N'
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // 'O'
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, // 'P'
    {0x3C,0x66,0x66,0x66,0x76,0x3C,0x70,0x00}, // 'Q'
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, // 'R'
    {0x3C,0x66,0x0E,0x1C,0x70,0x66,0x3C,0x00}, // 'S'
    {0x7E,0x5A,0x18,0x18,0x18,0x18,0x3C,0x00}, // 'T'
    {0x66,0x66,0x66,0x66,0x66,0x66,0x7E,0x00}, // 'U'
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, // 'V'
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // 'W'
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, // 'X'
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x3C,0x00}, // 'Y'
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, // 'Z'
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, // '['
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, // '\'
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, // ']'
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // '^'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // '_'
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // '`'
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, // 'a'
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, // 'b'
    {0x00,0x00,0x3C,0x66,0x06,0x66,0x3C,0x00}, // 'c'
    {0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00}, // 'd'
    {0x00,0x00,0x3C,0x66,0x7E,0x06,0x3C,0x00}, // 'e'
    {0x38,0x6C,0x0C,0x1E,0x0C,0x0C,0x1E,0x00}, // 'f'
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, // 'g'
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, // 'h'
    {0x18,0x00,0x1C,0x18,0x18,0x18,0x3C,0x00}, // 'i'
    {0x60,0x00,0x60,0x60,0x60,0x66,0x66,0x3C}, // 'j'
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, // 'k'
    {0x1C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // 'l'
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, // 'm'
    {0x00,0x00,0x3E,0x66,0x66,0x66,0x66,0x00}, // 'n'
    {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, // 'o'
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, // 'p'
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, // 'q'
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, // 'r'
    {0x00,0x00,0x7C,0x06,0x3C,0x60,0x3E,0x00}, // 's'
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, // 't'
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, // 'u'
    {0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, // 'v'
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, // 'w'
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, // 'x'
    {0x00,0x00,0x66,0x66,0x66,0x7C,0x60,0x3E}, // 'y'
    {0x00,0x00,0x7E,0x32,0x18,0x4C,0x7E,0x00}, // 'z'
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, // '{'
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // '|'
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, // '}'
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
/* ---------------------------------------------------------------------------------------
 * Antialiased text for the canvas.
 *
 * The font is an 8x8 bitmap, and drawing it as filled squares gives a staircase on every
 * diagonal that gets worse the larger it is drawn. Instead it is resampled once into a
 * coverage atlas: each glyph occupies a FONT_CELL_PX square whose alpha is the bitmap
 * sampled bilinearly, so an edge fades across a texel rather than stopping dead. Drawing is
 * then one textured quad per character with linear filtering, which stays smooth at any size
 * and at any zoom.
 *
 * A mild contrast curve keeps the strokes solid: without it a one-pixel stem loses half its
 * weight to the resampling and the text reads grey instead of lit.
 * ------------------------------------------------------------------------------------- */
#define FONT_SS       6                       /* atlas resolution per font pixel */
#define FONT_CELL_PX  (8 * FONT_SS)
#define FONT_GLYPHS   95

static float font_bit(int gi, int px, int py) {
    if (px < 0 || px > 7 || py < 0 || py > 7) return 0.0f;
    return (font8x8[gi][py] & (1 << px)) ? 1.0f : 0.0f;
}

static SDL_Texture *font_atlas_build(SDL_Renderer *renderer) {
    SDL_Texture *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_STATIC,
                                         FONT_CELL_PX * FONT_GLYPHS, FONT_CELL_PX);
    if (!tex) return NULL;

    size_t w = (size_t)FONT_CELL_PX * FONT_GLYPHS, h = FONT_CELL_PX;
    Uint32 *px = (Uint32 *)calloc(w * h, sizeof(Uint32));
    if (!px) { SDL_DestroyTexture(tex); return NULL; }

    for (int gi = 0; gi < FONT_GLYPHS; gi++) {
        for (int ty = 0; ty < FONT_CELL_PX; ty++) {
            for (int tx = 0; tx < FONT_CELL_PX; tx++) {
                /* texel centre, expressed in font-pixel coordinates */
                float sx = ((float)tx + 0.5f) / FONT_SS - 0.5f;
                float sy = ((float)ty + 0.5f) / FONT_SS - 0.5f;
                int x0 = (int)floorf(sx), y0 = (int)floorf(sy);
                float fx = sx - x0, fy = sy - y0;
                float c = font_bit(gi, x0,     y0)     * (1 - fx) * (1 - fy)
                        + font_bit(gi, x0 + 1, y0)     * fx       * (1 - fy)
                        + font_bit(gi, x0,     y0 + 1) * (1 - fx) * fy
                        + font_bit(gi, x0 + 1, y0 + 1) * fx       * fy;
                /* Contrast: solid through the middle of a stroke, soft only at its edge. A
                   plain resample leaves a one-pixel stem peaking well under full coverage, and
                   the text then reads grey instead of lit. */
                c = (c - 0.25f) / 0.45f;
                if (c < 0) c = 0; if (c > 1) c = 1;
                c = powf(c, 0.85f);
                Uint8 a = (Uint8)(c * 255.0f + 0.5f);
                px[(size_t)ty * w + (size_t)gi * FONT_CELL_PX + tx] =
                    ((Uint32)a << 24) | 0x00ffffffu;   /* white, alpha = coverage */
            }
        }
    }

    SDL_UpdateTexture(tex, NULL, px, (int)(w * sizeof(Uint32)));
    free(px);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(tex, SDL_ScaleModeLinear);
    return tex;
}

/* One character at an arbitrary pixel size. Returns how far to advance. */
/* The same antialiased atlas, reachable with only a renderer.

   The panels drew every glyph as up to 64 individual points, one per bit of an 8x8 bitmap.
   That is hard-edged by construction, and once the UI is scaled each of those points becomes a
   1.6-pixel square with seams showing between them - which is what the panel text looked like.
   The canvas has had a coverage-antialiased atlas of the same font all along; this shares it.
   Two renderers exist at most (the window and a popped-out scope), so a cache of four is
   generous. */
static struct { SDL_Renderer *r; SDL_Texture *tex; } g_atlas_cache[4];

SDL_Texture *render_shared_font_atlas(SDL_Renderer *r) {
    if (!r) return NULL;
    for (int i = 0; i < 4; i++) if (g_atlas_cache[i].r == r) return g_atlas_cache[i].tex;
    for (int i = 0; i < 4; i++) if (!g_atlas_cache[i].r) {
        g_atlas_cache[i].r = r;
        g_atlas_cache[i].tex = font_atlas_build(r);
        return g_atlas_cache[i].tex;
    }
    return NULL;
}

void render_text_at(SDL_Renderer *r, const char *text, int x, int y, int px, Color col) {
    if (!text || !*text || px <= 0) return;
    SDL_Texture *atlas = render_shared_font_atlas(r);
    if (!atlas) return;
    SDL_SetTextureColorMod(atlas, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(atlas, col.a);
    for (; *text; text++) {
        char c = *text;
        if (c < 32 || c > 126) c = '?';
        SDL_Rect src = { (c - 32) * FONT_CELL_PX, 0, FONT_CELL_PX, FONT_CELL_PX };
        SDL_Rect dst = { x, y, px, px };
        SDL_RenderCopy(r, atlas, &src, &dst);
        x += px;
    }
}

static int font_draw_char(RenderContext *ctx, char c, int x, int y, int size_px, Color col) {
    if (!ctx->font_atlas) {
        if (ctx->font_atlas_tried) return size_px;
        ctx->font_atlas_tried = true;
        ctx->font_atlas = font_atlas_build(ctx->renderer);
        if (!ctx->font_atlas) return size_px;
    }
    if (c < 32 || c > 126) c = '?';
    SDL_Rect src = { (c - 32) * FONT_CELL_PX, 0, FONT_CELL_PX, FONT_CELL_PX };
    SDL_Rect dst = { x, y, size_px, size_px };
    SDL_SetTextureColorMod(ctx->font_atlas, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(ctx->font_atlas, col.a);
    SDL_RenderCopy(ctx->renderer, ctx->font_atlas, &src, &dst);
    return size_px;
}

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
    /* Two is the whole of it: four times the pixels, which is where the smooth diagonals and
       the readable small text come from, and nothing this program draws is fill-bound. The
       command line can set it to 1 on a machine that cannot spare the memory. */
    ctx->ss = g_render_supersample;
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
    if (!ctx) return;
    if (ctx->ss_tex) SDL_DestroyTexture(ctx->ss_tex);
    if (ctx->font_atlas) SDL_DestroyTexture(ctx->font_atlas);
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

/* One line, at the weight it should have.

   Everything is drawn in logical coordinates and the frame is rendered into a target ss times
   that size, so a line SDL draws one device pixel wide comes back 1/ss of a pixel after the
   downscale - smooth, and much too faint to read as a wire. Drawing ss copies a fraction of a
   logical pixel apart puts the weight back and keeps the softened edge, which is the point of
   rendering large in the first place. */
void render_line_dev(RenderContext *ctx, float x1, float y1, float x2, float y2) {
    if (!ctx || !ctx->renderer) return;
    /* Device pixels per logical pixel: the UI scale and the supersample factor together, which
       is exactly what SDL_RenderSetScale was given in render_frame_begin. */
    float us = (ctx->ui_scale > 0.0f) ? ctx->ui_scale : 1.0f;
    float S = us * (ctx->ss > 1 ? (float)ctx->ss : 1.0f);
    if (S < 1.0f) S = 1.0f;

    /* A stroke is drawn as parallel copies of itself, because SDL rasterises a line one device
       pixel wide however the world is scaled. Two things about the spacing matter and both were
       wrong before:

       The copies must be ONE DEVICE PIXEL apart - 1/S of a logical pixel. They used to be
       1/ss apart, which was right until the UI gained a scale of its own: at 1.36x that is
       1.36 device pixels between copies, each one pixel wide, so every stroke was drawn with
       gaps down it. That is the pixellation, and no amount of supersampling hides it because
       the holes are in the geometry rather than in the sampling.

       And they must be centred on the line. Laying them all to one side moves every stroke half
       its own width, which on a symbol drawn from a dozen strokes is a dozen small
       inconsistencies in where its edges are. */
    float w = g_render_line_weight;
    if (w < 0.3f) w = 0.3f;

    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-6f) { dx = 1.0f; dy = 0.0f; len = 1.0f; }
    float ux = dx / len, uy = dy / len;                   /* along */
    float px = -uy, py = ux;                              /* across */

    /* Drawn as a quad with its edges faded out, rather than as a stack of one-pixel lines.

       Stacked lines are only ever as smooth as the supersampling underneath them: at 2x a
       diagonal has four levels of coverage to work with, which is still a visible staircase.
       Fading the last device pixel of each edge to nothing gives the rasteriser a real coverage
       ramp, so a shallow diagonal is smooth at any weight - including a hairline, which is the
       point. The feather is one device pixel wide however the world is scaled, so it neither
       fattens the line when zoomed in nor disappears when zoomed out.

       The ends are extended by half the width so that two segments meeting at a right angle -
       which on a schematic is most of them - fill their corner instead of leaving a notch. */
    float half = w * 0.5f;
    float f = 1.0f / S;                                   /* one device pixel, in logical units */
    x1 -= ux * half; y1 -= uy * half;
    x2 += ux * half; y2 += uy * half;

    Uint8 cr, cg, cb, ca;
    SDL_GetRenderDrawColor(ctx->renderer, &cr, &cg, &cb, &ca);
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    SDL_Color solid = { cr, cg, cb, ca };
    SDL_Color clear = { cr, cg, cb, 0 };

    float o[4] = { half + f, half, -half, -(half + f) };
    SDL_Color col[4] = { clear, solid, solid, clear };
    SDL_Vertex v[8];
    for (int k = 0; k < 4; k++) {
        v[k].position.x     = x1 + px * o[k]; v[k].position.y     = y1 + py * o[k];
        v[k + 4].position.x = x2 + px * o[k]; v[k + 4].position.y = y2 + py * o[k];
        v[k].color = v[k + 4].color = col[k];
        v[k].tex_coord.x = v[k].tex_coord.y = 0;
        v[k + 4].tex_coord.x = v[k + 4].tex_coord.y = 0;
    }
    /* three strips: the faded edge, the solid core, the other faded edge */
    static const int idx[18] = { 0,1,5, 0,5,4,  1,2,6, 1,6,5,  2,3,7, 2,7,6 };
    SDL_RenderGeometry(ctx->renderer, NULL, v, 8, idx, 18);
}

/* The same transform as render_world_to_screen, without the rounding.

   That rounding is what stops a line being smooth at every zoom rather than at some of them:
   an endpoint snapped to a whole pixel has had its position thrown away before the antialiaser
   ever sees it, so a diagonal steps as the view moves and a circle drawn from short chords
   comes out as a polygon with visibly flat sides. Hit-testing still wants the integer version
   and keeps it; only what gets drawn is carried in floating point. */
void render_world_to_screen_f(RenderContext *ctx, float wx, float wy, float *sx, float *sy) {
    if (!ctx) { *sx = wx; *sy = wy; return; }
    *sx = wx * ctx->zoom + ctx->offset_x + (float)ctx->canvas_rect.x;
    *sy = wy * ctx->zoom + ctx->offset_y + (float)ctx->canvas_rect.y;
}

void render_draw_line(RenderContext *ctx, float x1, float y1, float x2, float y2) {
    float sx1, sy1, sx2, sy2;
    render_world_to_screen_f(ctx, x1, y1, &sx1, &sy1);
    render_world_to_screen_f(ctx, x2, y2, &sx2, &sy2);
    render_line_dev(ctx, sx1, sy1, sx2, sy2);
}

/* Bind the oversized target and put the renderer into logical coordinates. Everything between
   this and render_frame_end draws exactly as it did before; it just lands on more pixels. */
/* win_w/win_h are UI pixels. Two scales are in play and they do different jobs: ui_scale makes
   a UI pixel bigger so the layout is the right physical size on the display, and ss makes each
   of those land on more device pixels so its edges are smooth. The target is sized for both and
   the renderer is put into UI coordinates, so nothing downstream has to know about either. */
void render_frame_begin(RenderContext *ctx, int win_w, int win_h) {
    if (!ctx || !ctx->renderer || win_w <= 0 || win_h <= 0) return;
    float us = (ctx->ui_scale > 0.0f) ? ctx->ui_scale : 1.0f;
    if (ctx->ss <= 1 && us <= 1.0f) return;             /* nothing to do at 1:1 */
    int tw = (int)(win_w * us * ctx->ss + 0.5f), th = (int)(win_h * us * ctx->ss + 0.5f);
    if (tw <= 0 || th <= 0) return;
    if (ctx->ss_tex && (ctx->ss_w != tw || ctx->ss_h != th)) {
        SDL_DestroyTexture(ctx->ss_tex);
        ctx->ss_tex = NULL;
    }
    if (!ctx->ss_tex) {
        /* linear, so the downscale averages rather than picks - the smoothing IS the result */
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
        ctx->ss_tex = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_RGBA32,
                                        SDL_TEXTUREACCESS_TARGET, tw, th);
        if (!ctx->ss_tex) { ctx->ss = 1; return; }   /* no target: carry on at 1x */
        ctx->ss_w = tw; ctx->ss_h = th;
        SDL_SetTextureScaleMode(ctx->ss_tex, SDL_ScaleModeLinear);
    }
    if (SDL_SetRenderTarget(ctx->renderer, ctx->ss_tex) != 0) { ctx->ss = 1; return; }
    SDL_RenderSetScale(ctx->renderer, us * ctx->ss, us * ctx->ss);
}

void render_frame_end(RenderContext *ctx) {
    if (!ctx || !ctx->renderer || !ctx->ss_tex) return;
    SDL_RenderSetScale(ctx->renderer, 1.0f, 1.0f);
    SDL_SetRenderTarget(ctx->renderer, NULL);
    SDL_RenderCopy(ctx->renderer, ctx->ss_tex, NULL, NULL);
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
    float sx, sy;
    render_world_to_screen_f(ctx, cx, cy, &sx, &sy);
    float sr = r * ctx->zoom;
    if (sr < 0.5f) sr = 0.5f;

    /* Enough chords that each is about two device pixels long, so a big circle does not show
       its corners and a small one does not pay for segments nobody can see. In floating point
       throughout: rounding each chord's ends to whole pixels was what made a source symbol
       read as a polygon. */
    float us = (ctx->ui_scale > 0.0f) ? ctx->ui_scale : 1.0f;
    float S = us * (ctx->ss > 1 ? (float)ctx->ss : 1.0f);
    int seg = (int)(sr * S * 1.6f);
    if (seg < 16) seg = 16;
    if (seg > 180) seg = 180;

    float prevx = sx + sr, prevy = sy;
    for (int i = 1; i <= seg; i++) {
        float a = (float)i * 2.0f * (float)M_PI / (float)seg;
        float nx = sx + sr * cosf(a), ny = sy + sr * sinf(a);
        render_line_dev(ctx, prevx, prevy, nx, ny);
        prevx = nx; prevy = ny;
    }
}

void render_fill_circle(RenderContext *ctx, float cx, float cy, float r) {
    if (!ctx || !ctx->renderer) return;
    /* A fan with a faded rim, not a stack of scanlines.

       The old one laid one horizontal span per integer row, from an integer centre, with an
       integer radius, which is three faults in four lines: the rim is a staircase because a row
       is either covered or not; the dot moves in whole pixels as the view is panned, because its
       centre was rounded before it was drawn; and it pops between sizes because the radius was
       truncated. Node dots, current-flow dots and smoke particles are all these, so they stayed
       visibly blocky while the wires running through them were smooth.

       The rim fades over one device pixel however the world is scaled - the same coverage ramp
       render_line_dev uses, and the same reason: no amount of supersampling smooths a hard
       edge as well as giving the rasteriser real partial coverage does. */
    float sx, sy;
    render_world_to_screen_f(ctx, cx, cy, &sx, &sy);
    float sr = r * ctx->zoom;

    float us = (ctx->ui_scale > 0.0f) ? ctx->ui_scale : 1.0f;
    float S = us * (ctx->ss > 1 ? (float)ctx->ss : 1.0f);
    if (S < 1.0f) S = 1.0f;
    float f = 1.0f / S;                                   /* one device pixel, in logical units */
    if (sr < f * 0.5f) sr = f * 0.5f;                     /* never smaller than the feather */

    /* One segment per couple of device pixels of circumference, so a big disc has no corners
       and a small one costs nothing. */
    int seg = (int)(sr * S * 1.6f);
    if (seg < 12) seg = 12;
    if (seg > 128) seg = 128;

    float r_in = sr - f * 0.5f;
    if (r_in < 0.0f) r_in = 0.0f;
    float r_out = sr + f * 0.5f;

    Uint8 cr, cg, cb, ca;
    SDL_GetRenderDrawColor(ctx->renderer, &cr, &cg, &cb, &ca);
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    SDL_Color solid = { cr, cg, cb, ca };
    SDL_Color clear = { cr, cg, cb, 0 };

    enum { MAX_SEG = 128 };
    SDL_Vertex v[1 + 2 * MAX_SEG];
    int idx[MAX_SEG * 9];
    int nv = 0, ni = 0;

    v[nv].position.x = sx; v[nv].position.y = sy;
    v[nv].color = solid; v[nv].tex_coord.x = v[nv].tex_coord.y = 0;
    nv++;
    for (int i = 0; i < seg; i++) {
        float a = (float)i * 2.0f * (float)M_PI / (float)seg;
        float ca_ = cosf(a), sa = sinf(a);
        v[nv].position.x = sx + r_in * ca_; v[nv].position.y = sy + r_in * sa;
        v[nv].color = solid; v[nv].tex_coord.x = v[nv].tex_coord.y = 0; nv++;
        v[nv].position.x = sx + r_out * ca_; v[nv].position.y = sy + r_out * sa;
        v[nv].color = clear; v[nv].tex_coord.x = v[nv].tex_coord.y = 0; nv++;
    }
    for (int i = 0; i < seg; i++) {
        int a0 = 1 + i * 2, b0 = a0 + 1;                  /* inner, outer at this angle */
        int a1 = 1 + ((i + 1) % seg) * 2, b1 = a1 + 1;    /* ...and at the next */
        idx[ni++] = 0;  idx[ni++] = a0; idx[ni++] = a1;   /* the solid core */
        idx[ni++] = a0; idx[ni++] = b0; idx[ni++] = b1;   /* the faded rim */
        idx[ni++] = a0; idx[ni++] = b1; idx[ni++] = a1;
    }
    SDL_RenderGeometry(ctx->renderer, NULL, v, nv, idx, ni);
}

void render_draw_line_screen(RenderContext *ctx, int x1, int y1, int x2, int y2) {
    render_line_dev(ctx, x1, y1, x2, y2);
}

void render_draw_rect_screen(RenderContext *ctx, int x, int y, int w, int h) {
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderDrawRect(ctx->renderer, &rect);
}

void render_fill_rect_screen(RenderContext *ctx, int x, int y, int w, int h) {
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(ctx->renderer, &rect);
}

/* Schematic text. CANVAS_TEXT_PX is the height of one character on the canvas: the font's own
   cell is 8 px, which is legible but mean, and everything here reads better a little larger.
   The geometry audit measures label boxes with the same number (tools/template_smoke.c). */
/* Schematic text is part of the drawing, so it scales with it. Drawn at a fixed pixel size it
   stayed the same while the circuit shrank, which is why a zoomed-out template was a wall of
   letters with a small diagram somewhere behind it - and why the geometry audit, which measures
   labels in world units, could call a template clean that plainly was not. */
int render_text_px(RenderContext *ctx, int font_size) {
    int fs = (font_size < 1) ? 1 : (font_size > 3) ? 3 : font_size;
    int px = (int)((float)(CANVAS_TEXT_PX * fs) * (ctx ? ctx->zoom : 1.0f) + 0.5f);
    return px < 3 ? 3 : px;    /* below this it is a smudge either way */
}

void render_draw_text(RenderContext *ctx, const char *text, int x, int y, Color color) {
    if (!text || !*text) return;  // Safety check for NULL or empty string
    int px = render_text_px(ctx, 1);
    int cx = x;
    while (*text) {
        cx += font_draw_char(ctx, *text, cx, y, px, color);
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
    int char_height = render_text_px(ctx, scale);       /* scales with the canvas */
    int char_width = char_height + (bold ? scale : 0);  // Extra width for bold

    int cx = x;
    int text_start_x = x;
    while (*text) {
        /* Italic and bold still go through the blocky path - they are only used by hand-placed
           annotations, and the atlas has no slanted or weighted variants. Everything else, which
           is every label a template places, takes the antialiased one. */
        if (italic || bold) {
            SDL_SetRenderDrawColor(ctx->renderer, color.r, color.g, color.b, color.a);
            draw_char_scaled(ctx->renderer, *text, cx, y, scale, bold, italic);
        } else {
            font_draw_char(ctx, *text, cx, y, char_height, color);
        }
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

// ============================================================================
// Neon Glow Effects for Selected Items (Synthwave Style)
// ============================================================================

// Draw a pulsating neon glow around a component
void render_neon_glow_component(RenderContext *ctx, float x, float y, float size) {
    // Get screen coordinates
    int scr_x, scr_y;
    render_world_to_screen(ctx, x, y, &scr_x, &scr_y);
    int screen_size = (int)(size * ctx->zoom);

    // Pulsating intensity using sine wave (0.5 to 1.0 range)
    double pulse = 0.5 + 0.5 * sin(ctx->animation_time * 3.0);

    // Enable additive blending for neon glow effect
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_ADD);

    // Synthwave pink/magenta color (from COLOR_ACCENT2)
    uint8_t r = 0xe9, g = 0x45, b = 0x60;

    // Draw multiple expanding rectangles with decreasing alpha for outer glow
    for (int layer = 6; layer >= 1; layer--) {
        int expand = layer * 3;
        uint8_t alpha = (uint8_t)(pulse * (30 + (7 - layer) * 20));
        if (alpha > 255) alpha = 255;

        SDL_SetRenderDrawColor(ctx->renderer, r, g, b, alpha);

        // Draw rectangle outline at this expansion level
        SDL_Rect rect = {
            scr_x - screen_size/2 - expand,
            scr_y - screen_size/2 - expand,
            screen_size + expand * 2,
            screen_size + expand * 2
        };
        SDL_RenderDrawRect(ctx->renderer, &rect);
    }

    // Inner bright core (white-ish pink)
    uint8_t core_alpha = (uint8_t)(pulse * 200);
    SDL_SetRenderDrawColor(ctx->renderer, 0xff, 0x80, 0xa0, core_alpha);
    SDL_Rect inner = {
        scr_x - screen_size/2 - 2,
        scr_y - screen_size/2 - 2,
        screen_size + 4,
        screen_size + 4
    };
    SDL_RenderDrawRect(ctx->renderer, &inner);

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
}

// Draw animated chaser lights around component selection
void render_neon_chaser_rect(RenderContext *ctx, float x, float y, float size) {
    int scr_x, scr_y;
    render_world_to_screen(ctx, x, y, &scr_x, &scr_y);
    int screen_size = (int)(size * ctx->zoom);
    int expand = 8;  // Chaser runs slightly outside the glow

    // Calculate perimeter positions for chaser dots
    int left = scr_x - screen_size/2 - expand;
    int right = scr_x + screen_size/2 + expand;
    int top = scr_y - screen_size/2 - expand;
    int bottom = scr_y + screen_size/2 + expand;
    int width = right - left;
    int height = bottom - top;
    int perimeter = 2 * width + 2 * height;

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_ADD);

    // Draw multiple chaser dots traveling around the perimeter
    int num_chasers = 6;
    for (int i = 0; i < num_chasers; i++) {
        // Each chaser has a different phase offset
        double phase = fmod(ctx->animation_time * 1.5 + (double)i / num_chasers, 1.0);
        int pos = (int)(phase * perimeter);

        int px, py;
        // Top edge (left to right)
        if (pos < width) {
            px = left + pos;
            py = top;
        }
        // Right edge (top to bottom)
        else if (pos < width + height) {
            px = right;
            py = top + (pos - width);
        }
        // Bottom edge (right to left)
        else if (pos < 2 * width + height) {
            px = right - (pos - width - height);
            py = bottom;
        }
        // Left edge (bottom to top)
        else {
            px = left;
            py = bottom - (pos - 2 * width - height);
        }

        // Draw glowing chaser dot (cyan for contrast with pink glow)
        // Outer glow
        SDL_SetRenderDrawColor(ctx->renderer, 0x00, 0xff, 0xff, 0x40);
        for (int dy = -4; dy <= 4; dy++) {
            for (int dx = -4; dx <= 4; dx++) {
                if (dx*dx + dy*dy <= 16) {
                    SDL_RenderDrawPoint(ctx->renderer, px + dx, py + dy);
                }
            }
        }

        // Middle glow
        SDL_SetRenderDrawColor(ctx->renderer, 0x00, 0xff, 0xff, 0x80);
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                if (dx*dx + dy*dy <= 4) {
                    SDL_RenderDrawPoint(ctx->renderer, px + dx, py + dy);
                }
            }
        }

        // Bright core
        SDL_SetRenderDrawColor(ctx->renderer, 0xff, 0xff, 0xff, 0xc0);
        SDL_RenderDrawPoint(ctx->renderer, px, py);
    }

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
}

// Draw a pulsating neon glow around a wire
void render_neon_glow_wire(RenderContext *ctx, float x1, float y1, float x2, float y2) {
    int sx1, sy1, sx2, sy2;
    render_world_to_screen(ctx, x1, y1, &sx1, &sy1);
    render_world_to_screen(ctx, x2, y2, &sx2, &sy2);

    // Pulsating intensity using sine wave
    double pulse = 0.5 + 0.5 * sin(ctx->animation_time * 3.0);

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_ADD);

    // Synthwave pink color
    uint8_t r = 0xe9, g = 0x45, b = 0x60;

    // Calculate perpendicular offset for parallel glow lines
    float dx = (float)(sx2 - sx1);
    float dy = (float)(sy2 - sy1);
    float len = sqrt(dx*dx + dy*dy);
    if (len < 1) len = 1;
    float px = -dy / len;
    float py = dx / len;

    // Draw multiple parallel lines with decreasing alpha for glow
    for (int layer = 5; layer >= 1; layer--) {
        float offset = layer * 2.0f;
        uint8_t alpha = (uint8_t)(pulse * (20 + (6 - layer) * 25));
        if (alpha > 255) alpha = 255;

        SDL_SetRenderDrawColor(ctx->renderer, r, g, b, alpha);

        // Draw offset lines on both sides
        render_line_dev(ctx,
            sx1 + (int)(px * offset), sy1 + (int)(py * offset),
            sx2 + (int)(px * offset), sy2 + (int)(py * offset));
        render_line_dev(ctx,
            sx1 - (int)(px * offset), sy1 - (int)(py * offset),
            sx2 - (int)(px * offset), sy2 - (int)(py * offset));
    }

    // Bright core line
    uint8_t core_alpha = (uint8_t)(pulse * 180);
    SDL_SetRenderDrawColor(ctx->renderer, 0xff, 0x80, 0xa0, core_alpha);
    render_line_dev(ctx, sx1, sy1, sx2, sy2);

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
}

// Draw animated chaser lights along a wire
void render_neon_chaser_line(RenderContext *ctx, float x1, float y1, float x2, float y2) {
    int sx1, sy1, sx2, sy2;
    render_world_to_screen(ctx, x1, y1, &sx1, &sy1);
    render_world_to_screen(ctx, x2, y2, &sx2, &sy2);

    float dx = (float)(sx2 - sx1);
    float dy = (float)(sy2 - sy1);
    float len = sqrt(dx*dx + dy*dy);
    if (len < 10) return;  // Skip very short wires

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_ADD);

    // Draw chaser dots along the wire
    int num_chasers = (int)(len / 30) + 1;
    if (num_chasers > 4) num_chasers = 4;

    for (int i = 0; i < num_chasers; i++) {
        double phase = fmod(ctx->animation_time * 2.0 + (double)i / num_chasers, 1.0);
        float t = (float)phase;

        int px = sx1 + (int)(dx * t);
        int py = sy1 + (int)(dy * t);

        // Outer glow (cyan)
        SDL_SetRenderDrawColor(ctx->renderer, 0x00, 0xff, 0xff, 0x50);
        for (int gy = -3; gy <= 3; gy++) {
            for (int gx = -3; gx <= 3; gx++) {
                if (gx*gx + gy*gy <= 9) {
                    SDL_RenderDrawPoint(ctx->renderer, px + gx, py + gy);
                }
            }
        }

        // Bright core
        SDL_SetRenderDrawColor(ctx->renderer, 0xff, 0xff, 0xff, 0xa0);
        SDL_RenderDrawPoint(ctx->renderer, px, py);
        SDL_RenderDrawPoint(ctx->renderer, px+1, py);
        SDL_RenderDrawPoint(ctx->renderer, px, py+1);
    }

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
}

/* How fast the flow dots should travel for a current of this size: log-compressed, because the
   templates span nine decades of current and a linear speed makes a microamp look stopped and an
   amp look like a strobe. Magnitude only - the sign is decided elsewhere. */
static double flow_speed(double amps) {
    double lg = log10(fabs(amps) + 1e-9);
    double s = 0.3 + (lg + 9.0) * 0.15;
    if (s < 0.2) s = 0.2;
    if (s > 3.0) s = 3.0;
    return s;
}

/* Where along an element its flow dots sit this frame, as a fraction of its length.
 *
 * Dots that only ever march forward say "there is a current here" and nothing else, and on an AC
 * element that is not merely incomplete, it is wrong: the charge in an AC circuit does not go
 * anywhere. It goes back and forth. Swapping the two endpoints on the sign of the current -
 * which is all this used to do - cannot show that either, because a 1 kHz source reverses about
 * thirty times between one frame and the next, so what came out was a jitter that read as
 * forward motion.
 *
 * The two cases are told apart by measurement rather than by asking the source what it is: a
 * running mean and mean-square of the SIGNED current say how much of it goes somewhere and how
 * much of it merely swings. |mean| ~ rms is direct current and marches; |mean| << rms is
 * alternating and oscillates; a rectifier's ripple sits between the two and does a little of
 * both, which is exactly what a rectifier does.
 *
 * The oscillation is drawn at a fixed readable rate, not at the true frequency. Sixty frames a
 * second cannot show a thousand reversals in one of them, and nothing is gained by trying: what
 * is true and worth showing at 1 kHz is THAT the current alternates, and the amplitude of the
 * swing carries how completely. */
/* Wrap into [0,1). fmod does not: the swing carries the offset negative for half its cycle and
   fmod(-0.2, 1) is -0.2, which puts a dot off the end of the wire it belongs to. */
static float flow_wrap(float t) {
    t -= (float)floor((double)t);
    if (t < 0.0f) t += 1.0f;                /* -0.0 and rounding at the boundary */
    if (t >= 1.0f) t = 0.0f;
    return t;
}

float render_flow_offset(FlowState *fs, double drift, double current, double dt, double clock) {
    if (!fs) return 0.0f;
    if (dt < 0) dt = 0;
    if (dt > 0.25) dt = 0.25;               /* a stalled frame must not jump the dots a lap */

    double steady = fabs(drift);
    if (steady > 1.0) steady = 1.0;

    /* The part that goes somewhere marches, in the direction the current is actually flowing. */
    double dir = current < 0 ? -1.0 : 1.0;
    double m = fs->march + steady * flow_speed(current) * dir * dt;
    m -= floor(m);
    fs->march = (float)m;

    /* ...and the part that merely swings, swings. */
    double swing = (1.0 - steady) * 0.30 * sin(6.28318530718 * 0.6 * clock);
    return (float)(m + swing);
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

/* A TO-220 package carries its name INSIDE the body, and the body scales with the zoom while
   the 8x8 font does not. Drawn from a fixed pixel offset, "LM317" is 40 px wide against a box
   that is 50 px only at zoom 1: zoom out and the name hangs out of both sides of the part it
   is naming. Centre it on its own width, and leave it out once the box is too small to hold
   it - an unlabelled box reads as a package, a box with text through its walls reads as a
   fault. The pin names sit outside the body, so they hug the drawn edge rather than a fixed
   offset, and they go once there is no room to tell which pin they belong to. */
#define REG_HALF_W 25.0f          /* render_regulator_box draws -25..+25 by -25..+20 */
#define REG_HALF_H 20.0f

static void render_package_label(RenderContext *ctx, float wx, float wy, const char *name) {
    int sx, sy;
    /* Above the body, not inside it, and centred on the string. A TO-220 is 50 units wide and
       the name wants 40 pixels of an 8x8 font that does not scale, so inside the box it only
       ever fits at full zoom - and hiding it below that leaves the part anonymous. Outside it
       always fits and always reads, which is what the MOSFET and BJT symbols already do.

       The offset is in pixels, not world units, for the same reason the pin names are: text
       that backs off in world units closes on text that does not, and at half zoom the part
       number lands on top of "IN". 34 clears the heat tab at full zoom and clears the pin
       row - which sits at -12 - at every zoom below it. */
    render_world_to_screen(ctx, wx, wy, &sx, &sy);
    render_draw_text_small(ctx, name, sx - (int)strlen(name) * 4, sy - 34, COLOR_TEXT);
}

static void render_package_pins(RenderContext *ctx, float wx, float wy,
                                const char *left, const char *right, const char *bottom) {
    if (ctx->zoom < 0.35f) return;          /* nothing legible to attach them to */
    int sx, sy;
    render_world_to_screen(ctx, wx, wy, &sx, &sy);
    int hw = (int)(REG_HALF_W * ctx->zoom), hh = (int)(REG_HALF_H * ctx->zoom);
    render_draw_text_small(ctx, left,  sx - hw - (int)strlen(left) * 8 - 2, sy - 12, COLOR_ACCENT);
    render_draw_text_small(ctx, right, sx + hw + 3, sy - 12, COLOR_ACCENT);
    render_draw_text_small(ctx, bottom, sx - (int)strlen(bottom) * 4, sy + hh + 14, COLOR_ACCENT);
}

/* How many components have been drawn with no symbol of their own since the counter was last
   read. --symbol-test asserts this stays zero across every placeable type. */
int g_render_missing_symbol = 0;

/* Which of the two looks everything is drawn in. See include/style.h. */
int g_draw_style = STYLE_SYNTHWAVE;
int g_style_in_canvas = 0;
uint8_t g_style_mapped_rgb[3] = { 255, 255, 255 };
int g_style_readback = 0;

const char *style_name(int style) {
    return (style == STYLE_SCHEMATIC) ? "schematic" : "synthwave";
}

/* Supersample factor new contexts are built with. 2 unless --ss says otherwise. */
int g_render_supersample = 2;

/* Set by --ui-scale; 0 means work it out from the display. */
float g_ui_scale_override = 0.0f;

/* Stroke weight for everything drawn on the canvas, in logical pixels. One pixel is what a
   plotter gives you and it reads as thin and fragile on a screen; a little over one and a half
   is what a printed schematic looks like. --line-weight sets it. */
float g_render_line_weight = 1.4f;

/* How many device pixels one UI pixel should be.

   The layout is written in pixels against a 720p window, so on a 1440p screen it draws at half
   the physical size it was designed at - legible only if you lean in, and the reason the text
   looked thin rather than small. Scaling by height against 900 holds the physical size roughly
   constant while still giving a large screen more room than a small one: 1366x768 keeps all of
   its pixels, and anything from 1080p up gets about 1600x900 of logical space with everything
   on it drawn correspondingly larger. */
float render_ui_scale(int device_h) {
    if (g_ui_scale_override > 0.0f) return g_ui_scale_override;
    if (device_h <= 0) return 1.0f;
    float s = (float)device_h / 900.0f;
    if (s < 1.0f) s = 1.0f;
    if (s > 3.0f) s = 3.0f;
    return s;
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
        case COMP_ARB_SOURCE: {
            // circle with a little arbitrary squiggle inside, so it reads as "table replay"
            render_draw_circle_rotated(ctx, comp->x, comp->y, 0, 0, 20, comp->rotation);
            render_draw_line_rotated(ctx, comp->x, comp->y, 0, -40, 0, -20, comp->rotation);
            render_draw_line_rotated(ctx, comp->x, comp->y, 0, 20, 0, 40, comp->rotation);
            render_draw_line_rotated(ctx, comp->x, comp->y, -11, 4, -6, -6, comp->rotation);
            render_draw_line_rotated(ctx, comp->x, comp->y, -6, -6, -1, 6, comp->rotation);
            render_draw_line_rotated(ctx, comp->x, comp->y, -1, 6, 4, -8, comp->rotation);
            render_draw_line_rotated(ctx, comp->x, comp->y, 4, -8, 9, 2, comp->rotation);
            break;
        }
        case COMP_AC_VOLTAGE:
            render_voltage_source(ctx, comp->x, comp->y, comp->rotation, true);
            // Live readout of a sweeping source: instantaneous frequency / amplitude
            if (comp->props.ac_voltage.frequency_sweep.enabled || comp->props.ac_voltage.amplitude_sweep.enabled) {
                /* Stack under the source's own value label, wherever that ended up. Fixed screen
                   offsets from the symbol's centre put this on the ground symbol below it, and
                   only lined up at all at zoom 1. */
                char lb[96]; float lx = comp->x, ly = comp->y;
                render_component_value_label(comp, lb, sizeof lb, &lx, &ly);
                int sx, sy;
                render_world_to_screen(ctx, lx, ly + CANVAS_TEXT_PX + 2, &sx, &sy);
                char rd[48];
                if (comp->props.ac_voltage.frequency_sweep.enabled) {
                    double f = sweep_get_value(&comp->props.ac_voltage.frequency_sweep,
                                               comp->props.ac_voltage.frequency, ctx->sim_time);
                    if (f >= 1e6) snprintf(rd, sizeof rd, "f=%.2fMHz", f / 1e6);
                    else if (f >= 1e3) snprintf(rd, sizeof rd, "f=%.2fkHz", f / 1e3);
                    else snprintf(rd, sizeof rd, "f=%.1fHz", f);
                    render_draw_text_small(ctx, rd, sx, sy, COLOR_ACCENT);
                }
                if (comp->props.ac_voltage.amplitude_sweep.enabled) {
                    double a = sweep_get_value(&comp->props.ac_voltage.amplitude_sweep,
                                               comp->props.ac_voltage.amplitude, ctx->sim_time);
                    snprintf(rd, sizeof rd, "A=%.2fV", a);
                    int ay = sy + (comp->props.ac_voltage.frequency_sweep.enabled ? (int)((CANVAS_TEXT_PX + 2) * ctx->zoom) : 0);
                    render_draw_text_small(ctx, rd, sx, ay, COLOR_ACCENT);
                }
            }
            break;
        case COMP_DC_CURRENT:
            render_current_source(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_RESISTOR:
            if (comp->props.resistor.high_power) { render_load_hp(ctx, comp->x, comp->y, comp->rotation); break; }
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

            // Check if LED is burning (current exceeds max_current)
            double current = comp->props.led.current;
            double max_current = comp->props.led.max_current;
            bool is_burning = (max_current > 0 && current > max_current);

            // Draw glow if LED has current
            if (current > 1e-6) {  // Threshold for visible glow
                int scr_x, scr_y;
                render_world_to_screen(ctx, comp->x, comp->y, &scr_x, &scr_y);

                if (is_burning) {
                    // Burning effect - flickering red/orange glow
                    double overcurrent_ratio = current / max_current;
                    double flicker = 0.7 + 0.3 * sin(SDL_GetTicks() * 0.02);  // Flicker animation
                    uint8_t burn_alpha = (uint8_t)(200 * fmin(1.0, overcurrent_ratio - 1.0) * flicker);

                    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_ADD);
                    // Draw burning glow - larger and more intense
                    for (int radius = 30; radius >= 5; radius -= 3) {
                        uint8_t glow_alpha = (uint8_t)(burn_alpha * (1.0 - (radius - 5) / 25.0));
                        // Orange-red burning color
                        SDL_SetRenderDrawColor(ctx->renderer, 255, (uint8_t)(50 * flicker), 0, glow_alpha);
                        int screen_radius = (int)(radius * ctx->zoom);
                        for (int dy = -screen_radius; dy <= screen_radius; dy++) {
                            int dx = (int)sqrt(screen_radius * screen_radius - dy * dy);
                            render_line_dev(ctx, scr_x - dx, scr_y + dy, scr_x + dx, scr_y + dy);
                        }
                    }
                    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
                } else {
                    // Normal LED glow - use max_current for proper scaling
                    double intensity = fmin(1.0, current / max_current);
                    // Increase brightness: boost intensity and use higher alpha
                    intensity = pow(intensity, 0.6);  // Gamma correction for brighter appearance
                    uint8_t alpha = (uint8_t)(intensity * 240);  // Increased from 200 to 240

                    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_ADD);
                    for (int radius = 20; radius >= 5; radius -= 3) {
                        uint8_t glow_alpha = (uint8_t)(alpha * (1.0 - (radius - 5) / 15.0));
                        SDL_SetRenderDrawColor(ctx->renderer, r, g, b, glow_alpha);
                        int screen_radius = (int)(radius * ctx->zoom);
                        for (int dy = -screen_radius; dy <= screen_radius; dy++) {
                            int dx = (int)sqrt(screen_radius * screen_radius - dy * dy);
                            render_line_dev(ctx, scr_x - dx, scr_y + dy, scr_x + dx, scr_y + dy);
                        }
                    }
                    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
                }
            }

            // Set LED symbol color based on wavelength or burning state
            if (!comp->selected && !comp->highlighted) {
                if (is_burning) {
                    // Burning LED shows red
                    render_set_color(ctx, (Color){0xff, 0x20, 0x20, 0xff});
                } else {
                    render_set_color(ctx, (Color){r, g, b, 0xff});
                }
            }
            render_led(ctx, comp->x, comp->y, comp->rotation);
            break;
        }
        case COMP_NPN_BJT:
            render_bjt(ctx, comp->x, comp->y, comp->rotation, false, comp->part[0] ? comp->part : "NPN");
            break;
        case COMP_PNP_BJT:
            render_bjt(ctx, comp->x, comp->y, comp->rotation, true, comp->part[0] ? comp->part : "PNP");
            break;
        case COMP_NMOS:
            render_mosfet(ctx, comp->x, comp->y, comp->rotation, false, comp->part[0] ? comp->part : "NMOS");
            break;
        case COMP_PMOS:
            render_mosfet(ctx, comp->x, comp->y, comp->rotation, true, comp->part[0] ? comp->part : "PMOS");
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
            /* wrapped, so a long note is a paragraph beside the circuit rather than a line
               wider than it - see label_wrap */
            {
                int fs = comp->props.text.font_size;
                if (fs < 1) fs = 1; if (fs > 3) fs = 3;
                int starts[CANVAS_TEXT_MAX_LINES], lens[CANVAS_TEXT_MAX_LINES];
                int nl = label_wrap(comp->props.text.text, CANVAS_TEXT_WRAP,
                                    starts, lens, CANVAS_TEXT_MAX_LINES);
                int line_h = render_text_px(ctx, fs) + 2;
                for (int li = 0; li < nl; li++) {
                    char line[256];
                    int n = lens[li] < (int)sizeof line - 1 ? lens[li] : (int)sizeof line - 1;
                    memcpy(line, comp->props.text.text + starts[li], n);
                    line[n] = 0;
                    render_draw_text_styled(ctx, line, sx, sy + li * line_h, text_color,
                                            fs, comp->props.text.bold,
                                            comp->props.text.italic,
                                            comp->props.text.underline);
                }
            }
            break;
        }
        // === NEW COMPONENT SYMBOLS ===
        case COMP_FUSE: {
            // Calculate heat level based on current relative to rating
            double heat_level = 0.0;
            if (!comp->props.fuse.blown && comp->props.fuse.rating > 0) {
                heat_level = comp->props.fuse.current / comp->props.fuse.rating;
                if (heat_level > 1.0) heat_level = 1.0;
            }
            render_fuse(ctx, comp->x, comp->y, comp->rotation,
                       comp->props.fuse.blown, heat_level);
            break;
        }
        case COMP_CRYSTAL:
            render_crystal(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_SPARK_GAP:
            render_spark_gap(ctx, comp->x, comp->y, comp->rotation);
            if (comp->props.spark_gap.conducting) {
                float ax, ay, bx, by;
                component_get_terminal_pos(comp, 0, &ax, &ay);
                component_get_terminal_pos(comp, 1, &bx, &by);
                // arc between the electrode tips (15 px in from each terminal)
                float dx = bx - ax, dy = by - ay, len = sqrtf(dx*dx + dy*dy);
                if (len > 1) { dx /= len; dy /= len; }
                render_arc_between(ctx, ax + dx*15, ay + dy*15, bx - dx*15, by - dy*15, 1.0f);
            }
            break;
        case COMP_TOROID:
            render_toroid(ctx, comp);
            break;
        case COMP_TLINE:
            render_tline(ctx, comp);
            break;
        case COMP_DELAY_LINE:
            render_delay_line(ctx, comp);
            break;
        case COMP_SOURCE_3PH:
            render_source_3ph(ctx, comp);
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
            /* Draw the blade where it actually is. It used to be hard-coded open, so a switch
               that was conducting - manually or from its control pin - looked identical to one
               that was not, and clicking it appeared to do nothing at all. */
            render_analog_switch(ctx, comp->x, comp->y, comp->rotation, comp->props.analog_switch.state);
            break;
        case COMP_DPDT_DRIVEN:
            render_dpdt_driven(ctx, comp->x, comp->y, comp->rotation, comp->props.dpdt_driven.thrown);
            break;
        case COMP_LAMP:
            render_lamp(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_ANTENNA_TX:
            render_antenna_tx(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_ANTENNA_RX:
            render_antenna_rx(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_BUS:
            render_bus(ctx, comp->x, comp->y, comp->rotation, comp->props.bus.width);
            break;
        case COMP_BUS_TAP:
            render_bus_tap(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_DC_MOTOR:
            render_dc_motor(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_VOLTMETER: {
            render_voltmeter(ctx, comp->x, comp->y, comp->rotation);
            // Display reading above the symbol
            int sx, sy;
            render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            char reading_str[32];
            double v = comp->props.voltmeter.reading;
            if (fabs(v) >= 1000.0)
                snprintf(reading_str, sizeof(reading_str), "%.2fkV", v / 1000.0);
            else if (fabs(v) >= 1.0)
                render_volt_str(reading_str, sizeof(reading_str), v);
            else if (fabs(v) >= 0.001)
                snprintf(reading_str, sizeof(reading_str), "%.2fmV", v * 1000.0);
            else
                snprintf(reading_str, sizeof(reading_str), "%.2fuV", v * 1e6);
            render_draw_text_small(ctx, reading_str, sx - 20, sy - 30, COLOR_ACCENT);
            break;
        }
        case COMP_AMMETER: {
            render_ammeter(ctx, comp->x, comp->y, comp->rotation);
            // Display reading above the symbol
            int sx, sy;
            render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            char reading_str[32];
            double i = comp->props.ammeter.reading;
            if (fabs(i) >= 1.0)
                snprintf(reading_str, sizeof(reading_str), "%.2fA", i);
            else if (fabs(i) >= 0.001)
                snprintf(reading_str, sizeof(reading_str), "%.2fmA", i * 1000.0);
            else if (fabs(i) >= 1e-6)
                snprintf(reading_str, sizeof(reading_str), "%.2fuA", i * 1e6);
            else
                snprintf(reading_str, sizeof(reading_str), "%.2fnA", i * 1e9);
            render_draw_text_small(ctx, reading_str, sx - 20, sy - 30, COLOR_ACCENT);
            break;
        }
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
            render_7seg_display(ctx, comp->x, comp->y, comp->rotation,
                                comp->props.seven_seg.currents,
                                comp->props.seven_seg.max_current);
            break;
        case COMP_LED_ARRAY:
            render_led_array(ctx, comp->x, comp->y, comp->rotation,
                           comp->props.led_array.currents, comp->props.led_array.failed,
                           comp->props.led_array.max_current, comp->props.led_array.color);
            break;
        case COMP_LED_MATRIX:
            render_led_matrix(ctx, comp->x, comp->y, comp->rotation,
                            comp->props.led_matrix.pixel_state, comp->props.led_matrix.color);
            break;
        case COMP_BCD_DECODER:
            render_bcd_decoder(ctx, comp->x, comp->y, comp->rotation);
            break;

        // Pin marker for subcircuit creation
        case COMP_PIN:
            render_pin(ctx, comp->x, comp->y, comp->rotation,
                      comp->props.pin.pin_number, comp->props.pin.pin_name);
            break;

        // User-defined sub-circuit / IC
        case COMP_SUBCIRCUIT:
            render_subcircuit(ctx, comp->x, comp->y, comp->rotation,
                            comp->props.subcircuit.def_id, comp->props.subcircuit.name);
            break;

        /* The programmable block: a package with its pin names on it, and a mark against each
           pin the code is currently driving. A board is the one part where a user needs to see
           at a glance which pins are outputs, because that is a decision the CODE made and is
           not written anywhere on the schematic. */
        case COMP_MCU: {
            const float hw = 70.0f, hh = 140.0f;
            Color body = {44, 52, 72, 255}, edge = {130, 130, 160, 255};
            render_set_color(ctx, body);
            render_fill_rect(ctx, comp->x - hw, comp->y - hh, hw * 2, hh * 2);
            render_set_color(ctx, edge);
            render_draw_line(ctx, comp->x - hw, comp->y - hh, comp->x + hw, comp->y - hh);
            render_draw_line(ctx, comp->x + hw, comp->y - hh, comp->x + hw, comp->y + hh);
            render_draw_line(ctx, comp->x + hw, comp->y + hh, comp->x - hw, comp->y + hh);
            render_draw_line(ctx, comp->x - hw, comp->y + hh, comp->x - hw, comp->y - hh);

            const ComponentTypeInfo *ci = component_get_info(COMP_MCU);
            for (int k = 0; k < MCU_PINS && ci && k < ci->num_terminals; k++) {
                float px = comp->x + ci->terminals[k].dx, py = comp->y + ci->terminals[k].dy;
                float inner = (ci->terminals[k].dx < 0) ? px + 16 : (ci->terminals[k].dx > 0) ? px - 16 : px;
                if (ci->terminals[k].dy > hh - 1) {
                    render_set_color(ctx, edge);
                    render_draw_line(ctx, px, py, px, comp->y + hh);
                } else {
                    /* A driven pin gets a lit lead. Low is drawn too, just dimmer - a pin held
                       low by the code is doing something, and looks nothing like a pin the
                       sketch never touched. */
                    Color lead = edge;
                    if (k < MCU_GND_PIN && comp->props.mcu.pin_drive[k] == 1) {
                        lead = comp->props.mcu.pin_level[k] > 0.5
                             ? (Color){120, 255, 160, 255} : (Color){70, 120, 90, 255};
                    } else if (k < MCU_GND_PIN && comp->props.mcu.pin_drive[k] == 2) {
                        lead = (Color){200, 170, 90, 255};
                    }
                    render_set_color(ctx, lead);
                    render_draw_line(ctx, px, py, inner, py);
                }
            }

            int sx, sy;
            render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            if (ctx->zoom >= 0.3f) {
                const char *title = comp->props.mcu.compiled ? "CODE" : "NO CODE";
                render_draw_text_small(ctx, title, sx - (int)strlen(title) * 4, sy - 8, COLOR_TEXT);
                for (int k = 0; k < MCU_PINS && ci && k < ci->num_terminals; k++) {
                    const char *nm = ci->terminals[k].name;
                    if (!nm || !nm[0]) continue;
                    int tx, ty;
                    render_world_to_screen(ctx, comp->x + ci->terminals[k].dx,
                                           comp->y + ci->terminals[k].dy, &tx, &ty);
                    if (ci->terminals[k].dx < 0)      tx += 6;
                    else if (ci->terminals[k].dx > 0) tx -= (int)strlen(nm) * 8 + 6;
                    else                              tx -= (int)strlen(nm) * 4;
                    render_draw_text_small(ctx, nm, tx, ty - 4, COLOR_ACCENT);
                }
            }
            break;
        }

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
        case COMP_COUNTER:
            render_counter(ctx, comp->x, comp->y, comp->rotation,
                           comp->props.counter.count, comp->props.counter.modulus);
            break;
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

        // Voltage regulators - use TO-220 style box with labels
        case COMP_LM317: {
            render_regulator_box(ctx, comp->x, comp->y, comp->rotation);
            render_package_label(ctx, comp->x, comp->y, "LM317");
            render_package_pins(ctx, comp->x, comp->y, "IN", "OUT", "ADJ");
            break;
        }
        case COMP_7805: {
            render_regulator_box(ctx, comp->x, comp->y, comp->rotation);
            render_package_label(ctx, comp->x, comp->y, "7805");
            render_package_pins(ctx, comp->x, comp->y, "IN", "OUT", "GND");
            break;
        }
        case COMP_TL431: {
            render_regulator_box(ctx, comp->x, comp->y, comp->rotation);
            render_package_label(ctx, comp->x, comp->y, "TL431");
            render_package_pins(ctx, comp->x, comp->y, "K", "A", "REF");
            break;
        }

        /* These four had no case at all and so drew nothing: on the canvas a battery, a
           high-power load and the two table/expression sources were two terminal dots with the
           wire running between them, which reads as a wire and not as a part. render_load_hp
           had even been written - it was just never called. */
        case COMP_BATTERY:
            render_battery(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_LOAD_HP:
            render_load_hp(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_PWL_SOURCE:
        case COMP_EXPR_SOURCE: {
            render_voltage_source(ctx, comp->x, comp->y, comp->rotation, false);
            int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
            const char *tag = (comp->type == COMP_PWL_SOURCE) ? "PWL" : "f(t)";
            render_draw_text_small(ctx, tag, sx - (int)strlen(tag) * 4, sy - 34, COLOR_TEXT);
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
            /* No symbol for this type. Silently drawing nothing is how a battery spent its
               life looking like a piece of wire: the part was in the netlist, carried current
               and appeared in every audit, and the only thing wrong with it was that you could
               not see it. A dashed box with a question mark says what is actually true. */
            g_render_missing_symbol++;
            for (int dx = -18; dx < 18; dx += 8) {
                render_draw_line_rotated(ctx, comp->x, comp->y, (float)dx, -18, (float)dx + 4, -18, comp->rotation);
                render_draw_line_rotated(ctx, comp->x, comp->y, (float)dx, 18, (float)dx + 4, 18, comp->rotation);
            }
            for (int dy = -18; dy < 18; dy += 8) {
                render_draw_line_rotated(ctx, comp->x, comp->y, -18, (float)dy, -18, (float)dy + 4, comp->rotation);
                render_draw_line_rotated(ctx, comp->x, comp->y, 18, (float)dy, 18, (float)dy + 4, comp->rotation);
            }
            {
                int sx, sy; render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
                render_draw_text_small(ctx, "?", sx - 4, sy - 4, COLOR_TEXT);
            }
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
        // Use signed wire current for direction
        // Positive current = flows from start_node to end_node
        // Negative current = flows from end_node to start_node
        double signed_current = wire->current;
        double abs_current = fabs(signed_current);

        // Show particles if any measurable current
        if (abs_current > 1e-9) {
            /* Always start -> end. Which way the dots actually travel is decided by
               render_flow_offset from the sign of the current over time, because swapping the
               ends on the instantaneous sign is what made an AC wire jitter rather than
               alternate. */
            float from_x = start->x, from_y = start->y;
            float to_x = end->x, to_y = end->y;

            float dx = to_x - from_x;
            float dy = to_y - from_y;
            float len = sqrt(dx*dx + dy*dy);

            if (len > 5) {  // Draw on wires >= 5 pixels
                // Normalize direction
                dx /= len;
                dy /= len;

                double log_current = log10(abs_current + 1e-9);  // for the dot brightness below
                /* A wire takes its answer from the net it is on, watched at the step rate. A
                   ground return sits at 0 V and has no answer of its own, so it borrows the
                   other end's - and if both ends are held, it is on a rail and marches. */
                double wdrift = flow_drift(&start->flow);
                if (start->flow.ms <= 1e-24) wdrift = flow_drift(&end->flow);
                double anim_phase = render_flow_offset(&wire->flow, wdrift, signed_current,
                                                       ctx->flow_dt, ctx->animation_time);

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
                    float t = flow_wrap(anim_phase + (i + 1) * particle_spacing);

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

    /* A named net says so on the drawing, because in a circuit that came from a table the names
       ARE the wires and a wire you cannot see is indistinguishable from one that is not there.

       Not the reference, though. Naming the ground net "0" next to a ground symbol says nothing
       the symbol has not already said, and it says it once per terminal - which on a circuit
       with a dozen grounded parts is a dozen labels carrying no information at all. Dimmed and
       tied to the same F2 that hides component values, so a drawing can be read without them. */
    if (node->name[0] && !node->is_ground && strcmp(node->name, "0") != 0 && ctx->show_values) {
        int sx, sy;
        render_world_to_screen(ctx, node->x, node->y, &sx, &sy);
        /* A tag on a stub, not a word floating in space. A terminal joined by name has no wire
           drawn to it - that is what a net label MEANS - and a bare dot with text beside it
           reads as a loose end instead. Drawn as a flag it reads as what it is: this pin is on
           that net, and so is every other pin wearing the same flag.

           Above the terminal rather than beside it. Beside puts it straight through the body of
           anything lying horizontally, which is most things - the label for a resistor's left
           end landed across its own zigzag. */
        int px = render_text_px(ctx, 1);
        int tw = (int)strlen(node->name) * px;
        int h = px + 5;
        int y1 = sy - 9, y0 = y1 - h;
        int x0 = sx - tw / 2 - 4, x1 = sx + tw / 2 + 4;
        Color c = { 0xc8, 0x9a, 0x50, 0xff };
        render_set_color(ctx, c);
        render_line_dev(ctx, (float)sx, (float)sy, (float)sx, (float)y1);
        render_line_dev(ctx, (float)x0, (float)y0, (float)x1, (float)y0);
        render_line_dev(ctx, (float)x0, (float)y1, (float)x1, (float)y1);
        render_line_dev(ctx, (float)x0, (float)y0, (float)x0, (float)y1);
        render_line_dev(ctx, (float)x1, (float)y0, (float)x1, (float)y1);
        render_draw_text_small(ctx, node->name, sx - tw / 2, y0 + 3, c);
    }
}

void render_probe(RenderContext *ctx, Circuit *circuit, Probe *probe, int index) {
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

    /* A schematic does not draw the instrument.
     *
     * The probe below is a picture of a piece of test gear: a moulded grip, a cable, a silver
     * tip, a yellow contact bead. That is exactly right in colour, where the grip is the channel
     * colour and reads at a glance. Drained to one ink it is four filled circles and five
     * parallel lines stacked in the same place, which comes out as a black lump sitting on top of
     * the very label it is meant to be naming.
     *
     * What a schematic draws instead is the measurement, not the thing measuring: a test point on
     * the net, a leader out to clear space, and an open terminal on the end for the channel name
     * to hang off. Same geometry, same anchor points, so the labels below - which are placed by
     * label.c against everything else on the canvas - land in exactly the same place either way. */
    if (g_draw_style == STYLE_SCHEMATIC && g_style_in_canvas) {
        float end_x = tip_x + handle_dx;
        float end_y = tip_y + handle_dy;
        render_set_color(ctx, COLOR_TEXT);          /* ink, whatever the channel colour is */
        render_fill_circle(ctx, tip_x, tip_y, 2.5f);
        render_draw_line(ctx, tip_x, tip_y, end_x, end_y);
        render_draw_circle(ctx, end_x, end_y, 4.0f);
    } else {
        /* A slim pen, not a lollipop.
         *
         * The old probe was three overlapping filled discs of radius 5 and 6 with a five-stroke
         * handle drawn through them and a white ring around the lot - about fourteen units
         * across, wider than the resistor it usually sits next to, and it read as a blob with a
         * wire coming out of it. It also hid whatever it was pointing at, which is the one thing
         * a probe must not do.
         *
         * This is the same probe drawn the way a real one looks: a thin barrel along the handle
         * axis, a collar at each end of it, a needle tapering to the contact point, and a short
         * cable tail. Everything is measured along the handle direction rather than in x and y,
         * so the shape stays a probe instead of a diagonal smear. */
        float ux = handle_dx, uy = handle_dy;
        float ul = (float)sqrt(ux*ux + uy*uy);
        if (ul < 1.0f) ul = 1.0f;
        ux /= ul; uy /= ul;                 /* tip -> handle */
        float px = -uy, py = ux;            /* across the barrel */

        /* The barrel stops short of the tip; the gap is the needle. */
        float b0x = tip_x + ux * 10.0f, b0y = tip_y + uy * 10.0f;
        float b1x = tip_x + handle_dx,  b1y = tip_y + handle_dy;

        // Cable: a short tail leaving the cap and bending away
        render_set_color(ctx, (Color){0x56, 0x56, 0x70, 0xff});
        render_draw_line(ctx, b1x, b1y, b1x + ux * 7 - px * 7, b1y + uy * 7 - py * 7);
        render_draw_line(ctx, b1x + ux * 7 - px * 7, b1y + uy * 7 - py * 7,
                              b1x + ux * 8 - px * 18, b1y + uy * 8 - py * 18);

        /* Barrel: a grip rather than a hairline. Three strokes 0.75 apart came to about three
           units across, which at the usual zoom is the same weight as a wire - so the probe read
           as flat, and as one more line on the canvas. This is wider, tapered slightly toward the
           cap the way a moulded handle is, and shaded down one side with a highlight along the
           other so it reads as round instead of as a ribbon. */
        render_set_color(ctx, probe->color);
        for (float o = -2.6f; o <= 2.6f; o += 0.5f)
            render_draw_line(ctx, b0x + px*o, b0y + py*o, b1x + px*o*0.78f, b1y + py*o*0.78f);

        Color shade = { (uint8_t)(probe->color.r * 0.45f), (uint8_t)(probe->color.g * 0.45f),
                        (uint8_t)(probe->color.b * 0.45f), 0xff };
        Color lite  = { (uint8_t)(probe->color.r + (255 - probe->color.r) * 0.60f),
                        (uint8_t)(probe->color.g + (255 - probe->color.g) * 0.60f),
                        (uint8_t)(probe->color.b + (255 - probe->color.b) * 0.60f), 0xff };
        render_set_color(ctx, shade);
        render_draw_line(ctx, b0x + px*2.6f, b0y + py*2.6f, b1x + px*2.05f, b1y + py*2.05f);
        render_set_color(ctx, lite);
        render_draw_line(ctx, b0x - px*1.3f, b0y - py*1.3f, b1x - px*1.00f, b1y - py*1.00f);

        /* The finger guard - the flange a real probe carries just behind the needle, the thing
           that stops a hand sliding onto the point. It is also what gives the silhouette its
           shape: an upside-down T, crossbar at the business end and the shaft running back to
           the cable. It sits ten units back from the contact point, so it still does not cover
           whatever the probe is pointing at. */
        render_set_color(ctx, probe->color);
        for (float t = -1.0f; t <= 3.0f; t += 0.5f) {
            /* Rounded ends: pull the bar in slightly on its outermost rows so the crossbar reads
               as a moulded flange and not as a rectangle stuck through the handle. */
            float w = (t < -0.5f || t > 2.5f) ? 6.4f : 7.4f;
            render_draw_line(ctx, b0x + ux*t - px*w, b0y + uy*t - py*w,
                                  b0x + ux*t + px*w, b0y + uy*t + py*w);
        }
        render_set_color(ctx, shade);
        render_draw_line(ctx, b0x + ux*3.0f - px*6.4f, b0y + uy*3.0f - py*6.4f,
                              b0x + ux*3.0f + px*6.4f, b0y + uy*3.0f + py*6.4f);
        render_set_color(ctx, lite);
        render_draw_line(ctx, b0x - ux*1.0f - px*6.4f, b0y - uy*1.0f - py*6.4f,
                              b0x - ux*1.0f + px*6.4f, b0y - uy*1.0f + py*6.4f);

        // Cap at the cable end
        render_set_color(ctx, probe->color);
        render_fill_circle(ctx, b1x, b1y, 2.8f);

        // Needle: two lines tapering from the collar to the contact point
        render_set_color(ctx, (Color){0xd8, 0xd8, 0xe4, 0xff});
        render_draw_line(ctx, tip_x, tip_y, b0x + px * 1.5f, b0y + py * 1.5f);
        render_draw_line(ctx, tip_x, tip_y, b0x - px * 1.5f, b0y - py * 1.5f);

        // Contact point
        render_set_color(ctx, (Color){0xff, 0xee, 0x70, 0xff});
        render_fill_circle(ctx, tip_x, tip_y, 1.7f);
    }

    /* Channel name and voltage, at whichever offsets around the probe are clear of the text
       already on the canvas. Both were pinned, which on 47 of 188 templates printed one of them
       straight through a component's value label or a template's annotation. label.c decides, so
       the audit that measures this asks the same question rather than keeping its own answer. */
    int sx, sy;
    float nx[MAX_PROBES], ny[MAX_PROBES], vx[MAX_PROBES], vy[MAX_PROBES];
    int placed = circuit && index >= 0 && index < circuit->num_probes && index < MAX_PROBES;
    if (placed) probe_text_positions(circuit, nx, ny, vx, vy);

    // Draw channel label near handle
    if (placed) render_world_to_screen(ctx, nx[index], ny[index], &sx, &sy);
    else        render_world_to_screen(ctx, tip_x + handle_dx - 10 - 8, tip_y + handle_dy + 5, &sx, &sy);
    if (probe->label[0]) {
        render_draw_text(ctx, probe->label, sx, sy, probe->color);
    } else {
        char buf[16];
        snprintf(buf, sizeof(buf), "CH%d", index + 1);
        render_draw_text(ctx, buf, sx - 4, sy, probe->color);
    }

    /* Voltage reading near the tip - at whichever of a few offsets around it is clear of the
       text already on the canvas. It was pinned to tip + (10, 10), which on 47 of 188 templates
       printed it straight through a component's value label. label.c decides, so the audit that
       measures this asks the same question rather than keeping its own copy of the answer. */
    char volt_str[16];
    render_volt_str(volt_str, sizeof(volt_str), probe->voltage);
    float rx = tip_x + 10, ry = tip_y + 10;
    if (placed) { rx = vx[index]; ry = vy[index]; }
    render_world_to_screen(ctx, rx, ry, &sx, &sy);
    render_draw_text(ctx, volt_str, sx, sy, probe->color);
}

// Draw smoke particles from failed components (magic smoke effect)
static void render_component_smoke(RenderContext *ctx, Component *comp) {
    if (!comp) return;

    /* A part that has failed stays visibly failed. The smoke is a puff and expires in about a
       second, and before this that was the only sign: after it cleared, a burned part looked
       exactly like a working one on a canvas where it was now an open circuit. A cross through
       it says so for as long as it is dead. */
    if (comp->thermal.failed) {
        int fx, fy;
        render_world_to_screen(ctx, comp->x, comp->y, &fx, &fy);
        int r = (int)(14 * ctx->zoom);
        if (r < 5) r = 5;
        Color charred = { 235, 90, 60, 255 };
        render_set_color(ctx, charred);
        render_draw_line_screen(ctx, fx - r, fy - r, fx + r, fy + r);
        render_draw_line_screen(ctx, fx - r, fy + r, fx + r, fy - r);
    }

    if (!comp->thermal.smoke_active || comp->thermal.num_smoke == 0) return;

    int cx, cy;
    render_world_to_screen(ctx, comp->x, comp->y, &cx, &cy);

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < comp->thermal.num_smoke; i++) {
        SmokeParticle *p = &comp->thermal.smoke[i];
        if (p->life <= 0) continue;

        // Screen position of smoke particle
        int sx = cx + (int)(p->x * ctx->zoom);
        int sy = cy + (int)(p->y * ctx->zoom);
        int size = (int)(p->size * ctx->zoom);

        // Draw smoke as semi-transparent dark gray circles
        uint8_t gray = 40 + (uint8_t)(60 * (1.0f - p->life));  // Gets lighter as it fades
        SDL_SetRenderDrawColor(ctx->renderer, gray, gray, gray, p->alpha);

        // Draw filled circle for smoke puff
        for (int dy = -size; dy <= size; dy++) {
            for (int dx = -size; dx <= size; dx++) {
                if (dx * dx + dy * dy <= size * size) {
                    SDL_RenderDrawPoint(ctx->renderer, sx + dx, sy + dy);
                }
            }
        }
    }

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_NONE);
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

    // Exact current from the solver: terminal_current[0] > 0 means current enters at
    // terminal 0 and leaves at terminal 1, so particles travel t0 -> t1. Sources therefore
    // show flow from - to + inside the source automatically.
    double current = comp->terminal_current[0];
    double abs_current = fabs(current);
    if (abs_current < 1e-9) return;

    /* Always terminal 0 -> terminal 1. render_flow_offset decides which way the dots go, from
       the sign of the current over time rather than at this instant - which is what lets an AC
       part show the charge going back and forth instead of jittering. */
    float from_x = t0_x, from_y = t0_y;
    float to_x = t1_x, to_y = t1_y;

    float dx = to_x - from_x;
    float dy = to_y - from_y;
    float len = sqrt(dx*dx + dy*dy);

    if (len < 5) return;  // Too short

    // Normalize direction
    dx /= len;
    dy /= len;

    double log_current = log10(abs_current + 1e-9);   // for the dot brightness below
    /* Measured across the part itself, every step, so a 60 Hz part cannot alias to a steady one. */
    double anim_phase = render_flow_offset(&comp->flow, flow_drift(&comp->flow), current,
                                           ctx->flow_dt, ctx->animation_time);

    // Particle spacing - about 20 pixels apart, same as wires
    int num_particles = (int)(len / 20) + 1;
    if (num_particles > 8) num_particles = 8;
    if (num_particles < 1) num_particles = 1;
    float particle_spacing = 1.0f / (num_particles + 1);

    // Cyan particles - brighter for higher current
    uint8_t base_intensity = 180;
    uint8_t intensity = (uint8_t)(base_intensity + fmin(log_current + 6.0, 3.0) * 25);

    for (int i = 0; i < num_particles; i++) {
        float t = flow_wrap(anim_phase + (i + 1) * particle_spacing);

        float particle_x = from_x + dx * len * t;
        float particle_y = from_y + dy * len * t;

        // Draw glowing particle
        render_set_color(ctx, (Color){0x00, 0xff, 0xff, 0x30});
        render_fill_circle(ctx, particle_x, particle_y, 3);

        render_set_color(ctx, (Color){0x00, 0xff, 0xff, 0x60});
        render_fill_circle(ctx, particle_x, particle_y, 2);

        render_set_color(ctx, (Color){0x00, intensity, intensity, 0xff});
        render_fill_circle(ctx, particle_x, particle_y, 1.5f);

        render_set_color(ctx, (Color){0xff, 0xff, 0xff, intensity});
        render_fill_circle(ctx, particle_x, particle_y, 0.8f);
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
        if (ctx->show_values) render_component_value(ctx, circuit->components[i]);
    }

    // Draw current flow animation through components (after components so particles appear on top)
    for (int i = 0; i < circuit->num_components; i++) {
        render_component_current_flow(ctx, circuit->components[i], circuit);
    }

    // Draw thermal heatmap overlay (if enabled)
    if (ctx->show_heatmap) {
        for (int i = 0; i < circuit->num_components; i++) {
            render_heatmap_overlay(ctx, circuit->components[i]);
        }
    }

    // Draw smoke from failed components (magic smoke effect)
    for (int i = 0; i < circuit->num_components; i++) {
        render_component_smoke(ctx, circuit->components[i]);
    }

    // Draw probes
    for (int i = 0; i < circuit->num_probes; i++) {
        render_probe(ctx, circuit, &circuit->probes[i], i);
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

/* A battery: alternating long and short plates, the symbol everyone learns first.

   There was no case for COMP_BATTERY in render_component at all, so a cell drew nothing but its
   two terminal dots and read on the canvas as a piece of bare wire with a gap in it - present in
   the netlist, carrying current, invisible on the schematic. Terminals are at (0,-40) and
   (0,+40); two cells are drawn, long plate at the positive end. */
void render_battery(RenderContext *ctx, float x, float y, int rotation) {
    /* leads in to the outermost plates */
    render_draw_line_rotated(ctx, x, y, 0, -40, 0, -18, rotation);
    render_draw_line_rotated(ctx, x, y, 0,  18, 0,  40, rotation);
    /* plate pairs: long (positive) then short, twice */
    const float plate[4][2] = { { -18, 14 }, { -10, 7 }, { 2, 14 }, { 10, 7 } };
    for (int i = 0; i < 4; i++) {
        float py = plate[i][0], half = plate[i][1];
        render_draw_line_rotated(ctx, x, y, -half, py, half, py, rotation);
    }
    /* the gaps between plates are the cell; join the pairs so it reads as one part */
    render_draw_line_rotated(ctx, x, y, 0, -10, 0, 2, rotation);
    /* + beside the long plate, so which way round it goes is on the drawing */
    render_draw_line_rotated(ctx, x, y, 14, -24, 20, -24, rotation);
    render_draw_line_rotated(ctx, x, y, 17, -27, 17, -21, rotation);
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

// High-power load: IEC box symbol with a bold outline (no thermal colouring)
void render_load_hp(RenderContext *ctx, float x, float y, int rotation) {
    render_draw_line_rotated(ctx, x, y, -40, 0, -28, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 28, 0, 40, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -28, -9, 28, -9, rotation);
    render_draw_line_rotated(ctx, x, y, -28, 9, 28, 9, rotation);
    render_draw_line_rotated(ctx, x, y, -28, -9, -28, 9, rotation);
    render_draw_line_rotated(ctx, x, y, 28, -9, 28, 9, rotation);
    render_draw_line_rotated(ctx, x, y, -28, -8, 28, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -28, 8, 28, 8, rotation);
    // diagonal hatch = "load"
    render_draw_line_rotated(ctx, x, y, -20, 8, -12, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -4, 8, 4, -8, rotation);
    render_draw_line_rotated(ctx, x, y, 12, 8, 20, -8, rotation);
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

    // Add "(IDEAL)" label
    int sx, sy;
    render_world_to_screen(ctx, x, y - 40, &sx, &sy);
    Color label_color = {0x80, 0x80, 0x80, 0xff};  // Gray
    render_draw_text(ctx, "(IDEAL)", sx - 18, sy, label_color);
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

void render_pin(RenderContext *ctx, float x, float y, int rotation, int pin_number, const char *pin_name) {
    // Pin marker: diamond shape with connection point
    // Terminal at (40, 0) for wire connection

    // Connection line from diamond to terminal
    render_draw_line_rotated(ctx, x, y, 12, 0, 40, 0, rotation);

    // Diamond shape (rotated square)
    render_draw_line_rotated(ctx, x, y, 0, -12, 12, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 12, 0, 0, 12, rotation);
    render_draw_line_rotated(ctx, x, y, 0, 12, -12, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -12, 0, 0, -12, rotation);

    // Small filled circle at center
    render_fill_circle(ctx, x, y, 4);

    // Draw pin label above the diamond
    char label[24];
    if (pin_name && pin_name[0] != '\0') {
        snprintf(label, sizeof(label), "%s", pin_name);
    } else {
        snprintf(label, sizeof(label), "P%d", pin_number);
    }

    // Convert to screen coords for text
    int sx, sy;
    render_world_to_screen(ctx, x, y - 20, &sx, &sy);
    render_draw_text_small(ctx, label, sx - 10, sy, (Color){0, 255, 255, 255});  // Cyan label
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

void render_short_circuit_highlights(RenderContext *ctx, Circuit *circuit,
                                     int *comp_ids, int comp_count) {
    if (!ctx || !circuit || !comp_ids || comp_count <= 0) return;

    Uint32 ticks = SDL_GetTicks();
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < comp_count; i++) {
        // Find component by ID
        Component *comp = NULL;
        for (int j = 0; j < circuit->num_components; j++) {
            if (circuit->components[j] && circuit->components[j]->id == comp_ids[i]) {
                comp = circuit->components[j];
                break;
            }
        }
        if (!comp) continue;

        // Convert world to screen coordinates
        int sx, sy;
        render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
        int half_size = (int)(40 * ctx->zoom);

        // === FIRE/BURNING GLOW EFFECT ===
        // Flickering orange/red glow around component
        float flicker = 0.5f + 0.5f * sinf((float)ticks * 0.015f + comp->id * 1.7f);
        float flicker2 = 0.5f + 0.5f * sinf((float)ticks * 0.023f + comp->id * 2.3f);

        // Draw multiple layers of fire glow
        for (int layer = 3; layer >= 0; layer--) {
            int expand = layer * 4;
            uint8_t alpha = (uint8_t)(60 + 40 * flicker - layer * 15);
            uint8_t red = (uint8_t)(255);
            uint8_t green = (uint8_t)(50 + 100 * flicker2 - layer * 20);
            uint8_t blue = 0;

            SDL_SetRenderDrawColor(ctx->renderer, red, green, blue, alpha);
            SDL_Rect glow = {
                sx - half_size - 5 - expand,
                sy - half_size - 5 - expand,
                half_size * 2 + 10 + expand * 2,
                half_size * 2 + 10 + expand * 2
            };
            SDL_RenderFillRect(ctx->renderer, &glow);
        }

        // === SMOKE PARTICLES ===
        // Draw rising smoke particles
        for (int p = 0; p < 12; p++) {
            // Use deterministic pseudo-random based on component ID and particle index
            unsigned int seed = comp->id * 17 + p * 31;
            float px_offset = ((seed * 1103515245 + 12345) % 1000) / 1000.0f - 0.5f;
            float speed_factor = 0.3f + ((seed * 1103515247 + 12347) % 1000) / 2000.0f;
            float phase_offset = ((seed * 1103515249 + 12349) % 1000) / 1000.0f * 6.28f;

            // Particle rises and resets in a cycle
            float cycle_time = 2000.0f;  // 2 second cycle
            float t = fmodf((float)ticks + phase_offset * 1000.0f, cycle_time) / cycle_time;

            // Position: start at component, rise upward
            float particle_x = sx + px_offset * half_size * 1.5f;
            float particle_y = sy - t * 80 * speed_factor * ctx->zoom;

            // Slight horizontal drift
            particle_x += sinf(t * 3.14159f + phase_offset) * 10 * ctx->zoom;

            // Size grows then shrinks
            float size = (3 + 4 * sinf(t * 3.14159f)) * ctx->zoom;

            // Alpha fades out as it rises
            uint8_t smoke_alpha = (uint8_t)(150 * (1.0f - t * t));

            // Gray smoke color with slight variation
            uint8_t gray = (uint8_t)(40 + ((seed * 13) % 30));
            SDL_SetRenderDrawColor(ctx->renderer, gray, gray, gray, smoke_alpha);

            // Draw smoke particle as filled circle
            for (int dy = -(int)size; dy <= (int)size; dy++) {
                for (int dx = -(int)size; dx <= (int)size; dx++) {
                    if (dx*dx + dy*dy <= (int)(size*size)) {
                        SDL_RenderDrawPoint(ctx->renderer,
                            (int)particle_x + dx,
                            (int)particle_y + dy);
                    }
                }
            }
        }

        // === BLINKING RED BORDER ===
        // Blinking red border (original effect)
        bool border_visible = ((ticks / 200) % 2) == 0;
        if (border_visible) {
            SDL_SetRenderDrawColor(ctx->renderer, 0xff, 0x20, 0x20, 0xe0);
            SDL_Rect rect = {
                sx - half_size - 5,
                sy - half_size - 5,
                half_size * 2 + 10,
                half_size * 2 + 10
            };
            SDL_RenderDrawRect(ctx->renderer, &rect);
            rect.x -= 2;
            rect.y -= 2;
            rect.w += 4;
            rect.h += 4;
            SDL_RenderDrawRect(ctx->renderer, &rect);
        }

        // === "SHORT!" TEXT ===
        // Draw "SHORT!" text above the component
        render_draw_text(ctx, "SHORT!", sx - 25, sy - half_size - 30, (Color){0xff, 0x40, 0x40, 0xff});
    }

    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_NONE);
}

void render_open_circuit_highlights(RenderContext *ctx, Circuit *circuit,
                                    int *comp_ids, int comp_count) {
    if (!ctx || !circuit || !comp_ids || comp_count <= 0) return;

    Uint32 ticks = SDL_GetTicks();
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < comp_count; i++) {
        // Find component by ID
        Component *comp = NULL;
        for (int j = 0; j < circuit->num_components; j++) {
            if (circuit->components[j] && circuit->components[j]->id == comp_ids[i]) {
                comp = circuit->components[j];
                break;
            }
        }
        if (!comp) continue;

        // Convert world to screen coordinates
        int sx, sy;
        render_world_to_screen(ctx, comp->x, comp->y, &sx, &sy);
        int half_size = (int)(40 * ctx->zoom);

        // === ELECTRIC/SPARKING GLOW EFFECT ===
        // Flickering yellow/cyan glow around component
        float flicker = 0.5f + 0.5f * sinf((float)ticks * 0.02f + comp->id * 1.7f);
        float flicker2 = 0.5f + 0.5f * sinf((float)ticks * 0.03f + comp->id * 2.3f);

        // Draw multiple layers of electric glow
        for (int layer = 3; layer >= 0; layer--) {
            int expand = layer * 4;
            uint8_t alpha = (uint8_t)(60 + 40 * flicker - layer * 15);
            uint8_t red = (uint8_t)(255);
            uint8_t green = (uint8_t)(200 + 55 * flicker2);
            uint8_t blue = (uint8_t)(50 * flicker);

            SDL_SetRenderDrawColor(ctx->renderer, red, green, blue, alpha);
            SDL_Rect glow = {
                sx - half_size - 5 - expand,
                sy - half_size - 5 - expand,
                half_size * 2 + 10 + expand * 2,
                half_size * 2 + 10 + expand * 2
            };
            SDL_RenderFillRect(ctx->renderer, &glow);
        }

        // === BLINKING YELLOW BORDER ===
        bool border_visible = ((ticks / 200) % 2) == 0;
        if (border_visible) {
            SDL_SetRenderDrawColor(ctx->renderer, 0xff, 0xcc, 0x00, 0xe0);
            SDL_Rect rect = {
                sx - half_size - 5,
                sy - half_size - 5,
                half_size * 2 + 10,
                half_size * 2 + 10
            };
            SDL_RenderDrawRect(ctx->renderer, &rect);
            rect.x -= 2;
            rect.y -= 2;
            rect.w += 4;
            rect.h += 4;
            SDL_RenderDrawRect(ctx->renderer, &rect);
        }

        // === "OPEN!" TEXT ===
        // Draw "OPEN!" text above the component
        render_draw_text(ctx, "OPEN!", sx - 22, sy - half_size - 30, (Color){0xff, 0xcc, 0x00, 0xff});
    }

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

// Fuse - rectangle with S-curve inside (shows blown state and heat glow)
void render_fuse(RenderContext *ctx, float x, float y, int rotation, bool blown, double heat_level) {
    SDL_Renderer *renderer = ctx->renderer;

    // Save original color
    uint8_t orig_r, orig_g, orig_b, orig_a;
    SDL_GetRenderDrawColor(renderer, &orig_r, &orig_g, &orig_b, &orig_a);

    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -20, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 20, 0, 40, 0, rotation);

    // Rectangle body - turns red/orange when hot, gray when blown
    if (blown) {
        // Blown fuse: gray body
        SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);
    } else if (heat_level > 0.5) {
        // Hot fuse: orange/red tint based on heat level
        uint8_t r = (uint8_t)(0x00 + (0xff - 0x00) * heat_level);
        uint8_t g = (uint8_t)(0xd9 - (0xd9 - 0x40) * heat_level);
        uint8_t b = (uint8_t)(0xff - (0xff - 0x00) * heat_level);
        SDL_SetRenderDrawColor(renderer, r, g, b, 0xff);
    }
    render_draw_line_rotated(ctx, x, y, -20, -8, 20, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -20, 8, 20, 8, rotation);
    render_draw_line_rotated(ctx, x, y, -20, -8, -20, 8, rotation);
    render_draw_line_rotated(ctx, x, y, 20, -8, 20, 8, rotation);

    // Restore color for element
    SDL_SetRenderDrawColor(renderer, orig_r, orig_g, orig_b, orig_a);

    if (blown) {
        // Blown fuse: broken element with gap in the middle
        // Left half of element (broken)
        SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xff);  // Gray for broken parts
        render_draw_line_rotated(ctx, x, y, -15, 0, -10, -4, rotation);
        render_draw_line_rotated(ctx, x, y, -10, -4, -6, 2, rotation);  // Broken end

        // Right half of element (broken)
        render_draw_line_rotated(ctx, x, y, 6, -2, 10, -4, rotation);  // Broken start
        render_draw_line_rotated(ctx, x, y, 10, -4, 15, 0, rotation);

        // X mark in the gap to indicate blown
        SDL_SetRenderDrawColor(renderer, 0xff, 0x44, 0x44, 0xff);  // Red X
        render_draw_line_rotated(ctx, x, y, -4, -4, 4, 4, rotation);
        render_draw_line_rotated(ctx, x, y, -4, 4, 4, -4, rotation);
    } else {
        // Intact fuse: S-curve element
        // Apply heat color if hot
        if (heat_level > 0.5) {
            uint8_t r = (uint8_t)(0x00 + (0xff - 0x00) * heat_level);
            uint8_t g = (uint8_t)(0xd9 - (0xd9 - 0x60) * heat_level);
            uint8_t b = (uint8_t)(0xff - (0xff - 0x20) * heat_level);
            SDL_SetRenderDrawColor(renderer, r, g, b, 0xff);
        }
        render_draw_line_rotated(ctx, x, y, -15, 0, -10, -4, rotation);
        render_draw_line_rotated(ctx, x, y, -10, -4, -5, 4, rotation);
        render_draw_line_rotated(ctx, x, y, -5, 4, 0, -4, rotation);
        render_draw_line_rotated(ctx, x, y, 0, -4, 5, 4, rotation);
        render_draw_line_rotated(ctx, x, y, 5, 4, 10, -4, rotation);
        render_draw_line_rotated(ctx, x, y, 10, -4, 15, 0, rotation);
    }

    // Restore original color
    SDL_SetRenderDrawColor(renderer, orig_r, orig_g, orig_b, orig_a);
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

// Jagged lightning polyline with a glow pass; jitter is re-rolled every frame on purpose
static void render_arc_between(RenderContext *ctx, float x1, float y1, float x2, float y2, float intensity) {
    float dx = x2 - x1, dy = y2 - y1, len = sqrtf(dx*dx + dy*dy);
    if (len < 2) return;
    float nx = -dy / len, ny = dx / len;
    int segs = (int)(len / 6) + 3; if (segs > 24) segs = 24;
    float px = x1, py = y1;
    float amp = fminf(len * 0.18f, 14.0f) * intensity;
    for (int pass = 0; pass < 2; pass++) {
        render_set_color(ctx, pass == 0 ? (Color){120, 170, 255, 110} : (Color){235, 245, 255, 255});
        px = x1; py = y1;
        for (int i = 1; i <= segs; i++) {
            float t = (float)i / segs;
            float j = (i == segs) ? 0 : ((rand() % 2001) - 1000) / 1000.0f * amp;
            float qx = x1 + dx * t + nx * j, qy = y1 + dy * t + ny * j;
            if (pass == 0) {
                render_draw_line(ctx, px + nx, py + ny, qx + nx, qy + ny);
                render_draw_line(ctx, px - nx, py - ny, qx - nx, qy - ny);
            } else {
                render_draw_line(ctx, px, py, qx, qy);
            }
            px = qx; py = qy;
        }
    }
}

// Three-phase source: circle with three sine strokes, leads A/B/C to the right, N below
void render_source_3ph(RenderContext *ctx, Component *comp) {
    float x = comp->x, y = comp->y; int rot = comp->rotation;
    const int N = 32;
    float lx = x + 24, ly = y;
    for (int i = 1; i <= N; i++) {
        float a = (float)(2.0 * M_PI * i / N);
        float qx = x + 24 * cosf(a), qy = y + 24 * sinf(a);
        render_draw_line(ctx, lx, ly, qx, qy);
        lx = qx; ly = qy;
    }
    for (int k = 0; k < 3; k++) {
        float oy = -8 + 8 * k;
        float px = -14, py = oy;
        for (int i = 1; i <= 12; i++) {
            float t = (float)i / 12;
            float qx = -14 + 28 * t, qy = oy - 5 * sinf((float)(2 * M_PI * t));
            render_draw_line_rotated(ctx, x, y, px, py, qx, qy, rot);
            px = qx; py = qy;
        }
    }
    // leads: A (40,-20), B (40,0), C (40,20) from the circle edge; N (0,40)
    render_draw_line_rotated(ctx, x, y, 22, -9, 32, -20, rot);
    render_draw_line_rotated(ctx, x, y, 32, -20, 40, -20, rot);
    render_draw_line_rotated(ctx, x, y, 24, 0, 40, 0, rot);
    render_draw_line_rotated(ctx, x, y, 22, 9, 32, 20, rot);
    render_draw_line_rotated(ctx, x, y, 32, 20, 40, 20, rot);
    render_draw_line_rotated(ctx, x, y, 0, 24, 0, 40, rot);
}

// Transmission line: a box with two little towers; the pi legs are drawn for the pi model
void render_tline(RenderContext *ctx, Component *comp) {
    float x = comp->x, y = comp->y; int rot = comp->rotation;
    /* The body was 52 x 18 with towers 15 px tall - smaller than the resistor next to it, and
       the two masts and their catenary were too small to read as anything. Same terminals, so
       nothing rewires: the body grows into the leads and, mostly, downwards. */
    render_draw_line_rotated(ctx, x, y, -40, 0, -32, 0, rot);
    render_draw_line_rotated(ctx, x, y, 32, 0, 40, 0, rot);
    // body
    render_draw_line_rotated(ctx, x, y, -32, -22, 32, -22, rot);
    render_draw_line_rotated(ctx, x, y, -32, 22, 32, 22, rot);
    render_draw_line_rotated(ctx, x, y, -32, -22, -32, 22, rot);
    render_draw_line_rotated(ctx, x, y, 32, -22, 32, 22, rot);
    // two towers (lattice masts) inside the body
    for (int t = -1; t <= 1; t += 2) {
        float cx = 15.0f * t;
        render_draw_line_rotated(ctx, x, y, cx - 9, 19, cx, -17, rot);
        render_draw_line_rotated(ctx, x, y, cx + 9, 19, cx, -17, rot);
        render_draw_line_rotated(ctx, x, y, cx - 13, -8, cx + 13, -8, rot);   // upper cross-arm
        render_draw_line_rotated(ctx, x, y, cx - 9, 2, cx + 9, 2, rot);       // lower cross-arm
        render_draw_line_rotated(ctx, x, y, cx - 6, 10, cx + 6, 10, rot);     // lattice brace
    }
    // catenary between the towers
    render_draw_line_rotated(ctx, x, y, -15, -8, -6, -2, rot);
    render_draw_line_rotated(ctx, x, y, -6, -2, 6, -2, rot);
    render_draw_line_rotated(ctx, x, y, 6, -2, 15, -8, rot);
    if (comp->props.tline.model >= 2) {
        // pi legs: short capacitor ticks below each end
        for (int t = -1; t <= 1; t += 2) {
            float cx = 26.0f * t;
            render_draw_line_rotated(ctx, x, y, cx, 22, cx, 28, rot);
            render_draw_line_rotated(ctx, x, y, cx - 6, 28, cx + 6, 28, rot);
            render_draw_line_rotated(ctx, x, y, cx - 6, 33, cx + 6, 33, rot);
        }
    } else if (comp->props.tline.model == 1) {
        render_draw_line_rotated(ctx, x, y, 22, 12, 28, 12, rot);   // small "L" hint
        render_draw_line_rotated(ctx, x, y, 22, 6, 22, 12, rot);
    }
}

// Toroid topload: a ring drawn as two ellipses above the terminal stub; corona streaks appear
// above ~50 kV (purely visual, scaled by the last accepted terminal voltage).
void render_toroid(RenderContext *ctx, Component *comp) {
    float x = comp->x, y = comp->y;
    double D = comp->props.toroid.major_in, d = comp->props.toroid.minor_in;
    float rx = (float)fminf(fmaxf((float)D * 2.4f, 16.0f), 70.0f);
    float ry = (float)fminf(fmaxf((float)d * 2.4f, 5.0f), 30.0f);
    float cy = y - 4;
    // stub from the terminal (0,40) up to the ring
    render_draw_line(ctx, x, y + 40, x, cy + ry);
    // outer and inner ellipse
    const int N = 40;
    for (int pass = 0; pass < 2; pass++) {
        float ex = pass ? rx * 0.42f : rx, ey = pass ? ry * 0.42f : ry;
        float lx = x + ex, ly = cy;
        for (int i = 1; i <= N; i++) {
            float a = (float)(2.0 * M_PI * i / N);
            float qx = x + ex * cosf(a), qy = cy + ey * sinf(a);
            render_draw_line(ctx, lx, ly, qx, qy);
            lx = qx; ly = qy;
        }
    }
    double v = fabs(comp->props.toroid.voltage);
    if (v > 50e3) {
        float inten = (float)fmin((v - 50e3) / 250e3, 1.0);
        int streaks = 2 + (int)(inten * 4);
        for (int k = 0; k < streaks; k++) {
            float a = (float)(-M_PI * (0.15 + 0.7 * ((rand() % 1000) / 1000.0)));   // upper half
            float sx = x + rx * cosf(a), sy = cy + ry * sinf(a);
            float L = 12 + 50 * inten * ((rand() % 1000) / 1000.0f);
            render_arc_between(ctx, sx, sy, sx + L * cosf(a), sy + L * sinf(a), 0.6f + 0.4f * inten);
        }
    }
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

// Real op-amp (with finite gain and rail voltages)
void render_opamp_real(RenderContext *ctx, float x, float y, int rotation) {
    // Same triangle as ideal op-amp
    render_draw_line_rotated(ctx, x, y, -25, -30, -25, 30, rotation);
    render_draw_line_rotated(ctx, x, y, -25, -30, 30, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -25, 30, 30, 0, rotation);
    // Inputs
    render_draw_line_rotated(ctx, x, y, -40, -20, -25, -20, rotation);
    render_draw_line_rotated(ctx, x, y, -40, 20, -25, 20, rotation);
    // Output
    render_draw_line_rotated(ctx, x, y, 30, 0, 40, 0, rotation);
    // - and + symbols
    render_draw_line_rotated(ctx, x, y, -20, -20, -12, -20, rotation);
    render_draw_line_rotated(ctx, x, y, -20, 20, -12, 20, rotation);
    render_draw_line_rotated(ctx, x, y, -16, 16, -16, 24, rotation);

    // Add "(REAL)" label
    int sx, sy;
    render_world_to_screen(ctx, x, y - 40, &sx, &sy);
    Color label_color = {0x80, 0x80, 0x80, 0xff};  // Gray
    render_draw_text(ctx, "(REAL)", sx - 18, sy, label_color);

    // Add V+ rail indicator at top
    int vplus_x, vplus_y;
    render_world_to_screen(ctx, x, y - 50, &vplus_x, &vplus_y);
    Color rail_color = {0xff, 0x80, 0x00, 0xff};  // Orange
    render_draw_text(ctx, "V+", vplus_x - 8, vplus_y, rail_color);

    // Add V- rail indicator at bottom
    int vminus_x, vminus_y;
    render_world_to_screen(ctx, x, y + 50, &vminus_x, &vminus_y);
    render_draw_text(ctx, "V-", vminus_x - 8, vminus_y, rail_color);
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
/* A length of cable: two parallel conductors with the delay written under it by the value
   label. Deliberately not a rectangle - a delay line is not a lumped part, and the symbol
   should not suggest one. */
void render_delay_line(RenderContext *ctx, Component *comp) {
    float x = comp->x, y = comp->y;
    int rot = comp->rotation;
    render_draw_line_rotated(ctx, x, y, -40, 0, -26, 0, rot);
    render_draw_line_rotated(ctx, x, y, 26, 0, 40, 0, rot);
    /* the two conductors */
    render_draw_line_rotated(ctx, x, y, -26, -7, 26, -7, rot);
    render_draw_line_rotated(ctx, x, y, -26, 7, 26, 7, rot);
    /* end caps */
    render_draw_line_rotated(ctx, x, y, -26, -7, -26, 7, rot);
    render_draw_line_rotated(ctx, x, y, 26, -7, 26, 7, rot);
    /* the lead enters the middle of each end */
    render_draw_line_rotated(ctx, x, y, -26, 0, -20, 0, rot);
    render_draw_line_rotated(ctx, x, y, 20, 0, 26, 0, rot);
    /* a chevron pointing the way a wave travels */
    render_draw_line_rotated(ctx, x, y, -6, -4, 2, 0, rot);
    render_draw_line_rotated(ctx, x, y, -6, 4, 2, 0, rot);
}

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

/* Logic-driven DPDT: two changeovers drawn one above the other, both blades moving together,
   with a dashed tie between them to say they are ganged and a control arrow underneath.
   Terminals: 1C(-50,-40) 1NC(50,-60) 1NO(50,-20)  2C(-50,40) 2NC(50,20) 2NO(50,60)  CTL(0,80) */
void render_dpdt_driven(RenderContext *ctx, float x, float y, int rotation, bool thrown) {
    const float pole[2] = { -40.0f, 40.0f };      /* the two commons */
    for (int p = 0; p < 2; p++) {
        float cy = pole[p];
        float nc = cy - 20.0f, no = cy + 20.0f;   /* its two throws */
        /* leads in and out */
        render_draw_line_rotated(ctx, x, y, -50, cy, -20, cy, rotation);
        render_draw_line_rotated(ctx, x, y, 20, nc, 50, nc, rotation);
        render_draw_line_rotated(ctx, x, y, 20, no, 50, no, rotation);
        /* contacts */
        render_draw_circle_rotated(ctx, x, y, -20, cy, 3, rotation);
        render_draw_circle_rotated(ctx, x, y, 20, nc, 3, rotation);
        render_draw_circle_rotated(ctx, x, y, 20, no, 3, rotation);
        /* the blade, on whichever throw the control has selected */
        render_draw_line_rotated(ctx, x, y, -20, cy, 20, thrown ? no : nc, rotation);
    }
    /* ganged: the two blades move together */
    for (float ty = -14.0f; ty < 14.0f; ty += 8.0f)
        render_draw_line_rotated(ctx, x, y, 0, ty, 0, ty + 4.0f, rotation);
    /* control input */
    render_draw_line_rotated(ctx, x, y, 0, 80, 0, 62, rotation);
    render_draw_line_rotated(ctx, x, y, -4, 68, 0, 62, rotation);
    render_draw_line_rotated(ctx, x, y, 4, 68, 0, 62, rotation);
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

// Microphone - circular capsule with sound waves coming in
void render_microphone(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (+40, 0) and (-40, 0) - output on right
    render_draw_line_rotated(ctx, x, y, 40, 0, 20, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -40, 0, -20, 0, rotation);
    // Capsule body (circle)
    render_draw_circle(ctx, x, y, 15);
    // Diaphragm lines inside
    render_draw_line_rotated(ctx, x, y, -10, -8, -10, 8, rotation);
    render_draw_line_rotated(ctx, x, y, -5, -10, -5, 10, rotation);
    // Sound waves coming in (curved arcs represented as lines pointing inward)
    render_draw_line_rotated(ctx, x, y, -30, -12, -22, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -30, 0, -22, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -30, 12, -22, 8, rotation);
    // Second set of waves further out
    render_draw_line_rotated(ctx, x, y, -38, -15, -32, -10, rotation);
    render_draw_line_rotated(ctx, x, y, -38, 0, -32, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -38, 15, -32, 10, rotation);
}

// Antenna TX - vertical antenna with radio waves going out
void render_antenna_tx(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -8, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 8, 0, 40, 0, rotation);
    // Vertical antenna mast
    render_draw_line_rotated(ctx, x, y, 0, 0, 0, -18, rotation);
    // Ground plane
    render_draw_line_rotated(ctx, x, y, -8, 0, 8, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -6, 3, 0, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 6, 3, 0, 0, rotation);
    // Radio waves going out (TX)
    render_draw_line_rotated(ctx, x, y, 8, -18, 12, -14, rotation);
    render_draw_line_rotated(ctx, x, y, 8, -10, 12, -10, rotation);
    render_draw_line_rotated(ctx, x, y, 8, -2, 12, -6, rotation);
    render_draw_line_rotated(ctx, x, y, 16, -18, 22, -12, rotation);
    render_draw_line_rotated(ctx, x, y, 16, -10, 22, -10, rotation);
    render_draw_line_rotated(ctx, x, y, 16, -2, 22, -8, rotation);
}

// Antenna RX - vertical antenna with radio waves coming in
void render_antenna_rx(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -8, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 8, 0, 40, 0, rotation);
    // Vertical antenna mast
    render_draw_line_rotated(ctx, x, y, 0, 0, 0, -18, rotation);
    // Ground plane
    render_draw_line_rotated(ctx, x, y, -8, 0, 8, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -6, 3, 0, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 6, 3, 0, 0, rotation);
    // Radio waves coming in (RX - arrows point inward)
    render_draw_line_rotated(ctx, x, y, 22, -18, 16, -14, rotation);
    render_draw_line_rotated(ctx, x, y, 22, -10, 16, -10, rotation);
    render_draw_line_rotated(ctx, x, y, 22, -2, 16, -6, rotation);
    render_draw_line_rotated(ctx, x, y, 12, -16, 8, -14, rotation);
    render_draw_line_rotated(ctx, x, y, 12, -10, 8, -10, rotation);
    render_draw_line_rotated(ctx, x, y, 12, -4, 8, -6, rotation);
}

// Bus - thick line with slash marks indicating multiple wires
void render_bus(RenderContext *ctx, float x, float y, int rotation, int width) {
    (void)width;  // Width is displayed as text, not used for drawing
    // Terminals at (-40, 0) and (40, 0)
    render_draw_line_rotated(ctx, x, y, -40, 0, -15, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 15, 0, 40, 0, rotation);
    // Thick bus line (multiple parallel lines)
    render_draw_line_rotated(ctx, x, y, -15, -4, 15, -4, rotation);
    render_draw_line_rotated(ctx, x, y, -15, 0, 15, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -15, 4, 15, 4, rotation);
    // Bus ends
    render_draw_line_rotated(ctx, x, y, -15, -4, -15, 4, rotation);
    render_draw_line_rotated(ctx, x, y, 15, -4, 15, 4, rotation);
    // Slash marks indicating bus (3 slashes)
    render_draw_line_rotated(ctx, x, y, -8, 8, -4, -8, rotation);
    render_draw_line_rotated(ctx, x, y, -2, 8, 2, -8, rotation);
    render_draw_line_rotated(ctx, x, y, 4, 8, 8, -8, rotation);
}

// Bus Tap - connection point from bus to single wire
void render_bus_tap(RenderContext *ctx, float x, float y, int rotation) {
    // Terminals at (-20, 0) BUS and (20, 0) SIG
    render_draw_line_rotated(ctx, x, y, -20, 0, -8, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 8, 0, 20, 0, rotation);
    // Small box with slash
    render_draw_line_rotated(ctx, x, y, -8, -6, 8, -6, rotation);
    render_draw_line_rotated(ctx, x, y, -8, 6, 8, 6, rotation);
    render_draw_line_rotated(ctx, x, y, -8, -6, -8, 6, rotation);
    render_draw_line_rotated(ctx, x, y, 8, -6, 8, 6, rotation);
    // Diagonal slash inside
    render_draw_line_rotated(ctx, x, y, -4, 4, 4, -4, rotation);
}

// LED Dot Matrix 8x8 - grid of LED dots
void render_led_matrix(RenderContext *ctx, float x, float y, int rotation, uint8_t *pixel_state, uint8_t color_idx) {
    // Matrix body - large rectangle
    render_draw_line_rotated(ctx, x, y, -50, -60, 50, -60, rotation);
    render_draw_line_rotated(ctx, x, y, -50, 60, 50, 60, rotation);
    render_draw_line_rotated(ctx, x, y, -50, -60, -50, 60, rotation);
    render_draw_line_rotated(ctx, x, y, 50, -60, 50, 60, rotation);

    // Row pins on left side (R0-R7 at terminals 0-7) - Row anodes
    for (int i = 0; i < 8; i++) {
        int pin_y = -52 + i * 15;
        render_draw_line_rotated(ctx, x, y, -60, pin_y, -50, pin_y, rotation);

        // Pin label (R0-R7)
        char label[4];
        snprintf(label, sizeof(label), "R%d", i);
        int sx, sy;
        render_world_to_screen(ctx, x - 48, y + pin_y, &sx, &sy);
        render_draw_text_small(ctx, label, sx + 2, sy - 4, COLOR_TEXT_DIM);
    }

    // Column pins on right side (C0-C7 at terminals 8-15) - Column cathodes
    for (int i = 0; i < 8; i++) {
        int pin_y = -52 + i * 15;
        render_draw_line_rotated(ctx, x, y, 50, pin_y, 60, pin_y, rotation);

        // Pin label (C0-C7)
        char label[4];
        snprintf(label, sizeof(label), "C%d", i);
        int sx, sy;
        render_world_to_screen(ctx, x + 48, y + pin_y, &sx, &sy);
        render_draw_text_small(ctx, label, sx - 14, sy - 4, COLOR_TEXT_DIM);
    }

    // Component label at top
    {
        int sx, sy;
        render_world_to_screen(ctx, x, y - 55, &sx, &sy);
        render_draw_text_small(ctx, "8x8 LED", sx - 20, sy - 10, COLOR_TEXT_DIM);
    }

    // Draw 8x8 LED grid inside
    SDL_Color led_colors[] = {
        {255, 60, 60, 255},    // 0 = Red
        {60, 255, 60, 255},    // 1 = Green
        {60, 100, 255, 255},   // 2 = Blue
        {255, 255, 60, 255},   // 3 = Yellow
        {255, 255, 255, 255}   // 4 = White
    };
    SDL_Color off_color = {40, 40, 40, 255};
    SDL_Color on_color = led_colors[color_idx % 5];

    int row, col;
    for (row = 0; row < 8; row++) {
        for (col = 0; col < 8; col++) {
            // LED position in grid (world coordinates relative to component center)
            float px = -35.0f + col * 10.0f;
            float py = -45.0f + row * 12.0f;

            // Check if this LED is on
            bool is_on = (pixel_state[row] >> (7 - col)) & 1;

            // Set LED color directly
            if (is_on) {
                SDL_SetRenderDrawColor(ctx->renderer, on_color.r, on_color.g, on_color.b, on_color.a);
            } else {
                SDL_SetRenderDrawColor(ctx->renderer, off_color.r, off_color.g, off_color.b, off_color.a);
            }

            // Transform to screen coordinates and draw filled rectangle for LED
            int sx, sy;
            render_world_to_screen(ctx, x + px, y + py, &sx, &sy);
            int led_size = (int)(6 * ctx->zoom);
            if (led_size < 2) led_size = 2;  // Minimum visible size
            SDL_Rect led_rect;
            led_rect.x = sx - led_size / 2;
            led_rect.y = sy - led_size / 2;
            led_rect.w = led_size;
            led_rect.h = led_size;
            SDL_RenderFillRect(ctx->renderer, &led_rect);
        }
    }

    // Note: render color will be reset by next render_set_color call
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
    // Polarity markers: + on left terminal, - on right terminal
    // "+" near left terminal (above wire)
    render_draw_line_rotated(ctx, x, y, -34, -10, -26, -10, rotation);  // horizontal
    render_draw_line_rotated(ctx, x, y, -30, -14, -30, -6, rotation);   // vertical
    // "-" near right terminal (above wire)
    render_draw_line_rotated(ctx, x, y, 26, -10, 34, -10, rotation);    // horizontal only
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
    // Polarity markers: + on left terminal, - on right terminal
    // "+" near left terminal (above wire)
    render_draw_line_rotated(ctx, x, y, -34, -10, -26, -10, rotation);  // horizontal
    render_draw_line_rotated(ctx, x, y, -30, -14, -30, -6, rotation);   // vertical
    // "-" near right terminal (above wire)
    render_draw_line_rotated(ctx, x, y, 26, -10, 34, -10, rotation);    // horizontal only
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
// Logic gates. Inputs A/B are at (-40, -20) / (-40, +20) and the output at (40, 0) - all on the
// 20 px grid, and the drawn stubs end exactly on those terminals.
static void gate_arc(RenderContext *ctx, float x, float y, int rotation, float cx, float cy, float r, int a0, int a1) {
    for (int a = a0; a < a1; a += 10) {
        float r1 = a * M_PI / 180, r2 = (a + 10) * M_PI / 180;
        render_draw_line_rotated(ctx, x, y, cx + r * cos(r1), cy + r * sin(r1), cx + r * cos(r2), cy + r * sin(r2), rotation);
    }
}
static void gate_inputs(RenderContext *ctx, float x, float y, int rotation, float body_left) {
    render_draw_line_rotated(ctx, x, y, -40, -20, body_left, -20, rotation);
    render_draw_line_rotated(ctx, x, y, -40, 20, body_left, 20, rotation);
}
static void gate_output(RenderContext *ctx, float x, float y, int rotation, float from) {
    render_draw_line_rotated(ctx, x, y, from, 0, 40, 0, rotation);
}
// AND body: flat back at x = -20, straight top/bottom to x = 0, semicircle of radius 20 to x = 20
static void gate_and_body(RenderContext *ctx, float x, float y, int rotation) {
    render_draw_line_rotated(ctx, x, y, -20, -20, -20, 20, rotation);
    render_draw_line_rotated(ctx, x, y, -20, -20, 0, -20, rotation);
    render_draw_line_rotated(ctx, x, y, -20, 20, 0, 20, rotation);
    gate_arc(ctx, x, y, rotation, 0, 0, 20, -90, 90);
}
// OR body: concave back through x = -20 at the centre, two arcs meeting at the point (20, 0)
static void gate_or_body(RenderContext *ctx, float x, float y, int rotation, float back_shift) {
    float bx = -30 + back_shift;
    gate_arc(ctx, x, y, rotation, bx - 30, 0, 36.06f, -34, 34);          // back: passes through (bx, +/-20) and (bx+6, 0)
    gate_arc(ctx, x, y, rotation, 0, -34, 40.4f, 32, 90);                 // top from (34,-... ) approximates (0,-20) .. (20,0)
    gate_arc(ctx, x, y, rotation, 0, 34, 40.4f, -90, -32);
    render_draw_line_rotated(ctx, x, y, bx, -20, 0, -20, rotation);
    render_draw_line_rotated(ctx, x, y, bx, 20, 0, 20, rotation);
}
static void render_gate_body(RenderContext *ctx, float x, float y, int rotation) { gate_and_body(ctx, x, y, rotation); }

// NOT gate (inverter)
void render_not_gate(RenderContext *ctx, float x, float y, int rotation) {
    render_draw_line_rotated(ctx, x, y, -20, -15, -20, 15, rotation);
    render_draw_line_rotated(ctx, x, y, -20, -15, 10, 0, rotation);
    render_draw_line_rotated(ctx, x, y, -20, 15, 10, 0, rotation);
    render_draw_circle_rotated(ctx, x, y, 15, 0, 5, rotation);
    render_draw_line_rotated(ctx, x, y, -40, 0, -20, 0, rotation);
    render_draw_line_rotated(ctx, x, y, 20, 0, 40, 0, rotation);
}

void render_and_gate(RenderContext *ctx, float x, float y, int rotation) {
    gate_and_body(ctx, x, y, rotation);
    gate_inputs(ctx, x, y, rotation, -20);
    gate_output(ctx, x, y, rotation, 20);
}

void render_or_gate(RenderContext *ctx, float x, float y, int rotation) {
    gate_or_body(ctx, x, y, rotation, 0);
    gate_inputs(ctx, x, y, rotation, -27);
    gate_output(ctx, x, y, rotation, 20);
}

void render_nand_gate(RenderContext *ctx, float x, float y, int rotation) {
    gate_and_body(ctx, x, y, rotation);
    render_draw_circle_rotated(ctx, x, y, 25, 0, 5, rotation);
    gate_inputs(ctx, x, y, rotation, -20);
    gate_output(ctx, x, y, rotation, 30);
}

void render_nor_gate(RenderContext *ctx, float x, float y, int rotation) {
    gate_or_body(ctx, x, y, rotation, 0);
    render_draw_circle_rotated(ctx, x, y, 25, 0, 5, rotation);
    gate_inputs(ctx, x, y, rotation, -27);
    gate_output(ctx, x, y, rotation, 30);
}

void render_xor_gate(RenderContext *ctx, float x, float y, int rotation) {
    gate_or_body(ctx, x, y, rotation, 4);
    gate_arc(ctx, x, y, rotation, -62, 0, 36.06f, -34, 34);               // extra back line
    gate_inputs(ctx, x, y, rotation, -32);
    gate_output(ctx, x, y, rotation, 20);
}

void render_xnor_gate(RenderContext *ctx, float x, float y, int rotation) {
    gate_or_body(ctx, x, y, rotation, 4);
    gate_arc(ctx, x, y, rotation, -62, 0, 36.06f, -34, 34);
    render_draw_circle_rotated(ctx, x, y, 25, 0, 5, rotation);
    gate_inputs(ctx, x, y, rotation, -32);
    gate_output(ctx, x, y, rotation, 30);
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

// Voltage regulator (TO-220 style package - 3-pin)
void render_regulator_box(RenderContext *ctx, float x, float y, int rotation) {
    // TO-220 style package body (rectangle)
    render_draw_line_rotated(ctx, x, y, -25, -20, 25, -20, rotation);
    render_draw_line_rotated(ctx, x, y, -25, 20, 25, 20, rotation);
    render_draw_line_rotated(ctx, x, y, -25, -20, -25, 20, rotation);
    render_draw_line_rotated(ctx, x, y, 25, -20, 25, 20, rotation);
    // Heat tab at top
    render_draw_line_rotated(ctx, x, y, -20, -20, -20, -25, rotation);
    render_draw_line_rotated(ctx, x, y, 20, -20, 20, -25, rotation);
    render_draw_line_rotated(ctx, x, y, -20, -25, 20, -25, rotation);
    // Mounting hole
    render_draw_circle_rotated(ctx, x, y, 0, -22, 3, rotation);
    // Three terminals: IN (left), OUT (right), GND/ADJ (bottom center)
    render_draw_line_rotated(ctx, x, y, -40, 0, -25, 0, rotation);   // IN
    render_draw_line_rotated(ctx, x, y, 25, 0, 40, 0, rotation);     // OUT
    render_draw_line_rotated(ctx, x, y, 0, 20, 0, 30, rotation);     // GND/ADJ/REF
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
void render_7seg_display(RenderContext *ctx, float x, float y, int rotation,
                         const double *currents, double max_current) {
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

    /* The digit. Each segment is drawn dim when it is off and as a thick bright bar when
       current is flowing through it, so the display reads as a display and not as a fixed
       "8" outline. Order matches the stamp: a,b,c,d,e,f,g,dp. */
    static const float seg_pts[7][4] = {
        { -15, -35,  15, -35 },   // a  top
        {  15, -35,  15,  -5 },   // b  top right
        {  15,  -5,  15,  25 },   // c  bottom right
        { -15,  25,  15,  25 },   // d  bottom
        { -15,  -5, -15,  25 },   // e  bottom left
        { -15, -35, -15,  -5 },   // f  top left
        { -15,  -5,  15,  -5 },   // g  middle
    };
    const Color off_color = {0x30, 0x50, 0x58, 0xff};   // unlit segment: barely there
    const Color on_color  = {0xff, 0x30, 0x30, 0xff};   // lit: red, like a real LED digit

    for (int s = 0; s < 7; s++) {
        double i_seg = currents ? currents[s] : 0.0;
        bool lit = i_seg > 0.0001;                       // 0.1 mA, same threshold as the LED bar
        float x1 = seg_pts[s][0], y1 = seg_pts[s][1];
        float x2 = seg_pts[s][2], y2 = seg_pts[s][3];

        if (!lit) {
            SDL_SetRenderDrawColor(ctx->renderer, off_color.r, off_color.g, off_color.b, 0xff);
            render_draw_line_rotated(ctx, x, y, x1, y1, x2, y2, rotation);
            continue;
        }

        float brightness = (float)(i_seg / (max_current > 0 ? max_current : 0.02));
        if (brightness > 1.0f) brightness = 1.0f;
        brightness = powf(brightness, 0.6f);
        brightness = fminf(brightness * 1.3f, 1.0f);
        SDL_SetRenderDrawColor(ctx->renderer,
                               (Uint8)(on_color.r * brightness),
                               (Uint8)(on_color.g * brightness),
                               (Uint8)(on_color.b * brightness), 0xff);
        /* Thicken across the segment's short axis so a lit bar is unmistakable. */
        bool horiz = (y1 == y2);
        for (int t = -2; t <= 2; t++) {
            if (horiz) render_draw_line_rotated(ctx, x, y, x1, y1 + t, x2, y2 + t, rotation);
            else       render_draw_line_rotated(ctx, x, y, x1 + t, y1, x2 + t, y2, rotation);
        }
    }

    // Decimal point (segment DP)
    double i_dp = currents ? currents[7] : 0.0;
    if (i_dp > 0.0001) {
        SDL_SetRenderDrawColor(ctx->renderer, on_color.r, on_color.g, on_color.b, 0xff);
        render_fill_circle(ctx, x + 20, y + 25, 4);
    } else {
        SDL_SetRenderDrawColor(ctx->renderer, off_color.r, off_color.g, off_color.b, 0xff);
        render_draw_circle(ctx, x + 20, y + 25, 3);
    }
    SDL_SetRenderDrawColor(ctx->renderer, 0x00, 0xff, 0xff, 0xff);

    /* Label below the package. It used to sit dead centre, on top of the digit. */
    int sx, sy;
    render_world_to_screen(ctx, x, y + 56, &sx, &sy);
    Color label_color = {0x00, 0xff, 0xff, 0xff};
    render_draw_text_small(ctx, "7SEG", sx - 14, sy, label_color);
}

// LED array (bar graph) - 8 individual LEDs with common cathode
// Component size: 160x70, 8 anode terminals at top (20-unit spacing), 1 common cathode at bottom
// Each LED lights independently based on its individual current
void render_led_array(RenderContext *ctx, float x, float y, int rotation,
                      double *currents, bool *failed, double max_current, int color_idx) {
    // Lead lines to 8 anode terminals at top (y = -30)
    // Terminals at: -70, -50, -30, -10, 10, 30, 50, 70 (20-unit spacing, grid-aligned)
    for (int i = 0; i < 8; i++) {
        float tx = -70 + i * 20;
        render_draw_line_rotated(ctx, x, y, tx, -30, tx, -22, rotation);
    }
    // Common cathode terminal at bottom (y = 30)
    render_draw_line_rotated(ctx, x, y, 0, 22, 0, 30, rotation);

    // Outer rectangle
    render_draw_line_rotated(ctx, x, y, -78, -22, 78, -22, rotation);  // Top
    render_draw_line_rotated(ctx, x, y, 78, -22, 78, 22, rotation);    // Right
    render_draw_line_rotated(ctx, x, y, 78, 22, -78, 22, rotation);    // Bottom
    render_draw_line_rotated(ctx, x, y, -78, 22, -78, -22, rotation);  // Left

    // Map LEDColor enum to RGB colors (matches standard LED specifications)
    Color led_colors[] = {
        {0x8B, 0x00, 0x00, 0xff},  // LED_COLOR_INFRARED: dark red (not visible, shown as dim red)
        {0xff, 0x00, 0x00, 0xff},  // LED_COLOR_RED: bright red (630nm)
        {0xff, 0xa5, 0x00, 0xff},  // LED_COLOR_ORANGE: orange (610nm)
        {0xff, 0xff, 0x00, 0xff},  // LED_COLOR_YELLOW: yellow (590nm)
        {0x00, 0xff, 0x00, 0xff},  // LED_COLOR_GREEN_STANDARD: yellow-green (565nm)
        {0x00, 0xff, 0x80, 0xff},  // LED_COLOR_GREEN_PURE: emerald green (525nm)
        {0x00, 0x00, 0xff, 0xff},  // LED_COLOR_BLUE: blue (470nm)
        {0xff, 0xff, 0xff, 0xff},  // LED_COLOR_WHITE: white (blue + phosphor)
        {0x94, 0x00, 0xd3, 0xff},  // LED_COLOR_UV: violet (395nm, near UV)
    };
    int num_colors = sizeof(led_colors) / sizeof(led_colors[0]);
    Color lit_color = led_colors[(color_idx >= 0 && color_idx < num_colors) ? color_idx : 1];  // Default to RED

    // LED segments (8 bars) - each with independent state
    for (int i = 0; i < 8; i++) {
        float bx = -70 + i * 20;

        // Draw outline (larger bars for bigger component)
        render_draw_line_rotated(ctx, x, y, bx - 7, -18, bx + 7, -18, rotation);
        render_draw_line_rotated(ctx, x, y, bx + 7, -18, bx + 7, 18, rotation);
        render_draw_line_rotated(ctx, x, y, bx + 7, 18, bx - 7, 18, rotation);
        render_draw_line_rotated(ctx, x, y, bx - 7, 18, bx - 7, -18, rotation);

        // Check if this LED is burned (failed)
        if (failed && failed[i]) {
            // Draw burned LED - dark with X pattern
            SDL_SetRenderDrawColor(ctx->renderer, 0x40, 0x20, 0x00, 0xff);
            for (int dy = -17; dy <= 17; dy++) {
                render_draw_line_rotated(ctx, x, y, bx - 6, (float)dy, bx + 6, (float)dy, rotation);
            }
            // Draw X to indicate burned
            SDL_SetRenderDrawColor(ctx->renderer, 0x00, 0x00, 0x00, 0xff);
            render_draw_line_rotated(ctx, x, y, bx - 5, -15, bx + 5, 15, rotation);
            render_draw_line_rotated(ctx, x, y, bx + 5, -15, bx - 5, 15, rotation);
            SDL_SetRenderDrawColor(ctx->renderer, 0x00, 0xff, 0xff, 0xff);
            continue;
        }

        // Get current for this LED
        double led_current = (currents) ? currents[i] : 0.0;

        // Determine if lit (current > threshold)
        bool is_lit = (led_current > 0.0001);  // 0.1mA threshold

        if (is_lit) {
            // Calculate brightness based on current
            float brightness = (float)(led_current / max_current);
            if (brightness > 1.0f) brightness = 1.0f;

            // Apply gamma correction for perceptual brightness (increased brightness)
            brightness = powf(brightness, 0.6f);  // Gamma 0.6 correction (brighter than 0.5)
            brightness = fminf(brightness * 1.3f, 1.0f);  // 30% brightness boost

            Uint8 r = (Uint8)(lit_color.r * brightness);
            Uint8 g = (Uint8)(lit_color.g * brightness);
            Uint8 b = (Uint8)(lit_color.b * brightness);

            SDL_SetRenderDrawColor(ctx->renderer, r, g, b, 0xff);
            for (int dy = -17; dy <= 17; dy++) {
                render_draw_line_rotated(ctx, x, y, bx - 6, (float)dy, bx + 6, (float)dy, rotation);
            }
            SDL_SetRenderDrawColor(ctx->renderer, 0x00, 0xff, 0xff, 0xff);
        }
    }

    // Label
    int sx, sy;
    render_world_to_screen(ctx, x, y + 35, &sx, &sy);
    Color label_color = {0x00, 0xff, 0xff, 0xff};
    render_draw_text(ctx, "LED BAR", sx - 22, sy, label_color);
}

// BCD to 7-segment decoder (7447/74LS47 style)
// Component size: 80x140, terminals: A,B,C,D inputs on left, a-g outputs on right
/* Counter: same DIP outline as the decoder it usually feeds. CLK and RST on the left, the four
   count bits and the carry on the right, and the value it is holding printed in the middle so
   you can read the count off the schematic without probing it. */
void render_counter(RenderContext *ctx, float x, float y, int rotation, int count, int modulus) {
    render_draw_line_rotated(ctx, x, y, -30, -65, 30, -65, rotation);
    render_draw_line_rotated(ctx, x, y, 30, -65, 30, 65, rotation);
    render_draw_line_rotated(ctx, x, y, 30, 65, -30, 65, rotation);
    render_draw_line_rotated(ctx, x, y, -30, 65, -30, -65, rotation);
    render_draw_circle(ctx, x, y - 65, 5);

    render_draw_line_rotated(ctx, x, y, -30, -40, -40, -40, rotation);   // CLK
    render_draw_line_rotated(ctx, x, y, -30, 40, -40, 40, rotation);     // RST
    render_draw_line_rotated(ctx, x, y, 30, -60, 40, -60, rotation);     // Q0
    render_draw_line_rotated(ctx, x, y, 30, -20, 40, -20, rotation);     // Q1
    render_draw_line_rotated(ctx, x, y, 30, 20, 40, 20, rotation);       // Q2
    render_draw_line_rotated(ctx, x, y, 30, 60, 40, 60, rotation);       // Q3
    render_draw_line_rotated(ctx, x, y, 0, 65, 0, 70, rotation);         // CY, on the bottom edge

    /* Pin names hug their own edge and the middle is left clear for the count, so the two never
       fight for the same pixels. The modulus goes under the package, the way the part number
       does on the other ICs, instead of being crammed inside next to the value. */
    int sx, sy;
    Color label_color = {0x00, 0xff, 0xff, 0xff};
    render_world_to_screen(ctx, x - 28, y - 44, &sx, &sy);
    render_draw_text_small(ctx, "CLK", sx, sy, label_color);
    render_world_to_screen(ctx, x - 28, y + 36, &sx, &sy);
    render_draw_text_small(ctx, "RST", sx, sy, label_color);
    render_world_to_screen(ctx, x + 16, y - 62, &sx, &sy);
    render_draw_text_small(ctx, "Q0", sx, sy, label_color);
    render_world_to_screen(ctx, x + 16, y - 24, &sx, &sy);
    render_draw_text_small(ctx, "Q1", sx, sy, label_color);
    render_world_to_screen(ctx, x + 16, y + 16, &sx, &sy);
    render_draw_text_small(ctx, "Q2", sx, sy, label_color);
    render_world_to_screen(ctx, x + 16, y + 56, &sx, &sy);
    render_draw_text_small(ctx, "Q3", sx, sy, label_color);
    render_world_to_screen(ctx, x + 6, y + 66, &sx, &sy);
    render_draw_text_small(ctx, "CY", sx, sy, label_color);

    /* the value, in a little window in the middle of the package */
    render_draw_line_rotated(ctx, x, y, -12, -12, 12, -12, rotation);
    render_draw_line_rotated(ctx, x, y, 12, -12, 12, 12, rotation);
    render_draw_line_rotated(ctx, x, y, 12, 12, -12, 12, rotation);
    render_draw_line_rotated(ctx, x, y, -12, 12, -12, -12, rotation);

    char buf[24];
    snprintf(buf, sizeof buf, "%d", count);
    render_world_to_screen(ctx, x - 4, y - 8, &sx, &sy);
    render_draw_text(ctx, buf, sx, sy, label_color);

    snprintf(buf, sizeof buf, "CNT mod%d", modulus > 0 ? modulus : 10);
    render_world_to_screen(ctx, x - 30, y + 92, &sx, &sy);
    render_draw_text_small(ctx, buf, sx, sy, label_color);
}

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

    /* Part number under the package, not across the middle of it: the pin names are only 18 px
       away on both sides and the three ran into each other. */
    render_world_to_screen(ctx, x - 26, y + 78, &sx, &sy);
    render_draw_text_small(ctx, "7447 BCD", sx, sy, label_color);
}

// User-defined sub-circuit / IC block
// Renders a DIP-style IC with dynamic pins based on the definition
void render_subcircuit(RenderContext *ctx, float x, float y, int rotation, int def_id, const char *name) {
    (void)rotation;  // TODO: Support rotation

    // Look up the subcircuit definition
    SubCircuitDef *def = NULL;
    for (int i = 0; i < g_subcircuit_library.count; i++) {
        if (g_subcircuit_library.defs[i].id == def_id) {
            def = &g_subcircuit_library.defs[i];
            break;
        }
    }

    // Default dimensions if no definition found
    float width = def ? def->block_width : 80.0f;
    float height = def ? def->block_height : 60.0f;
    float half_w = width / 2.0f;
    float half_h = height / 2.0f;

    Color body_color = {60, 60, 80, 255};      // Dark blue-gray IC body
    Color outline_color = {120, 120, 140, 255}; // Lighter outline
    Color pin_color = {180, 180, 180, 255};     // Silver pins
    Color label_color = {200, 200, 220, 255};   // Light label

    // Draw IC body (filled rectangle)
    render_set_color(ctx, body_color);
    render_fill_rect(ctx, x - half_w + 5, y - half_h + 5, width - 10, height - 10);

    // Draw outline
    render_set_color(ctx, outline_color);
    render_draw_line(ctx, x - half_w + 5, y - half_h + 5, x + half_w - 5, y - half_h + 5);  // Top
    render_draw_line(ctx, x + half_w - 5, y - half_h + 5, x + half_w - 5, y + half_h - 5);  // Right
    render_draw_line(ctx, x + half_w - 5, y + half_h - 5, x - half_w + 5, y + half_h - 5);  // Bottom
    render_draw_line(ctx, x - half_w + 5, y + half_h - 5, x - half_w + 5, y - half_h + 5);  // Left

    // Draw notch at top (IC orientation marker - semi-circular indent)
    // Draw a small half-circle indent at the top edge
    float notch_y = y - half_h + 5;
    float notch_r = 6.0f;
    // Draw arc segments to make a semi-circle indent pointing down
    for (int i = 0; i < 8; i++) {
        float a1 = 3.14159f * (float)i / 8.0f;  // 0 to PI
        float a2 = 3.14159f * (float)(i + 1) / 8.0f;
        float x1 = x - notch_r * cosf(a1);
        float y1 = notch_y + notch_r * sinf(a1);
        float x2 = x - notch_r * cosf(a2);
        float y2 = notch_y + notch_r * sinf(a2);
        render_draw_line(ctx, x1, y1, x2, y2);
    }

    // Draw pins if definition exists
    if (def) {
        render_set_color(ctx, pin_color);
        int sx, sy;

        for (int i = 0; i < def->num_pins; i++) {
            SubCircuitPin *pin = &def->pins[i];
            float px, py;

            switch (pin->side) {
                case 0:  // Left
                    px = x - half_w;
                    py = y - half_h + 20 + pin->position * 20;
                    render_draw_line(ctx, px - 10, py, px + 5, py);
                    // Draw label inside the body (right of pin)
                    render_world_to_screen(ctx, px + 10, py, &sx, &sy);
                    render_draw_text_small(ctx, pin->name, sx, sy - 4, label_color);
                    break;
                case 1:  // Right
                    px = x + half_w;
                    py = y - half_h + 20 + pin->position * 20;
                    render_draw_line(ctx, px - 5, py, px + 10, py);
                    // Draw label inside the body (left of pin)
                    render_world_to_screen(ctx, px - 10, py, &sx, &sy);
                    render_draw_text_small(ctx, pin->name, sx - 16, sy - 4, label_color);
                    break;
                case 2:  // Top
                    px = x - half_w + 20 + pin->position * 20;
                    py = y - half_h;
                    render_draw_line(ctx, px, py - 10, px, py + 5);
                    // Draw label inside the body (below pin)
                    render_world_to_screen(ctx, px, py + 12, &sx, &sy);
                    render_draw_text_small(ctx, pin->name, sx - 5, sy - 4, label_color);
                    break;
                case 3:  // Bottom
                    px = x - half_w + 20 + pin->position * 20;
                    py = y + half_h;
                    render_draw_line(ctx, px, py - 5, px, py + 10);
                    // Draw label inside the body (above pin)
                    render_world_to_screen(ctx, px, py - 12, &sx, &sy);
                    render_draw_text_small(ctx, pin->name, sx - 5, sy - 4, label_color);
                    break;
            }
        }
    } else {
        // Draw default 4-pin configuration when no definition
        render_set_color(ctx, pin_color);
        int sx, sy;

        // Left pins with labels (inside body)
        render_draw_line(ctx, x - half_w - 10, y - 20, x - half_w + 5, y - 20);
        render_world_to_screen(ctx, x - half_w + 10, y - 20, &sx, &sy);
        render_draw_text_small(ctx, "1", sx, sy - 4, label_color);

        render_draw_line(ctx, x - half_w - 10, y + 20, x - half_w + 5, y + 20);
        render_world_to_screen(ctx, x - half_w + 10, y + 20, &sx, &sy);
        render_draw_text_small(ctx, "2", sx, sy - 4, label_color);

        // Right pins with labels (inside body)
        render_draw_line(ctx, x + half_w - 5, y - 20, x + half_w + 10, y - 20);
        render_world_to_screen(ctx, x + half_w - 10, y - 20, &sx, &sy);
        render_draw_text_small(ctx, "3", sx - 8, sy - 4, label_color);

        render_draw_line(ctx, x + half_w - 5, y + 20, x + half_w + 10, y + 20);
        render_world_to_screen(ctx, x + half_w - 10, y + 20, &sx, &sy);
        render_draw_text_small(ctx, "4", sx - 8, sy - 4, label_color);
    }

    // Draw subcircuit name in center (use definition name, or instance name if no def)
    int sx, sy;
    render_world_to_screen(ctx, x, y, &sx, &sy);
    if (def && def->name[0]) {
        render_draw_text_small(ctx, def->name, sx - 15, sy - 4, label_color);
    } else if (name && name[0]) {
        render_draw_text_small(ctx, name, sx - 8, sy - 4, label_color);
    } else {
        render_draw_text_small(ctx, "IC", sx - 5, sy - 4, (Color){150, 150, 170, 255});
    }
}

// Convert temperature to heatmap color (blue -> cyan -> green -> yellow -> red)
Color temperature_to_color(double temp, double min_temp, double max_temp) {
    // Normalize temperature to 0-1 range
    double t = (temp - min_temp) / (max_temp - min_temp);
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;

    Color c = {0, 0, 0, 180};  // Semi-transparent

    // Color gradient: blue (0.0) -> cyan (0.25) -> green (0.5) -> yellow (0.75) -> red (1.0)
    if (t < 0.25) {
        // Blue to Cyan
        double f = t / 0.25;
        c.r = 0;
        c.g = (Uint8)(255 * f);
        c.b = 255;
    } else if (t < 0.5) {
        // Cyan to Green
        double f = (t - 0.25) / 0.25;
        c.r = 0;
        c.g = 255;
        c.b = (Uint8)(255 * (1.0 - f));
    } else if (t < 0.75) {
        // Green to Yellow
        double f = (t - 0.5) / 0.25;
        c.r = (Uint8)(255 * f);
        c.g = 255;
        c.b = 0;
    } else {
        // Yellow to Red
        double f = (t - 0.75) / 0.25;
        c.r = 255;
        c.g = (Uint8)(255 * (1.0 - f));
        c.b = 0;
    }

    return c;
}

// Render thermal heatmap overlay for a component
void render_heatmap_overlay(RenderContext *ctx, Component *comp) {
    if (!comp || comp->type == COMP_GROUND || comp->type == COMP_TEXT) {
        return;
    }

    // Get component temperature
    double temp = comp->thermal.temperature;
    double ambient = comp->thermal.ambient_temperature;
    double max_temp = comp->thermal.max_temperature;

    // Only show overlay if temperature is above ambient
    if (temp <= ambient + 1.0) {
        return;
    }

    // Get color based on temperature (ambient to max_temp range)
    Color heat_color = temperature_to_color(temp, ambient, max_temp);

    // Determine component size for overlay
    float size = 30.0f;  // Default size

    // Adjust size based on component type
    switch (comp->type) {
        case COMP_RESISTOR:
        case COMP_CAPACITOR:
        case COMP_INDUCTOR:
        case COMP_DIODE:
        case COMP_LED:
            size = 25.0f;
            break;
        case COMP_NPN_BJT:
        case COMP_PNP_BJT:
        case COMP_NMOS:
        case COMP_PMOS:
            size = 35.0f;
            break;
        case COMP_OPAMP:
            size = 45.0f;
            break;
        default:
            size = 30.0f;
            break;
    }

    // Draw semi-transparent circle overlay with blend
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);
    render_set_color(ctx, heat_color);

    // Draw filled circle (radial gradient effect approximated by multiple circles)
    for (float r = size; r > 0; r -= 2.0f) {
        // Increase alpha as we get closer to center
        Uint8 alpha = (Uint8)(heat_color.a * (1.0 - (r / size) * 0.5));
        Color inner_color = heat_color;
        inner_color.a = alpha;
        render_set_color(ctx, inner_color);
        render_fill_circle(ctx, comp->x, comp->y, r);
    }

    // Draw temperature label
    int sx, sy;
    render_world_to_screen(ctx, comp->x, comp->y + size + 10, &sx, &sy);
    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%.0f°C", temp);
    Color text_color = {255, 255, 255, 255};
    render_draw_text_small(ctx, temp_str, sx - 15, sy, text_color);
}

// Render node voltage tooltip near cursor
void render_node_voltage_tooltip(RenderContext *ctx, int screen_x, int screen_y, double voltage) {
    if (!ctx) return;

    // Format voltage string
    char voltage_str[32];
    if (fabs(voltage) >= 1000.0) {
        snprintf(voltage_str, sizeof(voltage_str), "%.2f kV", voltage / 1000.0);
    } else if (fabs(voltage) >= 1.0) {
        snprintf(voltage_str, sizeof(voltage_str), "%.3f V", voltage);
    } else if (fabs(voltage) >= 0.001) {
        snprintf(voltage_str, sizeof(voltage_str), "%.2f mV", voltage * 1000.0);
    } else if (fabs(voltage) >= 0.000001) {
        snprintf(voltage_str, sizeof(voltage_str), "%.2f µV", voltage * 1000000.0);
    } else {
        snprintf(voltage_str, sizeof(voltage_str), "%.3f V", voltage);
    }

    // Calculate tooltip dimensions
    int text_width = strlen(voltage_str) * 7;  // Approximate character width
    int text_height = 14;
    int padding = 6;
    int tooltip_w = text_width + padding * 2;
    int tooltip_h = text_height + padding * 2;

    // Position tooltip above and to the right of cursor
    int tooltip_x = screen_x + 15;
    int tooltip_y = screen_y - tooltip_h - 5;

    // Keep tooltip on screen
    if (tooltip_x + tooltip_w > 1920) tooltip_x = screen_x - tooltip_w - 5;
    if (tooltip_y < 0) tooltip_y = screen_y + 20;

    // Draw tooltip background with semi-transparent dark fill
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);

    // Dark background
    SDL_SetRenderDrawColor(ctx->renderer, 30, 30, 40, 230);
    SDL_Rect bg_rect = {tooltip_x, tooltip_y, tooltip_w, tooltip_h};
    SDL_RenderFillRect(ctx->renderer, &bg_rect);

    // Cyan border (matches circuit theme)
    SDL_SetRenderDrawColor(ctx->renderer, 0, 200, 255, 255);
    SDL_RenderDrawRect(ctx->renderer, &bg_rect);

    // Draw voltage text
    Color text_color = {0, 255, 200, 255};  // Cyan-green for visibility
    render_draw_text_small(ctx, voltage_str, tooltip_x + padding, tooltip_y + padding, text_color);
}

// Render component tooltip showing voltage drop and current
void render_component_tooltip(RenderContext *ctx, int screen_x, int screen_y, double voltage, double current) {
    if (!ctx) return;

    // Format voltage string
    char voltage_str[32];
    if (fabs(voltage) >= 1000.0) {
        snprintf(voltage_str, sizeof(voltage_str), "V: %.2f kV", voltage / 1000.0);
    } else if (fabs(voltage) >= 1.0) {
        snprintf(voltage_str, sizeof(voltage_str), "V: %.3f V", voltage);
    } else if (fabs(voltage) >= 0.001) {
        snprintf(voltage_str, sizeof(voltage_str), "V: %.2f mV", voltage * 1000.0);
    } else if (fabs(voltage) >= 0.000001) {
        snprintf(voltage_str, sizeof(voltage_str), "V: %.2f uV", voltage * 1000000.0);
    } else {
        snprintf(voltage_str, sizeof(voltage_str), "V: %.3f V", voltage);
    }

    // Format current string
    char current_str[32];
    double abs_current = fabs(current);
    if (abs_current >= 1.0) {
        snprintf(current_str, sizeof(current_str), "I: %.3f A", current);
    } else if (abs_current >= 0.001) {
        snprintf(current_str, sizeof(current_str), "I: %.2f mA", current * 1000.0);
    } else if (abs_current >= 0.000001) {
        snprintf(current_str, sizeof(current_str), "I: %.2f uA", current * 1000000.0);
    } else if (abs_current >= 0.000000001) {
        snprintf(current_str, sizeof(current_str), "I: %.2f nA", current * 1000000000.0);
    } else {
        snprintf(current_str, sizeof(current_str), "I: %.3f A", current);
    }

    // Calculate tooltip dimensions (two lines)
    int text_width1 = strlen(voltage_str) * 7;
    int text_width2 = strlen(current_str) * 7;
    int text_width = (text_width1 > text_width2) ? text_width1 : text_width2;
    int line_height = 14;
    int padding = 6;
    int tooltip_w = text_width + padding * 2;
    int tooltip_h = line_height * 2 + padding * 2 + 2;  // 2 lines + spacing

    // Position tooltip above and to the right of cursor
    int tooltip_x = screen_x + 15;
    int tooltip_y = screen_y - tooltip_h - 5;

    // Keep tooltip on screen
    if (tooltip_x + tooltip_w > 1920) tooltip_x = screen_x - tooltip_w - 5;
    if (tooltip_y < 0) tooltip_y = screen_y + 20;

    // Draw tooltip background with semi-transparent dark fill
    SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);

    // Dark background
    SDL_SetRenderDrawColor(ctx->renderer, 30, 30, 40, 230);
    SDL_Rect bg_rect = {tooltip_x, tooltip_y, tooltip_w, tooltip_h};
    SDL_RenderFillRect(ctx->renderer, &bg_rect);

    // Cyan border (matches circuit theme)
    SDL_SetRenderDrawColor(ctx->renderer, 0, 200, 255, 255);
    SDL_RenderDrawRect(ctx->renderer, &bg_rect);

    // Draw voltage text (first line)
    Color voltage_color = {0, 255, 200, 255};  // Cyan-green for voltage
    render_draw_text_small(ctx, voltage_str, tooltip_x + padding, tooltip_y + padding, voltage_color);

    // Draw current text (second line)
    Color current_color = {255, 200, 0, 255};  // Yellow-orange for current
    render_draw_text_small(ctx, current_str, tooltip_x + padding, tooltip_y + padding + line_height + 2, current_color);
}
