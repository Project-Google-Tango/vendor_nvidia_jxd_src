#ifndef __HGDEFS_H
#define __HGDEFS_H
/*------------------------------------------------------------------------
 *
 * Hybrid Compatibility Header
 * -----------------------------------------
 *
 * (C) 1994-2004 Hybrid Graphics, Ltd.
 * All Rights Reserved.
 *
 * This file consists of unpublished, proprietary source code of
 * Hybrid Graphics, and is considered Confidential Information for
 * purposes of non-disclosure agreement. Disclosure outside the terms
 * outlined in signed agreement may result in irrepairable harm to
 * Hybrid Graphics and legal action against the party in breach.
 *
 * Description:     Compatibility/Portability Header for C and C++
 *                  applications. The changes list can be found
 *                      from hgChanges.txt
 *
 *                      Definitions that are needed for compiling
 *                      our interfaces are placed into the file
 *                      hgPublicDefs.h.
 *
 *                      Include this file into the source files
 *                      of your project. Do *not* include this
 *                      into the public interface files, as this
 *                      file will not be shipped to customers.
 *
 * Notes:               When trying a new compiler, attempt to compile
 *                  the files hgtest_c.c and hgtest_cpp.cpp to
 *                      ensure that everything is in order.
 *
 * $Id: //hybrid/libs/hg/main/hgDefs.h#21 $
 * $Date: 2007/03/16 $
 * $Author: antti $
 *
 * \todo [wili 021008] should we define HG_OS_STRING etc. to have
 *                     string versions of the OS, CPU and compiler?
 * \todo [wili 021018] find out which GCC versions support which
 *                     __attribute__ tags (right now hard-coded for 3.0+
 *                     only)
 * \todo [wili 030219] How about HG_SUPPORT_PREDICATES (??)
 *----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*
 * Include the public definition header file (OS/CPU/Compiler
 * recognition) if not included before.
 *----------------------------------------------------------------------*/

#if !defined (__HGPUBLICDEFS_H)
#    include "hgpublicdefs.h"
#endif

HG_EXTERN_C const char* hgGetVersion(void);

#if (HG_COMPILER == HG_COMPILER_INTELC)
#    pragma warning (push)
#    pragma warning (disable:171)
#    pragma warning (disable:310)
#endif

#if (HG_COMPILER == HG_COMPILER_INTELC) && defined(HG_SUPPORT_WMMX)
#    include "mmintrin.h"
#endif

#if (HG_COMPILER == HG_COMPILER_MSEC) && (_WIN32_WCE > 0x500)
#   include "cmnintrin.h"
#endif

#if (HG_COMPILER == HG_COMPILER_HCVC) && (HG_CPU == HG_CPU_VC02)
#    include <vc/intrinsics.h>
#endif


/*------------------------------------------------------------------------
 * Compile-time assertions
 * \todo [janne 030219] partial fix provided by using HG_HEADER_UNIQUE_NAME
 * \todo [janne 030124] The unique name macro is buggy. The CT_ASSERTs used in hgDefs
 * can cause a conflict if the including .c file has CT_ASSERTs on the
 * same line as hgDefs. To alleviate the problem, I have appended
 * inclusion level into the string on GCC. Gotta fix this for MSDEV
 * and others too. This is not a complete fix, but makes the conflict
 * more improbable. And yes, this problem did occur in practice. :)
 *----------------------------------------------------------------------*/

#define HG_MAKE_NAME2(line,kludge)  HG_constraint_ ## kludge ## _ ## line
#define HG_MAKE_NAME(line,kludge)   HG_MAKE_NAME2(line,kludge)

#if (HG_COMPILER==HG_COMPILER_GCC || HG_COMPILER==HG_COMPILER_GCC_LEGACY)
#    define HG_UNIQUE_NAME    HG_MAKE_NAME(__LINE__,__INCLUDE_LEVEL__)
#else
#    define HG_UNIQUE_NAME    HG_MAKE_NAME(__LINE__,nada)
#endif

#define HG_HEADER_UNIQUE_NAME(x) HG_MAKE_NAME(__LINE__,x)

/* \todo [sampo 08/Jul/04] (expr) ? 1 : -1  -syntax doesn't work in macros with ChipWrights compiler */
#if (HG_COMPILER == HG_COMPILER_BORLAND)
#    define HG_CT_ASSERT(expr) typedef char HG_UNIQUE_NAME[1]
#    define HG_HEADER_CT_ASSERT(file,expr)  typedef char HG_UNIQUE_NAME[1]
#elif (HG_CPU==HG_CPU_CW4512)
#    define HG_CT_ASSERT(expr)             typedef char HG_UNIQUE_NAME[1]
#    define HG_HEADER_CT_ASSERT(file,expr)   typedef char HG_UNIQUE_NAME[1]
#else
#    if defined(HG_LINT) /* No CT_ASSERT when using Lint */
#        define HG_CT_ASSERT(expr) typedef char HG_UNIQUE_NAME[1]
#    else
#        define HG_CT_ASSERT(expr)   /*lint -save --e(92) --e(506) --e(944) --e(971) --e(756) */ typedef char HG_UNIQUE_NAME[(expr) ? 1 : -1] /*lint -restore */ /*\hideinitializer */
#    endif
/* DEBUG DEBUG BROKEN */
#    define HG_HEADER_CT_ASSERT(file,expr)   /*lint -save --e(944) --e(971) --e(756) */ typedef char HG_HEADER_UNIQUE_NAME(file) [(expr) ? 1 : -1] /*lint -restore */
#endif
/*----------------------------------------------------------------------*
 * HG_FILE_LINE is a macro for getting a string where both the current
 * file name and the line number are concatenated together.
 *----------------------------------------------------------------------*/

