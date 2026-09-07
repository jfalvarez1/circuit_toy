#!/usr/bin/env bash
# The whole audit battery, run several at a time.
#
#   bash tools/run_audits.sh                 # uses ./build
#   bash tools/run_audits.sh build-static    # or any other build tree
#
# Every suite is its own process over its own copy of the templates, so they are independent and
# there is no reason to run them one after another. In CI they took seventeen minutes of a
# twenty-minute job that spends thirty-five seconds compiling.
set -u
tree="${1:-build}"
SMOKE="$tree/tools/template_smoke.exe"
APP="$tree/circuit-playground.exe"
[ -x "$SMOKE" ] || SMOKE="$tree/tools/template_smoke"
[ -x "$APP" ] || APP="$tree/circuit-playground"
if [ ! -x "$SMOKE" ] || [ ! -x "$APP" ]; then
    echo "no build in '$tree' - run: meson compile -C $tree" >&2
    exit 2
fi

# How many at once: the runner has 4 cores, a desktop usually more. One spare for the shell.
JOBS="${AUDIT_JOBS:-0}"
if [ "$JOBS" -le 0 ]; then
    JOBS=$(nproc 2>/dev/null || echo 4)
    JOBS=$((JOBS > 2 ? JOBS - 1 : 2))
fi

SMOKE_MODES="--probe-test --probe-audit --label-test --span-test --osc-test --dvdt-test --state-test --meas-test --fft-test --dcm-test --iv-test --conv-test --stress-test --mc-test --bode-test --sign-test --load-test --scope-test --class-test --restamp-test
--flow-test --pair-test --ic-test --sketch-test --mcu-test --direction-test --thermal-test --battery-test --gallery-test --switch-test --part-test --op-test --sub-test --spice-test --xtal-test --view-test
--conn-test --file-test --parts-file-test --undo-test --session-test --ee-test --dpdt-test --residual-test --pin-test --text-test --wire-test --bias-test --netlist-test --line-test --std-test --burn-test --knob-test --geom-test --param-test --sweep-check
--tesla-test"
APP_MODES="--layout-test --symbol-test --autoset-test --place-test --trig-test --prop-test --value-sweep --style-test --shot-test --flowdir-test"
# ...and one app suite is long enough to shard as well: --bounce-test renders sixty frames of
# every template through the real scope.
APP_SHARDED="bounce-test:4"
# The battery cannot finish faster than its longest single suite, and two of them are most of it:
# demo-test is two thirds on its own, and the plain load-and-run is the next. Both walk every
# template independently, so they run as shards - quarters of the template list, one process each.
SHARDED="demo-test:4 default:2"

# Every suite that exists has to be in one of the lists above. --scope-test was in none of them,
# and so nobody ran it: its expectation still said MIN_TIME_STEP was 1 ns long after the floor
# became 10 ps, and it sat there failing where no one would see. A suite in no list is a hole
# that looks like coverage, so this refuses to run a battery that has one.
orphans=""
for src in tools/template_smoke.c src/main.c; do
    [ -f "$src" ] || continue
    # Any suite, however it is named. This matched only names ending in -test, so --probe-audit,
    # --sweep-check and --value-sweep were never guarded at all: the check that exists to stop a
    # suite going unrun had three of them outside it. A suite is anything whose flag ends in
    # -test, -audit, -check or -sweep.
    for flag in $(grep -oE 'strcmp\(argv\[i\], "--[a-z-]+-(test|audit|check|sweep)"' "$src" | grep -oE '\-\-[a-z-]+-(test|audit|check|sweep)' | sort -u); do
        bare="${flag#--}"
        # ...except the two that are options rather than suites: the updater is asked whether to
        # look for a new release, which is a switch and not a check of anything.
        case "$flag" in --update-check|--no-update-check) continue ;; esac
        # $(echo ...) collapses the embedded newlines: SMOKE_MODES spans four lines, and a
        # newline is not a space, so a flag at the start of a line looked absent.
        case " $(echo $SMOKE_MODES $APP_MODES $SHARDED $APP_SHARDED) " in
            *" $flag "*|*" $bare:"*) ;;
            *) orphans="$orphans $flag" ;;
        esac
    done
