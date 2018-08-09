# /usr/src/libc/ndir/Makefile 6/8/94

LIBOBJ=\
	$(TARGET)(closedir.o) \
	$(TARGET)(getdents.o) \
	$(TARGET)(opendir.o) \
	$(TARGET)(readdir.o) \
	$(TARGET)(rewinddir.o) \
	$(TARGET)(seekdir.o) \
	$(TARGET)(telldir.o)

# Add some explicit definitions to the passed CFLAGS.
CCFLAGS=-I$(SRCPATH) -DUFS -DBIT_16

.c.a:
	$(CC) -c $(CFLAGS) $(CCFLAGS) $<
	$(AR) $(ARFLAGS) $@ $%
	rm -f $%

# Primary target.
all:	$(LIBOBJ)
	: libc/ndir done

# end of /usr/src/libc/ndir/Makefile
