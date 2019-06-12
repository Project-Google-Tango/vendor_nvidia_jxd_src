# Copyright (c) 2013 NVIDIA Corporation.
#
# This is the appli used to get all AP logs after a modem crash is detected.
# (e.g modem logs are part of full coredump handled by BB)
LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= icera-crashlogs
LOCAL_SRC_FILES += \
    icera-crashlogs.c

LOCAL_SHARED_LIBRARIES := \
    libcutils

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../../icera-util

include $(NVIDIA_EXECUTABLE)

include $(NVIDIA_DEFAULTS)

