/*
 * req2.c
 * Nroff/Troff.
 * Requests (n-z).
 */

#include <ctype.h>
#include "roff.h"

#define A_BEL	0x07	/* Bell			*/
#define A_DEL	0x7F	/* Delete		*/

/*
 * Turn adjust mode off.
 */
req_na()
{
	adm = 0;
}

/*
 * Overstrike bold off.
 * !V7.
 */
req_nb()
{
	enb = 0;
}

/*
 * Need.
 */
req_ne(argc, argv) int argc; char *argv[];
{
	register int	need,
			have;

	need = number(argv[1], SMVLSP, SDVLSP, 0, 1, unit(SMVLSP, SDVLSP));
	if (cdivp->d_ctpp) {
		have = cdivp->d_ctpp->t_apos;
		if (have > pgl)
			have = pgl;
	} else
		have = pgl;
	have -= cdivp->d_rpos;
	if (have >= need || nsm != 0)
		return;
	setbreak();
	sspace(have);
}

/*
 * Go into no fill mode.
 */
req_nf()
{
	setbreak();
	fill = 0;
}

/*
 * Turn off hyphenation.
 */
req_nh()
{
	hyp = 0;		/* turn the durn thing off */
}

/*
 * Set number mode.				(.nm)	$$TO_DO$$
 */
