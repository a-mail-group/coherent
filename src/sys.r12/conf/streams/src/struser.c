#define	_DDI_DKI	1
#define	_DDI_DKI_IMPL	1
#define	_SYSV4		1

/*
 */

#include <common/ccompat.h>
#include <kernel/ddi_lock.h>
#include <kernel/strmlib.h>
#include <sys/confinfo.h>
#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/poll.h>
#include <sys/ksynch.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/cmn_err.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/fd.h>
#include <sys/cred.h>
#include <stropts.h>
#include <string.h>
#include <poll.h>

/*
 * The following function (local to this module) is forward-referenced due to
 * the mutual recursion between the final close process and the streams unlink
 * code.
 */

__LOCAL__ int	SHEAD_DO_CLOSE	__PROTO ((shead_t * sheadp, int mode,
					  cred_t * credp));


lkinfo_t __stream_schedule_lkinfo = {
	"STREAMS queue schedule", INTERNAL_LOCK
};

lkinfo_t __stream_event_lkinfo = {
	"STREAMS bufcall ()/esbbcall () event list", INTERNAL_LOCK
};

__LOCAL__ lkinfo_t _stream_head_lkinfo = {
	"stream head lock", INTERNAL_LOCK
};


/*
 * Allocate and initialize a queue pair. This function only performs a partial
 * initialization; many other fields are filled in by the caller, usually from
 * fields supplied in the "streamtab" structure.
 *
 * Since at initial stream open time we we be allocating at least two queue
 * pairs and one or two stream head structures, we try to satisfy all those
 * allocations in one step here.
 *
 * The "npairs" argument indicates the number of queue pairs to be allocated.
 * The "extra" argument is the number of additional bytes to allocate over and
 * above the memory for the queue pairs.
 *
 * Call only from base level. This function may sleep.
 */

#if	__USE_PROTO__
__LOCAL__ queue_t * (QUEUE_ALLOC) (int npairs, size_t extra)
#else
__LOCAL__ queue_t *
QUEUE_ALLOC __ARGS ((npairs, extra))
int		npairs;
size_t		extra;
#endif
{
	queue_t	      *	q;
	queue_t	      *	init;
	int		count;

	ASSERT (npairs > 0 && npairs < 3);

	/*
	 * We use kmem_zalloc () to allocate this space so we can sleaze our
	 * way out of most of the initialization work.
	 */

	if ((q = (queue_t *) kmem_zalloc (2 * npairs * sizeof (* q) + extra,
					  KM_SLEEP)) == NULL)
		return NULL;

	init = q;
	count = npairs;

	do {
		/*
		 * First, initialise the read side of the queue. While we are
		 * at it, we link the "q_next" members of multiple queues for
		 * a read side; if we treat the value "q" that we are going to
		 * return to the caller as the stream head, the links run from
		 * the last entry we initialize towards the first one.
		 */

		if (count > 1)
			(init + 2)->q_next = init;

		init->q_flag = QWANTR | QREADR | QPROCSOFF;

		SFREEZE_INIT (init);

		init ++;


		/*
		 * Next, work on the write side. For multiple queues, the
		 * write size "q_next" links run from the "head" to the device.
		 */

		if (count > 1)
			init->q_next = init + 2;

		init->q_flag = QWANTR | QPROCSOFF;

		SFREEZE_INIT (init);

		init ++;
	} while (-- count > 0);


	/*
	 * We put a pointer to any "extra" space that the caller requested in
	 * the "q_ptr" fields of the first two queues allocated, since they
	 * will almost always be the stream head queue pair.
	 */

	if (extra > 0) {

		q->q_ptr = (char *) q + 2 * sizeof (* q) * npairs;
		W (q)->q_ptr = q->q_ptr;
	}

	return q;
}


/*
 * Set a queue's initial watermark data and some other stuff.
 */

#if	__USE_PROTO__
__LOCAL__ void (QUEUE_INITOPT) (queue_t * q)
#else
__LOCAL__ void
QUEUE_INITOPT __ARGS ((q))
queue_t	      *	q;
#endif
{
	struct module_info
		      *	mi = q->q_qinfo->qi_minfo;

	q->q_minpsz = mi->mi_minpsz;
	q->q_maxpsz = mi->mi_maxpsz;
	q->q_hiwat = mi->mi_hiwat;
	q->q_lowat = mi->mi_lowat;
}


/*
 * Set up a queue pair's initial options.
 */

typedef enum {
	QI_NORMAL,
	QI_MUX
} qiflag_t;

#if	__USE_PROTO__
__LOCAL__ void (QUEUE_INIT) (queue_t * q, struct streamtab * stab,
			     qiflag_t mux)
#else
void
QUEUE_INIT __ARGS ((q, stab, mux))
queue_t	      *	q;
struct streamtab
	      *	stab;
qiflag_t	mux;
#endif
{
	q->q_qinfo = mux == QI_NORMAL ? stab->st_rdinit : stab->st_muxrinit;
	QUEUE_INITOPT (q);

	q = W (q);

	q->q_qinfo = mux == QI_NORMAL ? stab->st_wrinit : stab->st_muxwinit;
	QUEUE_INITOPT (q);
}


/*
 * This function is the dual to the QBAND_ALLOC () function, freeing any
 * allocated "qband" structures associated with the given queue.
 *
 * The caller must have the queue frozen or not linked on any stream.
 */

#if	__USE_PROTO__
void (QBAND_FREE) (queue_t * q)
#else
void
QBAND_FREE __ARGS ((q))
queue_t	      *	q;
#endif
{
	qband_t	      *	scan;
	qband_t	      *	prev;
	int		nbands;

	QUEUE_TRACE (q, "QBAND_FREE");

	/*
	 * We use flags in the "qband" entries to locate allocation
	 * boundaries rather than trying to recover this information purely
	 * from comparing addresses (although since the address comparisons
	 * provide an extra check we do that too). In theory, an allocator
	 * might not need extra information stored in the arena yet might
	 * fail if adjacent allocations are coalesced into a single free ().
	 *
	 * I don't know of any allocators with this property, but one might
	 * exist.
	 *
	 * This code will work just fine under the allocation scheme which
	 * keeps the QBAND entries in a single vector, so we don't need to
	 * conditionalize this code at all.
	 */

	nbands = 0;

	for (prev = scan = q->q_bandp ; scan != NULL ; scan = scan->qb_next) {

		if ((scan->qb_flag & QB_FIRST) != 0) {

			ASSERT (nbands > 0);

			kmem_free (prev, sizeof (* prev) * nbands);

			nbands = 1;
			prev = scan;
		} else {

			ASSERT (scan == prev + nbands);
			nbands ++;
		}
	}

	if (nbands > 0)
		kmem_free (prev, sizeof (* prev) * nbands);
}


/*
 * Destroy an individual queue.
 */

#if	__USE_PROTO__
__LOCAL__ void (QUEUE_DESTROY) (queue_t * q)
#else
__LOCAL__ void
QUEUE_DESTROY __ARGS ((q))
queue_t	      *	q;
#endif
{
	mblk_t	      *	mp;
	mblk_t	      *	next;

	QSCHED_UNSCHEDULE (q, str_mem->sm_sched);

	/*
	 * Free all the memory allocated to messages that remain on the queue.
	 */

	for (mp = q->q_first ; mp != NULL ; mp = next) {

		next = mp->b_next;
		freemsg (mp);
	}

	if ((q->q_flag & QWANTW) != 0)
		QUEUE_BACKENAB (q);

	SFREEZE_DESTROY (q);

	if (q->q_nband > 0)
		QBAND_FREE (q);
}


/*
 * Destroy and release the memory for a queue pair or group of pairs. The
 * arguments passed to this function should match those used to allocate a
 * pair or group of pairs exactly. This can easily be done by recognising the
 * various canonical forms for stream structures; modules are always lone
 * queue pairs, regular streams match a driver and stream head (with
 * associated extra data for the stream head), and stream pipes consist of
 * two pairs of queue structures with two head structures.
 */

#if	__USE_PROTO__
__LOCAL__ void (QUEUE_FREE) (queue_t * rq, int npairs, size_t extra)
#else
__LOCAL__ void
QUEUE_FREE __ARGS ((rq, npairs, extra))
queue_t	      *	rq;
int		npairs;
size_t		extra;
#endif
{
	queue_t	      *	destroy;
	int		count;

	ASSERT (rq != NULL);
	ASSERT (npairs > 0 && npairs < 3);

	destroy = rq;
	count = npairs * 2;

	do {

		QUEUE_DESTROY (destroy);
		destroy ++;
	} while (-- count > 0);

	kmem_free (rq, 2 * sizeof (* rq) * npairs + extra);
}


/*
 * STREAM HEAD MANAGEMENT NOTES:
 *
 * The management of stream head structures introduces some interesting
 * synchronization problems arising from the interaction of the rules
 * associated with driver close routines and the fact that stream head
 * structures are dynamically allocated.
 *
 * The first problem is really one of specification; what does it mean for a
 * driver close () entry point to return EINTR? System V does not talk about
 * this case specifically, so we prohibit it by treating error returns from
 * the close () entry point uniformly by still actually closing the device.
 *
 * The second problem is this; a driver should not be re-opened until the
 * close process has completed. However, completion of the close process will
 * normally involve deallocation of the stream head (where presumably the
 * open routines are waiting).
 *
 * Our problem is that it is not possible to reliably determine whether there
 * are any other contexts waiting on a sleep lock. While it might be possible
 * to do so using SLEEP_LOCKAVAIL (), this would require that the calling
 * context release the lock. Under a plausible implementation of SLEEP_LOCK ()
 * where sleep locks are basically implemented with the sleep () and wakeup ()
 * functions and a "locked" flag, there will be no way for a process to find
 * out whether there are any functions waiting on the lock, since the unlock
 * implementation could simply clear the "locked" flag and issue wakeup (),
 * so that there could be any number of contexts waiting to run and test the
 * "locked" flag, yet SLEEP_LOCKAVAIL () in the calling context would return
 * true. Deallocating the lock at this time would be potentially disastrous.
 *
 * Actually, even a quality implementation of sleep locks (such as is provided
 * with this STREAMS system) cannot be easily used this way, since it is very
 * difficult to ensure that the information returned by SLEEP_LOCKAVAIL () is
 * current.
 *
 * The situation can be resolved by maintaining a count of processes wishing
 * to lock the item. The count can be maintained by using the basic-lock
 * action associated with DDI/DKI synchronization variables, and the new
 * SV_SIGNAL () operation can be used to pass the ownership of the lock to
 * a waiting process reliably.
 *
 * Once this is in place, it becomes clear how the count of waiting processes
 * can be used to simplify the destruction of stream heads; essentially,
 * when a process wishes to release the "lock" on the stream head, if both
 * the open count and the waiting count are 0, then the memory for the item
 * can be safely released. Otherwise, control simply passes to the next
 * waiting process.
 *
 * So, if a process is performing a final close on a stream, and some open
 * requests are queued, the close will leave the stream's memory alone and
 * simply pass it on to the waiting open (which can detect that the stream
 * needs to be treated as new since the open count is 0). If the waiting open
 * was interrupted by a signal, it would still have to decrement the count of
 * waiting processes before it releases its lock, at which time it would know
 * to remove the item from the directory and release the memory.
 *
 * For this to work properly takes some coordination in the policy for the
 * directory; the basic lock used to guard lock operations should be held
 * during searches of the directory to ensure that the count value is correct
 * with respect to all processes; a process that has a pointer to the stream
 * head (obtained from the directory) which it has not incremented the count
 * for is an error. Note that this only applies to operations which might
 * later affect the count, of course.
 *
 *
 * STREAM OPERATIONS AND LOCKS:
 *
 * open ()	This operation cannot begin while there is a final close in
 *		progress. If this operation increments the "open count" before
 *		calling the device open routines, it is possible that it will
 *		also have to perform final close duties if a driver or module
 *		fails the open. This function may cause the creation of a new
 *		queue pair and directory entry.
 *
 *		The multiprocessor DDI/DKI also mandates that a particular
 *		device number's open () routine only have one instance active
 *		at any given time.
 *
 * close ()	Normally, this does not require extended locking, but the case
 *		of beginning a final close is special, since only then will
 *		the queue drain and final close procedures begin. Since there
 *		cannot be outstanding ioctl ()s during final close, the timer
 *		code used to control ioctl ()s can be shared with this for
 *		timing out while waiting for a write queue to drain.
 *
 * read (), getmsg (), getpmsg ()
 *		Under normal circumstances, these functions require no special
 *		treatment. It would be desirable to support an extension to
 *		STREAMS which supported "safe" multiple readers, where the
 *		serialization of reads is guaranteed.
 *
 * write (), putmsg (), putpmsg ()
 *		These functions require little special treatment. It would be
 *		desirable to support an extension to STREAMS which guaranteed
 *		serialization of writes, for instance to guarantee unlimited-
 *		length atomic pipe writes.
 *
 * ioctl ()	Depending on the details of the operation, we may need
 *
 *			a read lock on the stream head (eg. I_GETCLTIME).
 *			a write lock on the stream head (eg. I_SRDOPT).
 *			a block on open and pop (I_PUSH).
 *			a block on close and push (I_POP).
 *			a long-term lock on the message queue.
 *
 *		The long-term lock operations revolve around the operations
 *		that send messages downstream : I_LINK, I_UNLINK, I_PLINK,
 *		I_PUNLINK, and I_STR. These operations are also special in
 *		that they are capable of timing out.
 *
 *		Since the close or open routines invoked by an I_PUSH or I_POP
 *		operation may block, they require analagous locking to the
 *		open () and close () cases.
 *
 * LOCK SUMMARY:
 *	EXCLUSIVE LONG-TERM LOCK WITH OPTIONAL TIMEOUT:
 *	    open/close category:
 *		open (), close (), I_PUSH, I_POP
 *	    ioctl category:
 *		I_LINK, I_UNLINK, I_PLINK, I_PUNLINK, I_STR
 *
 *	    In theory, a single lock will do. However, once we take into
 *	    account terminal behaviour w.r.t. CLOCAL and other similar
 *	    situations, it seems that creating the subcategories above will
 *	    suit us better.
 *
 *	    Final close is a special case that blocks all other cases, which
 *	    can be distinguished fairly clearly.
 *
 *	STREAM HEAD WRITE LOCK:
 *		I_SRDOPT, I_SETSIG, I_SWROPT, I_SETCLTIME
 *		Certain stream head message processing routines may also write
 *		lock the stream head, such as M_SETOPT processing.
 *
 *	All other streams operations should acquire a stream head read lock
 *	before reading stream head variables.
 */
/*
 * STREAM HEAD WAIT NOTES:
 *
 * In addition to the above discussion about locking, there are other
 * operations that may cause a process to block while at the stream head. For
 * instance, read (), write (), I_RECVFD, and I_STR operations may cause the
 * outer context to block until some kind of message arrives.
 *
 * The question we are immediately faced with is what level of specificity to
 * provide in the arrangement of synchronization variables and basic locks.
 * Until the implementation is complete and we can perform detailed
 * measurements with a variety of (pathological) loads on a variety of
 * systems, we really don't know. For simplicity, the current system performs
 * all stream head blocking by sleeping on the "sh_wait_sv" synchronization
 * variable that is also used by the above locking operations.
 *
 * However, to give some isolation from changes in this scheme, we mandate a
 * generic layer to deal with this. Not only does this insulate operations
 * from the details of synchronization, but it allows us to perform some
 * simple optimizations that may allow this simple scheme to perform better.
 */


/*
 * Initialize a stream head structure, assuming that memory was allocated with
 * kmem_zalloc () and so NULL pointers and 0-value fields need not be filled
 * in.
 *
 * This function may sleep waiting for memory to become available to allocate
 * the locks needed by the stream head.
 */

#if	__USE_PROTO__
__LOCAL__ void (SHEAD_INIT) (shead_t * sheadp, struct streamtab * stabp,
			     n_dev_t dev, queue_t * rq)
#else
__LOCAL__ void
SHEAD_INIT __ARGS ((sheadp, stabp, dev, rq))
shead_t	      *	sheadp;
struct streamtab
	      *	stabp;
n_dev_t		dev;
queue_t	      *	rq;
#endif
{
	ASSERT (sheadp != NULL);
	ASSERT (rq != NULL);

	/*
	 * The sh_lock_count and sh_time_count members are initialized in the
	 * lock code.
	 */

	ASSERT (sheadp->sh_open_count == 0);
	ASSERT (sheadp->sh_attach_count == 0);
	ASSERT (sheadp->sh_lock_count == 0);
	ASSERT (sheadp->sh_time_count == 0);
	ASSERT (sheadp->sh_rerrcode == 0);
	ASSERT (sheadp->sh_werrcode == 0);
	ASSERT (sheadp->sh_wroff == 0);

	ASSERT (sheadp->sh_read_bufcall == 0);
	ASSERT (sheadp->sh_timeout_id == 0);

	ASSERT (sheadp->sh_sigs == NULL);
	ASSERT (sheadp->sh_linked == NULL);
	ASSERT (sheadp->sh_ioc_msg == NULL);

	ASSERT (rq->q_ptr == sheadp);

	sheadp->sh_dev = dev;
	sheadp->sh_tab = stabp;
	sheadp->sh_head = rq;

	sheadp->sh_pollhead = phalloc (KM_SLEEP);
	sheadp->sh_flags = SH_MASTER;
	sheadp->sh_readopt = RNORM | RPROTNORM;

	sheadp->sh_basic_lockp = LOCK_ALLOC (stream_head_hierarchy, plstr,
					     & _stream_head_lkinfo, KM_SLEEP);
	sheadp->sh_wait_sv = SV_ALLOC (KM_SLEEP);

	ASSERT (sheadp->sh_basic_lockp != NULL || sheadp->sh_wait_sv != NULL);


	/*
	 * The default time to wait for a queue to drain while closing is 15s.
	 */

	sheadp->sh_cltime = drv_usectohz (15000000L);
}


/*
 * Turn a stream head structure back into raw bits.
 */

#if	__USE_PROTO__
__LOCAL__ void (SHEAD_DESTROY) (shead_t * sheadp)
#else
__LOCAL__ void
SHEAD_DESTROY __ARGS((sheadp))
shead_t	      *	sheadp;
#endif
{
	ASSERT (sheadp != NULL);
	ASSERT (sheadp->sh_sigs == NULL);
	ASSERT (sheadp->sh_linked == NULL);

	ASSERT (sheadp->sh_open_count == 0);
	ASSERT (sheadp->sh_attach_count == 0);
	ASSERT (sheadp->sh_lock_count == 0);

	ASSERT (sheadp->sh_timeout_id == 0);

	if (sheadp->sh_read_bufcall != 0)
		unbufcall (sheadp->sh_read_bufcall);

	LOCK_DEALLOC (sheadp->sh_basic_lockp);
	SV_DEALLOC (sheadp->sh_wait_sv);

	phfree (sheadp->sh_pollhead);
}


/*
 * This function attempts to determine the appropriate id queue for a stream
 * head based on cues in the stream head.
 */

#if	__USE_PROTO__
__LOCAL__ slist_id_t (SHEAD_ID) (shead_t * sheadp)
#else
__LOCAL__ slist_id_t
SHEAD_ID __ARGS ((sheadp))
shead_t	      *	sheadp;
#endif
{
	ASSERT (sheadp != NULL);

	if (SHEAD_IS_PIPE (sheadp))
		return PIPE_SLIST;

	return DEV_SLIST;
}


/*
 * This function finds a stream head by looking up its device number, but
 * nothing else. To call this function, the caller must have good reason to
 * suspect that an open reference to the stream exists and isn't going to go
 * away.
 *
 * If this routine returns NULL, then that is a serious error (which may be
 * diagnosed by console messages), because it indicates that the caller has
 * not got the claimed knowledge of the state of the system!
 */

#if	__USE_PROTO__
shead_t * (SHEAD_FIND) (n_dev_t dev, slist_id_t id)
#else
shead_t *
SHEAD_FIND __ARGS ((dev, id))
n_dev_t		dev;
slist_id_t	id;
#endif
{
	shead_t	      *	scan;
	pl_t		prev_pl;

	prev_pl = RW_RDLOCK (str_mem->sm_head_lock, plstr);

	for (scan = str_mem->sm_streams [id] ; scan != NULL ;
	     scan = scan->sh_next) {

		if (scan->sh_dev == dev)
			break;
	}

	RW_UNLOCK (str_mem->sm_head_lock, prev_pl);

	if (scan == NULL)
		cmn_err (CE_WARN, "Unable to locate stream in SHEAD_FIND ()");
	else
		ASSERT (SHEAD_ID (scan) == id);

	return scan;
}


/*
 * This function attempts to locate an existing entry and increment its lock
 * count atomically.
 */

#if	__USE_PROTO__
__LOCAL__ shead_t * (SHEAD_FIND_AND_LOCK) (n_dev_t dev, slist_id_t id)
#else
__LOCAL__ shead_t *
SHEAD_FIND_AND_LOCK __ARGS ((dev, id))
n_dev_t		dev;
slist_id_t	id;
#endif
{
	shead_t	      *	scan;
	pl_t		prev_pl;

	prev_pl = RW_RDLOCK (str_mem->sm_head_lock, plstr);

	for (scan = str_mem->sm_streams [id] ; scan != NULL ;
	     scan = scan->sh_next) {

		if (scan->sh_dev == dev) {
			/*
			 * Now we have found the entry we want, increment the
			 * reference count atomically. We know that it will
			 * not disappear because of the read lock we have on
			 * the containing list.
			 */

			(void) SHEAD_LOCK (scan);

			scan->sh_lock_count ++;
			break;
		}
	}

	RW_UNLOCK (str_mem->sm_head_lock, prev_pl);
	return scan;
}


/*
 * This function adds a stream head to the global list. If an entry with the
 * same ID is present on the list, this operation fails.
 *
 * The stream head should be locked against further opens at this point.
 *
 * The return value is 0 on success, -1 on error.
 */

#if	__USE_PROTO__
__LOCAL__ int (SHEAD_ADD) (shead_t * sheadp)
#else
__LOCAL__ int
SHEAD_ADD __ARGS ((sheadp))
shead_t	      *	sheadp;
#endif
{
	pl_t		prev_pl;
	shead_t	      *	scan;
	slist_id_t	id;

	id = SHEAD_ID (sheadp);

	ASSERT (sheadp->sh_ref_count == 1);
	ASSERT ((sheadp->sh_lock_mask & SH_OPENCLOSE) != 0);

	prev_pl = RW_WRLOCK (str_mem->sm_head_lock, plstr);

	for (scan = str_mem->sm_streams [id] ; scan != NULL ;
	     scan = scan->sh_next)  {

		if (scan->sh_dev == sheadp->sh_dev) {
			/*
			 * We have found a conflict. Unlock the list and
			 * return an error.
			 */

			RW_UNLOCK (str_mem->sm_head_lock, prev_pl);
			return -1;
		}
	}

	sheadp->sh_next = str_mem->sm_streams [id];
	str_mem->sm_streams [id] = sheadp;

	RW_UNLOCK (str_mem->sm_head_lock, prev_pl);
	return 0;
}


/*
 * This function changes the device number of a stream head for a clone open
 * situation. The "st_dev" field of the stream head has to be changed with
 * the stream head list lock held for writing to avoid confusing anyone who
 * is looking for the original device number.
 *
 * Furthermore, the rename can fail because the new number is already in use.
 * The return value is -1 on error, or 0 on success.
 */

