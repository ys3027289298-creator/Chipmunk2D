#!/usr/bin/env bash
# Reproducible acceptance harness for the solver refactor.
# Builds the pristine baseline (git HEAD, -fno-fast-math) and the refactored
# library, then runs the full verification suite and writes every artifact
# under output/.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/output"
BUILD="$OUT/build"
mkdir -p "$OUT/data" "$OUT/video" "$BUILD"

echo "== [1/5] Building pristine baseline library from git HEAD =="
PRISTINE="$BUILD/pristine"
mkdir -p "$PRISTINE"
git -C "$ROOT" archive HEAD | tar -x -C "$PRISTINE"
mkdir -p "$BUILD/pristine_obj"
find "$BUILD/pristine_obj" -name '*.o' -delete
for f in "$PRISTINE"/src/*.c; do
	b="$(basename "$f" .c)"
	gcc -std=gnu99 -DNDEBUG -O2 -fno-fast-math \
		-I"$PRISTINE/include" -c "$f" -o "$BUILD/pristine_obj/$b.o"
done
ar rcs "$BUILD/lib_pristine.a" "$BUILD"/pristine_obj/*.o

echo "== [2/5] Building refactored library + CTest suite =="
cmake -S "$ROOT" -B "$ROOT/build-accept" \
	-DCMAKE_BUILD_TYPE=Release -DBUILD_DEMOS=OFF -DBUILD_SHARED=OFF
cmake --build "$ROOT/build-accept" -j"$(nproc)"

echo "== [3/5] Running CTest =="
ctest --test-dir "$ROOT/build-accept" --output-on-failure

echo "== [4/5] Collecting baseline vs refactor metrics =="
gcc -std=gnu99 -DNDEBUG -O2 -DCP_SKIP_DIAGNOSTICS \
	-I"$PRISTINE/include" -I"$ROOT/tests" \
	"$ROOT/tests/cp_solver_verify.c" "$BUILD/lib_pristine.a" -lm \
	-o "$BUILD/verify_baseline"
"$BUILD/verify_baseline" | tee "$OUT/data/baseline_metrics.txt"

gcc -std=gnu99 -DNDEBUG -O2 -DCP_HAS_SOLVER_CONFIG \
	-I"$ROOT/include" -I"$ROOT/tests" \
	"$ROOT/tests/cp_solver_verify.c" "$ROOT/build-accept/src/libchipmunk.a" -lm \
	-o "$BUILD/verify_refactor"
"$BUILD/verify_refactor" 2>/dev/null | tee "$OUT/data/refactor_default_metrics.txt"

"$ROOT/build-accept/tests/cp_solver_modes" 2>/dev/null \
	| tee "$OUT/data/refactor_modes.txt"
"$ROOT/build-accept/tests/cp_solver_diagnostics" \
	| tee "$OUT/data/diagnostics.txt"

echo "== [5/5] Rendering titled screen recording =="
"$ROOT/build-accept/tests/cp_solver_record" > "$BUILD/frames.ppm"
ffmpeg -hide_banner -loglevel error -y \
	-f image2pipe -framerate 60 -i "$BUILD/frames.ppm" \
	-c:v libx264 -pix_fmt yuv420p -preset medium -crf 23 \
	"$OUT/video/solver_refactor_recording.mp4"
gzip -c "$BUILD/frames.ppm" > "$BUILD/frames.ppm.gz"
find "$BUILD" -name frames.ppm -delete

echo "Acceptance run complete. Artifacts in $OUT"
