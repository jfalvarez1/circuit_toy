/**
 * Circuit Playground - Main Entry Point
 * A circuit simulator inspired by The Powder Toy
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_main.h>
#include "app.h"
#include "file_io.h"
#include "simulation.h"
#include "circuits.h"
#include "spice.h"
#include "version.h"
#include "updater.h"
#include "ui.h"

static bool rects_overlap(const Rect *a, const Rect *b) {
    return a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h;
}

/* Headless UI layout self-check: no SDL window needed. */
static int layout_test(void) {
    int fails = 0;
    UIState *ui = calloc(1, sizeof *ui);
    ui_init(ui);
    static const int sizes[][2] = { {1024, 600}, {1280, 720}, {1920, 1080} };
    for (unsigned k = 0; k < sizeof sizes / sizeof sizes[0]; k++) {
        ui->window_width = sizes[k][0]; ui->window_height = sizes[k][1];
        for (int tab = 0; tab < 3; tab++) {
            ui->scope_ctl_tab = tab;
            ui_update_layout(ui);
            Button *b[SCOPE_BTN_N]; ui_scope_buttons(ui, b);
            int visible = 0;
            for (int i = 0; i < SCOPE_BTN_N; i++) {
                if (b[i]->bounds.w <= 0) continue;
                visible++;
                if (b[i]->bounds.x < ui->scope_rect.x || b[i]->bounds.x + b[i]->bounds.w > ui->scope_rect.x + ui->scope_rect.w + 2) {
                    printf("[FAIL] layout %dx%d tab %d: scope button '%s' outside the scope width\n", sizes[k][0], sizes[k][1], tab, b[i]->label); fails++;
                }
                for (int j = i + 1; j < SCOPE_BTN_N; j++) {
                    if (b[j]->bounds.w <= 0) continue;
                    if (rects_overlap(&b[i]->bounds, &b[j]->bounds)) { printf("[FAIL] layout %dx%d tab %d: '%s' overlaps '%s'\n", sizes[k][0], sizes[k][1], tab, b[i]->label, b[j]->label); fails++; }
                }
            }
            if (ui->scope_buttons_bottom > ui->window_height - STATUSBAR_HEIGHT) { printf("[FAIL] layout %dx%d: scope buttons below the status bar\n", sizes[k][0], sizes[k][1]); fails++; }
            printf("[ OK ] layout %dx%d tab %d: %d visible scope buttons, none overlap, bottom %d\n", sizes[k][0], sizes[k][1], tab, visible, ui->scope_buttons_bottom);
        }
    }
    /* ---- pop-out front panel: every knob is hit-testable and every knob moves something ----
       The panel is laid out in the pop-out window's coordinates, so this drives
       ui_layout_scope_panel directly with a window size and then does what a drag does. */
    {
        const int PW = 1120, PH = 700;
        ui->scope_panel_active = true;
        ui->scope_rect = (Rect){18, 30, PW - 250 - 36, PH - 130};
        ui_layout_scope_panel(ui, PW, PH);
        ui->scope_num_channels = 4;
        ui->scope_selected_channel = 0;
        ui->scope_volt_div = 1.0;
        ui->scope_time_div = 1e-3;
        ui->trigger_level = 0.0;
        ui->scope_channels[0].offset = 0.0;
        ui_set_brightness(ui, 1.0f);

        int knobs_ok = 0;
        for (int k = 0; k < KNOB_COUNT; k++) {
            ScopeKnob *kn = &ui->scope_knobs[k];
            /* the knob must be inside the panel column and clear of the screen */
            if (kn->cx - kn->r < PW - 250 || kn->cx + kn->r > PW ||
                kn->cy - kn->r < 0 || kn->cy + kn->r > PH) {
                printf("[FAIL] knob %d outside the panel column\n", k); fails++;
            }
            if (ui_scope_knob_at(ui, kn->cx, kn->cy) != k) {
                printf("[FAIL] knob %d does not hit-test at its own centre\n", k); fails++;
            }
            for (int j = 0; j < KNOB_COUNT; j++) {
                if (j == k) continue;
                ScopeKnob *o = &ui->scope_knobs[j];
                int dx = kn->cx - o->cx, dy = kn->cy - o->cy;
                if (dx * dx + dy * dy < (kn->r + o->r) * (kn->r + o->r)) {
                    printf("[FAIL] knobs %d and %d overlap\n", k, j); fails++;
                }
            }
            knobs_ok++;
        }

        /* volts/div and time/div are detented: a long drag has to emit an action */
        int act = 0;
        for (int i = 0; i < 40 && !act; i++) act = ui_scope_knob_drag(ui, KNOB_VOLTS, -1);
        if (act != UI_ACTION_SCOPE_VOLT_UP) { printf("[FAIL] VOLTS/DIV knob emitted %d, not VOLT_UP\n", act); fails++; }
        act = 0;
        for (int i = 0; i < 40 && !act; i++) act = ui_scope_knob_drag(ui, KNOB_VOLTS, +1);
        if (act != UI_ACTION_SCOPE_VOLT_DOWN) { printf("[FAIL] VOLTS/DIV knob down emitted %d\n", act); fails++; }
        act = 0;
        for (int i = 0; i < 40 && !act; i++) act = ui_scope_knob_drag(ui, KNOB_TIME, -1);
        if (act != UI_ACTION_SCOPE_TIME_UP) { printf("[FAIL] TIME/DIV knob emitted %d, not TIME_UP\n", act); fails++; }
        act = 0;
        for (int i = 0; i < 40 && !act; i++) act = ui_scope_knob_drag(ui, KNOB_CHANNEL, -1);
        if (act != UI_ACTION_SCOPE_TRIG_CH) { printf("[FAIL] CHANNEL knob emitted %d, not TRIG_CH\n", act); fails++; }

        /* the continuous knobs move their own value, and dragging up increases it */
        double t0 = ui->trigger_level;
        ui_scope_knob_drag(ui, KNOB_TRIGGER, -30);
        if (!(ui->trigger_level > t0)) { printf("[FAIL] TRIG LEVEL knob did not rise on an upward drag\n"); fails++; }
        ui_scope_knob_drag(ui, KNOB_TRIGGER, +60);
        if (!(ui->trigger_level < t0)) { printf("[FAIL] TRIG LEVEL knob did not fall on a downward drag\n"); fails++; }

        double o0 = ui->scope_channels[0].offset;
        ui_scope_knob_drag(ui, KNOB_POSITION, -30);
        if (!(ui->scope_channels[0].offset > o0)) { printf("[FAIL] POSITION knob did not move the channel offset\n"); fails++; }

        float b0 = ui->brightness;
        ui_scope_knob_drag(ui, KNOB_INTENSITY, +200);
        if (!(ui->brightness < b0)) { printf("[FAIL] INTENSITY knob did not dim\n"); fails++; }
        ui_scope_knob_drag(ui, KNOB_INTENSITY, -400);
        if (!(ui->brightness > 0.9f)) { printf("[FAIL] INTENSITY knob did not come back up\n"); fails++; }

        /* a click away from every knob must not grab one */
        if (ui_scope_knob_at(ui, ui->scope_rect.x + 10, ui->scope_rect.y + 10) != -1) {
            printf("[FAIL] a click on the screen grabbed a knob\n"); fails++;
        }
        ui->scope_panel_active = false;
        if (ui_scope_knob_at(ui, ui->scope_knobs[0].cx, ui->scope_knobs[0].cy) != -1) {
            printf("[FAIL] knobs are live while the panel is not shown (docked scope)\n"); fails++;
        }
        printf("[ OK ] scope panel: %d knobs laid out, hit-tested and driven\n", knobs_ok);
        ui_set_brightness(ui, 1.0f);
    }

    /* every template is in the Circuits palette, exactly once */
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        int n = 0;
        for (int i = 0; i < ui->num_circuit_items; i++) if (ui->circuit_items[i].circuit_type == t) n++;
        if (n != 1) { const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t); printf("[FAIL] template %s appears %d times in the palette\n", ti ? ti->name : "?", n); fails++; }
    }
    printf("[ OK ] %d templates, %d circuit palette items\n", CIRCUIT_TYPE_COUNT - 1, ui->num_circuit_items);
    /* every part has a category before Circuits, and every part category has at least one item */
    int per_cat[PCAT_COUNT] = {0};
    for (int i = 0; i < ui->num_palette_items; i++) {
        if (ui->palette_items[i].category >= PCAT_CIRCUITS) { printf("[FAIL] palette item '%s' has no part category\n", ui->palette_items[i].label); fails++; }
        else per_cat[ui->palette_items[i].category]++;
        if (ui->palette_items[i].label && strlen(ui->palette_items[i].label) > 9) { printf("[FAIL] palette label '%s' too long for a 60 px button\n", ui->palette_items[i].label); fails++; }
    }
    for (int c = 0; c < PCAT_CIRCUITS; c++) if (per_cat[c] == 0) { printf("[FAIL] part category %d (%s) is empty\n", c, ui->categories[c].name); fails++; }
    for (int i = 0; i < ui->num_circuit_items; i++)
        if (strlen(ui->circuit_items[i].label) > 9) { printf("[FAIL] circuit label '%s' too long\n", ui->circuit_items[i].label); fails++; }
    /* action id sanity */
    if (UI_ACTION_SCOPE_STACK == UI_ACTION_SPOTLIGHT || UI_ACTION_SCOPE_TRACK == UI_ACTION_SPOTLIGHT) { printf("[FAIL] UI action id collision\n"); fails++; }
    printf("[ OK ] %d palette items in %d categories\n", ui->num_palette_items, (int)PCAT_CIRCUITS);
    printf("%d layout checks failed\n", fails);
    free(ui);
    return fails;
}

