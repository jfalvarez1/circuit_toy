#ifndef CRASHLOG_H
#define CRASHLOG_H

/* A trail of where start-up got to, and why it stopped.
 *
 * An app that closes instantly leaves nothing behind on the machine it closed on: no console,
 * no window, no message. This writes a line per milestone to a file next to the settings, in
 * append mode and flushed every time, so the last line before a crash says what it was doing.
 * On Windows it also catches the exception and records the code and address.
 *
 * The file lives at <SDL pref path>/crash.log, which is
 *   %APPDATA%\circuit_toy\circuit-playground\crash.log
 * and the path is printed on start-up and by --where.
 */

void        crashlog_init(const char *version);
void        crashlog_note(const char *fmt, ...);   /* timestamped, flushed immediately */
void        crashlog_ok(void);                     /* records a clean exit */
const char *crashlog_path(void);
void        crashlog_dump_last(void);              /* print the file to stdout (--crashlog) */

#endif
