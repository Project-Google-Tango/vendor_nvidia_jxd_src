LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvfxmath

LOCAL_SRC_FILES += sincos.c
LOCAL_SRC_FILES += fxconv_arm.S
LOCAL_SRC_FILES += fxdiv_arm.S
LOCAL_SRC_FILES += fxrsqrt_arm.S
LOCAL_SRC_FILES += fxexplog_arm.S

include $(NVIDIA_STATIC_LIBRARY)

