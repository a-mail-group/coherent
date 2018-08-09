/*
 * $Header: /v/src/rcskrnl/conf/fd/src/RCS/fl386.c,v 420.7 1993/12/10 22:09:04 srcadm Exp srcadm $
 */

/*********************************************************************
 *
 * Coherent, Mark Williams Company, Copyright 1993
 * RCS Header
 * This file contains proprietary information and is considered
 * a trade secret.  Unauthorized use is prohibited.
 *
 *
 * $Id: fl386.c,v 420.7 1993/12/10 22:09:04 srcadm Exp srcadm $
 *
 * $Log: fl386.c,v $
 * Revision 420.7  1993/12/10  22:09:04  srcadm
 * Fixed problems with end of disk reads by changing range checking code.
 *
 * Revision 420.6  1993/12/04  00:02:55  srcadm
 * Fixed problem with reading to end of raw device and receiving error.
 *
 *
 */

#ifdef EMBEDDED_VERSION
/* Embedded Version Constant */
char *MWC_FL386_VERSION = "MWC_FL386_VERSION($Revision: 420.7 $)";
#endif

/*
 * Diskette device control.
 * Requires 765 controller module fdc.c
 *
 * Revision 2.6  93/08/19  10:38:47  nigel
 * r83 ioctl (), corefile, new headers
 * 
 * Revision 2.5  93/08/19  04:02:21  nigel
 * Nigel's R83
 * 
 * Modified: 7/93 - autosensing and track 0 skips for iBCS2
 * complianacy and greater compatibility - CA8
 */

#define NO_RECAL1	0
#define NO_RECAL2	0
#define ST2_DD_RECAL	1
/*
 * Minor device assignments: xxuuhkkk
 *	uu  - unit = 0/1/2/3
 *	kkk - kind, struct fdata infra.
 *	h   - alternating head rather than side by side
 *	xx  - 0=diskette 1=floppy tape
 */

#include <sys/coherent.h>
#include <kernel/trace.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/con.h>
#include <sys/dmac.h>
#include <sys/devices.h>
#include <sys/fdc765.h>
#include <sys/fdioctl.h>
#include <sys/file.h>
#include <sys/sched.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uproc.h>

#define MAXDRVS  2	/* Maximum number of drives that we will support */
#define MAXSCTRS 21	/* Maximum acceptable sectors per track.  It is  */
			/* possible to put as many as 10 sectors/track	 */
			/* on low density diskettes, 18 sectors/track on */
			/* 5 1/4" high-density diskettes, and 21 sectors */
			/* per track on 3 1/2" high density diskettes.   */
			/* the maximum on "2.88" meg diskettes is not    */
			/* known as of this writing, but may be as high  */
			/* as 42.					 */
#define MAXTYPE  4	/* Maximum "type" value as set in the CMOS RAM.  */
			/* The current value of this is 4, the type for  */
			/* 1.44 meg diskettes.				 */

#ifdef FL_XTRA
/* This is conditioned out for now due to end-of-volume problems. */
#define FL_CYL_2STEP	42
#define FL_CYL_HDLO	82
#define FL_CYL_HDHI	83
#else
#define FL_CYL_2STEP	40
#define FL_CYL_HDLO	80
#define FL_CYL_HDHI	80
#endif

#define	MTIMER	2			/* Motor timeout (seconds) */

/*
 * Driver States.
 */
#define	SIDLE	0			/* Idle */
#define	SSEEK	1			/* Need seek */
#define	SRDWR	2			/* Need read/write command */
#define	SENDIO	3			/* Need end I/O processing */
#define	SDELAY	4			/* Delay before next disk operation */
#define	SHDLY	5			/* Head settling delay before r/w */
#define SLOCK	6			/* Got DMA controller lock */
#define SRECAL1 7			/* First recalibrate attempt */
#define SRECAL2 8			/* Try seeking to track 2 */
#define SRECAL3 9			/* Second recalibrate, if necessary */
#define SGOTO2	10			/* After seek to cylinder 2 */
#define SRDID	11			/* Get sector ID from FDC */
#define SSIDTST 12			/* Testing # of sides */

#define funit(x)	(minor(x) >> 4)   /* Unit/drive number */
#define fkind(x)	(minor(x) & 0x7)  /* Kind of format */
/*
 * From this point on, we will only support head-by-head attempts.
 * This is left here in original form as a comment in case something
 * breaks and it needs to be changed back.  Should eventually be
 * removed altogether. This was done because bit position 3 (2^3)
 * is now used to designate a special device - louis 7/93
 */
/* #define fhbyh(x)	(minor(x) & 0x8) */ /* 0=Side by side, 1=Head by head */
#define fhbyh(x)	1 /* Always true from now on */
#define fisspecial(x)	(!(minor(x) & 0x8))	/* if bit zero special device */
/* All special devices skip cylinder 0 except type 1; no normal devices skip */
#define fskip_cyl_0(x)	(fisspecial(x) && fkind(x) != 1)
/* Only special device types 0 and 1 are autosensing */
#define fautosense(x)	(fisspecial(x) && (fkind(x) == 0 || fkind(x) == 1))

struct	FDATA	{
	int	fd_size;	/* Blocks per diskette */
	int	fd_nhds;	/* Heads per drive */
	int	fd_trks;	/* Tracks per side */
	int	fd_offs;	/* Sector base */
	int	fd_nspt;	/* Sectors per track */
	char	fd_GPL[4];	/* Controller gap param (indexed by rate) */
	char	fd_N;		/* Controller size param */
	char	fd_FGPL;	/* Format gap length */
};

struct	FRATES	{
	char	fl_hi_kind;		/* "fdata" initial try for hi dens. */
	char	fl_hi_rate;		/* -1 here for lo-dens. 	    */
	char	fl_lo_kind;		/* Lo-dens "fdata" entry to try 1st.*/
	char	fl_lo_rate;		/* Proper lo-density rate for type. */
	char	dflt_kind;		/* Default parameters.		    */
	char	dflt_rate;		/* Default data rate.		    */
};

#define FL_NUM_DRV_STAT		2

struct	FL	{
	BUF	* fl_actf;		/* Queue, forward */
	BUF	* fl_actl;		/* Queue, backward */
	paddr_t	fl_addr;		/* Address */
	int	fl_nsec;		/* # of sectors */
	int	fl_secn;		/* Current sector */
	struct	FDATA fl_fd [MAXDRVS];	/* Disk kind data */
	int	fl_fcyl;		/* Floppy cylinder # */
	int	fl_2step [MAXDRVS];	/* = 1 for double-stepping */
	char	fl_incal [MAXDRVS];	/* Disk in cal flags and current cyl */
	char	fl_dsk_chngd [MAXDRVS];	/* Diskette changed flags */
	char	fl_ndsk;		/* # of drives */
	char	fl_unit;		/* Current unit # */
	char	fl_selected_unit;	/* Last unit selected */
	char	fl_mask;		/* Handy unit mask */
	char	fl_hbyh;		/* 0/1 = Side by side/Head by head */
	char	fl_nerr;		/* Error count */
	int	fl_ndrvstat;		/* Number of drv status bytes read */
	char	fl_drvstat [FL_NUM_DRV_STAT];	/* Drive Status buffer */
	int	fl_fsec;		/* Floppy sector # */
	int	fl_head;		/* Floppy head */
	char	fl_state;		/* Processing state */
	char	fl_mstatus;		/* Motor status */
	char	fl_time [MAXDRVS];	/* Motor timeout */
	char	fl_rate [MAXDRVS];	/* Data rate: 500, 300, 250,? kbps */
	char	fl_type [MAXDRVS];	/* Type of drive: 2 = HiCap */
	char	fl_rate_set;		/* Currently set data rate */
	int	fl_wflag;		/* Write operation  */
	int	fl_recov;		/* Recovery initiated */
	int	fl_opct [MAXDRVS];	/* open count for each unit */
	int	fl_we [MAXDRVS];	/* write enable for each unit */
};

static void	flfsm ();

