/*
 * tty.c
 * Nroff.
 * nroff output writer, aka TTY driver.
 */

/*
 * Print model notes:
 * Prior to 7/94, this nroff back end performed spacing with \b, \n, \r,
 * and the escape sequences RLF HRLF HLF LF (i.e. <Esc> [789B]).
 * \r implies movement to horizontal position 0 with no vertical movement (CR).
 * \n implies movement to both horizontal and vertical position 0 (CR + LF).
 * Escape sequences LF and RLF imply movement to the next or previous
 * line with no horizontal movement.
 * Escape sequences HLF and HRLF imply movement to the next or previous
 * half-line with no horizontal movement.
 *
 * fred noted 7/94 that the half-line movement escapes caused problems for e.g.
 *	.sp .5
 * and that the output was inconsistent with gnroff.
 * Accordingly, steve eliminated all use of half-line motions.
 * gnroff performs reverse line motions by building a virtual output page
 * and writing the entire page when done, so there are no reverse movement
 * characters in the output.  While this is clearly the right thing to do,
 * it requires fairly extensive recoding and is not done at this time.
 * Instead, this source still uses the escapes LF and RLF for now,
 * a simple back-end filter could be used to build the virtual page if desired.
 */

#include <ctype.h>
#include "roff.h"

/* Font indices. */
#define	FONTR	0			/* Roman			*/
#define	FONTB	1			/* Bold				*/
#define	FONTI	2			/* Italic			*/

/* Special escape sequences, meaningless for real devices; see comment above. */
#define	RLF	"\0337"			/* Reverse line feed		*/
#define	LF	"\033B"			/* Line feed			*/

/*
 * Device parameters.
 */
int	ntroff	=	NROFF;		/* Programme is NROFF type	*/
long	semmul	=	3;		/* Multiplier for em space	*/
long	semdiv	=	5;		/* Divisor for em space		*/
long	senmul	=	3;		/* Multiplier for en space	*/
long	sendiv	=	5;		/* Divisor for en space		*/
long	shrmul	=	12;		/* Horizontal resolution (mul)	*/
long	shrdiv	=	1;		/* Horizontal resolution (div)	*/
long	sinmul	=	120;		/* Multiplier for inch		*/
long	sindiv	=	1;		/* Divisor for inch		*/
long	snrmul	=	0;		/* Narrow space (mul)		*/
long	snrdiv	=	1;		/* Narrow space (div)		*/
long	svrmul	=	20;		/* Vertical resolution (mul)	*/
long	svrdiv	=	1;		/* Vertical resolution (div)	*/

/*
 * Map user fontnames to font numbers.
 */
FTB fontab[NFNAMES] ={
	{ 'R',  '\0', FONTR },
	{ 'B',  '\0', FONTB },
	{ 'I',  '\0', FONTI }
};
static	char	*font_names[NFNAMES] = { "Roman", "Bold", "Italic" };

/*
 * Initialize nroff-specific parameters.
 */
dev_init()
{
	swdmul	= 1;			/* multiplier for width table */
	swddiv	= 20;			/* divisor for width table */
	vls = psz = unit(SMINCH, 6*SDINCH);

	/* Sanity check: width() below assumes swdmul*psz == swddiv. */
	if (swdmul * psz != swddiv)
		panic("botch: swdmul=%d psz=%d swddiv=%d", swdmul, psz, swddiv);
}

/*
 * Compute the scaled width of a character:
 *	width(c) == unit(swdmul*fonwidt[c]*psz, swddiv).
 * For nroff, swdmul*psz == swddiv and fonwidt[i] is constant, so it's trivial.
 */
int
width(c) register int c;
{
	return (c < NWIDTH) ? 12 : 0;
}

/*
 * Given a font number, change to the given font.
 */
dev_font(n) register int n;
{
	addidir(DFONT, curfont = n);
}

/*
 * Given a pointer to a buffer containing stream directives
 * and a pointer to the end of the buffer, print the buffer
 * out.
 */
