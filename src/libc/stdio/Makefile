# /usr/src/libc/stdio/Makefile 9/13/94

# The order of stdio objects in libc.a is significant and a bit complicated.
# To reduce the size of linked objects when possible,
# libc includes both real and fake versions of _dtefg() (floating point output)
# and of _finish() (close open FILEs).
# If a program requires floating point output, the "-f" flag to /bin/cc
# produces an "-u _dtefg_" argument to /bin/ld, forcing the link to include
# the real floating point output routine from _dtefg.o.
# Otherwise, /bin/ld resolves the reference to _dtefg() in vfprintf.o with
# the much smaller fake floating point output routine from _fdtefg.o.
# To avoid linking in extra stdio overhead when not required,
# libc.a should contain _fp.o, _finit.o, exit.o and _finish.o, in that order,
# after stdio objects from $(OBJ) and from the machine-dependent subdirectory,
# If a program uses stdio, /bin/ld will link in _finit.o;
# finit.o defines the real _finish() which closes all open files for exit().
# If a program does not use stdio, /bin/ld will link in exit.o but not _finit.o;
# then it resolves the _finish() reference in exit() with the fake _finish.o,
# which does nothing.

LIBOBJ=\
	$(TARGET)(_dscan.o) \
	$(TARGET)(_fgetb.o) \
	$(TARGET)(_fgetc.o) \
	$(TARGET)(_fgete.o) \
	$(TARGET)(_fgetstr.o) \
	$(TARGET)(_filbuf.o) \
	$(TARGET)(_flsbuf.o) \
	$(TARGET)(_fopen.o) \
	$(TARGET)(_fpseek.o) \
	$(TARGET)(_fputa.o) \
	$(TARGET)(_fputb.o) \
	$(TARGET)(_fputc.o) \
	$(TARGET)(_fpute.o) \
	$(TARGET)(_fputt.o) \
	$(TARGET)(_scanf.o) \
	$(TARGET)(_stropen.o) \
	$(TARGET)(clearerr.o) \
	$(TARGET)(ctermid.o) \
	$(TARGET)(ecvt.o) \
	$(TARGET)(fclose.o) \
	$(TARGET)(fcvt.o) \
	$(TARGET)(fdopen.o) \
	$(TARGET)(feof.o) \
	$(TARGET)(ferror.o) \
	$(TARGET)(fflush.o) \
	$(TARGET)(fgetc.o) \
	$(TARGET)(fgetpos.o) \
	$(TARGET)(fgets.o) \
	$(TARGET)(fopen.o) \
	$(TARGET)(fputc.o) \
	$(TARGET)(fputs.o) \
	$(TARGET)(fread.o) \
	$(TARGET)(freopen.o) \
	$(TARGET)(fscanf.o) \
	$(TARGET)(fseek.o) \
	$(TARGET)(fsetpos.o) \
	$(TARGET)(ftell.o) \
	$(TARGET)(fwrite.o) \
	$(TARGET)(gcvt.o) \
	$(TARGET)(getc.o) \
	$(TARGET)(getchar.o) \
	$(TARGET)(gets.o) \
	$(TARGET)(perror.o) \
	$(TARGET)(popen.o) \
	$(TARGET)(putc.o) \
	$(TARGET)(putchar.o) \
	$(TARGET)(puts.o) \
	$(TARGET)(remove.o) \
	$(TARGET)(rewind.o) \
	$(TARGET)(scanf.o) \
	$(TARGET)(setbuf.o) \
	$(TARGET)(setvbuf.o) \
	$(TARGET)(sscanf.o) \
	$(TARGET)(tmpfile.o) \
	$(TARGET)(tmpnam.o) \
	$(TARGET)(ungetc.o)

# rename is a system call as of COHERENT 4.2.r05.
LIBUNUSED=\
	$(TARGET)(rename.o)

# N.B. The order of objects in $(OBJ2) is significant,
# see the lengthy comment above and be extremely careful if you change it.
LIBOBJ2=\
	$(TARGET)(_dtefg.o) \
	$(TARGET)(fprintf.o) \
	$(TARGET)(printf.o) \
	$(TARGET)(sprintf.o) \
	$(TARGET)(vprintf.o) \
	$(TARGET)(vsprintf.o) \
	$(TARGET)(vfprintf.o) \
	$(TARGET)(_fdtefg.o) \
	$(TARGET)(_fp.o) \
	$(TARGET)(_finit.o) \
	$(TARGET)(exit.o) \
	$(TARGET)(_finish.o)

# Primary target.
# This assumes that either libc.a is being made from scratch
# or the names of the objects in $(OBJ2) are unchanged.
# Otherwise, "$(AR) rkus libc.a $?" may not preserve the required object order.
# It could be done more correctly with $(AR) positional specifications.
all:	stdio$(MCH) $(LIBOBJ) $(LIBOBJ2)
	: libc/stdio done.

# Secondary target.
stdio$(MCH):
	$(MAKE) $(MAKEARGS) "SRCPATH=$(SRCPATH)/$(MCH)" "MAKEARGS=$(MAKEARGS)"

# end of /usr/src/libc/stdio/Makefile
