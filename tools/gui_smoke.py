#!/usr/bin/env python3
"""GUI smoke test: drive the built app the way a user does, and look at what it drew.

template_smoke.exe links the simulation and asks it questions. This does not: it launches
circuit-playground.exe, lets it place a template and run, takes the screenshot the app itself
saves, and reads the pixels. That is the only way to catch the class of bug where the physics
is right and the user still sees an empty screen - a probe on the wrong node, a preset that
puts the trace off-screen, a panel drawn over the canvas.

It also presses things. --click and --drag inject real SDL mouse events, so the Pan tool, the
zoom buttons, a switch and a probe are exercised through the same path a pointer takes.

  python tools/gui_smoke.py                 # every template, plus the interaction checks
  python tools/gui_smoke.py --quick         # a spread of 12 templates
  python tools/gui_smoke.py --only Pierce   # substring match on the template name
  python tools/gui_smoke.py --keep          # leave the screenshots in the output directory

Exit status is the number of failures.
"""

import argparse
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
APP = os.path.join(ROOT, "build", "circuit-playground.exe")
SMOKE = os.path.join(ROOT, "build", "tools", "template_smoke.exe")

WIN_W, WIN_H = 1600, 1000
SHOT_FRAME = 90

# The window is laid out as: left palette | canvas | properties over the scope on the right.
PALETTE_W = 160
SCOPE_X0, SCOPE_Y0 = 1190, 270          # graticule, inside the right-hand column
SCOPE_X1, SCOPE_Y1 = 1590, 600
CANVAS = (PALETTE_W + 10, 60, 1180, 900)

# Trace colours are PROBE_COLORS in include/ui.h; the graticule is dark green, the background
# near-black. "Something is on the screen" means pixels that are none of those.
BG_MAX = 60


def read_bmp(path):
    """Return (w, h, RGB bytes). Pillow if it is installed, otherwise a minimal BMP reader -
    the app writes SDL_SaveBMP output, which is bottom-up 24 or 32 bit BGR(A)."""
    try:
        from PIL import Image
        im = Image.open(path).convert("RGB")
        return im.width, im.height, im.tobytes()
    except ImportError:
        pass
    with open(path, "rb") as f:
        data = f.read()
    if data[:2] != b"BM":
        raise ValueError("not a BMP: %s" % path)
    off = struct.unpack_from("<I", data, 10)[0]
    w, h = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    if bpp not in (24, 32):
        raise ValueError("unexpected %d bpp in %s" % (bpp, path))
    step = bpp // 8
    stride = ((w * step) + 3) & ~3
    rows = []
    for row in range(abs(h)):
        src_row = row if h < 0 else abs(h) - 1 - row
        base = off + src_row * stride
        line = data[base:base + w * step]
        rows.append(bytes(b for i in range(w) for b in (line[i*step+2], line[i*step+1], line[i*step])))
    return w, abs(h), b"".join(rows)


class Shot:
    def __init__(self, path):
        self.w, self.h, self.px = read_bmp(path)

    def at(self, x, y):
        i = (y * self.w + x) * 3
        return self.px[i], self.px[i + 1], self.px[i + 2]

    def lit(self, box, step=2, threshold=BG_MAX):
        """Count pixels brighter than the background inside a box."""
        x0, y0, x1, y1 = box
        x1 = min(x1, self.w); y1 = min(y1, self.h)
        n = 0
        for y in range(max(0, y0), max(0, y1), step):
            for x in range(max(0, x0), max(0, x1), step):
                r, g, b = self.at(x, y)
                if r + g + b > threshold * 3:
                    n += 1
        return n

    def trace_pixels(self, box, step=2):
        """Pixels that look like a scope trace: saturated, and not the green graticule."""
        x0, y0, x1, y1 = box
        x1 = min(x1, self.w); y1 = min(y1, self.h)
        n = 0
        for y in range(max(0, y0), max(0, y1), step):
            for x in range(max(0, x0), max(0, x1), step):
                r, g, b = self.at(x, y)
                if max(r, g, b) < 150:
                    continue
                if g > 120 and r < 90 and b < 90:      # graticule / grid green
                    continue
                n += 1
        return n

    def is_flat(self):
        first = self.at(0, 0)
        for y in range(0, self.h, 37):
            for x in range(0, self.w, 41):
                if self.at(x, y) != first:
                    return False
        return True


