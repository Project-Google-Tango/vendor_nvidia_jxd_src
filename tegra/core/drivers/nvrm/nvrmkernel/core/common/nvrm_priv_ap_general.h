/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

 /** @file
  *
  * @b Description: Contains the maximum instance of the controller on soc.
  * Must be >= the max of all chips.
  */

#ifndef INCLUDED_NVRM_PRIV_AP_GENERAL_H
#define INCLUDED_NVRM_PRIV_AP_GENERAL_H


// Dma specific definitions for latest SOC

// Maximum number of DMA channels available on SOC.
#define MAX_APB_DMA_CHANNELS        32


// SPI specific definitions for latest SOC
#define MAX_SPI_CONTROLLERS         8

#define MAX_SLINK_CONTROLLERS       8


// I2C specific definitions for latest soc
#define MAX_I2C_CONTROLLERS         6

#define MAX_DVC_CONTROLLERS         1

#endif // INCLUDED_NVRM_PRIV_AP_GENERAL_H
