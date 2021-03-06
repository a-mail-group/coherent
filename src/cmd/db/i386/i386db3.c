/*
 * db/i386/i386db3.c
 * A debugger.
 * i386 disassembler tables for disassembler in i386/i386db2.c.
 * Intel operand order.
 */

#include "i386db.h"

/* 
 * Legend to characters following "%" tokens in strings,
 * taken from Intel i386 "Programmer's Reference Manual".
 *
 *	First char after '%'; see also 2nd char, general case below:
 *		A - direct address
 *		C - reg of r/m selects control register
 *		D - reg of r/m selects debug register
 *		E - modR/M selects operand
 *		F - flags register
 *		G - reg field of modR/M selects general register
 *		H - sign-extended immediate data (same as I but sign-extended)
 *		I - immediate data
 *		J - relative offset added to IP
 *		K - as J, except find symbol, if possible
 *		M - modR/M selects memory only
 *		O - no modR/M byte; offset only
 *		R - mod field of modR/M selects register only
 *		S - reg field of modR/M selects segment register
 *		T - reg feild of modR/M selects test register
 *		X - memory addressed by DS:ESI
 *		Y - memory addressed by ES:EDI
 *		Z - assembler instruction mnemonic size qualifier
 *		2 - prefix of two-byte opcode
 *
 *	For the following first characters after the %, the second after the %
 *	has a special meaning associated with it:
 *		e - put in 'e' if address size attribute = 32 
 *			- second char is part of reg name
 *		f - floating point 
 *			- second char is esc value
 *		g - REG/OPCODE field of the modR/M byte is interpreted as
 *		    additional opcode; it is used as an index to a
 *		    column in the 2-d group array (grp[][])
 *			- second character yields the group number, i.e., it 
 *			  is used as an index to a row in grp[][]  
 *		r - read next n bytes, but don't do anything with them; n is 
 *		    given by 2nd char and meaning conforms with general case
 *		s - size override:
 *			- second char has following meanings:
 *				"a" - address size override
 *				"o" - operand size override
 *
 *	Second char after '%', the general case:
 *		a - two words or dwords in memory (BOUND)
 *		b - byte
 *		d - dword
 *		f - 32 bit pointer
 *		l - 48 bit pointer
 *		p - 32 or 48 bit pointer
 *		s - six byte pseudo-descriptor
 *		v - word or dword
 *		w - word
 *		z - r/m16: append 'w' to assem. mnemonic
 *		  - r/m32: append 'w' to assem. mnemonic if operand size = 16,
 *			   append 'l' to assem. mnemonic if operand size = 32
 *		F - use floating regs in mod/rm (unused)
 *		1-8 - group number
 */

/******************************************************************************/

