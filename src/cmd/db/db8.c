/*
 * db/db8.c
 * A debugger.
 * Child process.
 * All the machine-independent ptrace() code is here.
 */

#include <errno.h>
#include <signal.h>
#include <sys/ptrace.h>
#include "db.h"

/*
 * Read 'n' characters starting at address 'a' into the buffer 'bp'
 * from segment 'f' in the child process.
 * Return 1 on success, 0 on failure.
 */
int
getp(f, a, bp, n) int f; ADDR_T a; register char *bp; register int n;
{
	int d, n1, pcmd;

	/* Set pcmd to the appropriate ptrace() read command. */
	switch(f) {
	case ISEG:	pcmd = PTRACE_RD_TXT;		break;
	case DSEG:	pcmd = PTRACE_RD_DAT;		break;
	case USEG:	pcmd = PTRACE_RD_USR;		break;
	}

	/* Read in PTSIZE-sized chunks. */
	for (errno = 0; n != 0; n -= n1, a += n1, bp += n1) {
		n1 = (n > PTSIZE) ? PTSIZE : n;
		d = ptrace(pcmd, pid, (int)a, 0);
		if (errno)
			return 0;
		memcpy(bp, &d, n1);
	}
	return 1;
}

/*
 * Infanticide.
 */
void
killc()
{
	reg_flag = R_INVALID;
	if (execflag) {
		ptrace(PTRACE_TERM, pid, 0, 0);
		waitc();
		execflag = 0;
	}
#if	0
	trapstr = NULL;
#endif
}

/*
 * Write 'n' characters from the buffer 'bp'
 * to segment 'f' in the child process starting at address 'a'.
 * Return 1 on success, 0 on failure.
 */
int
putp(f, a, bp, n) int f; ADDR_T a; register char *bp; register int n;
{
	int d, n1, pcmd, prcmd;

	/* Set pcmd to the appropriate ptrace() write command. */
	switch(f) {
	case ISEG:	pcmd = PTRACE_WR_TXT;	break;
	case DSEG:	pcmd = PTRACE_WR_DAT;	break;
	case USEG:	pcmd = PTRACE_WR_USR;	break;
	}
	for (errno = 0; n != 0; n -= n1, a += n1, bp += n1) {
		if (n < PTSIZE) {
			/* Read first so write can leave extra bytes unchanged. */
			n1 = n;
			switch(f) {
			case ISEG:	prcmd = PTRACE_RD_TXT;	break;
			case DSEG:	prcmd = PTRACE_RD_DAT;	break;
			case USEG:	prcmd = PTRACE_RD_USR;	break;
			}
			d = ptrace(prcmd, pid, (int)a, 0);
		} else
			n1 = PTSIZE;
		memcpy(&d, bp, n1);
		ptrace(pcmd, pid, (int)a, d);
		if (errno) {
			perror("putp()");
			return 0;
		}
	}
	return 1;
}

/*
 * Run the child and wait for it to stop.
 * Return 0 on error.
 */
int
runc()
{
	register int pcmd;

	switch (step_mode) {
	case SNULL:
	case SWAIT:
		pcmd = PTRACE_RESUME;
		break;
	case SCALL:
	case SCONT:
	case SSTEP:
		pcmd = PTRACE_SSTEP;
		break;
	default:
		panic("invalid step_mode=%d", step_mode);
	}
	errno = 0;
	reg_flag = R_INVALID;
	ptrace(pcmd, pid, 1, 0);
	if (errno) {
		perror("ptrace");
		return 0;
	}
	if (waitc() == 0)
		return 0;
	get_regs(R_SOME);
	return 1;
}

/*
 * Start execution of the child.
 * 'argv' is the argument list,
 * 'ifn' is the name of the input file,
 * 'ofn' is the name of the output file, and
 * 'aflag' tells us whether the output file is opened for append or write.
 */
int
startc(argv, ifn, ofn, aflag) char **argv; char *ifn; char *ofn; int aflag;
{
	register int n;

	reg_flag = R_INVALID;
	if ((pid = fork()) < 0)
		return printr("Cannot fork");
	if (pid == 0) {
		/* Child process. */
		if (ifn != NULL) {
			if ((n=open(ifn, 0)) < 0)
				panic("Cannot open \"%s\"", ifn);
			dup2(n, 0);
			close(n);
		}
		if (ofn != NULL) {
			n = -1;
			if (aflag) {
				if ((n = open(ofn, 1)) >= 0)
					lseek(n, 0L, SEEK_END);
			}
			if (n < 0) {
				if ((n=creat(ofn, 0644)) < 0)
					panic("%s: cannot create", ofn);
			}
			dup2(n, 1);
			close(n);
		}
		ptrace(PTRACE_SETUP, 0, 0, 0);	/* I hear you, Dad... */
		execv(lfn, argv);
		exit(1);
	}
	/* Parent process. */
	if (waitc() == 0)
		return 0;

	/*
	 * Now the child is running, so map the child process rather
	 * than the disk file.
	 * FIX_ME The map addresses for DSEG and ISEG here are bogus.
	 */
	map_init();
	map_set(DSEG, MIN_ADDR, MAX_ADDR, (off_t)0, MAP_CHILD);
	map_set(ISEG, MIN_ADDR, MAX_ADDR, (off_t)0, MAP_CHILD);
	map_set(USEG, MIN_ADDR, (ADDR_T)PTRACE_UEND-1, (off_t)0, MAP_CHILD);
	execflag = 1;
	get_regs(R_SOME);
	return 1;
}

/*
 * Wait for the child to stop.
 */
int
waitc()
{
	register int p;
	int s;

	while ((p = wait(&s)) != pid) {
		if (p >= 0) {
			printr("Adopted a child %d", p);
			continue;
		}
		if (intflag == 0) {
			execflag = 0;
			return printr("Nonexistent child");
		}
		intflag = 0;
	}
	if ((s & 0xff) != 0x7f) {
		execflag = 0;
		return printr("Child process terminated (0x%X)", (s >> 8) & 0xFF );
	}
	return 1;
}

#if	DBPTRACE
#undef	ptrace
int
dbptrace(cmd, pid, loc, val) int cmd, pid, loc, val;
{
	register int n;

	printf("ptrace(%d, %d, 0x%X, 0x%X) = ", cmd, pid, loc, val);
	n = ptrace(cmd, pid, loc, val);
	printf("0x%X\n", n);
	return n;
}
#endif

/* end of db/db8.c */
