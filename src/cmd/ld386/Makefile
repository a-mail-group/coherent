USRINC=/usr/include
GCC=gcc
GCCFLAGS=-O2 $(CFLAGS) -Wall -ansi -pedantic
GLDFLAGS=-nostdlib /lib/crts0.o -Xlinker '-R*' -Xlinker '-Im*'
GLIBFLAGS=/lib/libc.a

MAKEFILE=Makefile

SRCS=ld.c pass1.c message.c crc.c
OBJS=$(SRCS:.c=.o)
	
ld: $(OBJS)
	rm -f ld
	$(CC) $(LDFLAGS) -o ld $(OBJS) $(LIBFLAGS) >ld.nm


gcc:
	@+exec $(MAKE)	"CC=$(GCC)" "CFLAGS=$(GCCFLAGS)" \
			"LDFLAGS=$(GLDFLAGS)" "LIBFLAGS=$(GLIBFLAGS)" \
			-f $(MAKEFILE)

clean:
	rm -f *.o ld

depend:
	echo > makedep
	grep "^\#include" * \
		| sed 's/\(.*\)\.c:.*"\(.*\)".*/\1.o: \2/' \
		| sed '/\#include/d' >> makedep
	grep "^\#include" * \
		| sed 's/\(.*\)\.c:.*<\(.*\)>.*/\1.o: $$(USRINC)\/\2/' \
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

ld.o: ld.h
message.o: ld.h
pass1.o: ld.h
crc16.o: $(USRINC)/sys/compat.h
crc16.o: $(USRINC)/stddef.h
ld.o: $(USRINC)/stdlib.h
ld.o: $(USRINC)/signal.h
ld.o: $(USRINC)/ctype.h
ld.o: $(USRINC)/path.h
ld.o: $(USRINC)/string.h
ld.o: $(USRINC)/unistd.h
message.o: $(USRINC)/sys/compat.h
message.o: $(USRINC)/errno.h
message.o: $(USRINC)/string.h
message.o: $(USRINC)/unistd.h
pass1.o: $(USRINC)/string.h
pass1.o: $(USRINC)/stdlib.h

# DEPENDENCIES MUST END AT END OF FILE: IF YOU PUT STUFF HERE, IT WILL GO AWAY
# See make depend, above
