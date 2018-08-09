# libm/i387/Makefile 6/10/94
# To make the NDP version of the library library (/lib/ndp/libm.a):
#	export SRCPATH=/usr/src/libm/i387:/usr/src/libm
#	make

ARFLAGS=rckus
ASFLAGS=-x
CFLAGS=-VREADONLY -VNDP

OBJ=\
	libm.a(acos87.o) \
	libm.a(asin87.o) \
	libm.a(atan287.o) \
	libm.a(atan87.o) \
	libm.a(ceil87.o) \
	libm.a(cos87.o) \
	libm.a(cosh87.o) \
	libm.a(error87.o) \
	libm.a(exp87.o) \
	libm.a(fabs87.o) \
	libm.a(fmod.o) \
	libm.a(hypot87.o) \
	libm.a(j0.o) \
	libm.a(j1.o) \
	libm.a(jn.o) \
	libm.a(log87.o) \
	libm.a(pol.o) \
	libm.a(pow87.o) \
	libm.a(sin87.o) \
	libm.a(sqrt87.o) \
	libm.a(tan87.o) \
	libm.a(tanh87.o)

libm.a:	$(OBJ)
	: libm.a done

# end of /usr/src/libm/i387/Makefile
