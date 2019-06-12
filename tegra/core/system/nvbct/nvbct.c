/*
 * Copyright (c) 2008-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvbct.h"
#include "nvbit.h"
#include "nvbct_customer_platform_data.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvbct_private.h"

// Forward declarations
extern NvU32 NvBitPrivGetChipId(void);

static NvError GetDataSize(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *NumInstances);

// Function definitions

NvError NvBctInit(
    NvU32 *Size,
    void *Buffer,
    NvBctHandle *phBct)
{
    NvU32 ChipId;
    NvU32 BctSize;
    NvBctHandle tempHandle = NULL;
    NvBitHandle hBit = (NvBitHandle)NULL;
    NvError e = NvSuccess;

    ChipId = NvBitPrivGetChipId();

    switch(ChipId)
    {
#ifdef ENABLE_T30
        case 0x30:
            BctSize = NvBctGetBctSizeT30();
            break;
#endif
#ifdef ENABLE_T114
        case 0x35:
            BctSize = NvBctGetBctSizeT11x();
            break;
#endif
#ifdef ENABLE_T124
        case 0x40:
            BctSize = NvBctGetBctSizeT12x();
            break;
#endif
        default:
            return NvError_NotSupported;
    }

    if (!phBct && Size && *Size == 0)
    {
        *Size = BctSize;
        return NvSuccess;
    }

    if (!phBct)
        return NvError_InvalidAddress;

    if (Buffer && !Size)
        return NvError_InvalidAddress;

    if (Buffer && *Size < BctSize)
        return NvError_InsufficientMemory;

    if (Buffer)
    {
        tempHandle = Buffer;
    }
    else
    {
        NvU32 DataElemSize;
        NvU32 DataElemInstance;
        NvBool IsValidBct;
        NvU32 BctSize;
        NvU32 ActiveBctPtr;

        NV_CHECK_ERROR_CLEANUP(NvBitInit(&hBit));

        DataElemSize = sizeof(NvU32);
        DataElemInstance = 0;

        NV_CHECK_ERROR_CLEANUP(
            NvBitGetData(hBit, NvBitDataType_IsValidBct,
                         &DataElemSize, &DataElemInstance, &IsValidBct)
            );

        NV_ASSERT(IsValidBct == NV_TRUE);

        NV_CHECK_ERROR_CLEANUP(
            NvBitGetData(hBit, NvBitDataType_ActiveBctPtr,
                         &DataElemSize, &DataElemInstance, &ActiveBctPtr)
            );

        NV_CHECK_ERROR_CLEANUP(
            NvBitGetData(hBit, NvBitDataType_BctSize,
                         &DataElemSize, &DataElemInstance, &BctSize)
        );

        NV_CHECK_ERROR_CLEANUP(
            NvOsPhysicalMemMap(ActiveBctPtr, BctSize,
                NvOsMemAttribute_Uncached, NVOS_MEM_READ_WRITE,
                (void**)&tempHandle)
        );

        NvBitDeinit(hBit);
        hBit = NULL;
    }

    *phBct = tempHandle;

    return NvSuccess;

fail:
    NvBitDeinit(hBit);

    return e;
}


void NvBctDeinit(
    NvBctHandle hBct)
{
    if (!hBct)
        return;

    hBct = NULL;
}

NvError GetDataSize(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *NumInstances)
{
    NV_ASSERT(hBct);
    NV_ASSERT(Size);
    NV_ASSERT(NumInstances);

    switch(DataType)
    {
        case NvBctDataType_Version:
        case NvBctDataType_NumValidBootDeviceConfigs:
        case NvBctDataType_NumValidSdramConfigs:
        case NvBctDataType_NumValidDevType:
        case NvBctDataType_NumEnabledBootLoaders:
        case NvBctDataType_BootDeviceBlockSizeLog2:
        case NvBctDataType_BootDevicePageSizeLog2:
        case NvBctDataType_HashDataOffset:
        case NvBctDataType_HashDataLength:
           *Size = sizeof(NvU32);
            *NumInstances = 1;
            break;
        case NvBctDataType_BctPartitionId:
            *Size = sizeof(NvU8);
            *NumInstances = 1;
            break;

        case NvBctDataType_PartitionSize:
            *Size = sizeof(NvU32);
            *NumInstances = 1;
            break;

        case NvBctDataType_OdmOption:
            *Size = sizeof(NvU32);
            *NumInstances = 1;
            break;
        case NvBctDataType_CustomerDataVersion:
            *Size = sizeof(NvU32);
            *NumInstances = 1;
            break;
#if ENABLE_NVDUMPER
	 case NvBctDataType_DumpRamCarveOut:
            *Size = sizeof(NvU32);
            *NumInstances = 1;
            break;
#endif

        default:
        {
            NvU32 ChipId = NvBitPrivGetChipId();
            NvError e;

            switch(ChipId)
            {
#ifdef ENABLE_T30
                case 0x30:
                    e = NvBctPrivGetDataSizeT30(hBct, DataType, Size, NumInstances);
                    break;
#endif
#ifdef ENABLE_T114
                case 0x35:
                    e = NvBctPrivGetDataSizeT11x(hBct, DataType, Size, NumInstances);
                    break;
#endif
#ifdef ENABLE_T124
                case 0x40:
                    e = NvBctPrivGetDataSizeT12x(hBct, DataType, Size, NumInstances);
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

NvError NvBctGetData(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data)
{
    NvError e = NvSuccess;
    NvU32 MinDataSize;
    NvU32 NumInstances;
    NvU32 ChipId;


    NV_ASSERT(hBct);

    if (!hBct)
        return NvError_BadParameter;

    if (!Size || !Instance)
        return NvError_InvalidAddress;


    NV_CHECK_ERROR(GetDataSize(hBct, DataType, &MinDataSize, &NumInstances));

    if (*Size == 0)
    {
        *Size = MinDataSize;
        *Instance = NumInstances;

        if (Data)
            return NvError_InsufficientMemory;
        else
            return NvSuccess;
    }

    // FIXME -- AuxInternal should move to beginning of customer data;
    //          external requests for customer data should return data
    //          beginning after AuxInternal
    //          Any operation on AuxData needs to be private per chip
    //          When AP20 rolls around, we need to split AuxData ops
    //          into AP20 private versions.
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
            return NvBctGetDataT30(hBct, DataType, Size, Instance, Data);
#endif
#ifdef ENABLE_T114
        case 0x35:
            return NvBctGetDataT11x(hBct, DataType, Size, Instance, Data);
#endif
#ifdef ENABLE_T124
        case 0x40:
            return NvBctGetDataT12x(hBct, DataType, Size, Instance, Data);
#endif
        default:
            return NvError_NotSupported;
    }
}

NvError NvBctSetData(
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data)
{
    NvU32 ChipId;
    NvU32 MinDataSize;
    NvU32 NumInstances;
    NvError e;

    if (!hBct)
        return NvError_BadParameter;

    if (!Size|| !Instance)
        return NvError_InvalidAddress;


    NV_CHECK_ERROR(GetDataSize(hBct, DataType, &MinDataSize, &NumInstances));

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

    // FIXME -- AuxInternal should move to beginning of customer data;
    //          external requests for customer data should return data
    //          beginning after AuxInternal
    //          Any operation on AuxData needs to be private per chip
    //          When AP20 rolls around, we need to split AuxData ops
    //          into AP20 private versions.
    if (*Instance > NumInstances)
        return NvError_BadParameter;

    if (!Data)
        return NvError_InvalidAddress;

    ChipId = NvBitPrivGetChipId();

    switch(ChipId)
    {
#ifdef ENABLE_T30
        case 0x30:
            return NvBctSetDataT30(hBct, DataType, Size, Instance, Data);
#endif
#ifdef ENABLE_T114
        case 0x35:
            return NvBctSetDataT11x(hBct, DataType, Size, Instance, Data);
#endif
#ifdef ENABLE_T124
        case 0x40:
            return NvBctSetDataT12x(hBct, DataType, Size, Instance, Data);
#endif
        default:
            return NvError_NotSupported;
    }
}

NvU32 NvBctGetBctSize(void)
{
    NvU32 ChipId;
    NvU32 BctSize;

    ChipId = NvBitPrivGetChipId();
    switch(ChipId)
    {
#ifdef ENABLE_T30
        case 0x30:
            BctSize = NvBctGetBctSizeT30();
            break;
#endif
#ifdef ENABLE_T114
        case 0x35:
            BctSize = NvBctGetBctSizeT11x();
            break;
#endif
#ifdef ENABLE_T124
        case 0x40:
            BctSize = NvBctGetBctSizeT12x();
            break;
#endif
        default:
            BctSize = 0;
            break;
    }
    return BctSize;
}

NvU32 NvBctGetSignatureOffset(void)
{
    NvU32 ChipId;

    ChipId = NvBitPrivGetChipId();
    switch(ChipId)
    {
#ifdef ENABLE_T114
        case 0x35:
            return NvBctPrivGetSignatureOffsetT11x();
#endif
#ifdef ENABLE_T124
        case 0x40:
            return NvBctPrivGetSignatureOffsetT12x();
#endif
        default:
            return 0;
    }
}

NvU32 NvBctGetSignDataOffset(void)
{
    NvU32 ChipId;

    ChipId = NvBitPrivGetChipId();
    switch(ChipId)
    {
#ifdef ENABLE_T114
        case 0x35:
            return NvBctPrivGetSignDataOffsetT11x();
#endif
#ifdef ENABLE_T124
        case 0x40:
            return NvBctPrivGetSignDataOffsetT12x();
#endif
        default:
            return 16; // sizeof(NvBootHash) for ap20/t30
    }
}
