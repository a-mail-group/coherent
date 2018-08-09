/* mlp/spooler.c */

/******************************************************************************
	Various common routines used by the MLP spooler system.
******************************************************************************/

#include "mlp.h"

/* Statics. */
#if	USE_SEM
static	int	sID = BAD;
#endif

/* Globals. */
int	abort_write = FALSE;
char	curr_backend[WORKSTR];
int	curr_copies;
int	curr_copy;
char	curr_device[WORKSTR];
int	curr_formlen;
char	curr_name[NNAME];
char	curr_printer[WORKSTR];
char	progname[16];


/******************************************************************************
	Error message handling:
		error()		prints a message to stderr
		warning_user()	mails a message to the specified user
		warning()	mails a message to the current user (or root)
		fatal()		mails a message to root and dies
	Reset SIGCLD to SIG_DFL when sending mail, else mail complains.
******************************************************************************/

void
error(va_alist) va_dcl
{
	va_list args;

	va_start(args);  
	fprintf(stderr, "%s: %r\n", progname, args);
	va_end(args);
}


void
warning_user(va_alist) va_dcl
{
	va_list args;
	char	buf1[NBUF], buf2[NBUF], *user;
	int	(*sigcld)();

	va_start(args);  
	user = va_arg(args, char *);
	sprintf(buf2, "%r", args);
	va_end(args);
	log("%s", buf2);

	strcpy(buf1, "/bin/echo \"\n`date`\n\"'\n*** Message From The MLP Spooler ***\n\n");
	strcat(buf1, buf2); 
	strcat(buf1, (getenv("MLPLMAIL")) ? "\n\n' | lmail " : "\n\n' | mail ");
	strcat(buf1, user);
	sigcld = signal(SIGCLD, SIG_DFL);
	system(buf1);
	signal(SIGCLD, sigcld);
}


void
warning(va_alist) va_dcl
{
	va_list	args;

	va_start(args);
	warning_user(get_username(), "%r", args);
	va_end(args);
}

void
fatal(va_alist) va_dcl
{
	va_list	args;

	va_start(args);
	warning_user("root", "%r", args);
	va_end(args);
	sleep(2);			/* let system() complete sending mail */
	exit(EXIT_FAILURE);
}


/******************************************************************************
	Convert args to string like sprintf(),
	return pointer to statically allocated string.
******************************************************************************/

char *
string(va_alist) va_dcl
{
	static	char	buf[WORKSTR];
	va_list args;

	va_start(args);
	sprintf(buf, "%r", args);
	va_end(args);
	return buf;
}


/******************************************************************************
	Make a full pathname from a directory and a filename.
	Return pointer to statically allocated buffer;
	don't call it twice and expect to use both return values.
******************************************************************************/

char *
makepath(dir, name) register char *dir, *name;
{
	static	char	path[WORKSTR];

	strcpy(path, dir);
	strcat(path, "/");
	return strcat(path, name);
}

char *
makepath2(dir, name) register char *dir, *name;
{
	static	char	path[WORKSTR];

	strcpy(path, dir);
	strcat(path, "/");
	return strcat(path, name);
}


/******************************************************************************
	Return the name of the invoker.
******************************************************************************/

char *
get_username()
{
	register char *s;
	register struct passwd *pwp;

	if ((s = getlogin()) != NULL)
		return s;
	else if ((pwp = getpwuid(getuid())) != NULL)
		return pwp->pw_name;
	else
		return "root";
}

/******************************************************************************
	Removes the leading and trailing spaces from the given string.  First
	scan to the end of the string. Then eat the right hand spaces.  Then
	eat the left hand spaces.  Then copy the unpadded string back into its
	original place.
******************************************************************************/

char *
unpadd(s) register char *s;
{
	register char *beg, *end;

	/* Skip leading spaces; beg points to first nonspace (or NUL). */
	for (beg = s; *beg == ' '; ++beg)
		;

	/* Delete trailing spaces; end points to the NUL. */
	for (end = beg + strlen(beg); end > beg && end[-1] == ' '; )
		*--end = '\0';

	/* Copy to destination; memmove() allows overlapping. */
	return memmove(s, beg, end - beg + 1);
}

