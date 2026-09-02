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
#include "netlist.h"
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

/* --prop-test: every row the properties panel offers has to be a row that can be edited. The
   panel lists a field, the click carries its property type, and applying runs one long switch
   over (property, component type). A part that is listed but missing from that switch shows a
   field, takes the typing, and answers "Invalid value" - the value springs back and nothing says
   why. This asks the panel itself what it offers for each part, then tries to apply each of
   those rows, so the two lists cannot drift apart.

   It needs a renderer because the panel builds its row list while drawing it; the dummy video
   driver is enough (SDL_VIDEODRIVER=dummy), and that is what CI runs it under. */
/* --bounce-test: a settled trace has to hold its vertical position, not just its horizontal one.
 *
 * --trig-test already checks that a repeating waveform stands still left to right. This checks the
 * other axis, and it caught something that had been on the screen the whole time: an AC-coupled or
 * fitted channel centred itself on the mean of the captured window, and the capture is a ragged
 * fraction of a cycle whose length changes from frame to frame (250 samples, then 225). The mean
 * of a partial cycle depends on which partial cycle it is, so the zero line moved every frame and
 * took the trace with it - a slow shimmer of a few pixels on a waveform that was otherwise
 * perfectly triggered. It is exactly the kind of fault a screenshot cannot show and a person
 * watching the screen for ten seconds cannot miss.
 *
 * So this drives the real render path frame by frame, with the real capture and the real band
 * arithmetic, and measures where each channel's zero line lands in pixels. A settled, triggered
 * channel may not move more than a pixel over sixty frames.
 */
/* --shard i/n splits the template list between processes, the same way template_smoke does. The
   battery cannot finish faster than its longest single suite, and --bounce-test became that suite
   the day it was written: it renders sixty frames of every template through the real scope, which
   is 503 seconds on its own against 670 for the whole battery. Quartered, it stops being the
   critical path. Read before the mode flags, because those return straight out of the loop. */
static int g_shard_i = 0, g_shard_n = 1;
static int app_shard_skip(int t) { return g_shard_n > 1 && (t % g_shard_n) != g_shard_i; }

