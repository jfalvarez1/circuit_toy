"""Generate README screenshots and GIFs from a separate, scripted app instance.

Runs build/circuit-playground.exe with its automation flags (--template, --shot, --record)
so nothing touches the user's own window, then converts the BMP frames with Pillow.

    python tools/make_media.py            # everything
    python tools/make_media.py shots      # PNG screenshots only
    python tools/make_media.py gifs       # GIFs only

Outputs: screenshots/auto/<name>.png and gifs/auto_<name>.gif (existing gifs are never
overwritten; the auto_ prefix keeps them apart).
"""
import os, subprocess, sys, shutil, glob
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = os.path.join(ROOT, "build", "circuit-playground.exe")
SHOT_DIR = os.path.join(ROOT, "screenshots", "auto")
GIF_DIR = os.path.join(ROOT, "gifs")
SIZE = "1400x900"

# name, template (short name), frame to capture, left tab
SHOTS = [
    ("rc_lowpass_sweep", "LP", 220, "circuits"),
    ("wien_oscillator", "Wien", 140, "circuits"),
    ("line_345kv", "345kV", 120, "circuits"),
    ("grid_chain", "Grid", 120, "circuits"),
    ("ferranti", "Ferr", 120, "circuits"),
    ("line_model_ladder", "Ladder", 120, "circuits"),
    ("tesla_coil", "Tesla", 300, "circuits"),
    ("relaxation_osc", "RelOsc", 140, "circuits"),
    ("voltage_doubler", "Dblr", 200, "circuits"),
    ("parts_palette", None, 30, "parts"),
]
# name, template, first frame, frames, every
GIFS = [
    ("rc_lowpass_sweep", "LP", 60, 36, 3),
    ("tesla_coil", "Tesla", 120, 30, 2),
    ("grid_chain", "Grid", 60, 24, 3),
    ("relaxation_osc", "RelOsc", 60, 24, 2),
]


def run(args, timeout=120):
    cmd = [EXE, "--size", SIZE, "--exit"] + args
    r = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=timeout)
    if r.returncode not in (0,):
        print("  ! exit", r.returncode, r.stderr[-300:])
    return r


def shots():
    os.makedirs(SHOT_DIR, exist_ok=True)
    for name, tpl, frame, tab in SHOTS:
        bmp = os.path.join(SHOT_DIR, name + ".bmp")
        args = ["--tab", tab, "--frame", str(frame), "--shot", bmp]
        if tpl:
            args += ["--template", tpl]
        print("shot", name)
        run(args)
        if os.path.exists(bmp):
            Image.open(bmp).save(os.path.join(SHOT_DIR, name + ".png"), optimize=True)
            os.remove(bmp)


def gifs():
    os.makedirs(GIF_DIR, exist_ok=True)
    for name, tpl, first, frames, every in GIFS:
        tmp = os.path.join(SHOT_DIR, "_rec_" + name)
        shutil.rmtree(tmp, ignore_errors=True)
        os.makedirs(tmp)
        print("gif", name)
        run(["--tab", "circuits", "--template", tpl, "--frame", str(first), "--record", tmp, str(frames), str(every)], timeout=300)
        files = sorted(glob.glob(os.path.join(tmp, "frame_*.bmp")))
        if not files:
            print("  ! no frames"); continue
        imgs = []
        for f in files:
            im = Image.open(f).convert("RGB")
            im = im.resize((im.width // 2, im.height // 2), Image.LANCZOS)
            imgs.append(im.quantize(colors=64, method=Image.Quantize.MEDIANCUT))
        out = os.path.join(GIF_DIR, "auto_" + name + ".gif")
        imgs[0].save(out, save_all=True, append_images=imgs[1:], duration=90, loop=0, optimize=True)
        shutil.rmtree(tmp, ignore_errors=True)
        print("  ->", out, os.path.getsize(out) // 1024, "KB")


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "all"
    if not os.path.exists(EXE):
        sys.exit("build the app first: meson compile -C build")
    if what in ("all", "shots"):
        shots()
    if what in ("all", "gifs"):
        gifs()
