# cpo/Android.mk
#
# Copyright (c) 2012 NVIDIA Corporation. 
#

# This is the Icera usb mode switch daemon

# usb_modeswitch-1.1.9-arm-static is an open source binary 

LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= icera-switcherd
LOCAL_SRC_FILES += \
    icera-switcher.c

LOCAL_SHARED_LIBRARIES := \
    libusbhost\
    libcutils

include $(NVIDIA_EXECUTABLE)

include $(NVIDIA_DEFAULTS)

