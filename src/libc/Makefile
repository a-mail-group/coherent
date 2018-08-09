# /usr/src/libc/Makefile 6/9/94
#
# Assumes $(SRCPATH) defines root libc source directory, e.g. /usr/src/libc.
# Assumes $(MCH) defines target machine, e.g. i8086 or i386.
#
# To build a new native libc.a, move to an empty object directory and type e.g.
#	make SRCPATH=/usr/src/libc MCH=i386
# or
#	export SRCPATH=/usr/src/libc
#	export MCH=i386
#	make
# To build a new native NDP libc.a, type e.g.
#	export CFLAGS=-VNDP
#	export SRCPATH=/usr/src/libc
#	export MCH=i386
#	make -e
#
# To build a library in a cross-development environment,
# set $(AR), $(AS), $(ASFLAGS), $(CC), $(CFLAGS) appropriately
# in the environment and then invoke make -e.
#
# Takes about 8 minutes from scratch on 33 MHz i386 running COH 4.2.
SRCPATH=.
MCH=i386
TARGET=libc.a
ARFLAGS=rckus
CC=cc
CFLAGS=

# The order of targets here is significant.
# stdio.dir must be the last target, because it plays tricks to reduce size
# of executable if stdio is not required.
# gen.dir is next to last because e.g. perror() references stdio.
# copyright.dir is not included because it is the first object in the archive.
# crt.dir is not included because it is different for NDP and non-NDP versions.
DIRS= \
	ctype.dir \
	ndir.dir \
	string.dir \
	sys.dir \
	stdlib.dir \
	time.dir \
	gen.dir \
	stdio.dir

# Arguments for make.
MAKEARGS=-e MCH=$(MCH) TARGET=$(TARGET) ARFLAGS=$(ARFLAGS) CC=$(CC) CFLAGS=$(CFLAGS) MAKE=$(MAKE)

# Traverse a subtree with make.
GODOWN=$(MAKE) $(MAKEARGS) "SRCPATH=$(SRCPATH)/$*" "MAKEARGS=$(MAKEARGS)"

# Primary target.
all:	copyright.dir crt.dir $(DIRS)
	: libc done.

# Primary target for NDP library.
ndp:	copyright.dir crtndp.dir $(DIRS)
	: NDP libc done.

# Directory pseudotargets.
copyright.dir:
	$(GODOWN)

crt.dir:
	$(GODOWN)

crtndp.dir:
	$(MAKE) $(MAKEARGS) "SRCPATH=$(SRCPATH)/crt" "MAKEARGS=$(MAKEARGS)" ndp

ctype.dir:
	$(GODOWN)

ndir.dir:
	$(GODOWN)

string.dir:
	$(GODOWN)

sys.dir:
	$(GODOWN)

stdlib.dir:
	$(GODOWN)

time.dir:
	$(GODOWN)

gen.dir:
	$(GODOWN)

stdio.dir:
	$(GODOWN)

# Secondary targets.
clean:
	rm -f $(TARGET)

# end of /usr/src/libc/Makefile
