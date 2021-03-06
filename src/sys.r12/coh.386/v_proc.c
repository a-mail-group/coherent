#define	_DDI_DKI	1
#define	_DDI_DKI_IMPL	1
#define	_SYSV3		1

/*
 * This file contains the implementations of the abstract scheduling
 * interface functions defined in <sys/v_proc.h>.
 *
 * The functions defined here are used only by the code that needs to sleep
 * processes to implement the DDI/DKI sleep lock and synchronization variable
 * functionality. The code here has been broken out into a separate file and
 * a formal interface specified purely to separate the abstract and
 * implementation-dependent aspects of the DDI/DKI locking functions, ie to
 * stop the original code being infected with #ifdefs.
 *
 * This module runs in the _SYSV3 compilation mode to interface with the iBCS2
 * Coherent kernel.
 */

/*
 *-IMPORTS:
 *	<common/ccompat.h>
 *		__CONST__
 *		__USE_PROTO__
 *		__ARGS ()
 *	<common/xdebug.h>
 *		__LOCAL__
 *	<common/_siginfo.h>
 *		__siginfo_t
 *	<common/_tricks.h>
 *		__ARRAY_LENGTH ()
 *	<kernel/ddi_proc.h>
 *		ddi_proc_data ()
 *	<kernel/ddi_glob.h>
 *		ddi_global_data ()
 *	<kernel/ddi_lock.h>
 *		proc_global_priority
 *	<kernel/sig_lib.h>
 *		proc_send_signal ()
 *	<sys/debug.h>
 *		ASSERT ()
 *	<sys/types.h>
 *		plhi
 *	<sys/inline.h>
 *		splbase ()
 *	<sys/ksynch.h>
 *		LOCK ()
 *		UNLOCK ()
 *	<sys/signal.h>
 *		SIGTSTP
 */

#include <common/feature.h>
#include <common/ccompat.h>
#include <common/xdebug.h>
#include <common/_siginfo.h>
#include <common/_tricks.h>
#include <kernel/ddi_proc.h>
#include <kernel/ddi_glob.h>
#include <kernel/ddi_lock.h>
#include <kernel/sig_lib.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/types.h>
#include <sys/inline.h>
#include <sys/ksynch.h>
#include <sys/signal.h>
#include <stddef.h>

#include <kernel/v_proc.h>

/*
 * Type used in comparisons of process priority.
 */

typedef	unsigned short	pri_t;


#define	PROCESS_PNODE()		(& ddi_proc_data ()->dp_pnode)


/*
 * Here we deal with the implementation-specifics.
 */

#if	__COHERENT__

#define	_KERNEL		1

#include <sys/sched.h>
#include <sys/proc.h>

__sleep_t	x_sleep		__PROTO ((__VOID__ * _addr, int _pri,
					  int _how, char * where));
void		wakeup		__PROTO ((__VOID__ * _addr));

#define	DPDATA_TO_PROC(dpdata)	__DOWNCAST (PROC, p_ddi_space, dpdata)
#define	PNODE_TO_PROC(pnode)	DPDATA_TO_PROC (__DOWNCAST (dpdata_t, \
							    dp_pnode, pnode))

/*
 * Implementation-specific version of KERNEL_SLEEP ().
 */

