LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)
ifneq ($(filter  t148,$(TARGET_TEGRA_VERSION)),)
LOCAL_MODULE := libnvddk_mipibif

LOCAL_CFLAGS += -Os -ggdb0

LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc/$(TARGET_TEGRA_FAMILY)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/utils/nvos/aos

LOCAL_SRC_FILES := nvddk_mipibif.c

include $(NVIDIA_STATIC_AVP_LIBRARY)
endif
