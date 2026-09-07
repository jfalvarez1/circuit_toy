/**
 * Circuit Playground - a written-down circuit becomes a drawn one.
 *
 * See include/netlist.h for what this is for. The short version: the course hands a reader a
 * table of parts and the nets they connect to, and this places those parts and names those
 * nets. Nothing is routed. Two terminals carrying the same net name are one node - see the
 * name pass in circuit_build_node_map - so a table transfers without a wire being drawn.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "netlist.h"
#include "component.h"

/* A MOSFET carrying W, L and LAMBDA is already 11 tokens - "M1C n1 n1 0 NMOS W 90u L 1u LAMBDA
   0.02", since '=' is a delimiter here. At the old cap of 12 the next parameter anyone added
   would have been dropped in silence, taking its default and answering a different question.
   Raised, and nl_split now REFUSES a line it could not finish rather than truncating it. */
#define NL_MAX_TOK 24

/* A value with a SPICE suffix.

   The trap here is old and still catches people: M is milli and MEG is mega, so 1M is a
   thousandth of what someone who has only used engineering notation expects. Longest suffix
   first, and the comparison is case-insensitive because netlists are written both ways. */
static bool nl_value(const char *s, double *out) {
    if (!s || !*s) return false;
    char *end = NULL;
    double v = strtod(s, &end);
    if (end == s) return false;
    while (*end == ' ') end++;
    double mult = 1.0;
    if (!_strnicmp(end, "meg", 3))       mult = 1e6,  end += 3;
    else if (!_strnicmp(end, "mil", 3))  mult = 25.4e-6, end += 3;
    else if (*end == 'T' || *end == 't') mult = 1e12, end++;
    else if (*end == 'G' || *end == 'g') mult = 1e9,  end++;
    else if (*end == 'K' || *end == 'k') mult = 1e3,  end++;
    else if (*end == 'M' || *end == 'm') mult = 1e-3, end++;   /* milli, not mega */
    else if (*end == 'U' || *end == 'u') mult = 1e-6, end++;
    else if (*end == 'N' || *end == 'n') mult = 1e-9, end++;
    else if (*end == 'P' || *end == 'p') mult = 1e-12, end++;
    else if (*end == 'F' || *end == 'f') mult = 1e-15, end++;
    /* R is the notation's own decimal point, for values below a kilohm: 1R5 is 1.5 ohm and 4R7
       is 4.7. It multiplies by one, so it does nothing but stand where the point would - which
       is the whole reason it is written that way. Without it here 1R5 read as 1. */
    else if (*end == 'R' || *end == 'r') end++;
    v *= mult;
    /* R-notation: 4k7 is 4.7k, with the multiplier standing where the decimal point would be.
       It exists because a printed decimal point is the first thing to disappear off a
       photocopy or a silkscreen, and it is how most of the world writes a resistor. Read
       without it, 4k7 is 4k - a value that is wrong by 15 % and looks entirely reasonable. */
    if (isdigit((unsigned char)*end)) {
        double frac = 0.0, scale = 0.1;
        while (isdigit((unsigned char)*end)) { frac += (*end - '0') * scale; scale *= 0.1; end++; }
        v += frac * mult;
    }
    *out = v;
    return true;
}

static bool nl_is_ground(const char *net) {
    return net && (!strcmp(net, "0") || !_stricmp(net, "gnd") || !_stricmp(net, "ground"));
}

/* Split a line into tokens, dropping comments and treating ( ) , = as whitespace so that
   SIN(0 10m 1k) and PULSE(0 12 0 1n 1n 400n 833n) fall apart into their numbers. */
