# Oscillators and Waveform Generators (Sedra & Smith) - implementation research

Scope: every signal-generator circuit in Sedra & Smith, *Microelectronic Circuits*, 7th ed., Chapter 18
"Signal Generators and Waveform-Shaping Circuits" (6th ed. = Ch. 17, 5th ed. = Ch. 13; same section order).
Section map (7th ed.): 18.1 Basic principles of sinusoidal oscillators (18.1.2 oscillation criterion,
18.1.4 nonlinear amplitude control, 18.1.5 limiter), 18.2 Op-amp-RC oscillators (Wien, phase-shift,
quadrature, active-filter-tuned), 18.3 LC and crystal oscillators (Colpitts, Hartley, crystal), 18.4 Bistable
multivibrators, 18.5 Astable / triangle generation, 18.6 Monostable, 18.7 IC timers (555), 18.8 Nonlinear
waveform shaping (triangle-to-sine), 18.9 Precision rectifiers.

Simulator assumptions used below: +/-15 V op-amp rails, real op-amp saturates at about +/-14 V (L+ = 14,
L- = -14); an *ideal* op-amp saturates at the rail itself (+/-15 V) - oracles quote the 14 V case and note the
15 V case. Silicon diode drop V_D = 0.6-0.7 V. Existing templates: Wien, PhOsc, RelOsc (src/circuits.c).

Notation: beta = R1/(R1+R2) is the positive-feedback fraction; T = period; f = 1/T.

---

## 1. Bistable multivibrator / Schmitt trigger (S&S 18.4)

**Inverting** (Fig. 18.19): op-amp, R1 from OUT to +IN, R2 from +IN to ground, input v_I to -IN.
Values: R1 = 10k, R2 = 10k (beta = 0.5). Drive: 100 Hz triangle or sine, 10 V peak.
- Thresholds: V_TH = beta*L+ = +7 V, V_TL = beta*L- = -7 V (ideal op-amp: +/-7.5 V). Hysteresis = 14 V.
- Transfer characteristic: v_O = L+ while v_I < V_TH (falls to L- when v_I rises through V_TH); v_O = L- until
  v_I falls through V_TL. Inverting: rising input gives falling output.
- **Non-inverting** (Fig. 18.21): v_I enters through R1 to +IN, R2 from OUT to +IN, -IN grounded.
  V_TH = -L-*(R1/R2), V_TL = -L+*(R1/R2). With R1 = 10k, R2 = 20k: +/-7 V. Rising input -> rising output.
- Start-up: none needed; state is set by the first input excursion beyond a threshold.
- Probe: OUT (square, +/-14 V) against v_I; X-Y plot of v_O vs v_I shows the hysteresis loop.
- Application: comparator with hysteresis - add 0.5 V of noise to a slow sine and show one clean edge per crossing.
- Oracle: output flips at |v_I| = 7.0 V +/-0.3 V (7.5 for ideal op-amp); output levels +/-14 V +/-1 V;
  exactly 2 transitions per input period.

## 2. Astable multivibrator - square-wave generator (S&S 18.5.1, Fig. 18.24)

Inverting bistable of #1 plus R from OUT to -IN and C from -IN to ground.
Values: R1 = R2 = 10k (beta = 0.5), R = 10k, C = 100 nF.
- T = 2*R*C*ln((1+beta)/(1-beta)) = 2*1e-3*ln3 = 2.197 ms -> f = 455 Hz. Independent of L+/L-.
- Capacitor swings exponentially between beta*L- and beta*L+ (+/-7 V); output +/-14 V, 50 % duty.
- Start-up: automatic (C starts at 0 V, which is inside the hysteresis band; any output state charges C).
  An ideal op-amp with a virtual-short model must be given an initial output state or it sits at 0 V - see pitfalls.
- Probe: OUT (square) and the C node (rounded triangle). Half period = R*C*ln3 = 1.099 ms.
- Oracle: f = 455 Hz +/-3 %; V(C) peaks +/-7.0 V +/-0.4 V; V(OUT) +/-14 V.
  This is the existing RelOsc template; keep it and add duty-cycle variant (diode + second R in parallel).

## 3. Monostable (one-shot) with diode clamp (S&S 18.6, Fig. 18.28)

