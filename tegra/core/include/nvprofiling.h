/*
 * Copyright (c) 2012-2013 NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
#ifndef INCLUDED_PROFILING_H
#define INCLUDED_PROFILING_H

#if defined(__cplusplus)
extern "C"
{
#endif

//declaring a structure whose variables are used for profiling the bootloader
typedef struct BootloaderProfilingRec
{
    NvU32 MainStart;
    NvU32 BlockDevMgr_Init;
    NvU32 PartMgr_Init;
    NvU32 StorMgr_Init;
    NvU32 FsMgr_Init;
    NvU32 End_FsMgr;
    NvU32 PartitiontableLoad;
    NvU32 Start_Display;
    NvU32 End_Display;
    NvU32 Start_KernelRead;
    NvU32 End_KernelRead;
    NvU32 Kernel_Size;
    NvU32 ReadSpeed;
    NvU32 PreKernel_Jump;
} BootloaderProfiling;

typedef struct MicroBootProfilingRec
{
    NvU32 LoadBl_Start;
    NvU32 LoadBl_End;
    NvU32 LoadBl_Time;
    NvU32 LoadBl_Size;
    NvU32 DramInit_Done;
    NvU32 PreBootloader_Jump;
} MicroBootProfiling;

#if defined(__cplusplus)
}
#endif

#endif

