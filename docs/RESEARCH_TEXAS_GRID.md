# Research Note: The Texas (ERCOT) Grid as Teaching Examples for the Circuit Simulator

Purpose: give concrete, sourced numbers for generation, transmission and distribution in Texas, then turn
each voltage level into a **single-phase-equivalent, 60 Hz** circuit the simulator can build with its
existing parts (AC source, R, L, C, two-winding / center-tapped transformer, resistor loads, scope).
All hand calculations below are RMS phasor calculations; they are intended as **test oracles**
(expect the simulator to agree within ~1% once it reaches steady state).

Conventions used everywhere in this note:
- Source amplitude = **peak** of the phase-to-neutral voltage: `Vpk = sqrt(2) * V_LL / sqrt(3) = 0.8165 * V_LL`.
- Line inductance `L = X / (2*pi*60) = X / 376.99`; shunt capacitance `C = B / 376.99`.
- A three-phase load of `P3` MW is modelled as `P3/3` per phase: `R_load = V_LN^2 / (P3/3)`.
- Transformer turns ratio = ratio of phase-to-neutral voltages (same as line-to-line ratio for Y-Y).
  Transformer impedance `%Z` is placed as series R + L on the secondary: `Z = %Z/100 * V_sec^2 / S_rated`,
  `R = Z / sqrt(1 + (X/R)^2)`, `X = R * (X/R)`.

## 1. ERCOT overview

- ERCOT serves ~90% of Texas load and is **not synchronously connected** to the Eastern or Western
  Interconnections. Only asynchronous **DC ties** connect it: two to SPP (North/Oklaunion 220 MW and
  East/Welsh 600 MW, 820 MW total) and ties to Mexico's CFE (Railroad 150 MW, Laredo variable-frequency
  transformer 100 MW; Eagle Pass 36 MW, since decommissioned) [ERCOT DC-tie manual, ERCOT Grid Insights].
  Because no external AC grid helps hold frequency, ERCOT must balance 60.000 Hz itself; the Feb 2021
  event took frequency to 59.3 Hz, within minutes of automatic generator trips at 59.4 Hz.
- Peak demand record: **91,089 MW** (22 July 2026) [ERCOT]. Planning load forecast for 2030 exceeds 150 GW.
- Installed capacity for summer 2026 [ERCOT CDR fact sheet, Dec 2025]: natural gas 37.5%, wind 22.4%,
  solar 19.7%, coal 7.6%, battery storage 2.9%, nuclear (four units, ~5,100 MW) and other the remainder.
  Energy share, Jan-Sep 2025 [EIA]: gas ~43%, wind + solar ~36%, coal and nuclear the rest; solar
  surpassed coal as the third-largest source in 2025.
- Typical plant / unit sizes: nuclear units 1,200-1,300 MW (Comanche Peak 1,218 + 1,207 MW; South Texas
  Project 2 x ~1,280 MW); coal units 600-750 MW (Martin Lake 3 x 750 MW; W.A. Parish 3,653 MW total);
  gas combined-cycle blocks 500-1,000 MW; gas peakers 50-200 MW; wind farms 100-300 MW; utility solar
  100-500 MW; batteries 100-200 MW / 200-400 MWh.
- Generator terminal voltage is low: **13.8 kV** (small units) to **18-24 kV** (large units), because
  stator insulation limits voltage; the current is enormous (a 1,200 MW unit at 24 kV: 29 kA per phase).
  A **generator step-up (GSU)** transformer (e.g. 675 MVA, 345/21.9 kV; typical 10-14% Z, X/R 40-50)
  raises it directly to 345 kV or 138 kV. Wind/solar farms collect at 34.5 kV and step up to 138/345 kV.

## 2. Transmission: 345 kV, 138 kV, 69 kV

| Class | Role in Texas | Conductor (typical) | R (ohm/mi) | X (ohm/mi) | B (uS/mi) | Thermal rating | SIL |
|---|---|---|---|---|---|---|---|
| 345 kV | Backbone, CREZ, plant tie-lines | 2 x 795 kcmil ACSR "Drake" bundle | 0.06 | 0.55 | ~8 | ~1,800 MVA (1 ckt) | ~430 MW |
| 138 kV | Regional / sub-transmission | 1 x 795 Drake | 0.13 | 0.72 | ~5.6 | 165-300 MVA (689-1,200 A) | ~50 MW |
| 69 kV | Rural sub-transmission (mostly legacy) | 336 kcmil ACSR "Linnet" | 0.31 | 0.72 | ~5 | 40-80 MVA | ~12 MW |

