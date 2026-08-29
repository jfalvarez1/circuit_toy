/*
 * The value label a component would draw next to itself, and where it goes, in world
 * coordinates. Returns false for the parts that do not carry one.
 */

#ifndef LABEL_H
#define LABEL_H

#include <stdbool.h>
#include <stddef.h>
#include "types.h"

bool render_component_value_label(const Component *comp, char *out, size_t outn,
                                  float *out_x, float *out_y);

#endif
