# Roadmap

## A programmable block that runs pasted Arduino code (2026-09-02, proposed)

**The goal is not an Arduino emulator.** Nobody needs AVR instruction timing here, and building
it would be months of work for a result whose value to a circuit is nil. What is wanted is much
smaller and much more useful: **a part on the canvas with pins, that runs sketch-shaped code and
drives those pins.** Paste

```
void setup() { pinMode(13, OUTPUT); }
void loop()  { digitalWrite(13, HIGH); delay(500); digitalWrite(13, LOW); delay(500); }
```

into it, wire pin 13 to an LED and a resistor, press Run, and watch the LED blink on the scope
along with everything else in the circuit. That is the whole feature, and almost every sketch a
beginner writes is that shape.

### Why this is a circuit feature and not a novelty

Half of what a learner builds now has a microcontroller in it, and the BMS this program's
battery templates came from is a good example: its charge enable, its discharge enable and its
thermal cutout are all *decisions* taken by an ESP PICO W. Circuit Toy models the analog around
those decisions and then draws the decision itself as a 5 V source, because that is all it can
do. A block that runs `digitalWrite(EN, LOW)` when the cell voltage crosses a threshold turns
those templates from an analog fragment into the instrument.

### What has to exist

- **The block.** A component with a configurable pin count, each pin one of: driven output
  (a voltage source with a series resistance), high-impedance input (reads the node voltage and
  thresholds it), or PWM output (a pulse source whose duty the code sets). Pin names on the
  symbol, so a table can wire to `D13` by name now that named nets exist.
- **A parser and interpreter** for the subset that matters: `setup`/`loop`, `pinMode`,
  `digitalWrite`, `digitalRead`, `analogWrite`, `analogRead`, `delay`, `delayMicroseconds`,
  `millis`, `micros`, integer and float variables, `if`/`else`, `for`, `while`, the usual
  operators, and function definitions. Explicitly NOT: pointers, `String`, classes, libraries,
  interrupts, `Serial` beyond a print that goes to the status bar.
- **A clock that agrees with the simulation.** The interpreter runs against `sim->time`, not
  wall clock: `delay(500)` means the block does nothing until simulated time has advanced half a
  second. This is the part that will be got wrong first if it is not designed for on day one -
  a sketch that runs at wall-clock speed against a simulation running at a hundredth of real
  time produces nonsense, and produces it silently.
- **Somewhere to type it.** The properties panel is too small; this wants the same treatment the
  netlist paste got - a button that takes the clipboard, and a file path on the command line.

### How it should be checked

The same way everything else here is: a suite that runs sketches whose behaviour is arithmetic.
Blink at 500 ms has a period that can be measured off the node. A sketch that reads a divider
and switches an output at a threshold has a switching point that can be computed from the
resistors. A `for` loop that steps `analogWrite` has a duty ramp with a known slope. If the
interpreter drifts, those move.

### What to be careful of

- **Timing granularity.** `delayMicroseconds(1)` against a 10 us simulation step cannot mean
  what it says. The block should say so rather than round silently - the same principle as the
  time-step warnings already here.
- **The temptation to grow it.** Every sketch that does not run will look like a small missing
  feature. The line to hold is: this drives pins in response to pin states and time. Anything
  that is really about the language rather than the circuit belongs outside.

---

## The speed control is safe, and it was not honest (2026-08-30)

Asked whether the 1.0x speed slider is dangerous - whether running faster can break the
simulation. It cannot, and the reason is worth writing down: **speed never touches dt.** The
stepper asks for `delta_time * speed / dt` steps a frame, every one of them a full step at the
same accuracy, so 100x is the same arithmetic as 1x and simply more of it. What it can do is fail
to finish: an interactive frame gives up after 12 ms of wall clock so the interface stays alive,
and on a heavy circuit that means fewer steps than were asked for. Nothing is skipped, nothing is
approximated, and no partial step is ever taken - the loop breaks between steps. Scripted runs
(`--shot`, `--record`) do not use the wall-clock budget at all; they take a fixed, capped count so
a screenshot is reproducible.

So the trade the slider makes is exactly the one it was designed for: real time when the machine
can afford it, slower when it cannot. The original goal - a 100 Hz wave that looks like 100 Hz
rather than a slideshow - is met whenever the step budget allows, and the budget is the honest
limit rather than a fudge.

What was wrong was the reporting. `sim_realtime_ratio` has been measuring the achieved fraction
every frame, and the UI field it is copied into carries the comment "shown next to speed" - and
nothing drew it. The toolbar showed the request as though it were the fact, so a circuit crawling
at a twentieth of real time looked identical to one keeping up. The speed number is now drawn
amber when the stepper is behind.

The figure itself is not drawn, and that is a decision rather than an omission: there is room for
about four characters between the speed text and the dt label. The first attempt drew a second
multiplier there and it landed on top of the dt readout; the second, a compact percentage, was
correctly suppressed by its own fit guard, which is how it became clear the space is not there.
The colour costs nothing and answers the question that matters - am I seeing this in real time.

## Open items, ordered by what they cost (2026-08-30)

Everything below is either a recorded limitation or a warning the suites deliberately do not fail
on. Nothing here is failing. Ordered cheapest first, which is the order they are being worked.

**1. Parts with no editable properties. MEASURED 2026-08-30, and being worked.** `--prop-gap`
(new, in the app) lists them: **124 creatable types, 35 offered rows, 89 offered none - and only
two of those 89 were structural**, a ground and a text label. So it is 87 real gaps, not a handful.
The measurement changed the shape of the job, which is why it came first.

Some of it is worse than absent. The transformer drew no panel at all, so its turns ratio - the
one number the part is about - was uneditable, and its two winding resistances had **working apply
handlers with no way to reach them**. That is done: turns ratio, ideal/real, magnetising
inductance, coupling and both winding resistances, 37 parts offering rows now.

**Batches 3 and 4 done:** the cheap kind first - parts that already had panels and were missing
rows for properties that already existed. A capacitor's leakage and an inductor's saturation
current were being drawn in grey as read-only text while the stamp read them every step; the
capacitor's ESL and the diode's junction capacitance were not shown at all. Then six parts that
offered nothing whatsoever: potentiometer, photoresistor, thermistor, both JFETs, battery and the
four switch types.

