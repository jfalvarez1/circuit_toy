# Prebuilt Circuit Template Audit (81 templates)

Companion to `TEST_PLAN.md` §8. Every template gets the same five passes; the per-template
block adds the hand-calculated nominal, the **value variations** to try, and specific traps.

Values below were extracted from `src/circuits.c` `place_*` builders on 2026-08-24 — if a
template's BOM changes, update its block.

### Fixes applied 2026-08-24 (headless smoke test now reports 47/47; 65/65 after the 18 templates added later the same day; 72/72 after the 7 protection & control templates; 81/81 after the 4 three-phase and 5 signal-generator templates, see the added sections)
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

### Second pass 2026-08-24 (probe oracle `--probe-test`, 42/42)
Engine: inductor companion had the memory term with the wrong sign (every inductor looked
~30x too large - RL filters and RLC were wrong); capacitors now use theta-method (0.6)
trapezoidal integration, op-amp rail flip-flop falls back to the linear stamp (Wien no
longer latches at a rail), the real op-amp no longer silently caps its gain at 150.
Templates: Schmitt input biased at the 6 V reference (it could never cross it); instr amp
input wire had landed on op1's inverting terminal; precision rectifier rebuilt with the
textbook two-op-amp absolute-value topology (out = -|Vin|); phase-shift oscillator moved
to +/-5 V rails (single-supply DC feedback latched it) and Rf 33k; every template now
carries an on-canvas "how it works / PROBE" note.

### OPEN schematic-geometry items (`--geom-test`: crossings and wires through bodies; 59/81 clean — all 18 added templates, the 7 protection & control templates #66–#72 and the 9 three-phase / signal-generator templates #73–#81 are clean)
```
[WARN] geom  Common Emitter               diag=0 cross=0 through=1 touch=0 through:Q104
[WARN] geom  Source Follower              diag=0 cross=0 through=1 touch=0 through:M142
[WARN] geom  Two-Stage Amp                diag=0 cross=0 through=2 touch=0 through:Q159 through:Q167
[WARN] geom  Differential Pair            diag=0 cross=0 through=4 touch=0 through:Q181 through:R183 through:Q182 through:R183
[WARN] geom  Current Mirror               diag=0 cross=0 through=4 touch=0 through:Q197 through:Q197 through:Q198 through:Q198
[WARN] geom  Push-Pull                    diag=0 cross=1 through=5 touch=0 through:Q210 cross@(-90,25) through:Q211 through:Q210 through:Q211 through:Q211
[WARN] geom  CMOS Inverter                diag=0 cross=0 through=4 touch=0 through:M223 through:M223 through:M224 through:M224
[WARN] geom  Summing Amp                  diag=0 cross=2 through=0 touch=0 cross@(-30,45) cross@(50,45)
[WARN] geom  Comparator                   diag=0 cross=1 through=1 touch=0 cross@(-50,50) through:R271
[WARN] geom  Center-Tap Rect              diag=0 cross=1 through=0 touch=0 cross@(205,85)
[WARN] geom  AC-DC Supply                 diag=0 cross=1 through=0 touch=0 cross@(342,108)
[WARN] geom  US 120V-12V                  diag=0 cross=1 through=0 touch=0 cross@(342,108)
[WARN] geom  Difference Amp               diag=0 cross=0 through=2 touch=0 through:V338 through:~V342
[WARN] geom  Transimpedance               diag=0 cross=0 through=1 touch=0 through:V358
[WARN] geom  Instr. Amp                   diag=0 cross=2 through=0 touch=0 cross@(190,-75) cross@(240,-35)
[WARN] geom  Sallen-Key LP                diag=0 cross=0 through=4 touch=0 through:V392 through:~V396 through:C400 through:C401
[WARN] geom  Active Bandpass              diag=0 cross=0 through=2 touch=0 through:V410 through:~V414
[WARN] geom  Notch Filter                 diag=0 cross=0 through=3 touch=0 through:C431 through:R434 through:R434
[WARN] geom  Wien Oscillator              diag=0 cross=0 through=1 touch=0 through:V444
[WARN] geom  Current Source               diag=0 cross=0 through=1 touch=0 through:R476
[WARN] geom  Window Comp                  diag=0 cross=1 through=4 touch=0 cross@(180,-140) through:R488 through:V484 through:R488 through:R488
[WARN] geom  Schmitt Trigger              diag=0 cross=0 through=3 touch=0 through:V503 through:R513 through:R512
[WARN] geom  LM317 Adj Reg                diag=0 cross=0 through=4 touch=0 through:R568 through:R568 through:R568 through:R568
```

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

## Added 2026-08-24 (18 templates: #48–#65)

Every block below carries the **demo contract** (`template_demo[]` DemoKind + f_char checked by
`--demo-test`), the **auto-probe** (`template_output[]`) with the scope presets
(`template_time_div[]` / `template_volt_div[]`), and the **oracle** number from
`tools/template_smoke.c` `probe_cases[]` (tolerance and sample time in brackets). Geometry:
all 18 are clean under `--geom-test` (0 diagonal, 0 crossings, 0 wires through bodies) —
the WARN list above is unchanged. Power-system oracles come from `docs/RESEARCH_TEXAS_GRID.md`
(ERCOT/CREZ 345 kV, 138 kV, 12.47 kV feeder) and `docs/RESEARCH_AEP_PC.md`.

## Passive filters & diode circuits (added)

### 48. RC Band-Pass — 1 Vpk sweep 100 Hz–20 kHz, C1=200n, R1=1k (HP fc1 = 796 Hz), R2=10k, C2=5n (LP fc2 = 3.18 kHz)
- **Demo:** `DEMO_BANDPASS`, f_char 1600 Hz. Probe: C2 (output). Presets 200 µs/div, 0.5 V/div.
- **N:** peak near √(fc1·fc2) = 1.59 kHz; |H| ≈ 0.79 there (the 10k/1k stage separation costs the 1/(1+R1/R2) loading). Oracle: **0.79 Vpk** at 1.6 kHz (±15 %, sampled 6 ms).
- **V:** C1→2µ ⇒ fc1 80 Hz, peak flattens to ~0.9 across 100 Hz–3 kHz · C2→50n ⇒ fc2 318 Hz < fc1 ⇒ peak drops to ~0.2 (overlapping skirts) · R2→1k ⇒ HP stage loaded 2:1, peak ≈ 0.45 · disable the sweep and set f = 796 / 1590 / 3180 Hz ⇒ −3 dB / peak / −3 dB.
- **M:** M3 cap ESR negligible; set C2 ESR = 1k ⇒ HF floor ≈ 1k/11k.
- **T:** `Trk` steps time/div with the sweep; aliasing expected past 10 kHz at dt 10 µs.

### 49. LC Low-Pass — 1 Vpk sweep 100 Hz–20 kHz, L=10m series, C=1µ shunt, R=100 load ⇒ f0 = 1591.5 Hz, Q = R√(C/L) = 1
- **Demo:** `DEMO_LOWPASS`, f_char 1591.5 Hz. Probe: C (output). Presets 200 µs/div, 0.5 V/div.
- **N:** 2nd order: |H(1 kHz)| = 1/√((1−0.395)² + 0.628²) = 1.15 (slight peak below f0); −40 dB/dec above. Oracle: **1.15 Vpk** at 1 kHz (±15 %, 8 ms).
- **V:** R→1k ⇒ Q = 10, resonant peak ≈ 10 Vpk at f0 (scope 2 V/div) · R→10 ⇒ Q 0.1, over-damped, looks first-order · L→100m ⇒ f0 503 Hz · C→100n ⇒ f0 5.03 kHz.
- **M:** M1 inductor DCR 0.1 Ω + cap ESR reduce the Q=10 peak to ~9 — visible; M0 ideal at R→1k with sweep parked on f0 must not grow unbounded.
- **Trap:** ideal L in series with ideal source at t=0 with source phase 90° ⇒ step into L; must not spike.

### 50. Zener Clipper — 10 Vpk 1 kHz, amplitude sweep 1–10 V, R=1k, 2× Zener 5.1 V back-to-back
- **Demo:** `DEMO_LIMITER`, f_char 1000 Hz (output stops growing once the input passes the clamp). Probe: node between R and the zener pair (`COMP_RESISTOR` 0, terminal 1). Presets 50 ms/div, 5 V/div. No `probe_cases[]` oracle (checked by the limiter contract).
- **N:** clamp at ±(Vz + Vf) = ±5.8 V; below 5.8 Vpk the output follows the input, at 10 Vpk the tops flatten and the zener current is (10−5.8)/1k = 4.2 mA.
- **V:** Vz→3.3 ⇒ clamp ±4.0 · remove one zener (short it) ⇒ asymmetric: +5.1 / −0.7 · R→100 ⇒ 42 mA, Rz·I visible slope on the flat top · R→100k ⇒ clamp still ±5.8 but the source sees almost no load.
- **M:** M2 real zener: knee is soft (smooth breakdown model) so the corner rounds; ideal: hard corner. Compare at 6–7 Vpk where the difference is largest.

### 51. Voltage Doubler — 5 Vpk 1 kHz, amplitude sweep 1–5 V, D1/D2, C1=C2=1µ, R load 100k
- **Demo:** `DEMO_ENVELOPE`, f_char 1000 Hz. Probe: source (CH1) and C2 (CH2). Presets 50 ms/div, 2 V/div.
- **N:** Vout = 2·Vpk − 2·0.7. Oracle: **7.4 V DC** at t = 1.0 s (±15 %) where the sweep is at A ≈ 4.4 V.
- **V:** disable the sweep, A = 5 ⇒ 8.6 V · R→1k ⇒ heavy load: ripple I/(fC) = 8.6m/(1k·1µ) ≈ 8 V, output collapses to ~3 V · C→10µ ⇒ ripple 10× smaller, slower rise (τ = RC = 1 s) · add a third D/C stage ⇒ 3·Vpk − 2.1 (Cockcroft-Walton).
- **M:** M2 real diode: Vf ≈ 0.55 at these µA currents ⇒ output ≈ 0.3 V higher than ideal.
- **T:** rise time ≈ 5·RC = 0.5 s — at 50 ms/div you see the charge-up staircase.

