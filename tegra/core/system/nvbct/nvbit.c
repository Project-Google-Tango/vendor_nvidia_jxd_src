/*
 * Copyright (c) 2008 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvbit.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvbit_private.h"
#include "nvrm_drf.h"
#include "arapb_misc.h"
#include "nvboot_bct.h"  // needed for the NvBootDevType enumerant
#include "nvboot_bit.h"  // needed for the NvBootRdrStatus enumerant

#if NVODM_BOARD_IS_SIMULATION
    NvU32 nvflash_get_devid( void );
#endif

#define NV_BIT_APB_MISC_BASE 0x70000000UL

// Forward declarations
static NvError GetDataSize(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *NumInstances);


NvU32 NvBitPrivGetChipId(void);
NvU32 NvBitPrivGetChipId(void)
{
    NvU32 ChipId = 0;
#if NVODM_BOARD_IS_SIMULATION
    ChipId = nvflash_get_devid();
#else
    volatile NvU8 *pApbMisc = NULL;
    NvError e;
    NV_CHECK_ERROR_CLEANUP(
        NvOsPhysicalMemMap(NV_BIT_APB_MISC_BASE, 4096,
            NvOsMemAttribute_Uncached, NVOS_MEM_READ_WRITE, (void**)&pApbMisc)
    );
    ChipId = *(volatile NvU32*)(pApbMisc + APB_MISC_GP_HIDREV_0);
    ChipId = NV_DRF_VAL(APB_MISC_GP, HIDREV, CHIPID, ChipId);

 fail:
#endif
    return ChipId;
}

NvError NvBitInit(NvBitHandle *phBit)
{
    NvU32 ChipId;
    NvError e;

    if (!phBit)
        return NvError_InvalidAddress;

    ChipId = NvBitPrivGetChipId();
    switch (ChipId)
    {
#ifdef ENABLE_T30
        case 0x30:
            e = NvBitInitT30(phBit);
            break;
#endif
#ifdef ENABLE_T114
        case 0x35:
            e = NvBitInitT11x(phBit);
            break;
#endif
#ifdef ENABLE_T124
        case 0x40:
            e = NvBitInitT12x(phBit);
	    break;
#endif
        default:
            return NvError_NotSupported;
    }
    // If there is no BIT ...
    if (e == NvError_BadParameter)
    {
        // ... we may be using the backdoor loader perhaps.
        e = NvError_NotImplemented;
    }

    return e;
}

void NvBitDeinit(
    NvBitHandle hBit)
{
    if (!hBit)
        return;
#if NVODM_BOARD_IS_SIMULATION
    NvOsFree(hBit);
#endif
    hBit = NULL;
}

NvError NvBitGetData(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data)
{
    NvU32 ChipId;
    NvU32 MinDataSize = 0;
    NvU32 NumInstances = 0;
    NvError e;

    NV_ASSERT(hBit);

    if (!hBit)
        return NvError_BadParameter;

    if (!Size || !Instance)
        return NvError_InvalidAddress;


    NV_CHECK_ERROR(GetDataSize(hBit, DataType, &MinDataSize, &NumInstances));

    if (*Size == 0)
    {
        *Size = MinDataSize;
        *Instance = NumInstances;

        if (Data)
            return NvError_InsufficientMemory;
        else
            return NvSuccess;
    }

    *Size = MinDataSize;

    if (*Instance > NumInstances)
        return NvError_BadParameter;

    if (!Data)
        return NvError_InvalidAddress;

    ChipId = NvBitPrivGetChipId();

    switch(ChipId)
    {
#ifdef ENABLE_T30
        case 0x30:
            return NvBitGetDataT30(hBit, DataType, Size, Instance, Data);
#endif
#ifdef ENABLE_T114
        case 0x35:
            return NvBitGetDataT11x(hBit, DataType, Size, Instance, Data);
#endif
#ifdef ENABLE_T124
        case 0x40:
            return NvBitGetDataT12x(hBit, DataType, Size, Instance, Data);
#endif
        default:
            return NvError_NotSupported;
    }
}

NvBitSecondaryBootDevice NvBitGetBootDevTypeFromBitData(NvU32 BitDataDevice)
{
    NvU32 ChipId;
    ChipId = NvBitPrivGetChipId();

    switch(ChipId)
    {
#ifdef ENABLE_T30
       case  0x30:
         return NvBitPrivGetBootDevTypeFromBitDataT30(BitDataDevice);
#endif
#ifdef ENABLE_T114
       case  0x35:
         return NvBitPrivGetBootDevTypeFromBitDataT11x(BitDataDevice);
#endif
#ifdef ENABLE_T124
       case  0x40:
         return NvBitPrivGetBootDevTypeFromBitDataT12x(BitDataDevice);
#endif
       default:
          return NvError_NotSupported;
    }
}

NvBitRdrStatus NvBitGetBitStatusFromBootRdrStatus(NvU32 BootRdrStatus)
{
    NvBootRdrStatus BootStatus = (NvBootRdrStatus)BootRdrStatus;

    switch (BootStatus)
    {
    case NvBootRdrStatus_Success:
        return NvBitStatus_Success;
    case NvBootRdrStatus_ValidationFailure:
        return NvBitStatus_ValidationFailure;
    case NvBootRdrStatus_DeviceReadError:
        return NvBitStatus_DeviceReadError;
    default:
        return NvBitStatus_None;
    }
}
static NvError GetDataSize(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *NumInstances)
{
    NV_ASSERT(hBit);
    NV_ASSERT(Size);
    NV_ASSERT(NumInstances);

    switch(DataType)
    {
        case NvBitDataType_BootRomVersion:
        case NvBitDataType_BootDataVersion:
        case NvBitDataType_RcmVersion:
        case NvBitDataType_BootType:
        case NvBitDataType_PrimaryDevice:
        case NvBitDataType_SecondaryDevice:
        case NvBitDataType_OscFrequency:
        case NvBitDataType_ActiveBctBlock:
        case NvBitDataType_ActiveBctSector:
        case NvBitDataType_BctSize:
        case NvBitDataType_ActiveBctPtr:
        case NvBitDataType_SafeStartAddr:
            *Size = sizeof(NvU32);
            *NumInstances = 1;
            break;

        case NvBitDataType_IsValidBct:
            *Size = sizeof(NvBool);
            *NumInstances = 1;
            break;


        default:
        {
            NvU32 ChipId = NvBitPrivGetChipId();
            NvError e = NvSuccess;

            switch(ChipId)
            {
#ifdef ENABLE_T30
                case 0x30:
                    e = NvBitPrivGetDataSizeT30(hBit, DataType, Size, NumInstances);
                    break;
#endif
#ifdef ENABLE_T114
                case 0x35:
                    e = NvBitPrivGetDataSizeT11x(hBit, DataType, Size, NumInstances);
                    break;
#endif
#ifdef ENABLE_T124
                case 0x40:
                    e = NvBitPrivGetDataSizeT12x(hBit, DataType, Size, NumInstances);
		    break;
#endif
                default:
                    return NvError_NotSupported;
            }
            if (e!=NvSuccess)
                goto fail;
        }
    }
    return NvSuccess;

fail:
    return NvError_BadParameter;
}
