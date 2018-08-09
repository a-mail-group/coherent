
#line 8 "/u/steve/sh.fns/sh.y"

#include "sh.h"

#define YYERROR	{ yyerrflag=1; goto YYerract; }

extern	NODE	*node();

#include "y.tab.h"
#define YYCLEARIN yychar = -1000
#define YYERROK yyerrflag = 0
extern int yychar;
extern short yyerrflag;
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 150
#endif
YYSTYPE yyval, yylval;

#line 357 "/u/steve/sh.fns/sh.y"

/*
 * Create a node.
 */
NODE *
node(type, auxp, next)
NODE *auxp, *next;
{
	register NODE *np;

	np = (NODE *) balloc(sizeof (NODE));
	np->n_type = type;
	np->n_auxp = auxp;
	np->n_next = next;
	return np;
}

#define NBPC 8
#define NKEY 8
static char keys[NKEY] = { 0 };
static int  keyi = NKEY * NBPC;

keyflush()
{
	register char *kp;

	for (kp = keys+NKEY; kp > keys; *--kp = 0)
		;
	keyi = NKEY * NBPC;
}

keypop()
{
	register char	*kp;
	register int	km;

	if ((km = keyi++) >= NKEY * NBPC) {
		panic(11);
		NOTREACHED;
	}
	kp = keys + (km / NBPC);
	km = 1 << (km %= NBPC);
	keyflag = (*kp & km) ? 1 : 0;
	*kp &= ~km;
}

keypush()
{
	register char	*kp;
	register int	km;

	if ((km = --keyi) < 0) {
		panic(12);
		NOTREACHED;
	}
	if (keyflag) {
		kp = keys + (km / NBPC);
		km = 1 << (km %= NBPC);
		*kp |= km;
	}
}
/*
 * The following fragments might implement named pipes.
 * The token declaration goes in the header.
 * The nopen production should go with the others of its ilk.
 * The production fragment goes into arg:
%token _NOPEN _NCLOSE
nopen:	_NOPEN optnls ;

|	nopen pipe_cmd ')' {
		$$ = node(NRPIPE, $2, NULL);
	}
|	oparen pipe_cmd _NCLOSE {
		$$ = node(NWPIPE, $2, NULL);
	}
 *
 */