### 52. Relaxation Osc — ideal op-amp ±15 V, R=10k, C=100n, R1=R2=10k (β = 0.5), 0.1 V/20 µs start-up pulse via 100k
- **Demo:** `DEMO_OSC`, f_char 455 Hz (also in `--osc-test` at 40 ms window). Probe: op-amp OUT. Presets 1 ms/div, 5 V/div.
- **N:** f = 1/(2RC·ln((1+β)/(1−β))) = 1/(2·1 ms·ln 3) = **455 Hz**, square ±15 V; the C node is an exponential "triangle" between ±7.5 V.
- **V:** R→100k ⇒ 45.5 Hz · C→10n ⇒ 4.55 kHz (dt auto should follow) · R2→1k (β = 0.09) ⇒ f rises to 2.7 kHz and the triangle shrinks to ±1.4 V · R1→1k (β = 0.91) ⇒ 164 Hz.
- **M:** M4 real op-amp: slew rate rounds the edges, rails drop to ±14; f changes a few %. M0 ideal: exact.
- **Trap:** without the 100k kick an ideal op-amp sits at the metastable 0 V point — verify the pulse source restarts the oscillation after Reset.

### 53. HW Rect + Cap — 10 Vpk 60 Hz, amplitude sweep 2–10 V, D, C=100µ, R=1k
- **Demo:** `DEMO_ENVELOPE`, f_char 60 Hz. Probe: C (output). Presets 50 ms/div, 5 V/div.
- **N:** Vdc ≈ Vpk − 0.7 − ripple/2, ripple = I/(fC) = 9.3m/(60·100µ) = 1.55 V. Oracle: **8.0 V DC** at t = 1.0 s (±15 %) late in the sweep.
- **V:** C→10µ ⇒ ripple 15 V ⇒ output is nearly the raw half-wave · R→100 ⇒ ripple 15 V at full amplitude · C→1000µ ⇒ ripple 0.15 V · f→50 Hz ⇒ ripple ×1.2.
- **M:** M3 electrolytic ESR: the diode's current pulse produces a small step at each peak.
- **T:** at 50 ms/div the sawtooth is visible; at 5 ms/div count 60 Hz (not 120 Hz — half-wave).

---

## Power systems (added; 60 Hz, single-phase equivalents, phase-to-neutral peaks)

Source values are 60 Hz phase-to-neutral peaks: 345 kV → 281.7 kVpk, 138 kV → 112.7 kVpk,
12.47 kV → 10.18 kVpk, 18 kV → 14.7 kVpk. Lines are `COMP_TLINE` (length_mi × R/mi, X/mi,
B µS/mi; model 0 = R, 1 = R-L, 2 = nominal π). Transformers are ideal (V ratio N, current 1/N,
reflected). Load buses are probed on the load resistor (`COMP_RESISTOR` 0, terminal 0).

### 54. 345 kV Line — 281.7 kVpk, 100 mi twin Drake (R 0.06 Ω/mi, X 0.55 Ω/mi ⇒ 6 Ω + 145.9 mH), load 198.4 Ω (600 MW 3-φ)
- **Demo:** `DEMO_WAVEFORM`, 60 Hz. Probe: both ends. Presets 5 ms/div, 100 kV/div.
- **N:** I = 941 A rms; load 186.7 kV rms = **264 kVpk** (−6.3 %, mostly I·X, lagging). Oracle 264.0e3 (±5 %, 60 ms).
- **V:** length→200 mi ⇒ drop ~13 % · length→0.001 mi ⇒ load = source · model→0 (R only) ⇒ 0.03 % drop, no phase lag · model→2 (π) ⇒ +0.5 % charging rise · load→99 Ω (1200 MW) ⇒ ~13 % drop; load→10 MΩ ⇒ open line, no drop (Ferranti only with π model).
- **M:** all ideal already; the line's R is the only loss. Add an ideal 0 Ω load ⇒ fault current 281.7k/|6 + j55| = 5.1 kA pk, must stay finite.
- **T:** dt auto picks ~100 µs; 5 ms/div shows 1.2 cycles — set 20 ms/div for the phase-lag comparison.

### 55. 138 kV Line + VAR — 112.7 kVpk, 30 mi single Drake (3.9 Ω + 57.3 mH), load 171.5 Ω + 0.22 H (90 MW, pf 0.9 lag), SW → 6.1 µF cap bank
- **Demo:** `DEMO_WAVEFORM`, 60 Hz. Probe: source and load bus. Presets 5 ms/div, 50 kV/div.
- **N:** switch open: 74.3 kV rms = **105 kVpk** (−6.7 %). Oracle 105.0e3 (±6 %, 60 ms). Switch closed: bus recovers to ~78 kV rms (110 kVpk), phase lag shrinks.
- **V:** **toggle SW while running** — amplitude steps up within one cycle, no transient blow-up · cap bank→12 µF ⇒ over-compensated, bus above 80 kV (leading) · load L→0 (unity pf) ⇒ only 3 % drop · length→60 mi ⇒ −13 % open, cap bank recovers ~7 points.
- **M:** ideal switch closing an ideal C onto a bus with a series L: expect a damped ring at the cap/line resonance (√ of 57 mH·6.1 µF ⇒ 270 Hz) — plausible, not NaN.