static int bounce_test(double dt_force) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("bounce-test: no video (%s) - skipped\n", SDL_GetError());
        return 0;
    }
    SDL_Window *win = SDL_CreateWindow("bounce-test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       1600, 1000, SDL_WINDOW_HIDDEN);
    SDL_Renderer *ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE) : NULL;
    if (!ren) {
        printf("bounce-test: no renderer (%s) - skipped\n", SDL_GetError());
        if (win) SDL_DestroyWindow(win);
        SDL_Quit();
        return 0;
    }

    /* Thirty frames, not sixty, which halves a suite that was 503 s of a 670 s battery on its own
       - and on a four-core CI runner, where three suites run at a time, total work is what decides
       the wall clock. Thirty cost one template of detection when the centring was a whole-cycle
       average; against the midpoint estimator that replaced it they catch the same 16 as sixty do,
       so there is nothing being traded away here any more. */
    enum { FRAMES = 30 };
    const double FRAME_DT = 1.0 / 60.0;
    const double LIMIT_PX = 1.0;

    UIState *ui = calloc(1, sizeof *ui);
    ui_init(ui);
    ui->window_width = 1600; ui->window_height = 1000;
    ui_update_layout(ui);

    int fails = 0, judged = 0, skipped = 0, moving = 0, stepped = 0;
    /* BOUNCE_ONLY=substring runs one template, so SCOPE_DEBUG output is about one circuit instead
       of 187. Chasing a pixel needs the numbers for the two frames that disagree. */
    const char *only = getenv("BOUNCE_ONLY");
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        const CircuitTemplateInfo *ti = circuit_template_get_info((CircuitTemplateType)t);
        if (!ti) continue;
        if (only && only[0] && !strstr(ti->name, only)) continue;
        if (app_shard_skip(t)) continue;
        Circuit *c = circuit_create();
        if (circuit_place_template(c, (CircuitTemplateType)t, 0, 0) <= 0) { circuit_free(c); continue; }
        Simulation *sim = simulation_create(c);
        ui_scope_apply_template_preset(ui, (CircuitTemplateType)t);
        ui->scope_num_channels = c->num_probes < MAX_PROBES ? c->num_probes : MAX_PROBES;
        for (int ch = 0; ch < MAX_PROBES; ch++) {
            ui->scope_channels[ch].enabled = ch < ui->scope_num_channels;
            ui->scope_channels[ch].probe_idx = ch;
        }
        /* Same reason as trig-test: a swept source has no one frequency, and what the screen shows
           then depends on where in the sweep the run stopped. */
        for (int i = 0; i < c->num_components; i++)
            if (c->components[i]->type == COMP_AC_VOLTAGE)
                c->components[i]->props.ac_voltage.frequency_sweep.enabled = false;

        if (ui->scope_num_channels < 1 || !simulation_dc_analysis(sim)) {
            simulation_free(sim); circuit_free(c); skipped++; continue;
        }
        simulation_auto_time_step(sim);
        { double dtp = simulation_scope_time_step(sim, ui->scope_time_div);
          if (dtp > 0 && dtp < sim->time_step) simulation_set_time_step(sim, dtp); }
        /* `--bounce-test DT` forces the step. Circuits do not all settle at the same rate, and a
           rate depends on the step they are taking: an answer measured at one dt can be a property
           of that dt rather than of the circuit. Running the suite at two or three steps is how you
           tell the difference - a real display fault reads the same at all of them. */
        if (dt_force > 0) { simulation_enable_adaptive(sim, false); simulation_set_time_step(sim, dt_force); }
        simulation_set_history_span(sim, 20.0 * ui->scope_time_div);
        simulation_start(sim);
        if (dt_force > 0) simulation_set_time_step(sim, dt_force);
        /* Settle first: a start-up transient moving the trace is the trace being right. Rather
           than trusting one fixed time for 187 different circuits, watch the waveform and stop
           when it stops changing - an oscillator building up over milliseconds and an RC settling
           in microseconds both get what they need, and neither holds up the suite. */
        int alive = 1;
        {
            double last_lo = 1e300, last_hi = -1e300;
            int stable = 0, rounds = 0;
            double chunk = ui->scope_time_div;      /* one division at a time */
            while (stable < 3 && rounds++ < 300 && alive) {
                double until = sim->time + chunk;
                int guard = 0;
                while (sim->time < until && guard++ < 200000)
                    if (!simulation_step(sim)) { alive = 0; break; }
                if (!alive) break;
                double lo = 1e300, hi = -1e300;
                static double th[MAX_HISTORY], tv[MAX_HISTORY];
                int hn = simulation_get_history(sim, ui->scope_channels[0].probe_idx, th, tv, MAX_HISTORY);
                for (int i = 0; i < hn; i++) { if (tv[i] < lo) lo = tv[i]; if (tv[i] > hi) hi = tv[i]; }
                double span = hi - lo;
                int same = (fabs(lo - last_lo) <= 1e-3 * (span + 1e-12) &&
                            fabs(hi - last_hi) <= 1e-3 * (span + 1e-12));
                stable = same ? stable + 1 : 0;
                last_lo = lo; last_hi = hi;
            }
        }
        if (!alive) { simulation_free(sim); circuit_free(c); skipped++; continue; }

        /* Ask each channel what it is before judging it, and ask it per channel rather than per
           template - a schematic can put a clean oscillation on one probe and a waveform still
           filling out on the next, and the second one has nothing to say about the first.
           The invariant below - same envelope and same cycle count means same waveform, so the
           zero line must not move - holds for something periodic and does not hold for something
           still changing shape. The Pierce oscillator is the case that matters: it clips at its
           rails, so its envelope is pinned at +/-15 V while the waveform underneath is still
           filling out, and two frames can agree on every number this suite can see while
           genuinely showing different things. Judged anyway it reported 1.13 px that no amount of
           settling would remove, because nothing was wrong. */
        int ch_stepped[MAX_PROBES];
        int any_stepped = 0;
        for (int ch = 0; ch < MAX_PROBES; ch++) ch_stepped[ch] = 0;
        for (int ch = 0; ch < ui->scope_num_channels && ch < MAX_PROBES; ch++) {
            if (!ui->scope_channels[ch].enabled) continue;
            SignalCharacter sc;
            simulation_characterise(sim, ui->scope_channels[ch].probe_idx, &sc);
            if (sc.cls == SIGNAL_STEPPED) { ch_stepped[ch] = 1; any_stepped = 1; }
        }
        if (any_stepped) stepped++;

        /* Two things are measured per channel, and the difference between them is the whole
           point. `drawn` is where the waveform's own top and bottom actually land on the glass,
           which is what a person sees move. `sig` is the same top and bottom in volts, which is
           what the circuit is really doing. A trace that moves because the signal moved is a
           correct trace - a compressor starting, a curve tracer stepping its gate, a converter
           ramping up. A trace that moves while the volts stand still is the display's fault, and
           that is the only thing this suite is entitled to fail. */
        /* One row per frame per channel: the envelope the capture held, the zero line the channel
           chose from it, and the scale it was drawn at. The comparison happens afterwards. */
        static double f_lo[MAX_PROBES][FRAMES], f_hi[MAX_PROBES][FRAMES];
        static double f_dc[MAX_PROBES][FRAMES], f_sc[MAX_PROBES][FRAMES];
        static int f_x[MAX_PROBES][FRAMES], f_ns[MAX_PROBES][FRAMES];
        int f_n[MAX_PROBES];
        for (int ch = 0; ch < MAX_PROBES; ch++) f_n[ch] = 0;

        for (int f = 0; f < FRAMES && alive; f++) {
            double until = sim->time + FRAME_DT;
            int fg = 0;
            while (sim->time < until && fg++ < 20000) if (!simulation_step(sim)) { alive = 0; break; }
            if (!alive) break;
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);
            ui_render_oscilloscope(ui, ren, sim, NULL);
            for (int ch = 0; ch < ui->scope_num_channels && ch < MAX_PROBES; ch++) {
                if (!ui->scope_channels[ch].enabled) continue;
                int n = ui->scope_capture_count;
                if (n < 2 || f_n[ch] >= FRAMES) continue;
                double vlo = ui->scope_capture_values[ch][0], vhi = vlo;
                for (int i = 1; i < n; i++) {
                    double v = ui->scope_capture_values[ch][i];
                    if (v < vlo) vlo = v;
                    if (v > vhi) vhi = v;
                }
                /* How many times the waveform crosses its own mid-level going up: with the top
                    and the bottom, that is enough to say two frames are showing the same thing.
                    The extremes alone are not - a curve tracer stepping its gate revisits a level
                    with a different waveform under it, and gets compared against itself. */
                double mid = 0.5 * (vlo + vhi);
                int xings = 0;
                for (int i = 1; i < n; i++)
                    if (ui->scope_capture_values[ch][i - 1] < mid &&
                        ui->scope_capture_values[ch][i] >= mid) xings++;
                int k = f_n[ch]++;
                f_ns[ch][k] = n;
                f_x[ch][k] = xings;
                f_lo[ch][k] = vlo;
                f_hi[ch][k] = vhi;
                /* the zero line the channel chose, in volts: the centring subtracts it, so the
                   shift it recorded is that level negated */
                f_dc[ch][k] = -ui->scope_ch_shift[ch];
                f_sc[ch][k] = ui->scope_ch_scale[ch];
            }
        }
        if (!alive) { simulation_free(sim); circuit_free(c); skipped++; continue; }

        /* The invariant, and it needs no tolerance on what the circuit is doing: two frames whose
           captured window has the same top and the same bottom are showing the same waveform, so
           they must put the zero line in the same place. A signal that is genuinely moving never
           trips this - its envelope is different every frame, so no two frames are compared. What
           it catches is a centring that depends on something other than the signal, which is what
           the mean of a ragged window is: same waveform, different answer, trace bounces. */
        double worst = 0, worst_sig = 0; int worst_ch = -1;
        for (int ch = 0; ch < ui->scope_num_channels && ch < MAX_PROBES; ch++) {
            if (ch_stepped[ch]) continue;      /* still changing shape: nothing to compare against */
            for (int i = 0; i < f_n[ch]; i++) {
                for (int j = i + 1; j < f_n[ch]; j++) {
                    double sl = fabs(f_lo[ch][i]) + fabs(f_lo[ch][j]);
                    double sh = fabs(f_hi[ch][i]) + fabs(f_hi[ch][j]);
                    if (f_x[ch][i] != f_x[ch][j]) continue;
                    if (fabs(f_lo[ch][i] - f_lo[ch][j]) > 1e-9 * (sl + 1e-12)) continue;
                    if (fabs(f_hi[ch][i] - f_hi[ch][j]) > 1e-9 * (sh + 1e-12)) continue;
                    double sc = f_sc[ch][i] > f_sc[ch][j] ? f_sc[ch][i] : f_sc[ch][j];
                    double moved = fabs(f_dc[ch][i] - f_dc[ch][j]) * sc;
                    /* What the samples themselves can resolve, subtracted before judging.
                       Averaging a waveform over whole cycles reconstructs it between samples with
                       straight lines, and across a discontinuity that is wrong by up to half a
                       sample of the edge - so the level a square wave reports carries an
                       irreducible (hi - lo) / N, where N is the number of samples averaged. On the
                       Pierce oscillator, whose output is a hard +/-15 V square at about 50 samples
                       a period, that is 15/249 V - which at 9.375 px per volt is the 1.13 px this
                       suite spent a long time trying to remove. It is not a display fault and no
                       amount of settling touches it. A smooth waveform has no discontinuity and
                       this term is negligible, so nothing else is loosened by it. */
                    int ns = f_ns[ch][i] < f_ns[ch][j] ? f_ns[ch][i] : f_ns[ch][j];
                    double edge = (f_hi[ch][i] - f_lo[ch][i]) * sc;
                    double floor_px = (ns > 0) ? edge / (double)ns : 0.0;
                    moved -= floor_px;
                    if (moved > worst) { worst = moved; worst_ch = ch; }
                }
            }
            /* how much the envelope moved over the run, for the reader: a large number here says
               the template is a transient and most of its frames were never comparable */
            double elo = f_n[ch] ? f_lo[ch][0] : 0, ehi = elo;
            for (int i = 1; i < f_n[ch]; i++) {
                if (f_lo[ch][i] < elo) elo = f_lo[ch][i];
                if (f_lo[ch][i] > ehi) ehi = f_lo[ch][i];
            }
            double drift = (ehi - elo) * (f_n[ch] ? f_sc[ch][0] : 0);
            if (ch == worst_ch || worst_ch < 0) worst_sig = drift;
        }
        judged++;
        int bad = (worst > LIMIT_PX);
        if (bad) fails++;
        if (worst_sig > LIMIT_PX) moving++;
        printf("[%s] bounce %-28s zero line moves %6.2f px between frames showing the same waveform",
               bad ? "FAIL" : any_stepped ? "NOTE" : " OK ", ti->name, worst);
        if (any_stepped) printf("  [a channel still changing shape was not judged]");
        if (bad && worst_ch >= 0) {
            /* A number on its own is not a report. Say what the offending channel is, so the
               reader can tell a display fault from a waveform this suite should not be judging. */
            SignalCharacter wc;
            simulation_characterise(sim, ui->scope_channels[worst_ch].probe_idx, &wc);
            printf("  [%s: period %.4g, %d cycles, settled %.4g, %d samples]",
                   wc.cls == SIGNAL_STATIC ? "static" : wc.cls == SIGNAL_PERIODIC ? "periodic" :
                   wc.cls == SIGNAL_ONESHOT ? "one-shot" : "stepped",
                   wc.period, wc.cycles, wc.settle_time, wc.samples);
        }
        if (worst_ch >= 0) printf("  (%s, envelope drift %.0f px)", ui_channel_name(ui, worst_ch), worst_sig);
        printf("\n");

        simulation_free(sim);
        circuit_free(c);
    }

    free(ui);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    printf("\nbounce-test: %d templates judged, %d skipped, %d with a channel not judged "
           "because its waveform is still changing shape, %d whose signal is genuinely still "
           "moving, %d whose trace moves further than the signal explains over %d frames\n",
           judged, skipped, stepped, moving, fails, FRAMES);
    return fails ? 1 : 0;
}

