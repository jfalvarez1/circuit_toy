# How AEP Engineers Its Transmission System: Electrical and Protocol View for Simulator Teaching Examples

Research note (2026-08-24). Audience: someone building single-phase-equivalent 60 Hz teaching circuits
(AC sources, R/L/C, turns-ratio transformers, diodes, op-amps/comparators, switches, logic gates, scope).
All phase-to-neutral values below are single-phase equivalents: V_ln = V_ll / sqrt(3); a 3-phase MW figure
divided by 3 is the per-phase power. Items marked [typ] are industry-typical figures, not AEP-published numbers.

## 1. AEP system facts

- Footprint: 11 states (AR, IN, KY, LA, MI, OH, OK, TN, TX, VA, WV), 5.6 M customers, ~40,000 circuit-miles of
  transmission (largest in the US), ~252,000 miles of distribution [S1]. 2007 mileage by class: 765 kV 2,116 mi,
  500 kV 113 mi, 345 kV 5,910 mi, 230 kV 140 mi, 161 kV 282 mi, 138 kV 16,202 mi, 115 kV 66 mi, <100 kV 14,230 mi [S2].
  Today >2,200 mi of 765 kV, 30 substations, 6 states, >8,000 structures [S3][S4].
- Why 765 kV: AEP overlaid 345 kV in the 1950s and 765 kV in the 1960s so that "the resources of all power plants
  be available, without transmission constraints, to all parts of the system." Test line at Apple Grove WV (1961),
  first commercial 765 kV line energized May 1969 (KY-OH) [S2][S4]. One 765 kV circuit carries up to 6x a 345 kV
  circuit with about half the losses and less ROW per MW [S3]. AEP's loadability table (300 mi): 765 kV 2200-2400 MW,
  500 kV 900 MW, 345 kV double-ckt 800 MW, 345 kV single-ckt ~400 MW; "reach" of 1500 MW: 765 kV 550 mi, 500 kV 140 mi,
  345 kV DC 110 mi, 345 kV SC ~50 mi [S2].
- Voltage classes and roles: 765 kV = interstate backbone; 345 kV = regional bulk; 138 kV = AEP's workhorse
  sub-transmission (16,000+ mi); 69 kV = rural sub-transmission feeding 69/12.47 kV distribution stations.
  AEP treats 765/500/345 kV (and transformers with secondaries >= 345 kV) as EHV/BES [S5].
- Interconnections: AEP East (Ohio, I&M, APCo, KPCo, Kingsport) is in PJM; AEP West (PSO, SWEPCO) in SPP; AEP Texas in
  ERCOT (asynchronous to the Eastern Interconnection, tied only through HVDC back-to-back links such as Oklaunion and
  Welsh, both AEP-affiliated [typ]).
- AEP Texas = former Central Power & Light (Corpus Christi, Rio Grande Valley, Victoria, Laredo -> AEP Texas Central)
  and West Texas Utilities (Abilene, San Angelo -> AEP Texas North): >1 M meters, ~97,000 sq mi, 93 counties, a TDU in
  the ERCOT competitive market [S6]. Electric Transmission Texas (ETT, AEP/Berkshire JV) built ~465 mi of double-circuit
  345 kV plus 178 mi of 138 kV upgrades under CREZ (20% of the program), ~$1.5 B, last line energized Dec 2013 [S7].
  In April 2025 the PUCT approved ERCOT's first 765 kV lines (Permian Basin Reliability Plan); AEP Texas + CPS Energy's
  Howard-Solstice line is ~300-370 mi, Fort Stockton area to San Antonio [S8][S9]. ERCOT's 765 kV STEP costs $32.99 B
  vs $30.75 B for the 345 kV plan, saves 560 GWh/yr losses and adds 600-3000 MW transfer capability [S10].
