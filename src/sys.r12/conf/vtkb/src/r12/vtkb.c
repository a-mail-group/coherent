/* $Header: /ker/io.386/RCS/vtkb.c,v 1.3 93/08/19 10:39:40 nigel Exp Locker: nigel $ */
/*
 * virtual console, nonloadable keyboard driver
 *
 * $Log:	vtkb.c,v $
 * Revision 1.3  93/08/19  10:39:40  nigel
 * r83 ioctl (), corefile, new headers
 * 
 * Revision 1.2  93/08/19  04:03:29  nigel
 * Nigel's R83
 */

#include <common/_tricks.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <stddef.h>

#include <kernel/trace.h>
#include <sys/coherent.h>
#include <sys/uproc.h>
#include <sys/con.h>
#include <sys/devices.h>
#include <sys/tty.h>
#include <sys/sched.h>
#include <sys/silo.h>
#include <sys/kb.h>
#include <sys/vt.h>
#include <sys/vtkd.h>

/************************* Hardware Defines *****************************/

#define	DEBUG		0

#define SEPARATE_SHIFT_STATUS 0

#define	SPC	0xF4			/* Special encoding */
#define XXX	0xF5			/* Non-character */
#define	KBDATA	0x60			/* Keyboard data */
#define	KBCTRL	0x61			/* Keyboard control */
#define	KBFLAG	0x80			/* Keyboard reset flag */
#define	LEDCMD	0xED			/* status indicator command */
#define	KBACK	0xFA			/* status indicator acknowledge */
#define	EXTENDED0 0xE0			/* extended key seq initiator */
#define	EXTENDED1 0xE1			/* extended key seq initiator */

#define	KEYUP	0x80			/* Key up change */
#define	KEYSC	0x7F			/* Key scan code mask */
#define	LSHIFT	(0x2A - 1)		/* Left shift key */
#define LSHIFTA (0x2B - 1)		/* Alternate left-shift key */
#define	RSHIFT	(0x36 - 1)		/* Right shift key */
#define	CTRLkb	(0x1D - 1)		/* Control key */
/*-- #define	CAPLOCK	(0x1D - 1)--*/		/* Control key */
#define	ALTkb	(0x38 - 1)		/* Alt key or ALT GR */
#define	CAPLOCK	(0x3A - 1)		/* Caps lock key */
/*-- #define	CTRL	(0x3A - 1)--*/		/* Caps lock key */
#define	NUMLOCK	(0x45 - 1)		/* Numeric lock key */
#define	DELETE	(0x53 - 1)		/* Del, as in CTRL-ALT-DEL */
#define BACKSP	(0x0E - 1)		/* Back space */
#define SCRLOCK	(0x46 - 1)		/* Scroll lock */

/* Shift flags */
#define	SRS	0x01			/* Right shift key on */
#define	SLS	0x02			/* Left shift key on */
#define CTS	0x04			/* Ctrl key on */
#define ALS	0x08			/* Alt key on */
#define CPLS	0x10			/* Caps lock on */
#define NMLS	0x20			/* Num lock on */
#define AKPS	0x40			/* Alternate keypad shift */
#define SHFT	0x80			/* Shift key flag */
#define	AGS	0x100			/* Alt Graphics on */

/* Function key information */
#define	NFKEY	50			/* Number of settable functions */
#define	NFCHAR	150			/* Number of characters settable */
#define	NFBUF	(NFKEY * 2 + NFCHAR + 1)	/* Size of buffer */


/************************* Data Defines ******************************/


#define	ESCAPE_CHAR	'\x1B'
#define	ESCAPE_STRING	"\x1B"
#define	HEXFF_STRING	"\xFF"
#define	DELETE_STRING	"\x7F"

#define SS0	0			/* No shift */
#define SS1	(SLS | SRS | CTS)		/* Shift, Ctrl */
#define SES	(SLS | SRS)		/* Shift */
#define LET	(SLS | SRS | CPLS | CTS)	/* Shift, Caps, Ctrl */
#define KEY	(SLS | SRS | NMLS | AKPS)	/* Shift, Num, Alt keypad */


/**************************** X Stuff ************************************/


#define TIMER_CTL    0x43                     /* Timer control */
#define TIMER_CNT    0x42                     /* Timer counter */
#define SPEAKER_CTL  0x61                     /* Speaker control */



/************************** Tables ************************************/

/*
 * Default function key strings (terminated by -1)
 */

static char * deffuncs [] = {

	ESCAPE_STRING "[M" HEXFF_STRING,	/* F1 */
	ESCAPE_STRING "[N" HEXFF_STRING,	/* F2 */
	ESCAPE_STRING "[O" HEXFF_STRING,	/* F3 */
	ESCAPE_STRING "[P" HEXFF_STRING, 	/* F4 */
	ESCAPE_STRING "[Q" HEXFF_STRING,	/* F5 */
	ESCAPE_STRING "[R" HEXFF_STRING,	/* F6 */
	ESCAPE_STRING "[S" HEXFF_STRING,	/* F7 */
	ESCAPE_STRING "[T" HEXFF_STRING,	/* F8 */
	ESCAPE_STRING "[U" HEXFF_STRING,	/* F9 */
	ESCAPE_STRING "[V" HEXFF_STRING,	/* F10 - historical value */
	/* No F11 or F12 on these keyboards */

	/* Shifted function keys */
	ESCAPE_STRING "[Y" HEXFF_STRING,	/* sF1 */
	ESCAPE_STRING "[Z" HEXFF_STRING,	/* sF2 */
	ESCAPE_STRING "[a" HEXFF_STRING,	/* sF3 */
	ESCAPE_STRING "[b" HEXFF_STRING, 	/* sF4 */
	ESCAPE_STRING "[c" HEXFF_STRING,	/* sF5 */
	ESCAPE_STRING "[d" HEXFF_STRING,	/* sF6 */
	ESCAPE_STRING "[e" HEXFF_STRING,	/* sF7 */
	ESCAPE_STRING "[f" HEXFF_STRING,	/* sF8 */
	ESCAPE_STRING "[g" HEXFF_STRING,	/* sF9 */
	ESCAPE_STRING "[h" HEXFF_STRING,	/* sF10 */

	/* Ctrl-ed function keys */
	ESCAPE_STRING "[k" HEXFF_STRING,	/* cF1 */
	ESCAPE_STRING "[l" HEXFF_STRING,	/* cF2 */
	ESCAPE_STRING "[m" HEXFF_STRING,	/* cF3 */
	ESCAPE_STRING "[n" HEXFF_STRING,	/* cF4 */
	ESCAPE_STRING "[o" HEXFF_STRING,	/* cF5 */
	ESCAPE_STRING "[p" HEXFF_STRING,	/* cF6 */
	ESCAPE_STRING "[q" HEXFF_STRING,	/* cF7 */
	ESCAPE_STRING "[r" HEXFF_STRING,	/* cF8 */
	ESCAPE_STRING "[s" HEXFF_STRING,	/* cF9 */
	ESCAPE_STRING "[t" HEXFF_STRING,	/* cF10 */

	/* Ctrl-shifted function keys */
	ESCAPE_STRING "[w" HEXFF_STRING,	/* csF1 */
	ESCAPE_STRING "[x" HEXFF_STRING,	/* csF2 */
	ESCAPE_STRING "[y" HEXFF_STRING,	/* csF3 */
	ESCAPE_STRING "[z" HEXFF_STRING,	/* csF4 */
	ESCAPE_STRING "[@" HEXFF_STRING,	/* csF5 */
	ESCAPE_STRING "[[" HEXFF_STRING,	/* csF6 */
	ESCAPE_STRING "[\\" HEXFF_STRING,	/* csF7 */
	ESCAPE_STRING "[]" HEXFF_STRING,	/* csF8 */
	ESCAPE_STRING "[^" HEXFF_STRING,	/* csF9 */
	ESCAPE_STRING "[_" HEXFF_STRING,	/* csF10 */

	/* Alt keys -- use original 83-key setting since these are
	 * not defined for virtual terms; actually, many will never
	 * be used since intercepted by virtual term code, but 
	 * should be there in case someone defines like 1 virtual term
	 * so the keys should return something useful.
	 */
	ESCAPE_STRING "[1y" HEXFF_STRING,	/* aF1 */
	ESCAPE_STRING "[2y" HEXFF_STRING,	/* aF2 */
	ESCAPE_STRING "[3y" HEXFF_STRING,	/* aF3 */
	ESCAPE_STRING "[4y" HEXFF_STRING,	/* aF4 */
	ESCAPE_STRING "[5y" HEXFF_STRING,	/* aF5 */
	ESCAPE_STRING "[6y" HEXFF_STRING,	/* aF6 */
	ESCAPE_STRING "[7y" HEXFF_STRING,	/* aF7 */
	ESCAPE_STRING "[8y" HEXFF_STRING,	/* aF8 */
	ESCAPE_STRING "[9y" HEXFF_STRING,	/* aF9 */
	ESCAPE_STRING "[0y" HEXFF_STRING	/* aF10 */
};

