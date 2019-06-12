/*
 * Copyright (c) 2007 - 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVBOOT_OSC_H
#define INCLUDED_NVBOOT_OSC_H

#include "t30/arclk_rst.h"

/**
 * Defines the oscillator frequencies supported by the hardware.
 */

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Set of oscillator frequencies supported by the hardware.
 */
/*
 * Set of oscillator frequencies supproted in the internal API
 * + invalid (measured but not in any valid band)
 * + unknown (not measured at all)
 * Locally define NvBootClocksOscFreq here to have the correct collection of
 * oscillator frequencies.
 */
typedef enum
{  
    /// Specifies an oscillator frequency of 13MHz.
    NvBootClocksOscFreq_13 = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC13,

    /// Specifies an oscillator frequency of 19.2MHz.
    NvBootClocksOscFreq_19_2 = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC19P2,

    /// Specifies an oscillator frequency of 12MHz.
    NvBootClocksOscFreq_12 = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC12,

    /// Specifies an oscillator frequency of 26MHz.
    NvBootClocksOscFreq_26 = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC26,

    /// Specifies an oscillator frequency of 16.8MHz.
    NvBootClocksOscFreq_16_8 = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC16P8,

    /// Specifies an oscillator frequency of 38.4MHz.
    NvBootClocksOscFreq_38_4 = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC38P4,

    /// Specifies an oscillator frequency of 48MHz.
    NvBootClocksOscFreq_48 = CLK_RST_CONTROLLER_OSC_CTRL_0_OSC_FREQ_OSC48,

    NvBootClocksOscFreq_Num = 7,            // dummy to get number of frequencies
    NvBootClocksOscFreq_MaxVal = 13,      // dummy to get the max enum value
    NvBootClocksOscFreq_Unknown = 15,   // unused value to use a illegal/undefined frequency
    NvBootClocksOscFreq_Force32 = 0x7fffffff
} NvBootClocksOscFreq;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_OSC_H */
