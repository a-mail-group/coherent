/*
 * make -- maintain program groups
 * td 80.09.17
 * things done:
 *	20-Oct-82	Made nextc properly translate "\\\n[ 	]*" to ' '.
 *	15-Jan-85	Made portable enough for z-8000, readable enough for
 *			human beings.
 *	06-Nov-85	Added free(t) to make() to free used space.
 *	07-Nov-85	Modified docmd() to call varexp() only if 'cmd'
 *			actually contains macros, for efficiency.
 *	24-Feb-86	Minor fixes by rec to many things.  Deferred
 *			macro expansion in actions until needed, deferred
 *			getmdate() until needed, added canonicalization in
 *			archive searches, allowed ${NAME} in actions for
 *			shell to expand, put macro definitions in malloc,
 *			entered environ into macros.
 *	17-Oct-86	Very minor MS-DOS changes by steve: add _cmdname[],
 *			conditionalize archive code as #if COHERENT || GEMDOS.
 *	 8-Dec-86	Rec makes inpath() search current directory first,
 *			and allows : in dependency list under MSDOS && GEMDOS.
 *	 8-Feb-91	steve: fix comment handling, allow "\#", allow ${VAR}.
 *			Add docmd0() to make $< and $* work in Makefiles.
 *	12-Feb-91	steve: add $SRCPATH source path searching.
 *	 1-Nov-91	steve: fix bug in nextc() to handle "\n\t\n" correctly
 *      29-Sep-92	michael: fix problem with defining a rule that also
 *				exists in the ACTIONFILE.	
 *      08-Oct-92	michael: fix problem with making targets with no
 *				specified actions (empty productions).
 */

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _I386
#include <arcoff.h>
#include <fcntl.h>
#else
#include <ar.h>
#endif
#include <canon.h>
#include <path.h>
#include <time.h>

#include <sys/compat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>

#include	"make.h"

#if	MSDOS
char	_cmdname [] = "make";
#endif

char usage [] = "Usage: make [-deiknpqrstS] [-f file] [macro=value] [target]";
char nospace [] = "out of space";
char badmac [] = "bad macro name";
char incomp [] = "incomplete line at end of file";

/* Command line flags. */
int	iflag;			/* ignore command errors */
int	sflag;			/* don't print command lines */
int	rflag;			/* don't read built-in rules */
int	nflag;			/* don't execute commands */
int	tflag;			/* touch files rather than run commands */
int	qflag;			/* zero exit status if file up to date */
int	pflag;			/* print macro defns, target descriptions */
int	dflag;			/* debug mode -- verbose printout */
int	eflag;			/* make environ macros protected */
int	kflag;			/* continue to build other targets */

enum {
	MACRO_AT,
	MACRO_QUESTION,
	MACRO_LESS,
	MACRO_STAR,
	MACRO_PERCENT,
	MAX_MACRO
};

/* Globals. */
unsigned char	backup [NBACKUP];
int		defining;	/* nonzero => do not expand macros */
int		instring;	/* Are we in the middle of a string? */
SYM	      *	deflt;
char	      *	deftarget;
FILE	      *	fd;
int		lastc;
int		lineno;
char		macroname [NMACRONAME + 1];
char	      *	mvarval [MAX_MACRO];	/* list of macro variable values */
int		nbackup;
time_t		now;
char	      *	srcpath;
SYM	      *	suffixes;
SYM	      *	sym;
char		tokbuf [NTOKBUF];
char	      *	token;
int		toklen;
char	      *	tokp;
int 		inactionfile = 0;
TOKEN	      *	filep;		/* for -f arguments */
TOKEN	      *	targets;		/* target arguments */

/*
 * Statically declare make-macros with special values to cause the various
 * kinds of special-macro expansion magic to happen.
 */

enum {
	DEF_NORMAL,
	DEF_PROTECTED = 1,
	DEF_EXPORT = 2,
	DEF_NOEXPORT = 4,
	DEF_NOALLOC = 8
};

MACRO _specials [] = {
	{ _specials + 1,  "\n-0", "@", DEF_PROTECTED | DEF_NOEXPORT },
	{ _specials + 2,  "\n-1", "?", DEF_PROTECTED | DEF_NOEXPORT },
	{ _specials + 3,  "\n-2", "<", DEF_PROTECTED | DEF_NOEXPORT },
	{ _specials + 4,  "\n-3", "*", DEF_PROTECTED | DEF_NOEXPORT },
	{ _specials + 5,  "\n-4", "%", DEF_PROTECTED | DEF_NOEXPORT },
	{ _specials + 6,  "\nF0", "@F", DEF_PROTECTED | DEF_NOEXPORT },
	{ _specials + 7,  "\nF1", "?F", DEF_PROTECTED | DEF_NOEXPORT },
	{ _specials + 8,  "\nF2", "<F", DEF_PROTECTED | DEF_NOEXPORT },
	{ _specials + 9,  "\nF3", "*F", DEF_PROTECTED | DEF_NOEXPORT },
	{ _specials + 10, "\nF4", "%F", DEF_PROTECTED | DEF_NOEXPORT },
	{ _specials + 11, "\nD0", "@D", DEF_PROTECTED | DEF_NOEXPORT },
	{ _specials + 12, "\nD1", "?D", DEF_PROTECTED | DEF_NOEXPORT },
	{ _specials + 13, "\nD2", "<D", DEF_PROTECTED | DEF_NOEXPORT },
	{ _specials + 14, "\nD3", "*D", DEF_PROTECTED | DEF_NOEXPORT },
	{ NULL,		  "\nD4", "%D", DEF_PROTECTED | DEF_NOEXPORT }
};

MACRO		* macro = _specials;

/* cry and die */
#if	USE_PROTO
NO_RETURN die (CONST char * format, ...)
#else
NO_RETURN
die (format)
CONST char    *	format;
#endif
{
	va_list		args;

	fflush (stdout);
	fprintf (stderr, "make: ");

	va_start (args, format);
	vfprintf (stderr, format, args);
	va_end (args);

	fprintf (stderr, "\n");
	exit (2);
}

/* print lineno, cry and die */
#if	USE_PROTO
NO_RETURN err (CONST char * format, ...)
#else
NO_RETURN
err (format)
CONST char    *	format;
#endif
{
	va_list		args;

	fflush (stdout);
	fprintf (stderr, "make: %d: ", lineno);

	va_start (args, format);
	vfprintf (stderr, format, args);
	va_end (args);

	fprintf (stderr, "\n");
	exit (2);
}