static void	flQhang ();
static void	fldelay ();
static void	fldone ();
static int	flrate ();
static void	flrecov ();
static void	flIntHandler ();
static void	flDrvSelect ();
static void	flDrvStatus ();

/* Configurable variables */

extern int 	fl_dsk_ch_prob;
extern int	fl_auto_parm;
extern int	fl_disp;

int	jopen;

/*
Here's the problem. We need to be able to tell if the disk has been changed.
There is an i/o port we can read, but on some systems, we can get constant
false positives. This causes massive floppy slowdown as the disk is constantly
recalibrating. The solution is not completely satisfactory, but it's the best
one I could come up with. Basically, what I said was since we can't tell when
the disk has changed, we will act as if it has changed every time we do an
open or a reset. The code

		if (fl_dsk_ch_prob)
			jopen = 2;

indicates the need to pretend that the disk has changed. It is set to 2 since
there are two parts to the change procedure. Additional code dependent on
the value of fl_dsk_ch_prob says that if we have not just down an open, then
we should skip the recal.  Otherwise, decrement the counter, and do the
recal. - mlk */

/* Parameters for each kind of format */
struct	FDATA fdata [] = {
/* 8 sectors per track, surface by surface seek. */
	{  320, 1, 40, 0, 8, { 0x00, 0x20, 0x20 }, 2, 0x58 }, /* Single sided */
	{  640, 2, 40, 0, 8, { 0x00, 0x20, 0x20 }, 2, 0x58 }, /* Double sided */
	{ 1280, 2, 80, 0, 8, { 0x00, 0x20, 0x20 }, 2, 0x58 }, /* Quad density */
/* 9 sectors per track, surface by surface seek. */
	{  360, 1, 40, 0, 9, { 0x00, 0x20, 0x20 }, 2, 0x50 }, /* Single sided */
	{  720, 2, 40, 0, 9, { 0x00, 0x20, 0x20 }, 2, 0x50 }, /* Double sided */
	{ 1440, 2, 80, 0, 9, { 0x00, 0x20, 0x20 }, 2, 0x50 }, /* Quad density */
/* 15 sectors per track, surface by surface seek. */
	{ 2400, 2, 80, 0, 15, { 0x1B, 0x00, 0x00 }, 2, 0x54 }, /* High capacity */
/* 18 sectors per track, surface by surface seek. */
	{ 2880, 2, 80, 0, 18, { 0x1B, 0x00, 0x00 }, 2, 0x6C }	/* 1.44 3.5" */
};

/* Parameters for each device type */
struct	FRATES frates [] =	{
	{ 4,-1, 4,-1, 4, FDC_RATE_250K },	/* Type 0 = no drive */
	{ 4,-1, 4, 2, 4, FDC_RATE_250K },	/* Type 1 = 360K  */
	{ 6, 0, 4, 1, 6, FDC_RATE_500K },	/* Type 2 = 1.2M  */
	{ 4,-1, 5, 2, 5, FDC_RATE_250K },	/* Type 3 = 720K  */
	{ 7, 0, 5, 2, 7, FDC_RATE_500K }	/* Type 4 = 1.44M */
};

struct FL fl;

/*
 *	We need some areas in global RAM to use as BUF structures
 *	and as data areas for special functions such as formatting,
 *	reading the drive status and reading sector IDs.  There is
 *	one set for each drive.  When the blocks for a drive are in
 *	use, the "drv_locked" char is set to non-zero, and is
 *	cleared otherwise.  While this scheme doesn't provide
 *	complete reentrancy, it does allow both drives to be used
 *	"at once" by separate tasks.
 */
char	drv_locked [MAXDRVS];	/* One for each possible drive */
char	sw3 [MAXDRVS];
BUF     flbuf [MAXDRVS];

/*
 *	These next items are related to the floppy disk system
 *	in general, so we only need one of each.
 */

TIM	fltim;
TIM	fldmalck;	/* DMA lock deferred function structure. */
char	fl_clrng_cd;
char	fl_intlv_ct,	/* Counts sectors to find interleave.	 */
	fl_get_intlv,	/* = 1 to start search for interleave.	 */
	fl_lk4_id,	/* Sector ID to look for for interleave. */
	fl_alt_kind,	/* Alternate disk parameter index and	 */
	fl_alt_rate,	/* data rate to use if not first value.  */
	fl_1st_ID,	/* Detects when all track sectors scanned */
	fl_hi_ID;	/* Highest sector ID read so far.	 */

/*
 * Patchable parameters.
 */

int	fl_srt = 0xD;	/* Floppy seek step rate, in unit -2 millisec */
			/* F = 1ms, E = 2ms, etc. */
			/* All drives HAVE to work at 5 msec/step, so */
			/* they HAVE to work at 6 msec/step!  (The use of */
			/* 8 msec/step in the old PCs was just IBM being */
			/* excessively conservative. MS/PC-DOS, starting with */
			/* version 2.0, changes it to 6 msec/step.  There */
			/* are, in fact, lots of drives that will step at */
			/* 4 msec/step, but there's no sense in pushing it.) */
int	fl_hlt = 25;	/* Floppy head load time, in unit 4 millisec */
int	fl_hut = 0xF;	/* Floppy head unload time, in unit 32 millisec */

static int flOpenCount;		/* Number of pending opens for this driver. */


/*
 * The load routine asks the
 * switches how many drives are present
 * in the machine, and sets up the field
 * in the floppy database. It also grabs
 * the level 6 interrupt vector.
 */

#if __USE_PROTO__
static void flload (void)
#else
static void
flload ()
#endif
{
	register int	eflag;
	register int	s, t;

#ifdef EMBEDDED_VERSION
	printf("MWC Floppy Driver: %s\n", MWC_FL386_VERSION);
#endif

	/* set up interrupt handling routine */
	flIntr = flIntHandler;
	fl_clrng_cd = 0;

	/*
	 * Read floppy equipment byte from CMOS ram
	 *	drive 0 is in high nibble, drive 1 is in low nibble.
	 */
	eflag = read_cmos (0x10);

	/*
	 * Define AT drive information.
	 */
	fl.fl_type [0] = eflag >> 4;
	fl.fl_type [1] = eflag & 15;

	/*
	 * Pretend no drives -- we haven't found any yet! 
	 */
	fl.fl_ndsk = 0;

	/*
	 * Bang on all possible drives, setting startup vals
	 */
	for (s = 0; s < MAXDRVS; s ++) {
		drv_locked [s] = 0;		/* not locked */
		fl.fl_dsk_chngd [s] = 1;		/* disk has been changed */
		fl.fl_incal [s] = -1;		/* not in calibration */
		t = fl.fl_type [s];		/* type of drive */
		if (t > MAXTYPE)		  /* --in case we get, like, */
			fl.fl_type [s] = t = MAXTYPE;	/* a 2.88 meg drive. */
		fl.fl_rate [s] = frates [t].dflt_rate;	/* default rate */
		fl.fl_fd [s] = fdata [frates [t].dflt_kind]; /* data params */
		if (t) fl.fl_ndsk = s + 1;	  /* Type 0 = no drive. */
	}

	fl.fl_rate_set = 0;

	if (fl_dsk_ch_prob)
		jopen = 1;

	fl.fl_state = SIDLE;		/* Initial state of IDLE */

	fl.fl_mstatus = 0;		/* No motors on */
	fl.fl_selected_unit = -1;	/* No unit selected */
}


/*
 * Release resources.
 */
#if __USE_PROTO__
static void flunload (void)
#else
static void
flunload ()
#endif
{
	/*
	 * Cancel timed function.
	 */
	timeout (& fltim, 0, NULL, NULL);

	/*
	 * Free the interrupt routine
	 */
	flIntr = NULL;
}


/*
 * The open routine screens out
 * opens of illegal minor devices and
 * performs the NEC specify command if
 * this is the very first floppy disk
 * open call.
 *
 * NOTICE:  This routine needs to be fixed!  There is a problem
 * if first open is in progress and another first open is tried.
 * hws - 93/05/18
 */