**53 of 124 creatable types offer rows now, against 35 this morning. 133 typed rows, 21 property
types still unbuilt against 40.** What is left is mostly the digital family (gates, flip-flops,
counters, the 555) and the display parts, where the honest question is what a user would even set
- a NAND gate has a propagation delay and little else.

**Batch 2 done:** the DC motor (armature R and L, Kv, Kt, rotor inertia, friction, load torque),
the relay (coil R and L, pickup and dropout currents, both contact resistances) and the four
controlled sources, whose gain lived in the enum as PROP_GAIN with nothing on either end of it -
no row, no handler - so the one number a VCVS is for could not be set by any means. 43 parts offer
rows now, 107 typed rows, and 35 property types remain unbuilt.

 is the guard for the class: it fails when a property is wired at one end
only. It is in the battery, and it took two goes to make honest - see the note in its header about
fallthrough groups.

*Plan: work down the list by how much the simulation actually reads the property, in batches small
enough to verify. `--prop-test` guards each batch - it checks that every row offered can be
applied - and `--prop-gap` reports the remaining count. Next in line by that measure: the DC motor
and the relay (both had their physics fixed today and neither can be configured), the four
controlled sources (VCVS/VCCS/CCVS/CCCS, whose gain is the entire point of the part), the JFETs,
the Darlingtons and the thyristors.*

**2. The high-power load's power label is instantaneous.** It is v^2/R at the drawn frame, so two
identical AC loads can read 909 kW and 2.86 MW depending where the frame lands. *Plan: leave
unless asked. The field feeds the overload colouring and the burn-in audit, which both want the
instantaneous value; smoothing only the label needs display-side state that does not exist yet.*

**3. Twenty-one templates with cosmetic geometry warnings.** All drawn wire crossings, no hard
violations: twelve have one, five have two, and the Digital Clock alone has 87. *Plan: leave the
Digital Clock (a bus-heavy schematic legitimately crosses) and look at the single-crossing ones
opportunistically. A crossing is legal schematic practice, which is why these are tracked rather
than failed.*

**4. The Digital Clock overflows the canvas at the 0.3x zoom floor.** *Plan: leave, documented.
`tools/edge_gui.py` reports it as a NOTE keyed off the floor itself. A lower floor would make it
uncut and unreadable, which is not a trade worth making.*

**5. Three `--class-test` warnings.** Hartley sits on the 5 % interval-agreement threshold at 188
samples a cycle with a steady 13.98 V; the Tesla Coil is a ring-down burst that is genuinely
neither periodic nor stepped; the X-Y Plotter idles at 1e-10 V of solver residue. *Plan: leave.
Tightening means tuning thresholds until the classifier agrees with one particular set of
circuits, which is how a check stops meaning anything. Revisit only if a fourth appears - that
would say the rule is wrong rather than these three being marginal.*

**6. CCM vs DCM.** The hard one, and the only item whose blocker has actually moved. See its own
section below: it was blocked because a realistic snubber could not be resolved at dt = 100 ns,
and the note says what it needs is "a switch model with a defined off-state capacitance, or a
local time-step control that tightens dt across a commutation". Since then `MIN_TIME_STEP` became
10 ps and the simulation refines its own step once a circuit shows a period. *Plan: one measured
retry - place the DCM buck, watch the switch node across a commutation, and find out whether the
finer floor alone is enough before building anything.*

Not on this list because they are feature work rather than defects: S-parameter (`.s2p`) import
via rational fitting, behavioural DC-bias capacitance, and the Bode-on-an-imported-model check.

## Vendor / SPICE model import - FIRST VERSION SHIPPED 2026-08-28

`.SUBCKT` import is in: `src/spice.c` reads the passive subset of a manufacturer's netlist and
turns each subcircuit into a library entry, so an imported model is placed and solved like any
block. Supported: `.SUBCKT` / `.ENDS` with a port list, R / L / C instances, `X` instances of
another subcircuit in the same file (nested), `+` continuation lines, `*` and `;` comments, and
the value suffixes including the MEG-is-mega / M-is-milli trap. Anything else is reported and
skipped rather than guessed at. Load one with `--import-spice <file>`.

`--spice-test` imports a vendor-style ceramic model (C in series with its ESR and ESL) and
measures its impedance across three decades against the hand calculation: **15.91 ohm at
100 kHz** (capacitive), **0.033 ohm at 19.02 MHz** (series resonance, so just the ESR) and
**0.442 ohm at 100 MHz** (inductive), plus two of them in parallel at **7.957 ohm** against
7.96. That is the vendor-curve comparison this entry asked for, as a regression test.

Still to do from the original scope: `.PARAM` expressions and parameterised subcircuits, a
"Import model..." button in the properties panel (it is CLI-only for now), semiconductor
`.MODEL` cards, and S-parameter (`.s2p`) import.

## Original scope (2026-08-24)

Goal: load manufacturer models — Murata SimSurfing exports, TDK, Würth, KEMET, Samsung — for
capacitors, inductors and other passives so simulations use measured impedance behaviour
(ESR/ESL, DC-bias capacitance loss, self-resonance) instead of the built-in first-order
parasitics.

Scope for a first version:
1. **`.SUBCKT` importer** for the passive subset of SPICE: `R`, `L`, `C`, `K` (coupling),
   nested `.SUBCKT`/`.ENDS`, `.PARAM`, unit suffixes (`MEG`, `k`, `m`, `u`, `n`, `p`, `f`),
   `+` continuation lines, `*` comments. Expand into the existing `SubCircuitDef` / IC-block
   mechanism so an imported model is placed like any other subcircuit.
2. **Model library UI**: "Import model…" in the properties panel of a capacitor/inductor,
   remembering the file in the saved circuit (path + embedded netlist for portability).
3. **Frequency-domain check**: run the Bode tool on an imported model to display |Z| vs f and
   compare against the vendor's published curve (SimSurfing shows this directly).
