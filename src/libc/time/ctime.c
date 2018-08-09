/*
 * libc/time/ctime.c
 * C date and time library.
 * asctime()	ANSI 4.12.3.1
 * ctime()	ANSI 4.12.3.2
 * gmtime()	ANSI 4.12.3.3
 * localtime()	ANSI 4.12.3.4
 * tzset()	non-ANSI
 * tzname[]	non-ANSI
 * timezone	non-ANSI
 *
 * Pseudo system-5, employs TIMEZONE environment for gmt offset,
 * timezone abbreviations, and daylight savings time information.
 *	TIMEZONE = SSS:mmm:DDD:n.d.m:n.d.m:h:m
 * SSS - Standard timezone abbreviation up to 31 characters long
 * mmm - minutes west of GMT
 * DDD - Daylight timezone abbreviation also 31 characters
 * n.d.m - n'th occurrence of d'th day of week in m'th month of
 *	start and end of daylight savings time.  Negative n indicates
 *	counting backwards from end of month.  Zero n indicates absolute date,
 *	d'th day of m'th month.  Days and months from 1 to n.
 * h - Hour of change from standard to daylight.
 * m - Minutes of adjustment at change.
 * Example - Central standard in current US conventions:
 *	TIMEZONE=CST:3600:CDT:-1.1.4:-1.1.10:2:60
 * Only the first two fields are required.
 * If no daylight timezone is specified, then no daylight conversion is done.
 * If no dates for daylight are given, they default to 83-05-10 US standard.
 * If no hour of daylight time change is specified, it defaults to 2AM.
 * If no minutes of adjustment is specified, it defaults to 60.
 * These defaults can be changed by overwriting tzdstdef [].
 */

#include <sys/compat.h>
#include <time.h>
#include <stdlib.h>
#include <limits.h>

#if	0
/* Required for ftime (), now conditionalized out, cf. comments below. */
#include <sys/timeb.h>
#endif

#define	FEB	1
#define	NWDAY	7		/* Number of weekdays */
#define	NMON	12		/* Number of months */
#define	todigit(c) ((c) + '0')

/* Static data. */
static	char	daynames [3 * NWDAY + 1] = "SunMonTueWedThuFriSat";
static	char	dpm [] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static	int	dstadjust = 60 * 60;	/* In seconds */
static	struct	dsttimes {
	char	dst_occur, dst_day, dst_month;
	char	dst_hour;
} dsttimes [2] = { 
	{  1, 0, 3 },			/* First Sunday in April */
	{ -1, 0, 9 }			/* Last Sunday in October */
};
static	char	months [3 * NMON + 1] = "JanFebMarAprMayJunJulAugSepOctNovDec";
static	char	timestr [] = "AAA AAA DD DD:DD:DD DDDD\n";
static	struct	tm	tm;
static	char	tz0 [TZNAME_MAX + 1] = "GMT";
static	char	tz1 [TZNAME_MAX + 1] = "";

/* Global data. */
long	timezone = 0L;
char	* tzname [2] = { tz0, tz1 };
static	char	tzdstdef [] = "1.1.4:-1.1.10:2:60......";

#define	fromdigit(ch)	((ch) - '0')

/*
 * Scan in an unsigned number.
 */

#if	USE_PROTO
LOCAL unsigned scannum (CONST char ** cp1)
#else
LOCAL unsigned
scannum (cp1)
CONST char   **	cp1;
#endif
{
	char		ch;
	unsigned	tmp = 0;

	while (isdigit (ch = ** cp1)) {
		tmp = tmp * 10 + fromdigit (ch);
		(* cp1) ++;
	}
	return tmp;
}


/*
 * Scan in a time in hh:mm:ss format.
 */

#if	USE_PROTO
LOCAL long scanoffset (char ** src, long def)
#else
LOCAL long
scanoffset (src, def)
char	     **	src;
long		def;
#endif
{
	if (! isdigit (** src))
		return def;

	/*
	 * Scan hours
	 */

	def = scannum (src) * 60L * 60L;

	if (** src != ':' || ! isdigit (* ++ * src))
		return def;

	def += scannum (src) * 60L;	/* add in minutes */

	if (** src != ':' || ! isdigit (* ++ * src))
		return def;

	return def += scannum (src);
}


/*
 * Scan an M.N.D day specifier in.
 */