#if	__USE_PROTO__
__LOCAL__ int (KERNEL_SLEEP) (plist_t * plistp, int priority, int flag)
#else
__LOCAL__ int
(KERNEL_SLEEP) __ARGS ((plistp, priority, flag))
plist_t	      *	plistp;
int		priority;
int		flag;
#endif
{
	pnode_t	      *	pnodep = PROCESS_PNODE ();
	int		state;

	ASSERT (ATOMIC_FETCH_PTR (pnodep->pn_plistp) == plistp);

	/*
	 * We expect to be running with interrupts disabled, so there's no
	 * reason not to just unlock the basic lock now and use the standard
	 * off-the-shelf Coherent kernel sleep function.
	 */

	for (;;) {
		PLIST_UNLOCK (plistp, plhi);

		(void) x_sleep (pnodep, priority,
				flag == SLEEP_INTERRUPTIBLE ? slpriSigCatch :
							      slpriNoSig,
				"DDI/DKI");

		/*
		 * The Coherent kernel won't dequeue us from the process list
		 * if it awakens us via a signal, so we use that fact to
		 * figure out why we are awake (sleep () does not return any
		 * useful value).
		 *
		 * To do anything about this in a multiprocessor-friendly
		 * fashion, we have to re-acquire the process list lock.
		 */

		(void) PLIST_LOCK (plistp, "KERNEL_SLEEP");

		if (ATOMIC_FETCH_PTR (pnodep->pn_plistp) == NULL) {
			state = PROCESS_NORMAL_WAKE;
			break;	/* dequeued, normal wakeup */
		}


		/*
		 * We were signalled; our caller may have requested non-
		 * interruptible sleep, so we go back to sleep on the caller's
		 * behalf (Coherent will always wake up a sleeper on receipt
		 * of a signal).
		 */

		if (flag == SLEEP_INTERRUPTIBLE) {
			/*
			 * We were woken by a signal and the caller wants to
			 * know about it, so dequeue us from the list and
			 * return the indication.
			 */

			plistp->pl_head = pnodep->pn_next;
			pnodep->pn_next->pn_prev = NULL;

			ATOMIC_CLEAR_PTR (pnodep->pn_plistp);

			state = PROCESS_SIGNALLED;
			break;
		}
	}

	/*
	 * Since we acquired a lock on the process list in order to safely
	 * test the reason why we awoke, release that lock now. Since we can,
	 * we'll set the interrupt priority down low.
	 */

	PLIST_UNLOCK (plistp, plbase);

	return state;
}


/*
 * Implementation-specific version of KERNEL_WAKE ().
 */

#if	__USE_PROTO__
__LOCAL__ void (KERNEL_WAKE) (pnode_t * pnodep)
#else
__LOCAL__ void
KERNEL_WAKE __ARGS ((pnodep))
pnode_t	      *	pnodep;
#endif
{
	ATOMIC_CLEAR_PTR (pnodep->pn_plistp);
	wakeup (pnodep);		/* don't defer this */
}


/*
 * Implementation-specific code to send a signal to a process.
 */

#if	__USE_PROTO__
__LOCAL__ int (KERNEL_SIGNAL) (dpdata_t * dpdatap, int sig)
#else
__LOCAL__ int
KERNEL_SIGNAL __ARGS ((dpdatap, sig))
dpdata_t      *	dpdatap;
int		sig;
#endif
{
	__siginfo_t	siginfo;

	/*
 	 * This is a bit of a crock until such time as we can change the
	 * Coherent process-table to support reference-counting, etc.
	 */

	siginfo.__si_signo = sig;
	siginfo.__si_code = 0;
	siginfo.__si_errno = 0;
	siginfo.__si_pid = 0;
	siginfo.__si_uid = 0;

	proc_send_signal (DPDATA_TO_PROC (dpdatap), & siginfo);
	return 0;
}


#else	/* if ! __COHERENT__ */


uarea_t		_dos_uarea_;
proc_t		_dos_proc_;

#define	PNODE_TO_PROC(pnodep)	__DOWNCAST (proc_t, p_nigel, \
				    __DOWNCAST (dpdata_t, dp_pnode, pnodep))

# include <sys/cmn_err.h>
# include <bios.h>


/*
 * Implementation-specific version of KERNEL_SLEEP ().
 */

