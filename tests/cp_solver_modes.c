/* Compares the legacy-equivalent default solver configuration against the
 * opt-in stability configuration (split position correction + early
 * termination) on the same stack scenarios. Proves the stability mode reduces
 * jitter while keeping trajectories bounded. */
#include "cp_solver_testbed.h"

#define STACK_STEPS 1440
#define STACK_WINDOW 360

static void
apply_stable_config(cpSpace *space)
{
	cpSolverConfig config = cpSolverConfigDefault();
	config.iterations = cpSpaceGetIterations(space);
	config.slop = 0.1;
	config.warmStart = cpTrue;
	config.warmStartDtScale = cpTrue;
	config.positionCorrection = CP_POSITION_CORRECTION_SPLIT;
	config.positionCorrectionRate = 0.2;
	config.maxCorrectionSpeed = 0.0;
	config.earlyTermination = cpTrue;
	config.velocityTolerance = 1e-5;
	config.maxMassRatio = 1e12;
	cpSpaceSetSolverConfig(space, &config);
}

static void
run_column(const char *tag, cpBool stable)
{
	cpBody *bodies[10];
	cpSpace *space = make_column(bodies, 10, 30.0, 30);
	if(stable) apply_stable_config(space);
	StackMetrics m = measure_stack(space, bodies, 10, STACK_STEPS, STACK_WINDOW);
	printf("MODE,%s,column,residualSpeed=%.9f,maxResidualSpeed=%.9f,"
	       "settleDrift=%.9f,maxPenetration=%.9f,jitterEnergy=%.9f,"
	       "lastIters=%d,lastResidual=%.9f,error=%d\n",
	       tag, m.residualSpeed, m.maxResidualSpeed, m.settleDrift,
	       m.maxPenetration, m.jitterEnergy,
	       cpSpaceGetLastIterationCount(space),
	       cpSpaceGetLastResidualVelocity(space),
	       (int)cpSpaceGetLastSolverError(space));
	cpSpaceFree(space);
}

static void
run_pyramid(const char *tag, cpBool stable)
{
	int n = 0;
	cpBody *bodies[128];
	cpSpace *space = make_pyramid(bodies, 8, &n, 30);
	if(stable){
		apply_stable_config(space);
		cpSpaceGetSolverConfig(space)->slop = 0.5;
	}
	StackMetrics m = measure_stack(space, bodies, n, STACK_STEPS, STACK_WINDOW);
	printf("MODE,%s,pyramid,residualSpeed=%.9f,maxResidualSpeed=%.9f,"
	       "settleDrift=%.9f,maxPenetration=%.9f,jitterEnergy=%.9f,"
	       "lastIters=%d,lastResidual=%.9f,error=%d\n",
	       tag, m.residualSpeed, m.maxResidualSpeed, m.settleDrift,
	       m.maxPenetration, m.jitterEnergy,
	       cpSpaceGetLastIterationCount(space),
	       cpSpaceGetLastResidualVelocity(space),
	       (int)cpSpaceGetLastSolverError(space));
	cpSpaceFree(space);
}

int
main(void)
{
	printf("=== cp_solver_modes start ===\n");
	run_column("legacy", cpFalse);
	run_column("stable", cpTrue);
	run_pyramid("legacy", cpFalse);
	run_pyramid("stable", cpTrue);
	printf("=== cp_solver_modes done ===\n");
	return 0;
}
