/*
 * db/db1.c
 * A debugger.
 * Initialization and command line parsing.
 */

#include <stddef.h>
#include <canon.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/core.h>
#include "db.h"

int
main(argc, argv) int argc; char *argv[];
{
	signal(SIGINT, &arm_sigint);
	signal(SIGQUIT, SIG_IGN);
	initialize();
	setup(argc, argv);
	process();
}

/*
 * Catch and flag interrupts (SIGINT).
 */
void
arm_sigint()
{
	signal(SIGINT, &arm_sigint);
	intflag++;
}

/*
 * Canonicalize an l.out header.
 */
void
canlout()
{
	register int i;

	canint(ldh.l_magic);
	canint(ldh.l_flag);
	canint(ldh.l_machine);
	canvaddr(ldh.l_entry);
	for (i=0; i<NLSEG; i++)
		cansize(ldh.l_ssize[i]);
}

/*
 * Initialize segment formats, clear the breakpoint table,
 * invalidate the child process register image.
 */
void
initialize()
{
	init_mch();
	strcpy(seg_format[DSEG], "w");		/* patched in set_prog() if COFF */
	strcpy(seg_format[ISEG], "i");
	strcpy(seg_format[USEG], "w");		/* patched in set_prog() if COFF */
	bpt_init();
	reg_flag = R_INVALID;
}

/*
 * Leave.
 */
void
leave()
{
	killc();
	exit(0);
}

/*
 * Open the given file.
 * If 'rflag' is set, the file is opened for read only.
 */
FILE *
openfile(name, rflag) char *name; int rflag;
{
	register FILE *fp;

#if	__I386__
again:
#endif
	if (!rflag && (fp = fopen(name, "r+w")) != (FILE *)NULL)
		return fp;
	else if ((fp = fopen(name, "r")) != (FILE *)NULL) {
		if (!rflag)
			printr("%s: opened read only", name);
		return fp;
	}
#if	__I386__
	if (strcmp(name, DEFLT_OBJ) == 0) {
		name = DEFLT_OBJ2;	/* l.out not found, try a.out */
		goto again;
	}
#endif
	panic("Cannot open %s", name);
}

/*
 * Set up segmentation for a core dump.
 * The registers are also read.
 * This version is for interim COHERENT 4.2beta.r90 headers 10/7/93.
 */