4. Later: S-parameter (`.s2p`) models via rational fitting; behavioural DC-bias capacitance
   (`C(V)` tables); temperature tables.

See `docs/RESEARCH_SIMULATORS.md` for the survey of formats and of how other simulators
(CircuitJS, LTspice, ngspice) handle integration, convergence and model import, which should
drive the engine improvements (trapezoidal integration, LTE timestep control, junction
voltage limiting, op-amp macro-model).

## Named part models - DONE 2026-08-28

Shipped: 20 devices (2N7000, 2N7002, IRF540N, BS250, 2N3904, BC547B, 2N3906, 1N4148, 1N4001,
1N4733A, X5R 10uF, C0G 10nF, Alu 100uF, LM358, LM741, TL072, MCP6001, LM317, LM7805, TL431),
each with its data sheet parameters, a Part row in the properties panel, the part number on the
canvas symbol, and `--part-test` rebuilding each one's own data sheet test condition.

Still worth adding: an NE555 (needs a subcircuit, not a parameter set), a 78xx family beyond the
7805, logic-level power parts (IRLZ44), and a way for Spotlight to place a named part directly
rather than placing the generic symbol and then picking the device.

## Unmodelled properties - CLEARED 2026-08-28

Everything on the previous list now stamps: capacitor ESR / ESL / leakage and DC-bias
capacitance loss, source series resistance, the current source's shunt resistance, the ideal
diode, and the op-amp's GBW, slew rate, offset voltage, bias current, CMRR, input resistance,
output resistance and rail-to-rail flag. Each is gated on `ideal == false`, and the capacitor's
starting voltage is an initial condition rather than a dead field. See TEST_PLAN 3.20 and 3.22.

What is left is a note rather than a gap: the resistor's temperature coefficient is honoured
only when `ideal` is false, which the panel does not say; and the DC-bias model is the effective
capacitance at the operating point rather than a charge-conserving Q(V) integration, which is
the right shape for teaching but would need the vendor-model machinery above to be exact.

## Cuk converter - mostly resolved 2026-08-28

It is now pinned at 12.69 V against the ideal 12 (it was 15.0, 25 % high). The diagnosis worth
keeping: it was never a discretisation artifact - a 5x finer time step changes the answer by
less than 1 % - it simply had not settled. The output filter alone was a 9 ms time constant and
the transfer capacitor's DC level balances slowly on top of that, so a 10 ms run was measuring
the startup. The output capacitor is now 100 uF and the oracle gives it 20 ms.

Pre-charging the transfer capacitor to its theoretical V_in + |V_out| = 24 V makes the answer
WORSE (15.5 V), which is the real remaining issue: nothing in the loop forces the volt-second
balance quickly, so where that capacitor's DC level lands still depends on where it started.
Worth revisiting with a switch that has a real R_on and a diode with a finite recovery, both of
which add the loss that would pin it.

## Built-in parts that are circuits - first one shipped 2026-08-28

The NE555 is a subcircuit definition registered by the template that uses it: three 5k divider
resistors, two comparators, a NOR latch and the discharge transistor. It oscillates at 4818 Hz
against 4800 on the page. The pattern generalises - anything that is a circuit rather than a
parameter set can ship this way now that subcircuits simulate.

Worth doing next in the same shape: a 741-level op-amp macromodel (input pair, gain stage,
output stage) so its behaviour comes from its topology instead of the GBW/slew parameters; an
optocoupler; and a 7-segment decoder. The 555's RESET and CONTROL pins are not exposed yet.

## Crystal (Pierce) oscillator - shipped (2026-08-28)

Shipped as CIRCUIT_PIERCE. Two things had to be true. First, COMP_CRYSTAL integrates its motional
arm with the trapezoidal rule (theta = 0.5) inside the component rather than through the general
theta = 0.6 path, which was damping a Q of 314 away at the time steps these templates run at.
Second - and this is the part that took the longest - the holder capacitance Cp had to come down
from 1 nF to 33 pF. At 1 nF the arm is shunted hard enough that the loop either locks onto the
parallel resonance (110.8 kHz, 1.6 V) or rings down and dies. A real HC-49 is 3 to 30 pF; the old
value was three decades out. It now runs 30 V pk-pk at 100.6 kHz, just above f_s, pulled there by
the 4.7 nF load caps. --xtal-test measures |Z| at, below and above resonance and the off-resonance
ratio; --osc-test checks the frequency.

## CCM vs DCM - SHIPPED 2026-08-30; the blocker does not reproduce

The note below said an asynchronous buck in DCM ran away to hundreds or thousands of volts, and
that the fix needed a switch model with a defined off-state capacitance or a local time-step
control across the commutation. **Retried with measurements, and it does not reproduce.**

`--dcm-test` runs nine configurations: loads from 6 ohm to 20 k, the app's own step and forced
steps of 100 ns and 1 us, the freewheel diode's junction capacitance switched off, and the whole
thing again with ideal parts - the diode a hard switch and the analog switch at r_off 1e9, which
is the configuration the note describes. No runaway in any of them. The inductor current is
measured rather than inferred, so the mode is proved rather than assumed: 0.198..1.62 A at 6 ohm
never reaches zero, and every lighter load sits at zero for part of each cycle.

Two plausible causes were tested and neither is the answer: the time step (it works at 1 us, a
tenth of the switching period) and the junction capacitance stamped the same morning (it works
with cjo forced to zero). Whatever caused the runaway has been fixed by one of the stamp
corrections since. The original note is kept below, because a diagnosis that turned out to be
wrong is worth keeping visible.

The template exists now: the same 12 V buck twice, 100 kHz, 50 % duty, identical part values, one
into 6 ohm (5.44 V, continuous) and one into 30 ohm (8.37 V, discontinuous). Both numbers are
measured rather than predicted - the first draft used a 2 k load labelled "~9 V" from a 400 us
run, and that load actually settles at 11.89 V, because 400 us is a twentieth of its RC.

## The original note (2026-08-28)

