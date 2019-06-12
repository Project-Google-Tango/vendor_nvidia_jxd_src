/*
 * Copyright (c) 2014, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVABOOT_MEMLAYOUT_H
#define INCLUDED_NVABOOT_MEMLAYOUT_H

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * NOTE:: "NvTbotMemLayout" enum and "NvAbootCarveoutInfo"
 * defined below should be inilne with the nvtboot definations
 * in NvTboot_MemLayout.h file.
 */

/**
 * Defines the memory layout type to be queried for its base/size
 */
typedef enum
{
    NvTbootMemLayout_none,
    // Lower SDRAM
    NvTbootMemLayout_Primary,
    // Upper SDRAM
    NvTbootMemLayout_Extended,
    NvTbootMemLayout_Vpr,
    NvTbootMemLayout_Tsec,
    NvTbootMemLayout_SecureOs,
    NvTbootMemLayout_Lp0,
    NvTbootMemLayout_Xusb,
    NvTbootMemLayout_Nck,
    NvTbootMemLayout_Debug,
#if ENABLE_NVDUMPER
    NvTbootMemLayout_RamDump,
#endif
    NvTbootMemLayout_Num,
    NvTbootMemLayout_Force32 = 0x7fffffffUL
}NvTbootMemLayout;

/**
 * Tracks the base and size of the Carveout
 */
typedef struct
{
    NvU64 Base;
    NvU64 Size;
}NvAbootCarveoutInfo;

/**
 * Defines the array for Carveout information array.
 * Which will be filled by nvtboot and cpu bootloader
 * reads this object to get base and size of each
 * Carveout region which is allocated.
 */

typedef struct
{
    NvAbootCarveoutInfo  CarInfo[NvTbootMemLayout_Num];
    NvU64 DramCarveoutBottom;
}NvAbootMemInfo;


#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVABOOT_MEMLAYOUT_H