/******************************************************************************
	Rename a file.
	N.B. this unlinks new before trying the rename, unlike system call rename().
******************************************************************************/

int 
rename(old, new) char *old; char *new;
{
	unlink(new);
	if (link(old, new) == BAD)
		return 2;
	if (unlink(old) == BAD)
		return 3;
	return 0;
}


/*****************************************************************************
	Put a request in the inactive queue.
*****************************************************************************/

void
make_inactive(name) char *name;
{
	char newname[NNAME];

	if (*name == 'r')
		return;		/* already inactive */
	strcpy(newname, name);
	newname[0] = 'r';
	rename(makepath2(QUEUE_PATH, name), makepath(QUEUE_PATH, newname));
} 


/******************************************************************************
	Get/Set header information.
	Set the info if value is not NULL, otherwise get the info.
	STEVE: Blatchley has apparently not heard of the struct.
******************************************************************************/

char *
gethdrval(hdr, opt) char *hdr; HDR_FIELD opt;
{
	return hdrval(hdr, opt, NULL);
}

char *
getuhdrval(hdr, opt) char *hdr; HDR_FIELD opt;
{
	return unpadd(hdrval(hdr, opt, NULL));
}


char *
hdrval(hdr, opt, value) char *hdr; HDR_FIELD opt; char *value;
{
	static	char	s[HEADER_SIZE];
	int		offset, length, vallen;

	switch (opt) {
	case H_USER:		offset =  0;	length = 14;	break;
	case H_PRINTER:		offset = 14;	length = 14;	break;
	case H_TYPE:		offset = 28;	length = 10;	break;
	case H_FORMLEN:		offset = 38;	length =  3;	break;
	case H_PAGES:		offset = 41;	length =  4;	break;
	case H_COPIES:		offset = 45;	length =  2;	break;
	case H_LONGEVITY:	offset = 47;	length =  1;	break;
	case H_MAILME:		offset = 48;	length =  1;	break;
	case H_WRITEME:		offset = 49;	length =  1;	break;
	case H_DBASE:		offset = 50;	length = 14;	break;
	case H_FORMAT:		offset = 64;	length = 14;	break;
	case H_DSTAMP:		offset = 78;	length = 10;	break;
	case H_DESC:		offset = 88;	length = 60;	break;
	case H_ENTIRE:		offset =  0;	length = HEADER_SIZE-2; 
				hdr[HEADER_SIZE-1] = '\n';
				hdr[HEADER_SIZE] = 0;
				break;
	}

	if (value != NULL) {
		vallen = (strlen(value) <= length) ? strlen(value) : length;
		memset(&hdr[offset], ' ', length);
		strncpy(&hdr[offset], value, vallen);
	}

	strncpy(s, &hdr[offset], length);
	s[length] = 0;
	return s;
}


/******************************************************************************
	Log text messages into the log file.
******************************************************************************/

void
log(va_alist) va_dcl
{
	va_list		args;
	FILE		*fp;
	time_t		clock;
	struct	tm	*tt;

	time(&clock);
	tt = localtime(&clock);

	va_start(args);
	MLP_lock(L_LOG);
	if ((fp = fopen(LOG_PATH, "a+")) != NULL) {
		fprintf(fp,
			"%02d/%02d/%02d %02d:%02d - %s\n",
			tt->tm_mon + 1, tt->tm_mday, tt->tm_year,
			tt->tm_hour, tt->tm_min,
			string("%r", args)
			);
		fclose(fp);
	}
	MLP_unlock(L_LOG);
	va_end(args); 
}


/******************************************************************************
	Convert string in place to uppercase.
******************************************************************************/

char *
uppercase(str) register char *str;
{
	register char *c;

	for (c = str; *c; c++)
		if (islower(*c))
			*c = toupper(*c);
	return str;
}


