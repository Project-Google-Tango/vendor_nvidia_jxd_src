#ifndef __HGPUBLICDEFS_H
#define __HGPUBLICDEFS_H
/*------------------------------------------------------------------------
 *
 * Compatibility Header
 * --------------------
 *
 * (C) 1994-2004 Hybrid Graphics, Ltd.
 * All Rights Reserved.
 *
 * This file consists of unpublished, proprietary source code of
 * Hybrid Graphics, and is considered Confidential Information for
 * purposes of non-disclosure agreement. Disclosure outside the terms
 * outlined in signed agreement may result in irreparable harm to
 * Hybrid Graphics and legal action against the party in breach.
 *
 * $Id: //hybrid/libs/hg/main/hgPublicDefs.h#19 $
 * $Date: 2007/02/08 $
 * $Author: tero $
 *
 *//**
 * \file
 * \brief   Compatibility/portability Header for C and C++ interfaces.
 * \author  wili@hybrid.fi
 *//*-------------------------------------------------------------------*/

/* Version numbers for conditional compilation based on HG version */
#define HG_VERSION_MAJOR 3
#define HG_VERSION_MINOR 1
#define HG_VERSION_PATCH 1

/*----------------------------------------------------------------------*
 * Operating system/platform enumeration (see value of HG_OS)
 *----------------------------------------------------------------------*/

#define HG_OS_VANILLA           0   /*!< "any OS"               */
#define HG_OS_WIN32             1   /*!< Win32                  */
#define HG_OS_MACOSX            2   /*!< MacOS X                */
#define HG_OS_LINUX             3   /*!< Linux                  */
#define HG_OS_HPUX              4   /*!< HP-UX                  */
#define HG_OS_GAMECUBE          5   /*!< Nintendo GameCube      */
#define HG_OS_PS2               6   /*!< Sony PS2               */
#define HG_OS_XBOX              7   /*!< Microsoft X-Box        */
#define HG_OS_SYMBIAN           8   /*!< Symbian                */
#define HG_OS_WINCE             9   /*!< Windows CE             */
#define HG_OS_ARMULATOR         10  /*!< ADS Armulator          */
#define HG_OS_EVALUATOR7T       11  /*!< ARM Evaluator Board    */
#define HG_OS_INTENT                    12      /*!< Tao Group Intent           */
#define HG_OS_CYGWIN                    13      /*!< Cygwin                                     */
#define HG_OS_RTK                               14      /*!< RTK                                        */
#define HG_OS_PALMOS5                   15      /*!< Palm OS 5.x                        */
#define HG_OS_QNX                           16  /*!< QNX                                        */
#define HG_OS_BREW                              17      /*!< Qualcomm BREW                      */

/*----------------------------------------------------------------------*
 * CPU enumeration (see value of HG_CPU)
 * \todo [021009 jan] alpha is def'd in msc
 * \todo [021018 wili] what processor is an Alpha? HG_CPU_ALPHA??
 *----------------------------------------------------------------------*/

#define HG_CPU_VANILLA              0   /*!< "any CPU"              */
#define HG_CPU_X86              1   /*!< x86 series             */
#define HG_CPU_PPC              2   /*!< PowerPC                */
#define HG_CPU_ARM              3   /*!< ARM                    */
#define HG_CPU_MIPS             4   /*!< MIPS                   */
#define HG_CPU_ARM_THUMB        5   /*!< ARM in Thumb mode      */
#define HG_CPU_SH                               6   /*!< SuperH 3/4/5 (HG_CPU_SH_ARCH for version) */
#define HG_CPU_CW4512                   7   /*!< Chipwrights 4512               */
#define HG_CPU_ALPHA            8   /*!< Alpha                                  */
#define HG_CPU_VC02                             9       /*!< VideoCore02                        */
#define HG_CPU_X86_64                   10      /*!< x86_64 (amd64)             */

/*----------------------------------------------------------------------*
 * Compiler enumeration (see value of HG_COMPILER)
 * \note MSC6 can be recognized by _MSC_VER >= 1200
 * \note MSC7 can be recognized by _MSC_VER >= 1300
 * \note MSC8 can be recognized by _MSC_VER >= 1400
 * \note GCC 3.0 and above: __GNUC__ >= 3
 *----------------------------------------------------------------------*/

