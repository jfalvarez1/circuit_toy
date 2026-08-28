"""Copy one file, for meson custom_target use: copy_file.py <src> <dst>.

Meson's configure_file(copy: true) happens at configure time, which is too early for
anything a subproject builds - and reading a DLL out of the source tree only works on a
machine that happens to have one lying there. This runs at build time instead.
"""
import shutil
import sys

if len(sys.argv) != 3:
    sys.stderr.write("usage: copy_file.py <src> <dst>\n")
    raise SystemExit(2)

shutil.copyfile(sys.argv[1], sys.argv[2])
