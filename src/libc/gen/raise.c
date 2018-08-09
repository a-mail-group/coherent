/*
 * libc/gen/raise.c
 * raise()
 * POSIX function.
 */

#include <signal.h>
#include <unistd.h>

#ifdef USE_PROTO
int raise (int sig)
#else
int
raise(sig)
int sig;
#endif
{
	return kill(getpid(), sig);
}

/* end of libc/gen/raise.c */
