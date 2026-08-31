"""Delete something in the running app, press Ctrl+Z, and check the canvas is what it was.

Everything else about undo is checked by driving the circuit functions directly (--undo-test).
This drives the app: a real click with the delete tool, a real Ctrl+Z through the key handler,
and a comparison of what was actually drawn. It is the only check that the wiring between the
tool, the key and the undo stack is connected at all.

    python tools/undo_gui.py build/circuit-playground.exe
"""
import subprocess, sys, os, tempfile

try:
    from PIL import Image, ImageChops
except ImportError:
    print("undo-gui: needs pillow; skipped")
    sys.exit(0)

exe = sys.argv[1] if len(sys.argv) > 1 else "build/circuit-playground.exe"
if os.path.sep not in exe and "/" not in exe:
    exe = os.path.join(".", exe)
exe = os.path.abspath(exe)
if not os.path.exists(exe):
    print("undo-gui: no binary at %s" % exe)
    sys.exit(1)

W, H = 1200, 800
CANVAS = (200, 60, 980, 700)          # the drawing area, away from panels and the status bar
TEMPLATE = "RC Low Pass"
fails = 0


def run(out, *extra):
    # --tab parts so the Tools box is on screen whatever the saved preference is, and a last
    # click on empty canvas so the pointer is not left hovering a part: the app draws a readout
    # under the cursor, and that is a difference in the picture that is nothing to do with undo
    cmd = [exe, "--template", TEMPLATE, "--tab", "parts", "--size", "%dx%d" % (W, H),
           "--frame", "70", "--shot", out, "--exit", "--no-update-check",
           "--click", "900,650,60"] + list(extra)
    subprocess.run(cmd, capture_output=True, text=True)
    return Image.open(out).crop(CANVAS).convert("RGB") if os.path.exists(out) else None


def differs(a, b):
    return ImageChops.difference(a, b).getbbox() is not None


with tempfile.TemporaryDirectory() as tmp:
    # Where the app says its buttons and its parts are. Both used to be constants here; both
    # broke the day something above them moved.
    import json
    sp = os.path.join(tmp, "aim.json")
    subprocess.run([exe, "--template", TEMPLATE, "--tab", "parts", "--size", "%dx%d" % (W, H),
                    "--frame", "40", "--shot", os.path.join(tmp, "aim.bmp"),
                    "--state-out", sp, "--exit", "--no-update-check"],
                   capture_output=True, text=True)
    aim = {}
    if os.path.exists(sp):
        with open(sp) as fh:
            aim = json.load(fh)
    btn = (aim.get("buttons") or {}).get("pause")
    if not btn:
        print("[FAIL] undo-gui  the state does not say where the Pause button is")
        sys.exit(1)
    PAUSE = "%d,%d,15" % (btn[0], btn[1])

    plain = run(os.path.join(tmp, "a.bmp"), "--click", PAUSE)
    if plain is None:
        print("[FAIL] undo-gui  the app drew nothing at all")
        sys.exit(1)

    # Pause first: the app refuses to delete while the simulation is running, and a scripted
    # run starts it. Then the Delete tool, then a part the app itself locates: --state-out lists
    # each part's screen position, because a hardcoded "the resistor is at 300,300" broke the day
    # the fit-on-place margin moved every template by 40 px.
    spot = next((p for p in (aim.get("parts") or []) if p.get("type") == 1), None)
    if spot is None:
        print("[FAIL] undo-gui  the state lists no resistor to aim at")
        sys.exit(1)
    DELETE_TOOL, A_PART = "39,184,20", "%d,%d,30" % (spot["x"], spot["y"])
    deleted = run(os.path.join(tmp, "b.bmp"), "--click", PAUSE, "--click", DELETE_TOOL,
                  "--click", A_PART)
    if deleted is None or not differs(plain, deleted):
        print("[FAIL] undo-gui  clicking the delete tool and a part changed nothing on the canvas")
        print("                 (the click misses, or deleting is broken - either way this "
              "test cannot say anything about undo)")
        fails += 1
    else:
        # the same again, and then Ctrl+Z
        undone = run(os.path.join(tmp, "c.bmp"), "--click", PAUSE, "--click", DELETE_TOOL,
                     "--click", A_PART, "--keys", "~z", "45", "1")
        if undone is None:
            print("[FAIL] undo-gui  the app drew nothing after the undo")
            fails += 1
        elif differs(plain, undone):
            print("[FAIL] undo-gui  Ctrl+Z did not put the canvas back to what it was")
            fails += 1
        else:
            print("[ OK ] undo-gui  a part deleted with the tool comes back on Ctrl+Z")

print("\nundo-gui: %d failed" % fails)
sys.exit(1 if fails else 0)
