/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

//Don't compile this for gcc yet
#if !__GNUC__
#define _MK_ENUM_CONST(_constant_) _constant_
#include "t11x/arapb_misc.h"
#include "t11x/arclk_rst.h"

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

#define MISC_PA_BASE     0x70000000  // Base address for arapb_misc.h registers
#define CLK_RST_BASE     0x60006000  // Base address for arclk_rst.h registers

//###########################################################################
//############################### CODE ######################################
//###########################################################################

#pragma push
#pragma arm
//===========================================================================
// NvOsEnableJtagDebugging() - jtag enable function
//
// If debugging is enabled, this function will be patched (prior to code
// execution) such that the data at label NvOsSavedStartAddress will be
// replaced with the entry point address for the program.
//
// When the spin loop is exited (by user manually setting the PC to a value
// of PC+4), this code will branch and begin executing the original program
// instructions.
//===========================================================================
volatile __asm void NvOsEnableJtagDebugging(void)
{
    // Force this to be in ARM mode instead of Thumb
    ARM

    // retrieve the real entry point start address
    ldr r2, =(NvOsSavedStartAddress)
    ldr r2, [r2]

    // enable JTAG
    ldr r0, =(MISC_PA_BASE + APB_MISC_PP_CONFIG_CTL_0)
    ldr r1, =(NvOsJtagEnableBitField)
    ldr r1, [r1]
    str r1, [r0]

    // enable CPU clock
    ldr r3, =(CLK_RST_BASE + CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0)
    ldr r3, [r3]
    ldr r1, =(NvOsCpuClkEnableField)
    ldr r1, [r1]
    orr r0, r1, r3
    ldr r1, =(CLK_RST_BASE + CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0)
    str r0, [r1]

    // clear CPU reset field
    ldr r3, =(CLK_RST_BASE + CLK_RST_CONTROLLER_RST_DEVICES_L_0)
    ldr r3, [r3]
    ldr r1, =(NvOsCpuClkEnableField)
    ldr r1, [r1]
    bic r0, r3, r1
    ldr r1, =(CLK_RST_BASE + CLK_RST_CONTROLLER_RST_DEVICES_L_0)
    str r0, [r1]

    // spin to allow JTAG connection
    // manually increment pc by 4 to continue
NvOsJtagDebugLoop
    bal NvOsJtagDebugLoop

    // branch to entry point
    bx r2

NvOsJtagEnableBitField
    dcd (APB_MISC_PP_CONFIG_CTL_0_JTAG_ENABLE << APB_MISC_PP_CONFIG_CTL_0_JTAG_SHIFT) | \
        (APB_MISC_PP_CONFIG_CTL_0_TBE_ENABLE << APB_MISC_PP_CONFIG_CTL_0_TBE_SHIFT)

NvOsCpuClkEnableField
    dcd (CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_CPU_ENABLE<< \
         CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0_CLK_ENB_CPU_SHIFT)

NvOsSavedStartAddress
    dcd 0xDEADBEEF
}
#pragma pop
#endif //__GNUC__
