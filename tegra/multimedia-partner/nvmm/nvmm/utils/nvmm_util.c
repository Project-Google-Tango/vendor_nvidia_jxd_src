/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvassert.h"
#include "nvmm_util.h"
#include "nvmmlite.h"
#include "nvrm_hardware_access.h"
#include "nvmm_debug.h"

#include "nvmm_bufferprofile.h"
#include "nvmm_camera.h"
#include "nvmm_digitalzoom.h"
#include "nvmm_block.h"
#include "nvfuse_bypass.h"
#include "nvrm_module.h"

#if NV_USE_NVAVP
#include "nvavp.h"
#endif

#if defined(HAVE_X11)
#include <X11/Xlib.h>
#include "tdrlib.h"
#endif

NvError NvMMUtilDeallocateBufferPrivate(NvMMBuffer *pBuffer,
                                        void *hService,
                                        NvBool bAbnormalTermination);

NvError NvMMUtilAllocateBuffer(
    NvRmDeviceHandle hRmDevice,
    NvU32 size,
    NvU32 align,
    NvMMMemoryType memoryType,
    NvBool bInSharedMemory,
    NvMMBuffer **pBuf)
{
    NvRmMemHandle hMemHandle;
    NvMMBuffer *pBuffer = *pBuf;
    NvError status = NvSuccess;
    NvOsMemAttribute  coherency;

    NvOsMemset(pBuffer, 0, sizeof(NvMMBuffer));
    pBuffer->StructSize = sizeof(NvMMBuffer);
    pBuffer->Payload.Ref.PhyAddress = NV_RM_INVALID_PHYS_ADDRESS;

    if (bInSharedMemory == NV_TRUE)
    {
        switch (memoryType)
        {
            case NvMMMemoryType_Uncached :
                coherency = NvOsMemAttribute_Uncached;
                break;
            case NvMMMemoryType_WriteBack :
                coherency = NvOsMemAttribute_WriteBack;
                break;
            case NvMMMemoryType_WriteCombined :
                coherency = NvOsMemAttribute_WriteCombined;
                break;
            case NvMMMemoryType_InnerWriteBack :
                coherency = NvOsMemAttribute_InnerWriteBack;
                break;
            default :
                coherency = NvOsMemAttribute_Uncached;
                break;
        }

        status = NvRmMemHandleAlloc(hRmDevice,
                                NULL,
                                0,
                                align,
                                coherency,
                                size,
                                0,
                                0,
                                &hMemHandle);
        if (status != NvSuccess)
        {
            return status;
        }

        NvRmMemPin(hMemHandle);

        // the payload on the external carved out
        pBuffer->Payload.Ref.MemoryType = memoryType;
        pBuffer->Payload.Ref.sizeOfBufferInBytes = size;
        pBuffer->Payload.Ref.hMem = hMemHandle;

        if (!size)
        {
            pBuffer->PayloadType = NvMMPayloadType_None;
        }
        else
        {
            pBuffer->PayloadType = NvMMPayloadType_MemHandle;
            pBuffer->Payload.Ref.PhyAddress = NvRmMemGetAddress(pBuffer->Payload.Ref.hMem, pBuffer->Payload.Ref.Offset);

            status = NvRmMemMap(pBuffer->Payload.Ref.hMem,
                                pBuffer->Payload.Ref.Offset,
                                size,
                                NVOS_MEM_READ_WRITE,
                                &pBuffer->Payload.Ref.pMem);
            if (status != NvSuccess)
            {
                pBuffer->PayloadInfo.BufferFlags |= NvMMBufferFlag_MemMapError;
                status = NvSuccess;
            }
        }

        return status;
    }
    else
    {
        pBuffer->Payload.Ref.MemoryType = memoryType;
        pBuffer->Payload.Ref.sizeOfBufferInBytes = size;

        if (!size)
        {
            pBuffer->PayloadType = NvMMPayloadType_None;
        }
        else
        {
            pBuffer->PayloadType = NvMMPayloadType_MemPointer;
            pBuffer->Payload.Ref.pMem = NvOsAlloc(size);
            if (!pBuffer->Payload.Ref.pMem)
                return NvError_InsufficientMemory;
        }
    }
    return NvSuccess;
}