done
if [ -n "$orphans" ]; then
    echo "run_audits: these suites exist but are in no list, so nothing runs them:$orphans" >&2
    echo "run_audits: add them to SMOKE_MODES or APP_MODES, or delete them." >&2
    exit 2
fi

# The same guard for the gates written in python, which the one above cannot see because it reads
# C source for command-line flags. Two of them - edge_gui.py and svg_audit.py - had been written,
# committed and then never run by anything: the check that exists to stop a suite going unrun had
# a whole language outside it. A gate is any tools/*.py that is not on the short list of things
# that are plainly not gates.
py_orphans=""
for f in tools/*.py; do
    [ -f "$f" ] || continue
    case "$f" in
        tools/copy_file.py|tools/make_media.py) continue ;;   # a file copier and the media script
    esac
    grep -q "$f" "$0" || py_orphans="$py_orphans $f"
done
if [ -n "$py_orphans" ]; then
    echo "run_audits: these python gates exist but nothing in this script runs them:$py_orphans" >&2
    echo "run_audits: add a block for each, or delete them." >&2
    exit 2
fi

# And the third way a suite can report green without checking anything: a check written as
# assert(), in a build that defines NDEBUG. The assertion is then not weak, not unreached - it is
# not COMPILED, and the suite prints its pass line and exits 0 with nothing behind it.
#
# This is not hypothetical and it is not ours. The KiCad session next door found nine assertions
# in its impedance suite compiled out by /O2 /Ob2 /DNDEBUG, and proved it by asserting something
# false and watching the suite pass. A real stripline error had been sitting behind them, and the
# existing assertion would have caught it on day one had it existed at runtime.
#
# No magnitude is quoted here on purpose. The first figure for that error was retracted a few
# hours later - the hand evaluation behind it had used a symbol with two meanings, and had been
# compared against a reference formula outside its own stated validity window. The size of the
# error was wrong; that an uncompiled assertion hid it was not. A number borrowed from someone
# else's still-open investigation does not belong in a comment that will outlive the investigation.
#
# There are no assert()s in tools/ today. This is here so that stays true: a check belongs in
# ordinary control flow that counts a failure and returns it as an exit code, which no build flag
# can remove. Meson does not define NDEBUG by default, so the hole is one -Db_ndebug=true away
# rather than present - which is exactly when a guard is cheap.
bad_assert=$(grep -rln '[^_a-zA-Z]assert[[:space:]]*(' tools/*.c 2>/dev/null || true)
if [ -n "$bad_assert" ]; then
    echo "run_audits: assert() used as a check in:$bad_assert" >&2
    echo "run_audits: a build with NDEBUG deletes it and the suite still prints PASS." >&2
    echo "run_audits: count the failure and return it as an exit code instead." >&2
    exit 2
fi

# Sharding the battery across CI legs.
#
# Measured on 5ce3a62: the audit step was 3050 s of a 3203 s leg - 95 % of it, against 79 s of
# compile - and all four matrix legs ran ALL of it. Four identical batteries in parallel, so the
# wall clock was one whole battery and the compute was four. AUDIT_SHARD=i/n gives this leg every
# nth unit of work instead: the cross-product then costs ONE battery spread four ways, which
# takes both the wait and the compute to about a quarter.
#
# Round-robin rather than blocks, because the units are wildly uneven - demo-test is two thirds
# of the battery on its own and is already split into quarters, so interleaving spreads the long
# ones instead of piling them onto one leg.
#
# What it costs, stated plainly: a suite now runs on ONE leg rather than on all four, so a fault
# that appears only on windows-2022, or only in a shared build, is caught only if that suite
# happened to land there. Every suite still runs on every push. Unset - which is how it runs
# locally and on a tag - is the whole battery, unchanged.
shard_i=-1; shard_n=1
if [ -n "${AUDIT_SHARD:-}" ]; then
    shard_i="${AUDIT_SHARD%%/*}"; shard_n="${AUDIT_SHARD##*/}"
    case "$shard_i$shard_n" in ''|*[!0-9]*) echo "run_audits: AUDIT_SHARD must be i/n" >&2; exit 2 ;; esac
    if [ "$shard_n" -lt 1 ] || [ "$shard_i" -ge "$shard_n" ]; then
        echo "run_audits: AUDIT_SHARD=$AUDIT_SHARD is out of range" >&2; exit 2
    fi
