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

int nvdcGetCursor(struct nvdcState *state, int head)
{
    nvdc_warn("%s unimplemented\n",__func__);
    return ENOANO;
}

int nvdcPutCursor(struct nvdcState *state, int head)
{
    nvdc_warn("%s unimplemented\n",__func__);
    return ENOANO;
}

int nvdcSetCursorImage(struct nvdcState *state, int head,
                       struct nvdcCursorImage *args)
{
    nvdc_warn("%s unimplemented\n",__func__);
    return ENOANO;
}

int nvdcSetCursor(struct nvdcState *state, int head, int x, int y, int visible)
{
    nvdc_warn("%s unimplemented\n",__func__);
    return ENOANO;
}

int nvdcSetCursorImageLowLatency(struct nvdcState *state, int head,
        struct nvdcCursorImage *args, int visiable)
{
    nvdc_warn("%s unimplemented\n",__func__);
    return ENOANO;
}

int nvdcSetCursorLowLatency(struct nvdcState *state, int head, int x, int y,
        struct nvdcCursorImage *args)
{
    nvdc_warn("%s unimplemented\n",__func__);
    return ENOANO;
}

