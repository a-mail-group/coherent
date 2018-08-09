/* $Header: /ker/io.386/RCS/scsi.c,v 2.4 93/08/19 10:39:25 nigel Exp Locker: nigel $ */
/*
 * This is the generic SCSI part of the
 * Adaptec AHA154x host adapter driver for the AT.
 *
 * $Log:	scsi.c,v $
 * Revision 2.4  93/08/19  10:39:25  nigel
 * r83 ioctl (), corefile, new headers
 * 
 * Revision 2.3  93/08/19  04:03:12  nigel
 * Nigel's R83
 */

#include <sys/debug.h>

#include <sys/coherent.h>
#include <kernel/trace.h>
#include <sys/fdisk.h>
#include <sys/hdioctl.h>
#include <sys/sdioctl.h>
#include <sys/buf.h>
#include <sys/con.h>
#include <sys/stat.h>
#include <sys/uproc.h>
#include <sys/errno.h>
#include <sys/scsiwork.h>
#include <kernel/typed.h>
#include <sys/mmu.h>

extern	short	at_drive_ct;

/*
 * Configurable parameters
 */
#define DEF_AHA_HDS	64
#define DEF_AHA_SPT	32

#define NDRIVE	(8 * 4)			/* 8 SCSI ids and 4 LUNs */
#define	SDMAJOR	13			/* Major Device Number */

/*
 * Configurable variables.
 */

extern int	AHA_SDIRQ;		/* Interrupt */
extern int	AHA_SDBASE;		/* Port base */
extern int	AHA_SDDMA;		/* Used for first party DMA */

extern int	AHA_SD_HDS;
extern int	AHA_SD_SPT;

/*
 *					LUN --------++
 * device macros			Special-+   ||
 * minor device bits are of the form:		76543210
 *						 |||  ||
 *					SCSI ID--+++  ||
 *					Partition ----++
 * Partition mapping:
 *
 * Description	   Special Bit	   Partition #		Device		Type
 * -----------	   -----------	   -----------		------		----
 * partition a		0		00		/dev/sd??a	disk
 * partition b		0		01		/dev/sd??b	disk
 * partition c		0		10		/dev/sd??c	disk
 * partition d		0		11		/dev/sd??d	disk
 * partition table	1		00		/dev/sd??x	disk
 * no rewind tape	1		01		/dev/sd??n	tape
 * UNALLOCATED		1		10		  ---		????
 * rewind tape device	1		11		/dev/sd??	tape
 */

#define	DRIVENO(minor)	(((minor) >> 2) & 0x1F)	/* SCSI ID + LUN */
#define	SCSIID(minor)	(((minor) >> 4) & 0x7)	/* SCSI ID */
#define	LUN(minor)	(((minor) >> 2) & 0x3)	/* Logical Unit Number */
#define	PARTITION(minor) ((minor) & 0x3)	/* Partition */
#define	sdmkdev(maj, s, drv)	makedev((maj), ((s)|((drv)<<2)))

/*
 *	host adapter routines
 */
int	aha_load ();		/* initialize host adapter, DMA */
void	aha_unload ();		/* shutdown the host adapter */
int	aha_start ();		/* see if there's work */
int	aha_command ();

/*
 * Partition Parameters - copied from disk.
 *
 *	There are NPARTN positions for the user partitions in array PPARM,
 *	plus 1 additional position to span the entire drive.
 *	Array pparmp [] contains a pointer to a kalloc ()'ed PPARM
 *	entry if the drive actually exists, is a disk drive and if someone
 *	has attmpted to read a partition table from the drive.
 */

typedef	struct	fdisk_s	PPARM [NPARTN + 1];	/* 4 partitions + whole drive */
static	PPARM * pparmp [NDRIVE];			/* one per possible drive */
#define	WHOLE_DRIVE	NPARTN			/* index for whole drive */

/*
 * Per disk controller data.
 * Only one host adapter; no more, no less.
 */

