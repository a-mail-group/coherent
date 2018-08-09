/*
 * libm/atan2.c
 * C mathematics library.
 * atan2()
 * Inverse tangent, given two sides of a right triangle.
 */

#include <math.h>

#if	EMU87
#include "emumath.h"
#endif

double
atan2(y, x) double y, x;
{
	double r;

	if (x == 0.0) {
		r = PI / 2.0;
		if (y < 0.0)
			r = -r;
		return r;
	}
	r = atan(y / x);
	if (x < 0.0) {
		if (y < 0.0)
			r -= PI;
		else
			r += PI;
	}
	return r;
}

/* end of libm/atan2.c */
