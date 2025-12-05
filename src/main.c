/**
 * Circuit Playground - Main Entry Point
 * A circuit simulator inspired by The Powder Toy
 */

#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <SDL_main.h>
#include "app.h"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("Circuit Playground v1.0.0\n");
    printf("A circuit simulator inspired by The Powder Toy\n\n");

    // Initialize SDL (including audio for microphone support)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0) {
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
