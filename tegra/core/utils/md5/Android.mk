LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libmd5

LOCAL_SRC_FILES += md5.c

include $(NVIDIA_STATIC_LIBRARY)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libmd5_host_static

LOCAL_SRC_FILES += md5.c
include $(NVIDIA_HOST_STATIC_LIBRARY)
