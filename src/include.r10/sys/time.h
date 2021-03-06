/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef	__SYS_TIME_H__
#define	__SYS_TIME_H__

#include <common/feature.h>
#include <common/__time.h>
#include <common/_timestr.h>

#if	! _POSIX_C_SOURCE
/*
 * The System V, Release 4 ABI requires that time_t be defined here, as well
 * as CLK_TCK and "struct tm".
 */

#include <common/_time.h>
#include <common/_clktck.h>
#include <common/_tm.h>

typedef __timestruc_t	timestruc_t;

#endif	/* ! _POSIX_C_SOURCE */

#endif	/* ! defined (__SYS_TIME_H__) */