char *op_map_1[] = {
	/* 0 */
	"add%Zb %Eb,%Gb",    "add%Zv %Ev,%Gv",	  "add%Zb %Gb,%Eb",   "add%Zv %Gv,%Ev",
	"add%Zb al,%Ib",     "add%Zv %eax,%Iv",	  "push es",         "pop es",
	"or%Zb %Eb,%Gb",     "or%Zv %Ev,%Gv",	  "or%Zb %Gb,%Eb",    "or%Zv %Gv,%Ev",
	"or%Zb al,%Ib",      "or%Zv %eax,%Iv",	  "push cs",         "%2 ",

	/* 1 */
	"adc%Zb %Eb,%Gb",    "adc%Zv %Ev,%Gv",	  "adc%Zb %Gb,%Eb",   "adc%Zv %Gv,%Ev",
	"adc%Zb al,%Ib",     "adc%Zv %eax,%Iv",	  "push ss",	      "pop ss",
	"sbb%Zb %Eb,%Gb",    "sbb%Zv %Ev,%Gv",	  "sbb%Zb %Gb,%Eb",   "sbb%Zv %Gv,%Ev",
	"sbb%Zb al,%Ib",     "sbb%Zv %eax,%Iv",	  "push ds",	      "pop ds",

	/* 2 */
	"and%Zb %Eb,%Gb",    "and%Zv %Ev,%Gv",	  "and%Zb %Gb,%Eb",   "and%Zv %Gv,%Ev",
	"and%Zb al,%Ib",     "and%Zv %eax,%Iv",	  "es:",	      "daa",
	"sub%Zb %Eb,%Gb",    "sub%Zv %Ev,%Gv",	  "sub%Zb %Gb,%Eb",   "sub%Zv %Gv,%Ev",
	"sub%Zb al,%Ib",     "sub%Zv %eax,%Iv",	  "cs:",	      "das",

	/* 3 */
	"xor%Zb %Eb,%Gb",    "xor%Zv %Ev,%Gv",	  "xor%Zb %Gb,%Eb",   "xor%Zv %Gv,%Ev",
	"xor%Zb al,%Ib",     "xor%Zv %eax,%Iv",	  "ss:",	      "aaa",
	"cmp%Zb %Eb,%Gb",    "cmp%Zv %Ev,%Gv",	  "cmp%Zb %Gb,%Eb",   "cmp%Zv %Gv,%Ev",
	"cmp%Zb al,%Ib",     "cmp%Zv %eax,%Iv",	  "ds:",	      "aas",

	/* 4 */
	"inc%Zv %eax",       "inc%Zv %ecx",	  "inc%Zv %edx",      "inc%Zv %ebx",
	"inc%Zv %esp",       "inc%Zv %ebp",	  "inc%Zv %esi",      "inc%Zv %edi",
	"dec%Zv %eax",       "dec%Zv %ecx",	  "dec%Zv %edx",      "dec%Zv %ebx",
	"dec%Zv %esp",       "dec%Zv %ebp",	  "dec%Zv %esi",      "dec%Zv %edi",

	/* 5 */
	"push%Zv %eax",      "push%Zv %ecx",	  "push%Zv %edx",     "push%Zv %ebx",
	"push%Zv %esp",      "push%Zv %ebp",	  "push%Zv %esi",     "push%Zv %edi",
	"pop%Zv %eax",       "pop%Zv %ecx",	  "pop%Zv %edx",      "pop%Zv %ebx",
	"pop%Zv %esp",       "pop%Zv %ebp",	  "pop%Zv %esi",      "pop%Zv %edi",

	/* 6 */
	"pusha%Zv",          "popa%Zv",		  "bound%Zv %Gv,%Ma", "arpl %Ew,%Gw",
	"fs:",		     "gs:",		  "%so",	      "%sa",
	"push%Zv %Iv",     "imul%Zv %Gv,%Ev,%Iv", "push%Zb %Ib",      "imul%Zv %Gv,%Ev,%Hb",
	"ins%Zb %Yb",        "ins%Zv %Yv",	  "outs%Zb %Xb",      "outs%Zv %Xv",

	/* 7 */
	"jo %Jb",	     "jno %Jb",		  "jb %Jb",	       "jae %Jb",
	"je %Jb",	     "jne %Jb",		  "jbe %Jb",	       "ja %Jb",
	"js %Jb",	     "jns %Jb",		  "jp %Jb",	       "jnp %Jb",
	"jl %Jb",	     "jge %Jb",		  "jle %Jb",	       "jg %Jb",

	/* 8 */
	"%g1%Zb %Eb,%Ib",    "%g1%Zv %Ev,%Iv",	  "mov%Zb al,%Ib",     "%g1%Zv %Ev,%Hb",
	"test%Zb %Eb,%Gb",   "test%Zv %Ev,%Gv",	  "xchg%Zb %Eb,%Gb",   "xchg%Zv %Ev,%Gv",
	"mov%Zb %Eb,%Gb",    "mov%Zv %Ev,%Gv",	  "mov%Zb %Gb,%Eb",    "mov%Zv %Gv,%Ev",
	"mov%Zw %Ew,%Sw",    "lea%Zv %Gv,%M ",	  "mov%Zw %Sw,%Ew",    "pop%Zv %Ev",

	/* 9 */
	"nop",		     "xchg%Zv %eax,%ecx", "xchg%Zv %eax,%edx", "xchg%Zv %eax,%ebx",
	"xchg%Zv %eax,%esp", "xchg%Zv %eax,%ebp", "xchg%Zv %eax,%esi", "xchg%Zv %eax,%edi",
	"cbw",		     "cwd",		  "lcall %Ap",	       "fwait",
	"pushf%Zv",	     "popf%Zv",		  "sahf",	       "lahf",

	/* A */
	"mov%Zb al,%Ob",     "mov%Zv %eax,%Ov",	  "mov%Zb %Ob,al",     "mov%Zv %Ov,%eax",
	"movs%Zb %Xb%Yb",    "movs%Zz %Xv%Yv",	  "cmps%Zb %Xb%Yb",    "cmps%Zz %Xv%Yv",
	"test%Zb al,%Ib",    "test%Zv %eax,%Iv",  "stos%Zb %Yb",       "stos%Zz %Yv",
	"lods%Zb %Xb",	     "lods%Zz %Xv",	  "scas%Zb %Xb",       "scas%Zv %Xv",

	/* B */
	"mov%Zb al,%Ib",     "mov%Zb cl,%Ib",	  "mov%Zb dl,%Ib",     "mov%Zb bl,%Ib",
	"mov%Zb ah,%Ib",     "mov%Zb ch,%Ib",	  "mov%Zb dh,%Ib",     "mov%Zb bh,%Ib",
	"mov%Zv %eax,%Iv",   "mov%Zv %ecx,%Iv",	  "mov%Zv %edx,%Iv",   "mov%Zv %ebx,%Iv",
	"mov%Zv %esp,%Iv",   "mov%Zv %ebp,%Iv",	  "mov%Zv %esi,%Iv",   "mov%Zv %edi,%Iv",

	/* C */
	"%g2%Zb %Eb,%Ib",    "%g2%Zv %Ev,%Ib",	  "ret %Iw",	       "ret",
	"les%Zv %Gv,%Mp",    "lds%Zv %Gv,%Mp",	  "mov%Zb %Eb,%Ib",    "mov%Zv %Ev,%Iv",
	"enter %Iw,%Ib",     "leave",		  "lret %Iw",	       "lret",
	"int 3",	     "int %Ib",		  "into",	       "iret",

	/* D */
	"%g2%Zb %Eb,$1",     "%g2%Zv %Ev,$1",	  "%g2%Zb %Eb,cl",     "%g2%Zv %Ev,cl",
	"aam %rb",	     "aad %rb",		  NULL,		       "xlat",
	"%f0",		     "%f1",		  "%f2",	       "%f3",
	"%f4",		     "%f5",		  "%f6",	       "%f7",

	/* E */
	"loopne %Jb",	     "loope %Jb",	  "loop %Jb",	       "jcxz %Jb",
	"in%Zb %Ib",	     "in%Zv %Ib",	  "out%Zb %Ib,al",     "out%Zv %Ib,%eax",
	"call %Kv",	     "jmp %Jv",		  "ljmp %Ap",	       "jmp %Jb",
	"in%Zb ",	     "in%Zv ",		  "out%Zb dx,al",      "out%Zv dx,%eax",

	/* F */
	"lock",		     NULL,		  "repne",	       "rep",
	"hlt",		     "cmc",		  "%g3",	       "%g0",
	"clc",		     "stc",		  "cli",	       "sti",
	"cld",		     "std",		  "%g4",	       "%g5"
};

