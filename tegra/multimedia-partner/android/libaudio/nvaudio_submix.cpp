/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */
/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "nvaudio_submix"
//#define LOG_NDEBUG 0

#include "nvaudio.h"

#include <media/nbaio/MonoPipe.h>
#include <media/nbaio/MonoPipeReader.h>
#include <media/AudioBufferProvider.h>

#define MAX_PIPE_DEPTH_IN_FRAMES     (1024*8)
// The duration of MAX_READ_ATTEMPTS * READ_ATTEMPT_SLEEP_MS must be stricly inferior to
//   the duration of a record buffer at the current record sample rate (of the device, not of
//   the recording itself). Here we have:
//      3 * 5ms = 15ms < 1024 frames * 1000 / 48000 = 21.333ms
#define MAX_READ_ATTEMPTS            3
#define READ_ATTEMPT_SLEEP_MS        5 // 5ms between two read attempts when pipe is empty

struct nvaudio_submix
{
    pthread_mutex_t                         submix_lock;
    android::sp<android::MonoPipe>          rsx_sink;
    android::sp<android::MonoPipeReader>    rsx_source;
    uint32_t                                rsx_rate;
    uint32_t                                rsx_channel_count;
    struct resampler_itfe                   *rsx_resampler;
    int16_t                                 *rsx_resample_buffer;
    bool                                    rsx_standby;
};

/* use emulated popcount optimization
   http://www.df.lth.se/~john_e/gems/gem002d.html */
static inline uint32_t pop_count(uint32_t u)
{
    u = ((u&0x55555555) + ((u>>1)&0x55555555));
    u = ((u&0x33333333) + ((u>>2)&0x33333333));
    u = ((u&0x0f0f0f0f) + ((u>>4)&0x0f0f0f0f));
    u = ((u&0x00ff00ff) + ((u>>8)&0x00ff00ff));
    u = ( u&0x0000ffff) + (u>>16);
    return u;
}

bool nvaudio_is_submix_on(struct audio_hw_device *dev)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;

    pthread_mutex_lock(&hw_dev->submix->submix_lock);
    bool submix_on = hw_dev->submix->rsx_sink.get() != 0;
    pthread_mutex_unlock(&hw_dev->submix->submix_lock);

    return submix_on;
}

void nvaudio_in_standby_submix(struct audio_stream *stream)
{
    struct nvaudio_stream_in *in = (struct nvaudio_stream_in *)stream;

    ALOGE("submix standby+");
    pthread_mutex_lock(&in->hw_dev->submix->submix_lock);
    pthread_mutex_lock(&in->lock);
    if(in->hw_dev->submix->rsx_resampler) {
        in->hw_dev->submix->rsx_resampler->reset(in->hw_dev->submix->rsx_resampler);
    }
    in->standby = true;
    in->hw_dev->submix->rsx_standby = true;
    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&in->hw_dev->submix->submix_lock);
    ALOGE("submix standby-");
    return;
}

