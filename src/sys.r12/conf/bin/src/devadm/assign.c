#define	_DDI_DKI	1
#define	_SYSV4		1

/*
 * This file assigns device major and minor numbers based on configuration
 * data read in by "devadm.c".
 */
/*
 *-IMPORTS:
 *	<sys/compat.h>
 *		CONST
 *		PROTO
 *		ARGS ()
 *		LOCAL
 *	<sys/types.h>
 *		major_t
 *		minor_t
 *	<stddef.h>
 *		size_t
 *		offsetof ()
 *	<stdlib.h>
 *		NULL
 *		free ()
 *		malloc ()
 *	"ehand.h"
 *		throw_error ()
 *	"mdev.h"
 *		mdev_t
 *		mdev_sort ()
 */

#include <sys/compat.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>

#include "ehand.h"
#include "mdev.h"
#include "ecodes.h"

#include "assign.h"

#ifndef	NODEV
# define	((major_t) -1)
#endif


/*
 * Selection predicate for choosing enabled character devices.
 */

#if	USE_PROTO
LOCAL int chr_sel (mdev_t * mdevp)
#else
LOCAL int
chr_sel (mdevp)
mdev_t	      *	mdevp;
#endif
{
	return mdevp->md_configure == MD_ENABLED &&
	       mdev_flag (mdevp, MDEV_CHAR);
}


/*
 * Predicate for comparing two mdevice entries on the basis of minimum
 * external character-device number.
 */

#if	USE_PROTO
LOCAL int chr_pred (mdev_t * left, mdev_t * right)
#else
LOCAL int
chr_pred (left, right)
mdev_t	      *	left;
mdev_t	      *	right;
#endif
{
	return left->md_chr_maj [0] < right->md_chr_maj [0];
}


/*
 * Selection predicate for choosing enabled block devices.
 */

#if	USE_PROTO
LOCAL int blk_sel (mdev_t * mdevp)
#else
LOCAL int
blk_sel (mdevp)
mdev_t	      *	mdevp;
#endif
{
	return mdevp->md_configure == MD_ENABLED &&
	       mdev_flag (mdevp, MDEV_BLOCK);
}


/*
 * Predicate for comparing two mdevice entries on the basis of minimum
 * external block-device number.
 */

#if	USE_PROTO
LOCAL int blk_pred (mdev_t * left, mdev_t * right)
#else
LOCAL int
blk_pred (left, right)
mdev_t	      *	left;
mdev_t	      *	right;
#endif
{
	return left->md_blk_maj [0] < right->md_blk_maj [0];
}


/*
 * Predicate for selecting modules (streams drivers that are not devices).
 */

#if	USE_PROTO
LOCAL int mod_sel (mdev_t * mdevp)
#else
LOCAL int
mod_sel (mdevp)
mdev_t	      *	mdevp;
#endif
{
	return mdevp->md_configure == MD_ENABLED &&
	       mdev_flag (mdevp, MDEV_STREAM) &&
	       ! mdev_flag (mdevp, MDEV_CHAR);
}


/*
 * Predicate for selecting Coherent drivers.
 */

#if	USE_PROTO
LOCAL int coh_sel (mdev_t * mdevp)
#else
LOCAL int
coh_sel (mdevp)
mdev_t	      *	mdevp;
#endif
{
	return mdevp->md_configure == MD_ENABLED &&
	       mdev_flag (mdevp, MDEV_COHERENT);
}


/*
 * Predicate for comparing two Coherent devices on the basis of major number.
 */

#if	USE_PROTO
LOCAL int coh_pred (mdev_t * left, mdev_t * right)
#else
LOCAL int
coh_pred (left, right)
mdev_t	      *	left;
mdev_t	      *	right;
#endif
{
	return left->md_blk_maj [0] < right->md_blk_maj [0];
}


/*
 * This is where we build the external->internal major number mapping table.
 *
 * The external->internal device number system becomes immediately useful
 * under STREAMS, where a) STREAMS cannot use device major numbers lower than
 * MAJOR_RESERVED under Coherent, and b) Coherent uses old-style 16-bit
 * dev_t's, so that the ability to transform a range of external majors into
 * a contiguous sequence of internal minor numbers will be of immediate value
 * in supporting connection-oriented services (although this is also pending
 * on the addition of support for cloning to the kernel).
 *
 * Mapping between internal and external numbers is not simple; what makes it
 * worse is that because there is a single mapping that has to be the same
 * for both block and character tables, the construction of the bdevsw [] and
 * cdevsw [] internal tables is constrained. This is only a problem if
 * multiple external major numbers are allowed, and each device is only
 * permitted to have a single internal major number. The interpretation of the
 * 'M' flag in this circumstance is unclear.
 *
 * The assign_imajors () code should be able to deal with the most complex
 * case, which is where multiple external majors may actually be mapped to
 * multiple internal numbers, thus
 *
 *	Block external :            |-----scsi-------|
 *	Character external:   |--tcp--| |-udp-|  |ttys|
 *
 * Might map 'tcp' to internal 0, 'udp' to internal 1, 'ttys' to internal 2,
 * while 'scsi' would have 0, 1, and 2 as internal major numbers.
 *
 * This is all hypothetical at the moment, since multiple majors are new to
 * Coherent anyway. For simplicity, we may constrain the above to be an error
 * by requiring unique internal number for each device. However, the machinery
 * in assign_imajors () will have to be flexible enough to deal with the
 * above.
 *
 * NOTE: The complex model is only applicable if a device does not care what
 * minor numbers are given to it, since there is a single shared table used
 * to map a range of external major numbers into a range of internal minor
 * numbers. Since this table is shared, it constrains even more heavily the
 * circumstances in which numbers can overlap. Without a flag to detect when
 * a driver doesn't care about minor numbers, we simply forget about the
 * complex model.
 */

