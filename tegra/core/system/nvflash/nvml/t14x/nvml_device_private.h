/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef INCLUDED_NVML_DEVICE_PRIVATE_H
#define INCLUDED_NVML_DEVICE_PRIVATE_H

#include "nvboot_util_int.h"
#include "nvboot_pads_int.h"
#include "nvboot_hash_int.h"
#include "nvboot_device_int.h"
#include "nvboot_spi_flash_int.h"
#include "nvboot_spi_flash_context.h"
#include "nvboot_fuse_int.h"
#include "nvboot_badblocks.h"
#include "nvboot_sdmmc_int.h"
#include "nvboot_sdmmc_context.h"

typedef union
{
    NvBootSpiFlashContext  SpiFlashContext;
    NvBootSdmmcContext SdmmcContext;
} NvMlDevContext;
#endif//INCLUDED_NVML_DEVICE_PRIVATE_H
