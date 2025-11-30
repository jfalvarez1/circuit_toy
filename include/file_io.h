/**
 * Circuit Playground - File I/O
 */

#ifndef FILE_IO_H
#define FILE_IO_H

#include "types.h"
#include "circuit.h"

// File format magic number
#define CIRCUIT_FILE_MAGIC 0x43495243  // "CIRC"
#define CIRCUIT_FILE_VERSION 1

// Save circuit to file (binary format)
bool file_save_circuit(Circuit *circuit, const char *filename);

// Load circuit from file
bool file_load_circuit(Circuit *circuit, const char *filename);

// Export circuit as JSON (human-readable)
bool file_export_json(Circuit *circuit, const char *filename);

// Import circuit from JSON
bool file_import_json(Circuit *circuit, const char *filename);

// Get last error message
const char *file_get_error(void);

#endif // FILE_IO_H
