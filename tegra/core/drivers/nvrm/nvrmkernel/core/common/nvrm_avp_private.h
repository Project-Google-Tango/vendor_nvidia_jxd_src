/*
 * Copyright (c) 2011 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */


#ifndef NVRM_AVP_PRIVATE_H
#define NVRM_AVP_PRIVATE_H

/* Loads the avp executable binary and passes on execution jump
 * address to AVP and does tranport and RPC init.
 */
NvError
NvRmPrivInitAvp(NvRmDeviceHandle hDevice);

/* Shutdown RPC and transport. Should be called when the Rm is shutdown */
void
NvRmPrivDeInitAvp(NvRmDeviceHandle hDevice);

#if !NVRM_TRANSPORT_IN_KERNEL
NvError NvRmPrivTransportInit(NvRmDeviceHandle hRmDevice);
void NvRmPrivTransportDeInit(NvRmDeviceHandle hRmDevice);
#endif

#endif // NVRM_GRAPHICS_PRIVATE_H