- Transformers [typ]: 765/345 kV autotransformer banks 1500-2250 MVA (three 500-750 MVA single-phase units, one spare),
  Z ~ 8-14% on bank base; 345/138 kV autos 450-650 MVA, Z ~ 8-12%; 138/12.47 kV distribution 20-50 MVA, Z ~ 8-10%.
  Impedance in ohms: Z_ohm = Z% * kV^2 / MVA (e.g. 10%, 138 kV, 50 MVA -> 38 ohm on the 138 kV side). Station cost
  split for 765 kV: 70% materials, of which 50% transformers, 20% breakers [S2].
- EHV compensation: long 765 kV lines carry large charging (~500-600 Mvar/100 mi at 765 kV [typ]), so AEP hangs
  300 Mvar-class shunt reactors on line ends to hold light-load voltage; series capacitors (30-70%) are applied on the
  longest EHV paths to raise stability limits; Jacksons Ferry 765 kV has an SVC (-450 to +450 Mvar, 3 x 150 Mvar TSC +
  450 Mvar reactor) [S11]. AEP's Wyoming-Jacksons Ferry (90 mi, 2006) is North America's first six-conductor-bundle
  765 kV line; earlier AEP 765 kV used four-conductor bundles [S12].

## 2. Protection and control (P&C)

- Philosophy: overlapping zones (line, bus, transformer, breaker) so every element has primary and backup protection;
  fully redundant "System A / System B" at 200 kV and above, including batteries, DC panels, trip coils, high-speed
  relays, communication paths and instrument-transformer secondaries [S5]. Breaker failure is required on all BES
  elements [S5]. AEP's stated clearing time for its own high-speed protection is ~5 cycles (83 ms) [S5].
- Relay hardware: AEP does not publish model numbers in its interconnection requirements; it states that "type, model
  numbers, and firmware version" are agreed per project [S5]. In practice the North American transmission fleet is
  dominated by SEL (SEL-311C/321 step distance, SEL-411L line differential + distance, SEL-421 distance/breaker, SEL-487E
  transformer, SEL-487B bus/BF, SEL-451 bay control) and GE Multilin UR (D60/L90/T60/B90) [S13][S14]; AEP is a large
  SEL user [typ].
- Line protection (AEP ranking, from [S5]):
  1. Line current differential 87L over fiber (preferred pilot scheme; AEP prefers direct fiber over multiplexers,
     needs redundant diverse fiber). Concept: I_local + I_remote (into the zone) = 0 except for internal faults.
  2. Directional comparison blocking (DCB) over power-line carrier where existing DCB or PLC exists. POTT (permissive
     overreaching transfer trip) is the other common directional-comparison scheme. AEP may require dual pilot schemes.
  3. Step distance backup: Zone 1 = 80-90% of line impedance, instantaneous; Zone 2 = 120-150% (at least 50% into the
     next line), ~0.3 s (18-20 cycles); Zone 3 = remote backup covering the longest adjacent line, ~1 s [S15][S16].
     Mho characteristic: trip if apparent Z = V/I lies inside a circle through the origin with diameter Z_reach.
- Transformer 87T: percentage differential, slope 25-40%, with 2nd-harmonic restraint (block if I_2nd/I_fund > 15-20%)
  to ride through magnetising inrush; 5th-harmonic block for overexcitation [typ, SEL-487E]. Bus 87B: high-impedance or
  low-impedance differential summing all feeders (SEL-487B). Breaker failure 50BF: on trip, if current > pickup persists
  past a timer of 8-12 cycles (133-200 ms), trip all adjacent breakers [typ].
- Distribution feeders (12.47 kV): 50 instantaneous set above the max fault current at the first downstream fuse/recloser;
  51 phase pickup 1.25-1.5x max load with a very-/extremely-inverse IEEE curve; 51N ground pickup 0.25-0.5x phase pickup.
  Reclosing: typically one fast reclose (0.3-0.5 s) then 1-2 delayed (15-45 s), lockout after 3-4 shots. Fuse saving =
  trip fast before the lateral fuse blows; fuse blowing = let the fuse clear permanent lateral faults [typ].
  AEP requires a customer's fuse to clear faster than AEP's 5-cycle clearing so AEP's reclose succeeds [S5].
