/*
 * fdisk.c
 *
 * Change partitioning IBM-AT (or other type) hard disk.
 * Usage: /etc/fdisk [ -crvxB ] [ -b bootb ] [ device ... ]
 * Options:
 *	-B	Invoked during installation
 *	-b	Add master boot block code from "bootb"
 *	-c	Configure hard disk geometry
 *	-r	Read only
 *	-v	Print c:h:s start and end values
 */

#include <sys/compat.h>

#include <coff.h>
#include <fcntl.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/devices.h>
#include <sys/fdisk.h>
#include <sys/hdioctl.h>
#include <sys/scsiwork.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define SAVE_BOOT 0

/* ********* Start of erstwhile fdisk0.h ******************** */
#define	USAGE		"Usage: /etc/fdisk [ -Bcrsv ] [ -b mboot ] [ device ... ]\n"
#define	VERSION		"3.0"
#define	COH		"/coherent"
#define	KMEM		"/dev/kmemhi"
#define	NBUF		512	/* buffer size			*/
#define	SSIZE		512	/* sector size			*/

#define PATCHFILE	"/tmp/patches" /* WARNING! duplicated in build0.h */

/*
 * Conversions.
 * (unsigned) c:h:s to (ulong) sectors,
 * (ulong) sectors to (unsigned) c:h:s.
 */
#define	chs_to_sec(c,h,s) ((((unsigned long)(c)*nheads) + (h)) * nspt + (s) - 1)
#define	sec_to_c(sec)	((unsigned)((sec) / cylsize))
#define	sec_to_h(sec)	((unsigned)(((sec) / nspt) % nheads))
#define	sec_to_s(sec)	((unsigned)(((sec) % nspt) + 1))
/* (ulong) Sectors to (double) megabytes. */
#define	meg(sec)	(((double)(sec)) * SSIZE / 1000000L)
/* (ulong) Sectors to (unsigned) cylinders and tracks, rounding up. */
#define	sec_upto_c(sec)	(sec_to_c((sec) + nspt * nheads - 1))
#define	sec_upto_t(sec)	((unsigned)(((sec) + nspt - 1) / nspt))

/* ********* End of erstwhile fdisk0.h ******************** */

void cohtune_ent __PROTO((char *devName, char *tag, char *line)
);

/* Globals. */

int	Bflag;			/* special patching during installation */
char	*argv0;			/* Command name, for error messages.	*/
int	badflag;		/* Partition table is bad.		*/
char	buf[NBUF];		/* Input buffer.			*/
int	cfd;			/* Current file descriptor.		*/
int	cflag;			/* Configure disk geometry.		*/
int	cylflag;		/* Specify base and size in cylinders.	*/
unsigned int	cylsize;	/* Cylinder size in sectors.		*/
char	*device;		/* Partition table device name.		*/
char	drive_x[] = "Drive x";  /* Needs to be written on	*/
int	drivenum;		/* Drive number.			*/
int	freepart;		/* Free partition.			*/
unsigned long	freesize;	/* Free size.				*/
unsigned long	freestart;	/* First free sector.			*/
HDISK_S		hd;		/* Structure to house boot block.	*/
hdparm_t	hdparms;	/* Hard disk parameter block.		*/
int	isatflag;		/* Device is an AT-type disk.		*/
jmp_buf		loop;		/* Interactive input loop entry point.	*/
char	*mboot;			/* Name of new master boot file.	*/
int	megflag;		/* Specify sizes in megabytes.		*/
unsigned int	nspt;		/* Number of sectors per track.		*/
unsigned int	ncyls;		/* Number of cylinders.			*/
HDISK_S		newhd;		/* Structure to house new boot block.	*/
unsigned int	nheads;		/* Number of heads per track.		*/
int	nmods;			/* Modifications to the table.		*/
unsigned long	nsectors;	/* Total sectors.			*/
int	openmode = 2;		/* Default open mode: read/write.	*/
int	partbase;		/* Partition number base (0 or 4).	*/
int	qflag;			/* Quit.				*/
int	rflag;			/* Readonly.				*/
int	sflag;			/* Readonly.				*/
int	vflag;			/* Print c:h:s start and end values.	*/
int	cohtuneFlag;		/* cohtune was called			*/

char	drivename[40];		/* Disk drive name.			*/

int	warned;			/* Set if user has been warned about chs */

/******************************************************************
 *
 * get_line
 *
 * Print the args and get a line from the user to buf[].
 * Strip the trailing newline and return a pointer to the first non-space.
 *****************************************************************/

#if USE_PROTO
char	*get_line(char *format, ...)
#else
char	*
get_line(format)
char	*format;
#endif
{
	char	*s;
	va_list		args;

	va_start(args, format);
	vprintf (format, args);
	va_end(args);

	fflush(stdout);

	fgets(buf, sizeof buf, stdin);
	buf[strlen(buf) - 1] = '\0';

	for (s = buf; ; ++s) {
		if (*s == 0x1B)			/* <Esc> returns to loop */
			longjmp(loop, 1);
		else if (*s != ' ' && *s != '\t')
			return s;
	}
}


/******************************************************************
 *
 * yes_no
 *
 * Get the answer to a yes/no question.
 * Return 1 for yes, 0 for no.
 *****************************************************************/

#if USE_PROTO
int	yes_no (char *format, ...)
#else
int
yes_no (format)
char	*format;
#endif
{
	char	*s;
	va_list		args;

	for (; ; ) {
		va_start(args, format);
		vprintf (format, args);
		va_end(args);

		s = get_line(" [y or n]? ");
		if (*s == 'y')
			return 1;
		else if (*s == 'n')
			return 0;
	}
}


/******************************************************************
 *
 * get_int
 *
 * Prompt for integer input from the user.
 * Accept data in range min to max.
 * Return a valid result.
 *****************************************************************/

#if USE_PROTO
int	get_int(char *prompt, int defval, int min, int max)
#else
int
get_int(prompt, defval, min, max)
char	*prompt;
int	defval;
int	min;
int	max;
#endif
{
	int	val;
	char	*s;

	for (; ; ) {
		s = get_line("%s [%u]? ", prompt, defval);

		if (*s == '\0')
			return defval;		/* take default */

		val = atoi(s);

		if (val >= min && val <= max)
			return val;

		printf("Please enter a value between %u and %u.\n", min, max);
	}
}


/******************************************************************
 *
 * change_active
 *
 * Change the active partition.
 *****************************************************************/

