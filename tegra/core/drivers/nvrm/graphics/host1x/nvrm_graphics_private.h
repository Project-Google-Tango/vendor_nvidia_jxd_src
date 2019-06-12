/*
 * Copyright (c) 2008 - 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */


#ifndef NVRM_GRAPHICS_PRIVATE_H
#define NVRM_GRAPHICS_PRIVATE_H

/**
 * Initialize all graphics stuff
 *
 * @param hDevice The RM instance
 */
NvError
NvRmGraphicsOpen( NvRmDeviceHandle rm );

/**
 * Deinitialize all graphics stuff
 *
 * @param hDevice The RM instance
 */
void
NvRmGraphicsClose( NvRmDeviceHandle rm );

/**
 * Initialize the channels.
 *
 * @param hDevice The RM instance
 */
NvError
NvRmPrivChannelInit( NvRmDeviceHandle hDevice );

/**
 * Deinitialize the channels.
 *
 * @param hDevice The RM instance
 */
void
NvRmPrivChannelDeinit( NvRmDeviceHandle hDevice );

/**
 * Initialize the graphics host, including interrupts.
 */
void
NvRmPrivHostInit( NvRmDeviceHandle rm );

void
NvRmPrivHostShutdown( NvRmDeviceHandle rm );

#endif // NVRM_GRAPHICS_PRIVATE_H
