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

SMOKE_MODES="--probe-test --probe-audit --label-test --span-test --osc-test
--flow-test --switch-test --part-test --op-test --sub-test --spice-test --xtal-test --view-test
--conn-test --file-test --parts-file-test --undo-test --line-test --std-test --burn-test --knob-test --geom-test --param-test
--tesla-test"
APP_MODES="--layout-test --autoset-test --place-test --trig-test --prop-test"
# The battery cannot finish faster than its longest single suite, and two of them are most of it:
# demo-test is two thirds on its own, and the plain load-and-run is the next. Both walk every
# template independently, so they run as shards - quarters of the template list, one process each.
SHARDED="demo-test:4 default:2"

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
for entry in $SHARDED; do
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

echo
echo "audits: $fails of $(echo $SHARD_MODES $SMOKE_MODES $APP_MODES | wc -w) suites failed, ${JOBS} at a time, $(( $(date +%s) - start ))s"
[ "$fails" -eq 0 ]
