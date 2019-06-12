ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/nvmm/nvmm/include

LOCAL_MODULE := libnvparserhalutils

LOCAL_SRC_FILES += nvparserhal_content_pipe.cpp

include $(NVIDIA_STATIC_LIBRARY)
endif

