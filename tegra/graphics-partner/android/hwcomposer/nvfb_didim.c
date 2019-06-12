/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <cutils/atomic.h>

#include "nvassert.h"
#include "nvcommon.h"
#include "nvhwc.h"
#include "nvfb.h"
#include "nvhwc_props.h"
#include "nvfb_didim.h"

#define PATH_TEGRA_DC_0                  "/sys/class/graphics/fb0/device/"
#define PATH_SMARTDIMMER                 PATH_TEGRA_DC_0 "smartdimmer/"
#define PATH_SMARTDIMMER_ENABLE          PATH_SMARTDIMMER "enable"
#define PATH_SMARTDIMMER_AGGRESSIVENESS  PATH_SMARTDIMMER "aggressiveness"
#define PATH_SMARTDIMMER_WINDOW          PATH_SMARTDIMMER "sd_window"
#define PATH_SMARTDIMMER_WINDOW_ENABLE   PATH_SMARTDIMMER "sd_window_enable"

#define DIDIM_LEVEL_MIN 1
#define DIDIM_LEVEL_MAX 5

#define HWC_PRIORITY_LVL (3 << 3)

enum didimEnable {
    didimEnableOff = 0,
    didimEnableOn,
};

enum didimLevel {
    didimLevel1 = 1,
    didimLevel2,
    didimLevel3,
    didimLevel4,
    didimLevel5,
};

enum didimVersion {
    didimVersionNone = 0,
    didimVersion1,
    didimVersion2,
};

struct didim_state {
    struct didim_client client;

    enum didimVersion version;

    /* DIDIM 1 & 2 */
    struct {
        enum didimLevel normal;
        enum didimLevel video;
        enum didimLevel current;
    } level;

    /* DIDIM 2 */
    struct {
        enum didimEnable enable;
        hwc_rect_t rect;
    } window;
};

static int sysfs_exists(const char *pathname)
{
    int fd = open(pathname, O_RDONLY);

    if (fd < 0) {
        return 0;
    }

    close(fd);

    return 1;
}

static int sysfs_read_int(const char *pathname, int *value)
{
    int len, fd = open(pathname, O_RDONLY);
    char buf[20];

    if (fd < 0) {
        ALOGE("%s: open failed: %s", pathname, strerror(errno));
        return -1;
    }

    len = read(fd, buf, sizeof(buf)-1);
    if (len < 0) {
        ALOGE("%s: read failed: %s", pathname, strerror(errno));
    } else {
        buf[len] = '\0';
        *value = atoi(buf);
    }

    close(fd);

    return len < 0 ? len : 0;
}

static int sysfs_write_int(const char *pathname, int value)
{
    int len, fd = open(pathname, O_WRONLY);
    char buf[20];

    if (fd < 0) {
        ALOGE("%s: open failed: %s", pathname, strerror(errno));
        return -1;
    }

    snprintf(buf, sizeof(buf), "%d", value);

    len = write(fd, buf, strlen(buf));
    if (len < 0) {
        ALOGE("%s: write failed: %s", pathname, strerror(errno));
    }

    close(fd);

    return len < 0 ? len : 0;
}

static int sysfs_write_string(const char *pathname, const char *buf)
{
    int len, fd = open(pathname, O_WRONLY);

    if (fd < 0) {
        return -1;
    }

    len = write(fd, buf, strlen(buf));
    if (len < 0) {
        ALOGE("%s: write failed: %s", pathname, strerror(errno));
    }

    close(fd);

    return len < 0 ? len : 0;
}

static int didim_clamp_level(int value)
{
    if (DIDIM_LEVEL_MIN > value) {
        value = DIDIM_LEVEL_MIN;
    } else if (value > DIDIM_LEVEL_MAX) {
        value = DIDIM_LEVEL_MAX;
    }
    return value;
}

static int didim_get_version(const hwc_props_t *hwc_props)
{
    int enable;
    int window_enable;
    int version;
    int status;

    enable = sysfs_exists(PATH_SMARTDIMMER_ENABLE);

    if (!enable) {
        return didimVersionNone;
    }

    window_enable = sysfs_exists(PATH_SMARTDIMMER_WINDOW_ENABLE);

    if (!window_enable) {
        version = didimVersion1;
    } else {
        version = didimVersion2;
    }

    /* Get enable property, if there is one defined */
    enable = hwc_props->fixed.didim_enable;

    /* Set the enable parameter for didim */
    status = sysfs_write_int(PATH_SMARTDIMMER_ENABLE, enable);

    if (status) {
        ALOGE("Failed to set initial DIDIM status to %s",
              enable ? "enable" : "disable");
    }

    /* If enable property is set to 0, we want to set version to None */
    if (!enable) {
        version = didimVersionNone;
    }

    return version;
}

