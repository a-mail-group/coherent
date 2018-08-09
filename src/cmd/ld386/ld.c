/*
 * One pass Coff Loader.
 *
 * By Charles Fiterman for Mark Williams 3/30/92.
 */

#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <path.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <fnmatch.h>

#include "ld.h"

flag_t	reloc,	/* Combine input into a new .o not an executable */
	nosym,	/* No symbol table out. */
	watch,	/* Produce a trace */
	noilcl, /* Discard C local symbols beginning .L */
	nolcl,	/* Discard all local symbols */
	Gflag,	/* No warn on common/global */
	qflag,	/* No warn on commons of different length */
	Qflag,	/* Absolute silence on everything */
	debflg,	/* Create debug data */
	fource = -1;	/* halt on error */

#if	COMEAU
flag_t	Comeau_flag;		/* special error message handling for Comeau */
#endif

#if	0
flag_t	drvld;	/* Called as drvld */
#endif

int	errCount;
int	nundef;
mod_t	* head,			/* head of list of modules to load */
	* tail,			/* tail or list of modules */
	* xhead,			/* head of noload modules */
	* xtail;			/* tail of noload modules */
ren_t	* rhead;			/* rename list */

char * ofname = "a.out";		/* output file name */
long comnb, comns, comnl;	/* common lengths */
char * entrys;			/* entry string */
sym_t	* symtable [NHASH];	/* hashed symbol table */

unsigned short osegs = NLSEG;	/* the number of output segments */
unsigned short segMap [MAXSEG] = /* All segments map to the first four */
	{ S_TEXT, S_DATA, S_BSSD, S_COMM };

FILHDR fileh;
AOUTHDR aouth;
SCNHDR  * secth;		/* output segments */

static long str_length;
static int ofd;
static long * file_value = NULL, file_first;
char * argv0;


/*
 * Get space or die.
 */

#if	USE_PROTO
__VOID__ * alloc (size_t size)
#else
__VOID__ *
alloc (size)
size_t		size;
#endif
{
	__VOID__	      *	tmp;

	if (NULL == (tmp = malloc (size)))
		fatal ("Out of space");
	return memset (tmp, 0, size);
}


/*
 * Symbol-globbing code added to deal with keeping the kernel symbol table
 * as small as possible, mainly used in drivers. Since kernel backtraces are
 * not very useful without a list of symbols, we place the stripped symbol
 * values in a file somewhere.
 */

typedef struct sglob	sglob_t;

sglob_t	      *	pattern_head;
sglob_t	      *	pattern_tail;

struct sglob {
	sglob_t	      *	s_next;
	CONST char    *	s_pattern;
	int		s_include;
};

enum {
	EXCLUDE_SYM,
	INCLUDE_SYM
};

/*
 * Turn a colon-separated glob list into a series of glob structures.
 */

#if	USE_PROTO
LOCAL void add_globs (CONST char * globs, int include)
#else
LOCAL void
add_globs (globs, include)
CONST char    *	globs;
int		include;
#endif
{
	CONST char    *	temp;

	do {
		sglob_t	      *	newglob;
		size_t		newlen;

		temp = strchr (globs, ':');
		if (temp == NULL)
			newlen = strlen (globs) + 1;
		else {
			temp ++;
			newlen = temp - globs;
		}
		if (newlen <= 1)
			continue;

		newglob = alloc (sizeof (sglob_t) + newlen);
		newglob->s_pattern = (char *) (newglob + 1);
		newglob->s_next = NULL;
		newglob->s_include = include;

		if (pattern_tail == NULL)
			pattern_head = newglob;
		else
			pattern_tail->s_next = newglob;
		pattern_tail = newglob;

		memcpy (newglob + 1, globs, newlen - 1);
		((char *) newglob->s_pattern) [newlen - 1] = 0;
	} while ((globs = temp) != NULL);
}


/*
 * Try and match a symbol against a series of glob patterns; the returned
 * value is the "include" flag after having been passed through the list of
 * filters.
 */

#if	USE_PROTO
LOCAL int match_glob (CONST char * sname, int include)
#else
LOCAL int
match_glob (sname, include)
CONST char    *	sname;
int		include;
#endif
{
	sglob_t	      *	glistp;

	for (glistp = pattern_head ; glistp != NULL ; glistp = glistp->s_next) {
		if (include == glistp->s_include)
			continue;

		if (fnmatch (glistp->s_pattern, sname, FNM_NOESCAPE) == 0)
			include = glistp->s_include;
	}

	return include;
}


/*
 * Write or die.
 */

void
xwrite (loc, size)
char * loc;
unsigned int size;
{
	if (size != write (ofd, loc, size))
		fatal ("write error"); /**/
}


/*
 * Init segment descriptors.
 */
