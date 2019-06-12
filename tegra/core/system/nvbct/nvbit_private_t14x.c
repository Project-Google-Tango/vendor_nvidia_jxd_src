/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "t14x/nvboot_bit.h"
#include "t14x/nvboot_version.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvbit_private.h"

static NvBool NvBitPrivIsValidBitT14x(NvBootInfoTable *pBit)
{
    if (((pBit->BootRomVersion == NVBOOT_VERSION(0x14, 0x01))
        || (pBit->BootRomVersion == NVBOOT_VERSION(0x14, 0x02)))
        &&   (pBit->DataVersion    == NVBOOT_VERSION(0x14, 0x01))
        &&   (pBit->RcmVersion     == NVBOOT_VERSION(0x14, 0x01))
        &&   (pBit->PrimaryDevice  == NvBootDevType_Irom))
    {
        // There is a valid Boot Information Table.
        return NV_TRUE;
    }

    return NV_FALSE;
}

NvError
NvBitPrivGetDataSizeT14x(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *NumInstances)
{
    NV_ASSERT(hBit);
    NV_ASSERT(Size);
    NV_ASSERT(NumInstances);
    switch (DataType)
    {
        case NvBitDataType_BlStatus:
            *Size = sizeof(NvU32);
            *NumInstances = NVBOOT_MAX_BOOTLOADERS;
            break;

        case NvBitDataType_BlUsedForEccRecovery:
            *Size = sizeof(NvU32);
            *NumInstances = NVBOOT_MAX_BOOTLOADERS;
            break;

        default:
            return NvError_NotImplemented;
    }
    return NvSuccess;
}

static NvError
NvBitPrivGetDataT14x(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data)
{
    if (!hBit)
        return NvError_BadParameter;

    if (!Size || !Instance)
        return NvError_InvalidAddress;

    if (!Data)
        return NvError_InvalidAddress;

    switch(DataType)
    {
        default:
            return NvError_NotImplemented;
    }
}

NvBitSecondaryBootDevice
NvBitPrivGetBootDevTypeFromBitDataT14x(
    NvU32 BitDataDevice)
{
    NvBootDevType Boot = (NvBootDevType)BitDataDevice;
    switch (Boot)
    {
    case NvBootDevType_Nand_x8:
        return NvBitBootDevice_Nand8;
    case NvBootDevType_Nand_x16:
        return NvBitBootDevice_Nand16;
    case NvBootDevType_Sdmmc:
        return NvBitBootDevice_Emmc;
    case NvBootDevType_Spi:
        return NvBitBootDevice_SpiNor;
    case NvBootDevType_Nor:
        return NvBitBootDevice_Nor;
    default:
        return NvBitBootDevice_None;
    }
}

#define NVBIT_SOC(FN)  FN##T14x
#define NV_BIT_LOCATION 0x40000000UL
#include "nvbit_common.c"
#undef NVBIT_LOCATION
#undef NVBIT_SOC