#if	__USE_PROTO__
__LOCAL__ int (SHEAD_RENAME) (shead_t * sheadp, n_dev_t dev)
#else
__LOCAL__ int
SHEAD_RENAME __ARGS ((sheadp, dev))
shead_t	      *	sheadp;
n_dev_t		dev;
#endif
{
	pl_t		prev_pl;
	shead_t	      *	scan;
	int		ok = 0;		/* flag whether stream is on list */
	slist_id_t	id;

	id = SHEAD_ID (sheadp);

	prev_pl = RW_WRLOCK (str_mem->sm_head_lock, plstr);

	for (scan = str_mem->sm_streams [id] ; scan != NULL ;
	     scan = scan->sh_next) {

		if (scan->sh_dev == dev) {
			/*
			 * We have found a conflict. Unlock the list and
			 * return an error.
			 */

			RW_UNLOCK (str_mem->sm_head_lock, prev_pl);
			return -1;
		}

		if (scan == sheadp)
			ok ++;		/* Ok, we saw the item */
	}

	/*
	 * All OK, now change the name of the original stream head.
	 */

	sheadp->sh_dev = dev;
	RW_UNLOCK (str_mem->sm_head_lock, prev_pl);

	if (! ok)
		cmn_err (CE_WARN, "SHEAD_RENAME () of unlisted stream head");

	return 0;
}


/*
 * This function decrements the link count of the stream head; this may cause
 * the stream head to become unreferened, which means that the memory will be
 * reclaimed.
 */

#if	__USE_PROTO__
__LOCAL__ void (SHEAD_UNREFERENCE) (shead_t * sheadp)
#else
__LOCAL__ void
SHEAD_UNREFERENCE __ARGS ((sheadp))
shead_t	      *	sheadp;
#endif
{
	slist_id_t	id;
	int		unlink;

	SHEAD_ASSERT_LOCKED (sheadp);
	ASSERT (sheadp == SHEAD_MASTER (sheadp));
	ASSERT (sheadp->sh_lock_count > 0);

	id = SHEAD_ID (sheadp);

	/*
	 * We will delete a stream if there are no references holding it open,
	 * either pending locks or open references. If this is a stream pipe,
	 * then we need to check boths ends of the stream pipe. Because this
	 * function is called from the sleep-locking code, we know that
	 * "sheadp" points to the master end.
	 */

	unlink = sheadp->sh_open_count == 0 ||
			(SHEAD_IS_PIPE (sheadp) &&
				SHEAD_M2SLAVE (sheadp->sh_open_count) == 0);

	/*
	 * We normally assume that we won't be unlinking the stream, because
	 * to do that we need a lock on a global list (which is more expensive
	 * that a list on an individual stream).
	 *
	 * However, due to the relative hierarchy positions of the locks, if
	 * we discover we are likely to be the ones to dequeue the item, we
	 * take out a write lock then.
	 */

	if (sheadp->sh_lock_count > 1 && ! unlink) {
		/*
		 * Take the short path out.
		 */

		sheadp->sh_lock_count --;
		SHEAD_UNLOCK (sheadp, plbase);
		return;
	}

	/*
	 * Escalate to a write lock on the stream head; we will need
	 * to recheck to unlink condition after we escalate.
	 */

	SHEAD_UNLOCK (sheadp, plbase);

	(void) RW_WRLOCK (str_mem->sm_head_lock, plstr);
	(void) SHEAD_LOCK (sheadp);

	unlink = -- sheadp->sh_lock_count == 0 &&
			(sheadp->sh_open_count == 0 ||
			 (SHEAD_IS_PIPE (sheadp) &&
			  SHEAD_M2SLAVE (sheadp)->sh_open_count == 0));

	SHEAD_UNLOCK (sheadp, plstr);

	if (unlink) {
		shead_t	      *	scan;

		/*
		 * Remove from the singly-threaded list by searching for the
		 * immediate predecessor entry in the list (if any).
		 */

		if ((scan = str_mem->sm_streams [id]) == sheadp)
			str_mem->sm_streams [id] = sheadp->sh_next;
		else
			do {
				if (scan->sh_next == sheadp) {

					scan->sh_next = sheadp->sh_next;
					break;
				}
			} while ((scan = scan->sh_next) != NULL);

		if (scan == NULL)
			cmn_err (CE_WARN, "Failure unlinking stream from global directory");

		/*
		 * Note that stream pipes consist of four queues and
		 * two stream heads!
		 */

		SHEAD_DESTROY (sheadp);

		if (SHEAD_IS_PIPE (sheadp))
			QUEUE_FREE (sheadp->sh_head, 4,
				    2 * sizeof (* sheadp));
		else
			QUEUE_FREE (sheadp->sh_head, 2, sizeof (* sheadp));
	}

	RW_UNLOCK (str_mem->sm_head_lock, plbase);
}


/*
 * This function attempts to locate a stream linked below another stream based
 * on the multiplexor ID. If the multiplexor ID is -1, then this function
 * returns the first stream found linked below the given upper stream. In
 * addition, the "cmd" value is used to distinguish between the persistent and
 * regular multiplexor ID spaces.
 */

#if	__USE_PROTO__
__LOCAL__ shead_t * (SHEAD_FIND_MUXID) (shead_t * upper, int cmd,
					muxid_t muxid)
#else
__LOCAL__ shead_t *
SHEAD_FIND_MUXID __ARGS ((upper, cmd, muxid))
shead_t	      *	upper;
int		cmd;
muxid_t		muxid;
#endif
{
	shead_t	      *	scan;
	pl_t		prev_pl;
	int		checklist;

	/*
	 * Precook "cmd" for easier testing below.
	 */

	cmd = (cmd == I_PLINK || cmd == I_PUNLINK) ? SH_PLINK : 0;


	/*
	 * Now take out a lock to protect our list walking.
	 */

	prev_pl = RW_RDLOCK (str_mem->sm_head_lock, plstr);

	for (checklist = DEV_SLIST ; checklist < SLIST_MAX ; checklist ++) {

		for (scan = str_mem->sm_streams [checklist] ; scan != NULL ;
		     scan = scan->sh_next) {

			if (scan->sh_linked == upper &&
			    (scan->sh_flags & SH_PLINK) == cmd &&
			    ((scan->sh_muxid == muxid) || muxid == -1)) {

				goto done;
			}
		}
	}

done:
	RW_UNLOCK (str_mem->sm_head_lock, prev_pl);

	return scan;
}


/*
 * A local helper function for stream head timeouts.
 */

#if	__USE_PROTO__
__LOCAL__ void shead_timer_func (_VOID * arg)
#else
__LOCAL__ void
shead_timer_func (arg)
_VOID	      *	arg;
#endif
{
	shead_t	      *	sheadp = (shead_t *) arg;
	unsigned	locks;

	SHEAD_ASSERT_LOCKED (sheadp);

	SV_BROADCAST (sheadp->sh_wait_sv, 0);

	sheadp->sh_lock_mask &= ~ SH_TIMEFLAG;
	sheadp->sh_timeout_id = 0;
	sheadp->sh_time_count = 0;

	/*
	 * Note that we advance "sh_time_count" on behalf of the processes
	 * that have the stream head locked.
	 */

	locks = sheadp->sh_lock_mask & SH_LOCK_MASK;

	while (locks != 0) {

		if ((locks & 1) != 0)
			sheadp->sh_time_count ++;
		locks >>= 1;
	}
}


/*
 * This function is called when the time has come to actually initiate a
 * timeout.
 */

#if	__USE_PROTO__
int (SHEAD_START_TIMEOUT) (shead_t * sheadp)
#else
int
SHEAD_START_TIMEOUT __ARGS ((sheadp))
shead_t	      *	sheadp;
#endif
{
	__clock_t	the_time;

	SHEAD_ASSERT_LOCKED (sheadp);

	if ((sheadp->sh_lock_mask & SH_TIMEFLAG) == 0 ||
	    sheadp->sh_timeout_id != 0)
		return 1;		/* do nothing */

	(void) drv_getparm (LBOLT, & the_time);

	sheadp->sh_timeout_id = ltimeout (shead_timer_func, sheadp,
					  sheadp->sh_timeout_tick - the_time,
					  sheadp->sh_basic_lockp, plstr);

	/*
	 * If the timeout could not be scheduled, we return 0 to indicate to
	 * the caller that it should timeout immediately, and run the timeout
	 * function to fake a normal timeout.
	 */

	if (sheadp->sh_timeout_id == 0) {

		shead_timer_func (sheadp);
		return 0;
	}

	return 1;
}


/*
 * Indicate that no timeout will be necessary for this lock item.
 */

#if	__USE_PROTO__
void (SHEAD_NO_TIMEOUT) (shead_t * sheadp)
#else
void
SHEAD_NO_TIMEOUT __ARGS ((sheadp))
shead_t	      *	sheadp;
#endif
{
	SHEAD_ASSERT_LOCKED (sheadp);

	ASSERT (sheadp->sh_time_count < sheadp->sh_lock_count);

	if (++ sheadp->sh_time_count == sheadp->sh_lock_count) {
		/*
		 * Since we are the last process to register an end time, we
		 * get to actually initiate a timeout for the stream head.
		 */

		(void) SHEAD_START_TIMEOUT (sheadp);
	}
}


/*
 * Indicate that a timeout is desired for this lock item at the given clock
 * tick.
 */

#if	__USE_PROTO__
int (SHEAD_LOCK_TIMEOUT) (shead_t * sheadp, __clock_t end_time)
#else
int
SHEAD_LOCK_TIMEOUT __ARGS ((sheadp, end_time))
shead_t	      *	sheadp;
__clock_t	end_time;
#endif
{
	__clock_t	the_time;

	SHEAD_ASSERT_LOCKED (sheadp);

	ASSERT (sheadp->sh_time_count < sheadp->sh_lock_count);


	/*
	 * If our horizon falls before the current latest value (or if there
	 * is no latest value), select our horizon time. If the horizon time
	 * is *before* the current time, return 0.
	 *
	 * Comparing time values introduces the usual problems when dealing
	 * with sequence spaces in C. While the following expression is not
	 * as efficient as relying on the semantics of unsigned->signed
	 * casting, avoiding implementation-defined behaviour is important.
	 *
	 * "clock_t" MUST be unsigned for this to work.
	 */

	ASSERT ((__clock_t) -1 > 0);

	(void) drv_getparm (LBOLT, & the_time);

	if ((__clock_t) (the_time - end_time) < ((__clock_t) -1 >> 1)) {
		/*
		 * The indicated time has already passed, so we return a
		 * timeout indication directly.
		 */

		sheadp->sh_time_count ++;
		return 0;
	}


	if ((sheadp->sh_lock_mask & SH_TIMEFLAG) == 0 ||
	    (__clock_t) (sheadp->sh_timeout_tick - end_time) <
			((__clock_t) -1 >> 1)) {
		/*
		 * "end_time" will occur before the current latest time. If
		 * a timeout has been scheduled, we cancel it because we want
		 * to post a more recent one.
		 */

		if (sheadp->sh_timeout_id != 0) {

			untimeout (sheadp->sh_timeout_id);
			sheadp->sh_timeout_id = 0;
		}

		sheadp->sh_lock_mask |= SH_TIMEFLAG;
	}

	if (++ sheadp->sh_time_count == sheadp->sh_lock_count) {
		/*
		 * Since we are the last process to register an end time, we
		 * get to actually initiate a timeout for the stream head.
		 */

		return SHEAD_START_TIMEOUT (sheadp);
	} else {
		/*
		 * We can go to sleep and rely on someone else to actually
		 * intiate the timeout.
		 */

		return 1;
	}
}


/*
 * This function is used when a process wants to cancel a timeout after having
 * registered one.
 */

#if	__USE_PROTO__
void (SHEAD_END_TIMEOUT) (shead_t * sheadp)
#else
void
SHEAD_END_TIMEOUT __ARGS ((sheadp))
shead_t	      *	sheadp;
#endif
{
	SHEAD_ASSERT_LOCKED (sheadp);

	if (sheadp->sh_timeout_id != 0) {

		ASSERT (sheadp->sh_time_count == sheadp->sh_lock_count + 1);
		sheadp->sh_time_count --;
	}
}


/*
 * This function is used when a lock holder wishes to sleep waiting for a
 * timeout.
 */

#if	__USE_PROTO__
int (SHEAD_LOCKED_TIMEOUT) (shead_t * sheadp, __clock_t end_time)
#else
int
SHEAD_LOCKED_TIMEOUT __ARGS ((sheadp, end_time))
shead_t	      *	sheadp;
__clock_t	end_time;
#endif
{
	SHEAD_ASSERT_LOCKED (sheadp);

	ASSERT (sheadp->sh_time_count > 0);

	sheadp->sh_time_count --;

	return SHEAD_LOCK_TIMEOUT (sheadp, end_time);
}


/*
 * Common code to test whether a stream has experienced an error condition.
 */

#define	_shead_error(sheadp,mode) \
		(SHEAD_ASSERT_LOCKED (sheadp), \
		 sheadp->sh_linked != NULL ? EINVAL : \
		   ((mode) & FWRITE) != 0 && sheadp->sh_werrcode != 0 ? \
			sheadp->sh_werrcode : \
		     ((mode) & FREAD) != 0 ? sheadp->sh_rerrcode : 0)

/*
 * This function tests for error or hangup conditions on the stream head given
 * by "sheadp". It assumes that the caller holds a basic lock on the stream
 * head. If an error or hangup condition exists then the basic lock is
 * unlocked and a non-zero error number is returned.
 */

#if	__USE_PROTO__
__LOCAL__ int (SHEAD_ERRHUP_LOCKED) (shead_t * sheadp, int mode)
#else
__LOCAL__ int
SHEAD_ERRHUP_LOCKED __ARGS ((sheadp, mode))
shead_t	      *	sheadp;
int		mode;
#endif
{
	int		retval;

	SHEAD_ASSERT_LOCKED (sheadp);

	if ((retval = _shead_error (sheadp, mode)) != 0 ||
	    ((mode & (FREAD | FWRITE)) != 0 &&
		    (retval = ENXIO, SHEAD_HANGUP (sheadp) != 0))) {

		SHEAD_UNLOCK (sheadp, plbase);
		return retval;
	}

	return 0;
}


/*
 * Definitions use for the "interruptible" parameter to SHEAD_WAIT () and
 * SHEAD_LOCK ().
 */

enum {
	DONT_SIGNAL = 0,
	CHECK_SIGNALS = 1
};


/*
 * This function is the common interface to waiting for an event at a stream
 * head. It borrows the same synchronization variable used by the stream head
 * locking code in this implementation.
 *
 * We return 0 on success or an error number on failure.
 */

#if	__USE_PROTO__
__LOCAL__ int (SHEAD_WAIT) (shead_t * sheadp, int mode, cat_t category,
			    int interruptible)
#else
__LOCAL__ int
SHEAD_WAIT __ARGS ((sheadp, mode, category, interruptible))
shead_t	      *	sheadp;
int		mode;
cat_t		category;
int		interruptible;
#endif
{
	int		retval;

	SHEAD_ASSERT_LOCKED (sheadp);

	/*
	 * Test for error/hangup conditions before we sleep.
	 */

	if ((retval = SHEAD_ERRHUP_LOCKED (sheadp, mode)) != 0)
		return retval;

	/*
	 * Register our interest in the kind of event that we are waiting for.
	 */

	sheadp->sh_lock_mask |= category;

	if (interruptible == CHECK_SIGNALS)
		return SV_WAIT_SIG (sheadp->sh_wait_sv, primed,
				    sheadp->sh_basic_lockp) == 0 ? EINTR : 0;
	else {
		SV_WAIT (sheadp->sh_wait_sv, primed, sheadp->sh_basic_lockp);
		return 0;
	}
}


/*
 * This function is a slightly different interface to SHEAD_WAIT (), used when
 * the caller has been examining some property of a queue and wishes to go
 * to sleep atomically. A frozen queue is not suitable for passing to
 * SV_WAIT_SIG (), so we acquire the stream head global lock and then unfreeze
 * the queue on behalf of the caller. This yields correct behaviour because
 * SHEAD_WAKE () also attempts to acquire the stream head global lock; any
 * modification to a stream queue resulting in a wakeup request will follow
 * the same locking sequence.
 */

#if	__USE_PROTO__
__LOCAL__ int (SHEAD_WAIT_NONBLOCK) (shead_t * sheadp, int mode,
				     cat_t category, int interruptible)
#else
__LOCAL__ int
SHEAD_WAIT_NONBLOCK __ARGS ((sheadp, mode, category, interruptible))
shead_t	      *	sheadp;
int		mode;
cat_t		category;
int		interruptible;
#endif
{
	SHEAD_ASSERT_LOCKED (sheadp);

	if ((mode & (FNDELAY | FNONBLOCK)) != 0) {

		SHEAD_UNLOCK (sheadp, plbase);
		return EAGAIN;
	}

	return SHEAD_WAIT (sheadp, mode, category, interruptible);
}


/*
 * This function is used by lower-level code to signal events to functions
 * that have waited via SHEAD_WAIT (), above.
 */

#if	__USE_PROTO__
void (SHEAD_WAKE) (shead_t * sheadp, cat_t category)
#else
void
SHEAD_WAKE __ARGS ((sheadp, category))
shead_t	      *	sheadp;
cat_t		category;
#endif
{
	pl_t		prev_pl;

	prev_pl = SHEAD_LOCK (sheadp);

	if ((sheadp->sh_lock_mask & category) != 0) {

		sheadp->sh_lock_mask &= ~ category;
		SV_BROADCAST (sheadp->sh_wait_sv, 0);
	}

	SHEAD_UNLOCK (sheadp, prev_pl);
}


/*
 * Common code for locking stream head, shared between open-style locks (which
 * may need to allocate new head structures and need special coordination
 * with the stream head destruction code) and other kinds.
 *
 * We expect that the caller will have taken out a basic lock on the stream
 * head and that the caller will have incremented the lock count of the item
 * to prevent it from being deallocated while we wait.
 */

#if	__USE_PROTO__
int (SHEAD_SLEEP_LOCKED) (shead_t * sheadp, cat_t category, __clock_t timeout,
			  int interruptible)
#else
int
SHEAD_SLEEP_LOCKED __ARGS ((sheadp, category, timeout, interruptible))
shead_t	      *	sheadp;
cat_t		category;
__clock_t	timeout;
int		interruptible;
#endif
{
	__clock_t	end_time;	/* LBOLT when we time out */
	n_dev_t		devno;
	int		retval;

	SHEAD_ASSERT_LOCKED (sheadp);

	/*
	 * Since we expect the caller to have incremented the lock count, then
	 * the caller must have selected the master end of the stream pipe.
	 */

	ASSERT (sheadp == SHEAD_MASTER (sheadp));

	/*
	 * If we are going to be (possibly) timing out, calculate the time
	 * when that will happen.
	 */

	if (timeout > 0) {

		(void) drv_getparm (LBOLT, & end_time);
		end_time += timeout;
	}


	/*
	 * There is a special case that we have to note; in the case of a
	 * clone open, the device number of a stream head may be altered by
	 * the driver open routine. In this case, drivers waiting on the old
	 * number will have to be notified and give up on their lock attempts.
	 */

	devno = sheadp->sh_dev;


	/*
	 * Now we begin the actual business of locking the stream head.
	 */

	for (;;) {
		int		sigflg;

		/*
		 * Check to see whether our category is blocked. At this point
		 * we hold "global_lock".
		 *
		 * An earlier verson of this code had an explicit check for
		 * final close. I have no idea why, because if a stream is in
		 * final close, what else can happen?
		 */

		if ((sheadp->sh_lock_mask & category) == 0) {
			/*
			 * We can acquire a lock on the stream head in our
			 * chosen category, so we do so. Since we will not
			 * need a timeout, we increment the timeout count.
			 *
			 * If during later processing we need a timeout, we
			 * hook into this mechanism, but for simplicity we
			 * assume we won't.
			 */

			SHEAD_NO_TIMEOUT (sheadp);

			SHEAD_UNLOCK (sheadp, plbase);

			sheadp->sh_lock_mask |= category;

			return 0;
		}


		/*
		 * We need to wait, interruptibly. We might also want to time
		 * out at some stage.
		 *
		 * We (optionally) call a function to register the time we
		 * want to expire; this function also takes care of checking
		 * for timeout expiry.
		 */

		if (timeout == 0)
			SHEAD_NO_TIMEOUT (sheadp);
		else if (SHEAD_LOCK_TIMEOUT (sheadp, end_time) == 0) {
			/*
			 * Our horizon time has passed, so we return ETIME.
			 */

			retval = ETIME;
			break;
		}


		/*
		 * Now we can wait. No matter how we wake up, we will need
		 * to relock the global basic lock.
		 *
		 * The caller may not want this wait to be interruptible; this
		 * is reasonable when the lock is being acquired in some
		 * nested context where things are difficult to back out.
		 */

		if (interruptible != DONT_SIGNAL)
			sigflg = SV_WAIT_SIG (sheadp->sh_wait_sv, primed,
					      sheadp->sh_basic_lockp);
		else {
			SV_WAIT (sheadp->sh_wait_sv, primed,
				 sheadp->sh_basic_lockp);
			sigflg = 1;
		}

		(void) SHEAD_LOCK (sheadp);

		if (sigflg == 0) {
			/*
			 * We have been interrupted by a signal, so bang out
			 * to the caller with EINTR.
			 */

			retval = EINTR;
			break;
		} else if (sheadp->sh_dev != devno) {
			/*
			 * The device number has been altered. Flag the fact
			 * to the caller and give up this lock attempt.
			 */

			retval = ENODEV;
			break;
		}


		/*
		 * Now we have the global basic lock, we can wrap around to
		 * the start of the loop to recheck all our conditions.
		 */
	}


	/*
	 * For some reason we are aborting the lock attempt. The code which
	 * make us take this exit path should have set "* retvalp" with an
	 * error code.
	 */

	if (sheadp->sh_time_count > sheadp->sh_lock_count) {

		sheadp->sh_time_count --;
		ASSERT (sheadp->sh_time_count == sheadp->sh_lock_count);

		SHEAD_START_TIMEOUT (sheadp);
	}

	SHEAD_UNREFERENCE (sheadp);
	return retval;
}


/*
 * Entry point for the stream head locking system for use by routines that
 * already have a reference to the stream head. This entry performs checks for
 * routine errors including linked streams.
 */

#if	__USE_PROTO__
int (SHEAD_SLEEP_LOCK) (shead_t * sheadp, cat_t category, __clock_t timeout,
			int interruptible)
#else
int
SHEAD_SLEEP_LOCK __ARGS ((sheadp, category, timeout, interruptible))
shead_t	      *	sheadp;
cat_t		category;
__clock_t	timeout;
int		interruptible;
#endif
{
	SHEAD_LOCK (sheadp);

	/*
	 * The read and write lock modes are experimental. We have a mode bit
	 * that says whether or not we are really interested in honouring
	 * these lock types.
	 */

	if (((sheadp->sh_flags & SH_RWLOCKING) == 0 &&
	     (category & ~ (SH_READ_LOCK | SH_WRITE_LOCK)) == 0)) {

		SHEAD_UNLOCK (sheadp, plbase);
		return 0;
	}


	/*
	 * If the caller wishes to lock a stream head that is part of a stream
	 * pipe, we direct the lock attempt to the master stream head of the
	 * pair that form the pipe. This ensures that any attempt to modify
	 * the state of the pipe from either end will be properly single-
	 * threaded.
	 *
	 * Note that we *must* perform a similar redirection in the unlock.
	 */

	sheadp = SHEAD_MASTER (sheadp);


	/*
	 * We have a pointer to the stream head and hold a global basic lock.
	 *
	 * With the protection of the basic lock, we increment the lock count.
	 */

	sheadp->sh_lock_count ++;

	/*
	 * Now we begin the actual business of locking the stream head.
	 */

	return SHEAD_SLEEP_LOCKED (sheadp, category, timeout, interruptible);
}


