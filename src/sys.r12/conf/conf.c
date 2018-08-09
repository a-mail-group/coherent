/*
 * The code in this file was automatically generated. Do not hand-modify!
 * Generated at Tue Jul 26 15:34:03 1994
 */

#define _KERNEL		1
#define _DDI_DKI	1

#include <sys/confinfo.h>
#include <sys/types.h>

/* entry points for "at" facility */

DECLARE_DRVL (at)
DECLARE_INTR (at)


/* entry points for "pty" facility */

DECLARE_DRVL (pty)


/* entry points for "rm" facility */

DECLARE_DRVL (rm)


/* entry points for "asy" facility */

DECLARE_DRVL (asy)


/* entry points for "fdc" facility */

DECLARE_DRVL (fdc)
DECLARE_INTR (fdc)


/* entry points for "lp" facility */

DECLARE_DRVL (lp)


/* entry points for "vtkb" facility */

DECLARE_DRVL (vtkb)
DECLARE_INTR (vtkb)


/* entry points for "ct" facility */

DECLARE_DRVL (ct)


/* entry points for "nl" facility */

DECLARE_DRVL (nl)


/* entry points for "tty" facility */



/* entry points for "cohmain" facility */



/* entry points for "console" facility */



/* entry points for "mm" facility */



/* entry points for "fd" facility */



/* entry points for "msg" facility */

DECLARE_INIT (msg)


/* entry points for "shm" facility */

DECLARE_INIT (shm)


/* entry points for "sem" facility */

DECLARE_INIT (sem)


/* entry points for "streams" facility */

DECLARE_INIT (streams)


/* entry points for "em87" facility */



/* entry points for "timeout" facility */

DECLARE_INIT (timeout)


/* entry points for "clock" facility */

DECLARE_INTR (clock_)


init_t inittab [] = {
	INIT (msg),
	INIT (shm),
	INIT (sem),
	INIT (streams),
	INIT (timeout)
};

unsigned int ninit = sizeof (inittab) / sizeof (* inittab);

start_t starttab [1];

unsigned int nstart= 0;

exit_t exittab [1];

unsigned int nexit= 0;

halt_t halttab [1];

unsigned int nhalt= 0;

cdevsw_t cdevsw [1];

unsigned int ncdevsw = 0;

bdevsw_t bdevsw [1];

unsigned int nbdevsw = 0;

modsw_t modsw [1];

unsigned int nmodsw = 0;

__major_t _maxmajor = 1;

__major_t _major [] = {
	NODEV, NODEV
};

__minor_t _minor [] = {
	0, 0
};

intmask_t _masktab [] = {
	0x0UL, 0x2UL, 0x2UL, 0x2UL, 
	0x2UL, 0x4042UL, 0x4042UL, 0x4043UL, 
	0xFFFFFFFFUL
};

BEGIN_THUNK (0, 0x4043UL)
	CALL_INTR (0, 0, clock_)
END_THUNK (0)

BEGIN_THUNK (1, 0x2UL)
	CALL_INTR (1, 0, vtkb)
END_THUNK (1)

BEGIN_THUNK (6, 0x4042UL)
	CALL_INTR (6, 0, fdc)
END_THUNK (6)

BEGIN_THUNK (14, 0x4042UL)
	CALL_INTR (14, 0, at)
END_THUNK (14)

intr_t inttab [] = {
	INTR_THUNK (0, 0, 0x4043UL, clock_),
	INTR_THUNK (1, 0, 0x2UL, vtkb),
	INTR_THUNK (6, 0, 0x4042UL, fdc),
	INTR_THUNK (14, 0, 0x4042UL, at)
};

unsigned int nintr = sizeof (inttab) / sizeof (* inttab);

unsigned long	physical_mapping_map = 0;
DRV drvl [32] = {
	DRVL_ENTRY (nl),
	DRVL_ENTRY (ct),
	DRVL_ENTRY (vtkb),
	DRVL_ENTRY (lp),
	DRVL_ENTRY (fdc),
	DRVL_ENTRY (asy),
	NULL_DRVL (),
	NULL_DRVL (),
	DRVL_ENTRY (rm),
	DRVL_ENTRY (pty),
	NULL_DRVL (),
	DRVL_ENTRY (at),
	NULL_DRVL (),
	NULL_DRVL (),
	NULL_DRVL (),
	NULL_DRVL (),
	NULL_DRVL (),
	NULL_DRVL (),
	NULL_DRVL (),
	NULL_DRVL (),
	NULL_DRVL (),
	NULL_DRVL (),
	NULL_DRVL (),
	NULL_DRVL (),
	NULL_DRVL (),
	NULL_DRVL (),
	NULL_DRVL (),
	NULL_DRVL (),
	NULL_DRVL (),
	NULL_DRVL (),
	NULL_DRVL (),
	NULL_DRVL ()
};

int drvn = 32;

