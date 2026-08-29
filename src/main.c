/**
 * Circuit Playground - Main Entry Point
 * A circuit simulator inspired by The Powder Toy
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_main.h>
#include "app.h"
#include "file_io.h"
#include "simulation.h"
#include "circuits.h"
#include "spice.h"
#include "version.h"
#include "updater.h"
#include "ui.h"
#include "crashlog.h"
#ifdef _WIN32
#include <windows.h>
#endif

static bool rects_overlap(const Rect *a, const Rect *b) {
    return a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h;
}

/* --place-test: picking a circuit from the palette must place it, on its own, for every one of
   them. Two things have to hold and both broke in the same way. The click's action code has to
   be recognised as a circuit - the range only held a hundred templates while there are 187, so
   everything past the hundredth fell into the subcircuit range, armed a click and left the
   previous circuit sitting on the canvas. And placing has to replace: the canvas afterwards has
   to be exactly what a fresh canvas with that template on it holds, with nothing left over from
   the circuit before it. One circuit is reused down the whole list, so anything not cleared is
   still there to be found. The action codes of every other kind of click are checked here too,
   because that is the same fault: UI_ACTION_UPDATE sat inside the property-edit range, so
   clicking a source's Offset field ran the updater. */
static int place_test(void) {
    int fails = 0, checks = 0;
    Circuit *shared = circuit_create();

    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        if (!ti) continue;
        char why[220] = "";
        checks++;

        /* what the click produces, and what the dispatch makes of it */
        int action = UI_ACTION_SELECT_CIRCUIT + t, idx = -1;
        UIActionKind kind = ui_action_kind(action, &idx);
        if (kind != UIA_CIRCUIT || idx != t)
            snprintf(why, sizeof why,
                     "a click on it produces action %d, which the dispatch reads as kind %d index "
                     "%d - it would not place the circuit", action, (int)kind, idx);

        /* the placement itself, onto the canvas the previous template was left on */
        Circuit *fresh = circuit_create();
        int want = circuit_place_template(fresh, (CircuitTemplateType)t, 0, 0);
        circuit_clear(shared);
        int got = circuit_place_template(shared, (CircuitTemplateType)t, 0, 0);
        if (!why[0] && want <= 0)
            snprintf(why, sizeof why, "the template places nothing at all");
        else if (!why[0] && got != want)
            snprintf(why, sizeof why, "placed %d parts over the last circuit where a clean canvas "
                     "gets %d", got, want);
        else if (!why[0] && (shared->num_components != fresh->num_components ||
                             shared->num_wires != fresh->num_wires ||
                             shared->num_probes != fresh->num_probes))
            snprintf(why, sizeof why,
                     "left the last circuit behind: %d parts / %d wires / %d probes against the "
                     "%d / %d / %d a clean canvas gets", shared->num_components, shared->num_wires,
                     shared->num_probes, fresh->num_components, fresh->num_wires, fresh->num_probes);
        if (!why[0]) {
            for (int i = 0; i < shared->num_components; i++)
                if (shared->components[i]->type != fresh->components[i]->type) {
                    snprintf(why, sizeof why, "part %d is a %d where a clean canvas has a %d", i,
                             shared->components[i]->type, fresh->components[i]->type);
                    break;
                }
        }
        circuit_free(fresh);

        if (why[0]) { printf("[FAIL] place %-28s %s\n", ti->name, why); fails++; }
        else printf("[ OK ] place %-28s %d parts, %d wires, %d probes\n", ti->name,
                    shared->num_components, shared->num_wires, shared->num_probes);
    }
    circuit_free(shared);

    /* No other click may land inside a range. A property edit that reads as a plain button is
       how clicking Offset came to run the updater. */
    static const struct { int code; const char *name; } simple[] = {
        { UI_ACTION_SCOPE_TRACK, "scope track" }, { UI_ACTION_SPOTLIGHT, "spotlight" },
        { UI_ACTION_UPDATE, "update" }, { UI_ACTION_SCOPE_POPUP, "scope popout" },
        { UI_ACTION_SCOPE_STACK, "scope stack" }, { UI_ACTION_SCOPE_AC, "scope AC" },
        { UI_ACTION_SCOPE_FIT, "scope fit" }, { UI_ACTION_IMPORT_SPICE, "import SPICE" },
        { UI_ACTION_EXPORT_SVG, "export SVG" }, { UI_ACTION_SCREENSHOT, "screenshot" },
        { UI_ACTION_ZOOM_IN, "zoom in" }, { UI_ACTION_ZOOM_OUT, "zoom out" },
        { UI_ACTION_ZOOM_FIT, "zoom fit" }, { UI_ACTION_CREATE_SUBCIRCUIT, "create subcircuit" },
        { UI_ACTION_EDIT_SUBCIRCUIT, "edit subcircuit" }, { UI_ACTION_DEFER_UPDATE, "defer update" },
    };
    for (size_t i = 0; i < sizeof simple / sizeof simple[0]; i++) {
        checks++;
        if (ui_action_kind(simple[i].code, NULL) != UIA_SIMPLE) {
            printf("[FAIL] action %-28s code %d is inside another action's range\n",
                   simple[i].name, simple[i].code);
            fails++;
        }
        for (size_t j = i + 1; j < sizeof simple / sizeof simple[0]; j++)
            if (simple[i].code == simple[j].code) {
                printf("[FAIL] action %s and %s are both code %d\n", simple[i].name,
                       simple[j].name, simple[i].code);
                fails++;
            }
    }
    for (int p = 0; p < PROP_TYPE_COUNT; p++) {
        checks++;
        int idx = -1;
        if (ui_action_kind(UI_ACTION_PROP_EDIT + p, &idx) != UIA_PROP_EDIT || idx != p) {
            printf("[FAIL] property %d edits as something else\n", p);
            fails++;
        }
    }
    checks++;
    if (ui_action_kind(UI_ACTION_PROP_APPLY, NULL) != UIA_PROP_APPLY) {
        printf("[FAIL] applying a property edit reads as something else\n"); fails++;
    }
    for (int c = 0; c < COMP_TYPE_COUNT; c++) {
        checks++;
        int idx = -1;
        if (ui_action_kind(UI_ACTION_SELECT_COMP + c, &idx) != UIA_COMP || idx != c) {
            printf("[FAIL] part %d selects as something else\n", c); fails++;
        }
    }

    printf("\nplace-test: %d checks, %d failed\n", checks, fails);
    return fails;
}

