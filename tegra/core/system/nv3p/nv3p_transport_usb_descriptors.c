/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/* This file implements weakly-linked default versions of the ODM query
 * functions for the Nv3P USB transport descriptors
 */

#include "nvcommon.h"
#include "nv3p_transport_usb_descriptors.h"
#include "nvodm_query_nv3p.h"

NvOdmUsbDeviceDescFn Nv3pUsbGetDeviceDescFn(void)
{
    return NvOdmQueryUsbDeviceDescriptorIds;
}

NvOdmUsbConfigDescFn Nv3pUsbGetConfigDescFn(void)
{
    return NvOdmQueryUsbConfigDescriptorInterface;
}

NvOdmUsbProductIdFn Nv3pUsbGetProductIdFn(void)
{
    return NvOdmQueryUsbProductIdString;
}

NvOdmUsbSerialNumFn Nv3pUsbGetSerialNumFn(void)
{
    return NvOdmQueryUsbSerialNumString;
}
