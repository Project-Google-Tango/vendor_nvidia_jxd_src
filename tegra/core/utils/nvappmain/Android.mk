LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libnvappmain
LOCAL_SRC_FILES += nvappmain_standard.c

include $(NVIDIA_STATIC_LIBRARY)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvappmain_aos
LOCAL_NVIDIA_NO_COVERAGE := true
LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc
LOCAL_SRC_FILES += nvappmain_aos.c
LOCAL_SRC_FILES += nvappmain_jtag.c

include $(NVIDIA_STATIC_LIBRARY)

include $(NVIDIA_DEFAULTS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libnvappmain
LOCAL_SRC_FILES += nvappmain_standard.c

include $(NVIDIA_HOST_STATIC_LIBRARY)
