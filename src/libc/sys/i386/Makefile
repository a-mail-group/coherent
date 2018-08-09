# /usr/src/libc/sys/i386/Makefile 6/9/94

# System call list.
SCLIST=scall.s5

# Objects built from sources in sys/i386.
LIBOBJ=\
	$(TARGET)(_execve.o) \
	$(TARGET)(brk.o) \
	$(TARGET)(cerror.o) \
	$(TARGET)(dup2.o) \
	$(TARGET)(fork.o) \
	$(TARGET)(getpgrp.o) \
	$(TARGET)(gtty.o) \
	$(TARGET)(msgemul.o) \
	$(TARGET)(pipe.o) \
	$(TARGET)(sememul.o) \
	$(TARGET)(shmemul.o) \
	$(TARGET)(sigaction.o) \
	$(TARGET)(sigjmp.o) \
	$(TARGET)(signal.o) \
	$(TARGET)(time.o) \
	$(TARGET)(uname.o) \
	$(TARGET)(ustat.o) \
	$(TARGET)(wait.o)

# Replaced by getpgrp.o.
UNUSED=\
	setpgrp.o

# Objects built from sources generated automatically by mkscall.
# All these objects get rebuilt when $(SCLIST) changes,
# only the first is listed in the $(AUTO) dependency list below.
# N.B. getdents.o is not in this list for now, libc.a uses ndir version instead.
# _exit.o used to be included but now is part of crts0.o instead.
AUTO=\
	$(TARGET)(_msgsys.o)
AUTOOBJ=\
	_msgsys.o \
	_pgrp.o \
	_semsys.o \
	_shmsys.o \
	_time.o \
	_utssys.o \
	access.o \
	acct.o \
	alarm.o \
	chdir.o \
	chmod.o \
	chown.o \
	chroot.o \
	chsize.o \
	close.o \
	creat.o \
	dup.o \
	fcntl.o \
	fpathconf.o \
	fstat.o \
	fstatfs.o \
	ftime.o \
	getegid.o \
	geteuid.o \
	getgid.o \
	getgroups.o \
	getmsg.o \
	getpid.o \
	getppid.o \
	getuid.o \
	ioctl.o \
	kill.o \
	link.o \
	lseek.o \
	mkdir.o \
	mknod.o \
	mount.o \
	nap.o \
	nice.o \
	open.o \
	pathconf.o \
	pause.o \
	plock.o \
	poll.o \
	profil.o \
	ptrace.o \
	putmsg.o \
	read.o \
	rename.o \
	rmdir.o \
	setgid.o \
	setgroups.o \
	setuid.o \
	sigpending.o \
	sigprocmask.o \
	sigsuspend.o \
	stat.o \
	statfs.o \
	stime.o \
	sync.o \
	sysconf.o \
	sysi86.o \
	times.o \
	uadmin.o \
	ulimit.o \
	umask.o \
	umount.o \
	unlink.o \
	utime.o \
	write.o

# Primary target.
all:	$(AUTO) $(LIBOBJ)
	: libc/sys/i386 done

# Build system call sources and $(AUTOOBJ) objects automatically from scall.s5.
$(AUTO): $(SCLIST)
	-mkdir tmp; \
		cd tmp; \
		make mkscall; \
		mkscall -f "$(SRCPATH)/$(SCLIST)" || exit 1; \
		$(MAKE) $(MAKEARGS) $(AUTOOBJ) "SRCPATH=$(SRCPATH)" "MAKEARGS=$(MAKEARGS)" || exit 1; \
		mv $(AUTOOBJ) ..; cd ..
	$(AR) $(ARFLAGS) $(TARGET) $(AUTOOBJ)
	rm -f $(AUTOOBJ)
	rm -r tmp

# Build mkscall, which builds system call sources automatically.
mkscall:	mkscall.c
	$(CC) -o $@ $?

# end of /usr/src/libc/sys/i386/Makefile
