/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef LIBAUDIO_NVAUDIO_AVP_H
#define LIBAUDIO_NVAUDIO_AVP_H

#include <errno.h>
#include <cutils/log.h>
#include <fcntl.h>
#include <sys/stat.h>

typedef void* avp_audio_handle_t;
typedef void* avp_stream_handle_t;

#define AVP_DMA_BUFFER_SIZE     0x1000

#define NON_PCM_INPUT_BUF_SIZE  1024
#define PCM_INPUT_BUF_SIZE      4096

/* alsa pcm config for AVP pcm plyback device */
#define AVP_PCM_CONFIG_PERIOD_SIZE        (AVP_DMA_BUFFER_SIZE >> 2)
#define AVP_PCM_CONFIG_PERIOD_COUNT       1
#define AVP_PCM_CONFIG_START_THRESHOLD    1
#define AVP_PCM_CONFIG_STOP_THRESHOLD     (AVP_PCM_CONFIG_PERIOD_SIZE + 1)

enum {
    STREAM_TYPE_PCM,
    STREAM_TYPE_FAST_PCM,
    STREAM_TYPE_MP3,
    STREAM_TYPE_AAC,
    STREAM_TYPE_LOOPBACK
};

avp_audio_handle_t avp_audio_open(void);
void avp_audio_close(avp_audio_handle_t havp);
void avp_audio_register_dma_trigger_cb(avp_audio_handle_t havp, void *data,
                                   void (*dma_trigger_cb)(void *, void *, int));
void avp_audio_set_ulp(avp_audio_handle_t havp, uint32_t on);
void avp_audio_set_dma_params(avp_audio_handle_t havp, uint32_t ch_id,
                              void *vaddr, uint32_t paddr);
int avp_audio_set_rate(avp_audio_handle_t havp, uint32_t rate);
int avp_audio_dump_loopback_data(avp_audio_handle_t havp,
                                 char *file_name, uint32_t on);
void avp_audio_lock(avp_audio_handle_t havp, uint32_t lock);
void avp_audio_track_audio_latency(avp_audio_handle_t havp, int on);

avp_stream_handle_t avp_stream_open(avp_audio_handle_t havp,
                                    uint32_t stream_type, uint32_t rate);
void avp_stream_close(avp_stream_handle_t hstrm);
int avp_stream_write(avp_stream_handle_t hstrm,
                     const void *buffer, uint32_t bytes);
int avp_stream_stop(avp_stream_handle_t hstrm);
int avp_stream_set_rate(avp_stream_handle_t hstrm, uint32_t rate);
int avp_stream_set_volume(avp_stream_handle_t hstrm, float left, float right);

int avp_stream_flush(avp_stream_handle_t hstrm);
int avp_stream_pause(avp_stream_handle_t hstrm, uint32_t pause);
void avp_stream_set_eos(avp_stream_handle_t hstrm, uint32_t eos);
void avp_stream_seek(avp_stream_handle_t hstrm, int64_t seek_offset);

uint64_t avp_stream_get_position(avp_stream_handle_t hstrm);
uint32_t avp_stream_frame_consumed(avp_stream_handle_t hstrm);
uint32_t avp_stream_get_latency(avp_stream_handle_t hstrm);

#endif // LIBAUDIO_NVAUDIO_AVP_H
