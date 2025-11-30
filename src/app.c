/**
 * Circuit Playground - Main Application Implementation
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "app.h"
#include "file_io.h"

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
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    // Handle window resize
                    int w, h;
                    SDL_GetWindowSize(app->window, &w, &h);

                    // Update UI dimensions
                    app->ui.window_width = w;
                    app->ui.window_height = h;

                    // Update canvas area
                    app->render->canvas_rect.w = w - PALETTE_WIDTH - PROPERTIES_WIDTH;
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
                if (app->ui.trigger_mode == TRIG_SINGLE) {
                    app->ui.trigger_armed = true;
                    app->ui.triggered = false;
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

            case UI_ACTION_PROP_APPLY:
                // Apply text-edited property value
                if (app->input.selected_component) {
                    if (input_apply_property_edit(&app->input, app->input.selected_component)) {
                        circuit_update_component_nodes(app->circuit, app->input.selected_component);
                        app->circuit->modified = true;
                        ui_set_status(&app->ui, "Property updated");
                    } else {
                        ui_set_status(&app->ui, "Invalid value");
                    }
                }
                break;

            default:
                // Handle property edit start actions (UI_ACTION_PROP_EDIT + prop_type)
                if (app->input.pending_ui_action >= UI_ACTION_PROP_EDIT &&
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
                                case COMP_INDUCTOR: snprintf(current_value, sizeof(current_value), "%.6g", c->props.inductor.inductance); break;
                                case COMP_SQUARE_WAVE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.square_wave.amplitude); break;
                                case COMP_TRIANGLE_WAVE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.triangle_wave.amplitude); break;
                                case COMP_SAWTOOTH_WAVE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.sawtooth_wave.amplitude); break;
                                case COMP_NOISE_SOURCE: snprintf(current_value, sizeof(current_value), "%.6g", c->props.noise_source.amplitude); break;
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
                        input_start_property_edit(&app->input, prop_type, current_value);
                        ui_set_status(&app->ui, "Type value (use k,M,m,u,n,p suffix), Enter to apply");
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

    // Update UI state
    ui_update(&app->ui, app->circuit, app->simulation);

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
    int canvas_w = app->ui.window_width - PALETTE_WIDTH - PROPERTIES_WIDTH;
    int canvas_h = app->ui.window_height - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT;

    // Render canvas area (circuit)
    SDL_Rect canvas_clip = {CANVAS_X, CANVAS_Y, canvas_w, canvas_h};
    SDL_RenderSetClipRect(r, &canvas_clip);

    // Set render offset to canvas position
    app->render->canvas_rect = (Rect){CANVAS_X, CANVAS_Y, canvas_w, canvas_h};

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

    SDL_RenderSetClipRect(r, NULL);

    // Render UI elements
    ui_render_toolbar(&app->ui, r);
    ui_render_palette(&app->ui, r);
    ui_render_properties(&app->ui, r, app->input.selected_component, &app->input);
    ui_render_measurements(&app->ui, r, app->simulation);
    ui_render_oscilloscope(&app->ui, r, app->simulation);
    ui_render_statusbar(&app->ui, r);

    // Render dialogs
    if (app->ui.show_shortcuts_dialog) {
        ui_render_shortcuts_dialog(&app->ui, r);
    }

    // Present
    SDL_RenderPresent(r);
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
    circuit_update_component_nodes(app->circuit, comp);
    app->circuit->modified = true;
}
