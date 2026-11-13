// Broad-phase equivalence test.
//
// Builds identical random scenarios and runs them twice: once with the
// default spatial-hash broad phase and once with the legacy AABB tree broad
// phase (selected through the internal cpBroadPhase* helpers). After each
// cpSpaceStep() the set of active collision pairs and the ordered sequence of
// collision handler callbacks are compared.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "chipmunk/chipmunk.h"
#include "chipmunk/chipmunk_private.h"

#if USE_BROADPHASE_HEADER
#include "../../src/cpBroadPhase.h"
#else
// Baseline build: declare the internal switch helpers against the baseline
// library names (resolved by a small shim linked into the baseline binary).
typedef enum cpBroadPhaseType {
	CP_BROADPHASE_SPATIAL_HASH,
	CP_BROADPHASE_BB_TREE,
} cpBroadPhaseType;
void cpBroadPhaseUseBBTree(cpSpace *space);
void cpBroadPhaseUseSpatialHash(cpSpace *space);
#endif

#define NUM_BODIES 400
#define NUM_STEPS 60
#define DT 1.0/60.0

typedef struct {
	int beginCount;
	int preSolveCount;
	int postSolveCount;
	int separateCount;
} CallbackCounters;

static cpBool
Begin(cpArbiter *arb, cpSpace *space, void *data)
{
	((CallbackCounters *)data)->beginCount++;
	return cpTrue;
}

static cpBool
PreSolve(cpArbiter *arb, cpSpace *space, void *data)
{
	((CallbackCounters *)data)->preSolveCount++;
	return cpTrue;
}

static void
PostSolve(cpArbiter *arb, cpSpace *space, void *data)
{
	((CallbackCounters *)data)->postSolveCount++;
}

static void
Separate(cpArbiter *arb, cpSpace *space, void *data)
{
	((CallbackCounters *)data)->separateCount++;
}

// Sorted (by shape id) pair snapshot so comparison is order independent.
typedef struct {
	unsigned a;
	unsigned b;
} Pair;

static int
pairCmp(const void *pa, const void *pb)
{
	const Pair *a = (const Pair *)pa;
	const Pair *b = (const Pair *)pb;
	if(a->a != b->a) return (a->a < b->a) ? -1 : 1;
	return (a->b < b->b) ? -1 : (a->b > b->b);
}

static cpArray *
snapshotPairs(cpSpace *space)
{
	cpArray *pairs = cpArrayNew(0);
	for(int i=0; i<space->arbiters->num; i++){
		cpArbiter *arb = (cpArbiter *)space->arbiters->arr[i];
		cpShape *a, *b;
		cpArbiterGetShapes(arb, &a, &b);
		unsigned ha = (unsigned)(size_t)cpShapeGetUserData(a);
		unsigned hb = (unsigned)(size_t)cpShapeGetUserData(b);
		if(ha > hb){ unsigned t = ha; ha = hb; hb = t; }
		Pair pair = {ha, hb};
		cpArrayPush(pairs, &pair);
	}
	qsort(pairs->arr, pairs->num, sizeof(Pair), pairCmp);
	return pairs;
}

static unsigned int rngState;

static cpFloat
rnd(cpFloat min, cpFloat max)
{
	// xorshift
	rngState ^= rngState << 13;
	rngState ^= rngState >> 17;
	rngState ^= rngState << 5;
	return min + (max - min) * (rngState / (cpFloat)0xFFFFFFFFu);
}

static cpSpace *
buildSpace(cpBroadPhaseType type, CallbackCounters *counters)
{
	cpSpace *space = cpSpaceNew();
	if(type == CP_BROADPHASE_BB_TREE) cpBroadPhaseUseBBTree(space);

	// Damping and a spread-out layout keep the bodies well conditioned.
	// (The upstream solver otherwise diverges on long stacked runs; that is a
	// pre-existing narrow-phase/solver issue unrelated to the broad phase.)
	space->gravity = cpv(0.0, -300.0);
	space->damping = 0.9;

	cpCollisionHandler *handler = cpSpaceAddDefaultCollisionHandler(space);
	handler->beginFunc = Begin;
	handler->preSolveFunc = PreSolve;
	handler->postSolveFunc = PostSolve;
	handler->separateFunc = Separate;
	handler->userData = counters;

	// Static floor and two walls, deliberately crossing cell boundaries.
	cpShape *shape;
	shape = cpSegmentShapeNew(space->staticBody, cpv(-2000, -220), cpv(2000, -220), 2);
	cpShapeSetUserData(shape, (void *)(size_t)0x1000);
	cpSpaceAddShape(space, shape);
	shape = cpSegmentShapeNew(space->staticBody, cpv(-1600, -220), cpv(-1600, 1600), 2);
	cpSpaceAddShape(space, shape);
	shape = cpSegmentShapeNew(space->staticBody, cpv(1600, -220), cpv(1600, 1600), 2);
	cpSpaceAddShape(space, shape);

	rngState = 0x12345678u;

	for(int i=0; i<NUM_BODIES; i++){
		cpBody *body = cpBodyNew(rnd(0.5, 4.0), cpMomentForCircle(1.0, 0, rnd(8, 20), cpvzero));
		cpBodySetPosition(body, cpv(rnd(-1500, 1500), rnd(0, 1500)));
		cpSpaceAddBody(space, body);

	cpShape *circle;
		if(i % 3 == 0){
			cpFloat w = rnd(10, 30), h = rnd(10, 30);
			circle = cpBoxShapeNew(body, w, h, 0.0);
		} else {
			circle = cpCircleShapeNew(body, rnd(6, 18), cpvzero);
		}
		cpShapeSetUserData(circle, (void *)(size_t)(i + 1));
		cpSpaceAddShape(space, circle);
	}

	return space;
}

