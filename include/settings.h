#ifndef SETTINGS_H
#define SETTINGS_H

/* Persistent user preferences: %APPDATA%\circuit_toy\circuit-playground\settings.json (SDL_GetPrefPath).
   Loaded at startup, written on exit. Independent of the exe location, so updates never touch it. */

struct App;
const char *settings_path(void);           /* full path of settings.json ("" if no pref dir) */
void settings_load(struct App *app);       /* apply saved preferences to the app (missing file = defaults) */
int  settings_save(struct App *app);       /* 1 on success */

#endif
