extern long __bolt;
/*
 * ftx.c
 *
 * Manage i/o requests from tape driver to FDC.
 */

/*
 * ----------------------------------------------------------------------
 * Includes.
 */
#include <sys/coherent.h>
#include <sys/cmn_err.h>
#include <sys/dmac.h>
#include <sys/fdc765.h>

#include <coh/misc.h>

#include <sys/ft.h>
#include <sys/ftx.h>
#include <sys/ftxreq.h>

/* Globals. */

extern int fdcTblk;	/* For FDC error messages - tape block in last cmd. */

/* Data Structures and State Variables. */

__VOLATILE__ struct Ftx ftx;

/* Buffer area for read/write command that will be sent to FDC. */

unsigned char fdcRWCmd[] = {
	0x46,		/* FDC read, skip deleted data, MFM.	*/
	0,		/* Head 0, unit 0.			*/
	0,		/* C = FDC cylinder			*/
	0,		/* H = FDC head				*/
	1,		/* R = FDC block number (1-based)	*/
	3,		/* N = block length (3 = 1024 bytes)	*/
	128,		/* EOT = last block number on cylinder	*/
	109,		/* GPL = number of bytes in gap 3	*/
	255		/* DTL, use 0xFF when N != 0		*/
};

/* Read/write command bytes - first element of fdcRWCmd. */
enum {
	FDC_CMD_RD = 0x46,
	FDC_CMD_WR = 0x45
};

/* Offsets into rwCmd. */
enum {
	RW_CMD_VERB	= 0,
	RW_CMD_UNIT	= 1,
	RW_CMD_CYL	= 2,
	RW_CMD_HEAD	= 3,
	RW_CMD_SCTR	= 4,
	RW_CMD_SCSZ	= 5,	/* encoded block length		*/
	RW_CMD_SCHI	= 6,	/* high block number		*/
	RW_CMD_GPL	= 7,
	RW_CMD_DTL	= 8,
	RW_CMD_LEN	= 9
};

/************************************************************************
 * ftxSetDrive
 *
 * Enter FDC unit number into FDC write command.
 ***********************************************************************/
#if	__USE_PROTO__
void ftxSetDrive (unsigned char drive)
#else
void
ftxSetDrive (drive)
unsigned char drive;
#endif
{
	fdcRWCmd[RW_CMD_UNIT] = drive;
}

/************************************************************************
 * ftxC2chk
 *
 * Called by busyWait to check whether IRQ routine has finished.
 * Return nonzero when there is no IRQ pending for block transfer.
 ***********************************************************************/
#if	__USE_PROTO__
static int ftxC2chk (void)
#else
static int
ftxC2chk ()
#endif
{
	if (ftx.ftx_IRQpending == 0)
		return 1;
	else
		return 0;
}

/************************************************************************
 * ftxSendCmd
 *
 * Take request records off queue, processing skips, until a transfer
 * request is encountered, then send that to the FDC.
 * Called from interrupt level *and* from normal priority.
 ***********************************************************************/