/* --prop-gap: which parts offer the properties panel nothing, and whether that is right.
 *
 * --prop-test checks that every row the panel offers can actually be applied. It says nothing
 * about the parts that offer no rows at all, and most do not: 35 parts out of about 124. Some of
 * those are correct - a wire, a ground and a label have nothing to edit - and some are gaps. That
 * distinction had never been counted, so this lists them rather than guessing at a number.
 *
 * A part is judged "expected empty" from what it is, not from a list of names: no properties
 * struct worth showing means nothing to show. Everything else prints, and the printing is the
 * point - the list is the work item.
 */
static int prop_gap(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { printf("prop-gap: no video - skipped\n"); return 0; }
    SDL_Window *win = SDL_CreateWindow("prop-gap", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       1600, 1000, SDL_WINDOW_HIDDEN);
    SDL_Renderer *ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE) : NULL;
    if (!ren) { printf("prop-gap: no renderer - skipped\n"); if (win) SDL_DestroyWindow(win); SDL_Quit(); return 0; }

    UIState *ui = calloc(1, sizeof *ui);
    InputState *in = calloc(1, sizeof *in);
    ui_init(ui);
    ui->window_width = 1600; ui->window_height = 1000;
    ui_update_layout(ui);

    int total = 0, with_rows = 0, empty = 0, expected_empty = 0;
    printf("parts with no editable properties:\n");
    for (int ct = COMP_NONE + 1; ct < COMP_TYPE_COUNT; ct++) {
        const ComponentTypeInfo *info = component_get_info((ComponentType)ct);
        if (!info || !info->name || !info->name[0]) continue;
        Component *comp = component_create((ComponentType)ct, 100, 100);
        if (!comp) continue;
        total++;
        ui->num_properties = 0;
        ui_render_properties(ui, ren, comp, in);
        /* clamped: num_properties counts rows the panel wanted, which can exceed
           what the array holds - see ui_prop_slot() */
        int rows = ui->num_properties < UI_MAX_PROPERTY_ROWS ? ui->num_properties : UI_MAX_PROPERTY_ROWS;
        if (rows > 0) { with_rows++; component_free(comp); continue; }
        empty++;
        /* Parts that genuinely have nothing to offer: structural and decorative ones. */
        int structural = (ct == COMP_GROUND || ct == COMP_TEXT || ct == COMP_TEST_POINT);
        if (structural) { expected_empty++; component_free(comp); continue; }
        printf("   %-28s (type %d, %d terminals)\n", info->name, ct, info->num_terminals);
        component_free(comp);
    }
    free(in); free(ui);
    SDL_DestroyRenderer(ren); SDL_DestroyWindow(win); SDL_Quit();
    printf("\nprop-gap: %d creatable types - %d offer rows, %d offer none "
           "(%d of those correctly, being structural)\n",
           total, with_rows, empty, expected_empty);
    return 0;
}