/* print lineno, issue warning */
#if	USE_PROTO
NO_RETURN warn (CONST char * format, ...)
#else
NO_RETURN
warn (format)
CONST char    *	format;
#endif
{
	va_list		args;

	fflush (stdout);
	fprintf (stderr, "make: warning: %d: ", lineno);

	va_start (args, format);
	vfprintf (stderr, format, args);
	va_end (args);

	fprintf (stderr, "\n");
	exit (2);
}

/* Malloc nbytes and abort on failure */
#if	USE_PROTO
VOID * mmalloc (size_t size)
#else
VOID *
mmalloc (size)
size_t		size;
#endif
{
	VOID	      *	p;
	if ((p = malloc (size)) == NULL)
		err (nospace);
	return p;
}


/*
 * read characters from backup (where macros have been expanded) or from
 * whatever the open file of the moment is. keep track of line #s.
 */

int
readc ()
{
	if (nbackup != 0)
		return backup [-- nbackup];
	if (lastc == '\n')
		lineno ++;
	lastc = getc (fd);
	return lastc;
}


/*
 * put c into backup []
 */

void
putback (c)
{
	if (c == EOF)
		return;
	if (nbackup == NBACKUP)
		err ("macro definition too long");
	backup [nbackup ++] = c;
}


/*
 * put s into backup
 */

void
unreads (s)
char * s;
{
	register char * t;

	t = s + strlen (s);
	while (t > s)
		putback (* -- t);
}


/*
 * Reverse the section of the pushback buffer from "oldsize" to the current
 * end. This is used to make macro-expansions come out right.
 */

void
reverse_putback (oldsize)
size_t		oldsize;
{
	size_t		end = nbackup;

	while (oldsize < end) {
		char		c = backup [oldsize];
		backup [oldsize ++] = backup [-- end];
		backup [end] = c;
	}
}


/*
 * Get a pointer to a macro definition for "name", or NULL.
 */

MACRO *
get_macro (name)
char	      *	name;
{
	MACRO	      *	scan;

	for (scan = macro ; scan != NULL ; scan = scan->next)
		if (strcmp (name, scan->name) == 0)
			break;

	return scan;
}


/*
 * return a pointer to the macro definition assigned to macro name s.
 * return NULL if the macro has not been defined.
 */

char *
mexists (s)
char * s;
{
	MACRO	      *	temp;

	if ((temp = get_macro (s)) != NULL)
		return temp->value;

	return NULL;
}


/*
 * install macro with name name and value value in macro []. Overwrite an
 * existing value if it is not protected.
 */

void
define (name, value, flags)
char * name;
char * value;
int flags;
{
	MACRO	      *	temp;

	if (dflag)
		printf ("define %s%s = %s\n",
			(flags & DEF_EXPORT) != 0 ? "export " : "",
			name, value);

	if ((temp = get_macro (name)) != NULL) {
		if ((temp->flags & DEF_PROTECTED) == 0) {
		    	if ((temp->flags & DEF_NOALLOC) == 0)
				free (temp->value);
			temp->value = value;
			temp->flags |= flags;
		} else if (dflag) {
			printf ("... definition suppressed\n");
			return;
		}
	} else {
		temp = (MACRO *) mmalloc (sizeof (* temp));
		temp->name = name;
		temp->value = value;
		temp->flags = flags;
		temp->next = macro;
		macro = temp;
	}

	if ((temp->flags & DEF_EXPORT) != 0 &&
	    (temp->flags & DEF_NOEXPORT) == 0) {
		name = strcpy (mmalloc (strlen (name) + strlen (value) + 2),
			       name);
		strcat (name, "=");
		strcat (name, value);

		putenv (name);
	}
}


/*
 * Accept single letter user defined macros
 */

int
ismacro (c)
int c;
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
	       (c >= 'A' && c <= 'Z');
}


/*
 * Get a block of l0 + l1 bytes copy s0 and s1 into it, and return a pointer to
 * the beginning of the block.
 */

char *
extend (s0, l0, s1, l1)
char * s0;
char * s1;
int l0;
int l1;
{
	char * t;

 	if (s0 == NULL)
 		t = mmalloc (l1);
 	else {
 		if ((t = realloc (s0, l0 + l1)) == NULL)
			err (nospace);
	}
	strncpy (t + l0, s1, l1);
	return t;
}


/*
 * Prepare to copy a new token string into the token buffer; if the old value
 * in token wasn't saved, tough matzohs.
 */

void
starttoken ()
{
	token = NULL;
	tokp = tokbuf;
	toklen = 0;
}


/*
 * Put c in the token buffer; if the buffer is full, copy its contents into
 * token and start agin at the beginning of the buffer.
 */

void
addtoken (c)
char c;
{
	if (tokp == tokbuf + NTOKBUF) {
		token = extend (token, toklen - NTOKBUF, tokbuf, NTOKBUF);
		tokp = tokbuf;
	}
	* tokp ++ = c;
	toklen ++;
}


/*
 * mark the end of the token in the buffer and save it in token.
 */

void
endtoken ()
{
	addtoken (0);
	token = extend (token, toklen - (tokp - tokbuf), tokbuf,
			tokp - tokbuf);
}


/*
 * Add a string to the current token.
 */

void
addstring (string)
char	      *	string;
{
	while (* string)
		addtoken (* string ++);
}


/*
 * Structure for determining what macros are being expanded at any given
 * point in time.
 */

typedef	struct macroexp	mac_t;

struct macroexp {
	char	      *	mac_text;
	char	      *	mac_name;
	mac_t	      *	mac_next;
};


/*
 * Perform recursion check.
 */

#if	USE_PROTO
void macro_check (mac_t * check)
#else
void
macro_check (check)
mac_t	      *	check;
#endif
{
	mac_t	      *	scan;

	for (scan = check->mac_next ; scan != NULL ; scan = scan->mac_next)
		if (scan->mac_text == check->mac_text) {
			err ("Attempt to recursively expand macro %s",
			     check->mac_name);
		}
}


/*
 * Expand substitutes the macros in actions into the current token.
 */

