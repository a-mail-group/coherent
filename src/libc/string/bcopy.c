/*
 * libc/string/bcopy.c
 * BSD version of memmove(), included for BSD source compatability.
 */

#include <sys/compat.h>
#include <string.h>

#undef	bcopy

EXTERN_C_BEGIN

void		bcopy		PROTO ((VOID * src, VOID * dest,
					size_t count));

EXTERN_C_END


#if	USE_PROTO
void bcopy (VOID * src, VOID * dest, size_t count)
#else
void
bcopy (src, dest, count)
VOID	      *	src;
VOID	      *	dest;
size_t		count;
#endif
{
	memmove (dest, src, count);
}

/* end of libc/string/bcopy.c */
