/*
 * Get entries from name list.
 */

#include <stdio.h>
#include <l.out.h>
#include <canon.h>
#include <common/_fsize.h>

nlist(fn, nlp)
char *fn;
struct nlist *nlp;
{
	register struct nlist *np;
	register int ntodo = 0;
	struct FILE *lfp;
	struct ldheader lh;
	struct ldsym ste;
	fsize_t symsize;
	register int n;

	for (np = nlp; np->n_name[0] != '\0'; np++) {
		np->n_type = np->n_value = 0;
		ntodo++;
	}
	if ((lfp = fopen(fn, "r")) == NULL)
		return;
	n = fread(&lh, sizeof lh, 1, lfp);
	canint(lh.l_magic);
	if (n!=1 || lh.l_magic!=L_MAGIC) {
		fclose(lfp);
		return;
	}
	for (n=0; n<=L_SYM; n++)
		cansize(lh.l_ssize[n]);
	symsize = sizeof lh + lh.l_ssize[L_SHRI] + lh.l_ssize[L_SHRD]
	    + lh.l_ssize[L_PRVI] + lh.l_ssize[L_PRVD] + lh.l_ssize[L_DEBUG];
	fseek(lfp, symsize, 0);
	symsize = lh.l_ssize[L_SYM];
	for ( ; symsize>0 && ntodo; symsize -= sizeof ste) {
		if (fread(&ste, sizeof ste, 1, lfp) != 1)
			break;
		for (np = nlp; np->n_name[0] != '\0'; np++)
			if (strncmp(np->n_name, ste.ls_id, NCPLN) == 0) {
				canint(ste.ls_type);
				canvaddr(ste.ls_addr);
				np->n_type = ste.ls_type;
				np->n_value = ste.ls_addr;
				if (--ntodo == 0)
					break;
			}
	}
	fclose(lfp);
}
