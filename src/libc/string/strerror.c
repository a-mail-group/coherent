/*
 * strerror.c
 * ANSI 4.11.6.2.
 * String error function.
 * Assumes COHERENT <errno.h>.
 * Assumes exactly one of COHERENT, GEMDOS or MSDOS defined.
 */

#include <errno.h>

#ifdef	COHERENT
static char *_errors[] = {
	"Illegal error number",			/* 0		*/
	"Not super user",			/* EPERM	*/
	"No such file or directory",		/* ENOENT	*/
	"Process not found",			/* ESRCH	*/
	"Interrupted system call",		/* EINTR	*/
	"I/O error",				/* EIO		*/
	"No such device or address",		/* ENXIO	*/
	"Argument list too long",		/* E2BIG	*/
	"Sys exec format error",		/* ENOEXEC	*/
	"Bad file number",			/* EBADF	*/
	"No children (wait)",			/* ECHILD	*/
	"No more processes are available",	/* EAGAIN	*/
	"Cannot map process into memory",	/* ENOMEM	*/
	"Permission denied",			/* EACCES	*/
	"Bad system call argument address",	/* EFAULT	*/
	"Block device required (mount)",	/* ENOTBLK	*/
	"Device busy",				/* EBUSY	*/
	"File already exists",			/* EEXIST	*/
	"Cross device link",			/* EXDEV	*/
	"No such device",			/* ENODEV	*/
	"Not a directory",			/* ENOTDIR	*/
	"Is a directory",			/* EISDIR	*/
	"Invalid argument",			/* EINVAL	*/
	"File table overflow",			/* ENFILE	*/
	"Too many open files for this process",	/* EMFILE	*/
	"Not a terminal",			/* ENOTTY	*/
	"Text file busy",			/* ETXTBSY	*/
	"File too big to map",			/* EFBIG	*/
	"No space left on device",		/* ENOSPC	*/
	"Illegal seek on a pipe",		/* ESPIPE	*/
	"Read only filesystem",			/* EROFS	*/
	"Too many links",			/* EMLINK	*/
	"Broken pipe",				/* EPIPE	*/
	"Domain error",				/* EDOM		*/
	"Result too large",			/* ERANGE	*/
	"Out of kernel space",			/* EKSPACE	*/
#if	_I386
	"Identifier removed",			/* EIDRM	*/
#else
	"Driver not loaded",			/* ENOLOAD	*/
#endif
	"Bad format",				/* EBADFMT	*/
	"Device needs attention",		/* EDATTN	*/
	"Device busy",				/* EDBUSY	*/
	"Deadlock",				/* EDEADLK	*/
	"No lock available"			/* ENOLCK	*/
};
#endif

#ifdef	GEMDOS
extern char *sys_errlist[];
extern int sys_nerr;
#endif

char *strerror(errnum) int errnum;
{

#ifdef	COHERENT
	if (errnum < 0 || errnum > sizeof(_errors)/sizeof(char *))
		errnum = 0;
	return _errors[errnum];
#endif

#ifdef	GEMDOS
	if (errnum < 0 || errnum > sys_nerr)
		errnum = 0;
	return sys_errlist[errnum];
#endif

#ifdef	MSDOS
	return (  (errnum == EDOM)   ?	"Domain error"
		: (errnum == ERANGE) ?	"Result too large"
		:			"Illegal error number"
	);
#endif

}
