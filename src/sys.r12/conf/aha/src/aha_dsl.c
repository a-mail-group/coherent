/* $Header: /ker/io.386/RCS/aha_dsl.c,v 2.3 93/08/19 04:02:01 nigel Exp Locker: nigel $ */
/*
 * aha_dsl.c - routines for manipulating AHA-154x Data Segment Lists.
 * Part of the Adaptec SCSI driver.
 *
 * $Log:	aha_dsl.c,v $
 * Revision 2.3  93/08/19  04:02:01  nigel
 * Nigel's R83
 */

#include <sys/coherent.h>
#include <sys/buf.h>
#include <sys/scsiwork.h>
#include <sys/aha154x.h>
#include <sys/errno.h>

#ifndef LESSER
#define LESSER(a, b)	(((a)<(b))?(a):(b))
#endif /* LESSER */

extern unsigned long aha_p3_to_l();

/*
 * void
 * dsl_free( P3 dsl_ptr )
 *
 * Free a DSL generated by dsl_gen().
 */
void
dsl_free( dsl_ptr )
	P3 dsl_ptr;
{
	caddr_t dsl_vaddr;	/* Virtual address of DSL.  */

	dsl_vaddr = mem_recall (aha_p3_to_l (dsl_ptr));
	pfree (dsl_vaddr);
	mem_forget (dsl_vaddr);
}


#define MAX_ENTRIES	16	/* At most 16 DSL entries are allowed.  */

/*
 * void
 * dsl_gen(P3 new_dsl, paddr_t buffer, unsigned len)
 *
 * Generate a Data Segment List for virtual-physical buffer 'buffer' with
 * length 'len'.  Return a P3 pointer to this DSL in new_dsl.
 */
void
dsl_gen( new_dsl, new_len, buffer, len)
	P3 new_dsl, new_len;
	paddr_t buffer;
	unsigned long len;
{
	unsigned first;		/* Size of first chunk in bytes.  */
	unsigned rest;		/* len - first */
	unsigned middle;	/* Size of middle (aligned) chunk in bytes. */
	unsigned middle_clicks;	/* middle/NBPC */
	unsigned last;		/* Size of last chunk in bytes.  */

	unsigned total_entries;	/* Total number of entries in DSL.  */
	unsigned table_len;	/* total_entries * sizeof(DSL_ENTRY) */

	DSL_ENTRY *tmp_dsl;	/* Build DSL here.  */

	unsigned len_to_stuff;	/* How much buffer left to stuff into DSL?  */
	unsigned addr_to_stuff;	/* What address goes in the current DSL entry?  */

	int i;			/* Handy counter/index.  */

	/*
	 * The buffer can have up to three sections:
	 * first	-- from the start of the buffer
	 *		   to the start of the next click.
	 * middle	-- all full sized clicks
	 * last		-- the final partial click.
	 *
	 * A buffer must have at least a first section, may have only
	 * a first and last, first and middle, or may have all three.
	 */

	/*
	 * Calculate the sizes for all three sections in bytes.
	 */

	/*
	 * The first chunk is either the entire buffer, or up to then
	 * end of the first click.
	 */
	first = LESSER(len, ctob(btocrd(buffer + NBPC)) - buffer);

	rest = len - first;
	middle_clicks = rest / NBPC;	/* First calculate length in clicks.  */
	middle = middle_clicks * NBPC;	/* Convert to bytes.  */

	last = rest % NBPC;

	/* How many entries do we need in the DSL?  */
	total_entries = ((first > 0)?1:0) + middle_clicks + ((last > 0)?1:0);

	/* The AHA-154xB permits no more than MAX_ENTRIES DSL entries.  */
	if ( total_entries > MAX_ENTRIES ) {
		set_user_error (EINVAL);
		return;
	}

	/* How big a table do we need?  */
	table_len = total_entries * sizeof(DSL_ENTRY);

	/* Allocate the DSL.  */
	if (NULL == (tmp_dsl = (DSL_ENTRY *) palloc (table_len))) {
		set_user_error (ENOMEM);
		return;
	}
	
	/* Fill in the DSL.  */
	len_to_stuff = len;
	addr_to_stuff = buffer;

	/* First Part */
	aha_l_to_p3 (first, tmp_dsl [0].size);
	aha_l_to_p3 (vptop (addr_to_stuff), tmp_dsl [0].addr);

	len_to_stuff -= first;
	addr_to_stuff += first;

	/* Middle Part */
	i = 1;
	while (len_to_stuff > NBPC) {
		aha_l_to_p3 (NBPC, tmp_dsl [i].size);
		aha_l_to_p3 (vptop (addr_to_stuff), tmp_dsl [i].addr);

		len_to_stuff -= NBPC;
		addr_to_stuff += NBPC;
		++ i;
	}

	/* Last Part */
	if (len_to_stuff > 0) {
		aha_l_to_p3 (last, tmp_dsl [i].size);
		aha_l_to_p3 (vptop (addr_to_stuff), tmp_dsl [i].addr);

		len_to_stuff -= last;
		addr_to_stuff += last;
		++ i;
	}

	/* Fill in return values.  */
	aha_l_to_p3 (table_len, new_len);
	aha_l_to_p3 (vtop (tmp_dsl), new_dsl);
	mem_remember (tmp_dsl, aha_p3_to_l (new_dsl));
}
