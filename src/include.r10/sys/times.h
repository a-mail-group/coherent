/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef	 __SYS_TIMES_H__
#define	 __SYS_TIMES_H__

/*
 * Structure returned by the `times' system call.
 */

#include <common/feature.h>
#include <common/ccompat.h>
#include <common/_clock.h>

struct tms {
	__clock_t	tms_utime;		/* Process user time */
	__clock_t	tms_stime;		/* Process system time */
	__clock_t	tms_cutime;		/* Child user time */
	__clock_t	tms_cstime;		/* Child system time */
};

__EXTERN_C_BEGIN__

int		times		__PROTO ((struct tms * _buffer));

__EXTERN_C_END__

#endif	/* ! defined (__SYS_TIMES_H__) */