#if	USE_PROTO
void expand (void (* add) (), char * str, char * oldsuffix, char * newsuffix,
	     mac_t * link)
#else
void
expand (add, str, oldsuffix, newsuffix, link)
void	     (*	add) ();
char	      *	str;
char	      *	oldsuffix;
char	      *	newsuffix;
mac_t	      *	link;
#endif
{
	int c;
	char * p;
	int endc;
	int		oldsufflen = oldsuffix == NULL ? 0 :
							 strlen (oldsuffix);
	mac_t		mac_info;

	mac_info.mac_next = link;

	/*
	 * Look for a newline at the start of a macro to signal that the text
	 * is actually a special macro expansion. Since special macros do not
	 * contain other macros, we just plug the expansion straight in.
	 */

	if (* str == '\n') {
		int		pathmode = * ++ str;
		str = mvarval [* ++ str - '0'];

		/*
		 * We need to split each word in the source into path and
		 * file components; we also need to split into words to do
		 * the suffix-rewriting thing.
		 */

		while (* str) {
			char	      *	endword;

			if ((endword = strchr (str, ' ')) == NULL)
				endword = str + strlen (str);
			else if (endword == str) {
				(* add) (* str ++);
				continue;
			}

			p = endword;
			while (p > str && * p != PATHSEP)
				p --;

			switch (pathmode) {

			case 'F':
				str = p + (* p == PATHSEP);
				p = endword;
				break;

			case 'D':
				if (p == str) {
					str = ".";
					p = str + 1;
				}
				break;

			case '-':
				p = endword;
				break;
			}


			/*
			 * Check suffix.
			 */

			if (p - str >= oldsufflen &&
			    strncmp (p - oldsufflen, oldsuffix,
				     oldsufflen) == 0) {
				p -= oldsufflen;
				while (str < p)
					(* add) (* str ++);
				if (newsuffix != NULL)
					expand (add, newsuffix, NULL, NULL,
						link);
			} else {
				while (str < p)
					(* add) (* str ++);
			}

			if (* endword != 0)
				(* add) (' ');
			str = endword;
		}

		return;
	}

	while ((c = * str ++) != 0) {
		if (oldsufflen > 0 &&
		    strncmp (oldsuffix, str - 1, oldsufflen) == 0) {

		    	switch (* (str + oldsufflen - 1)) {
		    	case '\n':
		    	case '\t':
		    	case ' ':
		    	case 0:
			    	if (newsuffix != NULL)
				    	expand (add, newsuffix, NULL, NULL,
						link);
				str += oldsufflen - 1;
			    	continue;

			default:
				break;
			}
		}

		if (c != '$') {
			(* add) (c);
			continue;
		}

		switch (c = * str ++) {
		case 0:
			err (badmac);

		case '$':
			(* add) (c);
			continue;

		case '{':
		case '(':
			endc = (c == '(') ? ')' : '}';
			c = '(';
			p = str;
			do
				if ((c = * str ++) == 0)
					err (badmac);
			while (c != endc && c != ':');

			* -- str = 0;
			mac_info.mac_name = p;
			p = mexists (p);
			mac_info.mac_text = p;

			/*
			 * Scan the list of macros being expanded to ensure
			 * that we are not going to go into an infinite loop.
			 */

			macro_check (& mac_info);

			if ((* str ++ = c) == ':' && p != NULL) {
				char	      *	oldsuff = str;
				char	      *	newsuff;

				/*
				 * Set up for a rewriting expansion,
				 * $(<string>:<old>[=<new>])
				 */

				do
					if ((c = * str ++) == 0)
						err (badmac);
				while (c != endc && c != '=');

				str [-1] = 0;
				if (c == '=') {
					newsuff = str;

					do
						if ((c = * str ++) == 0)
							err (badmac);
					while (c != endc);

					str [- 1] = 0;
				} else
					newsuff = NULL;

				expand (add, p, oldsuff, newsuff, & mac_info);

				if (newsuff != NULL)
					newsuff [- 1] = '=';
				str [- 1] = endc;
				continue;
			}
			break;

		default:
			if (! ismacro (c)) {
				warn ("Illegal macro name");
				p = NULL;
				break;
			}
			/* FALL THROUGH */

		case '@':
		case '?':
		case '<':
		case '*':
		case '%':
			c = * str;
			* str = 0;

			mac_info.mac_name = str - 1;
			p = mexists (str - 1);
			mac_info.mac_text = p;

			/*
			 * Scan the list of macros being expanded to ensure
			 * that we are not going to go into an infinite loop.
			 */

			macro_check (& mac_info);

			* str = c;
			break;
		}

		if (p != NULL)
			expand (add, p, NULL, NULL, & mac_info);
	}
}


/*
 * Return the next character from the input file.
 * Eat comments.
 * Return EOS for newline not followed by an action.
 * Return '\n' for newline followed by an action.
 * If not in a macro definition or action specification,
 * then expand macro in backup or complain about the name.
 */

int
nextc ()
{
	register char * s;
	register int c, endc;

Again:
	if ((c = readc ()) == '\\') {
		c = readc ();
		if (c == '\n') {		/* escaped newline */
			while ((c = readc ()) == ' ' || c == '\t')
				/* DO NOTHING */ ; /* eat following whitespace */
			putback (c);
			return ' ';
		} else if (c == '#')
			return c;		/* "\#" means literal '#' */
		putback (c);
		return '\\';
	}
	if (c == '#' && ! instring) {
		do
			c = readc ();
		while (c != '\n' && c != EOF);
	}
	if (c == '"') {
		instring ^= 0x01;
		return c;
	}
	if (c == '\'') {
		instring ^= 0x02;
		return c;
	}

	if (c == '\n') {
		instring = 0;
Again2:
		if ((c = readc ()) != ' ' && c != '\t') {
			putback (c);
			if (c == '#')
				goto Again;	/* "\n# comment" */
			return EOS;		/* no action follows */
		}
		do
			c = readc ();
		while (c == ' ' || c == '\t')	/* skip whitespace */
			/* DO NOTHING */ ;
		if (c == '\n')
			goto Again2;		/* "\n\t\n" */
		putback (c);
		if (c == '#')
			goto Again;		/* "\n\t# comment" */
		return '\n';			/* action follows */
	}
	if (! defining && c == '$') {
		size_t		oldsize;

		s = macroname;
		* s ++ = c;
		c = readc ();
		* s ++ = c;
		if (c == '(' || c == '{') {
			endc = (c == '(') ? ')' : '}';

			while (' ' < (c = readc ()) && c < 0x7F && c != endc)
				if (s != macroname + NMACRONAME)
					* s ++ = c;
			if (c != endc)
				err (badmac);
			* s ++ = c;
		} else if (! ismacro (c))
			err (badmac);
		* s ++ = 0;

		/*
		 * Expand the macro.
		 */

		oldsize = nbackup;
		expand (putback, macroname, NULL, NULL, NULL);
		reverse_putback (oldsize);
		goto Again;
	}

	return c;
}


