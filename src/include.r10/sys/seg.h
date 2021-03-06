/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef	 __SYS_SEG_H__
#define	 __SYS_SEG_H__

#include <common/feature.h>
#include <common/__paddr.h>
#include <common/__caddr.h>
#include <common/__off.h>
#include <common/__daddr.h>
#include <sys/ksynch.h>

#if	! _KERNEL
# error	You must be compiling the kernel to use this header
#endif

#ifndef	__CSEG_T
#define	__CSEG_T
typedef	long		cseg_t;
#endif

/*
 * Segment structure.
 */
typedef struct seg {
	struct seg    *	s_forw;		/* Forward pointer */
	struct seg    *	s_back;		/* Backward pointer */
	struct inode  *	s_ip;		/* Inode pointer for shared text */
	short		s_flags;	/* Flags */
	short		s_urefc;	/* Reference count of segment */
	short		s_lrefc;	/* Lock reference count */

	__off_t		s_size;		/* Size in bytes */
	cseg_t	      *	s_vmem;		/* page table address */

	__daddr_t	s_daddr;	/* Disk base address */
} SEG;


/*
 * Flags (s_flags).
 */

#define SFCORE	0x0001			/* Memory resident */
#define	SFDOWN	0x0002			/* Segment grows downward */
#define SFSHRX	0x0004			/* Shared segment */
#define SFTEXT	0x0008			/* Text segment */
#define SFHIGH	0x0010			/* Text segment */
#define	SFSYST	0x0020			/* System segment */


/*
 * Pseudo flags.  (passed to salloc).
 */

#define	SFNSWP	0x4000			/* Don't swap */
#define SFNCLR	0x8000			/* Don't clear segment */


/*
 * Functions.
 */

extern	SEG	*segdupl();		/* seg.c */
extern	SEG	*ssalloc();		/* seg.c */
extern	SEG	*salloc();		/* seg.c */
extern	SEG	*smalloc();		/* seg.c */
extern	SEG	*shalloc();		/* seg.c */


/*
 * Open segment structure.
 */

typedef struct sr {
	int		sr_flag;	/* Flags for this reference */
	__caddr_t	sr_base;	/* Virtual address base */
	__off_t		sr_size;	/* Mapped in window size */
	struct seg    *	sr_segp;	/* Segment pointer */
} SR;


/*
 * Flags (sr_flag).
 */

#define SRFPMAP	0x01		/* Segment is mapped in process */
#define SRFDUMP	0x02		/* Dump segment */
#define	SRFDATA	0x04		/* Data segment */
#define	SRFRODT	0x08		/* Data, read-only (used by shm) */
#define	SRFBERM	0x10		/* Segment to be removed (used by shm) */


/*
 * Number of user segments.
 */

#define NUSEG	4
#define NSHMSEG	6


/*
 * Segment indices.
 */

#define SIUSERP	0			/* User area segment */
#define	SISTEXT	1			/* Shared text segment */
#define SIPDATA	2			/* Private data segment */
#define SISTACK	3			/* Stack segment */

#define	SIBSS	0			/* overlay of SIUSERP [coh/exec.c] */

/*
 * Declare and initialize an in-memory segment structure.
 */
#define	MAKESR(sr, seg) SEG seg; SR sr = { 0, 0, 0, &seg }

#endif	/* ! defined (__SYS_SEG_H__) */
