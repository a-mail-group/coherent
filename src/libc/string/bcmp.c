/*
 * libc/string/bcmp.c
 * BSD version of memcmp(), included for BSD source compatability.
 */

#include <sys/compat.h>
#include <string.h>

#undef	bcmp

EXTERN_C_BEGIN

int		bcmp		PROTO ((VOID * src, VOID * dest,
					size_t count));

EXTERN_C_END


#if	USE_PROTO
int bcmp (VOID * src, VOID * dest, size_t count)
#else
int
bcmp (src, dest, count)
VOID	      *	src;
VOID	      *	dest;
size_t		count;
#endif
{
	return memcmp (dest, src, count);
}

/* end of libc/string/bcmp.c */