/*
 * Return 1 if c is EOS, EOF, or one of the characters in s
 */

int
delim (c, s)
int		c;
char	      *	s;
{
	return c == EOS || c == EOF || strchr (s, c) != NULL;
}


/*
 * Install value at the end of the token list which begins with next; return
 * a pointer to the beginning of the list, which is the one just installed if
 * next was NULL.
 */

TOKEN *
listtoken (value, next)
char * value;
TOKEN * next;
{
	register TOKEN * p;
	register TOKEN * t;

	t =(TOKEN *) mmalloc (sizeof * t);	/* Necessaire ou le contraire?*/
	t->value = value;
	t->next = NULL;
	if (next == NULL)
		return t;
	for (p = next ; p->next != NULL ; p = p->next)
		/* DO NOTHING */ ;
	p->next = t;
	return next;
}


/*
 * Free the overhead of a token list
 */

TOKEN *
freetoken (t)
TOKEN * t;
{
	TOKEN * tp;
	while (t != NULL) {
		tp = t->next;
		free (t);
		t = tp;
	}
	return t;
}


/*
 * Return a pointer to the symbol table entry with name "name", NULL if it's
 * not there.
 */

SYM *
sexists (name)
char * name;
{
	SYM * sp;

	for (sp = sym ; sp != NULL ; sp = sp->next)
		if (Streq (name, sp->name))
			return sp;
	return NULL;
}


/*
 * Copy a string.
 */

char *
copy_token (string)
char	      *	string;
{
	starttoken ();
	addstring (string);
	endtoken ();
	return token;
}


/*
 * Return the last modified date of file with name name. If it's an archive,
 * open it up and read the insertion date of the pertinent member.
 */

time_t
getmdate (name)
char * name;
{
#if	_I386
	char	* subname;
	char	* lwa;
	int	fd;
	int	len;
	char	magic [SARMAG];
	int	size;

	time_t	result;
	struct ar_hdr	hdrbuf;
#endif
	struct stat	statbuf;

	if (stat (name, & statbuf) == 0)
		return statbuf.st_mtime;


#if	_I386
	subname = strchr (name, '(');
	if (subname == NULL)
		return 0;

	lwa = & name [strlen (name) - 1];
	if (* lwa != ')')
		return 0;

	* subname = 0;
	fd = open (name, O_RDONLY);
	* subname ++ = '(';
	if (fd == EOF)
		return 0;

	if (read (fd, magic, SARMAG) != SARMAG) {
		close (fd);
		return 0;
	}
	if (strncmp (magic, ARMAG, SARMAG) != 0) {
		close (fd);
		return 0;
	}
	* lwa = 0;
	result = 0;
	len = strlen (subname);
	while (read (fd, & hdrbuf, sizeof hdrbuf) == sizeof hdrbuf) {
		if (strncmp (hdrbuf.ar_name, subname, len) == 0 &&
		    hdrbuf.ar_name [len] == '/') {
			result = atoi (hdrbuf.ar_date);
			break;
		}
		if ((size = atoi (hdrbuf.ar_size)) < 0 ||
		    lseek (fd, (size + 1) & ~ 1, SEEK_CUR) == -1) {
			subname [-1] = 0;
			die ("archive %s is corrupt", name);
		}
	}
	* lwa = ')';

	close (fd);

	if (dflag && result == 0)
		printf (" %s: member not found\n", name);
	return result;
#else
	return 0;
#endif
}


/*
 * Does file name exist?
 */

int
fexists (name)
char * name;
{
#if 0
	if (dflag)
		printf ("fexists (%s) = %d getmdate (name) = %d\n", name,
		getmdate (name) != 0, getmdate (name));
#endif
	return getmdate (name) != 0;
}


/*
 * Check to see whether the named file exists in the current directory.
 */

char *
findbase (name)
char	      *	name;
{
	if ((name = strrchr (name, PATHSEP)) != NULL && fexists (name + 1))
		return copy_token (name + 1);
	return NULL;
}


/*
 * Find name on srcpath.
 * Return 'name' unchanged if file exists as 'name', 'name' is absolute,
 * or 'name' not found on sourcepath.
 * If successful, return pointer to allocated copy.
 */

char *
fpath (name)
char * name;
{
	register char * s;

	if (fexists (name) || * name == PATHSEP || srcpath == NULL ||
	    (s = path (srcpath, name, R_OK)) == NULL)
		return name;
	return copy_token (s);
}


/*
 * Look for symbol with name "name" in the symbol table; install it if it's
 * not there; initialize the action and dependency lists to NULL, the type to
 * unknown, zero the modification date, and return a pointer to the entry.
 */

SYM *
lookup (name)
char * name;
{
	SYM * sp;

	if ((sp = sexists (name)) != NULL)
		return sp;
	sp = (SYM *) mmalloc (sizeof (* sp));	/* necessary?*/
	sp->name = name;
	sp->filename = fpath (name);
	sp->action = NULL;
	sp->deplist = NULL;
	sp->type = T_UNKNOWN;
	sp->done = 0;
	sp->moddate = 0;
	sp->next = sym;
	sym = sp;
	return sp;
}


/*
 * Install a dependency with symbol having name "name", action "action" in
 * the end of the dependency list pointed to by next. If s has already
 * been noted as a file in the dependency list, install action. Return a
 * pointer to the beginning of the dependency list.
 */

