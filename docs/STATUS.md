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

### v3.21.1 — the crash every release since 3.19.2 had

- **The shipped binary died a moment after start-up.** `0xC00000FD`, a stack overflow, logged
  right after `app_init ok`. It happened in the binary CI ships and not in one built locally
  from the same source, which is what made it look like somebody's machine: `App app = {0}` is
  a local in `main()` carrying the whole UI state by value — the palette, the scope's capture
  buffers, every panel rect, **223 KB** of it — and whether that fits in the default 1 MB stack
  alongside everything `main()` then calls depends on the compiler's frame layout. CI's
  toolchain lays it out differently. It is in static storage now, with the reserve raised to
  8 MB besides.
- **v3.19.2, v3.20.0 and v3.21.0 are all affected**, and all three passed CI.
- **CI starts the binary it ships now.** `--layout-test` returns before `SDL_Init` and never
  constructs the App, so the job was building a release, running a check that could not reach
  the crashing path, and shipping it. It now runs the exe with a template and a screenshot and
  fails if it does not get there, plus a PE-header check that the stack reserve stays at 8 MB.
  `--version` works fine on a binary that cannot start; that is why it gave false confidence.
- **The updater writes `update.log`** next to `crash.log`, timestamped per step, because an
  update that fails leaves nothing behind once its console window closes.

### v3.21.0 — readable text

- **Schematic text is antialiased and larger.** The canvas font was the 8x8 bitmap drawn as
  filled squares, which staircases every diagonal and gets worse the bigger it is drawn. It is
  resampled once into a coverage atlas — each glyph a cell whose alpha is the bitmap sampled
  bilinearly, with a contrast curve so a one-pixel stem keeps its weight — and drawn as a
  textured quad per character. Characters are 11 px rather than 8. The UI panels keep their own
  font; this is only the canvas.
- **Values are in schematic notation**: `10k`, `4.7k`, `100nF`, `2.2uF`, `170V 60Hz`, where it
  used to say `10.0 kOhm`. Nine characters for a value a schematic writes in three, with parts
  80 px apart, is how they came to overlap.
- **Text landing on other text is audited**, which nothing had ever checked: 94 labels across 51
  templates were overlapping. The notation fixed two thirds; the rest moved, and a few templates
  had parts standing closer than their own labels are wide. Zero now, and the value labels come
  from the renderer's own function — split into `src/label.c` so the headless tool can call it —
  so what is measured is what is drawn.
- **Two placement rules changed.** A voltage source is a tall part whose terminals are top and
  bottom, but the rule keyed off the rotation field and called it horizontal, so its label went
  underneath — onto its own ground symbol and the lead down to it, which is where a sweeping
  source's frequency readout was landing too. Tall parts label to the right; a source labels to
  its outside. A transmission line's label is wider than the gap to the next part in a
  substation bay, so it goes above the line.
- **The transmission line symbol is drawn at a size you can read.** 52 x 18 with masts 15 px
  tall — smaller than the resistor beside it — is now 64 x 44 with both cross-arms and a lattice
  brace. Terminals unchanged.

### v3.19.2 — the update crash, and the counter that never counted

- **Update no longer closes the app and leaves it closed.** Two separate faults. A tag is
  published the moment it is pushed and CI attaches the binary a quarter of an hour later, so
  for that window the app saw a newer version, quit itself, and downloaded a file that did not
  exist; the updater now requires the zip to be on the release, and CI creates the release as a
  draft and publishes it only after the upload. Then the deeper one: the app shipped as a
  **console subsystem** binary and the updater relaunches it from a PowerShell console that
  closes a moment later — a console process attached to a console that is going away either
  takes the close event and dies ("reopens and just crashes instantly") or fails during DLL init
  with 0xC0000142 ("the application was unable to start correctly"). Both reports match. It is a
  GUI subsystem binary now; `main()` attaches to the parent console when given arguments, so
  `--version` and the rest still print and CI still captures output through its pipes.
- **The Counter is a real part.** It had no logic behind it at all: two output pins driven from a
  single shared bool, and nothing in the program incremented it — `logic_init_component` had no
  caller anywhere, so `is_logic_component` was false for everything and the whole
  sample → propagate → drive pass in `logic.c` did nothing. Four bits, a reset, a carry and a
  settable modulus now, driven once per time step. The engine is woken for the counter alone;
  switching the rest over would change how every existing logic template behaves.
- **The 7-segment display lights.** The renderer was never handed the segment currents, so it
  drew the same dead outline whatever it was driven with, with `7SEG` printed across the middle
  of the digit. Segments are drawn from their own current now and the label sits under the
  package.
- **Four new templates.** 7-Segment Segment Test (every segment on its own switch), BCD Counter
  to 7-Segment (ten counts light every segment, so a stuck one reads as a wrong digit), Digital
  Clock HH:MM:SS (six digit blocks carry-chained; 130 s of simulation reads 02:10, and an AND
  gate watching for tens=2 with units=4 resets the hours at 24), and Wireless Link (what the TX
  and RX antenna parts actually do).
- **TX and RX are out of the Display palette section** and into one of their own.
- **Labels printed across symbols are checked.** Nothing measured where a label lands; fourteen
  of them were sitting on a part, across eleven templates. All moved, count now zero.
- **Wiring:** Differential Pair had Q2 at 180 degrees, which turns an NPN upside down — collector
  at the bottom, emitter at the top — so RC2's wire ran down past Q2's own emitter and the
  emitter bus landed inside the netlist's 10 px merge radius of the collector, shorting them.
  Comparator's input ran through R2's body.

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
| `template_smoke` | 187/187 |
| `--probe-test` | 208 oracles |
| `--demo-test` | 187 demo contracts |
| `--conn-test` | 0 loose pins, 0 parts connected to nothing |
| `--view-test` | 187 templates show something; 58 switches clickable |
| `--line-test` | 5 transmission-line checks |
| `--xtal-test` | 4 crystal checks |
| `--flow-test` | 187/187 |
| `--geom-test` | 166/187 clean, 0 through-body, 0 hard violations, 0 text on symbols, 0 text on text |
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

- **No wire runs through a component body, in any template.** Down from 49 when the audit
  started. The last nine each needed their own answer: the Schmitt Trigger's reference divider
  stood in the only lane the input and feedback wires have to the op-amp pins and moved out to
  the left; the Window Comparator's supply rail sat *inside* R1, because the divider reaches up
  past it; Sallen-Key's R1 and C1 overlap in x, so a turn at either one's column goes through
  the other and the wire steps out of the row first; the MOSFET mirror's diode connection
  overshot the drain and came back through the far side of the transistor; the CMOS inverter had
  a stub to nowhere drawn back through its own PMOS.
- **121 wire crossings**, most of them in the two new digit templates: a 7-segment display has
  four segment pins on one side of the package and four on the other, so any driver has to wrap
  three wires around the body, and a crossing is what that costs. A crossing without a junction
  dot is what a real schematic does, so these stay the lower priority of the two.
- **0 labels printed across a symbol**, checked since the label-box measurement went in.

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
