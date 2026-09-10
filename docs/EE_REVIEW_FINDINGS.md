# Findings for the EE_Review course, from running its corpus (2026-09-07)

EE_Review is the textbook; this program is the simulator it leverages. Its `_audit/review/spice/`
corpus is 194 SPICE files, and running them here is a cross-check between two independent
implementations - which is the only reason any of the items below were found. Neither side found
them by reading its own code.

This file exists because the EE_Review session was not running when these were finalised, and a
message to a closed session is lost rather than queued. It is the handoff. Everything here is
either a decision for whoever owns the curriculum, or a defect in the corpus build.

## State of the cross-check

Local follow-up on 2026-09-09, using the same 194-file corpus and the new checked runner:

    185 of 194 accepted after inspecting every diagnostic and the exit status
        149 of those with nothing skipped
         36 with at least one element line this reader could not place
      8 refuse
      1 implausible - m05l11-5; source current about 278 kA, despite a small residual
     66 element lines skipped in total

The pre-fix baseline under that same classification is 183 accepted, 147 without skips.
**m05l11-4 and m24l06 improve, with no regressions.** The former goes from NOT A SOLUTION
to an accepted solution; the latter had a false KCL warning from a DC current-readback bug.
The old 185/149 count included both that KCL warning and the implausible result. After these
fixes the old residual-only measure is 186/150, whereas the strict measure is 185/149. Those
are different measures and must not be mixed.

**m24l06 was a reporting bug, now fixed.** The DC solver stamped sources at time zero, but
terminal-current readback re-stamped at -1e9 seconds, confusing the large DC storage pseudo-step
with elapsed time. Evaluating a 60 Hz sine there introduced 4.213 uA of roundoff. The solved
currents were correct; the displayed source current was not. Readback now uses time zero, and
DC plus five transient samples are checked against the source equation.

The auditor also now groups solver-node equivalence classes rather than case-sensitive net
names, compares KCL errors against each node's own tolerance, and returns a failing exit for
bad residuals or KCL diagnostics. Seven CLI cases check accepted and rejected circuits, including
the actual grounded-input m05l11-5; all regression guards were mutation-checked.

Run `python tools/spice_run.py <corpus-directory> --output build/corpus.json`. The report keeps
all diagnostics so the instrument can be checked against the actual output. Its classifier
self-checks run in `cli-smoke` and cover singular/plural skips, explicit refusal and failure
messages, nonfinite residuals, missing output and nonzero exit codes.

CORRECTION TO AN EARLIER FIGURE. A "167 clean" number was reported before this and was wrong.
The harness counting it matched `skipped (\d+) lines` while the reader writes the SINGULAR
"skipped 1 line" for a single one, so 21 files with exactly one unplaced element were counted as
having none. That is the second time today the measuring script has flattered the result rather
than reported it - the first counted "NOT A SOLUTION" outputs as solves - and both errors ran in
the same direction, which is the part worth remembering about instruments you wrote yourself.

Their `.cir` files now run unmodified through `--netlist-solve`, and `--netlist-bode FILE FSTART
FSTOP N NODE [AMPLITUDE]` answers AC claims from the same file. The bode path is validated
against an RC whose corner is 1591.5 Hz in closed form (it reads 1568) and against their own
m05l24 (27.0 MHz measured against their stated 27.5).

## Items for the curriculum owner

**1. Six zener tables state no breakdown voltage.** This program has one generic `COMP_ZENER`
with an editable `vz` (default 5.1 V, `rz` 7 ohm, refdes DZ) and no per-voltage parts, so the
table has to name the number because the part will never supply it. Six lessons, six decisions.
m23l04's refusal to converge is a correct complaint until its clamp voltage is stated. This is
the largest block of unfinished material.

**2. Their zener model has no series resistance; this one has rz = 7 ohm.** Once those six
tables state their voltages the two will disagree on clamp current, and theirs is the one that
is wrong. Worth fixing before the tables land, or the disagreement will read as a solver
difference rather than a model one.

**3. The m06l09 cascode bias is a topology fault, and there is no vb that works.** Swept 1.30 to
1.90 V at 10 mV:

    vb 1.30 - 1.60   no operating point in EITHER solver - the equations have no root
    vb 1.70          M4C saturated by 7.8 mV, but nref2 = 149.8 V
    vb 1.705         M4C leaves saturation, nref2 still ~130 V
    vb 1.80          nref2 = 1.09 V at last, and M4C is 98 mV into triode

The two requirements move in opposite directions and never overlap. So it is not a number to
nudge: the cascode reference needs diode-connecting, or vb needs generating from the reference
branch. Design intent, so it belongs to the course.

Both solvers refusing 1.30-1.60 is worth recording on its own - that window is a property of the
circuit, not a robustness limit in either implementation.

**4. Two AC lessons assume an ideal transistor and do not say so.** m05l15 writes CPI = 25 pF
and CMU = 4 pF as discrete parts, and those ARE a 2N3904's own capacitances; m05l24's CL = 10 pF
is meant to be the whole load capacitance. Any simulator carrying a real device model counts
them twice:

                   with a real 2N3904      charge-free device     the lesson states
    m05l15 fH          276 kHz                 620 kHz                481 kHz
    m05l24 fH         18.0 MHz                27.0 MHz               27.5 MHz
    m05l24 ratio         1.53                    1.705                  1.73

Writing the capacitances discretely is a good teaching choice - it is what makes Miller visible.
It needs one line saying the transistor is otherwise ideal, or a reader with a real simulator
has two numbers and no way to choose between them.

