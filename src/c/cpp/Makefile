# c/cpp/Makefile 6/9/94

CPPOBJ=\
	cpp.a(cpp.$X)

# Primary target.
cpp:	cpp.a common.a
	$(CC) $(LDFLAGS) -o $@ cpp.a common.a

cpp.a:	$(CPPOBJ)
	: cpp.a done

# end of c/cpp/Makefile