#if	USE_PROTO
LOCAL CONST char * scanmday (CONST char * cp1, struct dsttimes * dst)
#else
LOCAL CONST char *
scanmday (cp1, dst)
CONST char    *	cp1;
struct dsttimes * dst;
#endif
{
	if (* cp1 == '-') {
		++ cp1;
		dst->dst_occur = - scannum (& cp1);
	} else
		dst->dst_occur = scannum (& cp1);
	if (* cp1 != '.')
		return cp1;
	cp1 ++;

	dst->dst_day = scannum (& cp1);
	if (* cp1 != '.')
		return cp1;
	cp1 ++;

	dst->dst_month = scannum (& cp1);
	return cp1;
}


/*
 * Scan a single POSIX.1 DST rule in.
 */

#if	USE_PROTO
LOCAL char * scandstrule (CONST char * cp1, struct dsttimes * dst)
#else
LOCAL char *
scandstrule (cp1, dst)
CONST char    *	cp1;
struct dsttimes * dst;
#endif
{
	if (* cp1 != ',')
		return cp1;

	/*
	 * For now, only deal with 'M' format rather than Julian days.
	 */

	if (* ++ cp1 != 'M')
		return cp1;

	cp1 = scanmday (cp1, dst);

	/*
	 * Now check for optional hour of adjustment.
	 */

	if (* cp1 != '/') {
		dst->dst_hour = 2L * 60L * 60L;
		return cp1;
	}
	cp1 ++;

	dst->dst_hour = scanoffset (& cp1, 2L * 60L * 60L);
	return cp1;
}

			
/* Parse and set dst parameters */
#if	USE_PROTO
LOCAL void setdst (CONST char * cp1)
#else
LOCAL void
setdst (cp1)
CONST char    *	cp1;
#endif
{
	/* Get optional start of daylight time */
	if (* cp1) {
		cp1 = scanmday (cp1, dsttimes);
		while (* cp1 && * cp1 ++ != ':')
			/* DO NOTHING */ ;
	}

	/* Get optional end of daylight time */
	if (* cp1) {
		cp1 = scanmday (cp1, dsttimes + 1);
		while (* cp1 && * cp1 ++ != ':')
			/* DO NOTHING */ ;
	}

	/* Get optional hour of daylight time advance */
	if (* cp1) {
		dsttimes [1].dst_hour = dsttimes [0].dst_hour = atoi (cp1);
		while (* cp1 && * cp1 ++ != ':')
			/* DO NOTHING */ ;
	}

	/* Get optional minutes of adjustment */
	if (* cp1)
		dstadjust = atoi (cp1) * 60;
}


/*
 * Set up for no daylight saving time
 */
#if	USE_PROTO
LOCAL void nodst (void)
#else
LOCAL void
nodst ()
#endif
{
	dsttimes [1].dst_occur = dsttimes [0].dst_occur = 0;
	dstadjust = 0;
}

/*
 * Test whether a character is valid in a timezone name;
 * at a minimum, digits and sign specifiers must be excluded.
 */
#if	USE_PROTO
LOCAL int namechar (int ch)
#else
LOCAL int
namechar (ch)
int		ch;
#endif
{
	return !isdigit(ch) && ch != '-' && ch != '+';
}

/*
 * Set the timezone parameters. As per POSIX, we have to read the environment
 * every time, even if only to check that nothing has changed.
 */

