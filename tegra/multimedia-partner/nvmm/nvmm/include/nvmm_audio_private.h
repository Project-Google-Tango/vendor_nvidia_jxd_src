//*****************************************************************************
//*****************************************************************************
//
// Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
//
// NVIDIA Corporation and its licensors retain all intellectual property
// and proprietary rights in and to this software, related documentation
// and any modifications thereto.  Any use, reproduction, disclosure or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA Corporation is strictly prohibited.
//
//*****************************************************************************
//*****************************************************************************




#ifndef _NVMM_AUDIO_PRIVATE_H_
#define _NVMM_AUDIO_PRIVATE_H_


#include "nvmm.h"


//*****************************************************************************
//*****************************************************************************
//
// Functions
//
//*****************************************************************************
//*****************************************************************************

void NvMMAudioSinkClose(NvMMBlockHandle hBlock, NvMMInternalDestructionParameters* pParams);
NvError NvMMAudioSinkOpen(NvMMBlockHandle* phBlock, NvMMInternalCreationParameters* pParams,
                          NvOsSemaphoreHandle semaphore, NvMMDoWorkFunction* pDoWorkFunction);

void NvMMAudioSourceClose(NvMMBlockHandle hBlock, NvMMInternalDestructionParameters* pParams);
NvError NvMMAudioSourceOpen(NvMMBlockHandle* phBlock, NvMMInternalCreationParameters* pParams,
                          NvOsSemaphoreHandle semaphore, NvMMDoWorkFunction* pDoWorkFunction);


#endif