static inline void didim_level(struct didim_state *didim, int level)
{
    int enable, status;

    NV_ASSERT(DIDIM_LEVEL_MIN <= level && level <= DIDIM_LEVEL_MAX);

    status = sysfs_write_int(PATH_SMARTDIMMER_AGGRESSIVENESS,
                             level | HWC_PRIORITY_LVL);
    if (!status) {
        didim->level.current = level;
    } else {
        ALOGE("Failed to set DIDIM aggressiveness\n");
    }
}

static void didim_window_level(struct didim_client *client, hwc_rect_t *rect)
{
    struct didim_state *didim = (struct didim_state *) client;
    enum didimLevel level = rect ? didim->level.video : didim->level.normal;

    NV_ASSERT(didim && didim->version == didimVersion1);

    if (level != didim->level.current) {
        didim_level(didim, level);
    }
}

static void didim_window(struct didim_client *client, hwc_rect_t *rect)
{
    struct didim_state *didim = (struct didim_state *) client;
    char buf[32];
    int status;
    hwc_rect_t validated_rect;

    NV_ASSERT(didim && didim->version == didimVersion2);

    if (!rect) {
        if (didim->window.enable != didimEnableOff) {
            // Disable DIDIM2
            status = sysfs_write_int(PATH_SMARTDIMMER_WINDOW_ENABLE, 0);
            if (status) {
                ALOGE("Failed to disable DIDIM2");
                return;
            }

            didim_level(didim, didim->level.normal);

            didim->window.enable = didimEnableOff;
        }
        return;
    }

    // Clip the rect to be inside the display
    validated_rect.left = NV_MAX(0, rect->left);
    validated_rect.top = NV_MAX(0, rect->top);
    // TODO: should clip right and bottom as well but the
    // smartdimmer_sd_window sysfs entry seems to accept high values
    // while it doesn't accept negative values.
    validated_rect.right = rect->right;
    validated_rect.bottom = rect->bottom;

    // Check if enable status has changed
    if (didim->window.enable == didimEnableOn) {

        // If DIDIM2 was already enabled, check if the rect has changed
        if (r_equal(&didim->window.rect, &validated_rect)) {
            return;
        }
    }

    // Update DIDIM2 window rect
    snprintf(buf, sizeof(buf), "%i %i %i %i",
             validated_rect.left, validated_rect.top,
             r_width(&validated_rect), r_height(&validated_rect));

    status = sysfs_write_string(PATH_SMARTDIMMER_WINDOW, buf);
    if (status) {
        ALOGE("Failed to set DIDIM2 sd_window, disabling DIDIM2\n");
        status = sysfs_write_int(PATH_SMARTDIMMER_WINDOW_ENABLE, 0);
        if (status) {
            ALOGE("Failed to disable DIDIM2\n");
        } else {
            didim->window.enable = didimEnableOff;
        }
        return;
    }
    didim->window.rect = validated_rect;

    // Enable DIDIM2
    status = sysfs_write_int(PATH_SMARTDIMMER_WINDOW_ENABLE, 1);
    if (status) {
        ALOGE("Failed to enable DIDIM2");
        return;
    }
    didim->window.enable = didimEnableOn;

    didim_level(didim, didim->level.video);

    ALOGD("DIDIM sd_window was set to (%s)\n", buf);
}

struct didim_client *didim_open(const hwc_props_t *hwc_props)
{
    struct didim_state *didim;
    int version = didim_get_version(hwc_props);

    if (version == didimVersionNone) {
        return NULL;
    }

    didim = (struct didim_state *) malloc(sizeof(struct didim_state));
    if (!didim) {
        return NULL;
    }

    memset(didim, 0, sizeof(struct didim_state));

    didim->version = version;

    /* Invalidate cached state */
    didim->level.current = -1;

    /* Get aggressiveness level for normal content */
    didim->level.normal = didim_clamp_level(hwc_props->fixed.didim_normal);

    /* Get aggressiveness level for video content */
    didim->level.video = didim_clamp_level(hwc_props->fixed.didim_video);

    switch (didim->version) {
    case didimVersion1:
        didim->client.set_window = didim_window_level;
        break;

    case didimVersion2:
        didim->client.set_window = didim_window;
        didim_level(didim, didim->level.normal);
        break;
    default:
        NV_ASSERT(0);
        break;
    }

    /* Make sure the client is first so we can cast between the types */
    NV_ASSERT((struct didim_client *) didim == &didim->client);

    return (struct didim_client *) didim;
}

void didim_close(struct didim_client *client)
{
    if (client) {
        struct didim_state *didim = (struct didim_state *) client;

        switch (didim->version) {
        case didimVersion2:
            didim_window(client, NULL);
            break;
        default:
            break;
        }

        /* Disable smartdimmer */
        sysfs_write_int(PATH_SMARTDIMMER_ENABLE, 0);

        free(didim);
    }
}
