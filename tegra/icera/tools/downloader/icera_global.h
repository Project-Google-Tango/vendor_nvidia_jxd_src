/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @file icera_global.h Fundamental definitions.
 */

/**
 * @mainpage
 * libcommon.a is the place where all common functions, types, and global variables are
 * defined for general use by the Icera stack, LL1 and scheduler functions.
 */

/**
 * @defgroup dmem_scratch Scratch stack
 */


#ifndef ICERA_GLOBAL_H
/**
 * Macro definition used to avoid recursive inclusion.
 */
#define ICERA_GLOBAL_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/

#include "icera_assert.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

#if defined (_MSC_VER)
/** Support for non-ANSI C use of bool in the code. */
# if !defined (__cplusplus)
typedef unsigned char bool;
#  define false 0
#  define true  1
# endif
#else
#include <stdbool.h>        /* Needed for bool type definition. */
#endif

#include <stddef.h>

/*************************************************************************************************
 * Macros
 ************************************************************************************************/

/**
 * Branch annotations to improve code locality.
 * Requires -freorder-blocks to be defined in GCC_OPTS.
 *
 */
#if defined(__dxp__)
#if !defined(likely)
#if defined(__dxp__)
#if (ICERA_SDK_EV_AT_LEAST(4,1,'a'))
    /* GNAT pending with __builtin_expect on EV4 */
#define likely(x)   (x)
#else
#define likely(x)   __builtin_expect(!!(x),1)
#endif /* ICERA_SDK_EV_AT_LEAST */
#else /* __dxp__ */
  #define likely(x) (x)
#endif /* __dxp__ */
#endif /* !defined(likely) */

#if !defined(unlikely)
#if defined(__dxp__)
#if (ICERA_SDK_EV_AT_LEAST(4,1,'a'))
    /* GNAT pending with __builtin_expect on EV4 */
#define unlikely(x)   (x)
#else
#define unlikely(x)   __builtin_expect(!!(x),0)
#endif /* ICERA_SDK_EV_AT_LEAST */
#else /* __dxp__ */
#define unlikely(x) (x)
#endif /* __dxp__ */
#endif /* !defined(unlikely) */
#else
#define likely(x)   (x)
#define unlikely(x)   (x)
#endif


/**
 * Various tool libraries insist on using asm rather than __asm__. So bodge in a switch to __asm__
 */
#ifdef __dxp__
    #define asm __asm__
#endif

/**
 * @}
 */

/**
 * @defgroup icera_attributes Compiler attributes
 * @{
 */

/**
 * Indicates if a function is forced to be not inlined
 */
#ifdef __dxp__
#define DXP_NOINLINE  __attribute__((noinline))
#define DXP_FORCEINLINE __attribute__((always_inline))
#else
#define DXP_NOINLINE
#define DXP_FORCEINLINE
#endif

/**
 * Indicates to the compiler that a symbol is deliberately unused.
 */
#define NOT_USED(SYMBOL)    (void)(SYMBOL);

/*
 * Defines to specify the section for functions and variables;
 */


#ifdef __dxp__

/* NOTE: From EV4.5a, these values must not be used directly as arguments to mphal_overlay functions.
         Use tOverlayId enumeration instead */
#define DXP_OVERLAY_NONE    0
#define DXP_LTE_OVERLAY_SECTION 1
#define DXP_2G_OVERLAY_SECTION 2
#define DXP_3G_OVERLAY_SECTION 3

/**
 * Attribute definition to place code in internal memory. Use like the following:
 *
 * static short DXP_ICODE someFunction(void)
 * {
 * };
 */
#ifdef ENABLE_DXP1_DMEM_DATA
/* 9xxx case: should be in GMEM */
#define DXP1_LTEIDATA DXP_UNCACHED_GMEM
#else
#define DXP1_LTEIDATA
#endif

#if defined (DEBUG_NO_ONCHIP_CODE)
    #define DXP_COMMONICODE
    #define DXP_LTEICODE
    #define DXP_2GICODE
    #define DXP_3GICODE
    #define DXP0_COMMONICODE
    #define DXP2_COMMONICODE
    #define DXP_COMMONGCODE
    #define DXP_LTEGCODE
    #define DXP1_LTEGCODE
    #define DXP_2GGCODE
    #define DXP_3GGCODE
    #define DXP0_3GICODE
    #define DXP2_3GICODE
    #define DXP0_LTEICODE
    #define DXP2_LTEICODE
