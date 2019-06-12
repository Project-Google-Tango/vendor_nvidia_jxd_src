/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvassert.h"
#include "nvrm_drf.h"
#include "nvrm_hardware_access.h"
#include "nvddk_operatingmodes.h"
#include "nvddk_fuse.h"
#include "arfuse.h"
#include "nvddk_fuse_common.h"
#include "nvddk_fuse_read.h"

static NvError NvDdkFuseGetFuseTypeSize(NvDdkFuseDataType Type,
                                                  NvU32 *pSize, NvU32 ChipId)
{
    if (!pSize)
        return NvError_InvalidAddress;

    switch (Type)
    {
        case NvDdkFuseDataType_DeviceKey:
            *pSize = sizeof(NvU32);
             break;

        case NvDdkFuseDataType_JtagDisable:
            *pSize = sizeof(NvBool);
             break;

        case NvDdkFuseDataType_KeyProgrammed:
            *pSize = sizeof(NvBool);
             break;

        case NvDdkFuseDataType_OdmProduction:
            *pSize = sizeof(NvBool);
             break;

        case  NvDdkFuseDataType_SecBootDeviceConfig:
            *pSize = sizeof(NvU32);
             break;

        case NvDdkFuseDataType_Sku:
            *pSize = sizeof(NvU32);
             break;

        case NvDdkFuseDataType_SwReserved:
            *pSize = sizeof(NvU32);
             break;

        case NvDdkFuseDataType_SkipDevSelStraps:
            *pSize = sizeof(NvBool);
             break;

        case NvDdkFuseDataType_PackageInfo:
            *pSize = sizeof(NvU32);
             break;

        case NvDdkFuseDataType_SecBootDeviceSelect:
            *pSize = sizeof(NvU32);
             break;

        case NvDdkFuseDataType_SecBootDeviceSelectRaw:
            *pSize = sizeof(NvU32);
             break;

        case NvDdkFuseDataType_OpMode:
            *pSize = sizeof(NvU32);
             break;

        case NvDdkFuseDataType_SecBootDevInst:
            *pSize = sizeof(NvU32);
             break;

        case NvDdkFuseDataType_WatchdogEnabled:
            *pSize = sizeof(NvBool);
             break;

        case NvDdkFuseDataType_UsbChargerDetectionEnabled:
            *pSize = sizeof(NvBool);
             break;

        case NvDdkFuseDataType_PkcDisable:
            *pSize = sizeof(NvBool);
             break;

        case NvDdkFuseDataType_Vp8Enable:
            *pSize = sizeof(NvBool);
             break;

        case NvDdkFuseDataType_OdmLock:
            *pSize = sizeof(NvU32);
             break;

        case NvDdkFuseDataType_GetAge:
            *pSize = sizeof(NvU32);
             break;

        default:
        {
#ifdef ENABLE_T30
            if (ChipId == 0x30)
                return NvDdkFusePrivGetTypeSizeT30(Type, pSize);
#endif
#ifdef ENABLE_T114
            if (ChipId == 0x35)
                return NvDdkFusePrivGetTypeSizeT11x(Type, pSize);
#endif
#ifdef ENABLE_T148
            if (ChipId == 0x14)
                return NvDdkFusePrivGetTypeSizeT14x(Type, pSize);
#endif
#ifdef ENABLE_T124
            if (ChipId == 0x40)
                return NvDdkFusePrivGetTypeSizeT12x(Type, pSize);
#endif
            else
                return NvError_NotSupported;
        }
    }

    return NvSuccess;
}


/**
 * This gets value from the fuse cache.
 *
 * To read from the actual fuses, NvDdkFuseSense() must be called first.
 * Note that NvDdkFuseSet() follwed by NvDdkFuseGet() for the same data will
 * return the set data, not the actual fuse values.
 *
 * By passing a size of zero, the caller is requesting tfor the
 * expected size.
 */
NvError NvDdkFuseGet(NvDdkFuseDataType Type, void *pData, NvU32 *pSize)
{
    NvU32 RegData;
    NvU32 Size;
    NvU32 ChipId;
    NvError e = NvSuccess;

   if (!pSize)
       return NvError_InvalidAddress;

   if (Type == NvDdkFuseDataType_None )
            return NvError_BadValue;

    // Get Chip Id
    RegData = NV_READ32(AR_APB_MISC_BASE_ADDRESS + APB_MISC_GP_HIDREV_0);
    ChipId = NV_DRF_VAL(APB_MISC, GP_HIDREV, CHIPID, RegData);

    // Get min size of the Fuse type
    NV_CHECK_ERROR(NvDdkFuseGetFuseTypeSize(Type, &Size, ChipId));

    // Error checks
    if (*pSize == 0)
    {
        *pSize = Size;
        return NvError_Success;
    }
    if (*pSize < Size) return NvError_InsufficientMemory;
    if (pData == NULL) return NvError_BadParameter;

    switch (Type)
    {
        case NvDdkFuseDataType_DeviceKey:
            /*
             * Boot ROM expects DK to be stored in big-endian byte order;
             * since cpu is little-endian and client treats data as an NvU32,
             * perform byte swapping here
             */
            RegData = FUSE_NV_READ32(FUSE_PRIVATE_KEY4_0);
            SWAP_BYTES_NVU32(RegData);
            *((NvU32*)pData) = RegData;
            break;

        case NvDdkFuseDataType_JtagDisable:
            RegData = FUSE_NV_READ32(FUSE_ARM_DEBUG_DIS_0);
            *((NvBool*)pData) = RegData ? NV_TRUE : NV_FALSE;
            break;

        case NvDdkFuseDataType_KeyProgrammed:
            *((NvBool*)pData) = IsSbkOrDkSet();
            break;

        case NvDdkFuseDataType_OdmProduction:
            *((NvBool*)pData) = NvDdkFuseIsOdmProductionModeFuseSet();
            break;

        case NvDdkFuseDataType_SwReserved:
            RegData = FUSE_NV_READ32(FUSE_RESERVED_SW_0);
            RegData = NV_DRF_VAL(FUSE, RESERVED_SW, SW_RESERVED, RegData);
            *((NvU32*)pData) = RegData;
            break;

        case NvDdkFuseDataType_SkipDevSelStraps:
            RegData = FUSE_NV_READ32(FUSE_RESERVED_SW_0);
            RegData = NV_DRF_VAL(FUSE, RESERVED_SW, SKIP_DEV_SEL_STRAPS, RegData);
            *((NvBool*)pData) = RegData;
            break;

        case NvDdkFuseDataType_Sku:
            RegData = FUSE_NV_READ32(FUSE_SKU_INFO_0);
            RegData = NV_DRF_VAL(FUSE, SKU_INFO, SKU_INFO, RegData);
            *((NvU32*)pData) = RegData;
            break;

        case  NvDdkFuseDataType_SecBootDeviceConfig:
            *((NvU32*)pData) = NvDdkFuseGetBootDevInfo();
            break;

        default:
#ifdef ENABLE_T30
            if (ChipId == 0x30)
                return NvDdkFusePrivGetDataT30(Type, pData, &Size);
#endif
#ifdef ENABLE_T114
            if (ChipId == 0x35)
                return NvDdkFusePrivGetDataT11x(Type, pData, &Size);
#endif
#ifdef ENABLE_T148
            if (ChipId == 0x14)
                return NvDdkFusePrivGetDataT14x(Type, pData, &Size);
#endif
#ifdef ENABLE_T124
            if (ChipId == 0x40)
               return NvDdkFusePrivGetDataT12x(Type, pData, &Size);
#endif
            else
                return NvError_NotSupported;
        }

    return NvSuccess;
}
