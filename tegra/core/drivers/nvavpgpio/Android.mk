LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvgpio_avp

LOCAL_CFLAGS += -Os -ggdb0

LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc/$(TARGET_TEGRA_FAMILY)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/utils/nvos/aos

LOCAL_SRC_FILES := avp_gpio.c

include $(NVIDIA_STATIC_AVP_LIBRARY)
