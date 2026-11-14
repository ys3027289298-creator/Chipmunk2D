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

#include <float.h>
#include <stdarg.h>

#include "chipmunk/chipmunk_private.h"

cpBody*
cpBodyAlloc(void)
{
	return (cpBody *)cpcalloc(1, sizeof(cpBody));
}

cpBody *
cpBodyInit(cpBody *body, cpFloat mass, cpFloat moment)
{
	body->space = NULL;
	body->shapeList = NULL;
	body->arbiterList = NULL;
	body->constraintList = NULL;
	
	body->velocity_func = cpBodyUpdateVelocity;
	body->position_func = cpBodyUpdatePosition;
	
	body->sleeping.root = NULL;
	body->sleeping.next = NULL;
	body->sleeping.idleTime = 0.0f;
	
	body->p = cpvzero;
	body->v = cpvzero;
	body->f = cpvzero;
	
	body->w = 0.0f;
	body->t = 0.0f;
	
	body->v_bias = cpvzero;
	body->w_bias = 0.0f;
	
	body->userData = NULL;
	
	// Setters must be called after full initialization so the sanity checks don't assert on garbage data.
	cpBodySetMass(body, mass);
	cpBodySetMoment(body, moment);
	cpBodySetAngle(body, 0.0f);
	
	return body;
}

cpBody*
cpBodyNew(cpFloat mass, cpFloat moment)
{
	return cpBodyInit(cpBodyAlloc(), mass, moment);
}

cpBody*
cpBodyNewKinematic()
{
	cpBody *body = cpBodyNew(0.0f, 0.0f);
	cpBodySetType(body, CP_BODY_TYPE_KINEMATIC);
	
	return body;
}

cpBody*
cpBodyNewStatic()
{
	cpBody *body = cpBodyNew(0.0f, 0.0f);
	cpBodySetType(body, CP_BODY_TYPE_STATIC);
	
	return body;
}

void cpBodyDestroy(cpBody *body){}

void
cpBodyFree(cpBody *body)
{
	if(body){
		cpBodyDestroy(body);
		cpfree(body);
	}
}

#ifdef NDEBUG
	#define	cpAssertSaneBody(body)
#else
	static cpBool cpv_is_nan(cpVect v){return !(v.x == v.x && v.y == v.y);}
	static cpBool cpv_is_infinite(cpVect v){return (cpfabs(v.x) == INFINITY || cpfabs(v.y) == INFINITY);}
	
	static void
	cpBodySanityCheck(const cpBody *body)
	{
		cpBool badMass = !(body->m == body->m && body->m_inv == body->m_inv &&
		                  body->i == body->i && body->i_inv == body->i_inv) ||
		                 body->m < 0.0f || body->i < 0.0f;
		cpBool nanState = body->a != body->a || body->w != body->w ||
		                  body->t != body->t ||
		                  cpv_is_nan(body->p) || cpv_is_nan(body->v) || cpv_is_nan(body->f);
		cpBool infState = cpfabs(body->a) == INFINITY ||
		                  cpfabs(body->w) == INFINITY || cpfabs(body->t) == INFINITY ||
		                  cpv_is_infinite(body->p) || cpv_is_infinite(body->v) ||
		                  cpv_is_infinite(body->f);

		// Bodies already owned by a space feed the diagnostics pipeline instead
		// of aborting, so numerical faults become reportable error codes.
		if(body->space){
			if(badMass) cpSolverReportError(body->space, CP_SOLVER_INVALID_MASS);
			if(nanState) cpSolverReportError(body->space, CP_SOLVER_NAN_BODY);
			if(infState) cpSolverReportError(body->space, CP_SOLVER_INFINITE_BODY);
			return;
		}

		// A body not yet attached to a space may carry an explicitly injected
		// bad mass (the setter already warned). It is diagnosed later once the
		// body enters a space, so do not abort here either.
		if(badMass) return;

		cpAssertHard(!badMass, "Body's mass or moment is invalid.");
		cpAssertHard(!nanState, "Body's position/velocity/force contains NaN.");
		cpAssertHard(!infState, "Body's position/velocity/force is infinite.");
	}
	
	#define	cpAssertSaneBody(body) cpBodySanityCheck(body)