req_nm(argc, argv) int argc; char *argv[];
{
#if	1
	printu(".nm");
#else
	long	smdigw, sddigw;

	smdigw = width('0') * swddiv;
	sddigw = swddiv;
	if ((lnn=number(argv[1], SMUNIT, SDUNIT, lnn, 0, 0)) <= 0) {
		lnn = 0;
		return;
	}
	if (argv[2][0] != '\0')
		lnm = number(argv[2], SMUNIT, SDUNIT, 0, 0, 1);
	if (argv[3][0] != '\0')
		lns = number(argv[3], smdigw, sddigw, 0, 0, unit(smdigw, sddigw));
	if (argv[4][0] != '\0')
		lni = number(argv[4], smdigw, sddigw, 0, 0, unit(smdigw, sddigw);
#endif
}

/*
 * Turn off line number mode for n lines.	(.nn)	$$TO_DO$$
 */
req_nn(argc, argv) int argc; char *argv[];
{
#if	1
	printu(".nn");
#else
	register int n;

	lnc = number(argv[1], SMUNIT, SDUNIT, 0, 0, 1);
#endif
}

/*
 * Define a number register.
 */
req_nr(argc, argv) int argc; char *argv[];
{
	register REG *rp;
	char name[2];

	argname(argv[1], name);
#if	0
	rp = getnreg(name, RNUMR);	/* too many paramters!	*/
#else
	rp = getnreg(name);
#endif
	rp->n_reg.r_nval = number(argv[2], SMUNIT, SDUNIT, rp->n_reg.r_nval, 0, 0);
	if (argc >= 4)
		rp->n_reg.r_incr = numb(argv[3], SMUNIT, SDUNIT);
	if (rp == nrpnreg)
		npn = pno + 1;
}

/*
 * Turn no-space mode on.
 */
req_ns()
{
	nsm = 1;
}

/*
 * End input from current file and switch to given file.
 */
req_nx(argc, argv) int argc; char *argv[];
{
	register FILE *fp;
	register STR *sp;

	sp = strp;
	do {
		if (sp == NULL) {
			printe("cannot find current file");
			return;
		}
	} while (sp->x2.s_type != SFILE);
	if ((fp = fopen(argv[1], "r")) == NULL) {
		printe("cannot open %s", argv[1]);
		return;
	}
	fclose(sp->x2.s_fp);
	sp->x2.s_fp = fp;
	nfree(sp->x2.s_fname);
	sp->x2.s_fname = duplstr(argv[1]);
	sp->x2.s_clnc = 1;
	sp->x2.s_nlnc = 1;
}

/*
 * Output saved space.
 */
req_os()
{
	sspace(svs);
}

/*
 * Set page number character in title.
 */
req_pc(argc, argv) int argc; char *argv[];
{
	tpc = argv[1][0];
}

/*
 * Pipe nroff output.				(.pi)	$$TO_DO$$
 */
req_pi(argc, argv) int argc; char *argv[];
{
	printu(".pi");
}

/*
 * Set page length.
 */
req_pl(argc, argv) int argc; char *argv[];
{
	pgl = number(argv[1], SMVLSP, SDVLSP, pgl, 0, unit(11*SMINCH, SDINCH));
	if (pgl == 0)
		pgl = unit(SMVLSP, SDVLSP);
}

/*
 * Print sizes of macros.
 */
req_pm(argc, argv) int argc; char *argv[];
{
	register REG	**rpp,
			*rp;
	unsigned	rnum,		/* number of registers */
			mnum,		/* number of macros */
			csize;		/* total space */
	int		vflag;

	vflag = (argv[1][0] == '\0');
	if (vflag)
		fprintf(stderr, "Macro and register sizes:\n");
	else
		fprintf(stderr, "Macro and register size: ");
	rnum = mnum = 0;
	csize = 0;
	for (rpp = &regt[0];  rpp < &regt[RHTSIZE];  rpp++) {
		for (rp = *rpp;  rp;  rp = rp->t_reg.r_next)
			switch (rp->t_reg.r_type) {
			case RNUMR:
				++rnum;
				if (vflag)
					printnr(rp);
				csize += sizeof(*rp);
				break;
			case RTEXT:
				++mnum;
				csize += sizeof(*rp) - sizeof(rp->t_reg.r_macd);
				csize += printm(rp, vflag);
				break;
			default:
				panic("unknown macro/register type %d",
					rp->t_reg.r_type);
			}
	}
	fprintf(stderr, "%u registers, %u macros, %u bytes\n",
		rnum, mnum, csize);
}

/*
 * Printnr prints out the size and name of a number register.
 */
printnr(rp)
register REG	*rp;
{
	fprintf(stderr, "Register %.2s %u bytes\n", rp->t_reg.r_name,
		sizeof(*rp));
}

/*
 * Printm returns the size of a macro and prints out its name if vflag.
 */
printm(rp, vflag)
register REG	*rp;
int		vflag;
{
	register MAC	*mp;
	unsigned	size;
	int		type;
	static char	*tn[]	= {	"Macro/String",	"Diversion" };

	size = 0;
	mp = &rp->t_reg.r_macd;
	type = mp->t_div.m_type;
	if (type != MTEXT && type != MDIVN)
		return (size);
	fprintf(stderr, "%s %.2s", tn[type], rp->t_reg.r_name);
	for (;;) {
		size += sizeof(*mp);
		mp = mp->t_div.m_next;
		if (mp == NULL)
			break;
		if (mp->t_div.m_type != type)
			fprintf(stderr, " (changed to %s)",
				tn[mp->t_div.m_type]);
	}
	if (vflag)
		fprintf(stderr, " uses %u bytes\n", size);
	return (size);
}


/*
 * Set page number.
 */
req_pn(argc, argv) int argc; char *argv[];
{
	if (argc >= 2) {
		pno = number(argv[1], SMUNIT, SDUNIT, pno, 0, pno);
		npn = pno + 1;
	}
}

/*
 * Set page offset.
 */
req_po(argc, argv) int argc; char *argv[];
{
	setval(&pof, &oldpof, argv[1], SMEMSP, SDEMSP);
}

/*
 * Set pointsize.
 */
req_ps(argc, argv) int argc; char *argv[];
{
	register int n;

	n = number(argv[1], SMPOIN, SDPOIN, psz, 0, oldpsz);
	newpsze(n);
}

/*
 * Copy a file verbatim to output.
 * !V7.
 */
req_rb(argc, argv) int argc; char *argv[];
{
	setbreak();
	if (argc < 2)
		printe(".rb: no file specified");
	else if (copy_file(argv[1]) == 0)
		printe(".rb: cannot open file %s", argv[1]);
}

/*
 * Read an insertion from the standard input.
 */
req_rd(argc, argv) int argc; char *argv[];
{
	if (argc >= 2)
		fprintf(stderr, "%s", argv[1]);
	else
		putc(A_BEL, stderr);
	allstr(SSINP);
}

/*
 * Assign a named font to another name.
 * !V7.
 */
req_rf(argc, argv) int argc; char *argv[];
{
	register int n;

	if (argc < 2) {
		printe(".rf: requires name and new name");
		return;
	}
	if ((n = font_number(argv[1], ".rf: ")) >= 0)
		assign_font(argv[2], n);
}

/*
 * Remove text register or request.
 */
req_rm(argc, argv) int argc; char *argv[];
{
	char name[2];
	register int i;

	for (i=1; i<argc; i++) {
		argname(argv[i], name);
		if (reltreg(name))
			continue;
		printe("cannot remove %s", argv[i]);
	}
}

/*
 * Rename the given request or macro.
 * Lookup is hashed, so just changing the name field does not work.
 */
req_rn(argc, argv) int argc; char *argv[];
{
	register REG *orp, *nrp;
	char oname[2], nname[2];

	argname(argv[1], oname);
	if ((orp = findreg(oname, RTEXT)) == NULL) {
		printe("cannot find request %s", argv[1]);
		return;
	}
	argname(argv[2], nname);
	if ((nrp = makereg(nname, RTEXT)) == NULL) {
		printe("cannot make request %s", argv[2]);
		return;
	}
	nrp->t_reg.r_maxh = orp->t_reg.r_maxh;
	nrp->t_reg.r_maxw = orp->t_reg.r_maxw;
	nrp->t_reg.r_macd = orp->t_reg.r_macd;
	reltreg(oname);
}

/*
 * Restore the cursor position, just in case it got garbaged.
 * !V7.
 */
req_rp()
{
	addidir(DFPOS, 0);
}

/*
 * Remove number register.
 */
req_rr(argc, argv) int argc; char *argv[];
{
	char name[2];
	register int i;

	for (i=1; i<argc; i++) {
		argname(argv[i], name);
		if (relnreg(name))
			continue;
		printe("cannot remove %s", argv[i]);
	}
}

/*
 * Turn no space mode off.
 */
req_rs()
{
	nsm = 0;
}

/*
 * Return to marked vertical position.
 */
req_rt(argc, argv) int argc; char *argv[];
{
	register int n;

	if (argc == 1)
		n = cdivp->d_mk;
	else
		n = numb(argv[1], SMVLSP, SDVLSP);
	if (n >= cdivp->d_rpos)
		return;
	sspace(n - cdivp->d_rpos);
}

/*
 * Stack input and redirect from given file.
 */
req_so(argc, argv) int argc; char *argv[];
{
	adsfile(argv[1]);
}

/*
 * Space vertically.
 */
req_sp(argc, argv) int argc; char *argv[];
{
	if (nsm != 0)
		return;
	setbreak();
	sspace(number(argv[1], SMVLSP, SDVLSP, 0, 1, unit(SMVLSP, SDVLSP)));
}

/*
 * Set space character size.
 */
req_ss(argc, argv) int argc; char *argv[];
{
	ssz = sszmul = number(argv[1], SMEMSP,
				(ntroff == NROFF) ? SDEMSP : 36 * SDEMSP,
				ssz, 0, ssz);
	sszdiv = psz;
}

/*
 * Save vertical distance.
 */
req_sv(argc, argv) int argc; char *argv[];
{
	register int n;

	n = number(argv[1], SMVLSP, SDVLSP, 0, 0, unit(SMVLSP, SDVLSP));
	if (mdivp->d_rpos+n > mdivp->d_ctpp->t_rpos)
		svs = n;
	else
		sspace(n);
}

/*
 * Pass a command line to the system.
 * !V7.
 */
req_sy(argc, argv) int argc; char *argv[];
{
	system(&miscbuf[3]);		/* miscbuf[] contains "sy command" */
}

/*
 * Set tab stops.
 */
req_ta(argc, argv) int argc; char *argv[];
{
	register TAB *tp;
	register int i;
	register char *p;
	int	prevpos = 0;

	tp = &tab[1];
	for (i=1; i<argc; i++) {
		if (i > TABSIZE-2) {
			printe("too many tab stops");
			break;
		}
		p = argv[i];
		if (*p == '\0') {
			printe("bad tab stop");
			break;
		}
		while (*p)
			p++;
		tp->t_jus = LJUS;
		switch (*--p) {
		case 'L':
			tp->t_jus = LJUS;
			*p = '\0';
			break;
		case 'C':
			tp->t_jus = CJUS;
			*p = '\0';
			break;
		case 'R':
			tp->t_jus = RJUS;
			*p = '\0';
			break;
		}
		tp->t_pos = number(argv[i], SMEMSP, SDEMSP, prevpos, 0, 0);
		if (tp->t_pos < prevpos) {
			printe("bad tab stop");
			break;
		}
		prevpos = tp->t_pos;
		tp++;
	}
	tp->t_jus = '\0';
}

/*
 * Set tab character.
 */
req_tc(argc, argv) int argc; char *argv[];
{
	register int n;

	if (argc < 1) {
		tbc = '\0';
		return;
	}
	tbc = argv[1][0];
	tfn = curfont;
	if (argc > 2)
		tbs = number(argv[2], SMEMSP, SDEMSP, tbs, 0, tbs);
	if (argc > 3 && (n = font_number(argv[3], ".tc: ")) >= 0)
		tfn = n;
}

/*
 * Temporary indent.
 */
req_ti(argc, argv) int argc; char *argv[];
{
	setbreak();
	tin = number(argv[1], SMEMSP, SDEMSP, ind, 0, 0);
	tif = 1;
}

/*
 * Three part title.
 */
req_tl(argc, argv) int argc; char *argv[];
{
	CODE *cp;
	int i;
	register int n;
	char charbuf[CBFSIZE], endc, c;
	register char *bp, *lp;

	envsave(ENVTITLE);
	setline();
	ind = 0;
	lln = tln;
	fill = 0;
	setfont("R", 1);
	bp = nextarg(miscbuf, NULL, 0);
	if ((endc = *bp) != '\0')
		bp++;
	for (i=0; i<3; i++) {
		lp = charbuf;
		for (;;) {
			if (*bp == '\0')
				break;
			if ((c = *bp++) == endc)
				break;
			if (c == tpc) {
				char	digits[5];
				register char	*dp = &digits[0];
				register int	pn = pno;

				for(;  pn > 0;  pn /= 10)
					*dp++ = pn%10 + '0';
				while (--dp > &digits[0])
					*lp++ = *dp;
				c = *dp;
			}
			if (lp >= &charbuf[CBFSIZE-2]) {
				printe("section %d of title too large", i+1);
				break;
			}
			*lp++ = c;
		}
		if (lp != charbuf) {
			*lp = '\0';
			adscore(charbuf);
			strp->x1.s_eoff = 1;
			process();
			wordbreak(DHMOV);
		}
		switch (i) {
		case 0:
			n = llinsiz;
			cp = llinptr;
			break;
		case 1:
			n = (lln - llinsiz - n)/2;
			nlinsiz = llinsiz += cp->l_arg.c_iarg = n;
			cp = llinptr;
			break;
		case 2:
			n = lln - llinsiz;
			cp->l_arg.c_iarg += n;
			llinsiz += n;
			break;
		}
	}
	linebreak(1);
	envload(ENVTITLE);
}

/*
 * Print a message on the terminal.
 */
req_tm(argc, argv) int argc; char *argv[];
{
	fprintf(stderr, "%s\n", nextarg(miscbuf, NULL, 0));
}

/*
 * Translate on output.
 */
req_tr(argc, argv) int argc; char *argv[];
{
	register char c1, c2, *p;

	p = argv[1];
	while (c1 = *p++) {
		if ((c2 = *p++) == '\0') {
			--p;
			c2 = ' ';
		}
		if (!isascii(c1) || !isascii(c2))	/* Remove this! */
			continue;			/* $$TO_DO$$ */
		if (c2 == ' ')
			c2 = A_DEL;			/* Or some other value? */
		trantab[c1] = c2;
	}
}

/*
 * Set underline font.
 */
req_uf(argc, argv) int argc; char *argv[];
{
	register int n;
	char *name;

	name = (argc < 1) ? "I" : argv[1];
	if ((n = font_number(name, ".uf: ")) >= 0)
		ufn = n;
}

/*
 * Underline.
 */
req_ul(argc, argv) int argc; char *argv[];
{
	register int n;

	n = number(argv[1], SMUNIT, SDUNIT, 0, 0, 1);
	if (n != 0) {
		ufp = curfont;		/* save current font */
		setfontnum(ufn, 0);	/* set underline font */
	} else if (ulc != 0)
		setfontnum(ufp, 1);	/* restore previous font */
	ulc = n;			/* set underline count */
}

/*
 * Set vertical base line spacing.
 */
req_vs(argc, argv) int argc; char *argv[];
{
	setval(&vls, &oldvls, argv[1], SMPOIN, SDPOIN);
}

/*
 * Set a trap.
 */
req_wh(argc, argv) int argc; char *argv[];
{
	register TPL	**tpp, *tp;
	TPL		*in;
	register DIV *dp;
	int rpos, apos;

	rpos = numb(argv[1], SMVLSP, SDVLSP);
	apos = rpos>=0 ? rpos : pgl+rpos;
	dp = mdivp;
	for (tpp = &dp->d_stpl; tp = *tpp; tpp = &tp->t_next) {
		if (apos == tp->t_apos) {
			if (dp->d_trap == tp)
				dp->d_trap = tp->t_next;
			if (dp->d_ctpp == tp)
				dp->d_ctpp = tp->t_next;
			*tpp = tp->t_next;
			nfree(tp);
			if (argc < 3)	/* .wh NN xx !replaces all at NN */
				break;	/* .wh NN !only removes 1st */
		 } else if (apos < tp->t_apos)
			break;
	}
	if (argc >= 3) {
		tp = (TPL *)nalloc(sizeof (TPL));
		tp->t_rpos = rpos;
		tp->t_apos = apos;
		argname(argv[2], tp->t_name);

		if ((dp->d_stpl == NULL) || (dp->d_stpl->t_apos > apos)) {
			tp->t_next = dp->d_stpl;
			dp->d_stpl = tp;
		}
		else { in = dp->d_stpl;
			while (in->t_next)
				if (in->t_next->t_apos > apos)
					break;
				else
					in = in->t_next;
			tp->t_next = in->t_next;
			in->t_next = tp;
		}

		if (dp->d_trap==tp->t_next && apos>=0)
			dp->d_trap = tp;
		if (dp->d_ctpp==tp->t_next && apos>=dp->d_rpos)
			dp->d_ctpp = tp;
	}
}

/* end of req2.c */
