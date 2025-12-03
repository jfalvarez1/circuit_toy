# CLAUDE.md - Circuit Playground Simulator

This document provides guidance for AI assistants working with the Circuit Playground codebase.

## Project Overview

**Circuit Playground Simulator** is a native desktop circuit simulator written in C11 with SDL2, featuring a synthwave-themed interface. Users can build, simulate, and analyze electronic circuits with an intuitive drag-and-drop interface.

- **Current Version**: v2.6.0
- **Language**: C11
- **Graphics Framework**: SDL2 2.32.8
- **License**: MIT
- **Platforms**: Windows (x64), Linux (x64), macOS (x64/arm64)

## Repository Structure

```
circuit_toy/
├── include/           # Header files (14 files)
│   ├── types.h        # Common types, enums, constants, limits
│   ├── component.h    # Component definitions and models
│   ├── circuit.h      # Circuit container and node management
│   ├── simulation.h   # MNA simulation engine interface
│   ├── matrix.h       # Linear algebra for MNA solver
│   ├── analysis.h     # Advanced analysis (Bode, Monte Carlo, FFT)
│   ├── render.h       # SDL2 rendering system
│   ├── ui.h           # UI system and palette
│   ├── input.h        # Input handling
│   ├── file_io.h      # File I/O (binary + JSON export)
│   ├── app.h          # Main application state
│   ├── logic.h        # Mixed-signal logic solver
│   ├── threadpool.h   # Cross-platform thread pool
│   └── circuits.h     # Pre-built circuit templates
├── src/               # Implementation files (14 files)
│   ├── main.c         # SDL initialization and main loop
│   ├── app.c          # Application logic and threading
│   ├── circuit.c      # Circuit management
│   ├── circuits.c     # Circuit template implementations
│   ├── component.c    # Component behavioral models
│   ├── simulation.c   # MNA solver and transient analysis
│   ├── matrix.c       # Matrix operations and linear solver
│   ├── render.c       # Rendering implementation
│   ├── ui.c           # UI rendering and interaction
│   ├── input.c        # Input event handling
│   ├── file_io.c      # File save/load operations
│   ├── analysis.c     # Analysis algorithms (FFT, Bode)
│   ├── logic.c        # Mixed-signal logic propagation
│   └── threadpool.c   # Thread pool implementation
├── tools/             # Development tools
│   └── ui_editor.c    # UI layout editor tool
├── gifs/              # Demonstration GIFs for README
├── subprojects/       # Dependencies (SDL2 fetched by Meson)
├── meson.build        # Primary build configuration
├── CMakeLists.txt     # Alternative CMake build
├── package.json       # Electron wrapper configuration
└── README.md          # User documentation
```

## Build Instructions

### Primary Build System: Meson

```bash
# Setup build directory (SDL2 is fetched automatically on Windows)
meson setup build

# Compile
meson compile -C build

# Run
./build/circuit-playground      # Linux/macOS
./build/circuit-playground.exe  # Windows
```

### Requirements

- Meson 0.60+
- Ninja build tool
- C11 compiler (GCC, Clang, or MSVC)
- SDL2 (auto-fetched on Windows, install via package manager on Linux/macOS)

### Linux Dependencies

```bash
sudo apt-get install meson ninja-build libsdl2-dev
```

### macOS Dependencies

```bash
brew install meson ninja sdl2
```

## Architecture Overview

### Simulation Engine (Modified Nodal Analysis)

The simulator uses MNA to solve circuit equations:
1. Build conductance matrix G and current vector I
2. Each component "stamps" its contribution to the matrix
3. Solve `Gx = I` using Gaussian elimination with partial pivoting
4. Iterate with Newton-Raphson for nonlinear components until convergence

Key files: `src/simulation.c`, `src/matrix.c`

### Component System

150+ component types with behavioral models including:
- Passive (R, L, C with parasitics)
- Sources (DC, AC, waveform generators)
- Semiconductors (diodes with Shockley equation, BJTs with Ebers-Moll, MOSFETs with square-law)
- Op-amps (ideal and real models)
- Digital logic gates and ICs
- Voltage regulators

Key files: `src/component.c`, `include/component.h`, `include/types.h`

### UI System

Custom SDL2-based UI with synthwave theme (neon pink/cyan/purple). Features:
- Spotlight search (Ctrl+K) with fuzzy matching
- Collapsible component palette
- 8-channel oscilloscope with FFT, triggers, and math channels
- Bode plot analysis
- Monte Carlo analysis

