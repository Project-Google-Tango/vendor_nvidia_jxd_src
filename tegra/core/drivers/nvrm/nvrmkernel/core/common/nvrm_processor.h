//
// Copyright (c) 2007 - 2012 NVIDIA Corporation.  All Rights Reserved.
//
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation.  Any
// use, reproduction, disclosure or distribution of this software and related
// documentation without an express license agreement from NVIDIA Corporation
// is strictly prohibited.
//


#ifndef INCLUDED_NVRM_PROCESSOR_H
#define INCLUDED_NVRM_PROCESSOR_H

#include "nvcommon.h"

#ifdef  __cplusplus
extern "C" {
#endif


//==========================================================================
//  ARM CPSR/SPSR definitions
//==========================================================================

#define PSR_MODE_MASK   0x1F
#define PSR_MODE_USR    0x10
#define PSR_MODE_FIQ    0x11
#define PSR_MODE_IRQ    0x12
#define PSR_MODE_SVC    0x13
#define PSR_MODE_ABT    0x17
#define PSR_MODE_UND    0x1B
#define PSR_MODE_SYS    0x1F    // only available on ARM Arch. v4 and higher
#define PSR_MODE_MON    0x16    // only available on ARM Arch. v6 and higher with TrustZone extension


//==========================================================================
// Compiler-independent abstraction macros.
//==========================================================================

#define IS_USER_MODE(cpsr)  ((cpsr & PSR_MODE_MASK) == PSR_MODE_USR)

//==========================================================================
// Compiler-specific instruction abstraction macros.
//==========================================================================

#if  defined(__arm__) && !defined(__thumb__)  // ARM compiler compiling ARM code

    #if (__GNUC__) // GCC inline assembly syntax

    static NV_INLINE NvU32
    CountLeadingZeros(NvU32 x)
    {
        NvU32 count;
        __asm__ __volatile__ (      \
                "clz %0, %1 \r\t"   \
                :"=r"(count)        \
                :"r"(x));
        return count;
    }

    #define GET_CPSR(x) __asm__ __volatile__ (          \
                                "mrs %0, cpsr\r\t"     \
                                : "=r"(x))

    #else   // assume RVDS compiler
    /*
     *  @brief Macro to abstract retrieval of the current processor
     *  status register (CPSR) value.
     *  @param x is a variable of type NvU32 that will receive
     *  the CPSR value.
     */
    #define GET_CPSR(x) __asm { MRS x, CPSR }           // x = CPSR

    static NV_INLINE NvU32
    CountLeadingZeros(NvU32 x)
    {
        NvU32 count;
        __asm { CLZ count, x }
        return count;
    }

    #endif
#else
    /*
     *  @brief Macro to abstract retrieval of the current processor status register (CPSR) value.
     *  @param x is a variable of type NvU32 that will receive the CPSR value.
    */
    #define GET_CPSR(x) (x = PSR_MODE_USR)  // Always assume USER mode for now

    // If no built-in method for counting leading zeros do it the less efficient way.
    static NV_INLINE NvU32
    CountLeadingZeros(NvU32 x)
    {
        NvU32   i;

        if (x)
        {
            i = 0;

            do
            {
                if (x & 0x80000000)
                {
                    break;
                }
                x <<= 1;
            } while (++i < 32);
        }
        else
        {
            i = 32;
        }

        return i;
    }

#endif

#ifdef  __cplusplus
}
#endif

#endif // INCLUDED_NVRM_PROCESSOR_H

