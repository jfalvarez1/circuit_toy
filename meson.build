project('circuit-playground', 'c',
  version : '1.0.0',
  default_options : ['c_std=c11', 'warning_level=2']
)

# Find SDL2 dependency
sdl2_dep = dependency('sdl2', required : true)

# Source files
src_files = files(
  'src/main.c',
  'src/app.c',
  'src/matrix.c',
  'src/component.c',
  'src/circuit.c',
  'src/simulation.c',
  'src/render.c',
  'src/ui.c',
  'src/input.c',
  'src/file_io.c'
)

# Include directories
inc_dirs = include_directories('include')

# Build executable
executable('circuit-playground',
  src_files,
  include_directories : inc_dirs,
  dependencies : [sdl2_dep],
  install : true,
  win_subsystem : 'windows'  # Hide console window on Windows
)
