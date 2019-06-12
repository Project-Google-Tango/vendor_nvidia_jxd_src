LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvusbhost
LOCAL_SRC_FILES += $(HOST_OS)/nvusbhost_$(HOST_OS).c

ifeq ($(HOST_OS),darwin)
LOCAL_SRC_FILES += $(HOST_OS)/usb_osx.c
endif

include $(NVIDIA_HOST_STATIC_LIBRARY)