#if	__USE_PROTO__
static void ftxSendCmd (void)
#else
static void
ftxSendCmd ()
#endif
{
	int		s;
	struct FtxReq	* reqp;

	s = sphi ();

	for (;;) {

		/* Try to fetch a request from the queue. */
		reqp = ftxReqGet ();

		/* Queue was empty. */
		if (reqp == NULL)
			break;

		/* If block was mapped out by BBT, skip it. */
		if (reqp->ftr_type == FTXR_SKIP) {
			ftx.ftx_bytesXferred += FT_BLK_SIZE;

cmn_err (CE_CONT, "!G block %d, addr %x %d ",
  reqp->ftr_blockno, reqp->ftr_dmaPaddr, __bolt);

		/* Otherwise, block is for real I/O. */
		} else if (reqp->ftr_type == FTXR_XFER) {


cmn_err (CE_CONT, "!G block %d, addr %x, H=%d C=%d R=%d %d ",
  reqp->ftr_blockno, reqp->ftr_dmaPaddr,
  reqp->ftrH, reqp->ftrC, reqp->ftrR, __bolt);

			/* Store floppy pseudocoordinates into command string. */

			fdcRWCmd[RW_CMD_SCTR] = reqp->ftrR;
			fdcRWCmd[RW_CMD_CYL] = reqp->ftrC;
			fdcRWCmd[RW_CMD_HEAD] = reqp->ftrH;

			/* Turn on DMA. */
			if (!dmaon(DMA_CH2, reqp->ftr_dmaPaddr, FT_BLK_SIZE,
			  ftx.ftx_dmaDir)) {
				cmn_err (CE_WARN, "!FT DMA straddle ");
				break;
			}
			dmago(DMA_CH2);
			ftx.ftx_DMA_on = 1;

			/* Tell FDC IRQ handler, let us know when it's over. */
			ftx.ftx_IRQpending = 1;

			/* Send Read or Write command to FDC */
			fdcTblk = reqp->ftr_blockno;
			fdcPutStr(fdcRWCmd, RW_CMD_LEN);

			/* Save block number;  will log it if we get there. */
			ftx.ftx_blockno = reqp->ftr_blockno;

			break;

		/* Invalid request! */
		} else {
			cmn_err (CE_WARN,
			  "ftxSendCmd : Invalid command type %d",
			  reqp->ftr_type);
		}
	}

	spl (s);
}

/************************************************************************
 * ftXferFDC
 *
 * Given starting logical block number, number of blocks, and the
 * physical address of a virtually contiguous buffer region, attempt to
 * read/write between tape and buffer.  Argument "bbtSw" says whether to
 * use the bad block bitmap.
 * Blocks marked bad are skipped over.
 * Argument "iodir" is FT_READ for read, FT_WRITE for write.
 *
 * Tape motion is started at the beginning of the xfer and continues
 * until all data requested has been transferred or skipped over, or a
 * read or write fails; then motion is stopped.
 *
 * Return number of bytes read/written.
 ***********************************************************************/
#if __USE_PROTO__
int ftXferFDC (int logBlock, int numBlocks,
  caddr_t ioBuf, struct ftDiskLocn * dlp, int bbtSw, int ioDir, int retry)
