LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnvtestrun
LOCAL_SRC_FILES += nvrun.c
include $(NVIDIA_STATIC_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnvtestmain
LOCAL_SRC_FILES += nvmain.c
include $(NVIDIA_STATIC_LIBRARY)
