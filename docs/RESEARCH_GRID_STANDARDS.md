# Reliability standards and simulation methods (batch eight)

Source material: four utility technical reports supplied 2026-08-28 - *ERCOT, AEP and NERC Power
System Reliability, Interconnection and Operational Performance Standards*; *Computational
Methodologies for Bulk Power System Simulation and High-Voltage Grid Analysis*; *Grid-Scale
Frequency Control & Ancillary Services*; and *Substation Physical Security & Supply Chain
Compliance*. This note records what each template takes from them.

## 1. Transmission planning performance (NERC TPL-001-5.1, FERC Form 715)

AEP's ERCOT and SPP Form 715 filings mandate, for every transmission bus above 60 kV:

| Condition | Envelope |
|---|---|
| Pre-contingency, category P0 | 0.95 - 1.05 pu |
| Post-contingency, categories P1 - P7 | 0.92 - 1.05 pu (series-compensated facilities may exceed 1.05) |
| Post-contingency deviation above 100 kV | more than 8 % triggers a mandatory engineering review |
| Thermal | Normal rating pre-contingency, Emergency rating (2 - 4 hours) post-contingency |
| Off-peak maintenance studies | load shedding is **not** an allowed mitigation |

Short-circuit duties are assessed with a flat 1.05 pu profile at all buses.

**Template: N-1 Contingency.** Two 200 mi 345 kV circuits into a 340 MW bus. P0 = 0.970 pu; open the
breaker and P1 = 0.925 pu - below the P0 floor, inside the post-contingency envelope, with a 4.8 %
deviation that stays under the 8 % review threshold. `--std-test` pins the P0 value.

## 2. Extreme temperature (NERC TPL-008-1, PUCT 16 TAC 25.55)

Weather-related outages are up 67 % since 2000. TPL-008-1 (approved December 2024) requires an
Extreme Temperature Assessment at least every five years: benchmark temperature events per weather
zone (R2), cases coupling those temperatures with load growth and equipment derating (R3-R4),
steady-state and stability analysis (R5-R8), and corrective action plans for P0 deficiencies
(R9-R11). PUCT 25.55 adds the seasonal on-site side: weatherization by December 1 each year, sized
to the 95th percentile minimum average 72-hour wind chill, with the ERCOT weather study updated
every five years (next due 1 November 2026), plus SF6 breaker checks, transformer oil quality and
insulation/enclosure measures on the Cold- and Hot-Weather Critical Component lists.

**Template: Extreme Temperature Derating.** A 20 mi 12.47 kV feeder whose conductor carries the real
aluminium coefficient (4030 ppm/degC, `ideal = false`): 6.0 ohm at 25 degC, 7.2 ohm at 75 degC. The
status-bar Tmp slider is the live knob, and a switch adds the summer air-conditioning block, so the
derating and the load growth arrive together exactly as R3-R4 require.

## 3. Inverter-based resource ride-through (NERC PRC-024-3 to PRC-029-1, ERCOT NOGRR-245, IEEE 2800-2022)

From 1 October 2026 the settings-based PRC-024-3 is replaced by the performance-based PRC-029-1: the
question moves from "are the relay settings right?" to "did the plant actually stay on?". ERCOT
NOGRR-245 applies IEEE 2800-2022 at the point of interconnection rather than at inverter terminals:

| RMS voltage (pu) | Minimum ride-through |
|---|---|
| > 1.20 | permissive trip |
| 1.175 - 1.20 | 0.20 s |
| 1.15 - 1.175 | 0.50 s |
| 1.10 - 1.15 | 1.00 s |
| 0.90 - 1.10 | continuous |
| 0.00 - 0.90 | (V + 0.084375)/0.5625 seconds; must inject negative-sequence current |
| 0.00 | 0.15 s |

Active current must return to the pre-disturbance level within 1.0 s of the POI recovering to
0.90 - 1.10 pu, evidenced by IFRTCR and IVRTCR filings.

**Template: IBR Ride-Through.** A 150 ms fault at 100 ms holds the POI near 0.29 pu while the
inverter keeps injecting; open the inverter breaker to see the legacy trip behaviour.

## 4. Facility ratings and AEP BOLD (NERC FAC-008-5)

