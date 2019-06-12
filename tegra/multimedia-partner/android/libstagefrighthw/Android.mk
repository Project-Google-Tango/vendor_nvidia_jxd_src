LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_SRC_FILES :=
LOCAL_SRC_FILES += omxplugin.cpp

ifeq ($(BUILD_GOOGLETV),true)
LOCAL_CFLAGS += -DBUILD_GOOGLETV
LOCAL_C_INCLUDES += vendor/tv/frameworks/base/include
endif

LOCAL_C_INCLUDES += frameworks/native/include/media/hardware
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/include/openmax/il
LOCAL_SHARED_LIBRARIES := \
        libutils \
        libdl \
        liblog

LOCAL_MODULE := libstagefrighthw

LOCAL_PRELINK_MODULE := false

#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
include $(NVIDIA_SHARED_LIBRARY)

include $(CLEAR_VARS)

