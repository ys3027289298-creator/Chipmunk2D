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

/// @defgroup cpSolver cpSolver
///
/// Configurable constraint solver pipeline.
///
/// The iterative impulse solver, warm starting (persistent-contact impulse
/// reuse) and position correction are driven by a single cpSolverConfig
/// structure stored on each cpSpace. The default configuration reproduces the
/// historical Chipmunk 7 behavior bit-for-bit; the stability-oriented fields
/// (split position correction, early termination) are opt-in and off by
/// default, preserving external behavior.
/// @{

#ifndef CP_SOLVER_H
#define CP_SOLVER_H

#ifdef __cplusplus
extern "C" {
#endif

/// Solver diagnostics error codes. Numerical problems are detected and
/// reported here instead of aborting the host process.
typedef enum cpSolverError {
	/// No problem detected.
	CP_SOLVER_OK = 0,
	/// A body position, velocity or angle became NaN.
	CP_SOLVER_NAN_BODY = 1,
	/// A body position, velocity or angle became infinite.
	CP_SOLVER_INFINITE_BODY = 2,
	/// A body has NaN or negative mass/moment of inertia.
	CP_SOLVER_INVALID_MASS = 3,
	/// A contact effective mass is zero or non-finite (unsolvable contact).
	CP_SOLVER_DEGENERATE_CONTACT = 4,
	/// Ratio between the heaviest and lightest bodies sharing a contact
	/// exceeds cpSolverConfig.maxMassRatio.
	CP_SOLVER_EXTREME_MASS_RATIO = 5,
	/// A NaN or infinite impulse was produced during the iterative solve.
	CP_SOLVER_NAN_IMPULSE = 6,
	/// The solver reached the iteration cap without converging. This is a
	/// warning-level condition, not a hard failure.
	CP_SOLVER_DID_NOT_CONVERGE = 7,
} cpSolverError;

/// Human readable name of a solver error code. Never returns NULL.
CP_EXPORT const char *cpSolverErrorName(cpSolverError error);

///
/// Configuration of the constraint/contact solver pipeline.
///
/// Fields marked "(legacy)" keep their historical Chipmunk 7 semantics and
/// the default values reproduce the original cpSpaceStep() behavior exactly.
typedef struct cpSolverConfig {
	/// Maximum number of projected Gauss-Seidel iterations. Equivalent to
	/// cpSpace.iterations; cpSpaceSetIterations() keeps this field in sync.
	/// (legacy)
	int iterations;

	/// Reuse the previous step's accumulated contact/joint impulses as the
	/// starting point of the iterative solve. (legacy behavior: enabled)
	cpBool warmStart;

	/// Scale warm-started impulses by dt/previous_dt when the timestep
	/// changes. Matches the historical dt_coef handling. (legacy)
	cpBool warmStartDtScale;

	/// Position correction strategy.
	/// 0 = legacy Baumgarte bias velocity (no split correction).
	/// 1 = split position correction (Nudge): solve penetration with separate
	///      pseudo-impulses that never feed back into physical velocity.
	int positionCorrection;

	/// Baumgarte position-bias factor in [0,1], historically
	/// 1 - collisionBias^dt. Only used when positionCorrection == 0. (legacy)
	cpFloat baumgarteBias;

	/// Allowed penetration depth; contacts shallower than slop are not
	/// corrected. Mirrors cpSpace.collisionSlop. (legacy)
	cpFloat slop;

	/// Fraction of the remaining penetration removed per step by the split
	/// position corrector (Box2D-style, typically 0.2-0.8). Only used when
	/// positionCorrection == 1.
	cpFloat positionCorrectionRate;

	/// Maximum positional pseudo-velocity (world units per second) applied by
	/// the split corrector. Caps explosive corrections. 0 disables the cap.
	cpFloat maxCorrectionSpeed;

	/// Enable early termination of the iterative loop once the maximum
	/// residual contact velocity falls below velocityTolerance. Off by
	/// default so iteration counts stay deterministic.
	cpBool earlyTermination;

	/// Residual normal-velocity threshold (units/s) for early termination.
	cpFloat velocityTolerance;

	/// Heaviest/lightest dynamic-body mass ratio allowed across a contact.
	/// Ratios above this raise CP_SOLVER_EXTREME_MASS_RATIO. 0 disables the
	/// check. Default 1e12.
	cpFloat maxMassRatio;

	/// Error callback invoked once per cpSpaceStep() when a numerical problem
	/// is first detected. May be NULL.
	void (*errorCallback)(cpSpace *space, cpSolverError error, void *userData);
	void *errorUserData;
} cpSolverConfig;

/// Position correction strategies.
static const int CP_POSITION_CORRECTION_BAUMGARTE = 0;
static const int CP_POSITION_CORRECTION_SPLIT = 1;

/// Return the solver configuration of a space. The pointer is owned by the
/// space and remains valid for its lifetime.
CP_EXPORT cpSolverConfig *cpSpaceGetSolverConfig(cpSpace *space);

/// Replace the solver configuration. The structure is copied; passing NULL
/// restores the legacy defaults.
CP_EXPORT void cpSpaceSetSolverConfig(cpSpace *space, const cpSolverConfig *config);

/// Convenience accessor returning the default (legacy-equivalent) configuration.
CP_EXPORT cpSolverConfig cpSolverConfigDefault(void);

/// Last numerical problem recorded during cpSpaceStep(), or CP_SOLVER_OK.
/// The value is reset to CP_SOLVER_OK at the start of every step.
CP_EXPORT cpSolverError cpSpaceGetLastSolverError(const cpSpace *space);

/// Clear the sticky error code. Returns the previous value.
CP_EXPORT cpSolverError cpSpaceClearSolverError(cpSpace *space);

/// Number of solver iterations actually executed during the most recent step
/// (equals iterations unless early termination fired).
CP_EXPORT int cpSpaceGetLastIterationCount(const cpSpace *space);

/// Maximum residual contact velocity observed during the most recent step.
CP_EXPORT cpFloat cpSpaceGetLastResidualVelocity(const cpSpace *space);

#ifdef __cplusplus
}
#endif

#endif
/// @}
