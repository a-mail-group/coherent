#define PV_HUNT 0
/*
 * $Header: /v/src/rcskrnl/conf/at/src/RCS/at.c,v 420.6 1993/12/01 22:10:37 srcadm Exp srcadm $
 */

/*********************************************************************
 *
 * Coherent, Mark Williams Company
 * RCS Header
 * This file contains proprietary information and is considered
 * a trade secret.  Unauthorized use is prohibited.
 *
 *
 * $Id: at.c,v 420.6 1993/12/01 22:10:37 srcadm Exp srcadm $
 *
 * $Log: at.c,v $
 * Revision 420.6  1993/12/01  22:10:37  srcadm
 * Fixed 'break' bug on read failure in state machine and
 * added printf for version and revision tracking.
 *
 * Revision 420.4  1993/11/30  19:31:21  srcadm
 * Initial RCS submission.
 *
 *
 */

/* Embedded version constant */

char *MWC_AT_VERSION = "MWC_AT_VERSION($Revision: 420.6 $)";

static char rcsid[] = 	"#(@) $Id"
			"at.c: Last Modified: Sun Apr 24 22:46:56 1994 by [chris]";

/*
 * This is a driver for the
 * hard disk on the AT.
 *
 * Reads drive characteristics from ROM (thru interrupt vector 0x41 and 0x46).
 * Reads partition information from disk.
 *
 * Revision 2.5  93/08/20  10:49:54  nigel
 * Fix race work queue interlock problem
 * 
 * Revision 2.4  93/08/19  10:38:34  nigel
 * r83 ioctl (), corefile, new headers
 * 
 * Revision 2.3  93/08/19  04:02:13  nigel
 * Nigel's R83
 */

#include <sys/cmn_err.h>
#include <sys/inline.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <stdlib.h>

#include <kernel/typed.h>
#include <sys/coherent.h>
#include <sys/uproc.h>
#include <sys/fdisk.h>
#include <sys/hdioctl.h>
#include <sys/buf.h>
#include <sys/con.h>
#include <sys/devices.h>

#define	LOCAL

/*
 * Configurable parameters
 */

#define	HDBASE	0x01F0			/* Port base */
#define	SOFTLIM	6			/*  (7) num of retries before diag */
#define	HARDLIM	8			/* number of retries before fail */
#define	BADLIM	100			/* num to stop recov if flagged bad */

#define	BIT(n)		(1 << (n))

#define	CMOSA	0x70			/* write cmos address to this port */
#define	CMOSD	0x71			/* read cmos data through this port */

#if	_I386
# define	VERBOSE		1
# define	BAD_STUFF	0
#endif

/*
 * I/O Port Addresses
 */

#define	DATA_REG	(HDBASE + 0)	/* data (r/w) */
#define	AUX_REG		(HDBASE + 1)	/* error(r), write precomp cyl/4 (w) */
#define	NSEC_REG	(HDBASE + 2)	/* sector count (r/w) */
#define	SEC_REG		(HDBASE + 3)	/* sector number (r/w) */
#define	LCYL_REG	(HDBASE + 4)	/* low cylinder (r/w) */
#define	HCYL_REG	(HDBASE + 5)	/* high cylinder (r/w) */
#define	HDRV_REG	(HDBASE + 6)	/* drive/head (r/w) (D <<4)+(1 << H) */
#define	CSR_REG		(HDBASE + 7)	/* status (r), command (w) */
#define	HF_REG		(HDBASE + 0x206)	/* Usually 0x3F6 */


/*
 * Error from AUX_REG (r)
 */

#define	DAM_ERR		BIT(0)		/* data address mark not found */
#define	TR0_ERR		BIT(1)		/* track 000 not found */
#define	ABT_ERR		BIT(2)		/* aborted command */
#define	ID_ERR		BIT(4)		/* id not found */
#define	ECC_ERR		BIT(6)		/* data ecc error */
#define	BAD_ERR		BIT(7)		/* bad block detect */


/*
 * Status from CSR_REG (r)
 */

#define	ERR_ST		BIT(0)		/* error occurred */
#define	INDEX_ST	BIT(1)		/* index pulse */
#define	SOFT_ST		BIT(2)		/* soft (corrected) ECC error */
#define	DRQ_ST		BIT(3)		/* data request */
#define	SKC_ST		BIT(4)		/* seek complete */
#define	WFLT_ST		BIT(5)		/* improper drive operation */
#define	RDY_ST		BIT(6)		/* drive is ready */
#define	BSY_ST		BIT(7)		/* controller is busy */


/*
 * Commands to CSR_REG (w)
 */

#define	RESTORE(rate)	(0x10 +(rate))	/* X */
#define	SEEK(rate)	(0x70 +(rate))	/* X */
#define	READ_CMD	(0x21)		/* X */
#define	WRITE_CMD	(0x31)		/* X */
#define	FORMAT_CMD	(0x50)		/* X */
#define	VERIFY_CMD	(0x40)		/* X */
#define	DIAGNOSE_CMD	(0x90)		/* X */
#define	SETPARM_CMD	(0x91)		/* X */

/*
 * Device States.
 */

#define	SIDLE		0		/* controller idle */
#define	SRETRY		1		/* seeking */
#define	SREAD		2		/* reading */
#define	SWRITE		3		/* writing */

/*
 * Forward Referenced Functions.
 */

LOCAL void	atreset ();
LOCAL int	atdequeue ();
LOCAL void	atstart ();
LOCAL void	atdefer ();
LOCAL int	aterror ();
LOCAL void	atrecov ();
LOCAL void	atdone ();

#define	NOTBUSY()	((inb (ATSREG) & BSY_ST) == 0)
#define	ATBSYW(u)	(NOTBUSY () ? 1 : myatbsyw (u))

#define	DATAREQUESTED()	((inb (ATSREG) & DRQ_ST) != 0)
#define	ATDRQ()		(DATAREQUESTED () ? 1 : atdrq ())

/***********************************************************************
 *  We used to go to a lot of trouble to make sure that we didn't calculate
 *  any of these expressions twice. This only made the code to dequeue
 *  a block request more obscure. It would be much better to change
 *  the ordering of partitions on in our tables so the expressions
 *  cost less in terms of time to calculate.
 *  
 *  [csh]
 */


#define partn(dev)	((minor(dev) % (N_ATDRV * NPARTN)) + \
			 ((minor(dev) & SDEV) ? (N_ATDRV * NPARTN) : 0))
