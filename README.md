# Circuit Playground Simulator

**Latest Release: [v3.12.0](https://github.com/jfalvarez1/circuit_toy/releases/tag/v3.12.0)** (auto-updating from v3.4.0 on)

A native desktop circuit simulator written in C with SDL2, featuring a synthwave-themed interface. Build, simulate, and analyze electronic circuits with an intuitive drag-and-drop interface.

In the spirit of [Paul Falstad's circuit.js](https://www.falstad.com/circuit/), which is where a lot
of us first watched current move through a schematic - here as a native app, with a bench
oscilloscope, textbook-parameter device models, and a template library whose every number is
checked against a hand calculation.

![Circuit Playground Screenshot](screenshot.png)

![Circuit Example](gifs/circuit_example.gif)

### Spotlight Search (Ctrl+K)

![Spotlight Search](gifs/spotlight_search.gif)

## Features

### Extensive Component Library

**Sources**
- Ground reference
- DC Voltage Source (adjustable voltage, internal resistance)
- AC Voltage Source (amplitude, frequency, phase, DC offset, frequency sweep)
- DC Current Source (adjustable current)
- Square Wave Generator (frequency, duty cycle, rise/fall time)
- Triangle Wave Generator (frequency, amplitude)
- Sawtooth Wave Generator (frequency, amplitude)
- Noise Source (white noise)
- Arbitrary waveform source (`ARB`) - replays a sample table; load your own x/y coordinate list and the scope draws it in X-Y mode

**Passive Components**
- Resistor (with optional temperature coefficient)
- Capacitor (with ESR, ESL, leakage resistance models)
- Inductor (with DCR, saturation current models)
- Potentiometer (adjustable wiper position 0-100%)
- Photoresistor (light-dependent resistance)
- Thermistor (NTC with beta model)
- Fuse (with current rating, blow state)
- Transformer (primary/secondary turns ratio)

**Diodes**
- Standard Diode (Shockley equation model)
- LED (multiple colors: red, green, blue, yellow, white)
- Zener Diode (adjustable Vz, Rz)
- Schottky Diode (lower forward voltage)
- Photodiode (light-sensitive)
- Varactor (voltage-variable capacitance)

**Transistors - BJT**
- NPN BJT (Ebers-Moll model, adjustable beta, Is, VAF)
- PNP BJT (complementary to NPN)
- Darlington NPN/PNP pairs

**Transistors - FET**
- N-Channel MOSFET (square-law model, Vth, Kn, lambda)
- P-Channel MOSFET (complementary)
- N-Channel JFET (IDSS, VP parameters)
- P-Channel JFET (complementary)
- N-Channel Enhancement MOSFET
- P-Channel Enhancement MOSFET

**Thyristors**
- SCR (Silicon Controlled Rectifier)
- TRIAC (bidirectional thyristor)
- DIAC (trigger diode)

**Op-Amps**
- Ideal Op-Amp (adjustable gain, GBW, slew rate)
- Real Op-Amp model (input/output impedance, rail voltages)
- Rail-to-rail option

**Controlled Sources**
- VCVS (Voltage Controlled Voltage Source)
- VCCS (Voltage Controlled Current Source)
- CCVS (Current Controlled Voltage Source)
- CCCS (Current Controlled Current Source)

**Switches**
- SPST Switch (single pole, single throw)
- SPDT Switch (single pole, double throw)
- Push Button (momentary)
- Relay (with coil voltage and resistance)

**Logic Gates**
- AND, OR, NOT, NAND, NOR, XOR, XNOR gates
- Adjustable logic levels (V_low, V_high, V_threshold)

**Digital**
- 555 Timer IC (astable/monostable modes)
- 7-Segment Display
- Flip-Flops

**Display & Measurement**
- LED indicators (multiple colors)
- Lamp (power/voltage ratings)
- Voltmeter (virtual)
- Ammeter (virtual)

**Regulators**
- 7805 (5V fixed regulator with IN/OUT/GND terminal labels)
- 7812 (12V fixed regulator)
- LM317 (adjustable regulator with IN/OUT/ADJ terminal labels)
- TL431 (programmable shunt reference with K/A/REF terminal labels)

**Subcircuits**
- Pin Marker (mark nodes as subcircuit pins with customizable names)
- IC Block (user-defined subcircuit/integrated circuit)

**Other**
- Text Annotation (with font size, bold, italic, underline options)
- Voltage Probe (connect to oscilloscope)

### Pre-Built Circuit Templates

![Example Circuits](gifs/example_circuits.gif)

161 ready-made circuits live in the **Circuits** tab of the left panel, grouped by topic
(type in the filter box to find one). Every template carries an on-canvas note with the theory,
the governing equation and a **PROBE:** line; loading one places scope probes on its input and
output, presets time/div and V/div, and starts the simulation. Each template also declares a
*demo contract* (`DemoKind`: low-pass, band-pass, envelope, limiter, oscillator, ...) that the
smoke tests enforce, so the example really shows the behaviour it is named after.

**Basics**
- **Voltage Divider** (`Div`) - Resistive voltage divider (1:1)
- **LED + Resistor** (`LED`) - LED with current limiting resistor
- **Series RLC** (`sRLC`) - Series RLC resonant circuit
- **Parallel RLC** (`pRLC`) - Parallel RLC (tank) circuit
- **Wheatstone Bridge** (`Whst`) - Wheatstone bridge measurement circuit
- **Line Drop Basics** (`Drop`) - Battery, wire resistance, load: the simplest voltage drop
- **Thevenin Equivalent** (`Thev`) - Divider + series R seen by a load: Vth 6 V, Rth 2.2 k
- **Superposition** (`Super`) - Two voltage sources + a current source: responses add
- **Lissajous Figures** (`Lissa`) - Two sines into X-Y: the figure counts the frequency ratio
- **X-Y Plotter (upload)** (`XYplt`) - Replay a file of coordinates through two arb sources

**Filters**
- **RC Low Pass** (`LP`) - RC low-pass filter (fc=1.6kHz)
- **RC High Pass** (`HP`) - RC high-pass filter (fc=1.6kHz)
- **RL Low Pass** (`RL-LP`) - RL low-pass filter
- **RL High Pass** (`RL-HP`) - RL high-pass filter
- **Sallen-Key LP** (`S-K`) - 2nd order Sallen-Key low pass filter
- **Active Bandpass** (`BPF`) - Active band pass filter
- **Notch Filter** (`Notc`) - Twin-T 60Hz notch filter
- **RC Band-Pass** (`RC BP`) - Passive RC high-pass into low-pass
- **LC Low-Pass** (`LC LP`) - 2nd-order LC low-pass with load

**Op-amps**
- **Inverting Amp** (`Inv`) - Inverting op-amp (gain=-10)
- **Non-Inv Amp** (`NonI`) - Non-inverting op-amp (gain=11)
- **Voltage Follower** (`Fol`) - Unity gain buffer
- **Integrator** (`Int`) - Op-amp integrator circuit
- **Differentiator** (`Dif`) - Op-amp differentiator circuit
- **Summing Amp** (`Sum`) - Inverting summing amplifier
- **Comparator** (`Cmp`) - Op-amp voltage comparator
- **Difference Amp** (`DifA`) - Op-amp difference amplifier (subtractor)
- **Transimpedance** (`TIA`) - Transimpedance amplifier (I to V)
- **Instr. Amp** (`Inst`) - Three op-amp instrumentation amplifier
- **Window Comp** (`WCmp`) - Window comparator (OV/UV detection)
- **Schmitt Trigger** (`Schm`) - Comparator with hysteresis
- **Peak Detector** (`Peak`) - Op-amp peak detector circuit
- **Op-Amp Saturation** (`Sat`) - Gain -10 clips at the rails; the virtual short is lost

**Transistors**
- **Common Emitter** (`CE`) - BJT common-emitter amplifier
- **Common Source** (`CS`) - MOSFET common-source amplifier
- **Source Follower** (`SF`) - MOSFET source follower (common-drain)
- **Two-Stage Amp** (`2Stg`) - Two-stage BJT amplifier
- **Differential Pair** (`Diff`) - BJT differential amplifier
- **Current Mirror** (`CMir`) - BJT current mirror
- **Push-Pull** (`PP`) - Complementary push-pull output stage
- **Current Source** (`Isrc`) - BJT constant current source
- **Single-Tuned Amplifier** (`Tuned`) - CE stage with an LC tank load: gain peaks at f0 = 100 kHz
- **Common Base** (`CB`) - Non-inverting, low input resistance, gain g_m R_C
- **Darlington Follower** (`Darl`) - beta^2 input resistance: a 100k source still drives 100 ohm
- **MOSFET Transfer Curves** (`IdVgs`) - One gate ramp, three devices: Vth and kn compared
- **MOSFET Output Curves** (`IdVds`) - Drain sweep at three gate voltages: triode to saturation
- **MOSFET Tuned Amplifier** (`MTund`) - Common-source stage with the same 100 kHz LC tank
- **Common Gate (MOSFET)** (`CG`) - Signal into the source, output in phase, low R_in
- **Cascode (MOSFET)** (`Casc`) - CS under CG: high gain, almost no Miller effect
- **MOSFET Differential Pair** (`MDiff`) - Tail resistor sets the current, gates steer it
- **MOSFET Current Mirror** (`MMirr`) - Diode-connected reference copied by a matched device

**Oscillators**
- **Wien Oscillator** (`Wien`) - Wien bridge sine wave oscillator
- **Phase Shift Osc** (`PhOsc`) - RC phase shift oscillator (keep noise on)
- **Relaxation Osc** (`RelOsc`) - Op-amp Schmitt + RC relaxation oscillator
- **Bistable (Schmitt)** (`Schmit`) - Inverting op-amp bistable: +/-7.5 V thresholds, hysteresis loop
- **Triangle/Square Gen** (`TriSq`) - Bistable + integrator: 5 kHz triangle and square
- **Function Generator** (`FuncGn`) - Triangle -> 3-breakpoint diode shaper -> sine; R sets f, thresholds set A
- **Colpitts (MOSFET)** (`Colpit`) - LC tank C1-C2 capacitive divider, 712 kHz
- **Ring Oscillator** (`Ring`) - Five inverters with RC delay stages, ~145 kHz
- **Hartley (MOSFET)** (`Hartly`) - Tapped-inductor tank L1 + L2 with C: 503 kHz
- **Clapp (MOSFET)** (`Clapp`) - Colpitts with a small series cap setting f: 1.744 MHz

**Power supplies**
- **Half-Wave Rect** (`HW`) - Half-wave rectifier
- **Bridge Rectifier** (`Brdg`) - Full-wave bridge rectifier with filter
- **Center-Tap Rect** (`CTap`) - Center-tap transformer rectifier
- **AC-DC Supply** (`ACDC`) - Complete AC to DC power supply
- **US 120V-12V** (`US12`) - American 120V/60Hz to 12V DC
- **Zener Reference** (`Zref`) - Zener diode voltage reference
- **Precision Rect** (`PRec`) - Precision full-wave rectifier
- **7805 Regulator** (`7805`) - 7805 fixed 5V regulator with filtering
- **LM317 Adj Reg** (`317`) - LM317 adjustable regulator with voltage set
- **TL431 Reference** (`431`) - TL431 precision shunt reference
- **Neg Clamper** (`Clmp`) - Negative clamper (DC restorer)
- **Zener Clipper** (`ZClip`) - Back-to-back zeners limit the swing
- **Voltage Doubler** (`Dblr`) - Villard/Greinacher diode-capacitor doubler
- **HW Rect + Cap** (`HW+C`) - Half-wave rectifier with smoothing capacitor

**Digital**
- **CMOS Inverter** (`CMOS`) - CMOS logic inverter
- **SR Latch (NOR)** (`SRlat`) - Cross-coupled NOR gates remember S and R pulses
- **CMOS Inverter (VTC)** (`CMOSi`) - Sweep the gates and read the transfer characteristic
- **CMOS NAND (transistor level)** (`CMOSn`) - PMOS in parallel, NMOS in series
- **Transmission Gate** (`TGate`) - Complementary pair vs a lone NMOS pass transistor

**Power systems (Texas / ERCOT numbers)**
- **345 kV Line** (`345kV`) - 100-mile 345 kV line, 600 MW load (per-phase)
- **138 kV Line + VAR** (`138kV`) - 30-mile 138 kV line, lagging load, switchable cap bank
- **12.47 kV Feeder** (`Feedr`) - 5-mile distribution feeder, 1 MW per phase
- **Pole Xfmr 120/240** (`Pole`) - 7.2 kV to 240 V service transformer with a house load
- **Generator + GSU** (`GenSU`) - 18 kV generator, step-up to 345 kV, 600 MW
- **Grid: 18 kV to 240 V** (`Grid`) - Generator to house through every voltage level
- **Ferranti (open line)** (`Ferr`) - 200-mile 345 kV pi line, open end, switchable reactor
- **Line Model Ladder** (`Ladder`) - Same line as R, R-L and pi: compare the load buses
- **CT + 50/51 Overcurrent** (`50/51`) - CT 600:5, burden, rectify-hold, pickup comparator; pulsed fault
- **87 Line Differential** (`87L`) - Two CTs in opposition: internal fault trips, through fault does not
- **21 Distance Zone 1** (`21Z1`) - Replica impedance vs VT voltage: reach = 80 % of the line
- **50BF Breaker Failure** (`50BF`) - TRIP AND current-present starts a 150 ms timer -> BFT
- **SIL Loading** (`SIL`) - 200 mi 345 kV line at surge impedance load: flat voltage
- **Series Compensation** (`SerC`) - 50 % series capacitor restores the voltage at 2 x SIL
- **765 kV Line (AEP)** (`765kV`) - 300 mi six-bundle EHV line at ~2300 MW
- **3-Phase Y Balanced** (`3phY`) - Three 120-degree sources, Y load: neutral carries nothing
- **3-Phase Unbalanced** (`3phUn`) - Unequal Y loads: neutral current and neutral shift
- **3-Phase 345 kV Line** (`3ph345`) - Three per-phase lines from the 345 kV example
- **3-Phase 6-Pulse Rect** (`6Pulse`) - Three-phase diode bridge: 360 Hz ripple, 1.35 x V_LL
- **Power Plant (3-phase)** (`Plant`) - 3-phase generator, GSU bank, breakers, 345 kV line, load
- **Transmission Substation** (`Substn`) - 345 kV lines, breakers, 345/138 autos, feeders, cap banks
- **69 kV Subtransmission** (`69kV`) - AEP Texas 69 kV: 20 mi 336 ACSR into 20 MVA at 0.95 pf
- **Texas Voltage Ladder** (`TXLad`) - 345 / 138 / 69 / 12.47 kV and the 240 V service in one canvas
- **CREZ Wind Collector** (`Wind`) - 34.5 kV strings -> collector -> 345 kV GSU -> ERCOT grid
- **13.8 kV Industrial Service** (`13k8`) - 13.8/4.16 kV plant transformer, motor bus, 480 V shop

**High voltage**
- **Tesla Coil** (`Tesla`) - Spark-gap Tesla coil, 4x13 in toroid, streamer to a rod
- **Tesla Coil (big top)** (`TeslaB`) - Retuned for an 8x24 in toroid: more energy, longer arc
- **Tesla Coil (detuned)** (`TeslaX`) - Big toroid but the primary was not retuned: weak output

**Transients**
- **RC Step Response** (`RCstp`) - 63 % at one time constant, 10-90 % rise = 2.2 tau
- **RL Step Response** (`RLstp`) - Inductor current rises with tau = L/R
- **RLC Step (Ringing)** (`RLCst`) - Underdamped series RLC: 90 % overshoot, 199 us period
- **RLC Damping Ladder** (`Damp`) - Same L, C with R = 20 / 632 / 2000: under, critical, over

**IC I/O & drivers**
- **Push-Pull Output** (`PPout`) - CMOS totem-pole GPIO: PMOS sources, NMOS sinks, 1 MHz into 20 pF
- **Open-Drain + Pull-up** (`OD`) - Pin only pulls low; 4.7k pull-up makes the slow RC rise
- **Open-Collector Level Shift** (`OC`) - 3.3 V logic drives a 5 V line through an NPN (inverting)
- **I2C Bus (wired-AND)** (`I2C`) - Master and slave open-drain on one SDA, 4.7k / 200 pF
- **I2C Level Shifter** (`I2Clv`) - One NMOS, gate at 3.3 V, pull-ups both sides: 3.3 V <-> 5 V
- **GPIO Input + Debounce** (`Btn`) - Pull-up, button to ground, RC debounce, inverter
- **Low-side Switch + Flyback** (`LoSw`) - NMOS sinks a relay coil; flyback diode clamps the spike
- **High-side PMOS Switch** (`HiSw`) - 3.3 V logic -> NPN -> PMOS gate: load switched from the 12 V rail
- **SPI Lines** (`SPI`) - SCLK 10 MHz / MOSI 5 MHz, 33 ohm series termination, 200 pF cable
- **UART 5 V <-> 3.3 V** (`UART`) - Divider one way, direct the other way (TTL V_IH = 2 V)
- **RS-485 Differential Link** (`RS485`) - A/B antiphase, 120 ohm both ends, common-mode noise rejected
- **SPMI Bus (1.8 V)** (`SPMI`) - MIPI two-wire: 1.8 V SCLK 5 MHz + SDATA, 33 ohm into 15 pF

**Residential & commercial**
- **240/120 V Service** (`Split`) - Centre-tapped pole transformer, unbalanced legs, neutral
- **120 V Branch Circuits** (`Branch`) - #14 vs #10, 100 ft, 12 A: the NEC 3 % voltage drop
- **AC Compressor Start** (`ACstart`) - LRA 104 A on a weak service: a 10 % flicker dip
- **Rooftop Solar Backfeed** (`Solar`) - 7.6 kW export raises the PCC (IEEE 1547 / C84.1)
- **480Y/277 V Service** (`480Y`) - 3-phase 30 hp motor plus 277 V lighting
- **208Y/120 V Panel** (`208Y`) - Unbalanced 20/12/6 A and the shared neutral (NEC 220.61)
- **Power Factor Correction** (`PFC`) - 0.75 -> 0.95 pf with a switched 478 uF bank
- **Standby Generator Transfer** (`ATS`) - Utility drops, the generator picks the load up (NEC 700)

**Grid standards & methods**
- **N-1 Contingency** (`N-1`) - Two 345 kV circuits: open one and watch the P0 envelope break
- **IBR Ride-Through** (`IBR`) - PRC-029-1 / NOGRR-245: a 150 ms fault at the POI
- **AEP BOLD vs Conventional** (`BOLD`) - Compact phasing lowers Zc and raises SIL by 62 %
- **Extreme Temperature Derating** (`Derate`) - TPL-008-1: conductor R rises with the Tmp slider
- **Facility Rating (limiting element)** (`FacRt`) - FAC-008-5: the CT, not the conductor, sets the rating
- **Kron Reduction (Y to delta)** (`Kron`) - Eliminating an interior bus leaves the boundary identical
- **R/X Ratio and Decoupling** (`R/X`) - Why fast decoupled power flow diverges on feeders
- **Governor Droop & Swing Equation** (`Gov`) - BAL-001-TRE-2 frequency nadir on an op-amp patch
- **Supervised Alarm Loop** (`PIDS`) - CIP-014-2: four states on one pair into the RTU

**Hardware engineering**
- **Buck Converter** (`Buck`) - Vout = D Vin: 12 V to 6 V at 100 kHz
- **Boost Converter** (`Boost`) - Vout = Vin/(1-D): 5 V to 10 V
- **Buck-Boost Converter** (`BuckB`) - Inverted output, above or below the input
- **Cuk Converter** (`Cuk`) - Capacitive transfer: both currents continuous
- **Two-Phase Interleaved Buck** (`2Ph`) - 180 deg phases cancel ripple (the CLVR idea)
- **Power Delivery Network** (`PDN`) - Bulk, ceramic and plane inductance against a load step
- **Input vs Output Capacitance** (`Ccomp`) - Same cap, two places, very different result
- **Impedance Matching** (`Zmatch`) - 5 / 50 / 500 ohm on a 50 ohm source
- **Signal Reflections** (`Refl`) - Artificial 50 ohm line, terminated or not
- **Loop Stability & Phase Margin** (`Loop`) - The same stage with and without compensation

**Ideal vs real models**
- **Ideal vs Real Source** (`IdSrc`) - Internal resistance: the terminal voltage sags
- **Ideal vs Real Diode** (`IdDio`) - 0.7 V brick wall against the Shockley knee
- **Ideal vs Real Capacitor** (`IdCap`) - ESR turns the ripple triangle into a square step
- **Ideal vs Real Inductor** (`IdInd`) - Winding resistance damps the ring
- **Ideal vs Real Op-Amp** (`IdOA`) - Gain-bandwidth and slew rate against infinity
- **Ideal vs Real BJT** (`IdBJT`) - The Early effect moves the operating point
- **Ideal vs Real MOSFET** (`IdMOS`) - Channel-length modulation is not a rounding error
- **Op-Amp Error Sources** (`OAerr`) - Offset and bias current at DC, and how to cancel them
- **Named Parts: MOSFET Switches** (`Parts`) - 2N7000 / 2N7002 / IRF540N doing the same job
- **Ceramic DC Bias** (`Cbias`) - The same 10 uF X5R at 0, 2 and 5 V of bias

### Power Systems & High Voltage

![345 kV line](screenshots/auto/line_345kv.png)

The power-system examples use real per-mile conductor data (twin Drake 345 kV, Drake 138 kV,
1/0 ACSR feeders) and Texas/ERCOT sizes researched in `docs/RESEARCH_TEXAS_GRID.md` and
`docs/RESEARCH_AEP_PC.md`: a 100-mile 345 kV line at 600 MW, a 138 kV line with a lagging
load and a switchable capacitor bank, a 12.47 kV feeder, a pole transformer, a generator with
its step-up transformer, the whole chain from an 18 kV generator to a 240 V house, the Ferranti
rise of an open 200-mile line, and a *realism ladder* that shows the same line as R only, R-L,
and a nominal pi section. Each one has a hand-calculated oracle in the probe test.

New components make this possible:

- **Transmission line** (`TLine`) - length in miles times per-mile R, X and B; model 0 = resistance
  only, 1 = series R-L, 2 = nominal pi. All parameters are editable, so line length and conductor
  resistance can be explored live.
- **Ideal transformer** - now reflects current (Ip = -N Is), so generator, line and load currents
  are consistent through every voltage level.
- **Spark gap** - hysteretic arc: breaks down at 3 kV/mm x gap, conducts through `R arc`, quenches
  when the current stays below the hold level; arcs are drawn while it conducts.
- **Toroid** - Tesla-coil topload whose capacitance follows its outer/tube diameter (Bert Pool
  formula); corona streaks appear above ~50 kV.
- **HP load** (`R_HP`) - a resistor drawn as a box with no thermal limit, for loads, line and fault
  resistors that dissipate kilowatts to megawatts; the power-system and Tesla templates use it
  automatically (small sense resistors keep the normal part and its overload warning). Its label
  shows the real dissipation (e.g. `198 Ohm 221 MW`).
- **3-phase source** - a generator / grid block with A, B, C at 0 / -120 / +120 degrees and a common
  neutral, per-phase series R and L (a machine's X''); the Power Plant and Transmission Substation
  templates are built from it.

Every part shows its value on the canvas (ohms, farads, henries, source volts, line miles and
impedance, transformer ratio, 3-phase kV); **F2** toggles the labels.

![Tesla coil](screenshots/auto/tesla_coil.png)

Three **Tesla coil** examples (tuned, retuned for a bigger toroid, and deliberately detuned) use an
NST, a spark gap, the tank capacitor, a k = 0.2 coupled secondary and a streamer gap to a grounded
rod. The scope V/div range now reaches 500 kV/div with kV labels.

### Protection & Control (AEP practice)

![87 line differential](screenshots/auto/relay_differential.png)

Self-running relay examples built from the same parts (the fault is applied by a pulse-driven analog
switch, so one scope screen shows pre-fault, fault and the relay decision):

- **CT + 50/51 Overcurrent** - 600:5 CT into a 1 ohm burden, diode peak-hold, comparator pickup at 738 A.
- **87 Line Differential** - two CTs in opposition across a differential burden: an internal fault trips,
  a through fault beyond the second CT does not.
- **21 Distance Zone 1** - replica impedance (|I| x Z_set) against the VT voltage; a fault at 40 % of the
  line trips, one at 100 % is left to zone 2.
- **50BF Breaker Failure** - TRIP AND current-present starts a 150 ms timer; a stuck breaker produces BFT.
- **SIL Loading**, **Series Compensation**, **765 kV Line** - surge-impedance loading (flat profile vs 20 %
  sag at 2 x SIL), a 50 % series capacitor restoring the far end, and AEP's 765 kV six-bundle backbone.

The design numbers and the utility context (redundant A/B relaying, 87L over fiber, DNP3 / IEC 61850,
UFLS stages, St. Clair loadability) are collected in `docs/RESEARCH_AEP_PC.md`.

### Advanced Oscilloscope

![Pop-out Scope](gifs/pop_out_scope.gif)

Full-featured virtual oscilloscope with:
- **8 Channels** - Connect multiple voltage probes
- **Adjustable Scales** - Time/div (10 ns to 100 s), Volts/div (1mV to 100V)
- **Time step follows the scope** - changing time/div re-maps the simulation step to ~50 samples per division (never coarser than the signal-accuracy step); the dt +/- buttons still override until the next time/div change
- **Stacked view** - `Stack` button gives every channel its own band with its own zero line and CHn tag, so identical signals can be told apart; toggles back to overlay
- **Time-Based Display** - History decimation follows the visible time window (20 x time/div is kept), so the trace is smooth at every time/div setting
- **Voltage Scale Labels** - Y-axis shows voltage marks dynamically based on V/div setting
- **Trigger System** (Tektronix-style)
  - Auto, Normal, Single-shot modes
  - Rising, Falling, Both edge triggers
  - Backward edge search for stable display
  - Trigger-centered data capture
  - Adjustable trigger level
  - Per-channel trigger source selection
- **Display Modes**
  - Y-T (voltage vs time)
  - X-Y (Lissajous patterns)
- **Measurement Cursors** (Tektronix-style) - `CUR` cycles Off → Waveform → Screen. Waveform cursors a/b ride the source (trigger-channel) trace and read t, V, Δt, 1/Δt, ΔV and dV/dt, plus gated Vpp / mean / RMS between the cursors; Screen cursors add independent horizontal amplitude bars. Drag any bar; the active cursor is marked `*`
- **Waveform Measurements** - Vpp, Vrms, Vavg, frequency, period, duty cycle
- **FFT Analysis** - Frequency spectrum view with THD and SNR
- **Math Channels** - Combine probe signals with operations:
  - Add (A + B), Subtract (A - B), Multiply (A × B), Divide (A / B)
  - Derivative (dA/dt), Integral (∫A dt)
  - Absolute value, Invert, Log, Square root
- **Persistence Mode** - Phosphor-like trace decay for visualizing signal variations
- **Autoset** - Automatic scale adjustment
- **Screenshot** - Save oscilloscope display as BMP
- **Pop-out Window** - Detach the oscilloscope into its own resizable window, laid out as a bench instrument with working knobs (see below)

### Device Models You Can Actually Edit

![Ideal vs real MOSFET](screenshots/auto/id_mosfet.png)

Every part carries the parameters a textbook gives you, and an **Ideal** switch that says exactly
what is being left out:

- **MOSFET** - type `u*Cox`, `W`, `L`, `W/L`, `Kn = u*Cox(W/L)`, `lambda` or `tox` and each field
  back-solves the others, so a problem set's numbers go straight in whichever form it states them.
  Below them is the live operating point: region (cutoff / triode / saturation), Vgs, Vds, Id, gm
  and the overdrive Vov. `Type:` flips between enhancement and depletion.
- **BJT** - beta, Is, the forward Early voltage VAF, emission coefficients and leakage currents;
  ideal mode is Ebers-Moll transport with no Early effect.
- **Capacitor** - ESR, ESL and leakage resistance, all in the solve: ESR puts the current square
  straight onto the ripple, ESL sharpens the edges, leakage bleeds the charge away.
- **Inductor** - winding resistance (DCR), saturation current.
- **Source** - internal series resistance, so the terminal voltage sags with the load.
- **Diode** - Shockley by default; ideal mode is the switch-behind-a-0.7 V-battery model.
- **Op-amp** - open-loop gain, gain-bandwidth product, slew rate and rails. The GBW pole and the
  slew limit are imposed inside the solve, not clamped afterwards, so the node voltages and the
  branch currents stay consistent with each other.

The **Ideal vs real models** group builds each of those into a side-by-side circuit: the same
schematic two or three times with one part swapped, both on the scope at once. Every value in the
notes is a hand calculation that the regression suite checks - both halves of every comparison.

### Named Parts

![Named parts](screenshots/auto/named_parts.png)

A schematic says 2N7000, not "an NMOS with V_th = 2.1 V". Twenty real devices ship with their
data sheet parameters, and picking one from the **Part** row in the properties panel loads the
model and labels the symbol on the canvas:

| | |
|---|---|
| **MOSFETs** | 2N7000, 2N7002, IRF540N, BS250 |
| **BJTs** | 2N3904, BC547B, 2N3906 |
| **Diodes** | 1N4148, 1N4001, 1N4733A (5.1 V zener) |
| **Capacitors** | X5R 10uF, C0G 10nF, Alu 100uF |
| **Op-amps** | LM358, LM741, TL072, MCP6001 |
| **Regulators** | LM317, LM7805, TL431 |

`template_smoke --part-test` rebuilds each device's own data sheet test condition and checks the
model reproduces the number - R_DS(on) at the stated V_GS, I_D(on) off the transfer curve, h_FE
and V_BE at a forced base current, V_F at a forced forward current, V_Z at I_ZT, an op-amp's
offset out of a unity buffer and its slew rate off a 5 V step, each regulator in its reference
circuit. All 23 checks pass, and the panel shows the data sheet line each parameter came from.

### The Pop-Out Bench Scope

![Pop-out bench scope](screenshots/auto/scope_panel.png)

`PopOut` detaches the oscilloscope into its own 1120 x 700 window laid out like a real
instrument: the graticule sits in a recessed bezel with the whole width of the window, and a
front panel runs down the right with six **working knobs**. Drag one up or down:

| Knob | What it does |
|------|--------------|
| **VOLTS/DIV** | detented through the 1-2-5 sequence, 1 mV to 500 kV per division |
| **TIME/DIV** | detented, 1 ns to 100 s per division (the simulation step follows it) |
| **POSITION** | moves the selected channel up and down the screen |
| **TRIG LEVEL** | the trigger threshold, in volts |
| **INTENSITY** | screen brightness, the same setting as the status bar's `Brt` |
| **CHANNEL** | which channel the position knob and the trigger follow |

Each pointer sits where the value is on its travel, and the value is printed under the knob.
Below them a status plate shows the settings that are switches rather than knobs - trigger mode
and edge, Y-T or X-Y, AC or DC, stacked or overlay, and RUN / HOLD with the channel count.

The knobs are not a second copy of the state: they move the same variables the docked buttons
do, so the two views can never disagree. `--layout-test` checks every knob is inside the panel,
hit-tests at its own centre, does not overlap its neighbours, is inert while the scope is
docked, and actually moves its value in the right direction.

![Pop-out scope in X-Y mode](screenshots/auto/scope_panel_xy.png)

### Bode Plot Analysis

![Bode Plot](gifs/bode_plot.gif)

Frequency response analysis tool:
- Configurable frequency range (1Hz to 1GHz)
- Magnitude plot (dB)
- Phase plot (degrees)
- Cursor for precise measurements
- Automatic frequency sweep

### Monte Carlo Analysis

Statistical tolerance analysis for worst-case design:
- Run up to 1000 iterations with randomized component values
- Gaussian distribution based on component tolerances
- Statistical results: mean, standard deviation, min/max
- 1% and 99% percentile calculations
- Visualize output variation due to component tolerances
- Trigger from "MC" button in oscilloscope toolbar

### Environment Controls

Simulate real-world environmental conditions:
- **Light Level** - Adjust ambient light (0-100%) for photoresistors and photodiodes
- **Temperature** - Adjust ambient temperature (-40°C to +125°C) for temperature-sensitive components
- Sliders in status bar for quick adjustment

### Current Flow Visualization

- **Animated particles** - Cyan dots flow along wires and through components; size, speed and direction come from the exact solver currents (per-terminal currents plus a per-net wire flow solve), so a series path shows identical dots on wires, capacitors and resistors alike
- **Conventional current** - Particles follow conventional current flow (positive to negative)
- **BFS path tracing** - Current flows from voltage/current source positive terminals to ground nodes
- **Series wire uniformity** - All wires in series carry the same current; parallel branches split by their real currents; ground symbols absorb the return
- **Ground path completion** - Current properly flows to ground terminal wires
- **Voltage source handling** - Correct current direction on wires connected to voltage sources
- **Dual source handling** - When two voltage sources meet, current flows toward lower voltage
- **Speed scaling** - Particle speed scales logarithmically with current magnitude
- **Size indication** - Larger currents show larger particles

### Subcircuit Creation (Ctrl+G)

Create reusable subcircuits from your designs:

1. **Place Pin Markers** - Add Pin markers from the palette to nodes that will become subcircuit pins
2. **Set Pin Names** - Configure pin names (e.g., "VCC", "IN", "OUT", "GND") in the properties panel
3. **Select Components** - Select all components including Pin markers
4. **Open Dialog** - Press Ctrl+G to open the subcircuit creation dialog
5. **Auto-Detection** - Pin markers are automatically detected and their names populated in the dialog

### Simulation Engine

- **Modified Nodal Analysis (MNA)** for accurate DC and transient simulation
- **Newton-Raphson iteration** for nonlinear component convergence
- **Backward Euler integration** for capacitors and inductors
- **Adaptive time step** - Automatically adjusts for frequency (50-300 samples/period)
- Adjustable simulation speed (0.1x to 100x real-time)
- Step-by-step simulation mode
- Auto-adjusting time step for high-frequency accuracy

### User Interface

![Collapsible Sections](gifs/collapsible_sections.gif)

- **Synthwave color theme** - Neon pink, cyan, and purple accents
- **Animated neon border** - Dual smooth chasers flow around window edges with thick glowing bars
- **Spotlight Search (Ctrl+K)** - Quick component search with fuzzy matching, keyboard navigation, and mouse click selection
- **Tabbed left panel** - *Parts* and *Circuits* tabs with a type-to-filter box; parts keep their collapsible categories, circuits are grouped by topic
- **Compact scope controls** - one primary row (V/T scale, Autoset, cursors, stacked view, sweep tracking, pop-out) plus Display / Trigger / Analysis tabs; the same layout drives the pop-out window
- **Collapsible properties panel** - click its header to give the room to the scope
- **Hover tooltips** on every button and palette item
- **Search anywhere** - Ctrl+K or Ctrl+Space opens Spotlight, `/` jumps into the left-panel filter box; both understand plain words (`mosfet`, `coil`, `transistor`, `battery`, `power line`) as well as part names; F2 toggles the on-canvas value labels
- **Scope AC / Fit** - Display tab: `AC` draws each trace minus its DC level, `Fit` (stacked view) gives every band its own V/div centred on its mean, shown in the band tag; the amplifier templates preset Stack + Fit, and stacked logic signals sit one division above the band bottom
- **Brightness** - `Brt` slider in the status bar (25-100 %) or F3 / F4; dims the whole window including the popped-out scope
- **Remembered between runs** - window size, brightness, Parts/Circuits tab, value labels / voltages / current-flow / grid toggles, collapsed properties panel, speed, Lux and Tmp sliders are saved to `%APPDATA%\circuit_toy\circuit-playground\settings.json` on exit and restored at launch (independent of the exe folder, so updates never reset them; scripted `--shot`/`--record` runs never write it)
- **Collapsible palette categories** - Click to expand/collapse component groups
- **Grid-based placement** with snap-to-grid (toggle with 'S')
- **Pan and zoom** - Middle mouse or Shift+drag to pan, scroll to zoom
- **Component rotation** - Double-click or 'R' key (0°, 90°, 180°, 270°)
- **Wire routing** - Click to start wire, click again to route
- **Property editor** - Right panel for adjusting component values
- **Engineering notation** - Supports k, M, G, m, µ, n, p suffixes
- **Resizable panels** - Drag the scope's top edge (taller, may cover the properties list) or left edge (wider, out over the canvas up to the palette); the properties panel edge resizes too
- **Scrollable scope controls** - Mouse wheel scrolls oscilloscope buttons when window is small
- **Status bar** - Shows simulation time, voltmeter/ammeter readings, Lux/Temp sliders, node count, component count
- **Live measurements** - Voltmeter (VM) and Ammeter (AM) readings displayed in status bar

### Auto-Update (Windows)

On start-up the app asks GitHub for the latest release in a background thread (PowerShell
`Invoke-RestMethod`, nothing new is linked). If a newer tag exists an **Update** button appears at
the right of the toolbar: it downloads `circuit-playground-windows-vX.Y.Z.zip`, waits for the app to
close, extracts it over the install folder and relaunches. `--no-update-check` (or
`CIRCUIT_TOY_NO_UPDATE=1`) disables the check; `--update-check` / `--update-now` do it from the
command line. Releases are built with `pwsh tools/make_release.ps1 -Publish` (version from
`include/version.h`).

### File Operations

- **Save/Load circuits** - Binary format (.ckt)
- **Auto-save** - Periodic backup during work
- **Circuit templates** - Pre-built example circuits

## Gallery

| | |
|---|---|
| ![RC low-pass sweep](screenshots/auto/rc_lowpass_sweep.png) RC low-pass with a 100 Hz-20 kHz sweep and tracking scope | ![Wien oscillator](screenshots/auto/wien_oscillator.png) Wien bridge oscillator |
| ![Grid chain](screenshots/auto/grid_chain.png) 18 kV generator to a 240 V house | ![Ferranti](screenshots/auto/ferranti.png) Ferranti rise on an open 200-mile line |
| ![Line model ladder](screenshots/auto/line_model_ladder.png) The same line as R, R-L and pi | ![Relaxation oscillator](screenshots/auto/relaxation_osc.png) Op-amp relaxation oscillator |
| ![50/51 overcurrent](screenshots/auto/relay_overcurrent.png) CT + 50/51 overcurrent relay | ![21 distance](screenshots/auto/relay_distance.png) 21 distance zone 1 reach |
| ![50BF](screenshots/auto/breaker_failure.png) 50BF breaker-failure timer | ![765 kV](screenshots/auto/line_765kv.png) AEP 765 kV line at SIL |
| ![Three-phase](screenshots/auto/three_phase_balanced.png) Balanced three-phase Y (A, B, C, neutral) | ![6-pulse](screenshots/auto/six_pulse_rectifier.png) Three-phase six-pulse rectifier |
| ![Triangle/square](screenshots/auto/triangle_square_gen.png) Bistable + integrator triangle/square generator | ![Function generator](screenshots/auto/function_generator.png) Triangle-to-sine diode shaper (function generator) |
| ![Colpitts](screenshots/auto/colpitts.png) MOSFET Colpitts at 712 kHz | ![Ring](screenshots/auto/ring_oscillator.png) Five-inverter ring oscillator |
| ![Hartley](screenshots/auto/hartley.png) Hartley (tapped inductor) | ![RLC ringing](screenshots/auto/rlc_ringing.png) Series RLC step: 90 % overshoot, 199 us ring |
| ![Damping ladder](screenshots/auto/damping_ladder.png) Under / critical / over-damped on one screen | ![Op-amp saturation](screenshots/auto/opamp_saturation.png) Clipping at the rails and the lost virtual ground |
| ![Power plant](screenshots/auto/power_plant.png) Three-phase power plant: generator, GSU bank, breakers, lines | ![Substation](screenshots/auto/substation.png) Transmission substation: 345/138 kV autos, feeders, cap banks |

### Reliability Standards & Simulation Methods

![Governor droop](screenshots/auto/governor.png)

Nine templates built directly from utility technical reports, with the citations and numbers in
`docs/RESEARCH_GRID_STANDARDS.md`:

- **N-1 Contingency** - NERC TPL-001-5.1: two 345 kV circuits at 0.970 pu; open one breaker and the
  bus goes to 0.925 pu, below the P0 floor but inside the 0.92-1.05 post-contingency envelope.
- **IBR Ride-Through** - PRC-029-1 / ERCOT NOGRR-245 / IEEE 2800: a 150 ms fault holds the point of
  interconnection near 0.29 pu while the inverter keeps injecting. Open its breaker for the legacy trip.
- **AEP BOLD vs Conventional** - the same 150 mi corridor at 600 MW built twice. Compact phasing takes
  Zc from 262 to 162 ohm, SIL from 454 to 735 MW (+62 %), and the bus from 0.921 to 0.989 pu - and with
  no series capacitors there is no sub-synchronous resonance to worry about.
- **Extreme Temperature Derating** - TPL-008-1 / PUCT 25.55: a real 4030 ppm/degC aluminium conductor
  driven by the status-bar **Tmp slider**, plus a switchable summer air-conditioning block.
- **Facility Rating** - FAC-008-5: four elements in one path; at 500 A only the CT crosses 100 %, so it
  sets the rating no matter how strong the conductor is.
- **Kron Reduction** - a Y network and its delta equivalent driven identically; the boundary voltages
  match to the last digit (91.38 V and 81.59 V on both halves).
- **R/X Ratio and Decoupling** - a transmission branch at R/X = 0.09 beside a feeder at R/X = 1.5:
  the cross-coupling that makes fast decoupled power flow diverge below 100 kV.
- **Governor Droop & Swing Equation** - BAL-001-TRE-2 patched as an op-amp analog computer (1 V = 1 Hz).
  A 0.05 pu load step gives a -0.168 Hz nadir and settles at -0.143 Hz, the analytic -0.05/(1/R + D).
- **Supervised Alarm Loop** - CIP-014-2: one twisted pair into the substation RTU carrying four
  distinguishable states, so a cut or a short cannot look like "all clear".

| | |
|---|---|
| ![N-1 contingency](screenshots/auto/n1_contingency.png) Open a breaker and watch the bus leave the P0 envelope | ![AEP BOLD](screenshots/auto/bold_line.png) BOLD against a conventional 345 kV line, same corridor and load |
| ![Kron reduction](screenshots/auto/kron.png) Y and delta halves overlay exactly | ![Supervised loop](screenshots/auto/pids_loop.png) Four states on one pair into the RTU |

### Texas Voltages, Residential & Commercial

![Texas voltage ladder](screenshots/auto/tx_ladder.png)

Every voltage a Texas electron passes through has a template, and every number is sized against a
published criterion rather than picked to look good - the full table of standards, conductor data and
documented exceptions is in `docs/RESEARCH_ERCOT_STANDARDS.md`:

- **765 kV** (AEP backbone, outside ERCOT) - **345 kV** (ERCOT's highest transmission voltage, the CREZ
  build-out) - **138 kV** - **69 kV** subtransmission - **34.5 kV** wind collector - **13.8 kV**
  industrial primary - **12.47Y/7.2 kV** distribution - **4.16 kV** plant motor bus - **480Y/277 V** and
  **208Y/120 V** commercial - **240/120 V** residential.
- The **Texas Voltage Ladder** puts 345 / 138 / 69 / 12.47 kV and the 240 V service on one canvas with a
  tap load at each level, and the scope shows all five at once in Stack + Fit (each band with its own
  V/div in the tag). Its 69/12.47 kV transformer runs its LTC 8 steps up (+5 %); set it back to neutral
  and the house drops to 112 V, below ANSI C84.1 Range A - which is exactly why LTCs exist.
- **Residential & commercial**: the centre-tapped 240/120 V service (and the open-neutral failure), the
  NEC 3 % branch-circuit rule on #14 vs #10, AC compressor start flicker, rooftop solar raising the point
  of common coupling, 480Y/277 V and 208Y/120 V panels, power factor correction with a switched capacitor
  bank, and an NEC 700 open-transition generator transfer.

| | |
|---|---|
| ![240/120 V service](screenshots/auto/res_service.png) The centre-tapped pole transformer: L1 and L2 180 degrees apart | ![AC compressor start](screenshots/auto/ac_start.png) Locked-rotor current sags the panel 7 % - motor-start flicker |
| ![CREZ wind collector](screenshots/auto/wind_collector.png) 34.5 kV strings into a 345 kV POI, and the collector voltage rise | ![Power factor correction](screenshots/auto/pfc.png) A shunt in the supply return: 123 A falls to 95 A when the bank closes |

`template_smoke --std-test` measures 19 steady-state buses and reports each against its band (ERCOT
Planning Guide 4 / NERC TPL-001-5.1 P0 at 0.95-1.05 pu, ANSI C84.1 Range A at 114-126 V, NEC 210.19(A)
at 3 % on a branch), and pins the measured value so nothing drifts off its design point unnoticed.

### IC I/O & drivers

What a GPIO pin is made of and how it talks to the outside world - twelve templates in the **IC I/O & drivers** group:
push-pull (CMOS) output, open-drain + pull-up, open-collector level shift, the I2C wired-AND bus and the NXP one-MOSFET
level shifter, a debounced pull-up input, low-side (flyback) and high-side (PMOS) load switches, SPI series termination,
UART between 5 V and 3.3 V parts, an RS-485 differential link with common-mode noise, and the 1.8 V SPMI bus. Each one
probes every signal that matters (Stack view) and its text says what to change to break it.

| | |
|---|---|
| ![I2C bus](screenshots/auto/i2c_bus.png) I2C SDA: master and slave open-drain, the line is LOW when either pulls | ![RS-485](screenshots/auto/rs485.png) RS-485: A/B carry the data plus 1 V of common-mode noise, the receiver sees only A-B |
| ![High-side switch](screenshots/auto/high_side.png) High-side PMOS switch driven from 3.3 V through an NPN | ![GPIO input](screenshots/auto/gpio_input.png) Pull-up input, button, RC debounce, inverter |
| ![Two-stage amp](screenshots/auto/two_stage_fit.png) Scope **Fit**: 10 mV input and 130 mV output on 6 V DC, each band on its own scale | ![SPI](screenshots/auto/spi.png) SPI at 10 MHz through 33 ohm into 200 pF of cable |
| ![Single-tuned amplifier](screenshots/auto/single_tuned_amp.png) Single-tuned (LC collector load) amplifier | ![SR latch](screenshots/auto/sr_latch.png) SR latch from cross-coupled NOR gates |

### Ideal vs real models

| | |
|---|---|
| ![Ideal vs real capacitor](screenshots/auto/id_cap.png) ESR turns the ripple triangle into a square step | ![Ideal vs real op-amp](screenshots/auto/id_opamp.png) Gain-bandwidth, then slew rate: the sine leaves as a triangle |
| ![Ideal vs real inductor](screenshots/auto/id_ind.png) Winding resistance takes the ring from zeta 0.05 to 0.30 | ![Ideal vs real diode](screenshots/auto/id_diode.png) The 0.7 V brick wall against the Shockley knee |


![Function generator](gifs/auto_function_generator.gif)

![Three-phase](gifs/auto_three_phase_balanced.gif)

![RC sweep](gifs/auto_rc_lowpass_sweep.gif)

![Tesla coil](gifs/auto_tesla_coil.gif)

![Spotlight: type "mosfet", Enter places the NMOS](gifs/auto_spotlight_search.gif)

![87 line differential](gifs/auto_relay_differential.gif)

![50BF breaker failure](gifs/auto_breaker_failure.gif)

## Building

### Requirements

- Meson build system (0.60+)
- Ninja build tool
- SDL2 library
- C11 compatible compiler (GCC, Clang, or MSVC)

### Windows (with Meson)

```bash
# Clone the repository
git clone https://github.com/yourusername/circuit-playground.git
cd circuit-playground

# Setup build directory (SDL2 is fetched automatically)
meson setup build

# Build
meson compile -C build

# Run
./build/circuit-playground.exe
```

### Linux

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install meson ninja-build libsdl2-dev

# Setup and build
meson setup build
meson compile -C build

# Run
./build/circuit-playground
```

### macOS

```bash
# Install dependencies
brew install meson ninja sdl2

# Setup and build
meson setup build
meson compile -C build

# Run
./build/circuit-playground
```

## Testing

`tools/template_smoke.c` is a headless regression check built alongside the app
(`build/tools/template_smoke.exe`). It places every prebuilt circuit, runs the DC operating
point and a short transient, and reports solver errors, NaN/runaway voltages and the bias
point of every transistor / op-amp / regulator:

```bash
build/tools/template_smoke.exe             # 161/161 templates passed
build/tools/template_smoke.exe --verbose   # + bias voltages per active device
build/tools/template_smoke.exe --nodes "Wien"   # + node -> matrix mapping for one template
build/tools/template_smoke.exe --probe-test      # output node of every template vs hand calculation (159 oracles)
build/tools/template_smoke.exe --knob-test       # every template still converges with every value x0.5 and x2
build/tools/template_smoke.exe --trace "87 " 0.3 # per-node min/max over a run (debugging a template)
build/tools/template_smoke.exe --demo-test       # every template demonstrates its DemoKind contract
build/tools/template_smoke.exe --osc-test        # oscillators really oscillate (add --osc-dt 5e-6)
build/tools/template_smoke.exe --tesla-test      # spark-gap firings, ring frequency, toroid peak, streamer, tuned vs detuned
build/tools/template_smoke.exe --param-test      # spark gap / toroid / line / transformer limits vs phasor oracles; scope presets
build/tools/template_smoke.exe --flow-test       # current-flow display: KCL, conservation, series uniformity
build/tools/template_smoke.exe --burn-test       # no resistor/LED over its rating (HV templates use R_HP loads)
build/tools/template_smoke.exe --std-test        # bus voltages vs ERCOT / NERC / ANSI C84.1 / NEC limits
build/tools/template_smoke.exe --switch-test     # every switch in both states, measured at the probed output
build/tools/template_smoke.exe --part-test       # every named device at its data sheet's own test condition
build/tools/template_smoke.exe --op-test         # the operating point the properties panel shows, per device
build/tools/template_smoke.exe --param-test      # scope presets: the window really shows the circuit's own frequency
build/circuit-playground.exe --keys "^mosfet|" 24 8 --record DIR N EVERY   # scripted typing: ^ opens Spotlight, | is Enter
build/tools/template_smoke.exe --geom-test       # schematic audit: diagonals, crossings, wires through bodies
build/tools/template_smoke.exe --scope-test      # scope time/div <-> dt mapping
build/tools/template_smoke.exe --response "RC BP"   # amplitude vs frequency of every node during the sweep
build/tools/template_smoke.exe --svg screenshots/templates   # export every template as SVG
build/circuit-playground.exe --layout-test       # headless UI layout check (no overlaps, every template in the palette)
```

The app itself has automation flags for reproducible screenshots (used by `tools/make_media.py`,
which produced the images in this README):

```bash
build/circuit-playground.exe --template Tesla --size 1400x900 --shot out.bmp --frame 300 --exit
build/circuit-playground.exe --template LP --record frames 48 3 --exit    # 48 frames, one every 3
build/circuit-playground.exe --help
```

Every example circuit carries an on-canvas note explaining how it works, the governing
equation, and a **PROBE:** line saying where to put the scope probes and what to expect.
Loading a template also places probes on its input and output, presets the scope time/div
and starts the simulation, so the scope shows the circuit working immediately. The filter
examples (RC/RL, Sallen-Key, band-pass, notch) drive a **frequency-sweeping** source and the
peak detector / clamper an **amplitude-sweeping** one; a live `f=` / `A=` readout under the
source shows the instantaneous value.

`TEST_PLAN.md` (feature-by-feature manual plan) and `TEMPLATE_AUDIT.md` (per-template
hand-calculated expectations and value variations) track the interactive test campaign;
`docs/UI_DECLUTTER_PLAN.md` records the UI restructuring.

## Usage Guide

### Getting Started

1. **Launch the application** - The main window shows a grid canvas with component palette on the left, oscilloscope on the right.

2. **Place components** - Click a component in the left palette, then click on the canvas to place it. Press 'R' to rotate before placing.

3. **Connect with wires** - Select the Wire tool (or press 'W'), click on a component terminal, then click on another terminal to connect them.

4. **Set component values** - Click on a placed component to select it. Use the Properties panel on the right to adjust values (resistance, voltage, frequency, etc.).

5. **Run simulation** - Click the "Run" button in the toolbar or press Space. The oscilloscope will show voltage waveforms from any connected probes.

### Working with the Oscilloscope

1. **Add probes** - Select the Probe tool from the palette and click on a node to measure its voltage.

2. **Adjust scales** - Use the V/div and Time/div buttons, or click on the scale values to adjust manually.

3. **Set trigger** - Choose trigger mode (Auto/Normal/Single), edge (Rising/Falling), and level. Click on a channel to set it as trigger source.

4. **Use cursors** - Enable cursor mode to measure time intervals and voltage differences.

5. **FFT mode** - Toggle FFT to see frequency spectrum of the signal.

### Running Bode Analysis

1. Add an AC voltage source as input and a probe at the output.
2. Click the "Bode" button to open the Bode plot panel.
3. Set frequency range and click "Recalc" to run the analysis.
4. View magnitude and phase response across frequency.

### Tips

- Use **engineering notation** when entering values: 1k = 1000, 4.7u = 4.7µ, 10n = 10 nano
- **Double-click** a component to rotate it
- Press **F1** to see keyboard shortcuts
- **Middle-click and drag** to pan the canvas
- **Scroll wheel** to zoom in/out
- Click category headers in the palette to **collapse/expand** sections

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Escape | Cancel current action, return to select tool |
| Delete/Backspace | Delete selected component |
| R | Rotate component (while placing or selected) |
| W | Switch to Wire tool |
| G | Toggle grid visibility |
| S | Toggle snap-to-grid |
| Ctrl+C | Copy selected component |
| Ctrl+X | Cut selected component |
| Ctrl+V | Paste component |
| Ctrl+D | Duplicate selected component |
| Ctrl+S | Save circuit |
| Ctrl+O | Open circuit |
| Ctrl+Z | Undo |
| Ctrl+Shift+Z | Redo |
| Space | Start/pause simulation |
| + / - | Adjust simulation speed |
| Ctrl+K | Open spotlight search |
| Ctrl+G | Open subcircuit creation dialog (with selection) |
| F1 | Show keyboard shortcuts dialog |

## Technical Details

### Circuit Solver

The simulator uses Modified Nodal Analysis (MNA) to solve circuit equations:

1. Build conductance matrix $G$ and current vector $I$
2. Each component "stamps" its contribution to the matrix
3. Solve $Gx = I$ using Gaussian elimination with partial pivoting
4. Iterate with Newton-Raphson for nonlinear components until convergence

### Transient Analysis

For time-domain simulation:
- Capacitors use companion model: $I = C \frac{V - V_{prev}}{\Delta t}$
- Inductors use companion model: $V = L \frac{I - I_{prev}}{\Delta t}$
- Adaptive time step based on highest frequency signal
- 50-300 samples per period for smooth waveforms

### Component Models

- **Diode**: Shockley equation $I = I_s \left( e^{\frac{V}{nV_t}} - 1 \right)$
- **LED**: Diode model with forward voltage drop based on color
- **Zener**: Reverse breakdown with knee resistance $R_z$
- **BJT**: Ebers-Moll model with $\beta$, Early voltage $V_{AF}$, saturation current $I_s$
- **MOSFET**: Square-law model $I_D = K_n(V_{GS} - V_{th})^2$ with cutoff, triode, and saturation regions
- **JFET**: Shockley equation with $I_{DSS}$ and pinch-off voltage $V_P$
- **Op-Amp**: VCVS with gain $A_{OL}$, GBW, slew rate, and rail limiting
- **Thyristor**: Latching behavior with trigger conditions

## File Structure

```
circuit_toy/
├── meson.build              # Meson build configuration
├── README.md                # This file
├── include/                 # Header files
│   ├── types.h              # Common type definitions
│   ├── matrix.h             # Matrix/Vector operations
│   ├── component.h          # Component definitions
│   ├── circuit.h            # Circuit container
│   ├── circuits.h           # Pre-built circuit templates
│   ├── simulation.h         # Simulation engine
│   ├── analysis.h           # Bode plot analysis
│   ├── render.h             # SDL2 rendering
│   ├── ui.h                 # UI system
│   ├── input.h              # Input handling
│   ├── file_io.h            # File I/O
│   └── app.h                # Main application
├── src/                     # Source files
│   ├── main.c               # Entry point
│   ├── app.c                # Application logic
│   ├── matrix.c             # Linear algebra
│   ├── component.c          # Component implementation
│   ├── circuit.c            # Circuit management
│   ├── circuits.c           # Circuit template implementations
│   ├── simulation.c         # Simulation engine
│   ├── analysis.c           # Bode plot and frequency analysis
│   ├── render.c             # SDL2 rendering
│   ├── ui.c                 # UI rendering
│   ├── input.c              # Input handling
│   └── file_io.c            # File save/load
└── subprojects/             # Dependencies (SDL2)
```

## Platform Support

- Windows (x64) - Primary development platform
  - No VC++ Redistributable required - standalone executable with static CRT linking
- Linux (x64) - Tested on Ubuntu
- macOS (x64, arm64) - Should work with SDL2

## Known Issues / Work in Progress

- **Subcircuit Simulation (WIP)** - Subcircuits can be created and placed as IC blocks, but internal simulation is still under development. Currently subcircuits function as visual placeholders - full hierarchical simulation with internal node expansion is planned for a future release.

- **Current Flow Visualization** - The animated current flow now uses BFS-based path tracing from sources to ground, with improved handling for voltage source negative terminal connections and dual voltage source configurations.

## License

MIT License - See LICENSE file for details.

## Acknowledgments

- Inspired by **[Paul Falstad's circuit.js](https://www.falstad.com/circuit/)** (source:
  [pfalstad/circuitjs1](https://github.com/pfalstad/circuitjs1)) - the simulator that made circuit
  theory something you learn by dragging parts around and watching the current move. Its
  example-first library, its animated current dots and its click-anything-and-watch-it-respond
  feel are the model this project works from; the Circuits tab, the current-flow display and the
  on-canvas theory notes all exist because circuit.js showed how much a learner gets from them.
- Inspired by [The Powder Toy](https://github.com/The-Powder-Toy/The-Powder-Toy) particle simulation game
- Architecture follows the same C/SDL2 pattern
- Synthwave color theme inspired by 1980s aesthetics
- Component models based on SPICE simulation principles
