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

static void usage(void) {
    printf("Options:\n"
           "  --template NAME      place a circuit template (short or full name) and start it\n"
           "  --size WxH           window size\n"
           "  --shot FILE.bmp      save the window at frame N (see --frame, default 90)\n"
           "  --frame N            frame index for --shot / first frame of --record\n"
           "  --record DIR N EVERY save N frames, one every EVERY frames, as DIR/frame_XXX.bmp\n"
           "  --scroll PX          scroll the left palette by PX pixels (screenshots)\n"
           "  --tab parts|circuits left panel tab\n"
           "  --exit               quit when the shot / recording is done\n");
}

int main(int argc, char *argv[]) {
    const char *cli_template = NULL, *cli_shot = NULL, *cli_record = NULL, *cli_size = NULL;
    int cli_frame = 90, cli_rec_n = 0, cli_rec_every = 1, cli_scroll = -1, cli_tab = -1; bool cli_exit = false;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--template") && i + 1 < argc) cli_template = argv[++i];
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) cli_shot = argv[++i];
        else if (!strcmp(argv[i], "--frame") && i + 1 < argc) cli_frame = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) cli_size = argv[++i];
        else if (!strcmp(argv[i], "--record") && i + 3 < argc) { cli_record = argv[++i]; cli_rec_n = atoi(argv[++i]); cli_rec_every = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--exit")) cli_exit = true;
        else if (!strcmp(argv[i], "--scroll") && i + 1 < argc) cli_scroll = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--tab") && i + 1 < argc) cli_tab = !strcmp(argv[++i], "circuits") ? 1 : 0;
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(); return 0; }
        else { fprintf(stderr, "Unknown option: %s\n", argv[i]); usage(); return 2; }
    }

    printf("Circuit Playground v3.2.3\n");
    printf("A circuit simulator inspired by The Powder Toy\n\n");

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

    if (cli_size) {
        int w = 0, h = 0;
        if (sscanf(cli_size, "%dx%d", &w, &h) == 2 && w > 200 && h > 200) SDL_SetWindowSize(app.window, w, h);
    }
    if (cli_shot) { strncpy(app.cli_shot_path, cli_shot, sizeof app.cli_shot_path - 1); }
    if (cli_record) { strncpy(app.cli_record_dir, cli_record, sizeof app.cli_record_dir - 1); app.cli_record_frames = cli_rec_n; app.cli_record_every = cli_rec_every; }
    app.cli_shot_frame = cli_frame;
    if (cli_scroll >= 0) app.ui.palette_scroll_offset = cli_scroll;
    if (cli_tab >= 0) app.ui.left_tab = cli_tab;
    app.cli_exit = cli_exit;
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