#if	USE_PROTO
void tzset (void)
#else
void
tzset ()
#endif
{
	char * cp1, * cp2;

	if ((cp1 = getenv ("TIMEZONE")) == NULL) {
		char		ch;
		int		east;
		long		tmp;

		if ((cp1 = getenv ("TZ")) == NULL)
			return;

		cp2 = tzname [0];
		while ((ch = * cp1) != 0 && namechar (ch)) {
			if (cp2 < tzname [0] + TZNAME_MAX)
				* cp2 ++ = ch;
			cp1 ++;
		}
		* cp2 ++;

		east = 0;
		switch (* cp1) {

		case '-':
			east = 1;
			/* FALL THROUGH */

		case '+':
			cp1 ++;
			break;

		default:
			break;
		}
		timezone = scanoffset (& cp1, 0L);
		if (east)
			timezone = - timezone;

		/*
		 * Scan in DST timezone name.
		 */

		cp2 = tzname [1];
		while ((ch = * cp1) != 0 && namechar (ch)) {
			if (cp2 < tzname [1] + TZNAME_MAX)
				* cp2 ++ = ch;
			cp1 ++;
		}
		* cp2 ++;

		/*
		 * If DST name is empty, no daylight savings at all.
		 */

		if (tzname [1][0] == 0) {
			nodst ();
			return;
		}

		east = 0;
		switch (* cp1) {

		case '-':
			east = 1;
			/* FALL THROUGH */

		case '+':
			cp1 ++;
			break;

		default:
			break;
		}
		tmp = scanoffset (& cp1, timezone - 1 * 60UL * 60UL);
		if (east)
			tmp = - tmp;

		/*
		 * Convert from an absolute offset to a relative one, to deal
		 * with the way the rest of this module works.
		 */

		dstadjust = timezone - tmp;

		/*
		 * Look for DST rules.
		 */

		setdst (tzdstdef);
		scandstrule (& cp1, dsttimes);
		scandstrule (& cp1, dsttimes + 1);
		return;
	}

	timezone = 0;

	/* Read primary timezone name and nul terminate */
	cp2 = tzname [0];
	while (* cp1 && * cp1 != ':' && cp2 < & tzname [0][TZNAME_MAX])
		* cp2 ++ = * cp1 ++;
	* cp2 ++ = 0;
	while (* cp1 && * cp1 ++ != ':')
		/* DO NOTHING */ ;

	/* Read timezone offset and convert to seconds */
	timezone = (long)atoi (cp1) * 60L;
	while (* cp1 && * cp1 ++ != ':')
		/* DO NOTHING */ ;

	/* Read daylight timezone name and nul terminate */
	cp2 = tzname [1];
	while (* cp1 && * cp1 != ':' && cp2 < & tzname [1][TZNAME_MAX])
		* cp2 ++ = * cp1 ++;
	* cp2 ++ = 0;
	while (* cp1 && * cp1 ++ != ':')
		/* DO NOTHING */ ;

	/* Exit if no daylight time */
	if (tzname [1][0] == 0) {
		nodst ();
		return;
	}

	/* Set default dst parameters */
	setdst (tzdstdef);

	/* Set supplied dst parameters */
	setdst (cp1);
}


#if	0
/*
 * This ftime () is not required for COH 4.0.1r78 or later,
 * it is now a system call using the stub generated by sys / i386/ scall.s5.
 */
/*
 * ftime ()
 */
ftime (tb) struct timeb * tb;
{
	time_t t;

	tb->time = time (& t);
	settz ();	
	localtime (& t);
	tb->millitm = 0;
	tb->timezone = (short)(timezone / 60L);
       	tb->dstflag = isdaylight ();
}
#endif

/*
 * Most common interface, returns a static string
 * which is a printable version of the time and date.
 */

#if	USE_PROTO
char * ctime (time_t * tp)
#else
char *
ctime (tp)
time_t	      *	tp;
#endif
{
	return asctime (localtime (tp));
}


/*
 * Do what gmtime does for the local timezone.
 * And correct for daylight time.
 */

#if	USE_PROTO
struct tm * localtime (time_t * tp)
#else
struct tm *
localtime (tp)
time_t	      *	tp;
#endif
{
	long ltime;

	tzset ();
	ltime = * tp - timezone;
	gmtime (& ltime);

	/*
	 * If necessary, adjust for daylight saving time.
	 */
	if (isdaylight ()) {
		ltime = * tp - timezone + dstadjust;
		gmtime (& ltime);
		tm.tm_isdst = 1;
	} else
		tm.tm_isdst = 0;
	return & tm;
}


/*
 * Returns a printable version of the time
 * which has been broken down as in the tm structure.
 */

#if	USE_PROTO
char * asctime (struct tm * tmp)
#else
char *
asctime (tmp)
struct tm * tmp;
#endif
{
	register char * cp, * xp;
	register unsigned i;

	cp = timestr;

	/*
	 * Day of week
	 */

	if ((i = tmp->tm_wday) >= NWDAY)
		i = 0;
	xp = & daynames [i * 3];
	* cp ++ = * xp ++;
	* cp ++ = * xp ++;
	* cp ++ = * xp ++;
	* cp ++ = ' ';

	/*
	 * Month
	 */

	if ((i = tmp->tm_mon) >= NMON)
		i = 0;
	xp = & months [i * 3];
	* cp ++ = * xp ++;
	* cp ++ = * xp ++;
	* cp ++ = * xp ++;
	* cp ++ = ' ';

	/*
	 * Day of month
	 */

	if ((i = tmp->tm_mday) >= 10)
		* cp ++ = todigit (i / 10);
	else
		* cp ++ = ' ';
	* cp ++ = todigit (i % 10);
	* cp ++ = ' ';

	/*
	 * Hours:mins:seconds
	 */

	* cp ++ = todigit ((i = tmp->tm_hour) / 10);
	* cp ++ = todigit (i % 10);
	* cp ++ = ':';
	* cp ++ = todigit ((i = tmp->tm_min) / 10);
	* cp ++ = todigit (i % 10);
	* cp ++ = ':';
	* cp ++ = todigit ((i = tmp->tm_sec) / 10);
	* cp ++ = todigit (i % 10);
	* cp ++ = ' ';

	/*
	 * Year
	 */

	i = tmp->tm_year + 1900;
	* cp ++ = todigit (i / 1000);
	i = i % 1000;
	* cp ++ = todigit (i / 100);
	i = i % 100;
	* cp ++ = todigit (i / 10);
	* cp ++ = todigit (i % 10);
	* cp ++ = '\n';
	* cp ++ = 0;
	return timestr;
}


