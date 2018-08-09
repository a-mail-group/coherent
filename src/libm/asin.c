/*
 * libm/asin.c
 * C mathematics library.
 * asin()
 * Inverse sine function.
 */

#include <math.h>
#include <errno.h>

#if	EMU87
#include "emumath.h"
#endif

double
asin(x) double x;
{
	if (x < -1.0 || x > 1.0) {
		errno = EDOM;
		return 0.0;
	} else if (x == 1.0)
		return PI / 2.0;
	else if (x == -1.0)
		return -PI / 2.0;
	else
		return atan(x / sqrt(1.0 - x * x));
}

/* end of libm/asin.c */