An asynchronous buck in discontinuous conduction has a third interval where neither the switch nor
the diode conducts. With an ideal analog switch (r_off 1e9) that leaves the switch node undefined:
it runs away to hundreds or thousands of volts, and every attempt to define it made it worse - a
snubber capacitor small enough to be realistic cannot be resolved at dt = 100 ns (0.8 A into 2.2 nF
is 36 V per step), one large enough to be resolvable is not a snubber any more, and a leakage
resistor gives the interrupted inductor current a 100 k path to push against. What it needs is
either a switch model with a defined off-state capacitance, or a local time-step control that
tightens dt across a commutation. Until then the DCM lesson lives in the CCM vs DCM notes of
Discrete Buck, Node by Node rather than in a template of its own.

## Terminal currents and displacement current - measured 2026-08-30

The three circuits that cannot close their node KCL are Pierce, Discrete Buck Node by Node and
Pull-up Sizing, and `--flow-test` now says how far out each one is - both at the node and across
the whole net, because those are different faults. Measured with the exemptions lifted:

| circuit | node disagrees by | net is out by | what is on the node |
|---------|-------------------|---------------|----------------------|
| Pierce Crystal Oscillator | 0.85 uA | 6.8 uA | one resistor terminal |
| Discrete Buck, Node by Node | 3.9 uA | 15.4 uA | one Schottky terminal |
| Pull-up Sizing | 1.3 uA | 6.4 uA | one capacitor terminal |

The net being out by more than the node settles what is happening. Wire currents are the
minimum-norm solution of a net's conservation equations with the terminal currents as its
demands, so current that no terminal reports does not vanish - it is spread across the net and
lands hardest on whichever node has least of its own. The node named in the failure is a symptom;
the missing current is somewhere else on the net.

The cause of the largest one was that the diode had a junction capacitance in its *properties*
(`cjo`, 1 pF signal, 5 pF Schottky, 50 pF varactor) that **nothing read**. It was a default value
with no stamp behind it, so a reverse-biased junction carried no displacement current at all.

**Stamped 2026-08-30, and the first version of it was wrong.** The diode and the Schottky stamp
their junction capacitance now. Backward Euler deliberately - a trapezoidal companion needs the
branch current from the last accepted step, and that is state which would be advanced once per
Newton iteration rather than once per step.

**Corrected 2026-08-30 (v3.22.4).** As first written the companion was stamped with the current
source the wrong way round: `-Ieq` at the anode where an ordinary capacitor, and the crystal's
holder capacitance, both use `+Ieq`. That does not model a capacitor. The branch carried
`C(v + v_prev)/dt` instead of `C(v - v_prev)/dt` - not a memory of the charge but an injection of
it, roughly twice the displacement current, in phase with the voltage rather than with its
derivative.

Two claims were made for that version and both were artefacts of the sign:

- *"The buck closes: 15 uA accounted for."* It does not. The gap reappears the moment the sign is
  right, unchanged at 15.4 uA, because a reverse-biased Schottky at the sampled instant carries
  almost no displacement current - `v - v_prev` is nearly zero between switching edges. What had
  closed the node was a spurious current large enough to cover the discrepancy. The real gap is
  1 ppm of the 3 A that the switch node carries between the MOSFET and the inductor: Newton slack
  in a large cancellation, not missing physics. The flow test's own tolerance already reasons this
  way, but it sized the cancellation per node id, and those 3 A terminals sit on neighbouring ids
  of the same merged net. Scoped to the net, the buck passes on its merits and Pierce and Pull-up
  Sizing still fail, which is the point.
- *"The Function Generator's shaped sine moved from 0.324 to 0.302, and it is real."* It was not
  real. With the sign right the measure reads 0.324, exactly where it was before the capacitance
  existed, and the expectation has been put back. A picofarad has megohms of reactance at 5 kHz;
  it cannot bend a diode shaper's corners, and the reasoning that said it could was written to
  explain a number rather than to predict one.

The lesson worth keeping: KCL closing proves nothing about a stamp's sign. Terminal currents are
recovered by re-stamping each device alone and reading its residual, so the report agrees with
the stamp whatever the stamp says. A sign error is invisible to every conservation check in the
suite and shows up only in a waveform - which it did, and which was explained away.

## Two questions nothing was asking (2026-08-31)

After the interview-template pass, two audits went in for questions the battery had never put to
the whole library rather than to one template at a time.

**Does any template's answer move when the step is refined?** `--conv-test` runs all 188 at the
step the app picks and again eight times finer. 177 agree within 3 %; eleven are still building at
the end of their run and are reported and not compared, because two steps compared during a
start-up ramp say nothing about the steps. This is the general form of the question that found the
Hot-Plug Inrush sag reading 11.33 V where it is 10.02 - found by hand, on one template, having
noticed it by accident.

Two things had to be right before it said anything true. Judging a signal's MEAN against itself
failed a 28 kV three-phase bus whose mean moved from -0.77 V to nothing while its peak-to-peak
moved 0.3 %: a hundred per cent of almost zero. And a 2 % bar failed Colpitts, whose limit-cycle
amplitude measures 31.95 V at dt = 2 ns and 32.07 V at 500 ps - converging, slowly, because where a
soft-limited oscillator settles depends a little on numerical damping. The bar is 3 %.

**What happens to a value a person actually types?** `--stress-test` puts zero, 1e3, 1e6 and 1e-12
into every editable value on every template - 5164 runs - and asks only that the result is not a
NaN. A clean refusal passes: the refusal is not the fault. A very large voltage is reported and not
judged, because it is usually right - 4-Wire Kelvin Sensing forces 1 A through whatever it is given,
so at a megohm it produces a megavolt and should.

It found that an analog switch with a zero on-resistance - an ideal closed switch, a reasonable
thing to want - stamped `1.0/R` unguarded, put an infinity in the matrix, and returned NaN for every
node in the circuit, on five templates. Six stamps divided by a resistance with no floor; the
ammeter stamp had already floored its own at 1e-9 ohm for the same reason, and that number is now
shared. The SPST switch's on-resistance was also the one value row in `input.c` that assigned
without testing its input.

### What this does not cover: values that arrive from a file