#ifdef YYTNAMES
readonly struct yytname yytnames[33] =
{
	"$end", -1, 
	"error", -2, 
	"_ANDF", 256, 
	"_ASGN", 257, 
	"_CASE", 258, 
	"_CBRAC", 259, 
	"_DO", 260, 
	"_DONE", 261, 
	"_DSEMI", 262, 
	"_ELIF", 263, 
	"_ELSE", 264, 
	"_ESAC", 265, 
	"_FI", 266, 
	"_FOR", 267, 
	"_IF", 268, 
	"_IN", 269, 
	"_IORS", 270, 
	"_NAME", 271, 
	"_NULL", 272, 
	"_OBRAC", 273, 
	"_ORF", 274, 
	"_PARENS", 275, 
	"_RET", 276, 
	"_THEN", 277, 
	"_UNTIL", 278, 
	"_WHILE", 279, 
	"'\\n'", 10, 
	"';'", 59, 
	"'|'", 124, 
	"'('", 40, 
	"')'", 41, 
	"'&'", 38, 
	NULL
} ;
#endif
#include <action.h>
readonly unsigned char yypdnt[81] = {
0, 1, 1, 2, 2, 2, 4, 6,
7, 8, 9, 9, 10, 10, 11, 11,
13, 14, 15, 16, 17, 18, 19, 3,
3, 3, 3, 3, 20, 20, 20, 21,
21, 22, 22, 22, 23, 24, 24, 26,
26, 26, 26, 27, 25, 28, 29, 29,
29, 29, 29, 29, 29, 29, 29, 30,
30, 36, 36, 32, 32, 32, 37, 38,
38, 31, 31, 35, 35, 35, 34, 34,
33, 33, 12, 12, 12, 5, 5, 39,
39 
};
readonly unsigned char yypn[81] = {
2, 2, 0, 1, 2, 2, 2, 2,
2, 2, 2, 2, 2, 3, 1, 2,
2, 2, 2, 2, 2, 2, 2, 1,
2, 2, 3, 3, 1, 3, 3, 3,
1, 2, 2, 1, 0, 2, 1, 1,
1, 1, 1, 1, 1, 1, 6, 5,
6, 5, 4, 6, 3, 3, 6, 2,
0, 2, 0, 3, 1, 0, 3, 3,
1, 2, 0, 5, 2, 0, 1, 0,
3, 2, 1, 1, 2, 1, 0, 1,
2 
};
readonly unsigned char yypgo[40] = {
0, 0, 2, 4, 12, 14, 48, 52,
54, 56, 58, 60, 64, 70, 72, 74,
76, 78, 82, 84, 86, 92, 96, 98,
100, 104, 120, 122, 124, 126, 128, 130,
136, 142, 154, 168, 172, 176, 178, 182
};
readonly unsigned int yygo[194] = {
YYOTHERS, 0x1, YYOTHERS, 0x6, 0x1, 0x7, 0x10, 0x2b,
0x11, 0x2c, YYOTHERS, 0x3c, YYOTHERS, 0x21, 0x13, 0x30,
0x16, 0x33, 0x1b, 0x37, 0x1d, 0x38, 0x1e, 0x39,
0x1f, 0x3a, 0x20, 0x3b, 0x3c, 0x4c, 0x42, 0x55,
0x4e, 0x62, 0x50, 0x65, 0x64, 0x78, 0x6b, 0x7b,
0x6d, 0x7d, 0x73, 0x81, 0x74, 0x82, YYOTHERS, 0x2e,
0x83, 0x87, YYOTHERS, 0x4f, YYOTHERS, 0x75, YYOTHERS, 0x76,
YYOTHERS, 0x22, YYOTHERS, 0x51, 0x48, 0x5c, YYOTHERS, 0x47,
0x45, 0x56, 0x4b, 0x5f, YYOTHERS, 0x48, YYOTHERS, 0x14,
YYOTHERS, 0x15, YYOTHERS, 0x17, YYOTHERS, 0x23, 0x55, 0x68,
YYOTHERS, 0x24, YYOTHERS, 0x6e, YYOTHERS, 0x6c, 0x14, 0x31,
0x15, 0x32, YYOTHERS, 0x8, 0x17, 0x34, YYOTHERS, 0x9,
YYOTHERS, 0xa, YYOTHERS, 0xb, 0x27, 0x43, YYOTHERS, 0x25,
0x4, 0xe, 0xb, 0x26, 0x19, 0x35, 0x1a, 0x36,
0x27, 0x26, 0x4a, 0x5d, 0x5d, 0x5d, YYOTHERS, 0x58,
YYOTHERS, 0x27, YYOTHERS, 0x28, YYOTHERS, 0x29, YYOTHERS, 0x2a,
YYOTHERS, 0x4b, 0x4b, 0x60, 0x5f, 0x71, YYOTHERS, 0x52,
0x5c, 0x6f, 0x6c, 0x7c, YYOTHERS, 0x59, 0x21, 0x3d,
0x22, 0x3e, 0x4d, 0x61, 0x68, 0x79, 0x75, 0x83,
YYOTHERS, 0x3f, 0x24, 0x41, 0x4f, 0x63, 0x51, 0x66,
0x6e, 0x7e, 0x76, 0x84, 0x87, 0x88, YYOTHERS, 0x40,
0x88, 0x89, YYOTHERS, 0x77, 0x5d, 0x70, YYOTHERS, 0x5e,
YYOTHERS, 0x5a, 0x69, 0x7a, YYOTHERS, 0x5b, 0x35, 0x49,
0x3c, 0x4d, 0x45, 0x49, 0x46, 0x57, 0x4b, 0x49,
YYOTHERS, 0x2f 
};
readonly unsigned short yypa[138] = {
0, 2, 30, 34, 38, 42, 44, 46,
50, 56, 62, 66, 88, 90, 92, 94,
96, 120, 144, 144, 148, 148, 144, 152,
156, 158, 158, 144, 162, 144, 144, 144,
144, 152, 152, 164, 170, 176, 178, 182,
204, 206, 208, 210, 212, 214, 216, 218,
222, 224, 226, 228, 230, 232, 240, 244,
246, 248, 250, 252, 144, 254, 258, 262,
264, 268, 144, 272, 274, 276, 282, 286,
290, 294, 298, 302, 310, 312, 144, 338,
348, 354, 360, 364, 366, 368, 372, 374,
378, 382, 386, 390, 286, 298, 394, 258,
396, 400, 402, 404, 144, 410, 412, 414,
152, 416, 420, 144, 286, 144, 422, 430,
434, 436, 440, 144, 144, 152, 442, 448,
452, 454, 458, 460, 462, 464, 466, 468,
470, 472, 474, 254, 476, 478, 480, 338,
482, 488 
};
readonly unsigned int yyact[490] = {
0x2002, YYOTHERS, 0x2, YYEOFVAL, 0x4, 0x114, 0x5, 0xa,
0x2024, 0x101, 0x2024, 0x102, 0x2024, 0x10b, 0x2024, 0x10c,
0x2024, 0x10e, 0x2024, 0x10f, 0x2024, 0x111, 0x2024, 0x116,
0x2024, 0x117, 0x2024, 0x28, 0x3, YYOTHERS, 0x4000, YYEOFVAL,
0x6000, YYOTHERS, 0xc, 0xa, 0x6000, YYOTHERS, 0xd, 0x10f,
0x2023, YYOTHERS, 0x2003, YYOTHERS, 0x2001, YYOTHERS, 0xf, 0xa,
0x6000, YYOTHERS, 0x10, 0x3b, 0x11, 0x26, 0x2017, YYOTHERS,
0x12, 0x100, 0x13, 0x112, 0x201c, YYOTHERS, 0x16, 0x7c,
0x2020, YYOTHERS, 0x18, 0x101, 0x19, 0x102, 0x1a, 0x10b,
0x1b, 0x10c, 0x1c, 0x10e, 0xd, 0x10f, 0x1d, 0x111,
0x1e, 0x116, 0x1f, 0x117, 0x20, 0x28, 0x6000, YYOTHERS,
0x2005, YYOTHERS, 0x202c, YYOTHERS, 0x2022, YYOTHERS, 0x2004, YYOTHERS,
0x4, 0x114, 0x2024, 0x101, 0x2024, 0x102, 0x2024, 0x10b,
0x2024, 0x10c, 0x2024, 0x10e, 0x2024, 0x10f, 0x2024, 0x111,
0x2024, 0x116, 0x2024, 0x117, 0x2024, 0x28, 0x2019, YYOTHERS,
0x4, 0x114, 0x2024, 0x101, 0x2024, 0x102, 0x2024, 0x10b,
0x2024, 0x10c, 0x2024, 0x10e, 0x2024, 0x10f, 0x2024, 0x111,
0x2024, 0x116, 0x2024, 0x117, 0x2024, 0x28, 0x2018, YYOTHERS,
0x2d, 0xa, 0x204e, YYOTHERS, 0x4, 0x114, 0x2024, YYOTHERS,
0x4, 0x114, 0x2024, YYOTHERS, 0x202d, YYOTHERS, 0xd, 0x10f,
0x6000, YYOTHERS, 0x202b, YYOTHERS, 0x4, 0x114, 0x2047, 0x29,
0x2024, YYOTHERS, 0x4, 0x114, 0x2047, 0x103, 0x2024, YYOTHERS,
0x2021, YYOTHERS, 0x42, 0x113, 0x2028, YYOTHERS, 0x18, 0x101,
0x19, 0x102, 0x1a, 0x10b, 0x1b, 0x10c, 0x1c, 0x10e,
0xd, 0x10f, 0x1d, 0x111, 0x1e, 0x116, 0x1f, 0x117,
0x20, 0x28, 0x2026, YYOTHERS, 0x2027, YYOTHERS, 0x2029, YYOTHERS,
0x202a, YYOTHERS, 0x201b, YYOTHERS, 0x201a, YYOTHERS, 0x204f, YYOTHERS,
0x2011, YYOTHERS, 0x44, 0xa, 0x204d, YYOTHERS, 0x2010, YYOTHERS,
0x201d, YYOTHERS, 0x201e, YYOTHERS, 0x2012, YYOTHERS, 0x201f, YYOTHERS,
0x45, 0x10d, 0x2d, 0xa, 0x46, 0x3b, 0x6000, YYOTHERS,
0x4a, 0x10d, 0x2038, YYOTHERS, 0x2006, YYOTHERS, 0x2014, YYOTHERS,
0x200b, YYOTHERS, 0x200a, YYOTHERS, 0x2013, YYOTHERS, 0x4e, 0x115,
0x6000, YYOTHERS, 0x50, 0x104, 0x2042, YYOTHERS, 0x2046, YYOTHERS,
0x53, 0x29, 0x6000, YYOTHERS, 0x54, 0x103, 0x6000, YYOTHERS,
0x2025, YYOTHERS, 0x2050, YYOTHERS, 0x2d, 0xa, 0x46, 0x3b,
0x200e, YYOTHERS, 0x2d, 0xa, 0x204b, YYOTHERS, 0xd, 0x10f,
0x203d, YYOTHERS, 0x45, 0x10d, 0x6000, YYOTHERS, 0x44, 0xa,
0x204a, YYOTHERS, 0xd, 0x10f, 0x203a, YYOTHERS, 0x50, 0x104,
0x2d, 0xa, 0x46, 0x3b, 0x2042, YYOTHERS, 0x2049, YYOTHERS,
0x4, 0x114, 0x44, 0xa, 0x204d, 0x103, 0x204d, 0x104,
0x204d, 0x105, 0x204d, 0x106, 0x204d, 0x107, 0x204d, 0x108,
0x204d, 0x109, 0x204d, 0x10a, 0x204d, 0x115, 0x204d, 0x29,
0x2024, YYOTHERS, 0x4, 0x114, 0x2047, 0x107, 0x2047, 0x108,
0x2047, 0x10a, 0x2024, YYOTHERS, 0x2d, 0xa, 0x64, 0x3b,
0x204e, YYOTHERS, 0x4, 0x114, 0x2047, 0x105, 0x2024, YYOTHERS,
0x67, 0x105, 0x6000, YYOTHERS, 0x2034, YYOTHERS, 0x2035, YYOTHERS,
0x1d, 0x111, 0x6000, YYOTHERS, 0x200f, YYOTHERS, 0x44, 0xa,
0x204c, YYOTHERS, 0x69, 0x7c, 0x2040, YYOTHERS, 0x6a, 0x109,
0x6000, YYOTHERS, 0x6b, 0x106, 0x203c, YYOTHERS, 0x6d, 0x29,
0x6000, YYOTHERS, 0x2037, YYOTHERS, 0x72, 0x105, 0x6000, YYOTHERS,
0x2048, YYOTHERS, 0x2007, YYOTHERS, 0x73, 0x107, 0x74, 0x108,
0x2045, YYOTHERS, 0x200c, YYOTHERS, 0x2041, YYOTHERS, 0x2032, YYOTHERS,
0xd, 0x10f, 0x6000, YYOTHERS, 0x2031, YYOTHERS, 0x4, 0x114,
0x2047, 0x106, 0x2047, 0x109, 0x2024, YYOTHERS, 0x7f, 0x109,
0x6000, YYOTHERS, 0x2039, YYOTHERS, 0x80, 0x105, 0x6000, YYOTHERS,
0x202f, YYOTHERS, 0x4, 0x114, 0x2047, 0x10a, 0x2024, YYOTHERS,
0x85, 0x10a, 0x6000, YYOTHERS, 0x200d, YYOTHERS, 0x86, 0x103,
0x6000, YYOTHERS, 0x203f, YYOTHERS, 0x2016, YYOTHERS, 0x203b, YYOTHERS,
0x2015, YYOTHERS, 0x203e, YYOTHERS, 0x2030, YYOTHERS, 0x202e, YYOTHERS,
0x2008, YYOTHERS, 0x2009, YYOTHERS, 0x2044, YYOTHERS, 0x2033, YYOTHERS,
0x2036, YYOTHERS, 0x73, 0x107, 0x74, 0x108, 0x2045, YYOTHERS,
0x2043, YYOTHERS 
};
#define YYNOCHAR (-1000)
#define	yyerrok	yyerrflag=0
#define	yyclearin	yylval=YYNOCHAR
int yystack[YYMAXDEPTH];
YYSTYPE yyvstack[YYMAXDEPTH], *yyv;
int yychar;

