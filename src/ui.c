/**
 * Circuit Playground - UI System Implementation
 */

#include <stdio.h>
#include <string.h>
#include "ui.h"

void ui_init(UIState *ui) {
    memset(ui, 0, sizeof(UIState));

    // Initialize toolbar buttons
    int btn_x = 200;
    int btn_w = 60, btn_h = 30;

    ui->btn_run = (Button){{btn_x, 10, btn_w, btn_h}, "Run", "Start simulation (F5)", false, false, true, false};
    btn_x += btn_w + 10;
    ui->btn_pause = (Button){{btn_x, 10, btn_w, btn_h}, "Pause", "Pause simulation (F6)", false, false, false, false};
    btn_x += btn_w + 10;
    ui->btn_step = (Button){{btn_x, 10, btn_w, btn_h}, "Step", "Single step (F10)", false, false, true, false};
    btn_x += btn_w + 10;
    ui->btn_reset = (Button){{btn_x, 10, btn_w, btn_h}, "Reset", "Reset simulation (F4)", false, false, true, false};
    btn_x += btn_w + 30;
    ui->btn_clear = (Button){{btn_x, 10, btn_w, btn_h}, "Clear", "Clear circuit", false, false, true, false};
    btn_x += btn_w + 10;
    ui->btn_save = (Button){{btn_x, 10, btn_w, btn_h}, "Save", "Save circuit (Ctrl+S)", false, false, true, false};
    btn_x += btn_w + 10;
    ui->btn_load = (Button){{btn_x, 10, btn_w, btn_h}, "Load", "Load circuit (Ctrl+O)", false, false, true, false};

    // Speed slider
    ui->speed_slider = (Rect){btn_x + btn_w + 30, 15, 100, 20};
    ui->speed_value = 1.0f;

    // Initialize palette items
    int pal_y = TOOLBAR_HEIGHT + 30;
    int pal_h = 40;
    int col = 0;

    // Tools section
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_NONE, TOOL_SELECT, true, "Select", false, true
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_NONE, TOOL_WIRE, true, "Wire", false, false
    };
    col = 0;
    pal_y += pal_h + 10;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_NONE, TOOL_DELETE, true, "Delete", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_NONE, TOOL_PROBE, true, "Probe", false, false
    };

    // Sources section
    pal_y += pal_h + 20;
    col = 0;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_GROUND, TOOL_COMPONENT, false, "GND", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_DC_VOLTAGE, TOOL_COMPONENT, false, "DC V", false, false
    };
    col = 0;
    pal_y += pal_h + 5;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_AC_VOLTAGE, TOOL_COMPONENT, false, "AC V", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_DC_CURRENT, TOOL_COMPONENT, false, "DC I", false, false
    };

    // Passives section
    pal_y += pal_h + 20;
    col = 0;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_RESISTOR, TOOL_COMPONENT, false, "R", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_CAPACITOR, TOOL_COMPONENT, false, "C", false, false
    };
    col = 0;
    pal_y += pal_h + 5;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_INDUCTOR, TOOL_COMPONENT, false, "L", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_DIODE, TOOL_COMPONENT, false, "D", false, false
    };

    // Semiconductors section
    pal_y += pal_h + 20;
    col = 0;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_NPN_BJT, TOOL_COMPONENT, false, "NPN", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_PNP_BJT, TOOL_COMPONENT, false, "PNP", false, false
    };
    col = 0;
    pal_y += pal_h + 5;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_NMOS, TOOL_COMPONENT, false, "NMOS", false, false
    };
    col++;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_PMOS, TOOL_COMPONENT, false, "PMOS", false, false
    };
    col = 0;
    pal_y += pal_h + 5;
    ui->palette_items[ui->num_palette_items++] = (PaletteItem){
        {10 + col*70, pal_y, 60, pal_h}, COMP_OPAMP, TOOL_COMPONENT, false, "OpAmp", false, false
    };

    // Oscilloscope settings
    ui->scope_rect = (Rect){WINDOW_WIDTH - PROPERTIES_WIDTH + 10, 400, 260, 180};
    ui->scope_channels[0] = (ScopeChannel){true, {0xff, 0xff, 0x00, 0xff}, 0, 0};
    ui->scope_channels[1] = (ScopeChannel){false, {0x00, 0xff, 0xff, 0xff}, 1, 0};
    ui->scope_time_div = 0.001;
    ui->scope_volt_div = 1.0;

    strncpy(ui->status_message, "Ready", sizeof(ui->status_message));
}

