/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef NVCAP_VIDEO_H
#define NVCAP_VIDEO_H

/* API version supported */
#define NVCAP_VIDEO_API_VERSION_V4 4

#define NVCAP_VIDEO_API_VERSION NVCAP_VIDEO_API_VERSION_V4

/* API version requested */
#ifndef NVCAP_VIDEO_API_CLIENT_VERSION
#define NVCAP_VIDEO_API_CLIENT_VERSION 4
#endif

struct nvcap_video;

struct nvcap_layer {
    buffer_handle_t handle;
    int acquireFenceFd;
    int protect;
    int transform;
    NvRect src;
    NvRect dst;
};

typedef enum {
    HDCP_NONE,
    HDCP_V2_0,
    HDCP_V2_1,
} HdcpMode;

struct nvcap_hdcp_status {
    int isAuthenticated;
    HdcpMode hdcpMode;
};

struct nvcap_wfd_status {
    int isConnected;
};

struct nvcap_mode {
    int xres;
    int yres;
    float refresh;
};

struct nvcap_callbacks {
    void *data;
    void (*enable)(void *data, int enable);
    void (*set_mode)(void *data, struct nvcap_mode *mode);
};

struct nvcap_interface {
    int version;
    struct nvcap_video* (*open)(struct nvcap_callbacks*);
    void (*close)(struct nvcap_video*);
    int (*post)(struct nvcap_video*, struct nvcap_layer*, int *outFence);
    void (*get_hdcp_status)(struct nvcap_video*, struct nvcap_hdcp_status*);
    void (*get_wfd_status)(struct nvcap_video*, struct nvcap_wfd_status*);
    void (*wait_refresh)(struct nvcap_video*);
};

#define NVCAP_VIDEO_INTERFACE_SYM_V4     NVCAP_VIDEO_INTERFACE_V4
#define NVCAP_VIDEO_INTERFACE_STRING_V4 "NVCAP_VIDEO_INTERFACE_V4"

#define NVCAP_VIDEO_LIBRARY_STRING   "libnvcap_video.so"
#define NVCAP_VIDEO_PREMIUM_LIBRARY_STRING \
    "libnvcap_video_premium.so"

#define NVCAP_VIDEO_INTERFACE_SYM    NVCAP_VIDEO_INTERFACE_SYM_V4
#define NVCAP_VIDEO_INTERFACE_STRING NVCAP_VIDEO_INTERFACE_STRING_V4
typedef struct nvcap_interface       nvcap_interface_t;
typedef struct nvcap_layer           nvcap_layer_t;

#endif /* NVCAP_VIDEO_H */
