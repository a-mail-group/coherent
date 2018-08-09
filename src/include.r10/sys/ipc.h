/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef __SYS_IPC_H__
#define	__SYS_IPC_H__

#include <common/feature.h>
#include <common/_ipcperm.h>

/*
 * IPC Mode bits.
 */

#define	__IPC_ALLOC	0x8000		/* entry currently allocated */
#define	IPC_CREAT	0x0200		/* create entry if key doesn't exist */
#define	IPC_EXCL	0x0400		/* fail if key exists */
#define	IPC_NOWAIT	0x0800		/* error if request must wait */


/*
 * IPC Keys.
 */

#define IPC_PRIVATE	((key_t) 0)

/*
 * IPC Control Commands.
 */

#define	IPC_RMID	0		/* remove identifier */
#define	IPC_SET		1		/* set options */
#define	IPC_STAT	2		/* get options */

#if	_KERNEL

#include <common/ccompat.h>

/*
 * Common framework for IPC permissions-checking.
 */

__EXTERN_C_BEGIN__

int		ipc_cred_match	__PROTO ((struct ipc_perm * _ipcp));
int		ipc_lack_perm	__PROTO ((struct ipc_perm * _ipcp,
					  __mode_t _mode));
void		ipc_perm_init	__PROTO ((struct ipc_perm * _ipcp,
					  __mode_t _mpde));

__EXTERN_C_END__

#endif	/* _KERNEL */

#endif	/* ! defined (__SYS_IPC_H__) */