static int prop_test(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("prop-test: no video (%s) - skipped\n", SDL_GetError());
        return 0;
    }
    SDL_Window *win = SDL_CreateWindow("prop-test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       1600, 1000, SDL_WINDOW_HIDDEN);
    SDL_Renderer *ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE) : NULL;
    if (!ren) {
        printf("prop-test: no renderer (%s) - skipped\n", SDL_GetError());
        if (win) SDL_DestroyWindow(win);
        SDL_Quit();
        return 0;
    }

    UIState *ui = calloc(1, sizeof *ui);
    InputState *in = calloc(1, sizeof *in);
    ui_init(ui);
    ui->window_width = 1600; ui->window_height = 1000;
    ui_update_layout(ui);

    int fails = 0, rows_total = 0, parts_with_rows = 0, toggles = 0;
    for (int ct = COMP_NONE + 1; ct < COMP_TYPE_COUNT; ct++) {
        const ComponentTypeInfo *info = component_get_info((ComponentType)ct);
        if (!info || !info->name || !info->name[0]) continue;
        Component *comp = component_create((ComponentType)ct, 200, 200);
        if (!comp) continue;

        /* ask the panel what it shows for this part */
        memset(in, 0, sizeof *in);
        in->selected_component = comp;
        ui->num_properties = 0;
        ui_render_properties(ui, ren, comp, in);
        /* clamped: num_properties counts rows the panel wanted, which can exceed
           what the array holds - see ui_prop_slot() */
        int rows = ui->num_properties < UI_MAX_PROPERTY_ROWS ? ui->num_properties : UI_MAX_PROPERTY_ROWS;
        if (rows > 0) parts_with_rows++;

        for (int r = 0; r < rows; r++) {
            int pr = ui->properties[r].prop_type;
            /* the probe name is not a component property: the app applies it to the probe */
            if (pr == PROP_PROBE_NAME) continue;
            /* and a toggle is clicked, not typed into */
            if (property_is_toggle(pr)) { toggles++; continue; }
            rows_total++;

            /* Values across the decades a property could plausibly want. The apply switch
               validates ranges - a MOSFET's width has to be under a metre, a BJT's saturation
               current under a microamp - so one number cannot tell "this row is dead" from "that
               number was silly for this row". The row is fine if any of them lands. */
            static const char *candidates[] = { "3.3", "0.33", "33", "3.3m", "3.3u", "3.3n",
                                                "3.3p", "3.3k", "3.3M", "0.5" };
            Component *fresh = NULL;
            ComponentProps before;
            bool applied = false, changed = false;
            memset(&before, 0, sizeof before);
            for (unsigned ci = 0; ci < sizeof candidates / sizeof candidates[0] && !applied; ci++) {
                if (fresh) component_free(fresh);
                fresh = component_create((ComponentType)ct, 200, 200);
                if (!fresh) break;
                before = fresh->props;
                memset(in, 0, sizeof *in);
                in->selected_component = fresh;
                in->editing_property = true;
                in->editing_prop_type = (PropertyType)pr;
                snprintf(in->input_buffer, sizeof in->input_buffer, "%s", candidates[ci]);
                if (input_apply_property_edit(in, fresh)) {
                    applied = true;
                    changed = memcmp(&before, &fresh->props, sizeof before) != 0;
                }
            }
            if (!fresh) continue;

            if (!applied) {
                printf("[FAIL] prop  %-22s offers property %-3d and cannot apply it: the field "
                       "takes the typing and answers Invalid value\n", info->name, pr);
                fails++;
            } else if (!changed) {
                printf("[FAIL] prop  %-22s property %-3d applies and changes nothing\n",
                       info->name, pr);
                fails++;
            }
            component_free(fresh);
        }
        component_free(comp);
    }

    /* And the panel must not ask for more rows than it has room for.
       Everything above renders a part exactly as component_create() leaves it, which for a source
       means both of its sweeps switched off and seven rows - so nothing here ever approached the
       end of the array. Switch both sweeps on, as two clicks on the panel do, and an AC voltage
       source asks for seventeen rows; put either sweep in Step mode and it asks for twenty. The
       array held sixteen, and the two fields that follow it are num_properties itself and
       editing_component. Rows past the sixteenth were also drawn and dead, because the click
       handler was clamped where the array was not. */
    {
        static const ComponentType swept[] = { COMP_AC_VOLTAGE, COMP_SQUARE_WAVE, COMP_TRIANGLE_WAVE,
                                               COMP_SAWTOOTH_WAVE, COMP_NOISE_SOURCE };
        for (unsigned k = 0; k < sizeof swept / sizeof swept[0]; k++) {
            Component *comp = component_create(swept[k], 200, 200);
            if (!comp) continue;
            const ComponentTypeInfo *info = component_get_info(swept[k]);
            /* both sweeps on, and the widest mode each offers */
            SweepConfig *a = NULL, *f = NULL;
            switch (swept[k]) {
                case COMP_AC_VOLTAGE:     a = &comp->props.ac_voltage.amplitude_sweep;
                                          f = &comp->props.ac_voltage.frequency_sweep; break;
                case COMP_SQUARE_WAVE:    a = &comp->props.square_wave.amplitude_sweep;
                                          f = &comp->props.square_wave.frequency_sweep; break;
                case COMP_TRIANGLE_WAVE:  a = &comp->props.triangle_wave.amplitude_sweep;
                                          f = &comp->props.triangle_wave.frequency_sweep; break;
                case COMP_SAWTOOTH_WAVE:  a = &comp->props.sawtooth_wave.amplitude_sweep;
                                          f = &comp->props.sawtooth_wave.frequency_sweep; break;
                case COMP_NOISE_SOURCE:   a = &comp->props.noise_source.amplitude_sweep; break;
                default: break;
            }
            if (a) { a->enabled = true; a->mode = SWEEP_STEP; }
            if (f) { f->enabled = true; f->mode = SWEEP_STEP; }

            memset(in, 0, sizeof *in);
            in->selected_component = comp;
            ui->num_properties = 0;
            ui_render_properties(ui, ren, comp, in);
            int want = ui->num_properties;
            if (want > UI_MAX_PROPERTY_ROWS) {
                printf("[FAIL] prop  %-22s asks for %d rows with both sweeps on; the panel holds %d\n",
                       info ? info->name : "?", want, UI_MAX_PROPERTY_ROWS);
                fails++;
            } else {
                printf("[ OK ] prop  %-22s asks for %d rows with both sweeps on (room for %d)\n",
                       info ? info->name : "?", want, UI_MAX_PROPERTY_ROWS);
            }
            component_free(comp);
        }
    }

    printf("\nprop-test: %d typed rows and %d toggles over %d parts, %d that the panel "
           "offers and the app cannot apply\n", rows_total, toggles, parts_with_rows, fails);

    free(in);
    free(ui);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return fails;
}

