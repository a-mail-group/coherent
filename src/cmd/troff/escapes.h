/*
 * escapes.h
 * Nroff/Troff.
 * Escape character definitions.
 */

/*
 * Values kludged by steve 2/21/91.
 * getf() should support the use of any character value between 0 and 255.
 * But its returned value is often stored in an array of char,
 * e.g. for conditionals and macros; the values defined below, which are
 * returned for escape sequences, therefore must fit in a char for now.
 * The values below are in the ranges 0x01 to 0x1F and 0x80 to 0xA0;
 * this avoids the high-bit-set characters in the standard PostScript
 * character set and will have to do until someone has time to fix it
 * the right way (by changing various char array buffers to int arrays).
 * I'm not proud of this one, folks.
 */

/* Pseudoescapes. */
#define ENUL	0		/*	Character maps onto itself	*/
#define ECOD	0x10		/*	Return processed text		*/

/* Currently unimplemented. */
#define EBRA	0x13		/* \b	Bracket building function	*/

/* Recently added, should be in implemented list below. */
#define ECHR	0x11		/* \(	Special character indicator	*/
#define EVLF	0x12		/* \L	Vertical line drawing function	*/
#define EOVS	0x14		/* \o	Overstrike			*/
#define EZWD	0x15		/* \z	Print character with zero width	*/
#define	EHEX	0x16		/* \X	Hexadecimal character		*/

/* Implemented. */
#define EIGN	0x80		/* \\n	Concealed newline (ignored)	*/
#define EUNP	0x81		/* \SP	Unpaddable space		*/
#define ETLI	0x82		/* \!	Transparent line indicator	*/
#define ECOM	0x83		/* \"	Start of comment		*/
#define EARG	0x84		/* \$	Parameter character		*/
#define EHYP	0x85		/* \%	Hyphenation character		*/
#define ENOP	0x86		/* \&	Zero width character		*/
#define EACA	0x87		/* \'	Acute accent			*/
#define ESTR	0x88		/* \*	String macro indicator		*/
#define EMIN	0x89		/* \-	Minus sign			*/
#define EDWS	0x8A		/* \0	Digit width space		*/
#define EM12	0x8B		/* \^	1/12 em half narrow space	*/
#define EGRA	0x8C		/* \`	Grave accent			*/
#define	ELDR	0x8D		/* \a	Uninterpreted leader character	*/
#define EINT	0x8E		/* \c	Interrupt text processing	*/
#define EVNF	0x8F		/* \d	1/2 em vertical motion		*/
#define EESC	0x90		/* \e	Printable escape character	*/
#define EFON	0x91		/* \f	Font change			*/
#define EHMT	0x92		/* \h	Local horizontal motion		*/
#define EMAR	0x93		/* \k	Mark horizontal input place	*/
#define EHLF	0x94		/* \l	Horizontal line drawing fn	*/
#define ENUM	0x95		/* \n	Expand number register		*/
#define ESPR	0x96		/* \p	Break and spread output line	*/
#define EVRM	0x97		/* \r	Reverse 1 em vertically		*/
#define EPSZ	0x98		/* \s	Change pointsize		*/
#define	ETAB	0x99		/* \t	Uninterpreted horizontal tab	*/
#define EVRN	0x9A		/* \u	Reverse 1 en vertically		*/
#define EVMT	0x9B		/* \v	Local vertical motion		*/
#define EWID	0x9C		/* \w	Width function			*/
#define EXLS	0x9D		/* \x	Extra line spacing		*/
#define EBEG	0x9E		/* \{	Begin conditional input		*/
#define EM06	0x9F		/* \|	1/6 em narrow space		*/
#define EEND	0xA0		/* \}	End conditional input		*/

/* end of escapes.h */
