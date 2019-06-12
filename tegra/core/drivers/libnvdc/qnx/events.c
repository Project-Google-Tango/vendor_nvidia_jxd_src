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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <nvdc.h>
#include "nvdc_priv.h"

int nvdcEventFds(struct nvdcState *state, int **pFds, int *numFds)
{
    nvdc_warn("%s unimplemented\n",__func__);
    return ENOANO;
}

int nvdcEventData(struct nvdcState *state, int fd)
{
    nvdc_warn("%s unimplemented\n",__func__);
    return ENOANO;
}

int nvdcEventHotplug(struct nvdcState *state, nvdcEventHotplugHandler handler)
{
    nvdc_warn("%s unimplemented\n",__func__);
    return ENOANO;
}

int nvdcEventModeChange(struct nvdcState *state,
                        nvdcEventModeChangeHandler handler)
{
    nvdc_warn("%s unimplemented\n",__func__);
    return ENOANO;
}

int nvdcInitAsyncEvents(struct nvdcState *state)
{
    nvdc_warn("%s unimplemented\n",__func__);
    return ENOANO;
}