/******************************************************************************
  Like strtok() but with fixed second arg of SEP.
******************************************************************************/

char *
strtoksep(s) register char *s;
{
	return strtok(s, SEP);
}


/******************************************************************************
  Given the name of a control or status file parameter, return its value.
  Lock the file so we can read/write it without it changing on us.
  If the value of the string "newval" is non-null,
  then assign the parameter this new new value.
  STEVE 8/24/94: this was originally implemented without PARAMTYPE, which
  meant e.g. lp.c could not lock the sequence number correctly, yuck.
******************************************************************************/

char *
param(name, newval, locktype, paramtype)
char *name;
char *newval;
LOCKTYPE locktype;
PARAMTYPE paramtype;
{
	static	char	value[WORKSTR];
	register FILE	*fp, *ofp;
	register char	*c;
	char		*pathname;
	char		tmp[WORKSTR], tmp1[WORKSTR], parameter[NNAME], tmppath[WORKSTR];

	pathname = (locktype == L_CONTROL) ? CONTROL_PATH : STATUS_PATH;

	sprintf(tmppath, "%s.tmp", pathname);
	strncpy(parameter, name, sizeof(parameter));  uppercase(parameter);
	value[0] = '\0';

	if (paramtype != P_SETULOCK)
		MLP_lock(locktype);		/* locked already if P_SETULOCK */

	fp = xopen(pathname, "r", (locktype == L_STATUS));

	/* Opened for read, look for param or write new value. */
	if (paramtype == P_GET || paramtype == P_GETLOCK) {

		/* Look for existing value. */
		while (!feof(fp)) {
			if (fgets(tmp, sizeof(tmp), fp)) {
				for (c = tmp; *c && *c != '#'; c++)
					; /* look for comment */
				if (*tmp && !*c) {
					c = strtoksep(tmp);  uppercase(c);
					if (strequal(c, parameter)) {
						c = strtok(NULL, "\n");
						while (*c && (*c == ' ' || *c == '\t' || *c == '='))
							c++;
						strcpy(value, c);
						break;
					}
				} /* if (*tmp...) */
			} /* if (fgets(...) */
		} /* while() */
		fclose(fp);
	} else { /* i.e. (paramtype == P_SET || paramtype == P_SETULOCK) */

		/* Add or modify value. */
		if ((ofp = fopen(tmppath, "w")) == NULL) {
			 /* Cannot write temp file, bomb out. */
			fclose(fp);
			fatal("Cannot create \"%s\"", tmppath);
		}
		while (!feof(fp)) {

			if (fgets(tmp, sizeof(tmp), fp))
				strcpy(tmp1, tmp);
			else
				tmp1[0] = '\0';

			if (*tmp && *tmp != '#') {
				c = strtoksep(tmp);
				uppercase(c);
				if (!strequal(c, parameter)) {
					fprintf(ofp, "%s", tmp1); /* copy the line */
				}
			} else {
				fprintf(ofp, "%s", tmp1);	/* copy comment or blank */
			}
		} /* while (!feof(...)) */

		/* Append the new value. */
		fprintf(ofp, "%s = %s\n", name, newval);
		fclose(ofp);
		fclose(fp);
		rename(tmppath, pathname);

	}				/* if (newval == NULL) ... else ... */

	if (paramtype != P_GETLOCK)
		MLP_unlock(locktype);	/* leave locked for later P_SETULOCK if P_GETLOCK */
	return value;
}


/******************************************************************************
	Given a user name, return the name of the printer to which they are cur-
	rently routed.
******************************************************************************/

char *
getroute(user) char *user;
{
	static	 char printer[NPRINTER];
	FILE	*fp;
	char	*c;

	printer[0] = '\0';
	if ((fp = fopen(makepath(ROUTE_PATH, user), "r")) != NULL) {
		fgets(printer, sizeof(printer), fp);
		fclose(fp);
	} else {
		strcpy(printer, getcontrol(DEFAULT));
		if (printer[0] == 0)
			fatal("No default printer entry in file \"%s\"", CONTROL_PATH);
	}
	if (c = strchr(printer, '\n'))
		*c = 0;
	return printer;
}


