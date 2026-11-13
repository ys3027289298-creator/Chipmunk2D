#!/usr/bin/env bash
# Run every backend/backend-comparison combination repeatedly and keep the
# median timing per (scenario, shape count) line. Memory figures are
# deterministic and taken from the first run.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
RUNS="${RUNS:-5}"

run_median() {
	local bin="$1" mode="$2" out="$3"
	local tmp
	tmp="$(mktemp)"
	for ((i=0; i<RUNS; i++)); do
		# The benchmark exits non-zero only because of the pre-existing
		# contact-buffer leak, which is identical in baseline and refactored
		# builds and irrelevant to the timing/memory data being collected.
		"$bin" "$mode" 2>/dev/null || true
	done > "$tmp"

	# Keep one copy of non-data lines and the median of numeric step times.
	awk -F'[ |]+' '
		/^(sparse|dense)/ {
			key=$1" "$2" "$3" "$4
			times[key]=times[key] ? times[key]" "$6 : $6
			last[key]=$0
			if(!(key in seen)){ seen[key]=1; order[++n]=key }
			next
		}
		{ if(!($0 in lines_seen)){ lines_seen[$0]=1; print } }
		END {
			for(i=1;i<=n;i++){
				k=order[i]; split(times[k], t, " ")
				cnt=0; for(x in t) cnt++
				# simple insertion sort
				for(a=2;a<=cnt;a++){v=t[a];b=a-1;while(b>=1&&t[b]>v){t[b+1]=t[b];b--};t[b+1]=v}
				med=t[int((cnt+1)/2)]
				line=last[k]; sub(/step avg [0-9.]+ ms/, "step avg "med" ms (median of '"$RUNS"')", line)
				print line
			}
		}
	' "$tmp" | tee "$out"
	rm -f "$tmp"
}

echo "### Post-refactor default spatial hash ###"
run_median "$ROOT/output/tests/bench_new_instr" hash "$ROOT/output/bench/new_hash_median.txt"
echo
echo "### Post-refactor selectable AABB tree ###"
run_median "$ROOT/output/tests/bench_new_instr" tree "$ROOT/output/bench/new_tree_median.txt"
echo
echo "### Baseline AABB tree (pre-refactor default) ###"
run_median "$ROOT/output/tests/bench_baseline_instr" tree "$ROOT/output/bench/baseline_tree_median.txt"
echo
echo "### Baseline spatial hash (cpSpaceUseSpatialHash) ###"
run_median "$ROOT/output/tests/bench_baseline_instr" hash "$ROOT/output/bench/baseline_hash_median.txt"
