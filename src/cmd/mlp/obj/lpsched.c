/* mlp/lpsched.c */
/*****************************************************************************
	MLP spooler.  Search the print queue (/usr/spool/mlp/queue) for reports
	to print.  When necessary, spawn processes to print these reports on the
	proper printers observing the needs of concurrent printing processes.

	Author: B.Blatchley (c) 1992 Magnetic Data Operation
*****************************************************************************/

#include "mlp.h"

/* Manifest constants. */
#define	NSLEEP	30		/* sleep NSLEEP seconds in main() loop */
#define	NGRIM	10		/* grim_reaper() every NGRIM * NSLEEP seconds */
#define	SECS_PER_HOUR(f)	((time_t)(60 * 60 * f))	/* float hours f to time_t */

/* Globals. */
char	curr_desc[WORKSTR];
char	curr_user[NNAME];
char	curr_longevity[2];
int	curr_mailme;
int	curr_writeme;
int	kill_request = FALSE;
int	pip[2];			/* backend pipe, global so cancel can close it */
int	signalled;

/*****************************************************************************
  There are times when this program will sleep, while waiting for new things
  to do.  This process is called when the program receives a "wakeup" signal
  from another process (or from its internal alarm-clock).
*****************************************************************************/

void
wakeup()
{
	signal(SIGALRM, wakeup);
	signalled = 1;
}


/*****************************************************************************
	Kill the backend process without placing the request on hold
*****************************************************************************/

void
request_kill()
{
	abort_write = TRUE;
	kill_request = TRUE;
	signal(R_KILL, request_kill);
}


/*****************************************************************************
  Kill the backend process but keep the request current (will reprint when
  the despooler scans the the queue).
*****************************************************************************/

void
request_cancel()
{
	abort_write = TRUE;
	kill_request = FALSE;
	signal(R_CANCEL, request_cancel);
}


/*****************************************************************************
	Given the name of a printer, return the name of its device.  The controls
	line for a printer looks like this:

	printer = name, device, backend
*****************************************************************************/

char *
printer_device(newprinter) char *newprinter;
{
	char		line[WORKSTR], tmp[NPRINTER], printer[WORKSTR];
	register char	*c;
	register FILE	*fp;

	strcpy(printer, newprinter);

	MLP_lock(L_CONTROL);
	fp = xopen(CONTROL_PATH, "r", 0);

	while (fgets(line, sizeof(line), fp)) {

		*tmp = *curr_device = *curr_backend = 0;

		c = strtoksep(line);
		if (c) strncpy(tmp, c, sizeof(tmp));

		if (strequal("PRINTER", uppercase(tmp))) {
			c = strtoksep(NULL);

			if (strequal(printer, c)) {
				c = strtoksep(NULL); if (c) strncpy(curr_device,  c, WORKSTR);
				c = strtoksep(NULL); if (c) strncpy(curr_backend, c, WORKSTR);
				break;
			}
		}
	}

	fclose(fp);
	MLP_unlock(L_CONTROL);
	return curr_device;
}


/*****************************************************************************
  Extract Header Values
*****************************************************************************/

int
extract_hdr(name) char *name;
{
	char hdr[HEADER_SIZE];

	if (!read_hdr(makepath(QUEUE_PATH, name), hdr))
		return 0;

	strcpy(curr_name,       name);
	strcpy(curr_user,       getuhdrval(hdr, H_USER));
	strcpy(curr_printer,    getuhdrval(hdr, H_PRINTER));
	strcpy(curr_desc,       getuhdrval(hdr, H_DESC));
	strncpy(curr_longevity, getuhdrval(hdr, H_LONGEVITY), 1); 

	curr_mailme =  (*(gethdrval(hdr, H_MAILME )) != ' ');
	curr_writeme = (*(gethdrval(hdr, H_WRITEME)) != ' ');

	sscanf(gethdrval(hdr, H_COPIES ), "%d", &curr_copies);
	sscanf(gethdrval(hdr, H_FORMLEN), "%d", &curr_formlen);

	return 1;
}


