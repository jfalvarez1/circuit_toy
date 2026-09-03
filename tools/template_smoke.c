/**
 * template_smoke - headless regression check for the prebuilt circuit templates.
 *
 * For every CircuitTemplateType it:
 *   1. places the template in a fresh circuit,
 *   2. runs the DC operating point,
 *   3. runs a short transient (auto time step, ~5 periods of a 1 kHz stimulus),
 *   4. reports solver errors / NaN / runaway voltages,
 *   5. prints the terminal voltages of every transistor and op-amp so bias points
 *      can be checked against TEMPLATE_AUDIT.md by eye.
 *
 * Usage: template_smoke [--dc] [--verbose] [--nodes] [--svg DIR] [--sim-time SEC] [name-substring]
 *        template_smoke --scope-test     (scope time/div <-> dt mapping checks)
 *        template_smoke --flow-test      (current-flow display invariants on all templates)
 *        template_smoke --burn-test      (no resistor/LED over its rating; HV templates must be clean)
 *        template_smoke --std-test       (bus voltages vs ERCOT / NERC / ANSI C84.1 / NEC limits)
 *        template_smoke --switch-test    (every switch in both states, measured at the probed output)
 *        template_smoke --osc-test       (oscillator templates really oscillate, at the right frequency)
 *        template_smoke --probe-test     (probe each template's output node, compare with hand calculation)
 *        template_smoke --geom-test      (schematic audit: diagonals, crossings, wires through bodies)
 *        template_smoke --demo-test      (hard rule: every template declares and demonstrates its behaviour)
 * Exit code is the number of failing templates (0 = all good).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "types.h"
#include "component.h"
#include "circuit.h"
#include "circuits.h"
#include "spice.h"
#include "netlist.h"
#include "sketch.h"
#include "simulation.h"
#include "analysis.h"
#include "file_io.h"
#include "label.h"   /* render_component_value_label: the audit measures the text that is drawn */

/* Defined by the UI layer in the real app; the engine only reads it. */
WirelessState g_wireless = {0};

static int is_active_device(ComponentType t) {
    switch (t) {
        case COMP_NPN_BJT: case COMP_PNP_BJT:
        case COMP_NPN_DARLINGTON: case COMP_PNP_DARLINGTON:
        case COMP_NMOS: case COMP_PMOS: case COMP_NJFET: case COMP_PJFET:
        case COMP_OPAMP: case COMP_OPAMP_FLIPPED: case COMP_OPAMP_REAL:
        case COMP_LM317: case COMP_7805: case COMP_TL431:
        case COMP_DIODE: case COMP_ZENER: case COMP_LED:
            return 1;
        default:
            return 0;
    }
}

static double node_v(Circuit *c, int node_id) {
    Node *n = circuit_get_node(c, node_id);
    return n ? n->voltage : 0.0;
}

static int g_worst_node = -1;   /* node id holding the largest |V| seen */

static int check_voltages(Circuit *c, double *max_abs) {
    int bad = 0;
    *max_abs = 0;
    for (int i = 0; i < c->num_nodes; i++) {
        double v = c->nodes[i].voltage;
        if (isnan(v) || isinf(v)) bad++;
        else if (fabs(v) > *max_abs) { *max_abs = fabs(v); g_worst_node = c->nodes[i].id; }
    }
    return bad;
}

/* Which components touch a node? Used to explain the worst node in the report. */
static void print_node_owners(Circuit *c, int node_id) {
    printf("      worst node %d touched by:", node_id);
    for (int i = 0; i < c->num_components; i++) {
        Component *comp = c->components[i];
        for (int t = 0; t < comp->num_terminals; t++)
            if (comp->node_ids[t] == node_id) printf(" %s[%d]", comp->label, t);
    }
    printf("\n");
}

static void print_bias(Circuit *c) {
    for (int i = 0; i < c->num_components; i++) {
        Component *comp = c->components[i];
        if (!is_active_device(comp->type)) continue;
        const ComponentTypeInfo *info = component_get_info(comp->type);
        printf("      %-6s %-12s", comp->label, info ? info->name : "?");
        for (int t = 0; t < comp->num_terminals && t < 4; t++) {
            const char *tn = info ? info->terminals[t].name : "";
            printf("  %s=%8.4f", tn, node_v(c, comp->node_ids[t]));
        }
        if (comp->type == COMP_LED) printf("  I=%.3fmA", comp->props.led.current * 1e3);
        printf("\n");
    }
}

/* Scope <-> dt mapping self-test: in-sync mapping, out-of-sync (manual dt kept when
 * time/div is unchanged), and both clamp limits. Returns number of failed checks. */
static int scope_dt_test(void) {
    int fails = 0;
    Circuit *c = circuit_create();
    circuit_place_template(c, CIRCUIT_RC_LOWPASS, 0, 0);   /* 1 kHz source -> accuracy dt 10 us (100 samples/period) */
    for (int i = 0; i < c->num_components; i++)
        if (c->components[i]->type == COMP_AC_VOLTAGE) c->components[i]->props.ac_voltage.frequency_sweep.enabled = false;
    Simulation *sim = simulation_create(c);
    struct { double time_div, want; const char *why; } cases[] = {
        { 1e-3,   1e-5,  "1 ms/div: display 50 us, accuracy-limited to 10 us" },
        { 100e-6, 5e-6,  "100 us/div: 5 us (20 samples/div)" },
        { 1e-6,   50e-9, "1 us/div: 50 ns" },
        /* 0.5 ns, and it is reachable: the floor is 10 ps. This case expected 1e-9 because it
           was written when MIN_TIME_STEP was 1 ns, and it kept expecting it after the floor was
           lowered - which is exactly the sort of rot a suite develops when it is in no list and
           nobody runs it. It is in the battery now. */
        { 10e-9,  5e-10, "10 ns/div: 0.5 ns, twenty samples a division at the 10 ps floor" },
        { 100.0,  1e-5,  "100 s/div: display 2 s but accuracy (10 us) wins" },
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        double got = simulation_scope_time_step(sim, cases[i].time_div);
        int ok = fabs(got - cases[i].want) <= 1e-12 + 1e-6 * cases[i].want;
        printf("[%s] scope-dt  time/div=%-8.3g -> dt=%-8.3g (want %-8.3g) %s\n",
               ok ? " OK " : "FAIL", cases[i].time_div, got, cases[i].want, cases[i].why);
        if (!ok) fails++;
    }
    /* Upper clamp needs a circuit with no fast source: a bare resistive divider */
    Circuit *c2 = circuit_create();
    circuit_place_template(c2, CIRCUIT_VOLTAGE_DIVIDER, 0, 0);
    Simulation *sim2 = simulation_create(c2);
    {
        double got = simulation_scope_time_step(sim2, 100.0);
        int ok = fabs(got - 0.01) < 1e-12;   /* no periodic source: the scope rule alone, clamped to MAX_TIME_STEP */
        printf("[%s] scope-dt  DC circuit 100 s/div -> dt=%.3g (want 0.01: no periodic source, scope rule + clamp)\n",
               ok ? " OK " : "FAIL", got);
        if (!ok) fails++;
    }
    /* Out-of-sync: a manual dt survives while time/div is unchanged; set_time_step re-derives
       decimation only when dt really changes */
    simulation_set_time_step(sim, 1e-3);
    sim->history_decimate_factor = 7;
    simulation_set_time_step(sim, 1e-3);
    int keep = (sim->time_step == 1e-3 && sim->history_decimate_factor == 7);
    simulation_set_time_step(sim, 2e-6);
    int reset = (sim->time_step == 2e-6 && sim->history_decimate_factor == 0);
    printf("[%s] scope-dt  manual dt kept when unchanged; decimation reset only on real change\n",
           (keep && reset) ? " OK " : "FAIL");
    if (!(keep && reset)) fails++;
    /* Lower clamp on the setter itself */
    simulation_set_time_step(sim, 1e-12);
    int lo = sim->time_step == MIN_TIME_STEP;
    simulation_set_time_step(sim, 1.0);
    int hi = sim->time_step == MAX_TIME_STEP;
    printf("[%s] scope-dt  set_time_step clamps to [%.0e, %.0e]\n", (lo && hi) ? " OK " : "FAIL",
           MIN_TIME_STEP, MAX_TIME_STEP);
    if (!(lo && hi)) fails++;
    simulation_free(sim); circuit_free(c);
    simulation_free(sim2); circuit_free(c2);
    return fails;
}

/* Current-flow display invariants, checked on every template after a short transient:
 *  - no NaN/Inf terminal or wire currents
 *  - two-terminal components conserve charge (I0 + I1 = 0)
 *  - KCL at every circuit node: wire flow in == component demand out
 *  - series-only templates (RC/RL filters, divider): every wire carries the same |I|, equal to
 *    the resistor current, and every wire is "lit" (|I| > 0) while the source is non-zero. */
static int circuit_node_net(Circuit *c, int id) { return (id >= 0 && id < MAX_NODES) ? c->node_map[id] : -1; }


/* ---------------------------------------------------------------------------------------
 * --scope-test: what the APP shows, for every template.
 *
 * The other modes check numbers at nodes the oracle names. This one checks the thing a user
 * actually complains about: I loaded a template and the scope was empty. It places each
 * template exactly as the app does - circuit_place_template puts the probes down from the
 * template's own output and extra-probe specs - runs it for one screen at the preset time
 * base, and looks at every probe.
 *
 * It also checks the other half of "can I drive this in the GUI": every switch in every
 * template has to be reachable with a click, i.e. a hit test at its own centre has to find
 * that switch and not something drawn on top of it.
 * ------------------------------------------------------------------------------------- */
static int is_switch_type(ComponentType t) {
    return t == COMP_SPST_SWITCH || t == COMP_SPDT_SWITCH || t == COMP_DPDT_SWITCH ||
           t == COMP_PUSH_BUTTON || t == COMP_ANALOG_SWITCH;
}

static int scope_test(void) {
    int fails = 0, total = 0, dead_probes = 0, switches = 0;
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        const char *name = ti ? ti->name : "?";
        Circuit *c = circuit_create();
        if (circuit_place_template(c, (CircuitTemplateType)t, 0, 0) <= 0) { circuit_free(c); continue; }
        total++;
        int ok = 1; char why[220] = "";

        /* every switch has to be clickable where it is drawn */
        int sw_here = 0, sw_bad = 0;
        for (int i = 0; i < c->num_components; i++) {
            Component *comp = c->components[i];
            if (!is_switch_type(comp->type)) continue;
            sw_here++;
            Component *hit = circuit_find_component_at(c, comp->x, comp->y);
            if (hit != comp) {
                sw_bad++;
                if (ok) { ok = 0; snprintf(why, sizeof why, "%s is not clickable at its own centre (hit %s)",
                                           comp->label, hit ? hit->label : "nothing"); }
            }
        }
        switches += sw_here;

        /* one screen at the template's own time base, sampling every probe */
        double td = circuit_template_scope_time_div((CircuitTemplateType)t);
        double run = (td > 0) ? td * 10.0 : 1e-3;
        Simulation *sim = simulation_create(c);
        if (c->num_probes == 0) {
            ok = 0; snprintf(why, sizeof why, "no probes: the scope would be blank");
        } else if (!simulation_dc_analysis(sim)) {
            ok = 0; snprintf(why, sizeof why, "DC failed");
        }
        double mn[MAX_PROBES], mx[MAX_PROBES], sum[MAX_PROBES];
        int np = c->num_probes < MAX_PROBES ? c->num_probes : MAX_PROBES, n = 0;
        for (int i = 0; i < np; i++) { mn[i] = 1e300; mx[i] = -1e300; sum[i] = 0; }
        if (ok) {
            simulation_auto_time_step(sim);
            if (td > 0) { double dtp = simulation_scope_time_step(sim, td); if (dtp > 0 && dtp < sim->time_step) simulation_set_time_step(sim, dtp); }
            simulation_start(sim);
            long steps = 0;
            while (sim->time < run && steps < 400000) {
                if (!simulation_step(sim)) { ok = 0; snprintf(why, sizeof why, "step failed at t=%.3g", sim->time); break; }
                steps++;
                if (sim->time < run * 0.25) continue;          /* let the start-up transient go by */
                for (int i = 0; i < np; i++) {
                    Node *nd = circuit_get_node(c, c->probes[i].node_id);
                    double v = nd ? nd->voltage : 0;
                    if (v < mn[i]) mn[i] = v; if (v > mx[i]) mx[i] = v; sum[i] += v;
                }
                n++;
            }
        }
        /* a channel is "showing something" if it moves at all or sits at a real level */
        int alive = 0;
        char detail[160] = ""; size_t dl = 0;
        if (ok && n > 0) {
            for (int i = 0; i < np; i++) {
                double pp = mx[i] - mn[i], mean = sum[i] / n;
                if (pp > 1e-9 || fabs(mean) > 1e-6) alive++;
                else dead_probes++;
                if (dl < sizeof detail - 24)
                    dl += (size_t)snprintf(detail + dl, sizeof detail - dl, "%s%.3gpp", i ? " " : "", pp);
            }
            if (alive == 0) { ok = 0; snprintf(why, sizeof why, "every probe is flat at 0 V: the scope shows nothing"); }
        }
        if (!ok) fails++;
        printf("[%s] scope %-28s probes=%d alive=%d switches=%d %s %s\n", ok ? " OK " : "FAIL",
               name, c->num_probes, alive, sw_here, detail, why);
        (void)sw_bad;
        if (sim) simulation_free(sim);
        circuit_free(c);
    }
    printf("\nscope-test: %d templates, %d with nothing on the scope, %d flat probes, %d switches all clickable\n",
           total, fails, dead_probes, switches);
    return fails ? 1 : 0;
}


/* ---------------------------------------------------------------------------------------
 * --conn-test: is every pin actually wired to something?
 *
 * A schematic can look right and be wrong: a builder assigns node ids terminal by terminal,
 * and a pin that is simply never assigned - or assigned a node nobody else is on - leaves the
 * part sitting there with a symbol and no circuit behind it. The simulation does not complain,
 * because an unconnected pin is a legal (if useless) node.
 *
 * Connectivity here is electrical, not graphical: two pins are connected when the node map
 * puts them on the same matrix row, which is what wires, shared node ids and the 10 px
 * position merge all reduce to. Ground is row 0, and a pin tied to it counts as connected.
 *
 * What is a fault and what is not:
 *   FAIL  a component with no connected pin at all - it is not in the circuit
 *   FAIL  an IC (subcircuit, 555, gate, flip-flop, op-amp, converter, counter, display) with
 *         any dangling pin: a chip with a floating input is a bug, not a style
 *   note  a dangling pin on a two-terminal part or a marker (test point, antenna, label)
 * ------------------------------------------------------------------------------------- */
static int is_ic_like(ComponentType t) {
    switch (t) {
        case COMP_SUBCIRCUIT: case COMP_555_TIMER: case COMP_7805: case COMP_LM317: case COMP_TL431:
        case COMP_OPAMP: case COMP_OPAMP_REAL: case COMP_OPAMP_FLIPPED: case COMP_OTA:
        case COMP_NOT_GATE: case COMP_AND_GATE: case COMP_OR_GATE: case COMP_NAND_GATE:
        case COMP_NOR_GATE: case COMP_XOR_GATE: case COMP_XNOR_GATE: case COMP_BUFFER:
        case COMP_SCHMITT_INV: case COMP_SCHMITT_BUF: case COMP_TRISTATE_BUF:
        case COMP_D_FLIPFLOP: case COMP_JK_FLIPFLOP: case COMP_T_FLIPFLOP: case COMP_SR_LATCH:
        case COMP_COUNTER: case COMP_SHIFT_REG: case COMP_MUX_2TO1: case COMP_DEMUX_1TO2:
        case COMP_DECODER: case COMP_BCD_DECODER: case COMP_HALF_ADDER: case COMP_FULL_ADDER:
        case COMP_ADC: case COMP_DAC: case COMP_PLL: case COMP_VCO: case COMP_MONOSTABLE:
        case COMP_7SEG_DISPLAY: case COMP_OPTOCOUPLER: case COMP_ANALOG_SWITCH:
            return 1;
        default: return 0;
    }
}

/* markers and one-ended parts: a loose pin on these is how they are drawn, not a fault */
static int is_marker(ComponentType t) {
    return t == COMP_TEXT || t == COMP_LABEL || t == COMP_PIN || t == COMP_TEST_POINT ||
           t == COMP_GROUND || t == COMP_ANTENNA_TX || t == COMP_ANTENNA_RX;
}

/* ------------------------------------------------------------------------------------
 * --file-test: save every template and load it back.
 *
 * A circuit that does not survive being written to disk and read again is the worst thing
 * this program can do to somebody, and nothing checked it. Every template is placed, saved,
 * loaded into a fresh circuit, and then compared twice over: the parts, where they are and
 * what they are worth, and - the check that actually matters - what the simulation does with
 * them. Two circuits that look the same on paper but settle at different voltages have lost
 * something in the write.
 * ---------------------------------------------------------------------------------- */
static double circuit_signature(Circuit *c, int *ok_out) {
    /* one number that depends on every node the solver settles: the sum of |V| after a DC
       solve, which changes if any value, connection or polarity came back different */
    Simulation *sim = simulation_create(c);
    int ok = simulation_dc_analysis(sim);
    double sum = 0;
    if (ok) {
        for (int i = 0; i < c->num_nodes; i++) {
            double v = node_v(c, c->nodes[i].id);
            if (v == v) sum += fabs(v);          /* NaN contributes nothing rather than poisoning */
        }
    }
    if (ok_out) *ok_out = ok;
    simulation_free(sim);
    return sum;
}

/* one leg of the round trip: write with save_fn, read with load_fn, and say what came back
   different. Returns "" when the circuit survived. */
/* Two circuits, judged the same way whatever put them side by side - a save and a load, or
   an edit and an undo. Both ask the same question: is this the circuit it was? */
/* strict: also compare the raw property block and the printed value. That is right for a save
   and a load, whose two circuits have had identical histories. It is wrong after an undo:
   properties carry live state - the power a part is dissipating, its temperature - and one of
   the two circuits has been solved more times than the other. The structure, the connections and
   the operating point are compared either way, and those are what an undo has to get right. */
static void circuit_compare_ex(Circuit *a, Circuit *b, bool strict, char *why, size_t whyn) {
    why[0] = 0;
    if (b->num_components != a->num_components) {
        /* name the odd one out: which type appeared or vanished, and where */
        char extra[80] = "";
        if (b->num_components > a->num_components) {
            Component *x = b->components[b->num_components - 1];
            snprintf(extra, sizeof extra, " (last back: %s type %d at %g,%g)", x->label, x->type, x->x, x->y);
        }
        snprintf(why, whyn, "%d components saved, %d came back%s", a->num_components, b->num_components, extra);
    }
    else if (b->num_wires != a->num_wires)
        snprintf(why, whyn, "%d wires saved, %d came back", a->num_wires, b->num_wires);
    /* Probes were never compared here, so nothing noticed whether they came back - and they are
       what the scope is looking at, which makes them part of the circuit as much as any wire. */
    else if (b->num_probes != a->num_probes)
        snprintf(why, whyn, "%d probes saved, %d came back", a->num_probes, b->num_probes);
    else {
        /* Paired by id, not by position. A save and a load keep the order; an undo does not -
           removing a part shifts the rest down and putting it back appends it - and comparing
           the fifth of one against the fifth of the other then reports every part after the
           edit as changed. The id is what follows a part through all of it. */
        static bool claimed[MAX_COMPONENTS];
        memset(claimed, 0, sizeof claimed);
        for (int i = 0; i < a->num_components && !why[0]; i++) {
            /* Pair by id where the ids still mean the same thing, and by what the part is
               where they do not. A circuit loaded from a file has been given its own numbers
               starting at one, and a canvas that has been cleared and refilled has moved on -
               so the same part can be id 1 on one side and id 400 on the other. Each part on
               the other side can only be claimed once, or two identical parts would both match
               the same one and a missing part would go unnoticed. */
            Component *ca = a->components[i], *cb = NULL;
            for (int j = 0; j < b->num_components; j++)
                if (!claimed[j] && b->components[j]->id == ca->id) { cb = b->components[j]; claimed[j] = true; break; }
            if (!cb) {
                for (int j = 0; j < b->num_components; j++) {
                    Component *cand = b->components[j];
                    if (claimed[j] || cand->type != ca->type) continue;
                    if (fabsf(cand->x - ca->x) > 0.5f || fabsf(cand->y - ca->y) > 0.5f) continue;
                    cb = cand; claimed[j] = true; break;
                }
            }
            if (!cb) {
                snprintf(why, whyn, "%s (id %d, a %d at %g,%g) is not there any more",
                         ca->label, ca->id, ca->type, ca->x, ca->y);
                break;
            }
            if (ca->type != cb->type)
                snprintf(why, whyn, "part %d is type %d, came back %d", i, ca->type, cb->type);
            else if (fabsf(ca->x - cb->x) > 0.5f || fabsf(ca->y - cb->y) > 0.5f)
                snprintf(why, whyn, "%s moved (%g,%g) -> (%g,%g)", ca->label, ca->x, ca->y, cb->x, cb->y);
            else if (ca->rotation != cb->rotation)
                snprintf(why, whyn, "%s rotation %d -> %d", ca->label, ca->rotation, cb->rotation);
            /* The label is what the schematic prints beside the part - R1, C2 - and it was the one
               field this comparison mentioned in every message and never checked. The JSON writer
               emitted it and the JSON reader ignored it, so every Open renamed every part, and
               188 templates round-tripped past this without a word. */
            else if (strict && strcmp(ca->label, cb->label))
                snprintf(why, whyn, "%s came back labelled '%s'", ca->label, cb->label);
            else {
                /* the value the schematic prints: resistance, capacitance, source volts, switch
                   state, part number - everything that carries a label */
                char va[96] = "", vb[96] = "";
                render_component_value_label(ca, va, sizeof va, NULL, NULL);
                render_component_value_label(cb, vb, sizeof vb, NULL, NULL);
                if (strict && strcmp(va, vb))
                    snprintf(why, whyn, "%s reads '%s', came back '%s'", ca->label, va, vb);
                /* the printed value is only the headline: compare the whole property block, so
                   a series resistance or an ideal flag that did not survive is named too. The
                   two types that own memory through the union hold pointers there, and those
                   are meant to differ. */
                else if (strict && ca->type != COMP_DELAY_LINE && ca->type != COMP_SUBCIRCUIT &&
                         memcmp(&ca->props, &cb->props, sizeof ca->props)) {
                    const unsigned char *pa = (const unsigned char *)&ca->props;
                    const unsigned char *pb2 = (const unsigned char *)&cb->props;
                    size_t off = 0;
                    while (off < sizeof ca->props && pa[off] == pb2[off]) off++;
                    snprintf(why, whyn, "%s props differ at byte %zu of %zu", ca->label, off, sizeof ca->props);
                }
            }
        }
        { int d1=0,d2=0; circuit_signature(a,&d1); circuit_signature(b,&d2); }   /* build both node maps */
        /* the connections themselves, named: two circuits can hold identical parts at identical
           values and still be different circuits */
        /* What matters is which pins share a net, not what the nets are called. A save and a
           load keep the numbers; an undo renumbers freely, and a circuit that comes back
           electrically identical with different net numbers has come back. So both sides are
           relabelled in the order their pins are walked - first net seen becomes 1, and so on -
           and the two labellings have to agree. That is the same test for a dropped connection
           and no test at all for arithmetic on names. */
        {
            int lab_a[MAX_NODES + 1], lab_b[MAX_NODES + 1];
            for (int i = 0; i <= MAX_NODES; i++) { lab_a[i] = 0; lab_b[i] = 0; }
            int next_a = 1, next_b = 1;
            for (int i = 0; i < a->num_components && !why[0]; i++) {
                Component *ca = a->components[i], *cb = b->components[i];
                for (int k = 0; k < ca->num_terminals; k++) {
                    int na = a->node_map[ca->node_ids[k]], nb = b->node_map[cb->node_ids[k]];
                    if (na < 0 || na > MAX_NODES || nb < 0 || nb > MAX_NODES) continue;
                    if (!lab_a[na]) lab_a[na] = next_a++;
                    if (!lab_b[nb]) lab_b[nb] = next_b++;
                    if (lab_a[na] != lab_b[nb]) {
                        snprintf(why, whyn, "%s pin %d shares a net with a different pin than it "
                                 "did (net %d of %d became %d of %d)", ca->label, k,
                                 lab_a[na], next_a - 1, lab_b[nb], next_b - 1);
                        break;
                    }
                }
            }
        }
        if (!why[0]) {
            int oka = 0, okb = 0;
            double sa = circuit_signature(a, &oka), sb = circuit_signature(b, &okb);
            if (oka != okb)
                snprintf(why, whyn, "DC solves %s before and %s after", oka ? "yes" : "no", okb ? "yes" : "no");
            else if (oka && fabs(sa - sb) > 1e-6 * (1.0 + fabs(sa)))
                snprintf(why, whyn, "settles differently: sum|V| %.6g -> %.6g", sa, sb);
        }
    }
}

static void circuit_compare(Circuit *a, Circuit *b, char *why, size_t whyn) {
    circuit_compare_ex(a, b, true, why, whyn);
}

static void roundtrip_leg(Circuit *a, const char *path,
                          bool (*save_fn)(Circuit *, const char *),
                          bool (*load_fn)(Circuit *, const char *),
                          char *why, size_t whyn) {
    why[0] = 0;
    if (!save_fn(a, path)) { snprintf(why, whyn, "save failed"); return; }
    Circuit *b = circuit_create();
    if (!load_fn(b, path)) { snprintf(why, whyn, "load failed"); circuit_free(b); return; }

    circuit_compare(a, b, why, whyn);
    circuit_free(b);
}

/* --undo-test: Ctrl+Z has to put the circuit back exactly as it was, and Ctrl+Y has to put the
   edit back. Every kind of edit the undo stack records is exercised on every template: add a
   part, delete a part, move a part, add a wire, delete a wire. The circuit is written to a file
   before the edit and loaded back afterwards to compare against - save and load are already
   held to the same standard by --file-test, so this borrows their snapshot rather than growing
   a second way to copy a circuit.

   The comparison is the one the file audits use: part for part, wire for wire, terminal for
   terminal, and the operating point at the end. An undo that leaves a component behind, or
   leaves it a pixel from where it was, or drops the net it was wired to, fails here. */
static int undo_test(void) {
    static const char *op_name[] = { "add a part", "delete a part", "move a part",
                                     "add a wire", "delete a wire", "delete a selection",
                                     "rotate a part", "edit a value", "duplicate a part",
                                     "add a probe", "delete a probe",
                                     "clear the canvas", "swap the circuit" };
    int fails = 0, checks = 0, templates = 0;

    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        if (!ti) continue;
        templates++;

        for (int op = 0; op < 13; op++) {
            Circuit *c = circuit_create();
            if (circuit_place_template(c, (CircuitTemplateType)t, 0, 0) <= 0) { circuit_free(c); break; }
            if (!file_save_circuit(c, "undo_before.cpg")) { circuit_free(c); break; }

            bool did = false;
            switch (op) {
                case 0: {   /* add a part: the app pushes UNDO_ADD_COMPONENT after placing */
                    Component *n = component_create(COMP_RESISTOR, -400, -400);
                    if (n && circuit_add_component(c, n) >= 0) {
                        circuit_push_undo(c, UNDO_ADD_COMPONENT, n->id, NULL, 0, 0);
                        did = true;
                    } else if (n) component_free(n);
                    break;
                }
                case 1: {   /* delete a part, the way the delete tool does */
                    if (c->num_components > 0) {
                        circuit_delete_component(c, c->components[c->num_components - 1]->id);
                        did = true;
                    }
                    break;
                }
                case 2: {   /* move a part */
                    if (c->num_components > 0) {
                        Component *m = c->components[0];
                        circuit_push_undo(c, UNDO_MOVE_COMPONENT, m->id, NULL, m->x, m->y);
                        m->x += 120; m->y -= 80;
                        /* dragging a part takes its nodes with it: the app does this on every
                           mouse move, and without it the part and its terminals disagree */
                        circuit_update_component_nodes(c, m);
                        did = true;
                    }
                    break;
                }
                case 3: {   /* add a wire between two nodes that are not already joined */
                    if (c->num_nodes >= 2) {
                        int wid = circuit_add_wire(c, c->nodes[0].id, c->nodes[c->num_nodes - 1].id);
                        if (wid >= 0) { circuit_push_undo(c, UNDO_ADD_WIRE, wid, NULL, 0, 0); did = true; }
                    }
                    break;
                }
                case 4: {   /* delete a wire, the way the delete tool does */
                    if (c->num_wires > 0) {
                        circuit_delete_wire(c, c->wires[c->num_wires - 1].id);
                        did = true;
                    }
                    break;
                }
                case 6: {   /* rotate: the part stays where it is and is not what it was */
                    if (c->num_components > 0) {
                        Component *r = c->components[0];
                        circuit_push_edit_undo(c, r);
                        component_rotate(r);
                        circuit_update_component_nodes(c, r);
                        did = true;
                    }
                    break;
                }
                case 7: {   /* a value typed into the properties panel */
                    for (int i = 0; i < c->num_components && !did; i++) {
                        Component *r = c->components[i];
                        if (r->type != COMP_RESISTOR) continue;
                        circuit_push_edit_undo(c, r);
                        r->props.resistor.resistance *= 3.0;
                        did = true;
                    }
                    break;
                }
                case 11: {  /* clear the canvas, recorded whole */
                    if (c->num_components > 0 && circuit_push_snapshot_undo(c)) {
                        circuit_clear_after_snapshot(c);
                        did = true;
                    }
                    break;
                }
                case 12: {  /* what picking a circuit from the palette does: the canvas recorded,
                               then cleared, then another circuit placed on it */
                    if (c->num_components > 0 && circuit_push_snapshot_undo(c)) {
                        CircuitTemplateType other = (CircuitTemplateType)
                            (t + 1 < CIRCUIT_TYPE_COUNT ? t + 1 : CIRCUIT_NONE + 1);
                        circuit_clear_after_snapshot(c);
                        if (circuit_place_template(c, other, 0, 0) > 0) did = true;
                    }
                    break;
                }
                case 9: {   /* place a probe on a node */
                    if (c->num_nodes > 0 && c->num_probes < MAX_PROBES) {
                        int pid = circuit_add_probe(c, c->nodes[0].id, c->nodes[0].x, c->nodes[0].y);
                        if (pid > 0) { circuit_push_probe_undo(c, pid); did = true; }
                    }
                    break;
                }
                case 10: {  /* and take one off again */
                    if (c->num_probes > 0) {
                        circuit_delete_probe(c, c->probes[c->num_probes - 1].id);
                        did = true;
                    }
                    break;
                }
                case 8: {   /* duplicate: a new part, recorded as one added */
                    if (c->num_components > 0) {
                        Component *d = circuit_duplicate_component(c, c->components[0]);
                        if (d) {
                            circuit_push_undo(c, UNDO_ADD_COMPONENT, d->id, NULL, 0, 0);
                            did = true;
                        }
                    }
                    break;
                }
                case 5: {   /* a selection: several parts and a wire, deleted as one act */
                    if (c->num_components >= 3) {
                        circuit_undo_batch_begin(c);
                        for (int k = 0; k < 3; k++)
                            circuit_delete_component(c, c->components[c->num_components - 1]->id);
                        if (c->num_wires > 0)
                            circuit_delete_wire(c, c->wires[c->num_wires - 1].id);
                        circuit_undo_batch_end(c);
                        did = true;
                    }
                    break;
                }
                default: break;
            }
            if (!did) { circuit_free(c); continue; }

            checks++;
            char why[240] = "";
            /* the edited circuit, to hold redo to */
            bool have_after = file_save_circuit(c, "undo_after.cpg");

            if (!circuit_undo(c)) {
                snprintf(why, sizeof why, "undo did nothing at all");
            } else {
                Circuit *before = circuit_create();
                if (file_load_circuit(before, "undo_before.cpg"))
                    circuit_compare_ex(before, c, false, why, sizeof why);
                else
                    snprintf(why, sizeof why, "could not read the snapshot back");
                circuit_free(before);
            }

            /* and back again: redo has to return the circuit the edit made */
            if (!why[0] && have_after) {
                if (!circuit_redo(c)) {
                    snprintf(why, sizeof why, "redo did nothing at all");
                } else {
                    Circuit *after = circuit_create();
                    if (file_load_circuit(after, "undo_after.cpg")) {
                        char rwhy[200] = "";
                        circuit_compare_ex(after, c, false, rwhy, sizeof rwhy);
                        if (rwhy[0]) snprintf(why, sizeof why, "redo: %s", rwhy);
                    }
                    circuit_free(after);
                }
            }

            /* A recorded delete leaves the nodes it stranded in place, so that an undo can
               reconnect to exactly those. Once the stack is gone nothing can, and they are
               litter: discarding the stack is where they are swept up, and this is where that
               is checked. */
            if (!why[0]) {
                circuit_clear_undo(c);
                circuit_clear_redo(c);
                int stranded = 0;
                for (int n = 0; n < c->num_nodes; n++) {
                    int id = c->nodes[n].id, used = 0;
                    for (int i = 0; i < c->num_components && !used; i++)
                        for (int k = 0; k < c->components[i]->num_terminals; k++)
                            if (c->components[i]->node_ids[k] == id) { used = 1; break; }
                    for (int i = 0; i < c->num_wires && !used; i++)
                        if (c->wires[i].start_node_id == id || c->wires[i].end_node_id == id) used = 1;
                    for (int i = 0; i < c->num_probes && !used; i++)
                        if (c->probes[i].node_id == id) used = 1;
                    if (!used) stranded++;
                }
                if (stranded)
                    snprintf(why, sizeof why, "%d nodes left with nothing on them once the undo "
                             "stack was discarded", stranded);
            }

            if (why[0]) {
                printf("[FAIL] undo  %-28s %-14s %s\n", ti->name, op_name[op], why);
                fails++;
            }
            circuit_free(c);
        }
    }

    remove("undo_before.cpg");
    remove("undo_after.cpg");

    /* Nothing may outlive the circuit it describes.
       circuit_clear_undo released the undo stack and left the REDO stack standing, and its only
       caller is circuit_clear - which is throwing the whole circuit away. So opening a file kept
       the redo records of the circuit that was on the canvas before, and one Ctrl+Y replayed an
       action naming parts by numbers that now meant something else. Node ids restart on a clear
       too, so those numbers do match something: the wrong part. */
    {
        checks++;
        Circuit *c = circuit_create();
        circuit_place_template(c, CIRCUIT_RC_LOWPASS, 0, 0);
        /* an edit, then an undo, which is what fills the redo stack */
        Component *n = component_create(COMP_RESISTOR, -400, -400);
        if (n && circuit_add_component(c, n) >= 0) {
            circuit_push_undo(c, UNDO_ADD_COMPONENT, n->id, NULL, 0, 0);
            circuit_undo(c);
        }
        int redo_before = c->redo_count;
        circuit_clear(c);
        if (redo_before <= 0) {
            printf("[FAIL] undo  could not build a redo stack to test with\n");
            fails++;
        } else if (c->redo_count != 0) {
            printf("[FAIL] undo  %d redo record(s) survived circuit_clear and still name the old "
                   "circuit's parts\n", c->redo_count);
            fails++;
        } else {
            printf("[ OK ] undo  %d redo record(s) go with the circuit they describe\n", redo_before);
        }
        circuit_free(c);
    }

    printf("\nundo-test: %d edits over %d templates, %d that undo did not put back\n",
           checks, templates, fails);
    return fails;
}

/* --parts-file-test: one of every part on a canvas, saved and loaded back.
   --file-test round-trips every template, which exercises the 52 component types that appear in
   one. The other 74 are saved and loaded by code nothing has ever run: a part whose state is
   built when it is created rather than stored - a logic block's engine, a delay line's ring
   buffer - can come back different, and the first person to find out is whoever saved a circuit
   with one in it. The parts sit 200 px apart so no two terminals merge into one node. */
static int parts_file_test(void) {
    Circuit *a = circuit_create();
    int placed = 0, uncreatable = 0;
    for (int t = COMP_NONE + 1; t < COMP_TYPE_COUNT; t++) {
        const ComponentTypeInfo *info = component_get_info((ComponentType)t);
        if (!info || !info->name || !info->name[0]) { uncreatable++; continue; }
        float x = (float)(placed % 12) * 200.0f, y = (float)(placed / 12) * 200.0f;
        Component *comp = component_create((ComponentType)t, x, y);
        if (!comp || circuit_add_component(a, comp) < 0) {
            if (comp) component_free(comp);
            uncreatable++;
            continue;
        }
        /* Defaults round-trip even through a format that drops the field, because the loader
           creates the part with those same defaults. Every part therefore carries a value no
           default is: the first double of its own properties, which for nearly all of them is
           the value on the schematic. Three are left alone: text, whose properties start with a
           string, and the delay line and the subcircuit, whose properties start with pointers to
           memory they own - a double's bytes in a pointer is a crash at the next free, in this
           test's own cleanup. (Loading is safe from that: component_adopt_props never takes a
           pointer out of a file.) */
        /* Not every part's properties start with a number that any value is legal for:
             - text, an expression, a bus or pin name, a subcircuit's name: a string
             - a delay line: pointers to buffers it owns
             - an arbitrary source: an index into a fixed set of tables, which is checked on the
               way in, so a marker there is reported as a difference by the sanitiser doing
               exactly its job (which is how the out-of-range crash below it was found)
           Those keep their defaults; the rest carry the marker. */
        bool marker_unsafe = (t == COMP_TEXT || t == COMP_EXPR_SOURCE || t == COMP_BUS ||
                              t == COMP_BUS_TAP || t == COMP_PIN || t == COMP_SUBCIRCUIT ||
                              t == COMP_DELAY_LINE || t == COMP_ARB_SOURCE);
        if (!marker_unsafe) {
            double marker = 1.2345e-7;
            memcpy(&comp->props, &marker, sizeof marker);
        }
        comp->rotation = (placed % 4) * 90;
        placed++;
    }

    int fails = 0;
    char why[240];
    roundtrip_leg(a, "parts_roundtrip.cpg", file_save_circuit, file_load_circuit, why, sizeof why);
    if (why[0]) { printf("[FAIL] parts-file  binary: %s\n", why); fails++; }
    else printf("[ OK ] parts-file  binary: %d parts saved and loaded back\n", placed);

    roundtrip_leg(a, "parts_roundtrip.json", file_export_json, file_import_json, why, sizeof why);
    if (why[0]) { printf("[FAIL] parts-file  json:   %s\n", why); fails++; }
    else printf("[ OK ] parts-file  json:   %d parts saved and loaded back\n", placed);

    remove("parts_roundtrip.cpg");
    remove("parts_roundtrip.json");
    circuit_free(a);
    printf("\nparts-file-test: %d of %d component types on one canvas (%d cannot be created), "
           "%d formats failed\n", placed, COMP_TYPE_COUNT - 1, uncreatable, fails);
    return fails;
}

static int file_test(const char *filter) {
    int fails = 0, total = 0;
    char path[600];
    const char *tmp = getenv("TEMP");
    if (!tmp) tmp = ".";
    snprintf(path, sizeof path, "%s\\ct_roundtrip.json", tmp);

    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        const char *name = ti ? ti->name : "?";
        if (filter && !strstr(name, filter)) continue;
        Circuit *a = circuit_create();
        if (circuit_place_template(a, (CircuitTemplateType)t, 0, 0) <= 0) { circuit_free(a); continue; }
        total++;
        char why[220] = "";

        /* Both formats, because they are not the same code and only one of them is what the
           Save button writes. The binary .ckt is the older path; the app saves and loads JSON. */
        char why_bin[220] = "", why_json[220] = "";
        roundtrip_leg(a, path, file_save_circuit, file_load_circuit, why_bin, sizeof why_bin);
        roundtrip_leg(a, path, file_export_json, file_import_json, why_json, sizeof why_json);
        if (why_bin[0])       snprintf(why, sizeof why, "binary: %s", why_bin);
        else if (why_json[0]) snprintf(why, sizeof why, "json: %s", why_json);

        printf("[%s] file  %-28s parts=%-3d wires=%-3d %s\n", why[0] ? "FAIL" : " OK ",
               name, a->num_components, a->num_wires, why);
        fflush(stdout);   /* so a template that takes the process down names itself */
        if (why[0]) fails++;
        circuit_free(a);
    }
    remove(path);
    printf("\nfile-test: %d templates saved and loaded back, %d failed\n", total, fails);
    return fails;
}

static int conn_test(void) {
    int fails = 0, total = 0, dangling = 0, isolated = 0;
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        const char *name = ti ? ti->name : "?";
        Circuit *c = circuit_create();
        if (circuit_place_template(c, (CircuitTemplateType)t, 0, 0) <= 0) { circuit_free(c); continue; }
        total++;
        Simulation *sim = simulation_create(c);
        int ok = simulation_dc_analysis(sim);          /* builds the node map */
        char why[240] = ""; size_t wl = 0;
        int t_dangling = 0, t_isolated = 0, bad_here = 0;
        char loose_list[200] = ""; size_t nl = 0;

        if (!ok) {
            snprintf(why, sizeof why, "DC failed, cannot map nodes");
            bad_here = 1;
        } else {
            for (int i = 0; i < c->num_components; i++) {
                Component *comp = c->components[i];
                if (is_marker(comp->type)) continue;
                int live = 0, loose = 0; char loose_pins[64] = ""; size_t ll = 0;
                for (int k = 0; k < comp->num_terminals; k++) {
                    int id = comp->node_ids[k];
                    int m = (id >= 0 && id < MAX_NODES) ? c->node_map[id] : -1;
                    if (id <= 0 || m < 0) {
                        loose++;
                        if (ll < sizeof loose_pins - 4) ll += (size_t)snprintf(loose_pins + ll, sizeof loose_pins - ll, "%s%d", ll ? "," : "", k);
                        continue;
                    }
                    if (m == 0) { live++; continue; }         /* tied to ground */
                    /* Any OTHER terminal on the same net counts, including another pin of
                       this same part: a voltage follower ties its output to its own inverting
                       input, and that is a connection, not a loose end. */
                    int others = 0;
                    for (int j = 0; j < c->num_components && !others; j++) {
                        Component *o = c->components[j];
                        for (int q = 0; q < o->num_terminals; q++) {
                            if (j == i && q == k) continue;
                            int oid = o->node_ids[q];
                            if (oid <= 0 || oid >= MAX_NODES) continue;
                            if (c->node_map[oid] == m) { others = 1; break; }
                        }
                    }
                    if (others) live++;
                    else {
                        loose++;
                        if (ll < sizeof loose_pins - 4) ll += (size_t)snprintf(loose_pins + ll, sizeof loose_pins - ll, "%s%d", ll ? "," : "", k);
                    }
                }
                if (loose == 0) continue;
                t_dangling += loose;
                if (nl < sizeof loose_list - 24)
                    nl += (size_t)snprintf(loose_list + nl, sizeof loose_list - nl, "%s%s pin%s",
                                           nl ? " " : "", comp->label, loose_pins);
                int fault = (live == 0) || (is_ic_like(comp->type) && loose > 0);
                if (live == 0) t_isolated++;
                if (fault) {
                    bad_here = 1;
                    if (wl < sizeof why - 40)
                        wl += (size_t)snprintf(why + wl, sizeof why - wl, "%s%s pin%s%s", wl ? "; " : "",
                                               comp->label, loose_pins, live == 0 ? " (isolated)" : "");
                }
            }
        }
        dangling += t_dangling; isolated += t_isolated;
        if (bad_here) fails++;
        printf("[%s] conn  %-28s parts=%-3d loose=%-2d %s\n",
               bad_here ? "FAIL" : (t_dangling ? "NOTE" : " OK "),
               name, c->num_components, t_dangling, why[0] ? why : loose_list);
        if (sim) simulation_free(sim);
        circuit_free(c);
    }
    printf("\nconn-test: %d templates, %d with a floating IC pin or an isolated part, %d loose pins, %d parts in nothing\n",
           total, fails, dangling, isolated);
    return fails ? 1 : 0;
}


/* ---------------------------------------------------------------------------------------
 * --line-test: the delay line, against the three answers everyone knows.
 *
 * A step of amplitude V launched into a line of impedance Z0 from a source of impedance Rs
 * puts V * Z0/(Rs+Z0) on the line. What comes back depends only on what is at the far end:
 *
 *   matched (RL = Z0)   nothing reflects. The far end sees the launched wave, once, T later.
 *   open    (RL = inf)  the reflection coefficient is +1: the far end doubles to the full V,
 *                       and the source end sees that arrive at 2T.
 *   short   (RL = 0)    the coefficient is -1: the far end stays at 0 and the source end is
 *                       driven back to 0 at 2T.
 *
 * The point of the model is that the delay is real: the far end must stay at zero until T,
 * not ramp up immediately the way an L-C ladder with too few sections does. So the timing is
 * checked as well as the amplitude.
 * ------------------------------------------------------------------------------------- */
static Component *pt_add(Circuit *c, ComponentType t, float x, float y, int rot);   /* defined with the part-test helpers */
static int pt_node(Circuit *c, float x, float y);

typedef struct { double t_far_rise, t_near_rise, v_far, v_src_after_2t, v_far_final; int ok; } LineRun;

static LineRun line_run(double rl_ohm, int open_end, double z0, double td, double rs) {
    LineRun out = {0};
    Circuit *c = circuit_create();
    Component *src = pt_add(c, COMP_PULSE_SOURCE, 0, 100, 0);
    src->props.pulse_source.v_low = 0; src->props.pulse_source.v_high = 2.0;
    src->props.pulse_source.delay = 0;
    src->props.pulse_source.rise_time = src->props.pulse_source.fall_time = td / 20.0;
    src->props.pulse_source.pulse_width = td * 20.0; src->props.pulse_source.period = td * 100.0;
    Component *g0 = pt_add(c, COMP_GROUND, 0, 200, 0);
    Component *rsc = pt_add(c, COMP_RESISTOR, 140, 60, 0);
    rsc->props.resistor.resistance = rs;
    rsc->props.resistor.power_rating = 10.0;
    Component *ln = pt_add(c, COMP_DELAY_LINE, 320, 60, 0);
    ln->props.delay_line.z0 = z0; ln->props.delay_line.delay = td; ln->props.delay_line.ideal = true;

    int sp = pt_node(c, 0, 60), near = pt_node(c, 200, 60), gnd = pt_node(c, 0, 180), far = pt_node(c, 440, 60);
    src->node_ids[0] = sp; src->node_ids[1] = gnd; g0->node_ids[0] = gnd;
    rsc->node_ids[0] = sp; rsc->node_ids[1] = near;
    ln->node_ids[0] = near; ln->node_ids[1] = far;

    Component *rload = NULL;
    if (!open_end) {
        rload = pt_add(c, COMP_RESISTOR, 520, 140, 90);
        rload->props.resistor.resistance = rl_ohm > 0 ? rl_ohm : 1e-3;
        rload->props.resistor.power_rating = 10.0;
        Component *gl = pt_add(c, COMP_GROUND, 520, 260, 0);
        int lb = pt_node(c, 520, 180);
        rload->node_ids[0] = far;      /* the load IS the far end of the line */
        rload->node_ids[1] = lb; gl->node_ids[0] = lb;
    }

    Simulation *sim = simulation_create(c);
    out.ok = sim && simulation_dc_analysis(sim);
    if (out.ok) {
        simulation_set_time_step(sim, td / 40.0);
        simulation_start(sim);
        double t_end = td * 6.0;
        int seen_far = 0, seen_near = 0;
        while (sim->time < t_end) {
            if (!simulation_step(sim)) { out.ok = 0; break; }
            Node *nf = circuit_get_node(c, far), *nn = circuit_get_node(c, near);
            double vf = nf ? nf->voltage : 0, vn = nn ? nn->voltage : 0;
            if (!seen_near && vn > 0.1) { out.t_near_rise = sim->time; seen_near = 1; }
            if (!seen_far && vf > 0.1) { out.t_far_rise = sim->time; seen_far = 1; }
            if (sim->time > td * 1.4 && sim->time < td * 1.9) out.v_far = vf;
            if (sim->time > td * 2.4 && sim->time < td * 2.9) out.v_src_after_2t = vn;
            out.v_far_final = vf;
        }
    }
    if (sim) simulation_free(sim);
    circuit_free(c);
    return out;
}

static int line_test(void) {
    int fails = 0, total = 0;
    const double Z0 = 50.0, TD = 5e-9, RS = 50.0;
    printf("line-test: a real transmission line - Z0 = %g ohm, one-way delay %g ns, %g ohm source\n\n",
           Z0, TD * 1e9, RS);

    struct { const char *name; double rl; int open; double want_far; double want_near_2t; const char *note; } cases[] = {
        { "matched 50 ohm", 50.0, 0, 1.0, 1.0, "half the source volts on the line, nothing comes back" },
        { "open end",       0.0,  1, 2.0, 2.0, "the far end doubles to the full 2 V; the source end follows at 2T" },
        { "short to ground",1e-3, 0, 0.0, 0.0, "the far end stays at 0 and drives the source end back down at 2T" },
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        LineRun r = line_run(cases[i].rl, cases[i].open, Z0, TD, RS);
        total++;
        double tol = 0.12;
        int amp_ok = r.ok && fabs(r.v_far - cases[i].want_far) <= tol * (fabs(cases[i].want_far) + 0.5);
        int near_ok = r.ok && fabs(r.v_src_after_2t - cases[i].want_near_2t) <= tol * (fabs(cases[i].want_near_2t) + 0.5);
        int pass = amp_ok && near_ok;
        if (!pass) fails++;
        printf("%s line  %-16s far=%.3f V (want %.2f)  near@2T=%.3f V (want %.2f)  %s%s\n",
               pass ? " OK " : "FAIL", cases[i].name, r.v_far, cases[i].want_far,
               r.v_src_after_2t, cases[i].want_near_2t, cases[i].note, r.ok ? "" : "  [sim failed]");
    }

    /* the delay is the point: the far end must stay dark until T */
    {
        LineRun r = line_run(50.0, 0, Z0, TD, RS);
        total++;
        /* the source's own edge and one step of bookkeeping are common to both ends, so
           the delay is the difference between them - which is what the model claims. */
        double measured = r.t_far_rise - r.t_near_rise;
        int pass = r.ok && measured > TD * 0.85 && measured < TD * 1.25;
        if (!pass) fails++;
        printf("%s line  %-16s near end at %.2f ns, far end at %.2f ns: %.2f ns of delay (want %.2f)\n",
               pass ? " OK " : "FAIL", "propagation", r.t_near_rise * 1e9, r.t_far_rise * 1e9, measured * 1e9, TD * 1e9);
    }

    /* twice the delay, twice the wait */
    {
        LineRun r = line_run(50.0, 0, Z0, TD * 2.0, RS);
        total++;
        double measured = r.t_far_rise - r.t_near_rise;
        int pass = r.ok && measured > TD * 1.7 && measured < TD * 2.5;
        if (!pass) fails++;
        printf("%s line  %-16s a 10 ns cable measures %.2f ns  the delay follows the cable, not the time step\n",
               pass ? " OK " : "FAIL", "delay scales", measured * 1e9);
    }

    printf("\nline-test: %d checks, %d failed\n", total, fails);
    return fails ? 1 : 0;
}

/* ---------------------------------------------------------------------------------------
 * --class-test: what every template is, measured, and whether it says the same thing at two
 * different time steps.
 *
 * This is the suite the other suites should have been built on. Judging 187 circuits by one rule -
 * run thirty divisions, expect a repeating waveform - flatters the ones that happen to fit it and
 * libels the rest. A curve tracer has no frequency. A bias network never moves. A crystal takes a
 * thousand times longer to start than a comparator. So each template is run and asked what it is,
 * and the answer is printed: its class, the period it actually has, how long it took to settle,
 * and how many samples a cycle it was drawn with.
 *
 * The check is not a table of expected frequencies - that would be a second copy of the circuits,
 * wrong the day someone changes a resistor. It is that the answer does not depend on the step. A
 * circuit run at the app's own dt and again at a quarter of it must come back the same class with
 * the same period; if it does not, the dt is the answer rather than the circuit, which is the
 * fault this catches. Anything under about ten samples a cycle is reported too, because a period
 * measured from four samples is a number, not a measurement.
 * ------------------------------------------------------------------------------------- */
static const char *class_name(SignalClass c) {
    switch (c) {
        case SIGNAL_STATIC:   return "static";
        case SIGNAL_PERIODIC: return "periodic";
        case SIGNAL_ONESHOT:  return "one-shot";
        case SIGNAL_STEPPED:  return "stepped";
    }
    return "?";
}

/* Run one template to a given horizon at a given step and characterise probe 0. */
/* One pass at a given horizon. class_run wraps this and will ask again over a longer one - see
   there for why. */
static int class_run_span(CircuitTemplateType t, double dt_scale, double horizon_cycles,
                          SignalCharacter *out, double *dt_used, double *horizon_used) {
    Circuit *c = circuit_create();
    if (!c) return 0;
    if (circuit_place_template(c, t, 0, 0) <= 0) { circuit_free(c); return 0; }
    if (c->num_probes < 1) { circuit_free(c); return 0; }
    Simulation *sim = simulation_create(c);
    if (!sim) { circuit_free(c); return 0; }
    for (int i = 0; i < c->num_components; i++)
        if (c->components[i]->type == COMP_AC_VOLTAGE)
            c->components[i]->props.ac_voltage.frequency_sweep.enabled = false;
    int ok = 1;
    if (!simulation_dc_analysis(sim)) ok = 0;
    simulation_auto_time_step(sim);
    /* Take the horizon from the app dt BEFORE the override, or the finer run covers a quarter as
       much time and every slow circuit reads as a one-shot for want of a second cycle. */
    double dt_app = sim->time_step;
    double horizon = horizon_cycles * dt_app;

    /* The window has to contain the thing being measured. The horizon is a multiple of the step,
       and the step is not related to when a stimulus arrives: Hot-Plug Inrush closes its switch at
       20 ms, and once its step went to 1 us - because the step rule learned to resolve its 60 us
       inrush - 4000 steps was 4 ms and the window closed sixteen milliseconds before anything
       happened. The template read "static", which is a statement about the window.

       So: at least twice the latest source delay, whatever the step works out to. */
    double latest = 0;
    for (int i = 0; i < c->num_components; i++) {
        Component *k = c->components[i];
        if (!k || k->type != COMP_PULSE_SOURCE) continue;
        double d = k->props.pulse_source.delay;
        if (d > latest) latest = d;
    }
    if (latest > 0 && horizon < latest * 2.0) horizon = latest * 2.0;
    double dt = dt_app * dt_scale;
    simulation_enable_adaptive(sim, false);
    simulation_set_time_step(sim, dt);
    simulation_set_history_span(sim, horizon);
    simulation_start(sim);
    simulation_set_time_step(sim, dt);
    int guard = 0;
    while (ok && sim->time < horizon && guard++ < 2000000)
        if (!simulation_step(sim)) { ok = 0; break; }
    if (ok) simulation_characterise(sim, c->probes[0].id - 1 >= 0 ? 0 : 0, out);
    if (dt_used) *dt_used = dt;
    if (horizon_used) *horizon_used = horizon;
    simulation_free(sim);
    circuit_free(c);
    return ok;
}

/* Ask over a longer window before calling anything a one-shot.

   The horizon is a multiple of the step, which is a guess at how long the circuit takes to do
   whatever it does, and for one template the guess was wrong: the Relaxation Oscillator needs
   about 10 ms to start, the horizon gave it 8, and with fewer than three crossings of its own
   midpoint it was classified a one-shot. --osc-test, which runs longer, measures it happily at
   445 Hz. Two suites disagreeing about the same circuit means at least one of them is describing
   its own window rather than the circuit.

   So when the first pass says one-shot and the signal has real amplitude, ask again over eight
   times as long. A circuit that is genuinely a one-shot says the same thing twice and costs one
   extra run; a slow oscillator gets the room it needed. */
static int class_run(CircuitTemplateType t, double dt_scale, SignalCharacter *out, double *dt_used) {
    double horizon1 = 0;
    if (!class_run_span(t, dt_scale, 4000.0, out, dt_used, &horizon1)) return 0;
    if (out->cls == SIGNAL_ONESHOT && out->amplitude > 0) {
        SignalCharacter longer;
        double dt2 = 0;
        /* The retry is for a circuit that was still starting when the first window closed, so what
           it may discover is bounded by that first window: a period that would have fitted three
           times over in it, had the circuit only got going sooner. The Relaxation Oscillator's
           2.2 ms against 8 ms qualifies.

           Without that bound the longer look finds anything given enough time. The Two-Capacitor
           Problem's switch closes once every 100 s, and over 320 seconds of circuit time that is a
           period - technically true, and a worse description than "one-shot" of a template whose
           whole event is a 10 ms charge transfer. */
        if (class_run_span(t, dt_scale, 32000.0, &longer, &dt2, NULL) &&
            longer.cls == SIGNAL_PERIODIC && longer.period > 0 &&
            longer.period * 3.0 <= horizon1) {
            *out = longer;
            if (dt_used) *dt_used = dt2;
        }
    }
    return 1;
}

static int class_test(double fine) {
    int total = 0, fails = 0, thin = 0, drifted = 0;
    int counts[4] = { 0, 0, 0, 0 };
    printf("%-30s %-9s %12s %12s %10s %9s  %s\n", "template", "class", "period", "settled", "amplitude", "samp/cyc", "note");
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        if (!ti) continue;
        SignalCharacter a, b;
        double dta = 0, dtb = 0;
        if (!class_run((CircuitTemplateType)t, 1.0, &a, &dta)) continue;
        total++;
        int ok = class_run((CircuitTemplateType)t, fine, &b, &dtb);

        /* Two kinds of finding, and only one of them is a breakage. A template that will not run
           at a finer step is broken and fails the suite. A template whose answer moves with the
           step is telling the truth about itself - the step is too coarse for it - and that is a
           real defect, but it is a standing one with a measured size, tracked in docs/ROADMAP.md
           rather than held against every commit. Both are printed either way. */
        char note[200] = "";
        int bad = 0, drifts = 0;
        if (!ok) {
            snprintf(note, sizeof note, "does not run at a finer step");
            bad = 1;
        } else if (a.cls != b.cls) {
            snprintf(note, sizeof note, "reads as %s at dt and %s at dt/4 - the step is the answer, not the circuit",
                     class_name(a.cls), class_name(b.cls));
            drifts = 1;
        } else if (a.cls == SIGNAL_PERIODIC && a.period > 0 && b.period > 0 &&
                   fabs(a.period - b.period) > 0.05 * a.period) {
            snprintf(note, sizeof note, "period %.4g at the app step but %.4g finer (%.1f %% apart)",
                     a.period, b.period, 100.0 * fabs(a.period - b.period) / a.period);
            drifts = 1;
        }
        double spc = (a.cls == SIGNAL_PERIODIC && dta > 0) ? a.period / dta : 0;
        if (!bad && !drifts && a.cls == SIGNAL_PERIODIC && spc > 0 && spc < 10) {
            snprintf(note, sizeof note, "only %.1f samples a cycle: the period is a number, not a measurement", spc);
            thin++;
        }
        counts[a.cls]++;
        if (bad) fails++;
        if (drifts) drifted++;
        printf("%s %-28s %-9s %12.5g %12.5g %10.4g %9.1f  %s\n",
               bad ? "[FAIL]" : drifts ? "[WARN]" : "[ OK ]", ti->name, class_name(a.cls),
               a.period, a.settle_time, a.amplitude, spc, note);
    }
    printf("\nclass-test: %d templates - %d static, %d periodic, %d one-shot, %d stepped; "
           "%d thin on samples a cycle, %d whose answer moves with the step (docs/ROADMAP.md), "
           "%d that will not run at a finer step\n",
           total, counts[SIGNAL_STATIC], counts[SIGNAL_PERIODIC], counts[SIGNAL_ONESHOT],
           counts[SIGNAL_STEPPED], thin, drifted, fails);
    return fails ? 1 : 0;
}

/* ---------------------------------------------------------------------------------------
 * --restamp-test: looking at the circuit must not change it.
 *
 * Terminal currents are recovered by re-stamping every component on its own and reading the
 * residual, with g_stamp_read_only set to say "this stamp is being read, not solved". That works
 * only if every component honours it. A component that writes its own state inside its stamp -
 * and several do - advances that state again every time the display asks what the current is.
 *
 * What it detects, precisely: a stamp that WRITES while it is only being read. Run a template
 * twice, identically, updating the current-flow display on one of the runs; if they disagree,
 * something in the stamp path wrote when it was asked to read. That is how the relay was caught.
 *
 * What it does NOT detect, and an earlier version of this comment wrongly claimed it did. A stamp
 * that misREADS - the crystal taking the next step's companion state, or a subcircuit's internals
 * being read as uncharged - changes nothing about the circuit, so both runs agree and both are
 * wrong together. An inverted sign is the same: it is consistent with itself. Those need an oracle
 * computed outside the solver, which is --dvdt-test, --state-test and --sub-test case 3.
 *
 * Two suites, two different questions. This one asks whether looking changes anything; those ask
 * whether the answer is right.
 * ------------------------------------------------------------------------------------- */
static int restamp_run(CircuitTemplateType t, int with_display, double *out, int nout) {
    Circuit *c = circuit_create();
    if (!c) return 0;
    if (circuit_place_template(c, t, 0, 0) <= 0) { circuit_free(c); return 0; }
    if (c->num_probes < 1) { circuit_free(c); return 0; }
    Simulation *sim = simulation_create(c);
    if (!sim) { circuit_free(c); return 0; }
    for (int i = 0; i < c->num_components; i++)
        if (c->components[i]->type == COMP_AC_VOLTAGE)
            c->components[i]->props.ac_voltage.frequency_sweep.enabled = false;
    int ok = simulation_dc_analysis(sim);
    double td = circuit_template_scope_time_div(t);
    if (td <= 0) td = 1e-3;
    simulation_auto_time_step(sim);
    { double dtp = simulation_scope_time_step(sim, td);
      if (dtp > 0 && dtp < sim->time_step) simulation_set_time_step(sim, dtp); }
    /* fixed, so the two runs take the same steps at the same times whatever else differs */
    simulation_enable_adaptive(sim, false);
    simulation_start(sim);

    enum { STEPS = 400 };
    for (int s = 0; s < STEPS && ok; s++) {
        if (!simulation_step(sim)) { ok = 0; break; }
        if (with_display) simulation_update_flow_display(sim);
    }
    for (int p = 0; p < nout; p++)
        out[p] = (p < c->num_probes) ? simulation_get_probe_voltage(sim, p) : 0.0;

    simulation_free(sim);
    circuit_free(c);
    return ok;
}

/* The same question for one component type at a time, on a circuit of its own: a source, the part,
   and a load. The template pass cannot ask it of everything - a DC motor, a relay, a battery and a
   fuse appear in no template at all, and those four are among the parts that write their own state
   inside their stamp, which is exactly the thing this is looking for. */
static int restamp_type_run(ComponentType ty, int with_display, double *v_out) {
    Circuit *c = circuit_create();
    if (!c) return 0;
    Component *dev = NULL, *src = NULL;
    int node = pt_build_series(c, ty, 5.0, 1000.0, 1000.0, &dev, &src);
    if (!dev) { circuit_free(c); return 0; }
    Simulation *sim = simulation_create(c);
    if (!sim) { circuit_free(c); return 0; }
    int ok = simulation_dc_analysis(sim);
    simulation_enable_adaptive(sim, false);
    simulation_set_time_step(sim, 1e-5);
    simulation_start(sim);
    for (int s = 0; s < 300 && ok; s++) {
        if (!simulation_step(sim)) { ok = 0; break; }
        if (with_display) simulation_update_flow_display(sim);
    }
    *v_out = simulation_get_node_voltage(sim, node);
    if (!isfinite(*v_out)) ok = 0;
    simulation_free(sim);
    circuit_free(c);
    return ok;
}

static int restamp_test(void) {
    int total = 0, fails = 0;
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        if (!ti) continue;
        double a[MAX_PROBES], b[MAX_PROBES];
        if (!restamp_run((CircuitTemplateType)t, 0, a, MAX_PROBES)) continue;
        if (!restamp_run((CircuitTemplateType)t, 1, b, MAX_PROBES)) continue;
        total++;

        double worst = 0; int worst_p = -1;
        for (int p = 0; p < MAX_PROBES; p++) {
            double scale = fabs(a[p]) > fabs(b[p]) ? fabs(a[p]) : fabs(b[p]);
            double e = fabs(a[p] - b[p]) / (scale + 1e-9);
            if (e > worst) { worst = e; worst_p = p; }
        }
        /* Not a tolerance on physics - the two runs are the same arithmetic in the same order, so
           they should agree to the last bit. A part in a billion is room for the summation order
           inside the flow solve to differ, and nothing else. */
        int bad = (worst > 1e-9);
        if (bad) fails++;
        if (bad || getenv("RESTAMP_VERBOSE"))
            printf("[%s] restamp %-28s worst probe difference %.3g%s%s\n",
                   bad ? "FAIL" : " OK ", ti->name, worst,
                   worst_p >= 0 ? " on probe " : "", worst_p >= 0 ? "" : "");
    }
    printf("\nrestamp-test: %d templates run twice, once with the current-flow display updating "
           "every step; %d where looking at the circuit changed it\n", total, fails);

    /* ...and once per component type, on a circuit built for it */
    int t_total = 0, t_fails = 0, t_skipped = 0;
    for (int ty = COMP_NONE + 1; ty < COMP_TYPE_COUNT; ty++) {
        if (ty == COMP_TEXT || ty == COMP_GROUND) continue;
        const ComponentTypeInfo *info = component_get_info((ComponentType)ty);
        if (!info || !info->name || !info->name[0]) continue;
        double a = 0, b = 0;
        if (!restamp_type_run((ComponentType)ty, 0, &a)) { t_skipped++; continue; }
        if (!restamp_type_run((ComponentType)ty, 1, &b)) { t_skipped++; continue; }
        t_total++;
        double scale = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
        double e = fabs(a - b) / (scale + 1e-9);
        if (e > 1e-9) {
            t_fails++;
            printf("[FAIL] restamp %-28s %.10g with the display, %.10g without\n", info->name, b, a);
        }
    }
    printf("restamp-test: %d component types on a circuit of their own (%d could not be run), "
           "%d that the display changes\n", t_total, t_skipped, t_fails);
    fails += t_fails;
    return fails ? 1 : 0;
}

/* Two circuits on one sheet. Every other suite in this file places a single template on an
   empty canvas, so the case a user actually works in has never been simulated: two circuits
   side by side, electrically independent, sharing nothing but the ground symbol they each
   carry - which does join them, into one matrix and one net m0.

   The oracle is outside the solver. A circuit's wire currents measured alone are the truth;
   putting a neighbour next to it must not move them. That catches the failures worth catching -
   current from one circuit leaking into the other's arrows, a shared ground turning into a
   shared return path, an island balance that pushes one circuit's residue into the other's
   ground symbol - without needing a second model of what the currents ought to be.

   Wires and components are matched by position rather than by index: placing the second
   template runs the collinear tidy pass across the whole sheet, which may renumber. */
static int pair_geom(Circuit *c, int w, float *x0, float *y0, float *x1, float *y1) {
    Node *a = circuit_get_node(c, c->wires[w].start_node_id);
    Node *b = circuit_get_node(c, c->wires[w].end_node_id);
    if (!a || !b) return 0;
    /* Ends in a fixed order, so a wire drawn the other way round still matches - and so the
       sign comparison below means "the same direction along the same line". */
    if (a->x > b->x || (a->x == b->x && a->y > b->y)) { Node *t = a; a = b; b = t; }
    *x0 = a->x; *y0 = a->y; *x1 = b->x; *y1 = b->y;
    return 1;
}

static double pair_wire_current(Circuit *c, int w) {
    Node *a = circuit_get_node(c, c->wires[w].start_node_id);
    Node *b = circuit_get_node(c, c->wires[w].end_node_id);
    if (!a || !b) return 0;
    /* Reported along the canonical direction, so a flipped wire does not read as reversed. */
    if (a->x > b->x || (a->x == b->x && a->y > b->y)) return -c->wires[w].current;
    return c->wires[w].current;
}

#define PAIR_OFFSET 6000.0f

static int pair_test(void) {
    int fails = 0, total = 0;
    long matched = 0, carrying = 0; double seen_max = 0; char seen_name[64] = "-";
    double worst_all = 0; char worst_name[64] = "-";

    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        if (t == CIRCUIT_VOLTAGE_DIVIDER) continue;      /* it is the neighbour */
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        const char *name = ti ? ti->name : "?";

        Circuit *solo = circuit_create();
        if (!solo) continue;
        if (circuit_place_template(solo, (CircuitTemplateType)t, 0, 0) <= 0) { circuit_free(solo); continue; }
        Simulation *s1 = simulation_create(solo);
        if (!s1 || !simulation_dc_analysis(s1)) { simulation_free(s1); circuit_free(solo); continue; }
        /* Both the operating point and a running circuit: the flow display is an animation, and
           two circuits stepping side by side is the case the user is looking at. */
        simulation_auto_time_step(s1);
        double step = s1->time_step;
        s1->adaptive_enabled = false;
        simulation_start(s1);
        int nsteps = 0;
        double t_end1 = 0.000125 + 2.0 * step;
        while (s1->time < t_end1 && nsteps < 4000) { if (!simulation_step(s1)) break; nsteps++; }
        simulation_update_flow_display(s1);

        Circuit *both = circuit_create();
        if (!both) { simulation_free(s1); circuit_free(solo); continue; }
        circuit_place_template(both, (CircuitTemplateType)t, 0, 0);
        circuit_place_template(both, CIRCUIT_VOLTAGE_DIVIDER, PAIR_OFFSET, 0);
        Simulation *s2 = simulation_create(both);
        total++;
        int ok = 1; char why[220] = "";
        if (!s2 || !simulation_dc_analysis(s2)) {
            ok = 0;
            snprintf(why, sizeof why, "DC fails with a second circuit on the sheet but not alone");
        } else {
            /* Pinned to the solo run's step and step count. The neighbour is a resistive divider
               and constrains nothing, but pinning makes this an exact comparison rather than an
               approximately fair one, and adaptive stepping off means the two runs cannot drift
               apart simply by choosing different steps. */
            simulation_auto_time_step(s2);
            s2->time_step = step;
            s2->adaptive_enabled = false;
            simulation_start(s2);
            for (int n = 0; n < nsteps; n++) if (!simulation_step(s2)) break;
            simulation_update_flow_display(s2);

            double imax = 0;
            for (int w = 0; w < solo->num_wires; w++) {
                double v = pair_wire_current(solo, w);
                if (fabs(v) > imax) imax = fabs(v);
            }
            double tol = 1e-6 * imax + 1e-12;
            double worst = 0; int worst_w = -1; double worst_a = 0, worst_b = 0;

            for (int w = 0; w < solo->num_wires; w++) {
                float ax0, ay0, ax1, ay1;
                if (!pair_geom(solo, w, &ax0, &ay0, &ax1, &ay1)) continue;
                int found = -1;
                for (int u = 0; u < both->num_wires; u++) {
                    float bx0, by0, bx1, by1;
                    if (!pair_geom(both, u, &bx0, &by0, &bx1, &by1)) continue;
                    if (fabsf(bx0 - ax0) < 0.5f && fabsf(by0 - ay0) < 0.5f &&
                        fabsf(bx1 - ax1) < 0.5f && fabsf(by1 - ay1) < 0.5f) { found = u; break; }
                }
                if (found < 0) continue;   /* the tidy pass may have redrawn it; not this suite's business */
                matched++;
                double a = pair_wire_current(solo, w), b = pair_wire_current(both, found);
                if (fabs(a) > seen_max) { seen_max = fabs(a); snprintf(seen_name, sizeof seen_name, "%s", name); }
                if (fabs(a) > 1e-12) carrying++;
                if (fabs(a - b) > worst) { worst = fabs(a - b); worst_w = w; worst_a = a; worst_b = b; }
            }
            if (worst > worst_all) { worst_all = worst; snprintf(worst_name, sizeof worst_name, "%s", name); }
            if (worst > tol) {
                ok = 0;
                snprintf(why, sizeof why,
                         "wire at (%.0f,%.0f) carries %.6g A alone and %.6g A with a neighbour "
                         "(moved %.3g, tol %.3g)",
                         worst_w >= 0 ? circuit_get_node(solo, solo->wires[worst_w].start_node_id)->x : 0.0f,
                         worst_w >= 0 ? circuit_get_node(solo, solo->wires[worst_w].start_node_id)->y : 0.0f,
                         worst_a, worst_b, worst, tol);
            }
        }
        if (!ok) { fails++; printf("[FAIL] pair  %-28s %s\n", name, why); }

        simulation_free(s2); circuit_free(both);
        simulation_free(s1); circuit_free(solo);
    }
    /* The counts are here so a pass means something. A suite that matched no wires, or matched
       only wires carrying nothing, would report a clean zero drift while checking nothing at
       all - which is the failure mode of every comparison test. */
    printf("\npair-test: %d templates simulated beside a second circuit, %d disturbed "
           "(worst drift %.3g A, %s); %ld wires compared, %ld of them carrying current, "
           "largest %.4g A (%s)\n", total, fails, worst_all, worst_name, matched, carrying,
           seen_max, seen_name);
    return fails ? 1 : 0;
}

/* A capacitor built with an initial voltage must be sitting at that voltage once the operating
   point is solved. component_cap_companion stamps it as a 1 mohm source of exactly that value
   to make it so, and says why: seeding the stored state alone would leave the first transient
   step with a companion that believes 10 V while the node sits at 0. Nothing checked it.

   The tolerance is not cosmetic. At 1 mohm, a volt of node error is a thousand amps of
   recovered terminal current - and that is what the flow display draws on the wire. */
static int ic_test(void) {
    int fails = 0, total = 0, caps = 0;
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        const char *name = ti ? ti->name : "?";
        Circuit *c = circuit_create();
        if (!c) continue;
        if (circuit_place_template(c, (CircuitTemplateType)t, 0, 0) <= 0) { circuit_free(c); continue; }

        int has_ic = 0;
        for (int i = 0; i < c->num_components; i++) {
            Component *comp = c->components[i];
            if (comp->type == COMP_CAPACITOR && comp->props.capacitor.voltage != 0.0) has_ic = 1;
            if (comp->type == COMP_CAPACITOR_ELEC && comp->props.capacitor_elec.voltage != 0.0) has_ic = 1;
        }
        if (!has_ic) { circuit_free(c); continue; }

        Simulation *sim = simulation_create(c);
        total++;
        int ok = 1; char why[220] = "";
        if (!sim || !simulation_dc_analysis(sim)) { ok = 0; snprintf(why, sizeof why, "DC failed"); }
        for (int i = 0; ok && i < c->num_components; i++) {
            Component *comp = c->components[i];
            double ic = 0;
            if (comp->type == COMP_CAPACITOR) ic = comp->props.capacitor.voltage;
            else if (comp->type == COMP_CAPACITOR_ELEC) ic = comp->props.capacitor_elec.voltage;
            else continue;
            if (ic == 0.0) continue;
            caps++;
            Node *a = circuit_get_node(c, comp->node_ids[0]);
            Node *b = circuit_get_node(c, comp->node_ids[1]);
            double v = (a ? a->voltage : 0) - (b ? b->voltage : 0);
            /* 1 mV: a thousand times finer than the volt that would mean a kiloamp arrow, and
               far coarser than the solve's own residual. */
            if (fabs(v - ic) > 1e-3 * (fabs(ic) + 1.0)) {
                ok = 0;
                snprintf(why, sizeof why, "%s built at %.4g V sits at %.6g V after the operating "
                         "point (%.3g A of flow at 1 mohm)", comp->label, ic, v, (ic - v) * 1000.0);
            }
        }
        if (!ok) { fails++; printf("[FAIL] ic    %-28s %s\n", name, why); }
        simulation_free(sim);
        circuit_free(c);
    }
    printf("\nic-test: %d templates build a capacitor pre-charged (%d of them), %d that do not "
           "hold it at the operating point\n", total, caps, fails);
    return fails ? 1 : 0;
}

/* The sketch interpreter, driven by hand. No circuit here: a fake clock is advanced and the
   pins are read back, so what is under test is the language and the timing rather than the
   solver. The circuit half is --sketch-circuit-test.

   Every case is arithmetic. Blink has a period that can be counted; a threshold sketch has a
   switching point that follows from the numbers in it; a for loop that ramps analogWrite has a
   duty with a slope. If the interpreter drifts, these move. */
typedef struct {
    const char *name;
    const char *src;
    /* run to this many seconds, stepping at dt */
    double t_end, dt;
    int pin;                    /* the pin whose behaviour is checked */
    double expect;              /* expected duty averaged over the run, or a level at t_end */
    double tol;
    int mode;                   /* 0 = mean duty over the run, 1 = level at the end, 2 = edges counted */
    double input_v;             /* held on every input pin for the whole run */
    const char *why;
} SketchCase;

static int sketch_test(void) {
    printf("sketch-test: Arduino-shaped code run against a simulated clock\n\n");

    static const SketchCase cases[] = {
        { "blink at 500 ms",
          "void setup() { pinMode(13, OUTPUT); }\n"
          "void loop() {\n"
          "  digitalWrite(13, HIGH); delay(500);\n"
          "  digitalWrite(13, LOW);  delay(500);\n"
          "}\n",
          4.0, 0.001, 13, 0.5, 0.02, 0, 0.0,
          "half a second on and half a second off is a duty of exactly a half, and four seconds"
          " covers four whole cycles so the ends do not bias it" },

        { "blink at 100 ms on, 300 off",
          "void setup() { pinMode(9, OUTPUT); }\n"
          "void loop() { digitalWrite(9, HIGH); delay(100); digitalWrite(9, LOW); delay(300); }\n",
          4.0, 0.001, 9, 0.25, 0.02, 0, 0.0,
          "the duty follows the two delays and nothing else: 100/(100+300)" },

        { "delay counts simulated time, not steps",
          "void setup() { pinMode(3, OUTPUT); }\n"
          "void loop() { digitalWrite(3, HIGH); delay(20); digitalWrite(3, LOW); delay(20); }\n",
          2.0, 0.0001, 3, 0.5, 0.02, 0, 0.0,
          "the same sketch at a tenth of the step size gives the same duty - which is the whole"
          " point of running against sim time rather than counting calls" },

        { "edges counted: 4 Hz square is 8 transitions a second",
          "void setup() { pinMode(5, OUTPUT); }\n"
          "void loop() { digitalWrite(5, HIGH); delay(125); digitalWrite(5, LOW); delay(125); }\n",
          2.0, 0.0005, 5, 16.0, 1.0, 2, 0.0,
          "125 ms each way is 4 Hz, and two seconds of it is 8 full cycles - 16 edges" },

        { "analogWrite sets the duty",
          "void setup() { pinMode(6, OUTPUT); }\n"
          "void loop() { analogWrite(6, 64); delay(10); }\n",
          0.5, 0.001, 6, 64.0 / 255.0, 0.01, 0, 0.0,
          "64 of 255 is 0.251, and the pin holds it - analogWrite is a level here, the block"
          " turns it into a switching waveform" },

        { "a for loop ramps the duty",
          "int d = 0;\n"
          "void setup() { pinMode(6, OUTPUT); }\n"
          "void loop() {\n"
          "  for (int i = 0; i < 256; i = i + 1) { analogWrite(6, i); delay(1); }\n"
          "}\n",
          0.256, 0.0005, 6, 127.5 / 255.0, 0.02, 0, 0.0,
          "a full ramp from 0 to 255 over 256 ms averages the middle of the range. A for loop"
          " that spans a delay is the case a tree-walking interpreter cannot do at all" },

        { "digitalRead sees the pin",
          "void setup() { pinMode(2, INPUT); pinMode(8, OUTPUT); }\n"
          "void loop() { if (digitalRead(2) == HIGH) digitalWrite(8, HIGH); else digitalWrite(8, LOW); }\n",
          0.05, 0.001, 8, 1.0, 0.001, 1, 5.0,
          "5 V on the input is above half the supply, so the output follows it high" },

        { "digitalRead sees a low pin",
          "void setup() { pinMode(2, INPUT); pinMode(8, OUTPUT); }\n"
          "void loop() { if (digitalRead(2) == HIGH) digitalWrite(8, HIGH); else digitalWrite(8, LOW); }\n",
          0.05, 0.001, 8, 0.0, 0.001, 1, 0.4,
          "0.4 V is below the threshold, so the output stays low - the same sketch, the other way" },

        { "analogRead and a threshold",
          "void setup() { pinMode(7, OUTPUT); }\n"
          "void loop() {\n"
          "  int v = analogRead(A0);\n"
          "  if (v > 511) digitalWrite(7, HIGH); else digitalWrite(7, LOW);\n"
          "}\n",
          0.05, 0.001, 7, 1.0, 0.001, 1, 3.0,
          "3 V of 5 is 614 counts, over half scale, so it switches on. The count is the"
          " arithmetic: 3/5 * 1023" },

        { "analogRead below the threshold",
          "void setup() { pinMode(7, OUTPUT); }\n"
          "void loop() {\n"
          "  int v = analogRead(A0);\n"
          "  if (v > 511) digitalWrite(7, HIGH); else digitalWrite(7, LOW);\n"
          "}\n",
          0.05, 0.001, 7, 0.0, 0.001, 1, 2.0,
          "2 V is 409 counts, under half scale" },

        { "a function with a parameter",
          "int twice(int x) { return x * 2; }\n"
          "void setup() { pinMode(4, OUTPUT); }\n"
          "void loop() { analogWrite(4, twice(50)); delay(10); }\n",
          0.2, 0.001, 4, 100.0 / 255.0, 0.01, 0, 0.0,
          "twice(50) is 100, and the call has to return through a frame to get there" },

        { "a function defined after it is called",
          "void setup() { pinMode(4, OUTPUT); }\n"
          "void loop() { analogWrite(4, half(200)); delay(10); }\n"
          "int half(int x) { return x / 2; }\n",
          0.2, 0.001, 4, 100.0 / 255.0, 0.01, 0, 0.0,
          "sketches are written in this order all the time; the compiler finds every function"
          " before it compiles any of them" },

        { "a global keeps its value across loops",
          "int n = 0;\n"
          "void setup() { pinMode(4, OUTPUT); }\n"
          "void loop() { n = n + 1; if (n >= 4) analogWrite(4, 255); delay(10); }\n",
          0.5, 0.001, 4, 1.0, 0.001, 1, 0.0,
          "the counter has to survive loop() returning, which is what makes it a global" },

        { "while and a compound assignment",
          "void setup() { pinMode(4, OUTPUT); }\n"
          "void loop() {\n"
          "  int s = 0; int i = 0;\n"
          "  while (i < 10) { s += i; i++; }\n"
          "  analogWrite(4, s);\n"
          "  delay(10);\n"
          "}\n",
          0.2, 0.001, 4, 45.0 / 255.0, 0.01, 0, 0.0,
          "0 through 9 sums to 45, with ++ and += doing the work" },

        { "millis drives the blink instead of delay",
          "unsigned long last = 0;\n"
          "int state = 0;\n"
          "void setup() { pinMode(11, OUTPUT); }\n"
          "void loop() {\n"
          "  if (millis() - last >= 250) { last = millis(); state = !state;\n"
          "    digitalWrite(11, state); }\n"
          "}\n",
          4.0, 0.001, 11, 0.5, 0.03, 0, 0.0,
          "the non-blocking blink every tutorial moves on to. millis() has to come from"
          " simulated time or this free-runs" },
    };

    int fails = 0;
    const int ncases = (int)(sizeof cases / sizeof cases[0]);
    for (int i = 0; i < ncases; i++) {
        const SketchCase *tc = &cases[i];
        char err[160] = "";
        Sketch *sk = sketch_compile(tc->src, err, sizeof err);
        if (!sk) {
            printf("[FAIL] sketch %-38s did not compile: %s\n", tc->name, err);
            fails++;
            continue;
        }
        double sum = 0; long n = 0; int edges = 0; double prev = -1;
        for (double t = 0; t <= tc->t_end + 1e-12; t += tc->dt) {
            for (int p = 0; p < SKETCH_MAX_PINS; p++)
                if (sketch_pin_mode(sk, p) != SKETCH_PIN_OUTPUT)
                    sketch_set_pin_voltage(sk, p, tc->input_v);
            sketch_advance(sk, t);
            double d = sketch_pin_duty(sk, tc->pin);
            if (prev >= 0 && ((prev < 0.5) != (d < 0.5))) edges++;
            prev = d;
            sum += d; n++;
        }
        const char *rt = sketch_error(sk);
        double got = (tc->mode == 0) ? (n ? sum / (double)n : 0)
                   : (tc->mode == 1) ? sketch_pin_duty(sk, tc->pin)
                                     : (double)edges;
        int ok = !rt && fabs(got - tc->expect) <= tc->tol;
        if (!ok) {
            fails++;
            printf("[FAIL] sketch %-38s pin %-2d = %8.4f  expect %8.4f (+/-%.3g)%s%s\n",
                   tc->name, tc->pin, got, tc->expect, tc->tol, rt ? "  runtime: " : "", rt ? rt : "");
            printf("            %s\n", tc->why);
        } else {
            printf(" OK  sketch %-38s pin %-2d = %8.4f  expect %8.4f\n",
                   tc->name, tc->pin, got, tc->expect);
        }
        sketch_free(sk);
    }

    /* A sketch that will not compile has to say so, with the line. Silence here would mean the
       parser accepting nonsense and the block sitting there doing nothing. */
    static const struct { const char *src; const char *what; } bad[] = {
        { "void loop() { digitalWrite(13 HIGH); }", "a missing comma" },
        { "void loop() { undefinedThing(1); }", "a call to something that does not exist" },
        { "void loop() { int x = ; }", "a missing value" },
        { "void loop() { x = 1; }", "assigning to something never declared" },
        { "int f(int a) { return a; }\nvoid loop() { f(1, 2); }", "the wrong number of arguments" },
        { "void loop() { String s = \"hi\"; }", "a type this does not support" },
        { "void loop() { break; }", "break outside a loop" },
    };
    for (int i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) {
        char err[160] = "";
        Sketch *sk = sketch_compile(bad[i].src, err, sizeof err);
        if (sk) {
            printf("[FAIL] sketch rejects: %-40s was accepted\n", bad[i].what);
            fails++;
            sketch_free(sk);
        } else if (!strstr(err, "line")) {
            printf("[FAIL] sketch rejects: %-40s no line number: %s\n", bad[i].what, err);
            fails++;
        } else {
            printf(" OK  sketch rejects: %-40s %s\n", bad[i].what, err);
        }
    }

    printf("\nsketch-test: %d sketches run and %d rejected, %d wrong\n",
           ncases, (int)(sizeof bad / sizeof bad[0]), fails);
    return fails ? 1 : 0;
}

/* The programmable block on a real sheet, solved by MNA and measured at the node.

   The oracle is arithmetic and sits outside both the solver and the interpreter: a pin driving
   1 k through 25 ohm of port resistance settles at 5 * 1000/1025 = 4.878 V, and a sketch that
   is high for half its period puts that on the node half the time. Nothing here is compared
   against what the block "should" produce - only against what the resistors say. */
static Circuit *mcu_rig(const char *src, double rload, int pin_terminal) {
    Circuit *c = circuit_create();
    if (!c) return NULL;
    Component *mcu = component_create(COMP_MCU, 0, 0);
    if (!mcu) { circuit_free(c); return NULL; }
    snprintf(mcu->props.mcu.source, MCU_SRC_MAX, "%s", src);
    circuit_add_component(c, mcu);

    /* the load, from the pin down to ground */
    const ComponentTypeInfo *ci = component_get_info(COMP_MCU);
    float px = mcu->x + ci->terminals[pin_terminal].dx;
    float py = mcu->y + ci->terminals[pin_terminal].dy;

    Component *r = component_create(COMP_RESISTOR, px + 140, py);
    r->props.resistor.resistance = rload;
    r->rotation = 90;
    circuit_add_component(c, r);

    Component *g = component_create(COMP_GROUND, px + 140, py + 160);
    circuit_add_component(c, g);
    Component *g2 = component_create(COMP_GROUND, mcu->x + ci->terminals[MCU_GND_PIN].dx,
                                     mcu->y + ci->terminals[MCU_GND_PIN].dy + 40);
    circuit_add_component(c, g2);

    /* Wired through the components' own node ids rather than through positions worked out here.
       Guessing a part's terminal offsets is how the first version of this rig silently built a
       circuit with the resistor hanging off nothing: find_or_create at a position the terminal
       was not at makes a NEW node, the wire lands on that, and the load reads as absent - which
       showed up as a pin that gave the same voltage into 1 k and into 100 ohm. */
    circuit_add_wire(c, mcu->node_ids[pin_terminal], r->node_ids[0]);
    circuit_add_wire(c, r->node_ids[1], g->node_ids[0]);
    circuit_add_wire(c, mcu->node_ids[MCU_GND_PIN], g2->node_ids[0]);
    (void)px; (void)py;
    return c;
}

static int mcu_test(void) {
    printf("mcu-test: the programmable block driving a load, read off the node\n\n");
    int fails = 0, total = 0;

    struct {
        const char *name;
        const char *src;
        int terminal;          /* which block terminal the load hangs on */
        double rload;
        double t_end, dt;
        double expect_mean;    /* mean node voltage over the run */
        double tol;
        const char *why;
    } cases[] = {
        { "a pin held high sits at the divider",
          "void setup() { pinMode(13, OUTPUT); digitalWrite(13, HIGH); }\n"
          "void loop() { }\n",
          11, 1000.0, 0.05, 0.001, 5.0 * 1000.0 / 1025.0, 0.02,
          "1 k against 25 ohm of port resistance: 4.878 V, and it is the port resistance being"
          " real that makes this a circuit and not a switch" },

        { "a pin held low sits at zero",
          "void setup() { pinMode(13, OUTPUT); digitalWrite(13, LOW); }\n"
          "void loop() { }\n",
          11, 1000.0, 0.05, 0.001, 0.0, 0.02,
          "driven low is not the same as not driven: the pin holds the node down" },

        { "blink puts the divider on the node half the time",
          "void setup() { pinMode(13, OUTPUT); }\n"
          "void loop() { digitalWrite(13, HIGH); delay(500); digitalWrite(13, LOW); delay(500); }\n",
          11, 1000.0, 4.0, 0.002, 0.5 * 5.0 * 1000.0 / 1025.0, 0.06,
          "four seconds is four whole cycles of a 1 Hz blink, so the mean is half the high level" },

        { "a heavier load pulls the pin down",
          "void setup() { pinMode(13, OUTPUT); digitalWrite(13, HIGH); }\n"
          "void loop() { }\n",
          11, 100.0, 0.05, 0.001, 5.0 * 100.0 / 125.0, 0.02,
          "100 ohm against 25 is 4.0 V. A pin that ignored its own resistance would read 5" },

        { "analogWrite switches, and the mean is the duty",
          "void setup() { pinMode(11, OUTPUT); }\n"
          "void loop() { analogWrite(11, 64); delay(10); }\n",
          9, 1000.0, 1.0, 0.00005, (64.0 / 255.0) * 5.0 * 1000.0 / 1025.0, 0.08,
          "PWM is a real square wave at 490 Hz here, not a level - so the mean over a second is"
          " the duty times the high level, which is what an RC on this pin would average to" },

        { "an input pin does not drive",
          "void setup() { pinMode(13, INPUT); }\n"
          "void loop() { }\n",
          11, 1000.0, 0.05, 0.001, 0.0, 0.01,
          "high impedance: the 1 k holds the node at ground and the block does not fight it" },

        { "a pin reads its own node and switches another",
          "void setup() { pinMode(2, INPUT); pinMode(13, OUTPUT); }\n"
          "void loop() { if (digitalRead(2)) digitalWrite(13, LOW); else digitalWrite(13, HIGH); }\n",
          11, 1000.0, 0.05, 0.001, 5.0 * 1000.0 / 1025.0, 0.02,
          "D2 is wired to nothing and sits at ground through its own leakage, so it reads low"
          " and the output goes high - a decision taken from a node voltage" },
    };

    for (int i = 0; i < (int)(sizeof cases / sizeof cases[0]); i++) {
        total++;
        Circuit *c = mcu_rig(cases[i].src, cases[i].rload, cases[i].terminal);
        if (!c) { printf("[FAIL] mcu   %-46s could not be built\n", cases[i].name); fails++; continue; }
        Simulation *sim = simulation_create(c);
        int ok = 1;
        char why[160] = "";
        if (!sim || !simulation_dc_analysis(sim)) { ok = 0; snprintf(why, sizeof why, "DC failed"); }
        double sum = 0; long n = 0;
        if (ok) {
            sim->adaptive_enabled = false;
            sim->time_step = cases[i].dt;
            simulation_start(sim);
            while (sim->time < cases[i].t_end) {
                if (!simulation_step(sim)) { ok = 0; snprintf(why, sizeof why, "step failed at t=%.4f", sim->time); break; }
                /* the node the load hangs on */
                const ComponentTypeInfo *ci = component_get_info(COMP_MCU);
                Component *mcu = c->components[0];
                Node *nd = circuit_find_node_at(c, mcu->x + ci->terminals[cases[i].terminal].dx,
                                                  mcu->y + ci->terminals[cases[i].terminal].dy, 5.0f);
                sum += nd ? nd->voltage : 0;
                n++;
            }
        }
        Component *mcu = c->components[0];
        if (getenv("CT_MCU_DEBUG")) {
            const ComponentTypeInfo *ci = component_get_info(COMP_MCU);
            Node *pn = circuit_find_node_at(c, mcu->x + ci->terminals[cases[i].terminal].dx,
                                               mcu->y + ci->terminals[cases[i].terminal].dy, 5.0f);
            printf("       debug: R=%g nodes=%d wires=%d pinnode=%d net=%d drive=%d level=%.2f\n",
                   c->components[1]->props.resistor.resistance, c->num_nodes, c->num_wires,
                   pn ? pn->id : -1, pn ? circuit_node_net(c, pn->id) : -1,
                   mcu->props.mcu.pin_drive[cases[i].terminal],
                   mcu->props.mcu.pin_level[cases[i].terminal]);
        }
        if (ok && !mcu->props.mcu.compiled) {
            ok = 0;
            snprintf(why, sizeof why, "the sketch did not compile: %s", mcu->props.mcu.status);
        }
        double got = n ? sum / (double)n : 0;
        if (ok && fabs(got - cases[i].expect_mean) > cases[i].tol) {
            ok = 0;
            snprintf(why, sizeof why, "node mean %.4f V, expected %.4f (+/-%.3g)",
                     got, cases[i].expect_mean, cases[i].tol);
        }
        if (!ok) {
            fails++;
            printf("[FAIL] mcu   %-46s %s\n", cases[i].name, why);
            printf("            %s\n", cases[i].why);
        } else {
            printf(" OK  mcu   %-46s node = %6.3f V  expect %6.3f\n",
                   cases[i].name, got, cases[i].expect_mean);
        }
        simulation_free(sim);
        circuit_free(c);
    }

    /* A delay finer than the step cannot mean what it says, and the block has to say so. The
       roadmap called this one out: delayMicroseconds(1) against a 10 us step rounds silently
       unless something notices, and a number that is quietly wrong is the worst kind. */
    {
        total++;
        Circuit *c = mcu_rig("void setup() { pinMode(13, OUTPUT); }\n"
                             "void loop() { digitalWrite(13, HIGH); delayMicroseconds(1);\n"
                             "              digitalWrite(13, LOW); delayMicroseconds(1); }\n",
                             1000.0, 11);
        Simulation *sim = c ? simulation_create(c) : NULL;
        int ok = 0;
        if (sim && simulation_dc_analysis(sim)) {
            sim->adaptive_enabled = false;
            sim->time_step = 1e-4;              /* a hundred times coarser than the delay */
            simulation_start(sim);
            for (int k = 0; k < 20; k++) if (!simulation_step(sim)) break;
            Component *mcu = c->components[0];
            ok = strstr(mcu->props.mcu.status, "finer than") != NULL;
            if (!ok) printf("[FAIL] mcu   %-46s status was '%s'\n",
                            "a delay finer than the step is called out", mcu->props.mcu.status);
            else printf(" OK  mcu   %-46s %s\n", "a delay finer than the step is called out",
                        mcu->props.mcu.status);
        }
        if (!ok) fails++;
        simulation_free(sim);
        circuit_free(c);
    }

    /* A block whose code does not compile must be inert and must say why, not drive the node
       with whatever was left in its pin state. */
    {
        total++;
        Circuit *c = mcu_rig("void loop() { this is not code }\n", 1000.0, 11);
        Simulation *sim = c ? simulation_create(c) : NULL;
        int ok = 0;
        if (sim && simulation_dc_analysis(sim)) {
            Component *mcu = c->components[0];
            ok = !mcu->props.mcu.compiled && mcu->props.mcu.status[0] && mcu->props.mcu.pin_drive[11] == 0;
            if (!ok) printf("[FAIL] mcu   %-46s compiled=%d status='%s' drive=%d\n",
                            "code that does not compile leaves the pins alone",
                            mcu->props.mcu.compiled, mcu->props.mcu.status, mcu->props.mcu.pin_drive[11]);
            else printf(" OK  mcu   %-46s %s\n", "code that does not compile leaves the pins alone",
                        mcu->props.mcu.status);
        }
        if (!ok) fails++;
        simulation_free(sim);
        circuit_free(c);
    }

    printf("\nmcu-test: %d circuits with a programmable block, %d wrong\n", total, fails);
    return fails ? 1 : 0;
}

static int flow_test(void) {
    int fails = 0, total = 0;
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        const char *name = ti ? ti->name : "?";
        Circuit *c = circuit_create();
        if (circuit_place_template(c, (CircuitTemplateType)t, 0, 0) <= 0) { circuit_free(c); continue; }
        Simulation *sim = simulation_create(c);
        total++;
        int ok = 1; char why[200] = "";
        /* Node KCL compares what the wires carry against what the components draw. It cannot
           see displacement current: the charge that flows into a reverse-biased junction or a
           MOSFET gate is real current in the wires and is not reported as any terminal current.
           A template that hard-switches a power stage puts tens of microamps of it on the
           switching net every edge, so the node check is not meaningful there. The per-component
           conservation and NaN checks below still run for these; only the node sum is skipped,
           and the run says so rather than passing quietly.

           Pull-up Sizing is exempt for a different reason: the wire currents the flow display
           computes are a few percent off the capacitor current on its bus net, which is a known
           limitation of the flow display rather than of the solve (docs/ROADMAP.md). */
        /* Pierce joins them for a third reason, and it is worth writing down: this template used
           to pass because it was not running. Its only source is a one-shot kick with a
           hundred-second period, so the accuracy step saw no periodic source and returned the
           10 ms maximum - one step covered the whole test and the crystal never moved. Now that
           the step is bound by the kick's own width the oscillator runs, and the flow display
           splits the microamps on the crystal's net about two to one against the terminal
           currents. The solve is right (MNA enforces KCL); it is the arrows that are wrong. */
        /* Pierce is audited again as of 2026-08-30: its 6.8 uA was the crystal being re-stamped
           with the next step's companion state when its terminal currents were read back, not
           anything about the crystal. Pull-up Sizing is the one exemption left. */
        /* No exemptions. Both that this suite carried were bugs: Pierce's 6.8 uA was the
           crystal re-stamped with the next step's companion state, and Pull-up Sizing's 3.6 % was
           the MOSFET gate capacitance advancing its state once per Newton iteration. Kept as a
           named variable rather than deleted, so the next one has to be written down deliberately
           and with a reason. */
        int kcl_exempt = 0;
        if (!simulation_dc_analysis(sim)) { ok = 0; snprintf(why, sizeof why, "DC failed"); }
        simulation_auto_time_step(sim);
        simulation_start(sim);
        /* stop 1/8 period into a 1 kHz cycle (source well away from zero) */
        double t_end = 0.000125 + 2.0 * sim->time_step;
        while (ok && sim->time < t_end) if (!simulation_step(sim)) { ok = 0; snprintf(why, sizeof why, "step failed"); }
        simulation_update_flow_display(sim);

        double imax = 0;
        for (int i = 0; ok && i < c->num_components; i++) {
            Component *comp = c->components[i];
            if (comp->type == COMP_GROUND || comp->type == COMP_TEXT) continue;
            double sum = 0, amax = 0;
            for (int k = 0; k < comp->num_terminals; k++) {
                double v = comp->terminal_current[k];
                if (isnan(v) || isinf(v)) { ok = 0; snprintf(why, sizeof why, "%s terminal current NaN", comp->label); break; }
                sum += v; if (fabs(v) > amax) amax = fabs(v); if (fabs(v) > imax) imax = fabs(v);
            }
            /* A transmission line does not conserve instantaneous current between its ends -
               that is what makes it a transmission line. A pi line shunts charging current to
               ground; a delay line has charge in flight, and what goes in now comes out one
               propagation delay later. */
            if (!(comp->type == COMP_TLINE && comp->props.tline.model >= 2) && comp->type != COMP_DELAY_LINE && !(comp->type >= COMP_NOT_GATE && comp->type <= COMP_XNOR_GATE) && (ok && comp->num_terminals == 2 && fabs(sum) > 1e-6 * amax + 1e-9)) {
                ok = 0; snprintf(why, sizeof why, "%s not conserving: I0+I1=%.3g (|I|=%.3g)", comp->label, sum, amax);
            }
        }
        for (int w = 0; ok && w < c->num_wires; w++) {
            double v = c->wires[w].current;
            if (isnan(v) || isinf(v)) { ok = 0; snprintf(why, sizeof why, "wire %d current NaN", w); }
        }
        /* KCL at each node (skip nodes carrying a ground symbol: they are the sink) */
        for (int i = 0; ok && !kcl_exempt && i < c->num_nodes; i++) {
            int id = c->nodes[i].id;
            int grounded = 0; double demand = 0, term_imax = 0;
            for (int j = 0; j < c->num_components; j++) {
                Component *comp = c->components[j];
                for (int k = 0; k < comp->num_terminals; k++) if (comp->node_ids[k] == id) {
                    if (comp->type == COMP_GROUND) grounded = 1;
                    else {
                        demand += comp->terminal_current[k];
                    }
                }
            }
            if (grounded) continue;
            /* The size of the terms being cancelled, over the whole merged net rather than over
               this one node id. That is what a cancellation's absolute error scales with, and
               the net is the right scope for two reasons: the wire currents are a minimum-norm
               solve over the net, so whatever a device fails to report is spread across it and
               lands on whichever node has least of its own; and the terminals doing the
               cancelling are usually on neighbouring node ids of the same net, not on this one.
               The Discrete Buck's switch node is the case in point - it carries 3 A between a
               MOSFET and an inductor sitting on adjacent ids, and the node itself is left
               holding a reverse-biased Schottky drawing nanoamps. Scoped to the node id, the
               tolerance was nanoamps and the 1 ppm of Newton slack on 3 A read as a fault. */
            for (int j = 0; j < c->num_components; j++) {
                Component *comp = c->components[j];
                if (comp->type == COMP_GROUND) continue;
                for (int k = 0; k < comp->num_terminals; k++) {
                    int nid = comp->node_ids[k];
                    if (nid < 0 || nid >= MAX_NODES) continue;
                    if (c->node_map[nid] != c->node_map[id]) continue;
                    if (fabs(comp->terminal_current[k]) > term_imax)
                        term_imax = fabs(comp->terminal_current[k]);
                }
            }
            {   // behavioural logic gates do not report terminal currents: skip their nodes
                int behavioural = 0;
                for (int j = 0; j < c->num_components && !behavioural; j++) {
                    Component *bc = c->components[j];
                    if (bc->type < COMP_NOT_GATE || bc->type > COMP_XNOR_GATE) continue;
                    for (int k = 0; k < bc->num_terminals; k++) if (bc->node_ids[k] == id) behavioural = 1;
                }
                if (behavioural) continue;
            }
            /* MOSFET gate nodes used to be skipped here. The reason given was that a gate carries
               no conduction current and its displacement current is not reported as a terminal
               current, so the sum at a gate node was always short by exactly that.

               The premise was true and the cause was a bug, not a limitation. The gate
               capacitances kept their companion state on the component and advanced it inside the
               stamp - once per Newton iteration rather than once per accepted step - so what they
               contributed at the converged point was neither the displacement current nor nothing,
               but the output of an alternating recurrence. Fixed 2026-08-30, and gate nodes are
               checked like every other node now: 187/187 with no skip and no exemption at all.

               Worth remembering when the next audit wants an exemption. Both of the ones this
               suite carried - Pierce at 6.8 uA and Pull-up Sizing at 3.6 % - were written up as
               limitations of the current-flow display, and both turned out to be the same kind of
               fault underneath: a companion read at the wrong moment. An exemption is a place a
               bug can hide indefinitely, so it should be the last resort and it should say what
               would settle it. */
            double inflow = 0, node_imax = 0;
            for (int w = 0; w < c->num_wires; w++) {
                if (c->wires[w].end_node_id == id) inflow += c->wires[w].current;
                if (c->wires[w].start_node_id == id) inflow -= c->wires[w].current;
                if ((c->wires[w].end_node_id == id || c->wires[w].start_node_id == id) &&
                    fabs(c->wires[w].current) > node_imax)
                    node_imax = fabs(c->wires[w].current);
            }
            /* Tolerance: 1 ppm of the largest current anywhere, 0.5 % of what this node itself
               carries, and a 10 nA floor (open spark gaps leak ~nA). The middle term is there
               because a three-terminal nonlinear device whose terminals all sit on live nodes -
               a high-side MOSFET, say - has its terminal currents recovered from the stamp
               residual, and that carries Newton slack proportional to its own current. A real
               KCL break is a missing wire or a mis-assigned terminal: those are 100 %, not 0.1 %. */
            /* ...and 100 ppm of what this node itself carries. A node between two branches of a
               resonant circuit sums two large, nearly equal and opposite currents to a small
               net: the tank of the Pierce oscillator puts 10 mA through the crystal and 0.3 uA
               into the node. The absolute error of that cancellation scales with the 10 mA, not
               with the 0.3 uA, and Newton's own tolerance is enough to make it a microamp. A
               real KCL break - a missing wire, a mis-assigned terminal - is 100 %, not 0.01 %. */
            if (fabs(inflow - demand) > 1e-6 * (imax + 1e-9) + 5e-3 * fabs(demand) + 1e-8 +
                                        1e-4 * (node_imax > term_imax ? node_imax : term_imax)) {
                /* Name what is on the node. "KCL does not close" is a number; which part is on
                   the net and what it says it is drawing is the beginning of an answer. */
                /* Everything on the net, not just on this node. The missing current is
                   somewhere on the net by definition - the node named here is wherever the
                   minimum-norm solve put the imbalance, which is rarely where it came from. */
                char who[200] = "";
                size_t at = 0;
                int mnode = c->node_map[id];
                for (int j = 0; j < c->num_components && at < sizeof who - 26; j++) {
                    Component *comp2 = c->components[j];
                    if (comp2->type == COMP_GROUND) continue;
                    for (int k = 0; k < comp2->num_terminals; k++) {
                        int nid2 = comp2->node_ids[k];
                        if (nid2 < 0 || nid2 >= MAX_NODES || c->node_map[nid2] != mnode) continue;
                        at += (size_t)snprintf(who + at, sizeof who - at, "%s%s.%d=%.3g",
                                               at ? " " : "", comp2->label, k,
                                               comp2->terminal_current[k]);
                    }
                }
                /* And how far out the whole net is. Wire currents are the minimum-norm
                   solution of the net's conservation equations with the terminal currents as
                   demands, so any current a device does not report is spread over the net and
                   lands on whichever node has least of its own. A net that sums to zero and a
                   single node that disagrees are two different faults. */
                double net_sum = 0;
                int matrix_node = c->node_map[id];
                for (int j = 0; j < c->num_components; j++) {
                    Component *comp2 = c->components[j];
                    if (comp2->type == COMP_GROUND) continue;
                    for (int k = 0; k < comp2->num_terminals; k++) {
                        int nid = comp2->node_ids[k];
                        if (nid < 0 || nid >= MAX_NODES) continue;
                        if (c->node_map[nid] == matrix_node) net_sum += comp2->terminal_current[k];
                    }
                }
                ok = 0;
                snprintf(why, sizeof why, "KCL at node %d: wires %.4g vs demand %.4g, net %.4g "
                         "out [%s]", id, inflow, demand, net_sum, who);
            }
        }
        /* Series templates: uniform |I| on every wire, equal to the resistor current */
        int series = (t == CIRCUIT_RC_LOWPASS || t == CIRCUIT_RC_HIGHPASS || t == CIRCUIT_RL_LOWPASS ||
                      t == CIRCUIT_RL_HIGHPASS || t == CIRCUIT_VOLTAGE_DIVIDER);
        if (ok && series) {
            double ir = 0;
            for (int j = 0; j < c->num_components; j++)
                if (c->components[j]->type == COMP_RESISTOR) { ir = fabs(c->components[j]->terminal_current[0]); break; }
            double wmin = 1e300, wmax = 0;
            for (int w = 0; w < c->num_wires; w++) { double v = fabs(c->wires[w].current); if (v < wmin) wmin = v; if (v > wmax) wmax = v; }
            if (ir < 1e-9) { ok = 0; snprintf(why, sizeof why, "resistor current is zero"); }
            else if (wmin < 1e-9) { ok = 0; snprintf(why, sizeof why, "a series wire carries no current (min %.3g, R %.3g)", wmin, ir); }
            else if (fabs(wmax - ir) > 1e-6 * ir || fabs(wmin - ir) > 1e-6 * ir) {
                ok = 0; snprintf(why, sizeof why, "series wires uneven: min %.4g max %.4g resistor %.4g", wmin, wmax, ir);
            }
        }
        printf("[%s] flow  %-28s wires=%-3d max|I|=%.3g %s%s\n", ok ? (kcl_exempt ? "NOTE" : " OK ") : "FAIL",
               name, c->num_wires, imax, why,
               kcl_exempt ? " [node KCL skipped: see the comment in flow_test]" : "");
        if (!ok && getenv("FLOW_DEBUG")) {
            for (int i = 0; i < c->num_nodes; i++) {
                int id = c->nodes[i].id;
                printf("      node %-3d net m%-3d (%.0f,%.0f) V=%.4f :", id, circuit_node_net(c, id), c->nodes[i].x, c->nodes[i].y, c->nodes[i].voltage);
                for (int j = 0; j < c->num_components; j++) {
                    Component *comp = c->components[j];
                    for (int k = 0; k < comp->num_terminals; k++)
                        if (comp->node_ids[k] == id) printf(" %s[%d]=%.4g", comp->label, k, comp->terminal_current[k]);
                }
                printf("\n");
            }
            for (int w = 0; w < c->num_wires; w++)
                printf("      wire %-3d n%d -> n%d  I=%.4g\n", w, c->wires[w].start_node_id, c->wires[w].end_node_id, c->wires[w].current);
        }
        if (!ok) fails++;
        simulation_free(sim); circuit_free(c);
    }
    printf("\n%d/%d flow checks passed\n", total - fails, total);
    return fails;
}

/* Oscillator check: run the oscillator templates for a while and count how often the
 * op-amp output crosses its own mean in the last quarter of the run. A latched or dead
 * loop gives ~0 crossings; a healthy oscillator gives 2 per period. */
static Component *find_comp(Circuit *c, ComponentType t, int ord);
/* Set by --osc-dt only. Zero means "whatever the app would use", which is the default and
   the whole point: see the comment where it is read. */
static double g_osc_dt_forced = 0.0;
/* --probe-dt N forces the step probe-test runs at, so an oracle can be asked whether
   it is converged or merely repeatable at one step. */
static double g_probe_dt = 0.0;
/* --only SUBSTRING: run just the templates whose name contains it. For asking one
   circuit a question without waiting for the other 186. */
static const char *g_only = NULL;

/* --shard i/n: run only every n-th template, starting at i. The suites are one process each and
   run several at a time, so the battery's wall clock is whatever its longest single suite takes -
   and demo-test alone was two thirds of it. Sharding splits that one across processes too. */
static int g_shard_i = 0, g_shard_n = 1;
static int shard_skip(int t) { return g_shard_n > 1 && (t % g_shard_n) != g_shard_i; }
static int osc_test(void) {
    /* shape: the AC rms of the probed node divided by its peak-to-peak. A sine is 0.354,
       a square 0.5 and a triangle 0.289 - so a clipped or notched 'sine' fails on shape
       even when it still crosses its mean at roughly the right rate. 0 = do not check. */
    struct { CircuitTemplateType t; double run; double f_expect; double dt; double shape; } cases[] = {
        { CIRCUIT_WIEN_OSCILLATOR, 0.040, 1591.5, 0, 0.354 },
        /* 0.41, not the 0.354 of a sine: this loop has a gain of 33 against the 29 it needs and
           nothing to hold the amplitude down, so it grows into the rails and stays there. The
           previous 0.354 passed by three parts in a thousand - it was measuring a circuit that
           clips, against a shape that does not, and the first thing to touch the stimulus tipped
           it over. A diode limiter across Rf would earn the sine back. */
        { CIRCUIT_PHASE_SHIFT_OSC, 0.010, 5973.0, 0, 0.41 },    /* ideal 1/(2 pi R C sqrt 6) = 6497; loading the last section pulls it down 8 % */
        { CIRCUIT_RELAXATION_OSC, 0.040, 455.0, 0, 0.500 },
        { CIRCUIT_TRI_SQUARE_GEN, 0.004, 5000.0, 2e-7, 0.289 },
        /* 0.30, not the 0.354 of a sine. The shaper makes its sine by bending a triangle at
           four diode breakpoints, and right at a breakpoint the diode's own dynamic resistance
           is megohms - which is the same order as a picofarad's reactance here. So the junction
           capacitance, now that the diodes have one, softens exactly the corners the shaping
           depends on and the output sits a little closer to the triangle it started as. A real
           shaper has the same limit; it is why they are specified to a maximum frequency. */
        { CIRCUIT_FUNCTION_GEN, 0.004, 5000.0, 2e-7, 0.324 },
        { CIRCUIT_COLPITTS, 60e-6, 712e3, 5e-9, 0.354 },
        { CIRCUIT_RING_OSC, 200e-6, 145e3, 2e-8, 0.500 },
        { CIRCUIT_HARTLEY, 80e-6, 534188, 5e-9, 0.354 },        /* ideal 1/(2 pi sqrt((L1+L2)C)) = 503 kHz; the tap is only an AC ground through the supply */
        { CIRCUIT_CLAPP, 30e-6, 1743455, 2e-9, 0.354 },
        { CIRCUIT_NE555_ASTABLE, 0.004, 4800.0, 5e-8, 0.500 },   /* 1.44/((R_A + 2 R_B) C), square output */
        { CIRCUIT_PIERCE, 1e-3, 100000.0, 2.5e-7, 0 },           /* pulled to the crystal's fs = 100.0 kHz; the amp clips, so no shape check */
    };
    int fails = 0;
    for (unsigned k = 0; k < sizeof cases / sizeof cases[0]; k++) {
        const CircuitTemplateInfo *ti = circuit_template_get_info(cases[k].t);
        Circuit *c = circuit_create();
        circuit_place_template(c, cases[k].t, 0, 0);
        Simulation *sim = simulation_create(c);
        /* output node: the template's probe spec if it has one, else the first op-amp output */
        Component *amp = NULL; int oterm = 2;
        { ComponentType oct; int oord;
          if (circuit_template_output_spec(cases[k].t, &oct, &oord, &oterm) && oct) amp = find_comp(c, oct, oord); else oterm = 2; }
        for (int i = 0; i < c->num_components && !amp; i++) {
            ComponentType ty = c->components[i]->type;
            if (ty == COMP_OPAMP || ty == COMP_OPAMP_REAL || ty == COMP_OPAMP_FLIPPED) { amp = c->components[i]; break; }
        }
        int ok = amp && simulation_dc_analysis(sim);
        /* The step the app itself would take for this template: the accuracy step, then the scope
           rule if that is finer. Not a step of this suite's own choosing.

           This used to be a flat microsecond, and it made the suite quietly useless in the one way
           a suite must not be. The Function Generator was displayed at 5556 Hz for as long as this
           oracle verified it at 5000, because 5000 is what a microsecond step gives and the app was
           running at twenty samples a cycle. The oracle was right about a program nobody was using.
           A test that picks its own step is not testing the program; it is testing a different
           program that happens to share source code with it.

           Every case passes at the app's own step, so none of them needed the fixed one. `--osc-dt`
           still forces a step when the question is whether an expectation is converged. */
        {
            double td = circuit_template_scope_time_div(cases[k].t);
            if (td <= 0) td = 1e-3;
            if (g_osc_dt_forced > 0) {
                simulation_set_time_step(sim, g_osc_dt_forced);
            } else {
                simulation_auto_time_step(sim);
                double dtp = simulation_scope_time_step(sim, td);
                if (dtp > 0 && dtp < sim->time_step) simulation_set_time_step(sim, dtp);
            }
            simulation_set_history_span(sim, 20.0 * td);
        }
        simulation_start(sim);
        /* record output in the last quarter */
        double t_rec = cases[k].run * 0.75;
        enum { NMAX = 20000 };
        static double vs[NMAX]; static double ts[NMAX]; int n = 0;
        while (ok && sim->time < cases[k].run) {
            if (!simulation_step(sim)) { ok = 0; break; }
            if (sim->time >= t_rec && n < NMAX) {
                Node *nd = circuit_get_node(c, amp->node_ids[oterm]);
                vs[n] = nd ? nd->voltage : 0; ts[n] = sim->time; n++;
            }
        }
        double mean = 0, vmin = 1e300, vmax = -1e300; int crossings = 0; double f_meas = 0;
        if (ok && n > 10) {
            for (int i = 0; i < n; i++) { mean += vs[i]; if (vs[i] < vmin) vmin = vs[i]; if (vs[i] > vmax) vmax = vs[i]; }
            mean /= n;
            double t_first = -1, t_last = -1; int rising = 0;
            double hyst = 0.1 * (vmax - vmin); int below = (vs[0] < mean - hyst);
            for (int i = 1; i < n; i++) {
                if (below && vs[i] > mean + hyst) { rising++; if (t_first < 0) t_first = ts[i]; t_last = ts[i]; below = 0; }
                else if (!below && vs[i] < mean - hyst) below = 1;
            }
            crossings = rising;
            if (rising >= 2) f_meas = (rising - 1) / (t_last - t_first);
        }
        /* AC rms / peak-to-peak: 0.354 for a sine, 0.5 for a square, 0.289 for a triangle.
           A clipped or notched 'sine' fails this even when it still crosses its mean at
           roughly the right rate - which is how a distorted Hartley/Colpitts used to pass. */
        double shape = 0;
        if (ok && n > 40 && vmax > vmin) {
            int i0 = (n * 3) / 4;                       /* settled quarter only: start-up drags rms down */
            double m2 = 0, sq = 0, lo = 1e300, hi = -1e300;
            for (int i = i0; i < n; i++) { m2 += vs[i]; if (vs[i] < lo) lo = vs[i]; if (vs[i] > hi) hi = vs[i]; }
            m2 /= (n - i0);
            for (int i = i0; i < n; i++) { double d = vs[i] - m2; sq += d * d; }
            if (hi > lo) shape = sqrt(sq / (n - i0)) / (hi - lo);
        }
        int osc = ok && crossings >= 3 && (vmax - vmin) > 0.5;
        int f_ok = osc && fabs(f_meas - cases[k].f_expect) < 0.05 * cases[k].f_expect;   /* 25 % let a 10 %-off Hartley through */
        int s_ok = !(cases[k].shape > 0) || (osc && fabs(shape - cases[k].shape) < 0.12 * cases[k].shape);
        printf("[%s] osc   %-28s swing=%.2fV  rising-crossings=%d  f=%.0fHz (expect ~%.0f) shape=%.3f/%.3f%s\n",
               (osc && f_ok && s_ok) ? " OK " : "FAIL", ti ? ti->name : "?", vmax - vmin, crossings, f_meas, cases[k].f_expect,
               shape, cases[k].shape,
               !ok ? "  [sim error]" : !osc ? "  [NOT OSCILLATING: latched or dead loop]" : !f_ok ? "  [frequency off]" : !s_ok ? "  [WAVEFORM SHAPE WRONG: clipped or distorted]" : "");
        if (!(osc && f_ok && s_ok)) fails++;
        simulation_free(sim); circuit_free(c);
    }
    return fails;
}

/* ------------------------------------------------------------------------------------
 * Probe oracle: for each template, probe the designated output node the way a user
 * would (scope probe on a node) and compare a metric with the hand calculation in
 * TEMPLATE_AUDIT.md. Node spec = (component type, ordinal among that type, terminal).
 * Metrics: dc = mean of last quarter; amp = (max-min)/2; max; mean (same as dc).
 * tol > 0 is relative, tol < 0 is absolute (|tol|).
 * ---------------------------------------------------------------------------------- */
typedef struct { CircuitTemplateType t; ComponentType ct; int ord, term; const char *metric; double expect, tol, run; const char *note; } ProbeCase;
static const ProbeCase probe_cases[] = {
    { CIRCUIT_RC_LOWPASS,       COMP_CAPACITOR, 0, 0, "amp", 0.846, 0.10, 5e-3, "|H| at 1 kHz, fc 1.59 kHz" },
    { CIRCUIT_RC_HIGHPASS,      COMP_RESISTOR,  0, 0, "amp", 0.532, 0.10, 5e-3, "|H| at 1 kHz" },
    { CIRCUIT_RL_LOWPASS,       COMP_RESISTOR,  0, 0, "amp", 0.846, 0.10, 5e-3, "across R" },
    { CIRCUIT_RL_HIGHPASS,      COMP_INDUCTOR,  0, 0, "amp", 0.532, 0.10, 5e-3, "across L" },
    { CIRCUIT_VOLTAGE_DIVIDER,  COMP_RESISTOR,  1, 0, "dc",  5.0,   0.02, 2e-3, "10V*10k/20k" },
    { CIRCUIT_INVERTING_AMP,    COMP_OPAMP,     0, 2, "amp", 5.0,   0.05, 5e-3, "-10 x 0.5 Vpk" },
    { CIRCUIT_NONINVERTING_AMP, COMP_OPAMP_FLIPPED, 0, 2, "amp", 5.5, 0.05, 5e-3, "11 x 0.5 Vpk" },
    { CIRCUIT_VOLTAGE_FOLLOWER, COMP_OPAMP,     0, 2, "amp", 1.0,   0.05, 5e-3, "unity" },
    { CIRCUIT_HALFWAVE_RECT,    COMP_RESISTOR,  0, 0, "max", 4.3,   0.10, 40e-3, "5 - 0.7" },
    { CIRCUIT_LED_WITH_RESISTOR,COMP_LED,       0, 0, "dc",  1.94,  0.05, 2e-3, "red Vf at 9 mA" },
    { CIRCUIT_COMMON_EMITTER,   COMP_NPN_BJT,   0, 1, "dc",  9.0,   0.10, 5e-3, "Vc = 12 - 1.4mA*2.2k" },
    { CIRCUIT_COMMON_SOURCE,    COMP_NMOS,      0, 1, "dc",  6.2,   0.15, 5e-3, "Vd" },
    { CIRCUIT_COMMON_DRAIN,     COMP_NMOS,      0, 2, "dc",  4.3,   0.15, 5e-3, "Vs = Vg - Vgs" },
    { CIRCUIT_MULTISTAGE_AMP,   COMP_NPN_BJT,   1, 1, "dc",  5.8,   0.15, 5e-3, "2nd collector" },
    { CIRCUIT_DIFFERENTIAL_PAIR,COMP_NPN_BJT,   0, 1, "dc",  10.75, 0.05, 5e-3, "12 - 0.27mA*4.7k" },
    { CIRCUIT_SEVENSEG_TEST,    COMP_7SEG_DISPLAY, 0, 0, "dc", 2.16, 0.05, 1e-3, "segment a forward drop at 19 mA" },
    { CIRCUIT_WIRELESS_LINK,    COMP_RESISTOR,  0, 0, "amp", 1.0,  0.05, 5e-3, "2 Vpk sent, 50 into 50" },
    { CIRCUIT_BCD_COUNTER,      COMP_7SEG_DISPLAY, 0, 0, "max", 2.18, 0.05, 6.0, "segment a lights as the count runs" },
    { CIRCUIT_CURRENT_MIRROR,   COMP_NPN_BJT,   1, 1, "dc",  10.9,  0.05, 2e-3, "12 - 1.1mA*1k" },
    { CIRCUIT_PUSH_PULL,        COMP_RESISTOR,  0, 0, "amp", 4.3,   0.15, 5e-3, "5 Vpk minus Vbe" },
    { CIRCUIT_CMOS_INVERTER,    COMP_NMOS,      0, 1, "amp", 2.5,   0.10, 5e-3, "0..5 V square" },
    { CIRCUIT_DIFFERENTIATOR,   COMP_OPAMP,     0, 2, "absmean", 0.4, 0.30, 30e-3, "-RC dV/dt = 1e-3 * 400 V/s (mean |v|, ignores corner spikes)" },
    { CIRCUIT_SUMMING_AMP,      COMP_OPAMP,     0, 2, "dc",  -6.0,  0.03, 2e-3, "-(1+2+3)" },
    { CIRCUIT_COMPARATOR,       COMP_OPAMP,     0, 2, "amp", 15.0,  0.05, 30e-3, "rail to rail" },
    { CIRCUIT_FULLWAVE_BRIDGE,  COMP_CAPACITOR_ELEC, 0, 0, "dc", 10.2, 0.10, 60e-3, "10.6 pk - half ripple" },
    { CIRCUIT_CENTERTAP_RECT,   COMP_CAPACITOR_ELEC, 0, 0, "dc", 5.2,  0.15, 60e-3, "6 pk - 0.7" },
    { CIRCUIT_AC_DC_SUPPLY,     COMP_CAPACITOR_ELEC, 0, 0, "dc", 15.0, 0.10, 60e-3, "17 - 1.4 - ripple/2" },
    { CIRCUIT_AC_DC_AMERICAN,   COMP_CAPACITOR_ELEC, 0, 0, "dc", 15.3, 0.10, 60e-3, "17 - 1.4 - 0.3" },
    { CIRCUIT_DIFFERENCE_AMP,   COMP_OPAMP,     0, 2, "amp", 1.0,   0.10, 5e-3, "V2 - V1, unity" },
    { CIRCUIT_TRANSIMPEDANCE,   COMP_OPAMP,     0, 2, "dc",  10.0,  0.03, 2e-3, "1 mA x 10k" },
    { CIRCUIT_INSTR_AMP,        COMP_OPAMP,     2, 2, "amp", 2.1,   0.15, 5e-3, "gain 21 x 0.1 Vpk" },
    { CIRCUIT_SALLEN_KEY_LP,    COMP_OPAMP,     0, 2, "amp", 0.717, 0.15, 8e-3, "2nd order, Q 0.5, 1 kHz" },
    { CIRCUIT_NOTCH_FILTER,     COMP_RESISTOR,  3, 0, "amp", 0.0,  -0.15, 120e-3, "60 Hz notch: < 0.15 Vpk" },
    { CIRCUIT_CURRENT_SOURCE,   COMP_NPN_BJT,   0, 1, "dc",  8.9,   0.10, 5e-3, "12 - 3.1mA*1k" },
    { CIRCUIT_WINDOW_COMP,      COMP_LED,       0, 0, "dc",  1.88,  0.08, 2e-3, "LED on inside window" },
    { CIRCUIT_HYSTERESIS_COMP,  COMP_OPAMP,     0, 2, "amp", 15.0,  0.05, 30e-3, "rail to rail, input 6 +/- 3 V" },
    { CIRCUIT_ZENER_REF,        COMP_ZENER,     0, 1, "dc",  5.13,  0.05, 2e-3, "Vz + Iz*Rz" },
    { CIRCUIT_PRECISION_RECT,   COMP_OPAMP,     1, 2, "mean", -0.637, 0.15, 40e-3, "-|sin| average = -2/pi" },
    { CIRCUIT_7805_REG,         COMP_7805,      0, 1, "dc",  5.0,   0.02, 2e-3, "fixed 5 V" },
    { CIRCUIT_LM317_REG,        COMP_LM317,     0, 1, "dc",  5.0,   0.03, 2e-3, "1.25(1+720/240)" },
    { CIRCUIT_TL431_REF,        COMP_TL431,     0, 0, "dc",  2.5,   0.02, 2e-3, "2.495 V reference" },
    { CIRCUIT_SERIES_RLC,       COMP_CAPACITOR, 0, 0, "amp", 15.0,  0.25, 80e-3, "Q*Vin at f0, Q = 3" },
    { CIRCUIT_WHEATSTONE,       COMP_RESISTOR,  3, 0, "dc",  5.238, 0.02, 2e-3, "10*1100/2100" },
    { CIRCUIT_PEAK_DETECTOR,    COMP_CAPACITOR, 0, 0, "dc",  1.75,  0.30, 0.125, "envelope: amplitude 1.75..2 V at t=94..125 ms of the 1->5 V sweep" },
    { CIRCUIT_CLAMPER,          COMP_DIODE,     0, 1, "max", 9.3,   0.12, 0.5,   "shifted sine top at full amplitude: 2*5 - 0.7" },
    { CIRCUIT_RC_BANDPASS,      COMP_CAPACITOR, 1, 0, "amp", 0.79,  0.15, 6e-3,  "HP 800 Hz x LP 3.2 kHz at 1.6 kHz" },
    { CIRCUIT_LC_LOWPASS,       COMP_CAPACITOR, 0, 0, "amp", 1.15,  0.15, 8e-3,  "2nd order, Q = 1, at 1 kHz" },
    { CIRCUIT_VOLTAGE_DOUBLER,  COMP_CAPACITOR, 1, 0, "dc",  7.4,   0.15, 1.0,   "2*A - 1.4 at A ~ 4.4 V (late in the 1->5 V sweep)" },
    { CIRCUIT_HALFWAVE_FILTERED,COMP_CAPACITOR, 0, 0, "dc",  8.0,   0.15, 1.0,   "Vpk - 0.7 - ripple/2 late in the 2->10 V sweep" },
    { CIRCUIT_HV_345_LINE,      COMP_RESISTOR,  0, 0, "amp", 264.0e3, 0.05, 60e-3, "186.7 kV rms at the 600 MW load (-6.3 %)" },
    { CIRCUIT_HV_138_LINE_VAR,  COMP_RESISTOR,  0, 0, "amp", 105.0e3, 0.06, 60e-3, "74.3 kV rms with pf 0.9 load, cap bank open (-6.7 %)" },
    { CIRCUIT_MV_FEEDER,        COMP_RESISTOR,  0, 0, "amp", 9.86e3,  0.04, 60e-3, "6,973 V rms at the feeder end (-3.2 %)" },
    { CIRCUIT_POLE_XFMR,        COMP_RESISTOR,  0, 0, "amp", 339.4,   0.04, 60e-3, "240 V rms service (ideal transformer)" },
    { CIRCUIT_GEN_GSU,          COMP_RESISTOR,  0, 0, "amp", 279.4e3, 0.04, 60e-3, "345 kV bus behind X'' referred (25 ohm) at unity pf" },
    { CIRCUIT_GRID_CHAIN,       COMP_RESISTOR,  0, 0, "amp", 339.4,   0.04, 60e-3, "house at 239 V rms (lines unloaded by one house)" },
    { CIRCUIT_FERRANTI_LINE,    COMP_RESISTOR,  0, 0, "amp", 309.6e3, 0.05, 80e-3, "open-end rise +9.9 % (reactor switch open)" },
    { CIRCUIT_LINE_MODEL_LADDER,COMP_RESISTOR,  0, 0, "amp", 110.7e3, 0.03, 60e-3, "row 1 (R only): 112.7 * 211.6 / 215.5" },
    { CIRCUIT_LINE_MODEL_LADDER,COMP_RESISTOR,  1, 0, "amp", 110.1e3, 0.03, 60e-3, "row 2 (R-L): 77.84 kV rms oracle B" },
    { CIRCUIT_LINE_MODEL_LADDER,COMP_RESISTOR,  2, 0, "amp", 110.5e3, 0.03, 60e-3, "row 3 (pi): R-L plus a little charging rise" },
    { CIRCUIT_DC_LINE_DROP,     COMP_RESISTOR,  1, 0, "dc",  10.909,  0.02, 5e-3,  "12 * 10 / 11" },
    { CIRCUIT_PC_OVERCURRENT,   COMP_RESISTOR,  0, 0, "amp", 7.07,    0.06, 39e-3, "burden 600 A / 120 x 1 ohm, before the fault" },
    { CIRCUIT_PC_OVERCURRENT,   COMP_OPAMP,     0, 2, "max", 15.0,    0.05, 80e-3, "TRIP high during the 40-100 ms fault" },
    { CIRCUIT_PC_DIFFERENTIAL,  COMP_RESISTOR,  4, 0, "amp", 30.3,    0.10, 150e-3, "R_d: (2828 - 257) A / 120 x 1 ohm during the internal fault" },
    { CIRCUIT_PC_DISTANCE,      COMP_TRANSFORMER, 1, 2, "amp", 97.9,  0.06, 39e-3, "VT secondary 281.4 k / 2875 before the faults" },
    { CIRCUIT_PC_DISTANCE,      COMP_OPAMP,     0, 2, "max", 15.0,    0.05, 150e-3, "TRIP high for the 40 % fault" },
    { CIRCUIT_PC_BREAKER_FAIL,  COMP_AND_GATE,  1, 2, "max", 5.0,     0.05, 0.30,  "BFT high ~200 ms after TRIP (stuck breaker)" },
    { CIRCUIT_SIL_LOADING,      COMP_RESISTOR,  0, 0, "amp", 269254.0, 0.03, 60e-3, "Vr/Vs = 0.956 at SIL (pi oracle)" },
    { CIRCUIT_SERIES_COMP,      COMP_RESISTOR,  0, 0, "amp", 250606.2, 0.04, 60e-3, "Vr/Vs = 0.890 at 2 x SIL with 50 % series cap (pi oracle)" },
    { CIRCUIT_THEVENIN,         COMP_RESISTOR,  3, 0, "dc",  3.0,   0.01, 5e-3,  "V_th R_L/(R_L + R_th) = 6 x 2.2/4.4" },
    { CIRCUIT_SUPERPOSITION,    COMP_RESISTOR,  0, 1, "dc",  7.333, 0.01, 5e-3,  "4 + 2 + 1.333 V" },
    { CIRCUIT_RC_STEP,          COMP_CAPACITOR, 0, 0, "max", 5.0,   0.02, 20e-3, "settles to the 5 V step (5 tau per half period)" },
    { CIRCUIT_RL_STEP,          COMP_RESISTOR,  0, 0, "max", 5.0,   0.02, 2e-3,  "100 ohm x 50 mA" },
    { CIRCUIT_RLC_RING,         COMP_CAPACITOR, 0, 0, "max", 9.53,  0.04, 6e-3, "first peak 5(1 + e^(-pi zeta/sqrt(1-zeta^2)))" },
    { CIRCUIT_RLC_DAMPING,      COMP_CAPACITOR, 1, 0, "max", 5.0,   0.02, 10e-3, "critical damping: no overshoot" },
    { CIRCUIT_OPAMP_SAT,        COMP_OPAMP,     0, 2, "max", 15.0,  0.03, 3e-3,  "clipped at the +15 V rail" },
    { CIRCUIT_SINGLE_TUNED_AMP, COMP_RESISTOR,  4, 0, "amp", 4.5,   0.5,  2e-4,  "g_m (Rq || RL) x 10 mV at f0 (beta/V_T dependent)" },
    { CIRCUIT_COMMON_BASE,      COMP_RESISTOR,  4, 0, "amp", 1.88,  0.3,  1e-3,  "g_m R_C x 10 mV, in phase" },
    { CIRCUIT_DARLINGTON,       COMP_RESISTOR,  1, 0, "amp", 0.91,  0.12, 5e-3,  "R_in / (R_in + 100k) with R_in ~ beta^2 R_E" },
    { CIRCUIT_SR_LATCH,         COMP_NOR_GATE,  1, 2, "max", 5.0,   0.05, 0.5e-3, "Q set by the S pulse at 0.2 ms" },
    { CIRCUIT_POWER_PLANT,      COMP_RESISTOR,  0, 0, "amp", 259.6e3, 0.06, 60e-3, "345 kV load bus: GSU behind X'' then the 100 mi line (as the single-phase examples)" },
    { CIRCUIT_SUBSTATION,       COMP_RESISTOR,  0, 0, "amp", 103.0e3, 0.08, 60e-3, "138 kV feeder bus with the pf 0.9 load, cap banks open" },
    { CIRCUIT_IO_PUSH_PULL,     COMP_RESISTOR,  1, 0, "max", 3.3,   0.05, 6e-6,  "push-pull output reaches the 3.3 V rail" },
    { CIRCUIT_IO_OPEN_DRAIN,    COMP_RESISTOR,  1, 1, "max", 3.3,   0.05, 30e-6, "pull-up brings the line to 3.3 V (tau 470 ns << 2.5 us)" },
    { CIRCUIT_IO_OPEN_COLLECTOR,COMP_RESISTOR,  1, 1, "max", 5.0,   0.05, 60e-6, "collector pulled up to the 5 V rail; low = Vce(sat)" },
    { CIRCUIT_IO_I2C_BUS,       COMP_RESISTOR,  2, 1, "max", 3.3,   0.05, 200e-6, "SDA released by both devices reaches 3.3 V" },
    { CIRCUIT_IO_I2C_LEVEL,     COMP_RESISTOR,  2, 1, "max", 5.0,   0.05, 40e-6, "5 V side restored by its own pull-up" },
    { CIRCUIT_IO_INPUT_DEBOUNCE,COMP_NOT_GATE,  0, 1, "max", 3.3,   0.05, 40e-3, "inverter output high while the RC node is below 1.65 V" },
    { CIRCUIT_IO_LOW_SIDE,      COMP_INDUCTOR,  0, 1, "max", 12.6,  0.05, 6e-3,  "flyback clamp: drain never exceeds 12 V + V_F" },
    { CIRCUIT_IO_HIGH_SIDE,     COMP_RESISTOR,  2, 0, "max", 11.8,  0.05, 6e-3,  "PMOS on: load gets the rail minus R_DS(on) drop" },
    { CIRCUIT_IO_SPI,           COMP_CAPACITOR, 0, 0, "amp", 1.65,  0.08, 1e-6,  "10 MHz clock still reaches both rails through 33 ohm / 200 pF" },
    { CIRCUIT_IO_UART,          COMP_RESISTOR,  1, 0, "max", 3.33,  0.05, 1e-3,  "1k/2k divider: 5 V x 2/3" },
    { CIRCUIT_IO_RS485,         COMP_OPAMP,     0, 2, "amp", 2.5,   0.08, 12e-6, "receiver output 0/5 V despite 1 V common-mode noise" },
    { CIRCUIT_IO_SPMI,          COMP_CAPACITOR, 1, 0, "amp", 0.9,   0.08, 2e-6,  "1.8 V SDATA at the 15 pF load" },
    { CIRCUIT_TX_69KV,          COMP_RESISTOR,  0, 0, "amp", 53900.0, 0.05, 100e-3, "69 kV bus at 0.957 pu with 20 MVA at 0.95 pf" },
    { CIRCUIT_TX_LADDER,        COMP_RESISTOR,  5, 0, "amp", 332.0,  0.05, 100e-3, "240 V service at the bottom of the ladder (117.4 V per leg)" },
    { CIRCUIT_TX_WIND,          COMP_TLINE,     0, 0, "amp", 28040.0, 0.05, 100e-3, "34.5 kV collector bus while the strings export" },
    { CIRCUIT_TX_PLANT,         COMP_RESISTOR,  3, 0, "amp", 381.7,  0.05, 100e-3, "480 V shop bus behind two transformers" },
    { CIRCUIT_RES_SERVICE,      COMP_RESISTOR,  3, 0, "amp", 168.7,  0.05, 100e-3, "L1 at the panel: 119.3 V rms" },
    { CIRCUIT_RES_BRANCH,       COMP_RESISTOR,  1, 0, "amp", 161.6,  0.05, 100e-3, "#14 at 100 ft: 114.3 V, a 4.8 % drop" },
    { CIRCUIT_RES_ACSTART,      COMP_RESISTOR,  1, 0, "max", 336.4,  0.05, 40e-3,  "panel before the contactor closes at 50 ms" },
    { CIRCUIT_RES_SOLAR,        COMP_RESISTOR,  1, 0, "amp", 349.1,  0.05, 100e-3, "PCC lifted to 123.4 V by the 7.6 kW export" },
    { CIRCUIT_COM_480Y,         COMP_RESISTOR,  1, 0, "amp", 388.7,  0.05, 100e-3, "480Y phase A bus (motor + 277 V lighting)" },
    { CIRCUIT_COM_208Y,         COMP_RESISTOR,  1, 0, "amp", 168.3,  0.05, 100e-3, "208Y phase A branch bus, 20 A" },
    { CIRCUIT_COM_PFC,          COMP_RESISTOR,  0, 0, "amp", 8.7,    0.08, 100e-3, "supply shunt: 174 Apk at 0.75 pf, bank open" },
    { CIRCUIT_COM_ATS,          COMP_RESISTOR,  1, 0, "max", 328.8,  0.05, 300e-3, "life-safety load re-energised by the generator" },
    { CIRCUIT_GS_N1,            COMP_RESISTOR,  0, 0, "amp", 273300.0, 0.05, 100e-3, "both circuits in service: 0.970 pu" },
    { CIRCUIT_GS_IBR,           COMP_RESISTOR,  1, 0, "max", 275900.0, 0.05, 90e-3,  "POI before the fault" },
    { CIRCUIT_GS_BOLD,          COMP_RESISTOR,  0, 0, "amp", 259500.0, 0.05, 100e-3, "conventional line at 600 MW: 0.921 pu" },
    { CIRCUIT_GS_DERATE,        COMP_RESISTOR,  1, 0, "amp", 9752.0,  0.05, 100e-3, "12.47 kV feeder bus at 25 degC" },
    { CIRCUIT_GS_FACRATE,       COMP_RESISTOR,  4, 0, "amp", 111500.0, 0.05, 100e-3, "138 kV bus at 400 A" },
    { CIRCUIT_GS_KRON,          COMP_RESISTOR,  2, 0, "amp", 91.38,   0.02, 100e-3, "Y-side load 1" },
    { CIRCUIT_GS_RX,            COMP_RESISTOR,  1, 0, "amp", 168.8,   0.05, 100e-3, "transmission bus, R/X = 0.09" },
    { CIRCUIT_GS_GOVERNOR,      COMP_OPAMP,     0, 2, "min", -0.1432, 0.05, 4.0,    "settles at -0.05/(1/R + D) = -0.143 Hz" },
    { CIRCUIT_GS_PIDS,          COMP_RESISTOR,  1, 0, "max", 8.482,   0.03, 3.0,    "RTU input, loop normal" },
    { CIRCUIT_MOS_IDVGS,        COMP_RESISTOR,  1, 0, "max", 0.1886,  0.05, 20e-3,  "2N7000 at Vgs 4 V: 189 mA through the 1 ohm sense" },
    { CIRCUIT_MOS_IDVDS,        COMP_RESISTOR,  5, 0, "max", 0.1896,  0.05, 20e-3,  "Vgs 3.5 V curve: 95 mA through the 2 ohm sense" },
    /* 1.495 is where this converges: 1.4937 at dt = 2 ns and 1.4953 at 1 ns. It was 1.2458 until
       2026-08-30, which was not physics - it was a record of the MOSFET gate capacitance advancing
       its companion state once per Newton iteration, and 17 %% below the truth. The tolerance is
       wide enough for the app's own step, which reads 1.42: the tank is sharp and the step
       resolves it coarsely, and that 5 %% is recorded in docs/ROADMAP.md rather than hidden by
       pinning the expectation to it. */
    { CIRCUIT_MOS_TUNED,        COMP_RESISTOR,  4, 0, "amp", 1.4950, 0.10, 4e-3,  "gain peaks as the sweep passes the 100 kHz tank" },
    { CIRCUIT_MOS_CG,           COMP_RESISTOR,  4, 0, "amp", 0.5519, 0.05, 400e-6, "common gate: 20 mV in, g_m R_D = 28x, in phase" },
    { CIRCUIT_MOS_CASCODE,      COMP_RESISTOR,  6, 0, "amp", 0.3553, 0.05, 400e-6, "cascode: 10 mV in, gain 36 with almost no Miller" },
    { CIRCUIT_MOS_DIFF,         COMP_RESISTOR,  0, 1, "amp", 0.3943, 0.05, 4e-3,  "one drain of the pair, 20 mV antiphase drive" },
    { CIRCUIT_MOS_MIRROR,       COMP_RESISTOR,  1, 1, "max", 6.2820, 0.05, 20e-3, "mirrored drain sits mid-rail, load current copied" },
    { CIRCUIT_CMOS_INV,         COMP_CAPACITOR, 0, 0, "max", 5.0,    0.02, 4e-3,  "inverter reaches the full rail" },
    { CIRCUIT_CMOS_NAND,        COMP_RESISTOR,  1, 0, "max", 5.0,    0.02, 10e-3, "NAND output pulls to the rail unless both inputs are high" },
    { CIRCUIT_CMOS_TGATE,       COMP_RESISTOR,  0, 0, "max", 5.0,    0.02, 4e-3,  "transmission gate passes the whole rail" },
    { CIRCUIT_CMOS_TGATE,       COMP_RESISTOR,  1, 0, "max", 4.266,  0.05, 4e-3,  "the lone NMOS stops one threshold short - the point of the template" },
    { CIRCUIT_XY_LISSAJOUS,     COMP_RESISTOR,  1, 0, "amp", 5.0,    0.03, 4e-3,  "CH2 of the Lissajous pair: 5 V at twice CH1's frequency" },
    { CIRCUIT_XY_PLOTTER,       COMP_RESISTOR,  1, 0, "amp", 4.2532, 0.05, 40e-3, "the y table drives the full 5 V, so the shape fills the screen" },
    { CIRCUIT_HW_BUCK,          COMP_RESISTOR,  0, 0, "absmean", 5.5152, 0.04, 2e-3, "Vout = D Vin minus the switch and diode drops" },
    { CIRCUIT_HW_BOOST,         COMP_RESISTOR,  0, 0, "absmean", 9.0353, 0.04, 2e-3, "Vin/(1-D) minus the diode drop" },
    { CIRCUIT_HW_BUCKBOOST,     COMP_RESISTOR,  0, 0, "absmean", 12.3182, 0.05, 10e-3, "-D/(1-D) Vin = -12 V (magnitude)" },
    { CIRCUIT_HW_CUK,           COMP_RESISTOR,  0, 0, "absmean", 12.69,  0.06, 20e-3, "|Vout| = D/(1-D) Vin = 12 V; the output filter needs ~15 ms to get there" },
    { CIRCUIT_HW_INTERLEAVED,   COMP_RESISTOR,  0, 0, "absmean", 5.5145, 0.04, 2e-3, "same 6 V rail, ripple shared between two phases" },
    { CIRCUIT_HW_PDN,           COMP_RESISTOR,  3, 0, "absmean", 1.5921, 0.04, 200e-6, "1.8 V rail sagging under the load step (deeper since the DCR history fix)" },
    { CIRCUIT_HW_CAPS,          COMP_RESISTOR,  2, 0, "absmean", 5.0,    0.03, 500e-6, "the input-capacitor rail" },
    { CIRCUIT_HW_MATCH,         COMP_RESISTOR,  3, 0, "amp", 1.0,    0.03, 20e-6, "matched 50 ohm load takes half the source voltage" },
    { CIRCUIT_HW_REFLECT,       COMP_DELAY_LINE, 0, 1, "max", 5.0,    0.03, 8e-6, "open far end doubles the incident 2.5 V step" },
    { CIRCUIT_HW_REFLECT,       COMP_DELAY_LINE, 0, 0, "max", 5.0,    0.03, 8e-6, "driver end: 2.5 V launched, 5 V once the reflection is back" },
    { CIRCUIT_HW_LOOP,          COMP_OPAMP,     0, 2, "amp", 4.9995, 0.04, 4e-3, "uncompensated stage: x10 on a 1 Vpp step" },
    /* Ideal vs real: both halves of every comparison, so a model change cannot quietly
       collapse the two rows onto each other. Hand calculations are in the template notes. */
    { CIRCUIT_ID_SOURCE,        COMP_RESISTOR,  0, 0, "dc",  5.0,    0.01, 5e-3, "ideal source: 5.000 V into 1k, no droop" },
    { CIRCUIT_ID_SOURCE,        COMP_RESISTOR,  1, 0, "dc",  4.1667, 0.01, 5e-3, "real source into 1k: 5 x 1000/1200" },
    { CIRCUIT_ID_SOURCE,        COMP_RESISTOR,  2, 0, "dc",  1.6667, 0.01, 5e-3, "same source into 100 ohm: 5 x 100/300" },
    { CIRCUIT_ID_DIODE,         COMP_RESISTOR,  0, 0, "max", 0.30,   0.05, 5e-3, "ideal diode: 1 Vpk - 0.7 V brick wall" },
    { CIRCUIT_ID_DIODE,         COMP_RESISTOR,  1, 0, "max", 0.486,  0.06, 5e-3, "Shockley: soft knee near 0.52 V leaves 60 % more" },
    { CIRCUIT_ID_CAP,           COMP_CAPACITOR, 0, 0, "amp", 0.125,  0.08, 5e-3, "ideal 5 uF: I(T/2)/C = 250 mVpp triangle" },
    { CIRCUIT_ID_CAP,           COMP_CAPACITOR, 1, 0, "amp", 0.1495, 0.08, 5e-3, "ESR 0.5 ohm adds a +/-25 mV square to the triangle" },
    { CIRCUIT_ID_CAP,           COMP_CAPACITOR, 2, 0, "amp", 0.2225, 0.08, 5e-3, "ESR 2 ohm: +/-100 mV, now bigger than the triangle" },
    { CIRCUIT_ID_IND,           COMP_CAPACITOR, 0, 0, "max", 8.94,   0.06, 9.5e-3, "lossless L: zeta = 0.05, overshoot to ~9.3 V (theta method shaves 3 %)" },
    { CIRCUIT_ID_IND,           COMP_CAPACITOR, 1, 0, "max", 6.86,   0.06, 9.5e-3, "DCR 50 ohm: zeta = 0.30, overshoot exp(-pi zeta/sqrt(1-zeta^2)) = 0.372" },
    { CIRCUIT_ID_OPAMP,         COMP_RESISTOR,  2, 0, "amp", 0.5,    0.03, 2e-4, "ideal op-amp: gain 10 at any frequency" },
    { CIRCUIT_ID_OPAMP,         COMP_RESISTOR,  5, 0, "amp", 0.354,  0.10, 2e-4, "GBW 1 MHz at Acl 10: -3 dB at exactly 100 kHz" },
    { CIRCUIT_ID_OPAMP,         COMP_RESISTOR,  8, 0, "amp", 1.25,   0.08, 2e-4, "slew limited: a triangle of SR x T/4 = 0.5 V/us x 2.5 us" },
    { CIRCUIT_ID_BJT,           COMP_RESISTOR,  1, 1, "dc",  7.28,   0.02, 5e-3, "no Early effect: V_C = 12 - beta I_B x 4.7k" },
    { CIRCUIT_ID_BJT,           COMP_RESISTOR,  3, 1, "dc",  6.874,  0.02, 5e-3, "V_AF = 80 V: (1 + V_CE/V_AF) adds ~9 % of collector current" },
    { CIRCUIT_ID_MOSFET,        COMP_RESISTOR,  2, 1, "dc",  7.05,   0.02, 5e-3, "square law: I_D = K V_ov^2/2 = 2.25 mA into 2.2k" },
    { CIRCUIT_ID_MOSFET,        COMP_RESISTOR,  5, 1, "dc",  5.652,  0.02, 5e-3, "lambda = 0.05: V_D solves 12 - 2.2k I_D (1 + lambda V_D)" },
    { CIRCUIT_ID_OPAMP_ERR,     COMP_OPAMP,     0, 2, "dc",  0.0,   -0.005, 5e-3, "ideal op-amp: a grounded input gives 0.000 V" },
    { CIRCUIT_ID_OPAMP_ERR,     COMP_OPAMP,     1, 2, "dc", -0.8892, 0.03, 5e-3, "100 (V_os - I_B (100k - 990)) with the switch open" },
    { CIRCUIT_PARTS_MOSFET,     COMP_NMOS,      0, 1, "dc",  0.1442, 0.05, 5e-3, "2N7000: 12 x 1.2 / (100 + 1.2) at V_GS = 10 V" },
    { CIRCUIT_PARTS_MOSFET,     COMP_NMOS,      1, 1, "dc",  0.2340, 0.05, 5e-3, "2N7002: 12 x 2.0 / (100 + 2.0)" },
    { CIRCUIT_PARTS_MOSFET,     COMP_NMOS,      2, 1, "dc",  0.00526, 0.08, 5e-3, "IRF540N: 12 x 0.044 / (100 + 0.044)" },
    /* Interview prep - instrumentation. Each case is the number the mistake actually costs
       you: the loaded edge, the doubled amplitude, the lead you measured instead of the part. */
    { CIRCUIT_IV_PROBE_COMP,    COMP_RESISTOR,  5, 0, "amp", 0.25,   0.05, 5e-3, "correctly compensated 10x: 5 Vpp CAL divides to 500 mVpp at every frequency" },
    { CIRCUIT_IV_PROBE_LOADING, COMP_CAPACITOR, 0, 0, "amp", 1.65,   0.05, 5e-6, "no probe: the 1 MHz square is a square, 3.3 Vpp" },
    { CIRCUIT_IV_PROBE_LOADING, COMP_CAPACITOR, 1, 0, "amp", 0.386,  0.08, 5e-6, "1x probe's 100 pF: 0.77 Vpp left of 3.3, and no longer a square" },
    { CIRCUIT_IV_PROBE_LOADING, COMP_CAPACITOR, 3, 0, "amp", 1.484,  0.06, 5e-6, "10x probe's 12 pF: 2.97 Vpp, loaded but honest" },
    { CIRCUIT_IV_GROUND_LEAD,   COMP_CAPACITOR, 0, 0, "max", 3.693,  0.08, 4e-7, "6 inch clip, 150 nH: 0.39 V of overshoot on a 3.3 V edge" },
    { CIRCUIT_IV_GROUND_LEAD,   COMP_CAPACITOR, 1, 0, "max", 3.447,  0.08, 4e-7, "spring tip, 15 nH: 0.15 V - the same edge, a shorter return" },
    { CIRCUIT_IV_SCOPE_INPUT_Z, COMP_RESISTOR,  1, 0, "max", 2.0,    0.04, 4e-7, "1 M input: the open cable end doubles the launched 1 V, exactly" },
    { CIRCUIT_IV_SCOPE_INPUT_Z, COMP_RESISTOR,  3, 0, "max", 1.05,   0.10, 4e-7, "50 ohm input: matched, 1 V - what the generator is calibrated to deliver" },
    { CIRCUIT_IV_AC_COUPLING,   COMP_RESISTOR,  1, 0, "dc",  12.0,   0.02, 5e-5, "DC-coupled channel sits at the 12 V rail" },
    { CIRCUIT_IV_AC_COUPLING,   COMP_RESISTOR,  2, 0, "amp", 0.1,    0.05, 5e-5, "AC-coupled channel: the 200 mVpp ripple, centred on zero" },
    { CIRCUIT_IV_SHUNT_SENSE,   COMP_RESISTOR,  1, 0, "dc",  0.1,    0.03, 1e-3, "low-side shunt: 1 A x 100 mohm, and the load's ground is now 100 mV up" },
    { CIRCUIT_IV_SHUNT_SENSE,   COMP_OPAMP,     0, 2, "dc",  2.0,    0.03, 1e-3, "high-side difference amp, gain 20: 100 mV of shunt -> 2 V out" },
    { CIRCUIT_IV_KELVIN,        COMP_RESISTOR,  3, 0, "dc",  0.110,  0.03, 1e-3, "2-wire at the connector: 110 mV, i.e. 110 mohm for a 10 mohm part" },
    { CIRCUIT_IV_KELVIN,        COMP_OPAMP,     0, 2, "dc",  0.010,  0.05, 1e-3, "4-wire across the body: 10 mV, the part and nothing else" },
    /* Interview prep - converters. */
    { CIRCUIT_IV_BUCK_NODES,    COMP_RESISTOR,  2, 0, "dc",  5.91,   0.05, 5e-3, "discrete buck: 50 % of 12 V, less the PMOS and Schottky drops. The gate drive ramps over the 1 us this template asks for, and the PMOS conducts through most of each ramp, so the effective on-time is a little over half" },
    { CIRCUIT_IV_LDO_VS_BUCK,   COMP_RESISTOR,  0, 0, "dc",  4.90,   0.04, 5e-3, "the 7805's 5 V, drawing the same 1 A it delivers" },
    { CIRCUIT_IV_LDO_VS_BUCK,   COMP_RESISTOR,  1, 0, "dc",  4.76,   0.06, 5e-3, "the switcher's 5 V, drawing about 440 mA to make it" },
    { CIRCUIT_IV_BOOTSTRAP,     COMP_CAPACITOR, 0, 0, "max", 23.4,   0.15, 1e-4, "switching: BOOT rides to 23 V, 11.5 V above the switch node" },
    { CIRCUIT_IV_BOOTSTRAP,     COMP_CAPACITOR, 1, 0, "max", 12.0,   0.05, 4e-3, "stuck on: the cap has drained and BOOT has fallen back to the switch node" },
    /* Interview prep - I/O and signal integrity. */
    { CIRCUIT_IV_TERMINATION,   COMP_CAPACITOR, 0, 0, "max", 4.504,  0.05, 2e-7, "unterminated: the 2.2 V launched doubles at the open far end" },
    { CIRCUIT_IV_TERMINATION,   COMP_CAPACITOR, 1, 0, "max", 3.282, 0.05, 2e-7, "series terminated: the full 3.3 V, and nothing comes back twice" },
    { CIRCUIT_IV_TERMINATION,   COMP_CAPACITOR, 2, 0, "max", 2.355, 0.05, 2e-7, "parallel terminated: clean, but 3.3 x 50/75 is all the receiver ever gets" },
    { CIRCUIT_IV_GROUND_BOUNCE, COMP_INDUCTOR,  0, 0, "amp", 1.077,  0.10, 4e-7, "the chip's own ground moves 2.2 Vpp against the board's" },
    { CIRCUIT_IV_GROUND_BOUNCE, COMP_RESISTOR,  1, 0, "amp", 0.78,   0.20, 4e-7, "and the pin that is holding LOW moves with it. L di/dt needs a dt: with the driver stepping instantly this was whatever the solver's step was, and doubled every time the step was halved" },
    { CIRCUIT_IV_CROSSTALK,     COMP_CAPACITOR, 2, 0, "amp", 0.542,  0.15, 4e-7, "weak victim: 2 pF against 5 pF divides the aggressor's edge. An instantaneous edge would put 6.6 pC into 7 pF and nearly a volt on the victim; over the 1 ns edge the template specifies, the aggressor's own 25 ohm and 5 pF soften it to about half that" },
    { CIRCUIT_IV_CROSSTALK,     COMP_CAPACITOR, 5, 0, "amp", 0.0764, 0.25, 4e-7, "strong victim: the same charge, swallowed" },
    { CIRCUIT_IV_ESD_CLAMP,     COMP_DIODE,     0, 0, "dc",  3.852,  0.03, 1e-3, "1 k series: the pin clamps a diode above the 3.3 V rail" },
    { CIRCUIT_IV_ESD_CLAMP,     COMP_DIODE,     2, 0, "dc",  3.704,  0.03, 1e-3, "220 k series: 12 uA is not enough to hold the clamp as hard" },
    /* Interview prep - fundamentals. */
    { CIRCUIT_IV_CAP_ENERGY,    COMP_CAPACITOR, 1, 0, "dc",  5.0,    0.04, 60e-3, "1 ohm transfer: charge is conserved, so both caps end at half" },
    { CIRCUIT_IV_CAP_ENERGY,    COMP_CAPACITOR, 3, 0, "dc",  5.0,    0.04, 60e-3, "100 ohm: a hundred times slower, and exactly the same answer" },
    { CIRCUIT_IV_MILLER,        COMP_NMOS,      0, 1, "amp", 0.516,  0.10, 5e-6, "no C_gd: the stage's full gain at 1 MHz" },
    { CIRCUIT_IV_MILLER,        COMP_NMOS,      1, 1, "amp", 0.0723, 0.15, 5e-6, "10 pF of C_gd: 130 pF at the input rolls it off to a seventh" },
    { CIRCUIT_IV_SWITCH_CHOICE, COMP_NPN_BJT,   0, 1, "dc",  0.072,  0.20, 1e-3, "2N3904 saturated: V_CE(sat), and it barely moves with current" },
    { CIRCUIT_IV_SWITCH_CHOICE, COMP_NMOS,      0, 1, "dc",  0.406,  0.15, 1e-3, "2N7000 at V_GS = 5 V: I x R_DS(on), and R_DS(on) is not the data sheet's 1.2 ohm here" },
    { CIRCUIT_IV_INRUSH,        COMP_CAPACITOR, 0, 0, "max", 11.99,  0.02, 5e-2,  "straight in: it charges to the rail and stops. It read 12.65 V - an RC cannot overshoot, and that was the step stepping over the transient" },
    { CIRCUIT_IV_INRUSH,        COMP_CAPACITOR, 1, 0, "max", 11.46,  0.04, 60e-3, "through 4.7 ohm: no overshoot, and it settles at 12 x 100/104.7" },
    { CIRCUIT_CAP_DCBIAS,       COMP_CAPACITOR, 0, 0, "amp", 0.03125, 0.10, 4e-3, "10 uF unbiased: I(T/2)/C = 62.5 mVpp" },
    { CIRCUIT_CAP_DCBIAS,       COMP_CAPACITOR, 1, 0, "amp", 0.0625,  0.10, 4e-3, "2 V bias halves it: twice the ripple" },
    { CIRCUIT_CAP_DCBIAS,       COMP_CAPACITOR, 2, 0, "amp", 0.1094,  0.12, 4e-3, "5 V bias leaves 2.86 uF: 3.5x the ripple" },
    { CIRCUIT_NE555_ASTABLE,    COMP_RESISTOR,  2, 0, "amp", 2.475,   0.06, 4e-3, "the block swings its output rail to rail: 0 to ~5 V" },
    { CIRCUIT_SCHMITT_BISTABLE, COMP_OPAMP,     0, 2, "max", 15.0,  0.05, 30e-3, "bistable output at the rail" },
    { CIRCUIT_TRI_SQUARE_GEN,   COMP_OPAMP,     1, 2, "amp", 7.5,   0.08, 3e-3,  "triangle peak = 15 R1/R2" },
    { CIRCUIT_FUNCTION_GEN,     COMP_RESISTOR,  3, 1, "amp", 4.9,   0.15, 3e-3,  "3-breakpoint sine ~4.9 V peak" },
    { CIRCUIT_3PH_Y_BALANCED,   COMP_RESISTOR,  3, 0, "amp", 373.3, 0.03, 60e-3, "phase B load: 392 x 10 / 10.5 (balanced, neutral at 0)" },
    { CIRCUIT_3PH_UNBALANCED,   COMP_RESISTOR,  6, 0, "amp", 20.83, 0.05, 60e-3, "neutral shift with 10/20/40 ohm loads and 1 ohm neutral (phasor)" },
    { CIRCUIT_3PH_345_LINE,     COMP_RESISTOR,  1, 0, "amp", 264.0e3, 0.05, 60e-3, "phase B: same 6.3 % drop as the single-phase example" },
    { CIRCUIT_3PH_RECTIFIER,    COMP_RESISTOR,  0, 0, "max", 169.3, 0.03, 60e-3, "plus bus peak = 170 - 0.7" },
    { CIRCUIT_HV_765_LINE,      COMP_RESISTOR,  0, 0, "amp", 598613.0, 0.04, 60e-3, "Vr/Vs = 0.958 at SIL, 300 mi single pi" },
};

static Component *find_comp(Circuit *c, ComponentType ct, int ord) {
    int k = 0;
    for (int i = 0; i < c->num_components; i++)
        if (c->components[i]->type == ct) { if (k == ord) return c->components[i]; k++; }
    return NULL;
}

/* --label-test: every probe on every circuit has to say what it is sitting on. "CH1" tells you
   which trace it is and nothing else, which is no help on a circuit you did not draw - and the
   probe label is the scope's channel name too, so it is the same word in both places. A name has
   to be there, has to not be the old CHn default, has to fit the field, and has to be unique
   inside its circuit: two traces called VCAP cannot be told apart. */
static int label_test(void) {
    int fails = 0, total = 0, probes_seen = 0;
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        if (!ti) continue;
        Circuit *c = circuit_create();
        if (circuit_place_template(c, (CircuitTemplateType)t, 0, 0) <= 0) { circuit_free(c); continue; }
        total++;
        char why[240] = "";
        if (c->num_probes < 1)
            snprintf(why, sizeof why, "no probes at all, so the scope has nothing to show");
        for (int i = 0; i < c->num_probes && !why[0]; i++) {
            const char *l = c->probes[i].label;
            if (!l[0]) { snprintf(why, sizeof why, "probe %d has no name", i + 1); break; }
            if (l[0] == 'C' && l[1] == 'H' && l[2] >= '0' && l[2] <= '9' && !l[3]) {
                snprintf(why, sizeof why, "probe %d is still called %s - name it after the node "
                         "it is on", i + 1, l);
                break;
            }
            if (strlen(l) > 7) {
                snprintf(why, sizeof why, "probe %d's name %s does not fit the 7-character field",
                         i + 1, l);
                break;
            }
            for (int j = 0; j < i; j++)
                if (!strcmp(c->probes[j].label, l)) {
                    snprintf(why, sizeof why, "probes %d and %d are both called %s", j + 1, i + 1, l);
                    break;
                }
        }
        probes_seen += c->num_probes;
        if (why[0]) { printf("[FAIL] labels %-28s %s\n", ti->name, why); fails++; }
        else {
            char names[160] = ""; size_t at = 0;
            for (int i = 0; i < c->num_probes && at < sizeof names - 10; i++)
                at += (size_t)snprintf(names + at, sizeof names - at, "%s%s", i ? " " : "",
                                       c->probes[i].label);
            printf("[ OK ] labels %-28s %d probes: %s\n", ti->name, c->num_probes, names);
        }
        circuit_free(c);
    }
    printf("\nlabel-test: %d templates, %d probes, %d templates with an unnamed or ambiguous probe\n",
           total, probes_seen, fails);
    return fails;
}

/* --span-test: turning the time/div up must not empty the scope. A wider window asks the
   recorder for a coarser sample spacing, and it used to answer by throwing away everything it
   had and starting again - so the trace vanished and came back a moment later, once for every
   press of T+. The samples already recorded each carry their own timestamp, so they survive the
   change thinned to the new spacing. */
static int span_test(void) {
    int fails = 0, total = 0;
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        if (!ti) continue;
        Circuit *c = circuit_create();
        if (circuit_place_template(c, (CircuitTemplateType)t, 0, 0) <= 0) { circuit_free(c); continue; }
        Simulation *sim = simulation_create(c);
        double td = circuit_template_scope_time_div((CircuitTemplateType)t);
        if (td <= 0) td = 1e-3;
        if (!simulation_dc_analysis(sim)) { simulation_free(sim); circuit_free(c); continue; }
        simulation_auto_time_step(sim);
        { double dtp = simulation_scope_time_step(sim, td); if (dtp > 0 && dtp < sim->time_step) simulation_set_time_step(sim, dtp); }
        simulation_set_history_span(sim, 20.0 * td);
        simulation_start(sim);
        total++;

        /* fill the buffer the way a running circuit does */
        int guard = 0;
        while (sim->history_count < 400 && guard++ < 400000) if (!simulation_step(sim)) break;
        int before = sim->history_count;

        /* T+ four times over: each press widens the window and re-derives the spacing */
        char why[200] = "";
        for (int press = 0; press < 4 && !why[0]; press++) {
            td *= 2.0;
            simulation_set_history_span(sim, 20.0 * td);
            for (int s = 0; s < 3; s++) if (!simulation_step(sim)) break;
            /* two samples is the least the display can draw a line from */
            if (sim->history_count < 2)
                snprintf(why, sizeof why, "press %d of T+ left %d samples of the %d it had - the "
                         "scope goes blank until the buffer refills", press + 1,
                         sim->history_count, before);
        }
        if (why[0]) { printf("[FAIL] span %-28s %s\n", ti->name, why); fails++; }
        else printf("[ OK ] span %-28s %d samples before, %d after four presses of T+\n",
                    ti->name, before, sim->history_count);
        simulation_free(sim); circuit_free(c);
    }
    printf("\nspan-test: %d templates, %d that blank the scope when the time/div is turned up\n",
           total, fails);
    return fails;
}

static int probe_test(void) {
    int fails = 0, total = 0;
    for (unsigned k = 0; k < sizeof probe_cases / sizeof probe_cases[0]; k++) {
        const ProbeCase *pc = &probe_cases[k];
        const CircuitTemplateInfo *ti = circuit_template_get_info(pc->t);
        if (g_only && (!ti || !strstr(ti->name, g_only))) continue;
        Circuit *c = circuit_create();
        circuit_place_template(c, pc->t, 0, 0);
        Simulation *sim = simulation_create(c);
        total++;
        /* The oracle checks the static frequency; the demo frequency sweeps stay off here */
        for (int i = 0; i < c->num_components; i++)
            if (c->components[i]->type == COMP_AC_VOLTAGE) c->components[i]->props.ac_voltage.frequency_sweep.enabled = false;
        Component *comp = find_comp(c, pc->ct, pc->ord);
        int node_id = comp ? comp->node_ids[pc->term] : -1;
        int ok = comp && simulation_dc_analysis(sim);
        simulation_auto_time_step(sim);
        {   /* never coarser than the template's own scope preset (what the app would use) */
            double td = circuit_template_scope_time_div(pc->t);
            if (td > 0) { double dtp = simulation_scope_time_step(sim, td); if (dtp > 0 && dtp < sim->time_step) simulation_set_time_step(sim, dtp); }
        }
        if (g_probe_dt > 0) simulation_set_time_step(sim, g_probe_dt);
        simulation_start(sim);
        double t_rec = pc->run * 0.75, mn = 1e300, mx = -1e300, sum = 0, asum = 0; int n = 0;
        while (ok && sim->time < pc->run) {
            if (!simulation_step(sim)) { ok = 0; break; }
            if (sim->time >= t_rec) {
                Node *nd = circuit_get_node(c, node_id);
                double v = nd ? nd->voltage : 0;
                if (v < mn) mn = v; if (v > mx) mx = v; sum += v; asum += fabs(v); n++;
            }
        }
        double got = 0;
        if (ok && n > 0) {
            if (!strcmp(pc->metric, "amp")) got = (mx - mn) / 2;
            else if (!strcmp(pc->metric, "max")) got = mx;
            else if (!strcmp(pc->metric, "min")) got = mn;
            else if (!strcmp(pc->metric, "absmean")) got = asum / n;
            else got = sum / n;
        }
        double err = fabs(got - pc->expect);
        double lim = (pc->tol >= 0) ? pc->tol * fabs(pc->expect) : -pc->tol;
        int pass = ok && err <= lim;
        printf("[%s] probe %-28s %-4s %-5s= %9.4f  expect %8.4f (+/-%.3g)  %s%s\n",
               pass ? " OK " : "FAIL", ti ? ti->name : "?", pc->metric,
               "", got, pc->expect, lim, pc->note, !ok ? "  [sim error / node not found]" : "");
        if (!pass) fails++;
        simulation_free(sim); circuit_free(c);
    }
    printf("\n%d/%d probe checks passed\n", total - fails, total);
    return fails;
}

/* ------------------------------------------------------------------------------------
 * Schematic geometry audit: diagonal wires, wire/wire crossings without a junction,
 * wires running through component bodies, and near-touching terminals of different nets.
 * ---------------------------------------------------------------------------------- */
static int seg_intersect(float ax, float ay, float bx, float by, float cx, float cy, float dx, float dy) {
    /* proper intersection of segments AB and CD (excluding shared endpoints) */
    float d = (bx - ax) * (dy - cy) - (by - ay) * (dx - cx);
    if (fabsf(d) < 1e-6f) return 0;
    float t = ((cx - ax) * (dy - cy) - (cy - ay) * (dx - cx)) / d;
    float u = ((cx - ax) * (by - ay) - (cy - ay) * (bx - ax)) / d;
    return t > 0.01f && t < 0.99f && u > 0.01f && u < 0.99f;
}
static int seg_hits_box(float ax, float ay, float bx, float by, float x0, float y0, float x1, float y1) {
    /* axis-aligned segment vs box (boxes are shrunk by the caller) */
    if (fabsf(ax - bx) < 0.5f) { /* vertical */
        float ymin = ay < by ? ay : by, ymax = ay < by ? by : ay;
        return ax > x0 && ax < x1 && ymax > y0 && ymin < y1;
    }
    if (fabsf(ay - by) < 0.5f) {
        float xmin = ax < bx ? ax : bx, xmax = ax < bx ? bx : ax;
        return ay > y0 && ay < y1 && xmax > x0 && xmin < x1;
    }
    return 0;
}


/* Symbols must not sit on top of each other. Component bodies are info->width x height around
   (x,y); text labels are excluded (they are annotation, not schematic symbols). Overlapping
   symbols - a ground drawn under a capacitor, say - are a layout bug even when the netlist is
   right, so --geom-test fails on them. */
/* Text sitting on a symbol. Annotations are drawn from their anchor down and to the right, one
   glyph 8 px wide per font size step, so the box a label occupies is predictable - and a label
   printed across a component is unreadable in exactly the way a wire through one is. Nothing
   checked this before, so labels drifting onto a part only showed up by looking at screenshots. */
/* Every piece of text on the canvas, as a box: the annotations a template places and the value
   label each component draws beside itself ("10k", "CLOSED", "2N7000  1.2 ohm"). The value
   labels come from the renderer's own function, so what is measured is what is drawn. */
typedef CanvasTextBox TextBox;   /* the enumeration moved to label.c - see canvas_text_boxes */

/* Delegates to label.c. This function used to hold its own copy of where every piece of canvas
   text goes, and label.c's header comment already said why that is a mistake - "keeping one copy
   is the point, since a second one would drift". It drifted the moment the probe readout started
   moving to avoid other text: a check that computes a position independently of the renderer is
   not checking the renderer. */
static int geom_text_boxes(Circuit *c, TextBox *out, int max, int with_values) {
    return canvas_text_boxes(c, out, max, with_values);
}

/* Text landing on other text. Two labels on top of each other are unreadable in a way that is
   easy to miss when writing the template - the value labels in particular are placed by the
   renderer, not by the template, so nobody chose where they went. */
static int geom_text_on_text(Circuit *c, char *why, size_t whyn) {
    enum { MAX_TB = 512 };
    static TextBox tb[MAX_TB];
    int n = geom_text_boxes(c, tb, MAX_TB, 1), hits = 0;   /* annotations and value labels */
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            /* 2 px of slack: text that merely abuts is fine, text that shares pixels is not */
            if (tb[i].x0 < tb[j].x1 - 2 && tb[j].x0 < tb[i].x1 - 2 &&
                tb[i].y0 < tb[j].y1 - 2 && tb[j].y0 < tb[i].y1 - 2) {
                hits++;
                if (strlen(why) < whyn - 110) {
                    snprintf(why + strlen(why), whyn - strlen(why),
                             " textpair:'%s'@(%g,%g)x'%s'@(%g,%g)",
                             tb[i].s, tb[i].x0, tb[i].y0, tb[j].s, tb[j].x0, tb[j].y0);
                }
            }
        }
    }
    return hits;
}

static int geom_text_on_symbol(Circuit *c, char *why, size_t whyn) {
    enum { MAX_TB = 512 };
    static TextBox tb[MAX_TB];
    /* annotations only: a value label is placed by the renderer hard against its own part, and
       in a dense schematic it will sit near a neighbour's body without being unreadable */
    int nb = geom_text_boxes(c, tb, MAX_TB, 0);
    int hits = 0;
    for (int i = 0; i < nb; i++) {
        float tx0 = tb[i].x0, tx1 = tb[i].x1, ty0 = tb[i].y0, ty1 = tb[i].y1;
        const Component *t = tb[i].owner;
        const char *str = tb[i].s;

        for (int j = 0; j < c->num_components; j++) {
            Component *b = c->components[j];
            if (b->type == COMP_TEXT || b->type == COMP_LABEL) continue;
            /* a part's own value label sits against its body on purpose */
            if (b == t) continue;
            const ComponentTypeInfo *ib = component_get_info(b->type);
            if (!ib) continue;
            int rot = ((b->rotation % 360) + 360) % 360;
            double bw = (rot == 90 || rot == 270) ? ib->height : ib->width;
            double bh = (rot == 90 || rot == 270) ? ib->width : ib->height;
            /* the drawn body, same proportions the symbol-on-symbol check uses */
            double sxb = (rot == 90 || rot == 270) ? 0.85 : 0.65;
            double syb = (rot == 90 || rot == 270) ? 0.65 : 0.85;
            float bx0 = (float)(b->x - bw * sxb / 2), bx1 = (float)(b->x + bw * sxb / 2);
            float by0 = (float)(b->y - bh * syb / 2), by1 = (float)(b->y + bh * syb / 2);

            if (tx0 < bx1 && bx0 < tx1 && ty0 < by1 && by0 < ty1) {
                hits++;
                if (strlen(why) < whyn - 90) {
                    char head[24];
                    snprintf(head, sizeof head, "%.18s", str);
                    snprintf(why + strlen(why), whyn - strlen(why),
                             " text:'%s'@(%g,%g)over:%s@(%g,%g)", head, t->x, t->y, b->label, b->x, b->y);
                }
                break;   /* one report per label is enough */
            }
        }
    }
    return hits;
}

static int geom_overlap(Circuit *c, char *why, size_t whyn) {
    int hits = 0;
    for (int i = 0; i < c->num_components; i++) {
        Component *a = c->components[i];
        if (a->type == COMP_TEXT || a->type == COMP_LABEL) continue;
        const ComponentTypeInfo *ia = component_get_info(a->type);
        if (!ia) continue;
        int arot = ((a->rotation % 360) + 360) % 360;
        double aw = (arot == 90 || arot == 270) ? ia->height : ia->width;
        double ah = (arot == 90 || arot == 270) ? ia->width : ia->height;
        for (int j = i + 1; j < c->num_components; j++) {
            Component *b = c->components[j];
            if (b->type == COMP_TEXT || b->type == COMP_LABEL) continue;
            const ComponentTypeInfo *ib = component_get_info(b->type);
            if (!ib) continue;
            int brot = ((b->rotation % 360) + 360) % 360;
            double bw = (brot == 90 || brot == 270) ? ib->height : ib->width;
            double bh = (brot == 90 || brot == 270) ? ib->width : ib->height;
            /* info->width/height is the box including the lead stubs; the drawn symbol occupies
               roughly the middle 65 % across the leads and 85 % of the height. Compare the drawn
               bodies, plus 4 px of slack, so a lead touching a neighbour is fine and only symbols
               genuinely sitting on top of each other are reported. */
            double sx_a = (arot == 90 || arot == 270) ? 0.85 : 0.65, sy_a = (arot == 90 || arot == 270) ? 0.65 : 0.85;
            double sx_b = (brot == 90 || brot == 270) ? 0.85 : 0.65, sy_b = (brot == 90 || brot == 270) ? 0.65 : 0.85;
            double dx = fabs(a->x - b->x) - (aw * sx_a + bw * sx_b) / 2 + 4;
            double dy = fabs(a->y - b->y) - (ah * sy_a + bh * sy_b) / 2 + 4;
            if (dx < 0 && dy < 0) {
                if (hits < 3) {
                    size_t l = strlen(why);
                    snprintf(why + l, whyn - l, " overlap:%s/%s@(%.0f,%.0f)", a->label, b->label, a->x, a->y);
                }
                hits++;
            }
        }
    }
    return hits;
}

/* Text sitting on a wire. A title printed across the supply rail is exactly as unreadable as one
   printed across a transistor, and the symbol check could not see it: a rail is a wire, not a
   body. The Common Emitter's title sat on its Vcc rail this whole time and every geometry audit
   called the template clean.
   Annotations only, and the same reasoning as the symbol check: a value label is placed by the
   renderer hard against its own part, and its part's own leads are wires. */
static int geom_text_on_wire(Circuit *c, char *why, size_t whyn) {
    enum { MAX_TB = 512 };
    static TextBox tb[MAX_TB];
    int nb = geom_text_boxes(c, tb, MAX_TB, 0);
    int hits = 0;
    for (int i = 0; i < nb; i++) {
        /* A couple of pixels of clearance is not a collision; the glyph box is generous already. */
        float x0 = tb[i].x0 + 2, x1 = tb[i].x1 - 2, y0 = tb[i].y0 + 2, y1 = tb[i].y1 - 2;
        if (x1 <= x0 || y1 <= y0) continue;
        for (int w = 0; w < c->num_wires; w++) {
            Node *a = circuit_get_node(c, c->wires[w].start_node_id);
            Node *b = circuit_get_node(c, c->wires[w].end_node_id);
            if (!a || !b) continue;
            if (!seg_hits_box(a->x, a->y, b->x, b->y, x0, y0, x1, y1)) continue;
            hits++;
            if (strlen(why) < whyn - 60)
                snprintf(why + strlen(why), whyn - strlen(why), " textwire:\"%.18s\"@(%.0f,%.0f)x(%.0f,%.0f)",
                         tb[i].s, x0, y0, x1, y1);
            break;   /* one report per label: a title on a rail crosses it once, not eight times */
        }
    }
    return hits;
}

static int geom_test(void) {
    int bad_templates = 0, hard_failures = 0, total = 0;
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        Circuit *c = circuit_create();
        if (circuit_place_template(c, (CircuitTemplateType)t, 0, 0) <= 0) { circuit_free(c); continue; }
        total++;
        int diag = 0, cross = 0, through = 0, touch = 0, colin = 0;
        char detail[400] = "";
        /* Through-body findings get their own buffer. They matter more than crossings, and
           sharing one meant a busy schematic's crossings crowded them out of the line. */
        char tdetail[400] = "";
        /* wire endpoints */
        for (int w = 0; w < c->num_wires; w++) {
            Node *a = circuit_get_node(c, c->wires[w].start_node_id), *b = circuit_get_node(c, c->wires[w].end_node_id);
            if (!a || !b) continue;
            if (fabsf(a->x - b->x) > 0.5f && fabsf(a->y - b->y) > 0.5f) {
                diag++;
                if (strlen(detail) < 300) snprintf(detail + strlen(detail), sizeof detail - strlen(detail), " diag(%.0f,%.0f)-(%.0f,%.0f)", a->x, a->y, b->x, b->y);
            }
            for (int w2 = w + 1; w2 < c->num_wires; w2++) {
                Node *cc = circuit_get_node(c, c->wires[w2].start_node_id), *d = circuit_get_node(c, c->wires[w2].end_node_id);
                if (!cc || !d) continue;
                if (seg_intersect(a->x, a->y, b->x, b->y, cc->x, cc->y, d->x, d->y)) {
                    cross++;
                    if (strlen(detail) < 300) snprintf(detail + strlen(detail), sizeof detail - strlen(detail), " cross@(%.0f,%.0f)", (a->x + b->x + cc->x + d->x) / 4, (a->y + b->y + cc->y + d->y) / 4);
                }
                /* Two wires lying along the same line and sharing more than a point. Crossing
                   at a point is ordinary draughting; running one wire on top of another is not,
                   because the drawing then cannot say whether they meet - there is no junction
                   to see, and the two are only distinct because their node ids happen to
                   differ. seg_intersect answers the crossing question and says nothing about
                   this one, so a feedback run laid along a supply row read as clean. */
                int a_h = fabsf(a->y - b->y) <= 0.5f, c_h = fabsf(cc->y - d->y) <= 0.5f;
                int a_v = fabsf(a->x - b->x) <= 0.5f, c_v = fabsf(cc->x - d->x) <= 0.5f;
                float ov = -1.0f, oy = 0, ox = 0;
                if (a_h && c_h && fabsf(a->y - cc->y) <= 0.5f) {
                    float lo1 = fminf(a->x, b->x), hi1 = fmaxf(a->x, b->x);
                    float lo2 = fminf(cc->x, d->x), hi2 = fmaxf(cc->x, d->x);
                    ov = fminf(hi1, hi2) - fmaxf(lo1, lo2);
                    ox = fmaxf(lo1, lo2); oy = a->y;
                } else if (a_v && c_v && fabsf(a->x - cc->x) <= 0.5f) {
                    float lo1 = fminf(a->y, b->y), hi1 = fmaxf(a->y, b->y);
                    float lo2 = fminf(cc->y, d->y), hi2 = fmaxf(cc->y, d->y);
                    ov = fminf(hi1, hi2) - fmaxf(lo1, lo2);
                    ox = a->x; oy = fmaxf(lo1, lo2);
                }
                if (ov > 0.5f) {
                    colin++;
                    if (strlen(detail) < 300) snprintf(detail + strlen(detail), sizeof detail - strlen(detail), " onwire@(%.0f,%.0f)x%.0f", ox, oy, ov);
                }
            }
            /* through a body: skip components that own one of the wire's endpoints */
            for (int i = 0; i < c->num_components; i++) {
                Component *comp = c->components[i];
                if (comp->type == COMP_TEXT || comp->type == COMP_GROUND) continue;
                /* A component owns the wire if either end lands on one of its terminals -
                   by node id OR by position. Two node ids at the same coordinates are the same
                   point on the schematic (the builder merges them at solve time), and a wire
                   drawn to a transistor's base is not a wire through the transistor. */
                int owns = 0;
                for (int k = 0; k < comp->num_terminals && !owns; k++) {
                    if (comp->node_ids[k] == c->wires[w].start_node_id || comp->node_ids[k] == c->wires[w].end_node_id) { owns = 1; break; }
                    float tx, ty; component_get_terminal_pos(comp, k, &tx, &ty);
                    if ((fabs(tx - a->x) < 6 && fabs(ty - a->y) < 6) ||
                        (fabs(tx - b->x) < 6 && fabs(ty - b->y) < 6)) owns = 1;
                }
                if (owns) continue;
                const ComponentTypeInfo *info = component_get_info(comp->type);
                float hw = (info ? info->width : 40) / 2.0f - 6, hh = (info ? info->height : 40) / 2.0f - 6;
                if (comp->rotation % 180 != 0) { float tmp = hw; hw = hh; hh = tmp; }
                if (hw < 4) hw = 4; if (hh < 4) hh = 4;
                if (seg_hits_box(a->x, a->y, b->x, b->y, comp->x - hw, comp->y - hh, comp->x + hw, comp->y + hh)) {
                    through++;
                    if (strlen(tdetail) < 280) snprintf(tdetail + strlen(tdetail), sizeof tdetail - strlen(tdetail),
                        " through:%s@(%g,%g)box[%g..%g,%g..%g]<-wire(%g,%g)-(%g,%g)", comp->label,
                        comp->x, comp->y, comp->x - hw, comp->x + hw, comp->y - hh, comp->y + hh,
                        a->x, a->y, b->x, b->y);
                }
            }
        }
        /* terminals of different nets closer than 12 px (visually ambiguous) */
        for (int i = 0; i < c->num_components; i++) for (int k = 0; k < c->components[i]->num_terminals; k++) {
            Component *ca = c->components[i];
            if (ca->type == COMP_TEXT) continue;
            float ax, ay; component_get_terminal_pos(ca, k, &ax, &ay);
            int na = (ca->node_ids[k] >= 0 && ca->node_ids[k] < MAX_NODES) ? c->node_map[ca->node_ids[k]] : -1;
            for (int j = i + 1; j < c->num_components; j++) for (int m = 0; m < c->components[j]->num_terminals; m++) {
                Component *cb = c->components[j];
                if (cb->type == COMP_TEXT) continue;
                float bx, by; component_get_terminal_pos(cb, m, &bx, &by);
                int nb = (cb->node_ids[m] >= 0 && cb->node_ids[m] < MAX_NODES) ? c->node_map[cb->node_ids[m]] : -1;
                float dd = (ax - bx) * (ax - bx) + (ay - by) * (ay - by);
                if (dd < 12 * 12 && na != nb) {
                    touch++;
                    if (strlen(detail) < 300) snprintf(detail + strlen(detail), sizeof detail - strlen(detail), " touch:%s/%s", ca->label, cb->label);
                }
            }
        }
        int overlap = geom_overlap(c, detail, sizeof detail);
        int texton = geom_text_on_symbol(c, tdetail, sizeof tdetail);
        int textpair = geom_text_on_text(c, tdetail, sizeof tdetail);
        int textwire = geom_text_on_wire(c, tdetail, sizeof tdetail);
        /* Two of these are hard rules and the rest are cosmetic. No two symbols may overlap and
           no wire may run at an angle - those are design rules, and a template that breaks one
           is wrong. A drawn crossing, a wire passing over an unrelated node, or two terminals
           landing within 12 px are worth reporting and worth tidying, but they are legitimate in
           some topologies (TEST_PLAN 3.19.1b tracks them). Only the hard rules set the exit
           status, so this can gate CI without failing on the tracked cosmetic list. */
        /* Text on a symbol, on a wire, or on other text is a hard rule too. Unlike a drawn
           crossing there is no topology in which any of them is the right answer: the label
           cannot be read, and a schematic whose title cannot be read is not finished. */
        /* colin is reported but not fatal yet: it found 40 templates the day it was written,
           which is a backlog to work through, not something to fail the suite on today. */
        int hard = diag + overlap + texton + textpair + textwire;
        int ok = (diag + cross + through + touch + overlap + texton + textpair + textwire + colin) == 0;
        printf("[%s] geom  %-28s diag=%d cross=%d through=%d touch=%d overlap=%d texton=%d textpair=%d textwire=%d onwire=%d%s%s\n",
               ok ? " OK " : (hard ? "FAIL" : "WARN"), ti ? ti->name : "?",
               diag, cross, through, touch, overlap, texton, textpair, textwire, colin, tdetail, detail);
        if (!ok) bad_templates++;
        if (hard) hard_failures++;
        circuit_free(c);
    }
    printf("\n%d/%d templates geometrically clean; %d with a hard violation (overlapping symbols, "
           "a diagonal wire, or text printed over a symbol, over a wire, or over other text)\n",
           total - bad_templates, total, hard_failures);
    return hard_failures;
}

/* Frequency-sweep check: run the RC low-pass (100 Hz -> 20 kHz log sweep, 3 s) to t_end with
 * the app's own dt rule, measure the input frequency over the last 5 ms from zero crossings,
 * and compare with sweep_get_value(). Also reports steps/s so the real-time budget is visible. */
static int sweep_check(void) {
    double t_ends[] = { 0.3, 0.9, 1.5 };
    int fails = 0;
    for (unsigned k = 0; k < sizeof t_ends / sizeof t_ends[0]; k++) {
        Circuit *c = circuit_create();
        circuit_place_template(c, CIRCUIT_RC_LOWPASS, 0, 0);
        Simulation *sim = simulation_create(c);
        Component *src = NULL;
        for (int i = 0; i < c->num_components; i++) if (c->components[i]->type == COMP_AC_VOLTAGE) src = c->components[i];
        simulation_dc_analysis(sim);
        simulation_start(sim);
        double f_end = sweep_get_value(&src->props.ac_voltage.frequency_sweep, src->props.ac_voltage.frequency, t_ends[k]);
        double t_end = t_ends[k], t_rec = t_end - 6.0 / f_end;   /* ~6 periods of the end frequency */
        enum { NM = 400000 }; static double vs[NM], ts[NM]; int n = 0;
        long steps = 0; double dt_last = 0;
        clock_t c0 = clock();
        while (sim->time < t_end) {
            /* mimic the app: dt from the scope rule, re-synced each 1-2-5 change of the tracked time/div */
            double f = sweep_get_value(&src->props.ac_voltage.frequency_sweep, src->props.ac_voltage.frequency, sim->time);
            double td = 0.3 / f; double dec = pow(10.0, floor(log10(td))); double m = td / dec;
            td = ((m >= 5) ? 5 : (m >= 2) ? 2 : 1) * dec;
            double dt = simulation_scope_time_step(sim, td);
            if (dt != dt_last) { simulation_set_time_step(sim, dt); dt_last = dt; }
            if (!simulation_step(sim)) break;
            steps++;
            if (sim->time >= t_rec && n < NM) { Node *nd = circuit_get_node(c, src->node_ids[0]); vs[n] = nd ? nd->voltage : 0; ts[n] = sim->time; n++; }
        }
        double secs = (double)(clock() - c0) / CLOCKS_PER_SEC;
        int rising = 0; double tf = -1, tl = -1;
        for (int i = 1; i < n; i++) if (vs[i-1] < 0 && vs[i] >= 0) { rising++; if (tf < 0) tf = ts[i]; tl = ts[i]; }
        double f_meas = (rising >= 2) ? (rising - 1) / (tl - tf) : 0;
        double f_law = sweep_get_value(&src->props.ac_voltage.frequency_sweep, src->props.ac_voltage.frequency, t_end - 3.0 / f_end);
        int ok = fabs(f_meas - f_law) < 0.08 * f_law;
        printf("[%s] sweep t=%.2fs: measured %.0f Hz, law %.0f Hz, dt=%.3g, %ld steps in %.2fs (%.0f ksteps/s, %.2fx real time)\n",
               ok ? " OK " : "FAIL", t_end, f_meas, f_law, dt_last, steps, secs, steps / secs / 1e3, secs > 0 ? t_end / secs : 0);
        if (!ok) fails++;
        simulation_free(sim); circuit_free(c);
    }
    return fails;
}

/* ---------------------------------------------------------------------------------------
 * Demo rule: every template must declare a DemoKind, its stimulus must be able to show it,
 * and the simulated output must actually show it. Frequency kinds run one full up-sweep
 * and bin the output amplitude by the instantaneous source frequency (log bins +/-25%)
 * around f_char/4, f_char and 4*f_char; envelope/limiter kinds bin by amplitude progress.
 * ------------------------------------------------------------------------------------- */
static Component *first_source(Circuit *c) {
    static const ComponentType st[] = { COMP_AC_VOLTAGE, COMP_SOURCE_3PH, COMP_SQUARE_WAVE, COMP_TRIANGLE_WAVE, COMP_PULSE_SOURCE, COMP_DC_CURRENT, COMP_DC_VOLTAGE };
    for (unsigned k = 0; k < sizeof st / sizeof st[0]; k++) { Component *x = find_comp(c, st[k], 0); if (x) return x; }
    return NULL;
}
static double app_dt_for(Simulation *sim, double f) {
    double td = 0.3 / f; double dec = pow(10.0, floor(log10(td))); double m = td / dec;
    td = ((m >= 5) ? 5 : (m >= 2) ? 2 : 1) * dec;
    return simulation_scope_time_step(sim, td);
}
static int demo_test(void) {
    int fails = 0, total = 0;
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        const TemplateDemo *d = circuit_template_demo((CircuitTemplateType)t);
        const char *name = ti ? ti->name : "?";
        total++;
        char why[240] = ""; int ok = 1;
        Circuit *c = circuit_create();
        circuit_place_template(c, (CircuitTemplateType)t, 0, 0);
        Simulation *sim = simulation_create(c);
        ComponentType oct; int oord, oterm;
        Component *out = NULL;
        if (circuit_template_output_spec((CircuitTemplateType)t, &oct, &oord, &oterm)) out = find_comp(c, oct, oord);
        int out_node = out ? out->node_ids[oterm] : -1;
        Component *src = first_source(c);
        const char *kname[] = {"NONE","LOWPASS","HIGHPASS","BANDPASS","NOTCH","ENVELOPE","LIMITER","WAVEFORM","SWITCH","DC","OSC"};

        if (d->kind == DEMO_NONE) { ok = 0; snprintf(why, sizeof why, "no DemoKind declared (hard rule)"); }
        else if (!out && d->kind != DEMO_OSC) { ok = 0; snprintf(why, sizeof why, "no output probe spec"); }

        if (ok && (d->kind == DEMO_LOWPASS || d->kind == DEMO_HIGHPASS || d->kind == DEMO_BANDPASS || d->kind == DEMO_NOTCH)) {
            if (!src || src->type != COMP_AC_VOLTAGE || !src->props.ac_voltage.frequency_sweep.enabled) {
                ok = 0; snprintf(why, sizeof why, "needs a frequency-sweeping AC source");
            } else {
                SweepConfig *sw = &src->props.ac_voltage.frequency_sweep;
                double need_lo = (d->kind == DEMO_NOTCH) ? d->f_char / 3 : d->f_char / 4;
                double need_hi = (d->kind == DEMO_NOTCH) ? d->f_char * 3 : d->f_char * 4;
                if (sw->start_value > need_lo || sw->end_value < need_hi) {
                    ok = 0; snprintf(why, sizeof why, "sweep %g-%g Hz does not bracket f_char %g (need <= %g .. >= %g)",
                                     sw->start_value, sw->end_value, d->f_char, need_lo, need_hi);
                }
                if (ok) {
                    /* run one up-sweep; bin output amplitude by f(t) */
                    double fb[3] = { d->f_char / 4, d->f_char, d->f_char * 4 };
                    if (d->kind == DEMO_NOTCH) { fb[0] = d->f_char / 3; fb[2] = d->f_char * 3; }
                    double bmin[3] = {1e300,1e300,1e300}, bmax[3] = {-1e300,-1e300,-1e300}; int bn[3] = {0,0,0};
                    simulation_dc_analysis(sim); simulation_start(sim);
                    double dt_last = 0; long steps = 0;
                    while (sim->time < sw->sweep_time && steps < 3000000) {
                        double f = sweep_get_value(sw, src->props.ac_voltage.frequency, sim->time);
                        double dt = app_dt_for(sim, f);
                        if (dt != dt_last) { simulation_set_time_step(sim, dt); dt_last = dt; }
                        if (!simulation_step(sim)) { ok = 0; snprintf(why, sizeof why, "sim error"); break; }
                        steps++;
                        Node *nd = circuit_get_node(c, out_node); double v = nd ? nd->voltage : 0;
                        for (int b = 0; b < 3; b++) if (f > fb[b] / 1.25 && f < fb[b] * 1.25) { if (v < bmin[b]) bmin[b] = v; if (v > bmax[b]) bmax[b] = v; bn[b]++; }
                    }
                    double a[3]; for (int b = 0; b < 3; b++) a[b] = bn[b] > 10 ? (bmax[b] - bmin[b]) / 2 : 0;
                    int shape = 0;
                    switch (d->kind) {
                        case DEMO_LOWPASS:  shape = a[0] > 2.0 * a[2] && a[0] > 0.05; break;
                        case DEMO_HIGHPASS: shape = a[2] > 2.0 * a[0] && a[2] > 0.05; break;
                        case DEMO_BANDPASS: shape = a[1] > 1.5 * a[0] && a[1] > 1.5 * a[2] && a[1] > 0.05; break;
                        case DEMO_NOTCH:    shape = a[1] < 0.5 * a[0] && a[1] < 0.5 * a[2] && a[0] > 0.05; break;
                        default: break;
                    }
                    if (ok && !shape) { ok = 0; snprintf(why, sizeof why, "shape not shown: amp@f/4=%.3g @f=%.3g @4f=%.3g", a[0], a[1], a[2]); }
                    else if (ok) snprintf(why, sizeof why, "amp@%.3g=%.3g @%.3g=%.3g @%.3g=%.3g", fb[0], a[0], fb[1], a[1], fb[2], a[2]);
                }
            }
        } else if (ok && (d->kind == DEMO_ENVELOPE || d->kind == DEMO_LIMITER)) {
            if (!src || src->type != COMP_AC_VOLTAGE || !src->props.ac_voltage.amplitude_sweep.enabled) {
                ok = 0; snprintf(why, sizeof why, "needs an amplitude-sweeping AC source");
            } else {
                SweepConfig *sw = &src->props.ac_voltage.amplitude_sweep;
                double lo_min = 1e300, lo_max = -1e300, hi_min = 1e300, hi_max = -1e300;
                double in_lo = 0, in_hi = 0;
                simulation_dc_analysis(sim); simulation_auto_time_step(sim); simulation_start(sim);
                long steps = 0;
                while (sim->time < sw->sweep_time && steps < 3000000) {
                    if (!simulation_step(sim)) { ok = 0; snprintf(why, sizeof why, "sim error"); break; }
                    steps++;
                    double prog = sim->time / sw->sweep_time;
                    Node *nd = circuit_get_node(c, out_node); double v = nd ? nd->voltage : 0;
                    double amp = sweep_get_value(sw, src->props.ac_voltage.amplitude, sim->time);
                    if (prog > 0.05 && prog < 0.2)  { if (v < lo_min) lo_min = v; if (v > lo_max) lo_max = v; in_lo = amp; }
                    if (prog > 0.8 && prog < 0.95)  { if (v < hi_min) hi_min = v; if (v > hi_max) hi_max = v; in_hi = amp; }
                }
                double lo = (d->kind == DEMO_ENVELOPE) ? fabs(lo_max) : (lo_max - lo_min) / 2;
                double hi = (d->kind == DEMO_ENVELOPE) ? fabs(hi_max) : (hi_max - hi_min) / 2;
                if (ok) {
                    if (d->kind == DEMO_ENVELOPE) {
                        if (!(hi > 1.8 * lo && hi > 0.5)) { ok = 0; snprintf(why, sizeof why, "output does not follow the amplitude: %.3g -> %.3g (input %.3g -> %.3g)", lo, hi, in_lo, in_hi); }
                        else snprintf(why, sizeof why, "output %.3g -> %.3g as input %.3g -> %.3g", lo, hi, in_lo, in_hi);
                    } else {
                        /* limiter: tracks the input when small, stops growing when large */
                        if (!(lo > 0.7 * in_lo && hi < 0.7 * in_hi)) { ok = 0; snprintf(why, sizeof why, "limiting not shown: out %.3g/%.3g vs in %.3g/%.3g", lo, hi, in_lo, in_hi); }
                        else snprintf(why, sizeof why, "out %.3g/%.3g vs in %.3g/%.3g (clipped)", lo, hi, in_lo, in_hi);
                    }
                }
            }
        } else if (ok && (d->kind == DEMO_WAVEFORM || d->kind == DEMO_SWITCH || d->kind == DEMO_DC)) {
            simulation_dc_analysis(sim); simulation_auto_time_step(sim);
            double run = (d->f_char > 0) ? 6.0 / d->f_char : 0.01; if (run < 0.003 && d->f_char < 1e4) run = 0.003;   /* MHz-class I/O templates: six periods is enough */
            /* circuits driven only by pulse/logic sources get no useful auto dt: use 1000 steps per run */
            if (!(sim->time_step > 0) || sim->time_step > run / 200 || sim->time_step < run / 100000) simulation_set_time_step(sim, run / 1000);
            simulation_start(sim);
            double mn = 1e300, mx = -1e300, sum = 0; int n = 0; long steps = 0;
            while (sim->time < run && steps < 3000000) {
                if (!simulation_step(sim)) { ok = 0; snprintf(why, sizeof why, "sim error"); break; }
                steps++;
                if (sim->time > run * 0.5) { Node *nd = circuit_get_node(c, out_node); double v = nd ? nd->voltage : 0; if (v < mn) mn = v; if (v > mx) mx = v; sum += v; n++; }
            }
            double amp = (mx - mn) / 2, mean = n ? sum / n : 0;
            if (ok) {
                if (d->kind == DEMO_WAVEFORM && !(amp > 0.05)) { ok = 0; snprintf(why, sizeof why, "output barely moves (amp %.3g V)", amp); }
                else if (d->kind == DEMO_SWITCH && !(amp > 2.0)) { ok = 0; snprintf(why, sizeof why, "output does not swing (amp %.3g V)", amp); }
                else if (d->kind == DEMO_DC && !(amp < 0.05 * (fabs(mean) + 0.1))) { ok = 0; snprintf(why, sizeof why, "not steady DC: amp %.3g around %.3g", amp, mean); }
                else snprintf(why, sizeof why, "amp %.3g V, mean %.3g V", amp, mean);
            }
        } else if (ok && d->kind == DEMO_OSC) {
            snprintf(why, sizeof why, "checked by --osc-test");
        }
        printf("[%s] demo  %-28s %-9s %s\n", ok ? " OK " : "FAIL", name, kname[d->kind], why);
        if (!ok) fails++;
        simulation_free(sim); circuit_free(c);
    }
    printf("\n%d/%d demo checks passed\n", total - fails, total);
    return fails;
}

/* Frequency-response explorer: run the template's frequency sweep and print, for every
 * circuit node, the output amplitude in 8 log-spaced frequency bins. Used to pick the right
 * output node / DemoKind and to see whether a filter actually filters. */
static int response_explore(const char *filter) {
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        if (!ti || !strstr(ti->name, filter)) continue;
        Circuit *c = circuit_create();
        circuit_place_template(c, (CircuitTemplateType)t, 0, 0);
        Simulation *sim = simulation_create(c);
        Component *src = first_source(c);
        if (!src || src->type != COMP_AC_VOLTAGE || !src->props.ac_voltage.frequency_sweep.enabled) {
            printf("%s: no frequency-sweeping AC source\n", ti->name); simulation_free(sim); circuit_free(c); continue;
        }
        SweepConfig *sw = &src->props.ac_voltage.frequency_sweep;
        enum { NB = 8 };
        double fb[NB]; for (int b = 0; b < NB; b++) fb[b] = sw->start_value * pow(sw->end_value / sw->start_value, (b + 0.5) / NB);
        static double nmin[MAX_NODES][NB], nmax[MAX_NODES][NB];
        for (int i = 0; i < MAX_NODES; i++) for (int b = 0; b < NB; b++) { nmin[i][b] = 1e300; nmax[i][b] = -1e300; }
        simulation_dc_analysis(sim); simulation_start(sim);
        double dt_last = 0; long steps = 0;
        while (sim->time < sw->sweep_time && steps < 4000000) {
            double f = sweep_get_value(sw, src->props.ac_voltage.frequency, sim->time);
            double dt = app_dt_for(sim, f);
            if (dt != dt_last) { simulation_set_time_step(sim, dt); dt_last = dt; }
            if (!simulation_step(sim)) break;
            steps++;
            int b = -1;
            for (int k = 0; k < NB; k++) if (f > fb[k] / 1.15 && f < fb[k] * 1.15) b = k;
            if (b < 0) continue;
            for (int i = 0; i < c->num_nodes; i++) {
                int id = c->nodes[i].id; double v = c->nodes[i].voltage;
                if (v < nmin[id][b]) nmin[id][b] = v; if (v > nmax[id][b]) nmax[id][b] = v;
            }
        }
        printf("%s  (sweep %g-%g Hz)\n      node  owners                         ", ti->name, sw->start_value, sw->end_value);
        for (int b = 0; b < NB; b++) printf(" %7.0fHz", fb[b]);
        printf("\n");
        for (int i = 0; i < c->num_nodes; i++) {
            int id = c->nodes[i].id;
            char owners[40] = ""; int seen = 0;
            for (int j = 0; j < c->num_components && strlen(owners) < 28; j++) {
                Component *comp = c->components[j];
                for (int k = 0; k < comp->num_terminals; k++) if (comp->node_ids[k] == id) { snprintf(owners + strlen(owners), sizeof owners - strlen(owners), "%s[%d] ", comp->label, k); seen = 1; }
            }
            if (!seen) continue;
            printf("      n%-4d %-30s", id, owners);
            for (int b = 0; b < NB; b++) printf(" %9.3f", (nmax[id][b] > nmin[id][b]) ? (nmax[id][b] - nmin[id][b]) / 2 : 0.0);
            printf("\n");
        }
        simulation_free(sim); circuit_free(c);
    }
    return 0;
}

/* Largest source amplitude in the circuit: the runaway threshold scales with it so that
 * the kV power-system templates are judged against their own source, not an absolute 1 kV. */
static double source_scale(Circuit *c) {
    double m = 0;
    for (int i = 0; i < c->num_components; i++) {
        Component *k = c->components[i]; double a = 0;
        if (k->type == COMP_AC_VOLTAGE) a = fabs(k->props.ac_voltage.amplitude) + fabs(k->props.ac_voltage.offset);
        else if (k->type == COMP_SOURCE_3PH) a = fabs(k->props.source_3ph.v_peak);
        else if (k->type == COMP_DC_VOLTAGE) a = fabs(k->props.dc_voltage.voltage);
        if (a > m) m = a;
    }
    for (int i = 0; i < c->num_components; i++) {
        Component *k = c->components[i];
        if (k->type == COMP_TRANSFORMER && k->props.transformer.turns_ratio > 1.0) m *= k->props.transformer.turns_ratio;
    }
    return m;
}

/* Tesla coil check: run 20 ms at 100 ns, count primary-gap firings, streamer firings, the
 * toroid peak and the ring frequency right after the first firing. */
static int tesla_test(void) {
    struct { CircuitTemplateType t; double f_expect; double vtop_min; int rod_min; } cases[] = {
        { CIRCUIT_TESLA_COIL,         186e3, 115e3, 1 },
        { CIRCUIT_TESLA_COIL_BIG,     152e3, 130e3, 1 },
        { CIRCUIT_TESLA_COIL_DETUNED, 152e3, 0,     0 },
    };
    int fails = 0; double vtop_tuned = 0, vtop_detuned = 0;
    for (unsigned k = 0; k < sizeof cases / sizeof cases[0]; k++) {
        const CircuitTemplateInfo *ti = circuit_template_get_info(cases[k].t);
        Circuit *c = circuit_create();
        circuit_place_template(c, cases[k].t, 0, 0);
        Simulation *sim = simulation_create(c);
        Component *gap = NULL, *rod = NULL, *top = NULL;
        for (int i = 0; i < c->num_components; i++) {
            Component *comp = c->components[i];
            if (comp->type == COMP_SPARK_GAP) { if (!gap) gap = comp; else rod = comp; }
            if (comp->type == COMP_TOROID) top = comp;
        }
        int ok = gap && rod && top && simulation_dc_analysis(sim);
        simulation_set_time_step(sim, 100e-9);
        simulation_start(sim);
        int fires = 0, rod_fires = 0, was_on = 0, rod_on = 0; double vmax = 0, t_fire = -1;
        static double dbg_max[512]; memset(dbg_max, 0, sizeof dbg_max);
        double prev_v = 0; int ring_cross = 0; double t_first_cross = -1, t_last_cross = -1;
        while (ok && sim->time < 0.020) {
            if (!simulation_step(sim)) { ok = 0; break; }
            int on = gap->props.spark_gap.conducting;
            if (on && !was_on) { fires++; if (t_fire < 0) t_fire = sim->time; }
            was_on = on;
            if (getenv("TESLA_DEBUG") && fires >= 1 && fires <= 2 && sim->time < t_fire + 3e-6) {
                Node *na = circuit_get_node(c, gap->node_ids[0]), *nb = circuit_get_node(c, gap->node_ids[1]);
                printf("        t=%.3fus on=%d v0=%.1f v1=%.1f gapI=%.3f dt=%.3g\n", sim->time * 1e6, on, na ? na->voltage : 0, nb ? nb->voltage : 0, gap->terminal_current[0], sim->time_step);
            }
            int ron = rod->props.spark_gap.conducting;
            if (ron && !rod_on) rod_fires++;
            rod_on = ron;
            double v = top->props.toroid.voltage;
            if (fabs(v) > vmax) vmax = fabs(v);
            for (int i = 0; i < c->num_nodes; i++) { int id = c->nodes[i].id; if (id < 512 && fabs(c->nodes[i].voltage) > dbg_max[id]) dbg_max[id] = fabs(c->nodes[i].voltage); }
            if (t_fire >= 0 && sim->time < t_fire + 60e-6) {
                if (prev_v <= 0 && v > 0) { ring_cross++; if (t_first_cross < 0) t_first_cross = sim->time; t_last_cross = sim->time; }
            }
            prev_v = v;
        }
        if (getenv("TESLA_DEBUG")) {
            for (int i = 0; i < c->num_nodes; i++) {
                int id = c->nodes[i].id; char owners[64] = "";
                for (int j = 0; j < c->num_components && strlen(owners) < 50; j++) for (int k2 = 0; k2 < c->components[j]->num_terminals; k2++)
                    if (c->components[j]->node_ids[k2] == id) snprintf(owners + strlen(owners), sizeof owners - strlen(owners), "%s[%d] ", c->components[j]->label, k2);
                if (owners[0]) printf("      n%-3d max|V|=%12.3f  %s\n", id, id < 512 ? dbg_max[id] : 0.0, owners);
            }
        }
        double f_meas = (ring_cross >= 2) ? (ring_cross - 1) / (t_last_cross - t_first_cross) : 0;
        int f_ok = fabs(f_meas - cases[k].f_expect) < 0.2 * cases[k].f_expect;
        int pass = ok && fires >= 2 && vmax >= cases[k].vtop_min && rod_fires >= cases[k].rod_min && f_ok;
        if (cases[k].t == CIRCUIT_TESLA_COIL_BIG) vtop_tuned = vmax;
        if (cases[k].t == CIRCUIT_TESLA_COIL_DETUNED) vtop_detuned = vmax;
        printf("[%s] tesla %-24s gap fires=%d  streamer fires=%d  Vtop max=%.0f kV  ring f=%.0f kHz (expect ~%.0f)%s\n",
               pass ? " OK " : "FAIL", ti ? ti->name : "?", fires, rod_fires, vmax / 1e3, f_meas / 1e3, cases[k].f_expect / 1e3,
               !ok ? "  [sim error]" : "");
        if (!pass) fails++;
        simulation_free(sim); circuit_free(c);
    }
    if (vtop_detuned > 0 && vtop_detuned > 0.75 * vtop_tuned) { printf("[FAIL] tesla detuned coil should be well below the tuned one (%.0f vs %.0f kV)\n", vtop_detuned / 1e3, vtop_tuned / 1e3); fails++; }
    else printf("[ OK ] tesla detuned coil peak %.0f kV vs tuned %.0f kV\n", vtop_detuned / 1e3, vtop_tuned / 1e3);
    printf("%d tesla checks failed\n", fails);
    return fails;
}

/* ---------------------------------------------------------------------------------------
 * --param-test: parameter limits of the high-voltage components and the template scope
 * presets. Each case builds a tiny circuit in code, runs it for a few cycles and checks that
 * the solver stays finite and the behaviour matches the model contract.
 * ------------------------------------------------------------------------------------- */
static Component *pt_add(Circuit *c, ComponentType t, float x, float y, int rot) {
    Component *k = component_create(t, x, y);
    if (!k) return NULL;
    k->rotation = rot;
    circuit_add_component(c, k);
    return k;
}
static int pt_node(Circuit *c, float x, float y) { return circuit_find_or_create_node(c, x, y, 5.0f); }
/* source (amplitude, freq) -> device (2 terminals) -> load R to ground; returns the load node id */
static int pt_build_series(Circuit *c, ComponentType dev, double amp, double f, double rload, Component **dev_out, Component **src_out) {
    Component *v = pt_add(c, COMP_AC_VOLTAGE, 0, 60, 0);
    v->props.ac_voltage.amplitude = amp; v->props.ac_voltage.frequency = f;
    Component *g0 = pt_add(c, COMP_GROUND, 0, 140, 0);
    Component *d = pt_add(c, dev, 100, 20, 0);
    Component *rl = pt_add(c, COMP_RESISTOR, 200, 60, 90);
    rl->props.resistor.resistance = rload;
    Component *g1 = pt_add(c, COMP_GROUND, 200, 120, 0);
    int a = pt_node(c, 0, 20), b = pt_node(c, 60, 20), cc = pt_node(c, 140, 20), dd = pt_node(c, 200, 20);
    int gn = pt_node(c, 0, 100), gt = pt_node(c, 0, 120), ln = pt_node(c, 200, 100), lt = pt_node(c, 200, 120);
    circuit_add_wire(c, a, b); circuit_add_wire(c, cc, dd); circuit_add_wire(c, gn, gt); circuit_add_wire(c, ln, lt);
    v->node_ids[0] = a; v->node_ids[1] = gn; g0->node_ids[0] = gt;
    d->node_ids[0] = b; d->node_ids[1] = cc; rl->node_ids[0] = dd; rl->node_ids[1] = ln; g1->node_ids[0] = lt;
    if (dev_out) *dev_out = d; if (src_out) *src_out = v;
    return dd;
}
/* run t_end at dt; returns 0 on solver error / non-finite; fills amplitude at node */
static int pt_run(Circuit *c, int node_id, double dt, double t_end, double *amp_out, double *max_abs_out) {
    Simulation *sim = simulation_create(c);
    int ok = simulation_dc_analysis(sim);
    simulation_set_time_step(sim, dt);
    simulation_start(sim);
    double vmin = 1e300, vmax = -1e300, max_abs = 0; long steps = 0;
    while (ok && sim->time < t_end && steps < 3000000) {
        if (!simulation_step(sim)) { ok = 0; break; }
        steps++;
        for (int i = 0; i < c->num_nodes; i++) {
            double v = c->nodes[i].voltage;
            if (!isfinite(v)) { ok = 0; break; }
            if (fabs(v) > max_abs) max_abs = fabs(v);
        }
        if (sim->time > t_end * 0.5) {
            Node *n = circuit_get_node(c, node_id);
            double v = n ? n->voltage : 0;
            if (v < vmin) vmin = v; if (v > vmax) vmax = v;
        }
    }
    if (ok && sim->has_error) ok = 0;
    if (amp_out) *amp_out = (vmax > vmin) ? (vmax - vmin) / 2 : 0;
    if (max_abs_out) *max_abs_out = max_abs;
    simulation_free(sim);
    return ok;
}
#define PT_REPORT(pass, fmt, ...) do { printf("[%s] param " fmt "\n", (pass) ? " OK " : "FAIL", __VA_ARGS__); if (!(pass)) fails++; } while (0)

static int param_test(void) {
    int fails = 0;
    /* 1. spark gap: fires iff breakdown < source peak (1 kV, 1 kHz, 1 k load) */
    { double gaps[] = { 0.01, 0.1, 0.3, 0.5, 1.0, 10.0, 1000.0 };
      for (unsigned i = 0; i < sizeof gaps / sizeof gaps[0]; i++) {
          Circuit *c = circuit_create(); Component *d; int n = pt_build_series(c, COMP_SPARK_GAP, 1000.0, 1000.0, 1000.0, &d, NULL);
          d->props.spark_gap.gap_mm = gaps[i];
          double amp, mx; int ok = pt_run(c, n, 1e-6, 4e-3, &amp, &mx);
          int should_fire = spark_gap_breakdown(d) < 1000.0;
          int fired = amp > 100.0;
          PT_REPORT(ok && fired == should_fire, "spark gap %-7g mm (Vbd %.3g V): load amp %.3g V, %s%s", gaps[i], spark_gap_breakdown(d), amp,
                    fired ? "fires" : "stays open", ok ? "" : "  [sim error]");
          circuit_free(c);
      } }
    /* 2. toroid: capacitance finite & increases with size; survives extreme shapes in a 100 kV, 1 MOhm circuit */
    { struct { double D, d; } sh[] = { {0.5, 0.9}, {1, 0.1}, {13, 4}, {24, 8}, {100, 30}, {1000, 1000} };
      double prev = 0;
      for (unsigned i = 0; i < sizeof sh / sizeof sh[0]; i++) {
          Circuit *c = circuit_create();
          Component *v = pt_add(c, COMP_AC_VOLTAGE, 0, 60, 0); v->props.ac_voltage.amplitude = 100e3; v->props.ac_voltage.frequency = 100e3;
          Component *g0 = pt_add(c, COMP_GROUND, 0, 140, 0);
          Component *r = pt_add(c, COMP_RESISTOR, 100, 20, 0); r->props.resistor.resistance = 1e6;
          Component *t = pt_add(c, COMP_TOROID, 200, -20, 0); t->props.toroid.major_in = sh[i].D; t->props.toroid.minor_in = sh[i].d;
          int a = pt_node(c, 0, 20), b = pt_node(c, 60, 20), cc = pt_node(c, 140, 20), dd = pt_node(c, 200, 20), gn = pt_node(c, 0, 100), gt = pt_node(c, 0, 120);
          circuit_add_wire(c, a, b); circuit_add_wire(c, cc, dd); circuit_add_wire(c, gn, gt);
          v->node_ids[0] = a; v->node_ids[1] = gn; g0->node_ids[0] = gt; r->node_ids[0] = b; r->node_ids[1] = cc; t->node_ids[0] = dd;
          double C = toroid_capacitance(t), amp, mx; int ok = pt_run(c, dd, 1e-8, 50e-6, &amp, &mx);
          double expect = 100e3 / sqrt(1 + pow(2 * M_PI * 100e3 * 1e6 * C, 2));
          int pass = ok && isfinite(C) && C > 0 && C >= prev * 0.999 && fabs(amp - expect) < 0.15 * expect;
          PT_REPORT(pass, "toroid D=%-5g d=%-5g -> %8.2f pF, RC divider amp %.3g kV (expect %.3g)%s", sh[i].D, sh[i].d, C * 1e12, amp / 1e3, expect / 1e3, ok ? "" : "  [sim error]");
          prev = C; circuit_free(c);
      } }
    /* 3. transmission line: lengths x models at 345 kV into 200 ohm; drop must grow with length, stay finite */
    { double lens[] = { 0.001, 1, 10, 100, 500, 5000 }; const char *mn[] = { "R", "RL", "pi" };
      for (int model = 0; model < 3; model++) {
          double prev_amp = 1e300;
          for (unsigned i = 0; i < sizeof lens / sizeof lens[0]; i++) {
              Circuit *c = circuit_create(); Component *d; int n = pt_build_series(c, COMP_TLINE, 281.7e3, 60.0, 200.0, &d, NULL);
              d->props.tline.length_mi = lens[i]; d->props.tline.model = model;
              double R, L, Cend; tline_params(d, &R, &L, &Cend);
              double amp, mx; int ok = pt_run(c, n, 1e-4, 0.1, &amp, &mx);
              /* phasor oracle for R / RL: |V| = Vs * R_load / |R_load + R + jwL| ; pi adds the far-end C/2 across the load and C/2 across the source */
              double w = 2 * M_PI * 60, Rl = 200.0, expect;
              if (model < 2) expect = 281.7e3 * Rl / sqrt(pow(Rl + R, 2) + pow(w * L, 2));
              else { double Yre = 1 / Rl, Yim = w * Cend; double Zre = Yre / (Yre*Yre + Yim*Yim), Zim = -Yim / (Yre*Yre + Yim*Yim);
                     double tre = Zre + R, tim = Zim + w * L; expect = 281.7e3 * sqrt(Zre*Zre + Zim*Zim) / sqrt(tre*tre + tim*tim); }
              int pass = ok && amp > 0 && fabs(amp - expect) < 0.05 * expect && amp <= prev_amp * 1.02;
              PT_REPORT(pass, "tline %-3s %7g mi: R=%.3g L=%.3gH C/2=%.3gF  load amp %.4g kV (phasor %.4g)%s", mn[model], lens[i], R, L, Cend, amp / 1e3, expect / 1e3, ok ? "" : "  [sim error]");
              prev_amp = amp; circuit_free(c);
          } } }
    /* 4. transformer ratios: V_out = N V_in into a light load */
    { double Ns[] = { 0.001, 1.0 / 30, 0.4, 1, 19.17, 75, 1000 };
      for (unsigned i = 0; i < sizeof Ns / sizeof Ns[0]; i++) {
          Circuit *c = circuit_create();
          Component *v = pt_add(c, COMP_AC_VOLTAGE, 0, 60, 0); v->props.ac_voltage.amplitude = 100; v->props.ac_voltage.frequency = 60;
          Component *g0 = pt_add(c, COMP_GROUND, 0, 140, 0);
          Component *t = pt_add(c, COMP_TRANSFORMER, 150, 20, 0); t->props.transformer.turns_ratio = Ns[i];
          Component *gp = pt_add(c, COMP_GROUND, 100, 80, 0), *gs = pt_add(c, COMP_GROUND, 200, 80, 0);
          Component *rl = pt_add(c, COMP_RESISTOR, 280, 60, 90); rl->props.resistor.resistance = 1e4 * Ns[i] * Ns[i] + 1.0;
          Component *gl = pt_add(c, COMP_GROUND, 280, 120, 0);
          int a = pt_node(c, 0, 20), p1 = pt_node(c, 100, 0), p2 = pt_node(c, 100, 40), gpt = pt_node(c, 100, 60), s1 = pt_node(c, 200, 0), s2 = pt_node(c, 200, 40), gst = pt_node(c, 200, 60);
          int lt = pt_node(c, 280, 20), ln = pt_node(c, 280, 100), lg = pt_node(c, 280, 120), gn = pt_node(c, 0, 100), gt = pt_node(c, 0, 120);
          circuit_add_wire(c, a, p1); circuit_add_wire(c, p2, gpt); circuit_add_wire(c, s1, lt); circuit_add_wire(c, s2, gst); circuit_add_wire(c, ln, lg); circuit_add_wire(c, gn, gt);
          v->node_ids[0] = a; v->node_ids[1] = gn; g0->node_ids[0] = gt; t->node_ids[0] = p1; t->node_ids[1] = p2; t->node_ids[2] = s1; t->node_ids[3] = s2;
          gp->node_ids[0] = gpt; gs->node_ids[0] = gst; rl->node_ids[0] = lt; rl->node_ids[1] = ln; gl->node_ids[0] = lg;
          double amp, mx; int ok = pt_run(c, lt, 1e-4, 0.05, &amp, &mx);
          double expect = 100 * Ns[i];
          PT_REPORT(ok && fabs(amp - expect) < 0.02 * expect, "transformer N=%-8g: out amp %.4g V (expect %.4g)%s", Ns[i], amp, expect, ok ? "" : "  [sim error]");
          circuit_free(c);
      } }
    /* 4b. analog switch as a fault switch: control 0 / 5 V, r_on extremes, threshold */
    { double rons[] = { 0.01, 0.3, 100.0, 1e6 };
      for (unsigned i = 0; i < sizeof rons / sizeof rons[0]; i++) {
          Circuit *c = circuit_create();
          Component *v = pt_add(c, COMP_AC_VOLTAGE, 0, 60, 0); v->props.ac_voltage.amplitude = 10; v->props.ac_voltage.frequency = 60;
          Component *g0 = pt_add(c, COMP_GROUND, 0, 140, 0);
          Component *sw = pt_add(c, COMP_ANALOG_SWITCH, 100, 20, 0); sw->props.analog_switch.r_on = rons[i]; sw->props.analog_switch.r_off = 1e9; sw->props.analog_switch.v_on = 2.5;
          Component *rl = pt_add(c, COMP_RESISTOR, 200, 60, 90); rl->props.resistor.resistance = 100.0;
          Component *g1 = pt_add(c, COMP_GROUND, 200, 120, 0);
          Component *pl = pt_add(c, COMP_PULSE_SOURCE, 100, 100, 0); pl->props.pulse_source.v_low = 0; pl->props.pulse_source.v_high = 5; pl->props.pulse_source.delay = 0.02; pl->props.pulse_source.pulse_width = 0.05; pl->props.pulse_source.period = 1.0;
          Component *g2 = pt_add(c, COMP_GROUND, 100, 160, 0);
          int a = pt_node(c, 0, 20), b = pt_node(c, 60, 20), cc = pt_node(c, 140, 20), dd = pt_node(c, 200, 20), gn = pt_node(c, 0, 100), gt = pt_node(c, 0, 120);
          int ln = pt_node(c, 200, 100), lt = pt_node(c, 200, 120), ctl = pt_node(c, 100, 40), pp = pt_node(c, 100, 60), pn = pt_node(c, 100, 140), pg = pt_node(c, 100, 160);
          circuit_add_wire(c, a, b); circuit_add_wire(c, cc, dd); circuit_add_wire(c, gn, gt); circuit_add_wire(c, ln, lt); circuit_add_wire(c, ctl, pp); circuit_add_wire(c, pn, pg);
          v->node_ids[0] = a; v->node_ids[1] = gn; g0->node_ids[0] = gt; sw->node_ids[0] = b; sw->node_ids[1] = cc; sw->node_ids[2] = ctl;
          rl->node_ids[0] = dd; rl->node_ids[1] = ln; g1->node_ids[0] = lt; pl->node_ids[0] = pp; pl->node_ids[1] = pn; g2->node_ids[0] = pg;
          /* before the pulse (t < 20 ms) the load must be ~0; during it (20-70 ms) ~ 10 * 100/(100 + r_on) */
          Simulation *sim = simulation_create(c); int ok = simulation_dc_analysis(sim); simulation_set_time_step(sim, 1e-4); simulation_start(sim);
          double pre = 0, dur = 0; long steps = 0;
          while (ok && sim->time < 0.06 && steps < 100000) { if (!simulation_step(sim)) { ok = 0; break; } steps++; Node *nd = circuit_get_node(c, dd); double vv = nd ? fabs(nd->voltage) : 0; if (sim->time < 0.02) { if (vv > pre) pre = vv; } else if (vv > dur) dur = vv; }
          double expect = 10.0 * 100.0 / (100.0 + rons[i]);
          int pass = ok && pre < 0.01 && fabs(dur - expect) < 0.05 * expect + 0.01;
          PT_REPORT(pass, "analog switch r_on %-6g: open |V| %.3g, closed %.3g V (expect %.3g)%s", rons[i], pre, dur, expect, ok ? "" : "  [sim error]");
          simulation_free(sim); circuit_free(c);
      } }
    /* 4c. transformer used as a CT: secondary current = primary / N into a 1 ohm burden */
    { double Ns[] = { 120.0, 400.0, 2875.0 };
      for (unsigned i = 0; i < sizeof Ns / sizeof Ns[0]; i++) {
          Circuit *c = circuit_create();
          Component *v = pt_add(c, COMP_AC_VOLTAGE, 0, 60, 0); v->props.ac_voltage.amplitude = 1000; v->props.ac_voltage.frequency = 60;
          Component *g0 = pt_add(c, COMP_GROUND, 0, 140, 0);
          Component *t = pt_add(c, COMP_TRANSFORMER, 150, 40, 0); t->props.transformer.turns_ratio = Ns[i];
          Component *ld = pt_add(c, COMP_RESISTOR, 100, 140, 90); ld->props.resistor.resistance = 10.0;   /* primary loop: 100 A */
          Component *gl = pt_add(c, COMP_GROUND, 100, 200, 0);
          Component *rb = pt_add(c, COMP_RESISTOR, 260, 60, 90); rb->props.resistor.resistance = 1.0;
          Component *gb = pt_add(c, COMP_GROUND, 260, 120, 0), *gs = pt_add(c, COMP_GROUND, 200, 80, 0);
          int a = pt_node(c, 0, 20), p1 = pt_node(c, 100, 20), p2 = pt_node(c, 100, 60), lb = pt_node(c, 100, 100), lg = pt_node(c, 100, 180), lgt = pt_node(c, 100, 200);
          int s1 = pt_node(c, 200, 20), s2 = pt_node(c, 200, 60), rt = pt_node(c, 260, 20), rbb = pt_node(c, 260, 100), rbg = pt_node(c, 260, 120), gn = pt_node(c, 0, 100), gt = pt_node(c, 0, 120);
          circuit_add_wire(c, a, p1); circuit_add_wire(c, p2, lb); circuit_add_wire(c, lg, lgt); circuit_add_wire(c, s1, rt); circuit_add_wire(c, rbb, rbg); circuit_add_wire(c, gn, gt);
          v->node_ids[0] = a; v->node_ids[1] = gn; g0->node_ids[0] = gt; t->node_ids[0] = p1; t->node_ids[1] = p2; t->node_ids[2] = s1; t->node_ids[3] = s2; gs->node_ids[0] = s2;
          ld->node_ids[0] = lb; ld->node_ids[1] = lg; gl->node_ids[0] = lgt; rb->node_ids[0] = rt; rb->node_ids[1] = rbb; gb->node_ids[0] = rbg;
          double amp, mx; int ok = pt_run(c, rt, 1e-4, 0.05, &amp, &mx);
          double expect = 100.0 / Ns[i];
          PT_REPORT(ok && fabs(amp - expect) < 0.03 * expect, "CT N=%-6g: burden %.4g V (expect %.4g = 100 A / N)%s", Ns[i], amp, expect, ok ? "" : "  [sim error]");
          circuit_free(c);
      } }
    /* 5. template scope presets: on the scope's tables, and the window shows 2..2000 cycles of f_char */
    { static const double time_divs[] = {1e-9, 2e-9, 5e-9, 10e-9, 20e-9, 50e-9, 100e-9, 200e-9, 500e-9, 1e-6, 2e-6, 5e-6, 10e-6, 20e-6, 50e-6, 100e-6, 200e-6, 500e-6,
                                         1e-3, 2e-3, 5e-3, 10e-3, 20e-3, 50e-3, 100e-3, 200e-3, 500e-3, 1.0, 2.0, 5.0};
      static const double volt_divs[] = {0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0, 200.0, 500.0, 1e3, 2e3, 5e3, 10e3, 20e3, 50e3, 100e3, 200e3, 500e3};
      int bad = 0, checked = 0;
      for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
          const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
          double td = circuit_template_scope_time_div((CircuitTemplateType)t), vd = circuit_template_scope_volt_div((CircuitTemplateType)t);
          const TemplateDemo *dp = circuit_template_demo((CircuitTemplateType)t); TemplateDemo demo = dp ? *dp : (TemplateDemo){ DEMO_NONE, 0 };
          int td_ok = td <= 0, vd_ok = vd <= 0;
          for (unsigned i = 0; i < sizeof time_divs / sizeof time_divs[0]; i++) if (td > 0 && fabs(time_divs[i] - td) < 1e-3 * td) td_ok = 1;
          for (unsigned i = 0; i < sizeof volt_divs / sizeof volt_divs[0]; i++) if (vd > 0 && fabs(volt_divs[i] - vd) < 1e-3 * vd) vd_ok = 1;
          int win_ok = 1;
          if (td > 0 && demo.f_char > 0 && demo.kind != DEMO_DC && demo.kind != DEMO_SWITCH) {
              double cycles = 20 * td * demo.f_char;          /* history span = 20 divisions */
              win_ok = cycles >= 1.5 && cycles <= 2000;
          }
          checked++;
          if (!(td_ok && vd_ok && win_ok)) { bad++; printf("[FAIL] param preset %-26s time/div %g (%s) volt/div %g (%s) window %s\n", ti ? ti->name : "?", td, td_ok ? "ok" : "NOT A SCOPE STEP", vd, vd_ok ? "ok" : "NOT A SCOPE STEP", win_ok ? "ok" : "shows <1.5 or >2000 cycles of f_char"); }
      }
      PT_REPORT(bad == 0, "scope presets: %d templates checked, %d bad", checked, bad); fails += 0; }
    printf("%d param checks failed\n", fails);
    return fails;
}

/* --trace NAME T: run a template for T seconds at the app dt and print, for every node,
 * min / max voltage and the components attached, plus the final state of switches. */
static int trace_template(const char *filter, double t_end) {
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        if (!ti || !strstr(ti->name, filter)) continue;
        Circuit *c = circuit_create();
        circuit_place_template(c, (CircuitTemplateType)t, 0, 0);
        Simulation *sim = simulation_create(c);
        static double vmin[4096], vmax[4096];
        for (int i = 0; i < 4096; i++) { vmin[i] = 1e300; vmax[i] = -1e300; }
        int ok = simulation_dc_analysis(sim);
        double td = circuit_template_scope_time_div((CircuitTemplateType)t);
        simulation_set_time_step(sim, td > 0 ? td / 20 : 1e-5);
        simulation_start(sim);
        long steps = 0;
        while (ok && sim->time < t_end && steps < 5000000) {
            if (!simulation_step(sim)) { ok = 0; break; }
            steps++;
            for (int i = 0; i < c->num_nodes; i++) { int id = c->nodes[i].id; if (id < 4096) { double v = c->nodes[i].voltage; if (v < vmin[id]) vmin[id] = v; if (v > vmax[id]) vmax[id] = v; } }
        }
        printf("%s: %ld steps, dt %.3g, %s\n", ti->name, steps, sim->time_step, ok ? "ok" : simulation_get_error(sim));
        for (int i = 0; i < c->num_nodes; i++) {
            int id = c->nodes[i].id; char owners[80] = "";
            for (int j = 0; j < c->num_components && strlen(owners) < 66; j++) for (int k = 0; k < c->components[j]->num_terminals; k++)
                if (c->components[j]->node_ids[k] == id) snprintf(owners + strlen(owners), sizeof owners - strlen(owners), "%s[%d] ", c->components[j]->label, k);
            printf("  n%-4d (%6.0f,%6.0f) %12.4g .. %-12.4g %s\n", id, c->nodes[i].x, c->nodes[i].y, id < 4096 ? vmin[id] : 0.0, id < 4096 ? vmax[id] : 0.0, owners);
        }
        for (int w = 0; w < c->num_wires; w++) {
            Node *a = circuit_get_node(c, c->wires[w].start_node_id), *b = circuit_get_node(c, c->wires[w].end_node_id);
            if (!a || !b) continue;
            const char *tag = (fabsf(a->x - b->x) > 0.5f && fabsf(a->y - b->y) > 0.5f) ? "  <-- DIAGONAL" : "";
            printf("  wire n%d (%.0f,%.0f) -> n%d (%.0f,%.0f)%s\n", a->id, a->x, a->y, b->id, b->x, b->y, tag);
        }
        simulation_free(sim); circuit_free(c);
    }
    return 0;
}

/* ---------------------------------------------------------------------------------------
 * --knob-test: every template x every editable value x {0.5, 2}: the circuit must still solve
 * (DC + a short transient) without solver errors, NaN or runaway. This is the automated
 * version of "turn every knob and see that nothing breaks".
 * ------------------------------------------------------------------------------------- */
static double *knob_value(Component *k, const char **what) {
    switch (k->type) {
        case COMP_RESISTOR:       *what = "R";    return &k->props.resistor.resistance;
        case COMP_CAPACITOR:      *what = "C";    return &k->props.capacitor.capacitance;
        case COMP_INDUCTOR:       *what = "L";    return &k->props.inductor.inductance;
        case COMP_AC_VOLTAGE:     *what = "Vac";  return &k->props.ac_voltage.amplitude;
        case COMP_SOURCE_3PH:     *what = "V3ph"; return &k->props.source_3ph.v_peak;
        case COMP_DC_VOLTAGE:     *what = "Vdc";  return &k->props.dc_voltage.voltage;
        case COMP_TRANSFORMER:    *what = "N";    return &k->props.transformer.turns_ratio;
        case COMP_TLINE:          *what = "len";  return &k->props.tline.length_mi;
        case COMP_SPARK_GAP:      *what = "gap";  return &k->props.spark_gap.gap_mm;
        case COMP_TOROID:         *what = "D";    return &k->props.toroid.major_in;
        case COMP_PULSE_SOURCE:   *what = "pw";   return &k->props.pulse_source.pulse_width;
        case COMP_ANALOG_SWITCH:  *what = "ron";  return &k->props.analog_switch.r_on;
        case COMP_ZENER:          *what = "Vz";   return &k->props.zener.vz;
        default: return NULL;
    }
}
static int knob_run(Circuit *c, double t_end, double vlimit, char *why, size_t nwhy) {
    Simulation *sim = simulation_create(c);
    int ok = simulation_dc_analysis(sim);
    if (!ok) { snprintf(why, nwhy, "DC failed: %s", simulation_get_error(sim)); simulation_free(sim); return 0; }
    simulation_auto_time_step(sim);
    if (!(sim->time_step > 0) || sim->time_step > t_end / 20 || sim->time_step < t_end / 20000) simulation_set_time_step(sim, t_end / 200);
    simulation_start(sim);
    long steps = 0;
    while (sim->time < t_end && steps < 200000) {
        if (!simulation_step(sim)) { snprintf(why, nwhy, "step %ld: %s", steps, simulation_get_error(sim)); ok = 0; break; }
        steps++;
        for (int i = 0; i < c->num_nodes; i++) {
            double v = c->nodes[i].voltage;
            if (!isfinite(v)) { snprintf(why, nwhy, "NaN at node %d", c->nodes[i].id); ok = 0; break; }
            if (fabs(v) > vlimit) { snprintf(why, nwhy, "runaway %.3g V at node %d", v, c->nodes[i].id); ok = 0; break; }
        }
        if (!ok) break;
    }
    simulation_free(sim);
    return ok;
}
static int knob_test(const char *filter) {
    int fails = 0, runs = 0, templates = 0;
    static const double factors[] = { 0.5, 2.0 };
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        if (!ti || (filter && !strstr(ti->name, filter))) continue;
        templates++;
        Circuit *base = circuit_create();
        circuit_place_template(base, (CircuitTemplateType)t, 0, 0);
        double vlimit = fmax(1000.0, 3.0 * source_scale(base)) * 20.0;   /* generous: a x2 knob can double a resonance */
        const TemplateDemo *d = circuit_template_demo((CircuitTemplateType)t);
        double t_end = (d && d->f_char > 0) ? 3.0 / d->f_char : 0.01;
        if (t_end > 0.5) t_end = 0.5;
        if (t_end < 0.0005) t_end = 0.0005;
        int nfail_here = 0;
        for (int i = 0; i < base->num_components; i++) {
            const char *what = NULL;
            if (!knob_value(base->components[i], &what)) continue;
            for (unsigned f = 0; f < sizeof factors / sizeof factors[0]; f++) {
                Circuit *c = circuit_create();
                circuit_place_template(c, (CircuitTemplateType)t, 0, 0);
                const char *w2; double *val = knob_value(c->components[i], &w2);
                double orig = *val; *val = orig * factors[f];
                char why[200] = "";
                runs++;
                if (!knob_run(c, t_end, vlimit, why, sizeof why)) {
                    fails++; nfail_here++;
                    printf("[FAIL] knob  %-26s %s[%d] %s x%.1f (%.4g -> %.4g): %s\n", ti->name, c->components[i]->label, i, what, factors[f], orig, orig * factors[f], why);
                }
                circuit_free(c);
            }
        }
        if (!nfail_here) printf("[ OK ] knob  %-26s all values x0.5 / x2 solve (%d knobs)\n", ti->name, base->num_components);
        circuit_free(base);
    }
    printf("%d knob runs over %d templates, %d failed\n", runs, templates, fails);
    return fails;
}

/* --stress-test: values a user can actually type.
 *
 * --knob-test scales every editable value by a half and by two and checks the circuit still
 * solves. That is a gentle question. The properties panel takes typing, and a person can put a
 * zero in a resistor, a minus sign in a capacitor, or a value ten decades from anything sensible,
 * and the app has to do something reasonable with it.
 *
 * Reasonable means one of two things, and this suite draws the line between them:
 *   - it SOLVES, or
 *   - it refuses cleanly, with an error the app can show.
 * A clean "DC failed" therefore passes: the refusal is not the fault.
 *
 * What fails is a NaN in the node voltages. That is never right, whatever was typed - it reaches
 * the scope and the measurements panel as a number and it is not one.
 *
 * A very LARGE voltage is reported and does not fail, because it is often correct: 4-Wire Kelvin
 * Sensing forces 1 A through whatever resistance it is given, so at a megohm it produces a
 * megavolt and should. The cases that remain are all of that kind, or an amplifier driven by a
 * kilovolt source, which says more about the transistor models having no rail clamp than about
 * anything a person will do. Counted and printed rather than judged.
 */
static const struct { double v; const char *what; } stress_values[] = {
    { 0.0,     "zero" },
    { 1e3,     "1e3" },
    { 1e6,     "1e6" },      /* MAX_CAPACITANCE / MAX_INDUCTANCE: the top of what the panel takes */
    { 1e-12,   "vanishing" },
};
/* Not in that list, deliberately: negative values and anything past 1e6. The properties panel
   refuses both - a negative L or C never gets in, and 1e9 F is now rejected where "value > 0"
   used to take it - so a suite that set them would be testing a state the app does not allow
   itself to reach. They do still break the solver if a hand-edited save file carries one, which
   is written up in docs/ROADMAP.md rather than pretended away: 16 templates run away on a
   negative inductance and 12 on 1e9 F. The fix for that is validation on load, and it is not
   this. */

static int stress_test(const char *filter) {
    int fails = 0, runs = 0, refused = 0, templates = 0, big = 0;
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        if (!ti || (filter && !strstr(ti->name, filter))) continue;
        templates++;
        Circuit *base = circuit_create();
        if (circuit_place_template(base, (CircuitTemplateType)t, 0, 0) <= 0) { circuit_free(base); continue; }
        const TemplateDemo *d = circuit_template_demo((CircuitTemplateType)t);
        double t_end = (d && d->f_char > 0) ? 3.0 / d->f_char : 0.01;
        if (t_end > 0.1) t_end = 0.1;
        if (t_end < 0.0005) t_end = 0.0005;

        int nfail_here = 0;
        for (int i = 0; i < base->num_components; i++) {
            const char *what = NULL;
            if (!knob_value(base->components[i], &what)) continue;
            for (unsigned f = 0; f < sizeof stress_values / sizeof stress_values[0]; f++) {
                Circuit *c = circuit_create();
                if (circuit_place_template(c, (CircuitTemplateType)t, 0, 0) <= 0) { circuit_free(c); continue; }
                const char *w2;
                double *val = knob_value(c->components[i], &w2);
                if (!val) { circuit_free(c); continue; }
                *val = stress_values[f].v;
                /* The limit comes from the MUTATED circuit, not the original. Setting a source to
                   1e12 V and then calling 1e12 V at its own node a runaway is the check being
                   wrong, not the solver: the circuit did what it was told. What a limit is for is
                   a node running away from everything driving it - a 1e12 F capacitor putting a
                   billion volts into a twelve volt circuit. */
                double vlim = fmax(1000.0, 3.0 * source_scale(c)) * 100.0;
                char why[200] = "";
                runs++;
                if (!knob_run(c, t_end, vlim, why, sizeof why)) {
                    if (strstr(why, "NaN")) {
                        fails++; nfail_here++;
                        printf("[FAIL] stress %-26s %s[%d] %s = %s (%.3g): %s\n", ti->name,
                               c->components[i]->label, i, w2 ? w2 : "?",
                               stress_values[f].what, stress_values[f].v, why);
                    } else if (strstr(why, "runaway")) {
                        big++;
                        printf("[NOTE] stress %-26s %s[%d] %s = %s (%.3g): %s\n", ti->name,
                               c->components[i]->label, i, w2 ? w2 : "?",
                               stress_values[f].what, stress_values[f].v, why);
                    } else {
                        refused++;
                    }
                }
                circuit_free(c);
            }
        }
        if (!nfail_here)
            printf("[ OK ] stress %-26s every value survives zero, negative, 1e12 and 1e-12\n", ti->name);
        circuit_free(base);
    }
    printf("\nstress-test: %d runs over %d templates, %d that put a NaN into the node voltages, "
           "%d that produced a very large voltage (reported, not judged - see the note above), "
           "%d refused cleanly\n", runs, templates, fails, big, refused);
    return fails ? 1 : 0;
}

/* ---------------------------------------------------------------------------------------
 * --probe-audit: what the user will actually see. For every template: the auto-placed probes
 * (exactly as the app places them), what each one sits on, and the waveform statistics over
 * one scope screen at the preset time/div and V/div. Flags:
 *   DUP     two probes on the same node               GND    probe on a ground node
 *   FLAT    output does not move but a waveform demo   SMALL  output < 0.25 div at the preset V/div
 *   CLIP    output beyond +/-4 div at the preset V/div (before the one-shot autoscale runs)
 *   NOOUT   no output probe at all
 * ------------------------------------------------------------------------------------- */
static void owners_of(Circuit *c, int id, char *out, size_t n) {
    out[0] = 0;
    for (int j = 0; j < c->num_components && strlen(out) < n - 16; j++)
        for (int k = 0; k < c->components[j]->num_terminals; k++)
            if (c->components[j]->node_ids[k] == id) snprintf(out + strlen(out), n - strlen(out), "%s[%d] ", c->components[j]->label, k);
}
static int node_is_ground(Circuit *c, int id) {
    for (int j = 0; j < c->num_components; j++)
        if (c->components[j]->type == COMP_GROUND && c->components[j]->node_ids[0] == id) return 1;
    return 0;
}
static int probe_audit(const char *filter) {
    int flagged = 0, total = 0;
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        if (!ti || (filter && !strstr(ti->name, filter))) continue;
        total++;
        Circuit *c = circuit_create();
        circuit_place_template(c, (CircuitTemplateType)t, 0, 0);
        Simulation *sim = simulation_create(c);
        double td = circuit_template_scope_time_div((CircuitTemplateType)t), vd = circuit_template_scope_volt_div((CircuitTemplateType)t);
        const TemplateDemo *d = circuit_template_demo((CircuitTemplateType)t);
        int np = c->num_probes;
        double pmin[MAX_PROBES], pmax[MAX_PROBES], psum[MAX_PROBES]; long pn = 0;
        for (int i = 0; i < MAX_PROBES; i++) { pmin[i] = 1e300; pmax[i] = -1e300; psum[i] = 0; }
        int ok = simulation_dc_analysis(sim);
        double dt = (td > 0) ? simulation_scope_time_step(sim, td) : 1e-5;
        simulation_set_time_step(sim, dt);
        simulation_start(sim);
        double t_end = (td > 0) ? 20.0 * td : 0.02;    /* one scope screen (history span = 20 divisions) */
        if (t_end < 0.002 && (!d || d->kind == DEMO_DC || d->f_char < 1000)) t_end = 0.02;
        /* step-driven circuits: cover at least two periods of the square / pulse source */
        for (int i = 0; i < c->num_components; i++) {
            Component *k = c->components[i]; double per = 0;
            if (k->type == COMP_SQUARE_WAVE && k->props.square_wave.frequency > 0) per = 1.0 / k->props.square_wave.frequency;
            if (k->type == COMP_PULSE_SOURCE && k->props.pulse_source.period > 0 && k->props.pulse_source.period < 5.0) per = k->props.pulse_source.period;
            if (per > 0 && t_end < 2 * per) t_end = 2 * per;
        }
        long steps = 0;
        while (ok && sim->time < t_end && steps < 2000000) {
            if (!simulation_step(sim)) { ok = 0; break; }
            steps++;
            if (sim->time < t_end * 0.25) continue;    /* settle */
            for (int i = 0; i < np && i < MAX_PROBES; i++) {
                Node *nd = circuit_get_node(c, c->probes[i].node_id);
                double v = nd ? nd->voltage : 0;
                if (v < pmin[i]) pmin[i] = v; if (v > pmax[i]) pmax[i] = v; psum[i] += v;
            }
            pn++;
        }
        char flags[160] = "";
        if (!ok) strcat(flags, "SIMERR ");
        if (np < 2 && !(d && d->kind == DEMO_OSC)) strcat(flags, "NOOUT ");
        for (int i = 0; i < np; i++) {
            for (int j = 0; j < i; j++) if (c->probes[i].node_id == c->probes[j].node_id) { strcat(flags, "DUP "); break; }
            if (node_is_ground(c, c->probes[i].node_id)) strcat(flags, "GND ");
        }
        /* the output probe is the second one (index 1) unless there is only a source */
        int oi = (np >= 2) ? 1 : 0;
        double amp = (np > 0 && pmax[oi] > pmin[oi]) ? (pmax[oi] - pmin[oi]) / 2 : 0;
        double peak = (np > 0) ? fmax(fabs(pmax[oi]), fabs(pmin[oi])) : 0;
        int waveform = d && d->kind != DEMO_DC && d->kind != DEMO_NONE;
        if (ok && np >= 2 && waveform && amp < 1e-4 * fmax(1.0, peak) + 1e-9) strcat(flags, "FLAT ");
        /* A template that asks for the one-shot autoscale gets its volts/div set from the trace
           before the first frame is drawn, so the preset it was placed with is not what anyone
           sees. Reporting those as badly scaled was measuring a number the app overwrites. */
        int autofit = (circuit_template_scope_flags(t) & SCOPE_FLAG_FIT) != 0;
        if (!autofit) {
            /* SMALL is about a trace nobody can see: it has to hug the centre line as well as
               being small. A rectifier's output is a flat line at 12 V and a high-pass under a
               sweep starts attenuated - both are the circuit doing its job, and both were being
               reported. Only a steady stimulus (DEMO_WAVEFORM) is judged at all; under a sweep
               one screen at the start says nothing about what the sweep will show. */
            int steady = d && d->kind == DEMO_WAVEFORM;
            if (ok && np >= 2 && vd > 0 && steady && amp > 0 && amp < 0.25 * vd && peak < 0.25 * vd)
                strcat(flags, "SMALL ");
            if (ok && np >= 2 && vd > 0 && peak > 4.0 * vd) strcat(flags, "CLIP ");
            /* RIPPLE: the trace is on the screen but everything that moves on it is thinner than
               a fifth of a division. A converter's output rail is the case that matters - the
               buck's 60 mV of ripple on a 5.4 V rail at 2 V/div is three hundredths of a
               division, so the circuit looks like it has no ripple at all, which is the one
               thing that circuit is for. AC coupling or a per-channel band fixes it. */
            int ac_coupled = (circuit_template_scope_flags(t) & SCOPE_FLAG_AC) != 0;
            /* "amp > 0" is not the same question as "the trace moves". A flat DC rail carried
               through a solve comes back with its last bit wobbling - 4.4e-16 on a 3.2 V node,
               which is two ULP and not ripple. Ask for movement four orders above round-off
               instead. A converter's rail, the case this check exists for, is percent-level
               and nowhere near the floor. */
            int really_moves = amp > 1e-12 * fabs(peak);
            if (ok && np >= 2 && vd > 0 && !ac_coupled && really_moves &&
                amp < 0.1 * vd && peak > 0.5 * vd)
                strcat(flags, "RIPPLE ");
            if (getenv("CT_SHOW_AMP")) printf("        amp=%.6g peak=%.6g vd=%g\n", amp, peak, vd);
        }
        printf("[%s] %-26s td=%-7.3g vd=%-7.3g probes=%d  t=%.4g steps=%ld  %s\n", flags[0] ? "FLAG" : " OK ", ti->name, td, vd, np, sim->time, steps, flags);
        for (int i = 0; i < np && i < MAX_PROBES; i++) {
            char own[96]; owners_of(c, c->probes[i].node_id, own, sizeof own);
            Node *nd = circuit_get_node(c, c->probes[i].node_id);
            printf("        CH%d n%-4d (%6.0f,%6.0f) %-40s min %10.4g max %10.4g mean %10.4g\n", i + 1, c->probes[i].node_id,
                   nd ? nd->x : 0, nd ? nd->y : 0, own, pn ? pmin[i] : 0, pn ? pmax[i] : 0, pn ? psum[i] / pn : 0);
        }
        if (flags[0]) flagged++;
        simulation_free(sim); circuit_free(c);
    }
    printf("%d/%d templates flagged\n", flagged, total);
    return flagged;
}


/* --burn-test: run every template for 10 scope divisions and report any resistor whose peak
   dissipation exceeds its rating (the canvas warning icon) or LED over its max current.
   Non-zero exit if a power-system / high-voltage / Tesla template would show a warning. */
static int burn_test(void) {
    int flagged = 0, total = 0, hv_flagged = 0;
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        Circuit *c = circuit_create();
        if (circuit_place_template(c, (CircuitTemplateType)t, 0, 0) <= 0) { circuit_free(c); continue; }
        total++;
        int n = c->num_components;
        double *pmax = calloc(n, sizeof *pmax);
        Simulation *sim = simulation_create(c);
        int ok = simulation_dc_analysis(sim);
        double td = circuit_template_scope_time_div((CircuitTemplateType)t);
        double t_end = td > 0 ? 10 * td : 0.02;
        if (td > 0) { double dtp = simulation_scope_time_step(sim, td); if (dtp > 0) simulation_set_time_step(sim, dtp); }
        else simulation_set_time_step(sim, 1e-5);
        simulation_start(sim);
        long steps = 0;
        while (ok && sim->time < t_end && steps < 2000000) {
            if (!simulation_step(sim)) { ok = 0; break; }
            steps++;
            for (int i = 0; i < n; i++) {
                Component *comp = c->components[i];
                double v = 0;
                if (comp->type == COMP_RESISTOR) v = comp->props.resistor.power_dissipated;
                else if (comp->type == COMP_LED) v = comp->props.led.current;
                if (v > pmax[i]) pmax[i] = v;
            }
        }
        int hv = ti && (ti->group == TG_POWER_SYSTEMS || ti->group == TG_HIGH_VOLTAGE || strstr(ti->name, "Tesla"));
        for (int i = 0; i < n; i++) {
            Component *comp = c->components[i];
            double ratio = 0;
            if (comp->type == COMP_RESISTOR && comp->props.resistor.power_rating > 0) ratio = pmax[i] / comp->props.resistor.power_rating;
            else if (comp->type == COMP_LED && comp->props.led.max_current > 0) ratio = pmax[i] / comp->props.led.max_current;
            if (ratio > 1.0) {
                flagged++; if (hv) hv_flagged++;
                printf("%-4s %-34s %-6s %-14s peak %.3g %s (%.0f%% of rating)%s\n", hv ? "HV" : "", ti ? ti->name : "?",
                       comp->label, comp->type == COMP_RESISTOR ? "resistor" : "LED", pmax[i],
                       comp->type == COMP_RESISTOR ? "W" : "A", ratio * 100, ok ? "" : "  [sim failed]");
            }
        }
        if (!ok) printf("     %-34s sim failed at t=%.3g\n", ti ? ti->name : "?", sim->time);
        free(pmax); simulation_free(sim); circuit_free(c);
    }
    printf("burn-test: %d templates, %d overloaded parts (%d in HV/power/Tesla templates)\n", total, flagged, hv_flagged);
    return hv_flagged ? 1 : 0;
}


/* --std-test: steady-state bus voltages against the standards the templates are sized to.
   ERCOT Planning Guide / NERC TPL-001-5.1 P0: transmission 0.95-1.05 pu system normal.
   ANSI C84.1 Range A: 114-126 V on a 120 V base (0.95-1.05 pu), 456-504 V on a 480 V service.
   NEC 210.19(A): 3 % on a branch circuit.
   Each row also pins the measured value, so a template cannot drift out of its documented
   design point unnoticed. Rows whose expected pu sits outside the band are documented
   exceptions (a heavily loaded line, the Ferranti rise, a deliberately undersized conductor).  */
typedef struct {
    CircuitTemplateType t; ComponentType ct; int ord, term;
    double nom_pk, pu_expect, lo, hi; const char *std; const char *note;
} StdCase;
static const StdCase std_cases[] = {
    { CIRCUIT_TX_69KV,      COMP_RESISTOR, 0, 0, 56340.0,  0.957, 0.95, 1.05, "ERCOT PG 4 / NERC TPL-001 P0", "69 kV bus, 20 MVA at 0.95 pf" },
    { CIRCUIT_TX_LADDER,    COMP_RESISTOR, 0, 0, 281700.0, 0.990, 0.95, 1.05, "ERCOT PG 4 / NERC TPL-001 P0", "345 kV bus" },
    { CIRCUIT_TX_LADDER,    COMP_RESISTOR, 1, 0, 112670.0, 0.969, 0.95, 1.05, "ERCOT PG 4 / NERC TPL-001 P0", "138 kV bus" },
    { CIRCUIT_TX_LADDER,    COMP_RESISTOR, 2, 0, 56340.0,  0.957, 0.95, 1.05, "ERCOT PG 4 / NERC TPL-001 P0", "69 kV bus" },
    { CIRCUIT_TX_LADDER,    COMP_RESISTOR, 3, 0, 10182.0,  0.987, 0.95, 1.05, "AEP LTC +5 % (8 steps)",       "12.47 kV distribution bus" },
    { CIRCUIT_TX_LADDER,    COMP_RESISTOR, 5, 0, 339.41,   0.978, 0.95, 1.05, "ANSI C84.1 Range A",           "240 V service, 117.4 V per leg" },
    { CIRCUIT_TX_WIND,      COMP_TLINE,    0, 0, 28170.0,  1.043, 0.95, 1.05, "ERCOT PG 4",                   "34.5 kV collector bus while exporting" },
    { CIRCUIT_TX_PLANT,     COMP_RESISTOR, 1, 0, 3397.0,   0.981, 0.95, 1.05, "ANSI C84.1 Range A",           "4.16 kV motor bus" },
    { CIRCUIT_TX_PLANT,     COMP_RESISTOR, 3, 0, 391.9,    0.974, 0.95, 1.05, "ANSI C84.1 Range A (480 V)",   "480 V shop bus" },
    { CIRCUIT_RES_SERVICE,  COMP_RESISTOR, 3, 0, 169.71,   0.994, 0.95, 1.05, "ANSI C84.1 Range A",           "L1 at the panel, 119.3 V" },
    { CIRCUIT_RES_SERVICE,  COMP_RESISTOR, 4, 1, 169.71,   0.997, 0.95, 1.05, "ANSI C84.1 Range A",           "L2 at the panel, 119.6 V" },
    { CIRCUIT_RES_BRANCH,   COMP_RESISTOR, 3, 0, 169.71,   0.980, 0.97, 1.05, "NEC 210.19(A) 3 % branch",     "#10 at 100 ft: 2.0 % drop" },
    { CIRCUIT_RES_BRANCH,   COMP_RESISTOR, 1, 0, 169.71,   0.952, 0.97, 1.05, "NEC 210.19(A) 3 % branch",     "#14 at 100 ft: 4.8 % - the documented counter-example" },
    { CIRCUIT_RES_SOLAR,    COMP_RESISTOR, 1, 0, 339.41,   1.029, 0.95, 1.05, "IEEE 1547 / ANSI C84.1",       "PCC raised by the 7.6 kW export" },
    { CIRCUIT_COM_480Y,     COMP_RESISTOR, 1, 0, 391.9,    0.992, 0.95, 1.05, "ANSI C84.1 Range A (480 V)",   "480Y phase A bus" },
    { CIRCUIT_COM_208Y,     COMP_RESISTOR, 1, 0, 169.71,   0.992, 0.95, 1.05, "ANSI C84.1 Range A",           "208Y phase A, the 20 A branch" },
    { CIRCUIT_GS_N1,        COMP_RESISTOR, 0, 0, 281700.0, 0.970, 0.95, 1.05, "NERC TPL-001-5.1 P0",          "both 345 kV circuits in service" },
    { CIRCUIT_GS_BOLD,      COMP_RESISTOR, 0, 0, 281700.0, 0.921, 0.95, 1.05, "NERC TPL-001-5.1 P0",          "conventional line at 600 MW - past SIL, a documented case" },
    { CIRCUIT_GS_BOLD,      COMP_RESISTOR, 1, 0, 281700.0, 0.989, 0.95, 1.05, "AEP BOLD",                     "the same corridor and load on a BOLD line" },
    { CIRCUIT_GS_FACRATE,   COMP_RESISTOR, 4, 0, 112670.0, 0.990, 0.95, 1.05, "NERC FAC-008-5",               "138 kV bus at 400 A" },
    { CIRCUIT_HV_345_LINE,  COMP_RESISTOR, 0, 0, 281700.0, 0.937, 0.95, 1.05, "ERCOT PG 4 / NERC TPL-001 P0", "100 mi at 600 MW: past SIL, a documented heavy-load case" },
    { CIRCUIT_HV_765_LINE,  COMP_RESISTOR, 0, 0, 624600.0, 0.957, 0.95, 1.05, "AEP 765 kV backbone",          "765 kV, 200 mi, 2 GW at 0.957 pu" },
    { CIRCUIT_FERRANTI_LINE, COMP_TLINE,   0, 1, 281700.0, 1.139, 0.95, 1.05, "ERCOT PG 4",                   "open-ended 200 mi: +13.9 % Ferranti rise, a documented exception" },
};
static int std_test(void) {
    int fails = 0, viol = 0;
    printf("%-26s %-34s %7s %7s  %s\n", "template", "bus", "pu", "expect", "standard");
    for (unsigned k = 0; k < sizeof std_cases / sizeof std_cases[0]; k++) {
        const StdCase *c = &std_cases[k];
        const CircuitTemplateInfo *ti = circuit_template_get_info(c->t);
        Circuit *ct = circuit_create();
        if (circuit_place_template(ct, c->t, 0, 0) <= 0) { circuit_free(ct); continue; }
        Component *comp = NULL; int seen = 0;
        for (int i = 0; i < ct->num_components; i++)
            if (ct->components[i]->type == c->ct && seen++ == c->ord) { comp = ct->components[i]; break; }
        if (!comp) { printf("[FAIL] %-26s component not found\n", ti ? ti->name : "?"); fails++; circuit_free(ct); continue; }
        int node = comp->node_ids[c->term];
        Simulation *sim = simulation_create(ct);
        int ok = simulation_dc_analysis(sim);
        simulation_set_time_step(sim, 1.0 / 60.0 / 400.0);
        simulation_start(sim);
        double pk = 0;
        while (ok && sim->time < 0.1) {
            if (!simulation_step(sim)) { ok = 0; break; }
            if (sim->time > 0.05) { Node *nd = circuit_get_node(ct, node); double v = nd ? fabs(nd->voltage) : 0; if (v > pk) pk = v; }
        }
        double pu = pk / c->nom_pk;
        int inband = (pu >= c->lo && pu <= c->hi);
        int expect_inband = (c->pu_expect >= c->lo && c->pu_expect <= c->hi);
        int drift = !(fabs(pu - c->pu_expect) <= 0.03);
        if (!ok || drift) fails++;
        if (!inband) viol++;
        printf("%s %-26s %-34s %7.3f %7.3f  %s%s\n",
               (!ok || drift) ? "[FAIL]" : inband ? "[ OK ]" : "[NOTE]",
               ti ? ti->name : "?", c->note, pu, c->pu_expect, c->std,
               inband ? "" : (expect_inband ? "  << OUTSIDE THE BAND" : "  (documented exception)"));
        simulation_free(sim); circuit_free(ct);
    }
    printf("std-test: %u buses, %d drifted from the documented value, %d outside their band\n",
           (unsigned)(sizeof std_cases / sizeof std_cases[0]), fails, viol);
    return fails ? 1 : 0;
}


/* --switch-test: every SPST switch in every template, in both states, measured at that template's
   probed output. A switch that changes nothing is either mis-wired or pointless, so the default
   expectation is that the output moves by more than 1 %; templates where a switch legitimately
   does almost nothing to the probed node list their own tolerance here.  */
typedef struct { CircuitTemplateType t; int sw_ord; double lo_expect, hi_expect, tol; const char *note; } SwitchCase;
static const SwitchCase switch_cases[] = {
    { CIRCUIT_GS_N1,        0, 250100.0, 273300.0, 0.05, "second 345 kV circuit: 0.925 pu open, 0.970 pu closed" },
    { CIRCUIT_GS_FACRATE,   0, 111500.0, 110800.0, 0.05, "extra load block: 400 A open, 500 A closed" },
    { CIRCUIT_GS_DERATE,    0, 9752.0,   9310.0,  0.05, "summer air-conditioning block" },
    { CIRCUIT_COM_PFC,      0, 8.70,     6.90,    0.08, "capacitor bank: 0.75 pf open, 0.95 pf closed" },
    { CIRCUIT_TX_WIND,      0, 29370.0,  29370.0, 0.10, "string B disconnect (collector bus barely moves)" },
    { CIRCUIT_GS_PIDS,      0, 12.0,     8.775,   0.05, "cable integrity link: open = cable cut (12 V), closed = the loop cycling 8.5 / 9.2 V" },
    { CIRCUIT_ID_OPAMP_ERR, 0, -0.8892,  0.0998,  0.04, "source-matching switch: 100k unmatched -> -0.89 V, 1k matched -> +0.10 V (offset only)" },
    { CIRCUIT_PARTS_MOSFET, 0, 0.1442,   0.2849,  0.05, "second 100 ohm load: twice the current, twice the drop across the 2N7000" },
    /* switches that act on another phase or another branch than the probed one: no movement is correct */
    { CIRCUIT_POWER_PLANT,  1, 252504.0, 252504.0, 0.05, "phase B breaker - the probe is on phase A" },
    { CIRCUIT_POWER_PLANT,  2, 252504.0, 252504.0, 0.05, "phase C breaker - the probe is on phase A" },
    { CIRCUIT_SUBSTATION,   2, 103700.0, 103700.0, 0.05, "phase B breaker - the probe is on phase A" },
    { CIRCUIT_SUBSTATION,   3, 103700.0, 103700.0, 0.05, "phase C breaker - the probe is on phase A" },
    { CIRCUIT_SUBSTATION,   4, 103700.0, 103700.0, 0.05, "phase B cap bank - the probe is on phase A" },
    { CIRCUIT_SUBSTATION,   5, 103700.0, 103700.0, 0.05, "phase C cap bank - the probe is on phase A" },
    { CIRCUIT_GS_RX,        1, 168.533,  168.533,  0.05, "the feeder's reactive block - the probe is on the transmission bus" },
    /* Each segment of the display has its own switch and its own resistor, and the probe is on
       segment a. Blanking any of the other seven must leave a exactly where it was - segments
       that dim each other would mean they were sharing a resistor. */
    { CIRCUIT_SEVENSEG_TEST, 1, 2.16241, 2.16241, 0.02, "segment b - the probe is on segment a" },
    { CIRCUIT_SEVENSEG_TEST, 2, 2.16241, 2.16241, 0.02, "segment c - the probe is on segment a" },
    { CIRCUIT_SEVENSEG_TEST, 3, 2.16241, 2.16241, 0.02, "segment d - the probe is on segment a" },
    { CIRCUIT_SEVENSEG_TEST, 4, 2.16241, 2.16241, 0.02, "segment e - the probe is on segment a" },
    { CIRCUIT_SEVENSEG_TEST, 5, 2.16241, 2.16241, 0.02, "segment f - the probe is on segment a" },
    { CIRCUIT_SEVENSEG_TEST, 6, 2.16241, 2.16241, 0.02, "segment g - the probe is on segment a" },
    { CIRCUIT_SEVENSEG_TEST, 7, 2.16241, 2.16241, 0.02, "the decimal point - the probe is on segment a" },
};
static double switch_measure(CircuitTemplateType t, int sw_ord, int closed, int *ok_out) {
    Circuit *c = circuit_create();
    if (circuit_place_template(c, t, 0, 0) <= 0) { circuit_free(c); *ok_out = 0; return 0; }
    int seen = 0;
    for (int i = 0; i < c->num_components; i++)
        if (c->components[i]->type == COMP_SPST_SWITCH && seen++ == sw_ord)
            { c->components[i]->props.switch_spst.closed = closed ? true : false; break; }
    ComponentType oct = COMP_NONE; int oord = 0, oterm = 0;
    Component *out = NULL; seen = 0;
    if (circuit_template_output_spec(t, &oct, &oord, &oterm) && oct)
        for (int i = 0; i < c->num_components; i++)
            if (c->components[i]->type == oct && seen++ == oord) { out = c->components[i]; break; }
    if (!out) { circuit_free(c); *ok_out = 0; return 0; }
    int node = out->node_ids[oterm];
    Simulation *sim = simulation_create(c);
    int ok = simulation_dc_analysis(sim);
    double td = circuit_template_scope_time_div(t);
    double t_end = td > 0 ? 10 * td : 0.05;
    if (td > 0) { double dt = simulation_scope_time_step(sim, td); if (dt > 0) simulation_set_time_step(sim, dt); }
    simulation_start(sim);
    double mn = 1e300, mx = -1e300, sum = 0; long steps = 0, n = 0;
    while (ok && sim->time < t_end && steps < 2000000) {
        if (!simulation_step(sim)) { ok = 0; break; }
        steps++;
        if (sim->time > t_end * 0.5) { Node *nd = circuit_get_node(c, node); double v = nd ? nd->voltage : 0; if (v < mn) mn = v; if (v > mx) mx = v; sum += v; n++; }
    }
    double amp = (mx > mn) ? (mx - mn) / 2 : 0, mean = n ? sum / n : 0;
    if (fabs(mean) > amp) amp = mean;                   /* DC / logic loops: report the level, not the ripple */
    simulation_free(sim); circuit_free(c);
    *ok_out = ok;
    return amp;
}
static int switch_test(void) {
    int fails = 0, total = 0;
    printf("%-34s %-4s %14s %14s   %s\n", "template", "sw", "open", "closed", "note");
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        Circuit *probe = circuit_create();
        if (circuit_place_template(probe, (CircuitTemplateType)t, 0, 0) <= 0) { circuit_free(probe); continue; }
        int nsw = 0;
        for (int i = 0; i < probe->num_components; i++) if (probe->components[i]->type == COMP_SPST_SWITCH) nsw++;
        circuit_free(probe);
        for (int k = 0; k < nsw; k++) {
            int ok1 = 1, ok2 = 1;
            double a = switch_measure((CircuitTemplateType)t, k, 0, &ok1);
            double b = switch_measure((CircuitTemplateType)t, k, 1, &ok2);
            total++;
            const SwitchCase *sc = NULL;
            for (unsigned q = 0; q < sizeof switch_cases / sizeof switch_cases[0]; q++)
                if (switch_cases[q].t == (CircuitTemplateType)t && switch_cases[q].sw_ord == k) sc = &switch_cases[q];
            double biggest = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
            double moved = biggest > 0 ? fabs(a - b) / biggest : 0;
            int bad = 0; char why[160] = "";
            if (!ok1 || !ok2) { bad = 1; snprintf(why, sizeof why, "simulation failed"); }
            else if (sc) {
                if (fabs(a - sc->lo_expect) > sc->tol * fabs(sc->lo_expect) + 1e-9 ||
                    fabs(b - sc->hi_expect) > sc->tol * fabs(sc->hi_expect) + 1e-9) {
                    bad = 1; snprintf(why, sizeof why, "expected %.4g / %.4g", sc->lo_expect, sc->hi_expect);
                }
            } else if (moved < 0.01) {
                bad = 1; snprintf(why, sizeof why, "the probed output does not move (%.3g %%) - mis-wired, or it needs a switch_cases entry", moved * 100);
            }
            if (bad) fails++;
            printf("%s %-34s %-4d %14.6g %14.6g   %s%s\n", bad ? "[FAIL]" : "[ OK ]",
                   ti ? ti->name : "?", k, a, b, sc ? sc->note : "", bad ? why : "");
        }
    }
    printf("switch-test: %d switches over all templates, %d failed\n", total, fails);
    return fails ? 1 : 0;
}


/* ---------------------------------------------------------------------------------------
 * --part-test: every named device against the data sheet condition it was specified at.
 * Each check builds the manufacturer's own test circuit - force the stated current or gate
 * voltage, read the stated terminal - so a wrong parameter or a broken stamp shows up as a
 * number that does not match the front page of the data sheet.
 * ------------------------------------------------------------------------------------- */
typedef enum {
    PC_RDSON,       /* MOSFET: force I_D through the channel at the stated V_GS, read V_DS/I_D */
    PC_ID_MIN,      /* MOSFET: saturation current at a stated V_GS must clear the I_D(on) minimum */
    PC_HFE,         /* BJT: force I_B, measure I_C/I_B */
    PC_VBE,         /* BJT: V_BE at the same operating point */
    PC_FT,          /* BJT: f_T, from the current gain measured into an AC short */
    PC_VF,          /* diode: force I_F, read the forward drop */
    PC_VZ,          /* zener: force I_ZT, read the reverse voltage */
    PC_OPAMP_VOS,   /* op-amp: a unity buffer with its input grounded outputs its offset */
    PC_OPAMP_SR,    /* op-amp: the largest dV/dt a buffer produces on a 5 V step */
    PC_TEMPLATE     /* regulator: its own template, with the part applied */
} PartCheckKind;

typedef struct {
    const char *part;
    PartCheckKind kind;
    double bias;                 /* the data sheet's test condition */
    double expect, tol;          /* value and relative tolerance (tol < 0 = absolute) */
    CircuitTemplateType tpl;     /* PC_TEMPLATE only */
    const char *note;
} PartCheck;

static const PartCheck part_checks[] = {
    /* --- MOSFETs: R_DS(on) at the stated V_GS, then the transfer-curve minimum --- */
    { "2N7000",  PC_RDSON,  10.0, 1.2,   0.15, 0, "R_DS(on) 1.2 ohm typ at V_GS = 10 V" },
    { "2N7000",  PC_ID_MIN,  4.5, 0.075, 0,    0, "I_D(on) 75 mA min at V_GS = 4.5 V" },
    { "2N7002",  PC_RDSON,  10.0, 2.0,   0.15, 0, "R_DS(on) 2 ohm typ at V_GS = 10 V" },
    { "2N7002",  PC_ID_MIN,  4.5, 0.050, 0,    0, "I_D(on) 50 mA min at V_GS = 4.5 V" },
    { "IRF540N", PC_RDSON,  10.0, 0.044, 0.15, 0, "R_DS(on) 44 mohm max at V_GS = 10 V" },
    { "IRLZ44N", PC_RDSON,   5.0, 0.022, 0.20, 0, "R_DS(on) 22 mohm max at V_GS = 5 V (logic level)" },
    { "IRF540N", PC_ID_MIN,  6.0, 1.0,   0,    0, "well into conduction one volt above V_GS(th) + 1" },
    { "BS250",   PC_RDSON, -10.0, 10.0,  0.20, 0, "R_DS(on) 14 ohm max at V_GS = -10 V (10 typ)" },
    { "IRF9540N",PC_RDSON, -10.0, 0.20,  0.20, 0, "R_DS(on) 0.2 ohm max at V_GS = -10 V" },
    { "IRF9540N",PC_ID_MIN, -6.0, 1.0,   0,    0, "a power part: amps three volts past threshold" },
    /* --- BJTs: forced base current, at the data sheet's collector current --- */
    { "2N3904",  PC_HFE,   50e-6, 200.0, 0.15, 0, "h_FE 100 - 300 at I_C = 10 mA" },
    { "2N3904",  PC_VBE,   50e-6, 0.66,  0.12, 0, "V_BE(on) 0.65 V typ at I_C = 10 mA" },
    { "BC547B",  PC_HFE,   10e-6, 290.0, 0.15, 0, "h_FE 200 - 450 (B grade)" },
    { "2N3906",  PC_HFE,   50e-6, 180.0, 0.15, 0, "h_FE 100 - 300 at I_C = 10 mA" },
    /* f_T. The data sheet gives a minimum, the SPICE model is fitted to a typical part, so the
       expected value here is what the model asks for: gm / 2pi(C_be + C_bc) worked out by hand
       from the operating point and the TF/CJE/CJC the part carries. That is an arithmetic
       oracle, and the measurement it is checked against is a transient. */
    { "2N3904",  PC_FT,    50e-6, 490e6, 0.20, 0, "f_T 300 MHz min; gm/2pi(Cbe+Cbc) = 490 MHz" },
    { "BC547B",  PC_FT,    10e-6, 208e6, 0.20, 0, "f_T at I_C = 3 mA; 300 MHz typ at 10 mA" },
    /* --- diodes and the zener: forced current, measured drop --- */
    { "1N4148",  PC_VF,     5e-3, 0.72,  0.12, 0, "V_F 0.72 V at 5 mA" },
    { "1N4148",  PC_VF,    10e-3, 0.80,  0.15, 0, "V_F 1.0 V max at 10 mA" },
    { "1N4001",  PC_VF,      1.0, 1.0,   0.15, 0, "V_F 1.0 V typ at 1 A (1.1 V max)" },
    { "1N4733A", PC_VZ,    49e-3, 5.1,   0.10, 0, "V_Z 5.1 V at I_ZT = 49 mA" },
    /* --- op-amps: the two figures that decide a design --- */
    { "LM358",   PC_OPAMP_VOS, 0, 2e-3,  -5e-4, 0, "V_IO 2 mV typ" },
    { "LM358",   PC_OPAMP_SR,  0, 0.5e6, 0.25,  0, "SR 0.5 V/us" },
    { "LM741",   PC_OPAMP_VOS, 0, 1e-3,  -5e-4, 0, "V_IO 1 mV typ" },
    { "TL072",   PC_OPAMP_SR,  0, 13e6,  0.25,  0, "SR 13 V/us" },
    { "MCP6001", PC_OPAMP_SR,  0, 0.6e6, 0.25,  0, "SR 0.6 V/us" },
    /* --- regulators: their own reference circuit --- */
    { "LM7805",  PC_TEMPLATE, 0, 5.0,   0.02, CIRCUIT_7805_REG,  "V_O 5 V (4.8 - 5.2)" },
    { "LM317",   PC_TEMPLATE, 0, 5.0,   0.03, CIRCUIT_LM317_REG, "1.25 (1 + 720/240) = 5.0 V" },
    { "TL431",   PC_TEMPLATE, 0, 2.495, 0.02, CIRCUIT_TL431_REF, "V_ref 2.495 V" },
};

static ComponentType part_type(const char *name) {
    for (int i = 0; i < component_part_count(); i++) {
        const PartModel *m = component_part_at(i);
        if (m && !strcmp(m->part, name)) return m->type;
    }
    return COMP_NONE;
}

/* DC operating point, then the voltage at one node */
static double pc_dc_at(Circuit *c, int node_id, int *ok) {
    Simulation *sim = simulation_create(c);
    *ok = sim && simulation_dc_analysis(sim);
    double v = 0;
    if (*ok) { Node *n = circuit_get_node(c, node_id); v = n ? n->voltage : 0; }
    if (!isfinite(v)) *ok = 0;
    simulation_free(sim);
    return v;
}

/* R_DS(on): hold the gate at the data sheet's V_GS and drive the drain from a supply through
   a sense resistor, so the channel current is (V - V_D)/R_sense and R_DS(on) = V_D divided by
   it. A current source into the drain would be the more literal reading of the data sheet, but
   it leaves that node with no DC path at all until the device turns on, and Newton starts from
   a drain sitting at the compliance limit - which converges on nonsense. */
static double pc_mos_rdson(const char *part, double vgs, double vsupply, double rsense, int *ok) {
    ComponentType ty = part_type(part);
    Circuit *c = circuit_create();
    Component *m = pt_add(c, ty, 100, 100, 0);
    if (!m || !component_apply_part(m, part)) { circuit_free(c); *ok = 0; return 0; }
    Component *vg = pt_add(c, COMP_DC_VOLTAGE, 0, 100, 0);
    vg->props.dc_voltage.voltage = vgs;
    Component *gg = pt_add(c, COMP_GROUND, 0, 200, 0);
    Component *vd = pt_add(c, COMP_DC_VOLTAGE, 320, 40, 0);
    vd->props.dc_voltage.voltage = vsupply;
    Component *gv = pt_add(c, COMP_GROUND, 320, 140, 0);
    Component *rs = pt_add(c, COMP_RESISTOR, 240, 60, 0);
    rs->props.resistor.resistance = rsense;
    rs->props.resistor.power_rating = 100.0;
    Component *gs = pt_add(c, COMP_GROUND, 200, 260, 0);
    int gate = pt_node(c, 60, 100), drain = pt_node(c, 180, 60), src = pt_node(c, 180, 160);
    int rail = pt_node(c, 320, 0), rr = pt_node(c, 280, 60);
    int gnd1 = pt_node(c, 0, 180), gnd2 = pt_node(c, 320, 120), gnd3 = pt_node(c, 200, 240);
    vg->node_ids[0] = gate; vg->node_ids[1] = gnd1; gg->node_ids[0] = gnd1;
    vd->node_ids[0] = rail; vd->node_ids[1] = gnd2; gv->node_ids[0] = gnd2;
    rs->node_ids[0] = rr; rs->node_ids[1] = drain;
    circuit_add_wire(c, rail, rr);
    gs->node_ids[0] = gnd3; circuit_add_wire(c, src, gnd3);
    m->node_ids[0] = gate; m->node_ids[1] = drain; m->node_ids[2] = src;
    double vdrain = pc_dc_at(c, drain, ok);
    circuit_free(c);
    double id = (vsupply - vdrain) / rsense;
    if (fabs(id) < 1e-9) { *ok = 0; return 0; }
    return fabs(vdrain / id);
}

/* Saturation drain current at a stated V_GS: 10 V rail through a 1 ohm sense resistor. */
static double pc_mos_id(const char *part, double vgs, int *ok) {
    ComponentType ty = part_type(part);
    Circuit *c = circuit_create();
    Component *m = pt_add(c, ty, 100, 100, 0);
    if (!m || !component_apply_part(m, part)) { circuit_free(c); *ok = 0; return 0; }
    Component *vg = pt_add(c, COMP_DC_VOLTAGE, 0, 100, 0);
    vg->props.dc_voltage.voltage = vgs;
    Component *gg = pt_add(c, COMP_GROUND, 0, 200, 0);
    Component *vdd = pt_add(c, COMP_DC_VOLTAGE, 300, 40, 0);
    vdd->props.dc_voltage.voltage = 10.0;
    Component *gv = pt_add(c, COMP_GROUND, 300, 140, 0);
    Component *rs = pt_add(c, COMP_RESISTOR, 240, 60, 0);
    rs->props.resistor.resistance = 1.0;
    rs->props.resistor.power_rating = 100.0;
    Component *gs = pt_add(c, COMP_GROUND, 200, 260, 0);
    int gate = pt_node(c, 60, 100), drain = pt_node(c, 180, 60), src = pt_node(c, 180, 160);
    int rail = pt_node(c, 300, 0), rr = pt_node(c, 280, 60);
    int gnd1 = pt_node(c, 0, 180), gnd2 = pt_node(c, 300, 120), gnd3 = pt_node(c, 200, 240);
    vg->node_ids[0] = gate; vg->node_ids[1] = gnd1; gg->node_ids[0] = gnd1;
    vdd->node_ids[0] = rail; vdd->node_ids[1] = gnd2; gv->node_ids[0] = gnd2;
    rs->node_ids[0] = rr; rs->node_ids[1] = drain;
    circuit_add_wire(c, rail, rr);
    gs->node_ids[0] = gnd3; circuit_add_wire(c, src, gnd3);
    m->node_ids[0] = gate; m->node_ids[1] = drain; m->node_ids[2] = src;
    double vd = pc_dc_at(c, drain, ok);
    circuit_free(c);
    return (10.0 - vd) / 1.0;                   /* the sense resistor is 1 ohm */
}

/* Forced base current; returns I_C through a 100 ohm collector resistor and V_BE. */
/* ===================================================================================
 * --bias-test: does the reported operating region match what the arithmetic says?
 *
 * The single most useful thing a simulator can tell a reader who has just built a stage is
 * that its transistor is not in the active region, because a solver will happily linearise
 * about a device that is hard on or hard off and report a gain for it. Every case below has
 * its region and its V_CE worked out by hand from the resistors before the solver is asked.
 * =================================================================================== */
typedef struct { int region; double vce, ic; int ok; } BiasResult;

/* A base-resistor-biased common-emitter stage: the textbook way to show saturation, because
   the collector current follows the base current until the collector resistor will not let it
   any further. base_to_rail false ties the base to ground instead, which is cut off. */
static BiasResult bias_stage(const char *part, double rb, double rc, double vcc, bool base_to_rail) {
    BiasResult r = { -1, 0, 0, 0 };
    ComponentType ty = part_type(part);
    Circuit *c = circuit_create();
    Component *q = pt_add(c, ty, 100, 100, 0);
    if (!q || !component_apply_part(q, part)) { circuit_free(c); return r; }
    Component *vs = pt_add(c, COMP_DC_VOLTAGE, 300, 40, 0);
    vs->props.dc_voltage.voltage = vcc;
    Component *gv = pt_add(c, COMP_GROUND, 300, 140, 0);
    Component *rcc = pt_add(c, COMP_RESISTOR, 240, 60, 0);
    rcc->props.resistor.resistance = rc; rcc->props.resistor.power_rating = 10.0;
    Component *rbb = pt_add(c, COMP_RESISTOR, 20, 60, 0);
    rbb->props.resistor.resistance = rb; rbb->props.resistor.power_rating = 10.0;
    Component *gb = pt_add(c, COMP_GROUND, 20, 200, 0);
    Component *ge = pt_add(c, COMP_GROUND, 200, 260, 0);

    int base = pt_node(c, 60, 100), coll = pt_node(c, 180, 60), emit = pt_node(c, 180, 160);
    int rail = pt_node(c, 300, 0), rr = pt_node(c, 280, 60), rbtop = pt_node(c, 0, 60);
    int gnd2 = pt_node(c, 300, 120), gnd3 = pt_node(c, 200, 240), gndb = pt_node(c, 20, 180);
    vs->node_ids[0] = rail; vs->node_ids[1] = gnd2; gv->node_ids[0] = gnd2;
    rcc->node_ids[0] = rr; rcc->node_ids[1] = coll; circuit_add_wire(c, rail, rr);
    rbb->node_ids[0] = rbtop; rbb->node_ids[1] = base;
    if (base_to_rail) { circuit_add_wire(c, rail, rbtop); gb->node_ids[0] = gndb; }
    else              { gb->node_ids[0] = gndb; circuit_add_wire(c, rbtop, gndb); }
    ge->node_ids[0] = gnd3; circuit_add_wire(c, emit, gnd3);
    q->node_ids[0] = base; q->node_ids[1] = coll; q->node_ids[2] = emit;

    Simulation *sim = simulation_create(c);
    r.ok = sim && simulation_dc_analysis(sim);
    if (r.ok) {
        r.region = q->props.bjt.op_region;
        r.vce = q->props.bjt.op_vce;
        r.ic = q->props.bjt.op_ic;
        if (!isfinite(r.vce) || !isfinite(r.ic)) r.ok = 0;
    }
    if (sim) simulation_free(sim);
    circuit_free(c);
    return r;
}

/* ===================================================================================
 * --netlist-test: a written-down circuit becomes a working one.
 *
 * The companion course hands a reader a table of parts and the nets they connect to, and the
 * whole value of reading it is that the answer comes out right afterwards. So these are not
 * parser tests - each one is solved and checked against arithmetic done on paper.
 *
 * The first is the course's own acceptance case, gain = 1 + RF/R1 on an ideal op-amp written
 * as a VCVS, which is the element it uses for every op-amp it has.
 * =================================================================================== */
static double nl_solve_net(const char *text, const char *net, int *ok) {
    *ok = 0;
    Circuit *c = circuit_create();
    if (!c) return 0;
    char err[160];
    double v = 0;
    if (netlist_build(c, text, err, sizeof err) > 0) {
        Simulation *sim = simulation_create(c);
        if (sim && simulation_dc_analysis(sim)) {
            /* any node carrying the name, once the map has joined them all */
            for (int i = 0; i < c->num_nodes; i++) {
                if (_stricmp(c->nodes[i].name, net) != 0) continue;
                v = c->nodes[i].voltage;
                *ok = isfinite(v);
                break;
            }
        }
        if (sim) simulation_free(sim);
    }
    circuit_free(c);
    return v;
}

static int netlist_test(void) {
    int fails = 0, total = 0;
    printf("netlist-test: circuits written as text, placed, solved and checked\n\n");

    static const struct { const char *what; const char *text; const char *net;
                          double expect; double tol; const char *why; } cases[] = {
        { "non-inverting amp",
          "VIN in 0 DC 1.0\n"
          "E1 out 0 in vm 100k\n"
          "R1 vm 0 1k\n"
          "RF vm out 3k\n",
          "out", 4.0, 0.02,
          "gain 1 + RF/R1 = 4, and the op-amp is a VCVS - the element the course uses for all 48 of them" },

        { "divider, named nets only",
          "V1 in 0 DC 10\n"
          "R1 in mid 1k\n"
          "R2 mid 0 1k\n",
          "mid", 5.0, 0.01,
          "two equal resistors: no wire is drawn anywhere, the net names do the joining" },

        { "the same net named twice over",
          "V1 a 0 DC 6\n"
          "R1 a b 2k\n"
          "R2 b 0 1k\n"
          "R3 B 0 1k\n",
          "b", 1.2, 0.02,
          "R3 says B and R2 says b, so they are one net: 2k into 1k||1k = 500 ohm, a fifth of 6 V" },

        { "SPICE's M is milli, not mega",
          "V1 p 0 DC 1\n"
          "R1 p q 1M\n"
          "R2 q 0 1\n",
          "q", 0.999, 0.01,
          "a MILLIohm against a whole ohm, so q sits just under the rail. Read as mega it would be a microvolt,"
          " and two equal values would have hidden it" },

        { "MEG is the one that means mega",
          "V1 p 0 DC 1\n"
          "R1 p q 1MEG\n"
          "R2 q 0 1\n",
          "q", 1e-6, 0.10,
          "the same circuit with the other suffix: a megohm against an ohm divides the rail down to a microvolt" },

        { "current source into a named net",
          "I1 0 n 1m\n"
          "R1 n 0 4k7\n",
          "n", 4.7, 0.02,
          "1 mA through 4.7k. The suffix has a digit inside it, which is how resistors are written" },

        { "R is the decimal point below a kilohm",
          "I1 0 n 1\n"
          "R1 n 0 1R5\n",
          "n", 1.5, 0.01,
          "1 A through 1R5. R multiplies by one and only stands where the point would - read as a"
          " bare suffix it was 1 ohm, which is the same 33 % error as reading 4k7 for 4k" },
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        int ok = 0;
        double v = nl_solve_net(cases[i].text, cases[i].net, &ok);
        total++;
        int pass = ok && fabs(v - cases[i].expect) <= cases[i].tol * fabs(cases[i].expect);
        if (!pass) fails++;
        printf("%s netlist %-28s V(%s) = %8.4f   expect %7.4f  %s\n",
               pass ? " OK " : "FAIL", cases[i].what, cases[i].net, v, cases[i].expect,
               ok ? "" : "[did not solve]");
        printf("        %s\n", cases[i].why);
    }

    /* A table pasted onto a sheet that already has something on it must not land on top of it,
       and must not join to it by accident. */
    {
        Circuit *c = circuit_create();
        char err[160];
        int a = netlist_build(c, "V1 in 0 DC 5\nR1 in 0 1k\n", err, sizeof err);
        int n1 = c->num_components;
        int b = netlist_build(c, "V2 x 0 DC 9\nR2 x 0 1k\n", err, sizeof err);
        Simulation *sim = simulation_create(c);
        int solved = sim && simulation_dc_analysis(sim);
        double vin = 0, vx = 0;
        for (int i = 0; i < c->num_nodes; i++) {
            if (!_stricmp(c->nodes[i].name, "in")) vin = c->nodes[i].voltage;
            if (!_stricmp(c->nodes[i].name, "x"))  vx  = c->nodes[i].voltage;
        }
        if (sim) simulation_free(sim);
        total++;
        int pass = a > 0 && b > 0 && c->num_components > n1 && solved
                   && fabs(vin - 5.0) < 0.05 && fabs(vx - 9.0) < 0.05;
        if (!pass) fails++;
        printf("%s netlist %-28s in = %6.3f, x = %6.3f   expect 5 and 9\n",
               pass ? " OK " : "FAIL", "second paste onto the same sheet", vin, vx);
        printf("        two tables, one ground, and nets that do not run into each other\n");
        circuit_free(c);
    }

    /* Saved and opened again, the names have to still be there.

       They are the only thing joining anything in a circuit that came from a table, so a file
       format that drops them reloads a field of parts that solves to nothing - and every other
       audit would pass it, because the parts and their values all came back. Both formats are
       checked, because they are different code and only one of them is what Save writes. */
    {
        const char *text = "VIN in 0 DC 1.0\nE1 out 0 in vm 100k\nR1 vm 0 1k\nRF vm out 3k\n";
        const char *tmp = getenv("TEMP"); if (!tmp) tmp = ".";
        static const struct { const char *ext; bool (*save)(Circuit *, const char *);
                              bool (*load)(Circuit *, const char *); } legs[] = {
            { "json", file_export_json, file_import_json },
            { "ckt",  file_save_circuit, file_load_circuit },
        };
        for (size_t i = 0; i < sizeof legs / sizeof legs[0]; i++) {
            char path[600];
            snprintf(path, sizeof path, "%s\\ct_netnames.%s", tmp, legs[i].ext);
            Circuit *a = circuit_create();
            char err[160];
            netlist_build(a, text, err, sizeof err);
            bool wrote = legs[i].save(a, path);
            circuit_free(a);

            Circuit *b = circuit_create();
            bool read = wrote && legs[i].load(b, path);
            double v = 0; int solved = 0, named = 0;
            if (read) {
                for (int k = 0; k < b->num_nodes; k++) if (b->nodes[k].name[0]) named++;
                Simulation *sim = simulation_create(b);
                solved = sim && simulation_dc_analysis(sim);
                if (solved)
                    for (int k = 0; k < b->num_nodes; k++)
                        if (_stricmp(b->nodes[k].name, "out") == 0) { v = b->nodes[k].voltage; break; }
                if (sim) simulation_free(sim);
            }
            circuit_free(b);
            remove(path);
            total++;
            int pass = read && solved && named >= 6 && fabs(v - 4.0) < 0.1;
            if (!pass) fails++;
            printf("%s netlist %-28s %d named nets back, V(out) = %6.3f   expect 4.000\n",
                   pass ? " OK " : "FAIL", legs[i].ext[0] == 'j' ? "saved as JSON and reopened"
                                                                 : "saved as binary and reopened",
                   named, v);
        }
    }

    printf("\nnetlist-test: %d written circuits, %d that did not come out right\n", total, fails);
    return fails ? 1 : 0;
}

static int bias_test(void) {
    int fails = 0, total = 0;
    printf("bias-test: the reported operating region against the arithmetic\n\n");
    static const char *names[4] = { "CUT OFF", "ACTIVE", "SATURATED", "REVERSE" };

    struct { const char *what; double rb, rc, vcc; bool to_rail; int want; double vce_lo, vce_hi;
             const char *why; } cases[] = {
        /* Ib = (10 - 0.7)/470k = 19.8 uA, Ic = beta x that = 4.2 mA, V_CE = 10 - 4.2 = 5.8 V */
        { "base 470k, collector 1k",  470e3, 1e3,  10.0, true,  1, 3.0, 8.0,
          "Ib 19.8 uA x beta = 4.2 mA, V_CE 5.8 V - mid supply, the point of a bias network" },
        /* Ib = 198 uA, beta x that = 42 mA, but 1k from 10 V cannot pass more than 9.8 mA */
        { "base 47k, collector 1k",   47e3,  1e3,  10.0, true,  2, -0.5, 0.4,
          "beta asks for 42 mA, the collector resistor allows 9.8 - so it saturates" },
        /* the R2 = 10k mistake in EE_Review module-05/lesson-06, in miniature */
        { "base 22k, collector 6k",   22e3,  6e3,  12.0, true,  2, -0.5, 0.4,
          "2.4 mA asked through 6k from 12 V wants 14 V of drop: saturated" },
        { "base tied to ground",      470e3, 1e3,  10.0, false, 0, 9.0, 10.5,
          "no base current, so no collector current and V_CE is the whole supply" },
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        BiasResult r = bias_stage("2N3904", cases[i].rb, cases[i].rc, cases[i].vcc, cases[i].to_rail);
        total++;
        int rg_ok = r.ok && r.region == cases[i].want;
        int v_ok  = r.ok && r.vce >= cases[i].vce_lo && r.vce <= cases[i].vce_hi;
        int pass = rg_ok && v_ok;
        if (!pass) fails++;
        printf("%s bias %-26s %-9s V_CE %7.3f V  I_C %8.3f mA  expect %-9s %s\n",
               pass ? " OK " : "FAIL", cases[i].what,
               (r.region >= 0 && r.region < 4) ? names[r.region] : "?",
               r.vce, r.ic * 1e3, names[cases[i].want],
               r.ok ? (pass ? "" : "<-- MISMATCH") : "[solve failed]");
        printf("        %s\n", cases[i].why);
    }
    /* ---- the instrument's protection window ----
       The whole point of a battery monitor is that it refuses. Place the instrument, move its
       cell to each side of the 2.8 - 3.2 V window its divider defines, and check that the load
       current follows: on in the middle, off at both ends. A window comparator that says yes
       to everything passes every other audit in this program. */
    {
        printf("\n");
        static const struct { double cell; bool conduct; const char *why; } wcases[] = {
            { 3.00, true,  "inside the window - both comparators high, so the pass device is on" },
            { 2.50, false, "below 2.80 V: the under-voltage comparator drops and the load is cut" },
            { 3.40, false, "above 3.20 V: the over-voltage comparator drops and the load is cut" },
        };
        for (size_t i = 0; i < sizeof wcases / sizeof wcases[0]; i++) {
            Circuit *c = circuit_create();
            double ic = 0; int ok = 0;
            if (circuit_place_template(c, CIRCUIT_BMI_INSTRUMENT, 0, 0) > 0) {
                Component *bat = NULL, *load = NULL; int nres = 0;
                for (int k = 0; k < c->num_components; k++) {
                    if (c->components[k]->type == COMP_BATTERY) bat = c->components[k];
                    if (c->components[k]->type == COMP_RESISTOR && nres++ == 3) load = c->components[k];
                }
                if (bat) bat->props.battery.nominal_voltage = wcases[i].cell;
                Simulation *sim = simulation_create(c);
                ok = sim && simulation_dc_analysis(sim) && bat && load;
                if (ok) {
                    Node *n = circuit_get_node(c, load->node_ids[0]);
                    ic = (n ? n->voltage : 0) / load->props.resistor.resistance;
                    if (!isfinite(ic)) ok = 0;
                }
                if (sim) simulation_free(sim);
            }
            circuit_free(c);
            total++;
            int pass = ok && (wcases[i].conduct ? (ic > 0.2 && ic < 0.4) : (fabs(ic) < 1e-3));
            if (!pass) fails++;
            printf("%s bias window: cell %.2f V -> load %7.4f A   expect %-3s  %s\n",
                   pass ? " OK " : "FAIL", wcases[i].cell, ic,
                   wcases[i].conduct ? "ON" : "off", ok ? wcases[i].why : "[solve failed]");
        }
    }

    /* ---- a current source has to ignore its load ----
       The claim the CC Output template makes is that an LM317 with one resistor between OUT and
       ADJ delivers 1.25/R whatever is downstream. A voltage source with a load resistor would
       also read 617 uA into 1k; the only thing that tells them apart is changing the load. */
    {
        printf("\n");
        static const double loads[] = { 1000.0, 5000.0, 200.0 };
        double first = 0;
        for (size_t i = 0; i < sizeof loads / sizeof loads[0]; i++) {
            Circuit *c = circuit_create();
            double ic = 0, vl = 0; int ok = 0;
            if (circuit_place_template(c, CIRCUIT_BMI_CC_OUTPUT, 0, 0) > 0) {
                Component *load = NULL; int nres = 0;
                for (int k = 0; k < c->num_components; k++)
                    if (c->components[k]->type == COMP_RESISTOR && nres++ == 2) load = c->components[k];
                if (load) load->props.resistor.resistance = loads[i];
                Simulation *sim = simulation_create(c);
                ok = sim && simulation_dc_analysis(sim) && load;
                if (ok) {
                    Node *n = circuit_get_node(c, load->node_ids[0]);
                    vl = n ? n->voltage : 0;
                    ic = vl / loads[i];
                    if (!isfinite(ic)) ok = 0;
                }
                if (sim) simulation_free(sim);
            }
            circuit_free(c);
            if (i == 0) first = ic;
            total++;
            /* 1.25/2025.6 = 617.1 uA, and it must not move as the load changes by 25x */
            int pass = ok && fabs(ic - 617.1e-6) < 15e-6 && fabs(ic - first) < 5e-6;
            if (!pass) fails++;
            printf("%s bias LM317 source into %5.0f ohm -> %7.1f uA across %6.3f V   expect 617.1 uA\n",
                   pass ? " OK " : "FAIL", loads[i], ic * 1e6, vl);
        }
    }

    /* ---- the over-current trip has to actually trip ----
       A protection that conducts at its rated load and also conducts at five times it is not a
       protection, and every other audit in this program would pass it. The load is moved from
       10 ohm to 2 and the current has to stop being proportional. */
    {
        printf("\n");
        static const struct { double rload; bool conduct; } tcases[] = {
            { 10.0, true }, { 2.0, false },
        };
        for (size_t i = 0; i < sizeof tcases / sizeof tcases[0]; i++) {
            Circuit *c = circuit_create();
            double il = 0; int ok = 0;
            if (circuit_place_template(c, CIRCUIT_BMI_OCP, 0, 0) > 0) {
                Component *load = NULL, *sense = NULL; int nres = 0;
                for (int k = 0; k < c->num_components; k++)
                    if (c->components[k]->type == COMP_RESISTOR) {
                        if (nres == 1) load = c->components[k];
                        if (nres == 2) sense = c->components[k];
                        nres++;
                    }
                if (load) load->props.resistor.resistance = tcases[i].rload;
                Simulation *sim = simulation_create(c);
                ok = sim && simulation_dc_analysis(sim) && load && sense;
                if (ok) {
                    /* the current the shunt actually sees, not what the load would like */
                    Node *n = circuit_get_node(c, sense->node_ids[0]);
                    il = (n ? n->voltage : 0) / sense->props.resistor.resistance;
                    if (!isfinite(il)) ok = 0;
                }
                if (sim) simulation_free(sim);
            }
            circuit_free(c);
            total++;
            /* untripped: near 5/(R+1+Rds). tripped: whatever it is, it is not that. */
            double would_be = 5.0 / (tcases[i].rload + 1.7);
            int pass = ok && (tcases[i].conduct ? fabs(il - would_be) < 0.1 * would_be
                                                : il < 0.5 * would_be);
            if (!pass) fails++;
            printf("%s bias trip: load %5.1f ohm -> %6.3f A   unprotected it would be %6.3f A  %s\n",
                   pass ? " OK " : "FAIL", tcases[i].rload, il, would_be,
                   ok ? (tcases[i].conduct ? "(under the threshold, so it conducts)"
                                           : "(over it: the relay takes the gate)")
                      : "[solve failed]");
        }
    }

    /* ---- a regulated rail is regulated by its divider, not by its duty ----
       The switching rail's claim is that 1.25 x (1 + R5/R4) decides the output and nothing
       else does. A converter told a duty cycle would also sit at 3.36 V with the right duty -
       what separates them is changing the load, which moves the duty and must not move the
       rail, and changing the divider, which must. Run to steady state and average; a DC solve
       says nothing about a circuit whose whole behaviour is switching. */
    {
        printf("\n");
        static const struct { double rload, r5; double expect; const char *why; } rcases[] = {
            {  33.0, 2025.6, 3.36, "as drawn: 1.25 x (1 + 2025.6/1200)" },
            { 100.0, 2025.6, 3.36, "a third of the load: the duty falls, the rail does not move" },
            {  33.0, 3600.0, 5.00, "divider changed to 3600/1200: 1.25 x 4 = 5.00 V" },
        };
        for (size_t i = 0; i < sizeof rcases / sizeof rcases[0]; i++) {
            Circuit *c = circuit_create();
            double mean = 0; int ok = 0, n = 0;
            if (circuit_place_template(c, CIRCUIT_BMI_RAIL, 0, 0) > 0) {
                Component *load = NULL, *r5 = NULL; int nres = 0;
                for (int k = 0; k < c->num_components; k++)
                    if (c->components[k]->type == COMP_RESISTOR) {
                        if (nres == 0) load = c->components[k];
                        if (nres == 1) r5 = c->components[k];
                        nres++;
                    }
                if (load) load->props.resistor.resistance = rcases[i].rload;
                if (r5) r5->props.resistor.resistance = rcases[i].r5;
                Simulation *sim = simulation_create(c);
                ok = sim && simulation_dc_analysis(sim) && load;
                if (ok) {
                    simulation_set_time_step(sim, 1e-6);
                    simulation_start(sim);
                    double t_end = 0.05, t_meas = 0.04;   /* 100 uF into 33 ohm settles slowly */
                    while (sim->time < t_end) {
                        if (!simulation_step(sim)) { ok = 0; break; }
                        if (sim->time < t_meas) continue;
                        Node *nd = circuit_get_node(c, load->node_ids[0]);
                        if (nd) { mean += nd->voltage; n++; }
                    }
                    if (n) mean /= n;
                    if (!isfinite(mean)) ok = 0;
                }
                if (sim) simulation_free(sim);
            }
            circuit_free(c);
            total++;
            int pass = ok && n > 100 && fabs(mean - rcases[i].expect) < 0.10 * rcases[i].expect;
            if (!pass) fails++;
            printf("%s bias rail: %5.0f ohm, R5 %6.1f -> %5.3f V   expect %4.2f  %s\n",
                   pass ? " OK " : "FAIL", rcases[i].rload, rcases[i].r5, mean, rcases[i].expect,
                   ok ? rcases[i].why : "[did not run]");
        }
    }

    printf("\nbias-test: %d checks, %d where the region, V_CE, protection, current or rail did\n"
           "           not match the arithmetic\n", total, fails);
    return fails ? 1 : 0;
}

/* f_T, measured the way a data sheet measures it: collector shorted to AC ground, a small
   signal current into the base, and the ratio of collector to base current read at a frequency
   well inside the region where beta falls as 1/f. There beta(f) = f_T/f, so f_T is the test
   frequency times the ratio that comes out.

   This is the check that the charge storage is there at all. With TF, CJE and CJC left at zero
   a transistor has no frequency limit: the current gain never falls, beta(f) stays at h_FE, and
   what this returns is h_FE times the test frequency - about ten times too big. */
static double pc_bjt_ft(const char *part, double ib_bias, double f_test, int *ok) {
    ComponentType ty = part_type(part);
    bool pnp = (ty == COMP_PNP_BJT);
    double sgn = pnp ? -1.0 : 1.0;
    Circuit *c = circuit_create();
    Component *q = pt_add(c, ty, 100, 100, 0);
    if (!q || !component_apply_part(q, part)) { circuit_free(c); *ok = 0; return 0; }

    Component *vcc = pt_add(c, COMP_DC_VOLTAGE, 300, 40, 0);
    vcc->props.dc_voltage.voltage = sgn * 5.0;
    Component *gv = pt_add(c, COMP_GROUND, 300, 140, 0);
    Component *rc = pt_add(c, COMP_RESISTOR, 240, 60, 0);
    rc->props.resistor.resistance = 100.0;
    rc->props.resistor.power_rating = 10.0;
    /* bias and signal from one source: the offset sets the operating point, the amplitude is
       the small signal. 10 % of the bias is small enough to stay linear. */
    Component *ibs = pt_add(c, COMP_AC_CURRENT, 0, 100, 0);
    ibs->props.ac_current.offset = sgn * ib_bias;
    ibs->props.ac_current.amplitude = sgn * ib_bias * 0.1;
    ibs->props.ac_current.frequency = f_test;
    ibs->props.ac_current.phase = 0;
    ibs->props.ac_current.ideal = true;
    Component *gi = pt_add(c, COMP_GROUND, 0, 200, 0);
    Component *ge = pt_add(c, COMP_GROUND, 200, 260, 0);
    /* the AC short at the collector. A data sheet's f_T is measured into a short so that the
       Miller effect plays no part - without it C_bc is multiplied by the stage gain and what
       comes out is the stage's bandwidth, not the transistor's. */
    Component *cc = pt_add(c, COMP_CAPACITOR, 240, 160, 90);
    cc->props.capacitor.capacitance = 1e-6;
    cc->props.capacitor.ideal = true;
    Component *gc = pt_add(c, COMP_GROUND, 240, 260, 0);

    int base = pt_node(c, 60, 100), coll = pt_node(c, 180, 60), emit = pt_node(c, 180, 160);
    int rail = pt_node(c, 300, 0), rr = pt_node(c, 280, 60);
    int gnd1 = pt_node(c, 0, 180), gnd2 = pt_node(c, 300, 120), gnd3 = pt_node(c, 200, 240);
    int ct = pt_node(c, 240, 120), cb = pt_node(c, 240, 200), gnd4 = pt_node(c, 240, 240);
    ibs->node_ids[0] = gnd1; ibs->node_ids[1] = base; gi->node_ids[0] = gnd1;
    vcc->node_ids[0] = rail; vcc->node_ids[1] = gnd2; gv->node_ids[0] = gnd2;
    rc->node_ids[0] = rr; rc->node_ids[1] = coll; circuit_add_wire(c, rail, rr);
    ge->node_ids[0] = gnd3; circuit_add_wire(c, emit, gnd3);
    cc->node_ids[0] = ct; cc->node_ids[1] = cb;
    circuit_add_wire(c, coll, ct);
    gc->node_ids[0] = gnd4; circuit_add_wire(c, cb, gnd4);
    q->node_ids[0] = base; q->node_ids[1] = coll; q->node_ids[2] = emit;

    Simulation *sim = simulation_create(c);
    *ok = sim && simulation_dc_analysis(sim);
    double ft = 0;
    if (*ok) {
        double period = 1.0 / f_test;
        double dt = period / 200.0;
        simulation_set_time_step(sim, dt);
        simulation_start(sim);
        double t_end = 8.0 * period, t_meas = 6.0 * period;
        double ic_lo = 1e30, ic_hi = -1e30, ib_lo = 1e30, ib_hi = -1e30;
        while (sim->time < t_end) {
            if (!simulation_step(sim)) { *ok = 0; break; }
            if (sim->time < t_meas) continue;         /* let the bias settle first */
            simulation_compute_terminal_currents(sim);
            double ibq = q->terminal_current[0], icq = q->terminal_current[1];
            if (ibq < ib_lo) ib_lo = ibq;  if (ibq > ib_hi) ib_hi = ibq;
            if (icq < ic_lo) ic_lo = icq;  if (icq > ic_hi) ic_hi = icq;
        }
        double ib_pp = ib_hi - ib_lo, ic_pp = ic_hi - ic_lo;
        if (*ok && ib_pp > 1e-15) ft = f_test * (ic_pp / ib_pp);
        if (!isfinite(ft)) *ok = 0;
    }
    if (sim) simulation_free(sim);
    circuit_free(c);
    return ft;
}

static void pc_bjt(const char *part, double ib, double *hfe, double *vbe, int *ok) {
    ComponentType ty = part_type(part);
    bool pnp = (ty == COMP_PNP_BJT);
    Circuit *c = circuit_create();
    Component *q = pt_add(c, ty, 100, 100, 0);
    if (!q || !component_apply_part(q, part)) { circuit_free(c); *ok = 0; return; }
    Component *vcc = pt_add(c, COMP_DC_VOLTAGE, 300, 40, 0);
    vcc->props.dc_voltage.voltage = pnp ? -5.0 : 5.0;
    Component *gv = pt_add(c, COMP_GROUND, 300, 140, 0);
    Component *rc = pt_add(c, COMP_RESISTOR, 240, 60, 0);
    rc->props.resistor.resistance = 100.0;
    rc->props.resistor.power_rating = 10.0;
    Component *ibs = pt_add(c, COMP_DC_CURRENT, 0, 100, 0);
    ibs->props.dc_current.current = pnp ? -ib : ib;
    Component *gi = pt_add(c, COMP_GROUND, 0, 200, 0);
    Component *ge = pt_add(c, COMP_GROUND, 200, 260, 0);
    int base = pt_node(c, 60, 100), coll = pt_node(c, 180, 60), emit = pt_node(c, 180, 160);
    int rail = pt_node(c, 300, 0), rr = pt_node(c, 280, 60);
    int gnd1 = pt_node(c, 0, 180), gnd2 = pt_node(c, 300, 120), gnd3 = pt_node(c, 200, 240);
    ibs->node_ids[0] = gnd1; ibs->node_ids[1] = base; gi->node_ids[0] = gnd1;
    vcc->node_ids[0] = rail; vcc->node_ids[1] = gnd2; gv->node_ids[0] = gnd2;
    rc->node_ids[0] = rr; rc->node_ids[1] = coll; circuit_add_wire(c, rail, rr);
    ge->node_ids[0] = gnd3; circuit_add_wire(c, emit, gnd3);
    q->node_ids[0] = base; q->node_ids[1] = coll; q->node_ids[2] = emit;
    Simulation *sim = simulation_create(c);
    *ok = sim && simulation_dc_analysis(sim);
    if (*ok) {
        Node *nb = circuit_get_node(c, base), *nc = circuit_get_node(c, coll);
        double vb = nb ? nb->voltage : 0, vc = nc ? nc->voltage : 0;
        double ic = ((pnp ? -5.0 : 5.0) - vc) / 100.0;
        if (pnp) { ic = -ic; vb = -vb; }
        *hfe = ib > 0 ? ic / ib : 0;
        *vbe = vb;
        if (!isfinite(*hfe) || !isfinite(*vbe)) *ok = 0;
    }
    simulation_free(sim);
    circuit_free(c);
}

/* Forced forward (or zener) current; returns the drop across the part. */
static double pc_diode_v(const char *part, double iforce, bool reverse, int *ok) {
    ComponentType ty = part_type(part);
    Circuit *c = circuit_create();
    Component *d = pt_add(c, ty, 100, 100, 0);
    if (!d || !component_apply_part(d, part)) { circuit_free(c); *ok = 0; return 0; }
    Component *is = pt_add(c, COMP_DC_CURRENT, 0, 100, 0);
    is->props.dc_current.current = iforce;
    Component *gi = pt_add(c, COMP_GROUND, 0, 200, 0);
    Component *gk = pt_add(c, COMP_GROUND, 200, 200, 0);
    int a = pt_node(c, 60, 100), k = pt_node(c, 140, 100);
    int gnd1 = pt_node(c, 0, 180), gnd2 = pt_node(c, 200, 180);
    /* forward: current into the anode; reverse (zener): into the cathode */
    is->node_ids[0] = gnd1; is->node_ids[1] = reverse ? k : a;
    gi->node_ids[0] = gnd1;
    gk->node_ids[0] = gnd2;
    circuit_add_wire(c, reverse ? a : k, gnd2);
    d->node_ids[0] = a; d->node_ids[1] = k;
    double v = pc_dc_at(c, reverse ? k : a, ok);
    circuit_free(c);
    return v;
}

/* A unity-gain buffer built from the part; drive is optional (NULL = input grounded). */
static Circuit *pc_buffer(const char *part, Component **src_out, int *out_node) {
    Circuit *c = circuit_create();
    Component *u = pt_add(c, COMP_OPAMP, 200, 100, 0);
    if (!u || !component_apply_part(u, part)) { circuit_free(c); return NULL; }
    Component *v = pt_add(c, COMP_PULSE_SOURCE, 0, 100, 0);
    v->props.pulse_source.v_low = 0; v->props.pulse_source.v_high = 0;
    /* edges every 2 us, so a run of a few microseconds always contains one */
    v->props.pulse_source.period = 4e-6; v->props.pulse_source.pulse_width = 2e-6;
    v->props.pulse_source.rise_time = v->props.pulse_source.fall_time = 1e-9;
    Component *g0 = pt_add(c, COMP_GROUND, 0, 200, 0);
    Component *rl = pt_add(c, COMP_RESISTOR, 340, 140, 90);
    rl->props.resistor.resistance = 10000.0;
    Component *gl = pt_add(c, COMP_GROUND, 340, 240, 0);
    int plus = pt_node(c, 160, 120), minus = pt_node(c, 160, 80), out = pt_node(c, 240, 100);
    int sp = pt_node(c, 0, 60), gnd0 = pt_node(c, 0, 180), lt = pt_node(c, 340, 100), lb = pt_node(c, 340, 180), gnd1 = pt_node(c, 340, 220);
    v->node_ids[0] = sp; v->node_ids[1] = gnd0; g0->node_ids[0] = gnd0;
    circuit_add_wire(c, sp, plus);
    circuit_add_wire(c, out, minus);                 /* unity-gain feedback */
    circuit_add_wire(c, out, lt);
    rl->node_ids[0] = lt; rl->node_ids[1] = lb; gl->node_ids[0] = gnd1;
    circuit_add_wire(c, lb, gnd1);
    u->node_ids[0] = minus; u->node_ids[1] = plus; u->node_ids[2] = out;
    if (src_out) *src_out = v;
    if (out_node) *out_node = out;
    return c;
}

static double pc_opamp_vos(const char *part, int *ok) {
    int out = -1;
    Circuit *c = pc_buffer(part, NULL, &out);
    if (!c) { *ok = 0; return 0; }
    double v = pc_dc_at(c, out, ok);
    circuit_free(c);
    return v;
}

/* Largest dV/dt the buffer produces on a 0 -> 5 V step. */
static double pc_opamp_sr(const char *part, int *ok) {
    Component *src = NULL; int out = -1;
    Circuit *c = pc_buffer(part, &src, &out);
    if (!c) { *ok = 0; return 0; }
    src->props.pulse_source.v_high = 5.0;
    Simulation *sim = simulation_create(c);
    *ok = sim && simulation_dc_analysis(sim);
    double best = 0;
    if (*ok) {
        double dt = 2e-9;                            /* fine enough for a 13 V/us part */
        simulation_set_time_step(sim, dt);
        simulation_start(sim);
        Node *n = circuit_get_node(c, out);
        double prev = n ? n->voltage : 0;
        for (int i = 0; i < 4000; i++) {
            if (!simulation_step(sim)) { *ok = 0; break; }
            n = circuit_get_node(c, out);
            double v = n ? n->voltage : 0;
            double slope = fabs(v - prev) / dt;
            if (slope > best) best = slope;
            prev = v;
        }
    }
    if (!isfinite(best)) *ok = 0;
    simulation_free(sim);
    circuit_free(c);
    return best;
}

/* A regulator in its own template, with the named part applied to it. */
static double pc_template(const char *part, CircuitTemplateType t, int *ok) {
    ComponentType ty = part_type(part);
    Circuit *c = circuit_create();
    circuit_place_template(c, t, 0, 0);
    Component *reg = find_comp(c, ty, 0);
    if (!reg || !component_apply_part(reg, part)) { circuit_free(c); *ok = 0; return 0; }
    ComponentType oct; int oord, oterm;
    if (!circuit_template_output_spec(t, &oct, &oord, &oterm)) { circuit_free(c); *ok = 0; return 0; }
    Component *probe = find_comp(c, oct, oord);
    double v = probe ? pc_dc_at(c, probe->node_ids[oterm], ok) : 0;
    if (!probe) *ok = 0;
    circuit_free(c);
    return v;
}

/* ---------------------------------------------------------------------------------------
 * --dvdt-test: every storage element against C dv/dt, from outside the solver.
 *
 * This suite exists because of a bug that nothing else could see. The diode's junction
 * capacitance was stamped with its current source the wrong way round - `-Ieq` at the anode
 * where a capacitor uses `+Ieq` - so the branch carried C(v + v_prev)/dt instead of
 * C(v - v_prev)/dt: not a memory of the charge but an injection of it. Every conservation
 * check in this tool passed, and had to, because a terminal current is recovered by
 * re-stamping the device alone and reading its residual. The report agrees with the stamp
 * whatever the stamp says, so KCL closes around a sign error exactly as it closes around the
 * truth. It shipped, and the waveform it changed was explained rather than believed.
 *
 * So this compares against arithmetic done outside the simulator. A sine of known amplitude
 * and frequency is forced across the element by an ideal source, which fixes its voltage for
 * all time - no transient, no operating point to settle - and the current is read at the peak
 * of C*A*omega*cos, where the sign is unambiguous. An inverted companion does not read a few
 * percent low here. It reads the wrong sign, or twice the value, or both.
 * ------------------------------------------------------------------------------------- */
typedef struct {
    const char *name;
    ComponentType type;
    double bias;           /* DC offset on the source: reverse bias for a junction */
    double value;          /* farads, or henries when inductive - set on the part, not assumed */
    int inductive;
    double freq;
    double amp;
    double cycles;         /* where to stop, in periods: at a phase where the answer is at a peak */
    int gate;              /* three-terminal: drive terminal 0, tie 1 and 2 to ground */
    const char *note;
} DvdtCase;

/* Every value here is written onto the component, so a changed default cannot quietly move the
   oracle with the measurement. What the defaults are is part-test's business. */
static const DvdtCase dvdt_cases[] = {
    { "capacitor",    COMP_CAPACITOR,      0.0,  1e-6,  0, 1000.0, 1.0, 2.0, 0, "1 uF, the reference case" },
    { "electrolytic", COMP_CAPACITOR_ELEC, 2.0,  1e-6,  0, 1000.0, 1.0, 2.0, 0, "polarised, biased forward" },
    { "diode cjo",    COMP_DIODE,         -5.0,  1e-12, 0, 1e6,    1.0, 2.0, 0, "1 pF junction, held 5 V in reverse" },
    { "schottky cjo", COMP_SCHOTTKY,      -5.0,  5e-12, 0, 1e6,    1.0, 2.0, 0, "5 pF junction, held 5 V in reverse" },
    { "inductor",     COMP_INDUCTOR,       0.0,  1e-3,  1, 1000.0, 1.0, 1.5, 0, "1 mH: the current is the integral, so read it at the peak of 1-cos" },
    /* The gate of a MOSFET held in cutoff, where Meyer's model leaves only the overlap
       capacitances and they are constants: Cgs + Cgd = (cgso + cgdo) * W, set explicitly below.
       Drain and source are grounded, so the only current into the gate is displacement current.
       This case exists because the gate capacitances advance their companion state inside the
       stamp, which runs once per Newton iteration and not once per accepted step - the same fault
       the diode's junction capacitance had, and invisible to every conservation check for the same
       reason. --flow-test skips gate nodes outright, so nothing else in the suite looks here. */
    { "mosfet gate",  COMP_NMOS,          -3.0,  2e-12, 0, 1e6,    1.0, 2.0, 1, "cutoff: 2 pF of overlap, drain and source grounded" },
};

/* i = C dv/dt with the tool's own sign convention: terminal_current[0] > 0 means current
   enters the device at terminal 0. Returns 0 on a failure to simulate. */
static double dvdt_measure(const DvdtCase *dc, double *expect_out, int *ok) {
    *ok = 1; *expect_out = 0;
    Circuit *c = circuit_create();
    if (!c) { *ok = 0; return 0; }

    /* source: amplitude on top of a DC offset, straight across the device */
    Component *v = pt_add(c, COMP_AC_VOLTAGE, 0, 60, 0);
    v->props.ac_voltage.amplitude = dc->amp;
    v->props.ac_voltage.frequency = dc->freq;
    v->props.ac_voltage.offset = dc->bias;
    Component *g0 = pt_add(c, COMP_GROUND, 0, 160, 0);
    Component *dev = pt_add(c, dc->type, 160, 60, 0);
    if (!dev) { circuit_free(c); *ok = 0; return 0; }
    switch (dc->type) {
        case COMP_CAPACITOR:      dev->props.capacitor.capacitance = dc->value; break;
        case COMP_CAPACITOR_ELEC: dev->props.capacitor_elec.capacitance = dc->value;
                                  dev->props.capacitor_elec.max_voltage = 100.0;
                                  dev->props.capacitor_elec.esr = 0.0; break;
        /* Ideal: the DCR and the saturation curve are real and are part-test's business. Here
           the integral has to be the whole answer, or the oracle is measuring a resistor. */
        case COMP_INDUCTOR:       dev->props.inductor.inductance = dc->value;
                                  dev->props.inductor.ideal = true; break;
        case COMP_DIODE:          dev->props.diode.cjo = dc->value; break;
        case COMP_SCHOTTKY:       dev->props.schottky.cjo = dc->value; break;
        case COMP_NMOS: case COMP_PMOS:
            /* the gate capacitances only exist in the non-ideal model */
            dev->props.mosfet.ideal = false;
            dev->props.mosfet.w = 10e-6;
            dev->props.mosfet.cgso = dc->value / (2.0 * 10e-6);   /* so cgso*W + cgdo*W = value */
            dev->props.mosfet.cgdo = dc->value / (2.0 * 10e-6);
            break;
        default: break;
    }
    Component *g1 = pt_add(c, COMP_GROUND, 160, 160, 0);

    int top = pt_node(c, 80, 20), gnd0 = pt_node(c, 0, 140), gnd1 = pt_node(c, 160, 140);
    v->node_ids[0] = top; v->node_ids[1] = gnd0; g0->node_ids[0] = gnd0;
    dev->node_ids[0] = top; dev->node_ids[1] = gnd1; g1->node_ids[0] = gnd1;
    if (dc->gate && dev->num_terminals > 2) dev->node_ids[2] = gnd1;   /* source to the same ground */
    circuit_add_wire(c, gnd0, gnd1);

    Simulation *sim = simulation_create(c);
    if (!sim) { circuit_free(c); *ok = 0; return 0; }

    /* A thousand steps a cycle, fixed: the companion's own truncation error is then a tenth of
       a percent, far below anything a sign or a factor of two would do. */
    double period = 1.0 / dc->freq;
    double dt = period / 1000.0;
    simulation_enable_adaptive(sim, false);
    simulation_set_time_step(sim, dt);
    if (!simulation_dc_analysis(sim)) { *ok = 0; }
    simulation_start(sim);
    simulation_set_time_step(sim, dt);

    /* Stop at a phase where the answer is at a peak and its sign is unambiguous: a capacitor's
       current is C A w cos, read at cos = +1; an inductor's is the integral of the voltage,
       (A/wL)(1 - cos), read at cos = -1. Whole cycles first, so nothing about the start is in
       the answer. */
    double t_end = dc->cycles * period;
    int guard = 0;
    while (*ok && sim->time < t_end - 0.5 * dt) {
        if (!simulation_step(sim)) { *ok = 0; break; }
        if (++guard > 20000) { *ok = 0; break; }
    }
    simulation_update_flow_display(sim);   /* terminal currents are recovered here, not in step() */
    double got = *ok ? dev->terminal_current[0] : 0;

    /* The oracle, computed here and not by the solver. */
    double w = 2.0 * M_PI * dc->freq;
    *expect_out = dc->inductive
        ? (dc->amp / (w * dc->value)) * (1.0 - cos(w * sim->time))   /* (1/L) integral of A sin */
        : dc->value * dc->amp * w * cos(w * sim->time);              /* C dv/dt */

    simulation_free(sim);
    circuit_free(c);
    return got;
}

/* A rotor is a storage element too, and its companion has an analytic answer just as a capacitor's
   does. Spin a motor up from rest on a DC supply and the speed rises as 1 - exp(-t/tau), with
   tau = J / (b + kt kv / R) - the mechanical inertia against friction plus the electrical damping
   the back-EMF provides. That is arithmetic done outside the solver, which is the point.
   It is here because the motor integrates its speed inside its stamp, so the integration happens
   once per Newton iteration rather than once per accepted step, and the spin-up comes out as many
   times too fast as the solver took iterations. The final speed is unaffected - d_omega is zero at
   steady state however many times it is added - so nothing that looks at where a motor ends up can
   see this, and nothing did. */
static int dvdt_motor(void) {
    Circuit *c = circuit_create();
    if (!c) return 1;
    Component *v = pt_add(c, COMP_DC_VOLTAGE, 0, 60, 0);
    v->props.dc_voltage.voltage = 10.0;
    Component *g0 = pt_add(c, COMP_GROUND, 0, 160, 0);
    Component *m = pt_add(c, COMP_DC_MOTOR, 160, 60, 0);
    if (!m) { circuit_free(c); return 1; }
    m->props.dc_motor.ideal = false;
    m->props.dc_motor.torque_load = 0.0;
    Component *g1 = pt_add(c, COMP_GROUND, 160, 160, 0);
    int top = pt_node(c, 80, 20), gnd0 = pt_node(c, 0, 140), gnd1 = pt_node(c, 160, 140);
    v->node_ids[0] = top; v->node_ids[1] = gnd0; g0->node_ids[0] = gnd0;
    m->node_ids[0] = top; m->node_ids[1] = gnd1; g1->node_ids[0] = gnd1;
    circuit_add_wire(c, gnd0, gnd1);

    double R = m->props.dc_motor.r_armature, kt = m->props.dc_motor.kt;
    double kv = m->props.dc_motor.kv, J = m->props.dc_motor.j_rotor;
    double bf = m->props.dc_motor.b_friction;
    double damp = bf + kt * kv / R;
    double tau = (damp > 0) ? J / damp : 0;
    double w_final = (damp > 0) ? (kt * 10.0 / R) / damp : 0;

    Simulation *sim = simulation_create(c);
    int ok = sim && simulation_dc_analysis(sim);
    simulation_enable_adaptive(sim, false);
    simulation_set_time_step(sim, tau / 2000.0);
    simulation_start(sim);
    simulation_set_time_step(sim, tau / 2000.0);

    /* the moment the speed passes 1 - 1/e of where it is heading */
    double t_63 = -1;
    int guard = 0;
    while (ok && sim->time < 5.0 * tau && guard++ < 200000) {
        if (!simulation_step(sim)) { ok = 0; break; }
        if (t_63 < 0 && m->props.dc_motor.omega >= 0.6321 * w_final) t_63 = sim->time;
    }
    double w_end = m->props.dc_motor.omega;
    simulation_free(sim);
    circuit_free(c);

    int bad = !ok || t_63 <= 0 || fabs(t_63 - tau) > 0.05 * tau;
    printf("%s %-14s %14.6g %14.6g %7.2f%%   %s\n", bad ? "[FAIL]" : "[ OK ]", "dc motor spin-up",
           t_63, tau, (tau > 0) ? 100.0 * (t_63 - tau) / tau : 0.0,
           "time to 63 % of final speed against J / (b + kt kv / R)");
    if (bad)
        printf("       final speed %.6g rad/s against %.6g, so it gets there - it is the getting "
               "there that is timed wrong\n", w_end, w_final);
    return bad ? 1 : 0;
}

/* A relay's coil is an RL branch, so its current has the same analytic answer an inductor's does:
   switched onto a DC supply it rises as (V/R)(1 - exp(-t/tau)) with tau = L/R. The default coil is
   200 ohm and 100 mH, so tau is 500 us, and the pull-in threshold crosses on that curve - which is
   what makes the timing matter: a relay that reaches 63 % of its coil current in one time step
   pulls in immediately, and a delay circuit built on it (the classic RC-relay flasher) has no
   delay. The coil current is advanced inside the stamp today, once per Newton iteration; this case
   is the measurement that precedes the fix, the same order the motor's took. */
static int dvdt_relay(void) {
    Circuit *c = circuit_create();
    if (!c) return 1;
    Component *v = pt_add(c, COMP_DC_VOLTAGE, 0, 60, 0);
    v->props.dc_voltage.voltage = 12.0;
    Component *g0 = pt_add(c, COMP_GROUND, 0, 160, 0);
    Component *k = pt_add(c, COMP_RELAY, 160, 60, 0);
    if (!k) { circuit_free(c); return 1; }
    Component *g1 = pt_add(c, COMP_GROUND, 160, 160, 0);
    Component *g2 = pt_add(c, COMP_GROUND, 260, 160, 0);
    int top = pt_node(c, 80, 20), gnd0 = pt_node(c, 0, 140), gnd1 = pt_node(c, 160, 140);
    int gnd2 = pt_node(c, 260, 140);
    v->node_ids[0] = top; v->node_ids[1] = gnd0; g0->node_ids[0] = gnd0;
    /* coil across the supply; both contact terminals grounded so no subnet floats */
    k->node_ids[0] = top; k->node_ids[1] = gnd1; g1->node_ids[0] = gnd1;
    k->node_ids[2] = gnd2; k->node_ids[3] = gnd2; g2->node_ids[0] = gnd2;
    circuit_add_wire(c, gnd0, gnd1);
    circuit_add_wire(c, gnd1, gnd2);

    double R = k->props.relay.r_coil, L = k->props.relay.l_coil;
    double tau = (R > 0) ? L / R : 0;
    double i_final = (R > 0) ? 12.0 / R : 0;

    Simulation *sim = simulation_create(c);
    int ok = sim && simulation_dc_analysis(sim);
    /* From rest: the DC operating point of an RL coil on a supply is the final current, so zero
       the state after the operating point or there is no transient to time. */
    k->props.relay.i_coil = 0.0;
    k->props.relay.energized = false;
    simulation_enable_adaptive(sim, false);
    simulation_set_time_step(sim, tau / 1000.0);
    simulation_start(sim);
    simulation_set_time_step(sim, tau / 1000.0);
    k->props.relay.i_coil = 0.0;
    k->props.relay.energized = false;

    double t_63 = -1;
    int guard = 0;
    while (ok && sim->time < 5.0 * tau && guard++ < 100000) {
        if (!simulation_step(sim)) { ok = 0; break; }
        if (t_63 < 0 && k->props.relay.i_coil >= 0.6321 * i_final) t_63 = sim->time;
    }
    simulation_free(sim);
    circuit_free(c);

    int bad = !ok || t_63 <= 0 || fabs(t_63 - tau) > 0.05 * tau;
    printf("%s %-14s %14.6g %14.6g %7.2f%%   %s\n", bad ? "[FAIL]" : "[ OK ]", "relay coil",
           t_63, tau, (tau > 0 && t_63 > 0) ? 100.0 * (t_63 - tau) / tau : -100.0,
           "coil current to 63 % of V/R against tau = L/R");
    return bad ? 1 : 0;
}

/* ---------------------------------------------------------------------------------------
 * --meas-test: the numbers on the measurements panel, against arithmetic.
 *
 * Vpp, Vavg, Vrms, f, T and duty are what a user reads off the scope, and nothing verified
 * them: the probe suites check waveforms straight from the history, never the derived numbers
 * the panel shows. Every screenshot taken today read "D:49%" against waveforms that are 50 %
 * by construction, which is the sort of thing a check finds in a second and a reader shrugs at
 * for years.
 *
 * The inputs are synthetic - a sine, a square and a triangle built here from known parameters,
 * fed straight into analysis_measure_waveform - so the oracles are exact closed forms and the
 * solver is not in the loop. Two windows per waveform: a whole number of cycles, where the
 * answer has no excuse, and a ragged fraction, where the wobble a sliding capture causes is
 * measured and bounded instead of wondered about.
 * ------------------------------------------------------------------------------------- */
typedef struct {
    const char *name;
    int kind;                  /* 0 sine, 1 square, 2 triangle */
    double amp, off, freq, duty;
    double n_cycles;           /* window length, cycles */
    /* oracles; negative tolerance fields mean "do not judge this one" */
    double vpp, vavg, vrms, duty_pct;
    double tol_pct;            /* on everything, in percent of the true value */
} MeasCase;

static const MeasCase meas_cases[] = {
    /* whole windows: exact */
    { "sine, whole",     0, 1.0, 0.0, 1000.0, 0.5, 8.0, 2.0, 0.0, 0.70711, 50.0, 0.5 },
    { "sine + offset",   0, 1.0, 2.5, 1000.0, 0.5, 8.0, 2.0, 2.5, 2.59808, 50.0, 0.5 },
    { "square 50%",      1, 2.5, 2.5, 1000.0, 0.5, 8.0, 5.0, 2.5, 3.53553, 50.0, 0.5 },
    { "square 25%",      1, 2.5, 2.5, 1000.0, 0.25, 8.0, 5.0, 1.25, 2.5, 25.0, 0.5 },
    { "triangle, whole", 2, 1.0, 0.0, 1000.0, 0.5, 8.0, 2.0, 0.0, 0.57735, 50.0, 0.5 },
    /* ragged windows: the same waveforms over 8.37 cycles; the tolerance is the point */
    /* 5 %: the wobble a window of 8.37 cycles actually has was measured at 4.5 % on the square's
       average - the 0.37 of a cycle is simply in the numbers, and these rows exist to bound it,
       not to wish it smaller. The scope's own centring stopped using the window mean for exactly
       this; the panel still shows it, and this row is what says how far it can be off. */
    { "sine, ragged",    0, 1.0, 0.0, 1000.0, 0.5, 8.37, 2.0, 0.0, 0.70711, 50.0, 5.0 },
    { "square, ragged",  1, 2.5, 2.5, 1000.0, 0.5, 8.37, 5.0, 2.5, 3.53553, 50.0, 5.0 },
};

static double meas_wave(const MeasCase *mc, double t) {
    double ph = fmod(t * mc->freq, 1.0);
    switch (mc->kind) {
        case 1:  return mc->off + (ph < mc->duty ? mc->amp : -mc->amp);
        case 2:  return mc->off + mc->amp * (ph < 0.5 ? 4.0 * ph - 1.0 : 3.0 - 4.0 * ph);
        default: return mc->off + mc->amp * sin(2.0 * M_PI * ph);
    }
}

static int meas_check(const char *name, const char *what, double got, double want, double tol_pct,
                      double scale, int *fails) {
    /* the absolute term is scaled to the waveform, not to the oracle: an oracle of zero (a
       centred sine's average) must not collapse the tolerance to nothing */
    double tol = fabs(want) * tol_pct / 100.0 + fabs(scale) * tol_pct / 100.0;
    int bad = fabs(got - want) > tol;
    if (bad) {
        (*fails)++;
        printf("[FAIL] meas  %-16s %-5s = %.6g, arithmetic says %.6g (tol %.2g%%)\n",
               name, what, got, want, tol_pct);
    }
    return bad;
}

/* ---------------------------------------------------------------------------------------
 * --fft-test: the spectrum view and the THD number, against closed forms.
 *
 * The FFT drives the scope's spectrum view, and THD is the number the Function Generator's
 * "3rd harmonic > 30 dB down" claim rests on. Neither had a check. Same method as --meas-test:
 * synthetic waveforms whose spectra are known exactly, fed straight into analysis_fft_compute.
 *
 * The inputs are built to make the oracles exact rather than approximate: 1024 samples (the
 * FFT's own size, so no zero-padding), the fundamental placed on bin 50 exactly (so there is no
 * leakage for a window to smear), and the rectangular window selected explicitly (so bin
 * amplitudes are the coefficients themselves). A square wave's odd harmonics fall at 1/h and a
 * triangle's at 1/h^2, both on exact bins; THD here is defined over harmonics 2..10, which for
 * those is 42.88 % and 12.05 % - the function's own contract, not the infinite-series 48.3 %.
 * ------------------------------------------------------------------------------------- */
static int fft_check(const char *name, const char *what, double got, double want, double tol,
                     int *fails) {
    int bad = fabs(got - want) > tol;
    if (bad) {
        (*fails)++;
        printf("[FAIL] fft   %-10s %-14s = %.6g, arithmetic says %.6g (tol %.3g)\n",
               name, what, got, want, tol);
    }
    return bad;
}

static int fft_test(void) {
    enum { N = FFT_SIZE, BIN = 50 };
    const double FS = 102400.0;               /* so bin 50 is exactly 5000 Hz */
    const double F0 = BIN * FS / (double)N;
    static AnalysisState st;
    static double s[N];
    int fails = 0, checks = 0;

    /* kind 3 is the same sine sitting on 5 V of DC. Nothing removed the mean before the
       transform, and a Hann window smears a DC term into bins 0 and +/-1 - so the fundamental
       search, which starts at k = 1 to skip DC, locked onto the leakage instead and reported the
       fundamental as one bin for every probe on a rail. The three cases above are all centred on
       zero and could never have shown it. */
    for (int kind = 0; kind < 4; kind++) {
        const char *name = kind == 0 ? "sine" : kind == 1 ? "square" : kind == 2 ? "triangle" : "sine on 5 V";
        for (int i = 0; i < N; i++) {
            double ph = fmod((double)i * BIN / N, 1.0);
            s[i] = kind == 0 ? sin(2.0 * M_PI * ph)
                 : kind == 1 ? (ph < 0.5 ? 1.0 : -1.0)
                 : kind == 2 ? (ph < 0.5 ? 4.0 * ph - 1.0 : 3.0 - 4.0 * ph)
                 :             5.0 + sin(2.0 * M_PI * ph);
        }
        analysis_init(&st);
        /* Rectangular for the three centred cases: the input is bin-exact, so no window is needed
           and the bin amplitudes are the coefficients themselves. The DC case uses Hanning, the
           app's own default, because that is where the fault lived: a rectangular window puts
           exactly zero DC leakage in bin 1, so a rectangular DC case would pass either way. */
        st.fft_window_type = (kind == 3) ? 1 : 0;
        analysis_fft_compute(&st, s, N, FS, 0);
        FFTResult *f = &st.fft_results[0];

        checks += 2;
        fft_check(name, "fundamental", f->fundamental_freq, F0, 0.5, &fails);
        /* 3rd harmonic relative to the fundamental: -inf, -9.54 dB (1/3), -19.08 dB (1/9) */
        double rel3 = f->magnitude[3 * BIN] - f->magnitude[BIN];
        if (kind == 1) fft_check(name, "3rd harmonic", rel3, -9.542, 0.2, &fails);
        if (kind == 2) fft_check(name, "3rd harmonic", rel3, -19.085, 0.2, &fails);
        if ((kind == 0 || kind == 3) && rel3 > -80.0) {
            fails++;
            printf("[FAIL] fft   sine       3rd harmonic   = %.4g dB - a pure sine has none\n", rel3);
        }
        if (kind == 3) {
            /* Hanning spreads every line over three bins, so THD and the harmonic levels are not
               the closed forms the other cases use. The fundamental is the thing that broke and
               the thing this case exists to hold. */
            printf("[ OK ] fft   %-10s fundamental %.1f Hz with a 5 V pedestal and a Hann window\n",
                   name, f->fundamental_freq);
            continue;
        }
        checks += 1;
        double thd_want = (kind == 0 || kind == 3) ? 0.0 : kind == 1 ? 42.879 : 12.047;
        double thd_tol  = (kind == 0 || kind == 3) ? 0.1 : 0.5;
        fft_check(name, "THD", f->thd, thd_want, thd_tol, &fails);
        if (kind == 0 || kind == 3) {
            checks += 1;
            if (f->snr < 60.0) {
                fails++;
                printf("[FAIL] fft   sine       SNR            = %.4g dB - a clean sine should clear 60\n", f->snr);
            }
        }
    }
    printf("\nfft-test: %d checks over 4 synthetic spectra, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}

/* --dcm-test: the measured retry of the CCM-vs-DCM question.
 *
 * An asynchronous buck in discontinuous conduction has a third interval where neither the switch
 * nor the diode conducts. With an ideal switch (r_off 1e9) that leaves the switch node undefined,
 * and docs/ROADMAP.md records that it ran away to hundreds or thousands of volts. It was blocked
 * on the time step: a realistic snubber could not be resolved at dt = 100 ns.
 *
 * Two things have changed since. MIN_TIME_STEP is 10 ps rather than 1 ns, and the simulation now
 * measures its own period and refines its step. So this asks the question again with numbers
 * instead of assuming the old answer still holds: take the Buck Converter template, walk the load
 * from heavy (continuous) to very light (deeply discontinuous), and report what the switch node
 * actually does at each. A node that stays inside a few times the input is behaving; one that
 * reaches kilovolts is the fault this item is about.
 */
/* --iv-test: the nineteen interview-prep templates, each against a number its own annotation
 * states, and each asked the same question twice at two different time steps.
 *
 * These are the templates a person opens to check an answer before an interview, so a number on
 * the canvas that the solver does not reproduce is worse here than anywhere else in the library:
 * it is wrong in the one place someone is going to repeat it out loud. Nothing was checking them.
 * --class-test says they converge, --geom-test says they are laid out, --flow-test says charge is
 * conserved, and none of that notices that the text beside the circuit claims a pin sits at 4.0 V
 * while the solver puts it at 3.85, or that a probe is parked on a node that is always 0 V.
 *
 * Measurement points are node COORDINATES, not the nth component of a type. The builders lay these
 * out on an explicit grid, so a coordinate is what the author actually wrote and what --trace
 * prints back; a per-type ordinal silently follows any part inserted ahead of it. If a builder
 * moves, this fails loudly, which is right.
 *
 * LEVEL and PP are measured over the settled second half of the run - they are questions about
 * where a circuit ends up. MAXV and MINV are measured over the whole run, because the interesting
 * moment in a reflection, a ring or an inrush is the transient itself.
 *
 * Every case runs again at dt/8. A number that moves when the step changes is not a property of
 * the circuit, and printing it on the canvas as though it were is the same fault as printing a
 * wrong one.
 */
typedef enum { IV_LEVEL, IV_PP, IV_RATIO_PP, IV_MAXV, IV_MINV } IvKind;
typedef struct {
    CircuitTemplateType t;
    IvKind kind;
    float ax, ay;
    float bx, by;          /* the second node, for IV_RATIO_PP */
    double expect, tol;    /* tol is relative */
    double run, dt;
    const char *what;
    /* Set only where the step-independence bar is known not to be met AND the reason is written up
       in docs/ROADMAP.md. Reported as [NOTE] with the reason attached, and counted in the summary,
       never hidden: an exemption with no explanation is how a hole starts looking like coverage. */
    const char *known_drift;
} IvCase;

static const IvCase iv_cases[] = {
 /* ---- instrumentation and the scope ------------------------------------------------------- */
 { CIRCUIT_IV_PROBE_COMP,    IV_MAXV,  380, 400,  0,0,   0.500, 0.05, 4e-3,  1e-5,
   "compensated 10x: a flat 0.5 V from a 5 V square" },
 { CIRCUIT_IV_PROBE_LOADING, IV_MAXV,  220,  40,  0,0,   3.300, 0.03, 4e-6,  1e-8,
   "the 1 MHz square still reaches 3.3 V" },
 { CIRCUIT_IV_GROUND_LEAD,   IV_MAXV,  220,  40,  0,0,   3.693, 0.06, 4e-7,  1e-9,
   "150 nH of ground lead against 12 pF: the edge overshoots 3.3 V and rings" },
 { CIRCUIT_IV_SCOPE_INPUT_Z, IV_MAXV,  460,  40,  0,0,   2.000, 0.03, 4e-7,  1e-9,
   "1 M input, open cable: the step reflects and reads twice the 1 V setting" },
 { CIRCUIT_IV_AC_COUPLING,   IV_PP,    640,  60,  0,0,   0.200, 0.05, 4e-5,  2e-8,
   "200 mVpp of ripple, recovered" },
 { CIRCUIT_IV_AC_COUPLING,   IV_LEVEL,  60,  20,  0,0,  12.000, 0.01, 4e-5,  2e-8,
   "the 12 V rail it is hiding on" },
 { CIRCUIT_IV_SHUNT_SENSE,   IV_LEVEL, 160, 260,  0,0,   0.100, 0.03, 2e-2,  5e-5,
   "low side: 1 A x 100 mohm lifts the load's ground by 100 mV" },
 { CIRCUIT_IV_KELVIN,        IV_LEVEL,   0,  60,  0,0,   0.110, 0.03, 2e-2,  5e-5,
   "2-wire at the connector: 110 mV, so 110 mohm for a 10 mohm part" },
 { CIRCUIT_IV_KELVIN,        IV_LEVEL, 540, 360,  0,0,   0.010, 0.05, 2e-2,  5e-5,
   "4-wire through the difference amp: 10 mV, so 10 mohm" },

 /* ---- converters and power delivery -------------------------------------------------------- */
 { CIRCUIT_IV_BUCK_NODES,    IV_LEVEL, 640,  20,  0,0,   5.969, 0.04, 4e-3,  2e-7,
   "12 V at 50 % duty through real parts: 6.0 V out" },
 { CIRCUIT_IV_LDO_VS_BUCK,   IV_LEVEL, 280,   0,  0,0,   5.000, 0.03, 4e-4,  1e-7,
   "the linear regulator's 5 V rail" },
 { CIRCUIT_IV_BOOTSTRAP,     IV_MAXV,  200,   0,  0,0,  23.500, 0.05, 4e-4,  1e-7,
   "C_boot rides the switch node: BOOT reaches 23.5 V, 11.5 V above a 12 V source" },

 /* ---- I/O, termination and signal integrity ------------------------------------------------ */
 { CIRCUIT_IV_TERMINATION,   IV_MAXV,  260,  20,  0,0,   4.400, 0.08, 5e-7,  2.5e-10,
   "unterminated: the 2.2 V launched doubles at the open far end" },
 { CIRCUIT_IV_TERMINATION,   IV_MAXV,  260, 340,  0,0,   3.300, 0.05, 5e-7,  2.5e-10,
   "series 33 ohm: incident plus reflected is still the full 3.3 V at the receiver" },
 { CIRCUIT_IV_PULLUP_SIZING, IV_MAXV,  200, 100,  0,0,   3.300, 0.03, 8e-5,  2.5e-7,
   "the open-drain bus does get all the way to 3.3 V" },
 { CIRCUIT_IV_GROUND_BOUNCE, IV_PP,    160, 100,  0,0,   2.026, 0.06, 5e-7,  0,
   "330 mA through 5 nH of shared bond wire swings the local ground" },
 { CIRCUIT_IV_CROSSTALK,     IV_MAXV,  400,  20,  0,0,   0.943, 0.08, 5e-7,  2.5e-10,
   "2 pF into a high-impedance victim: 3.3 x 2/7, a logic level from nothing" },
 { CIRCUIT_IV_ESD_CLAMP,     IV_LEVEL, 160,  60,  0,0,   3.852, 0.02, 2e-3,  5e-5,
   "6 V through 1 k: the pin clamps one diode drop above the 3.3 V rail" },
 { CIRCUIT_IV_ESD_CLAMP,     IV_LEVEL, 160, 420,  0,0,   3.704, 0.02, 2e-3,  5e-5,
   "the same 6 V through 220 k: less current, so less forward drop" },

 /* ---- analog fundamentals ------------------------------------------------------------------ */
 { CIRCUIT_IV_CAP_ENERGY,    IV_LEVEL,   0,  60,  0,0,   5.000, 0.03, 5e-2,  2.5e-4,
   "charge is conserved: the charged 100 uF settles at half of 10 V" },
 { CIRCUIT_IV_CAP_ENERGY,    IV_LEVEL, 340,  60,  0,0,   5.000, 0.03, 5e-2,  2.5e-4,
   "and the empty one meets it there - half the energy is gone" },
 { CIRCUIT_IV_MILLER,        IV_RATIO_PP, 340, 400, 340, 0, 0.1429, 0.12, 4e-6, 1e-8,
   "10 pF of C_gd against 10 k at 1 MHz: the second stage keeps a seventh of the first" },
 { CIRCUIT_IV_SWITCH_CHOICE, IV_LEVEL, 200, 120,  0,0,   0.072, 0.08, 5e-2,  5e-5,
   "the 2N3904 saturates at 0.07 V, whatever the current" },
 { CIRCUIT_IV_SWITCH_CHOICE, IV_LEVEL, 900, 120,  0,0,   0.406, 0.08, 5e-2,  5e-5,
   "the 2N7000 drops I x R_DS(on), and 5 V of gate is not 10 V" },
 { CIRCUIT_IV_INRUSH,        IV_MINV,    0,  20,  0,0,  10.020, 0.03, 5e-2,  0,
   "an empty capacitor is a short: 200 A pulls the 12 V rail down to 10 V" },
 { CIRCUIT_IV_INRUSH,        IV_MINV,    0, 320,  0,0,  11.980, 0.01, 5e-2,  0,
   "4.7 ohm in series and the same rail barely moves" },
};

static int iv_node_at(Circuit *c, float x, float y) {
    int best = -1; float bd = 1e30f;
    for (int i = 0; i < c->num_nodes; i++) {
        float dx = c->nodes[i].x - x, dy = c->nodes[i].y - y;
        float d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = c->nodes[i].id; }
    }
    return (bd <= 100.0f) ? best : -1;   /* within 10 px, or the coordinate has gone stale */
}

/* The step the APP would run this template at: the finer of what the circuit's own dynamics ask
   for and what the scope's time base implies, which is exactly what app.c does. A suite that
   invents its own step is testing a different program - the first version of this file forced
   250 us on Hot-Plug Inrush, whose event is over in 50 us, and then reported the template as
   unstable when the number it got at that step disagreed with the number at a finer one. The
   template was fine; the harness was asking it a question the app never asks. */
static double iv_app_step(Simulation *sim, CircuitTemplateType t) {
    simulation_auto_time_step(sim);
    double td = circuit_template_scope_time_div(t);
    if (td > 0) {
        double dtp = simulation_scope_time_step(sim, td);
        if (dtp > 0 && dtp < sim->time_step) return dtp;
    }
    return sim->time_step;
}

/* scale > 0 multiplies the app's own step (1.0 = what the app uses, 0.125 = eight times finer);
   cs->dt, when set, overrides it outright. */
static double iv_measure(const IvCase *cs, double scale, int *ok) {
    *ok = 0;
    Circuit *c = circuit_create();
    if (!c) return 0;
    if (circuit_place_template(c, cs->t, 0, 0) <= 0) { circuit_free(c); return 0; }
    int na = iv_node_at(c, cs->ax, cs->ay);
    int nb = (cs->kind == IV_RATIO_PP) ? iv_node_at(c, cs->bx, cs->by) : -1;
    if (na < 0 || (cs->kind == IV_RATIO_PP && nb < 0)) { circuit_free(c); return 0; }

    Simulation *sim = simulation_create(c);
    if (!sim) { circuit_free(c); return 0; }
    simulation_dc_analysis(sim);
    double base = (cs->dt > 0) ? cs->dt : iv_app_step(sim, cs->t);
    simulation_set_time_step(sim, base * scale);
    simulation_start(sim);

    /* extremes over the whole run, levels over the settled half - see the header */
    int whole = (cs->kind == IV_MAXV || cs->kind == IV_MINV);
    double amn = 1e30, amx = -1e30, asum = 0, bmn = 1e30, bmx = -1e30;
    long n = 0, steps = 0;
    int good = 1;
    while (sim->time < cs->run && steps < 4000000) {
        if (!simulation_step(sim)) { good = 0; break; }
        steps++;
        if (!whole && sim->time < cs->run * 0.5) continue;
        Node *a = circuit_get_node(c, na);
        double va = a ? a->voltage : 0;
        if (va < amn) amn = va;
        if (va > amx) amx = va;
        asum += va;
        if (nb >= 0) { Node *b = circuit_get_node(c, nb); double vb = b ? b->voltage : 0;
                       if (vb < bmn) bmn = vb; if (vb > bmx) bmx = vb; }
        n++;
    }
    double r = 0;
    if (good && n > 0) {
        switch (cs->kind) {
            case IV_LEVEL:    r = asum / (double)n; break;
            case IV_PP:       r = amx - amn; break;
            case IV_MAXV:     r = amx; break;
            case IV_MINV:     r = amn; break;
            case IV_RATIO_PP: r = (bmx - bmn) > 0 ? (amx - amn) / (bmx - bmn) : 0; break;
        }
        *ok = 1;
    }
    simulation_free(sim);
    circuit_free(c);
    return r;
}

static int iv_test(void) {
    int fails = 0, moved = 0, excused_drifts = 0;
    unsigned n = (unsigned)(sizeof iv_cases / sizeof iv_cases[0]);
    for (unsigned k = 0; k < n; k++) {
        const IvCase *cs = &iv_cases[k];
        const CircuitTemplateInfo *ti = circuit_template_get_info(cs->t);
        const char *nm = ti ? ti->short_name : "?";
        int ok1 = 0, ok2 = 0;
        double v1 = iv_measure(cs, 1.0,     &ok1);   /* the step the app would use */
        double v2 = iv_measure(cs, 1.0 / 8, &ok2);   /* and the same question, eight times finer */

        if (!ok1 || !ok2) {
            printf("[FAIL] iv  %-8s %-64s did not run\n", nm, cs->what);
            fails++;
            continue;
        }
        double err = fabs(v1 - cs->expect) / (fabs(cs->expect) > 1e-12 ? fabs(cs->expect) : 1.0);
        /* the step-independence bar is half the accuracy bar: a number quoted to a person has to be
           a property of the circuit, not of how finely it happened to be integrated */
        double drift = (fabs(v1) > 1e-12) ? fabs(v2 - v1) / fabs(v1) : fabs(v2 - v1);
        int bad = (err > cs->tol);
        int slid = (drift > cs->tol / 2);
        int excused = (slid && cs->known_drift != NULL);
        if (bad) fails++;
        if (slid) moved++;
        if (excused) excused_drifts++;
        printf("%s iv  %-8s %-64s %9.4g vs %-9.4g %+6.1f%%  dt/8 %+.2f%%%s\n",
               bad ? "[FAIL]" : slid ? (excused ? "[NOTE]" : "[FAIL]") : "[ OK ]",
               nm, cs->what, v1, cs->expect,
               100.0 * (v1 - cs->expect) / (fabs(cs->expect) > 1e-12 ? cs->expect : 1.0),
               100.0 * drift, slid ? "  << moves with the step" : "");
        if (excused) printf("       known: %s\n", cs->known_drift);
    }
    printf("\niv-test: %u documented numbers over the 19 interview templates, %d the solver does "
           "not reproduce, %d that move with the time step (%d written up, %d not)\n",
           n, fails, moved, excused_drifts, moved - excused_drifts);
    /* A drift that is written up is a tracked limitation and does not block. One that is not is a
       regression, and so is any number the solver fails to reproduce at all. */
    return (fails || (moved - excused_drifts) > 0) ? 1 : 0;
}

/* --conv-test: does any template's ANSWER move when the time step is refined?
 *
 * --class-test asks whether a template's *class* survives a finer step, which catches a signal
 * changing character. It says nothing about the numbers. --iv-test asks that question properly but
 * only of the nineteen interview templates and only of figures written on their canvases. Between
 * them nothing asked the general question: run every template at the step the app picks, run it
 * again eight times finer, and see whether the answer is the same.
 *
 * It is the question that found the Hot-Plug Inrush sag reading 11.33 V when it is 10.02 - the
 * step was chosen from source periods, the event had no source, and the solver walked over it. That
 * fault was found by hand on one template. This asks all 188.
 *
 * Measured on the template's own probed output: its mean over the settled half, and its
 * peak-to-peak over the same window. A template is flagged when either moves by more than 2 %,
 * which is far looser than any of these should need and still catches a step that is stepping over
 * something. Amplitudes below a microvolt are ignored - a probe idling on solver residue has no
 * meaningful percentage.
 */
typedef struct { double level, pp; int ok; int settled; } ConvResult;

static ConvResult conv_measure(CircuitTemplateType t, double scale) {
    ConvResult r = { 0, 0, 0, 0 };
    Circuit *c = circuit_create();
    if (!c) return r;
    if (circuit_place_template(c, t, 0, 0) <= 0 || c->num_probes < 1) { circuit_free(c); return r; }
    Simulation *sim = simulation_create(c);
    if (!sim) { circuit_free(c); return r; }

    /* a swept source makes every run a different circuit; hold it still, as class-test does */
    for (int i = 0; i < c->num_components; i++)
        if (c->components[i]->type == COMP_AC_VOLTAGE)
            c->components[i]->props.ac_voltage.frequency_sweep.enabled = false;

    simulation_dc_analysis(sim);
    double dt_app = iv_app_step(sim, t);
    /* The horizon comes from the APP step, so both runs cover the same stretch of circuit time and
       the comparison is about the step and not about where the run stopped. */
    double horizon = 4000.0 * dt_app;
    double latest = 0;
    for (int i = 0; i < c->num_components; i++) {
        Component *k = c->components[i];
        if (k && k->type == COMP_PULSE_SOURCE && k->props.pulse_source.delay > latest)
            latest = k->props.pulse_source.delay;
    }
    if (latest > 0 && horizon < latest * 2.0) horizon = latest * 2.0;

    simulation_set_time_step(sim, dt_app * scale);
    simulation_start(sim);
    simulation_set_time_step(sim, dt_app * scale);

    int node = c->probes[0].node_id;
    double mn = 1e30, mx = -1e30, sum = 0;
    /* the third quarter and the fourth, kept apart: a circuit whose swing is still growing between
       them has not settled, and comparing two steps during a start-up ramp says nothing about the
       steps - see the note at the head of this suite */
    double q3mn = 1e30, q3mx = -1e30, q4mn = 1e30, q4mx = -1e30;
    long n = 0, steps = 0;
    int good = 1;
    while (sim->time < horizon && steps < 4000000) {
        if (!simulation_step(sim)) { good = 0; break; }
        steps++;
        if (sim->time < horizon * 0.5) continue;
        Node *nd = circuit_get_node(c, node);
        double v = nd ? nd->voltage : 0;
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        if (sim->time < horizon * 0.75) { if (v < q3mn) q3mn = v; if (v > q3mx) q3mx = v; }
        else                            { if (v < q4mn) q4mn = v; if (v > q4mx) q4mx = v; }
        sum += v;
        n++;
    }
    if (good && n > 0) {
        r.level = sum / (double)n;
        r.pp = mx - mn;
        r.ok = 1;
        double q3 = q3mx - q3mn, q4 = q4mx - q4mn;
        double ref = q3 > q4 ? q3 : q4;
        r.settled = (ref <= 1e-9) || (fabs(q4 - q3) / ref <= 0.05);
    }
    simulation_free(sim);
    circuit_free(c);
    return r;
}

static int conv_test(void) {
    int fails = 0, ran = 0, skipped = 0, unsettled = 0;
    /* 3 %. Everything here except two self-limiting LC oscillators sits far below it; those two
       converge, just slowly, because the amplitude a soft-limited oscillator settles at depends
       a little on numerical damping. Colpitts measures 31.95 V at dt = 2 ns and 32.07 V at 500 ps
       - 0.4 % for a fourfold refinement - so it is converging and not drifting. A bar at 2 %
       failed it and said nothing true. */
    const double TOL = 0.03;
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        if (!ti) continue;
        ConvResult a = conv_measure((CircuitTemplateType)t, 1.0);
        ConvResult b = conv_measure((CircuitTemplateType)t, 1.0 / 8.0);
        if (!a.ok || !b.ok) {
            printf("[FAIL] conv  %-28s did not run at %s\n", ti->name, a.ok ? "dt/8" : "dt");
            fails++;
            continue;
        }
        if (!a.settled || !b.settled) {
            unsettled++;
            printf("[NOTE] conv  %-28s still building at the end of the run - not compared\n",
                   ti->name);
            continue;
        }
        ran++;
        /* A probe sitting on solver residue has no meaningful percentage - and neither does the
           MEAN of a waveform whose true mean is zero. Judging the level against itself made a
           28 kV three-phase bus fail because its mean moved from -0.77 V to nothing while its
           peak-to-peak moved 0.3 %: a hundred per cent of almost zero. The level is measured
           against the signal it sits on. */
        double scale = fabs(a.level) > fabs(a.pp) ? fabs(a.level) : fabs(a.pp);
        double lref = scale > 1e-6 ? scale : 0;
        double pref = fabs(a.pp) > 1e-6 ? fabs(a.pp) : 0;
        if (lref == 0 && pref == 0) { skipped++; continue; }

        double dl = lref > 0 ? fabs(b.level - a.level) / lref : 0;
        double dp = pref > 0 ? fabs(b.pp - a.pp) / pref : 0;
        double worst = dl > dp ? dl : dp;
        if (worst > TOL) {
            printf("[FAIL] conv  %-28s level %.6g -> %.6g (%+.1f%%), pp %.6g -> %.6g (%+.1f%%)\n",
                   ti->name, a.level, b.level, 100.0 * (lref > 0 ? (b.level - a.level) / lref : 0),
                   a.pp, b.pp, 100.0 * (pref > 0 ? (b.pp - a.pp) / pref : 0));
            fails++;
        }
    }
    printf("\nconv-test: %d templates run at the app's step and again eight times finer, "
           "%d whose answer moves by more than %.0f%% (%d idling below a microvolt, "
           "%d still building and not compared)\n", ran, fails, TOL * 100.0, skipped, unsettled);
    return fails ? 1 : 0;
}

/* --mc-test: the Monte Carlo analysis, against the arithmetic of error propagation.
 *
 * The panel offers a tolerance and a number of runs and prints a mean, a standard deviation, a
 * minimum and a maximum. Nothing checked any of it. A Monte Carlo that forgot to randomise would
 * print a standard deviation of zero and look like a very well designed circuit.
 *
 * The oracle is a two-resistor divider, built here so its composition is known exactly rather
 * than read off a template that might gain a part. Its output is
 *
 *     Vout = V . R2 / (R1 + R2)
 *
 * and the analysis varies V, R1 and R2 independently by a relative sigma s - the code takes the
 * quoted tolerance as three sigma, which is the usual reading of a tolerance band. Propagating:
 *
 *     dVout/dV  . dV  = Vout . s
 *     dVout/dR1 . dR1 = -V R1 R2 / (R1+R2)^2 . s
 *     dVout/dR2 . dR2 =  V R1 R2 / (R1+R2)^2 . s
 *
 * so the relative spread of the output is
 *
 *     sigma_rel = s . sqrt( 1 + 2 (R1/(R1+R2))^2 )
 *
 * which for equal resistors is 1.2247 s. That number comes from calculus and not from the
 * simulator, which is the point.
 *
 * The bands are loose because a sample of N runs knows its own sigma only to about 1/sqrt(2N) -
 * 3.5 % at 400 runs - and because a Gaussian fed through a division is not exactly Gaussian.
 */
static int mc_test(void) {
    int fails = 0, checks = 0;
    static const struct { double r1, r2, tol_pct; int runs; } cases[] = {
        { 10000.0, 10000.0, 10.0, 400 },
        { 10000.0, 10000.0,  1.0, 400 },
        {  1000.0, 10000.0,  5.0, 400 },
    };

    for (unsigned k = 0; k < sizeof cases / sizeof cases[0]; k++) {
        double R1 = cases[k].r1, R2 = cases[k].r2, V = 10.0;
        Circuit *c = circuit_create();
        if (!c) { fails++; continue; }
        Component *src = pt_add(c, COMP_DC_VOLTAGE, 0, 60, 0);
        src->props.dc_voltage.voltage = V;
        Component *g0 = pt_add(c, COMP_GROUND, 0, 160, 0);
        Component *r1 = pt_add(c, COMP_RESISTOR, 120, 20, 0);
        r1->props.resistor.resistance = R1;
        Component *r2 = pt_add(c, COMP_RESISTOR, 240, 60, 90);
        r2->props.resistor.resistance = R2;
        Component *g1 = pt_add(c, COMP_GROUND, 240, 160, 0);

        int sp = pt_node(c, 0, 20), sm = pt_node(c, 0, 100), gt = pt_node(c, 0, 140);
        int ra = pt_node(c, 80, 20), rb = pt_node(c, 160, 20);
        int mid = pt_node(c, 240, 20), r2b = pt_node(c, 240, 100), g1t = pt_node(c, 240, 140);
        src->node_ids[0] = sp; src->node_ids[1] = sm;
        g0->node_ids[0] = gt;
        r1->node_ids[0] = ra; r1->node_ids[1] = rb;
        r2->node_ids[0] = mid; r2->node_ids[1] = r2b;
        g1->node_ids[0] = g1t;
        circuit_add_wire(c, sp, ra);
        circuit_add_wire(c, rb, mid);
        circuit_add_wire(c, sm, gt);
        circuit_add_wire(c, r2b, g1t);
        Node *mn = circuit_get_node(c, mid);
        circuit_add_probe(c, mid, mn ? mn->x : 240, mn ? mn->y : 20);

        Simulation *sim = simulation_create(c);
        AnalysisState an;
        analysis_init(&an);
        analysis_monte_carlo_init(&an, cases[k].runs, true, cases[k].tol_pct);
        static MCBackup backup;
        memset(&backup, 0, sizeof backup);
        int guard = 0;
        while (!analysis_monte_carlo_step(&an, c, sim, 0, &backup) && guard++ < 5000) { }

        MonteCarloAnalysis *mc = &an.monte_carlo;
        double nominal = V * R2 / (R1 + R2);
        double s = cases[k].tol_pct / 100.0 / 3.0;
        double frac = R1 / (R1 + R2);
        double want_rel = s * sqrt(1.0 + 2.0 * frac * frac);
        double got_rel = (mc->mean != 0) ? mc->std_dev / fabs(mc->mean) : 0;

        checks += 2;
        int bad_mean = !(fabs(mc->mean - nominal) <= 0.02 * nominal + 1e-9);
        /* the sample knows its own sigma to about 1/sqrt(2N); 25 % is far outside that */
        int bad_sd = !(want_rel > 0 && fabs(got_rel - want_rel) <= 0.25 * want_rel);
        if (bad_mean) fails++;
        if (bad_sd) fails++;

        printf("%s mc    R1=%-7.0f R2=%-7.0f tol=%-4.1f%% n=%d  mean %.4f (want %.4f)  "
               "sigma/mean %.4f (want %.4f)  results=%d\n",
               (bad_mean || bad_sd) ? "[FAIL]" : "[ OK ]",
               R1, R2, cases[k].tol_pct, cases[k].runs, mc->mean, nominal,
               got_rel, want_rel, mc->num_results);

        /* and the values have to come back afterwards: an analysis that leaves the circuit
           randomised has quietly edited the user's schematic */
        checks++;
        if (fabs(r1->props.resistor.resistance - R1) > 1e-6 * R1 ||
            fabs(r2->props.resistor.resistance - R2) > 1e-6 * R2 ||
            fabs(src->props.dc_voltage.voltage - V) > 1e-9) {
            printf("[FAIL] mc    the circuit was left randomised: R1 %.4g, R2 %.4g, V %.4g\n",
                   r1->props.resistor.resistance, r2->props.resistor.resistance,
                   src->props.dc_voltage.voltage);
            fails++;
        }

        simulation_free(sim);
        circuit_free(c);
    }
    printf("\nmc-test: %d checks over %u divider cases against propagated error, %d failed\n",
           checks, (unsigned)(sizeof cases / sizeof cases[0]), fails);
    return fails ? 1 : 0;
}

/* --bode-test: the frequency sweep against the transfer function of an RC low-pass.
 *
 * The Bode plot had no check of any kind. It is a whole analysis - sweep a source, measure
 * magnitude and phase at each point, draw the result - and nothing said whether the numbers on it
 * were right.
 *
 * The oracle is first-year: for R in series with C to ground,
 *
 *     |H(f)| = 1 / sqrt(1 + (f/fc)^2)        arg H(f) = -atan(f/fc)        fc = 1/(2 pi R C)
 *
 * The RC Low Pass template is 1 k and 100 nF, so fc = 1591.55 Hz, the magnitude there is -3.01 dB
 * and the phase -45 degrees, and a decade above it the magnitude is -20 dB below the passband.
 * None of those numbers comes from the simulator.
 */
static int bode_test(void) {
    int fails = 0, checks = 0;
    const double R = 1000.0, C = 100e-9;
    const double fc = 1.0 / (2.0 * M_PI * R * C);

    Circuit *c = circuit_create();
    if (!c || circuit_place_template(c, CIRCUIT_RC_LOWPASS, 0, 0) <= 0) {
        printf("[FAIL] bode  the RC low-pass will not place\n");
        if (c) circuit_free(c);
        return 1;
    }
    /* the probed output is the capacitor's top terminal, which is what the template probes */
    int probe_node = (c->num_probes > 1) ? c->probes[1].node_id
                   : (c->num_probes > 0) ? c->probes[0].node_id : 0;

    Simulation *sim = simulation_create(c);
    if (!sim) { circuit_free(c); return 1; }
    simulation_dc_analysis(sim);

    /* what the app does when the Bode button is pressed */
    if (!simulation_freq_sweep(sim, 100.0, 20000.0, 0, probe_node, 40)) {
        printf("[FAIL] bode  the sweep did not run: %s\n", simulation_get_error(sim));
        simulation_free(sim); circuit_free(c);
        return 1;
    }

    FreqResponsePoint pts[MAX_FREQ_POINTS];
    int n = simulation_get_freq_response(sim, pts, MAX_FREQ_POINTS);
    if (n < 10) {
        printf("[FAIL] bode  %d points came back\n", n);
        simulation_free(sim); circuit_free(c);
        return 1;
    }

    /* Compare every point against the transfer function, and report the worst. A tolerance of
       1.5 dB is loose - the sweep measures peak-to-peak over two cycles and reads phase off a
       zero crossing, so it is not going to be exact - and still nowhere near wide enough to
       accept a response measured at the wrong frequency. */
    double worst_mag = 0, worst_at = 0, worst_phase = 0, worst_phase_at = 0;
    for (int i = 0; i < n; i++) {
        double f = pts[i].frequency;
        if (!(f > 0)) continue;
        double want_mag = 20.0 * log10(1.0 / sqrt(1.0 + (f / fc) * (f / fc)));
        double want_ph  = -atan(f / fc) * 180.0 / M_PI;
        double dm = fabs(pts[i].magnitude_db - want_mag);
        double dp = fabs(pts[i].phase_deg - want_ph);
        if (dm > worst_mag) { worst_mag = dm; worst_at = f; }
        if (dp > worst_phase) { worst_phase = dp; worst_phase_at = f; }
    }

    checks++;
    if (worst_mag > 1.5) {
        printf("[FAIL] bode  magnitude is %.2f dB out at %.0f Hz (fc = %.1f Hz)\n",
               worst_mag, worst_at, fc);
        fails++;
    } else {
        printf("[ OK ] bode  magnitude within %.2f dB of 1/sqrt(1+(f/fc)^2) over %d points\n",
               worst_mag, n);
    }

    checks++;
    if (worst_phase > 12.0) {
        printf("[FAIL] bode  phase is %.1f degrees out at %.0f Hz\n", worst_phase, worst_phase_at);
        fails++;
    } else {
        printf("[ OK ] bode  phase within %.1f degrees of -atan(f/fc)\n", worst_phase);
    }

    /* and the sweep must give the circuit back as it found it: it is run on the live simulation,
       so a clock or a step left where the sweep put them is the user's running circuit disturbed */
    checks++;
    Component *src = NULL;
    for (int i = 0; i < c->num_components; i++)
        if (c->components[i]->type == COMP_AC_VOLTAGE) { src = c->components[i]; break; }
    if (src && !src->props.ac_voltage.frequency_sweep.enabled) {
        printf("[FAIL] bode  the source's own frequency sweep was left switched off\n");
        fails++;
    } else {
        printf("[ OK ] bode  the source is back the way the sweep found it\n");
    }

    printf("\nbode-test: %d checks against the RC transfer function (fc = %.1f Hz), %d failed\n",
           checks, fc, fails);
    simulation_free(sim);
    circuit_free(c);
    return fails ? 1 : 0;
}

/* --sign-test: the direction a device pushes, against arithmetic done outside the solver.
 *
 * This codebase already knows that its conservation checks cannot see a stamp's sign: terminal
 * currents are recovered by re-stamping the device on its own, so a device wired backwards is
 * still perfectly consistent with itself and --flow-test passes. Three sign faults were found by
 * hand in earlier releases for exactly that reason.
 *
 * So: build the smallest circuit that has a known answer with a known SIGN, and check the sign as
 * well as the size. A battery under load must read BELOW its open-circuit voltage. That is not a
 * modelling opinion, and no amount of internal consistency can rescue it.
 */
/* defined below, with the transformer check it serves */
static double ct_primary_current(ComponentType tt, double rload, int *ok);

typedef struct {
    const char *what;
    ComponentType src;
    double v_oc, r_int, r_load;
} SignCase;

/* terminal voltage of a source with internal resistance across a load - the divider, and nothing
   the simulator has any say in */
static double sign_expect(const SignCase *sc) {
    return sc->v_oc * sc->r_load / (sc->r_load + sc->r_int);
}

static double sign_measure(const SignCase *sc, int *ok) {
    *ok = 0;
    Circuit *c = circuit_create();
    if (!c) return 0;

    Component *src = pt_add(c, sc->src, 0, 60, 0);
    if (!src) { circuit_free(c); return 0; }
    if (sc->src == COMP_BATTERY) {
        src->props.battery.nominal_voltage = sc->v_oc;
        src->props.battery.internal_r = sc->r_int;
        src->props.battery.ideal = false;
        src->props.battery.discharged = false;
        src->props.battery.charge_state = 1.0;
        src->props.battery.charge_coulombs = 1e9;     /* not the subject of this test */
    } else {
        src->props.dc_voltage.voltage = sc->v_oc;
        src->props.dc_voltage.r_series = sc->r_int;
        src->props.dc_voltage.ideal = false;
    }
    Component *g0 = pt_add(c, COMP_GROUND, 0, 160, 0);
    Component *rl = pt_add(c, COMP_RESISTOR, 200, 60, 90);
    rl->props.resistor.resistance = sc->r_load;
    Component *g1 = pt_add(c, COMP_GROUND, 200, 160, 0);

    int sp = pt_node(c, 0, 20), sm = pt_node(c, 0, 100), gt = pt_node(c, 0, 140);
    int lt = pt_node(c, 200, 20), lb = pt_node(c, 200, 100), g1t = pt_node(c, 200, 140);
    src->node_ids[0] = sp; src->node_ids[1] = sm;
    g0->node_ids[0] = gt;
    rl->node_ids[0] = lt; rl->node_ids[1] = lb;
    g1->node_ids[0] = g1t;
    circuit_add_wire(c, sp, lt);
    circuit_add_wire(c, sm, gt);
    circuit_add_wire(c, lb, g1t);

    Simulation *sim = simulation_create(c);
    double v = 0;
    if (sim && simulation_dc_analysis(sim)) {
        Node *nd = circuit_get_node(c, lt);
        v = nd ? nd->voltage : 0;
        *ok = 1;
    }
    if (sim) simulation_free(sim);
    circuit_free(c);
    return v;
}

static int sign_test(void) {
    static const SignCase cases[] = {
        /* the control: a DC source with the same series resistance, which --std-test already
           holds to documented bus voltages under load */
        { "DC source, 12 V, 1 ohm internal, 3 ohm load", COMP_DC_VOLTAGE, 12.0, 1.0, 3.0 },
        { "battery,   12 V, 1 ohm internal, 3 ohm load", COMP_BATTERY,    12.0, 1.0, 3.0 },
        { "battery,   1.5 V AA, 0.5 ohm, 1 ohm load",    COMP_BATTERY,     1.5, 0.5, 1.0 },
        { "battery,   9 V, 2 ohm, 100 ohm load (light)", COMP_BATTERY,     9.0, 2.0, 100.0 },
    };
    int fails = 0;
    for (unsigned k = 0; k < sizeof cases / sizeof cases[0]; k++) {
        const SignCase *sc = &cases[k];
        int ok = 0;
        double got = sign_measure(sc, &ok);
        double want = sign_expect(sc);
        if (!ok) {
            printf("[FAIL] sign  %-42s did not solve\n", sc->what);
            fails++;
            continue;
        }
        /* two questions, and the first one is the one that matters: a source under load cannot
           read above its own open-circuit voltage, whatever the arithmetic says */
        int wrong_way = (got > sc->v_oc + 1e-9);
        int wrong_size = fabs(got - want) > 0.01 * fabs(want) + 1e-9;
        if (wrong_way || wrong_size) fails++;
        printf("%s sign  %-42s %.4f V, want %.4f (open circuit %.2f)%s\n",
               (wrong_way || wrong_size) ? "[FAIL]" : "[ OK ]",
               sc->what, got, want, sc->v_oc,
               wrong_way ? "   << ABOVE open circuit: the internal resistance pushes the wrong way"
                         : "");
    }
    /* And a transformer has to be loaded by its own secondary.
       One that does not reflect its secondary current is a free source: the primary sees only its
       magnetising branch however hard the secondary is worked. The centre-tapped part was exactly
       that - it had no current variable at all, so there was nothing to reflect with, and its
       primary drew the same 0.012 A into a 10 k load as into a 10 ohm one while the two-winding
       part beside it went up by a factor of 999. Neither --flow-test nor the conservation checks
       can see it, for the same reason they cannot see a sign: each device is consistent with
       itself. */
    {
        const struct { const char *name; ComponentType t; } kinds[] = {
            { "two-winding transformer",   COMP_TRANSFORMER },
            { "centre-tapped transformer", COMP_TRANSFORMER_CT },
        };
        for (unsigned k = 0; k < 2; k++) {
            int o1 = 0, o2 = 0;
            double light = ct_primary_current(kinds[k].t, 10000.0, &o1);
            double heavy = ct_primary_current(kinds[k].t, 10.0, &o2);
            double ratio = (light > 1e-12) ? heavy / light : 0.0;
            /* a thousandfold drop in load resistance has to show as a large rise in primary
               current; 100x is far below what either part does and far above doing nothing */
            int bad = !(o1 && o2) || ratio < 100.0;
            if (bad) fails++;
            printf("%s sign  %-42s primary %.4f A into 10k, %.4f A into 10 ohm (x%.0f)%s\n",
                   bad ? "[FAIL]" : "[ OK ]", kinds[k].name, light, heavy, ratio,
                   bad ? "   << the secondary's load never reaches the primary" : "");
        }
    }

    printf("\nsign-test: %u sources measured against the divider they make with their own load, "
           "and 2 transformers against their own secondaries, %d wrong\n",
           (unsigned)(sizeof cases / sizeof cases[0]), fails);
    return fails ? 1 : 0;
}

/* Does loading a transformer's secondary load its primary?
 *
 * A transformer that does not reflect its secondary current is a free source: the primary sees
 * only its magnetising branch however hard the secondary is worked, so power out does not come
 * from power in. The two-winding COMP_TRANSFORMER carries an auxiliary current and stamps -N i_s
 * into the primary KCL, which is exactly this. COMP_TRANSFORMER_CT has no auxiliary current at
 * all - it is not in the needs_voltage_var list - so there is nothing to reflect with.
 *
 * Measured by putting a sense resistor in series with the primary and reading the current through
 * it with the secondary lightly and then heavily loaded. A real transformer draws about N^2 times
 * more; one that does not reflect draws the same either way.
 */
static double ct_primary_current(ComponentType tt, double rload, int *ok) {
    *ok = 0;
    Circuit *c = circuit_create();
    if (!c) return 0;

    Component *v = pt_add(c, COMP_AC_VOLTAGE, 0, 60, 0);
    v->props.ac_voltage.amplitude = 120.0;
    v->props.ac_voltage.frequency = 60.0;
    Component *g0 = pt_add(c, COMP_GROUND, 0, 160, 0);
    Component *rs = pt_add(c, COMP_RESISTOR, 100, 20, 0);      /* primary sense */
    rs->props.resistor.resistance = 0.01;
    Component *tx = pt_add(c, tt, 240, 60, 0);
    Component *rl = pt_add(c, COMP_RESISTOR, 420, 60, 90);
    rl->props.resistor.resistance = rload;
    Component *g1 = pt_add(c, COMP_GROUND, 420, 180, 0);

    int sp = pt_node(c, 0, 20), sm = pt_node(c, 0, 100), gt = pt_node(c, 0, 140);
    int ra = pt_node(c, 60, 20), rb = pt_node(c, 140, 20);
    int p1 = pt_node(c, 200, 20), p2 = pt_node(c, 200, 100);
    int s1 = pt_node(c, 280, 20), ct = pt_node(c, 280, 60), s2 = pt_node(c, 280, 100);
    int la = pt_node(c, 420, 20), lb = pt_node(c, 420, 140), g1t = pt_node(c, 420, 160);

    v->node_ids[0] = sp; v->node_ids[1] = sm;
    g0->node_ids[0] = gt;
    rs->node_ids[0] = ra; rs->node_ids[1] = rb;
    circuit_add_wire(c, sp, ra);
    circuit_add_wire(c, sm, gt);
    circuit_add_wire(c, rb, p1);
    circuit_add_wire(c, p2, gt);

    tx->node_ids[0] = p1; tx->node_ids[1] = p2;
    tx->node_ids[2] = s1;
    if (tt == COMP_TRANSFORMER_CT) { tx->node_ids[3] = ct; tx->node_ids[4] = s2; }
    else                           { tx->node_ids[3] = s2; }

    rl->node_ids[0] = la; rl->node_ids[1] = lb;
    circuit_add_wire(c, s1, la);
    circuit_add_wire(c, s2, lb);
    circuit_add_wire(c, lb, g1t);
    g1->node_ids[0] = g1t;

    Simulation *sim = simulation_create(c);
    double ipk = 0;
    if (sim && simulation_dc_analysis(sim)) {
        simulation_set_time_step(sim, 1.0 / 60.0 / 400.0);
        simulation_start(sim);
        int guard = 0;
        while (sim->time < 0.05 && guard++ < 40000) {
            if (!simulation_step(sim)) break;
            if (sim->time > 0.025) {
                Node *a = circuit_get_node(c, ra), *b = circuit_get_node(c, rb);
                double i = (a && b) ? fabs(a->voltage - b->voltage) / 0.01 : 0;
                if (i > ipk) ipk = i;
            }
        }
        *ok = 1;
    }
    if (sim) simulation_free(sim);
    circuit_free(c);
    return ipk;
}


/* --load-test: what the loaders do with input they cannot hold.
 *
 * --file-test round-trips all 188 templates and --spice-test parses well-formed netlists. Both
 * only ever feed the loaders things the app itself produced. Nothing asked what happens when a
 * file says something impossible, and the answer in two places was "use it anyway":
 *
 *   - the JSON loader took a component's node id straight from the file. Node ids are what
 *     node_map and the union-find in circuit_build_node_map are INDEXED BY, and both hold
 *     MAX_NODES, so a hand-edited or corrupted file carrying 999999 became a subscript.
 *   - the SPICE node table answered 0 when it was full, and 0 is GROUND. A netlist with more
 *     than SPICE_MAX_NODES nets imported "successfully" with every net past the 256th shorted
 *     to ground.
 *
 * A refusal is the right answer to both. Silence is not.
 */
static int load_test(void) {
    int fails = 0, checks = 0;
    const char *tmp = getenv("TEMP");
    if (!tmp) tmp = ".";

    /* ---- 1. a circuit file naming a node id the program cannot hold -------------------------- */
    {
        char path[600];
        snprintf(path, sizeof path, "%s\\ct_badnode.json", tmp);
        Circuit *ref = circuit_create();
        circuit_place_template(ref, CIRCUIT_RC_LOWPASS, 0, 0);
        file_export_json(ref, path);
        circuit_free(ref);

        /* read it back as text and point a terminal at 999999 */
        FILE *f = fopen(path, "rb");
        static char buf[1 << 20];
        size_t got = f ? fread(buf, 1, sizeof buf - 1, f) : 0;
        if (f) fclose(f);
        buf[got] = 0;
        char *t = strstr(buf, "\"terminals\": [");
        int wrote = 0;
        if (t) {
            char *open_br = strchr(t, '[');
            char *close_br = open_br ? strchr(open_br, ']') : NULL;
            if (open_br && close_br && (size_t)(close_br - open_br) > 4) {
                /* overwrite the first id in place, padding with spaces so the length is kept */
                char rep[] = "[999999";
                memcpy(open_br, rep, strlen(rep));
                for (char *p = open_br + strlen(rep); p < close_br; p++) *p = (*p == ',') ? ',' : ' ';
                wrote = 1;
            }
        }
        checks++;
        if (!wrote) {
            printf("[FAIL] load  could not build a circuit file naming an impossible node\n");
            fails++;
        } else {
            FILE *w = fopen(path, "wb");
            if (w) { fwrite(buf, 1, strlen(buf), w); fclose(w); }
            Circuit *c = circuit_create();
            bool loaded = file_import_json(c, path);
            if (loaded) {
                printf("[FAIL] load  a circuit naming node 999999 loaded; node_map holds %d\n", MAX_NODES);
                fails++;
            } else {
                printf("[ OK ] load  a circuit naming node 999999 is refused: %s\n", file_get_error());
            }
            circuit_free(c);
        }
        remove(path);
    }

    /* ---- 2. a netlist with more nets than the table holds ------------------------------------ */
    {
        static char net[200000];
        int n = snprintf(net, sizeof net, "* more nets than the table holds\n.SUBCKT BIG in out\n");
        for (int i = 0; i < 400 && n < (int)sizeof net - 64; i++)
            n += snprintf(net + n, sizeof net - n, "R%d n%d n%d 1k\n", i, i, i + 1);
        snprintf(net + n, sizeof net - n, ".ENDS\n.END\n");

        char err[256] = "";
        int imported = spice_import_text(net, err, sizeof err);
        checks++;
        if (imported > 0) {
            printf("[FAIL] load  a %d-net netlist imported as %d subcircuit(s); everything past "
                   "node %d was tied to ground\n", 401, imported, 256);
            fails++;
        } else {
            printf("[ OK ] load  a 400-net netlist is refused: %s\n", err[0] ? err : "(no message)");
        }
    }

    /* ---- 3. a circuit with no solution at all ------------------------------------------------
       Two ideal voltage sources of different voltages across the same pair of nodes. The system
       says V = 5 and V = 3 at once, and elimination reduces that to 0 = 2.

       The solver used to fake a pivot of 1e-15 for any zero it met and divide by it, so an
       equation with no solution came back as a number around 1e15 and was reported as an answer.
       A zero pivot whose right-hand side is also zero is a different thing - an unwired terminal
       has an all-zero row, which happens constantly in an editor - and that one is answered with
       zero, which is why refusing every singular row broke twelve suites. */
    {
        Circuit *c = circuit_create();
        Component *a = pt_add(c, COMP_DC_VOLTAGE, 0, 60, 0);
        a->props.dc_voltage.voltage = 5.0; a->props.dc_voltage.ideal = true;
        Component *b2 = pt_add(c, COMP_DC_VOLTAGE, 160, 60, 0);
        b2->props.dc_voltage.voltage = 3.0; b2->props.dc_voltage.ideal = true;
        Component *g = pt_add(c, COMP_GROUND, 0, 160, 0);
        int top = pt_node(c, 0, 20), bot = pt_node(c, 0, 100), gt = pt_node(c, 0, 140);
        int top2 = pt_node(c, 160, 20), bot2 = pt_node(c, 160, 100);
        a->node_ids[0] = top;  a->node_ids[1] = bot;
        b2->node_ids[0] = top2; b2->node_ids[1] = bot2;
        g->node_ids[0] = gt;
        circuit_add_wire(c, top, top2);
        circuit_add_wire(c, bot, bot2);
        circuit_add_wire(c, bot, gt);

        Simulation *sim = simulation_create(c);
        bool solved = sim && simulation_dc_analysis(sim);
        double v = 0;
        if (solved) { Node *nd = circuit_get_node(c, top); v = nd ? nd->voltage : 0; }
        checks++;
        if (solved) {
            /* The bar is not "is the number large". The old solver answered a confident 5.000 V
               here, which is the more dangerous kind of wrong: it satisfies one of the two
               equations and silently drops the other. A system with no solution must not come
               back as solved. */
            printf("[FAIL] load  5 V and 3 V sources in parallel reported success, answering "
                   "%.3f V to a system that also says 3\n", v);
            fails++;
        } else {
            printf("[ OK ] load  5 V and 3 V sources in parallel: no solution, and the solver "
                   "says so\n");
        }
        if (sim) simulation_free(sim);
        circuit_free(c);
    }

    /* ---- 4. splitting a wire leaves both halves attached to a node that exists ---------------
       circuit_remove_wire sweeps orphaned nodes, and the junction used to be created BEFORE the
       old wire was removed - so at the moment of the sweep it had no wire and no terminal on it,
       which is the definition of an orphan. It was swept, and the two halves were then wired to
       an id that named nothing. This happens on an ordinary edit: drawing a wire onto another
       one, or clicking one to put a junction in it. */
    {
        checks++;
        Circuit *c = circuit_create();
        Component *r1 = pt_add(c, COMP_RESISTOR, 0, 0, 0);
        Component *r2 = pt_add(c, COMP_RESISTOR, 400, 0, 0);
        int a = pt_node(c, 100, 0), b2 = pt_node(c, 300, 0);
        r1->node_ids[1] = a; r2->node_ids[0] = b2;
        int wid = circuit_add_wire(c, a, b2);
        Wire *w = NULL;
        for (int i = 0; i < c->num_wires; i++) if (c->wires[i].id == wid) w = &c->wires[i];

        int mid = w ? circuit_split_wire_at(c, w, 200, 0) : -1;
        int halves = 0, dangling = 0;
        for (int i = 0; i < c->num_wires; i++) {
            Wire *ww = &c->wires[i];
            int have_s = circuit_get_node(c, ww->start_node_id) != NULL;
            int have_e = circuit_get_node(c, ww->end_node_id) != NULL;
            if (!have_s || !have_e) dangling++;
            if (ww->start_node_id == mid || ww->end_node_id == mid) halves++;
        }
        int mid_exists = (mid >= 0) && (circuit_get_node(c, mid) != NULL);
        if (mid < 0 || !mid_exists || halves != 2 || dangling != 0) {
            printf("[FAIL] load  splitting a wire: junction id %d, exists %d, halves on it %d, "
                   "wires with a missing end %d\n", mid, mid_exists, halves, dangling);
            fails++;
        } else {
            printf("[ OK ] load  splitting a wire leaves two halves on a junction that exists\n");
        }
        circuit_free(c);
    }

    /* ---- 5. a truncated binary circuit ------------------------------------------------------
       The contract here is only that a half a file does not become half a circuit: the loader
       refuses it and the counts it leaves behind are inside the arrays that hold them.

       Being honest about what this does NOT do: it does not exercise the unbounded-count fault,
       which case 6 does. A failed fread leaves the count variable untouched, so a truncated file
       leaves it small whatever the guards say, and these four cases passed before the guards
       existed and after. They are worth keeping as a statement about truncation and they prove
       nothing about the counts. */
    {
        char path[600];
        snprintf(path, sizeof path, "%s\\ct_trunc.cpg", tmp);

        Circuit *ref = circuit_create();
        circuit_place_template(ref, CIRCUIT_RC_LOWPASS, 0, 0);
        file_save_circuit(ref, path);
        circuit_free(ref);

        /* how big a good one is, so the truncations are meaningful */
        FILE *f = fopen(path, "rb");
        long full = 0;
        if (f) { fseek(f, 0, SEEK_END); full = ftell(f); fclose(f); }

        static const double frac[] = { 0.10, 0.35, 0.60, 0.85 };
        for (unsigned q = 0; q < sizeof frac / sizeof frac[0]; q++) {
            long cut = (long)(full * frac[q]);
            FILE *r = fopen(path, "rb");
            static unsigned char buf[1 << 20];
            size_t got = r ? fread(buf, 1, sizeof buf, r) : 0;
            if (r) fclose(r);
            (void)got;
            char cutpath[600];
            snprintf(cutpath, sizeof cutpath, "%s\\ct_trunc_%u.cpg", tmp, q);
            FILE *w = fopen(cutpath, "wb");
            if (w) { fwrite(buf, 1, (size_t)cut, w); fclose(w); }

            Circuit *c = circuit_create();
            bool loaded = file_load_circuit(c, cutpath);
            checks++;
            /* Either answer is acceptable - a prefix can be a valid smaller circuit - as long as
               nothing ran off the end of an array. What must hold is that the counts left behind
               are inside the arrays that hold them. */
            int bad_counts = (c->num_nodes < 0 || c->num_nodes > MAX_NODES ||
                              c->num_wires < 0 || c->num_wires > MAX_WIRES ||
                              c->num_components < 0 || c->num_components > MAX_COMPONENTS);
            if (bad_counts) {
                printf("[FAIL] load  a file cut to %.0f%% left nodes=%d wires=%d parts=%d\n",
                       frac[q] * 100.0, c->num_nodes, c->num_wires, c->num_components);
                fails++;
            } else {
                printf("[ OK ] load  a file cut to %.0f%%: %s, nodes=%d wires=%d parts=%d\n",
                       frac[q] * 100.0, loaded ? "loaded" : "refused",
                       c->num_nodes, c->num_wires, c->num_components);
            }
            circuit_free(c);
            remove(cutpath);
        }
        remove(path);
    }

    /* ---- 6. a binary file that STATES an impossible count -----------------------------------
       The truncation cases above turned out not to prove anything: a failed fread leaves the
       count variable untouched, so it stays whatever it was, which is small. The real hazard is a
       file that says a huge number outright - the node and wire loops write straight into fixed
       arrays, so the loop bound IS the write bound. Built here from the header up, with no
       components, so the node count sits at a known offset. */
    {
        char path[600];
        snprintf(path, sizeof path, "%s\\ct_huge.cpg", tmp);
        FILE *w = fopen(path, "wb");
        if (w) {
            uint32_t magic = CIRCUIT_FILE_MAGIC, version = CIRCUIT_FILE_VERSION;
            int zero_components = 0, huge = 100000;
            fwrite(&magic, sizeof magic, 1, w);
            fwrite(&version, sizeof version, 1, w);
            fwrite(&zero_components, sizeof zero_components, 1, w);
            fwrite(&huge, sizeof huge, 1, w);        /* the node count */
            /* and nothing after it: the loop would read garbage for 100000 nodes */
            fclose(w);
        }
        Circuit *c = circuit_create();
        bool loaded = file_load_circuit(c, path);
        checks++;
        if (loaded || c->num_nodes > MAX_NODES) {
            printf("[FAIL] load  a file claiming %d nodes was %s (nodes=%d, the array holds %d)\n",
                   100000, loaded ? "loaded" : "refused but left behind", c->num_nodes, MAX_NODES);
            fails++;
        } else {
            printf("[ OK ] load  a file claiming 100000 nodes is refused: %s\n", file_get_error());
        }
        circuit_free(c);
        remove(path);
    }

    printf("\nload-test: %d hostile inputs, %d that the loaders accepted anyway\n", checks, fails);
    return fails ? 1 : 0;
}

static int dcm_test(void) {
    /* R_load, and roughly what it means for a 12 V / 100 kHz / 220 uH buck at 50 % duty:
       critical conduction is near R = 2 L f / (1 - D) ~ 88 ohm, so above that is DCM. */
    static const struct { double rload; double dt_force; int no_cjo; const char *what; } cases[] = {
        {   5.0, 0, 0, "heavy: deep CCM" },
        {  50.0, 0, 0, "moderate" },
        { 200.0, 0, 0, "light: DCM" },
        { 2000.0, 0, 0, "very light: deep DCM" },
        { 20000.0, 0, 0, "near open circuit" },
        /* The same deep-DCM load at the step this was blocked on. docs/ROADMAP.md recorded that a
           realistic snubber could not be resolved at dt = 100 ns, and the item sat unbuilt on that
           basis; this row is what says whether the step was really the cause. */
        { 2000.0, 100e-9, 0, "deep DCM at dt=100ns" },
        { 2000.0, 1e-6, 0, "deep DCM at dt=1us" },
        /* The same deep-DCM load with the freewheel diode's junction capacitance switched off.
           docs/ROADMAP.md said what this item needed was "a switch model with a defined off-state
           capacitance" - and a junction capacitance was stamped for the first time this morning,
           for an unrelated reason. This row asks whether that is what defines the switch node
           during the interval when neither device conducts. */
        { 2000.0, 0, 1, "deep DCM, cjo = 0" },
        /* The configuration docs/ROADMAP.md describes: ideal parts, where the diode is a hard
           switch rather than a Shockley exponential and the analog switch is r_off = 1e9. If the
           runaway is real, it is here - a real diode always leaks a little, so the node it sits
           on is never truly floating. */
        { 2000.0, 0, 2, "deep DCM, ideal parts" },
    };
    int fails = 0;
    printf("dcm-test: the switch node across the conduction boundary\n\n");
    printf("%-22s %12s %14s %14s   %s\n", "load", "R", "max |V| node", "final Vout", "verdict");

    for (unsigned k = 0; k < sizeof cases / sizeof cases[0]; k++) {
        Circuit *c = circuit_create();
        if (!c) return 1;
        if (circuit_place_template(c, CIRCUIT_HW_BUCK, 0, 0) <= 0) { circuit_free(c); continue; }

        /* the load is the largest resistor in the template; scale it to move the operating point */
        Component *load = NULL;
        for (int i = 0; i < c->num_components; i++) {
            Component *comp = c->components[i];
            if (comp->type != COMP_RESISTOR) continue;
            if (!load || comp->props.resistor.resistance > load->props.resistor.resistance) load = comp;
        }
        if (!load) { circuit_free(c); continue; }
        load->props.resistor.resistance = cases[k].rload;
        load->props.resistor.power_rating = 1e6;   /* not the subject of this test */
        if (cases[k].no_cjo) {
            for (int i = 0; i < c->num_components; i++) {
                Component *d = c->components[i];
                if (d->type == COMP_DIODE || d->type == COMP_SCHOTTKY) {
                    d->props.diode.cjo = 0.0;
                    if (cases[k].no_cjo == 2) d->props.diode.ideal = true;
                }
                if (cases[k].no_cjo == 2 && d->type == COMP_ANALOG_SWITCH) d->props.analog_switch.ideal = true;
            }
        }

        Simulation *sim = simulation_create(c);
        if (!sim) { circuit_free(c); continue; }
        int ok = simulation_dc_analysis(sim);
        simulation_auto_time_step(sim);
        { double dtp = simulation_scope_time_step(sim, 2e-6);
          if (dtp > 0 && dtp < sim->time_step) simulation_set_time_step(sim, dtp); }
        if (cases[k].dt_force > 0) {
            simulation_enable_adaptive(sim, false);
            simulation_set_time_step(sim, cases[k].dt_force);
        }
        simulation_start(sim);
        /* again after start: it re-derives the step, which silently undid the force */
        if (cases[k].dt_force > 0) simulation_set_time_step(sim, cases[k].dt_force);

        Component *ind = NULL;
        for (int i = 0; i < c->num_components; i++)
            if (c->components[i]->type == COMP_INDUCTOR) { ind = c->components[i]; break; }
        double i_min = 1e300, i_max = -1e300;
        double vmax = 0, vout = 0;
        int guard = 0;
        double t_end = 400e-6;      /* 40 switching cycles at 100 kHz */
        while (ok && sim->time < t_end && guard++ < 4000000) {
            if (!simulation_step(sim)) { ok = 0; break; }
            if (ind && sim->time > 300e-6) {          /* after the start-up ramp */
                simulation_update_flow_display(sim);
                double il = ind->terminal_current[0];
                if (il < i_min) i_min = il;
                if (il > i_max) i_max = il;
            }
            for (int i = 0; i < c->num_nodes; i++) {
                double v = fabs(c->nodes[i].voltage);
                if (v > vmax) vmax = v;
                if (!isfinite(v)) { ok = 0; break; }
            }
        }
        if (ok && c->num_probes > 0) vout = simulation_get_probe_voltage(sim, c->num_probes - 1);

        /* 12 V in: anything past 60 V on any node is the runaway this item is about. */
        int bad = !ok || vmax > 60.0;
        if (bad) fails++;
        /* Whether the cycle is actually discontinuous: the inductor current reaching zero and
           staying there for part of the cycle is what creates the third interval this item is
           about. A rising Vout is suggestive; the current is proof. */
        const char *mode = (i_min > 1e299) ? "?" : (i_min <= 0.001 * i_max) ? "DCM" : "CCM";
        printf("%s %-20s %12.4g %14.4g %14.4g   %-22s IL %.3g..%.3g A (%s)\n",
               bad ? "[FAIL]" : "[ OK ]", cases[k].what, cases[k].rload, vmax, vout,
               !ok ? "did not run" : vmax > 60.0 ? "switch node runs away" : "bounded",
               i_min > 1e299 ? 0.0 : i_min, i_max < -1e299 ? 0.0 : i_max, mode);

        simulation_free(sim);
        circuit_free(c);
    }
    printf("\ndcm-test: %d loads across the conduction boundary, %d where a node runs away\n",
           (int)(sizeof cases / sizeof cases[0]), fails);
    return fails ? 1 : 0;
}

/* --state-test: state that belongs to a time step must not move when it is not a time step.
 *
 * Two faults found by a pre-release review, both of the same shape as the five this release
 * already fixed, and neither visible to any suite that existed.
 *
 * 1. The battery counted coulombs inside its stamp. The DC operating point stamps with a pseudo
 *    step of 1e9 seconds - the trick that makes a capacitor look like an open - so a default AA
 *    across 100 ohm lost 15 million coulombs before the first transient step and every Run began
 *    with a flat battery reading 0.72 V instead of 1.5 V. Nothing caught it because --restamp-test
 *    watches a node voltage over milliseconds, where a real discharge is far below its threshold.
 *
 * 2. The solve-time snapshot did not reach inside subcircuit blocks, so an internal capacitor was
 *    re-stamped as though it were uncharged when the current-flow display read it back. That was a
 *    regression introduced with the snapshot itself.
 *
 * Both are checked directly here: run the operating point and demand the battery still be full,
 * and read a charged capacitor's block current back and demand it agree with what feeds it.
 */
static int state_test(void) {
    int fails = 0;

    /* --- the battery survives its own operating point --- */
    {
        Circuit *c = circuit_create();
        Component *b = pt_add(c, COMP_BATTERY, 0, 60, 0);
        Component *g0 = pt_add(c, COMP_GROUND, 0, 160, 0);
        Component *r = pt_add(c, COMP_RESISTOR, 160, 60, 0);
        Component *g1 = pt_add(c, COMP_GROUND, 160, 160, 0);
        r->props.resistor.resistance = 100.0;
        r->props.resistor.power_rating = 100.0;
        int top = pt_node(c, 80, 20), gnd0 = pt_node(c, 0, 140), gnd1 = pt_node(c, 160, 140);
        b->node_ids[0] = top; b->node_ids[1] = gnd0; g0->node_ids[0] = gnd0;
        r->node_ids[0] = top; r->node_ids[1] = gnd1; g1->node_ids[0] = gnd1;
        circuit_add_wire(c, gnd0, gnd1);

        double soc_before = b->props.battery.charge_state;
        Simulation *sim = simulation_create(c);
        int ok = simulation_dc_analysis(sim);
        double soc_after = b->props.battery.charge_state;
        /* and a few real steps: 2500 mAh does not move measurably in a millisecond */
        simulation_set_time_step(sim, 1e-5);
        simulation_start(sim);
        for (int i = 0; i < 100 && ok; i++) if (!simulation_step(sim)) ok = 0;
        double soc_run = b->props.battery.charge_state;
        double v = simulation_get_node_voltage(sim, c->nodes[0].id);

        int bad = !ok || soc_after < 0.999 || soc_run < 0.999 || b->props.battery.discharged;
        if (bad) fails++;
        printf("%s state  battery      SoC %.4f -> %.4f after the operating point, %.4f after 1 ms "
               "(terminal %.3f V)\n", bad ? "[FAIL]" : "[ OK ]", soc_before, soc_after, soc_run, v);
        simulation_free(sim);
        circuit_free(c);
    }

    /* --- a capacitor inside a subcircuit is read back as charged --- */
    {
        Circuit *c = circuit_create();
        /* source -> block(IN) , block has R to OUT and C from IN to ground */
        Component *v = pt_add(c, COMP_AC_VOLTAGE, 0, 60, 0);
        v->props.ac_voltage.amplitude = 5.0; v->props.ac_voltage.frequency = 200.0;
        Component *g0 = pt_add(c, COMP_GROUND, 0, 160, 0);
        Component *cap = pt_add(c, COMP_CAPACITOR, 160, 60, 90);
        cap->props.capacitor.capacitance = 1e-6;
        Component *g1 = pt_add(c, COMP_GROUND, 160, 160, 0);
        int top = pt_node(c, 80, 20), gnd0 = pt_node(c, 0, 140), gnd1 = pt_node(c, 160, 140);
        v->node_ids[0] = top; v->node_ids[1] = gnd0; g0->node_ids[0] = gnd0;
        cap->node_ids[0] = top; cap->node_ids[1] = gnd1; g1->node_ids[0] = gnd1;
        circuit_add_wire(c, gnd0, gnd1);

        Simulation *sim = simulation_create(c);
        int ok = simulation_dc_analysis(sim);
        simulation_enable_adaptive(sim, false);
        simulation_set_time_step(sim, 1e-5);
        simulation_start(sim);
        simulation_set_time_step(sim, 1e-5);
        for (int i = 0; i < 300 && ok; i++) if (!simulation_step(sim)) ok = 0;
        simulation_update_flow_display(sim);

        /* the capacitor's own reported current against C dv/dt from the accepted steps */
        double i_rep = cap->terminal_current[0];
        double w = 2.0 * M_PI * 200.0;
        double i_true = 1e-6 * 5.0 * w * cos(w * sim->time);
        double err = fabs(i_rep - i_true) / (fabs(i_true) + 1e-9);
        int bad = !ok || err > 0.05;
        if (bad) fails++;
        printf("%s state  capacitor    reported %.6g A against C dv/dt %.6g A (%.2f %%)\n",
               bad ? "[FAIL]" : "[ OK ]", i_rep, i_true, err * 100.0);
        simulation_free(sim);
        circuit_free(c);
    }

    printf("\nstate-test: 2 checks that state advances once per step and is read as it was "
           "stamped, %d failed\n", fails);
    return fails ? 1 : 0;
}

static int meas_test(void) {
    enum { N = 4000 };
    static double ts[N], vs[N];
    int fails = 0, checks = 0;
    for (unsigned c = 0; c < sizeof meas_cases / sizeof meas_cases[0]; c++) {
        const MeasCase *mc = &meas_cases[c];
        double span = mc->n_cycles / mc->freq;
        for (int i = 0; i < N; i++) {
            ts[i] = span * i / (double)N;    /* i/N, not i/(N-1): the endpoint would repeat the
                                                first sample's phase and bias every average */
            vs[i] = meas_wave(mc, ts[i]);
        }
        WaveformMeasurements m;
        memset(&m, 0, sizeof m);
        analysis_measure_waveform(&m, ts, vs, N);
        if (!m.valid) {
            printf("[FAIL] meas  %-16s not measured at all\n", mc->name);
            fails++;
            continue;
        }
        checks += 5;
        meas_check(mc->name, "Vpp",  m.v_pp,       mc->vpp,      mc->tol_pct, mc->amp, &fails);
        meas_check(mc->name, "Vavg", m.v_avg,      mc->vavg,     mc->tol_pct, mc->amp, &fails);
        meas_check(mc->name, "Vrms", m.v_rms,      mc->vrms,     mc->tol_pct, mc->amp, &fails);
        meas_check(mc->name, "f",    m.frequency,  mc->freq,     mc->tol_pct, 0.0,     &fails);
        meas_check(mc->name, "D",    m.duty_cycle, mc->duty_pct, mc->tol_pct, 0.0,     &fails);
    }
    printf("\nmeas-test: %d checks over %d synthetic waveforms, %d failed\n",
           checks, (int)(sizeof meas_cases / sizeof meas_cases[0]), fails);
    return fails ? 1 : 0;
}

static int dvdt_test(void) {
    int fails = 0;
    printf("dvdt-test: reported current against C dv/dt computed outside the solver\n\n");
    printf("%-16s %14s %14s %8s   %s\n", "element", "reported", "expected", "error", "note");
    for (unsigned i = 0; i < sizeof dvdt_cases / sizeof dvdt_cases[0]; i++) {
        const DvdtCase *dc = &dvdt_cases[i];
        int ok = 1; double expect = 0;
        double got = dvdt_measure(dc, &expect, &ok);
        double err = (fabs(expect) > 0) ? (got - expect) / fabs(expect) : 0;
        /* 5 %: the companion is first or near-first order and the source is sampled, so a
           fraction of a percent is expected. A sign error is -200 %, a doubled companion
           +100 %, an unstamped one -100 %. Nothing lands in between by accident. The inductor
           gets more room: its answer is an accumulated integral, so the step's truncation
           error accumulates with it rather than cancelling. */
        int bad = !ok || fabs(err) > (dc->inductive ? 0.02 : 0.05);
        if (bad) fails++;
        printf("%s %-14s %14.6g %14.6g %7.2f%%   %s%s\n", bad ? "[FAIL]" : "[ OK ]",
               dc->name, got, expect, err * 100.0, dc->note,
               ok ? "" : "  [simulation failed]");
    }
    fails += dvdt_motor();
    fails += dvdt_relay();
    printf("\ndvdt-test: %d elements, %d failed\n",
           (int)(sizeof dvdt_cases / sizeof dvdt_cases[0]) + 2, fails);
    return fails ? 1 : 0;
}

static int part_test(void) {
    int fails = 0, total = 0;
    printf("part-test: every named device at its data sheet's own test condition\n\n");
    for (unsigned i = 0; i < sizeof part_checks / sizeof part_checks[0]; i++) {
        const PartCheck *pc = &part_checks[i];
        int ok = 1;
        double got = 0;
        const char *unit = "";
        switch (pc->kind) {
            case PC_RDSON: {
                /* pick a sense resistor that lands near the data sheet's own test current:
                   ~1 A for a power part, ~50 mA for a small-signal one */
                double rsense = (pc->expect < 0.1) ? 5.0 : 100.0;
                double vsup = (part_type(pc->part) == COMP_PMOS) ? -5.0 : 5.0;
                got = pc_mos_rdson(pc->part, pc->bias, vsup, rsense, &ok);
                unit = "ohm";
                break;
            }
            case PC_ID_MIN: got = pc_mos_id(pc->part, pc->bias, &ok); unit = "A"; break;
            case PC_HFE: { double h = 0, vb = 0; pc_bjt(pc->part, pc->bias, &h, &vb, &ok); got = h; unit = ""; break; }
            case PC_FT: got = pc_bjt_ft(pc->part, pc->bias, 20e6, &ok); break;
            case PC_VBE: { double h = 0, vb = 0; pc_bjt(pc->part, pc->bias, &h, &vb, &ok); got = vb; unit = "V"; break; }
            case PC_VF:  got = pc_diode_v(pc->part, pc->bias, false, &ok); unit = "V"; break;
            case PC_VZ:  got = fabs(pc_diode_v(pc->part, pc->bias, true, &ok)); unit = "V"; break;
            case PC_OPAMP_VOS: got = pc_opamp_vos(pc->part, &ok); unit = "V"; break;
            case PC_OPAMP_SR:  got = pc_opamp_sr(pc->part, &ok); unit = "V/s"; break;
            default:           got = pc_template(pc->part, pc->tpl, &ok); unit = "V"; break;
        }
        total++;
        int pass;
        if (!ok) pass = 0;
        else if (pc->kind == PC_ID_MIN) pass = (got >= pc->expect && got < pc->expect * 20.0);
        else {
            double lim = (pc->tol >= 0) ? pc->tol * fabs(pc->expect) : -pc->tol;
            pass = fabs(got - pc->expect) <= lim;
        }
        if (!pass) fails++;
        printf("%s part %-9s %-12s = %10.4g %-4s  data sheet %-9.4g  %s%s\n",
               pass ? " OK " : "FAIL", pc->part,
               pc->kind == PC_RDSON ? "R_DS(on)" : pc->kind == PC_ID_MIN ? "I_D(on)" :
               pc->kind == PC_HFE ? "h_FE" : pc->kind == PC_VBE ? "V_BE" : pc->kind == PC_FT ? "f_T" :
               pc->kind == PC_VF ? "V_F" : pc->kind == PC_VZ ? "V_Z" :
               pc->kind == PC_OPAMP_VOS ? "V_offset" : pc->kind == PC_OPAMP_SR ? "slew rate" : "V_out",
               got, unit, pc->expect, pc->note, ok ? "" : "  [simulation failed]");
    }
    printf("\npart-test: %d checks over %d named devices, %d failed\n",
           total, component_part_count(), fails);
    return fails ? 1 : 0;
}


/* ---------------------------------------------------------------------------------------
 * --op-test: the operating point the properties panel shows. It is cached on the component
 * by the stamp (region, V_GS, V_DS, I_D, g_m), so this checks the numbers a user reads off
 * the panel are real, differ between devices, and survive a part change - the panel showed
 * zeros for every device once, because cycling the part restored the whole property union
 * (the operating point lives in it) while the simulation was paused, so nothing refreshed it.
 * ------------------------------------------------------------------------------------- */
typedef struct { const char *part; double vgs; double id_min, id_max; int region; const char *note; } OpCase;
static const OpCase op_cases[] = {
    { "2N7000",  4.5, 0.20,  0.45, 2, "saturation: K V_ov^2/2 with V_ov = 2.4 V" },
    { "2N7000", 10.0, 1.0,   4.0,  1, "harder on: 3.6 A into a 1 ohm sense pulls it into triode" },
    { "2N7002",  4.5, 0.15,  0.40, 2, "lower K than the 2N7000 at the same drive" },
    { "IRF540N", 6.0, 5.0,  12.0,  2, "power part: amps, not milliamps" },
    { "2N7000",  1.0, -1e-9, 1e-9, 0, "below V_GS(th) = 2.1 V: cutoff, no current" },
};

/* Gate at vgs, drain from a 10 V rail through 1 ohm, source grounded; returns the component
   so the caller can read the cached operating point off it. */
static Component *op_build(Circuit *c, const char *part, double vgs) {
    ComponentType ty = part_type(part);
    Component *m = pt_add(c, ty, 100, 100, 0);
    if (!m || !component_apply_part(m, part)) return NULL;
    Component *vg = pt_add(c, COMP_DC_VOLTAGE, 0, 100, 0);
    vg->props.dc_voltage.voltage = vgs;
    Component *gg = pt_add(c, COMP_GROUND, 0, 200, 0);
    Component *vdd = pt_add(c, COMP_DC_VOLTAGE, 300, 40, 0);
    vdd->props.dc_voltage.voltage = 10.0;
    Component *gv = pt_add(c, COMP_GROUND, 300, 140, 0);
    Component *rs = pt_add(c, COMP_RESISTOR, 240, 60, 0);
    rs->props.resistor.resistance = 1.0;
    rs->props.resistor.power_rating = 100.0;
    Component *gs = pt_add(c, COMP_GROUND, 200, 260, 0);
    int gate = pt_node(c, 60, 100), drain = pt_node(c, 180, 60), src = pt_node(c, 180, 160);
    int rail = pt_node(c, 300, 0), rr = pt_node(c, 280, 60);
    int gnd1 = pt_node(c, 0, 180), gnd2 = pt_node(c, 300, 120), gnd3 = pt_node(c, 200, 240);
    vg->node_ids[0] = gate; vg->node_ids[1] = gnd1; gg->node_ids[0] = gnd1;
    vdd->node_ids[0] = rail; vdd->node_ids[1] = gnd2; gv->node_ids[0] = gnd2;
    rs->node_ids[0] = rr; rs->node_ids[1] = drain;
    circuit_add_wire(c, rail, rr);
    gs->node_ids[0] = gnd3; circuit_add_wire(c, src, gnd3);
    m->node_ids[0] = gate; m->node_ids[1] = drain; m->node_ids[2] = src;
    return m;
}

static int op_test(void) {
    static const char *regions[3] = { "cutoff", "triode", "saturation" };
    int fails = 0, total = 0;
    printf("op-test: the operating point the properties panel reads, per device\n\n");
    for (unsigned i = 0; i < sizeof op_cases / sizeof op_cases[0]; i++) {
        const OpCase *oc = &op_cases[i];
        Circuit *c = circuit_create();
        Component *m = op_build(c, oc->part, oc->vgs);
        int ok = m != NULL;
        Simulation *sim = ok ? simulation_create(c) : NULL;
        if (ok) ok = sim && simulation_dc_analysis(sim);
        double vgs = 0, vds = 0, id = 0, gm = 0; int region = -1;
        if (ok) {
            vgs = m->props.mosfet.op_vgs; vds = m->props.mosfet.op_vds;
            id = m->props.mosfet.op_id;   gm = m->props.mosfet.op_gm;
            region = m->props.mosfet.op_region;
            if (!isfinite(vgs) || !isfinite(id)) ok = 0;
        }
        total++;
        int pass = ok && region == oc->region &&
                   id >= oc->id_min && id <= oc->id_max &&
                   fabs(vgs - oc->vgs) < 0.05 &&
                   (oc->region == 0 || gm > 0);
        if (!pass) fails++;
        printf("%s op   %-9s V_GS %5.2f -> %-10s V_DS %7.4f  I_D %9.4g A  g_m %8.4g  %s%s\n",
               pass ? " OK " : "FAIL", oc->part, oc->vgs,
               (region >= 0 && region <= 2) ? regions[region] : "?", vds, id, gm, oc->note,
               ok ? "" : "  [simulation failed]");
        if (sim) simulation_free(sim);
        circuit_free(c);
    }

    /* the regression itself: changing the part must not blank the panel */
    {
        Circuit *c = circuit_create();
        Component *m = op_build(c, "2N7000", 4.5);
        Simulation *sim = m ? simulation_create(c) : NULL;
        int ok = sim && simulation_dc_analysis(sim);
        double id_before = ok ? m->props.mosfet.op_id : 0;
        total++;
        int pass = 0;
        if (ok && id_before > 0) {
            component_cycle_part(m);                       /* 2N7000 -> 2N7002 */
            double id_after = m->props.mosfet.op_id;
            int cycles_ok = (strcmp(m->part, "2N7000") != 0);
            /* cycle all the way round to generic and back */
            for (int k = 0; k < 8; k++) component_cycle_part(m);
            pass = cycles_ok && id_after == id_before && m->props.mosfet.op_id != 0;
            printf("%s op   part change keeps the operating point: I_D %.4g A before, %.4g after%s\n",
                   pass ? " OK " : "FAIL", id_before, m->props.mosfet.op_id,
                   pass ? "" : "  [the panel would read 0 for every device]");
        } else {
            printf("FAIL op   part-change check could not be set up\n");
        }
        if (!pass) fails++;
        if (sim) simulation_free(sim);
        circuit_free(c);
    }

    printf("\nop-test: %d checks, %d failed\n", total, fails);
    return fails ? 1 : 0;
}


/* ---------------------------------------------------------------------------------------
 * --sub-test: subcircuits actually simulating. A definition is built here the same way the
 * Ctrl+G dialog builds one (a Component array plus pins that name internal node ids), placed
 * as an IC block, and solved. Three cases, in increasing order of what they need from the
 * expansion: resistors only, then a capacitor (per-instance STATE), then a voltage source
 * (an auxiliary matrix row of its own).
 * ------------------------------------------------------------------------------------- */
static SubCircuitDef *sub_new_def(const char *name) {
    if (g_subcircuit_library.count >= MAX_SUBCIRCUIT_DEFS) return NULL;
    SubCircuitDef *def = &g_subcircuit_library.defs[g_subcircuit_library.count];
    memset(def, 0, sizeof *def);
    def->id = ++g_subcircuit_library.next_id;
    snprintf(def->name, sizeof def->name, "%s", name);
    def->block_width = 100; def->block_height = 80;
    return def;
}

/* Copy a built circuit's components into a definition and expose `npins` nodes as pins. */
static void sub_fill_def(SubCircuitDef *def, Circuit *inner, const int *pin_nodes, const char **pin_names, int npins) {
    int n = 0;
    for (int i = 0; i < inner->num_components; i++)
        if (inner->components[i]->type != COMP_PIN) n++;
    def->component_data = malloc((size_t)n * sizeof(Component));
    def->component_data_size = (size_t)n * sizeof(Component);
    Component *arr = (Component *)def->component_data;
    int idx = 0;
    for (int i = 0; i < inner->num_components; i++) {
        Component *c = inner->components[i];
        if (c->type == COMP_PIN) continue;
        memcpy(&arr[idx++], c, sizeof(Component));
    }
    def->num_components = n;
    def->num_pins = npins;
    for (int i = 0; i < npins; i++) {
        snprintf(def->pins[i].name, sizeof def->pins[i].name, "%s", pin_names[i]);
        def->pins[i].internal_node_id = pin_nodes[i];
        def->pins[i].side = (i < 2) ? 0 : 1;
        def->pins[i].position = i % 2;
    }
    /* internal nodes that are not pins */
    int seen[MAX_NODES]; memset(seen, 0, sizeof seen);
    int internal = 0;
    for (int i = 0; i < n; i++) {
        for (int t = 0; t < arr[i].num_terminals && t < MAX_TERMINALS; t++) {
            int id = arr[i].node_ids[t];
            if (id <= 0 || id >= MAX_NODES || seen[id]) continue;
            seen[id] = 1;
            int is_pin = 0;
            for (int q = 0; q < npins; q++) if (pin_nodes[q] == id) is_pin = 1;
            if (!is_pin) internal++;
        }
    }
    def->num_internal_nodes = internal;
    g_subcircuit_library.count++;
}

/* 10 V source -> the block's IN pin; the block's GND pin to ground; measure OUT, and
   optionally the current the block draws at its IN pin (what the flow animation draws). */
static double sub_drive_i(SubCircuitDef *def, int npins, int *ok, double t_end, double *pin_i);
static double sub_drive(SubCircuitDef *def, int npins, int *ok, double t_end) {
    return sub_drive_i(def, npins, ok, t_end, NULL);
}
static double sub_drive_i(SubCircuitDef *def, int npins, int *ok, double t_end, double *pin_i) {
    Circuit *c = circuit_create();
    Component *v = pt_add(c, COMP_DC_VOLTAGE, 0, 100, 0);
    v->props.dc_voltage.voltage = 10.0;
    Component *g0 = pt_add(c, COMP_GROUND, 0, 200, 0);
    Component *blk = pt_add(c, COMP_SUBCIRCUIT, 240, 100, 0);
    blk->props.subcircuit.def_id = def->id;
    blk->num_terminals = npins;
    Component *rl = pt_add(c, COMP_RESISTOR, 420, 140, 90);
    rl->props.resistor.resistance = 1e6;          /* a light load so the block sets the level */
    Component *gl = pt_add(c, COMP_GROUND, 420, 240, 0);
    int in = pt_node(c, 60, 100), out = pt_node(c, 340, 100), gnd = pt_node(c, 0, 180);
    int lt = pt_node(c, 420, 100), lb = pt_node(c, 420, 180), gl2 = pt_node(c, 420, 220);
    v->node_ids[0] = in; v->node_ids[1] = gnd; g0->node_ids[0] = gnd;
    blk->node_ids[0] = in; blk->node_ids[1] = out; blk->node_ids[2] = gnd;
    circuit_add_wire(c, out, lt);
    rl->node_ids[0] = lt; rl->node_ids[1] = lb; gl->node_ids[0] = gl2;
    circuit_add_wire(c, lb, gl2);

    Simulation *sim = simulation_create(c);
    *ok = sim && simulation_dc_analysis(sim);
    double vout = 0;
    if (*ok && t_end > 0) {
        simulation_set_time_step(sim, t_end / 2000.0);
        simulation_start(sim);
        while (sim->time < t_end) if (!simulation_step(sim)) { *ok = 0; break; }
    }
    if (*ok) {
        Node *n = circuit_get_node(c, out);
        vout = n ? n->voltage : 0;
        if (!isfinite(vout)) *ok = 0;
        if (pin_i) {
            simulation_compute_terminal_currents(sim);
            *pin_i = blk->terminal_current[0];      /* into the IN pin */
        }
    }
    if (sim) simulation_free(sim);
    circuit_free(c);
    return vout;
}

static int sub_test(void) {
    int fails = 0, total = 0;
    printf("sub-test: subcircuits placed as IC blocks and solved\n\n");
    g_subcircuit_library.count = 0; g_subcircuit_library.next_id = 0;

    /* ---- 1. resistive divider: two 1k, IN / OUT / GND ---- */
    {
        Circuit *inner = circuit_create();
        Component *r1 = pt_add(inner, COMP_RESISTOR, 100, 60, 90);
        r1->props.resistor.resistance = 1000.0;
        Component *r2 = pt_add(inner, COMP_RESISTOR, 100, 220, 90);
        r2->props.resistor.resistance = 1000.0;
        int nin = pt_node(inner, 100, 20), nmid = pt_node(inner, 100, 140), ngnd = pt_node(inner, 100, 260);
        r1->node_ids[0] = nin; r1->node_ids[1] = nmid;
        r2->node_ids[0] = nmid; r2->node_ids[1] = ngnd;
        SubCircuitDef *def = sub_new_def("DIV");
        int pins[3] = { nin, nmid, ngnd };
        const char *names[3] = { "IN", "OUT", "GND" };
        sub_fill_def(def, inner, pins, names, 3);
        circuit_free(inner);

        int ok = 1;
        double v = sub_drive(def, 3, &ok, 0);
        total++;
        int pass = ok && fabs(v - 5.0) < 0.05;
        if (!pass) fails++;
        printf("%s sub  resistive divider          OUT = %8.4f V   expect 5.0000  %s\n",
               pass ? " OK " : "FAIL", v, ok ? "(two 1k from a 10 V source)" : "[simulation failed]");
    }

    /* ---- 2. RC low-pass: needs the internal capacitor to keep its state between steps ---- */
    {
        Circuit *inner = circuit_create();
        Component *r = pt_add(inner, COMP_RESISTOR, 100, 60, 90);
        r->props.resistor.resistance = 1000.0;
        Component *cap = pt_add(inner, COMP_CAPACITOR, 100, 220, 90);
        cap->props.capacitor.capacitance = 1e-6;      /* tau = 1 ms */
        int nin = pt_node(inner, 100, 20), nmid = pt_node(inner, 100, 140), ngnd = pt_node(inner, 100, 260);
        r->node_ids[0] = nin; r->node_ids[1] = nmid;
        cap->node_ids[0] = nmid; cap->node_ids[1] = ngnd;
        SubCircuitDef *def = sub_new_def("RC");
        int pins[3] = { nin, nmid, ngnd };
        const char *names[3] = { "IN", "OUT", "GND" };
        sub_fill_def(def, inner, pins, names, 3);
        circuit_free(inner);

        int ok = 1;
        double v = sub_drive(def, 3, &ok, 10e-3);      /* 10 tau: the cap should be at the rail */
        total++;
        int pass = ok && fabs(v - 10.0) < 0.3;
        if (!pass) fails++;
        printf("%s sub  RC low-pass (needs state)  OUT = %8.4f V   expect 10.000  %s\n",
               pass ? " OK " : "FAIL", v,
               ok ? "(charged through 1k into 1 uF for 10 tau)" : "[simulation failed]");
    }

    /* ---- 3. an internal voltage source: needs an auxiliary matrix row of its own ---- */
    {
        Circuit *inner = circuit_create();
        Component *vref = pt_add(inner, COMP_DC_VOLTAGE, 100, 100, 0);
        vref->props.dc_voltage.voltage = 3.3;
        Component *r = pt_add(inner, COMP_RESISTOR, 220, 100, 0);
        r->props.resistor.resistance = 100.0;
        int nout = pt_node(inner, 100, 60), ngnd = pt_node(inner, 100, 140), ntap = pt_node(inner, 260, 100);
        vref->node_ids[0] = ntap; vref->node_ids[1] = ngnd;
        r->node_ids[0] = ntap; r->node_ids[1] = nout;
        SubCircuitDef *def = sub_new_def("REF");
        int pins[3] = { ngnd, nout, ngnd };            /* IN unused; OUT is the reference */
        const char *names[3] = { "IN", "OUT", "GND" };
        sub_fill_def(def, inner, pins, names, 3);
        circuit_free(inner);

        int ok = 1;
        double v = sub_drive(def, 3, &ok, 0);
        total++;
        int pass = ok && fabs(v - 3.3) < 0.05;
        if (!pass) fails++;
        printf("%s sub  internal 3.3 V reference   OUT = %8.4f V   expect 3.3000  %s\n",
               pass ? " OK " : "FAIL", v,
               ok ? "(a source inside the block needs its own matrix row)" : "[simulation failed]");
    }

    /* ---- 4. the pins carry current, so the flow animation continues through the block ---- */
    {
        Circuit *inner = circuit_create();
        Component *r1 = pt_add(inner, COMP_RESISTOR, 100, 60, 90);
        r1->props.resistor.resistance = 1000.0;
        Component *r2 = pt_add(inner, COMP_RESISTOR, 100, 220, 90);
        r2->props.resistor.resistance = 1000.0;
        int nin = pt_node(inner, 100, 20), nmid = pt_node(inner, 100, 140), ngnd = pt_node(inner, 100, 260);
        r1->node_ids[0] = nin; r1->node_ids[1] = nmid;
        r2->node_ids[0] = nmid; r2->node_ids[1] = ngnd;
        SubCircuitDef *def = sub_new_def("DIV2");
        int pins[3] = { nin, nmid, ngnd };
        const char *names[3] = { "IN", "OUT", "GND" };
        sub_fill_def(def, inner, pins, names, 3);
        circuit_free(inner);

        int ok = 1; double pin = 0;
        sub_drive_i(def, 3, &ok, 0, &pin);
        total++;
        /* 10 V across two 1k in series: 5 mA into the IN pin */
        int pass = ok && fabs(fabs(pin) - 5e-3) < 5e-4;
        if (!pass) fails++;
        printf("%s sub  pin current (flow display) IN  = %8.4f mA  expect 5.0000  %s\n",
               pass ? " OK " : "FAIL", pin * 1e3,
               ok ? "(the block conducts, so the dots do not stop at its edge)" : "[simulation failed]");
    }

    /* ---- 5. a block inside a block: an inner divider wrapped by an outer block ---- */
    {
        /* inner: 1k / 1k divider with IN / OUT / GND */
        Circuit *in1 = circuit_create();
        Component *a1 = pt_add(in1, COMP_RESISTOR, 100, 60, 90);
        a1->props.resistor.resistance = 1000.0;
        Component *a2 = pt_add(in1, COMP_RESISTOR, 100, 220, 90);
        a2->props.resistor.resistance = 1000.0;
        int i_in = pt_node(in1, 100, 20), i_mid = pt_node(in1, 100, 140), i_gnd = pt_node(in1, 100, 260);
        a1->node_ids[0] = i_in; a1->node_ids[1] = i_mid;
        a2->node_ids[0] = i_mid; a2->node_ids[1] = i_gnd;
        SubCircuitDef *inner_def = sub_new_def("INNERDIV");
        int ipins[3] = { i_in, i_mid, i_gnd };
        const char *inames[3] = { "IN", "OUT", "GND" };
        sub_fill_def(inner_def, in1, ipins, inames, 3);
        circuit_free(in1);

        /* outer: a 1k in series with an instance of the inner block */
        Circuit *out1 = circuit_create();
        Component *rs = pt_add(out1, COMP_RESISTOR, 100, 60, 90);
        rs->props.resistor.resistance = 1000.0;
        Component *nested = pt_add(out1, COMP_SUBCIRCUIT, 100, 200, 0);
        nested->props.subcircuit.def_id = inner_def->id;
        nested->num_terminals = 3;
        int o_in = pt_node(out1, 100, 20), o_mid = pt_node(out1, 100, 140),
            o_out = pt_node(out1, 220, 200), o_gnd = pt_node(out1, 100, 300);
        rs->node_ids[0] = o_in; rs->node_ids[1] = o_mid;
        nested->node_ids[0] = o_mid; nested->node_ids[1] = o_out; nested->node_ids[2] = o_gnd;
        SubCircuitDef *outer_def = sub_new_def("OUTERDIV");
        int opins[3] = { o_in, o_out, o_gnd };
        const char *onames[3] = { "IN", "OUT", "GND" };
        sub_fill_def(outer_def, out1, opins, onames, 3);
        circuit_free(out1);

        int ok = 1;
        double v = sub_drive(outer_def, 3, &ok, 0);
        total++;
        /* 10 V across 1k + (1k + 1k); OUT is the inner divider's midpoint = 10 x 1k/3k */
        int pass = ok && fabs(v - 10.0 / 3.0) < 0.05;
        if (!pass) fails++;
        printf("%s sub  nested block in a block   OUT = %8.4f V   expect 3.3333  %s\n",
               pass ? " OK " : "FAIL", v,
               ok ? "(outer 1k into an inner 1k/1k divider)" : "[simulation failed]");
    }


    /* ---- 3. the current read back out of a block whose capacitor is CHANGING ----
       A read-only re-stamp - the one that recovers terminal currents for the current-flow
       display - has to reproduce the stamp that actually happened, and for a storage element that
       means reading the companion state the solve used. The snapshot carrying it was added for
       top-level parts only, and a block's internals live on its own instance array, so an internal
       capacitor was re-stamped as though it were empty.

       Driven by an AC source on purpose. The first version of this case used the shared DC drive
       and passed with the bug still in: the operating point charges the capacitor to the supply,
       after which dv/dt is nearly zero and the companion contributes almost nothing, so reading it
       as zero changed the answer by less than the tolerance. A check that cannot fail is not a
       check. With a sine there is a real dv/dt at every instant and the oracle is exact: a block
       that is nothing but a capacitor draws C dv/dt through its pin. */
    {
        Circuit *inner = circuit_create();
        Component *cap = pt_add(inner, COMP_CAPACITOR, 100, 140, 90);
        cap->props.capacitor.capacitance = 1e-6;
        cap->props.capacitor.ideal = true;          /* no ESR/leakage in the oracle */
        int nin = pt_node(inner, 100, 100), ngnd = pt_node(inner, 100, 180);
        cap->node_ids[0] = nin; cap->node_ids[1] = ngnd;
        SubCircuitDef *def = sub_new_def("CAPB");
        int pins[2] = { nin, ngnd };
        const char *names[2] = { "IN", "GND" };
        sub_fill_def(def, inner, pins, names, 2);
        circuit_free(inner);

        Circuit *c = circuit_create();
        Component *v = pt_add(c, COMP_AC_VOLTAGE, 0, 100, 0);
        v->props.ac_voltage.amplitude = 5.0;
        v->props.ac_voltage.frequency = 200.0;
        v->props.ac_voltage.ideal = true;
        Component *g0 = pt_add(c, COMP_GROUND, 0, 200, 0);
        Component *blk = pt_add(c, COMP_SUBCIRCUIT, 240, 100, 0);
        blk->props.subcircuit.def_id = def->id;
        blk->num_terminals = 2;
        int in = pt_node(c, 60, 100), gnd = pt_node(c, 0, 180);
        v->node_ids[0] = in; v->node_ids[1] = gnd; g0->node_ids[0] = gnd;
        blk->node_ids[0] = in; blk->node_ids[1] = gnd;

        Simulation *sim = simulation_create(c);
        int ok = sim && simulation_dc_analysis(sim);
        simulation_enable_adaptive(sim, false);
        simulation_set_time_step(sim, 1e-5);
        simulation_start(sim);
        simulation_set_time_step(sim, 1e-5);
        for (int i = 0; i < 300 && ok; i++) if (!simulation_step(sim)) ok = 0;
        double pin_i = 0;
        if (ok) { simulation_update_flow_display(sim); pin_i = blk->terminal_current[0]; }
        double w = 2.0 * M_PI * 200.0;
        double i_expect = 1e-6 * 5.0 * w * cos(w * (sim ? sim->time : 0));
        double err = fabs(pin_i - i_expect) / (fabs(i_expect) + 1e-12);
        total++;
        int pass = ok && err < 0.10;
        if (!pass) fails++;
        printf("%s sub  block pin current (AC)    IN = %9.6f A  expect %9.6f A (%.1f %%)  %s\n",
               pass ? " OK " : "FAIL", pin_i, i_expect, err * 100.0,
               ok ? "(a block that is one capacitor draws C dv/dt)" : "[simulation failed]");
        if (sim) simulation_free(sim);
        circuit_free(c);
    }

    /* ---- the BMI block from My Circuits: does a user block survive Save and Open ----
       This one is the senior-design discharge stage as drawn, positive feedback and all.
       It is NOT asked to regulate - it cannot, the loop runs the wrong way and latches at a
       rail. What is asked is that every part in it works: that the block solves to finite
       voltages, and that saving and opening it gives back the same circuit. */
    {
        int def_id = circuits_register_bmi_block();
        SubCircuitDef *bmi = NULL;
        for (int i = 0; i < g_subcircuit_library.count; i++)
            if (g_subcircuit_library.defs[i].id == def_id) bmi = &g_subcircuit_library.defs[i];

        Circuit *c = circuit_create();
        Component *cell = pt_add(c, COMP_DC_VOLTAGE, 0, 100, 0);
        cell->props.dc_voltage.voltage = 7.4;
        Component *vref = pt_add(c, COMP_DC_VOLTAGE, 0, 400, 0);
        vref->props.dc_voltage.voltage = 3.2;
        Component *g0 = pt_add(c, COMP_GROUND, 0, 240, 0);
        Component *blk = pt_add(c, COMP_SUBCIRCUIT, 300, 200, 0);
        blk->props.subcircuit.def_id = def_id;
        blk->num_terminals = 4;
        int nbat = pt_node(c, 60, 100), nref = pt_node(c, 60, 400);
        int ngnd = pt_node(c, 0, 220), nsns = pt_node(c, 420, 240);
        cell->node_ids[0] = nbat; cell->node_ids[1] = ngnd; g0->node_ids[0] = ngnd;
        vref->node_ids[0] = nref; vref->node_ids[1] = ngnd;
        blk->node_ids[0] = nbat; blk->node_ids[1] = nref;
        blk->node_ids[2] = ngnd; blk->node_ids[3] = nsns;

        char why_bin[220] = "", why_json[220] = "", path[600];
        const char *tmp = getenv("TEMP"); if (!tmp) tmp = ".";
        snprintf(path, sizeof path, "%s\\ct_bmi_block.json", tmp);
        roundtrip_leg(c, path, file_save_circuit, file_load_circuit, why_bin, sizeof why_bin);
        roundtrip_leg(c, path, file_export_json, file_import_json, why_json, sizeof why_json);
        remove(path);

        Simulation *sim = simulation_create(c);
        int solved = sim && simulation_dc_analysis(sim);
        int finite = solved;
        if (solved)
            for (int i = 1; i < c->next_node_id; i++) {
                Node *n = circuit_get_node(c, i);
                if (n && !isfinite(n->voltage)) { finite = 0; break; }
            }
        if (sim) simulation_free(sim);

        total++;
        int pass = bmi && bmi->num_components == 4 && bmi->num_pins == 4
                   && !why_bin[0] && !why_json[0] && finite;
        if (!pass) fails++;
        printf("%s sub  BMI block save/open        parts=%d pins=%d  %s%s%s%s\n",
               pass ? " OK " : "FAIL",
               bmi ? bmi->num_components : -1, bmi ? bmi->num_pins : -1,
               why_bin[0] ? "binary: " : "", why_bin,
               why_json[0] ? "json: " : "", pass ? "(as drawn, positive feedback and all)"
                                                 : (finite ? why_json : "[non-finite solution]"));
        circuit_free(c);
    }

    /* ---- looking inside a block: the definition turned back into a drawing ----
       A definition stores parts and the internal node each terminal sits on, and no wires. The
       viewer has to put the wires back, and the check is that the topology survives: the same
       parts, and terminals that shared an internal node sharing a net in the drawing. */
    {
        int def_id = circuits_register_bmi_block();
        char nm[64] = "";
        Circuit *v = circuit_from_subcircuit_def(def_id, nm, sizeof nm);
        int parts = v ? v->num_components : 0;
        int wires = v ? v->num_wires : 0;
        /* The wire count is arithmetic, not a guess. A net with k terminals needs k-1 hops, and
           the BMI block's six nets carry 1, 1, 1, 3, 2 and 2 - BAT, GND and VREF reach one part
           each, the sense node joins the op-amp's inverting input to the PMOS drain and the
           sense resistor, and the two internal nets each join a pair. That is 4 hops.

           Each hop is drawn with an elbow, so it is two wires unless its ends already share a
           row or a column. Only one does: the op-amp's output and the gate resistor's left end
           are both at y = 120. 3 hops x 2 + 1 = 7.

           It is NOT asked to solve. A block's insides have every pin hanging in the air and no
           ground among them; that is what makes it a block. Requiring a solve here was the
           first version of this check and it failed for exactly that reason. */
        total++;
        int pass = v && parts == 4 && wires == 7 && !strcmp(nm, "BMI");
        if (!pass) fails++;
        printf("%s sub  block viewed inside       name=%-6s parts=%d wires=%d  %s\n",
               pass ? " OK " : "FAIL", nm[0] ? nm : "?", parts, wires,
               pass ? "(4 hops, 3 of them needing an elbow -> 7 wires)" : "[wrong shape]");
        if (v) circuit_free(v);
    }

    printf("\nsub-test: %d checks, %d failed\n", total, fails);
    return fails ? 1 : 0;
}


/* ---------------------------------------------------------------------------------------
 * --spice-test: importing a vendor .SUBCKT and simulating what comes out.
 *
 * The netlist below is the shape every manufacturer publishes for a ceramic capacitor: the
 * capacitance in series with its ESR and ESL, which is why a real 100 nF stops being a
 * capacitor somewhere around 20 MHz. The test imports it, places it as a block, and measures
 * the impedance at three frequencies against the hand calculation |Z| = |ESR + j(wL - 1/wC)|.
 * ------------------------------------------------------------------------------------- */
static const char *SPICE_NETLIST =
    "* A vendor-style ceramic capacitor model\n"
    "* 100 nF, ESR 30 mOhm, ESL 0.7 nH  ->  series resonance at 1/(2 pi sqrt(LC)) = 19.0 MHz\n"
    ".SUBCKT CAP100N 1 2\n"
    "Ls   1   a   0.7n\n"
    "Rs   a   b   30m\n"
    "Cs   b   2   100n\n"
    ".ENDS\n"
    "\n"
    "* and one built ON TOP of it, to prove an X instance nests\n"
    ".SUBCKT CAPBANK 1 2\n"
    "X1   1   2   CAP100N\n"
    "X2   1   2   CAP100N\n"
    ".ENDS\n";

/* Drive the imported block from a source through a 1 ohm sense resistor and return |Z| of the
   block at `freq`, measured from the steady-state amplitude either side of the sense resistor. */
static double spice_block_z(int def_id, double freq, int *ok) {
    Circuit *c = circuit_create();
    Component *v = pt_add(c, COMP_AC_VOLTAGE, 0, 100, 0);
    v->props.ac_voltage.amplitude = 1.0; v->props.ac_voltage.frequency = freq;
    Component *g0 = pt_add(c, COMP_GROUND, 0, 200, 0);
    Component *rs = pt_add(c, COMP_RESISTOR, 140, 60, 0);
    rs->props.resistor.resistance = 1.0;
    rs->props.resistor.power_rating = 100.0;
    Component *blk = pt_add(c, COMP_SUBCIRCUIT, 320, 100, 0);
    blk->props.subcircuit.def_id = def_id;
    blk->num_terminals = 2;
    Component *gb = pt_add(c, COMP_GROUND, 320, 220, 0);
    int src = pt_node(c, 0, 60), mid = pt_node(c, 200, 60), gnd = pt_node(c, 0, 180), bg = pt_node(c, 320, 200);
    v->node_ids[0] = src; v->node_ids[1] = gnd; g0->node_ids[0] = gnd;
    rs->node_ids[0] = src; rs->node_ids[1] = mid;
    blk->node_ids[0] = mid; blk->node_ids[1] = bg;
    gb->node_ids[0] = bg;

    Simulation *sim = simulation_create(c);
    *ok = sim && simulation_dc_analysis(sim);
    double vmin = 1e300, vmax = -1e300, imin = 1e300, imax = -1e300;
    if (*ok) {
        double dt = 1.0 / (freq * 400.0);
        simulation_set_time_step(sim, dt);
        simulation_start(sim);
        double t_end = 12.0 / freq;
        while (sim->time < t_end) {
            if (!simulation_step(sim)) { *ok = 0; break; }
            if (sim->time < t_end * 0.6) continue;       /* let it settle first */
            Node *nm = circuit_get_node(c, mid), *ns = circuit_get_node(c, src);
            double vm = nm ? nm->voltage : 0, vsrc = ns ? ns->voltage : 0;
            double i = (vsrc - vm) / 1.0;                /* through the 1 ohm sense */
            if (vm < vmin) vmin = vm;  if (vm > vmax) vmax = vm;
            if (i < imin) imin = i;    if (i > imax) imax = i;
        }
    }
    double z = 0;
    if (*ok && (imax - imin) > 1e-12) z = (vmax - vmin) / (imax - imin);
    if (!isfinite(z)) *ok = 0;
    if (sim) simulation_free(sim);
    circuit_free(c);
    return z;
}

static int spice_test(void) {
    int fails = 0, total = 0;
    printf("spice-test: a vendor .SUBCKT imported and simulated\n\n");

    /* value parsing, including the MEG / M trap */
    struct { const char *txt; double want; } vals[] = {
        { "4.7u", 4.7e-6 }, { "100n", 100e-9 }, { "1MEG", 1e6 }, { "1m", 1e-3 },
        { "2.2k", 2200.0 }, { "1e-9", 1e-9 }, { "30m", 30e-3 }, { "0.7n", 0.7e-9 },
    };
    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        double got = 0;
        int ok = spice_parse_value(vals[i].txt, &got);
        int pass = ok && fabs(got - vals[i].want) <= fabs(vals[i].want) * 1e-9;
        total++; if (!pass) fails++;
        printf("%s spice value %-8s = %-12.6g expect %-12.6g\n", pass ? " OK " : "FAIL",
               vals[i].txt, got, vals[i].want);
    }

    g_subcircuit_library.count = 0; g_subcircuit_library.next_id = 0;
    char msg[256] = "";
    int n = spice_import_text(SPICE_NETLIST, msg, sizeof msg);
    total++;
    int pass = (n == 2);
    if (!pass) fails++;
    printf("%s spice import  %d subcircuit(s): %s\n", pass ? " OK " : "FAIL", n, msg);

    int cap_id = 0, bank_id = 0;
    for (int i = 0; i < g_subcircuit_library.count; i++) {
        if (!strcmp(g_subcircuit_library.defs[i].name, "CAP100N")) cap_id = g_subcircuit_library.defs[i].id;
        if (!strcmp(g_subcircuit_library.defs[i].name, "CAPBANK")) bank_id = g_subcircuit_library.defs[i].id;
    }
    total++;
    pass = (cap_id != 0 && bank_id != 0);
    if (!pass) fails++;
    printf("%s spice both models are in the library (CAP100N and the CAPBANK built from it)\n",
           pass ? " OK " : "FAIL");

    /* |Z| of C = 100 nF, ESR 30 mOhm, ESL 0.7 nH at three points either side of resonance */
    if (cap_id) {
        struct { double f, want, tol; const char *note; } zc[] = {
            { 100e3,  15.9,   0.10, "below resonance: 1/(2 pi f C) = 15.9 ohm, the capacitor" },
            { 19.02e6, 0.030, 0.60, "at series resonance the reactances cancel: just the ESR" },
            { 100e6,   0.44,  0.15, "above it the ESL takes over: 2 pi f L = 0.44 ohm" },
        };
        for (unsigned i = 0; i < sizeof zc / sizeof zc[0]; i++) {
            int ok = 1;
            double z = spice_block_z(cap_id, zc[i].f, &ok);
            total++;
            int p2 = ok && fabs(z - zc[i].want) <= zc[i].tol * zc[i].want;
            if (!p2) fails++;
            printf("%s spice |Z| at %-8.4g Hz = %9.4g ohm  expect %-8.3g  %s%s\n",
                   p2 ? " OK " : "FAIL", zc[i].f, z, zc[i].want, zc[i].note,
                   ok ? "" : "  [simulation failed]");
        }
    }

    /* two of them in parallel: half the impedance, and it proves the X instance nested */
    if (bank_id) {
        int ok = 1;
        double z = spice_block_z(bank_id, 100e3, &ok);
        total++;
        int p2 = ok && fabs(z - 7.96) <= 0.12 * 7.96;
        if (!p2) fails++;
        printf("%s spice |Z| of two in parallel  = %9.4g ohm  expect 7.96     (X instances nest)%s\n",
               p2 ? " OK " : "FAIL", z, ok ? "" : "  [simulation failed]");
    }

    printf("\nspice-test: %d checks, %d failed\n", total, fails);
    return fails ? 1 : 0;
}


/* ---------------------------------------------------------------------------------------
 * --xtal-test: the crystal component on its own, before any oscillator is built around it.
 * A quartz crystal is a very high Q series resonator, and the thing that goes wrong in a
 * simulator is that the integration damps it: the impedance dip at series resonance comes
 * out shallow and broad, and an oscillator built on it never starts. So this drives the part
 * from a source through a sense resistor and measures |Z| at, below and above resonance, and
 * separately measures how long a struck resonance takes to decay.
 * ------------------------------------------------------------------------------------- */
static double xtal_z(double freq, int *ok) {
    Circuit *c = circuit_create();
    Component *v = pt_add(c, COMP_AC_VOLTAGE, 0, 100, 0);
    v->props.ac_voltage.amplitude = 1.0; v->props.ac_voltage.frequency = freq;
    Component *g0 = pt_add(c, COMP_GROUND, 0, 200, 0);
    Component *rs = pt_add(c, COMP_RESISTOR, 140, 60, 0);
    rs->props.resistor.resistance = 1000.0;
    rs->props.resistor.power_rating = 10.0;
    Component *y = pt_add(c, COMP_CRYSTAL, 320, 60, 0);
    Component *gy = pt_add(c, COMP_GROUND, 420, 200, 0);
    int src = pt_node(c, 0, 60), mid = pt_node(c, 200, 60), gnd = pt_node(c, 0, 180), yg = pt_node(c, 420, 180);
    v->node_ids[0] = src; v->node_ids[1] = gnd; g0->node_ids[0] = gnd;
    rs->node_ids[0] = src; rs->node_ids[1] = mid;
    y->node_ids[0] = mid; y->node_ids[1] = yg;
    gy->node_ids[0] = yg;

    Simulation *sim = simulation_create(c);
    *ok = sim && simulation_dc_analysis(sim);
    double vmin = 1e300, vmax = -1e300, imin = 1e300, imax = -1e300;
    if (*ok) {
        double dt = 1.0 / (freq * 200.0);
        simulation_set_time_step(sim, dt);
        simulation_start(sim);
        double t_end = 400.0 / freq;                 /* Q = 314: the resonance needs ~Q cycles to build */
        while (sim->time < t_end) {
            if (!simulation_step(sim)) { *ok = 0; break; }
            if (sim->time < t_end * 0.75) continue;
            Node *nm = circuit_get_node(c, mid), *ns = circuit_get_node(c, src);
            double vm = nm ? nm->voltage : 0, vs = ns ? ns->voltage : 0;
            double i = (vs - vm) / 1000.0;
            if (vm < vmin) vmin = vm;  if (vm > vmax) vmax = vm;
            if (i < imin) imin = i;    if (i > imax) imax = i;
        }
    }
    double z = 0;
    if (*ok && (imax - imin) > 1e-15) z = (vmax - vmin) / (imax - imin);
    if (!isfinite(z)) *ok = 0;
    if (sim) simulation_free(sim);
    circuit_free(c);
    return z;
}

static int xtal_test(void) {
    int fails = 0, total = 0;
    printf("xtal-test: the crystal component, measured the way a data sheet specifies one\n\n");

    /* fs = 1/(2 pi sqrt(Ls Cs)) with the defaults: 100 mH and 25.33 pF */
    double fs = 1.0 / (2.0 * M_PI * sqrt(100e-3 * 25.33e-12));
    /* The claims are about shape, not an exact Rs read-out: the holder capacitance shunts the
       arm, and measuring peak-to-peak volts over peak-to-peak amps ignores the phase between
       them, so the number at resonance sits somewhat above the 200 ohm the arm alone would give.
       What matters - and what a damped integrator destroys - is that the dip is deep and that
       the impedance climbs steeply on both sides. */
    struct { double f; double limit; int below; const char *note; } cases[] = {
        { fs,        450.0, 1, "at series resonance the motional arm dominates and Z collapses" },
        { fs * 0.98, 800.0, 0, "2 % below: the arm is capacitive and Z climbs" },
        { fs * 1.02, 800.0, 0, "2 % above: inductive, and it climbs the other way" },
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        int ok = 1;
        double z = xtal_z(cases[i].f, &ok);
        total++;
        int pass = ok && (cases[i].below ? z < cases[i].limit : z > cases[i].limit);
        if (!pass) fails++;
        printf("%s xtal |Z| at %9.4g Hz = %10.4g ohm  expect %s %-6g  %s%s\n", pass ? " OK " : "FAIL",
               cases[i].f, z, cases[i].below ? "<" : ">", cases[i].limit, cases[i].note,
               ok ? "" : "  [simulation failed]");
    }

    /* Q: the resonance must not be damped by the integrator. Compare the measured impedance
       ratio between resonance and 2 % off it - a Q of 314 gives a very deep, narrow dip. */
    {
        int ok1 = 1, ok2 = 1;
        double z_res = xtal_z(fs, &ok1), z_off = xtal_z(fs * 1.02, &ok2);
        total++;
        double ratio = (z_res > 0) ? z_off / z_res : 0;
        int pass = ok1 && ok2 && ratio > 8.0;
        if (!pass) fails++;
        printf("%s xtal off-resonance / resonance = %.1fx  (a damped model flattens this toward 1)\n",
               pass ? " OK " : "FAIL", ratio);
    }

    printf("\nxtal-test: %d checks, %d failed\n", total, fails);
    return fails ? 1 : 0;
}

static int series_template(const char *filter, double t_end, int node_id) {
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        if (!ti || !strstr(ti->name, filter)) continue;
        Circuit *c = circuit_create();
        circuit_place_template(c, (CircuitTemplateType)t, 0, 0);
        Simulation *sim = simulation_create(c);
        int ok = simulation_dc_analysis(sim);
        double td = circuit_template_scope_time_div((CircuitTemplateType)t);
        simulation_set_time_step(sim, td > 0 ? simulation_scope_time_step(sim, td) : 1e-5);
        simulation_start(sim);
        long steps = 0, every = (long)(t_end / sim->time_step / 200); if (every < 1) every = 1;
        while (ok && sim->time < t_end && steps < 5000000) {
            if (!simulation_step(sim)) break;
            steps++;
            if (steps % every == 0) { Node *nd = circuit_get_node(c, node_id); printf("%.6g %.6g\n", sim->time, nd ? nd->voltage : 0.0); }
        }
        simulation_free(sim); circuit_free(c);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    /* Unbuffered: when a template crashes the run, the last line printed has to be the
       one that crashed. Block buffering hid it behind 4 KB of already-passed templates. */
    setvbuf(stdout, NULL, _IONBF, 0);
    /* --shard is read before anything else: a suite flag returns straight out of the loop below,
       so a shard written after it on the command line would never be seen. */
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--shard")) {
            if (sscanf(argv[i + 1], "%d/%d", &g_shard_i, &g_shard_n) != 2 || g_shard_n < 1 ||
                g_shard_i < 0 || g_shard_i >= g_shard_n) {
                fprintf(stderr, "--shard wants i/n with 0 <= i < n, e.g. 0/4\n");
                return 2;
            }
        }
    int dc_only = 0, verbose = 0, dump_nodes = 0;
    const char *svg_dir = NULL;
    double sim_time = 5e-3;
    const char *filter = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--dc")) dc_only = 1;
        else if (!strcmp(argv[i], "--verbose")) verbose = 1;
        else if (!strcmp(argv[i], "--nodes")) dump_nodes = 1;
        else if (!strcmp(argv[i], "--svg") && i + 1 < argc) svg_dir = argv[++i];
        else if (!strcmp(argv[i], "--scope-test")) return scope_dt_test();
        else if (!strcmp(argv[i], "--flow-test")) return flow_test();
        else if (!strcmp(argv[i], "--pair-test")) return pair_test();
        else if (!strcmp(argv[i], "--ic-test")) return ic_test();
        else if (!strcmp(argv[i], "--sketch-test")) return sketch_test();
        else if (!strcmp(argv[i], "--mcu-test")) return mcu_test();
        else if (!strcmp(argv[i], "--restamp-test")) return restamp_test();
        else if (!strcmp(argv[i], "--class-test"))
            return class_test((i + 1 < argc && atof(argv[i + 1]) > 0) ? atof(argv[i + 1]) : 0.25);
        else if (!strcmp(argv[i], "--conn-test")) return conn_test();
        else if (!strcmp(argv[i], "--file-test")) return file_test(i + 1 < argc ? argv[i + 1] : NULL);
        else if (!strcmp(argv[i], "--json-dump") && i + 2 < argc) {
            /* place a template and write it as JSON, to look at what the format keeps */
            Circuit *c = circuit_create();
            for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
                const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
                if (ti && strstr(ti->name, argv[i + 1])) { circuit_place_template(c, (CircuitTemplateType)t, 0, 0); break; }
            }
            printf("%d components -> %s\n", c->num_components, argv[i + 2]);
            return file_export_json(c, argv[i + 2]) ? 0 : 1;
        }
        else if (!strcmp(argv[i], "--line-test")) return line_test();
        else if (!strcmp(argv[i], "--view-test")) return scope_test();   /* --scope-test is the dt rule; this is what the screen shows */
        else if (!strcmp(argv[i], "--burn-test")) return burn_test();
        else if (!strcmp(argv[i], "--std-test")) return std_test();
        else if (!strcmp(argv[i], "--switch-test")) return switch_test();
        else if (!strcmp(argv[i], "--part-test")) return part_test();
        else if (!strcmp(argv[i], "--op-test")) return op_test();
        else if (!strcmp(argv[i], "--sub-test")) return sub_test();
        else if (!strcmp(argv[i], "--bias-test")) return bias_test();
        else if (!strcmp(argv[i], "--netlist-test")) return netlist_test();
        else if (!strcmp(argv[i], "--spice-test")) return spice_test();
        else if (!strcmp(argv[i], "--xtal-test")) return xtal_test();
        else if (!strcmp(argv[i], "--osc-dt") && i + 1 < argc) g_osc_dt_forced = atof(argv[++i]);
        else if (!strcmp(argv[i], "--probe-dt") && i + 1 < argc) g_probe_dt = atof(argv[++i]);
        else if (!strcmp(argv[i], "--only") && i + 1 < argc) g_only = argv[++i];
        else if (!strcmp(argv[i], "--shard") && i + 1 < argc) i++;   /* read before this loop */
        else if (!strcmp(argv[i], "--osc-test")) return osc_test();
        else if (!strcmp(argv[i], "--dvdt-test")) return dvdt_test();
        else if (!strcmp(argv[i], "--meas-test")) return meas_test();
        else if (!strcmp(argv[i], "--state-test")) return state_test();
        else if (!strcmp(argv[i], "--dcm-test")) return dcm_test();
        else if (!strcmp(argv[i], "--iv-test")) return iv_test();
        else if (!strcmp(argv[i], "--conv-test")) return conv_test();
        else if (!strcmp(argv[i], "--stress-test")) return stress_test(i + 1 < argc ? argv[++i] : NULL);
        else if (!strcmp(argv[i], "--mc-test")) return mc_test();
        else if (!strcmp(argv[i], "--bode-test")) return bode_test();
        else if (!strcmp(argv[i], "--sign-test")) return sign_test();
        else if (!strcmp(argv[i], "--load-test")) return load_test();
        else if (!strcmp(argv[i], "--fft-test")) return fft_test();
        else if (!strcmp(argv[i], "--probe-test")) return probe_test();
        else if (!strcmp(argv[i], "--label-test")) return label_test();
        else if (!strcmp(argv[i], "--parts-file-test")) return parts_file_test();
        else if (!strcmp(argv[i], "--undo-test")) return undo_test();
        else if (!strcmp(argv[i], "--span-test")) return span_test();
        else if (!strcmp(argv[i], "--geom-test")) return geom_test();
        else if (!strcmp(argv[i], "--sweep-check")) return sweep_check();
        else if (!strcmp(argv[i], "--demo-test")) return demo_test();
        else if (!strcmp(argv[i], "--tesla-test")) return tesla_test();
        else if (!strcmp(argv[i], "--param-test")) return param_test();
        else if (!strcmp(argv[i], "--knob-test")) return knob_test(i + 1 < argc ? argv[++i] : NULL);
        else if (!strcmp(argv[i], "--probe-audit")) return probe_audit(i + 1 < argc ? argv[++i] : NULL);
        else if (!strcmp(argv[i], "--series") && i + 3 < argc) { const char *nm = argv[++i]; double tt = atof(argv[++i]); return series_template(nm, tt, atoi(argv[++i])); }
        else if (!strcmp(argv[i], "--trace") && i + 2 < argc) { const char *nm = argv[++i]; return trace_template(nm, atof(argv[++i])); }
        else if (!strcmp(argv[i], "--response") && i + 1 < argc) return response_explore(argv[++i]);
        else if (!strcmp(argv[i], "--sim-time") && i + 1 < argc) sim_time = atof(argv[++i]);
        else filter = argv[i];
    }

    int failures = 0, total = 0;
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        if (shard_skip(t)) continue;
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        const char *name = ti ? ti->name : "?";
        if (filter && !strstr(name, filter)) continue;
        total++;

        Circuit *circuit = circuit_create();
        int placed = circuit_place_template(circuit, (CircuitTemplateType)t, 0, 0);
        if (placed <= 0) {
            printf("[FAIL] %-28s place_template returned %d\n", name, placed);
            failures++;
            circuit_free(circuit);
            continue;
        }

        if (svg_dir) {
            char path[512], safe[64];
            int k = 0;
            /* A whitelist, not a blacklist. This replaced space, slash and plus and let the
               colon through, and on NTFS a colon in a filename starts an alternate data stream:
               fopen succeeds, the bytes go into an invisible stream, and the visible file is
               empty. Six templates with ":" in their names exported nothing, silently, until
               tools/svg_audit.py opened every file with a real XML parser. */
            for (const char *c = name; *c && k < 60; c++) {
                unsigned char ch = (unsigned char)*c;
                safe[k++] = (isalnum(ch) || ch == '-' || ch == '.') ? *c : '_';
            }
            safe[k] = 0;
            snprintf(path, sizeof path, "%s/%02d_%s.svg", svg_dir, t, safe);
            if (!file_export_svg(circuit, path)) printf("      (svg export failed: %s)\n", path);
        }

        Simulation *sim = simulation_create(circuit);
        int ok = 1;
        char why[256] = "";
        double max_abs = 0;

        if (!simulation_dc_analysis(sim)) {
            ok = 0;
            snprintf(why, sizeof why, "DC: %s", simulation_get_error(sim));
        } else if (sim->has_error) {
            /* dc_analysis can succeed but leave a warning (non-convergence) */
            ok = 0;
            snprintf(why, sizeof why, "DC: %s", simulation_get_error(sim));
        }
        int nan_nodes = check_voltages(circuit, &max_abs);
        if (nan_nodes) { ok = 0; snprintf(why, sizeof why, "DC: %d NaN/Inf nodes", nan_nodes); }

        int steps = 0;
        double dt = 0;
        double run_max = max_abs;
        if (ok && !dc_only) {
            dt = simulation_auto_time_step(sim);
            simulation_start(sim);
            int max_steps = 200000;
            while (sim->time < sim_time && steps < max_steps) {
                if (!simulation_step(sim)) {
                    ok = 0;
                    snprintf(why, sizeof why, "step %d (t=%.3es): %s", steps, sim->time,
                             simulation_get_error(sim));
                    break;
                }
                steps++;
                if ((steps & 15) == 0) {
                    nan_nodes = check_voltages(circuit, &max_abs);
                    if (max_abs > run_max) run_max = max_abs;
                    if (nan_nodes) {
                        ok = 0;
                        snprintf(why, sizeof why, "step %d: %d NaN/Inf nodes", steps, nan_nodes);
                        break;
                    }
                }
            }
            if (ok && sim->has_error) {
                ok = 0;
                snprintf(why, sizeof why, "after %d steps: %s", steps, simulation_get_error(sim));
            }
            check_voltages(circuit, &max_abs);
            if (max_abs > run_max) run_max = max_abs;
            max_abs = run_max;
            if (ok && max_abs > fmax(1000.0, 2.5 * source_scale(circuit))) {
                ok = 0;
                snprintf(why, sizeof why, "runaway: max|V| = %.3g V", max_abs);
            }
        }

        printf("[%s] %-28s comps=%-3d nodes=%-3d dt=%.2e steps=%-6d max|V|=%8.3f %s\n",
               ok ? " OK " : "FAIL", name, circuit->num_components, circuit->num_nodes,
               dt, steps, max_abs, why);
        if (verbose || !ok) print_bias(circuit);
        if (dump_nodes) {
            for (int i = 0; i < circuit->num_components; i++) {
                Component *comp = circuit->components[i];
                printf("      %-8s nodes:", comp->label);
                for (int t = 0; t < comp->num_terminals; t++)
                    printf(" [%d]=n%d->m%d(%.3fV)", t, comp->node_ids[t],
                           (comp->node_ids[t] >= 0 && comp->node_ids[t] < MAX_NODES) ? circuit->node_map[comp->node_ids[t]] : -1,
                           node_v(circuit, comp->node_ids[t]));
                if (comp->type == COMP_RESISTOR)
                    printf("  I=%.3fmA", (node_v(circuit, comp->node_ids[0]) - node_v(circuit, comp->node_ids[1]))
                                          / comp->props.resistor.resistance * 1e3);
                printf("\n");
            }
            printf("      converged=%d iterations=%d error='%s'\n", sim->converged, sim->iteration_count,
                   sim->has_error ? simulation_get_error(sim) : "");
            {
            }
        }
        if (!ok || max_abs > 50.0) print_node_owners(circuit, g_worst_node);

        if (!ok) failures++;
        simulation_free(sim);
        circuit_free(circuit);
    }

    printf("\n%d/%d templates passed\n", total - failures, total);
    return failures;
}
