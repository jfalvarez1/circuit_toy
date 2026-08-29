/**
 * Circuit Playground - Main Application Implementation
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>  // for _mkdir
#else
#include <sys/stat.h>  // for mkdir
#endif
#ifdef _WIN32
#include <windows.h>   /* GetOpenFileNameA */
#include <commdlg.h>
#endif

#include "app.h"
#include "spice.h"
#include "crashlog.h"
#include "version.h"
#include "label.h"   /* label_wrap: framing has to measure text the way it is drawn */
#include "settings.h"
#include "file_io.h"
#include "circuits.h"
#include "analysis.h"

// Global wireless state for antenna TX/RX pairs
WirelessState g_wireless = {0};

// Thread data for frequency sweep
typedef struct {
    Simulation *sim;
    double start_freq;
    double stop_freq;
    int source_node;
    int probe_node;
    int num_points;
    bool success;
} FreqSweepThreadData;

// Thread function for frequency sweep
static int freq_sweep_thread_func(void *data) {
    FreqSweepThreadData *td = (FreqSweepThreadData *)data;
    td->success = simulation_freq_sweep(td->sim, td->start_freq, td->stop_freq,
                                        td->source_node, td->probe_node, td->num_points);
    return 0;
}

// Static thread data (one sweep at a time)
static FreqSweepThreadData g_sweep_data;

// Static Monte Carlo backup storage
static MCBackup g_mc_backup;

bool app_init(App *app) {
    memset(app, 0, sizeof(App));

    // Create window. The title carries the version, which is what the taskbar shows.
    app->window = SDL_CreateWindow(
        "Circuit Playground v" APP_VERSION,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!app->window) {
        crashlog_note("FATAL: SDL_CreateWindow failed: %s", SDL_GetError());
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        return false;
    }
    crashlog_note("window created");

    // Set window icon for Linux/macOS (Windows uses embedded ICO from resource file)
#ifndef _WIN32
    SDL_Surface *icon = SDL_LoadBMP("icon.bmp");
    if (icon) {
        SDL_SetWindowIcon(app->window, icon);
        SDL_FreeSurface(icon);
    }
#endif

    // Create renderer
    /* Accelerated with vsync is what we want. On a machine with a broken or missing GPU driver
       - a VM, a fresh install, remote desktop - that combination fails, and refusing to start is
       a worse answer than running on the software renderer. Each fallback is logged, so a slow
       or odd-looking session can be explained afterwards. */
    app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!app->renderer) {
        crashlog_note("accelerated+vsync renderer failed (%s); trying accelerated without vsync", SDL_GetError());
        app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_ACCELERATED);
    }
    if (!app->renderer) {
        crashlog_note("accelerated renderer failed (%s); trying software", SDL_GetError());
        app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!app->renderer) {
        crashlog_note("FATAL: no renderer of any kind: %s", SDL_GetError());
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(app->window);
        return false;
    }
    {
        SDL_RendererInfo ri;
        if (SDL_GetRendererInfo(app->renderer, &ri) == 0)
            crashlog_note("renderer '%s'%s%s", ri.name ? ri.name : "?",
                          (ri.flags & SDL_RENDERER_ACCELERATED) ? " accelerated" : " software",
                          (ri.flags & SDL_RENDERER_PRESENTVSYNC) ? " vsync" : "");
    }

    // Enable alpha blending
    SDL_SetRenderDrawBlendMode(app->renderer, SDL_BLENDMODE_BLEND);

    // Create render context
    app->render = render_create(app->renderer);
    if (!app->render) {
        SDL_DestroyRenderer(app->renderer);
        SDL_DestroyWindow(app->window);
        return false;
    }

    // Create circuit
    app->circuit = circuit_create();
    if (!app->circuit) {
        render_free(app->render);
        SDL_DestroyRenderer(app->renderer);
        SDL_DestroyWindow(app->window);
        return false;
    }

    // Create simulation engine
    app->simulation = simulation_create(app->circuit);
    if (!app->simulation) {
        circuit_free(app->circuit);
        render_free(app->render);
        SDL_DestroyRenderer(app->renderer);
        SDL_DestroyWindow(app->window);
        return false;
    }

    // Initialize UI
    ui_init(&app->ui);

    // Initialize input
    input_init(&app->input);

    // Initialize analysis
    analysis_init(&app->analysis);

    // Set initial state
    app->running = true;
    app->render->show_values = true;
    app->show_voltages = false;
    app->show_current = false;
    app->last_frame_time = SDL_GetTicks();

    // Persistent preferences (brightness, window size, view toggles ...): %APPDATA%\circuit_toy\circuit-playground\settings.json
    crashlog_note("loading settings");
    settings_load(app);
    crashlog_note("settings loaded");
    if (app->saved_window_w > 0 && app->saved_window_h > 0) SDL_SetWindowSize(app->window, app->saved_window_w, app->saved_window_h);

    ui_set_status(&app->ui, "Ready - Select a component or tool to begin");

    render_reset_view(app->render);

    return true;
}

void app_shutdown(App *app) {
    if (!app->cli_shot_path[0] && !app->cli_record_dir[0]) settings_save(app);   // scripted runs never overwrite the user's preferences
    updater_shutdown(&app->updater);
    // Cancel and wait for frequency sweep thread if running
    if (app->freq_sweep_thread_running && app->simulation) {
        simulation_cancel_freq_sweep(app->simulation);
        SDL_WaitThread(app->freq_sweep_thread, NULL);
        app->freq_sweep_thread = NULL;
        app->freq_sweep_thread_running = false;
    }

    // Clean up popup oscilloscope window if open
    if (app->ui.scope_popup_renderer) {
        SDL_DestroyRenderer(app->ui.scope_popup_renderer);
        app->ui.scope_popup_renderer = NULL;
    }
    if (app->ui.scope_popup_window) {
        SDL_DestroyWindow(app->ui.scope_popup_window);
        app->ui.scope_popup_window = NULL;
    }
    app->ui.scope_popped_out = false;

    if (app->simulation) {
        simulation_free(app->simulation);
        app->simulation = NULL;
    }

    if (app->circuit) {
        circuit_free(app->circuit);
        app->circuit = NULL;
    }

    if (app->render) {
        render_free(app->render);
        app->render = NULL;
    }

    if (app->renderer) {
        SDL_DestroyRenderer(app->renderer);
        app->renderer = NULL;
    }

    if (app->window) {
        SDL_DestroyWindow(app->window);
        app->window = NULL;
    }
}