/******************************************************************************/

char *op_map_2[] = {
	/* 0 */
	"%g6",		"%g7",		"lar%Zv %Gv,%Ew",	"lsl%Zv %Gv,%Ew", 
	NULL,		NULL,		"clts",			NULL,
	"invd",		"wbinvd",	NULL,			NULL,
	NULL,		NULL,		NULL,			NULL,

	/* 1 */
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,

	/* 2 */
	"mov%Zd %Rd,%Cd", "mov%Zd %Rd,%Dd", "mov%Zd %Cd,%Rd", "mov%Zd %Dd,%Rd",
	"mov%Zd %Rd,%Td", NULL,		"mov%Zd %Td,%Rd",	NULL,
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,

	/* 3 */
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,

	/* 4 */
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,

	/* 5 */
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,

	/* 6 */
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,

	/* 7 */
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,

	/* 8 */
	"jo %Jv",	"jno %Jv",	"jb %Jv",	"jae %Jv",
	"je %Jv",	"jne %Jv",	"jbe %Jv",	"ja %Jv",
	"js %Jv",	"jns %Jv",	"jp %Jv",	"jnp %Jv",
	"jl %Jv",	"jge %Jv",	"jle %Jv",	"jg %Jv",

	/* 9 */
	"seto %Eb",	"setno %Eb",	"setb %Eb",	"setae %Eb",
	"sete %Eb",	"setne %Eb",	"setbe %Eb",	"seta %Eb",
	"sets %Eb",	"setns %Eb",	"setp %Eb",	"setnp %Eb",
	"setl %Eb",	"setge %Eb",	"setle %Eb",	"setg %Eb",

	/* A */
	"push fs",	"pop fs",	NULL,		"bt%Zv %Ev,%Gv",
	"shld%Zv %Ev,%Gv,%Ib",	"shld%Zv %Ev,%Gv,cl",
				"cmpxchg%Zb %Eb,%Gb",	"cmpxchg%Zb %Ev,%Gv",
	"push gs",	"pop gs",	NULL,		"bts%Zv %Ev,%Gv",
	"shrd%Zv %Ev,%Gv,%Ib",	"shrd%Zv %Ev,%Gv,cl",
					NULL, 		"imul%Zv %Gv,%Ev",

	/* B */
	NULL,		NULL,		"lss%Zv %Mp",	"btr%Zv %Ev,%Gv",
	"lfs%Zv %Mp",	"lgs%Zv %Mp",	"movzx%Zb %Gv,%Eb", "movzx%Zw %Gv,%Ew",
	NULL,		NULL,		"%g8%Zv %Ev,%Ib", "btc%Zv %Ev,%Gv",
	"bsf%Zv %Gv,%Ev", "bsr%Zv %Gv,%Ev", "movsx%Zb %Gv,%Eb", "movsx%Zw %Gv,%Ew",

	/* C */
	"xadd%Zb %Eb,%Gb",	"xadd%Zb %Ev,%Gv",	NULL,		NULL,
	NULL,			NULL,			NULL,		NULL,
	"bswap%Zb %Eb,%Gb",	"bswap%Zb %Ev,%Gv",	NULL,		NULL,
	NULL,			NULL,			NULL,		NULL,

	/* D */
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,

	/* E */
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,

	/* F */
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,
	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL
};

