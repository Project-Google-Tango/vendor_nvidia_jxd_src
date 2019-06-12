LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)
include $(LOCAL_PATH)/../Android.common.mk

LOCAL_MODULE := libnvmmtransport

LOCAL_SRC_FILES += nvmm_transport_block.c
LOCAL_SRC_FILES += nvmm_transport_client.c

#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1
include $(NVIDIA_STATIC_LIBRARY)