/* The keypad is translated by the following table,
 * the first entry is the normal sequence, the second the shifted,
 * and the third the alternate keypad sequence.
 */

static char * keypad [][3] = {
	{ ESCAPE_STRING "[H",  "7", ESCAPE_STRING "?w" },	/* 71 */
	{ ESCAPE_STRING "[A",  "8", ESCAPE_STRING "?x" },	/* 72 */
	{ ESCAPE_STRING "[I",  "9", ESCAPE_STRING "?y" },	/* 73 */
	{ ESCAPE_STRING "[D",  "4", ESCAPE_STRING "?t" },	/* 75 */
	{ ESCAPE_STRING "7",   "5", ESCAPE_STRING "?u" },	/* 76 */
	{ ESCAPE_STRING "[C",  "6", ESCAPE_STRING "?v" },	/* 77 */
	{ ESCAPE_STRING "[F",  "1", ESCAPE_STRING "?q" },	/* 79 */
	{ ESCAPE_STRING "[B",  "2", ESCAPE_STRING "?r" },	/* 80 */
	{ ESCAPE_STRING "[G",  "3", ESCAPE_STRING "?s" },	/* 81 */
	{ ESCAPE_STRING "[L",  "0", ESCAPE_STRING "?p" },	/* 82 */
	{ DELETE_STRING ,      ".", ESCAPE_STRING "?n" }	/* 83 */
};


/*
 * French Tables for converting key code to ASCII.
 * lmaptab specifies unshifted conversion,
 * umaptab specifies shifted conversion,
 * smaptab specifies the shift states which are active.
 * An entry of XXX says the key is dead.
 * An entry of SPC requires further processing.
 *
 * Key codes:
 *	ESC .. <- == 1 .. 14
 *	-> .. \n == 15 .. 28
 *	CTRL .. ` == 29 .. 41
 *	^ Shift .. PrtSc == 42 .. 55
 * 	ALT .. CapsLock == 56 .. 58
 *	F1 .. F10 == 59 .. 68
 *	NumLock .. Del == 69 .. 83
 *	ISO, F11, F12 == 86 .. 88
 */

static unsigned char fr_agmaptab [] ={					/* Alt Gr */
	       XXX,  XXX,  '~',  '#',  '{',  '[',  '|',		/* 1 - 7 */
	 '`', '\\',  '^',  '@',  ']',  '}',  XXX,  XXX,		/* 8 - 15 */
	 XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,		/* 16 - 23 */
	 XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,		/* 24 - 31 */
	 XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,		/* 32 - 39 */
	 XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,		/* 40 - 47 */
	 XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,		/* 48 - 55 */
	 XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,		/* 56 - 63 */
	 XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,		/* 64 - 71 */
	 XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,		/* 72 - 79 */
	 XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,		/* 80 - 87 */
	 XXX							/* 88 */
};

static unsigned char fr_lmaptab [] ={
       ESCAPE_CHAR,  '&','\202', '"', '\'',  '(',  '-',		/* 1 - 7 */
	'\212','_','\207','\205',')',  '=', '\b', '\t',		/* 8 - 15 */
	 'a',  'z',  'e',  'r',  't',  'y',  'u',  'i',		/* 16 - 23 */
	 'o',  'p','\260', '$', '\r',  XXX,  'q',  's',		/* 24 - 31 */
	 'd',  'f',  'g',  'h',  'j',  'k',  'l',  'm',		/* 32 - 39 */
	 '\227','\375', XXX,'*',  'w',  'x',  'c',  'v',		/* 40 - 47 */
	 'b',  'n',  ',',  ';',  ':',  SPC,  XXX,  SPC,		/* 48 - 55 */
	 XXX,  ' ',  XXX,  SPC,  SPC,  SPC,  SPC,  SPC,		/* 56 - 63 */
	 SPC,  SPC,  SPC,  SPC,  SPC,  SPC,  SPC,  SPC,		/* 64 - 71 */
	 SPC,  SPC,  '-',  SPC,  SPC,  SPC,  '+',  SPC,		/* 72 - 79 */
	 SPC,  SPC,  SPC,  SPC,  XXX,  XXX,  '<',  XXX,		/* 80 - 87 */
	 XXX							/* 88 */
};

static unsigned char fr_umaptab [] ={
       ESCAPE_CHAR,  '1',  '2',  '3',  '4',  '5',  '6',		/* 1 - 7 */
	 '7',  '8',  '9',  '0','\370', '+', '\b',  SPC,		/* 8 - 15 */
	 'A',  'Z',  'E',  'R',  'T',  'Y',  'U',  'I',		/* 16 - 23 */
	 'O',  'P','\261','\234','\r', XXX,  'Q',  'S',		/* 24 - 31 */
	 'D',  'F',  'G',  'H',  'J',  'K',  'L',  'M',		/* 32 - 39 */
	 '%','\300', XXX,'\346',  'W',  'X',  'C',  'V',		/* 40 - 47 */
	 'B',  'N',  '?',  '.',  '/',  SPC,  XXX,  SPC,		/* 48 - 55 */
	 XXX,  ' ',  XXX,  SPC,  SPC,  SPC,  SPC,  SPC,		/* 56 - 63 */
	 SPC,  SPC,  SPC,  SPC,  SPC,  SPC,  SPC,  SPC,		/* 64 - 71 */
	 SPC,  SPC,  '-',  SPC,  SPC,  SPC,  '+',  SPC,		/* 72 - 79 */
	 SPC,  SPC,  SPC,  SPC,  XXX,  XXX,  '>',  XXX,		/* 80 - 87 */
	 XXX							/* 88 */
};

static unsigned char fr_de_smaptab [] ={
	       SS0,  SES,  SS1,  SES,  SES,  SES,  SS1,		/* 1 - 7 */
	 SES,  SES,  SES,  SES,  SS1,  SES,  CTS,  SES,		/* 8 - 15 */
	 LET,  LET,  LET,  LET,  LET,  LET,  LET,  LET,		/* 16 - 23 */
	 LET,  LET,  SS1,  SS1,  CTS, SHFT,  LET,  LET,		/* 24 - 31 */
	 LET,  LET,  LET,  LET,  LET,  LET,  LET,  SES,		/* 32 - 39 */
	 SES,  SS1, SHFT,  SS1,  LET,  LET,  LET,  LET,		/* 40 - 47 */
	 LET,  LET,  LET,  SES,  SES,  SES, SHFT,  SES,		/* 48 - 55 */
	SHFT,  SS1, SHFT,  SS1,  SS1,  SS1,  SS1,  SS1,		/* 56 - 63 */
	 SS1,  SS1,  SS1,  SS1,  SS1, SHFT,  KEY,  KEY,		/* 64 - 71 */
	 KEY,  KEY,  SS0,  KEY,  KEY,  KEY,  SS0,  KEY,		/* 72 - 79 */
	 KEY,  KEY,  KEY,  KEY,  SS0,  SS0,  SES,  SS0,		/* 80 - 87 */
	 SS0
};

/*
 * U.S. Tables for converting key code to ASCII.
 * lmaptab specifies unshifted conversion,
 * umaptab specifies shifted conversion,
 * smaptab specifies the shift states which are active.
 * An entry of XXX says the key is dead.
 * An entry of SPC requires further processing.
 *
 * Key codes:
 *	ESC .. <- == 1 .. 14
 *	-> .. \n == 15 .. 28
 *	CTRL .. ` == 29 .. 41
 *	^ Shift .. PrtSc == 42 .. 55
 * 	ALT .. CapsLock == 56 .. 58
 *	F1 .. F10 == 59 .. 68
 *	NumLock .. Del == 69 .. 83
 */