#define partnbase(p)	(pparm[p]->p_base)
#define partnsize(p) 	(pparm[p]->p_size)

#define drv(dev)	((minor(dev) & SDEV) ? (minor(dev) % N_ATDRV) : \
					       (minor(dev) / NPARTN))
extern typed_space	boot_gift;
extern short		at_drive_ct;


/*
 *	ATSECS is number of seconds to wait for an expected interrupt.
 *	ATSREG needs to be 3F6 for most new IDE drives;  needs to be
 *		1F7 for Perstor controllers and some old IDE drives.
 *		Either value works with most drives.
 *	atparm - drive parameters.  If initialized zero, try to use ROM values.
 */

extern	unsigned	ATSECS;
extern	unsigned	ATSREG;
extern	struct hdparm_s	atparm [];


/*
 * Partition Parameters - copied from disk.
 *
 *	There are N_ATDRV * NPARTN positions for the user partitions,
 *	plus N_ATDRV additional partitions to span each drive.
 *
 *	Aligning partitions on cylinder boundaries:
 *	Optimal partition size: 2 * 3 * 4 * 5 * 7 * 17 = 14280 blocks
 *	Acceptable partition size:  3 * 4 * 5 * 7 * 17 =  7140 blocks
 */

static struct fdisk_s pparm [N_ATDRV * NPARTN + N_ATDRV];


/*
 * Per disk controller data.
 * Only one controller; no more, no less.
 */

static struct at {
	BUF	      *	at_actf;	/* Link to first */
	BUF	      *	at_actl;	/* Link to last */
	paddr_t		at_addr;	/* Source/Dest virtual address */
	daddr_t		at_bno;		/* Block # on disk */
	unsigned	at_nsec;	/* # of sectors on current transfer */
	unsigned	at_drv;
	unsigned	at_head;
	unsigned	at_cyl;
	unsigned	at_sec;
	unsigned	at_partn;
	unsigned char	at_dtype [N_ATDRV];	/* drive type, 0 if unused */
	unsigned char	at_tries;
	unsigned char	at_state;
#if	BAD_STUFF
	unsigned	at_bad_drv;
	unsigned	at_bad_head;
	unsigned	at_bad_cyl;
#endif
	unsigned	at_totalsec;
} at;


static char timeout_msg [] = "at%d: TO\n";


/*
 * void
 * atload ()	- load routine.
 *
 *	Action:	The controller is reset and the interrupt vector is grabbed.
 *		The drive characteristics are set up at this time.
 */

LOCAL void
atload ()
{
	unsigned int u;
	struct hdparm_s * dp;
	struct { unsigned short off, seg; } p;

	if (at_drive_ct <= 0)
		return;

	/* Flag drives 0, 1 as present or not. */
	at.at_dtype [0] = 1;
	at.at_dtype [1] = at_drive_ct > 1 ? 1 : 0;

#if 0
/* hex dump boot gift */
{
int bgi;
unsigned char * bgp = (char *)& boot_gift;
printf ("& boot_gift = %lx", & boot_gift);
for (bgi = 0; bgi < 80; bgi ++) {
	printf (" %x", (* bgp ++));
}
}
#endif

	/*
	 * Obtain Drive Characteristics.
	 */
	for (u = 0, dp = atparm; u < at_drive_ct; ++ dp, ++ u) {
		struct hdparm_s int_dp;
		unsigned short ncyl = _CHAR2_TO_USHORT (dp->ncyl);

		if (ncyl == 0) {
			/*
			 * Not patched.
			 *
			 * If tertiary boot sent us parameters,
			 *   Use "fifo" routines to fetch them.
			 *   This only gives us ncyl, nhead, and nspt.
			 *   Make educated guesses for other parameters:
			 *   Set landc to ncyl, wpcc to -1.
			 *   Set ctrl to 0 or 8 depending on head count.
			 *
			 * Follow INT 0x41/46 to get drive static BIOS drive
			 * parameters, if any.
			 *
			 * If there were no parameters from tertiary boot,
			 * or if INT 0x4? nhead and nspt match tboot parms,
			 *   use "INT" parameters (will give better match on
			 *   wpcc, landc, and ctrl fields, which tboot can't
			 *   give us).
			 */

			FIFO * ffp;
			typed_space * tp;
			int found, parm_int;

if (F_NULL != (ffp = fifo_open (& boot_gift, 0))) {
			for (found = 0; ! found && (tp = fifo_read (ffp)); ) {
				BIOS_DISK * bdp = (BIOS_DISK *)tp->ts_data;
				if ((T_BIOS_DISK == tp->ts_type) &&
				    (u == bdp->dp_drive) ) {
					found = 1;
					_NUM_TO_CHAR2(dp->ncyl,
					  bdp->dp_cylinders);
					dp->nhead = bdp->dp_heads;
					dp->nspt = bdp->dp_sectors;
					_NUM_TO_CHAR2(dp->wpcc, 0xffff);
					_NUM_TO_CHAR2(dp->landc,
					  bdp->dp_cylinders);
					if (dp->nhead > 8)
						dp->ctrl |= 8;
				}
			}
			fifo_close (ffp);
}

			if (u == 0)
				parm_int = 0x41;
			else /* (u == 1) */
				parm_int = 0x46;
			pxcopy ((paddr_t)(parm_int * 4), & p, sizeof p, SEL_386_KD);
			pxcopy ((paddr_t)(p.seg <<4L)+ p.off,
				& int_dp, sizeof (int_dp), SEL_386_KD);
			if (! found || (dp->nhead == int_dp.nhead &&
			     		dp->nspt == int_dp.nspt)) {
				* dp = int_dp;
				printf ("Using INT 0x%x", parm_int);
			} else
				printf ("Using INT 0x13(08)");
		} else {
			printf ("Using patched");
			/*
			 * Avoid incomplete patching.
			 */
			if (at.at_dtype [u] == 0)
				at.at_dtype [u] = 1;
			if (dp->nspt == 0)
				dp->nspt = 17;
#if FORCE_CTRL_8
			if (dp->nhead > 8)
				dp->ctrl |= 8;
#endif

		}
#if VERBOSE > 0
	printf (" drive %d parameters\n", u);

	cmn_err (CE_CONT,
		 "at%d: ncyl=%d nhead=%d wpcc=%d eccl=%d ctrl=%d landc=%d "
		 "nspt=%d\n", u, _CHAR2_TO_USHORT (dp->ncyl), dp->nhead,
		 _CHAR2_TO_USHORT (dp->wpcc), dp->eccl, dp->ctrl,
		 _CHAR2_TO_USHORT (dp->landc), dp->nspt);
#endif
	}

	/*
	 * Initialize Drive Size.
	 */
	for (u = 0, dp = atparm; u < at_drive_ct; ++ dp, ++ u) {

		if (at.at_dtype [u] == 0)
			continue;

		pparm [N_ATDRV * NPARTN + u].p_size =
			(long) _CHAR2_TO_USHORT (dp->ncyl) * dp->nhead *
				dp->nspt;
	}

	/*
	 * Initialize Drive Controller.
	 */

	atreset ();

#if	BAD_STUFF
	at.at_bad_drv = -1;
#endif
}


