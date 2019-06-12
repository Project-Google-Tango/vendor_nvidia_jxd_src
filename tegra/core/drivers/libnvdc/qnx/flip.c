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
#include <qnx/nvdisp_devctls.h>
#include <nvcolor.h>
#include <nvdc.h>
#include <nvassert.h>
#include "nvrm_channel.h"

#include "nvdc_priv.h"

#define NVDISP_FLIP_SEND_LEN (NVDISP_WIN_PER_FLIP \
                             + (NVDISP_WIN_PER_FLIP * NVDC_FLIP_SURF_NUM) \
                             + 1)

int nvdcGetWindow(struct nvdcState *state, int head, int num)
{
    int ret = 0;

    if (!NVDC_VALID_HEAD(head) || (state == NULL)) {
        nvdc_error("%s failed: invalid head/NULL ptr\n",__func__);
        return EINVAL;
    }

    ret = devctl(state->dispFd[head], NVDISP_GET_WINDOW,
                 &num, sizeof(num), NULL);
    if (ret != EOK) {
        nvdc_error("%s failed: devctl failure\n",__func__);
        return ret;
    }

    nvdc_info("%s passed \n",__func__);
    return ret;
}

int nvdcPutWindow(struct nvdcState *state, int head, int num)
{
    int ret = 0;

    if (!NVDC_VALID_HEAD(head) || (state == NULL)) {
        nvdc_error("%s failed: invalid head/NULL ptr\n",__func__);
        return EINVAL;
    }

    ret = devctl(state->dispFd[head], NVDISP_PUT_WINDOW,
                 &num, sizeof(num), NULL);
    if (ret != EOK) {
        nvdc_error("%s failed: devctl failure\n",__func__);
        return ret;
    }

    nvdc_info("%s passed \n",__func__);
    return ret;
}

int nvdcFlip(struct nvdcState *state, int head, struct nvdcFlipArgs *flip)
{
    int ret;
    iov_t send[NVDISP_FLIP_SEND_LEN];
    int i, s_idx = 0;
    iov_t recv[1];
    NvRmFence fence;

    if (!NVDC_VALID_HEAD(head) || (state == NULL) || (flip == NULL)) {
        nvdc_error("%s failed: not a valid head/NULL ptr\n",__func__);
        return EINVAL;
    }

    if (flip->numWindows > NVDISP_WIN_PER_FLIP) {
        nvdc_error("%s supports only %d window per flip call\n", __func__,
                NVDISP_WIN_PER_FLIP);
        return EINVAL;
    }

    flip->numWindows = NV_MIN(flip->numWindows, NVDISP_WIN_PER_FLIP);

    SETIOV(&send[s_idx], flip, sizeof(*flip));
    s_idx++;

    for (i = 0; i < flip->numWindows; i++) {
        int j;
        SETIOV(&send[s_idx], &flip->win[i], sizeof(struct nvdcFlipWinArgs));
        s_idx++;

        for (j = 0; j < NVDC_FLIP_SURF_NUM; j++) {
            if (flip->win[i].surfaces[j]) {
                SETIOV(&send[s_idx], flip->win[i].surfaces[j],
                        sizeof(NvRmSurface));
                s_idx++;
            }
            /*
             * NVDISP-NOTE: If the first surface is NULL, do not care about
             * other surfaces.
             */
            else if (j == 0) {
                break;
            }
        }
    }
    SETIOV(recv, &fence, sizeof(NvRmFence));

    ret = devctlv(state->dispFd[head], NVDISP_FLIP, s_idx,
                  NV_ARRAY_SIZE(recv), send, recv, NULL);
    if (ret != EOK) {
        nvdc_error("%s failed: devctl failure\n",__func__);
        return ret;
    }

    flip->postSyncptId = fence.SyncPointID;
    flip->postSyncptValue = fence.Value;
    return 0;
}
