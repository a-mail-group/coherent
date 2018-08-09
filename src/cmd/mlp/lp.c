/* mlp/lp.c */

/******************************************************************************
	lp/lpshut/reprint.
	This is the Multiple-Line-Printer spooler interface to applications
	and the Coherent user.  Its job is to accept a print job and place it
	into the spool queue.  As part of this, a special filename is formed to 
	ensure that requests are printed in a prioritized-first-come-first-served
	manner.  The actual file is given a 192 byte header.  Then the text of the
	report is appended on the header.  Much of the information stored in the
	header is extracted from an environment variable called MLP_SPOOL.  If a 
	printer is not specified, then the user name is used to derive the printer
	name.  This allows users to "route" their requests to various printers
	on demmand.

	This program's command line interface is very similar to lp in Unix SV.

	One noteable difference:  Output is ALWAYS copied into the spool directory.
	This is to facilitate deferred de-spooling.

	Author: B.Blatchley (c) Magnetic Data Operation 1992
******************************************************************************/

#include "mlp.h"

/* Externals in libterm.a. */
extern	char	*tgetstr();
extern	int	tgetent();

#define	PRIORITY	'2'
#define	LONGEVITY	'S'
#define	NPAGES		9999

static	int	copies = 1;
static	int	input_from_stdin = FALSE;
static	char	keep_status = LONGEVITY;
static	int	mailme = FALSE;
static	char	message[WORKSTR];		/* list of spool numbers */
static	char	path[WORKSTR];
static	char	printer[NPRINTER];
static	char	priority = PRIORITY;
static	char	*PS, *PN;
static	char	qstatus = 'R';
static	int	silent = FALSE;
static	char	title[WORKSTR];
static	char	user[15];
static	int	writeme = FALSE;


/******************************************************************************
	Return a datestamp string (actually it is the long-int in text form)
******************************************************************************/

char *
datestamp()
{
	time_t		t;

	time(&t);
	return string("%010lu", t);
}


/******************************************************************************
	Get Termcap Information.  If we've already filled PS then don't bother again.
******************************************************************************/

void
get_termcap()
{
	static	char	capbuf[WORKSTR];
	char		buf[NBUF], *term, *p;

	if (PS)
		return;

	if ((term = getenv("TERM")) == NULL)
		fatal("TERM must be defined for local print");

	if (tgetent(buf, term) == BAD)
		fatal("Cannot find %s in /etc/termcap for local print", term);

	p = capbuf;
	PS = tgetstr("PS", &p);
	PN = tgetstr("PN", &p);

	if (PS == NULL || PN == NULL)
		fatal("Both PS and PN must be defined for terminal %s in /etc/termcap for local print", term);
}


/******************************************************************************
	Return a new report sequence number.
	Leave the status file locked while getting it,
	otherwise e.g. multiple "lp *.c &" will not work right.
	The status file SEQNUM contains the next available number.
******************************************************************************/

int
get_seq_num()
{
	char	*v;
	int	n, next;

	n = 0;
	v = param(SEQNUM, NULL, L_STATUS, P_GETLOCK);
	sscanf(v, "%d", &n);
	next = (n >= 99999) ? 1 : n + 1;
	param(SEQNUM, string("%05u", next), L_STATUS, P_SETULOCK);
	return n;
}


/******************************************************************************
	Local print.  Send the contents of the spool file (less the header)
	to the printer connected to the user's terminal.  We assume that we are
	positioned past the header.
******************************************************************************/

void 
local_print(fd, start, end) int fd, start, end;
{
	static	char	ff = '\f';
	char		*s;
	long		here;

	here = lseek(fd, 0L, SEEK_CUR);
	get_termcap();
	write(1, PN, strlen(PN));
	writeOut(fd, 1, start, end, FALSE);
	s = getcontrol(LOCALFEED);
	if (strequal(s, "ON") || strequal(s, "on"))
		write(1, &ff, 1);
	write(1, PS, strlen(PS));
	lseek(fd, here, SEEK_SET);
}


/******************************************************************************
	Return a new filename for the spool request.
	Append the spool number to global message[].
******************************************************************************/

char *
getspoolname(when, priority) char when, priority;
{
	static	char	name[MAXNAMLEN + 1];
	int		seqnum;

	seqnum = get_seq_num();
	strcat(message, string("%d ", seqnum));
	sprintf(name, "%c%c%05d", when, priority, seqnum);
	return name; 
}


