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

#ifdef __cplusplus
extern "C" {
#endif

extern void nvaudio_dev_open_submix(struct audio_hw_device *dev);
extern void nvaudio_dev_close_submix(struct audio_hw_device *dev);
extern bool nvaudio_is_submix_on(struct audio_hw_device *dev);
extern void nvaudio_in_standby_submix(struct audio_stream *stream);
extern ssize_t nvaudio_out_write_submix(struct audio_stream_out *stream, const void* buffer, size_t bytes, int rate);
extern ssize_t nvaudio_in_read_submix(struct audio_stream_in *stream, void* buffer, size_t bytes);
extern void nvaudio_dev_open_input_stream_submix(struct audio_hw_device *dev, struct audio_config *config);
extern void nvaudio_dev_close_input_stream_submix(struct audio_hw_device *dev);

#ifdef __cplusplus
}
#endif