void app_handle_events(App *app) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                app->running = false;
                break;

            case SDL_WINDOWEVENT:
                // Check if this event is for the popup window
                if (app->ui.scope_popped_out &&
                    event.window.windowID == app->ui.scope_popup_window_id) {
                    if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                        // Close the popup window (dock the oscilloscope)
                        if (app->ui.scope_popup_renderer) {
                            SDL_DestroyRenderer(app->ui.scope_popup_renderer);
                            app->ui.scope_popup_renderer = NULL;
                        }
                        if (app->ui.scope_popup_window) {
                            SDL_DestroyWindow(app->ui.scope_popup_window);
                            app->ui.scope_popup_window = NULL;
                        }
                        app->ui.scope_popup_window_id = 0;
                        app->ui.scope_popped_out = false;
                        ui_set_status(&app->ui, "Oscilloscope docked");
                    }
                } else if (event.window.event == SDL_WINDOWEVENT_RESIZED || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    // Handle main window resize
                    int w, h;
                    SDL_GetWindowSize(app->window, &w, &h);

                    // Update UI dimensions
                    app->ui.window_width = w;
                    app->ui.window_height = h;

                    // Update canvas area
                    app->render->canvas_rect.w = w - PALETTE_WIDTH - app->ui.properties_width;
                    app->render->canvas_rect.h = h - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT;

                    // Update UI layout (scope position, buttons, etc.)
                    ui_update_layout(&app->ui);
                }
                break;

            default:
                // Let input handler process the event
                if (input_handle_event(&app->input, &event,
                                       app->circuit, app->render, &app->ui)) {
                    // Event was handled, check for actions
                    if (app->input.selected_component) {
                        app_on_component_selected(app, app->input.selected_component);
                    }
                }
                break;
        }
    }

    // Handle auto-start simulation for oscillator circuits
    if (app->input.should_autostart_sim) {
        app->input.should_autostart_sim = false;
        app_run_simulation(app);
    }

    // Handle UI actions from button clicks
    if (app->input.pending_ui_action != UI_ACTION_NONE) {
        switch (app->input.pending_ui_action) {
            case UI_ACTION_RUN:
                app_run_simulation(app);
                break;
            case UI_ACTION_PAUSE:
                app_pause_simulation(app);
                break;
            case UI_ACTION_STEP:
                app_step_simulation(app);
                break;
            case UI_ACTION_RESET:
                app_reset_simulation(app);
                break;
            case UI_ACTION_CLEAR:
                app_new_circuit(app);
                break;
            case UI_ACTION_SAVE:
                app_save_circuit(app);
                break;
            case UI_ACTION_LOAD:
                app_load_circuit(app);
                break;
            case UI_ACTION_IMPORT_SPICE: {
                char picked[512];
                if (!app_pick_file(picked, sizeof picked, "Import a SPICE model",
                                   "SPICE netlists (*.cir;*.sp;*.lib;*.mod;*.txt)\0*.cir;*.sp;*.lib;*.mod;*.txt\0All files (*.*)\0*.*\0")) {
                    ui_set_status(&app->ui, "Import cancelled");
                    break;
                }
                char msg[192] = "";
                int n = spice_import_file(picked, msg, sizeof msg);
                if (n > 0) {
                    ui_update_layout(&app->ui);          /* the new subcircuits go in the palette */
                    char done[256];
                    snprintf(done, sizeof done, "Imported %d subcircuit%s%s%s", n, n == 1 ? "" : "s",
                             msg[0] ? " - " : "", msg);
                    ui_set_status(&app->ui, done);
                } else {
                    char fail[256];
                    snprintf(fail, sizeof fail, "Nothing imported%s%s", msg[0] ? ": " : "", msg);
                    ui_set_status(&app->ui, fail);
                }
                break;
            }
            case UI_ACTION_EXPORT_SVG:
                {
                    // Generate timestamped filename
                    time_t now = time(NULL);
                    struct tm *t = localtime(&now);
                    char filename[256];
                    snprintf(filename, sizeof(filename), "circuit_%04d%02d%02d_%02d%02d%02d.svg",
                        t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                        t->tm_hour, t->tm_min, t->tm_sec);

                    if (file_export_svg(app->circuit, filename)) {
                        printf("Circuit exported to %s\n", filename);
                    } else {
                        printf("Failed to export SVG: %s\n", file_get_error());
                    }
                }
                break;
            case UI_ACTION_DEFER_UPDATE:
                app->update_deferred = true;
                app->update_due_ms = 0;
                app->ui.update_countdown_active = false;
                ui_set_status(&app->ui, "Auto-update cancelled - click Update in the toolbar when you are ready");
                break;
            case UI_ACTION_ZOOM_IN:
                render_zoom(app->render, 1.2f, app->render->canvas_rect.x + app->render->canvas_rect.w / 2,
                            app->render->canvas_rect.y + app->render->canvas_rect.h / 2);
                break;
            case UI_ACTION_ZOOM_OUT:
                render_zoom(app->render, 0.8f, app->render->canvas_rect.x + app->render->canvas_rect.w / 2,
                            app->render->canvas_rect.y + app->render->canvas_rect.h / 2);
                break;
            case UI_ACTION_ZOOM_FIT:
                app_zoom_to_fit(app);
                break;
            case UI_ACTION_SCREENSHOT:
                {
                    // Create screenshots directory
                    #ifdef _WIN32
                    _mkdir("screenshots");
                    #else
                    mkdir("screenshots", 0755);
                    #endif

                    // Generate timestamped filename
                    time_t now = time(NULL);
                    struct tm *t = localtime(&now);
                    char filename[256];
                    snprintf(filename, sizeof(filename), "screenshots/screenshot_%04d%02d%02d_%02d%02d%02d.bmp",
                        t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                        t->tm_hour, t->tm_min, t->tm_sec);

                    // Get window size
                    int w, h;
                    SDL_GetRendererOutputSize(app->renderer, &w, &h);

                    // Create surface to hold screenshot
                    SDL_Surface *surface = SDL_CreateRGBSurface(0, w, h, 32,
                        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

                    if (surface) {
                        // Read pixels from renderer
                        if (SDL_RenderReadPixels(app->renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                                                 surface->pixels, surface->pitch) == 0) {
                            // Save to BMP
                            if (SDL_SaveBMP(surface, filename) == 0) {
                                printf("Screenshot saved: %s\n", filename);
                                ui_set_status(&app->ui, "Screenshot saved!");
                            } else {
                                printf("Failed to save BMP: %s\n", SDL_GetError());
                                ui_set_status(&app->ui, "Screenshot failed!");
                            }
                        } else {
                            printf("Failed to read pixels: %s\n", SDL_GetError());
                            ui_set_status(&app->ui, "Screenshot failed!");
                        }
                        SDL_FreeSurface(surface);
                    } else {
                        printf("Failed to create surface: %s\n", SDL_GetError());
                        ui_set_status(&app->ui, "Screenshot failed!");
                    }
                }
                break;
            case UI_ACTION_SCOPE_VOLT_UP:
                /* on whatever the ALL / channel chips point at, so one channel can be scaled
                   without moving the others */
                ui_scope_volt_step(&app->ui, +1);
                break;
            case UI_ACTION_SCOPE_VOLT_DOWN:
                ui_scope_volt_step(&app->ui, -1);
                break;
            case UI_ACTION_SCOPE_TIME_UP:
                // Increase time/div using 1-2-5 sequence
                {
                    static const double time_steps[] = {
                        10e-9, 20e-9, 50e-9, 100e-9, 200e-9, 500e-9,
                        1e-6, 2e-6, 5e-6, 10e-6, 20e-6, 50e-6,
                        100e-6, 200e-6, 500e-6,
                        1e-3, 2e-3, 5e-3, 10e-3, 20e-3, 50e-3,
                        100e-3, 200e-3, 500e-3,
                        1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0
                    };
                    int n = sizeof(time_steps) / sizeof(time_steps[0]);
                    for (int i = 0; i < n - 1; i++) {
                        if (app->ui.scope_time_div <= time_steps[i] * 1.01) {
                            app->ui.scope_time_div = time_steps[i + 1];
                            break;
                        }
                    }
                    // Invalidate capture to force re-render with new time scale
                    app->ui.scope_capture_valid = false;
                }
                break;
            case UI_ACTION_SCOPE_TIME_DOWN:
                // Decrease time/div using 1-2-5 sequence
                {
                    static const double time_steps[] = {
                        10e-9, 20e-9, 50e-9, 100e-9, 200e-9, 500e-9,
                        1e-6, 2e-6, 5e-6, 10e-6, 20e-6, 50e-6,
                        100e-6, 200e-6, 500e-6,
                        1e-3, 2e-3, 5e-3, 10e-3, 20e-3, 50e-3,
                        100e-3, 200e-3, 500e-3,
                        1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0
                    };
                    int n = sizeof(time_steps) / sizeof(time_steps[0]);
                    for (int i = n - 1; i > 0; i--) {
                        if (app->ui.scope_time_div >= time_steps[i] * 0.99) {
                            app->ui.scope_time_div = time_steps[i - 1];
                            break;
                        }
                    }
                    // Invalidate capture to force re-render with new time scale
                    app->ui.scope_capture_valid = false;
                }
                break;
            case UI_ACTION_SCOPE_TRIG_MODE:
                // Cycle through trigger modes: Auto -> Normal -> Single -> Auto
                app->ui.trigger_mode = (app->ui.trigger_mode + 1) % 3;
                // Reset trigger capture state when mode changes
                app->ui.scope_capture_valid = false;
                app->ui.triggered = false;
                if (app->ui.trigger_mode == TRIG_SINGLE) {
                    app->ui.trigger_armed = true;
                }
                break;
            case UI_ACTION_SCOPE_TRIG_EDGE:
                // Cycle through trigger edges: Rising -> Falling -> Both -> Rising
                app->ui.trigger_edge = (app->ui.trigger_edge + 1) % 3;
                break;
            case UI_ACTION_SCOPE_TRIG_CH:
                // Cycle through trigger channels
                if (app->ui.scope_num_channels > 0) {
                    app->ui.trigger_channel = (app->ui.trigger_channel + 1) % app->ui.scope_num_channels;
                }
                break;
            case UI_ACTION_SCOPE_MODE:
                // Toggle between Y-T and X-Y mode
                app->ui.display_mode = (app->ui.display_mode == SCOPE_MODE_YT) ?
                                       SCOPE_MODE_XY : SCOPE_MODE_YT;
                break;
            case UI_ACTION_SCOPE_TRIG_UP:
            case UI_ACTION_SCOPE_TRIG_DOWN: {
                // One fifth of a division per click, kept inside the visible +/-4 divisions
                double step = app->ui.scope_volt_div * 0.2;
                double lim = 4.0 * app->ui.scope_volt_div;
                app->ui.trigger_level += (app->input.pending_ui_action == UI_ACTION_SCOPE_TRIG_UP) ? step : -step;
                if (app->ui.trigger_level > lim) app->ui.trigger_level = lim;
                if (app->ui.trigger_level < -lim) app->ui.trigger_level = -lim;
                app->ui.scope_capture_valid = false;   // re-trigger at the new level right away
                char msg[96], vs[32];
                if (fabs(app->ui.trigger_level) >= 1000) snprintf(vs, sizeof vs, "%.4g kV", app->ui.trigger_level / 1e3);
                else if (fabs(app->ui.trigger_level) < 0.1 && app->ui.trigger_level != 0) snprintf(vs, sizeof vs, "%.1f mV", app->ui.trigger_level * 1e3);
                else snprintf(vs, sizeof vs, "%.2f V", app->ui.trigger_level);
                snprintf(msg, sizeof msg, "Trigger level %s on CH%d (drag the orange line to set it directly)", vs, app->ui.trigger_channel + 1);
                ui_set_status(&app->ui, msg);
                break;
            }
            case UI_ACTION_SCOPE_SCREENSHOT:
                // Capture oscilloscope display as BMP
                {
                    Rect *sr = &app->ui.scope_rect;
                    SDL_Surface *surface = SDL_CreateRGBSurface(0, sr->w, sr->h, 32,
                        0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
                    if (surface) {
                        // Read pixels from renderer
                        SDL_Rect read_rect = {sr->x, sr->y, sr->w, sr->h};
                        if (SDL_RenderReadPixels(app->renderer, &read_rect,
                                SDL_PIXELFORMAT_ARGB8888, surface->pixels, surface->pitch) == 0) {
                            // Generate filename with timestamp
                            char filename[64];
                            time_t now = time(NULL);
                            struct tm *t = localtime(&now);
                            snprintf(filename, sizeof(filename), "scope_%04d%02d%02d_%02d%02d%02d.bmp",
                                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                                t->tm_hour, t->tm_min, t->tm_sec);
                            if (SDL_SaveBMP(surface, filename) == 0) {
                                char msg[128];
                                snprintf(msg, sizeof(msg), "Screenshot saved: %s", filename);
                                ui_set_status(&app->ui, msg);
                            } else {
                                ui_set_status(&app->ui, "Failed to save screenshot");
                            }
                        }
                        SDL_FreeSurface(surface);
                    }
                }
                break;

            case UI_ACTION_BODE_PLOT:
                // Toggle Bode plot display and run frequency sweep
                if (app->ui.show_bode_plot) {
                    // If already showing, hide it and cancel any running sweep
                    if (app->freq_sweep_thread_running && app->simulation) {
                        simulation_cancel_freq_sweep(app->simulation);
                        SDL_WaitThread(app->freq_sweep_thread, NULL);
                        app->freq_sweep_thread = NULL;
                        app->freq_sweep_thread_running = false;
                    }
                    app->ui.show_bode_plot = false;
                } else {
                    // Don't start a new sweep if one is already running
                    if (app->freq_sweep_thread_running) {
                        ui_set_status(&app->ui, "Frequency sweep already in progress...");
                        break;
                    }

                    // Show and run frequency sweep in background thread
                    app->ui.show_bode_plot = true;

                    // Find a probe node to use as output
                    int probe_node = 0;
                    if (app->circuit && app->circuit->num_probes > 0) {
                        probe_node = app->circuit->probes[0].node_id;
                    }

                    // Start frequency sweep in background thread
                    if (app->simulation) {
                        g_sweep_data.sim = app->simulation;
                        g_sweep_data.start_freq = app->ui.bode_freq_start;
                        g_sweep_data.stop_freq = app->ui.bode_freq_stop;
                        g_sweep_data.source_node = 0;
                        g_sweep_data.probe_node = probe_node;
                        g_sweep_data.num_points = app->ui.bode_num_points;
                        g_sweep_data.success = false;

                        app->freq_sweep_thread = SDL_CreateThread(
                            freq_sweep_thread_func, "FreqSweep", &g_sweep_data);
                        if (app->freq_sweep_thread) {
                            app->freq_sweep_thread_running = true;
                            ui_set_status(&app->ui, "Running frequency sweep...");
                        } else {
                            ui_set_status(&app->ui, "Failed to start frequency sweep thread");
                        }
                    }
                }
                break;

            case UI_ACTION_BODE_RECALC:
                // Recalculate Bode plot with current settings (don't toggle, just recalc)
                if (app->ui.show_bode_plot) {
                    // Cancel any running sweep first
                    if (app->freq_sweep_thread_running && app->simulation) {
                        simulation_cancel_freq_sweep(app->simulation);
                        SDL_WaitThread(app->freq_sweep_thread, NULL);
                        app->freq_sweep_thread = NULL;
                        app->freq_sweep_thread_running = false;
                    }

                    // Find a probe node to use as output
                    int probe_node = 0;
                    if (app->circuit && app->circuit->num_probes > 0) {
                        probe_node = app->circuit->probes[0].node_id;
                    }

                    // Start frequency sweep in background thread
                    if (app->simulation) {
                        g_sweep_data.sim = app->simulation;
                        g_sweep_data.start_freq = app->ui.bode_freq_start;
                        g_sweep_data.stop_freq = app->ui.bode_freq_stop;
                        g_sweep_data.source_node = 0;
                        g_sweep_data.probe_node = probe_node;
                        g_sweep_data.num_points = app->ui.bode_num_points;
                        g_sweep_data.success = false;

                        app->freq_sweep_thread = SDL_CreateThread(
                            freq_sweep_thread_func, "FreqSweep", &g_sweep_data);
                        if (app->freq_sweep_thread) {
                            app->freq_sweep_thread_running = true;
                            ui_set_status(&app->ui, "Recalculating frequency sweep...");
                        } else {
                            ui_set_status(&app->ui, "Failed to start frequency sweep thread");
                        }
                    }
                }
                break;

            case UI_ACTION_CURSOR_TOGGLE:
                // Cycle cursors: Off -> Waveform (a/b track the source trace) -> Screen (H+V bars) -> Off
                app->ui.scope_cursor_type = (app->ui.scope_cursor_type + 1) % 3;
                app->ui.scope_cursor_mode = (app->ui.scope_cursor_type != 0);
                if (app->ui.scope_cursor_type == 1) {
                    ui_set_status(&app->ui, "Waveform cursors: drag a/b; readout shows t, V, dt, 1/dt, dV/dt and gated Vpp/mean/rms (source = trigger channel)");
                } else if (app->ui.scope_cursor_type == 2) {
                    ui_set_status(&app->ui, "Screen cursors: drag the vertical (time) or horizontal (amplitude) bars");
                } else {
                    ui_set_status(&app->ui, "Cursors OFF");
                }
                break;

            case UI_ACTION_UPDATE: {
                char msg[200];
                if (updater_install(&app->updater, msg, sizeof msg)) { ui_set_status(&app->ui, msg); app->running = false; }
                else ui_set_status(&app->ui, msg);
                break;
            }
            case UI_ACTION_SCOPE_TRACK:
                app->ui.scope_track_sweep = !app->ui.scope_track_sweep;
                ui_set_status(&app->ui, app->ui.scope_track_sweep
                    ? "Scope: time/div tracks the sweeping source (~3 cycles per screen)"
                    : "Scope: time/div tracking off");
                break;
            case UI_ACTION_SCOPE_AC:
                app->ui.scope_ac_coupling = !app->ui.scope_ac_coupling;
                ui_set_status(&app->ui, app->ui.scope_ac_coupling ? "Scope: AC coupling (traces drawn minus their DC level; readouts stay DC)" : "Scope: DC coupling");
                break;
            case UI_ACTION_SCOPE_FIT:
                app->ui.scope_stack_fit = !app->ui.scope_stack_fit;
                ui_set_status(&app->ui, app->ui.scope_stack_fit ? "Scope: Fit - each stacked band scaled to its own signal" : "Scope: Fit off - bands share V/div");
                break;
            case UI_ACTION_SCOPE_STACK:
                // Toggle stacked (one band per channel) vs overlay view
                app->ui.scope_stacked = !app->ui.scope_stacked;
                ui_set_status(&app->ui, app->ui.scope_stacked ? "Scope: stacked view (one band per channel)"
                                                              : "Scope: overlay view");
                break;
            case UI_ACTION_FFT_TOGGLE:
                // Toggle FFT spectrum view
                app->ui.scope_fft_mode = !app->ui.scope_fft_mode;
                if (app->ui.scope_fft_mode) {
                    ui_set_status(&app->ui, "FFT spectrum view enabled");
                } else {
                    ui_set_status(&app->ui, "FFT view disabled");
                }
                break;

            case UI_ACTION_SCOPE_AUTOSET:
                // Auto-configure scope settings based on signal analysis
                ui_scope_autoset(&app->ui, app->simulation);
                ui_set_status(&app->ui, "Scope autoset complete");
                break;

            case UI_ACTION_SCOPE_POPUP:
                app_scope_popout(app, !app->ui.scope_popped_out);
                break;

            case UI_ACTION_SWEEP_PANEL:
                // Toggle parametric sweep panel
                app->ui.show_sweep_panel = !app->ui.show_sweep_panel;
                if (app->ui.show_sweep_panel) {
                    ui_set_status(&app->ui, "Parametric Sweep: Select component to sweep");
                } else {
                    ui_set_status(&app->ui, "Sweep panel closed");
                }
                break;

            case UI_ACTION_MONTE_CARLO:
                // Toggle Monte Carlo panel
                app->ui.show_monte_carlo_panel = !app->ui.show_monte_carlo_panel;
                if (app->ui.show_monte_carlo_panel) {
                    ui_set_status(&app->ui, "Monte Carlo Analysis Panel");
                } else {
                    ui_set_status(&app->ui, "Monte Carlo panel closed");
                }
                break;

            case UI_ACTION_MC_RUN:
                // Start Monte Carlo analysis
                if (!app->analysis.monte_carlo.active) {
                    // Initialize MC analysis
                    analysis_monte_carlo_init(&app->analysis, app->ui.monte_carlo_runs,
                                             true, app->ui.monte_carlo_tolerance);
                    // Backup original component values
                    analysis_mc_backup_values(app->circuit, &g_mc_backup);
                    ui_set_status(&app->ui, "Monte Carlo analysis started...");
                }
                break;

            case UI_ACTION_MC_RUNS_UP:
                // Increase MC runs
                if (app->ui.monte_carlo_runs < 1000) {
                    if (app->ui.monte_carlo_runs < 50) app->ui.monte_carlo_runs += 10;
                    else if (app->ui.monte_carlo_runs < 200) app->ui.monte_carlo_runs += 25;
                    else app->ui.monte_carlo_runs += 100;
                    if (app->ui.monte_carlo_runs > 1000) app->ui.monte_carlo_runs = 1000;
                }
                break;

            case UI_ACTION_MC_RUNS_DOWN:
                // Decrease MC runs
                if (app->ui.monte_carlo_runs > 10) {
                    if (app->ui.monte_carlo_runs <= 50) app->ui.monte_carlo_runs -= 10;
                    else if (app->ui.monte_carlo_runs <= 200) app->ui.monte_carlo_runs -= 25;
                    else app->ui.monte_carlo_runs -= 100;
                    if (app->ui.monte_carlo_runs < 10) app->ui.monte_carlo_runs = 10;
                }
                break;

            case UI_ACTION_MC_TOL_UP:
                // Increase MC tolerance
                if (app->ui.monte_carlo_tolerance < 30.0) {
                    app->ui.monte_carlo_tolerance += 1.0;
                }
                break;

            case UI_ACTION_MC_TOL_DOWN:
                // Decrease MC tolerance
                if (app->ui.monte_carlo_tolerance > 1.0) {
                    app->ui.monte_carlo_tolerance -= 1.0;
                }
                break;

            case UI_ACTION_MC_RESET:
                // Reset Monte Carlo results
                analysis_monte_carlo_reset(&app->analysis);
                // Restore original values if MC was interrupted
                analysis_mc_restore_values(app->circuit, &g_mc_backup);
                ui_set_status(&app->ui, "Monte Carlo analysis reset");
                break;

            case UI_ACTION_TIMESTEP_UP:
                // Increase time step using 1-2-5 sequence
                {
                    static const double dt_steps[] = {
                        1e-9, 2e-9, 5e-9, 10e-9, 20e-9, 50e-9,
                        100e-9, 200e-9, 500e-9,
                        1e-6, 2e-6, 5e-6, 10e-6, 20e-6, 50e-6,
                        100e-6, 200e-6, 500e-6,
                        1e-3, 2e-3, 5e-3, 10e-3
                    };
                    int n = sizeof(dt_steps) / sizeof(dt_steps[0]);
                    double current_dt = app->simulation->time_step;
                    for (int i = 0; i < n - 1; i++) {
                        if (current_dt <= dt_steps[i] * 1.01) {
                            simulation_set_time_step(app->simulation, dt_steps[i + 1]);
                            break;
                        }
                    }
                    char msg[64];
                    double dt = app->simulation->time_step;
                    if (dt >= 1e-3) {
                        snprintf(msg, sizeof(msg), "Time step: %.1f ms", dt * 1e3);
                    } else if (dt >= 1e-6) {
                        snprintf(msg, sizeof(msg), "Time step: %.1f us", dt * 1e6);
                    } else {
                        snprintf(msg, sizeof(msg), "Time step: %.0f ns", dt * 1e9);
                    }
                    ui_set_status(&app->ui, msg);
                }
                break;

            case UI_ACTION_TIMESTEP_DOWN:
                // Decrease time step using 1-2-5 sequence
                {
                    static const double dt_steps[] = {
                        1e-9, 2e-9, 5e-9, 10e-9, 20e-9, 50e-9,
                        100e-9, 200e-9, 500e-9,
                        1e-6, 2e-6, 5e-6, 10e-6, 20e-6, 50e-6,
                        100e-6, 200e-6, 500e-6,
                        1e-3, 2e-3, 5e-3, 10e-3
                    };
                    int n = sizeof(dt_steps) / sizeof(dt_steps[0]);
                    double current_dt = app->simulation->time_step;
                    for (int i = n - 1; i > 0; i--) {
                        if (current_dt >= dt_steps[i] * 0.99) {
                            simulation_set_time_step(app->simulation, dt_steps[i - 1]);
                            break;
                        }
                    }
                    char msg[64];
                    double dt = app->simulation->time_step;
                    if (dt >= 1e-3) {
                        snprintf(msg, sizeof(msg), "Time step: %.1f ms", dt * 1e3);
                    } else if (dt >= 1e-6) {
                        snprintf(msg, sizeof(msg), "Time step: %.1f us", dt * 1e6);
                    } else {
                        snprintf(msg, sizeof(msg), "Time step: %.0f ns", dt * 1e9);
                    }
                    ui_set_status(&app->ui, msg);
                }
                break;

            case UI_ACTION_TIMESTEP_AUTO:
                // Auto-adjust time step based on circuit's highest frequency
                {
                    double dt = simulation_auto_time_step(app->simulation);
                    char msg[64];
                    if (dt >= 1e-3) {
                        snprintf(msg, sizeof(msg), "Auto time step: %.1f ms", dt * 1e3);
                    } else if (dt >= 1e-6) {
                        snprintf(msg, sizeof(msg), "Auto time step: %.1f us", dt * 1e6);
                    } else {
                        snprintf(msg, sizeof(msg), "Auto time step: %.0f ns", dt * 1e9);
                    }
                    ui_set_status(&app->ui, msg);
                }
                break;

            case UI_ACTION_CREATE_SUBCIRCUIT:
                // Create or edit subcircuit
                {
                    // Check if we're editing an existing subcircuit
                    if (app->ui.subcircuit_editing_def_id >= 0) {
                        // Find and update existing definition
                        SubCircuitDef *def = NULL;
                        for (int i = 0; i < g_subcircuit_library.count; i++) {
                            if (g_subcircuit_library.defs[i].id == app->ui.subcircuit_editing_def_id) {
                                def = &g_subcircuit_library.defs[i];
                                break;
                            }
                        }
                        if (def) {
                            // Update name
                            strncpy(def->name, app->ui.subcircuit_name, sizeof(def->name) - 1);
                            def->name[sizeof(def->name) - 1] = '\0';

                            // Update pin names (keep same number of pins)
                            for (int i = 0; i < def->num_pins && i < 16; i++) {
                                strncpy(def->pins[i].name, app->ui.subcircuit_pin_names[i],
                                        sizeof(def->pins[i].name) - 1);
                                def->pins[i].name[sizeof(def->pins[i].name) - 1] = '\0';
                            }

                            char msg[128];
                            snprintf(msg, sizeof(msg), "Updated subcircuit '%s'", def->name);
                            ui_set_status(&app->ui, msg);
                        }
                        ui_subcircuit_dialog_close(&app->ui);
                        break;
                    }

                    // Creating new subcircuit from selected components
                    if (g_subcircuit_library.count >= MAX_SUBCIRCUIT_DEFS) {
                        ui_set_status(&app->ui, "Subcircuit library is full");
                        ui_subcircuit_dialog_close(&app->ui);
                        break;
                    }

                    // Get new definition slot
                    SubCircuitDef *def = &g_subcircuit_library.defs[g_subcircuit_library.count];
                    memset(def, 0, sizeof(SubCircuitDef));

                    // Set ID and name
                    def->id = g_subcircuit_library.next_id++;
                    strncpy(def->name, app->ui.subcircuit_name, sizeof(def->name) - 1);
                    def->name[sizeof(def->name) - 1] = '\0';
                    snprintf(def->description, sizeof(def->description), "User-created subcircuit");

                    // Count selected components and pins
                    int num_selected = 0;
                    int num_pins = 0;
                    float min_x = 1e9, min_y = 1e9, max_x = -1e9, max_y = -1e9;

                    for (int i = 0; i < app->circuit->num_components; i++) {
                        Component *c = app->circuit->components[i];
                        if (c && c->selected) {
                            if (c->type == COMP_PIN) {
                                // This is a pin marker
                                if (num_pins < MAX_SUBCIRCUIT_PINS) {
                                    // Use the pin name if set, otherwise generate "P1", "P2", etc.
                                    if (c->props.pin.pin_name[0] != '\0') {
                                        strncpy(def->pins[num_pins].name, c->props.pin.pin_name,
                                                sizeof(def->pins[num_pins].name) - 1);
                                    } else {
                                        snprintf(def->pins[num_pins].name, sizeof(def->pins[num_pins].name),
                                                "P%d", num_pins + 1);
                                    }
                                    def->pins[num_pins].name[sizeof(def->pins[num_pins].name) - 1] = '\0';
                                    def->pins[num_pins].internal_node_id = c->node_ids[0];
                                    def->pins[num_pins].side = (num_pins < 8) ? 0 : 1;  // left or right
                                    def->pins[num_pins].position = num_pins % 8;
                                    num_pins++;
                                }
                            } else {
                                num_selected++;
                            }
                            // Update bounding box
                            if (c->x < min_x) min_x = c->x;
                            if (c->y < min_y) min_y = c->y;
                            if (c->x > max_x) max_x = c->x;
                            if (c->y > max_y) max_y = c->y;
                        }
                    }

                    if (num_selected == 0) {
                        ui_set_status(&app->ui, "No components selected for subcircuit");
                        ui_subcircuit_dialog_close(&app->ui);
                        break;
                    }

                    // If no explicit PIN components, auto-generate default pins
                    if (num_pins == 0) {
                        // Create 4 default pins (2 left, 2 right)
                        for (int p = 0; p < 4 && p < MAX_SUBCIRCUIT_PINS; p++) {
                            snprintf(def->pins[p].name, sizeof(def->pins[p].name), "%d", p + 1);
                            def->pins[p].internal_node_id = -1;  // Not connected internally
                            def->pins[p].side = (p < 2) ? 0 : 1;  // First 2 on left, next 2 on right
                            def->pins[p].position = p % 2;
                        }
                        num_pins = 4;
                    }

                    // Apply user-edited pin names from dialog (overrides defaults)
                    for (int i = 0; i < num_pins && i < 16; i++) {
                        if (app->ui.subcircuit_pin_names[i][0] != '\0') {
                            strncpy(def->pins[i].name, app->ui.subcircuit_pin_names[i],
                                    sizeof(def->pins[i].name) - 1);
                            def->pins[i].name[sizeof(def->pins[i].name) - 1] = '\0';
                        }
                    }

                    def->num_components = num_selected;
                    def->num_pins = num_pins;
                    def->internal_width = (max_x - min_x) + 80;
                    def->internal_height = (max_y - min_y) + 80;
                    def->block_width = 80 + num_pins * 10;
                    def->block_height = 60 + num_pins * 10;

                    // Serialize selected components (simplified - store component pointers for now)
                    // In a full implementation, we'd serialize component data to allow save/load
                    size_t comp_size = num_selected * sizeof(Component);
                    def->component_data = malloc(comp_size);
                    def->component_data_size = comp_size;

                    if (def->component_data) {
                        Component *comp_arr = (Component *)def->component_data;
                        int idx = 0;
                        for (int i = 0; i < app->circuit->num_components && idx < num_selected; i++) {
                            Component *c = app->circuit->components[i];
                            if (c && c->selected && c->type != COMP_PIN) {
                                // Copy component data (excluding pointers)
                                memcpy(&comp_arr[idx], c, sizeof(Component));
                                // Offset positions relative to min corner
                                comp_arr[idx].x -= min_x - 40;
                                comp_arr[idx].y -= min_y - 40;
                                idx++;
                            }
                        }
                    }

                    // Increment library count
                    g_subcircuit_library.count++;

                    // Close dialog and show status
                    ui_subcircuit_dialog_close(&app->ui);

                    char msg[128];
                    snprintf(msg, sizeof(msg), "Created subcircuit '%s' with %d components, %d pins",
                             def->name, num_selected, num_pins);
                    ui_set_status(&app->ui, msg);
                }
                break;

            case UI_ACTION_PROP_APPLY:
                /* Naming a probe: the label is also the oscilloscope channel name, so a blank
                   entry puts it back to CHn rather than leaving the channel with no name. */
                if (app->input.editing_property && app->input.editing_prop_type == PROP_PROBE_NAME) {
                    int pi = app->input.selected_probe_idx;
                    if (pi >= 0 && pi < app->circuit->num_probes) {
                        Probe *pr = &app->circuit->probes[pi];
                        const char *t = app->input.input_buffer;
                        while (*t == ' ') t++;
                        if (!*t) snprintf(pr->label, sizeof pr->label, "CH%d", pr->channel_num + 1);
                        else { strncpy(pr->label, t, sizeof pr->label - 1); pr->label[sizeof pr->label - 1] = 0; }
                        char msg[64]; snprintf(msg, sizeof msg, "Probe named %s", pr->label);
                        ui_set_status(&app->ui, msg);
                        app->circuit->modified = true;
                    }
                    input_cancel_property_edit(&app->input);
                    break;
                }
                // Apply text-edited property value
                if (app->input.selected_component) {
                    if (input_apply_property_edit(&app->input, app->input.selected_component)) {
                        app_on_property_changed(app, app->input.selected_component);
                        ui_set_status(&app->ui, "Property updated");
                    } else {
                        ui_set_status(&app->ui, "Invalid value");
                    }
                }
                break;

            default:
                // Handle circuit template selection (UI_ACTION_SELECT_CIRCUIT + circuit_type)
                int sel_idx = 0;
                UIActionKind sel_kind = ui_action_kind(app->input.pending_ui_action, &sel_idx);
                if (sel_kind == UIA_SCOPE_CH) {
                    /* index 0 is the ALL chip; 1 + channel is one channel's own scale */
                    if (sel_idx == 0) {
                        app->ui.scope_scale_all = true;
                        ui_set_status(&app->ui, "V+ / V- and the wheel move every channel");
                    } else {
                        app->ui.scope_scale_all = false;
                        app->ui.scope_selected_channel = sel_idx - 1;
                        char m[96];
                        snprintf(m, sizeof m, "V+ / V- and the wheel move %s alone",
                                 ui_channel_name(&app->ui, sel_idx - 1));
                        ui_set_status(&app->ui, m);
                    }
                    app->ui.scope_capture_valid = false;
                }
                else if (sel_kind == UIA_CIRCUIT &&
                    sel_idx > CIRCUIT_NONE && sel_idx < CIRCUIT_TYPE_COUNT) {
                    /* Picking a circuit from the list places it, on its own, framed and running.
                       It used to arm a click: choose the circuit, find a clear patch of canvas,
                       click, and clear the last one by hand first. Nobody wants a template
                       *next to* another template - they want to look at it. */
                    int circuit_type = sel_idx;
                    const CircuitTemplateInfo *info = circuit_template_get_info(circuit_type);
                    char msg[160];
                    simulation_reset(app->simulation);
                    circuit_clear(app->circuit);
                    input_cancel_action(&app->input);
                    app->input.selected_component = NULL;
                    app->has_file = false;
                    app->current_file[0] = '\0';

                    if (app_place_template_centered(app, (CircuitTemplateType)circuit_type)) {
                        app->input.should_autostart_sim = true;
                        snprintf(msg, sizeof msg, "%s: probes on input and output, running - read the notes on the canvas",
                                 info ? info->name : "circuit");
                    } else {
                        snprintf(msg, sizeof msg, "Could not place %s", info ? info->name : "that circuit");
                    }
                    ui_set_status(&app->ui, msg);

                    /* the selection has done its job; leave nothing armed behind it */
                    app->ui.placing_circuit = false;
                    app->ui.selected_circuit_type = -1;
                    for (int i = 0; i < app->ui.num_circuit_items; i++)
                        app->ui.circuit_items[i].selected = false;
                }
                // Handle property edit start actions (UI_ACTION_PROP_EDIT + prop_type)
                else if (app->input.pending_ui_action >= UI_ACTION_PROP_EDIT &&
                    app->input.pending_ui_action < UI_ACTION_PROP_EDIT + 200) {
                    int prop_type = app->input.pending_ui_action - UI_ACTION_PROP_EDIT;
                    if (prop_type == PROP_PROBE_NAME) {
                        int pi = app->input.selected_probe_idx;
                        if (pi >= 0 && pi < app->circuit->num_probes && !app->input.editing_property) {
                            input_start_property_edit(&app->input, PROP_PROBE_NAME, app->circuit->probes[pi].label);
                            ui_set_status(&app->ui, "Type a name for this probe, Enter to apply (blank restores CHn)");
                        }
                        app->input.pending_ui_action = UI_ACTION_NONE;
                        break;
                    }
                    if (app->input.selected_component && !app->input.editing_property) {
                        // Get current value to show in edit field
                        char current_value[64] = "";
                        Component *c = app->input.selected_component;
                        if (prop_type == PROP_LINE_Z0) {
                            snprintf(current_value, sizeof current_value, "%.6g", c->props.delay_line.z0);
                        } else if (prop_type == PROP_LINE_DELAY) {
                            snprintf(current_value, sizeof current_value, "%.6g", c->props.delay_line.delay);
                        } else if (prop_type == PROP_VALUE || prop_type == PROP_AMPLITUDE) {
                            switch (c->type) {
                                case COMP_DC_VOLTAGE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.dc_voltage.voltage); break;
                                case COMP_AC_VOLTAGE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.ac_voltage.amplitude); break;
                                case COMP_DC_CURRENT: snprintf(current_value, sizeof(current_value), "%.6g", c->props.dc_current.current); break;
                                case COMP_RESISTOR: snprintf(current_value, sizeof(current_value), "%.6g", c->props.resistor.resistance); break;
                                case COMP_CAPACITOR: snprintf(current_value, sizeof(current_value), "%.6g", c->props.capacitor.capacitance); break;
                                case COMP_CAPACITOR_ELEC: snprintf(current_value, sizeof(current_value), "%.6g", c->props.capacitor_elec.capacitance); break;
                                case COMP_INDUCTOR: snprintf(current_value, sizeof(current_value), "%.6g", c->props.inductor.inductance); break;
                                case COMP_SQUARE_WAVE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.square_wave.amplitude); break;
                                case COMP_TRIANGLE_WAVE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.triangle_wave.amplitude); break;
                                case COMP_SAWTOOTH_WAVE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.sawtooth_wave.amplitude); break;
                                case COMP_NOISE_SOURCE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.noise_source.amplitude); break;
                                case COMP_LED: snprintf(current_value, sizeof(current_value), "%.0f", c->props.led.wavelength); break;
                                default: break;
                            }
                        } else if (prop_type == PROP_FREQUENCY) {
                            switch (c->type) {
                                case COMP_AC_VOLTAGE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.ac_voltage.frequency); break;
                                case COMP_SQUARE_WAVE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.square_wave.frequency); break;
                                case COMP_TRIANGLE_WAVE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.triangle_wave.frequency); break;
                                case COMP_SAWTOOTH_WAVE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.sawtooth_wave.frequency); break;
                                default: break;
                            }
                        } else if (prop_type == PROP_PHASE) {
                            switch (c->type) {
                                case COMP_AC_VOLTAGE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.ac_voltage.phase); break;
                                case COMP_SQUARE_WAVE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.square_wave.phase); break;
                                case COMP_TRIANGLE_WAVE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.triangle_wave.phase); break;
                                case COMP_SAWTOOTH_WAVE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.sawtooth_wave.phase); break;
                                default: break;
                            }
                        } else if (prop_type == PROP_OFFSET) {
                            switch (c->type) {
                                case COMP_AC_VOLTAGE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.ac_voltage.offset); break;
                                case COMP_SQUARE_WAVE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.square_wave.offset); break;
                                case COMP_TRIANGLE_WAVE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.triangle_wave.offset); break;
                                case COMP_SAWTOOTH_WAVE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.sawtooth_wave.offset); break;
                                default: break;
                            }
                        } else if (prop_type == PROP_DUTY) {
                            if (c->type == COMP_SQUARE_WAVE) {
                                snprintf(current_value, sizeof(current_value), "%.1f", c->props.square_wave.duty * 100);
                            }
                        }
                        // BJT parameters
                        else if (prop_type == PROP_BJT_BETA) {
                            if (c->type == COMP_NPN_BJT || c->type == COMP_PNP_BJT) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.bjt.bf);
                            }
                        } else if (prop_type == PROP_BJT_IS) {
                            if (c->type == COMP_NPN_BJT || c->type == COMP_PNP_BJT) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.bjt.is);
                            }
                        } else if (prop_type == PROP_BJT_VAF) {
                            if (c->type == COMP_NPN_BJT || c->type == COMP_PNP_BJT) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.bjt.vaf);
                            }
                        } else if (prop_type == PROP_BJT_IDEAL) {
                            // Ideal mode toggle - just toggle immediately, no text input
                            if (c->type == COMP_NPN_BJT || c->type == COMP_PNP_BJT) {
                                c->props.bjt.ideal = !c->props.bjt.ideal;
                                ui_set_status(&app->ui, c->props.bjt.ideal ? "BJT: Ideal model" : "BJT: SPICE model (Gummel-Poon)");
                            }
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;  // Don't start text edit for toggle
                        }
                        // MOSFET parameters
                        else if (prop_type == PROP_MOS_VTH) {
                            if (c->type == COMP_NMOS || c->type == COMP_PMOS) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.mosfet.vth);
                            }
                        } else if (prop_type == PROP_MOS_KP) {
                            if (c->type == COMP_NMOS || c->type == COMP_PMOS) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.mosfet.kp);
                            }
                        } else if (prop_type == PROP_MOS_W) {
                            if (c->type == COMP_NMOS || c->type == COMP_PMOS) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.mosfet.w);
                            }
                        } else if (prop_type == PROP_MOS_L) {
                            if (c->type == COMP_NMOS || c->type == COMP_PMOS) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.mosfet.l);
                            }
                        } else if (prop_type == PROP_MOS_WL) {
                            if (c->type == COMP_NMOS || c->type == COMP_PMOS) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.mosfet.w / c->props.mosfet.l);
                            }
                        } else if (prop_type == PROP_MOS_KN) {
                            if (c->type == COMP_NMOS || c->type == COMP_PMOS) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.mosfet.kp * c->props.mosfet.w / c->props.mosfet.l);
                            }
                        } else if (prop_type == PROP_MOS_LAMBDA) {
                            if (c->type == COMP_NMOS || c->type == COMP_PMOS) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.mosfet.lambda);
                            }
                        } else if (prop_type == PROP_MOS_TOX) {
                            if (c->type == COMP_NMOS || c->type == COMP_PMOS) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.mosfet.tox);
                            }
                        } else if (prop_type == PROP_CAP_VHALF) {
                            if (c->type == COMP_CAPACITOR) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.capacitor.v_half);
                            }
                        } else if (prop_type == PROP_PART) {
                            component_cycle_part(c);
                            if (c->part[0]) {
                                char msg[96];
                                snprintf(msg, sizeof msg, "Part: %s (datasheet model)", c->part);
                                ui_set_status(&app->ui, msg);
                            } else {
                                ui_set_status(&app->ui, "Part: generic (the component's own defaults)");
                            }
                            app->circuit->modified = true;
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;
                        } else if (prop_type == PROP_MOS_TYPE) {
                            // Enhancement <-> depletion: flip the sign of the threshold
                            if (c->type == COMP_NMOS || c->type == COMP_PMOS) {
                                c->props.mosfet.vth = -c->props.mosfet.vth;
                                bool depl = (c->type == COMP_NMOS) ? (c->props.mosfet.vth < 0) : (c->props.mosfet.vth > 0);
                                ui_set_status(&app->ui, depl ? "MOSFET: depletion mode (conducts at Vgs = 0)"
                                                             : "MOSFET: enhancement mode (off at Vgs = 0)");
                            }
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;
                        } else if (prop_type == PROP_MOS_IDEAL) {
                            // Ideal mode toggle - just toggle immediately, no text input
                            if (c->type == COMP_NMOS || c->type == COMP_PMOS) {
                                c->props.mosfet.ideal = !c->props.mosfet.ideal;
                                ui_set_status(&app->ui, c->props.mosfet.ideal ? "MOSFET: Ideal model" : "MOSFET: SPICE Level 1 model");
                            }
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;  // Don't start text edit for toggle
                        }
                        // LED parameters
                        else if (prop_type == PROP_LED_COLOR) {
                            // Color selector - cycle through LED colors
                            if (c->type == COMP_LED) {
                                const char *color_names[] = {"IR", "Red", "Orange", "Yellow", "Green", "Emerald", "Blue", "White", "UV"};

                                // Cycle to next color (0-8, 9 total colors)
                                c->props.led.color = (c->props.led.color + 1) % LED_COLOR_COUNT;

                                // Update all LED parameters based on new color
                                component_update_led_color(c);

                                char msg[64];
                                snprintf(msg, sizeof(msg), "LED Color: %s (%.0f nm, Vf=%.1fV)",
                                         color_names[c->props.led.color], c->props.led.wavelength, c->props.led.vf);
                                ui_set_status(&app->ui, msg);
                            }
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;  // Don't start text edit for color selector
                        }
                        else if (prop_type == PROP_LED_ARRAY_COLOR) {
                            // LED Array color selector - cycle through LED colors
                            if (c->type == COMP_LED_ARRAY) {
                                const char *color_names[] = {"IR", "Red", "Orange", "Yellow", "Green", "Emerald", "Blue", "White", "UV"};

                                // Cycle to next color (0-8, 9 total colors)
                                c->props.led_array.color = (c->props.led_array.color + 1) % LED_COLOR_COUNT;

                                // Update all LED parameters based on new color
                                component_update_led_color(c);

                                char msg[64];
                                snprintf(msg, sizeof(msg), "LED Array Color: %s (Vf=%.1fV)",
                                         color_names[c->props.led_array.color], c->props.led_array.vf);
                                ui_set_status(&app->ui, msg);
                            }
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;  // Don't start text edit for color selector
                        } else if (prop_type == PROP_LED_VF) {
                            if (c->type == COMP_LED) {
                                snprintf(current_value, sizeof(current_value), "%.2f", c->props.led.vf);
                            } else if (c->type == COMP_SCHOTTKY) {
                                snprintf(current_value, sizeof(current_value), "%.2f", c->props.schottky.vf);
                            }
                        } else if (prop_type == PROP_LED_IMAX) {
                            if (c->type == COMP_LED) {
                                // Display in mA
                                snprintf(current_value, sizeof(current_value), "%.0f", c->props.led.max_current * 1000);
                            }
                        }
                        // Generic ideal mode toggle for all components
                        else if (prop_type == PROP_IDEAL) {
                            const char *model_name = "Real";
                            switch (c->type) {
                                case COMP_DC_VOLTAGE:
                                    c->props.dc_voltage.ideal = !c->props.dc_voltage.ideal;
                                    model_name = c->props.dc_voltage.ideal ? "Ideal" : "Real (series R)";
                                    break;
                                case COMP_AC_VOLTAGE:
                                    c->props.ac_voltage.ideal = !c->props.ac_voltage.ideal;
                                    model_name = c->props.ac_voltage.ideal ? "Ideal" : "Real (series R)";
                                    break;
                                case COMP_DC_CURRENT:
                                    c->props.dc_current.ideal = !c->props.dc_current.ideal;
                                    model_name = c->props.dc_current.ideal ? "Ideal" : "Real (parallel R)";
                                    break;
                                case COMP_RESISTOR:
                                    c->props.resistor.ideal = !c->props.resistor.ideal;
                                    model_name = c->props.resistor.ideal ? "Ideal" : "Real (temp coeff)";
                                    break;
                                case COMP_CAPACITOR:
                                    c->props.capacitor.ideal = !c->props.capacitor.ideal;
                                    model_name = c->props.capacitor.ideal ? "Ideal" : "Real (ESR, leakage)";
                                    break;
                                case COMP_CAPACITOR_ELEC:
                                    c->props.capacitor_elec.ideal = !c->props.capacitor_elec.ideal;
                                    model_name = c->props.capacitor_elec.ideal ? "Ideal" : "Real (ESR)";
                                    break;
                                case COMP_INDUCTOR:
                                    c->props.inductor.ideal = !c->props.inductor.ideal;
                                    model_name = c->props.inductor.ideal ? "Ideal" : "Real (DCR)";
                                    break;
                                case COMP_DIODE:
                                    c->props.diode.ideal = !c->props.diode.ideal;
                                    model_name = c->props.diode.ideal ? "Ideal (0.7V drop)" : "Real (Shockley)";
                                    break;
                                case COMP_ZENER:
                                    c->props.zener.ideal = !c->props.zener.ideal;
                                    model_name = c->props.zener.ideal ? "Ideal" : "Real (Zener R)";
                                    break;
                                case COMP_SCHOTTKY:
                                    c->props.schottky.ideal = !c->props.schottky.ideal;
                                    model_name = c->props.schottky.ideal ? "Ideal (0.3V drop)" : "Real (Shockley)";
                                    break;
                                case COMP_LED:
                                    c->props.led.ideal = !c->props.led.ideal;
                                    model_name = c->props.led.ideal ? "Ideal (fixed Vf)" : "Real (Shockley)";
                                    break;
                                case COMP_FUSE:
                                    c->props.fuse.ideal = !c->props.fuse.ideal;
                                    model_name = c->props.fuse.ideal ? "Ideal (instant)" : "Real (i2t)";
                                    break;
                                default: break;
                            }
                            char msg[64];
                            snprintf(msg, sizeof(msg), "Model: %s", model_name);
                            ui_set_status(&app->ui, msg);
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;  // Don't start text edit for toggle
                        }
                        // Fuse reset
                        else if (prop_type == PROP_RESET_FUSE) {
                            if (c->type == COMP_FUSE) {
                                c->props.fuse.blown = false;
                                c->props.fuse.i2t_accumulated = 0.0;
                                c->props.fuse.blow_time = -1.0;
                                c->props.fuse.current = 0.0;
                                ui_set_status(&app->ui, "Fuse reset");
                            }
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;  // Don't start text edit for toggle
                        }
                        // Source internal resistance
                        else if (prop_type == PROP_R_SERIES) {
                            if (c->type == COMP_DC_VOLTAGE) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.dc_voltage.r_series);
                            } else if (c->type == COMP_AC_VOLTAGE) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.ac_voltage.r_series);
                            }
                        }
                        else if (prop_type == PROP_R_PARALLEL) {
                            if (c->type == COMP_DC_CURRENT) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.dc_current.r_parallel);
                            }
                        }
                        // Resistor temp coefficient
                        else if (prop_type == PROP_TEMP_COEFF) {
                            if (c->type == COMP_RESISTOR) {
                                snprintf(current_value, sizeof(current_value), "%.0f", c->props.resistor.temp_coeff);
                            }
                        }
                        // Capacitor ESR
                        else if (prop_type == PROP_ESR) {
                            if (c->type == COMP_CAPACITOR) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.capacitor.esr);
                            } else if (c->type == COMP_CAPACITOR_ELEC) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.capacitor_elec.esr);
                            }
                        }
                        // Inductor DCR
                        else if (prop_type == PROP_DCR) {
                            if (c->type == COMP_INDUCTOR) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.inductor.dcr);
                            }
                        }
                        // Diode breakdown voltage
                        else if (prop_type == PROP_BV) {
                            if (c->type == COMP_DIODE) {
                                snprintf(current_value, sizeof(current_value), "%.1f", c->props.diode.bv);
                            }
                        }
                        // Zener voltage and impedance
                        else if (prop_type == PROP_VZ) {
                            if (c->type == COMP_ZENER) {
                                snprintf(current_value, sizeof(current_value), "%.2f", c->props.zener.vz);
                            }
                        }
                        else if (prop_type == PROP_RZ) {
                            if (c->type == COMP_ZENER) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.zener.rz);
                            }
                        }
                        // Electrolytic cap max voltage
                        else if (prop_type == PROP_MAX_VOLTAGE) {
                            if (c->type == COMP_CAPACITOR_ELEC) {
                                snprintf(current_value, sizeof(current_value), "%.1f", c->props.capacitor_elec.max_voltage);
                            }
                        }
                        // Op-Amp parameters
                        else if (prop_type == PROP_OPAMP_GAIN) {
                            if (c->type == COMP_OPAMP) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.opamp.gain);
                            }
                        }
                        else if (prop_type == PROP_OPAMP_GBW) {
                            if (c->type == COMP_OPAMP) {
                                snprintf(current_value, sizeof(current_value), "%.6g", c->props.opamp.gbw);
                            }
                        }
                        else if (prop_type == PROP_OPAMP_SLEW) {
                            if (c->type == COMP_OPAMP) {
                                snprintf(current_value, sizeof(current_value), "%.2f", c->props.opamp.slew_rate);
                            }
                        }
                        else if (prop_type == PROP_OPAMP_VMAX) {
                            if (c->type == COMP_OPAMP) {
                                snprintf(current_value, sizeof(current_value), "%.1f", c->props.opamp.vmax);
                            }
                        }
                        else if (prop_type == PROP_OPAMP_VMIN) {
                            if (c->type == COMP_OPAMP) {
                                snprintf(current_value, sizeof(current_value), "%.1f", c->props.opamp.vmin);
                            }
                        }
                        else if (prop_type == PROP_OPAMP_IDEAL) {
                            if (c->type == COMP_OPAMP) {
                                c->props.opamp.ideal = !c->props.opamp.ideal;
                                ui_set_status(&app->ui, c->props.opamp.ideal ? "Op-Amp: Ideal model" : "Op-Amp: Real model (GBW, slew rate)");
                            }
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;
                        }
                        else if (prop_type == PROP_OPAMP_R2R) {
                            if (c->type == COMP_OPAMP) {
                                c->props.opamp.rail_to_rail = !c->props.opamp.rail_to_rail;
                                ui_set_status(&app->ui, c->props.opamp.rail_to_rail ? "Op-Amp: Rail-to-Rail enabled" : "Op-Amp: Rail-to-Rail disabled");
                            }
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;
                        }
                        // Sweep enable toggles
                        else if (prop_type == PROP_SWEEP_VOLTAGE_ENABLE) {
                            SweepConfig *sweep = NULL;
                            if (c->type == COMP_DC_VOLTAGE) sweep = &c->props.dc_voltage.voltage_sweep;
                            else if (c->type == COMP_DC_CURRENT) sweep = &c->props.dc_current.current_sweep;
                            if (sweep) {
                                sweep->enabled = !sweep->enabled;
                                if (sweep->enabled && sweep->sweep_time <= 0) {
                                    sweep->sweep_time = 1.0;  // Default 1 second
                                    sweep->mode = SWEEP_LINEAR;
                                    sweep->num_steps = 10;
                                }
                                ui_set_status(&app->ui, sweep->enabled ? "Voltage/Current sweep enabled" : "Voltage/Current sweep disabled");
                            }
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;
                        }
                        else if (prop_type == PROP_SWEEP_AMP_ENABLE) {
                            SweepConfig *sweep = NULL;
                            if (c->type == COMP_AC_VOLTAGE) sweep = &c->props.ac_voltage.amplitude_sweep;
                            else if (c->type == COMP_SQUARE_WAVE) sweep = &c->props.square_wave.amplitude_sweep;
                            else if (c->type == COMP_TRIANGLE_WAVE) sweep = &c->props.triangle_wave.amplitude_sweep;
                            else if (c->type == COMP_SAWTOOTH_WAVE) sweep = &c->props.sawtooth_wave.amplitude_sweep;
                            else if (c->type == COMP_NOISE_SOURCE) sweep = &c->props.noise_source.amplitude_sweep;
                            if (sweep) {
                                sweep->enabled = !sweep->enabled;
                                if (sweep->enabled && sweep->sweep_time <= 0) {
                                    sweep->sweep_time = 1.0;
                                    sweep->mode = SWEEP_LINEAR;
                                    sweep->num_steps = 10;
                                }
                                ui_set_status(&app->ui, sweep->enabled ? "Amplitude sweep enabled" : "Amplitude sweep disabled");
                            }
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;
                        }
                        else if (prop_type == PROP_SWEEP_FREQ_ENABLE) {
                            SweepConfig *sweep = NULL;
                            if (c->type == COMP_AC_VOLTAGE) sweep = &c->props.ac_voltage.frequency_sweep;
                            else if (c->type == COMP_SQUARE_WAVE) sweep = &c->props.square_wave.frequency_sweep;
                            else if (c->type == COMP_TRIANGLE_WAVE) sweep = &c->props.triangle_wave.frequency_sweep;
                            else if (c->type == COMP_SAWTOOTH_WAVE) sweep = &c->props.sawtooth_wave.frequency_sweep;
                            if (sweep) {
                                sweep->enabled = !sweep->enabled;
                                if (sweep->enabled && sweep->sweep_time <= 0) {
                                    sweep->sweep_time = 1.0;
                                    sweep->mode = SWEEP_LOG;  // Log is better for frequency
                                    sweep->num_steps = 10;
                                }
                                ui_set_status(&app->ui, sweep->enabled ? "Frequency sweep enabled" : "Frequency sweep disabled");
                            }
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;
                        }
                        // Sweep mode cycling
                        else if (prop_type == PROP_SWEEP_VOLTAGE_MODE || prop_type == PROP_SWEEP_AMP_MODE || prop_type == PROP_SWEEP_FREQ_MODE) {
                            SweepConfig *sweep = NULL;
                            if (prop_type == PROP_SWEEP_VOLTAGE_MODE) {
                                if (c->type == COMP_DC_VOLTAGE) sweep = &c->props.dc_voltage.voltage_sweep;
                                else if (c->type == COMP_DC_CURRENT) sweep = &c->props.dc_current.current_sweep;
                            } else if (prop_type == PROP_SWEEP_AMP_MODE) {
                                if (c->type == COMP_AC_VOLTAGE) sweep = &c->props.ac_voltage.amplitude_sweep;
                                else if (c->type == COMP_SQUARE_WAVE) sweep = &c->props.square_wave.amplitude_sweep;
                                else if (c->type == COMP_TRIANGLE_WAVE) sweep = &c->props.triangle_wave.amplitude_sweep;
                                else if (c->type == COMP_SAWTOOTH_WAVE) sweep = &c->props.sawtooth_wave.amplitude_sweep;
                                else if (c->type == COMP_NOISE_SOURCE) sweep = &c->props.noise_source.amplitude_sweep;
                            } else {
                                if (c->type == COMP_AC_VOLTAGE) sweep = &c->props.ac_voltage.frequency_sweep;
                                else if (c->type == COMP_SQUARE_WAVE) sweep = &c->props.square_wave.frequency_sweep;
                                else if (c->type == COMP_TRIANGLE_WAVE) sweep = &c->props.triangle_wave.frequency_sweep;
                                else if (c->type == COMP_SAWTOOTH_WAVE) sweep = &c->props.sawtooth_wave.frequency_sweep;
                            }
                            if (sweep) {
                                sweep->mode = (sweep->mode + 1) % 4;
                                if (sweep->mode == SWEEP_NONE) sweep->mode = SWEEP_LINEAR;
                                const char *mode_names[] = {"None", "Linear", "Log", "Step"};
                                char msg[64];
                                snprintf(msg, sizeof(msg), "Sweep mode: %s", mode_names[sweep->mode]);
                                ui_set_status(&app->ui, msg);
                            }
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;
                        }
                        // Sweep repeat toggle
                        else if (prop_type == PROP_SWEEP_VOLTAGE_REPEAT || prop_type == PROP_SWEEP_AMP_REPEAT || prop_type == PROP_SWEEP_FREQ_REPEAT) {
                            SweepConfig *sweep = NULL;
                            if (prop_type == PROP_SWEEP_VOLTAGE_REPEAT) {
                                if (c->type == COMP_DC_VOLTAGE) sweep = &c->props.dc_voltage.voltage_sweep;
                                else if (c->type == COMP_DC_CURRENT) sweep = &c->props.dc_current.current_sweep;
                            } else if (prop_type == PROP_SWEEP_AMP_REPEAT) {
                                if (c->type == COMP_AC_VOLTAGE) sweep = &c->props.ac_voltage.amplitude_sweep;
                                else if (c->type == COMP_SQUARE_WAVE) sweep = &c->props.square_wave.amplitude_sweep;
                                else if (c->type == COMP_TRIANGLE_WAVE) sweep = &c->props.triangle_wave.amplitude_sweep;
                                else if (c->type == COMP_SAWTOOTH_WAVE) sweep = &c->props.sawtooth_wave.amplitude_sweep;
                                else if (c->type == COMP_NOISE_SOURCE) sweep = &c->props.noise_source.amplitude_sweep;
                            } else {
                                if (c->type == COMP_AC_VOLTAGE) sweep = &c->props.ac_voltage.frequency_sweep;
                                else if (c->type == COMP_SQUARE_WAVE) sweep = &c->props.square_wave.frequency_sweep;
                                else if (c->type == COMP_TRIANGLE_WAVE) sweep = &c->props.triangle_wave.frequency_sweep;
                                else if (c->type == COMP_SAWTOOTH_WAVE) sweep = &c->props.sawtooth_wave.frequency_sweep;
                            }
                            if (sweep) {
                                sweep->repeat = !sweep->repeat;
                                ui_set_status(&app->ui, sweep->repeat ? "Sweep repeat: ON" : "Sweep repeat: OFF");
                            }
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;
                        }
                        // Sweep value edits (start, end, time, steps)
                        else if (prop_type >= PROP_SWEEP_VOLTAGE_START && prop_type <= PROP_SWEEP_FREQ_REPEAT) {
                            SweepConfig *sweep = NULL;
                            int base_prop = 0;
                            if (prop_type >= PROP_SWEEP_VOLTAGE_START && prop_type <= PROP_SWEEP_VOLTAGE_REPEAT) {
                                base_prop = PROP_SWEEP_VOLTAGE_START;
                                if (c->type == COMP_DC_VOLTAGE) sweep = &c->props.dc_voltage.voltage_sweep;
                                else if (c->type == COMP_DC_CURRENT) sweep = &c->props.dc_current.current_sweep;
                            } else if (prop_type >= PROP_SWEEP_AMP_START && prop_type <= PROP_SWEEP_AMP_REPEAT) {
                                base_prop = PROP_SWEEP_AMP_START;
                                if (c->type == COMP_AC_VOLTAGE) sweep = &c->props.ac_voltage.amplitude_sweep;
                                else if (c->type == COMP_SQUARE_WAVE) sweep = &c->props.square_wave.amplitude_sweep;
                                else if (c->type == COMP_TRIANGLE_WAVE) sweep = &c->props.triangle_wave.amplitude_sweep;
                                else if (c->type == COMP_SAWTOOTH_WAVE) sweep = &c->props.sawtooth_wave.amplitude_sweep;
                                else if (c->type == COMP_NOISE_SOURCE) sweep = &c->props.noise_source.amplitude_sweep;
                            } else if (prop_type >= PROP_SWEEP_FREQ_START && prop_type <= PROP_SWEEP_FREQ_REPEAT) {
                                base_prop = PROP_SWEEP_FREQ_START;
                                if (c->type == COMP_AC_VOLTAGE) sweep = &c->props.ac_voltage.frequency_sweep;
                                else if (c->type == COMP_SQUARE_WAVE) sweep = &c->props.square_wave.frequency_sweep;
                                else if (c->type == COMP_TRIANGLE_WAVE) sweep = &c->props.triangle_wave.frequency_sweep;
                                else if (c->type == COMP_SAWTOOTH_WAVE) sweep = &c->props.sawtooth_wave.frequency_sweep;
                            }
                            if (sweep) {
                                int offset = prop_type - base_prop;
                                if (offset == 0) snprintf(current_value, sizeof(current_value), "%.6g", sweep->start_value);
                                else if (offset == 1) snprintf(current_value, sizeof(current_value), "%.6g", sweep->end_value);
                                else if (offset == 2) snprintf(current_value, sizeof(current_value), "%.6g", sweep->sweep_time);
                                else if (offset == 3) snprintf(current_value, sizeof(current_value), "%d", sweep->num_steps);
                            }
                        }
                        // Text annotation properties
                        else if (prop_type == PROP_TEXT_CONTENT) {
                            if (c->type == COMP_TEXT) {
                                snprintf(current_value, sizeof(current_value), "%s", c->props.text.text);
                            }
                        }
                        else if (prop_type == PROP_TEXT_SIZE) {
                            // Toggle immediately, don't open text editor
                            if (c->type == COMP_TEXT) {
                                c->props.text.font_size = (c->props.text.font_size % 3) + 1;  // Cycle 1->2->3->1
                                const char *sizes[] = {"Small", "Normal", "Large"};
                                char msg[64];
                                snprintf(msg, sizeof(msg), "Text size: %s", sizes[c->props.text.font_size - 1]);
                                ui_set_status(&app->ui, msg);
                            }
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;
                        }
                        else if (prop_type == PROP_TEXT_BOLD) {
                            // Toggle bold immediately
                            if (c->type == COMP_TEXT) {
                                c->props.text.bold = !c->props.text.bold;
                                ui_set_status(&app->ui, c->props.text.bold ? "Text: Bold ON" : "Text: Bold OFF");
                            }
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;
                        }
                        else if (prop_type == PROP_TEXT_ITALIC) {
                            // Toggle italic immediately
                            if (c->type == COMP_TEXT) {
                                c->props.text.italic = !c->props.text.italic;
                                ui_set_status(&app->ui, c->props.text.italic ? "Text: Italic ON" : "Text: Italic OFF");
                            }
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;
                        }
                        else if (prop_type == PROP_TEXT_UNDERLINE) {
                            // Toggle underline immediately
                            if (c->type == COMP_TEXT) {
                                c->props.text.underline = !c->props.text.underline;
                                ui_set_status(&app->ui, c->props.text.underline ? "Text: Underline ON" : "Text: Underline OFF");
                            }
                            app->input.pending_ui_action = UI_ACTION_NONE;
                            break;
                        }
                        input_start_property_edit(&app->input, prop_type, current_value);
                        ui_set_status(&app->ui, "Type value (use k,M,m,u,n,p suffix), Enter to apply");
                    } else if (!app->input.selected_component) {
                        ui_set_status(&app->ui, "Select a component first");
                    } else if (app->input.editing_property) {
                        ui_set_status(&app->ui, "Press Enter to apply or Escape to cancel");
                    }
                }
                break;
        }
        app->input.pending_ui_action = UI_ACTION_NONE;
    }

    // Update oscilloscope channels from probes
    ui_update_scope_channels(&app->ui, app->circuit);
}