#endif

cpBool
cpBodyIsSleeping(const cpBody *body)
{
	return (body->sleeping.root != ((cpBody*)0));
}

cpBodyType
cpBodyGetType(cpBody *body)
{
	if(body->sleeping.idleTime == INFINITY){
		return CP_BODY_TYPE_STATIC;
	} else if(body->m == INFINITY){
		return CP_BODY_TYPE_KINEMATIC;
	} else {
		return CP_BODY_TYPE_DYNAMIC;
	}
}

void
cpBodySetType(cpBody *body, cpBodyType type)
{
	cpBodyType oldType = cpBodyGetType(body);
	if(oldType == type) return;
	
	// Static bodies have their idle timers set to infinity.
	// Non-static bodies should have their idle timer reset.
	body->sleeping.idleTime = (type == CP_BODY_TYPE_STATIC ? INFINITY : 0.0f);
	
	if(type == CP_BODY_TYPE_DYNAMIC){
		body->m = body->i = 0.0f;
		body->m_inv = body->i_inv = INFINITY;
		
		cpBodyAccumulateMassFromShapes(body);
	} else {
		body->m = body->i = INFINITY;
		body->m_inv = body->i_inv = 0.0f;
		
		body->v = cpvzero;
		body->w = 0.0f;
	}
	
	// If the body is added to a space already, we'll need to update some space data structures.
	cpSpace *space = cpBodyGetSpace(body);
	if(space != NULL){
		cpAssertSpaceUnlocked(space);
		
		if(oldType == CP_BODY_TYPE_STATIC){
			// TODO This is probably not necessary
//			cpBodyActivateStatic(body, NULL);
		} else {
			cpBodyActivate(body);
		}
		
		// Move the bodies to the correct array.
		cpArray *fromArray = cpSpaceArrayForBodyType(space, oldType);
		cpArray *toArray = cpSpaceArrayForBodyType(space, type);
		if(fromArray != toArray){
			cpArrayDeleteObj(fromArray, body);
			cpArrayPush(toArray, body);
		}
		
		// Move the body's shapes to the correct spatial index.
		cpSpatialIndex *fromIndex = (oldType == CP_BODY_TYPE_STATIC ? space->staticShapes : space->dynamicShapes);
		cpSpatialIndex *toIndex = (type == CP_BODY_TYPE_STATIC ? space->staticShapes : space->dynamicShapes);
		if(fromIndex != toIndex){
			CP_BODY_FOREACH_SHAPE(body, shape){
				cpSpatialIndexRemove(fromIndex, shape, shape->hashid);
				cpSpatialIndexInsert(toIndex, shape, shape->hashid);
			}
		}
	}
}



// Should *only* be called when shapes with mass info are modified, added or removed.
void
cpBodyAccumulateMassFromShapes(cpBody *body)
{
	if(body == NULL || cpBodyGetType(body) != CP_BODY_TYPE_DYNAMIC) return;
	
	// Reset the body's mass data.
	body->m = body->i = 0.0f;
	body->cog = cpvzero;
	
	// Cache the position to realign it at the end.
	cpVect pos = cpBodyGetPosition(body);
	
	// Accumulate mass from shapes.
	CP_BODY_FOREACH_SHAPE(body, shape){
		struct cpShapeMassInfo *info = &shape->massInfo;
		cpFloat m = info->m;
		
		if(m > 0.0f){
			cpFloat msum = body->m + m;
			
			body->i += m*info->i + cpvdistsq(body->cog, info->cog)*(m*body->m)/msum;
			body->cog = cpvlerp(body->cog, info->cog, m/msum);
			body->m = msum;
		}
	}
	
	// Recalculate the inverses.
	body->m_inv = 1.0f/body->m;
	body->i_inv = 1.0f/body->i;
	
	// Realign the body since the CoG has probably moved.
	cpBodySetPosition(body, pos);
	cpAssertSaneBody(body);
}

