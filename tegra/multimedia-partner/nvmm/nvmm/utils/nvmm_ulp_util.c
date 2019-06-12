/*
* Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#include "nvmm_ulp_util.h"

#include "nvrm_memmgr.h"
#include "nvrm_hardware_access.h"


NvError NvmmPushToOffsetList(NvMMBuffer* pListBuffer, NvU32 addr, NvU32 size, NvMMPayloadMetadata payload)
{
    NvMMOffsetList* pList = (NvMMOffsetList*)pListBuffer->Payload.Ref.pMem;
    NvU32 n = pList->numOffsets;
    NvMMOffset *OffsetTable = (NvMMOffset *)((NvU8 *)pList + sizeof(NvMMOffsetList));


    //Calculate offset using the base address
    if((addr <  pList->VBase) || (!size)) return NvError_BadParameter;

    if(pList->numOffsets >= pList->maxOffsets)
        return NvError_NotSupported;

    OffsetTable[n].offset = (NvU32)addr -  (NvU32)pList->VBase;
    OffsetTable[n].size = size;
    OffsetTable[n].BufferFlags = payload.BufferFlags;
    OffsetTable[n].TimeStamp = payload.TimeStamp;
    if ( payload.BufferFlags & NvMMBufferFlag_Marker)
    {
        OffsetTable[n].MarkerNum = payload.BufferMetaData.AsfMarkerdata.MarkerNumber;
    }
    pList->numOffsets++;
    return NvSuccess;
}


NvError NvmmPopFromOffsetList(NvMMBuffer* inputBuffer, NvMMBuffer* pListBuffer, NvBool doPeek)
{
    NvMMOffsetList* pList = inputBuffer->Payload.Ref.pMem;
    NvU32 i = pList->tableIndex;
    NvMMOffset *OffsetTable = (NvMMOffset *)((NvU8 *)pList + sizeof(NvMMOffsetList));

    if(i >= pList->numOffsets) {
        pList->tableIndex = 0;
        return NvError_BadParameter;
    }

#if !NV_IS_AVP
    pListBuffer->Payload.Ref.pMem =  (void*)((NvU32)OffsetTable[i].offset + (NvU32)pList->VBase);
#else
    pListBuffer->Payload.Ref.pMem =  (void*)((NvU32)OffsetTable[i].offset + (NvU32)pList->PBase);
    pListBuffer->Payload.Ref.PhyAddress = (NvU32)OffsetTable[i].offset + (NvU32)pList->PBase;
#endif
    pListBuffer->Payload.Ref.MemoryType = NvMMMemoryType_SYSTEM;
    pListBuffer->Payload.Ref.sizeOfValidDataInBytes = OffsetTable[i].size - pList->ConsumedSize;
    pListBuffer->Payload.Ref.startOfValidData = pList->ConsumedSize;
    pListBuffer->PayloadInfo.BufferFlags = OffsetTable[i].BufferFlags;
    pListBuffer->PayloadInfo.TimeStamp = OffsetTable[i].TimeStamp;
    pListBuffer->PayloadType = inputBuffer->PayloadType;

    if( pListBuffer->PayloadInfo.BufferFlags & NvMMBufferFlag_Marker)
    {
        pListBuffer->PayloadInfo.BufferMetaData.AsfMarkerdata.MarkerNumber = OffsetTable[i].MarkerNum;
    }
  
    // If this is real pop, then increment the table index
    if(doPeek == NV_FALSE) pList->tableIndex++;

    return NvSuccess;
}

NvError NvmmUpdateOffsetList(NvMMBuffer* inputBuffer, NvMMBuffer* pListBuffer)
{
    NvMMOffsetList* pList = inputBuffer->Payload.Ref.pMem;
    NvU32 i = pList->tableIndex;
    NvMMOffset *OffsetTable = (NvMMOffset *)((NvU8 *)pList + sizeof(NvMMOffsetList));

    pList->ConsumedSize = pListBuffer->Payload.Ref.startOfValidData;
    //Updating the buffers flags if modified by client
    OffsetTable[i].BufferFlags = pListBuffer->PayloadInfo.BufferFlags;

    if(pList->ConsumedSize >= OffsetTable[i].size) {
        pList->tableIndex++;
        pList->ConsumedSize = 0;
    }

    return NvSuccess;

}

NvError NvmmSetBaseAddress(NvRmPhysAddr pBufferMgrBaseAddress, NvU32 vBufferMgrBaseAddress, NvMMBuffer *pListBuffer)
{
    NvMMOffsetList* pList = (NvMMOffsetList*)pListBuffer->Payload.Ref.pMem;
    pList->VBase = vBufferMgrBaseAddress;
    pList->PBase = pBufferMgrBaseAddress;
    return NvSuccess;
}


NvError NvmmGetBaseAddress(NvMMBuffer *pListBuffer,NvU32 *pBaseAddr)
{
    NvMMOffsetList* pList = (NvMMOffsetList*)pListBuffer->Payload.Ref.pMem;
    *pBaseAddr = pList->VBase;
    return NvSuccess;
}

NvError NvmmResetOffsetList(NvMMBuffer *pListBuffer)
{

    NvMMOffsetList* pList = (NvMMOffsetList*)pListBuffer->Payload.Ref.pMem;
    NvMMOffset *OffsetTable = (NvMMOffset *)((NvU8 *)pList + sizeof(NvMMOffsetList));
    NvU32 i;
    // Set total offsets to zero
    pList->numOffsets = 0;
    // Reset consumed size
    pList->ConsumedSize = 0;
    //Set table index to zero
    pList->tableIndex = 0;
    pList->VBase = 0;
    pList->PBase = 0;   
    pList->isOffsetListInUse = NV_FALSE;
    pList->BuffMgrIndex = 0;

    // Set Base Address to NULL
    for (i = 0; i < MAX_READBUFF_ADDRESSES; i++)
        pList->BuffMgrBaseAddr[i] = NULL;
    // Reset of the offset table
    NvOsMemset(&OffsetTable[0], 0x00, (sizeof(NvMMOffset) * pList->maxOffsets));
    pListBuffer->Payload.Ref.startOfValidData = 0;
    pListBuffer->Payload.Ref.sizeOfValidDataInBytes = sizeof(NvMMOffsetList);
    return NvSuccess;

}

NvError NvmmSetMaxOffsets(NvMMBuffer *pListBuffer, NvU32 maxOffsets)
{
    NvMMOffsetList* pList = (NvMMOffsetList*)pListBuffer->Payload.Ref.pMem;
    pList->maxOffsets = maxOffsets;
    return NvSuccess;
}

NvError NvmmGetNumOfOffsets(NvMMBuffer *pListBuffer, NvU32* pCount)
{

    NvMMOffsetList* pList = (NvMMOffsetList*)pListBuffer->Payload.Ref.pMem;
    *pCount = pList->numOffsets;
    return NvSuccess;

}

NvError NvmmGetOffsetListStatus(NvMMBuffer *pListBuffer, NvBool* pStatus)
{

    NvMMOffsetList* pList = (NvMMOffsetList*)pListBuffer->Payload.Ref.pMem;
    *pStatus = pList->isOffsetListInUse;
    return NvSuccess;

}

NvError NvmmSetOffsetListStatus(NvMMBuffer *pListBuffer, NvBool status)
{

    NvMMOffsetList* pList = (NvMMOffsetList*)pListBuffer->Payload.Ref.pMem;
    pList->isOffsetListInUse = status;
    return NvSuccess;

}

NvError NvmmSetReadBuffBaseAddress(NvMMBuffer *pListBuffer, char* pBaseAddr)
{
    NvMMOffsetList* pList = (NvMMOffsetList*)pListBuffer->Payload.Ref.pMem;

    if (pList->BuffMgrIndex >= MAX_READBUFF_ADDRESSES)
        return NvError_BadParameter;
    pList->BuffMgrBaseAddr[pList->BuffMgrIndex] = pBaseAddr;
    pList->BuffMgrIndex++;
    return NvSuccess;
}

NvError NvmmGetReadBuffBaseAddress(NvMMBuffer *pListBuffer, char** pBaseAddr, NvU32 index)
{
    NvMMOffsetList* pList = (NvMMOffsetList*)pListBuffer->Payload.Ref.pMem;
    *pBaseAddr = pList->BuffMgrBaseAddr[index];

    return NvSuccess;
}
NvError NvmmGetNumOfBuffMgrIndexs(NvMMBuffer *pListBuffer, NvU32* pCount)
{
    NvMMOffsetList* pList = (NvMMOffsetList*)pListBuffer->Payload.Ref.pMem;
    *pCount = pList->BuffMgrIndex;
    return NvSuccess;
}

NvError NvmmGetNumOfValidOffsets(NvMMBuffer *pListBuffer, NvU32* pCount)
{
    NvMMOffsetList* pList = (NvMMOffsetList*)pListBuffer->Payload.Ref.pMem;
    *pCount = pList->numOffsets - pList->tableIndex;
    return NvSuccess;
}