NvError NvMMUtilAllocateVideoBuffer(
    NvRmDeviceHandle hRmDevice,
    NvMMVideoFormat VideoFormat,
    NvMMBuffer **pBuf)
{
    NvMMBuffer *pBuffer;
    NvError status = NvSuccess;


    pBuffer = NvOsAlloc(sizeof(NvMMBuffer));
    NvOsMemset(pBuffer, 0, sizeof(NvMMBuffer));
    pBuffer->StructSize = sizeof(NvMMBuffer);
    pBuffer->PayloadType = NvMMPayloadType_SurfaceArray;


    pBuffer->Payload.Surfaces.SurfaceCount = VideoFormat.NumberOfSurfaces;
    NvOsMemcpy(&pBuffer->Payload.Surfaces.Surfaces,
               VideoFormat.SurfaceDescription,sizeof(NvRmSurface) *
               NVMMSURFACEDESCRIPTOR_MAX_SURFACES);

    NvMMUtilAllocateSurfaces(hRmDevice, &pBuffer->Payload.Surfaces);

    *pBuf = pBuffer;
    return status;
}


NvError
NvMMUtilDeallocateBuffer(
    NvMMBuffer *pBuffer)
{
    NvError status = NvSuccess;

    status = NvMMUtilDeallocateBufferPrivate(pBuffer, NULL, NV_FALSE);

    return status;
}

NvError
NvMMUtilDeallocateBlockSideBuffer(
    NvMMBuffer *pBuffer,
    void *hService,
    NvBool bAbnormalTermination)
{
    NvError status = NvSuccess;

    status = NvMMUtilDeallocateBufferPrivate(pBuffer, NULL, bAbnormalTermination);

    return status;
}

NvError
NvMMUtilDeallocateBufferPrivate(
    NvMMBuffer *pBuffer,
    void *hService,
    NvBool bAbnormalTermination)
{
    NvError status = NvSuccess;

    if (!pBuffer)
        return status;

    if (pBuffer->PayloadType == NvMMPayloadType_MemHandle)
    {
        NvRmMemUnmap(pBuffer->Payload.Ref.hMem, pBuffer->Payload.Ref.pMem,
                     pBuffer->Payload.Ref.sizeOfBufferInBytes);
        NvRmMemUnpin(pBuffer->Payload.Ref.hMem);
        NvRmMemHandleFree(pBuffer->Payload.Ref.hMem);
        pBuffer->Payload.Ref.pMem = NULL;
        pBuffer->Payload.Ref.PhyAddress = 0;
    }
    else if (pBuffer->PayloadType == NvMMPayloadType_MemPointer)
    {
        NvOsFree(pBuffer->Payload.Ref.pMem);
        pBuffer->Payload.Ref.pMem = NULL;
    }

    return status;
}

NvError NvMMUtilDeallocateVideoBuffer(NvMMBuffer *pBuffer)
{
    NvError status = NvSuccess;

    if (!pBuffer)
    {
        return NvSuccess;
    }

    NvMMUtilDestroySurfaces(&pBuffer->Payload.Surfaces);
    NvOsFree(pBuffer);
    pBuffer = NULL;

    return status;
}

NvError NvMemAlloc(NvRmDeviceHandle hRmDev,
                          NvRmMemHandle* phMem,
                          NvU32 Size,
                          NvU32 Alignment,
                          NvU32 *PhysicalAddress)
{
    NvError e;
    NvRmMemHandle hMem;

    e = NvRmMemHandleAlloc(hRmDev, NULL, 0, Alignment,
            NvOsMemAttribute_Uncached, Size, 0, 0, &hMem);
    if (e != NvSuccess)
    {
        goto fail;
    }

    *phMem = hMem;
    *PhysicalAddress = NvRmMemPin(hMem);
    return NvSuccess;

fail:
    return e;
} // end of NvMemAlloc


NvError
NvMMUtilAllocateSurfaces(
    NvRmDeviceHandle hRmDev,
    NvMMSurfaceDescriptor *pSurfaceDesc)
{
    NvError err = NvSuccess;
    NvS32 ComponentSize, SurfaceAlignment, i;

    for (i=0; i<pSurfaceDesc->SurfaceCount; i++)
    {
        // calculating Y-Pitch based on chroma format instead of calling NvRmSurfaceComputePitch
        SurfaceAlignment = NvRmSurfaceComputeAlignment(hRmDev, &pSurfaceDesc->Surfaces[i]);
        ComponentSize = NvRmSurfaceComputeSize(&pSurfaceDesc->Surfaces[i]);
        if (ComponentSize)
        {
            err = NvMemAlloc(hRmDev, &pSurfaceDesc->Surfaces[i].hMem, ComponentSize, SurfaceAlignment, &pSurfaceDesc->PhysicalAddress[i]);
            if (err != NvSuccess)
            {
                 goto fail;
            }
        }
    }

    return NvSuccess;

fail:
    for (i=0; i<pSurfaceDesc->SurfaceCount - 1; i++)
    {
        NvMemFree(pSurfaceDesc->Surfaces[i].hMem);
        pSurfaceDesc->Surfaces[i].hMem = NULL;
    }
    return err;
}


