/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef _NVHWC_PROPS_H
#define _NVHWC_PROPS_H

/*
 * The primary compositor for layers that are not sent directly to display.
 * Possible values:
 * - surfaceflinger, Google GL multi-pass compositor
 * - gralloc, NVIDIA 2d hardware compositor
 * - glcomposer, NVIDIA GL multi-texturing compositor
 * - gldrawtexture, NVIDIA GL_NV_draw_texture compositor
 */
#define NV_PROPERTY_COMPOSITOR                       TEGRA_PROP(compositor)
#define NV_PROPERTY_COMPOSITOR_DEFAULT               NV_PROPERTY_COMPOSITOR_SurfaceFlinger
#define NV_PROPERTY_COMPOSITOR_SurfaceFlinger        surfaceflinger
#define NV_PROPERTY_COMPOSITOR_Gralloc               gralloc
#define NV_PROPERTY_COMPOSITOR_GLComposer            glcomposer
#define NV_PROPERTY_COMPOSITOR_GLDrawTexture         gldrawtexture

/*
 * The policy to decide composition strategy.
 * Possible values:
 * - composite-always, composite all overlays into framebuffer
 * - assign-windows, attempt to assign all overlays to display windows
 * - auto, use framebuffer cache and composite on idle to optimise workload
 */
#define NV_PROPERTY_COMPOSITE_POLICY                 TEGRA_PROP(composite.policy)
#define NV_PROPERTY_COMPOSITE_POLICY_DEFAULT         NV_PROPERTY_COMPOSITE_POLICY_Auto
#define NV_PROPERTY_COMPOSITE_POLICY_Auto            auto
#define NV_PROPERTY_COMPOSITE_POLICY_CompositeAlways composite-always
#define NV_PROPERTY_COMPOSITE_POLICY_AssignWindows   assign-windows

/*
 * When the primary compositor is not capable of handling composition, this
 * controls whether other compositors should be tried.
 * Disabling this is intended for test usage only.
 */
#define NV_PROPERTY_COMPOSITE_FALLBACK               TEGRA_PROP(composite.fallb)
#define NV_PROPERTY_COMPOSITE_FALLBACK_DEFAULT       1

/*
 * The preferred resolution to use for HDMI displays.
 * Possible values:
 * - Vga
 * - 480p
 * - 576p
 * - 720p
 * - 1080p
 * - 2160p
 * - Max, maximum that the HDMI display supports
 * - Auto, resolution that is closest to internal panel
 */
#define NV_PROPERTY_HDMI_RESOLUTION                  TEGRA_PROP(hdmi.resolution)
#define NV_PROPERTY_HDMI_RESOLUTION_DEFAULT          NV_PROPERTY_HDMI_RESOLUTION_Auto
#define NV_PROPERTY_HDMI_RESOLUTION_Vga              Vga
#define NV_PROPERTY_HDMI_RESOLUTION_480p             480p
#define NV_PROPERTY_HDMI_RESOLUTION_576p             576p
#define NV_PROPERTY_HDMI_RESOLUTION_720p             720p
#define NV_PROPERTY_HDMI_RESOLUTION_1080p            1080p
#define NV_PROPERTY_HDMI_RESOLUTION_2160p            2160p
#define NV_PROPERTY_HDMI_RESOLUTION_Max              Max
#define NV_PROPERTY_HDMI_RESOLUTION_Auto             Auto

/*
 * Use HDMI output as primary display.
 */
#define NV_PROPERTY_HDMI_PRIMARY                     TEGRA_PROP(hdmi.primary)
#define NV_PROPERTY_HDMI_PRIMARY_DEFAULT             0

/* Settop box mode disables the panel when HDMI is present and
 * hot-swaps the primary display to HDMI.
 */
#define NV_PROPERTY_STB_MODE                         TEGRA_PROP(stb.mode)
#define NV_PROPERTY_STB_MODE_DEFAULT                 0

/*
 * Forced framebuffer resolution - overrides panel resolution, and is
 * also used for HDMI-as-primary and NULL display.  Content will scale
 * to the physical display size.
 */
#define NV_PROPERTY_DISPLAY_RESOLUTION               SYS_PROP(display.resolution)
#define NV_PROPERTY_DISPLAY_RESOLUTION_DEFAULT       ""

/*
 * For HDMI output, if one of the layers contains a video or camera preview then
 * only that layer is sent to display.
 * Videoview layers must have gralloc usage flags:
 * - GRALLOC_USAGE_EXTERNAL_DISP
 * - NVGR_USAGE_CLOSED_CAP
 */
#define NV_PROPERTY_HDMI_VIDEOVIEW                   SYS_PROP(hdmi.videoview)
#define NV_PROPERTY_HDMI_VIDEOVIEW_DEFAULT           0

/*
 * Enable use of hardware cursor window for appropriate overlay.
 */
#define NV_PROPERTY_CURSOR_ENABLE                    TEGRA_PROP(cursor.enable)
#define NV_PROPERTY_CURSOR_ENABLE_DEFAULT            0

/*
 * Enable use of hardware cursor window for asynchronous setCursor
 * extension.
 */
#define NV_PROPERTY_CURSOR_ASYNC                     TEGRA_PROP(cursor.async)
#define NV_PROPERTY_CURSOR_ASYNC_DEFAULT             1

/*
 * Enable support for external stereo displays.
 */
#define NV_PROPERTY_STEREO_ENABLE                    SYS_PROP(NV_STEREOCTRL)
#define NV_PROPERTY_STEREO_ENABLE_DEFAULT            0

