#include <errno.h>
#include <sys/ioctl.h>

#include <linux/tegra_dc_ext.h>

#include <nvdc.h>

#include "nvdc_priv.h"


/*
 * Query DC capabilities.  Called once at startup,
 */
int _nvdcInitCaps(struct nvdcState *state)
{
    struct tegra_dc_ext_control_capabilities caps = { };

    state->caps = 0;

    if (ioctl(state->tegraCtrlFd, TEGRA_DC_EXT_CONTROL_GET_CAPABILITIES,
              &caps)) {
        if (errno == EINVAL || errno == ENOTTY) {
            return 0;
        }
        return errno;
    }

    state->caps = caps.caps;

    return 0;
}


int nvdcGetCapabilities(struct nvdcState *state,
                        unsigned int *nvdcCaps)
{
    *nvdcCaps = state->caps;

    return 0;
}

