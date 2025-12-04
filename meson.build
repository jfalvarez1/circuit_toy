project('circuit-playground', 'c',
  version : '1.0.0',
  default_options : ['c_std=c11', 'warning_level=2']
)

# Find SDL2 dependency (with subproject fallback for Windows)
sdl2_dep = dependency('sdl2', required : false)
sdl2main_dep = dependency('sdl2main', required : false)
if not sdl2_dep.found()
  sdl2_proj = subproject('sdl2', required : true)
  sdl2_dep = sdl2_proj.get_variable('sdl2_dep')
  sdl2main_dep = sdl2_proj.get_variable('sdl2main_dep')
endif

# Source files
src_files = files(
  'src/main.c',
  'src/app.c',
  'src/matrix.c',
  'src/component.c',
  'src/circuit.c',
  'src/circuits.c',
  'src/simulation.c',
  'src/logic.c',
  'src/render.c',
  'src/ui.c',
  'src/input.c',
  'src/file_io.c',
  'src/analysis.c',
  'src/threadpool.c'
)

# Windows resource file for application icon
if host_machine.system() == 'windows'
  windows = import('windows')
  win_resources = windows.compile_resources('app.rc')
  src_files += win_resources
endif

# Include directories
inc_dirs = include_directories('include')

# Build executable
executable('circuit-playground',
  src_files,
  include_directories : inc_dirs,
  dependencies : [sdl2_dep, sdl2main_dep],
  install : true,
  win_subsystem : 'console'  # Show console window for debug output
)

# Development tools
subdir('tools')