#if USE_PROTO
void change_active(void)
#else
void
change_active()
#endif
{
	int	active, oactive, i;

	active = oactive = -1;

	for (i = 0; i < NPARTN; i++)
		if (hd.hd_partn[i].p_boot == 0x80) {
			hd.hd_partn[i].p_boot = 0;	/* make inactive */
			active = oactive = i;		/* remember old */
		}

	if (!yes_no("Do you want to make a partition active")) {
		active = -1;

		if (active != oactive)
			++nmods;

		return;
	}

	if (active == -1)
		active = 0;				/* default */

	active = get_int("Active partition", active + partbase, partbase,
	    partbase + NPARTN - 1);
	active -= partbase;
	hd.hd_partn[active].p_boot = 0x80;	/* make active */

	if (active != oactive)
		++nmods;
}


/******************************************************************
 *
 * cls
 *
 * Clear the IBM AT console screen.
 * Prompt for <Enter> if the flag is true or if dflag.
 *****************************************************************/

#if USE_PROTO
void cls(int flag)
#else
void
cls(flag)
int	flag;
#endif
{
	if (flag)
		get_line("\nHit <Enter> to continue...");
	putchar('\n');
	fflush(stdout);
}


/******************************************************************
 *
 * sys_type
 *
 * Convert system type code i to a string describing the system type.
 * Return a pointer to statically allocated buffer.
 *****************************************************************/

#if USE_PROTO
char	*sys_type (int i)
#else
char	*
sys_type (i)
int	i;
#endif
{
	static char	buf[8+1];	/* longest name is "COHERENT" or "<Unused>"XS */
	char	*s;

	switch (i) {
	case SYS_EMPTY:		
		s = "<Unused>";	
		break;
	case SYS_DOS_12:
	case SYS_DOS_16:
	case SYS_DOS_LARGE:
		s = "MS-DOS";	
		break;
	case SYS_DOS_XP:
		s = "Ext.DOS";	
		break;
	case SYS_XENIX:		
		s = "Xenix";	
		break;
	case SYS_COH:		
		s = "Coherent";	
		break;
	case SYS_SWAP:		
		s = "Swap";	
		break;
	default:		
		s = NULL;	
		break;
	}

	if (s == NULL)
		sprintf(buf, "%8u", i);
	else
		strcpy(buf, s);
	return buf;
}


/******************************************************************
 *
 * fatal
 *
 * Print a fatal error message and die.
 *****************************************************************/

#if USE_PROTO
void fatal (char *format, ...)
#else
void
fatal (format)
char	*format;
#endif
{
	va_list		args;

	va_start(args, format);
	fprintf (stderr, "%s: ", argv0);
	vfprintf (stderr, format, args);
	fprintf (stderr, "\n");
	va_end(args);

	exit(1);
}


/******************************************************************
 *
 * sys
 *
 * Execute a command.
 *****************************************************************/

#if USE_PROTO
void sys (char *cmd)
#else
void
sys (cmd)
char	*cmd;
#endif
{
	if (system(cmd) != 0)
		fatal("fdisk : command \"%s\" failed", cmd);
}

/******************************************************************
 *
 * atpatch
 *
 *****************************************************************/

#if USE_PROTO
void atpatch(void)
#else
void
atpatch()
#endif
{
	int	i;
	int	dbase;
	unsigned char	*hdp;
	FILE * fp;
	char	line[512];
	char	line2[512];

	if (drivenum == 0)
		dbase = 0;
	else if (drivenum == 1)
		dbase = sizeof(hdparms);
	else
		fatal("unrecognized drive number");

	/* "Secondary kernel" is kernel booted just before loading
	 * diskette #2 during COH installation. */

	if (Bflag) {
		/*
		 * Secondary kernel.
		 * Write commands to patchfile.
		 * They run after kernel is linked.
		 */
		fp = fopen(PATCHFILE, "a");
		fprintf(fp,  "/conf/patch /mnt/coherent >/dev/null \\\n");

		for (i = 0, hdp = (unsigned char *) & hdparms; 
		    i < sizeof hdparms; i++, hdp++)
			fprintf(fp, "  atparm+%d=%u:c \\\n", dbase + i, *hdp);

		fprintf(fp, "\n");
		fclose(fp);

		/* Third kernel, if installing. */
		sprintf(line, "_TAG(AT%d)\t_HDPARMS(%d,%d,%d,%d,%d),",
		    drivenum,
		    (hdparms.ncyl[1] << 8) | hdparms.ncyl[0],
		    hdparms.nhead,
		    hdparms.nspt,
		    hdparms.ctrl,
		    (hdparms.wpcc[1] << 8) | hdparms.wpcc[0]);

		cohtune_ent("at", drivenum ? "AT1" : "AT0", line);

	} else {

		/* Not installing.  Invoke /etc/conf/bin/cohtune directly. */

		sprintf(line, "_TAG(AT%d)\t_HDPARMS(%d,%d,%d,%d,%d),",
		    drivenum,
		    (hdparms.ncyl[1] << 8) | hdparms.ncyl[0],
		    hdparms.nhead,
		    hdparms.nspt,
		    hdparms.ctrl,
		    (hdparms.wpcc[1] << 8) | hdparms.wpcc[0]);

		sprintf (line2, "/etc/conf/bin/cohtune at %s \"%s\"",
		  drivenum ? "AT1" : "AT0", line);
		sys (line2);
		cohtuneFlag = 1;
	}
}

/******************************************************************
 *
 * scsipatch
 *
 *****************************************************************/

