/**
 * Circuit Playground - build a circuit from a written-down one
 *
 * The companion course does not hand a reader a netlist file; it hands them a table of parts,
 * values and connections. This turns that table - or anything shaped like it - into a circuit
 * on the canvas, one line per part:
 *
 *     R1   in vm1     10k
 *     RF   vm1 out    3k
 *     VIN  in 0       DC 1.0
 *     E1   out 0 in vm1   100k
 *
 * Node names are the linking mechanism: anything sharing a name is one wire, and 0, GND and
 * GROUND are the reference. Nothing is routed, because nothing needs to be - the names do the
 * joining, which is exactly what the table means.
 *
 * This is deliberately not a SPICE reader. It accepts the subset the course actually uses and
 * says clearly what it could not place, rather than pretending to a compatibility it has not
 * got. src/spice.c remains the importer for vendor .SUBCKT model libraries.
 */

#ifndef NETLIST_H
#define NETLIST_H

#include "circuit.h"

/* Place every part the text describes into `circuit`, which is NOT cleared first - so a table
   can be pasted into an existing sheet. Returns how many parts were placed, or -1 if nothing
   could be. `err` gets a line naming what was skipped and why. */
int netlist_build(Circuit *circuit, const char *text, char *err, size_t err_size);

/* The same, reading the text from a file. */
int netlist_build_file(Circuit *circuit, const char *path, char *err, size_t err_size);

#endif // NETLIST_H
