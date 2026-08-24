# Prebuilt Circuit Template Audit (47 templates)

Companion to `TEST_PLAN.md` §8. Every template gets the same five passes; the per-template
block adds the hand-calculated nominal, the **value variations** to try, and specific traps.

Values below were extracted from `src/circuits.c` `place_*` builders on 2026-08-24 — if a
template's BOM changes, update its block.

### Fixes applied 2026-08-24 (headless smoke test now reports 47/47)
Engine: BJT stamp was missing the collector Newton equivalent current and flipped the PNP
transconductance sign (every BJT template ran away to MV); PMOS had the same sign bug (CMOS
inverter); zener breakdown and TL431 were hard switches that chattered (now smooth models);
op-amp rails were not modelled for the ideal op-amp and the real op-amp's post-solve clamp
indexed the wrong solution entry (now stamped as rail voltage sources inside Newton);
LED display current used "the first series resistor" instead of the diode equation.
Templates: Common Source / Source Follower had no DC gate bias (divider added); Differential
Pair had no base DC path (direct-coupled, 6 V-biased inputs); Push-Pull's VEE wire ran through
Q2's emitter (rebuilt, PNP rotated 180, VEE = −12 V); Wien's Rg grounded the + input and had no
start-up kick (Rg moved, Rf 22k, pulse kick); Window comparator tied two op-amp outputs together
(10k summing resistors); Zener reference was forward-biased (rotation fixed); Wheatstone's
source terminal collided with a routing corner (source moved). Blocks below are updated.

## Standard passes (run on every template)

| Pass | What | Pass criteria |
|------|------|---------------|
| **L** Load | Load from Circuits menu with canvas empty, then with a circuit already present, then while sim running | Placed centred in view; no overlapping symbols/labels/wires; all terminals connected (status-bar node count = expected); no dangling decoupling cap / unconnected supply |
| **N** Nominal | Run as-is, probe output, read scope measurements | Matches hand-calc in block below (±5 %, ±10 % for transistor circuits) |
| **V** Variations | Apply each variation in the block **(a) before Run** and **(b) live while running** | Result tracks the new hand-calc; no NaN, no convergence error, no time reset, waveform continuous at change instant |
| **M** Model matrix | Toggle `ideal` per component (see matrix) | Realistic results differ from ideal in the *expected direction* and by a plausible amount; never diverge/oscillate spuriously |
| **T** Time base | Run at dt = auto, 1 µs, 100 µs, 10 ms; time/div at 3 settings around signal period | Shape stable; document where dt aliasing is expected (dt·20 > period) |
| **S** Save/Load | Save `.ckt` + JSON, reload, re-run N | Identical readings; `ideal` flags and probes preserved |

### Model matrix (pass M) — run all five on every template that has an active device
| Config | Passives (R/L/C/xfmr) | Semis (D/LED/Z/BJT/MOS) | Op-amps / sources |
|--------|----------------------|------------------------|-------------------|
| M0 | ideal | ideal | ideal |
| M1 | real | real | real |
| M2 | ideal | real | ideal |
| M3 | real | ideal | ideal |
| M4 | ideal | ideal | real (`COMP_OPAMP_REAL` swap / `ideal=false` on sources ⇒ Rint) |

Expected realistic deltas to look for: cap ESR → ripple step & reduced peak; inductor DCR →
lower Q, DC offset; source Rint → droop under load; diode Shockley → Vf rises with I; BJT VAF →
finite output R (gain a bit lower); MOSFET λ → same; real op-amp → finite gain error
(gain/(1+gain/A)), GBW roll-off, slew limiting on square edges, output R droop, rail clamp
(post-solve clamp in pending diff — verify node V and VCVS var agree, §3.6.2 in TEST_PLAN).

**Mixed-model traps to watch:** ideal source + ideal cap in parallel (Rint=0 into a cap →
impulse current, NaN); ideal diode (fixed Vf) in a loop with ideal op-amp (no series R →
non-convergence); ideal switch/relay shorting ideal source; realistic transformer (leakage L)
with ideal rectifier diodes → ringing.

---

## Basic

### 1. Voltage Divider — 10 V, R1=10k, R2=10k
- **N:** Vout = 5.000 V; I = 0.5 mA.
- **V:** R2→1k ⇒ 0.909 V · R1→0 ⇒ 10 V (zero-ohm resistor must not break solve) · R2→10M ⇒ 9.999 V · V→−10 ⇒ −5 V · V→0 ⇒ 0 V exactly.
- **M:** M1 source Rint (default?) shifts Vout by Rint/(Rint+20k)·10.
- **Trap:** R=0 exactly; both R = 1e12 (GMIN-dominated node).