/*****************************************************************************
	Return a sorted list of the spool queue.
	The data structure returned contains n entries of MAXNAMLEN each,
	followed by "" as an end marker.
*****************************************************************************/

static
int
namecmp(p1, p2) register char *p1, *p2;
{
	return strncmp(p1, p2, MAXNAMLEN);
}

static
int
rnamecmp(p1, p2) register char *p1, *p2;
{
	return -strncmp(p1, p2, MAXNAMLEN);
}

char *
queuelist(rflag) int rflag;
{
	static	int		qfiles = 128;	/* max files in queue dir */
	register DIR		*dr;
	register struct	dirent	*dp;
	char			*qlist;
	register int		n;

	qlist = xalloc(qfiles * MAXNAMLEN);
	for (n = 0, dr = opendir(QUEUE_PATH); (dp = readdir(dr)) != NULL; ) {

		if (strequal(".", dp->d_name) || strequal("..", dp->d_name))
			continue;
		if (n == qfiles - 1) {
			qfiles *= 2;
			if ((qlist = realloc(qlist, qfiles * MAXNAMLEN)) == NULL)
				fatal("Out of space for queue list");
		}
		strncpy(qlist + n * MAXNAMLEN, dp->d_name, MAXNAMLEN);
		++n;
	}
	closedir(dr);
	strcpy(qlist + n * MAXNAMLEN, "");
	if ((qlist = realloc(qlist, (n + 1) * MAXNAMLEN)) == NULL)
		fatal("Queue list realloc failed");
	qsort(qlist, n, MAXNAMLEN, (rflag) ? rnamecmp : namecmp);
	return qlist;
}


/******************************************************************************
	Wakeup the scheduler by sending it an alarm signal.  The scheduler's PID
	is found in the status file.
******************************************************************************/

int
wakeup_scheduler()
{
	int	 pid;

	pid = 0;
	sscanf(getstatus(DESPOOLER), "%d", &pid);
	if (pid > 0)
		kill(pid, SIGALRM);
	return (pid > 0);
}


/******************************************************************************
	Get program name.  Strip the leading path information.
	Make sure the program is running with euid == daemon,
	so that it will have appropriate permissions on spool directories.
******************************************************************************/

void
getname(path) register char *path;
{
	register char		*s;
	register struct	passwd	*pwp;

	/* Set umask. */
	umask(UMASK_PERMS);

	/* Get program name to progname; must precede error() call below. */
	if (path == NULL)
		return;
	if ((s = strrchr(path, '/')) == NULL)
		s = path;
	else
		++s;
	strcpy(progname, s);

	/* Make sure running with euid == daemon. */
	if ((pwp = getpwnam("daemon")) == NULL || pwp->pw_uid != geteuid()) {
		error("not running euid==daemon, file permissions should be setuid");
		exit(EXIT_FAILURE);
	}
}


/******************************************************************************
	Return TRUE if the scheduler is running, FALSE if not.
******************************************************************************/

int
scheduler_status()
{
	int	pid;

	pid = 0;
	sscanf(getstatus(DESPOOLER), "%d", &pid);
	return (pid > 0 && !kill(pid, 0));
}

/******************************************************************************
	This routine does all the actual I/O to the device.  The tricky part is
	counting the pages and (optionally printing only the desired range of pages).
	It is used in several places.  Source and Sink are the input and output
	file descriptors.  The source's file pointer is expected to be at the
	start of the user's data; that is, seek past the header, if there is one.
	Start and end are the starting and ending page numbers for the reqeuest.
	If both of these are zero then we will send all of the source data to the
	sink, otherwise we send source to sink only when we are within the page
	range.  Counting pages depends heavily on the form length (curr_formlen)
	and the nature of the source data.  If you are sending bitmaps to a laser
	printer though this routine, the page numbers will probably be wildly
	skewed!  At this point, I know of no convienent way around this.  You'd
	have to parse the input for ALL known laser printers. Then the next new
	one could possibly make your effort moot.  Maybe this could be done for
	HPCL and Postscript?  If the flag "report" is TRUE then the routine's
	progress will be reported in the status file.  This routine returns the
	number of pages that it has processed, or -1 if aborted.
******************************************************************************/

