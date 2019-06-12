/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */


#ifndef INCLUDED_NVAVP_SDMMC_HEADERS_T30_H
#define INCLUDED_NVAVP_SDMMC_HEADERS_T30_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvbl_sdmmc.h"
#include "arahb_arbc.h"
#include "nvboot_device_int.h"



// TEST_ON_FPGA should be disabled for Silicon.
#define TEST_ON_FPGA 0

/// These defines are for Emmc operations time out.
#define SDMMC_COMMAND_TIMEOUT_IN_US 100000
#define SDMMC_READ_TIMEOUT_IN_US 200000
#define SDMMC_TIME_OUT_IN_US 100000

#if TEST_ON_FPGA
#define SDMMC_OP_COND_TIMEOUT_IN_US 100000000
#define SDMMC_PLL_FREQ_IN_MHZ 13
#else
#define SDMMC_OP_COND_TIMEOUT_IN_US 1000000
#define SDMMC_PLL_FREQ_IN_MHZ 216 // from 432
#endif

/// The following divider limits are based on the requirement that
/// SD Host controler clock should be in between 10MHz to 63MHz.
/// Max valid clock divider for PLLP clock source(216MHz).
#define SDMMC_MAX_CLOCK_DIVIDER_SUPPORTED 21
/// Min valid clock divider for PLLP clock source(216MHz).
#define SDMMC_MIN_CLOCK_DIVIDER_SUPPORTED 1
/// Max Power class supported.by card.
#define SDMMC_MAX_POWER_CLASS_SUPPORTED 16

/**
 * Check the status of read operation that is launched with
 *  API NvBootSdmmcReadPage, if it is pending.
 *
 * @retval NvBootDeviceStatus_ReadInProgress - Reading is in progress.
 * @retval NvBootDeviceStatus_CrcFailure - Data received is corrupted. Client
 *          should try to read again and should be upto 3 times.
 * @retval NvBootDeviceStatus_DataTimeout Data is not received from device.
 * @retval NvBootDeviceStatus_ReadFailure - Read operation failed.
 * @retval NvBootDeviceStatus_Idle - Read operation is complete and successful.
 */
NvBootDeviceStatus NvNvBlSdmmcQueryStatus(void);

/**
 * Shutdowns device and cleanup the state.
 *
 */
void NvNvBlSdmmcShutdown(void);

NvError HwSdmmcSetCardClock(NvBootSdmmcCardClock ClockRate);
void HwSdmmcSetDataWidth(NvBootSdmmcDataWidth DataWidth);
NvError HwSdmmcInitController(void);
NvError
HwSdmmcSendCommand(
    SdmmcCommand CommandIndex,
    NvU32 CommandArg,
    SdmmcResponseType ResponseType,
    NvBool IsDataCmd);
NvError EmmcGetExtCsd(void);
void GetTransSpeed(void);

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVAVP_SDMMC_HEADERS_T30_H */