#if USE_PROTO
void scsipatch(void)
#else
void
scsipatch()
#endif
{
	int		i;
	int		dbase;
	FILE		* fp;
	char		line[512];
	char		line2[512];
	_drv_parm_t	sdparm;
	char		* cp;
	char		drivetag [8];

	dbase = drivenum * sizeof sdparm;

	/* Convert from hdparms to the scsi struct, _drv_parm_t. */
	sdparm.ncyl = _CHAR2_TO_USHORT(hdparms.ncyl);
	sdparm.nhead = hdparms.nhead;
	sdparm.nspt = hdparms.nspt;

	/* "Secondary kernel" is kernel booted just before loading
	 * diskette #2 during COH installation. */

	if (Bflag) {
		/*
		 * Secondary kernel.
		 * Write commands to patchfile.
		 * They run after kernel is linked.
		 */
		fp = fopen(PATCHFILE, "a");
		fprintf(fp,  "/conf/patch /mnt/coherent >/dev/null \\\n");

		for (i = 0, cp = (char *) & sdparm; i < sizeof sdparm; i++, cp++)
			fprintf(fp, "  _sd_drv_parm+%d=%u:c \\\n", dbase + i, *cp);

		fprintf(fp, "\n");
		fclose(fp);

		/* Third kernel. */
		sprintf(line, "_TAG(SD%d)\t{%d, %d, %d},",
		    drivenum, sdparm.ncyl, sdparm.nhead, sdparm.nspt);

		/* drivetag is "SD0", "SD1", or whatever, depending on drivenum */
		sprintf (drivetag, "SD%d", drivenum);

		/* For interim when we ship both haiss and ss, _sd_drv_parm is
		   in /etc/conf/cohmain/Space.c;  once we drop old "ss" from COH,
		   it should move to /etc/conf/hai/Space.c */

		if (access ("/etc/conf/ss", F_OK) == 0) {
			/* tune _sd_drv_parm in /etc/conf/cohmain - interim kluge */
			cohtune_ent ("cohmain", drivetag, line);
		} else {
			/* tune _sd_drv_parm in /etc/conf/hai */
			cohtune_ent ("hai", drivetag, line);
		}
	} else {

		/* Not installing.  Invoke /etc/conf/bin/cohtune directly. */

		sprintf(line, "_TAG(SD%d)\t{%d, %d, %d},",
		    drivenum, sdparm.ncyl, sdparm.nhead, sdparm.nspt);

		/* drivetag is "SD0", "SD1", or whatever, depending on drivenum */
		sprintf (drivetag, "SD%d", drivenum);

		/* For interim when we ship both haiss and ss, _sd_drv_parm is
		   in /etc/conf/cohmain/Space.c;  once we drop old "ss" from COH,
		   it should move to /etc/conf/hai/Space.c */

		if (access ("/etc/conf/ss", F_OK) == 0) {

			/* tune _sd_drv_parm in /etc/conf/cohmain - interim kluge */
			sprintf (line2,
			  "/etc/conf/bin/cohtune cohmain %s \"%s\"",
			  drivetag, line);

		} else {

			/* tune _sd_drv_parm in /etc/conf/hai */
			sprintf (line2,
			  "/etc/conf/bin/cohtune hai %s \"%s\"",
			  drivetag, line);

		}

		sys (line2);
		cohtuneFlag = 1;
	}
}


/******************************************************************
 *
 * fix_chs
 *
 * Interactively obtain new disk geometry values.
 * Update running /coherent with correct values using HDSETA.
 * Call atpatch to create patched /tmp/coherent and /tmp/boot.[01].
 ******************************************************************/

#if USE_PROTO
void fix_chs(void)
#else
void
fix_chs()
#endif
{
	int	i;

	printf(
"Warning: if you specify incorrect disk parameter values, data on\n"
"existing partitions may be lost or your disk may not operate correctly.\n"
"Consult your disk controller manual or call your disk vendor\n"
"if you do not know the correct values.\n"
	    );

	if (!yes_no(
"Are you sure you want to change the disk parameter values"))
		return;

	/*
	 * Modify current values before displaying them as defaults.
	 */
	i = (hdparms.wpcc[1] << 8) | (hdparms.wpcc[0]);

	if (i <= -1 || i >= ncyls)
		i = 65535;

	hdparms.ctrl &= 0x0f;

	ncyls = get_int("Number of cylinders", ncyls, 1, 65536);
	/* NOTE: according to IDE specs, nheads maxes at 16 */
	nheads = get_int("Number of heads", nheads, 1, 255);
	nspt = get_int("Number of sectors per track", nspt, 1, 255);
	hdparms.ctrl = get_int("Control byte", hdparms.ctrl, 0, 255);
	i = get_int("Write precompensation cylinder", i, 0, ncyls + 1);

	hdparms.wpcc[1] = i >> 8;
	hdparms.wpcc[0] = i & 0xFF;
	cylsize = nheads * nspt;
	nsectors = (long)ncyls * cylsize;
	hdparms.ncyl[1] = ncyls >> 8;
	hdparms.ncyl[0] = ncyls & 0xFF;
	hdparms.nhead = nheads;
	hdparms.nspt = nspt;

	if (ioctl(cfd, HDSETA, (char *) & hdparms) == -1)
		fatal("cannot set \"%s\" drive characteristics", device);

	if (isatflag)
		atpatch ();
	else
		scsipatch ();

	/* If parameters were changed, we may again issue c/h/s warnings. */
	warned = 0;
}


/******************************************************************
 *
 * check_chs
 *
 * Check a c:h:s entry in the partition table for consistency.
 * Try correcting any inconsistency found, with warning to the user.
 * The beginFlag is 1 for beginning, 0 for end.
 ******************************************************************/

#if USE_PROTO
void check_chs(FDISK_S *p, int beginFlag)
#else
void
check_chs(p, beginFlag)
FDISK_S *p;
int	beginFlag;
#endif
{
	unsigned int	c, h, s, nc, nh, ns;
	unsigned long	lbkno;

again:
	if (beginFlag) {
		c = sec_to_c(p->p_base);
		h = bhd(p);
		s = bsec(p);
		lbkno = p->p_base;
	} else {
		c = sec_to_c(p->p_base + p->p_size - 1);
		h = ehd(p);
		s = esec(p);
		lbkno = p->p_base + p->p_size - 1;
	}

	if (!warned
	  && (c >= ncyls
	     || h >= nheads
	     || (s == 0 || s > nspt)
	     || lbkno != chs_to_sec(c, h, s))) {

		warned = 1;

		nc = sec_to_c(lbkno);
		nh = sec_to_h(lbkno);
		ns = sec_to_s(lbkno);

		cls(1);

		printf(
"According to your computer system, the disk contains\n"
"%6u cylinders (0 to %u)\n"
"%6u heads (0 to %u), and\n"
"%6u sectors per track (1 to %u).\n",
		    ncyls, ncyls - 1, nheads, nheads - 1, nspt, nspt);

		printf(
"According to the partition table, a partition %s at sector %lu,\n"
"which corresponds to\n"
"\tcylinder %6u\thead %6u\tsector %6u\n",
		    (beginFlag) ? "begins" : "ends" , lbkno, nc, nh, ns);
		printf(
"But the partition table entry gives\n"
"\tcylinder %6u\thead %6u\tsector %6u\n",
		    c, h, s);
		cls (1);

		if (beginFlag) {
			printf("\n"
"An inconsistency in a partition table entry usually occurs because\n"
"the system CMOS RAM area specifies the hard disk geometry incorrectly;\n"
"that is, your disk does not contain %u cylinders, %u heads, and %u sectors.\n",
			    ncyls, nheads, nspt);
			if (!cflag) {
				printf(
"If you think the disk geometry values above are wrong, invoke this program\n"
"again using the \"-c\" option to correct them.  Because changing these\n"
"values is dangerous and you have not specified the \"-c\" option, this\n"
"program will now terminate.\n"
				    );
				exit(1);
			}
			printf(
"This program lets you to resolve the inconsistency by specifying correct\n"
"values for the disk geometry (number of cylinders, heads and sectors) or by\n"
"making the partition table entry consistent with the given values.\n"
			    );

			if (yes_no(
"Do you think the above disk geometry values are wrong")) {
				fix_chs();
				goto again;
			}
		}

		printf(
"This program will now change the c:h:s of the entry to %u:%u:%u\n",
		    nc, nh, ns);
		printf(
"to resolve this inconsistency.  Changing a partition table entry can\n"
"make data on existing filesystems inaccessible.  If you feel this change is\n"
"incorrect, exit from this program now without updating the partition table.\n"
		    );

		if (yes_no("Do you want to exit from this program"))
			exit(1);

		++nmods;
		if (beginFlag) {
			p->p_bcyl = nc & 0xFF;
			p->p_bhd = nh;
			p->p_bsec = ((nc >> 2) & CYLMASK ) | ns;
		} else {
			p->p_ecyl = nc & 0xFF;
			p->p_ehd = nh;
			p->p_esec = ((nc >> 2) & CYLMASK ) | ns;
		}
	}
}


