/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * Copyright 2008, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _NV_CAMERA3_ABSTRACTS_H_
#define _NV_CAMERA3_ABSTRACTS_H_

#include <nverror.h>
#include <nvcommon.h>
#include <hardware/camera3.h>


namespace android
{

class AbsCaller;

class AbsCallback
{
    friend class AbsCaller;
public:
    virtual ~AbsCallback() {};

protected:
    virtual void notifyShutterMsg(int, NvU64) = 0;
    virtual void notifyError(
        int, camera3_error_msg_code_t, camera3_stream*) = 0;

    virtual void sendResult(camera3_capture_result*) = 0;
};

class AbsCaller
{
public:
    AbsCaller():mCb(0) {};
    virtual ~AbsCaller() {};

    virtual void registerCallback(AbsCallback *cb) { mCb = cb; };

protected:
    virtual void doSendResult(camera3_capture_result* result)
    {
        mCb->sendResult(result);
    }
    virtual void doNotifyError(
        int frame, camera3_error_msg_code_t err, camera3_stream* stream)
    {
        mCb->notifyError(frame, err, stream);
    }
    virtual void doNotifyShutterMsg(int frame, NvU64 ts)
    {
        mCb->notifyShutterMsg(frame, ts);
    }

private:
    AbsCallback *mCb;
};

} // namespace android

#endif
