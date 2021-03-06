/* $Header: /ker/coh.386/RCS/pipe.c,v 2.5 93/10/29 00:55:27 nigel Exp Locker: nigel $ */
/* (lgl-
 *	The information contained herein is a trade secret of Mark Williams
 *	Company, and  is confidential information.  It is provided  under a
 *	license agreement,  and may be  copied or disclosed  only under the
 *	terms of  that agreement.  Any  reproduction or disclosure  of this
 *	material without the express written authorization of Mark Williams
 *	Company or persuant to the license agreement is unlawful.
 *
 *	COHERENT Version 2.3.37
 *	Copyright (c) 1982, 1983, 1984.
 *	An unpublished work by Mark Williams Company, Chicago.
 *	All rights reserved.
 -lgl) */
/*
 * Pipes.
 *
 * $Log:	pipe.c,v $
 * Revision 2.5  93/10/29  00:55:27  nigel
 * R98 (aka 4.2 Beta) prior to removing System Global memory
 * 
 * Revision 2.4  93/08/19  03:26:40  nigel
 * Nigel's r83 (Stylistic cleanup)
 * 
 * Revision 2.2  93/07/26  14:28:58  nigel
 * Nigel's R80
 * 
 * Revision 1.7  93/04/14  10:06:40  root
 * r75
 * 
 * Revision 1.2  92/01/06  11:59:52  hal
 * Compile with cc.mwc.
 * 
 * Revision 1.1	88/03/24  16:14:07	src
 * Initial revision
 * 
 * 86/11/19	Allan Cornish		/usr/src/sys/coh/pipe.c
 * Added check for non-blocking read and write if (io_flag & IPNDLY) set.
 * Eliminated use of i_a inode field since now included in inode macros.
 */

#include <kernel/_sleep.h>
#include <kernel/proc_lib.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/cmn_err.h>
#include <signal.h>
#include <stddef.h>
#include <limits.h>

#define	_KERNEL		1

#include <sys/filsys.h>
#include <sys/ino.h>
#include <sys/inode.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <sys/sched.h>

/*
 *  These are nothing more than random different values at this point!
 *  Historically, these were bit's or'ed into ip->i_flag, no more!
 */

#define	IFWFR	1			/* Sleeping Waiting for a Reader */
#define	IFWFW	2			/* Sleeping Waiting for a Writer */


/*
 *  pdump(loc, ip, mode)  --  A kernel debugging output line.
 *  char *loc  --  prefix of line (two characters indicating where we are)
 *  INODE *ip  --  The inode information to dump
 *  int mode   --  The mode of the IO call, i.e. IPW, IPR, IPNDLY, ...
 */

#if 1
#define	pdump(loc, ip, mode)	((void) 0)
#else
__LOCAL__ void
pdump (loc, ip, mode)
char *loc;
struct inode *ip;
int mode;
{
	printf("%s ip=%x mde=%x nlk=%x rf=%x nc=%x rx=%x wx=%x",
		loc, ip, mode, ip->i_nlink, ip->i_refc,
		ip->i_pnc, ip->i_prx, ip->i_pwx);

	printf(" ar=%x aw=%x sr=%x sw=%x f=%x\n",
		ip->i_par, ip->i_paw, ip->i_psr, ip->i_psw, ip->i_flag);
}
#endif


#if	__USE_PROTO__
__LOCAL__ void pclear (struct inode * ip)
#else
__LOCAL__ void
pclear(ip)
struct inode  *	ip;
#endif
{
	ip->i_pnc = ip->i_prx = ip->i_pwx = ip->i_par = ip->i_paw = ip->i_psr =
		ip->i_psw = 0;

	/*
	 * This is sleazy, but it'll do.
	 */

	memset (& ip->i_iev, 0, sizeof (ip->i_iev));
	memset (& ip->i_oev, 0, sizeof (ip->i_oev));
}


/*
 * Mark all the blocks allocated to the inode as clean.
 */

#if	__USE_PROTO__
__LOCAL__ void pclean (struct inode * ip)
#else
__LOCAL__ void
pclean (ip)
struct inode  *	ip;
#endif
{
	int		i;

	ASSERT (ilocked (ip));

	for (i = 0 ; i < ND ; i ++) {
		if (ip->i_pipe [i] != 0)
			bclean (ip->i_dev, ip->i_pipe [i]);
	}
}


/*
 *  pwake(ip, who)  --  wake up processes which are waiting for a reader if
 *		        (who==IFWFR) or waiting for a writer if (who==IFWFW).
 */

