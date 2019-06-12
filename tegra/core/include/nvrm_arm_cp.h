//
// Copyright (c) 2007 - 2012 NVIDIA Corporation.  All Rights Reserved.
//
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation.  Any
// use, reproduction, disclosure or distribution of this software and related
// documentation without an express license agreement from NVIDIA Corporation
// is strictly prohibited.
//


#ifndef INCLUDED_ARM_CP_H
#define INCLUDED_ARM_CP_H

#include "nvassert.h"

#ifdef  __cplusplus
extern "C" {
#endif

//==========================================================================
// Compiler-specific status and coprocessor register abstraction macros.
//==========================================================================

#if defined(__ARMCC_VERSION)  // ARM compiler

    /*
     *  @brief Macro to abstract writing of a ARM coprocessor register via the MCR instruction.
     *  @param cp is the coprocessor name (e.g., p15)
     *  @param op1 is a coprocessor-specific operation code (must be a manifest constant).
     *  @param Rd is a variable that will be written to the coprocessor register.
     *  @param CRn is the destination coprocessor register (e.g., c7)
     *  @param CRm is an additional destination coprocessor register (e.g., c2).
     *  @param op2 is a coprocessor-specific operation code (must be a manifest constant).
    */
    #define MCR(cp,op1,Rd,CRn,CRm,op2)  __asm { MCR cp, op1, Rd, CRn, CRm, op2 }

    /*
     *  @brief Macro to abstract reading of a ARM coprocessor register via the MRC instruction.
     *  @param cp is the coprocessor name (e.g., p15)
     *  @param op1 is a coprocessor-specific operation code (must be a manifest constant).
     *  @param Rd is a variable that will receive the value read from the coprocessor register.
     *  @param CRn is the source coprocessor register (e.g., c7).
     *  @param CRm is an additional source coprocessor register (e.g., c2).
     *  @param op2 is a coprocessor-specific operation code (must be a manifest constant).
    */
    #define MRC(cp,op1,Rd,CRn,CRm,op2)  __asm { MRC cp, op1, Rd, CRn, CRm, op2 }

    /*
     *  @brief Macro to abstract writing of a ARM coprocessor register via the MCCR instruction.
     *  @param cp is the coprocessor name (e.g., p15)
     *  @param opc is a coprocessor-specific operation code (must be a manifest constant).
     *  @param Rt is the low-order word of a variable that will be written to the coprocessor register.
     *  @param Rt2 is the high-order word of a variable that will be written to the coprocessor register.
     *  @param CRm is the destination coprocessor register (e.g., c7)
    */
    #define MCRR(cp,opc,Rt,Rt2,CRm)  __asm { MCRR cp, opc, Rt, Rt2, CRm }

    /*
     *  @brief Macro to abstract reading of a ARM coprocessor register via the MRRC instruction.
     *  @param cp is the coprocessor name (e.g., p15)
     *  @param opc is a coprocessor-specific operation code (must be a manifest constant).
     *  @param Rd is low-order word of a variable that will receive the value read from the coprocessor register.
     *  @param Rd2 is high-order word of a variable that will receive the value read from the coprocessor register.
     *  @param CRm is the source coprocessor register (e.g., c2).
    */
    #define MRRC(cp,opc,Rt,Rt2,CRm)  __asm { MRRC cp, opc, Rt, Rt2, CRm }

#elif NVOS_IS_LINUX || __GNUC__ // linux compilers

    #if defined(__arm__)    // ARM GNU compiler

    // Define the standard ARM coprocessor register names because the ARM compiler requires
    // that we use the names and the GNU compiler requires that we use the numbers.
    #define p14     14
    #define p15     15
    #define c0      0
    #define c1      1
    #define c2      2
    #define c3      3
    #define c4      4
    #define c5      5
    #define c6      6
    #define c7      7
    #define c8      8
    #define c9      9
    #define c10     10
    #define c11     11
    #define c12     12
    #define c13     13
    #define c14     14
    #define c15     15

    /*
     *  @brief Macro to abstract writing of a ARM coprocessor register via the MCR instruction.
     *  @param cp is the coprocessor name (e.g., p15)
     *  @param op1 is a coprocessor-specific operation code (must be a manifest constant).
     *  @param Rd is a variable that will receive the value read from the coprocessor register.
     *  @param CRn is the destination coprocessor register (e.g., c7).
     *  @param CRm is an additional destination coprocessor register (e.g., c2).
     *  @param op2 is a coprocessor-specific operation code (must be a manifest constant).
    */
    #define MCR(cp,op1,Rd,CRn,CRm,op2)  asm(" MCR " #cp",%1,%2,"#CRn","#CRm ",%5" \
        : : "i" (cp), "i" (op1), "r" (Rd), "i" (CRn), "i" (CRm), "i" (op2))

    /*
     *  @brief Macro to abstract reading of a ARM coprocessor register via the MRC instruction.
     *  @param cp is the coprocessor name (e.g., p15)
     *  @param op1 is a coprocessor-specific operation code (must be a manifest constant).
     *  @param Rd is a variable that will receive the value read from the coprocessor register.
     *  @param CRn is the source coprocessor register (e.g., c7).
     *  @param CRm is an additional source coprocessor register (e.g., c2).
     *  @param op2 is a coprocessor-specific operation code (must be a manifest constant).
    */
    #define MRC(cp,op1,Rd,CRn,CRm,op2)  asm( " MRC " #cp",%2,%0," #CRn","#CRm",%5" \
        : "=r" (Rd) : "i" (cp), "i" (op1), "i" (CRn), "i" (CRm), "i" (op2))

    /*
     *  @brief Macro to abstract writing of a ARM coprocessor register via the MCCR instruction.
     *  @param cp is the coprocessor name (e.g., p15)
     *  @param opc is a coprocessor-specific operation code (must be a manifest constant).
     *  @param Rt is the low-order word of a variable that will be written to the coprocessor register.
     *  @param Rt2 is the high-order word of a variable that will be written to the coprocessor register.
     *  @param CRm is the destination coprocessor register (e.g., c7)
    */
    #define MCRR(cp,opc,Rt,Rt2,CRm)  asm(" MCRR " #cp",%1,%2,%3,c"#CRm \
        : : "i" (cp), "i" (opc), "r" (Rt), "r" (Rt2), "i" (CRm))

    /*
     *  @brief Macro to abstract reading of a ARM coprocessor register via the MRRC instruction.
     *  @param cp is the coprocessor name (e.g., p15)
     *  @param opc is a coprocessor-specific operation code (must be a manifest constant).
     *  @param Rd is low-order word of a variable that will receive the value read from the coprocessor register.
     *  @param Rd2 is high-order word of a variable that will receive the value read from the coprocessor register.
     *  @param CRm is the source coprocessor register (e.g., c2).
    */
    #define MRRC(cp,opc,Rt,Rt2,CRm)  asm(" MRRC " #cp",%3,%0,%1,c"#CRm \
        : "=r" (Rt), "=r" (Rt2) : "i" (cp), "i" (opc), "i" (CRm))

    #else

    /* x86 processor. No such instructions. Callers should not call these macros
     * when running on x86. If they do, it will compile but will not work. */
    #define MCR(cp,op1,Rd,CRn,CRm,op2)  do { Rd = Rd; NV_ASSERT(0); } while (0)
    #define MRC(cp,op1,Rd,CRn,CRm,op2)  do { Rd = 0; /*NV_ASSERT(0);*/ } while (0)
    #define MCRR(cp,opc,Rt,Rt2,CRm)     do { Rt = Rt; Rt2 = Rt2; NV_ASSERT(0); } while (0)
    #define MRRC(cp,opc,Rt,Rt2,CRm)     do { Rt = 0; Rt2 = 0; /*NV_ASSERT(0);*/ } while (0)

    #endif

    /*
     * The GCC equivalent to RVDS barrier intrinsics are empty because
     * GCC does the right thing without any hand-holding.
     */
    #define __memory_changed()
    #define __schedule_barrier()

