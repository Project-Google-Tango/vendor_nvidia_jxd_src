/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file
 * @brief <b>nVIDIA Driver Development Kit:
 *           Private functions for the SNOR device interface</b>
 *
 * @b Description:  Implements  the private interfacing functions for the SNOR
 * hw interface.
 *
 */

#include "nvrm_drf.h"
#include "nvrm_hardware_access.h"
#include "nvrm_power.h"
#include "nvassert.h"
#include "snor_priv_driver.h"
#include "amd_commandset.h"
#include "intel_commandset.h"
#include "nvodm_query.h"

#define SNOR_RESET_TIME_MS 1

#define SNOR_PRINTS(fmt,args...) NvOsDebugPrintf(fmt, ##args)

/* NVTODO : Only AMD/INTEL support for now */
static NvU16 s_ValidCmdId[] = {
                                SnorCmdID_IntelE,
                                SnorCmdID_AMD,
                                SnorCmdID_Intel,
                                SnorCmdID_IntelP
                               };
/**
 * Initialize the device interface.
 */
static void InitializeDeviceInterface(SnorDeviceIntRegister *pDevIntRegs, SnorDeviceInfo *pDevInfo, NvRmFreqKHz ClockFreq)
{
    NvU16 CmdId;
    NvU16 RCRConfig;

    /* NVTODO : We will add support for AMD and Intel
     * devices now. Later we need to check all
     * the devices supported
     */

    // Reset the Device to get it back into read array mode
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_AMD_Read_Array);
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_Intel_Read_Array);

    /* Read CFI is universal command, use Read_CFI from any command set */
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x55, SnorCmd_Intel_Read_CFI);

    /* Read Primary Vendor Command Set ID */
    CmdId = SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,0x13) & 0xFF;
    CmdId |= ((SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,0x14) & 0xFF) << 8);

    /* Leave CFI mode, return to Read Array Mode */
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_AMD_Read_Array);
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_Intel_Read_Array);

    switch(CmdId)
    {
        case SnorCmdID_AMD:
            break;

        case SnorCmdID_Intel:
        case SnorCmdID_IntelE:
        case SnorCmdID_IntelP:
            // Unlock all sectors
            IntelUnprotectAllSectors(pDevIntRegs,pDevInfo);
            if (pDevInfo->ReadMode == SnorReadMode_Burst){
                RCRConfig = 0xBFCF; // (micron (intel) default)
                RCRConfig = (RCRConfig & SYNC_MODE_MASK) | SYNC_BURST_MODE; // [15] = 0b;

                // Pick Frequency
                switch(ClockFreq)
                {
                    case 200000:
                    case 150000:
                        RCRConfig = (RCRConfig & LATENCY_MASK) | LATENCY_COUNT_133_3; // ([14:11] = 1101b)
                        break;

                    case 100000:
                        RCRConfig = (RCRConfig & LATENCY_MASK) | LATENCY_COUNT_108_7; // ([14:11] = 1010b)
                        break;

                    case 86000:
                    default:
                        RCRConfig = (RCRConfig & LATENCY_MASK) | LATENCY_COUNT_87_0; // ([14:11] = 1000b)
                        break;
                }
                RCRConfig = (RCRConfig & BURST_LENGTH_MASK) | BURST_LENGTH_CNTBURST; // ([2:0] = 111b)

                SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,RCRConfig, SnorCmd_Intel_Program_Config_Reg_Setup);
                SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,RCRConfig << 1, SnorCmd_Intel_Program_Read_Config_Reg);
            }
            break;

        default:
            break;

    }
}


/**
 * Find whether the device is available on this interface or not.
 */
static NvU32
ResetDevice(
    SnorDeviceIntRegister *pDevIntRegs,
    NvU32 DeviceChipSelect)
{
    // SNOR left in Read Array Mode after Hard Reset, Soft Reset is simply
    // turning on Read Array Mode
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_AMD_Read_Array);
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_Intel_Read_Array);
    // Return maximum command complete time.
    return SNOR_RESET_TIME_MS;
}

static NvBool IsDeviceReady(SnorDeviceIntRegister *pDevIntRegs)
{
    // No such status needed for SNOR
    return NV_TRUE;
}


/**
 * Get the device information.
 */
