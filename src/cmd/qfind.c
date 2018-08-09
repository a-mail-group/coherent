/*
 * qfind.c
 * 5/9/94
 * Find files with given name in filesystem using file database.
 * Usage: qfind [ -adp ] name ...
 * 	  qfind -b[v] [ -sname ] ...
 * Options:
 *	-a	All: search for files or directories (default: files only).
 *	-b	Build file database.
 *	-d	Search for directories only.
 *	-p	Partial name matching.
 *	-sname	Suppress name (and its subdirectories) when building database.
 *	-v	Verbose information.
 * Run as root when using -b to find everything.
 * Uses find, sed, sort.
 * MS-DOS version uses cohfind and sort.
 * Does not ignore SIG_INT, so "qfind -b&" aborts if <Ctrl-C> typed.
 */

#include <stdio.h>
#if	COHERENT
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define	PATHSEP	'/'
#endif
#if	MSDOS
#define	UCHAR_MAX	255
#define	SEEK_SET	0
#define	SEEK_END	2
#define	PATHSEP	'\\'
extern	char	*malloc();
extern	char	*realloc();
extern	char	*strchr();
#endif

extern	char	*mktemp();

#define	VERSION	"2.0"
#define	MINSEEK	512			/* binary search threshold */
#define	NBUF	512			/* buffer size		*/
#define	NCHARS	(UCHAR_MAX+1)		/* # of first characters */
#define	SED_D	"s/\\(.*\\)\\/\\(.*\\)/\\2\\/ \\1/"
#define	SED_F	"s/\\(.*\\)\\/\\(.*\\)/\\2 \\1/"
#if	COHERENT
#define	QFFILES	"/usr/adm/qffiles"	/* database filename	*/
#define	QFNEW	"/usr/adm/qffiles.new"	/* new database filename */
#define	QFTMP	"/tmp/qfXXXXXX"		/* tmpname prototype	*/
#endif
#if	MSDOS
#define	QFFILES	"\\usr\\adm\\qffiles"	/* database filename	*/
#define	QFNEW	"\\usr\\adm\\qffiles.new"	/* new database filename */
#define	QFTMP	"\\tmp\\qfXXXXXX"		/* tmpname prototype	*/
#endif

/* Forward. */
int	build();
void	fatal();
void	fpseek();
void	nonfatal();
int	qfind();
int	qseek();
void	suppress();
void	sys();
void	usage();

/* Globals. */
int	aflag;				/* look for all		*/
int	bflag;				/* build QFFILES	*/
unsigned char	buf[NBUF];		/* command buffer	*/
int	dflag;				/* look for directories	*/
FILE	*ifp;				/* input FILE		*/
int	pflag;				/* partial match	*/
char	*sed_cmd = NULL;		/* sed command string	*/
long	seektab[NCHARS];		/* seek table		*/
int	sflag;				/* suppress		*/
char	tmpname[] = QFTMP;		/* temporary filename	*/
#if	MSDOS
char	tmpname2[] = QFTMP;		/* temporary filename	*/
#endif
int	vflag;				/* verbose information	*/

main(argc, argv) int argc; char *argv[];
{
	register char *s;
	register int status;

	/* Process options. */
	while (argc > 1 && argv[1][0] == '-') {
		for (s = &argv[1][1]; *s; s++) {
			switch(*s) {
			case 'a':	++aflag;	break;
			case 'b':	++bflag;	break;
			case 'd':	++dflag;	break;
			case 'p':	++pflag;	break;
			case 's':
				++sflag;
				suppress(&argv[1][2]);
				s += strlen(s) - 1;
				break;
			case 'v':	++vflag;	break;
			case 'V':
				fprintf(stderr, "qfind: V%s\n", VERSION);
				break;
			case '?':
			default:	usage();
			}
		}
		--argc;
		++argv;
	}

	/* Arg sanity check. */
	if (bflag) {
		if (argc != 1)
			usage();
		if (aflag)
			nonfatal("-a option ignored with -b");
		if (dflag)
			nonfatal("-d option ignored with -b");
		if (pflag)
			nonfatal("-p option ignored with -b");
	} else {
		if (argc == 1)
			usage();
		if (vflag)
			nonfatal("-v option ignored without -b");
		if (sflag)
			nonfatal("-s option ignored without -b");
	}

	/* Build new database. */
	if (bflag)
		exit(build());

	/* Find given names in existing database. */
	if ((ifp = fopen(QFFILES, "rb")) == NULL)
		fatal("cannot open \"%s\"", QFFILES);
	else if (fread(seektab, sizeof(seektab), 1, ifp) != 1)
		fatal("seek buffer read error");
	for (status = 0; *++argv != NULL; )
		status |= qfind(*argv);
	if (fclose(ifp) == EOF)
		fatal("cannot close \"%s\"", QFFILES);
	exit(status);
}

