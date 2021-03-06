/* (-lgl
 *	Coherent 386 release 4.2
 *	Copyright (c) 1982, 1993 by Mark Williams Company.
 *	All rights reserved. May not be copied without permission.
 *	For copying permission and licensing info, write licensing@mwc.com
 -lgl) */

#ifndef	__COMMON__CCOMPAT_H__
#define	__COMMON__CCOMPAT_H__

/*
 * This header defines some handy things that allow us to work with K&R,
 * ANSI, and C++  * compilers in a way that is at least less painful than
 * not working. This file does mandate an ANSI C pre-processing environment.
 *
 * Although ANSI allows us not to define function prototypes, C++ mandates that
 * they exist, and it's a good idea to use them whereever possible.
 *
 * This file also deals with some compiler-specific features that are either
 * in the C or C++ language standards but not always available, and some
 * language extensions that are very widely available, if only because they
 * are part of the C++ standard and have been incorporated into C compilers.
 *
 * This file specifically excludes specifics about target machines and
 * compiler interactions. Such definitions belong in another file.
 */

#include <common/feature.h>


/*
 * There is some complexity on the way __STDC__ is used in practice.  The ANSI
 * committee merely says that if __STDC__ is defined and value 1, then the
 * implementation is ISO-conforming.
 *
 * Unfortunately, much existing code uses #ifdef as the only test, which
 * means that some non-conforming compilers which defined __STDC__ as zero
 * caused problems (the Coherent 3.x compiler is one such; the 4.x compiler
 * uses the alternate convention). The #if test is preferable in programs,
 * because in preprocessor tests, undefined symbols assume value 0, but still
 * many programs use the alternate form.
 *
 * For compilers with an intermediate status, e.g., with an ISO preprocessor,
 * or support for "const" but not prototypes, or prototypes but not "const",
 * we perform individual feature-ectomies below.
 *
 * A general rule for future extensions: use double-underscores before and
 * after for non-parameterized macros, double-underscores to prefix macros
 * that take parameters.  If this file's definitions are to be used by user-
 * level code, create a header that exports the definitions into the user
 * namespace.
 *
 * June '93 - split profile into two sections in anticipation of future growth.
 * We can't assume more than 16-bit arithmetic because some preprocessors
 * are limited.
 */

/* __STD_PROFILE__ bits */
#define	__STRINGIZE_M__	0x0001	/* supports ISO C "stringize" */
#define	__PASTE_M__	0x0002	/* support ISO C token-pasting */
#define	__PROTODECL_M__	0x0004	/* supports prototype declarations */
#define	__PROTODEFN_M__	0x0008	/* supports prototype definitions */
#define	__CONST_M__	0x0010	/* supports "const" construct */
#define	__VOLATILE_M__	0x0020	/* supports "volatile" construct */
#define	__VOIDSTAR_M__	0x0040	/* supports "void *" type */

#define	__STD_PROFILE_MASK__	0x007F

/* __MISC_PROFILE__ bits */
#define	__NOTUSED_M__	0x0001	/* allows "not used" warning suppression */
#define	__REGISTER_M__	0x0002	/* requires "register" declaration */
#define	__LINKID_M__	0x0004	/* requires linkage specifier (e.g. C++) */
#define	__INLINE_M__	0x0008	/* allows inline functions */
#define	__INLINEL_M__	0x0010	/* allows inline functions with loops */
#define	__DOTDOTDOT_M__	0x0020	/* requires (...) rather than () */
#define	__NOMERGESTR_M__ 0x0040	/* cannot merge identical strings */
#define	__ANONUNION_M__	0x0080	/* allows anonymous unions (e.g. C++) */

#define	__MISC_PROFILE_MASK__	0x00FF


/*
 * The Standard C language features in one definition for simplicity. This may
 * not be all the bits in __STD_PROFILE_MASK__ once C++ standards start
 * hitting the streets. Normative addenda to ISO C will complicate this, too.
 */

#define	__STDC_M__	(__STD_PROFILE_MASK__)


/*
 * Below, we attempt to determine a configuration suitable for the translator
 * that is working on the current program. Each group of macros attempts to
 * set a preprocessor macro __PROFILE__ with a bit-mask of the features
 * supported by the current translator.
 *
 * This approach has been taken since it considerably simplifies both the
 * task of adding new features to test for and adding new translators. Many
 * other programs intermingle the tasks of determining the translator and
 * defining the responses to that determination; in general, such programs
 * fail to be maintainable when the matrix of features and translators grows
 * larger than about 3x3.
 *
 * June '93 - With the split into two parts, we still let people use the single
 * __PROFILE__ definition if their translator's preprocessor can deal with
 * 32-bit arithmetic.
 */