The panel now bounds what it accepts - positive, and no more than a megafarad or a megahenry. A
save file edited by hand, or a SPICE import, does not go through the panel, and the solver still
comes apart on what such a file could carry: measured, **16 templates run away on a negative
inductance and 12 on 1e9 F**. Nothing crashes and no NaN escapes now that the conductance floor is
in, but the numbers are meaningless.

The fix is validation on load, in one place both paths reach, and it is deliberately not bundled
into the change that bounded the panel: that one is small and provable and this one needs its own
decision about what a loader should do with a value it will not accept - refuse the file, clamp the
part, or drop it.

## The interview templates, checked against their own numbers (2026-08-30)

The nineteen interview-prep templates are the ones a person opens to check an answer before a job
interview, so a number printed beside the circuit that the solver does not reproduce is worse here
than anywhere else in the library: it is wrong in the one place someone is going to repeat it out
loud. Nothing was checking them. `--class-test` said they converge, `--geom-test` said they are laid
out, `--flow-test` said charge is conserved, and none of that notices that the text beside the
circuit claims a pin sits at 4.0 V while the solver puts it at 3.85.

`--iv-test` now takes 26 numbers off those nineteen canvases and measures each one twice - at the
step the app itself would use, and again eight times finer. **All 26 reproduce.** Kelvin's 110 mV
against 10 mV, Crosstalk's 3.3 x 2/7, the 2N3904's 0.07 V beside the 2N7000's 0.41 V, Bootstrap's
23.5 V, and the two-capacitor problem settling at exactly 5.000 V on both sides.

What it found was not wrong physics. It was three other things.

**Four numbers on the canvas were wrong, and one was a typo of a different note.** The ESD template
said the pin clamps at 4.0 V with 2.7 mA into the rail; 2.7 mA is (6 - 3.3)/1k, computed as though
the pin sat *at* the rail, in the same sentence that says it does not. The solver says 3.85 V and
2.1 mA. The 220 k copy said 12 uA by the same slip and injects 6.7 uA, a third of which goes to the
1 M input rather than the rail. The Miller template's one-line description said 10 pF becomes 110 pF
while its notes and its own on-canvas label both said 130 pF - the notes were right, gain is 12. The
discrete buck said 5.5 V out and delivers 6.0 V. And the Crosstalk note began a line with an orphan
"nanosecond.", left behind from the Ground Bounce note next to it.

**Six templates were not showing the comparison they are about.** These are all "the same question
answered two or three ways, side by side", and the auto-probe places one channel on the source and
one on the output - which on a comparison lands both channels in the *same* copy. The Two-Capacitor
Problem probed the 1 ohm copy, whose transfer is over in 100 us on a 50 ms screen, and drew two flat
lines. Hot-Plug Inrush did the same with the unlimited copy. Worst of all, 4-Wire Kelvin Sensing had
CH1 parked on the current source's grounded return - a permanent 0 V - so the 110 mV two-wire
reading, which is the entire point of the template, was never on the scope at all. That last one was
a bug rather than an omission: the code picking the source's live terminal compared a ground
component's node id against the source's, and a template that reaches ground through a wire has two
different ids on one net, so the test never fired.

**And one number was not converged, which turned out to be a fault in the step rule itself.**
Hot-Plug Inrush's rail sag moved 8.2 % when the step was divided by eight, so what the canvas showed
was partly an artefact. The cause was general: `simulation_accuracy_time_step` picked the step from
**source periods and pulse widths only** and never looked at a circuit's own RC time constants.
Inrush charges 1 mF through 50 mohm - a 50 us event - with no source anywhere near that fast, so the
step came from the display rule at ~125 us and stepped straight over the thing the template is named
after.

It now also takes ten samples across the fastest RC it can find. Getting that in without wrecking
everything else took three attempts, and the wrong ones are worth recording:

- **Applied to every circuit, it is unaffordable.** A non-ideal source carries a default 1 mohm, so
  a power supply's 1000 uF against it is a 1 us time constant, and resolving that on a 60 Hz
  template means a 100 ns step where 333 us would do. The battery went from 224 seconds to over
  fifteen minutes and was still running when it was killed. It is now applied only where **no
  source sets the pace** - when one does, the source and display rules already resolve what is on
  screen, and the fast natural transient is a start-up artefact nobody is looking at.
- **Applied to oscillators, it breaks them.** Colpitts, Hartley and Pierce have no periodic source
  either, so they landed in the same branch; a 1 M bias resistor touching the tank made "tau" tiny
  and collapsed the step to the floor, and they stopped starting. The rule now returns nothing at
  all for a circuit containing an inductor, a crystal or a transformer: R times C is the scale a
  capacitor settles on and is simply not the scale an LC tank does anything on. A resonant
  circuit's own measured period was already the better answer and is what it keeps.
- **The resistance had to include the parasitics.** The first version scanned for `COMP_RESISTOR`
  and found only Inrush's load, because the "50 mohm of connector" is an analog switch's `r_on` and
  the supply's is a source `r_series`. Those parasitics *are* the circuit here - they are the only
  thing between 12 V and an empty capacitor.

The result: 48 of 48 suites pass in 205 seconds against a 224 second baseline, so the rule costs
nothing measurable. Two numbers changed, and both were wrong before. The rail sag is **10.02 V, not
11.33** - 200 A through the supply's own 10 mohm is 2 V, which is what a converged solve says and
what the coarse step was hiding. And `--probe-test` expected the bulk capacitor to reach 12.65 V,
"overshooting the rail": an RC circuit cannot overshoot, and that 12.65 was the step stepping over
the transient. It charges to 11.99 and stops.

The annotation was wrong too, in the same direction: it said 12 / 0.05 = 240 A, forgetting the
supply's own 10 mohm. It is 12 / 0.06 = 200 A.

## What a review found that the suites did not (2026-08-30)

The v3.23.0 diff was reviewed before publishing, by five independent readers each with a verifier
whose job was to refute what it found. **Twenty-three survived refutation, and all twenty-three are
fixed.** No suite in the battery saw any of them, which is the interesting part; where a suite
should have, the suite was extended rather than the finding waved off.

