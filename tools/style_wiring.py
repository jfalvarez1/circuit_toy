"""Source-level: the schematic style has to reach everything that draws, and stop where it should.

The style is a mapping bolted in front of two SDL calls by macros in include/style.h. That is what
makes it a hundred lines instead of a second renderer - but it also means the mapping only covers
a file that includes the header, and only covers the calls that come after the include. A new
drawing file, or a new include added below style.h, silently draws in raw synthwave on white
paper, and there is no error anywhere: the picture is just wrong.

Three things are checked, and all three have already been wrong once:

  1. Every file that sets a draw colour or tints a texture includes style.h. Miss it and that
     file's colours never see the mapping.

  2. style.h is included LAST. It redefines SDL_SetRenderDrawColor; a header included after it
     brings in its own declarations and inline functions with the macro already active, and any
     drawing helper defined below it goes unmapped.

  3. Every arming of g_style_in_canvas is balanced by a disarm in the same function. The flag is
     what keeps the schematic on the drawing instead of over the toolbar: leave it set and the
     whole window turns into a row of empty white boxes, which is exactly what the first version
     of this feature did.

  4. No colour that is a bare list of numbers (SYNTH_PINK is `0xff, 0x29, 0x75`) is an operand
     of ?:. The comma operator swallows it: with c true, `c ? SYNTH_PINK : SYNTH_TEXT, 0xff`
     draws pink's blue as its red, beside the text colour's green and blue. Twelve highlighted
     states were drawn like that, from the palette's selected item (dark blue, meant orange) to
     every focused text field (pink, meant cyan).

Prints one line per finding and a summary; exits non-zero if anything is wrong.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")
INC = os.path.join(ROOT, "include")
DEFINE_RE = re.compile(r'^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)[ \t]+(.*)$', re.M)
COMMENT_OR_STRING_RE = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S)


def bare_lists():
    """Object-like macros whose whole value is a comma-separated list with nothing around it -
    the shape that only works spliced into an argument list."""
    names = set()
    for entry in sorted(os.listdir(INC)):
        if not entry.endswith(".h"):
            continue
        text = open(os.path.join(INC, entry), encoding="utf-8", errors="replace").read()
        for m in DEFINE_RE.finditer(text):
            # a comment that runs on past this line counts as comment to its end
            value = re.sub(r'/\*.*$', ' ', COMMENT_OR_STRING_RE.sub(" ", m.group(2))).strip()
            if "," in value and value[:1] not in "({" and not value.endswith("\\"):
                names.add(m.group(1))
    return names


def check_list_operands(text, name, lists, fails):
    """4. A bare list as an operand of ?: (see the module docstring)."""
    if not lists:
        return
    # blank comments and strings, keeping the newlines so line numbers still count
    code = COMMENT_OR_STRING_RE.sub(lambda m: re.sub(r'[^\n]', ' ', m.group(0)), text)
    alt = "|".join(sorted(lists, key=len, reverse=True))
    for m in re.finditer(r'(\?|(?<!:):(?!:))\s*(' + alt + r')\b', code):
        line = code.count("\n", 0, m.start()) + 1
        print(f"FAIL {name}:{line}: {m.group(2)} is an operand of ?: - it is a bare list of "
              f"numbers, so the comma operator keeps only its last one and the call draws a "
              f"colour nobody wrote. Choose between the two with if/else.")
        fails.append(name)

DRAW_CALLS = ("SDL_SetRenderDrawColor", "SDL_SetTextureColorMod")
INCLUDE_RE = re.compile(r'^\s*#\s*include\s+[<"]([^>"]+)[>"]', re.M)


def check_file(path, name, fails):
    text = open(path, encoding="utf-8", errors="replace").read()

    includes = [(m.start(), m.group(1)) for m in INCLUDE_RE.finditer(text)]
    has_style = any(inc == "style.h" for _, inc in includes)

    # 1. does it draw at all?
    draws = any(call in text for call in DRAW_CALLS)
    if draws and not has_style:
        which = ", ".join(c for c in DRAW_CALLS if c in text)
        print(f"FAIL {name}: calls {which} but does not include style.h - "
              f"its colours never reach the schematic mapping")
        fails.append(name)
        return

    # 2. and is style.h the last include?
    if has_style:
        last_pos, last_name = includes[-1]
        if last_name != "style.h":
            print(f"FAIL {name}: style.h is included before {last_name!r}; it has to be last, or "
                  f"anything that header defines draws with the macros already in force")
            fails.append(name)

    # 3. is the canvas flag balanced inside each function that touches it?
    #    Functions are found by a brace-depth walk, which is enough for this codebase's style
    #    (every function opens its brace in column 0).
    depth = 0
    fn_start = None
    fn_line = 0
    line = 1
    arms = disarms = 0
    for i, chunk in enumerate(text):
        if chunk == "\n":
            line += 1
        elif chunk == "{":
            if depth == 0:
                fn_start = i
                fn_line = line
                arms = disarms = 0
            depth += 1
        elif chunk == "}":
            depth -= 1
            if depth == 0 and fn_start is not None:
                body = text[fn_start:i]
                arms = len(re.findall(r'g_style_in_canvas\s*=\s*1\s*;', body))
                disarms = len(re.findall(r'g_style_in_canvas\s*=\s*(?:0|[A-Za-z_]\w*)\s*;', body))
                if arms and disarms < arms:
                    print(f"FAIL {name}:{fn_line}: arms g_style_in_canvas {arms} time(s) but only "
                          f"puts it back {disarms} - the style would run on past the canvas")
                    fails.append(name)
                fn_start = None
    return


def main():
    fails = []
    checked = 0
    drawing = 0
    lists = bare_lists()
    for entry in sorted(os.listdir(SRC)):
        if not entry.endswith(".c"):
            continue
        path = os.path.join(SRC, entry)
        text = open(path, encoding="utf-8", errors="replace").read()
        checked += 1
        if any(call in text for call in DRAW_CALLS):
            drawing += 1
        check_file(path, "src/" + entry, fails)
        check_list_operands(text, "src/" + entry, lists, fails)

    if not drawing:
        print("FAIL no source file draws anything - this check is looking in the wrong place")
        return 1
    # The colours this exists for. If the scan stops finding them, check 4 checks nothing.
    if "SYNTH_PINK" not in lists:
        print("FAIL found no bare-list colours in include/ - check 4 is looking in the wrong place")
        return 1

    print(f"style-wiring: {checked} sources, {drawing} of them draw, {len(fails)} failures")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