/******************************************************************************/

/*
 * Intel groups 1 to 8.
 */
char	*grp_map[9][8] = {
	/* grpStrMap[0][] is the %Ev case for grpStrMap[3][]. */
	"test%Zv %Ev,%Iv", "test%Zv %Ev,%Iv,",	"not%Zv %Ev",	"neg%Zv %Ev",
	"mul%Zv %eax,%Ev", "imul%Zv %eax,%Ev",	"div%Zv %eax,%Ev", "idiv%Zv %eax,%Ev",

	/* grp[1][] */
	"add",	"or",	"adc",	"sbb",	"and",	"sub",	"xor",	"cmp",

	/* grp[2][] */
	"rol",	"ror",	"rcl",	"rcr",	"shl",	"shr",	"shl",	"sar",

	/* grp[3][] */
	"test%Zb %Eb,%Ib", "test%Zb %Eb,%Ib,",	"not%Zb %Eb",	 "neg%Zb %Eb",
	"mul%Zb al,%Eb",   "imul%Zb al,%Eb",	"div%Zb al,%Eb", "idiv%Zb al,%Eb",

	/* grp[4][] */
	"inc%Zb %Eb",	"dec%Zb %Eb",	NULL,		NULL,
	NULL,		NULL,		NULL,		NULL,

	/* grp[5][] */
	"inc%Zv %Ev",	"dec%Zv %Ev",	"icall %Ev",	"ilcall %Ep",
	"ijmp %Ev",	"iljmp %Ep",	"push%Zv %Ev",	NULL,

	/* grp[6][] */
	"sldt %Ew",	"str %Ew",	"lldt %Ew",	"ltr %Ew",
	"verr %Ew",	"verw %Ew",	NULL,		NULL,

	/* grp[7][] */
	"sgdt %Ms",	"sidt %Ms",	"lgdt%Zv %Ms",	"lidt%Zv %Ms",
	"smsw %Ew",	NULL,		"lmsw %Ew",	"invlpg %Ms",

	/* grp[8][] */
	NULL,	NULL,	NULL,	NULL,	"bt",	"bts",	"btr",	"btc"
};

