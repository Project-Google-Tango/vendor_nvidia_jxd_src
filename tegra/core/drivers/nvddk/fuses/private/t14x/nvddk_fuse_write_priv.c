/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvddk_fuse.h"
#include "nvassert.h"
#include "nvrm_drf.h"
#include "nvrm_hardware_access.h"
#include "t14x/nvboot_bct.h"
#include "nvddk_fuse_common.h"
#include "nvddk_fuse_write_priv.h"
#include "nvboot_misc_t1xx.h"
#include "nvboot_osc.h"
#include "nvrm_pmu.h"
#include "nvodm_query_discovery.h"


/* cycles corresponding to 13MHz, 16.8MHz, 19.2MHz, 38.4MHz, 12MHz, 48MHz, 26MHz */
static int fuse_pgm_cycles[] = {156, 202, 0, 0, 231, 461, 0, 0, 144, 576, 0, 0, 312};

extern NvDdkFuseData s_FuseData;
extern NvU32 s_FuseArray[FUSE_WORDS];
extern NvU32 s_MaskArray[FUSE_WORDS];

// Map NvDdkSecBootDeviceType enum onto T114 Boot Device Selection values.
static NvDdkFuseBootDevice NvDdkToFuseBootDeviceSel[] =
{
    NvDdkFuseBootDevice_Sdmmc,         // None
    NvDdkFuseBootDevice_NandFlash,     // NvDdkSecBootDeviceType_Nand
    NvDdkFuseBootDevice_SnorFlash,     // NvDdkSecBootDeviceType_Nor
    NvDdkFuseBootDevice_SpiFlash,      // NvDdkSecBootDeviceType_Spi_Flash
    NvDdkFuseBootDevice_Sdmmc,         // NvDdkSecBootDeviceType_eMMC
    NvDdkFuseBootDevice_NandFlash,     // NvDdkSecBootDeviceType_Nand_x16
    NvDdkFuseBootDevice_Usb3,          // NvDdkSecBootDeviceType_Usb3
    NvDdkFuseBootDevice_Sdmmc,         // NvDdkSecBootDeviceType_Sdmmc
    NvDdkFuseBootDevice_resvd_5,       // NvDdkSecBootDeviceType_resvd_5
    NvDdkFuseBootDevice_resvd_6,       // NvDdkSecBootDeviceType_resvd_6 (Sata)
    NvDdkFuseBootDevice_resvd_7,       // NvDdkSecBootDeviceType_resvd_7
};

static NvBootDevType MapFuseDevToBootDevType[] =
{
    NvBootDevType_Sdmmc,         // NvBootFuseBootDevice_Sdmmc
    NvBootDevType_Snor,          // NvBootFuseBootDevice_SnorFlash
    NvBootDevType_Spi,           // NvBootFuseBootDevice_SpiFlash
    NvBootDevType_Nand,          // NvBootFuseBootDevice_NandFlash
    NvBootDevType_Usb3           // NvBootFuseBootDevice_Usb3
};

NvU32 NvDdkToFuseBootDeviceSelMap(NvU32 RegData)
{
    if (RegData >= (int)((sizeof(NvDdkToFuseBootDeviceSel)) / sizeof(NvDdkFuseBootDevice)))
        return  -1;
    return NvDdkToFuseBootDeviceSel[RegData];
}

NvU32 NvFuseDevToBootDevTypeMap(NvU32 RegData)
{
    if (RegData >= (int)((sizeof(MapFuseDevToBootDevType)) / sizeof(NvBootDevType)))
        return  -1;
    return MapFuseDevToBootDevType[RegData];
}

NvError NvDdkFusePrivGetTypeSize(NvDdkFuseDataType Type,
                                 NvU32 *pSize)
{
    if (!pSize)
        return NvError_InvalidAddress;

    switch (Type)
    {
        case NvDdkFuseDataType_Uid:
            *pSize = NVDDK_UID_BYTES;
            break;

        case NvDdkFuseDataType_SecureBootKey:
            *pSize = NVDDK_SECURE_BOOT_KEY_BYTES;
            break;

        case NvDdkFuseDataType_ReservedOdm:
            *pSize = NVDDK_RESERVED_ODM_BYTES;
            break;

        case NvDdkFuseDataType_PublicKeyHash:
            *pSize = ARSE_SHA256_HASH_SIZE/8;
            break;

        case NvDdkFuseDataType_OdmInfo:
            *pSize = sizeof (NvU32);
            break;

        default:
             return NvError_NotImplemented;
    }
    return NvSuccess;
}

