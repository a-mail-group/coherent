# mlp/Makefile 8/16/94
# Makefile for mlp spooler.
# 'make install' should be done as root if DESTBIN is not .

#DESTBIN=.
DESTBIN=/usr/bin
#DESTSPOOLBACK=.
DESTSPOOLBACK=/usr/spool/mlp/backend

ALL=lp lpstat lpsched hpfilter

all: $(ALL)

install: all
	cp -d hpfilter $(DESTSPOOLBACK)
	cp -d lpsched $(DESTBIN)
	cp -d lpstat $(DESTBIN)
	cp -d lp $(DESTBIN)
	ln -f $(DESTBIN)/lp $(DESTBIN)/lpshut
	ln -f $(DESTBIN)/lp $(DESTBIN)/reprint
	ln -f $(DESTBIN)/lpstat $(DESTBIN)/cancel
	ln -f $(DESTBIN)/lpstat $(DESTBIN)/chreq
	ln -f $(DESTBIN)/lpstat $(DESTBIN)/route
	ln -f $(DESTBIN)/lpstat $(DESTBIN)/lpadmin

clean:
	rm -f *.o
	rm -f $(ALL)

hpfilter:	hpfilter.o
	$(CC) $(LDFLAGS) -o $@ $?
	strip $@

lp:		lp.o spooler.o
	$(CC) $(LDFLAGS) -o $@ lp.o spooler.o -lterm
	strip $@

lpsched:	lpsched.o spooler.o
	$(CC) $(LDFLAGS) -o $@ lpsched.o spooler.o -lterm
	strip $@

lpstat:		lpstat.o spooler.o
	$(CC) $(LDFLAGS) -o $@ lpstat.o spooler.o -lterm
	strip $@
	chmod u+s $@

hpfilter.o:	hpfilter.c
lp.o:		mlp.h lp.c
lpsched.o:	mlp.h lpsched.c
lpstat.o:	mlp.h lpstat.c
spooler.o:	mlp.h spooler.c

# end of mlp/Makefile
