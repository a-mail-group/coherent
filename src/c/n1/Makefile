# c/n1/Makefile 6/9/94
# This presumes that the code macros and patterns macros.c and patern.c
# have been made in the current directory by n1/$(MACHINE)/Makefile.

CC1=\
	cc1.a(cc1.$X) \
	cc1.a(cc1sym.$X) \
	cc1.a(code.$X) \
	cc1.a(mtree0.$X) \
	cc1.a(mtree1.$X) \
	cc1.a(mtree3.$X) \
	cc1.a(node.$X) \
	cc1.a(out.$X) \
	cc1.a(pool.$X) \
	cc1.a(reg0.$X) \
	cc1.a(sel0.$X) \
	cc1.a(sel1.$X) \
	cc1.a(snap0.$X) \
	cc1.a(tree0.$X) \
	cc1.a(tree1.$X)

CC1MCH=\
	cc1.a(altemp.$X) \
	cc1.a(amd.$X) \
	cc1.a(fixtop.$X) \
	cc1.a(gen1.$X) \
	cc1.a(gen2.$X) \
	cc1.a(mtree2.$X) \
	cc1.a(mtree4.$X) \
	cc1.a(outmch.$X) \
	cc1.a(reg1.$X) \
	cc1.a(sel2.$X) \
	cc1.a(snap1.$X) \
	cc1.a(table0.$X) \
	cc1.a(table1.$X)

CC1TABLES=\
	cc1.a(macros.$X) \
	cc1.a(patern.$X)

# Primary target.
cc1:	cc1.a common.a
	$(CC) $(LDFLAGS) -o cc1 cc1.a common.a

# cc1 library.
cc1.a:	$(CC1) $(CC1MCH) $(CC1TABLES)
	: cc1.a done

# end of c/n1/Makefile
