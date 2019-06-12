# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvfsmgr
LOCAL_NVIDIA_NO_COVERAGE := true
LOCAL_SRC_FILES += nvfsmgr.c

ifeq ($(NV_EMBEDDED_BUILD),1)
LOCAL_CFLAGS += -DNV_EMBEDDED_BUILD
endif

LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
include $(NVIDIA_STATIC_LIBRARY)
