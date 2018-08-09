/*
 * libc/gen/getspent.c
 * Coherent I/O Library.
 * Get shadow password file entry.
 * Searches by next entry or name.
 */

#include <stdio.h>
#include <shadow.h>

#define field(x)	{ x=cp; while (*cp++); }
#define	NPWLINE	120
#define	PWFILE	"/etc/shadow"

static	char	pwline[NPWLINE];
static	struct	spwd sp;
static	FILE	*pwfile	= { NULL };

struct spwd *
getspnam(name)
char *name;
{
	register struct spwd *spp;

	setspent();
	while ((spp = getspent()) != NULL)
		if (streq(name, spp->sp_name))
			return spp;
	return NULL;
}

struct spwd *
getspent()
{
	register char *cp, *xp;
	register int c, nseps;

	if (pwfile == NULL)
		if ((pwfile = fopen(PWFILE, "r")) == NULL)
			return NULL;
again:
	cp = pwline;
	for (nseps = 0; (c = getc(pwfile))!=EOF && c!='\n'; ) {
		if (c == ':') {
			c = '\0';
			++nseps;
		}
		if (cp < &pwline[NPWLINE-1])
			*cp++ = c;
	}
	if (c == EOF)
		return NULL;
	if (nseps != 6)
		goto again;		/* accept only lines with six ':'s */
	*cp = '\0';
	cp = pwline;
	field(sp.sp_name);
	field(sp.sp_passwd);
	field(xp);
	sp.sp_uid = atoi(xp);
	field(xp);
	sp.sp_gid = atoi(xp);
	field(sp.sp_comment);
	sp.sp_gecos = sp.sp_comment;
	field(sp.sp_dir);
	field(sp.sp_shell);
	return &sp;
}

void
setspent()
{
	if (pwfile != NULL)
		rewind(pwfile);
}

void
endspent()
{
	if (pwfile != NULL) {
		fclose(pwfile);
		pwfile = NULL;
	}
}

/* end of getspent.c */