#else
int
ftXferFDC (logBlock, numBlocks, ioBuf, dlp, bbtSw, ioDir, retry)
int logBlock;
int numBlocks;
caddr_t ioBuf;
struct ftDiskLocn * dlp;
int bbtSw;
int ioDir;
int retry;
#endif
{
	char	* dirname;	/* ASCII name of I/O direction, for debug. */
	int	blocksToBeDone;
	int	tapeStarted = 0;
	int	myblock = logBlock;

	__caddr_t	bufVaddr;

	/* Very little is different for read vs. write. Here's the diff. */
	switch (ioDir) {
	case FT_READ:
		ftx.ftx_dmaDir = DMA_TO_MEM;
		fdcRWCmd [RW_CMD_VERB] = FDC_CMD_RD;
		dirname = "read";
		break;
	case FT_WRITE:
		ftx.ftx_dmaDir = DMA_FROM_MEM;
		fdcRWCmd [RW_CMD_VERB] = FDC_CMD_WR;
		dirname = "write";
		break;
	default:
		cmn_err (CE_WARN, "FT : Invalid ioDir = %d", ioDir);
		goto endXferFDC;
	}

cmn_err (CE_NOTE, "!ftXferFDC (%d, %d, %x, %x, %d, %s, %d)",
  myblock, numBlocks, (int) ioBuf, (int) dlp, bbtSw, dirname, retry);

	/* Return now if no work to be done. */

	if (numBlocks <= 0)
		goto endXferFDC;

	/*
	 * Control loop for single-block transfer requests.
	 * The tape is in motion while this loop executes.
	 *
	 * "Producer" makes requests to transfer or skip a block, and puts
	 * them onto a queue so the interrupt handler can fetch them.
	 *
	 * "Consumer 1" starts the chain of i/o ops that run off the request
	 * queue, and watches the state of the interrupt level routine.
	 *
	 * "Consumer 2" runs from the interrupt handler, fetches the next
	 * request from the queue, and updates the transfer count.
	 */
	ftx.ftx_DMA_on = 0;
	ftx.ftx_IRQpending = 0;
	ftx.ftx_bytesXferred = 0;
	ftx.ftx_error = 0;
	ftx.ftx_residual = 0;
	ftx.ftx_stopio = 0;
	ftx.ftx_timedOut = 0;
	ftx.ftx_blockno = 0;
	ftxQinit ();

	blocksToBeDone = numBlocks;
	bufVaddr = ioBuf;

	for (;;) {

		int waitStat;
		int busyWaitStat;

		/* Queue as many requests as there is room for. */
		while (blocksToBeDone > 0) {

			int		logSeg;
			int		offset;
			unsigned int	bitMap;
			struct FtxReq	request;
			struct FtxReq	* reqp = & request;
			__paddr_t	tmpPaddr;

			/* Generate virt address for current block. */
			if (ftBufVtop (bufVaddr, & tmpPaddr)) {

				/* Failure.  Stop queueing requests. */
				blocksToBeDone = 0;
				break;
			}

			/* See if request queue is full. */
			if (ftxReqFreeSize () == 0)
				break;

			reqp->ftr_dmaPaddr = tmpPaddr;
			reqp->ftr_blockno = myblock;

			logSeg = myblock / FT_BLK_PER_SEG;
			offset = myblock % FT_BLK_PER_SEG;

			/* Is the current block good? */

			if (bbtSw == FT_USE_BBT && ftBBT (logSeg, & bitMap) == 0
			 && (bitMap & (1 << offset))) {

				/* Bad block. */
				reqp->ftr_type = FTXR_SKIP;

cmn_err (CE_CONT, "!P block %d, addr %x %d\n",
  reqp->ftr_blockno, reqp->ftr_dmaPaddr, __bolt);

			} else {

				/* Good block.  Do real i/o. */
				reqp->ftr_type = FTXR_XFER;

				/* Store floppy pseudocoordinates into request. */
				reqp->ftrR = dlp->ftDLsector;
				reqp->ftrC = dlp->ftDLcylinder;
				reqp->ftrH = dlp->ftDLhead;

cmn_err (CE_CONT, "!P block %d, addr %x, H=%d C=%d R=%d %d\n",
  reqp->ftr_blockno, reqp->ftr_dmaPaddr,
  reqp->ftrH, reqp->ftrC, reqp->ftrR, __bolt);
			}

			/* reqp now points to a valid request record. */
			ftxReqPut (reqp);

			/* Advance pseudo coordinates to next block. */
			if (ftCDLInc (dlp)) {

				/* Failure.  Stop queueing requests. */
				blocksToBeDone = 0;
				break;
			}

			/* Advance buffer address for next block. */
			bufVaddr += FT_BLK_SIZE;

			blocksToBeDone --;
			myblock++;
		}

		/* Get tape positioned at the right block and moving. */
		if (!tapeStarted) {
			tapeStarted = 1;
			if (ftMPosTape (logBlock)) {
				cmn_err (CE_WARN,
				  "FT : Position for Transfer Failed");
				goto endXferFDC;
			}
		}

		
		/* Check on status of interrupt handler. */
		if (ftx.ftx_error) {
cmn_err (CE_WARN, "!ftx err %d", ftx.ftx_error);
			break;
		}

		/* If no interrupt is pending, issue a request. */
		if (!ftx.ftx_IRQpending && ftxReqSize () > 0) {
cmn_err(CE_CONT, "!Level 1 send ");
			ftxSendCmd ();
		}

		/* If all work is done, quit. */

cmn_err(CE_CONT, "!btbd=%d frs=%d irqpd=%d\n",
  blocksToBeDone, ftxReqSize (), ftx.ftx_IRQpending);

		if (blocksToBeDone == 0 && ftxReqSize () == 0
		  && ftx.ftx_IRQpending == 0)
			break;

		/* Sleep while interrupt routine dequeues requests. */
cmn_err (CE_CONT, "!sleep %d\n", __bolt);
		waitStat = ftWaitWithTimeout (FT_XFER_SECS * HZ, "ftXfer");

		/* If signaled, wait for an IRQ before leaving the loop. */
		if (waitStat == FT_RESULT_SIGNALED) {

cmn_err (CE_NOTE, "!ftx signaled");

			/* Tell Consumer 2 not to start any more blocks. */
			ftx.ftx_stopio = 1; 

			/* Busywait for Consumer 2 to finish current request. */
			busyWaitStat = busyWait (ftxC2chk, FT_XFER_SECS * HZ);

			if (busyWaitStat != 0) {
				/* Consumer 2 finished. */
				break;
			}

cmn_err (CE_NOTE, "!ftx timed out 1");

			/* Alas, timed out waiting for IRQ. */
			ftx.ftx_timedOut = 1; 
		}

		/* If timed out, stop Consumer 2 then do cleanup below. */
		if (waitStat == FT_RESULT_TIMED_OUT) {

cmn_err (CE_NOTE, "!ftx timed out 2 %d", __bolt);

			ftx.ftx_timedOut = 2; 
		}

		/* Timed out, do some recovery then leave the loop. */
		if (ftx.ftx_timedOut != 0) {
			cmn_err (CE_WARN, "!ftXfer - timeout %d",
			  ftx.ftx_timedOut);
			ftx.ftx_IRQpending = 0;
			ftSelect(ft.ft_drv, ft.ft_softsel);
			break;
		}
	}

	/* Turn off the DMA channel, in case of error break from loop. */

	if (ftx.ftx_DMA_on) {
		ftx.ftx_residual = dmaoff (DMA_CH2);
		ftx.ftx_DMA_on = 0;
	}

	if (ftx.ftx_residual != 0) {
		cmn_err (CE_WARN,
		  "!FT : ftXferFDC : DMA residual %d bytes ",
		  ftx.ftx_residual);
	}

	/* Stop the tape. */
	ftStopTape ();

endXferFDC:
	cmn_err (CE_NOTE, "!ftXferFDC : ok %s of %d bytes",
	  dirname, ftx.ftx_bytesXferred);

	return ftx.ftx_bytesXferred;
}

