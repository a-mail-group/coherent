/*
 * libm/log.c
 * C mathematics library.
 * log()
 * Natural (base e) logarithm function.
 */

#include <math.h>

#if	EMU87
#include "emumath.h"
#endif

double
log(x) double x;
{
	return log10(x) * LOG10BE;
}

/* end of libm/log.c */
