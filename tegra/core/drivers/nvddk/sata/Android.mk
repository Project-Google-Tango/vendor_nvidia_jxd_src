#
# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvddk_sata

LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc

LOCAL_NVIDIA_NO_COVERAGE := true

LOCAL_SRC_FILES += nvddk_sata_block_driver.c
LOCAL_SRC_FILES += nvddk_sata.c

ifeq ($(TARGET_USE_NVDUMPER),true)
LOCAL_CFLAGS += -DENABLE_NVDUMPER=1
else
LOCAL_CFLAGS += -DENABLE_NVDUMPER=0
endif
include $(NVIDIA_STATIC_LIBRARY)

