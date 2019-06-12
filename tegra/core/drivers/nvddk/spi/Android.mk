#
# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvddk_spi

LOCAL_ARM_MODE := arm
LOCAL_CFLAGS += -Os -ggdb0
ifeq ($(AVP_EXTERNAL_TOOLCHAIN),)
  LOCAL_C_INCLUDES += $(TOP)/bionic/libc/include
  LOCAL_CFLAGS += -D_MALLOC_H_
endif

LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc/$(TARGET_TEGRA_FAMILY)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvddk/spi
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvddk/spi/common
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvddk/spi/$(TARGET_TEGRA_FAMILY)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/include


ifneq ($(filter t11x t14x t12x,$(TARGET_TEGRA_FAMILY)),)
LOCAL_SRC_FILES := nvddk_spi.c
LOCAL_SRC_FILES += $(TARGET_TEGRA_FAMILY)/nvddk_spi_clk_rst.c
LOCAL_SRC_FILES += common/nvddk_spi_hw.c
else
LOCAL_SRC_FILES := nvddk_spi_stub.c
endif

include $(NVIDIA_STATIC_LIBRARY)
