/*
 * libm/atan.c
 * C mathematics library.
 * atan()
 * Inverse tangent function.
 * (Hart 5100, 17.24)
 */

#include <math.h>

#if	EMU87
#include "emumath.h"
#endif

static readonly double tanntab[] ={
	0.12097470017580907217240715e+04,
	0.30310745956115083044212807e+04,
	0.27617198246138834959053784e+04,
	0.11141290728455183546172942e+04,
	0.19257920144815596134742860e+03,
	0.11322159411676465523624500e+02,
	0.97627215917176330369830000e-01
};
static readonly double tanmtab[] ={
	0.12097470017580907287514197e+04,
	0.34343235961975351716547069e+04,
	0.36645449563283749893504796e+04,
	0.18216003392918464941509225e+04,
	0.42307164648090478045242060e+03,
	0.39917884248653798150199900e+02,
	0.10000000000000000000000000e+01
};

double
atan(x) double x;
{
	double r;
	register int i, s;

	s = 0;
	i = 0;
	if (x < 0.0) {
		s = 1;
		x = -x;
	}
	if (x > 1.0) {
		i = 1;
		x = 1/x;
	}
	r = x * x;
	r = x * (_pol(r, tanntab, 7)/_pol(r, tanmtab, 7));
	if (i)
		r = PI / 2.0 - r;
	return (s) ? -r : r;
}

/* end of libm/atan.c */