DEP *
adddep (name, action, next)
char * name;
char * action;
DEP * next;
{
	DEP * v;
	SYM * s;
	DEP * dp;

	s = lookup (name);
	for (v = next ; v != NULL ; v = v->next)
		if (s == v->symbol) {
			if (action != NULL) {
				if (v->action != NULL)
					err ("multiple detailed actions for %s",
						s->name);
				v->action = action;
			}
			return next;
		}

	v = (DEP *) malloc (sizeof (* v));
	v->symbol = s;
	v->action = action;
	v->next = NULL;
	if (next == NULL)
		return v;
	for (dp = next ; dp->next != NULL ; dp = dp->next)
		/* DO NOTHING */ ;
	dp->next = v;
	return next;
}


/*
 * Do everything for a dependency with left-hand side cons, r.h.s. ante,
 * action "action", and one or two colons. If cons is the first target in the
 * file, it becomes the default target. Mark each target in cons as detailed
 * if twocolons, undetailed if not, and install action in the symbol table
 * action slot for cons in the latter case. Call adddep () to actually create
 * the dependency list.
 */

void
install (cons, ante, action, twocolons)
TOKEN * cons;
TOKEN * ante;
char * action;
int twocolons;
{
	SYM * cp;
	TOKEN * ap;

	if (deftarget == NULL &&
	    (cons->value [0] != '.' || ! ismacro (cons->value [1])))
		deftarget = cons->value;

	if (dflag) {
		printf ("Ante:");
		ap = ante;
		while (ap != NULL) {
			printf (" %s", ap->value);
			ap = ap->next;
		}
		printf ("\nCons:");
		ap = cons;
		while (ap != NULL) {
			printf (" %s", ap->value);
			ap = ap->next;
		}
		printf ("\n");
		if (action!= NULL)
			printf ("Action: '%s'\n", action);
		if (twocolons)
			printf ("two colons\n");
	}

	for (; cons != NULL ; cons = cons->next) {
		cp = lookup (cons->value);
		if (cp == suffixes && ante == NULL) {
			cp->deplist = NULL;
			continue;
		}

		if (twocolons) {
			if (cp->type == T_UNKNOWN)
				cp->type = T_DETAIL;
			else if (cp->type != T_DETAIL)
				err ("'::' not allowed for %s", cp->name);
		} else {
			if (cp->type == T_UNKNOWN)
				cp->type = T_NODETAIL;
			else if (cp->type != T_NODETAIL)
				err ("must use '::' for %s", cp->name);
			if (action != NULL) {
				if (cp->action == NULL) {
					cp->action = action;
				} else if (! inactionfile)
					err ("multiple actions for %s",
					     cp->name);
			}
		}

		for (ap = ante ; ap != NULL ; ap = ap->next)
			cp->deplist = adddep (ap->value,
					      twocolons ? action : NULL,
					      cp->deplist);
	}
}


/*
 * Try and process a directive line if the line consists of a string of
 * tokens without a ':', '=' or ';' and with no actions following.
 */

void	input	();

void
directive (tokens)
TOKEN	      *	tokens;
{
	if (tokens == NULL)
		return;

	if (strcmp (tokens->value, "include") == 0) {
		FILE	      *	oldfd = fd;

		if (tokens->next == NULL || tokens->next->next != NULL)
			err ("wrong number of arguments to \"include\"");

		fd = NULL;
		input (tokens->next->value);
		fd = oldfd;
		return;
	}
	err ("Unknown directive \"%s\"", tokens->value);
}


/*
 * Read macros, dependencies, and actions from the file with name file, or
 * from whatever file is already open. The first string of tokens is saved
 * in a list pointed to by tp; if it was a macro, the definition goes in
 * token, and we install it in macro []; if tp points to a string of targets,
 * its dependencies go in a list pointed to by dp, and the action to recreate
 * it in token, and the whole shmear is installed.
 */

void
input (file)
char * file;
{
	TOKEN * tp = NULL, * dp = NULL;
	int c;
	char * action;
	int twocolons;

	if (file != NULL && (fd = fopen (file, "r")) == NULL)
		die ("cannot open %s", file);
	lineno = 1;
	lastc = EOF;
	for (;;) {
		/*
		 * NIGEL: nextc () will only return '\n' if it is followed
		 * by whitespace, otherwise it returns an EOS. However, a
		 * line that begins with whitespace is *not* necessarily an
		 * action (it could be an indented macro definition, for
		 * example) and here we avoid giving bogus error messages by
		 * eating such benign newlines.
		 */

		do
			c = nextc ();
		while (c == '\n');

		for (;;) {
			while (c == ' ' || c == '\t')
				c = nextc ();
			if (delim (c, "=:;\n"))
				break;
			starttoken ();
			while (! delim (c, " \t\n=:;")) {
				addtoken (c);
				c = nextc ();
			}
			endtoken ();
			tp = listtoken (token, tp);
		}

		switch (c) {
		case EOF:
			if (tp != NULL)
				err (incomp);
			fclose (fd);
			return;

		case EOS:
			directive (tp);
			break;

		case '\n':
			err ("newline after target or macroname");

		case ';':
			err ("; after target or macroname");

		case '=':
			if (tp == NULL || tp->next != NULL)
				err ("= without macro name or in token list");
			defining ++;
			while ((c = nextc ()) == ' ' || c == '\t')
				/* DO NOTHING */ ;
			starttoken ();
			/*
			 * NIGEL: there never used to be a check for '\n' in
			 * here. Since the makefiles generate by 'imake' have
			 * idented macro-definitions, the EOS/'\n' thing is
			 * causing much damage.
			 */
			while (c != EOS && c != '\n' && c != EOF) {
				addtoken (c);
				c = nextc ();
			}
			endtoken ();
			define (tp->value, token, DEF_NORMAL);
			defining = 0;
			break;

		case ':':
			if (tp == NULL)
				err (": without preceding target");
			c = nextc ();
			if (c ==':') {
				twocolons = 1;
				c = nextc ();
			} else
				twocolons = 0;

			for (;;) {
				while (c == ' ' || c == '\t')
					c = nextc ();
				if (delim (c, "=:;\n"))
					break;
				starttoken ();
				while (! delim (c, TDELIM)) {
					addtoken (c);
					c = nextc ();
				}
				endtoken ();
				dp = listtoken (token, dp);
			}

			switch (c) {
#if	! MSDOS && ! GEMDOS
			case ':':
				err ("::: or : in or after dependency list");
#endif
			case '=':
				err ("= in or after dependency");

			case EOF:
				err (incomp);

			case ';':
				/*
				 * Normally, nextc () skips the whitespace,
				 * but only after a newline, so we have to do
				 * it for this case.
				 */

				do
					c = readc ();
				while (c == ' ' || c == '\t');
				putback (c);

			case '\n':
				++ defining;
				starttoken ();
				while ((c = nextc ()) != EOS && c != EOF)
					addtoken (c);
				endtoken ();
				defining = 0;
				action = token;
				break;

			case EOS:
				action = NULL;
			}
			install (tp, dp, action, twocolons);
		}
		tp = freetoken (tp);
		dp = freetoken (dp);
		dp = NULL;
	}
}