#if __USE_PROTO__
static void flopen (o_dev_t dev, int mode,
  int __NOTUSED(_flags), __cred_t * __NOTUSED(_credp))
#else
static void
flopen (dev, mode)
o_dev_t dev;
int	mode;
#endif
{
	int unit_number = funit (dev);
	int s;

	/*
	 * If first open, seize interrupt handler.
	 * Also bang on the FDC to get it warmed up -
	 * specify seek, load, and unload, and set rate.
	 */
	if (flOpenCount == 0) {
		if (! setFlIntr (1))  {
			devmsg (dev, "FDC busy.");
			set_user_error (EBUSY);
			goto worseFlopen;
		}
		fdcSpecify (fl_srt, fl_hut, fl_hlt);
		fdcRate (fl.fl_rate_set);
	}

	/*
	 * Validate existence and data rate (Gap length != 0).
	 */
	if (unit_number >= fl.fl_ndsk ||		/* not over MAX drive */
	    fl.fl_type [unit_number] == 0 ||		/* existence */
	    fdata [fkind (dev)].fd_GPL [flrate (dev)] == 0) {/* gap len == 0 */
		set_user_error (ENXIO);
		goto badFlopen;		
	}

	if (fl_dsk_ch_prob)
		jopen = 1;

	/*
	 * May need to write - see if diskette is write proteced.
	 * We do this with a "Sense Drive Status" command.  Since
	 * this requires the use of the FDC, we have to schedule it
	 * like data transfer I/O or FORMAT even though it doesn't
	 * use the DMA.
	 */
	if (fl.fl_opct [unit_number] == 0) {	/* first open for this floppy */
		if (drv_locked [unit_number]) {	/* Work areas avail? */
			set_user_error (EBUSY);		/* No. */
			goto badFlopen;
		}

		drv_locked [unit_number] = 1;	/* Grab work areas. */
		flbuf [unit_number].b_dev = dev;
		flbuf [unit_number].b_req = BFLSTAT;
		sw3 [unit_number] = 0;
							/* Get drive status. */
		/* Queue the request up */
		flQhang (& flbuf [unit_number]);

		for (;;) {
			/*
			 * Enter the state machine and
			 * execute the commands pending.
			 */
			s = sphi ();
			if (fl.fl_state == SIDLE)
				flfsm ();
			spl (s);

			/*
			 * if we have a result, exit the loop
			 */
			if (sw3 [unit_number])
				break;

			/*
			 * Otherwise, if the drive is still
			 * busy, go to sleep.
			 */
				
			if (fl.fl_state != SIDLE)
				x_sleep (& fl.fl_state,
				  pridisk, slpriSigCatch, "flopen");

			/* 
			 * What woke us, a signal or the result?
			 * if signal, trap it here.  Otherwise
			 * go back up to the top where we will
			 * bang on the drive again.
			 */

			if (curr_signal_pending ()) {  /* signal? */
				set_user_error (EINTR);
				drv_locked [unit_number] = 0;
				goto badFlopen;
			}
		}

		if (flbuf [unit_number].b_resid != 0) {
			set_user_error (EIO);	/* Couldn't get drive */
			drv_locked [unit_number] = 0;
			goto badFlopen;	
		}

		/* The payoff - set write enable status. */
		fl.fl_we [unit_number] = (sw3 [unit_number] & ST3_WP) == 0;
		drv_locked [unit_number] = 0;	/* Release work areas */

		/*
		 * If the drive is low density (no change line) we should
		 * flag the need to verify the disk format and density.
		 * High density drives (which are also dual density) have
		 * change lines that we can check each time we want to read
		 * the drive.
		 */

		if (frates [fl.fl_type [unit_number]].fl_hi_rate == -1) {
			fl.fl_incal [unit_number] = -1;
			fl.fl_dsk_chngd [unit_number] = 1;

			if (fl_dsk_ch_prob)
				jopen = 1;
		}
	}	/* end of first open stuff */

	/* If opening for write, volume must be write enabled. */
	if ((mode & IPW) != 0 && ! fl.fl_we [unit_number]) {
		printf ("fd%d: <Write Protected>\n", fl.fl_unit);
		set_user_error (EROFS);	/* Diskette write */
		goto badFlopen;		/* protected. */
	}

	fl.fl_opct [unit_number]++;
	flOpenCount ++;
	return;

badFlopen:
	setFlIntr (0);
worseFlopen:
	return;
}


/*
 * flclose ()
 */

#if __USE_PROTO__
static void flclose (o_dev_t dev, int mode,
  int __NOTUSED(_flags), __cred_t * __NOTUSED(_credp))
#else
static void
flclose (dev, mode)
o_dev_t	dev;
int	mode;
#endif
{
	int unit_number = funit (dev);
/*
 * Not sure it needed changing, but protect against negative
 * close count - ljg
 */
	if (-- fl.fl_opct [unit_number] < 0)
		fl.fl_opct [unit_number] = 0;
	if (-- flOpenCount == 0)
		setFlIntr (0);
}


/*
 * The read routine just calls
 * off to the common raw I/O processing
 * code, using a static buffer header in
 * the driver.
 */

#if __USE_PROTO__
static void flread (o_dev_t dev, IO* iop,
  __cred_t * __NOTUSED(_credp))
#else
static void
flread (dev, iop)
o_dev_t	dev;
IO	* iop;
#endif
{
	dmareq (NULL, iop, dev, BREAD);
}


/*
 * The write routine is just like the
 * read routine, except that the function code
 * is write instead of read.
 */

#if __USE_PROTO__
void flwrite(o_dev_t dev, IO * iop,
  __cred_t * __NOTUSED(_credp))
#else
static void
flwrite (dev, iop)
o_dev_t	dev;
IO	* iop;
#endif
{
	dmareq (NULL, iop, dev, BWRITE);
}


/*
 * The ioctl routine simply queues a format request.
 * The only valid command is to format a track.
 * The parameter block contains the header records supplied to the controller.
 */
#if __USE_PROTO__
static void flioctl(o_dev_t dev, int com, __VOID__ * par,
  int __NOTUSED(_mode), __cred_t * __NOTUSED(_credp),
  int * __NOTUSED(_rvalp))
#else
static void
flioctl (dev, com, par)
o_dev_t	dev;
int	com;
__VOID__ * par;
#endif
{
	unsigned s;
	struct FDATA * fdp;
	unsigned hd, cyl;
	IO		io;

	if (com != FDFORMAT) {
		set_user_error (EINVAL);
		return;
	}
	
	/*
	 * We do not allow formatting of special devices.
	 * Possible future mod.
	 */
	
	if (fisspecial (dev)) {
		set_user_error (EINVAL);
		return;
	}


	fdp = & fdata [fkind (dev)];		/* Locate formatting	*/
	cyl = getubd (par);			/* parameters.		*/
	hd  = getubd (par + 1);

	if (hd > 1 || cyl >= fdp->fd_trks) {
		set_user_error (EINVAL);
		return;
	}

	/*
	 * The following may need some explanation.
	 * dmareq will:
	 *	claim the buffer,
	 *	bounds check the parameter buffer,
	 *	lock the parameter buffer in memory,
	 *	convert io_seek to b_bno,
	 *	dispatch the request,
	 *	wait for completion,
	 *	and unlock the parameter buffer.
	 * The b_bno is reconverted to hd, cyl in flfsm.
	 */

	/*
	 * Convert to seek number based on whether or not we are running
	 * sbys or hbyh.
	 * Now we are always running hbyh, so this gets commented out
	 * and should eventually be removed. -louis 7/93
	 */

	/* s = fhbyh (dev) ? (cyl * fdp->fd_nhds + hd) : (hd * fdp->fd_trks + cyl); */
	s = (cyl * fdp->fd_nhds + hd);
	s *= fdp->fd_nspt;

	/*
	 * Build the device I/O request.
	 */

	io.io_seg = IOUSR;
	io.io_seek = (long) s * BSIZE;
	io.io.vbase = par;
	io.io_ioc = fdp->fd_nspt * 4;

	/*
	 * Hand off to the dmareq routine (i.e., indirectly call 
	 * flblock () letting dmareq () do all the dirty work).
	 */

	dmareq (NULL, & io, dev, BFLFMT);
}