/******************************************************************************/

/*
 * NDP opcode tables.
 * Each opcode is preceded by an implicit 'f'.
 */

/* NDPtab[] is used with memory operand when mod!=3. */
char *NDP_op[8][8] = {
/*	      /0       /1       /2        /3       /4        /5       /6        /7 */
/* 0xD8 */  "adds",  "muls",  "coms",  "comps",  "subs",  "subrs",  "divs",  "divrs",
/* 0xD9 */   "lds",    NULL,   "sts",   "stps", "ldenv",   "ldcw","nstenv",  "nstcw",
/* 0xDA */ "iaddl", "imull", "icoml", "icompl", "isubl", "isubrl", "idivl", "idivrl",
/* 0xDB */  "ildl",    NULL,  "istl",  "istpl",    NULL,    "ldt",    NULL,   "stpt",
/* 0xDC */  "addl",  "mull",  "coml",  "compl",  "subl",  "subrl",  "divl",  "divrl",
/* 0xDD */   "ldl",    NULL,   "stl",   "stpl", "rstor",     NULL, "nsave",    "stsw",
/* 0xDE */ "iadds", "imuls", "icoms", "icomps", "isubs", "isubrs", "idivs", "idivrs",
/* 0xDF */  "ilds",    NULL,  "ists",  "istps",   "bld",  "ildll",  "bstp",  "istpll"
};

/*
 * NDPtab3[] is used when mod==3.
 * The opcode always takes an NDP register from the r/m field as operand.
 * The first character indicates NDP register operand format:
 *	1	op takes one fpreg operand "%st(n)"
 *	2	op takes operands "%st(0), %st(n)"
 *	3	op takes operands "%st(n), %st(0)"
 */
char *NDP_op_3[8][8] = {
/*		0xC0	0xC8	0xD0	0xD8	0xE0	0xE8	0xF0	0xF8 */
/* 0xD8 */	"2add",	"2mul",	"2com","1comp",	"2sub",	"2subr","2div", "2divr",
/* 0xD9 */ 	"1ld",	"2xch",	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,
/* 0xDA */ 	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,
/* 0xDB */ 	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,
/* 0xDC */ 	"3add",	"3mul",	NULL,	NULL,	"3sub",	"3subr","3div",	"3divr",
/* 0xDD */ 	"1free",NULL,	"1st",	"1stp",	"3ucom","3ucomp", NULL,	NULL,
/* 0xDE */  	"3addp","3mulp",NULL,	NULL,  "3subp","3subrp","3divp","3divrp",
/* 0xDF */ 	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL,	NULL
};

/*
 * NDP_misc_op[] is used when mod==3 and the NDP_op_3[][] contains NULL.
 * This 0-terminated sorted list is searched linearly for the remaining opcodes;
 * they are too sparse for efficient table lookup.
 */
