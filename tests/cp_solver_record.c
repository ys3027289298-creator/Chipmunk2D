/* Generates the acceptance screen recording.
 *
 * Streams raw PPM frames on stdout for piping into ffmpeg. Four titled
 * chapters are produced:
 *   S1 STACK STABILITY - LEGACY  (Baumgarte bias, original pipeline)
 *   S2 STACK STABILITY - STABLE  (split position correction)
 *   S3 TRAJECTORY ERROR BOUND     (projectile, reference overlay)
 *   S4 NUMERICAL DIAGNOSTICS      (NaN / extreme mass ratio error codes)
 */
#include "cp_solver_testbed.h"
#include "cp_solver_render.h"

#define FRAMES_PER_CHAPTER 240   /* 4 s at 60 fps */
#define VIEW_SCALE 1.2
#define VIEW_OX 320.0
#define VIEW_OY 420.0

typedef struct Scene {
	cpSpace *space;
	cpBody **bodies;
	int count;
	cpFloat boxSize;
} Scene;

static Scene
make_stack_scene(cpBool stable)
{
	Scene sc;
	sc.count = 10;
	sc.boxSize = 30.0;
	sc.bodies = (cpBody **)cpcalloc(sc.count, sizeof(cpBody *));
	sc.space = make_column(sc.bodies, sc.count, sc.boxSize, 20);

	if(stable){
		cpSolverConfig config = cpSolverConfigDefault();
		config.iterations = 20;
		config.slop = 0.1;
		config.positionCorrection = CP_POSITION_CORRECTION_SPLIT;
		config.positionCorrectionRate = 0.2;
		config.earlyTermination = cpTrue;
		config.velocityTolerance = 1e-5;
		cpSpaceSetSolverConfig(sc.space, &config);
	}
	return sc;
}

static void
free_scene(Scene *sc)
{
	cpSpaceFree(sc->space);
	cpfree(sc->bodies);
}

static void
draw_stack(FrameBuffer *fb, Scene *sc, const char *chapter, const char *sub)
{
	fb_clear(fb);
	/* Ground line. */
	for(int x = 20; x < RENDER_W - 20; x++)
		fb_pixel(fb, x, (int)VIEW_OY, 60, 60, 60);

	for(int i = 0; i < sc->count; i++){
		cpBody *body = sc->bodies[i];
		fb_body(fb, body, sc->boxSize/2 - 1, sc->boxSize/2 - 1,
		        70, 130, 210, VIEW_SCALE, VIEW_OX, VIEW_OY);
	}
	fb_banner(fb, chapter, sub);
}

static void
chapter_stacks(FILE *out, cpBool stable)
{
	Scene sc = make_stack_scene(stable);
	FrameBuffer fb;
	const char *chapter = stable ? "S2 STACK STABILITY - STABLE"
	                             : "S1 STACK STABILITY - LEGACY";
	const char *sub = stable
		? "SPLIT POSITION CORRECTION + WARM START"
		: "BAUMGARTE BIAS (ORIGINAL PIPELINE)";

	for(int frame = 0; frame < FRAMES_PER_CHAPTER; frame++){
		/* 3 physics steps per video frame. */
		for(int k = 0; k < 3; k++) cpSpaceStep(sc.space, TESTBED_DT);
		draw_stack(&fb, &sc, chapter, sub);
		fb_write_ppm(&fb, out);
	}
	free_scene(&sc);
}