#if	__USE_PROTO__
__LOCAL__ int (KERNEL_SLEEP) (plist_t * plistp, int priority, int flag)
#else
__LOCAL__ int
(KERNEL_SLEEP) __ARGS ((plistp, priority, flag))
plist_t	      *	plistp;
int		priority;
int		flag;
#endif
{
	int	state;


	CURRENT_PROCESS ()->p_state = PS_SLEEP;

	PLIST_UNLOCK (plistp, plhi);
	(void) splbase ();

	while ((state = CURRENT_PROCESS ()->p_state) != PS_RUN &&
	       state != PS_SIGNALLED)
		if (_bios_keybrd (_KEYBRD_READY))
			switch (_bios_keybrd (_KEYBRD_READ) & 0xFF) {

			case 0x1B:
				cmn_err (CE_PANIC, "Abort!");
				break;

			default:
				break;
			}

	return (state == PS_RUN ? PROCESS_NORMAL_WAKE : PROCESS_SIGNALLED);
}


/*
 * Implementation-specific version of KERNEL_WAKE ()
 */

#if	__USE_PROTO__
__LOCAL__ void (KERNEL_WAKE) (pnode_t * pnodep)
#else
__LOCAL__ void
KERNEL_WAKE __ARGS ((pnodep))
pnode_t	      *	pnodep;
#endif
{
	ASSERT (pnodep == PROCESS_PNODE ());

	if (CURRENT_PROCESS ()->p_state == PS_SLEEP ||
	    CURRENT_PROCESS ()->p_state == PS_SLEEP_NO_SIG)
		CURRENT_PROCESS ()->p_state = PS_RUN;
}


/*
 * Implementation-specific code to send a signal to a process.
 */

#if	__USE_PROTO__
__LOCAL__ int (KERNEL_SIGNAL) (dpdata_t * dpdatap, int sig)
#else
__LOCAL__ int
KERNEL_SIGNAL __ARGS ((dpdatap, sig))
dpdata_t      *	dpdatap;
int		sig;
#endif
{
	if (CURRENT_PROCESS ()->p_state == PS_SLEEP)
		CURRENT_PROCESS ()->p_state = PS_SIGNALLED;

        return 0;
}

#endif


/*
 * Table to define the mapping between abstract processor priority and the
 * priority levels used in the comparisons between priority levels.
 */

__LOCAL__ pri_t _pri_map [] = {
	0, 1, 2, 3, 4, 5, 6,		/* prilo */
	7, 8, 9, 10, 11, 12, 13,	/* pritape */
	14, 15, 16, 17, 18, 19, 20,	/* primed */
	21, 22, 23, 24, 25, 26, 27,	/* pritty */
	28, 29, 30, 31, 32, 33, 34,	/* pridisk */
	35, 36, 37, 38, 39, 40, 41,	/* prinet */
	42, 43, 44, 45, 46, 47, 48	/* prihi */
};



/*
 * This function is the abstract interface used by the DDI/DKI functions to
 * invoke kernel sleep.
 *
 * This function is charged with sleeping the current process (possibly such
 * that it may be interrupted by signals). It is passed a pointer to a locked
 * process list structure on which the current process is to be threaded if
 * it is actually going to sleep. After moving the process to a "slept"
 * state and saving the process context the process list is unlocked before
 * the main kernel dispatched is invoked.
 */

