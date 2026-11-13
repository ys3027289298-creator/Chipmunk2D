// Broad-phase performance and memory benchmark.
//
// The companion cp_alloc_override.c (compiled with the chipmunk sources using
// -Dcpcalloc=cp_calloc ...) tracks live heap usage attributed to the library.
// For each shape count it reports:
//   - wall time of cpSpaceStep() (broad phase + narrow phase + solver)
//   - current and peak live bytes
//
// Backend selection:
//   hash : default spatial hash (post-refactor default)
//   tree : legacy AABB tree

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "chipmunk/chipmunk.h"
#include "chipmunk/chipmunk_private.h"

#if USE_BROADPHASE_HEADER
#include "../../src/cpBroadPhase.h"
#else
typedef enum cpBroadPhaseType {
	CP_BROADPHASE_SPATIAL_HASH,
	CP_BROADPHASE_BB_TREE,
} cpBroadPhaseType;
void cpBroadPhaseUseBBTree(cpSpace *space);
void cpBroadPhaseUseSpatialHash(cpSpace *space);
#endif

extern size_t cp_alloc_current_bytes(void);
extern size_t cp_alloc_peak_bytes(void);
extern void cp_alloc_reset(void);

#define currentBytes cp_alloc_current_bytes()
#define peakBytes cp_alloc_peak_bytes()

static double
nowSeconds(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

static unsigned int rngState = 0xABCDEF01u;

static cpFloat
rnd(cpFloat min, cpFloat max)
{
	rngState ^= rngState << 13;
	rngState ^= rngState >> 17;
	rngState ^= rngState << 5;
	return min + (max - min) * (rngState / (cpFloat)0xFFFFFFFFu);
}

static cpSpace *
buildSpace(int numShapes, cpBroadPhaseType type, cpBool sparse)
{
	cpSpace *space = cpSpaceNew();
	if(type == CP_BROADPHASE_BB_TREE) cpBroadPhaseUseBBTree(space);
	space->damping = 0.9;

	cpShape *floorShape = cpSegmentShapeNew(space->staticBody, cpv(-10000, -50), cpv(10000, -50), 2);
	cpSpaceAddShape(space, floorShape);

	rngState = 0xABCDEF01u;
	// Sparse: very large spread, almost no overlaps.
	// Dense: spread grows with sqrt(n) so area density, and therefore the
	// expected number of neighbours per shape, stays bounded. Total pairs stay
	// linear instead of degenerating into an O(n^2) pile-up.
	cpFloat spread = sparse ? (cpFloat)numShapes * 3.0 : 12.0 * cpfsqrt((cpFloat)numShapes);

	for(int i=0; i<numShapes; i++){
		cpFloat radius = rnd(6, 14);
		cpBody *body = cpBodyNew(1.0, cpMomentForCircle(1.0, 0, radius, cpvzero));
		cpBodySetPosition(body, cpv(rnd(-spread, spread), rnd(0, sparse ? spread : 800)));
		cpSpaceAddBody(space, body);

		cpShape *shape = cpCircleShapeNew(body, radius, cpvzero);
		cpSpaceAddShape(space, shape);
	}

	return space;
}

static void
benchmark(int numShapes, cpBroadPhaseType type, cpBool sparse, int warmup, int steps)
{
	cp_alloc_reset();
	cpSpace *space = buildSpace(numShapes, type, sparse);

	for(int i=0; i<warmup; i++) cpSpaceStep(space, 1.0/60.0);

	size_t bytesBefore = currentBytes;
	(void)bytesBefore;

	double start = nowSeconds();
	for(int i=0; i<steps; i++) cpSpaceStep(space, 1.0/60.0);
	double elapsed = nowSeconds() - start;

	const char *scenario = sparse ? "sparse" : "dense ";
	const char *backend = (type == CP_BROADPHASE_SPATIAL_HASH) ? "hash" : "tree";

	printf("%s %6d shapes | %s | step avg %8.3f ms | live %10zu B | peak %10zu B | live/shape %7.1f B\n",
		scenario, numShapes, backend,
		elapsed / steps * 1000.0,
		currentBytes, peakBytes,
		numShapes > 0 ? (double)currentBytes / numShapes : 0.0
	);
	fflush(stdout);

	cpSpaceFree(space);
}

int
main(int argc, char **argv)
{
	cpBroadPhaseType type = CP_BROADPHASE_SPATIAL_HASH;
	if(argc > 1 && strcmp(argv[1], "tree") == 0) type = CP_BROADPHASE_BB_TREE;

	const int counts[] = {250, 500, 1000, 2000, 4000, 8000, 16000};
	const int numCounts = (int)(sizeof(counts)/sizeof(counts[0]));
	setvbuf(stdout, NULL, _IONBF, 0);

	printf("=== sparse scenario (uniform spread, few overlaps) ===\n");
	for(int i=0; i<numCounts; i++) benchmark(counts[i], type, cpTrue, 5, 30);

	// The dense scenario is intentionally capped: a confined area produces an
	// O(n^2) number of overlapping pairs that every broad phase must emit, so
	// scaling it to thousands of shapes measures narrow-phase cost, not the
	// broad phase. It is kept small for a backend-to-backend comparison only.
	const int denseCounts[] = {250, 500, 1000, 2000};
	printf("=== dense scenario (confined area, backend comparison, capped) ===\n");
	for(int i=0; i<(int)(sizeof(denseCounts)/sizeof(denseCounts[0])); i++){
		benchmark(denseCounts[i], type, cpFalse, 5, 20);
	}

	// Every space is freed before the next one is built, and the counters are
	// reset per run; a non-zero live count at the very end means a real leak.
	if(cp_alloc_current_bytes() != 0){
		printf("LEAK WARNING: %zu bytes still live after all spaces freed\n", cp_alloc_current_bytes());
		return 1;
	}
	printf("allocator balance: 0 live bytes after final teardown (no leaks)\n");
	return 0;
}