/*
 * void
 * atunload ()	- unload routine.
 */

LOCAL void
atunload ()
{
}


/*
 * void
 * atreset ()	-- reset hard disk controller, define drive characteristics.
 */

LOCAL void
atreset ()
{
	int u;
	struct hdparm_s * dp;

	outb (HF_REG, 4);	/* Reset the Controller */
	busyWait2(NULL, 12);	/* Wait for minimum of 4.8 usec */
	outb (HF_REG, atparm [0].ctrl & 0x0F);

	ATBSYW (0);
	if (inb (AUX_REG) != 0x01) {
		/*
		 * Some IDE drives always timeout on initial reset.
		 * So don't report first timeout.
		 */
		static one_bad;

		if (one_bad)
			printf ("at: hd controller reset timeout\n");
		else
			one_bad = 1;
	}

	/*
	 * Initialize drive parameters.
	 */

	for (u = 0, dp = atparm; u < at_drive_ct; ++ dp, ++ u) {

		if (at.at_dtype [u] == 0)
			continue;

		ATBSYW (u);

		/*
		 * Set drive characteristics.
		 */

		outb (HF_REG, dp->ctrl);
		outb (HDRV_REG, 0xA0 + (u << 4) + dp->nhead - 1);

		outb (AUX_REG, _CHAR2_TO_USHORT (dp->wpcc) / 4);
		outb (NSEC_REG, dp->nspt);
		outb (SEC_REG, 0x01);
		outb (LCYL_REG, dp->ncyl [0]);
		outb (HCYL_REG, dp->ncyl [1]);
		outb (CSR_REG, SETPARM_CMD);
		ATBSYW (u);

		/*
		 * Restore heads.
		 */

		outb (CSR_REG, RESTORE (0));
		ATBSYW (u);
	}
}


/*
 * void
 * atopen (dev, mode)
 * dev_t dev;
 * int mode;
 *
 *	Input:	dev = disk device to be opened.
 *		mode = access mode [IPR, IPW, IPR + IPW].
 *
 *	Action:	Validate the minor device.
 *		Update the paritition table if necessary.
 */

LOCAL void
atopen (dev /* , mode */)
dev_t	dev;
{
	int d;		/* drive */
	int p;		/* partition */

	p = minor (dev) % (N_ATDRV * NPARTN);

	if (minor (dev) & SDEV) {
		d = minor (dev) % N_ATDRV;
		p += N_ATDRV * NPARTN;
	} else
		d = minor (dev) / NPARTN;

	if (d >= N_ATDRV || at.at_dtype [d] == 0) {
		printf ("atopen: drive %d not present ", d);
		set_user_error (ENXIO);
		return;
	}

	if (minor (dev) & SDEV)
		return;

	/*
	 * If partition not defined read partition characteristics.
	 */
	if (pparm [p].p_size == 0)
		fdisk (makedev (major (dev), SDEV + d), & pparm [d * NPARTN]);

	/*
	 * Ensure partition lies within drive boundaries and is non-zero size.
	 */
	if (pparm [p].p_base + pparm [p].p_size >
	    pparm [d + N_ATDRV * NPARTN].p_size) {
		printf ("atopen: p_size too big ");
		set_user_error (EINVAL);
	} else if (pparm [p].p_size == 0) {
		printf ("atopen: p_size zero ");
		set_user_error (ENODEV);
	}
}


/*
 * void
 * atread (dev, iop)	- write a block to the raw disk
 * dev_t dev;
 * IO * iop;
 *
 *	Input:	dev = disk device to be written to.
 *		iop = pointer to source I/O structure.
 *
 *	Action:	Invoke the common raw I/O processing code.
 */

LOCAL void
atread (dev, iop)
dev_t	dev;
IO	* iop;
{
	ioreq (NULL, iop, dev, BREAD, BFRAW | BFBLK | BFIOC);
}


/*
 * void
 * atwrite (dev, iop)	- write a block to the raw disk
 * dev_t dev;
 * IO * iop;
 *
 *	Input:	dev = disk device to be written to.
 *		iop = pointer to source I/O structure.
 *
 *	Action:	Invoke the common raw I/O processing code.
 */

LOCAL void
atwrite (dev, iop)
dev_t	dev;
IO	* iop;
{
	ioreq (NULL, iop, dev, BWRITE, BFRAW | BFBLK | BFIOC);
}


/*
 * void
 * atioctl (dev, cmd, arg)
 * dev_t dev;
 * int cmd;
 * char * vec;
 *
 *	Input:	dev = disk device to be operated on.
 *		cmd = input / output request to be performed.
 *		vec = (pointer to) optional argument.
 *
 *	Action:	Validate the minor device.
 *		Update the paritition table if necessary.
 */

LOCAL void
atioctl (dev, cmd, vec)
dev_t	dev;
int cmd;
char * vec;
{
	int d;

	/*
	 * Identify drive number.
	 */
	if (minor (dev) & SDEV)
		d = minor (dev) % N_ATDRV;
	else
		d = minor (dev) / NPARTN;

	/*
	 * Identify input / output request.
	 */

	switch (cmd) {

	case HDGETA:
		/*
		 * Get hard disk attributes.
		 */
		kucopy (atparm + d, vec, sizeof (atparm [0]));
		break;

	case HDSETA:
		/* Set hard disk attributes. */
		ukcopy (vec, atparm + d, sizeof (atparm [0]));
		at.at_dtype [d] = 1;		/* set drive type nonzero */
		pparm [N_ATDRV * NPARTN + d].p_size =
			  (long) _CHAR2_TO_USHORT (atparm [d].ncyl) *
			  atparm [d].nhead * atparm [d].nspt;
		atreset ();
		break;

	default:
		set_user_error (EINVAL);
		break;
	}
}


