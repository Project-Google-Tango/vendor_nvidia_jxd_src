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
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
 */

#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include <linux/fb.h>
#include <linux/tegra_dc_ext.h>
#include <sys/select.h>
#include <sys/stat.h>

#include "nvrm_chip.h"
#include "nvhwc.h"
#include "nvhwc_debug.h"
#include "nvsync.h"
#include "nvfb_cursor.h"
#include "nvfb_didim.h"
#include "nvfb_display.h"
#include "nvfb_hdcp.h"
#include "no_capture.h"
#include "nvfl.h"

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include "nvatrace.h"

/*********************************************************************
 *   Options
 */

/*
 * Video capture
 */
#if NVCAP_VIDEO_ENABLED
#define NVCAP_VIDEO_API_CLIENT_VERSION 4
#include "nvcap_video.h"
#include <dlfcn.h>
#endif

/* hwcomposer v1.2 uses only the first config and provides no
 * interface for selecting other configs.  The code as written is
 * forward looking and exposes all configs, which satisfies the needs
 * of the current version if only the first config is used.  Rather
 * than re-ordering configs if we want to use a different mode, this
 * ifdef modifies the behavior to expose only the current config.
 *
 * This ifdef'ed behavior is therefore a workaround for the
 * limitations of hwcomposer 1.2 and will be removed once we drop
 * support for this version.
 */
#define LIMIT_CONFIGS 1

/* A power optimization breaks FBIO_WAITFORVSYNC so smart panels need
 * to use syncpoint-based vblank notification until this is
 * corrected.
 */

#define WAR_1422639 1

/*********************************************************************
 *   Definitions
 */

#ifndef ABS
#define ABS(x) ((x) < 0 ? -(x) : (x))
#endif

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#define timespec_to_ns(t)   ((int64_t)t.tv_sec*1000000000 + t.tv_nsec)
#define timeval_to_ns(t)    ((int64_t)t.sec*1000000000 + (int64_t)t.nsec)

/* CEA-861 specifies a maximum of 65 modes in an EDID */
#define CEA_MODEDB_SIZE 65

struct nvfb_display {
    enum DisplayType type;

    /* Current state */
    int connected;
    int primary;
    int blank;

    struct nvfb_display_caps caps;
    struct nvfb_cursor *cursor;

    /* Device interface */
    union {
        struct {
            struct {
                int fd;
                uint32_t handle;
            } dc;
            struct {
                int fd;
            } fb;
        };
#if NVCAP_VIDEO_ENABLED
        struct {
            void *handle;
            struct nvcap_video *ctx;
            nvcap_interface_t *procs;
        } nvcap;
#endif
    };

    /* Mode table */
    struct {
        size_t num_modes;
        struct fb_var_screeninfo modedb[CEA_MODEDB_SIZE];
        struct fb_var_screeninfo *modes[CEA_MODEDB_SIZE];
        struct fb_var_screeninfo *table[DispMode_Count];
        struct fb_var_screeninfo *current;
#if NVDPS_ENABLE
        struct fb_var_screeninfo *nvdps[NvDPS_Count];
        enum NvDPSMode nvdps_mode;
#endif
        unsigned stereo : 1;
        unsigned dirty  : 1;
    } mode;

    struct {
        NvU32 period;
        NvU32 timeout;
        NvU32 syncpt;
    } vblank;

    struct {
        int (*blank)(struct nvfb_display *dpy, int blank);
        int (*post)(struct nvfb_device *dev, struct nvfb_display *dpy,
                    struct nvfb_window_config *conf, struct nvfb_buffer *bufs,
                    int *postFenceFd);
        int (*reserve_bw)(struct nvfb_device *dev, struct nvfb_display *dpy,
                          struct nvfb_window_config *conf,
                          struct nvfb_buffer *bufs);
        int (*set_mode)(struct nvfb_display *dpy,
                        struct fb_var_screeninfo *mode);
        void (*vblank_wait)(struct nvfb_device *dev, struct nvfb_display *dpy,
                            int64_t *timestamp_ns);
        int (*set_cursor)(struct nvfb_display *dpy, struct nvfb_window *cursor,
                          struct nvfb_buffer *buf);
    } procs;

    struct {
        struct nvfb_hotplug_status status;
    } hotplug;

    /* DIDIM state */
    struct didim_client *didim;
};

struct nvfb_device {
    int ctrl_fd;

    struct {
        int xres;
        int yres;
        int transform;
    } primary;

    uint32_t display_mask;
    struct nvfb_display displays[NVFB_MAX_DISPLAYS];
    struct nvfb_display *active_display[HWC_NUM_DISPLAY_TYPES];
    struct nvfb_display *vblank_display;

    /* External display when WFD is inactive */
    enum DisplayType default_external;

    struct {
        pthread_mutex_t mutex;
        pthread_t thread;
        int pipe[2];
    } hotplug;

    struct nvfb_hdcp *hdcp;
    NvNativeHandle *no_capture;

    struct nvfb_callbacks callbacks;

    NvGrModule *gralloc;

    hwc_props_t *hwc_props;
    enum NvFbCursorMode cursor_mode;
};

static const int default_xres = 1280;
static const int default_yres = 720;
static const float default_refresh = 60;

static const char TEGRA_CTRL_PATH[] = "/dev/tegra_dc_ctrl";

static inline NvBool is_primary(struct nvfb_display *dpy)
{
    return dpy->primary != 0;
}

static inline NvBool is_available(struct nvfb_display *dpy)
{
    return dpy && (dpy->connected || is_primary(dpy));
}

static inline NvBool is_valid(struct nvfb_display *dpy)
{
    return dpy && dpy->dc.fd >= 0;
}

/*********************************************************************
 *   VSync
 */

#define REFRESH_TO_PERIOD_NS(refresh) (1000000000.0 / refresh)
#define PERIOD_NS_TO_REFRESH(period)  (0.5 + 1000000000.0 / period)

static inline uint64_t fbmode_calc_period_ps(struct fb_var_screeninfo *info)
{
    return (uint64_t)
        (info->upper_margin + info->lower_margin + info->vsync_len + info->yres)
        * (info->left_margin + info->right_margin + info->hsync_len + info->xres)
        * info->pixclock;
}

static inline int fbmode_calc_period_ns(struct fb_var_screeninfo *info)
{
    uint64_t period = fbmode_calc_period_ps(info);

    return (int) (period ? (period / 1000.0) :
                  REFRESH_TO_PERIOD_NS(default_refresh));
}

static void update_vblank_period(struct nvfb_display *dpy)
{
    dpy->vblank.period = fbmode_calc_period_ns(dpy->mode.current);
    ALOGD("display type %d: vblank period = %u ns", dpy->type,
          dpy->vblank.period);

    if (dpy->vblank.syncpt != NVRM_INVALID_SYNCPOINT_ID) {
        /* Calculate a vblank syncpoint wait timeout in MS. */
        NvU32 timeout = (dpy->vblank.period + 500000) / 1000000;

        /* Add 10ms to the vblank interval to cover jiffies resolution
         * in nvhost.
         */
        timeout += 10;

        dpy->vblank.timeout = timeout;
    }
}

static void update_refresh_property(struct nvfb_device *dev)
{
    char value[PROPERTY_VALUE_MAX];
    struct nvfb_display *dpy = dev->vblank_display;
    int refresh = PERIOD_NS_TO_REFRESH(dpy->vblank.period);

    sprintf(value, "%d", refresh);
    property_set("sys.tegra.refresh", value);
}

static void update_vblank_display(struct nvfb_device *dev)
{
    struct nvfb_display *primary = dev->active_display[HWC_DISPLAY_PRIMARY];
    struct nvfb_display *external = dev->active_display[HWC_DISPLAY_EXTERNAL];
    struct nvfb_display *vblank = primary;

    /* set vblank trigger to external display when external display's
     * refresh rate is lower than 90% of internal's
     */
    if (external && external->connected && !external->blank) {
        const float ratio = 0.9f;

        if (external->vblank.period * ratio > primary->vblank.period) {
            vblank = external;
        }
    }

    NV_ASSERT(vblank);
    if (vblank != dev->vblank_display) {
        ALOGD("Setting vblank display to %s, new vblank period %u ns",
              vblank == primary ? "Internal" : "External",
              vblank->vblank.period);
        dev->vblank_display = vblank;
        update_refresh_property(dev);
    }
}

static void dc_vblank_wait_fb(struct nvfb_device *dev,
                              struct nvfb_display *dpy,
                              int64_t *timestamp_ns)
{
    struct timespec timestamp;

    SYNCLOG("FBIO_WAITFORVSYNC");
    if (ioctl(dpy->fb.fd, FBIO_WAITFORVSYNC) < 0) {
        ALOGE("%s: vblank ioctl failed: %s", __func__, strerror(errno));
    }

    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    *timestamp_ns = timespec_to_ns(timestamp);
}

static void dc_vblank_wait_syncpt(struct nvfb_device *dev,
                                  struct nvfb_display *dpy,
                                  int64_t *timestamp_ns)
{
    NvGrModule *m = dev->gralloc;
    NvOsSemaphoreHandle sem = 0;
    NvRmTimeval timestamp;
    NvError err;
    struct timespec timestamp_fallback;

    /* Wait for the next VBLANK */
    SYNCLOG("Waiting for syncpoint %d value %d",
            fence.SyncPointID, fence.Value);

    err = NvRmChannelSyncPointWaitmexTimeout(m->Rm,
                                            dpy->vblank.syncpt,
                                            1 + NvRmChannelSyncPointRead(m->Rm, dpy->vblank.syncpt),
                                            sem,
                                            dpy->vblank.timeout,
                                            NULL,
                                            &timestamp);
    switch (err) {
    case NvSuccess:
        *timestamp_ns = timeval_to_ns(timestamp);
        break;
    case NvError_Timeout:
        SYNCLOG("%s: NvRmFenceWait timed out.", __func__);
        clock_gettime(CLOCK_MONOTONIC, &timestamp_fallback);
        *timestamp_ns = timespec_to_ns(timestamp_fallback);
        break;
    default:
        ALOGE("%s: NvRmFenceWait returned %d", __func__, err);
        clock_gettime(CLOCK_MONOTONIC, &timestamp_fallback);
        *timestamp_ns = timespec_to_ns(timestamp_fallback);
        break;
    }
}

static void vblank_wait_sleep(struct nvfb_device *dev,
                              struct nvfb_display *dpy,
                              int64_t *timestamp_ns)
{
    struct timespec timestamp;
    struct timespec interval = {
        .tv_sec = 0,
        .tv_nsec = dpy->vblank.period,
    };

    nanosleep(&interval, NULL);

    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    *timestamp_ns = timespec_to_ns(timestamp);
}

void nvfb_vblank_wait(struct nvfb_device *dev, int64_t *timestamp_ns)
{
    struct nvfb_display *dpy = dev->vblank_display;

    NV_ASSERT(dpy);
    if (dpy->blank) {
        return;
    }

    dpy->procs.vblank_wait(dev, dpy, timestamp_ns);
}


/*********************************************************************
 *   Modes
 */

/* Minimum resolution which can be set in Auto mode */
#define HDMI_MIN_RES_IN_AUTO DispMode_720p

static float fbmode_calc_refresh(struct fb_var_screeninfo *info)
{
    uint64_t time_ps = fbmode_calc_period_ps(info);

    return time_ps ? (float)1000000000000LLU / time_ps : 60.f;
}

struct nvfb_mode_table {
    enum DispMode index;
    __u32 xres;
    __u32 yres;
    float max_refresh;
    NvBool stereo;
};

static const struct nvfb_mode_table builtin_modes[] = {
    { DispMode_DMT0659,       640,  480, 60, NV_FALSE },
    { DispMode_480pH,         720,  480, 60, NV_FALSE },
    { DispMode_576pH,         720,  576, 60, NV_FALSE },
    { DispMode_720p,         1280,  720, 60, NV_FALSE },
    { DispMode_720p_stereo,  1280,  720, 60, NV_TRUE  },
    { DispMode_1080p,        1920, 1080, 60, NV_FALSE },
    { DispMode_1080p_stereo, 1920, 1080, 60, NV_TRUE  },
    { DispMode_2160p,        3840, 2160, 30, NV_FALSE },
    { DispMode_2160p_stereo, 3840, 2160, 30, NV_TRUE  },
};

static const size_t builtin_mode_count =
    sizeof(builtin_modes) / sizeof(builtin_modes[0]);

static inline int is_stereo_mode(struct fb_var_screeninfo *mode)
{
    return mode && (mode->vmode & (FB_VMODE_STEREO_FRAME_PACK |
                                   FB_VMODE_STEREO_LEFT_RIGHT |
                                   FB_VMODE_STEREO_TOP_BOTTOM));
}