/******************************************************************
 *
 * pcompare
 *
 * Compare two partition table entries.
 * Called by qsort.
 * The result is sorted by base, with empty entries at the end.
 *****************************************************************/

#if USE_PROTO
int	pcompare (FDISK_S **pp1, FDISK_S **pp2)
#else
int
pcompare (pp1, pp2)
FDISK_S **pp1;
FDISK_S **pp2;
#endif
{
	FDISK_S * p1, *p2;

	p1 = *pp1;
	p2 = *pp2;

	if (p1->p_size == 0)
		return (p2->p_size == 0) ? 0 : 1;
	else if (p2->p_size == 0)
		return - 1;
	else if (p1->p_base < p2->p_base)
		return - 1;
	else if (p1->p_base == p2->p_base)
		return 0;
	else
		return 1;
}


/******************************************************************
 *
 * unused
 *
 * Report unused portion of disk.
 *****************************************************************/

#if USE_PROTO
void unused (unsigned long base, unsigned long next)
#else
void
unused (base, next)
unsigned long	base;
unsigned long	next;
#endif
{
	unsigned long	n, x, y;
	char	*s;

	n = base - next;
	if (cylflag && n >= cylsize) {
		s = "cylinder";
		x = sec_to_c(n);
		y = sec_upto_c(next);
	} else if (n >= nspt) {
		s = "track";
		x = n / nspt;
		y = sec_upto_t(next);
	} else {
		s = "sector";
		x = n;
		y = next;
	}
	if (x == 1)
		printf("%lu %s (%.2f megabytes) is unused starting at %s %lu.\n",
		    x, s, meg(n), s, y);
	else
		printf("%lu %ss (%.2f megabytes) are unused starting at %s %lu.\n",
		    x, s, meg(n), s, y);
	if (freesize < n) {
		freesize = n;
		freestart = next;
	}
}


/******************************************************************
 *
 * sanity
 *
 * Check a partition table for sanity.
 * Sort the partitions, look for gaps and overlaps.
 *
 * Return 1 if AOK, 0 if problem found.
 *****************************************************************/

#if USE_PROTO
int sanity(void)
#else
int
sanity()
#endif
{
	int		i;
	int		retval;

	FDISK_S		* p[NPARTN];
	unsigned long	base, next, size, safe;
	typedef int	(*COMP_FN_T)();

	badflag = 0;
	freepart = -1;
	freesize = freestart = 0;

	for (i = 0; i < NPARTN; i++) {
		p[i] = &hd.hd_partn[i];

		if (p[i]->p_size != 0) {
			check_chs(p[i], 1);	/* check start c:h:s */
			check_chs(p[i], 0);	/* check end c:h:s */
		} else if (freepart == -1)
			freepart = i;		/* first free partition */
	}

	qsort(p, NPARTN, sizeof(FDISK_S * ), (COMP_FN_T) pcompare);

	next = 1;		/* next block available after boot sector */
	for (i = 0; i < NPARTN; i++) {
		base = p[i]->p_base;
		size = p[i]->p_size;

		if (size == 0)
			break;			/* done when empty reached */

		if (base < next) {
			if (next == 1)
				printf("Partition overlaps boot sector.\n");
			else if (cylflag)
				printf("Partitions overlap starting at cylinder %lu.\n", base / cylsize);
			else
				printf("Partitions overlap starting at track %lu.\n", base / nspt);
			++badflag;
			retval = 0;
		} else if (base != next) {
			if (i == 0 && (base == nspt || base == cylsize))
				/* DO NOTHING */;	/* first partition at 0:1:1 or 1:0:1 */
			else
				unused(base, next);
		}

		if (base + size > next)
			next = base + size;
	}

	safe = nsectors - nspt * nheads;	/* safely usable sectors */

	if (next < safe)
		unused(safe, next);
	else if (next > safe) {
		printf(
"\n"
"Warning: the last cylinder of a hard disk is usually reserved for use by\n"
"disk diagnostic programs.  The current disk partitioning uses part of the\n"
"the last cylinder in a disk partition.  Mark Williams strongly recommends\n"
"that you change the partitioning to avoid using the last cylinder.\n"
		    );
		retval = 0;
	}
	return retval;
}


/******************************************************************
 *
 * print_ptable
 *
 * Output partition information.
 *****************************************************************/

#if USE_PROTO
void print_ptable ()
#else
void
print_ptable ()
#endif
{
	FDISK_S * p;
	char	c, *s, *dname;
	int	i;
	unsigned long	end;

	printf("%s currently has the following logical partitions:\n", drivename);
	printf(
	    "                 [ In Cylinders ]  [    In Tracks    ]\n"
	    "Number     Type  Start  End  Size  Start    End   Size Mbytes  Blocks Name\n");

	for (i = 0; i < NPARTN; ++i) {
		p = &hd.hd_partn[i];

		if (p->p_size == 0L)
			end = p->p_base = 0L;
		else
			end = p->p_base + p->p_size - 1;

		printf("%d", partbase + i);
		printf("%s\t", (p->p_boot == 0x80) ? " Boot" : "");
		printf("%8s ", sys_type(p->p_sys));
		printf("%5u ", sec_to_c(p->p_base));
		printf("%4u ", sec_to_c(end));
		printf("%5u ", sec_upto_c(p->p_size));
		printf("%6lu ", p->p_base / nspt);
		printf("%6lu ", end / nspt);
		printf("%6u ", sec_upto_t(p->p_size));
		printf("%6.2f ", meg(p->p_size));
		printf("%7lu ", p->p_size);

		if ((dname = malloc(strlen(device) + 1)) == NULL)
			fatal("out of memory");

		strcpy(dname, device);

		if (strncmp(dname, "/tmp", 4) == 0)
			dname += 4;

		s = &dname[strlen(dname) - 1];
		c = *s;
		*s = 'a' + i;
		printf("%s", dname);
		*s = c;

		if (vflag) {
			printf("\n\t%3u:%u:%u ", bcyl(p), bhd(p), bsec(p));
			printf("%3u:%u:%u ", ecyl(p), ehd(p), esec(p));
		}

		printf("\n");
	}
}