/*
 * Start up block I/O on a
 * buffer. Check that the block number
 * is not out of range, given the style of
 * the disk. Put the buffer header into the
 * device queue. Start up the disk if the
 * device is idle.
 * 
 * The range checking logic has been perturbed for some time.
 * Now, hopefully, it is correct.
 */

static void
flblock (bp)
BUF	* bp;
{
	int	s;
	unsigned bno;

	/*
	 * Compute the ending block of the transfer.
	 */

	bno = bp->b_bno + (bp->b_count / BSIZE) - 1;

{ /* DEBUG */
	int first = bp->b_bno, last = bno;
	int fdatasz = fdata [fkind (bp->b_dev)].fd_size;
	int fl_fdsz = fl.fl_fd [funit (bp->b_dev)].fd_size;
/* DEBUG */

	/*
	 * If we're formatting, make sure that the starting block
	 * number is within range for the device...the range is the
	 * entire device range.
	*/

	if (bp->b_req == BFLFMT &&
	    (unsigned) bp->b_bno >= fdata [fkind (bp->b_dev)].fd_size) {
		bioerror(bp, ENXIO);
		bdone (bp);
		return;
	}


/*
 * Here, we are checking the current setttings for each device 
 * rather than the defaults for the type of device.  This is for
 * a read/write rather than a format.
 *
 * Quickly hacked to check on special devices that skip cylinder 0
 * since they are missing a cylinder's worth of sectors.
 *
 * Now, to clean it up, the specification is this:
 * 
 * If it's a read:
 *	if a read is made at the end of the device, then
 *	a count of 0 bytes done is returned *without* an error.
 *
 *	if a read is made *past* the end of the device, then
 *	the error is set to ENXIO.
 *
 * If it's a write:
 *	A write *at* or *past* the end of device results in ENXIO.
 */
	if (bp->b_req != BFLFMT) {

		daddr_t start_req_blk;
		daddr_t end_dev_blk;

		end_dev_blk = fl.fl_fd[funit(bp->b_dev)].fd_size;

		/*
		 * Check starting block to see if in range 
		 */

		start_req_blk = (unsigned)bp->b_bno + (fskip_cyl_0(bp->b_dev) ? \
		  fl.fl_fd[funit(bp->b_dev)].fd_nspt * 2 : 0);
		
		if (start_req_blk >= end_dev_blk) {
		  
			/*
			 * If this is a read at the end of device
			 * on a the device, there is no error
			 * Just make it so that no bytes were read.
			 */
			if((bp->b_flags & B_READ) && (start_req_blk == end_dev_blk)) {
				bp->b_resid = bp->b_bcount;
				bioerror(bp, 0);
				bdone(bp);
				return;
			}

			/*
			 * In all other cases, it is ENXIO.
			 */

			bioerror(bp, ENXIO);
			bdone(bp);
			return;
		}

		/*
		 * Here we check the ending number to see if that
		 * is within range.
		 */

		start_req_blk = bno + (fskip_cyl_0(bp->b_dev) ? \
		  fl.fl_fd[funit(bp->b_dev)].fd_nspt * 2 : 0);

			
		if (start_req_blk >= end_dev_blk) {
			/*
			 * If this is a read at the end of device
			 * on a the device, there is no error
			 * Just make it so that no bytes were read.
			 */
			if((bp->b_flags & B_READ) && (start_req_blk == end_dev_blk)) {
				bp->b_resid = bp->b_bcount;
				bioerror(bp, 0);
				bdone(bp);
				return;
			}

			/*
			 * In all other cases, it is ENXIO.
			 */

			bioerror(bp, ENXIO);
			bdone(bp);
			return;
			
		}

		/*
		 * Finally, make sure the byte count is a multiple
		 * of 512 (the number of bytes per physical sector).
		 */
		if ((bp->b_count & 0x1FF) != 0) {
			bioerror(bp, EINVAL);
			bdone (bp);
			return;
		}
	}
} /* DEBUG */

	flQhang (bp);			/* Put the block in the queue. */

	s = sphi ();
	if (fl.fl_state == SIDLE)	/* --if necessary, to */
		flfsm ();		/* get things moving. */
	spl (s);
}

/******************************************************************/

/* Lower-level functions needed by CON entry points. */

/*
 * This routine hangs a BUF in the processing queue
 */

static void
flQhang (bp)
register BUF * bp;
{
	register int s = sphi ();     /* No interrupts during chaining, please */

	bp->b_actf = NULL;

	if (fl.fl_actf == NULL)
		fl.fl_actf = bp;
	else
		fl.fl_actl->b_actf = bp;

	fl.fl_actl = bp;

	spl (s);
}


/*
 * Remove all pending requests for a device from the queue
 * (used after errors).
 *
 * Further changed to correctly update error status using bioerror().
 * The old method is #if0'ed out.
 */

static void
clrQ (dev)
dev_t dev;
{
	BUF * bp, * bp2;
	int s;

	s = sphi ();

	while ((bp = fl.fl_actf) != NULL && bp->b_dev == dev) {
		bioerror(bp, EIO);
#if 0
		bp->b_flag |= BFERR;		/* Strip BUFs from front */
#endif
		fl.fl_actf = bp->b_actf;	/* of queue.		 */
		bdone (bp);
	}
	while (bp) {
		fl.fl_actl = bp;
		if ((bp2 = bp->b_actf) != NULL && bp2->b_dev == dev) {
			bioerror(bp2, EIO);
#if 0
			bp2->b_flag |= BFERR;	  /* Strip BUFs from rest  */
#endif
			bp->b_actf = bp2->b_actf; /* rest of queue.	   */
			bdone (bp2);
		} else
			bp = bp2;
	}
	fl.fl_state = SIDLE;
	wakeup (& fl.fl_state);
	spl (s);
}


/*
 * This finite state machine is
 * responsible for all sequencing on the disk.
 * It builds the commands, does the seeks, spins up
 * the drive motor for 1 second on the first call,
 * and so on.
 * Note that the format command is rather obscurely shoehorned into this.
 */

