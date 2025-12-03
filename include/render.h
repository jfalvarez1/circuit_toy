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
    bool show_heatmap;  // Thermal heatmap overlay mode

    // Animation timing (for current flow)
    double sim_time;
    bool sim_running;

    // Real-time animation (independent of simulation speed)
    double animation_time;      // Real-time accumulator for smooth animation
    double last_frame_time;     // Last frame timestamp for delta calculation
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
void render_draw_text_styled(RenderContext *ctx, const char *text, int x, int y, Color color,
                             int font_size, bool bold, bool italic, bool underline);

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
void render_bjt(RenderContext *ctx, float x, float y, int rotation, bool is_pnp, const char *label);
void render_mosfet(RenderContext *ctx, float x, float y, int rotation, bool is_pmos, const char *label);
void render_opamp(RenderContext *ctx, float x, float y, int rotation);
void render_opamp_flipped(RenderContext *ctx, float x, float y, int rotation);
void render_square_wave(RenderContext *ctx, float x, float y, int rotation);
void render_triangle_wave(RenderContext *ctx, float x, float y, int rotation);
void render_sawtooth_wave(RenderContext *ctx, float x, float y, int rotation);
void render_noise_source(RenderContext *ctx, float x, float y, int rotation);
void render_pin(RenderContext *ctx, float x, float y, int rotation, int pin_number, const char *pin_name);

// Ghost component (while placing)
void render_ghost_component(RenderContext *ctx, Component *comp);

// Short circuit highlight (draws blinking red rectangles around shorted components)
void render_short_circuit_highlights(RenderContext *ctx, Circuit *circuit,
                                     int *comp_ids, int comp_count);

// Wire preview
void render_wire_preview(RenderContext *ctx, float x1, float y1, float x2, float y2);

// Selection box (for multi-select drag)
void render_selection_box(RenderContext *ctx, float x1, float y1, float x2, float y2);

// Thermal heatmap rendering
void render_heatmap_overlay(RenderContext *ctx, Component *comp);
Color temperature_to_color(double temp, double min_temp, double max_temp);

// Node voltage tooltip (renders near cursor when hovering over a node)
void render_node_voltage_tooltip(RenderContext *ctx, int screen_x, int screen_y, double voltage);

#endif // RENDER_H