flushl(buffer, bufend) CODE *buffer; CODE *bufend;
{
	static int hpos, hres, vres, font;
	register CODE *cp;
	register int i, n;
	char *tp;

#if	(DDEBUG & DBGFUNC)
	printd(DBGFUNC, "flushl: hpos=%d, hres=%d, vres=%d, font=%d\n",
		hpos, hres, vres, font);
#endif
	for (cp = buffer; cp < bufend; cp++) {
		i = cp->l_arg.c_iarg;
#if	(DDEBUG & DBGCODE)
		codebug(cp->l_arg.c_code, i);
#endif
#if	0
		fprintf(stderr, "output: %d arg=%d\n", cp->l_arg.c_code, i);
#endif
		switch (cp->l_arg.c_code) {
		case DNULL:
		case DHYPH:
			continue;
		case DHMOV:
		case DPADC:
			hres += i;
			if ((hpos += i) < 0) {
				hres -= hpos;
				hpos = 0;
			}
			continue;
		case DVMOV:
			vres += i;
			continue;
		case DFONT:
			font = i;
			continue;
		case DPSZE:
			continue;
		case DSPAR:
			/* Space down and return. */
			hpos = hres = 0;
			vres += i;
			if (vres >= 0) {
				n = (vres + 9) / 20;
				while (n--)
					putchar('\n');
			} else {
				putchar('\r');
				n = (-vres + 9) / 20;
				while (n--)
					printf(RLF);
			}
			/*
			 * N.B. Before 7/94, this accumulated the difference
			 * in vres, now it zeroes it; i.e., non-line motions
			 * do not accumulate across commands.
			 */
			vres = 0;
			continue;
		case DTRAB:			/* transparent line */
			tp = cp->b_arg.c_bufp;
			while (*tp)
				putchar(*tp++);
			free(cp->b_arg.c_bufp);
			continue;
		default:
			if (vres >= 0) {
				n = (vres + 9) / 20;
				while (n--)
					printf(LF);
			} else {
				n = (-vres + 9 )/20;
				while (n--)
					printf(RLF);
			}
			vres = 0;
			if (hres >= 0) {
				n = (hres+6) / 12;
				hres -= n*12;
				while (n--)
					putchar(' ');
			} else {
				n = (-hres+5)/12;
				hres += n*12;
				while (n--)
					putchar('\b');
			}
			if (cp->l_arg.c_code==DHYPC)
				n = '-';
			else
				n = cp->l_arg.c_code;
			if (n < 0 || n >= NWIDTH)
				panic("bad directive %d", n);
			if ((font != FONTR)
			&&  (isascii(n))
			&&  (isupper(n) || islower(n) || isdigit(n)))
				switch (font) {
				case FONTB:
					printf("%c\b", n);
					break;
				case FONTI:
#if	1
					printf("_\b");
#else
					n |= 0x80;
#endif
					break;
#if	0
				case HELV:
					printf("_\b");
					break;
#endif
				default:
					panic("bad font %d", font);
				}
			putchar(n);
			hres += cp->c_arg.c_move-12;
			hpos += cp->c_arg.c_move;
		}
	}
}

/*
 * Display available fonts.
 */
void
font_display()
{
	register FTB *p;
	register int a, b;

	fprintf(stderr, "Fonts available in this version:\n");
	for (p = fontab; p < &fontab[NFNAMES]; p++) {
		if ((a = p->f_name[0]) == 0)
			break;
		if ((b = p->f_name[1]) == 0)
			b = ' ';
		fprintf(stderr," %c%c %s\n", a, b, font_names[p->f_font]);
	}
	fprintf(stderr, "Fonts may be renamed with the .rf request.\n");
}

/*
 * The following troff functions are nops for nroff.
 */
#if	ZKLUDGE
dev_close(){}
#endif
dev_cs(){}
dev_fz(){}
dev_ps(){}		/* psz initialized in devinit() */
newpsze(){}
load_font(){}


/* end of tty.c */