/*
 * This is a special form of the stream head locking code for open () access,
 * which specially coordinates with the close code to discover when to
 * allocate a new stream head, and carefully avoids the problems that can
 * occur if the stream head were to be deallocated which we are waiting for
 * it to be unlocked.
 *
 * We also have to do some funky stuff here because of clone opens.
 */

#if	__USE_PROTO__
shead_t * (SHEAD_OPEN_LOCK) (n_dev_t dev, struct streamtab * stabp,
			     int * retvalp)
#else
shead_t *
SHEAD_OPEN_LOCK __ARGS ((dev, stabp, retvalp))
n_dev_t		dev;
struct streamtab
	      * stabp;
int	      *	retvalp;
#endif
{
	shead_t	      *	sheadp;

	ASSERT (retvalp != NULL);

	/*
	 * PHASE 1: Locate the stream head. If the stream head did not
	 * previously exist, we might be able to lock it immediately by virtue
	 * of being able to create it that way. Of course, simultaneous open
	 * attempts might result in this looping as only once of the created
	 * stream heads will be entered in the stream directory.
	 */

	* retvalp = 0;

	for (;;) {
		queue_t	      *	q;

		/*
		 * The first thing we need to do is *find* the stream. We call
		 * a find routine that increments a reference count so that
		 * we can be sure that the stream will not be deallocated
		 * while we wait.
		 */

		if ((sheadp = SHEAD_FIND_AND_LOCK (dev, DEV_SLIST)) != NULL) {
			/*
			 * Sleep lock time; our call to SHEAD_FIND_AND_LOCK ()
			 * will have incremented the lock count of the stream
			 * head so it won't disappear underneath us.
			 */

			* retvalp = SHEAD_SLEEP_LOCKED (sheadp, SH_OPENCLOSE,
							0, CHECK_SIGNALS);

			if (* retvalp != 0) {
				/*
				 * If the lock attempt failed because of a
				 * clone open changing the stream head's
				 * device number, we need to try again.
				 */

				sheadp = NULL;

				if (* retvalp == ENODEV)
					continue;
			}

			return sheadp;
		}


		/*
		 * There ain't no such stream, so we have to allocate a queue
		 * pair.
		 */

		if ((q = QUEUE_ALLOC (2, sizeof (* sheadp))) == NULL) {

			* retvalp = ENFILE;
			return NULL;
		}

		sheadp = (shead_t *) q->q_ptr;

		SHEAD_INIT (sheadp, stabp, dev, q);

		sheadp->sh_ref_count = 1;
		sheadp->sh_lock_mask = SH_OPENCLOSE;
		sheadp->sh_time_count = sheadp->sh_lock_count = 1;

		if (SHEAD_ADD (sheadp) == 0)
			return sheadp;

		/*
		 * The new queue could not be added to the stream
		 * directory, presumably because of a nearly
		 * simultaneous open attempt.
		 *
		 * We undo the allocation we wrought before retrying.
		 */

		SHEAD_DESTROY (sheadp);
		QUEUE_FREE (q, 2, sizeof (* sheadp));
	}
}


/*
 * Unlock a stream head.
 */

#if	__USE_PROTO__
void (SHEAD_SLEEP_UNLOCK) (shead_t * sheadp, cat_t category)
#else
void
SHEAD_SLEEP_UNLOCK __ARGS ((sheadp, category))
shead_t	      *	sheadp;
cat_t		category;
#endif
{
	ASSERT (sheadp != NULL);
	ASSERT (category != 0);

	/*
	 * See if locking is necessary for the read and write operations.
	 */

	if ((sheadp->sh_flags & SH_RWLOCKING) == 0 &&
	    (category & ~ (SH_READ_LOCK | SH_WRITE_LOCK)) == 0) {
		/*
		 * Since we don't actually acquire any locks, we return early.
		 */

		ASSERT (sheadp->sh_open_count > 0);
		return;
	}

	ASSERT (sheadp->sh_lock_count > 0);
	ASSERT ((sheadp->sh_lock_mask & category) == category);


	/*
	 * We direct all locking operations on the slave part of a stream pipe
	 * to the master end.
	 */

	sheadp = SHEAD_MASTER (sheadp);

	(void) SHEAD_LOCK (sheadp);


	/*
	 * Unlike SHEAD_WAKE (), we can assume that our category mask will not
	 * be NULL because of the difference in interpretation between lock
	 * flags (indicating a holder) and wait flags (indicating a waiter).
	 */

	sheadp->sh_lock_mask &= ~ category;
	SV_BROADCAST (sheadp->sh_wait_sv, 0);


	/*
	 * We don't use SHEAD_END_TIMEOUT () here since we are a lock holder
	 * and shead_time_func () makes sure to keep our "sh_time_count" entry
	 * greater than 0.
	 */

	ASSERT (sheadp->sh_time_count > 0);
	sheadp->sh_time_count --;

	if (sheadp->sh_time_count == 0 && sheadp->sh_timeout_id != 0) {
		/*
		 * Since there is no-one waiting for anything, cancel any
		 * pending timeouts.
		 */

		untimeout (sheadp->sh_timeout_id);

		sheadp->sh_lock_mask &= SH_TIMEFLAG;
		sheadp->sh_timeout_id = 0;
	}


	/*
	 * Now that we have done everything that requires access to the stream
	 * head, decrement the lock count.
	 */

	 SHEAD_UNREFERENCE (sheadp);
}


/*
 * This local function asserts that the caller holds a lock on the stream head.
 * Since we can't really determine that, we actually just assert that someone
 * has a lock on the stream head.
 */

#if	__USE_PROTO__
__LOCAL__ void (ASSERT_SLEEP_LOCKED) (shead_t * sheadp, cat_t category)
#else
__LOCAL__ void
ASSERT_SLEEP_LOCKED __ARGS ((sheadp, category))
shead_t	      *	sheadp;
cat_t		category;
#endif
{
	ASSERT (sheadp != NULL);

	if (SHEAD_IS_PIPE (sheadp))
		sheadp = SHEAD_MASTER (sheadp);

	ASSERT ((sheadp->sh_lock_mask & category) == category);
}


/*
 * Wait for a queue to drain. The caller must have the stream head locked when
 * calling this function.
 */

#if	__USE_PROTO__
__LOCAL__ void (DRAIN_QUEUE) (shead_t * sheadp, queue_t * q)
#else
__LOCAL__ void
DRAIN_QUEUE __ARGS ((sheadp, q))
shead_t	      *	sheadp;
queue_t	      *	q;
#endif
{
	__clock_t	end_time;

	ASSERT_SLEEP_LOCKED (sheadp, SH_OPENCLOSE);

	/*
	 * When we are in final close, the STREAMS specification says we
	 * should wait for up to 15 seconds for the write-side queue to be
	 * drained of data, unless we are in O_NONBLOCK mode.
	 *
	 * If we can't post the timeout, then don't wait.
	 */

	if (sheadp->sh_cltime == 0)
		return;

	(void) drv_getparm (LBOLT, & end_time);
	end_time += sheadp->sh_cltime;

	for (;;) {
		/*
		 * We need to acquire a basic lock to pass to SV_WAIT (), and
		 * the code that wakes us up will attempt to acquire the same
		 * lock (see QUEUE_DRAINED ()). Since the wakeup code must
		 * acquire the lock while holding the stream frozen, we must
		 * do things in the same order to prevent the possibility of
		 * deadlock.
		 */

		(void) QFREEZE_TRACE (q, "DRAIN_QUEUE");

		if (q->q_first == NULL) {
			/*
			 * No messages on the queue => our job is done.
			 */

			QUNFREEZE_TRACE (q, plbase);
			break;
		}


		/*
		 * We are going to wait for this queue to become empty, so we
		 * set a flag to indicate that we are interested in finding
		 * out when that happens. Note that we don't ever clear the
		 * flag in this routine; that is the responsibility of the
		 * code which will wake us up.
		 */

		q->q_flag |= QDRAIN;


		/*
		 * We don't build on SHEAD_WAIT (), although we do expect to
		 * be woken up via SHEAD_WAKE (). We transfer our lock from
		 * the queue to the stream head.
		 */

		(void) SHEAD_LOCK (sheadp);

		QUNFREEZE_TRACE (q, plstr);


		/*
		 * Register when we want to time out. If that time has already
		 * passed, then exit to the caller.
		 */

		if (SHEAD_LOCKED_TIMEOUT (sheadp, end_time) == 0) {

			SHEAD_UNLOCK (sheadp, plbase);
			return;
		}

		sheadp->sh_lock_mask |= SH_DRAIN_WAIT;

		SV_WAIT (sheadp->sh_wait_sv, primed, sheadp->sh_basic_lockp);

		/*
		 * We were signalled, so we try again.
		 */
	}
}


/*
 * This function (used in the implementation of I_LIST ioctl ()) returns a
 * count of the number of modules on the stream, including the topmost driver.
 */

#if	__USE_PROTO__
__LOCAL__ int (SHEAD_MODCOUNT) (shead_t * sheadp)
#else
__LOCAL__ int
SHEAD_MODCOUNT __ARGS ((sheadp))
shead_t	      *	sheadp;
#endif
{
	pl_t		prev_pl;
	int		count;
	queue_t	      *	scan;

	ASSERT (sheadp != NULL);

	/*
	 * Note that we use SHEAD_MASTER () because this walk can be affected
	 * by attempts to push or pop queues from either end of a stream pipe.
	 * Using a single lock at the master end avoids problems with this.
	 */

	prev_pl = SHEAD_LOCK (SHEAD_MASTER (sheadp));

	count = 0;

	for (scan = W (sheadp->sh_head)->q_next ; scan != NULL ;
	     scan = scan->q_next) {

		if ((scan->q_flag & QPROCSOFF) == 0)
			count ++;
	}

	SHEAD_UNLOCK (SHEAD_MASTER (sheadp), prev_pl);

	return count;
}


/*
 * Utility routine to return the next write queue below the stream head. This
 * routine deals with locking the stream head for the duration of the walk and
 * also check whether the queue has been disabled with qprocsoff ().
 */

#if	__USE_PROTO__
__LOCAL__ queue_t * (TOP_QUEUE) (shead_t * sheadp)
#else
__LOCAL__ queue_t *
TOP_QUEUE __ARGS ((sheadp))
shead_t	      *	sheadp;
#endif
{
	pl_t		prev_pl;
	queue_t	      *	scan;

	ASSERT (sheadp != NULL);

	/*
	 * Note that we use SHEAD_MASTER () because this walk can be affected
	 * by attempts to push or pop queues from either end of a stream pipe.
	 * Using a single lock at the master end avoids problems with this.
	 */

	prev_pl = SHEAD_LOCK (SHEAD_MASTER (sheadp));

	scan = W (sheadp->sh_head)->q_next;

	while ((scan->q_flag & QPROCSOFF) != 0)
		if ((scan = scan->q_next) == NULL)
			cmn_err (CE_PANIC, "Off end of stream in TOP_QUEUE ()");

	SHEAD_UNLOCK (SHEAD_MASTER (sheadp), prev_pl);

	return scan;
}


/*
 * Utility routine for POP_MODULE () and the I_LOOK processing code that finds
 * the queue entry for the first module on the stream (if any).
 */

#if	__USE_PROTO__
__LOCAL__ queue_t * (TOP_MODULE) (shead_t * sheadp)
#else
__LOCAL__ queue_t *
TOP_MODULE __ARGS ((sheadp))
shead_t	      *	sheadp;
#endif
{
	queue_t	      *	scan;		/* module queue */

	/*
	 * We return the queue pointer if and only if the thing below the
	 * stream head is a module (not a driver) AND the module's read and
	 * write queues are not interchanged (as they would be at the
	 * crossover point of a STREAMS-based FIFO).
	 */

	scan = TOP_QUEUE (sheadp);

	if ((scan->q_flag & QREADR) !=
			(W (sheadp->sh_head)->q_flag & QREADR))
		return NULL;

	/*
	 * The test to see whether "scan" is a module or driver does not need
	 * to involve QUEUE_NEXT (), since the exact value of 'q->q_next'
	 * isn't important to us, just whether or not it's NULL.
	 */

	{
		pl_t		prev_pl;
		queue_t	      *	next;

		prev_pl = QFREEZE_TRACE (scan, "TOP_MODULE");

		next = scan->q_next;

		QUNFREEZE_TRACE (scan, prev_pl);

		if (next == NULL)
			return NULL;
	}

	return R (scan);
}


/*
 * Code common to both POP_MODULE () and PUSH_MODULE () for removing and
 * deallocating a queue pair from a stream.
 */

#if	__USE_PROTO__
__LOCAL__ void (POP_AND_FREE) (shead_t * sheadp, queue_t * module)
#else
__LOCAL__ void
POP_AND_FREE __ARGS ((sheadp, module))
shead_t	      *	sheadp;
queue_t	      *	module;
#endif
{
	pl_t		prev_pl;
	pl_t		q_pl;
	queue_t	      *	next;


	/*
	 * Now we must unlink the module queue from the stream. To do this, we
	 * freeze each queue before we change it. However, that is not enough
	 * if we are on a stream pipe, since the stream head at the other end
	 * of the pipe could be trying to modify the same stream, and thus the
	 * same queue pointers that we are going to change.
	 *
	 * We work our way around this by defining a master/slave relationship
	 * between the ends of a pipe, and requiring that the slave end
	 * acquire an exclusive lock on the "sh_rwlockp" lock belonging to the
	 * master. Since the master and slave contend for the same lock, we
	 * can be confident no other concurrent modifications to the stream
	 * are possible.
	 */

	prev_pl = SHEAD_LOCK (SHEAD_MASTER (sheadp));

	next = W (sheadp->sh_head);

	ASSERT (next->q_next == W (module));

	q_pl = QFREEZE_TRACE (next, "POP_AND_FREE");
	next->q_next = W (module)->q_next;
	QUNFREEZE_TRACE (next, q_pl);


	next = OTHERQ (next->q_next);

	ASSERT (next->q_next == module);

	q_pl = QFREEZE_TRACE (next, "POP_AND_FREE");
	next->q_next = sheadp->sh_head;
	QUNFREEZE_TRACE (next, q_pl);


	SHEAD_UNLOCK (SHEAD_MASTER (sheadp), prev_pl);


	/*
	 * Now we can de-initialize the queue pair and free the memory.
	 */

	QUEUE_FREE (module, 1, 0);
}


/*
 * Pop a module from a stream. In order to request this, the caller must have
 * the stream head sleep-locked for modification (see SHEAD_SLEEP_LOCK ()
 * above).
 *
 * Returns 0 on success or an error number on failure.
 */

#if	__USE_PROTO__
int (POP_MODULE) (shead_t * sheadp, queue_t * q, int mode, cred_t * credp)
#else
int
POP_MODULE __ARGS ((sheadp, q, mode, credp))
shead_t	      *	sheadp;
queue_t	      *	q;
int		mode;
cred_t	      *	credp;
#endif
{
	int		retval;

	ASSERT (q != NULL);
	ASSERT_SLEEP_LOCKED (sheadp, SH_OPENCLOSE);

	/*
	 * When a module is popped, it is blown away. If the module needs to
	 * be drained first (as in final close) then the caller has to do it.
	 */

	retval = (* q->q_qinfo->qi_qclose) (q, mode, credp);

	if (retval != 0)
		cmn_err (CE_WARN, "module close returned %d in POP_MODULE",
			 retval);

	/*
	 * In case the module didn't turn off put and service routines.
	 */

	if ((q->q_flag & QPROCSOFF) == 0) {

		cmn_err (CE_WARN, "Module %s did not call qprocsoff ()",
			 q->q_qinfo->qi_minfo->mi_idname);
		qprocsoff (q);
	}


	/*
	 * And now we can release the module. We do this whether or not the
	 * caller returned an error.
	 */

	POP_AND_FREE (sheadp, q);

	return retval;
}


/*
 * This function pushes the indicated module onto a stream. The caller must
 * have the stream head sleep-locked for modification.
 */

#if	__USE_PROTO__
int (PUSH_MODULE) (shead_t * sheadp, int mode, cred_t * credp,
		   modsw_t * module)
#else
int
PUSH_MODULE __ARGS ((sheadp, mode, credp, module))
shead_t	      *	sheadp;
int		mode;
cred_t	      *	credp;
modsw_t	      *	module;
#endif
{
	queue_t	      *	q;
	queue_t	      *	prev;
	pl_t		prev_pl;
	pl_t		q_pl;
	int		retval;
	n_dev_t		dev;
	char	      *	modname;

	ASSERT (module != NULL);
	ASSERT_SLEEP_LOCKED (sheadp, SH_OPENCLOSE);

	if ((q = QUEUE_ALLOC (1, 0)) == NULL)
		return ENOSR;

	QUEUE_INIT (q, module->mod_stream, QI_NORMAL);

	/*
	 * First we have to link the module into the stream. The newly
	 * allocated queues will have the QPROCSOFF flag set so that they are
	 * ignored by other stream elements until the new module has been
	 * opened.
	 *
	 * As above in POP_MODULE (), we acquire a write lock on the stream
	 * head, which must be a master end if a stream pipe.
	 */

	prev_pl = SHEAD_LOCK (SHEAD_MASTER (sheadp));

	prev = W (sheadp->sh_head);
	W (q)->q_next = prev->q_next;

	q_pl = QFREEZE_TRACE (prev, "PUSH_MODULE");
	prev->q_next = W (q);
	QUNFREEZE_TRACE (prev, q_pl);

	prev = OTHERQ (W (q)->q_next);
	q->q_next = sheadp->sh_head;

	ASSERT (prev->q_next == sheadp->sh_head);

	q_pl = QFREEZE_TRACE (prev, "PUSH_MODULE");
	prev->q_next = q;
	QUNFREEZE_TRACE (prev, q_pl);

	SHEAD_UNLOCK (SHEAD_MASTER (sheadp), prev_pl);


	/*
	 * Ask the module to set itself up.
	 */

	dev = sheadp->sh_dev;

	retval = (* q->q_qinfo->qi_qopen) (q, & dev, mode, MODOPEN,
					   credp);

	modname = q->q_qinfo->qi_minfo->mi_idname;

	if (dev != sheadp->sh_dev)
		cmn_err (CE_WARN, "Module \"%s\" altered its \"dev\" parameter",
			 modname);

	if (retval != 0) {
		/*
		 * OK, don't really open this. The module should not have
		 * turned on it's put and service routines.
		 */

		if ((q->q_flag & QPROCSOFF) == 0) {

			cmn_err (CE_WARN, "PUSH_MODULE () : Module %s enabled queue!",
				 modname);
			qprocson (q);
		}

		POP_AND_FREE (sheadp, q);
	} else if ((q->q_flag & QPROCSOFF) != 0) {

		cmn_err (CE_WARN, "PUSH_MODULE () : Module %s did not enable queue!",
			 modname);
		qprocson (q);
	}

	return retval;
}


/*
 * This function sends an IOCTL message downstream, then blocks until either
 * a reply arrives at the stream head, the (optional) timeout has expired, or
 * a signal is received.
 *
 * The caller must have the stream head locked for ioctl () processing.
 */

#if	__USE_PROTO__
mblk_t * (IOCTL_SEND) (shead_t * sheadp, int mode, mblk_t * msg,
		       int * errretp, __clock_t timeout_time)
#else
mblk_t *
IOCTL_SEND __ARGS ((sheadp, mode, msg, errretp, timeout_time))
shead_t	      *	sheadp;
int		mode;
mblk_t	      *	msg;
int	      *	errretp;
__clock_t	timeout_time;
#endif
{
	__clock_t	end_time;
	int		retval;

	ASSERT_SLEEP_LOCKED (sheadp, SH_IOCTL_LOCK);
	ASSERT (errretp != NULL);
	ASSERT (msg != NULL);

	/*
	 * Set the optional timeout up first. If the timeout cannot be
	 * allocated, we have to return failure.
	 */

	if (timeout_time > 0) {

		(void) drv_getparm (LBOLT, & end_time);
		end_time += timeout_time;
	}


	/*
	 * Now send the client's message downstream. We queue the message on
	 * the write queue rather than directly putting it to avoid the
	 * possibility of deadlock if an acknowledgement is sent back to us
	 * while we are holding the basic lock below.
	 *
	 * We also have to check for the possibility that an error has occurred
	 * on the stream.
	 */

	(void) SHEAD_LOCK (sheadp);

	for (;;) {
		if (sheadp->sh_ioc_msg != NULL) {
			/*
			 * If there is some stale ioctl () message lying
			 * around, dispose of it.
			 */

			freemsg (sheadp->sh_ioc_msg);
			sheadp->sh_ioc_msg = NULL;
		}

		if (msg != NULL)
			putq (W (sheadp->sh_head), msg);

		msg = NULL;


		/*
		 * Before we go to sleep, we schedule our timeout.
		 */

		if (timeout_time > 0 &&
		    SHEAD_LOCKED_TIMEOUT (sheadp, end_time) == 0) {
			/*
			 * We have timed out.
			 */

			retval = ETIME;
			break;
		}


		if (sheadp->sh_open_count == 0) {
			/*
			 * This is a kernel-generated ioctl () and signalling
			 * is not allowed to interrupt us. We pass a NULL mode
			 * to avoid detecting errors.
			 */

			ASSERT_SLEEP_LOCKED (sheadp, SH_OPENCLOSE);

			retval = SHEAD_WAIT (sheadp, 0, SH_IOCTL_WAIT,
					     DONT_SIGNAL);
		} else {
			/*
			 * SHEAD_WAIT () checks for hangups as well, if the
			 * mode includes read or write.
			 */

			ASSERT ((mode & (FREAD | FWRITE)) != 0);

			retval = SHEAD_WAIT (sheadp, mode, SH_IOCTL_WAIT,
					     CHECK_SIGNALS);
		}


		/*
		 * We take out the basic lock here again so we can read
		 * "sheadp->sh_ioc_msg" atomically.
		 */

		(void) SHEAD_LOCK (sheadp);

		msg = sheadp->sh_ioc_msg;
		sheadp->sh_ioc_msg = NULL;

		if (retval != 0 || msg != NULL)
			break;

		/*
		 * We have been woken up either by a timeout, or for some
		 * activity at the stream head not related to us. We loop to
		 * deal with this.
		 */
	}

	if (retval != 0) {

		if (errretp != NULL)
			* errretp = retval;

		if (msg != NULL)
			freemsg (msg);

		msg = NULL;
	}

	SHEAD_UNLOCK (sheadp, plbase);

	return msg;
}


/*
 * This function manages transparent ioctl () processing; it sets itself up
 * to service M_COPYIN and M_COPYOUT requests from a driver until it sees an
 * M_IOCACK or M_IOCNAK.
 */

typedef union {
	struct iocblk	ioc;
	struct copyreq	req;
	struct copyresp	resp;
} x_ioc_t;

#if	__USE_PROTO__
__LOCAL__ int (TRANSPARENT_IOCTL) (shead_t * sheadp, int mode, int cmd,
				   _VOID * arg, cred_t * credp, int * rvalp)
