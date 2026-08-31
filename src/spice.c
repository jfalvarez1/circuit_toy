#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>

#include "types.h"
#include "component.h"
#include "spice.h"

/* ---------------------------------------------------------------------------------------
 * SPICE .SUBCKT import - see spice.h for what is supported and why.
 *
 * The parser is deliberately small: it reads a flat netlist into a Component array plus a
 * table mapping node NAMES to node ids, then hands both to the same SubCircuitDef structure
 * the Ctrl+G dialog fills. Nothing here knows about the solver; the block is solved by the
 * subcircuit expansion like any other.
 * ------------------------------------------------------------------------------------- */

#define SPICE_MAX_NODES     256
#define SPICE_MAX_COMPS     256
#define SPICE_MAX_LINE      1024

typedef struct {
    char names[SPICE_MAX_NODES][32];
    int  ids[SPICE_MAX_NODES];
    int  count;
    int  next_id;
    bool overflowed;   /* set by node_lookup when it runs out of room - see there */
} NodeTable;

/* SPICE node 0 is always ground. Everything else gets an id of its own.

   Returns -1 when the table is full, and the caller has to notice. It used to return 0 - which is
   GROUND - so a netlist with more than SPICE_MAX_NODES nets did not fail: every net after the
   256th was silently tied to ground, shorting whatever it touched, and the import reported
   success. A quietly wrong circuit is worse than a refused one. */
static int node_lookup(NodeTable *t, const char *name) {
    if (!name || !*name) return 0;
    if (!strcmp(name, "0") || !_stricmp(name, "gnd") || !_stricmp(name, "ground")) return 0;
    for (int i = 0; i < t->count; i++)
        if (!_stricmp(t->names[i], name)) return t->ids[i];
    if (t->count >= SPICE_MAX_NODES) { t->overflowed = true; return -1; }
    snprintf(t->names[t->count], sizeof t->names[0], "%s", name);
    t->ids[t->count] = t->next_id++;
    return t->ids[t->count++];
}

bool spice_parse_value(const char *text, double *out) {
    if (!text || !*text) return false;
    char *end = NULL;
    double v = strtod(text, &end);
    if (end == text) return false;
    while (*end && isspace((unsigned char)*end)) end++;

    /* Suffixes, longest first: MEG is mega and M is milli, which is the classic way to get a
       value wrong by nine orders of magnitude. */
    if (!_strnicmp(end, "MEG", 3))      v *= 1e6,   end += 3;
    else if (!_strnicmp(end, "MIL", 3)) v *= 25.4e-6, end += 3;
    else if (*end == 'T' || *end == 't') v *= 1e12,  end++;
    else if (*end == 'G' || *end == 'g') v *= 1e9,   end++;
    else if (*end == 'K' || *end == 'k') v *= 1e3,   end++;
    else if (*end == 'M' || *end == 'm') v *= 1e-3,  end++;
    else if (*end == 'U' || *end == 'u') v *= 1e-6,  end++;
    else if (*end == 'N' || *end == 'n') v *= 1e-9,  end++;
    else if (*end == 'P' || *end == 'p') v *= 1e-12, end++;
    else if (*end == 'F' || *end == 'f') v *= 1e-15, end++;
    /* trailing unit text ("Ohm", "F", "H") is decoration and is ignored */
    *out = v;
    return true;
}

/* Split a line into whitespace-separated tokens; returns how many. */
static int tokenize(char *line, char **tok, int max) {
    int n = 0;
    char *p = line;
    while (*p && n < max) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        tok[n++] = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (*p) *p++ = '\0';
    }
    return n;
}

typedef struct {
    Component comps[SPICE_MAX_COMPS];
    int count;
    NodeTable nodes;
    char name[64];
    char ports[MAX_SUBCIRCUIT_PINS][32];
    int  num_ports;
} Subckt;

static Component *sub_add(Subckt *sc, ComponentType type, int idx) {
    if (sc->count >= SPICE_MAX_COMPS) return NULL;
    Component *proto = component_create(type, 60.0f + (idx % 6) * 110.0f, 60.0f + (idx / 6) * 110.0f);
    if (!proto) return NULL;
    Component *c = &sc->comps[sc->count++];
    memcpy(c, proto, sizeof(Component));
    free(proto);
    return c;
}