/*
 * Definitions used to control parts of the way assign_imajors () deals with
 * the external->internal mapping (as discussed above). The main things we
 * isolate here are the calculations of the upper bounds for table allocations
 * since the constraint of a common mapping means that the cdevsw [] and
 * bdevsw [] tables may be larger than a separate mapping would permit.
 *
 * In particular, if ncdevs is the number of character devices and nbdevs is
 * the number of block devices, the upper bound on table size is:
 *	Simple model:	block = char = max (ncdevs, nbdevs)
 *     Complex model:	char = ncdevs + max (nbdevs - 1, 0)
 *			block = nbdevs + max (ncdevs - 1, 0)
 */

#define	MAX(a,b)		((a) > (b) ? (a) : (b))
#define	MIN(a,b)		((a) < (b) ? (a) : (b))

#define	MAX_CHR_IMAJORS(c,b)	MAX (c, b)
#define	MAX_BLK_IMAJORS(c,b)	MAX (c, b)


/*
 * Assign internal major numbers to devices. For now, this function does not
 * attempt to deal with assigning major number ranges.
 */

#if	USE_PROTO
extinfo_t * (assign_imajors) (void)
#else
extinfo_t *
assign_imajors ARGS (())
#endif
{
	extinfo_t     *	extinfop;
	mdev_t	      *	blk_list;
	mdev_t	      *	blk_end;
	mdev_t	      *	chr_list;
	mdev_t	      *	chr_end;
	mdev_t	      *	mod_list;
	mdev_t	      *	coh_list;
	int		n_blk_list;
	int		n_chr_list;
	int		n_mod_list;
	int		n_coh_list;

	int		nemajors;
	int		i;


	/*
	 * The algorithm for assigning internal numbers for the block and
	 * character-device tables will be able to run in a single pass if we
	 * are able to provide lists of character and block devices sorted by
	 * beginning external major number. This sort process also provides us
	 * a variety of other useful information directly, such as the maximum
	 * external number used.
	 */

	n_chr_list = mdev_sort (& chr_list, & chr_end, chr_sel, chr_pred,
				offsetof (mdev_t, md_chrlink));
	n_blk_list = mdev_sort (& blk_list, & blk_end, blk_sel, blk_pred,
				offsetof (mdev_t, md_blklink));
	n_mod_list = mdev_sort (& mod_list, NULL, mod_sel, NULL,
				offsetof (mdev_t, md_modlink));
	n_coh_list = mdev_sort (& coh_list, NULL, coh_sel, coh_pred,
				offsetof (mdev_t, md_cohlink));

	nemajors = 0;

	if (chr_end != NULL)
		nemajors = MAX (nemajors, chr_end->md_chr_maj [1]);

	if (blk_end != NULL)
		nemajors = MAX (nemajors, blk_end->md_blk_maj [1]);

	nemajors ++;


	/*
	 * Now we know how many of everything there is, allocate space for
	 * tables.
	 */

	i = sizeof (* extinfop) +
	    sizeof (mdev_t *) * (MAX_CHR_IMAJORS (n_chr_list, n_blk_list) +
				 MAX_BLK_IMAJORS (n_chr_list, n_blk_list) +
				 n_mod_list + n_coh_list) +
	    2 * sizeof (minor_t) * nemajors;

	if ((extinfop = (extinfo_t *) malloc (i)) == NULL)
		throw_error (NO_MEMORY,
			     "insufficient memory in assign_imajors ()");

	extinfop->ei_etoimajor = (minor_t *) (extinfop + 1);
	extinfop->ei_minoroffset = extinfop->ei_etoimajor + nemajors;
	extinfop->ei_modules = (mdev_t **) (extinfop->ei_minoroffset +
					    nemajors);
	extinfop->ei_cohdrivers = extinfop->ei_modules + n_mod_list;
	extinfop->ei_cdevsw = extinfop->ei_cohdrivers + n_coh_list;
	extinfop->ei_bdevsw = extinfop->ei_cdevsw +
				MAX_CHR_IMAJORS (n_chr_list, n_blk_list);

	/*
	 * Since we allocate space for the maximum number of table entries,
	 * the loops below assign internal numbers which may not be
	 * contiguous, or may be below the upper limit allocated.
	 */

	extinfop->ei_nemajors = nemajors;
	extinfop->ei_ncdevs = 0;
	extinfop->ei_nbdevs = 0;
	extinfop->ei_nmodules = n_mod_list;
	extinfop->ei_ncohdrivers = n_coh_list;

	for (i = 0 ; i < nemajors ; i ++) {

		extinfop->ei_etoimajor [i] = NODEV;
		extinfop->ei_minoroffset [i] = 0;
	}


	for (i = 0 ; i < MAX_CHR_IMAJORS (n_chr_list, n_blk_list) ; i ++)
		extinfop->ei_cdevsw [i] = NULL;

	for (i = 0 ; i < MAX_BLK_IMAJORS (n_chr_list, n_blk_list) ; i ++)
		extinfop->ei_bdevsw [i] = NULL;


	while (chr_list != NULL || blk_list != NULL) {
		mdev_t	      *	chrp;
		mdev_t	      *	blkp;
		int		extlo;
		int		exthi;
		int		internal;
		int		minorinc;
		int		minorofs;

		/*
		 * Choose a range of external numbers that we are going to
		 * assign to a single internal number. If our choice is not
		 * constrained by an overlap between character and block
		 * external numbers, then we deal with that.
		 */

		blkp = blk_list;

		if ((chrp = chr_list) != NULL) {

			extlo = chrp->md_chr_maj [0];
			exthi = chrp->md_chr_maj [1];
			minorinc = chrp->md_minor_max;

			if (blkp != NULL) {

				if (blkp->md_blk_maj [1] < extlo) {

					chrp = NULL;
					goto doblock;
				}

				if (blkp->md_blk_maj [0] > exthi) {
					blkp = NULL;
					goto dochar;
				}

				/*
				 * Because of minor-number mapping, overlap is
				 * only valid in special circumstances.
				 */

				if (blkp->md_blk_maj [0] != extlo ||
				    (blkp->md_minor_max != minorinc &&
				     blkp->md_blk_maj [1] >
						blkp->md_blk_maj [0] &&
				     exthi > extlo)) {

					free (extinfop);
					throw_error (FORMAT_ERROR,
						     "minor number mapping conflict");
				}


				/*
				 * Choose a suitable internal major number.
				 */

				if (exthi < blkp->md_blk_maj [1]) {

					exthi = blkp->md_blk_maj [1];
					minorinc = blkp->md_minor_max;
				}

				internal = MAX (extinfop->ei_ncdevs,
						extinfop->ei_nbdevs);

				goto done;
			}
dochar:
			/*
			 * Simple case, select an internal number.
			 */

			for (internal = 0 ;
			     extinfop->ei_cdevsw [internal] != NULL ;
			     internal ++)
				;

			if (internal > extinfop->ei_ncdevs) {

				free (extinfop);
				throw_error (INTERNAL_ERROR,
					     "internal check failed, assign_imajors ()");
			}
		} else {
			/*
			 * Simple case for block device, select an internal
			 * number.
			 */

doblock:
			extlo = blkp->md_blk_maj [0];
			exthi = blkp->md_blk_maj [1];
			minorinc = blkp->md_minor_max;

			for (internal = 0 ;
			     extinfop->ei_bdevsw [internal] != NULL ;
			     internal ++)
				;

			if (internal > extinfop->ei_nbdevs) {

				free (extinfop);
				throw_error (INTERNAL_ERROR,
					     "internal check failed, assign_imajors ()");
			}
		}
done:
		/*
		 * Now we have decided when, where, and how much, fill in the
		 * table.
		 */

		minorofs = 0;

		while (extlo <= exthi) {

			if (extinfop->ei_etoimajor [extlo] != NODEV) {

				free (extinfop);
				throw_error (FORMAT_ERROR,
					     "major-number mapping conflict");
			}

			extinfop->ei_etoimajor [extlo] = internal;
			extinfop->ei_minoroffset [extlo] = minorofs;

			minorofs += minorinc;
			extlo ++;
		}

		if ((extinfop->ei_cdevsw [internal] = chrp) != NULL) {

			if (internal >= extinfop->ei_ncdevs)
				extinfop->ei_ncdevs = internal + 1;

			chr_list = chrp->md_chrlink;
		}


		if ((extinfop->ei_bdevsw [internal] = blkp) != NULL) {

			if (internal >= extinfop->ei_nbdevs)
				extinfop->ei_nbdevs = internal + 1;

			blk_list = blkp->md_blklink;
		}
	}


	/*
	 * Now we can build a table of all the STREAMS modules.
	 */

	i = 0;

	while (mod_list != NULL) {

		extinfop->ei_modules [i ++] = mod_list;
		mod_list = mod_list->md_modlink;
	}


	/*
	 * Now we can build a table of all the Coherent drivers.
	 */

	i = 0;

	while (coh_list != NULL) {

		extinfop->ei_cohdrivers [i ++] = coh_list;
		coh_list = coh_list->md_cohlink;
	}

	return extinfop;
}

