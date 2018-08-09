/*
 * libc/stdlib/system.c
 * C general utilities library.
 * system()
 * ANSI 4.10.4.4.
 * Call the system to execute a command line which is passed as an argument.
 */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

int
system(line) char *line;
{
	int status, pid;
	register wpid;
	register int (*intfun)(), (*quitfun)();

	if ((pid = fork()) < 0)
		return -1;
	if (pid == 0) {		/* Child */
		execl("/bin/sh", "sh", "-c", line, NULL);
		exit(0177);
	}
	intfun = signal(SIGINT, SIG_IGN);
	quitfun = signal(SIGQUIT, SIG_IGN);
	while ((wpid = wait(&status))!=pid && wpid>=0)
		;
	if (wpid < 0)
		status = wpid;
	signal(SIGINT, intfun);
	signal(SIGQUIT, quitfun);
	return status;
}

/* end of libc/stdlib/system.c */
