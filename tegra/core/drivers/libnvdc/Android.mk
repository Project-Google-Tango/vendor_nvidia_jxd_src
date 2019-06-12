#
# Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvdc

LOCAL_CFLAGS += -Wno-missing-field-initializers

LOCAL_SRC_FILES += \
	caps.c \
	cmu.c \
	crc.c \
	csc.c \
	cursor.c \
	displays.c \
	events.c \
	flip.c \
	lut.c \
	modes.c \
	openclose.c \
	util.c \

LOCAL_SHARED_LIBRARIES += libnvos

include $(NVIDIA_SHARED_LIBRARY)