static	scsi_work_t	sd;

static	int	sw_active;


/*
 * void
 * sdload ()	- load routine.
 *
 *	Action:	The controller is reset and the interrupt vector is grabbed.
 *		The drive characteristics are set up at this time.
 */

static void
sdload ()
{
	FIFO * ffp;
	typed_space * tp;
	extern typed_space boot_gift;

	/*
	 * Initialize Drive Controller.
	 */

	sw_active = 0;
	if (aha_load (AHA_SDDMA, AHA_SDIRQ, AHA_SDBASE, & sd) < 0) {
		SET_U_ERROR (ENXIO, "aha_load () failed.");
		return;
	}

	/*
	 * Set values for # of heads and # of sectors per track.
	 *
	 * AHA translation mode uses the same # of heads
	 * and the same # of sectors per track for all drives.
	 *
	 * If these values are already patched, leave them alone.
	 * Otherwise, look in the data area written by tboot.
	 * If nothing from tboot, use default values.
	 */
	if (AHA_SD_HDS == 0 || AHA_SD_SPT == 0) {
		/* heads & spt not both patched */
		AHA_SD_HDS = DEF_AHA_HDS;
		AHA_SD_SPT = DEF_AHA_SPT;
		if (F_NULL != (ffp = fifo_open (& boot_gift, 0))) {
			if (tp = fifo_read (ffp)) {
				BIOS_DISK * bdp = (BIOS_DISK *)tp->ts_data;
				if (T_BIOS_DISK == tp->ts_type &&
				    at_drive_ct == bdp->dp_drive ) {
				/* got values from tboot */
					AHA_SD_HDS = bdp->dp_heads;
					AHA_SD_SPT = bdp->dp_sectors;
				}
			}
			fifo_close (ffp);
		}
	}
printf (" AHA_SD_HDS =%d AHA_SD_SPT =%d\n", AHA_SD_HDS, AHA_SD_SPT);

/*	aha_device_info (); */		/* enable after this gets fixed */
}


/*
 * void
 * sdunload ()	- unload routine.
 */

static void
sdunload ()
{
	int i;

	if (sw_active > 0)
		printf ("aha154x: sdunload () athough %d active\n", sw_active);
	aha_unload (AHA_SDIRQ);
	for (i = 0; i < NDRIVE; ++ i)
		if (pparmp [i] != NULL)
			kfree (pparmp [i]);	/* free any partition tables */
}


/*
 * int
 * sdgetpartitions (dev)	- load partition table for specified drive
 *
 *			- return 1 on success and 0 on failure
 */

