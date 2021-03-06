/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef	__SYS_AHA154X_H__
#define	__SYS_AHA154X_H__

/*
 *	describe the data structures neccessary to
 *	access the Hard/Firmware of the AHA-154x family.
 *
 *	First the hardware which consists of three I/O ports.
 */

#define	AHA_CONTROL	(0*0+0)
#define	AHA_HARDRESET	0x80	/* Hard */
#define	AHA_SOFTRESET	0x40	/* Soft */
#define	AHA_INTRRESET	0x20	/* Interrupt */
#define	AHA_SCSIRESET	0x10	/* SCSI */
/* ... Reserved ... */

#define	AHA_STATUS	(0*0+0)
#define	AHA_SELFTEST	0x80		/* SELF TEST in progress */
#define	AHA_DIAGFAIL	0x40		/* DIAGnostics FAILed */
#define	AHA_INITMAIL	0x20		/* INIT of MAILbox required */
#define	AHA_SCSIIDLE	0x10		/* SCSI host adaptor IDLE */
#define	AHA_CDOPFULL	0x08		/* Command/Data Out Port FULL */
#define	AHA_DIPFULL	0x04		/* Data In Port FULL */
/* ... Reserved ... */
#define	AHA_INVDCMD	0x01		/* INValiD CoMmanD */

#define	AHA_READ	(0*0+1)
#define	AHA_WRITE	(0*0+1)

#define	AHA_INTERRUPT	(0*0+2)
#define	AHA_ANY_INTER	0x80
/* ... Reserved ... */
#define	AHA_RESETED	0x08
#define	AHA_CMD_DONE	0x04
#define	AHA_MBO_EMPTY	0x02
#define	AHA_MBI_STORED	0x01
#define	AHA_ALL_INTERRUPTS	\
	(AHA_RESETED|AHA_CMD_DONE|AHA_MBO_EMPTY|AHA_MBI_STORED)

/*
 *	Command Control Block Opcodes.
 */
#define AHA_OP_SIC	0x00	/* SCSI Initiator Command Control Block */
#define AHA_OP_TCC	0x01	/* SCSI Target Command Control Block */
#define AHA_OP_SIC_SG	0x02	/* SCSI Initiator Command Control Block
			 	* with Scatter/Gather */
#define AHA_OP_SIC_res	0x03	/* SCSI Initiator Command,
				 * residual data length returned.  */
#define AHA_OP_SIC_SG_res 0x04	/* SCSI Initiator Command
				 * with Scatter/Gather,
				 * residual data length returned.  */
#define AHA_OP_SPQR	0x81	/* SCSI Bus Device Reset
				 * (a work of the people :-)  */
#define AHA_OP_INVALID	0x22	/* This randomly chosen command is used to
				 * mark a ccb as invalidated. */

/*
 *	Adapter Firmware Components
 */
#define	AHA_DO_NOP		0x00
#define	AHA_DO_MAILBOX_INIT	0x01
	/* DATA send = MB count, MB adr[3] */
#define	AHA_DO_SCSI_START	0x02
#define	AHA_DO_BIOS_START	0x03
#define	AHA_DO_INQUIRY		0x04
	/* DATA recv =	board id (ASCII)
			special options id (standard = 'A')
			HW rev level (ASCII)
			FirmWare rev level (ASCII)*/
#define	AHA_DO_MBO_INTR_ON	0x05
	/* DATA send = 0/1 = Dis/Enable INTR */
#define	AHA_DO_SET_TIMEOUT	0x06
	/* DATA send =	0/1 = Dis/Enable Timeout
			...reserved...
			value[MSB] default = 250 millisec
			value[LSB] */
#define	AHA_DO_BUS_ON_TIME	0x07
	/* DATA send =	2 to 15 microsec. */
#define	AHA_DO_BUS_OFF_TIME	0x08
	/* DATA send =	1 to 64 microsec. */
#define	AHA_DO_XFER_SPEED	0x09
#define	AHA_SPEED_5_0_MB	0x00
#define	AHA_SPEED_6_7_MB	0x01
#define	AHA_SPEED_8_0_MB	0x02
#define	AHA_SPEED_10_MB		0x03
#define	AHA_SPEED_5_7_MB	0x04
#define	AHA_SPEED_CUSTOM	0x80
	/* see Chap. 5.1.1.10 */
