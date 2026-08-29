/*
 * The value label a component would draw next to itself, and where it goes, in world
 * coordinates. Returns false for the parts that do not carry one.
 */

#ifndef LABEL_H
#define LABEL_H

#include <stdbool.h>
#include <stddef.h>
#include "types.h"

/* Canvas annotations wrap at this many characters: a note is a paragraph the shape of the
   circuit, not one line wider than it. */
#define CANVAS_TEXT_WRAP 68
#define CANVAS_TEXT_MAX_LINES 12

int label_wrap(const char *s, int max_chars, int *starts, int *lens, int max_lines);

bool render_component_value_label(const Component *comp, char *out, size_t outn,
                                  float *out_x, float *out_y);

#endif
