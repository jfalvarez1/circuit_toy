"""Every part that claims a thermal limit must have a power expression that is actually read.

Source-level, so it needs no binary and no circuit that happens to exercise the part.

component_create gives some types a thermal.max_temperature, which is the part saying "I can
overheat". thermal_update_components then has a switch that works out what each of those is
dissipating. If a type is in the first list and not the second, it can never get warm; if it is
in the second but only as `power = c->thermal.power_dissipated;`, that is worse than absent -
the loop writes that same field at the bottom, so the value is read from itself, is zero, and
stays zero for ever.

That is not hypothetical. Every one of resistor, BJT, MOSFET, capacitor, LED and diode was
written that way, so nothing in this program had ever accumulated a joule of damage or released
any smoke, while the overload warning next to it worked fine off a different field. This guard
exists so the next part with a rating cannot repeat it.

Exit 1 and name the type if any claims a limit it cannot reach.
"""
import os
import re
import sys

os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

DEAD = 'power = c->thermal.power_dissipated;'


def types_claiming_a_limit(src):
    """COMP_* whose thermal init sets a non-zero max_temperature."""
    claims = set()
    pending = []
    for line in src.split('\n'):
        m = re.match(r'\s*case (COMP_[A-Z0-9_]+):\s*$', line)
        if m:
            pending.append(m.group(1))
            continue
        m = re.search(r'thermal\.max_temperature\s*=\s*(.+?);', line)
        if m and pending:
            expr = m.group(1).strip()
            # "high_power ? 0.0 : 155.0" still claims a limit for the ordinary case
            if expr not in ('0', '0.0'):
                claims.update(pending)
            pending = []
            continue
        if line.strip().startswith('break;'):
            pending = []
    return claims


def thermal_switch_body(src):
    """The switch in thermal_update_components: {COMP_*: body-of-its-case}."""
    start = src.find('static void thermal_update_components')
    if start < 0:
        return None
    end = src.find('\n}', start)
    region = src[start:end if end > 0 else len(src)]
    sw = region.find('switch (c->type)')
    if sw < 0:
        return None
    region = region[sw:]
    out, pending = {}, []
    for line in region.split('\n'):
        m = re.match(r'\s*case (COMP_[A-Z0-9_]+):\s*(\{)?\s*$', line)
        if m:
            pending.append(m.group(1))
            continue
        if re.match(r'\s*(default:|\})', line) and not pending:
            continue
        if pending:
            for t in pending:
                out.setdefault(t, []).append(line)
            if 'break;' in line:
                pending = []
    return out


def main():
    comp = open('src/component.c', encoding='utf-8', errors='replace').read()
    sim = open('src/simulation.c', encoding='utf-8', errors='replace').read()

    claims = types_claiming_a_limit(comp)
    handled = thermal_switch_body(sim)
    if handled is None:
        print("[FAIL] thermal-wiring: could not find the switch in thermal_update_components")
        return 1

    problems = []
    for t in sorted(claims):
        if t not in handled:
            problems.append((t, "claims a temperature limit and the damage model has no case for it"))
            continue
        body = '\n'.join(handled[t])
        live = [ln for ln in handled[t] if 'power' in ln and '=' in ln and DEAD not in ln]
        if DEAD in body and not live:
            problems.append((t, "reads thermal.power_dissipated, which the loop writes back to "
                                "itself - it is zero and always will be"))

    for t, why in problems:
        print("[FAIL] thermal-wiring: %-20s %s" % (t, why))
    print("\nthermal-wiring: %d part types claim a temperature limit, %d with no power the "
          "damage model can read" % (len(claims), len(problems)))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