// Match the simulation time step to the scope's time/div.
// Target ~50 samples per division (so a 10-division sweep has ~500 points), never coarser
// than the signal-accuracy step from simulation_auto_time_step(), snapped down to the
// 1-2-5 series and clamped to [MIN_TIME_STEP, MAX_TIME_STEP]. Runs only when time/div
// changes, so the manual dt buttons still work in between.
static void app_sync_time_step_to_scope(App *app) {
    if (!app || !app->simulation) return;
    double time_div = app->ui.scope_time_div;
    if (time_div <= 0 || time_div == app->synced_time_div) return;
    app->synced_time_div = time_div;

    double dt = simulation_scope_time_step(app->simulation, time_div);
    simulation_set_time_step(app->simulation, dt);
    char msg[96];
    snprintf(msg, sizeof(msg), "Time step matched to scope: dt = %.3g s", dt);
    ui_set_status(&app->ui, msg);
}

void app_update(App *app) {
    uint32_t current_time = SDL_GetTicks();
    float delta_time = (current_time - app->last_frame_time) / 1000.0f;
    app->last_frame_time = current_time;

    // Update FPS counter
    app->frame_count++;
    static uint32_t fps_timer = 0;
    if (current_time - fps_timer >= 1000) {
        app->fps = app->frame_count;
        app->frame_count = 0;
        fps_timer = current_time;
    }

    /* Auto-pause when the STRUCTURE changes under a running simulation - the node map and the
       matrix no longer describe the circuit. Editing a value is not that: the stamps pick the
       new number up on the next step, so turning a knob while it runs is the whole point of a
       simulator and used to stop it dead (and lose the operating point with it).
       Exception: a sweep changes values during the run by design. */
    if (app->simulation->state == SIM_RUNNING && app->circuit->topology_dirty &&
        !circuit_has_active_sweep(app->circuit)) {
        simulation_pause(app->simulation);
        ui_set_status(&app->ui, "Circuit changed - simulation paused");
    }

    // Check for frequency sweep thread completion
    if (app->freq_sweep_thread_running && app->simulation) {
        if (!app->simulation->freq_sweep_running) {
            // Thread has finished - wait for it to clean up
            SDL_WaitThread(app->freq_sweep_thread, NULL);
            app->freq_sweep_thread = NULL;
            app->freq_sweep_thread_running = false;

            if (g_sweep_data.success) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Frequency sweep complete: %d points",
                    app->simulation->freq_response_count);
                ui_set_status(&app->ui, msg);
            } else if (!app->simulation->freq_sweep_cancel) {
                ui_set_status(&app->ui, simulation_get_error(app->simulation));
            } else {
                ui_set_status(&app->ui, "Frequency sweep cancelled");
            }
        } else {
            // Update progress in status bar
            char msg[64];
            snprintf(msg, sizeof(msg), "Frequency sweep: %d/%d points...",
                app->simulation->freq_sweep_progress + 1,
                app->simulation->freq_sweep_total);
            ui_set_status(&app->ui, msg);
        }
    }

    // Run Monte Carlo analysis if active
    if (app->analysis.monte_carlo.active && !app->analysis.monte_carlo.complete) {
        // Run a few MC iterations per frame to keep UI responsive
        for (int i = 0; i < 5; i++) {
            bool done = analysis_monte_carlo_step(&app->analysis, app->circuit,
                                                   app->simulation, 0, &g_mc_backup);
            if (done) {
                // Restore original component values
                analysis_mc_restore_values(app->circuit, &g_mc_backup);

                // Update status with results
                char msg[128];
                snprintf(msg, sizeof(msg), "MC complete: Mean=%.3fV, StdDev=%.3fV, Range=[%.3f, %.3f]V",
                    app->analysis.monte_carlo.mean,
                    app->analysis.monte_carlo.std_dev,
                    app->analysis.monte_carlo.min_val,
                    app->analysis.monte_carlo.max_val);
                ui_set_status(&app->ui, msg);
                break;
            }
        }

        // Update progress in status bar
        if (!app->analysis.monte_carlo.complete) {
            char msg[64];
            snprintf(msg, sizeof(msg), "Monte Carlo: %d/%d runs...",
                app->analysis.monte_carlo.current_run,
                app->analysis.monte_carlo.num_runs);
            ui_set_status(&app->ui, msg);
        }
    }

    // Sweep tracking: follow the fastest sweeping source so ~3 cycles fill the screen.
    // Only re-snaps when the ideal 1-2-5 value changes, so the display does not flicker.
    if (app->ui.scope_track_sweep && app->simulation->state == SIM_RUNNING) {
        double fmax = 0;
        for (int i = 0; i < app->circuit->num_components; i++) {
            Component *c = app->circuit->components[i];
            if (c->type == COMP_AC_VOLTAGE && c->props.ac_voltage.frequency_sweep.enabled) {
                double f = sweep_get_value(&c->props.ac_voltage.frequency_sweep,
                                           c->props.ac_voltage.frequency, app->simulation->time);
                if (f > fmax) fmax = f;
            }
        }
        if (fmax > 0) {
            double ideal = 0.3 / fmax;                  // 3 cycles across 10 divisions
            double decade = pow(10.0, floor(log10(ideal)));
            double m = ideal / decade;
            double snapped = (m >= 5.0) ? 5.0 : (m >= 2.0) ? 2.0 : 1.0;
            double td = snapped * decade;
            if (td < 10e-9) td = 10e-9;
            if (td > 100.0) td = 100.0;
            if (fabs(td - app->ui.scope_time_div) > 1e-12 * td) {
                app->ui.scope_time_div = td;
                app->ui.scope_capture_valid = false;
            }
        }
    }

    // One-shot V/div from the measured signal range (after a template auto-starts)
    if (app->ui.scope_auto_vdiv_pending && app->simulation->state == SIM_RUNNING &&
        app->simulation->history_count >= 200) {
        static double t_buf[MAX_HISTORY], v_buf[MAX_HISTORY];
        double vmax = 0, span = 0;
        double ch_min[MAX_PROBES], ch_max[MAX_PROBES]; int nprobes = 0;
        for (int pi = 0; pi < app->circuit->num_probes && pi < MAX_PROBES; pi++) {
            int n = simulation_get_history(app->simulation, pi, t_buf, v_buf, MAX_HISTORY);
            ch_min[pi] = 1e300; ch_max[pi] = -1e300; nprobes = pi + 1;
            if (n > 1) span = t_buf[n - 1] - t_buf[0];
            for (int i = 0; i < n; i++) {
                if (fabs(v_buf[i]) > vmax) vmax = fabs(v_buf[i]);
                if (v_buf[i] < ch_min[pi]) ch_min[pi] = v_buf[i];
                if (v_buf[i] > ch_max[pi]) ch_max[pi] = v_buf[i];
            }
        }
        // Sources: never choose a scale that clips the drive signal itself
        for (int i = 0; i < app->circuit->num_components; i++) {
            Component *c = app->circuit->components[i];
            double a = 0;
            if (c->type == COMP_AC_VOLTAGE) {
                double amp = c->props.ac_voltage.amplitude;
                if (c->props.ac_voltage.amplitude_sweep.enabled) {
                    double e = c->props.ac_voltage.amplitude_sweep.end_value, st = c->props.ac_voltage.amplitude_sweep.start_value;
                    amp = (e > st) ? e : st;
                }
                a = fabs(amp) + fabs(c->props.ac_voltage.offset);
            } else if (c->type == COMP_DC_VOLTAGE) a = fabs(c->props.dc_voltage.voltage);
            else if (c->type == COMP_SQUARE_WAVE) a = fabs(c->props.square_wave.amplitude) + fabs(c->props.square_wave.offset);
            else if (c->type == COMP_TRIANGLE_WAVE) a = fabs(c->props.triangle_wave.amplitude);
            if (a > vmax) vmax = a;
        }
        // Wait until the history covers a full screen (so a slow signal has shown its peak)
        if (span >= 10.0 * app->ui.scope_time_div || app->simulation->time > 0.25) {
            double ideal = vmax * 1.15 / 4.0;          // fill ~4 of the 4 divisions above/below centre
            double decade = pow(10.0, floor(log10(ideal)));
            double m = ideal / decade;
            double snapped = (m > 5.0) ? 10.0 : (m > 2.0) ? 5.0 : (m > 1.0) ? 2.0 : 1.0;
            double vd = snapped * decade;
            if (vd < 0.001) vd = 0.001;
            if (vd > 500e3) vd = 500e3;
            app->ui.scope_volt_div = vd;
            app->ui.scope_auto_vdiv_pending = false;
            // Trigger from real data: a 0 V level never fires on rectified / pulsed / DC-offset
            // outputs, so the display free-runs and jitters. Prefer the current trigger channel
            // if it actually swings, else the channel with the largest swing; level = mid-range.
            {
                int best = -1; double best_swing = 0;
                for (int pi = 0; pi < nprobes; pi++) {
                    if (ch_max[pi] < ch_min[pi]) continue;
                    double sw = ch_max[pi] - ch_min[pi];
                    if (sw > best_swing) { best_swing = sw; best = pi; }
                }
                int tc = app->ui.trigger_channel;
                if (tc >= 0 && tc < nprobes && ch_max[tc] >= ch_min[tc] && (ch_max[tc] - ch_min[tc]) > 0.1 * best_swing && (ch_max[tc] - ch_min[tc]) > 1e-6) best = tc;
                if (best >= 0 && best_swing > 1e-6) {
                    app->ui.trigger_channel = best;
                    app->ui.trigger_level = 0.5 * (ch_min[best] + ch_max[best]);
                    app->ui.scope_capture_valid = false;
                }
            }
        }
    }
    app->ui.sim_realtime_ratio = app->sim_realtime_ratio;

    // Keep dt in step with the scope's time/div (only acts when time/div changed)
    app_sync_time_step_to_scope(app);

    // The recorder keeps MAX_HISTORY samples; ask it to span 2x the visible scope window so
    // the trigger search has slack, but never less than the raw (undecimated) span.
    {
        double want = 20.0 * app->ui.scope_time_div;
        double raw = MAX_HISTORY * app->simulation->time_step;
        simulation_set_history_span(app->simulation, want > raw ? want : raw);
    }

    // Run simulation if active
    if (app->simulation->state == SIM_RUNNING) {
        // Calculate steps based on speed
        // Real-time target: advance sim time by delta_time * speed, i.e. (delta_time*speed/dt)
        // steps, but never spend more than ~12 ms of wall clock per frame so the UI stays
        // responsive when dt is tiny. (The old code ran a fixed 1000 steps per wall second,
        // which at dt = 200 ns meant 0.2 ms of circuit time per real second.)
        double dt_now = app->simulation->time_step > 0 ? app->simulation->time_step : 1e-6;
        double want = (double)delta_time * app->simulation->speed / dt_now;
        long target = (long)(want + app->sim_step_carry);
        if (target < 1) target = 1;
        app->sim_step_carry = want + app->sim_step_carry - (double)target;
        if (app->sim_step_carry < 0) app->sim_step_carry = 0;
        if (app->sim_step_carry > 1e6) app->sim_step_carry = 0;
        Uint64 t0 = SDL_GetPerformanceCounter(), budget = SDL_GetPerformanceFrequency() * 12 / 1000;
        long done = 0;
        for (long i = 0; i < target; i++) {
            if (!simulation_step(app->simulation)) {
                simulation_pause(app->simulation);
                ui_set_status(&app->ui, simulation_get_error(app->simulation));
                break;
            }
            done++;
            if ((done & 63) == 0 && SDL_GetPerformanceCounter() - t0 > budget) break;   // out of time this frame
        }
        app->sim_realtime_ratio = (target > 0) ? (double)done / (double)target : 1.0;
        // Refresh terminal currents and wire flows once per frame for the animation
        simulation_update_flow_display(app->simulation);

    }

    // Update input state with simulation running status
    app->input.sim_running = (app->simulation->state == SIM_RUNNING);
    app->input.sim_paused = (app->simulation->state == SIM_PAUSED);

    // Update UI state
    ui_update(&app->ui, app->circuit, app->simulation);

    // Update waveform measurements if enabled
    if (app->analysis.auto_measure && app->simulation->state == SIM_RUNNING) {
        for (int i = 0; i < app->circuit->num_probes && i < MAX_PROBES; i++) {
            double times[MAX_HISTORY], values[MAX_HISTORY];
            int count = simulation_get_history(app->simulation, i, times, values, MAX_HISTORY);
            if (count > 10) {
                analysis_measure_waveform(&app->analysis.measurements[i],
                                          times, values, count);
            }
        }
    }

    // Update FFT if enabled
    if (app->ui.scope_fft_mode && app->simulation->state == SIM_RUNNING) {
        for (int i = 0; i < app->circuit->num_probes && i < MAX_PROBES; i++) {
            double times[MAX_HISTORY], values[MAX_HISTORY];
            int count = simulation_get_history(app->simulation, i, times, values, MAX_HISTORY);
            if (count >= 64) {
                double sample_rate = count > 1 ? 1.0 / (times[1] - times[0]) : 1000.0;
                int fft_samples = count < FFT_SIZE ? count : FFT_SIZE;
                analysis_fft_compute(&app->analysis, values, fft_samples, sample_rate, i);
            }
        }
    }

    // Update cursor position display
    app->ui.cursor_x = app->input.mouse_x;
    app->ui.cursor_y = app->input.mouse_y;
    render_screen_to_world(app->render,
                           app->input.mouse_x - CANVAS_X,
                           app->input.mouse_y - CANVAS_Y,
                           &app->ui.world_x, &app->ui.world_y);

    // Update node hover tooltip
    app->ui.show_node_tooltip = false;
    app->ui.hovered_node_id = -1;
    Node *hovered = circuit_find_node_at(app->circuit, app->ui.world_x, app->ui.world_y, 15);
    if (hovered) {
        app->ui.hovered_node_id = hovered->id;
        app->ui.hovered_node_voltage = hovered->voltage;
        app->ui.show_node_tooltip = true;
    }

    // Update component hover tooltip (only if not hovering over a node)
    app->ui.show_comp_tooltip = false;
    app->ui.hovered_comp_id = -1;
    if (!app->ui.show_node_tooltip) {
        Component *hovered_comp = circuit_find_component_at(app->circuit, app->ui.world_x, app->ui.world_y);
        if (hovered_comp && hovered_comp->num_terminals >= 2) {
            app->ui.hovered_comp_id = hovered_comp->id;

            // Get node voltages at component terminals
            double v0 = 0, v1 = 0;
            if (hovered_comp->node_ids[0] > 0) {
                Node *n0 = circuit_get_node(app->circuit, hovered_comp->node_ids[0]);
                if (n0) v0 = n0->voltage;
            }
            if (hovered_comp->node_ids[1] > 0) {
                Node *n1 = circuit_get_node(app->circuit, hovered_comp->node_ids[1]);
                if (n1) v1 = n1->voltage;
            }

            // Voltage drop across component (terminal 0 is positive reference)
            // Special handling for LED_ARRAY: use active anode - cathode (terminal 8)
            if (hovered_comp->type == COMP_LED_ARRAY && hovered_comp->num_terminals >= 9) {
                // Find first conducting segment for voltage display
                double anode_voltage = 0;
                bool found_active = false;
                for (int seg = 0; seg < 8; seg++) {
                    if (hovered_comp->props.led_array.currents[seg] > 1e-9) {
                        if (hovered_comp->node_ids[seg] > 0) {
                            Node *anode = circuit_get_node(app->circuit, hovered_comp->node_ids[seg]);
                            if (anode) {
                                anode_voltage = anode->voltage;
                                found_active = true;
                                break;
                            }
                        }
                    }
                }
                double cathode_voltage = 0;
                if (hovered_comp->node_ids[8] > 0) {
                    Node *cathode = circuit_get_node(app->circuit, hovered_comp->node_ids[8]);
                    if (cathode) cathode_voltage = cathode->voltage;
                }
                app->ui.hovered_comp_voltage = found_active ? (anode_voltage - cathode_voltage) : 0;
            } else {
                app->ui.hovered_comp_voltage = v0 - v1;
            }

            // Calculate current through component based on type
            double current = 0;
            switch (hovered_comp->type) {
                case COMP_RESISTOR: {
                    double R = hovered_comp->props.resistor.resistance;
                    if (R > 0.001) current = (v0 - v1) / R;
                    break;
                }
                case COMP_CAPACITOR:
                case COMP_CAPACITOR_ELEC: {
                    // For capacitors, current is C * dV/dt, approximate from voltage
                    double C = (hovered_comp->type == COMP_CAPACITOR) ?
                               hovered_comp->props.capacitor.capacitance :
                               hovered_comp->props.capacitor_elec.capacitance;
                    // Estimate current from stored charge - this is approximate
                    current = (v0 - v1) * C * 1000;  // Rough estimate
                    break;
                }
                case COMP_INDUCTOR: {
                    double L = hovered_comp->props.inductor.inductance;
                    // Inductor current - use stored state if available
                    current = (v0 - v1) / (L > 0.001 ? L : 0.001);  // Rough estimate
                    break;
                }
                case COMP_DIODE:
                case COMP_LED:
                case COMP_LED_ARRAY:
                case COMP_ZENER:
                case COMP_SCHOTTKY: {
                    // Use stored LED current from simulation for LEDs and LED arrays
                    // This matches render.c and gives accurate values
                    if (hovered_comp->type == COMP_LED) {
                        current = hovered_comp->props.led.current;
                    } else if (hovered_comp->type == COMP_LED_ARRAY) {
                        // Sum currents from all LED segments
                        current = 0;
                        for (int seg = 0; seg < 8; seg++) {
                            current += hovered_comp->props.led_array.currents[seg];
                        }
                    } else {
                        // For other diodes, use simplified linear model
                        double Vd = v0 - v1;
                        double vf = 0.7;  // Default diode Vf
                        if (hovered_comp->type == COMP_SCHOTTKY) {
                            vf = hovered_comp->props.schottky.vf;
                        }
                        if (Vd > vf) {
                            current = (Vd - vf) / 50.0;  // ~50 ohm dynamic resistance
                        } else if (Vd < -5.0 && hovered_comp->type == COMP_ZENER) {
                            current = fabs(Vd + hovered_comp->props.zener.vz) / hovered_comp->props.zener.rz;
                        }
                    }
                    break;
                }
                case COMP_DC_VOLTAGE:
                case COMP_AC_VOLTAGE:
                case COMP_SQUARE_WAVE:
                case COMP_TRIANGLE_WAVE:
                case COMP_SAWTOOTH_WAVE:
                case COMP_NOISE_SOURCE: {
                    // Get current directly from MNA solution vector
                    // In MNA, voltage sources have an associated current variable
                    // stored at index: num_matrix_nodes + voltage_var_idx
                    if (app->simulation->solution && hovered_comp->needs_voltage_var) {
                        int num_nodes = app->circuit->num_matrix_nodes;
                        int curr_idx = num_nodes + hovered_comp->voltage_var_idx;
                        if (curr_idx < app->simulation->solution_size) {
                            current = fabs(vector_get(app->simulation->solution, curr_idx));
                        }
                    }
                    break;
                }
                case COMP_DC_CURRENT: {
                    // Current source supplies its set current
                    current = hovered_comp->props.dc_current.current;
                    break;
                }
                default:
                    // For other components (transistors, opamps, etc.), show 0
                    current = 0;
                    break;
            }
            app->ui.hovered_comp_current = current;
            app->ui.show_comp_tooltip = true;
        }
    }
}

