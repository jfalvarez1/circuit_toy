/**
 * Circuit Playground - Main Application
 */

#ifndef APP_H
#define APP_H

#include <SDL.h>
#include "types.h"
#include "circuit.h"
#include "simulation.h"
#include "render.h"
#include "ui.h"
#include "input.h"

// Application state
typedef struct {
    // SDL handles
    SDL_Window *window;
    SDL_Renderer *renderer;

    // Core systems
    Circuit *circuit;
    Simulation *simulation;
    RenderContext *render;
    UIState ui;
    InputState input;

    // Application state
    bool running;
    bool show_voltages;
    bool show_current;

    // Current file
    char current_file[256];
    bool has_file;

    // Frame timing
    uint32_t last_frame_time;
    uint32_t frame_count;
    float fps;
} App;

// Initialize application
bool app_init(App *app);

// Shutdown application
void app_shutdown(App *app);

// Main loop iteration
void app_update(App *app);

// Render frame
void app_render(App *app);

// Handle events
void app_handle_events(App *app);

// File operations
void app_new_circuit(App *app);
void app_save_circuit(App *app);
void app_save_circuit_as(App *app);
void app_load_circuit(App *app);

// Simulation control
void app_run_simulation(App *app);
void app_pause_simulation(App *app);
void app_step_simulation(App *app);
void app_reset_simulation(App *app);

// UI callbacks
void app_on_component_selected(App *app, Component *comp);
void app_on_component_deselected(App *app);
void app_on_property_changed(App *app, Component *comp);

#endif // APP_H
