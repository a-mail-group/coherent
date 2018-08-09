//////////
/ n1/i386/tables/aft.t
//////////

/////////
/
/ Postfix increment and decrement (@++, @--).
/ The PTX case, which does these operations on pointers,
/ must handle anything on the right hand side.
/ The other cases must only handle a 1.
/ OP0 is ADD or SUB, OP1 is INC or DEC; cf. n1/i386/table1.c/optab[].
/ For the TL OP1 implementation, cf. n1/i386/outmch.c/maptype().
/
/////////

INCAFT:
DECAFT:

/ Integer or pointer.
/ Increment integer or pointer by 1.
%	PEFFECT|PVALUE
	DWORD		ANYR	*	*	TEMP
		ADR		DWORD|SHORT
		1|MMX		*
		[IFV]	[TL ZMOVSX]	[R],[AL]
			[TL OP1]	[AL]

/ Increment pointer by constant.
%	PEFFECT|PVALUE
	DWORD		ANYR	*	*	TEMP
		ADR		DWORD
		IMM|MMX		LONG
		[IFV]	[ZMOV]	[R],[AL]
			[OP0]	[AL],[AR]

//////////
/
/ Fields.
/ My heart says there is a better way to do the field versions
/ of this operator with a clever sequence of XCHG and XOR;
/ however, I cannot think of it and neither can Trevor.
/
//////////

/ Field.
%	PEFFECT|PVALUE|PBYTE
	LONG		ANYR	*	*	TEMP
		ADR|LV		FLD
		IMM|MMX		LONG
			[TL ZMOVZX]	[R],[AL]	/ fetch
		[IFV]	[ZAND]		[R],[EMASK]	/ extract
		[IFV]	[ZPUSH]		[R]		/ save old value
			[OP0]		[R],[AR]	/ bump
			[ZAND]		[R],[EMASK]	/ mask
			[TL ZAND]	[AL],[TL CMASK]	/ clear
			[TL ZOR]	[AL],[TL R]	/ store
		[IFV]	[ZPOP]		[R]		/ restore old value

//////////
/ end of n1/i386/tables/aft.t
//////////
