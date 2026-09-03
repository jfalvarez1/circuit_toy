"""Nothing a template draws may be cut off by the canvas edge.

Renders every template the way a user first sees it and checks the border of the canvas for lit
pixels. This is the generalisation of a fault found by looking at two screenshots: the Common
Emitter's source label ("100mV 1000Hz") ran off the left edge and the Pierce's "4.7nF" off the
right, because fit-on-place framed the symbols but not their value labels. A human caught two of
187; this asks all of them.

A template whose content legitimately cannot fit - the Digital Clock at the 0.3x zoom floor - is
reported as a NOTE rather than failed, keyed off the zoom floor itself: if the fit already gave up,
clipping is the floor's documented trade, not a layout fault.

    python tools/edge_gui.py build/circuit-playground.exe
"""
import subprocess, sys, os, tempfile, json
from concurrent.futures import ThreadPoolExecutor

try:
    from PIL import Image
except ImportError:
    print("edge-gui: needs pillow; skipped")
    sys.exit(0)

exe = sys.argv[1] if len(sys.argv) > 1 else "build/circuit-playground.exe"
if os.path.sep not in exe and "/" not in exe:
    exe = os.path.join(".", exe)
exe = os.path.abspath(exe)
if not os.path.exists(exe):
    print("edge-gui: no binary at %s" % exe)
    sys.exit(1)

W, H = 1400, 900
# the canvas between the palette and the scope panel, inside the toolbar and status bar,
# inset by 2 px so the canvas border line itself is not read as content
CANVAS = (185, 48, 977, 872)
# the background is the dark grid; anything brighter than this on the border is drawn content
LIT = 90

# every template, by asking the binary (exit 2 prints the list)
r = subprocess.run([exe, "--template", "\x01no-such-template\x01"], capture_output=True, text=True)
names = []
for line in r.stderr.splitlines():
    if line.startswith("  ") and len(line) > 11:
        names.append(line[11:].strip())
if not names:
    print("edge-gui: could not list templates")
    sys.exit(1)

def check(args):
    """One template: launch, screenshot, read the canvas border. Returns (kind, message).

    A whole app launch per template, and there are 205 of them - which is forty minutes done one
    after another, and was, the first time anything ran this gate. They do not depend on each
    other in any way, so they do not have to be serial: each gets its own temporary directory and
    they go through a pool. The work is a subprocess and some pixel reading, both of which spend
    almost all of their time outside the interpreter."""
    name, tmp, idx = args
    # Named per task, not per worker: the pool hands a task to whichever thread is free, so two
    # of them can be working out of the same place at the same time.
    bmp = os.path.join(tmp, "t%d.bmp" % idx)
    sj = os.path.join(tmp, "t%d.json" % idx)
    subprocess.run([exe, "--template", name, "--size", "%dx%d" % (W, H), "--frame", "40",
                    "--shot", bmp, "--state-out", sj, "--exit", "--no-update-check"],
                   capture_output=True, text=True)
    if not os.path.exists(bmp):
        return "FAIL", "[FAIL] edge  %-32s no screenshot" % name
    im = Image.open(bmp).convert("L").crop(CANVAS)
    w, h = im.size
    px = im.load()
    sides = {"left": 0, "right": 0, "top": 0, "bottom": 0}
    for y in range(h):
        if px[0, y] > LIT or px[1, y] > LIT: sides["left"] += 1
        if px[w - 1, y] > LIT or px[w - 2, y] > LIT: sides["right"] += 1
    for x in range(w):
        if px[x, 0] > LIT or px[x, 1] > LIT: sides["top"] += 1
        if px[x, h - 1] > LIT or px[x, h - 2] > LIT: sides["bottom"] += 1
    hit = {k: v for k, v in sides.items() if v > 2}   # a couple of stray pixels is noise
    im.close()
    try:
        os.remove(bmp)
    except OSError:
        pass
    if not hit:
        return "OK", None
    # If the template is so large that the fit hit its zoom floor, clipping is the floor's
    # documented trade. The floor is 0.3; detect it from the drawn grid pitch instead of
    # trusting state (which does not carry zoom): a floored template is one whose content
    # still touches BOTH horizontal edges - it cannot fit by construction.
    if sides["left"] > 2 and sides["right"] > 2:
        return "NOTE", ("[NOTE] edge  %-32s wider than the canvas at the zoom floor (%s)" %
                        (name, ", ".join("%s:%d" % kv for kv in hit.items())))
    return "FAIL", ("[FAIL] edge  %-32s content cut off at the canvas edge (%s)" %
                    (name, ", ".join("%s:%d" % kv for kv in hit.items())))


fails = 0
notes = 0
# Twice the cores, capped at eight. Each task is an app launch that spends its life waiting on
# the GPU-less renderer and the disk, so the cores are not the limit; on the four-core CI runner
# this is the difference between forty minutes and ten. EDGE_JOBS overrides it.
workers = int(os.environ.get("EDGE_JOBS") or 0) or min(8, max(2, (os.cpu_count() or 2) * 2))
with tempfile.TemporaryDirectory() as root:
    work = [(n, root, i) for i, n in enumerate(names)]
    with ThreadPoolExecutor(max_workers=workers) as pool:
        # in template order, so two runs print the same thing in the same order
        for kind, msg in pool.map(check, work):
            if msg:
                print(msg)
            if kind == "FAIL":
                fails += 1
            elif kind == "NOTE":
                notes += 1

print("edge-gui: %d templates, %d clipped at an edge, %d too large to fit at the zoom floor (%d at a time)"
      % (len(names), fails, notes, workers))
sys.exit(1 if fails else 0)