#if	__USE_PROTO__
void pwake (struct inode * ip, unsigned who)
#else
void
pwake (ip, who)
struct inode  *	ip;
unsigned	who;
#endif
{
	pdump ("KA", ip, 0);
	switch (who) {
	case IFWFW:
		if (ip->i_psr)
			wakeup ((char *) & ip->i_psw);
		if (ip->i_pnc > 0 ||
		    (! ip->i_paw && ! ip->i_psw)) /* HUP */
			pollwake (& ip->i_iev);
		break;

	case IFWFR:
		if (ip->i_psw)
			wakeup ((char *) & ip->i_psr);
		if (ip->i_pnc < PIPE_MAX && (ip->i_par || ip->i_psr))
			pollwake (& ip->i_oev);
		break;

	default:
		panic ("pwake() internal error");
	}
	pdump ("KZ", ip, 0);
}


/*
 *  pmake(mode)  --  called from the upipe() system call in sys3.c
 *
 *  Creates and returns a locked pipe inode with the given mode on
 *  the pipedev.
 */

#if	__USE_PROTO__
struct inode * pmake (unsigned mode)
#else
struct inode *
pmake (mode)
unsigned	mode;
#endif
{
	struct inode  *	ip;

	if ((ip = ialloc (pipedev, IFPIPE | mode, SELF->p_credp)) != NULL) {
		pclear (ip);
		icopymd (ip);
	}

	pdump ("M", ip, mode);
	return ip;
}


/*
 *  psleep(ip, who)  --  go to sleep either waiting for a reader if (who==IFWFR)
 *		         or waiting for a writer if (who==IFWFW).
 *  Returns:  0  if woke up ok
 *	     -1  if woke up by signal (e.g. SIGALRM, SIGKILL, etc.)
 */

#if	__USE_PROTO__
__LOCAL__ int psleep (struct inode * ip, unsigned who)
#else
__LOCAL__ int
psleep (ip, who)
struct inode  *	ip;
unsigned	who;
#endif
{
	__sleep_t	sleep;

	pdump ("SA", ip, 0);
	iunlock (ip);

	switch (who) {
	case IFWFW:
		-- ip->i_par;
		++ ip->i_psr;
		sleep = x_sleep ((char *) & ip->i_psw, primed, slpriSigCatch,
				 "pipe wx");
		++ ip->i_par;
		-- ip->i_psr;
		break;

	case IFWFR:
		-- ip->i_paw;
		++ ip->i_psw;
		sleep = x_sleep ((char *) & ip->i_psr, primed, slpriSigCatch,
				 "pipe rx");
		++ ip->i_paw;
		-- ip->i_psw;
		break;

	default:
		panic ("psleep() internal error");
	}
	ilock (ip, "psleep ()");
	pdump ("SZ", ip, 0);

	if (sleep == PROCESS_SIGNALLED) {
		set_user_error (EINTR);
		return -1;
	}
	return 0;
}


/*
 *  popen(ip, mode)  --  Opens a pipe inode, with the given mode.
 *			 Note:  The inode is locked upon entry.
 *
 *  This routine follows the requirements concerning opening pipes.
 *  Specifically, if opening readonly without O_NDELAY, then block
 *  until we have a writer.  If opening readonly with O_NDELAY, then
 *  return opened, no blocking.  If opening writeonly without O_NDELAY,
 *  then block until we have a reader.  If opening writeonly with
 *  O_NDELAY, then return an error, and set u.u_errno to ENXIO.
 *  Beware of subtle race conditions!  Also notice, I followed hal's
 *  style of no internal returns in a function.
 *
 *  Note: these pipe routines maintain the pipe counter variables:
 *	  ip->i_par:  Number of Awake readers
 *	  ip->i_paw:  Number of Awake writers
 *	  ip->i_psr:  Number of Sleeping readers
 *	  ip->i_psw:  Number of Sleeping writers
 */

#if	__USE_PROTO__
void popen (struct inode * ip, unsigned mode)
#else
void
popen (ip, mode)
struct inode  *	ip;
unsigned	mode;
#endif
{
	switch (mode & (IPR | IPW)) {
	case IPR:
		++ ip->i_par;

		while (! ip->i_paw && ! ip->i_psw) {
			if ((mode & (IPNDLY | IPNONBLOCK)) != 0)
				break;

			if (psleep (ip, IFWFW) < 0) {
				-- ip->i_par;
				return;
			}

			if (ip->i_pnc != 0)
				break;
		}
		pwake (ip, IFWFR);
		break;

	case IPW:
		++ ip->i_paw;
		if (! ip->i_par && ! ip->i_psr) {
			if ((mode & (IPNDLY | IPNONBLOCK)) != 0) {
				set_user_error (ENXIO);
				-- ip->i_paw;
				return;
			}

			if (psleep (ip, IFWFR) < 0) {
				-- ip->i_paw;
				return;
			}
		}
		pwake (ip, IFWFW);
		break;

	case IPR | IPW:
		++ ip->i_par;
		++ ip->i_paw;
		pwake (ip, IFWFW);
		pwake (ip, IFWFR);
		break;
	}
}


