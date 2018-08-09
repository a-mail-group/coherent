# /usr/src/libc/crt/i386/Makefile 6/8/94

# Software floating point support objects.
LIBOBJ=\
	$(TARGET)(_fxam.o) \
	$(TARGET)(_modf.o) \
	$(TARGET)(daddsub.o) \
	$(TARGET)(dcmp.o) \
	$(TARGET)(ddiv.o) \
	$(TARGET)(dfcvt.o) \
	$(TARGET)(dicvt.o) \
	$(TARGET)(dmul.o) \
	$(TARGET)(ducvt.o) \
	$(TARGET)(fdcvt.o) \
	$(TARGET)(fops.o) \
	$(TARGET)(frexp.o) \
	$(TARGET)(idcvt.o) \
	$(TARGET)(ifcvt.o) \
	$(TARGET)(ldexp.o) \
	$(TARGET)(modf.o) \
	$(TARGET)(scount.o) \
	$(TARGET)(udcvt.o) \
	$(TARGET)(ufcvt.o)

# NDP floating point support objects.
NDPLIBOBJ=\
	$(TARGET)(_fxam.o) \
	$(TARGET)(scount.o)

# Primary target.
all:	$(LIBOBJ)
	: libc/crt/i386 done

# Primary NDP target.
ndp:	$(NDPLIBOBJ)
	: NDP libc/crt/i386 done

# end of /usr/src/libc/crt/i386/Makefile
