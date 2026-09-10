# Handoff — where this is and what to do next (2026-09-09, after v3.32.0)

Written for whoever picks this up next, with no assumed context. Everything below is either a
fact you can re-verify in one command or a decision with its reasoning attached.

## What this is

Circuit Playground: a C11/SDL2 circuit simulator. MNA + Newton-Raphson solver written here, not
ngspice. 211 built-in templates, a netlist reader, an oscilloscope, and a 77-suite self-audit
battery that is the main thing keeping it honest.

    meson compile -C build           # or: ninja -C build
    bash tools/run_audits.sh         # the whole battery, ~400 s, must print "0 of 77 suites failed"
    build/tools/template_smoke.exe --netlist-test     # one suite, while iterating
    AUDIT_LIST=1 bash tools/run_audits.sh             # what the battery contains

On Windows the compiler needs its environment first:

    cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && ninja -C build'

## State

v3.32.0 is released, tagged, CI green, and the shipped zip has been downloaded and run - it
renders, simulates, and reads 10.89 V where its own on-canvas note says to expect 10.9.

Working tree clean, `main` == `origin/main`, tag `v3.32.0` pushed.

## The collaboration that is driving most of the work

There is a second project, **EE_Review** (`C:\Users\zerav\OneDrive\Desktop\EE_Review`), a
course. It is the textbook; this is the simulator it leverages. It ships its lessons' circuits
as SPICE in `_audit/review/spice/` (194 files plus `index.json`), and running that corpus here
is a cross-check between two independent implementations.

**That corpus has found more real bugs in this program than any suite has.** Six in v3.32.0
alone. Run it:

    python <scratch>/spice_run.py --tag whatever        # see docs/EE_REVIEW_FINDINGS.md for what it checks

The runner itself lives only in a scratch directory and is worth rewriting rather than hunting
for. It is thirty lines: walk `index.json`, run `--netlist-solve` on each file, classify the
output. **Two warnings, because both bit me and both flattered the result:**

- `--netlist-solve` prints a residual and THEN says `NOT A SOLUTION` if the equations are not
  satisfied there. Counting any output containing "residual" as a solve is wrong.
- the reader writes `skipped 1 line` in the SINGULAR for exactly one. A regex matching
  `skipped (\d+) lines` misses 21 files.

Current numbers, measured correctly: **185 of 194 produce a solution, 149 with nothing skipped**,
36 with at least one element the reader cannot place, 8 refuse, 1 is not a solution.

`docs/EE_REVIEW_FINDINGS.md` is the full cross-project record: what was found on each side, what
is theirs to decide, what is ours.

## Next work, in the order I would do it

### 1. Source stepping, for m05l11-4

A differential pair with a PNP current-mirror load that will not solve. This is the most
valuable open item because it is a solver limitation, not one circuit's problem.

**Do not try these. All four are already done, measured, and reverted:**

| attempt | residual, from a baseline of 0.000518 A |
|---|---|
| gmin stepping, fixed decade schedule | 24.02 A |
| damped Newton, 0.5 after ten passes | 5.234 A |
| relative step cap, 10 V + 2x scale | 3.037e6 A |
| MAX_ITERATIONS 50 -> 500 | 0.000617 A, i.e. nothing |

Use the instrument that exists rather than reasoning from the source - three separate diagnoses
were made by reading code and all three were wrong:

    NEWTON_TRACE=1 build/tools/template_smoke.exe --netlist-solve <the .cir>

It shows a clean **two-cycle**: the output node steps to +653,700 V and back to -28.58 V for
fifty passes with neither endpoint moving. Not diverging, not creeping.

EE_Review solves it and gave the mechanism: V(out) = -508.22 mV with **Q2 saturated** at
Vce = 139 mV, and V(out) = V(emit) + Vce(Q2) checks out (-647 + 139 = -508). The solution is on
the far side of a REGION CHANGE. That is why every knob on the iteration fails - a smaller step
from the wrong side still never crosses a corner. Source stepping (ramp the supplies from zero,
so devices cross their region boundaries in the order the physical circuit does) is the
candidate that addresses the actual shape of the problem.

`docs/ROADMAP.md` has the full write-up at the top.

### 2. A 4-terminal SPDT switch part

8 more corpus lines. `X ... SPDT_SWITCH` is written `Xname A B common control`, e.g.
`XS0 vref 0 b0_sw b0_in SPDT_SWITCH`. There is no part behind it: `COMP_SPDT_SWITCH` has three
terminals and no control pin, `COMP_DPDT_DRIVEN` has seven.

This is a real part to build (descriptor, stamp, render, properties), not a mapping to add.
**Do not synthesise it in the reader from two analog switches** - that is the reader inventing
topology, which is the one thing it must not do.

