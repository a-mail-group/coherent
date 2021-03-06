/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef	__SYS_COHERENT_H__
#define	__SYS_COHERENT_H__

/*
 * Note that including this renders driver code nonportable, and ties a
 * driver to a single version of COHERENT.
 */

#define	_KERNEL		1

#include <common/__paddr.h>
#include <common/_null.h>
#include <common/_time.h>
#include <kernel/_sleep.h>
#include <kernel/alloc.h>
#include <kernel/timeout.h>
#include <kernel/reg.h>
#if	0
#include <kernel/param.h>
#include <kernel/trace.h>
#endif
#include <sys/filsys.h>
#include <sys/types.h>
#include <sys/mmu.h>
#include <sys/uproc.h>
#include <sys/con.h>

#define kclear(buf, nbytes)		memset(buf, 0, nbytes)
#define kkcopy(src, dest, nbytes)	(memcpy(dest, src, nbytes),nbytes)

/*
 * This is for dealing with the pre-4.2 version of vtop ().  Note that under
 * the DDI/DKI, vtop () takes an extra parameter rather than doing the
 * "system global" manipulation.
 */

#define	vtop(addr)	__coh_vtop (addr)

/* prototypes from k0.s */
int	sphi		__PROTO ((void));
int	splo		__PROTO ((void));
int	spl		__PROTO ((int s));

/* prototypes from md.c */
void	__kdenabio	__PROTO ((void));
void	__kddisabio	__PROTO ((void));
void	__kdaddio	__PROTO ((unsigned short));
void	__kddelio	__PROTO ((unsigned short));

#endif	/* ! defined (__SYS_COHERENT_H__) */
