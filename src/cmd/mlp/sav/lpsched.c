/* mlp/lpsched.c */
/*****************************************************************************
   MLP-Spool.  Search the print queue (/usr/spool/mlp/queue) for reports
   to
   print.  When necesarry, spawn processes to print these reports on the
   proper printers observing the needs of concurent printing processes.

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


/*****************************************************************************
  There are times when this program will sleep, while waiting for new things
  to do.  This process is called when the program receives a "wakeup" signal
  from another process (or from its internal alarm-clock).
*****************************************************************************/

void
wakeup()
{
   signal( SIGALRM, wakeup);
}


/*****************************************************************************
   Kill the backend process without placing the request on hold
*****************************************************************************/

void
request_kill()
{
   abort_write = TRUE;
   kill_request = TRUE;
   signal( R_KILL, request_kill);
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
   signal( R_CANCEL, request_cancel);
}


/*****************************************************************************
   Given the name of a printer, return the name of its device.  The controls
   line for a printer looks like this:

   printer = name, device, backend
*****************************************************************************/

char *
printer_device( newprinter) char *newprinter;
{
   char  line[WORKSTR], tmp[NPRINTER], *c, tmppath[WORKSTR], printer[WORKSTR];
   FILE *fp;

   strcpy( printer, newprinter);

   sprintf( tmppath, "%s.tmp", CONTROL_PATH);

   MLP_lock( L_CONTROL);

   if ((fp = fopen( CONTROL_PATH, "r")) != NULL) {

      while ( fgets( line, sizeof( line), fp)) {

         *tmp = *curr_device = *curr_backend = 0;   

         c = strtoksep( line);  if (c) strncpy( tmp, c, sizeof(tmp));

         if (strequal( "PRINTER", uppercase( tmp))) {
            c = strtoksep( NULL);

            if (strequal( printer, c)) {
               c = strtoksep( NULL); if (c) strncpy( curr_device,  c, WORKSTR);
               c = strtoksep( NULL); if (c) strncpy( curr_backend, c, WORKSTR);
               break;
            }
         }
      }

      fclose( fp);
   } else if ( errno == EACCES) {
	 /* This shouldn't happen. */
         fatal("Permission Problem with \"%s\"; it should have the setuid bit set.", progname);
   } else if ( rename( tmppath, CONTROL_PATH) == 0) {
      MLP_unlock( L_CONTROL);
      return( printer_device( newprinter));
   } else
      fatal( "Missing control file: %s", CONTROL_PATH);

   MLP_unlock( L_CONTROL);
   return( curr_device);   
}


/*****************************************************************************
  Extract Header Values
*****************************************************************************/

int
extract_header( name) char *name;
{
   int  fd;
   char path[WORKSTR], header[HEADER_SIZE];

   sprintf( path, "%s/%s", QUEUE_PATH, name);

   if ( (fd = open( path, O_RDONLY)) != BAD) {

      read( fd, header, HEADER_SIZE);  close( fd); 

      strcpy( curr_name, name);
      strcpy( curr_user, getuheaderval( header, H_USER));
      strcpy( curr_printer, getuheaderval( header, H_PRINTER));
      strcpy( curr_desc, getuheaderval( header, H_DESC));
      strncpy( curr_longevity, getuheaderval( header, H_LONGEVITY), 1); 

      curr_mailme = FALSE; 
      if ( *(getheaderval( header, H_MAILME)) != ' ')
         curr_mailme = TRUE;

      curr_writeme = FALSE; 
      if ( *(getheaderval( header, H_WRITEME)) != ' ')
         curr_writeme = TRUE;

      sscanf( getheaderval( header, H_COPIES ), "%d", &curr_copies);
      sscanf( getheaderval( header, H_FORMLEN), "%d", &curr_formlen);

      return 1;
   } else
      return 0;
}


/*****************************************************************************
   Return TRUE if the device specified in this request is available
*****************************************************************************/

int
device_available( name) char *name;
{
   int   id;
   char *t, *c;

   if ( extract_header()) {
      printer_device( curr_printer);

      if (strequal( curr_device, "")) {  /* No device? */

         t = xalloc(NBUF);
         sprintf( t,
		"Printer \"%s\" has no device associated with it.\n\n"
		"Request #%s has been suspended.  One way to resolve the problem\n"
		"would be to define the printer in %s.  Another\n"
		"would be to move the request to a different printer.\n\n"
		"You may need to ask MLP to reprint the request.",
		curr_printer, &name[2], CONTROL_PATH);
         warning( curr_user, t);
         free( t);

         /* Put the request into the inactive queue */
         make_inactive( name);
         return(0);
      }

      /* find out if the device is being used */

      if (*(t = status( curr_device, ""))) {
         /* Who is using the device? */

         c = strtoksep( t);  sscanf( c, "%d", &id); 

         if (!kill( id, 0)) return(0);  /* process is still active */
      }
      return(1);
   }
   return(0);
}


