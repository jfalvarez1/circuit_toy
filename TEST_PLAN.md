# Circuit Playground — Feature Test Plan

Goal: exercise every feature one at a time and record pass/fail. Each test states the
**setup**, the **expected result** (with a hand-calculated number where possible), and the
**watch-for** list: graphical glitches, model accuracy, model glitches (NaN / blow-ups /
non-convergence), conflicting settings, time-base changes, and runtime (while-running) changes.

Status legend: `[ ]` not run · `[P]` pass · `[F]` fail (link issue/notes) · `[S]` skipped

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
| 0.6 | `[ ]` Headless regression: `build\tools\template_smoke.exe` (add `--verbose` for bias points, `--nodes` for net mapping) | Prints `65/65 templates passed`; run after every engine change |
| 0.7 | `[ ]` `template_smoke --probe-test` | 57/57: every template's designated output node matches the hand calculation (DC level, amplitude, peak or mean) — includes the kV power buses and the three Line Model Ladder rows |
| 0.8 | `[ ]` `template_smoke --osc-test` (also `--osc-dt 5e-6`) | Wien ~1.56 kHz, phase-shift ~6.0 kHz and relaxation ~455 Hz really oscillate, at dt from 100 ns to 5 us |
| 0.9 | `[ ]` `template_smoke --geom-test` | Schematic audit: 0 diagonal wires anywhere; 43/65 clean (all 18 templates added 2026-08-24 are clean); remaining crossings / wires-through-bodies are listed per template (see TEMPLATE_AUDIT open items) |
| 0.10 | `[ ]` `template_smoke --flow-test`, `--scope-test` | 65/65 and 8/8 |
| 0.11 | `[ ]` `template_smoke --demo-test` | 65/65: every template honours its `DemoKind` contract (`template_demo[]`: LOWPASS/HIGHPASS/BANDPASS/NOTCH bracket f_char with the sweep, ENVELOPE/LIMITER follow or clamp the amplitude sweep, WAVEFORM varies, SWITCH swings rail to rail, DC is steady, OSC self-starts) |
| 0.12 | `[ ]` `template_smoke --tesla-test` | 3/3 plus the tuned-vs-detuned comparison (§3.10.2) |
| 0.13 | `[ ]` `template_smoke --param-test` | All `OK`: spark gap, toroid, transmission line, transformer ratio and scope-preset limits (§3.10.1) |
| 0.14 | `[ ]` `template_smoke --response NAME` | Explorer, not pass/fail: prints per-node amplitude in 8 log bins of the template's sweep — use it to choose an output node / DemoKind for a new template |

Smoke test modes: (none) template load+run · `--demo-test` · `--probe-test` · `--osc-test [--osc-dt X]` · `--flow-test` ·
`--geom-test` · `--scope-test` · `--sweep-check` · `--tesla-test` · `--param-test` · `--response NAME` ·
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
| 1.13 | `[ ]` Spotlight `Ctrl+K`: fuzzy query, arrow keys, Enter, mouse click, Esc | Every component in `types.h` reachable by search |
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

---

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
| 6.6 | `[ ]` **Automated:** `template_smoke --flow-test` | All 65 templates: no NaN, two-terminal components conserve charge, KCL holds at every node between wire flows and terminal currents, series templates (RC/RL filters, divider) show identical \|I\| on every wire equal to the resistor current |
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

## 8. Templates (all 65)

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
can be hand-computed — a `probe_cases[]` oracle (`--probe-test`, 57 cases). The Tesla coils are
covered by `--tesla-test`, the new components by `--param-test` (§3.10), and the manual
switch/length checks live in §3.10.3.

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
4. Remaining §3 components (incl. §3.10 HV components / Tesla / power manual checks), §4 scope, §5 analysis, §6, §8 templates, §9, §10.

Commit after each section passes so bisecting stays cheap.
