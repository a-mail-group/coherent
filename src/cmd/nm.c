/*
 * nm.c
 * 7/1/94
 * Usage: see usage[] below.
 * List symbols.
 * This understands l.out/n.out objects and archives
 * as well as COFF objects and archives.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <canon.h>
#include <l.out.h>
#include <ar.h>
#include <errno.h>
#include <coff.h>
#include <arcoff.h>

/* Compilation options. */
#define	COMEAU	1			/* special handling for Comeau */

/*
 * <coff.h> defines n_name as a macro, which conflicts with <l.out.h> member.
 * Use n_cname in this source instead for COFF n_name member.
 */
#undef	n_name
#define	n_cname		_n._n_name

/* Shorthand for case statements. */
#define case_class(x)	case C_ ## x: printf("%6.6s ", #x); break
#define case_type(x)	case T_ ## x: printf("%6.6s ", #x); break

/* Flags. */
int	aflag;			/* list all symbols			*/
int	dflag;			/* list only defined symbols		*/
int	gflag;			/* list only global symbols		*/
int	nflag;			/* sort numerically not alphabetically	*/
int	oflag;			/* append file name to each line	*/
int	pflag;			/* list symbols in symbol table order	*/
int	rflag;			/* list in reverse alphabetic order	*/
int	uflag;			/* list only undefined symbols		*/
int	vflag;			/* print coff type information		*/

/* Other globals. */
#if	COMEAU
int	Comeau_flag;		/* special output format for Comeau	*/
#endif
FILHDR	filehdr;		/* file header				*/
char	*fname;			/* file name				*/
FILE	*fp;			/* input file				*/
int	need_hdr;		/* one if header is required		*/
SCNHDR	*sectab;		/* sections				*/
int	status;			/* exit status				*/
char	*strtab;		/* string table				*/

char *gname[] = {
	"SI", "PI", "BI",
	"SD", "PD", "BD",
	" D", "  ", "  ",
	" A", " C", "??"
};
char *lname[] = {
	"si", "pi", "bi",
	"sd", "pd", "bd",
	" d", "  ", "  ",
	" a", " c", "??"
};
char nospace[] = "out of space";
char nosyms[] = "no symbols\n";
char usage[] =
	"Usage: nm [ -adgnopruv ] file ...\n"
	"Options:\n"
	"\t-a\tList all symbols\n"
	"\t-d\tList only defined symbols\n"
	"\t-g\tList only global symbols\n"
	"\t-n\tSort numerically not alphabetically\n"
	"\t-o\tPrepend file name to each line\n"
	"\t-p\tList symbols in symbol table order\n"
	"\t-r\tSort in reverse alphabetic order\n"
	"\t-u\tList only undefined symbols\n"
	"\t-v\tPrint COFF type information\n";

/* Forward. */
char	*alloc();
void	archive();
int	cmp_a();
int	cmp_n();
int	cmp_r();
int	C_symbol();
void	error();
void	fatal();
void	old_ar();
int	old_cmp_a();
int	old_cmp_n();
int	old_cmp_r();
void	old_nout();
int	read_hdrs();
void	read_syms();
char	*sym_name();
void	xread();

int
main(argc, argv) int argc; char *argv[];
{
	char c;

#if	COMEAU
	char *s;

	if ((s = getenv("COMEAU")) != NULL)
		Comeau_flag = atoi(s);
#endif

	while (EOF != (c = getopt(argc, argv, "adgnopruv?"))) {
		switch (c) {
		case 'a':	aflag = 1;			break;
		case 'd':	dflag = 1;			break;
		case 'g':	gflag = 1;			break;
		case 'n':	nflag = 1;			break;
		case 'o':	oflag = 1;			break;
		case 'p':	pflag = 1;			break;
		case 'r':	rflag = 1;			break;
		case 'u':	uflag = 1;			break;
		case 'v':	vflag = 1;			break;
		case '?':	fprintf(stderr, usage);	break;
		default:	fprintf(stderr, usage);	exit(1);
		}
	}
	if (pflag + rflag + nflag > 1)
		fatal("more than one sort order");

	need_hdr = (argc - optind) > 1;
	for (; optind < argc; optind++) {
		fname = argv[optind];
		if (NULL == (fp = fopen(fname, "rb")))
			fatal("%s: cannot open", fname);
#if	COMEAU
		if (Comeau_flag)
			printf("%s\n", fname);
		else
#endif
			printf("%s:\n", fname);
		read_hdrs(0L, 0L);
		fclose(fp);
	}
	exit(status);
}

