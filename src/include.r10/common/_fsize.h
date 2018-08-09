/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef	__COMMON__FSIZE_H__
#define	__COMMON__FSIZE_H__

/*
 * This internal header file defines the COHERENT data type "fsize_t".  This
 * type abstracts the notion of file size for COHERENT utilities, to aid in
 * migration to file systems capable of representing files more than 2^32-1
 * bytes.
 *
 * Historically, COHERENT has exported this type to the user through the
 * <sys/types.h> header, but it has also been implicitly defined by various
 * other headers; there has never been any documented point of export.
 *
 * This type is never exported to the user if _POSIX_C_SOURCE is defined.
 * Macros for accessing items for this type are not yet defined; users of this
 * type should be aware that it may not remain a scalar type.  Use of this type
 * in portable software is strongly discouraged.
 */

#include <common/feature.h>
#include <common/__fsize.h>

#if	! _POSIX_C_SOURCE

typedef	__fsize_t	fsize_t;

#endif	/* ! _POSIX_C_SOURCE */

#endif	/* ! defined (__COMMON__FSIZE_H__) */
