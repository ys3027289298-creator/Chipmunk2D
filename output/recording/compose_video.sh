#!/usr/bin/env bash
# Compose the recorded terminal segments into one MP4 with an intro card and
# a titled card before every segment.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SEG="$ROOT/output/recording/segments"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

W=1280
H=720

make_card() {
	local out="$1"; local line1="$2"; local line2="$3"
	local png="$WORK/card.png"
	python3 "$ROOT/output/recording/make_card.py" "$png" "$line1" "$line2"
	ffmpeg -y -loglevel error -loop 1 -i "$png" -t 2 -r 20 -pix_fmt yuv420p "$out"
}

gif_to_mp4() {
	local gif="$1"; local out="$2"
	# Pad the terminal capture to the common canvas on a dark background.
	ffmpeg -y -loglevel error -i "$gif" \
		-vf "fps=20,scale=w='min(${W},iw)':h='min(${H},ih)':force_original_aspect_ratio=decrease:flags=lanczos,pad=${W}:${H}:(ow-iw)/2:(oh-ih)/2:color=0x10131a,format=yuv420p" \
		"$out"
}

make_card "$WORK/intro.mp4" "Chipmunk2D Broad-Phase Refactor" "pluggable spatial index - default spatial hash"

declare -a TITLES=(
	"Build the refactored library"
	"Collision pair equivalence: hash vs AABB tree"
	"Cross-binary trace vs pristine baseline"
	"Degenerate shape rejection"
	"Performance and linear memory bound"
)

LIST="$WORK/list.txt"
echo "file '$WORK/intro.mp4'" > "$LIST"

for i in 1 2 3 4 5; do
	idx=$(printf "%02d" "$i")
	gif="$(find "$SEG" -maxdepth 1 -name "${idx}_*.gif" | head -1)"
	gif_to_mp4 "$gif" "$WORK/seg${idx}.mp4"
	make_card "$WORK/card${idx}.mp4" "Segment ${i} of 5" "${TITLES[$((i-1))]}"
	echo "file '$WORK/card${idx}.mp4'" >> "$LIST"
	echo "file '$WORK/seg${idx}.mp4'" >> "$LIST"
done

ffmpeg -y -loglevel error -f concat -safe 0 -i "$LIST" -c copy "$ROOT/output/recording/broadphase_refactor.mp4"
echo "wrote $ROOT/output/recording/broadphase_refactor.mp4"
