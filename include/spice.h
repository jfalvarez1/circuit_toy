#ifndef SPICE_H
#define SPICE_H

#include <stddef.h>

/* ---------------------------------------------------------------------------------------
   SPICE .SUBCKT import.

   Manufacturers publish their parts as SPICE subcircuits - a Murata capacitor model is an
   R-L-C ladder, a Wurth inductor model adds the core loss resistor - and those are the models
   a design is actually checked against. This reads the passive subset of that format and turns
   each .SUBCKT into an entry in the subcircuit library, so an imported model is placed and
   solved like any block built with Ctrl+G.

   Supported: .SUBCKT / .ENDS with a port list, R / L / C instances, X instances of another
   .SUBCKT in the same file (nested, expanded up to SUBCIRCUIT_MAX_DEPTH), `+` continuation
   lines, `*` and `;` comments, and the usual value suffixes (T G MEG K M U N P F, and the
   trap that M is milli while MEG is mega). Anything else on a line is reported and skipped
   rather than guessed at.
   --------------------------------------------------------------------------------------- */

// Import every .SUBCKT in `path` into the subcircuit library. Returns how many were added,
// or -1 on a file error. `err` receives a message: the reason on failure, or a summary of
// what was imported and what was skipped.
int spice_import_file(const char *path, char *err, size_t err_size);

// Same, from a netlist already in memory (used by the tests).
int spice_import_text(const char *text, char *err, size_t err_size);

// Parse one SPICE value with its suffix ("4.7u", "1MEG", "10n", "1e-9"). Returns false if the
// text is not a number. Exposed for the tests and for the properties panel.
bool spice_parse_value(const char *text, double *out);

#endif // SPICE_H
