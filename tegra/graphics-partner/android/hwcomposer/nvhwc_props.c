/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <stdlib.h>
#include <string.h>
#include "nvcommon.h"
#include "nvhwc.h"
#include "nvfb.h"
#include "nvhwc_props.h"

#define FIXED_LIST(_MACRO)      \
    _MACRO( HDMI_PRIMARY,       parse_hdmi_primary       ) \
    _MACRO( HDMI_VIDEOVIEW,     parse_hdmi_videoview     ) \
    _MACRO( STB_MODE,           parse_stb_mode           ) \
    _MACRO( CURSOR_ENABLE,      parse_cursor_enable      ) \
    _MACRO( CURSOR_ASYNC,       parse_cursor_async       ) \
    _MACRO( STEREO_ENABLE,      parse_stereo_enable      ) \
    _MACRO( DIDIM_ENABLE,       parse_didim_enable       ) \
    _MACRO( DIDIM_NORMAL,       parse_didim_normal       ) \
    _MACRO( DIDIM_VIDEO,        parse_didim_video        ) \
    _MACRO( NULL_DISPLAY,       parse_null_display       ) \
    _MACRO( DISPLAY_RESOLUTION, parse_display_resolution ) \
    _MACRO( PANEL_ROTATION,     parse_panel_rotation     ) \
    _MACRO( SCAN_PROPS,         parse_scan_props         ) \
    _MACRO( NO_EGL,             parse_no_egl             )

#define DYNAMIC_LIST(_MACRO)      \
    _MACRO( HDMI_RESOLUTION,    parse_hdmi_resolution    ) \
    _MACRO( COMPOSITOR,         parse_compositor         ) \
    _MACRO( COMPOSITE_POLICY,   parse_composite_policy   ) \
    _MACRO( COMPOSITE_FALLBACK, parse_composite_fallback ) \
    _MACRO( IDLE_MINIMUM_FPS,   parse_idle_minimum_fps   ) \
    _MACRO( DUMP_LAYERLIST,     parse_dump_layerlist     ) \
    _MACRO( DUMP_CONFIG,        parse_dump_config        ) \
    _MACRO( DUMP_WINDOWS,       parse_dump_windows       ) \
    _MACRO( FTRACE_ENABLE,      parse_ftrace_enable      ) \
    _MACRO( IMP_ENABLE,         parse_imp_enable         )

#define TO_STR(p) #p
#define PROP_STR(p) TO_STR(p)

enum {
    PROP_CHANGED,
    PROP_UNCHANGED,
    PARSE_ERROR
};

static int
parse_bool(NvBool *var,
           const char *name,
           const char *value)
{
    NvBool b;

    if (strcmp(value, "1") == 0 ||
        strcasecmp(value, "on") == 0 ||
        strcasecmp(value, "true") == 0) {
        b = NV_TRUE;
    } else if (strcmp(value, "0") == 0 ||
        strcasecmp(value, "off") == 0 ||
        strcasecmp(value, "false") == 0) {
        b = NV_FALSE;
    } else {
        ALOGW("Invalid %s property: %s\n", name, value);
        return PARSE_ERROR;
    }

    if (*var == b)
        return PROP_UNCHANGED;

    *var = b;
    ALOGD("%s set to: %s", name, value);
    return PROP_CHANGED;
}

