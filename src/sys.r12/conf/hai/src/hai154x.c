/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1994 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */
/***********************************************************************
 *  Module: hai154x.c
 *
 *  Host Adapter Interface routines for the Adaptec AHA154XX series
 *  of host adapters.
 *
 *  Much of the information used to create this driver was obtained
 *  from the AHA1540B/1542B Technical Reference Manual available from
 *  Adaptec by phone or snail mail at:
 *
 *	  Adaptec - Literature Department
 *	  691 South Milpitas Blvd.
 *	  Milpitas, CA 95035
 *	  (408) 945-8600
 *
 *  Copyright (c), 1993 Christopher Sean Hilton, All Rights Reserved.
 *
 *  Last Modified: Wed Aug 25 07:19:10 1993 CDT
 */

#define HA_MODULE	   	/* Host Adapter Module */

#include <stddef.h>
#include <sys/coherent.h>
#include <sys/con.h>
#include <sys/devices.h>
#include <sys/types.h>
#include <sys/hdioctl.h>

#include <sys/haiscsi.h>

/*
 * Configurable variables - see /etc/conf/hai/Space.c
 */

extern unsigned short	HAI_AHABASE;
extern unsigned short	HAI_AHAINTR;
extern unsigned short	HAI_AHADMA;

extern int		HAI_SD_HDS;
extern int		HAI_SD_SPT;

extern unsigned char hai_xfer_speed;
extern unsigned char hai_bus_off_time;
extern unsigned char hai_bus_on_time;

#define CTRLREG		(HAI_AHABASE + 0)	/* Control Register (Write) */
#define	 HRST		bit(7)	/* Hard Reset */
#define	 SRST		bit(6)	/* Soft Reset */
#define	 IRST		bit(5)	/* Interrupt Reset */
#define	 SCRST   	bit(4)	/* SCSI Bus Reset */

#define STSREG	  	(HAI_AHABASE + 0)	/* Status Register (Read) */
#define	 STST		bit(7)	/* Self Test in progress */
#define	 DIAGF   	bit(6)	/* Internal Diagnostic Failure */
#define	 INIT		bit(5)	/* Mailbox Initialization Required */
#define	 IDLE		bit(4)	/* SCSI Host Adapter Idle */
#define	 CDF	 	bit(3)	/* Command/Data Out Port Full */
#define	 DF	  	bit(2)	/* Data In Port Full */
#define	 INVDCMD 	bit(0)	/* Invalid HA Command */

#define CMDDATAOUT 	(HAI_AHABASE + 1)   /* Command/Data Out (Write) */
#define	 NOP		0x00	/* No Operation (really?) */
#define	 MBINIT	  	0x01	/* Mail Box Initialization */
#define	 STARTSCSI   	0x02	/* Start a SCSI Command */
#define	 STARTBIOS   	0x03	/* Start a BIOS Command */
#define	 HAINQUIRY   	0x04	/* HA Inquiry */
#define	 ENBLMBOA	0x05	/* Enable Mailbox Out Available Interrupt */
#define	 SETSELTO	0x06	/* Set Selection Timeout */
#define	 SETBUSON	0x07	/* Set Bus on time */
#define	 SETBUSOFF   	0x08	/* Set Bus off time */
#define	 SETXFERSPD	0x09	/* Set transfer speed */
#define	 RETINSTDEV  	0x0a	/* Return Installed Devices */
#define	 RETCFGDATA  	0x0b	/* Return Configuration Data */
#define	 ENBLTRGTMD  	0x0c	/* Enable Target Mode */
#define	 RETSUDATA   	0x0d	/* Return Setup Data */

/***********************************************************************
 *  These commands are specific to the Adaptec AHA-154xC.
 */

#define RETEXTBIOS	0x28	/* Return Extended BIOS Information */
#define  EXTBIOSON	bit(3)

#define SETMBIENBL	0x29	/* Set Mailbox Interface Enable */
#define  MBIENABLED	bit(1)

#define DATAIN	  	(HAI_AHABASE + 1)

#define INTRFLGS	(HAI_AHABASE + 2)
#define	 ANYINTR 	bit(7)	/* Any Interrupt */
#define	 SCRD		bit(3)	/* SCSI Reset Detected */
#define	 HACC		bit(2)	/* HA Command Complete */
#define	 MBOA		bit(1)	/* MBO Empty */
#define	 MBIF		bit(0)	/* MBI Full */

#define MBOFREE		0x00	/* Mailbox out is free */
#define MBOSTART	0x01	/* Start CCB in this Mailbox */
#define MBOABORT	0x02	/* Abort CCB in this Mailbox */

#define MBIFREE		0x00	/* Mailbox in is free */
#define MBISUCCESS	0x01	/* Mailbox's CCB Completed Successfully */
#define MBIABORTED	0x02	/* Mailbox's CCB Aborted */
#define MBIABRTFLD	0x03	/* CCB to Abort not found */
#define MBIERROR	0x04	/* CCB Completed with error */

#define TIMEOUT		-1	/* Timeout from pollxxx() functions. */

#define ST_HAINIT   	0x0001	/* Host Adapter being initialized */
#define ST_HAIDLE   	0x0002	/* Host Adapter is idle */

/***********************************************************************
 *  Types
 */

#pragma align 1

typedef union addr3_u {		 /* addr3_u for big/little endian conversions */
	unsigned long   value;
	unsigned char   byteval[sizeof(unsigned long)];
} addr3_t;

typedef union mbo_u *mbo_p;	 /* Out Box to host adapter */