#if  	defined (__PROFILE__)				/* user-overridden */

# if	(1 << 31) == 0
#  error Your preprocessor cannot handle 32-bit arithmetic!
# endif
# if	__PROFILE__ & ~ (__STD_PROFILE_MASK__ | (__MISC_PROFILE_MASK << 16))
#  error __PROFILE__ contains unknown flag bits.
# endif

# define  __STD_PROFILE__	(__PROFILE__ & __STD_PROFILE_MASK__)
# define  __MISC_PROFILE__	((__PROFILE__ >> 16) & __MISC_PROFILE_MASK__)

#elif	defined (__STD_PROFILE__) || defined (__MISC_PROFILE__)

# ifndef __STD_PROFILE__
#  error If you defined __MISC_PROFILE__, you must also define __STD_PROFILE__
# elif   __STD_PROFILE__ & ~ __STD_PROFILE_MASK__
#  error Unknown bits set in __STD_PROFILE__
# endif

# ifndef __MISC_PROFILE__
#  error If you defined __STD_PROFILE__, you must also define __MISC_PROFILE__
# elif   __MISC_PROFILE__ & ~ __MISC_PROFILE_MASK__
#  error Unknown bits set in __MISC_PROFILE__
# endif

#elif	defined (__cplusplus)				/* C++ */

# ifdef	__GNUC__
#  define __STD_PROFILE__	__STDC_M__
#  define __MISC_PROFILE__	(__NOTUSED_M__ | __LINKID_M__ | \
				 __INLINE_M__ | __INLINEL_M__ | \
				 __DOTDOTDOT_M__ | __ANONUNION_M__)
# else
#  define __STD_PROFILE__	__STDC_M__
#  define __MISC_PROFILE__	(__NOTUSED_M__ | __LINKID_M__ | \
				 __INLINE_M__ | __DOTDOTDOT_M__ | \
				 __ANONUNION_M__)
# endif

#elif	__BORLANDC__					/* Borland C */

# if	__BORLANDC__ >= 0x410
#  define __STD_PROFILE__	__STDC_M__
#  define __MISC_PROFILE__	0	/* features restricted to C++ */
# else
#  define __STD_PROFILE__	__STDC_M__
#  define __MISC_PROFILE__	(__NOTUSED_M__ | __INLINE_M__)
# endif

#elif	defined (__GNUC__)				/* GCC w/o C++ */

# define  __STD_PROFILE__	__STDC_M__
# define  __MISC_PROFILE__	(__INLINE_M__ | __INLINEL_M__ | \
				 __ANONUNION_M__)

#elif	__STDC__ + 0					/* minimal ANSI C */

# define  __STD_PROFILE__	__STDC_M__
# define  __MISC_PROFILE__	0

#elif	__COHERENT__					/* MWC Coherent */

# define  __STD_PROFILE__	(__STRINGIZE_M__ | __PASTE_M__)
# define  __MISC_PROFILE__	(__REGISTER_M__ | __NOMERGESTR_M__)

#else							/* VANILLA */

# define  __STD_PROFILE__	0
# define  __MISC_PROFILE__	__REGISTER_M__

#endif


/*
 * In the following sections we determine the responses to take on the basis
 * of whether the current translator supports each feature.
 *
 * In cases where the feature requires considerable change to source code,
 * such as prototyping and inline functions, we define both an existence
 * feature-test and a value macro.
 *
 * For the case of inline functions, this is because the function should not
 * appear at all in the souce code unless unlining is supported (and because
 * often a macro may suffice in place, although with less safety).
 *
 * In addition, the tests below always check whether a given symbol is defined
 * already, which allow almost any feature to be turned on or off at will from
 * the command-line.  This is useful when testing the characteristics of a new
 * translator, and may often be useful to suppress certain features to aid in
 * debugging.
 */

/*
 * Some old Reiser preprocessors looked for macro-expansions unside quoted
 * strings; this was useful, but dangerous, so ISO C defined a new operator
 * for doing just this job.  A tip of the hat to P.J. Plauger for the form of
 * these macros.
 */

#ifndef	__STRING
# if	__STD_PROFILE__ & __STRINGIZE_M__

#  define __STRING(x)	#x
#  define __STRINGVAL(x) __STRING(x)
#  define _HAVE_STRINGIZE	1