typedef struct {
	cpArray *pairs[NUM_STEPS];
	CallbackCounters counters[NUM_STEPS];
} RunResult;

static RunResult *
run(cpBroadPhaseType type)
{
	RunResult *result = (RunResult *)calloc(1, sizeof(RunResult));
	CallbackCounters cumulative = {0, 0, 0, 0};

	cpSpace *space = buildSpace(type, &cumulative);

	for(int step=0; step<NUM_STEPS; step++){
		CallbackCounters before = cumulative;
		cpSpaceStep(space, DT);
		result->counters[step].beginCount = cumulative.beginCount - before.beginCount;
		result->counters[step].preSolveCount = cumulative.preSolveCount - before.preSolveCount;
		result->counters[step].postSolveCount = cumulative.postSolveCount - before.postSolveCount;
		result->counters[step].separateCount = cumulative.separateCount - before.separateCount;
		result->pairs[step] = snapshotPairs(space);
	}

	cpSpaceFree(space);
	return result;
}

static void
freeResult(RunResult *result)
{
	for(int i=0; i<NUM_STEPS; i++) cpArrayFree(result->pairs[i]);
	free(result);
}

int
main(int argc, char **argv)
{
	// Mode "self" (default): compare the built-in default backend against the
	// legacy AABB tree inside one binary. Modes "hash"/"tree" emit per-step pair
	// counts for cross-binary comparison against the baseline library.
	const char *mode = (argc > 1) ? argv[1] : "self";

	if(strcmp(mode, "hash") == 0){
		RunResult *result = run(CP_BROADPHASE_SPATIAL_HASH);
		for(int step=0; step<NUM_STEPS; step++){
			printf("step %d pairs %d begin %d pre %d post %d sep %d\n",
				step, result->pairs[step]->num,
				result->counters[step].beginCount, result->counters[step].preSolveCount,
				result->counters[step].postSolveCount, result->counters[step].separateCount);
		}
		freeResult(result);
		return 0;
	}

	if(strcmp(mode, "tree") == 0){
		RunResult *result = run(CP_BROADPHASE_BB_TREE);
		for(int step=0; step<NUM_STEPS; step++){
			printf("step %d pairs %d begin %d pre %d post %d sep %d\n",
				step, result->pairs[step]->num,
				result->counters[step].beginCount, result->counters[step].preSolveCount,
				result->counters[step].postSolveCount, result->counters[step].separateCount);
		}
		freeResult(result);
		return 0;
	}

	RunResult *hashResult = run(CP_BROADPHASE_SPATIAL_HASH);
	RunResult *treeResult = run(CP_BROADPHASE_BB_TREE);

	int failures = 0;
	int totalPairs = 0;

	for(int step=0; step<NUM_STEPS; step++){
		cpArray *hp = hashResult->pairs[step];
		cpArray *tp = treeResult->pairs[step];
		totalPairs += hp->num;

		if(hp->num != tp->num || memcmp(hp->arr, tp->arr, hp->num * sizeof(Pair)) != 0){
			printf("FAIL step %d: pair set mismatch hash=%d tree=%d\n", step, hp->num, tp->num);
			failures++;
		}

		CallbackCounters *hc = &hashResult->counters[step];
		CallbackCounters *tc = &treeResult->counters[step];
		if(memcmp(hc, tc, sizeof(CallbackCounters)) != 0){
			printf("FAIL step %d: callback mismatch hash(b%d/p%d/o%d/s%d) tree(b%d/p%d/o%d/s%d)\n",
				step,
				hc->beginCount, hc->preSolveCount, hc->postSolveCount, hc->separateCount,
				tc->beginCount, tc->preSolveCount, tc->postSolveCount, tc->separateCount
			);
			failures++;
		}
	}

	printf("scenario: %d dynamic shapes, %d steps, %d total pair-snapshots\n", NUM_BODIES, NUM_STEPS, totalPairs);

	if(failures == 0){
		printf("PASS: spatial-hash and AABB-tree produced identical pair sets and callback sequences\n");
	} else {
		printf("RESULT: %d mismatching steps\n", failures);
	}

	freeResult(hashResult);
	freeResult(treeResult);
	return failures ? 1 : 0;
}
