/* mlp/spooler.c */
/******************************************************************************
   Various routines used by the MLP spooler system.
******************************************************************************/

#include "mlp.h"

/* Statics. */
#if	USE_SEM
static	int	sID = BAD;
#endif

/* Globals. */
char	progname[16];
char	curr_printer[WORKSTR];
char	curr_device[WORKSTR];
char	curr_backend[WORKSTR];
char	curr_name[NNAME];
int	curr_copies;
int	curr_copy;
int	curr_formlen;
int	abort_write = FALSE;


/******************************************************************************
	Make sure the program is running with euid == daemon,
	so that it will have appropriate permissions on spool directories.
******************************************************************************/

void
bedaemon()
{
	register struct passwd *pwp;

	if ((pwp = getpwnam("daemon")) == NULL || pwp->pw_uid != geteuid()) {
		fprintf(stderr,
			"%s: not running euid==daemon, should be setuid/setgid\n",
			progname);
		exit(EXIT_FAILURE);
	}
}


/******************************************************************************
   fatal.  Handles fatal errors with "astonishing" grace.  It sends a formated
   messages to root's mailbox and terminates the program.  Warning sends a 
   formated message to the user's mailbox and returns to caller.
******************************************************************************/

void
warning_user( va_alist)
va_dcl
{
   va_list  args;
   char    *i, *j, *user;

   i = xalloc(NBUF);  j = xalloc(NBUF);

   strcpy(  i, MSGHEAD);

   va_start( args);  
   user = va_arg( args, char *);  sprintf( j, "%r", args);
   va_end( args);

   strcat( i, j); 
   strcat( i, MSGTAIL);
   strcat( i, user);

   log( "%s", j);  system( i);

   free( i);  free( j);
}


void
warning( va_alist)
va_dcl
{
   va_list args;
   char    *s, user[NNAME];

   strcpy( user, (s = getlogin()) ? s : "root");
   va_start( args);
   warning_user( user, "%r", args);
   va_end( args);
}

void
fatal( va_alist)
va_dcl
{
   va_list  args;

   va_start( args);
   warning_user( "root", "%r", args);
   va_end( args);
   sleep(2);			/* let system() complete sending mail */
   exit(EXIT_FAILURE);
}


/******************************************************************************
   Removes the leading and trailing spaces from the given string.  First
   scan to the end of the string. Then eat the right hand spaces.  Then
   eat the left hand spaces.  Then copy the unpadded string back into its
   original place.
******************************************************************************/

char *
unpadd( str) char *str;
{
   int   len = 0;
   char *t, *u;

   for (t = str; *t; t++) len++;    t--;

   while ( t != str && *t == ' ') { *t = 0; t--; len--;} 

   for (t = str; *t && *t == ' '; t++) len--;

   u = xalloc(len+1); strncpy( u, t, len); strcpy( str, u); free( u);

   return( str);
}


/******************************************************************************
   Rename a file.
   N.B. this unlinks new before trying the rename, unlike system call rename().
******************************************************************************/

int 
rename( old, new) char *old; char *new;
{
   unlink( new);
   if ( link( old, new) == BAD) return(2);
   if ( unlink( old) == BAD)    return(3);
   return(0);
}


/*****************************************************************************
   Put a request in the inactive queue
*****************************************************************************/

void
make_inactive( name) char *name;
{
   char newname[NNAME], path[WORKSTR], newpath[WORKSTR];

   if (*name == 'r') return;  /* already inactive */

   strcpy( newname, name); newname[0] = 'r';

   sprintf( path,    "%s/%s", QUEUE_PATH, name);
   sprintf( newpath, "%s/%s", QUEUE_PATH, newname);     

   rename( path, newpath);
} 


/******************************************************************************
   Get/Set header information
   Set the info if value is not NULL, otherwise get the info.
******************************************************************************/

char *
getheaderval( header, opt) char *header; HDR_FIELD opt;
{
	return headerval( header, opt, NULL);
}