static unsigned char us_lmaptab [] ={
       ESCAPE_CHAR,  '1',  '2',  '3',  '4',  '5',  '6',		/* 1 - 7 */
	 '7',  '8',  '9',  '0',  '-',  '=', '\b', '\t',		/* 8 - 15 */
	 'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',		/* 16 - 23 */
	 'o',  'p',  '[',  ']', '\r',  XXX,  'a',  's',		/* 24 - 31 */
	 'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',		/* 32 - 39 */
	 '\'', '`',  XXX,  '\\', 'z',  'x',  'c',  'v',		/* 40 - 47 */
	 'b',  'n',  'm',  ',',  '.',  '/',  XXX,  '*',		/* 48 - 55 */
	 XXX,  ' ',  XXX,  SPC,  SPC,  SPC,  SPC,  SPC,		/* 56 - 63 */
	 SPC,  SPC,  SPC,  SPC,  SPC,  SPC,  SPC,  SPC,		/* 64 - 71 */
	 SPC,  SPC,  '-',  SPC,  SPC,  SPC,  '+',  SPC,		/* 72 - 79 */
	 SPC,  SPC,  SPC,  SPC					/* 80 - 83 */
};

static unsigned char us_umaptab [] ={
       ESCAPE_CHAR,  '!',  '@',  '#',  '$',  '%',  '^',		/* 1 - 7 */
	 '&',  '*',  '(',  ')',  '_',  '+', '\b',  SPC,		/* 8 - 15 */
	 'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',		/* 16 - 23 */
	 'O',  'P',  '{',  '}', '\r',  XXX,  'A',  'S',		/* 24 - 31 */
	 'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',		/* 32 - 39 */
	 '"',  '~',  XXX,  '|',  'Z',  'X',  'C',  'V',		/* 40 - 47 */
	 'B',  'N',  'M',  '<',  '>',  '?',  XXX,  '*',		/* 48 - 55 */
	 XXX,  ' ',  XXX,  SPC,  SPC,  SPC,  SPC,  SPC,		/* 56 - 63 */
	 SPC,  SPC,  SPC,  SPC,  SPC,  SPC,  SPC,  SPC,		/* 64 - 71 */
	 SPC,  SPC,  '-',  SPC,  SPC,  SPC,  '+',  SPC,		/* 72 - 79 */
	 SPC,  SPC,  SPC,  SPC					/* 80 - 83 */
};

static unsigned char us_smaptab [] ={
	       SS0,  SES,  SS1,  SES,  SES,  SES,  SS1,		/* 1 - 7 */
	 SES,  SES,  SES,  SES,  SS1,  SES,  CTS,  SES,		/* 8 - 15 */
	 LET,  LET,  LET,  LET,  LET,  LET,  LET,  LET,		/* 16 - 23 */
	 LET,  LET,  SS1,  SS1,  CTS, SHFT,  LET,  LET,		/* 24 - 31 */
	 LET,  LET,  LET,  LET,  LET,  LET,  LET,  SES,		/* 32 - 39 */
	 SES,  SS1, SHFT,  SS1,  LET,  LET,  LET,  LET,		/* 40 - 47 */
	 LET,  LET,  LET,  SES,  SES,  SES, SHFT,  SES,		/* 48 - 55 */
	SHFT,  SS1, SHFT,  SS1,  SS1,  SS1,  SS1,  SS1,		/* 56 - 63 */
	 SS1,  SS1,  SS1,  SS1,  SS1, SHFT,  KEY,  KEY,		/* 64 - 71 */
	 KEY,  KEY,  SS0,  KEY,  KEY,  KEY,  SS0,  KEY,		/* 72 - 79 */
	 KEY,  KEY,  KEY,  KEY					/* 80 - 83 */
};

/*
 * German Tables for converting key code to ASCII.
 * lmaptab specifies unshifted conversion,
 * umaptab specifies shifted conversion,
 * smaptab specifies the shift states which are active.
 * An entry of XXX says the key is dead.
 * An entry of SPC requires further processing.
 *
 * Key codes:
 *	ESC .. <- == 1 .. 14
 *	-> .. \n == 15 .. 28
 *	CTRL .. ` == 29 .. 41
 *	^ Shift .. PrtSc == 42 .. 55
 * 	ALT .. CapsLock == 56 .. 58
 *	F1 .. F10 == 59 .. 68
 *	NumLock .. Del == 69 .. 83
 *	ISO, F11, F12 == 86 .. 88
 */

unsigned char de_agmaptab [] ={					/* Alt Gr */
	       XXX,  XXX,'\375','\374', XXX,  XXX,  XXX,		/* 1 - 7 */
	 '{',  '[',  ']',  '}', '\\',  XXX,  XXX,  XXX,		/* 8 - 15 */
	 '@',  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,		/* 16 - 23 */
	 XXX,  XXX,  XXX,  '~',  XXX,  XXX,  XXX,  XXX,		/* 24 - 31 */
	 XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,		/* 32 - 39 */
	 XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,		/* 40 - 47 */
	 XXX,  XXX,'\346', XXX,  XXX,  XXX,  XXX,  XXX,		/* 48 - 55 */
	 XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,		/* 56 - 63 */
	 XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,		/* 64 - 71 */
	 XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  XXX,		/* 72 - 79 */
	 XXX,  XXX,  XXX,  XXX,  XXX,  XXX,  '|',  XXX,		/* 80 - 87 */
	 XXX							/* 88 */
};

static unsigned char de_lmaptab [] ={
       ESCAPE_CHAR,  '1',  '2',  '3',  '4',  '5',  '6',		/* 1 - 7 */
	 '7',  '8',  '9',  '0','\341','\'', '\b', '\t',		/* 8 - 15 */
	 'q',  'w',  'e',  'r',  't',  'z',  'u',  'i',		/* 16 - 23 */
	 'o',  'p','\201', '+', '\r',  XXX,  'a',  's',		/* 24 - 31 */
	 'd',  'f',  'g',  'h',  'j',  'k',  'l','\224',	/* 32 - 39 */
	'\204','^',  XXX,  '#',  'y', 'x',  'c',  'v',		/* 40 - 47 */
	 'b',  'n',  'm',  ',',  '.',  SPC,  XXX,  SPC,		/* 48 - 55 */
	 XXX,  ' ',  XXX,  SPC,  SPC,  SPC,  SPC,  SPC,		/* 56 - 63 */
	 SPC,  SPC,  SPC,  SPC,  SPC,  SPC,  SPC,  SPC,		/* 64 - 71 */
	 SPC,  SPC,  '-',  SPC,  SPC,  SPC,  '+',  SPC,		/* 72 - 79 */
	 SPC,  SPC,  SPC,  SPC,  XXX,  XXX,  '<',  XXX,		/* 80 - 87 */
	 XXX							/* 88 */
};

static unsigned char de_umaptab [] ={
       ESCAPE_CHAR,  '!',  '"','\025', '$',  '%',  '&',		/* 1 - 7 */
	 '/',  '(',  ')',  '=',  '?',  '`', '\b',  SPC,		/* 8 - 15 */
	 'Q',  'W',  'E',  'R',  'T',  'Z',  'U',  'I',		/* 16 - 23 */
	 'O',  'P','\232',  '*', '\r',  XXX,  'A',  'S',	/* 24 - 31 */
	 'D',  'F',  'G',  'H',  'J',  'K',  'L','\231',	/* 32 - 39 */
	'\216','\370', XXX,'\'',  'Y',  'X',  'C',  'V',		/* 40 - 47 */
	 'B',  'N',  'M',  ';',  ':',  SPC,  XXX,  SPC,		/* 48 - 55 */
	 XXX,  ' ',  XXX,  SPC,  SPC,  SPC,  SPC,  SPC,		/* 56 - 63 */
	 SPC,  SPC,  SPC,  SPC,  SPC,  SPC,  SPC,  SPC,		/* 64 - 71 */
	 SPC,  SPC,  '-',  SPC,  SPC,  SPC,  '+',  SPC,		/* 72 - 79 */
	 SPC,  SPC,  SPC,  SPC,  XXX,  XXX,  '>',  XXX,		/* 80 - 87 */
	 XXX							/* 88 */
};




/************************** Functions. ****************************/

int	vtmmstart ();
int	vtmmwrite ();
void	vtmmwatch ();


/************************** Virtual Term Stuff ********************/

