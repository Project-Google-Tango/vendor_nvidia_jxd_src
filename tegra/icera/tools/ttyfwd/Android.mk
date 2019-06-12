# ttyfwd/Android.mk
#
# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#

# Android makefile for ttyfwd (TTY port forward).

LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= ttyfwd
LOCAL_SRC_FILES += \
       ttyfwd.c

LOCAL_SHARED_LIBRARIES := \
	libcutils

include $(NVIDIA_EXECUTABLE)

