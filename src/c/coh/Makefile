# c/coh/Makefile 6/10/94

CCOBJ=\
	cc.a(cc.o)

# Monolithic compiler phases as archives.
MLIB=\
	cc.a \
	cc0.a \
	cc1.a \
	cc2.a \
	cc3.a

# All for coherent.
all:	cc.new
	: all done

# Native cc driver.
cc.new:	cc.a common.a
	$(CC) $(LDFLAGS) -o $@ cc.a common.a

# Monolithic cc.
mcc.new:	$(MLIB) common.a
	$(CC) $(LDFLAGS) -o $@ $(MLIB) common.a

# Compiler driver.
cc.a:	$(CCOBJ)
	: cc.a done

# i8086 cross compiler
i8086cc: cc.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o i8086cc -DI8086CC cc.c common.a

# m68000 cross compiler
m68000cc: cc.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o m68000cc -DM68000CC cc.c common.a

# z8001 cross compiler
z8001cc: cc.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o z8001cc -DZ8001CC cc.c common.a

# code table generator
tabgen: tabgen.o
	$(CC) $(LDFLAGS) -o tabgen tabgen.o

# intel 8086 omf disassembler
ojed:	ojed.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o ojed ojed.c

# intel 80286 omf disassembler
ojd286:	ojd286.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o ojd286 ojd286.c

# coherent new l.out disassembler
ndis:	ndis.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o ndis ndis.c common.a

# end of c/coh/Makefile
