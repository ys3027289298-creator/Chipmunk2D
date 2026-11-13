// Shim used only when compiling the equivalence test against the pre-refactor
// (baseline) static library. The baseline default broad phase is the AABB
// tree, so cpBroadPhaseUseBBTree() is a no-op there.

#include "chipmunk/chipmunk.h"

void
cpBroadPhaseUseBBTree(cpSpace *space)
{
	(void)space; // baseline cpSpaceInit() already selects the AABB tree
}

void
cpBroadPhaseUseSpatialHash(cpSpace *space)
{
	cpSpaceUseSpatialHash(space, 100.0, 1000);
}