- Instrument transformers: CT secondaries are 5 A nominal (ratio e.g. 600:5, 2000:5, 3000:5); VT/CVT secondaries 120 V
  (line-line) or 69.3 V (line-neutral), e.g. 345,000:120 = 2875:1, 138,000:120 = 1150:1. IEEE C57.13 class C800 means
  the CT will deliver 800 V (20 x 5 A = 100 A into a standard 8-ohm burden) with <10% ratio error; higher class = less
  saturation on through faults. Relay-side apparent impedance: Z_sec = Z_pri * CTR / VTR [typ].

## 3. Protocols and control layer

- SCADA/EMS: substations have RTUs polled by the control centre; AEP's interconnection standards SS-500000 (RTU) and
  SS-502000 (point selection) govern this; supervisory control is mandatory on every interconnecting breaker/MOAB [S5].
  Inter-utility real-time data goes over ICCP (IEC 60870-6/TASE.2) to PJM, SPP and ERCOT [S5].
- DNP3 (IEEE 1815): the dominant RTU/relay-to-master protocol in North America, serial (RS-232/485, 9600-19200 bps) or
  TCP/IP port 20000. Point classes: binary inputs (breaker status), analog inputs (MW, Mvar, kV, A), counters, binary/
  analog outputs (trip/close, setpoints); masters do integrity polls (every 30-60 min) plus event (class 1/2/3) polls
  every 1-10 s; unsolicited reporting optional [typ]. Modbus RTU/TCP survives at the edge (meters, capacitor controllers,
  battery chargers), gatewayed into DNP3.
- IEC 61850: station bus (MMS for SCADA, GOOSE multicast for peer-to-peer trip/block/interlock in <4 ms) and process bus
  (Sampled Values, 80 or 256 samples/cycle, merging units replacing copper CT/VT wiring) [S13]. US utilities mostly use
  GOOSE for interlocking/breaker failure and keep DNP3 upward; process bus is still limited.
- Synchrophasors (IEEE C37.118): PMUs report GPS-timestamped phasors at 30-60 frames/s. PMUs grew out of AEP's
  1980s computer-relaying research; prototypes were installed at two AEP 765 kV stations in the early 1990s and
  Macrodyne built the first commercial PMU in 1992 [S17]. Today PMUs are embedded in line relays (SEL-421/411L).
- Relay logic: SEL relays use SELogic control equations (Boolean + timers); GE UR uses FlexLogic; IEC 61131-3 style
  ladder/FBD appears in RTU/automation controllers (SEL-3530 RTAC). Teaching model: AND/OR gates + RC timers.
- Time sync: IRIG-B (demodulated DC-shift, 1 ms class) from GPS clocks to every relay; IEEE 1588 PTP (C37.238 power
  profile, sub-microsecond) for 61850 process bus and PMUs [typ].
- NERC CIP: BES cyber systems (relays, RTUs, EMS) are classified High/Medium/Low impact; requires electronic security
  perimeters, no routable protocol without inspection, access management, patching, and encrypted remote access.
  Practical effect: DNP3 serial or DNP3-SA, firewalled ESPs, no direct internet, engineering-access logging.
- RAS/SPS: pre-armed schemes (e.g. trip generation for loss of a 765 kV outlet, or ERCOT Permian-area generic transmission
  constraints) that act in 50-150 ms on breaker-status logic; NERC PRC-012 governs them.
- Frequency: AGC (secondary control) moves units every 4 s to zero ACE and hold 60.000 Hz. Governor deadband limit in
  ERCOT is +/-0.036 Hz for legacy mechanical governors (+/-0.017 Hz for most others); NERC BAAL uses 59.964/60.036 Hz
  [S18]. ERCOT UFLS (NERC PRC-006 regional): shed >=5% at 59.3 Hz, >=15% cumulative at 58.9 Hz, >=25% cumulative at
  58.5 Hz [S19]. Generators must ride through to 57.5 Hz for 2 s in ERCOT [typ].

## 4. Line planning, electrical side