/* constants for vtdata [] */
#define VT_VGAPORT	0x3D4
#define VT_MONOPORT	0x3B4

#define VT_MONOBASE	(SEL_VIDEOa)
#define VT_VGABASE	(SEL_VIDEOb)

/*
	Patchable table entries.
	Indirect in order to produce a label which can be addressed.
*/

/* Configurable variables - see ker / conf / console / Space.c */
extern int 	vga_count;
extern int 	mono_count;
extern int 	kb_lang;

HWentry	VTVGA =		{ 4, 0, VT_VGAPORT, { 0, VT_VGABASE }, { 25, 80 } };
HWentry	VTMONO =	{ 4, 0, VT_MONOPORT, { 0, VT_MONOBASE }, { 25, 80 } };

HWentry	* vtHWtable [] = {
	& VTVGA,	/* VGA followed by MONO is compatible to DOS */
	& VTMONO
};
#define	vtHWend		(vtHWtable + __ARRAY_LENGTH (vtHWtable))

extern	int	vtmminit ();

static	VTDATA	const_vtdata	= {
	vtmminit,		/* func */
	0,			/* port */
	0,			/* seg */
	0,			/* off */
	0, 0,			/* rowl, col */
	0,			/* pos */
	0x07,			/* attr */
	'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', 	/* args */
	0,			/* nargs */
	0, 24, 25,		/* brow, erow, lrow */
	0, 0,			/* srow, scol */
	0, 24,			/* ibrow, ierow */
	0,			/* invis */
	0,			/* slow */
	1,			/* wrap */
	0x07,			/* oattr */
	0,			/* font */
	0,			/* esc */
	0			/* visible */
};

/* later this should be dynamic */
VTDATA	* vtconsole, ** vtdata;

int	vtcount, vtmax;
extern	int	vtactive;
int	vt_verbose = { 0 };
int	vt_opened = { 0 };

/* Terminal structure. */
TTY	** vttty;

/**************** State variables / Patchables ****************************/

int		islock;			/* Keyboard locked flag */
int		isbusy = 0;		/* Raw input conversion busy */
int		isvtbusy = 0;		/* VT switching busy (deferred) */
int		isvtnotdone = 0;	/* VT deferred not done */
int 		isturbo = 0;		/* Flag indicating turbo machine. */
static unsigned	shift;			/* Overall shift state */
static	char	scrollkb;		/* Scroll lock state */
static  char	lshiftkb = LSHIFT;	/* Left shift alternate state */
static	char	isfbuf [NFBUF];		/* Function key values */
static	char	* isfval [NFKEY];		/* Function key string pointers */
static	int	ledcmd;			/* LED update command count */
static	int	extended;		/* extended key scan count */
static	char	extmode;		/* use extended mode for this key */
static	char	ext0seen;		/* 0xE0 prefix seen */
static	char	fk_loaded;		/* true == funcion keys resident */
static	int	xlate = 1;		/* scan code translation flag */
static 	TIM	tp;			/* X thingamabob */
static	int	X11led;			/* Other X thingamabob */
static	silo_t	in_silo;		/* Used by character silo */
static	silo_t	vt_silo;		/* Queue of vt switches */

static int	kb_in_byte;

static unsigned char * lmaptab, * umaptab, * smaptab, * agmaptab;


/************************* Code *****************************************/

void
vtdeactivate (vp_new, vp_old)
VTDATA	* vp_new, * vp_old;
{
	int i;
	VTDATA	* vpi;

	/*
	 * if changing to another screen on same video board
	 *	for all screens on same board as new screen
	 *		deactivate, but don't update
	 * else - changing to a screen on different board
	 *	for all screens NOT on same board as new screen
	 *		deactivate, but don't update
	 */

	if (vp_old->vmm_port != vp_new->vmm_port) {
		T_CON (8, printf ("deactivate %x->%x ",
		  vp_old->vmm_port, vp_new->vmm_port));
		for (i = 0; i < vtcount; ++ i) {
			vpi = vtdata [i];
			if (vpi->vmm_port != vp_new->vmm_port &&
			    vpi->vmm_invis == 0) {
				/* update, but don't deactivate */
				vpi->vmm_invis = ~ 0; 
				vtupdscreen (i);
			}
		}
	}
}


void
vtactivate (vp)
VTDATA * vp;
{
	VTDATA	* vpi;
	int	i;

	/*
	 * If new active screen isn't visible, find currently visible
	 * screen and save to nonvideo memory, then restore new active
	 * screen from nonvideo.
	 */
	if (vp->vmm_visible == VNKB_FALSE) {
		for (i = 0 ; i < vtcount ; ++ i) {
			vpi = vtdata [i];

			if (vpi->vmm_port == vp->vmm_port
			  && vpi->vmm_visible == VNKB_TRUE) {
				ffcopy (vpi->vmm_voff, vpi->vmm_vseg,
					vpi->vmm_moff, vpi->vmm_mseg, TEXTBLOCK);
				break;
			}
		}
		ffcopy (vp->vmm_moff, vp->vmm_mseg,
			vp->vmm_voff, vp->vmm_vseg, TEXTBLOCK);
	}

	for (i = 0; i < vtcount; ++ i) {
		vpi = vtdata [i];
		if (vpi->vmm_port == vp->vmm_port) {
			vpi->vmm_invis = -1;
			vpi->vmm_visible = VNKB_FALSE;
			vpi->vmm_seg = vpi->vmm_mseg;
			vpi->vmm_off = vpi->vmm_moff;
			if (vpi->vmm_seg == 0)
				printf ("[2]vpi->vmm_seg = 0\n");
			PRINTV ("vt.back seg %x off %x\n",
				vpi->vmm_seg, vpi->vmm_off);
		}		
	}
	/*
	 * Set new active terminal
	 */
	vp->vmm_invis = 0;	
	vp->vmm_visible = VNKB_TRUE;
	vp->vmm_seg = vp->vmm_vseg;
	vp->vmm_off = vp->vmm_voff;
	if (vp->vmm_seg == 0)
		printf ("vp->vmm_seg = 0\n");
}


/*
 * update the keyboard status LEDS
 */

void
updleds ()
{
	int	s;

	s = sphi ();
	outb (KBDATA, LEDCMD);
       	ledcmd ++;
	spl (s);
}


/*
 * update the terminal to match vtactive
 */

void
updterminal (index)
int index;
{
	vtupdscreen (index);
#if SEPARATE_SHIFT_STATUS
	updleds ();
#endif
}


/*
 *
 * void
 * isvtswitch ()	-- deferred virtual terminal switch
 *
 *	Action: - save current shift key status
 *		- determine new active virtual terminal
 *		- deactivate shift key status of the current virtual terminal
 *		- deactivate current virtual terminal
 *		- activate shift key status of the new virtual terminal with 
 *		  the previously saved shift key status
 *		- activate new virtual terminal 
 *
 *	Notes:	isvtswitch () was scheduled as a deferred process by 
 *	process_key () which is a function called by vtkbintr ().
 */

void
isvtswitch (dummy)
int dummy;
{
	int		key_val;
	int		new_index, i;
	unsigned	lockshift, nolockshift; 
	VTDATA		* vp = vtdata [vtactive];
	VTDATA		* vp_old, * vp_new;
	static int	vtprevious;
	int		s;

	/*
 	 * When this is zero, queue_vt_switch () will crank off another
	 * deferred function
	 */

	isvtbusy = 0;

	/*
	 * Process the whole queue
	 */

	while (vt_silo.si_ix != vt_silo.si_ox) {

#if SEPARATE_SHIFT_STATUS
		lockshift = shift & (CPLS | NMLS);
		nolockshift = shift & ~(CPLS | NMLS);
#else
		lockshift = 0;
		nolockshift = shift;
#endif

		/*
		 * We must lock on the manipulation of these variables
		 * or we lose some vt switches.  Trust me -- it happens.
		 */

		s = sphi ();
		key_val = vt_silo.si_buf [vt_silo.si_ox];

		if (vt_silo.si_ox >= sizeof (vt_silo.si_buf) - 1)
			vt_silo.si_ox = 0;
		else
			vt_silo.si_ox ++;
		spl (s);

		PRINTV ("F%d: %d", key_val, vtactive);


		switch (key_val) {

		case VTKEY_HOME:
			new_index = 0;
			break;

		case VTKEY_NEXT:
			new_index = vtactive;
			for ( i = 0; i < vtcount; ++ i ) {
				new_index = ++ new_index % vtcount;
				if (vttty [new_index]->t_open)
					break;
			}
			break;

		case VTKEY_PREV:
			new_index = vtactive;
			for ( i = 0; i < vtcount; ++ i ) {
				new_index = (--new_index + vtcount) % vtcount;
				if ( vttty [new_index]->t_open )
					break;
			}
			break;

		case VTKEY_TOGL:
			new_index = vtprevious;
			break;

		default:
			new_index = vtindex (vtkey_to_dev (key_val));
			if ( new_index < 0) {
				putchar ('\a');
				return;
			}
		}

		T_CON (8, printf ("%d->%d ", vtactive, new_index));

		if (new_index == vtactive)
			return;

		/* Save which locking shift states are in effect. */

		s = sphi ();
		vp_old = vtdata [vtactive];
		vp_new = vtdata [new_index];

		vp_old->vnkb_shift = lockshift;
		vtdeactivate (vp_new, vp_old);	/* deactivate old virtual terminal */

		/* Restore shift lock state, append current momentary shift state. */
		shift = vp_new->vnkb_shift | nolockshift;
		vtactivate (vp_new);		/* activate new virtual terminal */
		updterminal (new_index);
		vtprevious = vtactive;
		vtactive = new_index;		/* update vtactive */
		spl (s);
	}
	isvtnotdone = 0;
}