#else
/* Common onchip code */
    #define DXP_COMMONGCODE     __attribute__((placement(gmem)))
    #define DXP0_COMMONICODE    __attribute__((placement(imem)))
    #define DXP2_COMMONICODE    __attribute__((placement(imem2)))

    #define DXP_COMMONICODE __attribute__((placement(imem)))
    #define DXP_LTEICODE    __attribute__((placement(imem), overlay(DXP_LTE_OVERLAY_SECTION)))
    #define DXP_2GICODE     __attribute__((placement(imem), overlay(DXP_2G_OVERLAY_SECTION)))
    #define DXP_3GICODE     __attribute__((placement(imem), overlay(DXP_3G_OVERLAY_SECTION)))

    #define DXP0_LTEICODE   __attribute__((placement(imem), overlay(DXP_LTE_OVERLAY_SECTION)))
    #define DXP2_LTEICODE   __attribute__((placement(imem2), overlay(DXP_LTE_OVERLAY_SECTION)))

    /*DXP0&2 switch overlays at the same time so can use common code placement */
    #define DXP_LTEGCODE    __attribute__((placement(gmem), overlay(DXP_LTE_OVERLAY_SECTION), overlay_set(A)))
    #define DXP1_LTEGCODE   __attribute__((placement(gmem), overlay(DXP_LTE_OVERLAY_SECTION), overlay_set(B)))

    #define DXP0_3GICODE    __attribute__((placement(imem), overlay(DXP_3G_OVERLAY_SECTION)))
    #define DXP2_3GICODE    __attribute__((placement(imem2), overlay(DXP_3G_OVERLAY_SECTION)))

    #define DXP_3GGCODE     __attribute__((placement(gmem), overlay(DXP_3G_OVERLAY_SECTION), overlay_set(A)))
    #define DXP1_3GGCODE    __attribute__((placement(gmem), overlay(DXP_3G_OVERLAY_SECTION), overlay_set(B)))
#endif  /* #ifdef DEBUG_NO_ONCHIP_CODE */

