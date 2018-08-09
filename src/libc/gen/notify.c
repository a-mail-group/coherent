/*
 * Routine to notify a user about
 * the completion of a transaction
 * Usually called by some daemon (e.g.
 * line printer daemon).
 * Return non-zero on failure.
 */

#include <stdio.h>
#include <pwd.h>

notify(name, msg)
char *name;
char *msg;
{
	register struct passwd *pwp;
	int pfd[2];
	register int pid, fd;
	int status;

	if (*name>='0' && *name<='9')
		if ((pwp = getpwuid(atoi(name))) == NULL)
			name = NULL; else
			name = pwp->pw_name;
	if (name==NULL || pipe(pfd)<0 || (pid = fork())<0)
		return (1);
	if (pid) {
		close(pfd[0]);
		write(pfd[1], msg, strlen(msg));
		close(pfd[1]);
		while (wait(&status) >= 0)
			;
	} else {
		close(pfd[1]);
		dup2(pfd[0], 0);
		close(pfd[0]);
		for (fd=3; fd<_NFILE; fd++)
			close(fd);
		execlp("/bin/send", "send", "-s", name, NULL);
		return (1);
	}
	return (0);
}
