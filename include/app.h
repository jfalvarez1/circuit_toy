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
#include "updater.h"
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
    /* --shot-region. The whole window unless asked otherwise, which is the opposite of the
       toolbar's default and deliberately so: the GUI audits point --shot at the program and
       measure its toolbar and palette, while a person pressing the button is photographing the
       circuit. Each default is what its own caller wants; --shot-region overrides this one. */
    int  cli_shot_region;
    char cli_record_dir[260];    // --record DIR N EVERY : save N frames, one every EVERY frames
    int  cli_record_frames, cli_record_every, cli_recorded;
    bool cli_exit;               // --exit : quit once the shot / recording is done
    int  cli_frame;              // frames rendered since start
    char cli_keys[64];           // --keys "^mosfet" FRAME EVERY : from FRAME, one char every EVERY frames ('^' opens Spotlight, '|' = Enter)
    int  cli_keys_frame, cli_keys_every, cli_keys_pos;
    /* --click and --drag: scripted mouse, so a GUI smoke test can press the buttons a user
       presses instead of calling the functions behind them. Events go through SDL_PushEvent,
       so they take exactly the path a real pointer takes. */
    struct { int x, y, x2, y2, frame; bool drag, done; } cli_mouse[12];
    int  cli_mouse_n;
    int  cli_mod_until;          // frame at which a scripted Ctrl release takes effect
    char cli_state_path[260];    // --state-out FILE: what the app is, in numbers, at the shot

    // Auto-update (GitHub releases)
    UpdaterState updater;
    bool update_announced;
    /* Auto-update: once the background check finds a newer tag the app installs it by itself
       and the installer relaunches it. The countdown exists so the message is readable and so
       there is a moment to press Esc; a circuit with unsaved changes is never interrupted. */
    Uint32 update_due_ms;        // 0 = nothing scheduled
    char   update_tag[128];
    bool   update_deferred;      // user pressed Esc, or the circuit is modified: use the button
    bool   no_auto_update;       // --no-auto-update: still check, but wait to be asked       // status message shown once
    bool skip_update_check;      // --no-update-check
    int  saved_window_w, saved_window_h;   // from settings.json (0 = default)
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
void app_update_window_metrics(App *app);  // UI size and scale from the window size
void app_zoom_to_fit(App *app);   // frame every placed component in the canvas

/* Ask the user for a file. Returns false if they cancelled, or if the platform has no dialog -
   on Windows this is the ordinary Open dialog; elsewhere the caller falls back to a fixed name.
   `filter` is the Win32 double-NUL filter string. */
bool app_pick_file(char *out, size_t out_size, const char *title, const char *filter);
// Start the background release check (unless --no-update-check)
void app_update_check(App *app);
// Save the current window contents as a BMP (used by --shot / --record)
bool app_save_window_bmp(App *app, const char *path);

/* ShotRegion lives in ui.h, because the toolbar button that cycles it is laid out there. */
const char *app_shot_region_name(int region);

/* Save a screenshot of `region`. Reads the window that has just been presented, so whatever
   style the canvas was drawn in is the style that lands in the file. */
bool app_save_shot(App *app, const char *path, int region);
// Pop the oscilloscope into its own window (or dock it again). Used by the PopOut button and
// by --popout, so a scripted screenshot gets the same window a user gets.
void app_scope_popout(App *app, bool on);

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
/* --state-out: the app's own account of itself, written beside the screenshot. */
void app_write_state(App *app, const char *path);
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
