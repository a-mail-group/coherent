/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef	__VARARGS_H__
#define	__VARARGS_H__

/*
 * This header defines variable-argument functions.  If you are writing new
 * code, use the functions defined in <stdarg.h> instead.
 */

typedef	char *va_list;
#define	va_dcl			int va_alist;
#define	va_start(ap)		ap = (va_list)&va_alist
#define	va_arg(ap, type)	(((type *)(ap += sizeof(type)))[-1])
#define	va_end(ap)

#endif	/* ! defined (__VARARGS_H__) */

