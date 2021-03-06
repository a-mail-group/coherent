//////////
/ libc/crt/i386/udcvt.s
/ i386 C runtime library.
/ IEEE software floating point support.
//////////

	.intelorder

//////////
/ unsigned int	_udcvt()
/
/ Convert double in %edx:%eax to unsigned int, return in %eax.
//////////

BIAS	.equ	1023
EXPMASK	.equ	0x7FF00000
MANMASK	.equ	0x000FFFFF
HIDDEN	.equ	0x00100000
MAXUINT	.equ	0xFFFFFFFF

	.globl	_udcvt

_udcvt:
	push	%ecx
	orl	%edx, %edx
	js	?zero			/ negative arg, return 0
	mov	%ecx, %edx
	andl	%ecx, $EXPMASK		/ shifted biased exponent to ECX
	shrl	%ecx, $20		/ biased exponent
	subl	%ecx, $BIAS		/ unbiased exponent
	jl	?zero			/ too small, return 0
	cmpl	%ecx, $31
	jg	?inf			/ too big, return MAXUINT
	andl	%edx, $MANMASK		/ extract hi mantissa in EDX
	orl	%edx, $HIDDEN		/ restore hidden bit
	shrd	%eax, %edx, $21		/ mantissa in EAX
	negl	%ecx
	addl	%ecx, $31		/ shift count to CL
	shrl	%eax, %cl		/ shift mantissa to correct position
	pop	%ecx
	ret

?inf:
	mov	%eax, $MAXUINT		/ overflow, return MAXUINT
	pop	%ecx
	ret

?zero:
	subl	%eax, %eax
	pop	%ecx
	ret

/ end of libc/crt/i386/udcvt.s