static HdmiResolution get_hdmi_resolution(struct nvfb_device *dev,
                                          struct nvfb_display *dpy)
{
    HdmiResolution hdmi_res, hdmi_res_default;

#define TOKEN(x) HdmiResolution_ ## x
#define HDMI_RES(x) TOKEN(x)
    hdmi_res_default = HDMI_RES(NV_PROPERTY_HDMI_RESOLUTION_DEFAULT);
#undef DISP_MODE
#undef TOKEN

    hdmi_res = dev->hwc_props->dynamic.hdmi_resolution;

    /* fall back to default resolution policy if the specified mode is not
       supported */
    if ((hdmi_res < HdmiResolution_Max) && !dpy->mode.table[hdmi_res]) {
        ALOGW("Preferred HDMI resolution %d is not supported, use default %d\n",
            hdmi_res, hdmi_res_default);
        hdmi_res = hdmi_res_default;
    }

    /* fall back to Max resolution policy if there is no supported resolution
       equal to or higher than minimum when Auto mode is set, so that next
       highest resolution can be set */
    if (hdmi_res == HdmiResolution_Auto) {
        int ii;
        int high_res_supported = 0;

        for (ii = HDMI_MIN_RES_IN_AUTO; ii < DispMode_Count; ii++) {
            if (dpy->mode.table[ii]) {
                high_res_supported = 1;
                break;
            }
        }

        if (!high_res_supported) {
            ALOGW("No supported resolution higher than %d, use Max resolution\n",
                HDMI_MIN_RES_IN_AUTO);
            hdmi_res = HdmiResolution_Max;
        }
    }

    return hdmi_res;
}

static enum DispMode
GetDisplayMode_Exact(struct nvfb_display *dpy, unsigned xres, unsigned yres,
                     NvBool stereo)
{
    int ii;

    for (ii = HDMI_MIN_RES_IN_AUTO; ii < DispMode_Count; ii++) {
        NvBool stereo_mode;

        if (!dpy->mode.table[ii]) continue;

        stereo_mode = is_stereo_mode(dpy->mode.table[ii]);
        if (stereo != stereo_mode) continue;

        if (dpy->mode.table[ii]->xres == xres &&
            dpy->mode.table[ii]->yres == yres) {
            return (enum DispMode) ii;
        }
    }

    return DispMode_None;
}

static enum DispMode
GetDisplayMode_Closest(struct nvfb_display *dpy, unsigned xres, unsigned yres,
                       NvBool stereo)
{
    int ii;
    enum DispMode best = DispMode_None;
    unsigned best_dx = ~0;
    unsigned best_dy = ~0;

    for (ii = HDMI_MIN_RES_IN_AUTO; ii < DispMode_Count; ii++) {
        unsigned dx, dy;
        NvBool stereo_mode;

        if (!dpy->mode.table[ii]) continue;

        stereo_mode = is_stereo_mode(dpy->mode.table[ii]);
        if (stereo != stereo_mode) continue;

        dx = ABS((int)dpy->mode.table[ii]->xres - (int)xres);
        dy = ABS((int)dpy->mode.table[ii]->yres - (int)yres);

        if (dx < best_dx || dy < best_dy) {
            best = (enum DispMode) ii;
            best_dx = dx;
            best_dy = dy;
        }
    }

    return best;
}

static enum DispMode
GetDisplayMode_RoundUp(struct nvfb_display *dpy, unsigned xres, unsigned yres)
{
    int ii;
    enum DispMode best = DispMode_None;

    for (ii = HDMI_MIN_RES_IN_AUTO; ii < DispMode_Count; ii++) {

        if (!dpy->mode.table[ii]) continue;
        if (is_stereo_mode(dpy->mode.table[ii])) continue;

        if (xres <= dpy->mode.table[ii]->xres &&
            yres <= dpy->mode.table[ii]->yres) {
            return (enum DispMode) ii;
        } else {
            best = (enum DispMode) ii;
        }
    }

    return best;
}

static enum DispMode GetDisplayMode_Highest(struct nvfb_display *dpy)
{
    int ii;

    for (ii = DispMode_Count - 1; ii >= 0; ii--) {

        if (dpy->mode.table[ii] && !is_stereo_mode(dpy->mode.table[ii]))
            return (enum DispMode) ii;
    }

    return DispMode_None;
}

static struct fb_var_screeninfo *
choose_display_mode(struct nvfb_device *dev, struct nvfb_display *dpy,
                    int xres, int yres,
                    enum NvFbVideoPolicy policy)
{
    enum DispMode mode = DispMode_None;
    HdmiResolution hdmi_res;

    if (!dpy) {
        return NULL;
    }

    hdmi_res = get_hdmi_resolution(dev, dpy);

    if (hdmi_res == HdmiResolution_Auto ||
        policy == NvFbVideoPolicy_Stereo_Exact ||
        policy == NvFbVideoPolicy_Stereo_Closest) {

        switch (policy) {
        case NvFbVideoPolicy_Exact:
            mode = GetDisplayMode_Exact(dpy, xres, yres, NV_FALSE);
            break;
        case NvFbVideoPolicy_Closest:
            mode = GetDisplayMode_Closest(dpy, xres, yres, NV_FALSE);
            break;
        case NvFbVideoPolicy_RoundUp:
            mode = GetDisplayMode_RoundUp(dpy, xres, yres);
            break;
        case NvFbVideoPolicy_Stereo_Closest:
            mode = GetDisplayMode_Closest(dpy, xres, yres, NV_TRUE);
            break;
        case NvFbVideoPolicy_Stereo_Exact:
            mode = GetDisplayMode_Exact(dpy, xres, yres, NV_TRUE);
            break;
        }
    }
    else if (hdmi_res == HdmiResolution_Max) {
        mode = GetDisplayMode_Highest(dpy);
    }
    else {
        mode = (enum DispMode)hdmi_res;
    }

    if (mode != DispMode_None) {
        return dpy->mode.table[mode];
    } else {
        return dpy->mode.current;
    }
}

struct fb_var_screeninfo *
nvfb_choose_display_mode(struct nvfb_device *dev, int disp, int xres, int yres,
                         enum NvFbVideoPolicy policy)
{
    struct nvfb_display *dpy = dev->active_display[disp];

    return choose_display_mode(dev, dpy, xres, yres, policy);
}

static void create_fake_mode(struct fb_var_screeninfo *mode,
                             int xres, int yres, float refresh)
{
    memset(mode, 0, sizeof(*mode));
    mode->xres = xres;
    mode->yres = yres;
    mode->pixclock = REFRESH_TO_PERIOD_NS(refresh) * 1000 / (xres * yres);
}

static void create_default_mode(struct fb_var_screeninfo *mode)
{
    int xres = default_xres;
    int yres = default_yres;

    create_fake_mode(mode, xres, yres, default_refresh);
}

#if NVDPS_ENABLE
static NvBool nvdps_modes_compatible(struct fb_var_screeninfo *a,
                                     struct fb_var_screeninfo *b)
{
    #define MATCH(field) if (a->field != b->field) { return NV_FALSE; }

    /* Match resolution */
    MATCH(xres);
    MATCH(yres);

    /* Match pixclock */
    MATCH(pixclock);
    MATCH(left_margin);
    MATCH(right_margin);
    MATCH(upper_margin);
    MATCH(hsync_len);
    MATCH(vsync_len);
    MATCH(sync);
    MATCH(vmode);
    MATCH(rotate);

    #undef MATCH

    return NV_TRUE;
}

static void update_nvdps_modes(struct nvfb_display *dpy)
{
    size_t ii;
    struct fb_var_screeninfo *prospect;
    struct fb_var_screeninfo *current = dpy->mode.current;
    struct fb_var_screeninfo *idle = current;

    for (ii = 0; ii < dpy->mode.num_modes; ii++) {
        prospect = dpy->mode.modes[ii];

        if (nvdps_modes_compatible(current, prospect) &&
            prospect->lower_margin > idle->lower_margin) {
            idle = prospect;
        }
    }

    dpy->mode.nvdps[NvDPS_Nominal] = current;
    dpy->mode.nvdps[NvDPS_Idle] = idle;
}
#endif

static int dc_set_mode(struct nvfb_display *dpy,
                       struct fb_var_screeninfo *mode)
{
    if (dpy->type != NVFB_DISPLAY_PANEL) {
        /* There is a known display bug where modes get out of sync.
         * The workaround is to blank and unblank around the mode set.
         * This can be removed once it is fixed.
         */
        if (ioctl(dpy->fb.fd, FBIOBLANK, FB_BLANK_POWERDOWN)) {
            ALOGE("FB_BLANK_POWERDOWN failed: %s", strerror(errno));
            return errno;
        }
    }

    if (ioctl(dpy->fb.fd, FBIOPUT_VSCREENINFO, mode)) {
        ALOGE("%s: FBIOPUT_VSCREENINFO: %s", __func__, strerror(errno));
        return errno;
    }

    if (dpy->type != NVFB_DISPLAY_PANEL) {
        /* This is currently redundant - setting the mode implicitly
         * unblanks.  That may change however, so explicitly unblank here
         * since we explicitly blanked above.
         */
        if (ioctl(dpy->fb.fd, FBIOBLANK, FB_BLANK_UNBLANK)) {
            ALOGE("FB_BLANK_UNBLANK failed: %s", strerror(errno));
            return errno;
        }

        dpy->blank = 0;
    }

    return 0;
}

static int dc_set_mode_deferred(struct nvfb_display *dpy,
                                struct fb_var_screeninfo *mode)
{
    dpy->mode.current = mode;
    dpy->mode.dirty = 1;
    update_vblank_period(dpy);
#if NVDPS_ENABLE
    update_nvdps_modes(dpy);
#endif

    return 0;
}

static int nvfb_update_display_mode(struct nvfb_device *dev,
                                    struct nvfb_display *dpy)
{
    struct fb_var_screeninfo *mode;
    int ret;

    /* Choose the target mode */
#if NVDPS_ENABLE
    mode = dpy->mode.nvdps[dpy->mode.nvdps_mode];
    NV_ASSERT(nvdps_modes_compatible(dpy->mode.current, mode));
#else
    mode = dpy->mode.current;
#endif

    /* Only DC displays set the dirty bit, therefore this function
     * cannot be reached from other display types.
     */
    NV_ASSERT(dpy->type == NVFB_DISPLAY_PANEL ||
              dpy->type == NVFB_DISPLAY_HDMI);

    ret = dc_set_mode(dpy, mode);
    if (ret) {
        return ret;
    }

    dpy->mode.dirty = 0;

    if (dpy->type != NVFB_DISPLAY_PANEL) {
        update_vblank_display(dev);
    }

    return 0;
}

void nvfb_update_nvdps(struct nvfb_device *dev, enum NvDPSMode mode)
{
#if NVDPS_ENABLE
    struct nvfb_display *dpy = dev->active_display[HWC_DISPLAY_PRIMARY];
    struct nvfb_display *external = dev->active_display[HWC_DISPLAY_EXTERNAL];

    /* TODO: Implement NvDPS + Dual display policy */
    if (!dpy || !dpy->connected || dpy->type != NVFB_DISPLAY_PANEL) {
        return;
    }

    if (external && external->connected) {
        mode = NvDPS_Nominal;
    }

    if (dpy->mode.nvdps_mode != mode) {
        dpy->mode.nvdps_mode = mode;
        dpy->mode.dirty = 1;
    }
#endif
}

int nvfb_set_display_mode(struct nvfb_device *dev, int disp,
                          struct fb_var_screeninfo *mode)
{
    struct nvfb_display *dpy = dev->active_display[disp];

    if (dpy && dpy->connected) {
        if (dpy->mode.current != mode) {
            if (dpy->type == NVFB_DISPLAY_HDMI) {
                update_vblank_display(dev);
            }
            return dpy->procs.set_mode(dpy, mode);
        }
        return 0;
    }

    return -1;
}

static void nvfb_assign_mode(struct nvfb_display *dpy,
                             struct fb_var_screeninfo *mode)
{
    const float refresh_epsilon = 0.01f;
    float refresh = fbmode_calc_refresh(mode);
    NvBool stereo = is_stereo_mode(mode);
    size_t ii;

    for (ii = 0; ii < builtin_mode_count; ii++) {
        enum DispMode index = builtin_modes[ii].index;

        if (builtin_modes[ii].xres == mode->xres &&
            builtin_modes[ii].yres == mode->yres &&
            builtin_modes[ii].stereo == stereo &&
            refresh <= (builtin_modes[ii].max_refresh + refresh_epsilon) &&
            (!dpy->mode.table[index] ||
             fbmode_calc_refresh(dpy->mode.table[index]) < refresh)) {
            dpy->mode.table[index] = mode;
            ALOGD("matched: %d x %d @ %0.2f Hz%s",
                  mode->xres, mode->yres, refresh,
                  stereo ? ", stereo" : "");
            return;
        }
    }
    ALOGD("unmatched: %d x %d @ %0.2f Hz%s",
          mode->xres, mode->yres, refresh,
          stereo ? ", stereo" : "");
}

