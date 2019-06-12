#
# Copyright (c) 2011 - 2013 NVIDIA Corporation.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#
LOCAL_PATH := $(call my-dir)

_nvml_local_path := $(call my-dir)

#t30
include $(_nvml_local_path)/Android.t30.mk

#t114
include $(_nvml_local_path)/Android.t11x.mk

#t124
include $(_nvml_local_path)/Android.t12x.mk

#nvtboot
ifeq ($(filter t124,$(TARGET_TEGRA_VERSION)),)
NVTBOOT_TEGRA_VERSION := t124
include $(_nvml_local_path)/../../../../bootloader/nvtboot/Android.mk
endif

_nvml_local_path :=