static NvBool
GetDeviceInfo(
    SnorDeviceIntRegister *pDevIntRegs,
    SnorDeviceInfo *pDevInfo,
    NvU32 DeviceChipSelect)
{
    NvU16 CmdId;
    NvU16 BusWidth;
    NvU16 EraseSize;
    NvU32 Size ;
    NvU32 TotalValidCmdId = NV_ARRAY_SIZE(s_ValidCmdId);
    NvU32 Index;
    NvBool IsValidDevice = NV_FALSE;
    SnorTimings Timings;

    /* NVTODO : We will add support for AMD and Intel
     * device now. Later we need to check all
     * the devices supported
     */

    // Reset the Device to get it back into read array mode
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_AMD_Read_Array);
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_Intel_Read_Array);

    /* Read CFI is universal command, use Read_CFI from any command set */
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x55, SnorCmd_Intel_Read_CFI);

    /* Read Primary Vendor Command Set ID */
    CmdId = SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,0x13) & 0xFF;
    CmdId |= ((SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,0x14) & 0xFF) << 8);

    /* Read Device Size */
    Size = SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,0x27);

    /* Read Bus Width */
    BusWidth = SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,0x28) & 0xFF;
    BusWidth |= ((SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,0x28) & 0xFF) << 8);

    /* Read Erase Size */
    EraseSize = SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,0x29) & 0xFF;
    EraseSize |= ((SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,0x30) & 0xFF) << 8);

    /* Leave CFI mode, return to Read Array Mode */
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_AMD_Read_Array);
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_Intel_Read_Array);

    /* Is it a valid device? */
    for (Index = 0; Index < TotalValidCmdId; ++Index)
    {
        if (s_ValidCmdId[Index] == CmdId)
        {
            IsValidDevice = NV_TRUE;
            break;
        }
    }

    if (!IsValidDevice)
        return NV_FALSE;

    /* NVNOTE : PagesPerBlock, PageSize are hardcoded due to BootRom
     * assuming a block size of 32kb and page size of 2kb.
     */
    /* Set device values */
    pDevInfo->CmdId = CmdId;
    pDevInfo->EraseSize = EraseSize * 256;
    pDevInfo->TotalBlocks = ((1 << Size)/pDevInfo->EraseSize);
    pDevInfo->PagesPerBlock = 16;
    pDevInfo->PageSize = 2048;
    pDevInfo->BusWidth = (BusWidth + 1) * 8;
    /* Set device specific values */
    switch(CmdId)
    {
        case SnorCmdID_AMD:
            pDevInfo->ReadMode = SnorReadMode_Page;
            NvOdmQueryGetSnorTimings(&Timings, NOR_READ_TIMING);
            pDevInfo->Timing0 = Timings.timing0;
            pDevInfo->Timing1 = Timings.timing1;
            return NV_TRUE;

        // NVTODO :  Intel specs must be changed to match intel devices
        case SnorCmdID_Intel:
        case SnorCmdID_IntelE:
        case SnorCmdID_IntelP:
            pDevInfo->ReadMode = SnorReadMode_Async;
            pDevInfo->Timing0 = INTEL_ASYNC_TIMING0;
            pDevInfo->Timing1 = INTEL_ASYNC_TIMING1;
            return NV_TRUE;

        default:
            break;
    }
    return NV_FALSE;
}


static NvBool
ConfigureHostInterface(
    SnorDeviceIntRegister *pDevIntRegs,
    NvBool IsAsynch)
{
    return NV_TRUE;
}

static SnorInterruptReason
GetInterruptReason(
    SnorDeviceIntRegister *pDevIntRegs)
{
    return SnorInterruptReason_None;
}

/**
 * Find whether the device is available on this interface or not.
 */
NvBool
IsSnorDeviceAvailable(
    SnorDeviceIntRegister *pDevIntRegs,
    NvU32 DeviceChipSelect)
{
    NvU16 CmdId=0;
    NvU32 TotalValidCmdId = NV_ARRAY_SIZE(s_ValidCmdId);
    NvU32 Index;
    NvBool IsValidDevice = NV_FALSE;

    /* NVTODO : We will add support for AMD and Intel
     * device now. Later we need to check all
     * the devices supported
     */

    // Reset the Device to get it back into read array mode
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_AMD_Read_Array);
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_Intel_Read_Array);

    /* Read CFI is universal command, use Read_CFI from any command set */
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x55, SnorCmd_Intel_Read_CFI);

    /* Read Primary Vendor Command Set ID */
    CmdId = SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,0x13) & 0xFF;
    CmdId |= ((SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,0x14) & 0xFF) << 8);

    /* Leave CFI mode, return to Read Array Mode */
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_AMD_Read_Array);
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_Intel_Read_Array);
    /* Is it a valid device? */
    for (Index = 0; Index < TotalValidCmdId; ++Index)
    {
        if (s_ValidCmdId[Index] == CmdId)
        {
            IsValidDevice = NV_TRUE;
            break;
        }
    }
    return IsValidDevice;
}

