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

## Terminal currents and displacement current (2026-08-28)

--flow-test audits node KCL by comparing wire currents against terminal currents. Terminal currents
are recovered by re-stamping the component and taking the row residual, and that recovers conduction
current only: the charge flowing into a MOSFET gate or a reverse-biased junction is real current in
the wires and appears in no terminal_current. On slow circuits it is under the noise floor; on a
hard-switched power stage it is tens of microamps and the audit cannot close. The test now skips
gate nets and bare wire corners on a MOSFET net, allows 0.5 % of a node's own current, and exempts
Discrete Buck, Node by Node from the node sum with a NOTE line. The real fix is to report the
capacitive terminal currents from the MOSFET and diode models.
