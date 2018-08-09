# mlp/Makefile 8/19/94
# Makefile for mlp spooler.
# 'make install' should be done as root.

# For install to /, use:
# DESTDIR=/
# For relative install, use:
DESTDIR=/tmp/MLP

DESTBACK=$(DESTDIR)/usr/spool/mlp/backend
DESTBIN=$(DESTDIR)/usr/bin

ALL=hpfilter lp lpsched lpstat

all: $(ALL)

# Install in directory rooted at DESTDIR, with correct permissions.
install: all
	strip $(ALL)
	-mkdir -r $(DESTBIN)
	-mkdir -r $(DESTBACK)
	chmog 755 root root $(DESTDIR)
	chmog 755 bin bin $(DESTDIR)/usr $(DESTBIN) $(DESTDIR)/usr/spool
	chmog 755 daemon daemon $(DESTDIR)/usr/spool/mlp $(DESTBACK)
	cp -d hpfilter $(DESTBACK)
	chmog 511 bin bin $(DESTBACK)/hpfilter
	cp -d lp lpsched lpstat $(DESTBIN)
	chmog 6511 daemon daemon $(DESTBIN)/lp $(DESTBIN)/lpsched $(DESTBIN)/lpstat
	ln -f $(DESTBIN)/lp $(DESTBIN)/lpshut
	ln -f $(DESTBIN)/lp $(DESTBIN)/reprint
	ln -f $(DESTBIN)/lpstat $(DESTBIN)/cancel
	ln -f $(DESTBIN)/lpstat $(DESTBIN)/chreq
	ln -f $(DESTBIN)/lpstat $(DESTBIN)/lpadmin
	ln -f $(DESTBIN)/lpstat $(DESTBIN)/route

clean:
	rm -f *.o
	rm -f $(ALL)

hpfilter:	hpfilter.o
	$(CC) $(LDFLAGS) -o $@ $?

lp:		lp.o spooler.o
	$(CC) $(LDFLAGS) -o $@ lp.o spooler.o -lterm

lpsched:	lpsched.o spooler.o
	$(CC) $(LDFLAGS) -o $@ lpsched.o spooler.o -lterm

lpstat:		lpstat.o spooler.o
	$(CC) $(LDFLAGS) -o $@ lpstat.o spooler.o -lterm

hpfilter.o:	hpfilter.c
lp.o:		mlp.h lp.c
lpsched.o:	mlp.h lpsched.c
lpstat.o:	mlp.h lpstat.c
spooler.o:	mlp.h spooler.c

# end of mlp/Makefile
