/*
 * Configurable information for the console driver(s).
 */

#define __KERNEL__	 1

#include <sys/kb.h>
#include "conf.h"

int		kb_lang = kb_lang_us;

/* Number of virtual console sessions on monochrome display. */
int		mono_count = MONO_COUNT;	/* Tunable */

/* Number of virtual console sessions on color display. */
int		vga_count = VGA_COUNT;		/* Tunable */

/* Greek keyboard option in vtnkb - 1=enabled, 0=disabled. */
int		VTGREEK = VTGREEK_SPEC;

int		sep_shift = 0;

/* Console beeps on <Ctrl-G>; 1=beeps, 0=no beeps. */
int		con_beep = 1;
