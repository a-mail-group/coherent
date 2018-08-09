/* Generated from Space.spc on Tue Jul 26 15:06:41 1994 PDT */
/*
 * Configurable information for "at" (ST506-compatible) device driver.
 */

#define __KERNEL__	 1

#include <sys/hdioctl.h>
#include "conf.h"

#define _TAG(tag)

/*
 * ATSECS is number of seconds to wait for an expected interrupt.
 * ATSREG needs to be 3F6 for most new IDE drives;  needs to be
 *	1F7 for Perstor controllers and some old IDE drives.
 *	Either value works with most drives.
 */

unsigned	ATSECS = ATSECS_SPEC;
unsigned	ATSREG = ATSREG_SPEC;

/*
 * Drive parameters (translation mode, if used).
 * Arguments for the macro _HDPARMS are:
 *   cylinders, heads, sectors per track, control byte, write precomp cylinder.
 */
struct hdparm_s	atparm[N_ATDRV] = {
_TAG(AT0)	_HDPARMS(0,0,0,0,0),
_TAG(AT1)	_HDPARMS(0,0,0,0,0)
};
