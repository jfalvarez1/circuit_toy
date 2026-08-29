"""A triggered trace has to stand still.

Renders the same template at two consecutive frames and compares the scope area. A properly
triggered display anchors its window on the trigger, so consecutive frames draw the same
waveform; an untriggered one slides sideways by however far the simulation advanced. The failure
this catches is subtle in a screenshot and obvious in a diff: the trace moves as a body.

    python tools/trace_stability.py build/circuit-playground.exe "Crosstalk" "Common Emitter"
"""
import subprocess, sys, os, tempfile

try:
    from PIL import Image
    import numpy as np
except ImportError:
    print("trace-stability: needs pillow and numpy; skipped")
    sys.exit(0)

exe = sys.argv[1] if len(sys.argv) > 1 else "build/circuit-playground.exe"
# a bare relative path is not on PATH: make it explicit, the way a shell would with ./
if os.path.sep not in exe and "/" not in exe:
    exe = os.path.join(".", exe)
exe = os.path.abspath(exe)
if not os.path.exists(exe):
    print("trace-stability: no binary at %s" % exe)
    sys.exit(1)
templates = sys.argv[2:] or ["Crosstalk", "Common Emitter", "Buck Converter"]
W, H = 1400, 900
CROP = (980, 280, 1390, 640)          # the scope screen
fails = 0

with tempfile.TemporaryDirectory() as tmp:
    for name in templates:
        shots = []
        for frame in (400, 401):
            out = os.path.join(tmp, "f%d.bmp" % frame)
            r = subprocess.run([exe, "--template", name, "--size", "%dx%d" % (W, H),
                                "--frame", str(frame), "--shot", out, "--exit",
                                "--no-update-check"], capture_output=True, text=True)
            if not os.path.exists(out):
                print("[FAIL] stability %-24s no screenshot: %s" % (name, (r.stderr or "").strip()[:80]))
                fails += 1
                shots = []
                break
            shots.append(np.asarray(Image.open(out).crop(CROP).convert("L"), dtype=int))
        if len(shots) != 2:
            continue

        a, b = shots
        # A slide shows up as "some horizontal shift matches much better than none".
        def diff(x, y):
            return int((x[:, 25:-25] != y[:, 25:-25]).sum())
        base = diff(a, b)
        best_dx, best = 0, base
        for dx in range(-25, 26):
            if dx == 0:
                continue
            d = diff(a, np.roll(b, dx, axis=1))
            if d < best:
                best, best_dx = d, dx
        moved = best_dx != 0 and best < base * 0.6
        pct = 100.0 * base / a[:, 25:-25].size
        if moved:
            print("[FAIL] stability %-24s the trace slides %+d px between frames "
                  "(%.1f%% of pixels differ in place, %.1f%% when shifted)"
                  % (name, best_dx, pct, 100.0 * best / a[:, 25:-25].size))
            fails += 1
        else:
            print("[ OK ] stability %-24s stands still (%.1f%% of pixels differ, no shift fits better)"
                  % (name, pct))

print("\ntrace-stability: %d templates, %d whose trace does not stand still" % (len(templates), fails))
sys.exit(1 if fails else 0)
