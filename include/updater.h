#ifndef UPDATER_H
#define UPDATER_H

#include <SDL.h>
#include <stddef.h>

typedef struct {
    SDL_mutex *lock;
    SDL_Thread *thread;
    char latest_tag[128];
    int checked, failed, available;
} UpdaterState;

void updater_init(UpdaterState *st);
void updater_shutdown(UpdaterState *st);
void updater_check_async(UpdaterState *st);                    // background GitHub "latest release" query
int  updater_available(UpdaterState *st, char *tag, size_t n); // 1 if a newer tag than APP_VERSION exists
int  updater_checked(UpdaterState *st, int *failed);
void updater_wait(UpdaterState *st);                          // block until the background check finished
int  updater_install(UpdaterState *st, char *msg, size_t msgn); // download + swap + relaunch; 1 = app should quit
int  updater_is_newer(const char *tag, const char *current);   // semver compare ("v3.4.1" vs "3.4.0")

#endif