# else

#  define __STRING(x)	"x"
#  define __STRINGVAL(x) __STRING(x)

# endif
#endif


/*
 * As above, this is a feature supported in old systems that ISO C implements
 * in a special operator.  We define __CONCAT3 () and __CONCAT4 () as
 * primitives just for fun; it's handy, and nested calls to __CONCAT () are
 * just too messy to be practical.  Furthermore, many of the users of
 * token-pasting need to be very careful about avoiding extra rescanning when
 * building a composite token, because some of the intermediate steps may
 * match other macros; sometimes this is useful, sometimes not.
 */

#ifndef	__CONCAT
# if	__STD_PROFILE__ & __PASTE_M__

#  define __CONCAT(x,y)		x##y
#  define __CONCAT3(x,y,z)	x##y##z
#  define __CONCAT4(a,b,c,d)	a##b##c##d

# else

#  define __CONCAT(x,y)		x/**/y
#  define __CONCAT3(x,y,z)	x/**/y/**/z
#  define __CONCAT4(a,b,c,d)	a/**/b/**/c/**/d

# endif
#endif


/*
 * __USE_PROTO__ is a general test that can be performed in .c files to see
 * whether to use a prototype form or a K&R form, because the two are so
 * different. Because some tools are hard-wired for K&R source code, they
 * can be confused by macros in the function header; therefore, keeping a
 * real K&R header around can help.
 *
 * __PROTO () is a macro that can be used in header files, because all that
 * differs between K&R and ANSI external declarations is whether the types
 * are present.
 *
 * The difference between the two is important, especially when "lint"-like
 * tools are used.  To check for consistency between the prototype and
 * K&R-style definitions, it may be necessary to enable prototypes in
 * the header files while suppressing them in the C files.
 */

#ifndef	__PROTO
# if 	__STD_PROFILE__ & __PROTODECL_M__

#  define  __PROTO(p)	p

# else	/* prototypes are not supported */

#  define  __PROTO(p)	()

# endif
# if	__STD_PROFILE__ & __PROTODEFN_M__

#  define	__USE_PROTO__	1

# endif
#endif	/* ! defined (__PROTO) */


/*
 * Several existing compilers either do not support the notion of a "const"
 * language element.  For these compilers, we let you suppress the "const"
 * specifier in prototypes, local variables, and structure declarations.  Note
 * that "const" will never appear in a K&R function header.
 */

#ifndef	__CONST__
# if	__STD_PROFILE__ & __CONST_M__

#  define  __CONST__	const

# else	/* const is not supported */

#  define  __CONST__

# endif
#endif	/* ! defined (__CONST__) */


/*
 * For symmetry with the "const" definition, we provide a wrapper for the
 * "volatile" feature.  Note that for some reason, "volatile" is available in
 * some compilers that do not implement "const".  For these compilers, we let
 * you suppress the "volatile" specifier in prototypes, local variables, and
 * structure declarations.
 */

#ifndef	__VOLATILE__
# if	__STD_PROFILE__ & __VOLATILE_M__

#  define  __VOLATILE__	volatile

# else	/* const is not supported */

#  define  __VOLATILE__

# endif
#endif	/* ! defined (__VOLATILE__) */


/*
 * Some compilers support the "void" type, but not the semantics of "void *".
 *
 * The following definition resembles the usage in System V documentation
 * (which probably exists for the same reason), except that we use two
 * underscores in ours before and after, whereas theirs is called "_VOID".
 *
 * A word of explanation:  What we do here is declare an incomplete type;
 * pointers to this incomplete type should function more-or-less as
 * a pointer to "void" would, except that they lack the implicit conversions
 * that are special to "void *", because "void" is an "incomplete type that
 * cannot be completed".  For details, see section 3.5.4 the Standard.
 */

#ifndef	__VOID__
# if	__STD_PROFILE__ & __VOIDSTAR_M__

#  define  __VOID__	void

# else	/* void with a pointer is not supported */

#  if	__COHERENT__ && __MWC__	/* stroke a bug in Coherent 'cc' */

#   define  __VOID__		char

#else

typedef	struct __deep_magic__	__void__;
#   define  __VOID__		__void__

#  endif

# endif
#endif	/* ! defined (__VOID__) */


/*
 * For some of the useful compiler extensions below to be kept available
 * during a "strict" compile (assuming that the feature-tests above enable
 * their use), the convention is to prefix compiler-specific keywords with
 * two underscore characters.
 *
 * This also serves to document which usages are not ISO C.  Note that this
 * may have to change a little for a potential standardized C++.  Also note
 * that as with __NON_POSIX () below, we do not use __CONCAT () but directly
 * use whatever style of token-pasting is available to avoid the effects of
 * macro-rescanning.
 */