/******************************************************************
 *
 * print_part
 *
 * Output partition information and do sanity check.
 *****************************************************************/

#if USE_PROTO
void print_part (void)
#else
void
print_part ()
#endif
{
	print_ptable ();
	if (!sanity ())
		print_ptable ();
}


/******************************************************************
 *
 * get_long
 *
 * Prompt for long input from the user.
 * Accept data in range min to max.
 * Return the result.
 *****************************************************************/

#if USE_PROTO
long	get_long(char *prompt, long defval, long min, long max)
#else
long
get_long(prompt, defval, min, max)
char	*prompt;
long	defval;
long	min;
long	max;
#endif
{
	long	val;
	char	*s;

	for (; ; ) {
		s = get_line("%s [%lu]?", prompt, defval);

		if (*s == '\0')
			return defval;		/* take default */

		val = atol(s);

		if (val >= min && val <= max)
			return val;

		printf("Please enter a value between %lu and %lu.\n", min, max);
	}
}


/******************************************************************
 *
 * change_part
 *
 * Interactively change the table entry for logical partition n.
 *****************************************************************/

#if USE_PROTO
void change_part(int n)
#else
void
change_part(n)
int	n;
#endif
{
	FDISK_S * p;
	int	sys, old;
	unsigned int	c, h, s;
	unsigned long	size, osize;
	unsigned long	base, obase;	/* base: starting lb of part */
	unsigned long	end;
	static int	optflag;

	/* Get options first time through. */
	if (optflag == 0) {
		cls(0);

		printf(
		    "Existing data on a partition will be lost if you change the\n"
		    "base or the size of the partition.  Be sure you have backed up\n"
		    "all data from any partition which you are going to change.\n"
		    "\n"
		    "You may specify partition bases in cylinders or in tracks.\n"
		    );

		cylflag = yes_no("Do you want to specify bases in cylinders");

		printf("You may specify partition sizes in %s or in megabytes.\n",
		    cylflag ? "cylinders" : "tracks");
		megflag = !yes_no("Do you want to specify partition sizes in %s",
		    cylflag ? "cylinders" : "tracks");
		++optflag;
		print_part();
	}

	p = &hd.hd_partn[n];
	printf("\nPartition %d:\n", n + partbase);
	size = p->p_size;

	/* Get new system type. */
	old = p->p_sys;

again:
	if (old != SYS_EMPTY)
		printf("The current operating system type is %s.\n", sys_type(old));

	if (yes_no("Do you want this to be a COHERENT partition"))
		sys = SYS_COH;
	else if (yes_no("Do you want the partition marked as unused"))
		sys = SYS_EMPTY;
	else {
		printf(
		    "\nThis program can mark a partition as a COHERENT partition\n"
		    "or mark it as unused.  It CANNOT initialize a partition for\n"
		    "use by any other operating system.  To do so, you must mark\n"
		    "it as unused now and subsequently use the disk partitioning\n"
		    "program provided by the other system to initialize it correctly.\n\n"
		    );

		if (yes_no("Do you still want to modify this partition"))
			goto again;
		return;
	}

	if (sys != old) {
		p->p_sys = sys;
		++nmods;
	}

	if (sys == SYS_EMPTY) {
		printf(
		    "For you convenience in partitioning your hard disk, this program\n"
		    "lets you create unused partitions with nonzero base and size.\n"
		    "However, other disk partitioning software may not work correctly\n"
		    "unless unused partitions have base and size zero.\n"
		    );

		if (yes_no("Do you want to zero the partition base and size")) {
			memset(p, 0, sizeof(FDISK_S));
			nmods++;
			printf("\n");
			return;
		}
	}

getbase:
	/* Specify the base. */
	/* Default: old or first free or track 1. */
	obase = p->p_base;
	base = (size != 0L) ? obase : (freesize != 0) ? freestart : nspt;

	if (cylflag) {				/* in cylinders */
		base = sec_to_c(base);
		base = get_long("Base cylinder", base, 0L, (long) ncyls - 1);

		if (base == 0)
			base = nspt;		/* skip first track for cyl 0 */
		else
			base *= nspt * nheads;	/* cylinders to sectors */
	} else {				/* in tracks */
		base = sec_upto_t(base);
		base = get_long("Base track", base, 1L, (long)ncyls * nheads - 1);
		base *= nspt;			/* tracks to sectors */
	}

	/* Check that base falls at a track boundary. */
	/* It might not if the disk was previously partitioned. */
	c = sec_to_c(base);
	h = sec_to_h(base);
	s = sec_to_s(base);

	if (s != 1) {
		printf("Partitions should begin at a track boundary.\n");
		printf("The partition does not begin at a track boundary with the selected base.\n");
		printf("The next track boundary is at track %u\n", sec_upto_t(base));

		if (yes_no("Do you want to change the partition base"))
			goto getbase;
	}

	/* Update the partition table base and start information. */
	if (base != obase) {
		++nmods;

		p->p_base = base;
		p->p_bcyl = c % 1024;
		p->p_bhd = h;
		p->p_bsec = (((c % 1024) >> 2) & CYLMASK ) | s;
	}

	/* Specify the partition size. */
	/* Default size: free block size, old size, largest possible. */
	osize = size;
	size = (base == freestart) ? freesize : (osize != 0L) ? osize : nsectors - base;
	if (megflag) {				/* in megabytes */
		size = meg(size);

		if ((long)meg(nsectors - base) == 0) {
			printf("Less than a megabyte of space remains.\n");
			size = nsectors - base;
		} else {
			size = get_long("Partition size in megabytes", size, 0L,
			    (long) meg(nsectors - base));
			size *= 1000000L;	/* megabytes to bytes */
			size /= SSIZE;		/* to sectors */
			size = sec_upto_c(size); /* round up to cylinders */
			size *= nspt * nheads;	/* cylinders to sectors */
		}
	} else if (cylflag) {			/* in cylinders */
		/* Tricky stuff again. */
		end = base + size - 1;
		size = sec_to_c(end) - sec_to_c(base) + 1;
		size = get_long("Partition size in cylinders", size, 0L,
		    (long) ncyls - sec_to_c(base));
		size *= nspt * nheads;		/* cylinders to sectors */
	} else {				/* in tracks */
		size = sec_upto_t(size);
		size = get_long("Partition size in tracks", size, 0L,
		    (long) sec_upto_t(nsectors - base));
		size *= nspt;			/* tracks to sectors */
	}

	/*
   * Adjust size to end at cylinder boundary
   * if it did not start at cylinder boundary.
   */
	if ((megflag || cylflag) && size != 0 && base % cylsize != 0
	     && size > base % cylsize)
		size -= base % cylsize;

	/* Check the size. */
	if (base + size > nsectors)
		size = nsectors - base;		/* roundup too big */

	end = base + size - 1;
	c = sec_to_c(end);
	h = sec_to_h(end);
	s = sec_to_s(end);

	if (s != nspt) {
		printf("Partitions should end at a track boundary.\n");
		printf("A partition with %u more sectors would end at a track boundary.\n",
		    nspt - s);

		if (yes_no("Do you want to add %u sectors to the partition size",
		    nspt - s)) {
			size += nspt - s;
			s = nspt;
		}
	}

	/* Update the partition table size and end. */
	if (base != obase || size != osize) {

		/* Force ending cylinder to maximum if mod confuses things. */
		if ((c % 1024) < bcyl(p))
			c = 1023;

		++nmods;
		p->p_size = size;
		p->p_ecyl = c % 1024;

		p->p_ehd = h;
		p->p_esec = (((c % 1024) >> 2) & CYLMASK) | s;
	}
	printf("\n");
}


