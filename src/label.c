/*
 * Circuit Playground - the value label a component draws beside itself
 *
 * Split out of render.c so the headless tools can ask what a part's label says and where it
 * goes without linking SDL. tools/template_smoke.c uses it to audit text that lands on other
 * text; keeping one copy is the point, since a second one would drift.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "types.h"
#include "component.h"
#include "label.h"

/* Value label next to a component: R / C / L / source volts / line miles / transformer ratio ...
   The text and where it goes are worked out here, without touching the renderer, so that the
   schematic audit in tools/template_smoke.c can ask for exactly what will be drawn instead of
   keeping its own copy of this switch and drifting away from it. */
/* Schematic notation: 10k, 4.7k, 100nF, 2.2uF. format_engineering writes "10.0 kOhm", which is
   nine characters for a value a schematic writes in three - and at 80 px between parts, two of
   those labels side by side overlap. A resistor carries no unit at all, which is what every
   printed schematic does; a capacitor and an inductor keep one letter. */
static void compact_eng(double v, const char *unit, char *buf, size_t n) {
    static const struct { double e; const char *p; } P[] = {
        {1e12,"T"},{1e9,"G"},{1e6,"M"},{1e3,"k"},{1,""},{1e-3,"m"},{1e-6,"u"},{1e-9,"n"},{1e-12,"p"}
    };
    double a = fabs(v);
    if (a == 0) { snprintf(buf, n, "0%s", unit); return; }
    for (size_t i = 0; i < sizeof P / sizeof P[0]; i++) {
        if (a >= P[i].e * 0.999) {
            double s = v / P[i].e;
            char num[24];
            if (fabs(s) < 10)       snprintf(num, sizeof num, "%.2f", s);
            else if (fabs(s) < 100) snprintf(num, sizeof num, "%.1f", s);
            else                    snprintf(num, sizeof num, "%.0f", s);
            char *dot = strchr(num, '.');           /* 10.00 -> 10, 4.70 -> 4.7 */
            if (dot) {
                char *e = num + strlen(num) - 1;
                while (e > num && *e == '0') *e-- = '\0';
                if (*e == '.') *e = '\0';
            }
            snprintf(buf, n, "%s%s%s", num, P[i].p, unit);
            return;
        }
    }
    snprintf(buf, n, "%.2g%s", v, unit);
}

/* How a canvas annotation is broken into lines.
   The notes are written as one long sentence each, and at 11 px a character a 120-character
   line is 1300 px wide - wider than the circuit it describes, so reading the note means zooming
   out until the schematic is too small to read. Wrapping at a fixed column turns that into a
   paragraph the same shape as the circuit. The renderer and the geometry audit both call this,
   so what is measured is what is drawn. */
int label_wrap(const char *s, int max_chars, int *starts, int *lens, int max_lines) {
    int n = 0, i = 0, len = (int)strlen(s);
    if (max_chars < 8) max_chars = 8;
    while (i < len && n < max_lines) {
        int remaining = len - i;
        if (remaining <= max_chars) {
            starts[n] = i; lens[n] = remaining; n++;
            break;
        }
        /* break on the last space that fits; if there is none, break at the column */
        int brk = -1;
        for (int k = max_chars; k > 0; k--)
            if (s[i + k] == ' ') { brk = k; break; }
        if (brk <= 0) brk = max_chars;
        starts[n] = i; lens[n] = brk; n++;
        i += brk;
        while (s[i] == ' ') i++;    /* the space itself is not drawn at the start of a line */
    }
    return n;
}

/* The probe's voltage readout. Here rather than in render.c because --geom-test measures the box
   this text occupies and template_smoke does not link the renderer - the same reason
   render_component_value_label lives here. What is checked is then what is drawn. */
void render_volt_str(char *out, size_t n, double v) {
    if (fabs(v) >= 1000.0) snprintf(out, n, "%.4gkV", v / 1e3);
    else snprintf(out, n, "%.2fV", v);
}

