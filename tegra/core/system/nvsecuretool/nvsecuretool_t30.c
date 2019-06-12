/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvsecuretool.h"
#include "t30/nvboot_bct.h"

#define NVFLASH_MINILOADER_ENTRY 0x4000A000 //entry point for miniloader in IRAM
#define NV_AOS_ENTRY_POINT 0x80108000 // entry point for bootloader in SDRAM
#define NV_MICROBOOT_ENTRY_POINT 0x4000A000 // entry point for microboot in IRAM
#define NV_MICROBOOT_SIZE_MAX 0x00025000 // minimum IRAM size

NvU32 NvSecureGetT30MlAddress(void);
NvU32 NvSecureGetT30BctSize(void);
NvU32 NvSecureGetT30BlLoadAddr(void);
NvU32 NvSecureGetT30BlEntryPoint(void);
NvU32 NvSecureGetT30MbLoadAddr(void);
NvU32 NvSecureGetT30MbEntryPoint(void);
NvU32 NvSecureGetT30MaxMbSize(void);
NvError NvSignT30WarmBootCode(NvU8* BlSrcBuf, NvU32 BlLength);

void NvSecureGetBctT30Interface(NvSecureBctInterface *pBctInterface)
{
    pBctInterface->NvSecureGetMlAddress = NvSecureGetT30MlAddress;
    pBctInterface->NvSecureGetBctSize = NvSecureGetT30BctSize;
    pBctInterface->NvSecureGetBlLoadAddr = NvSecureGetT30BlLoadAddr;
    pBctInterface->NvSecureGetBlEntryPoint = NvSecureGetT30BlEntryPoint;
    pBctInterface->NvSecureGetMbLoadAddr= NvSecureGetT30MbLoadAddr;
    pBctInterface->NvSecureGetMbEntryPoint = NvSecureGetT30MbEntryPoint;
    pBctInterface->NvSecureGetMaxMbSize = NvSecureGetT30MaxMbSize;
    pBctInterface->NvSecureGetDevUniqueId = NULL;
    pBctInterface->NvSecureSignWarmBootCode = NULL;
    pBctInterface->NvSecureSignWB0Image = NULL;
}

NvU32 NvSecureGetT30MlAddress(void)
{
    return NVFLASH_MINILOADER_ENTRY;
}

NvU32 NvSecureGetT30BctSize(void)
{
    return NvBctGetBctSize();
}

NvU32 NvSecureGetT30BlLoadAddr(void)
{
    return NV_AOS_ENTRY_POINT;
}

NvU32 NvSecureGetT30BlEntryPoint(void)
{
    return NV_AOS_ENTRY_POINT;
}

NvU32 NvSecureGetT30MbLoadAddr(void)
{
    return NV_MICROBOOT_ENTRY_POINT;
}

NvU32 NvSecureGetT30MbEntryPoint(void)
{
    return NV_MICROBOOT_ENTRY_POINT;
}

NvU32 NvSecureGetT30MaxMbSize(void)
{
    return NV_MICROBOOT_SIZE_MAX;
}
NvError NvSignT30WarmBootCode(NvU8* BlSrcBuf, NvU32 BlLength)
{
    return NvSuccess;
}
