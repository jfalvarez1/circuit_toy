/**
 * Circuit Playground - File I/O
 */

#ifndef FILE_IO_H
#define FILE_IO_H

#include "types.h"
#include "circuit.h"

// File format magic number
#define CIRCUIT_FILE_MAGIC 0x43495243  // "CIRC"
/* 2: each component record carries its terminals' node ids.
   Version 1 wrote the parts, the nodes and the wires but never which node each terminal was
   attached to, so a loaded circuit only knew the connections that happen to be re-derivable
   from geometry. Anything joined logically rather than by a drawn wire - a supply's negative
   tied to the ground net, a display's common cathode, every rail a builder assigns by node id -
   came back unconnected, and 44 of the templates settled at different voltages after a save
   and a load. Version 1 files still load: their terminals are recovered by position, which is
   what they always were. */
#define CIRCUIT_FILE_VERSION 2

// Save circuit to file (binary format)
bool file_save_circuit(Circuit *circuit, const char *filename);

// Load circuit from file
bool file_load_circuit(Circuit *circuit, const char *filename);

// Export circuit as JSON (human-readable)
bool file_export_json(Circuit *circuit, const char *filename);

// Import circuit from JSON
bool file_import_json(Circuit *circuit, const char *filename);

// Export circuit as SVG (scalable vector graphics)
bool file_export_svg(Circuit *circuit, const char *filename);

// Get last error message
const char *file_get_error(void);

#endif // FILE_IO_H
