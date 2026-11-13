#!/usr/bin/env bash
# Record the acceptance workflow as segmented terminal sessions.
# Each segment starts with a banner title; output goes to output/recording/.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
REC="$ROOT/output/recording"
T="$ROOT/output/tests"
mkdir -p "$REC/segments"

banner() {
	local title="$1"
	printf '\033[1;36m\n'
	printf '########################################################################\n'
	printf '# %s\n' "$title"
	printf '########################################################################\n'
	printf '\033[0m\n'
}

run_segment() {
	local name="$1"; local title="$2"; shift 2
	local script_file="$REC/segments/${name}.sh"
	local cast_file="$REC/segments/${name}.cast"

	{
		echo 'banner() {'
		echo '  printf "\033[1;36m\n"'
		echo '  printf "########################################################################\n"'
		echo '  printf "# %s\n" "$1"'
		echo '  printf "########################################################################\n"'
		echo '  printf "\033[0m\n"'
		echo '}'
		echo "cd $(printf '%q' "$ROOT")"
		echo "banner $(printf '%q' "$title")"
		echo "$*"
		echo 'echo'
		echo 'echo "[segment done]"'
	} > "$script_file"

	# asciinema records a real PTY; env TERM keeps colors deterministic.
	TERM=xterm-256color asciinema rec -c "bash $script_file" "$cast_file" >/dev/null
	echo "recorded $cast_file"
}

run_segment 01_build "SEGMENT 1/5: BUILD REFACTORED LIBRARY" \
	'cmake --build build-new -j4 2>&1 | tail -2'

run_segment 02_equivalence "SEGMENT 2/5: COLLISION PAIR EQUIVALENCE (hash vs tree)" \
	"./output/tests/equiv_new self"

run_segment 03_cross_binary "SEGMENT 3/5: CROSS-BINARY TRACE vs PRISTINE BASELINE" \
	"diff <(./output/tests/equiv_baseline tree) <(./output/tests/equiv_new hash) && echo 'PAIR TRACES IDENTICAL'"

run_segment 04_degenerate "SEGMENT 4/5: DEGENERATE SHAPE REJECTION" \
	"./output/tests/degenerate_new"

run_segment 05_perf "SEGMENT 5/5: PERFORMANCE AND MEMORY (linear bound)" \
	"./output/tests/bench_new_instr hash 2>/dev/null"

echo "all segments recorded in $REC/segments"
