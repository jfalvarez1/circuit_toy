#!/usr/bin/env python3
"""Every command-line option the app accepts, exercised once - and a guard that keeps it that way.

The suites (--*-test, --*-audit, --*-check, --*-sweep) are guarded already: run_audits.sh refuses
to start if one of them is in no list. The OPTIONS were not guarded by anything. Fourteen of them
were passed by no tool, no gate and no workflow: --xy, --import-spice, --inspect, --scroll, --ss,
--ui-scale, --line-weight, --shot-region, --prop-gap and the rest. They parse argv, they allocate,
they open files, and nothing had ever run them - one of them, --prop-gap, turned out to be a whole
diagnostic suite that nothing executed because its name did not end in "-test".

So this walks main.c for every flag it accepts and requires each to appear in the table below.
Add a flag to the app and this fails until it is exercised, which is the same bargain the suites
already have.

  python tools/cli_smoke.py [--exe build/circuit-playground.exe]

Exit status is the number of failures.
"""
import argparse
import json
import os
import re
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
APP = os.path.join(ROOT, "build", "circuit-playground.exe")
TMP = None


def app(args, timeout=120, env_extra=None):
    env = dict(os.environ, CIRCUIT_TOY_NO_UPDATE="1")
    if env_extra:
        env.update(env_extra)
    try:
        return subprocess.run([APP] + args, capture_output=True, text=True,
                              timeout=timeout, env=env)
    except subprocess.TimeoutExpired:
        return None


def shot(args, name, timeout=120):
    """Run with a screenshot and return its path, or None."""
    out = os.path.join(TMP, name)
    p = app(args + ["--shot", out, "--exit", "--no-update-check"], timeout=timeout)
    return out if (p is not None and os.path.exists(out)) else None


def bmp_size(path):
    with open(path, "rb") as f:
        d = f.read(32)
    if d[:2] != b"BM":
        return None
    w, h = struct.unpack_from("<ii", d, 18)
    return abs(w), abs(h)


