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
    NvBootSnorClockSource_PllPOut0 = 0,              // 0
    NvBootSnorClockSource_Rsvd1    = 0x7FFFFFF,      // PllC2Out Not Supported
    NvBootSnorClockSource_Rsvd2    = 0x7FFFFFF,      // PllCOut Not Supported
    NvBootSnorClockSource_Rsvd3    = 0x7FFFFFF,      // PllC3Out Not Supported
    NvBootSnorClockSource_Rsvd4    = 0x7FFFFFF,      // PllMOut Not Supported
    NvBootSnorClockSource_Rsvd5    = 0x7FFFFFF,
    /// Specifies SNOR clock source to be ClockM.
    NvBootSnorClockSource_ClockM   = 6,
    NvBootSnorClockSource_Num = NvBootSnorClockSource_ClockM,
    NvBootSnorClockSource_Force32 = 0x7FFFFFF
} NvBootSnorClockSource;

/**
 * Defines the snor timing parameters
 */
typedef struct NvBootSnorTimingParamRec
{
    /// SNOR timing config 0
    /// [31:28] : PAGE_RDY_WIDTH
    /// [23:20] : PAGE_SEQ_WIDTH
    /// [15:12] : MUXED_WIDTH
    /// [11:8]  : HOLD_WIDTH
    /// [7:4]   : ADV_WIDTH
    /// [3:0]   : CE_WIDTH
    NvU32 SnorTimingCfg0;

    /// SNOR timing config 1
    /// [31:26] : SNOR_TAP_DELAY
    /// [24:24] : SNOR_CE
    /// [23:16] : WE_WIDTH
    /// [15:8]  : OE_WIDTH
    /// [7:0]   : WAIT_WIDTH
    NvU32 SnorTimingCfg1;

    /// SNOR timing config 2
    /// [5:0] : SNOR_IN_TAP_DELAY
    NvU32 SnorTimingCfg2;

} NvBootSnorTimingParam;

/**
 * Identifies the data transfer mode to be used by the snor controller.
 */
typedef enum{
    /// Use Pio mode for data transfer from nor to memory
    SnorDataXferMode_Pio,

    /// Use Dma mode for data transfer from nor to memory
    SnorDataXferMode_Dma,

    SnorDataXferMode_Max,
    SnorDataXferMode_Force32 = 0x7FFFFFFF,
} SnorDataXferMode;

/**
 * Defines the parameters SNOR devices.
 */
typedef struct NvBootSnorParamsRec
{
    /// Specifies the clock source for SNOR accesses.
    NvBootSnorClockSource ClockSource;

    /// Specifies the clock divider to use.
    /// Based on input clock source PLLP_OUT0 = 204Mhz, divider should be
    /// calculated as:
    /// Divider = (204 + DesiredFrequency-1)/DesiredFrequency;
    NvU32 ClockDivider;

    /// Specifies snor controller timing configuration
    /// SNOR timing config 0
    NvU32 SnorTimingCfg0;

    /// SNOR timing config 1
    NvU32 SnorTimingCfg1;

    /// SNOR timing config 2
    NvU32 SnorTimingCfg2;

    /// Specifies the mode of data transfer.
    SnorDataXferMode DataXferMode;

    /// Maximum time for which driver should wait for controller busy
    /// Value specified in microseconds and valid only for dma mode transfers.
    NvU32 CntrllerBsyTimeout;

    /// Maximum time for which driver should wait for dma transfer to complete
    /// Value specified in microseconds and valid only for dma mode transfers.
    NvU32 DmaTransferTimeout;

} NvBootSnorParams;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_SNOR_PARAM_H */
