/*
 * Coff Loader header
 *
 * By Charles Fiterman for Mark Williams 3/30/92.
 */

#include <sys/compat.h>
#include <sys/stat.h>
#include <mtype.h>
#include <coff.h>
#include <arcoff.h>
#include <path.h>

#define	VERSION	"4.2.4"
#define NHASH 128		/* Hash table divisor */

/* Compilation option. */
#define	COMEAU	1		/* Special error message handling for Comeau */

#define S_TEXT	0		/* These segments and only these */
#define S_DATA	1		/* Go to all executables */
#define S_BSSD	2
#define S_COMM	3

#define NLSEG	4		/* The output segment count */
#define MAXSEG	20		/* The maxamum segment count */

#define SEGROUND 16
#define KERBASE	 0xFFC00000UL	/* Kernels start here */
#define DATABASE 0x000400000L	/* .data starts here + some */

#define undefined(s) ((C_EXT==(s)->n_sclass)&&!(s)->n_value&&!(s)->n_scnum)
#define common(s)    ((C_EXT==(s)->n_sclass)&& (s)->n_value&&!(s)->n_scnum)

#define w_message if (watch) watch_message

typedef struct mod_t mod_t;
typedef struct sym_t sym_t;
typedef struct ren_t ren_t;
typedef char flag_t;		/* command line flag option */

/*
 * Input modules are inhaled whole, this points to the module,
 * the next module and any pieces not easily reached.
 */
struct mod_t {
	mod_t		*next;	/* the next module in the chain. */
	FILHDR		*f;	/* the file header */
	SCNHDR		*s;	/* the first section header */
	char		*l;	/* trailing long symbols */
	char		*fname;	/* file name */
	char		*mname;	/* module name */
};

/*
 * Symbol table entry.
 */
struct sym_t {
	sym_t		*next;	/* hash chain */
	mod_t		*mod;	/* defining module */
	int	 	symno;	/* output symbol number */
	SYMENT		sym;	/* constructed symbol */
};

/*
 * Rename table entry.
 */
struct ren_t {
	ren_t		*next;	/* next rename entry */
	char		*to;	/* change to string */
	char		*from;	/* change from string */
};

extern flag_t	reloc,	/* Combine input into a new .o not an executable */
		nosym,	/* No symbol table out. */
		watch,	/* Produce a trace */
		nolcl,	/* Discard C local symbols */
		noilcl,	/* Discard all local symbols */
		Gflag,	/* No warn on common/global */
		qflag,	/* No warn on commons of different length */
		Qflag,	/* Absolute silence */
		debflg;	/* Pass through aux symbols */

#if	COMEAU
extern	flag_t	Comeau_flag;	/* special error message handling for Comeau */
#endif

extern int	errCount;
extern int	nundef;
extern int	symno;			/* Output symbol count */
extern mod_t	*head,			/* head of list of modules to load */
		*tail,			/* tail or list of modules */
		*xhead,			/* head of noload modules */
		*xtail;			/* tail of noload modules */
extern ren_t	*rhead;			/* rename chain */

extern char *ofname;			/* output file name */
extern long comnb, comns, comnl;	/* common lengths */

extern sym_t	*symtable[NHASH];	/* hashed symbol table */

extern unsigned short osegs;		/* the number of output segments */
extern unsigned short segMap[MAXSEG];	/* Segment maping */

extern FILHDR fileh;
extern AOUTHDR aouth;
extern SCNHDR  *secth;		/* output segments */
extern char *argv0;		/* main(  , argv[0]) */
extern char *mktemp();
extern void showUndef();
extern char *symName();

EXTERN_C_BEGIN

__VOID__      *	alloc		PROTO ((size_t _size));
NO_RETURN	fatal		PROTO ((CONST char * _format, ...))
				PRINTF_LIKE (1, 2);
void		message		PROTO ((CONST char * _format, ...))
				PRINTF_LIKE (1, 2);
void		watch_message	PROTO ((CONST char * _format, ...))
				PRINTF_LIKE (1, 2);
void		filemsg		PROTO ((CONST char * _fname,
					CONST char * _format, ...))
				PRINTF_LIKE (2, 3);
void		modmsg		PROTO ((CONST char * _fname,
					CONST char * _mname,
					CONST char * _format, ...))
				PRINTF_LIKE (3, 4);
void		mpmsg		PROTO ((CONST mod_t * _mp,
					CONST char * _format, ...))
				PRINTF_LIKE (2, 3);
void		spmsg		PROTO ((CONST sym_t * _sp,
					CONST char * _format, ...))
				PRINTF_LIKE (2, 3);
void		spwarn		PROTO ((CONST sym_t * sp,
					CONST char * _format, ...))
				PRINTF_LIKE (2, 3);
NO_RETURN	corrupt		PROTO ((CONST mod_t * _mp));
NO_RETURN	help		PROTO ((void));
NO_RETURN	usage		PROTO ((void));
unsigned long	strcrc		PROTO ((CONST char * _str));

void		addsym		PROTO ((SYMENT * _s, mod_t * _mp));
int		qopen		PROTO ((CONST char * _fn, int _do_creat));

EXTERN_C_END
