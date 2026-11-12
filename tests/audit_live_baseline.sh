#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Compare JLD feature-branch regression failures to exactly the current main
# history, built with the same live-only firmware flag. This script NEVER
# checks out main over the working tree, edits baselines or changes a branch.
#
# Run from anywhere: sh tests/audit_live_baseline.sh /tmp/jld-eq-regression.log
set -eu

ROOT=$(git rev-parse --show-toplevel)
FEATURE_LOG=${1:-/tmp/jld-eq-regression.log}
case "$FEATURE_LOG" in
    /*) ;;
    *) FEATURE_LOG="$PWD/$FEATURE_LOG" ;;
esac
if [ ! -s "$FEATURE_LOG" ]; then
    echo "Missing feature regression log: $FEATURE_LOG" >&2
    exit 2
fi
cd "$ROOT"
git fetch --quiet origin main
MAIN_SHA=$(git rev-parse origin/main)
FEATURE_SHA=$(git rev-parse HEAD)
if ! git merge-base --is-ancestor "$MAIN_SHA" "$FEATURE_SHA"; then
    echo "STOP: feature branch does not contain latest origin/main ($MAIN_SHA)." >&2
    echo "Do not interpret failures until the base is reconciled." >&2
    exit 2
fi

AUDIT_DIR=$(mktemp -d "${TMPDIR:-/tmp}/jld-live-baseline-XXXXXX")
BASE_TREE="$AUDIT_DIR/main"
cleanup() {
    if [ -d "$BASE_TREE" ]; then
        git -C "$ROOT" worktree remove --force "$BASE_TREE" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT HUP INT TERM
git worktree add --quiet --detach "$BASE_TREE" "$MAIN_SHA"
echo "Live-only baseline audit"
echo "  main:    $MAIN_SHA"
echo "  feature: $FEATURE_SHA"
echo "  files:   $AUDIT_DIR"
echo "Building isolated main worktree with FELUCCA_NO_SEQUENCER=1 ..."
if (cd "$BASE_TREE" && FELUCCA_NO_SEQUENCER=1 sh build.sh) > "$AUDIT_DIR/main-build.log" 2>&1; then
    echo "main hardware build: PASS"
else
    echo "main hardware build: FAIL (tail follows)" >&2
    tail -n 30 "$AUDIT_DIR/main-build.log" >&2
    exit 1
fi
echo "Running baseline host regression (known legacy tests may fail) ..."
if (cd "$BASE_TREE" && FELUCCA_NO_SEQUENCER=1 sh tests/run_tests.sh) > "$AUDIT_DIR/main-tests.log" 2>&1; then
    MAIN_RC=0
else
    MAIN_RC=$?
fi
awk '/^HOST TEST STEP FAILED / {sub(/^HOST TEST STEP FAILED \([0-9]+\): /, ""); print}' "$AUDIT_DIR/main-tests.log" | LC_ALL=C sort -u > "$AUDIT_DIR/main-failed-steps.txt"
awk '/^HOST TEST STEP FAILED / {sub(/^HOST TEST STEP FAILED \([0-9]+\): /, ""); print}' "$FEATURE_LOG" | LC_ALL=C sort -u > "$AUDIT_DIR/feature-failed-steps.txt"

{
    echo "JLD baseline comparison (same source ancestry, live-only firmware build)"
    echo "main SHA: $MAIN_SHA"
    echo "feature SHA: $FEATURE_SHA"
    echo "baseline suite exit: $MAIN_RC"
    echo "feature suite log: $FEATURE_LOG"
    echo ""
    echo "FAILED STEPS ON BOTH:"
    comm -12 "$AUDIT_DIR/main-failed-steps.txt" "$AUDIT_DIR/feature-failed-steps.txt"
    echo ""
    echo "FAILED ONLY ON FEATURE (investigate as possible regressions):"
    comm -13 "$AUDIT_DIR/main-failed-steps.txt" "$AUDIT_DIR/feature-failed-steps.txt"
    echo ""
    echo "FAILED ONLY ON MAIN (fixed or configuration-specific):"
    comm -23 "$AUDIT_DIR/main-failed-steps.txt" "$AUDIT_DIR/feature-failed-steps.txt"
    echo ""
    echo "GOLDEN / CPU / UI RENDER DETAILS (main):"
    grep -E '^regress: 160 golden renders|^regress:.*changed|^ui_render: .*lint findings|^HOST TEST STEP FAILED .*regression:' "$AUDIT_DIR/main-tests.log" || true
    echo ""
    echo "GOLDEN / CPU / UI RENDER DETAILS (feature log):"
    grep -E '^regress: 160 golden renders|^regress:.*changed|^ui_render: .*lint findings|^HOST TEST STEP FAILED .*regression:' "$FEATURE_LOG" || true
} > "$AUDIT_DIR/report.txt"
cat "$AUDIT_DIR/report.txt"
echo ""
echo "Full baseline logs and report: $AUDIT_DIR"
echo "Note: identical failed-step names do not prove identical root causes;"
echo "compare the detailed lines in both logs before clearing any failure."
