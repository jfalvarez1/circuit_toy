"""Do the keyboard shortcuts do what they say?

The app is driven for real - a template placed, keys pressed through the event loop - and then
asked what it is: how many parts, which tool, how deep the undo stack. Nothing here reads pixels,
because a count is a fact and a picture is an argument.

Shortcuts that open a native file dialog (Ctrl+S, Ctrl+O) are not driven: the dialog is modal
and a scripted run would sit in it until it was killed.

    python tools/keys_gui.py build/circuit-playground.exe
"""
import subprocess, sys, os, json, tempfile

exe = sys.argv[1] if len(sys.argv) > 1 else "build/circuit-playground.exe"
if os.path.sep not in exe and "/" not in exe:
    exe = os.path.join(".", exe)
exe = os.path.abspath(exe)
if not os.path.exists(exe):
    print("keys-gui: no binary at %s" % exe)
    sys.exit(1)

TEMPLATE = "RC Low Pass"
PAUSE = "300,25,15"          # the Pause button: several things are refused while running
W, H = 1200, 800
COMP_RESISTOR = 1            # ComponentType ordinal, stable since the enum is append-only
# A_PART is not a constant any more. It was "the resistor is at 300,300", and that held only
# until anything about template placement moved - widening the fit margin by 40 px shifted every
# circuit and five of these checks failed at once, all saying "selected 0". The app now reports
# each part's screen position in --state-out, so the run asks rather than remembers.
A_PART = None


def state(tmp, name, keys=None, clicks=(), frame=80):
    """Run the app, do things to it, and read back what it says it is."""
    out = os.path.join(tmp, name + ".json")
    cmd = [exe, "--template", TEMPLATE, "--tab", "parts", "--size", "%dx%d" % (W, H),
           "--frame", str(frame), "--shot", os.path.join(tmp, name + ".bmp"),
           "--state-out", out, "--exit", "--no-update-check"]
    for c in clicks:
        cmd += ["--click", c]
    if keys:
        cmd += ["--keys", keys, "45", "3"]
    subprocess.run(cmd, capture_output=True, text=True)
    if not os.path.exists(out):
        return None
    with open(out) as f:
        return json.load(f)


fails = 0
with tempfile.TemporaryDirectory() as tmp:
    base = state(tmp, "base", clicks=[PAUSE])
    if base is None:
        print("[FAIL] keys-gui  the app wrote no state at all")
        sys.exit(1)
    parts = base.get("parts") or []
    spot = next((p for p in parts if p.get("type") == COMP_RESISTOR), None)
    if spot is None:
        print("[FAIL] keys-gui  the state lists no resistor to aim at")
        sys.exit(1)
    A_PART = "%d,%d,25" % (spot["x"], spot["y"])

    # (label, keys, clicks, what it should have done)
    checks = [
        ("Ctrl+A selects everything", "~a", [PAUSE],
         lambda s: s["selected_count"] >= base["components"] - 1),
        ("Ctrl+D duplicates the selected part", "~d", [PAUSE, A_PART],
         lambda s: s["components"] == base["components"] + 1),
        ("Ctrl+Z takes the duplicate back", "~d~z", [PAUSE, A_PART],
         lambda s: s["components"] == base["components"]),
        ("Ctrl+Y puts it back again", "~d~z~y", [PAUSE, A_PART],
         lambda s: s["components"] == base["components"] + 1),
        ("Ctrl+R rotates the selected part", "~r", [PAUSE, A_PART],
         lambda s: s["selected_rotation"] not in (-1, 0)),
        ("Ctrl+Z takes a rotation back", "~r~z", [PAUSE, A_PART],
         lambda s: s["selected_rotation"] in (-1, 0)),
        ("Ctrl+C then Ctrl+V adds a part", "~c~v", [PAUSE, A_PART],
         lambda s: s["components"] == base["components"] + 1),
        ("an edit leaves something to undo", "~d", [PAUSE, A_PART],
         lambda s: s["undo_depth"] > 0),
        ("Ctrl+A then Delete empties the canvas", "~a#", [PAUSE],
         lambda s: s["components"] == 0),
        ("...and one Ctrl+Z brings all of it back", "~a#~z", [PAUSE],
         lambda s: s["components"] == base["components"]),
    ]

    for label, keys, clicks, ok in checks:
        s = state(tmp, label.split()[0].replace("+", "") + str(len(label)), keys=keys, clicks=clicks)
        if s is None:
            print("[FAIL] keys-gui  %-40s no state written" % label)
            fails += 1
        elif not ok(s):
            print("[FAIL] keys-gui  %-40s parts %d->%d, selected %d rot %d, undo %d"
                  % (label, base["components"], s["components"], s["selected_count"],
                     s["selected_rotation"], s["undo_depth"]))
            fails += 1
        else:
            print("[ OK ] keys-gui  %s" % label)

print("\nkeys-gui: %d shortcuts checked, %d that do not do what they say" % (len(checks), fails))
sys.exit(1 if fails else 0)
