/*
 * libc/string/bzero.c
 * BSD version of memset(), included for BSD source compatability.
 */

#include <sys/compat.h>
#include <string.h>

#undef	bzero

EXTERN_C_BEGIN

void		bzero		PROTO ((VOID * dest, size_t count));

EXTERN_C_END


#if	USE_PROTO
void bzero (VOID * dest, size_t count)
#else
void
bzero (dest, count)
VOID	      *	dest;
size_t		count;
#endif
{
	memset (dest, 0, count);
}

/* end of libc/string/bzero.c */