/*
 * Return 1 on leap years; 0 otherwise.
 */

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


/*
 * Check for daylight savings time.
 * Watch out for the Southern Hemisphere, where start month > end month.
 * This does not do the case where start == end correctly for all cases.
 */

#if	USE_PROTO
LOCAL int isdaylight (void)
#else
LOCAL int
isdaylight ()
#endif
{
	register int month, start, end, xday;

	if (tzname [1][0] == 0)
		return 0;			/* No name, no daylight time */
	month = tm.tm_mon;
	start = dsttimes [0].dst_month;
	end = dsttimes [1].dst_month;
	if ((start <= end && (month < start || month > end))
	 || (start >  end && (month < start && month > end)))
		return 0;			/* current month is never DST */
	else if (month == start) {		/* DST starts this month */
		xday = nthday (& dsttimes [0]);	/* DST starts on xday */
		if (tm.tm_mday != xday)
			return tm.tm_mday > xday;
		return tm.tm_hour >= dsttimes [0].dst_hour;
	} else if (month == end) {		/* DST ends this month */
		xday = nthday (& dsttimes [1]);	/* DST ends on xday */
		if (tm.tm_mday != xday)
			return tm.tm_mday < xday;
		return tm.tm_hour < dsttimes [1].dst_hour - 1;
	} else
		return 1;			/* current month is always DST */
}


#if	USE_PROTO
LOCAL int nthday (struct dsttimes * dp)
#else
LOCAL int
nthday (dp)
register struct dsttimes * dp;
#endif
{
	register int nthday;
	register int nth;

	if ((nth = dp->dst_occur) == 0)
		return dp->dst_day;
	nthday = tm.tm_mday - tm.tm_wday + dp->dst_day;
	if (nth > 0) {
		while (nthday > 0)
			nthday -= 7;
		do
			nthday += 7;
		while (-- nth > 0);
	} else {
		while (nthday <= dpm [tm.tm_mon])
			nthday += 7;
		do
			nthday -= 7;
		while (++ nth < 0);
	}
	return nthday;
}


/*
 * Do conversions for Greenwich Mean Time to
 * the tm structure.
 */

#if	USE_PROTO
struct tm * gmtime (time_t * tp)
#else
struct tm *
gmtime (tp)
time_t	      *	tp;
#endif
{
	long xtime;
	unsigned days;
	long secs;
	int year;
	int ydays;
	int wday;
	register char * mp;

	if ((xtime = * tp) < 0)
		xtime = 0;
	days = xtime /(60L * 60L * 24L);
	secs = xtime%(60L * 60L * 24L);
	tm.tm_hour = secs /(60L * 60L);
	secs = secs%(60L * 60L);
	tm.tm_min = secs /60;
	tm.tm_sec = secs%60;
	/*
	 * Start at Thursday (wday = 4) Jan 1, 1970 (the Epoch)
	 * and calculate from there.
	 */
	wday = (4 + days)%NWDAY;
	year = 1970;
	for (;;) {
		ydays = isleap (year) ? 366 : 365;
		if (days < ydays)
			break;
		year ++;
		days -= ydays;
	}
	tm.tm_year = year-1900;
	tm.tm_yday = days;
	tm.tm_wday = wday;
	/*
	 * Setup february's #days now.
	 */
	if (isleap (year))
		dpm [FEB] = 29;
	else
		dpm [FEB] = 28;
	for (mp = & dpm [0]; mp < & dpm [NMON] && days >= * mp; mp ++)
		days -= * mp;
	tm.tm_mon = mp-dpm;
	tm.tm_mday = days + 1;
	return & tm;
}

/* end of libc/time/ctime.c */