void
initSegs ()
{
	secth = alloc (sizeof (* secth) * MAXSEG);
	strcpy (secth [S_TEXT].s_name, _TEXT);
	secth [S_TEXT].s_flags = STYP_TEXT;
	strcpy (secth [S_DATA].s_name, _DATA);
	secth [S_DATA].s_flags = STYP_DATA;
	strcpy (secth [S_BSSD].s_name, _BSS);
	secth [S_BSSD].s_flags = STYP_BSS;
	strcpy (secth [S_COMM].s_name, _COMMENT);
	secth [S_COMM].s_flags = STYP_INFO;
}

#if	0
/*
 * build like normal file.
 */
static int
driver_alloc (aouth)
AOUTHDR * aouth;
{
	aouth->text_start = 0xA8;
	aouth->data_start = ((aouth->text_start + aouth->tsize) & 0x0fff)
				+ DATABASE;
	return 1; /* ok */
}

/*
 * We are drvld not ld. Ask the system where to put the stuff.
 */
static void
getsys ()
{
	register int i;
	long bsz, daddr;

	time (& fileh.f_timdat);
	fileh.f_nscns = osegs = NLSEG;

	daddr = sizeof (fileh) + (sizeof (* secth) * osegs) +
		(fileh.f_opthdr = sizeof (aouth));
	
	for (i = 0; i < S_BSSD; i ++) {	
		secth [i].s_scnptr = daddr;
		daddr += secth [i].s_size;
	}
	bsz = secth [S_BSSD].s_size;
	aouth.tsize = secth [S_TEXT].s_size;
	aouth.dsize = secth [S_DATA].s_size + bsz;
	if (! driver_alloc (& aouth))
		fatal ("kernel interface failed");
		/* This will become more elaborite when the kernel is done */
	secth [S_TEXT].s_vaddr = secth [S_TEXT].s_paddr = aouth.text_start;
	secth [S_DATA].s_vaddr = secth [S_DATA].s_paddr = aouth.data_start;	
	secth [S_BSSD].s_vaddr = secth [S_BSSD].s_paddr = aouth.data_start + bsz;
	aouth.dsize -= aouth.bsize = bsz;

	if (! nosym)
		fileh.f_symptr = daddr;
}
#endif

/*
 * Set virtual bases in array of segments.
 */

static void
baseall ()
{
	register SCNHDR  * sh;
	int i;
	unsigned long daddr, vaddr, size;

	time (& fileh.f_timdat);

	/* Add extra segments to real segments */
	for (i = NLSEG; i < osegs; i ++) {
		SCNHDR * oh;

		oh = secth + i;
		sh = secth + segMap [i];

		sh->s_size += oh->s_size;
		sh->s_nreloc += oh->s_nreloc;
		sh->s_nlnno += oh->s_nlnno;
	}

	/* only output S_COMM if it has something */
	fileh.f_nscns = secth [S_COMM].s_size ? NLSEG : S_COMM;

	size = 0;
	daddr = sizeof (fileh) + (sizeof (* secth) * fileh.f_nscns);
	if (reloc)
		vaddr = 0;
	else {
		daddr += fileh.f_opthdr = sizeof (aouth);
		if (fileh.f_flags & F_KER)	/* kernel */
			vaddr = KERBASE;
		else				/* executable */
			vaddr = daddr;
	}

	/* set s_vaddr, s_paddr, s_size, and s_scnptr for all segments */
	for (i = 0; i < NLSEG; i ++) {
		sh = secth + i;
		switch (i) {
		case S_TEXT:
			aouth.text_start = sh->s_vaddr = vaddr;
			aouth.tsize = size = sh->s_size;		
			break;

		case S_DATA:	/* data adjustment */
			if (! reloc) {
				if (fileh.f_flags & F_KER)
					vaddr = (vaddr + 0x0fff) & ~ 0x0fffL;
				else
					vaddr = (vaddr & 0x0fff) + DATABASE;
			}

			aouth.data_start = sh->s_vaddr = vaddr;
			aouth.dsize = size = sh->s_size;		
			break;

		case S_BSSD:
			sh->s_paddr = sh->s_vaddr = vaddr;
			vaddr += aouth.bsize = sh->s_size;
			sh->s_scnptr = 0;
			continue;

		default:
			sh->s_vaddr = vaddr;
			size = sh->s_size;
		}

		sh->s_paddr = sh->s_vaddr;
		sh->s_scnptr = daddr;
		daddr += size;
		vaddr += size;
	}

	/* fix new segments to point to right places */
	for (i = 0; i < NLSEG; i ++) {
		int j;
		long vaddr, daddr;

		sh = secth + i;
		vaddr = sh->s_vaddr + sh->s_size;	/* mark the end */
		daddr = sh->s_scnptr + sh->s_size;

		for (j = osegs; --j >= NLSEG;) {
			SCNHDR * oh;

			if (segMap [j] != i)
				continue;
			
			oh = secth + j;
			oh->s_vaddr  = vaddr -= oh->s_size;
			oh->s_scnptr = daddr -= oh->s_size;
			w_message ("set extra %.8s at %x %x",
				  oh->s_name, vaddr, daddr);
		}
	}

	/* set relocations for all segments */
	for (i = 0; i < NLSEG; i ++) {
		sh = secth + i;
		if (! reloc || S_BSSD == i || ! sh->s_nreloc) {
			sh->s_relptr = 0;
			continue;
		}
		sh->s_relptr = daddr;
		daddr += sh->s_nreloc * RELSZ;
	}

	/* set line numbers for all segments */
	for (i = 0; i < NLSEG; i ++) {
		sh = secth + i;
		if (! debflg || ! sh->s_nlnno) {
			sh->s_nlnno = sh->s_lnnoptr = 0;
			continue;
		}
		sh->s_lnnoptr = daddr;
		daddr += sh->s_nlnno * LINESZ;
	}

	if (! nosym)
		fileh.f_symptr = daddr;
}

