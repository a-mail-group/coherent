/*
 * PSfont.c
 * 6/8/00
 * Usage: PSfont [ -q ] [ -s ] [ infile [ outfile ] ]
 * Options:
 *	-q	Quiet: suppress warning messages.
 *	-s	Suppress "serverdict" header line.
 * Expand PostScript font from infile (default: stdin)
 * in IBM PC compressed binary format
 * to outfile (default: stdout) in downloadable ASCII PostScript.
 * Reference: "Supporting Downloadable PostScript Language Fonts",
 * Adobe Technical Note #5040, 3/31/92, section 3.3.
 */

/* Compatability. */
#if	defined(__STDC__)
#define	USE_PROTOS	1
#else	/* !defined(__STDC__) */
#define	USE_PROTOS	0
#endif	/* !defined(__STDC__) */

#if	USE_PROTOS
#define	__(args)	args
#else	/* !USE_PROTOS */
#define	__(args)	()
#endif	/* !USE_PROTOS */

#define	VERSION	"1.2"

#include <stdarg.h>
#include <stdio.h>

/* Manifest constants. */
#define	NBUF	32		/* buffer size for offset message */
#define	NDATA	32		/* binary data items per output line (64 in Adobe PCSEND.EXE) */
#define	HDRCMT	"%!PS-Adobe-1.0\n"
#define	HDRSRV	"%%BeginExitServer: 0\n"\
		"serverdict begin 0 exitserver\n"\
		"%%EndExitServer\n"

/* Definitions for binary segment header byte and type bytes. */
#define	HDRBYTE	128
#define	ISASCII	1
#define	ISBDATA	2
#define	ISEOF	3

/* Forward. */
extern	int	main	__((int argc, char *argv[]	));
extern	void	fatal	__((char *fmt, ...		));
extern	long	getlong	__((void			));
extern	void	process	__((void			));
extern	void	usage	__((void			));
extern	void	warning	__((char *fmt, ...		));
extern	int	xgetc	__((void			));
extern	FILE	*xopen	__((char *name, char *mode	));

/* Global. */
FILE	*ifp = stdin;				/* input FILE		*/
FILE	*ofp = stdout;				/* output FILE		*/
int	qflag;					/* no warnings		*/
int	sflag;					/* no serverdict line	*/

#if	USE_PROTOS
int
main(int argc, char *argv[])
#else	/* !USE_PROTOS */
int
main(argc, argv) int argc; char *argv[];
#endif	/* !USE_PROTOS */
{
	register char *s;

	while (argc > 1 && argv[1][0] == '-') {
		for (s = &argv[1][1]; *s != '\0'; ++s) {
			switch(*s) {
			case 'q':	++qflag;	break;
			case 's':	++sflag;	break;
			case 'V':
				fprintf(stderr, "PSfont: V" VERSION "\n");
				break;
			default:
				usage();
				break;
			}
		}
		--argc;
		++argv;
	}
	if (argc > 3)
		usage();
	if (argc > 2)
		ofp = xopen(argv[2], "w");
	if (argc > 1)
		ifp = xopen(argv[1], "rb");
	process();
	if (ifp != stdin)
		fclose(ifp);
	if (ofp != stdout)
		fclose(ofp);
	exit(0);
}

/* Print a fatal error message and die. */
#if	USE_PROTOS
void
fatal(char *fmt, ...)
#else	/* !USE_PROTOS */
/* VARARGS */
void
fatal(fmt) char *fmt;
#endif	/* !USE_PROTOS */
{
	va_list	args;

	va_start(args, fmt);
	fprintf(stderr, "PSfont: ");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
	exit(1);
}

/* Read a four-byte long in Intel byte order. */
#if	USE_PROTOS
long
getlong(void)
#else	/* !USE_PROTOS */
long
getlong()
#endif	/* !USE_PROTOS */
{
	register long l;

	l = xgetc();
	l |= (xgetc() << 8);
	l |= (xgetc() << 16);
	l |= (xgetc() << 24);
	return l;
}

