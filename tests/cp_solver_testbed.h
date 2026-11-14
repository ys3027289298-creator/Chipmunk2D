/* Solver refactor verification testbed.
 *
 * Shared scenario builders, metric collection and a tiny software rasterizer
 * used by the command line verification entry points. Only the public
 * Chipmunk2D API plus the new solver configuration / diagnostics API are used
 * so that the harness validates externally observable behavior.
 */
#ifndef CP_SOLVER_TESTBED_H
#define CP_SOLVER_TESTBED_H

#include "chipmunk/chipmunk_private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define TESTBED_DT (1.0/180.0)
#define TESTBED_GRAVITY 100.0

/* ------------------------------------------------------------------ */
/* Scenario builders                                                  */
/* ------------------------------------------------------------------ */

/* Vertical column of equal boxes resting on the ground. The hardest
 * configuration for iterative solvers because error propagates through the
 * whole contact chain. Returns the dynamic bodies in out_bodies. */
static inline cpSpace *
make_column(cpBody **out_bodies, int count, cpFloat boxSize, int iterations)
{
	cpSpace *space = cpSpaceNew();
	cpSpaceSetIterations(space, iterations);
	cpSpaceSetGravity(space, cpv(0, -TESTBED_GRAVITY));
	cpSpaceSetSleepTimeThreshold(space, INFINITY); // keep awake: expose jitter
	cpSpaceSetCollisionSlop(space, 0.1);

	cpBody *staticBody = cpSpaceGetStaticBody(space);
	cpShape *ground = cpSpaceAddShape(space,
		cpSegmentShapeNew(staticBody, cpv(-1000, 0), cpv(1000, 0), 0.0));
	cpShapeSetFriction(ground, 1.0);

	for(int i = 0; i < count; i++){
		cpFloat mass = 1.0;
		cpBody *body = cpSpaceAddBody(space,
			cpBodyNew(mass, cpMomentForBox(mass, boxSize, boxSize)));
		cpBodySetPosition(body, cpv(0, boxSize*0.5 + i*boxSize));
		cpShape *shape = cpSpaceAddShape(space,
			cpBoxShapeNew(body, boxSize, boxSize, 0.0));
		cpShapeSetFriction(shape, 0.8);
		cpShapeSetElasticity(shape, 0.0);
		if(out_bodies) out_bodies[i] = body;
	}
	return space;
}

/* Pyramid stack mirroring demo/PyramidStack.c (without the loose ball). */
static inline cpSpace *
make_pyramid(cpBody **out_bodies, int rows, int *out_count, int iterations)
{
	cpSpace *space = cpSpaceNew();
	cpSpaceSetIterations(space, iterations);
	cpSpaceSetGravity(space, cpv(0, -TESTBED_GRAVITY));
	cpSpaceSetSleepTimeThreshold(space, INFINITY);
	cpSpaceSetCollisionSlop(space, 0.5);

	cpBody *staticBody = cpSpaceGetStaticBody(space);
	cpShape *g = cpSpaceAddShape(space,
		cpSegmentShapeNew(staticBody, cpv(-320,-240), cpv(320,-240), 0.0));
	cpShapeSetFriction(g, 1.0);

	int k = 0;
	for(int i = 0; i < rows; i++){
		for(int j = 0; j <= i; j++){
			cpBody *body = cpSpaceAddBody(space,
				cpBodyNew(1.0, cpMomentForBox(1.0, 30.0, 30.0)));
			cpBodySetPosition(body, cpv(j*32 - i*16, 300 - i*32));
			cpShape *shape = cpSpaceAddShape(space,
				cpBoxShapeNew(body, 30.0, 30.0, 0.5));
			cpShapeSetElasticity(shape, 0.0);
			cpShapeSetFriction(shape, 0.8);
			if(out_bodies) out_bodies[k] = body;
			k++;
		}
	}
	if(out_count) *out_count = k;
	return space;
}

/* Free-flight projectile under gravity: no contacts, pure integration path. */
static inline cpSpace *
make_projectile(cpBody **out_body)
{
	cpSpace *space = cpSpaceNew();
	cpSpaceSetGravity(space, cpv(0, -TESTBED_GRAVITY));
	cpBody *body = cpSpaceAddBody(space,
		cpBodyNew(1.0, cpMomentForCircle(1.0, 0, 10, cpvzero)));
	cpBodySetPosition(body, cpv(-200, 100));
	cpBodySetVelocity(body, cpv(120, 240));
	if(out_body) *out_body = body;
	return space;
}

