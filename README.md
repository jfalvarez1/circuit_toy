# Circuit Playground Simulator

**Latest Release: [v3.4.0](https://github.com/jfalvarez1/circuit_toy/releases/tag/v3.4.0)** (auto-updating from v3.4.0 on)

A native desktop circuit simulator written in C with SDL2, featuring a synthwave-themed interface. Build, simulate, and analyze electronic circuits with an intuitive drag-and-drop interface.

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

86 ready-made circuits live in the **Circuits** tab of the left panel, grouped by topic
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

**High voltage**
- **Tesla Coil** (`Tesla`) - Spark-gap Tesla coil, 4x13 in toroid, streamer to a rod
- **Tesla Coil (big top)** (`TeslaB`) - Retuned for an 8x24 in toroid: more energy, longer arc
- **Tesla Coil (detuned)** (`TeslaX`) - Big toroid but the primary was not retuned: weak output

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
- **Pop-out Window** - Detach oscilloscope to separate resizable window

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

![Function generator](gifs/auto_function_generator.gif)

![Three-phase](gifs/auto_three_phase_balanced.gif)

![RC sweep](gifs/auto_rc_lowpass_sweep.gif)

![Tesla coil](gifs/auto_tesla_coil.gif)

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
build/tools/template_smoke.exe             # 86/86 templates passed
build/tools/template_smoke.exe --verbose   # + bias voltages per active device
build/tools/template_smoke.exe --nodes "Wien"   # + node -> matrix mapping for one template
build/tools/template_smoke.exe --probe-test      # output node of every template vs hand calculation (66 oracles)
build/tools/template_smoke.exe --knob-test       # every template still converges with every value x0.5 and x2
build/tools/template_smoke.exe --trace "87 " 0.3 # per-node min/max over a run (debugging a template)
build/tools/template_smoke.exe --demo-test       # every template demonstrates its DemoKind contract
build/tools/template_smoke.exe --osc-test        # oscillators really oscillate (add --osc-dt 5e-6)
build/tools/template_smoke.exe --tesla-test      # spark-gap firings, ring frequency, toroid peak, streamer, tuned vs detuned
build/tools/template_smoke.exe --param-test      # spark gap / toroid / line / transformer limits vs phasor oracles; scope presets
build/tools/template_smoke.exe --flow-test       # current-flow display: KCL, conservation, series uniformity
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

The app itself has automation flags for reproducible screenshots (used by `tools/make_media.py`,
which produced the images in this README):

```bash
build/circuit-playground.exe --template Tesla --size 1400x900 --shot out.bmp --frame 300 --exit
build/circuit-playground.exe --template LP --record frames 48 3 --exit    # 48 frames, one every 3
build/circuit-playground.exe --help
```

The app itself has automation flags for reproducible screenshots (used by `tools/make_media.py`,
which produced the images in this README):

```bash
build/circuit-playground.exe --template Tesla --size 1400x900 --shot out.bmp --frame 300 --exit
build/circuit-playground.exe --template LP --record frames 48 3 --exit    # 48 frames, one every 3
build/circuit-playground.exe --help
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

- Inspired by [The Powder Toy](https://github.com/The-Powder-Toy/The-Powder-Toy) particle simulation game
- Architecture follows the same C/SDL2 pattern
- Synthwave color theme inspired by 1980s aesthetics
- Component models based on SPICE simulation principles