cpSpace *
cpBodyGetSpace(const cpBody *body)
{
	return body->space;
}

cpFloat
cpBodyGetMass(const cpBody *body)
{
	return body->m;
}

void
cpBodySetMass(cpBody *body, cpFloat mass)
{
	cpAssertHard(cpBodyGetType(body) == CP_BODY_TYPE_DYNAMIC, "You cannot set the mass of kinematic or static bodies.");
	// Numerical problems are recorded by the solver diagnostics instead of
	// aborting the host process. A NaN or non-finite inverse mass neutralizes
	// the body dynamically; the condition is flagged during cpSpaceStep().
	if(!(0.0f <= mass && mass < INFINITY)){
		cpMessage("0.0f <= mass && mass < INFINITY", __FILE__, __LINE__, 0, 0,
			"Mass must be positive and finite; invalid mass retained for diagnostics.");
		body->m = mass;
		body->m_inv = (mass > 0.0f && mass == mass && mass < INFINITY) ? 1.0f/mass : 0.0f;
		return;
	}
	
	cpBodyActivate(body);
	body->m = mass;
	body->m_inv = mass == 0.0f ? INFINITY : 1.0f/mass;
	cpAssertSaneBody(body);
}

cpFloat
cpBodyGetMoment(const cpBody *body)
{
	return body->i;
}

void
cpBodySetMoment(cpBody *body, cpFloat moment)
{
	if(!(moment >= 0.0f)){
		cpMessage("moment >= 0.0f", __FILE__, __LINE__, 0, 0,
			"Moment of Inertia must be positive; invalid moment retained for diagnostics.");
		body->i = moment;
		body->i_inv = 0.0f;
		return;
	}
	
	cpBodyActivate(body);
	body->i = moment;
	body->i_inv = moment == 0.0f ? INFINITY : 1.0f/moment;
	cpAssertSaneBody(body);
}

cpVect
cpBodyGetRotation(const cpBody *body)
{
	return cpv(body->transform.a, body->transform.b);
}

void
cpBodyAddShape(cpBody *body, cpShape *shape)
{
	cpShape *next = body->shapeList;
	if(next) next->prev = shape;
	
	shape->next = next;
	body->shapeList = shape;
	
	if(shape->massInfo.m > 0.0f){
		cpBodyAccumulateMassFromShapes(body);
	}
}

void
cpBodyRemoveShape(cpBody *body, cpShape *shape)
{
  cpShape *prev = shape->prev;
  cpShape *next = shape->next;
  
  if(prev){
		prev->next = next;
  } else {
		body->shapeList = next;
  }
  
  if(next){
		next->prev = prev;
	}
  
  shape->prev = NULL;
  shape->next = NULL;
	
	if(cpBodyGetType(body) == CP_BODY_TYPE_DYNAMIC && shape->massInfo.m > 0.0f){
		cpBodyAccumulateMassFromShapes(body);
	}
}

static cpConstraint *
filterConstraints(cpConstraint *node, cpBody *body, cpConstraint *filter)
{
	if(node == filter){
		return cpConstraintNext(node, body);
	} else if(node->a == body){
		node->next_a = filterConstraints(node->next_a, body, filter);
	} else {
		node->next_b = filterConstraints(node->next_b, body, filter);
	}
	
	return node;
}

void
cpBodyRemoveConstraint(cpBody *body, cpConstraint *constraint)
{
	body->constraintList = filterConstraints(body->constraintList, body, constraint);
}

