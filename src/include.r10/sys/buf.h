/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef  __SYS_BUF_H__
#define  __SYS_BUF_H__

#include <common/feature.h>
#include <common/__paddr.h>
#include <common/__caddr.h>
#include <common/__daddr.h>
#include <common/__clock.h>
#include <common/__off.h>
#include <common/_uid.h>
#include <kernel/__buf.h>

#include <sys/ksynch.h>

#if	! _KERNEL
# error	You must be compiling the kernel to use this header!
#endif


typedef	void	     (*	__iodone_t)	__PROTO ((__buf_t * _bp));


/*
 * Complete the incomplete type declaration in <common/_buf.h>
 */

struct buf {
	int		b_flags;
	__buf_t	      *	b_forw;		/* Kernel/Driver list link */
	__buf_t	      *	b_back;		/* Kernel/Driver list link */
	__buf_t	      *	av_forw;	/* Driver work list link */
	__buf_t	      *	av_back;	/* Driver work list link */

	__off_t		b_bcount;	/* Size of I/O */

	union {
		__caddr_t	b_addr;	/* Buffer's virtual address */
	} b_un;

	__daddr_t	b_blkno;	/* Block number */
	__off_t		b_resid;	/* Driver returns count here */
	__clock_t	b_start;	/* Request start time */
	struct proc   *	b_proc;		/* Process structure address */
	long		b_bufsiz;	/* Size of allocated buffer */
	__iodone_t	b_iodone;
	n_dev_t		b_edev;		/* Expanded dev field */

	__VOID__      *	b_private;	/* For driver's use */

	/*
	 * The following fields are for non-DDI/DKI block drivers only.
	 */

	union {
		struct {
			__paddr_t	_b_paddr;/* system global address */
			o_dev_t		_b_dev;	/* Device (old-style) */
			char		_b_req;	/* I/O type */
		} b_coh;
		unsigned long	_reserved [2];
	} _reserved;


	/*
	 * The following fields are for kernel internal use only.
	 */

	unsigned short	b_errno;	/* Error number */


	/*
	 * The following are kernel internal fields for the non-DDI/DKI
	 * buffer cache implementation.
	 */

	unsigned long	b_hashval;	/* used to index into hasharray[] */

	__buf_t	      *	b_LRUf;		/* Next (older) in LRU chain */
	__buf_t	      *	b_LRUb;		/* Previous (newer) in LRU chain */

	__DUMB_GATE	__b_gate;
};


/*
 * Use these to lock things.
 */

#define	__INIT_BUFFER_LOCK(bp) \
		__GATE_INIT ((bp)->__b_gate, "buffer", 0)
#define	__LOCK_BUFFER(bp, where) \
		(__GATE_LOCK ((bp)->__b_gate, "lock : buffer " where))
#define	__UNLOCK_BUFFER(bp) \
		(__GATE_UNLOCK ((bp)->__b_gate))
#define	__IS_BUFFER_LOCKED(bp) \
		(__GATE_LOCKED ((bp)->__b_gate) != 0)
#define	__MAKE_BUFFER_ASYNC(bp) \
		((bp)->b_flag |= BFASY, \
		 __GATE_TO_INTERRUPT ((bp)->__b_gate))

/*
 * Map pre-4.2 structure usage into present terms.
 */

#define	b_flag		b_flags 
#define	b_actf		av_forw
#define	b_actl		av_back
#define	b_bno		b_blkno
#define	b_count		b_bcount
#define	b_vaddr		b_un.b_addr
#define	b_paddr		_reserved.b_coh._b_paddr
#define	b_dev		_reserved.b_coh._b_dev
#define	b_req		_reserved.b_coh._b_req
#define	b_hash		b_forw
#define	b_sort		b_back


/*
 * Requests.
 */

#define	BREAD	1			/* Read */
#define BWRITE	2			/* Write */

/* The floppy driver uses these: */

#define	BFLSTAT	3			/* Floppy Drive Status */
#define BFLFMT	4			/* Floppy Disk Format */


/*
 * DDI/DKI buffer flags.
 */

#define	B_WRITE		0x0000		/* Pseudo-flag, different from B_READ */
#define	B_READ		0x0001		/* Read into main memory */
#define	B_PAGEIO	0x0002		/* Paged I/O */
#define	B_PHYS		0x0004		/* Physical I/O */


/*
 * Pre-4.2 flags
 */

#define	BFWAIT	0x0080			/* Buffer being waited for */
#define BFNTP	0x0100			/* Buffer not valid */
#define BFERR	0x0200			/* Error */
#define BFMOD	0x0400			/* Data has been modified */
#define BFASY	0x0800			/* Asynchrous */
#define BFRAW	0x1000			/* Request is raw */
#define BFTAP	0x2000			/* Request is to a tape */


/*
 * These exist so old sources can still compile.
 */

#define BFBLK	0			/* Aligned on a block boundary */
#define BFIOC	0			/* Perform bounds checking */


/*
 * Functions.
 */

__EXTERN_C_BEGIN__

void		bsync		__PROTO ((void));
void		bflush		__PROTO ((o_dev_t _dev));
void		brelease	__PROTO ((__buf_t * _bp));
__buf_t	      *	bread		__PROTO ((o_dev_t _dev, daddr_t _bno,
					  int _sync));
void		bwrite		__PROTO ((__buf_t * _bp, int _sync));
__buf_t	      *	bclaim		__PROTO ((o_dev_t _dev, daddr_t _bno,
					  long _bsize, int _sync));
void		bdone		__PROTO ((__buf_t * _bp));
void		bclean		__PROTO ((o_dev_t _dev, daddr_t _bno));

/*
 * DDI/DKI routines. Note that ngeteblk () and getrbuf () cannot be
 * implemented under the current buffer-cache management scheme, and will
 * become available with the demand-paging release of COHERENT.
 */

__buf_t	      *	geteblk		__PROTO ((void));
int		geterror	__PROTO ((__buf_t * _bp));
void		clrbuf		__PROTO ((__buf_t * _bp));
void		biodone		__PROTO ((__buf_t * _bp));
void		bioerror	__PROTO ((__buf_t * _bp, int _errno));
int		biowait		__PROTO ((__buf_t * _bp));
void		brelse		__PROTO ((__buf_t * _bp));

__EXTERN_C_END__


/*
 * Flags for 3rd argument to bclaim () and bread () and 2nd argument
 * to bwrite ().
 */

enum {
	BUF_ASYNC,
	BUF_SYNC
};


#define BSIZE	512U			/* Block size */

#define	blockn(n)		((n) >> 9)
#define	blocko(n)		((n) & (BSIZE - 1))
#define nbnrem(b)		((int) (b) & (BSIZE / 4U - 1))
#define nbndiv(b)		((b) / (BSIZE / 4U))


/*
 * This is only for non-DDI/DKI COHERENT drivers.
 */

typedef	__buf_t		BUF;

#endif	/* ! defined (__SYS_BUF_H__) */
