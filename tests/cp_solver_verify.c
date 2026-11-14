/* Command line verification entry point for the solver refactor.
 *
 * Runs stack stability, trajectory equivalence and timing scenarios against
 * whatever libchipmunk it is linked against. Emits machine readable CSV-ish
 * lines prefixed with a tag so baseline/refactor runs can be diffed.
 *
 * Sections (also used as video chapter ids):
 *   S1 column stack stability
 *   S2 pyramid stack stability
 *   S3 projectile trajectory (integration equivalence)
 *   S4 stack trajectory equivalence (same-scene determinism)
 *   S5 timing benchmark
 *   S6 numerical diagnostics (NaN / extreme mass ratio)
 */
#include "cp_solver_testbed.h"

#define COLUMN_BOXES 10
#define PYRAMID_ROWS 8
#define STACK_STEPS 1440          // 8 simulated seconds at 180 Hz
#define STACK_WINDOW 360          // metrics over the last 2 seconds
#define TRAJ_STEPS 360
#define BENCH_STEPS 2000
#define BENCH_REPEATS 5

static void
report_metrics(const char *tag, const StackMetrics *m)
{
	printf("METRICS,%s,residualSpeed=%.9f,maxResidualSpeed=%.9f,"
	       "settleDrift=%.9f,maxPenetration=%.9f,jitterEnergy=%.9f\n",
	       tag, m->residualSpeed, m->maxResidualSpeed,
	       m->settleDrift, m->maxPenetration, m->jitterEnergy);
}

static void
section_column(void)
{
	cpBody *bodies[COLUMN_BOXES];
	cpSpace *space = make_column(bodies, COLUMN_BOXES, 30.0, 30);
	StackMetrics m = measure_stack(space, bodies, COLUMN_BOXES,
	                               STACK_STEPS, STACK_WINDOW);
	report_metrics("S1_column", &m);
	cpSpaceFree(space);
}

static void
section_pyramid(void)
{
	int n = 0;
	cpBody *bodies[128];
	cpSpace *space = make_pyramid(bodies, PYRAMID_ROWS, &n, 30);
	StackMetrics m = measure_stack(space, bodies, n,
	                               STACK_STEPS, STACK_WINDOW);
	report_metrics("S2_pyramid", &m);
	cpSpaceFree(space);
}

static void
section_projectile(void)
{
	cpBody *body;
	cpSpace *space = make_projectile(&body);
	cpVect *traj = (cpVect *)cpcalloc(TRAJ_STEPS, sizeof(cpVect));
	record_trajectory(space, body, TRAJ_STEPS, traj);

	/* Analytic semi-implicit Euler reference (Chipmunk integrates gravity
	 * with semi-implicit Euler: v += g*dt; x += v*dt). */
	double rmse = 0.0, maxErr = 0.0;
	cpVect p0 = cpv(-200, 100), v0 = cpv(120, 240);
	cpFloat dt = TESTBED_DT;
	cpVect v = v0;
	for(int i = 0; i < TRAJ_STEPS; i++){
		v.y += -TESTBED_GRAVITY * dt;
		p0.x += v.x * dt;
		p0.y += v.y * dt;
		double d = cpvdist(traj[i], p0);
		rmse += d*d;
		if(d > maxErr) maxErr = d;
	}
	rmse = sqrt(rmse / TRAJ_STEPS);
	printf("METRICS,S3_projectile,trajRmseVsAnalytic=%.9f,maxErr=%.9f\n",
	       rmse, maxErr);
	cpfree(traj);
	cpSpaceFree(space);
}

static void
section_stack_trajectory(void)
{
	/* Two independently constructed identical scenes must produce identical
	 * trajectories (determinism / bounded-error reference). */
	cpBody *b1[COLUMN_BOXES], *b2[COLUMN_BOXES];
	cpSpace *s1 = make_column(b1, COLUMN_BOXES, 30.0, 30);
	cpSpace *s2 = make_column(b2, COLUMN_BOXES, 30.0, 30);

	cpVect *t1 = (cpVect *)cpcalloc(TRAJ_STEPS, sizeof(cpVect));
	cpVect *t2 = (cpVect *)cpcalloc(TRAJ_STEPS, sizeof(cpVect));
	record_trajectory(s1, b1[COLUMN_BOXES-1], TRAJ_STEPS, t1);
	record_trajectory(s2, b2[COLUMN_BOXES-1], TRAJ_STEPS, t2);

	double rmse = trajectory_rmse(t1, t2, TRAJ_STEPS);
	double maxErr = 0.0;
	for(int i = 0; i < TRAJ_STEPS; i++){
		double d = cpvdist(t1[i], t2[i]);
		if(d > maxErr) maxErr = d;
	}
	printf("METRICS,S4_stack_traj,repeatRmse=%.12f,repeatMaxErr=%.12f\n",
	       rmse, maxErr);
	cpfree(t1); cpfree(t2);
	cpSpaceFree(s1); cpSpaceFree(s2);
}

