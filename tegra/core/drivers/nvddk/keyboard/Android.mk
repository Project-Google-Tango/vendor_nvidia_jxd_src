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

LOCAL_MODULE := libnvddk_keyboard

LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc/$(TARGET_TEGRA_FAMILY)

LOCAL_NVIDIA_NO_COVERAGE := true

ifneq ($(filter t11x,$(TARGET_TEGRA_FAMILY)),)
LOCAL_CFLAGS += -DTEGRA_11x_SOC
endif
ifneq ($(filter t30,$(TARGET_TEGRA_FAMILY)),)
LOCAL_CFLAGS += -DTEGRA_3x_SOC
endif
ifneq ($(filter t12x,$(TARGET_TEGRA_FAMILY)),)
LOCAL_CFLAGS += -DTEGRA_12x_SOC
endif

LOCAL_SRC_FILES += nvddk_kbc.c
LOCAL_SRC_FILES += nvddk_gpio_keys.c
LOCAL_SRC_FILES += nvddk_keyboard.c

include $(NVIDIA_STATIC_LIBRARY)