/*
 * Build the file and directory database.
 * The database consists of a seek pointer table
 * followed by a sorted list of files and directories.
 * seektab[c] gives the seek to the first line in the file starting with c.
 * The sorted list contains "file /dir1/dir2" for each file /dir1/dir2/file
 * and "dir3/ /dir1/dir2" for each directory /dir1/dir2/dir3.
 */
int
build()
{
	register FILE *fp;
	register int nfiles;
	int last;
	long lastseek;

	if (mktemp(tmpname) == NULL)
		fatal("cannot make temporary file name");
#if	MSDOS
	if (mktemp(tmpname2) == NULL)
		fatal("cannot make temporary file name");
#endif

#if	COHERENT
	/* Generate "file /dir1/dir2" for each file /dir1/dir2/file. */
	sys("find / ! -type d -print | sed -e '%s%s' >%s",
		(sed_cmd == NULL) ? "" : sed_cmd, SED_F, tmpname);

	/* Append "dir3/ /dir1/dir2" for each directory /dir1/dir2/dir3. */
	sys("find / -type d -print | sed -e '%s%s' >>%s",
		(sed_cmd == NULL) ? "" : sed_cmd, SED_D, tmpname);
#endif
#if	MSDOS
	/* The MSDOS cohfind command used here is a quick and dirty hack. */
	sys("cohfind -o %s -b", tmpname);
#endif

	/* Create data file containing an empty seek table. */
	if ((fp = fopen(QFNEW, "wb")) == NULL)
		fatal("cannot open \"%s\"", QFNEW);
	else if (fwrite(seektab, sizeof(seektab), 1, fp) != 1)
		fatal("write error on \"%s\"", QFNEW);
	else if (fclose(fp) == EOF)
		fatal("cannot close \"%s\"", QFNEW);

	/* Sort tempfile and append to new data file. */
#if	COHERENT
	sys("sort %s >>%s", tmpname, QFNEW);
#endif
#if	MSDOS
 	sys("sort -o %s %s", tmpname2, tmpname);
	/* Append tmpname2 to QFNEW. */
	{
		register int c;

		if ((fp = fopen(QFNEW, "ab")) == NULL)
			fatal("cannot open \"%s\"", QFNEW);
		if ((ifp = fopen(tmpname2, "rb")) == NULL)
			fatal("cannot open \"%s\"", tmpname2);
		while ((c = getc(ifp)) != EOF) {
			/* sort put in CRs, take them out here. */
			if (c != '\r')
				putc(c, fp);
		}
		fclose(fp);
		fclose(ifp);
	}
	if (unlink(tmpname2) == -1)
		fatal("cannot unlink temp file");
#endif
	if (unlink(tmpname) == -1)
		fatal("cannot unlink temp file");

	/* Initialize the seek table. */
	lastseek = (long)sizeof(seektab);
	last = -1;
	if ((fp = fopen(QFNEW, "rwb")) == NULL)
		fatal("cannot open \"%s\"", QFNEW);
	fpseek(fp, lastseek, SEEK_SET);
	for (nfiles = 0; fgets(buf, sizeof(buf)-1, fp) != NULL; ++nfiles) {
		if (*buf != last) {
			last = *buf;
			seektab[last] = lastseek;
		}
		lastseek += (long)strlen(buf);
	}
	if (vflag)
		printf("%d files\n%ld bytes\n", nfiles, lastseek);

	/* Rewrite the seek table in the data file. */
	fpseek(fp, 0L, SEEK_SET);
	if (fwrite(seektab, sizeof(seektab), 1, fp) != 1)
		fatal("write error on \"%s\"", QFNEW);
	else if (fclose(fp) == EOF)
		fatal("cannot close \"%s\"", QFNEW);

	/* Remove old if it exists, rename new accordingly. */
	unlink(QFFILES);		/* may not exist */
#if	COHERENT
	if (link(QFNEW, QFFILES) == -1)
		fatal("cannot link \"%s\" to \"%s\"", QFNEW, QFFILES);
	else if (unlink(QFNEW))
		fatal("cannot unlink \"%s\"", QFNEW);
#endif
#if	MSDOS
	if (rename(QFFILES, QFNEW) == 0)
		fatal("cannot rename \"%s\"", QFNEW);
#endif
	return 0;
}

/*
 * Cry and die.
 */
/* VARARGS */
void
fatal(s) char *s;
{
	fprintf(stderr, "qfind: %r\n", &s);
	unlink(tmpname);
	if (bflag)
		unlink(QFNEW);
	exit(1);
}

/*
 * Seek on fp, die on failure.
 */
void
fpseek(fp, where, how) FILE *fp; long where; int how;
{
	if (fseek(fp, where, how) == -1)
		fatal("seek failed");
}

/*
 * Cry but don't die.
 */
/* VARARGS */
void
nonfatal(s) char *s;
{
	fprintf(stderr, "qfind: %r\n", &s);
}