/* ---------------------------------------------------------------------------------------------
   Text on the canvas, and getting a probe's readout out of the way of it.

   The audit that finds text printed over other text could only see half the text: it walked
   components, and a probe is not a component. A probe draws its channel name by the grip and its
   live voltage at tip + (10, 10), and that put "0.09V" straight through the coupling capacitor's
   "10uF" on Common Emitter - on 47 of 188 templates, once the audit could finally see it.

   The readout is the thing that gives way. A value label is placed hard against the part it
   belongs to and a template's annotation was written where its author wanted it; a probe's number
   only has to be near its own tip. So it tries a short ladder of offsets around the tip and takes
   the first that is clear, and if none is clear it stays where it was rather than wandering off.
   --------------------------------------------------------------------------------------------- */

static int box_hits(const CanvasTextBox *a, int n, float x0, float y0, float x1, float y1) {
    for (int i = 0; i < n; i++)   /* 2 px of slack, as the audit uses: abutting is fine */
        if (a[i].x0 < x1 - 2 && x0 < a[i].x1 - 2 && a[i].y0 < y1 - 2 && y0 < a[i].y1 - 2)
            return 1;
    return 0;
}

/* the static text: annotations, and the value labels; no probes */
static int canvas_static_boxes(const Circuit *c, CanvasTextBox *out, int max, int with_values) {
    int n = 0;
    for (int i = 0; i < c->num_components && n < max; i++) {
        Component *t = c->components[i];
        if (!t) continue;
        if (t->type != COMP_TEXT && !with_values) continue;
        if (t->type == COMP_TEXT) {
            const char *str = t->props.text.text;
            int len = (int)strlen(str);
            if (len <= 0) continue;
            int fs = t->props.text.font_size;
            if (fs < 1) fs = 1;
            if (fs > 3) fs = 3;
            /* the wrapped box: as wide as the longest line it breaks into, as tall as the
               number of lines - the same call the renderer makes */
            float cell = (float)CANVAS_TEXT_PX * fs;
            int st[CANVAS_TEXT_MAX_LINES], ln[CANVAS_TEXT_MAX_LINES];
            int nl = label_wrap(str, CANVAS_TEXT_WRAP, st, ln, CANVAS_TEXT_MAX_LINES);
            int widest = 0;
            for (int k = 0; k < nl; k++) if (ln[k] > widest) widest = ln[k];
            if (nl <= 0) { widest = len; nl = 1; }
            out[n].x0 = t->x; out[n].x1 = t->x + cell * widest;
            out[n].y0 = t->y; out[n].y1 = t->y + (float)nl * (cell + 2.0f);
            snprintf(out[n].s, sizeof out[n].s, "%.20s", str);
            out[n].is_value = 0; out[n].owner = t;
            n++;
        } else {
            char buf[96]; float lx, ly;
            if (!render_component_value_label(t, buf, sizeof buf, &lx, &ly)) continue;
            int len = (int)strlen(buf);
            if (len <= 0) continue;
            out[n].x0 = lx; out[n].x1 = lx + (float)CANVAS_TEXT_PX * len;
            out[n].y0 = ly; out[n].y1 = ly + (float)CANVAS_TEXT_PX;
            snprintf(out[n].s, sizeof out[n].s, "%.20s", buf);
            out[n].is_value = 1; out[n].owner = t;
            n++;
        }
    }
    return n;
}

/* the probe's channel name, at its home position by the grip - the far end of the handle */
static const char *probe_name_home(const Probe *p, float *x, float *y) {
    *x = p->x - 25.0f - 10.0f - 8.0f;    /* handle_dx - 10, then the renderer's -8 in screen px */
    *y = p->y - 35.0f + 5.0f;            /* handle_dy + 5 */
    return p->label[0] ? p->label : "CH?";
}

/* Take the first offset in a ladder that puts a w x h box clear of everything placed so far, and
   record it as an obstacle for whatever comes next. Falls back to the home position: a readout
   that cannot find room should stay by its own probe, not wander to the far side of the canvas. */