void
set_core(name) char *name;
{
	struct ch_info		ch_info;
	struct core_seg		core_seg;
	long			offset, end;
	int			iflag, textflag, seg, nsegs;
	char			*corename, *cp;

	corename = NULL;

	/* Open the core file. */
	cfn = name;
	cfp = openfile(name, rflag);

	/* Read the core file header and set uproc offset. */
	if (fread(&ch_info, sizeof ch_info, 1, cfp) != 1)
		panic("Cannot read core file header");
	if (ch_info.ch_magic != CORE_MAGIC)
		panic("Not a core file");
	offset = ch_info.ch_info_len;
	if (ch_info.ch_info_len == sizeof(ch_info))
		panic("No core_proc info in core file\n");

	/*
	 * The registers are located at member cp_registers of the core_proc
	 * structure which follows the ch_info structure in the core file.
	 * Map the registers.
	 * Older versions of COHERENT allowed access to the entire u-area,
	 * but now only the registers are mapped by db.
	 */
	map_set(USEG, MIN_ADDR, (ADDR_T)sizeof(struct core_proc),
		(off_t)(sizeof(struct ch_info) + offsetof(struct core_proc, cp_registers)),
		MAP_CORE);

	/*
	 * There is currently no way to tell how many segments are dumped
	 * in a core file, so this uses the file size.
	 */
	if (fseek(cfp, (long)0, SEEK_END) == -1)
		panic("core file seek failed");
	end = ftell(cfp);

	/*
	 * Read the struct core_seg headers at the front of each memory segment.
	 * Set up segmentation accordingly.
	 * Because there is no rational way to tell which segment is which,
	 * this uses a nasty machine-dependent kludge.
	 */
	iflag = ISPACE == DSPACE;		/* non-sepid flag */
	textflag = 0;				/* iff text segment dumped */
	map_clear(DSEG, endpure);
	for (nsegs = 0; offset < end; ++nsegs) {
		dbprintf(("reading seg hdr %d at offset 0x%lx:\n", nsegs, offset));
		if (fseek(cfp, offset, SEEK_SET) == -1L)
			panic("core file seek failed");
		if (fread(&core_seg, sizeof(core_seg), 1, cfp) != 1)
			panic("core file segment header read failed");
		dbprintf(("pathlen=%x dumped=%x ", core_seg.cs_pathlen, core_seg.cs_dumped));
		dbprintf(("base=%x size=%x\n", core_seg.cs_base, core_seg.cs_size));
		offset += sizeof(core_seg) + core_seg.cs_pathlen;
		if (corename == NULL && core_seg.cs_pathlen != 0) {
			/* Read the core file name and compare it to name. */
			corename = nalloc(core_seg.cs_pathlen + 1, "core file name");
			corename[core_seg.cs_pathlen] = '\0';
			if (fread(corename, core_seg.cs_pathlen, 1, cfp) != 1)
				panic("core file name read failed");
			if ((cp = strrchr(lfn, '/')) != NULL)
				++cp;		/* ignore pathname to '/' */
			else
				cp = lfn;
			if (strcmp(corename, cp) != 0)
				printr("Core file name \"%s\" different from object file name \"%s\"", corename, cp);
		}
		if (core_seg.cs_dumped == 0)
			continue;			/* segment not dumped */
		if (core_seg.cs_dumped != core_seg.cs_size)
			printr("segment base 0x%x: dumped 0x%x of 0x%x bytes",
				core_seg.cs_base, core_seg.cs_dumped,
				core_seg.cs_size);
#if	__I386__
		if (IS_LOUT) {
			/*
			 * Set up segmentation for l.out core file.
			 */
			dbprintf(("core file assumed l.out\n"));
			if (core_seg.cs_base == (caddr_t)0) {
				seg = (nsegs == 0) ? ISEG : DSEG;
			} else if (core_seg.cs_base == (caddr_t)0x10000) {
				seg = DSEG;
				/* Adjust the i386 stack segment base. */
				core_seg.cs_base -= core_seg.cs_size;
				dbprintf(("Adjust stack segment base to %x\n", core_seg.cs_base));
			} else {
				seg = NOSEG;
			}
		} else {
			/* Set up segmentation for COFF core file. */
			dbprintf(("core file assumed COFF\n"));
			if (core_seg.cs_base == (caddr_t)0) {
				seg = ISEG;
				if (textflag == 0) {
					++textflag;
					map_clear(ISEG, NULL);
				}
			} else if (core_seg.cs_base == (caddr_t)0x400000) {
				seg = DSEG;
			} else if (core_seg.cs_base == (caddr_t)0x80000000) {
				seg = DSEG;
				/* Adjust the i386 stack segment base. */
				core_seg.cs_base -= core_seg.cs_size;
				dbprintf(("Adjust stack segment base to %x\n", core_seg.cs_base));
			} else {
				seg = NOSEG;
			}
		}
#else
		panic("segment base setup code is currently i386-specific!");
#endif
		if (seg == NOSEG)
			printr("unrecognized segment: base=0x%x size=0x%x",
				core_seg.cs_base, core_seg.cs_size);
		else
			map_set(seg, (ADDR_T)core_seg.cs_base,
				(ADDR_T)core_seg.cs_dumped,
				(off_t)offset, MAP_CORE);
		offset += core_seg.cs_dumped;
	}
	get_regs(R_ALL);			/* read the registers */
	if (iflag)
		ISPACE = DSPACE;
}

/*
 * Set up segmentation for an ordinary file.
 * This is really easy.
 */