def run_app(args, out, timeout=90):
    cmd = [APP, "--size", "%dx%d" % (WIN_W, WIN_H), "--frame", str(SHOT_FRAME),
           "--shot", out, "--exit", "--no-update-check"] + args
    env = dict(os.environ, CIRCUIT_TOY_NO_UPDATE="1")
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
    except subprocess.TimeoutExpired:
        return None, "timed out after %ds" % timeout
    if not os.path.exists(out):
        return None, "no screenshot (exit %d) %s" % (p.returncode, (p.stderr or "").strip()[:120])
    return Shot(out), None


def button_layout(extra=None, kind="button"):
    """Where the controls actually are, in the device pixels a click is delivered in.

    Asked of the app with --dump-layout, at the same window size this gate drives it at, so the
    numbers are whatever the layout actually produced - including the scaling the app applies on
    a tall display, which is the part that is easiest to get wrong by hand. `kind` selects the
    toolbar buttons or the palette items."""
    cmd = [APP, "--size", "%dx%d" % (WIN_W, WIN_H), "--dump-layout", "--no-update-check"]
    cmd += extra or []
    env = dict(os.environ, CIRCUIT_TOY_NO_UPDATE="1")
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=90, env=env)
    except subprocess.TimeoutExpired:
        return {}
    out = {}
    for line in (p.stdout or "").splitlines():
        m = re.match(kind + r"\s+(\S+)\s+(-?\d+)\s+(-?\d+)\s+(\d+)\s+(\d+)", line)
        if m:
            out[m.group(1)] = tuple(int(v) for v in m.groups()[1:])
    return out


def template_names():
    p = subprocess.run([SMOKE], capture_output=True, text=True, timeout=1800)
    names = []
    for line in p.stdout.splitlines():
        m = re.match(r"\[(?: OK |FAIL)\] (.+?)\s+comps=", line)
        if m:
            names.append(m.group(1).strip())
    return names


def check_template(name, outdir, keep):
    out = os.path.join(outdir, re.sub(r"[^A-Za-z0-9]+", "_", name)[:40] + ".bmp")
    shot, err = run_app(["--template", name], out)
    if err:
        return 1, "%-34s FAIL  %s" % (name, err)
    problems = []
    if shot.is_flat():
        problems.append("the whole window is one colour")
    if shot.lit(CANVAS) < 200:
        problems.append("canvas looks empty")
    tr = shot.trace_pixels((SCOPE_X0, SCOPE_Y0, SCOPE_X1, SCOPE_Y1))
    if tr < 20:
        problems.append("no trace on the scope (%d lit)" % tr)
    if not keep:
        try:
            os.remove(out)
        except OSError:
            pass
    if problems:
        return 1, "%-34s FAIL  %s" % (name, "; ".join(problems))
    return 0, "%-34s  ok   trace=%d" % (name, tr)


