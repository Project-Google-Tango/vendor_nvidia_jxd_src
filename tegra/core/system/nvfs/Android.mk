# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvfs
LOCAL_NVIDIA_NO_COVERAGE := true

LOCAL_C_INCLUDES += $(LOCAL_PATH)/basic
ifeq ($(NV_EMBEDDED_BUILD),1)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/enhanced
LOCAL_C_INCLUDES += $(LOCAL_PATH)/ext2
endif

LOCAL_SRC_FILES += basic/nvbasicfilesystem.c
ifeq ($(NV_EMBEDDED_BUILD),1)
LOCAL_SRC_FILES += enhanced/nvenhancedfilesystem.c
LOCAL_SRC_FILES += ext2/nvext2filesystem.c
LOCAL_SRC_FILES += ext2/nvext2operations.c
endif

ifeq ($(NV_EMBEDDED_BUILD),1)
LOCAL_CFLAGS += -DNV_EMBEDDED_BUILD
endif

# Disabling hardware support for floating point calculations
# as it causes increment corruption for NvU64 variables
LOCAL_CFLAGS += -mfloat-abi=soft

LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
include $(NVIDIA_STATIC_LIBRARY)