/**
 * Attribute definitions to place data in internal memory. Use like the following:
 *
 * static short DXP_COMMONIDATA DXP_A16 dxp_taps[] = {1,2,3,4};
 *
 * NOTE: _NOSR postfix is used to indicate memory which does not
 * need saving and restoring across hibernate cycles.
 */
    #define DXP_COMMONIDATA         __attribute__((placement(dmem), has_dmem_latency))
    #define DXP_COMMONIDATA_NOSR    __attribute__((placement(dmem), norestore, has_dmem_latency))
    #define DXP_LTEIDATA            __attribute__((placement(dmem), overlay(DXP_LTE_OVERLAY_SECTION), has_dmem_latency))
    #define DXP_LTEIDATA_NOSR       __attribute__((placement(dmem), overlay(DXP_LTE_OVERLAY_SECTION), norestore, has_dmem_latency))
    #define DXP_2GIDATA             __attribute__((placement(dmem), overlay(DXP_2G_OVERLAY_SECTION), has_dmem_latency))
    #define DXP_2GIDATA_NOSR        __attribute__((placement(dmem), overlay(DXP_2G_OVERLAY_SECTION), norestore, has_dmem_latency))
    #define DXP_3GIDATA             __attribute__((placement(dmem), overlay(DXP_3G_OVERLAY_SECTION), has_dmem_latency))
    #define DXP_3GIDATA_NOSR        __attribute__((placement(dmem), overlay(DXP_3G_OVERLAY_SECTION), norestore, has_dmem_latency))
    #define DXP_2GIDATA_M0          __attribute__((placement(dmem), multidalign(0),overlay(DXP_2G_OVERLAY_SECTION), has_dmem_latency))
    #define DXP_2GIDATA_M1          __attribute__((placement(dmem), multidalign(1),overlay(DXP_2G_OVERLAY_SECTION), has_dmem_latency))
    #define DXP_3GIDATA_M0          __attribute__((placement(dmem), multidalign(0),overlay(DXP_3G_OVERLAY_SECTION), has_dmem_latency))
    #define DXP_3GIDATA_M1          __attribute__((placement(dmem), multidalign(1),overlay(DXP_3G_OVERLAY_SECTION), has_dmem_latency))
    #define DXP_LTEIDATA_M0         __attribute__((placement(dmem), multidalign(0),overlay(DXP_LTE_OVERLAY_SECTION), has_dmem_latency))
    #define DXP_LTEIDATA_M1         __attribute__((placement(dmem),  multidalign(1), overlay(DXP_LTE_OVERLAY_SECTION),has_dmem_latency))

    #define DXP0_COMMONIDATA        __attribute__((placement(dmem), has_dmem_latency))
    #define DXP0_COMMONIDATA_NOSR   __attribute__((placement(dmem), norestore, has_dmem_latency))
    #define DXP0_LTEIDATA           __attribute__((placement(dmem), overlay(DXP_LTE_OVERLAY_SECTION), has_dmem_latency))
    #define DXP0_LTEIDATA_NOSR      __attribute__((placement(dmem), overlay(DXP_LTE_OVERLAY_SECTION), norestore, has_dmem_latency))
    #define DXP0_LTEIDATA_M0        __attribute__((placement(dmem), multidalign(0),overlay(DXP_LTE_OVERLAY_SECTION), norestore, has_dmem_latency))
    #define DXP0_LTEIDATA_M1        __attribute__((placement(dmem), multidalign(1),overlay(DXP_LTE_OVERLAY_SECTION), norestore, has_dmem_latency))
    #define DXP0_2GIDATA            __attribute__((placement(dmem), overlay(DXP_2G_OVERLAY_SECTION), has_dmem_latency))
    #define DXP0_2GIDATA_NOSR       __attribute__((placement(dmem), overlay(DXP_2G_OVERLAY_SECTION), norestore, has_dmem_latency))
    #define DXP0_2GIDATA_M0         __attribute__((placement(dmem), multidalign(0),overlay(DXP_2G_OVERLAY_SECTION), norestore, has_dmem_latency))
    #define DXP0_2GIDATA_M1         __attribute__((placement(dmem), multidalign(1),overlay(DXP_2G_OVERLAY_SECTION), norestore, has_dmem_latency))
    #define DXP0_3GIDATA            __attribute__((placement(dmem), overlay(DXP_3G_OVERLAY_SECTION), has_dmem_latency))
    #define DXP0_3GIDATA_NOSR       __attribute__((placement(dmem), overlay(DXP_3G_OVERLAY_SECTION), norestore, has_dmem_latency))
    #define DXP0_3GIDATA_M0         __attribute__((placement(dmem), multidalign(0),overlay(DXP_3G_OVERLAY_SECTION), norestore, has_dmem_latency))
    #define DXP0_3GIDATA_M1         __attribute__((placement(dmem), multidalign(1),overlay(DXP_3G_OVERLAY_SECTION), norestore, has_dmem_latency))
    #define DXP2_COMMONIDATA        __attribute__((placement(dmem2), has_dmem_latency))
    #define DXP2_COMMONIDATA_NOSR   __attribute__((placement(dmem2), norestore, has_dmem_latency))
    #define DXP2_3GIDATA            __attribute__((placement(dmem2), overlay(DXP_3G_OVERLAY_SECTION), has_dmem_latency))
    #define DXP2_3GIDATA_NOSR       __attribute__((placement(dmem2), overlay(DXP_3G_OVERLAY_SECTION), norestore, has_dmem_latency))
    #define DXP2_3GIDATA_M0         __attribute__((placement(dmem2), multidalign(0),overlay(DXP_3G_OVERLAY_SECTION), norestore, has_dmem_latency))
    #define DXP2_3GIDATA_M1         __attribute__((placement(dmem2), multidalign(1),overlay(DXP_3G_OVERLAY_SECTION), norestore, has_dmem_latency))      
    #define DXP2_LTEIDATA           __attribute__((placement(dmem2), overlay(DXP_LTE_OVERLAY_SECTION), has_dmem_latency))
    #define DXP2_LTEIDATA_NOSR      __attribute__((placement(dmem2), overlay(DXP_LTE_OVERLAY_SECTION), norestore, has_dmem_latency))
    #define DXP2_LTEIDATA_M0        __attribute__((placement(dmem2), multidalign(0),overlay(DXP_LTE_OVERLAY_SECTION), norestore, has_dmem_latency))
    #define DXP2_LTEIDATA_M1        __attribute__((placement(dmem2), multidalign(1),overlay(DXP_LTE_OVERLAY_SECTION), norestore, has_dmem_latency))
    #define DXPU_GDATA          __attribute__((placement(gmem), uncached, uni))
    #define DXP0_GDATA          __attribute__((placement(gmem), uni(0)))
    #define DXP1_GDATA          __attribute__((placement(gmem), uni(1)))
    #define DXP2_GDATA          __attribute__((placement(gmem), uni(2)))
    #define DXPU_LTEGDATA       __attribute__((placement(gmem), overlay(DXP_LTE_OVERLAY_SECTION), uni, overlay_set(A), uncached))
    #define DXP0_LTEGDATA       __attribute__((placement(gmem), overlay(DXP_LTE_OVERLAY_SECTION), uni(0), overlay_set(A)))
    #define DXP1_LTEGDATA       __attribute__((placement(gmem), overlay(DXP_LTE_OVERLAY_SECTION), uni(1), overlay_set(B)))
    #define DXP2_LTEGDATA       __attribute__((placement(gmem), overlay(DXP_LTE_OVERLAY_SECTION), uni(2), overlay_set(A)))
    #define DXPU_2GGDATA        __attribute__((placement(gmem), overlay(DXP_2G_OVERLAY_SECTION), uni, overlay_set(A), uncached))
    #define DXP0_2GGDATA        __attribute__((placement(gmem), overlay(DXP_2G_OVERLAY_SECTION), uni(0), overlay_set(A)))
    #define DXP1_2GGDATA        __attribute__((placement(gmem), overlay(DXP_2G_OVERLAY_SECTION), uni(1), overlay_set(B)))
    #define DXP2_2GGDATA        __attribute__((placement(gmem), overlay(DXP_2G_OVERLAY_SECTION), uni(2), overlay_set(A)))
    #define DXPU_3GGDATA        __attribute__((placement(gmem), overlay(DXP_3G_OVERLAY_SECTION), uni, overlay_set(A), uncached))
    #define DXP0_3GGDATA        __attribute__((placement(gmem), overlay(DXP_3G_OVERLAY_SECTION), uni(0), overlay_set(A)))
    #define DXP1_3GGDATA        __attribute__((placement(gmem), overlay(DXP_3G_OVERLAY_SECTION), uni(1), overlay_set(B)))
    #define DXP2_3GGDATA        __attribute__((placement(gmem), overlay(DXP_3G_OVERLAY_SECTION), uni(2), overlay_set(A)))


