/*
 * libc/gen/fnmatch.c
 * fnmatch()
 */

#include <fnmatch.h>

/*
 * Local helper for fnmatch () to match a pattern bracket-expression.
 */

#if	__USE_PROTO__
static int bracket (__CONST__ char * pattern, unsigned char stringchar,
		    unsigned char escape)
#else
static int
bracket (pattern, stringchar, escape)
__CONST__ char * pattern;
unsigned char	stringchar;
unsigned char	escape;
#endif
{
	int		notflag;
	unsigned char	rangelo;
	unsigned char	ch;

	if ((notflag = (* pattern == '!')) != 0)
		pattern ++;

	for (;;) {
		switch (ch = * pattern ++) {
		case 0:
			return __FNM_ERROR;	/* invalid pattern */

		case ']':
			return notflag ? __FNM_MATCH : FNM_NOMATCH;

		case '-':
			if (rangelo == 0)
				goto match;
			if ((ch = * pattern ++) == 0)
				return __FNM_ERROR;
			if (ch != escape) {
				if (stringchar > rangelo &&
				    stringchar <= ch)
				    	return notflag ? FNM_NOMATCH :
							 __FNM_MATCH;
				continue;
			}
			/* FALL THROUGH */

		case '\\':
			if (escape != 0 &&
			    (ch = * pattern ++) == 0)
			    	return __FNM_ERROR;	/* invalid pattern */
			/* FALL THROUGH */
match:
		default:
			if (ch == stringchar)
				return notflag ? FNM_NOMATCH : __FNM_MATCH;
			rangelo = ch;
			break;
		}
	}
}


/*
 * POSIX.2 filename matching.
 *
 * From POSIX.2 Draft 11 Section B.6:
 *
 * B.6.2 Description
 *
 * The fnmatch () function shall match patterns as described in 3.13.1 and
 * 3.13.2. It checks the string specified by the "string" argument to see if
 * it matches the pattern specified by the "pattern" argument.
 *
 * The "flags" argument modifies the interpretation of "pattern" and "string".
 * It is the bit-wise OR of zero or more of the flags shown in table B-11,
 * which are defined in the header <fnmatch.h>. If the FNM_PATHNAME flag is
 * set in "flags", then a slash character in "string" shall be explicitly
 * matched by a slash in "pattern"; it shall not be matched by either the
 * asterisk or question-mark special characters, nor by a bracket expression.
 * If the FNM_PATHNAME flag is not set, the slash character shall be treated
 * as an ordinary character.
 *
 * If FNM_NOESCAPE is not set in "flags", a backslash character (\) in
 * "pattern" followed by any other character shall match that second character
 * in "string". If particular, '\\' shall match a backslash in "string". If
 * FNM_NOESCAPE is set, a backslash character shall be treated as an ordinary
 * character.
 *
 * If FNM_PERIOD is set in "flags", then a leading period in "string" shall
 * match a period in "pattern" as described by rule (2) in 3.13.2, where the
 * location of "leading" is indicated by the value of FNM_PATHNAME:
 *	- if FNM_PATHNAME is set, a period is "leading" if it is the first
 *	  character in "string" or if it immediately follows a slash.
 *
 *	- if FNM_PATHNAME is not set, a period is "leading" only if it is the
 *	  first character of "string".
 *
 * If FNM_PERIOD is not set, then no special restrictions shall be placed on
 * matching a period.
 *
 * B.6.3 Returns
 *
 * If "string" matches the pattern specified by "pattern", then fnmatch ()
 * shall return zero. If there is no match, fnmatch () shall return
 * FNM_NOMATCH, which shall be defined in the header <fnmatch.h>. If an error
 * occurs, fnmatch () shall return another nonzero value.
 *
 * B.6.4 Errors
 *
 * This standard does not specify any error conditions that are required to
 * be detected by the fnmatch () function. Some errors may be detected under
 * unspecified conditions.
 */

#if	__USE_PROTO__
int fnmatch (__CONST__ char * pattern, __CONST__ char * string, int flags)
#else
int
fnmatch (pattern, string, flags)
__CONST__ char * pattern;
__CONST__ char * string;
int		flags;
#endif
{
	unsigned char	escape = (flags & FNM_NOESCAPE) != 0 ? '\\' : 0;
	unsigned char	path = (flags & FNM_PATHNAME) != 0 ? '/' : 0;
	unsigned char	ch;

	/*
	 * Deal with the leading period now.
	 */
newpath:
	if ((flags & FNM_PERIOD) != 0 && * string == '.') {
		if (* pattern ++ != '.')
			return FNM_NOMATCH;
		string ++;
	}

	while ((ch = * pattern ++) != 0) {
		switch (ch) {
		case '?':
			if ((ch = * string ++) != 0 && ch != path)
				continue;
			return FNM_NOMATCH;

		case '*':
			do {
				if (fnmatch (pattern, string, flags) ==
				    __FNM_MATCH)
					return __FNM_MATCH;
			} while ((ch = * string ++) != 0 && ch != path);
			return FNM_NOMATCH;

		case '[':
			if ((ch = * string) == 0)
				return FNM_NOMATCH;
			switch (bracket (pattern, ch, escape)) {

			case FNM_NOMATCH:
				return FNM_NOMATCH;

			case __FNM_MATCH:
				while ((ch = * pattern ++) != 0) {
				       	/*
				       	 * Hunt for end of brackets.
				       	 */

				       	if (ch == escape) {
				       		if (* pattern ++ == 0)
				       			break;
				       	} else if (ch == ']') {
				       		string ++;
                                                break;
                                        }
                                }
                                if (ch == ']')
                                	continue;

                                /* FALL THROUGH */
                                /*
                                 * An invalid pattern has been found, treat
                                 * the open-bracket as a literal.
                                 */
                        default:
                                break;
                        }
                        ch = '[';
                        /* FALL THROUGH TO DEFAULT */

                case '/':
                        if (ch == path) {
				if (* string ++ != path)
				    return FNM_NOMATCH;
				    
				/*
				 * Now we are at the beginning of a new path
				 * component, so do the special-case '.'
				 * handling now.
				 */

				goto newpath;
			}
			/* FALL THROUGH TO DEFAULT */

		case '\\':
			if (ch == escape &&
			    (ch = * pattern ++) == 0)
			    	return __FNM_ERROR;
			/* FALL THROUGH */

		default:
			if (ch != * string ++)
				return FNM_NOMATCH;
			break;
		}
	}

	return * string == 0 ? __FNM_MATCH : FNM_NOMATCH;
}


#if	_TEST

#if	USE_PROTO
int main (int argc, char * argv [])
#else
int
main (argc, argv)
int		argc;
char	      *	argv [];
#endif
{
	if (argc != 3)
		return -1;

	return fnmatch (argv [1], argv [2]);
}

#endif	/* _TEST */

/* end of libc/gen/fnmatch.c */