/******************************************************************************
	Given the contents of the SPOOLER environment variable other things and 
	return a header string.  This header will precede the actual printed-output.
******************************************************************************/

char *
make_hdr()
{
	static	char	hdr[HEADER_SIZE+1];
	char		s[WORKSTR], *cp;

	hdrval(hdr, H_ENTIRE, " ");

	if (input_from_stdin)
		hdrval(hdr, H_DESC, "Standard Input");
	if (path[0])
		hdrval(hdr, H_DESC,  path);
	if (title[0])
		hdrval(hdr, H_DESC,  title);

	hdrval(hdr, H_FORMLEN, string("%03d", curr_formlen));
	hdrval(hdr, H_USER, user);
	hdrval(hdr, H_PRINTER, printer);
	if (printer[0])
		hdrval(hdr, H_PRINTER, printer);
	hdrval(hdr, H_DSTAMP, datestamp());
	hdrval(hdr, H_COPIES, string("%02d", copies));
	hdrval(hdr, H_LONGEVITY, string("%c", keep_status));
	if (mailme)
		hdrval(hdr, H_MAILME,  "M");
	if (writeme)
		hdrval(hdr, H_WRITEME, "W");


	/* Fill from SPOOL Environment variable, if available. */
	if ((cp = getenv(SPOOL_ENV)) != NULL) { 
		strncpy(s, &cp[0],  10); hdrval(hdr, H_TYPE,	s);
		strncpy(s, &cp[10],  3); hdrval(hdr, H_FORMLEN,	s);
		strncpy(s, &cp[13], 14); hdrval(hdr, H_DBASE,	s);
		strncpy(s, &cp[27], 14); hdrval(hdr, H_FORMAT,	s);
		strncpy(s, &cp[41], 43); hdrval(hdr, H_DESC,	s);
	}

	return hdr;
}


/******************************************************************************
	Create a spooled printout.
	Count the pages as they go by and put count in the header for later use.
	The output file is created in SPOOLDIR, then moved to QUEUE_PATH,
	so that e.g. on lp from stdin the spooler does not find
	and despool the queued file before stdin is closed.
******************************************************************************/

void
spool_output()
{
	char	*qname, *hdr;
	int	ofd, pages, n;

	qname = getspoolname(qstatus, priority);
	hdr = make_hdr();

	if (strequal(LOCAL, printer) || strequal(DISPLAY, printer))
		qname[0] = 'r';

	if ((ofd = open(makepath(SPOOLDIR, qname), O_RDWR|O_CREAT|O_TRUNC, CREAT_PERMS)) == BAD)
		fatal("Cannot write request \"%s\" into %s", qname, SPOOLDIR);

	write(ofd, hdr, HEADER_SIZE);
	pages = writeOut(0, ofd, 0, NPAGES, FALSE);
	hdrval(hdr, H_PAGES, string("%04d", pages));

	lseek(ofd, 0L, SEEK_SET);
	write(ofd, hdr, HEADER_SIZE);

	if (strequal(LOCAL, printer)) {
		silent = TRUE;
		for (n = 0; n < copies; n++)
			local_print(ofd, 0, 0);
	}

	close(ofd);
	rename(makepath(SPOOLDIR, qname), makepath2(QUEUE_PATH, qname));

	if (strequal(DISPLAY, printer))
		display(qname);
}


/******************************************************************************
	Reprint a request that has already been spooled.
******************************************************************************/

int
reprint_request(seqnum, start, end) int seqnum, start, end;
{
	char	*qname, *hdr, request[NNAME], *name, *qlist, *last;
	int	ofd, n;

	if (start > end) {
		n = end; end = start; start = n;
	}

	if (start > 0 && end == 0)
		end = NPAGES;

	if (seqnum == 0) {
		/* Find the most recent sequence number from the spool queue. */
		last = NULL;
		for (name = qlist = queuelist(FALSE); *name != '\0'; name += MAXNAMLEN)
			if (do_i_own_it(&name[2]))
				last = name;
		free(qlist);
		if (last == NULL) {
			warning("Cannot find previous request in spool queue.");
			silent = TRUE;
			return;
		}
		sscanf(last + 2, "%d", &seqnum);	/* skip 'r' and priority */
	}

	strcpy(request, request_name(string("%05d", seqnum)));

	if (*request == 0) {
		warning("Cannot open request #%05d to despool.", seqnum);
		silent = TRUE;
		return;
	}

	if (strequal(LOCAL, printer)) {
		silent = TRUE;

		if ((ofd = open(makepath(QUEUE_PATH, request), O_RDWR)) != BAD) {
			lseek(ofd, (long) HEADER_SIZE, SEEK_SET);

			for (n = 0; n < copies; n++)
				local_print(ofd, start, end);

			close(ofd);
		}
	}

	if (strequal(DISPLAY, printer))
		display(request);

	else {
		qname = getspoolname(qstatus, priority);
		hdr = make_hdr();

		if ((ofd = open(makepath(QUEUE_PATH, qname), O_RDWR|O_CREAT|O_TRUNC, CREAT_PERMS)) == BAD)
			fatal("Cannot write request \"%s\" into the spool queue %s",
				qname, QUEUE_PATH);

		hdrval(hdr, H_DESC,
	 		string("Reprint #%05u from page %04u to %04u", seqnum, start, end)
			);
		hdrval(hdr, H_LONGEVITY, "R");

		write(ofd, hdr, HEADER_SIZE);

		close(ofd);
	}
}