#define HG_COMPILER_VANILLA     0   /*!< "any compiler"             */
#define HG_COMPILER_MSC         1   /*!< Microsoft Visual C/C++     */
#define HG_COMPILER_GCC         2   /*!< Gnu C/C++ (3.0 or newer)   */
#define HG_COMPILER_MWERKS      3   /*!< Metroworks CodeWarrior     */
#define HG_COMPILER_INTELC      4   /*!< Intel C/C++                */
#define HG_COMPILER_BORLAND     5   /*!< Borland C/C++              */
#define HG_COMPILER_VISUALAGE   6   /*!< IBM Visual Age             */
#define HG_COMPILER_KCC         7   /*!< Kai C++                    */
#define HG_COMPILER_DIGITALMARS 8   /*!< Digital Mars compiler      */
#define HG_COMPILER_DIAB        9   /*!< DIAB                       */
#define HG_COMPILER_ACC         10  /*!< ACC                        */
#define HG_COMPILER_SGIMIPSPRO  11  /*!< SGI MIPS Pro               */
#define HG_COMPILER_ADS         12  /*!< ARM Development Suite      */
#define HG_COMPILER_GHS         13  /*!< Green Hills Software       */
#define HG_COMPILER_MSEC        14  /*!< Microsoft Embedded Studio  */
#define HG_COMPILER_IAR         15  /*!< IAR compiler               */
#define HG_COMPILER_GCC_LEGACY  16  /*!< GCC versions prior to 3.0  */
#define HG_COMPILER_HITACHI             17      /*!< Hitachi C compiler (SHC)   */
#define HG_COMPILER_RVCT                18      /*!< RealView ARM compiler      */
#define HG_COMPILER_VPCC                19      /*!< Tao Group VPCC compiler    */
#define HG_COMPILER_PACC        20  /*!< Palm OS Protein Compiler   */
#define HG_COMPILER_TI          21  /*!< Texas Instruments TMS compiler */
#define HG_COMPILER_HCVC        22  /*!< MetaWare HighC/++          */
#define HG_COMPILER_TCC         23  /*!< Tiny C Compiler            */

/*------------------------------------------------------------------------
 * If PC-LINT is currently used to scan the sources, define HG_LINT.
 *----------------------------------------------------------------------*/

#if defined (_lint)
#       undef HG_LINT /*lint !e961*/
#       define HG_LINT
#endif

/*------------------------------------------------------------------------
 * If Splint is currently used to scan the sources, define HG_SPLINT.
 * If compiler/cpu/os have not been defined, set them to vanilla.
 *----------------------------------------------------------------------*/

#if defined (S_SPLINT_S)
#       undef  HG_SPLINT
#       define HG_SPLINT
#       if !defined (HG_COMPILER)
#               define HG_COMPILER HG_COMPILER_VANILLA
#       endif
#       if !defined (HG_OS)
#               define HG_OS HG_OS_VANILLA
#       endif
#       if !defined (HG_CPU)
#               define HG_CPU HG_CPU_VANILLA
#       endif
#endif

/*----------------------------------------------------------------------*
 * If HG_VANILLA is defined, the CPU/OS/Compiler are set to the
 * corresponding vanilla entries. This can be used when testing a new
 * compiler for the first time.
 *----------------------------------------------------------------------*/

#if defined (HG_VANILLA)
#   define HG_OS        HG_OS_VANILLA
#   define HG_CPU       HG_CPU_VANILLA
#   define HG_COMPILER  HG_COMPILER_VANILLA
#endif

/*----------------------------------------------------------------------*
 * Attempt to automatically recognize compiler based on pre-defined
 * macros. Note that the compiler can always be defined manually in
 * a makefile by setting the value of HG_COMPILER.
 *----------------------------------------------------------------------*/