#define HG_FILE_LINE_S1(line)   #line
#define HG_FILE_LINE_S2(line)   HG_FILE_LINE_S1(line)
#define HG_FILE_LINE __FILE__ ":" HG_FILE_LINE_S2(__LINE__)

/*----------------------------------------------------------------------*
 * HG_SUPPORT_ASM: Allow use of assembler code in hg-functions? By
 * default we allow assembler only in release builds.
 *----------------------------------------------------------------------*/

#if !defined (HG_SUPPORT_ASM)
#    if !defined (HG_DEBUG) && (HG_CPU != HG_CPU_ARM_THUMB)
#        define HG_SUPPORT_ASM
#    endif
#endif

/*------------------------------------------------------------------------
 * Hacks for non-compliant C++ compilers.
 * HG_STANDARD_COMPLIANT_STD_HEADERS means that the platform has C
 * headers as <cstdio>, <cstdlib>, placed into the std namespace.
 *----------------------------------------------------------------------*/

#if !defined(HG_STANDARD_COMPLIANT_STD_HEADERS)
#    define HG_STANDARD_COMPLIANT_STD_HEADERS
#endif

/* Symbian does not support cassert, cstdio, and the like std headers */
#if (HG_OS == HG_OS_SYMBIAN)
#    undef HG_STANDARD_COMPLIANT_STD_HEADERS
#endif

#if (HG_COMPILER == HG_COMPILER_MSEC)
#    define HG_NO_ASSERT_H
#endif

#if defined(HG_NO_ASSERT_H)
#    undef HG_STANDARD_COMPLIANT_STD_HEADERS
#endif

/*------------------------------------------------------------------------
 * Casting macros. These are defined so that when the C source is
 * compiled with a C++ compiler, we get better type-checking.
 *----------------------------------------------------------------------*/

#if defined (HG_CPLUSPLUS) && (HG_COMPILER != HG_COMPILER_ADS)
#    define HG_CONST_CAST(X,Y) const_cast<X>(Y)
#    define HG_REINTERPRET_CAST(X,Y) reinterpret_cast<X>(Y)
#    define HG_REINTERPRET_CAST_CONST(X,Y) reinterpret_cast<const X>(Y)
#    define HG_STATIC_CAST(X,Y) static_cast<X>(Y)
#else
#    define HG_CONST_CAST(X,Y) (X)(Y)
#    define HG_REINTERPRET_CAST(X,Y) /*@-unqualifiedtrans@*/ (X)((void*)(Y)) /*@=unqualifiedtrans@*/
#    define HG_REINTERPRET_CAST_CONST(X,Y) /*@-unqualifiedtrans@*/ (const X)((const void*)(Y)) /*@=unqualifiedtrans@*/
#    define HG_STATIC_CAST(X,Y) (X)(Y)
#endif

/*----------------------------------------------------------------------*
 * Union of a float and an integer
 * \todo [wili 021025] same for doubles? make bit fields for
 *        mantissa, sign, exponent etc.?
 *----------------------------------------------------------------------*/

typedef union
{
    HGfloat     f;                 /*!< Floating-point part */
    HGuint32    i;                 /*!< Integer part */
} HGfloatInt;

/*----------------------------------------------------------------------*
 * Recognize Visual Studio Processor Pack support
 * \todo [wili 030218] The recognition is now over-conservative because
 * we cannot properly distinguish between MSVC6 sp5 and sp5+ppack
 * builds (!!)
 *----------------------------------------------------------------------*/

#if !defined (HG_MSC_PROCESSOR_PACK)
#    if (HG_COMPILER==HG_COMPILER_MSC)
#        if(_MSC_VER >= 1300)
#         define HG_MSC_PROCESSOR_PACK
#        endif
#    endif
#endif

/*----------------------------------------------------------------------*
 * HG_ATTRIBUTE_ALIGNED Request alignment for variables/structures
 *        (not supported on all compilers - empty macro
 *             defined for them)
 *----------------------------------------------------------------------*/

#if (HG_COMPILER == HG_COMPILER_MSC) && defined(HG_MSC_PROCESSOR_PACK)
#    define HG_ATTRIBUTE_ALIGNED(X) __declspec(align(X))
#elif (HG_COMPILER == HG_COMPILER_INTELC)
#    define HG_ATTRIBUTE_ALIGNED(X) __declspec(align(X))
#elif (HG_COMPILER == HG_COMPILER_GCC)
#    define HG_ATTRIBUTE_ALIGNED(X) __attribute__ ((aligned (X)))
#elif defined(HG_COMPILER_ADS_OR_RVCT)
#    define HG_ATTRIBUTE_ALIGNED(X) __align(X)
#endif

#if !defined (HG_ATTRIBUTE_ALIGNED)
#    define HG_ATTRIBUTE_ALIGNED(X)
#endif