/*
 * return entry point
 */

static unsigned long
lentry (str)
char	* str;
{
	char	      *	end;
	unsigned long	addr;
	sym_t	      *	sp;
	char		work [SYMNMLEN + 1];

	if (NULL == str)	/* not user specified */
		return aouth.text_start;

	addr = strtoul (str, & end, 8);
	if (end != NULL && * end == 0)
		return addr;

	addr = strtoul (str, & end, 16);
	if (end != NULL && * end == 0)
		return addr;

	for (sp = symtable [strcrc (str) % NHASH] ; sp != NULL ;
	     sp = sp->next) {
		if (sp->sym.n_sclass == C_EXT &&
		    ! strcmp (symName (& sp->sym, work), str)) {
			if (sp->sym.n_scnum != S_TEXT + 1)
				message ("entry point '%s' not in .text", str);
			return sp->sym.n_value;
		}
	}

	message ("entry point '%s' undefined", str); /**/
	return aouth.text_start;
}


/*
 * Define referenced special symbols.
 */
void
symBind (sn, ldrv, name, loc)
int	sn, ldrv;
char	* name;
long	loc;
{
	register SYMENT * sym;
	register sym_t * sp;
	char  work [SYMNMLEN + 1];

	for (sp = symtable [strcrc (name) % NHASH]; sp != NULL; sp = sp->next) {
		sym = &(sp->sym);
		if ((sym->n_sclass != C_EXT) ||
		     strcmp (symName (sym, work), name))
			continue;

		if (undefined (sym)) {
			nundef--;
			sym->n_scnum = sn + 1;
			sym->n_value = loc;
			sp->mod = NULL;	/* not defined in any mod */
			return;
		}

		if (ldrv)
			return;

		spwarn (sp, "redefines builtin symbol");
		/* Some symbols such as __end and __end_text
		 * are special to the linker. In general symbols
		 * beginning __ are reserved to implementors and
		 * should be avoided by users.
		 * Your definition has been used. */
	}
}

/*
 * Add reference to symbol table
 */
void
undef (s)
char * s;
{
	SYMENT	lds;

	memset (& lds, 0, sizeof (lds));
	lds.n_offset = (long) s;	/* point symbol at our stuff */
	lds.n_sclass = C_EXTDEF;	/* mark undefined */
	addsym (& lds, NULL);		/* connect to no module */
}

/*
 * Show undefined symbols.
 */
void
showUndef (sp, sym)
register sym_t * sp;
register SYMENT * sym;
{
	if (undefined (sym))
		spmsg (sp, "undefined"); /* NODOC */
}

/*
 * Pass all symbols through a function.
 * In a really elaborite order.
 */

