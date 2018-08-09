#include "sh.h"

/*
 * Table for determining type of character.
 */
char lextab[] ={
	                                          0103,	/* EOF */
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,	/* 0x00 to 0x07 */
	0000, 0101, 0103, 0000, 0000, 0000, 0000, 0000,	/* 0x08 to 0x0F */
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,	/* 0x10 to 0x17 */
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,	/* 0x18 to 0x1F */
	0101, 0010, 0003, 0010, 0017, 0000, 0101, 0001,	/* 0x20 to 0x27 */
	0101, 0101, 0210, 0000, 0000, 0010, 0000, 0000,	/* 0x28 to 0x2F */
	0030, 0030, 0030, 0030, 0030, 0030, 0030, 0030,	/* 0x30 to 0x37 */
	0030, 0030, 0000, 0101, 0101, 0000, 0101, 0210,	/* 0x38 to 0x3F */
	0010, 0040, 0040, 0040, 0040, 0040, 0040, 0040,	/* 0x40 to 0x47 */
	0040, 0040, 0040, 0040, 0040, 0040, 0040, 0040,	/* 0x48 to 0x4F */
	0040, 0040, 0040, 0040, 0040, 0040, 0040, 0040,	/* 0x50 to 0x57 */
	0040, 0040, 0040, 0200, 0007, 0200, 0000, 0040,	/* 0x58 to 0x5F */
	0007, 0040, 0040, 0040, 0040, 0040, 0040, 0040,	/* 0x60 to 0x67 */
	0040, 0040, 0040, 0040, 0040, 0040, 0040, 0040,	/* 0x68 to 0x6F */
	0040, 0040, 0040, 0040, 0040, 0040, 0040, 0040,	/* 0x70 to 0x77 */
	0040, 0040, 0040, 0000, 0101, 0000, 0000, 0000	/* 0x78 to 0x7F */
};