/*
 * void
 * atwatch ()		- guard against lost interrupt
 *
 *	Action:	If drvl [AT_MAJOR] is greater than zero, decrement it.
 *		If it decrements to zero, simulate a hardware interrupt.
 */

LOCAL void
atwatch ()
{
	BUF * bp = at.at_actf;
	int s;


	s = sphi ();

	if (-- drvl [AT_MAJOR].d_time > 0) {
		spl (s);
		return;
	}

	/*
	 * Reset hard disk controller, cancel request.
	 */

	atreset ();

	if (at.at_tries ++ < SOFTLIM) {
#if PV_HUNT
		cmn_err (CE_WARN, "atwatch TIMEOUT, %d TRIES", at.at_tries);
#endif
		atstart ();
	} else {
		cmn_err (CE_WARN,
		  "at%d%c: bno=%lu head=%u cyl=%u nsec=%u tsec=%d "
		  "dsec=%d <Watchdog Timeout>\n", at.at_drv,
		  (bp->b_dev & SDEV) ? 'x' : at.at_partn % NPARTN + 'a',
		  bp->b_bno, at.at_head, at.at_cyl, at.at_nsec,
		  at.at_totalsec, inb (NSEC_REG));

		at.at_actf->b_flag |= BFERR;
		atdone (at.at_actf);
	}

	spl (s);
}


/*
 * void
 * atblock (bp)	- queue a block to the disk
 *
 *	Input:	bp = pointer to block to be queued.
 *
 *	Action:	Queue a block to the disk.
 *		Make sure that the transfer is within the disk partition.
 */

LOCAL void
atblock (bp)
BUF	* bp;
{
	struct fdisk_s * pp;
	int partn = minor (bp->b_dev) % (N_ATDRV * NPARTN);
	int		s;

	bp->b_resid = bp->b_count;

	if (minor (bp->b_dev) & SDEV)
		partn += N_ATDRV * NPARTN;

	pp = pparm + partn;

	/*
	 * Check for read at end of partition.
	 */

	if (bp->b_req == BREAD && bp->b_bno == pp->p_size) {
		bdone (bp);
		return;
	}

	/*
	 * Range check disk region.
	 */

	if (bp->b_bno + (bp->b_count / BSIZE) > pp->p_size ||
	    bp->b_count % BSIZE != 0 || bp->b_count == 0) {
		bp->b_flag |= BFERR;
		bdone (bp);
		return;
	}

	s = sphi ();
	bp->b_actf = NULL;
	if (at.at_actf == NULL)
		at.at_actf = bp;
	else
		at.at_actl->b_actf = bp;
	at.at_actl = bp;
	spl (s);

	if (at.at_state == SIDLE)
		if (atdequeue ())
			atstart ();
}


/*
 * int
 * atdequeue ()		- obtain next disk read / write operation
 *
 *	Action:	See if there is work to do in the work queue. If so
 *		zero the retry counter.
 *
 *	Return:	0 = no work.
 *		1 = work to do.
 */

LOCAL int
atdequeue ()
{
	if (!at. at_actf)
		return 0;
	else {
		at.at_tries = 0;
		return 1;
	}
}


LOCAL void
atsend (addr)
paddr_t		addr;
{
	if (!addr) {
		panic("atsend: NULL buffer address!");
	}

	addr = P2P (addr);
	repoutsw (DATA_REG, (unsigned short *) __PTOV (addr), BSIZE / 2);
}


LOCAL void
atrecv (addr)
paddr_t		addr;
{
	if (!addr) {
		panic("atrecv: NULL buffer address!");
	}
		
	addr = P2P (addr);
	repinsw (DATA_REG, (unsigned short *) __PTOV (addr), BSIZE / 2);
}


/*
 * Abandon all the existing requests.
 */

LOCAL void
atabandon ()
{
	buf_t	      *	bp;

	/*
	 * Abandon this operation.
	 */

	while ((bp = at.at_actf) != NULL) {
		at.at_actf = bp->b_actf;
		bp->b_flag |= BFERR;
		bdone (bp);
	}
	at.at_state = SIDLE;
}


/*
 * void
 * atstart ()	- start or restart next disk read / write operation.
 *
 *	Action:	Initiate disk read / write operation.
 */

