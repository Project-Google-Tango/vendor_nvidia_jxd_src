LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvbasewriter

LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/nvmm/nvmm/include

ifeq ($(NV_LOGGER_ENABLED),1)
LOCAL_CFLAGS += -DNV_LOGGER_ENABLED=1
endif
LOCAL_SRC_FILES += nvmm_basewriterblock.c

include $(NVIDIA_STATIC_LIBRARY)

