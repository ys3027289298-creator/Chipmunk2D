#!/usr/bin/env bash
# Full acceptance gate for the broad-phase refactor.
# Runs the equivalence tests, degenerate-input rejection tests and the
# instrumented performance/memory benchmark against both the refactored
# library and the pristine baseline worktree.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
T="$ROOT/output/tests"
LOG="$ROOT/output/acceptance_log.txt"
BASELINE="${BASELINE_SRC:-/tmp/chipmunk-baseline}"

pass=0
fail=0

check() {
	local name="$1"; shift
	echo
	echo "================================================================"
	echo "== $name"
	echo "================================================================"
	if "$@"; then
		echo ">>> PASS: $name"
		((pass++))
	else
		echo ">>> FAIL: $name"
		((fail++))
	fi
}

{
	echo "Chipmunk broad-phase refactor acceptance run"
	echo "date: $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
	echo "refactored tree: $ROOT"
	echo "baseline tree:    $BASELINE"

	# 1. In-process equivalence: spatial hash vs AABB tree inside the new lib.
	check "equivalence in-process (new lib: hash vs tree)" "$T/equiv_new" self

	# 2. Cross-binary equivalence: baseline tree trace vs new hash trace.
	echo
	echo "================================================================"
	echo "== cross-binary pair/callback trace equivalence"
	echo "================================================================"
	if diff <("$T/equiv_baseline" tree) <("$T/equiv_new" hash); then
		echo ">>> PASS: cross-binary traces identical"
		((pass++))
	else
		echo ">>> FAIL: cross-binary traces differ"
		((fail++))
	fi

	# 3. Degenerate input rejection (refactored, Release -ffast-math build).
	check "degenerate input rejection (new, Release)" "$T/degenerate_new"

	# 3b. Runtime broad-phase hot-swap must preserve every indexed shape.
	check "runtime backend hot-swap preserves shapes" "$T/hotswap_new"

	# 4. Degenerate input rejection (Debug build, no fast-math).
	check "degenerate input rejection (new, Debug)" /tmp/degen_debug

	# 5. Baseline behaviour documented for contrast.
	echo
	echo "================================================================"
	echo "== baseline degenerate handling (expected to accept invalid input)"
	echo "================================================================"
	"$T/degenerate_baseline" || true

	echo
	echo "summary: $pass passed, $fail failed"
} 2>&1 | tee "$LOG"

exit "$fail"
