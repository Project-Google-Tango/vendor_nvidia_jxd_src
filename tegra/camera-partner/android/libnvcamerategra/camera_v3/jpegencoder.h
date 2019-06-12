/*
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef _JPEG_ENCODER_H_
#define _JPEG_ENCODER_H_

#include <utils/RefBase.h>
#include <utils/Condition.h>
#include <utils/Atomic.h>
#include <nvimage_enc.h>

namespace android
{

class JpegEncoder: public virtual RefBase
{
public:
    JpegEncoder();
    ~JpegEncoder();

public:
    NvError Open(NvU32 levelOfSupport, NvImageEncoderType Type);
    void Close();

    NvError GetParam(NvJpegEncParameters *params);
    NvError SetParam(NvJpegEncParameters *params);

    NvError Encode(NvMMBuffer *InputBuffer, NvMMBuffer *OutPutBuffer,
        NvMMBuffer *thumbBuffer);

protected:
    static void imageEncoderCallback(void* pContext, NvU32 StreamIndex,
        NvU32 BufferSize, void *pBuffer, NvError EncStatus);

private:
    NvOsSemaphoreHandle mJpegSema;
    NvImageEncHandle hImageEnc;
    NvBool mEncoding;
    Condition cond;
    Mutex mutex;
};

};

#endif
