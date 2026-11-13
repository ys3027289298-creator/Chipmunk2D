#!/usr/bin/env bash
# Build the memory-instrumented benchmark against a given chipmunk source tree.
# All library sources are compiled directly with the allocator macros
# redirected to the tracking allocator in cp_alloc_override.c.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC_DIR="${1:?usage: build_bench.sh <chipmunk-src-dir> <output-binary> [shim.c]}"
OUT="${2:?usage: build_bench.sh <chipmunk-src-dir> <output-binary> [shim.c]}"
SHIM="${3:-}"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

SRCS=()
for c in "$SRC_DIR"/src/*.c; do
	base="$(basename "$c")"
	gcc -std=gnu99 -O2 -ffast-math \
		-include "$ROOT/output/tests/cp_alloc_override.h" \
		-Dcpcalloc=cp_calloc -Dcpfree=cp_free -Dcprealloc=cp_realloc \
		-I"$SRC_DIR/include" -I"$SRC_DIR/src" \
		-c "$c" -o "$TMP/$base.o"
	SRCS+=("$TMP/$base.o")
done

gcc -std=gnu99 -O2 \
	-I"$SRC_DIR/include" -I"$SRC_DIR/src" \
	-c "$ROOT/output/tests/cp_alloc_override.c" -o "$TMP/override.o"

HEAD_FLAG=1
EXTRA=()
if [[ -n "$SHIM" ]]; then
	HEAD_FLAG=0
	gcc -std=gnu99 -O2 -I"$SRC_DIR/include" -I"$SRC_DIR/src" \
		-c "$SHIM" -o "$TMP/shim.o"
	EXTRA+=("$TMP/shim.o")
fi

gcc -std=gnu99 -O2 -DUSE_BROADPHASE_HEADER=$HEAD_FLAG \
	-I"$SRC_DIR/include" -I"$SRC_DIR/src" \
	"$ROOT/output/tests/bench_broadphase.c" \
	"$TMP/override.o" "${EXTRA[@]}" "${SRCS[@]}" -lm -o "$OUT"

echo "built $OUT"
