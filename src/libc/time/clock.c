/*
 * libc/time/ctime.c
 * C date and time library.
 * clock()
 * ANSI 4.12.2.1.
 */

#include <time.h>
#include <sys/times.h>

#ifdef USE_PROTO
clock_t clock(void)
#else
clock_t
clock()
#endif
{
	struct tms junk_tms;

	return (clock_t)times(& junk_tms);
}

/* end of libc/gen/clock.c */