/* Turn one parsed .SUBCKT into a library entry. Returns false if the library is full. */
static bool subckt_commit(Subckt *sc, char *err, size_t err_size) {
    if (g_subcircuit_library.count >= MAX_SUBCIRCUIT_DEFS) {
        snprintf(err, err_size, "subcircuit library is full (%d)", MAX_SUBCIRCUIT_DEFS);
        return false;
    }
    SubCircuitDef *def = &g_subcircuit_library.defs[g_subcircuit_library.count];
    memset(def, 0, sizeof *def);
    def->id = ++g_subcircuit_library.next_id;
    snprintf(def->name, sizeof def->name, "%s", sc->name);

    def->component_data = malloc((size_t)sc->count * sizeof(Component));
    if (!def->component_data) { snprintf(err, err_size, "out of memory"); return false; }
    memcpy(def->component_data, sc->comps, (size_t)sc->count * sizeof(Component));
    def->component_data_size = (size_t)sc->count * sizeof(Component);
    def->num_components = sc->count;

    def->num_pins = sc->num_ports;
    for (int i = 0; i < sc->num_ports; i++) {
        snprintf(def->pins[i].name, sizeof def->pins[i].name, "%s", sc->ports[i]);
        {
            int pn = node_lookup(&sc->nodes, sc->ports[i]);
            def->pins[i].internal_node_id = (pn >= 0) ? pn : 0;
        }
        def->pins[i].side = (i < 2) ? 0 : 1;
        def->pins[i].position = i % 2;
    }

    /* internal nodes are everything the components touch that is not a port */
    int seen[MAX_NODES]; memset(seen, 0, sizeof seen);
    int internal = 0;
    for (int i = 0; i < sc->count; i++) {
        for (int t = 0; t < sc->comps[i].num_terminals && t < MAX_TERMINALS; t++) {
            int id = sc->comps[i].node_ids[t];
            if (id <= 0 || id >= MAX_NODES || seen[id]) continue;
            seen[id] = 1;
            bool is_port = false;
            for (int q = 0; q < sc->num_ports; q++)
                if (def->pins[q].internal_node_id == id) is_port = true;
            if (!is_port) internal++;
        }
    }
    def->num_internal_nodes = internal;
    def->internal_width = 6 * 110.0f;
    def->internal_height = (float)((sc->count / 6) + 1) * 110.0f;
    def->block_width = 80 + sc->num_ports * 10;
    def->block_height = 60 + sc->num_ports * 10;
    g_subcircuit_library.count++;
    return true;
}