/*
 *  pclose(ip, mode)  --  Opens a pipe inode, with the given mode.
 *			  Note:  The inode is locked upon entry.
 *
 *  This routine closes the given INODE with the given mode.  We
 *  must have the mode correct to maintain counters properly.
 *  Good thing that mode cannot be changed by fcntl()!
 */

#if	__USE_PROTO__
void pclose (struct inode * ip, unsigned mode)
#else
void
pclose (ip, mode)
struct inode  *	ip;
unsigned	mode;
#endif
{
	pdump ("CA", ip, mode);
	pwake (ip, IFWFR);
	pwake (ip, IFWFW);
	if (mode & IPR) {
		if (-- ip->i_par < 0)
			panic ("Out of sync IPR in pclose");
		if (! ip->i_par && ! ip->i_psr)
			pollwake (& ip->i_oev);	/* HUP */
	}
	if (mode & IPW) {
		if (-- ip->i_paw < 0)
			panic ("Out of sync IPW in pclose");
		if (! ip->i_paw && ! ip->i_psw)
			pollwake (& ip->i_iev);	/* HUP */
	}

	if (! ip->i_paw && ! ip->i_psw && ! ip->i_par && ! ip->i_psr)
		pclear (ip);
	pdump ("CZ", ip, mode);
}


/*
 *  pread(ip, iop)  --  Reads from a pipe inode, accoring to the IO info.
 *			Note:  The inode is locked upon entry.
 *
 *  This routine follows the requirements concerning reading from pipes.
 *  Specifically, if there is no data in the pipe, then the read will
 *  block waiting for data, unless you have IONDLY set in which case
 *  it will simply return zero.  Notice, the traditional value returned
 *  from uread() is the number of characters actually read.  This is
 *  nothing more that iop->io_ioc on entry minus iop->io_ioc on exit.
 *  This routine also works with the ring buffer in the inode maintained
 *  by the variables ip->i_pnc:  Number of Characters in pipe.
 *		     ip->i_prx:  Offset in pipe to begin reading.
 *		     ip->i_pwx:  Offset in pipe to begin writing.
 *  Notice: we do not unlock the inode when we call fread(), this is to
 *  guarantee that we read all that is available even if we go to sleep.
 *  Subtle race condition?  I don't think so, since if we go to sleep
 *  in fread(), it's wrt a resource unrelated to this particular inode.
 */

#if	__USE_PROTO__
void pread (struct inode * ip, IO * iop)
#else
void
pread (ip, iop)
struct inode  *	ip;
IO	      *	iop;
#endif
{
	unsigned n;
	unsigned ioc;

	pdump ("R", ip, 0);

	while (ip->i_pnc == 0) {
		/*
		 * If we are in O_NDELAY mode, just return and uread () will
		 * see nothing read, returning 0 to the user.
		 */

		if ((iop->io_flag & IONDLY) != 0)
			return;

		/*
		 * If there are no writers, we are at EOF. Note that we *must*
		 * check this before NONBLOCK, otherwise EOF is never seen by
		 * users who are in non-blocking mode.
		 */

		if (! ip->i_paw && ! ip->i_psw)
			return;

		/*
		 * If we are in O_NONBLOCK mode, set_user_error () so that upon
		 * returning to user level the return value of uread () gets
		 * forced to -1. Layering? What layering?
		 */

		if ((iop->io_flag & IONONBLOCK) != 0) {
			set_user_error (EAGAIN);
			return;
		}

		pclean (ip);
		if (psleep (ip, IFWFW) < 0)
			return;
	}

	ioc = iop->io_ioc;

	while (! get_user_error () && ioc > 0 && ip->i_pnc > 0) {
		if ((n = (PIPE_MAX - ip->i_prx)) > ioc)
			n = ioc;

		if (n > ip->i_pnc)
			n = ip->i_pnc;

		iop->io_ioc = n;
		iop->io_seek = ip->i_prx;
		fread (ip, iop);
		n -= iop->io_ioc;

		if ((ip->i_prx += n) == PIPE_MAX)
			ip->i_prx = 0;
		if ((ip->i_pnc -= n) == 0) {
			ip->i_prx = ip->i_pwx = 0;
			pclean (ip);
		}
		ioc -= n;
	}

	iop->io_ioc = ioc;

	if (ip->i_pnc < PIPE_MAX)
		pwake (ip, IFWFR);
}


