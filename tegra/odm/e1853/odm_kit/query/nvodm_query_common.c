/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b>NVIDIA Driver Development Kit:
 *                  ODM machine query interface</b>
 *
 */

#include "nvodm_query.h"
#include "nvmachtypes.h"

NvU32 NvOdmQueryMachine(void)
{
    return MACHINE_TYPE_E1853;
}

NvBool NvOdmQueryPlatformHasSkuSupport(void)
{
    return NV_TRUE;
}