/**
 * Attribute definition given to function parameters to indicate placement to compiler
 */
    #define DXP_IN_DMEM __attribute__((has_dmem_latency))

/**
 * Attribute definition to align storage on a 256-bit / 32-byte boundary.
 *  - this is the length of a DMEM cache line.
 *
 */
    #define DXP_A256 __attribute__((aligned (32)))
    #define DXP_ACACHE  DXP_A256

/**
 * Attribute definition to align storage on a 128 bit boundary.. Use like the following:
 *
 * static uint16 DXP_COMMONIDATA DXP_A128 dxp_taps[] = {1,2,3,4};
 */
    #define DXP_A128 __attribute__((aligned (16)))

/**
 * Attribute definition to align storage on a 64 bit boundary.. Use like the following:
 *
 * static uint16 DXP_COMMONIDATA DXP_A64 dxp_taps[] = {1,2,3,4};
 */
    #define DXP_A64 __attribute__((aligned (8)))
/**
 * Attribute definition to align storage on a 32 bit boundary.. Use like the following:
 *
 * static int8 DXP_COMMONIDATA DXP_A32 dxp_taps[] = {1,2,3,4};
 */
    #define DXP_A32 __attribute__((aligned (4)))
/**
 * Attribute definition to align storage on a 16 bit boundary.. Use like the following:
 *
 * static int8 DXP_COMMONIDATA DXP_A16 dxp_taps[] = {1,2,3,4};
 */
    #define DXP_A16 __attribute__((aligned (2)))

/**
 * Attribute definition to place data in external memory referenced relative to each DXP $dp pointer..
 * There will exist 2 instances of the variables, one on each DXP. Standard use for same code (thread) executing on both DXPs.
 * Variables are placed in a non-cacheable region.
 * Use like the following:
 * int8 DXP_UNCACHED_DUAL array[10];
 */
#define DXP_UNCACHED_DUAL __attribute__((placement(uncached), multi))

