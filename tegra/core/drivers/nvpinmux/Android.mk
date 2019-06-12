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

LOCAL_COMMON_SRC_FILES := pinmux.c
LOCAL_COMMON_SRC_FILES += pinmux_soc.c

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvpinmux

LOCAL_C_INCLUDES += $(LOCAL_COMMON_C_INCLUDES)

LOCAL_SRC_FILES := $(LOCAL_COMMON_SRC_FILES)

include $(NVIDIA_STATIC_LIBRARY)


include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvpinmux_avp
# For some reason it needs to be arm, as it is called
# very early from the bootloader.
LOCAL_ARM_MODE := arm

LOCAL_C_INCLUDES += $(LOCAL_COMMON_C_INCLUDES)

LOCAL_SRC_FILES := $(LOCAL_COMMON_SRC_FILES)

LOCAL_CFLAGS += -Os -ggdb0

include $(NVIDIA_STATIC_AVP_LIBRARY)

