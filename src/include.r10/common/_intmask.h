/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef	__COMMON__INTMASK_H__
#define	__COMMON__INTMASK_H__

/*
 * This internal header defines a data type used to hold interrupt-masking
 * information.  It is not formally specified whether this type is arithmetic
 * or non-arithmetic, and no portable uses for it have been defined.  It
 * exists in the common space for use by the kernel configuration mechanisms
 * included in the base system.
 */

/*
 * If we are using interrupt masking to define our interrupt priority scheme,
 * then we need a type sufficient to hold a bitmask for at least 16 vectors
 * (for the IBM PC). Defining this as a long integer should be sufficient
 * for now.
 */

typedef	unsigned long	intmask_t;

#endif	/* ! defined (__COMMON__INTMASK_H__) */

