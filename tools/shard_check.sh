#!/usr/bin/env bash
# The battery is sharded across CI legs with AUDIT_SHARD=i/n. This checks the partition is one:
# every unit of work runs on exactly one shard, and the union of the shards is the whole battery.
#
# Worth having as its own script rather than a comment, because the failure it guards is silent.
# A shard that drops a suite does not error - it prints a shorter list of passes and a green
# summary, which is the same shape as a suite sitting in no list at all. That one went unnoticed
# for months here.
#
#   bash tools/shard_check.sh [n]     # default 4, the size of the CI matrix
set -u
cd "$(dirname "$0")/.." || exit 2
n="${1:-4}"

# The calling CI leg exports its shard; that must not shrink the reference set.
full=$(AUDIT_LIST=1 AUDIT_SHARD= bash tools/run_audits.sh "${TREE:-build}" | sort)
[ -n "$full" ] || { echo "shard-check: the unsharded list is empty - is there a build?" >&2; exit 2; }

parts=""
i=0
while [ "$i" -lt "$n" ]; do
    # These are synthetic branch-leg lists, including when the caller is a release.
    one=$(GITHUB_REF=refs/heads/shard-check AUDIT_LIST=1 AUDIT_SHARD="$i/$n" bash tools/run_audits.sh "${TREE:-build}") || exit 2
    printf 'shard %d/%d: %d units\n' "$i" "$n" "$(echo "$one" | grep -c .)"
    parts="$parts$one
"
    i=$((i + 1))
done

union=$(printf '%s' "$parts" | grep -c .)
uniq_union=$(printf '%s' "$parts" | sort | uniq | grep -c .)
total=$(echo "$full" | grep -c .)

rc=0
if [ "$union" -ne "$uniq_union" ]; then
    echo "shard-check: FAIL - $((union - uniq_union)) unit(s) run on more than one shard:" >&2
    printf '%s' "$parts" | sort | uniq -d >&2
    rc=1
fi
missing=$(printf '%s' "$parts" | sort | uniq | comm -23 <(echo "$full") -)
if [ -n "$missing" ]; then
    echo "shard-check: FAIL - these run on NO shard, so nothing would run them:" >&2
    echo "$missing" >&2
    rc=1
fi
extra=$(printf '%s' "$parts" | sort | uniq | comm -13 <(echo "$full") -)
if [ -n "$extra" ]; then
    echo "shard-check: FAIL - these run on shards but are absent from the full battery:" >&2
    echo "$extra" >&2
    rc=1
fi
# Restore the old release configuration: it must be rejected before any suite starts.
if GITHUB_REF=refs/tags/shard-check AUDIT_LIST=1 AUDIT_SHARD=0/4 \
    bash tools/run_audits.sh "${TREE:-build}" >/dev/null 2>&1; then
    tag_rc=0
else
    tag_rc=$?
fi
if [ "$tag_rc" -ne 2 ]; then
    echo "shard-check: FAIL - a release tag accepted a partial audit (exit $tag_rc)" >&2
    rc=1
fi
if [ "$rc" -eq 0 ]; then
    echo "shard-check: $total units, $n shards, each unit on exactly one shard"
fi
exit $rc