/**
 * Attribute definition to place data in external memory referenced relative to each DXP $dp pointer..
 * There will exist 2 instances of the variables, one on each DXP. Standard use for same code (thread) executing on both DXPs.
 * Use like the following:
 * int8 DXP_DUAL array[10];
 */
#define DXP_DUAL __attribute__((placement(cached), multi))
#define DXP_MULTI __attribute__((placement(cached), multi))

/**
 * Attribute definition to place data in uncached external memory.. Use like the following:
 * c.f. also DXP_UNCACHED_DUAL for uncached dual variables
 *
 * int8 DXP_UNCACHED array[10];
 */
    #define DXP_UNCACHED __attribute__((placement(uncached), uni))

/**
 * Attribute definition to place data in uncached non initialised external memory .. Use like the following:
 *
 * int8 DXP_UNCACHED_NOINIT array[10];
 */
    #define DXP_UNCACHED_NOINIT __attribute__((placement("platform_noninit"), uni))

/**
 * Attribute definition to place data in uncached GMEM. Use like DXP_UNCACHED
 *
 */
    #define DXP_UNCACHED_GMEM __attribute__((placement(gmem), uncached, uni))

/**
 * Attribute definition to place data in cached external memory for DXP0.. Use like the following:
 *
 * int8 DXP_CACHED_UNI0 array[10];
 */
    #define DXP_CACHED_UNI0 __attribute__((placement(cached), uni(0)))

/**
 * Attribute definition to place data in cached external memory for DXP1.. Use like the following:
 *
 * int8 DXP_CACHED_UNI1 array[10];
 */
    #define DXP_CACHED_UNI1 __attribute__((placement(cached), uni(1)))

/**
 * Attribute definition to place data in cached external memory for DXP2.. Use like the following:
 *
 * int8 DXP_CACHED_UNI2 array[10];
 */
    #define DXP_CACHED_UNI2 __attribute__((placement(cached), uni(2)))

#else
/* If run elsewhere (eg x86/linux) use default allocation */
    #define DXP_ICODE
    #define DXP_2GICODE
    #define DXP_3GICODE
    #define DXP_LTEICODE
    #define DXP_2GIDATA
    #define DXP_3GIDATA
    #define DXP_3GIDATA_M0
    #define DXP_3GIDATA_M1
    #define DXP_LTEIDATA
    #define DXP_LTEIDATA_M0
    #define DXP_LTEIDATA_M1
    #define DXP_COMMONIDATA
    #define DXP_COMMONIDATA_NOSR
    #define DXP_COMMONICODE
    #define DXP_COMMONGCODE
    #define DXP0_COMMONICODE
    #define DXP2_COMMONICODE
    #define DXP0_2GICODE
    #define DXP0_3GICODE
    #define DXP0_LTEICODE
    #define DXP2_2GICODE
    #define DXP2_3GICODE
    #define DXP2_LTEICODE

    #define DXP_A256
    #define DXP_A128
    #define DXP_A64
    #define DXP_A32
    #define DXP_A16

    #define DXP_UNCACHED_DUAL
    #define DXP_DUAL
    #define DXP_MULTI
    #define DXP_CACHED_UNI0
    #define DXP_CACHED_UNI1
    #define DXP_CACHED_UNI2
    #define DXP_UNCACHED
    #define DXP_UNCACHED_GMEM
    
    #define DXP1_GDATA
#endif

/* T148 TB Workaround for Nvbug 1291308 */
#if (defined ICE9140_A0  && defined LTE_FULL_UPLANE_TESTBENCH)
	#ifdef DXP_LTEICODE
	#undef DXP_LTEICODE
	#define DXP_LTEICODE
	#endif
	#ifdef DXP_2GICODE
	#undef DXP_2GICODE
	#define DXP_2GICODE
	#endif
	#ifdef DXP_3GICODE
	#undef DXP_3GICODE
	#define DXP_3GICODE
	#endif
	#ifdef DXP_LTEGCODE
	#undef DXP_LTEGCODE
	#define DXP_LTEGCODE
	#endif
	#ifdef DXP_2GGCODE
	#undef DXP_2GGCODE
	#define DXP_2GGCODE
	#endif
	#ifdef DXP_3GGCODE
	#undef DXP_3GGCODE
	#define DXP_3GGCODE
	#endif
	#ifdef DXP0_3GICODE
	#undef DXP0_3GICODE
	#define DXP0_3GICODE
	#endif
	#ifdef DXP2_3GICODE
	#undef DXP2_3GICODE
	#define DXP2_3GICODE
	#endif
	#ifdef DXP0_LTEICODE
	#undef DXP0_LTEICODE
	#define DXP0_LTEICODE
	#endif
	#ifdef DXP2_LTEICODE
	#undef DXP2_LTEICODE
	#define DXP2_LTEICODE
	#endif
