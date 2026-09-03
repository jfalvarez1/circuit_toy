# Circuit Playground — Feature Test Plan

Goal: exercise every feature one at a time and record pass/fail. Each test states the
**setup**, the **expected result** (with a hand-calculated number where possible), and the
**watch-for** list: graphical glitches, model accuracy, model glitches (NaN / blow-ups /
non-convergence), conflicting settings, time-base changes, and runtime (while-running) changes.

Status legend: `[ ]` not run · `[P]` pass · `[F]` fail (link issue/notes) · `[S]` skipped

---

## Current state (2026-08-29, v3.16.0)

The per-test notes further down carry the counts they were written with (96/96, 86 oracles and
so on). They are kept as a record of what each test was checking at the time; these are the
numbers a clean run gives today.

| Mode | Result |
|---|---|
| `template_smoke` | 182/182 templates place, solve and step |
| `--probe-test` | 204/204 output oracles |
| `--demo-test` | 182/182 demo contracts |
| `--osc-test` | 11 oscillators at their own frequency and waveform shape |
| `--view-test` | 182 templates put something on the scope; 50 switches all clickable |
| `--flow-test` | 182/182 (two templates exempt from the node sum, and they say so) |
| `--switch-test` | 25 switches, both states |
| `--part-test` | 25 checks over 21 named devices at their data sheet conditions |
| `--op-test` | 6 device operating points |
| `--sub-test` | 5 subcircuit cases, nested eight deep |
| `--spice-test` | 14 `.SUBCKT` import cases |
| `--xtal-test` | 4 crystal impedance checks |
| `--std-test` | 23 buses against ERCOT / NERC / ANSI C84.1 / NEC |
| `--burn-test` | 182 templates, 0 parts over rating |
| `--knob-test` | 2646 runs: every adjustable value at x0.5 and x2 |
| `--param-test` | scope presets sane for all 182 |
| `--geom-test` | 0 hard violations (diagonal wires, overlapping symbols) |
| `--layout-test` | 0 failures: palette, scope panel knobs, zoom buttons, probe naming |
| `tools/gui_smoke.py` | every template driven through the real app, plus zoom / pan / switch-click |

**GUI smoke.** `python tools/gui_smoke.py` launches the app, places each template, reads the
screenshot it saves and checks the pixels: the window is not one flat colour, the canvas has a
schematic on it, and the graticule has trace-coloured pixels in it. Then it presses the toolbar
zoom buttons, selects the Pan tool and drags, and clicks the N-1 breaker - through injected SDL
mouse events (`--click`, `--drag`), so each one takes the path a real pointer takes. It needs a
display; `SDL_VIDEODRIVER=dummy` has no renderer to read back from.

Recording: for every failure, capture `screenshots/<test-id>.bmp` (F12/Screenshot action)
and note: build hash, steps, expected vs actual, stderr excerpt.

---

## 0. Pre-flight (run before everything else)