/*
 * Process a compressed binary font file.
 * The input file is a series of records, each starting with HDRBYTE (0x80).
 * The next byte of each record is either ISASCII, ISBDATA or ISEOF.
 * ISASCII records have a 4-byte length followed by length ASCII bytes.
 * ISBDATA records have a 4-byte length followed by length binary bytes.
 * ISEOF indicates the end of the input data, it should be followed
 * by EOF but in practice there are sometimes extraneous trailing bytes.
 * The output file contains a header comment,
 * an "exitserver" line if not -s,
 * and the input data, with binary converted to hex ASCII.
 * ASCII CR gets translated to NL for the benefit of other programs
 * which may want to edit the PS output.
 */
#if	USE_PROTOS
void
process(void)
#else	/* !USE_PROTOS */
void
process()
#endif	/* !USE_PROTOS */
{
	register int c, type, i, count;
	register long length;
	char buf[NBUF];

	fputs(HDRCMT, ofp);		/* write header comment */
	if (!sflag)
		fputs(HDRSRV, ofp);	/* cf. Red Book, 2nd Ed., 3.7.7, pp. 70 ff. */
	for (;;) {
		if ((c = xgetc()) != HDRBYTE) {
			if (ifp != stdin)
				sprintf(buf, "offset %ld: ", ftell(ifp));
			else
				buf[0] = '\0';
			fatal("%sbad header byte, expected 0x%X, read 0x%X",
				buf, HDRBYTE, c);
		}
		if ((type = xgetc()) == ISEOF)
			break;
		else if (type != ISASCII && type != ISBDATA)
			fatal("unexpected type byte 0x%X", type);
		length = getlong();
		if (type == ISASCII) {
			/* Copy ASCII data.  Convert CR to NL. */
			while (length--) {
				if ((c = xgetc()) == '\r')
					c = '\n';
				putc(c, ofp);
			}
		} else {
			/* Expand binary data. */
			for (; length > 0; length -= count) {
				count = (length > NDATA) ? NDATA : length;
				for (i = 0; i < count; i++)
					fprintf(ofp, "%02X", xgetc());
				putc('\n', ofp);
			}
		}
	}
	putc('\n', ofp);
	if (getc(ifp) != EOF)
		warning("extra data after EOF indicator in input file ignored");
}

/* Print a fatal usage message and die. */
#if	USE_PROTOS
void
usage(void)
#else	/* !USE_PROTOS */
void
usage()
#endif	/* !USE_PROTOS */
{
	fprintf(stderr,
		"Usage: PSfont [ -q ] [ -s ] [ infile [ outfile ] ]\n"
		"Options:\n"
		"\t-q\tQuiet: suppress warning messages.\n"
		"\t-s\tSuppress \"serverdict ... exitserver\" line in header\n"
		);
	exit(1);
}

/* Print a nonfatal warning message. */
#if	USE_PROTOS
void
warning(char *fmt, ...)
#else	/* !USE_PROTOS */
/* VARARGS */
void
warning(fmt) char *fmt;
#endif	/* !USE_PROTOS */
{
	va_list	args;

	if (qflag)
		return;
	va_start(args, fmt);
	fprintf(stderr, "PSfont: warning: ");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
}

/* Read a character and return it, die on EOF. */
#if	USE_PROTOS
int
xgetc(void)
#else	/* !USE_PROTOS */
int
xgetc()
#endif	/* !USE_PROTOS */
{
	register int c;

	if ((c = getc(ifp)) == EOF)
		fatal("unexpected EOF");
	return c & 0xFF;
}

/* Open a file, die on failure. */
#if	USE_PROTOS
FILE *
xopen(char *name, char *mode)
#else	/* !USE_PROTOS */
FILE *
xopen(name, mode) register char *name, *mode;
#endif	/* !USE_PROTOS */
{
	register FILE *fp;

	if ((fp = fopen(name, mode)) == NULL)
		fatal("cannot open file \"%s\"", name);
	return fp;
}

/* end of PSfont.c */