typedef union mbo_u {
	unsigned char   cmd;
	paddr_t		ccbaddr;
} mbo_t;

typedef union mbi_u *mbi_p;	 /* In Box from host adapter */

typedef union mbi_u {
	unsigned char   sts;
	paddr_t		ccbaddr;
} mbi_t;

typedef struct mb_s {		/* Host adapter mailbox type */
	mbo_t	   o[MAXDEVS];	/* One out box for each device */
	mbi_t	   i[MAXDEVS];	/* One in box for each possible reply */
} mb_t;

typedef struct haccb_s *haccb_p;	/* Host Adapter Command/Control Block */

typedef struct haccb_s {
	unsigned char   opcode;
	unsigned char   addrctrl;
	unsigned char   cdblen;
	unsigned char   senselen;
	unsigned char   datalen[3];
	unsigned char   bufaddr[3];
	unsigned char   linkaddr[3];
	unsigned char   linkid;
	unsigned char   hoststs;
	unsigned char   trgtsts;
	unsigned char   pad[2];
	cdb_t		cdb;
} haccb_t;

typedef struct dsentry_s *dsentry_p;

typedef struct dsentry_s {
	unsigned char   size[3];
	unsigned char   addr[3];
} dsentry_t;

typedef struct dslist_s *dslist_p;

typedef struct dslist_s {
	dsentry_t	entries[16];
} dslist_t;

#pragma align

/***********************************************************************
 *  Variables
 */
int		hapresent;	/* Host adapter present flag */
static int	hastate;	/* Host Adapter State */
static int	hainit_stsreg = 0;	/* Host Adapter Command Complete flag. */
static int	hainit_ccflag = 0;	/* Host Adapter Command Complete flag. */
static mb_t	mb;		/* Mailboxes for host adapter */	
static haccb_t  ccb[MAXDEVS];   /* CCB's for mailboxes */
static paddr_t  ccbbase;	/* ccbbase address for quick checkmail */
static srb_p	actv[MAXDEVS];  /* Active srb's for each target */
static dslist_t ds[MAXDEVS];	/* Data segment lists one for each target */
static unsigned chkport = 0,	/* Port for chkset/ckhclr */
		chkstop = 0,	/* Target value for chkset/chkclr */
		chkval  = 0;	/* Value in port chkset/chkclr */


/***********************************************************************
 *  Support Functions
 */

#ifndef DEBUG
#define dbg_printf(lvl, msg)
#else
unsigned AHA_DBGLVL = 1;
#define dbg_printf(lvl, msg)	{ if (AHA_DBGLVL >= lvl) printf(msg); }
#endif

#define min(a, b)   (((a) <= (b)) ? (a) : (b))

/***********************************************************************
 *  chkclr()	--  Check port (chkport) for bits (chkstop) to be clear.
 *				  If clear return 1 else return 0.  Leave value of
 *				  port in chkval;
 */

static int
chkclr()
{
	return ((chkval = inb(chkport)) & chkstop) == 0;
}   /* chkclr() */

/***********************************************************************
 *  chkset()	--  Check port (chkport) for bits (chkstop) to be set.
 *				  If all bits are set return 1 else return 0. Leave
 *				  value of port in chkval.
 */

static int
chkset()
{
	return ((chkval = inb(chkport)) & chkstop) == chkstop;
}   /* chkset() */

/***********************************************************************
 *  pollclr()   --  Wait usec milliseconds for bit(s) to clear in a
 *				  port.
 */

static int
pollclr(port, bits, usec)
register unsigned	port;	   /* port to watch */
register unsigned	bits;	   /* bits to watch for */
unsigned		usec;	   /* number of milliseconds to wait for */
{
	register s;

	s = sphi();
	chkport = port;
	chkstop = bits;
	busyWait(chkclr, usec);
	spl(s);
	
#if 1   /* DEBUG */
	if ((chkval & bits) == 0)
		return chkval;
	else {
		printf("pollclr: <Timeout Reg: 0x%x mask 0x%x value 0x%x>\n", port, bits, chkval);
		return TIMEOUT;
	}
#else
	return ((chkval & bits) == 0) ? chkval : TIMEOUT;
#endif	
}   /* pollclr() */

/***********************************************************************
 *  pollset()   --  Wait usec milliseconds for bit(s) set in a port.
 */

static int
pollset(port, bits, usec)
register unsigned	port;	   /* port to watch */
register unsigned	bits;	   /* bits to watch for */
unsigned		usec;	   /* number of milliseconds to wait for */
{
	register s;

	s = sphi();
	chkport = port;
	chkstop = bits;
	busyWait(chkset, usec);
	spl(s);

#if 1   /* DEBUG */
	if ((chkval & bits) == bits)
		return chkval;
	else {
		printf("pollset: <Timeout Reg: 0x%x mask 0x%x value 0x%x>\n", port, bits, chkval);
		return TIMEOUT;
	}
#else
	return ((chkval & bits) == bits) ? chkval : TIMEOUT;
#endif	
}   /* pollset() */

/***********************************************************************
 *  hacc()
 *
 *  Host Adapter Command Completed - Returns 1 if last host adapter
 *  command completed without error.
 */

static int
hacc()
{
	register unsigned stsreg;

	if (pollset(INTRFLGS, HACC, 350) == TIMEOUT)
		return 0;

	stsreg = pollset(STSREG, IDLE, 350) & (IDLE | INIT | CDF | INVDCMD);

	if (stsreg != IDLE) {
		printf("Aha154x stuck - STSREG: 0x%x\n", stsreg);
		return 0;
	}
	return 1;
}   /* hacc() */


