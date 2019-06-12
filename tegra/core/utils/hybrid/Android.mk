LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libhybrid

LOCAL_CFLAGS += -DHG_NATIVE_FLOATS
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/utils/hybrid/include

LOCAL_SRC_FILES += hgfloat.c

LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
include $(NVIDIA_STATIC_LIBRARY)

