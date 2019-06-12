/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVOS_ALLOC_H
#define INCLUDED_NVOS_ALLOC_H

#include "nvos_debug.h"

#if NVOS_RESTRACKER_COMPILED
/**
 * Gets the size of the given allocation.
 * \param  ptr  Pointer as given out by NvOsAllocLeak etc.
 * \return Zero on failure, size otherwise.
 */
size_t NvOsGetAllocSize(void* ptr);
#endif

#endif // INCLUDED_NVOS_ALLOC_H
