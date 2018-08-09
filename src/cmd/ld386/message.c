/*
 * ld/message.c
 * Print messages of various origins.
 * And misc functions.
 */

#include <sys/compat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>

#include "ld.h"

/*
 * Begin error message print; return 0 if normal, 1 if message suppressed.
 */

#if	USE_PROTO
LOCAL int start_msg (void)
#else
LOCAL int
start_msg ()
#endif
{
	errCount ++;

	if (Qflag)
		return 1;

	fprintf (stderr, "%s: ", argv0);
	return 0;
}


#if	USE_PROTO
LOCAL void file_msg (CONST char * fname)
#else
LOCAL void
file_msg (fname)
CONST char    *	fname;
#endif
{
	fprintf (stderr, "file '%s': ", fname);
}


#if	USE_PROTO
LOCAL void mod_msg (CONST char * mname)
#else
LOCAL void
mod_msg (mname)
CONST char    *	mname;
#endif
{
	if (mname [0] != 0)
		fprintf (stderr, "module '%s': ", mname);
}


/*
 * Fatal error; print message and exit
 */

#if	USE_PROTO
NO_RETURN fatal (CONST char * format, ...)
#else
NO_RETURN
fatal (format)
CONST char    *	format;
#endif
{
	if (! start_msg ()) {
		va_list		args;

		va_start (args, format);
		vfprintf (stderr, format, args);
		va_end (args);
		fputc ('\n', stderr);
	}
	exit (1);
}

#if	USE_PROTO
void message (CONST char * format, ...)
#else
void
message (format)
CONST char    *	format;
#endif
{
	va_list		args;

	if (start_msg ())
		return;

	va_start (args, format);
	vfprintf (stderr, format, args);
	va_end (args);
	fputc ('\n', stderr);
}


#if	USE_PROTO
void watch_message (CONST char * format, ...)
#else
void
watch_message (format)
CONST char    *	format;
#endif
{
	va_list		args;

	fprintf (stderr, "%s: ", argv0);

	va_start (args, format);
	vfprintf (stderr, format, args);
	va_end (args);
	fputc ('\n', stderr);
}

/*
 * message with filename
 */

#if	USE_PROTO
void filemsg (CONST char * fname, CONST char * format, ...)
#else
void
filemsg(fname, format)
CONST char    *	fname;
CONST char    *	format;
#endif
{
	va_list		args;

	if (start_msg ())
		return;

	file_msg (fname);

	va_start (args, format);
	vfprintf (stderr, format, args);
	va_end (args);
	fputc ('\n', stderr);
}

/*
 * Message with module and file name
 */

#if	USE_PROTO
void modmsg (CONST char * fname, CONST char * mname, CONST char * format, ...)
#else
void
modmsg (fname, mname, format)
CONST char    *	fname;
CONST char    *	mname;
CONST char    *	format;
#endif
{
	va_list		args;

	if (start_msg ())
		return;

	file_msg (fname);
	mod_msg (mname);

	va_start (args, format);
	vfprintf (stderr, format, args);
	va_end (args);
	fputc ('\n', stderr);
}

/*
 * Message for module passed by pointer
 */

#if	USE_PROTO
void mpmsg (CONST mod_t * mp, CONST char * format, ...)
#else
void
mpmsg (mp, format)
CONST mod_t   *	mp;
CONST char    *	format;
#endif
{
	va_list		args;

	if (start_msg ())
		return;
	file_msg (mp->fname);
	mod_msg (mp->mname);

	va_start (args, format);
	vfprintf (stderr, format, args);
	va_end (args);
	fputc ('\n', stderr);
}


/*
 * Message for symbol passed by pointer
 */

#if	USE_PROTO
void spmsg (CONST sym_t * sp, CONST char * format, ...)
#else
void
spmsg(sp, format)
CONST sym_t   *	sp;
CONST char    *	format;
#endif
{
	char work[SYMNMLEN + 1];
	va_list		args;

	if (start_msg ())
		return;
	
	if (sp->mod != NULL) {
		file_msg (sp->mod->fname);
		mod_msg (sp->mod->mname);
	}

#if	COMEAU
	if (Comeau_flag)
		fprintf (stderr, "symbol %s ", symName (& sp->sym, work));
	else
#endif
		fprintf (stderr, "symbol '%s' ", symName (& sp->sym, work));

	va_start (args, format);
	vfprintf (stderr, format, args);
	va_end (args);
	fputc ('\n', stderr);
}


/*
 * Warning message for symbol passed by pointer
 */

#if	USE_PROTO
void spwarn (CONST sym_t * sp, CONST char * format, ...)
#else
void
spwarn (sp, format)
CONST sym_t   *	sp;
CONST char    *	format;
#endif
{
	char work[SYMNMLEN + 1];
	va_list		args;

	if (Qflag)
		return;

	fprintf (stderr, "%s: ", argv0);

	if (sp->mod != NULL) {
		file_msg (sp->mod->fname);
		mod_msg (sp->mod->mname);
	}

	fprintf (stderr, "warning: symbol '%s' ", symName (& sp->sym, work));

	va_start (args, format);
	vfprintf (stderr, format, args);
	va_end (args);
	fputc ('\n', stderr);
}


/*
 * Module is corrupt message.
 */

#if	USE_PROTO
NO_RETURN corrupt (CONST mod_t * mp)
#else
NO_RETURN
corrupt (mp)
CONST mod_t   *	mp;
#endif
{
	mpmsg(mp, "is corrupt");
	/* This does not make sense as an object module. */
	exit (1);
}

/*
 * message plus command prototype
 */

#if USE_PROTO
NO_RETURN help (void)
#else
NO_RETURN
help ()
#endif
{
	static char document[] =
		"Usage: ld [ option ... ] file ...\n"
		"Options:\n"
		"\t-d\t\tDefine commons even if undefined symbols\n"
		"\t-e entry\tSet entry point\n"
		"\t-G\t\tSuppress common/global warnings\n"
		"\t-K\t\tCompile kernel\n"
		"\t-l lib\t\tUse library\n"
		"\t-o outfile\tSet output filename default is a.out\n"
		"\t-r\t\tRetain relocation information\n"
		"\t-s\t\tStrip symbol table\n"
		"\t-u sym\t\tUndefine sym\n"
		"\t-w\t\tWatch messages enabled\n"
		"\t-X\t\tDiscard local symbols beginning .L\n"
		"\t-x\t\tDiscard all local symbols\n";
	printf(document);
	exit(0);
}

#if	USE_PROTO
NO_RETURN usage (void)
#else
NO_RETURN
usage ()
#endif
{
	fprintf(stderr,
"usage: ld [-drswxX?] [-o out] [-e entry] [-u sym] file ... [-l lib] ...\n");
	exit(1);
}

/*
 * Open a file or die in the attempt.
 */

int
qopen(fn, do_creat)
CONST char    *	fn;
int		do_creat;
{
	int fd;

	if (do_creat)
		fd = creat (fn, 0666);
	else
		fd = open (fn, 0);

	if (-1 == fd) {
		filemsg (fn, "Cannot open.");	/**/
		exit (1);
	}
	return fd;
}
