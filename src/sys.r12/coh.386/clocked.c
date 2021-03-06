/* $Header: /ker/coh.386/RCS/clocked.c,v 2.4 93/10/29 00:55:01 nigel Exp Locker: nigel $ */
/*
 * clocked.c - support routines for alternate clock rate
 *
 *  altclk_in(hz, fn) - install routine with specified rate
 *                      "hz" should be a multiple of system rate of 100 Hz
 *			return 0 if completed ok, -1 otherwise
 *
 *  altclk_out()      - uninstall alternate clock routine and restore system rate
 *			return old value of "altclk"
 *
 *  altclk_rate(hz)   - set clock interrupt rate
 *			new rate must be an even multiple of system rate "HZ"
 *			return 0 if completed ok, -1 otherwise
 *
 *  History:
 *    90/08/08 hws	initial version, works with hs.c modified for com[1-4]
 *    90/08/14 hws	make it more like a Unix system call
 *
 * $Log:	clocked.c,v $
 * Revision 2.4  93/10/29  00:55:01  nigel
 * R98 (aka 4.2 Beta) prior to removing System Global memory
 * 
 * Revision 2.3  93/08/19  03:26:23  nigel
 * Nigel's r83 (Stylistic cleanup)
 * 
 * Revision 2.2  93/07/26  15:22:04  nigel
 * Nigel's R80
 */

#include	<kernel/param.h>		/* HZ */

extern	int	 (*altclk)();		/* hook for polled devices */


#define	PIT	0x40		/* 8253 port */
#define	TMR0_M3	0x36		/* timer 0, mode 3 */
#define	SYS_HZ	1193200L	/* rate of input clock to timer 0 */

typedef int (*PFI)();		/* pointer to function returning int */

int
altclk_rate(hz)
unsigned int hz;
{
	int s;			/* to save CPU irpt flag */
	unsigned int interval;	/* period for hz, in units of 1.19 MHz ticks */
	int ret;

	if (hz >= HZ && hz % HZ == 0) {		/* can't go slower than HZ! */
		interval = SYS_HZ/hz;
		s = sphi();			/* disable irpts */
		outb(PIT+3, TMR0_M3);
		outb(PIT, interval & 0xff);
		outb(PIT, interval >> 8);	/* unsigned shift */
		spl(s);				/* restore previous irpt state */
		ret = 0;
	} else {
		ret = -1;
	}
	return ret;
}

int
altclk_in(hz, fn)
int hz;
PFI fn;
{
	int ret;

	if ((ret = altclk_rate(hz)) == 0)
		altclk = fn;
	return ret;
}

PFI
altclk_out()
{
	PFI ret;

	ret = altclk;
	if (ret) {
		altclk_rate(HZ);
		altclk = 0;
	}
	return ret;
}