void
flfsm ()
{
	register BUF	* bp;
	register int	flcmd;
	register int	i;
	int		dods;	/* for PS/1, do disk swap */

again:
	/*
	 * This gets the head of the floppy request queue.
	 */

	bp = fl.fl_actf;

	switch (fl.fl_state) {

	case SIDLE:
T_HAL (0x40000, printf ("SIDLE "));
		/*
		 * Set the timer for floppy access.
		 */
		setFlTimer (1);

		/* 
		 * One way out completely is if there are no more
		 * I/O requests and we are in an IDLE state.
		 */

		if (bp == NULL)
			break;

		fl.fl_unit = funit (bp->b_dev);
		fl.fl_mask = 0x10 << fl.fl_unit;

#if 0
printf ("drv%d: cmd =%d (%s), position =%d, count =%d\n",
fl.fl_unit,
bp->b_req,
  (bp->b_req == BREAD)	     ? "BREAD"
: (bp->b_req == BWRITE)      ? "BWRITE"
: (bp->b_req == BFLSTAT)     ? "BFLSTAT"
: (bp->b_req == BFLFMT)      ? "BFLFMT"       : "?????",
bp->b_bno,
bp->b_count);
#endif
		/*
		 * We do an entire check for drive status here
		 */
		if (bp->b_req == BFLSTAT) {
			/*
			 * Clear status buffer and get new status.
			 */
			fl.fl_drvstat [0] = 0;
			flDrvStatus ();

			/*
			 * Why "| 3" ? -louis
			 */
			sw3 [fl.fl_unit] = fl.fl_drvstat [0] | 3;
			bp->b_resid = (fl.fl_ndrvstat == 1) ? 0 : 1;

			/*
			 * set up to service next request.
			 * and service it immediately by goto again.
			 */
			fl.fl_actf = bp->b_actf;
			fl.fl_state = SIDLE;
			goto again;
		}

		/*
		 * Only service hbyh now.  Original rval computation
		 * in comment.  Should remove eventually. - ljg 7/93
		 */
		fl.fl_hbyh = 1; /* fhbyh (bp->b_dev); */

		/*
		 * Get physical address and starting postition.
		 * Also set timeout value to zero.
		 */
		fl.fl_addr = bp->b_paddr;
		fl.fl_secn = bp->b_bno;
		fl.fl_time [fl.fl_unit] = 0;

		/*
		 * Make sure we read at least one sector.
		 * Even if the request is for say 20 bytes.
		 */
		if ((fl.fl_nsec = bp->b_count >> 9) == 0)
			fl.fl_nsec = 1;

		fl.fl_nerr = 0;

		/*
		 * Motor is turned off - turn it on, wait 1 second
		 * (for write operations only)
		 * flDrvSelect () is who selects the drive and turns
		 * the motor on.
		 */
		if ((fl.fl_mstatus & fl.fl_mask) == 0 ||
		    fl.fl_unit != fl.fl_selected_unit) {
			flDrvSelect ();
			if (bp->b_req == BWRITE || bp->b_req == BFLFMT) {
				timeout (& fltim, HZ, fldelay, SSEEK);
				fl.fl_state = SDELAY;
				break;
			}
		}

		/* FALL THROUGH */
		/* The "fsm" is fairly vile in gotos, fallthroughs,
		 * and other hazy notions of jumping from state to
		 * state without following a true transition edge.
		 * This is one example.
		 */

	case SSEEK:
T_HAL (0x40000, printf ("SSEEK "));

		/* In a fallthrough, this line would be a waste. */
		flDrvSelect ();			/* Keep drive turned on */

		/*
		 * Test dual-density drive's disk changed line.  We must
		 * test now before we (possibly) recalibrate the drive
		 * which would lose us the disk changed indication.
		 */

		if (frates [fl.fl_type [fl.fl_unit]].fl_hi_rate != -1 &&
		    (inb (FDCCHGL) & DSKCHGD) != 0 && fl_clrng_cd == 0) {
			/* See note at def of fl_dsk_ch_prob above */

			/*
			 * Hmm.  I would have done this:
			 * if ((FL_DSK_PROB && jopen) || !(FL_DSK_PROB))
			 * instead of multiple if's, but that's me.
			 * -louis
			 */
			if (fl_dsk_ch_prob) {
				if (jopen) {
					jopen--;
					fl.fl_dsk_chngd [fl.fl_unit] = 1;
					fl.fl_incal [fl.fl_unit] = -1;
				}
			} else {
				fl.fl_dsk_chngd [fl.fl_unit] = 1;
				fl.fl_incal [fl.fl_unit] = -1;
			}
		}

		fl_clrng_cd = 0;

		/*
		 * If we have a format command on cylinder zero, head
		 * zero, we must recalibrate the drive first, and set
		 * up the transfer speed and FDC stuff.  We ignore
		 * a disk changed condition since the current format
		 * (it may, remember, be unformatted!) is of no
		 * consequence.
		 */

		/* NOTE:
		 * Since formats to special devices are not currently
		 * allowed, this code should never be reached when
		 * the device is special.  Therefore, if formats
		 * to special devices are allowed later, this code
		 * may need to be updated.
		 */
		if (bp->b_req == BFLFMT) {
			/*
			 * By making disk changed 0 we can avoid
			 * figuring out the type of disk since we
			 * don't care -- we're formatting (it may
			 * be a 1.2M that we're formatting to 360K
			 * anyhow.
			 */
			fl.fl_dsk_chngd [fl.fl_unit] = 0;
	
			/*
			 * Here we just set up params for the
			 * current drive in normal tables so 
			 * we don't need to index the current unit
			 * any more.
			 */
			fl.fl_fd [fl.fl_unit] = fdata [fkind (bp->b_dev)];
			fl.fl_rate [fl.fl_unit] =
			fl.fl_rate_set		 = flrate (bp->b_dev);

			/*
			 * If less than 45 tracks and we're not a 360K
			 * drive, then we need to double step
			 * the drive.
			 */
			if (fl.fl_fd [fl.fl_unit].fd_trks < 45 &&
			    fl.fl_type [fl.fl_unit] != 1)
				fl.fl_2step [fl.fl_unit] = 1;

			fdcRate (fl.fl_rate_set);
			if (fl.fl_secn == 0)
				fl.fl_incal [fl.fl_unit] = -1;
		}
		/*
		 * Drive is not calibrated - seek to track 0.
		 */

		if (fl.fl_incal [fl.fl_unit] == -1) {
#if NO_RECAL1
/*
 * Some FDC/FD pairs get stuck if we do a recal instead of a seek
 * here, after reading out to block 2367 on a 5-1/4", then mounting
 * a new diskette.
 */
			fdcSeek (fl.fl_unit, 0, 0);
#else
			fdcRecal (fl.fl_unit);
#endif
			fl.fl_state = SRECAL1;
		} else
			goto Recalibrated;
		break;

	case SRECAL1:
T_HAL (0x40000, printf ("SRECAL1 "));
		/*
		 * If the recalibrate had to step more than 77 cylinders
		 * it will fail.  We must check for this condition and
		 * try once more.  With some controllers we will also get
		 * an error if the head STARTS over cylinder 0.  In either
		 * event we will force a seek to track 2, then recalibrate
		 * again.  If this fails, we can't recalibrate the drive.
		 */
		if (fdc.fdc_nintstat != 2 ||
		    (fdc.fdc_intstat [0] & (ST0_IC | ST0_SE)) != ST0_SE) {

			/* Seek to current drive #, head 0, cylinder 2. */
			fdcSeek (fl.fl_unit, 0, 2);

			fl.fl_state = SRECAL2;
			break;
		} else
			goto RecalibrateOK;

	case SRECAL2:
T_HAL (0x40000, printf ("SRECAL2 "));
#if NO_RECAL2
		fdcSeek (fl.fl_unit, 0, 0);
#else
		fdcRecal (fl.fl_unit);
#endif
		fl.fl_state = SRECAL3;
		break;

	case SRECAL3:
T_HAL (0x40000, printf ("SRECAL3 "));
		if (fdc.fdc_nintstat != 2 ||
		    (fdc.fdc_intstat [0] & (ST0_IC | ST0_SE)) != ST0_SE) {
RecalFailed:
			printf ("fd%d: <Can't Recalibrate>\n", fl.fl_unit);
			clrQ (bp->b_dev);
			goto again;
		}
RecalibrateOK:
		/* We now get off of cyl 0  */
		/* to try to clear the disk */
		/* changed line, which acts */
		/* differently on different */
		/* controllers.  <sigh>  We */
		/* use cyl 2 since all for- */
		/* matted disks will have a */
		/* track here.		    */

		/* Seek to current drive #, head 0, cylinder 2. */
		fdcSeek (fl.fl_unit, 0, 2);

		fl.fl_state = SGOTO2;
		break;

	case SGOTO2:
T_HAL (0x40000, printf ("SGOTO2 "));
		if (fdc.fdc_nintstat != 2 ||
		     (fdc.fdc_intstat [0] & (ST0_IC | ST0_SE)) != ST0_SE)
			goto RecalFailed;

		fl.fl_incal [fl.fl_unit] = 2;	/* Heads now on cylinder 2. */

Recalibrated:
		/*
		 * Now, if we don't have to check the interleave factor,
		 * we can continue with the seek!
		 */
		if (fl.fl_dsk_chngd [fl.fl_unit] == 0)
			goto RateKnown;

		/*
		 * <sigh>.  Okay, first we'll try the requested density.
		 */

						/* First we'll make sure   */
						/* we're sitting on cyl 2. */
		if (fl.fl_incal [fl.fl_unit] != 2)
			goto RecalibrateOK;

		/*
		 * We start by trying the requested density:
		 */

							/* Get requested rate.*/
		i = fl.fl_rate [fl.fl_unit] = flrate (bp->b_dev);
							/* This next mess gets */
							/* the disk parameters */
							/* and the alternate  */
							/* values.	      */
		/*
		 * Auto_parm defaults to active now, and is used
		 * for the appropriate minor devices.  It can be 
		 * patched to 0 if it causes problems with someone's
		 * floppy, but then the iBCS2 devices that autosense
		 * won't autosense anymore :-( -louis
		 */

		if (i == frates [fl.fl_type [fl.fl_unit]].fl_hi_rate) {
			fl.fl_fd [fl.fl_unit] =
			 fdata [frates [fl.fl_type [fl.fl_unit]].fl_hi_kind];
                        if (fl_auto_parm && fautosense (bp->b_dev)) {
				fl_alt_kind =
				  frates [fl.fl_type [fl.fl_unit]].fl_lo_kind;
				fl_alt_rate =
				  frates [fl.fl_type [fl.fl_unit]].fl_lo_rate;
			} else {
				fl_alt_kind =
				  frates [fl.fl_type [fl.fl_unit]].fl_hi_kind;
				fl_alt_rate =
				    frates [fl.fl_type [fl.fl_unit]].fl_hi_rate;
			}
		} else {
			fl.fl_fd [fl.fl_unit] =
			 fdata [frates [fl.fl_type [fl.fl_unit]].fl_lo_kind];
                        if (fl_auto_parm && fautosense (bp->b_dev)) {
				fl_alt_kind =
				  frates [fl.fl_type [fl.fl_unit]].fl_hi_kind;
				fl_alt_rate =
				    frates [fl.fl_type [fl.fl_unit]].fl_hi_rate;
			} else {
				fl_alt_kind =
				  frates [fl.fl_type [fl.fl_unit]].fl_lo_kind;
				fl_alt_rate =
				  frates [fl.fl_type [fl.fl_unit]].fl_lo_rate;
			}
		}

		fl.fl_state = SRDID;		/* Set up to read sector IDs. */
		fl_get_intlv = 1;
		fl_intlv_ct = fl_lk4_id = fl_1st_ID = fl_hi_ID = 0;

		/*
		 * Now we try the rate to see if we can read sector IDs
		 */
TryRate:
		if (fl.fl_rate_set != i) {
			fl.fl_rate_set = i;
			fdcRate (fl.fl_rate_set);
		}
GetNextID:
		/* Always read side 0. */
		fdcReadID (fl.fl_unit, 0);

		break;				/* Wait for ID to arrive. */

	case SRDID:
T_HAL (0x40000, printf ("SRDID "));

		if (fdc.fdc_ncmdstat < 7 ||	/* Did we get an ID? */
		    (fdc.fdc_cmdstat [0] & ST0_IC) != ST0_NT) {
			if (fl_alt_rate == -1) { /* No, is there an alternate?*/
				fdcStatus ();	 /* No, we can't go on.       */
				clrQ (bp->b_dev);
				goto again;
			} else {
				fl.fl_fd [fl.fl_unit] = fdata [fl_alt_kind];
				i = fl.fl_rate [fl.fl_unit] = fl_alt_rate;
				fl_alt_rate = -1; /* Flag tried alternate.    */
				goto TryRate;	  /* Try alternate density.   */
			}
		}

		/*
		 * Test interleave
		 */

		if (fl_get_intlv)		/* Looking for interleave? */
			if (fl_lk4_id) {	/* Yes; started yet?	   */
				if (fl_lk4_id == fdc.fdc_cmdstat [5]) /* Yes. */
					fl_get_intlv = 0; /* We have a hit.*/
				else			  /* No hit yet,   */
					fl_intlv_ct ++;	  /* count sector. */
			} else if (fdc.fdc_cmdstat [5] < 5) {/* Can we start yet?*/
				fl_intlv_ct = 1;	     /* Yes; count, */
				fl_lk4_id = fdc.fdc_cmdstat [5] + 1;/* set ID  */
			}					 /* to find.*/

		/*
		 * Look for highest ID on track
		 */

		if (fdc.fdc_cmdstat [5] != fl_1st_ID) {
			if (fl_1st_ID == 0)
				fl_1st_ID = fdc.fdc_cmdstat [5];
			if (fdc.fdc_cmdstat [5] > fl_hi_ID)
				fl_hi_ID = fdc.fdc_cmdstat [5];
			goto GetNextID;
		}

		/*
		 * Be sure we have the interleave
		 */

		if (fl_get_intlv)
			goto GetNextID;

		/*
		 * So now we know the density and sectors/track
		 */

		fl.fl_dsk_chngd [fl.fl_unit] = 0;
		fl.fl_fd [fl.fl_unit].fd_nspt = fl_hi_ID;

		/*
		 * There is a problem with the approach used here -
		 * it assumes that once scan of a track starts, all
		 * sectors appear in physical order without any misses.
		 * Unfortunately, this is not always the case, especially
		 * with 1.44 M 3-1/2" drives.
		 *
		 * A workaround which fixes incorrect nspt reading appears
		 * below.
		 */
		if (fl.fl_fd [fl.fl_unit].fd_nspt > 15 &&
		    fl.fl_fd [fl.fl_unit].fd_nspt < 18)
			fl.fl_fd [fl.fl_unit].fd_nspt = 18;

		/*
		 * We're (supposedly) sitting on track 2.  We'll
		 * look at the last sector ID we've read.  If it's 1,
		 * we need to do double-stepping.
		 */

		if (fl.fl_2step [fl.fl_unit] = (fdc.fdc_cmdstat [3] == 1)) {
			fl.fl_fd [fl.fl_unit].fd_trks = FL_CYL_2STEP;
			fl.fl_incal [fl.fl_unit] = 1;
		} else					/* Most 1.2M drives */
			fl.fl_fd [fl.fl_unit].fd_trks = /* have 83 cyls!   */
			  (fl.fl_type [fl.fl_unit] == 2)
			  ? FL_CYL_HDHI : FL_CYL_HDLO;

		/*
		 * We next test for one or two sides:
		 */

		if (fl.fl_rate [fl.fl_unit] == 0) {	    /* If diskette is */
			fl.fl_fd [fl.fl_unit].fd_nhds = 2; /* high-density it */
			goto DiskEstablished;		    /* will have two  */
		}					    /* sides.	      */

		/*
		 * If the diskette is requested by caller as low density
		 * we use the specified number of sides------
		 */

		if (fdata [fkind (bp->b_dev)].fd_nspt < 12) {
			fl.fl_fd [fl.fl_unit].fd_nhds =
			  fdata [fkind (bp->b_dev)].fd_nhds;
			goto DiskEstablished;
		}

		/*
		 * --otherwise we check to see:
		 * Try to read ANY sector ID from side two.
		 */
		fdcReadID (fl.fl_unit, 1);
		fl.fl_state = SSIDTST;
		break;

	case SSIDTST:				/* If we succeeded, we have */
T_HAL (0x40000, printf ("SSIDTST "));
						/* 2 sides, else we have 1. */
		fl.fl_fd [fl.fl_unit].fd_nhds =
			(fdc.fdc_ncmdstat < 7 ||
			 (fdc.fdc_cmdstat [0] & ST0_IC) != ST0_NT) ? 1 : 2;

		/*
		 * So now we now know all about the diskette!
		 */
DiskEstablished:
		fl.fl_fd [fl.fl_unit].fd_size = fl.fl_fd [fl.fl_unit].fd_nhds
		  * fl.fl_fd [fl.fl_unit].fd_trks
		  * fl.fl_fd [fl.fl_unit].fd_nspt;

		if (fl_disp) {
			printf ("fl%d: rate=%d, sctrs/trk=%d, hds=%d, cyls=%d,"
			  " size=%d, intlv=%d, stp=%d\n",
			  fl.fl_unit,
			  fl.fl_rate [fl.fl_unit],
			  fl.fl_fd [fl.fl_unit].fd_nspt,
			  fl.fl_fd [fl.fl_unit].fd_nhds,
			  fl.fl_fd [fl.fl_unit].fd_trks,
			  fl.fl_fd [fl.fl_unit].fd_size,
			  fl_intlv_ct,
			  fl.fl_2step [fl.fl_unit]+ 1);
		}

RateKnown:
		/*
		 * Set data rate if changed.
		 */
		if (fl.fl_rate_set != (i = fl.fl_rate [fl.fl_unit])) {
			fl.fl_rate_set = i;
			fdcRate (fl.fl_rate_set);
		}

		/*
		 * Next we must convert the ordinal block number to
		 * cylinder/head/sector form.
		 */
		fl.fl_fsec = (fl.fl_secn % fl.fl_fd [fl.fl_unit].fd_nspt) + 1;

#if ALLOW_SEEK_SURFACE_BY_SURFACE
		/*
		 * Minor numbers used for surface by surface seek have
		 * been reassigned.
		 * Will keep the old code around for awile, though.
		 */

		if (fl.fl_hbyh) {
			/*
			 * Seek cylinder by cylinder (XENIX / DOS compatible).
			 */

			fl.fl_head = fl.fl_secn / fl.fl_fd [fl.fl_unit].fd_nspt;
			fl.fl_fcyl = fl.fl_head / fl.fl_fd [fl.fl_unit].fd_nhds;
			fl.fl_head = fl.fl_head % fl.fl_fd [fl.fl_unit].fd_nhds;
		} else {
			/*
			 * Seek surface by surface.
			 */

			fl.fl_fcyl = fl.fl_secn / fl.fl_fd [fl.fl_unit].fd_nspt;
			fl.fl_head = fl.fl_fcyl / fl.fl_fd [fl.fl_unit].fd_trks;
			fl.fl_fcyl = fl.fl_fcyl % fl.fl_fd [fl.fl_unit].fd_trks;
		}
#else
		/*
		 * This should always be done from now on.  Original
		 * code conditioned out.  Should eventually be
		 * removed.  louis 7/93
		 * 
		 * Also, if it is a special device that skips track 0,
		 * we just bang the cylinder number up by 1.
		 */

		fl.fl_head = fl.fl_secn / fl.fl_fd [fl.fl_unit].fd_nspt;
		fl.fl_fcyl = fl.fl_head / fl.fl_fd [fl.fl_unit].fd_nhds;
		if (fskip_cyl_0 (bp->b_dev))
			fl.fl_fcyl ++;
		fl.fl_head = fl.fl_head % fl.fl_fd [fl.fl_unit].fd_nhds;
#endif

		/* Don't seek unless we have to. */
		if (fl.fl_fcyl == fl.fl_incal [fl.fl_unit])
			goto Sought;		/* Past tense of seek. */

		fl.fl_incal [fl.fl_unit] = fl.fl_fcyl; /* Save new cylinder. */

		if (fl.fl_fd [fl.fl_unit].fd_trks < 45 &&
		    fl.fl_type [fl.fl_unit] != 1) {

			/* If disk is around 40 tracks */
			/* and drive is not 40 track- */
			/* -use double step.  */
			fdcSeek (fl.fl_unit, fl.fl_head, (fl.fl_fcyl << 1));

		} else {
			/* Single step. */
			fdcSeek (fl.fl_unit, fl.fl_head, fl.fl_fcyl);
		}

		fl.fl_state = SHDLY;
		break;

	case SHDLY:
T_HAL (0x40000, printf ("SHDLY "));
		/*
		 * Delay for minimum 15 milliseconds after seek before w/ fmt.
		 * 2 clock ticks would give 10-20 millisecond (100 Hz clock).
		 * 3 clock ticks gives	    20-30 millisecond (100 Hz clock).
		 */
		if (bp->b_req != BREAD) {
			timeout (& fltim, 3, fldelay, SRDWR);
			fl.fl_state = SDELAY;
			break;
		}
		/* FALL THROUGH */

	case SRDWR:
T_HAL (0x40000, printf ("SRDWR "));
Sought:
		/*
		 * Disable watchdog timer while waiting to lock DMA controller.
		 */
		fl.fl_time [fl.fl_unit] = -1;

		/*
		 * Next state will be DMA locked state.
		 */
		fl.fl_state = SLOCK;

		/*
		 * If DMA controller locked by someone else, exit for now.
		 */

		if (dmalock (& fldmalck, flfsm, 0) != 0)
			return;
		/* FALL THROUGH */

	case SLOCK:
T_HAL (0x40000, printf ("SLOCK "));
		/*
		 * Reset watchdog timer to restart timeout sequence.
		 */

		fl.fl_time [fl.fl_unit] = 0;

		flcmd = FDC_CMD_RDAT;
		fl.fl_wflag = 0;

		if (fl_clrng_cd == 0) {
			if (bp->b_req == BWRITE) {
				fl.fl_wflag = 1;
				flcmd = FDC_CMD_WDAT;
			} else if (bp->b_req == BFLFMT) {
				fl.fl_wflag = 1;
				flcmd = FDC_CMD_FMT;

				if (! dmaon (DMA_CH2, P2P (fl.fl_addr), bp->b_count,
				  fl.fl_wflag))
					goto straddle;
				else
					goto command;
			}
		}

		if (dmaon (DMA_CH2, P2P (fl.fl_addr), 512, fl.fl_wflag) == 0) {
straddle:
			devmsg (bp->b_dev, "fd: DMA page straddle at %x:%x",
				fl.fl_addr);
			dmaunlock (& fldmalck);
			bp->b_flag |= BFERR;
			fldone (bp);
			goto again;
		}
command:
		dmago (DMA_CH2);
		fdcPut (flcmd);
		fdcPut ((fl.fl_head <<2) | fl.fl_unit);

		if (bp->b_req == BFLFMT) {
			fdcPut (fl.fl_fd [fl.fl_unit].fd_N);	/* N */
			fdcPut (fl.fl_fd [fl.fl_unit].fd_nspt);	/* SC */
			fdcPut (fl.fl_fd [fl.fl_unit].fd_FGPL);	/* GPL */
			fdcPut (0xF6);				/* D */
		} else {
			fdcPut (fl.fl_fcyl);
			fdcPut (fl.fl_head);
			fdcPut (fl.fl_fsec);
			fdcPut (fl.fl_fd [fl.fl_unit].fd_N);	/* N */
			fdcPut (fl.fl_fd [fl.fl_unit].fd_nspt);	/* EOT */
			fdcPut (fl.fl_fd [fl.fl_unit].fd_GPL [fl.fl_rate_set]);
								/* GPL */
			fdcPut (0xFF);				/* DTL */
		}

		fl.fl_state = SENDIO;
		break;

	case SENDIO:
T_HAL (0x40000, printf ("SENDIO "));
		fl.fl_time [fl.fl_unit] = 0;
		dmaoff (DMA_CH2);
		dmaunlock (& fldmalck);

		if (fl_clrng_cd) {
			fl.fl_state = SIDLE;
			wakeup (& fl.fl_state);
			goto again;
		}

		/*
		 * We now check for errors.  If the error is a data
		 * CRC error, we KNOW we're on the correct track, and
		 * we just retry the read once before recalibrating.
		 * We recalibrate for all other errors.
		 */
		if ((fdc.fdc_cmdstat [0] & ST0_IC) != ST0_NT) {
#if ST2_DD_RECAL
			if (++ fl.fl_nerr < 5) {
				if (fdc.fdc_cmdstat [2] & ST2_DD) {
					if (fl.fl_nerr & 1)
					  goto SetSEEKState;
					else
					  goto Ask4Recal;
				} else {
Ask4Recal:
					fl.fl_incal [fl.fl_unit] = -1;
SetSEEKState:
					fl.fl_state = SSEEK;
				}
#else
			/* Retry - recal after every 3rd try. */
			fl.fl_nerr ++;
			if (fl.fl_nerr < 9) {
				if ((fl.fl_nerr % 3) == 0)
					fl.fl_incal [fl.fl_unit] = -1;
				fl.fl_state = SSEEK;
#endif
			} else {
				fdcStatus ();		/* Total failure; */
				bioerror(bp, EIO);
#if 0
				bp->b_flag |= BFERR;	/* we give up.	  */
#endif
				fldone (bp);
			}
		} else if (-- fl.fl_nsec == 0) {
			bp->b_resid = 0;
			fldone (bp);
		} else {
			++ fl.fl_secn;
			fl.fl_addr += 512;	/* 512 == fl.fl_fd.fd_nbps */
			fl.fl_state = SSEEK;
		}

		/*
		 * Delay for minimum 1.5 msecs after writing before seek.
		 */

		if (fl.fl_wflag) {
			timeout (& fltim, 2, fldelay, fl.fl_state);
			fl.fl_state = SDELAY;
			break;
		}

		goto again;

	case SDELAY:
T_HAL (0x40000, printf ("SDELAY "));
		/*
		 * Ignore interrupts until timeout occurs.
		 */
		break;

	default:
		panic ("fds");
	}
}


/*
 * Delay before initiating next operation.
 * This allows the floppy motor to turn on,
 * the head to settle before writing,
 * the erase head to turn off after writing, etc.
 */

static void
fldelay (state)
int state;
{
	int s;

	s = sphi ();
	if (fl.fl_state == SDELAY) {
		fl.fl_state = state;
		flfsm ();
	}
	spl (s);
}


/*
 * The flrate function returns the data rate for the flopen and flfsm routines.
 */

static int
flrate (dev)
dev_t dev;
{
	int unit = funit (dev);
	int rate = frates [fl.fl_type [unit]].fl_hi_rate;

	if (rate == -1 || fdata [fkind (dev)].fd_nspt < 15)
		rate = frates [fl.fl_type [unit]].fl_lo_rate;

	return rate;
}


/*
 * fldone () returns current request to operating system.
 */

static void
fldone (bp)
BUF * bp;
{
	fl.fl_actf = bp->b_actf;
	fl.fl_state = SIDLE;
	bdone (bp);
	wakeup (& fl.fl_state);
}


/*
 * The recovery routine resets and reprograms the floppy controller,
 * and discards any queued requests on the current drive.
 * This is required if the floppy door is open, or diskette is missing.
 */

static void
flrecov ()
{
	register int	x;

	if (fl_dsk_ch_prob)
		jopen = 1;

	/*
	 * Disable DMA transfer.
	 * Reset floppy controller.
	 */
	dmaoff (DMA_CH2);

	/*
	 * Unlock the controller if locked by us.
	 */

	outb (FDCDOR, 0);
	fl.fl_state = SIDLE;
	wakeup (& fl.fl_state);
	dmaunlock (& fldmalck); 		/* Ensures 14 clock cycles */
	outb (FDCDOR, DORNMR | DORIEN);

	fl.fl_mstatus = 0;			/* No motors on */
	fl.fl_selected_unit = -1;		/* No unit selected */

	/*
	 * Program floppy controller.
	 */
	fdcSpecify (fl_srt, fl_hut, fl_hlt);	/* Forces wait */

	/*
	 * Program transfer bps.
	 */
	fdcRate (fl.fl_rate_set);

	/*
	 * Drives are no longer in calibration.
	 * Was: fl.fl_incal [1] = -1;
	 * Changed to: fl.fl_incal [x] = -1;
	 * -louis 7/93
	 */
	for (x = 0; x < MAXDRVS; x ++)
		fl.fl_incal [x] = -1;

	/*
	 * Abort all block requests on current drive after 1st recov attempt.
	 */
	if (fl.fl_actf) {
		printf ("fd%d: <Door Open>\n", fl.fl_unit);     /* Message    */
		clrQ (fl.fl_actf->b_dev);		/* Dump pending reqs. */
		fl.fl_dsk_chngd [fl.fl_unit] = 1;	/* Make disk changed. */
	}

	/*
	 * Delay before setting controller state to idle.
	 * This gives time for spurious floppy interrupts to occur.
	 * NOTE: Can't call flfsm (), since it may call us (future revision).
	 */
	timeout (& fltim, HZ /4, fldelay, SIDLE);
	fl.fl_state = SDELAY;
}


/*
 * This routine is called by the
 * clock handler every second. If the drive
 * has been idle for a long time it turns off
 * the motor and shuts off the timeouts.
 */

#if __USE_PROTO__
static void fltimeout (o_dev_t __NOTUSED(dev))
#else
static void
fltimeout (dev)
o_dev_t dev;
#endif
{
	register int	unit;
	register int	mask;
	register int	s;

	s = sphi ();

	/*
	 * Scan all drives, looking for motor timeouts.
	 */
	for (unit = 0, mask = 0x10; unit < MAXDRVS; unit ++, mask <<= 1) {

		/*
		 * Ignore drives which aren't spinning.
		 */
		if ((fl.fl_mstatus & mask) == 0)
			continue;

		/*
		 * If timer is disabled (i.e. we are waiting for the DMA
		 * controller), go on to the next drive.
		 */
		if (fl.fl_time [unit] < 0)
			continue;

		/*
		 * Leave recently accessed (in last 4 seconds) drives spinning.
		 */
		if (++ fl.fl_time [unit] < MTIMER)
			continue;

		/*
		 * Timeout drives which have been inactive for 5 seconds.
		 */
		fl.fl_mstatus &= ~ mask;
		if (unit == fl.fl_selected_unit)
			fl.fl_selected_unit = -1;

		/*
		 * Not selected drive, or selected drive is idle.
		 */
		if (unit != fl.fl_unit || fl.fl_state == SIDLE)
			continue;

		/*
		 * Active drive did not complete operation within 5 seconds.
		 * Attempt recovery.
		 */
		flrecov ();

		/*
		 * Initiate next block request.
		 */
		if (fl.fl_state == SIDLE)
			flfsm ();
	}

	/*
	 * Physically turn off drives which timed out.
	 */
	outb (FDCDOR, DORNMR | DORIEN | fl.fl_mstatus | fl.fl_unit);

	/*
	 * Stop checking once all drives have been stopped.
	 */
	if (fl.fl_mstatus == 0)
		setFlTimer (0);

	spl (s);
}


static void
flDrvSelect ()
{
	fl.fl_time [fl.fl_unit] = 0;		/* Start motor-on timeout. */
	fl.fl_mstatus |= fl.fl_mask;
	fdcDrvSelect (fl.fl_unit, 1);		/* "1" for motor on	*/
	fl.fl_selected_unit = fl.fl_unit;	/* This unit is running. */
}


/*
 * Get the drive status
 */

static void
flDrvStatus ()
{
	register int	b;
	register int	n = 0;		/* # of status bytes read */
	register int	i = 0;		/* Timeout count */
	register int	s;

	s = sphi ();

	flDrvSelect ();			/* Be sure drive is selected */

	/* Issue a sense drive status command. */
	fdcDrvStatus (fl.fl_unit, 0);

	/* Stash Drive Status results. */
	for (;;) {
		/* Check for incoming signal. */
		spl (s);
		if (curr_signal_pending ()) {  /* signal? */
			set_user_error (EINTR);
			break;
		}
		s = sphi ();

		/* Copy data byte from FDC into fl_drvstat array. */
		b = fdcGet ();
		if (b == -1)
			break;
		if (n < FL_NUM_DRV_STAT)
			fl.fl_drvstat [n ++] = b;
	}
	fl.fl_ndrvstat = n;
	spl (s);
}


static void
flIntHandler ()
{
	/*
	 * Need to get FDC status from result phase, and clear interrupt
	 * that may have been generated by diskette change or seek/recal
	 * complete.
	 */
	fdcSense ();

	if (fl.fl_state != SIDLE)
		flfsm ();
}


CON fl386con = {
	DFBLK | DFCHR,			/* Flags */
	FL_MAJOR,			/* Major index */
	flopen,				/* Open */
	flclose,			/* Close */
	flblock,			/* Block */
	flread,				/* Read */
	flwrite,			/* Write */
	flioctl,			/* Ioctl */
	NULL,				/* Powerfail */
	fltimeout,			/* Timeout */
	flload,				/* Load */
	flunload,			/* Unload */
	NULL				/* Poll */
};
