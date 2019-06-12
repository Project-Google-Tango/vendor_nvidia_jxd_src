/*
* Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

/**
* @file
* @brief <b>nVIDIA Driver Development Kit:
*           JPEG Encode control</b>
*
* @b Description: Defines the NvMM interfaces to the JPEG encoder.
*/

#ifndef INCLUDED_NVMM_JPEGENCBLOCK_H
#define INCLUDED_NVMM_JPEGENCBLOCK_H

#if defined(__cplusplus)
extern "C" {
#endif
#include "nvmm.h"
#include "nvmm_event.h"
#include "nvmm_queue.h"
#include "nvmm_util.h"
#include "nvmm_block.h"
#define JPEGENC_MAX_BUFFER_SIZE 1026*1024
#define JPEGENC_MIN_BUFFER_SIZE 1024
#define JPEGENC_MAX_INPUT_BUFFERS 4
#define JPEGENC_MIN_INPUT_BUFFERS 3
#define JPEGENC_MAX_OUTPUT_BUFFERS 4
#define JPEGENC_MIN_OUTPUT_BUFFERS 1
#define JPEGENC_OUTPUT_BUFFER_BYTE_ALIGNMENT 1024
#define JPEGENC_INPUT_BUFFER_BYTE_ALIGNMENT 256


    typedef enum
    {
        JPEGENC_INPUT_STREAM = 0,
        JPEGENC_OUTPUT_STREAM,
        JPEGENC_THUMBNAIL_INPUT_STREAM
    }JPEG_STREAM;


    /**
    * NvMM Block context for JPEG encoder
    */
    typedef struct NvJPEGEncBlockContext_ {
        NvMMBlockContext BaseClassCtx;
        void *pJPEGEncCtx;
    }NvJPEGEncBlockContext;


    NvError 
        NvMMJPEGEncDoWork(
        NvMMBlockHandle hBlock,
        NvMMDoWorkCondition Flag
        );


    NvError
        NvMMJPEGEncSetAttribute(
        NvMMBlockHandle hBlock, 
        NvU32 eAttributeType, 
        NvU32 SetAttrFlag,
        NvU32 AttributeSize,
        void *pAttribute);

    NvError
        NvMMJPEGEncGetAttribute(
        NvMMBlockHandle hBlock,
        NvU32 eAttributeType, 
        NvU32 AttributeSize,
        void *pAttribute);

    NvError 
        NvMMJPEGEncBlockOpen(
        NvMMBlockHandle *phBlock,
        NvMMInternalCreationParameters *pParams,
        NvOsSemaphoreHandle semaphore,
        NvMMDoWorkFunction *pDoWorkFunction);

    void
        NvMMJPEGEncBlockClose(
        NvMMBlockHandle hBlock,
        NvMMInternalDestructionParameters *pParams);

    NvError NvMMJPEGEncEVentHandler(void *hBlock, 
        NvU32 StreamIndex, 
        NvMMBufferType BufferType, 
        NvU32 BufferSize, 
        void* pBuffer, 
        NvBool *bEventHandled);

#if defined(__cplusplus)
}
#endif

#endif 
