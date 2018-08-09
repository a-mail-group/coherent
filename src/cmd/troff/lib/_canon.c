/*
 * _canon.c
 * COHERENT canonicalization for Sparc.
 */

#if	!defined(sparc)
#error	!defined(sparc)
#endif	/* !defined(sparc) */

unsigned short
_canshort(unsigned short s)
{
	unsigned char a[2];

	a[0] = s & 0xFF;
	a[1] = s >> 8;
	return *(unsigned short *)&a[0];
}

unsigned long
_canlong(unsigned long l)
{
	unsigned char a[4];

	a[0] = (l >> 16) & 0xFF;
	a[1] = l >> 24;
	a[2] = l & 0xFF;
	a[3] = l >> 8;
	return *(unsigned long *)&a[0];
}
