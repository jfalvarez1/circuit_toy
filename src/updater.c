/**
 * Auto-update (Windows): asks the GitHub releases API for the latest tag in a background
 * thread; if it is newer than APP_VERSION the UI shows an "Update" button. Installing
 * downloads the release zip and hands over to a small PowerShell script that waits for this
 * process to exit, extracts the zip over the install directory and relaunches the app.
 *
 * Network access goes through PowerShell (Invoke-RestMethod / Invoke-WebRequest) so the app
 * itself links nothing new. Set CIRCUIT_TOY_NO_UPDATE=1 or pass --no-update-check to skip.
 */
#include "updater.h"
#include "version.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#endif

static const char *REPO = "jfalvarez1/circuit_toy";

typedef struct {
    UpdaterState *st;
} CheckArgs;

/* parse "v3.4.1" / "3.4.1" into 3 ints; returns 0 on failure */
static int parse_semver(const char *s, int v[3]) {
    while (*s && !isdigit((unsigned char)*s)) s++;
    v[0] = v[1] = v[2] = 0;
    return sscanf(s, "%d.%d.%d", &v[0], &v[1], &v[2]) >= 2;
}

int updater_is_newer(const char *tag, const char *current) {
    int a[3], b[3];
    if (!parse_semver(tag, a) || !parse_semver(current, b)) return 0;
    for (int i = 0; i < 3; i++) { if (a[i] > b[i]) return 1; if (a[i] < b[i]) return 0; }
    return 0;
}

static int run_capture(const char *cmd, char *out, size_t n) {
    out[0] = 0;
#ifdef _WIN32
    FILE *p = _popen(cmd, "r");
#else
    FILE *p = popen(cmd, "r");
#endif
    if (!p) return 0;
    size_t got = fread(out, 1, n - 1, p);
    out[got] = 0;
#ifdef _WIN32
    _pclose(p);
#else
    pclose(p);
#endif
    /* trim */
    while (got > 0 && isspace((unsigned char)out[got - 1])) out[--got] = 0;
    return got > 0;
}

static int check_thread(void *arg) {
    CheckArgs *ca = (CheckArgs *)arg;
    UpdaterState *st = ca->st;
    char cmd[900], tag[128];
    /* Report the tag ONLY if the release already carries the zip we would download. A tag
       exists from the moment it is pushed; the binary is attached when CI has finished
       building and testing it, a quarter of an hour later. Offering an update in that window
       means quitting the app to fetch a file that is not there - which is how someone on
       v3.14.0 ended up with a window that closed and never came back.

       No pipeline in the script on purpose: the whole command goes through cmd.exe, and a
       vertical bar inside it does not survive the trip. */
    snprintf(cmd, sizeof cmd,
             "powershell -NoProfile -NonInteractive -Command \"try { [Net.ServicePointManager]::SecurityProtocol = 'Tls12'; "
             "$r = Invoke-RestMethod -Uri 'https://api.github.com/repos/%s/releases/latest' -Headers @{'User-Agent'='circuit-playground'} -TimeoutSec 8; "
             "$want = 'circuit-playground-windows-' + $r.tag_name + '.zip'; $hit = 0; "
             "foreach ($a in $r.assets) { if ($a.name -eq $want) { $hit = 1 } }; "
             "if ($hit) { $r.tag_name } else { '' } } catch { '' }\"",
             REPO);
    int ok = run_capture(cmd, tag, sizeof tag);
    SDL_LockMutex(st->lock);
    if (ok && tag[0]) {
        strncpy(st->latest_tag, tag, sizeof st->latest_tag - 1);
        const char *fake = getenv("CIRCUIT_TOY_FAKE_VERSION");   /* test hook: pretend to be an older build */
        st->available = updater_is_newer(tag, fake ? fake : APP_VERSION);
        st->checked = 1;
    } else {
        st->checked = 1;
        st->failed = 1;
    }
    SDL_UnlockMutex(st->lock);
    free(ca);
    return 0;
}

void updater_init(UpdaterState *st) {
    memset(st, 0, sizeof *st);
    st->lock = SDL_CreateMutex();
}

void updater_shutdown(UpdaterState *st) {
    if (st->thread) SDL_WaitThread(st->thread, NULL);
    if (st->lock) SDL_DestroyMutex(st->lock);
    st->thread = NULL; st->lock = NULL;
}

void updater_check_async(UpdaterState *st) {
    if (st->thread || (getenv("CIRCUIT_TOY_NO_UPDATE") && !getenv("CIRCUIT_TOY_FAKE_VERSION"))) return;
    CheckArgs *ca = (CheckArgs *)calloc(1, sizeof *ca);
    ca->st = st;
    st->thread = SDL_CreateThread(check_thread, "update-check", ca);
}

