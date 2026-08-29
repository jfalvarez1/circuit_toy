# Where things stand — 29 August 2026

A snapshot: what went in today, what is left, and what is deliberately not being done.
Written at v3.18.0 plus the unreleased work on `main`.

---

## Shipped today

### v3.15.0 — Pierce crystal, interview prep part 1, per-channel scope knobs

- **COMP_CRYSTAL** integrates its motional arm trapezoidally inside the component, which is what
  keeps a Q of 314 from being damped away at these time steps. Holder capacitance came down from
  1 nF to 33 pF — at 1 nF the arm is shunted hard enough that the loop locks onto the parallel
  resonance (110.8 kHz, 1.6 V) or dies. A real HC-49 is 3–30 pF; the old value was three decades
  out. **Pierce Crystal Oscillator** runs 30 V pk-pk at 100.6 kHz. `--xtal-test` is the oracle.
- **Interview: instrumentation & scope** — 7 templates.
- **Pop-out scope INPUTS strip**: one knob per channel, in that channel's trace colour, setting
  that channel's own volts/div. The name plate said DUAL-TRACE; `MAX_PROBES` has been 8 for a
  long time.

### v3.16.0 — interview prep complete, canvas navigation, named probes

- **Interview: power & converters** (3), **I/O & signal integrity** (5), **fundamentals** (4).
  Twenty templates in four groups, none repeating a circuit that already existed.
- **New named part**: IRF9540N, power P-channel, checked at its data sheet condition.
- **Pan tool** and toolbar **-** / **+** / **Fit** — a laptop trackpad has no middle button.
- **Palette opens collapsed**: only Tools expanded, 19 template groups closed.
- **Probes can be named.** Type `Vout` and every place the scope said CH2 says Vout.
- **Switches show their state** on the schematic. `render_analog_switch` had been called with
  `closed = false` hard-coded, so a switch that was conducting looked identical to one that was
  not — which is why clicking the buck's switch appeared to do nothing.
- **`--view-test`**: every template puts something on the scope; every switch is clickable.

### v3.17.0 — self-installing updates, connectivity audit

- **The updater updates itself**: six-second countdown, then download, wait for exit, unpack,
  relaunch. Never interrupts unsaved work; Esc defers; `--no-auto-update` opts out. The script is
  failure-safe — it runs after the app has quit, so every failure path restarts the old version.
- **`--conn-test`**: is every pin actually wired to something? It found six that were not — the
  output coupling capacitors in Common Emitter, Common Source, Source Follower and Two-Stage Amp
  had no node at all on their far side; Line Drop Basics' return conductor missed the ground
  terminal by 20 px; Ground Bounce had a rail I placed and never used.
- **CI attaches the binary to a tagged release.** It never had a step that did.

### v3.18.0 — a transmission line with a real delay

- **COMP_DELAY_LINE**, Bergeron's method: each end is Z0 in series with a source carrying what
  the far end launched one delay ago. Two stamps, no auxiliary row, exact for a lossless line at
  any time step. The delay is in a history buffer, so 5 ns of cable does not need 5 ns steps.
- **`--line-test`**: matched / open / shorted, and the timing — the far end stays dark for T, and
  a 10 ns cable waits 10 ns.
- **Transmission Line (real delay)** template; Z0 and delay editable, with the equivalent length
  of coax shown in the properties panel.
- **Eleven op-amp templates** ran their +12 V rail across the body of the supply feeding it. The
  through-a-body audit had been comparing node IDs, so a wire drawn to a transistor's base read
  as a wire through the transistor; it matches by terminal position now.
- **`permissions: contents: write`** — v3.17.0's build was green and the upload still failed with
  `Resource not accessible by integration`. GitHub's default token is read-only.

### On `main`, not yet released

- **Every cable that needs propagation now propagates.** Signal Reflections, Termination and
  Scope Input: 1 M vs 50 ohm each built their line as an L-C ladder (eight sections, five, five);
  all three use the Delay Line. The open-ended scope cable reads exactly 2.000 V for a 1 V launch
  where the ladder gave 2.207 with ringing that was the ladder's own resonance.
- **A real file dialog.** `Load` opened `circuit.json` and nothing else. `app_pick_file` wraps the
  Windows Open dialog.
- **SPICE button on the toolbar.** The importer has been command-line only since it landed.
- **LM317 Adj Reg** rewired: its two divider resistors overlapped and the ground bus ran through
  the lower one. Clean now.

---

## Test coverage as it stands

| Mode | Result |
|---|---|
| `template_smoke` | 183/183 |
| `--probe-test` | 205 oracles |
| `--demo-test` | 183 demo contracts |
| `--conn-test` | 0 loose pins, 0 parts connected to nothing |
| `--view-test` | 183 templates show something; 50 switches clickable |
| `--line-test` | 5 transmission-line checks |
| `--xtal-test` | 4 crystal checks |
| `--flow-test` | 183/183 |
| `--geom-test` | 159/183 clean, 0 hard violations |
| `--knob-test` | 2670 runs |
| `--part-test` | 25 checks over 21 named devices |
| `--layout-test` | palette, scope knobs, zoom and SPICE buttons, probe naming |
| `tools/gui_smoke.py` | every template through the real app, plus zoom / pan / switch-click |

---

## Left to do

### Roadmap

1. **741-level op-amp macromodel** as a subcircuit — input pair, gain stage, output stage — so
   its behaviour comes from its topology instead of GBW and slew-rate parameters. This is the
   biggest remaining teaching item.
2. **`.PARAM` expressions** in the SPICE importer, and parameterised subcircuits. The importer
   handles R, L, C and `X` inside `.SUBCKT`; `.MODEL` cards and semiconductor instances do not
   import, and each unsupported line is counted and named rather than dropped quietly.
3. **The 555's RESET and CONTROL pins** are not exposed. The block is a real subcircuit built
   from its own comparators and latch, so this is wiring two more pins out of it.
4. **An optocoupler and a 7-segment decoder** as built-in subcircuits, in the same shape as the
   555.

### Wiring audit

Cosmetic, all named per template with their endpoints by `--geom-test`:

- **17 wires still cross a component body.** Down from 49. The remaining ones are mostly
  op-amp templates where the bias divider sits in the lane the input and feedback resistors use
  to reach the amplifier — Schmitt Trigger, Window Comp, Difference Amp, Sallen-Key. Each needs
  its own look; moving the divider is not enough, the wires have to be rerouted around it.
- **23 wire crossings.** A crossing without a junction dot is what a real schematic does, so
  these are the lower priority of the two.

### Known limitations, written up rather than worked around

- **CCM vs DCM** — an asynchronous buck in discontinuous conduction has an interval where neither
  the switch nor the diode conducts, and with an ideal switch that leaves the switch node
  undefined. It needs a switch model with a defined off-state capacitance, or local time-step
  control across a commutation.
- **Displacement current and the flow audit** — terminal currents are recovered by re-stamping
  and taking the row residual, which recovers conduction current only. Charge into a MOSFET gate
  or a reverse-biased junction is real current in the wires and appears in no `terminal_current`.
  Two templates are exempt from the node-KCL check and say so in their result line. The real fix
  is for the MOSFET and diode models to report their capacitive terminal currents.
- **No dispersion** — neither transmission line model has frequency-dependent loss.
- **`gui_smoke.py` needs a display.** `SDL_VIDEODRIVER=dummy` has no renderer to read pixels back
  from, so it is a local check rather than a CI job.