LOCAL void
atstart ()
{
	BUF 		*bp;		/* Utility buffer. */
	struct hdparm_s *dp;		/* Drive paramters */
	ldiv_t		addr;		/* Utility address */
	unsigned short	secs;		/* sectors, this request */
	unsigned short	newsec;		/* sectors all requests so far */

	/***************************************************************
         *  Sanity check - Is there any work to do?
         */

        if (!(bp = at. at_actf)) {
#if 0
        	cmn_err(CE_NOTE, "at: atstart called with empty work queue\n");
#endif
        	at. at_actf = at. at_addr = NULL;
        	at. at_nsec = 0;
        	drvl[AT_MAJOR]. d_time = 0;
		return;
	}
	
#if	BAD_STUFF
	/*
	 * Check for repeated access to most recently identified bad track.
	 */

	if (at.at_drv == at.at_bad_drv && at.at_cyl == at.at_bad_cyl &&
	    at.at_head == at.at_bad_head) {
	  	BUF * bp = at.at_actf;
		printf ("at%d%c: bno=%lu head=%u cyl=%u <Track Flagged Bad>\n",
			at.at_drv,
			(bp->b_dev & SDEV) ? 'x' : at.at_partn % NPARTN + 'a',
			bp->b_bno,
			at.at_head,
			at.at_cyl);
		bp->b_flag |= BFERR;
		atdone (bp);
		return;
	}
#endif
	/***************************************************************
         *  Now calculate the parameters for the controller for the
	 *  current request. This used to be done in atdequeue and was
         *  probably appropriate when the driver was first written.
         *  After the driver was modified to read ahead in the buffer
         *  queue however this changed. In particular a nasty bug was
         *  generated which resulted in the work queue being empty
         *  but the drivers control structure thinking that there was
         *  still work to do:
         *  
         *  atdequeue calculates the cylinder, head, starting sector,
         *  and number of sectors for  a given multi-sector request.
         *  
         *  atstart programs the controller with the given values and
         *  starts the appropriate timers for this request.
         *  
         *  atintr/atdefer handshakes the data in for a couple of these
         *  transfers and the then hands the buffers back to the kernel
         *  saying the I/O completed successfully.
         *  
         *  The controller times out on a request for some reason or
         *  another. Why this happens I don't know but I do know that
         *  some of the AT drives periodically rezero themselves. This
         *  could be a part of the problem.
         *  
         *  atwatch catchs this and as a part of its recovery calls
         *  atstart to restart the transfer. The problem is here. Since
         *  a couple of blocks have been transfered correctly the hardware
         *  address computed in atdequeue are now bogus. But we use
         *  them to reprogram the hardware anyhow. This has two effects:
         *  
         *      1)  Bogus sectors get read into the buffers.
         *  
         *      2a) If no more requests were added to the queue
         *          atintr/atdefer will attempt to write to system
         *          global address 0 causing a GP fault.
         *  
         *      2b) Or worse, if requests were added to the queue
         *          they will be treated as reads regardless of what
         *          they really are and the buffers behind them will
         *          be corrupted with data read from the bogus address
         *          programmed into the controller. In the best case
         *          This will cause only cause random file corruption.
         */

	/***************************************************************
         *  Calculate the partition and drive number from the minor
         *  device of the first block on the list.
         *  
         *  NOTICE: This should be handled by a macro everywhere.
         *  [csh]
         */

	at.at_partn = minor (bp->b_dev) % (N_ATDRV * NPARTN);
	if (minor (bp->b_dev) & SDEV) {
		at.at_partn += N_ATDRV * NPARTN;
		at.at_drv = minor (bp->b_dev) % N_ATDRV;
	} else
		at.at_drv = minor (bp->b_dev) / NPARTN;


	/***************************************************************
         *  Test the macros. [csh]
         */

	if (partn(bp->b_dev) != at.at_partn)
        	panic("at.c: partno() macro failed (%d,%d)", major(bp->b_dev), minor(bp->b_dev));
	if (drv(bp->b_dev) != at.at_drv)
        	panic("at.c: drv() macro failed (%d,%d)", major(bp->b_dev), minor(bp->b_dev));


        /***************************************************************
         *  This is the kind of garbage that gets us into big trouble.
         *  The next line of code calculates a variable that is never
         *  used. This is especially stupid considering the obscurity
         *  added to avoid calculating:
         *  
         *      (minor(bp->b_dev) & SDEV) 
         *  
         *  twice.
         */
#if 0
	nspt = atparm [at.at_drv].nspt;
#endif
	at.at_addr = bp->b_paddr;
	at.at_bno = pparm[at.at_partn]. p_base + bp->b_bno;

	/*
	 * Added by Louis to trap divide by zero problem.
	 * When the disk becomes full, for some reason one
	 * of the ldiv's below causes a kernel panic with
	 * a trap on divide by zero.  Unless the atparm
	 * struct is being corrupted, at.at_drv may be wrong
	 * if given a negative drive number, or one too large.
	 * Assuming that N_ATDRV does not change, this means there
	 * is a sign-extension problem of some type and/or
	 * there is a plain-old-hosed inode on the system displaying
	 * this behavior, since bp->b_dev in the system call
	 * that paniced got the b_dev value from the inode
	 * i_rdev field.  Also, sign extension is possible, since
	 * minor(a) is just #def'ed as (a & 0xFF).....
	 */
	
	ASSERT(at.at_drv >= 0 && at.at_drv <= 1);
	ASSERT(at.at_partn >= 0);

	/***************************************************************
         *  Generate a pointer to the disk drive geometry table.
         */

	dp = &(atparm[at.at_drv]);

	addr = ldiv (at.at_bno, dp->nspt);
	at.at_sec = addr.rem + 1;
	addr = ldiv (addr.quot, dp->nhead);
	at.at_cyl = addr.quot;
	at.at_head = addr.rem;

	/*
	 * NIGEL: It is unclear why, but IDE writes appear to always blow a
	 * revolution no matter what, even though reads appear to work quite
	 * comfortably. It may be that this is a problem caused by IDE drives
	 * trying to maintain the synchronous semantics of the write, and/or
	 * because we are actually not making the read time either but the
	 * slack is taken up by track-buffering.
	 *
	 * Either way, we gain a vast improvement in throughput for writes and
	 * a modest gain for reads by looking ahead in the request chain and
	 * coalescing separate requests to consecutive blocks into a single
	 * multi-sector request (as far as the interface is concerned).
	 */

	newsec = secs = bp->b_count / BSIZE;
	while (bp->b_actf != NULL && bp->b_actf->b_bno == bp->b_bno + secs &&
	       bp->b_actf->b_req == bp->b_req &&
	       bp->b_actf->b_dev == bp->b_dev) {
		bp = bp->b_actf;

		/*******************************************************
                 *  The sector count register on the controller is
                 *  one byte long. Make sure that the number of sectors
                 *  to read will fit. Notice that a 0 in this register
                 *  means read 256 sectors.
                 */

		if (newsec + (secs = bp->b_count / BSIZE) > 256)
			break;
		newsec += secs;
	}
	at.at_totalsec = at.at_nsec = newsec;

	ATBSYW (at.at_drv);

	dp = atparm + at.at_drv;

	outb (HF_REG, dp->ctrl);

	/***************************************************************
         *  Would set this to 0 please tell me when write 
         *  precompensation became either
         *  
         *      a) unneccessary and harmful on new drives or,
         *      b) just unneccessary on the drives older drives that
         *      did use write precomp?
         *  
         *  [csh]
         */

#if	1
	outb (AUX_REG, _CHAR2_TO_USHORT (dp->wpcc) / 4);
#endif
	outb (HDRV_REG, (at.at_drv << 4) + at.at_head + 0xA0);

	outb (NSEC_REG, at.at_nsec);
	outb (SEC_REG, at.at_sec);
	outb (LCYL_REG, at.at_cyl);
	outb (HCYL_REG, at.at_cyl >> 8);

	if (inb (NSEC_REG) != (at.at_nsec & 0xFF)) {
		/*
		 * If we get here, things are confused. We should reset the
		 * controller and retry whatever operation we want to start
		 * now.
		 */


		drvl [AT_MAJOR].d_time = 1;
		return;
	}

	if (at.at_actf->b_req == BWRITE) {

		outb (CSR_REG, WRITE_CMD);

		while (ATDRQ () == 0) {
			cmn_err(CE_WARN,
			   "at%d%c: Failure starting write: no response from drive",
			   at.at_drv, (at.at_actf->b_dev & SDEV) ? 'x' : at.at_partn %
			   NPARTN + 'a');
		
			atabandon ();
			return;
		}

		atsend (at.at_addr);
		at.at_state = SWRITE;
	} else {
		outb (CSR_REG, READ_CMD);
		at.at_state = SREAD;
	}

	drvl [AT_MAJOR].d_time = ATSECS;
}


