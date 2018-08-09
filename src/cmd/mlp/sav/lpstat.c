/* mlp/lpstat.c */
/******************************************************************************
   lpstat.  MLP line-printer status a'la Unix V.
******************************************************************************/

#include "mlp.h"

/* Flag arguments to adjust_requests(). */
typedef	enum	adj_flag { A_LIFETIME, A_PRINTER } ADJ_FLAG;

/******************************************************************************
  Report the name of the system default printer
******************************************************************************/

void system_default_printer()
{
   char *c;
   c = controls( DEFAULT, "");

   printf("system default destination is: %s\n", c);
}



/******************************************************************************
  Report whether the scheduler is running or not
******************************************************************************/

void
report_scheduler_status()
{
   printf("scheduler is %srunning\n", ( scheduler_status()) ? "" : "not");
}


/******************************************************************************
  List the currently printing requests and the pending ones in Unix V style
******************************************************************************/

report_request_status()
{
   int   n, fd;
   char *requests, *entries, *name, header[HEADER_SIZE], path[WORKSTR];
   char  user[NNAME], timestr[NTIME], printer[NNAME];
   long  rtime;
   struct passwd *p;

   p = getpwuid(getuid());

   requests = report_printer_status( REQUEST_DISPLAY);

   entries  = dirlist( QUEUE_PATH, FALSE);  /* return sorted spool queue list */
                                            /* This list is a two dimensional */
                                            /* array (entries x MAXNAMLEN) */
                                            /* Ascending order */

   for ( n = 0; *(name = (entries+(n * MAXNAMLEN))); n++) {
      if ( *name == 'r') break;

      if (strstr( requests, name) == NULL) {
         sprintf( path, "%s/%s", QUEUE_PATH, name);

         if ( (fd = open( path, O_RDONLY)) != BAD) {
             read( fd, header, HEADER_SIZE);
             close( fd);

             strcpy( user, getuheaderval( header, H_USER));

             if ( (p && strequal( user, p->pw_name)) || is_root()) {
                strcpy( printer, getuheaderval( header, H_PRINTER));

                strcpy( timestr, getheaderval( header, H_DSTAMP));
                rtime = 0L; sscanf( timestr, "%ld", &rtime);
                strcpy( timestr, ctime(&rtime));
                timestr[strlen(timestr)-1] = 0;

                printf( "%s-%s\t\t%c\t%s", user,
                   (name+2), *(name+1), timestr);

                printf( "\tfor %s\n", printer);
             }
         }
      }
   }

   free( entries);
}


/******************************************************************************
  List the currently printing requests and the pending ones in MLP style
******************************************************************************/

report_mlp_request_status()
{
   int   n, fd, printing, val;
   char *requests, *entries, *name, header[HEADER_SIZE], path[WORKSTR];
   char  user[NNAME], printer[NNAME], title[100], tmp[WORKSTR];
   long  rtime;
   struct tm *tt;

   entries  = dirlist( QUEUE_PATH, TRUE);  /* return sorted spool queue list */
                                            /* This list is a two dimensional */
                                            /* array (entries x MAXNAMLEN) */
                                            /* Ascending order */

   printf("REQ # S P L #Pgs FL  #Cp   Date   Time  Description...\n");
   printf("----- - - - ---- --- --- -------- ----- -----------------------------------\n");

   for ( n = 0; *(name = (entries+(n * MAXNAMLEN))); n++) {
      printing = FALSE;

      if (strstr( requests, name)) printing = TRUE;

      sprintf( path, "%s/%s", QUEUE_PATH, name);

      if ( (fd = open( path, O_RDONLY)) != BAD) {
          read( fd, header, HEADER_SIZE);
          close( fd);

          printf("%-5s ", (name+2));  /* request sequence no. */

          /* Request Status */

          if (printing)
             printf("* ");  /* printing now */
          else
          if (*name == 'R')
             printf("P ");  /* waiting to print */
          else
          if (*name == 'r')
             printf("Q ");  /* held in queue print */
          else
             printf("? ");  /* don't know the status */

          printf("%c ", *(name+1));   /* priority */

          printf("%s ", getuheaderval( header, H_LONGEVITY));

          val = 0;
          sscanf( getuheaderval( header, H_PAGES), "%d", &val);
          printf("%4d ", val);
    
          val = 0;
          sscanf( getuheaderval( header, H_FORMLEN), "%d", &val);
          printf("%3d ", val);
    
          val = 0;
          sscanf( getuheaderval( header, H_COPIES), "%d", &val);
          printf("%3d ", val);


          sscanf( getuheaderval( header, H_DSTAMP), "%ld", &rtime);

          tt = localtime( &rtime);

          printf( "%02d/%02d/%02d ", (tt->tm_mon+1), tt->tm_mday, tt->tm_year);
          printf( "%02d:%02d ", tt->tm_hour, tt->tm_min);
    
          strcpy( user,    getuheaderval( header, H_USER));
          strcpy( printer, getuheaderval( header, H_PRINTER));
          strcpy( title,   getuheaderval( header, H_DESC));
   
          sprintf( tmp, "%s -by %s -on %s", title, user, printer);

          printf("%s\n", tmp);
      }
   }
}


