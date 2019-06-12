# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvddk_i2c

LOCAL_ARM_MODE := arm

LOCAL_CFLAGS += -Os -ggdb0

LOCAL_SRC_FILES := nvddk_i2c.c
LOCAL_SRC_FILES += nvddk_i2c_hw.c
LOCAL_SRC_FILES += nvddk_i2c_clk.c

include $(NVIDIA_STATIC_AVP_LIBRARY)
