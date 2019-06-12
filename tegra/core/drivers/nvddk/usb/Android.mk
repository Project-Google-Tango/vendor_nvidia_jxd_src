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

LOCAL_MODULE := libnvddk_usbphy

LOCAL_NVIDIA_NO_COVERAGE := true
LOCAL_CFLAGS += -DNV_IS_AVP=0
LOCAL_CFLAGS += -Wno-error=cast-align
LOCAL_CFLAGS += -Wno-error=enum-compare

LOCAL_SRC_FILES += nvddk_usbphy.c
LOCAL_SRC_FILES += host/usbhblockdriver/nvddk_usbh_block_driver.c
LOCAL_SRC_FILES += host/usbhblockdriver/nvddk_usbh_scsi_driver.c
LOCAL_SRC_FILES += nvddk_usbphy_t124.c
LOCAL_SRC_FILES += host/nvddk_usbh.c
LOCAL_SRC_FILES += host/nvddk_usbh_priv.c
LOCAL_SRC_FILES += host/nvddk_usbh_priv_t124.c

include $(NVIDIA_STATIC_LIBRARY)

