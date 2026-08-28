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
#include "simulation.h"
#include "file_io.h"

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
        { 10e-9,  1e-9,  "10 ns/div: wants 0.5 ns -> clamped to MIN_TIME_STEP" },
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

static int flow_test(void) {
    int fails = 0, total = 0;
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        const char *name = ti ? ti->name : "?";
        Circuit *c = circuit_create();
        if (circuit_place_template(c, (CircuitTemplateType)t, 0, 0) <= 0) { circuit_free(c); continue; }
        Simulation *sim = simulation_create(c);
        total++;
        int ok = 1; char why[200] = "";
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
            if (!(comp->type == COMP_TLINE && comp->props.tline.model >= 2) && !(comp->type >= COMP_NOT_GATE && comp->type <= COMP_XNOR_GATE) && (ok && comp->num_terminals == 2 && fabs(sum) > 1e-6 * amax + 1e-9)) {   /* pi lines shunt charging current to ground */
                ok = 0; snprintf(why, sizeof why, "%s not conserving: I0+I1=%.3g (|I|=%.3g)", comp->label, sum, amax);
            }
        }
        for (int w = 0; ok && w < c->num_wires; w++) {
            double v = c->wires[w].current;
            if (isnan(v) || isinf(v)) { ok = 0; snprintf(why, sizeof why, "wire %d current NaN", w); }
        }
        /* KCL at each node (skip nodes carrying a ground symbol: they are the sink) */
        for (int i = 0; ok && i < c->num_nodes; i++) {
            int id = c->nodes[i].id;
            int grounded = 0; double demand = 0;
            for (int j = 0; j < c->num_components; j++) {
                Component *comp = c->components[j];
                for (int k = 0; k < comp->num_terminals; k++) if (comp->node_ids[k] == id) {
                    if (comp->type == COMP_GROUND) grounded = 1; else demand += comp->terminal_current[k];
                }
            }
            if (grounded) continue;
            {   // behavioural logic gates do not report terminal currents: skip their nodes
                int behavioural = 0;
                for (int j = 0; j < c->num_components && !behavioural; j++) {
                    Component *bc = c->components[j];
                    if (bc->type < COMP_NOT_GATE || bc->type > COMP_XNOR_GATE) continue;
                    for (int k = 0; k < bc->num_terminals; k++) if (bc->node_ids[k] == id) behavioural = 1;
                }
                if (behavioural) continue;
            }
            double inflow = 0;
            for (int w = 0; w < c->num_wires; w++) {
                if (c->wires[w].end_node_id == id) inflow += c->wires[w].current;
                if (c->wires[w].start_node_id == id) inflow -= c->wires[w].current;
            }
            if (fabs(inflow - demand) > 1e-6 * (imax + 1e-9) + 1e-8) {   /* 10 nA floor: open spark gaps leak ~nA */
                ok = 0; snprintf(why, sizeof why, "KCL at node %d: wires %.4g vs demand %.4g", id, inflow, demand);
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
        printf("[%s] flow  %-28s wires=%-3d max|I|=%.3g %s\n", ok ? " OK " : "FAIL", name, c->num_wires, imax, why);
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
static double g_osc_dt = 1e-6;
static int osc_test(void) {
    /* shape: the AC rms of the probed node divided by its peak-to-peak. A sine is 0.354,
       a square 0.5 and a triangle 0.289 - so a clipped or notched 'sine' fails on shape
       even when it still crosses its mean at roughly the right rate. 0 = do not check. */
    struct { CircuitTemplateType t; double run; double f_expect; double dt; double shape; } cases[] = {
        { CIRCUIT_WIEN_OSCILLATOR, 0.040, 1591.5, 0, 0.354 },
        { CIRCUIT_PHASE_SHIFT_OSC, 0.010, 5973.0, 0, 0.354 },   /* ideal 1/(2 pi R C sqrt 6) = 6497; loading the last section pulls it down 8 % */
        { CIRCUIT_RELAXATION_OSC, 0.040, 455.0, 0, 0.500 },
        { CIRCUIT_TRI_SQUARE_GEN, 0.004, 5000.0, 2e-7, 0.289 },
        { CIRCUIT_FUNCTION_GEN, 0.004, 5000.0, 2e-7, 0.354 },
        { CIRCUIT_COLPITTS, 60e-6, 712e3, 5e-9, 0.354 },
        { CIRCUIT_RING_OSC, 200e-6, 145e3, 2e-8, 0.500 },
        { CIRCUIT_HARTLEY, 80e-6, 534188, 5e-9, 0.354 },        /* ideal 1/(2 pi sqrt((L1+L2)C)) = 503 kHz; the tap is only an AC ground through the supply */
        { CIRCUIT_CLAPP, 30e-6, 1743455, 2e-9, 0.354 },
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
        simulation_set_time_step(sim, cases[k].dt > 0 ? cases[k].dt : g_osc_dt);
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
    { CIRCUIT_MOS_TUNED,        COMP_RESISTOR,  4, 0, "amp", 1.2458, 0.06, 4e-3,  "gain peaks as the sweep passes the 100 kHz tank" },
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
    { CIRCUIT_HW_REFLECT,       COMP_INDUCTOR,  7, 1, "max", 5.0101, 0.04, 8e-6, "open far end doubles the incident 2.5 V step" },
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
    { CIRCUIT_CAP_DCBIAS,       COMP_CAPACITOR, 0, 0, "amp", 0.03125, 0.10, 4e-3, "10 uF unbiased: I(T/2)/C = 62.5 mVpp" },
    { CIRCUIT_CAP_DCBIAS,       COMP_CAPACITOR, 1, 0, "amp", 0.0625,  0.10, 4e-3, "2 V bias halves it: twice the ripple" },
    { CIRCUIT_CAP_DCBIAS,       COMP_CAPACITOR, 2, 0, "amp", 0.1094,  0.12, 4e-3, "5 V bias leaves 2.86 uF: 3.5x the ripple" },
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

static int probe_test(void) {
    int fails = 0, total = 0;
    for (unsigned k = 0; k < sizeof probe_cases / sizeof probe_cases[0]; k++) {
        const ProbeCase *pc = &probe_cases[k];
        const CircuitTemplateInfo *ti = circuit_template_get_info(pc->t);
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

static int geom_test(void) {
    int bad_templates = 0, total = 0;
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        Circuit *c = circuit_create();
        if (circuit_place_template(c, (CircuitTemplateType)t, 0, 0) <= 0) { circuit_free(c); continue; }
        total++;
        int diag = 0, cross = 0, through = 0, touch = 0;
        char detail[400] = "";
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
            }
            /* through a body: skip components that own one of the wire's endpoints */
            for (int i = 0; i < c->num_components; i++) {
                Component *comp = c->components[i];
                if (comp->type == COMP_TEXT || comp->type == COMP_GROUND) continue;
                int owns = 0;
                for (int k = 0; k < comp->num_terminals; k++)
                    if (comp->node_ids[k] == c->wires[w].start_node_id || comp->node_ids[k] == c->wires[w].end_node_id) owns = 1;
                if (owns) continue;
                const ComponentTypeInfo *info = component_get_info(comp->type);
                float hw = (info ? info->width : 40) / 2.0f - 6, hh = (info ? info->height : 40) / 2.0f - 6;
                if (comp->rotation % 180 != 0) { float tmp = hw; hw = hh; hh = tmp; }
                if (hw < 4) hw = 4; if (hh < 4) hh = 4;
                if (seg_hits_box(a->x, a->y, b->x, b->y, comp->x - hw, comp->y - hh, comp->x + hw, comp->y + hh)) {
                    through++;
                    if (strlen(detail) < 300) snprintf(detail + strlen(detail), sizeof detail - strlen(detail), " through:%s", comp->label);
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
        int ok = (diag + cross + through + touch + overlap) == 0;
        printf("[%s] geom  %-28s diag=%d cross=%d through=%d touch=%d overlap=%d%s\n", ok ? " OK " : "WARN", ti ? ti->name : "?", diag, cross, through, touch, overlap, detail);
        if (!ok) bad_templates++;
        circuit_free(c);
    }
    printf("\n%d/%d templates geometrically clean\n", total - bad_templates, total);
    return bad_templates;
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
        if (ok && np >= 2 && vd > 0 && waveform && amp > 0 && amp < 0.25 * vd) strcat(flags, "SMALL ");
        if (ok && np >= 2 && vd > 0 && peak > 4.0 * vd) strcat(flags, "CLIP ");
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
    { "IRF540N", PC_ID_MIN,  6.0, 1.0,   0,    0, "well into conduction one volt above V_GS(th) + 1" },
    { "BS250",   PC_RDSON, -10.0, 10.0,  0.20, 0, "R_DS(on) 14 ohm max at V_GS = -10 V (10 typ)" },
    /* --- BJTs: forced base current, at the data sheet's collector current --- */
    { "2N3904",  PC_HFE,   50e-6, 200.0, 0.15, 0, "h_FE 100 - 300 at I_C = 10 mA" },
    { "2N3904",  PC_VBE,   50e-6, 0.66,  0.12, 0, "V_BE(on) 0.65 V typ at I_C = 10 mA" },
    { "BC547B",  PC_HFE,   10e-6, 290.0, 0.15, 0, "h_FE 200 - 450 (B grade)" },
    { "2N3906",  PC_HFE,   50e-6, 180.0, 0.15, 0, "h_FE 100 - 300 at I_C = 10 mA" },
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
               pc->kind == PC_HFE ? "h_FE" : pc->kind == PC_VBE ? "V_BE" :
               pc->kind == PC_VF ? "V_F" : pc->kind == PC_VZ ? "V_Z" :
               pc->kind == PC_OPAMP_VOS ? "V_offset" : pc->kind == PC_OPAMP_SR ? "slew rate" : "V_out",
               got, unit, pc->expect, pc->note, ok ? "" : "  [simulation failed]");
    }
    printf("\npart-test: %d checks over %d named devices, %d failed\n",
           total, component_part_count(), fails);
    return fails ? 1 : 0;
}

static int series_template(const char *filter, double t_end, int node_id) {
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
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
        else if (!strcmp(argv[i], "--burn-test")) return burn_test();
        else if (!strcmp(argv[i], "--std-test")) return std_test();
        else if (!strcmp(argv[i], "--switch-test")) return switch_test();
        else if (!strcmp(argv[i], "--part-test")) return part_test();
        else if (!strcmp(argv[i], "--osc-dt") && i + 1 < argc) g_osc_dt = atof(argv[++i]);
        else if (!strcmp(argv[i], "--osc-test")) return osc_test();
        else if (!strcmp(argv[i], "--probe-test")) return probe_test();
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
            for (const char *c = name; *c && k < 60; c++) safe[k++] = (*c == ' ' || *c == '/' || *c == '+') ? '_' : *c;
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