/* --autoset-test: press Autoset on every template and check what it leaves on the screen.
   No window: the scope's scaling is arithmetic over the simulation's history, so it can be run
   and judged headlessly. Two things have to hold afterwards, and both are what a person means
   by "it autoset properly": every channel's trace fits inside the eight divisions the screen
   has, and the trigger level sits inside the range of the channel it triggers on - a level
   outside that range never fires, and the display free-runs. */
static int autoset_test(void) {
    int fails = 0, total = 0;
    /* One scope for the whole run, reused template after template exactly as the app reuses it.
       Each template therefore starts on whatever Autoset left behind for the one before it, which
       is the case the user hit: Autoset the Common Emitter, pick another circuit, no trace. */
    UIState *ui = calloc(1, sizeof *ui);
    ui_init(ui);
    ui->window_width = 1600; ui->window_height = 1000;
    ui_update_layout(ui);
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        if (!ti) continue;
        Circuit *c = circuit_create();
        if (circuit_place_template(c, (CircuitTemplateType)t, 0, 0) <= 0) { circuit_free(c); continue; }
        Simulation *sim = simulation_create(c);

        /* what the app itself does when it places a template - the same function, not a copy of
           it that can drift from it */
        ui_scope_apply_template_preset(ui, (CircuitTemplateType)t);
        ui->scope_num_channels = c->num_probes < MAX_PROBES ? c->num_probes : MAX_PROBES;
        for (int ch = 0; ch < MAX_PROBES; ch++) {
            ui->scope_channels[ch].enabled = ch < ui->scope_num_channels;
            ui->scope_channels[ch].probe_idx = ch;
        }

        total++;
        char why[220] = "";
        /* Nothing from the circuit before this one may still be on the scope. This is the state
           the user sees when they pick a circuit and do NOT press Autoset: a stale offset or a
           trigger level the new circuit never reaches leaves the screen blank. */
        for (int ch = 0; ch < MAX_PROBES && !why[0]; ch++) {
            if (ui->scope_channels[ch].offset != 0.0 || ui->scope_channels[ch].volt_div != 0.0)
                snprintf(why, sizeof why,
                         "CH%d still carries %.4g V of offset / %.4g V per division from the "
                         "circuit before this one", ch + 1,
                         ui->scope_channels[ch].offset, ui->scope_channels[ch].volt_div);
        }
        if (!why[0] && (ui->trigger_level != 0.0 || ui->trigger_channel != 0))
            snprintf(why, sizeof why,
                     "the trigger is still on CH%d at %.4g V from the circuit before this one",
                     ui->trigger_channel + 1, ui->trigger_level);
        if (why[0]) { /* already stale before anything ran; the rest would only add noise */ }
        else if (!simulation_dc_analysis(sim)) snprintf(why, sizeof why, "no operating point");
        else {
            simulation_auto_time_step(sim);
            { double dtp = simulation_scope_time_step(sim, ui->scope_time_div);
              if (dtp > 0 && dtp < sim->time_step) simulation_set_time_step(sim, dtp); }
            /* the scope tells the simulation how much time it wants to keep, which is what
               sets the history decimation; without it a fast template keeps almost nothing */
            simulation_set_history_span(sim, 10.0 * ui->scope_time_div);
            simulation_start(sim);
            /* ten screens' worth, so a slow signal has shown its peak before Autoset looks */
            double run_to = 10.0 * ui->scope_time_div;
            int guard = 0;
            while (sim->time < run_to && guard++ < 200000) if (!simulation_step(sim)) break;

            ui_scope_autoset(ui, sim);

            int with_data = 0;
            for (int ch = 0; ch < ui->scope_num_channels && !why[0]; ch++) {
                double times[MAX_HISTORY], values[MAX_HISTORY];
                int n = simulation_get_history(sim, ui->scope_channels[ch].probe_idx,
                                               times, values, MAX_HISTORY);
                if (n < 10) continue;
                with_data++;
                double lo = values[0], hi = values[0];
                for (int i = 1; i < n; i++) { if (values[i] < lo) lo = values[i]; if (values[i] > hi) hi = values[i]; }
                double vd = ui->scope_volt_div;
                double off = ui->scope_channels[ch].offset;
                /* In the per-channel view each band centres itself on that channel's own mean,
                   so an offset on top of it shifts the trace off the band twice over - which is
                   how Autoset once blanked the screen while every readout still updated. There,
                   the only correct offset is none. */
                if (ui->scope_stacked && ui->scope_stack_fit) {
                    if (fabs(off) > 1e-9)
                        snprintf(why, sizeof why,
                                 "CH%d carries a %.4g V offset while the bands are centring themselves, so it is drawn twice-shifted",
                                 ch + 1, off);
                    continue;
                }
                double top = (hi + off) / vd, bot = (lo + off) / vd;   /* in divisions from centre */
                if (top > 4.05 || bot < -4.05)
                    snprintf(why, sizeof why,
                             "CH%d runs off the screen: %.4g..%.4g V at %.4g V/div is %.1f..%.1f divisions",
                             ch + 1, lo, hi, vd, bot, top);
            }
            /* Autoset with nothing to look at falls back to its defaults, and the scope then
               shows a flat line whatever the circuit is doing. That is the same complaint as
               "no output on the scope", so it is a failure here rather than a silent pass. */
            if (!why[0] && with_data == 0)
                snprintf(why, sizeof why, "no channel had any history to autoset from after %d steps", guard);
            if (!why[0]) {
                int tc = ui->trigger_channel;
                if (tc >= 0 && tc < ui->scope_num_channels) {
                    double times[MAX_HISTORY], values[MAX_HISTORY];
                    int n = simulation_get_history(sim, ui->scope_channels[tc].probe_idx, times, values, MAX_HISTORY);
                    if (n >= 10) {
                        double lo = values[0], hi = values[0];
                        for (int i = 1; i < n; i++) { if (values[i] < lo) lo = values[i]; if (values[i] > hi) hi = values[i]; }
                        double lvl = ui->trigger_level;
                        /* a flat channel cannot be triggered on at all, and that is not a fault */
                        if (hi - lo > 1e-6 && (lvl <= lo || lvl >= hi))
                            snprintf(why, sizeof why,
                                     "trigger level %.4g V is outside CH%d's %.4g..%.4g V, so it never fires",
                                     lvl, tc + 1, lo, hi);
                    }
                }
            }
        }
        printf("[%s] autoset %-28s vdiv=%-9.4g tdiv=%-9.4g trig=CH%d @ %-8.4g %s\n",
               why[0] ? "FAIL" : " OK ", ti->name, ui->scope_volt_div, ui->scope_time_div,
               ui->trigger_channel + 1, ui->trigger_level, why);
        fflush(stdout);
        if (why[0]) fails++;
        simulation_free(sim);
        circuit_free(c);
    }
    free(ui);
    printf("\nautoset-test: %d templates, %d where Autoset leaves something off the screen or untriggerable\n",
           total, fails);
    return fails;
}

