/*
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_SPI_SOC_H
#define INCLUDED_NVDDK_SPI_SOC_H

#define SPI_INSTANCE_COUNT 4

NvU32 SpiBaseAddress[] =  {
    NV_ADDR_BASE_SPI1,
    NV_ADDR_BASE_SPI2,
    NV_ADDR_BASE_SPI3,
    NV_ADDR_BASE_SPI4
};

#endif // INCLUDED_NVDDK_SPI_SOC_H