static NvBool nvfb_modes_match(struct fb_var_screeninfo *a,
                               struct fb_var_screeninfo *b)
{

    #define MATCH(field) if (a->field != b->field) { return NV_FALSE; }

    /* Match resolution */
    MATCH(xres);
    MATCH(yres);

    /* Match timing */
    MATCH(pixclock);
    MATCH(left_margin);
    MATCH(right_margin);
    MATCH(upper_margin);
    MATCH(lower_margin);
    MATCH(hsync_len);
    MATCH(vsync_len);
    MATCH(sync);
    MATCH(vmode);
    MATCH(rotate);

    #undef MATCH

    return NV_TRUE;
}

static int nvfb_get_display_modes(struct nvfb_display *dpy)
{
    struct tegra_fb_modedb fb_modedb;
    size_t ii;

    /* Clear old modes */
    memset(&dpy->mode, 0, sizeof(dpy->mode));
#if NVDPS_ENABLE
    dpy->mode.nvdps_mode = NvDPS_Nominal;
#endif

    /* If the display is detached, set a default mode.  This will
     * never be used for a modeset but may be used for reporting
     * resolution and refresh to the framework.
     */
    if (!dpy->connected) {
        struct fb_var_screeninfo *mode = &dpy->mode.modedb[0];

        create_default_mode(mode);
        dpy->mode.modes[dpy->mode.num_modes++] = mode;

        return 0;
    }

    /* Query all supported modes */
    fb_modedb.modedb = dpy->mode.modedb;
    fb_modedb.modedb_len = sizeof(dpy->mode.modedb);
    if (ioctl(dpy->fb.fd, FBIO_TEGRA_GET_MODEDB, &fb_modedb)) {
        return errno;
    }

    ALOGD("Display %d: found %d modes", dpy->dc.handle,
          fb_modedb.modedb_len);

    /* There should always be at least one mode */
    if (!fb_modedb.modedb_len) {
        errno = EFAULT;
        return errno;
    }

    /* Filter the modes */
    for (ii = 0; ii < fb_modedb.modedb_len; ii++) {
        struct fb_var_screeninfo *mode = &dpy->mode.modedb[ii];

        /* XXX - Skip modes with invalid timing. This indicates an
         * error in the board file.
         */
        if (!fbmode_calc_period_ps(mode)) {
            ALOGW("Skipping mode with invalid timing (%d x %d)",
                  mode->xres, mode->yres);
            NV_ASSERT(0);
            continue;
        }

        /* Skip interlaced modes. */
        if (mode->vmode & FB_VMODE_INTERLACED) {
            ALOGD("Skipping interlaced mode (%d x %d @ %0.2f)",
                  mode->xres, mode->yres, fbmode_calc_refresh(mode));
            continue;
        }

        /* Match a built-in mode */
        nvfb_assign_mode(dpy, mode);

        dpy->mode.modes[dpy->mode.num_modes++] = mode;
    }

    /* There should always be at least one mode */
    NV_ASSERT(dpy->mode.num_modes);

    /* Check for supported stereo modes */
    dpy->mode.stereo = 0;
    for (ii = 0; ii < builtin_mode_count; ii++) {
        if (builtin_modes[ii].stereo) {
            enum DispMode index = builtin_modes[ii].index;

            if (dpy->mode.table[index]) {
                dpy->mode.stereo = 1;
            }
        }
    }

    return 0;
}

static int nvfb_init_display_mode(struct nvfb_device *dev,
                                  struct nvfb_display *dpy)
{
    int ret = 0;
    struct fb_var_screeninfo *default_mode;

    ret = nvfb_get_display_modes(dpy);
    if (ret) {
        return ret;
    }

    /* Pick a default mode */
    default_mode = dpy->mode.modes[0];

    /* Default HDMI mode depends on a property */
    if (dpy->type == NVFB_DISPLAY_HDMI) {
        struct fb_var_screeninfo *mode =
            choose_display_mode(dev, dpy,
                                default_mode->xres,
                                default_mode->yres,
                                NvFbVideoPolicy_RoundUp);
        if (mode) {
            default_mode = mode;
        }
    }

    if (dpy->connected) {
        ret = dpy->procs.set_mode(dpy, default_mode);
        if (ret) {
            return ret;
        }

#if NVDPS_ENABLE
        update_nvdps_modes(dpy);
#endif
        ret = dpy->procs.blank(dpy, !is_primary(dpy));
    } else {
        dpy->mode.current = default_mode;
        update_vblank_period(dpy);
    }

    return ret;
}

static void nvfb_mode_to_config(struct fb_var_screeninfo *mode,
                                struct nvfb_config *config)
{
    config->res_x = mode->xres;
    config->res_y = mode->yres;

    /* From hwcomposer_defs.h:
     *
     * The number of pixels per thousand inches of this configuration.
     */
    if ((int)mode->width <= 0 || (int)mode->height <= 0) {
        /* If the DPI for a configuration is unavailable or the HWC
         * implementation considers it unreliable, it should set these
         * attributes to zero.
         */
        config->dpi_x = 0;
        config->dpi_y = 0;
    } else {
        /* Scaling DPI by 1000 allows it to be stored in an int
         * without losing too much precision.
         */
        config->dpi_x = (int) (mode->xres * 25400.f / mode->width + 0.5f);
        config->dpi_y = (int) (mode->yres * 25400.f / mode->height + 0.5f);
    }

    config->period = fbmode_calc_period_ns(mode);
}

int nvfb_config_get_count(struct nvfb_device *dev, int disp, size_t *count)
{
    struct nvfb_display *dpy = dev->active_display[disp];

    if (is_available(dpy)) {
#if LIMIT_CONFIGS
        *count = 1;
#else
        *count = dpy->mode.num_modes;
#endif
        return 0;
    }

    return -1;
}

int nvfb_config_get(struct nvfb_device *dev, int disp, uint32_t index,
                    struct nvfb_config *config)
{
    struct nvfb_display *dpy = dev->active_display[disp];

#if LIMIT_CONFIGS
    if (is_available(dpy) && index == 0) {
        nvfb_mode_to_config(dpy->mode.current, config);
        return 0;
    }
#else
    if (is_available(dpy) && index < dpy->mode.num_modes) {
        nvfb_mode_to_config(dpy->mode.modes[index], config);
        return 0;
    }
#endif

    return -1;
}

int nvfb_config_get_current(struct nvfb_device *dev, int disp,
                            struct nvfb_config *config)
{
    struct nvfb_display *dpy = dev->active_display[disp];

    if (is_available(dpy)) {
        if (disp == HWC_DISPLAY_EXTERNAL) {
            while (dpy->mode.current == NULL) {
                struct timespec interval = {
                    .tv_sec = 0,
                    .tv_nsec = 10000000,
                };

                nanosleep(&interval, NULL);
                if (!dpy->connected) {
                    return -1;
                }
            }
        }
        nvfb_mode_to_config(dpy->mode.current, config);
        return 0;
    }

    return -1;
}

void nvfb_get_primary_resolution(struct nvfb_device *dev, int *xres, int *yres)
{
    NV_ASSERT(dev->primary.xres && dev->primary.yres);
    *xres = dev->primary.xres;
    *yres = dev->primary.yres;
}

/*********************************************************************
 *   Post
 */

static inline int convert_blend(int blend)
{
    switch (blend) {
    case HWC_BLENDING_PREMULT:
        return TEGRA_FB_WIN_BLEND_PREMULT;
    case HWC_BLENDING_COVERAGE:
        return TEGRA_FB_WIN_BLEND_COVERAGE;
    default:
        NV_ASSERT(blend == HWC_BLENDING_NONE);
        return TEGRA_FB_WIN_BLEND_NONE;
    }
}

static int SetWinAttr(struct tegra_dc_ext_flip_windowattr* w, int z,
                      struct nvfb_window *o, NvNativeHandle *h,
                      int preFenceFd)
{
    NvRmSurface* surf;
    int src_x, src_y, src_w, src_h;

    w->index = o ? o->window_index : z;
    w->z = z;

    NV_ASSERT(w->index >= 0 && w->index < NVFB_MAX_WINDOWS);

    if (!h) {
        w->buff_id = 0;
        w->buff_id_u = 0;
        w->buff_id_v = 0;
        w->blend = TEGRA_FB_WIN_BLEND_NONE;
        w->pre_syncpt_fd = -1;
        return -1;
    }

    NV_ASSERT(o && (int)h->SurfCount >= o->surf_index*2);
    surf = &h->Surf[o->surf_index];

    w->buff_id = (unsigned)surf->hMem;
    w->buff_id_u = 0;
    w->buff_id_v = 0;

    src_x = o->src.left + o->offset.x;
    src_y = o->src.top + o->offset.y;
    src_w = o->src.right - o->src.left;
    src_h = o->src.bottom - o->src.top;

    w->x = NVFB_WHOLE_TO_FX_20_12(src_x);
    w->y = NVFB_WHOLE_TO_FX_20_12(src_y);
    w->w = NVFB_WHOLE_TO_FX_20_12(src_w);
    w->h = NVFB_WHOLE_TO_FX_20_12(src_h);
    w->out_x = o->dst.left;
    w->out_y = o->dst.top;
    w->out_w = o->dst.right - o->dst.left;
    w->out_h = o->dst.bottom - o->dst.top;

    /* Sanity check */
    NV_ASSERT(o->dst.right > o->dst.left);
    NV_ASSERT(o->dst.bottom > o->dst.top);

    w->stride = surf->Pitch;
    w->stride_uv = h->Surf[o->surf_index+1].Pitch;
    w->offset = surf->Offset;
    w->offset_u = h->Surf[o->surf_index+1].Offset;
    w->offset_v = h->Surf[o->surf_index+2].Offset;
    w->blend = convert_blend(o->blend);
    w->flags = 0;
    if (o->transform & HWC_TRANSFORM_FLIP_H) {
        w->flags |= TEGRA_FB_WIN_FLAG_INVERT_H;
    }
    if (o->transform & HWC_TRANSFORM_FLIP_V) {
        w->flags |= TEGRA_FB_WIN_FLAG_INVERT_V;
    }
    NV_ASSERT(!w->offset_u ||
                  h->Surf[o->surf_index+1].Layout == surf->Layout);
    NV_ASSERT(!w->offset_v ||
                  h->Surf[o->surf_index+2].Layout == surf->Layout);
    if (surf->Layout == NvRmSurfaceLayout_Tiled) {
        w->flags |= TEGRA_FB_WIN_FLAG_TILED;
    }
    if (surf->Layout == NvRmSurfaceLayout_Blocklinear) {
        NV_ASSERT(!w->offset_u ||
                  h->Surf[o->surf_index+1].BlockHeightLog2 ==
                    surf->BlockHeightLog2);
        NV_ASSERT(!w->offset_v ||
                  h->Surf[o->surf_index+2].BlockHeightLog2 ==
                    surf->BlockHeightLog2);
        w->flags |= TEGRA_DC_EXT_FLIP_FLAG_BLOCKLINEAR;
        w->block_height_log2 = surf->BlockHeightLog2;
    }
    if (o->transform & HWC_TRANSFORM_ROT_90) {
        w->flags |= TEGRA_DC_EXT_FLIP_FLAG_SCAN_COLUMN;
        w->flags ^= TEGRA_DC_EXT_FLIP_FLAG_INVERT_V;
    }
    if (o->planeAlpha < 0xff) {
        w->flags |= TEGRA_DC_EXT_FLIP_FLAG_GLOBAL_ALPHA;
        w->global_alpha = o->planeAlpha;
    }

    w->pre_syncpt_fd = preFenceFd;
    w->pixformat = nvfb_translate_hal_format(h->Format);

    NV_ASSERT((int)w->pixformat != -1);

    return 0;
}

static int init_no_capture(struct nvfb_device *dev)
{
    NvGrModule *m = dev->gralloc;
    uint8_t *src, *dst;
    int ii, err, format, elementSize;

    switch (image_nocapture_rgb.bpp) {
    case 16:
        format = HAL_PIXEL_FORMAT_RGB_565;
        elementSize = 2;
        break;
    case 32:
        format = HAL_PIXEL_FORMAT_RGBX_8888;
        elementSize = 4;
        break;
    default:
        return -1;
    }

    err = m->alloc_scratch(m,
                           image_nocapture_rgb.xsize,
                           image_nocapture_rgb.ysize,
                           format,
                           (GRALLOC_USAGE_SW_READ_NEVER |
                            GRALLOC_USAGE_SW_WRITE_RARELY |
                            GRALLOC_USAGE_HW_FB),
                           NvRmSurfaceLayout_Pitch,
                           &dev->no_capture);
    if (err) {
        return -1;
    }

    m->Base.lock(&m->Base,
                 (buffer_handle_t)dev->no_capture,
                 GRALLOC_USAGE_SW_WRITE_RARELY, 0, 0,
                 image_nocapture_rgb.xsize,
                 image_nocapture_rgb.ysize,
                 (void**)&dst);

    src = image_nocapture_rgb.buf;
    for (ii = 0; ii < image_nocapture_rgb.ysize; ii++) {
        memcpy(dst, src, image_nocapture_rgb.xsize * elementSize);
        src += elementSize * image_nocapture_rgb.xsize;
        dst += dev->no_capture->Surf[0].Pitch;
    }

    m->Base.unlock(&m->Base, (buffer_handle_t)dev->no_capture);

    return 0;
}

