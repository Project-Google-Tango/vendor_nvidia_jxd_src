/*
 * Copyright (c) 2010-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef CODEC_H
#define CODEC_H

#include "nvos.h"

/**
 * Codec mode
 */
typedef enum tagCodecMode
{
    CodecMode_None    = 0,
    CodecMode_I2s1    = 1,
    CodecMode_I2s2    = 2,
    CodecMode_Tdm1    = 3,
    CodecMode_Tdm2    = 4,
	CodecMode_Force32 = 0x7FFFFFFF
} CodecMode;

/**
 * Open Codec
 */
NvError
CodecOpen(
    int fd,
    CodecMode mode,
    int sampleRate,
	int sku_id
    );

/**
 * Close Codec
 */
void
CodecClose(
    int fd
    );

/**
 * Dump Codec Registers
 */
NvError
CodecDump(
    int fd
    );

#endif /* CODEC_H */
