/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <errno.h>
#include <string.h>
#include <qnx/nvdisp_devctls.h>
#include <nvdc.h>
#include "nvdc_priv.h"

int nvdcGetMode(struct nvdcState *state, int head, struct nvdcMode *mode)
{
    int ret;

    if (!NVDC_VALID_HEAD(head) || (mode == NULL) || (state == NULL)) {
        nvdc_error("%s failed: invalid head/NULL ptr\n",__func__);
        return EINVAL;
    }

    ret = devctl(state->dispFd[head], NVDISP_GET_MODE,
                 mode, sizeof(*mode), NULL);
    if (ret != EOK) {
        nvdc_error("%s: failed: devctl failure\n",__func__);
    } else {
        nvdc_info("MODE:\n \
                hActive = %d\n \
                vActive = %d\n \
                hSyncWidth = %d\n \
                vSyncWidth = %d\n \
                hFrontPorch = %d\n \
                vFrontPorch = %d\n \
                hBackPorch = %d\n \
                vBackPorch = %d\n \
                hRefToSync = %d\n \
                vRefToSync = %d\n \
                pclkKHz = %d\n", \
                mode->hActive, \
                mode->vActive, \
                mode->hSyncWidth, \
                mode->vSyncWidth, \
                mode->hFrontPorch, \
                mode->vFrontPorch, \
                mode->hBackPorch, \
                mode->vBackPorch, \
                mode->hRefToSync, \
                mode->vRefToSync, \
                mode->pclkKHz);
    }

    return ret;
}

int nvdcGetModeDB(struct nvdcState *state, int head,
                  struct nvdcMode **modes, int *nmodes)
{
    nvdc_warn("%s unimplemented\n",__func__);
    return ENOANO;
}

int nvdcValidateMode(struct nvdcState *state, int head, struct nvdcMode *mode)
{
    int ret, dret;

    if (!NVDC_VALID_HEAD(head) || (mode == NULL) || (state == NULL)) {
        nvdc_error("%s failed: invalid head/NULL ptr\n",__func__);
        return EINVAL;
    }

    ret = devctl(state->dispFd[head], NVDISP_VALIDATE_MODE,
                 mode, sizeof(*mode), &dret);
    if (ret != EOK) {
        nvdc_error("%s failed: devctl failure\n", __func__);
        return ret;
    }

    return dret;
}

int nvdcSetMode(struct nvdcState *state, int head, struct nvdcMode *mode)
{
    int ret;

    if (!NVDC_VALID_HEAD(head) || (mode == NULL) || (state == NULL)) {
        nvdc_error("%s failed: invalid head/NULL ptr\n",__func__);
        return EINVAL;
    }

    ret = devctl(state->dispFd[head], NVDISP_SET_MODE,
                 mode, sizeof(*mode), NULL);
    if (ret != EOK) {
        nvdc_error("%s failed: devctl failure\n", __func__);
    }

    return ret;
}

int nvdcDPMS(struct nvdcState *state, int head, enum nvdcDPMSmode mode)
{
    nvdc_warn("%s unimplemented\n",__func__);
    return ENOANO;
}

int nvdcQueryHeadStatus(struct nvdcState *state, int head,
                        struct nvdcHeadStatus *nvdcStatus)
{
    unsigned int status = 0;
    int ret;

    if (!NVDC_VALID_HEAD(head) || (state == NULL)) {
        nvdc_error("%s failed: invalid head/NULL ptr\n",__func__);
        return EINVAL;
    }

    ret = devctl(state->dispFd[head], NVDISP_GET_STATUS,
                &status, sizeof(unsigned int), NULL);
    if (ret != EOK) {
        nvdc_error("%s: failed: devctl failure\n",__func__);
        return ret;
    }

    nvdcStatus->enabled = status & NVDISP_HEAD_ENABLED;
    return 0;
}