void ui_update(UIState *ui, Circuit *circuit, Simulation *sim) {
    if (!ui) return;

    // Update button states based on simulation
    if (sim) {
        ui->btn_run.enabled = (sim->state != SIM_RUNNING);
        ui->btn_pause.enabled = (sim->state == SIM_RUNNING);
        ui->sim_time = sim->time;
    }

    if (circuit) {
        ui->node_count = circuit->num_nodes;
        ui->component_count = circuit->num_components;
    }
}

static void draw_button(SDL_Renderer *r, Button *btn) {
    // Background
    if (btn->pressed) {
        SDL_SetRenderDrawColor(r, 0x00, 0xd9, 0xff, 0xff);
    } else if (btn->hovered && btn->enabled) {
        SDL_SetRenderDrawColor(r, 0x0f, 0x34, 0x60, 0xff);
    } else {
        SDL_SetRenderDrawColor(r, 0x16, 0x21, 0x3e, 0xff);
    }
    SDL_Rect rect = {btn->bounds.x, btn->bounds.y, btn->bounds.w, btn->bounds.h};
    SDL_RenderFillRect(r, &rect);

    // Border
    if (btn->enabled) {
        SDL_SetRenderDrawColor(r, 0x00, 0xd9, 0xff, 0xff);
    } else {
        SDL_SetRenderDrawColor(r, 0x50, 0x50, 0x50, 0xff);
    }
    SDL_RenderDrawRect(r, &rect);
}

static void draw_palette_item(SDL_Renderer *r, PaletteItem *item) {
    // Background
    if (item->selected) {
        SDL_SetRenderDrawColor(r, 0xe9, 0x45, 0x60, 0x40);
    } else if (item->hovered) {
        SDL_SetRenderDrawColor(r, 0x00, 0xd9, 0xff, 0x20);
    } else {
        SDL_SetRenderDrawColor(r, 0x0f, 0x34, 0x60, 0xff);
    }
    SDL_Rect rect = {item->bounds.x, item->bounds.y, item->bounds.w, item->bounds.h};
    SDL_RenderFillRect(r, &rect);

    // Border
    if (item->selected) {
        SDL_SetRenderDrawColor(r, 0xe9, 0x45, 0x60, 0xff);
    } else if (item->hovered) {
        SDL_SetRenderDrawColor(r, 0x00, 0xd9, 0xff, 0xff);
    } else {
        SDL_SetRenderDrawColor(r, 0x2a, 0x2a, 0x4e, 0xff);
    }
    SDL_RenderDrawRect(r, &rect);
}

