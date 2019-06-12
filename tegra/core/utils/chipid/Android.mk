LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvchip

LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc/$(TARGET_TEGRA_FAMILY)
LOCAL_SRC_FILES += nvchip_utility.c

include $(NVIDIA_STATIC_LIBRARY)

include $(NVIDIA_DEFAULTS)