### 56. 12.47 kV Feeder — 10.18 kVpk, 5 mi 1/0 ACSR (1.53 Ω + 8.22 mH), load 51.84 Ω (1 MW/phase)
- **Demo:** `DEMO_WAVEFORM`, 60 Hz. Probe: substation and feeder end. Presets 5 ms/div, 5 kV/div.
- **N:** I = 134.5 A; end 6,973 V rms = **9.86 kVpk** (−3.2 %). Oracle 9.86e3 (±4 %, 60 ms). Inside ANSI C84.1 ±5 %.
- **V:** load→26 Ω (2 MW) ⇒ −6.4 %, out of band · length→10 mi ⇒ −6.3 % · add a 2nd load resistor mid-line (rebuild as two 2.5 mi lines) ⇒ two-step profile · model→0 ⇒ 2.9 % (R dominates on a distribution feeder, unlike #54).
- **M:** n/a (ideal passives).

### 57. Pole Xfmr 120/240 — 7.2 kV feeder phase (10.18 kVpk), ideal xfmr N = 1/30, 240 V service, load 11.5 Ω (5 kW)
- **Demo:** `DEMO_WAVEFORM`, 60 Hz. Probe: 7.2 kV side (CH1, 5 kV/div manually) and 240 V side (CH2). Presets 5 ms/div, 100 V/div.
- **N:** 240 V rms = **339.4 Vpk**; 20.8 A rms on the house side, 0.69 A on the 7.2 kV side (ideal transformer: reflected current = I·N; the current view should show the 30:1 density change). Oracle 339.4 (±4 %, 60 ms).
- **V:** N→1/60 ⇒ 120 V · load→1.15 Ω (50 kW, 2× the can's rating) ⇒ still 240 V (ideal xfmr, no leakage) — document that sag needs a feeder in front (see #59) · N→30 (reversed) ⇒ 216 kV: scope must autoset to 100 kV/div, no overflow · load→open (10 MΩ) ⇒ 240 V, primary current ≈ 0.
- **M:** ideal transformer only. The realistic-transformer knobs (leakage) are gone — note in the block if they return.

### 58. Generator + GSU — 18 kV machine (14.7 kVpk), X'' 0.15 pu on 700 MVA (0.184 mH), GSU N = 19.17, 345 kV bus, load 198.4 Ω (600 MW)
- **Demo:** `DEMO_WAVEFORM`, 60 Hz. Probe: 18 kV terminals (CH1) and 345 kV bus (CH2) — 19× scale change. Presets 5 ms/div, 100 kV/div.
- **N:** referred X'' = 0.184 mH·19.17² ⇒ 25 Ω at 345 kV; at unity pf the bus is 281.7k·198.4/|198.4 + j25| = **279.4 kVpk** (−0.8 %). Oracle 279.4e3 (±4 %, 60 ms).
- **V:** load→99 Ω ⇒ −3 % · make the load lagging (add 0.5 H in series) ⇒ drop grows (the I·X of the machine now aligns with V) · X''→0.5 pu (0.61 mH) ⇒ −8 % · N→1 ⇒ 14.7 kVpk bus, 200× the current.
- **Trap:** ideal source + ideal L + ideal transformer: DC op point must not leave a current in the 0.184 mH.

### 59. Grid: 18 kV to 240 V — gen 18 kV → GSU 19.17 → 100 mi 345 kV → auto 345/138 (N 0.4) → 30 mi 138 kV → 138/12.47 (N 0.0903) → 5 mi feeder → pole xfmr (1/30) → house 11.5 Ω
- **Demo:** `DEMO_WAVEFORM`, 60 Hz. Probe: the house. Presets 5 ms/div, 100 V/div. Also probe every bus: 14.7 kVpk, 282, 113, 10.2 kVpk, 339 Vpk left to right.
- **N:** one house does not load the lines: house = 239 V rms = **339.4 Vpk** (−0.4 %). Oracle 339.4 (±4 %, 60 ms).
- **V:** scale the house to a town: load→11.5 mΩ (5 MW) ⇒ the 12.47 kV feeder and 138 kV line sag several % and the house sees ~225 V · scale further to 0.115 mΩ (500 MW) ⇒ the 345 kV drop of #54 appears at the house (~5 %) · set the 345 kV line model→2 ⇒ +0.5 % Ferranti rise propagates to the house · edit any length live ⇒ house amplitude steps.
- **T:** 7 transformers/lines in one loop; dt auto ~100 µs. Watch for the solver's condition number: 18 kV and 240 V nodes in one matrix (10⁵ dynamic range in V, 10⁴ in Ω) — any drift in the 240 V node is a scaling bug.

### 60. Ferranti (open line) — 281.7 kVpk, 200 mi 345 kV as π: 12 Ω + 291.8 mH, 2.12 µF each end, open end (10 MΩ), SW → 3.54 H shunt reactor
- **Demo:** `DEMO_WAVEFORM`, 60 Hz. Probe: both ends — far end is HIGHER than the source. Presets 5 ms/div, 100 kV/div.
- **N:** receiving end 1/(1 − ω²LC) with C = 2.12 µF, L = 291.8 mH ⇒ +9.9 % = **309.6 kVpk**. Oracle 309.6e3 (±5 %, 80 ms). Switch closed: the 3.54 H reactor (X = 1.33 kΩ) absorbs the far-end charging VARs ⇒ rise cancels (≈ 282 kVpk).
- **V:** **toggle SW while running** — rise collapses within a cycle; no ring-up · length→400 mi ⇒ +50 % (ω²LC → 0.33) — a real line this long is compensated · reactor→1 H ⇒ over-compensated, far end below the source · model→1 (R-L) ⇒ no C, rise vanishes (this is the point of the ladder, #64) · load the far end with 198 Ω ⇒ the rise turns into the −13 % drop.
- **Trap:** the switch closes an ideal 3.54 H across a charged 2.12 µF: L-C resonance at 58 Hz ≈ line frequency — a slow beat is expected, divergence is not.

### 64. Line Model Ladder — 112.7 kVpk source, three 30 mi 138 kV lines (0.13 Ω/mi, 0.72 Ω/mi, 6 µS/mi) into equal 90 MW loads; row 1 model 0 (R), row 2 model 1 (R-L), row 3 model 2 (π)
- **Demo:** `DEMO_WAVEFORM`, 60 Hz. Probe: row 2 load (`COMP_RESISTOR` 1). Presets 5 ms/div, 50 kV/div.
- **N:** row 1 (R only): 112.7·211.6/215.5 = **110.7 kVpk** · row 2 (R-L): **110.1 kVpk** (77.84 kV rms, oracle B in RESEARCH_TEXAS_GRID) · row 3 (π): **110.5 kVpk** (R-L plus a little charging rise). Oracles ±3 %, 60 ms; three `probe_cases[]` entries (resistor 0/1/2).
- **V:** click a line: length→100 mi ⇒ all three rows drop, row 3 rises relative to row 2 · X/mi→0 on row 2 ⇒ equals row 1 · B→60 µS/mi on row 3 ⇒ Ferranti visible even loaded · model→2 on row 1 ⇒ identical to row 3.
- **M:** n/a. **T:** all three loads share a source — the rows are independent (no coupling), verify by opening one load (10 MΩ).

### 65. Line Drop Basics — 12 V DC, wire R = 1 Ω, load 10 Ω
- **Demo:** `DEMO_DC`. Probe: load (`COMP_RESISTOR` 1). Presets 1 ms/div, 5 V/div.
- **N:** I = 12/11 = 1.09 A; load **10.909 V**; wire burns 1.19 W. Oracle 10.909 (±2 %, 5 ms).
- **V:** wire→2 Ω (double length) ⇒ 10.0 V · wire→0 ⇒ 12 V (zero-ohm resistor must not break the solve) · load→1 Ω ⇒ 6 V, wire burns 36 W · V→0 ⇒ 0 V.
- **M:** M1 source Rint adds to the wire; the wattmeter/ammeter reading must agree with V/R.

---

## Tesla coils (added; spark gap, toroid, coupled T-model)

Common to #61–#63 (`place_tesla`): NST = 170 Vpk 60 Hz through 10 Ω, ideal xfmr N = 75 ⇒ 12.75 kVpk
(9 kV rms), 56 kΩ referred (τ = 1.4 ms into 25 nF). Primary gap `COMP_SPARK_GAP` 3.2 mm ⇒ breakdown
3 kV/mm × 3.2 = 9.6 kV, r_on 1 Ω, hold 20 A, quench 1 µs. L1 = 29 µH, L2 = 30 mH, k = 0.2, T-model
(L1(1−k), k·L1, ideal 1:√(L2/L1) = 32, L2(1−k)), 2 Ω primary loss, 50 Ω secondary loss, 10 pF
secondary self-C, `COMP_TOROID` D×d inches (Bert Pool: C = (1.2781 − d/D)·2.8·√(π(D−d)d/4) pF),
streamer rod gap r_on 200 kΩ, hold 50 mA. Presets 20 µs/div, 100 kV/div. `DEMO_OSC` at the
secondary frequency; `--tesla-test` runs 20 ms at 100 ns and counts gap firings, streamer firings,
toroid peak and the ring frequency in the 60 µs after the first firing (±20 %).

### 61. Tesla Coil — C1 = 25 nF, toroid 4×13 in (14.5 pF), rod gap 40 mm
- **N:** f1 = 1/(2π√(29µ·25n)) = **186 kHz**; secondary 30 mH with 24.5 pF ⇒ 186 kHz (tuned). E = ½·25n·9.6k² = 1.15 J per bang; toroid ≥ **115 kV** (tesla_test vtop_min), streamer fires (≥1), gap fires ≥ 2 in 20 ms (every 8.3 ms half-cycle of the NST).
- **V:** gap→2 mm ⇒ 6 kV breakdown ⇒ bangs earlier in the NST cycle, more of them, less energy each (0.45 J) ⇒ lower toroid peak · gap→5 mm (15 kV > 12.75 kVpk) ⇒ never fires, C1 just follows the NST · C1→38n ⇒ primary at 152 kHz, detuned from the 186 kHz secondary (see #63 logic) · k→0.05 ⇒ slow energy transfer, many more ring cycles before the peak · toroid D/d→24×8 ⇒ this becomes #63.
- **M:** ideal everything except the 2 Ω / 50 Ω losses. quench_time→100 µs ⇒ the gap stays on through the whole burst (no "quench"), energy sloshes primary↔secondary with a beat.
- **T:** dt must be ≤ 100 ns (186 kHz ⇒ 54 samples/cycle); at dt 1 µs the ring aliases and the peak reads low. 20 µs/div shows one burst; 5 ms/div shows the 8.3 ms firing cadence.

### 62. Tesla Coil (big top) — C1 = 38 nF, toroid 8×24 in (26.5 pF), rod gap 45 mm
- **N:** secondary 30 mH with 36.5 pF ⇒ **152 kHz**; primary retuned 29 µH·38 nF ⇒ 152 kHz. E = 1.75 J per bang; toroid ≥ **130 kV** (vtop_min), streamer jumps 45 mm (135 kV) ≥ 1.
- **V:** edit the toroid D/d live and watch C and the ring frequency move · rod gap→60 mm (180 kV) ⇒ streamer never fires, toroid peak rises slightly (no streamer load) · C1→25n ⇒ detuned upward to 186 kHz.
- **Trap:** the streamer gap's 200 kΩ r_on across 26.5 pF: τ = 5 µs — a firing must decay the toroid, not instantly short it.

### 63. Tesla Coil (detuned) — C1 = 18 nF, toroid 8×24 in, rod gap 40 mm
- **N:** primary 29 µH·18 nF ⇒ **220 kHz**, secondary 152 kHz (45 % apart). Energy sloshes back; toroid peak must be **< 75 % of #62** (tesla_test pass rule) and the 40 mm rod (120 kV) never fires (rod_min 0). The ring frequency measured on the toroid is still ~152 kHz (the secondary's own resonance) — the test expects 152e3.
- **V:** fix it: C1→38n ⇒ becomes #62 · or toroid→4×13 in ⇒ secondary 186 kHz, still detuned from 220 kHz · C1→10n ⇒ 295 kHz, even worse · the sweep-free way to see tuning: step C1 18→25→30→38 nF while running and watch the envelope grow.
- **T:** as #61.

---

## Protection & control (added; AEP-style practice, `docs/RESEARCH_AEP_PC.md` §5.1–§5.8, 7 templates: #66–#72)

Status when this batch landed: 72/72 templates, 72/72 `--demo-test`, 66/66 `--probe-test`, 72/72 `--flow-test`,
50/72 `--geom-test` clean (all seven below are clean), `--param-test` all OK (now also §4b analog
switch as a fault switch, r_on 0.01 … 1e6 Ω under a 0/5 V pulse control, and §4c transformer as a
CT with N = 120 / 400 / 2875 into a 1 Ω burden), `--tesla-test` 3/3, layout-test 0 failures.

Common building blocks (`src/circuits.c`, above `place_pc_overcurrent`):
- **Fault switch** `fault_switch()`: `COMP_ANALOG_SWITCH` (r_on 0.3 Ω, r_off 1 GΩ, v_on 2.5 V) whose
  CTL pin is driven by a `COMP_PULSE_SOURCE` 0 → 5 V with `delay` / `pulse_width` / `period` set per
  template. The fault repeats every period, so the demo runs by itself: pre-fault → fault → relay
  decision on one scope screen. The fault resistance is the series R above the switch plus r_on.
- **Peak-hold detector** `peak_hold(C, R)`: diode → C to ground, bleed R to ground. Held value ≈ Vpk − 0.7 V
  (ideal diode: Vpk), decays with τ = RC after the input drops. Hold τ must be **shorter than the
  demo window** (see the traps) — 10 µF/10 k (τ 100 ms) on 50/51, 2.2 µF/10 k (τ 22 ms) on 87 and 21.
- **Comparator** `comparator_with_ref(vref)`: `COMP_OPAMP` with `ideal = false`, gain 1e5, ±15 V rails,
  DC reference on the − input, detector on the + input, 100 k load to ground on the output so
  TRIP is a real probed node. Output = +15 V (trip) / −15 V (no trip).
- **Instrument transformers**: ideal `COMP_TRANSFORMER` — CT turns_ratio 120 (600:5) or 400 (2000:5)
  with one secondary end grounded and a 1 Ω / 3.35 Ω burden; VT turns_ratio 1/2875 (345 kV / 120 V).
  §4c of `--param-test` confirms secondary current = primary / N into 1 Ω within 3 %.
- Sources are phase-to-neutral peaks: 13.8 kV → 11.27 kVpk (7.97 kV rms), 345 kV → 281.4/281.7 kVpk
  (199 kV rms), 765 kV → 624.6 kVpk (441.7 kV rms). Logic high is 5 V (`COMP_PULSE_SOURCE` v_high).

**Traps discovered while building these (apply to any future template):**
- Component terminal nodes snap to a **10 px grid** and `circuit_find_or_create_node` merges nodes
  **within 10 px** (5 px radius each side): a wire corner routed 10 px from a terminal silently
  shorts to it. Keep routing corners ≥ 20 px from any terminal and from other corners (the 87
  secondary loop and the 21 VT loop were both rebuilt for this; logic gates were placed at y+35 /
  y+65 so their ±15 px pins land on the grid).
- `ideal = true` op-amps are a **virtual-short model** (the solver forces V+ = V−) and cannot be used
  open-loop as comparators — the output just floats to whatever satisfies the constraint. Use
  `ideal = false` with gain 1e5; the rails then give a clean ±15 V decision.
- **Hold time constants must be shorter than the demo window.** `--demo-test` for `DEMO_SWITCH`
  measures the output only over the second half of a 6/f_char run and needs a > 2 V swing; if
  τ_hold keeps TRIP high past the end of the run the contract fails even though the relay is
  correct. That is why f_char was set per template (below) and why 87/21 use τ = 22 ms.
- Behavioural logic gates (`COMP_AND_GATE` …) have **no terminal currents**: the current view shows
  nothing in/out of the gate, the timer R sees the gate output as an ideal 0/5 V source, and
  `--flow-test` KCL is checked only on the passive nodes. Do not put an ammeter on a gate pin.
- The demo harness forces a usable dt for pulse-only circuits (no AC source ⇒ auto-dt has no
  frequency to work from): if auto dt is 0, > run/200 or < run/100000 it uses run/1000. In the GUI
  set dt manually (≈ 100 µs) on 50BF if the auto pick looks wrong.

### 66. CT + 50/51 Overcurrent — 11.27 kVpk (7.97 kV rms) 60 Hz, load 13.3 Ω (600 A), CT 120:1 into 1 Ω, hold 10 µF/10 k (τ 100 ms), ref 8.0 V, fault R 5 Ω via analog switch 40–100 ms (period 200 ms)
- **Demonstrates:** an instantaneous overcurrent (50) element: CT secondary current through a burden gives a voltage proportional to line current, the peak-hold turns it into a DC level, the comparator trips above the pickup. The 51 (time) curve is just a longer RC on V_hold (§5.1: R_t 100 k, C_t 10 µF ⇒ t_trip = τ·ln(V_c/(V_c − 8)): 1.65 s at 900 A, 0.90 s at 1200 A, 0.34 s at 2400 A).
- **Demo:** `DEMO_SWITCH`, f_char 30 ⇒ run 6/30 = 200 ms; the harness measures the second half (100–200 ms), so it sees TRIP high at 100 ms (end of the fault) and the release when the hold decays through 8 V — f_char 30 was chosen precisely so the 200 ms run contains that drop. Probe: op-amp OUT (TRIP). Presets 10 ms/div, 5 V/div.
- **N:** normal I1 = 7970/13.3 = 599 A rms ⇒ I2 = 5.0 A ⇒ V_b = 5.0 V rms = **7.07 Vpk**; hold ≈ 6.4 V < 8 V ⇒ TRIP = −15 V. Pickup: V_hold ≥ 8 ⇒ V_b ≥ 8.7 Vpk ⇒ I1 ≥ (8.7/7.07)·600 = **738 A**. During the fault the burden swings well above 8.7 Vpk ⇒ TRIP = **+15 V**. Oracles: burden (`COMP_RESISTOR` 0, term 0) **7.07 Vpk** (±6 %, 39 ms, pre-fault); op-amp OUT **max 15.0 V** (±5 %, 80 ms, during the fault).
- **Note the fault current:** the builder's R_f is 5 Ω (+0.3 Ω r_on) in parallel with the 13.3 Ω load ⇒ I1 ≈ 599 + 1504 ≈ **2.1 kA rms** (burden ≈ 25 Vpk), not the 1200 A quoted in the on-canvas note / §5.1 (1200 A total would need R_f ≈ 13 Ω). Pickup either way; the hold then needs τ·ln(25/8) ≈ 110 ms after 100 ms to release. If the release ever lands past the demo window, raise R_f to 13 Ω (V_b 14 Vpk ⇒ release ≈ 56 ms after the fault clears).
- **V:** pulse `delay` 40 → 120 ms ⇒ TRIP moves with it; `pulse_width` 60 → 10 ms ⇒ TRIP still fires (hold catches one peak), release timing unchanged · `period` 200 → 1000 ms ⇒ one fault per second · R_f 5 → 13 → 60 Ω ⇒ 2.1 kA / 1.2 kA / 700 A (700 A is just below pickup: no trip — the sharpest demo of the 738 A setting) · ref 8.0 → 12 V ⇒ pickup 1.1 kA; 8.0 → 5 V ⇒ trips on normal load (V_hold 6.4 V) · CT ratio 120 → 400 ⇒ V_b 2.1 Vpk normal, 7.4 Vpk fault: nothing trips until ref ≈ 6 · hold R 10 k → 100 k ⇒ τ 1 s: TRIP latches through several fault periods (this is the 51 behaviour in the wrong place) · C 10 µF → 1 µF ⇒ τ 10 ms, V_hold ripples at 60 Hz and TRIP chatters near pickup.
- **M:** M2 real diode: hold ≈ Vpk − 0.7 (6.4 V) vs ideal Vpk (7.07 V) — the ideal diode brings the normal-load hold within 0.9 V of the 8 V pickup; do not lower the reference below 7.5 V with ideal diodes. M4 op-amp is already `ideal=false`; setting it `ideal=true` breaks the comparator (virtual short) — expected, see traps.
- **T:** 10 ms/div shows 200 ms = one fault period; at 1 ms/div the 60 Hz burden waveform and the diode charging spikes are visible; dt auto (~100 µs) is fine, dt 10 ms aliases the 60 Hz input and the hold reads low.

### 67. 87 Line Differential — 11.27 kVpk, Rs 1 Ω, two CTs 120:1 bracketing the zone, load 20 Ω (380 A), 1 Ω differential burden R_d, hold 2.2 µF/10 k (τ 22 ms), ref 1.0 V; internal fault 2 Ω at 100–160 ms, through fault 2 Ω beyond CT2 at 240–300 ms (period 400 ms)
- **Demonstrates:** a current-differential zone. The CT secondaries are wired in opposition (A = CT1·S1 + CT2·S2 → R_d, B = CT1·S2 + CT2·S1 → ground) so equal current at both ends circulates and R_d sees I1 − I2. Only a fault *inside* the zone unbalances them.
- **Demo:** `DEMO_SWITCH`, f_char 20 ⇒ run 300 ms; the harness window (150–300 ms) sees TRIP high after the internal fault (100–160 ms) and the release ≈ 22·ln(30/1) ≈ 75 ms later (~235 ms) — hence τ 22 ms and f_char 20. The through fault at 240–300 ms must leave TRIP low. Probe: op-amp OUT (TRIP). Presets 20 ms/div, 5 V/div.
- **N:** no fault: I = 7970/21 = 380 A ⇒ 3.16 A in both secondaries, V_d = 0 ⇒ TRIP −15 V. Internal fault: R_par = 2.3 ∥ 20 = 2.06 Ω, I1 = 7970/3.06 ≈ 2.6 kA, I2 ≈ 260 A ⇒ (I1 − I2)/120 ≈ 20 A rms ⇒ **≈ 30 Vpk on R_d** ≫ 1 V ⇒ TRIP +15 V. Oracle: R_d (`COMP_RESISTOR` 4, term 0) **30.3 Vpk** (±10 %, 150 ms — the research value with a 2 Ω bolted fault: (2828 − 257)/120 × 1 Ω × √2). Through fault: ≈ 2.6 kA flows through both CTs, V_d ≈ 0 — no trip.
- **V:** swap the two `delay` values (internal 240, through 100) ⇒ TRIP appears in the second half instead · R_fi 2 → 20 Ω (high-resistance internal fault) ⇒ V_d ≈ 4 Vpk, still trips; 2 → 200 Ω ⇒ V_d ≈ 0.4 Vpk < 1 V ⇒ **misses** (the sensitivity limit of a plain 87 without slope) · ref 1.0 → 0.1 V and CT2 ratio 120 → 110 (9 % mismatch) ⇒ V_d ≈ 0.4 Vpk on load, ≈ 3 Vpk on the through fault ⇒ false trip — this is why real 87s add a slope (I_op > 0.3·I_restraint) · hold τ 22 → 220 ms ⇒ TRIP stays high into the through fault (looks like a mis-operation, it is only the hold) · R_d 1 → 10 Ω ⇒ V_d ×10 (the CTs are ideal so the burden does not saturate them).
- **M:** all ideal apart from the diode. Ideal transformers have no magnetising branch, so CT saturation on the through fault (the classic 87 problem) cannot be shown — note it on the canvas if a magnetising L is ever added.
- **T:** 20 ms/div shows one 400 ms period; 2 ms/div on R_d shows the 60 Hz difference current during the internal fault and its absence during the through fault.

### 68. 21 Distance Zone 1 — 281.4 kVpk (199 kV rms), source j10 Ω (26.5 mH), 50 mi line as 20 mi + 30 mi R-L segments (0.06 + j0.60 Ω/mi ⇒ Z1 = 3 + j30 Ω), load 500 Ω, CT 400:1 into a 3.35 Ω replica, VT 1/2875, holds 2.2 µF/10 k (τ 22 ms); bolted faults at 40 % (100–160 ms) and 100 % (240–300 ms), period 400 ms
- **Demonstrates:** a zone-1 impedance element. Reach = 0.8·|Z1| = 24.1 Ω primary ⇒ 24.1·400/2875 = **3.35 Ω secondary**; the replica R converts I_sec into |I|·Z_set, the VT gives |V|; TRIP when |I·Z_set| > |V|, i.e. |Z_app| = V/I < reach. The comparator's + input is the |I·Z| hold, the − input is the |V| hold (no DC reference — this template does not use `comparator_with_ref`).
- **Demo:** `DEMO_SWITCH`, f_char 20 ⇒ run 300 ms, window 150–300 ms: TRIP high at the end of the 40 % fault (160 ms), released ≈ 22 ms·ln(107/54) ≈ 15 ms later; the 100 % fault starts at 240 ms and must not trip. Probe: op-amp OUT (TRIP). Presets 20 ms/div, 5 V/div.
- **N:** pre-fault: V_sec = 281.4k/2875 = **97.9 Vpk**; I = 199k/|500 + 3 + j40| ≈ 397 A ⇒ I_sec ≈ 1 A ⇒ |I·Z| ≈ 4.7 Vpk ⇒ no trip. 40 % fault (1.2 + j12 Ω plus source j10): I = 199k/|1.2 + j22| = 9.03 kA, V_relay = 108.9 kV ⇒ V_sec 37.9 V rms (53.6 Vpk), V_I = 22.6 A × 3.35 = 75.6 V rms (107 Vpk) ⇒ **trip**. 80 %: 48.98 V vs 48.9 V — the balance point. 100 %: I = 4.96 kA, V_sec 52.0 V, V_I 41.6 V ⇒ **no trip** (zone 2's job, with a 0.3 s timer and 1.25·Z1 reach = 5.24 Ω secondary). Oracles: VT secondary (`COMP_TRANSFORMER` 1, term 2) **97.9 Vpk** (±6 %, 39 ms); op-amp OUT **max 15.0 V** (±5 %, 150 ms).
- **V:** **move the fault by editing the segment lengths** (keep the sum 50 mi): seg1 20 → 40 mi puts the first fault at 80 % ⇒ marginal, TRIP may flicker (balance point); seg1 20 → 45 mi (90 %) ⇒ no trip on either fault · replica 3.35 → 4.2 Ω (reach 100 %) ⇒ the 100 % fault now trips too — overreach, which is what zone 1 must never do · CT ratio 400 → 200 ⇒ |I·Z| doubles ⇒ effective reach doubles (same overreach) · VT ratio 1/2875 → 1/1437 ⇒ |V| doubles ⇒ reach halves · add fault resistance: put 5 Ω in series with sw1 ⇒ Z_app gains a real part, the 40 % fault still trips; 30 Ω ⇒ |Z_app| ≈ 33 Ω > 24.1 ⇒ misses (resistive-fault underreach) · pulse `delay` on sw1 100 → 240 and sw2 240 → 100 ⇒ the order flips; the second half must then be quiet · hold τ 22 → 220 ms ⇒ the trip from the 40 % fault is still latched when the 100 % fault arrives (false "overreach"; only the hold).
- **M:** ideal L/transformers; line segments are model 1 (R-L, B = 0) so no charging current disturbs the relay. Set model 2 on seg2 ⇒ ~0.2 % change, invisible.
- **T:** 20 ms/div; dt auto (~100 µs). The VT-side hold is routed below-left and its 60 Hz ripple is ~10 % at τ 22 ms — the comparator margin at 40 % (107 vs 54 Vpk) is far larger than the ripple.

### 69. 50BF Breaker Failure — TRIP pulse 5 V, delay 50 ms, width 300 ms, period 600 ms; 50BF current-present pulse identical; START = AND(TRIP, 50BF) → 10 k / 15 µF (τ 150 ms) → comparator ref 3.16 V (= 0.632 × 5 V) → BFT = AND(timer, 50BF), 100 k load
- **Demonstrates:** the breaker-failure timing chain (§5.8): a trip must make the current vanish within ~5 cycles; if TRIP AND current-present persists for a timer interval, BFT trips the adjacent breakers. The research write-up uses 12 V logic / 7.58 V; the builder uses the 5 V pulse sources and scales the reference to 0.632 × 5 = 3.16 V so the timer still expires at exactly one τ.
- **Demo:** `DEMO_SWITCH`, f_char 5 ⇒ run 1.2 s, window 0.6–1.2 s sees the second BFT pulse (800–950 ms); f_char 5 was chosen so a full 600 ms period plus the next pulse fit the run. Probe: AND gate 1 OUT (BFT). Presets 20 ms/div, 2 V/div. This is the only pulse-only template: the demo harness forces dt = run/1000 (auto-dt has no AC source to key on).
- **N:** stuck breaker (current pulse stays on): START at 50 ms, C reaches 3.16 V at 50 + 150 = **200 ms**, BFT high 200–350 ms (until the pulses end), repeats at 800–950 ms. Oracle: AND gate 1 OUT (`COMP_AND_GATE` 1, term 2) **max 5.0 V** (±5 %, 0.30 s). Healthy breaker: set the 50BF `pulse_width` 300 → **83 ms** (5 cycles): START drops at 133 ms, C only reaches 5·(1 − e^(−0.083/0.15)) = **2.13 V** < 3.16 ⇒ no BFT (§5.8 quotes 5.1 V vs 7.58 V on 12 V logic — same 42 %).
- **V:** 50BF width 300 → 83 → 120 → 150 ms ⇒ no BFT / no BFT (2.75 V) / BFT (3.16 V at 200 ms, marginal one-sample pulse) — the margin around 5 cycles · R 10 k → 5 k ⇒ τ 75 ms ⇒ BFT at 125 ms (would misoperate on a healthy 83 ms breaker) · C 15 µF → 30 µF ⇒ τ 300 ms, C reaches only 3.16 V at 350 ms = exactly when the pulses end ⇒ marginal · ref 3.16 → 4.5 V ⇒ C needs 2.3 τ = 345 ms ⇒ marginal · TRIP `delay` 50 → 100 ms with the current pulse unchanged ⇒ START starts 50 ms later, BFT at 250 ms · remove the current pulse (v_high 0) ⇒ no START, no BFT, C stays at 0 · period 600 → 400 ms with width 300 ⇒ only 100 ms off: C does not fully reset (τ 150) and the next BFT arrives early — the reset path is the R back into the gate's 0 V output.
- **M:** gates are behavioural (no terminal currents, ideal 0/5 V outputs); the comparator's +15 V into the AND input is read as logic high. No ideal/real toggle applies except the op-amp (must stay `ideal=false`).
- **T:** 20 ms/div shows 400 ms; set 100 ms/div to see two periods. dt auto may be coarse (no AC source): use 100 µs. The capacitor ramp is the best node to probe for the "healthy" case.

### 70. SIL Loading — 281.7 kVpk 60 Hz, 200 mi 345 kV nominal π (0.06 + j0.60 Ω/mi, 7.5 µS/mi ⇒ 12 Ω + 318 mH, 1.99 µF each end), load 283 Ω = Zc; SW adds a second 283 Ω (2 × SIL)
- **Demonstrates:** surge impedance loading — a line terminated in Zc = √(x/b) = √(0.60/7.5e-6) = 283 Ω generates exactly the VARs it absorbs: flat profile, small angle. P_SIL = 345²/283 = 420 MW. At 2 × SIL the line needs VARs it cannot supply and the far end sags (voltage-limited, St. Clair).
- **Demo:** `DEMO_WAVEFORM`, f_char 60. Probe: load (`COMP_RESISTOR` 0, term 0) plus the source end. Presets 5 ms/div, 100 kV/div.
- **N:** lossless π oracle at SIL (§5.6): Vr/Vs = 0.996; with R = 12 Ω and the single nominal π the probe oracle is **269.25 kVpk (0.956)** (±3 %, 60 ms). SW closed (141 Ω): Vr/Vs ≈ **0.80** (§5.6: 140.6/175.0 = 0.803). Open end: 1.099 (Ferranti, #60).
- **V:** **toggle SW while running** ⇒ far end steps 0.956 → 0.80 within a cycle, no growing ring · length 200 → 100 / 300 mi ⇒ at SIL the profile stays ≈ flat regardless (that is the point); at 2 × SIL the 300 mi drop is far worse — trace the St. Clair curve · load 283 → 566 Ω (0.5 SIL) ⇒ far end *rises* above the source (partial Ferranti) · line model 2 → 1 (no C) ⇒ SIL flatness disappears, plain R-L drop even at 283 Ω · B 7.5 → 0 µS/mi ⇒ same.
- **M:** ideal passives. An ideal SPST switch closing a resistor onto the far-end π capacitor: no transient issues expected (resistive).
- **T:** one π for 200 mi is coarse (the note on #72 says the same); split into 2 × 100 mi to see the mid-point voltage.

### 71. Series Compensation — 281.7 kVpk, two 100 mi 345 kV π sections (X_line = 120 Ω total), 44.21 µF series capacitor between them (Xc = 60 Ω = 50 % compensation), bypass SW across the cap, load 141.5 Ω (2 × SIL)
- **Demonstrates:** series capacitors cancel part of the line reactance and restore the voltage / power limit of a long line. AEP uses them on long 765/345 kV paths (with MOV bypass on faults and SSR protection).
- **Demo:** `DEMO_WAVEFORM`, f_char 60. Probe: load (`COMP_RESISTOR` 0, term 0) and the source end. Presets 5 ms/div, 100 kV/div.
- **N:** with the cap in: Vr/Vs = **0.890 ⇒ 250.6 kVpk** (±4 %, 60 ms, two-π phasor oracle). SW closed (bypass): back to the 2 × SIL drop of #70, ≈ **0.80** (§5.5: open-end rise halves from 1.099 to 1.047 with 50 % compensation — try it with the load at 10 MΩ).
- **V:** **close SW while running** ⇒ the load end drops 0.89 → 0.80 within a cycle, the source-side probe barely moves · C 44.2 → 22.1 µF (Xc 120 Ω, 100 % compensation) ⇒ far end ≈ source; → 14.7 µF (150 %, over-compensated) ⇒ far end *above* the source and the cap voltage (I·Xc) is huge — real installations stay at 25–70 % · C → 88 µF (25 %) ⇒ ≈ 0.85 · load 141.5 → 283 Ω (SIL) ⇒ ≈ flat with or without the cap · load → 10 MΩ ⇒ Ferranti 1.099 bypassed, 1.047 compensated.
- **M:** ideal capacitor in series with the ideal inductors of the two π sections: the series L-C (318 mH with 44 µF ⇒ 42 Hz, i.e. 60 Hz·√0.5) is below 60 Hz — a slow ~42 Hz beat after SW opens/closes is the subsynchronous-resonance mechanism in miniature; it must decay (12 Ω of line R), not grow. The bypass switch is ideal: closing it on a charged cap dumps the cap charge as a current spike, finite at r_on.
- **T:** dt auto ~100 µs; 5 ms/div for amplitude, 50 ms/div to see the ~42 Hz beat after a switch operation.

### 72. 765 kV Line (AEP) — 624.6 kVpk (441.7 kV rms = 765/√3), 300 mi six-conductor bundle as one nominal π (0.02 + j0.53 Ω/mi, 8.5 µS/mi ⇒ 6 Ω + 422 mH, 3.38 µF each end), load 250 Ω = Zc (SIL ≈ 2340 MW)
- **Demonstrates:** AEP's EHV backbone since 1969: bundling lowers X and raises B, Zc = √(0.53/8.5e-6) = 250 Ω, SIL = 765²/250 = 2340 MW ≈ 6 × a 345 kV circuit at about half the losses per MW. Built with `chain_line_load` (same shape as #54).
- **Demo:** `DEMO_WAVEFORM`, f_char 60. Probe: load (`COMP_RESISTOR` 0, term 0) and the source end. Presets 5 ms/div, **200 kV/div**.
- **N:** at SIL, single π: Vr/Vs = **0.958 ⇒ 598.6 kVpk** (±4 %, 60 ms). Lossless would be ~0.99: the 6 Ω R and the coarse 300 mi π account for the rest.
- **V:** load 250 → 125 Ω (2 × SIL, ~4700 MW) ⇒ the drop of #70 reappears at 765 kV scale (~0.8) · length 300 → 600 mi (one π) ⇒ ω²LC → 0.8 (the single π is near its own resonance and overstates everything): **split into 3 × 100 mi** `COMP_TLINE` parts and compare — the ladder of 3 sections sits ~1 % from the single π at 300 mi but diverges at 600 · R 0.02 → 0.06 Ω/mi (345 kV-class conductor) ⇒ losses ×3 · B 8.5 → 7.5 ⇒ Zc 266 Ω, no longer at SIL with 250 Ω · load → 10 MΩ ⇒ Ferranti at 300 mi: 1/(1 − ω²LC) ≈ 1.25 — the reason long 765 kV lines carry shunt reactors (#60).
- **M:** ideal passives. Scope autoset must reach 200 kV/div without overflow (largest voltage in any template).
- **T:** as #54; 20 ms/div for the phase-angle comparison between the two ends.

---

## Three-phase (added; 60 Hz, three `COMP_AC_VOLTAGE` sources at 0 / −120 / +120°, 4 templates: #73–#76)

Current status: 81/81 templates, 81/81 `--demo-test`, 73/73 `--probe-test`, 81/81 `--flow-test`,
59/81 `--geom-test` clean (all nine of #73–#81 are clean), `--osc-test` 7/7 (per-case dt, see the
signal-generator section), `--param-test` all OK, `--tesla-test` 3/3, knob test 970 runs 0 failed.

Common building blocks (`src/circuits.c`, `ph_source` / `place_3ph_y`):
- **Phase sources** `ph_source(vpk, deg)`: an `ac_source` with `props.ac_voltage.phase` set to 0, −120,
  +120 (A-B-C sequence). Swap two phase angles to reverse the sequence.
- **Y layout**: three rows 140 px apart, source → series line R → load R → neutral bus at x+300 →
  R_n (1 Ω, rotated 90) → ground. The neutral bus is a real probed node.
- **Extra probes**: a new `template_extra_probes[type][3]` table adds up to three scope probes on
  top of the auto probe so *all* phases appear on load — e.g. #73 auto-probes the phase B load
  (`COMP_RESISTOR` 3) and adds phase C (5) and the neutral (6); the source probe covers phase A.
  Use the scope **Stack** button to separate them.
- Sources are phase-to-neutral peaks: 277 V rms → 392 Vpk (480 V line-line), 345 kV → 281.7 kVpk,
  120 V rms → 170 Vpk (rectifier).

**Traps:** the 10 px node-merge rule applies to the three stacked rows (140 px pitch keeps corners
clear); the neutral resistor is the *seventh* resistor (ordinal 6) in the Y templates, so probe/oracle
ordinals shift if a resistor is inserted; an open neutral (1 MΩ) makes the neutral shift ~144 Vpk — set
the neutral channel to 50 V/div before trying it.

### 73. 3-Phase Y Balanced — 392 Vpk (277 V rms, 480 V L-L) × 3 at 0/−120/+120°, 0.5 Ω line R each, 10 Ω loads, neutral 1 Ω to ground
- **Demonstrates:** in a balanced Y the three load currents sum to zero at every instant, the neutral carries nothing and the total power is constant (not pulsating). V_LL = √3 · V_LN; phase order A-B-C sets motor direction.
- **Demo:** `DEMO_WAVEFORM`, f_char 60. Probe: phase B load (`COMP_RESISTOR` 3, term 0) auto; extras phase C load (5) and neutral (6); the source probe is phase A. Presets 5 ms/div, 100 V/div.
- **N:** each load = 392 · 10/10.5 = **373.3 Vpk** (`--probe-test` ±3 %, 60 ms), 120° apart; neutral ≈ **0 V** (< 1 V — solver noise only). Load current 37.3 Apk, I_n = 0.
- **V:** neutral R 1 → **1 mΩ** (solid) and → **1 MΩ** (open): balanced ⇒ nothing changes on any channel (that is the point) · load unbalance: phase C 10 → 40 Ω ⇒ neutral rises to ~10 Vpk and phase C load reads ≈ 386 Vpk (becomes #74) · swap the B and C phase angles (−120 ↔ +120) ⇒ same amplitudes, reversed sequence on the Stack view · line R 0.5 → 5 Ω ⇒ loads 261 Vpk · frequency 60 → 50 Hz ⇒ same amplitudes, 20 ms period.
- **M:** resistors only — no model matrix; the only "model" is the phase property, which must survive save/load.
- **T:** dt auto ~100 µs is fine; 5 ms/div shows 1.2 cycles, 2 ms/div to read the 120° spacing (5.56 ms) with cursors.

### 74. 3-Phase Unbalanced — as #73 but loads 10 / 20 / 40 Ω (A / B / C), neutral 1 Ω
- **Demonstrates:** unequal loads ⇒ I_A + I_B + I_C ≠ 0 and the difference returns through the neutral; with neutral impedance the star point shifts and the lightly loaded phases see *more* than nominal — the "lost neutral" hazard.
- **Demo:** `DEMO_WAVEFORM`, f_char 60. Probe: neutral (`COMP_RESISTOR` 6, term 0) auto; extras phase B load (3) and phase C load (5). Presets 5 ms/div, 100 V/div.
- **N:** phasor: V_n = Σ(V_k/Z_k) / (Σ 1/Z_k + 1/R_n) with Z_k = 10.5, 20.5, 40.5 Ω ⇒ |V_n| = **20.83 Vpk** (`--probe-test` ±5 %, 60 ms), lagging phase A by ≈ 20°. Loads: A ≈ 355 Vpk, B ≈ 387 Vpk, **C ≈ 403 Vpk** (above the 392 V source). I_n ≈ 20.8 Apk.
- **V:** neutral R → **1 mΩ** (solid neutral) ⇒ V_n ≈ 0.02 V, loads 373 / 382 / 387 Vpk (each phase independent) · neutral R → **1 MΩ** (open neutral) ⇒ V_n ≈ **144 Vpk**, phase C load ≈ 500 Vpk, phase A ≈ 250 Vpk — use 50 V/div on the neutral channel · load C 40 → 10 Ω ⇒ back to #73 (V_n → 0) · load A 10 → 1 Ω ⇒ V_n ≈ 100 Vpk and the neutral R dissipates 5 kW: extreme unbalance · reverse the sequence (swap B/C angles) ⇒ |V_n| unchanged, its phase mirrors.
- **M:** resistors only. Check the "lost neutral" case does not produce NaN — it is a plain resistive network at any R_n.
- **T:** as #73. Use the FFT on the neutral: pure 60 Hz (no harmonics — linear loads).

### 75. 3-Phase 345 kV Line — 281.7 kVpk × 3 at 0/−120/+120°, 100 mi twin Drake per phase (`COMP_TLINE` model 1: 6 Ω + 145.9 mH), 198.4 Ω loads (600 MW), neutral 1 Ω
- **Demonstrates:** the single-phase 345 kV example (#54) done for all three phases: a balanced system is solved per phase and multiplied by three, which is why the other power templates are single-phase equivalents. Unbalancing one load shows zero-sequence current in the neutral.
- **Demo:** `DEMO_WAVEFORM`, f_char 60. Probe: phase B load (`COMP_RESISTOR` 1, term 0) auto; extras phase A (0) and phase C (2). Presets 5 ms/div, **100 kV/div**.
- **N:** every phase drops the same 6.3 % as #54: **264 kVpk** (`--probe-test` ±5 %, 60 ms), 120° apart; neutral ≈ 0.
- **V:** load C 198.4 → 400 Ω ⇒ phase C ≈ 273 kVpk, neutral ≈ 25 kVpk at 60 Hz · load C → 10 MΩ (open phase) ⇒ neutral ≈ 90 kVpk · neutral R 1 → 1 MΩ with one load unbalanced ⇒ the star point floats and the healthy phases are overvolted (EHV systems are solidly grounded for exactly this reason) · length 100 → 200 mi on one phase only ⇒ that phase drops more, neutral current appears · all loads 198.4 → 99.2 Ω ⇒ 2 × 600 MW, each phase ≈ 0.88.
- **M:** tline model 1 (R-L) on all three; switch one phase to model 2 (π) and confirm the balanced answer barely moves at 100 mi (#64 shows the difference).
- **T:** as #54; 200 kV/div if the open-neutral variation is tried.

### 76. 3-Phase 6-Pulse Rect — 170 Vpk (120 V rms) × 3 at 0/−120/+120°, six diodes (upper K → plus bus, lower A → minus bus), neutral grounded, load 100 Ω between the buses
- **Demonstrates:** the plus bus follows the highest phase and the minus bus the lowest, so V+ − V− = V_LL,pk · cos(0…30°): DC ≈ 1.35 · V_LL,rms = 2.34 · V_LN,rms ≈ 280 V with only 4 % ripple at 6 × 60 = 360 Hz. Each diode conducts 120° per cycle. This is the front end of every VFD and HVDC pole.
- **Demo:** `DEMO_WAVEFORM`, f_char 60. Probe: plus bus (`COMP_RESISTOR` 0, term 0) auto; extra minus bus (`COMP_DIODE` 5, term 0); phase A from the source. Presets 5 ms/div, 50 V/div.
- **N:** plus bus peak = 170 − 0.7 = **169.3 V** (`--probe-test` "max" ±3 %, 60 ms); minus bus mirror −169.3 V; V+ − V− swings 294 · cos 30° − 1.4 ≈ **254 V** to 293 V (mean ≈ 279 V), 360 Hz ripple; load current ≈ 2.8 A; each diode 120° at ≈ 2.8 A.
- **V:** add 100 µF across the load ⇒ ripple shrinks from ~40 V to a few volts (the cap only bridges the ~30° dip between adjacent line-line peaks, not a full 1/360 s — the single-phase I/(fC) rule badly overestimates it; measure it) · remove one phase source (set amplitude 0) ⇒ the bridge degrades toward single-phase full-wave: 120 Hz ripple, deep 40 % dips · load 100 → 10 Ω ⇒ 28 A, diode drop matters (169.3 → ≈ 168.5) · reverse the sequence (swap B/C angles) ⇒ identical bus waveforms — a rectifier does not care about sequence · frequency 60 → 50 Hz ⇒ 300 Hz ripple.
- **M:** ideal diodes ⇒ plus bus peak 170.0 exactly; realistic Is = 1e-9 ⇒ 169.3. No NaN when two diodes commutate at the 30° crossover (Newton takes an extra iteration there).
- **T:** dt auto ~100 µs gives 46 points per ripple period — set 20 µs to resolve the commutation notch; 1 ms/div to count six pulses per 16.7 ms.

---

## Signal generators (added; Sedra & Smith ch. 18, `docs/RESEARCH_OSCILLATORS.md`, 5 templates: #77–#81)

Common building blocks (`src/circuits.c`, `sat_opamp` / `place_tri_square_core`):
- **Saturating op-amp** `sat_opamp()`: `COMP_OPAMP` with `ideal = false`, gain 1e5, ±15 V rails. The
  `ideal = true` virtual-short model **cannot** be used in a positive-feedback (bistable) or integrator
  role — the solver forces V+ = V− and the loop is dead or floats. Every op-amp in #77–#79 is finite-gain.
- **Kick sources**: a `COMP_PULSE_SOURCE` with `period` 100 s (one-shot) breaks the perfect
  equilibrium at t = 0: 0.5 V / 50 µs on the bistable input (#78, #79), 0.3 V / 50 ns on the Colpitts
  gate (#80), 3 V / 2 µs on the first RC of the ring (#81).
- `--osc-test` now carries a **per-case dt** (0 = the `--osc-dt` default of 1 µs): 200 ns for the
  5 kHz generators, 5 ns for the 712 kHz Colpitts, 20 ns for the ring, and it takes the output node
  from the template's `template_output[]` spec instead of "the first op-amp".
- `--flow-test` exempts behavioural logic gates (#81): KCL is asserted only on the passive nodes.

**Traps discovered while building these:**
- A **forgotten wire** (R2 of the bistable to the + input) made the triangle generator dead-quiet: the
  op-amp sat at a rail and nothing rang. `--trace "Triangle/Square Gen" 0.004` found it in one run
  (the + input node had min = max). Run `--trace` on every new oscillator before hunting in the engine.
- `ideal = true` op-amps in positive feedback: see above. The finite-gain op-amp integrates fine
  (gain 1e5 ⇒ 0.001 % droop per cycle).
- The Colpitts needs dt ≤ 10 ns (period 1.4 µs): the app's auto-dt has no AC source to work from, so
  the template's preset dt matters; the demo harness forces run/1000 if auto-dt is unusable.

### 77. Bistable (Schmitt) — 10 Vpk 100 Hz triangle in, inverting op-amp (finite gain, ±15 V), R1 = R2 = 10 k from OUT to + and + to ground, 100 k load
- **Demonstrates:** (S&S 18.4) positive feedback turns the op-amp into a latch. With OUT at +15 V the + input sits at +7.5 V, so the input must exceed +7.5 V to flip to −15 V and then fall below −7.5 V to flip back. V_TH = L+ · R1/(R1 + R2), V_TL = L− · R1/(R1 + R2) — 15 V of hysteresis rejects noise near the threshold.
- **Demo:** `DEMO_SWITCH`, f_char 100. Probe: OUT (`COMP_OPAMP` 0, term 2) auto; the triangle from the source probe. Presets 2 ms/div, 5 V/div.
- **N:** OUT = **±15 V** (`--probe-test` "max" 15.0 ±5 %, 30 ms); two edges per input cycle at input = **+7.5 V** (falling edge of OUT) and **−7.5 V** (rising edge), i.e. 2.5 ms apart then 7.5 ms. **X-Y view** (Y-T button, X = triangle, Y = OUT) draws the rectangular hysteresis loop 7.5 V wide, 30 V tall.
- **V:** **R1 10 → 20 k** (R2 10 k) ⇒ thresholds ±10 V: the 10 V triangle just barely reaches them, output may stop switching (raise the triangle to 12 V) · **R2 10 → 30 k** ⇒ thresholds ±3.75 V, edges 0.75 ms after each zero crossing · triangle 10 → 5 Vpk ⇒ never crosses ±7.5 V ⇒ OUT stays at one rail (the demo contract would fail — expected) · rails ±15 → ±10 V ⇒ thresholds ±5 V · frequency 100 → 1 kHz ⇒ same thresholds, edge timing scales.
- **M:** set the op-amp `ideal = true` ⇒ **latch dead** (virtual-short model, see the traps); back to `ideal = false`. Gain 1e5 → 1e3 ⇒ thresholds unchanged, the edges get a visible 30 mV slope.
- **T:** dt auto ~10 µs; 2 ms/div to see the edges, 500 µs/div with cursors to read the 7.5 V crossing on the triangle.

### 78. Triangle/Square Gen — non-inverting bistable (R1 10 k from integrator, R2 20 k from OUT ⇒ thresholds ±15 · R1/R2 = ±7.5 V) driving an inverting integrator (R 10 k, C 10 nF), 0.5 V/50 µs kick
- **Demonstrates:** (S&S 18.5.2) the integrator ramps at L/RC = 15 V/100 µs = 0.15 V/µs until it hits a bistable threshold, the bistable flips and the ramp reverses. f = R2/(4 R C R1) = 20 k/(4 · 10 k · 10 n · 10 k) = **5 kHz**; the rails cancel out of f. Frequency: R or C; amplitude: R1/R2.
- **Demo:** `DEMO_OSC`, f_char 5000; `--osc-test` 4 ms window, dt 200 ns, expects 5000 Hz (**measured 5000 Hz**). Probe: triangle (`COMP_OPAMP` 1, term 2) auto; extra square (`COMP_OPAMP` 0, term 2). Presets 100 µs/div, 5 V/div.
- **N:** triangle **±7.5 V** (`--probe-test` "amp" 7.5 ±8 %, 3 ms), square ±15 V, both at 5.00 kHz; the square edges sit exactly at the triangle peaks (Stack view).
- **V:** **R 10 → 20 k** ⇒ 2.5 kHz, amplitude unchanged · **C 10 → 4.7 nF** ⇒ 10.6 kHz · **R1 10 → 5 k** ⇒ triangle ±3.75 V and f doubles to 10 kHz (f ∝ R2/R1) · **R2 20 → 40 k** ⇒ ±3.75 V, 10 kHz likewise; R2 → 10 k ⇒ ±15 V thresholds equal the rails, the loop stalls — expected · rails ±15 → ±10 V ⇒ triangle ±5 V, f unchanged (the trick) · apply each **live**: the ramp continues from its current value, no time reset.
- **M:** both op-amps `ideal = false` gain 1e5. Either one at `ideal = true` ⇒ dead (see the traps; this is the failure the forgotten-wire trace also produced). Gain 1e5 → 1e3 ⇒ f drops ~1 % (finite-gain integrator).
- **T:** dt must be ≤ 1 µs (200 ns in `--osc-test`); at 5 µs the triangle peaks overshoot the thresholds by one step (≈ 0.75 V) and f reads low — a time-base check, not a model bug. 100 µs/div shows 2 cycles.

### 79. Function Generator — #78 core + R_in 10 k into a 3-breakpoint diode shaper: 22 k to +2.0 V, 5.6 k to +3.7 V, and mirror branches (22 k to −2.0 V, 5.6 k to −3.7 V, diodes reversed)
- **Demonstrates:** (S&S 18.8.2) piecewise-linear sine shaping. Below |2.6 V| nothing conducts (slope 1); above it the 22 k branch loads the node (slope 22/(10 + 22) = 0.69); above |4.3 V| the 5.6 k branch too (slope 0.31). Three breakpoints per half-cycle turn the ±7.5 V triangle into a **~4.9 V sine** with THD of a few %.
- **Demo:** `DEMO_OSC`, f_char 5000; `--osc-test` 4 ms, dt 200 ns, 5000 Hz. Probe: shaper output (`COMP_RESISTOR` 3, term 1) auto; extra triangle (`COMP_OPAMP` 1, term 2). Presets 100 µs/div, 2 V/div.
- **N:** sine **≈ 4.9 Vpk** (`--probe-test` "amp" 4.9 ±15 %, 3 ms) at 5.00 kHz. **FFT**: 3rd harmonic > 30 dB below the fundamental, no even harmonics (symmetric breakpoints).
- **V:** **R 10 → 20 k** ⇒ 2.5 kHz, shape unchanged (the shaper is amplitude-only) · **C 10 → 4.7 nF** ⇒ 10.6 kHz · **R2 20 → 40 k** (amplitude) ⇒ triangle ±3.75 V ⇒ only the first breakpoint is reached: output ≈ 3.4 Vpk triangle-with-rounded-tops, 3rd harmonic rises to ~−20 dB — then **re-scale the bias sources** (2.0 → 1.0 V, 3.7 → 1.85 V) to restore the sine at half amplitude · bias 3.7 → 5 V ⇒ second breakpoint never reached, pointed peaks · 22 k → 10 k ⇒ over-flattened tops (3rd harmonic phase flips) · set one diode ideal ⇒ that breakpoint moves by 0.6 V, asymmetric output ⇒ even harmonics appear in the FFT.
- **M:** op-amps as #78. Diodes: all ideal ⇒ breakpoints at exactly ±2.0 / ±3.7 V (slightly lower amplitude ≈ 4.6 V); realistic (Is = 1e-9) ⇒ ±2.6 / ±4.3 V as designed.
- **T:** as #78. Persistence + FFT at 100 µs/div; the FFT needs ≥ 10 cycles in the record — use 200 µs/div for a clean harmonic readout.

### 80. Colpitts (MOSFET) — 12 V, NMOS common source, tank L 100 µH with C1 = C2 = 1 nF (drain–ground / gate–ground), 1 mH RFC to the drain, 10 nF coupling cap, 1 M/1 M gate bias (6 V), 0.3 V/50 ns kick on the gate
- **Demonstrates:** (S&S 18.3.1) the capacitive divider feeds C1/C2 of the drain swing back to the gate; oscillation needs g_m R_tank > C2/C1 (= 1 here). f = 1/(2π √(L · C1C2/(C1 + C2))) = 1/(2π √(100 µ · 0.5 n)) = **712 kHz**. The RFC is open at RF but passes the drain DC; amplitude limits by cut-off each cycle (class C).
- **Demo:** `DEMO_OSC`, f_char 712e3; `--osc-test` 60 µs window, dt 5 ns, expects 712 kHz (**measured 710 kHz**). Probe: drain (`COMP_NMOS` 0, term 1) auto. Presets 500 ns/div, 5 V/div.
- **N:** drain swings several volts around the 12 V RFC rail (up to ≈ 2 × VDD on a good cycle), **710–712 kHz**; the gate rides on the 6 V bias with a C1/C2 = 1 share of the drain swing.
- **V:** **C1 1 → 2 nF** ⇒ C_eq = 0.667 nF ⇒ **616 kHz** (the on-canvas note says 581 kHz — that figure corresponds to C_eq = 0.75 nF, i.e. C1 = 3 nF; fix the note text in `template_notes[]` or accept 616) · C1 → 0.5 nF ⇒ C_eq 0.333 nF ⇒ 872 kHz, and the feedback fraction C1/C2 halves — check it still starts · **C2 1 → 2 nF** ⇒ 616 kHz too, but now C2/C1 = 2 needs g_m R_tank > 2 — may fail to start: raise kp or VDD · L 100 → 47 µH ⇒ 1.04 MHz (dt must follow) · RFC 1 mH → 10 µH ⇒ no longer an RF open: the tank is loaded, amplitude collapses · remove the kick (v_high 0) ⇒ starts from numerical noise only, much later.
- **M:** NMOS is the only nonlinear part — set vth 1.5 → 3 V to lower g_m until it stops (start-up criterion), kp × 10 to see hard class-C limiting (flat-bottomed drain). Ideal inductors/caps.
- **T:** **dt ≤ 10 ns** (5 ns in `--osc-test`, ~280 points per cycle); at 100 ns the tank is under-sampled and f reads 5 % low; at 1 µs it does not start at all — a time-base check. 500 ns/div shows 3.5 cycles; 20 µs/div for the start-up envelope.

### 81. Ring Oscillator — five `COMP_NOT_GATE` in a loop, each followed by R 1 k / C 1 nF to ground, 3 V/2 µs kick on the first RC
- **Demonstrates:** an odd number of inverters cannot settle: the signal returns inverted and the ring keeps flipping, period = 2 N t_pd. Here t_pd ≈ 0.69 RC = 0.69 µs (the gate flips when its RC input crosses the 2.5 V threshold) ⇒ f ≈ 1/(2 · 5 · 0.69 µs) ≈ **145 kHz**.
- **Demo:** `DEMO_OSC`, f_char 145e3; `--osc-test` 200 µs window, dt 20 ns, expects 145 kHz (**measured 139 kHz** — the RC output does not start from exactly 0/5 V each half-cycle, so the effective delay is a little more than 0.69 RC). Probe: last stage output (`COMP_NOT_GATE` 4, term 1) auto. Presets 2 µs/div, 2 V/div.
- **N:** 0/5 V square at **~139–145 kHz** on every gate output; probe several stages ⇒ five squares each shifted by one fifth of a half-period (0.7 µs); the RC nodes are exponential ramps between the rails.
- **V:** **per-stage C**: one C 1 → 2 nF ⇒ that stage delays 1.4 µs, period 6.9 → 8.3 µs ⇒ ≈ **120 kHz**; all five C → 2 nF ⇒ ≈ 72 kHz; one C → 0.1 nF ⇒ ≈ 160 kHz (the other four dominate) · one R 1 k → 10 k ⇒ same as C × 10 on that stage ⇒ ≈ 55 kHz · remove one stage (4 inverters, even) ⇒ **latches**, no oscillation — the demo contract fails, expected · add two stages (7) ⇒ ≈ 100 kHz · kick v_high 3 → 0 ⇒ the ring still starts (the gates' initial state is not the metastable 2.5 V) — note whether it does.
- **M:** gates are behavioural (no terminal currents): the current view shows nothing in/out of them and `--flow-test` checks only the RC nodes; an ammeter on a gate pin reads 0. No ideal/real toggle.
- **T:** **dt ≤ 50 ns** (20 ns in `--osc-test`); at 1 µs the RC ramps are 1-step staircases and f reads 20 % high. 2 µs/div shows ~2 cycles; 20 µs/div for the start-up.

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
| 29 | Window Comparator | | | | | | | OPEN (visual): input/pull-up wires drawn through Vref, R1, gnd3 bodies — needs re-route (feed input from a bus at x+200, rail at y-200) |
| 30 | Schmitt Trigger | | | | | | | |
| 31 | Precision Rectifier | | | | | | | |
| 32 | Peak Detector | | | | | | | OPEN (engine): with the ideal op-amp pinned at +15 V into D+10 µF, Newton does not fully converge each step (diode current at the solved voltages ≠ linearised value). Candidate for pnjlim junction limiting / relative convergence criteria (see docs/RESEARCH_SIMULATORS.md) |
| 33 | Sallen-Key LP | | | | | | | |
| 34 | Active Band-Pass | | | | | | | |
| 35 | Twin-T Notch | | | | | | | |
| 36 | Wien Bridge Osc | | | | | | | |
| 37 | RC Phase-Shift Osc | | | | | | | |
| 38 | Common Emitter | | | | | | | |
| 39 | Common Source | | | | | | | gate divider added |
| 40 | Common Drain | | | | | | | gate divider added |
| 41 | Multistage Amp | | | | | | | |
| 42 | Differential Pair | | | | | | | direct-coupled, 6 V bias. OPEN (visual): Rc2/emitter wires overlap Q2 terminals, emitter bus crosses RE body — needs re-layout (Q2 rot 0 at x+180, RE lower) |
| 43 | Current Mirror | | | | | | | |
| 44 | Push-Pull | | | | | | | |
| 45 | CMOS Inverter | | | | | | | |
| 46 | Constant Current Src | | | | | | | |
| 47 | Clamper | | | | | | | |
| 48 | RC Band-Pass | | | | | | | |
| 49 | LC Low-Pass | | | | | | | |
| 50 | Zener Clipper | | | | | | | |
| 51 | Voltage Doubler | | | | | | | |
| 52 | Relaxation Osc | | | | | | | |
| 53 | HW Rect + Cap | | | | | | | |
| 54 | 345 kV Line | | | | | | | |
| 55 | 138 kV Line + VAR | | | | | | | toggle cap bank live |
| 56 | 12.47 kV Feeder | | | | | | | |
| 57 | Pole Xfmr 120/240 | | | | | | | |
| 58 | Generator + GSU | | | | | | | |
| 59 | Grid: 18 kV to 240 V | | | | | | | |
| 60 | Ferranti (open line) | | | | | | | toggle reactor live |
| 61 | Tesla Coil | | | | | | | `--tesla-test` |
| 62 | Tesla Coil (big top) | | | | | | | `--tesla-test` |
| 63 | Tesla Coil (detuned) | | | | | | | `--tesla-test` (peak < 75 % of #62) |
| 64 | Line Model Ladder | | | | | | | 3 probe oracles |
| 65 | Line Drop Basics | | | | | | | |
| 66 | CT + 50/51 Overcurrent | | | | | | | 2 probe oracles; R_f 5 Ω vs the 1200 A note (see block) |
| 67 | 87 Line Differential | | | | | | | internal vs through fault, R_d oracle |
| 68 | 21 Distance Zone 1 | | | | | | | 2 probe oracles; move the fault via segment lengths |
| 69 | 50BF Breaker Failure | | | | | | | pulse-only (forced dt); 83 ms = healthy breaker |
| 70 | SIL Loading | | | | | | | toggle 2 × SIL live |
| 71 | Series Compensation | | | | | | | toggle bypass live |
| 72 | 765 kV Line (AEP) | | | | | | | 200 kV/div |
| 73 | 3-Phase Y Balanced | | | | | | | Stack view; neutral ≈ 0; try R_n 1 mΩ / 1 MΩ (no change) |
| 74 | 3-Phase Unbalanced | | | | | | | neutral 20.83 Vpk oracle; R_n 1 mΩ (0 V) / 1 MΩ (~144 V, 50 V/div) |
| 75 | 3-Phase 345 kV Line | | | | | | | 264 kVpk per phase; unbalance one load for zero-sequence |
| 76 | 3-Phase 6-Pulse Rect | | | | | | | plus-bus max 169.3 V; 360 Hz ripple; add 100 µF |
| 77 | Bistable (Schmitt) | | | | | | | X-Y hysteresis loop; R1/R2 set ±7.5 V; `ideal=true` kills it |
| 78 | Triangle/Square Gen | | | | | | | `--osc-test` 5000 Hz @ dt 200 ns; R/C = f, R1/R2 = amplitude |
| 79 | Function Generator | | | | | | | FFT: 3rd harmonic > 30 dB down; re-scale bias V after R2 change |
| 80 | Colpitts (MOSFET) | | | | | | | `--osc-test` 712 kHz @ dt 5 ns (measured 710); C1 → 2 nF = 616 kHz (note says 581 — see block) |
| 81 | Ring Oscillator | | | | | | | `--osc-test` 145 kHz @ dt 20 ns (measured 139); per-stage C retune; gates have no currents |

(81 blocks = the 81 `CIRCUIT_*` entries in `include/circuits.h` excluding `CIRCUIT_NONE`/`_COUNT`; #48-#65 follow the enum order after `CIRCUIT_PHASE_SHIFT_OSC`, #66-#72 the enum order after `CIRCUIT_DC_LINE_DROP`, #73-#81 the enum order after `CIRCUIT_HV_765_LINE`.)