void
set_file(name) char *name;
{
	lfp = openfile(name, rflag);
	map_set(DSEG, MIN_ADDR, MAX_ADDR, (off_t)0, MAP_PROG);
	ISPACE = DSPACE;
}

/*
 * Setup object file "name".
 * The flag is bit mapped:
 *	1 bit	read symbol table
 *	2 bit	read segment information
 */
void
set_prog(name, flag) char *name; int flag;
{
	dbprintf(("set_prog(%s, %d)\n", name, flag));
	lfp = openfile(name, (flag&2) ? rflag : 1);
	if (fread(&coff_hdr, sizeof(coff_hdr), 1, lfp) != 1)
		panic("Cannot read object file header");
	if (coff_hdr.f_magic == C_386_MAGIC) {
		/* The object is a COFF file. */
		dbprintf(("IS_COFF!\n"));
		file_type = COFF_FILE;
		addr_fmt = ADDR_FMT;
		aop_size = 32;
		strcpy(seg_format[DSEG], "l");
		strcpy(seg_format[USEG], "l");
	} else {
		/* Not a COFF file, might be an l.out. */
		if (fseek(lfp, 0L, SEEK_SET) == -1
		 || fread(&ldh, sizeof(ldh), 1, lfp) != 1)
			panic("Cannot read object file");
		canlout();
		if (ldh.l_magic != L_MAGIC)
			panic("Bad object file");

		/* The object is an l.out file. */
		dbprintf(("IS_LOUT!\n"));
		file_type = LOUT_FILE;
		addr_fmt = ADDR16_FMT;
		aop_size = 16;
	}
	if ((flag&1) != 0 && !sflag) {
		sfp = lfp;
		if (IS_LOUT) {
			nsyms = ldh.l_ssize[L_SYM] / sizeof(struct ldsym);
			if (nsyms != 0)
				read_lout_sym((long) sizeof(ldh)
					 + ldh.l_ssize[L_SHRI] + ldh.l_ssize[L_PRVI]
					 + ldh.l_ssize[L_SHRD] + ldh.l_ssize[L_PRVD]);
		} else {
			if (coff_hdr.f_nsyms != 0)
				read_coff_sym();
		}
	}
	if ((flag&2) != 0) {
		lfn = name;
		if (IS_LOUT)
			setloutseg();
		else
			setcoffseg();
	}
	/*
	 * Read additional COFF symbols as specified by the -a option.
	 * This follows setcoffseg() so that the segmentation information is
	 * already set up, because the .sym file currently does not include it.
	 */
	if (symfile != NULL)
		read_symfile();
}

/*
 * Setup arguments.
 */
