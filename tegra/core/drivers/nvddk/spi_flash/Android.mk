#
# Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software and related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvddk_spif

LOCAL_NVIDIA_NO_COVERAGE := true
LOCAL_CFLAGS += -DNV_IS_AVP=0

ifneq ($(TARGET_TEGRA_VERSION),t30)
LOCAL_CFLAGS += -DNV_USE_DDK_SPI
endif

LOCAL_SRC_FILES += nvddk_spif_block_driver.c

include $(NVIDIA_STATIC_LIBRARY)