*(This section said "two" for a day. The notification carrying the result was truncated at the two
findings I happened to read first, and I wrote the count down without checking the run. The number
was wrong in the direction that flatters the release, which is the direction to distrust.)*

Two of them were the fault this release was about - state that belongs to a time step, touched
somewhere that is not a time step - and they get the long write-up below. Then the other twenty-one,
short.

**The battery emptied itself during the DC operating point.** Its coulomb count lived inside
`component_stamp`: `charge_coulombs -= |I| * dt`. The operating point stamps with `dc_dt = 1e9`,
the pseudo-step that makes a capacitor look like an open - so a default AA across 100 ohm lost
0.015 A x 1e9 s of charge before the first transient step, and **every Run began with a flat
battery reading 0.72 V instead of 1.5 V**. It also ran once per Newton iteration and once more
every time the current-flow display read a terminal current back. Moved to the per-step advance in
`simulation.c` with the motor and the relay.

Why nothing caught it: `--restamp-test` names the battery as one of the parts it is looking for,
but it compares a node voltage over milliseconds, where a real discharge is far below its
threshold. The part had no properties panel until this release either, so nobody could place one
and watch it die.

**The solve-time snapshot did not reach inside subcircuit blocks, and that was a regression
introduced with the snapshot itself.** A block's internal parts live on its own instance array
rather than on `circuit->components`, so their `cap_vc_solve` stayed zero for ever while
`subcircuit_advance_caps` moved the real state every step. A read-only re-stamp therefore treated a
charged internal capacitor as empty: a block's reported pin current came out **94x wrong**. Before
the snapshot existed the same read used `cap_vc` directly - one step stale, but broadly right.
`snapshot_companion_state` recurses now, nested blocks included.

Why nothing caught it: `--restamp-test` detects *mutation* - a stamp that writes when it should
only read - and this is a *misread*, which changes nothing about the circuit. `--sub-test` had a
pin-current case, but its block was resistive, and its RC case checked a node voltage, which is
right either way.

Both now have checks that provably fail without the fix: `--state-test` (the battery is still full
after its own operating point) and a third `--sub-test` case (a block that is one capacitor draws
C dv/dt through its pin - 9369 % out with the bug, 1 % with it). The second of those took two
attempts: the first version reused the shared DC drive, and the operating point charges the
capacitor to the supply, after which dv/dt is nearly zero and reading the companion as zero changes
almost nothing. It passed with the bug still in. A check that cannot fail is not a check, and the
only way to know is to put the bug back and watch.

### The other twenty-one

**Wrong arithmetic, silently (5).** The fuse accumulated i2t inside the stamp, so its blow energy
was multiplied by the Newton iteration count - measured at exactly 2x on a DC circuit with the
current-flow display on. The step budget computed a double up to 1e14 and cast it to a 32-bit
`long`: undefined, and on x86 it lands on INT_MIN, which the `< 1` clamp below turned into one step
per frame, while the new keeping-up indicator compared progress against that same target of 1 and
reported a confident 100 %. The FFT transformed `values[0..N)` of a 10000-sample history held
oldest-first, so the spectrum on screen belonged to a second ago rather than to the trace beside it.
The FFT never removed DC, and a Hann window smears a DC term into bins 0 and +/-1, so the
fundamental search - which starts at k=1 precisely to skip DC - landed on the leakage: every probe
sitting on a rail reported its fundamental as one bin, with THD and SNR computed against that. The
relay's coil current was left at zero by the operating point, because the advance that maintains it
runs only on an accepted transient step - a DC answer that contradicted its own solve, and the
initial condition the transient then started from.

**The panel disagreed with the circuit (6).** `PROP_CJO` wrote `props.diode.cjo` for a Schottky,
which in that union is `props.schottky.ideal` - typing a junction capacitance silently toggled the
model. Three types gained a value row this release but were missed in the pre-fill lookup, so the
edit box opened empty and Enter on an untouched field parsed `""`. Five rows were inert: the
centre-tapped transformer's model toggle and winding resistances (its stamp reads only the turns
ratio), primary inductance and coupling on the two-winding transformer, a light level with no
reader, `CS_RIN` on the two controlled sources that have no input resistance, and Ideal toggles on
the motor and controlled sources. The scope's cursor overlay inverted the wrong mapping: it read the
manual offset and an unfitted scale, while the trace is drawn with the fitted centre and shift the
renderer records per channel - so the markers named the wrong volts whenever AC coupling or Fit was
on, which is the default for a stacked template. `simulation_reset` had no case for the relay or the
DC motor, so their state survived a reset.

**Documentation that had drifted from the program (10).** `--restamp-test`'s header claimed it had
caught three faults it structurally cannot catch: it detects a stamp that *writes* while being read,
and a stamp that *misreads* - the crystal, the subcircuit snapshot, an inverted sign - is consistent
with itself and agrees with its own second run. That comment is now explicit about the boundary and
names the suites that do cover the other side. The README's template list omitted CCM vs DCM and
quoted a class-test split the tool no longer produces; `TEMPLATE_AUDIT.md` and `docs/STATUS.md` both
still described CCM vs DCM as deliberately absent and blocked, a day after it shipped; and this
section reported two findings out of twenty-three.

**A check that could not fail (1).** The DC case added to `--fft-test` to lock in the DC-removal fix
passed with the fix reverted. It set the window to rectangular, where a DC term produces exactly
zero leakage - so it tested a configuration in which the bug does not exist, while the app defaults
to Hann. Fixed to use Hann, and confirmed the only way that means anything: with DC removal disabled
the case now reports the fundamental of a 5 kHz sine as **100 Hz**. That is the second time this
release a new check had to be re-run against the restored bug before it was worth anything, and the
second time it was worthless on the first attempt.

## Reading a circuit must not change it

Four faults found on 2026-08-30 were the same fault wearing different clothes: a component's
companion state read or written at the wrong moment. The diode's junction capacitance stamped with
an inverted sign, the crystal re-stamped with the next step's state, the MOSFET's gate capacitance
advancing once per Newton iteration, and the relay's coil doing the same. Finding them one at a
time is not a method.

