/*
 * table.c
 * Nroff/Troff.
 * Tables.
 */

#include "roff.h"

/*
 * Map escaped character.
 */
char esctab[ASCSIZE] ={
	ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL,	/* 0x00 to 0x07 */
	ENUL, ENUL, EIGN, ENUL, ENUL, ENUL, ENUL, ENUL,	/* 0x08 to 0x0F */
	ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL,	/* 0x10 to 0x17 */
	ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL,	/* 0x18 to 0x1F */
	EUNP, ETLI, ECOM, ENUL, EARG, EHYP, ENOP, EACA,	/* 0x20 to 0x27 */
	ECHR, ENUL, ESTR, ENUL, ENUL, EMIN, ENUL, ENUL,	/* 0x28 to 0x2F */
	EDWS, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL,	/* 0x30 to 0x37 */
	ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL,	/* 0x38 to 0x3F */
	ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL,	/* 0x40 to 0x47 */
	ENUL, ENUL, ENUL, ENUL, EVLF, ENUL, ENUL, ENUL,	/* 0x48 to 0x4F */
	ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL, ENUL,	/* 0x50 to 0x57 */
	EHEX, ENUL, ENUL, ENUL, ENUL, ENUL, EM12, ENUL,	/* 0x58 to 0x5F */
	EGRA, ELDR, EBRA, EINT, EVNF, EESC, EFON, ENUL,	/* 0x60 to 0x67 */
	EHMT, ENUL, ENUL, EMAR, EHLF, ENUL, ENUM, EOVS,	/* 0x68 to 0x6F */
	ESPR, ENUL, EVRM, EPSZ, ETAB, EVRN, EVMT, EWID,	/* 0x70 to 0x77 */
	EXLS, ENUL, EZWD, EBEG, EM06, EEND, ENUL, ENUL	/* 0x78 to 0x7F */
};

/*
 * Map numeric font index 0-9 to font name.
 */
char *mapfont[10] = { "P", "R", "B", "I", "\0", "\0", "\0", "\0", "\0", "\0" };

/*
 * Translation table, initialized in main.c.
 */
char trantab[NWIDTH];

/*
 * For forming registers containing requests.
 */
REQ reqtab[] ={
	'a', 'b', req_ab,
	'a', 'd', req_ad,
	'a', 'f', req_af,
	'a', 'm', req_am,
	'a', 's', req_as,
	'b', 'd', req_bd,
	'b', 'o', req_bo,
	'b', 'p', req_bp,
	'b', 'r', req_br,
	'c', '2', req_c2,
	'c', 'c', req_cc,
	'c', 'e', req_ce,
	'c', 'h', req_ch,
	'c', 'o', req_co,
	'c', 's', req_cs,
	'c', 'u', req_cu,
	'd', 'a', req_da,
	'd', 'c', req_dc,
	'd', 'e', req_de,
	'd', 'i', req_di,
	'd', 's', req_ds,
	'd', 't', req_dt,
	'e', 'c', req_ec,
	'e', 'l', req_el,
	'e', 'm', req_em,
	'e', 'o', req_eo,
	'e', 'v', req_ev,
	'e', 'x', req_ex,
	'f', 'b', req_fb,
	'f', 'c', req_fc,
	'f', 'd', req_fd,
	'f', 'i', req_fi,
	'f', 'l', req_fl,
	'f', 'p', req_fp,
	'f', 't', req_ft,
	'f', 'z', req_fz,
	'h', 'c', req_hc,
	'h', 'w', req_hw,
	'h', 'y', req_hy,
	'i', 'e', req_ie,
	'i', 'f', req_if,
	'i', 'g', req_ig,
	'i', 'n', req_in,
	'i', 't', req_it,
	'l', 'c', req_lc,
	'l', 'f', req_lf,
	'l', 'g', req_lg,
	'l', 'l', req_ll,
	'l', 's', req_ls,
	'l', 't', req_lt,
	'm', 'c', req_mc,
	'm', 'k', req_mk,
	'n', 'a', req_na,
	'n', 'b', req_nb,
	'n', 'e', req_ne,
	'n', 'f', req_nf,
	'n', 'h', req_nh,
	'n', 'm', req_nm,
	'n', 'n', req_nn,
	'n', 'r', req_nr,
	'n', 's', req_ns,
	'n', 'x', req_nx,
	'o', 's', req_os,
	'p', 'c', req_pc,
	'p', 'i', req_pi,
	'p', 'l', req_pl,
	'p', 'm', req_pm,
	'p', 'n', req_pn,
	'p', 'o', req_po,
	'p', 's', req_ps,
	'r', 'b', req_rb,
	'r', 'd', req_rd,
	'r', 'f', req_rf,
	'r', 'm', req_rm,
	'r', 'n', req_rn,
	'r', 'p', req_rp,
	'r', 'r', req_rr,
	'r', 's', req_rs,
	'r', 't', req_rt,
	's', 'o', req_so,
	's', 'p', req_sp,
	's', 's', req_ss,
	's', 'v', req_sv,
	's', 'y', req_sy,
	't', 'a', req_ta,
	't', 'c', req_tc,
	't', 'i', req_ti,
	't', 'l', req_tl,
	't', 'm', req_tm,
	't', 'r', req_tr,
	'u', 'f', req_uf,
	'u', 'l', req_ul,
	'v', 's', req_vs,
	'w', 'h', req_wh,
	'\0', '\0', NULL
};

/*
 * Table for putting out roman numerals.
 */
ROM romtab[10] ={
	0, 0,
	0, 0,
	0, 1,
	0, 2,
	1, 1,
	1, 0,
	0, 5,
	0, 6,
	0, 7,
	2, 1
};

/* end of table.c */