/*
 * Get space or die.
 */
char *
alloc(n) register unsigned n;
{
	register char *s;

	if (NULL == (s = malloc(n)))
		fatal(nospace);
	return memset(s, '\0', n);
}

/*
 * Print members of archive.
 */
void
archive(seek) long seek;
{
	long arhend;
	struct ar_hdr coff_arh;
	struct old_ar_hdr arh;
	char *p;

	need_hdr = 0;
	for (arhend = seek + SARMAG; ; ) {
		fseek(fp, arhend, 0);
		if (1 != fread(&coff_arh, sizeof(coff_arh), 1, fp))
			break;
		memset(&arh, '\0', sizeof(arh));
		memcpy(arh.ar_name, coff_arh.ar_name, DIRSIZ);
		if (NULL != (p = strchr(arh.ar_name, '/')))
			*p = '\0';

		sscanf(coff_arh.ar_date, "%ld %d %d %o %ld",
			&arh.ar_date, &arh.ar_uid,
			&arh.ar_gid, &arh.ar_mode, &arh.ar_size);

		arhend += sizeof(coff_arh);
		if (arh.ar_name[0]) {
#if	COMEAU
			if (Comeau_flag)
				printf("%s[%s]\n", fname, arh.ar_name);
			else
#endif
				printf("%s(%s):\n", fname, arh.ar_name);
			read_hdrs(arhend, arh.ar_size);
		}
		arhend += arh.ar_size;
		if (arhend & 1)
			arhend++;		
	}
	need_hdr = 1;
}

/*
 * Sort by alpha.
 */
int
cmp_a(s1, s2) register SYMENT *s1, *s2;
{
	char w1[SYMNMLEN + 1], w2[SYMNMLEN + 1];

	return strcmp(sym_name(s1, w1), sym_name(s2, w2));
}

/*
 * Sort by value;
 */
int
cmp_n(s1, s2) register SYMENT *s1, *s2;
{
	long i;

	if (!(i = (s1->n_value - s2->n_value)))
		return 0;
	return (i < 0) ? -1 : 1;
}

/*
 * Sort by reverse alpha.
 */
int
cmp_r(s1, s2) register SYMENT *s1, *s2;
{
	char w1[SYMNMLEN + 1], w2[SYMNMLEN + 1];

	return -strcmp(sym_name(s1, w1), sym_name(s2, w2));
}

/*
 * This routine gets called if we
 * are not in '-a' mode. It determines if
 * the symbol pointed to by 'sp' is a C
 * style symbol (trailing '_' or longer than
 * (NCPLN-1) characters). If it is it eats the '_'
 * and returns true.
 */
int
C_symbol(sp) register struct ldsym *sp;
{
	register char *cp1, *cp2;

	cp1 = &sp->ls_id[0];
	cp2 = &sp->ls_id[NCPLN];
	while (cp2!=cp1 && *--cp2==0)
		;
	if (*cp2 != 0) {
		if (*cp2 == '_') {
			*cp2 = 0;
			return 1;
		}
		if (cp2-cp1 >= (NCPLN-1))
			return 1;
	}
	return 0;
}

/*
 * Print a nonfatal error message.
 */
