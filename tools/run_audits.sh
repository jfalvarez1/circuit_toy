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
--conn-test --file-test --parts-file-test --undo-test --session-test --ee-test --dpdt-test --residual-test --pin-test --bias-test --netlist-test --line-test --std-test --burn-test --knob-test --geom-test --param-test --sweep-check
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

out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
pids=""

run_one() {   # binary, mode
    local bin="$1" mode="$2" name="${2#--}" args=""
    [ "$mode" = "default" ] || args="$mode"
    if "$bin" $args > "$out/$name.log" 2>&1; then
        echo "ok" > "$out/$name.rc"
    else
        echo "fail" > "$out/$name.rc"
    fi
}

run_shard() {   # binary, mode-without-dashes, shard index, shard count
    local bin="$1" mode="$2" i="$3" n="$4" name="$2.$3" args=""
    [ "$mode" = "default" ] || args="--$mode"
    if "$bin" $args --shard "$i/$n" > "$out/$name.log" 2>&1; then
        echo "ok" > "$out/$name.rc"
    else
        echo "fail" > "$out/$name.rc"
    fi
}

start=$(date +%s)
for entry in $SHARDED; do
    mode="${entry%%:*}"; parts="${entry##*:}"
    i=0
    while [ "$i" -lt "$parts" ]; do
        while [ "$(jobs -pr | wc -l)" -ge "$JOBS" ]; do wait -n 2>/dev/null || break; done
        run_shard "$SMOKE" "$mode" "$i" "$parts" &
        i=$((i + 1))
    done
done
for entry in $APP_SHARDED; do
    mode="${entry%%:*}"; parts="${entry##*:}"
    i=0
    while [ "$i" -lt "$parts" ]; do
        while [ "$(jobs -pr | wc -l)" -ge "$JOBS" ]; do wait -n 2>/dev/null || break; done
        run_shard "$APP" "$mode" "$i" "$parts" &
        i=$((i + 1))
    done
done
for m in $SMOKE_MODES; do
    while [ "$(jobs -pr | wc -l)" -ge "$JOBS" ]; do wait -n 2>/dev/null || break; done
    run_one "$SMOKE" "$m" &
    pids="$pids $!"
done
for m in $APP_MODES; do
    while [ "$(jobs -pr | wc -l)" -ge "$JOBS" ]; do wait -n 2>/dev/null || break; done
    run_one "$APP" "$m" &
    pids="$pids $!"
done
wait

# the shards report as one line each, so a failing quarter names itself
SHARD_MODES=""
for entry in $SHARDED $APP_SHARDED; do
    mode="${entry%%:*}"; parts="${entry##*:}"; i=0
    while [ "$i" -lt "$parts" ]; do SHARD_MODES="$SHARD_MODES $mode.$i"; i=$((i + 1)); done
done

fails=0
for m in $SHARD_MODES $SMOKE_MODES $APP_MODES; do
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
    for m in $SHARD_MODES $SMOKE_MODES $APP_MODES; do
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
if command -v python >/dev/null 2>&1; then
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
if command -v python >/dev/null 2>&1; then
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
if command -v python >/dev/null 2>&1; then
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
if command -v python >/dev/null 2>&1; then
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
if command -v python >/dev/null 2>&1; then
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

# The one check that has to draw: a triggered trace has to stand still between frames, which is
# a property of the picture and not of any number. Skipped where pillow is not installed.
if command -v python >/dev/null 2>&1; then
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
if command -v python >/dev/null 2>&1; then
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
if command -v python >/dev/null 2>&1; then
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
if command -v python >/dev/null 2>&1; then
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
if command -v python >/dev/null 2>&1; then
    if python tools/edge_gui.py "$APP" > "$out/edgegui.log" 2>&1; then
        printf '[ OK ] %-14s %s
' "edge-gui" "$(tail -n 1 "$out/edgegui.log" | cut -c1-100)"
    else
        printf '[FAIL] %-14s %s
' "edge-gui" "$(tail -n 1 "$out/edgegui.log" | cut -c1-100)"
        grep -m5 FAIL "$out/edgegui.log"
        fails=$((fails + 1))
    fi
fi

# Every template's SVG export, through a real XML parser. Also written, also in no list.
if command -v python >/dev/null 2>&1; then
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
if command -v python >/dev/null 2>&1; then
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
echo "audits: $fails of $(echo $SHARD_MODES $SMOKE_MODES $APP_MODES | wc -w) suites failed, ${JOBS} at a time, $(( $(date +%s) - start ))s"

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
