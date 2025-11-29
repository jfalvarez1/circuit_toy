/**
 * Circuit Playground - Rendering System Implementation
 */

#include <stdlib.h>
#include <string.h>
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
    ctx->offset_x = CANVAS_WIDTH / 2;
    ctx->offset_y = CANVAS_HEIGHT / 2;
    ctx->zoom = 1.0f;
    ctx->show_grid = true;
    ctx->snap_to_grid = true;

    ctx->canvas_rect = (Rect){CANVAS_X, CANVAS_Y, CANVAS_WIDTH, CANVAS_HEIGHT};

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
    ctx->offset_x = ctx->canvas_rect.w / 2;
    ctx->offset_y = ctx->canvas_rect.h / 2;
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

    int start_x = ((int)(left / GRID_SIZE) - 1) * GRID_SIZE;
    int start_y = ((int)(top / GRID_SIZE) - 1) * GRID_SIZE;
    int end_x = ((int)(right / GRID_SIZE) + 1) * GRID_SIZE;
    int end_y = ((int)(bottom / GRID_SIZE) + 1) * GRID_SIZE;

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
            render_resistor(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_CAPACITOR:
            render_capacitor(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_INDUCTOR:
            render_inductor(ctx, comp->x, comp->y, comp->rotation);
            break;
        case COMP_DIODE:
            render_diode(ctx, comp->x, comp->y, comp->rotation);
            break;
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

    render_set_color(ctx, probe->color);
    render_fill_circle(ctx, probe->x, probe->y, 8);

    render_set_color(ctx, COLOR_TEXT);
    render_draw_circle(ctx, probe->x, probe->y, 8);
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
void render_ground(RenderContext *ctx, float x, float y, int rotation) {
    render_draw_line(ctx, x, y - 15, x, y);
    render_draw_line(ctx, x - 12, y, x + 12, y);
    render_draw_line(ctx, x - 8, y + 5, x + 8, y + 5);
    render_draw_line(ctx, x - 4, y + 10, x + 4, y + 10);
}

void render_voltage_source(RenderContext *ctx, float x, float y, int rotation, bool is_ac) {
    render_draw_circle(ctx, x, y, 15);
    render_draw_line(ctx, x, y - 15, x, y - 25);
    render_draw_line(ctx, x, y + 15, x, y + 25);

    if (is_ac) {
        // Sine wave symbol
        for (int i = 0; i < 16; i++) {
            float x1 = x - 8 + i;
            float x2 = x - 8 + i + 1;
            float y1 = y + 6 * sin((i / 16.0) * 2 * M_PI);
            float y2 = y + 6 * sin(((i + 1) / 16.0) * 2 * M_PI);
            render_draw_line(ctx, x1, y1, x2, y2);
        }
    } else {
        // + and - symbols
        render_draw_line(ctx, x - 4, y - 6, x + 4, y - 6);
        render_draw_line(ctx, x, y - 10, x, y - 2);
        render_draw_line(ctx, x - 4, y + 6, x + 4, y + 6);
    }
}

void render_current_source(RenderContext *ctx, float x, float y, int rotation) {
    render_draw_circle(ctx, x, y, 15);
    render_draw_line(ctx, x, y - 15, x, y - 25);
    render_draw_line(ctx, x, y + 15, x, y + 25);
    // Arrow
    render_draw_line(ctx, x, y + 8, x, y - 8);
    render_draw_line(ctx, x - 4, y - 4, x, y - 8);
    render_draw_line(ctx, x + 4, y - 4, x, y - 8);
}

void render_resistor(RenderContext *ctx, float x, float y, int rotation) {
    render_draw_line(ctx, x - 35, y, x - 25, y);
    // Zigzag
    int points[][2] = {{-25,0},{-20,-8},{-12,8},{-4,-8},{4,8},{12,-8},{20,8},{25,0}};
    for (int i = 0; i < 7; i++) {
        render_draw_line(ctx, x + points[i][0], y + points[i][1],
                        x + points[i+1][0], y + points[i+1][1]);
    }
    render_draw_line(ctx, x + 25, y, x + 35, y);
}

void render_capacitor(RenderContext *ctx, float x, float y, int rotation) {
    render_draw_line(ctx, x - 25, y, x - 5, y);
    render_draw_line(ctx, x - 5, y - 12, x - 5, y + 12);
    render_draw_line(ctx, x + 5, y - 12, x + 5, y + 12);
    render_draw_line(ctx, x + 5, y, x + 25, y);
}

void render_inductor(RenderContext *ctx, float x, float y, int rotation) {
    render_draw_line(ctx, x - 35, y, x - 25, y);
    // Coils
    for (int i = 0; i < 4; i++) {
        float cx = x - 18 + i * 12;
        for (int a = 180; a <= 360; a += 15) {
            float r = a * M_PI / 180;
            float r2 = (a + 15) * M_PI / 180;
            render_draw_line(ctx, cx + 6*cos(r), y + 6*sin(r),
                            cx + 6*cos(r2), y + 6*sin(r2));
        }
    }
    render_draw_line(ctx, x + 25, y, x + 35, y);
}

void render_diode(RenderContext *ctx, float x, float y, int rotation) {
    render_draw_line(ctx, x - 25, y, x - 8, y);
    // Triangle
    render_draw_line(ctx, x - 8, y - 10, x - 8, y + 10);
    render_draw_line(ctx, x - 8, y - 10, x + 8, y);
    render_draw_line(ctx, x - 8, y + 10, x + 8, y);
    // Bar
    render_draw_line(ctx, x + 8, y - 10, x + 8, y + 10);
    render_draw_line(ctx, x + 8, y, x + 25, y);
}

void render_bjt(RenderContext *ctx, float x, float y, int rotation, bool is_pnp) {
    render_draw_circle(ctx, x, y, 20);
    render_draw_line(ctx, x - 25, y, x - 5, y);
    render_draw_line(ctx, x - 5, y - 12, x - 5, y + 12);
    render_draw_line(ctx, x - 5, y - 6, x + 12, y - 16);
    render_draw_line(ctx, x + 12, y - 16, x + 15, y - 20);
    render_draw_line(ctx, x - 5, y + 6, x + 12, y + 16);
    render_draw_line(ctx, x + 12, y + 16, x + 15, y + 20);
}

void render_mosfet(RenderContext *ctx, float x, float y, int rotation, bool is_pmos) {
    render_draw_line(ctx, x - 25, y, x - 10, y);
    render_draw_line(ctx, x - 10, y - 10, x - 10, y + 10);
    render_draw_line(ctx, x - 5, y - 12, x - 5, y - 4);
    render_draw_line(ctx, x - 5, y + 4, x - 5, y + 12);
    render_draw_line(ctx, x - 5, y - 8, x + 10, y - 8);
    render_draw_line(ctx, x + 10, y - 8, x + 10, y - 20);
    render_draw_line(ctx, x + 10, y - 20, x + 15, y - 20);
    render_draw_line(ctx, x - 5, y + 8, x + 10, y + 8);
    render_draw_line(ctx, x + 10, y + 8, x + 10, y + 20);
    render_draw_line(ctx, x + 10, y + 20, x + 15, y + 20);
}

void render_opamp(RenderContext *ctx, float x, float y, int rotation) {
    // Triangle
    render_draw_line(ctx, x - 25, y - 25, x - 25, y + 25);
    render_draw_line(ctx, x - 25, y - 25, x + 25, y);
    render_draw_line(ctx, x - 25, y + 25, x + 25, y);
    // Inputs
    render_draw_line(ctx, x - 35, y - 12, x - 25, y - 12);
    render_draw_line(ctx, x - 35, y + 12, x - 25, y + 12);
    // Output
    render_draw_line(ctx, x + 25, y, x + 35, y);
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
