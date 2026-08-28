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
GIF_SIZE = SIZE           # full app resolution (1400x900), same layout as the screenshots

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
    ("relay_overcurrent", "50/51", 130, "circuits"),
    ("relay_differential", "87L", 200, "circuits"),
    ("relay_distance", "21Z1", 200, "circuits"),
    ("breaker_failure", "50BF", 300, "circuits"),
    ("sil_loading", "SIL", 120, "circuits"),
    ("series_compensation", "SerC", 120, "circuits"),
    ("line_765kv", "765kV", 120, "circuits"),
    ("three_phase_balanced", "3phY", 120, "circuits"),
    ("six_pulse_rectifier", "6Pulse", 120, "circuits"),
    ("triangle_square_gen", "TriSq", 160, "circuits"),
    ("function_generator", "FuncGn", 160, "circuits"),
    ("colpitts", "Colpit", 200, "circuits"),
    ("ring_oscillator", "Ring", 160, "circuits"),
    ("hartley", "Hartly", 200, "circuits"),
    ("rlc_ringing", "RLCst", 140, "circuits"),
    ("damping_ladder", "Damp", 140, "circuits"),
    ("thevenin", "Thev", 60, "circuits"),
    ("opamp_saturation", "Sat", 140, "circuits"),
    ("power_plant", "Plant", 200, "circuits"),
    ("substation", "Substn", 200, "circuits"),
    ("single_tuned_amp", "Tuned", 200, "circuits"),
    ("sr_latch", "SRlat", 160, "circuits"),
    ("common_base", "CB", 140, "circuits"),
    ("i2c_bus", "I2C", 200, "circuits"),
    ("rs485", "RS485", 200, "circuits"),
    ("high_side", "HiSw", 200, "circuits"),
    ("gpio_input", "Btn", 200, "circuits"),
    ("two_stage_fit", "2Stg", 200, "circuits"),
    ("spi", "SPI", 200, "circuits"),
    ("open_drain", "OD", 200, "circuits"),
    ("i2c_level", "I2Clv", 200, "circuits"),
    ("low_side", "LoSw", 200, "circuits"),
    ("tx_ladder", "TXLad", 200, "circuits"),
    ("res_service", "Split", 200, "circuits"),
    ("ac_start", "ACstart", 260, "circuits"),
    ("wind_collector", "Wind", 200, "circuits"),
    ("pfc", "PFC", 200, "circuits"),
    ("tx_69kv", "69kV", 200, "circuits"),
    ("branch_drop", "Branch", 200, "circuits"),
    ("solar_backfeed", "Solar", 200, "circuits"),
    ("com_480y", "480Y", 200, "circuits"),
    ("com_208y", "208Y", 200, "circuits"),
    ("ats", "ATS", 200, "circuits"),
    ("plant_13k8", "13k8", 200, "circuits"),
]
# name, template, first frame, frames, every
# Full resolution, full 256-colour palette; low frame rate (5 fps) keeps the files small.
GIFS = [
    ("rc_lowpass_sweep", "LP", 60, 18, 6),
    ("tesla_coil", "Tesla", 120, 15, 4),
    ("grid_chain", "Grid", 60, 12, 6),
    ("relaxation_osc", "RelOsc", 60, 12, 4),
    ("relay_differential", "87L", 60, 20, 6),
    ("breaker_failure", "50BF", 60, 20, 8),
    ("function_generator", "FuncGn", 60, 15, 4),
    ("three_phase_balanced", "3phY", 60, 12, 6),
    # Spotlight search (Ctrl+K / Ctrl+Space): type "mosfet", Enter picks the NMOS; extra tuple = (keys, first, every)
    ("spotlight_search", None, 20, 20, 4, ("^mosfet|", 24, 8)),
    ("i2c_bus", "I2C", 60, 15, 6),
    ("rs485", "RS485", 60, 15, 6),
    ("ac_start", "ACstart", 40, 20, 5),
    ("ats", "ATS", 30, 18, 4),
]
GIF_FRAME_MS = 200


def run(args, timeout=120, size=SIZE):
    cmd = [EXE, "--size", size, "--exit"] + args
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
    for spec in GIFS:
        name, tpl, first, frames, every = spec[:5]
        keys = spec[5] if len(spec) > 5 else None
        tmp = os.path.join(SHOT_DIR, "_rec_" + name)
        shutil.rmtree(tmp, ignore_errors=True)
        os.makedirs(tmp)
        print("gif", name)
        args = ["--tab", "circuits" if tpl else "parts", "--frame", str(first), "--record", tmp, str(frames), str(every)]
        if tpl: args += ["--template", tpl]
        if keys: args += ["--keys", keys[0], str(keys[1]), str(keys[2])]
        run(args, timeout=300, size=GIF_SIZE)
        files = sorted(glob.glob(os.path.join(tmp, "frame_*.bmp")))
        if not files:
            print("  ! no frames"); continue
        imgs = []
        for f in files:
            im = Image.open(f).convert("RGB")
            imgs.append(im.quantize(colors=256, method=Image.Quantize.MEDIANCUT))
        out = os.path.join(GIF_DIR, "auto_" + name + ".gif")
        imgs[0].save(out, save_all=True, append_images=imgs[1:], duration=GIF_FRAME_MS, loop=0, optimize=True)
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