// 'p' is the position of the CoG
static void
SetTransform(cpBody *body, cpVect p, cpFloat a)
{
	cpVect rot = cpvforangle(a);
	cpVect c = body->cog;
	
	body->transform = cpTransformNewTranspose(
		rot.x, -rot.y, p.x - (c.x*rot.x - c.y*rot.y),
		rot.y,  rot.x, p.y - (c.x*rot.y + c.y*rot.x)
	);
}

static inline cpFloat
SetAngle(cpBody *body, cpFloat a)
{
	body->a = a;
	cpAssertSaneBody(body);
	
	return a;
}

cpVect
cpBodyGetPosition(const cpBody *body)
{
	return cpTransformPoint(body->transform, cpvzero);
}

void
cpBodySetPosition(cpBody *body, cpVect position)
{
	cpBodyActivate(body);
	cpVect p = body->p = cpvadd(cpTransformVect(body->transform, body->cog), position);
	cpAssertSaneBody(body);
	
	SetTransform(body, p, body->a);
}

cpVect
cpBodyGetCenterOfGravity(const cpBody *body)
{
	return body->cog;
}

void
cpBodySetCenterOfGravity(cpBody *body, cpVect cog)
{
	cpBodyActivate(body);
	body->cog = cog;
	cpAssertSaneBody(body);
}

cpVect
cpBodyGetVelocity(const cpBody *body)
{
	return body->v;
}

void
cpBodySetVelocity(cpBody *body, cpVect velocity)
{
	cpBodyActivate(body);
	body->v = velocity;
	cpAssertSaneBody(body);
}

cpVect
cpBodyGetForce(const cpBody *body)
{
	return body->f;
}

void
cpBodySetForce(cpBody *body, cpVect force)
{
	cpBodyActivate(body);
	body->f = force;
	cpAssertSaneBody(body);
}

cpFloat
cpBodyGetAngle(const cpBody *body)
{
	return body->a;
}

void
cpBodySetAngle(cpBody *body, cpFloat angle)
{
	cpBodyActivate(body);
	SetAngle(body, angle);
	
	SetTransform(body, body->p, angle);
}

cpFloat
cpBodyGetAngularVelocity(const cpBody *body)
{
	return body->w;
}

void
cpBodySetAngularVelocity(cpBody *body, cpFloat angularVelocity)
{
	cpBodyActivate(body);
	body->w = angularVelocity;
	cpAssertSaneBody(body);
}

cpFloat
cpBodyGetTorque(const cpBody *body)
{
	return body->t;
}

void
cpBodySetTorque(cpBody *body, cpFloat torque)
{
	cpBodyActivate(body);
	body->t = torque;
	cpAssertSaneBody(body);
}

cpDataPointer
cpBodyGetUserData(const cpBody *body)
{
	return body->userData;
}

void
cpBodySetUserData(cpBody *body, cpDataPointer userData)
{
	body->userData = userData;
}

void
cpBodySetVelocityUpdateFunc(cpBody *body, cpBodyVelocityFunc velocityFunc)
{
	body->velocity_func = velocityFunc;
}

void
cpBodySetPositionUpdateFunc(cpBody *body, cpBodyPositionFunc positionFunc)
{
	body->position_func = positionFunc;
}

void
cpBodyUpdateVelocity(cpBody *body, cpVect gravity, cpFloat damping, cpFloat dt)
{
	// Skip kinematic bodies.
	if(cpBodyGetType(body) == CP_BODY_TYPE_KINEMATIC) return;
	
	// Invalid mass is a reportable diagnostic, not a fatal error. The zeroed
	// inverse mass makes the integration below a no-op for this body.
	if(!(body->m > 0.0f && body->i > 0.0f)){
		if(body->space) cpSolverReportError(body->space, CP_SOLVER_INVALID_MASS);
		return;
	}
	
	body->v = cpvadd(cpvmult(body->v, damping), cpvmult(cpvadd(gravity, cpvmult(body->f, body->m_inv)), dt));
	body->w = body->w*damping + body->t*body->i_inv*dt;
	
	// Reset forces.
	body->f = cpvzero;
	body->t = 0.0f;
	
	cpAssertSaneBody(body);
}

