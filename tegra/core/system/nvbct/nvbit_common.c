/*
 * Copyright (c) 2009 - 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVBIT_SOC
#error "Error: NVBIT_SOC must be defined before including this file"
#endif

#ifndef NV_BIT_LOCATION
#error "Error: NV_BIT_LOCATION must be defined before including this file"
#endif

#define NV_BIT_LENGTH sizeof(NvBootInfoTable)

#define NVBITINITSOC NVBIT_SOC(NvBitInit)
#define NVBITPRIVISVALIDSOC NVBIT_SOC(NvBitPrivIsValidBit)

NvError NVBITINITSOC(NvBitHandle *phBit)
{
    NvBitHandle tempHandle = NULL;
    NvError e = NvSuccess;

#if NVODM_BOARD_IS_SIMULATION
    NvBootInfoTable *bit;
    tempHandle = NvOsAlloc(NV_BIT_LENGTH);
    if (!tempHandle)
        goto fail;
    NvOsMemset(tempHandle,0,NV_BIT_LENGTH);
    bit = (NvBootInfoTable *)tempHandle;
    //Claim that we booted off the first bootloader
    bit->BlState[0].Status = NvBootRdrStatus_Success;
    NVBITPRIVISVALIDSOC (bit); //ignore this, done to avoid compilation errors.
#else
    NV_CHECK_ERROR_CLEANUP(
        NvOsPhysicalMemMap(NV_BIT_LOCATION, (NV_BIT_LENGTH+4095)&~4095,
            NvOsMemAttribute_Uncached, NVOS_MEM_READ_WRITE,
            (void **)&tempHandle)
    );

    if (!NVBITPRIVISVALIDSOC ((NvBootInfoTable*)tempHandle))
    {
        e = NvError_BadParameter;
        goto fail;
    }
#endif
    *phBit = tempHandle;

    return NvSuccess;

fail:

    return e;
}

#undef NVBITINITSOC
#undef NVBITPRIVISVALIDSOC

#define NVBITGETDATASOC NVBIT_SOC(NvBitGetData)
#define NVBITPRIVGETDATASOC NVBIT_SOC(NvBitPrivGetData)

NvError NVBITGETDATASOC(
    NvBitHandle hBit,
    NvBitDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data)
{
    NvBootInfoTable *bit;

    bit = (NvBootInfoTable *)hBit;

    switch(DataType){
        case NvBitDataType_BootRomVersion:
            NvOsMemcpy(Data, &bit->BootRomVersion, *Size);
            break;
        case NvBitDataType_BootDataVersion:
            NvOsMemcpy(Data, &bit->DataVersion, *Size);
            break;
        case NvBitDataType_RcmVersion:
            NvOsMemcpy(Data, &bit->RcmVersion, *Size);
            break;
        case NvBitDataType_BootType:
            NvOsMemcpy(Data, &bit->BootType, *Size);
            break;
        case NvBitDataType_PrimaryDevice:
            NvOsMemcpy(Data, &bit->PrimaryDevice, *Size);
            break;
        case NvBitDataType_SecondaryDevice:
            NvOsMemcpy(Data, &bit->SecondaryDevice, *Size);
            break;
        case NvBitDataType_OscFrequency:
            NvOsMemcpy(Data, &bit->OscFrequency, *Size);
            break;
        case NvBitDataType_IsValidBct:
            NvOsMemcpy(Data, &bit->BctValid, *Size);
            break;
        case NvBitDataType_ActiveBctBlock:
            NvOsMemcpy(Data, &bit->BctBlock, *Size);
            break;
        case NvBitDataType_ActiveBctSector:
            NvOsMemcpy(Data, &bit->BctPage, *Size);
            break;
        case NvBitDataType_BctSize:
            NvOsMemcpy(Data, &bit->BctSize, *Size);
            break;
        case NvBitDataType_ActiveBctPtr:
            NvOsMemcpy(Data, &bit->BctPtr, *Size);
            break;
        case NvBitDataType_SafeStartAddr:
            NvOsMemcpy(Data, &bit->SafeStartAddr, *Size);
            break;
        case NvBitDataType_BlStatus:
            NvOsMemcpy(Data, &(bit->BlState[*Instance].Status), *Size);
            break;
        case NvBitDataType_BlUsedForEccRecovery:
            NvOsMemcpy(Data,
                &(bit->BlState[*Instance].UsedForEccRecovery), *Size);
            break;
        default:
            if (NVBITPRIVGETDATASOC(hBit, DataType, Size,
                   Instance, Data) != NvSuccess)
                goto fail;
    }

    return NvSuccess;

fail:
    return NvError_BadParameter;
}

#undef NVBITPRIVGETDATASOC
#undef NVBITGETDATASOC
#undef NV_BIT_LENGTH