#else
__LOCAL__ int
TRANSPARENT_IOCTL __ARGS ((sheadp, mode, cmd, arg, credp, rvalp))
shead_t	      *	sheadp;
int		mode;
int		cmd;
_VOID	      *	arg;
cred_t	      *	credp;
int	      *	rvalp;
#endif
{
	mblk_t	      *	msg;
	mblk_t	      *	data;
	int		retval;
	x_ioc_t	      *	ioc;

	ASSERT_SLEEP_LOCKED (sheadp, SH_IOCTL_LOCK);

	/*
	 * The message block allocated for a transparent ioctl () must be
	 * large enough to hold any of the ioctl-related message types so that
	 * modules and drivers (and the stream head) can just change the
	 * message type to reply to a message.
	 */

	if ((msg = MSGB_ALLOC (sizeof (* ioc), BPRI_LO, KM_SLEEP)) == NULL)
		return ENOSR;

	ioc = (x_ioc_t *) msg->b_rptr;
	msg->b_wptr = (unsigned char *) (ioc + 1);

	msg->b_datap->db_type = M_IOCTL;

	ioc->ioc.ioc_cmd = cmd;
	ioc->ioc.ioc_cr = credp;
	ioc->ioc.ioc_id = ++ sheadp->sh_ioc_seq;
	ioc->ioc.ioc_count = TRANSPARENT;
	ioc->ioc.ioc_rval = ioc->ioc.ioc_error = 0;


	/*
	 * A transparent ioctl () gets a single data block containing the
	 * value of "arg".
	 */

	if ((data = MSGB_ALLOC (sizeof (arg), BPRI_LO, KM_SLEEP)) == NULL) {

		retval = ENOSR;
		goto done;
	}

	* (_VOID **) data->b_rptr = arg;
	data->b_wptr += sizeof (arg);


	for (;;) {
		/*
		 * Now we send the ioctl () and wait for the acknowledgement.
		 * Transparent ioctl ()'s wait forever, but M_ERROR and
		 * hangup events are interesting to us, so we can blow out.
		 */

		if ((msg = IOCTL_SEND (sheadp, mode, msg, & retval,
				       0)) == NULL)
			return retval;


		/*
		 * Transparent ioctl ()'s dont have to worry about data coming
		 * back in the M_IOCACK message, they just have to process the
		 * M_COPYIN and M_COPYOUT requests.
		 */

		switch (msg->b_datap->db_type) {

		case M_IOCNAK:
			retval = ioc->ioc.ioc_error;
			goto done;

		case M_IOCACK:
			* rvalp = ioc->ioc.ioc_rval;
			retval = ioc->ioc.ioc_error;

			if (ioc->ioc.ioc_count > 0)
				cmn_err (CE_WARN, "Transparent ioctl () processing forbids data in M_IOCACK");
			goto done;

		case M_COPYIN:
			if ((data = msg->b_cont) != NULL)
				freemsg (data);

			/*
			 * The STREAMS documentation is unclear as to whether
			 * these blocks are split up or not, but it seems
			 * unlikely.
			 */

			if ((data = MSGB_ALLOC (ioc->req.cq_size, BPRI_LO,
						KM_SLEEP)) == NULL)
				retval = ENOSR;
			else if (copyin (ioc->req.cq_addr, data->b_rptr,
					 ioc->req.cq_size) != 0) {
				freemsg (data);
				data = NULL;
				retval = EFAULT;
			} else {

				data->b_wptr = data->b_rptr +
						ioc->req.cq_size;
				retval = 0;
			}

			msg->b_cont = data;
			break;

		case M_COPYOUT:
			retval = 0;

			while (ioc->req.cq_size > 0 &&
			       (data = msg->b_cont) != NULL) {
				size_t		unit;

				/*
				 * Copy a single M_DATA block at a time. After
				 * copying, we free the block.
				 */

				unit = data->b_wptr - data->b_rptr;
				if (unit > ioc->req.cq_size)
					unit = ioc->req.cq_size;

				if (copyout (data->b_rptr, ioc->req.cq_addr,
					     unit) != 0) {
					retval = EFAULT;
					break;
				}

				ioc->req.cq_size -= unit;
				ioc->req.cq_addr += unit;

				msg->b_cont = data->b_cont;
				freeb (data);
			}

			/*
			 * Throw away uncopied extra data.
			 */

			if ((data = msg->b_cont) != NULL)
				freemsg (data);

			msg->b_cont = NULL;
			break;

		default:
			cmn_err (CE_WARN, "Invalid message type %d received during unlink processing",
				 msg->b_datap->db_type);
			retval = ENXIO;
			goto done;
		}

		/*
		 * In common code for M_COPYIN and M_COPYOUT, we turn around
		 * the request message. If the request succeeded, we wrap
		 * around to the top of the loop to serve the next request;
		 * it there has been an error, we just put the message and
		 * bail out.
		 */

		msg->b_datap->db_type = M_IOCDATA;
		ioc->resp.cp_rval = (caddr_t) retval;

		if (retval != 0) {

			putq (W (sheadp->sh_head), msg);
			break;
		}
	}

done:
	freemsg (msg);

	return retval;
}


/*
 * This function contains code common to the ioctl () message processing for
 * stream link and unlink commands. These ioctl ()s send messages downstream
 * which are all of a single common form.
 */

#if	__USE_PROTO__
__LOCAL__ int (LINK_MESSAGE) (shead_t * upper, int mode, shead_t * lower,
			      int cmd, int muxid, cred_t * credp,
			      int * retvalp)
#else
__LOCAL__ int
LINK_MESSAGE __ARGS ((upper, mode, lower, cmd, muxid, credp, retvalp))
shead_t	      *	upper;
int		mode;
shead_t	      *	lower;
int		cmd;
int		muxid;
cred_t	      *	credp;
int	      *	retvalp;
#endif
{
	mblk_t	      *	msg;
	struct iocblk *	ioc;
	struct linkblk * linkblk;
	int		ackflag;
	queue_t	      *	q;

	ASSERT_SLEEP_LOCKED (upper, SH_OPENCLOSE | SH_IOCTL_LOCK);
	ASSERT_SLEEP_LOCKED (lower, SH_OPENCLOSE | SH_IOCTL_LOCK);
	ASSERT (credp != NULL);

	/*
	 * Set ourselves up for a STREAMS ioctl (). Note that since we keep
	 * transparent ioctl () processing separate from normal I_STR code, we
	 * don't have to allocate an initial message block that can be turned
	 * into a "copyreq" or "copyresp" structure.
	 */


	if ((msg = MSGB_ALLOC (sizeof (* ioc), BPRI_LO, KM_SLEEP)) == NULL)
		return ENOSR;

	ioc = (struct iocblk *) msg->b_rptr;
	msg->b_wptr = (unsigned char *) (ioc + 1);

	msg->b_datap->db_type = M_IOCTL;

	ioc->ioc_cmd = cmd;
	ioc->ioc_cr = credp;
	ioc->ioc_id = ++ upper->sh_ioc_seq;
	ioc->ioc_count = sizeof (struct linkblk);
	ioc->ioc_rval = ioc->ioc_error = 0;


	/*
	 * Now we allocate and fill in the data part of the I_...LINK message.
	 */

	if ((msg->b_cont = MSGB_ALLOC (sizeof (struct linkblk), BPRI_LO,
				       KM_SLEEP)) == NULL) {
		freeb (msg);
		return ENOSR;
	}

	linkblk = (struct linkblk *) msg->b_cont->b_rptr;
	msg->b_cont->b_wptr = (unsigned char *) (linkblk + 1);


	/*
	 * Find the bottom-most write queue on the upper stream. To make this
	 * walk of the queue safe with respect to I_PUSH and I_POP, we take
	 * out a read lock on the stream head. See the PUSH_MODULE () and
	 * POP_MODULE () routines for more details on this.
	 *
	 * We don't use QUEUE_NEXT () because the intermediate modules are of
	 * no interest to us; we just want the driver at the bottom.
	 */

	{
		queue_t	      *	next;

		(void) SHEAD_LOCK (upper);

		next = W (upper->sh_head);

		do {
			q = next;

			(void) QFREEZE_TRACE (q, "LINK_MESSAGE");

			next = q->q_next;

			QUNFREEZE_TRACE (q, plbase);
		} while (next != NULL);

		SHEAD_UNLOCK (upper, plbase);
	}

	linkblk->l_qtop = q;
	linkblk->l_qbot = lower->sh_head;
	linkblk->l_index = muxid;


	/*
	 * Now we send the ioctl () and wait for the acknowledgement. Since
	 * there is no mechanism for managing this timeout, we will use the
	 * close timeout (which is appropriate given that this operation will
	 * often be performed as the result of a close ()).
	 */

	if ((msg = IOCTL_SEND (upper, mode, msg, retvalp,
			       upper->sh_cltime)) == NULL)
		return 0;		/* counts as a negative ack */

	/*
	 * Now we see what kind of message the driver has send to us.
	 */

	ioc = (struct iocblk *) msg->b_rptr;

	if (retvalp != NULL)
		* retvalp = ioc->ioc_error;

	switch (msg->b_datap->db_type) {

	case M_IOCNAK:
		ackflag = 0;
		break;

	case M_IOCACK:
		/*
		 * We do not copy the "ioc_rval" member out because it is not
		 * documented as forming the return value from such a link-
		 * style ioctl () request.
		 */

		/*
		 * Since "arg" for an I_UNLINK or I_PUNLINK is not a pointer,
		 * it makes no sense for a driver to attempt to return data
		 * for the user. Since the canonical multiplexing driver code
		 * clears ioc_count and simply turns around the message, it
		 * is not a problem for there to be M_DATA messages following
		 * the M_IOCACK, but it is a problem if "ioc_count" is greater
		 * than 0.
		 */

		ackflag = 1;

		if (ioc->ioc_count > 0)
			cmn_err (CE_WARN, "Driver %s returned data with link/unlink ioctl ()",
				 q->q_qinfo->qi_minfo->mi_idname);

		break;

	default:
		cmn_err (CE_WARN, "Invalid message type %d received during link/unlink processing",
			 msg->b_datap->db_type);
		* retvalp = ENXIO;

		ackflag = 0;		/* treat as a negative ack */
		break;
	}

	freemsg (msg);

	return ackflag;
}


/*
 * Helper function for the link/unlink process to restore a stream to the
 * unlinked state.
 */

#if	__USE_PROTO__
__LOCAL__ int (SHEAD_INIT_UNLINKED) (shead_t * sheadp)
#else
__LOCAL__ int
SHEAD_INIT_UNLINKED __ARGS ((sheadp))
shead_t	      *	sheadp;
#endif
{
	pl_t		prev_pl;
	int		final_close;

	ASSERT_SLEEP_LOCKED (sheadp, SH_OPENCLOSE | SH_IOCTL_LOCK);

	/*
	 * The driver is no longer using this stream; restore it to normal
	 * operation. Note that we have to reset "q_ptr" back to point at the
	 * stream head!
	 */

	prev_pl = SHEAD_LOCK (sheadp);

	sheadp->sh_linked = NULL;
	sheadp->sh_flags &= ~ SH_PLINK;

	final_close = -- sheadp->sh_open_count == 0;

	SHEAD_UNLOCK (sheadp, prev_pl);


	prev_pl = QFREEZE_TRACE (sheadp->sh_head, "SHEAD_INIT_UNLINKED");

	sheadp->sh_head->q_ptr = sheadp;
	W (sheadp->sh_head)->q_ptr = sheadp;

	QUEUE_INIT (sheadp->sh_head, sheadp->sh_tab, QI_NORMAL);

	QUNFREEZE_TRACE (sheadp->sh_head, prev_pl);

	return final_close;
}


/*
 * This function takes care of unlinking a lower stream from an upper stream.
 *
 * The caller should have both the upper and lower stream heads locked for
 * open/close processing.
 */

#if	__USE_PROTO__
__LOCAL__ int (LOCKED_UNLINK) (shead_t * upper, int mode, shead_t * lower,
			       int cmd, cred_t * credp, int * retvalp)
#else
__LOCAL__ int
LOCKED_UNLINK __ARGS ((upper, mode, lower, cmd, credp, retvalp))
shead_t	      *	upper;
int		mode;
shead_t	      *	lower;
int		cmd;
cred_t	      *	credp;
int	      *	retvalp;
#endif
{
	ASSERT_SLEEP_LOCKED (upper, SH_OPENCLOSE | SH_IOCTL_LOCK);
	ASSERT_SLEEP_LOCKED (lower, SH_OPENCLOSE | SH_IOCTL_LOCK);

	/*
	 * Check that the right kind of unlink command is being issued.
	 */

	if (lower->sh_linked != upper ||
	    ((lower->sh_flags & SH_PLINK) != 0) == (cmd == I_PUNLINK))
		return EINVAL;

	/*
	 * Send the message downstream and get the return result.
	 */

	if (LINK_MESSAGE (upper, mode, lower, cmd, lower->sh_muxid, credp,
			  retvalp)) {
		/*
		 * The unlink was properly acknowledged. Note that this may
		 * cause the lower stream to close.
		 */

		if (SHEAD_INIT_UNLINKED (lower) != 0)
			SHEAD_DO_CLOSE (lower, mode, credp);

		return 1;
	}

	return 0;
}


/*
 * This function wraps up part of the required client functionality for
 * callers of SHEAD_UNLINK () by dealing with locking the lower stream head
 * and making appropriate calls to see if it needs closing.
 */

#if	__USE_PROTO__
__LOCAL__ int (SHEAD_UNLINK) (shead_t * upper, shead_t * lower, int cmd,
			      int mode, cred_t * credp, int * retvalp)
#else
__LOCAL__ int
SHEAD_UNLINK __ARGS ((upper, lower, cmd, mode, credp, retvalp))
shead_t	      *	upper;
shead_t	      *	lower;
int		cmd;
int		mode;
cred_t	      *	credp;
int	      *	retvalp;
#endif
{
	int		success;

	/*
	 * In order to unlink this lower stream we first have to lock it. This
	 * is necessary not only to ensure that we don't trip up other
	 * operations affecting this stream (since there may be open file
	 * descriptors referring to it, and/or it may be a stream pipe) but
	 * since stream deallocations are checked for in the unlock code this
	 * is necessary to ensure that an unlink of an otherwise unreferenced
	 * stream does the right thing.
	 */

	if ((* retvalp = SHEAD_SLEEP_LOCK (lower,
					   SH_OPENCLOSE | SH_IOCTL_LOCK, 0,
					   DONT_SIGNAL)) != 0) {

		cmn_err (CE_WARN, "Unable to lock stream in SHEAD_UNLINK (), error %d",
			 * retvalp);
		return 1;
	}


	/*
	 * OK, we unlink this lower stream, and do all the stuff we
	 * need to do to ensure that the lower stream gets closed if
	 * its time has come.
	 */

	success = LOCKED_UNLINK (upper, mode, lower, cmd, credp, retvalp);

	SHEAD_SLEEP_UNLOCK (lower, SH_OPENCLOSE | SH_IOCTL_LOCK);

	return success;
}


/*
 * Loop-detection algorithm for use in SHEAD_LINK ().
 *
 * The STREAMS documentation is very vague about what constitutes a cycle in
 * the link graph, probably deliberately.
 *
 * For our purposes we consider a cycle to be caused by any path of links
 * which lead upwards (to user level) from the *device* indicated by "upper"
 * to the *device* indicated by "lower".
 *
 * Frankly, given the nature of multiplexing drivers in terms of routing
 * information, I really don't see much advantage in this; it is perfectly
 * easy for messages cycles to form other ways, and given the typical nature
 * of multiplexor services interfaces a cycle in the hierarchy need not cause
 * a loop. Still, that's the way it's specified.
 *
 * THIS IS A RECURSIVE ALGORITHM FOR DEPTH-FIRST SEARCH. This fact does not
 * really worry me in the slightest, because the depth of the recursion here
 * is no worse than the possible recursive depth of the multiplexor put ()
 * calls themselves.
 */

#if	__USE_PROTO__
__LOCAL__ int (DETECT_LOOP) (shead_t * upper, shead_t * lower)
#else
__LOCAL__ int
DETECT_LOOP __ARGS ((upper, lower))
shead_t	      *	upper;
shead_t	      *	lower;
#endif
{
	shead_t	      *	scan;

	/*
	 * We base our comparisons on the "sh_tab" member of the stream head
	 * structure, since that is equivalent to the major part of the
	 * (internal) device number.
	 */

	if (upper->sh_tab == lower->sh_tab)
		return 1;

	/*
	 * Now recursively work upward through the multiplexing configuration
	 * calling DETECT_LOOP () for all the linked streams with the "sh_tab"
	 * entry of the stream which "upper" is linked to (if "upper" is in
	 * fact linked below another multiplexor).
	 *
	 * Since only device streams can be multiplexors, we only scan the
	 * device stream list.
	 */

	if ((upper = upper->sh_linked) == NULL)
		return 0;

	for (scan = str_mem->sm_streams [DEV_SLIST] ; scan != NULL ;
	     scan = scan->sh_next) {

		if (scan->sh_tab == upper->sh_tab &&
		    DETECT_LOOP (scan, lower))
			return 1;
	}

	return 0;
}


/*
 * This function deals with linking one stream below another. There are
 * numerous conditions which might prevent this from happening, including a
 * refusal from the multiplexing driver.
 *
 * The upper stream must be locked for open/close operations. The lower stream
 * will also be locked by this routine.
 */

#if	__USE_PROTO__
int (SHEAD_LINK) (shead_t * upper, int mode, shead_t * lower, int cmd,
		  cred_t * credp)
#else
int
SHEAD_LINK __ARGS ((upper, mode, lower, cmd, credp))
shead_t	      *	upper;
int		mode;
shead_t	      *	lower;
int		cmd;
cred_t	      *	credp;
#endif
{
	int		retval;
	muxid_t		muxid;

	ASSERT (lower != NULL);

	ASSERT_SLEEP_LOCKED (upper, SH_OPENCLOSE | SH_IOCTL_LOCK);

	if (SHEAD_IS_PIPE (upper) ||
	    upper->sh_tab->st_muxrinit == NULL ||
	    upper->sh_tab->st_muxwinit == NULL)
		return EINVAL;

	/*
	 * Perform some of the non-recursive setup (mainly locking) for the
	 * cycle-detection algorithm. We use the read/write lock on the stream
	 * head since the detection algorithm walks over the global stream
	 * list many times. Note that this single-threads checking operations,
	 * which we would also have to do if were were pushing marker bits
	 * around in a non-recursive implementation.
	 */

	(void) RW_WRLOCK (str_mem->sm_head_lock, plstr);

	retval = DETECT_LOOP (upper, lower);

	RW_UNLOCK (str_mem->sm_head_lock, plbase);

	if (retval != 0)
		return EINVAL;

	/*
	 * Generate a suitable multiplexor ID for the link. We use the device
	 * number of the lower stream as a suitable seed point.
	 */

	for (muxid = (muxid_t) lower->sh_dev ;
	     SHEAD_FIND_MUXID (upper, cmd, muxid) != NULL ; muxid ++)
		; /* DO NOTHING */

	/*
	 * We set up the lower now, before we send the I_LINK, so that by the
	 * time the driver sees the I_LINK the stream is ready for use. We
	 * NULL out the "q_ptr" member of the lower queue so that messages
	 * arriving early can be correctly handled.
	 *
	 * We take a sleep lock on the lower stream. If we can't lock it,
	 * return EINVAL since whatever error caused the failure isn't really
	 * relevant to the stream the caller is dealing with.
	 */

	if ((retval = SHEAD_SLEEP_LOCK (lower, SH_OPENCLOSE | SH_IOCTL_LOCK,
					0, DONT_SIGNAL)) != 0)
		return EINVAL;

	/*
	 * Note that the "q_ptr" field of the queue gets zeroed to avoid
	 * communicating our state to the driver. In addition, we have to
	 * count the link as an extra open now.
	 *
	 * Paranoia time; we check for errors, hangups and whether the lower
	 * stream is linked at this late stage so the cutover is atomic. This
	 * requires some cooperation from the stream head read side service
	 * routine; look to see that it tests for "sh_linked" in the service
	 * routine somewhere...
	 */

	(void) SHEAD_LOCK (lower);

	if (SHEAD_HANGUP (lower) || _shead_error (lower, FREAD | FWRITE)) {

		SHEAD_UNLOCK (lower, plbase);
		SHEAD_SLEEP_UNLOCK (lower, SH_OPENCLOSE | SH_IOCTL_LOCK);
		return EINVAL;
	}

	lower->sh_open_count ++;	/* duplicate reference */

	lower->sh_linked = upper;
	lower->sh_muxid = muxid;

	if (cmd == I_PLINK)
		lower->sh_flags |= SH_PLINK;

	SHEAD_UNLOCK (lower, plbase);


	(void) QFREEZE_TRACE (lower->sh_head, "LINK_STREAMS");

	lower->sh_head->q_ptr = NULL;
	W (lower->sh_head)->q_ptr = NULL;

	QUEUE_INIT (lower->sh_head, lower->sh_tab, QI_MUX);

	QUNFREEZE_TRACE (lower->sh_head, plbase);


	/*
	 * Send the message downstream and get the return result.
	 */

	if (LINK_MESSAGE (upper, mode, lower, cmd, muxid, credp,
			  & retval) == 0) {
		int		final;

		/*
		 * The driver failed the link; restore the lower stream. This
		 * won't cause the stream to close (because of our open
		 * reference) but we take care to ensure that the counts are
		 * properly maintained.
		 */

		final = SHEAD_INIT_UNLINKED (lower);

		if (final != 0)
			cmn_err (CE_WARN, "Final close in SHEAD_LINK () ????");
	}


	/*
	 * Unlock the lower stream for proper symmetry.
	 */

	SHEAD_SLEEP_UNLOCK (lower, SH_OPENCLOSE | SH_IOCTL_LOCK);
	return retval;
}


/*
 * This function deals with a special case in SHEAD_DO_CLOSE () below where
 * the last close of a stream pipe end causes the other end to be detached
 * from all the filesystem entries it has been mounted over.
 */

#if	__USE_PROTO__
__LOCAL__ void (SHEAD_PIPE_DETACH) (shead_t * __NOTUSED (other))
#else
__LOCAL__ void
SHEAD_PIPE_DETACH __ARGS ((other))
shead_t	      *	other;
#endif
{
	/*
	 * Until we know how attachments are going to be performed and what
	 * kind of structure exists, we cannot implement this function.
	 */

	cmn_err (CE_PANIC, "UNIMPLEMENTED : SHEAD_PIPE_DETACH ()");
}


/*
 * This module factors out some code from SHEAD_DO_CLOSE (). The final
 * close processing for a stream head is complicated a little because when a
 * stream pipe end is closed that side of the pipe is not actually closed
 * until the other end also closes.
 */

#if	__USE_PROTO__
__LOCAL__ void (SHEAD_FINAL_CLOSE) (shead_t * sheadp, int mode,
				    cred_t * credp)
#else
__LOCAL__ void
SHEAD_FINAL_CLOSE __ARGS ((sheadp, mode, credp))
shead_t	      *	sheadp;
int		mode;
cred_t	      *	credp;
#endif
{
	shead_t	      *	scan;
	queue_t	      *	q;
	int		retval;

	/*
	 * Final close of a stream; the close steps are to be performed in
	 * this order, as given in the DDI/DDK entry for close(D2DK).
	 *	Non-persistent multiplexor links are unlinked.
	 *	For each module and driver from the head to the driver:
	 *		Wait for the write queue to drain.
	 *		Call close () routine.
	 *		Remove module/driver from stream.
	 *		Free remaining messages.
	 *		Deallocate queue pair.
	 *
	 * Here we can behave as if we have an ioctl () lock on the stream
	 * because during final close no other process can legitimately hold
	 * another lock on the stream head (no other legal references to the
	 * stream exist).
	 */

	while ((scan = SHEAD_FIND_MUXID (sheadp, I_UNLINK, -1)) != NULL) {

		if (SHEAD_UNLINK (sheadp, scan, I_UNLINK, mode, credp,
				  & retval) == 0) {
			/*
			 * There is no good reason for a driver to fail an
			 * I_UNLINK request.
			 */

			cmn_err (CE_WARN, "Driver failed unlink () during final close (%d)",
				 retval);
			scan->sh_linked = NULL;
		}
	}


	/*
	 * Before draining the write side modules, drain the stream head.
	 */

	DRAIN_QUEUE (sheadp, W (sheadp->sh_head));


	/*
	 * Now pop all the modules from the stream.
	 */

	while ((q = TOP_MODULE (sheadp)) != NULL) {

		DRAIN_QUEUE (sheadp, W (q));

		(void) POP_MODULE (sheadp, q, mode, credp);
	}


	/*
	 * Now close the driver, unless this is a pipe. Once this has been
	 * done there is no reason for any context to reference this object
	 * except to unlock it.
	 */

	if (! SHEAD_IS_PIPE (sheadp)) {
		q = TOP_QUEUE (sheadp);

		DRAIN_QUEUE (sheadp, q);

		q = R (q);

		retval = (* q->q_qinfo->qi_qclose) (q, mode, credp);

		if (retval != 0)
			cmn_err (CE_WARN, "Driver close returned %d", retval);

		if ((q->q_flag & QPROCSOFF) == 0) {

			cmn_err (CE_WARN, "Driver %s did not call qprocsoff ()",
				 q->q_qinfo->qi_minfo->mi_idname);
			qprocsoff (q);
		}
	}
}