/*
 * Input with library lookup
 */

void
inlib (file)
char * file;
{
	char * pathstr;

	if ((pathstr = getenv ("LIBPATH")) == NULL)
		pathstr = DEFLIBPATH;
	pathstr = path (pathstr, file, R_OK);
	input (pathstr != NULL ? pathstr : file);
}


/*
 * Input first file in list which is found via SRCPATH.
 * Look in current directory first
 */

#if	USE_PROTO
void inpath (char * file, ...)
#else
void
inpath (file)
char * file;
#endif
{
	char ** vp, * cp;

	cp = NULL;
	for (vp = & file; * vp != NULL; vp += 1)
		if (access (* vp, R_OK) >= 0) {
			input (* vp);
			return;
		}

	for (vp = & file; * vp != NULL; vp += 1)
		if ((cp = path (srcpath, * vp, R_OK)) != NULL) {
			input (cp);
			break;
		}
}


/*
 * Return a pointer to the member of deplist which has name as the last
 * part of it's pathname, otherwise return NULL.
 */

SYM *
dexists (name, dp)
char * name;
DEP * dp;
{
	char * p;

	while (dp != NULL) {
		if ((p = strrchr (dp->symbol->name, PATHSEP)) &&
		    Streq (name, p + 1))
			return dp->symbol;

		dp = dp->next;
	}
	return NULL;
}


/*
 * look for '-' (ignore errors) and '@' (silent) in cmd, then execute it
 * and note the return status. Also look for '+' to force execution of a line
 * even if '-n' was specified.
 */

int
doit (cmd)
char * cmd;
{
	char * mark;
	int sflg, iflg, nflg, rstat;

	do {
		mark = strchr (cmd, '\n');
		if (mark != NULL)
			* mark = 0;

		iflg = iflag;
		sflg = sflag;
		nflg = nflag || tflag || qflag;

		for (;; cmd ++) {
			switch (* cmd) {

			case '-':
				iflg = 1;
				continue;

			case '@':
				sflg = 1;
				continue;

			case '+':
				if (nflag)
					printf ("%s\n", cmd);
				nflg = 0;
				continue;
			}

			break;
		}

		if (nflg) {
			if (nflag)
				printf ("%s\n", cmd);
			continue;
		}
		if (! sflg)
			printf ("%s\n", cmd);
		fflush (stdout);
		rstat = system (cmd);
		if (rstat != 0 && ! iflg) {
			if (sflg)
				err ("%s\texited with status %d", cmd, rstat);
			else
				err ("\texited with status %d", rstat);

			if (! kflag)
				die ("Halting make.");

			return 2;
		}
	} while (mark != NULL && * (cmd = mark + 1) != 0);

	return 0;
}


/*
 * Mark s as modified; if tflag, touch s, otherwise execute the necessary
 * commands.
 */

int
docmd (s, cmd, at, ques, less, star, percent)
SYM * s;
char * cmd;
char * at;
char * ques;
char * less;
char * star;
char * percent;
{
	int		retval;

	if (cmd == NULL)
		return 0;

	if (dflag)
		printf ("ex '%s'\n\t$@='%s'\n\t$?='%s'\n\t$<='%s'\n"
			"\t$*='%s'\n\t$%%='%s'\n",
			cmd, at, ques, less, star, percent);

	s->moddate = now;

	mvarval [MACRO_AT] = at;
	mvarval [MACRO_QUESTION] = ques;
	mvarval [MACRO_LESS] = less;
	mvarval [MACRO_STAR] = star;
	mvarval [MACRO_PERCENT] = percent;
	starttoken ();
	expand (addtoken, cmd, NULL, NULL, NULL);
	endtoken ();
	retval = doit (token);
	free (token);

	if (retval == 0 && tflag) {
		starttoken ();
		expand (addtoken, "@exec touch $@", NULL, NULL, NULL);
		endtoken ();
		retval = doit (token);
		free (token);
	}

	return retval;
}


/*
 * This function builds values suitable for the $@, $% and $* special macros.
 *
 * The return value points at the suffix part of "name"
 */

char *
build_macros (name, atp, percentp, starp)
char	      *	name;
char	     **	atp;
char	     **	percentp;
char	     **	starp;
{
	char	      *	prefix = strrchr (name, '(');
	char	      *	suffix = strrchr (name, '.');
	char	      *	temp;

	if (prefix != NULL && name [strlen (name) - 1] == ')') {

		starttoken ();
		temp = name;
		while (temp < prefix)
			addtoken (* temp ++);
		endtoken ();
		* atp = token;

		starttoken ();
		temp ++;
		while (* temp != ')')
			addtoken (* temp ++);
		endtoken ();
		* percentp = token;

		temp = prefix + 1;

		if (suffix != NULL && suffix > prefix)
			prefix = suffix;
		else
			prefix = name + strlen (name) - 1;
		suffix = ".a";
	} else {
		/*
		 * Since '.' may be matched by something further up in the
		 * pathname, check for a '/' nearer the end of the name.
		 */

		if (suffix == NULL || suffix == name ||
		    strrchr (suffix, PATHSEP) != NULL) {
			prefix = name + strlen (name);
			suffix = "";
		} else
			prefix = suffix;

		* atp = copy_token (name);
		* percentp = copy_token ("");

		temp = name;
	}

	starttoken ();
	while (temp < prefix)
		addtoken (* temp ++);
	endtoken ();
	* starp = token;

	return suffix;
}


/*
 * Like docmd (), except builds its own dependency list and prefix args.
 */