/*****************************************************************************
	Return TRUE if the device specified in this request is available.
	STEVE 8/24/94: This assumes that no other process can
	change the status file, otherwise there is
	no guarantee that the device will still be available after
	a successful return from device_available().
*****************************************************************************/

int
device_available(name) char *name;
{
	int	pid;
	char	*c, *t;

	if (extract_hdr(name) == 0)
		return 0;

	printer_device(curr_printer);

	if (curr_device[0] == '\0') {	/* No device? */

		warning_user(
			curr_user,
			"Printer \"%s\" has no associated device.\n"
			"Request #%s has been suspended.\n"
			"To resolve the problem, define the printer in \"%s\"\n"
			"or route the request to a different printer.\n"
			"Then you will need to ask MLP to reprint the request.",
			curr_printer,
			&name[2], CONTROL_PATH
			);

		/* Put the request into the inactive queue */
		make_inactive(name);
		return 0;
	}

	/* Find out if the device is being used. */
	if (*(t = getstatus(curr_device)) != '\0') {

		/* The status file reports device in use, check its pid. */
		c = strtoksep(t);  sscanf(c, "%d", &pid);
		if (!kill(pid, 0))
			return 0;	/* process is still active */
	}

	return 1;
}


/*****************************************************************************
	Write Request.  Setup a pipeline and spawn a backend process to handle
	the writing of the request.  We do this so that we can monitor to progress
	of the request.
*****************************************************************************/

