/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDE_NVBDK_PCB_INTERFACE_H
#define INCLUDE_NVBDK_PCB_INTERFACE_H

#include "common.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct PcbTestDataRec {
    char *type;
    char *name;
    NvBDKTest_TestFunc testFunc;
    void *privateData;
} PcbTestData;

PcbTestData *NvGetPcbTestDataList(void);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDE_NVBDK_PCB_INTERFACE_H