- Limits: short lines (<50 mi) are thermal-limited (conductor temperature, sag), medium lines (50-200 mi)
  voltage-drop-limited (5% drop / 0.95 pu), long lines (>200 mi) stability-limited (angle 30-45 deg, ~1.0 SIL at 300 mi).
  St. Clair curve (AEP, 1953; updated by Dunlop/Gutman/Marchenko, AEP, 1979) expresses the safe loading as a
  multiple of SIL vs length: ~3 SIL at 50 mi, ~2 SIL at 100 mi, ~1.3 SIL at 200 mi, ~1.0 SIL at 300 mi [S20][S2].
- Surge impedance Zc = sqrt(L/C) ~ 250-300 ohm (bundled) to 400 ohm (single conductor); SIL = kV^2/Zc [typ]:
  138 kV single ~50 MW (Zc 380 ohm); 345 kV bundled ~400-420 MW (Zc 285 ohm); 500 kV ~900-1000 MW; 765 kV ~2200-2400 MW
  (Zc 250 ohm) [S2][S20].
- Conductors (ACSR, 60 Hz, per phase per mile, GMD-dependent) [typ]: Drake 795 kcmil 26/7: R = 0.119 ohm/mi at 50 C,
  X_L ~ 0.80 ohm/mi at 30 ft spacing, thermal ~900 A [S21]; 138 kV single Drake: Z1 ~ 0.12 + j0.78 ohm/mi,
  B ~ 5.3 uS/mi; 345 kV 2 x 954 (Cardinal/Rail) bundle: Z1 ~ 0.06 + j0.60 ohm/mi, B ~ 7.5 uS/mi; 765 kV 4-6 x 954 bundle:
  Z1 ~ 0.02 + j0.53 ohm/mi, B ~ 8.5 uS/mi. Bundling lowers X and corona/audible noise, raises B and SIL [S12].
- Charging: Q_c = V^2 * B per mile (345 kV: 119025 * 7.5e-6 = 0.89 Mvar/mi; 765 kV: 585225 * 8.5e-6 = 5 Mvar/mi).
  Ferranti rise on an open line: Vr/Vs = 1/(1 - X_L*B/2) (nominal pi) -> ~10% at 200 mi 345 kV. Shunt reactors absorb
  60-80% of charging; series capacitors cancel 30-70% of X_L to cut angle and raise transfer.
- Planning criteria: NERC TPL-001 (P0 no contingency; P1 N-1 single element with no load loss, voltage 0.95-1.05 pu,
  facilities within normal rating; P2-P7 multi-element/extreme events with emergency ratings and allowed load shed).
  Studies: AC power flow (thermal/voltage under N-1), short circuit (breaker interrupting duty, e.g. 63 kA at 345 kV),
  transient stability (critical clearing time, RAS), EMT for series caps/SSR [S5].
- ROW and cost (AEP, 2008 $): 765 kV 200 ft ROW, $2.6-4.0 M/mi; 500 kV 175-200 ft, $2.3-3.5 M/mi; 345 kV DC 150 ft,
  $1.5-2.5 M/mi; 345 kV SC 150 ft, $1.1-2.0 M/mi. 765 kV line cost split: siting 3%, ROW 10%, engineering 5%, materials
  41% (60% structures, 30% conductor), construction 41% [S2]. ERCOT CREZ averaged ~$1.1 M/mi (345 kV) and $6.9 B total
  for 2,400 mi/18.5 GW of wind export [S7][S22]; ERCOT 765 kV STEP is ~$33 B for the 2030 horizon [S10].

## 5. Teaching circuits with hand-computed oracles

All use one-line (phase-to-neutral) equivalents. "Rectify + hold" = diode into a C with a bleed R (tau >> 1 cycle);
the held voltage is V_pk - 0.7 V (ideal diode: V_pk). Logic high = 12 V.

### 5.1 CT + 50/51 overcurrent with RC "inverse-time" curve
- Source 7.97 kV rms (13.8 kV/sqrt3) -> series load 13.3 ohm (I = 600 A) -> CT primary. CT = transformer 120:1
  (600:5). Secondary burden R_b = 1 ohm. Fault switch adds 5.3 ohm in parallel with the load (I -> 1200 A).
