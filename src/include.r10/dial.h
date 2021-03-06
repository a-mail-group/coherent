/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef	__DIAL_H__
#define	__DIAL_H__

#include <termio.h>

#define	LDEVS	"/usr/lib/uucp/L-devices"
#define	DEVDIR	"/dev/"				/* device path */
#define	LOCK	"/usr/spool/uucp/LCK.."		/* lock file semaphore */
#define	DVC_LEN	30			/* max num of chars in TTY-device path name */

/*
 *	Error mnemonics
 */

#define	INTRPT	(-1)	/* Interrupt occured */
#define	D_HUNG	(-2)	/* dialer hung (no return from write) */
#define	NO_ANS	(-3)	/* no answer within 10 seconds */
#define	ILL_BD	(-4)	/* illegal baud rate */
#define	A_PROB	(-5)	/* acu problem (open() failure) */
#define	L_PROB	(-6)	/* line problem (open() failure) */
#define	NO_Ldv	(-7)	/* cannot open LDEVS file */
#define	DV_NT_A	(-8)	/* requested device not available */
#define	DV_NT_K	(-9)	/* requested device not known */
#define	NO_BD_A	(-10)	/* no device available at requested baud */
#define	NO_BD_K	(-11)	/* no device known at requested baud */

typedef	struct {
	struct	termio	*attr;	/* ptr to termio attribute struct */
	int	baud;		/* transmission baud rate */
	int	speed;		/* 212A modem: low=300, high=1200 */
	char	*line;		/* device name for outgoing line */
	char	*telno;		/* ptr to telephone num digits string */
	int	modem;		/* allow modem control on direct lines */
} CALL;

extern	int	dial();
extern	void	undial();

#endif