Sources: Drake R = 0.1288 ohm/mi at 50 C, GMR 0.0373 ft (Chegg/Baylor course data); twin-bundle 345 kV
R = 0.0483, X = 0.327 ohm/km (= 0.078, 0.526 ohm/mi); 345 kV single-circuit twin bundle: SIL 429 MW,
thermal 1,793 MVA (MISO PAC 2023); 138 kV Drake ampacity 689 A at 104 F ambient = 165 MVA (Eng-Tips).
Lines are normally loaded well below thermal rating; long 345 kV lines are limited by **voltage/stability
at roughly 1-2 x SIL**, short lines by conductor temperature.

- Why 345 kV: power capacity scales with V^2 for the same loss/drop; 345 kV was the highest class in
  wide use in the US when Texas built its backbone (1960s-70s). Long, lightly populated distances
  (300+ miles from West Texas wind to DFW/Houston) favoured it. ERCOT is now planning a **765 kV** overlay
  (Texas 765 kV STEP) because 345 kV would need 3,007 miles of new ROW plus 4,274 miles of upgrades by 2030.
- **CREZ** (Competitive Renewable Energy Zones, 2008-2013): ~3,600 circuit-miles of new 345 kV lines,
  final cost **$6.9 billion** (about $2 B over estimate), sized for **18,500 MW** of West Texas/Panhandle
  wind; typical individual lines 100-200 miles, twin- or triple-bundle Drake/ACSS, some with series
  capacitors to shorten electrical length [ETT, Baker Institute, ScienceDirect].
- Voltage drop and reactive power: at transmission X/R ~ 8-10, so the drop is mostly `I * X` and is
  driven by **reactive** current. A lagging load pulls voltage down; a lightly loaded line's shunt C
  (charging) pushes it up (Ferranti effect), which is why shunt reactors are switched in at night.

## 3. Substations

| Transformer | Typical size | %Z | X/R | Notes |
|---|---|---|---|---|
| GSU 18-24 / 345 kV | 300-700 MVA | 10-14 | 40-50 | delta-Y, generator side delta |
| 345/138 kV autotransformer | 300-600 MVA | 6-10 (on own base) | 25-40 | Y-Y auto with delta tertiary (13.8 kV) |
| 138/69 kV | 50-150 MVA | 7-9 | 15-25 | |
| 138/12.47 or 138/24.9 kV distribution | 15-40 MVA (Oncor/CenterPoint commonly 2 x 28 MVA) | 7-9 | 12-20 | LTC +/-10% in 32 steps |
| Pole / pad-mount 7.2 kV / 120-240 V | 10-50 kVA | 1.5-2.5 | 1-2 | center-tapped secondary |

Sources: 345 kV utility transformers 9-12.5% Z, X/R 40-49 (Eng-Tips); 13.8/345 kV GSU specs (OMPA).
VAR support: shunt **capacitor banks** (transmission 50-150 MVAr at 138/345 kV; substation 6-12 MVAr at
12.47 kV; pole-top 300-1,200 kVAr) raise voltage under heavy load; **shunt reactors** (50-100 MVAr) absorb
charging on long 345 kV lines at light load; Oncor also uses SVCs (e.g. Renner SVC). In the simulator a
capacitor bank is just a C across the bus: `C = Q / (376.99 * V_LN^2)` per phase.

## 4. Distribution (Oncor, CenterPoint, AEP Texas, TNMP)

- Oncor: ~3 million meters, ~117,000 miles of T&D lines, ~1,050 substations [DOE filing].
- Feeder voltages: **12.47 kV** (7.2 kV to neutral, "12 kV" in CenterPoint spec), **24.9 kV** (14.4 kV
  L-N) in newer suburban areas, **34.5 kV** (19.9 kV L-N) in AEP Texas rural service and CenterPoint
  "35 kV" primary service. Four-wire multi-grounded-neutral Y.
- Feeder main: 336-795 kcmil ACSR or AAC, 300-600 A, 5-15 MVA; length 3-10 miles urban, 20-40 miles rural.
  336 Linnet: R = 0.306 ohm/mi, X ~ 0.62 ohm/mi at 4 ft spacing; 4/0 ACSR "Penguin" lateral:
  R = 0.59, X = 0.66 ohm/mi. Target drop under ANSI C84.1 range A: 114-126 V at the meter (+/-5%).
