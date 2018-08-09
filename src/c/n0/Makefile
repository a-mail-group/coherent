# c/n0/Makefile 6/9/94

CC0=\
	cc0.a(cc0.$X) \
	cc0.a(cc0key.$X) \
	cc0.a(cc0sym.$X) \
	cc0.a(cpp.$X) \
	cc0.a(dbgt0.$X) \
	cc0.a(ddecl.$X) \
	cc0.a(dope.$X) \
	cc0.a(double.$X) \
	cc0.a(etc.$X) \
	cc0.a(expand.$X) \
	cc0.a(expr.$X) \
	cc0.a(fold.$X) \
	cc0.a(gcandt.$X) \
	cc0.a(gdecl.$X) \
	cc0.a(get.$X) \
	cc0.a(init.$X) \
	cc0.a(lex.$X) \
	cc0.a(locals.$X) \
	cc0.a(sharp.$X) \
	cc0.a(size.$X) \
	cc0.a(stat.$X)
CC0MCH= \
	cc0.a(bind.$X)

# Primary target.
cc0:	cc0.a common.a
	$(CC) $(LDFLAGS) -o $@ cc0.a common.a

# cc0 library.
cc0.a:	$(CC0) $(CC0MCH)
	: cc0.a done

# end of c/n0/Makefile
