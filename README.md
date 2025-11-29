# Circuit Playground Simulator

A web-based interactive circuit simulator inspired by The Powder Toy, but focused on electronic circuit simulation. Build, simulate, and analyze electronic circuits with an intuitive drag-and-drop interface.

## Features

### Components

**Sources**
- Ground reference
- DC Voltage Source (adjustable voltage)
- AC Voltage Source (adjustable amplitude, frequency, phase, DC offset)
- DC Current Source (adjustable current)

**Passive Components**
- Resistor (adjustable resistance)
- Capacitor (adjustable capacitance, supports transient simulation)
- Inductor (adjustable inductance, supports transient simulation)

**Semiconductors**
- Diode (Shockley equation model)
- NPN BJT Transistor (Ebers-Moll model with adjustable beta)
- PNP BJT Transistor
- N-Channel MOSFET (square-law model with Vth, Kn parameters)
- P-Channel MOSFET

**Active Components**
- Ideal Op-Amp (adjustable gain, rail voltages)

### Simulation Engine

- **Modified Nodal Analysis (MNA)** for accurate DC and transient simulation
- **Newton-Raphson iteration** for nonlinear component convergence
- **Backward Euler integration** for capacitors and inductors
- Adjustable simulation speed (0.1x to 100x real-time)
- Step-by-step simulation mode

### Measurement Tools

- **Voltage Probes** - Place probes on nodes to measure voltage
- **Oscilloscope** - 2-channel display with adjustable time/div and V/div
- **Voltmeter** - Measure voltage between two points
- **Ammeter** - Measure current through components

### User Interface

- Grid-based component placement with snap-to-grid
- Pan (middle mouse or Shift+drag) and zoom (scroll wheel)
- Component rotation (double-click or 'R' key)
- Wire routing between component terminals
- Property editor panel for adjusting component values
- Save/load circuits as JSON files

## Getting Started

1. Open `index.html` in a modern web browser
2. Select a component from the left palette
3. Click on the canvas to place the component
4. Use the Wire tool to connect component terminals
5. Always include a Ground component as a reference
6. Click Run to start the simulation

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Escape | Cancel current action, return to select tool |
| Delete/Backspace | Delete selected component |
| R | Rotate selected component |
| G | Toggle grid visibility |
| S | Toggle snap-to-grid |

## Technical Details

### Circuit Solver

The simulator uses Modified Nodal Analysis (MNA) to solve the circuit equations:

1. Build conductance matrix (G) and current vector (I)
2. Each component "stamps" its contribution to the matrix
3. Solve Gx = I using LU decomposition with partial pivoting
4. Iterate for nonlinear components until convergence

### Transient Analysis

For time-domain simulation:
- Capacitors use companion model: `I = C(V - V_prev)/dt`
- Inductors use companion model: `V = L(I - I_prev)/dt`
- Adjustable time step for accuracy vs speed tradeoff

### Component Models

- **Diode**: Shockley equation `I = Is(exp(V/nVt) - 1)`
- **BJT**: Simplified Ebers-Moll with beta and early voltage
- **MOSFET**: Square-law model with cutoff, triode, and saturation regions
- **Op-Amp**: Ideal VCVS with rail limiting

## File Structure

```
circuit_toy/
├── index.html              # Main HTML file
├── css/
│   └── styles.css          # Styling
└── js/
    ├── math/
    │   ├── matrix.js       # Matrix/Vector classes
    │   └── solver.js       # Linear solver & MNA builder
    ├── core/
    │   ├── component.js    # Base component class
    │   ├── node.js         # Circuit node management
    │   ├── wire.js         # Wire connections
    │   └── circuit.js      # Circuit container
    ├── components/
    │   ├── ground.js       # Ground component
    │   ├── sources.js      # Voltage/current sources
    │   ├── passives.js     # R, L, C components
    │   ├── semiconductors.js # Diode, BJT, MOSFET
    │   └── opamp.js        # Operational amplifier
    ├── simulation/
    │   └── engine.js       # Simulation engine
    ├── ui/
    │   ├── canvas.js       # Canvas rendering & interaction
    │   ├── palette.js      # Component palette
    │   ├── properties.js   # Property editor
    │   ├── oscilloscope.js # Oscilloscope display
    │   └── measurements.js # Measurement panel
    └── app.js              # Main application
```

## Browser Support

Requires a modern browser with:
- HTML5 Canvas
- ES6+ JavaScript
- CSS Grid/Flexbox

Tested on Chrome, Firefox, Safari, and Edge.

## License

MIT License - See LICENSE file for details.

## Acknowledgments

Inspired by [The Powder Toy](https://github.com/jfalvarez1/powder_toy) particle simulation game.
