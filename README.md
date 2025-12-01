# Circuit Playground Simulator

A native desktop circuit simulator written in C with SDL2, inspired by The Powder Toy. Build, simulate, and analyze electronic circuits with an intuitive interface.

![Circuit Playground Screenshot](screenshot.png)

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
- Save/load circuits (binary and JSON formats)

## Building

### Requirements

- CMake 3.16 or higher
- SDL2 development libraries
- C11 compatible compiler (GCC, Clang, or MSVC)

### Linux

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install cmake libsdl2-dev

# Build
mkdir build && cd build
cmake ..
make

# Run
./CircuitPlayground
```

### Windows

```bash
# Using vcpkg for dependencies
vcpkg install sdl2

# Build with CMake
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release

# Or use the NSIS installer
cpack -G NSIS
```

### macOS

```bash
# Install dependencies
brew install cmake sdl2

# Build
mkdir build && cd build
cmake ..
make

# Run
./CircuitPlayground
```

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Escape | Cancel current action, return to select tool |
| Delete/Backspace | Delete selected component |
| R | Rotate selected component |
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
| F1 | Show keyboard shortcuts |

## Technical Details

### Circuit Solver

The simulator uses Modified Nodal Analysis (MNA) to solve the circuit equations:

1. Build conductance matrix (G) and current vector (I)
2. Each component "stamps" its contribution to the matrix
3. Solve Gx = I using Gaussian elimination with partial pivoting
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
├── CMakeLists.txt          # CMake build configuration
├── README.md               # This file
├── include/                # Header files
│   ├── types.h             # Common type definitions
│   ├── matrix.h            # Matrix/Vector operations
│   ├── component.h         # Component definitions
│   ├── circuit.h           # Circuit container
│   ├── simulation.h        # Simulation engine
│   ├── render.h            # SDL2 rendering
│   ├── ui.h                # UI system
│   ├── input.h             # Input handling
│   ├── file_io.h           # File I/O
│   └── app.h               # Main application
├── src/                    # Source files
│   ├── main.c              # Entry point
│   ├── app.c               # Application logic
│   ├── matrix.c            # Linear algebra
│   ├── component.c         # Component implementation
│   ├── circuit.c           # Circuit management
│   ├── simulation.c        # Simulation engine
│   ├── render.c            # SDL2 rendering
│   ├── ui.c                # UI rendering
│   ├── input.c             # Input handling
│   └── file_io.c           # File save/load
└── resources/              # Assets (fonts, icons)
```

## Platform Support

- Windows (x64)
- Linux (x64)
- macOS (x64, arm64)

## License

MIT License - See LICENSE file for details.

## Acknowledgments

Inspired by [The Powder Toy](https://github.com/The-Powder-Toy/The-Powder-Toy) particle simulation game.
Architecture follows the same C/SDL2 pattern used by The Powder Toy.
