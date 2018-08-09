/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef __SYS_SILO_H__
#define __SYS_SILO_H__

/*
 * Raw Character Silo.
 */

#define	SI_BUFSIZ	1024	/* Silo buffer size */

typedef struct silo {
	unsigned int	si_ix;
	unsigned int	si_ox;
	unsigned int	si_cnt;
	unsigned char	si_buf[SI_BUFSIZ];
} silo_t;

#if	0
/*
 * For rawin silo (see poll_clk.h), use last element of si_buf to count
 * the number of characters in the silo.
 */
#define SILO_CHAR_COUNT	si_buf[SI_BUFSIZ-1]
#else
#define SILO_CHAR_COUNT	si_cnt
#endif

#endif	/* ! defined (__SYS_SILO_H__) */
