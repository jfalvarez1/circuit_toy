"""Every button that exists must be clickable, and every click must reach something.

Source-level, so it covers buttons that no template or resolution happens to show.

A Button in UIState is drawn by ui.c and has to be hit-tested by something, or it is a control
that is painted, hovered, labelled, and dead. That is the hardest UI fault to notice by looking:
it looks exactly like a working button. It has happened here - the toolbar's rightmost button
was drawn past the edge of what the click handler was clamped to, and the frequency sweep's
Repeat control was drawn and unreachable for the same reason.

--layout-test already checks that no two buttons overlap, at four window sizes, which is the
other way a button becomes unreachable. This is the way that geometry cannot see.

Exit 1 and name any button that is declared and never hit-tested.
"""
import os
import re
import sys

os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

# Buttons that are deliberately not hit-tested by name, with the reason. Anything here has to
# earn its place: an exemption is how a dead control hides.
EXEMPT = {
    # none today
}


def declared_buttons(header):
    """Button btn_x; declarations in UIState."""
    return set(re.findall(r'^\s*Button\s+(btn_[a-z0-9_]+)\s*;', header, re.M))


def main():
    header = open('include/ui.h', encoding='utf-8', errors='replace').read()
    sources = ''
    for p in ('src/ui.c', 'src/app.c', 'src/input.c', 'src/main.c'):
        if os.path.exists(p):
            sources += open(p, encoding='utf-8', errors='replace').read()

    declared = declared_buttons(header)
    if not declared:
        print("[FAIL] click-wiring: no Button declarations found in include/ui.h")
        return 1

    missing = []
    for b in sorted(declared):
        if b in EXEMPT:
            continue
        # hit-tested by name, in any of the forms the code uses
        pat = re.compile(r'(point_in_rect\s*\([^)]*&\s*(?:ui|state|u)\s*->\s*%s\.bounds|'
                         r'&\s*(?:ui|state|u)\s*->\s*%s\.bounds\s*\)|'
                         r'%s\.bounds\s*,)' % (re.escape(b), re.escape(b), re.escape(b)))
        if not pat.search(sources):
            missing.append(b)

    for b in missing:
        print("[FAIL] click-wiring: %-24s is drawn and never hit-tested - a dead control" % b)
    print("\nclick-wiring: %d buttons declared, %d that no click can reach"
          % (len(declared), len(missing)))
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
