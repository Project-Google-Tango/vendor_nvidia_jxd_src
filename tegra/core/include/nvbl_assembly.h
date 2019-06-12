/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

/**
 * @file
 * <b>NVIDIA Tegra Boot Loader API</b>
 *
 */

#ifndef INCLUDED_NVBL_ASSEMBLY_H
#define INCLUDED_NVBL_ASSEMBLY_H 1

/// Define the default cache line size as a power of two.
#ifndef L1_CACHE_SHIFT
#define L1_CACHE_SHIFT  5
#endif

/// For AP20, use ARM v7 instruction mnemonics if nonzero -- has no effect for AP15.
/// NOTE: Requires support in Microsoft assembler before this can be enabled.
#if !defined(NVBL_USE_ARM_V7_MNEMONICS)
#define NVBL_USE_ARM_V7_MNEMONICS   0
#endif

/// For AP20, use ARM UAL instruction mnemonics if nonzero -- has no effect for AP15.
/// NOTE: Requires support in Microsoft assembler before this can be enabled.
#if !defined(NVBL_USE_ARM_UAL_MNEMONICS)
#define NVBL_USE_ARM_UAL_MNEMONICS  0
#endif

/// For AP20, use ARM TrustZone support if nonzero -- has no effect for AP15.
/// NOTE: Requires support in Microsoft assembler before this can be enabled.
#if !defined(NVBL_USE_ARM_TZ_SUPPORT)
#define NVBL_USE_ARM_TZ_SUPPORT     0
#endif

/// For AP20, use ARM MOV32 pseudo-op if nonzero -- has no effect for AP15.
/// NOTE: Requires support in Microsoft assembler before this can be enabled.
#if !defined(NVBL_USE_ARM_MOV32_MNEMONIC)
#define NVBL_USE_ARM_MOV32_MNEMONIC 0
#endif

#define NVBL_SECURE_BOOT_JTAG_CLEAR  0xFFFFFFF0
#define NVBL_SECURE_BOOT_PFCFG_OFFSET  0x8
#define NVBL_SECURE_BOOT_CSR_OFFSET    0X0
#define NVBL_SECURE_JTAG_OFFSET_IN_CSR 0x5
#define NVBL_DBGEN_OFFSET_IN_PFCFG     0x3

// Bit mask of JTAG bits to disable  (Used for T30)
#if defined(CONFIG_TRUSTED_FOUNDATIONS) || defined(CONFIG_TRUSTED_LITTLE_KERNEL)
    #if S_PROF_OFF_DBG_OFF_NS_PROF_OFF_DBG_OFF
        #define NVBL_SECURE_BOOT_JTAG_VAL 0x0
    #elif S_PROF_OFF_DBG_OFF_NS_PROF_ON_DBG_OFF
        #define NVBL_SECURE_BOOT_JTAG_VAL 0x4
    #elif S_PROF_OFF_DBG_OFF_NS_PROF_ON_DBG_ON
        #define NVBL_SECURE_BOOT_JTAG_VAL 0xC
    #elif S_PROF_ON_DBG_ON_NS_PROF_ON_DBG_ON
        #define NVBL_SECURE_BOOT_JTAG_VAL 0xF
    #else
        #define NVBL_SECURE_BOOT_JTAG_VAL 0x0
    #endif
#else
    #if NS_PROF_ON_DBG_OFF
        #define NVBL_SECURE_BOOT_JTAG_VAL 0x5
    #elif NS_PROF_ON_DBG_ON
        #define NVBL_SECURE_BOOT_JTAG_VAL 0xF
    #else
        #define NVBL_SECURE_BOOT_JTAG_VAL 0xF
    #endif
#endif


// JTAG configuration in T114
#if defined(CONFIG_TRUSTED_FOUNDATIONS) || defined(CONFIG_TRUSTED_LITTLE_KERNEL)
    #if SECURE_PROF
        #define NVBL_SECURE_PROF 0x1
    #else
        #define NVBL_SECURE_PROF 0x0
    #endif

    #if SECURE_DEBUG
        #define NVBL_SECURE_DEBUG 0x2
    #else
        #define NVBL_SECURE_DEBUG 0x0
    #endif
#endif
#if NON_SECURE_PROF
    #define NVBL_NON_SECURE_PROF 0x4
#else
    #define NVBL_NON_SECURE_PROF 0x0
#endif

#if defined(CONFIG_TRUSTED_FOUNDATIONS) || defined(CONFIG_TRUSTED_LITTLE_KERNEL)
#define NVBL_SECURE_BOOT_DEBUG_CONFIG ((1 << NVBL_DBGEN_OFFSET_IN_PFCFG) |\
                                       NVBL_NON_SECURE_PROF         |\
                                       NVBL_SECURE_PROF             |\
                                       NVBL_SECURE_DEBUG)
#else
#define NVBL_SECURE_BOOT_DEBUG_CONFIG ((1 << NVBL_DBGEN_OFFSET_IN_PFCFG) |\
                                       NVBL_NON_SECURE_PROF)
#endif


/**
 * @defgroup nvbl_assembly_group NvBL Assembly API
 *
 *
 *
 * @ingroup nvbl_group
 * @{
 */

#if defined(ASSEMBLY_SOURCE_FILE)
#ifndef INCLUDED_NVCOMMON_H
// OS-related #define's copied from nvcommon.h because nvcommon.h can't be
// included here.
#if defined(_WIN32)
  #define NVOS_IS_WINDOWS 1

#elif defined(__linux__)
  #define NVOS_IS_LINUX 1
  #define NVOS_IS_UNIX 1
