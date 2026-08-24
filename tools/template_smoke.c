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
 * Usage: template_smoke [--dc] [--verbose] [--sim-time SEC] [name-substring]
 * Exit code is the number of failing templates (0 = all good).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "types.h"
#include "component.h"
#include "circuit.h"
#include "circuits.h"
#include "simulation.h"

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

int main(int argc, char **argv) {
    int dc_only = 0, verbose = 0, dump_nodes = 0;
    double sim_time = 5e-3;
    const char *filter = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--dc")) dc_only = 1;
        else if (!strcmp(argv[i], "--verbose")) verbose = 1;
        else if (!strcmp(argv[i], "--nodes")) dump_nodes = 1;
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