void
allSym (fun)
int (* fun)();
{
	SYMENT * sym;
	sym_t * sp;
	mod_t * mp;
	int i;
	SYMENT * symEnd;

	if (reloc) {	/* We are producing another .o */
		/*
		 * Do symbols connected to modules in module order.
		 * Only process a global symbol for it's owner.
		 */
		for (mp = head; mp != NULL; mp = mp->next) {
			sym = (SYMENT *) mp->f->f_symptr;
			symEnd = sym + mp->f->f_nsyms;
			for (; sym < symEnd; sym += sym->n_numaux + 1) {
				if (1 == sym->n_zeroes) { /* pointer to original */
					sp = (sym_t *)sym->n_offset;
					if (sp->mod == mp)
						(* fun)(sp, & sp->sym, mp, sym);
				} else
					(* fun)(NULL, sym, mp, sym);
			}
		}
	} else {
		/*
		 * Producing an executable.
		 */

		for (mp = head; mp != NULL; mp = mp->next) {
			char showSym;

			sym = (SYMENT *) mp->f->f_symptr;
			symEnd = sym + mp->f->f_nsyms;

			/*
			 * If first entry is not C_FILE supress 
			 * all non C_EXT entries for debug, they make
			 * Orignon barf. If not debug mode and the
			 * user wants a symbol table give him everything.
			 */
			showSym = ! debflg || C_FILE == sym->n_sclass;

			for (; sym < symEnd; sym += sym->n_numaux + 1) {
				if (C_EXT == sym->n_sclass && ! sym->n_numaux)
					continue;	/* do later */

				/* pointer to original */
				if (1 == sym->n_zeroes) {
					sp = (sym_t *) sym->n_offset;
					if (sp->mod == mp)
						(* fun)(showSym ? sp : NULL,
							 & sp->sym, mp, sym);
				} else
					(* fun) (NULL, sym, mp, sym);
			}
		}

		/* All C_EXT entries go at the end. */
		for (mp = head; mp != NULL; mp = mp->next) {
			sym = (SYMENT *) mp->f->f_symptr;
			symEnd = sym + mp->f->f_nsyms;
			for (; sym < symEnd; sym += sym->n_numaux + 1) {
				if (!(C_EXT == sym->n_sclass &&
				      ! sym->n_numaux))
					continue;

				 /* pointer to original */
				if (1 == sym->n_zeroes) {
					sp = (sym_t *)sym->n_offset;
					if (sp->mod == mp)
						(* fun)(sp, &(sp->sym), mp, sym);
				} else
					(* fun)(NULL, sym, mp, sym);
			}
		}
	}

	/*
	 * Do symbols not connected to a module.
	 * These are end symbols and symbols defined with -u option
	 */
	for (i = 0; i < NHASH; i ++)
		for (sp = symtable [i]; NULL != sp; sp = sp->next)
			if (NULL == sp->mod)
				(* fun) (sp, & sp->sym, sp->mod, NULL);
}

/*
 * Fixup a symbol between passes.
 */

void
symFix (sp, sym, mp, auxp)
sym_t * sp;
SYMENT * sym, * auxp;
mod_t * mp;
{
	int segn, len;

	segn = sym->n_scnum;
	if (debflg && NULL != auxp && C_FILE == sym->n_sclass) {
		if (NULL != file_value)	/* files point in a circle */
			* file_value = fileh.f_nsyms;
		else
			file_first = fileh.f_nsyms;
		file_value = & sym->n_value;
		fileh.f_nsyms += sym->n_numaux + 1;
		return;
	}
	if (! reloc && common (sym)) {
		switch ((len = sym->n_value) & 3) {
		case 2:	/* 2 byte aligned */
			sym->n_value = comns;
			comns += len;
			break;
		case 0:	/* 4 byte aligned */
			sym->n_value = comnl;
			comnl += len;
			break;
		default: /* unaligned */
			sym->n_value = comnb;
			comnb += len;
		}
		sym->n_scnum  = S_BSSD + 1;
	} else if (segn > 0)
		/*
		 * Correct the value to the output sector header.
		 * If mp == NULL then segn this is a constructed symbol
		 * and segn is relative to the output sector headers.
		 * Otherwise it is relative to the input sector headers
		 * but the connection to the output sector headers was
		 * placed in s_paddr by pass1.c / object ().
		 */
		sym->n_value += secth [(NULL == mp) ? segn - 1 :
					mp->s [segn - 1].s_paddr].s_vaddr;

	if (NULL != sp && ! nosym) {
		char * name, work [SYMNMLEN + 1];

		/*
		 * Check to see if we should exclude/include symbols; note
		 * that undefined references are always retained.
		 */

		name = symName (& sp->sym, work);
		if (! reloc && sp->sym.n_scnum != N_UNDEF &&
		    match_glob (name, INCLUDE_SYM) == EXCLUDE_SYM) {
		    	if (sp->sym.n_sclass == C_EXT)
			    	printf ("%08x\t%s\n", sp->sym.n_value, name);
		    	sp->symno = -1;
		    	return;		/* leave this symbol out */
		}

		sp->symno = fileh.f_nsyms ++;
		if (debflg)
			fileh.f_nsyms += sym->n_numaux;
	}
}

/*
 * Do work between passes.
 */