### 2. LED with Resistor — 5 V, 330 Ω, LED (red default)
- **N:** I = (5−Vf)/330; red Vf≈1.9 ⇒ ≈9.4 mA; LED lit.
- **V:** Cycle all 9 colours ⇒ I falls as Vf rises (blue/white ≈3 V ⇒ 6 mA) · R→10 Ω ⇒ ~300 mA over-current visual/burn-out state · R→100k ⇒ 30 µA dim/off threshold · V→1.5 ⇒ below Vf, off, I≈0 · V→−5 ⇒ reverse, off, no breakdown blow-up.
- **M:** M2 real diode: Vf changes with I (log); ideal: fixed Vf. Compare I at R=10 Ω.

---

## Passive filters (AC 1 Vpk 1 kHz)

### 3. RC Low-Pass — R=1k, C=100n ⇒ fc = 1591.5 Hz
- **N:** |H(1 kHz)| = 0.846 ⇒ 0.846 Vpk, phase −32.1°. Bode −3 dB @ 1.59 kHz, −20 dB/dec.
- **V:** f→1.59k ⇒ 0.707 Vpk / −45° · f→16 kHz ⇒ 0.099 · C→1µ ⇒ fc 159 Hz, 1 kHz out 0.157 · R→10 ⇒ fc 159 kHz, needs auto-dt (T pass!).
- **M:** M3 cap ESR 0.01 Ω negligible; set ESR=100 Ω ⇒ HF floor 100/1100 = −20.8 dB.

### 4. RC High-Pass — C=100n, R=1k ⇒ fc 1591.5 Hz
- **N:** |H| = 0.532, phase +57.9°.
- **V:** f→10 Hz ⇒ 0.0063 (check scope V/div autoset at mV) · f→100 kHz ⇒ 1.0 · C→10n ⇒ fc 15.9 kHz.
- **Trap:** DC offset on source 5 V ⇒ output must settle to 0 V mean (cap blocks DC); watch initial transient at t=0 (DC op point should pre-charge cap).

### 5. RL Low-Pass — L=10m, R=100 ⇒ fc = R/2πL = 1591.5 Hz (output across R)
- **N:** 0.846 Vpk, −32°. Same numbers as #3.
- **V:** L→1m ⇒ fc 15.9 kHz · R→1k ⇒ fc 15.9 kHz · L→1 H ⇒ fc 16 Hz.
- **M:** inductor DCR 0.1 Ω (real) adds to R — negligible; set DCR=100 ⇒ passband 0.5. i_sat=1 A: raise source to 200 V to hit saturation, expect distortion not NaN.
- **Trap:** ideal L + ideal source at t=0 step (source starts at phase 0 so fine); set phase=90° ⇒ step into inductor — must not spike.

### 6. RL High-Pass — R=100, L=10m (output across L)
- **N:** 0.532 Vpk, +58°.
- **V:** as #5 inverted.

---

## RLC & bridges

### 7. Series RLC — 5 Vpk 159 Hz, R=100, L=10m, C=100µ
- **N:** f0 = 1/(2π√LC) = 159.15 Hz — source is at resonance. Q = (1/R)√(L/C) = 0.1. I = 50 mA; V_C = V_L = 0.5 Vpk (Q·Vin).
- **V:** R→1 ⇒ Q=10, V_C = 50 Vpk (voltage magnification — scope V/div 10 V) · f→50 Hz and 500 Hz ⇒ current falls · L→100m ⇒ f0 50.3 Hz.
- **M:** M1: cap ESR + DCR add to R — with R→1 the difference is visible (Q 10 → ~9).
- **T:** with R→1 the ring-up takes Q cycles ≈ 60 ms; verify at 10 ms/div.

### 8. Parallel RLC (tank) — 5 Vpk 159 Hz via 1k, L=10m, C=100µ
- **N:** at f0 tank Z→∞ (ideal) ⇒ Vout → 5 Vpk. Q = R√(C/L) = 100. BW ≈ 1.6 Hz.
- **V:** f→161 Hz (just outside BW) ⇒ Vout drops sharply — good numeric stress · f→1 kHz ⇒ Vout ≈ 5·|Z|/1k small · C→10µ ⇒ f0 503 Hz.
- **M:** DCR 0.1 Ω limits tank Z to L/(C·DCR) = 1 kΩ ⇒ Vout 2.5 Vpk at resonance — big, visible ideal-vs-real delta. **This is the best template for the mixed-model check.**
- **Trap:** ideal L across ideal C with ideal source at exactly f0 for >Q cycles — amplitude must not grow unbounded.

### 9. Wheatstone Bridge — 10 V, 1k/1k/1k/1.1k
- **N:** V_left = 5.000, V_right = 10·1100/2100 = 5.238 ⇒ ΔV = 0.238 V (sign per layout).
- **V:** R4→1000 ⇒ ΔV = 0 exactly (null) · R4→900 ⇒ −0.262 · add a 10k galvanometer resistor across the bridge ⇒ loaded ΔV ≈ 0.227.
- **Trap:** balanced bridge — mid-node with no current path difference; solver must give exactly 0, not ±1e-9 flicker on display.

