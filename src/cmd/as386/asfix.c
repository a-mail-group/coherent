/*
 * Convert old as format to new asm format.
 */
#include <stdio.h>
#include <ctype.h>
#define MAXL	100

#define START	0
#define INTOKEN	1
#define BSL	2
#define DQUOTE	3
#define SQUOTE	4
#define WHITE	5
#define ACOMM	6

typedef struct locsym locsym;
struct locsym {	/* one for local symbol */
	int number;	/* old local symbol number */
	int def;	/* 0 if undefined 1 if defined */
};
locsym locs[MAXL];
int top;		/* highest defined symbol */
int lineno = 1;		/* line number */
int rc;			/* return code */
int octdig;		/* octal digit ct */

/*
 * Report an error on stderr.
 */
error(s)
char *s;
{
	fprintf(stderr, "%d: %s\n", lineno, s);
	rc = 1;
}
/*
 * Find a local symbol in the right defined state.
 */
locsym *
scanLoc(n, at)
{
	register locsym *s;

	for (s = locs + (top - 1); s >= locs; s--)
		if (n == s->number && at == s->def)
			return (s);
	return(NULL);
}

/*
 * A token has been read process it looking for local symbols.
 * These are in the form [0-9]+[:bf]
 * Where [:] requires a pstate of START and [bf] a pstate of white
 */
static void
crunch(token, pstate)
char *token;
{
	register locsym *s;
	register i, c;
	register char *p;
	int sw;

	for (sw = i = 0, p = token; (c = *p++) && isdigit(c); ) {
		sw = 1;
		i = (i * 10) + (c - '0');
	}
	if (!sw || *p)	/* mark nomatch */
		c = 0;

	s = NULL;
	switch (c) {
	case ':':
		if (START != pstate)
			error(": local symbol after start of line");
		/* 
		 * if first use of this symbol build table entry.
		 * else mark it defined.
		 */
		if (NULL == (s = scanLoc(i, 0))) {
			s = locs + top++;
			s->number = i;
			break;
		}
		s->def = 1;
		break;
	case 'b':
		if (WHITE != pstate)
			error("b local symbol after start of line");
		if (NULL == (s = scanLoc(i, 1)))
			error("Unresolved local symbol");
		break;
	case 'f':
		if (WHITE != pstate)
			error("f local symbol after start of line");
		/*
		 * If first use of this symbol build it
		 * else use it.
		 */
		if (NULL == (s = scanLoc(i, 0))) {
			s = locs + top++;
			s->number = i;
			s->def = 0;
			break;
		}
		break;
	}
	if (NULL == s)
		printf("%s", token);
	else
		printf("?%03d", (int)(s - locs));
}

/*
 * Read input to output finding tokens.
 */
main()
{
	register c, state, pstate;
	char *tp, token[20];

	for (state = pstate = START; EOF != (c = getchar()); ) {
		switch (state) {
		case START:
			if (isspace(c)) {
				state = WHITE;
				break;
			}
		case WHITE:
			if (isalnum(c)) {
				pstate = state;
				state = INTOKEN;
				tp = token;
				*tp++ = c;
				continue;
			}
			switch (c) {
			case '"':
				state = DQUOTE;
				break;
			case '\'':
				state = SQUOTE;
				octdig = 0;
				break;
			case '/':
				state = ACOMM;
			}
		case ACOMM:
			break;
		case INTOKEN:
			if ((':' == c) || isalnum(c)) {
				*tp++ = c;
				continue;
			}
			*tp = '\0';
			crunch(token, pstate);
			state = WHITE;
			break;
		case DQUOTE:
			switch (c) {
			case '\'':
				pstate = state;
				state = BSL;
				break;
			case '"':
				state = WHITE;
				break;
			}
			break;
		case BSL:
			if (SQUOTE != (state = pstate))
				break;
			if ('\\' == c) {
				putchar(c);
				c = '\'';
				state = WHITE;
				break;
			}
			octdig = 2;	/* fall through */

		case SQUOTE:
			if ('\\' == c) {
				pstate = state;
				state = BSL;
				break;
			}
			if (isdigit(c) && octdig--)
				break;
			putchar(c);	/* turn 'x to 'x' */
			c = '\'';
			state = WHITE;
		}
		putchar(c);
		if ('\n' == c) {
			lineno++;
			state = START;
		}
	}
	exit(rc);
}