#define	IOBUFF 1024

int
writeOut(ifd, ofd, start, end, report)
int ifd;
int ofd;
int start;
int end;
int report;
{
	int		result, lines, progress;
	int		pages;
	long		rsize, oldpos;
	register char	*i, *ibuff, *imax;	/* input buffer pointers */
	register char	*o, *obuff, *omax;	/* output buffer pointers */

	/* how big is the source file? */
	oldpos = lseek(ifd, 0L, SEEK_CUR);
	rsize  = lseek(ifd, 0L, SEEK_END);
	lseek(ifd, oldpos, SEEK_SET);

	ibuff = xalloc(IOBUFF);
	o = obuff = xalloc(IOBUFF);
	omax = obuff + IOBUFF;

	lines = 0;  pages = 1;
 
	while (result = read(ifd, ibuff, IOBUFF)) {
		if (abort_write) {
			o = obuff;
			pages = -1;
			break;
		}

		if (start == 0 && end == 0)
			write(ofd, ibuff, result);	/* print entire request */
		else {

			/* print from start to end page of request */

			for (i = ibuff, imax = ibuff + result; i < imax; i++) {

				if (*i == '\n') {
					lines++;
					if (lines > curr_formlen) {
						lines = 0;
						/* lines -= formlen; */
						pages++;
					}
				} else if (*i == '\f') {
					pages++;
					lines = 0;
				}
				if (pages >= start && pages <= end) {

					*o++ = *i;
					if (o >= omax) {
						write(ofd, obuff, IOBUFF);
						o = obuff;
					}
				}
			}
		}


		/* Report Progress to the status file. */
		if (report) {
			progress = (int) (lseek(ifd, 0L, SEEK_CUR) * 100L / rsize);
			setstatus(curr_device,
				string("%d, %s, %s, %d, %d of %d",
					getpid(), curr_name,
					curr_printer, progress,
					curr_copy, curr_copies)
				);
		}
	}

	/* write anything leftover in the output buffer */

	if (o != obuff) write(ofd, obuff, (int) (o - obuff)); 

	free(ibuff);  free(obuff);

	return pages;
}


/******************************************************************************
	Given the sequence number of a request, return the file name of
	the request, or "" if not found.
******************************************************************************/

char *
request_name(seqnum) char *seqnum;
{
	DIR		*dr;
	struct	dirent	*dp;
	char		*val;
	static	char	value[MAXNAMLEN + 1];

	value[0] = 0;
	val = string("%05u", atoi(seqnum));
	for (dr = opendir(QUEUE_PATH); (dp = readdir(dr)) != NULL; ) {
		if (strequal(".", dp->d_name) || strequal("..", dp->d_name))
			continue;
		else if (!strncmp(val, &dp->d_name[2], 5)) {
			strncpy(value, dp->d_name, MAXNAMLEN);
			value[MAXNAMLEN] = '\0';
			break;
		}
	}
	closedir(dr);
	return value;
}


/****************************************************************************
	Read an MLP header from the given file.
****************************************************************************/

int
read_hdr(name, hdr) register char *name, *hdr;
{
	register int fd;

	if ((fd = open(name, O_RDONLY)) == BAD)
		return 0;
	read(fd, hdr, HEADER_SIZE);
	close(fd);
	return 1;
}

/****************************************************************************
	Given the report sequence number, return the owner's name
****************************************************************************/

