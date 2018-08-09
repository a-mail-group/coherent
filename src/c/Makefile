# /usr/src/c/Makefile 6/10/94

# Assumes $(SRC) defines root libc source directory, e.g. /usr/src/c.
# To build new C compiler, move to an empty object directory and type:
#	export SRC=/usr/src/c		(define root of cc source tree)
#	. $SRC/.prof.386		(set shell variables for i386 make)
#	make -e				(build compiler)
# or
#	make -e mcc			(build monolithic compiler)
# Takes about 4 minutes from scratch on 33 MHz i486.

# Primary target.
# Build compiler with phases and disk tempfiles.
# Build unstripped executables cc, cpp, cc[0123] linked to directory bin.
all:
	make disk phases tabgen
	make -e SRCPATH=$(SRC)/common:$(SRC)/common/$(MCH)	|| exit 1
	make -e SRCPATH=$(SRC)/coh				|| exit 1
	make -e SRCPATH=$(SRC)/cpp				|| exit 1
	make -e SRCPATH=$(SRC)/n0:$(SRC)/n0/$(MCH)		|| exit 1
	make -e SRCPATH=$(SRC)/n1/$(MCH):$(SRC)/n1/$(MCH)/tables || exit 1
	make -e SRCPATH=$(SRC)/n1:$(SRC)/n1/$(MCH)		|| exit 1
	make -e SRCPATH=$(SRC)/n2:$(SRC)/n2/$(MCH)		|| exit 1
	make -e SRCPATH=$(SRC)/n3:$(SRC)/n3/$(MCH)		|| exit 1
	-mkdir bin
	ln -f cpp cc0 cc1 cc2 cc3 bin
	ln -f cc.new bin/cc
	: Done.

# Make monolithic cc with memory tempfiles and cpp stub.
# Running cpp requires /lib/cc0 from the non-monolithic compiler.
mcc:
	make monolithic memory tabgen
	make -e SRCPATH=$(SRC)/common:$(SRC)/common/$(MCH)	|| exit 1
	make -e SRCPATH=$(SRC)/cpp				|| exit 1
	make -e SRCPATH=$(SRC)/n0:$(SRC)/n0/$(MCH) cc0.a	|| exit 1
	make -e SRCPATH=$(SRC)/n1/$(MCH):$(SRC)/n1/$(MCH)/tables || exit 1
	make -e SRCPATH=$(SRC)/n1:$(SRC)/n1/$(MCH) cc1.a	|| exit 1
	make -e SRCPATH=$(SRC)/n2:$(SRC)/n2/$(MCH) cc2.a	|| exit 1
	make -e SRCPATH=$(SRC)/n3:$(SRC)/n3/$(MCH) cc3.a	|| exit 1
	make -e SRCPATH=$(SRC)/coh mcc.new
	-mkdir bin
	ln -f cpp bin
	ln -f mcc.new bin/cc
	: Done.

# Build debug (!TINY) compiler with phases and disk tempfiles.
# Build unstripped executables cc, cpp, cc[0123] linked to directory bin.
cc.db:
	make debug phases tabgen
	make -e SRCPATH=$(SRC)/common:$(SRC)/common/$(MCH)	|| exit 1
	make -e SRCPATH=$(SRC)/coh				|| exit 1
	make -e SRCPATH=$(SRC)/cpp				|| exit 1
	make -e SRCPATH=$(SRC)/n0:$(SRC)/n0/$(MCH)		|| exit 1
	make -e SRCPATH=$(SRC)/n1/$(MCH):$(SRC)/n1/$(MCH)/tables || exit 1
	make -e SRCPATH=$(SRC)/n1:$(SRC)/n1/$(MCH)		|| exit 1
	make -e SRCPATH=$(SRC)/n2:$(SRC)/n2/$(MCH)		|| exit 1
	make -e SRCPATH=$(SRC)/n3:$(SRC)/n3/$(MCH)		|| exit 1
	-mkdir bin
	ln -f cpp cc0 cc1 cc2 cc3 bin
	ln -f cc.new bin/cc
	: Done.

# Headers host.h and mch.h must be set up correctly for the generated compiler.
# host.h determines if the compiler is monolithic or in phases.
# mch.h determines if tempfiles are in memory or on disk
# and also allows the generation of !TINY compilers with debug output.
# The following targets use ed scripts to massage the headers accordingly.
phases:
	ed - $(SRC)/h/host.h <$(SRC)/host.p.eds
monolithic:
	ed - $(SRC)/h/host.h <$(SRC)/host.m.eds
memory:
	ed - $(SRC)/h/$(MCH)/mch.h <$(SRC)/mch.mem.eds
disk:
	ed - $(SRC)/h/$(MCH)/mch.h <$(SRC)/mch.dsk.eds
debug:
	ed - $(SRC)/h/$(MCH)/mch.h <$(SRC)/mch.db.eds

# Strip executables for non-monolithic compiler.
strip:
	strip bin/cc bin/cpp bin/cc0 bin/cc1 bin/cc2 bin/cc3

# Build tabgen.
tabgen:
	make -e SRCPATH=$(SRC)/coh tabgen

clean:
	rm -f *.a macros.c patern.c cc.new cc0 cc1 cc2 cc3 cpp mcc tabgen*

# end of /usr/src/c/Makefile
