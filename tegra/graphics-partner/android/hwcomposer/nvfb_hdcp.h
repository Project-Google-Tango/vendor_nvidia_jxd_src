/*
 * Copyright (C) 2010-2012 The Android Open Source Project
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

/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 */

#ifndef __NVFB_HDCP_H__
#define __NVFB_HDCP_H__ 1

enum nvfb_hdcp_status {
    HDCP_STATUS_UNKNOWN = 0,
    HDCP_STATUS_COMPLIANT,
    HDCP_STATUS_NONCOMPLIANT,
};

struct nvfb_hdcp;

#ifndef NV_HDCP
#define NV_HDCP 0
#endif

#if NV_HDCP

int nvfb_hdcp_open(struct nvfb_hdcp **hdcp_ret, int deviceId);
void nvfb_hdcp_close(struct nvfb_hdcp *hdcp);
void nvfb_hdcp_enable(struct nvfb_hdcp *hdcp);
void nvfb_hdcp_disable(struct nvfb_hdcp *hdcp);
enum nvfb_hdcp_status nvfb_hdcp_get_status(struct nvfb_hdcp *hdcp);

#else

#define nvfb_hdcp_open(x,y) 0
#define nvfb_hdcp_close(x)
#define nvfb_hdcp_enable(x)
#define nvfb_hdcp_disable(x)
#define nvfb_hdcp_get_status(x) HDCP_STATUS_UNKNOWN

#endif

#endif /* __NVFB_HDCP_H__ */