void
betweenPass ()
{
	if (reloc)
		fileh.f_flags |= F_AR32WR;
	else {
		fileh.f_flags |= F_RELFLG | F_EXEC | F_AR32WR;
		comnb += (4 - ((comnb + comns) & 3)) & 3;
		secth [S_BSSD].s_size += comnb + comns + comnl;
	}
	if (nosym)
		fileh.f_flags |= F_LSYMS;

	if (! debflg)
		fileh.f_flags |= F_LNNO;

#if	0
	if (drvld)
		getsys ();	/* get segment base information from system */
	else
#endif
		baseall ();	/* compute segment base information */

	if (! reloc) {
		int i;

		/* define referenced end of segment symbols */
		for (i = 0; i < NLSEG; i ++) {
			char end_name [20], c, * p;

			sprintf (end_name, "__end%.8s", secth [i].s_name);
			for (p = end_name; 0 != (c = * p); p ++)
				if (! isalnum (c))
					* p = '_';

			symBind (i, 0 /* drvld */, end_name, secth [i].s_size);
		}
		/* define absolute end symbol */
		symBind (S_BSSD, 0 /* drvld */, "__end", secth [S_BSSD].s_size);

		/* get starting addresses for 1, 2 and 4 alligned commons */
		comnb = secth [S_BSSD].s_vaddr + secth [S_BSSD].s_size - comnb;
		comns = comnb - comns;
		comnl = comns - comnl;
	}

	if (nundef && ! reloc) {
		message ("the following symbols are undefined");
		errCount--;
		allSym (showUndef);
	}

	switch (errCount & fource) {
	case 0:
		break;
	case 1:
		fatal ("pass 1, 1 error"); /* NODOC */
	default:
		fatal ("pass 1, %d errors", errCount);
		/* At the end of pass 1 there were \fIn\fB errors detected.
		 * The link stopped here. */
	}

	/* Run through symbol table doing fixups */
	fileh.f_nsyms = 0;
	allSym (symFix);

	if (NULL != file_value)
		* file_value = file_first;

	aouth.entry = lentry (entrys);
}

/*
 * output symbol table.
 */

static void
outputSym (s, sm, mp, auxp)
sym_t * s;
SYMENT * sm;
mod_t * mp;
SYMENT * auxp;
{
	int i;
	char * name, work [SYMNMLEN + 1];
	SYMENT sym;
 
	if (NULL == s || s->symno == -1)
		return;

	/* build writeable copy */
	memcpy (& sym, sm, sizeof (sym));
	name = symName (sm, work);

	if (SYMNMLEN < (i = strlen (name))) {
		sym.n_offset = str_length;
		str_length += i + 1;
	} else
		memcpy (sym.n_name, name, SYMNMLEN);

	if (! debflg || NULL == auxp)
		sym.n_numaux = 0;
		
	if (sym.n_scnum > 0 && NULL != mp)
		sym.n_scnum = segMap [mp->s [sym.n_scnum - 1].s_paddr] + 1;

	xwrite (& sym, SYMESZ);

	if (sym.n_numaux) {
		int aux = 1;
		int has_fcn = 0;

		switch (sym.n_sclass) {
		case C_STRTAG:
		case C_UNTAG:
		case C_ENTAG:
		case C_BLOCK:
			has_fcn = 1;
			break;

		case C_STAT:
		case C_FILE:
			aux = sym.n_numaux + 1;	/* dont scan for ixs */
			break;

		default:
			if (ISFCN (sym.n_type))
				has_fcn = 1;
		}

		for (; aux <= sym.n_numaux; aux ++) {
			SYMENT * s1, * s2, * s3;
			AUXENT * a = (AUXENT *) (auxp + aux);

			/*
			 * Fix indices to new value.
			 */
			if (a->ae_tagndx) {
				s1 = ((SYMENT *)(mp->f->f_symptr)) +
					a->ae_tagndx;
				s = (sym_t *)s1->n_offset;
				a->ae_tagndx = s->symno;
			}
			/* ae_endndx points past an entry */
			if (has_fcn && a->ae_endndx) {
				s2 = ((SYMENT *)(mp->f->f_symptr));
				/* find the entry it points past */
				for (s1 = s2 + a->ae_endndx;
				     (s3 = (s2 + s2->n_numaux + 1)) < s1;
				     s2 = s3)
					;
				s = (sym_t *)s2->n_offset;
				a->ae_endndx = s->symno + s2->n_numaux + 1;
			}
		}
		xwrite (auxp + 1, SYMESZ * sym.n_numaux);
	}
}

/*
 * output long symbols.
 */
static void
longSym (s, sm)
sym_t * s;
SYMENT * sm;
{
	int i;
	char * name, work [SYMNMLEN + 1];

	if (NULL == s || s->symno == -1)
		return;

	name = symName (sm, work);
	if (SYMNMLEN < (i = strlen (name)))
		xwrite (name, i + 1);
}


/*
 * Scan this modules sections for a name match.
 */