int sdgetpartitions (dev)
dev_t	dev;
{
	int 	i;
	scsi_cmd_t	sc;
	unsigned char	* buffer;
	struct fdisk_s	* fdp;
	int	d = DRIVENO (minor (dev));

	pfree (palloc (100));
	pfree (palloc (3000));

	pparmp [d] = kalloc (sizeof * pparmp [0]);
	fdp = (struct fdisk_s *) pparmp [d];	/* point to first entry */
#ifdef _I386
	buffer = palloc (36 + 1);
#else /* _I386 */
	buffer = kalloc (36 + 1);
#endif /* _I386 */

	if (buffer == NULL || pparmp [d] == NULL) {
		printf ("aha154x: out of kernel memory\n");
		SET_U_ERROR (ENOMEM, "aha154x: out of kernel memory");
		return 0;
	}
	memset (pparmp [d], 0, sizeof * pparmp [0]);
	sc.unit = d;
	sc.block = 0L;
	sc.blklen = 0;

#ifdef _I386
	/* sc.buffer is a virtual-physical address (Ciaran Space.) */
	sc.buffer = vtovp (buffer);
#else /* _I386 */
	sc.buffer = VTOP2 (buffer, sds);
#endif /* _I386 */
	++ drvl [SDMAJOR].d_time;

	sc.cmd = ScmdREADCAPACITY;
	sc.buflen = 8;

	for (i = 0; i < sc.buflen; ++ i)
		buffer [i] = 0;

	/*
	 * If we call aha_command () only once we get a capacity of
	 * 0.  All ScmdREADCAPACITY calls after the first return a
	 * correct answer.
	 *
	 * This may be a bug in the aha154x.
	 */

	aha_command (& sc);
 	aha_command (& sc);

#if	0
	/*
	 * NIGEL: Tracer macros must be passed expressions now.
	 */
	T_PIGGY ( 0x20000, {
		printf ("buffer =");
		for (i = 0; i < sc.buflen; ++ i)
			printf (" %x", buffer [i]);
		printf ("\n");
	});
#endif

	sc.block = (buffer [0]<<8) | buffer [1];
	sc.block <<= 16;
	sc.block |= (buffer [2]<<8) | buffer [3];

	sc.blklen = (buffer [6]<<8) | buffer [7];

	T_PIGGY ( 0x20000,
		printf ("SCSI %ld. blocks of size %d\n", sc.block, sc.blklen));

	pfree (buffer);

	fdp [WHOLE_DRIVE].p_size = sc.block;
	if (0 == fdp [WHOLE_DRIVE].p_size) {
		/*
		 * If we are just opening this drive, make it so we can
		 * read the first block without an error.
		 */

		fdp [WHOLE_DRIVE].p_size = 1;
	}

	-- drvl [SDMAJOR].d_time;
	return fdisk (sdmkdev (major (dev), SDEV, d), pparmp [d]);
}


/*
 * void
 * sdopen (dev, mode)
 * dev_t dev;
 * int mode;
 *
 *	Input:	dev = disk device to be opened.
 *		mode = access mode [IPR, IPW, IPR + IPW].
 *
 *	Action:	Validate the minor device.
 *		Update the paritition table if necessary.
 */

static void
sdopen (dev, mode)
dev_t	dev;
{
	int p;			/* partition */
	int d;			/* drive (SCSI ID + LUN) */
	struct fdisk_s	* fdp;		/* one partition entry */

	if (minor (dev) & SDEV) {
		if (PARTITION (minor (dev)) != 0) {	/* tape device ? */
							/* not yet! */
			SET_U_ERROR (ENXIO, "No tape yet");
			devmsg (dev, "No tape yet");
		} else {
			++ drvl [SDMAJOR].d_time;
			++ sw_active;
		}
		return;
	}

	d = DRIVENO (minor (dev));
	p = PARTITION (minor (dev));

	/*
	 * If partition not defined read partition characteristics.
	 */
	if (pparmp [d] == NULL)   /* no entry yet for this drive ? */
		if (! sdgetpartitions (dev)) {
			SET_U_ERROR (ENXIO, "sdgetpartitions () failed.");
			return;
		}
	/*
	 * Ensure partition lies within drive boundaries and is non-zero size.
	 */

	fdp = (struct fdisk_s *) pparmp [d];
	if (fdp [p].p_base + fdp [p].p_size > fdp [WHOLE_DRIVE].p_size)
		SET_U_ERROR (EINVAL, "Partition exceeds drive size.");
	else if (fdp [p].p_size == 0)
		SET_U_ERROR (ENODEV, "No such partition.");
	else {
		++ drvl [SDMAJOR].d_time;
		++ sw_active;
	}
}


void sdclose (dev)
{
	-- drvl [SDMAJOR].d_time;
	-- sw_active;
}


/*
 * void
 * sdread (dev, iop)	- write a block to the raw disk
 * dev_t dev;
 * IO * iop;
 *
 *	Input:	dev = disk device to be written to.
 *		iop = pointer to source I / O structure.
 *
 *	Action:	Invoke the common raw I / O processing code.
 */

