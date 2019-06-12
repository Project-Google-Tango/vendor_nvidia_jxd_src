# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvintr
LOCAL_NVIDIA_NO_COVERAGE := true
LOCAL_ARM_MODE := arm

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvrm/nvrmkernel/core/common

ifneq ($(filter t30 t148,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DGIC_VERSION=1
else
LOCAL_CFLAGS += -DGIC_VERSION=2
endif

LOCAL_SRC_FILES += nvintrhandler.c
LOCAL_SRC_FILES += nvintrhandler_common.c
LOCAL_SRC_FILES += nvintrhandler_gic.c
LOCAL_SRC_FILES += nvintrhandler_gpio.c
LOCAL_SRC_FILES += nvintrhandler_gpio_t30.c

include $(NVIDIA_STATIC_LIBRARY)