static void set_no_capture(struct nvfb_device *dev,
                           struct nvfb_display *dpy,
                           struct tegra_dc_ext_flip_windowattr *w)
{
    struct nvfb_window o;
    NvRmSurface *surf;

    if (!dev->no_capture) {
        if (init_no_capture(dev)) {
            return;
        }
    }

    surf = &dev->no_capture->Surf[0];

    o.blend      = HWC_BLENDING_NONE;
    o.src.left   = 0;
    o.src.top    = 0;
    o.src.right  = surf->Width;
    o.src.bottom = surf->Height;
    o.surf_index = 0;
    o.window_index = 0;
    o.offset.x = 0;
    o.offset.y = 0;

    /* If the image fits within the display, center it; otherwise
     * scale to fit.
     */
    if (surf->Width  <= dpy->mode.current->xres &&
        surf->Height <= dpy->mode.current->yres) {
        o.dst.left   = (dpy->mode.current->xres - surf->Width)/2;
        o.dst.top    = (dpy->mode.current->yres - surf->Height)/2;
        o.dst.right  = o.dst.left + surf->Width;
        o.dst.bottom = o.dst.top  + surf->Height;
    } else {
        o.dst.left   = 0;
        o.dst.top    = 0;
        o.dst.right  = dpy->mode.current->xres;
        o.dst.bottom = dpy->mode.current->yres;
    }

    SetWinAttr(w, 0, &o, dev->no_capture, -1);
}

static void close_fences(struct nvfb_device *dev,
                         struct nvfb_display *dpy,
                         struct nvfb_buffer *bufs,
                         int wait)
{
    size_t ii;

    if (!bufs) {
        return;
    }

    #define CLOSE(buf) {                                \
        if ((buf)->buffer && (buf)->preFenceFd >= 0) {  \
            if (wait) {                                 \
                nvsync_wait((buf)->preFenceFd, -1);     \
            }                                           \
            nvsync_close((buf)->preFenceFd);            \
            (buf)->preFenceFd = -1;                     \
        }                                               \
    }

    for (ii = 0; ii < dpy->caps.num_windows; ii++) {
        CLOSE(&bufs[ii]);
    }

    if (dev->cursor_mode == NvFbCursorMode_Layer) {
        CLOSE(&bufs[dpy->caps.num_windows]);
    }

    #undef CLOSE
}

static int dc_reserve_bw(struct nvfb_device *dev, struct nvfb_display *dpy,
                         struct nvfb_window_config *conf,
                         struct nvfb_buffer *bufs)
{
    struct tegra_dc_ext_flip_2 args;
    struct tegra_dc_ext_flip_windowattr win[NVFB_MAX_WINDOWS];
    size_t ii;

    NV_ASSERT(dpy->dc.fd >= 0);

    memset(&args, 0, sizeof(args));
    memset(win, 0, sizeof(win));

    args.win = win;
    args.win_num = dpy->caps.num_windows;

    for (ii = 0; ii < dpy->caps.num_windows; ii++) {
        struct tegra_dc_ext_flip_windowattr *w = &args.win[ii];

        if (conf) {
            SetWinAttr(w, ii, &conf->overlay[ii], bufs[ii].buffer, -1);
        } else {
            SetWinAttr(w, dpy->caps.window_cap[ii].idx, NULL, NULL, -1);
        }
    }

    if (ioctl(dpy->dc.fd, TEGRA_DC_EXT_SET_PROPOSED_BW, &args) < 0) {
        return -1;
    }

    return 0;
}

static int dc_post(struct nvfb_device *dev, struct nvfb_display *dpy,
                   struct nvfb_window_config *conf, struct nvfb_buffer *bufs,
                   int *postFenceFd)
{
    NV_ATRACE_BEGIN(__func__);
    struct tegra_dc_ext_flip_3 args;
    struct tegra_dc_ext_flip_windowattr win[NVFB_MAX_WINDOWS];
    int show_warning = 0;
    size_t ii;

    NV_ASSERT(is_valid(dpy));

    if (conf && conf->hotplug.value != dpy->hotplug.status.value) {
        close_fences(dev, dpy, bufs, 0);
        conf = NULL;
    }

    if (dpy->type == NVFB_DISPLAY_HDMI) {
        if (conf && conf->protect) {
            enum nvfb_hdcp_status hdcp = nvfb_hdcp_get_status(dev->hdcp);

            if (hdcp != HDCP_STATUS_COMPLIANT) {
                close_fences(dev, dpy, bufs, 0);
                conf = NULL;
            }
            if (hdcp == HDCP_STATUS_NONCOMPLIANT) {
                show_warning = 1;
            }
        }
    } else {
        NV_ASSERT(dpy->type == NVFB_DISPLAY_PANEL);
    }

    if (conf && dev->cursor_mode == NvFbCursorMode_Layer) {
        NV_ASSERT(dpy->caps.max_cursor > 0);
        NV_ASSERT(dpy->procs.set_cursor);
        dpy->procs.set_cursor(dpy, &conf->cursor,
                              bufs ? &bufs[dpy->caps.num_windows] : NULL);
    }

    if (dpy->mode.dirty) {
        nvfb_update_display_mode(dev, dpy);
    }

    memset(&args, 0, sizeof(args));
    memset(win, 0, sizeof(win));

    args.win = (intptr_t)win;
    args.win_num = dpy->caps.num_windows;

    for (ii = 0; ii < dpy->caps.num_windows; ii++) {
        struct tegra_dc_ext_flip_windowattr *w = &win[ii];

        if (conf) {
            SetWinAttr(w, ii, &conf->overlay[ii], bufs[ii].buffer,
                       bufs[ii].preFenceFd);
        } else {
            SetWinAttr(w, dpy->caps.window_cap[ii].idx, NULL, NULL, -1);
        }
    }

    if (show_warning) {
        set_no_capture(dev, dpy, &win[0]);
    }

#if 0
    if (!conf->protect) {
        DebugBorders(m, &args);
        NvGrDumpOverlays(&dev->gralloc->Base, &dpy->Config, &args);
    }

    if (conf && dpy->overscan.use_overscan) {
        apply_underscan(dpy, w);
    }
#endif

    NV_ATRACE_BEGIN("TEGRA_DC_EXT_FLIP3");
    if (ioctl(dpy->dc.fd, TEGRA_DC_EXT_FLIP3, &args) < 0) {
        if (errno != ENXIO) {
            // When display is disabled, this will fail with ENXIO.
            // Any other error is unexpected.
            ALOGE("%s: Display %d: TEGRA_DC_EXT_FLIP3: %s\n", __func__,
                  dpy->dc.handle, strerror(errno));
        }
        // bug 857053 - keep going but invalidate the syncpoint so we
        // don't wait for it.
        args.post_syncpt_fd = -1;
    }
    NV_ATRACE_END();

    // We close the fds since display driver has the requisite fences.
    if (conf) {
        close_fences(dev, dpy, bufs, 0);
    }

    *postFenceFd = args.post_syncpt_fd;

    NV_ATRACE_END();
    return 0;
}

int nvfb_post(struct nvfb_device *dev, int disp, NvGrOverlayConfig *conf,
              struct nvfb_buffer *bufs, int *postFenceFd)
{
    NV_ATRACE_BEGIN(__func__);
    struct nvfb_display *dpy = dev->active_display[disp];

    if (postFenceFd) {
        *postFenceFd = NVSYNC_INIT_INVALID;
    }

    if (dpy && dpy->connected && (!dpy->blank || dpy->mode.dirty)) {
#if DEBUG_MODE
        if (dev->hwc_props->dynamic.dump_windows && conf) {
            hwc_dump_windows(dev->gralloc, dpy->type,
                             dpy->caps.num_windows, bufs);
        }
#endif
        dpy->procs.post(dev, dpy, conf, bufs, postFenceFd);
        nvfl_log_frame(dpy->type);
    } else {
        close_fences(dev, dpy, bufs, 0);
    }

    NV_ATRACE_END();
    return 0;
}

static int dc_blank(struct nvfb_display *dpy, int blank)
{
    ALOGD("%s: display %d, [%d -> %d]", __func__,
          dpy->type, dpy->blank, blank);

    if (dpy->blank != blank) {
        if (dpy->cursor && blank) {
            nvfb_cursor_update(dpy->cursor, NULL, NULL);
        }

        if (ioctl(dpy->fb.fd, FBIOBLANK,
                  blank ? FB_BLANK_POWERDOWN : FB_BLANK_UNBLANK)) {
            ALOGE("FB_BLANK_%s failed: %s",
                  blank ? "POWERDOWN" : "UNBLANK", strerror(errno));
            return -1;
        }

        dpy->blank = blank;
    }

    return 0;
}

int nvfb_blank(struct nvfb_device *dev, int disp, int blank)
{
    struct nvfb_display *dpy = dev->active_display[disp];

    if (dpy) {
        return dpy->procs.blank(dpy, blank);
    }

    return -1;
}

static int dc_set_cursor(struct nvfb_display *dpy, struct nvfb_window *cursor,
                         struct nvfb_buffer *buf)
{
    return nvfb_cursor_update(dpy->cursor, cursor, buf);
}

int nvfb_set_cursor(struct nvfb_device *dev, int disp,
                    struct nvfb_window *cursor, struct nvfb_buffer *buf)
{
    struct nvfb_display *dpy = dev->active_display[disp];

    NV_ASSERT(dev->cursor_mode == NvFbCursorMode_Async);
    /* This should be an assert but there is no graceful failure for nvcap yet */
    if (!dpy->procs.set_cursor) {
        return -1;
    }

    if (dpy && dpy->connected && !dpy->blank) {
        return dpy->procs.set_cursor(dpy, cursor, buf);
    }

    return 0;
}

/*********************************************************************
 *   Hot Plug
 */

static void nvfb_hotplug(struct nvfb_device *dev, int disp, int connected)
{
    struct nvfb_display *dpy = dev->active_display[disp];

    dpy->hotplug.status.connected = connected;
    dpy->hotplug.status.stereo = connected ? dpy->mode.stereo : 0;
    dpy->hotplug.status.generation++;
    dev->callbacks.hotplug(dev->callbacks.data, disp, dpy->hotplug.status);
    update_vblank_display(dev);
}

static void nvfb_set_display(struct nvfb_device *dev, int disp, int type)
{
    struct nvfb_display *old_dpy = dev->active_display[disp];
    struct nvfb_display *new_dpy = &dev->displays[type];
    int blank = old_dpy ? old_dpy->blank : disp != HWC_DISPLAY_PRIMARY;

    /* Disable the current display */
    if (old_dpy) {
        if (!old_dpy->blank) {
            dev->callbacks.release(dev->callbacks.data, disp);
        }
        if (disp == HWC_DISPLAY_PRIMARY) {
            NV_ASSERT(is_primary(old_dpy));
            old_dpy->primary = 0;
            if (old_dpy->type != NVFB_DISPLAY_PANEL) {
                NV_ASSERT(old_dpy->hotplug.status.transform ==
                          dev->primary.transform);
                old_dpy->hotplug.status.transform = 0;
                old_dpy->hotplug.status.generation++;
            }
        }
    }

    /* Set and enable the new display */
    if (disp == HWC_DISPLAY_PRIMARY) {
        NV_ASSERT(!is_primary(new_dpy));
        new_dpy->primary = 1;
        if (new_dpy->type != NVFB_DISPLAY_PANEL) {
            new_dpy->hotplug.status.transform = dev->primary.transform;
            new_dpy->hotplug.status.generation++;
        }
    }
    dev->active_display[disp] = new_dpy;
    dev->callbacks.acquire(dev->callbacks.data, disp);

    update_vblank_display(dev);
}

