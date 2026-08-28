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

## Crystal (Pierce) oscillator - not shipped yet (2026-08-24)

`place_pierce()` in src/circuits.c builds an op-amp Pierce loop around a "teaching crystal" (Ls 100 mH,
Cs 25.33 pF, Rs 200, Cp 1 nF, f_s = 100 kHz, Q ~ 314) with a C2 / crystal / C1 pi network and an extra
1k / 2.2 nF output pole. A phasor scan (loop gain ~24 at 100.63 kHz) says it should oscillate, and it
does at dt <= 10 ns, but the theta = 0.6 integrator damps the motional arm enough that at the app's
time steps (100-250 ns) the loop gain falls below 1 (or the stage latches at gain 1000). Options:
pure trapezoidal (theta = 0.5) for L and C with a startup damping ramp, a dedicated crystal component
that stamps the motional arm analytically, or accepting a lower-Q "crystal" and a 20 kHz f_s.
Until then the template is not registered in the palette (hard rule: every template must demonstrate).