- Oracle: normal I2 = 5.0 A rms, V_b = 5.0 V rms, 7.07 V pk. Half-wave rectify + hold: V_c = 6.4 V.
  Comparator ref 8.0 V (50 element): pickup at I1 >= (8.7/7.07)*600 = 738 A -> normal = no trip, fault = trip.
- 51 emulation: feed V_c into R_t = 100 k, C_t = 10 uF (tau = 1.0 s), second comparator at V_th = 8.0 V.
  t_trip = tau * ln(V_c / (V_c - 8)). I1 = 900 A: V_c = 9.9 V -> 1.65 s; 1200 A: V_c = 13.4 V -> 0.90 s;
  2400 A: V_c = 27.6 V -> 0.34 s. Trip time falls as current rises: an inverse curve, like an IEEE VI curve.

### 5.2 Differential relay (87) with two CTs
- Source 7.97 kV with 1 ohm source R -> CT1 primary -> protected zone -> CT2 primary -> 20 ohm load. Both CTs 120:1
  with 1 ohm burdens; op-amp subtractor gives |V1 - V2|; rectify + hold; comparator ref 1.0 V. Internal-fault switch:
  2 ohm to ground between CT1 and CT2.
- Oracle, no fault (and any "through" fault beyond CT2): I = 7970/21 = 380 A -> both secondaries 3.16 A, V1 = V2,
  difference 0 -> no trip. Internal fault: R_par = 2 || 20 = 1.818 ohm, I1 = 7970/2.818 = 2828 A, V_mid = 5141 V,
  I_load = 257 A. V1 = 23.6 V rms, V2 = 2.14 V rms, |V1 - V2| = 21.4 V >> 1.0 V -> trip. Mention: real relays add
  a slope (I_op > 0.3 * I_restraint) to tolerate CT ratio mismatch and saturation.

### 5.3 Distance relay (21) Zone 1 reach test
- 345 kV line, 50 mi, Z1 = 3 + j30 ohm (R = 3 ohm, L = 79.6 mH); source 199 kV rms behind j10 ohm (L = 26.5 mH).
  Fault switch to ground at 40% (1.2 + j12), 80% (2.4 + j24) and 100% (3 + j30) of the line.
  VT 2875:1 (345 kV/120 V), CT 400:1 (2000:5). Zone 1 reach 0.8 * 30.15 = 24.1 ohm primary -> 24.1*400/2875 =
  3.35 ohm secondary. Relay: CT secondary through R = 3.35 ohm gives V_I = I_sec * 3.35; compare rectified V_I vs
  rectified V_sec (VT). Trip if V_I > V_V, i.e. |Z_app| < reach.
- Oracle: 40% fault: I = 199k/|1.2 + j22| = 9.03 kA, V_relay = 108.9 kV; V_sec = 37.9 V, V_I = 22.6 * 3.35 = 75.6 V
  -> trip. 80%: I = 5.84 kA, V_sec = 48.98 V, V_I = 48.9 V -> balance point (reach boundary). 100%: I = 4.96 kA,
  V_sec = 52.0 V, V_I = 41.6 V -> no trip (this is Zone 2's job, add a 0.3 s RC timer to a second comparator with
  reach 1.25 * 30.15 = 37.7 ohm -> 5.24 ohm secondary).

### 5.4 Under-frequency load-shed detector (81U)
- Source 10 V, run at 60.0, 59.3, 58.9, 58.5 Hz. Zero-crossing comparator squares the wave; during the positive half
  the square drives R = 10 k into C = 10 uF (tau = 100 ms); a switch (or diode clamp) driven by the negative half
  resets C. Peak of the ramp = 10 * (1 - exp(-T/2/tau)), captured by a diode + hold, compared against three refs.
- Oracle: half-periods 8.333/8.432/8.489/8.547 ms -> ramp peaks 0.7995/0.8086/0.8139/0.8192 V. Refs 0.804 V (stage 1,
  shed 5%), 0.811 V (stage 2, +10%), 0.8165 V (stage 3, +10%) [S19]. Add an op-amp stage (gain 100, offset 0.79 V) so
  the scope shows 0.95/1.86/2.39/2.92 V. Point out the 9 mV steps: this is why real 81 relays count cycles against a
  crystal clock and add 6-cycle security timers.

