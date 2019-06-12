#
# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#

LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvavpsdmmc

LOCAL_NO_DEFAULT_COMPILER_FLAGS := true

LOCAL_CFLAGS += -Os
LOCAL_SRC_FILES := nvbl_sdmmc.c
LOCAL_SRC_FILES += nvbl_sdmmc_soc.c

ifneq ($(filter t12x,$(TARGET_TEGRA_FAMILY)),)
LOCAL_CFLAGS += -DT124_NVAVP_SDMMC
LOCAL_CFLAGS += -DTEGRA_12x_SOC
endif

ifeq ($(TARGET_TEGRA_FAMILY), t12x)
LOCAL_CFLAGS += -DT124_NVAVP_SDMMC
endif

ifneq ($(filter t30 t114, $(TARGET_SOC)),)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot/$(TARGET_TEGRA_VERSION)/include
else
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot/t114/include
endif
LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc/$(TARGET_TEGRA_VERSION)

include $(NVIDIA_STATIC_AVP_LIBRARY)
