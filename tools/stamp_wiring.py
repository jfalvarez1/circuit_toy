"""Every part that stamps into a current row must be given one.

Source-level, so it needs no binary and no circuit that happens to exercise the part.

MNA gives some parts an extra unknown - the current through them - and the row for it lives at
`num_nodes + comp->voltage_var_idx`. Those rows are handed out in simulation.c by
component_aux_count, which returns 0 for any type that component_create did not put in its
needs_voltage_var list. A type left off that list keeps whatever voltage_var_idx it was
created with, which is 0, and its stamp lands on row num_nodes + 0 - the FIRST voltage source's
current equation.

Nothing about that fails loudly. The matrix is the right size, the solve converges, and the
answer is wrong in a part of the circuit nowhere near the part at fault, because two elements
have been writing into one equation. COMP_PWL_SOURCE and COMP_EXPR_SOURCE both shipped this
way: two sources in the palette, each with a complete and correct stamp, neither of which ever
owned the row it was writing to.

The check is a law rather than a list of parts: read the stamp switch, collect every case whose
body mentions voltage_var_idx, and require each one to appear in the needs_voltage_var
expression. A part added tomorrow is covered the day it lands.

One exemption, and it is in the source rather than here: COMP_SUBCIRCUIT is deliberately not
gated on needs_voltage_var, because it needs as many rows as the definition inside it has -
component_aux_count answers for it separately. simulation.c says so on the line that skips it.

Exit 1 and name the type if any part stamps a row it was never given.
"""
import os
import re
import sys

os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

SRC = os.path.join('src', 'component.c')

# Rows for this one are counted by component_aux_count from the definition it points at, not by
# the flag. The reason lives in simulation.c; this guard checks that the reason is still there.
EXEMPT = {'COMP_SUBCIRCUIT'}
EXEMPT_EVIDENCE = 'not gated on needs_voltage_var'


def strip_comments(src):
    """Blank out comments, keeping newlines so nothing shifts line-wise.

    This guard caught its own first fix by accident and then failed on it: the comment written
    into needs_voltage_var to explain the repair contains the words `voltage_var_idx`, and a
    scanner reading raw text attributed that prose to the three case labels above it. A
    source-level guard that reads comments will always be fooled by the comment describing the
    thing it guards, so it reads code only.
    """
    out = []
    i, n = 0, len(src)
    while i < n:
        if src.startswith('/*', i):
            j = src.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(''.join(ch if ch == '\n' else ' ' for ch in src[i:j]))
            i = j
        elif src.startswith('//', i):
            j = src.find('\n', i)
            j = n if j < 0 else j
            out.append(' ' * (j - i))
            i = j
        elif src[i] == '"':
            j = i + 1
            while j < n and src[j] != '"':
                j += 2 if src[j] == '\\' else 1
            j = min(j + 1, n)
            out.append(src[i:j])
            i = j
        else:
            out.append(src[i])
            i += 1
    return ''.join(out)


def declared_types(src):
    """The COMP_* named in component_create's needs_voltage_var expression."""
    m = re.search(r'comp->needs_voltage_var\s*=\s*\((.*?)\);', src, re.S)
    if not m:
        return None
    return set(re.findall(r'COMP_[A-Z0-9_]+', m.group(1)))


def stamping_types(src):
    """The COMP_* whose case body in the stamp switch mentions voltage_var_idx.

    Fallthrough labels share the body below them, so a label with nothing of its own inherits
    the next real body - `case COMP_A: case COMP_B: { ... }` must count for both.
    """
    labels = [(m.start(), m.group(1)) for m in re.finditer(r'case (COMP_[A-Z0-9_]+):', src)]
    out = set()
    for i, (pos, name) in enumerate(labels):
        for j in range(i, len(labels)):
            end = labels[j + 1][0] if j + 1 < len(labels) else len(src)
            body = src[labels[j][0]:end]
            # a bare fallthrough label is just "case COMP_X:" and a little whitespace
            if len(body.strip()) < 40 and j + 1 < len(labels):
                continue
            if 'voltage_var_idx' in body:
                out.add(name)
            break
    return out


def main():
    src = strip_comments(open(SRC, encoding='utf-8', errors='replace').read())

    declared = declared_types(src)
    if declared is None:
        print("[FAIL] stamp-wiring: cannot find the needs_voltage_var expression in %s - the "
              "guard has gone blind, which is worse than a failure" % SRC)
        return 1

    stamping = stamping_types(src)
    if not stamping:
        print("[FAIL] stamp-wiring: no case body mentions voltage_var_idx, so this guard is "
              "reading the wrong file or the wrong shape")
        return 1

    sim = open(os.path.join('src', 'simulation.c'), encoding='utf-8', errors='replace').read()
    problems = []
    for t in sorted(stamping - declared):
        if t in EXEMPT:
            if EXEMPT_EVIDENCE not in sim:
                problems.append((t, "is exempt here because simulation.c said it counts its own "
                                    "rows, and simulation.c no longer says that"))
            continue
        problems.append((t, "stamps num_nodes + voltage_var_idx and is not in needs_voltage_var, "
                            "so it writes into the first voltage source's row"))

    for t, why in problems:
        print("[FAIL] stamp-wiring: %-22s %s" % (t, why))
    print("\nstamp-wiring: %d part types stamp a current row, %d of them own one, %d wrong"
          % (len(stamping), len(stamping & declared) + len(stamping & EXEMPT), len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
