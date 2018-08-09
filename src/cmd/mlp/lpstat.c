/* mlp/lpstat.c */

/******************************************************************************
	cancel/chreq/lpadmin/lpstat/route.
	MLP line-printer status a la Unix V.
******************************************************************************/

#include "mlp.h"

/* Flag arguments to adjust_requests(). */
typedef	enum	adj_flag { A_LIFETIME, A_PRINTER } ADJ_FLAG;


/******************************************************************************
  Report the name of the system default printer
******************************************************************************/

void
system_default_printer()
{
	printf("Default printer:\t%s\n", getcontrol(DEFAULT));
}


/******************************************************************************
  Report whether the scheduler is running or not
******************************************************************************/

void
report_scheduler_status()
{
	printf("Scheduler:\t%srunning\n", (scheduler_status()) ? "" : "not ");
}


/******************************************************************************
  List the currently printing requests and the pending ones in Unix V style
******************************************************************************/

void
request_status()
{
	char	*requests, *qlist, *name, hdr[HEADER_SIZE];
	char	user[NNAME], timestr[NTIME];
	long	rtime;
	struct	passwd	*p;

	p = getpwuid(getuid());

	requests = report_printer_status(REQUEST_DISPLAY);

	for (name = qlist = queuelist(FALSE); *name != '\0'; name += MAXNAMLEN) {
		if (*name == 'r')
			break;			/* don't list inactive jobs */

		if (strstr(requests, name) == NULL) {

			if (!read_hdr(makepath(QUEUE_PATH, name), hdr))
				continue;

			strcpy(user, getuhdrval(hdr, H_USER));

			if ((p && strequal(user, p->pw_name)) || is_root()) {

				strcpy(timestr, gethdrval(hdr, H_DSTAMP));
				rtime = 0L; sscanf(timestr, "%ld", &rtime);
				strcpy(timestr, ctime(&rtime));
				timestr[strlen(timestr)-1] = 0;

				printf("%s-%s\t\t%c\t%s", user,
					(name+2), *(name+1), timestr);

				printf("\tfor %s\n", getuhdrval(hdr, H_PRINTER));
			}
		}
	}
	free(qlist);
}


/******************************************************************************
  List the currently printing requests and the pending ones in MLP style
******************************************************************************/

void
mlp_status()
{
	int	printing, val;
	char	*requests, *qlist, *name, hdr[HEADER_SIZE];
	char	user[NNAME], printer[NNAME], title[WORKSTR];
	long	rtime;
	struct	tm	*tt;

	printf(
		"REQ # S P L #Pgs FL  #Cp   Date   Time  Description...\n"
		"----- - - - ---- --- --- -------- ----- -----------------------------------\n"
		);

	for (name = qlist = queuelist(TRUE); *name != '\0'; name += MAXNAMLEN) {
		printing = FALSE;

		if (strstr(requests, name)) printing = TRUE;

		if (read_hdr(makepath(QUEUE_PATH, name), hdr)) {

			printf("%-5s ", (name+2));	/* request sequence no. */

			/* Request Status */

			if (printing)
				printf("* ");	/* printing now */
			else if (*name == 'R')
				printf("P ");	/* waiting to print */
			else if (*name == 'r')
				printf("Q ");	/* held in queue print */
			else
				printf("? ");	/* don't know the status */

			printf("%c ", *(name+1));	/* priority */

			printf("%s ", getuhdrval(hdr, H_LONGEVITY));

			val = 0;
			sscanf(getuhdrval(hdr, H_PAGES), "%d", &val);
			printf("%4d ", val);
	 
			val = 0;
			sscanf(getuhdrval(hdr, H_FORMLEN), "%d", &val);
			printf("%3d ", val);
	 
			val = 0;
			sscanf(getuhdrval(hdr, H_COPIES), "%d", &val);
			printf("%3d ", val);

			sscanf(getuhdrval(hdr, H_DSTAMP), "%ld", &rtime);

			tt = localtime(&rtime);

			printf("%02d/%02d/%02d ", (tt->tm_mon+1), tt->tm_mday, tt->tm_year);
			printf("%02d:%02d ", tt->tm_hour, tt->tm_min);
	 
			strcpy(user,	getuhdrval(hdr, H_USER));
			strcpy(printer, getuhdrval(hdr, H_PRINTER));
			strcpy(title,	getuhdrval(hdr, H_DESC));

			printf("%s -by %s -on %s\n", title, user, printer);
		}
	}
	free(qlist);
}


