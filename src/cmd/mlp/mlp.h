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

/* Version number. */
#define	VERSION	"V1.6"

/* File permissions. */
#define	CREAT_PERMS	0644
#define	UMASK_PERMS	012

/* Pathnames. */
#define	SPOOLDIR	"/usr/spool/mlp"
#define	BACKEND_PATH	SPOOLDIR "/backend"
#define	CONTROL_PATH	SPOOLDIR "/controls"
#define	LOG_PATH	SPOOLDIR "/log"
#define	QUEUE_PATH	SPOOLDIR "/queue"
#define	ROUTE_PATH	SPOOLDIR "/route"
#define	STATUS_PATH	SPOOLDIR "/status"

/* Environment variable names. */
#define	COPIES_ENV	"MLP_COPIES"
#define	FORMLEN_ENV	"MLP_FORMLEN"
#define	LONGEVITY_ENV	"MLP_LIFE"
#define	PRIORITY_ENV	"MLP_PRIORITY"
#define	SPOOL_ENV	"MLP_SPOOL"

/* Control and status file entry names. */
#define	DBACKEND	"/bin/cat"		/* default backend script */
#define	DEFAULT		"default"
#define	DESPOOLER	"despooler"
#define	DISPLAY		"display"
#define	DOCOPIES	"docopies"
#define	FEED		"feed"
#define	LLIFE		"longlife"
#define	LOCAL		"local"
#define	LOCALFEED	"localfeed"
#define	LOGROLL		"logroll"
#define	SEQNUM		"seqnum"
#define	SLIFE		"shortlife"
#define	SYSTEM		"system"
#define	TLIFE		"templife"

/* Shorthand. */
#define	is_root()	(getuid() == 0)
#define	strequal(x, y)	(strcmp((x), (y)) == 0)

/*
 * Buffer sizes.
 * STEVE: Blatchley's source uses constants indiscriminately,
 * many of these are not particularly meaningful.
 */
#define	DESC_LEN	45
#define	HEADER_SIZE	192
#define	NBUF		1024
#define	NBUF2		512
#define	NNAME		20
#define	NNUM		11		/* strlen(long in decimal ASCII) + 1 */
#define	NPRINTER	15		/* 14 + 1 */
#define	NTIME		40
#define	WORKSTR		128

#define	BAD		-1
#define	FALSE		0
#define	TRUE		1
#define	SEP  "=,\t \n"

/* Signals. */
#define	R_KILL		SIGUSR1
#define	R_CANCEL	SIGUSR2

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

/* Commands to each_printer(). */
typedef	enum	printer_cmd	{
	E_START, E_END, E_NEXT, E_REWIND, E_UNLOCK
} PRINTER_CMD;

/* Parameter types for spooler.c/param(). */
typedef	enum	paramtype	{
	P_GET,	P_GETLOCK,	P_SET,	P_SETULOCK
} PARAMTYPE;

#if	USE_SEM
/* Semaphores. */
#define	IPC_NAME	SPOOLDIR
#define	IPC_PERMS	0666
#define	IPC_SEMS	4
#endif

/* Lock types. */
#define	NLOCKS	4		/* number of lock types */
typedef	enum	locktype	{
	L_CANCEL = 0, L_CONTROL, L_STATUS, L_LOG
} LOCKTYPE; 

/* Macros (previously implemented as functions in spooler.c). */
#define	getparam(name, lock)		param((name), NULL,  (lock), P_GET)
#define	setparam(name, val, lock)	param((name), (val), (lock), P_SET)
#define	getcontrol(name)		getparam((name), L_CONTROL)
#define	getstatus(name)			getparam((name), L_STATUS)
#define	setcontrol(name, val)		setparam((name), (val), L_CONTROL)
#define	setstatus(name, val)		setparam((name), (val), L_STATUS)

/* Functions in spooler.c. */
extern	void	error();
extern	void	warning();
extern	void	warning_user();
extern	void	fatal();
extern	char	*string();
extern	char	*makepath();
extern	char	*makepath2();
extern	char	*get_username();
extern	char	*unpadd();
extern	int	rename();
extern	void	make_inactive();
extern	char	*gethdrval();
extern	char	*getuhdrval();
extern	char	*hdrval();
extern	void	log();
extern	char	*uppercase();
extern	char	*strtoksep();
extern	char	*param();
extern	char	*getroute();
extern	char	*queuelist();
extern	int	wakeup_scheduler();
extern	void	getname();
extern	int	scheduler_status();
extern	int	writeOut();
extern	char	*request_name();
extern	int	read_hdr();
extern	char	*request_owner();
extern	int	do_i_own_it();
extern	char	*each_printer();
extern	char	*report_printer_status();
extern	int	kill_printing_request();
extern	int	MLP_lock();
extern	int	MLP_unlock();
extern	char	*xalloc();
extern	FILE	*xopen();

/* Globals defined in spooler.c. */
extern	int	abort_write;
extern	char	curr_backend[];
extern	int	curr_copies;
extern	int	curr_copy;
extern	char	curr_device[];
extern	int	curr_formlen;
extern	char	curr_name[];
extern	char	curr_printer[];
extern	char	progname[];

/* end of mlp/mlp.h */