/*----------------------------------------------------------------------*
 * 64-bit integer support. If the compiler supports 64-bit integers in
 * one way or another, the HG_64_BIT_INTS macro is defined and the
 * typedefs HGuint64 and HGint64 are set. Otherwise HGint64 and HGuint64
 * are defined as a two-component structure. Routines for manipulating
 * 64-bit integers can be found from hgInt64.h
 *
 * NB: There's a new #define HG_NATIVE_64_BIT_INTS for indicating that
 * the platform really has 64-bit registers in hardware
 *----------------------------------------------------------------------*/

#if (HG_COMPILER == HG_COMPILER_MSC) || (HG_COMPILER == HG_COMPILER_MSEC)
#    define HG_64_BIT_INTS
    typedef unsigned __int64 HGuint64;
    typedef signed   __int64 HGint64;
#elif (HG_COMPILER == HG_COMPILER_INTELC)
#    define HG_64_BIT_INTS
    typedef unsigned __int64 HGuint64;
    typedef signed   __int64 HGint64;
#elif (HG_COMPILER == HG_COMPILER_GCC) || (HG_COMPILER == HG_COMPILER_GCC_LEGACY)
#    define HG_64_BIT_INTS
    typedef unsigned long long int HGuint64;
    typedef signed   long long int HGint64;
#elif (HG_COMPILER == HG_COMPILER_VPCC)
#    define HG_64_BIT_INTS
    typedef unsigned    long    HGuint64;
    typedef signed      long    HGint64;
#elif (HG_COMPILER == HG_COMPILER_MWERKS) && (HG_CPU != HG_CPU_CW4512)
#    define HG_64_BIT_INTS
#    pragma longlong on
    typedef unsigned long long      HGuint64;
    typedef signed long long    HGint64;
#elif (HG_COMPILER == HG_COMPILER_VISUALAGE)
#    define HG_64_BIT_INTS
#    define HG_NATIVE_64_BIT_INTS
    typedef unsigned long  HGuint64;
    typedef signed long    HGint64;
#elif (HG_COMPILER == HG_COMPILER_MWERKS) && (HG_OS == HG_OS_PS2)
#    define HG_64_BIT_INTS
    typedef unsigned long  HGuint64;
    typedef signed long    HGint64;
#elif (HG_COMPILER == HG_COMPILER_GHS)
#    define HG_64_BIT_INTS
    typedef unsigned long long HGuint64;
    typedef signed long long   HGint64;
#elif (HG_COMPILER == HG_COMPILER_IAR)
#    define HG_64_BIT_INTS
    typedef unsigned long long HGuint64;
    typedef signed long long   HGint64;
#elif (HG_COMPILER == HG_COMPILER_RVCT) && !defined(__thumb)
#    define HG_64_BIT_INTS
    typedef unsigned long long      HGuint64;
    typedef signed long long    HGint64;
#elif (HG_COMPILER == HG_COMPILER_PACC)
#    define HG_64_BIT_INTS
    typedef unsigned __int64    HGuint64;
    typedef signed   __int64    HGint64;
/*#elif (HG_COMPILER == HG_COMPILER_ADS)
#    define HG_64_BIT_INTS
    typedef unsigned long long      HGuint64;
    typedef signed long long    HGint64;
*/
#elif (HG_COMPILER == HG_COMPILER_HCVC)
    /* use emulation -- the compiler's 64-bit int support is so bad */
#elif (HG_COMPILER == HG_COMPILER_TCC)
#    define HG_64_BIT_INTS
    typedef unsigned long long int HGuint64;
    typedef signed   long long int HGint64;
#endif

#if (HG_CPU == HG_CPU_X86_64)
#    define HG_NATIVE_64_BIT_INTS
#endif

/* New-style names for 64-bit types. Get rid of the old ones. */
#if defined (HG_64_BIT_INTS)
    typedef HGint64     hgInt64;
    typedef HGuint64    hgUint64;
#endif

/*----------------------------------------------------------------------*
 * If the platform stores the high and low components of an int64
 * in the reverse order, define HG_INT64_BIG_ENDIAN
 *----------------------------------------------------------------------*/

#if !defined (HG_INT64_BIG_ENDIAN)
#    if (HG_CPU == HG_CPU_PPC)
#       define HG_INT64_BIG_ENDIAN
#    endif
#endif

/*----------------------------------------------------------------------*
 * For compilers that don't support 64-bit integers, define
 * structures that can be used for representing the 64-bit ints.
 * Endianess doesn't matter here.
 *----------------------------------------------------------------------*/

#if !defined (HG_64_BIT_INTS)

/*----------------------------------------------------------------------*
 * This is a hack on RVCT/Thumb mode. We need to use anonymous unions
 * to guarantee that HGint64 has exactly the same alignment for Thumb
 * and ARM code (when compiling for ARM we use long long)
 *----------------------------------------------------------------------*/

#if (HG_COMPILER == HG_COMPILER_RVCT)
#       pragma anon_unions
    typedef union
    {
        signed long long _xxx;
        struct
        {
            HGint32 l;
            HGint32 h;
        };
    } HGint64;


    typedef union
    {
        unsigned long long _xxx;
        struct
        {
            HGint32 l;
            HGint32 h;
        };
    } HGuint64;
