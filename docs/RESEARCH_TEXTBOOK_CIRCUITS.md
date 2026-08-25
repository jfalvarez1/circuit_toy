# Research: Classic Textbook Teaching Circuits for the Template Palette

Date: 2026-08-24. Status: research only, no code changes.

Sources
- [SS] A. S. Sedra, K. C. Smith, *Microelectronic Circuits*, 7th ed. (Oxford, 2015). Section numbers below follow the 7th ed. table of contents; 6th/8th ed. differ by +/-1 in chapter numbers (marked ~ where I am unsure of the sub-section).
- [AL] A. Agarwal, J. H. Lang, *Foundations of Analog and Digital Electronic Circuits* (Morgan Kaufmann, 2005; MIT 6.002). Chapter map: 2 resistive nets, 3 network theorems, 4 nonlinear, 5 digital abstraction, 6 MOSFET switch, 7 MOSFET amplifier, 8 small-signal, 10 first-order transients, 11 energy/power in digital, 12 second-order, 13 sinusoidal SS, 14 filters/resonance, 15 op amp, 16 diodes.
- Standard device numbers used for oracles: VT = 25 mV, VBE(on) = 0.7 V, beta = 100, NMOS/PMOS Vt = 1 V with K = 1 mA/V^2 (the [AL] textbook device), Ron = 100 ohm for the [AL] switch model, 741-class op amp SR = 0.5 V/us, GBW = 1 MHz.

Existing coverage that was deliberately NOT repeated (72 templates in `src/circuits.c`): RC/RL/RLC filters and resonance, divider, LED+R, rectifiers, clamps/doublers, BJT CE / CS / diff pair / mirror / current source / push-pull, CMOS inverter (time domain), op-amp inverting/non-inverting/follower/integrator/differentiator/summing/difference/instrumentation/TIA/comparator/Schmitt/precision rectifier/peak detector, oscillators, regulators, power systems. Everything below is either absent or exercises a distinct textbook concept (e.g. VTC sweep instead of time-domain inverter).

Column key: **Stim** = source to attach; **Oracle** = hand-computed number the smoke test should check, with tolerance; **Name** = palette label (<= 6 chars). All oracles assume the device defaults above; if the simulator's BJT/MOS model parameters differ, the oracle formula (given) must be re-evaluated with the sim's values.