/****************************************************************************
	Print a list of the generated spool id numbers.
****************************************************************************/

void 
echo_command()
{
	char *t, *u;

	if (silent)
		return;

	if (message[0] != '\0') {
		t = strtok(message, " "); u = strtok(NULL, " ");

		if (u != NULL)
			printf("MLP spooled requests: %s, %s", t, u);
		else
			printf("MLP spooled request: %s", t);

		while (t = strtok(NULL, " "))
			printf(", %s", t);
	}
	printf("\n");
}


/******************************************************************************
	Ask the scheduler to shutdown.  Remove the scheduler's PID from the status  
	file.  Then wakeup the scheduler (which will read the newly changed status
	file).  When the scheduler sees its PID removed, it will terminate.
******************************************************************************/

int
shutdown_scheduler()
{
	int	pid;

	if (is_root()) {

		pid = 0;
		sscanf(getstatus(DESPOOLER), "%d", &pid);
		if (pid == 0) {
			error("cannot find MLP Scheduler");
			return 0;
		}

		setstatus(DESPOOLER, " ");		/* remove DESPOOLER pid from status file */
		kill(pid, SIGALRM);
		printf("MLP scheduler shutdown [%d].\n", pid);
		return 1;

	} else {

		error("you must be superuser to shut down the MLP scheduler");
		return 0;

	}
}


/******************************************************************************
	Get the user name and printer routing.
******************************************************************************/

char *
get_printer_and_user_name()
{
	strcpy(user, get_username());
	strcpy(printer, getroute(user));
	if (strequal(SYSTEM, printer))
		strcpy(printer, getcontrol(DEFAULT));
}


/******************************************************************************
	Check for environmental overides for the priority, form length and copies.
******************************************************************************/

char *
get_environment()
{
	register char	c, *s;
	int		n;

	if ((s = getenv(PRIORITY_ENV)) != NULL) { 
		c = *s;
		if (c < '0')
			c = '0';
		else if (c > '9')
			c = '9';
		priority = c;
	}

	if ((s = getenv(LONGEVITY_ENV)) != NULL) { 
		switch (*s) {

		case 'l':
		case 'L':	keep_status = 'L';	break;

		case 't':
		case 'T':	keep_status = 'T';	break;

		case 's':
		case 'S':
		default:	keep_status = 'S';	break;

		}
	}


	if (getenv(FORMLEN_ENV) && sscanf(getenv(FORMLEN_ENV), "%d", &n))
		curr_formlen = n;
	else
		curr_formlen = 66;

	if (getenv(COPIES_ENV)) {
		if (sscanf(getenv(COPIES_ENV), "%d", &n))
			copies = n;
		else
			copies = 1;
	}
}


/******************************************************************************
	Display a file on the screen.
	STEVE 9/14/94: this used to system("review ..."),
	conditionalized out below; dunno what review is.
	This should use getenv("PAGER"), but instead it
	uses "more -c +2" explicitly so that it can skip the MLP header.
******************************************************************************/

display(name) register char *name;
{
	silent = TRUE;
#if	0
	system(string("review %s/%s %d -mlp; clear", QUEUE_PATH, name, curr_formlen));
#else
	system(string("more -c +2 %s/%s", QUEUE_PATH, name));
#endif
}