/*****************************************************************************
   Write Request.  Setup a pipeline and spawn a backend process to handle
   the writing of the request.  We do this so that we can monitor to progress
   of the request.
*****************************************************************************/

void
write_request( name, backend, start, end)
char *name;
char *backend;
int   start;
int   end;
{
   int  source, sink, feed = FALSE, copy, docopies, tmpv;
   char path[WORKSTR], tmp[WORKSTR], copies[10], *t, *u;
   int twait, tmp_statp;

   signal( R_KILL,   request_kill);
   signal( R_CANCEL, request_cancel);

   /* Open the print request and the device (data source & sink) */ 

   sprintf( path, "%s/%s", QUEUE_PATH, name);

   if ( ( source = open( path, O_RDONLY)) == BAD) {
      warning_user(curr_user, "Cannot open request file: %s for request %s", path, &name[2]);
      make_inactive( name);
      return;
   }

   if ( ( sink = open( curr_device, O_WRONLY|O_CREAT|O_TRUNC|O_SYNC, CREAT_PERMS)) == BAD) {
      warning_user(curr_user, "Cannot open device %s for request %s", curr_device, &name[2]);
      make_inactive( name);
      return;
   }

   if( pipe( pip) == BAD) {
      warning_user(curr_user, "Cannot create pipe to despool #%s", &name[2]);
      make_inactive( name);
      return;
   }

  	/*
	 * STEVE: this seems wrongheaded and just plain wrong, but I haven't
	 * changed it yet.  I think the intent is to fork(), parent to exit(),
	 * and child to setpgrp(), but in the code as written below it is
	 * the child which just exits immediately.  Also, since the
	 * despooler daemon fork()s immediately in main, why not just
	 * do the setpgrp() there and eliminate this fork() completely?
	 * I think Blatchey misunderstood the fork() return value,
	 * note his comments "parent feeds child" below when in fact
	 * as implemented the child feeds the parent.
	 */
   /* See Steven's Networking ~p.80 for fork()/setpgrp() sequences. */
   switch (fork()) {

   case BAD: /* fork() failed */
             warning_user(curr_user, "Cannot fork to despool #%s", &name[2]);
             close( source); close( sink); close( pip[0]); close( pip[1]);
             make_inactive( name);
             return;

   case   0: exit(EXIT_SUCCESS);		/* child just returns */

   default:  /* parent of successful fork() */
        setpgrp();  /* cut us loose from any control terminals! */
	/* This was missing before and this is why mail failed. */
	signal(SIGHUP, SIG_IGN);

      /* The parent feeds the child, while noting the progress of the feeding
         in the status file -- Inquiring Minds want to know! */

      switch ( fork()) {
      case BAD: /* fork() failed */
                warning_user(curr_user, "Cannot fork to despool #%s", &name[2]); 
                close( source); close( sink); close( pip[0]); close( pip[1]);
                return;

      case   0: /* child execs the backend piped to the parent */
                close( source); close( pip[1]); dup2( pip[0], 0); dup2( sink, 1);

                signal( SIGTERM, SIG_IGN);
                signal( SIGINT,  SIG_IGN);

                sprintf( copies, "%d", curr_copies);

                if (strequal( backend, DBACKEND))
                   execl( backend, backend, NULL);
                else
                   execl( "/bin/sh", "/bin/sh", backend, &name[2], curr_user,
                   	copies, NULL);
      }

      /* Parent feeds the child... */

      close( pip[0]); signal( SIGCLD, SIG_IGN);

      t = controls( FEED, ""); 
      if (strequal( t, "ON") || strequal( t, "on")) feed = TRUE;

      docopies = 1;
      t = controls( DOCOPIES, "");
      if (strequal( t, "ON") || strequal( t, "on")) docopies = curr_copies;


      for ( copy = 0; copy < docopies; copy++) {
         curr_copy = copy + 1;

         lseek( source, (long) HEADER_SIZE, 0);

         if (writeOut( source, pip[1], start, end, curr_formlen, TRUE) == BAD) {
            
            sscanf( &name[2], "%d", &tmpv);

            if ( kill_request) {
               sprintf( tmp, "\n\n*** Request #%d Terminated ***\n\f", tmpv);
               warning_user(curr_user, "The despooling of request #%d has been aborted.", tmpv);
            }
            else {
               sprintf( tmp, "\n\n*** Request #%d Canceled ***\n\f", tmpv);
               warning_user(curr_user, "Request #%d has been canceled.", tmpv);
            }
               
            write( pip[1], tmp, strlen(tmp));
            break;
         }

         if ( feed) { *tmp = '\f'; write( pip[1], tmp, 1); }
      }

      close( source); close( pip[1]); close( sink);
 
      /* If the request was aborted, keep it in the active
         part of the queue.  Otherwise, retire it to the
         inactive queue.  If the request is a reprint
         then remove it from the queue. */

      if (!kill_request)
           make_inactive( name);

      /* Reap Children */
      while ((twait = wait(&tmp_statp)) != -1)
		/* Do Nothing */;

      /* Send Messages if necessary */

      if (curr_writeme || curr_mailme) {
         t = xalloc(NBUF2);
         u = xalloc(NBUF2);

         strcpy( t, "/bin/echo \"\n`date`\n\"'\n*** MLP Spooler Reports");
         strcat( t, " ***\n\nYour request, \"");
         strcat( t, curr_desc);
         strcat( t, ",\" ");

         if (strlen( curr_desc) > 15)
            strcat( t, "\n");

         sprintf( tmp, "has been despooled on printer: %s", curr_printer);
         strcat( t, tmp);

         if (curr_writeme) {
            sprintf( u, "%s\n\n'| write %s 2>/dev/null", t, curr_user);
            system( u);
         }

         if ( curr_mailme) {
		int tmp_ret;
		int (*old_sigcld)();

	    old_sigcld = signal(SIGCLD, SIG_DFL);
            sprintf( u, "%s\n\n'| lmail %s 2>/dev/null",
		t, (getenv("MLPLMAIL")) ? "lmail" : "/bin/mail", curr_user);

            tmp_ret = system( u);
	    signal(SIGCLD, old_sigcld);
         }

         free( t); free( u);
      }
   } 

   close( pip[0]);  close( pip[1]);          
}


