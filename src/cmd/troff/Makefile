# /usr/src/cmd/nroff/Makefile 4/26/95
# Makefile for nroff and troff

# CFLAGS for Linux requires copies of COH headers in $(SRC)/include.
CFLAGS=-I$(SRC)/include

# Targets.
ALL=\
	nroff\
	troff\
	fwtable

# Objects included in both nroff and troff images.
OBJ=\
	div.o\
	env.o\
	expr.o\
	extern.o\
	getf.o\
	hyphen.o\
	hyptab.o\
	main.o\
	misc.o\
	process.o\
	reg.o\
	req1.o\
	req2.o\
	request.o\
	table.o

# Objects in nroff only.
NOBJ=	tty.o

# Objects in troff only.
TOBJ=\
	fonts.o\
	output.o

# No extra objects unless Linux or Sparc.
EXTRA=
FWTEXTRA=
# Extra objects for Linux; no canon.o because same byte ordering.
# EXTRA=path.o
# FWTEXTRA=
# Extra objects for Sparc.
# EXTRA=_canon.o path.o
# FWTEXTRA=_canon.o

# Too many header files.
HDR=\
	$(SRC)/code.h\
	$(SRC)/codebug.h\
	$(SRC)/div.h\
	$(SRC)/env.h\
	$(SRC)/escapes.h\
	$(SRC)/fonts.h\
	$(SRC)/hyphen.h\
	$(SRC)/reg.h\
	$(SRC)/request.h\
	$(SRC)/roff.h\
	$(SRC)/str.h

# fwtable objects.
FWTOBJ=\
	fwtable.o\
	fwt_PS.o\
	fwt_TFM.o

# Primary target.
all:	$(ALL)
	: Done.

# Secondary targets.
troff:	$(OBJ) $(TOBJ) $(EXTRA)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(TOBJ) $(EXTRA)

nroff:	$(OBJ) $(NOBJ) $(EXTRA)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(NOBJ) $(EXTRA)

fwtable:	$(FWTOBJ) $(FWTEXTRA)
	$(CC) -o $@ $(FWTOBJ) $(FWTEXTRA) -f

strip:	$(ALL)
	strip $(ALL)

clean:
	rm -f $(OBJ) $(TOBJ) $(NOBJ) $(FWTOBJ)

# Unlinked objects.
div.o:		$(HDR) $(SRC)/div.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/div.c
env.o:		$(HDR) $(SRC)/env.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/env.c
expr.o:		$(HDR) $(SRC)/expr.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/expr.c
extern.o:	$(HDR) $(SRC)/extern.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/extern.c
fonts.o:	$(HDR) $(SRC)/fonts.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/fonts.c
getf.o:		$(HDR) $(SRC)/getf.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/getf.c
hyphen.o:	$(HDR) $(SRC)/hyphen.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/hyphen.c
hyptab.o:	$(HDR) $(SRC)/hyptab.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/hyptab.c
main.o:		$(HDR) $(SRC)/main.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/main.c
misc.o:		$(HDR) $(SRC)/misc.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/misc.c
output.o:	$(HDR) $(SRC)/output.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/output.c
process.o:	$(HDR) $(SRC)/process.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/process.c
reg.o:		$(HDR) $(SRC)/reg.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/reg.c
req1.o:		$(HDR) $(SRC)/req1.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/req1.c
req2.o:		$(HDR) $(SRC)/req2.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/req2.c
request.o:	$(HDR) $(SRC)/request.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/request.c
table.o:	$(HDR) $(SRC)/table.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/table.c
tty.o:		$(HDR) $(SRC)/tty.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/tty.c

fwtable.o:	$(SRC)/fwtable.c $(SRC)/fwtable.h
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/fwtable.c
fwt_PS.o:	$(SRC)/fwt_PS.c $(SRC)/fwtable.h
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/fwt_PS.c
fwt_TFM.o:	$(SRC)/fwt_TFM.c $(SRC)/fwtable.h
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/fwt_TFM.c

# Extra objects for other operating systems.
_canon.o:	$(SRC)/lib/_canon.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/lib/_canon.c
path.o:		$(SRC)/lib/path.c
	$(CC) -c $(CFLAGS) -o $@ $(SRC)/lib/path.c

# end of /usr/src/cmd/nroff/Makefile