/* Headless UI layout self-check: no SDL window needed. */
static int layout_test(void) {
    int fails = 0;
    UIState *ui = calloc(1, sizeof *ui);
    ui_init(ui);
    static const int sizes[][2] = { {1024, 600}, {1280, 720}, {1920, 1080} };
    for (unsigned k = 0; k < sizeof sizes / sizeof sizes[0]; k++) {
        ui->window_width = sizes[k][0]; ui->window_height = sizes[k][1];
        for (int tab = 0; tab < 3; tab++) {
            ui->scope_ctl_tab = tab;
            ui_update_layout(ui);
            Button *b[SCOPE_BTN_N]; ui_scope_buttons(ui, b);
            int visible = 0;
            for (int i = 0; i < SCOPE_BTN_N; i++) {
                if (b[i]->bounds.w <= 0) continue;
                visible++;
                if (b[i]->bounds.x < ui->scope_rect.x || b[i]->bounds.x + b[i]->bounds.w > ui->scope_rect.x + ui->scope_rect.w + 2) {
                    printf("[FAIL] layout %dx%d tab %d: scope button '%s' outside the scope width\n", sizes[k][0], sizes[k][1], tab, b[i]->label); fails++;
                }
                for (int j = i + 1; j < SCOPE_BTN_N; j++) {
                    if (b[j]->bounds.w <= 0) continue;
                    if (rects_overlap(&b[i]->bounds, &b[j]->bounds)) { printf("[FAIL] layout %dx%d tab %d: '%s' overlaps '%s'\n", sizes[k][0], sizes[k][1], tab, b[i]->label, b[j]->label); fails++; }
                }
            }
            if (ui->scope_buttons_bottom > ui->window_height - STATUSBAR_HEIGHT) { printf("[FAIL] layout %dx%d: scope buttons below the status bar\n", sizes[k][0], sizes[k][1]); fails++; }
            printf("[ OK ] layout %dx%d tab %d: %d visible scope buttons, none overlap, bottom %d\n", sizes[k][0], sizes[k][1], tab, visible, ui->scope_buttons_bottom);
        }
    }
    /* ---- pop-out front panel: every knob is hit-testable and every knob moves something ----
       The panel is laid out in the pop-out window's coordinates, so this drives
       ui_layout_scope_panel directly with a window size and then does what a drag does. */
    {
        const int PW = 1120, PH = 700;
        ui->scope_panel_active = true;
        ui->scope_rect = (Rect){18, 30, PW - 250 - 36, PH - 130};
        ui_layout_scope_panel(ui, PW, PH);
        ui->scope_num_channels = 4;
        ui->scope_selected_channel = 0;
        ui->scope_volt_div = 1.0;
        ui->scope_time_div = 1e-3;
        ui->trigger_level = 0.0;
        ui->scope_channels[0].offset = 0.0;
        ui_set_brightness(ui, 1.0f);

        int knobs_ok = 0;
        for (int k = 0; k < KNOB_COUNT; k++) {
            ScopeKnob *kn = &ui->scope_knobs[k];
            /* the knob must be inside the panel column and clear of the screen */
            if (kn->cx - kn->r < PW - 250 || kn->cx + kn->r > PW ||
                kn->cy - kn->r < 0 || kn->cy + kn->r > PH) {
                printf("[FAIL] knob %d outside the panel column\n", k); fails++;
            }
            if (ui_scope_knob_at(ui, kn->cx, kn->cy) != k) {
                printf("[FAIL] knob %d does not hit-test at its own centre\n", k); fails++;
            }
            for (int j = 0; j < KNOB_COUNT; j++) {
                if (j == k) continue;
                ScopeKnob *o = &ui->scope_knobs[j];
                int dx = kn->cx - o->cx, dy = kn->cy - o->cy;
                if (dx * dx + dy * dy < (kn->r + o->r) * (kn->r + o->r)) {
                    printf("[FAIL] knobs %d and %d overlap\n", k, j); fails++;
                }
            }
            knobs_ok++;
        }

        /* The vertical section drives whichever input the CHANNEL knob is on: VOLTS/DIV writes
           that channel's own volts/div rather than a global one, so three probes can be on three
           different scales with one set of knobs. */
        {
            ui->scope_num_channels = 3;
            for (int ch = 0; ch < MAX_PROBES; ch++) {
                ui->scope_channels[ch].enabled = (ch < 3);
                ui->scope_channels[ch].volt_div = 0.0;      /* all following the main setting */
            }
            ui->scope_volt_div = 1.0;
            ui->scope_selected_channel = 1;                 /* point the section at CH2 */

            for (int ch = 0; ch < 3; ch++)
                if (ui_channel_volt_div(ui, ch) != 1.0) {
                    printf("[FAIL] channel %d does not follow the main setting (own %g, main %g)\n",
                           ch, ui->scope_channels[ch].volt_div, ui->scope_volt_div); fails++;
                }

            for (int i = 0; i < 40 && ui->scope_channels[1].volt_div <= 0; i++)
                ui_scope_knob_drag(ui, KNOB_VOLTS, -1);
            if (!(ui->scope_channels[1].volt_div > 1.0)) {
                printf("[FAIL] VOLTS/DIV did not take CH2 coarser than the main 1 V (got %g)\n",
                       ui->scope_channels[1].volt_div); fails++;
            }
            if (ui->scope_channels[0].volt_div != 0.0 || ui->scope_channels[2].volt_div != 0.0) {
                printf("[FAIL] driving CH2 moved another channel\n"); fails++;
            }
            if (ui_channel_volt_div(ui, 0) != 1.0) {
                printf("[FAIL] the untouched channels stopped following the main setting\n"); fails++;
            }

            /* and a click on an INPUTS row re-points the section */
            ui->scope_panel_active = true;
            Rect r2 = ui->scope_input_rows[2];
            if (ui_scope_input_row_at(ui, r2.x + 4, r2.y + 4) != 2) {
                printf("[FAIL] the INPUTS row for CH3 does not hit-test\n"); fails++;
            }
            if (ui_scope_input_row_at(ui, r2.x + 4, r2.y - 40) == 2) {
                printf("[FAIL] a click above the CH3 row still selects it\n"); fails++;
            }
            if (ui_scope_input_row_at(ui, ui->scope_input_rows[5].x + 4, ui->scope_input_rows[5].y + 4) != -1) {
                printf("[FAIL] a row for a channel that is not there is clickable\n"); fails++;
            }
            printf("[ OK ] one vertical section: VOLTS/DIV drives the selected input only, and the\n");
            printf("       INPUTS rows re-point it - channels that are not there are not clickable\n");

            for (int ch = 0; ch < MAX_PROBES; ch++) ui->scope_channels[ch].volt_div = 0.0;
            ui->scope_selected_channel = 0;
        }

        /* time/div and the channel selector are detented: a long drag has to emit an action */
        int act = 0;
        for (int i = 0; i < 40 && !act; i++) act = ui_scope_knob_drag(ui, KNOB_TIME, -1);
        if (act != UI_ACTION_SCOPE_TIME_UP) { printf("[FAIL] TIME/DIV knob emitted %d, not TIME_UP\n", act); fails++; }
        act = 0;
        for (int i = 0; i < 40 && !act; i++) act = ui_scope_knob_drag(ui, KNOB_CHANNEL, -1);
        if (act != UI_ACTION_SCOPE_TRIG_CH) { printf("[FAIL] CHANNEL knob emitted %d, not TRIG_CH\n", act); fails++; }

        /* the continuous knobs move their own value, and dragging up increases it */
        double t0 = ui->trigger_level;
        ui_scope_knob_drag(ui, KNOB_TRIGGER, -30);
        if (!(ui->trigger_level > t0)) { printf("[FAIL] TRIG LEVEL knob did not rise on an upward drag\n"); fails++; }
        ui_scope_knob_drag(ui, KNOB_TRIGGER, +60);
        if (!(ui->trigger_level < t0)) { printf("[FAIL] TRIG LEVEL knob did not fall on a downward drag\n"); fails++; }

        double o0 = ui->scope_channels[0].offset;
        ui_scope_knob_drag(ui, KNOB_POSITION, -30);
        if (!(ui->scope_channels[0].offset > o0)) { printf("[FAIL] POSITION knob did not move the channel offset\n"); fails++; }

        float b0 = ui->brightness;
        ui_scope_knob_drag(ui, KNOB_INTENSITY, +200);
        if (!(ui->brightness < b0)) { printf("[FAIL] INTENSITY knob did not dim\n"); fails++; }
        ui_scope_knob_drag(ui, KNOB_INTENSITY, -400);
        if (!(ui->brightness > 0.9f)) { printf("[FAIL] INTENSITY knob did not come back up\n"); fails++; }

        /* a click away from every knob must not grab one */
        if (ui_scope_knob_at(ui, ui->scope_rect.x + 10, ui->scope_rect.y + 10) != -1) {
            printf("[FAIL] a click on the screen grabbed a knob\n"); fails++;
        }
        ui->scope_panel_active = false;
        if (ui_scope_knob_at(ui, ui->scope_knobs[0].cx, ui->scope_knobs[0].cy) != -1) {
            printf("[FAIL] knobs are live while the panel is not shown (docked scope)\n"); fails++;
        }
        printf("[ OK ] scope panel: %d knobs laid out, hit-tested and driven\n", knobs_ok);
        ui_set_brightness(ui, 1.0f);
    }

    /* An imported vendor model has to be placeable, not just present in the library: it
       belongs in the Circuits tab's subcircuit list, which is where a user reaches for it. */
    {
        static const char *netlist =
            ".SUBCKT LAYOUTCHK 1 2\n"
            "R1 1 2 1k\n"
            ".ENDS\n";
        char msg[128] = "";
        int before = ui->num_subcircuit_items;
        int n = spice_import_text(netlist, msg, sizeof msg);
        ui_update_layout(ui);
        int after = ui->num_subcircuit_items;
        int found = 0;
        for (int i = 0; i < ui->num_subcircuit_items; i++)
            if (!strcmp(ui->subcircuit_items[i].label, "LAYOUTCHK")) found = 1;
        if (n != 1 || after != before + 1 || !found) {
            printf("[FAIL] imported model is not in the subcircuit palette (imported %d, items %d -> %d, found %d)\n",
                   n, before, after, found);
            fails++;
        } else {
            printf("[ OK ] imported SPICE model appears in the palette as '%s' (%d subcircuit item%s)\n",
                   ui->subcircuit_items[found ? 0 : 0].label, after, after == 1 ? "" : "s");
        }
    }

    /* every template is in the Circuits palette, exactly once */
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        int n = 0;
        for (int i = 0; i < ui->num_circuit_items; i++) if (ui->circuit_items[i].circuit_type == t) n++;
        if (n != 1) { const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t); printf("[FAIL] template %s appears %d times in the palette\n", ti ? ti->name : "?", n); fails++; }
    }
    printf("[ OK ] %d templates, %d circuit palette items\n", CIRCUIT_TYPE_COUNT - 1, ui->num_circuit_items);
    /* every part has a category before Circuits, and every part category has at least one item */
    int per_cat[PCAT_COUNT] = {0};
    for (int i = 0; i < ui->num_palette_items; i++) {
        if (ui->palette_items[i].category >= PCAT_CIRCUITS) { printf("[FAIL] palette item '%s' has no part category\n", ui->palette_items[i].label); fails++; }
        else per_cat[ui->palette_items[i].category]++;
        if (ui->palette_items[i].label && strlen(ui->palette_items[i].label) > 9) { printf("[FAIL] palette label '%s' too long for a 60 px button\n", ui->palette_items[i].label); fails++; }
    }
    for (int c = 0; c < PCAT_CIRCUITS; c++) if (per_cat[c] == 0) { printf("[FAIL] part category %d (%s) is empty\n", c, ui->categories[c].name); fails++; }
    for (int i = 0; i < ui->num_circuit_items; i++)
        if (strlen(ui->circuit_items[i].label) > 9) { printf("[FAIL] circuit label '%s' too long\n", ui->circuit_items[i].label); fails++; }
    /* action id sanity: every id below 100 must be unique, or two buttons do the same thing */
    if (UI_ACTION_SCOPE_STACK == UI_ACTION_SPOTLIGHT || UI_ACTION_SCOPE_TRACK == UI_ACTION_SPOTLIGHT) { printf("[FAIL] UI action id collision\n"); fails++; }
    {
        static const int ids[] = { UI_ACTION_ZOOM_IN, UI_ACTION_ZOOM_OUT, UI_ACTION_ZOOM_FIT,
                                   UI_ACTION_SCOPE_AC, UI_ACTION_SCOPE_FIT, UI_ACTION_SCREENSHOT,
                                   UI_ACTION_EXPORT_SVG, UI_ACTION_SPOTLIGHT, UI_ACTION_SCOPE_STACK };
        for (unsigned a = 0; a < sizeof ids / sizeof ids[0]; a++)
            for (unsigned b = a + 1; b < sizeof ids / sizeof ids[0]; b++)
                if (ids[a] == ids[b]) { printf("[FAIL] two UI actions share id %d\n", ids[a]); fails++; }
    }

    /* ---- probe names are the scope's channel names ---- */
    {
        if (probe_label_is_default("Vout")) { printf("[FAIL] 'Vout' read as a default probe label\n"); fails++; }
        if (!probe_label_is_default("CH3")) { printf("[FAIL] 'CH3' not read as a default probe label\n"); fails++; }
        if (!probe_label_is_default(""))    { printf("[FAIL] an empty label is not a default\n"); fails++; }
        if (probe_label_is_default("CH"))   { printf("[FAIL] 'CH' with no number read as a default\n"); fails++; }
        if (probe_label_is_default("CLK"))  { printf("[FAIL] 'CLK' read as a default\n"); fails++; }
        memset(ui->scope_channels, 0, sizeof ui->scope_channels);
        if (strcmp(ui_channel_name(ui, 2), "CH3")) { printf("[FAIL] an unnamed channel is not CH3\n"); fails++; }
        snprintf(ui->scope_channels[2].name, sizeof ui->scope_channels[2].name, "Vout");
        if (strcmp(ui_channel_name(ui, 2), "Vout")) { printf("[FAIL] a named channel does not report its name\n"); fails++; }
        printf("[ OK ] probe names: defaults renumber, typed names survive, the scope reads them\n");
    }

    /* ---- pan and zoom: the controls a laptop without a middle button needs ---- */
    {
        int pan_items = 0;
        for (int i = 0; i < ui->num_palette_items; i++)
            if (ui->palette_items[i].is_tool && ui->palette_items[i].tool_type == TOOL_PAN) pan_items++;
        if (pan_items != 1) { printf("[FAIL] the Pan tool appears %d times in the palette\n", pan_items); fails++; }

        ui->window_width = 1280; ui->window_height = 720;
        ui_update_layout(ui);
        Button *zb[4] = { &ui->btn_zoom_out, &ui->btn_zoom_in, &ui->btn_zoom_fit, &ui->btn_import_spice };
        for (int i = 0; i < 4; i++) {
            if (zb[i]->bounds.y < 0 || zb[i]->bounds.y + zb[i]->bounds.h > TOOLBAR_HEIGHT) {
                printf("[FAIL] zoom button '%s' is not inside the toolbar\n", zb[i]->label); fails++;
            }
            if (zb[i]->bounds.x + zb[i]->bounds.w > ui->window_width) {
                printf("[FAIL] zoom button '%s' runs off the right edge\n", zb[i]->label); fails++;
            }
            if (rects_overlap(&zb[i]->bounds, &ui->btn_screenshot.bounds)) {
                printf("[FAIL] zoom button '%s' overlaps the screenshot button\n", zb[i]->label); fails++;
            }
            if (rects_overlap(&zb[i]->bounds, &ui->speed_slider)) {
                printf("[FAIL] zoom button '%s' overlaps the speed slider\n", zb[i]->label); fails++;
            }
            for (int j = i + 1; j < 4; j++)
                if (rects_overlap(&zb[i]->bounds, &zb[j]->bounds)) {
                    printf("[FAIL] zoom buttons '%s' and '%s' overlap\n", zb[i]->label, zb[j]->label); fails++;
                }
            int cx = zb[i]->bounds.x + zb[i]->bounds.w / 2, cy = zb[i]->bounds.y + zb[i]->bounds.h / 2;
            int want = (i == 0) ? UI_ACTION_ZOOM_OUT : (i == 1) ? UI_ACTION_ZOOM_IN
                     : (i == 2) ? UI_ACTION_ZOOM_FIT : UI_ACTION_IMPORT_SPICE;
            if (ui_handle_click(ui, cx, cy, true) != want) {
                printf("[FAIL] clicking '%s' does not return its zoom action\n", zb[i]->label); fails++;
            }
        }
        printf("[ OK ] pan tool present; zoom and SPICE-import buttons in the toolbar, each returns its own action\n");
    }

    /* ---- the palette opens as a table of contents, not a wall of buttons ---- */
    {
        int open = 0;
        for (int c = 0; c < PCAT_COUNT; c++) if (!ui->categories[c].collapsed) open++;
        if (ui->categories[PCAT_TOOLS].collapsed) { printf("[FAIL] the Tools category starts collapsed\n"); fails++; }
        if (open != 1) { printf("[FAIL] %d palette categories start open, expected only Tools\n", open); fails++; }
        int groups_open = 0;
        for (int g = 0; g < TG_COUNT; g++) if (!ui->circuit_group_collapsed[g]) groups_open++;
        if (groups_open != 0) { printf("[FAIL] %d template groups start open, expected none\n", groups_open); fails++; }
        printf("[ OK ] palette starts with Tools open and all %d template groups closed\n", (int)TG_COUNT);
    }
    printf("[ OK ] %d palette items in %d categories\n", ui->num_palette_items, (int)PCAT_CIRCUITS);
    printf("%d layout checks failed\n", fails);
    free(ui);
    return fails;
}

