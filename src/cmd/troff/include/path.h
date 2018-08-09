/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef __PATH_H__
#define __PATH_H__

/*
 * This header defines constants and function declarations that are not part
 * of any standard.  Users of the path () function are referred to the
 * POSIX standards for portable alternatives.
 */

/*
 * Path lookup.
 *
 * Path takes a path search list, a file name, and an access mode, and
 * searches each directory in the path list for a file with the given name and
 * access mode.
 * The constructed pathname, which is kept in a static buffer, is returned on
 * success. NULL is returned if either pointer argument is NULL, or if the
 * search failed.
 */

#if	__COHERENT__
#define PATHSEP		'/'	/* Path name component separator */
#define	PATHSEPSTRING	"/"
#define LISTSEP		':'	/* Search list component separator */
#define DEFPATH		":/bin:/usr/bin"
#define DEFLIBPATH	"/lib:/usr/lib"
#define DEFSHELL	"sh"

#elif	defined(__linux__)

#define PATHSEP		'/'	/* Path name component separator */
#define	PATHSEPSTRING	"/"
#define LISTSEP		':'	/* Search list component separator */
#define DEFPATH		":/bin:/usr/bin"
#define DEFLIBPATH	"/lib:/usr/lib"
#define DEFSHELL	"sh"

#elif	defined(sparc)

#define PATHSEP		'/'	/* Path name component separator */
#define	PATHSEPSTRING	"/"
#define LISTSEP		':'	/* Search list component separator */
#define DEFPATH		":/bin:/usr/bin"
#define DEFLIBPATH	"/lib:/usr/lib"
#define DEFSHELL	"sh"

#elif	GEMDOS

#define PATHSEP		'\\'
#define	PATHSEPSTRING	"\\"
#define LISTSEP		','
#define DEFPATH		",\\bin,\\usr\\bin"
#define DEFLIBPATH	"\\lib,,\\usr\\lib"
#define DEFSHELL	"msh.prg"

#elif	MSDOS

#define PATHSEP		'\\'
#define	PATHSEPSTRING	"\\"
#define LISTSEP		';'
#define DEFPATH		";\\bin;\\usr\\bin"
#define DEFLIBPATH	"\\lib;\\usr\\lib"
#define	DEFSHELL	"steve where are you"

#endif

#define MAXPATH	128		/* Size of static pathname buffer */
extern	char	*path();	/* (char *path, *file; int mode) */

#endif /* __PATH_H__ */