static void
sdread (dev, iop)
dev_t	dev;
IO	* iop;
{
	ioreq (NULL, iop, dev, BREAD, BFRAW | BFBLK | BFIOC);
}


/*
 * void
 * sdwrite (dev, iop)	- write a block to the raw disk
 * dev_t dev;
 * IO * iop;
 *
 *	Input:	dev = disk device to be written to.
 *		iop = pointer to source I / O structure.
 *
 *	Action:	Invoke the common raw I / O processing code.
 */

static void
sdwrite (dev, iop)
dev_t	dev;
IO	* iop;
{
	ioreq (NULL, iop, dev, BWRITE, BFRAW | BFBLK | BFIOC);
}


/*
 * void
 * sdioctl (dev, cmd, arg)
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

static void
sdioctl ( dev, cmd, vec)
dev_t	dev;
int cmd;
char * vec;
{
	int i;	/* Integer for abusing.  */
	int d;	/* Drive number.  */
	hdparm_t hdparm;
	struct fdisk_s	* fdp;
	int do_getpt = 0;	/* 1 if need to call sdgetpartitions () */

	d = DRIVENO (minor (dev));

	/*
	 * Identify input / output request.
	 */
	switch (cmd) {

	case HDGETA:
		/*
		 * If haven't loaded partition table yet for this drive,
		 * try to do it now.  Note sdgetpartitions () will fail
		 * if there is a new drive (e.g. no signature).  But all
		 * we need is allocation of pparmp [d] and capacity read
		 * properly from the drive.
		 */
		if (pparmp [d] == NULL) {
			do_getpt = 1;	/* REALLY just want Read Capacity */

			i = sdgetpartitions (dev);

			if (pparmp [d] == NULL) {
				SET_U_ERROR (ENXIO, "sdgetparitions () failed.");
				break;
			}
		}
		fdp = (struct fdisk_s *) pparmp [d];
		*(short *)& hdparm.landc [0] =
		*(short *)& hdparm.ncyl [0] = fdp [WHOLE_DRIVE].p_size
						/ (AHA_SD_HDS * AHA_SD_SPT);
		hdparm.nhead = AHA_SD_HDS;
		hdparm.nspt = AHA_SD_SPT;
		kucopy (& hdparm, vec, sizeof hdparm);
		/*
		 * I know it's ugly.  But it gets around startup Catch-22.
		 *
		 * The fdisk command needs HDGETA.  HDGETA invokes
		 * sdgetpartitions (), but we want to call it again
		 * after the partition table has been created by the fdisk
		 * command.
		 */
		if (do_getpt) {
			kfree (pparmp [d]);
			pparmp [d] = NULL;	/* force re-read of p. table */
		}
		break;

	case HDSETA:
		/*
		 * Set hard disk attributes.
		 */
		fdp = (struct fdisk_s *) pparmp [d];
		ukcopy (vec, & hdparm, sizeof hdparm);
		AHA_SD_HDS = hdparm.nhead;
		AHA_SD_SPT = hdparm.nspt;
		fdp [WHOLE_DRIVE].p_size =
			(long)(*(short *)& hdparm.ncyl [0])
			* (long)AHA_SD_HDS * (long)AHA_SD_SPT;

		break;

	case SCSI_HA_CMD:
		aha_ioctl (cmd, vec);
		/* FALL THROUGH */

	case SCSI_CMD:
	case SCSI_CMD_IN:
	case SCSI_CMD_OUT:
		break;

	default:
		SET_U_ERROR ( EINVAL, "Illegal SCSI ioctl command." );
		break;
	}
}


/*
 * void
 * sdblock (bp)	- queue a block to the disk
 *
 *	Input:	bp = pointer to block to be queued.
 *
 *	Action:	Queue a block to the disk.
 *		Make sure that the transfer is within the disk partition.
 */

