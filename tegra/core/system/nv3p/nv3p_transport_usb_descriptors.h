/*
 * Copyright (c) 2009 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/* This file implements weakly-linked default versions of the ODM query
 * functions for the Nv3P USB transport descriptors, to allow existing
 * clients (e.g., miniloader) to link without needing to weigh them down with
 * the ODM queries.
 */

#include "nvcommon.h"

#ifndef INCLUDED_NV3P_TRANSPORT_USB_DESCRIPTORS_H
#define INCLUDED_NV3P_TRANSPORT_USB_DESCRIPTORS_H

#ifdef __cplusplus
extern "C" 
{
#endif

typedef NvBool(*NvOdmUsbDeviceDescFn)(NvU32 *, NvU32 *, NvU32 *);
typedef NvBool(*NvOdmUsbConfigDescFn)(NvU32 *, NvU32 *, NvU32 *);
typedef NvU8 *(*NvOdmUsbProductIdFn)(void);
typedef NvU8 *(*NvOdmUsbSerialNumFn)(void);

NvOdmUsbDeviceDescFn Nv3pUsbGetDeviceDescFn(void);
NvOdmUsbConfigDescFn Nv3pUsbGetConfigDescFn(void);
NvOdmUsbProductIdFn Nv3pUsbGetProductIdFn(void);
NvOdmUsbSerialNumFn Nv3pUsbGetSerialNumFn(void);

#ifdef __cplusplus
}
#endif

#endif
