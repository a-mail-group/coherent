/* (-lgl
 *	Coherent 386 release 4.2-Beta
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */
/*
 * /usr/include/sys/tape.h
 *
 * Mon Nov  1 18:09:51 1993 CST
 */

#ifndef __SYS_TAPE_H__
#define __SYS_TAPE_H__

#define T_BASE		('t' << 8)
#define T_RETENSION	(T_BASE | 0x01) /* Retension tape */
#define T_RWD		(T_BASE | 0x02) /* Rewind tape */
#define T_ERASE		(T_BASE | 0x03) /* Erase tape */
#define T_WRFILEM	(T_BASE | 0x04) /* Write file marks -- not used */
#define T_RST		(T_BASE | 0x05) /* Reset */
#define T_RDSTAT	(T_BASE | 0x06) /* Read status */
#define T_SFF		(T_BASE | 0x07)	/* Space Filemark Forward */
#define T_SBF		(T_BASE | 0x08) /* Space Block Forward -- not used */
#define T_LOAD		(T_BASE | 0x09)	/* Load -- not used */
#define T_UNLOAD	(T_BASE | 0x0A)	/* Unload -- not used */
#define T_SFREC		(T_BASE | 0x0B)	/* Not used */
#define T_SBREC		(T_BASE | 0x0C)	/* Not used */
#define T_TINIT		(T_BASE | 0x0D)	/* Not used */
#define T_SFB		(T_BASE | 0x28)	/* Space Filemark Backward */
#define T_SBB		(T_BASE | 0x29)	/* Space Block Backward -- not used */
#define T_FORMAT	(T_BASE | 0x0E)	/* Format tape -- floppy tape only */

#define T_DEFAULT "/dev/tape"

#endif				/* __SYS_TAPE_H__ */
