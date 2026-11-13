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
#include "cpBroadPhase.h"
#include "prime.h"

// Starting table size for a freshly created spatial hash. The table grows on
// demand (see cpSpaceHash.c) so that its size stays linear in the number of
// shapes actually inserted.
#define CP_BROADPHASE_MIN_CELLS 257

static void
cpBroadPhaseCreateSpatialHash(cpSpatialIndex **staticIndex, cpSpatialIndex **dynamicIndex)
{
	cpSpatialIndex *staticShapes = cpSpaceHashNew(
		CP_BROADPHASE_DEFAULT_CELLDIM, CP_BROADPHASE_MIN_CELLS,
		(cpSpatialIndexBBFunc)cpShapeGetBB, NULL
	);
	cpSpatialIndex *dynamicShapes = cpSpaceHashNew(
		CP_BROADPHASE_DEFAULT_CELLDIM, CP_BROADPHASE_MIN_CELLS,
		(cpSpatialIndexBBFunc)cpShapeGetBB, staticShapes
	);

	*staticIndex = staticShapes;
	*dynamicIndex = dynamicShapes;
}

static void
cpBroadPhaseCreateBBTree(cpSpatialIndex **staticIndex, cpSpatialIndex **dynamicIndex)
{
	cpSpatialIndex *staticShapes = cpBBTreeNew((cpSpatialIndexBBFunc)cpShapeGetBB, NULL);
	cpSpatialIndex *dynamicShapes = cpBBTreeNew((cpSpatialIndexBBFunc)cpShapeGetBB, staticShapes);

	*staticIndex = staticShapes;
	*dynamicIndex = dynamicShapes;
}

void
cpBroadPhaseCreate(cpSpatialIndex **staticIndex, cpSpatialIndex **dynamicIndex, cpBroadPhaseType type)
{
	switch(type){
		case CP_BROADPHASE_BB_TREE:
			cpBroadPhaseCreateBBTree(staticIndex, dynamicIndex);
			break;

		case CP_BROADPHASE_SPATIAL_HASH:
		default:
			cpBroadPhaseCreateSpatialHash(staticIndex, dynamicIndex);
			break;
	}
}

static void
cpBroadPhaseCopyShape(cpShape *shape, cpSpatialIndex *index)
{
	cpSpatialIndexInsert(index, shape, shape->hashid);
}

void
cpBroadPhaseReplace(cpSpace *space, cpBroadPhaseType type)
{
	cpSpatialIndex *staticShapes = NULL;
	cpSpatialIndex *dynamicShapes = NULL;
	cpBroadPhaseCreate(&staticShapes, &dynamicShapes, type);

	// Migrate every indexed object while preserving its hash id.
	cpSpatialIndexEach(space->staticShapes, (cpSpatialIndexIteratorFunc)cpBroadPhaseCopyShape, staticShapes);
	cpSpatialIndexEach(space->dynamicShapes, (cpSpatialIndexIteratorFunc)cpBroadPhaseCopyShape, dynamicShapes);

	cpSpatialIndexFree(space->staticShapes);
	cpSpatialIndexFree(space->dynamicShapes);

	space->staticShapes = staticShapes;
	space->dynamicShapes = dynamicShapes;
}

void
cpBroadPhaseUseBBTree(cpSpace *space)
{
	cpBroadPhaseReplace(space, CP_BROADPHASE_BB_TREE);
}

void
cpBroadPhaseUseSpatialHash(cpSpace *space)
{
	cpBroadPhaseReplace(space, CP_BROADPHASE_SPATIAL_HASH);
}
