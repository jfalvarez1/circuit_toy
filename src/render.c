/**
 * Circuit Playground - Rendering System Implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "render.h"

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
            render_bjt(ctx, comp->x, comp->y, comp->rotation, false);
            break;
        case COMP_PNP_BJT:
            render_bjt(ctx, comp->x, comp->y, comp->rotation, true);
            break;
        case COMP_NMOS:
            render_mosfet(ctx, comp->x, comp->y, comp->rotation, false);
            break;
        case COMP_PMOS:
            render_mosfet(ctx, comp->x, comp->y, comp->rotation, true);
            break;
        case COMP_OPAMP:
            render_opamp(ctx, comp->x, comp->y, comp->rotation);
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

    // Draw current flow arrow (conventional current: high to low voltage)
    if (ctx->show_current) {
        double v_diff = start->voltage - end->voltage;
        double abs_diff = v_diff < 0 ? -v_diff : v_diff;

        // Only show arrow if voltage difference is significant
        if (abs_diff > 0.001) {
            // Arrow direction: from higher voltage to lower voltage
            float from_x, from_y, to_x, to_y;
            if (v_diff > 0) {
                from_x = start->x; from_y = start->y;
                to_x = end->x; to_y = end->y;
            } else {
                from_x = end->x; from_y = end->y;
                to_x = start->x; to_y = start->y;
            }

            // Calculate midpoint and direction
            float mid_x = (from_x + to_x) / 2;
            float mid_y = (from_y + to_y) / 2;
            float dx = to_x - from_x;
            float dy = to_y - from_y;
            float len = sqrt(dx*dx + dy*dy);

            if (len > 20) {  // Only draw on wires long enough
                // Normalize direction
                dx /= len;
                dy /= len;

                // Arrow size scales with zoom
                float arrow_size = 8 / ctx->zoom;
                if (arrow_size < 3) arrow_size = 3;
                if (arrow_size > 10) arrow_size = 10;

                // Calculate arrow points
                // Arrow tip slightly ahead of midpoint
                float tip_x = mid_x + dx * (arrow_size / 2);
                float tip_y = mid_y + dy * (arrow_size / 2);

                // Perpendicular direction for arrow wings
                float px = -dy;
                float py = dx;

                // Arrow wing points
                float wing1_x = tip_x - dx * arrow_size + px * (arrow_size * 0.5f);
                float wing1_y = tip_y - dy * arrow_size + py * (arrow_size * 0.5f);
                float wing2_x = tip_x - dx * arrow_size - px * (arrow_size * 0.5f);
                float wing2_y = tip_y - dy * arrow_size - py * (arrow_size * 0.5f);

                // Draw arrow in orange/yellow color
                render_set_color(ctx, (Color){0xff, 0xaa, 0x00, 0xff});
                render_draw_line(ctx, tip_x, tip_y, wing1_x, wing1_y);
                render_draw_line(ctx, tip_x, tip_y, wing2_x, wing2_y);
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

void render_bjt(RenderContext *ctx, float x, float y, int rotation, bool is_pnp) {
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
}

void render_mosfet(RenderContext *ctx, float x, float y, int rotation, bool is_pmos) {
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