fi
unit=0
mine() {
    [ "$shard_i" -lt 0 ] && return 0
    r=$(( unit % shard_n )); unit=$((unit + 1)); [ "$r" -eq "$shard_i" ]
}

SEL_SHARDS=""; SEL_SMOKE=""; SEL_APP=""
for entry in $SHARDED; do
    mode="${entry%%:*}"; parts="${entry##*:}"; i=0
    while [ "$i" -lt "$parts" ]; do
        mine && SEL_SHARDS="$SEL_SHARDS SMOKE:$mode:$i:$parts"
        i=$((i + 1))
    done
done
for entry in $APP_SHARDED; do
    mode="${entry%%:*}"; parts="${entry##*:}"; i=0
    while [ "$i" -lt "$parts" ]; do
        mine && SEL_SHARDS="$SEL_SHARDS APP:$mode:$i:$parts"
        i=$((i + 1))
    done
done
for m in $SMOKE_MODES; do mine && SEL_SMOKE="$SEL_SMOKE $m"; done
for m in $APP_MODES;   do mine && SEL_APP="$SEL_APP $m"; done

# The python gates are units of work too, and leaving them out was the first version's mistake:
# sharding only the C suites took a local quarter-run from 400 s to 376 s, because these were
# still running in full on every leg. They are named here in a fixed order so every leg walks
# the same sequence and the partition is the same one shard_check verifies.
# edge-gui appears as four units, not one: it launches the app per template and was 2031 s of a
# 2031 s CI leg on its own. The heaviest unit sets the floor for the whole battery, so it divides.
PY_GATES="prop-wiring click-wiring key-wiring style-wiring thermal-wiring stamp-wiring stability undo-gui cli-smoke gui-smoke edge-gui.0 edge-gui.1 edge-gui.2 edge-gui.3 svg-audit keys-gui"
SEL_PY=""
for g in $PY_GATES; do mine && SEL_PY="$SEL_PY $g"; done
py_sel() { case " $SEL_PY " in *" $1 "*) return 0 ;; esac; return 1; }

# AUDIT_LIST=1 prints what this leg would run and exits. The point is that the partition is
# checkable without running anything: a shard that silently drops a suite is the same failure as
# a suite in no list, and that one went unnoticed for months. Compare the union of the shards
# against the unsharded list and they must match exactly - tools/shard_check.sh does that.
if [ -n "${AUDIT_LIST:-}" ]; then
    for u in $SEL_SHARDS; do
        rest="${u#*:}"; mode="${rest%%:*}"; rest="${rest#*:}"; si="${rest%%:*}"
        echo "$mode.$si"
    done
    for m in $SEL_SMOKE $SEL_APP; do echo "$m"; done
    for g in $SEL_PY; do echo "py:$g"; done
    exit 0
fi

# And the partition itself is a thing that can break silently, so it is checked rather than
# trusted. A shard that drops a suite prints a shorter list of passes and a green summary - the
# same shape as a suite in no list, which is the failure the guard above exists for. Runs after
# the AUDIT_LIST exit above, which is what stops this recursing: shard_check calls back into this
# script with AUDIT_LIST set, and that returns before reaching here.
if [ -f tools/shard_check.sh ]; then
    if ! bash tools/shard_check.sh 4 >/dev/null 2>&1; then
        echo "run_audits: the AUDIT_SHARD partition is not a partition - some suite would run on" >&2
        echo "run_audits: no leg, or on two. Run: bash tools/shard_check.sh 4" >&2
        exit 2
    fi
fi

out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
pids=""

