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
    NvBootSnorClockSource_PllPOut0 = 1,


    /// Specifies SNOR clock source to be PLLM.
    /// This clock source may be used when mc/emc are 
    /// also initialized - PLLM will be enabled in that case.
    NvBootSnorClockSource_PllMOut0,

    /// Specifies SNOR clock source to be ClockM.
    NvBootSnorClockSource_ClockM,

    NvBootSnorClockSource_Num = NvBootSnorClockSource_ClockM,

    /// Specifies SNOR clock source to be PLLC.
    /// PLLC is never initialized by bootrom. Therefore, using this clock
    /// source will lead to SNOR controller being dead to bootrom code.
    /// Hence, moving this to set of unsupported clock sources in bootrom.
    NvBootSnorClockSource_PllCOut0,

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
    SnorDataXferMode_Pio = 1,

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
    /// Based on input clock source PLLP_OUT0 = 216Mhz, divider should be
    /// calculated as:
    /// Divider = (216 + DesiredFrequency-1)/DesiredFrequency;
    NvU32 ClockDivider;
    
    /// Specifies snor controller timing configuration
    NvBootSnorTimingParam TimingParamCfg;

    /// Specifies the mode of data transfer.
    SnorDataXferMode DataXferMode;

    /// Specifies the external memory address to which data should be
    /// transferred from nor. This parameter is valid only when dma is used.
    NvU32 DmaDestMemAddress;

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