#if NVCAP_VIDEO_ENABLED
static void nvfb_set_hotplug_display(struct nvfb_device *dev, int type)
{
    struct nvfb_display *old_dpy = dev->active_display[HWC_DISPLAY_EXTERNAL];
    struct nvfb_display *new_dpy = NULL;

    if (type != NVFB_DISPLAY_NONE) {
        new_dpy = &dev->displays[type];
    }

    if (old_dpy != new_dpy) {
        int old_connected = old_dpy && old_dpy->connected;
        int new_connected = new_dpy && new_dpy->connected;

        /* If connected state hasn't changed, skip the hotplug */
        if (old_connected && new_connected &&
            old_dpy->mode.current->xres == new_dpy->mode.current->xres &&
            old_dpy->mode.current->yres == new_dpy->mode.current->yres) {

            /* Release the old display */
            dev->callbacks.release(dev->callbacks.data, HWC_DISPLAY_EXTERNAL);

            /* Hot swap the displays */
            dev->active_display[HWC_DISPLAY_EXTERNAL] = new_dpy;

            /* Acquire the new display */
            dev->callbacks.acquire(dev->callbacks.data, HWC_DISPLAY_EXTERNAL);
            update_vblank_display(dev);
        } else {
            /* Disconnect old display */
            if (old_connected) {
                nvfb_hotplug(dev, HWC_DISPLAY_EXTERNAL, 0);
                old_dpy->procs.blank(old_dpy, 1);
            }

            /* Hot swap the displays */
            dev->active_display[HWC_DISPLAY_EXTERNAL] = new_dpy;

            /* Connect new display */
            if (new_connected) {
                new_dpy->procs.blank(new_dpy, 0);
                nvfb_hotplug(dev, HWC_DISPLAY_EXTERNAL, 1);
            }
        }

        if (old_connected && old_dpy->type == NVFB_DISPLAY_HDMI) {
            nvfb_hdcp_disable(dev->hdcp);
        } else if (new_connected && new_dpy->type == NVFB_DISPLAY_HDMI) {
            nvfb_hdcp_enable(dev->hdcp);
        }
    }
}
#endif

static void handle_hotplug(struct nvfb_device *dev, void *data)
{
    const int disp = NVFB_DISPLAY_HDMI;
    struct nvfb_display *dpy = &dev->displays[disp];
    __u32 handle = ((struct tegra_dc_ext_control_event_hotplug *) data)->handle;
    struct tegra_dc_ext_control_output_properties props;

    if (handle != dpy->dc.handle) {
        ALOGD("Hotplug: not HDMI");
        return;
    }

    props.handle = handle;
    if (ioctl(dev->ctrl_fd, TEGRA_DC_EXT_CONTROL_GET_OUTPUT_PROPERTIES,
              &props)) {
        ALOGE("Could not query out properties: %s", strerror(errno));
        return;
    }

    pthread_mutex_lock(&dev->hotplug.mutex);

    NV_ASSERT(props.handle == dpy->dc.handle);
    dpy->connected = props.connected;

    ALOGI("hotplug: connected = %d", dpy->connected);

    if (dpy->connected) {
        /* Update properties on HDMI hotplug */
        hwc_props_scan(dev->hwc_props);
    }

    /* Disable if not connected or init_display_mode fails */
    if (!dpy->connected || nvfb_init_display_mode(dev, dpy)) {
        if (dpy->connected) {
            ALOGE("Failed to get display modes: %s", strerror(errno));
            dpy->connected = 0;
        }
        dpy->mode.num_modes = 0;
        dpy->mode.current = NULL;
        dpy->mode.dirty = 0;
    }

    if (dev->hwc_props->fixed.stb_mode) {
        nvfb_set_display(dev, HWC_DISPLAY_PRIMARY, dpy->connected ?
                         NVFB_DISPLAY_HDMI : NVFB_DISPLAY_PANEL);
    } else {
        int ii;
        for (ii = 0; ii < HWC_NUM_DISPLAY_TYPES; ii++) {
            if (dev->active_display[ii] == dpy) {
                nvfb_hotplug(dev, ii, dpy->connected);
                break;
            }
        }
    }

    if (dpy->connected) {
        nvfb_hdcp_enable(dev->hdcp);
    } else {
        nvfb_hdcp_disable(dev->hdcp);
    }

    pthread_mutex_unlock(&dev->hotplug.mutex);
}

static void handle_bandwidth_change(struct nvfb_device *dev, void *data)
{
    IMPLOG("request bandwidth change");
    dev->callbacks.bandwidth_change(dev->callbacks.data);
}

static void *hotplug_thread(void *arg)
{
    struct nvfb_device *dev = (struct nvfb_device *) arg;
    int nfds;

    nfds = MAX(dev->ctrl_fd, dev->hotplug.pipe[0]) + 1;

    while (1) {
        fd_set rfds;

        FD_ZERO(&rfds);
        FD_SET(dev->ctrl_fd, &rfds);
        FD_SET(dev->hotplug.pipe[0], &rfds);

        int status = select(nfds, &rfds, NULL, NULL, NULL);

        if (status < 0) {
            if (errno != EINTR) {
                ALOGE("%s: select failed: %s", __func__, strerror(errno));
                return (void *) -1;
            }
        } else if (status) {
            ssize_t bytes;

            if (FD_ISSET(dev->hotplug.pipe[0], &rfds)) {
                uint8_t value = 0;

                ALOGD("%s: processing pipe fd", __func__);
                do {
                    bytes = read(dev->hotplug.pipe[0], &value, sizeof(value));
                } while (bytes < 0 && errno == EINTR);

                if (value) {
                    ALOGD("%s: exiting", __func__);
                    pthread_exit(0);
                }
            }

            if (FD_ISSET(dev->ctrl_fd, &rfds)) {
                char buf[1024];
                struct tegra_dc_ext_event *event = (void *)buf;

                ALOGD("%s: processing control fd", __func__);
                do {
                    bytes = read(dev->ctrl_fd, &buf, sizeof(buf));
                } while (bytes < 0 && errno == EINTR);

                if (bytes < (ssize_t) sizeof(event)) {
                    return (void *) -1;
                }

                switch (event->type) {
                case TEGRA_DC_EXT_EVENT_HOTPLUG:
                    handle_hotplug(dev, event->data);
                    break;
                case TEGRA_DC_EXT_EVENT_BANDWIDTH_INC:
                case TEGRA_DC_EXT_EVENT_BANDWIDTH_DEC:
                    handle_bandwidth_change(dev, event->data);
                    break;
                default:
                    ALOGE("%s: unexpected event type %d", __func__,
                          event->type);
                    break;
                }
            }
        }
    }

    return (void *) 0;
}

/*********************************************************************
 *   Misc
 */

void nvfb_didim_window(struct nvfb_device *dev, int disp, hwc_rect_t *rect)
{
    struct nvfb_display *dpy = dev->active_display[disp];

    if (dpy && dpy->didim) {
        dpy->didim->set_window(dpy->didim, rect);
    }
}

/*********************************************************************
 *   Initialization
 */

void nvfb_get_display_caps(struct nvfb_device *dev, int disp,
                           struct nvfb_display_caps *caps)
{
    struct nvfb_display *dpy = dev->active_display[disp];

    memset(caps, 0, sizeof(struct nvfb_display_caps));

    if (dpy) {
        *caps = dpy->caps;
    }
}

/* Display properties, ver 2 (T114) */
#define NVFB_WINDOW_COMMON_CAP(n) (NVFB_WINDOW_CAP_YUV_FORMATS | \
                                   NVFB_WINDOW_CAP_SCALE       | \
                                   NVFB_WINDOW_CAP_TILED       | \
                                   NVFB_WINDOW_CAP_FLIPHV      | \
                                   ((!n) ? NVFB_WINDOW_CAP_SWAPXY_TILED : 0))

static const struct nvfb_display_caps ver2_display0_caps =
{
    .num_windows = 3,
    .num_seq_windows = 0,
    .max_cursor = 256,
    .max_rot_src_height = 2560,
    .max_rot_src_height_noscale = 2560,
    .display_cap = NVFB_DISPLAY_CAP_ROTATE |
                   NVFB_DISPLAY_CAP_SCALE,
    .window_cap = {
        { 0, NVFB_WINDOW_COMMON_CAP(0) | NVFB_WINDOW_CAP_SWAPXY_TILED_PLANAR },
        { 1, NVFB_WINDOW_COMMON_CAP(0) },
        { 2, NVFB_WINDOW_COMMON_CAP(0) }
    },
    .cursor_format = NvColorFormat_R8G8B8A8
};

static const struct nvfb_display_caps ver2_display1_caps =
{
    .num_windows = 3,
    .num_seq_windows = 0,
    .max_cursor = 256,
    .max_rot_src_height = 0,
    .max_rot_src_height_noscale = 0,
    .display_cap = NVFB_DISPLAY_CAP_SCALE,
    .window_cap = {
        { 0, NVFB_WINDOW_COMMON_CAP(1) },
        { 1, NVFB_WINDOW_COMMON_CAP(1) },
        { 2, NVFB_WINDOW_COMMON_CAP(1) }
    },
    .cursor_format = NvColorFormat_R8G8B8A8
};

#undef NVFB_WINDOW_COMMON_CAP

/* Display properties, ver 3 (T148) */
#define NVFB_WINDOW_COMMON_CAP (NVFB_WINDOW_CAP_TILED    | \
                                NVFB_WINDOW_CAP_FLIPHV)

static const struct nvfb_display_caps ver3_display0_caps =
{
    .num_windows = 5,
    .num_seq_windows = 2,
    .max_cursor = 0,
    .max_rot_src_height = 0,
    .max_rot_src_height_noscale = 0,
    .display_cap = NVFB_DISPLAY_CAP_SCALE |
                   NVFB_DISPLAY_CAP_SEQUENTIAL,
    .window_cap = {
        { 0, NVFB_WINDOW_COMMON_CAP },
        { 1, NVFB_WINDOW_COMMON_CAP | NVFB_WINDOW_CAP_YUV_FORMATS |
                                      NVFB_WINDOW_CAP_SCALE },
        { 2, NVFB_WINDOW_COMMON_CAP | NVFB_WINDOW_CAP_YUV_FORMATS },
        { 3, NVFB_WINDOW_CAP_SEQUENTIALD },
        { 4, NVFB_WINDOW_CAP_SEQUENTIALH }
    },
    .cursor_format = NvColorFormat_Unspecified
};

static const struct nvfb_display_caps ver3_display1_caps =
{
    .num_windows = 4,
    .num_seq_windows = 1,
    .max_cursor = 0,
    .max_rot_src_height = 0,
    .max_rot_src_height_noscale = 0,
    .display_cap = NVFB_DISPLAY_CAP_SCALE |
                   NVFB_DISPLAY_CAP_SEQUENTIAL,
    .window_cap = {
        { 0, NVFB_WINDOW_COMMON_CAP },
        { 1, NVFB_WINDOW_COMMON_CAP | NVFB_WINDOW_CAP_YUV_FORMATS |
                                      NVFB_WINDOW_CAP_SCALE },
        { 2, NVFB_WINDOW_COMMON_CAP | NVFB_WINDOW_CAP_YUV_FORMATS },
        { 4, NVFB_WINDOW_CAP_SEQUENTIALH }
    },
    .cursor_format = NvColorFormat_Unspecified
};

#undef NVFB_WINDOW_COMMON_CAP

/* Display properties, ver 4 (T124) */
#define NVFB_WINDOW_COMMON_CAP(n) (NVFB_WINDOW_CAP_YUV_FORMATS | \
                                   NVFB_WINDOW_CAP_SCALE       | \
                                   NVFB_WINDOW_CAP_TILED       | \
                                   NVFB_WINDOW_CAP_FLIPHV      | \
                                   NVFB_WINDOW_CAP_SEQUENTIAL  | \
                                   ((!n) ? NVFB_WINDOW_CAP_SWAPXY_TILED : 0))

static const struct nvfb_display_caps ver4_display0_caps =
{
    .num_windows = 4,
    .num_seq_windows = 4,
    .max_cursor = 256,
    .max_rot_src_height = 2560,
    .max_rot_src_height_noscale = 3200,
    .display_cap = NVFB_DISPLAY_CAP_ROTATE |
                   NVFB_DISPLAY_CAP_SCALE |
                   NVFB_DISPLAY_CAP_SEQUENTIAL |
                   NVFB_DISPLAY_CAP_PREMULT_CURSOR |
                   NVFB_DISPLAY_CAP_BANDWIDTH_NEGOTIATE,
    .window_cap = {
        { 0, NVFB_WINDOW_COMMON_CAP(0) | NVFB_WINDOW_CAP_SWAPXY_TILED_PLANAR},
        { 1, NVFB_WINDOW_COMMON_CAP(0)},
        { 2, NVFB_WINDOW_COMMON_CAP(0)},
        { 3, NVFB_WINDOW_CAP_SEQUENTIAL },
    },
    .cursor_format = NvColorFormat_R8G8B8A8
};

static const struct nvfb_display_caps ver4_display1_caps =
{
    .num_windows = 3,
    .num_seq_windows = 3,
    .max_cursor = 256,
    .max_rot_src_height = 0,
    .max_rot_src_height_noscale = 0,
    .display_cap = NVFB_DISPLAY_CAP_SCALE |
                   NVFB_DISPLAY_CAP_SEQUENTIAL |
                   NVFB_DISPLAY_CAP_PREMULT_CURSOR |
                   NVFB_DISPLAY_CAP_BANDWIDTH_NEGOTIATE,
    .window_cap = {
        { 0, NVFB_WINDOW_COMMON_CAP(1) },
        { 1, NVFB_WINDOW_COMMON_CAP(1) },
        { 2, NVFB_WINDOW_COMMON_CAP(1) }
    },
    .cursor_format = NvColorFormat_R8G8B8A8
};

