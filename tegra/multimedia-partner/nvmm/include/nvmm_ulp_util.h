/*
* Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#ifndef INCLUDED_NVMM_ULP_UTIL_H
#define INCLUDED_NVMM_ULP_UTIL_H

#include "nvos.h"
#include "nvmm.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    //Defines various read buffer sizes for fine tuning in ULP mode
    typedef enum 
    {
        ReadBufferSize_2KB     = 0x800,
        ReadBufferSize_4KB     = 0x1000,
        ReadBufferSize_8KB     = 0x2000,
        ReadBufferSize_16KB   = 0x4000,
        ReadBufferSize_64KB   = 0x10000,
        ReadBufferSize_128KB = 0x20000,
        ReadBufferSize_256KB = 0x40000,
        ReadBufferSize_512KB = 0x80000,
        ReadBufferSize_1MB    = 0x100000,
        ReadBufferSize_2MB    = 0x200000
    } ReadBufferSize;


    NvError NvmmPushToOffsetList(NvMMBuffer* pListBuffer, NvU32 addr, NvU32 size, NvMMPayloadMetadata payload);

    NvError NvmmPopFromOffsetList(NvMMBuffer* inputBuffer, NvMMBuffer* pListBuffer, NvBool doPeek);

    NvError NvmmUpdateOffsetList(NvMMBuffer* inputBuffer, NvMMBuffer* pListBuffer);

    NvError NvmmSetBaseAddress(NvRmPhysAddr pBufferMgrBaseAddress, NvU32 vBufferMgrBaseAddress, NvMMBuffer *pListBuffer);

    NvError NvmmResetOffsetList(NvMMBuffer *pListBuffer);
    NvError NvmmSetMaxOffsets(NvMMBuffer *pListBuffer, NvU32 maxOffsets);
    NvError NvmmGetNumOfOffsets(NvMMBuffer *pListBuffer, NvU32* pCount);
    NvError NvmmGetOffsetListStatus(NvMMBuffer *pListBuffer, NvBool* pStatus);
    NvError NvmmSetOffsetListStatus(NvMMBuffer *pListBuffer, NvBool status);
    NvError NvmmSetReadBuffBaseAddress(NvMMBuffer *pListBuffer, char* pBaseAddr);
    NvError NvmmGetReadBuffBaseAddress(NvMMBuffer *pListBuffer, char** pBaseAddr, NvU32 index);
    NvError NvmmGetNumOfBuffMgrIndexs(NvMMBuffer *pListBuffer, NvU32* pCount);
    NvError NvmmGetBaseAddress(NvMMBuffer *pListBuffer,NvU32 *pBaseAddr);

    NvError NvmmGetNumOfValidOffsets(NvMMBuffer *pListBuffer, NvU32* pCount);


#if defined(__cplusplus)
}
#endif

#endif
