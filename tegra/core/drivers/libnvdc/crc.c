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
#include <sys/ioctl.h>
#include <stdio.h>

#include <linux/tegra_dc_ext.h>

#include <nvdc.h>

#include "nvdc_priv.h"

#define MAX_CRC_PATH 80

static int _openCrcFs(int head, FILE** crc_fs)
{
    char crc_path[MAX_CRC_PATH];
    snprintf(crc_path, MAX_CRC_PATH,
             "/sys/class/graphics/fb%d/device/crc_checksum_latched", head);

    *crc_fs = fopen(crc_path, "r+");

    if (!(*crc_fs)) {
        return errno;
    }

    return 0;
}

static int _enableDisableCrc(struct nvdcState *state, int head, int enabled)
{
    FILE *crc_fs;
    int err;

    if (!NVDC_VALID_HEAD(head)) {
        return EINVAL;
    }

    err = _openCrcFs(head, &crc_fs);

    if (err) {
       return err;
    }

    if (fwrite(enabled ? "1":"0", 1, 1, crc_fs) != 1) {
        fclose(crc_fs);
        return ferror(crc_fs);
    }
    fclose(crc_fs);

    return 0;
}

int nvdcEnableCrc(struct nvdcState *state, int head)
{
    return _enableDisableCrc(state, head, 1);
}

int nvdcDisableCrc(struct nvdcState *state, int head)
{
    return _enableDisableCrc(state, head, 0);
}

int nvdcGetCrc(struct nvdcState *state, int head, unsigned int *crc)
{
    unsigned int c = 0;
    int ret;
    FILE *crc_fs;

    if (!NVDC_VALID_HEAD(head)) {
        return EINVAL;
    }

    ret = _openCrcFs(head, &crc_fs);

    if (ret) {
       return ret;
    }

    if (fscanf(crc_fs, "%u", &c) != 1) {
        fclose(crc_fs);
        if (errno) {
            return errno;
        }
        return ENOENT;
    }
    fclose(crc_fs);

    *crc = c;

    return 0;
}

