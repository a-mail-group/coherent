#ifndef	ASSIGN_H
#define	ASSIGN_H

/*
 *-IMPORTS:
 *	<sys/types.h>
 *		major_t
 *		minor_t
 */

#include <sys/types.h>
#include "devadm.h"


/*
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

struct extinfo {
	major_t		ei_nemajors;
	major_t	      *	ei_etoimajor;
	major_t	      *	ei_minoroffset;
	int		ei_ncdevs;
	mdev_t	     **	ei_cdevsw;
	int		ei_nbdevs;
	mdev_t	     **	ei_bdevsw;
	int		ei_nmodules;
	mdev_t	     **	ei_modules;
	int		ei_ncohdrivers;
	mdev_t	     **	ei_cohdrivers;
};


EXTERN_C_BEGIN

extinfo_t     *	assign_imajors		PROTO ((void));

EXTERN_C_END

#endif	/* ! defined (ASSIGN_H) */