/* ------------------------------------------------------------------ */
/* Metrics                                                            */
/* ------------------------------------------------------------------ */

typedef struct StackMetrics {
	double residualSpeed;   // mean |v| over bodies, averaged over the window
	double maxResidualSpeed;
	double settleDrift;     // mean horizontal drift |x - x0| at the end
	double maxPenetration;  // max contact penetration (negative arbiter depth)
	double jitterEnergy;    // mean per-step position change once "settled"
} StackMetrics;

/* Collect metrics for the last window_fraction of a run. The space is stepped
 * in place; initial body positions are taken from the current state. */
static inline StackMetrics
measure_stack(cpSpace *space, cpBody **bodies, int n, int totalSteps,
              int windowSteps)
{
	cpVect *x0 = (cpVect *)cpcalloc(n, sizeof(cpVect));
	cpVect *prev = (cpVect *)cpcalloc(n, sizeof(cpVect));
	for(int i = 0; i < n; i++){
		x0[i] = bodies[i]->p;
		prev[i] = bodies[i]->p;
	}

	StackMetrics m;
	memset(&m, 0, sizeof(m));
	int windowStart = totalSteps - windowSteps;
	double speedSum = 0.0, jitterSum = 0.0;
	int samples = 0;

	for(int step = 0; step < totalSteps; step++){
		cpSpaceStep(space, TESTBED_DT);

		if(step >= windowStart){
			double stepSpeed = 0.0, stepJitter = 0.0;
			for(int i = 0; i < n; i++){
				double sp = cpvlength(bodies[i]->v);
				stepSpeed += sp;
				if(sp > m.maxResidualSpeed) m.maxResidualSpeed = sp;
				stepJitter += cpvdist(bodies[i]->p, prev[i]);
				prev[i] = bodies[i]->p;
			}
			speedSum += stepSpeed / n;
			jitterSum += stepJitter / n;
			samples++;
		}
	}

	double drift = 0.0;
	for(int i = 0; i < n; i++){
		drift += fabs(bodies[i]->p.x - x0[i].x);
	}

	m.residualSpeed = speedSum / samples;
	m.jitterEnergy = jitterSum / samples;
	m.settleDrift = drift / n;

	/* Penetration from active arbiters. */
	cpArray *arbiters = space->arbiters;
	double maxPen = 0.0;
	for(int i = 0; i < arbiters->num; i++){
		cpArbiter *arb = (cpArbiter *)arbiters->arr[i];
		int count = cpArbiterGetCount(arb);
		for(int k = 0; k < count; k++){
			cpFloat depth = cpArbiterGetDepth(arb, k);
			if(depth < 0.0 && -depth > maxPen) maxPen = -depth;
		}
	}
	m.maxPenetration = maxPen;

	cpfree(x0);
	cpfree(prev);
	return m;
}

/* Record the full trajectory of one body as (x, y) samples. */
static inline void
record_trajectory(cpSpace *space, cpBody *body, int steps, cpVect *out)
{
	for(int i = 0; i < steps; i++){
		out[i] = body->p;
		cpSpaceStep(space, TESTBED_DT);
	}
}

static inline double
trajectory_rmse(const cpVect *a, const cpVect *b, int n)
{
	double acc = 0.0;
	double maxErr = 0.0;
	for(int i = 0; i < n; i++){
		double d = cpvdist(a[i], b[i]);
		acc += d*d;
	if(d > maxErr) maxErr = d;
	}
	(void)maxErr;
	return sqrt(acc / n);
}

/* ------------------------------------------------------------------ */
/* Timing                                                             */
/* ------------------------------------------------------------------ */

static inline double
now_seconds(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static inline double
benchmark_steps(cpSpace *space, int steps, int repeats)
{
	double best = 1e300;
	for(int r = 0; r < repeats; r++){
		double t0 = now_seconds();
		for(int i = 0; i < steps; i++) cpSpaceStep(space, TESTBED_DT);
		double elapsed = now_seconds() - t0;
		if(elapsed < best) best = elapsed;
	}
	return best;
}

#endif
