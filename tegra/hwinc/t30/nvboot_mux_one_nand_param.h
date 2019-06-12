/*
 * Copyright (c) 2008 - 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


/**
 * Defines the parameters and data structure for MuxOneNAND and
 * FlexMuxOneNAND devices.
 */

#ifndef INCLUDED_NVBOOT_MUX_ONE_NAND_PARAM_H
#define INCLUDED_NVBOOT_MUX_ONE_NAND_PARAM_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Defines the clock sources that can be selected for MuxOneNAND and
 * FlexMuxOneNAND devices.
 */
/*
 * The order of the enum MUST match the HW definition for the first
 * four values.  Note that this is a violation of the SW convention on enums.
 */
typedef enum 
{
    /// Specifies PLLP as the clock source
    NvBootMuxOneNandClockSource_PllPOut0 = 0,

    /// Specifies PLLC as the clock source
    NvBootMuxOneNandClockSource_PllCOut0,

    /// Specifies PLLM as the clock source
    NvBootMuxOneNandClockSource_PllMOut0,

    /// Specifies ClockM as the clock source
    NvBootMuxOneNandClockSource_ClockM,

    NvBootMuxOneNandClockSource_Num,
    NvBootMuxOneNandClockSource_Force32 = 0x7FFFFFF
} NvBootMuxOneNandClockSource;

/// Defines the params that can be configured for MuxOneNAND and
/// FlexMuxOneNAND devices.
typedef struct NvBootMuxOneNandParamsRec
{
    /// Specifies the clock source for the MuxOneNand/SNOR controller
    NvBootMuxOneNandClockSource ClockSource;

    /// Specifies the clock divider for the chosen clock source
    NvU32 ClockDivider;

} NvBootMuxOneNandParams;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_MUX_ONE_NAND_PARAM_H */