static int nl_split(char *line, char *tok[NL_MAX_TOK]) {
    for (char *p = line; *p; p++)
        if (*p == '(' || *p == ')' || *p == ',' || *p == '=' || *p == '\t') *p = ' ';
    char *semi = strchr(line, ';');  if (semi) *semi = 0;
    int n = 0;
    char *p = line;
    while (*p && n < NL_MAX_TOK) {
        while (*p == ' ') p++;
        if (!*p) break;
        tok[n++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    while (*p == ' ') p++;
    if (*p) return -1;      /* more line than tokens: the caller must refuse it, not use half */
    return n;
}

/* Give one of a part's terminals a net name, and remember whether the net is the reference. */
static void nl_set_net(Circuit *c, Component *p, int term, const char *net) {
    if (!c || !p || term < 0 || term >= p->num_terminals) return;
    Node *n = circuit_get_node(c, p->node_ids[term]);
    if (!n) return;
    snprintf(n->name, NET_NAME_MAX, "%s", nl_is_ground(net) ? "0" : net);
}

int netlist_build(Circuit *circuit, const char *text, char *err, size_t err_size) {
    if (err && err_size) err[0] = 0;
    if (!circuit || !text) return -1;

    int placed = 0, skipped = 0, needs_ground = 0, approx = 0;
    char first_bad[64] = "";
    char first_approx[64] = "";
    /* below anything already on the sheet, so a paste does not land on top of it */
    float base_y = 0;
    for (int i = 0; i < circuit->num_components; i++)
        if (circuit->components[i]->y + 200.0f > base_y) base_y = circuit->components[i]->y + 200.0f;

    const char *cur = text;
    char line[512];
    while (*cur) {
        const char *nlp = strchr(cur, '\n');
        size_t len = nlp ? (size_t)(nlp - cur) : strlen(cur);
        if (len >= sizeof line) len = sizeof line - 1;
        memcpy(line, cur, len);
        line[len] = 0;
        cur += len + (nlp ? 1 : 0);

        char *tok[NL_MAX_TOK];
        int nt = nl_split(line, tok);
        if (nt < 0) {           /* too many fields to hold: refuse rather than use the first 24 */
            skipped++;
            if (!first_bad[0]) snprintf(first_bad, sizeof first_bad, "%.40s", line);
            continue;
        }
        if (nt == 0) continue;
        if (tok[0][0] == '*' || tok[0][0] == '#' || tok[0][0] == '.') continue;  /* comment / directive */

        char kind = (char)toupper((unsigned char)tok[0][0]);
        /* one cell per part, eight to a row */
        float px = 160.0f + 220.0f * (float)(placed % 8);
        float py = base_y + 160.0f + 200.0f * (float)(placed / 8);

        ComponentType ty = COMP_NONE;
        int nnodes = 0;
        switch (kind) {
            case 'R': ty = COMP_RESISTOR;  nnodes = 2; break;
            case 'C': ty = COMP_CAPACITOR; nnodes = 2; break;
            case 'L': ty = COMP_INDUCTOR;  nnodes = 2; break;
            case 'I': ty = COMP_DC_CURRENT; nnodes = 2; break;
            case 'D': ty = COMP_DIODE;     nnodes = 2; break;
            case 'V': ty = COMP_DC_VOLTAGE; nnodes = 2; break;   /* may become AC or pulse */
            case 'Q': ty = COMP_NPN_BJT;   nnodes = 3; break;
            case 'M': ty = COMP_NMOS;      nnodes = 3; break;
            case 'E': ty = COMP_VCVS;      nnodes = 4; break;
            case 'G': ty = COMP_VCCS;      nnodes = 4; break;
            /* T1 near far 50 5n - a lossless line, written the way SPICE writes it except that
               the two ports share a return. Added so a question about what a driver sees at the
               far end of a track can be ASKED from outside this program; the model behind it is
               the one --line-test has been holding to matched/open/short reflection amplitudes
               and 2T timing all along. */
            case 'T': ty = COMP_DELAY_LINE; nnodes = 2; break;
            /* A subcircuit call is only worth as much as the model standing behind the name,
               so exactly one name is honoured and every other X line is still refused. OPAMP
               is worth honouring because the part behind it is BETTER than the two-line
               equivalent a netlist would otherwise have to write: a bare VCVS of gain 100k
               has no rails, so a stage whose feedback is broken reports -165 kV instead of
               sitting on a rail where it is recognisable as saturated. That difference is not
               cosmetic - it is the whole reason a finite-gain macromodel sees faults an ideal
               op-amp cannot. Three nodes, written (+in, -in, out), which is the order the
               course writes them in and the order the commented E fallback beside each X line
               confirms. Rails come from the part: +/-15 V, because the netlist does not say. */
            case 'X':
                if (nt == 5 && !_stricmp(tok[4], "OPAMP")) { ty = COMP_OPAMP; nnodes = 3; break; }
                skipped++;
                if (!first_bad[0]) snprintf(first_bad, sizeof first_bad, "%s", tok[0]);
                continue;
            default:
                skipped++;
                if (!first_bad[0]) snprintf(first_bad, sizeof first_bad, "%s", tok[0]);
                continue;
        }
        if (nt < 1 + nnodes) {
            skipped++;
            if (!first_bad[0]) snprintf(first_bad, sizeof first_bad, "%s", tok[0]);
            continue;
        }

        /* A model name after the nodes chooses the polarity for the parts that have one. */
        const char *model = (nt > 1 + nnodes) ? tok[1 + nnodes] : NULL;
        if (kind == 'Q' && model && (strstr(model, "3906") || strstr(model, "PNP") || strstr(model, "pnp")))
            ty = COMP_PNP_BJT;
        if (kind == 'M' && model && (strstr(model, "PMOS") || strstr(model, "pmos") || strstr(model, "9540")))
            ty = COMP_PMOS;

        Component *p = component_create(ty, px, py);
        if (!p) { skipped++; continue; }
        if (circuit_add_component(circuit, p) < 0) { component_free(p); skipped++; continue; }
        snprintf(p->label, sizeof p->label, "%s", tok[0]);

        /* E and G are written output-first - "E1 out 0 in vm gain" - and this VCVS lists its
           control pair first. Swapping here rather than asking the reader to write it backwards
           is the whole job of a reader: an amplifier entered in the form every book prints it
           in would otherwise come out driving its own input. */
        static const int ctl_order[4] = { 2, 3, 0, 1 };
        /* And the same job for the three-terminal devices, which was left half done.
         *
         * Every netlist in the world writes a BJT collector-base-emitter and a MOSFET
         * drain-gate-source. This program's parts carry them base-first and gate-first, and the
         * reader was handing the tokens straight across in that internal order - so a line
         * copied from any book or any other tool built a transistor with its base where its
         * collector should be. It solved, too, which is what makes it worth a comment: a
         * diode-connected device has its drain and gate on one net, so the mirror and the
         * reference that check this kind of thing came out exactly right and the error only
         * showed on the first device whose gate went somewhere else. */
        static const int bjt_order[3] = { 1, 0, 2 };    /* C B E written -> B C E stored */
        static const int fet_order[3] = { 1, 0, 2 };    /* D G S written -> G D S stored */
        /* And once more for the op-amp, whose terminals are stored minus-first. An X line
           writes the non-inverting input first; handing that straight across would build an
           inverting amplifier out of a non-inverting one and still solve. */
        static const int oa_order[3]  = { 1, 0, 2 };    /* + - OUT written -> - + OUT stored */
        bool ctl = (kind == 'E' || kind == 'G');
        bool three = (kind == 'Q' || kind == 'M');
        for (int t = 0; t < nnodes && t < p->num_terminals; t++) {
            int slot = ctl ? ctl_order[t]
                     : three ? (kind == 'Q' ? bjt_order[t] : fet_order[t])
                     : (kind == 'X') ? oa_order[t]
                     : t;
            nl_set_net(circuit, p, slot, tok[1 + t]);
            if (nl_is_ground(tok[1 + t])) needs_ground = 1;
        }

        /* the value, or the waveform */
        double v = 0;
        switch (kind) {
            case 'R':
                if (nl_value(model, &v)) p->props.resistor.resistance = v;
                p->props.resistor.power_rating = 1e9;   /* a written-down circuit has no package */
                break;
            case 'C': if (nl_value(model, &v)) p->props.capacitor.capacitance = v; break;
            case 'T': {
                /* Z0 then the ONE-WAY delay. Both are required: a line with a default impedance
                   would answer a reflection question with a number the caller never supplied,
                   which is the one thing this element exists not to do. */
                double z0 = 0, td = 0;
                bool ok = model && nl_value(model, &z0) && nt > 4 && nl_value(tok[4], &td);
                if (!ok || z0 <= 0 || td <= 0) {
                    circuit_delete_component(circuit, p->id);
                    skipped++;
                    if (!first_bad[0]) snprintf(first_bad, sizeof first_bad, "%s", tok[0]);
                    continue;
                }
                p->props.delay_line.z0 = z0;
                p->props.delay_line.delay = td;
                p->props.delay_line.ideal = true;
                break;
            }
            case 'L': if (nl_value(model, &v)) p->props.inductor.inductance = v; break;
            case 'I': {
                /* The same forms V takes, and it should have taken them all along.
                 *
                 * "IREF1 vdd nref1 DC 200u" used to hand nl_value the token "DC", which is not
                 * a number, so the read failed and the source silently kept the part's default
                 * of 1 mA - five times the stated current, in a circuit that still converged
                 * and still looked entirely reasonable. That is the exact failure this reader
                 * exists to avoid, and it survived because nothing here distinguishes "the
                 * caller did not say" from "the caller said something I could not read". */
                const char *w = model ? model : "0";
                if (!_stricmp(w, "SIN") && nt >= 6) {
                    circuit_delete_component(circuit, p->id);
                    p = component_create(COMP_AC_CURRENT, px, py);
                    if (!p || circuit_add_component(circuit, p) < 0) { if (p) component_free(p); skipped++; continue; }
                    snprintf(p->label, sizeof p->label, "%s", tok[0]);
                    nl_set_net(circuit, p, 0, tok[1]); nl_set_net(circuit, p, 1, tok[2]);
                    double off = 0, amp = 0, f = 1000;
                    nl_value(tok[4], &off); nl_value(tok[5], &amp);
                    if (nt >= 7) nl_value(tok[6], &f);
                    p->props.ac_current.offset = off;
                    p->props.ac_current.amplitude = amp;
                    p->props.ac_current.frequency = f;
                } else if ((!_stricmp(w, "PULSE") || !_stricmp(w, "PWL")) && nt >= 6) {
                    /* There is no pulsed or piecewise CURRENT part, so this keeps the value the
                       waveform holds at t = 0. For an operating point that is not an
                       approximation at all - it is the right number - but for a transient it
                       throws the whole load step away. Counted, not assumed: the caller is told
                       how many sources were flattened, because a load-step circuit answered as
                       a steady one has answered a different question than the one asked. */
                    if (nl_value(tok[!_stricmp(w, "PWL") ? 5 : 4], &v))
                        p->props.dc_current.current = v;
                    approx++;
                    if (!first_approx[0]) snprintf(first_approx, sizeof first_approx, "%s", tok[0]);
                } else if (!_stricmp(w, "AC")) {
                    /* An AC-only source carries no operating-point current, so it is 0 A here
                       rather than the 1 mA the part would otherwise keep. */
                    p->props.dc_current.current = 0;
                } else {
                    const char *val = !_stricmp(w, "DC") ? (nt > 4 ? tok[4] : NULL) : w;
                    if (nl_value(val, &v)) p->props.dc_current.current = v;
                }
                break;
            }
            case 'E': case 'G':
                if (nt > 5 && nl_value(tok[5], &v)) p->props.controlled_source.gain = v;
                break;
            case 'Q': case 'M': {
                if (model) component_apply_part(p, model);   /* silently keeps the default if unknown */
                if (kind != 'M') break;
                /* W= and L= after the model, in either order, as every netlist writes them.
                 *
                 * The stamp works from the ratio, and the default geometry is W/L = 10. A course
                 * that specifies W/L = 90 and is silently given 10 gets nine times too little
                 * drain current out of an answer that looks entirely reasonable - so a W= or L=
                 * that cannot be read, or that is not positive, DROPS the device rather than
                 * falling back. Absent is different from unreadable: no W= at all means the
                 * caller did not ask, and the model's own geometry stands.
                 */
                /* nl_split turns '=' into a space, so "W=90u" arrives as the two tokens "W" and
                   "90u" - the keyword and its value are never one string here. That also makes
                   "W = 90u" and "W 90u" read the same, which is free and harmless. */
                bool bad_geom = false;
                for (int t = 1 + nnodes; t < nt; t++) {
                    const char *a = tok[t];
                    if (!a || !a[0]) continue;
                    bool is_w   = (a[0] == 'W' || a[0] == 'w') && !a[1];   /* single letter, so */
                    bool is_l   = (a[0] == 'L' || a[0] == 'l') && !a[1];   /* L is not LAMBDA */
                    bool is_lam = !_stricmp(a, "LAMBDA");
                    if (!is_w && !is_l && !is_lam) continue;
                    double g = 0;
                    if (t + 1 >= nt || !nl_value(tok[t + 1], &g) || !(g > 0)) { bad_geom = true; break; }
                    if (is_w)      p->props.mosfet.w = g;
                    else if (is_l) p->props.mosfet.l = g;
                    else {
                        /* Channel-length modulation, and it has to take the device out of ideal
                           mode to mean anything: the stamp reads lambda only when !ideal, and
                           `ideal` is the default. Setting LAMBDA and leaving the flag alone would
                           put a number in the part that the solver never looks at - an editable
                           parameter that does not stamp, which this codebase has shipped before
                           and now checks for. Asking for channel-length modulation is asking for
                           the model that has it. */
                        p->props.mosfet.lambda = g;
                        p->props.mosfet.ideal = false;
                    }
                    t++;                                        /* the value is consumed */
                }
                if (bad_geom) {
                    circuit_delete_component(circuit, p->id);
                    skipped++;
                    if (!first_bad[0]) snprintf(first_bad, sizeof first_bad, "%s", tok[0]);
                    continue;
                }
                break;
            }
            case 'V': {
                /* DC 5 | 5 | SIN(off amp freq) | PULSE(v1 v2 td tr tf pw per) | AC 1 */
                const char *w = model ? model : "0";
                if (!_stricmp(w, "SIN") && nt >= 6) {
                    circuit_delete_component(circuit, p->id);
                    p = component_create(COMP_AC_VOLTAGE, px, py);
                    if (!p || circuit_add_component(circuit, p) < 0) { if (p) component_free(p); skipped++; continue; }
                    snprintf(p->label, sizeof p->label, "%s", tok[0]);
                    nl_set_net(circuit, p, 0, tok[1]); nl_set_net(circuit, p, 1, tok[2]);
                    double off = 0, amp = 0, f = 1000;
                    nl_value(tok[4], &off); nl_value(tok[5], &amp);
                    if (nt >= 7) nl_value(tok[6], &f);
                    p->props.ac_voltage.offset = off;
                    p->props.ac_voltage.amplitude = amp;
                    p->props.ac_voltage.frequency = f;
                } else if (!_stricmp(w, "PULSE") && nt >= 6) {
                    circuit_delete_component(circuit, p->id);
                    p = component_create(COMP_PULSE_SOURCE, px, py);
                    if (!p || circuit_add_component(circuit, p) < 0) { if (p) component_free(p); skipped++; continue; }
                    snprintf(p->label, sizeof p->label, "%s", tok[0]);
                    nl_set_net(circuit, p, 0, tok[1]); nl_set_net(circuit, p, 1, tok[2]);
                    double a[7] = { 0, 5, 0, 1e-9, 1e-9, 1e-3, 2e-3 };
                    for (int k = 0; k < 7 && 4 + k < nt; k++) nl_value(tok[4 + k], &a[k]);
                    p->props.pulse_source.v_low = a[0];
                    p->props.pulse_source.v_high = a[1];
                    p->props.pulse_source.delay = a[2];
                    p->props.pulse_source.rise_time = a[3];
                    p->props.pulse_source.fall_time = a[4];
                    p->props.pulse_source.pulse_width = a[5];
                    p->props.pulse_source.period = a[6];
                } else if (!_stricmp(w, "PWL") && nt >= 6) {
                    /* PWL(t0 v0 t1 v1 ...) - the form a course reaches for when it wants one
                       named edge at a named instant rather than a repeating waveform, which is
                       why repeat is off here: SPICE holds the last value forever, and the part
                       defaults the other way. A trailing time with no value is dropped rather
                       than paired with a zero, since the pair is the unit. */
                    circuit_delete_component(circuit, p->id);
                    p = component_create(COMP_PWL_SOURCE, px, py);
                    if (!p || circuit_add_component(circuit, p) < 0) { if (p) component_free(p); skipped++; continue; }
                    snprintf(p->label, sizeof p->label, "%s", tok[0]);
                    nl_set_net(circuit, p, 0, tok[1]); nl_set_net(circuit, p, 1, tok[2]);
                    int np = 0;
                    for (int k = 4; k + 1 < nt && np < 32; k += 2) {
                        double tt = 0, vv = 0;
                        if (!nl_value(tok[k], &tt) || !nl_value(tok[k + 1], &vv)) break;
                        p->props.pwl_source.times[np] = tt;
                        p->props.pwl_source.values[np] = vv;
                        np++;
                    }
                    if (np < 1) {   /* nothing readable: a source with the part's demo waveform
                                       in it would answer with an edge the caller never wrote */
                        circuit_delete_component(circuit, p->id);
                        skipped++;
                        if (!first_bad[0]) snprintf(first_bad, sizeof first_bad, "%s", tok[0]);
                        continue;
                    }
                    p->props.pwl_source.num_points = np;
                    p->props.pwl_source.repeat = false;
                    p->props.pwl_source.repeat_period = 0;
                } else if (!_stricmp(w, "AC") && nt > 4) {
                    /* "VIN in 0 AC 1" is a small-signal drive, not a 1 V battery.
                     *
                     * SPICE gives it a DC value of zero and an AC magnitude for .AC to use, and
                     * this reader used to build a 1 V DC source instead - which biases the
                     * circuit the source was supposed to leave alone. 18 of EE_Review's corpus
                     * are written this way and every one of them is an amplifier input.
                     *
                     * It becomes an AC source carrying the magnitude and no offset, which makes
                     * the operating point right. The FREQUENCY is the part the line does not
                     * state, because in SPICE it never has to - a magnitude with no frequency is
                     * exactly what .AC consumes. So the part keeps its own default and any
                     * caller doing a sweep sets it; nothing here invents a number and reports it
                     * as the caller's. */
                    circuit_delete_component(circuit, p->id);
                    p = component_create(COMP_AC_VOLTAGE, px, py);
                    if (!p || circuit_add_component(circuit, p) < 0) { if (p) component_free(p); skipped++; continue; }
                    snprintf(p->label, sizeof p->label, "%s", tok[0]);
                    nl_set_net(circuit, p, 0, tok[1]); nl_set_net(circuit, p, 1, tok[2]);
                    double mag = 0;
                    nl_value(tok[4], &mag);
                    p->props.ac_voltage.amplitude = mag;
                    p->props.ac_voltage.offset = 0;
                } else {
                    const char *val = !_stricmp(w, "DC") ? (nt > 4 ? tok[4] : NULL) : w;
                    if (nl_value(val, &v)) p->props.dc_voltage.voltage = v;
                }
                break;
            }
            default: break;
        }
        placed++;
    }

    /* One ground symbol for the reference net. Every terminal named 0 joins it by name, so the
       circuit has a reference without a single wire being drawn. Without this a pasted table
       solves to nothing: every node floats and the matrix is singular. */
    if (needs_ground) {
        bool have = false;
        for (int i = 0; i < circuit->num_components; i++)
            if (circuit->components[i]->type == COMP_GROUND) {
                Node *n = circuit_get_node(circuit, circuit->components[i]->node_ids[0]);
                if (n && !_stricmp(n->name, "0")) have = true;
            }
        if (!have) {
            Component *g = component_create(COMP_GROUND, 40.0f, base_y + 160.0f);
            if (g && circuit_add_component(circuit, g) >= 0) {
                Node *n = circuit_get_node(circuit, g->node_ids[0]);
                if (n) snprintf(n->name, NET_NAME_MAX, "0");
                placed++;
            } else if (g) component_free(g);
        }
    }

    circuit->topology_dirty = true;
    if (err && err_size) {
        int w = 0;
        w = snprintf(err, err_size, "placed %d part%s", placed, placed == 1 ? "" : "s");
        if (skipped && w > 0 && (size_t)w < err_size)
            w += snprintf(err + w, err_size - (size_t)w, ", skipped %d line%s (first: %s)",
                          skipped, skipped == 1 ? "" : "s", first_bad);
        /* Flattened is not skipped and it is not placed-as-written either, so it gets said out
           loud. A source whose waveform was thrown away still solves, and the operating point
           it gives is right - which is exactly why nobody would go looking for it. */
        if (approx && w > 0 && (size_t)w < err_size)
            snprintf(err + w, err_size - (size_t)w, ", flattened %d source%s to DC (first: %s)",
                     approx, approx == 1 ? "" : "s", first_approx);
    }
    return placed ? placed : -1;
}

int netlist_build_file(Circuit *circuit, const char *path, char *err, size_t err_size) {
    FILE *f = path ? fopen(path, "rb") : NULL;
    if (!f) { if (err && err_size) snprintf(err, err_size, "cannot open %s", path ? path : "(null)"); return -1; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > 1024 * 1024) {
        fclose(f);
        if (err && err_size) snprintf(err, err_size, "%s is empty or too large", path);
        return -1;
    }
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) { fclose(f); if (err && err_size) snprintf(err, err_size, "out of memory"); return -1; }
    size_t got = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[got] = 0;
    int n = netlist_build(circuit, buf, err, err_size);
    free(buf);
    return n;
}