/************************************************************************
 * ftxIntr
 *
 * Consumer 2 part of block transfer.
 * Runs as part of the FDC IRQ handler.
 ***********************************************************************/
#if	__USE_PROTO__
void ftxIntr (void)
#else
void
ftxIntr ()
#endif
{
cmn_err (CE_CONT, "!I:XI %d ", __bolt);

	/* Wrap up the request that just ended. */

	/* Conclude DMA transfer. */
	ftx.ftx_residual = dmaoff (DMA_CH2);
	ftx.ftx_DMA_on = 0;

	/* Did the FDC report an error? */
	if ((fdc.fdc_cmdstat [0] & ST0_IC) != ST0_NT) {
		ftx.ftx_error = 2;
cmn_err (CE_CONT, "!err2\n");
		fdcStatus ();
		wakeup(&ft.ft_wakeMeUp);
		return;
	}

	/* Set error status (will stop i/o) if DMA was not complete. */
	if (ftx.ftx_residual != 0) {
		ftx.ftx_error = 1;
cmn_err (CE_CONT, "!err1\n");
		wakeup(&ft.ft_wakeMeUp);
		return;
	}

	/* A block was successfully transferred. */
	ftx.ftx_bytesXferred += FT_BLK_SIZE;

	/* Make a note of our location on the tape. */
	ftMSetLocB (ftx.ftx_blockno);

	/* Has normal priority level requested a stop to i/o? */
	if (ftx.ftx_stopio) {
		ftx.ftx_stopio = 0;
cmn_err (CE_CONT, "!stopio\n");
		wakeup(&ft.ft_wakeMeUp);
		return;
	}

	/* Now - immediately the previous block is done, command the next. */
	if (ftxReqSize () > 0) {
cmn_err (CE_CONT, "!Level 2 send\n");
		ftxSendCmd ();
	}

	/* If the queue falls below threshold, alert control level. */
	if (ftxReqFreeSize () > 3) {
cmn_err (CE_CONT, "!wake ");
		wakeup(&ft.ft_wakeMeUp);
	}

cmn_err (CE_CONT, "!endXI\n");
	return;
}