/*
 * Enable smartdimmer on display controller.
 */
#define NV_PROPERTY_DIDIM_ENABLE                     TEGRA_PROP(didim.enable)
#define NV_PROPERTY_DIDIM_ENABLE_DEFAULT             1

/*
 * Default smartdimmer aggressiveness for normal content.
 * Possible range is between 1 (min) and 5 (max).
 */
#define NV_PROPERTY_DIDIM_NORMAL                     TEGRA_PROP(didim.normal)
#define NV_PROPERTY_DIDIM_NORMAL_DEFAULT             3

/*
 * Default smartdimmer aggressiveness for video content.
 * Possible range is between 1 (min) and 5 (max).
 */
#define NV_PROPERTY_DIDIM_VIDEO                      TEGRA_PROP(didim.video)
#define NV_PROPERTY_DIDIM_VIDEO_DEFAULT              5

/*
 * Force a default rotation on the primary display.
 */
#define NV_PROPERTY_PANEL_ROTATION                   TEGRA_PROP(panel.rotation)
#define NV_PROPERTY_PANEL_ROTATION_DEFAULT           0

/*
 * This property controls the detection of an idle display based on frame rate.
 * Values can be a positive integer or 0 to disable.
 */
#define NV_PROPERTY_IDLE_MINIMUM_FPS                 TEGRA_PROP(idle.minimum_fps)
#define NV_PROPERTY_IDLE_MINIMUM_FPS_DEFAULT         IDLE_MINIMUM_FPS

/*
 * Prevent usage of display controller hardware.
 * Enabling this is intended for test usage only.
 */
#define NV_PROPERTY_NULL_DISPLAY                     "nvidia.hwc.null_display"
#define NV_PROPERTY_NULL_DISPLAY_DEFAULT             0

/*
 * Allow configuration properties to be scanned each frame and hence alter the
 * behaviour of hwcomposer whilst it is running.
 * Enabling this is intended for test usage only.
 */
#define NV_PROPERTY_SCAN_PROPS                       "nvidia.hwc.scan_props"
#define NV_PROPERTY_SCAN_PROPS_DEFAULT               0

/*
 * Print information to the log each frame about the input layers.
 * Enabling this is intended for test usage only.
 */
#define NV_PROPERTY_DUMP_LAYERLIST                   "nvidia.hwc.dump_layerlist"
#define NV_PROPERTY_DUMP_LAYERLIST_DEFAULT           0

/*
 * Print information to the log each frame about composition config.
 * Enabling this is intended for test usage only.
 */
#define NV_PROPERTY_DUMP_CONFIG                      "nvidia.hwc.dump_config"
#define NV_PROPERTY_DUMP_CONFIG_DEFAULT              0

/*
 * Dump buffers contents of windows each frame before sending to display.
 * Enabling this is intended for test usage only.
 */
#define NV_PROPERTY_DUMP_WINDOWS                     "nvidia.hwc.dump_windows"
#define NV_PROPERTY_DUMP_WINDOWS_DEFAULT             0

/*
 * Add markers to ftrace for the hwcomposer set API call.
 * Enabling this is intended for test usage only.
 */
#define NV_PROPERTY_FTRACE_ENABLE                    "nvidia.hwc.ftrace_enable"
#define NV_PROPERTY_FTRACE_ENABLE_DEFAULT            0

/*
 * IMP is a mechanism to decrease the number of overlays to decrease
 * the used isochronous bandwidth.
 * When disabled, HWC will use normal overlay assigment without
 * reserving bandhwidth for it.
 */
#define NV_PROPERTY_IMP_ENABLE                       "nvidia.hwc.imp_enable"
#define NV_PROPERTY_IMP_ENABLE_DEFAULT               1

/*
 * Prevent access to EGL and the GPU hardware.
 * Enabling this is intended for test usage only.
 */
#define NV_PROPERTY_NO_EGL                           "nvidia.hwc.no_egl"
#define NV_PROPERTY_NO_EGL_DEFAULT                   0

typedef struct hwc_fixed_props {
    NvBool              hdmi_primary;
    NvBool              hdmi_videoview;
    NvBool              stb_mode;
    NvBool              cursor_enable;
    NvBool              cursor_async;
    NvBool              stereo_enable;
    NvBool              didim_enable;
    int                 didim_normal;
    int                 didim_video;
    NvBool              null_display;
    struct {
        int             xres;
        int             yres;
    }                   display_resolution;
    int                 panel_rotation;
    NvBool              scan_props;
    NvBool              no_egl;
} hwc_fixed_props_t;

typedef struct hwc_dynamic_props {
    HdmiResolution      hdmi_resolution;
    HWC_Compositor      compositor;
    HWC_CompositePolicy composite_policy;
    NvBool              composite_fallback;
    int                 idle_minimum_fps;
    NvBool              dump_layerlist;
    NvBool              dump_config;
    NvBool              dump_windows;
    NvBool              ftrace_enable;
    NvBool              imp_enable;
} hwc_dynamic_props_t;

typedef struct hwc_props {
    hwc_fixed_props_t   fixed;
    hwc_dynamic_props_t dynamic;
} hwc_props_t;

void
hwc_props_init(hwc_props_t *props);

void
hwc_props_scan(hwc_props_t *props);

int
hwc_set_prop(struct nvhwc_context *ctx,
             const char *prop,
             const char *value);

#endif /* ifndef _NVHWC_PROPS_H */
