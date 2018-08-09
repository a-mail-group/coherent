//////////
/ libc/sys/i386/sigjmp.s
/ C library - sigjmp
//////////

	.unixorder

/ Process context save/restore functions for use in association with POSIX.1
/ signalling. This version is only suitable for iBCS2 systems, which don't
/ store such esoterica as the NPX state.
/
/ $Log: $
/
		.globl	sigsetjmp
		.globl	siglongjmp

/ The first part of a "sigjmp_buf" is identical to a regular jmp_buf, i.e.
/ containing an image of %ebp, %esp, return address, %esi, %edi, %ebx in that
/ order. After that we add in "savemask" and a copy of the signal mask.

_ebp		=	0
_esp		=	4
_retaddr	=	8
_esi		=	12
_edi		=	16
_ebx		=	20
_savemask	=	24
_sigmask	=	28

/ int sigsetjmp (sigjmp_buf buf, int savemask);
buf		=	4
savemask	=	8

sigsetjmp:
		movl	savemask(%esp), %eax
		movl	buf(%esp), %edx
		pop	%ecx		/ get return address

		movl	%ebp, _ebp(%edx)
		movl	%esp, _esp(%edx)
		movl	%ecx, _retaddr(%edx)
		movl	%esi, _esi(%edx)
		movl	%edi, _edi(%edx)
		movl	%ebx, _ebx(%edx)
		movl	%eax, _savemask(%edx)

		orl	%eax, %eax
		je	?nosavesigmask

		leal	_sigmask(%edx), %eax
		pushl	%eax		/ oset

		subl	%eax, %eax
		pushl	%eax		/ set
		pushl	%eax		/ how
		pushl	%eax		/ dummy return value

		movl	$0x2828, %eax	/ sigprocmask system call number
		lcall	$0x07, $0	/ enter the kernel

		addl	$16, %esp	/ pop arguments

?nosavesigmask:
		ijmp	%ecx		/ return to caller


/ void siglongjmp (sigjmp_buf buf, int retval);

buf		=	4
retval		=	8

siglongjmp:
		movl	buf(%esp), %edx
		movl	_savemask(%edx), %eax
		orl	%eax, %eax
		je	?norestsigmask

		subl	%ecx, %ecx
		pushl	%ecx		/ oset
		leal	_sigmask(%edx), %eax
		pushl	%eax		/ set
		pushl	%ecx		/ how == SIG_SETMASK
		pushl	%ecx		/ dummy return value

		movl	$0x2828, %eax	/ sigprocmask system call no.
		lcall	$0x07, $0

		movl	buf+16(%esp), %edx	/ system call clears %edx

?norestsigmask:
		movl	retval+16(%esp), %eax	/ new return value
		orl	%eax, %eax
		jne	?ok_retval

		incl	%eax			/ must be nonzero
?ok_retval:
		movl	_ebp(%edx), %ebp
		movl	_esp(%edx), %esp
		movl	_esi(%edx), %esi
		movl	_edi(%edx), %edi
		movl	_ebx(%edx), %ebx

		ijmp	_retaddr(%edx)

/ end of sigjmp.s
