# make's Makefile for make, what else?

INSTALL_DIR=.
PREFIX=.
MANIFEST=$(PREFIX)/make
MAKEFILE=Makefile
USRINC=/usr/include
GCC=gcc
GCCFLAGS=-O2 -Wall -ansi -pedantic
GLDFLAGS=-nostdlib /lib/crts0.o
#GLIBFLAGS=-lc
GLIBFLAGS=/lib/libc.a

COMMANDS=$(INSTALL_DIR)/make

TARGETS=$(COMMANDS)

OBJS=

all: $(TARGETS)

gcc:
	+@exec make "CC=$(GCC)" "CFLAGS=$(GCCFLAGS)" \
		    "LDFLAGS=$(GLDFLAGS)" "LIBFLAGS=$(GLIBFLAGS)" \
		    -f $(MAKEFILE)

install setup:

clean:
	rm -f $(TARGETS)

manifest:
	@for i in *; do echo $(MANIFEST)/$$i; done

distrib:
	@for i in $(TARGETS); do echo $(PREFIX)/$${i\#$(INSTALL_DIR)/}; done

$(COMMANDS): $(OBJS)

depend:
	echo > makedep
	grep "^\#include" * \
		| sed 's/\(.*\)\.c:.*"\(.*\)".*/$$(INSTALL_DIR)\/\1: \2/' \
		| sed '/\#include/d' >> makedep
	grep "^\#include" * \
		| sed 's/\(.*\)\.c:.*<\(.*\)>.*/$$(INSTALL_DIR)\/\1: $$(USRINC)\/\2/' \
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

$(INSTALL_DIR)/make: make.h
$(INSTALL_DIR)/make: $(USRINC)/stdio.h
$(INSTALL_DIR)/make: $(USRINC)/sys/stat.h
$(INSTALL_DIR)/make: $(USRINC)/sys/types.h
$(INSTALL_DIR)/make: $(USRINC)/arcoff.h
$(INSTALL_DIR)/make: $(USRINC)/fcntl.h
$(INSTALL_DIR)/make: $(USRINC)/ar.h
$(INSTALL_DIR)/make: $(USRINC)/canon.h
$(INSTALL_DIR)/make: $(USRINC)/path.h
$(INSTALL_DIR)/make: $(USRINC)/time.h
$(INSTALL_DIR)/make: $(USRINC)/sys/compat.h
$(INSTALL_DIR)/make: $(USRINC)/string.h
$(INSTALL_DIR)/make: $(USRINC)/stdlib.h
$(INSTALL_DIR)/make: $(USRINC)/unistd.h
$(INSTALL_DIR)/make: $(USRINC)/stdarg.h

# DEPENDENCIES MUST END AT END OF FILE: IF YOU PUT STUFF HERE, IT WILL GO AWAY
# See make depend, above