char *
request_owner(seqnum) char *seqnum;
{
	static	char	owner[NNAME];
	char		head[HEADER_SIZE];

	strcpy(owner, request_name(seqnum));
	if (*owner != '\0'
	 && read_hdr(makepath(QUEUE_PATH, owner), head))
		strcpy(owner, getuhdrval(head, H_USER));
	return owner;
}


/****************************************************************************
  Return 1 if I (the program's user) own the request, 0 otherwise. If I'm
  superuser, then I own it all.
****************************************************************************/

int
do_i_own_it(seqnum) char *seqnum;
{
	struct	passwd *p;

	p = getpwuid(getuid());
	return ((p && strequal(p->pw_name, request_owner(seqnum))) || is_root());
}

/******************************************************************************
	Report the status of all the printers defined in the control file.
******************************************************************************/

char *
each_printer(command) PRINTER_CMD command;
{
	static	FILE	*fp;
	static	char	line[WORKSTR];
	char		tmp[WORKSTR], *t;

	*line = 0;

	switch (command) {

	case E_START:
		MLP_lock(L_CONTROL);
		fp = xopen(CONTROL_PATH, "r+", 0);
		break;

	case E_NEXT:
		while (fgets(tmp, sizeof(tmp), fp)) {

			t = strtoksep(tmp); uppercase(t);
			if (strequal("PRINTER", t)) {
				for (t = tmp; *t++; )
					;
				while (strchr(SEP, *t))
					t++;
				strcpy(line, t);
				break;
			}
		}
		break;

	case E_REWIND:
		fseek(fp, 0L, SEEK_SET);
		break;

	case E_END:
		fclose(fp);
		/* and fall through... */

	case E_UNLOCK:
		MLP_unlock(L_CONTROL);
		break;
	}

	return line;
}

char *
report_printer_status(stype) STATUS_TYPE stype;
{
	static	char	request_list[WORKSTR];
	int		pid;
	char		tmp[WORKSTR], *t, *c, *d;
	char		request[NNAME], user[NNAME], timestr[NTIME];
	char		hdr[HEADER_SIZE];
	long		rtime;

	request_list[0] = 0;	/* will contain a list of all the request that are
				 currently printing.  This is so we can skip these
				 when we go over the list of all pending requests.*/

	each_printer(E_START);

	while (*(t = each_printer(E_NEXT))) {

		*tmp = *curr_printer = *curr_device = *curr_backend = 0;

		c = strtoksep(t	);   if (c) strncpy(curr_printer, c, WORKSTR);
		c = strtoksep(NULL); if (c) strncpy(curr_device,  c, WORKSTR);
		c = strtoksep(NULL); if (c) strncpy(curr_backend, c, WORKSTR);

		/*** Now look at this printer from the status database ***/
		strcpy(tmp, getstatus(curr_device));

		c = strtoksep(tmp);  pid = 0; sscanf(c, "%d", &pid);

		if (pid > 0 && kill(pid, 0)) c = NULL;

		switch (stype) {

		case PID_DISPLAY:
			if (c != NULL) {
				c = strtoksep(NULL);
				if (c) sprintf(timestr, "%d|%s ", pid, (c+2));

				if (strlen(request_list) + strlen(timestr)
						< sizeof(request_list))
					strcat(request_list, timestr);
			}
			break;

		case DEVICE_DISPLAY:
			printf("Device for %s:\t%s\n", curr_printer, curr_device);
			break;

		case PRINTER_DISPLAY:
			*request = 0; if (d = strtoksep(NULL)) strcpy(request, d);

			d = strtoksep(NULL);

			if (c == NULL || d == NULL || !strequal(d, curr_printer))
				printf("Printer %s:\tidle\n", curr_printer);
			else {
				printf("Printer %s:\tprinting request #", curr_printer);
				printf("%s, ", &request[2]);

				if (c = strtoksep(NULL))
					printf("%s%% sent to device", c);

				if (c = strtok(NULL, ","))
					printf(", copy %s", c);

				printf("\n");
			}
			break;

		case REQUEST_DISPLAY:
			if (c == NULL)
				break;
			strcpy(request, strtoksep(NULL));

			if ((c = strtoksep(NULL)) && !strcpy(c, curr_printer)) {
				  
				if (!read_hdr(makepath(QUEUE_PATH, request), hdr))
					break;

				strcpy(user, getuhdrval(hdr, H_USER));
				strcpy(timestr, gethdrval(hdr, H_DSTAMP));
				rtime = 0L; sscanf(timestr, "%ld", &rtime);
				strcpy(timestr, ctime(&rtime));
				timestr[strlen(timestr)-1] = 0;

				printf("%s-%s\t\t%c\t%s", user,
					(request+2), *(request+1), timestr);

				printf("\ton %s\n", curr_printer);

				sprintf(timestr, "%s|", request);
				if (strlen(request_list) + strlen(timestr)
					  < sizeof(request_list))
					strcat(request_list, timestr);
			}
			break;
		}
	}

	each_printer(E_END);
	return request_list;
}



