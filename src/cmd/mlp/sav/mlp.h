/* mlp/mlp.h */
/******************************************************************************
   MLP Spooler header
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#if	USE_SEM
#include <sys/ipc.h>
#include <sys/sem.h>
#endif
#include <sys/stat.h>
#include <signal.h>
#include <dirent.h>
#include <varargs.h>
#include <errno.h>
#include <pwd.h>

/* File permissions. */
#define	UMASK_PERMS	012
#define	CREAT_PERMS	0644

/* Pathnames and file info strings. */
#define	SPOOLDIR	"/usr/spool/mlp"
#define BACKEND_PATH	SPOOLDIR "/backend"
#define ROUTE_PATH	SPOOLDIR "/route"
#define QUEUE_PATH	SPOOLDIR "/queue"
#define CONTROL_PATH	SPOOLDIR "/controls"
#define STATUS_PATH	SPOOLDIR "/status"
#define LOG_PATH	SPOOLDIR "/log"
#define SPOOL_ENV	"MLP_SPOOL"
#define PRIORITY_ENV	"MLP_PRIORITY"
#define FORMLEN_ENV	"MLP_FORMLEN"
#define COPIES_ENV	"MLP_COPIES"
#define LONGEVITY_ENV	"MLP_LIFE"
#define SEQNUM		"seqnum"
#define SYSTEM		"system"
#define DISPLAY		"display"
#define LOCAL		"local"
#define DESPOOLER	"despooler"
#define LOCALFEED	"localfeed"
#define FEED		"feed"
#define DOCOPIES	"docopies"
#define DEFAULT		"default"
#define TLIFE		"templife"
#define SLIFE		"shortlife"
#define LLIFE		"longlife"
#define	LOGROLL		"logroll"
#define DBACKEND	"/bin/cat"

/* Mail message header and footer, cf. spooler.c/warning(). */
#define	MSGHEAD	"/bin/echo \"\n`date`\n\"'\n*** Message From The MLP Spooler ***\n\n"
#define	MSGTAIL (getenv("MLPLMAIL")) ? "\n\n' | lmail " : "\n\n' | mail "

/* Shorthand. */
#define	is_root()	(getuid() == 0)
#define	strequal(x, y)	(strcmp((x), (y)) == 0)

/* Buffer sizes. */
#define DESC_LEN	45
#define HEADER_SIZE	192
#define	NBUF		1024
#define	NBUF2		512
#define	NNAME		20
#define	NPRINTER	40
#define	NTIME		40
#define WORKSTR		128

#define TRUE		1
#define FALSE		0
#define BAD		-1

/* Header fields, passed to headerval(). */
typedef	enum	hdr_field	{
	H_USER,		H_PRINTER,	H_TYPE,		H_FORMLEN,
	H_PAGES,	H_DBASE,	H_FORMAT,	H_DSTAMP,
	H_DESC,		H_LONGEVITY,	H_COPIES,	H_MAILME,
	H_WRITEME,	H_ENTIRE
} HDR_FIELD;

/* Arguments to report_printer_status(). */
typedef	enum	status_type	{
	PRINTER_DISPLAY, REQUEST_DISPLAY, DEVICE_DISPLAY, PID_DISPLAY
} STATUS_TYPE;

/* Flags to kill_printing_request(). */
typedef	enum	kill_flag { CLEAR_ONE = FALSE, CLEAR_ALL = TRUE } KILL_FLAG;

/* Commands to each_printer(). */
typedef	enum	printer_cmd	{
	E_START, E_END, E_NEXT, E_REWIND, E_FILE, E_UNLOCK
} PRINTER_CMD;

/* Signals. */
#define R_KILL    SIGUSR1
#define R_CANCEL  SIGUSR2

#define SEP  "=,\t \n"

#if	USE_SEM
#define	IPC_NAME	SPOOLDIR
#define	IPC_PERMS	0666
#endif
/* Lock types. */
#define	IPC_SEMS	4		/* number of lock types */
typedef	enum	locktype	{
	L_CANCEL = 0, L_CONTROL, L_STATUS, L_LOG
} LOCKTYPE; 

/* Macros (previously implemented as functions in spooler.c). */
#define	controls(param, val)	parameters((param), (val), CONTROL_PATH, L_CONTROL)
#define	status(param, val)	parameters((param), (val), STATUS_PATH, L_STATUS)

/* Externals in libterm.a. */
extern	char	*tgetstr();
extern	int	tgetent();

/* Functions in spooler.c. */
extern	void	bedaemon();
extern	void	warning();
extern	void	warning_user();
extern	void	fatal();
extern	char	*unpadd();
extern	int	rename();
extern	void	make_inactive();
extern	char	*getheaderval();
extern	char	*getuheaderval();
extern	char	*headerval();
extern	void	log();
extern	char	*uppercase();
extern	char	*strtoksep();
extern	char	*parameters();
extern	char	*route_request();
extern	char	*dirlist();
extern	void	wakeup_despooler();
extern	char	*basename();
extern	int	scheduler_status();
extern	int	writeOut();
extern	char	*request_name();
extern	char	*request_owner();
extern	int	do_i_own_it();
extern	char	*each_printer();
extern	char	*report_printer_status();
extern	int	kill_printing_request();
extern	int	MLP_lock();
extern	int	MLP_unlock();
extern	char	*xalloc();

/* Globals defined in spooler.c. */
extern	char	progname[];
extern	char	curr_printer[];
extern	char	curr_device[];
extern	char	curr_backend[];
extern	char	curr_name[];
extern	int	curr_copies;
extern	int	curr_copy;
extern	int	curr_formlen;
extern	int	abort_write;

/* end of mlp/mlp.h */