void
setup(argc, argv) int argc; char *argv[];
{
	register char *cp;
	register int c;
	register int t;
	register int u;
	register int tflag;

	t = '\0';
	tflag = 0;

	/* Process command line switches -[cdefkorstV]. */
	for (; argc > 1; argc--, argv++) {
		cp = argv[1];
		if (*cp++ != '-')
			break;
		while ((c = *cp++) != '\0') {
			switch (c) {
			case 'c':
			case 'd':
			case 'e':
			case 'f':
			case 'k':
			case 'o':
				/* only one of [cdefko] is allowed */
				if (t != '\0')
					usage();
				t = c;
				continue;
			case 'a':
				if (argc < 3)
					usage();
				--argc;
				symfile = argv[2];
				++argv;
				continue;
			case 'p':
				if (argc < 3)
					usage();
				--argc;
				prompt = argv[2];
				++argv;
				continue;
			case 'r':
				rflag = 1;
				continue;
			case 's':
				sflag = 1;
				continue;
			case 't':
				tflag = 1;
				continue;
			case 'V':
				fprintf(stderr,
					"db: " MCHNAME " "
#if	DEBUG
					"DEBUG "
#endif
#ifdef	NOCANON
				/*	"NOCANON "	*/
#endif
#ifdef	NOFP
					"NOFP "
#endif
					"V%s\n", VERSION);
				continue;
			default:
				usage();
			}
		}
	}
	switch (t) {
	case '\0':
		switch (argc) {
		case 1:
			set_prog(DEFLT_OBJ, 3);
			set_core(DEFLT_AUX);
			break;
		case 2:
			set_prog(argv[1], 3);
			break;
		case 3:
			set_prog(argv[1], 3);
			set_core(argv[2]);
			break;
		default:
			usage();
		}
		break;
	case 'c':
		switch (argc) {
		case 1:
			set_prog(DEFLT_OBJ, 3);
			set_core(DEFLT_AUX);
			break;
		case 2:
			set_core(argv[1]);
			break;
		case 3:
			set_prog(argv[1], 3);
			set_core(argv[2]);
			break;
		default:
			usage();
		}
		break;
	case 'd':
		switch (argc) {
		case 1:
			set_prog("/coherent", 3);
			setdump("/dev/dump");
			break;
		case 3:
			set_prog(argv[1], 3);
			setdump(argv[2]);
			break;
		default:
			usage();
		}
		break;
	case 'e':
		if (argc < 2)
			usage();
		set_prog(argv[1], 3);
		if (startc(&argv[1], NULL, NULL, 0) == 0)
			leave();
		break;
	case 'f':
		switch (argc) {
		case 2:
			set_file(argv[1]);
			break;
		case 3:
			set_prog(argv[1], 1);
			set_file(argv[2]);
			break;
		default:
			usage();
		}
		break;
	case 'k':
		switch (argc) {
		case 1:
			set_prog("/coherent", 1);
			setkmem("/dev/mem");
			break;
		case 2:
			setkmem(argv[1]);
			break;
		case 3:
			set_prog(argv[1], 1);
			setkmem(argv[2]);
			break;
		default:
			usage();
		}
		break;
	case 'o':
		switch (argc) {
		case 1:
			set_prog(DEFLT_OBJ, 3);
			break;
		case 2:
			set_prog(argv[1], 3);
			break;
		case 3:
			set_prog(argv[1], 1);
			set_prog(argv[2], 2);
			break;
		default:
			usage();
		}
	}
	if (tflag) {
		if ((u=open("/dev/tty", 2)) < 0)
			panic("Cannot open /dev/tty");
		dup2(u, 0);
		dup2(u, 1);
		dup2(u, 2);
	}
}

/*
 * Check for interrupts and clear flag.
 */
int
testint()
{
	register int n;

	if ((n = intflag) != 0) {
		printf("Interrupted\n");
		intflag = 0;
	}
	return n;
}

/*
 * Generate a verbose usage message.
 */
void
usage()
{
	panic(
		"Usage: db [ -acdefkoprst ] [ [ mapfile ] program ]\n"
		"Options:\n"
		"\t-a sym\tRead additional symbols from file sym\n"
		"\t-c\tprogram is a core file\n"
		"\t-d\tprogram is a system dump; mapfile defaults to /coherent\n"
		"\t-e\tNext argument is object file and rest of command line is passed\n"
		"\t\tto the child process\n"
		"\t-f\tprogram is binary data (an ordinary file)\n"
		"\t-k\tprogram is a kernel process; mapfile defaults to /coherent\n"
		"\t-o\tprogram is an object file\n"
		"\t-p str\tUse str as interactive command prompt (default: \"db: \")\n"
		"\t-r\tAccess all files read-only\n"
		"\t-s\tDo not load symbol table\n"
		"\t-t\tPerform input and output via /dev/tty\n"
		"\tmapfile defaults to a.out or l.out.\n"
		"\tprogram defaults to core.\n"
		"Examples:\n"
		"\tdb prog\t\tExamine, patch, execute \"prog\" under db control\n"
		"\tdb prog core\tExamine postmortem core dump \"core\",\n"
		"\t\t\tusing symbol table from \"prog\"\n"
		"\tdb -e prog *.c\tExecute \"prog\" under db control with args *.c\n"
		"\tdb -f file\tExamine and patch \"file\" as stream of bytes\n"
	);
}

/* end of db/db1.c */
