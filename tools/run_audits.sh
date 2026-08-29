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

SMOKE_MODES="default --probe-test --probe-audit --label-test --span-test --demo-test --osc-test
--flow-test --switch-test --part-test --op-test --sub-test --spice-test --xtal-test --view-test
--conn-test --file-test --line-test --std-test --burn-test --knob-test --geom-test --param-test
--tesla-test"
APP_MODES="--layout-test --autoset-test --place-test"

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

start=$(date +%s)
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

fails=0
for m in $SMOKE_MODES $APP_MODES; do
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
    for m in $SMOKE_MODES $APP_MODES; do
        name="${m#--}"
        [ "$(cat "$out/$name.rc" 2>/dev/null)" = "fail" ] || continue
        echo
        echo "=== $m ==="
        grep -i "fail" "$out/$name.log" | head -40
    done
fi

echo
echo "audits: $fails of $(echo $SMOKE_MODES $APP_MODES | wc -w) suites failed, ${JOBS} at a time, $(( $(date +%s) - start ))s"
[ "$fails" -eq 0 ]
