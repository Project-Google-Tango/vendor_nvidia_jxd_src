/*
* Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property and
* proprietary rights in and to this software and related documentation.  Any
* use, reproduction, disclosure or distribution of this software and related
* documentation without an express license agreement from NVIDIA Corporation
* is strictly prohibited.
*/

#include <errno.h>
#include <qnx/nvdisp_devctls.h>
#include <nvdc.h>
#include "nvdc_priv.h"

int nvdcEnableCrc(struct nvdcState *state, int head)
{
    int ret = 0;
    bool crc = true;

    if (!NVDC_VALID_HEAD(head) || (state == NULL)) {
        nvdc_error("%s failed: invalid head/NULL ptr\n",__func__);
        return EINVAL;
    }

    ret = devctl(state->dispFd[head], NVDISP_CONFIG_CRC,
                 &crc, sizeof(crc), NULL);
    if (ret != EOK) {
        nvdc_error("%s failed: devctl failure\n",__func__);
        return ret;
    }

    nvdc_info("%s passed \n",__func__);
    return ret;
}

int nvdcDisableCrc(struct nvdcState *state, int head)
{
    int ret = 0;
    bool crc = false;

    if (!NVDC_VALID_HEAD(head) || (state == NULL)) {
        nvdc_error("%s failed: invalid head/NULL ptr\n",__func__);
        return EINVAL;
    }

    ret = devctl(state->dispFd[head], NVDISP_CONFIG_CRC,
                 &crc, sizeof(crc), NULL);
    if (ret != EOK) {
        nvdc_error("%s failed: devctl failure\n",__func__);
        return ret;
    }

    nvdc_info("%s passed \n",__func__);
    return ret;
}

int nvdcGetCrc(struct nvdcState *state, int head, unsigned int *crc)
{
    int ret = 0;

    if (!NVDC_VALID_HEAD(head) || (state == NULL)
        || (crc == NULL)) {
        nvdc_error("%s failed: invalid head/NULL ptr/param\n",__func__);
        return EINVAL;
    }

    ret = devctl(state->dispFd[head], NVDISP_GET_CRC,
                 crc, sizeof(*crc), NULL);
    if (ret != EOK) {
        nvdc_error("%s failed: devctl failure\n",__func__);
        return ret;
    }

    nvdc_info("%s passed \n",__func__);
    return ret;
}