/******************************************************************************
  Print a list of printers available for routine.
******************************************************************************/

void
list_printers(user) char *user;
{
	char *list;

	printf("%s  (", getroute(user));

	each_printer(E_START);

	while (*(list = each_printer(E_NEXT)))
		printf("%s ", strtoksep(list));

	each_printer(E_END);

	printf("system display local )\n");
}


/******************************************************************************
  Set the printer that the user wants to route to.
******************************************************************************/

void
set_route(user, printer) char *user, *printer;
{
	FILE	*fp;
	int	inlist;
	char	*list;

	each_printer(E_START);

	inlist = FALSE;

	while (*(list = each_printer(E_NEXT)))
		if (strequal(strtoksep(list), printer))
			inlist = TRUE;

	each_printer(E_END);

	if (strequal("local",	printer)
	 || strequal("display", printer)
	 || strequal("system",  printer))
		inlist = TRUE;

	if (!inlist) {
		error("\"%s\" is not a defined printer", printer);
		return;
	}

	/* Write an entry for user in the route directory. */
	if ((fp = fopen(makepath(ROUTE_PATH, user), "w+")) != NULL) {
		fprintf(fp, "%s\n", printer);
		fclose(fp);
		printf("%s, your requests will now spool to the \"%s\" printer.\n", 
			user, printer);
	}
}


/******************************************************************************
	Reprioritize the requests
******************************************************************************/

void
reprioritize_requests(argc, argv) int argc; char *argv[];
{
	int		m, reqnum, filenum;
	char		*qlist, *name, prior, *oldnam, *head, auser[NNAME];
	struct	passwd	*p;

	p = getpwuid(getuid());

	prior = argv[1][2];

	if (prior < '0')
		prior = '0';
	else if (prior > '9')
		prior = '9';

	MLP_lock(L_CANCEL);

	/* Not sure why I want to do this? */
	/* kill_printing_request(0, R_CANCEL); */

	head = xalloc(HEADER_SIZE);

	for (name = qlist = queuelist(FALSE); *name != '\0'; name += MAXNAMLEN) {

		if (sscanf(&name[2], "%d", &filenum) != 1)
			continue;

		for (m = 2; m <= argc; m++) {

			if (sscanf(argv[m], "%d", &reqnum) != 1 || filenum != reqnum)
				continue;

			if (!read_hdr(oldnam = makepath(QUEUE_PATH, name), head))
				continue;

			strcpy(auser, getuhdrval(head, H_USER));

			if ((p && strequal(auser, p->pw_name)) || is_root()) {

				name[1] = prior;
				rename(oldnam, makepath2(QUEUE_PATH, name));

				if (*name == 'R') {
					/* make_inactive(name); */
					kill_printing_request(reqnum, R_CANCEL);
					warning("Request #%d aborted due to a change in priority", 
						reqnum);
				}
			}
		}
	}

	free(head);
	MLP_unlock(L_CANCEL);
	free(qlist);
}


/******************************************************************************
	Change the longevity or the printer
******************************************************************************/