int spice_import_text(const char *text, char *err, size_t err_size) {
    if (err && err_size) err[0] = '\0';
    if (!text) { snprintf(err, err_size, "no netlist"); return -1; }

    int imported = 0, skipped = 0;
    char skipped_first[64] = "";
    Subckt sc;
    bool in_subckt = false;

    const char *p = text;
    char line[SPICE_MAX_LINE];
    char pending[SPICE_MAX_LINE] = "";

    while (*p || pending[0]) {
        /* read one logical line, joining `+` continuations */
        if (pending[0]) {
            snprintf(line, sizeof line, "%s", pending);
            pending[0] = '\0';
        } else {
            int n = 0;
            while (*p && *p != '\n' && n < (int)sizeof line - 1) line[n++] = *p++;
            if (*p == '\n') p++;
            line[n] = '\0';
        }
        /* peek: while the next line starts with '+', append it */
        while (*p == '+') {
            p++;
            size_t len = strlen(line);
            if (len < sizeof line - 2) { line[len++] = ' '; line[len] = '\0'; }
            while (*p && *p != '\n' && strlen(line) < sizeof line - 1) {
                size_t l = strlen(line);
                line[l] = *p++; line[l + 1] = '\0';
            }
            if (*p == '\n') p++;
        }

        /* strip comments */
        char *semi = strchr(line, ';');
        if (semi) *semi = '\0';
        char *trim = line;
        while (*trim && isspace((unsigned char)*trim)) trim++;
        if (!*trim || *trim == '*') continue;

        char *tok[64];
        int nt = tokenize(trim, tok, 64);
        if (nt == 0) continue;

        if (!_stricmp(tok[0], ".SUBCKT")) {
            if (nt < 2) continue;
            memset(&sc, 0, sizeof sc);
            sc.nodes.next_id = 1;
            snprintf(sc.name, sizeof sc.name, "%s", tok[1]);
            for (int i = 2; i < nt && sc.num_ports < MAX_SUBCIRCUIT_PINS; i++) {
                if (strchr(tok[i], '=')) continue;          /* .SUBCKT params: not a port */
                snprintf(sc.ports[sc.num_ports], sizeof sc.ports[0], "%s", tok[i]);
                node_lookup(&sc.nodes, tok[i]);             /* give ports their ids first */
                sc.num_ports++;
            }
            in_subckt = true;
            continue;
        }
        if (!_stricmp(tok[0], ".ENDS")) {
            if (in_subckt && sc.count > 0) {
                char e[128] = "";
                if (subckt_commit(&sc, e, sizeof e)) imported++;
                else if (err && err_size) snprintf(err, err_size, "%s", e);
            }
            in_subckt = false;
            continue;
        }
        if (!in_subckt) continue;                            /* only .SUBCKT bodies are imported */
        if (tok[0][0] == '.') continue;                      /* .PARAM, .MODEL, .ENDS handled above */

        char kind = (char)toupper((unsigned char)tok[0][0]);
        if ((kind == 'R' || kind == 'L' || kind == 'C') && nt >= 4) {
            double val = 0;
            if (!spice_parse_value(tok[3], &val)) { skipped++; continue; }
            ComponentType ty = (kind == 'R') ? COMP_RESISTOR : (kind == 'L') ? COMP_INDUCTOR : COMP_CAPACITOR;
            Component *c = sub_add(&sc, ty, sc.count);
            if (!c) { skipped++; continue; }
            if (kind == 'R') { c->props.resistor.resistance = val; c->props.resistor.power_rating = 1e9; }
            else if (kind == 'L') { c->props.inductor.inductance = val; c->props.inductor.ideal = true; }
            else { c->props.capacitor.capacitance = val; c->props.capacitor.ideal = true; }
            int na = node_lookup(&sc.nodes, tok[1]), nb = node_lookup(&sc.nodes, tok[2]);
            c->node_ids[0] = (na >= 0) ? na : 0;
            c->node_ids[1] = (nb >= 0) ? nb : 0;
            snprintf(c->label, sizeof c->label, "%s", tok[0]);
        } else if (kind == 'X' && nt >= 3) {
            /* X<name> n1 n2 ... <subckt>: an instance of another .SUBCKT in this file */
            const char *target = tok[nt - 1];
            SubCircuitDef *tdef = NULL;
            for (int i = 0; i < g_subcircuit_library.count; i++)
                if (!_stricmp(g_subcircuit_library.defs[i].name, target)) tdef = &g_subcircuit_library.defs[i];
            if (!tdef) { skipped++; if (!skipped_first[0]) snprintf(skipped_first, sizeof skipped_first, "%s (unknown subckt %s)", tok[0], target); continue; }
            Component *c = sub_add(&sc, COMP_SUBCIRCUIT, sc.count);
            if (!c) { skipped++; continue; }
            c->props.subcircuit.def_id = tdef->id;
            c->props.subcircuit.inst_data = NULL;
            c->props.subcircuit.inst_count = 0;
            c->props.subcircuit.inst_def_id = 0;
            int pins = nt - 2;
            if (pins > MAX_TERMINALS) pins = MAX_TERMINALS;
            c->num_terminals = pins;
            for (int i = 0; i < pins; i++) {
                int nn = node_lookup(&sc.nodes, tok[1 + i]);
                c->node_ids[i] = (nn >= 0) ? nn : 0;
            }
            snprintf(c->label, sizeof c->label, "%s", tok[0]);
        } else {
            skipped++;
            if (!skipped_first[0]) snprintf(skipped_first, sizeof skipped_first, "%s", tok[0]);
        }
    }

    /* A full node table is a failed import, not a quiet one. node_lookup used to answer 0 -
       ground - for every net past the 256th, so a large netlist came in with whole sections
       shorted together and the import reported success. */
    if (sc.nodes.overflowed) {
        if (err && err_size)
            snprintf(err, err_size, "netlist has more than %d nodes: too large to import",
                     SPICE_MAX_NODES);
        return -1;
    }

    if (err && err_size && !err[0]) {
        if (skipped)
            snprintf(err, err_size, "imported %d subcircuit%s, skipped %d unsupported line%s (first: %s)",
                     imported, imported == 1 ? "" : "s", skipped, skipped == 1 ? "" : "s", skipped_first);
        else
            snprintf(err, err_size, "imported %d subcircuit%s", imported, imported == 1 ? "" : "s");
    }
    return imported;
}

int spice_import_file(const char *path, char *err, size_t err_size) {
    FILE *f = fopen(path, "rb");
    if (!f) { snprintf(err, err_size, "cannot open %s", path ? path : "(null)"); return -1; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > 4 * 1024 * 1024) { fclose(f); snprintf(err, err_size, "%s is empty or too large", path); return -1; }
    char *buf = malloc((size_t)len + 1);
    if (!buf) { fclose(f); snprintf(err, err_size, "out of memory"); return -1; }
    size_t got = fread(buf, 1, (size_t)len, f);
    buf[got] = '\0';
    fclose(f);
    int n = spice_import_text(buf, err, err_size);
    free(buf);
    return n;
}