/***********************************************************************
 *  haidle()
 *
 *  Returns 1 if the Idle Bit is on in the adapter status register
 */

#define haidle()	(pollset(STSREG, IDLE, 350) != TIMEOUT)

/***********************************************************************
 *  gethabyte()
 *
 *  Get a byte from the host adapter Data In register.
 */

static int
gethabyte()
{
	if (pollset(STSREG, DF, 350) == TIMEOUT) {
		printf("haiscsi: <gethabyte timeout (0x%x) 0x%x 0x%x>\n", STSREG, DF, chkval);
		return TIMEOUT;
	}
	else
		return (inb(DATAIN) & 0xff);
}   /* gethabyte() */

/***********************************************************************
 *  puthabyte()
 *
 *  Write a byte to the host adapter Command/Data Out Register.
 */

static int
puthabyte(b)
unsigned char b;
{
	if (pollclr(STSREG, CDF, 350) == TIMEOUT)
		return 0;
	else {
		outb(CMDDATAOUT, b);
		return 1;
	}
}   /* puthabyte() */

/***********************************************************************
 *  Function:   aha_icmd()
 *  
 *  Send a command to the host adapter. and do all the handshaking
 *  etc. 
 *  
 *  This is used to send a multi-byte command to the host adapter without
 *  having to individually handshake each byte going out or comming
 *  back over the wire. 
 */

static int aha_icmd(cmd, cmdlen, data, datalen)
unsigned char 	cmd[];
size_t		cmdlen;
unsigned char	data[];
size_t		datalen;
{
	int 		errorflag = 0;
	int		ch;
	unsigned char	*p;


	if ((pollset(STSREG, IDLE, 35) & IDLE) == 0) {
		printf("<hai: Timeout waiting for host adapter idle (hai154x)>\n");
		errorflag = 1;
	}	/* if */
	else {
                /*******************************************************
                 *  Send as many of the command bytes as possible to
		 *  the host.
                 */

		hainit_stsreg = 0;
		p = cmd;
		while (p < (cmd + cmdlen) && !errorflag) {
			errorflag = !puthabyte(*p);
			++p;
		}
        
                /*******************************************************
                 *  If the handshake failed then message to that effect.
                 */
                
                if (errorflag) {
                        printf("<hai: Command handshake failed (hai154x)>\n");
                }
        
                /*******************************************************
                 *  If the command was invalid flag it.
                 */
        
                else if (hainit_stsreg & INVDCMD) {
                        printf("<hai: Invalid command or parameter (hai154x)>\n");
                        errorflag = 1;
                }

                /*******************************************************
                 *  Everything seems okay, try to fill up the data
                 *  buffer.
                 */

		else if (data && datalen) {
                        p = data;
                        while (p < (data + datalen) && !errorflag) {
                        	errorflag = ((ch = gethabyte()) == TIMEOUT);
                               	*p++ = ch;
                        }      	/* while */
		}
	}	/* if - else */

	/***************************************************************
         *  In case of any sort of error handshake bytes into the bit
         *  bucket.
         */

	while (inb(STSREG) & DF) {
		ch = gethabyte();
		busyWait(NULL, 1);
	}

	return !errorflag;		
}	/* aha_icmd() */

/***********************************************************************
 *  Function:   aha_checkbios()
 *  
 *  On the AHA154xC/154xCF check to see if the user is using the BIOS
 *  extensions available on this card. If so turn them off. This also
 *  determines if the card is using 255 head translation.
 */

static int aha_checkbios()

{
	unsigned char 	cmddata[3];
	int 		retval;	

	cmddata[0] = RETEXTBIOS;
	hainit_ccflag = 0;
	if (!aha_icmd(cmddata, 1, cmddata + 1, 2)) {
		printf("<hai: Return extended BIOS information failed (hai154x)>\n");
		return 0;
	}

	retval = (cmddata[1] & EXTBIOSON) ? 255 : 64;
	if (cmddata[2] == 0)
		return retval;

	cmddata[0] = SETMBIENBL;
	cmddata[1] = 0;
	hainit_ccflag = 0;
	if (!aha_icmd(cmddata, 3, NULL, 0)) {
		printf("<hai: Set mailbox interface enable failed (hai154x)\n");
		return 0;
	}
	return retval;
}	/* aha_checkbios() */

/***********************************************************************
 *  Function:   aha_inquiry()
 *  
 *  Do an inquiry on the host adapter to find out what it's capable
 *  of.
 *  
 *  I don't like having to do this. It makes us vulnerable to the whim
 *  of hardware manufacturers and virtually guarantees incompatiblity
 *  with future host adapters until we get a chance to put in a fix
 *  for them. Unfortunatly I cannot figure out any other way to determine
 *  if the host is a 154xC/CF so I can check to see if the extended
 *  BIOS has been turned on. [csh]
 */

static int aha_inquiry()

{
	unsigned char 	buf[5];
	unsigned char 	*command	= &buf[0],
			*data		= &buf[1];

	command[0] = HAINQUIRY;
	if (!aha_icmd(command, 1, data, 4)) {
		printf("<hai: Host adapter inquiry command timeout (hai154x)>\n");
		return -1;
	}
	dbg_printf(1, ("     Adapter Rev:  %c %c", data[2], data[3]));

	return data[0];
}	/* aha_inquiry() */
/***********************************************************************
 *  Function:   aha_tunescsibus()
 *  
 *  Set host adapter bus on/bus off times from the external constants
 *  patched into the kernel. These constants are currently selectable
 *  by patching them into the kernel.  They should be set up to work
 *  with idtune. [csh]
 */