void app_update_check(App *app) {
    updater_init(&app->updater);
    if (!app->skip_update_check) updater_check_async(&app->updater);
}

/* Called once per frame: surface the result of the background check, then act on it.
 *
 * A new version installs itself. The updater script downloads the release zip, waits for this
 * process to exit, unpacks over the install directory and starts the new binary, so the whole
 * thing needs no clicks. Two things hold it back, and both are deliberate: a circuit with
 * unsaved changes is never interrupted, and there is a visible countdown that Esc cancels. */
#define UPDATE_GRACE_MS 6000

static void app_update_poll(App *app) {
    if (!app->updater.lock) return;

    if (!app->update_announced) {
        char tag[128];
        if (updater_available(&app->updater, tag, sizeof tag)) {
            app->update_announced = true;
            snprintf(app->update_tag, sizeof app->update_tag, "%s", tag);
            app->update_due_ms = SDL_GetTicks() + UPDATE_GRACE_MS;
            app->ui.btn_update.bounds = (Rect){app->ui.window_width - 70, 10, 60, 24};
        } else {
            int failed = 0;
            if (updater_checked(&app->updater, &failed)) app->update_announced = true;   // up to date or offline: stay quiet
            return;
        }
    }
    if (!app->update_due_ms || app->update_deferred) return;
    if (app->no_auto_update) {
        app->update_deferred = true;
        char msg[220];
        snprintf(msg, sizeof msg, "Update %s available (you have v%s) - click Update in the toolbar",
                 app->update_tag, APP_VERSION);
        ui_set_status(&app->ui, msg);
        return;
    }

    /* never pull the floor out from under unsaved work */
    if (app->circuit && app->circuit->modified) {
        app->update_deferred = true;
        char msg[220];
        snprintf(msg, sizeof msg, "Update %s ready (you have v%s) - save your circuit, then click Update",
                 app->update_tag, APP_VERSION);
        ui_set_status(&app->ui, msg);
        return;
    }

    Uint32 now = SDL_GetTicks();
    if (now < app->update_due_ms) {
        app->ui.update_countdown_active = true;
        char msg[220];
        snprintf(msg, sizeof msg, "Update %s found (you have v%s) - installing in %u s, Esc to keep working",
                 app->update_tag, APP_VERSION, (app->update_due_ms - now + 999) / 1000);
        ui_set_status(&app->ui, msg);
        return;
    }

    app->ui.update_countdown_active = false;
    char msg[256];
    app->update_due_ms = 0;
    if (updater_install(&app->updater, msg, sizeof msg)) {
        ui_set_status(&app->ui, msg);
        app->running = false;          /* the installer waits for this, then relaunches */
    } else {
        app->update_deferred = true;   /* it did not take: leave the button for the user */
        ui_set_status(&app->ui, msg);
    }
}

