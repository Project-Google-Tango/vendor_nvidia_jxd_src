LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvdioconverter

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvflash/app

LOCAL_SRC_FILES += nvdioconverter.c
LOCAL_SRC_FILES += nvstorebinconverter.c

include $(NVIDIA_HOST_STATIC_LIBRARY)
