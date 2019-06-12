LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_SRC_FILES := nvgr.c
LOCAL_MODULE := libnvgr

LOCAL_SHARED_LIBRARIES := \
	libhardware \
	libcutils \
	libsync

LOCAL_CFLAGS += -DLOG_TAG=\"nvgrapi\"

LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics-partner/android/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics-partner/android/gralloc
LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics/2d/include

include $(NVIDIA_SHARED_LIBRARY)