static int
parse_compositor(hwc_props_t *props,
                 const char *value)
{
    #define PARSE(c) \
    if (strcasecmp(value, PROP_STR(NV_PROPERTY_COMPOSITOR_##c)) == 0) { \
        if (props->dynamic.compositor == HWC_Compositor_##c) {          \
            return PROP_UNCHANGED;                                      \
        }                                                               \
        props->dynamic.compositor = HWC_Compositor_##c;                 \
        ALOGD("Compositor set to: %s", value);                          \
        return PROP_CHANGED;                                            \
    }
    PARSE(SurfaceFlinger);
    PARSE(Gralloc);
    PARSE(GLComposer);
    PARSE(GLDrawTexture);
    #undef PARSE

    ALOGE("Unsupported %s: %s", NV_PROPERTY_COMPOSITOR, value);
    return PARSE_ERROR;
}

static int
parse_composite_policy(hwc_props_t *props,
                       const char *value)
{
    #define PARSE(c) \
    if (strcasecmp(value, PROP_STR(NV_PROPERTY_COMPOSITE_POLICY_##c)) == 0) { \
        if (props->dynamic.composite_policy == HWC_CompositePolicy_##c) {     \
            return PROP_UNCHANGED;                                            \
        }                                                                     \
        props->dynamic.composite_policy = HWC_CompositePolicy_##c;            \
        ALOGD("Composite policy set to: %s", value);                          \
        return PROP_CHANGED;                                                  \
    }
    PARSE(Auto);
    PARSE(CompositeAlways);
    PARSE(AssignWindows);
    #undef PARSE

    ALOGE("Unsupported %s: %s", NV_PROPERTY_COMPOSITE_POLICY, value);
    return PARSE_ERROR;
}

static int
parse_composite_fallback(hwc_props_t *props,
                         const char *value)
{
    return parse_bool(&props->dynamic.composite_fallback,
                      "composite_fallback",
                      value);
}

static int
parse_hdmi_resolution(hwc_props_t *props,
                      const char *value)
{
    #define PARSE(c) \
    if (strcasecmp(value, PROP_STR(NV_PROPERTY_HDMI_RESOLUTION_##c)) == 0) { \
        if (props->dynamic.hdmi_resolution == HdmiResolution_##c) {          \
            return PROP_UNCHANGED;                                           \
        }                                                                    \
        props->dynamic.hdmi_resolution = HdmiResolution_##c;                 \
        ALOGD("HDMI preferred resolution set to: %s", value);                \
        return PROP_CHANGED;                                                 \
    }
    PARSE(Vga);
    PARSE(480p);
    PARSE(576p);
    PARSE(720p);
    PARSE(1080p);
    PARSE(2160p);
    PARSE(Max);
    PARSE(Auto);
    #undef PARSE

    ALOGW("Invalid HDMI resolution property: %s\n", value);
    return PARSE_ERROR;
}

static int
parse_hdmi_primary(hwc_props_t *props,
                   const char *value)
{
    return parse_bool(&props->fixed.hdmi_primary, "hdmi_primary", value);
}

static int
parse_hdmi_videoview(hwc_props_t *props,
                     const char *value)
{
    return parse_bool(&props->fixed.hdmi_videoview, "hdmi_videoview", value);
}

static int
parse_stb_mode(hwc_props_t *props,
               const char *value)
{
    return parse_bool(&props->fixed.stb_mode, "stb_mode", value);
}

static int
parse_cursor_enable(hwc_props_t *props,
                    const char *value)
{
    return parse_bool(&props->fixed.cursor_enable, "cursor_enable", value);
}

static int
parse_cursor_async(hwc_props_t *props,
                   const char *value)
{
    return parse_bool(&props->fixed.cursor_async, "cursor_async", value);
}

static int
parse_stereo_enable(hwc_props_t *props,
                    const char *value)
{
    return parse_bool(&props->fixed.stereo_enable, "stereo_enable", value);
}

static int
parse_didim_enable(hwc_props_t *props,
                   const char *value)
{
    return parse_bool(&props->fixed.didim_enable, "didim_enable", value);
}

static int
parse_didim_normal(hwc_props_t *props,
                   const char *value)
{
    int level = atoi(value);
    if (level > 0) {
        if (props->fixed.didim_normal == level) {
            return PROP_UNCHANGED;
        }
        props->fixed.didim_normal = level;
        ALOGD("didim_normal set to: %s\n", value);
        return PROP_CHANGED;

    }

    ALOGW("Invalid didim_normal property: %s\n", value);
    return PARSE_ERROR;
}

static int
parse_didim_video(hwc_props_t *props,
                  const char *value)
{
    int level = atoi(value);
    if (level > 0) {
        if (props->fixed.didim_video == level) {
            return PROP_UNCHANGED;
        }
        props->fixed.didim_video = level;
        ALOGD("didim_video set to: %s\n", value);
        return PROP_CHANGED;
    }

    ALOGW("Invalid didim_video property: %s\n", value);
    return PARSE_ERROR;
}

static int
parse_panel_rotation(hwc_props_t *props,
                     const char *value)
{
    int r;

    r = rotation_to_transform(atoi(value));
    if (r == -1) {
        return PARSE_ERROR;
    }

    if (props->fixed.panel_rotation == r)
        return PROP_UNCHANGED;

    props->fixed.panel_rotation = r;
    ALOGD("Panel rotation set to: %s", value);
    return PROP_CHANGED;
}

static int
parse_idle_minimum_fps(hwc_props_t *props,
                       const char *value)
{
    int fps = atoi(value);
    if (fps >= 0) {
        if (props->dynamic.idle_minimum_fps == fps) {
            return PROP_UNCHANGED;
        }
        props->dynamic.idle_minimum_fps = fps;
        ALOGD("Overriding minimum_fps, new value = %s", value);
        return PROP_CHANGED;
    }

    ALOGW("Invalid idle_minimum_fps property: %s\n", value);
    return PARSE_ERROR;
}

static int
parse_null_display(hwc_props_t *props,
                   const char *value)
{
    return parse_bool(&props->fixed.null_display, "null_display", value);
}

static int
parse_display_resolution(hwc_props_t *props,
                         const char *value)
{
    int xres, yres;

    if (sscanf(value, "%dx%d", &xres, &yres) == 2) {
        if (props->fixed.display_resolution.xres == xres &&
            props->fixed.display_resolution.yres == yres) {
            return PROP_UNCHANGED;
        }
        props->fixed.display_resolution.xres = xres;
        props->fixed.display_resolution.yres = yres;
        ALOGD("Null display size set to: %s", value);
        return PROP_CHANGED;
    }

    ALOGW("Invalid display_resolution property: %s\n", value);
    return PARSE_ERROR;
}

static int
parse_scan_props(hwc_props_t *props,
                 const char *value)
{
    return parse_bool(&props->fixed.scan_props, "scan_props", value);
}

static int
parse_no_egl(hwc_props_t *props,
             const char *value)
{
    return parse_bool(&props->fixed.no_egl, "no_egl", value);
}

static int
parse_dump_layerlist(hwc_props_t *props,
                     const char *value)
{
    return parse_bool(&props->dynamic.dump_layerlist, "dump_layerlist", value);
}

static int
parse_dump_config(hwc_props_t *props,
                  const char *value)
{
    return parse_bool(&props->dynamic.dump_config, "dump_config", value);
}

static int
parse_dump_windows(hwc_props_t *props,
                   const char *value)
{
    return parse_bool(&props->dynamic.dump_windows, "dump_windows", value);
}

static int
parse_ftrace_enable(hwc_props_t *props,
                    const char *value)
{
    return parse_bool(&props->dynamic.ftrace_enable, "ftrace_enable", value);
}

static int
parse_imp_enable(hwc_props_t *props,
                 const char *value)
{
    return parse_bool(&props->dynamic.imp_enable, "imp_enable", value);
}

void
hwc_props_init(hwc_props_t *props)
{
    int result;
    const char *env;
    char sysprop[PROPERTY_VALUE_MAX];

    // PROPERTY_KEY_MAX is the maximum length *including* the NULL-terminator.
    #define INIT(p, f) \
    NV_CT_ASSERT(sizeof(NV_PROPERTY_##p) <= PROPERTY_KEY_MAX); \
    env = getenv(NV_PROPERTY_##p);                             \
    if (env == NULL || f(props, env) == PARSE_ERROR) {         \
        result = property_get(NV_PROPERTY_##p, sysprop, NULL); \
        if (result < 1 || f(props, sysprop) == PARSE_ERROR)    \
            f(props, PROP_STR(NV_PROPERTY_##p##_DEFAULT));     \
    }
    FIXED_LIST(INIT)
    DYNAMIC_LIST(INIT)
    #undef INIT
}

void
hwc_props_scan(hwc_props_t *props)
{
    int result;
    char value[PROPERTY_VALUE_MAX];

    #define SCAN(p, f) \
    result = property_get(NV_PROPERTY_##p, value, NULL); \
    if (result > 0)                                      \
        f(props, value);
    DYNAMIC_LIST(SCAN)
    #undef SCAN
}

int
hwc_set_prop(struct nvhwc_context *ctx,
             const char *prop,
             const char *value)
{
    if (ctx == NULL || prop == NULL || value == NULL)
        return -1;

    #define SET(p, f) \
    if (strcmp(prop, NV_PROPERTY_##p) == 0) \
        return f(&ctx->props, value) == PARSE_ERROR ? -1 : 0;
    DYNAMIC_LIST(SET)
    #undef SET

    return -1;
}