static int aha_tunescsibus()

{
	unsigned char 	busonbuf[2],
			*busoncmd	= &busonbuf[0],
			*busondata	= &busonbuf[1],
			busoffbuf[2],
			*busoffcmd	= &busoffbuf[0],
			*busoffdata	= &busoffbuf[1],
			xferspdbuf[2],
			*xferspdcmd 	= &xferspdbuf[0],
			*xferspddata	= &xferspdbuf[1];

	/***************************************************************
         *  Set up command buffers for bus on/off times and xfer speed.
         */

	busoncmd[0] = SETBUSON;
	busondata[0] = hai_bus_on_time;

	busoffcmd[0] = SETBUSOFF;
	busoffdata[0] = hai_bus_off_time;

	xferspdcmd[0] = SETXFERSPD;
	xferspddata[0] = hai_xfer_speed;
	
        /*******************************************************
         *  Set the bus on time.
         */

        if (/* hai_bus_on_time != DEFAULT  && */
	    !aha_icmd(busoncmd, 2, NULL, 0)) {
                printf("<hai: Set bus on time failed>\n");
		return 0;
	}

        /*******************************************************
         *  And finally the bus off time.
         */
        if (/* hai_bus_off_time != DEFAULT && */
	    !aha_icmd(busoffcmd, 2, NULL, 0))  {
                printf("<hai: Set bus off time failed>\n");
		return 0;		
	}

        if (/* hai_bus_off_time != DEFAULT && */
	    !aha_icmd(xferspdcmd, 2, NULL, 0))  {
                printf("<hai: Set bus off time failed>\n");
		return 0;		
	}

	return 1;
}	/* aha_tunescsibus() */

/***********************************************************************
 *  Function:   aha_hareset()
 *  
 *  Reset the host adapter. There are two types of reset. A hard reset
 *  resets both the host adapter and all the devices attached to the
 *  bus. A soft reset only resets the host adapter. During initialization
 *  a hard reset is called for. This seems to be the only thing that
 *  will work with the AHA-154xC adapters as well as the 174x adapters
 *  operating in standard mode. The Bus Logic controllers also seem
 *  to need the hard reset. During operation a soft reset is called
 *  for as this will allow operations on the devices attached to continue
 *  without problems. When calling this function as a part of error
 *  recovery try the soft reset first before going to the hard reset.
 */

static int aha_hareset(hardreset)
int	hardreset;
{
	paddr_t 	mbaddr;		/* Mail box array's paddr. */
	unsigned	stsreg;
	unsigned char	mbibuf[5],
			*mbicmd 	= &mbibuf[0],
			*mbidata	= &mbibuf[1];
	int		retries = 0;

	/***************************************************************
         *  Set up the mailbox command in the buffer.
         */

	mbaddr = vtop(&mb);
	mbicmd[0] = MBINIT;
	mbidata[0] = (sizeof(mb) / (sizeof(mbo_t) + sizeof(mbi_t)));
	mbidata[1] = (( unsigned char *) &mbaddr)[2];
	mbidata[2] = (( unsigned char *) &mbaddr)[1];
	mbidata[3] = (( unsigned char *) &mbaddr)[0];

	do {
		/*******************************************************
                 *  If desired try a soft reset first. The soft reset
                 *  doesn't halt operations in progress on devices on
                 *  the bus so if the soft reset will work then more
                 *  power to it. If the soft reset doesn't cut go back
                 *  to the hard reset.
                 */

		if (hardreset || retries > 0)
			outb(CTRLREG, HRST);
		else
			outb(CTRLREG, SRST);

		/*******************************************************
                 *  Wait for the self-test status to clear in the status
                 *  register. give it 100 clock ticks (1000 millisec).
                 */

		stsreg = pollclr(STSREG, STST, 100);
		if (stsreg == TIMEOUT)
			printf("<hai: Host adapter self-test timeout>\n");

		/*******************************************************
                 *  After the self test clears make sure the diagnostics
                 *  didn't fail.
                 */
		
		else if (stsreg & DIAGF)
			printf("<hai: Host adapter diagnostic failed>\n");

		/*******************************************************
                 *  If the diagnostics passed the host adapter should
                 *  now be idle waiting for mailbox initilization.
                 */

		else if ((stsreg & (INIT | IDLE)) != (INIT | IDLE))
			printf("<hai: Host adapter stuck (0x%x) (hai154x)>", stsreg);

		/*******************************************************
                 *  Initialize the mailboxes.
                 */

		else if (!aha_icmd(mbicmd, 5, NULL, 0))
			printf("<hai: Host adapter mailbox init failed (hai154x)>\n");

		/*******************************************************
                 *  Lastly, attempt to tune the SCSI Bus.
                 */

		else if (!aha_tunescsibus())
			printf("<hai: Set SCSI bus parameters failed (hai154x)>\n");

      		/*******************************************************
                 *  Complete success, return.
                 */

		else
			return 1;
		

	} while (++retries < 1);
	return 0;
}	/* aha_hareset() */

#define hardreset()	aha_hareset(1)
#define softreset()	aha_hareset(0)

#if 0		/* Scheduled for removal */

/***********************************************************************
 *  hareset()
 *
 *  Reset the host adapter and leave it ready for operation.
 *  Return 1 on success, 0 on failure.
 */