void
error(s) char *s;
{
	fprintf(stderr, "nm: %r\n", &s);
	status = 1;
}

/*
 * Print an error message and die.
 */
void
fatal(s) char *s;
{
	int save = errno;

	fprintf(stderr, "nm: %r\n", &s);
#if	0
	if (0 != (errno = save))
		perror("errno reports");
#endif
	exit(1);
}

/*
 * Process old format archives.
 */
void
old_ar(name, seek) char *name; long seek;
{
	long arhend;
	struct old_ar_hdr arh;

	need_hdr = 0;
	for (arhend = seek + sizeof(short); ; ) {
		fseek(fp, arhend, 0);
		if (1 != fread(&arh, sizeof(arh), 1, fp))
			break;
		arhend += sizeof(arh);
		arh.ar_date = 0;	/* terminate name */
		cansize(arh.ar_size);
		if (strcmp(arh.ar_name, HDRNAME)) {
			printf("%s(%s):\n", name, arh.ar_name);
			read_hdrs(arhend, arh.ar_size);
		}
		arhend += arh.ar_size;
	}
	need_hdr = 1;
}

/*
 * n.out sort by alpha.
 */
int
old_cmp_a(s1, s2) struct nlist *s1, *s2;
{
	return strncmp(s1->n_name, s2->n_name, NCPLN);
}

/*
 * n.out sort by value.
 */
int
old_cmp_n(s1, s2) struct nlist *s1, *s2;
{
	long i;

	if (!(i = (s1->n_value - s2->n_value)))
		return 0;
	return (i < 0) ? -1 : 1;
}

/*
 * n.out sort by reverse alpha.
 */
int
old_cmp_r(s1, s2) struct nlist *s1, *s2;
{
	return strncmp(s2->n_name, s1->n_name, NCPLN);
}

/*
 * Process n.out stuff.
 */
void
old_nout(seek) long seek;
{
	struct ldheader ldh;
	register struct ldsym *sym, *s;
	register unsigned i, syms, n;
	long toSym;
	short type;

	fseek(fp, seek, 0);
	xread(&ldh, sizeof(ldh), "n.out header");
	if (!ldh.l_ssize[L_SYM]) {
		printf(nosyms);
		return;
	}

	if (need_hdr)
		printf("%s:\n", fname);

	canshort(ldh.l_machine);
	toSym = ldh.l_entry + sizeof(ldh);

	for (i = 0; i < L_SYM; i++) {
		cansize(ldh.l_ssize[i]);
		if (i != L_BSSI && i != L_BSSD)
			toSym += ldh.l_ssize[i];
	}

	fseek(fp, seek + toSym, 0);
	cansize(ldh.l_ssize[L_SYM]);
	i = ldh.l_ssize[L_SYM];
	if (i != ldh.l_ssize[L_SYM])
		fatal(nospace);
	sym = alloc(i);
	xread(sym, i, "symbol table");
	n = i / sizeof(*sym);

	/* Squeeze out unneeded stuff before sort. */
	for (i = syms = 0; i < n; i++) {
		canshort(sym[i].ls_type);
		canshort(sym[i].ls_addr);
		type = sym[i].ls_type;
		if (gflag && !(type & L_GLOBAL))
			continue;
		if ((type & ~L_GLOBAL) == L_REF) {	/* a reference */
			if (dflag)	/* list only defined */
				continue;
			if (uflag && sym[i].ls_addr)
				continue;
		}
		else if (uflag)
			continue;
		if (!aflag && !C_symbol(sym + i))
			continue;
		if (i != syms)
			sym[syms] = sym[i];
		syms++;
	}
	sym = realloc(sym, syms * sizeof(*sym));

	if (!pflag)
		qsort(sym, syms, sizeof(*sym),
			nflag ? old_cmp_n
		      : rflag ? old_cmp_r
		      :		old_cmp_a);

	for (s = sym; s < (sym + syms); s++) {
		if (oflag)
			printf("%s:\t", fname);
		i = s->ls_type & L_GLOBAL;
		type = s->ls_type & ~ L_GLOBAL;
		if (type < L_SHRI || type > L_REF)
			type = L_REF + 1;
		if (type == L_REF && !s->ls_addr)
			printf("      %c", i ? 'U' : 'u');
		else
			printf("%04x %s",
				s->ls_addr,
				i ? gname[type] : lname[type]);
		printf(" %.*s\n", NCPLN, s->ls_id);
	}
	free(sym);
}