#endif


#ifndef TARGET_DXP9040 
    #define DXP9140_GCODE DXP_COMMONGCODE
#else
    #define DXP9140_GCODE
#endif

#ifndef __dxp__
    /* Read a register */
    #define REG_RD1(_BASE, _OFFSET)
    #define REG_RD2(_BASE, _OFFSET)
    #define REG_RD4(_BASE, _OFFSET)
    /* Write a register */
    #define REG_WR1(_BASE, _OFFSET, _VALUE)
    #define REG_WR2(_BASE, _OFFSET, _VALUE)
    #define REG_WR4(_BASE, _OFFSET, _VALUE)
#else
    /* Read a register */
    #define REG_RD1(_BASE, _OFFSET) (((volatile int8 *) (_BASE))[(_OFFSET)/sizeof(int8)])
    #define REG_RD2(_BASE, _OFFSET) (((volatile int16 *)(_BASE))[(_OFFSET)/sizeof(int16)])
    #define REG_RD4(_BASE, _OFFSET) (((volatile int32 *)(_BASE))[(_OFFSET)/sizeof(int32)])
    /* Write a register */
    #define REG_WR1(_BASE, _OFFSET, _VALUE) ((((volatile int8 *) (_BASE))[(_OFFSET)/sizeof(int8)]) = (_VALUE))
    #define REG_WR2(_BASE, _OFFSET, _VALUE) ((((volatile int16 *)(_BASE))[(_OFFSET)/sizeof(int16)]) = (_VALUE))
    #define REG_WR4(_BASE, _OFFSET, _VALUE) ((((volatile int32 *)(_BASE))[(_OFFSET)/sizeof(int32)]) = (_VALUE))
#endif

/** @} */

#ifndef USE_STDINT_H
/**
 * @defgroup icera_common_types Fundamental types
 *
 * @{
 */

/**
 * Minimum value that can be held by type int8.
 * @see INT8_MAX
 * @see int8
 */
#define INT8_MIN        (-128)
/**
 * Maximum value that can be held by type int8.
 * @see INT8_MIN
 * @see int8
 */
#define INT8_MAX        (127)
/**
 * Maximum value that can be held by type uint8.
 * @see uint8
 */
#define UINT8_MAX       (255)
/**
 * Minimum value that can be held by type int16.
 * @see INT16_MAX
 * @see int16
 */
#define INT16_MIN       (-32768)
/**
 * Maximum value that can be held by type int16.
 * @see INT16_MIN
 * @see int16
 */
#define INT16_MAX       (32767)
/**
 * Maximum value that can be held by type uint16.
 * @see uint16
 */
#define UINT16_MAX      (65535)
/**
 * Minimum value that can be held by type int32.
 * @see INT32_MAX
 * @see int32
 */
#define INT32_MIN       (-2147483648L)
/**
 * Maximum value that can be held by type int32.
 * @see INT32_MIN
 * @see int32
 */
#define INT32_MAX       (2147483647L)
/**
 * Maximum value that can be held by type uint32.
 * @see uint32
 */
#define UINT32_MAX      (4294967295UL)
/**
 * Minimum value that can be held by type int64.
 * @see INT64_MAX
 * @see int64
 */
#define INT64_MIN        (-0x7fffffffffffffffLL-1)
/**
 * Maximum value that can be held by type int64.
 * @see INT64_MIN
 * @see int64
 */
#define INT64_MAX       (0X7fffffffffffffffLL)
/**
 * Maximum value that can be held by type uint64.
 * @see uint64
 */
#define UINT64_MAX      (0xffffffffffffffffULL)

#endif /* USE_STDINT_H */

#define com_Swift1Entry()                                           \
  {                                                                 \
    uint32 cycles, dgm0_prologue=4;                                 \
    __asm volatile ("get %0, $DBGCNT\n" : "=b" (cycles));           \
    __asm volatile ("put $DBGDQM0, %0\n" : : "b" (dgm0_prologue));  \
    __asm volatile ("put $DBGDQM1, %0\n" : :  "b" (cycles));        \
  }
