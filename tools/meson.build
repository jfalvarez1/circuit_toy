# UI Editor Tool
ui_editor = executable(
    'ui_editor',
    'ui_editor.c',
    dependencies: [sdl2_dep, sdl2main_dep],
    include_directories: include_directories('..', '../include'),
    install: false
)

# Headless template smoke test: places every prebuilt circuit, runs DC + transient,
# reports non-convergence / NaN and prints semiconductor bias points.
template_smoke = executable(
    'template_smoke',
    ['template_smoke.c',
     '../src/matrix.c', '../src/component.c', '../src/circuit.c', '../src/circuits.c',
     '../src/simulation.c', '../src/logic.c', '../src/analysis.c', '../src/threadpool.c',
     '../src/file_io.c', '../src/spice.c', '../src/label.c', '../src/netlist.c'],
    include_directories: include_directories('..', '../include'),
    install: false
)
