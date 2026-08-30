# Roadmap

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

## CCM vs DCM - not shipped (2026-08-28)

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

What is left, in the order that would help:

1. Report the crystal's motional-arm current as a terminal current rather than leaving it to the
   residual. That is Pierce, 6.8 uA.
2. The capacitor bus in Pull-up Sizing is 3.6 % out on its own current, which is the known
   flow-display limitation and the smallest of the three.

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
