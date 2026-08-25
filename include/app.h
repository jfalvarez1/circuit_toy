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
#include "circuits.h"
#include "input.h"
#include "analysis.h"
#include "threadpool.h"

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
    AnalysisState analysis;

    // Application state
    bool running;

    // Command-line automation (screenshots / GIF frames without touching the user's window)
    char cli_shot_path[260];     // --shot FILE.bmp  : save the window at frame cli_shot_frame
    int  cli_shot_frame;
    char cli_record_dir[260];    // --record DIR N EVERY : save N frames, one every EVERY frames
    int  cli_record_frames, cli_record_every, cli_recorded;
    bool cli_exit;               // --exit : quit once the shot / recording is done
    int  cli_frame;              // frames rendered since start
    bool show_voltages;
    bool show_current;
    double synced_time_div;      // scope time/div the sim dt was last matched to (0 = never)
    double sim_step_carry;       // fractional steps carried between frames
    double sim_realtime_ratio;   // fraction of the real-time step target achieved last frame

    // Current file
    char current_file[256];
    bool has_file;

    // Frame timing
    uint32_t last_frame_time;
    uint32_t frame_count;
    float fps;

    // Background thread for frequency sweep
    SDL_Thread *freq_sweep_thread;
    bool freq_sweep_thread_running;

    // Thread pool for parallel processing
    ThreadPool thread_pool;
    int num_threads;
} App;

// Initialize application
bool app_init(App *app);
// Place a template at the canvas centre with its scope presets and auto-start (used by --template)
bool app_place_template_centered(App *app, CircuitTemplateType type);
// Save the current window contents as a BMP (used by --shot / --record)
bool app_save_window_bmp(App *app, const char *path);

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
