/**
 * Circuit Playground - drawing style
 *
 * Two ways of looking at the same circuit.
 *
 * SYNTHWAVE is the program's own look and the default: magenta and cyan on deep violet, which
 * is legible as well as pretty - every channel has its own colour and the scope reads the way a
 * scope reads.
 *
 * SCHEMATIC is what you print, or paste into a report, or send to somebody who wants to look at
 * the circuit rather than at the program. Black on white, no colour at all. A schematic is a
 * document, and a document is monochrome: it photocopies, it prints on a laser printer, it
 * survives being pasted into a Word file, and nobody has to explain why the wires are cyan.
 *
 * It is a mapping applied where a colour meets the renderer rather than a second set of drawing
 * code, which is the whole reason it is only a hundred lines: every symbol, every wire, every
 * label and every panel already goes through SDL_SetRenderDrawColor or SDL_SetTextureColorMod,
 * so redefining those two - see the macros at the bottom - puts the mapping in front of all of
 * them at once. Nothing that draws had to know this exists.
 *
 * Antialiasing survives it because the feathered edges in render.c carry their coverage in
 * ALPHA, and alpha is passed through untouched. A monochrome schematic is as smooth as the
 * coloured one.
 */

#ifndef STYLE_H
#define STYLE_H

#include <SDL.h>
#include <stdint.h>

typedef enum {
    STYLE_SYNTHWAVE = 0,   /* the program's own look */
    STYLE_SCHEMATIC,       /* black on white, for printing and for sharing */
    STYLE_COUNT
} DrawStyle;

/* The style everything is currently drawn in. */
extern int g_draw_style;

/* ...and whether the thing being drawn right now is part of the SCHEMATIC.
 *
 * The style is a property of the document, not of the program. Mapping every colour in the
 * window turned the toolbar into a row of empty white boxes - the button faces and their labels
 * both landed on the same side of the ramp - and a schematic view that eats its own controls is
 * not a view of anything. So app.c arms this while the canvas is being drawn and disarms it
 * afterwards, which is the same region a canvas-only screenshot crops to: the two agree by
 * construction rather than by anyone remembering to keep them in step. */
extern int g_style_in_canvas;

const char *style_name(int style);

/* Map one colour into the current style. The identity in synthwave.
 *
 * In schematic, luminance decides what a colour is FOR, and there are three answers, not two.
 * Splitting only into paper and ink is what made the first attempt unreadable: the grid is a
 * shade brighter than the background it sits on, so a two-way split had to put it on one side or
 * the other, and either way it came out as heavy as the wires. On paper the grid is not ink. It
 * is ruling - the faint blue squares of engineering pad - and it belongs a hair off white where
 * you can align to it and read straight through it.
 *
 *   background      -> paper, pure white
 *   ruling/dividers -> a light grey, present but never competing with the drawing
 *   everything else -> ink, black
 *
 * The bands come out of the palette rather than out of taste: COLOR_BG lands at luma 27,
 * COLOR_GRID at 45, the origin cross at 61, and every wire, symbol, label and annotation colour
 * in the program is 105 or above. The thresholds sit in the gaps. */
/* What the last mapped colour came out AS, and whether a read has just happened. Between them
   they let one specific double-application be recognised and skipped; see style_set_draw_color. */
extern uint8_t g_style_mapped_rgb[3];
extern int g_style_readback;

static inline void style_map_rgb(uint8_t *r, uint8_t *g, uint8_t *b) {
    if (g_draw_style != STYLE_SCHEMATIC || !g_style_in_canvas) return;
    /* Rec. 709 luma: the eye's own weighting, so cyan reads as bright (it is) and deep blue as
       dark (it is). Using a plain average would make the violet background and the cyan wires
       nearly the same shade and the schematic would come out flat. */
    int luma = (2126 * (int)*r + 7152 * (int)*g + 722 * (int)*b) / 10000;
    int v;
    if (luma <= 34)       v = 255;                                  /* paper */
    else if (luma <= 72)  v = 236 - (luma - 34) * 28 / 38;          /* ruling: 236..208 */
    else if (luma >= 108) v = 0;                                    /* ink */
    else                  v = 170 - (luma - 72) * 130 / 36;         /* the short ramp between */
    g_style_mapped_rgb[0] = g_style_mapped_rgb[1] = g_style_mapped_rgb[2] = (uint8_t)v;
    *r = *g = *b = (uint8_t)v;
}