- Voltage regulators: 32-step +/-10% single-phase step regulators (or substation LTC) mid-feeder on long
  rural feeders; switched capacitor banks at 2/3 of the way out.
- Service transformers: single-phase 7.2 kV -> 120/240 V, **10/15/25/37.5/50 kVA**, ratio 30:1 with
  center-tapped secondary; commercial three-phase 277/480 V (480/sqrt(3) = 277) pad-mounts 75-2,500 kVA.
- Residential: 200 A, 120/240 V split-phase (48 kW theoretical, ~4-8 kW typical peak with A/C).
  Texas average use **1,096 kWh/month** (EIA 2024), i.e. ~1.5 kW average, national average 855 kWh.

## 5. Worked single-phase-equivalent examples (test oracles)

All at 60 Hz, resistive load unless stated. "V_LN" = phase-to-neutral RMS.

### 5.1 345 kV line, 100 mi, 600 MW load (Example A)
Source 199.19 kV_LN RMS -> **Vpk = 281.7 kV**. Line: R = 6.0 ohm, X = 55 ohm -> **L = 145.9 mH**.
Optional pi-model C: B = 800 uS -> C_total = 2.12 uF (put **1.06 uF at each end**).
Load 200 MW/phase: **R = 198.4 ohm**. Ignoring C:
Z = 204.4 + j55 = 211.7 ohm; **I = 941 A**; **V_load = 186.7 kV (-6.3%)**; **P = 175.7 MW/phase
(527 MW 3-ph)**; line loss 5.3 MW/phase; load lags source by 15.1 deg on the scope.
With the pi C: the receiving-end 1.06 uF supplies ~14 MVAr and lifts V_load to ~188 kV (approximate).

### 5.2 138 kV line, 30 mi, 90 MW load (Example B)
Source 79.67 kV_LN -> **Vpk = 112.7 kV**. R = 3.9 ohm, X = 21.6 ohm -> **L = 57.3 mH**.
Load 30 MW/phase: **R = 211.6 ohm**. Z = 215.5 + j21.6 = 216.6 ohm; **I = 367.9 A**;
**V_load = 77.84 kV (-2.3%)**; **P = 28.64 MW/phase (85.9 MW 3-ph)**.
Reactive-power variant: same 30 MW at pf 0.9 lag = series R = 171.4 ohm + L = 220 mH (X = 83 ohm).
Z = 175.3 + j104.6 = 204.1 ohm; I = 390 A; **V_load = 74.3 kV (-6.7%)** -- nearly 3x the drop for the
same real power. Adding a capacitor bank across the load, **C = 6.1 uF** (14.5 MVAr), restores ~-2.5%.

### 5.3 69 kV line, 15 mi, 30 MW load (Example C)
Source 39.84 kV_LN -> **Vpk = 56.3 kV**. R = 4.59 ohm, X = 10.8 ohm -> **L = 28.6 mH**.
Load 10 MW/phase: **R = 158.7 ohm**. Z = 163.3 + j10.8 = 163.7 ohm; **I = 243.4 A**;
**V_load = 38.63 kV (-3.0%)**; **P = 9.40 MW/phase**.

### 5.4 138/12.47 kV substation + 5 mi feeder, 3 MW (Example D)
Transformer 25 MVA, 8% Z, X/R = 15: ratio **11.07:1** (79.67 kV / 7.2 kV); on the 12.47 kV side
Z_base = 6.22 ohm, Z = 0.498 ohm -> **R_t = 0.033 ohm, L_t = 1.32 mH**.
Feeder 5 mi Linnet: R = 1.53 ohm, X = 3.1 ohm -> **L = 8.22 mH**. Load 1 MW/phase: **R = 51.84 ohm**.
Source 79.67 kV_LN (Vpk 112.7 kV) on the primary, or simply 7.2 kV (Vpk 10.18 kV) at the secondary
ideal node. Z = 53.40 + j3.60 = 53.52 ohm; **I = 134.5 A**; **V_load = 6,973 V (-3.2%)**;
**P = 938 kW/phase**. A 32-step regulator raising the feeder head 3 steps (+1.875%) returns the load
to ~7,100 V; model it as a second ideal transformer with ratio 1:1.01875.