/*
 * This function does most of the close processing for a stream head. It is
 * used by regular stream close, and by stream open code if certain
 * irregularities are detected.
 *
 * The caller must have the stream head locked for close operations. If the
 * stream head being operated on is a pipe, that means both ends must be
 * locked (so that the caller can sequence the lock order on the basis of the
 * master/slave bits).
 */

#if	__USE_PROTO__
__LOCAL__ int (SHEAD_DO_CLOSE) (shead_t * sheadp, int mode, cred_t * credp)
#else
__LOCAL__ int
SHEAD_DO_CLOSE __ARGS ((sheadp, mode, credp))
shead_t	      *	sheadp;
int		mode;
cred_t	      *	credp;
#endif
{
	ASSERT (credp != NULL);
	ASSERT_SLEEP_LOCKED (sheadp, SH_OPENCLOSE | SH_IOCTL_LOCK);

	/*
	 * If other processes have references to this stream head, then we can
	 * just return doing no more work. Note that we keep the notion of
	 * number of open references, number of filesystem attachments, and
	 * multiplexor links completely separate, but any one qualifies as
	 * a reason for keeping the stream around.
	 *
	 * Therefore, it follows that any code which manipulates any of these
	 * quantities such that the next expression might become false needs
	 * to call this function to ensure that the stream memory will be
	 * properly reclaimed.
	 */

	ASSERT (sheadp->sh_open_count == 0);

	if (SHEAD_IS_PIPE (sheadp)) {
		shead_t	      *	other = SHEAD_OTHER (sheadp);

		/*
		 * If this is a pipe, there are some extra things we should
		 * worry about. First, the first end of a stream pipe to close
		 * should send an M_HANGUP message to the other end rather
		 * than actually closing. The other end then gets to destroy
		 * both sides of the pipe when it finally closes.
		 *
		 * The second exception is that when an unattached end of a
		 * streams pipe is closed, the other end is automatically
		 * detached.
		 *
		 * Note that if the other end of the pipe is linked below a
		 * multiplexor, we do not attempt to automatically unlink it.
		 * This might seem entirely reasonable, but we cannot attempt
		 * this without introducing a possibility of deadlock.
		 */

		if (other->sh_attach_count > 0)
			SHEAD_PIPE_DETACH (other);

		if (other->sh_open_count > 0) {
			/*
			 * The last close/detach/unlink of the other end will
			 * clean everything up.
			 */

			putctl (W (other->sh_head), M_HANGUP);
			return 0;
		}

		/*
		 * Now we clean up the "other" end.
		 */

		SHEAD_FINAL_CLOSE (other, mode, credp);
	}

	SHEAD_FINAL_CLOSE (sheadp, mode, credp);

	/*
	 * We leave the last part of the cleanup (dallocating the queue pair
	 * and any remaining messages) to the unlock call which the caller
	 * will perform.
	 */

	return 0;
}


/*
 * This function builds and send an M_FLUSH message down the stream.
 */

#if	__USE_PROTO__
__LOCAL__ int (SHEAD_FLUSH) (shead_t * sheadp, int flag, uchar_t band)
#else
__LOCAL__ int
SHEAD_FLUSH __ARGS ((sheadp, flag, band))
shead_t	      *	sheadp;
int		flag;
uchar_t		band;
#endif
{
	mblk_t	      *	msg;

	if ((flag & FLUSHRW) == 0 || (flag & ~ FLUSHRW) != 0)
		return EINVAL;
	else if ((msg = MSGB_ALLOC (2, BPRI_LO, KM_SLEEP)) == NULL)
		return ENOSR;
	else {
		/*
		 * Send the message downstream...
		 */

		if (band > 0)
			flag |= FLUSHBAND;

		msg->b_datap->db_type = M_FLUSH;
		* msg->b_wptr ++ = (unsigned char) flag;

		if (band > 0)
			* msg->b_wptr ++ = band;

		put (W (sheadp->sh_head), msg);
	}

	return 0;
}


/*
 * This function locates a module's streamtab entry given the name. Only the
 * first FMNAMESZ characters are considered significant.
 */

#if	__USE_PROTO__
modsw_t * (FIND_MODULE) (__CONST__ char * modname)
#else
modsw_t *
FIND_MODULE __ARGS ((modname))
__CONST__ char * modname;
#endif
{
	modsw_t	      *	scan;
	modsw_t	      *	end;

	for (scan = modsw, end = scan + nmodsw ; scan != end ; scan ++) {
		__CONST__ char * name;

		name = scan->mod_stream->st_rdinit->qi_minfo->mi_idname;

		if (name != NULL && strncmp (modname, name, FMNAMESZ) == 0)
			return scan;
	}

	return NULL;
}


/*
 * This function holds code to process an I_STR ioctl ().
 */

#if	__USE_PROTO__
__LOCAL__ int (ISTR_IOCTL) (shead_t * sheadp, int mode,
			    struct strioctl * strioc, cred_t * credp,
			    int * rvalp)
#else
__LOCAL__ int
ISTR_IOCTL __ARGS ((sheadp, mode, strioc, credp, rvalp))
shead_t	      *	sheadp;
int		mode;
struct strioctl
	      *	strioc;
cred_t	      *	credp;
int	      *	rvalp;
#endif
{
	__clock_t	ticks;
	mblk_t	      *	msg;
	mblk_t	      *	data;
	struct iocblk *	ioc;
        int		retval;

	ASSERT_SLEEP_LOCKED (sheadp, SH_IOCTL_LOCK);

	/*
	 * Set up timeout; "ic_timeout" == -1 means infinite
	 * timeout, while "ic_timeout" == 0 means default
	 * (which is infinite).
	 */

	if (strioc->ic_timeout != -1 && strioc->ic_timeout != 0) {
		/*
		 * We take care to deal with overflow here.
		 */

		if ((ticks = strioc->ic_timeout) > (__clock_t) -1 / 1000000L)
			ticks = (__clock_t) -1;

		ticks = drv_usectohz (ticks * 1000000L);
	} else
		ticks = 0;

	if ((msg = MSGB_ALLOC (sizeof (* ioc), BPRI_LO, KM_SLEEP)) == NULL)
		return ENOSR;

	ioc = (struct iocblk *) msg->b_rptr;
	msg->b_wptr = (unsigned char *) (ioc + 1);

	msg->b_datap->db_type = M_IOCTL;

	ioc->ioc_cmd = strioc->ic_cmd;
	ioc->ioc_cr = credp;
	ioc->ioc_id = ++ sheadp->sh_ioc_seq;
	ioc->ioc_count = strioc->ic_len;
	ioc->ioc_rval = ioc->ioc_error = 0;


	/*
	 * If there is data to be sent downstream, we allocate a buffer to
	 * hold it and copy the data into that buffer.
	 */

	if (ioc->ioc_count > 0) {

		if ((data = MSGB_ALLOC (strioc->ic_len, BPRI_LO,
					KM_SLEEP)) == NULL) {

			retval = ENOSR;
			goto done;
		}

		msg->b_cont = data;
		data->b_wptr += strioc->ic_len;

		if (copyin (strioc->ic_dp, data->b_rptr,
			    strioc->ic_len) != 0) {

			retval = EFAULT;
			goto done;
		}
	}

	/*
	 * Now we send the ioctl () and wait for the acknowledgement.
	 */

	if ((msg = IOCTL_SEND (sheadp, mode, msg, & retval, ticks)) == NULL)
		return retval;


	/*
	 * What do we do with the results? We copy at most "ioc_count" bytes
	 * of data back into the user's address space if this is M_IOCACK.
	 */

	ioc = (struct iocblk *) msg->b_rptr;

	switch (msg->b_datap->db_type) {

	case M_IOCNAK:
		retval = ioc->ioc_error;
		break;

	case M_IOCACK:
		* rvalp = ioc->ioc_rval;
		retval = ioc->ioc_error;

		data = msg->b_cont;

		while (ioc->ioc_count > 0) {
			size_t		len;

			if (data == NULL) {

				cmn_err (CE_WARN, "ISTR_IOCTL : Insufficient M_DATA blocks for ioc_count");
				break;
			}

			len = data->b_wptr - data->b_rptr;

			if (len > ioc->ioc_count)
				len = ioc->ioc_count;

			if (len > 0 &&
			    copyout (data->b_rptr, strioc->ic_dp,
				     len) != 0) {

				retval = EFAULT;
				goto done;
			}

			ioc->ioc_count -= len;
			data = data->b_cont;
			strioc->ic_dp += len;
		}
		break;

	default:
		cmn_err (CE_WARN, "Invalid message type %d received during unlink processing",
			 msg->b_datap->db_type);
		retval = ENXIO;
		break;
	}

done:
	freemsg (msg);

	return retval;
}


/*
 * Here are the details of the implementation of sigpoll_t.
 */

struct sigpoll {
	sigpoll_t     *	sp_next;	/* single-threaded */
	_VOID	      *	sp_proc;	/* from proc_ref () */
	short		sp_events;	/* events to signal per <stropts.h> */
};


/*
 * This function is used when an M_SIG or M_PCSIG message is processed at the
 * stream head.
 */

#if	__USE_PROTO__
void (SHEAD_SIGNAL) (shead_t * sheadp, uchar_t signal)
#else
void
SHEAD_SIGNAL __ARGS ((sheadp, signal))
shead_t	      *	sheadp;
uchar_t		signal;
#endif
{
	pl_t		prev_pl;

	prev_pl = SHEAD_LOCK (sheadp);

	if (signal == SIGPOLL) {
		sigpoll_t     *	sigs;

		/*
		 * SIGPOLL is only sent to those processes that have
		 * registered to receive it with I_SETSIG.
		 */


		for (sigs = sheadp->sh_sigs ; sigs != NULL ;
		     sigs = sigs->sp_next) {

			if ((sigs->sp_events & S_MSG) != 0)
				proc_signal (sigs->sp_proc, signal);
		}
	} else {
		/*
		 * Send a signal to the controlling process group for this
		 * stream; if this stream is not a controlling tty, then no
		 * signal is sent.
		 */

		if (sheadp->sh_pgrp != 0)
			proc_kill_group (sheadp->sh_pgrp, signal);
	}

	SHEAD_UNLOCK (sheadp, prev_pl);
}


/*
 * This function encapsulates all user-level access to the front of the stream
 * head message queue. It deals with ensuring that the STREAMS scheduling
 * policy works (by managing QWANTR) and some other details such as dealing
 * with in-band processing of M_SIG messages.
 *
 * The stream head read queue should be frozen by the caller.
 */

#if	__USE_PROTO__
__LOCAL__ mblk_t * (SHEAD_FIRSTMSG) (shead_t * sheadp)
#else
__LOCAL__ mblk_t *
SHEAD_FIRSTMSG __ARGS ((sheadp))
shead_t	      *	sheadp;
#endif
{
	mblk_t	      *	msg;

	QFROZEN_TRACE (sheadp->sh_head, "SHEAD_FIRSTMSG");

	while ((msg = sheadp->sh_head->q_first) != NULL) {

		if (msg->b_datap->db_type != M_SIG)
			return msg;

		rmvq (sheadp->sh_head, msg);
		SHEAD_SIGNAL (sheadp, * msg->b_rptr);
		freemsg (msg);
	}

	sheadp->sh_head->q_flag |= QWANTR;

	return NULL;
}


/*
 * This function does the necessary work to implement POLLWRBAND, which tests
 * to see if any of the previously written-to bands of flow in the next
 * downstream queue with a service procedure are not flow controlled.
 */

#if	__USE_PROTO__
__LOCAL__ int (POLL_WRBAND) (queue_t * q)
#else
__LOCAL__ int
POLL_WRBAND __ARGS ((q))
queue_t	      *	q;
#endif
{
	pl_t		prev_pl;
	qband_t	      *	qbandp;

	prev_pl = QFREEZE_TRACE (q, "POLL_WRBAND");

	do {
		q = QUEUE_NEXT (q);

		if (q == NULL)
			return 1;

	} while (q->q_qinfo->qi_srvp == NULL);


	/*
	 * We have found a queue with a service procedure, and have it frozen.
	 * If there are no bands, then we can write a band...
	 */

	if (q->q_nband == 0) {

		QUNFREEZE_TRACE (q, prev_pl);
		return 1;
	}

	qbandp = QUEUE_BAND (q, q->q_nband);

	ASSERT (qbandp != NULL);

	/*
	 * Since if a band is blocked implies lower bands are blocked, we can
	 * simply test the highest-numbered band.
	 */

	if ((qbandp->qb_flag & QB_FULL) != 0) {

		QUNFREEZE_TRACE (q, prev_pl);
		return 1;
	}


	/*
	 * If all are full, request notification via back-enabling when the
	 * highest band becomes writeable.
	 */

	qbandp->qb_flag |= QB_WANTW;

	QUNFREEZE_TRACE (q, prev_pl);
	return 0;
}


/*
 * This function has responsibility for checking to see whether the event mask
 * for this SIGPOLL request is immediately satisfied.
 */

#if	__USE_PROTO__
__LOCAL__ short (SHEAD_POLL_CHECK) (shead_t * sheadp, short events)
#else
__LOCAL__ short
SHEAD_POLL_CHECK __ARGS ((sheadp, events))
shead_t	      *	sheadp;
short		events;
#endif
{
	short		revents = 0;
	pl_t		prev_pl;
	mblk_t	      *	msg;

	/*
	 * Check the conditions that do not depend on the status of the first
	 * queued message (if any).
	 *
	 * For S_OUTPUT, using canputnext () is important because it sets the
	 * back-enable flag so that we will be properly notified when the
	 * condition becomes true.
	 *
	 * For S_WRBAND, things are a little more complex; this tests whether
	 * *any* downstream priority band is writeable, which involves walking
	 * over all the 'qband' structures allocated to the next stream with
	 * a service procedure.
	 */

	if ((events & __POLL_OUTPUT) != 0 && canputnext (W (sheadp->sh_head)))
		revents |= __POLL_OUTPUT;

	if ((events & __POLL_WRBAND) != 0 &&
	    POLL_WRBAND (W (sheadp->sh_head)))
		revents |= __POLL_WRBAND;


	prev_pl = SHEAD_LOCK (sheadp);

	if ((events & S_ERROR) != 0 &&
	    (sheadp->sh_rerrcode != 0 || sheadp->sh_werrcode != 0))
		revents |= POLLERR;

	if ((events & S_HANGUP) != 0 && (sheadp->sh_flags & SH_HANGUP) != 0)
		revents = (revents | POLLHUP) &
				~ (__POLL_OUTPUT | __POLL_WRBAND);

	SHEAD_UNLOCK (sheadp, prev_pl);


	prev_pl = QFREEZE_TRACE (sheadp->sh_head, "SHEAD_POLL_CHECK");

	if ((msg = SHEAD_FIRSTMSG (sheadp)) != NULL &&
	    datamsg (msg->b_datap->db_type)) {

		if (! pcmsg (msg->b_datap->db_type)) {

			if ((events & __POLL_INPUT) != 0)
				revents |= __POLL_INPUT;

			if ((events & __POLL_RDNORM) != 0 && msg->b_band == 0)
				revents |= __POLL_RDNORM;

			if ((events & __POLL_RDBAND) != 0 && msg->b_band > 0)
				revents |= __POLL_RDBAND;
		} else if ((events & __POLL_HIPRI) != 0)
			revents |= __POLL_HIPRI;
	}

	QUNFREEZE_TRACE (sheadp->sh_head, prev_pl);

	return revents;
}


/*
 * This function attempts to locate any sigpoll record for the current
 * process. The stream head needs to be locked out against modification of
 * the signal list, which means an IOCTL lock is sufficient.
 */

#if	__USE_PROTO__
__LOCAL__ sigpoll_t * (FIND_SIGPOLL) (shead_t * sheadp)
#else
__LOCAL__ sigpoll_t *
FIND_SIGPOLL __ARGS ((sheadp))
shead_t	      *	sheadp;
#endif
{
	_VOID	      *	proc;
	sigpoll_t     *	scan;

	ASSERT_SLEEP_LOCKED (sheadp, SH_IOCTL_LOCK);

	/*
	 * See if the process is already registered (so that we can modify or
	 * free an existing record).
	 */

	proc = proc_ref ();

	for (scan = sheadp->sh_sigs ; scan != NULL ; scan ++)
		if (scan->sp_proc == proc)
			break;

	proc_unref (proc);

	return scan;
}


/*
 * This function deals with (de)registering a process for SIGPOLL. The caller
 * should have an IOCTL lock on the stream head so that there are no other
 * contexts which could modify the list of registered signals.
 */

#if	__USE_PROTO__
__LOCAL__ int (REGISTER_SIGPOLL) (shead_t * sheadp, short events)
#else
__LOCAL__ int
REGISTER_SIGPOLL __ARGS ((sheadp, events))
shead_t	      *	sheadp;
short		events;
#endif
{
	_VOID	      *	proc;
	sigpoll_t     *	scan;
	sigpoll_t     *	prev;

	ASSERT_SLEEP_LOCKED (sheadp, SH_IOCTL_LOCK);

	/*
	 * See if the process is already registered (so that we can modify or
	 * free an existing record). We don't use the FIND_SIGPOLL () routine
	 * because we want to locate the previous record also.
	 */

	proc = proc_ref ();

	for (prev = NULL, scan = sheadp->sh_sigs ; scan != NULL ;
	     prev = scan, scan ++) {

		if (scan->sp_proc == proc) {
			/*
			 * We have found a preexisting record... now modify
			 * or free it.
			 */

			proc_unref (proc);

			(void) SHEAD_LOCK (sheadp);

			if ((scan->sp_events = events) == 0) {
				/*
				 * Free the cell after unlinking it from the
				 * list of registered processes. We take out
				 * a lock on the stream head to protect
				 * against streams-level contexts walking the
				 * list.
				 */

				if (prev == NULL)
					sheadp->sh_sigs = scan->sp_next;
				else
					prev->sp_next = scan->sp_next;

				SHEAD_UNLOCK (sheadp, plbase);

				/*
				 * We unlock the stream head before calling
				 * the heap manager as a matter of courtesy.
				 */

				kmem_free (scan, sizeof (* scan));
				proc_unref (proc);
				return 0;
			}

			SHEAD_UNLOCK (sheadp, plbase);
			goto sigcheck;
		}
	}


	if (events == 0) {
		/*
		 * Not found, nothing to do. That is an error according to the
		 * streamio (7) man pages.
		 */

		return EINVAL;
	}


	/*
	 * We need to make a new event.
	 */

	if ((scan = (sigpoll_t *) kmem_alloc (sizeof (* scan),
					      KM_SLEEP)) == NULL) {

		proc_unref (proc);
		return EAGAIN;
	}

	scan->sp_proc = proc;
	scan->sp_events = events;
	scan->sp_next = sheadp->sh_sigs;

	/*
	 * Since we are the only process context modifying the list, we only
	 * need to lock the last stage of the insert against interrupt-level
	 * contexts walking the list.
	 */

	(void) SHEAD_LOCK (sheadp);

	sheadp->sh_sigs = scan;

	SHEAD_UNLOCK (sheadp, plbase);

sigcheck:
	if ((events = SHEAD_POLL_CHECK (sheadp, scan->sp_events)) != 0) {
		/*
		 * We have a winner! Check for the SIGURG special case, and
		 * otherwise/also send SIGPOLL via proc_signal ().
		 */

		if ((events & __POLL_RDBAND) != 0 &&
		    (scan->sp_events & S_BANDURG) != 0) {

			proc_signal (scan->sp_proc, SIGURG);
			events &= ~ __POLL_RDBAND;
		}

		if (events != 0)
			proc_signal (scan->sp_proc, SIGPOLL);
	}

	return 0;
}


/*
 * This function maps a file descriptor into a stream head by calling upon the
 * abstract file-description functions declared in <sys/fhsys.h>. The caller
 * must lock the stream somehow.
 */


#if	__USE_PROTO__
__LOCAL__ shead_t * (FH_TO_STREAM) (int fd, int * retvalp)
#else
__LOCAL__ shead_t *
FH_TO_STREAM __ARGS ((fd, retvalp))
int		fd;
int	      *	retvalp;
#endif
{
	__fd_t	      *	fdp;

	ASSERT (retvalp != NULL);

	if ((fdp = fd_get (fd)) == NULL) {

		* retvalp = EBADF;
		return NULL;
	}

	* retvalp = EINVAL;
	return NULL;
}


/*
 * The following structure is used by the routines below to deal with the
 * M_PASSFP message type. The structure of the data contained in that message
 * is completely opaque to STREAMS routines, so we keep the definition local.
 */

struct passfp {
	__fd_t	      *	fdp;	/* system file table entry address */
	n_uid_t		uid;
	n_gid_t		gid;
};


/*
 * This function attempts to retrieve a file descriptor sent by an I_SENDFD
 * ioctl () and create a local file descriptor referring to the same file.
 */

#if	__USE_PROTO__
__LOCAL__ int (FH_RECV) (shead_t * sheadp, int mode, struct strrecvfd * recvp)
#else
__LOCAL__ int
FH_RECV __ARGS ((sheadp, mode, recvp))
shead_t	      *	sheadp;
int		mode;
struct strrecvfd
	      *	recvp;
#endif
{
	mblk_t	      *	msg;
	struct passfp *	fp;
	int		retval;

	ASSERT (sheadp != NULL);
	ASSERT (recvp != NULL);

	if (! SHEAD_IS_PIPE (sheadp))
		return EINVAL;

	if ((mode & FREAD) == 0)
		return EBADF;

	mode &= ~ FWRITE;


	/*
	 * Look at the stream head to see if a message is present.
	 */

	for (;;) {
		int		retval;

		(void) QFREEZE_TRACE (sheadp->sh_head, "FH_RECV");

		if ((msg = SHEAD_FIRSTMSG (sheadp)) != NULL)
			break;

		/*
		 * Use the STREAMS flag to indicate that we have found the
		 * queue empty.
		 */

		(void) SHEAD_LOCK (sheadp);

		QUNFREEZE_TRACE (sheadp->sh_head, plstr);

		/*
		 * We need to wait (unless O_NDELAY or O_NONBLOCK has been
		 * specified), or the stream has been hung up.
		 */

		if ((retval = SHEAD_WAIT_NONBLOCK (sheadp, mode, SH_READ_WAIT,
						   CHECK_SIGNALS)) != 0)
			return retval;

		/*
		 * Check again...
		 */
	}

	if (msg->b_datap->db_type != M_PASSFP)
		retval = EBADMSG;
	else if ((recvp->fd = fd_get_free ()) == ERROR_FD)
		retval = EMFILE;
	else {

		retval = 0;
		rmvq (sheadp->sh_head, msg);
	}

	QUNFREEZE_TRACE (sheadp->sh_head, plbase);

	if (retval != 0)
		return retval;

	/*
	 * After this point, "msg" is our responsibility and we either have to
	 * free it or put it back if there is an error.
	 */

	fp = (struct passfp *) msg->b_rptr;

	if ((retval = fd_recv (recvp->fd, fp->fdp)) == 0 &&
	    ((recvp->uid = (o_uid_t) fp->uid) != fp->uid ||
	     (recvp->gid = (o_gid_t) fp->gid) != fp->gid))
		retval = EOVERFLOW;

	freemsg (msg);
	return retval;
}