void
write_request(name, backend, start, end) char *name, *backend; int start, end;
{
	int		ifd, ofd, feed, copy, docopies;
	char		*tmp, *t;
	static	char	ff = '\f';
	int		pid, status;
	int		(*sigcld)();

	signal(R_KILL,	request_kill);
	signal(R_CANCEL, request_cancel);

	/* Open the print request and the device (data source & sink) */ 
	if ((ifd = open(t = makepath2(QUEUE_PATH, name), O_RDONLY)) == BAD) {
		warning_user(curr_user,
			"Cannot open spool file \"%s\" for printing request #%s",
			t, &name[2]);
		make_inactive(name);
		return;
	}

	if ((ofd = open(curr_device, O_WRONLY|O_CREAT|O_TRUNC|O_SYNC, CREAT_PERMS)) == BAD) {
		close(ifd);
		warning_user(curr_user,
			"Cannot open device %s for printing request #%s (\"%s\").\n"
			"Request #%s is now inactive.\n"
			"Use \"reprint %s\" after checking printer status.\n",
			curr_device, &name[2], curr_desc, &name[2], &name[2]);
		make_inactive(name);
		return;
	}

	if (pipe(pip) == BAD) {
		close(ifd);
		close(ofd);
		warning_user(curr_user,
			"Cannot create pipe to despool #%s", &name[2]);
		make_inactive(name);
		return;
	}

	/* The parent feeds the child, while noting the progress of the feeding
		in the status file -- Inquiring Minds want to know! */

	if ((pid = fork()) == BAD) {

		/* fork() failed */
		 warning_user(curr_user,
			"Cannot fork to despool #%s", &name[2]);
		 close(ifd); close(ofd); close(pip[0]); close(pip[1]);
		 return;

	} else if (pid == 0) {

		/*
		 * Child: exec the backend, reading input from pipe as stdin
		 * and writing output to ofd as stdout.
		 * If the backend is the default (i.e. /bin/cat), just exec it.
		 * Otherwise, exec a shell with the backend (a shell script) and
		 * request number, user name, copies and description as args.
		 * To the shell script, filename et al. will be $1 to $4.
		 */
		close(ifd); close(pip[1]);
		dup2(pip[0], 0); dup2(ofd, 1);
		close(pip[0]); close(ofd);

		signal(SIGTERM, SIG_IGN);
		signal(SIGINT,  SIG_IGN);

		if (strequal(backend, DBACKEND))
			execl(backend, backend, NULL);
		else
			execl("/bin/sh", "/bin/sh", backend, &name[2], curr_user,
				string("%d", curr_copies), curr_desc, NULL);
		/* NOTREACHED */
	}

	/* Parent feeds the child, reading from ifd and writing to pipe. */
	close(ofd); close(pip[0]);

	t = getcontrol(FEED);
	feed = (strequal(t, "ON") || strequal(t, "on"));

	docopies = 1;
	t = getcontrol(DOCOPIES);
	if (strequal(t, "ON") || strequal(t, "on")) docopies = curr_copies;

	for (copy = 0; copy < docopies; copy++) {
		curr_copy = copy + 1;

		lseek(ifd, (long) HEADER_SIZE, SEEK_SET);

		if (writeOut(ifd, pip[1], start, end, TRUE) == BAD) {

			if (kill_request) {
				tmp = string("\n\n*** Request #%s terminated ***\n\f", &name[2]);
				warning_user(curr_user,
					"The despooling of request #%s has been aborted.",
					&name[2]);
			} else {
				tmp = string("\n\n*** Request #%s canceled ***\n\f", &name[2]);
				warning_user(curr_user,
					"Request #%s has been canceled.",
					&name[2]);
			}

			write(pip[1], tmp, strlen(tmp));
			break;
		}

		if (feed)
			write(pip[1], &ff, 1);
	}

	close(ifd); close(pip[1]);
 
	/* If the request was aborted, keep it in the active
		part of the queue.  Otherwise, retire it to the
		inactive queue.  If the request is a reprint
		then remove it from the queue. */

	if (!kill_request)
		make_inactive(name);

	/* Reap Children */
printf("calling wait()...\n");
{
int i;
	while ((i = wait(&status)) != -1)
printf("wait: i=%d status=%d\n", i, status);
		/* Do Nothing */ ;

printf("wait: i=%d status=%d\n", i, status);
}
	/* Send Messages if desired. */
	if (curr_writeme) {
		system(string(
			"/bin/echo \"\n`date`\n\"'\n"
			"*** MLP Spooler Reports ***\n\n"
			"Your printing request \"%s\" (%s) \n"
			"has been despooled on printer %s.\n\n"
			"' | write %s 2>/dev/null",
			curr_desc, &name[2], curr_printer, curr_user));
	}
	if (curr_mailme) {
		/* Reset SIGCLD to SIG_DFL when sending mail, else mail complains. */
		sigcld = signal(SIGCLD, SIG_DFL);
		system(string(
			"/bin/echo \"\n`date`\n\"'\n"
			"*** MLP Spooler Reports ***\n\n"
			"Your printing request \"%s\" (%s) \n"
			"has been despooled on printer %s.\n\n"
			"' | %s %s 2>/dev/null",
			curr_desc, &name[2], curr_printer,
			(getenv("MLPLMAIL")) ? "lmail" : "mail",
			curr_user));
		signal(SIGCLD, sigcld);
	}
}


/*****************************************************************************
	De-spool a given file.
*****************************************************************************/

void
despool_file(name, start, end) char *name; int start, end;
{
	char	tmp[NBUF2], newname[NNAME];
	char	*path;
	int	fd, ocopies;

	if (*curr_longevity == 'R') {		 /* Is this a reprint? */

		/*
		 * Sample description:
		 *	"Reprint #00000 from page 0000 to 0000"
		 *	 012345678|012345678901234|6789012|456
		 */

		sscanf(&curr_desc[9], "%5s", tmp);
		strcpy(newname, request_name(tmp));

		sscanf(&curr_desc[25], "%d", &start);
		sscanf(&curr_desc[33], "%d", &end);  

		unlink(makepath(QUEUE_PATH, name));

		ocopies = curr_copies;  strcpy(tmp, curr_printer);
		extract_hdr(newname);
		curr_copies = ocopies;  strcpy(curr_printer, tmp);
	} else
		strcpy(newname, name);

	if (*curr_backend == 0) strcpy(curr_backend, DEFAULT);

	/* Find the appropriate backend name in path; make sure it exists. */
	if (strequal(curr_backend, DBACKEND))
		path = DBACKEND;
	else if ((fd = open(path = makepath(BACKEND_PATH, curr_backend), O_RDONLY)) == BAD) {
		warning(
			"Cannot find backend script \"%s\" for printer \"%s\"\n"
			"while trying to despool request #%s.\n"
			"Despooling done with \"%s\" instead.\n",
			curr_backend, curr_printer, &newname[2], DBACKEND
			);
		path = DBACKEND;
	} else
		close(fd);

	/* Do it! */
	write_request(newname, path, start, end);
	log("Despooled request #%s", &name[2]);
}