#define com_Swift1Exit()                                            \
  {                                                                 \
    uint32 cycles, dgm0_epilogue=5;                                 \
    __asm volatile ("get %0, $DBGCNT\n" : "=b" (cycles));           \
    __asm volatile ("put $DBGDQM0, %0\n" : : "b" (dgm0_epilogue));  \
    __asm volatile ("put $DBGDQM1, %0\n"  : : "b" (cycles));        \
  }

#define com_Swift0Entry()                                           \
  {                                                                 \
    register uint32 dgm0_prologue=4;                                \
    __asm volatile ("put $DBGDQM0, %0\n" : : "b" (dgm0_prologue));  \
  }
#define com_Swift0Exit()                                            \
  {                                                                 \
    register uint32 dgm0_epilogue=5;                                \
    __asm volatile ("put $DBGDQM0, %0\n" : : "b" (dgm0_epilogue));  \
  }

/**
 * Converts n to ceil(n/8)
 */
#define COM_SCRATCH_BYTES_TO_DOUBLES(n) (((n)+7) >> 3)


/*************************************************************************************************
 * Public Types
 ************************************************************************************************/

/**
 * 8 bit signed integer
 * @see INT8_MAX
 * @see INT8_MIN
 */
typedef signed   char int8;
/**
 * 8 bit unsigned integer
 * @see UINT8_MAX
 */
typedef unsigned char uint8;

/**
 * 16 bit signed integer
 * @see INT16_MAX
 * @see INT16_MIN
 */
typedef signed   short int16;
/**
 * 16 bit unsigned integer
 * @see UINT16_MAX
 */
typedef unsigned short uint16;

/**
 * 32 bit signed integer
 * @see INT32_MAX
 * @see INT32_MIN
 */
typedef signed   int int32;

/**
 * 32 bit unsigned integer
 * @see UINT32_MAX
 */
typedef unsigned int uint32;

/**
 * 64 bit signed integer
 * @see INT64_MAX
 * @see INT64_MIN
 */
#if !defined(int64)
typedef signed   long long int64;
#endif

/**
 * 64 bit unsigned integer
 * @see UINT64_MAX
 */
typedef unsigned long long uint64;




/**
 * Complex number type with fixed point 8 bit members.
 */
typedef struct complex8Tag
{
    int8 real;                                                                 /*!< Real part. */
    int8 imag;                                                                 /*!< Imaginary part. */

} DXP_A16 complex8;

/**
 * Complex number type with fixed point 16 bit members.
 */
typedef struct complex16Tag
{
    int16 real;                                                                /*!< Real part. */
    int16 imag;                                                                /*!< Imaginary part. */

} DXP_A32 complex16;

/**
 * Complex number type with fixed point 32 bit members.
 */
typedef struct complex32Tag
{
    int32 real;                                                                /*!< Real part. */
    int32 imag;                                                                /*!< Imaginary part. */

} DXP_A64 complex32;

/**
 * Structure to hold a pointer into a circular buffer
 *
 * This structure allows the representation of a position in a circular buffer to be managed. The
 * position is managed in atoms
 */
typedef struct circ_ptr_s
{
    void *base;
    uint16 offset;                                                             /**< in atoms */
    uint16 length;                                                             /**< in atoms */
} circ_ptr_s;


#ifdef __dxp__

#include "mphal_overlay.h"

/* Enumerated type for specifying memory overlay IDs - use enumerations defined by mphal_overlay.h */
typedef enum
{
    OVERLAY_NONE        = DXP_OVERLAY_NONE,
    OVERLAY_ID_LTE      = MPHAL_OVERLAYID_DXP0_L1MEM_OVERLAY1,    /* LTE Memory overlay */
    OVERLAY_ID_2G       = MPHAL_OVERLAYID_DXP0_L1MEM_OVERLAY2,    /* GSM Memory overlay */
    OVERLAY_ID_3G       = MPHAL_OVERLAYID_DXP0_L1MEM_OVERLAY3,    /* UMTS memory overlay */
    MAX_OVERLAY_ID      = OVERLAY_ID_3G
} tOverlayId;