/* Frame everything that is placed. The same arithmetic app_place_template_centered does for a
   freshly placed template, but over the whole circuit and on demand. */
void app_zoom_to_fit(App *app) {
    if (!app || !app->circuit || app->circuit->num_components == 0) return;
    RenderContext *render = app->render;
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    for (int i = 0; i < app->circuit->num_components; i++) {
        Component *c = app->circuit->components[i];
        /* A label's box the way it is actually drawn: wrapped, at CANVAS_TEXT_PX a character.
           The old 6 px guess was less than half of it, so the fit framed a template as if its
           notes were half their width and the right-hand ones ended up under the scope panel. */
        float w = 60.0f, h = 60.0f;
        if (c->type == COMP_TEXT) {
            int fs = c->props.text.font_size; if (fs < 1) fs = 1; if (fs > 3) fs = 3;
            int st[CANVAS_TEXT_MAX_LINES], ln[CANVAS_TEXT_MAX_LINES];
            int nl = label_wrap(c->props.text.text, CANVAS_TEXT_WRAP, st, ln, CANVAS_TEXT_MAX_LINES);
            int widest = 0;
            for (int q = 0; q < nl; q++) if (ln[q] > widest) widest = ln[q];
            if (nl <= 0) { nl = 1; widest = (int)strlen(c->props.text.text); }
            w = (float)widest * CANVAS_TEXT_PX * fs;
            h = (float)nl * (CANVAS_TEXT_PX * fs + 2);
        }
        float x0 = (c->type == COMP_TEXT) ? c->x : c->x - w / 2;
        float y0 = (c->type == COMP_TEXT) ? c->y : c->y - h / 2;
        if (x0 < minx) minx = x0; if (y0 < miny) miny = y0;
        if (x0 + w > maxx) maxx = x0 + w; if (y0 + h > maxy) maxy = y0 + h;
    }
    if (!(maxx > minx && maxy > miny)) return;
    float bw = maxx - minx + 80.0f, bh = maxy - miny + 80.0f;
    float z = fminf(render->canvas_rect.w / bw, render->canvas_rect.h / bh);
    if (z > 2.0f) z = 2.0f;
    if (z < 0.1f) z = 0.1f;
    render->zoom = z;
    render->offset_x = render->canvas_rect.w * 0.5f - (minx + maxx) * 0.5f * z;
    render->offset_y = render->canvas_rect.h * 0.5f - (miny + maxy) * 0.5f * z;
    char msg[64]; snprintf(msg, sizeof msg, "Zoom to fit: %.0f %%", z * 100.0f);
    ui_set_status(&app->ui, msg);
}

