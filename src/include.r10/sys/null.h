/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1994 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef __SYS_NULL_H__
#define __SYS_NULL_H__

/*
 * ioctl commands for devices with same major number as /dev/null.
 */

#define NLIOC	('N' << 8)

/*
 * /dev/kmem, diagnostic kernels only:
 * NLCALL : Takes an array of unsigned ints.  The first element is the
 * length of the whole array, the second element is a pointer to the
 * function to call, all other elements are arguments.  At most 5
 * arguments may be passed.
 */
#define NLCALL	(NLIOC|1)	/* Call a function with arguments.  */

/*
 * /dev/idle
 * NLIDLE : takes a (struct idlesys *) argument
 */
#define NLIDLE	(NLIOC|2)	/* Get idle time of the system process */

/*
 * /dev/freemem
 * NLIDLE : takes a (struct freemem *) argument
 */
#define NLFREE	(NLIOC|3)	/* Get free memory */

/*
 * structure returned by NLIDLE command
 */
typedef struct idlesys {
	int	idle_ticks;	/* total idle time in ticks */
	int	total_ticks;	/* total ticks since system booted */
} IDLESYS;

/*
 * structure returned by NLFREE command
 */
typedef struct freemem {
	int	avail_mem;	/* available memory in KB */
	int	free_mem;	/* free memory in KB */
} FREEMEM;

#endif /* ! defined (__SYS_NULL_H__) */
