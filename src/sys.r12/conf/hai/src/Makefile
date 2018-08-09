# Standard 'conf'-system makefile with productions for standardized targets;
# 'make install', 'make clean', 'make manifest', 'make setup', 'make distrib'
# See the top-level 'conf' system Makefile for more details.

INSTALL_DIR=..
PREFIX=.
MANIFEST=$(PREFIX)/src
MAKEFILE=Makefile
USRINC=/usr/include
GCC=gcc
GCCFLAGS=-O2 $(CFLAGS) -Wall -ansi -pedantic
GLDFLAGS=-nostdlib /lib/crts0.o
GLIBFLAGS=/lib/libc.a

DRIVER=$(INSTALL_DIR)/Driver.a

TARGETS=$(INSTALL_DIR)/Space.c $(INSTALL_DIR)/mkdev $(DRIVER)

OBJS= $(DRIVER)(hai154x.o) $(DRIVER)(haict.o) $(DRIVER)(haiscsi.o) \
	$(DRIVER)(haisd.o) 

all: $(TARGETS)

gcc:
	+@exec make "CC=$(GCC)" "CFLAGS=$(GCCFLAGS)" \
		    "LDFLAGS=$(GLDFLAGS)" "LIBFLAGS=$(GLIBFLAGS)" \
		    -f $(MAKEFILE)

install setup:

clean:
	rm -f $(TARGETS)

manifest:
	@for i in *; do \
		case $$i in \
		RCS)	;; \
		*)	echo $(MANIFEST)/$$i \
			;; \
		esac \
	done

distrib:
	@for i in $(TARGETS); do echo $(PREFIX)/$${i\#$(INSTALL_DIR)/}; done

$(DRIVER): $(OBJS)

# The following is temporary, to be replaced when gcc timing issues are fixed.

$(DRIVER)(hai154x.o):	hai154x.c
	/bin/cc -o $% -c hai154x.c
	$(AR) $(ARFLAGS) $@ $%
	rm -f $%

$(DRIVER)(haict.o):	haict.c
	/bin/cc -o $% -c haict.c
	$(AR) $(ARFLAGS) $@ $%
	rm -f $%

$(DRIVER)(haiscsi.o):	haiscsi.c
	/bin/cc -o $% -c haiscsi.c
	$(AR) $(ARFLAGS) $@ $%
	rm -f $%

$(DRIVER)(haisd.o):	haisd.c
	/bin/cc -o $% -c haisd.c
	$(AR) $(ARFLAGS) $@ $%
	rm -f $%

depend:
	echo > makedep
	grep "^\#include" * \
		| sed 's/\(.*\)\.c:.*"\(.*\)".*/$$(DRIVER)(\1.o): \2/' \
		| sed '/\#include/d' >> makedep
	grep "^\#include" * \
		| sed 's/\(.*\)\.c:.*<\(.*\)>.*/$$(DRIVER)(\1.o): $$(USRINC)\/\2/' \
		| sed '/\#include/d' >> makedep
	grep "^\#include" * \
		| sed 's/\(.*\)\.spc:.*<\(.*\)>.*/$$(INSTALL_DIR)\/\1.c: $$(USRINC)\/\2/' \
		| sed '/\#include/d' >> makedep
	echo '/^\# DO NOT DELETE THIS LINE/+1,$$d' > eddep
	echo '$$r makedep' >> eddep
	echo 'w' >> eddep
	ed $(MAKEFILE) < eddep
	rm makedep eddep
	echo >> $(MAKEFILE)
	echo "\# DEPENDENCIES MUST END AT END OF FILE:" \
	     "IF YOU PUT STUFF HERE, IT WILL GO AWAY" >> $(MAKEFILE)
	echo "\# See make depend, above" >> $(MAKEFILE)

# DO NOT DELETE THIS LINE - make depend needs it

$(DRIVER)(hai154x.o): $(USRINC)/stddef.h
$(DRIVER)(hai154x.o): $(USRINC)/sys/coherent.h
$(DRIVER)(hai154x.o): $(USRINC)/sys/con.h
$(DRIVER)(hai154x.o): $(USRINC)/sys/devices.h
$(DRIVER)(hai154x.o): $(USRINC)/sys/types.h
$(DRIVER)(hai154x.o): $(USRINC)/sys/hdioctl.h
$(DRIVER)(hai154x.o): $(USRINC)/sys/haiscsi.h
$(DRIVER)(haict.o): $(USRINC)/stddef.h
$(DRIVER)(haict.o): $(USRINC)/sys/coherent.h
$(DRIVER)(haict.o): $(USRINC)/sys/buf.h
$(DRIVER)(haict.o): $(USRINC)/sys/inode.h
$(DRIVER)(haict.o): $(USRINC)/sys/stat.h
$(DRIVER)(haict.o): $(USRINC)/sys/sched.h
$(DRIVER)(haict.o): $(USRINC)/errno.h
$(DRIVER)(haict.o): $(USRINC)/sys/mtioctl.h
$(DRIVER)(haict.o): $(USRINC)/sys/file.h
$(DRIVER)(haict.o): $(USRINC)/sys/haiscsi.h
$(DRIVER)(haict.o): $(USRINC)/sys/haiioctl.h
$(DRIVER)(haiscsi.o): $(USRINC)/stddef.h
$(DRIVER)(haiscsi.o): $(USRINC)/sys/coherent.h
$(DRIVER)(haiscsi.o): $(USRINC)/sys/con.h
$(DRIVER)(haiscsi.o): $(USRINC)/sys/buf.h
$(DRIVER)(haiscsi.o): $(USRINC)/sys/io.h
$(DRIVER)(haiscsi.o): $(USRINC)/sys/sched.h
$(DRIVER)(haiscsi.o): $(USRINC)/sys/stat.h
$(DRIVER)(haiscsi.o): $(USRINC)/sys/uproc.h
$(DRIVER)(haiscsi.o): $(USRINC)/errno.h
$(DRIVER)(haiscsi.o): $(USRINC)/sys/haiscsi.h
$(DRIVER)(haiscsi.o): $(USRINC)/sys/haiioctl.h
$(DRIVER)(haisd.o): $(USRINC)/stddef.h
$(DRIVER)(haisd.o): $(USRINC)/sys/fdisk.h
$(DRIVER)(haisd.o): $(USRINC)/sys/coherent.h
$(DRIVER)(haisd.o): $(USRINC)/sys/buf.h
$(DRIVER)(haisd.o): $(USRINC)/sys/inode.h
$(DRIVER)(haisd.o): $(USRINC)/sys/stat.h
$(DRIVER)(haisd.o): $(USRINC)/sys/sched.h
$(DRIVER)(haisd.o): $(USRINC)/sys/sdioctl.h
$(DRIVER)(haisd.o): $(USRINC)/sys/hdioctl.h
$(DRIVER)(haisd.o): $(USRINC)/errno.h
$(DRIVER)(haisd.o): $(USRINC)/sys/haiscsi.h
$(INSTALL_DIR)/Space.c: $(USRINC)/sys/haiscsi.h

# DEPENDENCIES MUST END AT END OF FILE: IF YOU PUT STUFF HERE, IT WILL GO AWAY
# See make depend, above