/*
 * void
 * atintr ()	- Interrupt routine.
 *
 *	Clear interrupt then defer actual processing.
 */

void
atintr ()
{
#if	0
	do {
#endif
		(void) inb (CSR_REG);	/* clears controller interrupt */

		atdefer ();
#if	0
	} while (at.at_state != SIDLE && NOTBUSY ());
#endif

	/*
	 * NIGEL: One upon a time we tried spinning in the interrupt routine
	 * if the controller could finish what it was doing quickly enough.
	 * In the first attempt at this, we cleared the controller interrupt
	 * only the first time we came in, which apparently was a mistake;
	 * some controllers get /very/ confused by this, despite the fact
	 * it appears to be legal. The symptoms were sporadic timeouts in
	 * the cases where the controllers were sentitive to this, although
	 * in one case the controller want totally haywire and vaporized the
	 * partition table!
	 *
	 * It still isn't clear whether or not this is a good idea, or whether
	 * controller problems render this pointless...
	 */
}


/*
 * void
 * atdefer ()	- Deferred service of hard disk interrupt.
 *
 *	Action:	Service disk interrupt.
 *		Transfer required data.
 *		Update state.
 */

LOCAL void
atdefer ()
{
	BUF * bp = at.at_actf;

	switch (at.at_state) {

	case SRETRY:
		atstart ();
		break;

	case SREAD:
		/*
		 * Check for I/O error before waiting for data.
		 */

		if (aterror ()) {
			atrecov ();
			break;
		}

		/*
		 * Wait for data, or forever.
		 */

		if (ATDRQ () == 0) {
			cmn_err(CE_WARN,
			   "at%d%c: Failure on read: no response from drive",
			   at.at_drv, (bp->b_dev & SDEV) ? 'x' : at.at_partn %
			   NPARTN + 'a');
			
			atabandon ();
			break;
		}

		/*
		 * Read data block.
		 */

		atrecv (at.at_addr);

		/*
		 * Check for I/O error after reading data.
		 */

		if (aterror ()) {
			atrecov ();
			break;
		}

		/*
		 * Every time we transfer a block, bump the timeout to prevent
		 * very large multisector transfers from timing out due to
		 * sheer considerations of volume.
		 */

#if PV_HUNT
		drvl [AT_MAJOR].d_time = ATSECS;
#else
		drvl [AT_MAJOR].d_time = ATSECS * 2;
#endif

		at.at_addr += BSIZE;
		bp->b_resid -= BSIZE;

		at.at_tries = 0;
		at.at_bno ++;

		/*
		 * Check for end of transfer (total, or simply part of a large
		 * combined request).
		 */

		if (-- at.at_nsec == 0)
			atdone (bp);
		else if (bp->b_resid == 0) {
			at.at_addr = (at.at_actf = bp->b_actf)->b_paddr;
			bdone (bp);
		}
		break;

	case SWRITE:
		/*
		 * Check for I/O error.
		 */

		if (aterror ()) {
			atrecov ();
			break;
		}

		/*
		 * Every time we transfer a block, bump the timeout to prevent
		 * very large multisector transfers from timing out due to
		 * sheer considerations of volume.
		 */

#if PV_HUNT
		drvl [AT_MAJOR].d_time = ATSECS;
#else
		drvl [AT_MAJOR].d_time = ATSECS * 2;
#endif

		at.at_addr += BSIZE;
		bp->b_resid -= BSIZE;
		at.at_tries = 0;
		at.at_bno ++;

		/*
		 * Check for end of transfer, either the real end or the end
		 * of a block boundary in a combined transfer.
		 */

		if (-- at.at_nsec == 0) {
			atdone (bp);
			break;
		} else if (bp->b_resid == 0)
			at.at_addr = bp->b_actf->b_paddr;

		/*
		 * Wait for ability to send data, or forever.
		 */

		while (ATDRQ () == 0) {
			/* 
			 * Changed to print warning and also to
			 * return instead of breaking since the
			 * following code is bogus after an
			 * atabandon.
			 */
			cmn_err(CE_WARN,
			   "at%d%c: Failure on write: no response from drive",
			   at.at_drv, (bp->b_dev & SDEV) ? 'x' : at.at_partn %
			   NPARTN + 'a');
			atabandon();

			return;
	
		}

		/*
		 * Send data block.
		 */

		atsend (at.at_addr);

		if (bp->b_resid == 0) {
			at.at_actf = bp->b_actf;
			bdone (bp);
		}
	}
}


/*
 * int
 * aterror ()
 *
 *	Action:	Check for drive error.
 *		If found, increment error count and report it.
 *
 *	Return: 0 = No error found.
 *		1 = Error occurred.
 */

LOCAL int
aterror ()
{
	BUF * bp = at.at_actf;
	int csr;
	int aux;

	if ((csr = inb (ATSREG)) & (ERR_ST | WFLT_ST)) {

		cmn_err (CE_WARN, "aterror SOFT ERROR csr = %x", csr);

		aux = inb (AUX_REG);

		if (aux & BAD_ERR) {
			at.at_tries	= BADLIM;
#if	BAD_STUFF
			at.at_bad_drv	= at.at_drv;
			at.at_bad_head	= at.at_head;
			at.at_bad_cyl	= at.at_cyl;
#endif
		} else if (++ at.at_tries < SOFTLIM)
			return 1;

#if	1
		printf ("at%d%c: bno =%lu head =%u cyl =%u",
			at.at_drv,
			(bp->b_dev & SDEV) ? 'x' : at.at_partn % NPARTN + 'a',
			(bp->b_count / BSIZE) + bp->b_bno - at.at_nsec,
			at.at_head, at.at_cyl);

#if VERBOSE > 0
		if ((csr & RDY_ST) == 0)
			printf (" <Drive Not Ready>");
		if (csr & WFLT_ST)
			printf (" <Write Fault>");

		if (aux & DAM_ERR)
			printf (" <No Data Addr Mark>");
		if (aux & TR0_ERR)
			printf (" <Track 0 Not Found>");
		if (aux & ID_ERR)
			printf (" <ID Not Found>");
		if (aux & ECC_ERR)
			printf (" <Bad Data Checksum>");
		if (aux & ABT_ERR)
			printf (" <Command Aborted>");
#else
		if ((csr & (RDY_ST | WFLT_ST)) != RDY_ST)
			printf (" csr =%x", csr);
		if (aux & (DAM_ERR | TR0_ERR | ID_ERR | ECC_ERR | ABT_ERR))
			printf (" aux =%x", aux);
#endif
		if (aux & BAD_ERR)
			printf (" <Block Flagged Bad>");

		if (at.at_tries < HARDLIM)
			printf (" retrying...");
		printf ("\n");
#endif
		return 1;
	}
	return 0;
}


