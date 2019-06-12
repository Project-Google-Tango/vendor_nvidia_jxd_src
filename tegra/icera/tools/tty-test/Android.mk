LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= ttytestapp
LOCAL_SRC_FILES += \
       ttytestapp.c
include $(NVIDIA_EXECUTABLE)