/*
 *  pwrite(ip, iop)  --  Writes to a pipe inode, according to the IO info.
 *			 Note:  The inode is locked upon entry.
 *
 *  This routine follows the requirements concerning writing to pipes.
 *  Specifically, if the pipe is full, then the write will block waiting
 *  for data to be consumed, unless you have IONDLY set in which case
 *  it will simply return zero.  Notice, the traditional value returned
 *  from uwrite() is the number of characters actually written.  This is
 *  nothing more that iop->io_ioc on entry minus iop->io_ioc on exit.
 *  In other words, iop->io_ioc had better be zero on exit.  The possibility
 *  does exist if the number of characters to be written is larger than
 *  PIPE_MAX, and thus we do not guarantee atomic writes, that while the
 *  process is sleeping waiting for a reader to consume data, that the
 *  process will be woken from sleeping by a SIGNAL, thus causing a partial
 *  write.  The return value will be the actual number of character written.
 *  This routine also works with the ring buffer in the inode maintained
 *  by the variables ip->i_pnc:  Number of Characters in pipe.
 *		     ip->i_prx:  Offset in pipe to begin reading.
 *		     ip->i_pwx:  Offset in pipe to begin writing.
 *  Notice: we do not unlock the inode when we call fwrite(), this is to
 *  guarantee that we have an atomic write for all writes of size less
 *  than PIPE_MAX, even if we go to sleep in the fwrite().  Subtle race
 *  condition?  I don't think so, since if we go to sleep in fwrite(),
 *  it's wrt a resource unrelated to this particular INODE.
 */

#if	__USE_PROTO__
void pwrite (struct inode * ip, IO * iop)
#else
void
pwrite (ip, iop)
struct inode  *	ip;
IO	      *	iop;
#endif
{
	unsigned n;
	unsigned ioc;

	pdump ("W", ip, 0);

	ioc = iop->io_ioc;
	while (! get_user_error () && ioc > 0) {
		if (! ip->i_par && ! ip->i_psr) {
			set_user_error (EPIPE);

			/*
			 * SIGPIPE is actually only meant to be sent for a
			 * pipe, not a FIFO, under R4 semantics. Since
			 * filesystem pipes are a aberration, we don't need to
			 * fix this.
			 */

			sendsig (SIGPIPE, SELF);
			break;
		}

		if ((n = PIPE_MAX - ip->i_pwx) > ioc)
			n = ioc;

		if (n > PIPE_MAX - ip->i_pnc)
			n = PIPE_MAX - ip->i_pnc;

		if (n == 0 || (ioc <= PIPE_MAX && n != ioc)) {
			/*
			 * If we are in O_NDELAY mode, just return and all
			 * uwrite () will see is 0 bytes written.
			 */
			if ((iop->io_flag & IONDLY) != 0)
				break;

			/*
			 * If we are in O_NONBLOCK mode, set_user_error () so that
			 * the return from system-call code will force the
			 * return value of uwrite () to -1.
			 */

			if ((iop->io_flag & IONONBLOCK) != 0) {
				set_user_error (EAGAIN);
				break;
			}
			if (psleep (ip, IFWFR) < 0)
				break;
			continue;
		}

		iop->io_ioc = n;
		iop->io_seek = ip->i_pwx;
		fwrite (ip, iop);
		n -= iop->io_ioc;
		if ((ip->i_pwx += n) == PIPE_MAX)
			ip->i_pwx = 0;
		ip->i_pnc += n;
		ioc -= n;

		if (ip->i_pnc > 0)
			pwake (ip, IFWFW);
	}

	iop->io_ioc = ioc;
}


/*
 *  ppoll(ip, ev)  --  Poll the given pipe inode.
 *  INODE *ip  --  The inode in question.
 *  int ev     --  The event bit field.
 *  int msec   --  Number of msecs to wait.
 *  Returns or'ed bits according to the following rules:
 *  POLLIN:  indicates input is available for reading, notice it is possible
 *	     to read even if there are no more writers anywhere!
 *  POLLOUT: indicates room in pipe for new output, notice it is not possible
 *	     to write unless there is a reader attached!
 *
 *  No priority polls are supported.
 */

#if	__USE_PROTO__
int ppoll (struct inode * ip, int ev, int msec)
#else
int
ppoll (ip, ev, msec)
struct inode  *	ip;
int		ev;
int		msec;
#endif
{
	int rval = 0;

	if (ev & POLLIN) {
		if (ip->i_pnc > 0)
			rval |= POLLIN;
		if (! ip->i_paw && ! ip->i_psw)
			rval |= POLLHUP;
		if (rval == 0 && msec != 0)
			pollopen (& ip->i_iev);
	}

	if (ev & POLLOUT) {
		if (ip->i_pnc < PIPE_MAX && (ip->i_par || ip->i_psr))
			rval |= POLLOUT;
		if (! ip->i_par && ! ip->i_psr)
			rval |= POLLHUP;
		if (rval == 0 && msec != 0)
			pollopen (& ip->i_oev);
	}

	return rval;
}
