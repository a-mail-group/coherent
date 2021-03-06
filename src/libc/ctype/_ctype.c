/*
 * C character handling library.
 * _ctype[]
 * ASCII character set type table for ctype macros and functions.
 * Indices:
 *	  0		EOF			[-1]
 *	  1 <= n <= 128	ASCII character n-1	[NUL, ..., DEL]
 *	129 <= n <= 256	NonASCII character n-1	[0x80, ... 0xFF]
 * setlocale() may overwrite this with locale-specific type information.
 */

#include <ctype.h>

unsigned char _ctype[_CTYPEN] = {
	0,				/* EOF				*/
	_C,	_C,	_C,	_C,	/* NUL	SOH	STX	ETX	*/
	_C,	_C,	_C,	_C,	/* EOT	ENQ	ACK	BEL	*/
	_C,	_S|_C,	_S|_C,	_S|_C,	/* BS	HT	LF	VT	*/
	_S|_C,	_S|_C,	_C,	_C,	/* FF	CR	SO	SI	*/
	_C,	_C,	_C,	_C,	/* DLE	DC1	DC2	DC3	*/
	_C,	_C,	_C,	_C,	/* DC4	NAK	SYN	ETB	*/
	_C,	_C,	_C,	_C,	/* CAN	EM	SUB	ESC	*/
	_C,	_C,	_C,	_C,	/* FS	GS	RS	US	*/
	_S|_B,	_P,	_P,	_P,	/* SP	!	"	#	*/
	_P,	_P,	_P,	_P,	/* $	%	&	'	*/
	_P,	_P,	_P,	_P,	/* (	)	*	+	*/
	_P,	_P,	_P,	_P,	/* ,	-	.	/	*/
	_N|_X,	_N|_X,	_N|_X,	_N|_X,	/* 0	1	2	3	*/
	_N|_X,	_N|_X,	_N|_X,	_N|_X,	/* 4	5	6	7	*/
	_N|_X,	_N|_X,	_P,	_P,	/* 8	9	:	;	*/
	_P,	_P,	_P,	_P,	/* <	=	>	?	*/
	_P,	_U|_X,	_U|_X,	_U|_X,	/* @	A	B	C	*/
	_U|_X,	_U|_X,	_U|_X,	_U,	/* D	E	F	G 	*/
	_U,	_U,	_U,	_U,	/* H	I	H	K	*/
	_U,	_U,	_U,	_U,	/* L	M	N	O	*/
	_U,	_U,	_U,	_U,	/* P	Q	R	S	*/
	_U,	_U,	_U,	_U,	/* T	U	V	W	*/
	_U,	_U,	_U,	_P,	/* X	Y	Z	[	*/
	_P,	_P,	_P,	_P,	/* \	]	^	_	*/
	_P,	_L|_X,	_L|_X,	_L|_X,	/* `	a	b	c	*/
	_L|_X,	_L|_X,	_L|_X,	_L,	/* d	e	f	g	*/
	_L,	_L,	_L,	_L,	/* h	i	j	k	*/
	_L,	_L,	_L,	_L,	/* l	m	n	o	*/
	_L,	_L,	_L,	_L,	/* p	q	r	s	*/
	_L,	_L,	_L,	_L,	/* t	u	v	w	*/
	_L,	_L,	_L,	_P,	/* x	y	z	{	*/
	_P,	_P,	_P,	_C,	/* |	}	~	DEL	*/
	0,	0,	0,	0,	/* 0x80	0x81	0x82	0x83	*/
	0,	0,	0,	0,	/* 0x84				*/
	0,	0,	0,	0,	/* 0x88				*/
	0,	0,	0,	0,	/* 0x8C				*/
	0,	0,	0,	0,	/* 0x90				*/
	0,	0,	0,	0,	/* 0x94				*/
	0,	0,	0,	0,	/* 0x98				*/
	0,	0,	0,	0,	/* 0x9C				*/
	0,	0,	0,	0,	/* 0xA0				*/
	0,	0,	0,	0,	/* 0xA4				*/
	0,	0,	0,	0,	/* 0xA8				*/
	0,	0,	0,	0,	/* 0xAC				*/
	0,	0,	0,	0,	/* 0xB0				*/
	0,	0,	0,	0,	/* 0xB4				*/
	0,	0,	0,	0,	/* 0xB8				*/
	0,	0,	0,	0,	/* 0xBC				*/
	0,	0,	0,	0,	/* 0xC0				*/
	0,	0,	0,	0,	/* 0xC4				*/
	0,	0,	0,	0,	/* 0xC8				*/
	0,	0,	0,	0,	/* 0xCC				*/
	0,	0,	0,	0,	/* 0xD0				*/
	0,	0,	0,	0,	/* 0xD4				*/
	0,	0,	0,	0,	/* 0xD8				*/
	0,	0,	0,	0,	/* 0xDC				*/
	0,	0,	0,	0,	/* 0xE0				*/
	0,	0,	0,	0,	/* 0xE4				*/
	0,	0,	0,	0,	/* 0xE8				*/
	0,	0,	0,	0,	/* 0xEC				*/
	0,	0,	0,	0,	/* 0xF0				*/
	0,	0,	0,	0,	/* 0xF4				*/
	0,	0,	0,	0,	/* 0xF8				*/
	0,	0,	0,	0	/* 0xFC	0xFD	0xFE	0xFF	*/
};
