#ifndef	__COH_TIMEOUT_H__
#define	__COH_TIMEOUT_H__

#include <kernel/timeout.h>

/* prototypes from timeout.c */
void	timeout		__PROTO ((TIM * tp, unsigned int n,
			  int (*f)(int a), int a));

#endif	/* ! defined (__COH_TIMEOUT_H__) */
