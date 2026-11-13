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

// Internal validation helpers. Release builds compile with -ffast-math
// (including -ffinite-math-only), which makes every arithmetic or comparison
// based finiteness test fold to "true". Classifying the IEEE-754 bit pattern
// through a memcpy into an integer is independent of the compiler's numeric
// assumptions and therefore survives -ffast-math.

#ifndef CP_VALIDATE_H
#define CP_VALIDATE_H

#include <string.h>
#include "chipmunk/chipmunk_private.h"

static inline cpBool
cpIsFinite(cpFloat value)
{
#if CP_USE_DOUBLES
	// Double precision: 11 exponent bits starting at bit 52.
	unsigned long long bits = 0;
	memcpy(&bits, &value, sizeof(value));
	return ((bits >> 52) & 0x7FF) != 0x7FF;
#else
	// Single precision: 8 exponent bits starting at bit 23.
	unsigned int bits = 0;
	memcpy(&bits, &value, sizeof(value));
	return ((bits >> 23) & 0xFF) != 0xFF;
#endif
}

#endif