run_one() {   # binary, mode
    local bin="$1" mode="$2" name="${2#--}" args="" t0
    [ "$mode" = "default" ] || args="$mode"
    t0=$(date +%s)
    if "$bin" $args > "$out/$name.log" 2>&1; then
        echo "ok" > "$out/$name.rc"
    else
        echo "fail" > "$out/$name.rc"
    fi
    echo $(( $(date +%s) - t0 )) > "$out/$name.sec"
}

run_shard() {   # binary, mode-without-dashes, shard index, shard count
    local bin="$1" mode="$2" i="$3" n="$4" name="$2.$3" args="" t0
    [ "$mode" = "default" ] || args="--$mode"
    t0=$(date +%s)
    if "$bin" $args --shard "$i/$n" > "$out/$name.log" 2>&1; then
        echo "ok" > "$out/$name.rc"
    else
        echo "fail" > "$out/$name.rc"
    fi
    echo $(( $(date +%s) - t0 )) > "$out/$name.sec"
}

start=$(date +%s)
for u in $SEL_SHARDS; do
    binv="${u%%:*}"; rest="${u#*:}"
    mode="${rest%%:*}"; rest="${rest#*:}"
    si="${rest%%:*}"; sn="${rest##*:}"
    while [ "$(jobs -pr | wc -l)" -ge "$JOBS" ]; do wait -n 2>/dev/null || break; done
    if [ "$binv" = "SMOKE" ]; then run_shard "$SMOKE" "$mode" "$si" "$sn" &
    else                           run_shard "$APP"   "$mode" "$si" "$sn" &
    fi
done
for m in $SEL_SMOKE; do
    while [ "$(jobs -pr | wc -l)" -ge "$JOBS" ]; do wait -n 2>/dev/null || break; done
    run_one "$SMOKE" "$m" &
    pids="$pids $!"
done
for m in $SEL_APP; do
    while [ "$(jobs -pr | wc -l)" -ge "$JOBS" ]; do wait -n 2>/dev/null || break; done
    run_one "$APP" "$m" &
    pids="$pids $!"
done
wait

# the shards report as one line each, so a failing quarter names itself. Only the ones this leg
# actually ran: a missing .rc counts as a failure, which is deliberate, so reporting a unit that
# was never dispatched here would turn every other leg's work into a red line.
SHARD_MODES=""
for u in $SEL_SHARDS; do
    rest="${u#*:}"; mode="${rest%%:*}"; rest="${rest#*:}"; si="${rest%%:*}"
    SHARD_MODES="$SHARD_MODES $mode.$si"
done

fails=0
for m in $SHARD_MODES $SEL_SMOKE $SEL_APP; do
    name="${m#--}"
    rc=$(cat "$out/$name.rc" 2>/dev/null || echo fail)
    last=$(tail -n 1 "$out/$name.log" 2>/dev/null | cut -c1-100)
    if [ "$rc" = "ok" ]; then
        printf '[ OK ] %-14s %s\n' "$m" "$last"
    else
        fails=$((fails + 1))
        printf '[FAIL] %-14s %s\n' "$m" "$last"
    fi
done

# A failure is worth its whole log, not just its last line.
if [ "$fails" -gt 0 ]; then
    for m in $SHARD_MODES $SEL_SMOKE $SEL_APP; do
        name="${m#--}"
        [ "$(cat "$out/$name.rc" 2>/dev/null)" = "fail" ] || continue
        echo
        echo "=== $m ==="
        grep -i "fail" "$out/$name.log" | head -40
    done
fi

# Source-level, so it needs no binary: every property type has to be wired at both ends. A
# handler with no row is a part that looks unconfigurable while the plumbing sits there; a row
# with no handler looks configurable, takes a value and drops it. Both existed.
if py_sel prop-wiring && command -v python >/dev/null 2>&1; then
    if python tools/prop_wiring.py > "$out/propwiring.log" 2>&1; then
        printf '[ OK ] %-14s %s
' "prop-wiring" "$(tail -n 1 "$out/propwiring.log" | cut -c1-100)"
    else
        printf '[FAIL] %-14s %s
' "prop-wiring" "$(tail -n 1 "$out/propwiring.log" | cut -c1-100)"
        grep -i fail "$out/propwiring.log" | head -10
        fails=$((fails + 1))
    fi
fi