void ui_render_toolbar(UIState *ui, SDL_Renderer *renderer) {
    // Toolbar background
    SDL_SetRenderDrawColor(renderer, 0x16, 0x21, 0x3e, 0xff);
    SDL_Rect toolbar = {0, 0, WINDOW_WIDTH, TOOLBAR_HEIGHT};
    SDL_RenderFillRect(renderer, &toolbar);

    // Title
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    SDL_Rect title = {10, 15, 150, 20};
    SDL_RenderFillRect(renderer, &title);

    // Buttons
    draw_button(renderer, &ui->btn_run);
    draw_button(renderer, &ui->btn_pause);
    draw_button(renderer, &ui->btn_step);
    draw_button(renderer, &ui->btn_reset);
    draw_button(renderer, &ui->btn_clear);
    draw_button(renderer, &ui->btn_save);
    draw_button(renderer, &ui->btn_load);

    // Speed slider background
    SDL_SetRenderDrawColor(renderer, 0x0f, 0x34, 0x60, 0xff);
    SDL_Rect slider_bg = {ui->speed_slider.x, ui->speed_slider.y, ui->speed_slider.w, ui->speed_slider.h};
    SDL_RenderFillRect(renderer, &slider_bg);

    // Speed slider fill
    int fill_w = (int)(ui->speed_slider.w * (ui->speed_value / 100.0f));
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    SDL_Rect slider_fill = {ui->speed_slider.x, ui->speed_slider.y, fill_w, ui->speed_slider.h};
    SDL_RenderFillRect(renderer, &slider_fill);

    // Toolbar border
    SDL_SetRenderDrawColor(renderer, 0x0f, 0x34, 0x60, 0xff);
    SDL_RenderDrawLine(renderer, 0, TOOLBAR_HEIGHT - 1, WINDOW_WIDTH, TOOLBAR_HEIGHT - 1);
}

void ui_render_palette(UIState *ui, SDL_Renderer *renderer) {
    // Palette background
    SDL_SetRenderDrawColor(renderer, 0x16, 0x21, 0x3e, 0xff);
    SDL_Rect palette = {0, TOOLBAR_HEIGHT, PALETTE_WIDTH, WINDOW_HEIGHT - TOOLBAR_HEIGHT - STATUSBAR_HEIGHT};
    SDL_RenderFillRect(renderer, &palette);

    // Palette items
    for (int i = 0; i < ui->num_palette_items; i++) {
        draw_palette_item(renderer, &ui->palette_items[i]);
    }

    // Border
    SDL_SetRenderDrawColor(renderer, 0x0f, 0x34, 0x60, 0xff);
    SDL_RenderDrawLine(renderer, PALETTE_WIDTH - 1, TOOLBAR_HEIGHT, PALETTE_WIDTH - 1, WINDOW_HEIGHT - STATUSBAR_HEIGHT);
}

void ui_render_properties(UIState *ui, SDL_Renderer *renderer, Component *selected) {
    int x = WINDOW_WIDTH - PROPERTIES_WIDTH;
    int y = TOOLBAR_HEIGHT;

    // Background
    SDL_SetRenderDrawColor(renderer, 0x16, 0x21, 0x3e, 0xff);
    SDL_Rect panel = {x, y, PROPERTIES_WIDTH, 200};
    SDL_RenderFillRect(renderer, &panel);

    // Title bar
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    SDL_Rect title = {x + 10, y + 10, 100, 15};
    SDL_RenderFillRect(renderer, &title);

    // Border
    SDL_SetRenderDrawColor(renderer, 0x0f, 0x34, 0x60, 0xff);
    SDL_RenderDrawLine(renderer, x, y, x, WINDOW_HEIGHT - STATUSBAR_HEIGHT);
}

void ui_render_measurements(UIState *ui, SDL_Renderer *renderer, Simulation *sim) {
    int x = WINDOW_WIDTH - PROPERTIES_WIDTH;
    int y = TOOLBAR_HEIGHT + 210;

    // Background
    SDL_SetRenderDrawColor(renderer, 0x16, 0x21, 0x3e, 0xff);
    SDL_Rect panel = {x, y, PROPERTIES_WIDTH, 180};
    SDL_RenderFillRect(renderer, &panel);

    // Title
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    SDL_Rect title = {x + 10, y + 10, 100, 15};
    SDL_RenderFillRect(renderer, &title);
}

