#
# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvbootupdate
LOCAL_NVIDIA_NO_COVERAGE := true
LOCAL_SRC_FILES += nvbu_private_$(TARGET_TEGRA_FAMILY).c
LOCAL_SRC_FILES += nvbu.c

LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
include $(NVIDIA_STATIC_LIBRARY)
include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnvbootupdatehost
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)
LOCAL_SRC_FILES += nvbu_private_$(TARGET_TEGRA_FAMILY).c
LOCAL_SRC_FILES += nvbu.c

include $(NVIDIA_HOST_STATIC_LIBRARY)