static void place_text(CanvasTextBox *b, int *n, int maxb, float hx, float hy, float w, float h,
                       const float *dx, const float *dy, int nd, float *ox, float *oy) {
    float bx = hx, by = hy;
    for (int k = 0; k < nd; k++) {
        float x = hx + dx[k], y = hy + dy[k];
        if (!box_hits(b, *n, x, y, x + w, y + h)) { bx = x; by = y; break; }
    }
    *ox = bx; *oy = by;
    if (*n < maxb) {
        b[*n].x0 = bx; b[*n].x1 = bx + w;
        b[*n].y0 = by; b[*n].y1 = by + h;
        b[*n].s[0] = 0; b[*n].is_value = 1; b[*n].owner = NULL;
        (*n)++;
    }
}

void probe_text_positions(const Circuit *c, float *name_x, float *name_y,
                          float *volt_x, float *volt_y) {
    /* Ladders in world units. Both start at 0,0 - the home position - so a probe with room around
       it does not move at all, and only a colliding one is pushed. The name is allowed a smaller
       range than the voltage because it belongs to the grip and reads wrong far from it. */
    static const float ndx[] = { 0,   0,   0, -14,  14,   0 };
    static const float ndy[] = { 0, -14,  14,   0,   0, -28 };
    static const float vdx[] = { 0,  0,  0, -20, -20, -20,  0, -20 };
    static const float vdy[] = { 0, -32, 16,   0, -32,  16, -44,  28 };

    if (!c) return;

    enum { MAX_B = 512 };
    static CanvasTextBox b[MAX_B];
    int n = canvas_static_boxes(c, b, MAX_B, 1);

    /* Names first, then voltages. Both in probe order, each placement becoming an obstacle for
       the next - so two probes on the same node do not step aside into the same empty space. */
    for (int i = 0; i < c->num_probes && i < MAX_PROBES; i++) {
        float hx, hy, ox, oy;
        const char *nm = probe_name_home(&c->probes[i], &hx, &hy);
        place_text(b, &n, MAX_B, hx, hy, (float)CANVAS_TEXT_PX * (float)strlen(nm),
                   (float)CANVAS_TEXT_PX, ndx, ndy, (int)(sizeof ndx / sizeof ndx[0]), &ox, &oy);
        if (name_x) name_x[i] = ox;
        if (name_y) name_y[i] = oy;
    }
    for (int i = 0; i < c->num_probes && i < MAX_PROBES; i++) {
        char vs[16];
        render_volt_str(vs, sizeof vs, c->probes[i].voltage);
        float ox, oy;
        place_text(b, &n, MAX_B, c->probes[i].x + 10.0f, c->probes[i].y + 10.0f,
                   (float)CANVAS_TEXT_PX * (float)strlen(vs), (float)CANVAS_TEXT_PX,
                   vdx, vdy, (int)(sizeof vdx / sizeof vdx[0]), &ox, &oy);
        if (volt_x) volt_x[i] = ox;
        if (volt_y) volt_y[i] = oy;
    }
}

int canvas_text_boxes(const Circuit *c, CanvasTextBox *out, int max, int with_values) {
    int n = canvas_static_boxes(c, out, max, with_values);
    if (!with_values) return n;   /* the annotations-only set is read for its owner pointers */

    float nx[MAX_PROBES], ny[MAX_PROBES], vx[MAX_PROBES], vy[MAX_PROBES];
    probe_text_positions(c, nx, ny, vx, vy);

    for (int i = 0; i < c->num_probes && i < MAX_PROBES && n < max; i++) {
        const Probe *p = &c->probes[i];

        char vs[16];
        render_volt_str(vs, sizeof vs, p->voltage);
        int vlen = (int)strlen(vs);
        if (vlen > 0) {
            out[n].x0 = vx[i]; out[n].x1 = vx[i] + (float)CANVAS_TEXT_PX * vlen;
            out[n].y0 = vy[i]; out[n].y1 = vy[i] + (float)CANVAS_TEXT_PX;
            snprintf(out[n].s, sizeof out[n].s, "%.20s", vs);
            out[n].is_value = 1; out[n].owner = NULL;
            n++;
        }
        if (n >= max) break;

        const char *nm = p->label[0] ? p->label : "CH?";
        out[n].x0 = nx[i]; out[n].x1 = nx[i] + (float)CANVAS_TEXT_PX * (float)strlen(nm);
        out[n].y0 = ny[i]; out[n].y1 = ny[i] + (float)CANVAS_TEXT_PX;
        snprintf(out[n].s, sizeof out[n].s, "%.20s", nm);
        out[n].is_value = 1; out[n].owner = NULL;
        n++;
    }
    return n;
}