int updater_available(UpdaterState *st, char *tag, size_t n) {
    if (!st->lock) return 0;
    SDL_LockMutex(st->lock);
    int a = st->available;
    if (tag) { strncpy(tag, st->latest_tag, n - 1); tag[n - 1] = 0; }
    SDL_UnlockMutex(st->lock);
    return a;
}

void updater_wait(UpdaterState *st) {
    if (st->thread) { SDL_WaitThread(st->thread, NULL); st->thread = NULL; }
}

int updater_checked(UpdaterState *st, int *failed) {
    if (!st->lock) return 0;
    SDL_LockMutex(st->lock);
    int c = st->checked; if (failed) *failed = st->failed;
    SDL_UnlockMutex(st->lock);
    return c;
}

/* Write and launch the PowerShell updater; returns 1 if it started (caller should quit). */
int updater_install(UpdaterState *st, char *msg, size_t msgn) {
#ifndef _WIN32
    snprintf(msg, msgn, "Auto-update is Windows only; download the release from GitHub");
    return 0;
#else
    char tag[128];
    if (!updater_available(st, tag, sizeof tag)) { snprintf(msg, msgn, "No update available"); return 0; }
    char exe[MAX_PATH];
    GetModuleFileNameA(NULL, exe, MAX_PATH);
    char dir[MAX_PATH];
    strncpy(dir, exe, MAX_PATH); dir[MAX_PATH - 1] = 0;
    char *slash = strrchr(dir, '\\'); if (slash) *slash = 0;
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    char script[MAX_PATH];
    snprintf(script, sizeof script, "%scircuit_toy_update.ps1", tmp);
    FILE *f = fopen(script, "w");
    if (!f) { snprintf(msg, msgn, "Cannot write %s", script); return 0; }
    fprintf(f,
        "$ErrorActionPreference = 'Stop'\n"
        "[Net.ServicePointManager]::SecurityProtocol = 'Tls12'\n"
        "$tag = '%s'\n"
        "$dir = '%s'\n"
        "$exe = '%s'\n"
        "$pid0 = %lu\n"
        "$zip = Join-Path $env:TEMP ('circuit-playground-' + $tag + '.zip')\n"
        "$url = 'https://github.com/%s/releases/download/' + $tag + '/circuit-playground-windows-' + $tag + '.zip'\n"
        /* The app has already quit by the time this runs, so every failure path has to put it
           back. A release with no zip attached, a network that drops, a locked file: all of
           them end with the version that was working being started again. */
        "try {\n"
        "  Write-Host \"Downloading $url\"\n"
        "  Invoke-WebRequest -Uri $url -OutFile $zip -Headers @{'User-Agent'='circuit-playground'}\n"
        "  Write-Host 'Waiting for Circuit Playground to close...'\n"
        "  while (Get-Process -Id $pid0 -ErrorAction SilentlyContinue) { Start-Sleep -Milliseconds 300 }\n"
        "  $stage = Join-Path $env:TEMP ('circuit-playground-' + $tag)\n"
        "  if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }\n"
        "  Expand-Archive -Path $zip -DestinationPath $stage -Force\n"
        "  $src = $stage\n"
        "  $inner = Get-ChildItem -Path $stage -Directory | Select-Object -First 1\n"
        "  if (-not (Test-Path (Join-Path $stage 'circuit-playground.exe')) -and $inner) { $src = $inner.FullName }\n"
        "  if (-not (Test-Path (Join-Path $src 'circuit-playground.exe'))) { throw 'the zip has no circuit-playground.exe in it' }\n"
        "  Copy-Item -Path (Join-Path $src '*') -Destination $dir -Recurse -Force\n"
        "  Write-Host \"Updated to $tag - relaunching\"\n"
        "} catch {\n"
        "  Write-Host \"Update to $tag failed: $_\"\n"
        "  Write-Host 'Starting the version you had.'\n"
        "  while (Get-Process -Id $pid0 -ErrorAction SilentlyContinue) { Start-Sleep -Milliseconds 300 }\n"
        "}\n"
        "Start-Process -FilePath $exe -WorkingDirectory $dir\n",
        tag, dir, exe, (unsigned long)GetCurrentProcessId(), REPO);
    fclose(f);
    char cmd[1024];
    snprintf(cmd, sizeof cmd, "powershell -NoProfile -ExecutionPolicy Bypass -File \"%s\"", script);
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    memset(&pi, 0, sizeof pi);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        snprintf(msg, msgn, "Could not start the updater (PowerShell)");
        return 0;
    }
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    snprintf(msg, msgn, "Updating to %s: the app will close and relaunch", tag);
    return 1;
#endif
}