static void usage(void) {
    printf("Options:\n"
           "  --template NAME      place a circuit template (short or full name) and start it\n"
           "  --size WxH           window size\n"
           "  --shot FILE.bmp      save the window at frame N (see --frame, default 90)\n"
           "  --frame N            frame index for --shot / first frame of --record\n"
           "  --record DIR N EVERY save N frames, one every EVERY frames, as DIR/frame_XXX.bmp\n"
           "  --scroll PX          scroll the left palette by PX pixels (screenshots)\n"
           "  --keys S FRAME EVERY type S one char every EVERY frames from FRAME (^ opens Spotlight, | is Enter)\n"
           "  --xy FILE            load 'x y' coordinate pairs into the X-Y Plotter template\n"
           "  --tab parts|circuits left panel tab\n"
           "  --exit               quit when the shot / recording is done\n"
           "  --no-update-check    do not query GitHub for a newer release (also CIRCUIT_TOY_NO_UPDATE=1)\n"
           "  --version            print the version and exit\n"
           "  --update-check       query the latest GitHub release and exit; --update-now also installs it\n"
           "  --layout-test        headless UI layout self-check (no window)\n");
}

int main(int argc, char *argv[]) {
    const char *cli_template = NULL, *cli_shot = NULL, *cli_record = NULL, *cli_size = NULL;
    int cli_frame = 90, cli_rec_n = 0, cli_rec_every = 1, cli_scroll = -1, cli_tab = -1; bool cli_exit = false, no_update = false;
    const char *cli_keys = NULL; int cli_keys_frame = 30, cli_keys_every = 6;
    const char *cli_xy = NULL;
    bool cli_popout = false;
    const char *cli_spice = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--template") && i + 1 < argc) cli_template = argv[++i];
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) cli_shot = argv[++i];
        else if (!strcmp(argv[i], "--frame") && i + 1 < argc) cli_frame = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) cli_size = argv[++i];
        else if (!strcmp(argv[i], "--record") && i + 3 < argc) { cli_record = argv[++i]; cli_rec_n = atoi(argv[++i]); cli_rec_every = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--exit")) cli_exit = true;
        else if (!strcmp(argv[i], "--no-update-check")) no_update = true;
        else if (!strcmp(argv[i], "--version")) { printf("%s\n", APP_VERSION); return 0; }
        else if (!strcmp(argv[i], "--update-check") || !strcmp(argv[i], "--update-now")) {
            UpdaterState st; updater_init(&st); updater_check_async(&st); updater_wait(&st);
            char tag[128]; int failed = 0; updater_checked(&st, &failed);
            int avail = updater_available(&st, tag, sizeof tag);
            printf("installed %s, latest %s -> %s\n", getenv("CIRCUIT_TOY_FAKE_VERSION") ? getenv("CIRCUIT_TOY_FAKE_VERSION") : APP_VERSION,
                   failed ? "(query failed)" : tag, avail ? "update available" : "up to date");
            int rc = 0;
            if (!strcmp(argv[i], "--update-now")) { char msg[200]; rc = updater_install(&st, msg, sizeof msg) ? 0 : 3; printf("%s\n", msg); }
            updater_shutdown(&st);
            return failed ? 2 : rc;
        }
        else if (!strcmp(argv[i], "--scroll") && i + 1 < argc) cli_scroll = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--keys") && i + 3 < argc) { cli_keys = argv[++i]; cli_keys_frame = atoi(argv[++i]); cli_keys_every = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--xy") && i + 1 < argc) cli_xy = argv[++i];
        else if (!strcmp(argv[i], "--tab") && i + 1 < argc) cli_tab = !strcmp(argv[++i], "circuits") ? 1 : 0;
        else if (!strcmp(argv[i], "--popout")) cli_popout = true;
        else if (!strcmp(argv[i], "--import-spice") && i + 1 < argc) cli_spice = argv[++i];
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(); return 0; }
        else if (!strcmp(argv[i], "--layout-test")) return layout_test();
        else { fprintf(stderr, "Unknown option: %s\n", argv[i]); usage(); return 2; }
    }

    printf("Circuit Playground v%s\n", APP_VERSION);
    printf("A circuit simulator inspired by Paul Falstad's circuit.js and The Powder Toy\n\n");

    // Initialize SDL (video + timer; audio subsystem was removed with the microphone feature)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }

    // Create application
    App app = {0};
    if (!app_init(&app)) {
        fprintf(stderr, "Application initialization failed\n");
        SDL_Quit();
        return 1;
    }

    printf("Application initialized successfully\n");
    printf("Press F1 for keyboard shortcuts\n\n");

    if (cli_spice) {
        char msg[256] = "";
        int n = spice_import_file(cli_spice, msg, sizeof msg);
        if (n > 0) printf("SPICE import: %s\n", msg);
        else fprintf(stderr, "SPICE import failed: %s\n", msg);
    }
    if (cli_xy) {
        int n = arb_load_xy_file(cli_xy);
        if (n) printf("Loaded %d X-Y points from %s\n", n, cli_xy);
        else fprintf(stderr, "Could not read X-Y data from %s\n", cli_xy);
    }
    if (cli_size) {
        int w = 0, h = 0;
        if (sscanf(cli_size, "%dx%d", &w, &h) == 2 && w > 200 && h > 200) SDL_SetWindowSize(app.window, w, h);
    }
    if (cli_shot) { strncpy(app.cli_shot_path, cli_shot, sizeof app.cli_shot_path - 1); }
    if (cli_record) { strncpy(app.cli_record_dir, cli_record, sizeof app.cli_record_dir - 1); app.cli_record_frames = cli_rec_n; app.cli_record_every = cli_rec_every; }
    app.cli_shot_frame = cli_frame;
    if (cli_keys) { strncpy(app.cli_keys, cli_keys, sizeof app.cli_keys - 1); app.cli_keys_frame = cli_keys_frame; app.cli_keys_every = cli_keys_every; }
    app.skip_update_check = no_update || cli_shot || cli_record;   // scripted runs never phone home
    app_update_check(&app);
    if (cli_scroll >= 0) app.ui.palette_scroll_offset = cli_scroll;
    if (cli_tab >= 0) app.ui.left_tab = cli_tab;
    app.cli_exit = cli_exit;
    if (cli_popout) app_scope_popout(&app, true);   /* --shot then also writes <name>_scope.bmp */
    if (cli_template) {
        CircuitTemplateType found = CIRCUIT_NONE;
        for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
            const CircuitTemplateInfo *info = circuit_template_get_info((CircuitTemplateType)t);
            if (info && (!_stricmp(info->name, cli_template) || !_stricmp(info->short_name, cli_template))) { found = (CircuitTemplateType)t; break; }
        }
        if (found == CIRCUIT_NONE) {
            fprintf(stderr, "Unknown template '%s'. Available:\n", cli_template);
            for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
                const CircuitTemplateInfo *info = circuit_template_get_info((CircuitTemplateType)t);
                if (info) fprintf(stderr, "  %-8s %s\n", info->short_name, info->name);
            }
        } else {
            // a few frames first so the resize event lands and the canvas rect is laid out
            for (int k = 0; k < 4; k++) { app_handle_events(&app); app_update(&app); app_render(&app); SDL_Delay(16); }
            app_place_template_centered(&app, found);
        }
    }

    // Main loop
    while (app.running) {
        app_handle_events(&app);
        app_update(&app);
        app_render(&app);

        // Cap frame rate to ~60 FPS
        SDL_Delay(16);
    }

    // Cleanup
    app_shutdown(&app);
    SDL_Quit();

    printf("Application closed\n");
    return 0;
}