#else
    typedef struct
    {
        HGint32 l;          /*!< Low part       */
        HGint32 h;          /*!< High part      */
    } HGint64;

    typedef struct
    {
        HGuint32 l;         /*!< Low part       */
        HGuint32 h;         /*!< High part      */
    } HGuint64;
#endif
    typedef HGint64     hgInt64;
    typedef HGuint64    hgUint64;

#else /* HG_64_BIT_INTS defined */

/*----------------------------------------------------------------------*
 * On some compilers that support 64-bit code (such as GCC) on 32-bit
 * platforms, the generated code is not very good. We define a helper
 * union structure so that our own 64-bit routines can easily perform
 * the necessary operations manually. We still want to use the
 * "native" 64-bit data type for passing the HGint64s around.
 *----------------------------------------------------------------------*/

#if !defined(HG_INT64_BIG_ENDIAN)
    typedef union
    {
        struct
        {
            HGuint32 l,h;
        } c;
        HGuint64 r;
    } HGuint64_s;

    typedef union
    {
        struct
        {
            HGint32 l,h;
        } c;
        HGint64 r;
    } HGint64_s;
#else
    typedef union
    {
        struct
        {
            HGuint32 h, l;
        } c;
        HGuint64 r;
    } HGuint64_s;

    typedef union
    {
        struct
        {
            HGint32 h, l;
        } c;
        HGint64 r;
    } HGint64_s;

#endif /* HG_INT64_BIT_ENDIAN   */
#endif /* !HG_64_BIT_INTS           */

/*------------------------------------------------------------------------
 * Make sure that HG_PTR_SIZE is defined. The valid values are either
 * 4 or 8 (this corresponds to sizeof(void*))
 *
 * The reason for the existence of HG_PTR_SIZE is that it can be used
 * conveniently in preprocessor #ifs.  Using sizeof(void*) doesn't
 * work with the preprocessor.
 *----------------------------------------------------------------------*/

#if (HG_CPU == HG_CPU_ALPHA) || (HG_CPU == HG_CPU_X86_64)
#    define HG_PTR_SIZE 8
#endif

#if !defined (HG_PTR_SIZE)
#       define HG_PTR_SIZE 4
#endif

#if (HG_PTR_SIZE != 4) && (HG_PTR_SIZE != 8)
#       error Invalid HG_PTR_SIZE
#endif

HG_HEADER_CT_ASSERT(HGDEFS_H, sizeof(void*) == (size_t)HG_PTR_SIZE);

/*------------------------------------------------------------------------
 * Validate sizes of integer data types at compilation-time.
 *----------------------------------------------------------------------*/

HG_HEADER_CT_ASSERT(HGDEFS_H, sizeof(HGint8)        == (size_t)(1u));
HG_HEADER_CT_ASSERT(HGDEFS_H, sizeof(HGuint8)       == (size_t)(1u));
HG_HEADER_CT_ASSERT(HGDEFS_H, sizeof(HGuint16)      == (size_t)(2u));
HG_HEADER_CT_ASSERT(HGDEFS_H, sizeof(HGint16)       == (size_t)(2u));
HG_HEADER_CT_ASSERT(HGDEFS_H, sizeof(HGuint32)      == (size_t)(4u));
HG_HEADER_CT_ASSERT(HGDEFS_H, sizeof(HGint32)       == (size_t)(4u));
HG_HEADER_CT_ASSERT(HGDEFS_H, sizeof(HGfloatInt)    == (size_t)(4u));
HG_HEADER_CT_ASSERT(HGDEFS_H, sizeof(HGuintptr)     >= sizeof(void*));
HG_HEADER_CT_ASSERT(HGDEFS_H, sizeof(HGintptr)      >= sizeof(void*));
HG_HEADER_CT_ASSERT(HGDEFS_H, sizeof(HGint64)       == (size_t)(8u));
HG_HEADER_CT_ASSERT(HGDEFS_H, sizeof(HGuint64)      == (size_t)(8u));

/*----------------------------------------------------------------------*
 * Double-check that 64-bit integers are aligned the same way on
 * all compilers. What we want is that HGint64 is treated as a single
 * 8 byte entity (therefore the sizes of the two structures below
 * should be 16). Note that if there's _no_ way to force this alignment
 * on a particular compiler, the following checks should be #ifdefed.
 *----------------------------------------------------------------------*/

#if (HG_COMPILER == HG_COMPILER_RVCT)
typedef struct
{
    HGint32 a;
    HGint64 b;
} TestHGint64Alignment;

typedef struct
{
    HGint64 a;
    HGint32 b;
} TestHGint64Alignment2;

HG_HEADER_CT_ASSERT (HGDEFS_H, sizeof(TestHGint64Alignment)  == 16);
HG_HEADER_CT_ASSERT (HGDEFS_H, sizeof(TestHGint64Alignment2) == 16);
#endif


/*----------------------------------------------------------------------*
 * Emulated floats or doubles? (set HG_EMULATED_FLOATS and/or
 * HG_EMULATED_DOUBLES)
 *----------------------------------------------------------------------*/

#if (HG_OS == HG_OS_PS2)        /* PS2 has HW floats but no doubles */
#    define HG_EMULATED_DOUBLES
#endif

#if (defined(HG_COMPILER_ADS_OR_RVCT) && defined (__SOFTFP__)) || (HG_CPU == HG_CPU_CW4512)
#    define HG_EMULATED_FLOATS
#    define HG_EMULATED_DOUBLES
#endif