def bmp_pixels(path):
    """(w, h, rows of RGB tuples) from the app's SDL_SaveBMP output (bottom-up BGR/BGRA)."""
    with open(path, "rb") as f:
        data = f.read()
    off = struct.unpack_from("<I", data, 10)[0]
    w, h = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    step = bpp // 8
    flip = h > 0
    h = abs(h)
    stride = ((w * step + 3) // 4) * 4
    rows = []
    for y in range(h):
        src = (h - 1 - y) if flip else y
        base = off + src * stride
        row = []
        for x in range(w):
            i = base + x * step
            row.append((data[i + 2], data[i + 1], data[i]))
        rows.append(row)
    return w, h, rows


def write(name, text):
    p = os.path.join(TMP, name)
    with open(p, "w", encoding="utf-8") as f:
        f.write(text)
    return p


# --------------------------------------------------------------------------------------------
# One entry per option. Each returns None when it is satisfied, or a string saying what was wrong.
# --------------------------------------------------------------------------------------------

def c_version():
    p = app(["--version"], timeout=60)
    if not p:
        return "timed out"
    got = (p.stdout or "").strip()
    want = ""
    vh = open(os.path.join(ROOT, "include", "version.h"), encoding="utf-8").read()
    m = re.search(r'APP_VERSION\s+"([^"]+)"', vh)
    if m:
        want = m.group(1)
    if got != want:
        return "printed %r, version.h says %r" % (got, want)
    return None


def c_help():
    p = app(["--help"], timeout=60)
    if not p or p.returncode != 0:
        return "exit %s" % (p.returncode if p else "timeout")
    if "Options:" not in (p.stdout or ""):
        return "no option list printed"
    # the help is the only place a user learns these exist, so it has to mention the ones a
    # person would look for rather than only the ones that were easy to document
    for must in ("--template", "--shot", "--style", "--shot-region", "--size"):
        if must not in p.stdout:
            return "help does not mention %s" % must
    return None


def c_crashlog():
    p = app(["--crashlog"], timeout=60)
    return None if (p and p.returncode == 0) else "exit %s" % (p.returncode if p else "timeout")


def c_size():
    out = shot(["--size", "1024x600", "--ui-scale", "1", "--frame", "5"], "size.bmp")
    if not out:
        return "no screenshot"
    got = bmp_size(out)
    if got != (1024, 600):
        return "asked for 1024x600, got %s" % (got,)
    return None


def c_ui_scale():
    p = app(["--size", "1400x900", "--ui-scale", "2", "--dump-layout", "--no-update-check"])
    if not p:
        return "timed out"
    m = re.search(r"ui_scale\s+([\d.]+)", p.stdout or "")
    if not m:
        return "no ui_scale reported"
    if abs(float(m.group(1)) - 2.0) > 0.001:
        return "asked for 2, reported %s" % m.group(1)
    return None


def c_ss():
    """Supersampling changes how the frame is drawn, not how big it is."""
    a = shot(["--size", "800x600", "--ui-scale", "1", "--ss", "1", "--frame", "5"], "ss1.bmp")
    b = shot(["--size", "800x600", "--ui-scale", "1", "--ss", "3", "--frame", "5"], "ss3.bmp")
    if not a or not b:
        return "no screenshot"
    if bmp_size(a) != bmp_size(b):
        return "--ss changed the window size: %s vs %s" % (bmp_size(a), bmp_size(b))
    return None


def c_line_weight():
    """A different stroke weight has to draw a different picture."""
    a = shot(["--template", "RC Low Pass", "--ui-scale", "1", "--size", "900x700",
              "--line-weight", "0.6", "--frame", "20"], "lw_thin.bmp")
    b = shot(["--template", "RC Low Pass", "--ui-scale", "1", "--size", "900x700",
              "--line-weight", "3.5", "--frame", "20"], "lw_fat.bmp")
    if not a or not b:
        return "no screenshot"
    if open(a, "rb").read() == open(b, "rb").read():
        return "0.6 and 3.5 drew identical frames"
    return None


def c_style():
    """Schematic has to be monochrome inside the canvas; synthwave has to not be."""
    def spread(path):
        w, h, rows = bmp_pixels(path)
        worst = 0
        for y in range(h // 4, 3 * h // 4, 7):
            for x in range(w // 4, 3 * w // 4, 7):
                r, g, b = rows[y][x]
                worst = max(worst, max(r, g, b) - min(r, g, b))
        return worst

    a = shot(["--template", "RC Low Pass", "--ui-scale", "1", "--size", "900x700",
              "--style", "schematic", "--shot-region", "canvas", "--frame", "20"], "st_bw.bmp")
    b = shot(["--template", "RC Low Pass", "--ui-scale", "1", "--size", "900x700",
              "--style", "synthwave", "--shot-region", "canvas", "--frame", "20"], "st_sw.bmp")
    if not a or not b:
        return "no screenshot"
    bw, sw = spread(a), spread(b)
    if bw > 12:
        return "schematic canvas is not monochrome (channel spread %d)" % bw
    if sw < 40:
        return "synthwave canvas has no colour in it (channel spread %d)" % sw
    return None


def c_shot_region():
    w = shot(["--template", "RC Low Pass", "--ui-scale", "1", "--size", "1200x800",
              "--shot-region", "window", "--frame", "20"], "rg_win.bmp")
    c = shot(["--template", "RC Low Pass", "--ui-scale", "1", "--size", "1200x800",
              "--shot-region", "canvas", "--frame", "20"], "rg_can.bmp")
    s = shot(["--template", "RC Low Pass", "--ui-scale", "1", "--size", "1200x800",
              "--shot-region", "canvas+scope", "--frame", "20"], "rg_cs.bmp")
    if not (w and c and s):
        return "no screenshot"
    ww, wh = bmp_size(w)
    cw, ch = bmp_size(c)
    sw, sh = bmp_size(s)
    if not (cw < ww and ch < wh):
        return "canvas %dx%d is not smaller than the window %dx%d" % (cw, ch, ww, wh)
    if sh <= ch:
        return "canvas+scope (%d tall) is no taller than canvas alone (%d)" % (sh, ch)
    return None


def c_frame_shot_exit():
    out = shot(["--template", "RC Low Pass", "--frame", "12", "--ui-scale", "1",
                "--size", "800x600"], "fr.bmp")
    return None if out else "no screenshot at frame 12"


def c_state_out():
    sj = os.path.join(TMP, "state.json")
    p = app(["--template", "RC Low Pass", "--size", "800x600", "--frame", "10",
             "--state-out", sj, "--exit", "--no-update-check"])
    if not p or not os.path.exists(sj):
        return "no state written"
    try:
        d = json.load(open(sj, encoding="utf-8"))
    except Exception as e:
        return "state is not JSON: %s" % e
    n = d.get("components")
    if isinstance(n, list):
        n = len(n)
    if not n:
        return "state lists no components"
    return None


def state_of(args, name, frame=10):
    """The app's own JSON summary after running with these arguments.

    `frame` is when the snapshot is taken, and anything scripted has to happen BEFORE it - a
    click at frame 20 read back from a state written at frame 10 reports the world as it was
    before the click, which looks exactly like a click that did nothing."""
    sj = os.path.join(TMP, name)
    app(args + ["--size", "800x600", "--frame", str(frame), "--state-out", sj,
                "--exit", "--no-update-check"])
    if not os.path.exists(sj):
        return None
    try:
        return json.load(open(sj, encoding="utf-8"))
    except Exception:
        return None


def c_template():
    """Differential, so it needs no knowledge of which enum value a resistor is: an empty canvas
    against the same canvas with a template on it."""
    empty = state_of([], "tpl_empty.json")
    full = state_of(["--template", "RC Low Pass"], "tpl_rc.json")
    if empty is None or full is None:
        return "no state written"
    if empty.get("components"):
        return "the canvas was not empty to begin with (%s parts)" % empty.get("components")
    if not full.get("components", 0) > 3:
        return "RC Low Pass placed %s parts" % full.get("components")
    if not full.get("wires", 0) > 0:
        return "RC Low Pass placed no wires"
    return None


def c_tab():
    a = app(["--size", "1200x800", "--tab", "parts", "--dump-layout", "--no-update-check"])
    b = app(["--size", "1200x800", "--tab", "circuits", "--dump-layout", "--no-update-check"])
    if not a or not b:
        return "timed out"
    pa = [l for l in (a.stdout or "").splitlines() if l.startswith("palette")]
    pb = [l for l in (b.stdout or "").splitlines() if l.startswith("palette")]
    if not pa:
        return "the parts tab lists no palette items"
    if pa == pb:
        return "parts and circuits tabs show the same palette"
    return None


def c_scroll():
    a = app(["--size", "1200x800", "--tab", "parts", "--scroll", "0",
             "--dump-layout", "--no-update-check"])
    b = app(["--size", "1200x800", "--tab", "parts", "--scroll", "120",
             "--dump-layout", "--no-update-check"])
    if not a or not b:
        return "timed out"

    def first_y(p):
        for l in (p.stdout or "").splitlines():
            m = re.match(r"palette\s+\S+\s+-?\d+\s+(-?\d+)", l)
            if m:
                return int(m.group(1))
        return None

    ya, yb = first_y(a), first_y(b)
    if ya is None or yb is None:
        return "no palette items to compare"
    if ya == yb:
        return "--scroll 120 did not move the palette (both at y=%d)" % ya
    return None


def c_record():
    d = os.path.join(TMP, "rec")
    os.makedirs(d, exist_ok=True)
    app(["--template", "RC Low Pass", "--size", "700x520", "--ui-scale", "1", "--frame", "10",
         "--record", d, "4", "2", "--exit", "--no-update-check"], timeout=180)
    n = len([f for f in os.listdir(d) if f.startswith("frame_")])
    if n != 4:
        return "asked for 4 frames, got %d" % n
    return None


def c_keys():
    """Typed input reaches the app: '^' opens Spotlight, which draws over the canvas."""
    a = shot(["--template", "RC Low Pass", "--ui-scale", "1", "--size", "900x700",
              "--frame", "40"], "k_plain.bmp")
    b = shot(["--template", "RC Low Pass", "--ui-scale", "1", "--size", "900x700",
              "--keys", "^", "20", "1", "--frame", "40"], "k_spot.bmp")
    if not a or not b:
        return "no screenshot"
    if open(a, "rb").read() == open(b, "rb").read():
        return "typing did not change anything on screen"
    return None


def c_click_drag():
    """A click selects the part under it; a rubber band selects everything inside it.

    This used to diff two screenshots, which is the weak form of the question and was flaky for a
    good reason: a rubber band over empty canvas leaves nothing behind by the frame the shot is
    taken, so the check was really asking "did anything at all differ" and got a different answer
    depending on where the band happened to land. What a drag is FOR is selection, and the app
    reports how many parts are selected."""
    common = ["--template", "RC Low Pass", "--ui-scale", "1", "--size", "900x700"]
    base = state_of(common, "cd_base.json", frame=60)
    if base is None:
        return "no state written"
    parts = base.get("parts") or []
    if not parts:
        return "the template placed nothing to click on"

    # --click, on a control whose effect is unambiguous: the app says where its buttons are.
    pause = (base.get("buttons") or {}).get("pause")
    if not pause:
        return "the state does not say where the Pause button is"
    px, py = pause[0], pause[1]          # the state gives a centre point, not a rectangle
    clicked = state_of(common + ["--click", "%d,%d,20" % (px, py)], "cd_click.json", frame=60)
    if clicked is None:
        return "no state written for the click"
    if clicked.get("sim_running") == base.get("sim_running"):
        return "clicking Pause left the simulation %s" % (
            "running" if base.get("sim_running") else "stopped")

    # --drag: a rubber band selects everything inside it.
    #
    # Banded around the parts that are actually ON the canvas. The bounding box of all of them
    # starts left of the palette edge, so a band drawn from there begins its drag on the palette
    # and never rubber-bands anything - which reads as "drag does not work" and is really "the
    # test started the drag in the wrong place".
    CANVAS_L, CANVAS_T = 210, 80
    inside = [p for p in parts if p["x"] >= CANVAS_L + 10 and p["y"] >= CANVAS_T + 10]
    if len(inside) < 2:
        return "only %d parts are clear of the panels; nothing to band" % len(inside)
    xs = [p["x"] for p in inside]
    ys = [p["y"] for p in inside]
    dragged = state_of(common + ["--drag", "%d,%d,%d,%d,20" % (
        max(min(xs) - 30, CANVAS_L), max(min(ys) - 30, CANVAS_T),
        max(xs) + 30, max(ys) + 30)], "cd_drag.json", frame=60)
    if dragged is None:
        return "no state written for the drag"
    if dragged.get("selected_count", 0) < 2:
        return "a rubber band around %d parts selected %s of them" % (
            len(inside), dragged.get("selected_count"))
    return None


def c_netlist():
    f = write("n.txt", "R1 in out 1k\nV1 in 0 5\nR2 out 0 2k\n")
    got = state_of(["--netlist", f], "n.json")
    if got is None:
        return "no state written"
    if not got.get("components", 0) >= 3:
        return "a written-down circuit of three parts placed %s" % got.get("components")
    return None


def c_xy():
    f = write("xy.txt", "\n".join("%f %f" % (i / 50.0, (i % 20) / 10.0) for i in range(60)))
    out = shot(["--template", "X-Y", "--xy", f, "--size", "900x700", "--ui-scale", "1",
                "--frame", "30"], "xy.bmp")
    return None if out else "no screenshot with a coordinate file loaded"


def c_sketch():
    f = write("s.ino", "void setup(){pinMode(13,OUTPUT);}\nvoid loop(){digitalWrite(13,HIGH);delay(100);digitalWrite(13,LOW);delay(100);}\n")
    out = shot(["--template", "Blink", "--sketch", f, "--size", "900x700", "--ui-scale", "1",
                "--frame", "30"], "sk.bmp")
    return None if out else "no screenshot with a sketch loaded"


def c_import_spice():
    f = write("m.cir", ".SUBCKT SMOKE 1 2\nR1 1 2 1k\n.ENDS\n")
    p = app(["--import-spice", f, "--size", "800x600", "--frame", "10",
             "--exit", "--no-update-check"])
    if not p:
        return "timed out"
    if p.returncode != 0:
        return "exit %d: %s" % (p.returncode, (p.stderr or "").strip()[:100])
    return None


def c_inspect():
    p = app(["--inspect", "\x01nothing\x01", "--size", "800x600", "--frame", "5",
             "--exit", "--no-update-check"])
    if not p:
        return "timed out"
    if p.returncode != 0:
        return "asking for a block that does not exist should not be fatal (exit %d)" % p.returncode
    return None


def c_popout():
    out = os.path.join(TMP, "po.bmp")
    app(["--template", "RC Low Pass", "--popout", "--size", "900x700", "--ui-scale", "1",
         "--frame", "30", "--shot", out, "--exit", "--no-update-check"], timeout=180)
    scope = out[:-4] + "_scope.bmp"
    if not os.path.exists(scope):
        return "--popout wrote no _scope image beside the shot"
    return None


def c_dump_layout():
    p = app(["--size", "1400x900", "--dump-layout", "--no-update-check"])
    if not p:
        return "timed out"
    n = len([l for l in (p.stdout or "").splitlines() if l.startswith("button ")])
    if n < 16:
        return "only %d toolbar buttons reported" % n
    return None


def c_shard():
    """The shards must PARTITION the work: together exactly the whole, separately none of it twice.

    That is the only property sharding has to have, and the one whose failure is invisible - a
    shard that skips a template drops coverage without anything reporting less than success.
    Checked on --bounce-test, the only suite in this binary that shards, narrowed with BOUNCE_ONLY
    so it costs seconds rather than minutes.
    """
    env = {"BOUNCE_ONLY": "RC"}

    def judged(args):
        p = app(args + ["--bounce-test"], timeout=600, env_extra=env)
        if not p:
            return None
        m = re.search(r"bounce-test:\s+(\d+)\s+templates judged", p.stdout or "")
        return int(m.group(1)) if m else None

    whole = judged([])
    a = judged(["--shard", "0/2"])
    b = judged(["--shard", "1/2"])
    if whole is None or a is None or b is None:
        return "could not read the counts back"
    if whole == 0:
        return "the filter matched no templates, so this proves nothing"
    if a + b != whole:
        return "shards 0/2 and 1/2 judged %d + %d, but the whole is %d" % (a, b, whole)
    if a == 0 or b == 0:
        return "one shard of two got everything (%d and %d)" % (a, b)

    # and it has to refuse nonsense rather than silently running a wrong subset
    bad = app(["--shard", "9/4", "--bounce-test"], timeout=120, env_extra=env)
    if not bad:
        return "--shard 9/4 did not terminate"
    if bad.returncode == 0:
        return "--shard 9/4 was accepted"
    return None


def c_no_update_check():
    p = app(["--size", "800x600", "--frame", "5", "--exit", "--no-update-check"])
    return None if (p and p.returncode == 0) else "exit %s" % (p.returncode if p else "timeout")


def c_no_auto_update():
    p = app(["--size", "800x600", "--frame", "5", "--exit", "--no-update-check",
             "--no-auto-update"])
    return None if (p and p.returncode == 0) else "exit %s" % (p.returncode if p else "timeout")


def c_update_check():
    """Accepted and terminating. CIRCUIT_TOY_NO_UPDATE keeps it off the network."""
    p = app(["--update-check"], timeout=90)
    if not p:
        return "did not terminate"
    return None


def c_update_now():
    p = app(["--update-check", "--update-now"], timeout=90)
    if not p:
        return "did not terminate"
    return None


def c_prop_gap():
    """It is a suite, not an option - it just does not look like one. Nothing ran it for that
    reason alone. At minimum it has to run and report on every creatable type."""
    p = app(["--prop-gap"], timeout=300)
    if not p:
        return "timed out"
    m = re.search(r"prop-gap:\s+(\d+)\s+creatable types", p.stdout or "")
    if not m:
        return "printed no summary"
    if int(m.group(1)) < 100:
        return "only %s types examined" % m.group(1)
    # It reports a ratchet now, so its exit code means something. Reading only the summary line
    # would repeat the mistake the ratchet exists to fix.
    if p.returncode != 0:
        tail = [l for l in (p.stdout or "").splitlines() if "FAIL" in l]
        return tail[0].strip() if tail else "exit %d" % p.returncode
    return None


CASES = [
    ("--version", c_version), ("--help", c_help), ("--crashlog", c_crashlog),
    ("--size", c_size), ("--ui-scale", c_ui_scale), ("--ss", c_ss),
    ("--line-weight", c_line_weight), ("--style", c_style), ("--shot-region", c_shot_region),
    ("--shot", c_frame_shot_exit), ("--frame", c_frame_shot_exit), ("--exit", c_frame_shot_exit),
    ("--state-out", c_state_out), ("--template", c_template), ("--tab", c_tab),
    ("--scroll", c_scroll), ("--record", c_record), ("--keys", c_keys),
    ("--click", c_click_drag), ("--drag", c_click_drag), ("--netlist", c_netlist),
    ("--xy", c_xy), ("--sketch", c_sketch), ("--import-spice", c_import_spice),
    ("--inspect", c_inspect), ("--popout", c_popout), ("--dump-layout", c_dump_layout),
    ("--shard", c_shard), ("--no-update-check", c_no_update_check),
    ("--no-auto-update", c_no_auto_update), ("--update-check", c_update_check),
    ("--update-now", c_update_now), ("--prop-gap", c_prop_gap),
]

SUITE = re.compile(r"-(test|audit|check|sweep)$")


def declared_flags():
    src = open(os.path.join(ROOT, "src", "main.c"), encoding="utf-8", errors="replace").read()
    return sorted(set(re.findall(r'strcmp\(argv\[i\],\s*"(--[a-z0-9-]+)"', src)))


def main():
    global APP, TMP
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=None)
    ap.add_argument("--only", default=None)
    a = ap.parse_args()
    if a.exe:
        APP = os.path.abspath(a.exe)
    if not os.path.exists(APP):
        print("cli-smoke: no binary at %s" % APP)
        return 2

    fails = 0

    # The guard: every option the app accepts has to be exercised by something here. Suites are
    # excluded because run_audits.sh already refuses to run with one of them unlisted - except
    # --update-check, which is an option that happens to end in "-check".
    covered = {f for f, _ in CASES}
    missing = [f for f in declared_flags()
               if f not in covered and (not SUITE.search(f) or f == "--update-check")]
    if missing:
        print("cli-smoke: FAIL these options are accepted by the app and exercised by nothing:")
        for f in missing:
            print("           %s" % f)
        print("cli-smoke: add a case for each to tools/cli_smoke.py.")
        fails += len(missing)

    with tempfile.TemporaryDirectory(prefix="cli_smoke_") as tmp:
        TMP = tmp
        seen = set()
        for flag, fn in CASES:
            if a.only and a.only not in flag:
                continue
            key = fn.__name__
            if key in seen:
                print("%-18s  ok   (with %s)" % (flag, key))
                continue
            seen.add(key)
            try:
                why = fn()
            except Exception as e:
                why = "raised %s: %s" % (type(e).__name__, e)
            if why:
                print("%-18s FAIL  %s" % (flag, why))
                fails += 1
            else:
                print("%-18s  ok" % flag)

    print("\ncli-smoke: %d options exercised, %d failures" % (len(CASES), fails))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
