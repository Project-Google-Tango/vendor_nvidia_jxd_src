/*
**
** Copyright 2011, Google Inc.
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

#ifndef NVHDMI_PASSTHRU_PLAYER_H
#define NVHDMI_PASSTHRU_PLAYER_H

#include <SkBitmap.h>

#include <utils/threads.h>

#include <media/HDMIPassthruPlayer.h>
#include <gui/Surface.h>
#include <media/stagefright/MetaData.h>
#include <OMX_CoreExt.h>
#include "omx/AOMXVideoDecoder.h"
#include "MooPlayerDriver.h"

namespace nvidia {

class NvHDMIPassthruPlayer : public googletv::HDMIPassthruPlayer {
  public:
    NvHDMIPassthruPlayer(player_type playerType);
    virtual ~NvHDMIPassthruPlayer();

    sp<android::MooPlayerDriver> mPlayer;

    virtual android::status_t initCheck();
    virtual android::status_t setDataSource(const char *url,
            const android::KeyedVector<android::String8,
            android::String8> *headers);
    virtual android::status_t setDataSource(
            int fd, int64_t offset, int64_t length);
    virtual android::status_t setVideoSurfaceTexture(
            const android::sp<android::ISurfaceTexture>&);
    virtual android::status_t prepareAsync();
    virtual android::status_t start();
    virtual android::status_t stop();
    virtual bool isPlaying();
    virtual android::status_t reset();

    virtual status_t setVideoRectangle(int x, int y, int width, int height);

    virtual android::status_t setVolume(float leftVolume, float rightVolume);

    virtual bool getActiveAudioSubstreamInfo(ActiveAudioSubstreamInfo* info);
    virtual bool getActiveVideoSubstreamInfo(ActiveVideoSubstreamInfo* info);

    static void notify(void* cookie, int msg, int ext1, int ext2, const android::Parcel *obj);

  private:
    static void* staticWorkThreadEntry(void* thiz);
    void* workThreadEntry();
    status_t buildVideoPipeline(
            const sp<android::MetaData> &trackMeta, const sp<android::MetaData> &containerMeta);

  private:
    pthread_t work_thread_;
    bool thread_stop_;
    sp<android::Surface> surface_;
};

}  // namespace nvidia
#endif  // NVHDMI_PASSTHRU_PLAYER_H