void NvMemFree(NvRmMemHandle hMem)
{
    NvRmMemUnpin(hMem);
    NvRmMemHandleFree(hMem);
} // end of NvMemFree


void NvMMUtilDestroySurfaces(NvMMSurfaceDescriptor *pSurfaceDesc)
{
    NvS32 i, j, surfCnt;
    NvRmMemHandle hMem = NULL;

    surfCnt = pSurfaceDesc->SurfaceCount;
    for (i = 0; i < surfCnt; i++)
    {
        hMem = pSurfaceDesc->Surfaces[i].hMem;
        if (hMem != NULL)
        {
            NvMemFree(hMem);
            pSurfaceDesc->Surfaces[i].hMem = NULL;
            // If our hMem handle is shared by several surfaces,
            // clean it out so we don't attempt to free it again:
            for (j = i+1; j < surfCnt; j++)
            {
                if (pSurfaceDesc->Surfaces[j].hMem == hMem)
                    pSurfaceDesc->Surfaces[j].hMem = NULL;
            }
        }
    }
}

NvError
NvMMUtilAddBufferProfilingEntry(
    NvMMBufferProfilingData* pProfilingData,
    NvMMBufferProfilingEvent Event,
    NvU32 StreamIndex,
    NvU32 FrameId)
{
    NvU32 i = pProfilingData->NumEntries;

    if (pProfilingData->NumEntries >= MAX_BUFFER_PROFILING_ENTRIES)
    {
        return NvError_InvalidState;
    }

    pProfilingData->Entries[i].Event = Event;
    pProfilingData->Entries[i].FrameId = FrameId;
    pProfilingData->Entries[i].StreamIndex = StreamIndex;
    pProfilingData->Entries[i].TimeStamp = NvOsGetTimeUS() * 10;

    pProfilingData->NumEntries++;

    return NvSuccess;
}

NvError
NvMMUtilDumpBufferProfilingData(
    NvMMBufferProfilingData* pProfilingData,
    NvOsFileHandle hFile)
{
    NvU32 i;

    for (i = 0; i < pProfilingData->NumEntries; i++)
    {
        // StreamIndex
        switch(pProfilingData->BlockType)
        {

            case NvMMBlockType_SrcCamera:
                NvOsFprintf(hFile, "Camera Block, ");
                switch(pProfilingData->Entries[i].StreamIndex)
                {
                    case NvMMCameraStreamIndex_OutputPreview:
                        NvOsFprintf(hFile, "Preview Output, ");
                        break;
                    case NvMMCameraStreamIndex_Output:
                        NvOsFprintf(hFile, "Still/Video Output, ");
                        break;
                    default:
                        NvOsFprintf(hFile, "Unknown Stream, ");
                        break;

                }
                break;
            case NvMMBlockType_DigitalZoom:
                NvOsFprintf(hFile, "DZ Block, ");
                switch(pProfilingData->Entries[i].StreamIndex)
                {
                    case NvMMDigitalZoomStreamIndex_InputPreview:
                        NvOsFprintf(hFile, "Preview Input, ");
                        break;
                    case NvMMDigitalZoomStreamIndex_Input:
                        NvOsFprintf(hFile, "Still/Video Input, ");
                        break;
                    case NvMMDigitalZoomStreamIndex_OutputPreview:
                        NvOsFprintf(hFile, "Preview Output, ");
                        break;
                    case NvMMDigitalZoomStreamIndex_OutputStill:
                        NvOsFprintf(hFile, "Still Output, ");
                        break;
                    case NvMMDigitalZoomStreamIndex_OutputVideo:
                        NvOsFprintf(hFile, "Video Output, ");
                        break;
                    default:
                        NvOsFprintf(hFile, "Unknown Stream, ");
                        break;
                }
                break;
            case NvMMBlockType_EncAAC:
                NvOsFprintf(hFile, "EncAAC, ");
                switch(pProfilingData->Entries[i].StreamIndex)
                {
                    case NvMMStreamDir_In:
                        NvOsFprintf(hFile, "Audio Input, ");
                        break;
                    case NvMMStreamDir_Out:
                        NvOsFprintf(hFile, "Audio Output, ");
                        break;
                    default:
                        NvOsFprintf(hFile, "Unknown Stream, ");
                        break;
                }
                break;


            case NvMMBlockTypeForBufferProfiling_3gpAudio:
                NvOsFprintf(hFile, "3GP Audio, ");
                switch(pProfilingData->Entries[i].StreamIndex)
                {
                    case NvMMStreamDir_In:
                        NvOsFprintf(hFile, "Audio Input, ");
                        break;
                    case NvMMStreamDir_Out:
                        NvOsFprintf(hFile, "Audio Output, ");
                        break;
                    default:
                        NvOsFprintf(hFile, "Unknown Stream, ");
                        break;
                }
                break;

            case NvMMBlockTypeForBufferProfiling_3gpVideo:
                NvOsFprintf(hFile, "3GP Video, ");
                switch(pProfilingData->Entries[i].StreamIndex)
                {
                    case NvMMStreamDir_In:
                        NvOsFprintf(hFile, "Video Input, ");
                        break;
                    case NvMMStreamDir_Out:
                        NvOsFprintf(hFile, "Video Output, ");
                        break;
                    default:
                        NvOsFprintf(hFile, "Unknown Stream, ");
                        break;
                }
                break;
            default:
                NvOsFprintf(hFile, "Unknown Block, Unknown Stream, ");
                        break;
        }

        // Event
        switch(pProfilingData->Entries[i].Event)
        {
            case NvMMBufferProfilingEvent_ReceiveBuffer:
                NvOsFprintf(hFile, "ReceiveBuffer, ");
                break;
            case NvMMBufferProfilingEvent_SendBuffer:
                NvOsFprintf(hFile, "SendBuffer, ");
                break;
            case NvMMBufferProfilingEvent_StartProcessing:
                NvOsFprintf(hFile, "StartProcessing, ");
                break;
            default:
                NvOsFprintf(hFile, "UnknownEvent, ");
        }



        // FrameID and TimeStamps (converted to macroseconds)
        NvOsFprintf(hFile, "%d, %u\n", pProfilingData->Entries[i].FrameId,
                    (NvU32)pProfilingData->Entries[i].TimeStamp/10);
    }

    return NvSuccess;

}

