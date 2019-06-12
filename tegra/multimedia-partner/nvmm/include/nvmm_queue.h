/*
 * Copyright (c) 2006 - 2007 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVMM_QUEUE_H
#define INCLUDED_NVMM_QUEUE_H

#include "nvcommon.h"
#include "nverror.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct NvMMQueueRec *NvMMQueueHandle;

NvError NvMMQueueCreate(NvMMQueueHandle *phQueue, NvU32 MaxEntries, NvU32 EntrySize, NvBool ThreadSafe);
NvError NvMMQueueEnQ(NvMMQueueHandle hQueue, void *pElem, NvU32 Timeout);
NvError NvMMQueueDeQ(NvMMQueueHandle hQueue, void *pElem);
NvError NvMMQueuePeek(NvMMQueueHandle hQueue, void *pElem);
NvError NvMMQueuePeekEntry(NvMMQueueHandle hNvMMQueue, void *pElem, NvU32 nEntry);
NvError NvMMQueueInsertHead(NvMMQueueHandle hQueue, void *pElem, NvU32 uTimeout);
void NvMMQueueDestroy(NvMMQueueHandle *phQueue);
NvU32 NvMMQueueGetNumEntries(NvMMQueueHandle hQueue);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVMM_QUEUE_H
