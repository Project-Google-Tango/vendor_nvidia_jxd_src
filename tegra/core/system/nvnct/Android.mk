#
# Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvnct

LOCAL_NVIDIA_NO_COVERAGE := true

LOCAL_CFLAGS += -DNV_USE_NCT

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/fastboot
LOCAL_SRC_FILES += nvnct.c
LOCAL_SRC_FILES += nvnct_api.c
LOCAL_SRC_FILES += nvnct_utils.c
LOCAL_SRC_FILES += nvnct_items.c

LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
include $(NVIDIA_STATIC_LIBRARY)
