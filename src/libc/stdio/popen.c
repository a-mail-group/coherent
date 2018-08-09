/*
 * Standard I/O library
 * Make streams to or from a process.
 */

#include <coh_stdio.h>
#include <signal.h>

#define	R	0	/* Pipe read descriptor index */
#define	W	1	/* Pipe write descriptor index */

static	int	poppid[_NFILE];

FILE *
popen(command, type)
char *command, *type;
{
	register mode;
	register fd;
	int pd[2];

	if (pipe(pd) < 0)
		return (NULL);
	if (*type == 'w') {
		mode = W;
		fd = pd[W];
	} else {
		mode = R;
		fd = pd[R];
	}
	if ((poppid[fd] = fork()) < 0) {
		close(pd[R]);
		close(pd[W]);
		return (NULL);
	}
	if (poppid[fd] == 0) {		/* Child */
		if (mode == W) {
			close(pd[W]);
			close(fileno(stdin));
			dup(pd[R]);
			close(pd[R]);
		} else {
			close(pd[R]);
			close(fileno(stdout));
			dup(pd[W]);
			close(pd[W]);
		}
		execl("/bin/sh", "sh", "-c", command, NULL);
		exit(1);
	}
	if (mode == W) {
		close(pd[R]);
		return (fdopen(pd[W], "w"));
	} else {
		close(pd[W]);
		return (fdopen(pd[R], "r"));
	}
}

pclose(stream)
FILE *stream;
{
	register fd;
	register pid, wpid;
	int status;
	int (*hupfun)(), (*intfun)(), (*quitfun)();

	fd = fileno(stream);
	pid = poppid[fd];
	poppid[fd] = 0;
	if (pid==0 || fclose(stream)==EOF)
		return (-1);
	hupfun = signal(SIGHUP, SIG_IGN);
	intfun = signal(SIGINT, SIG_IGN);
	quitfun = signal(SIGQUIT, SIG_IGN);
	while ((wpid = wait(&status))!=pid && wpid>=0)
		;
	if (wpid < 0)
		status = -1;
	signal(SIGHUP, hupfun);
	signal(SIGINT, intfun);
	signal(SIGQUIT, quitfun);
	return (status);
}