SCNHDR *
findSeg (mp, segn)
CONST mod_t   *	mp;
int		segn;
{
	int i;
	SCNHDR * orsp;

	orsp = secth + segn;
	for (i = 0; i < mp->f->f_nscns; i ++) {
		if (strncmp (mp->s [i].s_name, orsp->s_name, 8) == 0) {
			return mp->s + i;
			break;
		}
	}
	return NULL;
}

/*
 * Do relocations
 */

static void
relocations (mp, osegn, isegn)
mod_t *  mp;
int		osegn;
int		isegn;
{
	RELOC * rel;
	char	* t;	/* actual text */
	SCNHDR	* isgp, * orsp;
	unsigned i;
	long	size, told, fixr;

	if (NULL == (isgp = findSeg (mp, isegn)) || ! (size = isgp->s_size))
		return;

	orsp = secth + osegn;
	fixr = isgp->s_vaddr - orsp->s_vaddr;
	t = (char *) isgp->s_scnptr;

	if (watch) {
		errCount--;
		mpmsg (mp, "relocating seg#%.8s [%06lx]@%06lx to %06lx r %ld",
			isgp->s_name,
			size,
			isgp->s_vaddr,
			orsp->s_vaddr,
			isgp->s_nreloc); /* NODOC */
		told = lseek (ofd, 0, 2);
	}
	
	for (i = 0; i < isgp->s_nreloc; i ++) {
		char * ptr;
		char * mtype;
		sym_t * sp;
		SYMENT * s, * sym;
		long relf, w, at;
		int   undef;
		char work [SYMNMLEN + 1], * name;
		static char * pcrel = "pcrel";

		/* get reloc record */
		rel = ((RELOC *) isgp->s_relptr) + i;

		w = rel->r_vaddr - isgp->s_vaddr;
		if (w < 0 || w > size)
			corrupt (mp);
		ptr = t + w;

		if (rel->r_symndx < 0 || rel->r_symndx > mp->f->f_nsyms)
			corrupt (mp);
		
		s = (SYMENT *) mp->f->f_symptr + rel->r_symndx;
		if (1 == s->n_zeroes) {	/* fixed elsewhere */ 
			sp = (sym_t *)s->n_offset;
			sym = & sp->sym;
		} else {
			sp = NULL;
			sym = s;
		}
		relf = sym->n_value;
		mtype = "rel";
		undef = undefined (sym);				

		if (watch) {
			at = told + w;
			name = symName (sym, work);
		}

		/*
		 * This wierdness is to deal with a coff wierdness.
		 * The address of a common is incremented by the
		 * length of the common as seen in that module.
		 */

		if (common (s))
			relf -= s->n_value;
		/*
		 * If the symbol is native to this module
		 * the reference already has this modules
		 * segment address. Subtract it.
		 */
		else if (! undef && (NULL == sp || sp->mod == mp))
			relf -= mp->s [s->n_scnum - 1].s_vaddr;

/* relocate what the pointer is aimed at and leave a record */
#define relocate(type) *(type *)ptr = (w = *(type *)ptr) + relf

		if (reloc) {
			rel->r_vaddr = w + orsp->s_vaddr;

			switch (rel->r_type) {
			case R_PCRBYTE:
				mtype = pcrel;
				relf = fixr;
				/* fall through... */
			case R_DIR8:
			case R_RELBYTE:
				relocate (char);
				break;

			case R_PCRWORD:
				mtype = pcrel;
				relf = fixr;
				/* fall through... */
			case R_DIR16:
			case R_RELWORD:
				relocate (short);
				break;

			case R_PCRLONG:
				mtype = pcrel;
				relf = fixr;
				/* fall through... */
			case R_DIR32:
			case R_RELLONG:
				relocate (long);
				break;

			case R_NONREL:
				mtype = "nonrel";
				w = *(long *)ptr;
				relf = 0;
				break;

			default:
			mpmsg (mp, "unknown r_type %d in segment %.8s record %d",
				rel->r_type, isgp->s_name, i);
			/* Unknown type on COFF relocation record. */
			}

			if (sym->n_scnum > 0 && mtype != pcrel)
				rel->r_symndx = sym->n_scnum - 1;
			else
				rel->r_symndx = sp->symno;

			if (! undef && sym->n_scnum < 0)
				rel->r_type = R_NONREL;

			w_message ("%lx '%s'(%d %lx %lx) %lx = '%s'(%lx) + %lx",
				at,
				mtype,
				rel->r_type,
				rel->r_vaddr,
				rel->r_symndx,
				relf + w,
				name,
				sym->n_value,
				w);
		} else {
			switch (rel->r_type) {
			case R_PCRBYTE:
				mtype = pcrel;
				relf += fixr;
				/* fall through... */
			case R_DIR8:
			case R_RELBYTE:
				relocate (char);
				break;

			case R_PCRWORD:
				mtype = pcrel;
				relf += fixr;
				/* fall through... */
			case R_RELWORD:
			case R_DIR16:
				relocate (short);
				break;

			case R_PCRLONG:
				mtype = pcrel;
				relf += fixr;
				/* fall through... */
			case R_RELLONG:
			case R_DIR32:
				relocate (long);
				break;

			case R_NONREL:
				mtype = "nonrel";
				w = *(long *)ptr;
				relf = 0;
				break;

			default:
			mpmsg (mp, "unknown r_type %d in segment %.8s record %d",
				rel->r_type, isgp->s_name, i);
			/* NODOC */
			}
			w_message ("%lx '%s'(%d) %lx = '%s'(%lx) + %lx",
				at,
				mtype,
				rel->r_type,
				relf + w,
				name,
				sym->n_value,
				w);
		}
	}
#undef relocate

	orsp->s_vaddr += size;
	xwrite (t, (int)size);
}


