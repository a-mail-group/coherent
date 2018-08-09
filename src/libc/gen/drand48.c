/*
 * libc/gen/drand48.c
 *
 * Generate pseudo-random numbers using a linear conguential formula
 * with 48-bit integer arithmetic.
 * See "System V Interface Definition", drand48(lib).
 *
 * Defines:
 *	double		drand48()
 *	double		erand48(unsigned short xsubi[3])
 *	long		jrand48(unsigned short xsubi[3])
 *	void		lcong48(unsigned short param[7])
 *	long		lrand48()
 *	long		mrand48()
 *	long		nrand48(unsigned short xsubi[3])
 *	unsigned short	*seed48(unsigned short seed16v[3])
 *	void		srand48(long seedval)
 *
 * Assumes: 16-bit shorts, 32-bit longs, 64-bit IEEE-format doubles.
 */


/************************ Manifest constants.  ************************/

/* Default 48-bit multiplier and default 16 bit addend. */
#define	A2	0x0005			/* multiplier, hi 16 bits	*/
#define	A1	0xDEEC			/* mulliplier, middle 16 bits	*/
#define	A0	0xE66D			/* multiplier, lo 16 bits	*/
#define	C	0x000B			/* addend			*/

/* Lo seed for srand48(). */
#define	LO_SEED	0x330E			/* used in srand48()		*/


/************************       Macros.        ************************/

/* Assign 48-bit src to 48-bit dst. */
#define	assign48(dst, src)	dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]

/*
 * Convert hi-order bits of 48-bit value to:
 *	31-bit nonnegative long in range [0, 2 ^ 31),
 *	32-bit signed long in range [-2 ^ 31, 2 ^ 31).
 * Conversion to double is complicated, cf. function cvt_d().
 */
#define	cvt_31(x)	((x[2] << 15) | (x[1] & 0x7FFF))
#define	cvt_32(x)	((x[2] << 16) | x[1])

/* Initialize multiplier and addend to default values. */
#define	init48()	a[0] = A0; a[1] = A1; a[2] = A2; c = C

/* Extract lo or hi word (16 bits) of dword (32 bit unsigned long) x. */
#define	lo(x)	((x) & 0xFFFF)
#define	hi(x)	((x) >> 16)


/************************     Local data.      ************************/

/* Multiplier, addend, current Xi. */
static	unsigned short	a[3] = { A0, A1, A2 };	/* multiplier		*/
static	unsigned short	c = C;			/* addend		*/
static	unsigned short	Xi[3] = { -1, -1, -1 };	/* Xi[0]==lo, Xi[2]==hi	*/


/************************     Global data.     ************************/

/* No global data. */


/************************  Local functions.    ************************/

/*
 * Add 48-bit quantity x and 16-bit addend y
 * to obtain a 48-bit result in z.
 */
static
unsigned short *
add48(z, x, y) unsigned short z[3], x[3], y;
{
	assign48(z, x);
	if ((z[0] += y) == 0 && y != 0)		/* propagate carry on overflow */
		if (++z[1] == 0)
			++z[2];
	return z;
}

/*
 * Multiply two 48-bit quantities x and y.
 * Throw away the upper 48 bits of the product
 * and return the low order 48 bits of the product in z.
 * Using dword (unsigned long) arithmetic for the partial products
 * and intermediate results makes the computation straightforward.
 *
 *	| word5	| word4	| word3	| word2	| word1	| word0	|
 * x: 				|  x2	|  x1	|  x0	|
 * y:				|  y2	|  y1	|  y0	|
 * Partial products:
 * 1				    	|    x0 * y0	|
 * 2				|    x1 * y0	|
 * 3			|    x2 * y0	|
 * 4				|    x0 * y1	|
 * 5			|    x1 * y1	|
 * 6		|    x2 * y1	|
 * 7			|    x0 * y2	|
 * 8		|    x1 * y2	|
 * 9	|    x2 * y2	|
 * z:	|     [ discarded ] 	|  z2	|  z1	|  z0	|
 */
static
unsigned short *
mul48(z, x, y) unsigned short z[3], x[3], y[3];
{
	register unsigned long	n;	/* 32-bit partial product */
	register unsigned long	z1, z2;	/* accumulators for 16-bit results */

	n = x[0] * y[0];		/* first partial product, cf. above */
	z[0] = lo(n);			/* lo 16 bits of result */
	z1 = hi(n);			/* accumulator for middle 16 bits */

	n = x[1] * y[0];		/* second partial product */
	z1 += lo(n);
	z2 = hi(n);			/* accumulator for hi 16 bits */

	z2 += x[2] * y[0];		/* third partial product */

	n = x[0] * y[1];		/* fourth partial product */
	z1 += lo(n);
	z2 += hi(n);
	z[1] = lo(z1);			/* middle 16 bits of result */
	z2 += hi(z1);			/* overflow from middle 16 bits */

	z2 += x[1] * y[1];		/* fifth partial product */
	z2 += x[0] * y[2];		/* seventh partial product */
	z[2] = lo(z2);			/* hi 16 bits of result */

	return z;
}