static void usage(void) {
    printf("Options:\n"
           "  --template NAME      place a circuit template (short or full name) and start it\n"
           "  --size WxH           window size\n"
           "  --shot FILE.bmp      save the window at frame N (see --frame, default 90)\n"
           "  --frame N            frame index for --shot / first frame of --record\n"
           "  --record DIR N EVERY save N frames, one every EVERY frames, as DIR/frame_XXX.bmp\n"
           "  --scroll PX          scroll the left palette by PX pixels (screenshots)\n"
           "  --keys S FRAME EVERY type S one char every EVERY frames from FRAME (^ opens Spotlight, | is Enter)\n"
           "  --click X,Y,FRAME    left-click at X,Y on that frame (repeatable, up to 12)\n"
           "  --drag X1,Y1,X2,Y2,FRAME  press at X1,Y1, move to X2,Y2 and release, on that frame\n"
           "  --xy FILE            load 'x y' coordinate pairs into the X-Y Plotter template\n"
           "  --tab parts|circuits left panel tab\n"
           "  --exit               quit when the shot / recording is done\n"
           "  --no-update-check    do not query GitHub for a newer release (also CIRCUIT_TOY_NO_UPDATE=1)\n"
           "  --no-auto-update     check, but do not install by itself (also CIRCUIT_TOY_NO_AUTO_UPDATE=1)\n"
           "  --version            print the version and exit\n"
           "  --update-check       query the latest GitHub release and exit; --update-now also installs it\n"
           "  --layout-test        headless UI layout self-check (no window)\n"
           "  --crashlog           print the start-up / crash log and exit\n");
}