#define	AHA_DO_GET_DEVICES	0x0A
	/* DATA recv =	eight bytes, one per target
		whereby each bit indicates if a LUN is installed */
#define	AHA_DO_GET_CONFIG	0x0B
	/* DATA recv =	DMA Arb. Priority,
			Inter. Channel,
			SCSI Id. */
#define	AHA_DO_TARGET_ON	0x0C
	/* DATA send =	0/1 = Dis/Enable Target Mode
			LUN mask, 1 -> LUN will respond in Target Mode */
#define	AHA_DO_GET_SETUP	0x0D
	/* DATA send =	n, count requested, 0 -> 256 bytes
	   DATA recv =	00 SDT + Parity,
			01 Transfer Speed,
			02 Bus On Time,
			03 Bus Off Time,
			04 # MB's,
			05-07 MailBox Adr,
			08-0F Sync Transfer Agreements,
			10-FF Reserved (default 00) */
			
/* ... reserved ... */
#define	AHA_DO_PUT_CH2_BUF	0x1A
	/* DATA send =	00-02 Buffer Area (64 bytes) Address */
#define	AHA_DO_GET_CH2_BUF	0x1B
	/* DATA send =	00-02 Buffer Area (64 bytes) Address */
#define	AHA_DO_PUT_FIFO_BUF	0x1C
	/* DATA send =	00-02 FIFO Area (54 bytes) Address */
#define	AHA_DO_GET_FIFO_BUF	0x1D
	/* DATA send =	00-02 FIFO Area (54 bytes) Address */
/* ... reserved ... */
#define	AHA_DO_ECHO_CMD_DATA	0x1F
	/* DATA send =	1 byte
	   DATA recv =	1 byte, hopefully the same one ! */

/* A P3 is a 3 byte physical address.  */
typedef unsigned char P3[3];

typedef	struct	{
#	define	MBO_IS_FREE	0x00
#	define	MBO_TO_START	0x01
#	define	MBO_IS_ABORT	0x02
	char	state;
	P3	ccb_adr;
}	MBO;

/*
 * MBI for CCB completed
 */
typedef	struct	{
#	define	MBI_IS_FREE	0x00
#	define	MBI_NO_ERROR	0x01
#	define	MBI_IS_ABORT	0x02
#	define	MBI_NOT_FOUND	0x03
#	define	MBI_HAS_ERROR	0x04
	char	state;
	/* MBI for CCB required */
	/* byte 0 */
#	define	MBI_CCB_REQUEST	0x10
		/* byte 1 */
#		define	MBI_MSK_INITOR	0xE0
#		define	MBI_WAS_READ	0x10
#		define	MBI_WAS_WRITE	0x08
#		define	MBI_MSK_LUN	0x03
		/* byte 2,3 are data len */
	P3	ccb_adr;
}	MBI;

#ifndef	MAX_SCSICMD
#define	MAX_SCSICMD	14
#endif	/* MAX_SCSICMD */

#ifndef	MAX_SENSEDATA
#define	MAX_SENSEDATA	27
#endif	/* MAX_SENSEDATA */

typedef	struct	{
	unsigned char	cmd;
	P3	adr;
}	mailentry;

#define	SENSE_UNIT_ATTENTION	6
#define	CHECK_TARGET_STATUS	2
typedef	struct	{
	char	opcode;
	char	target;
#define	AHA_CCB_DATA_OUT	0x10
#define	AHA_CCB_DATA_IN		0x08
	char	cmdlen;
	char	senselen;
	P3	datalen;
	P3	dataptr;
	P3	linkptr;
	char	linkid;
	char	hoststatus;
	char	targetstatus;
	char	reserve[2];
	unsigned char	cmd_status[MAX_SCSICMD+MAX_SENSEDATA];
	scsi_work_t	*ccb_sw;
	unsigned char	buffer[1];
}	ccb_t;
 
/* These macros are for stashing away P3 addresses for ccbs.  */
#define ccb_forget( a_ccb )	mem_forget( a_ccb )
#define ccb_remember( a_ccb, a_p3 ) \
	mem_remember( (a_ccb), aha_p3_to_l( a_p3 ) )
#define ccb_recall( a_p3 )	mem_recall( aha_p3_to_l( a_p3 ) )

typedef struct {
	P3 size;
	P3 addr;
} DSL_ENTRY;	

#endif	/* ! defined (__SYS_AHA154X_H__) */