Key files: `src/ui.c`, `src/input.c`, `src/render.c`

## Key Constants and Limits

Defined in `include/types.h`:

```c
#define MAX_COMPONENTS 1024
#define MAX_NODES 2048
#define MAX_WIRES 2048
#define MAX_PROBES 8
#define GRID_SIZE 20
#define MAX_ZOOM 4.0f
#define MIN_ZOOM 0.25f
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
```

## Coding Conventions

### Naming

- **Functions**: `snake_case` (e.g., `circuit_add_component`, `render_draw_line`)
- **Types/Structs**: `PascalCase` or `snake_case` (e.g., `Component`, `RenderContext`)
- **Constants/Macros**: `UPPER_SNAKE_CASE` (e.g., `MAX_COMPONENTS`, `GRID_SIZE`)
- **Enums**: Prefixed with type (e.g., `COMP_RESISTOR`, `TOOL_SELECT`, `SIM_RUNNING`)

### Memory Management

- Manual allocation with `_create()` / `_free()` function pairs
- Fixed-size arrays (no dynamic reallocation)
- Global state for audio, microphone, wireless, environment, subcircuit library

### Error Handling

- Functions return `bool` for success/failure
- Error messages stored in simulation/file_io state structs
- No exceptions (C language)

### File Organization

- Each module has paired `.h` and `.c` files
- Headers in `include/`, implementations in `src/`
- One struct/concept per module (Component, Circuit, Simulation, etc.)

## Component Model Reference

When modifying or adding components:

```c
// Component types are defined in include/types.h (ComponentType enum)
// Component properties use a union in include/component.h

// To add a new component:
// 1. Add enum value to ComponentType in types.h
// 2. Add property struct to ComponentProps union in component.h
// 3. Implement stamping in simulation.c (simulation_stamp_component)
// 4. Add rendering in render.c (render_component)
// 5. Add to palette in ui.c (ui_init or palette category arrays)
// 6. Handle in input.c for selection/placement
```

## Global State Variables

Located in various source files:

```c
extern EnvironmentState g_environment;    // component.c - light/temperature
extern AudioState g_audio;                 // app.c - speaker output
extern MicrophoneState g_microphone;       // app.c - mic input
extern WirelessState g_wireless;           // simulation.c - antenna communication
extern SubCircuitLibrary g_subcircuit_library;  // component.c - user subcircuits
```

## Testing

Currently no automated unit tests. Validation approaches:
- Pre-built circuit templates serve as integration tests
- Visual verification via oscilloscope output
- Simulation convergence tracking
- Error handling for numerical issues (short circuits, non-convergence)

## Known Issues / Work in Progress

1. **Subcircuit Simulation (WIP)**: Subcircuits can be created and placed as IC blocks, but internal simulation is incomplete. Currently visual placeholders.

2. **Current Flow Visualization**: Uses BFS-based path tracing from sources to ground (recently improved in v2.6.0).

## Common Development Tasks

### Adding a New Component Type

1. Add to `ComponentType` enum in `include/types.h`
2. Add properties struct to `ComponentProps` union in `include/component.h`
3. Implement `component_get_name()` case in `src/component.c`
4. Add MNA stamping in `simulation_stamp_component()` in `src/simulation.c`
5. Add rendering in `render_component()` in `src/render.c`
6. Add to appropriate palette category in `src/ui.c`

### Adding a Pre-built Circuit Template

1. Add function declaration in `include/circuits.h`
2. Implement in `src/circuits.c`
3. Add to circuits menu in `src/ui.c`

### Modifying the UI

- Panel sizes defined in `include/types.h`
- Colors defined as `COLOR_*` macros in `include/types.h`
- Synthwave theme colors: pink (#ff2975), cyan (#00d9ff), purple (#bd00ff)

## File Formats

### Binary Circuit Format (.ckt)

- Magic: `0x43495243` ("CIRC")
- Version: 1
- Contains serialized components, wires, and circuit state

### JSON Export

Human-readable format for circuit interchange.

## Keyboard Shortcuts (for reference)

| Key | Action |
|-----|--------|
| Space | Start/pause simulation |
| R | Rotate component |
| W | Wire tool |
| G | Toggle grid |
| S | Toggle snap-to-grid |
| Delete | Delete selected |
| Ctrl+K | Spotlight search |
| Ctrl+G | Create subcircuit |
| Ctrl+Z/Ctrl+Shift+Z | Undo/Redo |
| Ctrl+S/Ctrl+O | Save/Open |
| F1 | Keyboard shortcuts dialog |
