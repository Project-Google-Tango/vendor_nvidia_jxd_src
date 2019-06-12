#
# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#
LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvddk_snor

LOCAL_NVIDIA_NO_COVERAGE := true
LOCAL_CFLAGS += -DNV_IS_AVP=0
LOCAL_CFLAGS += -DTARGET_SOC_$(TARGET_TEGRA_VERSION)

LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc/$(TARGET_TEGRA_FAMILY)

LOCAL_SRC_FILES += nvddk_snor.c
LOCAL_SRC_FILES += nvddk_snor_block_driver.c
LOCAL_SRC_FILES += snor_priv_driver.c
LOCAL_SRC_FILES += amd_commandset.c
LOCAL_SRC_FILES += intel_commandset.c

include $(NVIDIA_STATIC_LIBRARY)