#undef NVFB_WINDOW_COMMON_CAP

static int nvfb_calc_display_caps(struct nvfb_display *dpy)
{
    NvRmChipDisplayVersion ver;

    NvRmChipGetCapabilityU32(NvRmChipCapability_Display_Version, &ver);

    switch (ver) {
    case NvRmChipDisplayVersion_2:
        if (dpy->type == NVFB_DISPLAY_PANEL) {
            dpy->caps = ver2_display0_caps;
        } else {
            dpy->caps = ver2_display1_caps;
        }
        break;
    case NvRmChipDisplayVersion_3:
        if (dpy->type == NVFB_DISPLAY_PANEL) {
            dpy->caps = ver3_display0_caps;
        } else {
            dpy->caps = ver3_display1_caps;
        }
        break;
    case NvRmChipDisplayVersion_4:
        if (dpy->type == NVFB_DISPLAY_PANEL) {
            dpy->caps = ver4_display0_caps;
        } else {
            dpy->caps = ver4_display1_caps;
        }
        break;
    default:
        NV_ASSERT(0);
        return -1;
    }

    return 0;
}

int nvfb_reserve_bw(struct nvfb_device *dev, int disp,
                    NvGrOverlayConfig *conf, struct nvfb_buffer *bufs)
{
    struct nvfb_display *dpy = dev->active_display[disp];

    if (dev->hwc_props->dynamic.imp_enable &&
        dpy && dpy->connected && (!dpy->blank || dpy->mode.dirty)) {
        return dpy->procs.reserve_bw(dev, dpy, conf, bufs);
    }
    return 0;
}

/* Cursors */
static int dc_cursor_init(struct nvfb_device *dev, struct nvfb_display *dpy)
{
    int result = 0;

    if (dev->hwc_props->fixed.cursor_enable &&
        !dev->hwc_props->fixed.null_display &&
        dpy->caps.max_cursor > 0) {

        dev->cursor_mode = NvFbCursorMode_Layer;

#ifdef FRAMEWORKS_HAS_SET_CURSOR
        if (dev->hwc_props->fixed.cursor_async) {
            dev->cursor_mode = NvFbCursorMode_Async;
        }
#endif

       result = nvfb_cursor_open(&dpy->cursor,
                                 dev->gralloc,
                                 dpy->dc.fd,
                                 &dpy->caps);
    }

    if (result != 0) {
        dpy->caps.max_cursor = 0;
        dev->cursor_mode = NvFbCursorMode_None;
    }

    return result;
}

static void dc_cursor_deinit(struct nvfb_display *dpy)
{
    if (dpy->cursor) {
        nvfb_cursor_close(dpy->cursor);
    }
}

enum NvFbCursorMode nvfb_get_cursor_mode(struct nvfb_device *dev)
{
    return dev->cursor_mode;
}

char const * const fb_template[] = {
    "/dev/graphics/fb%u",
    "/dev/fb%u",
    0
};

char const * const dc_template[] = {
    "/dev/tegra_dc_%u",
    0
};

static int nvfb_open_device(char const * const template[], int disp)
{
    char path[FILENAME_MAX];
    int fd;
    size_t ii;

    for (ii = 0, fd = -1; fd == -1 && template[ii]; ii++) {
        snprintf(path, sizeof(path), template[ii], disp);
        fd = open(path, O_RDWR, 0);
    }

    return fd;
}

static int nvfb_open_display(struct nvfb_device *dev, struct nvfb_display *dpy)
{
    int disp = dpy->dc.handle;
    int fb, dc;
    size_t ii;
#ifndef WAR_1422639
    char path[FILENAME_MAX];
    const char *smart_panel_template =
        "/sys/class/graphics/fb%u/device/smart_panel";
    struct stat buf;
#endif

    dpy->fb.fd = dpy->dc.fd = -1;

    fb = nvfb_open_device(fb_template, disp);
    if (fb == -1) {
        ALOGE("Can't open framebuffer device %d: %s\n", disp, strerror(errno));
        return errno;
    }

    dc = nvfb_open_device(dc_template, disp);
    if (dc == -1) {
        ALOGE("Can't open DC device %d: %s\n", disp, strerror(errno));
        close(fb);
        return errno;
    }

    dpy->fb.fd = fb;
    dpy->dc.fd = dc;

    if (ioctl(dpy->dc.fd, TEGRA_DC_EXT_SET_NVMAP_FD,
              NvRm_MemmgrGetIoctlFile()) < 0) {
        ALOGE("Can't set nvmap fd: %s\n", strerror(errno));
        return errno;
    }

    /* Set up vblank wait */
#ifndef WAR_1422639
    snprintf(path, sizeof(path), smart_panel_template, disp);
    if (!stat(path, &buf)) {
        dpy->vblank.syncpt = NVRM_INVALID_SYNCPOINT_ID;
    } else
#endif
    if (ioctl(dpy->dc.fd, TEGRA_DC_EXT_GET_VBLANK_SYNCPT,
                     &dpy->vblank.syncpt)) {
        ALOGE("Can't query vblank syncpoint for device %d: %s\n",
              disp, strerror(errno));
        return errno;
    }

    if (nvfb_calc_display_caps(dpy) < 0) {
        ALOGE("Can't get display caps for display %d\n", disp);
        return errno;
    }

    for (ii = 0; ii < dpy->caps.num_windows; ii++) {
        if (ioctl(dpy->dc.fd, TEGRA_DC_EXT_GET_WINDOW,
                  dpy->caps.window_cap[ii].idx) < 0) {
            ALOGE("Failed to allocate window %d for display %d\n", ii, disp);
            return errno;
        }
    }

    if (dc_cursor_init(dev, dpy)) {
        return -1;
    }

    return 0;
}

static void nvfb_close_display(struct nvfb_display *dpy)
{
    if (!is_valid(dpy)) {
        return;
    }
    if (dpy->type == NVFB_DISPLAY_PANEL) {
        didim_close(dpy->didim);
    }
    dc_blank(dpy, 1);
    dc_cursor_deinit(dpy);
    close(dpy->fb.fd);
    close(dpy->dc.fd);
}

static enum DisplayType
convert_dc_type(enum tegra_dc_ext_control_output_type dc_type)
{
    switch (dc_type) {
    case TEGRA_DC_EXT_DSI:
    case TEGRA_DC_EXT_LVDS:
    case TEGRA_DC_EXT_DP:
        return NVFB_DISPLAY_PANEL;
    case TEGRA_DC_EXT_VGA:
    case TEGRA_DC_EXT_HDMI:
    case TEGRA_DC_EXT_DVI:
        return NVFB_DISPLAY_HDMI;
    default:
        NV_ASSERT(!"unrecognized output type");
        return NVFB_DISPLAY_HDMI;
        break;
    }
}

static int nvfb_get_displays(struct nvfb_device *dev)
{
    uint_t num, ii;

    dev->ctrl_fd = open(TEGRA_CTRL_PATH, O_RDWR);
    if (dev->ctrl_fd < 0) {
        return errno;
    }

    if (ioctl(dev->ctrl_fd, TEGRA_DC_EXT_CONTROL_GET_NUM_OUTPUTS, &num)) {
        return errno;
    }

    if (num > NVFB_MAX_DC_TYPES) {
        num = NVFB_MAX_DC_TYPES;
    }

    for (ii = 0; ii < num; ii++) {
        struct nvfb_display *dpy;
        struct tegra_dc_ext_control_output_properties props;
        enum DisplayType type;

        props.handle = ii;
        if (ioctl(dev->ctrl_fd, TEGRA_DC_EXT_CONTROL_GET_OUTPUT_PROPERTIES,
                  &props)) {
            return errno;
        }

        type = convert_dc_type(props.type);
        dpy = &dev->displays[type];
        if (is_valid(dpy)) {
            /* Only a single display of each type is currently supported */
            continue;
        }

        dpy->type = type;
        dpy->dc.handle = ii;
        dpy->connected = props.connected;

        if (nvfb_open_display(dev, dpy)) {
            return errno;
        }

        dpy->procs.blank = dc_blank;
        dpy->procs.post = dc_post;
        dpy->procs.reserve_bw = dc_reserve_bw;
        dpy->procs.set_mode = dc_set_mode_deferred;
        if (dpy->vblank.syncpt != NVRM_INVALID_SYNCPOINT_ID) {
            dpy->procs.vblank_wait = dc_vblank_wait_syncpt;
        } else {
            dpy->procs.vblank_wait = dc_vblank_wait_fb;
        }
        if (dev->cursor_mode != NvFbCursorMode_None) {
            dpy->procs.set_cursor = dc_set_cursor;
        }

        dpy->blank = -1;
    }

    return 0;
}

static int nvfb_init_displays(struct nvfb_device *dev)
{
    uint_t i;

    for (i = 0; i < NVFB_MAX_DC_TYPES; i++) {
        struct nvfb_display *dpy = &dev->displays[i];

        if (!is_valid(dpy))
            continue;

        /* Always set a default mode, even if disconnected. */
        if (nvfb_init_display_mode(dev, dpy)) {
            return errno;
        }
    }

    return 0;
}

static int null_blank(struct nvfb_display *dpy, int blank)
{
    dpy->blank = blank;

    return 0;
}

static int
null_post(struct nvfb_device *dev, struct nvfb_display *dpy,
          struct nvfb_window_config *conf, struct nvfb_buffer *bufs,
          int *postFenceFd)
{
    size_t ii;

    if (conf && bufs) {
        close_fences(dev, dpy, bufs, 1);
    }

    *postFenceFd = -1;

    return 0;
}

static int
null_reserve_bw(struct nvfb_device *dev, struct nvfb_display *dpy,
                struct nvfb_window_config *conf, struct nvfb_buffer *bufs)
{
    return 0;
}

static int null_set_mode(struct nvfb_display *dpy,
                         struct fb_var_screeninfo *mode)
{
    return 0;
}

static void init_null_display(struct nvfb_device *dev)
{
    struct nvfb_display *dpy = &dev->displays[NVFB_DISPLAY_PANEL];
    struct fb_var_screeninfo *mode;
    int xres = default_xres;
    int yres = default_yres;

    dpy->type = NVFB_DISPLAY_PANEL;
    dpy->connected = 1;
    dpy->blank = 0;
    nvfb_calc_display_caps(dpy);
    dpy->vblank.syncpt = NVRM_INVALID_SYNCPOINT_ID;

    dpy->procs.blank = null_blank;
    dpy->procs.post = null_post;
    dpy->procs.reserve_bw = null_reserve_bw;
    dpy->procs.set_mode = null_set_mode;
    dpy->procs.vblank_wait = vblank_wait_sleep;

    if (dev->hwc_props->fixed.display_resolution.xres > 0 &&
        dev->hwc_props->fixed.display_resolution.yres > 0) {
        xres = dev->hwc_props->fixed.display_resolution.xres;
        yres = dev->hwc_props->fixed.display_resolution.yres;
    }

    mode = dpy->mode.modedb;
    create_fake_mode(mode, xres, yres, default_refresh);
    dpy->mode.modes[dpy->mode.num_modes++] = mode;
    dpy->mode.current = mode;

    update_vblank_period(dpy);

    /* Cursor is not currently supported with null display */
    if (dev->hwc_props->fixed.cursor_enable) {
        ALOGW("Cursor not supported with null-display. Disabling cursor.");
    }
    dpy->caps.max_cursor = 0;
}

#if NVCAP_VIDEO_ENABLED
static void nvfb_nvcap_set_mode(void *data, struct nvcap_mode *mode)
{
    struct nvfb_device *dev = (struct nvfb_device *) data;
    struct nvfb_display *dpy = &dev->displays[NVFB_DISPLAY_WFD];
    int changed = NV_FALSE;

    pthread_mutex_lock(&dev->hotplug.mutex);

    /* The Android Display Manager gets confused when the hotplug
     * state of a display is rapidly toggled off and on without
     * actually changing the resolution.  Specifically, it processes
     * the disconnect but no corresponding reconnect.  Work around
     * this by detecting redundant mode sets and update only the
     * vblank period.  (bug 1268948)
     */
    if (dpy->mode.current->xres != (unsigned)mode->xres ||
        dpy->mode.current->yres != (unsigned)mode->yres) {
        changed = NV_TRUE;
    }

    if (dpy->connected && changed) {
        NV_ASSERT(dpy == dev->active_display[HWC_DISPLAY_EXTERNAL]);
        /* disconnect old */
        nvfb_hotplug(dev, HWC_DISPLAY_EXTERNAL, 0);
    }

    ALOGD("%s: %d x %d @ %1.f Hz", __func__, mode->xres, mode->yres,
          mode->refresh);

    create_fake_mode(dpy->mode.current, mode->xres, mode->yres, mode->refresh);
    update_vblank_period(dpy);

    if (dpy->connected && changed) {
        /* reconnect new */
        nvfb_hotplug(dev, HWC_DISPLAY_EXTERNAL, 1);
    }

    pthread_mutex_unlock(&dev->hotplug.mutex);
}