#if !defined(HG_COMPILER)
#       if defined (__INTEL_COMPILER) || defined (__ICL)
#       define HG_COMPILER HG_COMPILER_INTELC
#   elif defined (__MWERKS__) || defined (__CW32__)
#       define HG_COMPILER HG_COMPILER_MWERKS
#       elif defined (__ELATE__)        /* vpcc also defines __GNUC__ */
#               define HG_COMPILER HG_COMPILER_VPCC
#   elif defined (__BORLANDC__)
#       define HG_COMPILER HG_COMPILER_BORLAND
#   elif defined (__ARMCC_VERSION)
#               if (__ARMCC_VERSION > 200000)
#                       define HG_COMPILER HG_COMPILER_RVCT
#               else
#               define HG_COMPILER HG_COMPILER_ADS
#               endif
#   elif defined (__SYMBIAN32__) && (__ARMCC_2__) /* Symbian RVCT MakMake fix */
#               define HG_COMPILER HG_COMPILER_RVCT
#               define __arm
#   elif defined (__GNUC__) || defined (__GCC32__)
#       if  (__GNUC__ >= 3)
#           define HG_COMPILER HG_COMPILER_GCC
#       else
#           define HG_COMPILER HG_COMPILER_GCC_LEGACY
#       endif
#   elif defined (__HPUX_SOURCE)
#       define HG_COMPILER HG_COMPILER_ACC
#   elif defined (__IBMVISUALAGE__)
#       define HG_COMPILER HG_COMPILER_VISUALAGE
#   elif defined (__KCC)
#       define HG_COMPILER HG_COMPILER_KCC
#   elif defined (__SGI__)
#       define HG_COMPILER HG_COMPILER_SGIMIPSPRO
#   elif defined (_MSC_VER) || defined (__VC32__) /* must come after INTELC */
#       if defined(_WIN32_WCE)
#           define HG_COMPILER HG_COMPILER_MSEC
#       else
#           define HG_COMPILER HG_COMPILER_MSC
#       endif
#   elif defined (__ghs__)
#       define HG_COMPILER HG_COMPILER_GHS
#   elif defined(__ICCARM__) || defined(__IAR_SYSTEMS_ICC__)
#       define HG_COMPILER HG_COMPILER_IAR
#   elif defined (__HITACHI__)
#               define HG_COMPILER HG_COMPILER_HITACHI
#       elif defined (__PACC_VER)
#               define HG_COMPILER HG_COMPILER_PACC
#   elif defined (__TINYC__)
#       define HG_COMPILER HG_COMPILER_TCC
#   endif
#   if !defined (HG_COMPILER)
#       error Unrecognized compiler (please update your makefile or hgPublicDefs.h)
#   endif
#endif /* !HG_COMPILER */

/*----------------------------------------------------------------------*
 * HG_COMPILER_ADS_OR_RVCT is short-hand for RVCT migration. Basically
 * RVCT supports everything that ADS supports. Thus we can basically
 * do the same kinds of tricks on both compilers.
 *----------------------------------------------------------------------*/

#if (HG_COMPILER == HG_COMPILER_ADS) || (HG_COMPILER == HG_COMPILER_RVCT)
#       define HG_COMPILER_ADS_OR_RVCT
#endif

/*----------------------------------------------------------------------*
 * Attempt to automatically recognize OS from pre-defined macros. Note
 * that the OS can always be specified in the makefile by setting
 * the value of HG_OS.
 *----------------------------------------------------------------------*/

#if !defined(HG_OS)
#   if defined(__SYMBIAN32__)                                           /* Symbian emulator must be detected before Win32 */
#       define HG_OS HG_OS_SYMBIAN
#               if !defined(HG_SYMBIAN_VERSION)                         /* Format MMmmvv, M=Major, m=minor, v=variant. (e.g 9.1 is 090100) */
#                       if defined(EKA2)
#                               define HG_SYMBIAN_VERSION 80110         /* 8.1b */
#                       else
#                               define HG_SYMBIAN_VERSION 60100         /* for Hybrid code everything before 9.1 is handled similarly */
#                       endif
#               endif /* !defined(HG_SYMBIAN_VERSION) */
#   elif defined (_WIN32) || defined (WIN32)
#       if defined(UNDER_CE) || defined(_WIN32_WCE) /* WIN32 can also be specified on WinCE */
#           define HG_OS HG_OS_WINCE
#       else
#           define HG_OS HG_OS_WIN32
#       endif
#   elif defined(UNDER_CE) || defined(_WIN32_WCE)
#       define HG_OS HG_OS_WINCE
#   elif defined (macintosh) || defined(_MAC) || defined(__APPLE__)
#       define HG_OS HG_OS_MACOSX
#   elif defined (__HPUX_SOURCE)
#       define HG_OS HG_OS_HPUX
#   elif defined(linux) || defined(__linux)
#       define HG_OS HG_OS_LINUX
#   elif defined(HG_ARMULATOR)
#       define HG_OS HG_OS_ARMULATOR
#   elif defined(__ELATE__)
#       define HG_OS HG_OS_INTENT
#   elif defined(__CYGWIN__) || defined(__CYGWIN32__)
#       define HG_OS HG_OS_CYGWIN
#   else
#       define HG_OS HG_OS_VANILLA
#   endif
#endif /* !HG_OS */