`--restamp-test` is the invariant behind all of them, and it needs no oracle and no physics. Every
component's terminal current is recovered by re-stamping it alone with `g_stamp_read_only` set,
which says "this stamp is being read, not solved". So: run a circuit twice, identically, and update
the current-flow display on one of the runs. If a component writes its state while being read, the
two runs diverge, and which template or which part diverged says where.

It runs both ways round, because neither alone is enough:

- **187 templates**, twice each. Clean.
- **121 component types**, each on a source-part-load circuit of its own. This pass exists because
  a DC motor, a relay, a battery and a fuse appear in **no template at all**, and all four are
  among the parts that write their own state inside their stamp. The relay failed it - its coil
  current and its armature both moved when the display asked what the current was - and is fixed.

Two things it uncovered, both since fixed:

1. **The relay's coil current** advanced once per Newton iteration, and it was measured before it
   was fixed, the same order the motor's took. `--dvdt-test` switches the default coil (200 ohm,
   100 mH) onto its 12 V supply and times the current to 63 % of V/R against `tau = L/R = 500 us`,
   worked out outside the solver. It read **10.5 us** - 48 times too fast, so a relay pulled in
   with essentially no delay and every delay circuit built on one would have been a lie.

   **Fixed 2026-08-30.** The coil current advances once per accepted step in `simulation.c`, and so
   does the pull-in/drop-out decision, because energizing is a decision about a step's converged
   current, not about a Newton iterate. The stamp only reads, through the solve-time snapshot. It
   now times at 500.5 us against 500 us, and with the relay runnable, `--restamp-test` covers all
   122 component types clean.
2. **The DC motor** did the same, and it was measured before it was fixed. `--dvdt-test` spins one
   up from rest on a 10 V supply and times it to 63 % of its final speed, against
   `tau = J / (b + kt kv / R)` worked out outside the solver. The oracle is 99 ms. It read
   **49.5 us**, which is one time step: each Newton iteration advanced the rotor, and Newton stops
   iterating when nothing is changing any more - which for a motor is its steady state. A time step
   did not advance the motor by dt, it drove it to wherever it was heading.

   The final speed was exactly right, 990.099 rad/s against 990.099, which is why nothing had ever
   noticed: `d_omega` is zero at steady state however many times it is added, so every check that
   looks at where a motor ends up was satisfied. Only timing the getting-there could see it.

   **Fixed 2026-08-30.** The rotor and the armature current advance once per accepted step in
   `simulation.c`, next to every other companion, and the stamp only reads them - through the
   solve-time snapshot, so reading a terminal current does not see the next step's rotor. It now
   times at 0.0990099 s against 0.0990099 s.

The general point for the next component: **companion state belongs to a time step, not to a
stamp.** A stamp runs many times per step - once per Newton iteration, plus once more whenever the
display reads a current back - so anything advanced inside one advances at a rate that has nothing
to do with the clock.

## A suite that picks its own time step is testing a different program

`--osc-test` is the oracle for every oscillator in the program: what frequency it runs at, how big
its swing is, what shape its output has. It ran every one of them at a flat microsecond.

The app does not run at a flat microsecond. It takes the accuracy step and then the scope's rule if
that is finer, which for a self-oscillator used to mean about twenty samples a cycle. So for as
long as the Function Generator was **displayed at 5556 Hz**, this suite **verified it at 5000 Hz**
and passed - because 5000 Hz is what a microsecond step gives. The oracle was right about a program
nobody was running.

Fixed 2026-08-30: `--osc-test` now takes the step the app would take for that template. Every case
passes at it, so none of them needed the fixed step they had; `--osc-dt` remains for asking whether
an expectation is converged.

The general shape is worth keeping in mind when adding a suite:

- If a check is about **what the program shows**, it must run the program's own step. Anything else
  measures a different program that shares source code with it.
- If a check is about **physics** - `--line-test` builds a transmission line and samples it at a
  fortieth of its own propagation delay - then a step chosen for the measurement is right, because
  there is no display path in the question.
- `--class-test` exists to catch the first kind going wrong: it runs at the app's step and again
  finer, and reports anything whose answer does not survive.

## The time step was the answer for five templates; three are fixed

`--class-test` runs every template twice, at the step the app itself picks and at a finer one, and
asks whether it comes back the same circuit. Five say no. This is not a test artefact: the finer
runs agree with each other down to dt/64, so there is a converged answer and the app is not showing
it.

| template | period at the app's step | converged | out by |
|---|---|---|---|
| Triangle/Square Gen | 0.18 ms (5.56 kHz) | 0.2 ms (5 kHz) | 10 % |
| Function Generator | 0.18 ms (5.56 kHz) | 0.2 ms (5 kHz) | 10 % |
| Ring Oscillator | 8.0 us (125 kHz) | 7.25 us (138 kHz) | 9.4 % |
| Hartley (MOSFET) | reads periodic | reads stepped at dt/4 | class |
| Clapp (MOSFET) | reads periodic | reads stepped at dt/4 | class |

The first three shared a mechanism, and it is fixed. Their period is not set by an RC or an LC but
by *when a threshold is crossed* - a comparator flipping, an inverter chain propagating - and the
step quantised that crossing. None of them has a source with a frequency, so nothing in the netlist
said how fine the step had to be and it fell through to the display's rule, about twenty samples a
division. Twenty samples a cycle draws a waveform perfectly well and times one badly.

**Fixed 2026-08-30.** A circuit with no source still has a frequency; it just cannot be known until
the circuit has run. So the simulation now looks at itself: every 600 accepted steps it
characterises its own probes, and if it turns out to have a period, it holds the step to the
hundred samples a cycle a circuit with a real source would have been given. Only ever finer, at
most twice, and it keeps looking until there is something to measure - a crystal takes a thousand
times longer to start than a comparator, and giving up on the first flat history would have left
exactly the slow oscillators unrefined.

| template | was shown | now | converged answer |
|---|---|---|---|
| Triangle/Square Gen | 5.56 kHz | 5.00 kHz | 5.00 kHz |
| Function Generator | 5.56 kHz | 5.00 kHz | 5.00 kHz |
| Ring Oscillator | 125 kHz | 138.9 kHz | 138.9 kHz |

