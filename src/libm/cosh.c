/*
 * libm/cosh.c
 * C mathematics library.
 * cosh()
 * Hyperbolic cosine.
 */

#include <math.h>
#include <errno.h>

#if	EMU87
#include "emumath.h"
#endif

double
cosh(x) double x;
{
	double r;
	register int e;

	e = errno;
	r = exp(x);
	errno = e;
	r = (r + 1.0 / r) / 2.0;
	return r;
}

/* end of libm/cosh.c */
