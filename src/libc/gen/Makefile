# /usr/src/libc/gen/Makefile 9/13/94

# N.B. ttyname compilation requires -DNEWTTYS=1, now #defined in the source.
LIBOBJ=\
	$(TARGET)(__assertfail.o) \
	$(TARGET)(__fltused.o) \
	$(TARGET)(_addargs.o) \
	$(TARGET)(_cleanup.o) \
	$(TARGET)(_pow10.o) \
	$(TARGET)(coffnlist.o) \
	$(TARGET)(crypt.o) \
	$(TARGET)(ctol.o) \
	$(TARGET)(decvax_d.o) \
	$(TARGET)(decvax_f.o) \
	$(TARGET)(drand48.o) \
	$(TARGET)(fnmatch.o) \
	$(TARGET)(ftok.o) \
	$(TARGET)(getcwd.o) \
	$(TARGET)(getgrent.o) \
	$(TARGET)(getlogin.o) \
	$(TARGET)(getopt.o) \
	$(TARGET)(getpass.o) \
	$(TARGET)(getpw.o) \
	$(TARGET)(getpwent.o) \
	$(TARGET)(getspent.o) \
	$(TARGET)(getwd.o) \
	$(TARGET)(ieee_d.o) \
	$(TARGET)(ieee_f.o) \
	$(TARGET)(isatty.o) \
	$(TARGET)(lockf.o) \
	$(TARGET)(ltoc.o) \
	$(TARGET)(mkfifo.o) \
	$(TARGET)(mktemp.o) \
	$(TARGET)(monitor.o) \
	$(TARGET)(mtype.o) \
	$(TARGET)(nlist.o) \
	$(TARGET)(notify.o) \
	$(TARGET)(path.o) \
	$(TARGET)(pnmatch.o) \
	$(TARGET)(raise.o) \
	$(TARGET)(shellsort.o) \
	$(TARGET)(sleep.o) \
	$(TARGET)(swab.o) \
	$(TARGET)(sys_err.o) \
	$(TARGET)(tempnam.o) \
	$(TARGET)(ttyname.o) \
	$(TARGET)(ttyslot.o)

# Primary target.
all:	$(LIBOBJ) gen$(MCH)
	: libc/gen done.

# Secondary target.
gen$(MCH):
	$(MAKE) $(MAKEARGS) "SRCPATH=$(SRCPATH)/$(MCH)" "MAKEARGS=$(MAKEARGS)"

# end of /usr/src/libc/gen/Makefile