### 5.5 Long 345 kV line: Ferranti rise, shunt reactor, series capacitor
- 200 mi at 0.06 + j0.60 ohm/mi, B = 7.5 uS/mi: nominal pi = R 12 ohm, L 318 mH, C 1.99 uF at each end. Source
  199 kV rms. Load switch: open / shunt reactor 3.54 H (X = 1333 ohm) / series cap 44.2 uF (X = 60 ohm, 50% comp.).
- Oracle (lossless): open end Vr/Vs = 1/(1 - 120 * 0.00075) = 1.099 -> Vr = 218.8 kV (+9.9% Ferranti rise, 379 kV
  line-line, above the 362 kV equipment limit). Shunt reactor exactly cancelling C/2: current in the series branch
  -> 0, Vr = Vs (1.000). With 50% series cap and open end: 1/(1 - 60 * 0.00075) = 1.047 (rise halves).
  Scope: Vr leads/lags Vs by only a few degrees when unloaded; with R present, expect ~0.5% lower magnitudes.

### 5.6 SIL demonstration on the same line
- Replace the open end with a resistive load: SIL R = Zc = sqrt(0.60/7.5e-6) = 283 ohm (P = 3 * 199k^2/283 = 420 MW,
  matches 345^2/283); 2 x SIL: 141 ohm; 0.5 x SIL: 566 ohm.
- Oracle (lossless pi): SIL load: Z_r = 270.6 - j57.4, Vr/Vs = 276.6/277.7 = 0.996 (flat profile, line is reactive-
  neutral). 2 x SIL: Z_r = 139.8 - j14.8, Vr/Vs = 140.6/175.0 = 0.803 (20% drop, voltage-limited). Open: 1.099.
  Sweep length in the sim (100/200/300 mi) to trace the St. Clair idea: the permissible load in SIL units falls with length.

### 5.7 Transformer inrush and 2nd-harmonic restraint sketch
- The simulator's transformer is linear, so synthesise the CT-secondary "differential current" as V = 5 V at 60 Hz +
  V2 at 120 Hz (V2 = 2 V for inrush, 0.5 V for a real fault; true inrush has 30-60% 2nd harmonic). Two series-LC
  band-pass branches: 60 Hz: L = 100 mH, C = 70.4 uF; 120 Hz: L = 100 mH, C = 17.6 uF, each with a 10 ohm sense R.
  Rectify + hold each, then comparator: BLOCK = V120 > 0.15 * V60 (use a 0.15 gain divider on V60).
- Oracle: at 60 Hz the 120 Hz branch has |X| = |37.7 - 150.7| = 113 ohm vs 10 ohm in-band, so fundamental leakage is
  ~9%; in-band currents are V/10 ohm. Inrush case: V120/V60 ~ 0.40 -> block. Fault case: ~0.10 -> restraint released,
  87T trips (AND of the 5.2 difference detector and NOT block).

