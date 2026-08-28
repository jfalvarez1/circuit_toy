# ERCOT / NERC / AEP standards used by the power-system templates

Every number in the Power systems, High voltage and Residential & commercial templates is sized
against a published criterion rather than picked to look good. `template_smoke --std-test` measures
the steady-state bus voltages and reports each one against the band below, and pins the measured
value so a template cannot drift out of its documented design point unnoticed.

## Steady-state voltage

| Level | Band | Source |
|---|---|---|
| Transmission, system normal (P0) | 0.95 - 1.05 pu | ERCOT Planning Guide Section 4 / Nodal Operating Guide Section 2; NERC TPL-001-5.1 performance table, category P0 |
| Transmission, post-contingency (P1 - P7) | 0.90 - 1.10 pu, no cascading | NERC TPL-001-5.1 |
| Service, 120 V base | 114 - 126 V (Range A) | ANSI C84.1-2020 Table 1 |
| Utilization, 120 V base | 110 - 125 V (Range A) | ANSI C84.1-2020 Table 1 |
| Service, 480 V base | 456 - 504 V (Range A) | ANSI C84.1-2020 |
| Range B (infrequent, limited duration) | 106.7 - 127 V | ANSI C84.1-2020 |

## Voltage drop and flicker

| Rule | Value | Source |
|---|---|---|
| Branch circuit | 3 % informational | NEC (NFPA 70) 210.19(A) informational note |
| Feeder + branch | 5 % informational | NEC 215.2(A) informational note |
| Infrequent motor start | ~3 % dip at the point of common coupling | IEEE 1453 flicker curve; AEP distribution planning practice |
| Distribution regulation | LTC +/-10 % in 32 steps of 0.625 % | AEP substation standard; the Texas Voltage Ladder runs its 69/12.47 kV LTC 8 steps up (+5 %) |

## Protection, reactive power and ride-through

| Standard | What it requires | Where it shows up |
|---|---|---|
| NERC PRC-023 | Relay loadability: no trip below 150 % of the highest emergency rating | 50/51, 21 distance templates |
| NERC PRC-024-3 | Generator voltage / frequency ride-through envelopes | Power Plant, CREZ Wind Collector |
| NERC PRC-005 | Protection system maintenance intervals | P&C templates (documentation only) |
| NERC PRC-006 / ERCOT UFLS | Load shed at 59.3 / 58.9 / 58.5 Hz, 5 / 10 / 10 % | documented in RESEARCH_AEP_PC.md |
| NERC VAR-001, VAR-002 | Voltage schedules and reactive planning | Power Factor Correction, 138 kV VAR line, substation cap banks |
| NERC FAC-008, FAC-011/014 | Facility ratings, SOL / IROL | SIL Loading, 345 kV and 765 kV lines |
| ERCOT Nodal Protocols / interconnection | 0.95 lead-lag power factor at the point of interconnection | CREZ Wind Collector |
| IEEE 1547-2018 | A distributed resource must keep the PCC inside ANSI C84.1 Range A; volt-var and volt-watt above it | Rooftop Solar Backfeed |
| IEEE 2800-2022 | Inverter-based resource performance at the transmission interface | CREZ Wind Collector (context) |
| NEC 700 / 701, NFPA 110 | Emergency transfer in 10 s, legally required standby in 60 s, Type 10 classification | Standby Generator Transfer |
| NEC 220.61 | Feeder neutral sized on the maximum unbalance | 208Y/120 V Panel |
| NEC 210.6(C) | 277 V permitted for luminaires | 480Y/277 V Service |

## Texas voltage levels modelled

| Voltage | Role | Template |
|---|---|---|
| 765 kV | AEP backbone (outside ERCOT) | 765 kV Line |
| 345 kV | ERCOT's highest transmission voltage, CREZ build-out | 345 kV Line, Power Plant, Substation, Texas Voltage Ladder, CREZ Wind Collector |
| 138 kV | AEP Texas transmission | 138 kV VAR Line, Substation, Texas Voltage Ladder |
| 69 kV | AEP Texas subtransmission | 69 kV Subtransmission, Texas Voltage Ladder |
| 34.5 kV | Wind and solar collector systems, rural distribution | CREZ Wind Collector |
| 13.8 kV | Industrial primary | 13.8 kV Industrial Service |
| 12.47Y/7.2 kV | Standard AEP distribution feeder | MV Feeder, Texas Voltage Ladder, 240/120 V Service |
| 4.16 kV | Plant motor bus | 13.8 kV Industrial Service |
| 480Y/277 V | Commercial and industrial utilisation | 480Y/277 V Service, 13.8 kV Industrial Service |
| 208Y/120 V | Commercial panels | 208Y/120 V Panel |
| 240/120 V | Residential split-phase service | 240/120 V Service, branch, AC start, solar, ATS |

## Conductor data used

| Conductor | R (ohm/mi) | X (ohm/mi) | B (uS/mi) | Used for |
|---|---|---|---|---|
| Twin bundle Drake ACSR | 0.06 | 0.55 | 8.0 | 345 kV |
| Six bundle | 0.02 | 0.30 | 14.0 | 765 kV |
| Drake ACSR | 0.13 | 0.72 | 6.0 | 138 kV |
| 336.4 ACSR (Linnet) | 0.306 | 0.75 | 5.4 | 69 kV |
| 1/0 ACSR | 0.30 | 0.65 | - | 12.47 kV feeder |
| 1000 kcmil XLPE | 0.15 | 0.12 | - | 34.5 kV collector cable |
| #14 Cu / #10 Cu | 2.525 / 0.999 ohm per 1000 ft | - | - | branch circuits |
| 4/0 AL triplex | 0.0983 ohm per 1000 ft | - | - | 240 V service drop |

## Documented exceptions

Three buses sit outside their band on purpose; `--std-test` prints them as `[NOTE] ... (documented
exception)` rather than failing:

* **345 kV Line** at 0.937 pu - 600 MW over 100 miles is well past the line's surge impedance
  loading, which is exactly the point of the SIL Loading and Series Compensation templates.
* **Ferranti (open line)** at 1.139 pu - an open-ended 200 mile line rises by design.
* **120 V Branch Circuits**, #14 leg at 0.952 pu - the deliberately undersized conductor that
  demonstrates the NEC 3 % guideline being missed.