#if	__USE_PROTO__
__sleep_t (MAKE_SLEEPING) (plist_t * plistp, int priority, int flag)
#else
__sleep_t
MAKE_SLEEPING __ARGS ((plistp, priority, flag))
plist_t	      *	plistp;
int		priority;
int		flag;
#endif
{
	pnode_t	      *	pnodep = PROCESS_PNODE ();
	pnode_t	      *	pscan;
	pnode_t	      * pprev;
	unsigned	my_pri = _pri_map [priority];

	ASSERT (plistp != NULL);
	PLIST_ASSERT_LOCKED (plistp);

	ASSERT (priority >= 0 && priority < __ARRAY_LENGTH (_pri_map));
	ASSERT (flag == SLEEP_NO_SIGNALS || flag == SLEEP_INTERRUPTIBLE);

	/*
	 * Walk down the list of processes until one is found with a lower
	 * abstract priority than the process we are going to put to sleep.
	 */

	for (pscan = plistp->pl_head, pprev = NULL ; pscan != NULL ;
	     pscan = (pprev = pscan)->pn_next) {
		/*
		 * Don't forget to map the stored priority...
		 */

		if (_pri_map [pscan->pn_priority] < my_pri)
			break;
	}


	/*
	 * Now insert the current process between pprev and pscan.
	 */

	if (pprev == NULL)
		plistp->pl_head = pnodep;
	else
		pprev->pn_next = pnodep;

	if (pscan != NULL)
		pscan->pn_prev = pnodep;

	pnodep->pn_prev = pprev;
	pnodep->pn_next = pscan;


	/*
	 * We store the unmapped (abstract) priority since it should make more
	 * sense for a "ps"-like utility, and mapping it is cheap.
	 */

	pnodep->pn_priority = priority;
	pnodep->pn_flag = flag;
	ATOMIC_STORE_PTR (pnodep->pn_plistp, plistp);


	/*
	 * Now we are going to actually perform the low-level sleep.
	 *
	 * This thing gets to perform the kernel-specific bit of sleeping the
	 * process, possibly with signal checks and whatnot, unlocking the
	 * plist, and running the next process.
	 */

	return KERNEL_SLEEP (plistp, priority, flag);
}


/*
 * This function wakes one of the processes queued on a plist; since the
 * MAKE_SLEEPING () function queues them in priority order, let's wake up
 * the first...
 */

#if	__USE_PROTO__
__VOID__ * (WAKE_ONE) (plist_t * plistp)
#else
__VOID__ *
WAKE_ONE __ARGS ((plistp))
plist_t	      *	plistp;
#endif
{
	pnode_t	      *	pnodep;

	ASSERT (plistp != NULL);
	PLIST_ASSERT_LOCKED (plistp);

	if ((pnodep = plistp->pl_head) != NULL) {
		/*
		 * Given that we have something to awaken, dequeue it and
		 * make it runnable.
		 */

		if ((plistp->pl_head = pnodep->pn_next) != NULL)
			pnodep->pn_next->pn_prev = NULL;

		KERNEL_WAKE (pnodep);

		/*
		 * Return the identity of the person we gave the lock to.
		 */

		return pnodep;
	}

	/*
	 * Indicate that we found no-one to take over the mantle...
	 */

	return NULL;
}


/*
 * This function wakes all of the processes queued on a plist.
 */

#if	__USE_PROTO__
void (WAKE_ALL) (plist_t * plistp)
#else
void
WAKE_ALL __ARGS ((plistp))
plist_t	      *	plistp;
#endif
{
	pnode_t	      *	pnodep;
	pnode_t	      *	pnext;

	ASSERT (plistp != NULL);
	PLIST_ASSERT_LOCKED (plistp);

	for (pnodep = plistp->pl_head ; pnodep != NULL ; pnodep = pnext) {
		/*
		 * We take the value of the "pn_next" member now, because the
		 * MAKE_RUNNABLE () call has the freedom to alter any of the
		 * members of the pnode to deal with such things as
		 * signalling.
		 */

		pnext = pnodep->pn_next;

		KERNEL_WAKE (pnodep);
	}

	plistp->pl_head = NULL;
}


/*
 * This function returns a value suitable for use as a process identifier for
 * the DDI/DKI SLEEP_LOCKOWNED () function to identify the current context.
 */

