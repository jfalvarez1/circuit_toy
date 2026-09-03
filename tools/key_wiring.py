"""Source-level: every keyboard shortcut the program PROMISES has to be one it handles.

There are two places the app tells a user which keys work - the F1 dialog in src/ui.c and the
shortcut tables in guide.html - and nothing tied either of them to the code. A key removed or
renamed leaves the promise behind it, and a promise is worse than an omission: the user presses
it, nothing happens, and there is no way to tell a broken feature from a mistyped key.

Checked in the direction that has no false positives: documented -> handled. The other direction
is not an error at all, because plenty of keys are context-only - Backspace inside a text field,
the arrows in a list - and listing those as shortcuts would be noise.

  python tools/key_wiring.py

Prints one line per finding and a summary; exits non-zero if anything is promised and not wired.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# What a documented key is called in SDL. Anything not here and not a single character or Fn is
# not a key (Scroll, Mid-drag) and is skipped.
NAMED = {
    "escape": "ESCAPE", "esc": "ESCAPE", "delete": "DELETE", "del": "DELETE",
    "backspace": "BACKSPACE", "space": "SPACE", "tab": "TAB", "enter": "RETURN",
    "return": "RETURN", "home": "HOME", "end": "END", "up": "UP", "down": "DOWN",
    "left": "LEFT", "right": "RIGHT", "+": "PLUS", "-": "MINUS", "=": "EQUALS",
    "/": "SLASH", ".": "PERIOD", ",": "COMMA",
}
NOT_A_KEY = {"scroll", "mid-drag", "drag", "click", "wheel", "mouse", "shift"}


def sdl_names(token):
    """Every SDLK_ spelling a documented key could reasonably be handled as."""
    t = token.strip().lower()
    if not t or t in NOT_A_KEY:
        return None
    if re.fullmatch(r"f\d{1,2}", t):
        return ["SDLK_" + t.upper()]
    if t in NAMED:
        out = ["SDLK_" + NAMED[t]]
        if NAMED[t] == "PLUS":
            out += ["SDLK_EQUALS", "SDLK_KP_PLUS"]        # '+' is shift-equals on most layouts
        if NAMED[t] == "MINUS":
            out += ["SDLK_KP_MINUS"]
        if NAMED[t] == "RETURN":
            out += ["SDLK_KP_ENTER"]
        return out
    if len(t) == 1 and (t.isalpha() or t.isdigit()):
        return ["SDLK_" + t]
    return None


def parse_f1(ui_c):
    """The F1 dialog: a run of "KEY  - what it does" string literals."""
    fn = ui_c.find("void ui_render_shortcuts_dialog")
    if fn < 0:
        return []
    body = ui_c[fn:fn + 4000]
    out = []
    for lit in re.findall(r'"([^"]*)"', body):
        m = re.match(r"\s*([A-Za-z0-9+.,/\- ]+?)\s+-\s+\S", lit)
        if m:
            out.append((m.group(1).strip(), "F1 dialog"))
    return out


def parse_tooltips(ui_c):
    """A button tooltip that names a key is a promise too - and it was where four broken ones
    lived. "Start simulation (F5)", "Single step (F10)" and "Reset simulation (F4)" were all on
    buttons; F5, F6 and F10 had no handler anywhere, and F4 is the brightness key."""
    out = []
    for lit in re.findall(r'"([^"]*)"', ui_c):
        # Only shapes that can only be a shortcut. A bare letter in brackets is not one:
        # "Beta (K):" and "Cells (S):" are property labels, and reading those as promises put
        # two keys on the failure list that nothing had ever claimed.
        for m in re.finditer(r"\((F\d{1,2}|Ctrl\+[A-Za-z0-9])\)", lit):
            out.append((m.group(1), "a button tooltip"))
    return out


def parse_guide(html):
    """The guide's shortcut tables: <td><kbd>Ctrl</kbd>+<kbd>S</kbd></td>."""
    out = []
    for row in re.findall(r"<tr>(.*?)</tr>", html, re.S):
        cells = re.findall(r"<td>(.*?)</td>", row, re.S)
        if not cells:
            continue
        keys = re.findall(r"<kbd>(.*?)</kbd>", cells[0])
        if not keys:
            continue
        out.append(("+".join(k.strip() for k in keys), "guide.html"))
    return out


def handled(spec, src):
    """Is this key combination actually wired up in the input handlers?"""
    parts = [p.strip() for p in re.split(r"\s*\+\s*", spec) if p.strip()]
    parts = [p for p in parts if p.lower() not in ("shift",)]        # shift alone changes nothing here
    want_ctrl = any(p.lower() == "ctrl" for p in parts)
    keys = [p for p in parts if p.lower() != "ctrl"]
    if len(keys) != 1:
        return None                                                  # not a single-key shortcut
    names = sdl_names(keys[0])
    if not names:
        return None                                                  # not a key at all
    # The handlers write the modifier two ways: inline, `sym == SDLK_g && (GetModState() &
    # KMOD_CTRL)`, or as a local set once at the top of the function, `bool ctrl =
    # input->ctrl_down;` followed by `case SDLK_s: if (ctrl)`. Only the first is visible on the
    # same line as the key, so looking there alone reported every Ctrl shortcut in the program as
    # missing - including the seven that keys_gui drives through the real app every run.
    for line_no, line in enumerate(src):
        if not any(re.search(n + r"\b", line) for n in names):
            continue
        window = " ".join(src[line_no:line_no + 8])
        ctrl_yes = ("KMOD_CTRL" in window
                    or re.search(r"(?<!!)\bctrl\b\s*(&&|\))", window) is not None)
        # The plain-key path is written either as `if (!ctrl)` or as the else of `if (ctrl)`.
        # Only the first was recognised, so `case SDLK_r: if (ctrl) {rotate} else {resistor}`
        # counted as Ctrl-only and plain R was reported unwired while it plainly works.
        ctrl_no = (re.search(r"!\s*ctrl\b", window) is not None
                   or re.search(r"\belse\b", window) is not None)
        if want_ctrl and ctrl_yes:
            return True
        if not want_ctrl and (ctrl_no or not ctrl_yes):
            return True
    return False


def main():
    ui_c = open(os.path.join(ROOT, "src", "ui.c"), encoding="utf-8", errors="replace").read()
    guide = open(os.path.join(ROOT, "guide.html"), encoding="utf-8", errors="replace").read()
    src = []
    for f in ("src/input.c", "src/app.c", "src/ui.c"):
        src += open(os.path.join(ROOT, f), encoding="utf-8", errors="replace").read().splitlines()

    promised = parse_f1(ui_c) + parse_tooltips(ui_c) + parse_guide(guide)
    checked = 0
    fails = []
    skipped = 0
    for spec, where in promised:
        got = handled(spec, src)
        if got is None:
            skipped += 1
            continue
        checked += 1
        if not got:
            fails.append((spec, where))
            print("FAIL %-22s promised in %s, but no handler takes that key" % (spec, where))

    if checked < 15:
        print("FAIL only %d shortcuts were checked - the documentation parser has stopped "
              "finding them, which would make this pass by seeing nothing" % checked)
        return 1

    print("key-wiring: %d documented shortcuts checked, %d not keys, %d promised and not wired"
          % (checked, skipped, len(fails)))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
