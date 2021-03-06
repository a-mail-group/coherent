/*
 * libm/log10.c
 * C mathematics library.
 * log10()
 * Common (base 10) logarithm function.
 * (Hart 2355, 19.74)
 */

#include <math.h>
#include <errno.h>

#if	EMU87
#include "emumath.h"
#endif

static readonly double logntab[] ={
	-0.1042911213725266949744122e+02,
	 0.1344458152275036223645300e+02,
	-0.4185596001312662063300000e+01,
	 0.1828759212091999337000000e+00
};
static readonly double logmtab[] ={
	-0.1200695907020063424342180e+02,
	 0.1948096618798093652415500e+02,
	-0.8911109060902708565400000e+01,
	 0.1000000000000000000000000e+01
};

double
log10(x) double x;
{
	double r, z;
	int n;

	if (x <= 0.0) {
		errno = EDOM;
		return 0.0;
	}
	if (x == 1.)
		return 0.0;
	x = frexp(x, &n);
	x *= SQRT2;
	z = (x - 1.0) / (x + 1.0);
	r = z * z;
	r = z * (_pol(r, logntab, 4) / _pol(r, logmtab, 4));
	r += (n - 0.5) * LOG2B10;
	return r;
}

/* end of libm/log10.c */
