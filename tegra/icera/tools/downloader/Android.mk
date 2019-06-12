# downloader/Android.mk
#
# Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
#

# Android makefile for Icera downloader.

LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= downloader
LOCAL_SRC_FILES += \
       at_cmd.c \
       com_linux.c \
       Updater.c \
       dld_main.c

LOCAL_SHARED_LIBRARIES := \
	libcutils

include $(NVIDIA_EXECUTABLE)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := icera-loader
LOCAL_SRC_FILES := icera-loader
LOCAL_MODULE_CLASS := EXECUTABLES

include $(NVIDIA_PREBUILT)