/* --trig-test: a repeating waveform has to stand still on the screen. The scope finds the most
   recent edge through the trigger level and draws the window around it; if it finds none, the
   display free-runs and the trace crawls sideways. A circuit whose response is a one-off - a step
   into an RC, a fault, a start-up - has nothing to trigger on and is not judged here. The test
   for "there is something to trigger on" is the channel's own data: two rising edges through the
   middle of its range is a period, and then the scope must find an edge. A trace also has to be
   drawn from enough samples to be worth triggering: at five samples a division the trigger point
   can only land in five places, which reads as jitter however correct it is. */
static int trig_test(void) {
    int fails = 0, judged = 0, skipped = 0;
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
        ui_scope_apply_template_preset(ui, (CircuitTemplateType)t);
        ui->scope_num_channels = c->num_probes < MAX_PROBES ? c->num_probes : MAX_PROBES;
        for (int ch = 0; ch < MAX_PROBES; ch++) {
            ui->scope_channels[ch].enabled = ch < ui->scope_num_channels;
            ui->scope_channels[ch].probe_idx = ch;
        }
        /* A swept source has no one frequency, and the app widens the time base to follow it
           (scope_track_sweep). Judged at a fixed time base it is judged at whatever point of the
           sweep the run happens to stop, which says nothing: at 100 Hz the whole screen is a
           fifth of a cycle. The other audits pin the static frequency for the same reason. */
        for (int i = 0; i < c->num_components; i++)
            if (c->components[i]->type == COMP_AC_VOLTAGE)
                c->components[i]->props.ac_voltage.frequency_sweep.enabled = false;

        if (!simulation_dc_analysis(sim)) { simulation_free(sim); circuit_free(c); continue; }
        simulation_auto_time_step(sim);
        { double dtp = simulation_scope_time_step(sim, ui->scope_time_div);
          if (dtp > 0 && dtp < sim->time_step) simulation_set_time_step(sim, dtp); }
        simulation_set_history_span(sim, 20.0 * ui->scope_time_div);
        simulation_start(sim);
        /* far enough in that a start-up transient is over and the steady state is what is on
           the screen - which is also what the user is looking at when they complain */
        double run_to = 30.0 * ui->scope_time_div;
        int guard = 0;
        while (sim->time < run_to && guard++ < 400000) if (!simulation_step(sim)) break;

        /* the trigger the app picks from the data, then the search the screen runs */
        ui_scope_autotrigger(ui, sim);
        int tc = ui->trigger_channel;
        static double times[MAX_HISTORY], values[MAX_HISTORY];
        int n = (tc >= 0 && tc < ui->scope_num_channels)
                    ? simulation_get_history(sim, ui->scope_channels[tc].probe_idx, times, values, MAX_HISTORY)
                    : 0;
        double time_window = 10.0 * ui->scope_time_div;

        /* Rises and falls counted apart, because "repeating" means the same edge twice over: a
           single pulse crosses the level once each way, and a fault or a step is not something a
           scope can hold still. Two rising edges is a period. */
        int rises = 0, falls = 0;
        if (n > 10) {
            double lvl = ui->trigger_level;
            for (int i = 1; i < n; i++) {
                if (values[i - 1] < lvl && values[i] >= lvl) rises++;
                else if (values[i - 1] > lvl && values[i] <= lvl) falls++;
            }
        }
        int crossings = rises + falls;
        bool repeating = (rises >= 2 || falls >= 2);
        int idx = (n > 10) ? ui_scope_find_trigger(ui, times, values, n, time_window, ui->trigger_level) : -1;

        double per_div = (n > 1 && times[n - 1] > times[0])
                             ? (double)n / ((times[n - 1] - times[0]) / ui->scope_time_div) : 0.0;

        if (!repeating) {
            /* nothing repeating on the trigger channel: a transient, a DC operating point, a
               one-shot. Free-running is the right behaviour and there is nothing to check. */
            skipped++;
            printf("[skip] trig %-28s %s crosses its level %d time(s), %d of them rising - not a "
                   "repeating waveform\n", ti->name, ui_channel_name(ui, tc < 0 ? 0 : tc),
                   crossings, rises);
        } else {
            judged++;
            /* A trace drawn from a handful of samples a division is a zigzag whose trigger point
               can only land on one of that handful, which reads as jitter however correctly it
               triggers. The transmission-line templates had five samples a division because the
               time step could not go below a nanosecond. */
            if (idx >= 0 && per_div < 10.0) {
                printf("[FAIL] trig %-28s %s triggers, but its window is drawn from %.1f samples "
                       "a division (dt=%.4g): the trace and its trigger point both jitter\n",
                       ti->name, ui_channel_name(ui, tc), per_div, sim->time_step);
                fails++;
            } else if (idx < 0) {
                printf("[FAIL] trig %-28s %s crosses its level %d times in the window but the "
                       "scope finds no edge to trigger on: the trace free-runs\n",
                       ti->name, ui_channel_name(ui, tc), crossings);
                fails++;
            } else {
                printf("[ OK ] trig %-28s %s @ %-10.4g %d crossings, edge at %d of %d, %.1f "
                       "samples/div, dt=%.4g\n", ti->name, ui_channel_name(ui, tc),
                       ui->trigger_level, crossings, idx, n,
                       n > 1 ? (double)n / ((times[n - 1] - times[0]) / ui->scope_time_div) : 0.0,
                       sim->time_step);
            }
        }
        simulation_free(sim);
        circuit_free(c);
    }
    free(ui);
    printf("\ntrig-test: %d repeating waveforms judged, %d free-running, %d one-shots skipped\n",
           judged, fails, skipped);
    return fails;
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

    /* Node ids must still be inside the arrays that are indexed by them.
       node_map and the union-find parent[] in circuit_build_node_map both hold MAX_NODES and are
       indexed BY node id, while circuit_create_node only ever bounded the node COUNT. Ids climbed
       across the 188 clear-and-place cycles above and passed 2048 around the sixtieth template,
       and building a node map then wrote off the end of the Circuit struct - a real segfault,
       found by this suite. */
    checks++;
    if (shared->next_node_id >= MAX_NODES) {
        printf("[FAIL] place node ids reached %d after %d clear-and-place cycles; node_map holds %d\n",
               shared->next_node_id, checks, MAX_NODES);
        fails++;
    } else {
        printf("[ OK ] place node ids top out at %d over %d cycles, inside node_map's %d\n",
               shared->next_node_id, checks, MAX_NODES);
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
    for (int ch = 0; ch <= MAX_PROBES; ch++) {   /* 0 is the ALL chip, then one per channel */
        checks++;
        int idx = -1;
        if (ui_action_kind(UI_ACTION_SCOPE_CH_SEL + ch, &idx) != UIA_SCOPE_CH || idx != ch) {
            printf("[FAIL] the scope's channel chip %d reads as something else\n", ch); fails++;
        }
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
/* --symbol-test: does every part that can be placed have a symbol to draw?

   COMP_BATTERY had none. It was in the netlist, it solved, it carried current, every audit
   passed it - and on the canvas it was two terminal dots with the wire running between them,
   which reads as a wire. Three others were the same: the high-power load (whose symbol had
   even been written, and never called), and the PWL and expression sources.

   Nothing that looked at voltages could have found this, so this looks at the drawing: one of
   every type is rendered into an off-screen surface, and render_component's default case -
   the one that means "no symbol for this" - is counted. */
static int symbol_test(void) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("[FAIL] symbol  SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, 400, 400, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer *ren = surf ? SDL_CreateSoftwareRenderer(surf) : NULL;
    if (!ren) {
        printf("[FAIL] symbol  no software renderer: %s\n", SDL_GetError());
        if (surf) SDL_FreeSurface(surf);
        SDL_Quit();
        return 1;
    }
    RenderContext ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.renderer = ren;
    ctx.zoom = 1.0f;
    ctx.offset_x = 200; ctx.offset_y = 200;
    ctx.canvas_rect = (Rect){ 0, 0, 400, 400 };

    int missing = 0, checked = 0, skipped = 0;
    for (int t = COMP_NONE + 1; t < COMP_TYPE_COUNT; t++) {
        const ComponentTypeInfo *info = component_get_info((ComponentType)t);
        if (!info || !info->name || !info->name[0]) { skipped++; continue; }
        /* A block draws itself from a definition it has not got here, and the two text parts
           are text by nature - none of the three has a fixed symbol to check. */
        if (t == COMP_SUBCIRCUIT || t == COMP_TEXT || t == COMP_LABEL) { skipped++; continue; }
        Component *c = component_create((ComponentType)t, 0, 0);
        if (!c) { skipped++; continue; }
        int before = g_render_missing_symbol;
        render_component(&ctx, c);
        if (g_render_missing_symbol > before) {
            printf("[FAIL] symbol  %-24s draws no symbol of its own\n", info->name);
            missing++;
        }
        checked++;
        component_free(c);
    }
    SDL_DestroyRenderer(ren);
    SDL_FreeSurface(surf);
    SDL_Quit();
    printf("symbol-test: %d part types drawn, %d with no symbol, %d not applicable\n",
           checked, missing, skipped);
    return missing ? 1 : 0;
}

static int layout_test(void) {
    int fails = 0;
    UIState *ui = calloc(1, sizeof *ui);
    ui_init(ui);
    /* The UI sizes that can actually occur. A display taller than 900 is scaled rather than
       given more UI pixels, so everything from 1080p up lands on about 1600x900 and the small
       end is what needs the checking - 1024x600 is a netbook panel and 1366x768 is still the
       commonest laptop screen there is. */
    static const int sizes[][2] = { {1024, 600}, {1280, 720}, {1366, 768}, {1600, 900} };
    {
        /* and the mapping that produces them: a device height, the UI height it should give */
        static const struct { int dev_h; int want_lo, want_hi; } scal[] = {
            { 600,  600,  600 },   /* below the baseline: no scaling, every pixel is UI */
            { 768,  768,  768 },
            { 900,  900,  900 },   /* exactly the baseline */
            { 1080, 895,  905 },   /* 1.2x */
            { 1440, 895,  905 },   /* 1.6x */
            { 2160, 895,  905 },   /* 2.4x */
        };
        for (unsigned k = 0; k < sizeof scal / sizeof scal[0]; k++) {
            float s = render_ui_scale(scal[k].dev_h);
            int lh = (int)(scal[k].dev_h / s);
            if (lh < scal[k].want_lo || lh > scal[k].want_hi) {
                printf("[FAIL] layout %d px display -> %d UI px (scale %.2f), expected %d-%d\n",
                       scal[k].dev_h, lh, s, scal[k].want_lo, scal[k].want_hi);
                fails++;
            }
        }
    }
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

        /* ---- the toolbar's right-hand end ----------------------------------------------------
           The speed slider, the dt readout and the three time-step buttons were at absolute
           offsets computed once from a 1280-wide window, and they did not fit in one: the dt value
           was cut off mid-character and [-] [+] [Auto] sat entirely past the right edge, so at the
           DEFAULT window size three controls could not be seen or clicked, and no resize moved
           them. What is asserted is reachability - every control fully inside the window - at every
           size. Not overlapping the button strip is asserted only from 1280 up, because below that
           there is genuinely no room for both and staying on screen is the more important half. */
        {
            struct { const char *name; Rect b; } tb[] = {
                { "speed slider", (Rect){ui->speed_slider.x, ui->speed_slider.y,
                                         ui->speed_label_w + ui->speed_slider.w, ui->speed_slider.h} },
                { "dt readout",   (Rect){ui->timestep_display_x, 12, 24 + 52, 20} },
                { "dt -",         ui->btn_timestep_down.bounds },
                { "dt +",         ui->btn_timestep_up.bounds },
                { "dt Auto",      ui->btn_timestep_auto.bounds },
            };
            int n = (int)(sizeof tb / sizeof tb[0]);
            for (int i = 0; i < n; i++) {
                if (tb[i].b.x < 0 || tb[i].b.x + tb[i].b.w > ui->window_width) {
                    printf("[FAIL] layout %dx%d: toolbar '%s' at x %d..%d is outside a %d px window\n",
                           sizes[k][0], sizes[k][1], tb[i].name, tb[i].b.x, tb[i].b.x + tb[i].b.w,
                           ui->window_width);
                    fails++;
                }
                for (int j = i + 1; j < n; j++)
                    if (rects_overlap(&tb[i].b, &tb[j].b)) {
                        printf("[FAIL] layout %dx%d: toolbar '%s' overlaps '%s'\n",
                               sizes[k][0], sizes[k][1], tb[i].name, tb[j].name); fails++;
                    }
            }
            if (ui->window_width >= 1280) {
                Rect *spice = &ui->btn_import_spice.bounds;
                for (int i = 0; i < n; i++)
                    if (rects_overlap(spice, &tb[i].b)) {
                        printf("[FAIL] layout %dx%d: toolbar '%s' overlaps the SPICE button\n",
                               sizes[k][0], sizes[k][1], tb[i].name); fails++;
                    }
            }
            printf("[ OK ] layout %dx%d: %d toolbar controls right-aligned inside the window (label %s)\n",
                   sizes[k][0], sizes[k][1], n, ui->speed_label_w ? "shown" : "dropped");
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
           "  --netlist FILE       build a circuit from a written-down one (one part per line)\n"
           "  --inspect NAME       open a My Circuits block and show what is inside it\n"
           "  --line-weight W      canvas stroke weight in pixels (default 1.7)\n"
           "  --ui-scale S         UI pixels to device pixels (default: from the display height)\n"
           "  --ss N               supersample the frame N times (1 = off, default 2, max 4)\n"
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

/* Resolve --template to a type from the static table alone, so a bad name can be rejected before
   anything is initialised. It used to be resolved after app_init, which meant a typo opened a
   window, sized it, ran the update check and drew four frames before printing "Unknown template"
   and exiting 2 - a visible flash for a person, and an SDL window on a headless runner for a
   script that was only ever going to be told no. Returns 0 and sets *out on success, 2 on failure
   having already explained itself. */
static int resolve_cli_template(const char *cli_template, CircuitTemplateType *out) {
    CircuitTemplateType found = CIRCUIT_NONE;
    for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
        const CircuitTemplateInfo *info = circuit_template_get_info((CircuitTemplateType)t);
        if (info && (!_stricmp(info->name, cli_template) || !_stricmp(info->short_name, cli_template))) { found = (CircuitTemplateType)t; break; }
    }
    /* No exact match: a unique case-insensitive substring of the full name is accepted, so
       "--template Pierce" and "--template \"Digital Clock\"" both work. Ambiguity is an
       error, not a guess - a script that asked for "Buck" should be told there are three. */
    int ambiguous = 0;
    if (found == CIRCUIT_NONE) {
        char want[128];
        snprintf(want, sizeof want, "%s", cli_template);
        for (char *p = want; *p; p++) *p = (char)tolower((unsigned char)*p);
        for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
            const CircuitTemplateInfo *info = circuit_template_get_info((CircuitTemplateType)t);
            if (!info) continue;
            char have[128];
            snprintf(have, sizeof have, "%s", info->name);
            for (char *p = have; *p; p++) *p = (char)tolower((unsigned char)*p);
            if (strstr(have, want)) {
                if (found != CIRCUIT_NONE) { ambiguous = 1; fprintf(stderr, "  matches: %s\n", info->name); }
                else found = (CircuitTemplateType)t;
            }
        }
        if (ambiguous) {
            const CircuitTemplateInfo *fi = circuit_template_get_info(found);
            fprintf(stderr, "  matches: %s\n", fi ? fi->name : "?");
            fprintf(stderr, "--template '%s' is ambiguous.\n", cli_template);
            return 2;
        }
    }
    if (found == CIRCUIT_NONE) {
        /* This used to print the list and then carry on with an empty canvas, which is the
           worst thing a scripted flag can do: the run exits 0, the screenshot exists, and it
           is a picture of nothing. An audit driving the app through --template would judge an
           empty canvas and pass. Unknown means stop. */
        fprintf(stderr, "Unknown template '%s'. Available:\n", cli_template);
        for (int t = CIRCUIT_NONE + 1; t < CIRCUIT_TYPE_COUNT; t++) {
            const CircuitTemplateInfo *info = circuit_template_get_info((CircuitTemplateType)t);
            if (info) fprintf(stderr, "  %-8s %s\n", info->short_name, info->name);
        }
        return 2;
    }
    *out = found;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc > 1) attach_parent_console();
    const char *cli_inspect = NULL, *cli_netlist = NULL;
    const char *cli_template = NULL, *cli_shot = NULL, *cli_record = NULL, *cli_size = NULL;
    const char *cli_state = NULL;
    int cli_frame = 90, cli_rec_n = 0, cli_rec_every = 1, cli_scroll = -1, cli_tab = -1; bool cli_exit = false, no_update = false, no_auto_update = false;
    const char *cli_keys = NULL; int cli_keys_frame = 30, cli_keys_every = 6;
    struct { int x, y, x2, y2, frame; bool drag; } cli_mouse[12]; int cli_mouse_n = 0;
    const char *cli_xy = NULL;
    bool cli_popout = false;
    const char *cli_spice = NULL;
    /* --shard is read before anything else: a suite flag returns straight out of the loop below,
       so a shard written after it on the command line would never be seen. */
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--shard")) {
            int si = 0, sn = 1;
            if (sscanf(argv[i + 1], "%d/%d", &si, &sn) != 2 || sn < 1 || si < 0 || si >= sn) {
                fprintf(stderr, "--shard wants i/n with 0 <= i < n, e.g. 0/4\n");
                return 2;
            }
            g_shard_i = si; g_shard_n = sn;
        }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--template") && i + 1 < argc) cli_template = argv[++i];
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) cli_shot = argv[++i];
        else if (!strcmp(argv[i], "--state-out") && i + 1 < argc) cli_state = argv[++i];
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
        else if (!strcmp(argv[i], "--inspect") && i + 1 < argc) cli_inspect = argv[++i];
        else if (!strcmp(argv[i], "--netlist") && i + 1 < argc) cli_netlist = argv[++i];
        else if (!strcmp(argv[i], "--line-weight") && i + 1 < argc) {
            g_render_line_weight = (float)atof(argv[++i]);
            if (g_render_line_weight < 0.5f) g_render_line_weight = 0.5f;
            if (g_render_line_weight > 4.0f) g_render_line_weight = 4.0f;
        }
        else if (!strcmp(argv[i], "--ui-scale") && i + 1 < argc) {
            g_ui_scale_override = (float)atof(argv[++i]);
            if (g_ui_scale_override < 0.5f) g_ui_scale_override = 0.5f;
            if (g_ui_scale_override > 4.0f) g_ui_scale_override = 4.0f;
        }
        else if (!strcmp(argv[i], "--ss") && i + 1 < argc) {
            int v = atoi(argv[++i]);
            g_render_supersample = (v < 1) ? 1 : (v > 4) ? 4 : v;
        }
        else if (!strcmp(argv[i], "--layout-test")) return layout_test();
        else if (!strcmp(argv[i], "--symbol-test")) return symbol_test();
        else if (!strcmp(argv[i], "--autoset-test")) return autoset_test();
        else if (!strcmp(argv[i], "--place-test")) return place_test();
        else if (!strcmp(argv[i], "--trig-test")) return trig_test();
        else if (!strcmp(argv[i], "--bounce-test"))
            return bounce_test((i + 1 < argc) ? atof(argv[i + 1]) : 0.0);
        else if (!strcmp(argv[i], "--prop-test")) return prop_test();
        else if (!strcmp(argv[i], "--prop-gap")) return prop_gap();
        else if (!strcmp(argv[i], "--crashlog")) { crashlog_dump_last(); return 0; }
        else { fprintf(stderr, "Unknown option: %s\n", argv[i]); usage(); return 2; }
    }

    /* Resolved here, before SDL_Init and the crash log, so --template with a typo costs a
       message and nothing else. */
    CircuitTemplateType cli_template_type = CIRCUIT_NONE;
    if (cli_template) {
        int rc = resolve_cli_template(cli_template, &cli_template_type);
        if (rc) return rc;
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
    } else if (!cli_shot && !cli_record) {
        /* Open at a useful fraction of the screen rather than a fixed 1280x720. On a 1440p
           display that fixed size is a quarter of the desktop, and a schematic drawn into it
           has to be zoomed out until the labels are a few pixels tall - which is a resolution
           problem that no amount of smoothing fixes.

           Only for an interactive run. --shot and --record keep the fixed size on purpose:
           every screenshot the audits compare, and every crop in the documentation, is taken
           at 1280x720, and a window that changed with the machine would make those unrepeatable. */
        SDL_DisplayMode dm;
        if (SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(app.window), &dm) == 0
            && dm.w > 0 && dm.h > 0) {
            int w = (int)(dm.w * 0.85), h = (int)(dm.h * 0.85);
            if (w < WINDOW_WIDTH)  w = WINDOW_WIDTH;
            if (h < WINDOW_HEIGHT) h = WINDOW_HEIGHT;
            if (w > dm.w) w = dm.w;
            if (h > dm.h) h = dm.h;
            if (w > WINDOW_WIDTH || h > WINDOW_HEIGHT) {
                SDL_SetWindowSize(app.window, w, h);
                SDL_SetWindowPosition(app.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            }
        }
    }
    /* After any resize above, and before the first frame: the UI size and the scale both come
       from the window, so nothing is laid out until the window is the size it will be. */
    app_update_window_metrics(&app);

    if (cli_netlist) {
        char nerr[200] = "";
        int n = netlist_build_file(app.circuit, cli_netlist, nerr, sizeof nerr);
        printf("%s\n", nerr[0] ? nerr : (n > 0 ? "loaded" : "nothing placed"));
        if (n > 0) { app_zoom_to_fit(&app); ui_set_status(&app.ui, nerr); }
    }

    if (cli_inspect) {
        int id = 0;
        for (int i = 0; i < g_subcircuit_library.count; i++)
            if (!_stricmp(g_subcircuit_library.defs[i].name, cli_inspect))
                id = g_subcircuit_library.defs[i].id;
        if (id) ui_open_subcircuit_view(&app.ui, id);
        else fprintf(stderr, "No block called %s in My Circuits\n", cli_inspect);
    }

    if (cli_shot) { strncpy(app.cli_shot_path, cli_shot, sizeof app.cli_shot_path - 1); }
    if (cli_state) { strncpy(app.cli_state_path, cli_state, sizeof app.cli_state_path - 1); }
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
        // a few frames first so the resize event lands and the canvas rect is laid out
        for (int k = 0; k < 4; k++) { app_handle_events(&app); app_update(&app); app_render(&app); SDL_Delay(16); }
        app_place_template_centered(&app, cli_template_type);
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
