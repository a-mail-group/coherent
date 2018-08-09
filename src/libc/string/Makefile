# /usr/src/libc/string/Makefile 10/7/94

LIBOBJ=\
	$(TARGET)(bcmp.o) \
	$(TARGET)(bcopy.o) \
	$(TARGET)(bzero.o) \
	$(TARGET)(strcoll.o) \
	$(TARGET)(strdup.o) \
	$(TARGET)(strerror.o) \
	$(TARGET)(strtok.o) \
	$(TARGET)(strxfrm.o)

# These C sources are unused, the assembler sources in $(MCH)
# are used instead for efficiency.
UNUSED=\
	_memxchg.c\
	memchr.c\
	memcmp.c\
	memcpy.c\
	memmove.c\
	memset.c\
	strcat.c\
	strchr.c\
	strcmp.c\
	strcpy.c\
	strcspn.c\
	streq.c\
	strlen.c\
	strncat.c\
	strncmp.c\
	strncpy.c\
	strpbrk.c\
	strrchr.c\
	strspn.c\
	strstr.c

# Primary target.
all:	$(LIBOBJ) string$(MCH)
	: libc/string done

# Secondary target.
string$(MCH):
	$(MAKE) $(MAKEARGS) "SRCPATH=$(SRCPATH)/$(MCH)" "MAKEARGS=$(MAKEARGS)"

# end of /usr/src/libc/string/Makefile