# And source-level again: a button that is drawn has to be hit-tested by something, or it is a
# control that is painted, hovered, labelled and dead - which looks exactly like a working one.
# --layout-test covers the other way a button becomes unreachable, by being overlapped.
if py_sel click-wiring && command -v python >/dev/null 2>&1; then
    if python tools/click_wiring.py > "$out/clickwiring.log" 2>&1; then
        printf '[ OK ] %-14s %s
' "click-wiring" "$(tail -n 1 "$out/clickwiring.log" | cut -c1-100)"
    else
        printf '[FAIL] %-14s %s
' "click-wiring" "$(tail -n 1 "$out/clickwiring.log" | cut -c1-100)"
        grep -i fail "$out/clickwiring.log" | head -10
        fails=$((fails + 1))
    fi
fi

# Source-level: a keyboard shortcut the program promises has to be one it handles. The F1 dialog,
# the guide's tables and the buttons' own tooltips all name keys, and nothing tied any of them to
# the code: F5, F6, F10, F12, Ctrl+O, Ctrl+N and "." were advertised with no handler at all, and
# two more were advertised as doing something they do not.
if py_sel key-wiring && command -v python >/dev/null 2>&1; then
    if python tools/key_wiring.py > "$out/keywiring.log" 2>&1; then
        printf '[ OK ] %-14s %s
' "key-wiring" "$(tail -n 1 "$out/keywiring.log" | cut -c1-100)"
    else
        printf '[FAIL] %-14s %s
' "key-wiring" "$(tail -n 1 "$out/keywiring.log" | cut -c1-100)"
        grep -m8 FAIL "$out/keywiring.log"
        fails=$((fails + 1))
    fi
fi

# Source-level: the schematic style is a mapping bolted in front of two SDL calls by macros, so
# it only covers a file that includes style.h, only the calls below that include, and only while
# the canvas flag is armed. A new drawing file draws in raw synthwave on white paper and nothing
# reports it; leaving the flag armed turns the toolbar into empty white boxes, which it did.
if py_sel style-wiring && command -v python >/dev/null 2>&1; then
    if python tools/style_wiring.py > "$out/stylewiring.log" 2>&1; then
        printf '[ OK ] %-14s %s
' "style-wiring" "$(tail -n 1 "$out/stylewiring.log" | cut -c1-100)"
    else
        printf '[FAIL] %-14s %s
' "style-wiring" "$(tail -n 1 "$out/stylewiring.log" | cut -c1-100)"
        grep -i fail "$out/stylewiring.log" | head -10
        fails=$((fails + 1))
    fi
fi

# Also source-level: a part that claims a temperature limit has to have a power expression the
# damage model can actually read. Every one of them was reading a field the loop wrote back to
# itself, so nothing had ever burned; the electrolytic had no case at all.
if py_sel thermal-wiring && command -v python >/dev/null 2>&1; then
    if python tools/thermal_wiring.py > "$out/thermalwiring.log" 2>&1; then
        printf '[ OK ] %-14s %s
' "thermal-wiring" "$(tail -n 1 "$out/thermalwiring.log" | cut -c1-100)"
    else
        printf '[FAIL] %-14s %s
' "thermal-wiring" "$(tail -n 1 "$out/thermalwiring.log" | cut -c1-100)"
        grep -i fail "$out/thermalwiring.log" | head -10
        fails=$((fails + 1))
    fi
fi

# And source-level again: a part whose stamp writes into a current row has to have been given
# one. Two sources shipped without - each with a complete, correct stamp landing in the first
# voltage source's equation, which converges and puts the wrong answer somewhere else entirely.
if py_sel stamp-wiring && command -v python >/dev/null 2>&1; then
    if python tools/stamp_wiring.py > "$out/stampwiring.log" 2>&1; then
        printf '[ OK ] %-14s %s
' "stamp-wiring" "$(tail -n 1 "$out/stampwiring.log" | cut -c1-100)"
    else
        printf '[FAIL] %-14s %s
' "stamp-wiring" "$(tail -n 1 "$out/stampwiring.log" | cut -c1-100)"
        grep -i fail "$out/stampwiring.log" | head -10
        fails=$((fails + 1))
    fi
