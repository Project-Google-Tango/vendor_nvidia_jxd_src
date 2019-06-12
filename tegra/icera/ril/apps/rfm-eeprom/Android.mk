# rfm-eeprom/Android.mk
#
# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#

# This is the RFM module eeprom util

LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= icera-rfm-eeprom
LOCAL_SRC_FILES += \
       drv_eeprom_nvram.c \
       drv_i2c.c \
       drv_m24m01.c \
       main.c

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../icera-util

LOCAL_SHARED_LIBRARIES := libcutils

include $(NVIDIA_EXECUTABLE)
