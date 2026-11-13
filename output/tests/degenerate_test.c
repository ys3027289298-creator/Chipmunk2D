// Degenerate shape rejection test.
//
// Each invalid shape construction is run in a forked child because chipmunk's
// hard assertions call abort(). A rejected construction must terminate the
// child abnormally; a valid construction must complete normally.

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "chipmunk/chipmunk.h"

static cpBody *body;

typedef void (*CaseFunc)(void);

static void zeroRadiusCircle(void){ cpShapeFree(cpCircleShapeNew(body, 0.0, cpvzero)); }
static void negativeRadiusCircle(void){ cpShapeFree(cpCircleShapeNew(body, -5.0, cpvzero)); }
static void nanRadiusCircle(void){ cpShapeFree(cpCircleShapeNew(body, (cpFloat)NAN, cpvzero)); }
static void nanOffsetCircle(void){ cpShapeFree(cpCircleShapeNew(body, 5.0, cpv((cpFloat)NAN, 0.0))); }

static void identicalSegment(void){ cpShapeFree(cpSegmentShapeNew(body, cpv(1, 2), cpv(1, 2), 1.0)); }
static void nanSegment(void){ cpShapeFree(cpSegmentShapeNew(body, cpv(0, 0), cpv((cpFloat)NAN, 1), 1.0)); }

static void tooFewVerts(void)
{
	cpVect verts[2] = {cpv(0, 0), cpv(1, 0)};
	cpShapeFree(cpPolyShapeNew(body, 2, verts, cpTransformIdentity, 0.0));
}

static void coincidentVerts(void)
{
	cpVect verts[4] = {cpv(0, 0), cpv(0, 0), cpv(1, 0), cpv(1, 1)};
	cpShapeFree(cpPolyShapeNew(body, 4, verts, cpTransformIdentity, 0.0));
}

static void collinearVerts(void)
{
	cpVect verts[4] = {cpv(0, 0), cpv(1, 0), cpv(2, 0), cpv(3, 0)};
	cpShapeFree(cpPolyShapeNew(body, 4, verts, cpTransformIdentity, 0.0));
}

static void nanVerts(void)
{
	cpVect verts[3] = {cpv(0, 0), cpv(1, 0), cpv((cpFloat)NAN, 1)};
	cpShapeFree(cpPolyShapeNew(body, 3, verts, cpTransformIdentity, 0.0));
}

static void validCircle(void){ cpShapeFree(cpCircleShapeNew(body, 5.0, cpvzero)); }
static void validSegment(void){ cpShapeFree(cpSegmentShapeNew(body, cpv(0, 0), cpv(1, 0), 0.0)); }
static void validPoly(void)
{
	cpVect verts[3] = {cpv(0, 0), cpv(1, 0), cpv(0, 1)};
	cpShapeFree(cpPolyShapeNew(body, 3, verts, cpTransformIdentity, 2.0));
}

typedef struct {
	const char *name;
	CaseFunc func;
	int expectReject;
} TestCase;

int
main(void)
{
	body = cpBodyNew(1.0, cpMomentForCircle(1.0, 0, 5, cpvzero));

	TestCase cases[] = {
		{"zero-radius circle", zeroRadiusCircle, 1},
		{"negative-radius circle", negativeRadiusCircle, 1},
		{"NaN-radius circle", nanRadiusCircle, 1},
		{"NaN-offset circle", nanOffsetCircle, 1},
		{"identical segment endpoints", identicalSegment, 1},
		{"NaN segment endpoint", nanSegment, 1},
		{"polygon with < 3 verts", tooFewVerts, 1},
		{"polygon with coincident verts", coincidentVerts, 1},
		{"polygon with collinear verts", collinearVerts, 1},
		{"polygon with NaN verts", nanVerts, 1},
		{"valid circle", validCircle, 0},
		{"valid zero-radius segment", validSegment, 0},
		{"valid polygon", validPoly, 0},
	};

	int failures = 0;

	for(size_t i=0; i<sizeof(cases)/sizeof(cases[0]); i++){
		pid_t pid = fork();
		if(pid == 0){
			// Silence the assertion message in the child.
			FILE *devnull = fopen("/dev/null", "w");
			if(devnull) dup2(fileno(devnull), STDERR_FILENO);
			cases[i].func();
			_exit(0);
		}

		int status = 0;
		waitpid(pid, &status, 0);
		int rejected = !WIFEXITED(status) || WEXITSTATUS(status) != 0;

		if(rejected != cases[i].expectReject){
			printf("FAIL %-28s expected %s, got %s\n",
				cases[i].name,
				cases[i].expectReject ? "REJECT" : "ACCEPT",
				rejected ? "REJECT" : "ACCEPT");
			failures++;
		} else {
			printf("ok   %-28s -> %s\n", cases[i].name, rejected ? "rejected" : "accepted");
		}
	}

	cpBodyFree(body);

	if(failures == 0){
		printf("PASS: all degenerate inputs rejected and all valid inputs accepted\n");
		return 0;
	}
	printf("RESULT: %d failures\n", failures);
	return 1;
}