/*
 * This builds the queue of vt switches so they may be done as
 * a deferred function.  This had to be done since re-enabling a
 * virtual term was slow enough that if vt's were switched rapidly,
 * the deferred queue overflowed and things locked up.
 *
 * There is a small chance of a race condition about the variabl
 * isvtbusy, but this has been minimized through the use of a lock
 * variable in isvtswitch.
 */

static void
queue_vt_switch (vt_key)
char vt_key;
{
	vt_silo.si_buf [vt_silo.si_ix] = vt_key;

	if (++ vt_silo.si_ix >= sizeof (vt_silo.si_buf))
		vt_silo.si_ix = 0;

	if (! isvtbusy) {
		isvtbusy = 1;
		defer (isvtswitch, 0);
	}
}


void
vtdatainit (vp)
VTDATA	* vp;
{
	/*
	 * vtdata init - vmm part
	 */
	vp->vmm_invis = -1;			/* cursor invisible */

	vp->vt_buffer = kalloc (TEXTBLOCK);
	vp->vmm_seg = vp->vmm_mseg = vtds_sel ();
	vp->vmm_off = vp->vmm_moff = vp->vt_buffer;
	PRINTV ("vt@%x init index %d,%d), seg %x, off %x\n",
		vp, vp->vt_ind, vp->vmm_mseg, vp->vmm_moff);

	/*
	 * vtdata init - vnkb part
	 */

	/* Make the first memory block active, if present */ 
	vp->vnkb_lastc = 0;
	vp->vnkb_fnkeys = 0;	
	vp->vnkb_funkeyp = 0;	
	vp->vnkb_fk_loaded = 0;			/* no Fn keys yet */
}


/*
 * Load entry point.
 *  Do reset the keyboard because it gets terribly munged
 *  if you type during the boot.
 */

void
isload ()
{
	short 		i;	/* was: int i */
 	HWentry 	** hw;
 	VTDATA 	* vp;

	/* Use stune et al to adjust vga_count and mono_count. */
	VTVGA.count = vga_count;
	VTMONO.count = mono_count;

	/*
	 * Set up keyboard tables
	 */

	switch (kb_lang) {
		case kb_lang_fr:
			agmaptab = fr_agmaptab;
			lmaptab = fr_lmaptab;
			umaptab = fr_umaptab;
			smaptab = fr_de_smaptab;
			break;
		case kb_lang_de:
			agmaptab = de_agmaptab;
			lmaptab = de_lmaptab;
			umaptab = de_umaptab;
			smaptab = fr_de_smaptab;
			break;
		default:
			agmaptab = 0;
			lmaptab = us_lmaptab;
			umaptab = us_umaptab;
			smaptab = us_smaptab;
			break;
	} /* switch */

	/*
	 * Reset keyboard if NOT an XT turbo.
	 */

	if (! isturbo) {
		outb (KBCTRL, 0x0C);		/* Clock low */
		for (i = 10582; --i >= 0; );	/* For 20ms */
		outb (KBCTRL, 0xCC);		/* Clock high */
		for (i = 0; --i != 0; )
			;
		i = inb (KBDATA);
		outb (KBCTRL, 0xCC);			/* Clear keyboard */
		outb (KBCTRL, 0x4D); 			/* Enable keyboard */
	}

	PRINTV ("vtload:\n");
	fk_loaded = 0;

	/* figure out what our current max is */
	for (vtmax = 0, hw = vtHWtable ; hw != vtHWend ; hw ++) {
		vtmax += (* hw)->count;
		(* hw)->found = 0;	/* assume non-exist */
	}
	PRINTV ("vtload: %d screens possible\n", vtmax);

	vtdata = (VTDATA **) kalloc (vtmax * sizeof (* vtdata));
	if (vtdata == NULL) {
		printf ("vtload: unable to obtain vtdata [%d]\n", vtmax);
		return;
	}

	PRINTV ( "vtload: obtained vtdata [%d] @%x\n", vtmax, vtdata );

	vttty = (TTY **) kalloc (vtmax * sizeof (* vttty));
	if (vttty == NULL) {
		printf ("vtload: unable to obtain vttty [%d]\n", vtmax);
		return;
	}

	PRINTV ( "vtload: obtained vttty [%d] @%x\n", vtmax, vttty );

	/* determine which video adaptors are present */

	for (vtcount = 0, hw = vtHWtable ; hw != vtHWend ; hw ++) {
		/* remember our logical start */
		(* hw)->start = vtcount;
		PRINTV (", start %d\n", vtcount);

		/* allocate the necessary memory */
		for (i = 0; i < (* hw)->count; ++ i) {
			vp = vtdata [vtcount] = kalloc (sizeof (VTDATA));
			PRINTV ( "     vtdata [%d] = @%x\n", vtcount, vp );
			if (vp == NULL || ! VTttyinit (vtcount)) {
				printf ("not enough memory for VTDATA\n" );
				return;
			}

			/* fill in appropriately */
			* vp = const_vtdata;
			vp->vmm_port = (* hw)->port;
			vp->vmm_vseg = (* hw)->vidmemory.seg;
			vp->vmm_voff = (* hw)->vidmemory.off;

			vp->vt_ind = vtcount;
			vtdatainit (vp);
			if (i == 0) {
				vp->vmm_visible = VNKB_TRUE;
				vp->vmm_seg = vp->vmm_vseg;
				vp->vmm_off = vp->vmm_voff;
				vtupdscreen (vtcount);
			}
			(* hw)->found ++;
			vtcount ++;
		}
	}

	/*
	 * initialize vtconsole
	 */

	vtconsole = vtdata [vtactive = 0];
	vtconsole->vmm_invis = 0;		/* vtconsole cursor visible */

	/*
	 * Enable vtmmwatch () invocation every second.
	 */

	drvl [VT_MAJOR].d_time = 1;

	/*
	 * Initialize video display.
	 */

	for (i = 0 ; i < vtcount ; ++ i)
		vtmmstart (vttty [i]);
}


/*
 * Unload entry point.
 */

void
isuload ()
{
	/* Restore pointers to original state. */
	vtconsole = vtdata [0];
	vtconsole->vmm_invis = 0;
	vtconsole->vmm_visible = VNKB_TRUE;

	if (vt_opened)
		printf ("VTclose with %d open screens\n", vt_opened);

}


/* Init function keys */

void
initkeys ()
{
	int i;
	char * cp1, * cp2;

	for (i = 0; i < NFKEY ; i ++)
		isfval [i] = 0; 	/* clear function key buffer */
	cp2 = isfbuf;			/* pointer to key buffer */

	for (i = 0 ; i < NFKEY ; i ++) {
		isfval [i] = cp2;	/* save pointer to key string */
		cp1 = deffuncs [i];	/* get init string pointer */
		while ((* cp2 ++ = * cp1 ++) != -1) 	/* copy key data */
			if (cp2 >= isfbuf + NFBUF - 3)	/* overflow? */
				return;
	}
}


/*
 * Open routine.
 */

