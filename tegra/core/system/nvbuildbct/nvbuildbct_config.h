/**
 * Copyright (c) 2007-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * nvbuildbct_config.h - Configuration definitions for the nvbuildbct code.
 */

#ifndef INCLUDED_NVBUILDBCT_CONFIG_H
#define INCLUDED_NVBUILDBCT_CONFIG_H

#if defined(__cplusplus)
extern "C"
{
#endif

#define NVBOOT_AES_BLOCK_SIZE_LOG2 4

#define CMAC_HASH_LENGTH_BYTES    16

#define KEY_LENGTH (128/8)

#define MAX_BUFFER 200
#define MAX_BOOTLOADER_SIZE (16 * 1024 * 1024)
#define NVOSMEDIA_BCT_MAX_PARAM_SETS 2

/**
 * Defines the version of the BCT customer data.
 *
 * Revision history (update whenever nvbuildbct is modified such that the aux
 * data stored in the BCT will change):
 *
 * Rev 1:  Refer NvBctAuxInfo structure in nvbct.h
 */
#define NV_CUSTOMER_DATA_VERSION 1

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBUILDBCT_CONFIG_H */