/******************************************************************
 *
 * drive_info
 *
 * Print drive information.
 *****************************************************************/

#if USE_PROTO
void drive_info(void)
#else
void
drive_info()
#endif
{
	printf("%s has %u cylinders, %u heads, and %u sectors per track.\n",
	    drivename, ncyls, nheads, nspt);
	printf("It contains:\n");
	printf("\t%u cylinders of %lu bytes each,\n",
	    ncyls, (long)cylsize * SSIZE);
	printf("\t%u tracks of %lu bytes each,\n",
	    ncyls * nheads, (long)nspt * SSIZE);
	printf("\t%lu sectors of %d bytes each,\n",
	    nsectors, SSIZE);
	printf("or a total of %ld bytes (%.2f megabytes).\n",
	    nsectors * SSIZE, meg(nsectors));
}


/******************************************************************
 *
 * get_boot
 *
 * Read boot block from a file into the given structure.
 * Return a file descriptor to the open file.
 *****************************************************************/

#if USE_PROTO
int	get_boot(char *name, int mode, HDISK_S *hdp)
#else
int
get_boot(name, mode, hdp)
char	*name;
int	mode;
HDISK_S *hdp;
#endif
{
	int	fd;

	/* Open the file. */
	if ((fd = open(name, mode)) < 0)
		fatal("cannot open \"%s\"", name);

	/* Read the current boot block into the hd structure. */
	if (read(fd, (char *) hdp, sizeof hd) != sizeof hd) {
		close(fd);
		fatal("read error on \"%s\"", name);
	}

	return fd;
}


/******************************************************************
 *
 * quit
 *
 * Done.
 * If changes, prompt for confirmation and save.
 * Return 1 to quit, 0 to not quit.
 *****************************************************************/

#if USE_PROTO
int	quit (char *fname)
#else
int
quit (fname)
char	*fname;
#endif
{
	if (badflag) {
		printf("Because the partition table defines overlapping disk\n");
		printf("partitions, it will not be saved to the disk if you quit.\n");

		if (!yes_no("Do you wish to quit without saving the changes"))
			return 0;
	} else if (nmods != 0) {
		if (yes_no("\nAre you sure you want to write the updated "
		    "partition table")) {
			if (lseek(cfd, 0L, 0) != 0L)
				fatal("seek failed on \"%s\"", fname);
			else if (write(cfd, &hd, sizeof hd) != sizeof hd)
				fatal("write error on \"%s\"", fname);

			/*
			 * This HDGETA is for the benefit of the SCSI driver,
			 * which needs to reset the parameters if they changed.
			 */
			if (ioctl(cfd, HDGETA, (char *) & hdparms) == -1)
				fprintf(stderr, "HDGETA failed on \"%s\"\n",
				    fname);

			sync();
		} else if (!yes_no("Changes will not be saved.  Quit anyway"))
			longjmp(loop, 2);
		else
			printf("Changes not saved.\n");
	} else
		printf("The partition table is unchanged.\n");

	close(cfd);
	return 1;
}


#if SAVE_BOOT
/******************************************************************
 *
 * saveboot
 *
 * Save/restore a copy of boot block to/from floppy.
 * Some fuss required to find the name of the root device.
 *****************************************************************/

#if USE_PROTO
void saveboot(void)
#else
void
saveboot()
#endif
{
	int	fd;
	dev_t dev;
	char	*floppy;
	SYMENT	se;
	long	rootdev_addr;

	/* put together SYMENT stuff for coffnlist */
	se._n._n_n._n_zeroes = 0;
	se._n._n_n._n_offset = sizeof(long);
	se.n_type = -1;
	coffnlist(COH, &se, "rootdev", 1);
	rootdev_addr = se.n_value;

	/* Forget it if lookup failed. */
	if (rootdev_addr == 0) {
		printf ("fdisk : saveboot : rootdev lookup failed\n");
		return;
	}

	/* Forget it if rootdev_addr is not where it should be. */
	if ((rootdev_addr & 0x80000000) == 0) {
		printf ("fdisk : saveboot : rootdev address unlikely value %x\n",
		  rootdev_addr);
		return;
	}

	/* Open kernel memory and read value of rootdev. */
	if ((fd = open(KMEM, 0)) < 0) {
		printf ("fdisk : saveboot : open %s failed\n", KMEM);
		return;
	}

	rootdev_addr -= 0x80000000;

	if (lseek(fd, rootdev_addr, 0) == -1L) {
		printf ("fdisk : saveboot : seek rootdev at %x failed\n", rootdev_addr);
		return;
	}

	if (read(fd, &dev, sizeof(dev_t)) != sizeof(dev_t)) {
		printf ("fdisk : saveboot : read rootdev at %x failed\n", rootdev_addr);
		return;
	}
	close(fd);

	if (dev == makedev(FL_MAJOR, 14))
		floppy = "/dev/rfha0";
	else if (dev == makedev(FL_MAJOR, 15))
		floppy = "/dev/rfva0";
	else {
		printf ("fdisk : saveboot : rootdev not diskette, = %04x\n", dev);
		return;
	}

	/* Cheesiness.  Get inode in buffer cache. */
	if ((fd = open(floppy, O_RDONLY)) == -1) {
		printf ("fdisk : saveboot : open %s failed\n", floppy);
		return;			/* open failed, bag out */
	}
	close (fd);

	cls(0);
	sync();
	printf(
"If you are installing COHERENT on your hard disk for the first time and you\n"
"want to use your drive with other operating systems, we recommend that you\n"
"save a copy of the current boot block (which includes the partition table)\n"
"to a diskette.  You can restore the original boot block from the diskette\n"
"if your COHERENT installation fails or if you are subsequently unable to run\n"
"another operating system on the drive.\n"
"\n"
"You will be asked about saving and restoring the boot block once for each\n"
"hard drive you are using.  Use a separate diskette for each hard drive.\n"
	    );
	if (yes_no("Do you want to save the original boot block")) {
		printf(
		    "\n"
		    "Remove the COHERENT boot diskette, insert a formatted blank diskette,\n"
		    );
		get_line("then hit <Enter>.");

		if ((fd = open(floppy, O_WRONLY)) == -1) {
			printf ("fdisk : saveboot : open %s failed\n", floppy);

			/* open failed, bag out */
			goto sb_bagout;
		}

		if (write(fd, &hd, sizeof hd) != sizeof hd)
			fprintf(stderr, "fdisk: write error on \"%s\"\n", floppy);
		else
			printf(
"\n"
"Remove the diskette containing the original boot block.\n"
"Label it and file it with your COHERENT installation disks.\n"
			    );
	} else if (yes_no("Do you want to restore a previously saved boot block")) {
		printf(
"\n"
"WARNING: This step will overwrite your hard disk partition table\n"
"with the previously saved copy from the diskette in drive A:.\n"
"Type <Ctrl-C> if you do not want to overwrite the existing partition table.\n"
"\n"
"Remove the COHERENT boot diskette,\n"
"insert the diskette containing the saved boot block,\n"
		    );
		get_line("then hit <Enter>.");

		if ((fd = open(floppy, O_RDONLY)) == -1) {
			printf ("fdisk : saveboot : open %s failed\n", floppy);

			/* open failed, bag out */
			goto sb_bagout;
		}

		if (read(fd, &hd, sizeof hd) != sizeof hd)
			fprintf(stderr, "fdisk: read error on \"%s\"\n", floppy);
		else if (lseek(cfd, 0L, 0) != 0L)
			fatal("seek failed on \"%s\"", device);
		else if (write(cfd, &hd, sizeof hd) != sizeof hd)
			fatal("write error on \"%s\"", device);
	} else {

		close(fd);
		return;
	}
	close(fd);
	sync();
sb_bagout:
	get_line("\nReplace the COHERENT boot diskette, then hit <Enter>.");
}
#endif