NDPMTAB	NDP_misc_op[] = {
	{ 0xD9D0,	"nop" },
	{ 0xD9E0,	"chs" },
	{ 0xD9E1,	"abs" },
	{ 0xD9E4,	"tst" },
	{ 0xD9E5,	"xam" },
	{ 0xD9E8,	"ld1" },
	{ 0xD9E9,	"ldl2t" },
	{ 0xD9EA,	"ldl2e" },
	{ 0xD9EB,	"ldpi" },
	{ 0xD9EC,	"ldlg2" },
	{ 0xD9ED,	"ldln2" },
	{ 0xD9EE,	"ldz" },
	{ 0xD9F0,	"2xm1" },
	{ 0xD9F1,	"yl2x" },
	{ 0xD9F2,	"ptan" },
	{ 0xD9F3,	"patan" },
	{ 0xD9F4,	"xtract" },
	{ 0xD9F5,	"prem1" },
	{ 0xD9F6,	"decstp" },
	{ 0xD9F7,	"incstp" },
	{ 0xD9F8,	"prem" },
	{ 0xD9F9,	"yl2xp1" },
	{ 0xD9FA,	"sqrt" },
	{ 0xD9FB,	"sincos" },
	{ 0xD9FC,	"rndint" },
	{ 0xD9FD,	"scale" },
	{ 0xD9FE,	"sin" },
	{ 0xD9FF,	"cos" },
	{ 0xDAE9,	"ucompp" },
	{ 0xDBE2,	"nclex" },
	{ 0xDBE3,	"ninit" },
	{ 0xDBE4,	"setpm" },
	{ 0xDBF4,	"rstpm" },
	{ 0xDED9,	"compp" },
	{ 0xDFE0,	"nstsw\t%ax" },
	{ 0,		NULL }
};

/******************************************************************************/

/* 
 * i80386 registers.
 */

/* Segment registers. */
char	*segReg[] = {"es", "cs", "ss", "ds", "fs", "gs" };

/* General registers. */
char	*genReg[3][8] = {
/*		    0       1       2       3       4       5       6       7 */
/* [0] 8-bit  */  "%al",  "%cl",  "%dl",  "%bl",  "%ah",  "%ch",  "%dh",  "%bh",
/* [1] 16-bit */  "%ax",  "%cx",  "%dx",  "%bx",  "%sp",  "%bp",  "%si",  "%di",
/* [2] 32-bit */ "%eax", "%ecx", "%edx", "%ebx", "%esp", "%ebp", "%esi", "%edi"
};

/* Control registers. */
char *ctrlReg[] = {
	"%cr0",	"%cr1", "%cr2", "%cr3"	
};

/* Debug registers. */
char *dbgReg[] = {
	"%dr0", "%dr1", "%dr2", "%dr3", "%dr4", "%dr5", "%dr6", "%dr7"	
};

/* Test registers. */
char *tstReg[] = {
	NULL, NULL, NULL, NULL, NULL, NULL, "%tr6", "%tr7"	
};

/* NDP registers. */
char *NDPReg[] = {
	"%st", "%st(1)", "%st(2)", "%st(3)", "%st(4)", "%st(5)", "%st(6)", "%st(7)"
};

/******************************************************************************/

/*
 * Indirect addressing mode tables.
 */

/* 16 and 32 bit indirect addressing with the mod r/m byte. */
char *modRMtab[2][8] = {

	/* [0] 16 bit indirect addressing. */
	"(%bx)(%si)",	"(%bx)(%di)",	"(%bp)(%si)",	"(%bp)(%di)",
	"(%si)",	"(%di)",	"(%bp)",	"(%bx)",

	/* [1] 32 bit indirect addressing. */
	"(%eax)",	"(%ecx)",	"(%edx)",	"(%ebx)", 
	"(%esp)",	"(%ebp)",	"(%esi)",	"(%edi)"		
};

/* 32 bit indirect indexed addressing forms with the SIB byte. */
char	*sibtab[4][8] = { 

	/* [0] scale by 1 */
	"(%eax)",	"(%ecx)",	"(%edx)",	"(%ebx)",
	NULL,		"(%ebp)",	"(%esi)",	"(%edi)",

	/* [1] scale by 2 */
	"(%eax*2)",	"(%ecx*2)",	"(%edx*2)",	"(%ebx*2)",
	 NULL,		"(%ebp*2)",	"(%esi*2)",	"(%edi*2)",

	/* [2] scale by 4 */
	"(%eax*4)",	"(%ecx*4)",	"(%edx*4)",	"(%ebx*4)", 
	 NULL,		"(%ebp*4)",	"(%esi*4)",	"(%edi*4)",

	/* [3] scale by 8 */
	"(%eax*8)",	"(%ecx*8)",	"(%edx*8)",	"(%ebx*8)", 
	 NULL,		"(%ebp*8)",	"(%esi*8)",	"(%edi*8)"
};

/******************************************************************************/

/* end of db/i386/i386db3.c */
