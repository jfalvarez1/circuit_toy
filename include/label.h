/*
 * The value label a component would draw next to itself, and where it goes, in world
 * coordinates. Returns false for the parts that do not carry one.
 */

#ifndef LABEL_H
#define LABEL_H

#include <stdbool.h>
#include <stddef.h>
#include "types.h"
#include "circuit.h"

/* Canvas annotations wrap at this many characters: a note is a paragraph the shape of the
   circuit, not one line wider than it. */
#define CANVAS_TEXT_WRAP 68
#define CANVAS_TEXT_MAX_LINES 12

int label_wrap(const char *s, int max_chars, int *starts, int *lens, int max_lines);

/* The probe's voltage readout, exactly as the renderer draws it */
void render_volt_str(char *out, size_t n, double v);

bool render_component_value_label(const Component *comp, char *out, size_t outn,
                                  float *out_x, float *out_y);

/* Every piece of text the canvas draws, as a box in world coordinates: the annotations a template
   places, the value label each component draws beside itself, and - when with_values is set -
   each probe's channel name and live voltage.

   This lives here rather than in the audit that used to own it because the probe readout now MOVES
   to avoid what is already on the canvas. Two copies of that rule, one in the renderer and one in
   the check, would agree only until one of them changed. */
typedef struct {
    float x0, y0, x1, y1;
    char s[40];
    int is_value;
    const Component *owner;    /* NULL for probe text, which belongs to no component */
} CanvasTextBox;

int canvas_text_boxes(const Circuit *c, CanvasTextBox *out, int max, int with_values);

/* Where each probe draws its two pieces of text - the channel name by the grip and the voltage by
   the tip - in world coordinates, after stepping clear of the text already on the canvas. Every
   probe at once, because each has to avoid the ones placed before it. Any of the four arrays may
   be NULL. The renderer draws them there and the audit measures them there. */
void probe_text_positions(const Circuit *c, float *name_x, float *name_y,
                          float *volt_x, float *volt_y);

#endif