fi

# The one check that has to draw: a triggered trace has to stand still between frames, which is
# a property of the picture and not of any number. Skipped where pillow is not installed.
if py_sel stability && command -v python >/dev/null 2>&1; then
    if python tools/trace_stability.py "$APP" > "$out/stability.log" 2>&1; then
        printf '[ OK ] %-14s %s
' "stability" "$(tail -n 1 "$out/stability.log" | cut -c1-100)"
    else
        printf '[FAIL] %-14s %s
' "stability" "$(tail -n 1 "$out/stability.log" | cut -c1-100)"
        grep -i fail "$out/stability.log" | head -10
        fails=$((fails + 1))
    fi
fi

# and one that drives the app itself: delete a part with the tool, press Ctrl+Z, look at the
# canvas. Everything else about undo is checked by calling the circuit functions directly.
if py_sel undo-gui && command -v python >/dev/null 2>&1; then
    if python tools/undo_gui.py "$APP" > "$out/undogui.log" 2>&1; then
        printf '[ OK ] %-14s %s
' "undo-gui" "$(grep -m1 'OK\|skipped' "$out/undogui.log" | cut -c1-100)"
    else
        printf '[FAIL] %-14s %s
' "undo-gui" "$(grep -m1 FAIL "$out/undogui.log" | cut -c1-100)"
        fails=$((fails + 1))
    fi
fi

# Every command-line OPTION, exercised once with a real assertion. The suites are guarded by the
# orphan check above; the options were guarded by nothing, and fourteen of them were passed by no
# tool, gate or workflow at all. One of them, --prop-gap, turned out to be a whole diagnostic
# suite that nothing ran because its name does not end in "-test".
if py_sel cli-smoke && command -v python >/dev/null 2>&1; then
    if python tools/cli_smoke.py --exe "$APP" > "$out/clismoke.log" 2>&1; then
        printf '[ OK ] %-14s %s
' "cli-smoke" "$(tail -n 1 "$out/clismoke.log" | cut -c1-100)"
    else
        printf '[FAIL] %-14s %s
' "cli-smoke" "$(tail -n 1 "$out/clismoke.log" | cut -c1-100)"
        grep -m8 FAIL "$out/clismoke.log"
        fails=$((fails + 1))
    fi
fi

# The app driven the way a user drives it: place a template, press the toolbar, pick up a tool,
# drag the canvas, and look at the pixels that came out. This existed and was in no list either -
# and when it was finally run, three of its four interaction checks were failing on coordinates
# that had been typed into the script instead of read from the app. --quick, because a launch per
# template over 205 templates is three quarters of an hour.
if py_sel gui-smoke && command -v python >/dev/null 2>&1; then
    if python tools/gui_smoke.py --quick --exe "$APP" --smoke "$SMOKE" > "$out/guismoke.log" 2>&1; then
        printf '[ OK ] %-14s %s
' "gui-smoke" "$(tail -n 1 "$out/guismoke.log" | cut -c1-100)"
    else
        printf '[FAIL] %-14s %s
' "gui-smoke" "$(tail -n 1 "$out/guismoke.log" | cut -c1-100)"
        grep -m8 FAIL "$out/guismoke.log"
        fails=$((fails + 1))
    fi
fi

# Nothing a template draws may run off the edge of the canvas. This existed and was in no list,
# so from the day it was written until now nothing ran it.
# Split four ways, because this one unit WAS the wall clock. It launches the app once per
# template, and on the first sharded CI run it took 2031 s while the other three legs finished in
# 360, 409 and 775 - so no assignment of whole units could have balanced it. A battery cannot
# finish faster than its largest indivisible piece, and the answer to that is to divide the piece
# rather than to shuffle it between legs.
if command -v python >/dev/null 2>&1; then
    for _es in 0 1 2 3; do
        py_sel "edge-gui.$_es" || continue
        _elog="$out/edgegui.$_es.log"
        _t0=$(date +%s)
        if python tools/edge_gui.py "$APP" "$_es/4" > "$_elog" 2>&1; then
            printf '[ OK ] %-14s %s
