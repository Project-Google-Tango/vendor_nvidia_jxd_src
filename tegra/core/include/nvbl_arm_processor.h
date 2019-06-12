//
// Copyright (c) 2007 - 2012 NVIDIA Corporation.  All Rights Reserved.
//
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation.  Any
// use, reproduction, disclosure or distribution of this software and related
// documentation without an express license agreement from NVIDIA Corporation
// is strictly prohibited.
//

/**
 * @file
 * <b>NVIDIA Tegra Boot Loader API</b>
 *
 */

#ifndef INCLUDED_NVBL_ARM_PROCESSOR_H
#define INCLUDED_NVBL_ARM_PROCESSOR_H

/**
 * @defgroup nvbl_armproc_group NvBL ARM Processor API
 * See the header file for macro definitions.
 *
 *
 * @ingroup nvbl_group
 * @{
 */

#include "nvbl_common.h"
#include "nvbl_arm_cpsr.h"

#ifdef  _MSCVER         // Microsoft compiler
#include <armintr.h>
#endif

#ifdef  __cplusplus
extern "C" {
#endif

//==========================================================================
// Compiler-specific status and coprocessor register abstraction macros.
//==========================================================================

#if   defined(__ARMCC_VERSION)    // ARM compiler

    /**
     *  @brief Macro to abstract retrieval of the current processor status register (CPSR) value.
     *  @param x is a variable of type NvU32 that will receive the CPSR value.
    */
    __inline NvU32 _ReadStatusReg(void)
    {
        int temp;
        __asm
        {
            MRS temp, CPSR
        }

        return temp;
    }

    #define GET_CPSR(x) *((NvU32*)(&(x))) = _ReadStatusReg() // x = CPSR

    /**
     *  @brief Macro to count the number of leading zeros in a 32 bit value.
     *  @param x is a variable of type NvU32.
    */
    #define CountLeadingZeros(x) __asm { CLZ r0, x }    // R0 = # leading zeros

    /**
     * @brief Return current stack pointer.
     *
     * @retval Contents of the stack pointer register.
     */
    // This is a built-in intrinsic function in the ARM compiler.
    // NvU32 __current_sp(void);

#elif   defined(__GNUC__)   // GCC

    #if NVCPU_IS_ARM

        /**
         *  @brief Macro to abstract retrieval of the current processor status register (CPSR) value.
         *  @param x is a variable of type NvU32 that will receive the CPSR value.
        */
        #define GET_CPSR(x) asm(" MRS %0, CPSR" :"=r"(x))           // x = CPSR

    #else

        /**
         *  @brief Macro to abstract retrieval of the current processor status register (CPSR) value.
         *  @param x is a variable of type NvU32 that will receive the CPSR value.
        */
        #error "GET_CPSR(x) is only supported for ARM processors"

    #endif

    /**
     *  @brief Macro to count the number of leading zeros in a 32 bit value.
     *  @param x is a variable of type NvU32.
    */
    #define CountLeadingZeros(x) __builtin_clz(x)

    /**
     * @brief Return current stack pointer.
     *
     * @retval Contents of the stack pointer register.
     */
    NvU32 __current_sp(void);

#else
    #error "Unknown compiler"
#endif

#ifdef  __cplusplus
}
#endif

/** @} */

#endif // INCLUDED_NVBL_ARM_PROCESSOR_H