/******************************************************************************
  Print a list of printers available for routine.
******************************************************************************/

list_printers( user)
char *user;
{
   char *list;

   printf("%s  ( ", route_request( user));

   each_printer( E_START);

   while (*(list = each_printer( E_NEXT)))
      printf( "%s ", strtoksep( list));

   each_printer( E_END);

   printf("system display local )\n");
}


/******************************************************************************
  Set the printer that the user wants to route to.
******************************************************************************/

set_route( user, printer)
char *user;
char *printer;
{
   FILE *fp;
   int   inlist;
   char *list, path[WORKSTR];

   each_printer( E_START);

   inlist = FALSE;

   while (*(list = each_printer( E_NEXT)))
      if (strequal( strtoksep( list), printer)) inlist = TRUE;

   each_printer( E_END);

   if (strequal( "local",   printer)
    || strequal( "display", printer)
    || strequal( "system",  printer))
       inlist = TRUE;

   if (!inlist) {
      printf("\a\"%s\" is not a defined printer.\n", printer);
      return;
   }

   sprintf( path, "%s/%s", ROUTE_PATH, user);

   if ((fp = fopen( path, "w+")) != NULL) {
      fprintf( fp, "%s\n", printer);
      fclose( fp);
      printf( "%s, your requests will now spool to the \"%s\" printer.\n", 
         user, printer);
   }
}


/******************************************************************************
   Reprioritize the requests
******************************************************************************/

void reprioritize_requests( argc, argv)
int   argc;
char *argv[];
{
   int   n, m, reqnum, filenum, fd;
   char *t, *name, prior, oldnam[WORKSTR], newnam[WORKSTR], *head, auser[30];
   struct passwd *p;

   p = getpwuid(getuid());

   prior = argv[1][2];

   if ( prior < '0') prior = '0';
   if ( prior > '9') prior = '9';


   MLP_lock( L_CANCEL);

   /* Not sure why I want to do this? */
   /* kill_printing_request( 0, R_CANCEL, CLEAR_ALL); */

   t = dirlist( QUEUE_PATH, FALSE);  /* return sorted spool queue list */
                                     /* This list is a two dimensional */
                                     /* (entries x MAXNAMLEN) */
                                     /* Ascending order */

   head = xalloc( HEADER_SIZE);

   for (n = 0; *(name = (t+(n * MAXNAMLEN))); n++) {

      if (sscanf( &name[2], "%d", &filenum) == 1) {

         for (m = 2; m <= argc; m++) {
            if (sscanf( argv[m], "%d", &reqnum) == 1) {
               if ( filenum == reqnum) {
                  sprintf( oldnam, "%s/%s", QUEUE_PATH, name);

                  if ( (fd = open( oldnam, O_RDONLY)) != BAD) {
                     read( fd, head, HEADER_SIZE);
                     close( fd);

                     strcpy( auser, getuheaderval( head, H_USER));

                     if (p && strequal( auser, p->pw_name) || is_root()) {

                        name[1] = prior;
                        sprintf( newnam, "%s/%s", QUEUE_PATH, name);
                        rename( oldnam, newnam);

                        if ( *name == 'R') {
                           /* make_inactive( name); */
                           kill_printing_request( reqnum, R_CANCEL, CLEAR_ONE);
                           warning( "Request #%d aborted due to a change in priority", 
                              reqnum);
                        }
                     }
                  }
               }
            }
         }
      }
   }

   free( head);

   MLP_unlock( L_CANCEL);

   free( t);
}


