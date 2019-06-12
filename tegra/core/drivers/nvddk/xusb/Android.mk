# Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

ifneq ($(filter  t114,$(TARGET_TEGRA_VERSION)),)

LOCAL_MODULE := libnvddk_xusb

LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc/$(TARGET_TEGRA_FAMILY)/xusb
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot/$(TARGET_TEGRA_VERSION)/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvrm/nvrmkernel/core/common

LOCAL_CFLAGS += -DNVBOOT_TARGET_RTL=0
LOCAL_CFLAGS += -DNVBOOT_TARGET_FPGA=0

LOCAL_SRC_FILES += nvddk_xusb_block_driver.c
LOCAL_SRC_FILES += nvddk_xusbh.c
LOCAL_SRC_FILES += nvddk_xusb_arc.c
LOCAL_SRC_FILES += nvddk_xusb_msc.c

include $(NVIDIA_STATIC_LIBRARY)

endif