/*
 * Write line records for a segment.
 */

void
writeLine (mp, segn)
CONST mod_t   *	mp;
int		segn;
{
	int j;
	long fptr;
	AUXENT * a;
	SYMENT * sym;
	LINENO * l;
	SCNHDR * scn;

	if (NULL == (scn = findSeg (mp, segn)) || (! scn->s_nlnno))
		return;

	/*
	 * set up n_lnnoptr in symbol table to point
	 * to line records
	 */
	l = (LINENO *)scn->s_lnnoptr;
	fptr = lseek (ofd, 0L, 1);	/* where I am */
	for (j = 0; j < scn->s_nlnno; j ++, l ++) {
		if (! l->l_lnno) {
			sym = (SYMENT *) mp->f->f_symptr + l->l_addr.l_symndx;
			if (sym->n_numaux) {
				a = (AUXENT *)sym + 1;
				a->ae_lnnoptr = fptr;
				if (1 == sym->n_zeroes) {
				    sym_t * sp;
				    sp = (sym_t *)sym->n_offset;
				    l->l_addr.l_symndx = sp->symno;
				}
			}
		}
		fptr += LINESZ;
	}
	xwrite (scn->s_lnnoptr, LINESZ * scn->s_nlnno);
}


/*
 * Output all data.
 */

void
outputAll ()
{
	mod_t * mp;
	SCNHDR * scn;
	int i, j;

	/* Open output file */
	ofd = qopen (ofname, 1);

	/* write header */
	xwrite (& fileh, sizeof (fileh));

	if (! reloc) {	/* if executable set bits and write opt header */
		struct stat statbuf;
		stat (ofname, & statbuf);
		chmod (ofname, statbuf.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);

		aouth.magic = Z_MAGIC;
		xwrite (& aouth, sizeof (aouth));
	}

	/* write sector headers */
	xwrite (secth, sizeof (* secth) * fileh.f_nscns);

	/* write corrected text segments */
	for (i = 0; i < NLSEG; i ++) {
		if (i == S_BSSD)
			continue;
		for (j = 0; j < osegs; j ++) {
			if (segMap [j] == i)
				for (mp = head; mp != NULL; mp = mp->next)
					relocations (mp, i, j);
		}
	}

	/* write relocation if required */
	for (i = reloc ? 0 : S_BSSD ; i < S_BSSD; i ++) {
		for (j = 0; j < osegs; j ++) {
			if (segMap [j] != i)
				continue;

			for (mp = head; mp != NULL; mp = mp->next) {
				if ((NULL == (scn = findSeg (mp, j))) ||
				     ! scn->s_nreloc)
					continue;
				xwrite (scn->s_relptr,
					RELSZ * (int) scn->s_nreloc);
			}
		}
	}

	/* write lines if required */
	for (i = (debflg ? 0 : NLSEG); i < NLSEG; i ++) {
		for (j = 0; j < osegs; j ++) {
			if (segMap [j] != i)
				continue;

			for (mp = head; mp != NULL; mp = mp->next)
				writeLine (mp, j);
		}
	}

	if (! nosym) {
		str_length = 4;

		allSym (outputSym);
		if (4 != str_length) {
			xwrite (& str_length, sizeof (str_length));
			allSym (longSym);
		}
	}

	close (ofd);
}


/*
 * Process arguements, and call other work.
 */