static void nvfb_nvcap_enable(void *data, int enable)
{
    struct nvfb_device *dev = (struct nvfb_device *) data;
    struct nvfb_display *dpy = &dev->displays[NVFB_DISPLAY_WFD];

    ALOGD("%s: nvcap %sabled", __func__, enable ? "en" : "dis");

    pthread_mutex_lock(&dev->hotplug.mutex);

    if (enable) {
        dpy->connected = 1;
        nvfb_set_hotplug_display(dev, NVFB_DISPLAY_WFD);
    } else {
        nvfb_set_hotplug_display(dev, dev->default_external);
        dpy->connected = 0;
    }

    pthread_mutex_unlock(&dev->hotplug.mutex);
}

static void nvfb_nvcap_close(struct nvfb_device *dev)
{
    struct nvfb_display *dpy = &dev->displays[NVFB_DISPLAY_WFD];

    if (dpy->nvcap.ctx) {
        dpy->nvcap.procs->close(dpy->nvcap.ctx);
        dpy->nvcap.ctx = NULL;
    }

    if (dpy->nvcap.handle) {
        dlclose(dpy->nvcap.handle);
        dpy->nvcap.handle = NULL;
    }
}

static int nvfb_nvcap_open(struct nvfb_device *dev)
{
    struct nvfb_display *dpy = &dev->displays[NVFB_DISPLAY_WFD];
    const char *library_name_premium = NVCAP_VIDEO_PREMIUM_LIBRARY_STRING;
    const char *library_name = NVCAP_VIDEO_LIBRARY_STRING;
    const char *interface_string = NVCAP_VIDEO_INTERFACE_STRING;
    struct nvcap_callbacks callbacks = {
        .data = dev,
        .enable = nvfb_nvcap_enable,
        .set_mode = nvfb_nvcap_set_mode,
    };

    /* Load the premium library first, and the standard library if that fails */
    dpy->nvcap.handle = dlopen(library_name_premium, RTLD_LAZY);
    if (!dpy->nvcap.handle) {
        dpy->nvcap.handle = dlopen(library_name, RTLD_LAZY);
        if (!dpy->nvcap.handle) {
            ALOGE("failed to load %s: %s", library_name, dlerror());
            goto fail;
        }
    }

    dpy->nvcap.procs = (nvcap_interface_t *)
        dlsym(dpy->nvcap.handle, interface_string);
    if (!dpy->nvcap.procs) {
        ALOGE("failed to load nvcap interface: %s%s",
              dlerror(), interface_string);
        goto fail;
    }

    ALOGD("Creating nvcap video capture service");
    dpy->nvcap.ctx = dpy->nvcap.procs->open(&callbacks);
    if (!dpy->nvcap.ctx) {
        ALOGE("failed to create nvcap video capture service\n");
        goto fail;
    }

    return 0;

fail:
    nvfb_nvcap_close(dev);
    return -1;
}

static struct nvfb_display_caps nvcap_display_caps =
{
    .num_windows = 1,
    .max_cursor = 0,
    .display_cap = NVFB_DISPLAY_CAP_SCALE,
    .window_cap = {
        { 0, NVFB_WINDOW_CAP_YUV_FORMATS | NVFB_WINDOW_CAP_SCALE |
             NVFB_WINDOW_CAP_TILED },
    },
    .cursor_format = NvColorFormat_Unspecified
};

static int nvcap_blank(struct nvfb_display *dpy, int blank)
{
    ALOGD("%s: display %d, [%d -> %d]", __func__,
          dpy->type, dpy->blank, blank);

    if (dpy->blank != blank) {
        if (blank) {
            dpy->nvcap.procs->post(dpy->nvcap.ctx, NULL, NULL);
        }
        dpy->blank = blank;
    }

    return 0;
}

static void nvcap_vblank_wait(struct nvfb_device *dev,
                              struct nvfb_display *dpy,
                              int64_t *timestamp_ns)
{
    struct timespec timestamp;
    dpy->nvcap.procs->wait_refresh(dpy->nvcap.ctx);

    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    *timestamp_ns = timespec_to_ns(timestamp);
}

static int
nvcap_post(struct nvfb_device *dev, struct nvfb_display *dpy,
           struct nvfb_window_config *conf, struct nvfb_buffer *bufs,
           int *releaseFenceFd)
{
    nvcap_layer_t layer;
    int isHdcpAuthenticated = NV_FALSE;
    int ret;

    NV_ASSERT(!releaseFenceFd || !nvsync_is_valid(*releaseFenceFd));

    if (!conf) {
        close_fences(dev, dpy, bufs, 0);
        return nvcap_blank(dpy, 1);
    }

    if (conf->protect) {
        struct nvcap_hdcp_status sHdcpStatus;
        dpy->nvcap.procs->get_hdcp_status(dpy->nvcap.ctx,&sHdcpStatus);
        isHdcpAuthenticated = sHdcpStatus.isAuthenticated;
    }

    if (conf->protect && !isHdcpAuthenticated) {
        NvRmSurface *surf;

        if (!dev->no_capture) {
            int err = init_no_capture(dev);
            if (err) {
                close_fences(dev, dpy, bufs, 0);
                return err;
            }
        }

        surf = &dev->no_capture->Surf[0];

        layer.handle = &dev->no_capture->Base;
        layer.acquireFenceFd = -1;
        layer.transform = 0;
        layer.src.left = 0;
        layer.src.top = 0;
        layer.src.right = surf->Width;
        layer.src.bottom = surf->Height;
        layer.dst = layer.src;
    } else {
        layer.acquireFenceFd = bufs[0].preFenceFd;
        layer.handle = &bufs[0].buffer->Base;
        layer.transform = conf->overlay[0].transform;
        layer.src = conf->overlay[0].src;
        /* A possible scratchBlit will try to avoid the additional crop
         * pass and just return offset values. NvCap does not use that
         * information so we need to adjust the input src rect
         */
        layer.src.left = conf->overlay[0].src.left + conf->overlay[0].offset.x;
        layer.src.top = conf->overlay[0].src.top + conf->overlay[0].offset.y;
        layer.src.right = conf->overlay[0].src.right + conf->overlay[0].offset.x;
        layer.src.bottom = conf->overlay[0].src.bottom + conf->overlay[0].offset.y;
        layer.dst = conf->overlay[0].dst;

        // nvcap takes ownership of this fd
        bufs[0].preFenceFd = NVSYNC_INIT_INVALID;
    }
    layer.protect = conf->protect;

    ret = dpy->nvcap.procs->post(dpy->nvcap.ctx, &layer, releaseFenceFd);
    if (ret != 0) {
        nvsync_close(layer.acquireFenceFd);
    }

    close_fences(dev, dpy, bufs, 0);

    return ret;
}

static void nvcap_init_display(struct nvfb_device *dev)
{
    struct nvfb_display *dpy = &dev->displays[NVFB_DISPLAY_WFD];
    struct fb_var_screeninfo *mode;

    dpy->type = NVFB_DISPLAY_WFD;
    dpy->blank = 1;
    dpy->caps = nvcap_display_caps;
    dpy->vblank.syncpt = NVRM_INVALID_SYNCPOINT_ID;

    dpy->procs.blank = nvcap_blank;
    dpy->procs.post = nvcap_post;
    dpy->procs.reserve_bw = null_reserve_bw;
    dpy->procs.set_mode = null_set_mode;
    dpy->procs.vblank_wait = nvcap_vblank_wait;

    /* WFD supports only a single mode at a time */
    mode = dpy->mode.modedb;
    dpy->mode.num_modes = 1;
    dpy->mode.modes[0] = mode;
    dpy->mode.current = mode;

    /* Set the default mode */
    create_fake_mode(mode, default_xres, default_yres, default_refresh);
    update_vblank_period(dpy);
}
#endif /* NVCAP_VIDEO_ENABLED */

#include <android/configuration.h>
static int get_bucketed_dpi(int dpi)
{
    int dpi_bucket[] = {
        ACONFIGURATION_DENSITY_DEFAULT, ACONFIGURATION_DENSITY_LOW,
        ACONFIGURATION_DENSITY_MEDIUM, ACONFIGURATION_DENSITY_TV,
        ACONFIGURATION_DENSITY_HIGH, ACONFIGURATION_DENSITY_XHIGH,
        ACONFIGURATION_DENSITY_XXHIGH, ACONFIGURATION_DENSITY_NONE
    };
    int ii;

    for (ii = 1; ii < (int)NV_ARRAY_SIZE(dpi_bucket); ii++) {
        /* Find the range of bucketed values that contain the value
         * specified in dpi and choose the bucket that is closest.
         */
        if (dpi <= dpi_bucket[ii]) {
            dpi = dpi_bucket[ii] - dpi > dpi - dpi_bucket[ii-1] ?
                dpi_bucket[ii-1] : dpi_bucket[ii];
            break;
        }
    }

    return dpi;
}

static void set_display_properties(struct nvfb_device *dev)
{
    char value[PROPERTY_VALUE_MAX];

    /* Normally the lcd_density property is set in system.prop but
     * some reference boards support multiple panels and therefore
     * cannot reliably set this value.  If the override property is
     * set and the lcd_density is not, compute the density from the
     * panel mode and set it here.
     */
    property_get("ro.sf.override_null_lcd_density", value, "0");
    if (strcmp(value, "0")) {
        property_get("ro.sf.lcd_density", value, "0");
        if (!strcmp(value, "0")) {
            struct nvfb_display *dpy = dev->active_display[HWC_DISPLAY_PRIMARY];
            struct fb_var_screeninfo *mode = dpy->mode.current;
            int dpi;

            if ((int)mode->width > 0) {
                dpi = (int) (dev->primary.xres * 25.4f / mode->width + 0.5f);
            } else {
                dpi = 160;
            }

            // lcd density needs to be set to a bucketed dpi value.
            sprintf(value, "%d", get_bucketed_dpi(dpi));
            property_set("ro.sf.lcd_density", value);
        }
    }

    /* XXX get rid of this please */
    sprintf(value, "%d", dev->primary.xres);
    property_set("persist.sys.NV_DISPXRES", value);
    sprintf(value, "%d", dev->primary.yres);
    property_set("persist.sys.NV_DISPYRES", value);
}

void nvfb_set_transform(struct nvfb_device *dev, int disp, int transform,
                        struct nvfb_hotplug_status *hotplug)
{
    if (transform & HWC_TRANSFORM_ROT_90) {
        transform ^= HWC_TRANSFORM_ROT_180;
    }

    pthread_mutex_lock(&dev->hotplug.mutex);

    if (disp == HWC_DISPLAY_PRIMARY) {
        struct nvfb_display *dpy = dev->active_display[disp];

        dev->primary.transform = transform;

        if (dpy->type != NVFB_DISPLAY_PANEL) {
            dpy->hotplug.status.transform = transform;
        }
        *hotplug = dpy->hotplug.status;
    }

    pthread_mutex_unlock(&dev->hotplug.mutex);
}

static void set_primary_resolution(struct nvfb_device *dev)
{
    struct nvfb_display *dpy = dev->active_display[HWC_DISPLAY_PRIMARY];
    int xres = dpy->mode.current->xres;
    int yres = dpy->mode.current->yres;
    char value[PROPERTY_VALUE_MAX];

    if (dev->hwc_props->fixed.display_resolution.xres > 0 &&
        dev->hwc_props->fixed.display_resolution.yres > 0) {
        xres = dev->hwc_props->fixed.display_resolution.xres;
        yres = dev->hwc_props->fixed.display_resolution.yres;
    }

    if (dpy->hotplug.status.transform & HWC_TRANSFORM_ROT_90) {
        NVGR_SWAP(xres, yres);
    }

    dev->primary.xres = xres;
    dev->primary.yres = yres;
}

static int get_rotation_info(struct nvfb_display *dpy,
                             const hwc_props_t *hwc_props)
{
    int disp = dpy->dc.handle;
    int degrees = 0;
    char path[FILENAME_MAX];
    const char *rotate_template =
        "/sys/class/graphics/fb%u/device/panel_rotation";
    struct stat stat_buf;

    if (hwc_props->fixed.panel_rotation) {
        return hwc_props->fixed.panel_rotation;
    }

    snprintf(path, sizeof(path), rotate_template, disp);
    if (!stat(path, &stat_buf)) {
        int fd_rotate;
        fd_rotate = open(path, O_RDONLY);

        if (fd_rotate != -1) {
            char buf[20] = {0};

            ssize_t count = read(fd_rotate, buf, sizeof(buf));
            if (count < 1) {
                ALOGE("Failed reading rotation info for device %d: %s, %d\n",
                      disp, strerror(errno), count);
                NV_ASSERT(0);
            }
            degrees = atoi(buf);
            close(fd_rotate);
        } else {
            ALOGE("Rotation skipped for device %d: %s\n", disp, strerror(errno));
            NV_ASSERT(0);
        }
    }

    return rotation_to_transform(degrees);
}