#ifdef YYDEBUG
int yydebug = 1;	/* No sir, not in the BSS */
#include <stdio.h>
#endif

short yyerrflag;
int *yys;

yyparse()
{
	register YYSTYPE *yypvt;
	int act;
	register unsigned *ip, yystate;
	int pno;
	yystate = 0;
	yychar = YYNOCHAR;
	yyv = &yyvstack[-1];
	yys = &yystack[-1];

stack:
	if( ++yys >= &yystack[YYMAXDEPTH] ) {
		write(2, "Stack overflow\n", 15);
		exit(1);
	}
	*yys = yystate;
	*++yyv = yyval;
#ifdef YYDEBUG
	if( yydebug )
		fprintf(stdout, "Stack state %d, char %d\n", yystate, yychar);
#endif

read:
	ip = &yyact[yypa[yystate]];
	if( ip[1] != YYNOCHAR ) {
		if( yychar == YYNOCHAR ) {
			yychar = yylex();
#ifdef YYDEBUG
			if( yydebug )
				fprintf(stdout, "lex read char %d, val %d\n", yychar, yylval);
#endif
		}
		while (ip[1]!=YYNOCHAR) {
			if (ip[1]==yychar)
				break;
			ip += 2;
		}
	}
	act = ip[0];
	switch( act>>YYACTSH ) {
	case YYSHIFTACT:
		if( ip[1]==YYNOCHAR )
			goto YYerract;
		if( yychar != -1 )
			yychar = YYNOCHAR; /* dont throw away EOF */
		yystate = act&YYAMASK;
		yyval = yylval;
#ifdef YYDEBUG
		if( yydebug )
			fprintf(stdout, "shift %d\n", yystate);
#endif
		if( yyerrflag )
			--yyerrflag;
		goto stack;

	case YYACCEPTACT:
#ifdef YYDEBUG
		if( yydebug )
			fprintf(stdout, "accept\n");
#endif
		return(0);

	case YYERRACT:
	YYerract:
		switch (yyerrflag) {
		case 0:
			yyerror("Syntax error");

		case 1:
		case 2:

			yyerrflag = 3;
			while( yys >= & yystack[0] ) {
				ip = &yyact[yypa[*yys]];
				while( ip[1]!=YYNOCHAR )
					ip += 2;
				if( (*ip&~YYAMASK) == (YYSHIFTACT<<YYACTSH) ) {
					yystate = *ip&YYAMASK;
					goto stack;
				}
#ifdef YYDEBUG
				if( yydebug )
					fprintf(stderr, "error recovery leaves state %d, uncovers %d\n", *yys, yys[-1]);
#endif
				yys--;
				yyv--;
			}
#ifdef YYDEBUG
			if( yydebug )
				fprintf(stderr, "no shift on error; abort\n");
#endif
			return(1);

		case 3:
#ifdef YYDEBUG
			if( yydebug )
				fprintf(stderr, "Error recovery clobbers char %o\n", yychar);
#endif
			if( yychar==YYEOFVAL )
				return(1);
			yychar = YYNOCHAR;
			goto read;
		}

	case YYREDACT:
		pno = act&YYAMASK;
#ifdef YYDEBUG
		if( yydebug )
			fprintf(stdout, "reduce %d\n", pno);
#endif
		yypvt = yyv;
		yyv -= yypn[pno];
		yys -= yypn[pno];
		yyval = yyv[1];
		switch(pno) {

case 3: {

#line 65 "/u/steve/sh.fns/sh.y"

		sesp->s_node = NULL;
		reset(RCMD);
		NOTREACHED;
	}break;

case 4: {

#line 71 "/u/steve/sh.fns/sh.y"

		sesp->s_node = yypvt[-1].yu_node;
		reset(errflag ? RERR : RCMD);
		NOTREACHED;
	}break;

case 5: {

#line 76 "/u/steve/sh.fns/sh.y"

		keyflush();
		keyflag = 1;
		reset(RERR);
		NOTREACHED;
	}break;

case 10: {

#line 92 "/u/steve/sh.fns/sh.y"
	yyval.yu_nval = NWHILE;	}break;

case 11: {

#line 93 "/u/steve/sh.fns/sh.y"
	yyval.yu_nval = NUNTIL;	}break;

case 23: {

#line 115 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = yypvt[0].yu_node;
	}break;

case 24: {

#line 118 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NBACK, yypvt[-1].yu_node, NULL);
	}break;

case 25: {

#line 121 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = yypvt[-1].yu_node;
	}break;

case 26: {

#line 124 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NBACK, yypvt[-2].yu_node, yypvt[0].yu_node);
	}break;

case 27: {

#line 127 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NLIST, yypvt[-2].yu_node, yypvt[0].yu_node);
	}break;

case 28: {

#line 133 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = yypvt[0].yu_node;
	}break;

case 29: {

#line 136 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NORF, yypvt[-2].yu_node, yypvt[0].yu_node);
	}break;

case 30: {

#line 139 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NANDF, yypvt[-2].yu_node, yypvt[0].yu_node);
	}break;

case 31: {

#line 145 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NPIPE, yypvt[-2].yu_node, yypvt[0].yu_node);
	}break;

case 32: {

#line 148 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = yypvt[0].yu_node;
	}break;

case 33: {

#line 154 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NCOMS, yypvt[0].yu_node, NULL);
		keypop();
	}break;

case 34: {

#line 158 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NRET, yypvt[0].yu_strp, NULL);
	}break;

case 35: {

#line 161 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NRET, "", NULL);
	}break;

case 36: {

#line 167 "/u/steve/sh.fns/sh.y"

		keypush();
		keyflag = 1;
	}break;

case 37: {

#line 174 "/u/steve/sh.fns/sh.y"

		if ((yypvt[-1].yu_node->n_type == NCTRL && yypvt[0].yu_node->n_type == NARGS)
		 || (yypvt[-1].yu_node->n_type == NARGS && yypvt[0].yu_node->n_type == NCTRL)) {
			YYERROR;
		}
		(yyval.yu_node = yypvt[-1].yu_node)->n_next = yypvt[0].yu_node;
	}break;

case 38: {

#line 181 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = yypvt[0].yu_node;
	}break;

case 39: {

#line 187 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NIORS, yypvt[0].yu_strp, NULL);
	}break;

case 40: {

#line 190 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NARGS, yypvt[0].yu_strp, NULL);
		keyflag = 0;
	}break;

case 41: {

#line 194 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NASSG, yypvt[0].yu_strp, NULL);
	}break;

case 42: {

#line 197 "/u/steve/sh.fns/sh.y"

		if (!keyflag) {
			YYERROR;
		}
		yyval.yu_node = node(NCTRL, yypvt[0].yu_node, NULL);
		keyflag = 0;
	}break;

case 43: {

#line 206 "/u/steve/sh.fns/sh.y"

		yyval.yu_strp = duplstr(strt, 0);
	}break;

case 44: {

#line 211 "/u/steve/sh.fns/sh.y"

		yyval.yu_strp = duplstr(strt, 0);
	}break;

case 45: {

#line 216 "/u/steve/sh.fns/sh.y"

		yyval.yu_strp = duplstr(strt, 0);
	}break;

case 46: {

#line 222 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NFOR, yypvt[-4].yu_strp, node(NFOR2, yypvt[-3].yu_node, node(NLIST, yypvt[-1].yu_node, NULL)));
		yyval.yu_node->n_next->n_next->n_next = yyval.yu_node->n_next;
	}break;

case 47: {

#line 226 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NFOR, yypvt[-3].yu_strp, node(NFOR2, yypvt[-2].yu_node, node(NLIST, yypvt[-1].yu_node, NULL)));
		yyval.yu_node->n_next->n_next->n_next = yyval.yu_node->n_next;
	}break;

case 48: {

#line 230 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NCASE, yypvt[-4].yu_strp, yypvt[-1].yu_node);
	}break;

case 49: {

#line 233 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NCASE, yypvt[-3].yu_strp, yypvt[-1].yu_node);
	}break;

case 50: {

#line 236 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(yypvt[-3].yu_nval, yypvt[-2].yu_node, node(NLIST, yypvt[-1].yu_node, NULL));
		yyval.yu_node->n_next->n_next = yyval.yu_node;
	}break;

case 51: {

#line 240 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NIF, node(NNULL, yypvt[-4].yu_node, yypvt[-2].yu_node), yypvt[-1].yu_node);
	}break;

case 52: {

#line 243 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NPARN, yypvt[-1].yu_node, NULL);
	}break;

case 53: {

#line 246 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NBRAC, yypvt[-1].yu_node, NULL);
	}break;

case 54: {

#line 249 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NFUNC, yypvt[-5].yu_strp, yypvt[-1].yu_node);
	}break;

case 55: {

#line 255 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = yypvt[0].yu_node;
	}break;

case 56: {

#line 258 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NARGS, "\"$@\"", NULL);
	}break;

case 57: {

#line 264 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NARGS, yypvt[-1].yu_strp, yypvt[0].yu_node);
	}break;

case 58: {

#line 267 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = NULL;
	}break;

case 59: {

#line 273 "/u/steve/sh.fns/sh.y"

		register NODE *np;

		for (np=yypvt[-2].yu_node; np->n_next; np=np->n_next)
			;
		np->n_next = yypvt[0].yu_node;
		yyval.yu_node = yypvt[-2].yu_node;
	}break;

case 60: {

#line 281 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = yypvt[0].yu_node;
	}break;

case 61: {

#line 284 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = NULL;
	}break;

case 62: {

#line 290 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NCASE2, yypvt[0].yu_node, yypvt[-2].yu_node);
	}break;

case 63: {

#line 296 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NCASE3, yypvt[-2].yu_strp, yypvt[0].yu_node);
	}break;

case 64: {

#line 299 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NCASE3, yypvt[0].yu_strp, NULL);
	}break;

case 65: {

#line 305 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = yypvt[0].yu_node;
	}break;

case 66: {

#line 308 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = NULL;
	}break;

case 67: {

#line 314 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NIF, node(NNULL, yypvt[-3].yu_node, yypvt[-1].yu_node), yypvt[0].yu_node);
	}break;

case 68: {

#line 317 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NELSE, yypvt[0].yu_node, NULL);
	}break;

case 69: {

#line 320 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = NULL;
	}break;

case 70: {

#line 326 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = yypvt[0].yu_node;
	}break;

case 71: {

#line 330 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = NULL;
	}break;

case 72: {

#line 336 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = node(NLIST, yypvt[-2].yu_node, yypvt[0].yu_node);
	}break;

case 73: {

#line 339 "/u/steve/sh.fns/sh.y"

		yyval.yu_node = yypvt[-1].yu_node;
	}break;

		}
		ip = &yygo[ yypgo[yypdnt[pno]] ];
		while( *ip!=*yys && *ip!=YYNOCHAR )
			ip += 2;
		yystate = ip[1];
		goto stack;
	}
}




