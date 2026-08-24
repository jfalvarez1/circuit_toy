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
        int ok = fabs(got - 1e-7) < 1e-12;   /* no AC source: accuracy step = DEFAULT 100 ns */
        printf("[%s] scope-dt  DC circuit 100 s/div -> dt=%.3g (want 1e-07: default accuracy step)\n",
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
            if (ok && comp->num_terminals == 2 && fabs(sum) > 1e-6 * amax + 1e-9) {
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
            double inflow = 0;
            for (int w = 0; w < c->num_wires; w++) {
                if (c->wires[w].end_node_id == id) inflow += c->wires[w].current;
                if (c->wires[w].start_node_id == id) inflow -= c->wires[w].current;
            }
            if (fabs(inflow - demand) > 1e-6 * (imax + 1e-9) + 1e-9) {
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
static double g_osc_dt = 1e-6;
static int osc_test(void) {
    struct { CircuitTemplateType t; double run; double f_expect; } cases[] = {
        { CIRCUIT_WIEN_OSCILLATOR, 0.040, 1591.5 },
        { CIRCUIT_PHASE_SHIFT_OSC, 0.010, 6497.0 },
        { CIRCUIT_RELAXATION_OSC, 0.040, 455.0 },
    };
    int fails = 0;
    for (unsigned k = 0; k < sizeof cases / sizeof cases[0]; k++) {
        const CircuitTemplateInfo *ti = circuit_template_get_info(cases[k].t);
        Circuit *c = circuit_create();
        circuit_place_template(c, cases[k].t, 0, 0);
        Simulation *sim = simulation_create(c);
        Component *amp = NULL;
        for (int i = 0; i < c->num_components; i++) {
            ComponentType ty = c->components[i]->type;
            if (ty == COMP_OPAMP || ty == COMP_OPAMP_REAL || ty == COMP_OPAMP_FLIPPED) { amp = c->components[i]; break; }
        }
        int ok = amp && simulation_dc_analysis(sim);
        simulation_set_time_step(sim, g_osc_dt);
        simulation_start(sim);
        /* record output in the last quarter */
        double t_rec = cases[k].run * 0.75;
        enum { NMAX = 20000 };
        static double vs[NMAX]; static double ts[NMAX]; int n = 0;
        while (ok && sim->time < cases[k].run) {
            if (!simulation_step(sim)) { ok = 0; break; }
            if (sim->time >= t_rec && n < NMAX) {
                Node *nd = circuit_get_node(c, amp->node_ids[2]);
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
        int osc = ok && crossings >= 3 && (vmax - vmin) > 0.5;
        int f_ok = osc && fabs(f_meas - cases[k].f_expect) < 0.25 * cases[k].f_expect;
        printf("[%s] osc   %-28s swing=%.2fV  rising-crossings=%d  f=%.0fHz (expect ~%.0f)%s\n",
               (osc && f_ok) ? " OK " : "FAIL", ti ? ti->name : "?", vmax - vmin, crossings, f_meas, cases[k].f_expect,
               !ok ? "  [sim error]" : !osc ? "  [NOT OSCILLATING: latched or dead loop]" : !f_ok ? "  [frequency off]" : "");
        if (!(osc && f_ok)) fails++;
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
        int ok = (diag + cross + through + touch) == 0;
        printf("[%s] geom  %-28s diag=%d cross=%d through=%d touch=%d%s\n", ok ? " OK " : "WARN", ti ? ti->name : "?", diag, cross, through, touch, detail);
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
    static const ComponentType st[] = { COMP_AC_VOLTAGE, COMP_SQUARE_WAVE, COMP_TRIANGLE_WAVE, COMP_PULSE_SOURCE, COMP_DC_CURRENT, COMP_DC_VOLTAGE };
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
            simulation_dc_analysis(sim); simulation_auto_time_step(sim); simulation_start(sim);
            double run = (d->f_char > 0) ? 6.0 / d->f_char : 0.01; if (run < 0.003) run = 0.003;
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
        else if (!strcmp(argv[i], "--osc-dt") && i + 1 < argc) g_osc_dt = atof(argv[++i]);
        else if (!strcmp(argv[i], "--osc-test")) return osc_test();
        else if (!strcmp(argv[i], "--probe-test")) return probe_test();
        else if (!strcmp(argv[i], "--geom-test")) return geom_test();
        else if (!strcmp(argv[i], "--sweep-check")) return sweep_check();
        else if (!strcmp(argv[i], "--demo-test")) return demo_test();
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
            if (ok && max_abs > 1000.0) {
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
