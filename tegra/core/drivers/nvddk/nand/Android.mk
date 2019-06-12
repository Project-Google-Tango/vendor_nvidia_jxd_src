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

LOCAL_MODULE := libnvddk_nand

LOCAL_NVIDIA_NO_COVERAGE := true
LOCAL_CFLAGS += -DNV_IS_AVP=0
# Bootloader doesn't support FORTIFY_SOURCE
LOCAL_CFLAGS += -U_FORTIFY_SOURCE

LOCAL_SRC_FILES += nvddk_nand.c
LOCAL_SRC_FILES += nvddk_nand_block_driver.c
LOCAL_SRC_FILES += bbmwl/nvnandregion.c
LOCAL_SRC_FILES += bbmwl/extftl/nvnandextftl.c
LOCAL_SRC_FILES += bbmwl/ftllite/nvnandftllite.c
LOCAL_SRC_FILES += bbmwl/ftlfull/nvnandftlfull.c
LOCAL_SRC_FILES += bbmwl/ftlfull/nand_ttable.c
LOCAL_SRC_FILES += bbmwl/ftlfull/nand_tat.c
LOCAL_SRC_FILES += bbmwl/ftlfull/nandsectorcache.c
LOCAL_SRC_FILES += bbmwl/ftlfull/nand_strategy.c
LOCAL_SRC_FILES += bbmwl/ftlfull/nvnandtlfull.c

LOCAL_CFLAGS += -Wno-error=sign-compare
LOCAL_CFLAGS += -Wno-error=enum-compare
LOCAL_CFLAGS += -Wno-missing-field-initializers

include $(NVIDIA_STATIC_LIBRARY)
