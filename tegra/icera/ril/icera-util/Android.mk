#***********************************************************************************************
# NVIDIA
# Copyright (c) 2011-2013
# All rights reserved
#***********************************************************************************************
# $Revision: $
# $Date: $
# $Author: $
#***********************************************************************************************

LOCAL_PATH:= $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional

# Build static library
LOCAL_MODULE:= libutil-icera

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/include

LOCAL_SRC_FILES:= \
    icera-util-power.c \
    icera-util-wakelock.c \
    icera-util-crashlogs.c

LOCAL_SHARED_LIBRARIES := \
    libcutils

include $(NVIDIA_STATIC_LIBRARY)

