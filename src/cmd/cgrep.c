/*
 * To compile use
 * cc cgrep.c -lmisc
 * This uses libmisc.a and the misc.h header from libmisc.a.Z
 */

/*
 * cgrep is egrep for c programs.
 *
 * cgrep checks all c identifiers (cgrep considers if, etc to be
 * identifiers) against an egrep type pattern for a full match.
 * 
 * 	cgrep tmp *.c
 * will find tmp as an identifier, but not tmpname, or tmp in a string or
 * comment.
 * 	cgrep "x|abc|d" *.c
 * will find x, ab, or d. Note these are egrep style patterns with a
 * surrounding ^( )$ Thus "reg*" will not match register but "reg.*"
 * will.
 *
 * cgrep accumulates names with included . and -> for testing against the
 * egrep pattern. Thus you can look for ptr->val with
 * 	cgrep "ptr->val" x.c
 * This will find ptr->val even if it contains spaces, comments or is
 * spread across lines. If it is spread across lines it will be reported
 * on the line containing the first identifier with a -A option and the
 * line containing the last identifier otherwise.
 *
 * For structure.member use
 * 	cgrep "structure\.member"
 * because . has the egrep meaning any character. Do not include spaces
 * in any pattern. Only identifiers and . or -> between identifiers are
 * included in the tokens checked for pattern match.
 *
 * Options.
 *
 * -l lists the files with hits not the lines.
 * -n puts a line number on found lines.
 * -A builds a tmp file and calls me to process the file with the tmp file
 *    as an "error" list like the -A option of cc. Each line of this list
 *    shows the found pattern and where multiple patterns are found on a
 *    line there are multiple lines, just like cc -A showing multiple
 *    errors on a line. This is to allow emacs scripts to make systematic
 * -c takes no pattern and prints all comments.
 * -s takes no pattern and prints all strings.
 */
#include <ctype.h>
#include <stdio.h>
#include <misc.h>
#include <regexp.h>

struct token {
	int start;	/* token index on buff */
	int atline;	/* line number where token spotted */
};

/* cgrep has no fixed size arrays only growable buffers */
static char *buff = NULL;	/* accumulate stuff */
static int buffLen;		/* buffer length */

static struct token *tokens = NULL; /* token array */
static int tokenLen;		/* size of token array */

static char *line;		/* expandable input line */
static char lineLen;		/* current length of input line */

static int lswitch;		/* list files found */
static int aswitch;		/* call emacs with line list */
static int nswitch;		/* print line number */
static int sswitch;		/* print all strings */
static int cswitch;		/* print all comments */

static regexp *pat;		/* a compiled regular expression */

static char *filen = NULL;	/* the file currently being processed */
static char *tname = NULL;	/* temp file name */
static FILE *tfp;		/* tmp file pointer */

static int lineno;		/* current line number */
static int marked;		/* 1 if pattern found on line. */

/*
 * Character types table
 * for the ASCII character set modified to see _ as alpha.
 * _ctype[0] is for EOF, the rest if indexed
 * by the ascii values of the characters.
 */
static unsigned char _ctype[] = {
	0,	/* EOF */
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _S|_C, _S|_C, _S|_C, _S|_C, _S|_C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_S|_X, _P, _P, _P, _P, _P, _P, _P,
	_P, _P, _P, _P, _P, _P, _P, _P,
	_D, _D, _D, _D, _D, _D, _D, _D,
	_D, _D, _P, _P, _P, _P, _P, _P,
	_P, _U, _U, _U, _U, _U, _U, _U,
	_U, _U, _U, _U, _U, _U, _U, _U,
	_U, _U, _U, _U, _U, _U, _U, _U,
	_U, _U, _U, _P, _P, _P, _P, _L,
	_P, _L, _L, _L, _L, _L, _L, _L,
	_L, _L, _L, _L, _L, _L, _L, _L,
	_L, _L, _L, _L, _L, _L, _L, _L,
	_L, _L, _L, _P, _P, _P, _P, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C
};

/*
 * Report errors for public domain regexp package.
 */
void
regerror(s)
char *s;
{
	fatal("cgrep: pattern error %s\n", s);
}

/*
 * Pattern found with -A mode. It is assumed that users of
 * this mode will want to know about all hits on a line.
 */
static void
emacsLine(found, atline)
char *found;
{
	extern char *tempnam();

	/* if tmp file not opened open it. */
	if ((NULL == tname) &&
	    ((NULL == (tname = tempnam(NULL, "cgr"))) ||
             (NULL == (tfp = fopen(tname, "w")))))
		fatal("cgrep: Cannot open tmp file");
	fprintf(tfp, "%d: %s: found '%s'\n", atline, filen, found);
}