char *
getuheaderval( header, opt) char *header; HDR_FIELD opt;
{
	return unpadd( headerval( header, opt, NULL));
}


char *
headerval( header, opt, value) char *header; HDR_FIELD opt; char *value;
{
   static char s[HEADER_SIZE];
   int         offset, length, n;

   switch (opt) {
      case H_USER:      offset =  0; length =  14; break;
      case H_PRINTER:   offset = 14; length =  14; break;
      case H_TYPE:      offset = 28; length =  10; break;
      case H_FORMLEN:   offset = 38; length =   3; break;
      case H_PAGES:     offset = 41; length =   4; break;
      case H_COPIES:    offset = 45; length =   2; break;
      case H_LONGEVITY: offset = 47; length =   1; break;
      case H_MAILME:    offset = 48; length =   1; break;
      case H_WRITEME:   offset = 49; length =   1; break;
      case H_DBASE:     offset = 50; length =  14; break;
      case H_FORMAT:    offset = 64; length =  14; break;
      case H_DSTAMP:    offset = 78; length =  10; break;
      case H_DESC:      offset = 88; length =  60; break;

      case H_ENTIRE:    offset =  0; length = HEADER_SIZE-2; 
                        header[HEADER_SIZE-1] = '\n';
                        header[HEADER_SIZE] = 0;
                        break;
   }

   if (value != NULL) {
      for (n = 0; n < length; n++) header[offset+n] = ' ';
      for (n = 0; value[n] && n < length; n++) header[offset+n] = value[n]; 
   }

   strncpy( s, &header[offset], length); s[length] = 0;

   return( s);
}




/******************************************************************************
   Log text messages into the log file.
******************************************************************************/

void
log( va_alist)
va_dcl
{
   va_list    args;
   FILE      *fp;
   time_t     clock;
   struct tm *tt;
   char       message[NBUF];


   va_start( args);  sprintf( message, "%r", args);  va_end( args); 

   MLP_lock( L_LOG);

   time( &clock); tt = localtime( &clock);

   if ((fp = fopen( LOG_PATH, "a+")) != NULL) {
      fprintf( fp, "%02d/%02d/%02d ", (tt->tm_mon+1), tt->tm_mday, tt->tm_year);
      fprintf( fp, "%02d:%02d - ", (tt->tm_hour), tt->tm_min);
      fprintf( fp, "%s\n", message);
      fclose( fp);
   }

   MLP_unlock( L_LOG);
}


/******************************************************************************
   Given a string, convert it to uppercase.
******************************************************************************/

char *
uppercase( str) register char *str;
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
strtoksep( s) register char *s;
{
	return strtok(s, SEP);
}


/******************************************************************************
  Given the name of a control-file parameter, return its value.  Lock the
  file so we can read/write it without it changing on us.  If the value of the
  string "newval", is non-null then assign the parameter this new new value.
******************************************************************************/