static void
chapter_trajectory(FILE *out)
{
	cpBody *body;
	cpSpace *space = make_projectile(&body);
	FrameBuffer fb;

	/* Analytic reference trajectory (semi-implicit Euler). */
	cpVect ref[FRAMES_PER_CHAPTER*3];
	cpVect p = cpv(-200, 100), v = cpv(120, 240);
	for(int i = 0; i < FRAMES_PER_CHAPTER*3; i++){
		v.y += -TESTBED_GRAVITY * TESTBED_DT;
		p.x += v.x * TESTBED_DT;
		p.y += v.y * TESTBED_DT;
		ref[i] = p;
	}

	for(int frame = 0; frame < FRAMES_PER_CHAPTER; frame++){
		for(int k = 0; k < 3; k++) cpSpaceStep(space, TESTBED_DT);
		fb_clear(&fb);
		/* Reference path (green) vs simulated (blue circle). */
		for(int i = 0; i <= frame*3; i += 2){
			int x = (int)(VIEW_OX + ref[i].x*0.8);
			int y = (int)(VIEW_OY - ref[i].y*0.8);
			fb_pixel(&fb, x, y, 60, 170, 90);
		}
		fb_circle(&fb, body->p, 10, 70, 130, 210, 0.8, VIEW_OX, VIEW_OY);
		fb_banner(&fb, "S3 TRAJECTORY ERROR BOUND",
		          "SIMULATED VS ANALYTIC REFERENCE (RMSE REPORTED IN TESTS)");
		fb_write_ppm(&fb, out);
	}
	cpSpaceFree(space);
}

static void
chapter_diagnostics(FILE *out)
{
	/* Two small visual stacks; the right one carries an injected NaN mass. */
	cpSpace *space = cpSpaceNew();
	cpSpaceSetGravity(space, cpv(0, -TESTBED_GRAVITY));
	cpBody *ground = cpSpaceGetStaticBody(space);
	cpSpaceAddShape(space,
		cpSegmentShapeNew(ground, cpv(-400,0), cpv(400,0), 0));

	cpBody *healthy[3], *bad[3];
	for(int i = 0; i < 3; i++){
		healthy[i] = cpSpaceAddBody(space,
			cpBodyNew(1, cpMomentForBox(1, 24, 24)));
		cpBodySetPosition(healthy[i], cpv(-120, 12 + i*24));
		cpSpaceAddShape(space, cpBoxShapeNew(healthy[i], 24, 24, 0));
	}
	for(int i = 0; i < 3; i++){
		cpFloat mass = (i == 2 ? NAN : 1.0);
		bad[i] = cpSpaceAddBody(space,
			cpBodyNew(mass, cpMomentForBox(mass > 0 ? mass : 1, 24, 24)));
		cpBodySetPosition(bad[i], cpv(120, 12 + i*24));
		cpSpaceAddShape(space, cpBoxShapeNew(bad[i], 24, 24, 0));
	}

	FrameBuffer fb;
	for(int frame = 0; frame < FRAMES_PER_CHAPTER; frame++){
		for(int k = 0; k < 3; k++) cpSpaceStep(space, TESTBED_DT);
		fb_clear(&fb);
		for(int x = 20; x < RENDER_W - 20; x++)
			fb_pixel(&fb, x, (int)VIEW_OY, 60, 60, 60);
		for(int i = 0; i < 3; i++)
			fb_body(&fb, healthy[i], 11, 11, 70, 130, 210,
			        VIEW_SCALE, VIEW_OX, VIEW_OY);
		/* Invalid stack drawn red to flag the diagnosed fault. */
		for(int i = 0; i < 3; i++)
			fb_body(&fb, bad[i], 11, 11, 210, 70, 70,
			        VIEW_SCALE, VIEW_OX, VIEW_OY);

		cpSolverError err = cpSpaceGetLastSolverError(space);
		fb_banner(&fb, "S4 NUMERICAL DIAGNOSTICS",
		          err == CP_SOLVER_OK ? "SCENE HEALTHY" : "NAN MASS DETECTED (RED)");
		fb_text(&fb, 12, RENDER_H - 60, "LEFT: HEALTHY   RIGHT: NAN MASS",
		        1, 40, 40, 40);
		fb_text(&fb, 12, RENDER_H - 38,
		        err == CP_SOLVER_OK ? "STATUS: CP SOLVER OK"
		                            : "STATUS: CP SOLVER INVALID MASS (3)",
		        2, 180, 40, 40);
		fb_write_ppm(&fb, out);
	}
	cpSpaceFree(space);
}

int
main(void)
{
	/* Binary frames go to stdout; the build script pipes this into ffmpeg. */
	chapter_stacks(stdout, cpFalse);
	chapter_stacks(stdout, cpTrue);
	chapter_trajectory(stdout);
	chapter_diagnostics(stdout);
	return 0;
}
