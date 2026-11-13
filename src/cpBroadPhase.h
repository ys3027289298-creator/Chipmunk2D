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

// Internal broad-phase factory. The broad phase is a replaceable
// implementation of the cpSpatialIndex vtable (cpSpatialIndexClass); the
// default implementation is the cell-based spatial hash.
// Nothing in this header is part of the public Chipmunk API.

#ifndef CP_BROADPHASE_H
#define CP_BROADPHASE_H

#include "chipmunk/chipmunk_private.h"

// Identifiers of the selectable broad-phase backends.
typedef enum cpBroadPhaseType {
	CP_BROADPHASE_SPATIAL_HASH,
	CP_BROADPHASE_BB_TREE,
} cpBroadPhaseType;

// Default cell size of the spatial hash. Chosen to be comparable to the
// typical shape dimension so that ordinary shapes only touch a few cells.
#define CP_BROADPHASE_DEFAULT_CELLDIM 100.0

// Create a matched pair of spatial indexes (static + dynamic) implementing
// the requested broad phase. The dynamic index is wired to the static index
// exactly as cpSpaceInit()/cpSpaceUseSpatialHash() did before the refactor.
void cpBroadPhaseCreate(cpSpatialIndex **staticIndex, cpSpatialIndex **dynamicIndex, cpBroadPhaseType type);

// Replace the broad phase of a space in place, migrating every shape already
// stored in the old indexes. Shape identity, insertion order and the set of
// indexed objects are preserved.
void cpBroadPhaseReplace(cpSpace *space, cpBroadPhaseType type);

// Test/debug helper: switch an initialized space back to the AABB tree
// broad phase without going through the public cpSpace API. Declared in an
// internal header so that no public symbol or header changes.
void cpBroadPhaseUseBBTree(cpSpace *space);
void cpBroadPhaseUseSpatialHash(cpSpace *space);

#endif