int
main (argc, argv)
int		argc;
char	      *	argv [];
{
	int	c;
	char	* specialList = NULL;
	char	* argString = "?ge:finKl:L:o:A:O:rsu:wXxVZ:GqQI:R:";
	char	* env;

	/* find program name */
	if (NULL == (argv0 = strrchr (argv [0], '/')))
		argv0 = argv [0];
	else
		argv0 ++;

#if	COMEAU
	if ((env = getenv("COMEAU")) != NULL && strcmp(env, "1") == 0)
		++Comeau_flag;
#endif
	env = getenv ("LIBPATH");

#if	0
	/*
	 * drvld is an alternative name for ld.
	 * In this mode ld will load the kernel's symbol table, so
	 * that the loadable driver can link directly to kernel services.
	 * After pass1 we call driver_alloc () to tell the kernel the
	 * sizes of the driver segments, the kernel replys with locations
	 * for them. Then we output the driver to a tmp file and execl ()
	 */
	if (! strcmp (argv0, "drvld")) {
		ofname = tmpnam (NULL);
		argString = "?ge:l:L:u:wdq";
		fileh.f_flags |= F_KER;
		drvld = 1;
		/* read kernel for symbol table but don't load */
		readFile (kernelName (), 0, NULL);
		_addargs ("DRVLD", & argc, & argv);	
	}
	else
#endif
		_addargs ("LD", & argc, & argv);	

	initSegs ();

	while (optind < argc) {
		switch (c = getopt (argc, argv, argString)) {
		case -1:	/* Not an option, read a file for load */
			if (optind < argc)
				readFile (argv [optind ++], 1, NULL);
			break;

		case 'A': /* Read a file as an archive regardless of name */
			readFile (optarg, 1, ".a");
			break;;

		case 'O': /* Read a file as an object regardless of name */
			readFile (optarg, 1, ".o");
			break;

		case 'f':	/* attempt link even if errors */
			fource = 0;
			break;

		case 'g':	/* save extra segments and aux symbols */
			debflg ^= 1;
			break;

		case 'G':
			Gflag = 1;
			break;

		case 'e':
			entrys = optarg;
			break;

		case 'Z':	/* use and erase after */
		case 'i':	/* obselete options */
		case 'n':
			break;

		case 'K':	/* recompile of Kernel */
			fileh.f_flags |= F_KER;
			break;

		case 'L':
			/*
			 * Special filelist for lookup.
			 */
			{
				char * new;

				if (NULL == specialList) {
					new = alloc (strlen (optarg) + 2);
					sprintf (new, "%c%s", LISTSEP, optarg);
				} else {
					new = alloc (strlen (specialList) +
						     strlen (optarg)+ 2);
					sprintf (new, "%s%c%s", specialList,
						 LISTSEP, optarg);
					free (specialList);
				}
				specialList = new;
			}
			break;

		case 'l':
			/* -l <lib>: use standard lib */
			{
				char * xp, * lp;

				xp = alloc (strlen (optarg) + 6);
				sprintf (xp, "lib%s.a", optarg);
				lp = NULL;
				if (NULL != specialList)
					lp = path (specialList, xp, R_OK);
				if (NULL == lp && NULL != env)
					lp = path (env, xp, R_OK);
				if (NULL == lp)
					lp = path (DEFLIBPATH, xp, R_OK);
				if (NULL == lp)
				   fatal ("can't find '%s'", xp);
				   /* Can't locate requested library. */

				readFile (lp, 1, NULL);
				free (xp);
			}
			break;

		case 'o':
			ofname = optarg;
			break;

		case 'r':
			reloc ^= 1;	/* retain relocation information */
			break;

		case 's':
			nosym ^= 1;
			break;

		case 'u':
			undef (optarg);
			break;

		case 'Q':
			Qflag = 1;

		case 'q':
			qflag = 1;
			break;

		case 'w':
			watch ^= 1;
			break;

		case 'X':
			noilcl ^= 1;
			break;

		case 'x':
			nolcl ^= 1;
			break;

		case 'I':
			/*
			 * Colon-separated list of symbol inclusion glob
			 * patterns.
			 */

			add_globs (optarg, INCLUDE_SYM);
			break;

		case 'R':
			/*
			 * Colon-separated list of symbol exclusion glob
			 * patterns.
			 */

			add_globs (optarg, EXCLUDE_SYM);
			break;

		case 'V':
			fprintf(stderr, "ld: V" VERSION "\n");
			break;

		case '?':
			help ();		/* non returning */

		default:
			usage ();	/* non returning */
		}
	}

	if (nosym)
		nolcl = 1;

	if (reloc || debflg)
		nosym = nolcl = noilcl = 0;

	if (! fileh.f_magic)
		fatal ("No work");
	/* There were no object files loaded. */

	betweenPass ();	/* between passes */
	outputAll ();	/* output all data */

	/* repass argument list to erase any -Z files */
	optind = 1;
	while ((c = getopt (argc, argv, argString)) != -1)
		if ('Z' == c)
			unlink (optarg);

#if 0
	if (drvld) {
		execl (ofname, ofname, NULL); /* should never return */
		fatal ("Cannot execute loadable driver '%s'.", ofname); /**/
	}
#endif
	return 0;
}