/*
 * Compute the next value of x
 * from the linear congruential formula x = a * x + c.
 */
static
unsigned short *
next(x) unsigned short x[3];
{
	unsigned short n[3];

	return add48(x, mul48(n, a, x), c);
}

/*
 * Convert a 48-bit value to a non-negative double in the range [0.0, 1.0).
 * Consider the 48-bit value as a floating point value with an implicit
 * binary point to its left, which puts it in the desired range.
 * Assumes IEEE format doubles:
 * 1 sign bit, 11 exponent bits, 53-bit significand with hidden 1 bit.
 */
static
double
cvt_d(x) unsigned short x[3];
{
	double			d;
	register unsigned long	i, j;
	register unsigned char	*dp;
	register int		exp;

	if (x[0] == 0 && x[1] == 0 && x[2] == 0)
		return 0.0;			/* no bits set in x */

	/*
	 * Using [i j] as a 64-bit register representing the IEEE double result,
	 * shift the 48-bit value to the correct significand position;
	 * shift left 5 bits initially, then left by 1 until hidden bit is on.
	 */
	i = (x[2] << 5) | (x[1] >> 11);		/* hi 32 bits */
	j = (x[1] << 21) | (x[0] << 5);		/* lo 32 bits */
	for (exp = 0x3FE; (i & 0x100000L) == 0; --exp) {
		/* Shift [i j] left by 1 and adjust exp until hidden bit on. */
		i <<= 1;
		i |= (j >> 31);
		j <<= 1;
	}

	/* Mask the hidden bit and pack the exponent into the result. */
	i &= 0xFFFFFL;
	i |= (exp << 20);

	/* Store the result from [i j] into the double d and return it. */
	dp = (unsigned char *)&d;
	*dp++ = j;
	*dp++ = j >> 8;
	*dp++ = j >> 16;
	*dp++ = j >> 24;
	*dp++ = i;
	*dp++ = i >> 8;
	*dp++ = i >> 16;
	*dp   = i >> 24;

	return d;
}


/************************ External functions.  ************************/

/*
 * Compute the next Xi and return a non-negative double
 * uniformly distributed in the range [0.0, 1.0).
 */
double
drand48()
{
	next(Xi);
	return cvt_d(Xi);
}

/*
 * Compute the next xsubi and return a non-negative double
 * uniformly distributed in the range [0.0, 1.0).
 */
double
erand48(xsubi) unsigned short xsubi[3];
{
	next(xsubi);
	return cvt_d(xsubi);
}

/*
 * Compute the next xsubi and return the hi-order 32 bits
 * as a signed long integer in the range [-2 ^ 31, 2 ^ 31).
 */
long
jrand48(xsubi) unsigned short xsubi[3];
{
	next(xsubi);
	return cvt_32(xsubi);
}

/*
 * Initialize initial Xi, multiplier a and addend c with given values.
 */
void
lcong48(param) unsigned short param[7];
{
	register unsigned short *usp;

	assign48(Xi, param);
	usp = &param[3];
	assign48(a, usp);
	c = param[6];
}

/*
 * Compute the next Xi and return the hi-order 31 bits
 * as a nonnegative long integer in the range [0, 2 ^ 31).
 */
long
lrand48()
{
	next(Xi);
	return cvt_31(Xi);
}

/*
 * Compute the next Xi and return the hi-order 32 bits
 * as a signed long integer in the range [-2 ^ 31, 2 ^ 31).
 */
long
mrand48()
{
	next(Xi);
	return cvt_32(Xi);
}

/*
 * Compute the next xsubi and return the hi-order 31 bits
 * as a nonnegative long integer in the range [0, 2 ^ 31).
 */
long
nrand48(xsubi) unsigned short xsubi[3];
{
	next(xsubi);
	return cvt_31(xsubi);
}

/*
 * Save the previous Xi and reseed it with given value.
 * Restore the default values of the multiplier and addend.
 * Return a pointer to the saved previous value.
 */
unsigned short *
seed48(seed16v) unsigned short seed16v[3];
{
	static unsigned short oldXi[3];

	init48();
	assign48(oldXi, Xi);
	assign48(Xi, seed16v);
	return oldXi;
}

/*
 * Seed Xi with with the given long and LO_SEED.
 * Restore the default values of the multiplier and addend.
 */
void
srand48(seedval) long seedval;
{
	init48();
	Xi[2] = hi(seedval);
	Xi[1] = lo(seedval);
	Xi[0] = LO_SEED;
}

/* end of libc/gen/drand48.c */
