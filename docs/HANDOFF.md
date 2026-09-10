# Handoff — where this is and what to do next (2026-09-10, v3.33.0)

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

v3.33.0 adds live scope mouse measurements, trace selection, and the rendering/input fixes
described below. It also includes the BJT Early-effect Jacobian fix, correct DC current readback
and Kirchhoff diagnostics, four additional netlist checks, and `tools/spice_run.py` with seven
real CLI checks. The portable Windows release is built and published by the tag workflow;
`gh release view v3.33.0` reports its publication state and attached downloads.
The source-stepping experiment was removed after the actual defect was measured; there is no
continuation fallback in the final implementation.

Validation of the v3.33.0 build completed locally: **0 of 77 suites failed**, 15 at a time, 458 seconds.
`--netlist-test` passes all 30 checks; the classifier passes 13 checks and the real CLI audit
passes seven. The full audit includes the numerical suites, scope stability, 34 CLI options,
GUI smoke, all 211 template edge/export checks, and ten keyboard shortcuts. Deliberate mutations
confirmed the new numerical, current-readback, KCL, classifier and rejection guards fail when
their faults are restored. These are local validation results; GitHub workflow results are
tracked against each pushed commit.

## Scope interaction follow-up (2026-09-10)

The Y-T scope now has a live mouse crosshair/readout and direct trace selection. Readouts use the
last drawn channel transform, including AC/Fit shifts, interpolate nonuniform capture times,
and distinguish pointer voltage from signal voltage. Disabled, nonfinite and missing data do
not produce samples. A trace click selects its vertical controls with CUR off; CUR on keeps A/B
positioning. Explicit trigger handles still work. Popup pointer coordinates use popup pixels,
independently of the main window's UI scale.

Removed the unconditional trigger-level fallback on docked scope clicks. Popup input-row clicks
now restore temporary coordinates. Signals below 10 mV are drawn from their actual samples,
not flattened to their mean. The graticule divides its full dimensions without accumulating
rounding errors and gives each stacked band eight voltage divisions. Manual cursor report boxes now fit their text and the available plot space.

Coverage extends `--layout-test` and `tools/cli_smoke.py --only=--hover`. `--hover X,Y,FRAME`
injects motion without clicking; with `--popout`, scripted pointer events target that window.
`--state-out` includes scope/readout state. The complete battery remains 77 suites.
Twelve deliberate mutations were caught: interpolation, absolute time origin, AC shift,
per-channel scale, finite-sample validation, FFT exclusion, trace selection, competing trigger
clicks, small-signal segment flattening, missing crosshair painting, popup double scaling,
and a stacked graticule with the wrong number of divisions.
The rendering check includes slopes as well as extrema so partial endpoint corruption fails.
The full local audit passed all 77 suites in 449 seconds. Layout and real mouse checks were
repeated successfully after the final stacked-grid correction.

This task owns Circuit Toy simulator work. Course diagram edits were handed to the separate
EE_Review task, "Improve EE lessons with references"; do not resume editing the course here.

## Maintenance authorization

On 2026-09-09, the project owner authorized ongoing project maintenance and GitHub pushes.
On 2026-09-10, the owner also specified that pushes should always go directly to `main`.
Carry tested fixes through commit and push without asking for confirmation again. Check the
workflows for the pushed commit and resolve regressions. Follow the release checks below when
publishing a new release.

## The collaboration that is driving most of the work

There is a second project, **EE_Review** (`C:\Users\zerav\OneDrive\Desktop\EE_Review`), a
course. It is the textbook; this is the simulator it leverages. It ships its lessons' circuits
as SPICE in `_audit/review/spice/` (194 files plus `index.json`), and running that corpus here
is a cross-check between two independent implementations.

**That corpus has found more real bugs in this program than any suite has.** Six in v3.32.0
alone. Run it:

    python tools/spice_run.py <corpus-directory> --output build/corpus.json

The runner now lives in the repository:

    python tools/spice_run.py "C:/Users/zerav/OneDrive/Desktop/EE_Review/_audit/review/spice" --output build/corpus.json
    python tools/spice_run.py --self-test

It records every file's stdout/stderr, exit code, residual, and skipped-line count. Its 13
classifier checks and seven real CLI checks run inside `cli-smoke`, so the battery still has
77 suites. The corpus command returns 1 when any circuit is rejected; the report distinguishes those outcomes from a crash.

The older count was **185 of 194 with a small solver residual, 149 with nothing skipped**.
Fixing m05l11-4 raises that same measure to **186 / 150**, but it is not a sufficient success
criterion: m05l11-5 already printed IMPLAUSIBLE (and exited 1), and m24l06 already printed KCL
VIOLATED (despite exiting 0). Both were counted as solutions in the old handoff.

With all diagnostics honored, the baseline was **183 accepted, 147 without skips**; the new
result is **185 accepted: 149 without skips and 36 with skips**, plus 8 refused
and 1 implausible result. **m05l11-4 and m24l06 improve; no circuit regresses.**

