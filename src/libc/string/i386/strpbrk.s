//////////
/ libc/string/i386/strpbrk.s
/ i386 C string library.
/ ANSI 4.11.5.4.
//////////

	.intelorder

//////////
/ char *
/ strpbrk(char *String1, char *String2)
/
/ Return a pointer to the first char in String1
/ which matches any character in String2.
//////////

String1	.equ	12
String2	.equ	String1+4

	.globl	strpbrk

strpbrk:
	push	%esi
	push	%edi

	movl	%edi, String1(%esp)	/ String1 to EDI
	subl	%eax, %eax		/ Clear EAX for NULL return value
	cld

	/ AL and the high word of EAX are always 0 here.
?fetch1:
	movb	%ah, (%edi)		/ Fetch String1 char to AH
	incl	%edi			/ and point to next
	orb	%ah, %ah
	jz	?done			/ No match found, return NULL
	movl	%esi, String2(%esp)	/ String2 to ESI

?fetch2:
	lodsb				/ Fetch String2 char to AL
	orb	%al, %al
	jz	?fetch1			/ No match in String2, try next String1 char
	cmpb	%ah, %al
	jnz	?fetch2			/ Mismatch, try next String2 char
	decl	%edi			/ Back up to match 
	movl	%eax, %edi		/ Return pointer in EAX

?done:
	pop	%edi
	pop	%esi
	ret

/ end of libc/string/i386/strpbrk.s
