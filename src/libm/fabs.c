/*
 * libm/fabs.c
 * C mathematics library.
 * fabs()
 * Floating absolute value.
 */

#include <math.h>

#if	EMU87
#include "emumath.h"
#endif

double
fabs(x) double x;
{
	return (x < 0.0) ? -x : x;
}

/* end of libm/fabs.c */