---

## Rectifiers & power

### 10. Half-Wave — 5 Vpk 60 Hz, D, 1k
- **N:** Vpk_out ≈ 4.3 V, Vavg ≈ 1.37 V (Vpk/π), f = 60 Hz.
- **V:** amp→0.5 ⇒ output ≈ 0 (below Vf) · add 100 µ across R ⇒ DC ≈ 4.2 V, ripple ≈ I/(fC) = 4.3m/(60·100µ) = 0.72 V · reverse diode ⇒ negative half.
- **M:** M2 real diode: at 0.5 Vpk shows exponential turn-on rather than hard cut.

### 11. Full-Wave Bridge — 12 Vpk 60 Hz, 4×D, 100µ elec, 1k
- **N:** Vpk = 12−1.4 = 10.6 V, ripple f = 120 Hz, ΔV ≈ 10.6m/(120·100µ) = 0.88 Vpp, Vdc ≈ 10.2.
- **V:** C→1000µ ⇒ ripple 0.088 · R→100 ⇒ ripple 8.8 V (deep) · f→50 Hz ⇒ ripple 1.06 · amp→170 V ⇒ elec cap max_voltage check (should flag over-voltage).
- **M:** M3 electrolytic ESR ⇒ small step at diode turn-on; M1 vs M0 ripple bottom shape.
- **Trap:** two grounds in template — confirm both on the same net (isolated ground = floating output).

### 12. Center-Tap Rectifier — 120 Vpk 60 Hz, CT xfmr ratio 0.1, 2×D, 470µ, 1k
- **N:** verify how `turns_ratio` applies to CT: full secondary 12 Vpk ⇒ each half 6 Vpk ⇒ Vout ≈ 5.3 V; **or** each half 12 ⇒ 11.3 V. Read the model, then hand-calc ripple = I/(120·470µ).
- **V:** ratio→0.05 · load→100 · swap a diode orientation ⇒ half-wave only.
- **M:** real transformer (leakage/winding R) with ideal diodes ⇒ ringing check.

### 13. AC-DC Supply — 170 Vpk 60 Hz, xfmr 0.1, bridge, 1000µ, 100 Ω
- **N:** 17 Vpk − 1.4 = 15.6 V; I ≈ 156 mA; ripple ≈ 0.156/(120·1e-3) = 1.3 Vpp.
- **V:** load→10 Ω ⇒ ripple 13 V, sag · C→100µ ⇒ ripple 13 V · ratio→0.2 ⇒ 32.6 V.
- **T:** 60 Hz with default dt 100 ns ⇒ 166 k steps/cycle; confirm auto-dt raises dt and history decimation still shows ≥3 cycles at 5 ms/div.

### 14. 120V/60Hz → 12 V DC (American) — as #13 but 2200µ (25 V rating)
- **N:** 15.6 V, ripple ≈ 0.59 Vpp.
- **V:** ratio→0.2 ⇒ 32.6 V > 25 V rating ⇒ expect cap over-voltage indication/failure, and Reset must repair.
- **Trap:** peak inrush into 2200µ at t=0 with ideal source+xfmr ⇒ huge first-step diode current; must not NaN.

### 15. Zener Reference — 12 V, 1k, Z(5.1 V, Rz 10), load 10k
- **N:** Iz ≈ (12−5.1)/1k − 0.51m ≈ 6.4 mA; Vout ≈ 5.1 + 10·6.4m = 5.16 V.
- **V:** Vin 6→20 V sweep ⇒ Vout 5.10→5.25 (line reg = Rz/(R+Rz)) · load→100 Ω ⇒ Iload 51 mA > available ⇒ zener drops out, Vout = 12·100/1100 = 1.09 · Vin→4 ⇒ below knee, Vout = 4·10k/11k = 3.64.
- **M:** ideal zener = hard 5.1; real = knee/Rz slope. Compare the 6→20 V sweep slope.

### 16. 7805 — 9 V in, 0.33µ/0.1µ, 50 Ω
- **N:** 5.00 V, 100 mA.
- **V:** Vin→6.5 (below 7 V dropout) ⇒ Vout < 5, tracks Vin−2 · Vin→35 ⇒ still 5 · load→2 Ω ⇒ 2.5 A > limit ⇒ current-limit/thermal behaviour, no NaN · load→open ⇒ 5 V.
- **T:** check no startup overshoot at dt=10 ms.

### 17. LM317 — 12 V, R1=240, R2=720, 1µ out, 100 Ω
- **N:** 1.25(1+720/240) = 5.00 V (+ Iadj·720 ≈ 0.04) ⇒ ≈5.04 V; 50 mA.
- **V:** R2→2.4k ⇒ 13.75 > Vin−dropout ⇒ clips at ≈10 V · R2→0 ⇒ 1.25 V · R1→0 ⇒ divide-by-zero guard.
- **M:** source Rint at 50 mA droop.

