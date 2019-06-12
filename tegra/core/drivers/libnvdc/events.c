#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include <linux/tegra_dc_ext.h>

#include <nvdc.h>

#include "nvdc_priv.h"

int nvdcEventFds(struct nvdcState *state, int **pFds, int *numFds)
{
    int num = 1;
    int *fds;

    fds = malloc(sizeof(int) * num);
    fds[0] = state->tegraCtrlFd;

    *pFds = fds;
    *numFds = num;

    return 0;
}

static int handleHotplug(struct nvdcState *state, void *data)
{
    struct tegra_dc_ext_control_event_hotplug *hpData = data;

    if (!state->hotplug) {
        return 0;
    }

    if (hpData->handle >= NVDC_MAX_OUTPUTS) {
        return EINVAL;
    }

    state->hotplug(&state->displays[hpData->handle]);

    return 0;
}

int nvdcEventData(struct nvdcState *state, int fd)
{
    char buf[1024];
    struct tegra_dc_ext_event *event = (void *)buf;
    ssize_t bytes;

    do {
        bytes = read(fd, &buf, sizeof(buf));
    } while (bytes < 0 && errno == EINTR);

    if (bytes < 0) {
        return errno;
    }

    if (bytes < (ssize_t)sizeof(event)) {
        /* XXX arguably should support partial writes, but the kernel doesn't
         * actually do that so punt for now */
        return EINVAL;
    }

    switch (event->type) {
    case TEGRA_DC_EXT_EVENT_HOTPLUG:
        return handleHotplug(state, event->data);
    default:
        return EINVAL;
    }

    return 0;
}

static int setEventMask(struct nvdcState *state, int eventMask)
{
    if (ioctl(state->tegraCtrlFd, TEGRA_DC_EXT_CONTROL_SET_EVENT_MASK,
              eventMask)) {
        return errno;
    }

    state->eventMask = eventMask;

    return 0;
}

int nvdcEventHotplug(struct nvdcState *state, nvdcEventHotplugHandler handler)
{
    nvdcEventHotplugHandler oldHandler = state->hotplug;
    int newEventMask, ret = 0;

    if (handler) {
        newEventMask = state->eventMask | TEGRA_DC_EXT_EVENT_HOTPLUG;
    } else {
        newEventMask = state->eventMask & ~TEGRA_DC_EXT_EVENT_HOTPLUG;
    }

    if (handler) {
        state->hotplug = handler;
    }

    if (newEventMask != state->eventMask) {
        ret = setEventMask(state, newEventMask);
    }

    if (!ret) {
        if (!handler) {
            state->hotplug = handler;
        }
    } else {
        state->hotplug = oldHandler;
    }

    return ret;
}

int nvdcEventModeChange(struct nvdcState *state, nvdcEventModeChangeHandler handler)
{
    return ENOANO;
}

int nvdcInitAsyncEvents(struct nvdcState *state)
{
    return ENOANO;
}
