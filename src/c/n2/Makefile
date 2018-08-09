# c/n2/Makefile 6/9/94
# Expects $(OUTPUT) to define desired output writer.

CC2=\
	cc2.a(cc2.$X) \
	cc2.a(cc2sym.$X) \
	cc2.a(dbgt2.$X) \
	cc2.a(emit0.$X) \
	cc2.a(getfun.$X) \
	cc2.a(optim.$X)

CC2MCH=\
	cc2.a(afield.$X) \
	cc2.a(asm.$X) \
	cc2.a(emit1.$X) \
	cc2.a(getcod.$X) \
	cc2.a(optab.$X) \
	cc2.a(peep.$X) \
	cc2.a(unasm.$X)

CC2OUT=\
	cc2.a($(OUTPUT))

# Primary target.
cc2:	cc2.a common.a
	$(CC) $(LDFLAGS) -o $@ cc2.a common.a

# cc2 library.
cc2.a:	$(CC2) $(CC2MCH) $(CC2OUT)
	: cc2.a done

# c/n2/Makefile
