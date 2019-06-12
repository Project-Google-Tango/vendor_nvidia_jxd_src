LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvextfsmgr
LOCAL_NVIDIA_NO_COVERAGE := true
LOCAL_SRC_FILES += nvextfsmgr.c

include $(NVIDIA_STATIC_LIBRARY)

