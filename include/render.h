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

    /* Supersampling. The frame is drawn into a target this many times the window's size and
       scaled back down when it is presented, which is the whole of the resolution difference:
       diagonals and circles stop being staircases, and glyphs come off a 48-pixel atlas with
       enough detail left to be worth smoothing. 1 turns it off. */
    int ss;
    float ui_scale;             // device pixels per UI pixel
    SDL_Texture *ss_tex;
    int ss_w, ss_h;             // window size the target was built for

    // Viewport
    float offset_x;
    float offset_y;
    float zoom;

    // Canvas bounds
    Rect canvas_rect;

    // Options
    bool show_grid;
    bool show_voltages;
    bool show_values;   // component value labels (F2)
    bool show_current;
    bool snap_to_grid;
    bool show_heatmap;  // Thermal heatmap overlay mode

    // Animation timing (for current flow)
    double sim_time;
    bool sim_running;

    // Real-time animation (independent of simulation speed)
    double animation_time;      // Real-time accumulator for smooth animation
    double last_frame_time;     // Last frame timestamp for delta calculation
    double flow_dt;             // seconds the flow dots should advance this frame

    /* Antialiased glyph atlas for schematic text: the 8x8 bitmap font resampled to a coverage
       map, built once on the first draw. The UI panels keep their own hard-edged font in ui.c;
       this one is only for what is drawn on the canvas. */
    SDL_Texture *font_atlas;
    bool font_atlas_tried;      // do not retry every frame if creating it failed
} RenderContext;

// Initialize/cleanup
RenderContext *render_create(SDL_Renderer *renderer);
void render_free(RenderContext *ctx);

// Coordinate transforms
void render_screen_to_world(RenderContext *ctx, int sx, int sy, float *wx, float *wy);
void render_world_to_screen(RenderContext *ctx, float wx, float wy, int *sx, int *sy);
void render_world_to_screen_f(RenderContext *ctx, float wx, float wy, float *sx, float *sy);

// Viewport control
void render_pan(RenderContext *ctx, int dx, int dy);
void render_zoom(RenderContext *ctx, float factor, int center_x, int center_y);
void render_reset_view(RenderContext *ctx);

// Drawing primitives (in world coordinates)
void render_set_color(RenderContext *ctx, Color color);
void render_draw_line(RenderContext *ctx, float x1, float y1, float x2, float y2);
void render_line_dev(RenderContext *ctx, float x1, float y1, float x2, float y2);

/* Draw the frame into the supersampled target, then scale it down onto the window. */
void render_frame_begin(RenderContext *ctx, int win_w, int win_h);
void render_frame_end(RenderContext *ctx);
void render_draw_rect(RenderContext *ctx, float x, float y, float w, float h);
void render_fill_rect(RenderContext *ctx, float x, float y, float w, float h);
void render_draw_circle(RenderContext *ctx, float cx, float cy, float r);
void render_fill_circle(RenderContext *ctx, float cx, float cy, float r);

// Drawing (screen coordinates)
void render_draw_line_screen(RenderContext *ctx, int x1, int y1, int x2, int y2);
void render_draw_rect_screen(RenderContext *ctx, int x, int y, int w, int h);
void render_fill_rect_screen(RenderContext *ctx, int x, int y, int w, int h);

// Text rendering (basic - screen coordinates)
int  render_text_px(RenderContext *ctx, int font_size);   /* glyph height at the current zoom */
void render_draw_text(RenderContext *ctx, const char *text, int x, int y, Color color);
void render_draw_text_small(RenderContext *ctx, const char *text, int x, int y, Color color);

/* Panel text: the same antialiased glyph atlas, reachable with only a renderer. */
void render_text_at(SDL_Renderer *r, const char *text, int x, int y, int px, Color col);
void render_draw_text_styled(RenderContext *ctx, const char *text, int x, int y, Color color,
                             int font_size, bool bold, bool italic, bool underline);

// Circuit rendering
void render_grid(RenderContext *ctx);
void render_component(RenderContext *ctx, Component *comp);

/* Counts components drawn with no symbol - see the default case in render_component. */
extern int g_render_missing_symbol;

/* Supersample factor for new render contexts; --ss sets it. */
extern int g_render_supersample;

/* Device pixels per UI pixel. The layout works in UI pixels throughout; this is applied once,
   when the frame is drawn and when a mouse position comes in. --ui-scale overrides it. */
extern float g_ui_scale_override;

/* Canvas stroke weight in logical pixels; --line-weight sets it. */
extern float g_render_line_weight;
float render_ui_scale(int device_h);

/* Where an element's flow dots sit this frame, as a fraction of its length from its first
   terminal to its second. `drift` is flow_drift() of whatever was watched at the step rate: 1
   marches, 0 swings, between the two does both. Advances `fs` by `dt` seconds; `clock` drives
   the swing. Exposed so --flowdir-test can assert it over a direct current and an alternating
   one without a window. */
float render_flow_offset(FlowState *fs, double drift, double current, double dt, double clock);
void render_wire(RenderContext *ctx, Wire *wire, Circuit *circuit);
void render_node(RenderContext *ctx, Node *node, bool show_voltage);
void render_probe(RenderContext *ctx, Circuit *circuit, Probe *probe, int index);
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

// Open circuit highlight (draws blinking yellow rectangles around open current sources)
void render_open_circuit_highlights(RenderContext *ctx, Circuit *circuit,
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

// Component tooltip (renders voltage drop and current when hovering over a component)
void render_component_tooltip(RenderContext *ctx, int screen_x, int screen_y, double voltage, double current);

#endif // RENDER_H