#if	__STDC__ + 0
# if	__STD_PROFILE__ & __PASTE_M__
#  define __NON_ISO(name)	__##name
# else
#  define __NON_ISO(name)	__/**/name
# endif
#else
# define   __NON_ISO(k)		k
#endif


/*
 * A feature defined as part of the C++ language that also exists in many
 * C implementations is the ability to suppress "argument not used" warnings
 * in some cases by omitting the name of the variable in the function
 * prototype and merely giving the type.
 *
 * This feature is common in type-checking compilers because checks of
 * function-pointer arguments and other extra checks mean that functions
 * must be declared with unused arguments to match the shape of some function
 * pointer.
 *
 * Of course, it is desirable to leave the original name of the variable in
 * the same place for documentation purposes (often commented out), but this
 * usage chokes some compilers. It seems preferable to use the following
 * definition to state the intention, even in cases where the compiler
 * generates spurious warnings.
 */

#ifndef	__NOTUSED
# if	__MISC_PROFILE__ & __NOTUSED_M__

#  define  __NOTUSED(name)	/* name */

# else	/* does not understand name suppression */

#  define  __NOTUSED(name)	__CONCAT (unused_, name)

# endif
#endif	/* ! defined (__NOTUSED) */


/*
 * Most modern compilers perform their own register allocation and ignore
 * the "register" directive from K&R C.  Such compilers usually have debugging
 * tools that know how to deal with variables that spend at least part of
 * their lifetime in a machine register (or at worst, the option to suppress
 * the auto-register allocation).
 *
 * For compilers that require a "register" declaration for a variable to be
 * placed in a machine register, often it is desirable to suppress the use
 * of registers when debugging.
 *
 * Of course, most uses of "register" are machine-specific.  It is worthwhile
 * to profile a function before you insert some of its variables into
 * registers.
 */

#ifndef	__REGISTER__
# if	__MISC_PROFILE__ & __REGISTER_M__

#  define  __REGISTER__		register

# else

#  define  __REGISTER__

# endif
#endif	/* ! defined (__REGISTER__) */


/*
 * Some compilers for C-like languages such as C++, Objective-C, or even
 * Pascal/Modula-2/Fortran, support cross-language linkage.
 *
 * The standard way to do this within the C family is to use a special
 * form of "extern" that names the language in which a function is implemented.
 * Functions that are implemented in C in a library should be declared as
 * such in the exported header.
 *
 * Currently, this is most important for C++.
 */

#ifndef	__EXTERN_C__
# if	__MISC_PROFILE__ & __LINKID_M__

#  define  __EXTERN_C__		extern "C"
#  define  __EXTERN_C_BEGIN__	__EXTERN_C__ {
#  define  __EXTERN_C_END__	}

# else	/* this is being compiled by a C compiler */

#  define  __EXTERN_C__
#  define  __EXTERN_C_BEGIN__
#  define  __EXTERN_C_END__

# endif
#endif	/* ! defined (__EXTERN_C__) */


/*
 * All C++ compilers and many C compilers support the notion of "inline
 * functions" as an alternative to macros that (i) can be used to wrap up
 * casts so they are only used in safe contexts, (ii) can be used as an
 * alternative to macros that allow arguments with side-effects.
 *
 * This comes in two strengths:  can inline anything; or can inline anything
 * that does not contain a loop.  GNU C has extra strength and can inline tail-
 * recursive inline function, but that facility is not yet sufficiently
 * widespread to be useful.
 *
 * The question is, what should the default setting of the client tests be?
 * #if ! __NO_INLINE__ is a double-negative, so don't use that.  The
 * possibility of defining __INLINE__ as "static", so that inline functions
 * appear in the module separately breakpoint-able from other modules, is a
 * desirable facility (assuming the debug namespace is separate from the
 * linkage namespace, likely in a system sophisticated enough to support
 * inlining).  Furthermore, be aware of any interactions with the __LOCAL__
 * macro defined in <common/xdebug.h>
 */

#ifndef __INLINE__
# if	__MISC_PROFILE__ & __INLINE_M__

#  define __INLINE__		__NON_ISO (inline)
#  define __USE_INLINE__	1

# else

#  define __INLINE__

# endif
#endif