#if	__USE_PROTO__
__VOID__ * (PROC_HANDLE) (void)
#else
__VOID__ *
PROC_HANDLE __ARGS (())
#endif
{
	return PROCESS_PNODE ();
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	proc_ref	Obtain a reference to a process for signalling.
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *
 *	void * proc_ref ();
 *
 *-DESCRIPTION:
 *	A non-STREAMS character driver can call proc_ref () to obtain a
 *	reference to the process in whose context it is running. The value
 *	returned can be used in subsequent calls to proc_signal () to post a
 *	signal to the process. The return value should not be used in any
 *	other way (ie, the driver should not attempt to interpret its
 *	meaning).
 *
 *-RETURN VALUE:
 *	An identifier that can be used in calls to proc_signal () and
 *	proc_unref ().
 *
 *-LEVEL:
 *	Base only.
 *
 *-NOTES:
 *	Processes can exit even though they are referenced by drivers. In this
 *	event, reuse of the identifier will be deferred until all driver
 *	references are given up.
 *
 *	There must be a matching call to proc_unref () for every call to
 *	proc_ref (), when the driver no longer needs to reference the process.
 *	This is typically done as part of close () processing.
 *
 *	Requires user context.
 *
 *	Does not sleep.
 *
 *	Driver-defined basic locks, read/write locks, and sleep locks may be
 *	held across calls to this function.
 *
 *-SEE ALSO:
 *	proc_signal (), proc_unref ()
 */

#if	__USE_PROTO__
__VOID__ * (proc_ref) (void)
#else
__VOID__ *
proc_ref __ARGS (())
#endif
{
	pl_t		prev_pl;
	dpdata_t      *	dpdatap;

	ASSERT_BASE_LEVEL ();


	/*
	 * This implementation is a really just a stub for testing. Later
	 * revisions of the code (especially in proc_unref () will deal with
	 * the synchronization and storage management issues properly.
	 *
	 * In addition, leaving the multiprocessor rewrite of the process
	 * system for another time will be good for ensuring that proper
	 * encapsulation boundaries are created and respected, and will ensure
	 * that the ideas prototypes here have been properly tested before
	 * they are incorporated into other kernel areas.
	 */

	dpdatap = ddi_proc_data ();

	prev_pl = LOCK (ddi_global_data ()->dg_proclock,
			proc_global_priority);

	dpdatap->dp_refcount ++;

	UNLOCK (ddi_global_data ()->dg_proclock, prev_pl);

	return dpdatap;
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-NAME:
 *	proc_signal	Send a signal to a process.
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *	#include <sys/signal.h>
 *
 *	int proc_signal (void * pref, int sig);
 *
 *-ARGUMENTS:
 *	pref		Identifier obtained by a previous call to proc_ref ().
 *
 *	sig		Signal number to be sent. Valid signal numbers to be
 *			sent are:
 *			    SIGHUP	The device has been disconnected.
 *			    SIGINT	The interrupt character has been
 *					received.
 *			    SIGQUIT	The quit character has been received.
 *			    SIGWINCH	The window size has changed.
 *			    SIGURG	Urgent data are available.
 *			    SIGPOLL	A pollable event has occurred.
 *
 *-DESCRIPTION:
 *	The proc_signal () function can be used to post a signal to the
 *	process represented by "pref". This will interrupt any process blocked
 *	in SV_WAIT_SIG () or SLEEP_LOCK_SIG () at the time the signal is
 *	posted, causing those functions to return prematurely in most cases.
 *	If the process has exited then this function has no effect.
 *
 *-RETURN VALUE:
 *	If the process still exists, 0 is returned. Otherwise, -1 is returned
 *	to indicate that the process no longer exists.
 *
 *-LEVEL:
 *	Base or interrupt.
 *
 *-NOTES:
 *	STREAMS drivers and modules should not use this mechanism for
 *	signalling processes. Instead, they can send M_SIG or M_PCSIG STREAMS
 *	messages to the stream head.
 *
 *	proc_signal () must not be used to send SIGTSTP to a process.
 *
 *	Does not sleep.
 *
 *	Driver-defined basic locks, read/write locks, and sleep locks may be
 *	held across calls to this function.
 *
 *-SEE ALSO:
 *	proc_ref (), proc_unref ()
 */

#if	__USE_PROTO__
int (proc_signal) (__VOID__ * pref, int sig)
#else
int
proc_signal __ARGS ((pref, sig))
__VOID__      *	pref;
int		sig;
#endif
{
	dpdata_t      *	dpdatap = (dpdata_t *) pref;

	ASSERT (pref != NULL);
	ASSERT (sig != SIGTSTP);

	return KERNEL_SIGNAL (dpdatap, sig);
}


/*
 *-STATUS:
 *	DDI/DKI
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *
 *	void proc_unref (void * pref);
 *
 *-ARGUMENTS:
 *	pref		Identifier obtained by a previous call to proc_ref ().
 *
 *-DESCRIPTION:
 *	The proc_unref () function can be used to release a reference to a
 *	process identified by the parameter "pref". There must be a matching
 *	call to proc_unref () for every previous call to proc_ref ().
 *
 *-RETURN VALUE:
 *	None.
 *
 *-LEVEL:
 *	Base or interrupt.
 *
 *-NOTE:
 *	Processes can exit even though they are referenced by drivers. In this
 *	event, reuse of "pref" will be deferred until all driver references
 *	are given up.
 *
 *	Does not sleep.
 *
 *	Driver-defined basic locks, read/write locks, and sleep locks may be
 *	held across calls to this function.
 *
 *-SEE ALSO:
 *	proc_ref (), proc_signal ()
 */

#if	__USE_PROTO__
void (proc_unref) (__VOID__ * pref)
#else
void
proc_unref __ARGS ((pref))
__VOID__      *	pref;
#endif
{
	pl_t		prev_pl;
	dpdata_t      *	dpdatap = (dpdata_t *) pref;

	ASSERT (pref != NULL);

	/*
	 * This implementation is a really just a stub for testing. Later
	 * revisions of the code will deal with the synchronization and
	 * storage management issues properly.
	 */

	prev_pl = LOCK (ddi_global_data ()->dg_proclock,
			proc_global_priority);

	dpdatap->dp_refcount --;

	UNLOCK (ddi_global_data ()->dg_proclock, prev_pl);
}


/*
 *-STATUS:
 *	Internal
 *
 *-SYNOPSIS:
 *	#include <sys/types.h>
 *
 *	void proc_kill_group (pid_t group, int sig);
 *
 *-ARGUMENTS:
 *	group		Process group ID to which the signal will be sent. All
 *			members of the indicated process group will be sent
 *			the signal.
 *
 *	sig		Signal number to be sent. Valid signal numbers are:
 *			    SIGHUP	The device has been disconnected.
 *			    SIGINT	The interrupt character has been
 *					received.
 *			    SIGQUIT	The quit character has been received.
 *			    SIGWINCH	The window size has changed.
 *			    SIGURG	Urgent data are available.
 *			    SIGPOLL	A pollable event has occurred.
 *
 *-DESCRIPTION:
 *	The proc_kill_group () function can be used to post a signal to a
 *	group of processes. This will interrupt any process blocked in
 *	SV_WAIT_SIG () or SLEEP_LOCK_SIG () at the time the signal is posted,
 *	causing those functions to return prematurely in most cases.
 *
 *-RETURN VALUE:
 *	None.
 *
 *-LEVEL:
 *	Base or interrupt.
 *
 *-NOTES:
 *	STREAMS drivers and modules should not use this mechanism for
 *	signalling processes. Instead, they can send M_SIG or M_PCSIG STREAMS
 *	messages to the stream head.
 *
 *	proc_kill_group () must not be used to send SIGTSTP to a process.
 *
 *	Does not sleep.
 *
 *	Driver-defined basic locks, read/write locks, and sleep locks may be
 *	held across calls to this function.
 *
 *-SEE ALSO:
 *	proc_ref (), proc_signal (), proc_unref ()
 */

#if	__USE_PROTO__
void (proc_kill_group) (pid_t group, int sig)
#else
void
proc_kill_group __ARGS ((group, sig))
pid_t		group;
int		sig;
#endif
{
	ASSERT ("proc_kill_group () : not implemented" == 0);
}