/*
 * Read header info and process appropriately.
 */
int
read_hdrs(seek, size) long seek, size;
{
	unsigned i;

	if (1 != fread(&filehdr, sizeof(filehdr), 1, fp)) {
		if (ferror(fp))
			fatal("%s: read error", fname);
		else
			error("%s: inappropriate filetype", fname);
		return 0;
	}
	if (C_386_MAGIC != filehdr.f_magic) {
		if (!memcmp(ARMAG, &filehdr, SARMAG))
			return archive(seek);

		canshort(filehdr.f_magic);
		if (filehdr.f_magic == OLD_ARMAG)
			return old_ar(fname, seek);
		if (filehdr.f_magic == L_MAGIC)
			return old_nout(seek);
		error("%s: inappropriate filetype", fname);
		return 0;
	}
	if (filehdr.f_opthdr)	/* pass opt hdr */
		fseek(fp, seek + sizeof(filehdr) + filehdr.f_opthdr, 0);
	sectab = alloc(i = (filehdr.f_nscns * sizeof(SCNHDR)));
	xread(sectab, i, "section headers");

	if (filehdr.f_nsyms)
		read_syms(seek, size);
	else
		printf(nosyms);
}

/*
 * Read COFF symbols.
 */
void
read_syms(seek, size) long seek, size;
{
	register SYMENT *s, *sym;
	long str_len;
	unsigned len;
	register int i, aux, syms;

	/* Read the whole symbol table. */
	fseek(fp, seek + filehdr.f_symptr, 0);
	len = str_len = SYMESZ * filehdr.f_nsyms;
	if (str_len != len)
		fatal("symbol table too large");
	sym = alloc(len);
	xread(sym, len, "symbol table");

	/* Squeeze out unneeded stuff before sort. */
	for (i = aux = syms = 0; i < filehdr.f_nsyms; i++) {
		if (aux) {
			aux--;			/* ignore an aux entry */
			continue;
		}
		s = sym + i;
		aux = s->n_numaux;
		switch (s->n_sclass) {
		case C_EXT:
			if (s->n_scnum || s->n_value) { /* defined */
				if (uflag)	/* print only undefined */
					continue;
			} else {			/* undefined */
				if (dflag)	/* print only defined */
					continue;
			}
			break;
		case C_EXTDEF:
			if (dflag)
				continue;
			break;
		case C_STAT:
			if (gflag || uflag)
				continue;
			switch (s->n_scnum) {
			case 1:
				if (!strcmp(s->n_cname, ".text"))
					continue;
				break;
			case 2:
				if (!strcmp(s->n_cname, ".data"))
					continue;
				break;
			case 3:
				if (!strcmp(s->n_cname, ".bss"))
					continue;
				break;
			}
			break;
		default:
			if (!vflag)
				continue;
		}
		if (i != syms)
			sym[syms] = *s;
		syms++;
	}
	sym = realloc(sym, syms * SYMESZ);
	str_len = 0;
	if (!size || ((filehdr.f_symptr + len + sizeof(long)) < size))
		if (1 != fread(&str_len, sizeof(str_len), 1, fp))
			str_len = 0;
	if (str_len) {
		len = str_len -= 4;
		if (len != str_len)
			fatal("string table too large");
		strtab = alloc(len);
		xread(strtab, len, "string table");
	}

	if (!pflag)
		qsort(sym, syms, SYMESZ,
			nflag ? cmp_n
		      : rflag ? cmp_r
		      :		cmp_a);

	for (s = sym; s < (sym + syms); s++) {
		char w1[SYMNMLEN + 1], *n;

		i = strlen(n = sym_name(s, w1));

#if	COMEAU
		if (Comeau_flag) {
			char c;

			switch (s->n_scnum) {
			case N_UNDEF:	c = (s->n_value) ? 'C' : 'U';	break;
			case 1:		c = 'T';			break;
			case 2:		c = 'D';			break;
			case 3:		c = 'B';			break;
			default:
				continue;
			}
			printf("%c %s\n", c, n);
			continue;
		}
#endif
		if (oflag)
			printf("%s:\t", fname);

		/* Display section name. */
		switch (s->n_scnum) {
		case N_DEBUG:	printf("DEBUG    ");		break;
		case N_ABS:	printf("ABSOLUTE ");		break;
		case N_TV:	printf("TV       ");		break;		
		case N_UNDEF:
			if (s->n_value)
				printf(".comm    ");
			else
				printf("UNDEF    ");
			break;
		default:
			printf("%-8.8s ", sectab[s->n_scnum - 1].s_name);
		}

		if (vflag) {	/* Print coff type data. */

			/* Display storage class. */
			switch (s->n_sclass) {
			case_class(EFCN);
			case_class(NULL);
			case_class(AUTO);
			case_class(EXT);
			case_class(STAT);
			case_class(REG);
			case_class(EXTDEF);
			case_class(LABEL);
			case_class(ULABEL);
			case_class(MOS);
			case_class(ARG);
			case_class(STRTAG);
			case_class(MOU);
			case_class(UNTAG);
			case_class(TPDEF);
			case_class(USTATIC);
			case_class(ENTAG);
			case_class(MOE);
			case_class(REGPARM);
			case_class(FIELD);
			case_class(BLOCK);
			case_class(FCN);
			case_class(EOS);
			case_class(FILE);
			default:
				printf("unknown ");
			}

			if (s->n_type & N_TMASK) {
				if (ISPTR(s->n_type))
					printf("pointer ");
				else if (ISFCN(s->n_type))
					printf("function ");
				else if (ISARY(s->n_type))
					printf("array ");
				else if (ISTAG(s->n_type))
					printf("struct ");
				else if (INCREF(s->n_type))
					printf("union  ");
				else if (DECREF(s->n_type))
					printf("enum ");
			}
			else
				printf("- ");
	
			/* Display type. */
			switch (BTYPE(s->n_type)) {
			case_type(NULL);
			case_type(VOID);
			case_type(CHAR);
			case_type(SHORT);
			case_type(INT);
			case_type(LONG);
			case_type(FLOAT);
			case_type(DOUBLE);
			case_type(STRUCT);
			case_type(UNION);
			case_type(ENUM);
			case_type(MOE);
			case_type(UCHAR);
			case_type(USHORT);
			case_type(UINT);
			case_type(ULONG);
			default:
				printf("unknown ");
			}
		}
		printf("%08lX %s\n", s->n_value, n);		/* value */
	}
	free(sym);
}

/*
 * Return a pointer to a symbol name, not necessarily in the given buffer.
 */
char *
sym_name(sym, work) SYMENT *sym; char *work;
{
	if (sym->n_zeroes == 0L)
		return strtab + sym->n_offset - 4;	/* sym in string table */

	/* Make sure symbol name is NUL-terminated. */
	memcpy(work, sym->n_cname, SYMNMLEN);
	work[SYMNMLEN] = '\0';
	return work;
}

/*
 * Read, die on error.
 */
void
xread(dest, size, msg) char *dest; unsigned int size; char *msg;
{
	if (1 != fread(dest, size, 1, fp))
		fatal("%s: read error (%s)", fname, msg);
}

/* end of nm.c */
