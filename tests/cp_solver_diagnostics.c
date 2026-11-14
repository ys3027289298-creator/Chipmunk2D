/* Focused acceptance tests for numerical diagnostics: NaN bodies, extreme
 * mass ratios, the error callback and the sticky error lifecycle. Every
 * failing condition exits non-zero so CTest reports it directly. */
#include "cp_solver_testbed.h"

static int failures = 0;

#define CHECK(cond, message)                                       \
	do {                                                           \
		if(!(cond)){                                              \
			printf("FAIL: %s\n", message);                       \
			failures++;                                          \
		} else {                                                   \
			printf("PASS: %s\n", message);                       \
		}                                                          \
	} while(0)

static cpSolverError callbackSeen = CP_SOLVER_OK;
static void
on_error(cpSpace *space, cpSolverError error, void *userData)
{
	(void)space; (void)userData;
	callbackSeen = error;
}

static void
test_nan_mass(void)
{
	cpSpace *space = cpSpaceNew();
	cpBody *body = cpSpaceAddBody(space,
		cpBodyNew(NAN, cpMomentForBox(1.0, 10.0, 10.0)));
	cpBodySetPosition(body, cpv(0, 5));
	cpSpaceAddShape(space, cpBoxShapeNew(body, 10, 10, 0));

	cpSpaceStep(space, TESTBED_DT);
	CHECK(cpSpaceGetLastSolverError(space) == CP_SOLVER_INVALID_MASS,
	      "NaN mass reported as CP_SOLVER_INVALID_MASS");

	cpSolverError previous = cpSpaceClearSolverError(space);
	CHECK(previous == CP_SOLVER_INVALID_MASS, "clear returns previous error");
	CHECK(cpSpaceGetLastSolverError(space) == CP_SOLVER_OK,
	      "error resets to CP_SOLVER_OK after clear");
	cpSpaceFree(space);
}

static void
test_extreme_mass_ratio(void)
{
	cpSpace *space = cpSpaceNew();
	cpSpaceSetGravity(space, cpv(0, -TESTBED_GRAVITY));
	cpBody *ground = cpSpaceGetStaticBody(space);
	cpSpaceAddShape(space,
		cpSegmentShapeNew(ground, cpv(-100,0), cpv(100,0), 0));

	cpBody *heavy = cpSpaceAddBody(space,
		cpBodyNew(1.0e9, cpMomentForBox(1.0e9, 20, 20)));
	cpBodySetPosition(heavy, cpv(0, 40));
	cpSpaceAddShape(space, cpBoxShapeNew(heavy, 20, 20, 0));

	cpBody *light = cpSpaceAddBody(space,
		cpBodyNew(1.0e-9, cpMomentForBox(1.0e-9, 20, 20)));
	cpBodySetPosition(light, cpv(0, 15));
	cpSpaceAddShape(space, cpBoxShapeNew(light, 20, 20, 0));

	cpSolverError saw = CP_SOLVER_OK;
	for(int i = 0; i < 120 && saw == CP_SOLVER_OK; i++){
		cpSpaceStep(space, TESTBED_DT);
		saw = cpSpaceGetLastSolverError(space);
	}
	CHECK(saw == CP_SOLVER_EXTREME_MASS_RATIO,
	      "1e18 mass ratio reported as CP_SOLVER_EXTREME_MASS_RATIO");
	CHECK(strcmp(cpSolverErrorName(saw), "CP_SOLVER_EXTREME_MASS_RATIO") == 0,
	      "error name round trips through cpSolverErrorName");
	cpSpaceFree(space);
}

static void
test_callback_and_healthy(void)
{
	cpBody *bodies[4];
	cpSpace *space = make_column(bodies, 4, 20.0, 10);
	cpSolverConfig *config = cpSpaceGetSolverConfig(space);
	config->errorCallback = on_error;
	config->errorUserData = NULL;

	cpSpaceStep(space, TESTBED_DT);
	CHECK(cpSpaceGetLastSolverError(space) == CP_SOLVER_OK,
	      "healthy scene reports CP_SOLVER_OK");
	CHECK(callbackSeen == CP_SOLVER_OK, "callback not fired for healthy scene");
	cpSpaceFree(space);

	/* Reuse the callback on a failing scene. */
	cpSpace *bad = cpSpaceNew();
	cpBody *body = cpSpaceAddBody(bad,
		cpBodyNew(NAN, NAN));
	cpBodySetPosition(body, cpv(0, 5));
	cpSpaceAddShape(bad, cpBoxShapeNew(body, 10, 10, 0));
	cpSpaceGetSolverConfig(bad)->errorCallback = on_error;
	cpSpaceStep(bad, TESTBED_DT);
	CHECK(callbackSeen == CP_SOLVER_INVALID_MASS,
	      "callback fires with CP_SOLVER_INVALID_MASS on NaN body");
	CHECK(cpSpaceGetLastSolverError(bad) == CP_SOLVER_INVALID_MASS,
	      "sticky error flag set on invalid body");
	cpSpaceFree(bad);
}

int
main(void)
{
	printf("=== cp_solver_diagnostics start ===\n");
	test_nan_mass();
	test_extreme_mass_ratio();
	test_callback_and_healthy();
	printf("=== cp_solver_diagnostics done: %d failure(s) ===\n", failures);
	return failures ? 1 : 0;
}