| ID | Test | Expected |
|----|------|----------|
| 0.1 | `[ ]` Clean build (`rebuild.bat`) with `-Wall` — zero new warnings | Builds |
| 0.2 | `[ ]` Launch, empty canvas, idle 60 s | No stderr spam, stable FPS, no memory growth (Task Manager) |
| 0.3 | `[P]` Debug leftovers in the working tree | Removed 2026-08-24: all `[DC ANALYSIS]`/`[SOLVER]`/`[CLAMP]`/LED-array prints, the `probe_debug.log` and `oscillator_debug.txt` writers, and the `debug_circuit.json` auto-load block |
| 0.4 | `[P]` Repo hygiene | Stray `nul`, debug logs and scratch scripts deleted; `.gitignore` covers `*.log`, `screenshots/`, `circuit.json` |
| 0.6 | `[ ]` Headless regression: `build\tools\template_smoke.exe` (add `--verbose` for bias points, `--nodes` for net mapping) | Prints `96/96 templates passed`; run after every engine change (last engine change: the terminal-current re-stamp now evaluates time-dependent sources at `sim->time − dt` — it used the already-advanced time, one step of phase error, seen as a 12 % KCL mismatch through the 3-phase source's 1 mΩ `r_series`; before that the inductor companion moved to the theta = 0.6 method — all previous oracles still pass) |
| 0.7 | `[ ]` `template_smoke --probe-test` | 86/86: every template's designated output node matches the hand calculation (DC level, amplitude, peak or mean) — includes the kV power buses, the three Line Model Ladder rows, the nine protection & control oracles (burden / R_d / VT secondary amplitudes, TRIP and BFT maxima, SIL / series-comp / 765 kV load ends) and the seven three-phase / generator oracles (373.3 V balanced load, 20.83 V neutral shift, 264 kV per phase, 169.3 V plus bus, ±15 V bistable, 7.5 V triangle, 4.9 V shaped sine) and the seven textbook oracles (3.00 V Thevenin load, 7.333 V superposition node, 5 V settled RC / RL steps, **9.53 V first RLC ring peak**, 5.0 V critical-damping row with no overshoot, 15 V op-amp clip) and the six batch-five oracles (4.5 V tuned-amp peak at f0 ±50 %, 1.88 V common base ±30 %, 0.91 Darlington ±12 %, SR latch Q max 5 V at 0.5 ms, 259.6 kVpk plant bus ±6 %, 103 kVpk substation feeder bus ±8 %). **Probe-dt rule:** the probe run never uses a dt coarser than the template's scope preset maps to (`simulation_scope_time_step(circuit_template_scope_time_div(t))`) — the ring oracle needs 1 µs from its 50 µs/div preset; set a new template's preset for its fastest feature |
| 0.8 | `[ ]` `template_smoke --osc-test` (also `--osc-dt 5e-6`) | 9/9: Wien ~1.56 kHz, phase-shift ~6.0 kHz and relaxation ~455 Hz really oscillate at dt from 100 ns to 5 us (`--osc-dt` sets these three); Triangle/Square and Function Generator 5000 Hz at their own dt 200 ns, Colpitts 712 kHz (measured 710) at 5 ns, Ring 145 kHz (measured 139) at 20 ns, **Hartley 503 kHz (measured 557 — coupling cap and device C pull it; the pass band is ±25 %) at 5 ns, Clapp 1.744 MHz (measured 1.76) at 2 ns** — the **per-case dt** in `cases[]` overrides `--osc-dt` for those six, and the output node comes from the template output spec |
| 0.9 | `[ ]` `template_smoke --geom-test` | Schematic audit: 0 diagonal wires anywhere; 74/96 clean (all 18 templates added 2026-08-24, the 7 protection & control templates, the 9 three-phase / signal-generator templates #73–#81, the 9 Hartley / Clapp / textbook templates #82–#90 and the 6 batch-five templates #91–#96 are clean); remaining crossings / wires-through-bodies are listed per template (see TEMPLATE_AUDIT open items) |
| 0.10 | `[ ]` `template_smoke --flow-test`, `--scope-test` | 96/96 and 8/8 (flow: behavioural logic gates have no terminal currents, so KCL is only asserted on passive nodes in 50BF, the Ring Oscillator and the SR Latch; the `COMP_SOURCE_3PH` terminal currents come from its three aux variables — `component_aux_count()` — and the t − dt re-stamp) |
| 0.11 | `[ ]` `template_smoke --demo-test` | 96/96: every template honours its `DemoKind` contract (`template_demo[]`: LOWPASS/HIGHPASS/BANDPASS/NOTCH bracket f_char with the sweep, ENVELOPE/LIMITER follow or clamp the amplitude sweep, WAVEFORM varies, SWITCH swings rail to rail, DC is steady, OSC self-starts). WAVEFORM/SWITCH/DC run 6/f_char and judge the second half; the harness now **forces a usable dt for pulse-only circuits** (no AC source ⇒ auto-dt unusable: if dt is 0, > run/200 or < run/100000 it uses run/1000 — 50BF is the case that needs it) |
| 0.12 | `[ ]` `template_smoke --tesla-test` | 3/3 plus the tuned-vs-detuned comparison (§3.10.2) |
| 0.13 | `[ ]` `template_smoke --param-test` | All `OK`: spark gap, toroid, transmission line, transformer ratio, **4b analog switch as a fault switch** (r_on 0.01 / 0.3 / 100 / 1e6 Ω, 0/5 V pulse control: load ≈ 0 before the pulse, 10·100/(100 + r_on) during it ±5 %), **4c transformer as a CT** (N = 120 / 400 / 2875, 100 A primary ⇒ 100/N V on a 1 Ω burden ±3 %) and scope-preset limits (§3.10.1) |
| 0.14 | `[ ]` `template_smoke --response NAME` | Explorer, not pass/fail: prints per-node amplitude in 8 log bins of the template's sweep — use it to choose an output node / DemoKind for a new template |
| 0.15 | `[ ]` `template_smoke --trace NAME T` | Explorer, not pass/fail: runs the template for T seconds at the app dt and prints, for every node, min / max voltage with the components attached, plus the final state of every switch — the tool for "why does TRIP never drop" / "which node shorted" questions on the relay templates (e.g. `--trace 50/51 0.2`, `--trace 50BF 1.2`) |
| 0.16 | `[ ]` `template_smoke --probe-audit [NAME]` | "What the user will actually see": for every template the auto-placed probes exactly as the app places them (source, output, extra), the component terminals each probe sits on (owner list), and min / max over one scope screen at the preset time/div and V/div, with flags `DUP` (two probes on one node), `GND` (probe on ground), `FLAT` (waveform demo that does not move), `SMALL` (output < 0.25 div at the preset V/div), `CLIP` (beyond ±4 div before the one-shot autoscale), `NOOUT`. Current: **10/96 flagged, all physically expected** — HP / band-pass outputs SMALL at the start of their sweep, the 14.7 kV generator channel SMALL at 100 kV/div, etc. A *new* flag after a template or preset edit is the thing to chase; oscillators no longer auto-probe their kick source, so a `DUP`/`GND` on a kick is gone for good |
| 0.17 | `[ ]` `template_smoke --series NAME T NODE` | Explorer: prints `time voltage` pairs for node id NODE over T seconds (node ids from `--nodes`) — paste into a plot, or diff two runs to see a sign / phase error (this is how the one-step phase error of the terminal-current re-stamp was localised). Use `--trace` first to pick the node |

Smoke test modes: (none) template load+run · `--demo-test` · `--probe-test` (dt never coarser than the template's scope preset) · `--osc-test [--osc-dt X]` (per-case dt in `cases[]` wins over `--osc-dt`; output node from the template spec) · `--flow-test` (behavioural gates exempt) · `--burn-test` (no part over its rating; HV templates must be clean) · `--std-test` (bus voltages vs ERCOT / NERC / ANSI C84.1 / NEC) · `--switch-test` (every switch in both states at the probed output) · `--geom-test` (also symbol overlap) ·
`--geom-test` · `--scope-test` · `--sweep-check` · `--tesla-test` · `--param-test` · `--response NAME` · `--trace NAME T` · `--probe-audit [NAME]` · `--series NAME T NODE` ·
flags `--verbose`, `--nodes`, `--svg DIR`, `--dc`, `--sim-time T`, and a bare NAME argument to filter templates.
| 0.5 | `[ ]` Window resize to min size / maximized / 4K | Panels reflow, no overlapping text, neon border tracks edges |

---

## 1. Canvas & Editing (UI-only, sim stopped)

| ID | Test | Expected / Watch-for |
|----|------|----------------------|
| 1.1 | `[ ]` Place every component category via palette click → canvas | Ghost follows cursor, snaps to grid, symbol matches `SCHEMATIC_DESIGN_GUIDELINES.md` |
| 1.2 | `[ ]` Rotate `R` ×4 before placing and after placing (double-click) | Terminals stay on grid at all 4 angles; labels don't overlap body; no mirrored text |
| 1.3 | `[ ]` Wire tool: terminal→terminal, terminal→wire midpoint, wire→wire junction | Junction dot drawn, node count in status bar updates |
| 1.4 | `[ ]` Wire crossing without junction | No junction dot; nodes stay separate |
| 1.5 | `[ ]` Delete component with attached wires (Delete, Backspace, Delete tool) | Wires removed/dangling handled; no orphan node in status count |
| 1.6 | `[ ]` Undo/Redo across place, move, wire, delete, property edit, rotate (10 steps deep each way) | State exactly reversible; probes/scope channels survive undo |
| 1.7 | `[ ]` Copy/Cut/Paste/Duplicate single + multi-select | New IDs/labels unique (R1→R2), wires inside selection copied |
| 1.8 | `[ ]` Box select, Shift-add to selection, drag-move selection | Wires stretch, snap respected |
| 1.9 | `[ ]` Pan (middle drag, Shift+drag), zoom (wheel) at extremes | Grid still drawn at max zoom-out; text scales; no z-fighting of wires |
| 1.10 | `[ ]` Grid toggle `G`, snap toggle `S`, place with snap off then on | Off-grid component still wireable |
| 1.11 | `[ ]` Text annotation: size/bold/italic/underline, multi-line, empty | No clipped glyphs; empty text deletable |
| 1.12 | `[ ]` Palette collapse/expand all sections, scroll palette | Headers clickable at every scroll offset; every item in every section (incl. the new Sub-circuit / Bus section: Pin, IC, Bus, Tap, Lamp, and Measurement: VMeter, AMeter, WMeter, TstPt) places exactly that item; circuit buttons (sRLC, pRLC, Cmp, …) place the whole template — was broken by stale hit-boxes of unlisted items |
| 1.13 | `[ ]` Spotlight `Ctrl+K` **or `Ctrl+Space`**: fuzzy query, arrow keys, Enter, mouse click, Esc | Every component in `types.h` reachable by search (incl. the 3-Phase Source); both chords open the same Spotlight |
| 1.13a | `[ ]` Palette filter: press **`/`** (sim idle or running), type `dar`, Enter; then `/`, type, Esc; `/` while Spotlight is open or a property edit is active | `/` focuses the left-panel type-to-filter: the palette narrows to matching items (Darlington, Darl template…), Enter keeps the filter and returns focus, Esc clears it; while filtering, keys go to the filter (no shortcut fires — `r` must not rotate, Space must not run); `/` is a no-op when Spotlight or a property edit already owns the keyboard |
| 1.14 | `[ ]` Property panel: engineering notation `1k 4.7u 10n 2.2M 1p`, negative, `0`, garbage `abc`, empty | Correct parse; invalid input rejected without crash; value redisplayed in same notation |
| 1.15 | `[ ]` Property panel: scroll-wheel edit, keyboard shortcuts per guide §10 | Steps sane (1-2-5), clamps at limits |
| 1.16 | `[ ]` Resize panels (scope, properties) to min/max | Scope controls scroll with wheel when squeezed; nothing draws outside panel |
| 1.16a | `[ ]` Scope in-place resize: drag top edge up to just under the toolbar; drag left edge out to the palette; then resize the window; then pop out and back | Scope grows over properties list / canvas; clicks on the enlarged scope never place components underneath; size persists across window resize; corner ticks visible |
| 1.16b | `[ ]` Peak Detector & Clamper templates (sweeping sources) at 50 ms/div | Peak detector output rides the 1-5 V envelope (rises fast, decays with 47 ms bleed); clamper bottom stays at -0.7 V while the top follows twice the amplitude |
| 1.16c | `[ ]` Load any template | Probes appear on input (CH1) and output (CH2), scope time/div presets (e.g. 200 µs/div filters, 50 ms/div peak detector, 50 µs/div phase-shift), simulation auto-starts, status bar names the template; loading a second template adds its probes (max 8) |
| 1.16f | `[ ]` Scope mouse: wheel over the screen, Shift+wheel, middle-drag horizontally and vertically | Wheel steps time/div 1-2-5 and turns Trk off; Shift+wheel steps V/div; middle-drag slides the trigger position (0..1) and shifts channel offsets; wheel elsewhere still scrolls palette/properties/controls; canvas pan unaffected |
| 1.16e | `[ ]` `Trk` (sweep tracking) on the filter templates | time/div steps down 1-2-5 as f rises (2 ms/div at 100 Hz → 20 µs/div at 20 kHz) so ~3 cycles always show; no flicker between steps; toggling Trk off freezes time/div; V/div preset 0.5 V |
| 1.16d | `[ ]` RC LP / RC HP / RL / Sallen-Key / band-pass / notch templates | Source sweeps 100 Hz-20 kHz (notch 10-300 Hz) log, back and forth every 3 s; `f=` readout under the source updates live; LP output shrinks past 1.59 kHz, HP grows, notch vanishes at 60 Hz; dt auto-picks ~250 ns for the 20 kHz end; disabling the sweep in the source properties restores the static case |
| 1.17 | `[ ]` F1 shortcuts dialog | Matches README table; Esc closes |

---

## 2. Simulation Control & Time Base

Key numbers: `DEFAULT_TIME_STEP=1e-7`, range `1e-9 … 1e-2`, `MAX_HISTORY=10000`, decimation
kicks in above that span. Adaptive stepping is **disabled by default** (`adaptive_enabled=false`).

| ID | Test | Expected / Watch-for |
|----|------|----------------------|
| 2.1 | `[ ]` Run/Pause/Stop/Reset via toolbar and Space | Status bar time advances, pauses, resets to 0; reset re-runs DC op point |
| 2.2 | `[ ]` Step mode: single-step ×10 | Time advances exactly `dt` per step |
| 2.3 | `[ ]` Speed `+`/`-` 0.1× → 100× while running | No time jump backwards; particles speed up; UI stays responsive at 100× |
| 2.4 | `[ ]` Time step `dt` buttons through full 1n…10m table **while running** (RC circuit, 1 kHz) | Waveform continuous across change; no spike at the change instant; no NaN |
| 2.5 | `[ ]` Auto time-step button on: 1 Hz, 1 kHz, 1 MHz AC sources | Picks 50–300 samples/period; sanity-check reported dt |
| 2.6 | `[ ]` dt too large for the signal (10 ms dt, 1 kHz sine) | Aliasing visible but no crash; consider warning |
| 2.7 | `[ ]` dt tiny (1 ns) on 1 Hz signal for 30 s wall time | History decimation engages; scope still shows a full cycle at 1 s/div; RAM flat |
| 2.8 | `[ ]` Scope Time/div sweep 10 ns → 100 s while running and while paused | Range extended 2026-08-24 (was 1 µs floor). History decimation now follows time/div (span = 20×time/div), so the trace re-fills briefly after each change; labels show ns/us/ms/s. Below ~10 samples per division the trace is stepped — lower dt (Auto) for ns/div work |
| 2.9 | `[ ]` **Conflict:** time/div < dt (e.g. dt=10 µs, 1 µs/div) | Defined behaviour (flat/stepped trace), not garbage |
| 2.9a | `[ ]` **dt ↔ time/div sync** (automated: `template_smoke --scope-test`, must print all `OK`) | Changing time/div re-maps dt to ~50 samples/div, never coarser than the accuracy step (1 kHz source ⇒ 10 µs), snapped to 1‑2‑5: 1 ms/div→10 µs, 100 µs/div→2 µs, 1 µs/div→20 ns; **lower limit** 10 ns/div→1 ns (MIN_TIME_STEP); **upper limit** 100 s/div stays accuracy-limited (10 µs; DC-only circuit → 100 ns default), MAX_TIME_STEP 10 ms never exceeded |
| 2.9b | `[ ]` **Out of sync** (GUI): press dt `+`/`−` without touching time/div ⇒ manual dt kept; then change time/div ⇒ dt re-mapped and status bar says "Time step matched to scope"; history re-fills once, no stale spacing | Manual override survives until time/div changes |
| 2.10 | `[ ]` **Conflict:** time/div window > history span | Trace fills what exists; left edge not mirrored/garbage |
| 2.11 | `[ ]` Change component value while running (R, C, V, freq) | Sim absorbs change next step; no reset of time; capacitor state preserved (or documented reset) |
| 2.12 | `[ ]` Add/delete component/wire while running | Matrix rebuilt; no crash on node-count change; probes remapped correctly |
| 2.13 | `[ ]` Delete probed node while running | Channel goes flat/disabled, no read past history |
| 2.14 | `[ ]` Non-convergence case (two ideal voltage sources in parallel, different V; current source open) | Error message, sim halts cleanly, canvas still editable |
| 2.15 | `[ ]` Floating node (resistor with one end unconnected) | GMIN keeps solve stable; voltage shown ≈ 0 or sensible |
| 2.16 | `[ ]` No ground in circuit | Clear warning, no crash |
| 2.17 | `[ ]` Run 10 min continuous on Wien bridge | No drift/NaN; stderr not growing (see 0.3) |

---

## 3. Component Model Accuracy

Method: build the minimal circuit, probe, compare against hand calc. Tolerance ±2 % unless stated.
Also for **each** component: rotate 4× and verify terminal semantics don't change; save/load round-trip (§7).

### 3.1 Sources
| ID | Test | Expected |
|----|------|----------|
| 3.1.1 | `[ ]` DC 5 V + 1 kΩ | 5 mA; with Rint=100 Ω → 4.545 mA |
| 3.1.2 | `[ ]` AC 1 V 1 kHz, phase 90°, offset 1 V | Scope: peak 2 V, starts at max; measured f=1 kHz, Vrms=0.707 (AC part) |
| 3.1.3 | `[ ]` AC frequency sweep | Bode/scope show sweep; no jump when sweep wraps |
| 3.1.4 | `[ ]` DC current 1 mA into 1 kΩ | 1 V |
| 3.1.5 | `[ ]` Square 1 kHz 25 % duty, rise/fall 10 µs | Duty measurement 25 %; edges not vertical at 10 µs/div |
| 3.1.6 | `[ ]` Triangle / Sawtooth / Pulse / PWM / PWL / Expr source | Shape correct; PWL endpoints hold; Expr `sin(2*pi*1000*t)` parses; bad expr doesn't crash |
| 3.1.7 | `[ ]` Noise source | Non-repeating; FFT flat-ish |
| 3.1.8 | `[ ]` AM / FM / VCO | Envelope / freq deviation visible in FFT |
| 3.1.9 | `[ ]` Battery discharge | Voltage sags with load over sim time; `discharged` resets on Reset |
| 3.1.10 | `[ ]` Clock source | Logic-level square; period correct |

### 3.2 Passives
| ID | Test | Expected |
|----|------|----------|
| 3.2.1 | `[ ]` Divider 10 V, 1k/1k | 5.000 V |
| 3.2.2 | `[ ]` RC 1k/1µ step | τ=1 ms; 63.2 % at 1 ms |
| 3.2.3 | `[ ]` RL 1k/1mH step | τ=1 µs; check at 1 µs/div |
| 3.2.3a | `[ ]` **Inductor integration** (changed 2026-08-24 with #82–#90): series RLC R 20 / L 10 mH / C 100 nF (the RLC Step template) at dt 1 µs, 2 µs, 6 µs, 20 µs; count peaks and read the first peak | The inductor companion now uses the theta = 0.6 method like the capacitor (was backward Euler, which damped a high-Q LC by ≈ (ω dt)²/2 per step — a Q = 16 ring lost ~10 % per period at 200 steps/period). Expect first peak 9.5 V at 1–2 µs, ≈ 9.0 V at 6 µs, ~15 % low at 20 µs (sampling, not damping); the ring frequency stays 5.03 kHz at every dt. RL/RC step τ readings unchanged (63 % at τ within 1 % at dt ≤ τ/50) |
| 3.2.4 | `[ ]` RC LPF 1k/159n | −3 dB at 1 kHz (Bode) |
| 3.2.5 | `[ ]` Capacitor ESR/ESL/leakage, electrolytic reverse polarity | ESR shows in step; reversed electrolytic — defined behaviour |
| 3.2.6 | `[ ]` Inductor DCR / saturation | Current limits when saturated |
| 3.2.7 | `[ ]` Potentiometer wiper 0/50/100 % **changed while running** | Output tracks; no discontinuity glitch at 0/100 |
| 3.2.8 | `[ ]` Photoresistor vs Lux slider 0→100 % live | R changes monotonic |
| 3.2.9 | `[ ]` Thermistor vs Temp slider −40→125 °C live; resistor tempco | Beta model matches formula at 25 °C and 85 °C |
| 3.2.10 | `[ ]` Fuse 1 A rating, drive 2 A | Blows, stays blown, Reset repairs |
| 3.2.11 | `[ ]` Transformer 10:1, center-tap | 120 Vac → 12 Vac; CT gives ±6. The transformer is now an **ideal** transformer (V_s = N·V_p, primary current = N·I_s reflected, no leakage/magnetising L) — see §3.10.1 for the N range |
| 3.2.12 | `[ ]` Memristor, Crystal | Hysteresis / resonance happen without NaN (spark gap moved to §3.10) |

### 3.3 Diodes
| ID | Test | Expected |
|----|------|----------|
| 3.3.1 | `[ ]` Diode 5 V, 1k | ≈0.7 V drop, 4.3 mA |
| 3.3.2 | `[ ]` LED each of 9 colors (color cycle button) | Vf per color table in guide; brightness visual scales with I; over-current visual |
| 3.3.3 | `[ ]` Zener 5.1 V reverse with 1k from 12 V | 5.1 V ± Rz·I |
| 3.3.4 | `[ ]` Schottky | ≈0.3 V |
| 3.3.5 | `[ ]` Photodiode vs Lux | Photocurrent proportional |
| 3.3.6 | `[ ]` Varactor / Tunnel diode | C(V) changes LC freq; tunnel NDR region converges |

### 3.4 Transistors
| ID | Test | Expected |
|----|------|----------|
| 3.4.1 | `[ ]` NPN β=100, Ib=10 µA, in active | Ic≈1 mA |
| 3.4.2 | `[ ]` PNP mirror of 3.4.1 | Same magnitudes, signs flipped |
| 3.4.3 | `[ ]` Darlington pair | β²-ish, Vbe≈1.4 |
| 3.4.4 | `[ ]` NMOS Vth=2, Kn=1m, Vgs=4, sat | Id=4 mA (square law) |
| 3.4.5 | `[ ]` PMOS, NJFET (IDSS, VP), PJFET | Sign/pinch-off correct |
| 3.4.6 | `[ ]` Templates: CE, CS, CD, diff pair, push-pull, CMOS inverter, mirror, CCS | Gain/bias per template comments |
| 3.4.7 | `[ ]` UJT | Relaxation oscillator template fires |

### 3.5 Thyristors
| ID | Test | Expected |
|----|------|----------|
| 3.5.1 | `[ ]` SCR gate trigger → latches; drops out when I < Ihold | Latching correct, resets on Reset |
| 3.5.2 | `[ ]` TRIAC both polarities; DIAC breakover | Fires both halves of AC |

### 3.6 Op-amps & controlled sources (**touched by pending diff — high priority**)
| ID | Test | Expected |
|----|------|----------|
| 3.6.1 | `[ ]` Ideal op-amp follower / inverting −10 / non-inverting +11 | Exact gains |
| 3.6.2 | `[ ]` Real op-amp rails ±15, input 2 V, gain −10 | Output clamps at −15. Saturation is now stamped as a rail voltage source inside Newton iteration (all three op-amp types), with the post-solve clamp kept as a safety net — a resistor load current must equal V/R using the clamped V, and no other node may exceed the rails |
| 3.6.3 | `[ ]` Real op-amp single supply 0–5 V, comparator | Clamps at 0 and 5; no overshoot below 0 |
| 3.6.4 | `[ ]` Real op-amp GBW, slew rate | 1 V/µs slew visible on step |
| 3.6.5 | `[ ]` Real op-amp with output node = ground (edge case in clamp: `out_node==0`) | No crash |
| 3.6.6 | `[ ]` Flipped op-amp symbol | Same result as normal with swapped inputs; symbol polarity labels correct |
| 3.6.7 | `[ ]` Wien bridge + **RC phase-shift oscillator template (new)** | Phase shift ≈6.5 kHz (1k/10n) sustained sine, not rail-to-rail square; no DC-analysis debug flood; autostart flag works and doesn't re-trigger |
| 3.6.8 | `[ ]` Change Vmax/Vmin **while running** an oscillator | Clamp updates; no NaN |
| 3.6.9 | `[ ]` OTA, CCII±, VCVS/VCCS/CCVS/CCCS gain 2 | Output = 2×control |
| 3.6.10 | `[ ]` Templates: all op-amp circuits (follower … precision rectifier) | Each behaves per description |

### 3.7 Switches & relays
| ID | Test | Expected |
|----|------|----------|
| 3.7.1 | `[ ]` SPST/SPDT/DPDT toggle **while running** | Instant change, no residual current (v3.2.2 isolation fix) |
| 3.7.2 | `[ ]` Push button press/hold/release | Momentary; release on mouse-up outside |
| 3.7.3 | `[ ]` Relay coil V/R, analog switch control V | Contacts follow coil; hysteresis sane |

### 3.8 Logic & digital
| ID | Test | Expected |
|----|------|----------|
| 3.8.1 | `[ ]` Truth table for all 7 gates + buffer/tristate/Schmitt with Logic Input | All rows; tristate Hi-Z floats |
| 3.8.2 | `[ ]` Custom logic levels (Vlow/Vhigh/Vth) | Threshold respected |
| 3.8.3 | `[ ]` D/JK/T FF, SR latch with clock 1 kHz | Edge-triggered only; T divides by 2 |
| 3.8.4 | `[ ]` Counter, shift reg, mux/demux, decoder, BCD→7-seg, half/full adder | Correct outputs; 7-seg shows 0-9 |
| 3.8.5 | `[ ]` 555 astable (R1=1k,R2=10k,C=10n) & monostable | f≈6.9 kHz, duty 52 %; mono pulse 1.1RC |
| 3.8.6 | `[ ]` DAC/ADC, PLL, monostable, optocoupler | Round-trip value; PLL locks |
| 3.8.7 | `[ ]` Logic driven by analog ramp | Clean threshold crossing, no chatter |

### 3.9 Regulators, display, misc
| ID | Test | Expected |
|----|------|----------|
| 3.9.1 | `[ ]` 7805/7812 in 12/15 V, load 100 Ω | 5.00/12.00 V; dropout when Vin low |
| 3.9.2 | `[ ]` LM317 R1=240, R2=720 | 5.0 V; TL431 ref 2.5 V |
| 3.9.3 | `[ ]` Lamp power/voltage; DC motor | Brightness scales; motor speed |
| 3.9.4 | `[ ]` LED array 8 seg: drive, per-segment burn-out, color cycle, **Reset repairs all** (new) | Segments light in order; failed stays dark until Reset |
| 3.9.5 | `[ ]` LED matrix 8×8 | Pattern from inputs |
| 3.9.6 | `[ ]` Voltmeter/Ammeter/Wattmeter/Test point in status bar | Match probe values; ammeter zero-ohm doesn't break solve |
| 3.9.7 | `[ ]` Antenna TX/RX, Bus/Bus tap, Label (named node join) | Two same-name labels form one node |

### 3.10 High-voltage components (new 2026-08-24: `COMP_SPARK_GAP`, `COMP_TOROID`, `COMP_TLINE`, ideal `COMP_TRANSFORMER`)

Automated by `template_smoke --param-test` and `--tesla-test`; the manual rows exercise the same
limits in the GUI (property panel, live edits). Models: spark gap = hysteretic arc, breakdown
V = 3 kV/mm × gap_mm, on-state r_on, drops out below hold_current, re-arms after quench_time;
toroid = capacitance to ground from D/d inches (Bert Pool: C = (1.2781 − d/D)·2.8·√(π(D−d)d/4) pF);
transmission line = length_mi × (r_per_mi, x_per_mi, b_us_per_mi) with model 0 = R, 1 = R-L,
2 = nominal π (C/2 at each end).

#### 3.10.1 Parameter limits (`--param-test`)
| ID | Test | Expected |
|----|------|----------|
| 3.10.1.1 | `[ ]` Spark gap in series with 1 kVpk 1 kHz into 1 kΩ, gap_mm = 0.01, 0.1, 0.3, 0.5, 1, 10, 1000 mm | Fires iff breakdown (3 kV/mm × gap) < 1 kV: 0.01–0.3 mm conduct (load amplitude > 100 V), 0.5 mm (1.5 kV) and above stay open (load ≈ 0). No NaN at 0.01 mm (30 V, r_on 1 Ω) or 1000 mm (3 MV) |
| 3.10.1.2 | `[ ]` Spark gap GUI: set gap_mm = 0 and negative, r_on = 0, hold_current = 0, quench_time = 0 | Clamped/rejected by the property panel; the solver never sees a 0 Ω arc directly across an ideal source |
| 3.10.1.3 | `[ ]` Toroid shapes D×d in = 0.5×0.9 (d > D), 1×0.1, 13×4, 24×8, 100×30, 1000×1000 | C finite, > 0 and non-decreasing across the list; 13×4 ≈ 14.5 pF, 24×8 ≈ 26.5 pF; as an RC divider with 1 MΩ at 100 kV/100 kHz the node amplitude matches 100k/√(1+(ωRC)²) ±15 % |
| 3.10.1.4 | `[ ]` Toroid GUI: edit D then d live on a running Tesla coil; d ≥ D; D = 0 | C readout in the property panel follows the formula; degenerate shapes give a small finite C, not NaN |
| 3.10.1.5 | `[ ]` Transmission line 281.7 kVpk 60 Hz into 200 Ω, length = 0.001, 1, 10, 100, 500, 5000 mi × models R / RL / π | Load amplitude matches the phasor oracle ±5 %: R/RL: Vs·R_L/\|R_L + R + jωL\|; π: same with C/2 across the load (and source). Amplitude never increases with length by more than 2 % (monotone drop), finite at 5000 mi (RL: 1650 Ω + 4.4 H ⇒ ~10 % of Vs) |
| 3.10.1.6 | `[ ]` Line GUI: change model 0→1→2 **while running**, edit length/R/X/B live, B = 0 with model 2 | Re-stamps without a time reset; model 2 with B = 0 equals model 1; length 0 behaves as a short (R = 0 must not break the solve) |
| 3.10.1.7 | `[ ]` Transformer N = 0.001, 1/30, 0.4, 1, 19.17, 75, 1000 with 100 Vpk in and a load of 10k·N² + 1 Ω | Output amplitude = 100·N ±2 % (0.1 V … 100 kV); primary current = N × secondary current (ammeter/current view); no drift in the DC op point |
| 3.10.1.8 | `[ ]` Transformer GUI: N edited live 30:1 → 1:30 on Pole Xfmr; N = 0; both windings grounded on the same side | Scope autoset climbs to 100 kV/div; N = 0 rejected; a shared ground between windings is legal for the ideal model (no loop error) |
| 3.10.1.9 | `[ ]` Scope presets of every template (`template_time_div[]`, `template_volt_div[]`) | Each preset is an exact step of the scope's 1‑2‑5 tables (time 1 ns … 5 s, volts 1 mV … 500 kV) and the 20-division window shows 1.5–2000 cycles of the template's f_char (DC/SWITCH templates exempt) |
| 3.10.1.10 | `[ ]` Analog switch as a fault switch (`--param-test` 4b): 10 Vpk 60 Hz through `COMP_ANALOG_SWITCH` into 100 Ω, control = pulse 0→5 V (delay 20 ms, width 50 ms), r_on = 0.01, 0.3, 100, 1e6 Ω, r_off 1 GΩ, v_on 2.5 V | Load ≈ 0 (< 10 mV) before 20 ms, 10·100/(100 + r_on) ±5 % during the pulse (9.999 / 9.97 / 5.0 / 0.001 V); no NaN at r_on 0.01 across an ideal source; GUI: edit r_on / v_on live on a relay template, TRIP timing unchanged |
| 3.10.1.11 | `[ ]` Transformer as a CT (`--param-test` 4c): 1 kVpk into 10 Ω (100 A) through the primary, N = 120 / 400 / 2875, secondary grounded on one side into 1 Ω | Burden amplitude = 100/N V ±3 % (0.833 / 0.25 / 0.0348 V); GUI: on 50/51 change the CT ratio 120 → 400 live — burden drops to 2.1 Vpk and the relay stops tripping until the reference is lowered |

#### 3.10.2 Tesla coil (`--tesla-test`, templates #61–#63 in TEMPLATE_AUDIT)
Runs 20 ms at dt = 100 ns: NST (170 Vpk × 75 through 10 Ω) charges C1, the 3.2 mm gap (9.6 kV) fires, the
primary rings and pumps the secondary/toroid; counts primary-gap firings, streamer (rod gap) firings, the toroid
peak and the ring frequency on the toroid in the 60 µs after the first firing.
| ID | Test | Expected |
|----|------|----------|
| 3.10.2.1 | `[ ]` Tesla Coil (C1 25 nF, 4×13 in toroid 14.5 pF, rod 40 mm) | ≥ 2 gap firings in 20 ms (one per NST half-cycle, 8.3 ms), ring f = 186 kHz ±20 %, toroid peak ≥ 115 kV, streamer fires ≥ 1 |
| 3.10.2.2 | `[ ]` Tesla Coil (big top) (C1 38 nF, 8×24 in toroid 26.5 pF, rod 45 mm) | ≥ 2 firings, ring f = 152 kHz ±20 %, toroid ≥ 130 kV, streamer fires ≥ 1 (135 kV at 3 kV/mm) |
| 3.10.2.3 | `[ ]` Tesla Coil (detuned) (C1 18 nF ⇒ primary 220 kHz vs secondary 152 kHz) | ≥ 2 firings, ring f (secondary) ≈ 152 kHz ±20 %, streamer never fires, toroid peak **< 75 %** of the big-top coil |
| 3.10.2.4 | `[ ]` GUI: 20 µs/div, 100 kV/div, probe the toroid; then 5 ms/div | One burst per screen with the 186/152 kHz ring and a decaying envelope; at 5 ms/div bursts every 8.3 ms; arc glyphs appear on the gaps while conducting; set `TESLA_DEBUG=1` on the smoke tool for per-node peaks |
| 3.10.2.5 | `[ ]` Live retune: on the detuned coil set C1 = 38 nF; on the tuned coil set the toroid to 8×24 in | Envelope grows / shrinks within the next burst; no time reset, no NaN when C1 changes mid-ring |
| 3.10.2.6 | `[ ]` dt stress: run the tuned coil at dt = 1 µs and 10 µs | Ring aliases and the peak reads low — expected; must not diverge. Auto-dt should pick ≤ 100 ns from the 186 kHz DEMO_OSC f_char |

#### 3.10.3 Manual checks on the power-system templates (#54–#60, #64, #65)
| ID | Test | Expected |
|----|------|----------|
| 3.10.3.1 | `[ ]` 138 kV Line + VAR: **toggle the cap-bank switch while running** (50 kV/div, 5 ms/div) | Load-bus amplitude steps 105 → ~110 kVpk (74 → ~78 kV rms) within a cycle, phase lag shrinks; a short damped ring at the switch-in instant is acceptable, growth is not; toggle back restores 105 kVpk |
| 3.10.3.2 | `[ ]` Ferranti (open line): **toggle the reactor switch while running** | Far end drops from 309.6 kVpk (+9.9 %) to ≈ 282 kVpk (source level); reopen and the rise returns; no beat that grows (reactor+line C resonate near 58 Hz) |
| 3.10.3.3 | `[ ]` 345 kV Line: click the line, **edit length 100 → 200 → 0.001 mi while running**; then model 1 → 0 → 2 | Load drops 264 → ~245 kVpk → 281.7 kVpk; model 0 removes the lag, model 2 adds ~0.5 % rise; every change re-stamps without resetting time |
| 3.10.3.4 | `[ ]` 12.47 kV Feeder: load 51.84 → 26 Ω, length 5 → 10 mi | −3.2 % → −6.4 % / −6.3 % (outside ANSI ±5 %); the `V=` readouts on both ends update |
| 3.10.3.5 | `[ ]` Pole Xfmr and Grid chain: probe the 7.2 kV side and the 240 V side on separate channels; use Stack view | 10.18 kVpk vs 339 Vpk both readable (kV labels on CH1, V on CH2); current view shows the 30:1 particle-density change through the transformer |
| 3.10.3.6 | `[ ]` Grid chain: scale the house load 11.5 Ω → 11.5 mΩ → 0.115 mΩ | House sags ~239 → ~225 V → ~5 % below nominal as the feeder, 138 kV and then the 345 kV drops appear; no NaN at 0.115 mΩ (500 MW) |
| 3.10.3.7 | `[ ]` Line Model Ladder: probe all three load buses (stack), edit row 3's B to 60 µS/mi and row 2's X to 0 | Row 1 110.7, row 2 110.1, row 3 110.5 kVpk; row 3 rises above row 1 with big B; row 2 equals row 1 with X = 0 |
| 3.10.3.8 | `[ ]` Line Drop Basics: wire 1 → 2 → 0 Ω | 10.909 → 10.0 → 12.0 V; wattmeter on the wire reads 1.19 W → 0.8 W → 0 |
| 3.10.3.9 | `[ ]` Save/Load each power template (`.ckt` + JSON) | tline length/R/X/B/model, transformer N, switch state, toroid D/d and spark-gap gap/r_on/hold/quench round-trip exactly |

### 3.11 Protection & control examples (new 2026-08-24: templates #66–#72 in TEMPLATE_AUDIT, `docs/RESEARCH_AEP_PC.md` §5)

Automated by `--demo-test` (`DEMO_SWITCH` / `DEMO_WAVEFORM` contracts), `--probe-test` (nine oracles) and
`--param-test` 4b/4c; `--trace NAME T` is the explorer for these. Building blocks: fault = `COMP_ANALOG_SWITCH`
(r_on 0.3 Ω) with its control pin on a `COMP_PULSE_SOURCE` (0 → 5 V, `delay` / `pulse_width` / `period`);
relay = diode peak-hold (C, bleed R ⇒ τ) into an op-amp comparator (`ideal=false`, gain 1e5, ±15 V rails)
against a DC reference; CTs/VTs are ideal transformers (120, 400, 1/2875). TRIP is +15 V / −15 V; BFT is 0 / 5 V.
Every manual row is done **(a) before Run and (b) live while running**; nothing may reset time or produce NaN.

| ID | Test | Expected |
|----|------|----------|
| 3.11.1 | `[ ]` CT + 50/51: probe TRIP (auto) and the pulse source on CH2, 10 ms/div, 5 V/div | TRIP sits at −15 V, rises to +15 V within a cycle of the pulse going high at 40 ms, stays high past the pulse end (100 ms) while the 10 µF/10 k hold (τ 100 ms) decays through the 8 V reference, drops before the next fault at 240 ms; repeats every 200 ms |
| 3.11.2 | `[ ]` CT + 50/51: pulse `delay` 40 → 120 ms; `pulse_width` 60 → 10 ms; `period` 200 → 1000 ms | TRIP follows the pulse edge each time; a 10 ms fault still trips (the hold catches one peak) and releases on the same τ; one trip per second at period 1 s; no time reset on any edit |
| 3.11.3 | `[ ]` CT + 50/51: pickup reference 8.0 → 12 → 5 V | 12 V: pickup 1.1 kA — still trips on the 5 Ω fault (≈ 2.1 kA); 5 V: trips permanently (normal-load hold ≈ 6.4 V > 5); back to 8 V restores the normal/fault split |
| 3.11.4 | `[ ]` CT + 50/51: fault R 5 → 13 → 60 Ω; CT ratio 120 → 400 | 13 Ω ⇒ ≈ 1.2 kA (the value in the on-canvas note), burden 14 Vpk, trips and releases ≈ 56 ms after the fault; 60 Ω ⇒ ≈ 700 A < 738 A pickup ⇒ **no trip**; N = 400 ⇒ burden 2.1 Vpk normal / 7.4 Vpk fault ⇒ nothing trips until the reference is lowered to ≈ 6 V |
| 3.11.5 | `[ ]` CT + 50/51: hold τ — R 10 k → 100 k (τ 1 s), C 10 µF → 1 µF (τ 10 ms) | τ 1 s: TRIP latches across several fault periods (release > 1 s); τ 10 ms: hold ripples at 60 Hz, TRIP chatters when the hold is near 8 V, releases within ~20 ms of the fault end |
| 3.11.6 | `[ ]` 87 Differential: probe TRIP (auto) and the differential burden R_d, 20 ms/div | Internal fault 100–160 ms: R_d ≈ 30 Vpk, TRIP +15 V, released ≈ 75 ms later (τ 22 ms from 30 V to 1 V); through fault 240–300 ms: R_d ≈ 0, TRIP stays −15 V; normal load: R_d ≈ 0 |
| 3.11.7 | `[ ]` 87 Differential: swap the two pulse delays; internal fault R 2 → 20 → 200 Ω; hold τ 22 → 220 ms | Order flips (trip in the second half only); 20 Ω still trips (≈ 4 Vpk), 200 Ω misses (≈ 0.4 Vpk < 1 V) — plain 87 sensitivity limit; τ 220 ms keeps TRIP high into the through fault (hold only, not a relay error) |
| 3.11.8 | `[ ]` 87 Differential: CT2 ratio 120 → 110 with reference 1.0 → 0.1 V | 9 % ratio mismatch ⇒ ≈ 0.4 Vpk on R_d at load, ≈ 3 Vpk on the through fault ⇒ **false trip** at 0.1 V reference — the reason real 87s add a restraint slope; restore 120 / 1.0 V and the through fault is quiet again |
| 3.11.9 | `[ ]` 21 Distance: probe TRIP (auto), the `|I| x Z_set` hold and the `|V| (VT)` hold (stack), 20 ms/div | Pre-fault: VT hold ≈ 98 V, I·Z hold ≈ 5 V, TRIP −15 V. 40 % fault (100–160 ms): I·Z ≈ 107 Vpk vs V ≈ 54 Vpk ⇒ TRIP +15 V, released shortly after 160 ms (τ 22 ms). 100 % fault (240–300 ms): I·Z ≈ 59 vs V ≈ 74 Vpk ⇒ **no trip** |
| 3.11.10 | `[ ]` 21 Distance: **move the fault by editing the segment lengths** (sum stays 50 mi): seg1 20 → 40 mi, then 45 mi; then seg1 back to 20 and replica R 3.35 → 4.2 Ω | 40 mi (80 %): balance point, TRIP marginal/flickers; 45 mi (90 %): no trip on either fault; replica 4.2 Ω (100 % reach): the far-end fault now trips — zone-1 overreach, which the 80 % setting exists to prevent |
| 3.11.11 | `[ ]` 21 Distance: CT ratio 400 → 200; VT ratio 1/2875 → 1/1437; 30 Ω in series with the 40 % fault switch | N_CT 200 doubles I·Z ⇒ overreach (100 % fault trips); VT ×2 halves the reach (40 % fault no longer trips); 30 Ω fault resistance ⇒ \|Z_app\| ≈ 33 Ω > 24.1 Ω ⇒ the 40 % fault is missed (resistive underreach) |
| 3.11.12 | `[ ]` 50BF: probe BFT (auto), the timer capacitor and both pulses, 20 ms/div then 100 ms/div; check dt | Stuck breaker: START at 50 ms, C ramps with τ 150 ms, crosses 3.16 V (0.632 × 5) at 200 ms ⇒ BFT 5 V from 200 to 350 ms, again 800–950 ms; C discharges back through the 10 k into the gate's 0 V output after 350 ms. No AC source ⇒ auto-dt may be coarse: set dt ≈ 100 µs manually if the ramp looks stepped |
| 3.11.13 | `[ ]` 50BF: **set the 50BF current pulse width to 83 ms** (healthy breaker, 5 cycles), then 120 and 150 ms | 83 ms: START drops at 133 ms, C peaks at 2.13 V < 3.16 ⇒ **no BFT**; 120 ms: 2.75 V, no BFT; 150 ms: 3.16 V exactly at 200 ms ⇒ marginal one-sample BFT (document what the comparator does at the boundary) |
| 3.11.14 | `[ ]` 50BF: timer R 10 k → 5 k; C 15 → 30 µF; reference 3.16 → 4.5 V; TRIP `delay` 50 → 100 ms | τ 75 ms ⇒ BFT at 125 ms (would misoperate on a healthy breaker); τ 300 ms ⇒ C reaches 3.16 V only at 350 ms ⇒ marginal; 4.5 V needs 2.3 τ = 345 ms ⇒ marginal; TRIP delay 100 ms ⇒ START/BFT shift 50 ms later (BFT at 250 ms) |
| 3.11.15 | `[ ]` SIL Loading: probe both ends, 5 ms/div, 100 kV/div; **toggle SW live** (2 × SIL) | SIL: far end 269 kVpk (0.956 of 281.7 with R and one π); SW closed: ≈ 0.80 within a cycle, no growing ring; reopen restores 0.956. Length 200 → 300 mi: still ≈ flat at SIL, much worse at 2 × SIL; load 283 → 566 Ω: far end above the source |
| 3.11.16 | `[ ]` Series Compensation: probe both ends; **close the bypass SW live**; C 44.2 → 22.1 → 14.7 µF | Cap in: load end 0.89 (250.6 kVpk); bypassed: ≈ 0.80; the source-end probe barely moves. 22.1 µF (100 %): far end ≈ source; 14.7 µF (150 %): far end above the source, large cap voltage. A ~42 Hz beat after a switch operation must decay, not grow (series L-C below 60 Hz — SSR in miniature) |
| 3.11.17 | `[ ]` 765 kV Line: probe both ends at 200 kV/div; load 250 → 125 Ω; length 300 → 600 mi; then rebuild as 3 × 100 mi `COMP_TLINE` parts | SIL: 598.6 kVpk (0.958); 2 × SIL ≈ 0.8; 600 mi as one π is near its own resonance (ω²LC ≈ 0.8) and reads nonsense — the 3-section ladder stays sane; scope autoset reaches 200 kV/div without overflow |
| 3.11.18 | `[ ]` Model toggles on the relay templates: set the comparator op-amp `ideal=true`; set the hold diode ideal | `ideal=true` op-amp = virtual-short model ⇒ the comparator stops working (output floats) — expected, keep `ideal=false`; ideal diode raises the normal-load hold on 50/51 from 6.4 to 7.07 V (0.9 V from the 8 V pickup) |
| 3.11.19 | `[ ]` Current view / `--flow-test` on 50BF; ammeter on an AND-gate pin | Behavioural gates have no terminal currents: no particles in/out of the gates, KCL is only asserted on the passive nodes (R/C/comparator); an ammeter on a gate pin reads 0 — document, do not file as a bug |
| 3.11.20 | `[ ]` Save/Load each of #66–#72 (`.ckt` + JSON) | Analog-switch r_on/r_off/v_on, pulse delay/width/period/levels, op-amp `ideal=false` + gain, DC reference, CT/VT ratios, tline segment lengths and switch states round-trip exactly; TRIP/BFT timing identical after reload |
| 3.11.21 | `[ ]` `template_smoke --trace "50/51" 0.2`, `--trace 87 0.3`, `--trace 21 0.3`, `--trace 50BF 1.2` | Per-node min/max match the rows above (burden ±7 Vpk normal, TRIP −15…+15, hold peaks, VT secondary ±98 V, BFT 0…5 V); the final switch states are all open (pulse low at the end of the run). Any node whose min/max is a rail on both ends or 0/0 unexpectedly is a merged-node (10 px) suspect |

### 3.12 Three-phase examples (new 2026-08-24: templates #73–#76 in TEMPLATE_AUDIT)

Automated by `--demo-test` (`DEMO_WAVEFORM` 60 Hz) and `--probe-test` (four oracles). Three `COMP_AC_VOLTAGE`
sources carry `phase` 0 / −120 / +120°; a `template_extra_probes[]` table puts up to three extra probes on
load so every phase and the neutral show at once — use the scope **Stack** button (§4.1a) to separate them.
Every manual row is done **(a) before Run and (b) live while running**; nothing may reset time or produce NaN.

| ID | Test | Expected |
|----|------|----------|
| 3.12.1 | `[ ]` 3-Phase Y Balanced: load, press **Stack**, 2 ms/div, 100 V/div | Four bands: phase A (source, 392 Vpk), B and C loads (373.3 Vpk) and the neutral (flat ≈ 0). Successive zero crossings of A, B, C are 5.56 ms (120°) apart in the order A → B → C |
| 3.12.2 | `[ ]` Phase sequence: swap the B and C source phases (−120 ↔ +120) live; then set all three to 0° | Same amplitudes, the zero-crossing order becomes A → C → B (reversed sequence); all at 0° ⇒ the three currents add, neutral = 3 × 37.3 A × 1 Ω ≈ 112 Vpk and each load drops to ≈ 291 Vpk |
| 3.12.3 | `[ ]` Balanced: neutral R 1 → 1 mΩ, then 1 MΩ | No channel changes by more than solver noise — the neutral carries nothing when balanced |
| 3.12.4 | `[ ]` 3-Phase Unbalanced: probe neutral (auto), B and C loads, Stack, 5 ms/div, 100 V/div; cursors on the neutral | Neutral ≈ 20.8 Vpk 60 Hz, lagging phase A by ≈ 20°; loads A ≈ 355, B ≈ 387, **C ≈ 403 Vpk** (above the 392 V source — the lightly loaded phase is overvolted) |
| 3.12.5 | `[ ]` Unbalanced: **neutral shift** — R_n 1 → 1 mΩ (solid), then 1 MΩ (open); set the neutral channel to 50 V/div first | Solid: neutral ≈ 0, loads 373 / 382 / 387 Vpk (phases independent). Open: neutral ≈ **144 Vpk**, phase C ≈ 500 Vpk, phase A ≈ 250 Vpk — the "lost neutral" hazard. Waveform continuous at the change, no NaN at either extreme |
| 3.12.6 | `[ ]` Unbalanced: load C 40 → 10 Ω (rebalance); load A 10 → 1 Ω | Rebalanced: neutral → 0, all loads 373.3. 1 Ω: neutral ≈ 100 Vpk, R_n dissipates ~5 kW (power readout), still no convergence trouble |
| 3.12.7 | `[ ]` 3-Phase 345 kV Line: probe all three loads, Stack, 100 kV/div; then load C → 400 Ω and → 10 MΩ | Three bands at 264 kVpk 120° apart, neutral ≈ 0. 400 Ω: phase C ≈ 273 kVpk, neutral ≈ 25 kVpk (zero-sequence current). Open phase: neutral ≈ 90 kVpk; with R_n → 1 MΩ as well the star point floats and healthy phases are overvolted |
| 3.12.8 | `[ ]` 3-Phase 345 kV Line: one phase's `COMP_TLINE` model 1 → 2 (π), one phase length 100 → 200 mi | Model change moves the balanced answer < 1 % at 100 mi; the 200 mi phase drops more (≈ 0.87) and neutral current appears |
| 3.12.9 | `[ ]` 6-Pulse Rect: probe plus bus (auto), minus bus (extra) and phase A; 1 ms/div, 50 V/div; then dt 20 µs | Plus bus follows the highest phase (169.3 Vpk), minus bus the lowest; V+ − V− ripples between ≈ 254 and 293 V at 360 Hz — six pulses per 16.7 ms. At dt 20 µs the commutation notch at each 30° crossover is resolved; no NaN when two diodes hand over |
| 3.12.10 | `[ ]` 6-Pulse Rect: add 100 µF across the load; set phase B amplitude 170 → 0; swap B/C phases; 60 → 50 Hz | Cap: ripple collapses to a few volts (cap only bridges the 30° dip). One phase lost: 120 Hz ripple with deep dips (degrades to single-phase full-wave). Sequence swap: identical buses (rectifiers ignore sequence). 50 Hz: 300 Hz ripple |
| 3.12.11 | `[ ]` FFT on the neutral (#74) and on V+ − V− (#76, math channel or probe both) | Neutral: pure 60 Hz line, no harmonics (linear loads). Rectifier: DC + 360 Hz and multiples, nothing at 60/120/180 Hz when balanced; 120 Hz appears as soon as one phase is unbalanced |
| 3.12.12 | `[ ]` Save/Load each of #73–#76 (`.ckt` + JSON) | Source `phase` values (0 / −120 / +120), tline model and length, neutral R and diode Is round-trip exactly; the Stack view and the extra probes are restored (probe save/load, §7) |

### 3.13 Signal generators (new 2026-08-24: templates #77–#81 in TEMPLATE_AUDIT, Sedra & Smith ch. 18, `docs/RESEARCH_OSCILLATORS.md`)

Automated by `--demo-test` (`DEMO_SWITCH` for the bistable, `DEMO_OSC` for the four generators), `--probe-test`
(bistable rail, triangle 7.5 V, sine 4.9 V) and `--osc-test` with a **per-case dt** (200 ns / 200 ns / 5 ns / 20 ns; Hartley 5 ns and Clapp 2 ns added with #82–#83, rows 3.13.21–3.13.24).
All op-amps are `ideal=false`, gain 1e5, ±15 V — the virtual-short model is dead in positive-feedback and
integrator roles. `--trace NAME T` is the first thing to run on a dead generator (it found the missing R2 wire).

| ID | Test | Expected |
|----|------|----------|
| 3.13.1 | `[ ]` Bistable: probe OUT (auto) and the triangle, 2 ms/div, 5 V/div; cursors at the edges | OUT ±15 V; falling edge when the triangle passes **+7.5 V**, rising edge at **−7.5 V** (2.5 ms and 7.5 ms apart). Two edges per 10 ms input cycle |
| 3.13.2 | `[ ]` Bistable: **X-Y view** (Y-T button), X = triangle, Y = OUT; persistence on | A rectangular hysteresis loop 15 V wide (−7.5 … +7.5) and 30 V tall, traversed counter-clockwise for the inverting bistable; the loop is drawn once per cycle and does not drift |
| 3.13.3 | `[ ]` Bistable: R2 10 → 30 k; R1 10 → 20 k; triangle 10 → 5 Vpk; rails ±15 → ±10 V | R2 30 k: thresholds ±3.75 V (loop narrows on X-Y). R1 20 k: ±10 V — the 10 V triangle barely reaches them, switching may stop until the triangle is raised to 12 V. 5 Vpk: no switching, OUT parks at one rail (expected; the demo contract would fail). ±10 V rails: thresholds ±5 V, loop 10 V tall |
| 3.13.4 | `[ ]` Bistable: op-amp `ideal=true`; then back, gain 1e5 → 1e3 | `ideal=true` ⇒ latch dead (output floats or sits at one rail) — expected, restore `ideal=false`. Gain 1e3: same thresholds, 30 mV of visible slope on the edges |
| 3.13.5 | `[ ]` Triangle/Square: probe triangle (auto) + square (extra), Stack, 100 µs/div, 5 V/div; measurements | Triangle ±7.5 V and square ±15 V at **5.00 kHz**; the square edges coincide with the triangle peaks. Start-up: the 0.5 V/50 µs kick makes it run from the first cycle |
| 3.13.6 | `[ ]` Triangle/Square **retuning**: R 10 → 20 k; C 10 → 4.7 nF; R1 10 → 5 k; R2 20 → 40 k; rails ±15 → ±10 V — each live | R 20 k: 2.5 kHz, amplitude unchanged. C 4.7 nF: 10.6 kHz. R1 5 k: ±3.75 V *and* 10 kHz (f ∝ R2/R1). R2 40 k: same as R1 5 k. ±10 V rails: ±5 V, f unchanged (rails cancel out of f). The ramp continues from its current value at every change — no time reset |
| 3.13.7 | `[ ]` Triangle/Square: R2 20 → 10 k; either op-amp `ideal=true`; dt 200 ns → 5 µs | R2 10 k: thresholds equal the rails ⇒ the loop stalls (expected). `ideal=true` ⇒ dead. dt 5 µs: triangle overshoots the thresholds by one step (~0.75 V) and f reads low — time-base effect, restore ≤ 1 µs |
| 3.13.8 | `[ ]` Function Generator: probe sine (auto) + triangle (extra), 100 µs/div, 2 V/div; then **FFT** at 200 µs/div | Sine ≈ 4.9 Vpk at 5 kHz with rounded, symmetric peaks. FFT: fundamental at 5 kHz, **3rd harmonic > 30 dB down**, no even harmonics |
| 3.13.9 | `[ ]` Function Generator retuning: R 10 → 20 k; C 10 → 4.7 nF | 2.5 kHz / 10.6 kHz with the *same* wave shape and harmonic content — the shaper depends on amplitude only |
| 3.13.10 | `[ ]` Function Generator amplitude: R2 20 → 40 k (triangle ±3.75 V); then re-scale the bias sources 2.0 → 1.0 V and 3.7 → 1.85 V | Before re-scaling: only the first breakpoint is reached, output ≈ 3.4 Vpk with pointed tops and the 3rd harmonic rises to ≈ −20 dB. After re-scaling: sine at half amplitude (≈ 2.45 Vpk), 3rd harmonic back > 30 dB down |
| 3.13.11 | `[ ]` Function Generator shaper edits: bias 3.7 → 5 V; 22 k → 10 k; one diode `ideal=true` | 5 V: second breakpoint never reached, pointed peaks. 10 k: over-flattened tops (3rd harmonic phase flips, level rises). One ideal diode: that breakpoint moves 0.6 V, asymmetric wave ⇒ **even harmonics appear** in the FFT |
| 3.13.12 | `[ ]` Colpitts: probe drain (auto), 500 ns/div, 5 V/div, dt 5 ns; measure f; 20 µs/div for the envelope | 710–712 kHz, drain swings around the 12 V rail (up to ≈ 2 × VDD), class-C flat-bottom when limiting; start-up envelope grows over the first ~20 µs after the 0.3 V/50 ns kick |
| 3.13.13 | `[ ]` Colpitts retuning: C1 1 → 2 nF; C1 → 0.5 nF; C2 1 → 2 nF; L 100 → 47 µH | C1 2 nF: **616 kHz** (the on-canvas note says 581 kHz — that is C_eq = 0.75 nF, i.e. C1 = 3 nF; note text to fix in `template_notes[]`). C1 0.5 nF: 872 kHz, feedback fraction halves, must still start. C2 2 nF: 616 kHz but C2/C1 = 2 needs g_m R > 2 — may not start (raise kp or VDD). 47 µH: 1.04 MHz, dt must follow |
| 3.13.14 | `[ ]` Colpitts limits: RFC 1 mH → 10 µH; vth 1.5 → 3 V; kp × 10; kick v_high 0.3 → 0; dt 5 ns → 100 ns → 1 µs | 10 µH RFC loads the tank ⇒ amplitude collapses. vth 3 V lowers g_m ⇒ stops (start-up criterion). kp × 10 ⇒ hard limiting. No kick ⇒ starts from numerical noise only, much later. dt 100 ns: f reads ~5 % low; dt 1 µs: does not start — time-base, not model |
| 3.13.15 | `[ ]` Ring Oscillator: probe the last stage (auto) then all five gate outputs, Stack, 2 µs/div, 2 V/div; measure f | 0/5 V squares at ~139–145 kHz; the five outputs are shifted by one fifth of a half-period (≈ 0.7 µs) each; RC nodes are exponential ramps between the rails |
| 3.13.16 | `[ ]` Ring **per-stage C** retuning (live): one C 1 → 2 nF; all five → 2 nF; one C → 0.1 nF; one R 1 → 10 k | One 2 nF: ≈ 120 kHz (period 6.9 → 8.3 µs). All 2 nF: ≈ 72 kHz. One 0.1 nF: ≈ 160 kHz (the other four dominate). One 10 k: ≈ 55 kHz. Waveform continuous at each edit |
| 3.13.17 | `[ ]` Ring topology: delete one stage (4 inverters); add two (7); kick v_high 3 → 0 | Even count ⇒ **latches**, no oscillation (demo contract fails — expected). 7 stages ≈ 100 kHz. No kick: record whether it still starts (gate initial state vs the 2.5 V metastable point) |
| 3.13.18 | `[ ]` Ring: current view, `--flow-test`, ammeter on a gate pin; dt 20 ns → 1 µs | Behavioural gates show no particles in/out and KCL is asserted only on the RC nodes; ammeter on a gate pin reads 0 — document, not a bug. dt 1 µs: RC ramps become 1-step staircases and f reads ~20 % high |
| 3.13.19 | `[ ]` Save/Load each of #77–#81 (`.ckt` + JSON) | Op-amp `ideal=false` + gain, rails, pulse-kick v_high/width/period (100 s one-shot), triangle-source amplitude/frequency, diode bias sources, NMOS vth/kp, inductors and every per-stage R/C round-trip exactly; the generators restart at the same f after reload |
| 3.13.20 | `[ ]` `template_smoke --trace "Triangle/Square Gen" 0.004`, `--trace "Colpitts (MOSFET)" 60e-6`, `--trace "Ring Oscillator" 200e-6` | Every node shows a real min/max spread (triangle ±7.5, square ±15, drain swinging around 12 V, gate outputs 0/5). A node with min = max (e.g. an op-amp + input pinned at 0) is the forgotten-wire signature that caught the bistable's R2 |
| 3.13.21 | `[ ]` Hartley (#82): probe drain (auto), 500 ns/div, 5 V/div, dt 5 ns; measure f; compare with the Colpitts at the same drive | Drain swings around the 12 V rail (L1 is the drain load — no RFC) at **~557 kHz**, not the 503 kHz of 1/(2π√((L1+L2)C)): the 10 nF coupling cap is in series with L2 (L2_eff ≈ 40 µH) and the MOSFET C_gs/C_gd shift the tap — a real pull, not a bug (`--osc-test` band ±25 %). Same class-C flat bottom as the Colpitts |
| 3.13.22 | `[ ]` Hartley **retuning** (live): C 1 → 2 nF; L2 50 → 100 µH; L1 50 → 100 µH; coupling cap 10 → 100 nF, then → 1 nF, then delete it | C 2 nF: 356 kHz formula, ≈ 390 kHz measured (same ~10 % pull). L2 100 µH: 411 kHz, L1/L2 = 0.5 starts more easily. L1 100 µH: 411 kHz but L1/L2 = 2 may not start (raise kp). 100 nF: f falls toward ≈ 510 kHz (series-C pull gone). 1 nF: L2_eff collapses, f > 700 kHz or dead. Deleted: **dead** — the 6 V gate bias is shorted to ground through L2 (the DC-path trap); `--trace "Hartley (MOSFET)" 80e-6` shows the gate at min = max |
| 3.13.23 | `[ ]` Clapp (#83): probe drain (auto), 200 ns/div, 5 V/div, dt 2 ns; measure f; then **C3 100 → 47 pF**; C3 → 220 pF; C3 → 10 nF | **1.74–1.76 MHz** (formula 1.744 with C_eff = 83.3 pF, within 1 % — contrast the Hartley's +10 %). 47 pF: **2.43 MHz** (note says 2.4). 220 pF: 1.19 MHz. 10 nF: it is the Colpitts again, 712 kHz — dt may then be needlessly fine but harmless |
| 3.13.24 | `[ ]` Clapp stability demo: C1 1 → 2 nF (and C2 → 2 nF); vth 1.5 → 2.5 V; kp × 10; dt 2 ns → 20 ns → 100 ns | C1 × 2: f moves only −2 % (1.707 MHz) where the Colpitts moves −13 % for the same edit — the point of the Clapp. C2 × 2: same f, C2/C1 = 2 may not start. vth 2.5 V: may already stop (small feedback fraction); kp × 10 restores it. dt 20 ns: f a few % low; 100 ns: does not start — time-base, not model |

### 3.14 Textbook set (new 2026-08-24: templates #84–#90 in TEMPLATE_AUDIT, Agarwal & Lang / Sedra & Smith, `docs/RESEARCH_TEXTBOOK_CIRCUITS.md` items 1, 2, 6, 7, 8, 9, 26; palette groups Basics / **Transients** (new, 10th group) / Op-amps)

Automated by `--demo-test` (`DEMO_DC` Thevenin / Superposition, `DEMO_WAVEFORM` the rest) and `--probe-test` (3.00 V, 7.333 V,
5 V settled RC / RL, 9.53 V first ring peak, 5.0 V critical row, 15 V clip). The step templates use a 0/5 V `COMP_SQUARE_WAVE`
(amplitude 2.5, offset 2.5) into `series_series_shunt()`. **Probe-dt rule:** `--probe-test` runs at the template's scope-preset
dt when that is finer than auto-dt (the ring needs 1 µs from 50 µs/div); in the app the same happens when you pick time/div (§2.9a).

| ID | Test | Expected |
|----|------|----------|
| 3.14.1 | `[ ]` Thevenin (#84): probe the load (auto), 1 ms/div, 2 V/div; power readout on R_L | **3.00 V** flat (V_th 6 V, R_th 2.2 k); 4.09 mW = the maximum-power point since R_L = R_th |
| 3.14.2 | `[ ]` Thevenin **load edits** (live): R_L 2.2 k → 1 k; → 10 k; → 100 Ω; → 10 M (open); → 1 Ω (short, read the current); R2 3 → 6 k; source 10 → 20 V | 1.875 V; 4.92 V; 0.261 V; **6.00 V = V_th**; **2.73 mA = I_N**; R2 6 k: V_th 7.5 V, R_th 2.5 k ⇒ 3.51 V; 20 V: 6.00 V. Every change is instantaneous (resistive), no transient, no time reset |
| 3.14.3 | `[ ]` Superposition (#85): probe node N (auto), 1 ms/div, 2 V/div | **7.33 V** = 4 (12 V alone) + 2 (6 V alone) + 1.33 (1 mA into 4 k‖4 k‖4 k) |
| 3.14.4 | `[ ]` Superposition **source zeroing** (live): current 1 → 0 mA; V1 12 → 0; V2 6 → 0; current → 2 mA; V1 → 24 V | 6.00 V; 3.33 V; 5.33 V; 8.67 V; 11.33 V — each contribution adds linearly and the differences reproduce the single-source values |
| 3.14.5 | `[ ]` Superposition **current-source polarity**: rotate the `COMP_DC_CURRENT` from 180° back to 0°; then Save/Load with it at 180° | At 0° it *sinks* 1 mA (the model's current leaves the "−" terminal) ⇒ **4.67 V** — a plausible wrong answer; the template ships it rotated 180° so it injects. After reload N must still read 7.33 V (rotation round-trips) |
| 3.14.6 | `[ ]` RC Step (#86): probe input + capacitor (auto), 1 ms/div, 1 V/div; **cursor A on the rising edge, cursor B at the 63 % point (3.16 V)**; rise-time measurement | Δt = **1.00 ms** = τ = RC; 10–90 % rise 2.2 ms; settled (99 %) at 5 ms, exactly the half period, so the discharge starts from 5.00 V |
| 3.14.7 | `[ ]` RC Step retuning (live): R 10 → 20 k; C 100 → 10 nF; f 100 → 1 kHz; duty 50 → 20 %; source 0/5 → 0/10 V | R 20 k: τ 2 ms, peaks 4.59 V, never settles (the "max" oracle would read 4.59 — expected). 10 nF: τ 100 µs, use 100 µs/div. 1 kHz: rounded triangle 1.89–3.11 V. 20 %: charges 2 τ (4.32 V), discharges 8 τ. 10 V: 6.32 V at τ. V_C continues from its present value at each edit |
| 3.14.8 | `[ ]` RC Step models and dt: C `ideal=false` ESR 10 Ω / leakage 1 M; source Rint 100 Ω; dt 1 µs → 100 µs → 1 ms | ESR: 5 mV step at the edge; leakage: final 4.95 V; Rint: τ 1.01 ms. dt 100 µs: 63 % point reads ≈ 1.05 ms (theta 0.6 with 10 steps per τ); dt 1 ms: one step per τ — staircase, expected |
| 3.14.9 | `[ ]` RL Step (#87): probe input + resistor (auto), 100 µs/div, 1 V/div; cursors at 63 % | V_R = 100 · i_L rises to **5.00 V (50 mA)**, 3.16 V at **τ = 100 µs**; V_in − V_R (math channel) jumps to 5 V at the edge and decays — the current is the state that cannot jump |
| 3.14.10 | `[ ]` RL Step retuning (live): L 10 → 20 mH; R 100 → 1 k; R → 10 Ω; f 1 → 10 kHz; L → 1 mH | 20 mH: τ 200 µs, peaks 4.59 V. 1 k: τ 10 µs, still 5 V final (5 mA). 10 Ω: τ 1 ms ⇒ rounded triangle 1.89–3.11 V, 500 mA. 10 kHz at 100 Ω: same triangle. 1 mH: τ 10 µs. i_L continues at every edit — no current spike |
| 3.14.11 | `[ ]` RL Step models and the **inductor integration change**: L `ideal=false` DCR 10 Ω, then 100 Ω; dt 2 µs → 10 µs → 100 µs | DCR 10: final 4.55 V, τ 91 µs; DCR 100: 2.5 V, τ 50 µs. The inductor now uses theta = 0.6 (was backward Euler): 63 % at 100 µs within 1 % at dt 2 µs, ≈ 105 µs at dt 10 µs, a 1-step staircase at dt 100 µs = τ — expected |
| 3.14.12 | `[ ]` RLC Step (#88): probe input + capacitor (auto), 50 µs/div, 2 V/div; read the first peak; **cursors on two successive peaks**; 500 µs/div for the envelope | First peak **9.5 V** (formula 9.53, ζ 0.032), successive peaks × 0.82 per cycle; **ring period 199 µs** (5.03 kHz); envelope τ 1 ms — decayed to 8 % before the next edge at 2.5 ms |
| 3.14.13 | `[ ]` RLC Step retuning (live): R 20 → 632; → 2 k; → 5 Ω; → 100 Ω; C 100 → 400 nF; L 10 → 40 mH; square 200 Hz → 2 kHz | 632: critical, 5 V no overshoot. 2 k: overdamped, τ 195 µs. 5 Ω: peak 9.88 V, envelope 4 ms, rings into the next edge. 100 Ω: peak 8.0 V, ~5 cycles. 400 nF: 2.52 kHz, peak 9.10 V. 40 mH: 2.52 kHz, peak 9.75 V (L raises Q, C lowers it). 2 kHz: each edge's ring rides on the last — looks chaotic, is linear. Continuous at every edit |
| 3.14.14 | `[ ]` RLC Step **dt resolution**: with 50 µs/div (dt auto-maps to 1 µs, §2.9a) then dt by hand 6 µs, 20 µs, 100 µs; and 1 ms/div (dt re-maps to 20 µs) | 1 µs: 9.5 V. 6 µs (the old auto-dt for a 200 Hz square): ≈ 9.0 V — the value the oracle used to read before the probe-dt rule. 20 µs: 10-point polygon, peak ~15 % low. 100 µs: ring gone. **1 ms/div re-maps dt to 20 µs and the ring visibly degrades** — go back to 50 µs/div or set dt manually; document, not a bug |
| 3.14.15 | `[ ]` RLC Step models: L DCR 10 Ω; C ESR 1 Ω; source Rint 10 Ω | DCR: R_total 30 ⇒ peak 9.3 V, envelope 0.67 ms; ESR: small step at the edge; Rint: same as DCR |
| 3.14.16 | `[ ]` Damping Ladder (#89): auto probe on the critical row + the two extra probes, **Stack view**, 100 µs/div, 2 V/div; cursors on row 2 at 95 µs and on row 3 at 63 % | Row 1 rings to 9.5 V; row 2 (R = 632 = 2√(L/C)) rises with **no overshoot**, 4.0 V at t = 3/ω0 = 95 µs; row 3 (2 k) reaches 63 % at ≈ 200 µs (slow root τ 195 µs). Three independent circuits, one edge, three shapes |
| 3.14.17 | `[ ]` Damping Ladder edits (live): row 2 R 632 → 500 / 800 / 300; row 3 R 2 k → 20 k; row 1 R 20 → 100; one row's C 100 → 400 nF; square 200 → 50 Hz | 500: ζ 0.79, 1.7 % overshoot (5.09 V — the 2 % oracle just passes). 800: ζ 1.27, slower, no overshoot. 300: 19 % overshoot (5.9 V). 20 k: τ ≈ RC = 2 ms > half period, never settles. 100 Ω: peak 8.0 V. 400 nF in row 2: critical R is now 316 Ω, so 632 is overdamped (ζ 2). 50 Hz: every row settles before the next edge |
| 3.14.18 | `[ ]` Damping Ladder models and dt: inductor DCR 10 Ω in every row; dt as 3.14.14 | DCR barely moves rows 2–3 (632 → 642, 2000 → 2010 effective) but changes row 1's ζ by 50 % — the low-R row is the sensitive one. Row 1 shows dt aliasing first; rows 2–3 are fine even at 20 µs |
| 3.14.19 | `[ ]` Op-Amp Saturation (#90): probe output (auto) and the **inverting-node probe** (extra; set it to 0.2 V/div), 200 µs/div, 5 V/div; FFT | Output −10× sine with flats at **±15.0 V** for 46 % of each half-cycle (clip starts at \|v_i\| = 1.5 V). The − input is ≈ 0 while the loop is closed and shows **±0.45 V** pulses exactly during the flats: (v_i R2 + v_o R1)/(R1 + R2) = (200 k − 150 k)/110 k. FFT: odd harmonics only |
| 3.14.20 | `[ ]` Op-Amp Saturation edits (live): amplitude 2 → 1 Vpk; → 5 Vpk; R2 100 → 50 k; R1 10 → 5 k; rails ±15 → ±10 V; f 1 → 10 kHz | 1 V: clean −10 V sine, v− ≈ −100 µV (v_o/A). 5 V: near-square, v− peaks 3.18 V. R2 50 k: gain −5, 10 Vpk, no clip, v− ≈ 0. R1 5 k: gain −20, clip from 0.75 V, v− peak 1.19 V. ±10 V rails: flats at ±10 V from 1 V, v− peak 0.91 V. 10 kHz: unchanged with the finite-gain model. Immediate at every edit |
| 3.14.21 | `[ ]` Op-Amp Saturation models: op-amp `ideal=true`; `COMP_OPAMP_REAL` swap; load 10 k → 100 Ω under each | Finite-gain `ideal=false` is the reference. `ideal=true`: rails are stamped for the ideal model too — record whether it clips at 15 V and whether v− still leaves 0 V or is pinned by the virtual short (document). Real op-amp: slew rate rounds the clip corners, GBW trims the gain at 10 kHz, output R droops the 15 V into 100 Ω; the ideal-rail model stays at 15 V into 100 Ω (no output R) |
| 3.14.22 | `[ ]` Save/Load each of #82–#90 (`.ckt` + JSON) | Inductor / capacitor values incl. 100 pF and 50 µH, the Hartley's 10 nF coupling cap, the square-wave amplitude/offset/duty/frequency, the current source's 180° rotation, op-amp `ideal=false` + gain + rails, and the extra probes (damping rows, inverting input) round-trip exactly; Thevenin 3.00 V and Superposition 7.33 V after reload; the oscillators restart at the same f |
| 3.14.23 | `[ ]` `template_smoke --trace "Hartley (MOSFET)" 80e-6`, `--trace "Clapp (MOSFET)" 30e-6`, `--trace "RLC Step (Ringing)" 6e-3` | Hartley/Clapp: gate and drain show real spreads (gate min = max = 0 was the missing bias wire on the first Hartley build). RLC: capacitor max ≈ 9.5 V at the trace's dt — if it prints ≈ 9.0 V the run used the coarse auto-dt, not the scope-preset dt |

---

### 3.15 Batch five: tuned / CB / Darlington amplifiers, SR latch, three-phase plants (new 2026-08-24: templates #91–#96 in TEMPLATE_AUDIT, S&S 7.3.5 / 7.3.7 / 15.1.1, AEP power-system practice)

Automated by `--demo-test` (`DEMO_BANDPASS` tuned amp, `DEMO_WAVEFORM` CB / Darlington / plants, `DEMO_SWITCH` SR latch) and
`--probe-test` (six oracles, see 0.7). New component **`COMP_SOURCE_3PH`** (A / B / C at `phase`, −120°, +120°, common N; per-phase
`r_series` + `l_series`; **three MNA aux variables** via `component_aux_count()`). The two plants use `three_phase_fanout()` +
`xfmr_row()` (grounded single-phase banks, so the phases are independent circuits). Every manual row is done **(a) before Run and
(b) live while running**; nothing may reset time or produce NaN. Hand calculations are in the TEMPLATE_AUDIT blocks.

| ID | Test | Expected |
|----|------|----------|
| 3.15.1 | `[ ]` Single-Tuned Amplifier (#91): load, `Trk` on, 2 V/div; watch the 0.5 s sweep 20–500 kHz; then sweep off, f = 100.06 kHz, 5 µs/div | One narrow spike (~6 kHz wide) near 100 kHz at ≈ 83 ms into the sweep, ≈ 4.5–5 Vpk for 10 mV in; fixed at f0: 5 cycles per screen, output inverted (CE). `--probe-audit` flags it SMALL at the sweep start — expected |
| 3.15.2 | `[ ]` **Retune the tank** (live): C 2.53 → 10 nF; L 1 → 4 mH; Rq 10 k → 1 k; Rq → 100 k; delete the emitter bypass cap; f = 94 / 106 kHz with the sweep off | 10 nF: peak moves to 50.3 kHz (Q 8). 4 mH: 50 kHz, Q 4, broader. Rq 1 k: Q 1.6, gain ≈ 50, broad hump. 100 k: gain up 2–3× (r_o limits, not 5×). No bypass: gain ≈ 9 flat. 94 / 106 kHz: −3 dB points (0.7 × peak). The tank rings for ~16 cycles after each step edit, then settles — no NaN |
| 3.15.3 | `[ ]` Tuned amp dt: 5 µs/div (dt auto 100 ns), then dt by hand 1 µs, 10 µs; amplitude 10 → 100 mV | 1 µs: peak ~10 % low, f0 ~1 % low. 10 µs: aliased, no resonance (document). 100 mV: clips against the 12 V rail (collector swings ≈ ±10 V), flat tops |
| 3.15.4 | `[ ]` Common Base (#92): probe input + load (auto), 20 µs/div, 1 V/div | ≈ 1.8–1.9 Vpk **in phase** with the 10 mV input (the CE inverts); DC V_C 7.2 V, V_E 3.05 V (`--verbose` bias) |
| 3.15.5 | `[ ]` **Source resistance on the CB** (live): insert 50 Ω between the source and the input cap; then 600 Ω; then source `ideal = false` Rint 50 Ω instead | 50 Ω: output ÷ 3 (0.63 Vpk) — R_in = r_e = 25 Ω; 600 Ω: 0.075 Vpk; Rint 50 Ω gives the same 1/3 as the physical resistor. Compare with the Common Emitter, which does not care about 50 Ω |
| 3.15.6 | `[ ]` CB edits (live): Rc 4.7 → 2.2 k; Re 3 → 1.5 k; amplitude 10 → 100 mV; f 10 kHz → 100 Hz; delete the base bypass cap | 2.2 k: ≈ 88×. **Re 1.5 k: saturates** (V_C 2.6 V < V_E) — output collapses; raise R2 or lower Rc first. 100 mV: clips at ≈ +4.8 / −4.2 V. 100 Hz: gain ÷ 6.5 (input cap X_C 159 Ω ≫ 25 Ω). No bypass: gain drops by ~(1 + 6.9 k/(β r_e)) |
| 3.15.7 | `[ ]` Darlington Follower (#93): probe input + emitter (auto), 200 µs/div, 2 V/div, Stack | Output ≈ 0.9 Vpk on ≈ 4.2–4.6 V DC, in phase, undistorted; input 6 V ± 1 V. `--verbose`: I_E ≈ 46 mA, I_B1 ≈ 5 µA |
| 3.15.8 | `[ ]` Darlington edits (live): source 100 k → 10 k; → 1 M; Re 100 → 1 k; → 10 Ω; β 100 → 50 on both; delete Q2 and wire Q1's emitter to Re; offset 6 → 2 V; amplitude 1 → 5 Vpk | 0.99; 0.50; 0.99; 0.50 (and 0.4 A); 0.71; **0.09** (single follower, R_in 10 k); 0.7 V DC and the negative half cuts off (class-A limit); 5 Vpk still follows (Q1 collector 12 V) |
| 3.15.9 | `[ ]` SR Latch (#94): four probes (S, Q auto; Qbar, R extra), **Stack**, 200 µs/div, 2 V/div | S 50 µs at 0.2 ms, Q rises with S and holds; R 50 µs at 0.6 ms, Q falls; Q a 0.4 ms pulse every 1 ms; Qbar the complement. Record the power-up state before 0.2 ms (Q low expected; Q high is a benign race). `--probe-test` "max" at 0.5 ms = 5 V — this oracle caught the first build's S/R swap |
| 3.15.10 | `[ ]` **S/R timing edits** (live): R delay 0.6 → 0.3 ms; R delay → 0.1 ms; swap S and R delays; R delay → 0.2 ms (overlap); S width 50 → 5 µs; S `v_high` 5 → 1 V; period 1 → 2 ms | Q width 0.1 ms; Q high 0.2 → 1.1 ms (wraps the period); same; **overlap: both outputs 0 for 50 µs then a race on release** — whichever input drops last wins, identical widths ⇒ evaluation order decides (document, not a bug); 5 µs still sets (no minimum pulse width; dt ≤ 1 µs to see it); 1 V never sets (below threshold); 0.4 ms every 2 ms. The latch keeps its state across every edit |
| 3.15.11 | `[ ]` SR Latch dt: 200 µs/div (dt ≈ 4 µs), then dt by hand 100 µs | At 100 µs a 50 µs pulse can fall between samples ⇒ Q never sets — expected for a pulse-only circuit (the harness forces run/1000; the app maps time/div). Back at 4 µs it sets every period |
| 3.15.12 | `[ ]` Power Plant (#95): probes A / B / C load buses + generator A, **Stack view of the phases**, 5 ms/div, 100 kV/div (generator channel 5 kV/div) | Three ≈ 260 kVpk sines 120° apart (A-B-C); generator 14.7 kVpk; ≈ 0.90 of the 281.7 kVpk ideal (X'' reflected 25.5 Ω + 6 + j55 Ω line). `--probe-audit` flags the generator channel SMALL at 100 kV/div — expected |
| 3.15.13 | `[ ]` **Open a breaker** (live, then before Run): breaker A open; re-close; open B and C too | Phase A load bus 0 V within a step, B and C unchanged (grounded banks — no neutral coupling); re-close ⇒ back with only the line's L/R settling (τ ≈ 24 ms); all three open ⇒ all 0, generator terminals still 14.7 kVpk. No NaN, no time reset |
| 3.15.14 | `[ ]` Plant edits (live): `l_series` 0.184 → 1.84 mH; → 0; loads 198.4 → 99.2 Ω; line 100 → 200 mi; turns 19.17 → 15; `phase` 0 → 30°; `frequency` 60 → 50; load B → 400 Ω; `r_series` 1 mΩ → 1 Ω | ≈ 150 kVpk (weak machine); ≈ 262; ≈ 211; ≈ 223; ≈ 205; all three shift 30°; +1 %; phase B alone rises to ≈ 271 (independent circuits); **r_series 1 Ω halves the bus** (reflected × 19.17² = 367 Ω) — keep it in mΩ, it sits on the 18 kV side |
| 3.15.15 | `[ ]` Plant models: transformer `ideal` toggle; line model 1 → 2; breaker r_on 1 Ω | Leakage ≈ 17 Ω ⇒ −3 %; π model ⇒ +1 % (line charging); r_on ⇒ −0.5 % |
| 3.15.16 | `[ ]` Transmission Substation (#96): probes A / B / C feeder buses + grid A, Stack, 5 ms/div, 50 kV/div (grid channel 100 kV/div) | Three ≈ 103 kVpk sines (−8 % from 112.7 ideal: lagging pf 0.9 load through 3.9 + j21.6 Ω feeder + reflected line), lagging the grid by ≈ 30° |
| 3.15.17 | `[ ]` **Close the cap banks** (live): bank A only; then B, then C; then all three before Run | Only the switched phase recovers, +5.6 % (≈ 109–110 kVpk) — obvious in Stack; each closure gives a half-cycle bump (≈ 270 Hz ring through the feeder L, damped in ~2 cycles); all three closed before Run: 109–110 kVpk from t = 0, lag ≈ 8° |
| 3.15.18 | `[ ]` Substation **open a breaker** (live): breaker B open, bank B closed / open | Phase B feeder bus 0 V (the cap bank then just sits on a dead bus — no ring, no NaN); A and C unchanged; re-close ⇒ back |
| 3.15.19 | `[ ]` Substation edits (live): cap 6.1 → 12.2 µF; load L 0.22 → 0; load R 171.5 → 85.7 Ω; auto N 0.4 → 0.36; feeder 30 → 60 mi | 12.2 µF: ≈ 113 kVpk, *above* nominal (over-compensated, leading — why banks are voltage-switched); unity-pf load: ≈ 108.7 kVpk and the bank now raises it further; 180 MW: ≈ 89 kVpk, bank +8 %; tap −10 %: everything × 0.9; 60 mi: ≈ 96 kVpk |
| 3.15.20 | `[ ]` Substation models: transformer `ideal` toggle; line / feeder model 1 → 2; inductor DCR 5 Ω; cap ESR | −1–2 %; +0.5 % (charging); −0.3 % (pf 0.89); none |
| 3.15.21 | `[ ]` `COMP_SOURCE_3PH` alone: place one (default 392 Vpk / 60 Hz / 1 mΩ / 0 H), three 10 Ω loads A/B/C to ground, N to ground; probe A, B, C, N; Save/Load | 392 Vpk at 0 / −120 / +120°, N 0 V; `phase` 90 shifts all three; `l_series` 1 mH ⇒ 0.377 Ω in series per phase; JSON and `.ckt` round-trip all five props and the four node ids; deleting it while running frees its three aux variables (no stale index, §10.6) |
| 3.15.22 | `[ ]` Save/Load each of #91–#96 (`.ckt` + JSON) | Tank L / C / Rq, the sweep 20–500 kHz / 0.5 s, the 6 V AC offset, the S / R pulse delays and widths, breaker and cap-bank switch states (incl. one opened / closed by hand), transformer ratios 19.17 / 0.4, `COMP_TLINE` lengths and per-mile constants, the 3-phase source props and all extra probes round-trip; oracles re-read after reload |
| 3.15.23 | `[ ]` `template_smoke --trace "Power Plant (3-phase)" 0.05`, `--trace "Transmission Substation" 0.05`, `--trace "SR Latch (NOR)" 0.001`, `--probe-audit "Single-Tuned"` | Plant / substation: three distinct phase nodes with equal ± spreads at every stage (a node with the same min/max as its neighbour is the merged-phase signature — B/C merged and C-onto-P2 both showed up here); SR: S / R / Q / Qbar all 0–5 V. Probe-audit on the tuned amp: SMALL at the sweep start, output owner = the 100 k load |

---

### 3.16 Batch six: IC I/O & drivers (new 2026-08-27: templates #97–#108 in TEMPLATE_AUDIT, palette group **IC I/O & drivers**)

Automated by `--demo-test` (`DEMO_WAVEFORM`, six periods for MHz-class templates) and `--probe-test` (twelve oracles, see 0.7),
`--burn-test` (50 Ω coil 5 W, 100 Ω load 3 W, 33 Ω terminations 0.5 W). New helper `logic_pulse()` gives every pulse source edges of
1 % of its period (a 2 ns edge into a 100 ns step made the BJT stage unsolvable). The BJT model change (base–collector junction
in ideal mode) is regression-tested by the existing 86 oracles. Every manual row is done **(a) before Run and (b) live**.

| # | Check | Expected |
|---|-------|----------|
| 3.16.1 | `[ ]` Push-Pull Output: run; delete the PMOS; load 20 pF → 1 nF | inverted 0–3.3 V square, edges < 20 ns; without the PMOS the output only falls (floats); 1 nF: RC ramps |
| 3.16.2 | `[ ]` Open-Drain: C 100 pF → 1 nF; R 4.7 k → 1 k | rise τ 470 ns → 4.7 µs (never reaches 3.3 V in 2.5 µs) → 100 ns |
| 3.16.3 | `[ ]` Open-Collector: base R 1 k → 100 k → 1 M; rail 5 → 12 V | V_OL ≈ 20 mV stays until the base starves (1 M: linear region); 12 V rail: same waveform at 12 V; **no negative collector voltage** (model fix) |
| 3.16.4 | `[ ]` I2C Bus: Stack; slave delay 15 → 0 µs; bus C → 1 nF | SDA = NOT(master OR slave); overlapping pulls extend the low; 1 nF: 6 µs lows barely recover |
| 3.16.5 | `[ ]` I2C Level Shifter: gate rail 3.3 → 1.8 V; 5 V rail → 12 V | 5 V side follows the 3.3 V side low/high; 1.8 ↔ 5 and 3.3 ↔ 12 both work |
| 3.16.6 | `[ ]` GPIO Input: Stack (4 bands); C 100 nF → 10 nF → 1 µF; switch r_on → 1 k; threshold → 3 V | latency 1–2 ms → 0.1 ms → 20 ms (press missed); dirty contact 0.3 V still LOW; 3 V threshold chatters |
| 3.16.7 | `[ ]` Low-side Switch: delete the diode; gate 5 → 3.3 → 2 V; L → 100 mH | drain 12.65 V clamp → kV spike without the diode; 2 V gate = linear heater; 100 mH plateau lasts the off-time |
| 3.16.8 | `[ ]` High-side Switch: Stack; rail 12 → 24 V; PMOS w → 50 µm | gate 12/0, load in phase 0/11.8 V; works at 24 V; small PMOS only reaches ~8 V |
| 3.16.9 | `[ ]` SPI: Stack (4 bands); C → 1 nF; SCLK 10 → 50 MHz; R 33 → 0 | rounded edges 6.6 ns; 1 nF triangle ~2.5 V peak; 50 MHz collapses to ~1 V; 0 Ω instant edges (no ringing: no L modelled) |
| 3.16.10 | `[ ]` UART: divider 1 k/1 k; threshold 2 → 3.5 V; RX C → 100 nF | 2.5 V (no margin); 3.3 V TX no longer registers on a 3.5 V V_IH input; 100 nF smears bits |
| 3.16.11 | `[ ]` RS-485: Stack; noise 1 → 3 Vpk; far termination open; inverter r_out → 1 k | receiver output stays a clean 0/5 V; A/B levels change, no reflections modelled; weak B still resolved |
| 3.16.12 | `[ ]` SPMI: C 15 → 150 pF; rail 1.8 → 1.2 V; SCLK → 26 MHz | edges appear at 150 pF; 1.2 V picture identical with 0.4 V margin; 26 MHz still square |
| 3.16.13 | `[ ]` Scope **AC** / **Fit** (Display tab) on Two-Stage Amp, CE, Diff Pair, Instr. Amp | Fit: per-band tags "CHn 5.0mV/div"; both traces readable; axis labels hidden; trigger line follows the band; AC alone: shared V/div, traces centred |
| 3.16.14 | `[ ]` Multi-input probes: Summing (4 ch), Difference (3), Instr (3), Superposition (3), Diff Pair (3) | every input on its own channel; diff pair inputs 10 mV, both collectors mirror images (0.5 V) |
| 3.16.15 | `[ ]` **Automated:** `template_smoke --probe-test`, `--demo-test`, `--burn-test`, `--flow-test`, `--knob-test` | 98/98, 108/108, 0 overloads, 108/108, 1328/0 |

### 3.17 Batch seven: Texas voltage levels and building services (new 2026-08-27: templates #109–#120 in TEMPLATE_AUDIT, palette group **Residential & commercial**, `docs/RESEARCH_ERCOT_STANDARDS.md`)

Automated by `--demo-test`, `--probe-test` (twelve oracles), `--burn-test` and the new **`--std-test`**
(19 buses against ERCOT Planning Guide 4 / NERC TPL-001-5.1 P0, ANSI C84.1 Range A and NEC 210.19(A); it
fails on a >0.03 pu drift from the documented value and prints `[NOTE] … (documented exception)` for the
three buses that sit outside their band on purpose). Every manual row is done **(a) before Run and (b) live**.

| # | Check | Expected |
|---|-------|----------|
| 3.17.1 | `[ ]` 69 kV: load 20 → 25 MVA; line 20 → 40 mi | 0.957 → 0.946 pu (an ERCOT steady-state violation) → ≈ 0.92 pu |
| 3.17.2 | `[ ]` Texas Ladder: Stack + Fit, read the five band tags | 100 kV / 200 V / 50 kV / 20 kV / 5 kV per division; buses 0.991 / 0.971 / 0.959 / 0.988 pu and 117.4 V per leg |
| 3.17.3 | `[ ]` Texas Ladder: set the 69/12.47 kV transformer ratio 0.189735 → 0.1807 (LTC neutral) | the service falls to ≈ 112 V, below ANSI C84.1 Range A — the reason the LTC exists |
| 3.17.4 | `[ ]` Wind: open the string-B switch; set a string phase 6 → 0 ° | export and the collector rise both halve; at 0 ° the export stops and the bus falls below the grid |
| 3.17.5 | `[ ]` Industrial: halve the motor resistance | the 4.16 kV and 480 V buses sag together; both stay above 0.95 pu until roughly double load |
| 3.17.6 | `[ ]` 240/120 V: check L1 and L2 are 180 ° apart; **neutral 0.02 → 5 Ω** | L1-L2 = 240 V; with the bad neutral L1 collapses and L2 rises past 126 V (the open-neutral failure) |
| 3.17.7 | `[ ]` Branch: compare the two loads; shorten the #14 run to 50 ft; load 12 → 20 A | 114.2 V vs 117.7 V; 50 ft brings #14 inside 3 %; 20 A pushes it to ≈ 8 % |
| 3.17.8 | `[ ]` AC start: watch the 50 ms contactor; service R 0.20 → 0.05 Ω | panel 333 → 310 Vpk (7 %) while the motor runs; with the stiffer service the dip is under 2 % |
| 3.17.9 | `[ ]` Solar: compare the PCC with the utility; double the inverter current | +9.7 Vpk (123.4 V per leg); doubled it passes 126 V — where IEEE 1547 volt-var/volt-watt takes over |
| 3.17.10 | `[ ]` 480Y/277: Stack; delete the lighting load | three bands 120 ° apart, A lowest; without the lighting A matches B and C |
| 3.17.11 | `[ ]` 208Y/120: neutral 0.05 → 1 Ω; then balance the loads | the three branches spread apart badly; balanced, the neutral current goes to zero |
| 3.17.12 | `[ ]` PFC: **close the cap-bank switch**; then 478 → 956 µF | shunt trace shrinks ≈ 20 % (123 → 95 A) and moves into phase; 956 µF over-corrects and the current climbs |
| 3.17.13 | `[ ]` ATS: watch the 20 ms dead window; generator delay 70 → 40 ms | load dead 50–70 ms then restored; overlapped contactors make the two sources fight |
| 3.17.14 | `[ ]` **Automated:** `template_smoke --std-test` | 19 buses, **0 drifted**, 3 outside their band (all printed as documented exceptions) |
| 3.17.15 | `[ ]` **Automated:** `--probe-test`, `--demo-test`, `--burn-test`, `--flow-test`, `--knob-test`, `--geom-test` | 110/110, 120/120, 0 overloads, 120/120, 1518/0, all twelve new templates geometrically clean |

### 3.18 Batch eight: reliability standards and simulation methods (new 2026-08-28: templates #121–#129, palette group **Grid standards & methods**, `docs/RESEARCH_GRID_STANDARDS.md`)

| # | Check | Expected |
|---|-------|----------|
| 3.18.1 | `[ ]` N-1: open the second circuit's breaker live | 0.970 → 0.925 pu; below the P0 floor, inside the 0.92 post-contingency envelope; 4.8 % deviation |
| 3.18.2 | `[ ]` IBR: watch the 150 ms fault at 100 ms; then open the inverter breaker | POI falls to ≈ 0.29 pu and the inverter keeps injecting; with the breaker open it stops (the legacy trip) |
| 3.18.3 | `[ ]` BOLD: compare the two receiving buses; Stack | conventional 0.921 pu, BOLD 0.989 pu on the same corridor and load |
| 3.18.4 | `[ ]` Derating: **drag the Tmp slider 25 → 75 °C**; then close the summer block | conductor 6.0 → 7.2 Ω and the bus falls; with both, ≈ 0.915 pu — neither alone breaks Range A |
| 3.18.5 | `[ ]` Facility Rating: close the extra-load switch and watch each element's label | only the CT shows an overload marker (125 %); the conductor is at 63 % |
| 3.18.6 | `[ ]` Kron: compare the Y and delta load traces | they overlay exactly — 91.38 V and 81.59 V on both halves |
| 3.18.7 | `[ ]` R/X: close each reactive block in turn | the transmission bus moves mostly with vars; the feeder bus moves comparably with watts and vars |
| 3.18.8 | `[ ]` Governor: read the nadir and the settling value; then droop 90 k → 180 k, and the integrator cap 10 → 5 µF | nadir ≈ −0.168 Hz at 1.2 s, settles −0.143 Hz; doubled droop doubles the deviation; halved H deepens the nadir |
| 3.18.9 | `[ ]` Alarm loop: watch the contact open at 4 s; open the integrity switch; short the pair | 8.5 V normal, 9.2 V alarm, 12 V cut, 0 V short |
| 3.18.10 | `[ ]` **Automated:** `template_smoke --switch-test` | Every SPST switch in every template, run in both states and measured at that template's probed output: **22 switches, 0 failed**. A switch that moves the output less than 1 % fails unless it has a `switch_cases` entry saying why (a phase-B breaker cannot move a phase-A probe). |
| 3.18.11 | `[ ]` **Automated:** `--probe-test`, `--std-test`, `--demo-test`, `--burn-test`, `--flow-test`, `--knob-test`, `--geom-test` | 119/119, 23 buses 0 drifted, 129/129, 0 overloads, 129/129, 1652/0, all nine new templates clean |

### 3.19 Schematic layout and oscillator audit (2026-08-28)

Three checks were tightened after the Hartley/Colpitts report and a ground-under-capacitor overlap:

| # | Check | Expected |
|---|-------|----------|
| 3.19.1 | `[ ]` **Automated:** `--geom-test` now also reports `overlap=` | No two component symbols may overlap. The check compares the **drawn bodies** (65 % of the box across the leads, 85 % of the height, rotation aware, text excluded, 4 px slack) rather than the full bounding box, which includes lead stubs. **131/131 have overlap = 0** and **0 diagonal wires**, so the two hard layout rules hold everywhere. Five real collisions were fixed to get there: the kick source's ground in the Triangle/Square and Function Generator cores, the replica resistor's ground sitting on the line section in 21 Distance Zone 1, the AC source's ground in Neg Clamper, and the emitter resistor, load resistor and 12 V rail ground in the Single-Tuned Amplifier. |
| 3.19.1b | `[ ]` Remaining geometry warnings | 32 of 158 templates still report `cross=` (13 drawn wire crossings, which are legitimate in some topologies) or `through=` (43 wires passing over a node they do not connect to — cosmetic, tracked). |
| 3.19.2 | `[ ]` **Automated:** `--osc-test` frequency tolerance | Was ±25 %, which let a Hartley running 10 % fast pass. Now **±5 %**, and it immediately caught the Phase Shift oscillator (−8 % from loading) and the Hartley (+6 %); both expectations are now the measured values with the physical reason documented on the canvas. |
| 3.19.3 | `[ ]` **Automated:** `--osc-test` waveform shape | New: AC rms ÷ peak-to-peak over the **settled quarter** of the run — 0.354 sine, 0.5 square, 0.289 triangle, ±12 %. Nothing previously checked shape, so a clipped or notched "sine" passed as long as it crossed its mean. Colpitts 0.355, Clapp 0.339, Hartley 0.370, Ring 0.471, Tri/Square 0.299. |
| 3.19.4 | `[ ]` LC oscillators on screen | Colpitts / Hartley / Clapp swing about a 12 V rail and were being clipped by the top of the graticule; all three now preset **AC coupling** so the tank waveform is centred and complete. |
| 3.19.5 | `[ ]` Hartley coupling capacitor | 10 nF sat in series with the 1 nF tank cap (pulling f 10 % high) and resonated with L2 near 225 kHz, putting a notch on the falling edge. Now 220 nF. |

### 3.20 Batch nine: MOSFET / CMOS / X-Y / hardware, and the ideal-vs-real models (2026-08-28: templates #130-#158)

Templates #130-#151 (MOSFET amplifier set, transistor-level CMOS, X-Y and arbitrary waveform,
the hardware-engineering lab) and #152-#158 (palette group **Ideal vs real models**).

Six component properties were editable in the panel but never reached the solver, so changing
them did nothing. All six now stamp, and every one of them is gated on `ideal == false`, which
no pre-existing template sets - the check below is that nothing else moved.

| # | Check | Expected |
|---|-------|----------|
| 3.20.1 | `[ ]` Ideal vs Real Source: read all three loads; then untick **Ideal** on the first source | 5.000 / 4.167 / 1.667 V. Unticking drops the first to 4.167 V - the divider with r_series = 200 |
| 3.20.2 | `[ ]` Ideal vs Real Diode: compare the two peaks; then tick **Ideal** on the second diode | 0.30 V against 0.486 V; ticking collapses the second onto the first |
| 3.20.3 | `[ ]` Ideal vs Real Capacitor: the three ripple shapes | a clean 250 mVpp triangle, then the same triangle with a +/-25 mV and a +/-100 mV square added by ESR. Set ESL to 1 uH on the third to round the edges instead |
| 3.20.4 | `[ ]` Ideal vs Real Inductor: overshoot on the rising edge | 8.94 V (zeta = 0.05) against 6.80 V (DCR 50 ohm, zeta = 0.30); the second ring is gone within three cycles |
| 3.20.5 | `[ ]` Ideal vs Real Op-Amp: the three rows at 100 kHz | 500 mV (ideal), 354 mV (GBW 1 MHz at Acl 10 = -3 dB at exactly 100 kHz), and a **triangle** ~1.25 V tall where 5 V was asked for. Drop the source to 10 kHz and the third becomes a clean 1 V sine |
| 3.20.6 | `[ ]` Ideal vs Real BJT / MOSFET: both collector and both drain voltages | 7.28 / 6.87 V (Early effect, V_AF = 80 V) and 7.05 / 5.65 V (lambda = 0.05). Set V_AF to 1e9 or lambda to 0 and the halves meet |
| 3.20.7 | `[ ]` **Regression:** no existing template changes | Every part defaults to `ideal = true` except the diode, whose default flips to `false` - which is what the solver has always done. `--probe-test`, `--demo-test`, `--osc-test` all unchanged except the two noted below |
| 3.20.8 | `[ ]` **Model fix:** BJT Early effect | It read V_CE back out of the CLAMPED V_bc (clamped to ~0.13 V so exp() stays finite), so V_AF = 80 V produced a 1 % effect instead of 9 %. Now taken from the node voltages |
| 3.20.9 | `[ ]` **Model fix:** inductor theta-method history | The history term used the whole previous terminal voltage, including the DCR drop, adding a spurious +K R I_prev that cancelled part of the winding resistance: a branch set to zeta = 0.30 rang as if it were 0.21. Both converter oracles moved (Cuk 14.996 -> 13.302, closer to the ideal 12 V; PDN 1.6605 -> 1.5921) |
| 3.20.10 | `[ ]` **Automated:** the full battery | 158/158 templates, 158/158 demos, **159/159 probe oracles** (17 new: both halves of every ideal-vs-real comparison), 9/9 oscillators, 23/23 switches, 0 burns, 2104 knob runs 0 failed, 23 std buses 0 drifted, 158/158 flow, 0 param-preset failures, 0 layout failures |
| 3.20.11 | `[ ]` **Automated:** `--geom-test` on the new templates | All seven ideal-vs-real templates report `diag=0 cross=0 through=0 touch=0 overlap=0`. The op-amp rows were rewired to get there: the wire feeding the + input ran along R1's own column, which reads - correctly - as a wire straight through the resistor |

### 3.21 Pop-out bench scope (2026-08-28)

`PopOut` now opens a 1120 x 700 window (it was 600 x 400, which made the popped-out screen
smaller than the docked one) laid out as an instrument: the graticule in a recessed bezel, a
front panel of six working knobs down the right, and a status plate for the settings that are
switches rather than knobs. The docked scope also starts taller (375 px from y = 220, was
300 px from y = 250); the existing clamp still shrinks it on a small window so its three
button rows clear the status bar.

| # | Check | Expected |
|---|-------|----------|
| 3.21.1 | `[ ]` Pop the scope out with a template loaded | Full-width graticule in a bezel, six knobs, and a status plate reading TRIG / MODE / VIEW / RUN with the channel count |
| 3.21.2 | `[ ]` Drag VOLTS/DIV and TIME/DIV up and down | Detented: each ~14 px of drag steps one 1-2-5 position, and the docked V+/V-/T+/T- buttons show the same value. Time/div still re-maps the simulation step |
| 3.21.3 | `[ ]` Drag POSITION, then switch CHANNEL and drag again | Each channel moves independently; the value under the knob is that channel's offset in volts |
| 3.21.4 | `[ ]` Drag TRIG LEVEL | The trigger line on the screen follows it, and the reading at the bottom of the window agrees |
| 3.21.5 | `[ ]` Drag INTENSITY | Both windows dim together - it is the same setting as the status bar's `Brt` slider, and it persists to settings.json |
| 3.21.6 | `[ ]` Click on the graticule, then on the button rows | Neither grabs a knob; the buttons keep working exactly as they do docked |
| 3.21.7 | `[ ]` Dock the scope again and click where a knob was | Nothing happens - the knobs are only live while the panel is shown |
| 3.21.8 | `[ ]` **Automated:** `circuit-playground --layout-test` | New section: all six knobs inside the panel column, each hit-tests at its own centre, none overlap, a click on the screen grabs none, they are inert when docked, the two detented knobs emit the same UI actions the buttons do, and the three continuous knobs move their value in the right direction |
| 3.21.9 | `[ ]` **Automated:** `--popout` + `--shot` | A scripted run with `--popout` writes both windows: `name.bmp` and `name_scope.bmp`. `tools/make_media.py` uses it for the two README screenshots |

### 3.22 Named parts, the rest of the op-amp, and ceramic DC bias (2026-08-28: templates #159-#161)

| # | Check | Expected |
|---|-------|----------|
| 3.22.1 | `[ ]` **Automated:** `template_smoke --part-test` | Every named device at its own data sheet condition: R_DS(on) at the stated V_GS and I_D(on) off the transfer curve, h_FE and V_BE at a forced base current, V_F at a forced forward current, V_Z at I_ZT, an op-amp's offset out of a unity buffer and its slew rate off a 5 V step, and each regulator in its reference circuit. **23 checks over 20 devices, 0 failed** |
| 3.22.2 | `[ ]` Select a transistor and click the **Part** row | It cycles the devices that fit that symbol and then back to generic; the canvas symbol is labelled with the part number and the panel shows the data sheet line the parameters came from |
| 3.22.3 | `[ ]` Named Parts template: read the three drops | 143 mV (2N7000), 233 mV (2N7002), 5 mV (IRF540N) - each is 12 V x R_DS(on) / (100 + R_DS(on)) |
| 3.22.4 | `[ ]` Named Parts: close the switch | Twice the current, twice the drop across the 2N7000 (143 -> 285 mV) and four times the heat in it |
| 3.22.5 | `[ ]` Op-Amp Error Sources: both outputs, then close the switch | 0.000 V ideal, -0.89 V real; closing it matches R_s to R1||Rf and the bias errors cancel, leaving +0.10 V of offset |
| 3.22.6 | `[ ]` Ceramic DC Bias: the three ripples | 62, 125 and 219 mVpp on the same 25 mA - the capacitance is halved at 2 V and down to 2.9 uF at 5 V. Set **Bias 1/2** to 0 for a class-I part and all three become equal |
| 3.22.7 | `[ ]` **Model:** the op-amp's remaining properties | Input offset voltage, bias current, CMRR, differential input resistance and output resistance all stamp, and a part that is not rail-to-rail keeps 1.5 V of headroom. These apply at the operating point as well as in the transient - only the GBW pole and the slew limit are transient-only |
| 3.22.8 | `[ ]` **Model:** MOSFET Newton limiting | A large-K device could be thrown by one linear solve to a V_DS hundreds of volts from anything reachable, and the solve settled where no KCL held (a 2N7000 switching 100 ohm read -42 V, reported as converged). The linearisation point is now bounded per iteration. V_GS is deliberately not limited - holding it back stalls the device in cutoff while the node voltages sit still, which the convergence test reads as success |
| 3.22.9 | `[ ]` **Model:** capacitor initial conditions | Five converter templates set a starting voltage that was never read. At the operating point such a capacitor is stamped as a stiff source of that value so the nodes agree with it; their settled outputs are unchanged and only the startup is shorter |
| 3.22.10 | `[ ]` **Automated:** `--layout-test` palette count | It caught the circuit palette silently dropping a template once the library passed its 160-item capacity. 161 templates, 161 palette items |
| 3.22.11 | `[ ]` **Automated:** the full battery | 161/161 templates, 161/161 demos, **167/167 probe oracles**, 23/23 part checks, 25/25 switches, 0 burns, 2158 knob runs, 0 param failures, 0 layout failures |

### 3.23 The operating point the panel shows, and what a value edit does to a running simulation (2026-08-28)

Reported: "the V_GS cutoff and I_D don't get modified, or at least it displays 0 regardless of
which model I pick". Three separate faults were behind it, and the automated check below is the
one that would have caught it.

| # | Check | Expected |
|---|-------|----------|
| 3.23.1 | `[ ]` **Automated:** `template_smoke --op-test` | The operating point cached for the panel, per device: region, V_GS, V_DS, I_D and g_m at a stated gate voltage. **6 checks, 0 failed.** 2N7000 at V_GS 4.5 V is in saturation at 361 mA; the same part at 10 V is pulled into **triode** by 3.6 A through the sense resistor; the 2N7002 gives 301 mA at the same drive; an IRF540N gives 7.7 A; and below V_GS(th) the device reads cutoff, 0 A, with V_DS at the full rail |
| 3.23.2 | `[ ]` **Bug:** the panel read 0 for every device | Cycling the Part row restored the component's whole property union to defaults - and the operating point lives in that union - so it was blanked. `component_cycle_part` now carries it across. `--op-test` asserts I_D is unchanged by a part change and non-zero after cycling all the way round to generic and back |
| 3.23.3 | `[ ]` **Bug:** nothing refreshed it afterwards | Editing ANY property set `circuit->modified`, which auto-paused a running simulation, so the operating point froze at whatever it was - and after 3.23.2 that was zero. `modified` now means only "unsaved"; the new `topology_dirty` means "a component or wire was added, moved or deleted", and only that pauses the run or forces a restart from t = 0. Turning a knob while it runs now does what a simulator should |
| 3.23.4 | `[ ]` **Bug:** V_DS showed the linearisation point | The MOSFET Newton limiter (3.22.8) kept its working value in `op_vds`, which is also what the panel reads. A device in cutoff showed V_DS 5 V with its drain actually at 10 V, because the solve stops as soon as the nodes stop moving - before the limiter has walked out. The limiter has its own field now and the panel gets the real terminal voltage |
| 3.23.5 | `[ ]` Interactive: select a MOSFET in Named Parts with the simulation running, cycle the Part row | Region, V_GS, V_DS, I_D and g_m all update as each device is applied, and the simulation keeps running |
| 3.23.6 | `[ ]` Interactive: edit a resistor value mid-run | The waveform responds and the run continues from where it was. Adding, moving or deleting a part still pauses with "Circuit changed - simulation paused" |

### 3.24 Subcircuits, and the CI gate (2026-08-28)

| # | Check | Expected |
|---|-------|----------|
| 3.24.1 | `[ ]` **Automated:** `template_smoke --sub-test` | Definitions built the way the Ctrl+G dialog builds them, each placed as a block and driven: resistive divider **4.9975 V** of 5; RC low-pass **9.99 V** of 10 after ten time constants (needs per-instance state); a block with a 3.3 V source inside it **3.2997 V** (needs its own matrix row); and **5.0025 mA** at the IN pin, which is what the flow animation draws. 4 checks, 0 failed |
| 3.24.2 | `[ ]` **Bug:** internal state was shared with the definition | Every stamp copied the components out of the definition, so an internal capacitor never charged (0.03 V after 10 tau) and two blocks of one type shared a template. Each block now owns live copies |
| 3.24.3 | `[ ]` **Bug:** internal sources had no matrix row | An internal voltage source or inductor stamped into a row belonging to something else and read 0 V. `component_aux_count()` now reports what a block needs |
| 3.24.4 | `[ ]` **Bug:** internal node ids were off by one | They were allocated from `num_nodes + num_volt_vars`, but a node id addresses row id-1, so the first internal node sat on the last auxiliary row. Fixed with +1 |
| 3.24.5 | `[ ]` **Bug:** blocks carried no pin current | The terminal-current pass skipped subcircuits, so the flow animation stopped at the block's edge. Stamping the block gives the residual at each pin |
| 3.24.6 | `[ ]` Interactive: build a divider, Ctrl+G, place two blocks on one canvas | Both solve; giving one a different internal value does not change the other |
| 3.24.7 | `[ ]` **CI:** every workflow had been failing since it was added | `configure_file(copy: true)` read SDL2-8.dll from the source tree at configure time - an untracked file that happened to be in my working copy - so every clean checkout died before compiling. The copy now comes from what the SDL2 subproject builds, at build time, and is skipped for static builds |
| 3.24.8 | `[ ]` **CI:** the gate failed on warnings | `--geom-test` returned the count of templates with ANY geometry remark, and `set -e` read 31 tracked cosmetic warnings as 31 failures. Only the two hard rules - overlapping symbols, diagonal wires - set the exit status now |
| 3.24.9 | `[ ]` **CI:** the audit runs the new suites | `--part-test`, `--op-test` and `--sub-test` are in the workflow, so a model regression cannot reach a release |

### 3.25 SPICE .SUBCKT import, and nesting (2026-08-28)

| # | Check | Expected |
|---|-------|----------|
| 3.25.1 | `[ ]` **Automated:** `template_smoke --spice-test` | Value parsing (including MEG = mega against M = milli), a two-subcircuit netlist imported, and the model measured against the hand calculation. **14 checks, 0 failed** |
| 3.25.2 | `[ ]` The imported ceramic across three decades | 100 nF with 30 mOhm ESR and 0.7 nH ESL: **15.91 ohm at 100 kHz** (1/(2 pi f C) = 15.9, still a capacitor), **0.033 ohm at 19.02 MHz** (series resonance - the reactances cancel and only the ESR is left), **0.442 ohm at 100 MHz** (2 pi f L = 0.44, now an inductor). This is the vendor-curve comparison the roadmap asked for |
| 3.25.3 | `[ ]` Two of them in parallel via `X` instances | **7.957 ohm** against 7.96 by hand, which also proves an X instance nests |
| 3.25.4 | `[ ]` `circuit-playground --import-spice <file>` | Prints "imported N subcircuits", or names the first line it had to skip. Each one appears in the subcircuit palette and can be placed |
| 3.25.5 | `[ ]` **Bug:** matrix sizing ignored nested internal nodes | `subcircuit_count_internal_nodes` counted one level, so a nested model's nodes were past the end of the matrix and `matrix_add`'s bounds check dropped them - they behaved like ground. Two capacitor models in parallel measured 0.026 ohm. The count recurses now |
| 3.25.6 | `[ ]` **Bug:** nested capacitors never advanced | The per-step state pass walked only top-level blocks, so a nested capacitor's stored voltage stayed at zero and the part acted as a near short (0.09 ohm). The walk recurses now, and so does the operating-point seeding |
| 3.25.7 | `[ ]` Unsupported lines | A `.MODEL` card, a semiconductor instance or an unparseable value is counted and the first one is named in the summary, rather than being silently dropped or guessed at |

### 3.26 The 555 as a subcircuit (2026-08-28: template #162)

The first built-in part that is a *circuit* rather than a parameter set - and the first real
exercise of the subcircuit engine by a shipped template.

| # | Check | Expected |
|---|-------|----------|
| 3.26.1 | `[ ]` **Automated:** `--osc-test` on 555 Astable | **4818 Hz** against 1.44/((R_A + 2 R_B) C) = 4800 with 10k, 10k and 10 nF - 0.4 % - and a square output (shape 0.472; a symmetric square is 0.500, and this one is 67 % duty by design) |
| 3.26.2 | `[ ]` **Automated:** `--probe-test` | The output swings the rail: amp 2.475 V of a 5 V supply |
| 3.26.3 | `[ ]` Watch the capacitor node | It ramps between 1/3 and 2/3 of the supply - 1.67 V and 3.33 V - which is the divider inside the block doing its job |
| 3.26.4 | `[ ]` Change R_A, R_B or C and re-run | f follows 1.44/((R_A + 2 R_B) C); doubling R_B roughly halves it and moves the duty cycle toward 50 % |
| 3.26.5 | `[ ]` **Bug:** the whole suite segfaulted | `src/circuits.c` includes `<stdio.h>` and `<string.h>` but not `<stdlib.h>`, so the `calloc` added for the definition was implicitly declared returning `int` and the pointer came back sign-extended (0xFFFFFFFF8F3F10B0). MSVC warns C4013 and it scrolls past in a 562-target build. Other C4013 warnings remain in that file for `place_*` functions used before their declarations - harmless (they do return int) but the same hazard |
| 3.26.6 | `[ ]` **Tooling:** `template_smoke` stdout is unbuffered | A crash used to show the last 4 KB of already-passed templates, so the culprit looked 14 places earlier than it was |
| 3.26.7 | `[ ]` **Bug:** two burn warnings on R_A and R_B | The discharge transistor was in ideal mode, where saturation output conductance is 1e-12; the DISCH node was barely defined while the latch flipped and threw a 58 V startup spike. A real output conductance settles it |

### 3.27 Every circuit places itself, named probes, and the scope's mouse (2026-08-29: v3.22.0)

Picking a circuit from the palette had only ever worked for the first hundred: the click's
action code is a base plus an index, circuits were given a hundred codes, and there are 187.
Everything past the hundredth landed in the range that belongs to saved subcircuits and was not
recognised at all, so nothing was cleared and nothing was placed. The same fault had put
UI_ACTION_UPDATE inside the property-edit range, where it is Offset.

| # | Check | Expected |
|---|-------|----------|
| 3.27.1 | `[ ]` Pick I2C Bus, then Button Debounce, then any circuit past the hundredth | Each one clears the last and places itself, framed and running - no click needed |
| 3.27.2 | `[ ]` Click a source's **Offset** property, then **Frequency** | The edit field opens. Before this, Offset ran the updater and Frequency toggled sweep-tracking |
| 3.27.3 | `[ ]` **Automated:** `circuit-playground --place-test` | 476 checks: every circuit recognised from its click and leaving the canvas holding exactly what a clean canvas holds, plus every part, property and plain button checked for overlapping codes |
| 3.27.4 | `[ ]` Autoset the Common Emitter, pick another circuit, come back | Both show their traces every time - a template's preset lands on a scope reset to neutral first |
| 3.27.5 | `[ ]` Read the probes on any circuit | Named for the node they sit on - IN, OUT, GATE, NEUT, SW OUT - on the schematic and as the scope's channel names |
| 3.27.6 | `[ ]` **Automated:** `template_smoke --label-test` | 445 probes over 187 circuits: named, not CHn, inside seven characters, unique within the circuit |
| 3.27.7 | `[ ]` Load LDO vs Switcher | Three probes: the PWM, the linear rail and the switcher's rail. Both halves of the comparison are probed |
| 3.27.8 | `[ ]` Load Buck Converter and look at OUT | The ripple is visible - each channel has its own fitted band. At the old shared 2 V/div it was three hundredths of a division |
| 3.27.9 | `[ ]` **Automated:** `--probe-audit` | The RIPPLE flag: a trace on the screen whose every movement is under a tenth of a division. Nine circuits had it |
| 3.27.10 | `[ ]` Press T+ several times | The trace stays up. The recorder thins what it has to the new sample spacing instead of starting again |
| 3.27.11 | `[ ]` **Automated:** `--span-test` | Four presses of T+ on all 187 circuits leave something to draw; 23 went blank before |
| 3.27.12 | `[ ]` Left-drag on the scope screen | The trigger level lands where you drop it |
| 3.27.13 | `[ ]` Wheel, then shift-wheel over the screen | Volts/div, then time/div |
| 3.27.14 | `[ ]` Middle-drag on the screen | Pans: sideways moves the time window, down moves every channel. It had never run on the docked scope - the press was consumed by the trigger-level drag above the button test |
| 3.27.15 | `[ ]` Click the `OUT` chip under V+/V-, then V+ | Only that channel rescales, from what its band was showing; the rail stays centred in its band. `ALL` hands every channel back to the shared scale |
| 3.27.16 | `[ ]` Pop the scope out and press **KNOBS / SLIDERS** | The six controls redraw as sliders, drag the same way, and the choice survives a restart |
| 3.27.17 | `[ ]` Circuits tab | Group rows are pressable bars; a name too long for the panel is cut with two dots rather than running over the canvas |
| 3.27.18 | `[ ]` **Automated:** `--layout-test` at 1024x600 | The scope's four button rows clear the status bar |

### 3.28 What a trace is drawn from, and what a source's edge is (2026-08-29: v3.22.1)

Crosstalk, Ground Bounce and Termination drew each division from five samples: at 5 ns a
division the scope asks for a 250 ps step and the floor was a nanosecond. The trigger point
could only land in one of five places across a division, which reads as a trace that will not
settle. Chasing it found that the pulse and square sources carried a rise and fall time, the
templates set them, and both stamps stepped instantly - so any circuit that answers to an edge
rate was answering to the solver's step.

| # | Check | Expected |
|---|-------|----------|
| 3.28.1 | `[ ]` Load Crosstalk and look at the edges | A resolved edge and the victim's coupled spike, 25 samples a division. Five before |
| 3.28.2 | `[ ]` Same for Ground Bounce and Termination | The same |
| 3.28.3 | `[ ]` **Automated:** `circuit-playground --trig-test` | 154 repeating waveforms judged, 0 free-running, 33 one-shots skipped. Under the old floor it names exactly those three |
| 3.28.4 | `[ ]` **Automated:** `template_smoke --probe-test` | 208/208. Three oracles are recalibrated against the edges their templates ask for: the discrete buck's 1 us gate ramp, the crosstalk aggressor's 1 ns |
| 3.28.5 | `[ ]` Halve the time step on Ground Bounce and read the bounce | It stays put. Before, every halving doubled it and it never settled |
| 3.28.6 | `[ ]` Load Relaxation Osc | It oscillates. Its 20 us kick has a hundred-second period, so the step was 50 us and the kick fell between two samples - it only ever started because sample zero landed on its edge |
| 3.28.7 | `[ ]` **Automated:** `--flow-test` | 186/187, Pierce exempt: it used to pass because it was not running, and now that it does, the flow display mis-splits microamps on the crystal's net (the arrows, not the solve) |
| 3.28.8 | `[ ]` Run the same `--shot` command twice | The scope area is identical. Scripted runs step a fixed frame and a fixed number of steps, not the wall clock |
| 3.28.9 | `[ ]` **Automated:** `python tools/trace_stability.py build/circuit-playground.exe` | A triggered trace stands still between frames on all three templates |
| 3.28.10 | `[ ]` Phase Shift Osc: read the output shape | Lightly clipped, and the audit says so: gain 33 against the 29 it needs with nothing holding the amplitude down. A diode limiter across Rf is the fix, not yet made |

### 3.29 Rows that cannot be applied, and parts no template contains (2026-08-29: v3.22.2)

Two audits of paths nothing had ever run. The properties panel builds its rows while drawing
them and the app applies them in a switch somewhere else, and nothing compared the two lists.
--file-test round-trips every template, which covers the 52 component types that appear in one;
the other 74 are saved and loaded by code nobody exercises.

| # | Check | Expected |
|---|-------|----------|
| 3.29.1 | `[ ]` Select a Schottky diode and type a forward voltage | It sticks. Before, the field showed the right number, took the typing and answered "Invalid value" - the panel reuses the LED's row and only an LED could be written back |
| 3.29.2 | `[ ]` **Automated:** `circuit-playground --prop-test` | 86 typed rows and 50 toggles over 35 parts, 0 that the panel offers and the app cannot apply |
| 3.29.3 | `[ ]` **Automated:** `template_smoke --parts-file-test` | All 124 component types on one canvas, each with a rotation and a non-default value, through both formats |
| 3.29.4 | `[ ]` **Bug:** an out-of-range table index crashed the app | An arbitrary source's properties start with an index into four tables and the lookup read the array before checking it - "idx < 2" is true of every negative number. The index comes from a saved file. The lookup checks its argument now, and loading clamps it |
| 3.29.5 | `[ ]` Place an Arbitrary source, save, reload | It plays a table; a file carrying a bad index loads with table 0 rather than taking the app down |
| 3.29.6 | `[ ]` **Automated:** the whole battery | `bash tools/run_audits.sh` - 34 suites, several at a time, about four minutes |

### 3.30 Undo that covers everything, Select All, and a diode that has a capacitance (2026-08-30: v3.22.3)

Ctrl+Z could take back an add and a move. Everything else a person does to a circuit went
unrecorded: deleting anything at all, typing a value, cycling a part number, toggling a model,
rotating, pasting, duplicating, placing or removing a probe, clearing the canvas, and picking a
circuit from the palette - which replaces everything on it.

| # | Check | Expected |
|---|-------|----------|
| 3.30.1 | `[ ]` Delete a part with the tool, press Ctrl+Z | It comes back, on the same nets |
| 3.30.2 | `[ ]` Delete a wire, press Ctrl+Z | It comes back joining the same two nodes; the circuit settles where it did |
| 3.30.3 | `[ ]` Select a few parts, press Delete, press Ctrl+Z once | All of them come back on one press, not one per part |
| 3.30.4 | `[ ]` Type a value into the properties panel, press Ctrl+Z | The old value returns |
| 3.30.5 | `[ ]` Rotate with Ctrl+R, press Ctrl+Z | It rotates back and stays wired to what it was wired to |
| 3.30.6 | `[ ]` Place a probe, press Ctrl+Z; delete one, press Ctrl+Z | Both come back, with the name they had |
| 3.30.7 | `[ ]` Pick a circuit from the palette, press Ctrl+Z | The circuit that was on the canvas returns, and the scope settles for it rather than keeping the new one's time base |
| 3.30.8 | `[ ]` Ctrl+A then Delete | Everything goes. Before this, Ctrl+A marked parts as selected and nothing acted on the mark: Delete removed nothing and a drag moved one part |
| 3.30.9 | `[ ]` **Automated:** `template_smoke --undo-test` | 2426 edits over 187 templates across thirteen kinds of edit, each undone and redone and compared |
| 3.30.10 | `[ ]` **Automated:** `python tools/keys_gui.py build/circuit-playground.exe` | Ten shortcuts driven through the real event loop and checked against the app's own account of itself |
| 3.30.11 | `[ ]` **Automated:** `python tools/undo_gui.py build/circuit-playground.exe` | A part deleted with the tool comes back on Ctrl+Z - the only check that goes through the tool, the key and the drawing |
| 3.30.12 | `[ ]` Load Discrete Buck, Node by Node and watch the current arrows | They close. The 15 uA that does not appear in any terminal is 1 ppm of the 3 A the switch node carries - Newton slack in a cancellation, not missing displacement current, and the flow test now sizes its tolerance by the net rather than by the node id |
| 3.30.13 | `[ ]` Read the Function Generator's output | Unchanged, at 0.324 rms/peak-to-peak. A picofarad has megohms of reactance at 5 kHz and cannot round the shaper's corners. v3.22.3 shipped this as 0.302 with an explanation attached; that was an inverted sign in the junction-capacitance stamp, not physics (docs/ROADMAP.md) |
| 3.30.14 | `[ ]` **Automated:** `--flow-test` | 187/187 with the buck audited rather than exempt; two exemptions left, both recorded in docs/ROADMAP.md |
| 3.30.15 | `[ ]` **Automated:** the whole battery | `bash tools/run_audits.sh` - 35 suites plus three that drive the app, about four and a half minutes |

### 3.31 A trace that stands still, a title that can be read, and a stamp with the right sign (2026-08-30)

Three faults found from one report each, and in every case the report was one instance of a class
the audits could not see at all.

| # | Check | Expected |
|---|-------|----------|
| 3.31.1 | `[ ]` Load Common Emitter, let it settle, watch the trace for ten seconds | It does not move. It used to shimmer a few pixels vertically: an AC-coupled or fitted channel centred on the mean of the captured window, and the capture is a ragged fraction of a cycle whose length changes every frame |
| 3.31.2 | `[ ]` **Automated:** `--bounce-test` | Two frames whose capture has the same envelope and the same cycle count must put the zero line in the same place. 187 templates, 0 failing; with the fix reverted, 16 fail |
| 3.31.3 | `[ ]` Load Common Emitter and read the title | Above the circuit, not across the Vcc rail |
| 3.31.4 | `[ ]` **Automated:** `--geom-test` | Text over a symbol, over a wire, or over other text is now a hard failure, not a warning. Text-over-wire was never checked, so a title on a supply rail read as clean; it was on 7 templates, all fixed |
| 3.31.5 | `[ ]` **Automated:** `--dvdt-test` | A capacitor, an electrolytic, an inductor and both junction capacitances against `C dv/dt` computed outside the solver. All within 0.1 %; with the shipped sign inverted, the junction cases read -159555 % |
| 3.31.6 | `[ ]` Read the Function Generator's output | 0.324 on the rms/peak-to-peak measure, where it has always been. v3.22.3 shipped 0.302 with an explanation attached; the explanation was wrong and so was the sign behind it |
| 3.31.7 | `[ ]` **Automated:** `--flow-test` | 187/187 with the buck audited on its merits. Its 15 uA is 1 ppm of the 3 A its switch node carries, and the tolerance now scales with the net rather than with one node id |
| 3.31.8 | `[ ]` **Automated:** `--class-test` | Every template measured for what it is: 29 static, 146 periodic, 4 one-shot, 8 stepped, each with its real period and settling time. Run again at a finer step, 5 answer differently - the step is the answer rather than the circuit, listed with numbers in docs/ROADMAP.md |
| 3.31.9 | `[ ]` **Automated:** the whole battery | `bash tools/run_audits.sh` - 38 suites |

Notes for whoever reads this next:

- **A conservation check cannot see a sign error.** Terminal currents are recovered by re-stamping
  each device alone and reading its residual, so the report always agrees with the stamp. Every
  KCL check in the suite passed over an inverted junction capacitance. Only an oracle computed
  outside the solver catches it, which is what 3.31.5 is.
- **The Pierce oscillator's last 1.13 px was not a fault.** Its output is a hard +/-15 V square at
  about 50 samples a period, and averaging over whole cycles reconstructs it with straight lines
  between samples, which across an edge is wrong by up to half a sample: 15/249 V, exactly the
  1.13 px observed. `--bounce-test` subtracts that floor rather than tolerating it by a round
  number, so it stays sharp - with the bug reverted it still catches 16 templates.

### 3.32 Every node of every template, with nothing excused (2026-08-30)

`--flow-test` carried two exemptions and skipped every MOSFET gate node. All three were written up
as limitations of the current-flow display. All three were bugs in how a companion is read.

| # | Check | Expected |
|---|-------|----------|
| 3.32.1 | `[ ]` **Automated:** `--flow-test` | 187/187, no exemption and no skipped node. It used to exempt Pierce and Pull-up Sizing and skip gate nodes |
| 3.32.2 | `[ ]` **Automated:** `--dvdt-test` | Six elements against `C dv/dt` computed outside the solver, all within 0.1 %. The MOSFET gate case reads 1.25658e-05 A against 1.25664e-05 |
| 3.32.3 | `[ ]` Load Pierce, Node by Node, watch the arrows | They close. The 6.8 uA was the crystal being re-stamped with the next step's companion state when its terminal currents were read back |
| 3.32.4 | `[ ]` Load Pull-up Sizing, Node by Node | They close. The 3.6 % was the MOSFET gate capacitance advancing its state once per Newton iteration |
| 3.32.5 | `[ ]` Put a non-ideal MOSFET on a DC-biased gate and watch the gate current | Microamps of displacement current, not milliamps. It used to draw about 4 G V_bias for ever - 24 mA on a 2 pF gate at 3 V |
| 3.32.6 | `[ ]` Read the MOSFET Tuned Amplifier's gain | About 1.42 on screen, 1.495 converged. The expectation was 1.2458 until this was fixed, which was a record of the broken model rather than of the circuit |
| 3.32.7 | `[ ]` **Automated:** `--bounce-test` | 0 of 187. With the centring reverted it catches 16 |
| 3.32.8 | `[ ]` **Automated:** the whole battery | `bash tools/run_audits.sh` - 41 suites |

Two notes for whoever adds the next audit:

- **An exemption is a place a bug can hide.** Every exemption this suite ever carried turned out to
  be a fault rather than a limitation. If one is unavoidable, it should say what would settle it.
- **The scope's centring is the midpoint of the extremes, deliberately.** The mean of the captured
  window bounces because the window is a ragged fraction of a cycle; averaging over whole cycles
  fixes that but steps by the difference whenever the window drifts in and out of holding a second
  crossing. Both were measured, both moved the trace, and the reasoning is written where the
  function is.

### 3.33 The screenshots were looked at (2026-08-30)

Ten templates rendered at 1400x900 and actually read, which is how every one of these was found -
none of them is visible to a pixel-diff or to the geometry audit, because each is either off the
canvas, in the panel rather than the schematic, or a number that is technically true.

| # | Check | Expected |
|---|-------|----------|
| 3.33.1 | `[ ]` `--template "Digital Clock"` | Places the clock: a unique piece of a name is a match. An unmatched or ambiguous name exits 2 with candidates listed - it used to carry on and screenshot an empty canvas with exit 0 |
| 3.33.2 | `[ ]` Load MOSFET Output Curves, read the scope's voltage row | Four channels with space between them, wrapping to a second line if the row fills. It read "OUT:0.18VDEV2:17.6mDEV3:80.5mV" - a fixed 80 px pitch worked until a channel was named SW OUT or there were four |
| 3.33.3 | `[ ]` Load Crosstalk, read the toolbar | dt:200ps. A 200 ps step used to display as "dt:0ns" |
| 3.33.4 | `[ ]` Load Common Emitter / Pierce | The source's "1000Hz" and the rightmost "4.7nF" inside the canvas: the fit-on-place margin now leaves room for value labels, which its bounding boxes never included |
| 3.33.5 | `[ ]` Load Function Generator | The four bias sources' voltage labels each beside their own symbol; at 60 px columns a label was drawn across the neighbouring source |
| 3.33.6 | `[ ]` **Automated:** `--geom-test` | Still 0 hard violations after the spacing change |
| 3.33.7 | `[ ]` Load LDO vs Switcher, read the channel chips | [LDO] and [SW OUT] as separate chips. They are sized from the channel names, which used to happen only at window-resize time - before a template's probes exist - so a chip sized for "CH2" was drawn holding "SW OUT" and the row read "LDOSW OUT". The chips re-lay themselves when a name changes |
| 3.33.9 | `[ ]` **Automated, before a release:** `python tools/edge_gui.py` | Renders all 187 templates and checks the canvas border for cut-off content. Too slow for the battery (~8 min), so it is the pre-release visual gate. Its first run found a clip on the Facility Rating template that three rounds of hand-read screenshots had missed |
| 3.33.8 | `[ ]` **Automated:** `keys_gui.py` / `undo_gui.py` | Both aim their clicks from `--state-out`, which now lists every part's screen position through the same transform it is drawn with. They used to click "300,300 is the resistor", which held until the fit margin moved every template by 40 px and five checks failed at once |

Known and left alone, recorded so it is a decision rather than an oversight: a high-power load's
power label is the instantaneous v^2/R at the drawn frame, so two identical AC loads can read
"909 kW" and "2.86 MW" if the frame lands near one's zero crossing. The field feeds the overload
colouring and a burn-in audit that both want the instantaneous value; smoothing only the label
needs display-side state that does not exist yet.

### 3.34 The numbers on the measurements panel (2026-08-30)

Vpp, Vavg, Vrms, f, T and duty are what a user reads off the scope, and nothing verified them: the
probe suites check waveforms straight from the history, never the derived numbers the panel shows.

| # | Check | Expected |
|---|-------|----------|
| 3.34.1 | `[ ]` **Automated:** `--meas-test` | Synthetic sine, square and triangle built from known parameters, fed straight into `analysis_measure_waveform`, judged against closed forms. 35 checks, 0 failing |
| 3.34.2 | `[ ]` Load any 50 % waveform and read D on the panel | 50 %, not 49 %. The duty computation counted an interval only when both its samples were high, dropping half an interval at every crossing - one sample per cycle - and every screenshot ever taken showed "D:49%" against waveforms that are 50 % by construction |
| 3.34.3 | `[ ]` The ragged-window rows of `--meas-test` | Within 5 %. A window of 8.37 cycles wobbles its average by up to 4.5 % - that is the sliding-capture effect the scope's centring stopped using the window mean for. The panel still shows the window mean, and these rows are what bound how far it can be off |
| 3.34.6 | `[ ]` **Automated:** `bash tools/run_audits.sh` with a suite removed from its list | Exit 2, naming the suite. `--scope-test` was in no list, so nothing ran it, and its expectation still said MIN_TIME_STEP was 1 ns long after the floor became 10 ps - it sat failing where no one would see. A suite in no list is a hole that looks like coverage |
| 3.34.5 | `[ ]` **Automated:** `python tools/svg_audit.py` | Exports all 187 templates to SVG and opens each with a strict XML parser: well-formed, an svg root with a viewBox, enough elements to be a picture, no nan/inf coordinate. Its first run found six templates exporting **zero bytes, silently**: the export filename sanitizer let a colon through, and on NTFS a colon starts an alternate data stream - fopen succeeds, the bytes vanish into an invisible stream, and the visible file is empty. Every template whose name contains ":" was affected |
| 3.34.4 | `[ ]` **Automated:** `--fft-test` | The spectrum view and THD against known spectra: a bin-exact sine, square and triangle through the real FFT with the rectangular window. The square's 3rd harmonic sits at -9.54 dB and its THD over harmonics 2..10 at 42.88 %; the triangle at -19.08 dB and 12.05 %; a pure sine has no 3rd harmonic above -80 dB and clears 60 dB of SNR. 10 checks, 0 failing - this is also what stands behind the Function Generator's "3rd harmonic > 30 dB down" claim |

### 3.35 v3.23.0 - five components read their own state at the wrong moment (2026-08-30)

The release is mostly one fault found five times, and the checks that can see it. A companion -
the little source that carries a storage element's memory from one step to the next - belongs to a
time step. A stamp runs many times per step: once per Newton iteration, and once more whenever the
current-flow display reads a terminal current back. Anything advanced or read inside one is being
advanced or read at a rate that has nothing to do with the clock.

| # | Check | Expected |
|---|-------|----------|
| 3.35.1 | `[ ]` **Automated:** `--dvdt-test` | Eight storage elements against arithmetic done outside the solver. It found the diode's junction capacitance stamped with its sign inverted, a MOSFET gate drawing 24 mA where 12.6 uA was due, a DC motor reaching 63 % of speed in one time step instead of 99 ms, and a relay coil at 10.5 us instead of 500 us |
| 3.35.2 | `[ ]` **Automated:** `--restamp-test` | Reading a circuit's currents must not change the circuit. 188 templates and 122 component types, each run twice with the display on and off. The relay failed it. It detects a stamp that *writes* while being read, and nothing else - a stamp that misreads agrees with its own second run |
| 3.35.3 | `[ ]` **Automated:** `--flow-test` | 188/188 with **no exemptions and no skipped nodes**. It carried two exemptions and skipped every MOSFET gate this morning; all three were bugs rather than limitations |
| 3.35.4 | `[ ]` **Automated:** `--meas-test` / `--fft-test` | The measurements panel and the spectrum against closed forms. Found the duty-cycle bias that made every 50 % waveform read "D:49%" on every screenshot ever taken |
| 3.35.5 | `[ ]` **Automated:** `--class-test` | Every template measured for what it is, then asked again at a finer step. Three self-oscillators were running 10 % fast because their step came from the display rule; the simulation now measures its own period and refines |
| 3.35.6 | `[ ]` **Automated:** `--bounce-test` | A settled trace holds its vertical position. 14 templates were shimmering; the scope centred on the mean of a ragged capture window |
| 3.35.7 | `[ ]` Load **CCM vs DCM** (new) | Two identical 12 V bucks at 50 % duty: 6 ohm settles at 5.44 V, 30 ohm at 8.37 V. The roadmap called this unbuildable for two days; the blocker did not reproduce in nine measured configurations |
| 3.35.8 | `[ ]` Select a transformer, a DC motor, a relay, a VCVS, a potentiometer | All of them have a properties panel now. 53 of 124 types offer rows against 35, and a VCVS's gain - the entire point of the part - could not be set by any means before |
| 3.35.9 | `[ ]` Run a heavy circuit and watch the speed readout | Amber when the stepper cannot keep up. It showed the request as though it were the fact |
| 3.35.10 | `[ ]` **Automated:** `python tools/svg_audit.py` | All 188 SVG exports parse. Six were writing zero bytes: a colon in a filename opens an NTFS alternate data stream |
| 3.35.11 | `[ ]` **Automated:** `python tools/edge_gui.py` | Nothing a template draws is cut off at the canvas edge |
| 3.35.12a | `[ ]` **Automated:** `--state-test` | A battery is still full after its own DC operating point. It was not: the coulomb count ran inside the stamp, the operating point stamps with a 1e9 pseudo-step, and every Run began with a flat battery reading 0.72 V instead of 1.5 V |
| 3.35.12b | `[ ]` **Automated:** `--sub-test` case 3 | A subcircuit that is one capacitor draws C dv/dt through its pin. The solve-time snapshot did not reach inside blocks, so a charged internal capacitor was read back as empty - 94x wrong, and a regression introduced by the snapshot itself |
| 3.35.13 | `[ ]` Probe a node sitting on a rail (a 5 V logic output) and open the spectrum | The fundamental is the signal's, not one bin. The FFT never removed DC, and a Hann window smears a DC term into bins 0 and +/-1 - so the search that starts at k=1 to skip DC landed on the leakage, and THD and SNR were computed against it |
| 3.35.14 | `[ ]` Watch the spectrum while the trace changes | It follows. The transform read `values[0..N)` of a history held oldest-first, so the spectrum belonged to a second ago rather than to the trace beside it |
| 3.35.15 | `[ ]` Turn on AC coupling or Fit, then drag a cursor | The readout names the volts the trace is actually drawn at. The overlay inverted the manual offset and an unfitted scale, which is wrong under either - and both are the default for a stacked template |
| 3.35.16 | `[ ]` Place a relay, Run, and read its coil before the first step | Energised if the DC solve says so. The advance that maintains the coil runs only on an accepted transient step, so the operating point left it at zero - contradicting its own solve, and the initial condition the transient started from |
| 3.35.17 | `[ ]` Type a junction capacitance into a Schottky | It sets the capacitance. `PROP_CJO` wrote `props.diode.cjo`, which in that union is `props.schottky.ideal` - so it silently toggled the model instead |
| 3.35.18 | `[ ]` Open the value row on a potentiometer, a battery, a transformer | The box opens with the current value in it. All three gained the row this release and were missed in the pre-fill, so Enter on an untouched field parsed `""` |
| 3.35.19 | `[ ]` Look for a row that does nothing | There are none. Five were inert: the centre-tapped transformer's model toggle and winding resistances, primary inductance and coupling, a light level with no reader, `CS_RIN` on sources that have no input resistance, and two Ideal toggles |
| 3.35.20 | `[ ]` **Automated:** the step budget | A double reaching 1e14 cast to a 32-bit `long` is undefined and lands on INT_MIN, which the `< 1` clamp turned into one step per frame - while the keeping-up indicator, comparing progress against that same target of 1, reported a confident 100 % |
| 3.35.21 | `[ ]` **Automated:** the fuse | i2t is integration state, so accumulating it inside the stamp multiplied the blow energy by the Newton iteration count - exactly 2x on a DC circuit with the current-flow display on |
| 3.35.22 | `[ ]` Reset with a relay or a DC motor placed | Their state clears. `simulation_reset` had a case for neither |
| 3.35.12 | `[ ]` **Automated:** the battery | `bash tools/run_audits.sh` - 48 suites, and it refuses to start if a suite exists in no list |

3.35.13 through 3.35.22 are the tail of a pre-release review: five independent readers, each finding
verified by a reader whose job was to refute it, **23 survived and all 23 are fixed.** No suite saw
any of them. Two are written up at length in `docs/ROADMAP.md`; the rest are above.

The thing worth carrying forward: **a check passing is not evidence the thing it checks is right.**
Conservation checks cannot see a stamp's sign, because a terminal current is recovered from the
same stamp that produced it. A suite that picks its own time step is testing a different program.
An exemption is a place a bug can hide - every one this project carried turned out to be a fault.
The suites added here are the ones that answer to arithmetic done outside the solver.

### 3.36 Interview prep: the nineteen templates, against their own numbers (2026-08-30)

These are the templates someone opens to check an answer before an interview, so a number printed
beside the circuit that the solver does not reproduce is wrong in the one place a person is going to
repeat it out loud. `--iv-test` takes 26 such numbers off those nineteen canvases and measures each
twice - at the step the app itself would use, and again eight times finer.

| # | Check | Expected |
|---|-------|----------|
| 3.36.1 | `[ ]` **Automated:** `--iv-test` | 26 documented numbers, 0 the solver fails to reproduce. Kelvin's 110 mV against 10 mV, Crosstalk's 3.3 x 2/7, the 2N3904's 0.07 V beside the 2N7000's 0.41 V, Bootstrap's 23.5 V, the two-capacitor problem at exactly 5.000 V both sides |
| 3.36.2 | `[ ]` **Automated:** the same suite at dt/8 | A number quoted to a person has to be a property of the circuit, not of how finely it was integrated. **26 of 26 hold.** Hot-Plug Inrush moved 8.2 % until the step rule learned to look at a circuit's own RC and not only at its sources; with the rule disabled it reads 11.33 V against the converged 10.02 |
| 3.36.3 | `[ ]` Load **4-Wire (Kelvin) Sensing** and look at CH1 | 110 mV, the two-wire reading. It was parked on the current source's grounded return - a permanent 0 V - so the comparison the template exists for was never on the scope |
| 3.36.4 | `[ ]` Load **The Two-Capacitor Problem** and **Hot-Plug Inrush** | Both now probe the slow copy as well. They drew two flat lines each: the probed copy finishes in 100 us and 50 us respectively, on a 50 ms screen |
| 3.36.5 | `[ ]` Load **ESD Clamp Diodes** | The pin clamps at 3.85 V through 1 k, 3.70 V through 220 k, and both are on the scope. The notes said 4.0 V and 2.7 mA - which is (6-3.3)/1k, the current if the pin sat *at* the rail, in the sentence saying it does not |
| 3.36.6 | `[ ]` Read the **Miller** palette line | 130 pF. It said 110 pF while the notes and the on-canvas label both said 130, which is what a gain of 12 gives |
| 3.36.7 | `[ ]` **Automated:** the battery | `bash tools/run_audits.sh` - 48 suites |

The thing worth carrying forward: **a template's annotation is an assertion, and nothing was
testing the assertions.** Every suite in the battery checked that these circuits run, converge,
conserve charge and lay out cleanly. None of them read what the circuit claims about itself.

### 3.37 v3.23.1 - the interview templates, and the step rule behind one of them (2026-08-31)

| # | Check | Expected |
|---|-------|----------|
| 3.37.1 | `[ ]` **Automated:** `--iv-test` | 26 of 26 documented numbers reproduce and **none moves at dt/8**, with no exemptions. Inrush moved 8.2 % until the step rule learned to look at a circuit's own RC |
| 3.37.2 | `[ ]` **Automated:** the step rule | `simulation_accuracy_time_step` took the step from source periods and pulse widths only. It now also takes ten samples across the fastest RC - but only where no source sets the pace, only where the circuit has not measured its own period, and never for a circuit with an inductor, a crystal or a transformer. Each of those three conditions is a bug it caused without them, written up in `docs/ROADMAP.md` |
| 3.37.3 | `[ ]` Load **Hot-Plug Inrush** | The rail sags to 10.0 V, not 11.3: 200 A through the supply's own 10 mohm is 2 V, and the coarse step was hiding it. The annotation said 12/0.05 = 240 A and forgot that same 10 mohm |
| 3.37.4 | `[ ]` Load **The Two-Capacitor Problem** and **Hot-Plug Inrush** | The scope catches the event and holds it. Both are one-shots that were left on AUTO, so the scope free-ran over a settled circuit and drew flat lines |
| 3.37.5 | `[ ]` Load any interview template and count the traces | Every one of the nineteen puts its own comparison on the scope. Eleven were showing one answer of the two or three they are about |
| 3.37.6 | `[ ]` Load **Termination** | Three bands: 4.55 V open, 3.28 V series, 2.45 V shunt. And the band labels sit inside the graticule - the tag was placed at a fixed offset that fitted "IN" and clipped "SERIES" to "SERI" |
| 3.37.7 | `[ ]` **Automated:** `--class-test` | The Relaxation Oscillator reads periodic at 2.2478 ms, which is the 445 Hz `--osc-test` had been measuring all along. It read "one-shot" because the class horizon gave it 8 ms and it needs 10 to start |
| 3.37.8 | `[ ]` **Automated:** the battery | `bash tools/run_audits.sh` - 48 suites, 0 failed, ~205 s |

The thing worth carrying forward: **three of the faults in this release were a fixed pixel offset
standing in for a measured width** - the toolbar controls off the right edge of the window, the
scope's readout row sliced by the status bar, and the channel tag clipped at the graticule. The
fourth was a fixed *time* offset doing the same thing: a step chosen from what the sources do,
standing in for what the circuit does.

### 3.43 v3.28.0 - the drawing is a document, and a screenshot is of the drawing (2026-09-03)

There was one look and one screenshot. The look was synthwave, which is the right look for the
program and the wrong one for a filter you are pasting into a report; the screenshot was the
whole window, so what came out was a picture of the program with a circuit somewhere inside it.

**Schematic view** is the same drawing in black on white - a printed document rather than a
screenshot of software - and **screenshots now have a shape**: the canvas, the canvas with the
scope under it, or the whole window.

| # | Check | Expected |
|---|-------|----------|
| 3.43.1 | `[ ]` Click **BW** on the toolbar | the canvas turns to black on white; the toolbar, palette and properties panel keep their own colours |
| 3.43.2 | `[ ]` In schematic, read the grid | a faint ruling you can align to and see straight through, never as heavy as a wire |
| 3.43.3 | `[ ]` In schematic, look at a probe | a test point, a leader and an open terminal - not a drawing of a plastic probe |
| 3.43.4 | `[ ]` In schematic, read the scope | white screen, light graticule, black and grey traces, and every number on the voltage axis still there |
| 3.43.5 | `[ ]` Two channels in schematic | the traces are different greys; you can still tell the input from the output |
| 3.43.6 | `[ ]` Click **Canvas** | it cycles Canvas -> Cnv+Sc -> Window and back, and the status bar says which |
| 3.43.7 | `[ ]` **Scr** with each of the three | canvas only; canvas with the scope stacked under it; the whole window. All three in whichever style is on screen |
| 3.43.8 | `[ ]` Zoom in on a node dot | a smooth disc, not a staircase - `render_fill_circle` is an antialiased fan |
| 3.43.9 | `[ ]` Look at a probe in synthwave | a slim barrel with a needle, and you can see the node it is pointing at |
| 3.43.10 | `[ ]` `--style schematic --shot-region canvas+scope --shot out.bmp` | the same page without touching the running window |
| 3.43.11 | `[ ]` **Automated:** the battery | `bash tools/run_audits.sh` - 69 suites, 0 failed |

**The style is a property of the document, not of the program.** The first version mapped every
colour in the window and turned the toolbar into a row of empty white boxes: the button faces and
their labels both landed on the same side of the ramp. The mapping is now armed over the canvas
and over the scope's screen, and disarmed for everything else - which is the same region a
canvas-only screenshot crops, so the two features agree by construction rather than by anyone
remembering to keep them in step. `tools/style_wiring.py` fails the battery if an arm is ever
left without its disarm.

**Three faults, and all three were the shape of the map rather than any one colour.**

- *Paper and ink is not enough.* `COLOR_BG` is luma 27 and `COLOR_GRID` is 45 - eighteen apart -
  so a two-way split had to put the ruling on one side or the other, and either way it came out
  as heavy as the wires. There are three roles on a page, not two: paper, ruling, ink.
- *The window is cleared before the style is armed*, so the canvas kept its violet background
  under a schematic grid: a white grid on deep violet, the one combination where the circuit is
  invisible. The canvas is now repainted as paper inside the armed region.
- *The map is not idempotent and cannot be* - it sends dark to light - and `ui_draw_text` reads
  the current colour back and sets it again on the way to the glyph atlas. Every label on the
  scope was mapped to ink and then straight back to paper: an axis with no numbers on it. Only
  the immediate read-then-set round trip is now treated as a restore, because the wide fix -
  always handing back the pre-map colour - breaks the antialiased quads in `render.c`, which read
  the draw colour to fill their vertices and draw with it directly.

**`--style-test` asserts the shape, not the answers.** Palette colours are declared as paper,
ruling or ink and checked against their band; the bands are checked for separation; the map is
checked to be monotone over all 256 luma values; and the read-back-and-set-again round trip is
checked to be an identity on both the draw-colour and the texture-tint paths. It found the
channel inks two apart from their neighbours before anyone looked at a plot.

### 3.42 v3.27.0 - a battery is a chemistry, an arrangement and a C rating (2026-09-02)

A battery was a nominal voltage, a capacity and an internal resistance typed in by hand, and its
discharge curve was `V_nom * (0.85 + 0.15 * SoC)` - the same straight line whatever it was made
of, which is precisely the thing that distinguishes one chemistry from another.

Seven chemistries with their own curves, charge voltages and resistance constants; packs that
derive their voltage, capacity, cutoff and resistance from the arrangement; and a C rating that
is the internal resistance written a second way.

| # | Check | Expected |
|---|-------|----------|
| 3.42.1 | `[ ]` Place a battery, click **Chemistry** | steps through Alkaline, NiMH, NiCd, Li-ion, LiPo, LiFePO4, Lead-acid; the voltage, capacity, cutoff and resistance all move with it |
| 3.42.2 | `[ ]` Set **Cells (S)** from 2 to 3 on a LiPo | 7.4 V becomes 11.1 V. Capacity does not change - series multiplies voltage and nothing else |
| 3.42.3 | `[ ]` Set **Cells (P)** from 1 to 3 | capacity triples and internal resistance divides by three; voltage does not move |
| 3.42.4 | `[ ]` **Battery Packs: 1S to 4S** | 3.7 / 7.4 / 11.1 / 14.8 V nominal, every column 2200 mAh, and 16.8 V on the 4S at full charge because four times 4.2 is 16.8 |
| 3.42.5 | `[ ]` **Battery: 25C vs 100C** | the same 3S 2200 mAh pack twice into 0.3 ohm: the 25C sags to 11.46 V, the 100C holds 12.29 |
| 3.42.6 | `[ ]` **Charging by Chemistry** | 4.20 V a cell for Li-ion, 3.65 for LiFePO4, 2.40 for lead-acid absorption. Each pack half charged and taking current: 100.6 mA, 81 mA |
| 3.42.7 | `[ ]` **Lead-Acid: Bulk, Absorption, Float** | 2.69 A bulk into a flat pack, 1.78 A tapering in absorption, 0.76 A holding at float. (14.4 - 11.64) / 1.0257 = 2.69 |
| 3.42.8 | `[ ]` **Battery Chemistries Compared** | four cells into the same load; the LiFePO4 flat and the alkaline sloping |
| 3.42.9 | `[ ]` **Automated:** the battery | `bash tools/run_audits.sh` - 66 suites, 0 failed |

**`--battery-test` enumerates rather than lists.** The preset table covers the eleven packs
people build with and missed NiCd entirely - it had no preset, so it had no check. The suite now
walks `BATT_CHEMISTRY_COUNT` and asserts what makes a discharge curve a discharge curve: it only
falls as the cell empties (a battery that rises as charge leaves it is a perpetual motion
machine), full is above nominal and empty below, and a rechargeable chemistry charges to at
least its own full voltage. Mutation-checked: bend the NiMH curve and it is caught at the step
where it turns.

**Two suites had to change, and both were reading the same simplification.** A pack under load
is measured against its OPEN-CIRCUIT voltage, and that is no longer its nominal: a full cell
sits above its nominal and a flat one below, which is why a fresh alkaline AA reads 1.6 V.
`--sign-test` set the nominal and computed its expected divider from it, so every battery case
read 6.7 % high and tripped the suite's own "above open circuit" alarm; `--bias-test` set a cell
to "3.00 V", got 3.20, and fell outside the 400 mV protection window it was probing. There is
now one function that says what a pack's terminal voltage is and one that sets a pack to one,
and the stamp and both suites read the same definition.

**The lead-acid charger is three columns, not a handover.** A stage controller switching on a
current threshold is a latch, and the CC-CV handover attempted for v3.24.0 turned out bistable
in the constant-current region and had to be cut. What each stage does is the part worth
learning and three columns cannot sit in a wrong root.

### 3.41 v3.26.0 - sweep every knob, and the audit plan that keeps it swept (2026-09-02)

Asked to sweep every value and knob and find what breaks, to check current direction and
thermal overload, and to make the coverage hold for circuits and components that do not exist
yet. Nine faults, all in shipped code.

**The audit plan, and why it is shaped this way.** Three of the suites here take a *law* or an
*enumeration* rather than a table of expected answers, because a table only ever covers what
was written down the day it was written:

| Suite | Covers | How it stays covered |
|-------|--------|----------------------|
| `--value-sweep` | every part type, every row the properties panel offers, thirteen values from 0 to 1e9 and negative, simulated | walks `COMP_TYPE_COUNT` and asks the panel for its rows, so a new part with a new property is swept the day it is added. A part no template uses gets a bench: supply on terminal 0, every other terminal to ground through a kilohm |
| `--direction-test` | current direction on every template | a law - a passive part cannot generate power, so `I*(V0-V1) >= 0` - which needs no expected value and covers templates not yet drawn. `--sign-test` checks four sources against hand arithmetic and is worth keeping; it does not scale and was never meant to |
| `--thermal-test` | a part over its rating burns, a part inside it does not | both directions, and the rating used is the part's own |
| `tools/thermal_wiring.py` | every part that claims a temperature limit has a power expression the damage model can read | source-level, so a part added with a rating and no handling fails the battery instead of quietly never getting warm |
| `--pair-test` | two circuits on one sheet do not disturb each other | every template, against itself measured alone |
| `tools/click_wiring.py` | every button that is drawn is hit-tested by something | source-level, so a control added and never wired to a click fails the battery. A dead button looks exactly like a working one - it is painted, hovered and labelled. `--layout-test` covers the other way a button becomes unreachable, by being overlapped, at four window sizes |

| # | Check | Expected |
|---|-------|----------|
| 3.41.1 | `[ ]` **Automated:** `--value-sweep` | 125 part types, 132 panel rows, 1215 values applied and simulated, 0 that break the circuit |
| 3.41.2 | `[ ]` **Automated:** `--direction-test` | 200 templates, 702 passive parts, 0 generating power |
| 3.41.3 | `[ ]` **Automated:** `--thermal-test` | 6 overload cases, 0 wrong |
| 3.41.4 | `[ ]` Put a 10 ohm resistor across 10 V | 10 W into a quarter-watt part: it heats, takes damage and releases smoke. Set its rating to 100 W and it survives |
| 3.41.5 | `[ ]` **Automated:** the battery | `bash tools/run_audits.sh` - 65 suites, 0 failed |

**A DC motor's back-EMF was adding to its current instead of opposing it.** The stamp wrote its
equivalent source with the opposite sign to every other companion in the file - the capacitor
two hundred lines up does `b[n0] += +Ieq` for the same `I = G*dV - Ieq` branch, the motor did
`-I_eq`. Back-EMF opposing is the whole of what makes a DC motor regulate its own speed, and
with the sign inverted it is a positive feedback loop: on a bench the current and speed grew by
a factor of 1.889 every step until the node reached 1e15 V. simulation.c recovered the current
with the correct sign all along, so the operating point looked right and only the transient ran
away. No motor template caught it, because every one of them has a periodic source that forces a
short step.

**The motor's rotor was integrated with forward Euler off the previous step's current** while
the armature took its back-EMF from the previous step's speed - a two-step lag in a feedback
loop, which rings. Now semi-implicit: torque from the current this step produced, friction at
the new speed.

**Nothing constrained the time step for a motor.** With no periodic source the step went to the
10 ms ceiling. `J*R/(kt*kv)` is the electromechanical time constant and a tenth of it is the
bound now.

**A JFET with a pinch-off voltage of zero put a NaN through the solve.** Every region of the
square law divides by Vp squared, and the panel took a zero. A device with no channel conducts
nothing, which is finite.

**Nothing in this program had ever burned.** `thermal_update_components` read
`c->thermal.power_dissipated` to decide damage and then wrote that same field at the bottom of
the loop, so the value it judged parts on was computed from itself: zero, for ever. The overload
*warning* worked the whole time because render.c reads `props.resistor.power_dissipated`, which
is computed properly every step - the two were different fields and only one was ever filled in.

**The damage model ignored the part's rating**, using a flat 0.25 W for every resistor. A 5 W
resistor at 1 W drew no warning and accumulated damage anyway.

**Thermal resistance was a fixed 100 C/W** for everything, so a part explicitly rated 100 W
still cooked at 10 W. It now follows the rating, which is what a rating means: `(T_max - T_amb)
/ P_rated` puts a quarter-watt through-hole part at 520 C/W and a 100 W part at 1.3 C/W, and a
part at exactly its rating sits at exactly its limit.

**The electrolytic capacitor claimed a 105 C limit and had no case in the damage model** - the
one part in the library that genuinely dies of heat. Found by the wiring guard above.

**The guard that stops a suite going unrun only matched names ending in `-test`.** So
`--probe-audit`, `--sweep-check` and `--value-sweep` were outside the check that exists to
catch exactly this, and `--sweep-check` was in no list: nobody had run it. It now matches
`-test`, `-audit`, `-check` and `-sweep`.

### 3.40 v3.25.0 - a canvas that scales, a circuit written as text, and the instrument entire (2026-09-02)

The schematic is drawn at the display's own resolution instead of at 900p and stretched, a
circuit can arrive as a parts list rather than as clicks, and the battery instrument from the
KiCad sheet is on the palette whole as well as block by block.

| # | Check | Expected |
|---|-------|----------|
| 3.40.1 | `[ ]` Open any template on a 4K display | Lines and text are sharp, not stretched from 900p. The frame renders supersampled and the UI scales with the display height (`--ss`, `--ui-scale` to override) |
| 3.40.2 | `[ ]` Zoom right in on a wire, then right out | No jagged edges at either end. Wires are feathered quads through `SDL_RenderGeometry`, not stacks of one-pixel lines, so a hairline stays smooth at any zoom |
| 3.40.3 | `[ ]` Read the word "Circuits" in the left panel | "Circuits", not "Circui ts". 46 of the font's glyphs sat off to one side of their cell; every one is centred now |
| 3.40.4 | `[ ]` Open a saved circuit larger than the window | It fits the window. Loading a file zooms to fit instead of leaving the circuit half off-screen |
| 3.40.5 | `[ ]` Name two nodes `vm1` and wire nothing between them | One net. A build table says a part goes to `vm1` and never says where `vm1` is, which is how a written circuit connects up |
| 3.40.6 | `[ ]` Copy a parts list to the clipboard, press **Paste** | The circuit is drawn. Values parse SPICE-style including R-notation, so `4k7` is 4700 and not 4000 |
| 3.40.7 | `[ ]` Double-click a block | Its internals open as a schematic. `Esc` returns |
| 3.40.8 | `[ ]` Place **BMI: Battery Monitoring Instrument** | The whole sheet: sense, protection window, current source, OCP and the switching rail, the same blocks that ship separately |
| 3.40.9 | `[ ]` Select a BJT and read the panel | Its region - cutoff, active, saturation - with Vbe, Vce, Ic, Ib and gm, and a warning when the bias is not where the circuit needs it |
| 3.40.10 | `[ ]` Place a battery | It is visible. Four component types had no renderer at all and drew as nothing |
| 3.40.11 | `[ ]` **Automated:** the battery | `bash tools/run_audits.sh` - 59 suites, 0 failed |

Four faults found this round, all in code that was already shipped.

**A wire lying invisibly on top of another.** Reported as "an invisible wire in the LiPo
charger". Four returns running back along the same row draw as one line with three others
underneath it - 114 of them across 39 templates. `circuit_tidy_collinear_wires` now lays each
run out as the chain it was meant to be, 114 down to 25.

Getting that right took three goes, and the ways it was wrong are worth keeping. It chained
across gaps, which bridges two separate nets. It restarted its scan unconditionally after every
wire rather than only after a rebuild, which is an infinite loop and hung for ten minutes. It
deduped points at half a pixel while nodes merge at five, so a rebuilt run came back unchanged
for ever. And tearing a run down orphans its nodes, which the cleanup sweeps - clearing
`ground_node_id` if one of them is ground and floating the whole circuit.

**Two nets drawn down the same line, chained into one.** The pass above, before it was told
that lying on top of another wire is not connection. Wires join only through shared nodes, and
the CMOS inverter draws its rail and its output overlapping on one vertical; chaining them
shorted the output to VDD and the gate stopped swinging. A run now only absorbs a wire that
already shares a node with it. The five templates still flagged are all cross-net overlaps: a
layout fault to be moved apart, not a bus to be tidied.

**A capacitor built pre-charged solved its operating point at 0 V.** The stamp zeroed companion
current whenever there was no previous solution - but an initial condition is not companion
memory, it is a source of a value the circuit was built with, returned through the same field.
Zeroing it turns a capacitor built at 10 V into a plain 1 mohm resistor across its own
terminals. The Two-Capacitor Problem drew **10 kA** on the wire, since at 1 mohm a volt of node
error is a thousand amps. It survived or not depending on whether Newton had a previous
solution yet, which is why an unrelated second circuit on the sheet changed the answer.

**The DC operating point could contradict the relay it had just moved.** The armature settles
from the solve, and the solve was not redone with the armature where it ended up, so the panel
showed a bias for a contact position the circuit was no longer in.

Two suites were added because the case they cover had never been simulated. Every other suite
places one template on an empty canvas; `--pair-test` places each one beside a second circuit
and requires its own wire currents not to move - at the operating point and again after 125 us
of stepping, both runs pinned to the same time step. Its oracle is outside the solver: the
currents the circuit has alone. It reports how many wires it compared and how many carried
current, because a comparison suite that matched nothing would report a clean zero while
checking nothing at all. 198 templates, 3386 wires, 2590 of them carrying, worst drift zero.
`--ic-test` is the standing check for the fault it found.

### 3.39 v3.24.0 - the circuits of a battery instrument (2026-08-31)

Seven templates in a new **Battery monitoring & electronic load** group, built from a
senior-design BMI: the load that discharges a cell, the two stages that charge it, the thermal
cutout and a supercapacitor standing in for the cell. Each has an arithmetic answer and hits it.

| # | Check | Expected |
|---|-------|----------|
| 3.39.1 | `[ ]` **E-Load: Constant Current Sink** | 3.200 V on a 3.2 ohm sense = 1.000 A, and the cell sags 7.4 -> 7.35 across its own 0.05 ohm, which is the same amp measured a second way |
| 3.39.2 | `[ ]` **E-Load: Constant Resistance** | cell 7.363 V, sense 0.7363 V - divide and it is 10.00 ohm, the value the divider ratio asks for |
| 3.39.3 | `[ ]` **E-Load: Constant Voltage** | the terminal sits at 3.000 V and 4.4 V across 4.4 ohm is 1.000 A. The inputs are the other way round from the sink, on purpose |
| 3.39.4 | `[ ]` **LiPo Charger: CC Stage** | 4.95 V on a 4.5 ohm sense = 1.100 A. The sense is in the return leg so one op-amp reads it against ground |
| 3.39.5 | `[ ]` **LiPo Charger: CV Stage** | 4.201 V into 21 ohm. The divider is 580 and not the textbook 566: this LM317 model puts its 0.1 ohm of output impedance outside the divider, so 200 mA costs 68 mV that never gets corrected |
| 3.39.6 | `[ ]` **BMI: NTC Thermal Cutout** | 13.5 V at 25 C; drag the **Tmp** slider past 35 C and it falls to the negative rail |
| 3.39.7 | `[ ]` **BMI: Supercap Cell Simulator** | rises to 4.167 V with tau 0.167 s and drains with tau 1 s - the asymmetry is the two resistors, and both are on the screen |
| 3.39.8 | `[ ]` **My Circuits -> BMI**, place it and Save, then Open | the same block comes back. It is the original schematic uncorrected, positive feedback and all, and it is not asked to regulate - only that its parts work and that it survives a round trip |
| 3.39.9 | `[ ]` **Automated:** the battery | `bash tools/run_audits.sh` - 54 suites, 0 failed |

Three faults found while building them, all in code that was already shipped:

**An op-amp could report two million volts.** When Newton flip-flops between the two rails, the
stamp falls back to a linear one so it has something to converge on - and a linear stamp has
nothing left to keep the output inside the supplies. Open the feedback loop round a gain of 1e5
and out comes 1e5 times the input difference, on a part running on 15 V. It surfaced as nine
undo/redo failures, because whether the solver landed on that root or on a rail depended on
where it started. The fall-back now lowers the gain just enough to land on the rail.

**The ripple check was firing on round-off.** `--probe-audit` asks whether a trace moves too
little to see, and "moves" was `amp > 0`. A flat DC rail comes back from a solve with its last
bit wobbling - 4.4e-16 on a 3.2 V node, two ULP - so a dead-flat trace read as invisible ripple.
It now wants movement four orders above round-off.

**A wire lying on top of another wire passed the geometry suite.** The first draft of the
current sink returned its feedback along the row the cell's own drain wire uses: collinear,
touching, with no junction to say whether they meet. `--geom-test` counts crossings and wires
through component bodies and had nothing to say about two wires sharing a line.

One template was cut rather than shipped broken. A CC-CV handover - both loops driving one gate
through a diode OR - is bistable in the constant-current region: the voltage amplifier sitting
at its negative rail drags the gate below itself through the diode and latches there, fully on,
with the current amplifier saturated and unable to pull it back. Both roots satisfy the
equations. The two stages ship separately and the note in each says what the join would need.

### 3.38 v3.23.2 - twenty-five faults from an adversarial review (2026-08-31)

Six readers over the source, each with a lens the audit battery does not cover, and a refuter
against every finding whose job was to argue it away. Twelve survived refutation; the rest of what
is here I found or corrected while checking their work, and two of their claims I refuted myself
and did not act on.

| # | Check | Expected |
|---|-------|----------|
| 3.38.1 | `[ ]` Select an AC source, switch on both its sweeps | 19 rows into an array of 16. Row 16's bounds write landed on `num_properties` and `editing_component`, every frame the panel was drawn, and the click handler was clamped where the array was not - so the frequency sweep's Repeat control was drawn and dead. Two clicks reach it |
| 3.38.2 | `[ ]` Open the Bode plot on **RC Low Pass** | Magnitude within 0.03 dB of 1/sqrt(1+(f/fc)^2) and phase within 3.6 degrees of -atan(f/fc). It was wrong five ways at once: the clock ran at 2 dt per solve, the source ignored the frequency being plotted because its own sweep was still on, phase came out +85 where an RC is -85, the sweep restored nothing but the source frequency, and it ran on a thread while the main loop stepped the same Simulation |
| 3.38.3 | `[ ]` Put a battery across a load | Below its open-circuit voltage. A 12 V battery with 1 ohm internal read **18 V** across a 3 ohm load, and a 1.5 V AA read 3.0 - the internal resistance was stamped with the sign that makes it push |
| 3.38.4 | `[ ]` Load a centre-tapped transformer and work the secondary | The primary current follows. It did not move at all - 0.012 A into a 10 k load and 0.012 A into a 10 ohm one - because the part had no current variable and nothing to reflect the load with |
| 3.38.5 | `[ ]` Set an analog switch's on-resistance to zero | It solves. `1.0/R` was stamped unguarded in six places and a zero put a NaN through every node in the circuit |
| 3.38.6 | `[ ]` Save a circuit as JSON and open it | The parts keep their names. The writer emitted every label and the reader ignored it, so every Open renamed every part |
| 3.38.7 | `[ ]` Draw a wire onto another wire | The junction exists and both halves are on it. The junction was created before the old wire was removed, and removing a wire sweeps orphaned nodes - so it was swept, and the halves were wired to nothing |
| 3.38.8 | `[ ]` Open a file with a part selected | No use-after-free. `file_import_json` clears the circuit and frees every part, and the selection went on pointing at one |
| 3.38.9 | `[ ]` Two windows, undo in each | They do not share snapshots. Both wrote `circuit_undo_0.cpg` |
| 3.38.10 | `[ ]` Reset the Pierce oscillator | It starts from nothing. Its motional state lives in the same fields a capacitor uses and only the capacitor cases were cleared |
| 3.38.11 | `[ ]` **Automated:** the battery | `bash tools/run_audits.sh` - 54 suites. `--sign-test`, `--load-test` and `--bode-test` are new here; `--conv-test`, `--stress-test` and `--mc-test` came in just before |

The thing worth carrying forward: **every one of these was verified by putting the bug back and
watching the check fail.** That is not ceremony. Three checks written during this release passed
against the broken code and had to be rewritten - a truncated-file case that could not fail because
a failed read leaves the count untouched, an inconsistency check that asked "is the number large"
when the wrong answer was a perfectly reasonable 5.000 V, and an FFT case using a window in which
the bug does not exist. A check that has never failed has not been shown to be a check.

Two of the review's findings were refuted and are recorded as refuted: the MOSFET
Enhancement/Depletion toggle does flip the sign of Vth, and the report's account of the properties
overflow spraying 90 KB and enabling an arbitrary action was wrong on both counts. The reported
`rows[]` overflow is real and takes more than sixteen aux-needing parts in one subcircuit to reach,
not five.

## 4. Oscilloscope

Setup: AC 1 V 1 kHz + 2nd probe on divider output; square 500 Hz on CH3.

| ID | Test | Expected / Watch-for |
|----|------|----------------------|
| 4.1 | `[ ]` 8 probes, 8 colors, enable/disable each | Legend matches trace colours; disabled channel hides |
| 4.1a | `[ ]` **Stack** button (row 3) with 2–8 probes on the *same* node | Overlay: traces coincide. Stacked: one band per enabled channel with its own zero line, CHn tag in channel colour, 8 divisions per band; disabling a channel re-flows the bands; trigger level line drawn in the trigger channel's band; FFT/X-Y unaffected; pop-out window keeps the toggle |
| 4.2 | `[ ]` V/div 1 mV → 500 kV (table extended 2026-08-24: 200 V, 500 V, 1 kV … 500 kV) | Y labels update; ≥ 1 kV/div labels read `+200kV` style (`%+.3gkV`), below 1 kV plain volts, below 0.1 V mV; trace clipped at panel edge, not drawn over labels |
| 4.2a | `[ ]` High-voltage display: load 345 kV Line (preset 100 kV/div) then Pole Xfmr (100 V/div) then Tesla Coil (100 kV/div); Shift+wheel through the top of the table; Autoset on a 282 kVpk trace and on a 339 Vpk trace | Presets land on the kV steps; wheel stops at 500 kV/div without wrapping; Autoset picks 100 kV/div and 100 V/div respectively; cursor and measurement readouts (Vpp/Vrms) print kV with 3 significant digits; stacked view keeps per-channel kV/V labels |
| 4.3 | `[ ]` Trigger Auto/Normal/Single, rising/falling/both, level slider, source per channel | Stable display; Normal with no crossing → holds last; Single re-arm |
| 4.4 | `[ ]` Trigger level above signal in Auto | Free-runs; in Normal freezes (no blank) |
| 4.5 | `[ ]` X-Y mode with sin/cos 1 kHz | Circle; with 2:1 freq Lissajous |
| 4.6 | `[ ]` Cursors: CUR cycles Off → **Wave** → **Screen** → Off | Wave: a/b vertical bars with square markers on the source (trigger-channel) trace; readout shows t and V at a and b, Δt, 1/Δt, ΔV, dV/dt and gated Vpp/mean/rms between the cursors — on a 1 kHz 1 Vpk sine, a at a peak and b at the next peak reads Δt 1 ms, 1/Δt 1 kHz, gated Vpp 2 V, rms 707 mV. Screen: two extra horizontal bars; dragging picks the nearest bar; V readouts follow the bar in the source channel's band (works in stacked view). Active cursor marked `*`. Keys: ←/→ nudge (Shift ×10, Ctrl ×0.1), ↑/↓ amplitude bar (Screen), Tab switch, L link (header shows LINK; dragging a keeps Δt), Home reset, 1–8 bind the active cursor to a channel (tags read a:1 / b:2 in channel colours; on an RC LP, a on CH1 and b on CH2 at the same t gives ΔV = the filter drop), 0 restores defaults. Readout survives time/div and V/div changes |
| 4.7 | `[ ]` Measurements Vpp/Vrms/Vavg/f/period/duty on sine and square | Sine: 2.0/0.707/0/1000/1ms/50 %; square 500 Hz |
| 4.8 | `[ ]` FFT on 1 kHz sine + THD/SNR; on square (odd harmonics) | Peak at 1 kHz; square harmonics 1/n |
| 4.9 | `[ ]` Math channels: all 10 ops; Divide by a channel crossing 0 | No Inf/NaN painting whole screen |
| 4.10 | `[ ]` Persistence on/off, Autoset | Decay looks right; Autoset picks sane scales for 1 mV and 100 V signals |
| 4.11 | `[ ]` Scope screenshot BMP; app screenshot action → `screenshots/` dir created | File valid |
| 4.12 | `[ ]` Pop-out window: open, resize, close, reopen, close app with it open | No crash; main scope resumes |
| 4.13 | `[ ]` **Conflict:** FFT + X-Y + cursors + persistence toggled together | Mutually exclusive modes handled |
| 4.14 | `[ ]` Scope while paused / stopped / after Reset | Trace freezes / clears predictably |

---

## 5. Analysis Tools

| ID | Test | Expected |
|----|------|----------|
| 5.1 | `[ ]` Bode on RC LPF, range 1 Hz–1 GHz | −3 dB @ fc, −20 dB/dec, phase −45° at fc; cursor readout |
| 5.2 | `[ ]` Bode on Sallen-Key / notch templates | Q and notch depth plausible |
| 5.3 | `[ ]` Bode with no AC source / no probe | Friendly error |
| 5.4 | `[ ]` Bode **while sim running** then resume | Transient state not corrupted |
| 5.5 | `[ ]` Monte Carlo 10 / 1000 iterations, tolerance 5 % on divider | Mean≈nominal, σ sane; UI responsive; cancel works |
| 5.6 | `[ ]` Parametric sweep | Steps applied, restored after |
| 5.7 | `[ ]` Data export CSV waveform + measurements | Opens in spreadsheet, column count = channels |

---

## 6. Current Flow Visualization

| ID | Test | Expected |
|----|------|----------|
| 6.1 | `[ ]` Series loop | Same particle speed on all wires; direction + → − → ground |
| 6.2 | `[ ]` Parallel branches 1k / 10k | Particle density/size proportional |
| 6.3 | `[ ]` Two sources facing each other | Flow toward lower V |
| 6.4 | `[ ]` AC source | Particles reverse each half-cycle |
| 6.5 | `[ ]` Zoom/pan while animating | Particles stay on wires |
| 6.6a | `[ ]` **Automated:** `template_smoke --burn-test` | All 96 templates run for 10 scope divisions: no resistor above its power rating and no LED above its max current (the canvas warning icon). Power-system / HV / Tesla templates use `R_HP` loads (no limit); low-voltage templates keep real ratings (5 W bleeders, 25 W line-drop load, 1 W emitter resistor) so a genuine overload still warns |
| 6.6 | `[ ]` **Automated:** `template_smoke --flow-test` | All 96 templates: no NaN, two-terminal components conserve charge, KCL holds at every node between wire flows and terminal currents, series templates (RC/RL filters, divider) show identical \|I\| on every wire equal to the resistor current |
| 6.7 | `[ ]` RC High-Pass running, current view on | Particles flow on **every** wire including both resistor leads and the ground return; dot size/speed identical on wire, capacitor body and resistor body; direction reverses each half-cycle |
| 6.8 | `[ ]` Parallel branches (divider with a 2nd resistor across R2) | Branch particle speed/brightness differ by current; junction dot in = out |
| 6.9 | `[ ]` Sources | Inside a source particles run − → + (out of the + terminal); a reverse-connected second source shows the flow reversal |

---

## 7. File I/O (**probe save/load newly added — high priority**)

| ID | Test | Expected |
|----|------|----------|
| 7.1 | `[ ]` Save/Load `.ckt` with every component type placed once, 8 probes, math channels, text | Byte-identical re-save; probe channel/colour/label restored |
| 7.2 | `[ ]` Load a `.ckt` saved by v3.2.2 (no probe section) | Loads; no read past EOF |
| 7.3 | `[ ]` JSON save/load (`circuit.json`) round-trip; LED colour re-derives Vf | Same as binary |
| 7.4 | `[ ]` Load corrupt / truncated / wrong-extension file | Error dialog, current circuit intact |
| 7.5 | `[ ]` Auto-save fires; recover after kill | Backup loads |
| 7.6 | `[ ]` Load while running | Sim stops/resets cleanly |
| 7.7 | `[ ]` Unicode path / spaces in path (this repo is in `python scripts/`) | Works |

---

## 8. Templates (all 96)

See **`TEMPLATE_AUDIT.md`** — one block per template with hand-calculated nominal output,
value variations (applied before run *and* live), the ideal/realistic model matrix (M0–M4),
time-base checks, and save/load. Three templates are flagged as suspect from the BOM alone
(Common Source, Common Drain, Differential Pair — bias path may be blocked by the coupling
cap); the RC Phase-Shift Oscillator is new and uncommitted.

Added 2026-08-24 (#48–#65): RC Band-Pass, LC Low-Pass, Zener Clipper, Voltage Doubler,
Relaxation Osc, HW Rect + Cap, 345 kV Line, 138 kV Line + VAR, 12.47 kV Feeder, Pole Xfmr
120/240, Generator + GSU, Grid: 18 kV to 240 V, Ferranti (open line), Tesla Coil ×3, Line Model
Ladder, Line Drop Basics. Each ships with a `DemoKind` contract (`--demo-test`), an auto-probe
and scope presets, an on-canvas note with the equations and a PROBE line, and — where a number
can be hand-computed — a `probe_cases[]` oracle (`--probe-test`, 66 cases). The Tesla coils are
covered by `--tesla-test`, the new components by `--param-test` (§3.10), and the manual
switch/length checks live in §3.10.3.

Added 2026-08-24, second batch (#66–#72, protection & control, `docs/RESEARCH_AEP_PC.md` §5): CT + 50/51
Overcurrent, 87 Line Differential, 21 Distance Zone 1, 50BF Breaker Failure, SIL Loading, Series
Compensation, 765 kV Line (AEP). Faults are applied by a `COMP_ANALOG_SWITCH` driven by a
`COMP_PULSE_SOURCE` so each demo runs pre-fault → fault → decision unattended; relays are diode
peak-hold detectors into an `ideal=false` op-amp comparator (gain 1e5, ±15 V rails). Contracts:
`DEMO_SWITCH` with f_char 30 / 20 / 20 / 5 (so the 6/f_char run contains the TRIP or BFT release),
`DEMO_WAVEFORM` 60 Hz for the three line templates; nine `probe_cases[]` oracles; manual checks in
§3.11. Status: 72/72 templates, 72/72 demo, 66/66 probe, 72/72 flow, 50/72 geometry clean (all 7 clean),
param-test all OK, tesla 3/3, layout-test 0 failures.

Added 2026-08-24, third batch (#73–#81): **three-phase** — 3-Phase Y Balanced, 3-Phase Unbalanced,
3-Phase 345 kV Line, 3-Phase 6-Pulse Rect (three `COMP_AC_VOLTAGE` sources at 0 / −120 / +120°; a new
`template_extra_probes[]` table adds up to three extra scope probes so all phases and the neutral show;
the unbalanced neutral oracle 20.83 Vpk is a phasor calculation with 10/20/40 Ω loads and a 1 Ω neutral) —
and **signal generators** from Sedra & Smith ch. 18 (`docs/RESEARCH_OSCILLATORS.md`): Bistable (Schmitt)
(inverting, ±7.5 V thresholds), Triangle/Square Gen (f = R2/(4RCR1) = 5 kHz, measured 5000 Hz), Function
Generator (3-breakpoint diode shaper, ~4.9 V sine), Colpitts (MOSFET) (712 kHz, measured 710 kHz), Ring
Oscillator (5 inverters + RC, ~145 kHz, measured 139 kHz). `--osc-test` now takes a per-case dt and uses
the template output spec; `--flow-test` exempts behavioural logic gates. Traps: a forgotten wire (R2 of the
bistable) made the generator dead — `--trace` found it; `ideal=true` op-amps (virtual short) must not be used
in positive-feedback or integrator roles — use finite gain 1e5. Manual checks in §3.12 (three-phase) and
§3.13 (generators). Status: 81/81 templates, 81/81 demo, 73/73 probe, 81/81 flow, osc 7/7, 59/81 geometry
clean (all 9 new clean), knob test 970 runs 0 failed.

Added 2026-08-24, fourth batch (#82–#90): **Hartley (MOSFET)** (tap-at-Vdd topology built from the Colpitts core:
L1 = drain inductor 50 µH, L2 50 µH via a 10 nF coupling cap, C 1 nF; 503 kHz formula, **measured 557 kHz** — the
coupling cap in series with L2 and the device capacitances pull it; `--osc-test` band ±25 %) and **Clapp (MOSFET)**
(Colpitts with 100 pF in series with L: 1.744 MHz, measured 1.76 MHz), both in Oscillators; and the **textbook set**
from Agarwal & Lang / Sedra & Smith (`docs/RESEARCH_TEXTBOOK_CIRCUITS.md` items 1, 2, 6, 7, 8, 9, 26): Thevenin
Equivalent (3.00 V), Superposition (7.333 V — the DC current source is rotated 180° so it injects into the node; the
model's current flows out of its "−" terminal), RC Step Response, RL Step Response, RLC Step (Ringing) (first peak
9.53 V), RLC Damping Ladder (R = 20 / 632 / 2000 rows with extra probes), Op-Amp Saturation (clips at 15 V, extra probe
on the inverting input). A new palette group **Transients** (`TG_TRANSIENTS`, the 10th) holds the step responses.
Engine: the **inductor now uses the theta = 0.6 method like the capacitor** (backward Euler damped a high-Q LC by
≈ (ω dt)²/2 per step); all previous oracles still pass. `--probe-test` now honours the template's scope time/div
(never a coarser dt than the preset maps to) so the ring is resolved. Traps: a missing wire found by `--trace`,
current-source polarity, the Hartley's DC path through the drain inductor (and none allowed through L2), a probe
window straddling a falling edge, and dt resolution of ringing — all in the TEMPLATE_AUDIT section. **Not shipped:**
a Pierce Crystal template (op-amp inverting stage + Ls/Cs/Rs/Cp teaching crystal at 100 kHz + π network) — its
builder `place_pierce()` stays in `src/circuits.c` but it only sustains at dt ≤ 10 ns with the current integrator;
logged in `docs/ROADMAP.md` as future work (trapezoidal theta = 0.5 or a crystal-specific model). Manual checks in
§3.13.21–24 (Hartley / Clapp) and §3.14 (textbook set). Status: 90/90 templates, 90/90 demo, 80/80 probe, 90/90 flow,
osc 9/9, knob test 1066 runs 0 failed, 68/90 geometry clean (all 9 new clean).

Added 2026-08-24, fifth batch (#91–#96): **Single-Tuned Amplifier** (CE with an L 1 mH ‖ C 2.53 nF ‖ Rq 10 k collector tank,
f0 = 100.06 kHz, Q 16, sweep 20–500 kHz in 0.5 s, `DEMO_BANDPASS`; oracle 4.5 Vpk ±50 % for 10 mV in), **Common Base** (+188, oracle
1.88 ±30 %; R_in = 25 Ω so 50 Ω of source resistance loses 1/3), **Darlington Follower** (100 k source, Re 100 Ω, 6 V + 1 Vpk in; oracle
0.91 ±12 %), **SR Latch (NOR)** (S 50 µs at 0.2 ms into the Qbar gate, R at 0.6 ms into the Q gate, 1 ms period, Q = NOR ordinal 1; oracle
Q max 5 V at 0.5 ms — the first build had S and R swapped and the oracle caught it), **Power Plant (3-phase)** and **Transmission
Substation** built on the new **`COMP_SOURCE_3PH`** block (A / B / C at `phase`, −120°, +120°, common N; `v_peak`, `frequency`, `phase`,
`r_series`, `l_series`; **three MNA aux variables** via `component_aux_count()`, which the allocator and the terminal-current code now use):
14.7 kVpk generator behind 0.184 mH → three GSU 1:19.17 → breakers → 3 × 100 mi 345 kV lines → 198.4 Ω (oracle 259.6 kVpk ±6 %); 281.7 kVpk grid
→ 50 mi lines → breakers → 345/138 autos N = 0.4 → 30 mi feeders → 171.5 Ω + 0.22 H with switchable 6.1 µF cap banks (oracle 103 kVpk ±8 %).
Engine / tool: the terminal-current re-stamp used `sim->time` *after* the step had advanced (one step of phase error, a 12 % KCL mismatch
through a 1 mΩ source) — now stamps at `sim->time − dt`; oscillators no longer auto-probe their kick source; new modes **`--probe-audit`**
(per-template probe / owner / min / max / flags) and **`--series NAME T NODE`** (time series); UI: **Ctrl+Space** opens Spotlight, **`/`**
focuses the palette filter. Traps: the 10 px node merge shorted phases B and C in the fan-out and phase C onto a transformer's grounded P2
(fixed by 20 px spacing / re-routed rows); the S/R swap. Manual checks in §3.15. Status: 96/96 templates, 96/96 demo, 86/86 probe, 96/96
flow, 74/96 geometry clean (all 6 new clean), knob test 1182 runs 0 failed, probe-audit 10/96 flagged — all physically expected (HP outputs
small at the sweep start etc.).

---

## 9. Subcircuits (WIP)

| ID | Test | Expected |
|----|------|----------|
| 9.1 | `[ ]` Pin markers → Ctrl+G → IC block created with named pins | Dialog pre-fills pins |
| 9.2 | `[ ]` Place IC block, wire it, run | Documented placeholder behaviour, no crash |
| 9.3 | `[ ]` Ctrl+G with no selection / no pins | Friendly message |

---

## 10. Environment & Cross-cutting Conflicts

| ID | Test | Expected |
|----|------|----------|
| 10.1 | `[ ]` Temp slider extremes with thermistor + tempco resistor + diode (Is(T)) | Monotonic, no NaN at −40/125 |
| 10.2 | `[ ]` Lux 0 % with photodiode in TIA template | No divide-by-zero |
| 10.3 | `[ ]` Speed 100× + dt 1 ns + 8 probes + FFT + persistence | Frame time acceptable; sim doesn't starve renderer |
| 10.4 | `[ ]` Undo a property change while running | Value reverts live |
| 10.5 | `[ ]` Reset while Bode/MC dialog open | Dialog closes or ignores; no dangling pointer |
| 10.6 | `[ ]` Delete op-amp during clamp step (delete while running) | No stale `voltage_var_idx` |

---

## Execution order (suggested)

1. §0 pre-flight — especially decide on the debug prints (0.3), they affect everything.
2. §1 editing → §2 sim/time-base (foundation for everything else).
3. §3.6 op-amps + §7 file I/O + §3.9.4 LED array (all touched by the uncommitted diff).
4. Remaining §3 components (incl. §3.10 HV components / Tesla / power manual checks and §3.11 protection & control, §3.12 three-phase, §3.13 signal generators, §3.15 batch five / 3-phase source), §4 scope, §5 analysis, §6, §8 templates, §9, §10.

Commit after each section passes so bisecting stays cheap.
