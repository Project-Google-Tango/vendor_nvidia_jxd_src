#ifndef __NVDC_PRIV_H__
#define __NVDC_PRIV_H__

#include <nvdc.h>

#define NVDC_MAX_HEADS      2
#define NVDC_MAX_WINDOWS    3
#define NVDC_MAX_OUTPUTS    5

static inline int NVDC_VALID_HEAD(int head) {
    return head >= 0 && head < NVDC_MAX_HEADS;
}

struct nvdcDisplay {
    int dcHandle;
    enum nvdcDisplayType type;
};

struct nvdcState {
    int tegraExtFd[NVDC_MAX_HEADS];
    int tegraCtrlFd;
    int fbFd[NVDC_MAX_HEADS];
    int nvmapUserFd;

    struct nvdcDisplay displays[NVDC_MAX_OUTPUTS];
    unsigned int numOutputs;

    int eventMask;
    nvdcEventHotplugHandler hotplug;

    unsigned int caps;
};

int _nvdcInitOutputInfo(struct nvdcState *);

int _nvdcInitCaps(struct nvdcState *);

#endif /* __NVDC_PRIV_H__ */
