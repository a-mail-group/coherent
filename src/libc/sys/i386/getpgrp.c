/*
 * libc/sys/i386/getpgrp.c
 * Inspired by Bureau d'Etudes Ciaran O'Donnell,1987,1990,1991
 */

#include <sys/compat.h>
#include <sys/types.h>

#define	_GETPGRP	0
#define	_SETPGRP	1
#define	_SETPGID	2
#define	_SETSID		3
#define	_RESERVED4	4
#define	_RESERVED5	5

#ifdef USE_PROTO
pid_t getpgrp(VOID)
#else
pid_t
getpgrp()
#endif
{
	return (pid_t)_pgrp(_GETPGRP);
}

#ifdef USE_PROTO
pid_t setpgrp(VOID)
#else
pid_t
setpgrp()
#endif
{
	return _pgrp(_SETPGRP);
}

#ifdef USE_PROTO
int setpgid(pid_t pid, pid_t pgid)
#else
int
setpgid(pid, pgid)
pid_t		pid;
pid_t		pgid;
#endif
{
	return (int)_pgrp(_SETPGID, pid, pgid);
}

#ifdef USE_PROTO
pid_t setsid(VOID)
#else
pid_t
setsid()
#endif
{
	return _pgrp(_SETSID);
}

/* end of libc/sys/i386/getpgrp.c */