void nvfb_get_hotplug_status(struct nvfb_device *dev, int disp,
                             struct nvfb_hotplug_status *hotplug)
{
    struct nvfb_display *dpy;


    NV_ASSERT(disp >= 0 && disp < HWC_NUM_DISPLAY_TYPES);
    hotplug->value = 0;
    if (dev->active_display[disp]) {
        *hotplug = dev->active_display[disp]->hotplug.status;
    }
}

static int get_bw_events(struct nvfb_display *dpy)
{

    if (dpy->caps.display_cap & NVFB_DISPLAY_CAP_BANDWIDTH_NEGOTIATE) {
        return TEGRA_DC_EXT_EVENT_BANDWIDTH_INC |
            TEGRA_DC_EXT_EVENT_BANDWIDTH_DEC;
    }

    return 0;
}

int nvfb_open(struct nvfb_device **fbdev, struct nvfb_callbacks *callbacks,
              hwc_props_t *hwc_props)
{
    struct nvfb_device *dev;
    hw_module_t const *module;
    struct nvfb_display *panel, *hdmi;
    int ctrl_events = 0;
    int ii;

    dev = malloc(sizeof(struct nvfb_device));
    if (!dev) {
        return -1;
    }

    memset(dev, 0, sizeof(struct nvfb_device));
    dev->ctrl_fd = -1;
    for (ii = 0; ii < NVFB_MAX_DISPLAYS; ii++) {
        dev->displays[ii].dc.fd = -1;
        dev->displays[ii].fb.fd = -1;
        /* blank state unknown */
        dev->displays[ii].blank = -1;
    }

    dev->callbacks = *callbacks;

    hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    dev->gralloc = (NvGrModule *) module;

    dev->hwc_props = hwc_props;
    dev->cursor_mode = NvFbCursorMode_None;

    /* Enumerate displays */
    if (hwc_props->fixed.null_display) {
        if (hwc_props->fixed.hdmi_primary) {
            ALOGE("null display and hdmi_primary are incompatible");
            goto fail;
        }
        if (hwc_props->fixed.stb_mode) {
            ALOGE("null display and stb_mode are incompatible");
            goto fail;
        }
        init_null_display(dev);
    } else {
        if (nvfb_get_displays(dev)) {
            ALOGE("Can't get display information: %s", strerror(errno));
            goto fail;
        }
    }

    /* Set up active displays */
    panel = &dev->displays[NVFB_DISPLAY_PANEL];
    hdmi = &dev->displays[NVFB_DISPLAY_HDMI];

    if ((!is_valid(panel) && !hwc_props->fixed.null_display) ||
        hwc_props->fixed.hdmi_primary) {
        dev->active_display[HWC_DISPLAY_PRIMARY] = hdmi;
        dev->active_display[HWC_DISPLAY_EXTERNAL] = NULL;
        dev->default_external = NVFB_DISPLAY_NONE;
    } else if (hwc_props->fixed.stb_mode) {
        if (hdmi->connected) {
            dev->active_display[HWC_DISPLAY_PRIMARY] = hdmi;
        } else {
            dev->active_display[HWC_DISPLAY_PRIMARY] = panel;
        }
        dev->active_display[HWC_DISPLAY_EXTERNAL] = NULL;
        dev->default_external = NVFB_DISPLAY_NONE;
    } else {
        dev->active_display[HWC_DISPLAY_PRIMARY] = panel;
        dev->active_display[HWC_DISPLAY_EXTERNAL] = hdmi;
        dev->default_external = NVFB_DISPLAY_HDMI;
    }
    dev->active_display[HWC_DISPLAY_PRIMARY]->primary = 1;

    nvfb_init_displays(dev);

    if (!is_valid(dev->active_display[HWC_DISPLAY_PRIMARY]) &&
        !hwc_props->fixed.null_display) {
        goto fail;
    }
    if (!is_valid(dev->active_display[HWC_DISPLAY_EXTERNAL])) {
        dev->active_display[HWC_DISPLAY_EXTERNAL] = NULL;
    }

    /* Set up display mask */
    for (ii = 0; ii < HWC_NUM_DISPLAY_TYPES; ii++) {
        if (dev->active_display[ii]) {
            dev->display_mask |= 1 << ii;
        }
    }

    /* Use panel by default for vblank waits */
    update_vblank_display(dev);
    NV_ASSERT(dev->vblank_display);

    pthread_mutex_init(&dev->hotplug.mutex, NULL);

    /* Display-specific initialization */
    if (is_valid(panel)) {
        pthread_mutex_lock(&dev->hotplug.mutex);
        panel->hotplug.status.type = NVFB_DISPLAY_PANEL;
        panel->hotplug.status.connected = panel->connected;
        panel->hotplug.status.transform = get_rotation_info(panel, hwc_props);
        panel->hotplug.status.generation++;
        pthread_mutex_unlock(&dev->hotplug.mutex);

        panel->didim = didim_open(hwc_props);

        ctrl_events = get_bw_events(panel);
    }

    if (is_valid(hdmi)) {
        pthread_mutex_lock(&dev->hotplug.mutex);
        hdmi->hotplug.status.type = NVFB_DISPLAY_HDMI;
        hdmi->hotplug.status.connected = hdmi->connected;
        hdmi->hotplug.status.stereo = hdmi->connected ? hdmi->mode.stereo : 0;
        hdmi->hotplug.status.generation++;
        pthread_mutex_unlock(&dev->hotplug.mutex);

        ctrl_events |= (TEGRA_DC_EXT_EVENT_HOTPLUG | get_bw_events(hdmi));

        if (pipe(dev->hotplug.pipe) < 0) {
            ALOGE("pipe failed: %s", strerror(errno));
            goto fail;
        }

        if (pthread_create(&dev->hotplug.thread, NULL, hotplug_thread, dev)) {
            ALOGE("Can't create hotplug thread: %s\n", strerror(errno));
            goto fail;
        }

        nvfb_hdcp_open(&dev->hdcp, hdmi->dc.handle);

        if (hdmi->connected) {
            nvfb_hdcp_enable(dev->hdcp);
        }
    }

    if (ctrl_events != 0 && !hwc_props->fixed.null_display) {

        /* Register events. */
        if (ioctl(dev->ctrl_fd, TEGRA_DC_EXT_CONTROL_SET_EVENT_MASK,
                  ctrl_events)) {
            ALOGE("Failed to register control events (0x%x): %s\n",
                  ctrl_events, strerror(errno));
            goto fail;
        }
    }

#if NVCAP_VIDEO_ENABLED
    /* Start nvcap service */
    if (!nvfb_nvcap_open(dev)) {
        nvcap_init_display(dev);
    }
#endif

    set_primary_resolution(dev);
    set_display_properties(dev);

    *fbdev = dev;

    return 0;

fail:
    nvfb_close(dev);
    return -errno;
}

void nvfb_close(struct nvfb_device *dev)
{
    int ii;

    if (!dev) {
        return;
    }

#if NVCAP_VIDEO_ENABLED
    nvfb_nvcap_close(dev);
#endif

    if (dev->no_capture) {
        NvGrModule *m = dev->gralloc;
        m->free_scratch(m, dev->no_capture);
    }

    if (is_valid(&dev->displays[NVFB_DISPLAY_HDMI])) {
        nvfb_hdcp_close(dev->hdcp);

        if (dev->hotplug.thread) {
            int status;
            uint8_t value = 1;

            /* Notify the hotplug thread to terminate */
            if (write(dev->hotplug.pipe[1], &value, sizeof(value)) < 0) {
                ALOGE("hotplug thread: write failed: %s", strerror(errno));
            }

            /* Wait for the thread to exit */
            status = pthread_join(dev->hotplug.thread, NULL);
            if (status) {
                ALOGE("hotplug thread: thread_join: %d", status);
            }

            close(dev->hotplug.pipe[0]);
            close(dev->hotplug.pipe[1]);
        }
    }

    pthread_mutex_destroy(&dev->hotplug.mutex);

    for (ii = 0; ii < NVFB_MAX_DC_TYPES; ii++) {
        nvfb_close_display(&dev->displays[ii]);
    }

    close(dev->ctrl_fd);

    free(dev);
}

#if 0
/* This will eventually be part of dumpsys */

#define FB_PREFERRED_FORMAT HAL_PIXEL_FORMAT_RGBA_8888

// bpp, red, green, blue, alpha
static const int fb_format_map[][9] = {
    { 0,  0,  0,  0,  0,  0,  0,  0,  0}, // INVALID
    {32,  0,  8,  8,  8, 16,  8, 24,  8}, // HAL_PIXEL_FORMAT_RGBA_8888
    {32,  0,  8,  8,  8, 16,  8,  0,  0}, // HAL_PIXEL_FORMAT_RGBX_8888
    {24, 16,  8,  8,  8,  0,  8,  0,  0}, // HAL_PIXEL_FORMAT_RGB_888
    {16, 11,  5,  5,  6,  0,  5,  0,  0}, // HAL_PIXEL_FORMAT_RGB_565
    {32, 16,  8,  8,  8,  0,  8, 24,  8}, // HAL_PIXEL_FORMAT_BGRA_8888
    {16, 11,  5,  6,  5,  1,  5,  0,  1}, // HAL_PIXEL_FORMAT_RGBA_5551
    {16, 12,  4,  8,  4,  4,  4,  0,  4}  // HAL_PIXEL_FORMAT_RGBA_4444
};

static int FillFormat (struct fb_var_screeninfo* info, int format)
{
    const int *p;

    if (format == 0 || format > HAL_PIXEL_FORMAT_RGBA_4444)
        return -ENOTSUP;

    p = fb_format_map[format];
    info->bits_per_pixel = *(p++);
    info->red.offset = *(p++);
    info->red.length = *(p++);
    info->green.offset = *(p++);
    info->green.length = *(p++);
    info->blue.offset = *(p++);
    info->blue.length = *(p++);
    info->transp.offset = *(p++);
    info->transp.length = *(p++);
    return 0;
}

static int MapFormat (struct fb_var_screeninfo* info)
{
    int i;
    struct fb_var_screeninfo cmp;

    for (i = 1; i <= HAL_PIXEL_FORMAT_RGBA_4444; i++)
    {
        FillFormat(&cmp, i);
        if ((info->bits_per_pixel == cmp.bits_per_pixel) &&
            (info->red.offset == cmp.red.offset) &&
            (info->red.length == cmp.red.length) &&
            (info->green.offset == cmp.green.offset) &&
            (info->green.length == cmp.green.length) &&
            (info->blue.offset == cmp.blue.offset) &&
            (info->blue.length == cmp.blue.length) &&
            (info->transp.offset == cmp.transp.offset) &&
            (info->transp.length == cmp.transp.length))
            return i;
    }
    return 0;
}

void nvfb_dump(struct nvfb_device *dev, char *buff, int buff_len)
{
    struct fb_fix_screeninfo finfo;

    if (dev->null_display) {
        strcpy(finfo.id, "null display");
    } else if (dev->display_mask & PRIMARY) {
        int pixformat;

        if (ioctl(dev->displays[NVFB_DISPLAY_PANEL].fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
            ALOGE("Can't get FSCREENINFO: %s\n", strerror(errno));
            goto fail;
        }

        pixformat = MapFormat(mode);
        if (pixformat == 0) {
            ALOGE("Unrecognized framebuffer pixel format\n");
            errno = EINVAL;
            goto fail;
        } else if (pixformat != FB_PREFERRED_FORMAT) {
            ALOGW("Using format %d instead of preferred %d\n",
                  pixformat, FB_PREFERRED_FORMAT);
        }
    }

    ALOGI("using (fd=%d)\n"
         "id           = %s\n"
         "xres         = %d px\n"
         "yres         = %d px\n"
         "xres_virtual = %d px\n"
         "yres_virtual = %d px\n"
         "bpp          = %d\n"
         "r            = %2u:%u\n"
         "g            = %2u:%u\n"
         "b            = %2u:%u\n"
         "smem_start   = 0x%08lx\n"
         "smem_len     = 0x%08x\n",
         dev->displays[NVFB_DISPLAY_PANEL].fd,
         finfo.id,
         info.xres,
         info.yres,
         info.xres_virtual,
         info.yres_virtual,
         info.bits_per_pixel,
         info.red.offset, info.red.length,
         info.green.offset, info.green.length,
         info.blue.offset, info.blue.length,
         finfo.smem_start,
         finfo.smem_len
    );
    ALOGI("format       = %d\n"
         "stride       = %d\n"
         "width        = %d mm (%f dpi)\n"
         "height       = %d mm (%f dpi)\n"
         "refresh rate = %.2f Hz\n",
         pixformat,
         dev->Base.stride,
         info.width,  dev->Base.xdpi,
         info.height, dev->Base.ydpi,
         dev->Base.fps
    );
}
#endif
