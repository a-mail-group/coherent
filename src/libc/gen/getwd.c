/*
 * libc/gen/getwd.c
 * Return a static string containing the current
 * working directory for this process.
 * The arguments to this function are NOT compatible with
 * the BSD function getwd(), so it is now called _getwd().
 * Warning: this call may change the current directory
 * of the process if for any reason it fails.
 */

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <kernel/dir.h>
#include <canon.h>

#define	MAXNAME	400		/* Longest pathname */

static	int	oerrno;

static
char *
fail()
{
	if (errno == 0)
		errno = oerrno;		/* preserve previous errno */
	return NULL;
}

char *
_getwd()
{
	struct stat d, dd;
	struct direct dir;
	static char fnbuf[MAXNAME];
	register char *cp, *dp;
	register int file;
	register dev_t rdev;
	register ino_t rino;

	oerrno = errno;			/* save old errno */
	errno = 0;
	dp = fnbuf+MAXNAME-1;
	*dp = '\0';
	if (stat("/", &d) < 0)
		return fail();
	rdev = d.st_dev;
	rino = d.st_ino;
	while (stat(".", &d)>=0 && (d.st_ino!=rino || d.st_dev!=rdev)) {
		if ((file = open("..", 0)) < 0)
			return fail();
		if (fstat(file, &dd)<0 || chdir("..")<0) {
			close(file);
			return fail();
		}
		if (d.st_dev == dd.st_dev) {
			if (d.st_ino == dd.st_ino) {
				close(file);
				break;
			}
			do {
				if (read(file, (char *)&dir, sizeof (dir))
				    != sizeof (dir)) {
					close(file);
					return fail();
				}
				canino(dir.d_ino);
			} while (dir.d_ino != d.st_ino);
		} else
			do {
				if (read(file, (char *)&dir, sizeof (dir))
				    != sizeof  (dir)) {
					close(file);
					return fail();
				}
				canino(dir.d_ino);
				if (dir.d_ino!=0 && stat(dir.d_name, &dd)<0) {
					close(file);
					return fail();
				}
			} while (dd.st_ino!=d.st_ino || dd.st_dev!=d.st_dev);
		close(file);
		if (dp-DIRSIZ <= fnbuf)
			return fail();
		for (cp=dir.d_name; cp!=dir.d_name+DIRSIZ && *cp!='\0'; cp++)
			;
		while (cp > dir.d_name)
			*--dp = *--cp;
		*--dp = '/';
	}
	if (errno)
		return NULL;
	if (*dp != '/')
		*--dp = '/';
	if (chdir(dp) < 0)
		return fail();
	errno = oerrno;
	return dp;
}

/* end of libc/gen/getwd.c */
