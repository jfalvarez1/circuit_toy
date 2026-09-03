project('circuit-playground', 'c',
  version : '3.2.3',
  default_options : ['c_std=c11', 'warning_level=2', 'b_vscrt=mt']
)

# Option to enable static linking (standalone executable without DLL dependencies)
static_build = get_option('default_library') == 'static'

# Find SDL2 dependency (with subproject fallback for Windows)
# For static builds, we build SDL2 as a static library
sdl2_from_subproject = false
if static_build and host_machine.system() == 'windows'
  # Build SDL2 statically for standalone Windows executable
  sdl2_proj = subproject('sdl2',
    default_options : ['default_library=static'],
    required : true
  )
  sdl2_dep = sdl2_proj.get_variable('sdl2_dep')
  sdl2main_dep = sdl2_proj.get_variable('sdl2main_dep')
  sdl2_from_subproject = true
else
  sdl2_dep = dependency('sdl2', required : false)
  sdl2main_dep = dependency('sdl2main', required : false)
  if not sdl2_dep.found()
    sdl2_proj = subproject('sdl2', required : true)
    sdl2_dep = sdl2_proj.get_variable('sdl2_dep')
    sdl2main_dep = sdl2_proj.get_variable('sdl2main_dep')
    sdl2_from_subproject = true
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
  win_deps += cc.find_library('comdlg32')   # GetOpenFileName: the Load / Import file picker
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
  'src/label.c',
  'src/ui.c',
  'src/input.c',
  'src/file_io.c',
  'src/analysis.c',
  'src/threadpool.c',
  'src/updater.c',
  'src/settings.c',
  'src/crashlog.c',
  'src/spice.c',
  'src/netlist.c',
  'src/sketch.c'
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
  # A GUI subsystem binary, not a console one. As a console app the updater's relaunch was a
  # coin flip: PowerShell starts the new exe, its own console window closes a moment later, and
  # a console process attached to a console that is going away either takes the close event and
  # dies ("reopens and just crashes instantly") or fails during DLL init with 0xC0000142 ("the
  # application was unable to start correctly"). It also meant a console window behind the app
  # for anyone who just double-clicks it.
  # Command-line output is not lost: main() attaches to the parent console when there are
  # arguments (see attach_parent_console), and redirection to a pipe or file works regardless,
  # which is how CI captures it.
  win_subsystem : 'windows',
  # 8 MB of stack instead of the default 1. The app state is off the stack now, but the template
  # builders are long functions with a lot of locals and the simulation recurses through the
  # netlist, and a release that dies with 0xC00000FD before its first frame is not a failure
  # anyone can diagnose from the outside. Headroom is free: this is a reserve, not a commitment.
  link_args : host_machine.system() == 'windows' ? ['/STACK:8388608'] : []
)

# SDL2 runtime next to the exe, so the app starts from a taskbar pin or Explorer (Windows
# searches the exe's own directory first) and so a freshly configured build directory can run
# the headless UI checks. It is copied from the library the subproject BUILDS: doing it with
# configure_file(copy: true) reads from the source tree at configure time, which only ever
# worked on a machine with a stray SDL2-8.dll in it and failed every clean checkout.
# A static build links SDL2 in and needs no DLL at all.
if host_machine.system() == 'windows' and not static_build and sdl2_from_subproject
  py_copy = find_program('python3', 'python')
  custom_target('sdl2-runtime-dll',
    input : sdl2_proj.get_variable('sdl2'),
    output : 'SDL2-8.dll',            # the name the exe's import table records
    command : [py_copy, files('tools/copy_file.py'), '@INPUT@', '@OUTPUT@'],
    build_by_default : true
  )
endif

# Development tools
subdir('tools')
