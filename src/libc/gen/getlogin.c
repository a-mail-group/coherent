/*
 * libc/gen/getlogin.c
 * Coherent Standard C Library.
 * getlogin()
 * Get login name from the utmp file.
 */

#include <stdio.h>
#include <utmp.h>
#include <unistd.h>

char *
getlogin()
{
	register struct utmp *up;
	register int n;
	register char *tname;
	register int ufd;
	char iobuf[BUFSIZ+1];
	static char uname[DIRSIZ];

	if ((ufd = open("/etc/utmp", 0))>=0
	    && (tname = ttyname(fileno(stderr)))!=NULL) {
		tname += 5;			/* Skip over "/dev/" */
		while ((n = read(ufd, iobuf, BUFSIZ)) > 0)
			for (up = iobuf; up < &iobuf[n]; up++)
				if (strncmp(up->ut_line, tname, 8) == 0) {
					close(ufd);
					strncpy(uname, up->ut_name, DIRSIZ);
					return uname;
				}
		close(ufd);
	}
	return NULL;
}

/* end of libc/gen/getlogin.c */
