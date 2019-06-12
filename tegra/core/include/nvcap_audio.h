/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * nvcap_audio.h
 */

#ifndef INVCAP_AUDIO_H_
#define INVCAP_AUDIO_H_

#define FRAMEWORK_HAS_WIFI_DISPLAY_SUPPORT

typedef enum
{
    NVCAP_AUDIO_STATE_INIT = 0,
    NVCAP_AUDIO_STATE_RUN,
    NVCAP_AUDIO_STATE_STOP,
} NvCapAudioState;

typedef enum
{
    NVCAP_AUDIO_STREAM_FORMAT_NONE = 0,
    NVCAP_AUDIO_STREAM_FORMAT_LPCM,
    NVCAP_AUDIO_STREAM_FORMAT_AAC,
    NVCAP_AUDIO_STREAM_FORMAT_AC3
} NvCapAudioStreamFormat;

typedef struct
{
    NvCapAudioState serverState;
    uint32_t server;
    uint32_t user;
    uint32_t isEmpty;
    uint32_t size;
    uint32_t offset;
    uint64_t timeStampStart;
} NvCapAudioBuffer;

#endif