/*
 * This function attempts to send a file descriptor to another process at the
 * other end of a stream pipe.
 */

#if	__USE_PROTO__
__LOCAL__ int (FH_SEND) (shead_t * sheadp, int fd, cred_t * credp)
#else
__LOCAL__ int
FH_SEND __ARGS ((sheadp, fd, credp))
shead_t	      *	sheadp;
int		fd;
cred_t	      *	credp;
#endif
{
	mblk_t	      *	msg;
	struct passfp *	fp;
	__fd_t	      *	fdp;
	shead_t	      *	other;
	int		retval;

	ASSERT (sheadp != NULL);
	ASSERT (credp != NULL);

	if (! SHEAD_IS_PIPE (sheadp))
		return EINVAL;

	/*
	 * We are passed a file descriptor (a user-level abstract entity);
	 * here we turn that into a kernel-level abstract entity.
	 */

	if ((fdp = fd_get (fd)) == NULL)
		return EBADF;

	other = SHEAD_OTHER (sheadp);

	if (! canput (W (other->sh_head)) ||
	    (msg = MSGB_ALLOC (sizeof (* fp), BPRI_LO, KM_SLEEP)) == NULL)
		return EAGAIN;

	/*
	 * Fill in the new message block and put the message to the other
	 * side. After this point we have to free the message block if there
	 * is a problem.
	 */

	fp = (struct passfp *) msg->b_rptr;
	msg->b_wptr = (unsigned char *) (fp + 1);

	msg->b_datap->db_type = M_PASSFP;

	fp->fdp = fdp;
	fp->uid = credp->cr_uid;
	fp->gid = credp->cr_gid;

	(void) SHEAD_LOCK (sheadp);

	if ((retval = SHEAD_ERRHUP_LOCKED (sheadp, FWRITE)) == 0) {

		putq (W (other->sh_head), msg);

		SHEAD_UNLOCK (sheadp, plbase);
	} else
		freemsg (msg);

	return retval;
}


/*
 * This function factors out the details of copying data out from kernel space
 * into a "strbuf" stream buffer structure in user space.
 *
 * The return value of this function is 0 on success or -1 on error.
 */

enum {
	CONTROL_PART,
	DATA_PART
};

#if	__USE_PROTO__
__LOCAL__ int (COPYOUT_BUF) (struct strbuf * bufp, mblk_t ** mpp, int data)
#else
__LOCAL__ int
COPYOUT_BUF __ARGS ((bufp, mpp, data))
struct strbuf *	bufp;
mblk_t	     **	mpp;
int		data;
#endif
{
	mblk_t	      *	prev;
	mblk_t	      *	scan;
	int		remaining;
	caddr_t		outaddr;

	ASSERT (mpp != NULL);

	/*
	 * The first thing we do is check some special values; if a "strbuf"
	 * entry is NULL or has its "maxlen" member set to "-1", we do nothing
	 * with the message.
	 */

	if (bufp == NULL || bufp->maxlen == -1)
		return 0;

	/*
	 * Next, if this is a data-part copy, skip any intial M_PROTO or
	 * M_PCPROTO message blocks. There *should* only ever be one of these
	 * at the front of a message, but we are required to effectively
	 * coalesce multiple control blocks.
	 */

	prev = NULL;
	scan = * mpp;

	if (data == DATA_PART) {
		/*
		 * Find the data portion of the message, if any.
		 */

		while (scan != NULL) {

			if (scan->b_datap->db_type == M_DATA)
				break;
			scan = (prev = scan)->b_cont;
		}
	} else
		if (scan->b_datap->db_type == M_DATA)
			scan = NULL;

	/*
	 * If there is no control (or data, as appropriate) part to the
	 * message, then we set the "len" member of the "strbuf" to -1.
	 */

	if (scan == NULL) {

		bufp->len = -1;
		return 0;
	}


	/*
	 * Now be do the actual copy; the form of this loop is organized so
	 * that zero-length message blocks will be consumed if "maxlen" is
	 * set to 0. This is important not only to comply with the manual page
	 * for getmsg ()/getpmsg (), but also ensures that trailing zero-
	 * length blocks at the end of a message get cleaned up properly.
	 */

	bufp->len = 0;
	remaining = bufp->maxlen;
	outaddr = (caddr_t) bufp->buf;

	for (;;) {
		size_t		copylen = scan->b_wptr - scan->b_rptr;
		mblk_t	      *	next;

		if (copylen > remaining)
			copylen = remaining;

		if (copylen > 0) {
			/*
			 * Copy the data to the user. Don't forget that
			 * copyout () is like bcopy () in that the arguments
			 * are src, dest, len !
			 */

			if (copyout (scan->b_rptr, outaddr, copylen) != 0)
				return EFAULT;

			bufp->len += copylen;
			scan->b_rptr += copylen;
			remaining -= copylen;
		}

		if (scan->b_rptr != scan->b_wptr) {
			/*
			 * Since this message block was not fully consumed, we
			 * can infer that we have copied all the data we can.
			 */

			ASSERT (remaining == 0);
			break;
		}


		/*
		 * This message block has been consumed; unlink and free it.
		 */

		next = scan->b_cont;
		if (prev == NULL)
			* mpp = next;
		else
			prev->b_cont = next;
		freeb (scan);


		/*
		 * Get ready to go around the loop again; if we are copying
		 * the control part of a message, we have to test for the end
		 * of the control part here.
		 */

		if ((scan = next) == NULL)
			break;

		if (data == CONTROL_PART && scan->b_datap->db_type == M_DATA)
			break;
	}

	return 0;
}


/*
 * This is a buffer callback function used by SHEAD_PEEK () to deal with
 * dupmsg () failures via bufcall ().
 */

#if	__USE_PROTO__
__LOCAL__ void peek_bufcall_func (_VOID * arg)
#else
__LOCAL__ void
peek_bufcall_func (arg)
_VOID	      *	arg;
#endif
{
	pl_t		prev_pl;
	shead_t	      *	sheadp = (shead_t *) arg;

	/*
	 * We freeze the stream head read queue to synchronize ourselves with
	 * the SHEAD_PEEK () and avoid race conditions where we try and wake
	 * up the process that scheduled us before they have slept.
	 */

	prev_pl = QFREEZE_TRACE (sheadp->sh_head, "peek_bufcall_func");

	sheadp->sh_read_bufcall = 0;

	SHEAD_WAKE (sheadp, SH_PEEK_WAIT);

	QUNFREEZE_TRACE (sheadp->sh_head, prev_pl);
}


/*
 * This is a helper function for I_PEEK and read ()-like functions who want
 * to recover from an out-of-memory situation by performing a short sleep.
 *
 * This function takes care of scheduling and cancelling the bufcall. The
 * caller must have the stream head locked when calling this function.
 *
 * This function returns with the stream head read queue unlocked, and with
 * the value 0 on success and an error number on error.
 */

#if	__USE_PROTO__
__LOCAL__ int (SHEAD_READ_BUFCALL) (shead_t * sheadp, int __NOTUSED (mode))
#else
__LOCAL__ int
SHEAD_READ_BUFCALL __ARGS ((sheadp, mode))
shead_t	      *	sheadp;
int		mode;
#endif
{
	SHEAD_ASSERT_LOCKED (sheadp);

	if (sheadp->sh_read_bufcall == 0 &&
	    (sheadp->sh_read_bufcall = bufcall (1024, BPRI_LO,
						peek_bufcall_func,
						sheadp)) == 0) {
		SHEAD_UNLOCK (sheadp, plbase);
		return ENOSR;
	}

	return SHEAD_WAIT_NONBLOCK (sheadp, FREAD, SH_PEEK_WAIT,
				    CHECK_SIGNALS);
}


/*
 * This function implements I_PEEK read-ahead for streams. The caller should
 * have the stream head locked for read/write access.
 */

#if	__USE_PROTO__
__LOCAL__ int (SHEAD_PEEK) (shead_t * sheadp, struct strpeek * peek,
			    int * rvalp)
#else
__LOCAL__ int
SHEAD_PEEK __ARGS ((sheadp, peek, rvalp))
shead_t	      *	sheadp;
struct strpeek * peek;
int	      *	rvalp;
#endif
{
	mblk_t	      *	msg;
	int		retval;

	ASSERT (sheadp != NULL);
	ASSERT (peek != NULL);
	ASSERT (rvalp != NULL);

	if (peek->flags != 0 && peek->flags != RS_HIPRI)
		return EINVAL;

	/*
	 * Look at the stream head to see if there are any queued messages.
	 *
	 * Some notes about this are in order; as mentioned in the discussion
	 * on locking elsewhere, the possibility of page fault resolution
	 * during copies to user space complicates life, because copy routines
	 * may sleep while a page is made resident.
	 *
	 * This affects routines which read from the stream head in various
	 * ways, some of which are outlined in the general section on locking.
	 * Other than data consistency, it also affects the way message are
	 * deallocated; this routine wants to take a copy of the data in a
	 * message, but we need to ensure that the message will not be
	 * deallocated not just by other processes but also by actions at the
	 * stream level such as M_FLUSH.
	 *
	 * Other than getting into a web of locking flags and reference counts
	 * (there are already too many of those), the simplest ways of dealing
	 * with this are a) create a duplicate reference to the message data
	 * with dupmsg (), and b) simply dequeue the message and put it back
	 * when we're done.
	 *
	 * Neither alternative is without unpleasant consequences; a) has to
	 * deal with a lack of available storage to duplicate the message
	 * blocks, and b) has to deal with such things as reads blocking while
	 * the message is dequeued.
	 *
	 * a) and b) seem to have more-or-less equal implementation costs, but
	 * while a) is guaranteed to maintain the semantics of all stream
	 * operations, b) is not. Unless we can predict all the consequences
	 * of b) and either work around them or determine that they are benign
	 * then a) seems preferable.
	 */

	for (;;) {
		(void) QFREEZE_TRACE (sheadp->sh_head, "SHEAD_PEEK");

		if ((msg = SHEAD_FIRSTMSG (sheadp)) == NULL) {
no_message:
			/*
			 * No go; arrange for the value 0 to be returned to
			 * the caller of the ioctl ().
			 */

			QUNFREEZE_TRACE (sheadp->sh_head, plbase);
			* rvalp = 0;

			return 0;
		}

		switch (msg->b_datap->db_type) {

		case M_DATA:
		case M_PROTO:
			/*
			 * If the caller has asked for high-priority messages
			 * only, then we arrange to return 0.
			 */

			if ((peek->flags & RS_HIPRI) != 0)
				goto no_message;
			break;

		case M_PCPROTO:
			break;

		default:
			QUNFREEZE_TRACE (sheadp->sh_head, plbase);
			return EBADMSG;
		}


		/*
		 * Attempt to obtain a duplicate reference to this message.
		 */

		if ((msg = dupmsg (msg)) != NULL) {

			QUNFREEZE_TRACE (sheadp->sh_head, plbase);
			break;
		}


		/*
		 * Wait a short time for buffers to be available. To do this,
		 * we transfer our locking attention from the queue to the
		 * stream head.
		 */

		(void) SHEAD_LOCK (sheadp);

		QUNFREEZE_TRACE (sheadp->sh_head, plstr);

		if ((retval = SHEAD_READ_BUFCALL (sheadp, FREAD)) != 0)
			return retval;
	}


	/*
	 * Now we have a duplicate copy of a message to transfer to user
	 * space. From this point on, any attempt to exit from this routine
	 * must take care of freeing this message. However, we don't have to
	 * touch the queue again.
	 */

	peek->flags = msg->b_datap->db_type == M_PCPROTO ? RS_HIPRI : 0;

	retval = (COPYOUT_BUF (& peek->ctlbuf, & msg, CONTROL_PART) != 0 ||
		  COPYOUT_BUF (& peek->databuf, & msg,
			       DATA_PART) != 0) ? EFAULT : 0;

	if (msg != NULL)
		freemsg (msg);

	* rvalp = 1;

	return retval;
}


/*
 * Helper function to atomically read the stream head write offset.
 */

#if	__USE_PROTO__
__LOCAL__ short (SHEAD_WRITEOFFSET) (shead_t * sheadp)
#else
__LOCAL__ short
SHEAD_WRITEOFFSET __ARGS ((sheadp))
shead_t	      *	sheadp;
#endif
{
	pl_t		prev_pl;
	short		opt;

	prev_pl = SHEAD_LOCK (sheadp);

	opt = sheadp->sh_readopt;

	SHEAD_UNLOCK (sheadp, prev_pl);

	return opt;
}


/*
 * This function tests for errors on a stream and optionally also waits for
 * flow control to be relieved.
 */

#if	__USE_PROTO__
__LOCAL__ int (SHEAD_WRITE_TEST) (shead_t * sheadp, int mode, int band,
				   int hipri)
#else
__LOCAL__ int
SHEAD_WRITE_TEST __ARGS ((sheadp, mode, band, hipri))
shead_t	      *	sheadp;
int		mode;
int		band;
int		hipri;
#endif
{
	int		retval;

	do {
		(void) SHEAD_LOCK (sheadp);

		if ((retval = SHEAD_ERRHUP_LOCKED (sheadp, mode)) != 0)
			break;

		if (hipri != 0 || bcanputnext (W (sheadp->sh_head), band)) {

			SHEAD_UNLOCK (sheadp, plbase);
			break;
		}
	} while ((retval = SHEAD_WAIT_NONBLOCK (sheadp, mode, SH_WRITE_WAIT,
						CHECK_SIGNALS)) == 0);

	return retval;
}


/*
 * This function deals with all the grunge of creating a message for sending
 * down a stream. There are many conditions that need to be checked, including
 * whether the message fits within the size limits given by the downstream
 * queue.
 *
 * Since the message is going to be written to the stream, we also deal with
 * waiting for flow control here. We only allocate memory for the user's data
 * when we have an indication that it will be valid to write it... this may
 * not be an optimal choice for a high-performance system with vast amounts
 * of STREAMS buffer space.
 */

#if	__USE_PROTO__
__LOCAL__ mblk_t * (SHEAD_MAKEMSG) (shead_t * sheadp, int mode,
				    __CONST__ struct strbuf * ctlbuf,
				    __CONST__ struct strbuf * databuf,
				    int flags, int band, int * retvalp)
#else
__LOCAL__ mblk_t *
SHEAD_MAKEMSG __ARGS ((sheadp, mode, ctlbuf, databuf, flags, band, retvalp))
shead_t	      *	sheadp;
int		mode;
__CONST__ struct strbuf
	      *	ctlbuf;
__CONST__ struct strbuf
	      *	databuf;
int		flags;
int		band;
int	      *	retvalp;
#endif
{
	queue_t	      *	q;
	int		ctlsize;
	int		datasize;
	int		wroff;
	mblk_t	      *	ctlmsg;
	mblk_t	      *	datamsg;

	ASSERT (sheadp != NULL);
	ASSERT (retvalp != NULL);

	/*
	 * The "flags" value specifies one of the constants MSG_BAND or
	 * MSG_HIPRI. Despite being arranged as flags, only one is allowed to
	 * be given.
	 */

	if (flags != MSG_BAND && flags != MSG_HIPRI) {

		* retvalp = EINVAL;
		return NULL;
	}

	if ((mode & FWRITE) == 0) {

		* retvalp = EBADF;
		return NULL;
	}

	mode &= ~ FREAD;

	if (sheadp->sh_linked != NULL) {

		* retvalp = EINVAL;
		return NULL;
	}


	/*
	 * The absolute maximum possible size for the control or data parts of
	 * a STREAMS message are configured system-wide.
	 */

	ctlsize = ctlbuf == NULL ? -1 : ctlbuf->len;
	datasize = databuf == NULL ? -1 : databuf->len;
	wroff = SHEAD_WRITEOFFSET (sheadp);

	q = TOP_QUEUE (sheadp);

	if (ctlsize > str_mem->sm_maxctlsize ||
	    (datasize + wroff) > str_mem->sm_maxdatasize ||
	    (datasize + wroff) < q->q_minpsz ||
	    (datasize + wroff) > q->q_maxpsz) {

		* retvalp = ERANGE;
		return NULL;
	}


	/*
	 * You can't send a high-priority message without a control part, or a
	 * high-priority message with a non-zero band number.
	 *
	 * Otherwise, if no data at all is specified, then no message will be
	 * sent.
	 */

	if (flags == MSG_HIPRI) {

		if (ctlsize < 0 || band != 0) {

			* retvalp = EINVAL;
			return NULL;
		}
	} else if (datasize < 0 && ctlsize < 0) {

		* retvalp = 0;
		return NULL;
	}


	/*
	 * Let's see if the stream is flow controlled; if it is, we either
	 * block or return EAGAIN depending on the FNDELAY/FNONBLOCK setting.
	 */

	if ((* retvalp = SHEAD_WRITE_TEST (sheadp, mode, band, flags)) != 0)
		return NULL;

	/*
	 * Now we allocate data space for the messages. If the size of a
	 * component is -1, then we don't allocate any space for that part,
	 * otherwise we allocate a component of length 0.
	 *
	 * Don't forget that copyin () has arguments in the bcopy () order,
	 * ie. src, dest, len
	 */

	if (ctlsize >= 0) {
		/*
		 * Special case; the stream head is required to ensure that
		 * the control part of any message has at least 64 bytes of
		 * space. This is specified in the putmsg (2) manual page!
		 */

		if ((ctlmsg = MSGB_ALLOC (ctlsize < 64 ? 64 : ctlsize,
					  BPRI_LO, KM_SLEEP)) == NULL) {
			* retvalp = ENOSR;
			return NULL;
		}

		ctlmsg->b_datap->db_type = flags == MSG_HIPRI ? M_PCPROTO :
								M_PROTO;
		ctlmsg->b_band = band;

		if (ctlsize > 0 &&
		    copyin (ctlbuf->buf, ctlmsg->b_rptr, ctlsize) != 0) {

			freeb (ctlmsg);
			* retvalp = EFAULT;
			return NULL;
		}

		ctlmsg->b_wptr += ctlsize;
	} else
		ctlmsg = NULL;		/* paranoia */

	if (datasize >= 0) {

		if ((datamsg = MSGB_ALLOC (datasize + wroff, BPRI_LO,
					   KM_SLEEP)) == NULL) {
			if (ctlsize >= 0)
				freeb (ctlmsg);

			* retvalp = ENOSR;
			return NULL;
		}

		if (ctlsize < 0)
			ctlmsg = datamsg;
		else
			ctlmsg->b_cont = datamsg;

		datamsg->b_band = band;
		datamsg->b_wptr = datamsg->b_rptr += wroff;

		if (datasize > 0 &&
		    copyin (databuf->buf, datamsg->b_rptr, datasize) != 0) {

			freemsg (ctlmsg);

			* retvalp = EFAULT;
			return NULL;
		}

		datamsg->b_wptr += datasize;
	}


	/*
	 * Since the allocation requests could have blocked for memory to
	 * become available, and the copy requests could have blocked to
	 * resolve page faults, we could actually be a fair way down the track
	 * by now.
	 *
	 * If we were paranoid, we would recheck the flow control parameters
	 * and a bunch of other stuff, but there doesn't seem to be a whole
	 * lot of point to that. As long as we don't use data that could have
	 * changed while we waited, we're fine.
	 */

	* retvalp = 0;
	return ctlmsg;
}


/*
 * This function implements the I_FDINSERT ioctl ().
 */

#if	__USE_PROTO__
__LOCAL__ int (SHEAD_FDINSERT) (shead_t * sheadp, int mode,
				struct strfdinsert * fdinsp)
#else
__LOCAL__ int
SHEAD_FDINSERT __ARGS ((sheadp, mode, fdinsp))
shead_t	      *	sheadp;
int		mode;
struct strfdinsert
	      *	fdinsp;
#endif
{
	int		retval;
	shead_t	      *	other;
	mblk_t	      *	msg;

	ASSERT (sheadp != NULL);
	ASSERT (fdinsp != NULL);

	/*
	 * There are a large number of error conditions to check for this
	 * function. Don't lose sight of the fact that the big set of chained
	 * conditions below computes "other" for us.
	 */
	/*
	 * ALIGNMENT-DEPENDENT CODE.
	 */

	if ((fdinsp->flags != 0 && fdinsp->flags != RS_HIPRI) ||
	    ((unsigned) fdinsp->offset & ~ sizeof (int)) != 0 ||
	    fdinsp->offset + sizeof (queue_t *) > fdinsp->ctlbuf.len ||
	    (other = FH_TO_STREAM (fdinsp->fildes, & retval)) != 0) {
		/*
		 * A failure of any of the above returns EINVAL; according to
		 * streamio (7) EINVAL rather than EBADF results from a bad
		 * file descriptor.
		 */

		return EINVAL;
	}

	/*
	 * Make a band 0 message (possibly high-priority). This can fail if
	 * the requested message is too large for the advertised limits set
	 * by the next thing downstream.
	 *
	 * IMPORANT: we *rely* on this function making a message with a single
	 * large control block at least as big as the advertised size. If we
	 * cannot rely on this function we have to do lots of extra checking
	 * which I'd rather avoid.
	 */

	msg = SHEAD_MAKEMSG (sheadp, mode, & fdinsp->ctlbuf,
			     & fdinsp->databuf,
			     fdinsp->flags ? MSG_HIPRI : MSG_BAND, 0,
			     & retval);
	if (msg == NULL)
		return retval;

	/*
	 * Note that we don't recheck for hangups even though we could have
	 * waited a poentially long time in SHEAD_MAKEMSG (). The hangup
	 * condition has plenty of slop in it with the time it takes write
	 * messages to move down the queue anyway; drivers and modules have to
	 * be able to cope.
	 */

	* (queue_t **) (msg->b_rptr + fdinsp->offset) = other->sh_head;

	putq (W (sheadp->sh_head), msg);

	return 0;
}


/*
 * This function implements the I_FIND ioctl ().
 */

#if	__USE_PROTO__
__LOCAL__ int (SHEAD_FIND_MODINFO) (shead_t * sheadp, char * modname,
				    int * rvalp)
#else
__LOCAL__ int
SHEAD_FIND_MODINFO __ARGS ((sheadp, modname, rvalp))
shead_t	      *	sheadp;
char	      *	modname;
int	      *	rvalp;
#endif
{
	modsw_t	      *	module;

	/*
	 * First try to find the module in the global list of modules, then
	 * attempt to find that module's info on the stream.
	 */

	if ((module = FIND_MODULE (modname)) == NULL)
		return EINVAL;
	else {
		queue_t	      *	scan;
		pl_t		prev_pl;

		/*
		 * Walk down the write side of the stream until either the
		 * write side module info matches -or- a cross-point is found.
		 */

		scan = W (sheadp->sh_head);

		prev_pl = SHEAD_LOCK (sheadp);

		while (scan->q_next != NULL) {
			if ((scan->q_flag & QREADR) !=
			    (scan->q_next->q_flag & QREADR)) {
				/*
				 * Set scan to NULL to flag an unsuccessful
				 * search.
				 */

				scan = NULL;
				break;
			}

			scan = scan->q_next;

			if ((scan->q_flag & QPROCSOFF) != 0)
				continue;

			if (scan->q_qinfo == module->mod_stream->st_wrinit)
				break;
		}

		SHEAD_UNLOCK (sheadp, prev_pl);

		* rvalp = scan != NULL;
	}

	return 0;
}