ssize_t nvaudio_out_write_submix(struct audio_stream_out *stream, const void* buffer, size_t bytes, int rate)
{
    //ALOGV("submix out_write(bytes=%d)", bytes);
    ssize_t written_frames = 0;
    struct  nvaudio_stream_out *out = reinterpret_cast<struct  nvaudio_stream_out *>(stream);
    ssize_t size;
    int ret;
    void *bufptr = (void *)buffer;

    const size_t frame_size = audio_stream_frame_size(&stream->common);
    size_t in_frames = bytes / frame_size;
    size_t out_frames = in_frames;

    if(out->hw_dev->submix->rsx_standby) return 0;

    pthread_mutex_lock(&out->lock);

    if(out->hw_dev->submix->rsx_sink.get() == 0) {
        //ALOGV("No submix device");
        pthread_mutex_unlock(&out->lock);
        return 0;
    }

    android::sp<android::MonoPipe> sink = out->hw_dev->submix->rsx_sink.get();
    if (sink != 0) {
        if (sink->isShutdown()) {
            ALOGE("out_write submix sink shutdown");
            sink.clear();
            pthread_mutex_unlock(&out->lock);
            // the pipe has already been shutdown, this buffer will be lost but we must
            //   simulate timing so we don't drain the output faster than realtime
            usleep(out_frames * 1000000 / rate);
            return bytes;
        }
    } else {
        pthread_mutex_unlock(&out->lock);
        //ALOGV("out_write without a remote submix in pipe");
        return 0;
    }
	ALOGV("submix out_write(rate=%d rsx_rate=%d)", rate,out->hw_dev->submix->rsx_rate);
    if(rate != out->hw_dev->submix->rsx_rate) {
        if(out->hw_dev->submix->rsx_resampler == NULL) {
            ALOGV("submix create resampler due to submix input rate %d != dev output rate %d", out->hw_dev->submix->rsx_rate, rate);
            ret = create_resampler(rate,
                                   out->hw_dev->submix->rsx_rate,
                                   out->channel_count,
                                   RESAMPLER_QUALITY_DEFAULT,
                                   NULL,
                                   &out->hw_dev->submix->rsx_resampler);
            out->hw_dev->submix->rsx_resample_buffer = (int16_t *)malloc(MAX_PIPE_DEPTH_IN_FRAMES * frame_size);
        }
        out_frames = in_frames * out->hw_dev->submix->rsx_rate / rate + 1;
        //ALOGV("submix resample for submix in frames = %d out frames = %d", in_frames, out_frames);
        ret = out->hw_dev->submix->rsx_resampler->resample_from_input(
                    out->hw_dev->submix->rsx_resampler,
                    (int16_t*)buffer,
                    &in_frames,
                    (int16_t*)out->hw_dev->submix->rsx_resample_buffer,
                    &out_frames);
        bufptr = out->hw_dev->submix->rsx_resample_buffer;
    }

    pthread_mutex_unlock(&out->lock);

    written_frames = sink->write(bufptr, out_frames);

    if (written_frames < 0) {
        if (written_frames == (ssize_t)android::NEGOTIATE) {
            ALOGE("out_write() write to pipe returned NEGOTIATE");

            pthread_mutex_lock(&out->lock);
            sink.clear();
            pthread_mutex_unlock(&out->lock);

            written_frames = 0;
            return 0;
        } else {
            // write() returned UNDERRUN or WOULD_BLOCK, retry
            ALOGE("out_write() write to pipe returned unexpected %16i", written_frames);
            written_frames = sink->write(bufptr, out_frames);
        }
    }

    pthread_mutex_lock(&out->lock);
    sink.clear();
    pthread_mutex_unlock(&out->lock);

    if (written_frames < 0) {
        ALOGE("out_write() failed writing to pipe with %16i", written_frames);
        return 0;
    } else {
        //ALOGV("out_write() wrote %lu bytes)", written_frames * frame_size);
        return written_frames * frame_size;
    }
}

ssize_t nvaudio_in_read_submix(struct audio_stream_in *stream, void* buffer, size_t bytes)
{
    //ALOGV("submix in_read bytes=%u", bytes);

    ssize_t frames_read = -1977;
    struct nvaudio_stream_in *in = reinterpret_cast<struct nvaudio_stream_in *>(stream);
    const size_t frame_size = audio_stream_frame_size(&stream->common);
    const size_t frames_to_read = bytes / frame_size;

    if(in->hw_dev->submix->rsx_standby) return 0;

    pthread_mutex_lock(&in->lock);

    if(in->hw_dev->submix->rsx_sink.get() == 0) {
        //ALOGV("No submix device");
        pthread_mutex_unlock(&in->lock);
        return 0;
    }

    if (in->standby) {
        in->standby = false;
        // keep track of when we exit input standby (== first read == start "real recording")
        // or when we start recording silence, and reset projected time
        int rc = clock_gettime(CLOCK_MONOTONIC, &in->record_start_time);
        if (rc == 0) {
            in->read_counter_frames = 0;
        }
    }

    in->read_counter_frames += frames_to_read;
    size_t remaining_frames = frames_to_read;

    {
        // about to read from audio source
        android::sp<android::MonoPipeReader> source = in->hw_dev->submix->rsx_source.get();
        if (source == 0) {
            ALOGE("no audio pipe yet we're trying to read!");
            pthread_mutex_unlock(&in->lock);
            usleep((bytes * 1000000) / (frame_size * in->rate));
            memset(buffer, 0, bytes);
            return bytes;
        }

        pthread_mutex_unlock(&in->lock);

        // read the data from the pipe (it's non blocking)
        int attempts = 0;
        char* buff = (char*)buffer;

        while ((remaining_frames > 0) && (attempts < MAX_READ_ATTEMPTS)) {
            attempts++;
            frames_read = source->read(buff, remaining_frames, android::AudioBufferProvider::kInvalidPTS);
            if (frames_read > 0) {
                remaining_frames -= frames_read;
                buff += frames_read * frame_size;
                //ALOGV("  in_read (att=%d) got %ld frames, remaining=%u",attempts, frames_read, remaining_frames);
            } else {
                //ALOGE("  in_read read returned %ld", frames_read);
                usleep(READ_ATTEMPT_SLEEP_MS * 1000);
            }
        }

        // done using the source
        pthread_mutex_lock(&in->lock);
        source.clear();
        pthread_mutex_unlock(&in->lock);
    }

    if (remaining_frames > 0) {
        //ALOGV("  remaining_frames = %d", remaining_frames);
        memset(((char*)buffer)+ bytes - (remaining_frames * frame_size), 0,
                remaining_frames * frame_size);
    }


    // compute how much we need to sleep after reading the data by comparing the wall clock with
    //   the projected time at which we should return.
    struct timespec time_after_read;// wall clock after reading from the pipe
    struct timespec record_duration;// observed record duration
    int rc = clock_gettime(CLOCK_MONOTONIC, &time_after_read);
    const uint32_t sample_rate = in->rate;

    if (rc == 0) {
        // for how long have we been recording?
        record_duration.tv_sec  = time_after_read.tv_sec - in->record_start_time.tv_sec;
        record_duration.tv_nsec = time_after_read.tv_nsec - in->record_start_time.tv_nsec;
        if (record_duration.tv_nsec < 0) {
            record_duration.tv_sec--;
            record_duration.tv_nsec += 1000000000;
        }

        // read_counter_frames contains the number of frames that have been read since the beginning
        // of recording (including this call): it's converted to usec and compared to how long we've
        // been recording for, which gives us how long we must wait to sync the projected recording
        // time, and the observed recording time
        long projected_vs_observed_offset_us =
                ((int64_t)(in->read_counter_frames
                            - (record_duration.tv_sec*sample_rate)))
                        * 1000000 / sample_rate
                - (record_duration.tv_nsec / 1000);

        //ALOGV("  record duration %5lds %3ldms, will wait: %7ldus",
        //        record_duration.tv_sec, record_duration.tv_nsec/1000000,
        //        projected_vs_observed_offset_us);
        if (projected_vs_observed_offset_us > 0) {
            usleep(projected_vs_observed_offset_us);
        }
    }


    //ALOGV("submix in_read returns %d", bytes);
    return bytes;

}