/******************************************************************************
	Cancel currently printing requests (not pending ones!).  Kill only the ones
	I own.  If I'm superuser, then I own it all.
******************************************************************************/

int
kill_printing_request(seqnum, sig) int seqnum, sig;
{
	int	pid, seq, status;
	char	*list, *t;

	status = FALSE;
	list = report_printer_status(PID_DISPLAY);

	t = strtok(list, " ");

	do {
		if (t) {
			sscanf(t, "%d|%d", &pid, &seq);

			if ((seqnum == 0 || (seq == seqnum))
			 && do_i_own_it(string("%05u", seqnum))) {
				kill(pid, sig);
				status = TRUE;
			}

			t = strtok(NULL, " ");
		}
	} while (t);

	if (seqnum == 0)
		log("Aborting currently printing requests");

	return status;
}


/******************************************************************************
	MLP_lock.  This routine can lock several critical sections involving MLP
	process.  These are: Cancel, Controls and Status.  The controls and status
	files need to be locked while they are modified.  The Cancel lock keeps
	processes that cancel requests out of the hair of the scheduler and vice-
	versa.

	***NOTE***
	Because Mark Williams has not implemented SEM_UNDO, we will fall back to
	lockfiles for safety reasons.

	 STEVE: the above comments are Brett's, from the original source.
	 The locking in his source did not work correctly.
	 His source also included commented-out code implementing locking with
	 semget()/semop(), but this code (retained #if USE_SEM below) did not work.
	 The calling routines should check the returned status, obviously...
******************************************************************************/

#if	USE_SEM
/*
 * Code using semget() and semop(), from original source.
 * STEVE: tried this briefly 8/16/94,
 * it does not seem to work, did not pursue at length.
 */
int
MLP_lock(which) LOCKTYPE which;
{
	static	struct	sembuf lcancel[2]  = { 0,0,0,  0,1,SEM_UNDO };
	static	struct	sembuf lcontrol[2] = { 1,0,0,  1,1,SEM_UNDO };
	static	struct	sembuf lstatus[2]  = { 2,0,0,  2,1,SEM_UNDO };
	static	struct	sembuf llog[2]	  = { 3,0,0,  3,1,SEM_UNDO };
	key_t k;

	if (sID == BAD) {
		/* First time, execute semget(). */
		k = ftok(IPC_NAME, 'A');
		if ((sID = semget(k, IPC_SEMS, IPC_PERMS|IPC_CREAT|IPC_EXCL)) == BAD
		 && (sID = semget(k, IPC_SEMS, 0)) == BAD)
			fatal("Cannot get semaphores");
	}

	switch (which) {
		case L_CANCEL:  semop(sID, lcancel,  2); break;
		case L_CONTROL: semop(sID, lcontrol, 2); break;
		case L_STATUS:  semop(sID, lstatus,  2); break;
		case L_LOG:	semop(sID, llog,     2); break;
	}
}