For the record, m05l24's stated figure is right by construction and worth saying in the lesson:
the unpeaked corner puts C at 10.1 pF, so L/(R^2 C) = 4.1u/(1k^2 * 10p) = 0.41, the textbook
optimum for maximally flat series peaking, whose bandwidth improvement is 1.73. The 4.1 uH was
not chosen by trial.

## Defects in the corpus build

**5. m05l24 has a floating output node.** `out` sits between COUT (1 uF) and CL (10 pF) with
nothing resistive on it - no DC return path anywhere. The DC solve reports 0.000000 V, which is
regularisation inventing a number for a node that has none, and in transient it drifts. Measure
at col2 instead. Same family as m15l06's missing ground: a node whose voltage is not determined,
reported confidently.

**6. Three files carry duplicate refdes, and the pattern says one extraction bug.**

    m05l11-4   VIN_ on inp, VIN_ on inm
    m23l06     Vecg_ on inp, Vecg_ on inn
    m26l06     Rin_ twice, Vin_ twice

Every case is a DIFFERENTIAL part - a two-ended source or a matched input pair - written as one
table row and emitted as two elements sharing that row's single name. The table builder is
dropping the +/- suffix. Real SPICE refuses a duplicate refdes outright.

**7. m26l06 is the one that bites.** Its two `Vin_` lines are on the SAME node pair: two
independent voltage sources in parallel across IN_ and 0. While this reader misparsed `AC 1m` as
a DC value the two disagreed and the matrix was singular, so the circuit was refused for a
reason unrelated to the real defect. Now they agree at DC and it solves - which is worse,
because the duplicate is still there and nothing complains.

## Ours, not theirs

**8. m05l11-4 now solves locally.** Source stepping was tried but stalled near 10.4% supply.
A finite-difference check exposed a missing derivative of the Early-effect collector current.
Adding that term and its equivalent-current correction makes ordinary Newton converge, lowering
residual from 0.0005183 A to 2.429e-17 A. Both transistor polarities and the original circuit now
have mutation-checked regressions in `--netlist-test`.

This repository's named models give V(out) = 4.887035 V, V(emit) = -0.641160 V, with Q2 active.
That differs from EE_Review's -0.50822 V saturated result. The corpus comments and the local
named parts have different Is/BF/BR values, and the local model includes Early effect. We have
not yet done a parameter-matched comparison, so convergence here must not be reported as
agreement between the two simulators. See the new top entry in ROADMAP.md.

## Open questions for them

**9a. ANSWERED, and the first one is built.** They ranked the blocks by family and said to build
the switch first and as ONE part rather than five, since ANALOG_SWITCH / SPDT / DPDT / IDEAL /
PWM-driven are all a controlled resistance with different pin counts.

`X ... ANALOG_SWITCH` now maps to `COMP_ANALOG_SWITCH`, which already was that model - r_on 100
when the control pin is above v_on, r_off 1e9 below v_off. Nine lines, no new part, and the
guard checks both states against arithmetic (a 9k/1k divider reads 9.10 V with the switch closed
across the 9k and 1.00 V with it open).

The rest of the family is NOT done, because a 4-terminal SPDT with its own control pin has no
part behind it. Synthesising one in the reader out of two analog switches would be the reader
inventing topology, which is the one thing it must not do. That is a real part to build, not a
mapping to add: 8 more lines.

**9b. ANSWERED, and both checked.** m07l04 confirmed on all six perturbations including their
base-drive test (1.4022 A with Q2 off, against their ~1.4 A). m05l16 confirmed on bias, gain and
two of three distortion ratios - the third is a same-input / same-output mismatch in the lesson,
written up in item 10.

## 10. m05l16's distortion procedure measures something its formula does not

Their claim: shorting RE1 raises HD2 by 14 dB, from (1 + gm*RE1) = 5.06. Measured both ways:

    baseline 10 mV in, RE1 = 100        output 0.3709 Vpk    HD2 -48.03 dBc
    RE1 = 1 mohm, still 10 mV in        output 2.0005 Vpk    HD2 -20.33 dBc    +27.7 dB
    RE1 = 1 mohm, drive 1.854 mV        output 0.3772 Vpk    HD2 -34.92 dBc    +13.1 dB

The formula is right; the PROCEDURE measures a different comparison. Local series feedback cuts
HD2 by (1 + gm*RE) at a fixed OUTPUT. Hold the INPUT fixed instead, as the lesson says to, and
the output also grows by (1 + gm*RE) - and HD2 grows with output amplitude, so the two factors
multiply and the answer is (1 + gm*RE)^2, or 29.3 dB against the 27.7 measured.

Either fix works and it is a teaching decision: say "reduce the drive to 1.85 mV so the output
stays at 377 mV" and keep +14 dB, which also keeps their line about 14 dB of distortion buying
14 dB of gain since that framing is already the same-output one; or keep the 10 mV drive and
state the rise as about 28 dB, noting that (1 + gm*RE) appears twice, once in the linearity and
once in the amplitude.

Measured by transient plus FFT rather than any built-in distortion analysis, so the instrument
is outside the solver: twelve cycles at 1 kHz, the last eight Hann-windowed.

## The pattern the whole exchange kept producing

Every fault found on either side today produced a PLAUSIBLE ANSWER rather than an error. A
current source silently taking 1 mA instead of 200 uA. A PWL source stamping into another
element's matrix row. A frequency sweep reporting an amplifier's turn-on as its gain. A
differential pair reporting sensible node voltages 2.876 A from satisfying KCL. Two schematics
here drawing a signal net and a supply net down the same line while solving perfectly.

None of them announced themselves, and that is the argument for the corpus continuing to exist.