' "edge-gui.$_es" "$(tail -n 1 "$_elog" | cut -c1-100)"
        else
            printf '[FAIL] %-14s %s
' "edge-gui.$_es" "$(tail -n 1 "$_elog" | cut -c1-100)"
            grep -m5 FAIL "$_elog"
            fails=$((fails + 1))
        fi
        echo $(( $(date +%s) - _t0 )) > "$out/edge-gui.$_es.sec"
    done
fi

# Every template's SVG export, through a real XML parser. Also written, also in no list.
if py_sel svg-audit && command -v python >/dev/null 2>&1; then
    if python tools/svg_audit.py "$SMOKE" > "$out/svgaudit.log" 2>&1; then
        printf '[ OK ] %-14s %s
' "svg-audit" "$(tail -n 1 "$out/svgaudit.log" | cut -c1-100)"
    else
        printf '[FAIL] %-14s %s
' "svg-audit" "$(tail -n 1 "$out/svgaudit.log" | cut -c1-100)"
        grep -m5 -i fail "$out/svgaudit.log"
        fails=$((fails + 1))
    fi
fi

# and the shortcuts, asked of the app itself rather than of a picture
if py_sel keys-gui && command -v python >/dev/null 2>&1; then
    if python tools/keys_gui.py "$APP" > "$out/keysgui.log" 2>&1; then
        printf '[ OK ] %-14s %s
' "keys-gui" "$(tail -n 1 "$out/keysgui.log" | cut -c1-100)"
    else
        printf '[FAIL] %-14s %s
' "keys-gui" "$(tail -n 1 "$out/keysgui.log" | cut -c1-100)"
        grep -m5 FAIL "$out/keysgui.log"
        fails=$((fails + 1))
    fi
fi

echo
echo "audits: $fails of $(echo $SHARD_MODES $SEL_SMOKE $SEL_APP | wc -w) suites failed, ${JOBS} at a time, $(( $(date +%s) - start ))s"

# What each unit cost, longest first.
#
# There is a reason this is worth printing rather than just knowing the total. The battery is
# sharded across four CI legs by ROUND ROBIN, which balances by count and not by time - and the
# first sharded run came out 360 s, 409 s, 775 s and 2031 s, because one leg happened to draw the
# expensive units. All four had 22 or 23 of the 89, and that told nobody anything.
#
# These numbers are what a longest-first assignment needs, and they are worth having on their own:
# a suite that quietly doubles in cost is invisible in a total that is dominated by the slowest
# one, and shows up here immediately.
if [ -n "${AUDIT_TIMES:-}" ]; then
    echo "audits: unit cost, longest first -"
    for m in $SHARD_MODES $SEL_SMOKE $SEL_APP; do
        name="${m#--}"
        s=$(cat "$out/$name.sec" 2>/dev/null || echo 0)
        printf '%6s %s\n' "$s" "$m"
    done | sort -rn | head -20
fi

# A gate that skipped is not a gate that passed.
#
# Three of them - trace-stability, undo-gui and edge-gui - need pillow to read the pixels the app
# drew, and without it they print "needs pillow; skipped" and return 0. They did that in CI from
# the day they were written, so a green run said nothing whatever about any of them, and the line
# saying so scrolled past among sixty others. It is now counted and said out loud at the end,
# where the failure count is read.
# "needs X; skipped" is a gate declining to run. Plain "skipped" is not: --trig-test skips
# one-shots and bounce-test counts skipped templates, both of which are ordinary results. Match
# the declining form only, and say nothing at all when nothing declined.
skips=""
for f in "$out"/*.log; do
    [ -f "$f" ] || continue
    line=$(grep -i "needs .*; *skipped" "$f" 2>/dev/null | head -n 1)
    [ -n "$line" ] && skips="$skips
         $(basename "$f" .log): $line"
done
if [ -n "$skips" ]; then
    echo
    echo "audits: NOTE - these gates skipped rather than ran; they cover nothing here:$skips"
    echo "audits: install what they ask for (pip install pillow numpy) to actually run them."
fi

[ "$fails" -eq 0 ]