/* The Open dialog. Everything else in this program draws its own UI, but a file picker that is
   not the system's is a file picker nobody can use: it has to know the drives, the network
   places and the last folder you were in. */
bool app_pick_file(char *out, size_t out_size, const char *title, const char *filter) {
    if (!out || out_size == 0) return false;
    out[0] = '\0';
#ifdef _WIN32
    char buf[MAX_PATH] = "";
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof ofn);
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = sizeof buf;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (!GetOpenFileNameA(&ofn)) return false;      /* cancelled, or no dialog */
    snprintf(out, out_size, "%s", buf);
    return out[0] != '\0';
#else
    (void)title; (void)filter;
    return false;
#endif
}

bool app_place_template_centered(App *app, CircuitTemplateType type) {
    RenderContext *render = app->render;
    UIState *ui = &app->ui;
    float wx, wy;
    render_screen_to_world(render, render->canvas_rect.w * 0.5f - 300.0f, render->canvas_rect.h * 0.5f - 80.0f, &wx, &wy);
    wx = floorf(wx / 20.0f) * 20.0f; wy = floorf(wy / 20.0f) * 20.0f;
    int first = app->circuit->num_components;
    int count = circuit_place_template(app->circuit, type, wx, wy);
    if (count <= 0) return false;
    // Fit the placed circuit (and its note) into the canvas: wide templates zoom out
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    for (int i = first; i < app->circuit->num_components; i++) {
        Component *c = app->circuit->components[i];
        /* A label's box the way it is actually drawn: wrapped, at CANVAS_TEXT_PX a character.
           The old 6 px guess was less than half of it, so the fit framed a template as if its
           notes were half their width and the right-hand ones ended up under the scope panel. */
        float w = 60.0f, h = 60.0f;
        if (c->type == COMP_TEXT) {
            int fs = c->props.text.font_size; if (fs < 1) fs = 1; if (fs > 3) fs = 3;
            int st[CANVAS_TEXT_MAX_LINES], ln[CANVAS_TEXT_MAX_LINES];
            int nl = label_wrap(c->props.text.text, CANVAS_TEXT_WRAP, st, ln, CANVAS_TEXT_MAX_LINES);
            int widest = 0;
            for (int q = 0; q < nl; q++) if (ln[q] > widest) widest = ln[q];
            if (nl <= 0) { nl = 1; widest = (int)strlen(c->props.text.text); }
            w = (float)widest * CANVAS_TEXT_PX * fs;
            h = (float)nl * (CANVAS_TEXT_PX * fs + 2);
        }
        float x0 = (c->type == COMP_TEXT) ? c->x : c->x - w / 2, y0 = (c->type == COMP_TEXT) ? c->y : c->y - h / 2;
        if (x0 < minx) minx = x0; if (y0 < miny) miny = y0;
        if (x0 + w > maxx) maxx = x0 + w; if (y0 + h > maxy) maxy = y0 + h;
    }
    if (maxx > minx && maxy > miny) {
        float bw = maxx - minx + 80.0f, bh = maxy - miny + 80.0f;
        float z = fminf(render->canvas_rect.w / bw, render->canvas_rect.h / bh);
        if (z > 1.0f) z = 1.0f;
        if (z < 0.3f) z = 0.3f;
        render->zoom = z;
        render->offset_x = render->canvas_rect.w * 0.5f - (minx + maxx) * 0.5f * z;
        render->offset_y = render->canvas_rect.h * 0.5f - (miny + maxy) * 0.5f * z;
    }
    app->circuit->modified = true;
    app->circuit->topology_dirty = true;
    /* The scope's whole setup for this template: everything Autoset or a hand on the knobs left
       behind goes back to neutral first, so the preset lands on a clean scope rather than on the
       last circuit's - a 9 V trigger level carried over from the Common Emitter never fires on
       the circuit that follows it, and its trace never appears. */
    ui_scope_apply_template_preset(ui, type);
    ui->scope_track_sweep = false;
    for (int i = 0; i < app->circuit->num_components; i++) {
        Component *cc = app->circuit->components[i];
        if (cc->type == COMP_AC_VOLTAGE && cc->props.ac_voltage.frequency_sweep.enabled) ui->scope_track_sweep = true;
    }
    app->input.should_autostart_sim = true;
    const CircuitTemplateInfo *info = circuit_template_get_info(type);
    char msg[128];
    snprintf(msg, sizeof msg, "Placed %s (%d components)", info ? info->name : "template", count);
    ui_set_status(ui, msg);
    return true;
}