/*----------------------------------------------------------------------*
 * Attempt to automatically recognize CPU. The CPU can also be defined
 * manually in the makefile by defining the value of HG_CPU.
 *----------------------------------------------------------------------*/

#if !defined(HG_CPU)
#   if defined (_M_IX86) || defined (__WINS__)
#       define HG_CPU HG_CPU_X86
#   elif (HG_COMPILER==HG_COMPILER_GCC) && defined(__amd64)
#       define HG_CPU HG_CPU_X86_64
#   elif defined (_M_PPC)
#       define HG_CPU HG_CPU_PPC
#   elif defined(__alpha__)
#       define HG_CPU HG_CPU_ALPHA
#   elif defined(HG_COMPILER_ADS_OR_RVCT)
#       if defined (__thumb)
#           define HG_CPU HG_CPU_ARM_THUMB
#       elif defined (__arm)
#           define HG_CPU HG_CPU_ARM
#       else
#           error Either __thumb or __arm must be defined!
#       endif
#   elif defined (ARM) || defined(_ARM_) || defined(__MARM__) || defined(__ARM_ARCH_2__) || defined(__ARM_ARCH_3__) || defined(__ARM_ARCH_4T__) || defined(__ARM_ARCH_5T__) || defined(__ARM_ARCH_5TE__) || defined(__arm) || defined (__ARM) || (HG_COMPILER == HG_COMPILER_MSEC) || defined(__ARM4TM__) || defined(__ARM5__) || defined(__IWMMXT__) || defined(__ARM_EABI__)
/* DEBUG DEBUG \todo [janne] how to properly detect the processor under arm w/ Embedded VC */
#       if defined (__thumb)
#           define HG_CPU HG_CPU_ARM_THUMB
#       else
#           define HG_CPU HG_CPU_ARM
#       endif
#               if defined(__IWMMXT__) && !defined (__thumb)
#                       define HG_SUPPORT_WMMX
#               endif
#       elif defined(__POWERPC__)
#               define HG_CPU HG_CPU_PPC
#   elif defined (__SGI__)
#       define HG_CPU HG_CPU_MIPS
#   elif (HG_COMPILER == HG_COMPILER_GCC) && (defined(i386) || defined(__i386))
#       define HG_CPU HG_CPU_X86
/* \note CPU must be manually set to either x86 or ARM for Intent build */
#   elif (HG_COMPILER == HG_COMPILER_VPCC)
#       error HG_CPU must be manually set when building for Intent
#   elif defined (__HITACHI__) || defined (__sh3__)
#               define HG_CPU HG_CPU_SH
#       elif defined (__CW4512__)
#               define HG_CPU HG_CPU_CW4512
#       elif defined (__i386__)
#               define HG_CPU HG_CPU_X86
#       endif
#   if !defined (HG_CPU)
#       error Unrecognized CPU (please update your makefile or hgPublicDefs.h)
#   endif
#endif /* !HG_CPU */

/*------------------------------------------------------------------------
 * Recognize C++.
 *----------------------------------------------------------------------*/

#if defined (__cplusplus)
#   undef HG_CPLUSPLUS
#   define HG_CPLUSPLUS
#endif

/*------------------------------------------------------------------------
 * Extern "C" macros (HG_EXTERN_C, HG_EXTERN_C_BLOCK_BEGIN,
 * HG_EXTERN_C_BLOCK_END)
 *----------------------------------------------------------------------*/

#if defined (HG_CPLUSPLUS)
#   define HG_EXTERN_C extern "C"
#   define HG_EXTERN_C_BLOCK_BEGIN extern "C" {
#   define HG_EXTERN_C_BLOCK_END }
#else
#   define HG_EXTERN_C
#   define HG_EXTERN_C_BLOCK_BEGIN
#   define HG_EXTERN_C_BLOCK_END
#endif