/******************************************************************************
	Main Driver.  Parse the arguments and spool each report named (otherwise
	spool the contents of standard input).	Then send the scheduler a wakup
	call.

	NOTE:  This program does DIFFERENT things depending on what it is called.
	 1) as "lp" it does lp kinds of things.
	 2) as "lpshut" it shuts down the scheduler.
	 3) as "reprint" it reprints spooled requests.
******************************************************************************/

int
main(argc, argv) int argc; char *argv[];
{
	char	o;
	int	fd, reprint, seqnum, start, end, has_spooled;

	has_spooled = 0;
	reprint = FALSE;

	getname(argv[0]);
	if (argc > 1 && strequal(argv[1], "-V")) {
		fprintf(stderr, "%s: " VERSION "\n", progname);
		--argc;
		++argv;
	}

	get_printer_and_user_name();

	/******** As the lpshut command *************/

	if (strequal(progname, "lpshut")) {

		if (argc > 2 || (argc == 2 && !strequal(argv[1], "-d"))) {
			fprintf(stderr, "Usage: lpshut [ -d ]\n");
			exit(EXIT_FAILURE);
		}

		/* Shut it down. */
		if (shutdown_scheduler() == 0)
			exit(EXIT_FAILURE);
		log("Shutdown scheduler daemon");

		/* Cancel the currently running backend processes. */
		if (argv[1] == NULL || !strequal(argv[1], "-d")) 
			kill_printing_request(0, R_KILL);

		exit(EXIT_SUCCESS);
	}


	/******** As the reprint command *************/

	if (strequal(progname, "reprint")) {
		if (argc > 4) {
			fprintf(stderr, "Usage:\treprint [ job [ start [ end ] ] ]\n");
			exit(EXIT_FAILURE);
		}
		get_environment();

		seqnum = start = end = 0;
		if (argc > 1)
			sscanf(argv[1], "%d", &seqnum);
		if (argc > 2)
			sscanf(argv[2], "%d", &start);
		if (argc > 3)
			sscanf(argv[3], "%d", &end);

		reprint_request(seqnum, start, end);
		wakeup_scheduler();
		exit(EXIT_SUCCESS);
	}


	/******** As the lp command *************/

	while ((o = getopt(argc, argv, "bd:mn:t:wsSR")) != EOF) {

		switch (o) {
		case 'd': strncpy(printer, optarg, sizeof(printer));	break;
		case 't': strncpy(title,   optarg, sizeof(title));	break;
		case 'n': sscanf(optarg, "%d", &copies);		break;
		case 'm': mailme  = TRUE;				break;
		case 's': silent  = TRUE;				break;
		case 'w': writeme = TRUE;				break;
		case 'R': reprint = TRUE;				break;
		case 'S': exit((shutdown_scheduler()) ? EXIT_SUCCESS : EXIT_FAILURE);
		default:
			fprintf(stderr,
				"Usage: lp [ option ... ] [ file ... ]\n"
				"Options:\n"
				"\t-dprinter\tUse specified printer.\n"
				"\t-ttitle\t\tUse given title.\n"
				"\t-m\t\tSend mail when job is done.\n"
				"\t-ncopies\tPrint multiple copies.\n"
				"\t-R req beg end\tReprint request from beg to end.\n"
				"\t-s\t\tSilent operation.\n"
				"\t-S\t\tShutdown the MLP scheduler.\n"
				"\t-w\t\tWrite message when job is done.\n"
				);
			exit(EXIT_FAILURE);
		}

	}

	get_environment();

	if (reprint) {

		seqnum = start = end = 0;
		sscanf(argv[optind],   "%d", &seqnum);
		if (optind + 1 < argc)
			sscanf(argv[optind+1], "%d", &start);
		if (optind + 2 < argc)
			sscanf(argv[optind+2], "%d", &end);

		reprint_request(seqnum, start, end);

	} else if (optind < argc) {

		for ( ; optind < argc; optind++) {

			if (!strequal(argv[optind], "-")) {
				if ((fd = open(argv[optind], O_RDONLY)) != BAD) {
					dup2(fd, 0);
					close(fd);
					memset(path, 0, sizeof(path));
					strcpy(path, argv[optind]);
					spool_output();
					has_spooled = 1;
				} else
					error("cannot open file \"%s\"", argv[optind]);
			}
		}

	} else {

		input_from_stdin = TRUE;
		spool_output();
		has_spooled = 1;

	}

	if (has_spooled)
		echo_command();

	wakeup_scheduler();
	exit(EXIT_SUCCESS); 

}

/* end of mlp/lp.c */