/* The app is a GUI subsystem binary, so it owns no console. When it is run from a terminal
   with arguments, borrow the terminal's console so --version, --update-check, --layout-test
   and the rest still print where the person typing them can see. Output that is redirected to
   a pipe or a file already works without this and must not be clobbered, hence the isatty-ish
   check on each handle before reopening it. */
static void attach_parent_console(void) {
#ifdef _WIN32
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) return;
    if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_UNKNOWN) {
        FILE *f; freopen_s(&f, "CONOUT$", "w", stdout);
    }
    if (GetFileType(GetStdHandle(STD_ERROR_HANDLE)) == FILE_TYPE_UNKNOWN) {
        FILE *f; freopen_s(&f, "CONOUT$", "w", stderr);
    }
#endif
}

int main(int argc, char *argv[]) {
    if (argc > 1) attach_parent_console();
    const char *cli_template = NULL, *cli_shot = NULL, *cli_record = NULL, *cli_size = NULL;
    int cli_frame = 90, cli_rec_n = 0, cli_rec_every = 1, cli_scroll = -1, cli_tab = -1; bool cli_exit = false, no_update = false, no_auto_update = false;
    const char *cli_keys = NULL; int cli_keys_frame = 30, cli_keys_every = 6;
    struct { int x, y, x2, y2, frame; bool drag; } cli_mouse[12]; int cli_mouse_n = 0;
    const char *cli_xy = NULL;
    bool cli_popout = false;
    const char *cli_spice = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--template") && i + 1 < argc) cli_template = argv[++i];
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) cli_shot = argv[++i];
        else if (!strcmp(argv[i], "--frame") && i + 1 < argc) cli_frame = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--size") && i + 1 < argc) cli_size = argv[++i];
        else if (!strcmp(argv[i], "--record") && i + 3 < argc) { cli_record = argv[++i]; cli_rec_n = atoi(argv[++i]); cli_rec_every = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--exit")) cli_exit = true;
        else if (!strcmp(argv[i], "--no-update-check")) no_update = true;
        else if (!strcmp(argv[i], "--no-auto-update")) no_auto_update = true;
        else if (!strcmp(argv[i], "--version")) { printf("%s\n", APP_VERSION); return 0; }
        else if (!strcmp(argv[i], "--update-check") || !strcmp(argv[i], "--update-now")) {
            UpdaterState st; updater_init(&st); updater_check_async(&st); updater_wait(&st);
            char tag[128]; int failed = 0; updater_checked(&st, &failed);
            int avail = updater_available(&st, tag, sizeof tag);
            printf("installed %s, latest %s -> %s\n", getenv("CIRCUIT_TOY_FAKE_VERSION") ? getenv("CIRCUIT_TOY_FAKE_VERSION") : APP_VERSION,
                   failed ? "(query failed)" : tag, avail ? "update available" : "up to date");
            int rc = 0;
            if (!strcmp(argv[i], "--update-now")) { char msg[200]; rc = updater_install(&st, msg, sizeof msg) ? 0 : 3; printf("%s\n", msg); }
            updater_shutdown(&st);
            return failed ? 2 : rc;
        }
        else if (!strcmp(argv[i], "--scroll") && i + 1 < argc) cli_scroll = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--keys") && i + 3 < argc) { cli_keys = argv[++i]; cli_keys_frame = atoi(argv[++i]); cli_keys_every = atoi(argv[++i]); }
        else if ((!strcmp(argv[i], "--click") || !strcmp(argv[i], "--drag")) && i + 1 < argc) {
            bool drag = !strcmp(argv[i], "--drag");
            if (cli_mouse_n < (int)(sizeof cli_mouse / sizeof cli_mouse[0])) {
                int a = 0, b = 0, c2 = 0, d = 0, f = 60;
                int got = drag ? sscanf(argv[i + 1], "%d,%d,%d,%d,%d", &a, &b, &c2, &d, &f)
                               : sscanf(argv[i + 1], "%d,%d,%d", &a, &b, &f);
                if (got >= (drag ? 4 : 2)) {
                    cli_mouse[cli_mouse_n].x = a; cli_mouse[cli_mouse_n].y = b;
                    cli_mouse[cli_mouse_n].x2 = drag ? c2 : a; cli_mouse[cli_mouse_n].y2 = drag ? d : b;
                    cli_mouse[cli_mouse_n].frame = f; cli_mouse[cli_mouse_n].drag = drag;
                    cli_mouse_n++;
                } else fprintf(stderr, "bad %s argument: %s\n", argv[i], argv[i + 1]);
            }
            i++;
        }
        else if (!strcmp(argv[i], "--xy") && i + 1 < argc) cli_xy = argv[++i];
        else if (!strcmp(argv[i], "--tab") && i + 1 < argc) cli_tab = !strcmp(argv[++i], "circuits") ? 1 : 0;
        else if (!strcmp(argv[i], "--popout")) cli_popout = true;
        else if (!strcmp(argv[i], "--import-spice") && i + 1 < argc) cli_spice = argv[++i];
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(); return 0; }
        else if (!strcmp(argv[i], "--layout-test")) return layout_test();
        else if (!strcmp(argv[i], "--autoset-test")) return autoset_test();
        else if (!strcmp(argv[i], "--place-test")) return place_test();
        else if (!strcmp(argv[i], "--crashlog")) { crashlog_dump_last(); return 0; }
        else { fprintf(stderr, "Unknown option: %s\n", argv[i]); usage(); return 2; }
    }

    printf("Circuit Playground v%s\n", APP_VERSION);
    printf("A circuit simulator inspired by Paul Falstad's circuit.js and The Powder Toy\n\n");

    /* From here on every milestone is written to the log, flushed, so a window that never
       appears still says how far it got. */
    crashlog_init(APP_VERSION);
    printf("log: %s\n", crashlog_path());
    crashlog_note("App state %zu KB", sizeof(App) / 1024);
    crashlog_note("start-up: about to SDL_Init(VIDEO | TIMER)");

    // Initialize SDL (video + timer; audio subsystem was removed with the microphone feature)
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        crashlog_note("FATAL: SDL_Init failed: %s", SDL_GetError());
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    crashlog_note("SDL_Init ok, video driver '%s'", SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "?");

    // Create application
    /* Not on the stack. The App carries the whole UI state by value - the palette, the scope's
       capture buffers, every panel rect - and at that size it sits close enough to the default
       1 MB stack that whether it fits depends on the compiler's frame layout. It fitted in the
       build here and overflowed in the one CI ships: v3.19.2 through v3.21.0 all died with
       0xC00000FD a moment after start-up, which is the instant crash people reported. */
    static App app;
    memset(&app, 0, sizeof app);
    crashlog_note("start-up: app_init (window, renderer, circuit, simulation)");
    if (!app_init(&app)) {
        crashlog_note("FATAL: app_init failed: %s", SDL_GetError());
        fprintf(stderr, "Application initialization failed\n");
        SDL_Quit();
        return 1;
    }
    crashlog_note("app_init ok");

    printf("Application initialized successfully\n");
    printf("Press F1 for keyboard shortcuts\n\n");

    if (cli_spice) {
        char msg[256] = "";
        int n = spice_import_file(cli_spice, msg, sizeof msg);
        if (n > 0) printf("SPICE import: %s\n", msg);
        else fprintf(stderr, "SPICE import failed: %s\n", msg);
    }
    if (cli_xy) {
        int n = arb_load_xy_file(cli_xy);
        if (n) printf("Loaded %d X-Y points from %s\n", n, cli_xy);
        else fprintf(stderr, "Could not read X-Y data from %s\n", cli_xy);
    }
    if (cli_size) {
        int w = 0, h = 0;
        if (sscanf(cli_size, "%dx%d", &w, &h) == 2 && w > 200 && h > 200) SDL_SetWindowSize(app.window, w, h);
    }
    if (cli_shot) { strncpy(app.cli_shot_path, cli_shot, sizeof app.cli_shot_path - 1); }
    if (cli_record) { strncpy(app.cli_record_dir, cli_record, sizeof app.cli_record_dir - 1); app.cli_record_frames = cli_rec_n; app.cli_record_every = cli_rec_every; }
    app.cli_shot_frame = cli_frame;
    if (cli_keys) { strncpy(app.cli_keys, cli_keys, sizeof app.cli_keys - 1); app.cli_keys_frame = cli_keys_frame; app.cli_keys_every = cli_keys_every; }
    for (int i = 0; i < cli_mouse_n; i++) {
        app.cli_mouse[i].x = cli_mouse[i].x; app.cli_mouse[i].y = cli_mouse[i].y;
        app.cli_mouse[i].x2 = cli_mouse[i].x2; app.cli_mouse[i].y2 = cli_mouse[i].y2;
        app.cli_mouse[i].frame = cli_mouse[i].frame; app.cli_mouse[i].drag = cli_mouse[i].drag;
    }
    app.cli_mouse_n = cli_mouse_n;
    app.skip_update_check = no_update || cli_shot || cli_record;   // scripted runs never phone home
    app.no_auto_update = no_auto_update || getenv("CIRCUIT_TOY_NO_AUTO_UPDATE") != NULL;
    app_update_check(&app);
    if (cli_scroll >= 0) app.ui.palette_scroll_offset = cli_scroll;
    if (cli_tab >= 0) app.ui.left_tab = cli_tab;
    app.cli_exit = cli_exit;
    if (cli_popout) app_scope_popout(&app, true);   /* --shot then also writes <name>_scope.bmp */
    if (cli_template) {
        CircuitTemplateType found = CIRCUIT_NONE;
        for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
            const CircuitTemplateInfo *info = circuit_template_get_info((CircuitTemplateType)t);
            if (info && (!_stricmp(info->name, cli_template) || !_stricmp(info->short_name, cli_template))) { found = (CircuitTemplateType)t; break; }
        }
        if (found == CIRCUIT_NONE) {
            fprintf(stderr, "Unknown template '%s'. Available:\n", cli_template);
            for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
                const CircuitTemplateInfo *info = circuit_template_get_info((CircuitTemplateType)t);
                if (info) fprintf(stderr, "  %-8s %s\n", info->short_name, info->name);
            }
        } else {
            // a few frames first so the resize event lands and the canvas rect is laid out
            for (int k = 0; k < 4; k++) { app_handle_events(&app); app_update(&app); app_render(&app); SDL_Delay(16); }
            app_place_template_centered(&app, found);
        }
    }

    // Main loop
    crashlog_note("entering the main loop");
    long frames = 0;
    while (app.running) {
        app_handle_events(&app);
        app_update(&app);
        app_render(&app);
        /* the first frames are where a driver problem shows up, and the hundredth says the thing
           is really running; after that, silence */
        if (++frames == 1 || frames == 10 || frames == 100) crashlog_note("frame %ld drawn", frames);

        // Cap frame rate to ~60 FPS
        SDL_Delay(16);
    }

    crashlog_note("main loop ended");

    // Cleanup
    app_shutdown(&app);
    SDL_Quit();
    crashlog_ok();

    printf("Application closed\n");
    return 0;
}