void
adjust_requests(argc, argv, flag) int argc; char *argv[]; ADJ_FLAG flag;
{
	int	m, reqnum, filenum, fd;
	char	*qlist, *name, oper[NNAME], head[HEADER_SIZE], auser[NNAME];
	struct	passwd	*p;

	p = getpwuid(getuid());

	strcpy(oper, &argv[1][2]);

	if (flag == A_LIFETIME) {
		oper[1] = 0;
		switch (*oper) {

		case 't':
		case 'T':
			*oper = 'T';
			break;

		case 'l':
		case 'L':
			*oper = 'L';
			break;

		case 's':
		case 'S':
		default:
			*oper = 'S';
			break;

		}
	}

	MLP_lock(L_CANCEL);

	/* Not sure why I want to do this? */
	/* kill_printing_request(0, R_CANCEL); */

	for (name = qlist = queuelist(FALSE); *name != '\0'; name += MAXNAMLEN) {

		if (sscanf(&name[2], "%d", &filenum) != 1)
			continue;

		for (m = 2; m <= argc; m++) {

			if (sscanf(argv[m], "%d", &reqnum) != 1 || filenum != reqnum)
				continue;

			if ((fd = open(makepath(QUEUE_PATH, name), O_RDWR)) == BAD)
				continue;

			read(fd, head, HEADER_SIZE);

			strcpy(auser, getuhdrval(head, H_USER));

			if ((p && strequal(auser, p->pw_name)) || is_root()) {

				hdrval(head, (flag == A_PRINTER) ? H_PRINTER : H_LONGEVITY, oper);

				lseek(fd, 0L, SEEK_SET);
				write(fd, head, HEADER_SIZE);
				close(fd);
 
				if (*name == 'R' && flag == A_PRINTER) {
					/* make_inactive(name); */
					kill_printing_request(reqnum, R_CANCEL);
					warning("Request #%d aborted because it was moved to a new printer", 
						reqnum);
				}
			} else
				close(fd);
		}
	}

	MLP_unlock(L_CANCEL);
	free(qlist);
}


/******************************************************************************
  Cancel all the requests in the list given.  This is a critical section--we
  cannot allow the scheduler to reschedule a request that we just canceled
  before we are able to put the request in the inactive queue.  First we
  cancel the despooling of the request, if necessary.  Then we place the
  request in the inactive part of the queue.  The keyword "all" on the
  command line will cancel all printing and pending reports.
******************************************************************************/

void
cancel_all()
{
	int	val;
	char	*qlist, *name;

	MLP_lock(L_CANCEL);

	kill_printing_request(0, R_CANCEL);

	for (name = qlist = queuelist(FALSE); *name != '\0'; name += MAXNAMLEN) {

		if (*name == 'R' && do_i_own_it(&name[2])) {
			make_inactive(name);
			sscanf(&name[2], "%d", &val);
			warning("Request #%d has been canceled.", val);
			printf("#%d canceled\n", val);
		}
	}

	MLP_unlock(L_CANCEL);
	free(qlist);
}


void
cancel_requests(argc, argv) int argc; char *argv[];
{
	int	n, seqnum, Seqnum;
	char	*c, *d, *qlist, *name, *head;

	MLP_lock(L_CANCEL);
	qlist = queuelist(FALSE);
	head = xalloc(HEADER_SIZE);

	for (n = 1; n < argc; n++) {

		c = argv[n];

		if (d = strrchr(c, '-')) c = (d+1);

		seqnum = 0;
		sscanf(c, "%d", &seqnum);
		if (seqnum == 0)
			continue;

		if (!kill_printing_request(seqnum, R_CANCEL)) {

			for (name = qlist; *name != '\0'; name += MAXNAMLEN) {
				if (*name == 'R') {
					sscanf(&name[2], "%d", &Seqnum);

					if (Seqnum == seqnum && do_i_own_it(&name[2])) {
						make_inactive(name);
						warning("Request #%d has been canceled.", Seqnum);
						printf("#%d canceled\n", Seqnum);
					}
				} else
					break;
			}
		}
	}

	free(head);
	MLP_unlock(L_CANCEL);
	free(qlist);
}


/*****************************************************************************
	Print usage message.
*****************************************************************************/

void
lpadmin_usage()
{
	fprintf(stderr,
		"Usage:\tlpadmin -pprinter [ -vdevice ] [ -mbackend ]\n"
		"\tlpadmin -dprinter\n"
		"\tlpadmin -xprinter\n"
		);
	exit(EXIT_FAILURE);
}


