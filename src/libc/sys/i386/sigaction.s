//////////
/ libc/sys/i386/sigaction.s
/ C library - sigaction
//////////

	.unixorder
	.globl	sigaction
	.globl	.cerror

sigaction:
	movl	$sigreturn,%edx		/ proc = &sigreturn
	movl	$10024,%eax
	lcall	$0x7,$0
	jc	.cerror
	ret


sigreturn:
	addl	$4, %esp
	lcall	$0xf, $0
	/ notreached...

/ end of sigaction.s