/*
 * This function implements the I_LIST ioctl () for the case where the user
 * supplies a buffer to copy the module/driver names into.
 */

#if	__USE_PROTO__
__LOCAL__ int (SHEAD_LIST) (shead_t * sheadp, struct str_list * slistp,
			    int * rvalp)
#else
__LOCAL__ int
SHEAD_LIST __ARGS ((sheadp, slistp, rvalp))
shead_t	      *	sheadp;
struct str_list
	      *	slistp;
int	      *	rvalp;
#endif
{
	queue_t	      *	scan;
	int		modcount;
	int		i;
	struct str_mlist
		      *	buf;
	struct str_mlist
		      *	temp;

	if (slistp->sl_nmods < 1)
		return EINVAL;

	/*
	 * The EAGAIN error documented for I_LIST in streamio (7) is
	 * suggestive of how it is implemented; rather than get involved in
	 * tricky synchronization issues, copy the module names into a kernel
	 * buffer and then copy that to user level.
	 */

	modcount = SHEAD_MODCOUNT (sheadp);

	if (modcount > slistp->sl_nmods)
		modcount = slistp->sl_nmods;


	/*
	 * If we were paranoid, we'd user kmem_zalloc () to ensure that the
	 * data we copy to user space contains no sensitive information. As it
	 * happens, because of the fact we user strncpy () to fill the buffer
	 * with data, we are guaranteed that we have overwritten all of the
	 * contents.
	 */

	if ((buf = (struct str_mlist *) kmem_alloc (modcount * sizeof (* buf),
						    KM_SLEEP)) == NULL)
		return EAGAIN;

	/*
	 * Now fill the buffer in by moving down the stream. We don't worry
	 * about the race between the calculation of the buffer size and the
	 * time we fill it in, because the problem also exists for the user;
	 * in order to know how much space to allocate at user level, some
	 * arrangement must have been made to ensure things are stable.
	 */

	i = 0;
	scan = W (sheadp->sh_head);
	temp = buf;

	(void) SHEAD_LOCK (sheadp);

	while (scan->q_next != NULL &&
	       ((scan->q_flag & QREADR) == (scan->q_next->q_flag & QREADR))) {

		scan = scan->q_next;

		if ((scan->q_flag & QPROCSOFF) != 0)
			continue;

		if (i ++ > modcount)
			break;

		/*
		 * Now actually copy the module name. Note that we count on
		 * strncpy () null-padding the target for security.
		 */

		strncpy (temp->l_name, scan->q_qinfo->qi_minfo->mi_idname,
			 sizeof (temp->l_name) - 1);
		temp->l_name [sizeof (temp->l_name) - 1] = 0;
	}

	SHEAD_UNLOCK (sheadp, plbase);

	/*
	 * After unlocking we can safely call copyout (), which we could not
	 * use inside the loop because it may sleep resolving a page fault.
	 * Don't forget that copyout () is like bcopy (), not memcpy ()!
	 */

	* rvalp = i;

	i = copyout (buf, slistp->sl_modlist, i * sizeof (* buf));

	kmem_free (buf, modcount * sizeof (* buf));

	return i == 0 ? 0 : EFAULT;
}


/*
 * This function deals with setting the stream head read mode flag bits in
 * M_SETOPT messages or from an I_SRDOPT ioctl ().
 */

#if	__USE_PROTO__
int (SHEAD_SRDOPT) (shead_t * sheadp, int flag)
#else
int
SHEAD_SRDOPT __ARGS ((sheadp, flag))
shead_t	      *	sheadp;
int		flag;
#endif
{
	int		newflag;

	SHEAD_ASSERT_LOCKED (sheadp);

	if ((newflag = flag & RMODEMASK) == __RINVAL)
		return EINVAL;

	/*
	 * The streamio (7) man pages seem ambiguous about whether an
	 * application is permitted to, has to, or cannot diagnose a request
	 * to set multiple read options.
	 *
	 * Arbitrarily, we choose not to.
	 */

	newflag |= (flag & RPROTNORM) != 0 ? RPROTNORM :
		   (flag & RPROTDAT) != 0 ? RPROTDAT :
		   (flag & RPROTDIS) != 0 ? RPROTDIS :
			(sheadp->sh_readopt & ~ RMODEMASK);

	sheadp->sh_readopt = newflag;

	return 0;
}


/*
 * This table determines how many bytes to copy from user space at the start
 * of ioctl () processing and how many bytes to copy to user space at the end
 * of processing (presuming no other errors have occurred yet).
 */

#define	BADLEN		((unsigned short) -1)

typedef enum {
	IO_NOLOCK = 0,
	IO_BASIC_LOCK,
	IO_READFREEZE,
	IO_SLEEP_LOCK
} iolock_t;

enum {	NOHUP,
	HUP
};

static struct ioinfo {
	iolock_t	lock;		/* lock type */
	cat_t		cat;		/* category flag */
	unsigned short	in_len;		/* bytes to copy in */
	unsigned short	out_len;	/* bytes to copy out */
	unsigned char	hup_chk;	/* check for hangup */
} _ioctl_table [] = {
	{ IO_NOLOCK, SH_NONE, BADLEN, BADLEN, NOHUP },
					/* illegal */
	{ IO_READFREEZE, SH_NONE, sizeof (size_t), NOHUP },
						/* I_NREAD */
	{ IO_SLEEP_LOCK, SH_OPENCLOSE | SH_IOCTL_LOCK,
		FMNAMESZ + 1, 0, HUP },		/* I_PUSH */
	{ IO_SLEEP_LOCK, SH_OPENCLOSE | SH_IOCTL_LOCK, 0, 0, HUP },
						/* I_POP */
	{ IO_NOLOCK, SH_NONE, FMNAMESZ + 1, FMNAMESZ + 1, NOHUP },
						/* I_LOOK */
	{ IO_NOLOCK, SH_NONE, 0, 0, HUP },	/* I_FLUSH */
	{ IO_BASIC_LOCK, SH_NONE, 0, 0, NOHUP },/* I_SRDOPT */
	{ IO_BASIC_LOCK, SH_NONE, 0, sizeof (int), NOHUP },
						/* I_GRDOPT */
	{ IO_SLEEP_LOCK, SH_IOCTL_LOCK, sizeof (struct strioctl),
		sizeof (struct strioctl), HUP },/* I_STR */
	{ IO_SLEEP_LOCK, SH_IOCTL_LOCK, 0, 0, NOHUP },
						/* I_SETSIG */
	{ IO_SLEEP_LOCK, SH_IOCTL_LOCK, 0, sizeof (int), NOHUP },
						/* I_GETSIG */
	{ IO_NOLOCK, SH_NONE, FMNAMESZ + 1, 0, NOHUP },
						/* I_FIND */
	{ IO_SLEEP_LOCK, SH_IOCTL_LOCK, 0, 0, HUP },
						/* I_LINK */
	{ IO_SLEEP_LOCK, SH_IOCTL_LOCK, 0, 0, HUP },
						/* I_UNLINK */
	{ IO_SLEEP_LOCK, SH_READ_LOCK, 0, sizeof (struct strrecvfd), HUP },
						/* I_RECVFD */
	{ IO_SLEEP_LOCK, SH_READ_LOCK, sizeof (struct strpeek),
		sizeof (struct strpeek), NOHUP },/* I_PEEK */
	{ IO_NOLOCK, SH_NONE, sizeof (struct strfdinsert), 0, NOHUP },
						/* I_FDINSERT */
	{ IO_NOLOCK, SH_NONE, 0, 0, HUP },	/* I_SENDFD */
	{ IO_NOLOCK, SH_NONE, BADLEN, BADLEN, NOHUP },
					/* --- */
	{ IO_BASIC_LOCK, SH_NONE, 0, 0, NOHUP },/* I_SWROPT */
	{ IO_BASIC_LOCK, SH_NONE, 0, sizeof (int), NOHUP },
						/* I_GWROPT */
	{ IO_NOLOCK, SH_NONE, 0, 0, NOHUP },	/* I_LIST */ /* special */
	{ IO_BASIC_LOCK, SH_OPENCLOSE | SH_IOCTL_LOCK, 0, 0, HUP },
						/* I_PLINK */
	{ IO_BASIC_LOCK, SH_OPENCLOSE | SH_IOCTL_LOCK, 0, 0, HUP },
						/* I_PUNLINK */
	{ IO_NOLOCK, SH_NONE, BADLEN, BADLEN, NOHUP },
					/* I_SETEV */
	{ IO_NOLOCK, SH_NONE, BADLEN, BADLEN, NOHUP },
					/* I_GETEV */
	{ IO_NOLOCK, SH_NONE, BADLEN, BADLEN, NOHUP },
					/* I_STREV */
	{ IO_NOLOCK, SH_NONE, BADLEN, BADLEN, NOHUP },
					/* I_UNSTREV */
	{ IO_NOLOCK, SH_NONE, sizeof (struct bandinfo), 0, HUP },
						/* I_FLUSHBAND */
	{ IO_READFREEZE, SH_NONE, 0, 0, NOHUP },/* I_CKBAND */
	{ IO_READFREEZE, SH_NONE, 0, sizeof (int), NOHUP },
						/* I_GETBAND */
	{ IO_READFREEZE, SH_NONE, 0, 0, NOHUP },/* I_ATMARK */
	{ IO_BASIC_LOCK, SH_NONE, sizeof (__clock_t), 0, NOHUP },
						/* I_SETCLTIME */
	{ IO_BASIC_LOCK, SH_NONE, 0, sizeof (__clock_t), NOHUP },
						/* I_GETCLTIME */
	{ IO_NOLOCK, SH_NONE, 0, 0, HUP }	/* I_CANPUT */
};

struct ioinfo	_transparent = {
	IO_SLEEP_LOCK, SH_IOCTL_LOCK, 0, 0, HUP
};


/*
 * Symbol for accessing the default ioctl () and close timeouts. Consider
 * making this a static variable and initializing it at boot time.
 */

#define	IOCTL_TIMEOUT	drv_usectohz ((__clock_t) 15000000L)


/*
 * Main ioctl () processing for STREAMS files and pipes.
 *
 * This switch+table is an abomination, but trying to emulate C++ style in C
 * to subsume the switch and the above table would probably kill me. This
 * probably is about as short as it can really be.
 */

#if	__USE_PROTO__
int (STREAMS_IOCTL) (shead_t * sheadp, int cmd, _VOID * arg, int mode,
		     cred_t * credp, int * rvalp)
#else
int
STREAMS_IOCTL (sheadp, cmd, arg, mode, credp, rvalp)
shead_t	      *	sheadp;
int		cmd;
_VOID	      *	arg;
int		mode;
cred_t	      *	credp;
int	      *	rvalp;
#endif
{
	union {
		int		i_int;
		__clock_t	i_clock;
		size_t		i_size;
		char		i_modname [FMNAMESZ + 1];
		struct strioctl	i_strioc;
		struct strrecvfd i_recvfd;
		struct strbuf	i_strbuf;
		struct strfdinsert
				i_fdinsert;
		struct str_list	i_list;
		struct bandinfo	i_band;
		struct strpeek	i_peek;
	} iocbuf;
	int		retval;
	struct ioinfo *	info;

	ASSERT (sheadp != NULL);

	/*
	 * We start out by copying in the data specified by the table, which
	 * means we also get to range-check the ioctl entry if it has the
	 * magic STREAMS id.
	 */

	if ((cmd & ~ 0xFF) == STREAM_I) {
		int		index = cmd & 0xFF;

		if (index >= sizeof (_ioctl_table) / sizeof (* info))
			return EINVAL;

		info = & _ioctl_table [index];
	} else
		info = & _transparent;

	if (info->in_len == BADLEN)
		return EINVAL;

	/*
	 * We *must* do the copy before the lock! Never forget that
	 * copyin ()/copyout () can block in page fault resolution.
	 */

	if (info->in_len > 0 && copyin (arg, & iocbuf, info->in_len) != 0)
		return EFAULT;

	/*
	 * We may wish to check for a hangup or error before proceeding.
	 */

	(void) SHEAD_LOCK (sheadp);

	if (info->hup_chk == HUP &&
	    (retval = SHEAD_ERRHUP_LOCKED (sheadp, mode)) != 0)
		return ENXIO;

	if (info->lock != IO_BASIC_LOCK)
		SHEAD_UNLOCK (sheadp, plbase);

	switch (info->lock) {

	case IO_BASIC_LOCK:
		/* stay holding on to the basic lock */
		break;

	case IO_READFREEZE:
		(void) QFREEZE_TRACE (sheadp->sh_head, "STREAMS_IOCTL");
		break;

	case IO_SLEEP_LOCK:
		if ((retval = SHEAD_SLEEP_LOCK (sheadp, info->cat,
						IOCTL_TIMEOUT,
						CHECK_SIGNALS)) != 0)
			return retval;
		break;

	default:
		break;
	}

	retval = 0;

	switch (cmd) {

	case I_NREAD:		/* Get message length, count */
		{
			mblk_t	      *	scan;
			mblk_t	      *	first = NULL;

			* rvalp = 0;

			for (scan = SHEAD_FIRSTMSG (sheadp) ; scan != NULL ;
			     scan = scan->b_next) {

				if (datamsg (scan->b_datap->db_type)) {

					if (first == NULL)
						first = scan;
					(* rvalp) ++;
				}
			}

			if (first != NULL)
				iocbuf.i_int = msgdsize (first);
			else
				iocbuf.i_int = 0;
		}
		break;

	case I_PUSH:		/* push named module */
		{
			modsw_t	      *	module;

			if ((module = FIND_MODULE (iocbuf.i_modname)) == NULL)
				retval = EINVAL;
			else
				retval = PUSH_MODULE (sheadp, mode, credp,
						      module);
		}
		break;

	case I_POP:		/* pop topmost module */
		{
			queue_t	      *	q;

			if ((q = TOP_MODULE (sheadp)) == NULL)
				retval = EINVAL;
			else
				retval = POP_MODULE (sheadp, q, mode, credp);
		}
		break;

	case I_LOOK:		/* get topmost module name */
		{
			queue_t	      *	q;

			if ((q = TOP_MODULE (sheadp)) == NULL)
				retval = EINVAL;
			else {
				/*
				 * Copy the module name, taking care to
				 * 0-terminate it at FMNAMESZ bytes long.
				 */

				strncpy (iocbuf.i_modname,
					 q->q_qinfo->qi_minfo->mi_idname,
					 FMNAMESZ);
				iocbuf.i_modname [FMNAMESZ] = 0;
			}
		}
		break;

	case I_FLUSH:		/* flush read and/or write side */
		retval = SHEAD_FLUSH (sheadp, (int) arg, 0);
		break;

	case I_SRDOPT:		/* set read options */
		retval = SHEAD_SRDOPT (sheadp, (int) arg);
		break;

	case I_GRDOPT:		/* retrieve read options */
		iocbuf.i_int = sheadp->sh_readopt;
		break;

	case I_STR:		/* send ioctl () data down a stream */
		if ((iocbuf.i_strioc.ic_cmd & ~ 0xFF) == STREAM_I ||
		    (unsigned) iocbuf.i_strioc.ic_len == TRANSPARENT) {
			/*
			 * We do not permit I_STR to send STREAMS ioctl ()
			 * codes downstream; in certain cases such as I_LINK
			 * this could produce disastrous results.
			 *
			 * We also do not permit TRANSPARENT length I_STR
			 * messages. While the STREAMS documentation neither
			 * explicitly permits or forbids this, we keep the
			 * transparent ioctl () behaviour separate.
			 */

			retval = EINVAL;
			break;

		}

		retval = ISTR_IOCTL (sheadp, mode, & iocbuf.i_strioc, credp,
				     rvalp);
		break;

	case I_SETSIG:		/* register events for SIGPOLL signal */
		{
			int		events = (short) (ulong_t) arg;

			if ((events & ~ __POLL_MASK) != 0)
				retval = EINVAL;
			else
				retval = REGISTER_SIGPOLL (sheadp, events);
		}
		break;

	case I_GETSIG:		/* return registered event mask */
		{
			sigpoll_t     *	sigs;

			if ((sigs = FIND_SIGPOLL (sheadp)) == NULL)
				retval = EINVAL;
			else
				* rvalp = sigs->sp_events;
		}
		break;

	case I_FIND:		/* determine if module exists on stream */
		retval = SHEAD_FIND_MODINFO (sheadp, iocbuf.i_modname, rvalp);
		break;

	case I_LINK:		/* link stream below another */
	case I_PLINK:		/* create a persistent link */
		{
			shead_t	      *	lower;

			if ((lower = FH_TO_STREAM ((int) arg,
						   & retval)) != NULL) {
				retval = SHEAD_LINK (sheadp, mode, lower, cmd,
						     credp);

				if (retval == 0)
					* rvalp = lower->sh_muxid;
			}
		}
		break;

	case I_UNLINK:		/* remove a (or all) link(s) below a stream */
	case I_PUNLINK:		/* undo a single or all persistent link(s) */
		do {
			shead_t	      *	lower;

			if ((lower = SHEAD_FIND_MUXID (sheadp, cmd,
						       (int) arg)) == NULL) {
				/*
				 * We return EINVAL if a specific mux ID was
				 * given, 0 otherwise.
				 */

				retval = (int) arg == -1 ? 0 : EINVAL;
				break;

			}

			(void) SHEAD_UNLINK (sheadp, lower, cmd, mode, credp,
					     & retval);
		} while ((int) arg == -1);
		break;

	case I_RECVFD:		/* receive a file descriptor from stream */
		retval = FH_RECV (sheadp, mode, & iocbuf.i_recvfd);
		break;

	case I_PEEK:		/* examine data at stream head */
		retval = SHEAD_PEEK (sheadp, & iocbuf.i_peek, rvalp);
		break;

	case I_FDINSERT:	/* send read queue pointer down stream */
		retval = SHEAD_FDINSERT (sheadp, mode, & iocbuf.i_fdinsert);
		break;

	case I_SENDFD:		/* send a file descriptor down a pipe */
		retval = FH_SEND (sheadp, (int) arg, credp);
		break;

	case I_SWROPT:		/* set write options for stream */
		{
			int		flag = (int) arg;

			if ((flag & ~ SNDZERO) != 0)
				retval = EINVAL;
			else
				sheadp->sh_wropt = flag;
		}
		break;

	case I_GWROPT:		/* retrieve write options for stream */
		iocbuf.i_int = sheadp->sh_wropt;
		break;

	case I_LIST:		/* get names of all modules/drivers */
		/*
		 * The value of "arg" is a pointer to a structure for this
		 * entry, but since a NULL value is legal we don't copy the
		 * data in automatically.
		 *
		 * Here we select the call type and copy in the structure for
		 * the non-NULL case.
		 */

		if (arg == NULL)
			* rvalp = SHEAD_MODCOUNT (sheadp);
		else if (copyin (arg, & iocbuf, sizeof (iocbuf.i_list)) != 0)
			retval = EFAULT;
		else
			retval = SHEAD_LIST (sheadp, & iocbuf.i_list, rvalp);
		break;

	case I_SETEV:		/* The meaning of these ioctl ()'s is not */
	case I_GETEV:		/* documented, although their names and */
	case I_STREV:		/* numeric values are given in the System */
	case I_UNSTREV:		/* V ABI. */
		retval = EINVAL;
		break;

	case I_FLUSHBAND:	/* flush messages in a priority band */
		retval = SHEAD_FLUSH (sheadp, iocbuf.i_band.bi_flag,
				      iocbuf.i_band.bi_pri);
		break;

	case I_CKBAND:		/* check for existence of band on stream */
		if ((uchar_t) (ulong_t) arg != (ulong_t) arg)
			retval = EINVAL;
		else {
			mblk_t	      *	scan;

			for (scan = SHEAD_FIRSTMSG (sheadp) ; scan != NULL ;
			     scan = scan->b_next)
				if (datamsg (scan->b_datap->db_type) &&
				    scan->b_band == (uchar_t) (ulong_t) arg)
					break;

			* rvalp = scan != NULL;
		}
		break;

	case I_GETBAND:		/* get the band number of the first message */
		{
			mblk_t	     *	scan;

			for (scan = SHEAD_FIRSTMSG (sheadp) ; scan != NULL ;
			     scan = scan->b_next) {

				if (datamsg (scan->b_datap->db_type))
					break;
			}

			if (scan == NULL)
				retval = ENODATA;
			else
				iocbuf.i_int = scan->b_band;
		}
		break;

	case I_ATMARK:		/* test for (last) mark on messages */
		{
			mblk_t	      *	scan = SHEAD_FIRSTMSG (sheadp);

			if ((ulong_t) arg != ANYMARK &&
			    (ulong_t) arg != LASTMARK) {

				retval = EINVAL;
				break;
			}

			if (scan == NULL || (scan->b_flag & MSGMARK) == 0) {

				* rvalp = 0;
				break;
			}

			* rvalp = 1;

			if ((ulong_t) arg == LASTMARK)
				while ((scan = scan->b_next) != NULL)
					if ((scan->b_flag & MSGMARK) != 0) {
						* rvalp = 0;
						break;
					}
		}
		break;

	case I_SETCLTIME:	/* set close timeout for stream */
		if (iocbuf.i_clock != 0)
			sheadp->sh_cltime = iocbuf.i_clock;
		else
			retval = EINVAL;
		break;

	case I_GETCLTIME:	/* retrieve current close timeout */
		iocbuf.i_clock = sheadp->sh_cltime;
		break;

	case I_CANPUT:		/* test if band is writeable */
		if ((uchar_t) (ulong_t) arg != (ulong_t) arg)
			retval = EINVAL;
		else
			* rvalp = bcanputnext (W (sheadp->sh_head),
					       (uchar_t) (ulong_t) arg);
		break;

	default:
		ASSERT (info == & _transparent);

		retval = TRANSPARENT_IOCTL (sheadp, mode, cmd, arg, credp,
					    rvalp);
		break;
	}


	/*
	 * Perform any necessary unlocking operations and copy back any
	 * results into the data area pointed to by "arg" (if this is a
	 * STREAMS-specific ioctl ()).
	 */

	switch (info->lock) {

	case IO_BASIC_LOCK:
		SHEAD_UNLOCK (sheadp, plbase);
		break;

	case IO_READFREEZE:
		(void) QUNFREEZE_TRACE (sheadp->sh_head, plbase);
		break;

	case IO_SLEEP_LOCK:
		SHEAD_SLEEP_UNLOCK (sheadp, info->cat);
		break;

	default:
		break;
	}


	/*
	 * We only copy out results if there is no error. We have to
	 * do this *after* unlocking, above; copyout () can block in
	 * page fault resolution!
	 */

	if (retval == 0 && info->out_len > 0 &&
	    copyout (& iocbuf, arg, info->out_len) != 0)
		retval = EFAULT;

	return retval;
}


/*
 * Helper function to atomically read the stream head read options.
 */

#if	__USE_PROTO__
__LOCAL__ short (SHEAD_READOPT) (shead_t * sheadp)
#else
__LOCAL__ short
SHEAD_READOPT __ARGS ((sheadp))
shead_t	      *	sheadp;
#endif
{
	pl_t		prev_pl;
	short		opt;

	prev_pl = SHEAD_LOCK (sheadp);

	opt = sheadp->sh_readopt;

	SHEAD_UNLOCK (sheadp, prev_pl);

	return opt;
}


/*
 * Stream head user-level getmsg () processing.
 *
 * This is way too many parameters; this should be bundled into a block like
 * the "uio" structure is.
 */

#if	__USE_PROTO__
int (STREAMS_GETPMSG) (shead_t * sheadp, struct strbuf * ctlbuf,
		       struct strbuf * databuf, int * bandp, int * flagsp,
		       int mode, cred_t * credp, int * rvalp)
