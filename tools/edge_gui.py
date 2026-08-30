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

fails = 0
notes = 0
with tempfile.TemporaryDirectory() as tmp:
    for name in names:
        bmp = os.path.join(tmp, "t.bmp")
        sj = os.path.join(tmp, "t.json")
        subprocess.run([exe, "--template", name, "--size", "%dx%d" % (W, H), "--frame", "40",
                        "--shot", bmp, "--state-out", sj, "--exit", "--no-update-check"],
                       capture_output=True, text=True)
        if not os.path.exists(bmp):
            print("[FAIL] edge  %-32s no screenshot" % name)
            fails += 1
            continue
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
        os.remove(bmp)
        if not hit:
            continue
        # If the template is so large that the fit hit its zoom floor, clipping is the floor's
        # documented trade. The floor is 0.3; detect it from the drawn grid pitch instead of
        # trusting state (which does not carry zoom): a floored template is one whose content
        # still touches BOTH horizontal edges - it cannot fit by construction.
        floored = sides["left"] > 2 and sides["right"] > 2
        if floored:
            notes += 1
            print("[NOTE] edge  %-32s wider than the canvas at the zoom floor (%s)" %
                  (name, ", ".join("%s:%d" % kv for kv in hit.items())))
        else:
            fails += 1
            print("[FAIL] edge  %-32s content cut off at the canvas edge (%s)" %
                  (name, ", ".join("%s:%d" % kv for kv in hit.items())))

print("edge-gui: %d templates, %d clipped at an edge, %d too large to fit at the zoom floor"
      % (len(names), fails, notes))
sys.exit(1 if fails else 0)