int
docmd0 (s, cmd, at, ques)
SYM * s;
char * cmd;
char * at;
char * ques;
{
#if	0
	DEP * dep;
	char * less;
#endif
	char	      *	prefix;
	char	      *	percent;
	int		retval;

	/* Build dependency list. */
	/*
	 * This is bogus; the $< macro is defined in POSIX.2 as only being
	 * meaningful for inference rules.
	 */
#if	0
	starttoken ();
	for (dep = s->deplist ; dep != NULL ; dep = dep->next) {
		addtoken (' ');
		addstring (dep->symbol->filename);
	}
	endtoken ();
	less = token;
#endif
	(void) build_macros (at, & at, & percent, & prefix);

	retval = docmd (s, cmd, at, ques,
			"$< is only permitted in inference rules",
			prefix, percent);

	free (at);
	free (prefix);
	free (percent);

	return retval;
}


/*
 * Deflt uses the commands associated to '.DEFAULT' to make the object
 * 'obj'.
 */

int
defalt (obj, ques)
SYM * obj;
char * ques;
{
	if (deflt == NULL) {
		if (obj->deplist == NULL && obj->type == T_UNKNOWN)
			die ("do not know how to make %s", obj->name);

#if	1
		/*
		 * There seems to be no reason not to consider a target with
		 * no actions up-to-date if it has had prerequisites rebuilt.
		 */

		obj->moddate = now;
#endif
		return 0;
	}

	return docmd0 (obj, deflt->action, obj->name, ques, "");
}


/*
 * Make s; first, make everything s depends on; if the target has detailed
 * actions, execute any implicit actions associated with it, then execute
 * the actions associated with the dependencies which are newer than s.
 * Otherwise, put the dependencies that are newer than s in token ($?),
 * make s if it doesn't exist, and call docmd.
 */

int		implicit ();

int
make (s)
SYM * s;
{
	DEP * dep;
	char * t;
	char * name;
	int update;

	if (s->done)
		return s->done;

	name = s->filename;
	if (dflag) {
		if (s->name == name)
			printf ("Making %s\n", name);
		else
			printf ("Making %s (file %s)\n", s->name, name);
	}
	s->done = 1;
	s->moddate = getmdate (name);

	for (dep = s->deplist ; dep != NULL ; dep = dep->next)		
		if (make (dep->symbol) > 1) {
prereq_error:
			if (dflag)
				printf ("Bypassing %s due to error building "
					"prerequisite\n", name);

			return s->done = 2;
		}

	if (s->type == T_DETAIL) {
		if (implicit (s, "", 0))
			return s->done = 2;

		for (dep = s->deplist ; dep != NULL ; dep = dep->next)
			if (dep->symbol->moddate > s->moddate) {
				update = 1;
				if (docmd0 (s, dep->action, name,
					    dep->symbol->filename))
					goto prereq_error;
			}
		return update;
	}

	update = 0;
	starttoken ();

	for (dep = s->deplist ; dep != NULL ; dep = dep->next) {
		if (dflag)
			printf ("%s time =%ld %s time =%ld\n",
			    dep->symbol->filename, dep->symbol->moddate,
			    name, s->moddate);

		if (dep->symbol->moddate > s->moddate) {
			update = 1;
			addtoken (' ');
			addstring (dep->symbol->filename);
		}
	}
	endtoken ();
	t = token;

	if (! update && s->moddate == 0) {
		update = 1;
		if (dflag)
			printf ("'%s' made due to non-existence\n", name);
	}

	if (s->action == NULL) {
		if (implicit (s, t, update)) {
had_err_2:
			free (t);
			return s->done = 2;
		}
	} else if (update && docmd0 (s, s->action, name, t))
		goto had_err_2;
	free (t);

	return update;
}


/*
 * Find the implicit rule to generate obj and execute it. Put the name of
 * obj up to '.' in prefix, and look for the rest in the dependency list
 * of .SUFFIXES. Find the file "prefix.foo" upon which obj depends, where
 * foo appears in the dependency list of suffixes after the suffix of obj.
 * Then make obj according to the rule from makeactions. If we can't find
 * any rules, use .DEFAULT, provided we're definite.
 */

int
implicit (obj, ques, definite)
SYM * obj;
char * ques;
int definite;
{
	register char * s;
	register DEP * d;
	char * prefix;
	char * rulename;
	char * at;
	char * percent;
	char * file;
	char * suffix;
	SYM * rule;
	SYM * subj;
	int		retval;

	if (dflag)
		printf ("Implicit %s (%s)\n", obj->name, ques);

	suffix = build_macros (obj->name, & at, & percent, & prefix);

	for (d = suffixes->deplist ; d != NULL ; d = d->next)
		if (Streq (suffix, d->symbol->name))
			break;

	if (d == NULL) {
		/*
		 * No match on suffix, try for a single-suffix rule instead.
		 */

		free (prefix);
		prefix = copy_token (obj->name);
		d = suffixes->deplist;
	} else
		d = d->next;

	/*
	 * Note that the suffix we matched might be the last one in the
	 * list, or the suffix list may be empty, in which case we should
	 * not run through this loop.
	 */

	while (d != NULL) {
		starttoken ();
		addstring (prefix);
		addstring (d->symbol->name);
		endtoken ();
		file = token;

		if ((s = fpath (file)) != file ||
		    (s = findbase (file)) != NULL) {
			free (file);
			file = s;
			if (dflag)
				printf ("Found implied target %s\n", file);
		}

		subj = NULL;

		if ((subj = dexists (file, obj->deplist)) != NULL ||
		    (rule = sexists (file)) != NULL || fexists (file)) {
			starttoken ();
			addstring (d->symbol->filename);
			addstring (suffix);
			endtoken ();
			rulename = token;

			if ((rule = sexists (rulename)) != NULL) {
				if (subj != NULL || (subj = sexists (file))) {
					free (file);
					file = subj->name;
				} else
					subj = lookup (file);

				(void) make (subj);
				if (definite || subj->moddate > obj->moddate)
					retval = docmd (obj, rule->action,
							at, ques, file,
							prefix, percent);
				else
					retval = 0;
				free (rulename);
				free (prefix);
				free (at);
				free (percent);
				return retval;
			}

			if (dflag)
				printf ("No matching suffix rule %s\n",
					rulename);
			free (rulename);
		}
		free (file);
		d = d->next;
	}

	if (definite)
		retval = defalt (obj, ques);
	else
		retval = 0;

	free (prefix);
	free (at);
	free (percent);
	return retval;
}


