/*
 * libm/exp.c
 * C mathematics library.
 * exp()
 * Exponential function.
 */

#include <math.h>

#if	EMU87
#include "emumath.h"
#endif

double
exp(x) double x;
{
	return _two(x * LOGEB2);
}

/* end of libm/exp.c */