static int
hareset()
{
	paddr_t mbaddr;		/* Mail box array's paddr. */
	int	mbiok,		/* Mail box initialization proceding ok */
		stsreg,		/* local copy of STSREG */
		retval,		/* return value */
		s;

	retval = 0;
	s = sphi();

	outb(CTRLREG, HRST);
	if ((stsreg = pollclr(STSREG, STST, 350)) == TIMEOUT) {
		printf("hai154x: Reset failed, host adapter stuck.\n");
		spl(s);
		return retval;
	}

	if ((stsreg & DIAGF) || (stsreg & (INIT | IDLE)) != (INIT | IDLE)) {
		printf("AHA-154x: Reset/diagnostics failed.\n");
		spl(s);
		return retval;
	}

	mbaddr = vtop(&mb);
	mbiok = 1;
	mbiok &= puthabyte(MBINIT);
	mbiok &= puthabyte(sizeof(mb) / (sizeof(mbo_t) + sizeof(mbi_t)));
	mbiok &= puthabyte(((unsigned char *) &mbaddr)[2]);
	mbiok &= puthabyte(((unsigned char *) &mbaddr)[1]);
	mbiok &= puthabyte(((unsigned char *) &mbaddr)[0]);
	if (!mbiok || !hacc()) {
		printf("hai154x: Mailbox initialization failed.\n");
		outb(CTRLREG, IRST);
		spl(s);
		return retval;
	}

	/*
	 * Note on outb(CTRLREG, IRST) appearing below:
	 *
	 * Fake interrupt service on the pending hacc stuff.
	 * Needs to be reworked.
	 * 
	 * The right way would be to have interrupts enabled and
	 * then initialize the card so the interrupt routine
	 * can tell the card that the hacc's have been read.
	 * 
	 * Interrupt service needs to be available if sending a lot
	 * of commands.  This may be part of the problem with Bob.H's
	 * board.
	 */

	/*
	 * Clean up after the last hacc() call above.
	 */
	outb(CTRLREG, IRST);

	/* Set the aha xfer rate */
	printf("hai154x Xferspeed: %d  ", (unsigned int) hai_xfer_speed);

	mbiok = 1;
	mbiok &= puthabyte(SETXFERSPD);
	mbiok &= puthabyte(hai_xfer_speed);
	if(!mbiok || !hacc()) {
		outb(CTRLREG, IRST);
		spl(s);
		return retval;
	}
	outb(CTRLREG, IRST);


	/* Set the aha bus off time */
	printf("Bus off: %d  ", (unsigned int)hai_bus_off_time);

	mbiok = 1;
	mbiok &= puthabyte(SETBUSOFF);
	mbiok &= puthabyte(hai_bus_off_time);
	if(!mbiok || !hacc()) {
		outb(CTRLREG, IRST);
		spl(s);
		return retval;
	}
	outb(CTRLREG, IRST);

	/* Set the aha bus on time */
	printf("Bus on: %d\n", (unsigned int) hai_bus_on_time);

	mbiok = 1;
	mbiok &= puthabyte(SETBUSON);
	mbiok &= puthabyte(hai_bus_on_time);
	if(!mbiok || !hacc()) {
		outb(CTRLREG, IRST);
		spl(s);
		return retval;
	}
	outb(CTRLREG, IRST);
	retval = 1;

	spl(s);
	return retval;
}   /* hareset() */

#endif

/***********************************************************************
 *  dmacascade()
 *
 *  Set the selected (HAI_AHADMA) dma channel to cascade mode.
 */

static void
dmacascade()
{
	int dmaporta, dmaportb, s;

	s = sphi();
	if (HAI_AHADMA == 0) {
		dmaporta = 0x0b;
		dmaportb = 0x0a;
	} else {
		dmaporta = 0xd6;
		dmaportb = 0xd4;
	}
	outb(dmaporta, 0xc0 | (HAI_AHADMA & 3));
	outb(dmaportb, (HAI_AHADMA & 3));
	spl(s);
}   /* dmacascade() */

/***********************************************************************
 *  checkmail()
 *
 *  Check the incoming mailboxes for messages.  Do this on any Mail
 *  Box In Full interrupt and before you fail a command for timeout.
 *  The code to determine which target device finished by tid is a
 *  bit tricky and relies on the following assumptions:
 *
 *	  1)  The host adapter is installed in a Intel (big-endian)
 *		  machine. Believe it or not there is (are) non-Intel CPU
 *		  ISA bus machines. and the one that I know of is a M68000
 *		  machine where this would not work.
 *
 *	  2)  The kernel's data space is physically contiguous and is
 *		  never swapped out.
 */

static int
checkmail()
{
	static int startid = 0;
	int msgs = 0;
	int sts;
	int s;
	register int id;
	register srb_p r;
	register int i = startid;

	do {
		if (mb. i[i]. sts != MBIFREE) {
			s = sphi();
			sts = mb. i[i]. sts;
			flip(mb. i[i]. ccbaddr);
			id = (unsigned) ((mb. i[i]. ccbaddr & 0x00ffffffL) - ccbbase) / sizeof(haccb_t);
			if (actv[id]) {
				switch (sts) {
				case MBIABRTFLD:
					devmsg(actv[id]->dev,
"Command 0x%x abort failed\n", actv[id]->cdb. g0. opcode);
					actv[id]->status = ST_ABRTFAIL;
					break;
				case MBISUCCESS:
				case MBIERROR:
					actv[id]->status = ccb[id]. trgtsts;
					break;
				case MBIABORTED:
					devmsg(actv[id]->dev,
"Command 0x%x aborted successfully\n", actv[id]->cdb. g0. opcode);
					actv[id]->status = ST_DRVABRT;
					break;
				default:
printf("Host Adapter Mailbox In value corrupted\n");
					break;
				}

				r = actv[id];
				actv[id] = NULL;
				if (r->cleanup)
					(*(r->cleanup))(r);
				msgs |= bit(id);
			}
			mb. i[i]. ccbaddr = 0;
			spl(s);
		}
		else if (msgs)
			break;
		i = (i + 1) & 7;
	} while (i != startid);
	startid = i;

	return msgs;
}   /* checkmail() */

