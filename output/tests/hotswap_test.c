#include <stdio.h>
#include "chipmunk/chipmunk.h"
#include "chipmunk/chipmunk_private.h"
#include "cpBroadPhase.h"
static unsigned s=11u;
static cpFloat rnd(cpFloat lo,cpFloat hi){s^=s<<13;s^=s>>17;s^=s<<5;return lo+(hi-lo)*(s/(cpFloat)0xFFFFFFFFu);}
static int count_index(cpSpatialIndex*idx){return cpSpatialIndexCount(idx);}
int main(void){
	cpSpace*sp=cpSpaceNew(); sp->damping=0.9; sp->gravity=cpv(0,-300);
	cpSpaceAddShape(sp, cpSegmentShapeNew(sp->staticBody,cpv(-2000,-220),cpv(2000,-220),2));
	s=11u;
	for(int i=0;i<300;i++){cpFloat r=rnd(6,14);cpBody*b=cpBodyNew(1,cpMomentForCircle(1,0,r,cpvzero));
		cpBodySetPosition(b,cpv(rnd(-1200,1200),rnd(0,1200)));cpSpaceAddBody(sp,b);
		cpSpaceAddShape(sp,cpCircleShapeNew(b,r,cpvzero));}
	printf("default dyn=%d static=%d\n", count_index(sp->dynamicShapes), count_index(sp->staticShapes));
	// run a few steps on default hash, then hot-swap to tree and back
	for(int k=0;k<10;k++)cpSpaceStep(sp,1/60.0);
	int arbHash=sp->arbiters->num;
	cpBroadPhaseUseBBTree(sp);
	printf("after swap->tree dyn=%d static=%d\n", count_index(sp->dynamicShapes), count_index(sp->staticShapes));
	for(int k=0;k<10;k++)cpSpaceStep(sp,1/60.0);
	int arbTree=sp->arbiters->num;
	cpBroadPhaseUseSpatialHash(sp);
	printf("after swap->hash dyn=%d static=%d\n", count_index(sp->dynamicShapes), count_index(sp->staticShapes));
	for(int k=0;k<10;k++)cpSpaceStep(sp,1/60.0);
	printf("arbiters hash=%d tree=%d final=%d (migrated shapes preserved)\n", arbHash, arbTree, sp->arbiters->num);
	cpSpaceFree(sp); printf("freed ok\n");
	return 0;
}
