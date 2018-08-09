/*
 * libc/stdio/ctermid.c
 * ctermid()
 * POSIX.1 - 4.7.1.
 * Return controlling terminal id, namely "/dev/tty".
 * Cf. Stevens, APUE, p. 346.
 */

#include <stdio.h>
#include <string.h>

/* Local data. */
static	char	ctermid_name[L_ctermid];

char *
ctermid(s) register char *s;
{
	if (s == NULL)
		s = ctermid_name;
	return strcpy (s, "/dev/tty");
}

/* end of libc/stdio/ctermid.c */
