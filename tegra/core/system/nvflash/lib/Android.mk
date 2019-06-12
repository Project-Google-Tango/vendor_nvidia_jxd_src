# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvflash

LOCAL_CFLAGS += -DNVODM_BOARD_IS_FPGA=0
ifeq ($(NV_EMBEDDED_BUILD),1)
LOCAL_CFLAGS += -DNV_EMBEDDED_BUILD
endif

ifeq ($(TARGET_USE_NVDUMPER),true)
LOCAL_CFLAGS += -DENABLE_NVDUMPER=1
else
LOCAL_CFLAGS += -DENABLE_NVDUMPER=0
endif

LOCAL_SRC_FILES += nvflash_commands.c
LOCAL_SRC_FILES += nvflash_configfile.c
LOCAL_SRC_FILES += nvflash_configfile.tab.c
LOCAL_SRC_FILES += nvflash_verifylist.c
LOCAL_SRC_FILES += lex.yy.c
LOCAL_SRC_FILES += nvflash_parser.c

LOCAL_STATIC_LIBRARIES += libnvapputil

include $(NVIDIA_HOST_STATIC_LIBRARY)
