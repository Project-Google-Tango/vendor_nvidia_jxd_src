/*
 * Copyright (C) 2010-2013 The Android Open Source Project
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

#ifndef __NVFB_H__
#define __NVFB_H__ 1

#include <linux/tegrafb.h>
#include <linux/tegra_dc_ext.h>

#define DEBUG_SYNC 0
#if DEBUG_SYNC
#define SYNCLOG(...) ALOGD("Event Sync: "__VA_ARGS__)
#else
#define SYNCLOG(...)
#endif

/* Stereo modes  TODO FIXME Once these defines are
 * added to the kernel header files (waiting on Erik@Google),
 * then these lines can be removed */
#ifndef FB_VMODE_STEREO_NONE
#define FB_VMODE_STEREO_NONE        0x00000000  /* not stereo */
#define FB_VMODE_STEREO_FRAME_PACK  0x01000000  /* frame packing */
#define FB_VMODE_STEREO_TOP_BOTTOM  0x02000000  /* top-bottom */
#define FB_VMODE_STEREO_LEFT_RIGHT  0x04000000  /* left-right */
#define FB_VMODE_STEREO_MASK        0xFF000000
#endif

#define NVFB_WHOLE_TO_FX_20_12(x)    ((x) << 12)

/* Convert the HAL pixel format to the equivalent TEGRA_FB format (see
 * format list in tegrafb.h). Returns -1 if the format is not
 * supported by gralloc.
 */
static inline int nvfb_translate_hal_format(int format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
        return TEGRA_FB_WIN_FMT_R8G8B8A8;
    case HAL_PIXEL_FORMAT_RGB_565:
        return TEGRA_FB_WIN_FMT_B5G6R5;
    case HAL_PIXEL_FORMAT_BGRA_8888:
        return TEGRA_FB_WIN_FMT_B8G8R8A8;
#ifndef PLATFORM_IS_KITKAT
    case HAL_PIXEL_FORMAT_RGBA_5551:
        return TEGRA_FB_WIN_FMT_AB5G5R5;
#endif
    case NVGR_PIXEL_FORMAT_YUV420:
        return TEGRA_FB_WIN_FMT_YCbCr420P;
    case NVGR_PIXEL_FORMAT_YUV422:
        return TEGRA_FB_WIN_FMT_YCbCr422P;
    case NVGR_PIXEL_FORMAT_YUV422R:
        return TEGRA_FB_WIN_FMT_YCbCr422R;
    case NVGR_PIXEL_FORMAT_UYVY:
        return TEGRA_FB_WIN_FMT_YCbCr422;
    case NVGR_PIXEL_FORMAT_NV12:
        return TEGRA_DC_EXT_FMT_YCbCr420SP;
    case NVGR_PIXEL_FORMAT_NV16:
        return TEGRA_DC_EXT_FMT_YCbCr422SP;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        return TEGRA_DC_EXT_FMT_YCrCb420SP;
    default:
        return -1;
    }
}

/* Maximum number of display windows */
#define NVFB_MAX_WINDOWS 6

/* Display caps */
#define NVFB_DISPLAY_CAP_ROTATE              0x01
#define NVFB_DISPLAY_CAP_SCALE               0x02
#define NVFB_DISPLAY_CAP_SEQUENTIAL          0x04
#define NVFB_DISPLAY_CAP_PREMULT_CURSOR      0x08
#define NVFB_DISPLAY_CAP_BANDWIDTH_NEGOTIATE 0x10

/* Window caps
 *
 * Note: NVFB_WINDOW_CAP_SEQUENTIAL must have highest value so that it is pick
 *       later then any window with parallel blending
 */
#define NVFB_WINDOW_CAP_YUV_FORMATS          0x001
#define NVFB_WINDOW_CAP_SCALE                0x002
#define NVFB_WINDOW_CAP_SWAPXY_PITCH         0x004
#define NVFB_WINDOW_CAP_SWAPXY_TILED         0x008
#define NVFB_WINDOW_CAP_SWAPXY_TILED_PLANAR  0x010
#define NVFB_WINDOW_CAP_FLIPHV               0x020
#define NVFB_WINDOW_CAP_TILED                0x040
#define NVFB_WINDOW_CAP_SEQUENTIAL           0x080