Astable of #2 with D1 clamping the C1 node: R3 = 10k from OUT to -IN, C1 = 100 nF to ground, D1 anode at
-IN, cathode at ground (holds v_(-IN) at +V_D in the stable state, output at L+ ... note S&S uses the
inverted polarity; either works, thresholds mirror). Trigger: C2 = 10 nF + R4 = 10k differentiator into +IN,
with D2 steering only the negative edge. beta = 0.5.
- Stable state: OUT = L+ = 14 V, v_(-IN) = V_D = 0.7 V, +IN = beta*L+ = 7 V.
- Trigger pulse (5 V, 10 us negative edge) pulls +IN below 0.7 V -> OUT flips to L-; C1 charges from V_D
  toward L- through R3; when it reaches beta*L- = -7 V the circuit flips back.
- Pulse width T = C1*R3*ln((V_D - L-)/(beta*L- - L-)) = 1e-3*ln(14.7/7) = 0.742 ms.
  Approximation for V_D << |L-|: T ~ C1*R3*ln(1/(1-beta)) = 0.693 ms.
- Recovery: C1 recharges to +V_D with time constant R3*C1; re-trigger no sooner than ~3*R3*C1 = 3 ms.
- Probe: trigger input, C1 node, OUT. Oracle: one 14 V negative pulse of width 0.74 ms +/-5 % per trigger.

## 4. Triangle / square generator (S&S 18.5.2, Fig. 18.26)

