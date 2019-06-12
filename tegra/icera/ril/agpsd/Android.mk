# agpsd/Android.mk
#
# Copyright (c) 2012, NVIDIA CORPORATION. All rights reserved.
#

# This is the Icera aGPS daemon

LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= agpsd
LOCAL_SRC_FILES += \
       agps_daemon.c \
       agps_threads.c \
       agps_port.c

LOCAL_SHARED_LIBRARIES := \
	libcutils \
        libdl

include $(NVIDIA_EXECUTABLE)