/*
 * Find s.
 */
int
qfind(s) char *s;
{
	register int val;
	register char *cp;
	int len, notfound, isdir;

	/* Seek to appropriate place in data file to begin linear search. */
	notfound = 1;
	if (!qseek(s)) {
		fprintf(stderr, "qfind: %s: not found\n", s);
		return notfound;
	}

	/* Read lines and look for matches. */
	len = strlen(s);
	while (fgets(buf, sizeof(buf)-1, ifp) != NULL) {

		if ((val = strncmp(buf, s, len)) < 0)
			continue;		/* not there yet */
		else if (val > 0)
			break;			/* past it */

		/* Possible match. */
		if ((cp = strchr(buf, ' ')) == NULL)
			fatal("strchr botch, buf=%s", buf);
		isdir = *(cp - 1) == PATHSEP;	/* iff buf contains dir name */
		if ((isdir && !dflag && !aflag)	/* directories not wanted */
		 || (!isdir && dflag))		/* files not wanted */
			continue;
		if (!pflag && (buf[len] != ((isdir) ? PATHSEP : ' ')))
			continue;		/* not exact match */

		/* Match, print it out. */
		notfound = 0;
		buf[strlen(buf)-1] = '\0';	/* zap trailing newline */
		*cp++ = '\0';			/* NUL-terminate filename */
		printf("%s%c%s\n", cp, PATHSEP, buf);	/* print the match */
	}

	/* Return appropriate status. */
	if (notfound)
		fprintf(stderr, "qfind: %s: not found\n", s);
	return notfound;
}

/*
 * Seek in data file to someplace preceding the desired key.
 * Use binary search to get close, for efficiency.
 * Return 0 on failure.
 */
int
qseek(key) char *key;
{
	register int i, len;
	long new, min, max;

	i = *key;
	if ((min = seektab[i]) == 0L)		/* lower bound for search */
		return 0;		/* no entries with right first char */

	/* Binary search. */
	for (++i; i < NCHARS; ++i) {
		if (seektab[i] != 0L) {
			max = seektab[i];	/* upper bound for search */
			break;
		}
	}
	if (i == NCHARS) {
		fpseek(ifp, 0L, SEEK_END);
		max = ftell(ifp);
	}
	len = strlen(key);
	while (max - min > MINSEEK) {
		new = (min + max) / 2;
		fpseek(ifp, new, SEEK_SET);	/* seek to midpoint of range */
		while ((i = getc(ifp)) != EOF) {
			++new;
			if (i == '\n')
				break;		/* scan to next newline */
		}
		if (new >= max
		 || fgets(buf, sizeof(buf) - 1, ifp) == NULL)
			break;			/* should not happen */
		if ((i = strncmp(key, buf, len)) <= 0)
			max = new;
		else
			min = new;
	}

	fpseek(ifp, min, SEEK_SET);
	return 1;
}

/*
 * Suppress a directory.
 * This appends the sed delete command "/^name/d;" to sed_cmd.
 */
void
suppress(name) register char *name;
{
	register int len;
	register char *s;

	/* Allocate space for additional command in sed_cmd. */
	len = strlen(name) + 6;		/* for "/^name/d;" + NUL */
	for (s = name; *s != '\0'; s++)
		if (*s == '/')
			++len;		/* bump count for each slash in name */
	if (sed_cmd == NULL)
		s = sed_cmd = malloc(len);
	else {
		s = sed_cmd = realloc(sed_cmd, strlen(sed_cmd) + len);
		s += strlen(sed_cmd);
	}

	/* Append the sed delete command "/^name/d;" to the sed_cmd string. */
	*s++ = '/';
	*s++ = '^';
	while (*name != '\0') {
		if (*name == '/')
			*s++ = '\\';
		*s++ = *name++;
	}
	*s++ = '/';
	*s++ = 'd';
	*s++ = ';';
	*s = '\0';
}

/*
 * Execute a system command, die on failure.
 */
/* VARARGS */
void
sys(s) char *s;
{
	sprintf(buf, "%r", &s);
#if	DEBUG
	fprintf(stderr, "%s\n", buf);		/* for debugging */
#endif
	if (system(buf) != 0)
		fatal("command \"%s\" failed", buf);
}

/*
 * Print a usage message and die.
 */
void
usage()
{
	fprintf(stderr, 
		"Usage:\tqfind [ -adp ] name ...\n"
		"\tqfind -b[v] [ -sname ] ...\n"
		"Options:\n"
		"\t-a\tAll: search for files or directories (default: files only).\n"
		"\t-b\tBuild file database.\n"
		"\t-d\tSearch for directories only.\n"
		"\t-p\tPartial name matching.\n"
		"\t-sname\tSuppress name (and its subdirectories) when building database.\n"
		"\t-v\tVerbose information.\n"
		);
	exit(1);
}

/* end of qfind.c */