/******************************************************************
 *
 * fdisk
 *
 * Print/change configuration for given device.
 * The 'lastflag' is true if the current device is the last one.
 *****************************************************************/

#if USE_PROTO
void fdisk(int lastflag)
#else
void
fdisk(lastflag)
int	lastflag;
#endif
{
	int	nfd, p, flag;
	unsigned	action;
	static int	firstflag = 1;
	int	partchoice;

	nmods = 0;
	if ((cfd = open(device, openmode)) < 0)
		fatal("cannot open \"%s\"", device);

	/* Obtain drive characteristics. */
	if (ioctl(cfd, HDGETA, (char *) & hdparms) == -1)
		fatal("cannot get \"%s\" drive characteristics", device);

	ncyls = (hdparms.ncyl[1] << 8) | hdparms.ncyl[0];

	if (ncyls > 1024) {
bad_deal:
		printf(
"\nWARNING: The BIOS says your disk has %d cylinders.\n"
"Many older BIOSes will have trouble booting programs that reside beyond\n"
"cylinder 1023.  You have 3 choices:\n"
"  1) Force COHERENT to use only the first 1024 cylinders.\n"
"  2) Continue and use all the cylinders, making sure that all bootable\n"
"     partitions are within the first 1024.  This may cause trouble if you\n"
"     later run versions of fdisk from DOS or other operating systems.\n"
"  3) Quit now and change the CMOS settings for your hard drive by running\n"
"     your BIOS hard disk setup. Some new BIOSes support translation mode\n"
"     parameters that give you access to the full drive but keep the number\n"
"     of cylinders less than 1024.  Choosing this option may cause you to\n"
"     lose all data if the disk is already partitioned.\n\n",
		    ncyls);

		partchoice = get_int("1) Limit to 1024  2) Continue  3) Quit", 1, 1, 3);

		switch (partchoice) {
		case 1:
			ncyls = 1024;
			break;

		case 2:
			break;

		case 3:
			exit(1);
			break;

		default:
			goto bad_deal;		/* Should never get here */
			break;
		}
	}

	nheads = hdparms.nhead;
	nspt = hdparms.nspt;
	cylsize = nheads * nspt;
	nsectors = (long)ncyls * cylsize;

	/* Print drive characteristics and allow user to patch. */
	if (cflag && isatflag) {
		cls(1);
		printf("According to your computer system:\n");
		drive_info();
		if (!yes_no("Do you think the above values are correct"))
			fix_chs();
	}

	close(cfd);

	/* Read the current boot block. */
	cfd = get_boot(device, openmode, &hd);		/* read boot */

#if SAVE_BOOT
	if (sflag)
		saveboot();
#endif

	/* Check for Ontrack Disk Manager. */
	if (*(unsigned short *)(&hd.hd_boot[0xFC]) == HDSIG) {
		printf( "\n"
"Your hard disk appears to include Disk Manager software.  Disk Manager can\n"
"partition your disk into more than four partitions, but COHERENT only\n"
"understands the first four partitions.  If you have more than four\n"
"partitions on your disk, you will not see information about the additional\n"
"partitions, so proceed with extreme caution.\n"
"To install COHERENT while leaving Disk Manager intact, you must\n"
"remove all data from one of the first four disk partitions.\n"
		    );
		if (firstflag && mboot != NULL)
			printf(
"\n"
"If you use the COHERENT master bootstrap and you have more than four\n"
"Disk Manager partitions, ALL data in any Disk Manager partition\n"
"other than the first four partitions WILL BE LOST!\n"
			    );
		if (!yes_no("Do you want to continue partitioning your disk"))
			exit(1);
	}

	/* Read master boot if desired. */
	if (firstflag && mboot != NULL) {
		nfd = get_boot(mboot, 0, &newhd);	/* read new boot */
		close(nfd);

		if (newhd.hd_sig != HDSIG)
			fatal("invalid signature in \"%s\"", mboot);

		memcpy(hd.hd_boot, newhd.hd_boot, sizeof hd.hd_boot);
		nmods++;
	}

	firstflag = 0;		/* replace mboot only on first device */

	/* If no signature, zap the partition entries. */
	if (hd.hd_sig != HDSIG) {
		printf(
"The boot block on this disk drive does not contain a valid partition table.\n"
"This program will now create a valid partition table with zeroed entries.\n"
"Exit from this program immediately if you do not want to zero the entries.\n"
		    );

		if (yes_no("Do you want to exit instead of zeroing the partition table"))
			exit(1);

		memset(hd.hd_partn, 0, NPARTN * sizeof(FDISK_S));
		hd.hd_sig = HDSIG;
		nmods++;
	}

	/* If readonly, print information and return. */
	if (openmode == 0) {
		print_part();
		close(cfd);
		return;
	}

	/* Interactive input loop. */
	for (flag = 1; ; ) {
		setjmp(loop);
		print_part();
		flag = 0;
		printf(
"Possible actions:\n"
"\t0 = Quit\n"
"\t1 = Change active partition (or make no partition active)\n"
"\t2 = Change one logical partition\n"
"\t3 = Change all logical partitions\n"
"\t4 = Delete one logical partition\n"
"\t5 = Change drive characteristics\n"
"\t6 = Display drive information\n"
		    );

		if (lastflag)
			action = get_int("Action", 0, 0, 6);
		else {
			printf("\t7 = Proceed with next drive\n");
			action = get_int("Action", 7, 0, 7);
		}

		switch (action) {
		case 0:
			if (quit (device) == 1) {
				qflag = 1;
				return;
			}
			continue;
			break;

		case 1:
			printf("Change active partition:\n");
			change_active();
			continue;
			break;

		case 2:
			p = (freepart != -1) ? freepart : 0;
			p = get_int("Which partition", p + partbase, partbase, partbase + NPARTN - 1);
			p -= partbase;

			if (action == 2)
				change_part(p);

			continue;
			break;

		case 3:
			for (p = 0; p < NPARTN; ) {
				change_part(p++);

				if (p < NPARTN)
					print_part();
			}
			continue;
			break;

		case 4:
			p = get_int("Which partition", partbase, partbase,
			    partbase + NPARTN - 1);
			p -= partbase;
			memset(&hd.hd_partn[p], 0, sizeof(FDISK_S));
			nmods++;
			continue;
			break;

		case 5:
			cls(0);
			printf("According to your computer system:\n");
			drive_info();

			if (!yes_no("Do you think the above values are correct"))
				fix_chs();

			continue;
			break;

		case 6:
			cls(0);
			drive_info();
			flag = 1;
			continue;
			break;

		case 7:
			if (quit (device) == 1)
				return;
			continue;
			break;

		default:
			continue;
			break;
		}
	}
}