/***********************************************************************
 *  Driver Interface Functions
 */

/***********************************************************************
 *  hatimer()
 *
 *  Host adapter Timeout handler.
 */

void
hatimer()
{
	register int	id;
	register srb_p  r;
	register int	active;
	int			 s;

	s = sphi();
	checkmail();		/* Cleanup any missed interrupts, etc. */
	active = 0;
	for (id = 0; id < MAXDEVS; ++id) {
		if (actv[id] != NULL && actv[id]->timeout != 0) {
			if (--actv[id]->timeout == 0) {
				devmsg(actv[id]->dev,
				  "<hai154x: timeout - id: %d lun: %d scmd: (0x%x)>",
				  actv[id]->target,
				  actv[id]->lun,
				  actv[id]->cdb. g0. opcode);
				abortscsi(actv[id]);
				if (actv[id]) {
					printf("<hai154x:No cleanup>\n");
					actv[id]->status = ST_DRVABRT;
					r = actv[id];
					actv[id] = NULL;
					if (r->cleanup)
						(*(r->cleanup))(r);
				}
			}
			else
				active = 1;
		}
	}
	drvl[SCSIMAJOR]. d_time = active;
	spl(s);
}   /* hatimer() */

/***********************************************************************
 *  haintr()
 *
 *  SCSI interrupt handler for host adapter.
 */

void
haintr()
{
	int 	intrflgs,
		stsreg;

	intrflgs = inb(INTRFLGS);

	/***************************************************************
         *  If the host adapter command complete flag is set get the
         *  status register (for invalid command flag [INVDCMD]).
         */

	if (intrflgs & HACC) {
		stsreg = inb(STSREG);
#if 0
		/*******************************************************
                 *  If the state here is "just tried to start a SCSI
                 *  command" then that command failed (invalid command)
                 *  mark this condition and bail.
                 */
#endif		
	}

	/***************************************************************
         *  If this is a real interrupt do an Interrupt reset to clear
         *  it.
         */

	if (intrflgs & ANYINTR)
		outb(CTRLREG, IRST);

	if (hastate == ST_HAINIT) {

		/*******************************************************
                 *  During initialization the we should be looking
                 *  for HACC in the INTR Register to go high before
                 *  proceeding on many commands. This code will do
                 *  that.
                 */

  		hainit_ccflag = (intrflgs & HACC) != 0;
  		hainit_stsreg = stsreg;
		return;
	}
	else if (intrflgs & MBIF) {

		/*******************************************************
                 *  During normal operation the only interrupt that
                 *  we concern ourselves with is the MBIF (Mailbox
                 *  in full) interrupt. This means that a SCSI command
                 *  has finished.
                 */

		checkmail();
		return;
	}
}   /* haintr() */

/***********************************************************************
 *  hainit()
 *
 *  Initialize the host adapter for operation.
 */

int
hainit()
{
	register int i;

	hastate = ST_HAINIT;
	hapresent = 0;
	setivec(HAI_AHAINTR, haintr);
	printf("Host Adapter Module: AHA-154x v1.1\n");
	i = aha_inquiry();
        switch (i) {
        case '\0':
                /***********************************************
                 *  AHA-1540 with a 16 head BIOS.
                 */
                HAI_SD_HDS = 16;
                printf("AHA-1540 with 16 head BIOS\n");
                break;
                
        case '0':
                HAI_SD_HDS = 64;
                printf("AHA-1540 with 64 head BIOS\n");
                break;
                
        case 'A':
                HAI_SD_HDS = 64;
                printf("AHA-1540B/1542B with 64 head BIOS\n");
                break;
                
        case 'B':
                HAI_SD_HDS = 64;
                printf("AHA-1640 with 64 head BIOS\n");
                break;
                
        case 'C':
                HAI_SD_HDS = 64;
                printf("AHA-1740A/1742A/1744 (standard mode)\n");
                break;
                
        case 'D':
        case 'E':
                printf("AHA-1540C/AHA-1542C ");
                switch (aha_checkbios()) {
                case 64:
                        HAI_SD_HDS = 64;
                        printf("64 head BIOS\n");
                        break;
                case 255:
                        HAI_SD_HDS = 255;
                        HAI_SD_SPT = 63;
                        printf("Extended BIOS Enabled\n");
                        break;
                default:
                        printf("\nCannot determine number of heads.\n");
                        return 0;
                }
                break;
        default:
                /*******************************************************
                 *  Here we should check an override variable before
                 *  we go on. Right now just say we couldn't find the
                 *  host. THIS CODE SHOULD _NOT_ SHIP!!!!
                 */
                printf("hainit() failed: Adaptec AHA-154x not found.\n");
                return 0;
        }
	if (!(hapresent = hardreset())) {
		printf("hainit() failed: Could not initialize AHA-154x at (0x%x)\n", HAI_AHABASE);
		return 0;
	}

	for (i = 0; i < MAXDEVS; ++i) {
		actv[i] = NULL;
		mb. o[i]. ccbaddr = vtop(ccb + i);
		flip(mb. o[i]. ccbaddr);
		mb. o[i]. cmd = MBOFREE;
	}
	ccbbase = vtop(ccb);
	dmacascade();
	hastate = ST_HAIDLE;
	return 1;
}   /* hainit() */