/*****************************************************************************
	De-spool.  Check the queue for work, then process it.
	This will queue work to multiple printers simultaneously.
*****************************************************************************/

int
despool()
{
	char	*name, *plist, *qlist, tmp[WORKSTR];
	int	ppid, pid, despooled;

	ppid = getpid();
	plist = xalloc(NBUF);	/* printers in use during this pass over the spool queue */
	despooled = 0;		/* files despooled during this pass */
	for (name = qlist = queuelist(FALSE); *name != '\0'; name += MAXNAMLEN) {

		if (name[0] != 'R')
			continue;		/* despool only "Ready" entries */
		if (!device_available(name))
			continue;		/* device not available */
		sprintf(tmp, "%s|", curr_device);

		/* Avoid trying if device is already in the plist. */
		if (strstr(plist, tmp) == NULL) {
			strcat(plist, tmp);

			if ((pid = fork()) == BAD) {
				/* Fork failed. */
				warning("Cannot fork backend process for request #%s", &name[2]);
				break;
			} else if (pid == 0) {
				/* Child: claim the device and do the work. */
				pid = getpid();
				sprintf(tmp, "%d, %s", pid, name);
				setstatus(curr_device, tmp); 
				despool_file(name, 0, 0);

				/* Wakeup parent to rescan for work for this device */
				kill(ppid, SIGALRM);
				exit(EXIT_SUCCESS);
			}
			++despooled;
		}
	}

	free(plist);
	free(qlist);
	return despooled;
}


/******************************************************************************
	Roll the log over after a given time so that it does not grow
	without bounds.
******************************************************************************/

#define	RHOURS (7.0 * 24.0)		/* one week */

void
roll_log()
{
	time_t	current, rollover;
	struct	stat sb;
	float	rhours;
	char	*tmp;

	rhours = RHOURS;	sscanf(getcontrol(LOGROLL), "%f", &rhours);
	rollover = SECS_PER_HOUR(rhours);

	time(&current);

	if (stat(LOG_PATH, &sb) == BAD)
		return;

	if (sb.st_ctime + rollover < current) {
		tmp = string("%s.old", LOG_PATH);
		unlink(tmp); rename(LOG_PATH, tmp);
		warning_user("root", "Logfile moved to log.old, starting new log.");
	}
}


/*****************************************************************************
	This routine scans the spool queue looking for request that need to be
	removed.  That is, they need to die of old age!  There are four classes
	of longevity: (R) reprint, (T) temporary, (S) shortterm, and (L) longterm.
	R requests are really just headers and they are deleted as soon as they
	have despooled.  This routine handles the other three classes.
*****************************************************************************/

#define	THOURS 0.5			/* half an hour */
#define	SHOURS 24.0			/* one day */
#define	LHOURS (7.0 * 24.0)		/* one week */

