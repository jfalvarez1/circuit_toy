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

#define NL_MAX_TOK 12

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

    int placed = 0, skipped = 0, needs_ground = 0;
    char first_bad[64] = "";
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
        bool ctl = (kind == 'E' || kind == 'G');
        for (int t = 0; t < nnodes && t < p->num_terminals; t++) {
            nl_set_net(circuit, p, ctl ? ctl_order[t] : t, tok[1 + t]);
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
            case 'L': if (nl_value(model, &v)) p->props.inductor.inductance = v; break;
            case 'I': if (nl_value(model, &v)) p->props.dc_current.current = v; break;
            case 'E': case 'G':
                if (nt > 5 && nl_value(tok[5], &v)) p->props.controlled_source.gain = v;
                break;
            case 'Q': case 'M':
                if (model) component_apply_part(p, model);   /* silently keeps the default if unknown */
                break;
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
                } else {
                    const char *val = (!_stricmp(w, "DC") || !_stricmp(w, "AC")) ? (nt > 4 ? tok[4] : NULL) : w;
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
        if (skipped)
            snprintf(err, err_size, "placed %d part%s, skipped %d line%s (first: %s)",
                     placed, placed == 1 ? "" : "s", skipped, skipped == 1 ? "" : "s", first_bad);
        else
            snprintf(err, err_size, "placed %d part%s", placed, placed == 1 ? "" : "s");
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
