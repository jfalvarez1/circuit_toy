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

## Named part models as schematic symbols (requested 2026-08-28)

Add real parts - 2N7000, LM317, TL431, 1N4148, BC547, IRF540, NE555 - as palette items whose
properties are preloaded from the datasheet, drawn with their own symbol and labelled with the
part number. Each one needs: the parameter set (for the 2N7000: V_GS(th) 2.1 V typ, R_DS(on)
1.2 ohm at V_GS = 10 V, K from the transfer curve), a `--probe-test` oracle taken from a
datasheet operating point rather than from the simulator, and a `--param-test` entry checking
the model against two points on the published curve. The MOSFET curve-tracer templates
(#130, #131) already sweep three devices side by side and are the natural home for them.

## Remaining unmodelled properties (2026-08-28)

The ESR / ESL / leakage, source resistance, ideal-diode and op-amp GBW / slew-rate properties
now stamp (see TEST_PLAN 3.20). These are still editable but inert, and should either be
implemented or removed from the panel:

- **Op-amp**: input offset voltage, input bias current, CMRR, finite input and output
  resistance, the rail-to-rail flag. `r_out` in particular changes what a stage does into a
  heavy or capacitive load, so it is the most worth having.
- **Capacitor**: DC-bias capacitance loss (a class-II ceramic loses more than half its value at
  its rated voltage) - this is the one that surprises people, and it needs a C(V) table, which
  is the same machinery the vendor-model import above wants.
- **Resistor**: tolerance is used by Monte Carlo but not by the nominal solve, which is correct;
  the temperature coefficient is honoured only when `ideal` is false, which is worth a note in
  the panel.

## Cuk converter ripple (2026-08-28)

The Cuk template settles to the right mean (13.30 V against the ideal 12 V, and it moved much
closer when the inductor's theta-method history term was fixed - see TEST_PLAN 3.20.9) but its
ripple is still coarse: the transfer capacitor's current is reconstructed from a switch model
that has no on-resistance, so the corners of each cycle are sharper than the part could
manage. Worth revisiting with: a switch with real R_on, a finite diode recovery, and a smaller
time step tied to the switching period rather than the scope's time/div.

## Crystal (Pierce) oscillator - not shipped yet (2026-08-24)

`place_pierce()` in src/circuits.c builds an op-amp Pierce loop around a "teaching crystal" (Ls 100 mH,
Cs 25.33 pF, Rs 200, Cp 1 nF, f_s = 100 kHz, Q ~ 314) with a C2 / crystal / C1 pi network and an extra
1k / 2.2 nF output pole. A phasor scan (loop gain ~24 at 100.63 kHz) says it should oscillate, and it
does at dt <= 10 ns, but the theta = 0.6 integrator damps the motional arm enough that at the app's
time steps (100-250 ns) the loop gain falls below 1 (or the stage latches at gain 1000). Options:
pure trapezoidal (theta = 0.5) for L and C with a startup damping ramp, a dedicated crystal component
that stamps the motional arm analytically, or accepting a lower-Q "crystal" and a 20 kHz f_s.
Until then the template is not registered in the palette (hard rule: every template must demonstrate).