char *
parameters( param, newval, pathname, locktype)
char *param;
char *newval;
char *pathname;
LOCKTYPE locktype;
{
   static  char value[ WORKSTR];
   FILE   *fp, *ofp;
   char    tmp[WORKSTR], tmp1[WORKSTR], parameter[30], *c, tmppath[WORKSTR];

   sprintf( tmppath, "%s.tmp", pathname);
   strncpy( parameter, param, sizeof( parameter));  uppercase( parameter);
   value[0] = '\0';

   MLP_lock( locktype); 

   if ((fp = fopen( pathname, "r")) != NULL) {

      /* Opened for read, look for param or write new value. */
      if (newval[0] == '\0') {

	 /* Look for existing value. */
         while (!feof( fp)) {
            if (fgets( tmp, sizeof( tmp), fp)) {
            	for (c = tmp; *c && *c != '#'; c++); /* look for comment */
            	if ( *tmp && !*c) {
               		c = strtoksep( tmp);  uppercase( c);
               		if ( strequal( c, parameter)) {
                  		c = strtok( NULL, "\n");
                  		while (*c && (*c == ' ' || *c == '\t' || *c == '=')) c++;
                  		strcpy( value, c);
                  		break;
               		}
            	} /* if (*tmp...) */
	    } /* if (fgets(...) */
         } /* while() */
         fclose( fp);
      } else { 

	 /* Add or modify value. */
         if ((ofp = fopen( tmppath, "w")) != NULL) {

            while (!feof( fp)) {

               if (fgets( tmp, sizeof( tmp), fp))
		   strcpy( tmp1, tmp);
		else
		   tmp1[0] = '\0';

               if ( *tmp && *tmp != '#') {
                  c = strtoksep( tmp);
		  uppercase( c);

                  if ( !strequal( c, parameter)) {
                     fprintf( ofp, "%s", tmp1);		/* copy the line */
		  }
                } else {
                  fprintf( ofp, "%s", tmp1);		/* copy comment or blank */
		}
            } /* while (!feof(...)) */

            /* Append the new value. */
            fprintf( ofp, "%s = %s\n", param, newval);
            fclose( ofp);  fclose( fp);
            rename( tmppath, pathname);
         } else {
	    /* Cannot write temp file, bomb out. */
            fclose( fp);
            fatal("Cannot create %s", tmppath);
         }
      } /* if (newval[0] == 0) ... else ... */

   } else if ( errno == EACCES) {

         /* fopen() failed because of permissions. */
         fatal("Permission Problem with \"%s\".  It should have the suid bit set.", progname);

   } else if ( rename( tmppath, pathname) == 0) {

      /*
       * fopen() failed, presumably because pathname does not exist,
       * but tmppath does exist, so try again after renaming.
       * This seems wrongheaded to steve...
       */
      MLP_unlock( locktype);
      return parameters( param, newval, pathname, locktype);

   } else if ((fp = fopen( pathname, "w")) != NULL) {

      /* File did not exist, create it.  This seems wrong too. */
      fclose( fp);
      MLP_unlock( locktype);
      return parameters( param, newval, pathname, locktype);

   } else {
         fatal("Cannot create control file: %s", pathname);
   }

   MLP_unlock( locktype);
   return value;
}


/******************************************************************************
   Given a user name, return the name of the printer to which they are cur-
   rently routed.
******************************************************************************/

char *
route_request( user) char *user;
{
   static  char printer[15];
   FILE   *fp;
   char         path[WORKSTR], *c;

   sprintf( path, "%s/%s", ROUTE_PATH, user);

   printer[0] = '\0';

   if ((fp = fopen( path, "r")) != NULL) {
      fgets( printer, sizeof( printer), fp);
      fclose( fp);
   } else {
      strcpy( printer, controls( DEFAULT,""));

      if (printer[0] == 0)
         fatal( "Controls file (%s) needs a default printer entry", CONTROL_PATH);
   }

   if ( c = strchr( printer, '\n')) *c = 0;

   return printer;
}


/*****************************************************************************
   Return a sorted list of the spool queue.  The data structure returned is
   a two dimensional array ( entries x MAXNAMLEN).
*****************************************************************************/

