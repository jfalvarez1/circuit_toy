# Roadmap

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

## What a review found that the suites did not (2026-08-31)

The v3.23.0 diff was reviewed before publishing, by five independent readers with a verifier
against each finding whose job was to refute it. Two survived, and both were the same fault this
release was about - state that belongs to a time step, touched somewhere that is not a time step.
Neither was visible to any suite that existed, which is the interesting part.

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