#ifndef __INLINEL__
# if	__MISC_PROFILE__ & __INLINEL_M__

#  define __INLINEL__		__NON_ISO (inline)
#  define __USE_INLINEL__	1

# else

#  define __INLINEL__

# endif
#endif


/*
 * One particular incompatibility between ANSI C and C++ code exists in the
 * way in which prototypes that do not specify any types at all are handled.
 * Under C++, the () form is used to imply (void), because such declarations
 * are extremely common and because early versions of the C++ translators did
 * not allow any declarations in the argument lists of constructors or
 * destructors, not even void, so this form was used to syntactically imply
 * (void).
 *
 * The ANSI C committee declared that a prototype of the form
 *	extern char * malloc ();
 * said nothing whatsoever about the types of its arguments, because such
 * declarations were extremely common in K&R C code, and doing anything else
 * would gratuitously require considerable rewriting.
 *
 * The ANSI C committee decided that the special form "..." to introduce
 * variable arguments was not valid unless preceeded by a regular argument
 * type declaration.  Therefore, there is no way of to be unambiguous that
 * will compile under both transators.
 *
 *		ANSI		C++
 * ()		(...)		(void)		ambiguous
 * (void)	(void)		(void)		unambiguous
 * (...)	error		(...)
 *
 * So, use the preprocessor symbol __ANY_ARGS__ in this context to expand to
 * whatever the current translator needs to see for it to make no assumptions
 * about the number and type of any function arguments.
 */

#ifndef	__ANY_ARGS__
# if	__MISC_PROFILE__ & __DOTDOTDOT__

#  define  __ANY_ARGS__		...

# else

#  define  __ANY_ARGS__

# endif
#endif	/* ! defined (__ANY_ARGS__) */


/*
 * String constants are associated with problems in C, largely because K&R C
 * always treated strings as unique, writeable data.  The introduction of the
 * 'const' qualifier should have made things simpler, as it allows a compiler
 * to discover whether a string is being used in a 'const' or non-'const'
 * way.  However, the precedent of just making all strings that are not being
 * used as character array initializers into shareable constants has already
 * been established.
 *
 * This has an effect on complex macros, because a string in a macro may
 * cause a host of anonymous, identical string constants to be created, which
 * have to be suppressed by using a static declaration.  On the other hand,
 * this does worse than a compiler that does share strings, because if the
 * macro is not used, the data space is still taken up.
 *
 * Here, we give a hint to those macros that care.
 */

#ifndef	_SHARED_STRINGS
# if	(__MISC_PROFILE__ & __NOMERGESTR_M__) == 0

#  define	_SHARED_STRINGS		1

# endif
#endif	/* ! defined (_SHARED_STRINGS) */


/*
 * The use of anonymous unions in C++ greatly simplifies the problems caused
 * by attempts to forward structure tags into unions with use of #define.  This
 * use of unions is the primary reason why names that would normally be
 * properly scope-controlled in C are given over to the preprocessor to wreak
 * havok globally.
 *
 * Below, we expand __ANON__ into empty if such items need no tag, and
 * leave it undefined otherwise, and make _ANONYMOUS a feature-test.
 */

#ifndef	_ANONYMOUS
# if	(__MISC_PROFILE__ & __ANONUNION_M__) != 0

#  define	__ANONYMOUS__
#  define	_ANONYMOUS	1

# endif
#endif


/*
 * Often, it is convenient to map a pointer to a member of a structure back
 * to a pointer to the parent structure.  Unfortunately, while ISO C mandates
 * that conversions between pointer types will not cause loss of information,
 * there are few things that can portably be done with such a converted
 * pointer. In particular, the easy way of mapping back from a structure
 * member pointer to the containing structure performs address arithmetic
 * on character pointers using offsetof ().  It appears that this puts us in
 * the realm of implementation-defined behavior, except when the
 * offsetof () the member is 0.
 */

#ifndef	__DOWNCAST

# define	__DOWNCAST(str,mem,ptomem) \
			((str *) ((char *) (ptomem) - \
					 offsetof (str, mem)))

#endif


/*
 * This is a minor K&R compatibility issue: certain K&R compilers reject the
 * ISO C idiom of enclosing a macro name in parentheses to suppress macro
 * expansion when this idiom is used in function declarations. To get around
 * this, we can use the ISO preprocessor in a clumsy fashion by providing an
 * identity macro to provide the same overall effect of making the name for
 * which we wish to suppress expansion not be immediately followed by a left
 * parenthesis.  (It will be followed by a parenthesis eventually, but because
 * the proprocessor won't revisit the text it has seen before the expansion of
 * the identity macro we get the behavior we want.)
 */

