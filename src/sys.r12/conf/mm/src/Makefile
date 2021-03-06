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

TARGETS=$(DRIVER)

OBJS= $(DRIVER)(mm.o) $(DRIVER)(mmas.o) $(DRIVER)(vtmm.o) \
	$(DRIVER)(vtmmas.o) 

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

$(DRIVER)(mm.o): $(USRINC)/kernel/timeout.h
$(DRIVER)(mm.o): $(USRINC)/sys/coherent.h
$(DRIVER)(mm.o): $(USRINC)/sys/sched.h
$(DRIVER)(mm.o): $(USRINC)/sys/errno.h
$(DRIVER)(mm.o): $(USRINC)/sys/stat.h
$(DRIVER)(mm.o): $(USRINC)/sys/io.h
$(DRIVER)(mm.o): $(USRINC)/sys/tty.h
$(DRIVER)(vtmm.o): $(USRINC)/sys/errno.h
$(DRIVER)(vtmm.o): $(USRINC)/sys/stat.h
$(DRIVER)(vtmm.o): $(USRINC)/kernel/timeout.h
$(DRIVER)(vtmm.o): $(USRINC)/sys/coherent.h
$(DRIVER)(vtmm.o): $(USRINC)/sys/uproc.h
$(DRIVER)(vtmm.o): $(USRINC)/sys/sched.h
$(DRIVER)(vtmm.o): $(USRINC)/sys/io.h
$(DRIVER)(vtmm.o): $(USRINC)/sys/tty.h
$(DRIVER)(vtmm.o): $(USRINC)/sys/kb.h
$(DRIVER)(vtmm.o): $(USRINC)/sys/vt.h

# DEPENDENCIES MUST END AT END OF FILE: IF YOU PUT STUFF HERE, IT WILL GO AWAY
# See make depend, above