void
cpBodyUpdatePosition(cpBody *body, cpFloat dt)
{
	cpVect p = body->p = cpvadd(body->p, cpvmult(cpvadd(body->v, body->v_bias), dt));
	cpFloat a = SetAngle(body, body->a + (body->w + body->w_bias)*dt);
	SetTransform(body, p, a);
	
	body->v_bias = cpvzero;
	body->w_bias = 0.0f;
	
	cpAssertSaneBody(body);
}

cpVect
cpBodyLocalToWorld(const cpBody *body, const cpVect point)
{
	return cpTransformPoint(body->transform, point);
}

cpVect
cpBodyWorldToLocal(const cpBody *body, const cpVect point)
{
	return cpTransformPoint(cpTransformRigidInverse(body->transform), point);
}

void
cpBodyApplyForceAtWorldPoint(cpBody *body, cpVect force, cpVect point)
{
	cpBodyActivate(body);
	body->f = cpvadd(body->f, force);
	
	cpVect r = cpvsub(point, cpTransformPoint(body->transform, body->cog));
	body->t += cpvcross(r, force);
}

void
cpBodyApplyForceAtLocalPoint(cpBody *body, cpVect force, cpVect point)
{
	cpBodyApplyForceAtWorldPoint(body, cpTransformVect(body->transform, force), cpTransformPoint(body->transform, point));
}

void
cpBodyApplyImpulseAtWorldPoint(cpBody *body, cpVect impulse, cpVect point)
{
	cpBodyActivate(body);
	
	cpVect r = cpvsub(point, cpTransformPoint(body->transform, body->cog));
	apply_impulse(body, impulse, r);
}

void
cpBodyApplyImpulseAtLocalPoint(cpBody *body, cpVect impulse, cpVect point)
{
	cpBodyApplyImpulseAtWorldPoint(body, cpTransformVect(body->transform, impulse), cpTransformPoint(body->transform, point));
}

cpVect
cpBodyGetVelocityAtLocalPoint(const cpBody *body, cpVect point)
{
	cpVect r = cpTransformVect(body->transform, cpvsub(point, body->cog));
	return cpvadd(body->v, cpvmult(cpvperp(r), body->w));
}

cpVect
cpBodyGetVelocityAtWorldPoint(const cpBody *body, cpVect point)
{
	cpVect r = cpvsub(point, cpTransformPoint(body->transform, body->cog));
	return cpvadd(body->v, cpvmult(cpvperp(r), body->w));
}

cpFloat
cpBodyKineticEnergy(const cpBody *body)
{
	// Need to do some fudging to avoid NaNs
	cpFloat vsq = cpvdot(body->v, body->v);
	cpFloat wsq = body->w*body->w;
	return (vsq ? vsq*body->m : 0.0f) + (wsq ? wsq*body->i : 0.0f);
}

void
cpBodyEachShape(cpBody *body, cpBodyShapeIteratorFunc func, void *data)
{
	cpShape *shape = body->shapeList;
	while(shape){
		cpShape *next = shape->next;
		func(body, shape, data);
		shape = next;
	}
}

void
cpBodyEachConstraint(cpBody *body, cpBodyConstraintIteratorFunc func, void *data)
{
	cpConstraint *constraint = body->constraintList;
	while(constraint){
		cpConstraint *next = cpConstraintNext(constraint, body);
		func(body, constraint, data);
		constraint = next;
	}
}

void
cpBodyEachArbiter(cpBody *body, cpBodyArbiterIteratorFunc func, void *data)
{
	cpArbiter *arb = body->arbiterList;
	while(arb){
		cpArbiter *next = cpArbiterNext(arb, body);
		
		cpBool swapped = arb->swapped; {
			arb->swapped = (body == arb->body_b);
			func(body, arb, data);
		} arb->swapped = swapped;
		
		arb = next;
	}
}