SnorDeviceType GetSnorDeviceType(SnorDeviceIntRegister *pDevIntRegs)
{
    NvU16 CmdId;

    /* NVTODO : We will add support for AMD and Intel
     * devices now. Later we need to check all
     * the devices supported
     */

    // Reset the Device to get it back into read array mode
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_AMD_Read_Array);
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_Intel_Read_Array);

    /* Read CFI is universal command, use Read_CFI from any command set */
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x55, SnorCmd_Intel_Read_CFI);

    /* Read Primary Vendor Command Set ID */
    CmdId = SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,0x13) & 0xFF;
    CmdId |= ((SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,0x14) & 0xFF) << 8);

    /* Leave CFI mode, return to Read Array Mode */
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_AMD_Read_Array);
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_Intel_Read_Array);

    switch(CmdId)
    {
        case SnorCmdID_AMD:
            return SnorDeviceType_x16_NonMuxed;

        case SnorCmdID_Intel:
        case SnorCmdID_IntelE:
        case SnorCmdID_IntelP:
            return SnorDeviceType_x16_Muxed;

        default:
            break;

    }
    return SnorDeviceType_Unknown;
}


/**
 * Initialize the snor device interface.
 */


void
NvRmPrivSnorDeviceInterface(
    SnorDevInterface *pDevInterface,
    SnorDeviceIntRegister *pDevIntRegs,
    SnorDeviceInfo *pDevInfo)
{
    NvU16 CmdId;

    // This can be made as generic CFI Query
    pDevInterface->DevInitializeDeviceInterface = InitializeDeviceInterface;
    pDevInterface->DevResetDevice = ResetDevice;
    pDevInterface->DevIsDeviceReady = IsDeviceReady;
    pDevInterface->DevGetDeviceInfo = GetDeviceInfo;
    pDevInterface->DevConfigureHostInterface = ConfigureHostInterface;
    pDevInterface->DevGetInterruptReason = GetInterruptReason;

    /* NVTODO : We will add support for AMD and Intel
     * device now. Later we need to check all
     * the devices supported
     */

    // Reset the Device to get it back into read array mode
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_AMD_Read_Array);
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_Intel_Read_Array);

    /* Read CFI is universal command, use Read_CFI from any command set */
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x55, SnorCmd_Intel_Read_CFI);

    /* Read Primary Vendor Command Set ID */
    CmdId = SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,0x13) & 0xFF;
    CmdId |= ((SNOR_FLASH_READ16(pDevIntRegs->pNorBaseAdd,0x14) & 0xFF) << 8);

    /* Leave CFI mode, return to Read Array Mode */
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_AMD_Read_Array);
    SNOR_FLASH_WRITE16(pDevIntRegs->pNorBaseAdd,0x00, SnorCmd_Intel_Read_Array);

    switch(CmdId)
    {
        case SnorCmdID_AMD:
            pDevInterface->DevProgram = AMDProgram;
            pDevInterface->DevFormatDevice = AMDFormatDevice;
            pDevInterface->DevEraseSector = AMDEraseSector;
            pDevInterface->DevProtectSectors = AMDProtectSectors;
            pDevInterface->DevUnprotectAllSectors = AMDUnprotectAllSectors;
            pDevInterface->DevReadMode = AMDReadMode ;
            break;

        case SnorCmdID_Intel:
        case SnorCmdID_IntelE:
        case SnorCmdID_IntelP:
            pDevInterface->DevProgram = IntelProgram;
            pDevInterface->DevFormatDevice = IntelFormatDevice;
            pDevInterface->DevEraseSector = IntelEraseSector;
            pDevInterface->DevProtectSectors = IntelProtectSectors;
            pDevInterface->DevUnprotectAllSectors = IntelUnprotectAllSectors;
            pDevInterface->DevReadMode = IntelReadMode ;
            break;

        default:
            break;
    }
}