/******************************************************************************
   Change the longevity or the printer
******************************************************************************/

void adjust_requests( argc, argv, flag)
int   argc;
char *argv[];
ADJ_FLAG   flag;
{
   int   n, m, reqnum, filenum, fd;
   char *t, *name, nam[WORKSTR], oper[NNAME], head[HEADER_SIZE], auser[30];
   struct passwd *p;

   p = getpwuid(getuid());

   strcpy( oper, &argv[1][2]);

   if (flag == A_LIFETIME) {
      oper[1] = 0;
      switch (*oper) {
         case 't':
         case 'T': *oper = 'T'; break;
         case 'l':
         case 'L': *oper = 'L'; break;
         case 's':
         case 'S':
         default:  *oper = 'S'; break;
      }
   }

   MLP_lock( L_CANCEL);

   /* Not sure why I want to do this? */
   /* kill_printing_request( 0, R_CANCEL, CLEAR_ALL); */

   t = dirlist( QUEUE_PATH, FALSE);  /* return sorted spool queue list */
                                     /* This list is a two dimensional */
                                     /* (entries x MAXNAMLEN) */
                                     /* Ascending order */

   for (n = 0; *(name = (t+(n * MAXNAMLEN))); n++) {

      if (sscanf( &name[2], "%d", &filenum) == 1) {

         for (m = 2; m <= argc; m++) {
            if (sscanf( argv[m], "%d", &reqnum) == 1) {

               if ( filenum == reqnum) {
                  sprintf( nam, "%s/%s", QUEUE_PATH, name);

                  if (( fd = open( nam, O_RDWR)) != BAD) {
                     read( fd, head, HEADER_SIZE);

                     strcpy( auser, getuheaderval( head, H_USER));

                     if ((p && strequal( auser, p->pw_name)) || is_root()) {

                        headerval( head, (flag == A_PRINTER) ? H_PRINTER : H_LONGEVITY, oper);

                        lseek( fd, 0L, 0);

                        write( fd, head, HEADER_SIZE);
                        close( fd);
 
                        if ( *name == 'R' && flag == A_PRINTER) {
                           /* make_inactive( name); */
                           kill_printing_request( reqnum, R_CANCEL, CLEAR_ONE);

                           warning( "Request #%d aborted because it was moved to a new printer", 
                              reqnum);
                        }
                     }
                  }
               }


            }
         }
      }
   }

   MLP_unlock( L_CANCEL);

   free( t);
}


/******************************************************************************
  Cancel all the requests in the list given.  This is a critical section--we
  cannot allow the despooler to reschedule a request that we just canceled
  before we are able to put the request in the inactive queue.  First we
  cancel the despooling of the request, if necessary.  Then we place the
  request in the inactive part of the queue.  The keyword "all" on the
  command line will cancel all printing and pending reports.
******************************************************************************/

void cancel_all_requests()
{
   int n, val;
   char *t, *name;

   MLP_lock( L_CANCEL);

   kill_printing_request( 0, R_CANCEL, CLEAR_ALL);

   t = dirlist( QUEUE_PATH, FALSE);  /* return sorted spool queue list */
                                     /* This list is a two dimensional */
                                     /* (entries x MAXNAMLEN) */
                                     /* Ascending order */




   for (n = 0; *(name = (t+(n * MAXNAMLEN))); n++)
      if ( *name == 'R') {
         if (do_i_own_it()) {
            make_inactive( name);
            sscanf( &name[2], "%d", &val);
            warning("Request #%d has been canceled.", val);
            printf("#%d canceled\n", val);
         }
      }

   MLP_unlock( L_CANCEL);

   free( t);
}


void
 cancel_requests( argc, argv)
int   argc;
char *argv[];
{
   int n, m, seqnum, Seqnum;
   char *c, *d, *t, *name, *head;

   if (strequal( argv[1], "all")) {
      cancel_all_requests();
      return;
   }


   MLP_lock( L_CANCEL);


   t = dirlist( QUEUE_PATH, FALSE);  /* return sorted spool queue list */
                                     /* This list is a two dimensional */
                                     /* (entries x MAXNAMLEN) */
                                     /* Ascending order */

   head = xalloc( HEADER_SIZE);

   for ( n = 1; n < argc; n++) {

      c = argv[n];

      if ( d = strrchr( c, '-')) c = (d+1);

      seqnum = 0; sscanf( c, "%d", &seqnum);

      if ( seqnum > 0) {

         if (!kill_printing_request( seqnum, R_CANCEL, CLEAR_ONE)) {

            for (m = 0; *(name = (t+(m * MAXNAMLEN))); m++)
               if ( *name == 'R') {
                  sscanf( &name[2], "%d", &Seqnum);

                  if ( Seqnum == seqnum) {

                     if (do_i_own_it()) {
                        make_inactive( name);
                        warning("Request #%d has been canceled.", Seqnum);
                        printf("#%d canceled\n", Seqnum);
                     }
                  }
               }
               else
                  break;
         }
      }
   }

   free( head);

   MLP_unlock( L_CANCEL);

   free( t);
}