`X ... ANALOG_SWITCH` is already done and is the pattern to follow: it mapped onto
`COMP_ANALOG_SWITCH`, which already was a controlled resistance (r_on 100 above v_on, r_off 1e9
below v_off). See `src/netlist.c`, the `case 'X'` block.

### 3. The rest of the false junctions

20 false junctions and 27 loose ends remain, down from 66/47. `--wire-test` reads GEOMETRY, not
nets, so everything it reports is a drawing that disagrees with a netlist that is correct.

    WIRE_NOTES=200 build/tools/template_smoke.exe --wire-test

Three idioms account for most of what has been fixed so far, and the remainder look similar:

- a wire run to a ground's ORIGIN rather than its pin, which is 20 px further on
- a DC source at rotation 90 - the part is ALREADY vertical, so 90 lays it on its side
- two nets drawn down one line (Current Mirror's base bus on its ground rail, CMOS Inverter's
  gate line through Vdd's negative pin)

Tighten `WIRE_FALSE_JUNCTION_BASELINE` / `WIRE_LOOSE_END_BASELINE` in `tools/template_smoke.c`
when you fix some. A ratchet never tightened is a permanent allowance.

## Traps specific to this repo

- **Sources are CRLF.** `sed -i` will silently rewrite a whole file to LF. Use the editing
  tools, and if you do use sed, convert back.
- **`TN(x, y)` creates a node whether or not anything wires to it.** Declaring nodes you might
  not use leaves them sitting in the middle of whatever wire spans them.
- **`circuit_find_or_create_node` merges within 5 px.** Two things 4 px apart are one node.
- **A ground's pin is at (0, -20) from where the symbol is placed.** A wire to the placement
  point runs through the pin and past it.
- **A wire joins its two endpoints and nothing else.** A node lying along a longer wire is not
  connected to it. This is the single most repeated bug in the template code.
- **`node_ids[]` assignment overrides geometry.** That is why so many wrong drawings solve
  perfectly: the netlist is set by hand and the picture is decoration. Only `--wire-test` and
  `--geom-test` can see the difference.
- Fixing a wire-test finding by moving a symbol can create a **geom-test** hard violation (text
  over a wire). Run the whole battery, not the suite you are working on.

## How to work here, learned the hard way this week

**Every new guard must be mutation-checked** - break the thing it guards, confirm the guard
fails, restore. Three guards written this week passed against deliberately broken builds and
were thrown away. A guard that passes whether or not the fault is present is not a guard, and it
looks exactly like coverage.

Two specific ways that goes wrong:

- an oracle of `expect 0.0` with a RELATIVE tolerance demands an exact bit. Design the circuit
  so the expected value is non-zero.
- a guard written using a feature that later becomes supported starts passing for the wrong
  reason. (`an X the reader has no model for is refused` used ANALOG_SWITCH as its example and
  had to be rewritten the day ANALOG_SWITCH landed.)

**When a measurement looks wrong, run the same input through the other code path before forming
a hypothesis.** A frequency sweep misbehaving gets checked with `--netlist-trace` on the same
file. This cost an hour and a retracted diagnosis: the reasoning was source-checked, internally
consistent, made a prediction that came true, and was wrong - because every control it designed
varied the suspect, and a self-consistent theory only generates confirming experiments.

**Check your instrument against the thing it measures.** Two bugs in the corpus harness this
week, both of which made the results look better than they were. The solver was checked hard
against the harness and the harness was never checked against the solver.

## Release process

1. bump `include/version.h` (single source of truth; `tools/make_release.ps1` reads it)
2. update `README.md` (version line, suite count), `TEST_PLAN.md` (a new numbered section),
   `TEMPLATE_AUDIT.md` if templates or drawings changed
3. `bash tools/run_audits.sh` -> 0 of 77. **Rebuild first** - `cli-smoke` compares the running
   binary's `--version` against `version.h` and will fail on a stale exe. It did exactly that
   on this release.
4. commit, push, **wait for CI green on the commit**
5. `git tag -a vX.Y.Z -F <message>` and `git push origin vX.Y.Z`. The tag makes CI run the
   whole unsharded battery and attach the zip.
6. **Download the released zip and run it.** `--version` and `--layout-test` both pass on a
   binary that cannot start; render a template and look at the bitmap.

## Reaching EE_Review

It runs as a separate Claude session and is reachable by cross-session message when it is up. It
does not appear in agent listings by name - only by its pipe address - and when it is closed,
messages sent to it are LOST rather than queued. Four were, which is why
`docs/EE_REVIEW_FINDINGS.md` exists: the findings are worth more than the transcript they were
stranded in.

Open questions with them, both asked and unanswered as of this writing: whether to do source
stepping or the SPDT part first, and their m05l16 lesson needs one edit (its distortion
procedure holds the input fixed while its formula assumes fixed output - the two differ by
(1 + gm*RE) squared rather than once; measured +27.7 dB against a stated +14, and +13.1 dB when
the output is held constant instead).