### 18. TL431 — 5 V, 470 Ω, TL431, 1k load
- **N:** Vk = 2.50 V (ref tied to K); I_R = 5.3 mA, load 2.5 mA, shunt 2.8 mA (> 1 mA min).
- **V:** R→4.7k ⇒ 0.53 mA < 1 mA min ⇒ regulation lost, Vk rises toward divider value · add divider ref (10k/10k) ⇒ Vk = 5.0 (clips near Vin) · Vin→2 ⇒ Vk ≈ 2 (no regulation).

---

## Op-amp circuits (ideal `COMP_OPAMP` unless noted)

**Common trap (check on every one):** several op-amp templates place a **12 V DC source + 0.1 µF
cap** as a "supply" that the ideal op-amp model does not actually use. Verify it's wired to a
real net (or is intentionally decorative and clearly labelled); a floating source/cap pair
inflates node count and can produce a GMIN-only node. Also confirm `vmax/vmin` defaults so
clipping expectations below hold.

### 19. Voltage Follower — 1 Vpk 1 kHz
- **N:** 1 Vpk, 0° phase.
- **V:** amp→20 V ⇒ clips at rails · f→1 MHz ⇒ ideal: still 1 Vpk; M4 real (GBW 1 M): −3 dB at 1 MHz, phase −45° · f→10 MHz + real: slew 0.5 V/µs ⇒ triangle.
- **M4** is the main event here: gain error 1/(1+A) with A=1e5.