/*------------------------------------------------------------------------
 * Integer and floating point limits
 * \todo [wili 021018] kinda weird that we use INT32_MIN to mean the
 * negative limit and FLOAT_MIN to mean the smallest value.. any ideas??
 *----------------------------------------------------------------------*/

#define HG_UINT32_MAX (0xffffffffu)
#define HG_INT32_MAX  (0x7fffffff)
#define HG_INT32_MIN  (-0x7fffffff-1)
#define HG_FLOAT_MAX  (3.402823466e+38f)
#define HG_FLOAT_MIN  (1.175494351e-38f)
#define HG_DOUBLE_MAX (1.7976931348623158e+308)
#define HG_DOUBLE_MIN (2.2250738585072014e-308)
#define HG_PI         (3.14159265358979323846)

/*------------------------------------------------------------------------
 * Empty statement
 *----------------------------------------------------------------------*/

#define HG_NULL_STATEMENT (HG_STATIC_CAST(void,0))

/*----------------------------------------------------------------------*
 * The NULL macro (CodeWarrior PS2 compiler defines NULL improperly).
 * \todo [021014 jan] for C, this can be defined as ((void*)0)
 * \todo [021014 jan] for both C and C++, this can be defined as (0),
 *                    needs --emacro((910),HG_NULL) for lint
 *----------------------------------------------------------------------*/

#define HG_NULL 0

/*----------------------------------------------------------------------*
 * Unrefencing function parameters in C (in C++ we can just leave
 * the parameters unnamed)
 *----------------------------------------------------------------------*/

#define HG_UNREF(X) /*@-noeffect@*/ /*lint -save --e(920) */ ((void)(X)) /*lint -restore */

/*-------------------------------------------------------------------------
 * Function that returns zero, for try-catch pattern and macros.
 *-----------------------------------------------------------------------*/

HG_INLINE int hgZero (void) { return 0; }

/*----------------------------------------------------------------------*
 * Note: this assertion function does not need to be defined at
 * all. It is here to make HG_ASSERT work better when using Lint.
 *----------------------------------------------------------------------*/
#if defined(HG_LINT) || defined(HG_NO_ASSERT_H)
/*lint -sem(hgAssertFail,r_no)*/    /*the function does not return*/
HG_EXTERN_C void hgAssertFail(const char* expr, const char* file, int line, const char* message);
#endif

/*-------------------------------------------------------------------*//*!
 * \brief       Triggers the debugger
 * \note    This function may not be implemented on all platforms
 *              (i.e., in those cases it is a no-op)
 *//*-------------------------------------------------------------------*/

HG_INLINE hgBool hgBreakpoint (void)
{
#if (HG_COMPILER == HG_COMPILER_MSC)
    __asm int 3
    return HG_TRUE;
#elif (HG_COMPILER == HG_COMPILER_HCVC)
    _bkpt();
    return HG_TRUE;
#elif (HG_COMPILER == HG_COMPILER_GCC) && (HG_CPU == HG_CPU_X86)
    __asm__ __volatile__ ("int $3");
    return HG_TRUE;
/*
#elif (HG_COMPILER == HG_COMPILER_RVCT)
    __breakpoint(0);
    return HG_TRUE;
*/
#else
    /* nada */
    return HG_FALSE;
#endif
}

/*----------------------------------------------------------------------*
 * Assertion macro (HG_ASSERT)
 *----------------------------------------------------------------------*/

#if !defined(HG_ASSERT)
#    if defined (HG_DEBUG)
#        if (HG_COMPILER == HG_COMPILER_GCC) && (HG_CPU == HG_CPU_X86) && (HG_OS == HG_OS_CYGWIN)
#         include <stdio.h>
#         define HG_ASSERT(X) (void)((X) || (printf("Assertion '%s' failed in %s:%d\n", #X, __FILE__, __LINE__), hgBreakpoint()))
#        elif defined (HG_CPLUSPLUS) && defined(HG_STANDARD_COMPLIANT_STD_HEADERS)
#         include <cassert>
#         define HG_ASSERT(X) assert(X)
#        elif !defined(HG_NO_ASSERT_H) && !defined(HG_LINT)
#         include <assert.h>
#         define HG_ASSERT(X) assert(X)
#        elif (defined(HG_LINT) || defined(HG_NO_ASSERT_H))
#         define HG_ASSERT(e) ((e) ? (HG_STATIC_CAST(void,0)) : hgAssertFail(HG_STATIC_CAST(const char*, #e), __FILE__, __LINE__, ""))
#        endif
#    else
#      define HG_ASSERT(X) /*@-noeffect@*/ ((void)(0))
#    endif
#endif /* !HG_ASSERT */

/*----------------------------------------------------------------------*
 * Macro for indicating unreachable code
 * \todo [wili 021025] LINT warning suppression?
 *----------------------------------------------------------------------*/

#if !defined(HG_DEBUG) && (HG_COMPILER == HG_COMPILER_MSC) && !defined (HG_LINT)
#    if (_MSC_VER >= 1200)
#        define HG_UNREACHABLE_CODE __assume(0);
#    endif
#endif