/* Window D and H ordering */
#define NVFB_WINDOW_CAP_SEQUENTIALD         (0x000 | NVFB_WINDOW_CAP_SEQUENTIAL)
#define NVFB_WINDOW_CAP_SEQUENTIALH         (0x200 | NVFB_WINDOW_CAP_SEQUENTIAL)

#define NVFB_WINDOW_CAP_MASK                 0x3ff

#define NVFB_WINDOW_CAP_SWAPXY  (NVFB_WINDOW_CAP_SWAPXY_PITCH | \
                                 NVFB_WINDOW_CAP_SWAPXY_TILED | \
                                 NVFB_WINDOW_CAP_SWAPXY_TILED_PLANAR)

/* If window with any of these capabilities cannot be selected for layer,
 * the functionality is replaced by 2d.
 */
#define NVFB_WINDOW_CAP_OPTIONAL (NVFB_WINDOW_CAP_SCALE  | \
                                  NVFB_WINDOW_CAP_FLIPHV | \
                                  NVFB_WINDOW_CAP_SWAPXY | \
                                  NVFB_WINDOW_CAP_TILED)

/* Remaining capabilities must be provided by hw windows. If selection fails,
 * the window cannot be used for the layer.
 */
#define NVFB_WINDOW_CAP_MANDATORY (NVFB_WINDOW_CAP_MASK ^ \
                                      (NVFB_WINDOW_CAP_HINTS | \
                                       NVFB_WINDOW_CAP_OPTIONAL))

/* Do not allow >2x downscaling in DC as it is
   not reliable and inefficient in memory BW
   perspective, bug 395578 */
#define NVFB_WINDOW_DC_MAX_INC         2
#define NVFB_WINDOW_DC_MIN_SCALE       (1.0 / NVFB_WINDOW_DC_MAX_INC)

enum NvFbVideoPolicy {
    NvFbVideoPolicy_Exact,
    NvFbVideoPolicy_Closest,
    NvFbVideoPolicy_RoundUp,
    NvFbVideoPolicy_Stereo_Exact,
    NvFbVideoPolicy_Stereo_Closest,
};

enum NvFbCursorMode {
    NvFbCursorMode_None,
    NvFbCursorMode_Async,
    NvFbCursorMode_Layer,
};

enum NvDPSMode {
    NvDPS_Idle,
    NvDPS_Nominal,
    NvDPS_Count,
};

enum DispMode {
    DispMode_None = -1,
    DispMode_DMT0659 = 0,
    DispMode_480pH,
    DispMode_576pH,
    DispMode_720p,
    DispMode_720p_stereo,
    DispMode_1080p,
    DispMode_1080p_stereo,
    DispMode_2160p,
    DispMode_2160p_stereo,
    DispMode_Count,
};

typedef enum {
    HdmiResolution_Vga = DispMode_DMT0659,
    HdmiResolution_480p = DispMode_480pH,
    HdmiResolution_576p = DispMode_576pH,
    HdmiResolution_720p = DispMode_720p,
    HdmiResolution_1080p = DispMode_1080p,
    HdmiResolution_2160p = DispMode_2160p,
    HdmiResolution_Max = DispMode_Count,
    HdmiResolution_Auto,
} HdmiResolution;

struct nvfb_device;

struct nvfb_config {
    int index;
    int res_x;
    int res_y;
    int dpi_x;
    int dpi_y;
    int period;
};

struct nvfb_window_cap {
    uint_t idx;
    uint_t cap;
};

struct nvfb_display_caps {
    size_t num_windows;
    size_t num_seq_windows;
    size_t max_cursor;  // Maximum size of RGBA cursor, 0 if unsupported
    size_t max_rot_src_height; // Max for packed and planar
    size_t max_rot_src_height_noscale; // Max without scaling, for packed only
    uint_t display_cap;
    struct nvfb_window_cap window_cap[NVFB_MAX_WINDOWS];
    NvColorFormat cursor_format;
};

