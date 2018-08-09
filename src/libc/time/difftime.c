/*
 * libc/time/difftime.c
 * C date and time library.
 * difftime()
 * ANSI 4.12.2.2.
 */

#include <time.h>

#ifdef USE_PROTO
double difftime(time_t time1, time_t time0)
#else
double
difftime(time1, time0)
time_t time1;
time_t time0;
#endif
{
	return (double)time1 - (double)time0;
}

/* end of libc/gen/difftime.c */
