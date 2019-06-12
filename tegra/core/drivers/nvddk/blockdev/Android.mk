# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvddk_blockdevmgr

LOCAL_NVIDIA_NO_COVERAGE := true
LOCAL_CFLAGS += -DNV_IS_AVP=0
ifneq ($(filter  t30 t114,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DBSEAV_USED
endif

ifneq ($(filter  t114,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DXUSB_EXISTS=1
else
LOCAL_CFLAGS += -DXUSB_EXISTS=0
endif

ifneq ($(filter  t124,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DSATA_EXISTS=1
LOCAL_CFLAGS += -DUSBH_EXISTS=1
else
LOCAL_CFLAGS += -DSATA_EXISTS=0
LOCAL_CFLAGS += -DUSBH_EXISTS=0
endif

ifeq ($(BOOT_MINIMAL_BL),1)
LOCAL_CFLAGS += -DBOOT_MINIMAL_BL
endif

LOCAL_SRC_FILES += nvddk_blockdevmgr.c

include $(NVIDIA_STATIC_LIBRARY)