#elif defined(__arm__)  && defined(__ARM_EABI__)
    /* GCC arm eabi compiler, potentially used for kernel compilation without
     * __linux__, but also for straight EABI (AOS) executable builds */
#  if defined(__KERNEL__)
#    define NVOS_IS_LINUX 1
#    define NVOS_IS_UNIX 1
#    define NVOS_IS_LINUX_KERNEL 1
#  endif
    /* Nothing to define for AOS */
#elif defined(__arm)
  // For ARM RVDS compiler, we don't know the final target OS at compile time
#else
  #error Unknown OS
#endif

#if !defined(NVOS_IS_WINDOWS)
#define NVOS_IS_WINDOWS 0
#endif
#if !defined(NVOS_IS_LINUX)
#define NVOS_IS_LINUX 0
#endif
#if !defined(NVOS_IS_UNIX)
#define NVOS_IS_UNIX 0
#endif
#if !defined(NVOS_IS_LINUX_KERNEL)
#define NVOS_IS_LINUX_KERNEL 0
#endif
#endif // !INCLUDED_NVCOMMON_H

//-----------------------------------------------------------------------------
// Assemblers don't like C-style constant suffix operators in spec-generated
// headers.
//-----------------------------------------------------------------------------
#undef  _MK_ENUM_CONST
#define _MK_ENUM_CONST(_constant_) (_constant_)

//-----------------------------------------------------------------------------
// Abstract assember operator because some ARM assemblers don't accept
// the common ARM-defined assembler operators. They want their own operators.
//-----------------------------------------------------------------------------

#if   defined(__GNUC__)   // GNU compiler
#define IMPORT .extern
#define ALIGN  .align 2
#define P2ALIGN(x) .align x
#define BALIGN(x) .balign x
#define EXPORT .globl
#define AREA .section
#define LABEL :
#define END .end
#define LTORG .ltorg
#define SPACE .space
#define DCD .word
#define DCB .byte
#define IF .if
#define ENDIF .endif
#define EQUATE(A,B)   .EQU A, B
#define TEXT    .section .text
#define PRESERVE8
#else
#if defined(_MSC_VER)
#define P2ALIGN(x) ALIGN 1<<x
#define BALIGN(x) ALIGN x
#else
#define P2ALIGN(x) ALIGN 1<<x
// BALIGN (arbitrary byte alignment) not support for ARM tool chain
#endif
#define LABEL
#define EQUATE(A,B)   (A)    EQU    (B)
#define TEXT    AREA |.text|,ALIGN=4,CODE,READONLY
#endif

//-----------------------------------------------------------------------------
// Abstract logical operations because some ARM assemblers don't
// understand C operators.
//-----------------------------------------------------------------------------

#if !defined(_AND_)
#if defined(_MSC_VER)
#define _AND_ :AND:
#else
#define _AND_ &
#endif
#endif

#if !defined(_OR_)
#if defined(_MSC_VER)
#define _OR_ :OR:
#else
#define _OR_ |
#endif
#endif

#if !defined(_SHL_)
#if defined(_MSC_VER)
#define _SHL_ :SHL:
#else
#define _SHL_ <<
#endif
#endif

#if !defined(_SHR_)
#if defined(_MSC_VER)
#define _SHR_ :SHR:
#else
#define _SHR_ >>
#endif
#endif

//-----------------------------------------------------------------------------
// Abstract debug build EXPORTs whose only purpose is to make symbols visible
// to debuggers.
//-----------------------------------------------------------------------------

#if !defined(NV_DEBUG)
#define NV_DEBUG    0
#define DEBUG_EXPORT(x)
#elif   NV_DEBUG
#define DEBUG_EXPORT(x)    EXPORT x
#else
#define DEBUG_EXPORT(x)
#endif

#if defined(_MSC_VER) || defined (__ARMCC_VERSION)  // Microsoft or RVDS compiler


#if !NVBL_USE_ARM_V7_MNEMONICS
#define CLREX   DCD     0xF57FF01F      // Clear Exclusive
#define DMB     DCD     0xF57FF05F      // Data Memory Barrier
#define DSB     DCD     0xF57FF04F      // Data Synchronization Barrier
#define ISB     DCD     0xF57FF06F      // Instruction Synchronization Barrier
#define POP     LDMFD   sp!,            // Pop
#define PUSH    STMFD   sp!,            // Push
#define SEVAL   DCD     0xE320F004      // Send Event Always
#define SEV     SEVAL                   // Send Event
#define VLDM    FLDMIAD                 // Vector Load Multiple (double precision only)
#define VMRS    FMRX                    // Move VFP System Reg to General Purpose Reg
#define VMSR    FMXR                    // Move General Purpose Reg to VFP System Reg
#define VSTM    FSTMIAD                 // Vector Store Multiple (double precision only)
#define WFEAL   DCD     0xE320F002      // Wait For Exception Always
#define WFE     WFEAL                   // Wait For Exception
#define WFIAL   DCD     0xE320F003      // Wait For Interrupt Always
#define WFI     WFIAL                   // Wait For Exception
#endif  // !NVBL_USE_ARM_V7_MNEMONICS

#if !NVBL_USE_ARM_UAL_MNEMONICS
#define ITTT    ;                       // If-Then-Then-Then instruction
#define LDM     LDMIA                   // Load Multiple
#define STM     STMIA                   // Store Multiple
#define SUBSNE  SUBNES                  // Subtract if NE set condition codes
#endif

#endif  // defined(_MSC_VER) || defined(__ARMCC_VERSION)

#endif // defined(ASSEMBLY_SOURCE_FILE)

/** @} */

#endif