/*****************************************************************************
   De-spool a given file.
*****************************************************************************/

void
despool_file( name, start, end)
char *name;
int   start;
int   end;
{
   char path[WORKSTR], tmp[NBUF2], newname[NNAME];
   int  backend, r_copies;

   if ( *curr_longevity == 'R') { /* Is this a reprint? */

      /* sample description: "Reprint #00000 from page 0000 to page 0000"
                                    10-|            26-|         34-|     */

      sscanf( &curr_desc[9], "%5s", tmp);
      strcpy( newname, request_name( tmp));

      sscanf( &curr_desc[26], "%d", &start);  
      sscanf( &curr_desc[34], "%d", &end);  

      sprintf( tmp, "%s/%s", QUEUE_PATH, name);
      unlink( tmp);

      r_copies = curr_copies;  strcpy( tmp, curr_printer);
      extract_header( newname);
      curr_copies = r_copies;  strcpy( curr_printer, tmp);
   }
   else
      strcpy( newname, name);


   if (*curr_backend == 0) strcpy( curr_backend, DEFAULT);

   sprintf( path, "%s/%s", BACKEND_PATH, curr_backend);

   if ( (backend = open( path, O_RDONLY)) == BAD) {
      sprintf( tmp,
		"Cannot find backend script (%s) for "
      		"the %s printer\nwhile trying to "
		"despool request #%s\n\n"
		"Despooling done with %s.\n",
		curr_backend, curr_printer, &newname[2], DBACKEND);
      warning( tmp);
      write_request( newname, DBACKEND, start, end);
   }
   else
      write_request( newname, path, start, end);


   log( "Despooled Request #%s", &name[2]);
}


/*****************************************************************************
   De-spool.   Check the queue for work, then process it.
*****************************************************************************/

void
despool()
{
   char *name, *t, *plist, tmp[WORKSTR];
   int      n = 0, parent;


   parent = getpid();

   plist = xalloc(NBUF);    /* contains a list of printer in use during
                                 this pass over the spool queue */

   t = dirlist( QUEUE_PATH, FALSE);  /* return sorted spool queue list */
                                     /* This list is a two dimensional array */
                                     /* (entries x MAXNAMLEN) */
                                     /* Ascending order */

   while ( *(name = (t+(n * MAXNAMLEN)))) {

      if ( name[0] == 'R') {               /* despool the "ready" entries */
         if ( device_available( name)) {
            sprintf( tmp, "%s|", curr_device);

            if (!strstr( plist, tmp)) {
               strcat( plist, tmp);

               switch (fork()) {

               case -1: warning("Cannot fork backend process for reqest #%s", &name[2]);
                        break;

               case  0: /* claim the device */

                        sprintf( tmp, "%d, %s", getpid(), name);
                        status( curr_device, tmp); 

                        despool_file( name, 0, 0);

                        /* wakeup parent to rescan for work for this device */
                        kill( parent, SIGALRM);

                        exit(EXIT_SUCCESS);
               }

               signal( SIGCLD, SIG_IGN);
            }
         }
      }

      n++;
   }

   free( plist); free( t);
}