bool app_save_window_bmp(App *app, const char *path) {
    int w = 0, h = 0;
    SDL_GetRendererOutputSize(app->renderer, &w, &h);
    if (w <= 0 || h <= 0) return false;
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surf) return false;
    bool ok = SDL_RenderReadPixels(app->renderer, NULL, SDL_PIXELFORMAT_ARGB8888, surf->pixels, surf->pitch) == 0
              && SDL_SaveBMP(surf, path) == 0;
    SDL_FreeSurface(surf);
    return ok;
}

void app_scope_popout(App *app, bool on) {
    if (!app) return;
    if (!on) {
        if (app->ui.scope_popup_renderer) { SDL_DestroyRenderer(app->ui.scope_popup_renderer); app->ui.scope_popup_renderer = NULL; }
        if (app->ui.scope_popup_window)   { SDL_DestroyWindow(app->ui.scope_popup_window);     app->ui.scope_popup_window = NULL; }
        app->ui.scope_popup_window_id = 0;
        app->ui.scope_popped_out = false;
        ui_set_status(&app->ui, "Oscilloscope docked");
        return;
    }
    if (app->ui.scope_popped_out) return;
    app->ui.scope_popup_window = SDL_CreateWindow("Oscilloscope",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1120, 700,                     /* screen plus the knob column, both usable at once */
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!app->ui.scope_popup_window) { ui_set_status(&app->ui, "Failed to create popup window"); return; }
    app->ui.scope_popup_renderer = SDL_CreateRenderer(app->ui.scope_popup_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!app->ui.scope_popup_renderer) {
        SDL_DestroyWindow(app->ui.scope_popup_window);
        app->ui.scope_popup_window = NULL;
        ui_set_status(&app->ui, "Failed to create popup renderer");
        return;
    }
    app->ui.scope_popup_window_id = SDL_GetWindowID(app->ui.scope_popup_window);
    app->ui.scope_popped_out = true;
    ui_set_status(&app->ui, "Oscilloscope popped out");
}

/* Save any renderer's contents; the popped-out scope is a second window, so a scripted
   screenshot has to be able to read from it too. */
static bool app_save_renderer_bmp(SDL_Renderer *r, const char *path) {
    int w = 0, h = 0;
    if (!r) return false;
    SDL_GetRendererOutputSize(r, &w, &h);
    if (w <= 0 || h <= 0) return false;
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surf) return false;
    bool ok = SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_ARGB8888, surf->pixels, surf->pitch) == 0
              && SDL_SaveBMP(surf, path) == 0;
    SDL_FreeSurface(surf);
    return ok;
}


/* Save any renderer's contents; the popped-out scope is a second window, so a scripted
   screenshot has to be able to read from it too. */