#else
int
STREAMS_GETPMSG __ARGS ((sheadp, ctlbuf, databuf, bandp, flagsp, mode, credp,
			 rvalp))
shead_t	      *	sheadp;
int		mode;
struct strbuf *	ctlbuf;
struct strbuf *	databuf;
int	      *	bandp;
int	      *	flagsp;
cred_t	      *	credp;
int	      *	rvalp;
#endif
{
	mblk_t	      *	msg;
	mblk_t	      *	scan;
	int		retval;

	if ((mode & FREAD) == 0)
		return EBADF;

	mode &= ~ FWRITE;
	if (sheadp->sh_linked != NULL)
		return EINVAL;

	/*
	 * As discussed in the general comment on read synchronization and in
	 * the I_PEEK documentation, there are some warts when it comes to
	 * read locking. Here we can solve things by dequeueing a message,
	 * modifying at, and writing it back without worrying about any nasty
	 * unintended effects.
	 *
	 * Because getmsg ()/getpmsg () always honours message boundaries,
	 * there is no real value in single-threading this.
	 */

	/*
	 * Look at the stream head to see if a message is present. Note that
	 * we freeze the read queue before acquiring the stream head basic
	 * lock because that's our canonical ordering.
	 */

	for (;;) {
		(void) QFREEZE_TRACE (sheadp->sh_head, "SHEAD_GETPMSG");

		(void) SHEAD_LOCK (sheadp);

		if ((retval = SHEAD_ERRHUP_LOCKED (sheadp, FREAD)) != 0) {

			QUNFREEZE_TRACE (sheadp->sh_head, plbase);
			return retval;
		}


		if ((msg = SHEAD_FIRSTMSG (sheadp)) != NULL) {

			switch (msg->b_datap->db_type) {

			case M_DATA:
			case M_PROTO:

				if (* flagsp == MSG_HIPRI ||
				    (* flagsp == MSG_BAND &&
				     msg->b_band < * bandp)) {

					sheadp->sh_head->q_lastband =
						msg->b_band;
					msg = NULL;
					break;
				}

				/* FALL THROUGH */

			case M_PCPROTO:
				rmvq (sheadp->sh_head, msg);
				break;

			default:
				retval = EBADMSG;
				break;
			}
		}

		QUNFREEZE_TRACE (sheadp->sh_head, plbase);

		if (retval != 0) {
			SHEAD_UNLOCK (sheadp, plbase);
			return retval;
		}

		if (msg != NULL)
			break;

		/*
		 * We need to wait (unless O_NDELAY or O_NONBLOCK has been
		 * specified). We don't wait any more if the stream has been
		 * hung up.
		 */

		if (SHEAD_HANGUP (sheadp)) {
			SHEAD_UNLOCK (sheadp, plbase);

			/*
			 * Hangups are not an error for getpmsg ().
			 */

			if (ctlbuf != NULL)
				ctlbuf->len = 0;
			if (databuf != NULL)
				databuf->len = 0;

			* rvalp = 0;
			return 0;
		}

		if ((retval = SHEAD_WAIT_NONBLOCK (sheadp, mode, SH_READ_WAIT,
						   CHECK_SIGNALS)) != 0)
			return retval;
	}


	/*
	 * After this point, "msg" is our responsibility and we either have to
	 * free it or put it back if there is an error.
	 */

	* bandp = msg->b_band;
	* flagsp = msg->b_datap->db_type == M_PCPROTO ? MSG_HIPRI : MSG_BAND;

	retval = (COPYOUT_BUF (ctlbuf, & msg, CONTROL_PART) != 0 ||
		  COPYOUT_BUF (databuf, & msg,
			       DATA_PART) != 0) ? EFAULT : 0;

	/*
	 * Formulate a return mask indicating what components of the message
	 * being transferred have not been fully consumed.
	 */

	* rvalp = 0;

	for (scan = msg ; scan != NULL ; scan = scan->b_cont)
		* rvalp |= scan->b_datap->db_type == M_DATA ? MOREDATA
							    : MORECTL;

	if (msg != NULL)
		putbq (sheadp->sh_head, msg);

	SHEAD_UNLOCK (sheadp, plbase);
	return retval;
}


/*
 * In order to keep the logic of SHEAD_READ () manageable, this section of
 * code has been factored into a separate function. Here we wait for data to
 * become available at the stream head for reading.
 *
 * We return 0 on success, or an error number on failure. The value of "mpp"
 * is only valid if 0 is returned.
 */

#if	__USE_PROTO__
__LOCAL__ int (SHEAD_READ_DATA) (shead_t * sheadp, int mode, mblk_t ** mpp,
				 int resid)
#else
__LOCAL__ int
SHEAD_READ_DATA __ARGS ((sheadp, mode, mpp, resid))
shead_t	      *	sheadp;
int		mode;
mblk_t	     **	mpp;
int		resid;
#endif
{
	int		retval;

	for (;;) {
		(void) QFREEZE_TRACE (sheadp->sh_head, "SHEAD_READ");

		(void) SHEAD_LOCK (sheadp);

		if ((retval = SHEAD_ERRHUP_LOCKED (sheadp, FREAD)) != 0) {

			QUNFREEZE_TRACE (sheadp->sh_head, plbase);
			return retval;
		}


		if ((* mpp = SHEAD_FIRSTMSG (sheadp)) != NULL) {

			switch ((* mpp)->b_datap->db_type) {

			case M_PROTO:
			case M_PCPROTO:
				/*
				 * These are valid depending on the read mode
				 * of the stream.
				 */

				if ((sheadp->sh_readopt & RPROTNORM) != 0) {

					retval = EBADMSG;
					break;
				}

				/* FALL THROUGH */

			case M_DATA:
				rmvq (sheadp->sh_head, * mpp);
				break;

			default:
				retval = EBADMSG;
				break;
			}
		}

		QUNFREEZE_TRACE (sheadp->sh_head, plbase);

		if (retval != 0 || * mpp != NULL) {

			SHEAD_UNLOCK (sheadp, plbase);
			return retval;
		}


		/*
		 * Since we are going to sleep on a read (), this is the place
		 * to generate M_READ messages if that is how this stream has
		 * been configured.
		 *
		 * If we can't generate an M_READ message, schedule a bufcall.
		 * If we can't do that, return EAGAIN???
		 */

		if (SHEAD_READMSG (sheadp)) {
			mblk_t	      *	msg;

			if ((msg = MSGB_ALLOC (sizeof (int), BPRI_LO,
					       KM_NOSLEEP)) == NULL) {
				/*
				 * Execute a short wait for buffer memory for
				 * the M_READ, then retry the loop. Note that
				 * SHEAD_READ_BUFCALL () unlocks the stream
				 * head for us.
				 */

				if ((retval = SHEAD_READ_BUFCALL (sheadp,
								  mode)) != 0)
					return retval;

				continue;
			}

			msg->b_datap->db_type = M_READ;
			* (int *) msg->b_rptr = resid;
			msg->b_wptr += sizeof (int);

			putq (W (sheadp->sh_head), msg);
		}

		if ((retval = SHEAD_WAIT_NONBLOCK (sheadp, mode, SH_READ_WAIT,
						   CHECK_SIGNALS)) != 0)
			return retval;
	}

}


/*
 * In order to keep the logic of SHEAD_READ () manageable, this section of
 * code has been factored into this routine, which manages the transfer of
 * message data to the user.
 *
 * We return EAGAIN to the user if more data needs to be read, 0 if the read
 * has successfully completed, or some other error number on failure.
 */

#if	__USE_PROTO__
__LOCAL__ int (SHEAD_READ_MOVE) (shead_t * sheadp, uio_t * uiop,
				 mblk_t * msg)
#else
__LOCAL__ int
SHEAD_READ_MOVE __ARGS ((sheadp, uiop, msg))
shead_t	      *	sheadp;
uio_t	      *	uiop;
mblk_t	      *	msg;
#endif
{
	int		readopt = SHEAD_READOPT (sheadp);
	mblk_t	      *	scan;

	if ((readopt & RMODEMASK) == RNORM &&
	    (msg->b_cont == NULL && msg->b_rptr == msg->b_wptr)) {
		/*
		 * We have run into a zero-length message. Put it
		 * back and terminate the read.
		 */

		putbq (sheadp->sh_head, msg);
		return 0;
	}

	if ((readopt & RPROTDIS) != 0) {

		while (msg != NULL && msg->b_datap->db_type != M_DATA) {
			/*
			 * Consume the control part of the message.
			 */

			scan = msg->b_cont;
			freeb (msg);
			msg = scan;
		}

		if (msg == NULL)
			return EAGAIN;
	}


	/*
	 * Actual data transfer time; copy each message segment to the user
	 * with uiomove ().
	 */

	do {
		size_t		unit = msg->b_wptr - msg->b_rptr;

		if (unit > uiop->uio_resid)
			unit = uiop->uio_resid;

		if (unit > 0 &&
		    uiomove (msg->b_rptr, unit, UIO_READ, uiop) != 0) {
			/*
			 * Address fault time. But first, put back the data.
			 */

			putbq (sheadp->sh_head, msg);

			return EFAULT;
		}

		msg->b_rptr += unit;

		if (msg->b_wptr != msg->b_rptr) {
			/*
			 * Since we didn't finish this message block, we must
			 * be finished with the read (). If we are in message-
			 * discard mode we throw away the remaining data.
			 */

			ASSERT (uiop->uio_resid == 0);

			if ((readopt & RMODEMASK) == RMSGD)
				freemsg (msg);
			else
				putbq (sheadp->sh_head, msg);

			return 0;
		}

		scan = msg->b_cont;
		freeb (msg);
	} while ((msg = scan) != NULL);


	/*
	 * We have run out of message; what now? If in byte-stream mode, look
	 * for more data, otherwise exit.
	 */

	return (readopt & RMODEMASK) == RNORM ? EAGAIN : 0;
}


/*
 * Stream head user-level read processing.
 *
 * I apologise for the abysmal structure of this code; the gotos should be
 * replaced by proper loops and the major subsections factored into auxiliary
 * functions, but this code is the victim of time pressure and several
 * rewrites. By all means encourage the author to improve this in case he has
 * forgotten to come back and fix it.
 */

#if	__USE_PROTO__
int (STREAMS_READ) (shead_t * sheadp, uio_t * uiop, cred_t * credp)
#else
int
STREAMS_READ __ARGS ((sheadp, uiop, credp))
shead_t	      *	sheadp;
uio_t	      *	uiop;
cred_t	      *	credp;
#endif
{
	mblk_t	      *	msg;
	int		retval;
	int		mode;
	int		readcount = uiop->uio_resid;

	if ((uiop->uio_fmode & FREAD) == 0)
		return EBADF;

	mode = uiop->uio_fmode & ~ FWRITE;

	if (sheadp->sh_linked != NULL)
		return EINVAL;

	/*
	 * We (optionally) take out a lock on the stream head to single-thread
	 * reads. This is important because in byte-stream mode we may want
	 * to guarantee atomicity of reads. This is only relevant to byte-
	 * stream mode because modes which honor message boundaries cannot
	 * be protected against multiple readers anyway.
	 */

	if ((retval = SHEAD_SLEEP_LOCK (sheadp, SH_READ_LOCK, 0,
					CHECK_SIGNALS)) != 0)
		return retval;

	do {
		if ((retval = SHEAD_READ_DATA (sheadp, mode, & msg,
					       uiop->uio_resid)) != 0)
			break;

		/*
		 * After this point, "msg" is our responsibility and we either
		 * have to free it or put it back if there is an error.
		 */

	} while ((retval = SHEAD_READ_MOVE (sheadp, uiop, msg)) == EAGAIN);


	/*
	 * If a partial read has been done, we return a short read rather than
	 * reporting an error immediately.
	 */

	if (retval != 0 && readcount != uiop->uio_resid)
		retval = 0;

	SHEAD_SLEEP_UNLOCK (sheadp, SH_READ_LOCK);
	return retval;
}


/*
 * Helper function to atomically read the stream head write options.
 */

#if	__USE_PROTO__
__LOCAL__ short (SHEAD_WRITEOPT) (shead_t * sheadp)
#else
__LOCAL__ short
SHEAD_WRITEOPT __ARGS ((sheadp))
shead_t	      *	sheadp;
#endif
{
	pl_t		prev_pl;
	short		opt;

	prev_pl = SHEAD_LOCK (sheadp);

	opt = sheadp->sh_wropt;

	SHEAD_UNLOCK (sheadp, prev_pl);

	return opt;
}


/*
 * Stream head user level putpmsg () processing. As with getmsg (), there are
 * enough parameters being passed that this should be abstracted into a
 * structure like uio(D4DK).
 */

#if	__USE_PROTO__
int (STREAMS_PUTPMSG) (shead_t * sheadp, __CONST__ struct strbuf * ctlbuf,
		       __CONST__ struct strbuf * databuf, int band, int flags,
		       int mode, cred_t * credp, int * rvalp)
#else
int
STREAMS_PUTPMSG __ARGS ((sheadp, ctlbuf, databuf, band, flags, mode, credp,
			 rvalp))
shead_t	      *	sheadp;
int		mode;
__CONST__ struct strbuf
	      *	ctlbuf;
__CONST__ struct strbuf
	      *	databuf;
int		band;
int		flags;
cred_t	      *	credp;
int	      *	rvalp;
#endif
{
	mblk_t	      *	msg;
	int		retval;

	/*
	 * Make a message (possibly high-priority). This can fail if the
	 * requested message is too large for the advertised limits set by the
	 * next thing downstream. SHEAD_MAKEMSG () also checks for error and
	 * hangup conditions, FNDELAY/FNONBLOCK, and flow control.
	 */

	msg = SHEAD_MAKEMSG (sheadp, mode, ctlbuf, databuf, flags, band,
			     & retval);

	if (msg == NULL)
		return retval;

	/*
	 * Note that we don't recheck for hangups even though we could have
	 * waited a poentially long time in SHEAD_MAKEMSG (). The hangup
	 * condition has plenty of slop in it with the time it takes write
	 * messages to move down the queue anyway; drivers and modules have to
	 * be able to cope.
	 */

	putq (W (sheadp->sh_head), msg);

	* rvalp = 0;
	return 0;
}


/*
 * Stream head user-level write processing.
 *
 * I apologise for the abysmal structure of this code; the gotos should be
 * replaced by proper loops and the major subsections factored into auxiliary
 * functions, but this code is the victim of time pressure and several
 * rewrites. By all means encourage the author to improve this in case he has
 * forgotten to come back and fix it.
 */

#if	__USE_PROTO__
int (STREAMS_WRITE) (shead_t * sheadp, uio_t * uiop, cred_t * credp)
#else
int
STREAMS_WRITE __ARGS ((sheadp, uiop, credp))
shead_t	      *	sheadp;
uio_t	      *	uiop;
cred_t	      *	credp;
#endif
{
	queue_t	      *	q;
	short		wropt = SHEAD_WRITEOPT (sheadp);
	mblk_t	      *	datamsg;
	int		datasize;
	int		wroff;
	int		retval;
	int		mode;
	int		writecount;

	if ((uiop->uio_fmode & FWRITE) == 0)
		return EBADF;

	mode = uiop->uio_fmode & ~ FREAD;
	if (sheadp->sh_linked != NULL)
		return EINVAL;

	/*
	 * Deal with the zero-length-message special case.
	 */

	if ((writecount = uiop->uio_resid) == 0 && (wropt & SNDZERO) == 0)
		return 0;

	/*
	 * Unlike putmsg ()/putpmsg (), write () can potentially spread the
	 * data it writes over multiple messages and take a considerable time
	 * to do it, we allow for the possibility of locking the stream head
	 * so that only one write () is in progress at any time.
	 *
	 * The primary purpose of this is to allow PIPE_BUF to be effectively
	 * unlimited. This is an experimental idea, though.
	 */

	if ((retval = SHEAD_SLEEP_LOCK (sheadp, SH_WRITE_LOCK, 0,
					CHECK_SIGNALS)) != 0)
		return retval;

	wroff = SHEAD_WRITEOFFSET (sheadp);

	q = TOP_QUEUE (sheadp);

	do {

		if ((datasize = uiop->uio_resid) + wroff > q->q_maxpsz) {
			/*
			 * Special case (documented on the write (2) manual
			 * page; if we can't fit within the max/min range and
			 * the minimum is greater than 0, return ERANGE.
			 */

			if (q->q_minpsz > 0) {

				retval = ERANGE;
				break;
			}

			datasize = q->q_maxpsz - wroff;
		} else if (datasize + wroff < q->q_minpsz) {

			retval = ERANGE;
			break;
		}


		/*
		 * Let's see if the stream is flow controlled; if it is, we
		 * either block or return EAGAIN depending on the
		 * FNDELAY/FNONBLOCK setting.
		 */

		if ((retval = SHEAD_WRITE_TEST (sheadp, mode, 0, 0)) != 0)
			break;

		if ((datamsg = MSGB_ALLOC (datasize + wroff, BPRI_LO,
					   KM_SLEEP)) == NULL) {
			retval = ENOSR;
			break;
		}

		datamsg->b_wptr = datamsg->b_rptr = datamsg->b_rptr + wroff;

		if (datasize > 0 &&
		    uiomove (datamsg->b_rptr, datasize, UIO_WRITE,
			     uiop) != 0) {

			freemsg (datamsg);

			retval = EFAULT;
			break;
		}

		datamsg->b_wptr += datasize;

		/*
		 * Now do the write, and see if there is more data.
		 */

		putq (W (sheadp->sh_head), datamsg);

	} while (uiop->uio_resid > 0);


	/*
	 * If there has been an error after some data was actually written
	 * we return a short read rather than report the error immediately.
	 */

	if (retval != 0 && uiop->uio_resid != writecount)
		retval = 0;

	SHEAD_SLEEP_UNLOCK (sheadp, SH_WRITE_LOCK);
	return retval;
}


/*
 * Stream head user-level open processing.
 */

extern struct streamtab headinfo;

#if	__USE_PROTO__
int (STREAMS_OPEN) (n_dev_t * devp, struct streamtab * stabp, int mode,
		    cred_t * credp, int cloneflag)
#else
int
STREAMS_OPEN (devp, stabp, mode, credp, cloneflag)
n_dev_t	      *	devp;
struct streamtab
	      *	stabp;
int		mode;
cred_t	      *	credp;
int		cloneflag;
#endif
{
	shead_t	      *	sheadp;
	queue_t	      *	q;
	int		retval;
	n_dev_t		dev;

	ASSERT (devp != NULL);
	ASSERT (stabp != NULL);
	ASSERT (credp != NULL);

	dev = * devp;

	if ((sheadp = SHEAD_OPEN_LOCK (* devp, stabp, & retval)) == NULL)
		return retval;

	/*
	 * If this is the first open of the stream, set up the stream head
	 * entry points and the driver entry points.
	 */

	q = W (sheadp->sh_head)->q_next;

	if (sheadp->sh_open_count == 0) {

		QUEUE_INIT (sheadp->sh_head, & headinfo, QI_NORMAL);
		qprocson (sheadp->sh_head);

		QUEUE_INIT (R (q), stabp, QI_NORMAL);
	}


	/*
	 * Now we have a stream head (locked, no less), we can call the open
	 * entry points of all the modules and the driver. In the special case
	 * where the open count of the entry is 0, we allow the driver to
	 * change the "dev_t" value to a previously unused number.
	 */

	do {
		retval = (* R (q)->q_qinfo->qi_qopen)
				(R (q), & dev, mode,
				 q->q_next != NULL ? MODOPEN :
					 cloneflag ? CLONEOPEN : 0, credp);

		if (dev != * devp && q->q_next != NULL) {
			/*
			 * A module has changed the device number that we
			 * passed a pointer to. This is not valid!
			 */

			cmn_err (CE_WARN, "Module \"%s\" changed its device number",
				 q->q_qinfo->qi_minfo->mi_idname);
			retval = ENXIO;
		}

		if (retval != 0)
			goto failure;
	} while ((q = q->q_next) != NULL);


	/*
	 * The modules and driver have all OK'ed the open, so increment the
	 * open count. Here we also check for the clone case.
	 *
	 * If we want to detect an error after this point, we should execute
	 * a close.
	 */

	sheadp->sh_open_count ++;

	if (dev != * devp) {
		/*
		 * The driver has requested that the device number of the
		 * queue be assigned differently than the initial device
		 * number. This is only really valid if this is the first open
		 * of the given queue.
		 */

		if (sheadp->sh_open_count > 1) {

			cmn_err (CE_WARN, "Driver \"%s\" changed its device number after inital open",
				 q->q_qinfo->qi_minfo->mi_idname);

			sheadp->sh_open_count --;

			retval = ENXIO;
			goto failure;
		}


		/*
		 * Other open attempts may be waiting on the stream head for
		 * the original device number; they need a wakeup.
		 */

		if (SHEAD_RENAME (sheadp, dev) != 0) {

			cmn_err (CE_WARN, "Clone device number chosen by driver \"%s\"is in use",
				 q->q_qinfo->qi_minfo->mi_idname);

			if (-- sheadp->sh_open_count == 0)
				SHEAD_DO_CLOSE (sheadp, mode, credp);

			retval = ENXIO;
			goto failure;
		}

		* devp = dev;
	}

failure:
	/*
	 * The module or driver has failed the open request. We unlock the
	 * stream head, which may deallocate the stream head if the open count
	 * is 0.
	 */

	SHEAD_SLEEP_UNLOCK (sheadp, SH_OPENCLOSE);
	return retval;
}


/*
 * Stream head interface to generic polling.
 */

#if	__USE_PROTO__
int (STREAMS_CHPOLL) (shead_t * sheadp, short events, int anyyet,
		      short * reventsp, struct pollhead ** phpp)
#else
int
STREAMS_CHPOLL __ARGS ((sheadp, events, anyyet, reventsp, phpp))
shead_t	      *	sheadp;
short		events;
int		anyyet;
short	      *	reventsp;
struct pollhead
	     **	phpp;
#endif
{
	short		my_events;

	/*
	 * The chpoll () entry point uses the POLL... constants rather than
	 * the S_... constants that I_SETSIG uses. We convert to the S_...
	 * form for our internal use... see <sys/poll.h>
	 */

	my_events = (events & (__POLL_INPUT | __POLL_HIPRI | __POLL_OUTPUT |
			       __POLL_RDNORM | __POLL_OUTPUT |
			       __POLL_RDBAND | __POLL_WRBAND));

	if ((events & POLLERR) != 0)
		my_events = S_ERROR;
	if ((events & POLLHUP) != 0)
		my_events = S_HANGUP;

	if ((my_events = SHEAD_POLL_CHECK (sheadp, my_events)) == 0) {

		* reventsp = 0;

		if (anyyet == 0)
			* phpp = sheadp->sh_pollhead;
	} else
		* reventsp = my_events;

	return 0;
}


/*
 * Stream head user-level close processing.
 */

#if	__USE_PROTO__
int (STREAMS_CLOSE) (shead_t * sheadp, int mode, cred_t * credp)
#else
int
STREAMS_CLOSE __ARGS ((sheadp, mode, credp))
shead_t	      *	sheadp;
int		mode;
cred_t	      *	credp;
#endif
{
	int		retval;

	if ((retval = SHEAD_SLEEP_LOCK (sheadp, SH_OPENCLOSE | SH_IOCTL_LOCK,
					0, CHECK_SIGNALS)) != 0)
		return retval;

	if (-- sheadp->sh_open_count == 0)
		SHEAD_DO_CLOSE (sheadp, mode, credp);

	SHEAD_SLEEP_UNLOCK (sheadp, SH_OPENCLOSE | SH_IOCTL_LOCK);
	return 0;
}