/***********************************************************************
 *  mkdslist()
 *
 *  Make an Adaptec Data Segment list for a Scatter/Gather Request.
 *  Input:	  d - the memory area for the Scatter/Gather List.
 *			  b - the bufaddr structure for the memory block.
 */

static int
mkdslist(d, b)
register dsentry_p  d;
bufaddr_p		   b;
{
	paddr_t			p_start;
	size_t			p_size;
	paddr_t			p_next;
	int			segments = 1;
	unsigned long		start;
	unsigned long		end;
	dsentry_p		l = d;

	switch (b->space) {
	case KRNL_ADDR:	 /* Kernal Address */
	case USER_ADDR:	 /* User Address (Anything goes) */
		start = (unsigned long) b->addr. caddr;
		p_start = vtop(start);
		break;
	case SYSGLBL_ADDR:  /* System Global address (yeah) */
		start = b->addr. paddr;
		p_start = P2P(start);
		break;
	case PHYS_ADDR:	 /* Physical Address - (who knows) */
	default:
		return 0;
	}
	end = start + b->size;
	p_size  = min(NBPC - (p_start & (NBPC - 1)), end - start);

	for ( ; ; ) {
		d->size[2] = ((unsigned char *) &p_size)[0];
		d->size[1] = ((unsigned char *) &p_size)[1];
		d->size[0] = ((unsigned char *) &p_size)[2];
		d->addr[2] = ((unsigned char *) &p_start)[0];
		d->addr[1] = ((unsigned char *) &p_start)[1];
		d->addr[0] = ((unsigned char *) &p_start)[2];
		if (start + p_size >= end)
			return segments;

		p_next = (b->space == SYSGLBL_ADDR) ? P2P(start + p_size) :
						      vtop(start + p_size);
		if (p_next == p_start + p_size)
			/* Continue Last Segment */
			p_size += min(NBPC, end - start - p_size);
		else {
			/* Start New Segment */
			p_start = p_next;
			start += p_size;
			p_size = min(NBPC, end - start);
			++d;
			if (++segments > (sizeof(dslist_t) / sizeof(dsentry_t)))
				return 0;
		}
	}
}   /* mkdslist() */

/***********************************************************************
 *  startscsi()
 *
 *  Send a SCSI CDB to a target device on the bus.
 */

int
startscsi(r)
register srb_p   r;
{
	register haccb_p	c;
	paddr_t			 bufaddr;
	size_t			  datalen;
	register int		s;

	if (r->target >= MAXDEVS || r->lun >= MAXUNITS) {
		printf("Illegal device ID: %d LUN: %d", r->target, r->lun);
		return 0;
	}

	++r->tries;
	if (actv[r->target]) {
		devmsg(r->dev,
			   "Device busy: old opcode (0x%x) new opcode (0x%x)",
			   ccb[r->target]. cdb. g0. opcode,
			   r->cdb. g0. opcode);
		return 0;
	}

	s = sphi();
	r->status = ST_PENDING;
	c = ccb + r->target;
	memset(c, 0, sizeof(haccb_t));
	c->opcode = 0;			  /* Start SCSI CDB */
	c->addrctrl = (r->target << 5) | (r->lun & 7);
	if (r->xferdir & DMAREAD)  c->addrctrl |= 0x08;
	if (r->xferdir & DMAWRITE) c->addrctrl |= 0x10;
	c->cdblen = cpycdb(&(c->cdb), &(r->cdb));
	c->senselen = 1;

/***********************************************************************
 *  Set up the CCB's Address here. This turned out to be a bit more
 *  complicated than I thought it would be.
 */

	if (r->buf. space == PHYS_ADDR) {
		c->datalen[0] = ((unsigned char *) &(r->buf. size))[2];
		c->datalen[1] = ((unsigned char *) &(r->buf. size))[1];
		c->datalen[2] = ((unsigned char *) &(r->buf. size))[0];
		c->bufaddr[0] = ((unsigned char *) &(r->buf. addr. paddr))[2];
		c->bufaddr[1] = ((unsigned char *) &(r->buf. addr. paddr))[1];
		c->bufaddr[2] = ((unsigned char *) &(r->buf. addr. paddr))[0];
	}
	else {
		datalen = mkdslist(ds[r->target]. entries, &(r->buf));
		if (datalen == 0) {
			printf("SCSI ID %d - Bad Scatter/Gather list\n", r->target);
			spl(s);
			return 0;
		}
		else if (datalen == 1)
			memcpy(c->datalen, ds[r->target]. entries, 6);
		else {
			c->opcode = 2;
			bufaddr = vtop(ds[r->target]. entries);
			datalen *= 6;
			c->datalen[0] = ((unsigned char *) &datalen)[2];
			c->datalen[1] = ((unsigned char *) &datalen)[1];
			c->datalen[2] = ((unsigned char *) &datalen)[0];
			c->bufaddr[0] = ((unsigned char *) &bufaddr)[2];
			c->bufaddr[1] = ((unsigned char *) &bufaddr)[1];
			c->bufaddr[2] = ((unsigned char *) &bufaddr)[0];
		}
	}

	mb. o[r->target]. cmd = MBOSTART;

	/***************************************************************
         *  CLEAN THIS UP.
         *  
         *  The startscsi command will NOT generate an hacc interrupt
         *  if the command bytes and ccb are okay, It will just go
         *  on and do its thing. If however there is a problem startscsi
         *  will generate a HACC interrupt with invalid command set
         *  high. WE SHOULD TRAP this condition.
         *  
         *  [csh]
         */

	if (puthabyte(STARTSCSI) && (inb(STSREG) & INVDCMD) == 0) {
		actv[r->target] = r;
		if (r->timeout)
			drvl[SCSIMAJOR]. d_time = 1;
	}
	else {
		printf("startscsi() failed: Resetting host adapter.\n");
		mb. o[r->target]. cmd = MBOFREE;
		actv[r->target] = NULL;
		r->status = ST_DRVABRT;
		if (r->cleanup)
			(*r->cleanup)(r);
		hapresent = softreset();
		return 0;
	}

	spl(s);
	return 1;
}   /* startscsi() */

