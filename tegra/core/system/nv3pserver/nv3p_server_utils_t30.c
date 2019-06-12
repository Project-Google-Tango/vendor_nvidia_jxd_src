/*
 * Copyright (c) 2010 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvddk_blockdevmgr.h"
#include "nv3p.h"
#include "t30/nvboot_bit.h"
#include "nv3p_server_private.h"
#include "t30/arapb_misc.h"
#include "nvrm_drf.h"
#include "nvrm_hardware_access.h"

#define MISC_PA_BASE 0x70000000UL
#define MISC_PA_LEN  4096

#if NVODM_BOARD_IS_SIMULATION
    NvU32 nvflash_get_devid( void );
#endif
//proto types
static NvError
T30ConvertBootToRmDeviceType(
    NvBootDevType bootDevId,
    NvDdkBlockDevMgrDeviceId *blockDevId);
static NvError
T30Convert3pToRmDeviceType(
    Nv3pDeviceType DevNv3p,
    NvRmModuleID *pDevNvRm);
static NvBool
T30ValidateRmDevice(
     NvBootDevType BootDevice,
     NvRmModuleID RmDeviceId);
static void
T30UpdateBct(
    NvU8* data,
    NvU32 BctSection);



//Fixme: Added T30 supported devices to NvRmModuleID enum and extend this API to
//support devices on T30
NvError
T30ConvertBootToRmDeviceType(
    NvBootDevType bootDevId,
    NvDdkBlockDevMgrDeviceId *blockDevId)
{
    switch (bootDevId)
    {
    case NvBootDevType_Nand:
    case NvBootDevType_Nand_x16:
            *blockDevId = NvDdkBlockDevMgrDeviceId_Nand;
            break;
    case NvBootDevType_Sdmmc:
            *blockDevId = NvDdkBlockDevMgrDeviceId_SDMMC;
            break;
    case NvBootDevType_Spi:
            *blockDevId = NvDdkBlockDevMgrDeviceId_Spi;
            break;
    case NvBootDevType_Nor:
             *blockDevId = NvDdkBlockDevMgrDeviceId_Nor;
             break ;
        default:
            *blockDevId = NvDdkBlockDevMgrDeviceId_Invalid;
            break;
    }

    return NvSuccess;
}

/* ------------------------------------------------------------------------ */
//Fixme: Added T30 supported devices to NvRmModuleID enum and extend this API to
//support devices on T30
NvError
T30Convert3pToRmDeviceType(
    Nv3pDeviceType DevNv3p,
    NvRmModuleID *pDevNvRm)
{
    switch (DevNv3p)
    {
        case Nv3pDeviceType_Nand:
            *pDevNvRm = NvRmModuleID_Nand;
            break;
        case Nv3pDeviceType_Emmc:
            *pDevNvRm = NvRmModuleID_Sdio;
            break;
        case Nv3pDeviceType_Spi:
            *pDevNvRm = NvRmModuleID_Spi;
            break;
        case Nv3pDeviceType_Snor:
            *pDevNvRm = NvRmModuleID_SyncNor;
            break;
        default:
            return NvError_BadParameter;
    }
    return NvSuccess;
}
NvBool T30ValidateRmDevice(NvBootDevType BootDevice, NvRmModuleID RmDeviceId)
{
        NvBool IsValidDevicePresent = NV_TRUE;
        switch (BootDevice)
        {
            case NvBootDevType_None:
                // boot device fuses are not burned, therefore no check
                // is possible, so just fall through
                break;
            case NvBootDevType_Nand:
            case NvBootDevType_Nand_x16:
               if (RmDeviceId != NvRmModuleID_Nand)
                    IsValidDevicePresent = NV_FALSE;
                break;
            case NvBootDevType_Sdmmc:
               if (RmDeviceId != NvRmModuleID_Sdio)
                    IsValidDevicePresent = NV_FALSE;
                break;
            case NvBootDevType_Spi:
               if (RmDeviceId != NvRmModuleID_Spi)
                    IsValidDevicePresent = NV_FALSE;
                break;
            case NvBootDevType_Nor:
                if (RmDeviceId != NvRmModuleID_SyncNor)
                    IsValidDevicePresent = NV_FALSE;
                break;
            default:
                    IsValidDevicePresent = NV_FALSE;
                break;
        }

        return IsValidDevicePresent;
    }