/*
 * void
 * atrecov ()
 *
 *	Action:	Attempt recovery.
 */

LOCAL void
atrecov ()
{
	BUF * bp = at.at_actf;
	int cmd = SEEK (0);
	int cyl = at.at_cyl;

	printf("RECOVERING!\n");

	switch (at.at_tries) {

	case 1:
	case 2:
		/*
		 * Move in 1 cylinder, then retry operation
		 */
		if (--cyl < 0)
			cyl += 2;
		break;

	case 3:
	case 4:
		/*
		 * Move out 1 cylinder, then retry operation
		 */
		if (++ cyl >= _CHAR2_TO_USHORT (atparm [at.at_drv].ncyl))
			cyl -= 2;
		break;

	case 5:
	case 6:
		/*
		 * Seek to cylinder 0, then retry operation
		 */
		cyl = 0;
		break;

	default:
		/*
		 * Restore drive, then retry operation
		 */
		cmd = RESTORE (0);
		cyl = 0;
		break;
	}

	/*
	 * Retry operation [after repositioning head]
	 */
	if (at.at_tries < HARDLIM) {
#if PV_HUNT
		drvl [AT_MAJOR].d_time = ATSECS;
#else
		drvl [AT_MAJOR].d_time = cmd == RESTORE (0) ? ATSECS * 2 :
							      ATSECS;
#endif
		outb (LCYL_REG, cyl);
		outb (HCYL_REG, cyl >> 8);
		outb (HDRV_REG, (at.at_drv << 4) + 0xA0);
		outb (CSR_REG, cmd);
		at.at_state = SRETRY;
	} else {
		/*
		 * Give up on block.
		 */

		bp->b_flag |= BFERR;
		atdone (bp);
	}
}


/**
 * void
 * atdone (bp)
 * BUF * bp;
 *
 *	Action:	Release current I/O buffer.
 */

LOCAL void
atdone (bp)
BUF * bp;
{
	at.at_actf = bp->b_actf;
	drvl [AT_MAJOR].d_time = 0;
	at.at_state = SIDLE;

	if (atdequeue ())
		atstart ();

	bdone (bp);
}


int
notBusy ()
{
	return NOTBUSY ();
}

int
dataRequested ()
{
	return DATAREQUESTED ();
}


/*
 * NIGEL: Set up to report a timeout.
 */

static	int		report_scheduled;
static	int		report_drv;

LOCAL void
_report_timeout ()
{
	printf (timeout_msg, report_drv);
	report_scheduled = 0;
}

LOCAL void
report_timeout (unit)
int		unit;
{
	short		s = sphi ();
	if (report_scheduled == 0) {
		report_scheduled = 1;
		spl (s);

		report_drv = unit;
		defer (_report_timeout);
	} else
		spl (s);
}


/*
 * Wait while controller is busy.
 *
 * Return 0 if timeout, nonzero if not busy.
 */

int
myatbsyw (unit)
int unit;
{
	if (busyWait (notBusy, ATSECS * HZ))
		return 1;
	report_timeout (unit);
	return 0;
}

/*
 * Wait for controller to initiate request.
 *
 * Return 0 if timeout, 1 if data requested.
 */

int
atdrq ()
{
	if (busyWait (dataRequested, ATSECS * HZ))
		return 1;
	report_timeout (at.at_drv);
	return 0;
}


CON	atcon	= {
	DFBLK | DFCHR,			/* Flags */
	AT_MAJOR,			/* Major index */
	atopen,				/* Open */
	NULL,				/* Close */
	atblock,			/* Block */
	atread,				/* Read */
	atwrite,			/* Write */
	atioctl,			/* Ioctl */
	NULL,				/* Powerfail */
	atwatch,			/* Timeout */
	atload,				/* Load */
	atunload			/* Unload */
};

/* End of file */


#if	CSH_MERGE_IN
#endif

#if	CSH_MERGE_HOLD
/***********************************************************************
 *  atdone()
 *  
 *  Return the top block in the driver's queue to the kernel.
 */

LOCAL void atdone(restart)
int restart;
{
	int s;

	s = sphi();
	if (!at.at_actf) {
		cmn_err(CE_NOTE, "at: <Driver idle - atdone>");

		at.at_actf = at.at_actl = (char *) at. at_addr = NULL;
		at.at_blkcnt = at.at_nsec = 0;
		at.at_state = SIDLE;
		drvl[AT_MAJOR].d_time = 0;
	}
	else {
		/*******************************************************
                 *  Mark the top block done and move to next block.
                 */

		bp = at.at_actf;
		at.at_actf = bp->b_actf;
		bp->b_actf = bp->b_actl = NULL;
		bdone(bp);

		/*******************************************************
                 *  Decrement the block count and check for a restart.
                 */
		
		--at.at_blkcnt;
		if (at.at_blkcnt <= 0 || restart) {
			at.at_state = SIDLE;
			if (!atdequeue()) {
				at.at_actf = at.at_actl = (char *) at.at_addr = NULL;
				at.at_blkcnt = at.at_nsec = 0;	
				drvl[AT_MAJOR].d_time = 0;
			}
			else
				atstart();
		}
	}
	spl(s);
}	/* atdone() */

/***********************************************************************
 *  atrecov()
 *  
 *  Error recovery function, called either when an hard error occurs
 *  (as indicated by the hardware) or when a request times out. 
 */

LOCAL void atrecov()