char *
dirlist( dirname, descend)
char *dirname;
int   descend;
{
   DIR *dr;
   struct dirent *dp;
   char *t, tmp[MAXNAMLEN];
   int   size, top, swapit, i, j, gap, comp;

   dr = opendir( dirname);  t = xalloc(MAXNAMLEN);  size = 0;

   while ( ( dp = readdir( dr)) != NULL)
      if (strequal( ".", dp->d_name) || strequal( "..", dp->d_name))
         continue;
      else {
         strncpy( (t+(size * MAXNAMLEN)), dp->d_name, MAXNAMLEN);
         size++;
         if ((t = realloc( t, ((size+1) * MAXNAMLEN))) == NULL)
	    fatal("Out of space");
      }

   closedir( dr);

   strcpy( (t+(size * MAXNAMLEN)), "");


   /* Use a COMBsort to sort the items in this directory */
   /* This sort is like a bubble sort only MUCH faster */

   gap = size;

   do {
      gap = gap * 10 / 13;    if (gap == 0) gap = 1;

      top = size - gap;       swapit = 0;

      for ( i = 0; i < top; i++) {
         j = i + gap;
        
         comp = strcmp( (t+(i * MAXNAMLEN)), (t+(j * MAXNAMLEN)));

         if (descend)      /* Reverse the sort order */
            comp = ( comp > 0) ? 0 : 1;
       
         if ( comp > 0) {
            strcpy( tmp, (t+(i * MAXNAMLEN)));
            strcpy( (t+(i * MAXNAMLEN)), (t+(j * MAXNAMLEN)));
            strcpy( (t+(j * MAXNAMLEN)), tmp);

            swapit++;
         } 
      }
   } while ( swapit || gap > 1);
   return( t);
}


/******************************************************************************
   Wakeup The Despooler by sending it an alarm signal.  The despooler's PID
   will be found in the controls database.
******************************************************************************/

void wakeup_despooler()
{
   int    pid;

   pid = 0;
   sscanf( status( DESPOOLER,""), "%d", &pid);
   if ( pid > 0) kill( pid, SIGALRM);
}


/******************************************************************************
   Basename.  Strip the leading path information from a pathname.
******************************************************************************/

char *
basename( path) register char *path;
{
   register char *s;

   if (path == NULL) return "";
   return ((s = strrchr(path, '/')) == NULL) ? path : s + 1;
}


/******************************************************************************
   Return TRUE if the scheduler is running, FALSE if not.
******************************************************************************/