/******************************************************************************
   Roll the log over after a given time so that it does not grow
   without bounds.
******************************************************************************/

#define RHOURS (7.0 * 24.0)		/* one week */

void
roll_log()
{
   time_t current, rollover;
   struct stat sb;
   float  rhours;
   char   tmp[WORKSTR];

   rhours = RHOURS; sscanf( controls( LOGROLL, ""), "%f", &rhours);

   rollover = SECS_PER_HOUR(rhours);   time( &current);

   if (stat( LOG_PATH, &sb) == BAD)
      return;

   if (sb.st_ctime + rollover < current) {
      sprintf( tmp, "%s.old", LOG_PATH);

      unlink( tmp); rename( LOG_PATH, tmp);

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

#define THOURS 0.5			/* half an hour */
#define SHOURS 24.0			/* one day */
#define LHOURS (7.0 * 24.0)		/* one week */

void
grim_reaper()
{
   long   dstamp;
   time_t current, temporary, shortterm, longterm, elapst;
   float  thours, shours, lhours;
   char  *entries, *name, header[HEADER_SIZE], path[WORKSTR], life[2];
   char   desc[WORKSTR];
   int    n, fd, die;

   thours = THOURS; sscanf( controls( TLIFE, ""), "%f", &thours);
   shours = SHOURS; sscanf( controls( SLIFE, ""), "%f", &shours);
   lhours = LHOURS; sscanf( controls( LLIFE, ""), "%f", &lhours);

   temporary = SECS_PER_HOUR(thours);
   shortterm = SECS_PER_HOUR(shours);
   longterm  = SECS_PER_HOUR(lhours);

   time( &current);

   entries  = dirlist( QUEUE_PATH, FALSE);  /* return sorted spool queue list */
                                            /* This list is a two dimensional */
                                            /* array (entries x MAXNAMLEN) */
                                            /* Ascending order */

   for ( n = 0; *(name = (entries+(n * MAXNAMLEN))); n++) {
      if ( *name == 'R') continue;

      sprintf( path, "%s/%s", QUEUE_PATH, name);
      if ( (fd = open( path, O_RDONLY)) != BAD) {
         read( fd, header, HEADER_SIZE);
         dstamp = 0L; 
         sscanf( getheaderval( header, H_DSTAMP   ), "%ld", &dstamp);
         sscanf( getheaderval( header, H_LONGEVITY), "%1s", life);
         strcpy( desc, getuheaderval( header, H_DESC));
         close( fd);

         elapst = current - (time_t)dstamp;
         die = FALSE;
         switch (*life) {
            case 'T': if (elapst > temporary) die = TRUE; break;
            case 'S': if (elapst > shortterm) die = TRUE; break;
            case 'L': if (elapst > longterm)  die = TRUE; break;
         }

         if (die) { 
            log( "Request #%s \"%s\" expired.", &name[2], desc);
            unlink( path);
         }
      }
   }

   free( entries);
}


/*****************************************************************************
   Main Driver.   Tell the spooler how to find us.  Then process the queue
   and sleep until something exciting happens ;)  First check to see if
   the despooler is already running.
*****************************************************************************/

int
main(argc, argv) int argc; char *argv[];
{
   char tmp[10];
   int  child, cycles;

   umask(UMASK_PERMS);

   strcpy(progname, basename( argv[0]));
   bedaemon();

   if ( scheduler_status()) {
      printf("MLP Scheduler already started.\n");
      exit(EXIT_SUCCESS); 
   }

   switch (child = fork()) {
   case -1: fatal("Scheduler cannot fork()");

   case 0: /* Child, running in background as despooler daemon. */
	   /* I am in the background now, and will despool requests */

           chdir("/"); /* so we can dismount any mounted filesystems */

           /* for (fd = 0; fd < NOFILE; fd++) close(fd); */

           setpgrp();  /* detach me from my parent process */

           signal( SIGALRM, wakeup);
           signal( SIGCLD,  SIG_IGN);
           signal( SIGHUP,  SIG_IGN);

           sprintf( tmp, "%d", getpid());  status( DESPOOLER, tmp); 

           for (cycles = 0; *(status( DESPOOLER, "")); sleep( NSLEEP)) {

              MLP_lock( L_CANCEL);

              despool();
              
              if (++cycles % NGRIM == 0) {
                 grim_reaper();		/* purge the inactive queue */
		 roll_log();		/* roll over the log file */
	      }

              MLP_unlock( L_CANCEL);

           }

           fatal("Scheduler daemon expired gracefully"); 
           break;

   default: /* Parent, prints and logs start info and then just exits. */
            printf("MLP Scheduler Started, PID: %d\n", child);
            log("MLP Scheduler Started, PID: %d", child);
            break;
   }

   exit(EXIT_SUCCESS); 
}

/* end of mlp/lpsched.c */
