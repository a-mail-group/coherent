/*
 * libc/gen/getopt.c
 * C library.
 * getopt()
 */

#include <sys/compat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

char	      *	optarg;
int		optind = 1;
int		opterr = 1;
int		optopt;

/*
 * Internal function to move to the end of an option list section (where
 * sections are delimited by '-'.
 */

#if	USE_PROTO
LOCAL CONST char * list_end (CONST char * optstring)
#else
LOCAL CONST char *
list_end (optstring)
CONST char    *	optstring;
#endif
{
	char		optchar;

	while ((optchar = * optstring) != 0 && optchar != '-')
		optstring ++;
	return optstring;
}


/*
 * Internal function to send an error message to standard error.
 */

#if	USE_PROTO
LOCAL int print_err (CONST char * msg, size_t len)
#else
LOCAL int
print_err (msg, len)
CONST char    *	msg;
size_t		len;
#endif
{
#if	STDERR_FILENO
	/*
	 * This is a POSIX system, bypass the stdio library.
	 */

	if (len == 0)
		len = strlen (msg);
	return write (STDERR_FILENO, msg, len) == len;
#else
	if (len == 0)
		len = strlen (msg);
	return fwrite (msg, 1, len, stderr) == len;
#endif
}


/*
 * POSIX.2 compatible getopt () library function.
 */

#if	USE_PROTO
int getopt (int argc, char * CONST * argv, CONST char * optstring)
#else
int
getopt (argc, argv, optstring)
int		argc;
char	* CONST	argv [];
CONST char    *	optstring;
#endif
{
static	char	      *	optsub = NULL;
	size_t		optstrlen;
	char		optchar;
	char	      *	optscan;
	int		has_arg;

	/*
	 * In order to allow all kinds of useful extended forms of "optstring"
	 * we permit an option-character '-' to delimit the list of single-
	 * letter option characters.
	 */

	argv += optind;

	if (* argv == NULL) 
		return -1;		/* no more arguments */

	optscan = list_end (optstring);

	while (optsub == NULL) {
		if ((* argv) [0] != '-' || (* argv) [1] == 0)
			return -1;	/* no leading '-', or just '-' */

		if ((* argv) [1] != '-')
			break;

		/*
		 * Some kind of long argument, either --longopt or
		 * just '--', indicating no more options.
		 */

		if ((* argv) [2] == 0) {
			optind ++;
			return -1;
		}

		/*
		 * Long options start after a '-' in "optstring", and
		 * are delimited by '-' thereafter. Long options can
		 * be abbreviated as long as the abbreviation is
		 * unique.
		 */

		optopt = -1;		/* index of matched long option */
		optstrlen = 0;		/* index of current long option */
		optarg = NULL;

		while (* (optstring = optscan) != 0) {
			optsub = * argv + 2;
			optstring ++;
			optscan = list_end (optstring);

			while ((optchar = * optsub ++) != 0 &&
			       optchar != '=' && optchar == * optstring) {
				optstring ++;
			}

			switch (optchar) {

			case 0:
			case '=':
				if (optopt == -1) {
					optopt = optstrlen;

					if (optscan [- 1] == ':')
						has_arg = 1;

					if (optchar == '=') {
						optarg = optsub;
						has_arg = 0;
					}
					break;
				}

				/*
				 * This has already matched another
				 * argument prefix.
				 */

				if (opterr &&
				    print_err (argv [ - optind], 0) &&
				    print_err (": ambiguous long option '",
					       0) &&
				    print_err (* argv, 0) &&
				    print_err ("'\n", 0)) {
				    	/* DO NOTHING */
				}
				optsub = NULL;
				optind ++;
				optopt = -1;
				return '-';	/* ambiguous */

			default:
				break;
			}

			optstrlen ++;
		}

		optsub = NULL;
		optind ++;
		argv ++;

		if (optstrlen == 0) {
			if (opterr &&
			    print_err (argv [- optind], 0) &&
			    print_err (": long options not supported\n", 0)) {
			    	/* DO NOTHING */
			}
			return -1;	/* no long options */
		}

		if (optopt == -1) {
			if (opterr &&
			    print_err (argv [- optind], 0) &&
			    print_err (": unknown long option '", 0) &&
			    print_err (argv [- 1], 0) &&
			    print_err ("'\n", 0)) {
			    	/* DO NOTHING */
			}
			return '-';	/* unknown option */
		}

		if (has_arg && * argv != NULL) {
		    	optarg = * argv;
		    	optind ++;
		}
		return '-';		/* flag long option */
	}

	/*
	 * Match a regular short option.
	 */

	optarg = NULL;
	if (optsub == NULL)
		optsub = * argv + 1;
	optchar = * optsub ++;

	if (* optsub == 0) {
		optsub = NULL;
		optind ++;
		argv ++;
	}
	has_arg = 0;

	while (optscan > optstring) {
		int		scan_char;

		if ((scan_char = * -- optscan) == ':') {
			has_arg ++;
			continue;
		}

		if (scan_char != optchar) {
			has_arg = 0;
			continue;
		}

		/*
		 * Match on option character: test to see whether there is an
		 * argument needed.
		 */

		if (has_arg == 1 && optsub == NULL && * argv == NULL) {
			/*
			 * A required option-argument is missing.
			 */

			if (opterr && * optstring != ':' &&
			    print_err (argv [- optind], 0) &&
			    print_err (": missing argument to option '", 0) &&
			    print_err (& optchar, 1) &&
			    print_err ("'\n", 0)) {
			    	/* DO NOTHING */
			}

			return * optstring == ':' ? ':' : '?';
		}

		if (has_arg) {
			if ((optarg = optsub) != NULL) {
				optsub = NULL;
				optind ++;
			} else if ((optarg = * argv) != NULL)
				optind ++;
		}

		return optchar;
	}

	/*
	 * No match; take the requisite error action.
	 */

	if (has_arg == 0 && opterr &&
	    print_err (argv [- optind], 0) &&
	    print_err (": invalid option character '", 0) &&
	    print_err (& optchar, 1) &&
	    print_err ("'\n", 0)) {
	    	/* DO NOTHING */
	}

	optopt = optchar;
	return '?';
}


#if	TEST

#if	USE_PROTO
int main (int argc, char * argv [])
#else
int
main (argc, argv)
int		argc;
char	      *	argv [];
#endif
{
	int		c;

	while ((c = getopt (argc, argv, "abf:o:-for:-false")) != -1) {
		printf ("char = '%c' optopt = '%c' (%d) optarg = '%s'\n",
			c, optopt, optopt, optarg == NULL ? "" : optarg);
		printf ("new ind = %d\n", optind);
	}

	while (optind < argc)
		printf ("Regular argument: %s\n", argv [optind ++]);

	return 0;
}

#endif	/* TEST */

/* end of libc/gen/getopt.c */
