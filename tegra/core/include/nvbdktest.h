/*
 * Copyright (c) 2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVBDKTEST_H
#define INCLUDED_NVBDKTEST_H

#if defined(__cplusplus)
extern "C"
{
#endif


/**
 * Defines the max no. of suites
 */
#define MAX_SUITE_NO 10

/**
 * Defines the maximum length of a string, including null terminator.
 */
#define NV3P_STRING_MAX (64)

/**
 * Prototype for free function pointer for private data
 */
typedef void (*NvBDKTest_FreePrivateFunc)(void *);

/**
 * Holds PrivData structure for NvBdkTestserver Communication
 */
typedef struct NvBDKTest_TestPrivDataRec
{
    NvU32 Instance;
    NvU32 Mem_address;
    NvU32 Reserved;
    NvU32 File_size;
    NvU8 *File_buff;
    void *pData;
    NvBDKTest_FreePrivateFunc pFreeDataFunc;
} NvBDKTest_TestPrivData;

/**
 * Holds PrivData structure for each suites
 */
typedef struct NvBDKTest_ArgRec
{
    char suite[NV3P_STRING_MAX];
    NvBDKTest_TestPrivData arg_data;
} NvBDkTest_arg;


#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVBDKTEST_H
