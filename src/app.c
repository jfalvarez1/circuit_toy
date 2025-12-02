/**
 * Circuit Playground - Main Application Implementation
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "app.h"
#include "file_io.h"
#include "circuits.h"

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

bool app_init(App *app) {
    memset(app, 0, sizeof(App));

    // Create window
    app->window = SDL_CreateWindow(
        "Circuit Playground",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!app->window) {
        fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        return false;
    }

    // Create renderer
    app->renderer = SDL_CreateRenderer(
        app->window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!app->renderer) {
        fprintf(stderr, "Renderer creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(app->window);
        return false;
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
    app->show_voltages = false;
    app->show_current = false;
    app->last_frame_time = SDL_GetTicks();

    // Center the view
    render_reset_view(app->render);

    ui_set_status(&app->ui, "Ready - Select a component or tool to begin");

    return true;
}

void app_shutdown(App *app) {
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
                } else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
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
            case UI_ACTION_SCOPE_VOLT_UP:
                // Increase volts/div using 1-2-5 sequence (like real scopes)
                {
                    static const double volt_steps[] = {
                        0.001, 0.002, 0.005, 0.01, 0.02, 0.05,
                        0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0
                    };
                    int n = sizeof(volt_steps) / sizeof(volt_steps[0]);
                    for (int i = 0; i < n - 1; i++) {
                        if (app->ui.scope_volt_div <= volt_steps[i] * 1.01) {
                            app->ui.scope_volt_div = volt_steps[i + 1];
                            break;
                        }
                    }
                }
                break;
            case UI_ACTION_SCOPE_VOLT_DOWN:
                // Decrease volts/div using 1-2-5 sequence
                {
                    static const double volt_steps[] = {
                        0.001, 0.002, 0.005, 0.01, 0.02, 0.05,
                        0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0
                    };
                    int n = sizeof(volt_steps) / sizeof(volt_steps[0]);
                    for (int i = n - 1; i > 0; i--) {
                        if (app->ui.scope_volt_div >= volt_steps[i] * 0.99) {
                            app->ui.scope_volt_div = volt_steps[i - 1];
                            break;
                        }
                    }
                }
                break;
            case UI_ACTION_SCOPE_TIME_UP:
                // Increase time/div using 1-2-5 sequence
                {
                    static const double time_steps[] = {
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
                }
                break;
            case UI_ACTION_SCOPE_TIME_DOWN:
                // Decrease time/div using 1-2-5 sequence
                {
                    static const double time_steps[] = {
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
                // Increase trigger level by 0.1V (or scaled by volts/div)
                app->ui.trigger_level += app->ui.scope_volt_div * 0.2;
                break;
            case UI_ACTION_SCOPE_TRIG_DOWN:
                // Decrease trigger level by 0.1V (or scaled by volts/div)
                app->ui.trigger_level -= app->ui.scope_volt_div * 0.2;
                break;
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
                // Toggle measurement cursors
                app->ui.scope_cursor_mode = !app->ui.scope_cursor_mode;
                if (app->ui.scope_cursor_mode) {
                    ui_set_status(&app->ui, "Cursors ON - Click in scope to position");
                } else {
                    ui_set_status(&app->ui, "Cursors OFF");
                }
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
                // Toggle pop-out oscilloscope window
                if (app->ui.scope_popped_out) {
                    // Close the popup window
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
                } else {
                    // Create popup window
                    app->ui.scope_popup_window = SDL_CreateWindow(
                        "Oscilloscope",
                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                        600, 400,
                        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
                    );
                    if (app->ui.scope_popup_window) {
                        app->ui.scope_popup_renderer = SDL_CreateRenderer(
                            app->ui.scope_popup_window, -1,
                            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
                        );
                        if (app->ui.scope_popup_renderer) {
                            app->ui.scope_popup_window_id = SDL_GetWindowID(app->ui.scope_popup_window);
                            app->ui.scope_popped_out = true;
                            ui_set_status(&app->ui, "Oscilloscope popped out");
                        } else {
                            SDL_DestroyWindow(app->ui.scope_popup_window);
                            app->ui.scope_popup_window = NULL;
                            ui_set_status(&app->ui, "Failed to create popup renderer");
                        }
                    } else {
                        ui_set_status(&app->ui, "Failed to create popup window");
                    }
                }
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

            case UI_ACTION_PROP_APPLY:
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
                if (app->input.pending_ui_action >= UI_ACTION_SELECT_CIRCUIT &&
                    app->input.pending_ui_action < UI_ACTION_SELECT_CIRCUIT + 100) {
                    int circuit_type = app->input.pending_ui_action - UI_ACTION_SELECT_CIRCUIT;
                    const CircuitTemplateInfo *info = circuit_template_get_info(circuit_type);
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Click on canvas to place %s circuit", info->name);
                    ui_set_status(&app->ui, msg);
                    // Note: actual placement happens in input.c when user clicks on canvas
                }
                // Handle property edit start actions (UI_ACTION_PROP_EDIT + prop_type)
                else if (app->input.pending_ui_action >= UI_ACTION_PROP_EDIT &&
                    app->input.pending_ui_action < UI_ACTION_PROP_EDIT + 100) {
                    int prop_type = app->input.pending_ui_action - UI_ACTION_PROP_EDIT;
                    if (app->input.selected_component && !app->input.editing_property) {
                        // Get current value to show in edit field
                        char current_value[64] = "";
                        Component *c = app->input.selected_component;
                        if (prop_type == PROP_VALUE || prop_type == PROP_AMPLITUDE) {
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
                            // Color selector - cycle through presets immediately
                            if (c->type == COMP_LED) {
                                // Cycle through colors: Red->Orange->Yellow->Green->Cyan->Blue->Violet->White->IR->Red
                                double wl = c->props.led.wavelength;
                                const char *new_color = "Red";
                                if (wl >= 640 && wl <= 780) {
                                    c->props.led.wavelength = 620; c->props.led.vf = 2.0; new_color = "Orange";
                                } else if (wl >= 600 && wl < 640) {
                                    c->props.led.wavelength = 590; c->props.led.vf = 2.1; new_color = "Yellow";
                                } else if (wl >= 580 && wl < 600) {
                                    c->props.led.wavelength = 550; c->props.led.vf = 2.2; new_color = "Green";
                                } else if (wl >= 510 && wl < 580) {
                                    c->props.led.wavelength = 500; c->props.led.vf = 3.0; new_color = "Cyan";
                                } else if (wl >= 490 && wl < 510) {
                                    c->props.led.wavelength = 470; c->props.led.vf = 3.2; new_color = "Blue";
                                } else if (wl >= 440 && wl < 490) {
                                    c->props.led.wavelength = 420; c->props.led.vf = 2.8; new_color = "Violet";
                                } else if (wl >= 380 && wl < 440) {
                                    c->props.led.wavelength = 0; c->props.led.vf = 3.2; new_color = "White";
                                } else if (wl == 0) {
                                    c->props.led.wavelength = 850; c->props.led.vf = 1.4;
                                    c->props.led.max_current = 0.050; new_color = "IR";
                                } else {
                                    c->props.led.wavelength = 660; c->props.led.vf = 1.8;
                                    c->props.led.max_current = 0.020; new_color = "Red";
                                }
                                char msg[64];
                                snprintf(msg, sizeof(msg), "LED Color: %s (%.0f nm, Vf=%.1fV)",
                                         new_color, c->props.led.wavelength, c->props.led.vf);
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
                                default: break;
                            }
                            char msg[64];
                            snprintf(msg, sizeof(msg), "Model: %s", model_name);
                            ui_set_status(&app->ui, msg);
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

    // Run simulation if active
    if (app->simulation->state == SIM_RUNNING) {
        // Calculate steps based on speed
        int steps = (int)(delta_time * app->simulation->speed * 1000);
        steps = CLAMP(steps, 1, 1000);

        for (int i = 0; i < steps; i++) {
            if (!simulation_step(app->simulation)) {
                simulation_pause(app->simulation);
                ui_set_status(&app->ui, simulation_get_error(app->simulation));
                break;
            }
        }
    }

    // Update input state with simulation running status
    app->input.sim_running = (app->simulation->state == SIM_RUNNING);

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
}

void app_render(App *app) {
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
    ui_render_properties(&app->ui, r, app->input.selected_component, &app->input);
    ui_render_measurements(&app->ui, r, app->simulation);
    ui_render_oscilloscope(&app->ui, r, app->simulation, &app->analysis);
    ui_render_statusbar(&app->ui, r);

    // Render dialogs
    if (app->ui.show_shortcuts_dialog) {
        ui_render_shortcuts_dialog(&app->ui, r);
    }

    // Render overlay panels
    ui_render_bode_plot(&app->ui, r, app->simulation);
    ui_render_sweep_panel(&app->ui, r, &app->analysis);
    ui_render_monte_carlo_panel(&app->ui, r, &app->analysis);

    // Render synthwave LED trim on top
    ui_render_neon_trim(&app->ui, r);

    // Present main window
    SDL_RenderPresent(r);

    // Render to popup oscilloscope window if it exists
    if (app->ui.scope_popped_out && app->ui.scope_popup_renderer) {
        SDL_Renderer *popup_r = app->ui.scope_popup_renderer;

        // Get popup window size
        int popup_w, popup_h;
        SDL_GetWindowSize(app->ui.scope_popup_window, &popup_w, &popup_h);

        // Clear popup window
        SDL_SetRenderDrawColor(popup_r, 0x10, 0x10, 0x10, 0xff);
        SDL_RenderClear(popup_r);

        // Save original scope rect and button positions
        Rect orig_scope_rect = app->ui.scope_rect;
        Rect orig_btn_volt_up = app->ui.btn_scope_volt_up.bounds;
        Rect orig_btn_volt_down = app->ui.btn_scope_volt_down.bounds;
        Rect orig_btn_time_up = app->ui.btn_scope_time_up.bounds;
        Rect orig_btn_time_down = app->ui.btn_scope_time_down.bounds;
        Rect orig_btn_autoset = app->ui.btn_scope_autoset.bounds;
        Rect orig_btn_trig_mode = app->ui.btn_scope_trig_mode.bounds;
        Rect orig_btn_trig_edge = app->ui.btn_scope_trig_edge.bounds;
        Rect orig_btn_trig_ch = app->ui.btn_scope_trig_ch.bounds;
        Rect orig_btn_trig_up = app->ui.btn_scope_trig_up.bounds;
        Rect orig_btn_trig_down = app->ui.btn_scope_trig_down.bounds;
        Rect orig_btn_mode = app->ui.btn_scope_mode.bounds;
        Rect orig_btn_cursor = app->ui.btn_scope_cursor.bounds;
        Rect orig_btn_fft = app->ui.btn_scope_fft.bounds;
        Rect orig_btn_screenshot = app->ui.btn_scope_screenshot.bounds;
        Rect orig_btn_bode = app->ui.btn_bode.bounds;

        app->ui.scope_rect = (Rect){10, 30, popup_w - 20, popup_h - 130};

        // Recalculate button positions for popup window
        int scope_btn_y = app->ui.scope_rect.y + app->ui.scope_rect.h + 5;
        int scope_btn_w = 32, scope_btn_h = 22;
        int scope_btn_x = app->ui.scope_rect.x;
        int row_spacing = scope_btn_h + 4;

        // Row 1: Scale controls
        app->ui.btn_scope_volt_up.bounds = (Rect){scope_btn_x, scope_btn_y, scope_btn_w, scope_btn_h};
        scope_btn_x += scope_btn_w + 3;
        app->ui.btn_scope_volt_down.bounds = (Rect){scope_btn_x, scope_btn_y, scope_btn_w, scope_btn_h};
        scope_btn_x += scope_btn_w + 10;
        app->ui.btn_scope_time_up.bounds = (Rect){scope_btn_x, scope_btn_y, scope_btn_w, scope_btn_h};
        scope_btn_x += scope_btn_w + 3;
        app->ui.btn_scope_time_down.bounds = (Rect){scope_btn_x, scope_btn_y, scope_btn_w, scope_btn_h};
        scope_btn_x += scope_btn_w + 10;
        app->ui.btn_scope_autoset.bounds = (Rect){scope_btn_x, scope_btn_y, 50, scope_btn_h};

        // Row 2: Trigger controls
        scope_btn_y += row_spacing;
        scope_btn_x = app->ui.scope_rect.x;
        app->ui.btn_scope_trig_mode.bounds = (Rect){scope_btn_x, scope_btn_y, 45, scope_btn_h};
        scope_btn_x += 48;
        app->ui.btn_scope_trig_edge.bounds = (Rect){scope_btn_x, scope_btn_y, 28, scope_btn_h};
        scope_btn_x += 31;
        app->ui.btn_scope_trig_ch.bounds = (Rect){scope_btn_x, scope_btn_y, 35, scope_btn_h};
        scope_btn_x += 38;
        app->ui.btn_scope_trig_up.bounds = (Rect){scope_btn_x, scope_btn_y, 24, scope_btn_h};
        scope_btn_x += 27;
        app->ui.btn_scope_trig_down.bounds = (Rect){scope_btn_x, scope_btn_y, 24, scope_btn_h};

        // Row 3: Display modes and tools
        scope_btn_y += row_spacing;
        scope_btn_x = app->ui.scope_rect.x;
        app->ui.btn_scope_mode.bounds = (Rect){scope_btn_x, scope_btn_y, 35, scope_btn_h};
        scope_btn_x += 38;
        app->ui.btn_scope_cursor.bounds = (Rect){scope_btn_x, scope_btn_y, 35, scope_btn_h};
        scope_btn_x += 38;
        app->ui.btn_scope_fft.bounds = (Rect){scope_btn_x, scope_btn_y, 35, scope_btn_h};
        scope_btn_x += 38;
        app->ui.btn_scope_screenshot.bounds = (Rect){scope_btn_x, scope_btn_y, 35, scope_btn_h};
        scope_btn_x += 38;
        app->ui.btn_bode.bounds = (Rect){scope_btn_x, scope_btn_y, 40, scope_btn_h};

        // Render oscilloscope to popup window
        ui_render_oscilloscope(&app->ui, popup_r, app->simulation, &app->analysis);

        // Restore original scope rect and button bounds
        app->ui.scope_rect = orig_scope_rect;
        app->ui.btn_scope_volt_up.bounds = orig_btn_volt_up;
        app->ui.btn_scope_volt_down.bounds = orig_btn_volt_down;
        app->ui.btn_scope_time_up.bounds = orig_btn_time_up;
        app->ui.btn_scope_time_down.bounds = orig_btn_time_down;
        app->ui.btn_scope_autoset.bounds = orig_btn_autoset;
        app->ui.btn_scope_trig_mode.bounds = orig_btn_trig_mode;
        app->ui.btn_scope_trig_edge.bounds = orig_btn_trig_edge;
        app->ui.btn_scope_trig_ch.bounds = orig_btn_trig_ch;
        app->ui.btn_scope_trig_up.bounds = orig_btn_trig_up;
        app->ui.btn_scope_trig_down.bounds = orig_btn_trig_down;
        app->ui.btn_scope_mode.bounds = orig_btn_mode;
        app->ui.btn_scope_cursor.bounds = orig_btn_cursor;
        app->ui.btn_scope_fft.bounds = orig_btn_fft;
        app->ui.btn_scope_screenshot.bounds = orig_btn_screenshot;
        app->ui.btn_bode.bounds = orig_btn_bode;

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

    if (file_save_circuit(app->circuit, app->current_file)) {
        app->circuit->modified = false;
        ui_set_status(&app->ui, "Circuit saved");
    } else {
        ui_set_status(&app->ui, file_get_error());
    }
}

void app_save_circuit_as(App *app) {
    // In a real app, this would show a file dialog
    // For now, use a default name
    const char *filename = "circuit.json";

    if (file_export_json(app->circuit, filename)) {
        strncpy(app->current_file, filename, sizeof(app->current_file) - 1);
        app->has_file = true;
        app->circuit->modified = false;
        ui_set_status(&app->ui, "Circuit saved");
    } else {
        ui_set_status(&app->ui, file_get_error());
    }
}

void app_load_circuit(App *app) {
    // In a real app, this would show a file dialog
    const char *filename = "circuit.json";

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
    if (app->simulation->state == SIM_PAUSED) {
        simulation_start(app->simulation);
        ui_set_status(&app->ui, "Simulation resumed");
        return;
    }

    // Auto-adjust time step based on circuit's highest frequency
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