#if !defined (HG_UNREACHABLE_CODE)
#    define HG_UNREACHABLE_CODE /*lint -save --e(527) */ HG_ASSERT(0); /*lint -restore */
#endif

/*----------------------------------------------------------------------*
 * Macro for indicating that a switch-case does not have a default
 * case (suppresses LINT warnings and in some cases may improve the
 * code).
 *----------------------------------------------------------------------*/

#define HG_NO_DEFAULT_CASE  HG_UNREACHABLE_CODE

/*------------------------------------------------------------------------
 * Define HG_MSC_SUPPRESS_WARNINGS on compilers that support
 * MSVC compatible #pragmas.
 *----------------------------------------------------------------------*/

#if (HG_COMPILER == HG_COMPILER_MSC) || (HG_COMPILER == HG_COMPILER_INTELC) || (HG_COMPILER == HG_COMPILER_MSEC)
#    define HG_MSC_SUPPRESS_WARNINGS
#endif

/*----------------------------------------------------------------------*
 * Endianess enumeration (see function hgGetEndianess())
 *----------------------------------------------------------------------*/

typedef enum
{
    HG_ENDIAN_LITTLE = 0,       /*!< Little endian (e.g. Intel x86)     */
    HG_ENDIAN_BIG    = 1        /*!< Big endian                     */
} HGEndianess;

/*-------------------------------------------------------------------*//*!
 * \brief       Determines CPU endianess at run-time
 * \return      HG_ENDIAN_LITTLE if CPU is little-endian, HG_ENDIAN_BIG
 *              if big-endian. Weirdo endianesses are not supported.
 * \note    Proper optimizing compilers will make this function a no-op
 *              (i.e. just a constant value).
 *//*-------------------------------------------------------------------*/

HG_INLINE HGEndianess hgGetEndianess (void)
{
#if (HG_COMPILER == HG_COMPILER_RVCT) /* work-around an RVCT static const scoping bug */
    HGuint32 v = 0x12345678u;
#else
    static const HGuint32 v = 0x12345678u;
#endif
    const HGuint8*        p = HG_REINTERPRET_CAST_CONST(HGuint8*,&v);
    HG_ASSERT (*p == (HGuint8)0x12u || *p == (HGuint8)0x78u);
    return (HGEndianess)((*p == (HGuint8)(0x12)) ? HG_ENDIAN_BIG : HG_ENDIAN_LITTLE);
}

/*----------------------------------------------------------------------*
 * Native shifter modulo (most CPUs are mod-32 but some CPUs have larger
 * shifters and can thus handle >> 32 etc. without special checks).
 * The value here is *conservative*, i.e., MODULO 32 is given if not
 * known for the platform.
 *----------------------------------------------------------------------*/

#if !defined (HG_SHIFTER_MODULO)
#    if (HG_CPU == HG_CPU_ARM) && (HG_COMPILER != HG_COMPILER_INTELC)
#        define HG_SHIFTER_MODULO 256
#    endif
#endif

#if !defined (HG_SHIFTER_MODULO)
#    define HG_SHIFTER_MODULO 32
#endif

/*----------------------------------------------------------------------*
 * HG_NOINLINE Request that function is not inlined (currently
 *             supported by MSC7 and GCC)
 *----------------------------------------------------------------------*/

#if (HG_COMPILER == HG_COMPILER_GCC)
#    define HG_NOINLINE __attribute__ ((noinline))
#elif (HG_COMPILER == HG_COMPILER_MSC)
#    if (_MSC_VER >= 1300) /* MSC7 or higher*/
#        define HG_NOINLINE __declspec(noinline)
#    endif
#endif

#if !defined (HG_NOINLINE)
#    define HG_NOINLINE
#endif

/*----------------------------------------------------------------------*
 * HG_REGCALL Request that as many arguments as possible are passed
 *            in registers regardless of the calling convention.
 *----------------------------------------------------------------------*/

#if (HG_CPU == HG_CPU_X86)
#    if ((HG_COMPILER == HG_COMPILER_MSC) || (HG_COMPILER == HG_COMPILER_INTELC)) && !defined (HG_LINT)
#        define HG_REGCALL __fastcall
#    elif (HG_COMPILER == HG_COMPILER_GCC)
#        define HG_REGCALL __attribute__ ((regparm(3)))
#    endif
#endif

#if !defined(HG_REGCALL)
#    define HG_REGCALL
#endif

/*----------------------------------------------------------------------*
 * HG_REG_RETURN Request that return value (structure) is passed in
 *                   registers. Mainly useful when returning copies of very
 *                   small C structures. Example:
 *                   HG_REG_RETURN MyStruct foobar (int x);
 *----------------------------------------------------------------------*/

#if defined(HG_COMPILER_ADS_OR_RVCT)
#    define HG_REG_RETURN __value_in_regs
#endif

#if (HG_COMPILER == HG_COMPILER_PACC)
#    define HG_REG_RETURN __value_in_regs
#endif

#if !defined(HG_REG_RETURN)
#    define HG_REG_RETURN
#endif

/*----------------------------------------------------------------------*
 * HG_FUNCTION  Name of current function (unmangled). If the compiler
 *              does not support this the string "unknown function" is
 *                  set instead. On most platforms where this is supported,
 *                  the original macro is either __func__ or __FUNCTION__.
 * \todo [wili 030225] THIS DOES NOT WORK ON ALL COMPILERS!!
 *----------------------------------------------------------------------*/


