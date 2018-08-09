# /usr/src/libm/Makefile 6/10/94
# For the moment, this just makes the main directory.
# It should have separate targets for different machines and representations,
# notably i8087.
# To make:
#	export SRCPATH=/usr/src/libm
#	make

ARFLAGS=rckus
CFLAGS=-VREADONLY

OBJ=\
	libm.a(acos.o) \
	libm.a(asin.o) \
	libm.a(atan.o) \
	libm.a(atan2.o) \
	libm.a(cabs.o) \
	libm.a(ceil.o) \
	libm.a(cos.o) \
	libm.a(cosh.o) \
	libm.a(exp.o) \
	libm.a(fabs.o) \
	libm.a(floor.o) \
	libm.a(fmod.o) \
	libm.a(hypot.o) \
	libm.a(j0.o) \
	libm.a(j1.o) \
	libm.a(jn.o) \
	libm.a(log.o) \
	libm.a(log10.o) \
	libm.a(pol.o) \
	libm.a(pow.o) \
	libm.a(sin.o) \
	libm.a(sinh.o) \
	libm.a(sqrt.o) \
	libm.a(tan.o) \
	libm.a(tanh.o) \
	libm.a(two.o)

# Primary target.
libm.a:	$(OBJ)
	: libm.a done

clean:
	rm $(OBJ)

# end of /usr/src/libm/Makefile