/*----------------------------------------------------------------------*
 * Debug build settings (assertions etc.)
 *
 * Note that HG_DEBUG is *not* turned on by default, as we want
 * our code to compile into "release" build even if _DEBUG or DEBUG
 * are defined.  This is to prevent accidental debug builds when
 * building for target devices.
 *
 * We default to HG_DEBUG only when HG_DEVEL is turned on.
 *
 * If NV_DEBUG == 1, we turn HG_DEBUG on.
 *----------------------------------------------------------------------*/
#if defined(HG_DEVEL)
#       if defined (_DEBUG) || defined (DEBUG)
#               undef HG_DEBUG
#               define HG_DEBUG
#       endif
#endif

#if defined(NV_DEBUG)
#   if (NV_DEBUG == 1)
#       undef HG_DEBUG
#       define HG_DEBUG
#   endif
#endif

/*----------------------------------------------------------------------*
 * Make sure that we have size_t always defined
 *----------------------------------------------------------------------*/

#if !defined (HG_SPLINT)
#       if (HG_COMPILER==HG_COMPILER_MSEC)
        /* size_t is not defined in stddef on MS Embedded Studio 3.0 */
#               include <stdlib.h>              /* not using <cstddef> as it is broken on so many platforms */
#       else
#               undef __need_size_t
#               define __need_size_t            /*optimization for gcc */
#                       include <stddef.h>          /*not using <cstddef> as it is broken on so many platforms */
#               undef __need_size_t             /*lint !e961*/
#       endif
#endif

/*----------------------------------------------------------------------*
 * Basic integer data types with guaranteed sizes. Note that
 * HGuint64 and HGint64 are defined only if HG_64_BIT_INTS is defined.
 * \todo [021011 jan] add -strong(AJXb, HGbool) for lint
 *----------------------------------------------------------------------*/

typedef signed char     HGint8;         /*!< 8-bit signed integer       */
typedef unsigned char   HGuint8;        /*!< 8-bit unsigned integer     */
typedef short           HGint16;        /*!< 16-bit signed integer      */
typedef unsigned short  HGuint16;       /*!< 16-bit unsigned integer    */
typedef int             HGint32;        /*!< 32-bit signed integer      */
typedef unsigned int    HGuint32;       /*!< 32-bit unsigned integer    */
typedef float           HGfloat;        /*!< 32-bit IEEE float          */
typedef double          HGdouble;       /*!< 64-bit IEEE float          */
typedef int             HGbool;         /*!< Boolean data type for C    */

/*!< Unsigned int that can hold a pointer to any type */
/*!< Signed int that can hold a pointer to any type */
#if (HG_COMPILER != HG_COMPILER_MSC)
typedef unsigned long   HGuintptr;
typedef signed long     HGintptr;
#elif defined(_MSC_VER) && (_MSC_VER >= 1300) && (HG_OS != HG_OS_SYMBIAN)
typedef uintptr_t               HGuintptr;
typedef intptr_t                HGintptr;
#else
typedef unsigned long   HGuintptr;
typedef signed long     HGintptr;
#endif

/* \note explicit casting allows stronger type-checking with Lint */
#define HG_FALSE ((HGbool)(0==1))               /*!< False enumeration          */
#define HG_TRUE  ((HGbool)(0==0))               /*!< True enumeration           */

/*-------------------------------------------------------------------------
 * Wrappers for HG types using the new naming conventions.
 *-----------------------------------------------------------------------*/

typedef HGint8                  hgInt8;                 /*!< 8 bit signed integer */
typedef HGuint8                 hgUint8;                /*!< 8 bit unsigned integer */
typedef HGint16                 hgInt16;                /*!< 16 bit signed integer */
typedef HGuint16                hgUint16;               /*!< 16 bit unsigned integer */
typedef HGint32                 hgInt32;                /*!< 32 bit signed integer */
typedef HGuint32                hgUint32;               /*!< 32 bit unsigned integer */
typedef HGfloat                 hgFloat;                /*!< 32 bit IEEE float */
typedef HGdouble                hgDouble;               /*!< 54 bit IEEE float */
typedef HGbool                  hgBool;                 /*!< Boolean data type for C */
typedef HGuintptr               hgUintPtr;
typedef HGintptr                hgIntPtr;

/*------------------------------------------------------------------------
 * DLL import/export macros (WIN32-specific)
 *----------------------------------------------------------------------*/

