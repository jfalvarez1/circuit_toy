/**
 * Circuit Playground - Rendering System
 */

#ifndef RENDER_H
#define RENDER_H

#include <SDL.h>
#include "types.h"
#include "circuit.h"

// Render context
typedef struct {
    SDL_Renderer *renderer;
    SDL_Texture *canvas_texture;

    // Viewport
    float offset_x;
    float offset_y;
    float zoom;

    // Canvas bounds
    Rect canvas_rect;

    // Options
    bool show_grid;
    bool show_voltages;
    bool show_current;
    bool snap_to_grid;
} RenderContext;

// Initialize/cleanup
RenderContext *render_create(SDL_Renderer *renderer);
void render_free(RenderContext *ctx);

// Coordinate transforms
void render_screen_to_world(RenderContext *ctx, int sx, int sy, float *wx, float *wy);
void render_world_to_screen(RenderContext *ctx, float wx, float wy, int *sx, int *sy);

// Viewport control
void render_pan(RenderContext *ctx, int dx, int dy);
void render_zoom(RenderContext *ctx, float factor, int center_x, int center_y);
void render_reset_view(RenderContext *ctx);

// Drawing primitives (in world coordinates)
void render_set_color(RenderContext *ctx, Color color);
void render_draw_line(RenderContext *ctx, float x1, float y1, float x2, float y2);
void render_draw_rect(RenderContext *ctx, float x, float y, float w, float h);
void render_fill_rect(RenderContext *ctx, float x, float y, float w, float h);
void render_draw_circle(RenderContext *ctx, float cx, float cy, float r);
void render_fill_circle(RenderContext *ctx, float cx, float cy, float r);

// Drawing (screen coordinates)
void render_draw_line_screen(RenderContext *ctx, int x1, int y1, int x2, int y2);
void render_draw_rect_screen(RenderContext *ctx, int x, int y, int w, int h);
void render_fill_rect_screen(RenderContext *ctx, int x, int y, int w, int h);

// Text rendering (basic - screen coordinates)
void render_draw_text(RenderContext *ctx, const char *text, int x, int y, Color color);
void render_draw_text_small(RenderContext *ctx, const char *text, int x, int y, Color color);

// Circuit rendering
void render_grid(RenderContext *ctx);
void render_component(RenderContext *ctx, Component *comp);
void render_wire(RenderContext *ctx, Wire *wire, Circuit *circuit);
void render_node(RenderContext *ctx, Node *node, bool show_voltage);
void render_probe(RenderContext *ctx, Probe *probe, int index);
void render_circuit(RenderContext *ctx, Circuit *circuit);

// Component shape rendering
void render_ground(RenderContext *ctx, float x, float y, int rotation);
void render_voltage_source(RenderContext *ctx, float x, float y, int rotation, bool is_ac);
void render_current_source(RenderContext *ctx, float x, float y, int rotation);
void render_resistor(RenderContext *ctx, float x, float y, int rotation);
void render_capacitor(RenderContext *ctx, float x, float y, int rotation);
void render_inductor(RenderContext *ctx, float x, float y, int rotation);
void render_diode(RenderContext *ctx, float x, float y, int rotation);
void render_bjt(RenderContext *ctx, float x, float y, int rotation, bool is_pnp);
void render_mosfet(RenderContext *ctx, float x, float y, int rotation, bool is_pmos);
void render_opamp(RenderContext *ctx, float x, float y, int rotation);

// Ghost component (while placing)
void render_ghost_component(RenderContext *ctx, Component *comp);

// Wire preview
void render_wire_preview(RenderContext *ctx, float x1, float y1, float x2, float y2);

#endif // RENDER_H