struct nvfb_hotplug_status {
    union {
        struct {
            unsigned type        : 2;  // unique NVFB display type
            unsigned connected   : 1;  // HDMI device connected bit
            unsigned stereo      : 1;  // HDMI device stereo capable bit
            unsigned unused      : 4;  // spare bits
            unsigned transform   : 8;  // forced rotation
            unsigned generation  : 16; // hotplug generation counter
        };
        uint32_t value;
    };
};

struct nvfb_window {
    int window_index;
    int blend;
    int transform;
    NvRect src;
    NvRect dst;
    NvPoint offset;
    int surf_index;
    uint8_t planeAlpha;
};

struct nvfb_window_config {
    int protect;
    struct fb_var_screeninfo *mode;
    struct nvfb_hotplug_status hotplug;
    struct nvfb_window overlay[NVFB_MAX_WINDOWS];
    struct nvfb_window cursor;
};

struct nvfb_buffer {
    NvNativeHandle *buffer;
    int preFenceFd;
};

typedef struct nvfb_window NvGrOverlayDesc;
typedef struct nvfb_window_config NvGrOverlayConfig;

struct nvfb_callbacks {
    void *data;
    int (*hotplug)(void *data, int disp, struct nvfb_hotplug_status hotplug);
    int (*acquire)(void *data, int disp);
    int (*release)(void *data, int disp);
    int (*bandwidth_change)(void *data);
};

typedef struct hwc_props hwc_props_t;

int nvfb_open(struct nvfb_device **dev, struct nvfb_callbacks *callbacks,
              hwc_props_t *hwc_props);
void nvfb_close(struct nvfb_device *dev);

void nvfb_get_hotplug_status(struct nvfb_device *dev, int disp,
                             struct nvfb_hotplug_status *hotplug);

void nvfb_get_display_caps(struct nvfb_device *dev, int disp,
                           struct nvfb_display_caps *caps);

int nvfb_config_get_count(struct nvfb_device *dev, int disp, size_t *count);
int nvfb_config_get(struct nvfb_device *dev, int disp, uint32_t index,
                    struct nvfb_config *config);
int nvfb_config_get_current(struct nvfb_device *dev, int disp,
                            struct nvfb_config *config);
void nvfb_get_primary_resolution(struct nvfb_device *dev, int *xres, int *yres);

int nvfb_post(struct nvfb_device *dev, int disp, NvGrOverlayConfig *conf,
              struct nvfb_buffer *bufs, int *postFenceFd);
int nvfb_reserve_bw(struct nvfb_device *dev, int disp,
                    NvGrOverlayConfig *conf, struct nvfb_buffer *bufs);
int nvfb_blank(struct nvfb_device *dev, int disp, int blank);
void nvfb_vblank_wait(struct nvfb_device *dev, int64_t *timestamp_ns);
void nvfb_dump(struct nvfb_device *dev, char *buff, int buff_len);

struct fb_var_screeninfo *nvfb_choose_display_mode(struct nvfb_device *dev,
                                                   int disp, int xres, int yres,
                                                   enum NvFbVideoPolicy policy);
int nvfb_set_display_mode(struct nvfb_device *dev, int disp,
                          struct fb_var_screeninfo *mode);
void nvfb_update_nvdps(struct nvfb_device *dev, enum NvDPSMode nvdps_mode);
void nvfb_didim_window(struct nvfb_device *dev, int disp, hwc_rect_t *rect);
void nvfb_set_transform(struct nvfb_device *dev, int disp, int transform,
                        struct nvfb_hotplug_status *hotplug);

enum NvFbCursorMode nvfb_get_cursor_mode(struct nvfb_device *dev);

int nvfb_set_cursor(struct nvfb_device *dev, int disp,
                    struct nvfb_window *cursor, struct nvfb_buffer *buf);
#endif /* __NVFB_H__ */