{
	if (++at.at_tries < HARDLIM) {
		switch (at.at_tries) {
		case 1:
		case 2:
			/***********************************************
                         *  Seek to one cylinder before the target.
                         */

	                cmd = SEEK(0);
			cyl = at.at_cyl - 1;
			if (cyl < 0)
				cyl = 0;
			break;

		case 3:
		case 4:
			/***********************************************
                         *  Seek to one cylinder after the target.
                         */

			cmd = SEEK(0);
			cyl = at.at_cyl + 1;
			if (cyl > atparm[at.at_drv].ncyl)
				cyl = atparm[at.at_drv].ncyl;
			break;
		case 5:
		case 6:
			/***********************************************
                         *  Rezero the Drive.
                         */

			cmd = SEEK(0);
			cyl = 0;
			break;

		default:
			/***********************************************
                         *  Try restoring the drive. Note that the
                         *  docs say that this operation may need to
                         *  be done twice to actually work.
                         */
		
			cmd = RESTORE (0);
			cyl = 0;
			break;
				
		}

		/*******************************************************
                 *  Program the controller for whatever operation we
                 *  gave it.
                 */

		drvl [AT_MAJOR].d_time = (cmd == RESTORE(0)) ? ATSECS * 2 :
							       ATSECS;
		outb (LCYL_REG, cyl);
		outb (HCYL_REG, cyl >> 8);
		outb (HDRV_REG, (at.at_drv << 4) + 0xA0);
		outb (CSR_REG, cmd);
		at.at_state = SRETRY
	}

	/***************************************************************
         *  Give up on the block and restart the transfer from the
         *  next one in the chain.
         */

	else {
		at.at_actf->b_flag |= BFERR;
		atdone(1);
	}
}	/* atrecov() */

/***********************************************************************
 *  atwatch()
 *  
 *  Watchdog timeout. If Driver is idle just mark the state and leave
 *  otherwise reset the controller and call the recovery routine.
 */

LOCAL void atwatch()

{
	int s;
	
	s = sphi();
	if (!at.at_actf) {
		cmn_err(CE_NOTE, "at: <Driver idle watchdog timeout>");

		at.at_actf = at.at_actl = (char *) at. at_addr = NULL;
		at.at_nsec = 0;
		at.at_state = SIDLE;

		drvl[AT_MAJOR]. d_time = 0;
	}	/* if */
	else {
		cmn_err(CE_WARN, "at: <Watchdog timeout>");
		
		atreset();		/* Reset the controller */
		atrecov();		/* Call the recovery routine */
	}
	spl(s);
	return;
}	/* atwatch() */

/***********************************************************************
 *  atrecv()
 *  
 *  Read a sector from the controller's buffer.
 *  
 *  Returns     0 on errors,
 *              1 on success.
 */

LOCAL int atrecv(bp)
buf_t	*bp;
{
	paddr_t	sysg_addr;
	paddr_t	phys_addr;
	
	/***************************************************************
         *  First make sure that the data is actually going somewhere.
         *  If it isn't then something is really foo bar (since we
         *  check all over the place.)
         */

	if (!phys_addr) {
		cmn_err(CE_WARN, "at: <Empty buffer address in atrecv>");
		return 0;		
	}

	/***************************************************************
         *  Next make sure that the drive is ready to send data. If
         *  it times out here we are in big trouble.
         */

        if (!ATDRQ()) {
		cmn_err(CE_WARN,
			"at: <Read command transfer failed - device: (%d,%d) blockno: %d>", 
			major(bp->b_dev),
			minor(bp->b_dev),
			bp->b_bno);
		return 0;
	}

	/***************************************************************
         *  Otherwise assume the address is good and transfer a sector
         *  to it.
         */

        /***************************************************************
         *  Notice that what we are calculating in the next couple of
         *  lines should be bp->b_vaddr. I'll run an assert test before
         *  I change this though.
         */

        sysg_addr = bp->b_paddr + bp->b_count - bp->b_resid;
	phys_addr = P2P(sysg_addr);
	repinsw(DATA_REG, __PTOV(phys_addr), BSIZE / 2);
	bp->b_resid -= BSIZE
	return 1;
}	/* atrecv() */

/***********************************************************************
 *  atsend()
 *  
 *  Send a sector to the drive (assumed for a write operation).
 */

LOCAL int atsend(bp)
buf_t	*bp;
{
	paddr_t	sysg_addr;
	paddr_t	phys_addr;
	
	/***************************************************************
         *  First make sure that the data is actually going somewhere.
         *  If it isn't then something is really foo bar (since we
         *  check all over the place.)
         */

	if (!phys_addr) {
		cmn_err(CE_WARN, "at: <Empty buffer address in atsend>");
		return 0;		
	}

	/***************************************************************
         *  Next make sure that the drive is ready to send data. If
         *  it times out here we are in big trouble.
         */

        if (!ATDRQ()) {
		cmn_err(CE_WARN,
			"at: <Write command transfer failed - device: (%d,%d) blockno: %d>", 
			major(bp->b_dev),
			minor(bp->b_dev),
			bp->b_bno);
		return 0;
	}

	/***************************************************************
         *  Otherwise assume the address is good and transfer a sector
         *  to it.
         */

        /***************************************************************
         *  Notice that what we are calculating in the next couple of
         *  lines should be bp->b_vaddr. I'll run an assert test before
         *  I change this though.
         */

        sysg_addr = bp->b_paddr + bp->b_count - bp->b_resid;
	phys_addr = P2P(sysg_addr);
	repoutsw(DATA_REG, __PTOV(phys_addr), BSIZE / 2);
	bp->b_resid -= BSIZE;
	return 1;
}	/* atsend() */

/***********************************************************************
 *  atintr()
 *  
 *  Command complete interrupt handler.
 */

LOCAL void atintr()

{
	(void) inb (CSR_REG);	/* clears controller interrupt */
	switch (at.at_state) {
	case SIDLE:
		cmn_err(CE_NOTE, "at: <Spurious interrupt>");
		break;

	case SREAD:
		/*******************************************************
                 *  If an error happened on the last transfer go into
                 *  recovery.
                 */

		if (aterror()) {
			atrecov();
			break;
		}

		/*******************************************************
                 *  If there isn't a buffer in the queue things are
                 *  really screwed up. Print a message to the console
                 *  saying so and then reset the state back to normal.
                 */

		if (!(bp = at.at_actf)) {
			cmn_err(CE_WARN, "at: <Attempted read into empty buffer>");
			atabandon();
			break;
		}

		/*******************************************************
                 *  Finally, try to read the sector into the controller's
                 *  buffer. If this fails then abandon the whole request
                 *  chain until I can think of something better to do.
                 */

		if (!atrecv(bp)) {
			atabandon();
			break;
		}

		/*******************************************************
                 *  If this finished this sector then return the block
                 *  to the kernel and queue another request if   
                 *  neccessary. Assume that this request does not finish
                 *  up the driver's work queue and keep the watchdog timer
                 *  alive. Defer the decision to kill it to atdone.
                 */

                drvl[AT_MAJOR].d_time = ATSECS;
		if (bp->b_resid == 0)
			atdone(0);
		break;

	case SWRITE:
		break;

	case SRETRY:
		atstart();
		break;
	}	/* switch */
}	/* atintr() */

#endif