/*****************************************************************************
	Change the given destination in the controls file.  This routine is in
	two parts inorder to reuse some of the code.
*****************************************************************************/

void
change_destination(dest, newdest, device, backend)
char *dest;
char *newdest;
char *device;
char *backend;
{
	FILE	*fp, *newfp;
	char	line[WORKSTR], *t, tmppath[WORKSTR], tmp[WORKSTR];

	sprintf(tmppath, "%s.tmp", CONTROL_PATH);
	if ((newfp = fopen(tmppath, "w")) == NULL)
		return;

	MLP_lock(L_CONTROL);
	fp = xopen(CONTROL_PATH, "r+", 0);

	while (fgets(line, sizeof(line), fp)) {
		strcpy(tmp, line);
		if (t = strtoksep(tmp)) {
			if (strequal("PRINTER", uppercase(t))) {
				t = strtoksep(NULL);
				if (!strequal(dest, t))
					fprintf(newfp, "%s", line);
			} else
				fprintf(newfp, "%s", line);
		} else
			fprintf(newfp, "%s", line);
	}

	if (*newdest)
		fprintf(newfp, "printer = %s, %s, %s\n", newdest, device, backend);

	fclose(fp);
	fclose(newfp);

	rename(tmppath, CONTROL_PATH);
	chmod(CONTROL_PATH, CREAT_PERMS);
	MLP_unlock(L_CONTROL);
}


void
delete_destination(dest) char *dest;
{
	change_destination(dest, "", "", ""); 
	log("Remove printer: %s", dest); 
}


/*****************************************************************************
  Adds a new destination to the controls file.  Any current definition by
  the same name will be overwritten
*****************************************************************************/

void
add_destination(printer, device, backend) char *printer, *device, *backend;
{
	change_destination(printer, printer, device, backend);
	log("Add printer: %s, device=%s, backend=%s", printer, device, backend); 
}


/*****************************************************************************
	Change the system default destination, checking to see if the destination
	exists first.
*****************************************************************************/

void
change_default(dest) char *dest;
{
	int	ok;
	char	*list;

	ok = FALSE;
	each_printer(E_START);
	while (*(list = each_printer(E_NEXT)))
		if (strequal(strtoksep(list), dest))
			ok = TRUE;
	each_printer(E_END);

	if (ok)
		setcontrol(DEFAULT, dest);
	else
		error("destination \"%s\" does not exist", dest);

	log("Change system default printer to %s", dest); 
}


/*****************************************************************************
	Parse the arguments and figure out what function to do.
*****************************************************************************/

void
lpadmin(argc, argv) int argc; char *argv[];
{
	int n, add, del, dest;
	char printer[NNAME], backend[NNAME], device[WORKSTR];

	if (argc == 1)
		lpadmin_usage();

	strcpy(printer, "main");
	strcpy(backend, "");
	strcpy(device,  "/dev/null");

	add = del = dest = FALSE;

	for (n = 1; n < argc; n++) {

		if (!strncmp(argv[n], "-p", 2)) {
			strncpy(printer, &argv[n][2], sizeof(printer));
			add = TRUE;
		} else if (!strncmp(argv[n], "-x", 2)) {
			strncpy(printer, &argv[n][2], sizeof(printer));
			del = TRUE;
		} else if (!strncmp(argv[n], "-d", 2)) {
			strncpy(printer, &argv[n][2], sizeof(printer));
			dest = TRUE;
		} else if (!strncmp(argv[n], "-m", 2)) {
			strncpy(backend, &argv[n][2], sizeof(backend));
		} else if (!strncmp(argv[n], "-v", 2))
			strncpy(device, &argv[n][2], sizeof(device));
	}

	if (add) {	/* Add a new printer */
		add_destination(printer, device, backend);
		exit(EXIT_SUCCESS);
	}

	if (del) {	/* Delete a printer definition */
		delete_destination(printer);
		exit(EXIT_SUCCESS);
	}

	if (dest) {	/* Change the system default printer */
		change_default(printer);
		exit(EXIT_SUCCESS);
	}
}


