/*
 * Special Getline for tabbld. Keeps comments in char *comment.
 *
 * char * getline(ifp, lineno) FILE *ifp; int *lineno;
 *
 * Function to get lines from an input file.
 * Returns the address of the line, or NULL for eof.
 *
 * lineno should usually be started at 1. lineno will
 * be incremented by the number of lines in the previous call.
 * Thus lineno will be the number of the line just gotten.
 *
 * # to end of line is ignored.
 * \ whitespace through end of line is ignored.
 * \n newline
 * \p #
 * \b backspace
 * \r carrage return
 * \f form feed
 * \t tab
 * \\ backslash
 * \ddd octal number
 * all other \ sequences are errors.
 */
#include <misc.h>

extern char *realloc();

static char *line = NULL;
char *comment = NULL;
static int oldline = 0;
static int i = 0, size = 0;	/* stuff for line */
static int j = 0, csize = 0;	/* stuff for comment */

/*
 * Add char to line.
 */
static void
addchr(c)
{
	while (i >= size)
		if (NULL == (line = realloc(line, size += 80)))
			fatal("Out of space");
			/* NODOC */
	line[i++] = c;
}

/*
 * Add char to comment.
 */
static void
addcom(c)
{
	while (j >= csize)
		if (NULL == (comment = realloc(comment, csize += 80)))
			fatal("Out of space");	/* NODOC */
	comment[j++] = c;
}

char *
getline(ifp, lineno)
FILE *ifp;
int *lineno;
{
	int c, octacc, octcnt;
	enum state { normal, incont, incomm, incommb, bsl, octdig } state;

	*lineno += oldline;
	for (state = normal, oldline = j = i = 0;;) {
		if (EOF == (c = fgetc(ifp))) {
			if (i)
				fprintf(stderr, 
					"line %d truncated at end\n", 
						*lineno + oldline);
			return (NULL);
		}

		switch (state) {
		case normal:
			switch (c) {
			case '\\':
				state = bsl;
				continue;

			case '#':
				state = incomm;
				continue;

			case '\n':
				oldline++;
				addcom(0);
				addchr(0);
				return (line);
			}

			addchr(c);
			continue;

		case incont:
			if ('\n' == c) {
				oldline++;
				state = normal;
			}
			continue;

		case incomm:
			switch (c) {
			case ' ':
			case '\t':
				continue;
			case '\n':
				state = normal;
				oldline++;
				addchr(0);
				addcom(0);
				return (line);
			}
			state = incommb;
			addcom(c);
			continue;

		case incommb:
			if ('\n' == c) {
				state = normal;
				oldline++;
				addchr(0);
				addcom(0);
				return (line);
			}
			addcom(c);
			continue;

		case bsl:
			switch (c) {
			case 'b':
				c = '\b';

			case '\\':
				break;

			case 'p':
				c = '#';
				break;

			case 'f':
				c = '\f';
				break;

			case 'a':
				c = '\a';
				break;

			case 'r':
				c = '\r';
				break;

			case 't':
				c = '\t';
				break;

			case 'n':
				c = '\n';
				break;

			case ' ':
			case '\t':
				state = incont;
				continue;

			case '\n':
				oldline++;
				state = normal;
				continue;

			default:
				if (('0' <= c) && ('7' >= c)) {
					state = octdig;
					octcnt = 1;
					octacc = c - '0';
					continue;
				}
				fprintf(stderr, 
					"%d: '%c' 0x%2x after \\\n",
						*lineno + oldline, c, c);
			}
			state = normal;
			addchr(c);
			continue;

		case octdig:
			if (('0' <= c) && ('7' >= c)) {
				octacc *= 8;
				octacc += c - '0';
				if (3 == ++octcnt) {
					state = normal;
					addchr(octacc);
				}
			}
			else {
				ungetc(c, ifp);
				state = normal;
				addchr(octacc);
			}
			continue;
		}
	}
	
}

#ifdef TEST
main()
{
	int line = 1;
	char *got;

	for (;;) {
		if (NULL == (got = getline(stdin, &line)))
			exit(0);

		if (*comment)
			printf("%d: %s #%s\n", line, got, comment);
		else
			printf("%d: %s\n", line, got);
	}
}
#endif