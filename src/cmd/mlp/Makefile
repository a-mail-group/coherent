# mlp/Makefile 10/6/94
# Makefile for mlp spooler.
# 'make install' should be done as root.
# hpfilter is no longer shipped as part of COH mlp.

# For install to /, use:
# DESTDIR=/
# For relative install, use:
DESTDIR=/tmp/MLP

DESTBIN=$(DESTDIR)/usr/bin

ALL=lp lpsched lpstat

all: $(ALL)

# Install in directory rooted at DESTDIR, with correct permissions.
install: all
	strip $(ALL)
	-mkdir -r $(DESTBIN)
	chmog 755 root root $(DESTDIR)
	chmog 755 bin bin $(DESTDIR)/usr $(DESTBIN)
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
	$(CC) $(LDFLAGS) -o $@ lpsched.o spooler.o

lpstat:		lpstat.o spooler.o
	$(CC) $(LDFLAGS) -o $@ lpstat.o spooler.o

hpfilter.o:	hpfilter.c
lp.o:		mlp.h lp.c
lpsched.o:	mlp.h lpsched.c
lpstat.o:	mlp.h lpstat.c
spooler.o:	mlp.h spooler.c

# end of mlp/Makefile
