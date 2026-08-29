/* Start-up trail and crash capture. See include/crashlog.h for why. */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <SDL.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "crashlog.h"

static char  g_path[600];
static FILE *g_f;
static int   g_clean;

const char *crashlog_path(void) { return g_path; }

static void stamp(FILE *f) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t) fprintf(f, "%02d:%02d:%02d ", t->tm_hour, t->tm_min, t->tm_sec);
}

void crashlog_note(const char *fmt, ...) {
    if (!g_f) return;
    stamp(g_f);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_f, fmt, ap);
    va_end(ap);
    fputc('\n', g_f);
    fflush(g_f);           /* the whole point: the last line has to survive a hard stop */
}

#ifdef _WIN32
static const char *exception_name(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION:      return "access violation (read or write to a bad address)";
        case EXCEPTION_STACK_OVERFLOW:        return "stack overflow";
        case EXCEPTION_ILLEGAL_INSTRUCTION:   return "illegal instruction (a build for a newer CPU?)";
        case EXCEPTION_PRIV_INSTRUCTION:      return "privileged instruction";
        case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "integer divide by zero";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return "float divide by zero";
        case EXCEPTION_IN_PAGE_ERROR:         return "in-page error (a file or mapping went away)";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "misaligned data";
        default: return "unknown exception";
    }
}

static LONG WINAPI on_exception(EXCEPTION_POINTERS *ep) {
    if (g_f && ep && ep->ExceptionRecord) {
        DWORD code = ep->ExceptionRecord->ExceptionCode;
        crashlog_note("CRASH: %s (0x%08lx) at %p",
                      exception_name(code), (unsigned long)code, ep->ExceptionRecord->ExceptionAddress);
        if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
            crashlog_note("  %s address %p",
                          ep->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
                          (void *)ep->ExceptionRecord->ExceptionInformation[1]);
        }
        /* where the exe is loaded, so an address in the log can be turned into a line later */
        HMODULE self = GetModuleHandleA(NULL);
        crashlog_note("  module base %p", (void *)self);
        fflush(g_f);
    }
    return EXCEPTION_EXECUTE_HANDLER;   /* let it die, but with the log written */
}
#endif

static void on_signal(int sig) {
    const char *name = sig == SIGSEGV ? "SIGSEGV (bad memory access)"
                     : sig == SIGABRT ? "SIGABRT (abort - a failed assertion or a bad free)"
                     : sig == SIGFPE  ? "SIGFPE (arithmetic)"
                     : sig == SIGILL  ? "SIGILL (illegal instruction)"
                     : "signal";
    crashlog_note("CRASH: %s", name);
    _exit(3);
}

static void on_exit_hook(void) {
    if (g_f && !g_clean) crashlog_note("EXIT: process ended without reaching a clean shutdown");
    if (g_f) fclose(g_f);
    g_f = NULL;
}

void crashlog_init(const char *version) {
    char *dir = SDL_GetPrefPath("circuit_toy", "circuit-playground");
    if (dir) { snprintf(g_path, sizeof g_path, "%scrash.log", dir); SDL_free(dir); }
    if (!g_path[0]) return;

    /* keep the file from growing without bound: past 256 kB, start again */
    FILE *sz = fopen(g_path, "rb");
    if (sz) {
        fseek(sz, 0, SEEK_END);
        long n = ftell(sz);
        fclose(sz);
        if (n > 256 * 1024) remove(g_path);
    }
    g_f = fopen(g_path, "ab");
    if (!g_f) return;

    fprintf(g_f, "\n=== circuit-playground %s ===\n", version ? version : "?");
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t) fprintf(g_f, "started %04d-%02d-%02d %02d:%02d:%02d\n",
                   t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

    SDL_version c, l;
    SDL_VERSION(&c);
    SDL_GetVersion(&l);
    fprintf(g_f, "SDL built %d.%d.%d, running %d.%d.%d\n", c.major, c.minor, c.patch, l.major, l.minor, l.patch);
    fprintf(g_f, "%d logical cores, %d MB RAM\n", SDL_GetCPUCount(), SDL_GetSystemRAM());
#ifdef _WIN32
    {   /* the real build number, which GetVersionEx lies about for unmanifested apps */
        HMODULE nt = GetModuleHandleA("ntdll.dll");
        typedef LONG (WINAPI *RtlGetVersionFn)(OSVERSIONINFOEXW *);
        RtlGetVersionFn f = nt ? (RtlGetVersionFn)(void *)GetProcAddress(nt, "RtlGetVersion") : NULL;
        if (f) {
            OSVERSIONINFOEXW vi;
            memset(&vi, 0, sizeof vi);
            vi.dwOSVersionInfoSize = sizeof vi;
            if (f(&vi) == 0)
                fprintf(g_f, "Windows %lu.%lu build %lu\n",
                        (unsigned long)vi.dwMajorVersion, (unsigned long)vi.dwMinorVersion,
                        (unsigned long)vi.dwBuildNumber);
        }
        char exe[MAX_PATH] = "";
        GetModuleFileNameA(NULL, exe, MAX_PATH);
        fprintf(g_f, "exe %s\n", exe);
    }
#endif
    fflush(g_f);

#ifdef _WIN32
    SetUnhandledExceptionFilter(on_exception);
#endif
    signal(SIGSEGV, on_signal);
    signal(SIGABRT, on_signal);
    signal(SIGFPE,  on_signal);
    signal(SIGILL,  on_signal);
    atexit(on_exit_hook);
}

void crashlog_ok(void) {
    g_clean = 1;
    crashlog_note("EXIT: clean");
}

void crashlog_dump_last(void) {
    if (!g_path[0]) {
        char *dir = SDL_GetPrefPath("circuit_toy", "circuit-playground");
        if (dir) { snprintf(g_path, sizeof g_path, "%scrash.log", dir); SDL_free(dir); }
    }
    printf("crash log: %s\n\n", g_path);
    FILE *f = fopen(g_path, "rb");
    if (!f) { printf("(no log yet - it is written from the next start-up on)\n"); return; }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) fwrite(buf, 1, n, stdout);
    fclose(f);
}