static void
sdblock (bp)
BUF	* bp;
{
	scsi_work_t * sw;
	int s;
	struct	fdisk_s	* fdp;

	int p = PARTITION (minor (bp->b_dev));
	int drv = DRIVENO (minor (bp->b_dev));

	if (minor (bp->b_dev) & SDEV)
		p = WHOLE_DRIVE;
	bp->b_resid = bp->b_count;

	fdp = (struct fdisk_s *) pparmp [drv];

	/*
	 * Range check disk region.
	 */
	if (pparmp [drv] == NULL) {
		if (p == WHOLE_DRIVE) {
#if 0
/* Why did we only allow people to access the first block of WHOLE_DRIVE?
   in cases where there was not a valid partition table? */
			if ((bp->b_bno != 0) || (bp->b_count != BSIZE)) {
				bp->b_flag |= BFERR;
				bdone (bp);
				return;
			}
#endif
		} else {
			printf ("aha154x: no partition table\n");
			bp->b_flag |= BFERR;
			bdone (bp);
			return;
		}
	} else if ((bp->b_bno + (bp->b_count / BSIZE)) > fdp [p].p_size) {

	T_PIGGY ( 0x20000 ,
		(printf ("\n (bp->b_bno: %x + (bp->b_count: %x / BSIZE): %x): %x > ",
			bp->b_bno, bp->b_count, (bp->b_count / BSIZE),
			(bp->b_bno + (bp->b_count / BSIZE))),
		 printf (" fdp [p].p_size: %x\n", fdp [p].p_size)));

		bp->b_flag |= BFERR;
		bdone (bp);
		return;
	}

	bp->b_actf = NULL;

	sw = (scsi_work_t *) palloc (sizeof (* sw));
	T_PIGGY (0x100000, printf ("sw (%x)", sw));

	if (sw == NULL) {
		printf ("aha154x: out of kernel memory\n");
		bp->b_flag |= BFERR;
		bdone (bp);
		return;
	}

	sw->sw_bp = bp;
	sw->sw_drv = drv;
	sw->sw_type = 0;
	if (p != WHOLE_DRIVE)
		sw->sw_bno   = fdp [p].p_base + bp->b_bno;
	else
		sw->sw_bno   = bp->b_bno;
	sw->sw_retry = 1;

	T_PIGGY (0x20000,
		 printf ("sdblock: drv %x bno %lx bp =%x, flag = %x\n",
			drv, (long) sw->sw_bno, bp, bp->b_flag));

	/*
	 * NIGEL: These fields were never filled in before, now that kalloc ()
	 * does as it is supposed to we have to do it properly.
	 */

	sw->sw_actl = sw->sw_actf = NULL;

	s = sphi ();
	if (sd.sw_actf == NULL) {
		ASSERT (sd.sw_actl == NULL);
		sd.sw_actf = sw;
	} else {
		ASSERT  (sd.sw_actl != NULL);
		sd.sw_actl->sw_actf = sw;
	}
	sd.sw_actl = sw;
	spl (s);

#ifdef	TRACER
	begin_count (0);
#endif
	aha_start ();
}


static void
sdwatch ()
{
	int i;

	if (0 != (i = aha_start ())) {
		T_PIGGY ( 0x20000, printf ("sdwatch: started %d actions\n", i));
	}

	if (0 != (i = aha_completed ())) {
		T_PIGGY ( 0x20000, printf ("sdwatch: completed %d actions\n", i));
	}

	T_PIGGY (0x20000, aha_mbox_status ());
}


/*
 * Driver configuration.
 */

CON	sdcon	= {
	DFBLK | DFCHR,			/* Flags */
	SDMAJOR,			/* Major index */
	sdopen,				/* Open */
	sdclose,			/* Close */
	sdblock,			/* Block */
	sdread,				/* Read */
	sdwrite,			/* Write */
	sdioctl,			/* Ioctl */
	NULL,				/* Powerfail */
	sdwatch,			/* Timeout */
	sdload,				/* Load */
	sdunload			/* Unload */
};
