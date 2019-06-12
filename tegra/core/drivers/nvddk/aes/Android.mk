# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvddk_aes

ifneq ($(filter t30,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DSTORE_SSK_IN_PMC=1
else
LOCAL_CFLAGS += -DSTORE_SSK_IN_PMC=0
endif

# Wait for AHB arbitration controller to be done with the job to make sure
# that AES writes to memory are complete when AHB interface is used instead of MCIF
ifneq ($(filter  t30 t114,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DWAIT_ON_AHB_ARBC=1
else
LOCAL_CFLAGS += -DWAIT_ON_AHB_ARBC=0
endif

LOCAL_SRC_FILES += nvddk_aes_blockdev.c
LOCAL_SRC_FILES += nvddk_aes_core.c
LOCAL_SRC_FILES += nvddk_aes_hw.c
LOCAL_SRC_FILES += nvddk_aes_hw_utils.c

# TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
include $(NVIDIA_STATIC_LIBRARY)
