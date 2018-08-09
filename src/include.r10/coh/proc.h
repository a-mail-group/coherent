#ifndef	__COH_PROC_H__
#define	__COH_PROC_H__

#include <sys/sched.h>

/* prototypes from proc.c */
__sleep_t x_sleep	__PROTO ((char * event, int schedPri,
			  int sleepPri, char * reason));

#endif	/* ! defined (__COH_PROC_H__) */