static void app_cli_capture(App *app) {
    app->cli_frame++;
    // Scripted typing (--keys): feed characters through the normal SDL event path so the
    // Spotlight / palette filter behave exactly as they do for a user.
    if (app->cli_keys[0] && app->cli_keys[app->cli_keys_pos] && app->cli_frame >= app->cli_keys_frame &&
        (app->cli_frame - app->cli_keys_frame) % (app->cli_keys_every > 0 ? app->cli_keys_every : 1) == 0) {
        char ch = app->cli_keys[app->cli_keys_pos++];
        if (ch == '^') ui_spotlight_open(&app->ui);
        else if (ch == '|') {
            SDL_Event ev; memset(&ev, 0, sizeof ev);
            ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_RETURN; ev.key.keysym.scancode = SDL_SCANCODE_RETURN;
            SDL_PushEvent(&ev);
        } else {
            SDL_Event ev; memset(&ev, 0, sizeof ev);
            ev.type = SDL_TEXTINPUT; ev.text.text[0] = ch; ev.text.text[1] = 0;
            SDL_PushEvent(&ev);
        }
    }
    /* Scripted pointer. A click is press-then-release at one point; a drag presses, moves in
       four steps and releases, which is what the Pan tool and the knobs need to see. */
    for (int i = 0; i < app->cli_mouse_n; i++) {
        if (app->cli_mouse[i].done || app->cli_frame != app->cli_mouse[i].frame) continue;
        app->cli_mouse[i].done = true;
        int x1 = app->cli_mouse[i].x, y1 = app->cli_mouse[i].y;
        int x2 = app->cli_mouse[i].x2, y2 = app->cli_mouse[i].y2;
        SDL_Event ev;
        memset(&ev, 0, sizeof ev);
        ev.type = SDL_MOUSEMOTION; ev.motion.x = x1; ev.motion.y = y1; SDL_PushEvent(&ev);
        memset(&ev, 0, sizeof ev);
        ev.type = SDL_MOUSEBUTTONDOWN; ev.button.button = SDL_BUTTON_LEFT;
        ev.button.x = x1; ev.button.y = y1; ev.button.clicks = 1; SDL_PushEvent(&ev);
        if (app->cli_mouse[i].drag) {
            for (int k = 1; k <= 4; k++) {
                memset(&ev, 0, sizeof ev);
                ev.type = SDL_MOUSEMOTION;
                ev.motion.x = x1 + (x2 - x1) * k / 4; ev.motion.y = y1 + (y2 - y1) * k / 4;
                ev.motion.state = SDL_BUTTON_LMASK;
                SDL_PushEvent(&ev);
            }
        }
        memset(&ev, 0, sizeof ev);
        ev.type = SDL_MOUSEBUTTONUP; ev.button.button = SDL_BUTTON_LEFT;
        ev.button.x = x2; ev.button.y = y2; ev.button.clicks = 1; SDL_PushEvent(&ev);
    }

    bool done = true;
    if (app->cli_shot_path[0]) {
        if (app->cli_frame == app->cli_shot_frame) {
            if (app_save_window_bmp(app, app->cli_shot_path)) printf("Saved %s\n", app->cli_shot_path);
            else fprintf(stderr, "Screenshot failed: %s\n", SDL_GetError());
            if (app->ui.scope_popped_out && app->ui.scope_popup_renderer) {
                char scope_path[352];
                const char *dot = strrchr(app->cli_shot_path, '.');
                int stem = dot ? (int)(dot - app->cli_shot_path) : (int)strlen(app->cli_shot_path);
                snprintf(scope_path, sizeof scope_path, "%.*s_scope%s", stem, app->cli_shot_path, dot ? dot : ".bmp");
                if (app_save_renderer_bmp(app->ui.scope_popup_renderer, scope_path)) printf("Saved %s\n", scope_path);
            }
        }
        if (app->cli_frame < app->cli_shot_frame) done = false;
    }
    if (app->cli_record_dir[0] && app->cli_recorded < app->cli_record_frames) {
        if (app->cli_frame >= app->cli_shot_frame && (app->cli_frame - app->cli_shot_frame) % (app->cli_record_every > 0 ? app->cli_record_every : 1) == 0) {
            char path[320];
            snprintf(path, sizeof path, "%s/frame_%03d.bmp", app->cli_record_dir, app->cli_recorded);
            if (app_save_window_bmp(app, path)) app->cli_recorded++;
        }
        if (app->cli_recorded < app->cli_record_frames) done = false;
    }
    if (done && app->cli_exit && (app->cli_shot_path[0] || app->cli_record_dir[0])) app->running = false;
}

void app_render(App *app) {
    app_update_poll(app);
    SDL_Renderer *r = app->renderer;

    // Clear screen
    SDL_SetRenderDrawColor(r, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(r);

    // Calculate dynamic canvas dimensions
    int canvas_w = app->ui.window_width - PALETTE_WIDTH - app->ui.properties_width;
    int canvas_h = app->ui.window_height - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT;

    // Render canvas area (circuit)
    SDL_Rect canvas_clip = {CANVAS_X, CANVAS_Y, canvas_w, canvas_h};
    SDL_RenderSetClipRect(r, &canvas_clip);

    // Set render offset to canvas position
    app->render->canvas_rect = (Rect){CANVAS_X, CANVAS_Y, canvas_w, canvas_h};

    // Update render context with simulation state for animations
    app->render->sim_time = app->simulation ? app->simulation->time : 0.0;
    app->render->sim_running = app->simulation && app->simulation->state == SIM_RUNNING;

    // Update real-time animation (independent of simulation speed for smooth visuals)
    double current_time = (double)SDL_GetTicks() / 1000.0;
    double delta_time = current_time - app->render->last_frame_time;
    app->render->last_frame_time = current_time;
    // Only advance animation when simulation is running
    if (app->render->sim_running) {
        app->render->animation_time += delta_time;
    }

    // Render grid
    if (app->render->show_grid) {
        render_grid(app->render);
    }

    // Render circuit
    render_circuit(app->render, app->circuit);

    // Render short circuit highlights (blinking red rectangles) if detected
    if (app->simulation && app->simulation->has_short_circuit) {
        render_short_circuit_highlights(app->render, app->circuit,
                                        app->simulation->short_circuit_comp_ids,
                                        app->simulation->short_circuit_count);
    }

    // Render open circuit highlights (blinking yellow rectangles) if detected
    if (app->simulation && app->simulation->has_open_circuit) {
        render_open_circuit_highlights(app->render, app->circuit,
                                       app->simulation->open_circuit_comp_ids,
                                       app->simulation->open_circuit_count);
    }

    // Render node voltage tooltip when hovering over a node
    if (app->ui.show_node_tooltip) {
        render_node_voltage_tooltip(app->render, app->ui.cursor_x, app->ui.cursor_y,
                                    app->ui.hovered_node_voltage);
    }

    // Render component tooltip when hovering over a component
    if (app->ui.show_comp_tooltip) {
        render_component_tooltip(app->render, app->ui.cursor_x, app->ui.cursor_y,
                                 app->ui.hovered_comp_voltage, app->ui.hovered_comp_current);
    }

    // Render ghost component if placing
    if (app->input.current_tool == TOOL_COMPONENT && app->input.placing_component != COMP_NONE) {
        Component ghost = {0};
        ghost.type = app->input.placing_component;
        ghost.x = snap_to_grid(app->ui.world_x);
        ghost.y = snap_to_grid(app->ui.world_y);
        ghost.rotation = app->input.placing_rotation;
        render_ghost_component(app->render, &ghost);
    }

    // Render wire preview
    if (app->input.drawing_wire) {
        Node *start = circuit_get_node(app->circuit, app->input.wire_start_node);
        if (start) {
            render_wire_preview(app->render,
                               start->x, start->y,
                               app->input.wire_preview_x,
                               app->input.wire_preview_y);
        }
    }

    // Render selection box if doing box select
    if (app->input.box_selecting) {
        render_selection_box(app->render,
                            app->input.box_start_x, app->input.box_start_y,
                            app->input.box_end_x, app->input.box_end_y);
    }

    SDL_RenderSetClipRect(r, NULL);

    // Render UI elements
    ui_render_toolbar(&app->ui, r);
    ui_render_palette(&app->ui, r);
    {   /* the properties panel names probes as well as components */
        int pi = app->input.selected_probe_idx;
        app->ui.selected_probe = (pi >= 0 && pi < app->circuit->num_probes) ? &app->circuit->probes[pi] : NULL;
    }
    ui_render_properties(&app->ui, r, app->input.selected_component, &app->input);
    // Only render oscilloscope in main window if not popped out
    if (!app->ui.scope_popped_out) {
        ui_render_oscilloscope(&app->ui, r, app->simulation, &app->analysis);
    }
    ui_render_statusbar(&app->ui, r);
    // Render VM/AM measurements after statusbar so they appear on top
    ui_render_measurements(&app->ui, r, app->simulation);

    // Render dialogs
    if (app->ui.show_shortcuts_dialog) {
        ui_render_shortcuts_dialog(&app->ui, r);
    }

    // Render spotlight search (on top of everything except neon trim)
    ui_render_spotlight(&app->ui, r);

    // Render subcircuit creation dialog
    ui_render_subcircuit_dialog(&app->ui, r);

    // Render overlay panels
    ui_render_bode_plot(&app->ui, r, app->simulation);
    ui_render_sweep_panel(&app->ui, r, &app->analysis);
    ui_render_monte_carlo_panel(&app->ui, r, &app->analysis);

    // Render synthwave LED trim on top
    ui_render_neon_trim(&app->ui, r);
    ui_render_tooltip(&app->ui, r);
    ui_render_brightness(&app->ui, r, app->ui.window_width, app->ui.window_height);

    // Present main window
    SDL_RenderPresent(r);
    app_cli_capture(app);

    // Render to popup oscilloscope window if it exists
    if (app->ui.scope_popped_out && app->ui.scope_popup_renderer) {
        SDL_Renderer *popup_r = app->ui.scope_popup_renderer;

        // Get popup window size
        int popup_w, popup_h;
        SDL_GetWindowSize(app->ui.scope_popup_window, &popup_w, &popup_h);

        // Clear popup window
        SDL_SetRenderDrawColor(popup_r, 0x10, 0x10, 0x10, 0xff);
        SDL_RenderClear(popup_r);

        // Swap in the popup layout (shared with the input path), render, restore
        ScopeCoordsBackup popup_backup = ui_setup_popup_scope_coords(&app->ui);
        ui_render_oscilloscope(&app->ui, popup_r, app->simulation, &app->analysis);
        ui_render_scope_panel(&app->ui, popup_r);
        ui_restore_popup_scope_coords(&app->ui, &popup_backup);
        { int pw = 0, ph = 0; SDL_GetRendererOutputSize(popup_r, &pw, &ph); ui_render_brightness(&app->ui, popup_r, pw, ph); }

        // Present popup window
        SDL_RenderPresent(popup_r);
    }
}

void app_new_circuit(App *app) {
    simulation_reset(app->simulation);
    circuit_clear(app->circuit);
    app->has_file = false;
    app->current_file[0] = '\0';
    input_cancel_action(&app->input);
    app->input.selected_component = NULL;
    ui_set_status(&app->ui, "New circuit created");
}

void app_save_circuit(App *app) {
    if (!app->has_file) {
        app_save_circuit_as(app);
        return;
    }

    // Always use JSON format for easier parsing
    if (file_export_json(app->circuit, app->current_file)) {
        app->circuit->modified = false;
        ui_set_status(&app->ui, "Circuit saved");
        printf("Circuit saved to: %s\n", app->current_file);
    } else {
        ui_set_status(&app->ui, file_get_error());
    }
}

void app_save_circuit_as(App *app) {
    // In a real app, this would show a file dialog
    // For now, use debug_circuit.json for automated testing
    const char *filename = "debug_circuit.json";

    if (file_export_json(app->circuit, filename)) {
        strncpy(app->current_file, filename, sizeof(app->current_file) - 1);
        app->has_file = true;
        app->circuit->modified = false;
        ui_set_status(&app->ui, "Circuit saved");
        printf("Circuit saved to: %s\n", filename);
    } else {
        ui_set_status(&app->ui, file_get_error());
    }
}

void app_load_circuit(App *app) {
    char picked[512];
    const char *filename = "circuit.json";
    if (app_pick_file(picked, sizeof picked, "Open circuit",
                      "Circuit files (*.json;*.ckt)\0*.json;*.ckt\0All files (*.*)\0*.*\0"))
        filename = picked;

    if (file_import_json(app->circuit, filename)) {
        strncpy(app->current_file, filename, sizeof(app->current_file) - 1);
        app->has_file = true;
        simulation_reset(app->simulation);
        ui_set_status(&app->ui, "Circuit loaded");
    } else {
        ui_set_status(&app->ui, file_get_error());
    }
}

void app_run_simulation(App *app) {
    // If paused and the structure is unchanged, just resume - a new component value does not
    // need the run restarted from zero
    if (app->simulation->state == SIM_PAUSED && !app->circuit->topology_dirty) {
        simulation_start(app->simulation);
        ui_set_status(&app->ui, "Simulation resumed");
        return;
    }

    // Structure changed or stopped - need full re-evaluation
    simulation_reset(app->simulation);
    app->circuit->topology_dirty = false;   // `modified` stays: it means unsaved, and it is

    // Auto-adjust timestep based on highest frequency signal in circuit
    simulation_auto_time_step(app->simulation);

    // Run DC analysis first
    if (!simulation_dc_analysis(app->simulation)) {
        ui_set_status(&app->ui, simulation_get_error(app->simulation));
        return;
    }

    simulation_start(app->simulation);
    ui_set_status(&app->ui, "Simulation running");
}

void app_pause_simulation(App *app) {
    simulation_pause(app->simulation);
    ui_set_status(&app->ui, "Simulation paused");
}

void app_step_simulation(App *app) {
    if (app->simulation->solution == NULL) {
        if (!simulation_dc_analysis(app->simulation)) {
            ui_set_status(&app->ui, simulation_get_error(app->simulation));
            return;
        }
    }

    if (simulation_step(app->simulation)) {
        ui_set_status(&app->ui, "Step completed");
    } else {
        ui_set_status(&app->ui, simulation_get_error(app->simulation));
    }
}

void app_reset_simulation(App *app) {
    simulation_reset(app->simulation);
    ui_set_status(&app->ui, "Simulation reset");
}

void app_on_component_selected(App *app, Component *comp) {
    app->ui.editing_component = comp;
    // Properties panel will be updated in ui_render_properties
}

void app_on_component_deselected(App *app) {
    app->ui.editing_component = NULL;
}

void app_on_property_changed(App *app, Component *comp) {
    // Note: We do NOT call circuit_update_component_nodes here because
    // changing a property value doesn't change the component's position.
    // Calling it would move shared junction nodes (created by circuit templates)
    // to wrong positions and break wire connections.
    app->circuit->modified = true;

    // Re-adjust time step if a frequency-related component was changed
    if (comp->type == COMP_AC_VOLTAGE || comp->type == COMP_SQUARE_WAVE ||
        comp->type == COMP_TRIANGLE_WAVE || comp->type == COMP_SAWTOOTH_WAVE) {
        simulation_auto_time_step(app->simulation);
    }
}