void
grim_reaper()
{
	long	dstamp;
	time_t	current, temporary, shortterm, longterm, elapst;
	float	thours, shours, lhours;
	char	*qlist, *name, *path, hdr[HEADER_SIZE], life[2];
	char	desc[WORKSTR];
	int	die;

	thours = THOURS; sscanf(getcontrol(TLIFE), "%f", &thours);
	shours = SHOURS; sscanf(getcontrol(SLIFE), "%f", &shours);
	lhours = LHOURS; sscanf(getcontrol(LLIFE), "%f", &lhours);

	temporary = SECS_PER_HOUR(thours);
	shortterm = SECS_PER_HOUR(shours);
	longterm  = SECS_PER_HOUR(lhours);

	time(&current);

	for (name = qlist = queuelist(FALSE); *name != '\0'; name += MAXNAMLEN) {
		if (*name == 'R')
			continue;

		if (!read_hdr(path = makepath(QUEUE_PATH, name), hdr))
			continue;

		dstamp = 0L; 
		sscanf(gethdrval(hdr, H_DSTAMP   ), "%ld", &dstamp);
		sscanf(gethdrval(hdr, H_LONGEVITY), "%1s", life);
		strcpy(desc, getuhdrval(hdr, H_DESC));

		elapst = current - (time_t)dstamp;
		die = FALSE;
		switch (*life) {
			case 'T': if (elapst > temporary) die = TRUE; break;
			case 'S': if (elapst > shortterm) die = TRUE; break;
			case 'L': if (elapst > longterm)  die = TRUE; break;
		}

		if (die) { 
			log("Request #%s \"%s\" expired", &name[2], desc);
			unlink(path);
		}
	}

	free(qlist);
}


/*****************************************************************************
	Main Driver.	Tell the spooler how to find us.  Then process the queue
	and sleep until something exciting happens ;)  First check to see if
	the despooler is already running.
*****************************************************************************/

int
main(argc, argv) int argc; char *argv[];
{
	int  pid, cycles;

	getname(argv[0]);
	if (argc > 1 && strequal(argv[1], "-V")) {
		fprintf(stderr, "%s: " VERSION "\n", progname);
		--argc;
		++argv;
	}

	if (argc != 1) {
		fprintf(stderr, "Usage: lpsched\n");
		exit(EXIT_FAILURE);
	}

	if (scheduler_status()) {
		error("MLP scheduler already started");
		exit(EXIT_FAILURE);
	}

	/* 
	 * Cf. Stevens, "Unix Network Programming", pp. 78-81,
	 * for fork()/setpgrp() sequences.
	 */
	if ((pid = fork()) == BAD)
		fatal("Scheduler cannot fork");		/* fork() failed */
	else if (pid != 0)
		exit(EXIT_SUCCESS);			/* parent just exits */

	/*
	 * Now the running process is the first child.
	 */
	chdir("/");		/* so we can dismount any mounted filesystems */
	setpgrp();		/* change process group and lose control tty */
	signal(SIGALRM, wakeup);
	signal(SIGCLD,  SIG_IGN);
	signal(SIGHUP,  SIG_IGN);

	/*
	 * Now the child forks again.
	 * The second fork keeps the process from ever reacquiring a controlling
	 * terminal, since the second child is not a process group leader and
	 * SV only assigns new control terminals to group leaders.
	 */
	if ((pid = fork()) == BAD)
		fatal("Scheduler cannot fork");		/* fork() failed */
	else if (pid != 0) {
		/* The first child prints and logs start info and then just exits. */
		printf("MLP scheduler started [%d].\n", pid);
		log("MLP scheduler started [%d]", pid);
	} else {

		/* Second child is scheduler daemon running in background. */

#if	0
		/*
		 * If debugging, conditionalize this out
		 * to allow lpsched to write to stdout or stderr.
		 * For production, this should be conditionalized in,
		 * because otherwise e.g. "lp -w foo" prints a
		 * "User whoever has been notified" message on the invoker's
		 * screen.
		 */
		close(1);
		close(2);
#endif

		setstatus(DESPOOLER, string("%d", getpid())); 

		for (cycles = 0; *(getstatus(DESPOOLER)) != '\0'; ) {

			signalled = 0;
			MLP_lock(L_CANCEL);
			despool();
			if (++cycles % NGRIM == 0) {
				grim_reaper();		/* purge the inactive queue */
				roll_log();		/* roll over the log file */
			}
			MLP_unlock(L_CANCEL);
			if (signalled)
				continue;		/* signalled, try again */
			sleep(NSLEEP);
		}
		fatal("Scheduler daemon expired gracefully"); 
	}

	exit(EXIT_SUCCESS);
}

/* end of mlp/lpsched.c */