/***********************************************************************
 *  abortscsi()
 *
 *  Abort the SCSI Command at a target device on the bus.
 */

void
abortscsi(r)
register srb_p   r;
{
	int s,
		tries;

	printf("abortscsi(id: %d opcode: (0x%x))\n", r->target, r->cdb. g0. opcode);
	s = sphi();
	r->timeout = 0;
	for (tries = 10; hapresent && tries > 0; --tries) {
		mb. o[r->target]. cmd = MBOABORT;
		if (puthabyte(STARTSCSI) && (inb(STSREG) & INVDCMD) == 0)
			break;

		printf("abortscsi(): AHA-154x Command start failed.\n");
		hapresent = softreset();
	}

	if (tries <= 0)
		printf("abortscsi() failed: Cannot reach host adapter\n");
	else {
		busyWait(checkmail, 100);

		if (r->status == ST_PENDING) {
			printf("abortscsi() failed: id %d\n", r->target);
			resetdevice(r->target);

			actv[r->target] = NULL;
			r->status = ST_DRVABRT;
			if (r->cleanup)
				(*r->cleanup)(r);
		}
	}

	spl(s);
}   /* abortscsi() */

/***********************************************************************
 *  resetdevice()
 *
 *  Reset a SCSI target.
 */

void
resetdevice(id)
register int id;
{
	register haccb_p	c = &(ccb[HAI_HAID]);
	int			tries;

	c->opcode = 0x81;
	c->addrctrl = (id << 5);
	for (tries = 10; tries > 0; --tries) {
		if (!hapresent)
			hapresent = softreset();
		if (hapresent) {
			mb. o[HAI_HAID]. cmd = MBOSTART;
			if (puthabyte(STARTSCSI) && (inb(STSREG) & INVDCMD) == 0)
				break;
		}
	}
}   /* resetdevice() */

/***********************************************************************
 *  haihdgeta()	 --  Get disk driver paramters.
 *  
 *	This function is provided to support the HDGETA/HDSETA I/O Controls
 *  so you don't need the old Adaptec SCSI driver to set up the partition
 *  table on initial setup.  There is a catch-22 with this.  You need
 *  to know the drive's geometry in order to set up the partition table
 *  but cannot get the drive's geometry until you have set up the partition
 *  table.  We solve this by using the drive's size and then dividing
 *  down based upon SDS_HDS heads and SDS_SPT sectors per track.  Every
 *  host adapter will want to do this differently.  The Adaptec solution
 *  is the best that I've seen so far. (It allows you to use Huge (1.0
 *  GB) disks under DOS without trouble.
 */

void
haihdgeta(target, hdp, diskcap)
int		target;
hdparm_t	*hdp;
unsigned int	diskcap;
{
	unsigned short	ncyl = (unsigned short) (diskcap / (HAI_SD_HDS * HAI_SD_SPT));
	unsigned short 	landc = ncyl;
	unsigned short 	rwccp = 0xffff;
	unsigned short 	wpcc = 0xffff;

	memset(hdp, 0, sizeof(hdparm_t));

	*((unsigned short *) hdp->ncyl) = ncyl;
	*((unsigned short *) hdp->rwccp) = rwccp;
	*((unsigned short *) hdp->wpcc) = wpcc;
	*((unsigned short *) hdp->landc) = landc;

	hdp->nhead = HAI_SD_HDS;
	if (hdp->nhead > 8)
		hdp->ctrl |= 8;
	hdp->nspt = HAI_SD_SPT;
}   /* haihdseta() */

/***********************************************************************
 *  haihdseta()	 --  Set disk parameters in accordance with HDSETA
 *					  I/O Control.
 *  
 *  Set the disk paramters accordingly for a request from the fdisk
 *  program.  This call really doesn't do anything on the adaptec or
 *  in the SCSI driver in general because SCSI Disks use Logical Block
 *  addressing rather than Cylinder/Head/Track addressing found with
 *  less intelligent Disk drive types.  What this will do is allow
 *  the fdisk program to work so a user can format his disk and install
 *  Coherent on it (A Good Thing).  This boils down to a fancy way to
 *  patch SDS_HDS and SDS_SPT.
 */

void
haihdseta(target, hdp)
hdparm_t	*hdp;
int		target;
{
	HAI_SD_HDS = hdp->nhead;
	HAI_SD_SPT = hdp->nspt;
}   /* haihdseta() */

/* End of file */
