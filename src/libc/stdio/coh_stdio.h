/*
 * coh_stdio.h
 * COHERENT Standard Input/Output library internals header.
 * The internal names defined here are kept in a separate header
 * to avoid pollution of the user namespace.
 */

#ifndef	__COH_STDIO_H__

#include <stdio.h>

/* Internal functions. */
extern	void	_dassign();
extern	int	_dscan	();
extern	char *	_dtefg	();
extern	void	_dtoa	();
extern	int	_fgetb	();
extern	int	_fgetc	();
extern	int	_fgete	();
extern	int	_fgetstr();
extern	int	_fginit	();
extern	int	_filbuf	();
extern	void	_finish	();
extern	int	_flsbuf	();
extern	FILE *	_fopen	();
extern	int	_fpinit	();
extern	int	_fpseek	();
extern	int	_fputa	();
extern	int	_fputb	();
extern	int	_fputc	();
extern	int	_fpute	();
extern	int	_fputt	();
extern	int	_scanf	();
extern	FILE *	_stropen();

#endif

/* end of coh_stdio.h */