The extra improvement is a reporting fix: DC terminal-current readback stamped time -1e9
rather than time zero. A 60 Hz current source consequently reported 4.213 uA from floating-point
sine argument reduction even when its DC value was zero, giving m24l06 a false KCL violation.
Readback now uses zero for DC and the accepted step's start time for transient.

Warnings for anyone changing the classifier:

- A residual can precede `NOT A SOLUTION`, `IMPLAUSIBLE`, or `KCL VIOLATED`.
- Exactly one skipped element is printed as `skipped 1 line`, in the singular.
- Exit 0 alone is insufficient, and nonzero exit must not be counted as success.

`docs/EE_REVIEW_FINDINGS.md` is the full cross-project record: what was found on each side, what
is theirs to decide, what is ours.

## Next work, in the order I would do it

### Resolved: m05l11-4 and the missing Early derivative

Source stepping was tried with adaptive backtracking from zero. It got past the original
large-voltage cycle but stalled near 10.4% source strength. Before adding more iteration knobs,
a central finite difference was compared with the BJT's stamped Jacobian. Both NPN and PNP
collector derivatives disagreed: the model included `1 + Vce/Vaf` in collector current but
omitted its derivative `Go = Is * (exp(Vbe/nVt) - 1) / Vaf` from the matrix.

Stamping Go between collector and emitter, and subtracting Go*Vce from the equivalent current,
fixes the original circuit using ordinary Newton. Its residual falls from **0.0005183 A to
2.429e-17 A**, at full ±6 V supplies and 1 mA tail current. No source stepping or larger iteration
limit is needed. `--netlist-test` now has 30 checks, including both transistor Jacobians in
active and saturated operation and the original circuit. Removing Go makes all three new
checks fail; restoring it makes them pass.

The local named models give V(out) = **4.887035 V**, V(emit) = **-0.641160 V**, with Q2 active.
This does **not** confirm EE_Review's reported -0.50822 V saturated solution. The models are
not matched: the corpus comments and this repository's named-part defaults contain different
Is/BF/BR values, and this model includes Early effect. A parameter-matched comparison is still
open. The earlier conclusion that this circuit must cross a saturation boundary was a hypothesis,
not a property established for the local equations. ROADMAP.md preserves the investigation.

### 1. A 4-terminal SPDT switch part

8 more corpus lines. `X ... SPDT_SWITCH` is written `Xname A B common control`, e.g.
`XS0 vref 0 b0_sw b0_in SPDT_SWITCH`. There is no part behind it: `COMP_SPDT_SWITCH` has three
terminals and no control pin, `COMP_DPDT_DRIVEN` has seven.

This is a real part to build (descriptor, stamp, render, properties), not a mapping to add.
**Do not synthesise it in the reader from two analog switches** - that is the reader inventing
topology, which is the one thing it must not do.

`X ... ANALOG_SWITCH` is already done and is the pattern to follow: it mapped onto
`COMP_ANALOG_SWITCH`, which already was a controlled resistance (r_on 100 above v_on, r_off 1e9
below v_off). See `src/netlist.c`, the `case 'X'` block.

### 2. The rest of the false junctions

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

### 3. The grounded-input two-stage op-amp remains implausible

`m05l11-5` has a tiny reported solver residual but prints IMPLAUSIBLE (roughly 278 kA source
current). It is rejected by the CLI, and the new CLI audit checks the actual grounded-input
circuit. The older residual-only test omits VINP: it is a different, floating-input fixture,
now labeled as such. Do not mistake that test passing for the corpus circuit working.

The Kirchhoff auditor now groups actual solver nodes, so `Rail` and `rail` do not produce
false violations. It selects the worst error relative to each node's tolerance, so a large
branch cannot hide a smaller branch's violation. A genuine bad residual or KCL diagnostic
now causes a nonzero CLI exit. All these guards were checked by reintroducing their faults.

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

## Release audit configuration

The v3.33.0 release preparation fixes two audit-control bugs. The workflow used
`tag && '' || shard`, which always selected the nonempty shard, including on tags. Its condition
now selects a shard only for non-tag refs, and `run_audits.sh` refuses a tagged run with a shard.
That rejection is checked without executing suites using `AUDIT_LIST=1`.

The partition check now clears the inherited shard when reading its full reference list,
checks for unexpected units as well as omissions/duplicates, and honors the requested build
tree. Its per-shard lists explicitly model branch runs even when the caller is a release tag.
The manifests contain 93 work units (including split suites); the public battery count remains
77 suites. Three deliberate mutations were caught: an inherited partial reference list,
a release tag accepting a shard, and an unexpected work unit. The corrected checker passes
with both an inherited branch shard and a release-tag context.

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

Open questions with them: a parameter-matched comparison for m05l11-4, and their m05l16 lesson needs one edit (its distortion
procedure holds the input fixed while its formula assumes fixed output - the two differ by
(1 + gm*RE) squared rather than once; measured +27.7 dB against a stated +14, and +13.1 dB when
the output is held constant instead).
