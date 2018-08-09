/*
 * libc/stdlib/labs.c
 * C general utilities library.
 * labs()
 * ANSI 4.10.6.3.
 * Long integer absolute value.
 * This does not work on the largest negative long integer.
 */

#include <stdlib.h>

long
labs(j) register long j;
{
	return (j < 0) ? -j : j;
}

/* end of libc/stdlib/labs.c */