static void
section_timing(void)
{
	cpBody *bodies[COLUMN_BOXES];
	cpSpace *space = make_column(bodies, COLUMN_BOXES, 30.0, 30);
	double elapsed = benchmark_steps(space, BENCH_STEPS, BENCH_REPEATS);
	printf("METRICS,S5_timing,bestSeconds=%.9f,steps=%d,usPerStep=%.6f\n",
	       elapsed, BENCH_STEPS, elapsed / BENCH_STEPS * 1e6);
	cpSpaceFree(space);
}

#ifdef CP_HAS_SOLVER_CONFIG
static void
section_diagnostics(void)
{
	/* NaN mass body. */
	{
		cpSpace *space = cpSpaceNew();
		cpBody *body = cpSpaceAddBody(space,
			cpBodyNew(NAN, cpMomentForBox(1.0, 10.0, 10.0)));
		cpBodySetPosition(body, cpv(0, 5));
		cpSpaceAddShape(space, cpBoxShapeNew(body, 10, 10, 0));
		cpSpaceStep(space, TESTBED_DT);
		cpSolverError err = cpSpaceGetLastSolverError(space);
		printf("METRICS,S6_nan_mass,errorCode=%d,errorName=%s\n",
		       (int)err, cpSolverErrorName(err));
		cpSpaceFree(space);
	}

	/* Extreme mass ratio across a contact. */
	{
		cpSpace *space = cpSpaceNew();
		cpSpaceSetGravity(space, cpv(0, -TESTBED_GRAVITY));
		cpBody *groundBody = cpSpaceGetStaticBody(space);
		cpSpaceAddShape(space,
			cpSegmentShapeNew(groundBody, cpv(-100,0), cpv(100,0), 0));

		cpBody *heavy = cpSpaceAddBody(space,
			cpBodyNew(1.0e9, cpMomentForBox(1.0e9, 20, 20)));
		cpBodySetPosition(heavy, cpv(0, 40));
		cpSpaceAddShape(space, cpBoxShapeNew(heavy, 20, 20, 0));

		cpBody *light = cpSpaceAddBody(space,
			cpBodyNew(1.0e-9, cpMomentForBox(1.0e-9, 20, 20)));
		cpBodySetPosition(light, cpv(0, 15));
		cpSpaceAddShape(space, cpBoxShapeNew(light, 20, 20, 0));

	for(int i = 0; i < 120; i++) cpSpaceStep(space, TESTBED_DT);
		cpSolverError err = cpSpaceGetLastSolverError(space);
		printf("METRICS,S6_extreme_mass,errorCode=%d,errorName=%s\n",
		       (int)err, cpSolverErrorName(err));
		cpSpaceFree(space);
	}

	/* Healthy scene reports CP_SOLVER_OK. */
	{
		cpBody *bodies[4];
		cpSpace *space = make_column(bodies, 4, 20.0, 10);
		cpSpaceStep(space, TESTBED_DT);
		cpSolverError err = cpSpaceGetLastSolverError(space);
		printf("METRICS,S6_healthy,errorCode=%d,errorName=%s\n",
		       (int)err, cpSolverErrorName(err));
		cpSpaceFree(space);
	}
}
#else
static void
section_diagnostics(void)
{
	printf("METRICS,S6_nan_mass,errorCode=-1,errorName=UNSUPPORTED_BY_BASELINE\n");
	printf("METRICS,S6_extreme_mass,errorCode=-1,errorName=UNSUPPORTED_BY_BASELINE\n");
	printf("METRICS,S6_healthy,errorCode=-1,errorName=UNSUPPORTED_BY_BASELINE\n");
}
#endif

int
main(void)
{
	printf("=== cp_solver_verify start ===\n");
	section_column();
	section_pyramid();
	section_projectile();
	section_stack_trajectory();
	section_timing();
#ifdef CP_SKIP_DIAGNOSTICS
	printf("METRICS,S6_nan_mass,errorCode=-1,errorName=BASELINE_ABORTS_NO_DIAGNOSTICS\n");
	printf("METRICS,S6_extreme_mass,errorCode=-1,errorName=BASELINE_ABORTS_NO_DIAGNOSTICS\n");
	printf("METRICS,S6_healthy,errorCode=-1,errorName=BASELINE_ABORTS_NO_DIAGNOSTICS\n");
#else
	section_diagnostics();
#endif
	printf("=== cp_solver_verify done ===\n");
	return 0;
}
