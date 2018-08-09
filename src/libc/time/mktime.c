/*
 * libc/time/mktime.c
 * C date and time library.
 * mktime()
 * ANSI 4.12.2.3.
 * Convert broken-down local time to time_t.
 */

#include <sys/compat.h>
#include <time.h>

/* From ctime.c ... */
extern	long	timezone;
int	dstadjust = 60 * 60;
static	char	dpm [] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
#define	FEB	1
#define	NWDAY	7		/* Number of weekdays */
#if	USE_PROTO
LOCAL int isleap (int yr)
#else
LOCAL int
isleap (yr)
register int yr;
#endif
{
	if (yr % 4000 == 0)
		return 0;
	return yr % 400 == 0 || (yr % 100 != 0 && yr % 4 == 0);
}

/* Force quantity *ip to range [min, max], adjusting *np accordingly. */
#ifdef USE_PROTO
LOCAL void
rangefix(int *ip, int *np, int min, int max)
#else
LOCAL void
rangefix(ip, np, min, max)
register int *ip, *np; int min, max;
#endif
{
	register int range;

	range = max - min + 1;
	while (*ip < min) {
		*ip += range;
		--*np;
	}
	while (*ip > max) {
		*ip -= range;
		++*np;
	}
}

#ifdef USE_PROTO
time_t
mktime(struct tm *timeptr)
#else
time_t
mktime(timeptr) register struct tm *timeptr;
#endif
{
	register int year, month, ndays;
	time_t t;

	/* Reduce second, minute, hour and month args to appropriate ranges. */
	rangefix(&(timeptr->tm_sec),  &(timeptr->tm_min),  0, 59);
	rangefix(&(timeptr->tm_min),  &(timeptr->tm_hour), 0, 59);
	rangefix(&(timeptr->tm_hour), &(timeptr->tm_mday), 0, 23);
	rangefix(&(timeptr->tm_mon),  &(timeptr->tm_year), 0, 11);

	/* Find the correct year and month for the given mday. */
	year = timeptr->tm_year + 1900;
	month = timeptr->tm_mon;
	dpm[FEB] = (isleap(year)) ? 29 : 28;
	while (timeptr->tm_mday < 1) {
		/* Move back a month. */
		if (--month < 0) {
			/* Move back a year. */
			--year;
			dpm[FEB] = (isleap(year)) ? 29 : 28;
			month = 11;
		}
		timeptr->tm_mday += dpm[month];
	}
	while (timeptr->tm_mday > dpm[month]) {
		/* Move up a month. */
		timeptr->tm_mday -= dpm[month];
		if (++(month) > 11) {
			/* Move up a year. */
			++year;
			dpm[FEB] = (isleap(year)) ? 29 : 28;
			month = 0;
		}
	}
	timeptr->tm_mon = month;
	timeptr->tm_year = year - 1900;

	/* Find day of the year. */
	for (ndays = timeptr->tm_mday - 1, month = 0; month < timeptr->tm_mon; month++)
		ndays += dpm[month];
	timeptr->tm_yday = ndays;

	/* Find days since the epoch 1/1/70 (wday=4) and day of the week. */
	if (year < 1970)
		return (time_t)-1;		/* before the epoch */
	for (year = 1970; year < timeptr->tm_year + 1900; ++year)
		ndays += (isleap(year)) ? 366 : 365;
	timeptr->tm_wday = (ndays + 4) % NWDAY;

	/* Compute the result. */
	t = (((time_t)ndays * 24			/* hours */
		+ timeptr->tm_hour) * 60		/* minutes */
		 + timeptr->tm_min) * 60		/* seconds */
		  + timeptr->tm_sec
		  + timezone;				/* localtime adjust */
	if (timeptr->tm_isdst)
		t -= dstadjust;				/* DST adjust */
	return t;
}

/* end of libc/time/mktime.c */
