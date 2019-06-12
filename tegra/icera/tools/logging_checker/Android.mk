# logging_checker/Android.mk
#
# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#

# Android makefile for Icera logging checker.

LOCAL_PATH := $(call my-dir)

# android arm variant
include $(NVIDIA_DEFAULTS)
LOCAL_MODULE_TAGS := nvidia_tests
LOCAL_MODULE:= logging_checker
LOCAL_SRC_FILES := main.cpp logging_checker.cpp pattern_checker.cpp socket.cpp iom_data.cpp getopt.cpp
LOCAL_SHARED_LIBRARIES := libcutils
include $(NVIDIA_EXECUTABLE)

# linux host variant
include $(NVIDIA_DEFAULTS)
LOCAL_MODULE_TAGS := nvidia_tests
LOCAL_MODULE:= logging_checker
LOCAL_SRC_FILES := main.cpp logging_checker.cpp pattern_checker.cpp socket.cpp iom_data.cpp getopt.cpp
LOCAL_CFLAGS += -DANDROID_HOST_BUILD
include $(NVIDIA_HOST_EXECUTABLE)

