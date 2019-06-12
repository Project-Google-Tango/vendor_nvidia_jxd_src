/*
 * Copyright (c) 2007 - 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * Defines the parameters and data structure for SNOR devices.
 */

#ifndef INCLUDED_NVBOOT_SNOR_PARAM_H
#define INCLUDED_NVBOOT_SNOR_PARAM_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum 
{
    /// Specifies SNOR clock source to be PLLP.
    NvBootSnorClockSource_PllPOut0 = 0,

    /// Specifies SNOR clock source to be PLLC.
    NvBootSnorClockSource_PllCOut0,

    /// Specifies SNOR clock source to be PLLM.
    NvBootSnorClockSource_PllMOut0,

    /// Specifies SNOR clock source to be ClockM.
    NvBootSnorClockSource_ClockM,

    NvBootSnorClockSource_Num,
    NvBootSnorClockSource_Force32 = 0x7FFFFFF
} NvBootSnorClockSource;


/**
 * Defines the parameters SNOR devices.
 */
typedef struct NvBootSnorParamsRec
{
    /// Specifies the clock source for SNOR accesses.
    NvBootSnorClockSource ClockSource;
    
    /// Specifes the clock divider to use.
    /// The value is a 7-bit value based on an input clock of 432Mhz.
    /// Divider = (432+ DesiredFrequency-1)/DesiredFrequency;
    NvU32 ClockDivider;
    
} NvBootSnorParams;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_SNOR_PARAM_H */