Simulator prerequisites to verify before building: (P1) COMP_OPAMP_REAL exposes gain, GBW, slew rate and Ro; (P2) NMOS/PMOS expose K (or kn', W/L) and Vt; (P3) BJT exposes beta; (P4) scope has an XY mode or a sweep/"transfer" plot for VTC items; (P5) scope can average supply current for the CV^2f item.

---

## Group 1: Basics (resistive networks, theorems, diode models)

### 1. Thevenin / Norton equivalent and maximum power transfer  `Thev`
- Ref: [AL] 3.6.1-3.6.3; [SS] Appendix D.
- Circuit: Vs = 10 V DC, divider R1 = 2 k (top) / R2 = 3 k (bottom), series R3 = 1 k from the divider tap to the load, RL = 2.2 k (potentiometer 0-10 k to sweep).
- Demonstrates: two-terminal network replaced by Vth = 6 V, Rth = (2k||3k) + 1k = 2.2 k; Norton In = Vth/Rth = 2.727 mA; max power when RL = Rth.
- Equations: Vth = Vs*R2/(R1+R2); Rth = R1||R2 + R3; VL = Vth*RL/(RL+Rth); PL = VL^2/RL.
- Stim: DC only; drag the pot.
- Oracle: VL = 3.00 V (+/-1 %) at RL = 2.2 k; open-circuit tap-load voltage 6.00 V; short-circuit load current 2.727 mA; PL,max = 4.09 mW. Sanity: VL at RL = 1 k is 1.875 V.

### 2. Superposition with three sources  `Super`
- Ref: [AL] 3.5.1; [SS] App. D.
- Circuit: V1 = 12 V DC - R1 = 4 k - node N - R2 = 4 k - V2 = 6 V DC; R3 = 4 k from N to ground; DC current source I1 = 1 mA injected into N.
- Demonstrates: V(N) = sum of the three single-source responses (4 V + 2 V + 1.333 V).
- Equation: V(N) = (V1/R1 + V2/R2 + I1) / (1/R1 + 1/R2 + 1/R3).
- Stim: DC. Optional: make I1 an AC 1 mA / 1 kHz source so the ac ripple (1.333 Vpk) rides on the 6 V DC from the voltage sources.
- Oracle: V(N) = 7.333 V (+/-0.5 %). With I1 = 0: 6.000 V. With only I1: 1.333 V.

### 3. Diode piecewise-linear (CVD) model vs. exponential  `DPWL`
- Ref: [AL] 16.2 (piecewise-linear analysis), [SS] 4.3.3-4.3.5 (constant-voltage-drop, iterative exponential).
- Circuit: 5 V DC, R = 1 k, silicon diode to ground; second branch 5 V, 100 ohm, diode (shows how VD moves with current).
- Demonstrates: VD ~ 0.7 V at ~4 mA, ~0.76 V at ~42 mA (60 mV/decade); the CVD model error is < 2 % on the current.
- Equations: ID = (5 - VD)/R; VD2 - VD1 = n*VT*ln(ID2/ID1).
- Stim: DC.
- Oracle: I(1 k) = 4.3 mA (+/-3 %); I(100 ohm) = 42.4 mA (+/-5 %); VD difference between branches = 60 mV per decade of current, i.e. about 58 mV (n = 1) (+/-20 mV).

### 4. Diode small-signal (incremental) resistance  `Dsmal`
- Ref: [AL] 16.3 (incremental analysis), [SS] 4.3.6 (small-signal model, rd = VT/ID).
- Circuit: 5 V DC in series with 1 Vpk / 1 kHz AC, R = 1 k, diode to ground. Scope on the diode (AC-coupled, mV/div) and on the resistor current.
- Demonstrates: the diode acts as rd = n*VT/ID = 25 mV / 4.3 mA = 5.8 ohm for the ac part while the resistor current swings +/-1 mA.
- Equations: rd = n*VT/ID; vd = vs*rd/(R + rd).
- Stim: 1 Vpk at 1 kHz on top of 5 V DC (10 ms window = 10 cycles).
- Oracle: vd,pk = 5.8 mV (+/-30 %, depends on n and on the sim's Is); current ripple = 0.99 mApk (+/-2 %); diode DC ~0.70 V.

### 5. Wheatstone bridge / null and small unbalance  `Bridg`
- Ref: [AL] 2.x worked examples and Problem 3.x; [SS] Problem 1.x (bridge). Classic sensor front-end.
- Circuit: 10 V DC; R1 = R2 = R3 = 1 k, R4 = 1.1 k (or a 1 k thermistor/LDR/pot in the R4 leg).
- Demonstrates: differential output is zero at balance and linear-ish in the unbalance; leads directly into the existing difference-amp template.
- Equation: Vout = Vs*(R4/(R3+R4) - R2/(R1+R2)).
- Stim: DC; sweep R4 with the pot or use the thermistor.
- Oracle: Vout = 0.238 V (+/-1 %) at R4 = 1.1 k; 0 V at 1.0 k; -0.263 V at 0.9 k.

---

## Group 2: Transients (first- and second-order, MOSFET switch)

### 6. RC step response, 63 % point  `RCstp`
- Ref: [AL] 10.1 (RC with step), [SS] 1.? App. E.
- Circuit: square 0-5 V, R = 10 k, C = 100 nF, scope on C. tau = 1 ms.
- Stim: square 0-5 V at 100 Hz (half-period 5 ms = 5 tau, fully settles; 10 ms window).
- Equation: vC = 5(1 - e^(-t/tau)); vC(tau) = 3.16 V; vC(2.3 tau) = 4.5 V (10-90 % rise = 2.2 tau = 2.2 ms).
- Oracle: vC = 3.16 V (+/-2 %) 1.0 ms after the rising edge; 10-90 % rise time 2.20 ms (+/-3 %). Distinct from the existing RC LP filter template because it is a step, not a sine, and the cursor lands on the 63 % mark.

### 7. RL step response, inductor current  `RLstp`
- Ref: [AL] 10.2 (RL with step), 9.4.
- Circuit: square 0-5 V, R = 100 ohm, L = 10 mH; scope on the resistor voltage (= 100 * iL). tau = L/R = 100 us.
- Stim: square 0-5 V at 1 kHz (half-period = 5 tau).
- Equation: iL = (V/R)(1 - e^(-t/tau)); vL = V e^(-t/tau).
- Oracle: iL(inf) = 50.0 mA (+/-1 %); iL(100 us) = 31.6 mA (+/-3 %); vL jumps to 5 V then decays to 0.

### 8. Series RLC step, underdamped ringing  `Ring`
- Ref: [AL] 12.2-12.3 (driven second-order, damping), [SS] Appendix E.
- Circuit: square 0-5 V, R = 20 ohm, L = 10 mH, C = 100 nF; scope on C.
- Equations: w0 = 1/sqrt(LC) = 31.6 krad/s (f0 = 5.03 kHz); alpha = R/2L = 1000 /s; zeta = alpha/w0 = 0.0316; Q = 15.8; overshoot = exp(-pi*zeta/sqrt(1-zeta^2)) = 90.5 %; envelope tau = 1/alpha = 1 ms.
- Stim: square 0-5 V at 200 Hz (each half-period 2.5 ms shows ~12 ring cycles with visible decay).
- Oracle: first peak 9.53 V (+/-3 %); ring period 199 us (+/-2 %); envelope decays to 1/e in 1.0 ms (+/-10 %).

### 9. Same RLC, critically damped and overdamped  `Damp`
- Ref: [AL] 12.2.2-12.2.4 (over/critical/under); [SS] App. E.
- Circuit: as #8 but R = 632 ohm (critical, R = 2 sqrt(L/C)) and R = 2 k (overdamped); a 3-position selection (20 / 632 / 2000 ohm) via pot or three parallel copies.
- Equations: critical: vC = 5[1 - (1 + w0 t) e^(-w0 t)]; overdamped roots s1,2 = -alpha +/- sqrt(alpha^2 - w0^2) = -5.13 k /s and -195 k /s (slow tau = 195 us).
- Stim: square 0-5 V at 200 Hz.
- Oracle: critical: no overshoot, vC(94.9 us = 3/w0) = 4.00 V (+/-3 %), 4.75 V at 5/w0 = 158 us; overdamped: no overshoot, 63 % point at ~230 us (+/-10 %, dominated by the slow root).

### 10. MOSFET switch (SR model) driving a load capacitor: propagation delay  `SRdly`
- Ref: [AL] 6.6-6.8 (S and SR models), 10.4 (propagation delay of gates, t_pd = 0.69 RC).
- Circuit: VDD = 5 V, pull-up RL = 10 k, NMOS (Ron ~ 100 ohm) to ground, CL = 10 nF on the drain; gate driven by a square wave.
- Demonstrates: asymmetric delays; rise through RL (tau = 100 us), fall through Ron||RL (tau ~ 1 us).
- Equations: t_pd,LH = 0.69 RL CL = 69 us; t_pd,HL = 0.69 (Ron||RL) CL ~ 0.69 us; VOL = VDD Ron/(RL+Ron) = 50 mV.
- Stim: square 0-5 V at 1 kHz (half-period 500 us = 5 tau).
- Oracle: 50 % rise delay 69 us (+/-5 %); fall delay < 2 us; VOL ~ 0.05 V (+/-50 %, depends on the sim's MOS triode model).

### 11. CMOS dynamic power CV^2f  `CVVf`
- Ref: [AL] 11.1-11.3 (energy in the RC switch, CMOS dissipation); [SS] 14.3.4 (dynamic power).
- Circuit: CMOS inverter, VDD = 5 V, CL = 10 nF, gate driven by square wave at 10 kHz; ammeter/scope on the VDD supply current.
- Equations: P = CL VDD^2 f = 10 nF * 25 * 10 kHz = 2.5 mW; average supply current = CL VDD f = 0.5 mA; energy per cycle = CL VDD^2 = 250 nJ (half in the PMOS on charge, half in the NMOS on discharge).
- Stim: square 0-5 V at 10 kHz (1 ms window = 10 cycles; make f selectable 1/10/100 kHz to show linear scaling).
- Oracle: mean supply current 0.50 mA (+/-10 %); P doubles when VDD -> 7.07 V, quadruples at 10 V.

### 12. Inductive kick and freewheel diode  `Flybk`
- Ref: [AL] 10.2 example (inductor with switch, "why the light bulb flashes"), [SS] 4.? (diode as flyback clamp); relay/solenoid drivers.
- Circuit: 12 V, L = 10 mH in series with R = 12 ohm (coil), low-side NMOS switch driven by pulse; freewheel diode across the coil (switchable).
- Equations: on: iL -> 1 A with tau = L/R = 0.83 ms; off with diode: current decays through diode+R with same tau, drain clamped to VDD + 0.7 = 12.7 V; without diode the drain voltage spikes (limited only by the sim's MOS breakdown / min conductance).
- Stim: pulse 0-5 V, 2 ms on / 2 ms off (250 Hz).
- Oracle: with diode, drain peak 12.7 V (+/-0.1 V); iL peak 0.91 A (1 - e^(-2.4)) (+/-3 %); without diode, drain peak >> 12 V (report the value; expect the GMIN-limited spike).

---

## Group 3: Amplifiers (configurations, biasing, output stages, frequency response)

### 13. Common-base amplifier  `CB`
- Ref: [SS] 7.3.5 (CB), 7.3.6 comparison table.
- Circuit: VCC = 12 V; base divider R1 = 22 k / R2 = 10 k (VB = 3.75 V), base bypassed with 10 uF to ground; RE = 3 k (IE = 1.0 mA), RC = 4.7 k; input coupled into the emitter via 10 uF; output at collector via 10 uF.
- Demonstrates: non-inverting, low Rin = re = 25 ohm, Av = gm RC.
- Equations: gm = IC/VT = 40 mA/V; Av = +gm RC = +188; Rin = re = 25 ohm (add Rs = 50 ohm to show the 1/3 drop).
- Stim: 10 mVpk at 10 kHz (1 ms window).
- Oracle: vout = 1.88 Vpk in phase (+/-20 %, beta/VT dependent); with Rs = 50 ohm, 0.63 Vpk (+/-25 %); VC(DC) = 7.3 V (+/-5 %).

### 14. Cascode (CE + CB)  `Casc`
- Ref: [SS] 8.5.? (cascode amplifier), 10.5.? (cascode high-frequency response).
- Circuit: VCC = 12 V; Q1 CE with divider bias for IC = 1 mA (R1 = 47 k / R2 = 10 k, RE = 1 k bypassed 100 uF); Q2 CB stacked on Q1's collector, base held at 6 V by divider (22 k/22 k) bypassed 10 uF; RC = 4.7 k; input via Rs = 10 k and 10 uF; explicit Ccb = 100 pF on Q1 (for comparison with #15).
- Demonstrates: same gain as CE but Q1's collector sees only re2 = 25 ohm, so the Miller multiplication of Ccb is ~x2 instead of x189 -> far wider bandwidth.
- Equations: Av ~ -gm RC = -188; fH,casc ~ 1/(2 pi (Rs||rpi) (Cpi + 2 Ccb)) ~ 1/(2 pi 2 k 200 p) = 400 kHz vs 4.2 kHz in #15.
- Stim: 10 mVpk at 10 kHz, then sweep 100 Hz - 1 MHz.
- Oracle: |Av| = 188 (+/-25 %); -3 dB corner > 100 kHz (vs #15's ~4 kHz).

### 15. Miller effect limiting CE bandwidth  `Millr`
- Ref: [SS] 10.3.3 (Miller's theorem), 10.4.? (CE high-frequency response).
- Circuit: CE amp, IC = 1 mA, RC = 4.7 k, RE bypassed, driven through Rs = 10 k; explicit external C = 100 pF from collector to base.
- Equations: Cin = C(1 + |Av|) = 100 p * 189 = 18.9 nF; rpi = beta/gm = 2.5 k; fH = 1/(2 pi (Rs||rpi) Cin) = 1/(2 pi * 2 k * 18.9 n) = 4.2 kHz.
- Stim: AC 10 mVpk; sweep 100 Hz - 1 MHz, or spot-check 500 Hz vs 4.2 kHz.
- Oracle: midband |Av| at 500 Hz = 188 * rpi/(Rs+rpi) = 37.6 (+/-25 %); gain at 4.2 kHz = 0.707 * midband (+/-30 % on the corner); gain at 42 kHz ~ 3.8.

### 16. Darlington emitter follower: input resistance beta^2 RE  `Darl`
- Ref: [SS] 7.3.7? / 12.? (Darlington configuration); [SS] 8.? "Darlington pair" (COMP_NPN_DARLINGTON exists).
- Circuit: VCC = 12 V; Darlington collector to VCC, emitter to RE = 100 ohm; input 6 V DC + 1 Vpk / 1 kHz through Rs = 100 k (source resistance deliberately huge); parallel copy with a single NPN for comparison.
- Equations: Rin,Darl ~ beta^2 RE = 1 M; Rin,single ~ beta RE = 10 k; vout/vs ~ Rin/(Rin+Rs) * 1.
- Stim: 1 Vpk at 1 kHz on 6 V DC.
- Oracle: Darlington output 0.91 Vpk (+/-10 %), DC level ~4.6 V (two VBE drops); single-NPN copy 0.09 Vpk (+/-30 %).

### 17. Class B crossover distortion  `XOver`
- Ref: [SS] 12.3 (class B), 12.3.2 crossover distortion; [SS] 12.4 fix with VBE bias.
- Circuit: NPN/PNP pair, bases tied directly to the input, emitters joined to RL = 100 ohm, +/-12 V rails. No bias diodes (the existing push-pull template is the biased version).
- Equation: vo = 0 for |vi| < VBE(on) ~ 0.6 V; vo ~ vi - 0.7 outside. Dead-zone ~1.2-1.4 V of input.
- Stim: 1.5 Vpk at 1 kHz.
- Oracle: output flat at 0 V while |vi| < 0.6 V (+/-0.1 V); output peak 0.8 V (+/-0.1 V). Add a 2 Vpk case: output THD dominated by the notch.

### 18. Class A follower with current-source load: asymmetric clipping  `ClsA`
- Ref: [SS] 12.2 (class A output stage, transfer characteristic, Fig. 12.2-12.3).
- Circuit: NPN emitter follower, collector +10 V, emitter to RL = 100 ohm and to a 10 mA constant current sink (existing BJT current-source template) going to -10 V.
- Equations: negative limit vo,min = -I RL = -1.0 V (transistor cuts off); positive limit ~ VCC - VCE,sat ~ +9.7 V.
- Stim: 3 Vpk at 1 kHz.
- Oracle: negative clip at -1.00 V (+/-10 %); positive peak 2.3 V (vi - 0.7, unclipped). Raise I to 50 mA: no clipping.

### 19. VBE multiplier (rubber diode)  `VBEx`
- Ref: [SS] 12.4.2? (biasing class AB with a VBE multiplier); also thermally tracking bias, first step toward bandgap refs.
- Circuit: NPN with R1 = 2.2 k (collector-base) and R2 = 1 k (base-emitter), fed from a 1 mA current source (5 V through 4.3 k is fine) with a 0.5 mApk / 1 kHz AC current added.
- Equations: VCE = VBE (1 + R1/R2) = 0.7 * 3.2 = 2.24 V; incremental resistance r ~ (R1||R2)/(1 + gm R2)... ~ (R1+R2)/(1+gm R1) ~ 3.2 k/89 = 36 ohm.
- Stim: DC 1 mA, plus optional 0.5 mApk ac to show stiffness.
- Oracle: VCE = 2.2 V (+/-5 %); ac ripple across it 18 mVpk (+/-40 %).

### 20. Four-resistor bias: IC independent of beta  `Bias4`
- Ref: [SS] 7.5.1 (classical discrete-circuit bias), 7.5.3 (constant-current-source bias); [AL] 7.? (MOSFET bias point).
- Circuit: VCC = 12 V; R1 = 47 k, R2 = 10 k, RE = 1 k, RC = 3.3 k; two copies with beta = 50 and beta = 200 (P3). Optional third copy: fixed-base bias RB = 1 M with no RE, showing IC = beta * 11.3 uA = 0.57 mA vs 2.26 mA.
- Equations: VB = 12 * 10/57 = 2.1 V (minus base loading); IE = (VB - 0.7)/RE ~ 1.3-1.4 mA; VC = 12 - IC RC.
- Stim: DC.
- Oracle: IC = 1.35 mA (+/-8 %) for both betas (difference < 10 %); VC = 7.5 V (+/-5 %); fixed-bias copy: IC ratio between betas = 4 (+/-10 %).

### 21. MOSFET amplifier, large-signal transfer and SCS incremental gain  `SCS`
- Ref: [AL] 7.4-7.6 (large-signal analysis with SCS model), 8.2 (small-signal), Example 8.1.
- Circuit: VS = 10 V, RL = 10 k, NMOS with K = 1 mA/V^2, Vt = 1 V (P2); gate driven by 2.0 V DC + 0.1 Vpk / 1 kHz. No coupling caps (the existing CS template is cap-coupled).
- Equations: ID = (K/2)(VGS - Vt)^2 = 0.5 mA; VDS = 10 - 5 = 5 V (saturation check: VDS > VGS - Vt OK); gm = K(VGS - Vt) = 1 mA/V; Av = -gm RL = -10. Second stimulus: triangle 0-3 V at 100 Hz for the transfer curve: cutoff below 1 V, parabola until VDS = VGS - Vt at VGS ~ 2.3 V, triode beyond.
- Stim: 0.1 Vpk at 1 kHz on 2.0 V DC; then triangle sweep with scope in XY (P4).
- Oracle: vout = 1.0 Vpk inverted (+/-15 %) on 5.0 V DC (+/-5 %); with 0.5 Vpk input the output is visibly asymmetric (up 2.5 V, down ~ -3.6 V ... clipped by triode), showing the SCS limit.

---

## Group 4: Feedback and op-amp non-idealities

### 22. Negative feedback gain desensitivity  `Desen`
- Ref: [SS] 11.1-11.2 (feedback structure, gain desensitivity); [AL] 15.? (op amp with feedback).
- Circuit: VCVS with A = 1000 (COMP_VCVS) as the "amplifier"; feedback divider R1 = 9 k / R2 = 1 k (beta = 0.1) from output to the inverting input; second copy with A = 100.
- Equations: Af = A/(1 + A beta): 1000 -> 9.90; 100 -> 9.09; sensitivity dAf/Af = (dA/A)/(1 + A beta): a 10x change in A moves Af by only 8 %.
- Stim: 0.1 Vpk at 1 kHz.
- Oracle: vout = 0.990 Vpk (+/-0.5 %) and 0.909 Vpk (+/-0.5 %).

### 23. Feedback lowers output resistance  `Rout`
- Ref: [SS] 11.3-11.4 (series-shunt topology, Rof = Ro/(1 + A beta)); [AL] 15.?.
- Circuit: COMP_OPAMP_REAL with A = 100 and Ro = 100 ohm (P1), non-inverting gain 10 (9 k/1 k); load switch between RL = 1 k and RL = 10 ohm. Open-loop copy: same VCVS + 100 ohm driving the same loads.
- Equations: Rof = Ro/(1 + A beta) = 100/11 = 9.1 ohm; loaded/unloaded ratio = RL/(RL + Rof).
- Stim: 0.1 Vpk at 1 kHz; toggle the load switch mid-trace.
- Oracle: closed loop: 0.909 Vpk into 1 k -> 0.476 Vpk into 10 ohm (ratio 0.52, +/-3 %); open loop drops to 9 % (ratio 0.091).

### 24. Op-amp slew-rate limiting and full-power bandwidth  `Slew`
- Ref: [SS] 2.8.2-2.8.3 (slew rate, full-power bandwidth); [AL] 15.? (saturation).
- Circuit: COMP_OPAMP_REAL follower, SR = 0.5 V/us, +/-15 V rails (P1); input 5 Vpk sine, frequency selectable 1 kHz / 50 kHz.
- Equations: fM = SR/(2 pi Vo,pk) = 0.5e6/(2 pi 5) = 15.9 kHz; above fM the output is a triangle of peak SR*T/4 = 0.5 V/us * 20 us / 4 = 2.5 V at 50 kHz.
- Stim: 5 Vpk at 50 kHz (200 us window = 10 cycles); compare 1 kHz (clean).
- Oracle: 50 kHz output is a triangle of 2.5 Vpk (+/-10 %) with slope 0.5 V/us; 1 kHz output 5.0 Vpk sine. Companion: raise input to 20 Vpk at 1 kHz to show rail saturation at ~+/-13.5-15 V.

### 25. Finite gain-bandwidth: closed-loop corner = GBW / gain  `GBW`
- Ref: [SS] 2.7 (frequency response of op-amp circuits), 2.7.2-2.7.3.
- Circuit: COMP_OPAMP_REAL with A0 = 1e5, GBW = 1 MHz (P1); non-inverting gain 100 (99 k/1 k) and a second copy at gain 10 (9 k/1 k).
- Equations: f3dB = GBW/(1 + R1/R2) = 10 kHz and 100 kHz; |Af(f)| = Af0/sqrt(1 + (f/f3dB)^2).
- Stim: 10 mVpk; sweep 100 Hz - 1 MHz, or spot 1 kHz / 10 kHz / 100 kHz.
- Oracle (gain-100 copy): 1.00 Vpk at 1 kHz (+/-1 %), 0.707 Vpk at 10 kHz (+/-5 %), 0.10 Vpk at 100 kHz (+/-10 %); gain-10 copy still 0.0995 Vpk at 10 kHz.

### 26. Op-amp saturation and the loss of the virtual short  `Sat`
- Ref: [AL] 15.5? (op amp saturation), [SS] 2.8.1 (output voltage saturation).
- Circuit: inverting amp gain -10 (10 k / 100 k), rails +/-15 V (real op amp or ideal op amp with rail limits); input 2 Vpk at 1 kHz.
- Equations: vo = -10 vi until |vo| hits the rail; while clipped, v- is no longer 0: v- = (vi R2 + vo R1)/(R1 + R2).
- Stim: 2 Vpk at 1 kHz; second trace on the inverting input node.
- Oracle: output clips at +/-15 V ideal (+/-13.5 V for a 741 model, +/-1 V tolerance); clipping starts at |vi| = 1.5 V (35 % of each half cycle is flat); inverting-node voltage rises to (2 - 1.5) * 100/110 = 0.45 V at the input peak (+/-10 %).

---

## Group 5: Digital (static discipline, CMOS gates, latches, memory)

### 27. CMOS inverter voltage transfer characteristic and noise margins  `VTC`
- Ref: [SS] 14.2 (static operation of the CMOS inverter, VM, VIL, VIH, NMH, NML); [AL] 5.3 (static discipline), 6.? (NMOS inverter transfer).
- Circuit: existing CMOS inverter with matched devices (kn = kp, Vt = 1 V), VDD = 5 V; gate driven by a triangle 0-5 V at 100 Hz; scope XY (P4) or plot vout vs t and read at known times.
- Equations: VM = VDD/2 = 2.5 V for matched devices; VIL = (3 VDD + 2 Vt)/8 = 2.125 V, VIH = (5 VDD - 2 Vt)/8 = 2.875 V; NMH = NML = VDD/2 - ... = (3 VDD + 2 Vt)/8 = 2.125 V. [SS Eqs. 14.7-14.10 for matched inverter].
- Stim: triangle 0-5 V at 100 Hz (100 ms window = 10 sweeps; 10 ms for one).
- Oracle: VM = 2.50 V (+/-0.15 V); VOH = 5.0, VOL = 0.0; slope at VM >> 1 (ideally -infinite; sim gives finite with lambda). Variant with kp = kn/4: VM shifts to (1 + 0.5*4)/(1 + 0.5) = 2.0 V.

### 28. Resistor-pull-up NMOS inverter: static discipline VOL/VOH  `NMinv`
- Ref: [AL] 5.3-5.5 (static discipline, noise margins), 6.4-6.6 (MOSFET switch and the NMOS inverter, VOL from the SR model).
- Circuit: VDD = 5 V, RL = 10 k, NMOS (K = 1 mA/V^2, Vt = 1 V); triangle input.
- Equations: VOH = 5 V; VOL: with the SR model Ron = 100 ohm gives 5 * 100/10100 = 50 mV; with the SCS model solve 5 - 10k * K[(VGS - Vt)VDS - VDS^2/2] -> VOL ~ 0.13 V at VGS = 5 V; VM: output = input when (5 - v)/10k = 0.5(v - 1)^2 -> v ~ 1.79 V.
- Stim: triangle 0-5 V at 100 Hz.
- Oracle: VOL = 0.13 V (+/-0.05 V) with the square-law model (0.05 V with SR model); VOH = 5.00 V; VM = 1.8 V (+/-0.1 V). Choose VIL = 1 V, VIH = 2.5 V, VOL = 0.5 V, VOH = 4 V as a valid static discipline and show NML = 0.5 V, NMH = 1.5 V.

### 29. CMOS NAND2 and NOR2 with truth-table timing and shifted VM  `NAND`
- Ref: [SS] 14.3 (CMOS logic-gate circuits: NAND, NOR, sizing); [AL] 6.? (NAND with switch model).
- Circuit: NAND2 = 2 series NMOS (to ground) + 2 parallel PMOS (to VDD); NOR2 = the dual. VDD = 5 V, all devices equal (kn = kp, Vt = 1). Inputs A = square 1 kHz, B = square 2 kHz (use two COMP_CLOCK), output to a 100 pF load.
- Equations: out = NOT(A AND B). With A = B tied and driven by a triangle, VM,NAND = (Vt + sqrt(2)(VDD - Vt))/(1 + sqrt(2)) = 2.76 V, VM,NOR = (Vt + (1/sqrt(2))(VDD - Vt))/(1 + 1/sqrt(2)) = 2.24 V (series NMOS/PMOS at half strength).
- Stim: two clocks 1 kHz and 2 kHz (2 ms window = 2 truth-table sweeps); tie-and-triangle variant for VM.
- Oracle: output low only during the 25 % of the period where A = B = 1; VM,NAND = 2.76 V (+/-0.15 V); VM,NOR = 2.24 V (+/-0.15 V). Name the NOR copy `NOR`.

### 30. NMOS pass transistor vs CMOS transmission gate  `TGate`
- Ref: [SS] 14.4 (pass-transistor logic, transmission gate; "weak 1" = VDD - Vt); [AL] 6.? (switch abstraction).
- Circuit: two branches from a 0-5 V square input: (a) single NMOS pass transistor, gate at 5 V; (b) CMOS TG (NMOS gate 5 V, PMOS gate 0 V); each into CL = 1 nF to ground (no other load).
- Equations: (a) high level charges only to VDD - Vt = 4.0 V (body effect makes it lower); (b) full 5.0 V. Low level 0 V in both.
- Stim: square 0-5 V at 1 kHz.
- Oracle: (a) settles at 4.0 V (+/-0.2 V, lower if the sim models body effect); (b) 5.0 V (+/-0.05 V); both reach 0 V on the low half.

### 31. SR latch from cross-coupled NOR gates  `SRlat`
- Ref: [SS] 15.1.1 (the SR flip-flop / latch); [AL] 5.? sequential logic intro.
- Circuit: two NOR2 (from #29, 8 MOSFETs) cross-coupled: Q = NOR(R, Qbar), Qbar = NOR(S, Q); S and R from COMP_PULSE_SOURCE; 10 pF on each output.
- Equations: S=1,R=0 -> Q=1; S=0,R=1 -> Q=0; S=R=0 -> hold; S=R=1 forbidden (both 0).
- Stim: S pulse 5 V, 50 us wide at t = 0.2 ms; R pulse at t = 0.6 ms; period 1 ms (10 ms window).
- Oracle: Q rises to 5 V at 0.2 ms and holds until 0.6 ms (+/-0.5 us for gate delays), Qbar complementary; at power-up the state is whichever the DC solver picks (document, do not oracle).

### 32. D latch with transmission gates  `Dlat`
- Ref: [SS] 15.1.2-15.1.3 (CMOS latch with TGs, master-slave D flip-flop).
- Circuit: TG1 (clk high) passes D into inverter pair; TG2 (clk low) closes the feedback loop; 2 inverters + 2 TGs + 1 inverter for clkbar = 10 MOSFETs.
- Equations: Q follows D while CLK = 1 (transparent), holds the last value while CLK = 0.
- Stim: CLK = clock 1 kHz; D = square 300 Hz (10 ms window shows 10 clocks, 3 D changes).
- Oracle: Q equals D at every falling CLK edge and stays constant across each CLK-low interval (500 us); Q changes only during CLK-high. Extension: two copies in series with inverted clocks = master-slave D flip-flop (edge-triggered) `DFF`.

### 33. 6T SRAM cell: write and hold  `SRAM`
- Ref: [SS] 15.2.1? (SRAM cell, read and write operations); [SS] Fig. 15.? 6T cell.
- Circuit: two cross-coupled CMOS inverters (Q, Qbar), two NMOS access transistors from bitlines BL / BLbar to Q / Qbar, gates on word line WL; BL/BLbar driven by complementary pulse sources for a write, or by 5 V through 10 k with 100 pF (precharged) for a read.
- Equations: write succeeds when the access NMOS overpowers the PMOS pull-up (W/L access > W/L PMOS, typically kn,access >= 1.5 kp); hold when WL = 0; read: precharged BL on the 0 side droops through access+NMOS, Q must stay below the inverter VM (cell ratio > 1.2 typical).
- Stim: WL pulse 5 V, 100 ns...1 us wide; BL = 5 V / BLbar = 0 V for write-1, swapped 1 ms later for write-0; 5 ms window.
- Oracle: after the write-1 pulse Q = 5.0 V, Qbar = 0 V and remains so with WL = 0 (+/-0.05 V); after write-0 the cell flips; if access devices are made 4x weaker (kn = 0.25 mA/V^2) the write fails (Q stays) - useful negative test.

---

## Notes for implementation

1. Item count: 33 entries in 5 groups (Basics 5, Transients 7, Amplifiers 9, Feedback 5, Digital 7 + NOR/DFF sub-variants). If the palette must stay at 30, drop #5 (Bridg), #12 (Flybk) and #26 (Sat, fold into #24).
2. Every oracle is a single scalar readable with the scope cursors; the tolerances are wide where the result depends on the sim's device model (BJT VT/beta, MOS K/Vt, op-amp Ro/SR). Items marked P1-P5 need a capability check before being added to TEMPLATE_AUDIT.md.
3. Section numbers: [AL] numbers are from the 2005 edition ToC and are reliable to the chapter; sub-sections marked "?" should be confirmed against the physical book before being printed in the in-app hint text. [SS] 7th-ed numbers are likewise reliable to the chapter (2 op amps, 4 diodes, 7 transistor amplifiers, 8 IC building blocks, 10 frequency response, 11 feedback, 12 output stages, 14 CMOS logic, 15 memory).
4. Template-builder trap from memory: keep node spacing > 5 px in the builders or the node-merge pass will short adjacent nodes (matters most for the 8-10 transistor digital items #29-#33).
5. Suggested in-app hint format (one line): "Sedra/Smith 14.3 - CMOS NAND2 - VM = 2.76 V with A=B; out low only when A=B=1".
