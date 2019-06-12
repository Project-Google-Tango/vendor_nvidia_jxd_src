/*
** Copyright 2011, Google Inc.
** Copyright (C) 2013 NVIDIA CORPORATION.  All rights reserved.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/**
 * Fake implementation for emulator purposes.
 */

#define LOG_NDEBUG 0
#define LOG_TAG "NVResourceManager"
#include <utils/Log.h>

#include <RMAllocator.h>
#include <RMClient.h>
#include <RMClientListener.h>
#include <RMRequest.h>
#include <RMResponse.h>
#include <RMService.h>
#include <RMTypes.h>

namespace android {

// Defines resource types that are specific to a vendor.
enum nv_resource_types {
    kNvAudioDecoder = 10,
    kNvAudioChannel,
    kNvVideoDecoder,
    kNvVideoPlane,
};

// Defines resources that are specific to a vendor.
enum nv_resources {
    kNvAudioDecoder_1 = 20,
    kNvAudioDecoder_2,
    kNvAudioDecoder_3,
    kNvAudioDecoder_4,
    kNvAudioDecoder_5,
    kNvAudioDecoder_6,
    kNvAudioDecoder_7,
    kNvAudioDecoder_8,
    kNvAudioDecoder_9,
    kNvAudioDecoder_10,
    kNvAudioDecoder_11,
    kNvAudioDecoder_12,
    kNvAudioDecoder_13,
    kNvAudioDecoder_14,
    kNvAudioDecoder_15,
    kNvAudioDecoder_16,
    kNvAudioDecoder_17,
    kNvAudioDecoder_18,
    kNvAudioDecoder_19,
    kNvAudioDecoder_20,
    kNvAudioDecoder_21,
    kNvAudioDecoder_22,
    kNvAudioDecoder_23,
    kNvAudioDecoder_24,
    kNvAudioDecoder_25,
    kNvAudioDecoder_26,
    kNvAudioDecoder_27,
    kNvAudioDecoder_28,
    kNvAudioDecoder_29,
    kNvAudioDecoder_30,
    kNvAudioDecoder_31,
    kNvAudioDecoder_32,
    kNvAudioDecoder_MAX,

    kNvAudioChannel_1,
    kNvAudioChannel_2,
    kNvAudioChannel_3,
    kNvAudioChannel_4,
    kNvAudioChannel_5,
    kNvAudioChannel_6,
    kNvAudioChannel_7,
    kNvAudioChannel_8,
    kNvAudioChannel_MAX,

    kNvVideoDecoder_1,
    kNvVideoDecoder_2,
    kNvVideoDecoder_3,
    kNvVideoDecoder_4,
    kNvVideoDecoder_5,
    kNvVideoDecoder_6,
    kNvVideoDecoder_7,
    kNvVideoDecoder_8,
    kNvVideoDecoder_9,
    kNvVideoDecoder_10,
    kNvVideoDecoder_11,
    kNvVideoDecoder_12,
    kNvVideoDecoder_13,
    kNvVideoDecoder_14,
    kNvVideoDecoder_15,
    kNvVideoDecoder_16,
    kNvVideoDecoder_17,
    kNvVideoDecoder_18,
    kNvVideoDecoder_19,
    kNvVideoDecoder_20,
    kNvVideoDecoder_21,
    kNvVideoDecoder_22,
    kNvVideoDecoder_23,
    kNvVideoDecoder_24,
    kNvVideoDecoder_25,
    kNvVideoDecoder_26,
    kNvVideoDecoder_27,
    kNvVideoDecoder_28,
    kNvVideoDecoder_29,
    kNvVideoDecoder_30,
    kNvVideoDecoder_31,
    kNvVideoDecoder_32,
    kNvVideoDecoder_MAX,

    kNvVideoPlane_1,
    kNvVideoPlane_2,
    kNvVideoPlane_3,
    kNvVideoPlane_4,
    kNvVideoPlane_5,
    kNvVideoPlane_6,
    kNvVideoPlane_7,
    kNvVideoPlane_8,
    kNvVideoPlane_MAX,

};

class NvRMAllocator : public RMAllocator {
public:
    NvRMAllocator() {

        addResourceType(kComponentAudioDecoder, kNvAudioDecoder);
        addResourceType(kComponentAudioRenderer, kNvAudioChannel);
        addResourceType(kComponentVideoDecoder, kNvVideoDecoder);
        addResourceType(kComponentVideoRenderer, kNvVideoPlane);

        for (int i = kNvAudioDecoder_1; i < kNvAudioDecoder_MAX; i++) {
            addResource(kNvAudioDecoder, i);
        }

        for (int i = kNvAudioChannel_1; i < kNvAudioChannel_MAX; i++) {
            addResource(kNvAudioChannel, i);
        }

        for (int i = kNvVideoDecoder_1; i < kNvVideoDecoder_MAX; i++) {
            addResource(kNvVideoDecoder, i);
        }

        for (int i = kNvVideoPlane_1; i < kNvVideoPlane_MAX; i++) {
            addResource(kNvVideoPlane, i);
        }
    }

    // TODO: We can test different policies by implementing the following three
    // functions differently.
    virtual void getCandidates(ResourceType type,
                               const sp<RMRequest> &request,
                               Vector<Resource> *resourceCandidates) {

        // A very simple policy: blindly add all the possible resources
        if (type == kNvAudioChannel) {
            for (int i = kNvAudioChannel_1; i < kNvAudioChannel_MAX; i++) {
                resourceCandidates->push(i);
            }
        } else if (type == kNvAudioDecoder) {
            for (int i = kNvAudioDecoder_1; i < kNvAudioDecoder_MAX; i++) {
                resourceCandidates->push(i);
            }
        } else if (type == kNvVideoPlane) {
            for (int i = kNvVideoPlane_1; i < kNvAudioDecoder_MAX; i++) {
                resourceCandidates->push(i);
            }
        } else if (type == kNvVideoDecoder) {
            for (int i = kNvVideoDecoder_1; i < kNvVideoDecoder_MAX; i++) {
                resourceCandidates->push(i);
            }
        }
    }

    virtual void onResourceReserved(ComponentType compType,
                                    const sp<AMessage> &requestElement,
                                    Resource resourceReserved) {
    }

    virtual void onResourceReturned(ComponentType compType,
                                    const sp<AMessage> &requestElement,
                                    Resource resourceReturned) {
    }
};

void InstantiateGTVResourceManager() {
  ALOGV("InstantiateGTVResourceManager");
  sp<NvRMAllocator> myAllocator = new NvRMAllocator();
  startResourceManagerAsService(myAllocator);
}
}
