# UI Editor Tool
ui_editor = executable(
    'ui_editor',
    'ui_editor.c',
    dependencies: [sdl2_dep, sdl2main_dep],
    include_directories: include_directories('..', '../include'),
    install: false
)

# libm and pthreads are libraries of their own outside Windows. The app gets both through SDL2;
# the smoke test links no SDL, so it has to ask. On MSVC these come back empty and change nothing.
smoke_cc = meson.get_compiler('c')
smoke_deps = [smoke_cc.find_library('m', required : false), dependency('threads')]

# Headless template smoke test: places every prebuilt circuit, runs DC + transient,
# reports non-convergence / NaN and prints semiconductor bias points.
template_smoke = executable(
    'template_smoke',
    ['template_smoke.c',
     '../src/matrix.c', '../src/component.c', '../src/circuit.c', '../src/circuits.c',
     '../src/simulation.c', '../src/logic.c', '../src/analysis.c', '../src/threadpool.c',
     '../src/file_io.c', '../src/spice.c', '../src/label.c', '../src/netlist.c',
     '../src/sketch.c'],
    include_directories: include_directories('..', '../include'),
    dependencies: smoke_deps,
    install: false
)
