#include <stdio.h>
#include <sys/types.h>
#include <errno.h>

extern	char *_brk();

#if Z8001
paddr_t
vtop(cp)
register unsigned long cp;
{
	register unsigned int offs = cp;

	cp >>= 8;
	cp &= 0x7F0000L;
	return(cp | offs);
}

char *
ptov(p)
register unsigned long p;
{
	register unsigned int offs = p;

	p <<=8;
	p &= 0x7F000000L;
	return(p | offs);
}
#else
#define vtop(A) ((paddr_t)(A))
#define ptov(A) ((char *)(A))
#endif

#if IAPX86
static	char	*minbrk = NULL;		/* must init at run time */
static	char	*actbrk = NULL;
static	char	*reqbrk = NULL;
#else
extern	char	end[];			/* end of loaded private data */
static	char	*minbrk = end;		/* can't brk before end */
static	char	*actbrk = end;		/* actual end of private data */
static	char	*reqbrk = end;		/* requested (virtual) end of p.d. */
#endif

int
brk(newbrk)
register char *newbrk;
{
	register char *abrk;

	errno = 0;
	newbrk += (int)newbrk & 1;
#if IAPX86
	if (minbrk == NULL)
		minbrk = actbrk = reqbrk = _brk(NULL);
#endif
	if (newbrk < minbrk) {
		errno = EINVAL;
		return(-1);
	}
	abrk = _brk(newbrk);
	if (errno)
		return(-1);
	if (abrk < newbrk) {
		errno = ENOMEM;
		return(-1);
	}
	reqbrk = newbrk;
	actbrk = abrk;
	return(0);
}

char *
sbrk(incr)
register unsigned int incr;
{
	register paddr_t rbrk;

#if IAPX86
	if (minbrk == NULL)
		minbrk = actbrk = reqbrk = _brk(NULL);
#endif
	incr += incr & 1;
	rbrk = vtop(reqbrk);
	if (rbrk + incr <= vtop(actbrk)) {
		reqbrk = ptov(rbrk + incr);
		return(ptov(rbrk));
	}
#if Z8001
	if ((rbrk+incr ^ rbrk) > 65536L  &&  (int)rbrk+incr) {
		rbrk += 65536L;
		rbrk &= 0xFFFF0000L;
		reqbrk = ptov(rbrk);
	}
#endif
	if (brk(ptov(rbrk + incr)))
		return((char *)-1);
	return(ptov(rbrk));
}