int
MLP_unlock(which) LOCKTYPE which;
{
	static	struct	sembuf lcancel[1]  = { 0,-1, IPC_NOWAIT|SEM_UNDO };
	static	struct	sembuf lcontrol[1] = { 1,-1, IPC_NOWAIT|SEM_UNDO };
	static	struct	sembuf lstatus[1]  = { 2,-1, IPC_NOWAIT|SEM_UNDO };
	static	struct	sembuf llog[1]     = { 3,-1, IPC_NOWAIT|SEM_UNDO };
#if	0
	key_t k;

	k = ftok(IPC_NAME, 'A');

	if ((sID = semget(k, IPC_SEMS, IPC_PERMS|IPC_CREAT|IPC_EXCL)) == BAD) {
		if ((sID = semget(k, IPC_SEMS, 0)) == BAD) {
			return;
		}
	}
#else
	if (sID == BAD)
		fatal("No semaphores in MLP_unlock()");
#endif
	switch (which) {
	case L_CANCEL:  semop(sID, lcancel,  1); break;
	case L_CONTROL: semop(sID, lcontrol, 1); break;
	case L_STATUS:  semop(sID, lstatus,  1); break;
	case L_LOG:	semop(sID, llog,     1); break;
	}
}

#else					/* !USE_SEM */

/*
 * STEVE: this code uses lockf() rather than semget()/semop().
 * It did not work at all as originally written, now is better but not ideal.
 */
int locks[NLOCKS];			/* lockfile fd's */

int
MLP_lock(which) LOCKTYPE which;
{
	char path[WORKSTR];

	sprintf(path, "%s/LCK..%d", SPOOLDIR, which);
	if ((locks[which] = open(path, O_RDWR|O_CREAT, CREAT_PERMS)) != BAD) {
		/*
		 * Opened successfully, try to lock it.
		 * Try again if lockf() fails with EINTR.
		 */
		for (;;) {
			errno = 0;
			if (lockf(locks[which], F_LOCK, 0L) != BAD)
				return 1;			/* success */
			if (errno != EINTR)
				break;				/* failed */
		}
	}

	/* Failure. */
	/* N.B. no close(locks[which]) here because MLP_unlock will close it. */
	warning_user("root", "MLP_lock(%s) failed (fd=%d, errno=%d)",
		path, locks[which], errno);
	return 0;
}

int
MLP_unlock(which) LOCKTYPE which;
{
	char path[WORKSTR];

	sprintf(path, "%s/LCK..%d", SPOOLDIR, which);
	errno = 0;
	if (locks[which] == BAD)
		return 0;			/* fails but message unnecessary */
	else if (lockf(locks[which], F_ULOCK, 0L) != BAD
		 && close(locks[which]) != BAD)
		return 1;			/* success */
	warning_user("root", "MLP_unlock(%s) failed (fd=%d, errno=%d)",
		path, locks[which], errno);
	return 0;				/* failure */
}

#endif				/* #if	USE_SEM ... #else ... */


/******************************************************************************
 Allocate space, die on failure.
******************************************************************************/

char *
xalloc(size) unsigned int size;
{
	register char *s;

	if ((s = calloc(size, 1)) == NULL)
		fatal("Out of space");
	return s;
}

/*
 * Open FILE.
 * If file does not exist, die if !flag or create it if flag.
 * Die on error.
 * Give permission message if EACCESS.
 */
FILE *
xopen(name, mode, flag) register char *name, *mode; int flag;
{
	register FILE *fp;

	if ((fp = fopen(name, mode)) != NULL)
		return fp;
	else if (flag) {
		if ((fp = fopen(name, "a+")) != NULL) {
			warning_user("root", "File \"%s\" created.", name);
			return fp;
		}
	}
	if (errno == EACCES)
		fatal("Permission problem: \"%s\" should have the setuid bit set.",
			progname);
	else
		fatal("Cannot open file \"%s\"", name);

}

/* end of mlp/spooler.c */
