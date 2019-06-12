# test-mode/Android.mk
#
# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#

# Android makefile for test-mode helper scripts for tethering.

LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := nwifup
LOCAL_SRC_FILES := nwifup.sh
LOCAL_MODULE_CLASS := EXECUTABLES

include $(NVIDIA_PREBUILT)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := nwifdown
LOCAL_SRC_FILES := nwifdown.sh
LOCAL_MODULE_CLASS := EXECUTABLES

include $(NVIDIA_PREBUILT)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE :=  at-port-forward
LOCAL_SRC_FILES :=  at-port-forward.sh
LOCAL_MODULE_CLASS := EXECUTABLES

include $(NVIDIA_PREBUILT)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE :=  log-port-forward
LOCAL_SRC_FILES :=  log-port-forward.sh
LOCAL_MODULE_CLASS := EXECUTABLES

include $(NVIDIA_PREBUILT)