def interaction_checks(outdir, keep):
    """Press the things a user presses, and check the picture changed the way it should."""
    fails, lines = 0, []
    base_out = os.path.join(outdir, "_base.bmp")
    base, err = run_app(["--template", "RC Low Pass"], base_out)
    if err:
        return 1, ["interaction                        FAIL  base shot: %s" % err]

    def shot_with(args, tag):
        o = os.path.join(outdir, "_%s.bmp" % tag)
        s, e = run_app(["--template", "RC Low Pass"] + args, o)
        return s, e, o

    # Where the toolbar buttons actually are, asked of the app rather than typed in here.
    #
    # These were three numbers in this source with a comment above them saying the coordinates
    # come from ui_update_layout and should be read rather than guessed. They were guessed. Two
    # of the three landed in the gap between buttons or on the neighbour, so the checks had been
    # failing since the day they were written - and nothing ran this gate, so nobody saw. The
    # toolbar has since been made to lay itself out against the window width, which would have
    # invalidated hardcoded numbers all over again.
    layout = button_layout()
    palette = button_layout(["--tab", "parts"], "palette")
    if not layout:
        return 1, ["interaction                        FAIL  --dump-layout returned nothing"]

    def centre(name):
        x, y, w, h = layout[name]
        return "%d,%d,40" % (x + w // 2, y + h // 2)

    checks = [
        ("zoom-in",  ["--click", centre("zoom_in")],  "clicking + changes the canvas"),
        ("zoom-out", ["--click", centre("zoom_out")], "clicking - changes the canvas"),
        ("fit",      ["--click", centre("zoom_fit")], "clicking Fit changes the canvas"),
    ]
    for tag, args, what in checks:
        s, e, path = shot_with(args, tag)
        if e:
            fails += 1; lines.append("%-34s FAIL  %s" % (tag, e)); continue
        same = s.lit(CANVAS) == base.lit(CANVAS)
        if same:
            fails += 1; lines.append("%-34s FAIL  %s: nothing moved" % (tag, what))
        else:
            lines.append("%-34s  ok   %s" % (tag, what))
        if not keep:
            try: os.remove(path)
            except OSError: pass

    # The Pan tool: select it in the palette, then drag the canvas and check it moved.
    #
    # The coordinate here used to be 40,205, which is the DELETE tool - one row up. So this check
    # picked up the wrong tool, dragged across empty canvas with it, saw nothing move and
    # reported the Pan tool broken. It is not, and never was. Ask where the tool is.
    pan = palette.get("Pan")
    if not pan:
        return fails + 1, lines + ["pan-tool                           FAIL  no Pan tool in the palette dump"]
    px, py, pw, ph = pan
    s, e, path = shot_with(["--tab", "parts",
                            "--click", "%d,%d,30" % (px + pw // 2, py + ph // 2),
                            "--drag", "600,500,900,560,50"], "pan")
    if e:
        fails += 1; lines.append("%-34s FAIL  %s" % ("pan-tool", e))
    else:
        if s.lit(CANVAS) == base.lit(CANVAS):
            fails += 1; lines.append("%-34s FAIL  Pan tool: the canvas did not move" % "pan-tool")
        else:
            lines.append("%-34s  ok   Pan tool drags the canvas" % "pan-tool")
        if not keep:
            try: os.remove(path)
            except OSError: pass

    # A switch has to visibly change when it is clicked. N-1 Contingency has one breaker, and
    # the template's own label says to open it.
    o1 = os.path.join(outdir, "_sw_before.bmp")
    o2 = os.path.join(outdir, "_sw_after.bmp")
    before, e1 = run_app(["--template", "N-1 Contingency"], o1)
    after, e2 = run_app(["--template", "N-1 Contingency", "--click", "563,466,45"], o2)
    if e1 or e2:
        fails += 1; lines.append("%-34s FAIL  %s" % ("switch-click", e1 or e2))
    else:
        d = abs(before.lit((400, 400, 800, 560)) - after.lit((400, 400, 800, 560)))
        if d == 0:
            lines.append("%-34s  --   no change at the breaker (position is approximate)" % "switch-click")
        else:
            lines.append("%-34s  ok   the breaker area changed by %d px" % ("switch-click", d))
    for p in (o1, o2, base_out):
        if not keep:
            try: os.remove(p)
            except OSError: pass
    return fails, lines


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=None,
                    help="the app to drive (default build/circuit-playground.exe). The battery "
                         "runs against whichever build tree it was given, so it has to be able "
                         "to say which one.")
    ap.add_argument("--smoke", default=None, help="template_smoke, for the template list")
    ap.add_argument("--quick", action="store_true", help="a spread of 12 templates")
    ap.add_argument("--only", default=None, help="substring match on the template name")
    ap.add_argument("--keep", action="store_true", help="keep the screenshots")
    ap.add_argument("--outdir", default=None)
    a = ap.parse_args()

    global APP, SMOKE
    if a.exe:
        APP = os.path.abspath(a.exe)
        if not a.smoke:
            SMOKE = os.path.join(os.path.dirname(APP), "tools", "template_smoke.exe")
    if a.smoke:
        SMOKE = os.path.abspath(a.smoke)

    if not os.path.exists(APP):
        print("build the app first: meson compile -C build"); return 2
    outdir = a.outdir or tempfile.mkdtemp(prefix="gui_smoke_")
    os.makedirs(outdir, exist_ok=True)
    print("GUI smoke: driving %s at %dx%d, screenshots in %s\n" % (os.path.basename(APP), WIN_W, WIN_H, outdir))

    names = template_names()
    if a.only:
        names = [n for n in names if a.only.lower() in n.lower()]
    elif a.quick:
        names = names[:: max(1, len(names) // 12)][:12]

    fails = 0
    for n in names:
        f, line = check_template(n, outdir, a.keep)
        fails += f
        print(line)

    if not a.only:
        print()
        f, lines = interaction_checks(outdir, a.keep)
        fails += f
        for l in lines:
            print(l)

    print("\ngui-smoke: %d templates driven through the app, %d failures" % (len(names), fails))
    if not a.keep and not a.outdir:
        shutil.rmtree(outdir, ignore_errors=True)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