bool render_component_value_label(const Component *comp, char *out, size_t outn,
                                  float *out_x, float *out_y) {
    char buf[64] = "";
    switch (comp->type) {
        case COMP_GROUND: case COMP_TEXT: case COMP_LABEL: case COMP_PIN: case COMP_SUBCIRCUIT:
        case COMP_VOLTMETER: case COMP_AMMETER: case COMP_WATTMETER: case COMP_TEST_POINT:
        case COMP_NOT_GATE: case COMP_AND_GATE: case COMP_OR_GATE: case COMP_NAND_GATE: case COMP_NOR_GATE: case COMP_XOR_GATE: case COMP_XNOR_GATE:
        case COMP_OPAMP: case COMP_OPAMP_FLIPPED: case COMP_DPDT_SWITCH:
        case COMP_DIODE: case COMP_SCHOTTKY: case COMP_NPN_BJT: case COMP_PNP_BJT: case COMP_NMOS: case COMP_PMOS:
            return false;
        /* Switches say what state they are in. A closed switch is a straight line between two
           small circles, which on a busy schematic is a wire - and then a template that says
           "open the breaker" gives you nothing to look for. */
        case COMP_SPST_SWITCH:
            snprintf(buf, sizeof buf, comp->props.switch_spst.closed ? "CLOSED" : "OPEN");
            break;
        case COMP_SPDT_SWITCH:
            snprintf(buf, sizeof buf, "POS %d", comp->props.switch_spdt.position + 1);
            break;
        case COMP_PUSH_BUTTON:
            snprintf(buf, sizeof buf, comp->props.push_button.pressed ? "PRESSED" : "BUTTON");
            break;
        case COMP_ANALOG_SWITCH:
            snprintf(buf, sizeof buf, "%s%s", comp->props.analog_switch.state ? "ON" : "OFF",
                     comp->props.analog_switch.manual ? " MAN" : "");
            break;
        case COMP_TRANSFORMER: case COMP_TRANSFORMER_CT:
            snprintf(buf, sizeof buf, "1:%.4g", comp->props.transformer.turns_ratio);
            break;
        case COMP_TLINE: {
            double R, L, C; tline_params(comp, &R, &L, &C);
            snprintf(buf, sizeof buf, "%.4gmi %.3g+j%.3g", comp->props.tline.length_mi, R, 2 * M_PI * 60 * L);
            break;
        }
        case COMP_DELAY_LINE: {
            double td = comp->props.delay_line.delay;
            if (td >= 1e-6)      snprintf(buf, sizeof buf, "%.3gohm %.4gus", comp->props.delay_line.z0, td * 1e6);
            else if (td >= 1e-9) snprintf(buf, sizeof buf, "%.3gohm %.4gns", comp->props.delay_line.z0, td * 1e9);
            else                 snprintf(buf, sizeof buf, "%.3gohm %.4gps", comp->props.delay_line.z0, td * 1e12);
            break;
        }
        case COMP_SOURCE_3PH:
            snprintf(buf, sizeof buf, "%.4gkV 3ph", comp->props.source_3ph.v_peak / 1.41421356 * 1.7320508 / 1e3);
            break;
        case COMP_AC_VOLTAGE: {
            /* "170V 60Hz", not "170.0 Vpk 60Hz": three-phase templates put these sources 100 px
               apart and the long form ran one label into the next. */
            char v[24]; compact_eng(comp->props.ac_voltage.amplitude, "V", v, sizeof v);
            snprintf(buf, sizeof buf, "%s %.4gHz", v, comp->props.ac_voltage.frequency);
            break;
        }
        case COMP_ZENER: snprintf(buf, sizeof buf, "%.3gV", comp->props.zener.vz); break;
        /* The four that appear over and over, in the notation a schematic uses */
        case COMP_RESISTOR:  compact_eng(comp->props.resistor.resistance, "", buf, sizeof buf); break;
        case COMP_CAPACITOR: compact_eng(comp->props.capacitor.capacitance, "F", buf, sizeof buf); break;
        case COMP_INDUCTOR:  compact_eng(comp->props.inductor.inductance, "H", buf, sizeof buf); break;
        case COMP_DC_VOLTAGE: compact_eng(comp->props.dc_voltage.voltage, "V", buf, sizeof buf); break;
        case COMP_DC_CURRENT: compact_eng(comp->props.dc_current.current, "A", buf, sizeof buf); break;
        default:
            component_get_value_string(comp, buf, sizeof buf);
            break;
    }
    if (comp->type == COMP_RESISTOR && comp->props.resistor.high_power && comp->props.resistor.power_dissipated > 0) {
        char pw[24]; format_engineering(comp->props.resistor.power_dissipated, "W", pw, sizeof pw);
        size_t n = strlen(buf); snprintf(buf + n, sizeof buf - n, "  %s", pw);   // show the real dissipation instead of a warning
    }
    /* A named device shows its part number - that is what a schematic is labelled with. The
       transistor symbols draw their own label, so those are already covered; everything else
       gets it in front of the value. */
    if (comp->part[0] && comp->type != COMP_NMOS && comp->type != COMP_PMOS &&
        comp->type != COMP_NPN_BJT && comp->type != COMP_PNP_BJT) {
        char merged[96];
        if (buf[0]) snprintf(merged, sizeof merged, "%s  %s", comp->part, buf);
        else        snprintf(merged, sizeof merged, "%s", comp->part);
        snprintf(buf, sizeof buf, "%s", merged);
    }
    if (!buf[0]) return false;
    const ComponentTypeInfo *info = component_get_info(comp->type);
    int rot = ((comp->rotation % 360) + 360) % 360;
    float w = info ? info->width : 60, h = info ? info->height : 40;
    float lx, ly;
    /* The side is chosen by the shape as drawn, not by the rotation field. A voltage source has
       its terminals top and bottom at rotation 0, so the old rule called it horizontal and put
       the label underneath - which is exactly where its ground symbol and the lead to it are.
       Tall parts label to the right, wide parts underneath. */
    float ew = (rot == 90 || rot == 270) ? h : w;    /* width as drawn */
    float eh = (rot == 90 || rot == 270) ? w : h;    /* height as drawn */
    if (eh > ew) {
        lx = comp->x + ew / 2 + 4;
        ly = comp->y - 5;
        /* A source stands at the edge of the schematic with its ground below and its first
           series part to the right: below is the ground lead, right is that part's own label,
           and the outside is empty. Its label goes there, right-aligned to the symbol. */
        switch (comp->type) {
            case COMP_DC_VOLTAGE: case COMP_AC_VOLTAGE: case COMP_DC_CURRENT:
            case COMP_SQUARE_WAVE: case COMP_TRIANGLE_WAVE: case COMP_SAWTOOTH_WAVE:
            case COMP_PULSE_SOURCE: case COMP_CLOCK: case COMP_NOISE_SOURCE:
            case COMP_ARB_SOURCE: case COMP_SOURCE_3PH:
                lx = comp->x - ew / 2 - 4 - (float)CANVAS_TEXT_PX * (float)strlen(buf);
                break;
            default: break;
        }
    } else {
        lx = comp->x - ew / 2 + 4;
        ly = comp->y + eh / 2 + 3;
    }
    /* A transmission line's label carries its length and its impedance, which is wider than the
       gap to the next part in a substation bay - it landed on the breaker's own CLOSED. Above
       the symbol there is nothing to compete with, and a line's rating reads naturally there. */
    if (comp->type == COMP_TLINE && rot != 90 && rot != 270)
        ly = comp->y - h / 2 - CANVAS_TEXT_PX - 3;
    if (out && outn) { snprintf(out, outn, "%s", buf); }
    if (out_x) *out_x = lx;
    if (out_y) *out_y = ly;
    return true;
}