void
lpstat_usage()
{
	fprintf(stderr,
		"Usage: lpstat [-dopqrstv]\n"
		"Options:\n"
		"\t-d\tIdentify default printer.\n"
		"\t-o\tList all pending jobs.\n"
		"\t-p\tList printer status.\n"
		"\t-q\tList detailed report.\n"
		"\t-r\tReport scheduler daemon status.\n"
		"\t-s\tSummarize job and printer status.\n"
		"\t-t\tLike -s, but more detailed.\n"
		"\t-v\tList available printers and devices.\n"
		);
	exit(EXIT_FAILURE);
}


int
main(argc, argv) int argc; char *argv[];
{
	int	n;

	getname(argv[0]);
	if (argc > 1 && strequal(argv[1], "-V")) {
		fprintf(stderr, "%s: " VERSION "\n", progname);
		--argc;
		++argv;
	}

	/********* If program is called "lpadmin" ***********/

	if (strequal(progname, "lpadmin")) {
		if (!is_root()) {
			error("you must be superuser to run lpadmin");
			exit(EXIT_FAILURE);
		}
		lpadmin(argc, argv);
	}


	/********* If program is called "cancel" ***********/

	else if (strequal(progname, "cancel")) {
		if (argc == 1) {
			fprintf(stderr,
				"Usage:\tcancel request ... \n"
				"\tcancel -all\n"
				);
			exit (EXIT_FAILURE);
		} else if (argc == 2 && strequal(argv[1], "-all"))
			cancel_all();
		else
			cancel_requests(argc, argv);
	}


	/********* If program is called "chreq" ***********/

	else if (strequal(progname, "chreq")) {

		if (!strncmp(argv[1], "-p", 2) && argc > 2)
			reprioritize_requests(argc, argv);
		else if (!strncmp(argv[1], "-d", 2) && argc > 2)
			adjust_requests(argc, argv, A_PRINTER);
		else if (!strncmp(argv[1], "-l", 2) && argc > 2)
			adjust_requests(argc, argv, A_LIFETIME);
		else {
			fprintf(stderr,
				"Usage:\tchreq -ppriority job ...\n"
				"\tchreq -ddestination job ...\n"
				"\tchreq -llifetime job ...\n"
				);
			exit(EXIT_FAILURE);
		}
	}


	/********* If program is called "route" ***********/

	else if (strequal(progname, "route")) {

		if (argc == 1)
			list_printers(get_username());
		else if (argc == 2)
			set_route(get_username(), argv[1]);
		else {
			fprintf(stderr, "Usage: route [ printer ]\n");
			exit(EXIT_FAILURE);
		}
	}


	/********* If program is called "lpstat" (or ???) ***********/

	else {

		if (argc == 1)
			request_status();

		for (n = 1; n < argc; n++) {

			if (argv[n][0] != '-')
				lpstat_usage();

			switch(argv[n][1]) {

			case 'd':			/* Default Destination */
				system_default_printer();
				break;

			case 'o':			/* Request Status */
				request_status();
				break;

			case 'p':			/* Printer Status */
				report_printer_status(PRINTER_DISPLAY);
				break;

			case 'q':			/* MLP Queue Status */
				mlp_status();
				break;

			case 'r':			/* Scheduler Status */
				report_scheduler_status();
				break;

			case 's':			/* Summary */
				report_scheduler_status();
				system_default_printer();
				report_printer_status(DEVICE_DISPLAY);
				break;

			case 't':			/* All */
				report_scheduler_status();
				system_default_printer();
				report_printer_status(DEVICE_DISPLAY);
				report_printer_status(PRINTER_DISPLAY);
				request_status();
				break;

			case 'v':			/* Devices */
				report_printer_status(DEVICE_DISPLAY);
				break;

			default:
				lpstat_usage();
				break;
			}
		}
	}

	/* Common success exit for all five invocation flavors. */
	wakeup_scheduler();
	exit(EXIT_SUCCESS); 
}

/* end of mlp/lpstat.c */
