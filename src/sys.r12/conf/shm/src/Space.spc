/*
 * Configurable information for COHERENT shared memory.
 */

#define __KERNEL__	 1

#include "conf.h"

/* Maximum # of shared memory segments, systemwide. */
int		SHMMNI = SHMMNI_SPEC;

/* Max size in bytes of shared memory segment. */
int		SHMMAX = SHMMAX_SPEC;
