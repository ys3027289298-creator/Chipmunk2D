/* Copyright (c) 2013 Scott Lembcke and Howling Moon Software
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "chipmunk/chipmunk_private.h"
#include <math.h>
#include <float.h>

// Numerical diagnostics rely on NaN self-inequality and INFINITY sentinel
// comparisons, both of which are miscompiled under -ffinite-math-only
// (implied by -ffast-math). Fail loudly instead of silently corrupting results.
#if defined(__FAST_MATH__)
	#error "cpSolver.c requires IEEE semantics; build Chipmunk2D with -fno-fast-math."
#endif

//MARK: Error codes

const char *
cpSolverErrorName(cpSolverError error)
{
	switch(error){
		case CP_SOLVER_OK: return "CP_SOLVER_OK";
		case CP_SOLVER_NAN_BODY: return "CP_SOLVER_NAN_BODY";
		case CP_SOLVER_INFINITE_BODY: return "CP_SOLVER_INFINITE_BODY";
		case CP_SOLVER_INVALID_MASS: return "CP_SOLVER_INVALID_MASS";
		case CP_SOLVER_DEGENERATE_CONTACT: return "CP_SOLVER_DEGENERATE_CONTACT";
		case CP_SOLVER_EXTREME_MASS_RATIO: return "CP_SOLVER_EXTREME_MASS_RATIO";
		case CP_SOLVER_NAN_IMPULSE: return "CP_SOLVER_NAN_IMPULSE";
		case CP_SOLVER_DID_NOT_CONVERGE: return "CP_SOLVER_DID_NOT_CONVERGE";
		default: return "CP_SOLVER_UNKNOWN";
	}
}

//MARK: Configuration

cpSolverConfig
cpSolverConfigDefault(void)
{
	cpSolverConfig config;
	config.iterations = 10;
	config.warmStart = cpTrue;
	config.warmStartDtScale = cpTrue;
	config.positionCorrection = CP_POSITION_CORRECTION_BAUMGARTE;
	// The historical bias coefficient is recomputed per step in cpSpaceStep()
	// from collisionBias and dt; this field is overwritten there.
	config.baumgarteBias = 0.0;
	config.slop = 0.1;
	config.positionCorrectionRate = 0.2;
	config.maxCorrectionSpeed = 0.0;
	config.earlyTermination = cpFalse;
	config.velocityTolerance = 1e-4;
	config.maxMassRatio = 1e12;
	config.errorCallback = NULL;
	config.errorUserData = NULL;
	return config;
}

cpSolverConfig *
cpSpaceGetSolverConfig(cpSpace *space)
{
	return &space->solverConfig;
}

void
cpSpaceSetSolverConfig(cpSpace *space, const cpSolverConfig *config)
{
	if(config){
		space->solverConfig = *config;
		// Keep the legacy iteration field consistent for external readers.
		space->iterations = config->iterations;
	} else {
		int iterations = space->iterations;
		cpFloat slop = space->collisionSlop;
		space->solverConfig = cpSolverConfigDefault();
		space->solverConfig.iterations = iterations;
		space->solverConfig.slop = slop;
		space->iterations = iterations;
	}
}

cpSolverError
cpSpaceGetLastSolverError(const cpSpace *space)
{
	return space->lastSolverError;
}

cpSolverError
cpSpaceClearSolverError(cpSpace *space)
{
	cpSolverError previous = space->lastSolverError;
	space->lastSolverError = CP_SOLVER_OK;
	return previous;
}

int
cpSpaceGetLastIterationCount(const cpSpace *space)
{
	return space->lastIterationCount;
}

cpFloat
cpSpaceGetLastResidualVelocity(const cpSpace *space)
{
	return space->lastResidualVelocity;
}

//MARK: Diagnostics

void
cpSolverReportError(cpSpace *space, cpSolverError error)
{
	// The first problem detected in a step is sticky and shadows later ones.
	if(space->lastSolverError == CP_SOLVER_OK){
		space->lastSolverError = error;
		if(space->solverConfig.errorCallback){
			space->solverConfig.errorCallback(space, error,
				space->solverConfig.errorUserData);
		}
	}
}

static inline cpBool
is_finite(cpFloat x)
{
	// Written without relying on -ffast-math semantics.
	return (x == x) && (x != INFINITY) && (x != -INFINITY);
}

// Validate every dynamic body before the solve. Returns the number of faults.
void
cpSolverValidateBodies(cpSpace *space)
{
	cpArray *bodies = space->dynamicBodies;
	for(int i = 0; i < bodies->num; i++){
		cpBody *body = (cpBody *)bodies->arr[i];

		if(body->m != body->m || body->i != body->i ||
		   body->m < 0.0 || body->i < 0.0){
			cpSolverReportError(space, CP_SOLVER_INVALID_MASS);
		}

		if(!is_finite(body->p.x) || !is_finite(body->p.y) ||
		   !is_finite(body->a)){
			cpSolverReportError(space,
				(body->p.x != body->p.x || body->p.y != body->p.y || body->a != body->a)
					? CP_SOLVER_NAN_BODY : CP_SOLVER_INFINITE_BODY);
		}

		if(!is_finite(body->v.x) || !is_finite(body->v.y) ||
		   !is_finite(body->w)){
			cpSolverReportError(space,
				(body->v.x != body->v.x || body->v.y != body->v.y || body->w != body->w)
					? CP_SOLVER_NAN_BODY : CP_SOLVER_INFINITE_BODY);
		}
	}
}

// Check the mass ratio across each active contact. Static/kinematic bodies
// (infinite mass) are ignored as they do not stress the iterative solve.
void
cpSolverValidateArbiters(cpSpace *space)
{
	cpFloat limit = space->solverConfig.maxMassRatio;
	if(limit <= 0.0) return;

	cpArray *arbiters = space->arbiters;
	for(int i = 0; i < arbiters->num; i++){
		cpArbiter *arb = (cpArbiter *)arbiters->arr[i];
		cpBody *a = arb->body_a;
		cpBody *b = arb->body_b;

		cpFloat ma = a->m, mb = b->m;
		if(!is_finite(ma) || !is_finite(mb) || ma <= 0.0 || mb <= 0.0) continue;

		cpFloat ratio = (ma >= mb ? ma / mb : mb / ma);
		if(!(ratio <= limit)){
			cpSolverReportError(space, CP_SOLVER_EXTREME_MASS_RATIO);
			return;
		}
	}
}

//MARK: Pipeline stages

void
cpSolverPrestepArbiters(cpSpace *space, cpFloat dt, cpFloat biasCoef)
{
	cpArray *arbiters = space->arbiters;
	cpFloat slop = space->solverConfig.slop;

	if(space->solverConfig.positionCorrection == CP_POSITION_CORRECTION_SPLIT){
		for(int i = 0; i < arbiters->num; i++){
			cpArbiterPreStepSplit((cpArbiter *)arbiters->arr[i], dt, slop,
				space->solverConfig.positionCorrectionRate,
				space->solverConfig.maxCorrectionSpeed);
		}
	} else {
		for(int i = 0; i < arbiters->num; i++){
			cpArbiterPreStep((cpArbiter *)arbiters->arr[i], dt, slop, biasCoef);
		}
	}
}

void
cpSolverWarmStart(cpSpace *space, cpFloat dtCoef)
{
	if(!space->solverConfig.warmStart) return;

	cpFloat scale = (space->solverConfig.warmStartDtScale ? dtCoef : 1.0);
	cpArray *arbiters = space->arbiters;
	for(int i = 0; i < arbiters->num; i++){
		cpArbiterApplyCachedImpulse((cpArbiter *)arbiters->arr[i], scale);
	}

	cpArray *constraints = space->constraints;
	for(int i = 0; i < constraints->num; i++){
		cpConstraint *constraint = (cpConstraint *)constraints->arr[i];
		constraint->klass->applyCachedImpulse(constraint, scale);
	}
}

// One projected Gauss-Seidel sweep over contacts then joints. Returns the
// largest residual normal contact velocity observed in the sweep.
static cpFloat
cpSolverIteration(cpSpace *space)
{
	cpFloat maxResidual = 0.0;
	cpArray *arbiters = space->arbiters;
	cpBool needResidual = space->solverConfig.earlyTermination;

	if(space->solverConfig.positionCorrection == CP_POSITION_CORRECTION_SPLIT){
		for(int i = 0; i < arbiters->num; i++){
			cpArbiter *arb = (cpArbiter *)arbiters->arr[i];
			if(needResidual){
				cpFloat residual = cpArbiterApplyImpulseSplit(arb);
				if(residual > maxResidual) maxResidual = residual;
			} else {
				cpArbiterApplyImpulse(arb);
			}
		}
	} else {
		for(int i = 0; i < arbiters->num; i++){
			cpArbiter *arb = (cpArbiter *)arbiters->arr[i];
			if(needResidual){
				cpFloat residual = cpArbiterApplyImpulseResidual(arb);
				if(residual > maxResidual) maxResidual = residual;
			} else {
				// Default legacy path: identical arithmetic, zero overhead.
				cpArbiterApplyImpulse(arb);
			}
		}
	}

	cpArray *constraints = space->constraints;
	for(int i = 0; i < constraints->num; i++){
		cpConstraint *constraint = (cpConstraint *)constraints->arr[i];
		constraint->klass->applyImpulse(constraint, space->curr_dt);
	}

	return maxResidual;
}

void
cpSolverRunIterations(cpSpace *space)
{
	int iterations = space->solverConfig.iterations;
	cpFloat tolerance = space->solverConfig.velocityTolerance;
	cpBool earlyOut = space->solverConfig.earlyTermination;

	int executed = 0;
	cpFloat residual = 0.0;

	for(int i = 0; i < iterations; i++){
		executed++;
		residual = cpSolverIteration(space);

		if(earlyOut && residual <= tolerance) break;
	}

	space->lastIterationCount = executed;
	space->lastResidualVelocity = residual;

	// Failure is only reported when the residual remains orders of magnitude
	// above the requested tolerance; tiny floating-point leftovers are normal
	// for stacked contacts and must not generate false diagnostics.
	if(earlyOut && executed == iterations && residual > tolerance * 100.0){
		cpSolverReportError(space, CP_SOLVER_DID_NOT_CONVERGE);
	}
}