/*
 * Whine about usage and then quit
 */

NO_RETURN
Usage ()
{
	fprintf (stderr, "%s\n", usage);
	exit (1);
}

/*
 * Process switches
 */

int
switches (arg, next)
char	      *	arg;
char	      *	next;
{
	while (arg != NULL && * arg) {
		switch (* arg ++) {
		case 'd':
			dflag = 1;
			break;

		case 'e':
			eflag = 1;
			break;

		case 'i':
			iflag = 1;
			break;

		case 'n':
			nflag = 1;
			break;

		case 'q':
			qflag = 1;
			break;

		case 'r':
			rflag = 1;
			break;

		case 's':
			sflag = 1;
			break;

		case 't':
			tflag = 1;
			break;

		case 'k':
			kflag = 1;
			break;

		case 'S':
			kflag = 0;
			break;

		case 'p':
			pflag = 1;
			continue;

		case 'f':
			if (* arg == NULL) {
				if (next == NULL)
					Usage ();
				arg = next;
				next = NULL;
			}
			filep = listtoken (arg, filep);
			arg = NULL;
			continue;

		default:
			Usage ();
		}

		addtoken (arg [-1]);
	}

	return next == NULL;
}


/*
 * Process a command-line-style argument.
 */

int
process_argument (arg, next)
char	      *	arg;
char	      *	next;
{
	char	      *	value;

	if (* arg == '-')
		return switches (arg + 1, next) + 1;

	if ((value = strchr (arg, '=')) != NULL) {
		char	      *	s = arg;

		while (* s != ' ' && * s != '\t' && * s != '=')
			++ s;
		* s = 0;

		define (arg, value + 1, DEF_PROTECTED | DEF_EXPORT);
		return 1;
	}

	targets = listtoken (arg, targets);
	return 1;
}


int
main (argc, argv, envp)
int argc;
char * argv [];
char * envp [];
{
	char	* s, * value;
	char	* namesave;
	int c;
	int	len;
	SYM	* sp;
	DEP	* d;
	MACRO	* mp;
	int		update;

	time (& now);

	define ("MAKE", argv [0], DEF_NOALLOC);

	/*
	 * Before we process our command-line arguments, set up for recording
	 * all the switches we will see, and look for a definition of the
	 * MAKEFLAGS variable from the environment.
	 */

	starttoken ();

	while ((s = getenv ("MAKEFLAGS")) != NULL) {
		/*
		 * We are required to process these first by POSIX.2, and to
		 * take either SysV "-n -d" or BSD "nd" forms. Our generated
		 * one has the BSD format.
		 */

		if (* s != '-') {	/* Assume BSD format */
			switches (s, NULL);
			break;
		}

		value = mmalloc (strlen (s) + 1);
		strcpy (value, s);
		s = value;

		value = strtok (value, " \t\n");
		while (value != NULL) {
			char	      *	next;

			next = strtok (NULL, " \t\n");

			if (process_argument (value, next) > 1)
				next = strtok (NULL, " \t\n");

			value = next;
		}
		free (s);
		break;
	}

	++ argv;
	-- argc;

	while (argc > 0) {
		c = process_argument (argv [0], argv [1]);
		argc -= c;
		argv += c;
	}

	endtoken ();
	define ("MAKEFLAGS", token, DEF_PROTECTED | DEF_EXPORT);

	/*
	 * We must process the environment after processing both MAKEFLAGS
	 * and the command-line arguments so that we know what flags to
	 * provide for the definitions imported from the environment.
	 */

	while (* envp != NULL) {
		if ((value = strchr (* envp, '=')) != NULL &&
		    strchr (value, '$') == NULL) {
			s = * envp;
			while ((c = * s) != ' ' && c != '\t' && c != '=')
				++ s;

			len = s - * envp;
			namesave = mmalloc (len + 1);
			strncpy (namesave, * envp, len);
			namesave [len] = 0;

			if (eflag)
				define (namesave, value + 1, DEF_PROTECTED);
			else
				define (namesave, copy_token (value + 1),
					DEF_NORMAL);
		}
		++ envp;
	}

	/*
	 * The SHELL macro is handled specially; see POSIX.2 6.2.7.6 for
	 * details.
	 */

	if ((mp = get_macro ("SHELL")) != NULL) {
		mp->value = "/bin/sh";
		mp->flags = DEF_NOEXPORT | DEF_NOALLOC;
	}

	srcpath = mexists ("SRCPATH");
	suffixes = lookup (".SUFFIXES");

	if (! rflag)
		inlib (MACROFILE);

	deftarget = NULL;
	if (filep == NULL)
		inpath ("makefile", "Makefile", NULL);
	else {
		fd = stdin;
		do {
			input (strcmp (filep->value,
				       "-") == 0 ? NULL : filep->value);
			filep = filep->next;
		} while (filep != NULL);
	}

	if (! rflag) {
		inactionfile = 1;
		inlib (ACTIONFILE);
		inactionfile = 0;
	}

	if (sexists (".IGNORE") != NULL)
		++ iflag;
	if (sexists (".SILENT") != NULL)
		++ sflag;

	deflt = sexists (".DEFAULT");

	if (pflag) {
		if (macro != NULL) {
			printf ("Macros:\n");
			for (mp = macro; mp != NULL; mp = mp->next)
				printf ("%s =%s\n", mp->name, mp->value);
		}
		printf ("Rules:\n");
		for (sp = sym ; sp != NULL ; sp = sp->next) {
			if (sp->type != T_UNKNOWN) {
				printf ("%s:", sp->name);
				if (sp->type == T_DETAIL)
					putchar (':');
				for (d = sp->deplist ; d != NULL ; d = d->next)
					printf (" %s", d->symbol->name);
				printf ("\n");
				if (sp->action)
					printf ("\t%s\n", sp->action);
			}
		}
	}

	update = 0;

	if (targets != NULL) {
		do
			if ((make (lookup (targets->value))) == 2)
				die ("cannot make %s",
				     targets->value);
		while ((targets = targets->next) != NULL);
	} else if ((sp = sexists (deftarget)) != NULL)
		update = make (sexists (deftarget));
	else if (deftarget)
		die ("do not know how to make %s", deftarget);
	else
		die ("nothing to make");

	return qflag && update ? 1 : 0;
}
