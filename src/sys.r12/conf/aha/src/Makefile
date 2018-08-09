# Standard 'conf'-system makefile with productions for standardized targets;
# 'make install', 'make clean', 'make manifest', 'make setup', 'make distrib'
# See the top-level 'conf' system Makefile for more details.

INSTALL_DIR=..
OBJ_DIR=../obj
PREFIX=.
MANIFEST=$(PREFIX)/src
MAKEFILE=Makefile
USRINC=/usr/include
GCC=gcc
GCCFLAGS=-O2 $(CFLAGS) -Wall -ansi -pedantic
GLDFLAGS=-nostdlib /lib/crts0.o
GLIBFLAGS=/lib/libc.a

DRIVER=$(INSTALL_DIR)/Driver.o

OBJS= $(OBJ_DIR)/aha.o $(OBJ_DIR)/aha_dsl.o $(OBJ_DIR)/aha_host.o \
	$(OBJ_DIR)/mem_cache.o $(OBJ_DIR)/palloc.o

TARGETS=$(INSTALL_DIR)/Space.c $(INSTALL_DIR)/mkdev $(DRIVER)

all: $(TARGETS)

gcc:
	+@exec make "CC=$(GCC)" "CFLAGS=$(GCCFLAGS)" \
		    "LDFLAGS=$(GLDFLAGS)" "LIBFLAGS=$(GLIBFLAGS)" \
		    -f $(MAKEFILE)

install:

setup:
	[ -d ../obj ] || mkdir ../obj

clean:
	rm -f $(TARGETS) $(OBJS)

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
	ld -r -R"*" -I*con -o $@ $(OBJS)

depend:
	echo > makedep
	grep "^\#include" * \
		| sed 's/\(.*\)\.c:.*"\(.*\)".*/$$(OBJ_DIR)\/\1.o: \2/' \
		| sed '/\#include/d' >> makedep
	grep "^\#include" * \
		| sed 's/\(.*\)\.c:.*<\(.*\)>.*/$$(OBJ_DIR)\/\1.o: $$(USRINC)\/\2/' \
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

$(OBJ_DIR)/aha.o: $(USRINC)/sys/coherent.h
$(OBJ_DIR)/aha.o: $(USRINC)/sys/buf.h
$(OBJ_DIR)/aha.o: $(USRINC)/sys/sched.h
$(OBJ_DIR)/aha.o: $(USRINC)/sys/scsiwork.h
$(OBJ_DIR)/aha.o: $(USRINC)/sys/aha154x.h
$(OBJ_DIR)/aha.o: $(USRINC)/stddef.h
$(OBJ_DIR)/aha_dsl.o: $(USRINC)/sys/coherent.h
$(OBJ_DIR)/aha_dsl.o: $(USRINC)/sys/buf.h
$(OBJ_DIR)/aha_dsl.o: $(USRINC)/sys/scsiwork.h
$(OBJ_DIR)/aha_dsl.o: $(USRINC)/sys/aha154x.h
$(OBJ_DIR)/aha_dsl.o: $(USRINC)/sys/errno.h
$(OBJ_DIR)/aha_host.o: $(USRINC)/sys/debug.h
$(OBJ_DIR)/aha_host.o: $(USRINC)/sys/coherent.h
$(OBJ_DIR)/aha_host.o: $(USRINC)/kernel/trace.h
$(OBJ_DIR)/aha_host.o: $(USRINC)/sys/fdisk.h
$(OBJ_DIR)/aha_host.o: $(USRINC)/sys/hdioctl.h
$(OBJ_DIR)/aha_host.o: $(USRINC)/sys/sdioctl.h
$(OBJ_DIR)/aha_host.o: $(USRINC)/sys/buf.h
$(OBJ_DIR)/aha_host.o: $(USRINC)/sys/con.h
$(OBJ_DIR)/aha_host.o: $(USRINC)/sys/stat.h
$(OBJ_DIR)/aha_host.o: $(USRINC)/sys/uproc.h
$(OBJ_DIR)/aha_host.o: $(USRINC)/sys/errno.h
$(OBJ_DIR)/aha_host.o: $(USRINC)/sys/scsiwork.h
$(OBJ_DIR)/aha_host.o: $(USRINC)/kernel/typed.h
$(OBJ_DIR)/aha_host.o: $(USRINC)/sys/mmu.h
$(OBJ_DIR)/mem_cache.o: $(USRINC)/sys/coherent.h
$(OBJ_DIR)/mem_cache.o: $(USRINC)/kernel/trace.h
$(OBJ_DIR)/palloc.o: $(USRINC)/common/ccompat.h
$(OBJ_DIR)/palloc.o: $(USRINC)/sys/cmn_err.h
$(OBJ_DIR)/palloc.o: $(USRINC)/sys/debug.h
$(OBJ_DIR)/palloc.o: $(USRINC)/stddef.h
$(OBJ_DIR)/palloc.o: $(USRINC)/kernel/alloc.h
$(OBJ_DIR)/palloc.o: $(USRINC)/kernel/param.h
$(OBJ_DIR)/palloc.o: $(USRINC)/kernel/trace.h
$(OBJ_DIR)/palloc.o: $(USRINC)/sys/types.h

# DEPENDENCIES MUST END AT END OF FILE: IF YOU PUT STUFF HERE, IT WILL GO AWAY
# See make depend, above
