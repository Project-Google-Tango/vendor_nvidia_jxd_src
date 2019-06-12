/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 *
 * nvaudioalsawfd.cpp
 *
 * Extend the Binder interface to create a service that receives and serves
 * messages.
 *
 */

#define LOG_TAG "NvAudioALSAWfd"
//#define LOG_NDEBUG 0

#include "nvaudioalsa.h"

using namespace android;

static void NvAudioALSAWfdClose(NvAudioALSADev *hDev)
{
    ALOGV("NvAudioWFDClose");

    NvCapAudioBuffer *hpcm = (NvCapAudioBuffer*)hDev->hpcm;

    if (hpcm) {
        hpcm->serverState = NVCAP_AUDIO_STATE_STOP;
        hDev->hpcm = 0;
    }
}

static status_t NvAudioALSAWfdOpen(NvAudioALSADev *hDev,
                                   uint32_t devices, int mode)
{
    ALOGV("NvAudioALSAWfdOpen");

    NvCapAudioBuffer *hpcm = NULL;

    AutoMutex lock(hDev->device->m_Lock);

    if (hDev->hpcm)
        return NO_ERROR;
    else if (hDev->device->sharedMem == NULL)
       return NO_INIT;

    hpcm = (NvCapAudioBuffer*)hDev->device->sharedMem;

    hDev->periods = TEGRA_DEFAULT_OUT_PERIODS;
    hDev->bufferSize = hpcm->size;
    hDev->latency = (uint32_t)(((uint64_t)hDev->bufferSize * 1000000) /
                    (hDev->sampleRate * hDev->channels *
                    ((hDev->format == SND_PCM_FORMAT_S16_LE) ? 2 : 1)));
    hDev->hpcm = (void*)hpcm;

    return NO_ERROR;
}

static size_t getBuffer(NvAudioALSADev *hDev, void** shareBuffer, size_t bytes)
{
    NvCapAudioBuffer *hpcm = (NvCapAudioBuffer *)hDev->hpcm;
    uint32_t avail = 0;
    uint32_t u = hpcm->user;
    uint32_t s = hpcm->server;
    uint32_t bufSize = hpcm->size;
    uint8_t* sharedBufferBase = (uint8_t*)hpcm + hpcm->offset;
    uint32_t waitCount = 0;

    // ALOGV("getBuffer: %d bytes u=%d s=%d", bytes, hpcm->user, hpcm->server);

    while(1) {
        avail = hpcm->isEmpty ? bufSize :
                    ((s > u) ? (bufSize - s + u) : (u - s));
        avail = avail > bytes ? bytes : avail;

        if (avail == 0) {
            if (waitCount > hDev->periods) {
                ALOGE("WFD stack is not consuming data");
                return 0;
            }
            usleep(hDev->latency / hDev->periods);
            u = hpcm->user;
            waitCount++;
            continue;
        } else if ((s + avail) > bufSize) {
            *shareBuffer = &sharedBufferBase[s];
            return (bufSize - s);
        } else {
            *shareBuffer = &sharedBufferBase[s];
            return avail;
        }
    }
}

static ssize_t NvAudioALSAWfdWrite(NvAudioALSADev *hDev,
                                   void* buffer, size_t bytes)
{
    //ALOGV("NvAudioWFDWrite %d bytes", bytes);

    if (hDev->hpcm == NULL) {
        ALOGE("NvAudioALSAWfdWrite: Device has not opened yet\n");
        return NO_INIT;
    }

    NvCapAudioBuffer *hpcm = (NvCapAudioBuffer *)hDev->hpcm;
    void *shareBuffer = NULL;
    uint32_t avail = 0;
    uint32_t bytesWritten = 0;
    uint32_t s = hpcm->server;

    if (hpcm->serverState != NVCAP_AUDIO_STATE_RUN)
        hpcm->serverState = NVCAP_AUDIO_STATE_RUN;

    do {
        avail = getBuffer(hDev, &shareBuffer, bytes);
        if (avail <= 0)
            return bytesWritten;

        memcpy(shareBuffer, (uint8_t*)buffer + bytesWritten, avail);
        s += avail;
        s = (s >= hpcm->size) ? (s - hpcm->size) : s;
        bytes -= avail;
        bytesWritten += avail;
        avail = 0;
        hpcm->server = s;
        hpcm->isEmpty = 0;
    } while (bytes > 0);

    return bytesWritten;
}

static status_t NvAudioALSAWfdRoute(NvAudioALSADev *hDev,
                                    uint32_t devices, int mode)
{
    ALOGV("NvAudioALSAWfdRoute: device %d mode %d", devices, mode);
    if (devices & hDev->validDev)
        hDev->curDev = devices & hDev->validDev;

    return 0;
}

static int tegra_wfd_close(hw_device_t* device)
{
    free(device);
    return 0;
}

static int tegra_wfd_open(const hw_module_t* module, const char* name,
                          hw_device_t** device)
{
    ALOGV("tegra_wfd_open: IN");

    struct tegra_alsa_device_t *dev;

    dev = (struct tegra_alsa_device_t*)malloc(sizeof(*dev));
    if (!dev)
        return -ENOMEM;

    memset(dev, 0, sizeof(*dev));

    /* initialize the procs */
    dev->common.tag     = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module  = (hw_module_t *) module;
    dev->common.close   = tegra_wfd_close;
    dev->open           = NvAudioALSAWfdOpen;
    dev->close          = NvAudioALSAWfdClose;
    dev->route          = NvAudioALSAWfdRoute;
    dev->write          = NvAudioALSAWfdWrite;

    *device = &dev->common;

    return 0;

}

struct hw_module_methods_t tegra_wfd_module_methods = {
    /** Open a specific device */
    open : tegra_wfd_open,
};

extern "C" const struct hw_module_t TEGRA_WFD_MODULE = {
            tag              : (uint32_t )HARDWARE_MODULE_TAG,
            version_major    : 1,
            version_minor    : 0,
            id               : TEGRA_ALSA_HARDWARE_MODULE_ID,
            name             : "NVIDIA Tegra WFD audio module",
            author           : "NVIDIA",
            methods          : &tegra_wfd_module_methods,
            dso              : 0,
            reserved         : { 0, },
};