void ui_render_oscilloscope(UIState *ui, SDL_Renderer *renderer, Simulation *sim) {
    Rect *r = &ui->scope_rect;

    // Background (black)
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
    SDL_Rect bg = {r->x, r->y, r->w, r->h};
    SDL_RenderFillRect(renderer, &bg);

    // Grid
    SDL_SetRenderDrawColor(renderer, 0x33, 0x33, 0x33, 0xff);
    int div_x = r->w / 10;
    int div_y = r->h / 8;
    for (int i = 0; i <= 10; i++) {
        SDL_RenderDrawLine(renderer, r->x + i * div_x, r->y, r->x + i * div_x, r->y + r->h);
    }
    for (int i = 0; i <= 8; i++) {
        SDL_RenderDrawLine(renderer, r->x, r->y + i * div_y, r->x + r->w, r->y + i * div_y);
    }

    // Center lines (brighter)
    SDL_SetRenderDrawColor(renderer, 0x55, 0x55, 0x55, 0xff);
    SDL_RenderDrawLine(renderer, r->x + r->w / 2, r->y, r->x + r->w / 2, r->y + r->h);
    SDL_RenderDrawLine(renderer, r->x, r->y + r->h / 2, r->x + r->w, r->y + r->h / 2);

    // Draw traces if simulation has data
    if (sim && sim->history_count > 1) {
        for (int ch = 0; ch < 2; ch++) {
            if (!ui->scope_channels[ch].enabled) continue;

            SDL_SetRenderDrawColor(renderer,
                ui->scope_channels[ch].color.r,
                ui->scope_channels[ch].color.g,
                ui->scope_channels[ch].color.b, 0xff);

            double times[MAX_HISTORY];
            double values[MAX_HISTORY];
            int count = simulation_get_history(sim, ch, times, values, r->w);

            int center_y = r->y + r->h / 2;
            double scale = (r->h / 8.0) / ui->scope_volt_div;

            for (int i = 1; i < count; i++) {
                int x1 = r->x + (i - 1);
                int x2 = r->x + i;
                int y1 = center_y - (int)(values[i-1] * scale);
                int y2 = center_y - (int)(values[i] * scale);
                y1 = CLAMP(y1, r->y, r->y + r->h);
                y2 = CLAMP(y2, r->y, r->y + r->h);
                SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
            }
        }
    }

    // Border
    SDL_SetRenderDrawColor(renderer, 0x0f, 0x34, 0x60, 0xff);
    SDL_RenderDrawRect(renderer, &bg);
}

void ui_render_statusbar(UIState *ui, SDL_Renderer *renderer) {
    int y = WINDOW_HEIGHT - STATUSBAR_HEIGHT;

    // Background
    SDL_SetRenderDrawColor(renderer, 0x0f, 0x34, 0x60, 0xff);
    SDL_Rect bar = {0, y, WINDOW_WIDTH, STATUSBAR_HEIGHT};
    SDL_RenderFillRect(renderer, &bar);

    // Status indicators (placeholder rectangles)
    SDL_SetRenderDrawColor(renderer, 0xb0, 0xb0, 0xb0, 0xff);
    SDL_Rect status_rect = {10, y + 5, 300, 14};
    SDL_RenderFillRect(renderer, &status_rect);

    // Time display
    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    SDL_Rect time_rect = {WINDOW_WIDTH - 200, y + 5, 80, 14};
    SDL_RenderFillRect(renderer, &time_rect);

    // Counts
    SDL_SetRenderDrawColor(renderer, 0xb0, 0xb0, 0xb0, 0xff);
    SDL_Rect nodes_rect = {WINDOW_WIDTH - 100, y + 5, 80, 14};
    SDL_RenderFillRect(renderer, &nodes_rect);
}