#define	__ARGS(x)	x


/*
 * The POSIX.1 standard discusses a special namespace issue:  how can standard
 * structures be portably extended, given that the structure tags are in the
 * user namespace.  For structures that have members with regular names and
 * that are likely to be extended, the POSIX.1 standard deals with this by
 * implicitly reserving all names of that form (something which further
 * underscores the restrictions on standard headers not including each other).
 *
 * However, when we wish to extend a structure not covered by the namespace
 * reservation rules, or wish to name a member according to some other usage,
 * we must not define the member such that it might conflict with some macro
 * name that the user is permitted to define. See POSIX.1 B.2.7.2 for
 * discussion of this point.
 *
 * The following definition can be used to wrap the definition of structure-
 * member names such that those names will not conflict with user macros if
 * _POSIX_SOURCE or _POSIX_C_SOURCE is defined.  You can use this form
 * in references to the member name that may be encapsulated in a macro so
 * that there is no loss of functionality or alteration of behavior when
 * POSIX compilation is used.
 *
 * Note that we do not use __CONCAT () here:  the aim of this is to cause no
 * accidental conflicts with user-defined macros, but using __CONCAT () would
 * result in an extra layer of macro processing, thus causing the "name"
 * argument to be expanded anyway.
 */

#if	_POSIX_C_SOURCE
# if	__STD_PROFILE__ & __PASTE_M__
#  define __NON_POSIX(name)	__##name
# else
#  define __NON_POSIX(name)	__/**/name
# endif
#else
# define	__NON_POSIX(name)	name
#endif


/*
 * GCC has a special feature for declaring functions as not ever returning.
 */

#if	__GNUC__ && ! __PEDANTIC__
# define	__NO_RETURN__	volatile void
#else
# define	__NO_RETURN__	void
#endif


/*
 * With some systems, structure alignment is configured as a compiler global
 * via #pragma, whereas GCC deals with alignment on a per-item basic via
 * keyword extensions. The GCC way is preferable (since #pragma is not only
 * non-portable, it can't be conditionally wrapped in a macro) and so we wrap
 * it up in a macro.
 *
 * Note that in GCC, the maximum member alignment determines the size to which
 * the structure is rounded.  COHERENT 'cc' instead pads structures based on the
 * largest default alignment, ignoring the actual item alignment; if a non-
 * default alignment is in effect at the end of the structure (the exact way
 * to do that is not defined, but at the moment that is the alignment in
 * effect at the next token *after* the '}' at the end of the structure), then
 * that alignment is used as the amount to round the structure size to.
 *
 * Furthermore, GCC aligns structures based on the strictest actual alignment
 * of any member.  COHERENT 'cc' aligns structures based on the strictest
 * *default* alignment of any member, and this can only be overridden when the
 * structure is later used.
 */

#if	__GNUC__
# define	__ALIGN(n)	__attribute__ ((packed, aligned (n)))
#else
# define	__ALIGN(n)
#endif


/*
 * A feature of both GCC and many "lint" packages is the ability to specify
 * that a user-defined function should have special type-checking based on a
 * printf () or scanf () format string.
 */

#if	__GNUC__
# define	__PRINTF_LIKE(fmt, first) \
			__attribute__ ((format (printf, fmt, first)))
# define	__SCANF_LIKE(fmt, first) \
			__attribute__ ((format (scanf, fmt, first)))
#else
# define	__PRINTF_LIKE(fmt, first)
# define	__SCANF_LIKE(fmt, first)
#endif


/*
 * Undefine all our internal symbols.
 */

#undef	__STRINGIZE_M__
#undef	__PASTE_M__
#undef	__PROTODECL_M__
#undef	__PROTODEFN_M__
#undef	__CONST_M__
#undef	__VOLATILE_M__
#undef	__VOIDSTAR_M__
#undef	__STD_PROFILE_MASK__

#undef	__NOTUSED_M__
#undef	__REGISTER_M__
#undef	__LINKID_M__
#undef	__INLINE_M__
#undef	__INLINEL_M__
#undef	__DOTDOTDOT_M__
#undef	__NOMERGESTR_M__
#undef	__MISC_PROFILE_MASK__

#undef	__STDC_M__
#undef	__PROFILE__
#undef	__STD_PROFILE__
#undef	__MISC_PROFILE__

#endif	/* ! defined (__COMMON__CCOMPAT_H__) */