int
scheduler_status()
{
   int   pid;

   pid = 0;
   sscanf( status( DESPOOLER, ""), "%d", &pid);
   return ( pid > 0 && !kill( pid, 0));
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
   number of pages that it has processed.
******************************************************************************/

#define IOBUFF 1024

int 
writeOut( source, sink, start, end, formlen, report)
int source;
int sink;
int start;
int end;
int formlen;
int report;
{
   int   result, lines, progress;
   int   pages;
   long  rsize, oldpos;
   char  tmp[WORKSTR];
   char *i, *ibuff, *imax;  /* input buffer pointers */
   char *o, *obuff, *omax;  /* output buffer pointers */

   /* how big is the source file? */
 
   oldpos = lseek( source, 0L, 1);
   rsize  = lseek( source, 0L, 2);  lseek( source, oldpos, 0);

   
   ibuff = xalloc( IOBUFF);  obuff = xalloc( IOBUFF);

   o = obuff;  omax = obuff + IOBUFF;

   lines = 0;  pages = 1;
 
   while ( result = read( source, ibuff, IOBUFF)) {
      if (abort_write) {
         o = obuff;
         pages = -1;
         break;
      }

      if ( start == 0 && end == 0)
         write( sink, ibuff, result);   /* print entire request */
      else {

         /* print from start to end page of request */

         for ( i = ibuff, imax = ibuff + result; i < imax; i++) {

            if ( *i == '\n') {
               lines++;

               if ( lines > formlen) {
                  lines = 0;
                  /* lines -= formlen; */
                  pages++;
               }
            }
            else
            if ( *i == '\f') {
               pages++;
               lines = 0;
            }

            if ( pages >= start && pages <= end) {

               *o++ = *i;

               if ( o >= omax) {

                  write( sink, obuff, IOBUFF);
                  o = obuff;
               }
            }
         }
      }


      /* Report Progress to the status file */

      if (report) {
         progress = (int) (lseek( source, 0L, 1) * 100L / rsize);

         sprintf( tmp, "%d, %s, %s, %d, %d of %d", getpid(), curr_name, 
            curr_printer, progress, curr_copy, curr_copies);

         status( curr_device, tmp);
      }
   }

   /* write anything leftover in the output buffer */

   if ( o != obuff) write( sink, obuff, (int) (o - obuff)); 


   free( ibuff);  free( obuff);

   return( pages);
}


/******************************************************************************
   Given the sequence number of a request, return the full file name of
   the request.
******************************************************************************/

char *request_name( seqnum)
char *seqnum;
{
   DIR *dr;
   struct dirent *dp;
   int val;
   char tvalue[15];
   static char value[15];


   val = 0;  sscanf( seqnum, "%d", &val);   sprintf( tvalue, "%05u", val);

   dr = opendir( QUEUE_PATH);  value[0] = 0;


   while ( ( dp = readdir( dr)) != NULL)
      if (strequal( ".", dp->d_name) || strequal( "..", dp->d_name))
         continue;
      else {
         if (!strncmp( tvalue, &dp->d_name[2], 5)) {
            sprintf( value, "%s", dp->d_name);
            break;
         }
      }

   closedir( dr);

   return( value);
}


/****************************************************************************
   Given the report sequence number, return the owner's name
****************************************************************************/

char *
request_owner( seqnum) char *seqnum;
{
   static char owner[NNAME];
   char path[WORKSTR], head[HEADER_SIZE];
   int  fd;

   strcpy( owner, request_name( seqnum));

   if (*owner) {
      sprintf( path, "%s/%s", QUEUE_PATH, owner);

      if ( (fd = open( path, O_RDONLY)) != BAD) {
         read( fd, head, HEADER_SIZE);
         close( fd);

         strcpy( owner, getuheaderval( head, H_USER));
      }
   }
   else
      *owner = 0;

   return owner;
}


/****************************************************************************
  Return 1 if I (the program's user) own the request, 0 otherwise. If I'm
  superuser, then I own it all.
****************************************************************************/

int
do_i_own_it( seqnum) char *seqnum;
{
   struct passwd *p;
   
   p = getpwuid( getuid());
   return ((p && strequal( p->pw_name, request_owner( seqnum))) || is_root());
}

/******************************************************************************
   Report the status of all the printers defined in the control file.
   N.B. case E_FILE really returns a FILE *, gak...
******************************************************************************/

char *
each_printer( command) PRINTER_CMD command;
{
   static FILE *fp;
   static char  line[WORKSTR];
   char         tmp[WORKSTR], *t;

   *line = 0;

   switch (command) {
      case E_START: MLP_lock( L_CONTROL);

                    if ( (fp = fopen( CONTROL_PATH, "r+")) == NULL) {
                       sprintf( tmp, "%s.tmp", CONTROL_PATH);
                       MLP_unlock( L_CONTROL);
                       if ( errno == EACCES)
                          fatal("Permission Problem with \"%s\".  It should have the suid bit set.", progname);
                       else if ( rename( tmp, CONTROL_PATH) == 0) {
                          return( each_printer( command));
                       } else
                          fatal( "Missing control file: %s", CONTROL_PATH);
                    }
                    break;

      case E_NEXT:  while (fgets( tmp, sizeof(tmp), fp)) {

                       t = strtoksep( tmp); uppercase( t);
                       if (strequal( "PRINTER", t)) {
                          for (t = tmp; *t; t++); t++;
                          while (strchr(SEP, *t)) t++;
                          strcpy( line, t);
                          break;
                       }
                    }
                    break;

      case E_REWIND: fseek( fp, 0L, SEEK_SET); break;

      case E_END:    fclose( fp);
      case E_UNLOCK: MLP_unlock( L_CONTROL); break;

      /* N.B. case E_FILE really returns a FILE *, gak... */
      case E_FILE:   return( (char *) fp);
   }

   return( line);
}

char *
report_printer_status( stype) STATUS_TYPE stype;
{
   static char request_list[WORKSTR];
   int    pid, req;
   char   tmp[NPRINTER], *t, *c, *d, *header, rpath[WORKSTR];
   char   request[NNAME], user[NNAME], timestr[NTIME];
   long   rtime;

   request_list[0] = 0;  /* will contain a list of all the request that are
                            currently printing.  This is so we can skip these
                            when we go over the list of all pending requests.*/


   each_printer( E_START);

   while ( *(t = each_printer( E_NEXT))) {

         *tmp = *curr_printer = *curr_device = *curr_backend = 0;   

         c = strtoksep( t   ); if (c) strncpy( curr_printer, c, WORKSTR);
         c = strtoksep( NULL); if (c) strncpy( curr_device,  c, WORKSTR);
         c = strtoksep( NULL); if (c) strncpy( curr_backend, c, WORKSTR);


         /*** Now look at this printer from the status database ***/

         strcpy( tmp, status( curr_device, ""));


         c = strtoksep( tmp);  pid = 0; sscanf( c, "%d", &pid);

         if (pid > 0 && kill( pid, 0)) c = NULL;

         switch (stype) {
         case PID_DISPLAY:
            if (c != NULL) {
               c = strtoksep( NULL);
               if (c) sprintf( timestr, "%d|%s ", pid, (c+2));

               if ((strlen( request_list)+strlen( timestr))
                     < sizeof( request_list))
                  strcat( request_list, timestr);
            }
         break;

         case DEVICE_DISPLAY:
            printf("device for %s: %s\n", curr_printer, curr_device);
         break;

         case PRINTER_DISPLAY:
            *request = 0; if ( d = strtoksep( NULL)) strcpy( request, d);

            d = strtoksep( NULL);

            if ( c == NULL || d == NULL || !strequal(d, curr_printer))
               printf("printer %s is idle\n", curr_printer);
            else {
               printf("printer %s is printing request #", curr_printer);
               printf("%s, ", &request[2]);

               if ( c = strtoksep( NULL))
                  printf("%s%% sent to device", c);

               if ( c = strtok( NULL, ","))
                  printf(", copy %s", c);
   
               printf("\n");
            }
         break;

         case REQUEST_DISPLAY:
            if ( c != NULL) {
               strcpy( request, strtoksep( NULL));
               sprintf( rpath, "%s/%s", QUEUE_PATH, request);

               if ((c = strtoksep( NULL)) && !strcpy( c, curr_printer)) {   
              
                  if ( (req = open( rpath, O_RDONLY)) != BAD) {
                     header = xalloc( HEADER_SIZE);
                     read( req, header, HEADER_SIZE);
                     close( req);

                     strcpy( user, getuheaderval( header, H_USER));

                     strcpy( timestr, getheaderval( header, H_DSTAMP));
                     rtime = 0L; sscanf( timestr, "%ld", &rtime);
                     strcpy( timestr, ctime(&rtime));
                     timestr[strlen(timestr)-1] = 0;

                     free( header);

                     printf( "%s-%s\t\t%c\t%s", user,
                        (request+2), *(request+1), timestr);

                     printf( "\ton %s\n", curr_printer);


                     sprintf( timestr, "%s|", request);
                     if ((strlen( request_list)+strlen( timestr))
                          < sizeof( request_list))
                        strcat( request_list, timestr);
                  }
               }                  
            }

         break;
         }
   }

   each_printer( E_END);

   return( request_list);
}


/******************************************************************************
   Cancel currently printing requests (not pending ones!).  Kill only the ones
   I own.  If I'm superuser, then I own it all.
******************************************************************************/

int
kill_printing_request( seqnum, sig, flag)
int  seqnum;
int  sig;
KILL_FLAG  flag;
{
   int   pid, seq, sucessfull = FALSE;
   char *list, *t;

   list = report_printer_status( PID_DISPLAY);

   t = strtok( list, " ");

   do {
      if (t) {
         sscanf( t, "%d|%d", &pid, &seq);

         if ( flag || (seq == seqnum)) 
            if (do_i_own_it( seqnum)) {
               kill( pid, sig);
               sucessfull = TRUE;
            }

         t = strtok( NULL, " ");
      }
   } while (t);

   if (flag) log("Aborting currently printing requests");

   return( sucessfull);
}


/******************************************************************************
   MLP_lock.  This routine can lock several critical sections involving MLP
   process.  These are: Cancel, Controls and Status.  The controls and status
   files need to be locked while they are modified.  The Cancel lock keeps
   processes that cancel requests out of the hair of the despooler and vice-
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
/* Code using semget() and semop(), from original source; does not work, hangs! */
int
MLP_lock( which) LOCKTYPE which;
{
   static struct sembuf lcancel[2]  = { 0,0,0,  0,1,SEM_UNDO };
   static struct sembuf lcontrol[2] = { 1,0,0,  1,1,SEM_UNDO };
   static struct sembuf lstatus[2]  = { 2,0,0,  2,1,SEM_UNDO };
   static struct sembuf llog[2]     = { 3,0,0,  3,1,SEM_UNDO };
   key_t k;

   if (sID == BAD) {
      /* First time, execute semget(). */
      k = ftok( IPC_NAME, 'A');
      if ((sID = semget( k, IPC_SEMS, IPC_PERMS|IPC_CREAT|IPC_EXCL)) == BAD
       && (sID = semget( k, IPC_SEMS, 0)) == BAD)
            fatal("cannot get semaphores");
   }

   switch (which) {
      case L_CANCEL:  semop( sID, lcancel,  2); break;
      case L_CONTROL: semop( sID, lcontrol, 2); break;
      case L_STATUS:  semop( sID, lstatus,  2); break;
      case L_LOG:     semop( sID, llog,     2); break;
   }
}

int
MLP_unlock( which) LOCKTYPE which;
{
   static struct sembuf lcancel[1]  = { 0,-1, IPC_NOWAIT|SEM_UNDO };
   static struct sembuf lcontrol[1] = { 1,-1, IPC_NOWAIT|SEM_UNDO };
   static struct sembuf lstatus[1]  = { 2,-1, IPC_NOWAIT|SEM_UNDO };
   static struct sembuf llog[1]  =    { 3,-1, IPC_NOWAIT|SEM_UNDO };
#if	0
   key_t k;

   k = ftok( IPC_NAME, 'A');

   if ((sID = semget( k, IPC_SEMS, IPC_PERMS|IPC_CREAT|IPC_EXCL)) == BAD) {
      if ((sID = semget( k, IPC_SEMS, 0)) == BAD) {
         return;
      }
   }
#else
   if (sID == BAD)
      fatal("no semaphores in MLP_unlock()");
#endif
   switch (which) {
      case L_CANCEL:  semop( sID, lcancel,  1); break;
      case L_CONTROL: semop( sID, lcontrol, 1); break;
      case L_STATUS:  semop( sID, lstatus,  1); break;
      case L_LOG:     semop( sID, llog,     1); break;
   }
}

#else					/* !USE_SEM */

int locks[IPC_SEMS];			/* lockfile fd's */

int
MLP_lock( which) LOCKTYPE which;
{
   char path[WORKSTR];

   sprintf( path, "%s/LCK..%d", SPOOLDIR, which);
   if ((locks[which] = open( path, O_RDWR|O_CREAT, CREAT_PERMS)) != BAD
    && lockf( locks[which], F_LOCK, 0L) != BAD)
      return 1;
   warning_user("root", "MLP_lock(%s) failed", path);
   return 0;
}

int
MLP_unlock( which) LOCKTYPE which;
{
   char path[WORKSTR];

   sprintf( path, "%s/LCK..%d", SPOOLDIR, which);
   if (lockf( locks[which], F_ULOCK, 0L) != BAD
    && close( locks[which]) != BAD)
      return 1;
   warning_user("root", "MLP_unlock(%s) failed", path);
   return 0;
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

/* end of mlp/spooler.c */
