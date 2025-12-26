project('circuit-playground', 'c',
  version : '1.0.0',
  default_options : ['c_std=c11', 'warning_level=2', 'b_vscrt=mt']
)

# Option to enable static linking (standalone executable without DLL dependencies)
static_build = get_option('default_library') == 'static'

# Find SDL2 dependency (with subproject fallback for Windows)
# For static builds, we build SDL2 as a static library
if static_build and host_machine.system() == 'windows'
  # Build SDL2 statically for standalone Windows executable
  sdl2_proj = subproject('sdl2',
    default_options : ['default_library=static'],
    required : true
  )
  sdl2_dep = sdl2_proj.get_variable('sdl2_dep')
  sdl2main_dep = sdl2_proj.get_variable('sdl2main_dep')
else
  sdl2_dep = dependency('sdl2', required : false)
  sdl2main_dep = dependency('sdl2main', required : false)
  if not sdl2_dep.found()
    sdl2_proj = subproject('sdl2', required : true)
    sdl2_dep = sdl2_proj.get_variable('sdl2_dep')
    sdl2main_dep = sdl2_proj.get_variable('sdl2main_dep')
  endif
endif

# Windows system libraries required for static SDL2 linking
win_deps = []
if host_machine.system() == 'windows' and static_build
  cc = meson.get_compiler('c')
  win_deps += cc.find_library('user32')
  win_deps += cc.find_library('gdi32')
  win_deps += cc.find_library('winmm')
  win_deps += cc.find_library('imm32')
  win_deps += cc.find_library('ole32')
  win_deps += cc.find_library('oleaut32')
  win_deps += cc.find_library('version')
  win_deps += cc.find_library('uuid')
  win_deps += cc.find_library('advapi32')
  win_deps += cc.find_library('setupapi')
  win_deps += cc.find_library('shell32')
  win_deps += cc.find_library('hid')
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
  dependencies : [sdl2_dep, sdl2main_dep] + win_deps,
  install : true,
  win_subsystem : 'console'  # Show console window for debug output
)

# Development tools
subdir('tools')