### 5.8 Breaker-failure (50BF) timing chain with logic gates
- Inputs: TRIP (switch, or 5.1's output), 50BF current detector (5.1 comparator, ref = 0.5 x nominal), breaker
  auxiliary contact (switch that opens the CT primary current path when the breaker "opens").
- Logic: START = TRIP AND 50BF -> drives R = 10 k, C = 15 uF (tau = 150 ms) -> comparator at 0.632 * 12 V = 7.58 V ->
  BFT = timer AND 50BF -> "trip adjacent breakers" indicator. A reset diode/switch discharges C when START drops.
- Oracle: healthy breaker opens 5 cycles (83 ms) after TRIP: current -> 0, START drops at 83 ms, C reaches only
  12 * (1 - exp(-0.083/0.15)) = 5.1 V < 7.58 V, no BFT. Stuck breaker: BFT at t = 150 ms (9 cycles) after TRIP; with
  1-cycle relay and 3-cycle backup breakers total clearing ~13 cycles (217 ms), the number planners use for stability.

## Sources

- [S1] AEP news, DOE loan guarantee release, https://www.aep.com/news/stories/view/10501/
- [S2] AEP "Transmission Facts" Q&A (2008), https://web.ecs.baylor.edu/faculty/grady/_13_EE392J_2_Spring11_AEP_Transmission_Facts.pdf
- [S3] AEP 765 kV Transmission page, https://www.aep.com/about/businesses/transmission/765kv/
- [S4] "Experience with the AEP 765-kV system" (OSTI), https://www.osti.gov/biblio/4341204
- [S5] AEP "Requirements for Connection of New Facilities... to the AEP Transmission System" Rev 6, https://docs.aep.com/docs/requiredpostings/TransmissionStudies/Requirements/AEP_Interconnection_Requirements_Rev6.pdf
- [S6] AEP Texas fact sheet, https://www.aeptexas.com/lib/docs/company/about/2024_AEP_Texas_Fact_Sheet.pdf
- [S7] ETT energizes last CREZ line, https://www.aep.com/news/releases/read/1338/ETT-Energizes-Last-of-Seven-CREZ-Transmission-Lines-in-West-Texas
- [S8] AEP Texas 765 kV announcement, https://www.aep.com/news/stories/view/10165/
- [S9] Howard-Solstice project page, https://www.aeptransmission.com/texas/howard-solstice/
- [S10] ERCOT Trending Topics: 345 kV vs 765 kV STEP (Dec 2025), https://www.ercot.com/files/docs/2025/01/28/ERCOT_Trending_Topic_345-kV_vs_765-kV_Transmission.pdf
- [S11] WSP, Jacksons Ferry 765 kV SVC, https://www.wsp.com/en-us/projects/jacksons-ferry-substation-765-kv-svc
- [S12] ASCE, "765kV Tower Design for 6-Conductor Bundle", https://ascelibrary.org/doi/10.1061/40790%28218%299
- [S13] SEL-411L product page, https://selinc.com/products/411l/
- [S14] SEL-487E / SEL-487B product pages, https://selinc.com/products/487E/ , https://selinc.com/products/487B/
- [S15] SynchroGrid, "A Guide for Calculating Step Distance Relay Settings", https://synchrogrid.com/wp-content/uploads/2021/10/A-Guide-for-Calculating-Step-Distance-Relay-Settings_SynchroGrid_.pdf
- [S16] SEL, "Considerations and Benefits of Using Five Zones for Distance Protection", https://selinc.com/api/download/122907/
- [S17] Phadke/Kezunovic et al., overview of IEEE C37.118.2 and PMU history, https://kezunovic.engr.tamu.edu/wp-content/uploads/sites/282/2023/04/An_Overview_of_the_IEEE_Standard_C37.118.2Synchrophasor_Data_Transfer_for_Power_Systems.pdf
- [S18] ERCOT Nodal Operating Guides Sec. 2 / NERC BAL-003, https://www.ercot.com/files/docs/2022/05/27/02-052722.doc , https://www.nerc.com/globalassets/standards/projects/2017-01/related-files/2017-01_draft_version_2_reliability_standard_bal-003-3_04182023-2.pdf
- [S19] ERCOT Nodal Operating Guides Sec. 8 (UFLS) and PUCT load-shed study, https://www.ercot.com/files/docs/2021/09/01/08C-120922.doc , https://ftp.puc.texas.gov/public/puct-info/agency/resources/reports/leg/PUC_Load_Shed_Protocols_Study.pdf
- [S20] MISO/ERCOT EHV workshop (SIL, St. Clair), https://www.ercot.com/files/docs/2023/06/27/2_ERCOT%20Discussion%20of%20EHV%20and%20HVDC_MISO_Tackett_20230626.pdf
- [S21] 795 ACSR Drake impedance guide, https://industrialmonitordirect.com/blogs/knowledgebase/calculating-transmission-line-impedance-per-mile-for-795-acsr
- [S22] Baker Institute, "Texas CREZ Lines", https://www.bakerinstitute.org/sites/default/files/2020-11/import/ces-pub-texascrez-111720.pdf