/*****************************************************************************
   Print usage message.
*****************************************************************************/

void
usage()
{
   printf(
	"Usage:\tlpadmin -pprinter [-vdevice] [-mmodel]\n"
	"\tlpadmin -ddestination\n"
	"\tlpadmin -xdestination\n\n"
	"NOTE:\tThis utility modifies %s.\n"
	"\tModels are found in %s\n\n.",
		CONTROL_PATH, BACKEND_PATH);
}


/*****************************************************************************
   Delete the given destination from the controls file.  This routine is in
   two parts inorder to reuse some of the code.
*****************************************************************************/

void
_delete_destination( dest, newdest, device, backend)
char *dest;
char *newdest;
char *device;
char *backend;
{
   FILE *fp, *newfp;
   char  line[128], *t, tmppath[WORKSTR], tmp[WORKSTR];

   fp = (FILE *) each_printer( E_FILE);

   sprintf( tmppath, "%s.tmp", CONTROL_PATH);

   if ((newfp = fopen( tmppath, "w")) != NULL) {

      while (fgets( line, sizeof(line), fp)) {
         strcpy( tmp, line);

         if (t = strtoksep( tmp)) {

            if (strequal( "PRINTER", uppercase( t))) {
               t = strtoksep( NULL);
               if (!strequal( dest, t))
                  fprintf( newfp, "%s", line);
            }
            else
               fprintf( newfp, "%s", line);
         }
         else
            fprintf( newfp, "%s", line);
      }


      if ( *newdest)
         fprintf( newfp, "printer = %s, %s, %s\n", newdest, device, backend);

      fclose( fp);  fclose( newfp);

      rename( tmppath, CONTROL_PATH);

      chmod( CONTROL_PATH, 0664);
   }
}


void
delete_destination( dest) char *dest;
{
    each_printer( E_START); 
   _delete_destination( dest, "", "", ""); 
    each_printer( E_UNLOCK);
    log( "Remove Printer: %s", dest); 
}


/*****************************************************************************
  Adds a new destination to the controls file.  Any current definition by
  the same name will be overwritten
*****************************************************************************/

add_destination( printer, device, backend)
char *printer;
char *device;
char *backend;
{
    each_printer( E_START);

   _delete_destination( printer, printer, device, backend);

    each_printer( E_UNLOCK);

    log( "Add Printer: %s, device=%s, backend=%s", printer, device, backend); 
}


/*****************************************************************************
   Change the system default destination, checking to see if the destination
   exists first.
*****************************************************************************/

change_destination( dest)
char *dest;
{
   int   ok = FALSE;
   char *list;

   each_printer( E_START);

   while (*(list = each_printer( E_NEXT)))
      if (strequal( strtoksep( list), dest)) ok = TRUE;

   each_printer( E_END);

   if (ok)
      controls( DEFAULT, dest);
   else
      printf("\aDestination \"%s\" does not exist.\n", dest);

   log( "Change System Default to: %s", dest); 
}


/*****************************************************************************
   Parse the arguments and figure out what function to do.
*****************************************************************************/