void T30UpdateBct(NvU8* data, NvU32 BctSection)
{
    NvBootInfoTable *pBitTable;
    NvBootConfigTable *bct = 0;
    NvBootConfigTable *UpdateBct = (NvBootConfigTable*)data;
#define NV_BIT_LOCATION 0x40000000UL
    pBitTable = (NvBootInfoTable *)NV_BIT_LOCATION;
#undef NVBIT_LOCATION
    //If a valid BCT is already present, just overwrite it
    if (pBitTable->BctValid == NV_TRUE)
    {
        bct = pBitTable->BctPtr;
    }
    else
    {
        bct = (NvBootConfigTable*) pBitTable->SafeStartAddr;
    }
    switch(BctSection)
    {
        case Nv3pUpdatebctSectionType_Sdram:
            NvOsMemcpy(&bct->NumSdramSets,
                        &UpdateBct->NumSdramSets,
                        sizeof(NvU32));
            NvOsMemcpy(bct->SdramParams,
                        UpdateBct->SdramParams,
                        sizeof(NvBootSdramParams) * NVBOOT_BCT_MAX_SDRAM_SETS);
            break;
        case Nv3pUpdatebctSectionType_DevParam:
            NvOsMemcpy(&bct->NumParamSets,
                        &UpdateBct->NumParamSets,
                        sizeof(NvU32));
            NvOsMemcpy(bct->DevType,
                        UpdateBct->DevType,
                        sizeof(NvBootDevType) * NVBOOT_BCT_MAX_PARAM_SETS);
            NvOsMemcpy(bct->DevParams,
                        UpdateBct->DevParams,
                        sizeof(NvBootDevParams) * NVBOOT_BCT_MAX_PARAM_SETS);
            break;
        case Nv3pUpdatebctSectionType_BootDevInfo:
            NvOsMemcpy(&bct->BlockSizeLog2,
                        &UpdateBct->BlockSizeLog2,
                        sizeof(NvU32));
            NvOsMemcpy(&bct->PageSizeLog2,
                        &UpdateBct->PageSizeLog2,
                        sizeof(NvU32));
            NvOsMemcpy(&bct->PartitionSize,
                        &UpdateBct->PartitionSize,
                        sizeof(NvU32));
            break;
    }
}

NvBool Nv3pServerUtilsGetT30Hal(Nv3pServerUtilsHal *pHal)
{
    NvU32 ChipId;
#if NVODM_BOARD_IS_SIMULATION
    ChipId = nvflash_get_devid();
#else
    NvU8 *pMisc;
    NvU32 RegData;
    NvError e;

    NV_CHECK_ERROR_CLEANUP(
        NvOsPhysicalMemMap(MISC_PA_BASE, MISC_PA_LEN,
            NvOsMemAttribute_Uncached, NVOS_MEM_READ_WRITE, (void **)&pMisc)
    );

    RegData = NV_READ32(pMisc + APB_MISC_GP_HIDREV_0);

    NvOsPhysicalMemUnmap(pMisc, MISC_PA_LEN);

    ChipId = NV_DRF_VAL(APB_MISC_GP, HIDREV, CHIPID, RegData);
#endif
    if (ChipId == 0x30)
    {
        pHal->ConvertBootToRmDeviceType = T30ConvertBootToRmDeviceType;
        pHal->Convert3pToRmDeviceType = T30Convert3pToRmDeviceType;
        pHal->ValidateRmDevice = T30ValidateRmDevice;
        pHal->UpdateBct = T30UpdateBct;
        pHal->UpdateCustDataBct = NULL;
        return NV_TRUE;
    }

 fail:
    return NV_FALSE;
}


