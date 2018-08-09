# /usr/src/libc/stdlib/Makefile 6/8/94

# This directory should contain all the ANSI <stdlib.h> sources
# but does not, simply for historical reasons.
# They should be moved here in the future.

LIBOBJ=\
	$(TARGET)(abs.o) \
	$(TARGET)(atexit.o) \
	$(TARGET)(atof.o) \
	$(TARGET)(atoi.o) \
	$(TARGET)(atol.o) \
	$(TARGET)(bsearch.o) \
	$(TARGET)(getenv.o) \
	$(TARGET)(labs.o) \
	$(TARGET)(putenv.o) \
	$(TARGET)(qsort.o) \
	$(TARGET)(rand.o) \
	$(TARGET)(strtod.o) \
	$(TARGET)(strtol.o) \
	$(TARGET)(system.o)

# Primary target.
all:	$(LIBOBJ) stdlibmalloc stdlib$(MCH)
	: libc/stdlib done.

# Secondary targets.
stdlibmalloc:
	$(MAKE) $(MAKEARGS) "SRCPATH=$(SRCPATH)/malloc" "MAKEARGS=$(MAKEARGS)"

stdlib$(MCH):
	$(MAKE) $(MAKEARGS) "SRCPATH=$(SRCPATH)/$(MCH)" "MAKEARGS=$(MAKEARGS)"

# end of /usr/src/libc/stdlib/Makefile