/* Enumerated type for specifying dxp2 dmem overlay IDs on DXP2 */
typedef enum
{
    OVERLAY_DXP2_ID_NONE      = DXP_OVERLAY_NONE,
    OVERLAY_DXP2_ID_LTE       = MPHAL_OVERLAYID_DXP2_L1MEM_OVERLAY1,
    OVERLAY_DXP2_ID_2G        = MPHAL_OVERLAYID_DXP2_L1MEM_OVERLAY2,
    OVERLAY_DXP2_ID_3G        = MPHAL_OVERLAYID_DXP2_L1MEM_OVERLAY3,
    MAX_OVERLAY_DXP2_ID       = OVERLAY_DXP2_ID_3G
} tOverlay2Id;

/* Enumerated type for specifying gmem overlay IDs (must not exceed MPHAL_NUM_OVERLAYS) */
typedef enum
{
    OVERLAY_GMEMA_NONE        = DXP_OVERLAY_NONE,
    OVERLAY_GMEMA_ID_LTE      = MPHAL_OVERLAYID_GMEM_SET_A_OVERLAY1,
    OVERLAY_GMEMA_ID_2G       = MPHAL_OVERLAYID_GMEM_SET_A_OVERLAY2,
    OVERLAY_GMEMA_ID_3G       = MPHAL_OVERLAYID_GMEM_SET_A_OVERLAY3,
    OVERLAY_GMEMB_ID_LTE      = MPHAL_OVERLAYID_GMEM_SET_B_OVERLAY1,
    OVERLAY_GMEMB_ID_2G       = MPHAL_OVERLAYID_GMEM_SET_B_OVERLAY2,
    OVERLAY_GMEMB_ID_3G       = MPHAL_OVERLAYID_GMEM_SET_B_OVERLAY3,
    MAX_OVERLAY_GMEMB_ID      = OVERLAY_GMEMB_ID_3G
} tOverlayGId;

#endif

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/


/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/
#define COM_MAX_SCRATCH_INSTANCES           2

/**
 * Initialise the scratch memory allocator
 *
 * @ingroup dmem_scratch
 * @param base_i64ptr Aligned pointer to the base of the internal scratch area
 * @param num_bytes   Number of bytes available. Should be a multiple of 8.
 * @param inst        Scratch memory area to initialise
 *
 * There must be no outstanding scratch use when this function is called.
 *
 * @return The maximum number of bytes allocated from the previously configured scratch buffer, or
 * 0 if SCRATCH_WATERMARKING is disabled
 *
 * @see scratch_free
 */
extern int com_ScratchInit(void *base_i64ptr, uint32 n_bytes, int32 inst);

/**
 * Reserves a memory area from the scratch stack. 
 *   Example call: 
 * <pre> int8 *data = com_ScratchAlloc(100,0);  // reserve 800 
 * bytes 
 * </pre>
 *
 * @ingroup dmem_scratch
 * @param num_doubles
 *               Number of doubles (8-byte blocks) to reserve.
 * @param inst scratch memory area to allocate from
 *
 * @return Pointer to newly reserved area.
 *
 * @see scratch_free
 */
extern void *com_ScratchAlloc(uint32 num_doubles, int32 inst);

/**
 * Return a memory area to the scratch stack. 
 *  Example call: 
 * <pre>
 * com_ScratchFree(100,0);                     // free 800 bytes
 * </pre>
 *
 * @ingroup dmem_scratch
 * @param num_doubles
 *               Number of 8 byte blocks to return.
 * @param inst scratch memory area to free from
 * @see scratch_alloc
 */
extern void com_ScratchFree(uint32 num_doubles, int32 inst);

/**
 * Check that the specified scratch memory "scratch_inst" can be freed
 */
extern void scratch_check(uint32 num_doubles, int scratch_inst);

/**
 * Read the current scratch pointer
 *  @param scratch_inst scratch memory area ptr (upper / lower) to return
 *  @return current scratch pointer
 */
extern void *com_ScratchGetCurrentPtr(int scratch_inst);

/**
 * Send out os log message containing the maximum scratch depth and free bytes
 */
extern void com_ScratchStats(void);


/*************************************************************************************************
 * Useful macro declarations
 ************************************************************************************************/

/* BIT macro define a mask for a bit a the _INDEX position in the word */
#define BIT(_INDEX) (1L<<(_INDEX))

#define BIT64(_INDEX) (1LL<<(_INDEX))


#endif

/** @} END OF FILE */

