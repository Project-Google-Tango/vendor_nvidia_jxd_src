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

LOCAL_MODULE := libnvodm_audiocodec

LOCAL_SRC_FILES += audiocodec_hal.c
LOCAL_SRC_FILES += nvodm_codec_wolfson8753.c
LOCAL_SRC_FILES += nvodm_codec_wolfson8903.c
LOCAL_SRC_FILES += nvodm_codec_wolfson8961.c
LOCAL_SRC_FILES += nvodm_codec_wolfson8994.c
LOCAL_SRC_FILES += nvodm_codec_maxim9888.c
LOCAL_SRC_FILES += nvodm_codec_null.c

#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
LOCAL_CFLAGS += -Wno-missing-field-initializers
include $(NVIDIA_STATIC_LIBRARY)