// Concurrancy doesn't matter, this is harmless if done twice..
int NvMMIsUsingNewAVP(void)
{
#if NV_USE_NVAVP
    static int snewavp = -1;
    NvAvpHandle hAVP = NULL;

    if (snewavp != -1)
        return snewavp;

    if (NvAvpOpen(&hAVP) != NvSuccess || hAVP == NULL)
        snewavp = 0;
    else
    {
        snewavp = 1;
        NvAvpClose(hAVP);
    }

    return snewavp;
#else
    return 0;
#endif
}

NvError
NvMMVP8Support(void)
{
    NvError err = NvError_VideoDecUnsupportedStreamFormat;
    {
        NvRmDeviceHandle hRmDev;
        static NvU32 VP8Cap = 1;
        NvU32 *chip;
        NvRmModuleID ModuleId = NVRM_MODULE_ID(NvRmModuleID_Vp8, 0);
        NvError e = NvSuccess;

        NvRmModuleCapability caps[] = {
                {1, 0, 0, NvRmModulePlatform_Silicon, (void *)&VP8Cap}
        };

        NvRmOpen(&hRmDev, 0);
        NvVP8FuseSet(hRmDev, 1);

        e = NvRmModuleGetCapabilities(hRmDev, ModuleId, caps, NV_ARRAY_SIZE(caps), (void **)&chip);
        if (e == NvSuccess)
        {
            err = NvSuccess;
        }
        else
        {
            NvOsDebugPrintf("NvMM: VP8 Support is not present \n");
        }
        NvRmClose(hRmDev);
    }
    return err;
}

NvU32 NvMMGetHwChipId()
{
    NvRmDeviceHandle hRmDev;
    static HwChipArch chipArchT30;
    static HwChipArch chipArchT114;
    static HwChipArch chipArchT148;
    static HwChipArch chipArchT124;
    HwChipArch *chip;
    NvRmModuleID ModuleId = NVRM_MODULE_ID(NvRmModuleID_Vde, 0);
    NvError e = NvSuccess;

    NvRmModuleCapability caps[] = {
            {1, 3, 0, NvRmModulePlatform_Silicon, (void *)&chipArchT30},
            {3, 0, 0, NvRmModulePlatform_Silicon, (void *)&chipArchT114},
            {4, 0, 0, NvRmModulePlatform_Silicon, (void *)&chipArchT148},
            {5, 0, 0, NvRmModulePlatform_Silicon, (void *)&chipArchT124},
    };


    chipArchT30.ChipId = HW_ARCH_TEGRA_30;
    chipArchT114.ChipId = HW_ARCH_TEGRA_T114;
    chipArchT148.ChipId = HW_ARCH_TEGRA_T148;
    chipArchT124.ChipId = HW_ARCH_TEGRA_T124;

    NvRmOpen(&hRmDev, 0);

    e = NvRmModuleGetCapabilities(hRmDev, ModuleId, caps, NV_ARRAY_SIZE(caps), (void **)&chip);
    NvRmClose(hRmDev);
    if (e == NvSuccess) {
        return chip->ChipId;
    }
    return 0;
}

