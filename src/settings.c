#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "app.h"
#include "settings.h"
#include "version.h"

static char g_path[512];

const char *settings_path(void) {
    if (!g_path[0]) {
        char *dir = SDL_GetPrefPath("circuit_toy", "circuit-playground");
        if (dir) { snprintf(g_path, sizeof g_path, "%ssettings.json", dir); SDL_free(dir); }
    }
    return g_path;
}

static int get_num(const char *s, const char *key, double *out) {
    char pat[64]; snprintf(pat, sizeof pat, "\"%s\":", key);
    const char *p = strstr(s, pat);
    if (!p) return 0;
    return sscanf(p + strlen(pat), " %lf", out) == 1;
}

void settings_load(App *app) {
    const char *path = settings_path();
    if (!path[0]) return;
    FILE *f = fopen(path, "rb");
    if (!f) return;
    char buf[4096]; size_t n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f);
    double v;
    if (get_num(buf, "window_w", &v) && v >= 640 && v <= 8192) app->saved_window_w = (int)v;
    if (get_num(buf, "window_h", &v) && v >= 480 && v <= 8192) app->saved_window_h = (int)v;
    if (get_num(buf, "brightness", &v) && v >= 0.2 && v <= 1.0) app->ui.brightness = (float)v;
    if (get_num(buf, "show_values", &v)) app->render->show_values = v != 0;
    if (get_num(buf, "show_voltages", &v)) app->render->show_voltages = app->show_voltages = v != 0;
    if (get_num(buf, "show_current", &v)) app->render->show_current = app->show_current = v != 0;
    if (get_num(buf, "show_grid", &v)) app->render->show_grid = v != 0;
    if (get_num(buf, "left_tab", &v)) app->ui.left_tab = v != 0;
    if (get_num(buf, "properties_collapsed", &v)) app->ui.properties_collapsed = v != 0;
    /* knobs or sliders on the pop-out panel: a preference, so it is remembered */
    if (get_num(buf, "scope_sliders", &v)) app->ui.scope_sliders = v != 0;
    if (get_num(buf, "speed", &v) && v >= 1 && v <= 100) app->ui.speed_value = (float)v;
    if (get_num(buf, "light_level", &v) && v >= 0 && v <= 1) g_environment.light_level = v;
    if (get_num(buf, "temperature", &v) && v >= -40 && v <= 125) g_environment.temperature = v;
}

int settings_save(App *app) {
    const char *path = settings_path();
    if (!path[0]) return 0;
    int w = 0, h = 0;
    if (app->window) SDL_GetWindowSize(app->window, &w, &h);
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fprintf(f, "{\n");
    fprintf(f, "  \"version\": \"%s\",\n", APP_VERSION);
    fprintf(f, "  \"window_w\": %d,\n  \"window_h\": %d,\n", w, h);
    fprintf(f, "  \"brightness\": %.3f,\n", app->ui.brightness);
    fprintf(f, "  \"show_values\": %d,\n", app->render->show_values ? 1 : 0);
    fprintf(f, "  \"show_voltages\": %d,\n", app->render->show_voltages ? 1 : 0);
    fprintf(f, "  \"show_current\": %d,\n", app->render->show_current ? 1 : 0);
    fprintf(f, "  \"scope_sliders\": %d,\n", app->ui.scope_sliders ? 1 : 0);
    fprintf(f, "  \"show_grid\": %d,\n", app->render->show_grid ? 1 : 0);
    fprintf(f, "  \"left_tab\": %d,\n", app->ui.left_tab);
    fprintf(f, "  \"properties_collapsed\": %d,\n", app->ui.properties_collapsed ? 1 : 0);
    fprintf(f, "  \"speed\": %.3f,\n", app->ui.speed_value);
    fprintf(f, "  \"light_level\": %.3f,\n", g_environment.light_level);
    fprintf(f, "  \"temperature\": %.1f\n", g_environment.temperature);
    fprintf(f, "}\n");
    fclose(f);
    return 1;
}
