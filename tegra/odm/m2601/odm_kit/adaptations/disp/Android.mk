#
# Copyright (c) 2012 NVIDIA CORPORATION.  All Rights Reserved.
#
LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)
LOCAL_NVIDIA_NO_COVERAGE := true

LOCAL_MODULE := libnvodm_disp

LOCAL_SRC_FILES += dsi_to_lvds_converter.c
LOCAL_SRC_FILES += panels.c
LOCAL_SRC_FILES += panel_lcd_wvga.c


#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1
include $(NVIDIA_STATIC_LIBRARY)
