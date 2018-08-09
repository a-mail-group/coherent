/*
 * libc/stdlib/abs.c
 * C general utilities library.
 * abs()
 * ANSI 4.10.6.1.
 * Return integer absolute value.
 * This doesn't work on the largest negative integer.
 */

#include <stdlib.h>

int
abs(x) int x;
{
	return (x < 0) ? -x : x;
}

/* end of libc/stdlib/abs.c */
