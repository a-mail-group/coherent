# /usr/src/libc/crt/Makefile 6/8/94

LIBOBJ=\
	$(TARGET)(_prof.o)

# Primary target.
all:	$(LIBOBJ) crt$(MCH)
	: libc/crt done.

# Primary NDP target.
ndp:	$(LIBOBJ) crti387
	: libc/crt done.

# Secondary targets.
crt$(MCH):
	$(MAKE) $(MAKEARGS) "SRCPATH=$(SRCPATH)/$(MCH)" "MAKEARGS=$(MAKEARGS)"

crti387:
	$(MAKE) $(MAKEARGS) "SRCPATH=$(SRCPATH)/$(MCH)" "MAKEARGS=$(MAKEARGS)" ndp
	$(MAKE) $(MAKEARGS) "SRCPATH=$(SRCPATH)/i387" "MAKEARGS=$(MAKEARGS)"

# end of /usr/src/libc/crt/Makefile
