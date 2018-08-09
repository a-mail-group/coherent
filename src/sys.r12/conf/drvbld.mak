# Kernel makefile
# This makefile was automatically generated. Do not hand-modify
# Generated at Tue Jul 26 15:34:03 1994

# Template for kernel build makefile.

# Below, %C -> compile rules, %l -> link files, %b = "before" files,
# %B -> "before" rules, %a = "after" files, %A -> "after" rules,
# %r -> clean files and %% -> %.

PREFLAGS = 
POSTFLAGS = $(TARGET)

DRVOBJS = obj/conf.o \
	obj/em87.o /home/src/sys.r12/conf/em87/Driver.o \
	obj/streams.o obj/sem.o /home/src/sys.r12/conf/sem/Driver.o \
	obj/shm.o /home/src/sys.r12/conf/shm/Driver.o obj/msg.o \
	/home/src/sys.r12/conf/msg/Driver.o obj/fd.o \
	/home/src/sys.r12/conf/fd/Driver.o \
	/home/src/sys.r12/conf/ft/Stub.o obj/console.o \
	obj/cohmain.o /home/src/sys.r12/conf/ct/Driver.o \
	/home/src/sys.r12/conf/vtkb/Driver.o obj/lp.o \
	/home/src/sys.r12/conf/lp/Driver.o \
	/home/src/sys.r12/conf/fdc/Driver.o \
	/home/src/sys.r12/conf/asy/Driver.o \
	/home/src/sys.r12/conf/tr/Stub.o \
	/home/src/sys.r12/conf/rm/Driver.o obj/pty.o \
	/home/src/sys.r12/conf/pty/Driver.o obj/at.o \
	/home/src/sys.r12/conf/at/Driver.o 
DRVLIBS = /home/src/sys.r12/conf/streams/Driver.a \
	/home/src/sys.r12/conf/mm/Driver.a \
	/home/src/sys.r12/conf/tty/Driver.a 

all: pre $(TARGET) post

pre:
	for i in ; do [ -x $$i ] && $$i $(PREFLAGS); done

$(TARGET): $(K386LIB)/k0.o $(DRVOBJS) $(DRVLIBS) $(K386LIB)/k386.a \
	   $(FORCE_KERNEL)
	set -f; ld -K $$($(KEEP_FILE)) -o $(TARGET) -e stext $(K386LIB)/k0.o \
		$(DRVOBJS) $(DRVLIBS) $(K386LIB)/k386.a >$(TARGET).sym

post:
	for i in /home/src/sys.r12/conf/asy/after ; do [ -x $$i ] && $$i $(POSTFLAGS); done

obj/conf.o : conf.c conf.h
	$(CC) $(CFLAGS) -o obj/conf.o -c conf.c

conf.h: $(MTUNE_FILE) $(STUNE_FILE)
	echo confpath=$(CONFPATH)
	$(CONFPATH)/bin/devadm -x $(MTUNE_FILE) -y $(STUNE_FILE) -t
	rm -f obj/*

conf.c: $(MDEV_FILE) $(SDEV_FILE)
	@echo "This shouldn't happen, but if you see this message then it is"
	@echo "likely that some files have dates set in the future or some"
	@echo "similar configuration anomaly."
	exit 1

obj/em87.o : /home/src/sys.r12/conf/em87/Space.c
	$(CC) $(CFLAGS) -I/home/src/sys.r12/conf/em87 -I. -o obj/em87.o  -c /home/src/sys.r12/conf/em87/Space.c

obj/streams.o : /home/src/sys.r12/conf/streams/Space.c
	$(CC) $(CFLAGS) -I/home/src/sys.r12/conf/streams -I. -o obj/streams.o  -c /home/src/sys.r12/conf/streams/Space.c

obj/sem.o : /home/src/sys.r12/conf/sem/Space.c
	$(CC) $(CFLAGS) -I/home/src/sys.r12/conf/sem -I. -o obj/sem.o  -c /home/src/sys.r12/conf/sem/Space.c

obj/shm.o : /home/src/sys.r12/conf/shm/Space.c
	$(CC) $(CFLAGS) -I/home/src/sys.r12/conf/shm -I. -o obj/shm.o  -c /home/src/sys.r12/conf/shm/Space.c

obj/msg.o : /home/src/sys.r12/conf/msg/Space.c
	$(CC) $(CFLAGS) -I/home/src/sys.r12/conf/msg -I. -o obj/msg.o  -c /home/src/sys.r12/conf/msg/Space.c

obj/fd.o : /home/src/sys.r12/conf/fd/Space.c
	$(CC) $(CFLAGS) -I/home/src/sys.r12/conf/fd -I. -o obj/fd.o  -c /home/src/sys.r12/conf/fd/Space.c

obj/console.o : /home/src/sys.r12/conf/console/Space.c
	$(CC) $(CFLAGS) -I/home/src/sys.r12/conf/console -I. -o obj/console.o  -c /home/src/sys.r12/conf/console/Space.c

obj/cohmain.o : /home/src/sys.r12/conf/cohmain/Space.c
	$(CC) $(CFLAGS) -I/home/src/sys.r12/conf/cohmain -I. -o obj/cohmain.o  -c /home/src/sys.r12/conf/cohmain/Space.c

obj/lp.o : /home/src/sys.r12/conf/lp/Space.c
	$(CC) $(CFLAGS) -I/home/src/sys.r12/conf/lp -I. -o obj/lp.o  -c /home/src/sys.r12/conf/lp/Space.c

obj/pty.o : /home/src/sys.r12/conf/pty/Space.c
	$(CC) $(CFLAGS) -I/home/src/sys.r12/conf/pty -I. -o obj/pty.o  -c /home/src/sys.r12/conf/pty/Space.c

obj/at.o : /home/src/sys.r12/conf/at/Space.c
	$(CC) $(CFLAGS) -I/home/src/sys.r12/conf/at -I. -o obj/at.o  -c /home/src/sys.r12/conf/at/Space.c



clean:
	rm conf.c conf.h
	[ "obj/em87.o obj/streams.o obj/sem.o obj/shm.o obj/msg.o \
	obj/fd.o obj/console.o obj/cohmain.o obj/lp.o obj/pty.o \
	obj/at.o " ] && rm -f obj/em87.o obj/streams.o obj/sem.o obj/shm.o obj/msg.o \
	obj/fd.o obj/console.o obj/cohmain.o obj/lp.o obj/pty.o \
	obj/at.o 

# The following allows the macro FORCE_KERNEL to be used to force a relink
# of the target file; doing this allows driver developers to use their own
# style when it comes to kernel rebuilds. We use this in preference to some
# more brute-force technique such as removing an old kernel to allow people
# more flexibility with permissions.

in_case_we_are_not_forcing $(FORCE_KERNEL): this_does_not_exist

this_does_not_exist:

