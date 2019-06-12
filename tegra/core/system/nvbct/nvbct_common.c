/*
 * Copyright (c) 2009-2014, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVBCT_SOC
#error "Error: NVBCT_SOC must be defined before including this file"
#endif

#define NV_BCT_LENGTH sizeof(NvBootConfigTable)

#define NVBCTGETDATASOC NVBCT_SOC(NvBctGetData)
#define NVBCTPRIVGETDATASOC NVBCT_SOC(NvBctPrivGetData)

NvError NVBCTGETDATASOC (
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data)
{
    NvU32 AuxDataOffset;
    NvU8 *Data8 = (NvU8 *)Data;
    NvBctAuxInternalData *pAuxInternal;
    NvBctCustomerPlatformData *pCustomerPlatformData;
    NvBootConfigTable *bct;

    AuxDataOffset = NVBOOT_BCT_CUSTOMER_DATA_SIZE - 
     sizeof(NvBctAuxInternalData);

    bct = (NvBootConfigTable*)hBct;

    pAuxInternal = (NvBctAuxInternalData *)
        (bct->CustomerData + AuxDataOffset);

    pCustomerPlatformData = (NvBctCustomerPlatformData *)bct->CustomerData;


    switch(DataType){    
        case NvBctDataType_Version:
            NvOsMemcpy(Data, &bct->BootDataVersion, *Size);
            break;
        case NvBctDataType_BootDeviceConfigInfo:
            NvOsMemcpy(Data, &(bct->DevParams[*Instance]), *Size);
            break;
        case NvBctDataType_NumValidBootDeviceConfigs:
            NvOsMemcpy(Data, &bct->NumParamSets, *Size);
            break;
        case NvBctDataType_SdramConfigInfo:
            NvOsMemcpy(Data, &(bct->SdramParams[*Instance]), *Size);
            break;
        case NvBctDataType_NumValidSdramConfigs:
            NvOsMemcpy(Data, &bct->NumSdramSets, *Size);
            break;
        case NvBctDataType_DevType:
            NvOsMemcpy(Data, &(bct->DevType[*Instance]), *Size);
            break;
        case NvBctDataType_NumValidDevType: {
            NvU32 NumValidDevType = NVBOOT_BCT_MAX_PARAM_SETS;
            NvOsMemcpy(Data, &NumValidDevType, *Size);
            break;
            }
        case NvBctDataType_NumEnabledBootLoaders:
            NvOsMemcpy(Data, &bct->BootLoadersUsed, *Size);
            break;
        case NvBctDataType_OdmOption:
            NvOsMemcpy(Data, &pAuxInternal->CustomerOption, *Size);
            break;
        case NvBctDataType_BctPartitionId:
            NvOsMemcpy(Data, &pAuxInternal->BctPartitionId, *Size);
            break;
        case NvBctDataType_BootLoaderStartBlock:
            NvOsMemcpy(Data, &(bct->BootLoader[*Instance].StartBlock), *Size);
            break;
        case NvBctDataType_BootLoaderStartSector:
            NvOsMemcpy(Data, &(bct->BootLoader[*Instance].StartPage), *Size);
            break;
        case NvBctDataType_BootLoaderLength:
            NvOsMemcpy(Data, &(bct->BootLoader[*Instance].Length), *Size);
            break;
        case NvBctDataType_BootLoaderLoadAddress:
            NvOsMemcpy(Data, &(bct->BootLoader[*Instance].LoadAddress), *Size);
            break;
        case NvBctDataType_BootLoaderEntryPoint:
            NvOsMemcpy(Data, &(bct->BootLoader[*Instance].EntryPoint), *Size);
            break;
        case NvBctDataType_BootDeviceBlockSizeLog2:
            NvOsMemcpy(Data, &bct->BlockSizeLog2, *Size);
            break;
        case NvBctDataType_BootDevicePageSizeLog2:
            NvOsMemcpy(Data, &bct->PageSizeLog2, *Size);
            break;
        case NvBctDataType_CryptoSalt:
        {
            NvU32 i;
            NV_ASSERT(*Size == NVBOOT_CMAC_AES_HASH_LENGTH);

            for (i=0; i<NVBOOT_CMAC_AES_HASH_LENGTH; i++)
                UNPACK_NVU32(&Data8[i*sizeof(NvU32)], bct->RandomAesBlock.hash[i]);
        }
        break;
        case NvBctDataType_BadBlockTable:
            NvOsMemcpy(Data, &bct->BadBlockTable, *Size);
            break;
        case NvBctDataType_PartitionSize:
            NvOsMemcpy(Data, &bct->PartitionSize, *Size);
            break;
        case NvBctDataType_CustomerDataVersion:
        {
            NvU32 cusDataVer = (NvU32)pCustomerPlatformData->CustomerDataVersion;
            NvOsMemcpy(Data, &cusDataVer, *Size);
        }
            break;
        case NvBctDataType_AuxData:
            NvOsMemcpy(Data, bct->CustomerData +
                       sizeof(NvBctCustomerPlatformData), *Size);
            break;
        case NvBctDataType_AuxDataAligned:
        {
            NvU8 *ptr, *alignedPtr;

            // get aligned aux data pointer
            ptr = bct->CustomerData + sizeof(NvBctCustomerPlatformData);
            alignedPtr = (NvU8*)((((NvU32)ptr + sizeof(NvU32) - 1) >> 2)
                                 << 2);
            NvOsMemcpy(Data, alignedPtr, *Size);
        }
        break;
        case NvBctDataType_HashDataOffset:
        {
            NvU32 Offset = offsetof(NvBootConfigTable, RandomAesBlock);
            NvOsMemcpy(Data, &Offset, *Size);
        }
        break;
        case NvBctDataType_HashDataLength:
        {
            NvU32 Length = sizeof(NvBootConfigTable);
            Length -= offsetof(NvBootConfigTable, RandomAesBlock);
            NvOsMemcpy(Data, &Length, *Size);
        }
        break;
        case NvBctDataType_FullContents:
            NvOsMemcpy(Data, bct, NV_BCT_LENGTH);
            break;
        case NvBctDataType_BctSize:
        {
            NvU32 Length = sizeof(NvBootConfigTable);
            NvOsMemcpy(Data, &Length, *Size);
        }
        break;
        case NvBctDataType_Reserved:
            NvOsMemcpy(Data, &bct->Reserved[*Instance], *Size);
            break;
#if ENABLE_NVDUMPER
	 case NvBctDataType_DumpRamCarveOut:
            NvOsMemcpy(Data, &pAuxInternal->DumpRamCarveOut, *Size);
            break;
#endif

        default:
            if (NVBCTPRIVGETDATASOC (hBct, DataType, Size, Instance, Data) != NvSuccess)
                goto fail;
    }
    return NvSuccess;
    
fail:
    return NvError_BadParameter;
}

#undef NVBCTPRIVGETDATASOC
#undef NVBCTGETDATASOC

#define NVBCTSETDATASOC NVBCT_SOC(NvBctSetData)
#define NVBCTPRIVSETDATASOC NVBCT_SOC(NvBctPrivSetData)

NvError NVBCTSETDATASOC (
    NvBctHandle hBct,
    NvBctDataType DataType,
    NvU32 *Size,
    NvU32 *Instance,
    void *Data)
{
    NvBctAuxInternalData *pAuxInternal;
    NvBootConfigTable *bct;
    NvU8 *Data8 = (NvU8 *)Data;
    NvU32 AuxDataOffset;
    NvError e = NvSuccess;

    bct = (NvBootConfigTable*)hBct;

    AuxDataOffset = NVBOOT_BCT_CUSTOMER_DATA_SIZE - 
         sizeof(NvBctAuxInternalData);
    
    pAuxInternal = (NvBctAuxInternalData *)
        (bct->CustomerData + AuxDataOffset);
    switch(DataType){
        case NvBctDataType_Version:
            NvOsMemcpy(&bct->BootDataVersion, Data, *Size);
            break;
        case NvBctDataType_BootDeviceConfigInfo:
            NvOsMemcpy(&(bct->DevParams[*Instance]), Data, *Size);
            break;
        case NvBctDataType_NumValidBootDeviceConfigs:
            NvOsMemcpy(&bct->NumParamSets, Data, *Size);
            break;
        case NvBctDataType_SdramConfigInfo:
            NvOsMemcpy(&(bct->SdramParams[*Instance]), Data, *Size);
            break;
        case NvBctDataType_NumValidSdramConfigs:
            NvOsMemcpy(&bct->NumSdramSets, Data, *Size);
            break;
        case NvBctDataType_DevType:
            NvOsMemcpy(&(bct->DevType[*Instance]), Data, *Size);
            break;
        case NvBctDataType_NumValidDevType:
            break;
        case NvBctDataType_NumEnabledBootLoaders:
            NvOsMemcpy(&bct->BootLoadersUsed, Data, *Size);
            break;
        case NvBctDataType_OdmOption:
            NvOsMemcpy(&(pAuxInternal->CustomerOption), Data, *Size);
            break;
        case NvBctDataType_BctPartitionId:
            NvOsMemcpy(&pAuxInternal->BctPartitionId, Data, *Size);
            break;
        case NvBctDataType_BootLoaderStartBlock:
            NvOsMemcpy(&(bct->BootLoader[*Instance].StartBlock), Data, *Size);
            break;
        case NvBctDataType_BootLoaderStartSector:
            NvOsMemcpy(&(bct->BootLoader[*Instance].StartPage), Data, *Size);
            break;
        case NvBctDataType_BootLoaderLength:
            NvOsMemcpy(&(bct->BootLoader[*Instance].Length), Data, *Size);
            break;
        case NvBctDataType_BootLoaderLoadAddress:
            NvOsMemcpy(&(bct->BootLoader[*Instance].LoadAddress), Data, *Size);
            break;
        case NvBctDataType_BootLoaderEntryPoint:
            NvOsMemcpy(&(bct->BootLoader[*Instance].EntryPoint), Data, *Size);
            break;
        case NvBctDataType_BootDeviceBlockSizeLog2:
            NvOsMemcpy(&bct->BlockSizeLog2, Data, *Size);
            break;
        case NvBctDataType_BootDevicePageSizeLog2:
            NvOsMemcpy(&bct->PageSizeLog2, Data, *Size);
            break;
        case NvBctDataType_CryptoSalt:
        {
            NvU32 i;
            NV_ASSERT(*Size == NVBOOT_CMAC_AES_HASH_LENGTH);

            for (i=0; i<NVBOOT_CMAC_AES_HASH_LENGTH; i++)
                PACK_NVU32(bct->RandomAesBlock.hash[i], &Data8[i*sizeof(NvU32)]);
        }
        break;
        case NvBctDataType_BadBlockTable:
            NvOsMemcpy(&bct->BadBlockTable, Data, *Size);
            break;
        case NvBctDataType_PartitionSize:
            NvOsMemcpy(&bct->PartitionSize, Data, *Size);
            break;
        case NvBctDataType_AuxData:
            NvOsMemcpy(bct->CustomerData + sizeof(NvBctCustomerPlatformData),
                       Data, *Size);
            break;
        case NvBctDataType_AuxDataAligned:
        {
            NvU8 *ptr, *alignedPtr;

            // get aligned aux data pointer
            ptr = bct->CustomerData + sizeof(NvBctCustomerPlatformData);
            alignedPtr = (NvU8*)((((NvU32)ptr + sizeof(NvU32) - 1) >> 2)
                                 << 2);
            NvOsMemcpy(alignedPtr, Data, *Size);
        }
        break;
        case NvBctDataType_HashDataOffset:
            e = NvError_InvalidState; // member is write-only
            break;
        case NvBctDataType_HashDataLength:
            e = NvError_InvalidState; // member is write-only
            break;
        case NvBctDataType_FullContents:
            NvOsMemcpy(bct, Data, *Size);
            break;
        case NvBctDataType_Reserved:
            NvOsMemcpy(&(bct->Reserved[*Instance]), Data, *Size);
            break;
#if ENABLE_NVDUMPER
        case NvBctDataType_DumpRamCarveOut:
            NvOsMemcpy(&pAuxInternal->DumpRamCarveOut, Data, *Size);
            break;
#endif

        default:
            if (NVBCTPRIVSETDATASOC (hBct, DataType, Size, Instance, Data) != NvSuccess)
            goto fail;
    }
    
    return e;
  
fail:
    return NvError_BadParameter;
}
#undef NVBCTSETDATASOC
#undef NVBCTPRIVSETDATASOC
#undef UNPACK_NVU32
#undef PACK_NVU32
#undef NV_BCT_LENGTH
