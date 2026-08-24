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