/*
 * check that token table has enough entries.
 */
static void
tokenRoom(buf, has, needs)
struct token **buf;
int *has;
{
	extern char *realloc();

	while (needs >= *has)
		if (NULL == (*buf = realloc(*buf,
					sizeof(struct token) * (*has += 10))))
			fatal("cgrep: out of space");
}

/*
 * check that a buffer has enough space and get more if it needs it.
 */
static void
bufRoom(buf, has, needs)
char **buf;
int *has;
{
	extern char *realloc();

	while (needs >= *has)
		if (NULL == (*buf = realloc(*buf, *has += 512)))
			fatal("cgrep: out of space");
}

/* word processing states */
#define WORD	1	/* some c identifier */
#define DOT	2	/* . or -> actually */
#define OTHER	0	/* anything else */

/*
 * When we get a word, dot, arrow or other we come here.
 *
 * cgrep accumulates things like ptr->memb.x in buff.
 * as these are accumulated it is nessisary to match buff
 * against the regular expression one slice at a time
 * that is when the x in ptr->memb.x was found cgrep would check
 * ptr->memb.x then memb.x then x
 * against the pattern. Thus memb.x matches ptr->memb.x
 */
static void
gota(got, what)
register int got;	/* what we have */
char *what;		/* the string we have or NULL */
{
	static int tokenCt;	    /* number of tokens */
	static int state = OTHER;   /* current word processing state state */
	static int wlen, blen;	    /* strlen(what) and strlen(buff) + 1 */
	int i;

	if (sswitch || cswitch)
		return;

	switch (got) {
	case OTHER:
		state = OTHER;
		return;
	case WORD:
		wlen = strlen(what);
		switch (state) {
		case OTHER:
		case WORD:
			/* store start and line number of token */
			tokens[0].start = 0;
			tokens[0].atline = lineno;
			tokenCt = 1;

			/* store token */
			blen = wlen;
			strcpy(buff, what);
			break;
		case DOT:
			/* store start and line number of token */
			tokenRoom(&tokens, &tokenLen, tokenCt);
			tokens[tokenCt].start = blen;
			tokens[tokenCt++].atline = lineno;

			/* store token */
			bufRoom(&buff, &buffLen, blen += wlen);
			strcat(buff, what);
		}
		state = WORD;

		/* Check the accumulated token for matches */
		for (i = 0; i < tokenCt; i++) {
			char *p;

			if (regexec(pat, p = buff + tokens[i].start)) {
				if (aswitch)
					emacsLine(p, tokens[i].atline);
				else {
					marked = 1;
					break;
				}
			}
		}
		return;
	case DOT:
		if (WORD == state) {
			state = got;
			bufRoom(&buff, &buffLen, blen += strlen(what));
			strcat(buff, what);
			return;
		}
		state = OTHER;
		return;
	}
}

/*
 * call emacs with file and tmpfile.
 */
static void
callEmacs()
{
#ifdef GEMDOS
#include <path.h>
	extern  char *path(), *getenv();
	extern char **environ;
	static char* cmda[5] = { NULL, "-e", NULL, NULL, NULL };
#endif
	int quit;

	fclose(tfp);
#ifdef MSDOS
	sprintf(line, "-e %s %s", tname, filen);
	if (0x7f == (quit = execall("me", line)))
#endif
#ifdef COHERENT
	sprintf(line, "me -e %s %s ", tname, filen);
	if (0x7f == (quit = system(line)))
#endif
#ifdef GEMDOS
	cmda[2] = tname;
	cmda[3] = filen;
	if ((NULL == cmda[0]) &&
	   (NULL == (cmda[0] = path(getenv("PATH"), "me.tos", 1))))
		fatal("cgrep: Cannot locate me.tos\n");
	else if ((quit = execve(cmda[0], cmda, environ)) < 0)
#endif
		fatal("cgrep: cannot execute 'me'");
	unlink(tname);
	free(tname);
	tname = NULL;
	if (quit)
		exit(0);
}

/*
 * print special hit.
 */
static void
printx(s)
char *s;
{
	if (aswitch)
		emacsLine(s, lineno);
	else {
		if (NULL != filen)
			printf("%s: ", filen);
		if (nswitch)
			printf("%4d: ", lineno);
		printf("%s\n", s);
	}
}

/* file processing states */
#define START	0
#define SLASH	1		/* slash encountered in normal state */
#define COMMENT	2		/* in comment */
#define STAR	3		/* * in comment */
#define BSL	4		/* back slash */
#define DQUOTE	5		/* double quote */
#define SQUOTE	6		/* single quote */
#define TOKEN	7		/* c word */
#define MINUS	8		/* - maybe -> */