Non-inverting bistable (#1, R1 = 10k, R2 = 20k, thresholds +/-7 V) driving an inverting integrator
(R = 10k from bistable OUT to -IN of op-amp 2, C = 10 nF feedback), integrator output back into R1.
- Triangle amplitude = bistable thresholds: peaks V_TH = -L-*R1/R2 = +7 V, V_TL = -7 V (ideal: +/-7.5).
- Ramp time T1 = C*R*(V_TH - V_TL)/L+ ; T = T1 + T2 = 4*C*R*(R1/R2) (L+ = -L- cancels).
  f = R2/(4*R*C*R1) = (1/(4*R*C))*(R2/R1) = 2500 * 2 = 5.00 kHz. Slope = L+/(RC) = 14 V / 0.1 ms.
- Asymmetric supplies (L+ != -L-) give unequal ramp times -> sawtooth-ish; S&S uses this for sawtooth.
- Start-up: bistable output is never 0 in the real-op-amp model; with an ideal op-amp set an initial
  condition (kick) on the bistable output or on C.
- Probe: integrator OUT (triangle +/-7 V) and bistable OUT (square +/-14 V, edges at triangle peaks).
- Oracle: f = 5.0 kHz +/-3 %; triangle peak 7.0 V +/-0.4 V; peak-to-peak linearity: |V(t)-ramp| < 0.2 V.

## 5. Function generator: triangle -> sine shaper (S&S 18.8.2, Fig. 18.36 "breakpoint" method)

Feed the +/-7 V triangle of #4 through R_in = 10k into a diode/resistor "soft limiter" to ground.
Three-breakpoint network per polarity (mirror for the negative half with reversed diodes and -V bias):
| branch | series R | bias source | knee (output) | slope after knee |
|---|---|---|---|---|
| none (0 .. 2.6 V) | - | - | - | 1.00 (y = x) |
| D1 + Ra | Ra = 22k | +2.0 V | 2.6 V | Ra/(R_in+Ra) = 0.69 |
| D2 + Rb | Rb = 5.6k | +3.7 V | 4.28 V | (Ra||Rb)/(R_in+Ra||Rb) = 0.31 |
Bias sources: DC sources +/-2.0 V and +/-3.7 V (or dividers from +/-15 V: 13k/2k -> 2.0 V, 11k/3.6k -> 3.7 V;
add the divider Thevenin R to Ra/Rb). Output peak = 4.28 + 0.31*(7 - 4.9) ~ 4.9 V vs. an ideal sine of 5.0 V
peak; the diode exponential softens the knees. Expected THD 3-5 % (S&S quotes < 2 % with 4 breakpoints).
- Amplitude control: the sine peak scales with the triangle peak, i.e. with the bistable thresholds R1/R2
  and with the op-amp saturation. To decouple from the supply, clamp the bistable output with back-to-back
  zeners (R = 1k series, 2 x 5.1 V zener -> +/-5.8 V square) so the triangle is +/-5.8*R1/R2; then a pot
  (or a second R2 value) scales amplitude. The shaper bias voltages must scale with the same ratio, so a
  simpler product-level control is a potentiometer *after* the shaper (attenuator into a unity buffer).
- Frequency control: f = R2/(4*R*C*R1). Decade switch C (1 nF / 10 nF / 100 nF -> 50 k / 5 k / 500 Hz),
  vernier with R (pot 1k-100k). VCO form: replace R by a control voltage - feed the integrator not from
  the bistable but from +/-V_C selected by the bistable through an analog switch (or a MOSFET pair);
  then f = V_C*R2/(4*R*C*R1*L+)... i.e. slope = V_C/(RC), f = V_C/(4*R*C*V_TH), linear in V_C.
- Probe: shaper OUT vs triangle; FFT of shaper OUT (fundamental at f, 3rd harmonic > 30 dB down).
- Oracle: fundamental amplitude 4.9 V +/-10 %, f = 5.0 kHz, THD < 8 %; 3rd-harmonic / fundamental < 0.06.
- Alternative shapers: (a) differential BJT pair with emitter degeneration (tanh shaping, S&S 18.8.3) -
  triangle of ~ +/-2*V_T*... peak ~ 0.5 V into a pair with R_E = 0 gives THD ~ 1 % at optimum drive;
  (b) two zeners in the integrator feedback for a hard clip (poor THD, easy demo).

## 6. Sinusoidal op-amp-RC oscillators (S&S 18.1-18.2)

Barkhausen (18.1.2): loop gain L(jw0) = 1 with zero phase; start with |L| slightly > 1 and let a nonlinearity
reduce it to exactly 1 (18.1.4). All need a kick or noise to start in a simulator.

**Wien bridge** (18.2.1, Fig. 18.4/18.5) - series R-C from OUT to +IN, parallel R||C from +IN to ground;
R1 = 10k from -IN to ground, feedback R2. R = 10k, C = 10 nF -> f = 1/(2*pi*R*C) = 1.59 kHz; needs gain
1 + R2/R1 = 3 (network gain 1/3 at f0).
- Diode limiter: R2 = 18k in series with (5.6k || anti-parallel diodes D1/D2). Small-signal gain 1+23.6/10 =
  3.36 (starts); with diodes on gain -> 2.8 (< 3). Amplitude settles where the average gain is 3:
  voltage across the 5.6k = 0.19*V_o, diodes conduct near V_o ~ 3.2 V -> steady peak 3.5-4.5 V, THD ~ 1-2 %.
- Lamp / JFET idea: R1 = tungsten lamp whose R rises with current (no lamp model; approximate with a
  MOSFET in triode as voltage-controlled R driven by a peak-detected, low-pass-filtered output: gate =
  -(rectified V_o), drain-source in place of part of R1). Good stretch goal; document, do not template first.
- Probe OUT. Oracle: f = 1.59 kHz +/-2 %, peak 4 V +/-1 V after 30 ms; without limiter clips at +/-14 V.

**Phase-shift** (18.2.2, Fig. 18.7) - three cascaded RC high-pass sections from OUT back to -IN, Rf feedback.
Op-amp: R = 10k, C = 1 nF -> f = 1/(2*pi*sqrt(6)*R*C) = 6.50 kHz, |gain| >= 29 -> Rf = 330k (33x). Existing
PhOsc template; add anti-parallel diodes across Rf (series 100k) for amplitude ~ +/-5 V, THD < 5 %.
Single-BJT (CE, Fig. 18.7 variant): Vcc = 12 V, Rc = 4.7k, R1 = 47k, R2 = 10k, Re = 1k with Ce = 100 uF,
three sections R = 10k, C = 10 nF (last R is the base bias network's equivalent).
f = 1/(2*pi*R*C*sqrt(6 + 4*Rc/R)) = 1591/sqrt(7.88) = 567 Hz; h_FE > 23 + 29*R/Rc + 4*Rc/R = 87 -> use
beta = 150-200. Collector swing several V peak, clipped on the cutoff side. Oracle: 567 Hz +/-8 % (loading
by the base shifts it), amplitude 2-5 V peak.

**Quadrature** (18.2.3, Fig. 18.10) - non-inverting (Miller) integrator with Rf/R = 2 followed by an inverting
integrator; feedback to the first. R = 10k, C = 10 nF: f0 = 1/(2*pi*R*C) = 1.59 kHz. Make the pole right-half
plane for start-up: Rf = 18k instead of 20k (or 2R slightly > ... S&S: make R_f slightly less than 2R).
Limiter: back-to-back 5.1 V zeners across the first integrator's C (series 1k) -> v_o1 ~ +/-5.8 V clipped;
v_o2 is a cleaner sine of the same amplitude, 90 deg lagging. Probe both; X-Y = circle. Oracle: f = 1.59 kHz
+/-3 %, |v_o2| = 5.8 V +/-15 %, phase(v_o2 - v_o1) = 90 +/-5 deg.

**Active-filter-tuned** (18.2.4, Fig. 18.11/18.12) - band-pass filter (Tow-Thomas or MFB, f0 = 1/(2*pi*R*C)
with R = 10k, C = 10 nF, Q = 5-10, centre gain 1) whose output feeds a hard limiter (1k + back-to-back 5.1 V
zeners -> +/-5.8 V square), square fed back to the filter input. Filter extracts the fundamental:
amplitude = (4/pi)*5.8*G0 = 7.4 V peak for G0 = 1. THD ~ 1/(3Q) for the 3rd harmonic (~3 % at Q = 10).
Oracle: f = 1.59 kHz +/-2 %, sine peak 7.4 V +/-10 %, square +/-5.8 V.

## 7. LC and crystal oscillators (S&S 18.3)

**Colpitts** (18.3.1, Fig. 18.13/18.14) - CE BJT: Vcc = 12 V, R1 = 47k, R2 = 10k, Re = 1k (bypass 1 uF),
collector to RFC 1 mH, tank L = 100 uH from collector to a 10 nF coupling / C1 = 1 nF collector-ground,
C2 = 1 nF base-side-ground (C1 top at collector, C2 at the base via 10 nF coupling; junction grounded).
f = 1/(2*pi*sqrt(L*C1*C2/(C1+C2))) = 1/(2*pi*sqrt(100u*0.5n)) = 712 kHz. Start: g_m*R_tank >= C2/C1 = 1
(easy; with I_C = 1 mA, g_m = 40 mA/V). Amplitude: collector swings ~ +/-(Vcc - 2) V around Vcc with an
RFC (can exceed Vcc); base-emitter self-limits by cutoff (class C). MOSFET version: same tank, common-source,
R_G = 1M bias, RFC drain; condition g_m*R_P >= C2/C1 (ratio C2/C1 up to ~ 4 still starts).
Oracle: 712 kHz +/-5 % (BJT C_pi and Miller pull it down), collector peak 5-10 V, dt <= 20 ns.

**Hartley** (18.3.1, Fig. 18.15) - swap: L1 = L2 = 50 uH (no mutual coupling, tap grounded), C = 1 nF.
f = 1/(2*pi*sqrt((L1+L2)*C)) = 503 kHz; start condition g_m*R >= L1/L2 = 1. Same bias network.
Oracle: 503 kHz +/-5 %.

**Clapp** - Colpitts with C3 = 100 pF in series with L = 100 uH; C1 = C2 = 1 nF swamp the transistor
capacitances. 1/C_s = 1/C1 + 1/C2 + 1/C3 -> C_s = 83.3 pF; f = 1/(2*pi*sqrt(L*C_s)) = 1.744 MHz.
Oracle: 1.744 MHz +/-2 % (better than Colpitts because C3 dominates). dt <= 5 ns.

**Crystal / Pierce** (18.3.2, Fig. 18.17-18.19) - crystal = Ls + Cs + Rs series, Cp in parallel.
4 MHz model: Ls = 100 mH, Cs = 15.83 fF, Rs = 50 ohm, Cp = 5 pF.
f_s = 1/(2*pi*sqrt(Ls*Cs)) = 4.000 MHz; f_p = f_s*sqrt(1 + Cs/Cp) = 4.0063 MHz; Q = w*Ls/Rs = 50 000.
(Typical datasheet values are Rs 25-100 ohm, Cp 3-7 pF, Cs 10-30 fF; see sources.) Crystal is inductive
only between f_s and f_p, so it replaces L in a Colpitts (Pierce): CMOS inverter (or a single MOSFET +
drain R 2.2k), R_f = 1M from out to in (linear bias), crystal from out to in, C1 = C2 = 22 pF to ground,
optional 1k series R at the output for drive limiting. Load capacitance C_L = C1*C2/(C1+C2) = 11 pF ->
f = f_s*(1 + Cs/(2*(Cp+C_L))) = 4.00198 MHz. Output: rail-to-rail square at the inverter, sine-ish (~1 V)
at the crystal input side. Oracle: 4.002 MHz +/-0.05 % (i.e. must sit between f_s and f_p).
Simulation warning: with Q = 50 000 the envelope ring-up time ~ Q/(pi*f) = 4 ms = 800 000 steps at dt = 5 ns.
For the template use a "teaching" crystal with Rs = 2 kohm (Q ~ 1250, start in 0.1 ms) and explain the
scaling, or precharge Cs with an initial voltage.

## 8. Digital / timer relaxation oscillators (S&S 18.7 for 555; ring and Schmitt-RC are standard extras)

**Ring** - odd N CMOS inverters in a loop; f = 1/(2*N*t_pd). With N = 5 and gate delay 10 ns -> 10 MHz.
If the logic gates are zero-delay, insert R = 1k, C = 1 nF between stages: each stage delays ~ 0.69*R*C
(charging to the 50 % threshold) -> f ~ 1/(2*5*0.69 us) = 145 kHz (+/-15 %: threshold and edge effects).
Start: an all-zero-delay loop is an algebraic loop; the per-stage RC (or one initial condition) fixes it.
Probe every node: five phase-shifted squares, each 1/(2N) period apart.

**555 astable** (18.7.2, Fig. 18.31) - R_A = 1k, R_B = 10k, C = 10 nF, Vcc = 5 V.
T_H = 0.693*(R_A+R_B)*C = 76.2 us, T_L = 0.693*R_B*C = 69.3 us, T = 0.693*(R_A+2R_B)*C = 145.5 us ->
f = 6.87 kHz, duty = (R_A+R_B)/(R_A+2R_B) = 52.4 %. C swings 1/3 Vcc to 2/3 Vcc (1.67 - 3.33 V).
Oracle: 6.87 kHz +/-3 %, duty 52 +/-2 %, V(C) 1.67/3.33 V +/-0.1 V. Self-starting.

**555 monostable** (18.7.1, Fig. 18.30) - R = 100k, C = 10 nF; T = 1.1*R*C = 1.10 ms (C charges from 0 to
2/3 Vcc: ln3 = 1.0986). Trigger: 10 us low pulse on pin 2 (below 1/3 Vcc). Oracle: one 1.10 ms +/-2 % high pulse.

**Schmitt-inverter RC** (74HC14 style: R from out to in, C from in to ground) - thresholds V_T+ = 2.9 V,
V_T- = 1.9 V at Vdd = 5 V (HC14 typical). T = R*C*ln[((Vdd - V_T-)*V_T+)/((Vdd - V_T+)*V_T-)] =
R*C*ln(8.99/3.99) = 0.81*R*C; R = 10k, C = 10 nF -> 81 us, f = 12.3 kHz, duty ~ 50 %. If the sim's Schmitt
gate has different thresholds, re-derive with the same formula. Oracle: 12.3 kHz +/-5 %.

**Relaxation (op-amp Schmitt + RC)** - identical to #2; already RelOsc (455 Hz).

---

## Simulation pitfalls

1. Ideal op-amp with a virtual-short (infinite-gain, forced v+ = v-) model cannot run open loop or with net
   positive feedback: the solver either fails or parks at 0 V. Bistable/astable/monostable/triangle circuits
   need the finite-gain saturating model (A ~ 1e5, clamp at the rails); ideal op-amps only in the integrators.
2. Every sinusoidal oscillator sits at an unstable equilibrium: give a kick (pulse source through 100 nF
   into the loop, an initial capacitor voltage, or the existing "noise on" option). Startup time ~ several
   periods / (|L| - 1); with a Wien gain of 3.36 expect ~ 20 cycles to reach the limiter.
3. Time step: >= 100 points per period for frequency accuracy to 1 % (relaxation edges also need
   dt << R*C/50). LC: dt <= T/200 (Colpitts 712 kHz -> 7 ns); crystal 4 MHz -> 2-5 ns and long runs.
   Suggest each template set dt and run length with its "PROBE" note.
4. Amplitude limiting: without diodes/zeners any working oscillator ends at the rails (clipped, high THD);
   diode limiters make the amplitude depend on V_D (temperature) - fine for teaching. Zeners need the
   sharp-knee model, else the limit level is soft and the oracle band must widen.
5. Frequency oracles must tolerate op-amp finite GBW (phase-shift and quadrature drift low by ~ f/f_t*pi),
   BJT capacitances (Colpitts), and finite slew rate (relaxation edges). Use +/-3 % for RC, +/-5 % for LC.
6. Node-merge trap: template builders merge nodes closer than 5 px (see conventions memory) - LC tanks and the
   shaper's paralleled diode branches are dense; space them.
7. Very high Q (crystal) = huge ring-up time; use the reduced-Q teaching model or an initial condition.
8. Algebraic loops in zero-delay gate rings; give gates a delay or add per-stage RC.

## Recommended palette (TG_OSCILLATORS), 14 templates

| short | circuit | ref | oracle |
|---|---|---|---|
| Schmit | inverting op-amp bistable driven by 10 V triangle | 18.4 | flips at +/-7 V |
| RelOsc | astable square (exists) | 18.5.1 | 455 Hz |
| OneSht | op-amp monostable, diode clamp | 18.6 | 0.74 ms pulse |
| TriSq | bistable + integrator triangle/square | 18.5.2 | 5 kHz, +/-7 V |
| FuncGn | TriSq + 3-breakpoint diode shaper + amplitude pot | 18.8.2 | 5 kHz sine 4.9 V, THD < 8 % |
| Wien | Wien bridge (exists; add diode limiter) | 18.2.1 | 1.59 kHz, ~4 V |
| PhOsc | op-amp phase-shift (exists) | 18.2.2 | 6.5 kHz |
| BJTPhs | single-BJT phase-shift | 18.2.2 | 567 Hz |
| Quad | two-integrator quadrature | 18.2.3 | 1.59 kHz, 90 deg |
| BPFOsc | band-pass filter + zener limiter | 18.2.4 | 1.59 kHz, 7.4 V |
| Colpit | BJT Colpitts (MOSFET as option) | 18.3.1 | 712 kHz |
| Hartly | BJT Hartley | 18.3.1 | 503 kHz |
| Pierce | CMOS-inverter crystal oscillator | 18.3.2 | 4.002 MHz |
| 555Ast | 555 astable | 18.7.2 | 6.87 kHz, 52 % |
Optional extras: 555Mon (1.10 ms), Ring (5 inverters + RC, 145 kHz), SchmRC (HC14, 12.3 kHz), Clapp (1.744 MHz).
Group order in the palette: Schmit, RelOsc, OneSht, TriSq, FuncGn | Wien, PhOsc, BJTPhs, Quad, BPFOsc |
Colpit, Hartly, Pierce | 555Ast.

## Sources

- A. S. Sedra, K. C. Smith, *Microelectronic Circuits*, 7th ed., Oxford UP, 2015, Ch. 18 (section numbers above).
  Chapter outline confirmed via https://www.studocu.com/in/document/delhi-technological-university/analog-electronics/sedra-and-smith-11-complete-notes-on-signal-generators/47466530
- Crystal equivalent-circuit values: CTS "Crystal Basics" app note, https://www.mouser.com/pdfDocs/ctsappnote-crystal-basics.pdf ;
  Abracon crystal glossary, https://abracon.com/Support/qtzcry_glossary.pdf ; Maxim/ADI AN726 "Specifying Quartz Crystals",
  https://www.maximintegrated.com/en/app-notes/index.mvp/id/726 ; UIUC ECE453 Lab 4 crystal model,
  https://courses.grainger.illinois.edu/ece453/sp2018/lab_files/Lab4_QuartzCrystalOscillator.pdf ; PA3FWM tech note,
  https://www.pa3fwm.nl/technotes/tn13a.html
- 555 formulas: standard NE555 datasheet (T = 0.693(RA+2RB)C astable, 1.1RC monostable).
- 74HC14 thresholds: typical HC14 datasheet values at Vcc = 5 V (V_T+ ~ 2.9 V, V_T- ~ 1.9 V).
