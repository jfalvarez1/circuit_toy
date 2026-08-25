# Roadmap

## Vendor / SPICE model import (requested 2026-08-24)

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

## Crystal (Pierce) oscillator - not shipped yet (2026-08-24)

`place_pierce()` in src/circuits.c builds an op-amp Pierce loop around a "teaching crystal" (Ls 100 mH,
Cs 25.33 pF, Rs 200, Cp 1 nF, f_s = 100 kHz, Q ~ 314) with a C2 / crystal / C1 pi network and an extra
1k / 2.2 nF output pole. A phasor scan (loop gain ~24 at 100.63 kHz) says it should oscillate, and it
does at dt <= 10 ns, but the theta = 0.6 integrator damps the motional arm enough that at the app's
time steps (100-250 ns) the loop gain falls below 1 (or the stage latches at gain 1000). Options:
pure trapezoidal (theta = 0.5) for L and C with a startup damping ramp, a dedicated crystal component
that stamps the motional arm analytically, or accepting a lower-Q "crystal" and a 20 kHz f_s.
Until then the template is not registered in the palette (hard rule: every template must demonstrate).