#if (HG_OS == HG_OS_WIN32)
#   if (HG_COMPILER == HG_COMPILER_MSC) || (HG_COMPILER == HG_COMPILER_INTELC)
#       define HG_DLL_EXPORT __declspec(dllexport)
#       define HG_DLL_IMPORT __declspec(dllimport)
#   endif
#   if (HG_COMPILER == HG_COMPILER_GCC) || (HG_COMPILER == HG_COMPILER_GCC_LEGACY) || (HG_COMPILER == HG_COMPILER_VPCC)
#       define HG_DLL_EXPORT __attribute__ ((dllexport))
#       define HG_DLL_IMPORT __attribute__ ((dllimport))
#   endif
#endif

#if !defined (HG_DLL_EXPORT)
#   define HG_DLL_EXPORT
#endif

#if !defined (HG_DLL_IMPORT)
#   define HG_DLL_IMPORT
#endif


/*----------------------------------------------------------------------*
 * HG_INLINE
 * \note        If your compiler refuses to accept the __inline keyword,
 *                      set your compiler's corresponding keyword (likely to be
 *                      _inline, inline) or define HG_INLINE as 'static'. Note that
 *                      in C++ 'inline' should always be valid (and 'static' is
 *                      not needed).
 *
 * \note        Forceinline is not always turned on in debug builds.  This is
 *                      the case for example with GCC because we want to get more detailed
 *                      logs out of valgrind.
 *----------------------------------------------------------------------*/

#if defined(HG_LINT) || defined (HG_SPLINT)
#       if defined(HG_CPLUSPLUS)
#               define HG_INLINE inline
#       else
#               define HG_INLINE static /*@unused@*/
#       endif
#elif (HG_COMPILER == HG_COMPILER_MSC) || (HG_COMPILER == HG_COMPILER_MSEC)
#       define HG_INLINE __forceinline
#elif (HG_COMPILER == HG_COMPILER_INTELC)
#       define HG_INLINE __forceinline
#elif (HG_COMPILER == HG_COMPILER_GCC)
#       if defined(HG_DEBUG)
#               if defined(HG_CPLUSPLUS)
#                       define HG_INLINE inline
#               else
#                       define HG_INLINE static __inline__
#               endif
#       else
#               if defined(HG_CPLUSPLUS)
#                       define HG_INLINE inline __attribute__ ((always_inline))
#               else
#                       define HG_INLINE static __inline__ __attribute__ ((always_inline))
#               endif
#       endif
#elif (HG_COMPILER == HG_COMPILER_GCC_LEGACY)
#       if defined(HG_CPLUSPLUS)
#               define HG_INLINE inline
#       else
#               define HG_INLINE static inline
#       endif
#elif (HG_COMPILER == HG_COMPILER_RVCT)
#       define HG_INLINE static __forceinline
#elif (HG_COMPILER == HG_COMPILER_IAR)
#       if defined(HG_CPLUSPLUS)
#               define inline
#       else
#               define HG_INLINE _Pragma("inline=forced") static
#       endif
#elif (HG_COMPILER == HG_COMPILER_HITACHI)
#       if defined (HG_CPLUSPLUS)
#               define HG_INLINE inline
#       else
#               define HG_INLINE static
#   endif
#elif (HG_COMPILER == HG_COMPILER_MWERKS)
/*#     pragma always_inline on */
#       if defined (HG_CPLUSPLUS)
#               define HG_INLINE inline
#       else
#               define HG_INLINE static
#   endif
#elif (HG_COMPILER == HG_COMPILER_PACC)
#       define HG_INLINE __inline
#elif (HG_COMPILER == HG_COMPILER_HCVC)
#       if defined(HG_CPLUSPLUS)
#               define HG_INLINE inline
#       else
#               define HG_INLINE static _Inline
#       endif
#else
#       if defined(HG_CPLUSPLUS)
#               define HG_INLINE inline
#       else
#               define HG_INLINE static __inline
#       endif
#endif

/*----------------------------------------------------------------------*
 * HG_STATICF
 * \note        Local function. The purpose of this define is to allow
 *                      making all functions non-static by defining the macro to
 *                      be empty. This way, it is certain that the functions are
 *                      always included in compiler map files.
 *----------------------------------------------------------------------*/

#ifdef HG_NO_STATIC_FUNCTIONS
#       define HG_STATICF
#else
#       define HG_STATICF static
#endif

/*----------------------------------------------------------------------*/
#endif /* __HGPUBLICDEFS_H */
