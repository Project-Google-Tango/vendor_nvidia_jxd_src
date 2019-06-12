/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <errno.h>
#include <nvdc.h>
#include "nvdc_priv.h"

/*
 * Query DC capabilities.  Called once at startup,
 */
int _nvdcInitCaps(struct nvdcState *state)
{
    /* FIXME: Not implemented/used on QNX.*/
    return 0;
}


int nvdcGetCapabilities(struct nvdcState *state,
                        unsigned int *nvdcCaps)
{
    nvdc_warn("%s unimplemented\n",__func__);
    return 0;
}
