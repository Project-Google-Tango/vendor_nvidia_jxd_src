/*
 * Copyright (c) 2010-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "t30/nvboot_bit.h"
#include "t30/nvboot_version.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvbit_private.h"

static NvBool NvBitPrivIsValidBitT30(NvBootInfoTable *pBit)
{
    if (((pBit->BootRomVersion == NVBOOT_VERSION(3, 1))
      || (pBit->BootRomVersion == NVBOOT_VERSION(3, 2)))
    &&   (pBit->DataVersion    == NVBOOT_VERSION(3, 1))
    &&   (pBit->RcmVersion     == NVBOOT_VERSION(3, 1))
    &&   (pBit->PrimaryDevice  == NvBootDevType_Irom))
    {
        // There is a valid Boot Information Table.
        return NV_TRUE;
    }

    return NV_FALSE;
}

NvError
NvBitPrivGetDataSizeT30(
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
NvBitPrivGetDataT30(
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
    //return NvSuccess;
}

NvBitSecondaryBootDevice
NvBitPrivGetBootDevTypeFromBitDataT30(
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
    case NvBootDevType_Sata:
        return NvBitBootDevice_Sata;
    default:
        return NvBitBootDevice_None;
    }
}

#define NVBIT_SOC(FN)  FN##T30
#define NV_BIT_LOCATION 0x40000000UL
#include "nvbit_common.c"
#undef NVBIT_LOCATION
#undef NVBIT_SOC

