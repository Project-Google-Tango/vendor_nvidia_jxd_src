/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef INCLUDED_NVBCT_CUSTOMER_DATA_H
#define INCLUDED_NVBCT_CUSTOMER_DATA_H

/*
 * nvbct_customer_platform_data.h - structure definitions for the BCT customer 
 * platform data area
 */

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct NvBctCustomerPlatformRec
{
    NvU8      CustomerDataVersion;
    NvU8      DevParamsType;
} NvBctCustomerPlatformData;

#if defined(__cplusplus)
}
#endif

#endif