FAC-008-5 defines a circuit's capacity by its **most limiting element** - conductor, buswork,
breaker, CT, switch or sag clearance. AEP's Breakthrough Overhead Line Design replaces the
conventional three-arm double-circuit tower with a single arch crossarm and a compact triangular
phase arrangement bound by V-string insulators. Compaction raises the line's capacitance and lowers
its inductance, so Zc = sqrt(L/C) falls and SIL = V^2 / Zc rises by up to 60 %; losses fall up to
40 % and the structures are 15 - 20 % lower. Because BOLD carries the transfer naturally it removes
the need for series capacitors, and with them the sub-synchronous resonance risk.

**Templates: Facility Rating** (line 800 kW, breaker 20 kW, CT 4 kW, buswork 25 kW - only the CT
crosses 100 % at 500 A) and **AEP BOLD vs Conventional** (the same 150 mi corridor at 600 MW:
Zc 262 ohm / SIL 454 MW at 0.921 pu, against Zc 162 ohm / SIL 735 MW at 0.989 pu).

## 5. Frequency control (NERC BAL-001-TRE-2, ERCOT ancillary services)

ERCOT is an electrical island tied to its neighbours only by DC, so it must hold its own frequency;
the loss of the two largest units (2750 MW) sets the design contingency. BAL-001-TRE-2 caps governor
deadband at +/-0.034 Hz for mechanical-governor steam and hydro units and +/-0.017 Hz for everything
else, and droop at 5 % (4 % for combined-cycle combustion turbines). Ancillary services: Regulation
(seconds), Responsive Reserve (30 s, with UFRs at 59.7 Hz and battery FFR at 59.85 Hz), ERCOT
Contingency Reserve (10 minutes) and Non-Spinning Reserve (30 minutes). Generator owners must sustain
a rolling Primary Frequency Response performance of at least 0.75.

The report notes that these transfer functions are patched in a circuit simulator from op-amp
integrators and lags - which is exactly what the template does:

| Block | Transfer function | Values used |
|---|---|---|
| Swing equation | (2H/w_s) d2delta/dt2 = Pm - Pe - D dw | H = 4 s, D = 1 pu, f0 = 60 Hz |
| Governor sensing / droop | 1 / (R (1 + s Tg)) | R = 5 %, Tg = 0.1 s |
| Steam chest | 1 / (1 + s Tch) | Tch = 0.3 s |

**Template: Governor Droop & Swing Equation.** Three op-amps, 1 V = 1 Hz and 1 V = 0.1 pu. A 0.05 pu
load step gives a nadir of -0.168 Hz at about 1.2 s and settles at -0.05/(1/R + D) = **-0.143 Hz**
(59.857 Hz), against the ERCOT UFLS first stage at 59.3 Hz and load resources at 59.7 Hz.

## 6. Simulation methods (power flow formulations and network reduction)

* **Kron reduction** eliminates zero-injection buses with the Schur complement
  Y_red = Y_aa - Y_ab Y_bb^-1 Y_ba, preserving Laplacian structure, effective resistance between
  boundary nodes and spectral interlacing - at the cost of hiding branch thermal limits and
  controllable loads (which is what Opti-KRON's MILP formulation restores).
  **Template: Kron Reduction** - a Y network and its delta equivalent driven identically; the two
  load voltages agree to the last digit (91.38 V and 81.59 V on both halves).
* **Fast decoupled power flow** (Stott-Alsac) assumes G << B, small angle differences and near-unity
  magnitudes, giving two constant matrices. It degrades or diverges where R/X >= 1, which is normal
  in sub-transmission and distribution; granular per-bus axis rotation restores convergence.
  **Template: R/X Ratio and Decoupling** - a transmission branch at R/X = 0.09 beside a feeder at
  R/X = 1.5, each with a switchable reactive block, so the cross-coupling is visible.

## 7. Substation physical security (NERC CIP-014-2, Texas LSIPA)

CIP-014-2 followed the 2013 Metcalf attack. R1 criticality covers stations at or above 500 kV, and
200 - 499 kV stations with three or more connected substations. R5 requires a documented
defense-in-depth plan across six layers: deter, detect, delay, assess, communicate, respond. Because
wireless sensors false-alarm in the EMI around energised HV plant, the detect and communicate layers
use passive fibre perimeter systems and zoned fence alarms wired as dry contacts into the substation
RTU. The Texas LSIPA (SB 2013) separately bars Critical Electric Grid Equipment and Services sourced
from designated countries, with annual ERCOT attestations.

**Template: Supervised Alarm Loop.** One twisted pair carries four distinguishable states into the
RTU input - normal 8.5 V, alarm 9.2 V, cable cut 12 V, short 0 V - using a 5.6k end-of-line resistor
and a 2.2k zone resistor, so neither a cut nor a short can be mistaken for "all clear".
