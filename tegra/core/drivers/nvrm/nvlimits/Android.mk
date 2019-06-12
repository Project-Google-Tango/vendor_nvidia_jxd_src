# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvrm_limits
LOCAL_NVIDIA_NO_COVERAGE := true

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../nvrmkernel/core
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../nvrmkernel/core/common
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../nvrmkernel/core/ap15

LOCAL_SRC_FILES += $(TARGET_TEGRA_FAMILY)rm_clocks_limits.c

LOCAL_CFLAGS += -Wno-missing-field-initializers
include $(NVIDIA_STATIC_LIBRARY)