The uncomfortable half of this finding is worth keeping written down: **the Function Generator's own
oracle passed at 5000 Hz the whole time it was being displayed at 5556 Hz**, because `--osc-test`
ran a finer step than the app did. A suite that runs its own step is not testing the program; it is
testing a different program that happens to share source code. `--class-test` exists to catch that
class of thing - it runs at the step the app itself picks and asks whether the answer survives
refinement.

The suspicion about Hartley and Clapp - that the coarse step might have been sustaining them, an
oscillator oscillating because of its own integration error - was checked and is wrong. Their
amplitude is 13.98 V and 2.355 V and does not move across a sixteenfold change of step; a
numerically sustained oscillation collapses when the step is refined, and neither does. What was
actually happening was in the classifier: it judged the whole record, start-up included, and an
oscillator building up has crossing intervals that genuinely disagree because it is genuinely
changing. It now takes a provisional period from the median interval, finds where the envelope
stopped changing, and judges the crossings after that - and reports amplitude and level for the
settled part too, which is what a reader means by them. Clapp reads periodic at every step now.

Three `--class-test` warnings remain, and none of them is a circuit fault:

- **Hartley (MOSFET)** still reads periodic at the app's step and stepped at a finer one. 188
  samples a cycle and a steady 13.98 V either way, so this is the 5 % interval-agreement threshold
  being marginal for it rather than anything about the circuit.
- **Tesla Coil** is a ring-down burst - a spark gap firing and the tank decaying - which is neither
  periodic nor stepped in any clean sense, and lands on either side of the line depending on the
  step.
- **X-Y Plotter (upload)** idles at about 1e-10 V of solver residue at the app's step and a little
  more at a finer one. A microvolt floor was added so residue is not mistaken for a signal, and it
  reads static at the app's step now; at a quarter of the step it still crosses the line.

Tightening these three further means tuning thresholds until the classifier agrees with a
particular set of circuits, which is how a check stops meaning anything. They are reported with
their numbers and do not fail the battery.

**Pierce closed 2026-08-30, and not for the reason written here.** The note above guessed that the
crystal's motional-arm current was going unreported and being left to the residual. It was not. The
crystal keeps its companion state on the component - `trap_i_prev` for the holder capacitance,
`cap_vc` for the motional arm - and that state is advanced when a step is accepted. Terminal
currents are recovered *after* that, by re-stamping each device alone and reading its residual, so
the re-stamp reproduced a stamp that never happened: the right voltages against the next step's
state. The difference was the 6.8 uA, and it was the flow display's to lose.

The fix is the shape the MOSFET's gate capacitance already used: each step records the state its
own solve is about to use (`trap_i_solve`, `cap_vc_solve`), and a read-only stamp reads that
instead. Taken at the top of the step rather than just before the advance, so a re-stamp *during*
a step - which the step-rejection path does - sees this step's state and not the last one's.
Pierce is audited again and `--flow-test` is 187/187 with one exemption left.

**Pull-up Sizing closed 2026-08-30, and `--flow-test` now has no exemptions and no skips at all.**

The suspicion recorded here - that the MOSFET's gate capacitances advanced their companion state
inside the stamp, once per Newton iteration instead of once per accepted step - was measured and
was worse than suspected. A gate case was added to `--dvdt-test`: an NMOS held in cutoff, where
Meyer's model leaves only the overlap capacitances and they are constants, drain and source
grounded, gate driven by a known sine. The oracle is 12.6 uA. It read **-0.024 A**: two thousand
times too large, the wrong sign, and not a displacement current at all but the output of an
alternating recurrence, `I_eq(k) = 2 G V(k-1) - I_eq(k-1)`, which never settles. In plain terms a
non-ideal MOSFET holding a DC gate bias drew about `4 G V_bias` into its gate for ever.

Replaced with the stateless backward-Euler companion the diode's junction capacitance uses - the
previous accepted step read from the solution vector, nothing kept on the component, nothing to
advance at the wrong moment - and `+Ieq` on the gate, where it had been `-Ieq`. The gate now reads
1.25658e-05 A against an oracle of 1.25664e-05.

Three consequences:

- Pull-up Sizing's 3.6 % is gone. It was the same bug.
- The gate-node skip in `--flow-test` is gone. It existed because "a gate's displacement current is
  not reported as a terminal current", which was true, and was a bug rather than a limitation.
- `--flow-test` is 187/187 with every node of every template checked.

The general lesson, and it is the third time this pattern has appeared today: **both exemptions this
suite carried were written up as limitations of the current-flow display, and both were the same
kind of fault underneath - a companion read at the wrong moment.** An exemption is a place a bug can
hide indefinitely. It should be the last resort, and it should say what would settle it.

And the reason none of this was visible before: a terminal current is recovered by re-stamping a
device and reading its residual, so it agrees with the stamp whatever the stamp says. Conservation
checks cannot see any of these faults. `--dvdt-test`, which compares against arithmetic done outside
the solver, has now caught three: the diode's inverted sign, the crystal's stale state, and this.

Recovering a terminal current no longer disturbs the device it is recovered from: the display
re-stamps every component once a frame, and a stamp that remembers something - a MOSFET's gate
charge - was overwriting its memory of the step that actually happened with values worked out
from the linearisation. `g_stamp_read_only` says the stamp is being read, not run. It changed
none of the numbers above, which is worth knowing too.

## Terminal currents and displacement current (2026-08-28)

--flow-test audits node KCL by comparing wire currents against terminal currents. Terminal currents
are recovered by re-stamping the component and taking the row residual, and that recovers conduction
current only: the charge flowing into a MOSFET gate or a reverse-biased junction is real current in
the wires and appears in no terminal_current. On slow circuits it is under the noise floor; on a
hard-switched power stage it is tens of microamps and the audit cannot close. The test now skips
gate nets and bare wire corners on a MOSFET net, allows 0.5 % of a node's own current, and exempts
Discrete Buck, Node by Node from the node sum with a NOTE line. The real fix is to report the
capacitive terminal currents from the MOSFET and diode models.
