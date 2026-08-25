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
| 0.6 | `[ ]` Headless regression: `build\tools\template_smoke.exe` (add `--verbose` for bias points, `--nodes` for net mapping) | Prints `81/81 templates passed`; run after every engine change |
| 0.7 | `[ ]` `template_smoke --probe-test` | 73/73: every template's designated output node matches the hand calculation (DC level, amplitude, peak or mean) — includes the kV power buses, the three Line Model Ladder rows, the nine protection & control oracles (burden / R_d / VT secondary amplitudes, TRIP and BFT maxima, SIL / series-comp / 765 kV load ends) and the seven three-phase / generator oracles (373.3 V balanced load, 20.83 V neutral shift, 264 kV per phase, 169.3 V plus bus, ±15 V bistable, 7.5 V triangle, 4.9 V shaped sine) |
| 0.8 | `[ ]` `template_smoke --osc-test` (also `--osc-dt 5e-6`) | 7/7: Wien ~1.56 kHz, phase-shift ~6.0 kHz and relaxation ~455 Hz really oscillate at dt from 100 ns to 5 us (`--osc-dt` sets these three); Triangle/Square and Function Generator 5000 Hz at their own dt 200 ns, Colpitts 712 kHz (measured 710) at 5 ns, Ring 145 kHz (measured 139) at 20 ns — the **per-case dt** in `cases[]` overrides `--osc-dt` for those four, and the output node comes from the template output spec |
| 0.9 | `[ ]` `template_smoke --geom-test` | Schematic audit: 0 diagonal wires anywhere; 59/81 clean (all 18 templates added 2026-08-24, the 7 protection & control templates and the 9 three-phase / signal-generator templates #73–#81 are clean); remaining crossings / wires-through-bodies are listed per template (see TEMPLATE_AUDIT open items) |
| 0.10 | `[ ]` `template_smoke --flow-test`, `--scope-test` | 81/81 and 8/8 (flow: behavioural logic gates have no terminal currents, so KCL is only asserted on passive nodes in 50BF and the Ring Oscillator) |
| 0.11 | `[ ]` `template_smoke --demo-test` | 81/81: every template honours its `DemoKind` contract (`template_demo[]`: LOWPASS/HIGHPASS/BANDPASS/NOTCH bracket f_char with the sweep, ENVELOPE/LIMITER follow or clamp the amplitude sweep, WAVEFORM varies, SWITCH swings rail to rail, DC is steady, OSC self-starts). WAVEFORM/SWITCH/DC run 6/f_char and judge the second half; the harness now **forces a usable dt for pulse-only circuits** (no AC source ⇒ auto-dt unusable: if dt is 0, > run/200 or < run/100000 it uses run/1000 — 50BF is the case that needs it) |
| 0.12 | `[ ]` `template_smoke --tesla-test` | 3/3 plus the tuned-vs-detuned comparison (§3.10.2) |
| 0.13 | `[ ]` `template_smoke --param-test` | All `OK`: spark gap, toroid, transmission line, transformer ratio, **4b analog switch as a fault switch** (r_on 0.01 / 0.3 / 100 / 1e6 Ω, 0/5 V pulse control: load ≈ 0 before the pulse, 10·100/(100 + r_on) during it ±5 %), **4c transformer as a CT** (N = 120 / 400 / 2875, 100 A primary ⇒ 100/N V on a 1 Ω burden ±3 %) and scope-preset limits (§3.10.1) |
| 0.14 | `[ ]` `template_smoke --response NAME` | Explorer, not pass/fail: prints per-node amplitude in 8 log bins of the template's sweep — use it to choose an output node / DemoKind for a new template |
| 0.15 | `[ ]` `template_smoke --trace NAME T` | Explorer, not pass/fail: runs the template for T seconds at the app dt and prints, for every node, min / max voltage with the components attached, plus the final state of every switch — the tool for "why does TRIP never drop" / "which node shorted" questions on the relay templates (e.g. `--trace 50/51 0.2`, `--trace 50BF 1.2`) |

Smoke test modes: (none) template load+run · `--demo-test` · `--probe-test` · `--osc-test [--osc-dt X]` (per-case dt in `cases[]` wins over `--osc-dt`; output node from the template spec) · `--flow-test` (behavioural gates exempt) ·
`--geom-test` · `--scope-test` · `--sweep-check` · `--tesla-test` · `--param-test` · `--response NAME` · `--trace NAME T` ·
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
(bistable rail, triangle 7.5 V, sine 4.9 V) and `--osc-test` with a **per-case dt** (200 ns / 200 ns / 5 ns / 20 ns).
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
| 6.6 | `[ ]` **Automated:** `template_smoke --flow-test` | All 81 templates: no NaN, two-terminal components conserve charge, KCL holds at every node between wire flows and terminal currents, series templates (RC/RL filters, divider) show identical \|I\| on every wire equal to the resistor current |
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

## 8. Templates (all 81)

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
4. Remaining §3 components (incl. §3.10 HV components / Tesla / power manual checks and §3.11 protection & control, §3.12 three-phase, §3.13 signal generators), §4 scope, §5 analysis, §6, §8 templates, §9, §10.

Commit after each section passes so bisecting stays cheap.