void NvDdkFusePrivMapDataToFuseArrays(void)
{
    NvU32 Data;
    NvU32 *Src;

    if (s_FuseData.ProgramFlags & FLAGS_PROGRAM_PKC_DISABLE)
    {
        Data = (NvU32)(s_FuseData.PkcDisable);
        SET_FUSE_BOTH(PKC_DISABLE, 0, Data);
    }

    if (s_FuseData.ProgramFlags & FLAGS_PROGRAM_VP8_ENABLE)
    {
        Data = (NvU32)(s_FuseData.Vp8Enable);
        SET_FUSE_BOTH(VP8_ENABLE, 0, Data);
    }

    if (s_FuseData.ProgramFlags & FLAGS_PROGRAM_ODM_LOCK)
    {
        Data = (NvU32)(s_FuseData.OdmLock);
        SET_FUSE_BOTH(ODM_LOCK, 0, Data);
    }

    if (s_FuseData.ProgramFlags & FLAGS_PROGRAM_PUBLIC_KEY_HASH)
    {
        Src = (NvU32*)(s_FuseData.PublicKeyHash);
        Data = Src[0];
        SET_FUSE_BOTH(PUBLIC_KEY0, 0, Data);
        UPDATE_DATA  (PUBLIC_KEY0, 0, Data);
        SET_FUSE_BOTH(PUBLIC_KEY0, 1, Data);

        Data = Src[1];
        SET_FUSE_BOTH(PUBLIC_KEY1, 0, Data);
        UPDATE_DATA  (PUBLIC_KEY1, 0, Data);
        SET_FUSE_BOTH(PUBLIC_KEY1, 1, Data);

        Data = Src[2];
        SET_FUSE_BOTH(PUBLIC_KEY2, 0, Data);
        UPDATE_DATA  (PUBLIC_KEY2, 0, Data);
        SET_FUSE_BOTH(PUBLIC_KEY2, 1, Data);

        Data = Src[3];
        SET_FUSE_BOTH(PUBLIC_KEY3, 0, Data);
        UPDATE_DATA  (PUBLIC_KEY3, 0, Data);
        SET_FUSE_BOTH(PUBLIC_KEY3, 1, Data);

        Data = Src[4];
        SET_FUSE_BOTH(PUBLIC_KEY4, 0, Data);
        UPDATE_DATA  (PUBLIC_KEY4, 0, Data);
        SET_FUSE_BOTH(PUBLIC_KEY4, 1, Data);

        Data = Src[5];
        SET_FUSE_BOTH(PUBLIC_KEY5, 0, Data);
        UPDATE_DATA  (PUBLIC_KEY5, 0, Data);
        SET_FUSE_BOTH(PUBLIC_KEY5, 1, Data);

        Data = Src[6];
        SET_FUSE_BOTH(PUBLIC_KEY6, 0, Data);
        UPDATE_DATA  (PUBLIC_KEY6, 0, Data);
        SET_FUSE_BOTH(PUBLIC_KEY6, 1, Data);

        Data = Src[7];
        SET_FUSE_BOTH(PUBLIC_KEY7, 0, Data);
        UPDATE_DATA  (PUBLIC_KEY7, 0, Data);
        SET_FUSE_BOTH(PUBLIC_KEY7, 1, Data);
    }

    if (s_FuseData.ProgramFlags & FLAGS_PROGRAM_ODM_INFO)
    {
        Data = (NvU32)(s_FuseData.OdmInfo);
        SET_FUSE_BOTH(ODM_INFO, 0, Data);
    }
}

// Start the regulator and wait
void StartRegulator(void)
{
    /* PWR_GOOD not part of programming sequence */
}

// Stop the regulator and wait
void StopRegulator(void)
{
    /* PWR_GOOD not part of programming sequence */
}

NvU32 NvDdkFuseGetFusePgmCycles(void)
{
    NvBootClocksOscFreq freq;

    freq = NvBootClocksGetOscFreqT1xx();

    return fuse_pgm_cycles[(NvU32)freq];
}

NvError NvDdkFuseSetPriv(void** pData, NvDdkFuseDataType Type,
                       NvDdkFuseData* p_FuseData, NvU32 Size)
{
    NvError e = NvError_Success;

    switch (Type)
    {
        FUSE_SET_PRIV(PkcDisable,        PKC_DISABLE             );
        FUSE_SET_PRIV(Vp8Enable,         VP8_ENABLE            );
        FUSE_SET_PRIV(OdmLock,           ODM_LOCK             );
        FUSE_SET_PRIV(PublicKeyHash,     PUBLIC_KEY_HASH             );
        FUSE_SET_PRIV(OdmInfo,           ODM_INFO              );
        default:
            return NvError_BadValue;
    }

    return e;
}

void NvDdkFuseFillChipOptions(void)
{
    /* Programming  FUSE_PRIV2INTFC_START_0 causes a hang. Refer Bug 1172524 */
    return;
}

void EnableVppFuse(void)
{
    const NvOdmPeripheralConnectivity *pConnectivity = NULL;
    NvRmPmuVddRailCapabilities RailCaps;
    NvRmDeviceHandle hRmDevice = NULL;
    NvU32 i;
    NvError err;


    err = NvRmOpen(&hRmDevice, 0);

    pConnectivity = NvOdmPeripheralGetGuid(NV_VPP_FUSE_ODM_ID);
    if (pConnectivity != NULL)
    {
        for (i = 0; i < pConnectivity->NumAddress; i++)
        {
            // Search for the vdd rail entry
            if (pConnectivity->AddressList[i].Interface == NvOdmIoModule_Vdd)
            {
                NvRmPmuGetCapabilities(hRmDevice,
                            pConnectivity->AddressList[i].Address, &RailCaps);

                NvRmPmuSetVoltage(hRmDevice,
                            pConnectivity->AddressList[i].Address, RailCaps.requestMilliVolts, NULL);
            }
        }
    }

    NvRmClose(hRmDevice);

}

void DisableVppFuse(void)
{
    const NvOdmPeripheralConnectivity *pConnectivity = NULL;
    NvRmPmuVddRailCapabilities RailCaps;
    NvRmDeviceHandle hRmDevice = NULL;
    NvU32 i;
    NvError err;


    err = NvRmOpen(&hRmDevice, 0);

    pConnectivity = NvOdmPeripheralGetGuid(NV_VPP_FUSE_ODM_ID);
    if (pConnectivity != NULL)
    {
        for (i = 0; i < pConnectivity->NumAddress; i++)
        {
            // Search for the vdd rail entry
            if (pConnectivity->AddressList[i].Interface == NvOdmIoModule_Vdd)
            {
                NvRmPmuGetCapabilities(hRmDevice,
                            pConnectivity->AddressList[i].Address, &RailCaps);

                NvRmPmuSetVoltage(hRmDevice,
                            pConnectivity->AddressList[i].Address, 0, NULL);
            }
        }
    }

    NvRmClose(hRmDevice);

}
