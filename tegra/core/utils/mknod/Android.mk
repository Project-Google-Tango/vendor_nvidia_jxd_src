LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := mknod

LOCAL_SRC_FILES += mknod.c

include $(NVIDIA_EXECUTABLE)
