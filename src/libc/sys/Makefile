# /usr/src/libc/sys/Makefile 6/8/94

LIBOBJ=\
	$(TARGET)(execl.o) \
	$(TARGET)(execle.o) \
	$(TARGET)(execlp.o) \
	$(TARGET)(execlpe.o) \
	$(TARGET)(execv.o) \
	$(TARGET)(execve.o) \
	$(TARGET)(execvp.o) \
	$(TARGET)(execvpe.o)

UNUSED=\
	brk.o

# Primary target.
all:	$(LIBOBJ) sys$(MCH)
	: libc/sys done.

# Secondary target.
sys$(MCH):
	$(MAKE) $(MAKEARGS) "SRCPATH=$(SRCPATH)/$(MCH)" "MAKEARGS=$(MAKEARGS)"

# end of /usr/src/libc/sys/Makefile
