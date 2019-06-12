/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef __NVDC_PRIV_H__
#define __NVDC_PRIV_H__

#include <nvdc.h>
#include <stdio.h>
#include <sys/slog.h>

#define NVDISP_DEV_PREFIX            "/dev/nvdisp-"
#define NVDISP_WIN_PER_FLIP          (1)

#define NVDC_MAX_HEADS      2
#define NVDC_MAX_WINDOWS    3
#define NVDC_MAX_OUTPUTS    3

#define NVDC_ERROR          0
#define NVDC_WARN           (NVDC_ERROR + 1)
#define NVDC_INFO           (NVDC_WARN + 1)

#define NVDC_LOG_LEVEL      NVDC_ERROR
#define NV_SLOGCODE         0xAAAA

#define SLOG(...)           slogf(_SLOG_SETCODE(NV_SLOGCODE, 0), \
                                  _SLOG_ERROR, ##__VA_ARGS__)

#define NVDC_PRINT(_fmt, ...)  SLOG("nvdisp: " _fmt, ##__VA_ARGS__)

#if NVDC_LOG_LEVEL >= NVDC_INFO
#define nvdc_info(fmt, ...)  NVDC_PRINT(fmt, ##__VA_ARGS__)
#else
#define nvdc_info(fmt, ...)
#endif

#if NVDC_LOG_LEVEL >= NVDC_WARN
#define nvdc_warn(fmt, ...)  NVDC_PRINT(fmt, ##__VA_ARGS__)
#else
#define nvdc_warn(fmt, ...)
#endif

#if NVDC_LOG_LEVEL >= NVDC_ERROR
#define nvdc_error(fmt, ...)  NVDC_PRINT(fmt, ##__VA_ARGS__)
#else
#define nvdc_error(fmt, ...)
#endif

#define NVDISP_HEAD_ENABLED (1 << 0)

static inline int NVDC_VALID_HEAD(int head) {
    return head >= 0 && head < NVDC_MAX_HEADS;
}

struct nvdcDisplay {
    int dcHandle;
    enum nvdcDisplayType type;
};

struct nvdcState {
    int dispFd[NVDC_MAX_HEADS];
    int ctrlFd;
    struct nvdcDisplay displays[NVDC_MAX_OUTPUTS];
    unsigned int numOutputs;
};

struct nvdcEdid {
    int dcHandle;
    void *edid;
    size_t size;
};

int _nvdcInitOutputInfo(struct nvdcState *);

int _nvdcInitCaps(struct nvdcState *);

#endif /* __NVDC_PRIV_H__ */
