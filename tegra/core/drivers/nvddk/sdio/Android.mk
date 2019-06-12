#
# Copyright (c) 2012 - 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#
LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvddk_sdio

LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc

LOCAL_NVIDIA_NO_COVERAGE := true
LOCAL_CFLAGS += -DNV_IS_AVP=0

ifeq ($(TARGET_TEGRA_VERSION),t30)
LOCAL_CFLAGS += -DNVDDK_SDMMC_T30=1
else
LOCAL_CFLAGS += -DNVDDK_SDMMC_T30=0
endif
ifeq ($(TARGET_TEGRA_VERSION),t148)
LOCAL_CFLAGS += -DNV_IF_T148=1
else
LOCAL_CFLAGS += -DNV_IF_T148=0
endif
ifeq ($(TARGET_TEGRA_VERSION),t114)
LOCAL_CFLAGS += -DNV_IF_T114=1
else
LOCAL_CFLAGS += -DNV_IF_T114=0
endif

ifeq ($(TARGET_TEGRA_VERSION),t124)
LOCAL_CFLAGS += -DNVDDK_SDMMC_T124=1
else
LOCAL_CFLAGS += -DNVDDK_SDMMC_T124=0
endif

ifeq ($(BOOT_MINIMAL_BL),1)
LOCAL_CFLAGS += -DBOOT_MINIMAL_BL
endif

LOCAL_SRC_FILES += nvddk_sd_block_driver.c
LOCAL_SRC_FILES += nvddk_sdio.c
LOCAL_SRC_FILES += t30_sdio.c
LOCAL_SRC_FILES += t1xx_sdio.c

include $(NVIDIA_STATIC_LIBRARY)