#if !defined (HG_FUNCTION)
#    if defined (__FUNCTION__)
#        define HG_FUNCTION __FUNCTION__
#    elif defined (__func__)
#        define HG_FUNCTION __func__
#    else
#        define HG_FUNCTION "unknown function"
#    endif
#endif

/*----------------------------------------------------------------------*
 * HG_ATTRIBUTE_PURE
 *
 * Function has no side effects (dependent only on input params and
 * global variables). Note that on GCC, pure means that the function
 * _can_ read global memory. However, we'd like to be even more
 * conservative: functions defined HG_PURE should not depend on anything
 * else except its input parameters.
 *----------------------------------------------------------------------*/

#if (HG_COMPILER == HG_COMPILER_GCC)
#    define HG_ATTRIBUTE_PURE  __attribute__ ((pure))
#elif (HG_COMPILER == HG_COMPILER_RVCT)
#    define HG_ATTRIBUTE_PURE
#elif (HG_COMPILER == HG_COMPILER_PACC)
#    define HG_ATTRIBUTE_PURE __pure
#endif

#if !defined (HG_ATTRIBUTE_PURE)
#    define HG_ATTRIBUTE_PURE
#endif

/*----------------------------------------------------------------------*
 * HG_ATTRIBUTE_DEPRECATED Mark variable as deprecated (its usage
 *                         will give warnings on some compilers). Doesn't
 *                         affect code generation in any way.
 *----------------------------------------------------------------------*/

#if (HG_COMPILER == HG_COMPILER_GCC)
#    define HG_ATTRIBUTE_DEPRECATED __attribute__ ((deprecated))
#elif (HG_COMPILER == HG_COMPILER_MSC)
#    if (_MSC_VER >= 1300) /* MSC7 or higher*/
#        define HG_ATTRIBUTE_DEPRECATED __declspec(deprecated)
#    endif
#endif

#if !defined(HG_ATTRIBUTE_DEPRECATED)
#    define HG_ATTRIBUTE_DEPRECATED
#endif

/*----------------------------------------------------------------------*
 * HG_ATTRIBUTE_CONST (function does not read or write to memory)
 * (Almost the same as HG_ATTRIBUTE_PURE, just stricter.)
 *----------------------------------------------------------------------*/

#if (HG_COMPILER == HG_COMPILER_GCC)
#    define HG_ATTRIBUTE_CONST  __attribute__ ((const))
#elif (HG_COMPILER == HG_COMPILER_ADS) || (HG_COMPILER == HG_COMPILER_RVCT)
#    define HG_ATTRIBUTE_CONST
#endif

#if !defined (HG_ATTRIBUTE_CONST)
#    define HG_ATTRIBUTE_CONST
#endif

/*----------------------------------------------------------------------*
 * HG_ATTRIBUTE_MALLOC (function behaves like malloc(), i.e. pointer
 * returned by the function cannot alias with anything)
 *----------------------------------------------------------------------*/

#if (HG_COMPILER == HG_COMPILER_GCC)
#    define HG_ATTRIBUTE_MALLOC  __attribute__ ((malloc))
#endif

#if !defined (HG_ATTRIBUTE_MALLOC)
#    define HG_ATTRIBUTE_MALLOC
#endif

/*----------------------------------------------------------------------*
 * HG_RESTRICT (pointer does not alias with other pointers)
 * \note Example use: static void testRestrict (void* HG_RESTRICT foobar)
 * \todo [wili 021018] Intel C++ has this as well?
 *----------------------------------------------------------------------*/

#if (HG_COMPILER == HG_COMPILER_GCC) && !defined(HG_SPLINT)
#    define HG_RESTRICT __restrict__
#endif

#if (HG_COMPILER == HG_COMPILER_INTELC)
#    define HG_RESTRICT __restrict
#endif
/*
#if (HG_COMPILER == HG_COMPILER_RVCT)
#       define HG_RESTRICT restrict
#endif
*/
#if !defined (HG_RESTRICT)
#    define HG_RESTRICT
#endif

/*----------------------------------------------------------------------*
 * HG_ALLOCA (Stack allocation macro)
 * \todo [wili 031022] IAR does not seem to support alloca of any kind?
 *----------------------------------------------------------------------*/

#if !defined(HG_NO_ALLOCA_H)

#if defined (HG_SPLINT)
#    include <malloc.h>
#    define HG_ALLOCA(X) _alloca(X)
#elif ((HG_COMPILER == HG_COMPILER_MSC) || (HG_COMPILER == HG_COMPILER_INTELC)) && (HG_OS == HG_OS_WIN32)
#    include <malloc.h>
#    define HG_ALLOCA(X) _alloca(X)
#elif (HG_COMPILER == HG_COMPILER_MWERKS)
#    define HG_ALLOCA(X) __alloca(X)
#elif (HG_COMPILER == HG_COMPILER_GCC)
#    define HG_ALLOCA(X) __builtin_alloca(X)
#elif defined(HG_COMPILER_ADS_OR_RVCT)
#    include <alloca.h>
#elif (HG_COMPILER == HG_COMPILER_HCVC)
#    include <alloca.h>
#    define HG_ALLOCA(X) _Alloca(X)
#endif