#else

    // !!!FIXME!!! TEST FOR ALL KNOWN COMPILERS -- FOR NOW JUST DIE AT RUN-TIME
    // #error "Unknown compiler"
    #define MCR(cp,op1,Rd,CRn,CRm,op2)  do { Rd = Rd; NV_ASSERT(0); } while (0)
    #define MRC(cp,op1,Rd,CRn,CRm,op2)  do { Rd = 0; /*NV_ASSERT(0);*/ } while (0)
    #define MCRR(cp,opc,Rt,Rt2,CRm)     do { Rt = Rt; Rt2 = Rt2; NV_ASSERT(0); } while (0)
    #define MRRC(cp,opc,Rt,Rt2,CRm)     do { Rt = 0; Rt2 = 0; /*NV_ASSERT(0);*/ } while (0)

#endif

/* @brief 64-bit to 2x32-bit union mapping type.
 */
union _union64
{
    NvU64   _u64;
    struct
    {
        NvU32 _lo;
        NvU32 _hi;
    } _u32;
};

/*  @brief Helper macro for writing 64-bit variables to ARM coprocessor
 *         registers via the MRRC instruction
 *  @param cp is the coprocessor name (e.g., p15)
 *  @param opc is a coprocessor-specific operation code (must be a manifest constant)
 *  @param u64 is 64-bit variable that will be written to the coprocessor register
 *  @param CRm is the source coprocessor register (e.g., c2)
 *
 *  NOTE: The call to the RVDS __memory_changed() and __schedule_barrier()
 *        intrinsics are requried when compiling under RVDS. The compiler
 *        will generate the instructions in the wrong sequence without
 *        them. When compiling under GCC, these calls expand to nothing.
 */
#define MCRR_U64(cp,opc,u64,CRm) do {       \
    volatile union _union64 u;              \
    u._u64 = u64;                           \
    __memory_changed();                     \
    MCRR(cp,opc,u._u32._lo,u._u32._hi,CRm); \
    u = u;  /* Suppress warnings */         \
    __schedule_barrier();                   \
} while (0)

/*  @brief Helper macro for reading 64-bit variables from ARM coprocessor
 *         registers via the MCRR instruction
 *  @param cp is the coprocessor name (e.g., p15)
 *  @param opc is a coprocessor-specific operation code (must be a manifest constant)
 *  @param u64 is 64-bit variable that will be receive the value read from the coprocessor register
 *  @param CRm is the source coprocessor register (e.g., c2)
 *
 *  NOTE: The call to the RVDS __schedule_barrier() intrinsic is requried
 *        when compiling under RVDS. The compiler will generate the
 *        instructions in the wrong sequence without them. When compiling
 *        under GCC, this call expand to nothing.
 */
#define MRRC_U64(cp,opc,u64,CRm) do {       \
    volatile union _union64 u;              \
    MRRC(cp,opc,u._u32._lo,u._u32._hi,CRm); \
    u64 = u._u64;                           \
    __schedule_barrier();                   \
} while (0)


#ifdef  __cplusplus
}
#endif

#endif // INCLUDED_ARM_CP_H