lpadmin( argc, argv)
int   argc;
char *argv[];
{
   int n, add, del, dest;
   char printer[NNAME], backend[NNAME], device[129];

   if ( argc == 1) { usage(); exit(EXIT_FAILURE); }

   strcpy( printer, "main");
   strcpy( backend, "");
   strcpy( device,  "/dev/null");

   add = del = dest = FALSE;

   for ( n = 1; n < argc; n++) {
      if (!strncmp( argv[n], "-p", 2)) {
         strncpy( printer, &argv[n][2], sizeof( printer));
         add = TRUE;
      }
      else
      if (!strncmp( argv[n], "-x", 2)) {
         strncpy( printer, &argv[n][2], sizeof( printer));
         del = TRUE;
      }
      else
      if (!strncmp( argv[n], "-d", 2)) {
         strncpy( printer, &argv[n][2], sizeof( printer));
         dest = TRUE;
      }
      else
      if (!strncmp( argv[n], "-m", 2))
         strncpy( backend, &argv[n][2], sizeof( backend));
      else
      if (!strncmp( argv[n], "-v", 2))
         strncpy( device, &argv[n][2], sizeof( device));
   }

   if (add) {  /* Add a new printer */
      add_destination( printer, device, backend);
      exit(EXIT_SUCCESS);
   }
   
   if (del) {  /* Delete a printer definition */
      delete_destination( printer);
      exit(EXIT_SUCCESS);
   }
   
   if (dest) { /* Change the system default printer */
      change_destination( printer);
      exit(EXIT_SUCCESS);
   }
}


int
main( argc, argv) int argc; char *argv[];
{
   int    n;
   char   user[NNAME];

   umask(UMASK_PERMS);

   strcpy(progname, basename( argv[0]));
   bedaemon();

   /********* If program is called "lpadmin" ***********/

   if (strequal( progname, "lpadmin")) {
      if (is_root()) {
         lpadmin( argc, argv);
         wakeup_despooler();
      }
      else
         printf("\a\nSorry, you must be superuser to do this.\n\n");

      exit(EXIT_FAILURE);
   }


   /********* If program is called "cancel" ***********/

   if (strequal( progname, "cancel")) {
      if (argc == 1)
         printf("\nusage:  cancel request_num1 request_num2 ... request_numN\n\n");
      else
         cancel_requests( argc, argv);

      wakeup_despooler();

      exit(EXIT_SUCCESS);
   }


   /********* If program is called "chreq" ***********/

   if (strequal( progname, "chreq")) {
      if (!strncmp( argv[1], "-p", 2) && argc > 2)
         reprioritize_requests( argc, argv);
      else
      if (!strncmp( argv[1], "-d", 2) && argc > 2)
         adjust_requests( argc, argv, A_PRINTER);
      else
      if (!strncmp( argv[1], "-l", 2) && argc > 2)
         adjust_requests( argc, argv, A_LIFETIME);
      else {
         printf("\nusage: chreq -pNew_priority    reqests_num1 request_");
         printf("num2 ... request_numN\n\n");
         printf("             -dNew_destination request_num1 request_");
         printf("num2 ... request_numN\n\n");
         printf("             -lNew_lifetime    request_num1 request_");
         printf("num2 ... request_numN\n\n");
      }

      wakeup_despooler();
      exit(EXIT_SUCCESS);
   }



   /********* If program is called "route" ***********/

   if (strequal( progname, "route")) {
      strcpy( user, "unknown");

      if ( getlogin())
         strcpy( user, getlogin());

      if ( argc == 1) list_printers( user);
      else
      if ( argv[1] != NULL)
         set_route( user, argv[1]);

      wakeup_despooler();
      exit(EXIT_SUCCESS);
   }


   /********* If program is called "lpstat" ***********/
   if ( argc == 1) report_request_status();

   for ( n = 1; n < argc; n++) {
	if (argv[n][0] != '-')
		continue;
	switch(argv[n][1]) {
	case 's':				/* Summary */
		report_scheduler_status();
		system_default_printer();
		report_printer_status( DEVICE_DISPLAY);
		break;
	case 'v':				/* Devices */
		report_printer_status( DEVICE_DISPLAY);
		break;
	case 't':				/* All */
		report_scheduler_status();
		system_default_printer();
		report_printer_status( DEVICE_DISPLAY);
		report_printer_status( PRINTER_DISPLAY);
		report_request_status();
		break;
	case 'q':				/* MLP Queue Status */
		report_mlp_request_status();
		break;
	case 'r':				/* Scheduler Status */
		report_scheduler_status();
		break;
	case 'p':				/* Printer Status */
		report_printer_status( PRINTER_DISPLAY);
		break;
	case 'o':				/* Request Status */
		report_request_status();
		break;
	case 'd':				/* Default Destination */
		system_default_printer();
		break;
	}
   }

   wakeup_despooler();
   exit(EXIT_SUCCESS); 
}

/* end of mlp/lpstat.c */