void
isopen (dev, mode)
dev_t dev;
unsigned int mode;
{
	int s;
	TTY * tp;
	int	index = vtindex (dev);

	PRINTV ("isopen: %x\n", dev);
	if (index < 0 || index >= vtcount) {
		set_user_error (ENXIO);
		return;
	}

	tp = vttty [index];
	if ((tp->t_flags & T_EXCL) != 0 && ! super ()) {
		set_user_error (ENODEV);
		return;
	}
	ttsetgrp (tp, dev, mode);

	s = sphi ();
	if (tp->t_open ++ == 0) {
	  	initkeys ();		/* init function keys */
	   	tp->t_flags = T_CARR;  /* indicate "carrier" */
	   	ttopen (tp);
	}
	spl (s);
}


/*
 * Close a tty.
 */

void
isclose (dev)
{
	TTY * tp = vttty [vtindex (dev)];

	if (-- tp->t_open == 0)
		ttclose (tp);

}


/*
 * Read routine.
 */

void
isread (dev, iop)
dev_t dev;
IO * iop;
{
	TTY * tp = vttty [vtindex (dev)];

	ttread (tp, iop, 0);
	if (tp->t_oq.cq_cc)
		vtmmtime (tp);
}


void
kbstate (action)
int action;
{
	if (action == 1) {
		timeout (& tp, 20, kbstate, 2);
		outb (KBCTRL, 0xCC);		/* Clock high */
	}

	if (action == 2) {
		int i = inb (KBDATA);
		outb (KBCTRL, 0xCC);		/* Clear keyboard */
		outb (KBCTRL, 0x4D);		/* Enable keyboard */
	}
}


/*
 * Set and receive the function keys.
 */

void
isfunction (c, v)
int c;
char * v;
{
	char * cp;
	int i;

	if (c == TIOCGETF) {
		for (cp = isfbuf; cp < & isfbuf [NFBUF]; cp ++)
		    putubd (v ++, * cp);
	} else {
		for (i = 0 ; i < NFKEY ; i ++)	/* zap current settings */
			isfval [i] = 0;
		cp = isfbuf;			/* pointer to key buffer */
		for (i = 0 ; i < NFKEY ; i ++) {
			isfval [i] = cp;        /* save pointer to key string */
			while ((* cp ++ = getubd (v ++)) != -1)  /* copy key data */
				if (cp >= isfbuf + NFBUF - 3)  /* overflow? */
					return;
		}
	}
}


/*
 * Ioctl routine.
 */

void
isioctl (dev, com, vec)
dev_t dev;
struct sgttyb * vec;
{
	int s;

	switch (com) {
#define KDDEBUG 0
#if KDDEBUG
	case KDMEMDISP: {
		struct kd_memloc * mem;
		unsigned char ub, pb;
		mem = (struct kd_memloc *) vec;
		pxcopy (mem->physaddr, & pb, 1, SEL_386_KD);
		ub = getubd (mem->vaddr);
		printf ("User's byte %x (%x), Physical byte %x, Addresses %x %x\n",
			mem->ioflg, ub, pb, mem->vaddr, mem->physaddr);
		break;
	}
#endif
	case KDMAPDISP: {
		struct kd_memloc * mem;
		mem = (struct kd_memloc *) vec;
		mapPhysUser (mem->vaddr, mem->physaddr, mem->length);
		break;
	}

	case KDDISABIO:
		__kddisabio();
		break;

	case KDENABIO:
		__kdenabio();
		break;

	case KDADDIO:
		__kdaddio((unsigned short)vec);
		break;

	case KDDELIO:
		__kddelio((unsigned short)vec);
		break;

	case KIOCSOUND: {
		if (vec) {
			outb (TIMER_CTL, 0xB6); 
			outb (TIMER_CNT, (int)vec & 0xFF);
			outb (TIMER_CNT, (int)vec >> 8);
			/* Turn speaker on */
			outb (SPEAKER_CTL, inb (SPEAKER_CTL) | 03);
		} else 
			outb (SPEAKER_CTL, inb (SPEAKER_CTL) & ~ 03 );
		break;
	}

	case KDSKBMODE: {
		outb (KBCTRL, 0x0C);		/* Clock low */
		timeout (& tp, 3, kbstate, 1);	/* wait about 20-30ms */
		xlate = (int)vec;
		break;
	}

	case KDGKBSTATE: 
		* (char *) vec = '\x00';
		if (shift & SES)
			* (char *) vec |= M_KDGKBSTATE_SHIFT;
		if (shift & CTS)
			* (char *) vec |= M_KDGKBSTATE_CTRL;
		if (shift & ALS)
			* (char *) vec |= M_KDGKBSTATE_ALT;
		break;

	case KDSETLED: {
		X11led = (int)vec;
		updleds ();
		break;
	}

	case KDGETLED:
		* (char *) vec = 0;
		if (shift & NMLS)
			* (char *) vec |= M_KDGETLED_NUMLOCK;
		if (shift & CPLS)
			* (char *) vec |= M_KDGETLED_CAPLOCK;
		if (scrollkb & 1)
			* (char *) vec |= M_KDGETLED_SCRLOCK;
		break;

	case TIOCSETF:
	case TIOCGETF:
		isfunction (com, (char *)vec);
		break;

	case TIOCSHIFT:   /* switch left-SHIFT and "\" */
		lshiftkb = LSHIFTA;    /* alternate values */
		lmaptab [41] = '\\';
		lmaptab [42] = XXX;
		umaptab [41] = '|';
		umaptab [42] = XXX;
		smaptab [41] = SS1;
		smaptab [42] = SHFT;
		break;

	case TIOCCSHIFT:  /* normal (default) left-SHIFT and "\" */
		lshiftkb = LSHIFT;     /* normal values */
		lmaptab [41] = XXX;
		lmaptab [42] = '\\';
		umaptab [41] = XXX;
		umaptab [42] = '|';
		smaptab [41] = SHFT;
		smaptab [42] = SS1;
		break;

	  case KIOCINFO:
	        * (int *) vec = 0x6664;
	        break;

	/* Don't squawk if they try to load tables. */
	case TIOCSETKBT:
	        break;

	default:
		s = sphi ();
		ttioctl (vttty [vtindex (dev)], com, vec);
		spl (s);
		break;
	}
}


/*
 * Poll routine.
 */

int
ispoll (dev, ev, msec)
dev_t dev;
int ev;
int msec;
{
	return ttpoll (vttty [vtindex (dev)], ev, msec);
}


/*
 * Process numeric keypad for virtual terminals.
 */

void
vtnumeric (c)
int	c;
{
	switch (c) {
	case 71:			/* ctrl-7 */
		queue_vt_switch (vt7);
		break;

	case 72:			/* ctrl-8 */
		queue_vt_switch (vt8);
		break;

	case 73:			/* ctrl-9 */
		queue_vt_switch (vt9);
		break;

	case 74:			/* ctrl - */
		queue_vt_switch (vtp);
		break;

	case 75: 
	        queue_vt_switch (vt4);
	        break;

	case 76:
	        queue_vt_switch (vt5);
	        break;

	case 77:	/* ctrl 4/5/6 (vt5, vt6, vt7) */
		queue_vt_switch (vt6);
		break;

	case 78:			/* ctrl + */
		queue_vt_switch (vtn);
		break;

	case 79:			/* ctrl-1 */
		queue_vt_switch (vt1);
		break;

	case 80:			/* ctrl-2 */
		queue_vt_switch (vt2);
		break;

	case 81:			/* ctrl-3 */
		queue_vt_switch (vt3);
		break;

	case 82: 			/* ctrl 0  (vt0) */
		queue_vt_switch (vt0);
		break;

	case 83:			/* ctrl del (toggle) */
		c = vtt;
		queue_vt_switch (vtt);
		break;
	}
}


/**
 *
 * void
 * isin ( c )	-- append character to raw input silo
 * char c;
 */