static void
process()
{
	register int c, state, i;
	int pstate;
	char *w;
	FILE *ifp;

	if (NULL == filen)
		ifp = stdin;
	else if (NULL == (ifp = fopen(filen, "r"))) {
		fprintf(stderr, "cgrep: warning cannot open %s\n", filen);
		return;
	}

	lineno = 1;
	i = marked = 0;
	for (state = START; ; ) {
		line[i] = '\0';
		c = fgetc(ifp);

		switch (state) {
		case MINUS:
			if ('>' == c) {
				gota(DOT, "->");
				state = START;
				break;
			}
			goto isstart;
		case TOKEN:
			if (isalnum(c))
				break;
			gota(WORD, w);
isstart:		state = START;
		case START:
			switch (c) {
			case '.':
				gota(DOT, ".");
				break;
			case '-':
				state = MINUS;
				break;
			case '/':
				state = SLASH;
				w = line + i;
				break;
			case '\\':
				pstate = state;
				state = BSL;
				break;
			case '"':
				w = line + i;
				state = DQUOTE;
				break;
			case '\'':
				state = SQUOTE;
				break;
			default:
				if (isalpha(c)) {
					w = line + i;
					state = TOKEN;
				}
				else if (!isspace(c))
					gota(OTHER, NULL);
			}
			break;
		case SLASH:
			if ('*' != c)
				goto isstart;
			state = COMMENT;
			break;
		case STAR:
			if ('/' == c) {
				if (cswitch) { /* report comment */
					line[i++] = c;
					if (lineLen == i)
						bufRoom(&line, &lineLen, i);
					line[i--] = '\0';
					printx(w);
				}
				state = START;
				break;
			}
			state = COMMENT;
		case COMMENT:
			if ('*' == c)
				state = STAR;
			break;
		case BSL:
			state = pstate;
			break;
		case DQUOTE:
			switch (c) {
			case '"':
			case '\n':
				state = START;
				if (sswitch)
					printx(w + 1);
				else
					gota(OTHER, NULL);
				break;
			case '\\':
				pstate = state;
				state  = BSL;
				break;
			}
			break;
		case SQUOTE:
			switch (c) {
			case '\'':
			case '\n':
				gota(OTHER, NULL);
				state = START;
				break;
			case '\\':
				pstate = state;
				state  = BSL;
				break;
			}
			break;
		}
		if (('\n' != c) && (EOF != c)) {
			line[i++] = c;
			if (lineLen == i)	/* make line big enough */
				bufRoom(&line, &lineLen, i);
		}
		else {	/* end of line */
			if (cswitch && (COMMENT == state)) {
				printx(w);
				w = line;
			}
			if (marked) {
				marked = 0;			
				if (!lswitch)
					printx(line);
				else {
					printf("%s\n", filen);
					break;
				}
			}
			lineno++;
			i = 0;
			if (EOF == c)
				break;
		}
	}

	fclose(ifp);
	if (NULL != tname) /* tmp file opened for -A option */
		callEmacs();
}

main(argc, argv)
int argc;
register char **argv;
{
	register char c;
	int errsw = 0;
	static char msg[] = "cgrep [-clnsA] [pattern] filename ...";
	char *p, *q;
	extern int optind;

	if (1 == argc)
		usage(msg);

	while (EOF != (c = getopt(argc, argv, "cslnA?"))) {
		switch (c) {
		case 'c':
			cswitch = 1;	/* comments only */
			break;
		case 's':
			sswitch = 1;	/* strings only */
			break;
		case 'l':
			lswitch = 1;	/* print filenames only */
			break;
		case 'n':
			nswitch = 1;	/* print line numbers */
			break;
		case 'A':
			aswitch = 1;	/* interact with emacs */
			break;
		default:
			errsw = 1;
		}
	}

	if (errsw)
		usage(msg);

	if (!sswitch && !cswitch) {
		if (optind == argc)
			usage(msg);

		/* inclose pattern in ^(  )$ to force full match */
		p = alloc(5 + strlen(q = argv[optind++]));
		sprintf(p, "^(%s)$", q);
		if (NULL == (pat = regcomp(p)))
			fatal("\ncgrep: Illegal pattern\n");

		/* doing these here saves 2 calls per identifier */
		bufRoom(&buff, &buffLen, 1);	  /* get token buffer started */
		tokenRoom(&tokens, &tokenLen, 1); /* get token list started */
	}

	bufRoom(&line, &lineLen, 1);	/* get input line started */

	if (optind == argc) {
		if (aswitch || lswitch)
			fatal("cgrep: -A and -l require a filename");
		process();
	}
	else {
		while (optind < argc) {
			filen = argv[optind++];
			process();
		}
	}
	exit(0);
}