### 20. Inverting Amp — 0.5 Vpk, Rin=1k, Rf=10k
- **N:** −10 ⇒ 5 Vpk, 180°.
- **V:** Rf→100k ⇒ −100 ⇒ 50 Vpk ⇒ rail clip (square-ish; check clip level = vmax) · Rin→10k ⇒ −1 · Rf→0 ⇒ 0 V · Rin→0 ⇒ ideal op-amp with zero input R — must not NaN (expected: rail).
- **M4:** real op-amp Rout 75 Ω into Rf 10k negligible; add 100 Ω load ⇒ 57 % droop... wait, closed loop corrects it — expect ≈ no change (that's the point of feedback). Open-loop test: remove Rf ⇒ rail.

### 21. Non-Inverting Amp — 0.5 Vpk, Rg=1k, Rf=10k, uses `COMP_OPAMP_FLIPPED`
- **N:** 1+10 = 11 ⇒ 5.5 Vpk, 0°.
- **V:** Rf→0 ⇒ ×1 · Rg→open (delete) ⇒ ×1 · Rf→1M ⇒ ×1001 ⇒ clip.
- **Trap:** flipped symbol — replace with normal `COMP_OPAMP` and re-wire; result must be identical.

### 22. Summing Amp — 1 V, 2 V, 3 V, 4×10k
- **N:** −(1+2+3) = −6.000 V.
- **V:** V3→−3 ⇒ 0.000 · R_f→20k ⇒ −12 (rails?) · R1→5k ⇒ −(2+2+3) = −7.
- **Trap:** three ideal DC sources into ideal op-amp virtual ground — pure resistive, must converge in 1–2 NR iterations.

### 23. Difference Amp — 1 Vpk sine vs 0.5 V DC, 4×10k, gain=1e5
- **N:** Vout = (V+) − (V−) ⇒ ±1 Vpk sine with −0.5 or +0.5 V offset depending on which input is which. Vavg = ∓0.5, Vpp = 2.
- **V:** make one R 11k ⇒ CMRR degrades: feed both inputs the same 1 V sine (delete DC, add wire) ⇒ output should be ≈0 with matched R, ≈ 0.05 Vpk with 11k.
- **M4:** finite CMRR.

### 24. Transimpedance — 1 mA DC current source, Rf=10k
- **N:** Vout = ∓10 V (sign by current direction).
- **V:** I→10 mA ⇒ 100 V ⇒ rail clip · I→−1 mA ⇒ sign flips · Rf→1M with I=1µA ⇒ 1 V · replace source with photodiode + Lux slider ⇒ Vout tracks lux 0→100 % live.
- **Trap:** ideal current source into ideal virtual ground — if op-amp deleted while running, current source has no path ⇒ must error cleanly (TEST_PLAN 2.14).

### 25. Instrumentation Amp — 0.1 Vpk vs 0.05 V DC, Rg=1k, 6×10k
- **N:** stage-1 gain 1+2·10k/1k = 21; diff stage 1 ⇒ Vout = 21·(0.1 sin − 0.05) ⇒ 2.1 Vpk on −1.05 V offset (sign per wiring).
- **V:** Rg→10k ⇒ gain 3 · Rg→100 ⇒ 201 ⇒ clip · common-mode test as #23.

### 26. Integrator — square ±1 V 100 Hz, R=10k, C=100n (τ = 1 ms)
- **N:** slope = 1 V/1 ms = 1000 V/s; half period 5 ms ⇒ 5 V excursion ⇒ triangle ≈ 10 Vpp (may hit rails). Phase: triangle peaks at square edges.
- **V:** f→1 kHz ⇒ 1 Vpp triangle · C→1µ ⇒ 1 Vpp · offset→0.1 V ⇒ integrator drifts to rail in ≈ (rail/0.1)·τ·... ⇒ ~ 0.12 s — **verify it drifts** (that's correct physics) and that Reset recovers.
- **Trap:** no DC feedback resistor ⇒ DC op-point is undefined (cap open) — solver must not fail at t=0 (GMIN should pin it). Add 1M across C ⇒ drift stops, corner at 1.6 Hz.
- **T:** dt=10 ms > edge spacing ⇒ aliased; document.

### 27. Differentiator — triangle 1 Vpk 100 Hz, C=100n, R=10k
- **N:** dv/dt = 4·A·f = 400 V/s ⇒ Vout = −RC·dv/dt = −1e-3·400 = ∓0.4 V square.
- **V:** f→1 kHz ⇒ ±4 V · R→100k ⇒ ±4 V · feed square wave ⇒ spikes at edges (ideal: huge — check clamp, no NaN).
- **M4:** real op-amp GBW/slew turns spikes into finite pulses — classic realistic delta.
- **T:** spike width depends on dt — record spike amplitude at dt = 1 µs vs 100 µs (expected to change; that's a dt artefact to document, not a bug).

### 28. Comparator — 10 V ref via 10k/10k (=5 V), sine 6 Vpk offset 5 V 100 Hz
- **N:** input crosses 5 V at 0° and 180° ⇒ square 50 % duty at 100 Hz, rail-to-rail.
- **V:** offset→7 ⇒ duty = fraction above 5 V: crossing at asin(−2/6) ⇒ 60.6 % · offset→11 ⇒ never below 5 ⇒ constant high · amp→0.01 ⇒ tiny signal around 5 V + 5 V ref: noisy chatter? (ideal gain 1e5 ⇒ 0.1 mV resolves) · ref divider 10k→20k ⇒ 6.67 V threshold.
- **M4:** real: slew-limited edges; measure rise time = (vmax−vmin)/slew.

### 29. Window Comparator — 5 V via 3×10k (1.67 V / 3.33 V), input 2.5 V DC, 2 op-amps, 2×10k summing + 10k pull-up, LED
- **N:** 2.5 V inside window ⇒ both outputs +15 V ⇒ LED ON at ≈2.9 mA (Vf ≈ 1.88 V). Either output −15 V ⇒ net LED current negative ⇒ OFF. 
- **V:** input→1 V ⇒ outside · 4 V ⇒ outside · 1.67 V exactly ⇒ boundary, no oscillation · replace input with triangle 0–5 V 10 Hz ⇒ LED pulses twice per cycle; scope output shows window pulse width = 1/3 period.
- **Trap (fixed):** outputs now sum through R_hi/R_lo; if you delete one of them the remaining path still lights the LED weakly (0.3 mA via the pull-up).

### 30. Schmitt Trigger — sine 3 Vpk 100 Hz, Rin 10k, Rf 100k, R 10k
- **N:** hysteresis ≈ ±Vsat·10k/(10k+100k) = ±Vsat/11; with ±12 V ⇒ ±1.09 V thresholds; output square, edges at ±1.09 V crossings (not 0).
- **V:** Rf→10k ⇒ ±6 V thresholds ⇒ with 3 Vpk input **never switches** (good test) · amp→1 ⇒ below ±1.09 ⇒ holds last state · Rf→1M ⇒ ±0.12 V.
- **M4:** Vsat = real vmax ⇒ thresholds change; verify they track.

### 31. Precision Rectifier — 1 Vpk 100 Hz, 2 op-amps, 2 diodes, 10k/10k/10k/5k/10k
- **N:** Vout = |Vin| (1 Vpk, 200 Hz fundamental in FFT, Vavg = 0.637).
- **V:** amp→10 mV ⇒ still rectifies (this is the point vs #10) · f→10 kHz ⇒ ideal fine; M4 real: diode-switch delay/slew distorts zero crossing · swap diodes ⇒ −|Vin|.
- **Trap:** ideal diodes inside ideal op-amp loops = worst-case NR problem; log iteration counts.

### 32. Peak Detector — 5 Vpk 100 Hz, op-amp, D, 10µ
- **N:** holds 5.00 V (op-amp inside loop cancels Vf). Ripple ≈ 0 (no bleed).
- **V:** add 100k bleed ⇒ droop τ = 1 s ⇒ ripple ≈ 5·10 ms/1 s = 50 mV · amp→2 then back to 5 live ⇒ must never drop (no bleed) then rise · negative input only ⇒ 0.
- **M2:** real diode reverse leakage ⇒ slight droop.

### 33. Sallen-Key LPF — R=10k×2, C=10n×2, unity gain ⇒ fc = 1591.5 Hz, Q = 0.5
- **N:** at 1 kHz |H| = 0.717 (−2.9 dB); Bode: −40 dB/dec above fc, −3 dB at ≈ 1.0 kHz·(Q=0.5 ⇒ −3 dB at 0.644·fc = 1.02 kHz — so nearly exactly at the stimulus!). Phase −90° at fc.
- **V:** C1→100n (10:1) ⇒ Q ↑ peaking; verify Bode peak · R→1k ⇒ fc 15.9 kHz (T pass: needs auto-dt).
- **Trap:** stimulus sits at −3 dB; small model differences show — good sensitivity test for M4.

### 34. Active Band-Pass — 10k/10n pairs ⇒ f0 ≈ 1591 Hz
- **N:** peak near 1.59 kHz; at 1 kHz below peak; read Bode for gain/Q and record as baseline.
- **V:** f→f0 ⇒ max · f→100 Hz / 20 kHz ⇒ −20 dB/dec skirts.

### 35. Twin-T Notch — R=26525×2, R/2=13262, C=100n×3 ⇒ f = 1/(2πRC) = 60.0 Hz, 60 Hz stimulus
- **N:** output ≈ 0 (> 30 dB notch ideal); measure residual.
- **V:** f→30 / 120 Hz ⇒ passes ≈ 1 Vpk · R/2→13k (mismatch 2 %) ⇒ notch depth drops to ~ −25 dB · C→110n on one ⇒ notch shifts.
- **M3:** cap ESR fills the notch — measure depth M0 vs M3.

### 36. Wien Bridge Oscillator — R=10k, C=10n ⇒ f = 1591.5 Hz, gain 1+22k/10k = 3.2, 0.5 V/50 µs start-up pulse in R2's ground leg
- **N:** grows ≈6.7 %/cycle from the kick, reaches the ±15 V rails after ≈30 ms, then runs rail-limited (no AGC) ⇒ flat-topped sine at ≈1.59 kHz; measure THD.
- **V:** Rf 22k→18k (gain 2.8) ⇒ decays to 0 · Rf→30k ⇒ square-ish · C→100n ⇒ 159 Hz · R→1k ⇒ 15.9 kHz · delete the pulse ⇒ never starts (ideal noiseless loop).
- **M4 / pending-diff focus:** swap to `COMP_OPAMP_REAL` ⇒ post-solve clamp path. Run 10 min (TEST_PLAN 2.17). Change vmax live.
- **Trap:** startup needs noise/asymmetry; if it never starts with ideal parts that's a model issue to note (real op-amp offset should start it).

### 37. RC Phase-Shift Oscillator (**new, uncommitted**) — pulse kick 1 V/0.1 ms, `OPAMP_REAL` gain 1e6 GBW 10 M rails 0–5 V, bias 2.5 V (10k/10k), Rf=40k, 3×(1k,10n), coupling 100n
- **N:** f = 1/(2π√6·RC) = 6.50 kHz; gain 40 ≥ 29 ⇒ grows to rail limits (0–5 V) ⇒ expect clipped sine centred on 2.5 V. Autostart flag fires once.
- **V:** Rf→25k ⇒ dies out · Rf→29k ⇒ marginal · C→100n ⇒ 650 Hz · bias divider 10k/20k ⇒ centre 1.67 V ⇒ asymmetric clipping · rails→±12 ⇒ verify clamp uses new values live · delete pulse source ⇒ does it still start?
- **Load pass extra:** it was hand-positioned from `debug_circuit.json` — check overlaps carefully; stderr must not flood with `[CLAMP]`/`[DC ANALYSIS]`.

---

## Transistor circuits (BJT `bf=100`, `ideal=true` by default ⇒ M2 flips to Gummel-Poon)

### 38. Common Emitter — 12 V, 47k/10k bias, Rc 2.2k, Re 1k, 10µ in/out, 0.1 Vpk in
- **N (bias):** Vb = 12·10/57 = 2.105 V; Ve = 1.40; Ie ≈ 1.40 mA; Vc = 12 − 1.40·2.2 = 8.9 V.
  **Gain:** Re unbypassed ⇒ Av ≈ −Rc/(Re+re) = −2.2k/1018 ≈ −2.2 ⇒ 0.22 Vpk out. If the template has an emitter bypass cap ⇒ ≈ −Rc/re ≈ −120 (clips). Read the schematic and pin the expectation.
- **V:** amp→1 V ⇒ clipping asymmetric · bf→300 ⇒ bias shifts slightly (Vb loaded less), gain ≈ same · Rc→10k ⇒ Vc < Ve ⇒ saturation, output collapses · Re→0 ⇒ thermal-runaway-ish bias; gain ≈ −Rc/re, clips.
- **M2:** VAF=100 ⇒ gain ≈ 2 % lower; `ise/isc` leakage.
- **T:** coupling caps 10µ with 47k‖10k ≈ 8.2k ⇒ input corner ≈ 2 Hz ⇒ settle time ~0.5 s — verify DC op-point pre-charges caps (no 0.5 s startup wobble) or document.

### 39. Common Source — 12 V, NMOS vth 1.5 kp 0.01, gate divider 1M/330k (Vg = 2.98 V), Rd 2.2k, Rs 470, AC 0.1 Vpk via 10µ
- **N (measured):** Vg 2.98, Vs 1.25, Vd 6.16 ⇒ Id = 2.65 mA, Vov = 0.23 V (model uses Id = ½·kp·(W/L)·Vov²). Gain ≈ −gm·Rd/(1+gm·Rs).
- **V:** vth→0.5 · kp→0.001 · Rs→0 · offset→3 V (if it reaches the gate).
- **M2:** λ=0.04 ⇒ output R 1/(λId).

### 40. Common Drain (source follower) — NMOS vth 1.5 kp 0.02, gate divider 1M/1M (Vg = 6 V), Rs 1k, 1 Vpk via 10µ
- **N (measured):** Vg 6.00, Vs 4.29 ⇒ Id 4.3 mA; gain ≈ gm·Rs/(1+gm·Rs) ≈ 0.9, no inversion.
- **V:** amp→5 V ⇒ cutoff clipping on negative side · Rs→100.

### 41. Multistage (2× CE 4.7k/1k, 47k/10k) — 10 mVpk in
- **N:** per stage Vb 2.105, Ie 1.4 mA, Vc = 12 − 6.6 = 5.4 V; Av per stage ≈ −4.7k/1018 ≈ −4.6 (unbypassed) ⇒ total ≈ +21 ⇒ 0.21 Vpk, 0° phase (two inversions).
- **V:** amp→100 mV ⇒ 2.1 Vpk ok · amp→1 V ⇒ stage 2 clips · interstage cap 10µ→10n ⇒ HPF corner ≈ 1/(2π·10n·(4.7k‖8.2k)) ≈ 5.3 kHz ⇒ 1 kHz attenuated ~5×.

### 42. Differential Pair — 12 V, Rc 4.7k×2, tail 10k to ground, inputs 50 mVpk anti-phase **DC-biased at 6 V** through 1k base resistors
- **N (measured):** Vb 6.00, Ve 5.38 ⇒ Itail 0.54 mA, Ic 0.27 mA each, Vc 10.75 V. Ad ≈ Rc/(2·re) = 4.7k/(2·96) ≈ 24 ⇒ ≈1.2 Vpk differential output.
- **V:** common-mode test (both sources same phase) ⇒ output ≈ 0 · mismatch Rc 4.7k/5.6k ⇒ offset · tail→1k.

### 43. Current Mirror — 12 V, Rref 10k, 2 NPN, Rload 1k
- **N:** Iref = (12−0.7)/10k = 1.13 mA (ideal β) ⇒ Iout = Iref·β/(β+2) = 1.108 mA; V_load = 1.11 V; Vc2 = 10.9 V.
- **V:** Rload→10k ⇒ still 1.1 mA (compliance ok) · Rload→15k ⇒ needs 16.6 V ⇒ saturates, Iout drops · bf→10 ⇒ Iout = 0.94 mA (17 % error — nice check of β/(β+2)) · Rref→1k ⇒ 11.3 mA.
- **M2:** VAF=100 ⇒ Iout rises with Vce2 (Early): Rload 1k vs 9k should differ by ≈ 8 %.

### 44. Push-Pull — ±12 V (VEE source value −12), NPN + PNP (rotated 180, emitters joined), 5 Vpk in, 100 Ω load
- **N:** crossover dead-band ±0.7 V ⇒ output ≈ 4.3 Vpk with flat notch at zero; FFT shows odd harmonics.
- **V:** amp→0.5 ⇒ output ≈ 0 (all in dead-band) · amp→13 ⇒ clips at ≈ 11.3 · load→8 Ω ⇒ 0.54 A, check β-limited base drive (ideal source drives base directly so no limit) · add bias diodes (2×D between bases + resistors) ⇒ notch disappears.
- **Trap:** ideal AC source driving two bases directly — base current unlimited; check for absurd currents in status bar.

### 45. CMOS Inverter — 5 V, PMOS vth −1 kp 50µ, NMOS vth 1 kp 110µ, 100 pF load, square 0–5 V 1 kHz
- **N:** inverted square; switching threshold where currents match: solve ½·110µ(Vin−1)² = ½·50µ(4−Vin)² ⇒ Vin ≈ 2.19 V. Rise/fall ≈ C·ΔV/I with I ≈ ½·50µ·9 = 225 µA ⇒ t ≈ 100p·5/225µ ≈ 2.2 µs (fall faster: 495 µA ⇒ 1 µs).
- **V:** drive with triangle 0–5 V 100 Hz ⇒ transfer curve on X-Y mode, threshold 2.19 V · load→10 nF ⇒ 220 µs edges, visible at 1 kHz · kp equal ⇒ threshold 2.5 · Vdd→3 V.
- **T:** edge time 1–2 µs vs dt: at dt=10 µs edges vanish (expected) — key time-base sensitivity case.
- **M2:** λ ⇒ slight slope on plateaus.

### 46. Constant Current Source — 12 V, 10k/2.2k, NPN, Re 470, load 1k
- **N:** Vb = 12·2.2/12.2 = 2.164; Ve = 1.464; I = 3.11 mA; V_load = 3.11 V; Vce = 12 − 3.11 − 1.46 = 7.4 V.
- **V:** load 1k→2k ⇒ I unchanged 3.11 mA (Vload 6.2) · load→3.5k ⇒ needs 10.9 V > available ⇒ saturates ⇒ I drops · Re→100 ⇒ 14.6 mA · bf→20 ⇒ base current loads divider (Ib = 0.73 mA vs divider 1 mA) ⇒ I drops noticeably (ideal-vs-real β sensitivity).

---

## Misc

### 47. Clamper (DC restorer) — 5 Vpk 1 kHz, C 1µ, D (Is=1e-9), R 100k
- **N:** τ = 0.1 s ≫ 1 ms ⇒ output shifted to ≈ −0.7…+9.3 V (positive clamper) after ~2 cycles.
- **V:** R→1k ⇒ τ = 1 ms ≈ period ⇒ clamp sags, tilt visible · C→1n ⇒ τ 0.1 ms ⇒ barely clamps · flip diode ⇒ negative clamper · f→10 Hz ⇒ τ/period = 1 ⇒ tilt.
- **M2:** ideal diode (fixed Vf) vs real: offset differs by the Is-dependent Vf.
- **T:** settling takes ≈ 3 cycles; single-shot trigger at t=0 should capture the ramp.

---

## Result log

| # | Template | L | N | V | M | T | S | Notes / issue link |
|---|----------|---|---|---|---|---|---|--------------------|
| 1 | Voltage Divider | | | | | | | |
| 2 | LED w/ Resistor | | | | | | | |
| 3 | RC Low-Pass | | | | | | | |
| 4 | RC High-Pass | | | | | | | |
| 5 | RL Low-Pass | | | | | | | |
| 6 | RL High-Pass | | | | | | | |
| 7 | Series RLC | | | | | | | |
| 8 | Parallel RLC | | | | | | | |
| 9 | Wheatstone | | | | | | | |
| 10 | Half-Wave Rect | | | | | | | |
| 11 | Full-Wave Bridge | | | | | | | |
| 12 | Center-Tap Rect | | | | | | | |
| 13 | AC-DC Supply | | | | | | | |
| 14 | 120V→12V DC | | | | | | | |
| 15 | Zener Ref | | | | | | | |
| 16 | 7805 | | | | | | | |
| 17 | LM317 | | | | | | | |
| 18 | TL431 | | | | | | | |
| 19 | Voltage Follower | | | | | | | |
| 20 | Inverting Amp | | | | | | | |
| 21 | Non-Inverting Amp | | | | | | | |
| 22 | Summing Amp | | | | | | | |
| 23 | Difference Amp | | | | | | | |
| 24 | Transimpedance | | | | | | | |
| 25 | Instrumentation Amp | | | | | | | |
| 26 | Integrator | | | | | | | |
| 27 | Differentiator | | | | | | | |
| 28 | Comparator | | | | | | | |
| 29 | Window Comparator | | | | | | | |
| 30 | Schmitt Trigger | | | | | | | |
| 31 | Precision Rectifier | | | | | | | |
| 32 | Peak Detector | | | | | | | |
| 33 | Sallen-Key LP | | | | | | | |
| 34 | Active Band-Pass | | | | | | | |
| 35 | Twin-T Notch | | | | | | | |
| 36 | Wien Bridge Osc | | | | | | | |
| 37 | RC Phase-Shift Osc | | | | | | | |
| 38 | Common Emitter | | | | | | | |
| 39 | Common Source | | | | | | | gate divider added |
| 40 | Common Drain | | | | | | | gate divider added |
| 41 | Multistage Amp | | | | | | | |
| 42 | Differential Pair | | | | | | | direct-coupled, 6 V bias |
| 43 | Current Mirror | | | | | | | |
| 44 | Push-Pull | | | | | | | |
| 45 | CMOS Inverter | | | | | | | |
| 46 | Constant Current Src | | | | | | | |
| 47 | Clamper | | | | | | | |

(47 blocks = the 47 `CIRCUIT_*` entries in `include/circuits.h` excluding `CIRCUIT_NONE`/`_COUNT`.)
