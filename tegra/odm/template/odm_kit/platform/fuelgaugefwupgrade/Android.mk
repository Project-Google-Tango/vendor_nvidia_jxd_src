# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvodm_fuelgaugefwupgrade
LOCAL_NVIDIA_NO_COVERAGE := true

LOCAL_SRC_FILES += nvodm_fuelgaugefwupgrade.c

include $(NVIDIA_STATIC_LIBRARY)