void ui_render_shortcuts_dialog(UIState *ui, SDL_Renderer *renderer) {
    // Semi-transparent overlay
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xb0);
    SDL_Rect overlay = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(renderer, &overlay);

    // Dialog box
    int dw = 400, dh = 300;
    int dx = (WINDOW_WIDTH - dw) / 2;
    int dy = (WINDOW_HEIGHT - dh) / 2;

    SDL_SetRenderDrawColor(renderer, 0x16, 0x21, 0x3e, 0xff);
    SDL_Rect dialog = {dx, dy, dw, dh};
    SDL_RenderFillRect(renderer, &dialog);

    SDL_SetRenderDrawColor(renderer, 0x00, 0xd9, 0xff, 0xff);
    SDL_RenderDrawRect(renderer, &dialog);

    // Title
    SDL_Rect title = {dx + 20, dy + 20, 200, 20};
    SDL_RenderFillRect(renderer, &title);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static bool point_in_rect(int x, int y, Rect *r) {
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

int ui_handle_click(UIState *ui, int x, int y, bool is_down) {
    if (!ui) return UI_ACTION_NONE;

    // Check toolbar buttons
    if (is_down) {
        if (point_in_rect(x, y, &ui->btn_run.bounds) && ui->btn_run.enabled) {
            ui->btn_run.pressed = true;
            return UI_ACTION_RUN;
        }
        if (point_in_rect(x, y, &ui->btn_pause.bounds) && ui->btn_pause.enabled) {
            return UI_ACTION_PAUSE;
        }
        if (point_in_rect(x, y, &ui->btn_step.bounds) && ui->btn_step.enabled) {
            return UI_ACTION_STEP;
        }
        if (point_in_rect(x, y, &ui->btn_reset.bounds) && ui->btn_reset.enabled) {
            return UI_ACTION_RESET;
        }
        if (point_in_rect(x, y, &ui->btn_clear.bounds) && ui->btn_clear.enabled) {
            return UI_ACTION_CLEAR;
        }
        if (point_in_rect(x, y, &ui->btn_save.bounds) && ui->btn_save.enabled) {
            return UI_ACTION_SAVE;
        }
        if (point_in_rect(x, y, &ui->btn_load.bounds) && ui->btn_load.enabled) {
            return UI_ACTION_LOAD;
        }

        // Check palette items
        for (int i = 0; i < ui->num_palette_items; i++) {
            if (point_in_rect(x, y, &ui->palette_items[i].bounds)) {
                // Deselect all
                for (int j = 0; j < ui->num_palette_items; j++) {
                    ui->palette_items[j].selected = false;
                }
                ui->palette_items[i].selected = true;
                ui->selected_palette_idx = i;

                if (ui->palette_items[i].is_tool) {
                    return UI_ACTION_SELECT_TOOL + ui->palette_items[i].tool_type;
                } else {
                    return UI_ACTION_SELECT_COMP + ui->palette_items[i].comp_type;
                }
            }
        }
    } else {
        ui->btn_run.pressed = false;
    }

    return UI_ACTION_NONE;
}

int ui_handle_motion(UIState *ui, int x, int y) {
    if (!ui) return UI_ACTION_NONE;

    // Update button hover states
    ui->btn_run.hovered = point_in_rect(x, y, &ui->btn_run.bounds);
    ui->btn_pause.hovered = point_in_rect(x, y, &ui->btn_pause.bounds);
    ui->btn_step.hovered = point_in_rect(x, y, &ui->btn_step.bounds);
    ui->btn_reset.hovered = point_in_rect(x, y, &ui->btn_reset.bounds);
    ui->btn_clear.hovered = point_in_rect(x, y, &ui->btn_clear.bounds);
    ui->btn_save.hovered = point_in_rect(x, y, &ui->btn_save.bounds);
    ui->btn_load.hovered = point_in_rect(x, y, &ui->btn_load.bounds);

    // Update palette hover states
    for (int i = 0; i < ui->num_palette_items; i++) {
        ui->palette_items[i].hovered = point_in_rect(x, y, &ui->palette_items[i].bounds);
    }

    return UI_ACTION_NONE;
}

void ui_set_status(UIState *ui, const char *msg) {
    if (ui && msg) {
        strncpy(ui->status_message, msg, sizeof(ui->status_message) - 1);
        ui->status_message[sizeof(ui->status_message) - 1] = '\0';
    }
}

void ui_update_measurements(UIState *ui, Simulation *sim, Circuit *circuit) {
    if (!ui) return;

    if (sim) {
        ui->sim_time = sim->time;
    }

    if (circuit) {
        ui->node_count = circuit->num_nodes;
        ui->component_count = circuit->num_components;
    }
}