static void
isin (c)
int c;
{
	int cache_it = 1;
	TTY * tp = vttty [vtactive];

	/*
	 * If using software incoming flow control, process and
	 * discard t_stopc and t_startc.
	 */
	if (_IS_IXON_MODE (tp)) {
		if (_IS_START_CHAR (tp, c) ||
		    (_IS_IXANY_MODE (tp) && (tp->t_flags & T_STOP) != 0)) {
			tp->t_flags &= ~ (T_STOP | T_XSTOP);
			ttstart (tp);
			cache_it = 0;
		} else if (_IS_STOP_CHAR (tp, c)) {
			if ((tp->t_flags & T_STOP) == 0)
				tp->t_flags |= (T_STOP | T_XSTOP);
			cache_it = 0;
		}
	}

	/*
	 * If the tty is not open the character is
	 * just tossed away.
	 */

	if (vttty [vtactive]->t_open == 0)
		return;

	/*
	 * Cache received character.
	 */
	if (cache_it) {
		in_silo.si_buf [ in_silo.si_ix ] = c;

		if ( ++ in_silo.si_ix >= sizeof (in_silo.si_buf) )
			in_silo.si_ix = 0;

	}
}


/*
 * Handle special input sequences.
 * The character passed is the key number.
 */

int
isspecial (c)
int c;
{
	char * cp;
	int s;
	int	update_leds = 0;

	cp = 0;

	switch (c) {
	case 15:					/* cursor back tab */
		cp = ESCAPE_STRING "[Z";
		break;

	case 53:
		if (kb_lang == kb_lang_fr || kb_lang == kb_lang_de) {
			if (extmode)
				cp = "/";
			else 
				if (shift & SES)
					cp = "_";
				else
					cp = "-";
		}
		break;

	case 55:					/* ignore PrtScr */
		if (kb_lang == kb_lang_fr || kb_lang == kb_lang_de) {
			if (! extmode)
				cp = "*";
		}
		break;

	case 59: case 60: case 61: case 62: case 63:	/* Function keys */
	case 64: case 65: case 66: 
		/* offset to function string */
		/* Magic numbers 21 and 61 to mach vtnkb constants */

		if ( shift & ALS ) {
			switch (c) {
			case 59:	/* Alt-F1 */
				queue_vt_switch (vt0);
				break;

			case 60:	/* Alt-F2 */
				queue_vt_switch (vt1);
				break;

			case 61:	/* Alt-F3 */
				queue_vt_switch (vt2);
				break;

			case 62:	/* Alt-F4 */
				queue_vt_switch (vt3);
				break;

			case 63:	/* Alt-F5 */
				queue_vt_switch (vt4);
				break;

			case 64:	/* Alt-F6 */
				queue_vt_switch (vt5);
				break;

			case 65:	/* Alt-F7 */
				queue_vt_switch (vt6);
				break;

			case 66:	/* Alt-F8 */
				queue_vt_switch (vt7);
				break;

			default:
				break;
			}
		} else if ((shift & (SES)) && (shift & CTS)) /* ctrl-shft-Fx */
			cp = isfval [c - 29];
		else if (shift & CTS)			/* ctrl-Fx */
			cp = isfval [c - 39];
		else if (shift & (SES))		/* shift-Fx */
			cp = isfval [c - 49];
		else
			cp = isfval [c - 59];			/* Plain Fx */
		break;

	case 67: case 68:
		/* offset to function string */
		if ( shift & ALS )  {
			switch (c) {
			case 67:	/* Alt-F9 */
				queue_vt_switch (vt8);
				break;

			case 68:	/* Alt-F10 */
				queue_vt_switch (vt9);
				break;

			default:
				break;
			}
		} else if ((shift & (SES)) && (shift & CTS)) /* cs-Fx */
			cp = isfval [c-29];
		else if (shift & CTS)		/* c-Fx */
			cp = isfval [c-39];
		else if (shift & (SES))	/* s-Fx */
			cp = isfval [c-49];
		else
			 cp = isfval [c-59];		/* Fx */
		break;

	case 70:		/* Scroll Lock -- stop / start output */
	{
		static char cbuf [2];

		cp = cbuf;	/* working buffer */
		if ((vttty [vtactive]->t_sgttyb.sg_flags & RAWIN) != 0)
			break;

		/* not if in RAW mode */

		++ update_leds;
		if (vttty [vtactive]->t_flags & T_STOP){/* output stopped? */
			cbuf [0] = vttty [vtactive]->t_tchars.t_startc;  
			scrollkb = 0;
		} else {
			cbuf [0] = vttty [vtactive]->t_tchars.t_stopc;   
			scrollkb = 1;
		}
		break;
	}

	case 79:		/* 1/ End */
	case 80:		/* 2/ DOWN */
	case 81:		/* 3/ PgDn */
	case 82:		/* 0/ Ins */
	case 83:		/* ./ Del */
		-- c;		/* adjust code */
		/* FALL THROUGH */

	case 75:		/* 4/ LEFT */
	case 76:		/* 5 */
	case 77:		/* 6/ RIGHT */
		-- c;		/* adjust code */
		/* FALL THROUGH */

	case 71:		/* 7/ Home / Clear */
	case 72:		/* 8/ UP */
	case 73:		/* 9/ PgUp */
		s = 0;			/* start off with normal keypad */
		if (shift & NMLS)		/* num lock? */
			s = 1;		/* set shift pad */
		if (shift & SES)		/* shift? */
			s ^= 1;		/* toggle shift pad */
		if (shift & AKPS)		/* alternate pad? */
			s = 2;		/* set alternate pad */
		if (kb_lang == kb_lang_fr || kb_lang == kb_lang_de) {
			if (extmode)		/* not from keypad? */
				s = 0;		/* force normal sequence */
		}
		cp = keypad [c - 71][s];   /* get keypad value */
		break;
	}
	if (cp)					/* send string */
		while ((* cp != 0) && (* cp != -1))
			isin (* cp ++ & 0xFF);
	return update_leds;
}


/**
 *
 * void
 * ismmfunc ( c )	-- process keyboard related output escape sequences
 * char c;
 */

void
ismmfunc (c)
int c;
{
	switch (c) {
	case 't':	/* Enter numlock */
		shift |= NMLS;
		updleds ();			/* update LED status */
		break;

	case 'u':	/* Leave numlock */
		shift &= ~ NMLS;
		updleds ();			/* update LED status */
		break;

	case '=':	/* Enter alternate keypad */
		shift |= AKPS;
		break;

	case '>':	/* Exit alternate keypad */
		shift &= ~ AKPS;
		break;

	case 'c':	/* Reset terminal */
		islock = 0;
		shift  = 0;
		initkeys ();
		updleds ();			/* update LED status */
		break;
	}
}


/**
 *
 * void
 * isbatch ()	-- raw input conversion routine
 *
 *	Action:	Enable the video display.
 *		Canonize the raw input silo.
 *
 *	Notes:	isbatch () was scheduled as a deferred process by vtkbintr ().
 */

static void
isbatch (tp)
TTY * tp;
{
	int c;
	static int lastc;
	VTDATA		* vp = (VTDATA *) tp->t_ddp;
	int s;

	/*
	 * Ensure video display is enabled.
	 */
	if (vp->vmm_visible)
		vtmm_von (vp);

	isbusy = 0;

	/*
	 * Process all cached characters.
	 */
	while (in_silo.si_ix != in_silo.si_ox) {
		/*
		 * Get next cached char.
		 */

	        s = sphi ();
		c = in_silo.si_buf [in_silo.si_ox];

		if (in_silo.si_ox >= sizeof (in_silo.si_buf) - 1)
			in_silo.si_ox = 0;
		else
			in_silo.si_ox ++;
		spl (s);

		if (islock == 0 || _IS_INTERRUPT_CHAR (tp, c) ||
		    _IS_QUIT_CHAR (tp, c)) {
			ttin (tp, c);
		} else if (c == 'b' && lastc == ESCAPE_CHAR) {
			islock = 0;
			ttin (tp, lastc);
			ttin (tp, c);
		} else if (c == 'c' && lastc == ESCAPE_CHAR) {
			ttin (tp, lastc);
			ttin (tp, c);
		} else
			putchar ('\a');

		lastc = c;
	}
}


/*
 * unlock the scroll in case an interrupt character is received
 */

void
kbunscroll ()
{
	scrollkb = 0;
	updleds ();
}


int
VTttyinit (i)
int i;
{
	TTY * tp;

	/*
	 * get pointer to TTY structure from kernal memory space
	 */
	if ((tp = vttty [i] = (TTY *) kalloc (sizeof (TTY))) == NULL)
		return 0;
	PRINTV ("     vttty [%d]: @%x, ", i, tp);

	tp->t_param = NULL;
	tp->t_start = & vtmmstart;

	tp->t_ddp = (char *) vtdata [i];
	PRINTV ("data @%lx\n", tp->t_ddp);
	return 1;
}