/*
 * Print a usage message and die.
 */
void
usage()
{
	fprintf(stderr, USAGE);
	exit(1);
}


/******************************************************************
 *
 * main
 *
 *****************************************************************/

#if USE_PROTO
int	main(int argc, char **argv)
#else
int
main(argc, argv)
int	argc;
char	*argv[];
#endif
{
	char	*s;
	int	i;
	struct stat sb;

	/* Sanity check. */
	argv0 = argv[0];
	if (sizeof hd != SSIZE)
		fatal("invalid HDISK_S size %u != %u", sizeof hd, SSIZE);
	while (argc > 1 && **++argv == '-') {
		--argc;
		for (s = &argv[0][1]; *s; ++s) {
			switch (*s) {
			case 'B':
				++Bflag;
				break;

			case 'b':
				if (argc-- < 2)
					usage();
				mboot = *++argv;
				break;

			case 'c':
				++cflag;
				break;

			case 'r':
				++rflag;
				openmode = 0;
				break;

			case 's':
				++sflag;
				break;

			case 'v':
				++vflag;
				break;

			case 'V':
				fprintf(stderr, "%s: V%s\n", argv0, VERSION);
				break;

			default:
				usage();
				break;
			}
		}
	}

	if (openmode == 0 && mboot != NULL)
		fatal("cannot specify both 'b' and 'r' options");

	if (--argc == 0) {

		/* no arguments specified, warn user that an x device must be
     * specified.
     * bob h (8/11/93)
     */

		usage();
	}

	cls(0);

	printf(
"This program will let you change partition information for each disk drive.\n"
"A disk drive can be divided into one to four logical partitions.\n"
"You can change the active partition (the partition which your\n"
"system boots by default) or change the layout of logical partitions.\n"
"Other programs which change hard disk partition information\n"
"may list logical partitions in a different order.\n"
"Hit <Esc><Enter> to return to the main menu at any time.\n"
	    );

	get_line("Now hit <Enter>.");

	for (i = 0; (device = *argv++) != NULL; ++i) {
#if 0
		/*
     * Set the drive number, drive name, partition base.
     * /etc/build calls /etc/fdisk with an ordered list of args
     * which correspond to the drive number, so this usually works.
     * But there is no obvious way to find the correct drive number
     * when the user invokes /etc/fdisk directly; hence the
     * kluge below for AT disks.
     */
		drivenum = i;
		partbase = i * NPARTN;
		drivename = drive_x;
		drivename[6] = drivenum + '0';
#endif

	/*
     * Check if device is an AT device.
     * fix_chs() works differently for ide vs. scsi disks.
     */
		if (stat(device, &sb) < 0)
			fatal("cannot stat \"%s\"", device);

		switch (major(sb.st_rdev)) {
		case AT_MAJOR:
			isatflag = 1;
			break;
		case SCSI_MAJOR:
			isatflag = 0;
			break;
		default:
			fatal("fdisk: \"%s\" has major # %d, not hard disk",
			  device, major(sb.st_rdev));
		}

		if (isatflag) {
			/* kluge, see comment above... */
			if (minor(sb.st_rdev) == AT0X_MINOR) {
				drivenum = 0;
			} else if (minor(sb.st_rdev) == AT1X_MINOR) {
				drivenum = 1;
			} else {
				fatal(
"fdisk: \"%s\" minor # %d, not partition table (try /dev/at?x)\n",
			  device, minor(sb.st_rdev));
			}
			sprintf (drivename, "IDE Drive %d", drivenum);
		} else {
			/* for SCSI:
			 * drivenum is SCSI ID:
			 * name is SCSI Drive n */
			drivenum = (((minor(sb.st_rdev)) >> 4) & 0x7);
			sprintf (drivename, "SCSI Drive %d", drivenum);
			if ((minor(sb.st_rdev) & 0x83) != 0x80)
				fatal(
"fdisk: \"%s\" minor # %d, not partition table (try /dev/sd?x)\n",
			  device, minor(sb.st_rdev));
		}
		partbase = 4 * drivenum;

		/* Do it. */
		fdisk(*argv == NULL);

		if (qflag)
			break;
	}

	if (cohtuneFlag) {
		printf(
"\nDrive parameters have been altered for one or more hard disks.  This change"
"\nwill be in effect until the operating system is rebooted.  To make the"
"\nchange permanent, run"
"\n\t/etc/conf/bin/idmkcoh -o /coh.new"
"\nand boot coh.new.  If the new kernel is satisfactory, you can make it the"
"\ndefault with:"
"\n\tln -f /coh.new /autoboot\n");
	}
	exit(0);
}

/* end of fdisk.c */