void nvaudio_dev_open_input_stream_submix(struct audio_hw_device *dev, struct audio_config *config)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;

    ALOGE("submix open_input+");
    pthread_mutex_lock(&hw_dev->submix->submix_lock);

    const android::NBAIO_Format format = android::Format_from_SR_C(config->sample_rate, 2);
    const android::NBAIO_Format offers[1] = {format};
    size_t numCounterOffers = 0;
    // creating a MonoPipe with optional blocking set to true.
    android::MonoPipe* sink = new android::MonoPipe(MAX_PIPE_DEPTH_IN_FRAMES, format, true/*writeCanBlock*/);
    ssize_t index = sink->negotiate(offers, 1, NULL, numCounterOffers);
    ALOG_ASSERT(index == 0);
    android::MonoPipeReader* source = new android::MonoPipeReader(sink);
    numCounterOffers = 0;
    index = source->negotiate(offers, 1, NULL, numCounterOffers);
    ALOG_ASSERT(index == 0);

    hw_dev->submix->rsx_sink = sink;
    hw_dev->submix->rsx_source = source;
    hw_dev->submix->rsx_rate = config->sample_rate;
    hw_dev->submix->rsx_channel_count = pop_count(config->channel_mask);
    hw_dev->submix->rsx_resampler = NULL;
    hw_dev->submix->rsx_resample_buffer = NULL;
    hw_dev->submix->rsx_standby = false;

    pthread_mutex_unlock(&hw_dev->submix->submix_lock);
    ALOGV("submix open_input rsx_rate=%d",config->sample_rate);
    return;
}

void nvaudio_dev_close_input_stream_submix(struct audio_hw_device *dev)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;

    ALOGE("submix close_input+");

    pthread_mutex_lock(&hw_dev->submix->submix_lock);

    if(hw_dev->submix->rsx_resampler) {
        ALOGV("submix free resampler for submix in");
        release_resampler(hw_dev->submix->rsx_resampler);
        free(hw_dev->submix->rsx_resample_buffer);
    }

    hw_dev->submix->rsx_sink.clear();
    hw_dev->submix->rsx_source.clear();

    android::MonoPipe* sink = hw_dev->submix->rsx_sink.get();
    if (sink != NULL) {
        ALOGI("shutdown");
        sink->shutdown(true);
    }
    pthread_mutex_unlock(&hw_dev->submix->submix_lock);
    ALOGE("submix close_input-");
}

void nvaudio_dev_open_submix(struct audio_hw_device *dev)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;

    ALOGE("submix open");

    hw_dev->submix = new nvaudio_submix;
    pthread_mutex_init(&hw_dev->submix->submix_lock, NULL);
}

void nvaudio_dev_close_submix(struct audio_hw_device *dev)
{
    struct nvaudio_hw_device *hw_dev = (struct nvaudio_hw_device *)dev;

    ALOGE("submix close");

    pthread_mutex_destroy(&hw_dev->submix->submix_lock);
    delete hw_dev->submix;
    hw_dev->submix = NULL;
}
