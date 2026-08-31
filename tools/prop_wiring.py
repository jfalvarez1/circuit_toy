"""Every property type has to be wired at both ends.

Editing a property takes three sites: a row in src/ui.c that draws it and registers its type, a
handler in src/input.c that validates and applies a typed value, and a current-value lookup in
src/app.c so the edit box opens on what is there. Miss one and the failure is silent in a
particular way - the part looks configurable and is not, or is configurable and looks inert.

Both halves of that were real. PROP_GAIN sat in the enum with nothing on either end, so the gain
of a VCVS - the entire point of the part - could not be set by any means. PROP_TRANS_R_PRIMARY had
a working handler and no row, so the plumbing was there and the tap was missing. Neither is
visible to --prop-test, which only checks that rows which DO exist can be applied.

    python tools/prop_wiring.py
"""
import os, re, sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def read(rel):
    with open(os.path.join(root, rel), encoding="utf-8", errors="replace") as f:
        src = f.read()
    # Comments are not code. A note saying "no PROP_X handler here, and why" would otherwise be
    # counted as a handler - which is exactly what happened when three inert rows were removed
    # and the comments explaining their absence kept them looking wired.
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    src = re.sub(r"//[^\n]*", " ", src)
    return src

enum_src = read("include/input.h")
ui_src = read("src/ui.c")
# app.c applies some properties itself - the probe's name, and every toggle through the
# PROP_IDEAL chain - so it is an apply site too. Looking only at input.c reported the probe
# name as an inert row when it has worked all along.
apply_src = read("src/input.c") + read("src/app.c")

# the PropertyType enum body
m = re.search(r"typedef enum\s*\{(.*?)\}\s*PropertyType", enum_src, re.S)
if not m:
    print("prop-wiring: could not find the PropertyType enum")
    sys.exit(1)
names = re.findall(r"^\s*(PROP_[A-Z0-9_]+)", m.group(1), re.M)
names = [n for n in names if n not in ("PROP_NONE", "PROP_TYPE_COUNT")]

# Cases fall through in groups: "case PROP_VALUE: case PROP_AMPLITUDE:" share one body, which
# makes PROP_AMPLITUDE a synonym rather than a dead handler. The first draft of this tool did not
# know that and called it unreachable - and the attempt to act on that report deleted the shared
# body and broke every value row in the program. A group counts as drawn if any member is drawn.
synonym = {}
for group in re.findall(r"((?:^[ \t]*case PROP_[A-Z0-9_]+:[ \t]*\n){2,})", apply_src, re.M):
    members = re.findall(r"case (PROP_[A-Z0-9_]+):", group)
    for member in members:
        synonym[member] = members

dead, unreachable, inert = [], [], []
for n in names:
    # A row may register a literal type or pass one to the shared sweep helper, so "mentioned in
    # ui.c at all" is the test - narrower matching produced false positives on the sweep rows.
    drawn = any(re.search(r"\b%s\b" % alias, ui_src) for alias in synonym.get(n, [n]))
    applied = re.search(r"\b%s\b" % n, apply_src) is not None
    if not drawn and not applied:
        dead.append(n)
    elif applied and not drawn:
        unreachable.append(n)
    elif drawn and not applied:
        inert.append(n)

# Two different things, and only one of them is a fault.
#
# A name with nothing at either end is a property somebody meant to add and did not - a
# placeholder. PROP_WIPER_POS is the potentiometer's wiper, PROP_IDSS and PROP_VP are the JFET's:
# they map onto the parts that still have no panel, which is a tracked roadmap item and not a
# wiring bug. Those are counted, not failed.
#
# A name wired at one end only IS a bug, and a quiet one: a handler with no row is a part that
# looks unconfigurable while the plumbing sits there (PROP_TRANS_R_PRIMARY was exactly this), and
# a row with no handler is worse - it looks configurable, accepts a value, and drops it.
for n in unreachable:
    print("[FAIL] prop-wiring  %-28s has a handler but no row draws it - unreachable" % n)
for n in inert:
    print("[FAIL] prop-wiring  %-28s is drawn but nothing applies it - an inert row" % n)

bad = len(unreachable) + len(inert)
print("prop-wiring: %d property types - %d wired at one end only, %d not built yet "
      "(the parts still without panels; see docs/ROADMAP.md)"
      % (len(names), bad, len(dead)))
sys.exit(1 if bad else 0)