### 5.5 Pole transformer and house (Example E)
25 kVA, 7,200 V -> 120/240 V center-tapped (**ratio 30:1**), 2% Z, X/R 1.5: on 240 V side
Z_base = 2.304 ohm, Z = 0.0461 ohm -> **R_t = 0.0276 ohm, L_t = 97.8 uH**.
House 5 kW at 240 V: **R = 11.52 ohm**. Source 7,200 V RMS (**Vpk 10,182 V**).
Z = 11.548 + j0.037; **I = 20.78 A**; **V_load = 239.4 V (-0.25%)**; **P = 4.975 kW**.
Split-phase variant: 1.5 kW on each 120 V leg (R = 9.6 ohm each, 12.5 A) -> neutral current 0;
change one leg to 3 kW (4.8 ohm, 25 A) -> neutral carries 12.5 A and the light leg rises ~0.3 V
while the heavy leg sags. Open the neutral and watch the 120 V legs split to ~80 V / ~160 V.

### 5.6 Full chain (Example F): 18 kV gen -> GSU -> 345 kV 100 mi -> 345/138 -> 138 kV 30 mi -> 138/12.47 -> 5 mi feeder -> pole xfmr -> 120/240 V house
Components in order (per-phase, all series R + L placed on each transformer's secondary):
1. Generator: 18 kV L-L -> 10.39 kV_LN, **Vpk = 14.70 kV**, 60 Hz, phase 0.
2. GSU 18/345 kV, 600 MVA, 12%, X/R 40: **ratio 1:19.17**; on 345 side R = 0.60 ohm, **L = 63.1 mH**.
3. 345 kV line 100 mi: R = 6.0 ohm, L = 145.9 mH (omit shunt C for the oracle).
4. 345/138 kV auto, 450 MVA, 8%, X/R 30: **ratio 2.5:1**; on 138 side R = 0.113 ohm, **L = 8.98 mH**.
5. 138 kV line 30 mi: R = 3.9 ohm, L = 57.3 mH.
6. 138/12.47 kV, 25 MVA, 8%, X/R 15: **ratio 11.07:1**; R = 0.033 ohm, L = 1.32 mH.
7. Feeder 5 mi: R = 1.53 ohm, L = 8.22 mH.
8. Pole xfmr 7,200/240 V, 25 kVA, 2%, X/R 1.5: **ratio 30:1** (center tap for 120 V); R = 0.0276 ohm,
   L = 97.8 uH.
9. House: R = 11.52 ohm (5 kW).
Oracle (everything referred to the 240 V side; overall ratio 19.17/2.5/11.07/30 -> source = 240.0 V):
referred impedances are 0.0000096 + j0.000114 (GSU + 345 line), 0.0000364 + j0.000227 (auto + 138 line),
0.00174 + j0.0040 (sub + feeder), 0.0276 + j0.0369 (pole xfmr): total **0.0294 + j0.0412 ohm**.
**I_house = 20.78 A, V_house = 239.4 V (-0.25%), P = 4.97 kW**; generator current = 20.78 / 829.9 =
25 mA. Teaching point: with one house the whole grid upstream is invisible -- 94% of the drop is the
pole transformer. Replace the house with the aggregated **1 MW/phase feeder load (R = 51.84 ohm at
7.2 kV)**: total referred to 7.2 kV = 1.604 + j3.903 ohm, **I = 134.4 A, V = 6,965 V (-3.3%),
P = 936 kW/phase**; generator terminal current = 134.4 x (7.2/10.39) = 93 A per phase at 18 kV.
Add the shunt C's from 5.1 and the 6.1 uF bank from 5.2 as follow-on exercises.

Expected intermediate RMS voltages for the 1 MW/phase case (for scope checks): 345 kV bus 199.1 kV
(-0.05%), 138 kV bus 79.6 kV (-0.1%), 12.47 kV bus 7,155 V (-0.6%), load 6,965 V (-3.3%).

## 6. Simplifications and common misconceptions

Simplifications of the single-phase equivalent:
- A balanced three-phase system is solved as **one phase to neutral**; the neutral carries no current, so
  the "return wire" in the simulator is an ideal conductor. Real neutral/ground returns are ignored.
- **sqrt(3) bookkeeping**: nameplate voltages (345, 138, 12.47 kV) are line-to-line; use `V_LL/sqrt(3)`
  for the source, divide three-phase MW/MVA by 3, keep per-phase ohms unchanged (per-phase impedance
  is the same in Y-equivalent). Multiply per-phase P, Q, S by 3 to report three-phase values.
- Transformers are modelled as ideal turns ratio + series R-L; magnetizing current, core loss,
  saturation and delta-Y 30 deg phase shift are omitted. Line shunt C is lumped (pi), not distributed,
  which is fine below ~150 miles.
- Loads are constant impedance; real loads are partly constant-power (motors, electronics), so real
  voltage drops are somewhat larger than the resistive-load oracles.
- Steady-state only: no generator dynamics, frequency stays exactly 60 Hz, no fault studies.

Misconceptions to address in the lessons:
1. "Higher voltage means more dangerous drop." No -- at 345 kV a 100-mile line carrying 500 MW drops
   ~6%; the same power at 138 kV would need 2.5x the current and ~6x the I^2 R loss per ohm.
2. "The source is 345 kV." The per-phase source is 199 kV RMS, and the simulator wants the **peak**,
   281.7 kV. Reading 345 kV on the scope peak is a sqrt(2) x sqrt(3) = 2.45x error.
3. "A 30:1 transformer makes 120 V." It makes 240 V across the full secondary; 120 V is from the
   **center tap** to either end. Both are the same winding.
4. "Voltage drop = I x R." At transmission it is mostly I x X and depends on the load's power factor;
   a unity-pf load barely drops the voltage while a 0.9-lag load of the same MW drops it 3x more.
5. "Capacitors store power for the load." A capacitor bank supplies reactive VARs that cancel inductive
   VARs, reducing the current and thus the I x X drop; it delivers no average real power.
6. "The 60 Hz comes from the outlet." Frequency is set by the rotating generators (or inverters
   following them) across the whole island; ERCOT alone holds it, and a mismatch of load and generation
   moves it -- something the simulator's fixed-frequency source cannot show.
7. "Power flows from the plant to my house through a dedicated wire." Every element is shared; the
   full-chain example shows a single house barely loading a 25 kVA transformer while 600 MW flows past.

## Sources
- ERCOT fact sheet (Dec 2025 CDR): https://www.ercot.com/files/docs/2022/02/08/ERCOT_Fact_Sheet.pdf
- EIA, ERCOT solar/wind/batteries: https://www.eia.gov/todayinenergy/detail.php?id=66464 and id=67685
- ERCOT DC-tie manual: https://www.ercot.com/files/pobs/2017/05/31/Power_Operations_Bulletin_785.doc ;
  ERCOT Grid Insights (DC ties): https://www.ercot.com/files/docs/2025/05/12/ERCOT-Grid-Insights-Electricity-Connection-and-Transfer.pdf
- CREZ: https://www.aep.com/news/releases/read/1338/ETT-Energizes-Last-of-Seven-CREZ-Transmission-Lines-in-West-Texas ;
  https://www.bakerinstitute.org/sites/default/files/2020-11/import/ces-pub-texascrez-111720.pdf ;
  https://www.sciencedirect.com/science/article/abs/pii/S0301421520300100
- 345 vs 765 kV plans: https://www.ercot.com/files/docs/2025/01/28/ERCOT_Trending_Topic_345-kV_vs_765-kV_Transmission.pdf
- Line parameters / SIL: https://cdn.misoenergy.org/20230308%20PAC%20Item%2007%20Supporting%20Materials628087.pdf ;
  https://www.eng-tips.com/threads/acsr-795-mcm-drake-26-7-conductor-rating.498177/ ;
  https://industrialmonitordirect.com/blogs/knowledgebase/calculating-transmission-line-impedance-per-mile-for-795-acsr
- Transformer %Z and X/R: https://www.eng-tips.com/threads/345kv-34-5kv-transformer-impedance-amp-x-r.517900/ ;
  https://www.ompa.com/wp-content/uploads/2025/02/CDLEC-Spare-GSU-Specificatio1.pdf
- Plants: Wikipedia pages for Comanche Peak, South Texas Project, Martin Lake, W.A. Parish
- Oncor system: https://www.energy.gov/sites/prod/files/gcprod/documents/Oncor_Comments_CommsReqs.pdf ;
  CenterPoint 12/35 kV primary service spec: https://www.centerpointenergy.com/en-us/Documents/DistributedGenerationDocs/CenterPoint-Energy-12-kV-and-35-kV-Primary-Service.pdf
- Household use (EIA 2024, 1,096 kWh/mo): https://www.choosetexaspower.org/energy-resources/average-electric-bill-in-texas/
