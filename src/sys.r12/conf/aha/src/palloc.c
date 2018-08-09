#include <common/ccompat.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <stddef.h>

#define	__KERNEL	1

#include <kernel/alloc.h>
#include <kernel/param.h>
#include <kernel/trace.h>
#include <kernel/reg.h>
#include <sys/types.h>


/*
 * unsigned char *palloc(int size);
 *
 * Allocate 'size' bytes of kernel space, which does not cross a click
 * boundary.  Returns a pointer to the space allocated on success,
 * NULL on failure.
 *
 * Allocate twice as much memory as we need, and then return a chunk that
 * does not cross a click boundary.  Immediately before the chunk that
 * we return, we store the true address of the chunk that was kalloc()'d.
 *
 * Since this routine is for relatively small short-lived objects,
 * which we expect to allocate frequently, speed is more important than
 * space overhead.
 *
 * We assume that kalloc() returns word aligned addresses.
 *
 * There are two cases:
 * There is enough room before the click boundary (or there is no click
 * 	boundary) for the pointer and the memory we need.
 * Otherwise, return the chunk starting at the click boundary, storing
 *	the pointer right before the click boundary.  This trick allows
 *	us to allocate up to 1 full click.
 *
 * If kalloc() did NOT return word aligned chunks, then there would be
 * a third case, where there might not be enough space for the pointer
 * before the click boundary.
 */

#define c_boundry(x)	ctob(btocru((x)+1)) /* Next click boundary above x.  */

#if	__USE_PROTO__
caddr_t palloc (size_t size)
#else
caddr_t
palloc (size)
size_t		size;
#endif
{
	caddr_t	 local_arena;	/* Value returned by kalloc() */
	caddr_t	 boundry;	/* Next click boundry above local_arena */
	caddr_t	 retval;	/* What we give back to our caller */

	ASSERT (size <= NBPC);

	/* Fetch twice as much space as requested, plus a pointer.  */
	if ((local_arena = kalloc (sizeof (caddr_t) + 2 * size)) == NULL)
		return NULL;
	
	boundry = (caddr_t) c_boundry (local_arena);

	/* First case:  enough space before the boundry.  */
	if (boundry - local_arena >= size + sizeof(caddr_t)) {

		* (caddr_t *) local_arena = local_arena;
		retval = local_arena + sizeof(caddr_t);
	} else if (boundry - local_arena < sizeof(caddr_t)) {
		/*
		 * Second case: There is not enough space before the
		 * boundry for the whole pointer.
		 */

		* (caddr_t *) local_arena = local_arena;
		retval = local_arena + sizeof(caddr_t);
	} else {

		* (caddr_t *) (boundry - sizeof(caddr_t)) = local_arena;
		retval = boundry;
	}

	return retval;
}

/*
 * __VOID__ pfree(__VOID__ *ptr);
 * Free the chunk of memory 'ptr' allocated by palloc().
 *
 * Note that 'ptr' is really a __VOID__ *, but we call it __VOID__ **
 * to simplify arithmetic.
 *
 * The address returned by kalloc() is stored immediately
 * before the chunk returned by palloc().
 */

#if	__USE_PROTO__
void pfree (caddr_t ptr)
#else
void
pfree (ptr)
caddr_t		ptr;
#endif
{
	T_PIGGY (0x2000, cmn_err (CE_CONT, "pfree(%x):kfree(%x), ", ptr,
			 	  * ((caddr_t *) ptr - 1)));
	kfree (* ((caddr_t *) ptr - 1));
}

