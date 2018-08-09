# c/n3/Makefile 6/9/94

CC3=\
	cc3.a(cc3.$X) \
	cc3.a(dbgt3.$X) \
	cc3.a(itree.$X)

CC3MCH=\
	cc3.a(icode.$X) \
	cc3.a(igen.$X)

# Primary target.
cc3:	cc3.a common.a
	$(CC) $(LDFLAGS) -o $@ cc3.a common.a

# cc3 library.
cc3.a:	$(CC3) $(CC3MCH)
	: cc3.a done

# end of c/n3/Makefile