/* Setting a colour maps it - unless the colour is one that was just read back out.
 *
 * The mapping cannot be idempotent: it sends dark to white and light to black, so running it
 * twice sends ink back to paper. That is not hypothetical. ui_draw_text reads the current draw
 * colour and sets it again on the way to the glyph renderer, so every label on the scope went
 * (0x60,0x80,0x60) -> black -> white, and the voltage axis came out with no numbers on it.
 *
 * The fix has to be narrow, because the obvious wide one is worse. Handing back the PRE-map
 * colour from every read breaks the antialiased quads in render.c, which read the draw colour to
 * fill their vertices and then draw with it directly rather than setting it again - given the
 * unmapped colour they paint the canvas in raw synthwave over white paper. Both callers are
 * doing something reasonable; they just want different halves of the same question.
 *
 * So a read is only trusted to be the start of a restore until the very next set, and only a set
 * of exactly the colour the map last produced counts as one. Anything else - including the same
 * grey arriving on its own merits a moment later - is mapped normally. */
static inline int style_set_draw_color(SDL_Renderer *ren, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int restoring = g_style_readback;
    g_style_readback = 0;
    if (!(restoring && r == g_style_mapped_rgb[0]
                    && g == g_style_mapped_rgb[1]
                    && b == g_style_mapped_rgb[2]))
        style_map_rgb(&r, &g, &b);
    return SDL_SetRenderDrawColor(ren, r, g, b, a);
}

static inline int style_get_draw_color(SDL_Renderer *ren, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a) {
    g_style_readback = 1;
    return SDL_GetRenderDrawColor(ren, r, g, b, a);
}

/* The scope's traces are the one thing the blind map cannot handle.
 *
 * Every channel colour in the program is bright - that is the point of them - so the map sends
 * all of them to the same ink, and a two-channel plot of a filter comes out as two black curves
 * with nothing to say which one is the input. The information the colour was carrying has to
 * survive into monochrome, so channels get separated grey values instead, ordered so that the
 * first two - the pair almost every measurement actually uses - are as far apart as black and
 * mid-grey on white paper.
 *
 * These greys go to the renderer directly rather than through the map above, which would read
 * them as ordinary colours and flatten every one of them back to ink. */
static inline int style_set_trace_color(SDL_Renderer *ren, int channel,
                                        uint8_t r, uint8_t g, uint8_t b) {
    if (g_draw_style == STYLE_SCHEMATIC && g_style_in_canvas) {
        /* Evenly spaced 32 apart, ordered so the first four are the four furthest apart. */
        static const uint8_t ink[8] = { 0, 128, 64, 192, 32, 160, 96, 224 };
        uint8_t v = ink[channel & 7];
        /* Recorded as the map's output, so a band tag that reads this colour back and sets it
           again keeps the channel's ink instead of being flattened to black. */
        g_style_mapped_rgb[0] = g_style_mapped_rgb[1] = g_style_mapped_rgb[2] = v;
        g_style_readback = 0;
        return SDL_SetRenderDrawColor(ren, v, v, v, 0xff);
    }
    return style_set_draw_color(ren, r, g, b, 0xff);
}

/* Text takes the same round trip and needs the same guard.
 *
 * ui_draw_text reads the draw colour and hands it to render_text_at, which tints the glyph atlas
 * with it - so a label's colour reaches the mapping a second time through the texture path
 * rather than the draw-colour one. Without this the scope's voltage axis had every number on it
 * mapped to ink and then straight back to paper. */
static inline int style_set_texture_mod(SDL_Texture *tex, uint8_t r, uint8_t g, uint8_t b) {
    int restoring = g_style_readback;
    g_style_readback = 0;
    if (!(restoring && r == g_style_mapped_rgb[0]
                    && g == g_style_mapped_rgb[1]
                    && b == g_style_mapped_rgb[2]))
        style_map_rgb(&r, &g, &b);
    return SDL_SetTextureColorMod(tex, r, g, b);
}

/* Everything that draws goes through one of these two. Redefining them here is what makes the
   schematic style a mapping rather than a second renderer - and it is done in a header nobody
   includes by accident, with the real functions still reachable by their SDL names inside the
   shims above (which are defined before the macros). */
#define SDL_SetRenderDrawColor  style_set_draw_color
#define SDL_GetRenderDrawColor  style_get_draw_color
#define SDL_SetTextureColorMod  style_set_texture_mod

#endif /* STYLE_H */
