LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvopenmax_samplebase
LOCAL_MODULE_TAGS := nvidia_tests

LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/include/openmax/il

LOCAL_CFLAGS += -DOMXVERSION=2

LOCAL_CFLAGS += -Wno-error=switch

LOCAL_SRC_FILES += samplebase.c

LOCAL_CFLAGS += -Wno-error=sign-compare
include $(NVIDIA_STATIC_LIBRARY)