/*
 * Given device number, return index for vtdata [], vttty [], etc.
 *
 * Major number must be VT_MAJOR for CPU to get here.
 *
 *      Minor Number	Index Value
 *	----- ------ 	----- -----  
 *	0000  0000	vtactive ... device (2, 0) is the active screen
 *	0000  0001	0
 *	0000  0010	1
 *	0000  0011	2
 *	   ....
 *	0000  1111	14
 *
 *	0100  xxxx	xxxx ... color devices only
 *	0101  xxxx	xxxx - (# of color devices found) ... monochrome only
 *
 * Return value is in range 0 to vtcount-1 for valid minor numbers,
 * -1 for invalid minor numbers.
 */

int
vtindex (dev)
dev_t dev;
{
	int	ret = -1;

	if ( dev & VT_PHYSICAL ) {
		int	hw = ( dev >> 4 ) & 3;
		int	hw_index = dev & 0x0F;

		if ( hw_index < vtHWtable [hw]->found )
			ret = vtHWtable [hw]->start + hw_index;
	} else {
		int	lg_index = dev & 0x0F;

		if (lg_index == 0)
			ret = vtactive;
		if (lg_index > 0 && lg_index <= vtcount ) 
			ret = lg_index-1;
	}
	if (ret >= 0)
		ret %= vtcount;
	else
		PRINTV ( "vtindex: (%x) %d. invalid !\n", dev, ret );
	return ret;
}


/*
 * Given a function key number (e.g. vt0),
 * return the corresponding minor device number.
 *
 * Assume valid key number (VTKEY (fnum) is true) by the time we get here.
 */

int
vtkey_to_dev (fnum)
int fnum;
{
	if (fnum >= vt0 && fnum <= vt15)
		return fnum - vt0 + 1;
	if (fnum >= color0 && fnum <= color15)
		return (fnum - color0) | (VT_PHYSICAL | VT_HW_COLOR);
	if (fnum >= mono0 && fnum <= mono15)
		return (fnum - mono0) | (VT_PHYSICAL | VT_HW_MONO);

	printf ("vtkey_to_dev (%d)! ", fnum);
	return 0;
}


/*
 * Receive interrupt.
 */

void
vtkbintr ()
{
	int	c;
	int	s;
	int	r;
	int	savests;
	int	update_leds = 0;
	int	i;

	if (isbusy == 0) {
		isbusy = 1;
		defer (isbatch, vttty [vtactive]);
	}

	
	/*
	 * Pull character from the data
	 * port. Pulse the KBFLAG in the control
	 * port to reset the data buffer.
	 */

	r = inb (KBDATA) & 0xFF;
	c = inb (KBCTRL);
	outb (KBCTRL, c | KBFLAG);
	outb (KBCTRL, c);

	if (! xlate) {
		if (ledcmd) {
			if (r == KBACK) {		/* output to status LEDS */
				ledcmd --;
				outb (KBDATA, X11led);
				return;
			}
		}
		isin (r);
		return;
	}

#if	KBDEBUG
	printf ("kbd: %d\n", r);			/* print scan code / direction */
#endif
	if (ledcmd) {
		if (r == KBACK) {		/* output to status LEDS */
		        ledcmd --;

			 /*
			  * This is a stupid delay for a stupid way
			  * of doing things.  Timing constraints between
			  * an ACK receive and a sending of data should
			  * have been implemented, so now we suffer.
			  */

			busyWait2(0, 2500);
			c = scrollkb & 1;
			if (shift & NMLS)
				c |= 2;
			if (shift & CPLS)
				c |= 4;
			outb (KBDATA, c);
		}
		return;
	}

	if (extended > 0) {			/* if multi-character seq, */
		-- extended;			/* ... ignore this char */
		return;
	}

	switch (r) {
	case EXTENDED0:				/* 0xE0 prefix found */
		if (kb_lang == kb_lang_fr || kb_lang == kb_lang_de) {
			ext0seen = 1;
			return;
		}
		break;

	case EXTENDED1:				/* ignore extended sequences */
		extended = 5;
		return;

	case 0xFF:				/* Overrun */
		return;
	}

	if (kb_lang == kb_lang_fr || kb_lang == kb_lang_de) {
		if (ext0seen) {
			ext0seen = 0;
			extmode = 1;
		} else 
			extmode = 0;
	}

	c = (r & KEYSC) - 1;

#if	0
	/*
	 * Check for reset.
	 */

	if ((r & KEYUP) == 0 && c == DELETE &&
	    (shift & (CTS | ALS)) == (CTS | ALS))
		boot ();
#endif

	/*
	 * Track "shift" keys.
	 */

	s = smaptab [c];
	if (s & SHFT) {
		if (r & KEYUP) {			/* "shift" released */
			if (c == RSHIFT)
				shift &= ~ SRS;
			else if (c == lshiftkb)
				shift &= ~ SLS;
			else if (c == CTRLkb)
				shift &= ~ CTS;
			else if (c == ALTkb) {
				if (kb_lang == kb_lang_fr ||
				    kb_lang == kb_lang_de)
					shift &= extmode ? ~ AGS : ~ ALS;
				else
					shift &= ~ ALS;
			}
		} else {			/* "shift" pressed */
			if (c == lshiftkb)
				shift |= SLS;
			else if (c == RSHIFT)
				shift |= SRS;
			else if (c == CTRLkb)
				shift |= CTS;
			else if (c == ALTkb) {
				if (kb_lang == kb_lang_fr ||
				    kb_lang == kb_lang_de)
					shift |= extmode ? AGS : ALS;
				else
					shift |= ALS;
			} else if (c == CAPLOCK) {
				shift ^= CPLS;	/* toggle cap lock */
				updleds ();
			} else if (c == NUMLOCK) {
				shift ^= NMLS;	/* toggle num lock */
				updleds ();
			}
		}
		return;
	}

	/*
	 * No other key up codes of interest.
	 */
	if (r & KEYUP)
		return;

	/*
	 * Map character, based on the
	 * current state of the shift, control, alt graphics,
	 * meta (ALT) and lock flags.
	 */
	if ((shift & AGS) &&  /* Alt Graphics ? */
	    (kb_lang == kb_lang_fr || kb_lang == kb_lang_de))
		c = agmaptab [c];
	else if (shift & CTS) {
		if (s == CTS)			/* Map Ctrl (BS | NL) */
			c = (c == BACKSP) ? 0x7F : 0x0A;
		else if (s == SS1 || s == LET) {
			if ((c = umaptab [c]) != SPC)	/* Normal Ctrl map */
				c &= 0x1F;	/* Clear bits 5-6 */
		} else {
			if (s == KEY || s == SS0) 
				vtnumeric (r);
			return;			/* Ignore this char */
		}
	} else if (s &= shift) {
		if (shift & SES) {		/* if shift on */
			if (s & (CPLS | NMLS))  /* if caps / num lock */
				c = lmaptab [c];/* use unshifted */
			else 
				c = umaptab [c];/* use shifted */
			
		} else {			/* if shift not on */
			if (s & (CPLS | NMLS))	/* if caps / num lock */
				c = umaptab [c];/* use shifted */
			else
				c = lmaptab [c]; /* use unshifted */
		}
	} else
		c = lmaptab [c];		 /* use unshifted */

	/*
	 * Act on character.
	 */
	if (c == XXX)
		return;				 /* char to ignore */

	if (c != SPC) {			 /* not special char? */
		if (shift & ALS)	 /* ALT (meta bit)? */
			c |= 0x80;	 /* set meta */
		isin (c);		 /* send the char */
	} else
		update_leds += isspecial (r);	 /* special chars */

	if (update_leds) {
		savests = sphi ();
		outb (KBDATA, LEDCMD);
		ledcmd ++;
		spl (savests);
	}
}


CON vtkbcon = {
	DFCHR | DFPOL,			/* Flags */
	KB_MAJOR,			/* Major index */
	isopen,				/* Open */
	isclose,			/* Close */
	NULL,				/* Block */
	isread,				/* Read */
	vtmmwrite,			/* Write */
	isioctl,			/* Ioctl */
	NULL,				/* Powerfail */
	vtmmwatch,			/* Timeout */
	isload,				/* Load */
	isuload,			/* Unload */
	ispoll				/* Poll */
};