#if !defined (HG_ALLOCA)
#    define HG_ALLOCA(X) alloca(X)
#endif

#endif

/*----------------------------------------------------------------------*
 * HG_BUILTIN_CONSTANT. Non-zero if compiler can recognize that
 * the input variable is a built-in constant, zero otherwise.
 * \todo [wili 021018] support builtin_prefetch() and builtin_expect()?
 * \todo [wili 030301] should this be named HG_COMPILETIME_CONSTANT or
 *                     something like that??
 *----------------------------------------------------------------------*/

#if ((HG_COMPILER == HG_COMPILER_GCC) || ((HG_COMPILER == HG_COMPILER_RVCT) && defined (__GNUC__))) && !defined(HG_SPLINT)
#    define HG_BUILTIN_CONSTANT(X) __builtin_constant_p(X)
#endif

#if !defined (HG_BUILTIN_CONSTANT)
#    define HG_BUILTIN_CONSTANT(X) /*@-predboolptr*/((X)?0:0)/*@=predboolptr*/       /*ensure that X is evaluated*/
#endif


/*----------------------------------------------------------------------*
 * HG_OFFSET_OF. Returns offset (in bytes) of member of a structure.
 * Note that the return value is unsigned (size_t).
 *----------------------------------------------------------------------*/

#if (HG_COMPILER == HG_COMPILER_RVCT)
#   define HG_OFFSET_OF(TYPE,MEMB) (__INTADDR__(&(((TYPE*)0)->MEMB)))
#endif

#if defined (offsetof) && !defined(HG_OFFSET_OF)
#    define HG_OFFSET_OF(TYPE,MEMB) (size_t)(offsetof(TYPE,MEMB))
#endif

#if !defined(HG_OFFSET_OF)
#    define HG_OFFSET_OF(TYPE,MEMB) ((size_t) &((TYPE*)0)->MEMB)
#endif

/*----------------------------------------------------------------------*
 * HG_LENGTH_OF_ARRAY. Returns number of elements in a (fixed-size)
 * array. The return value is of type size_t.
 *----------------------------------------------------------------------*/

#define HG_LENGTH_OF_ARRAY(X) ((sizeof(X))/sizeof((X)[0]))

/*----------------------------------------------------------------------*
 * Expected cache line size (for alignment in allocators etc.)
 *----------------------------------------------------------------------*/

#if !defined(HG_CACHE_LINE_SIZE)
#    define HG_CACHE_LINE_SIZE 32
#endif

HG_INLINE HGuintptr hgAlignCacheLine (const void* foo)
{
    return (((HGuintptr)(foo)+(HGuintptr)(HG_CACHE_LINE_SIZE-1))&~(HGuintptr)(HG_CACHE_LINE_SIZE-1));
}

/*----------------------------------------------------------------------*
 * For-loop fix macro for non-standard compilers
 * \todo [wili 021023] Do we want this in the header (perhaps we
 *       should have it under a compiler switch??) .. This header may
 *       be included in the _interfaces_ of our applications!
 *----------------------------------------------------------------------*/

#if defined(HG_CPLUSPLUS) && ((HG_COMPILER == HG_COMPILER_MSC) || (HG_COMPILER == HG_COMPILER_INTELC))
#    if (_MSC_VER < 1300) /* 1300 = MSC7 */
#        if !defined (for)
     namespace HGDefs { HG_INLINE bool getFalse (void) { return false; } }
#         define for if(HGDefs::getFalse()); else for
#        endif /* for */
#    endif /* <= MSVC 6 */
#endif  /* HG_COMPILER==MSC */

/*----------------------------------------------------------------------*
 * Debug code shorthand.
 *----------------------------------------------------------------------*/

#if defined(HG_DEBUG)
#    define HG_DEBUG_CODE(x) x
#else
#    define HG_DEBUG_CODE(x)
#endif

/*----------------------------------------------------------------------*
 * Disable a few irritating warnings.
 * \note Enable this in your own private defs, if you really wish to have it.
 *----------------------------------------------------------------------*/

#if defined(HG_MSC_SUPPRESS_WARNINGS)
    /* C4514: unreferenced inline function has been removed */
#    pragma warning (disable : 4514)
#endif

#if (HG_COMPILER == HG_COMPILER_INTELC)
#    pragma warning (pop)
#endif

/*-------------------------------------------------------------------------
 * Sanity checking utilities for verifying that the implemented rvct
 * functions behave as expected.
 *-----------------------------------------------------------------------*/

HG_EXTERN_C_BLOCK_BEGIN

int     hgTestIDiv(int a, int b);
int     hgTestIMod(int a, int b);
void    hgTestRtMemclr(unsigned char* dst, int size);
void    hgTestRtMemclrW(unsigned int* dst, int size);
void    hgTestRtMemcpy(unsigned char* dst, unsigned char* src, int size);
void    hgTestRtMemcpyW(unsigned char* dst, unsigned char* src, int size);
void    hgTestRtMemmove(unsigned char* dst, unsigned char* src, int size);
void    hgTestAeabiMemcpy4(unsigned char* dst, unsigned char* src, int size);
void    hgTestAeabiMemcpy(unsigned char* dst, unsigned char* src, int size);

HG_EXTERN_C_BLOCK_END

/*----------------------------------------------------------------------*/
#endif /* __HGDEFS_H */
