LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvodm_gpio_ext

LOCAL_SRC_FILES += gpio_ext_hal.c
LOCAL_SRC_FILES += gpio_ext_null.c
LOCAL_SRC_FILES += gpio_pcf50626.c

#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
LOCAL_CFLAGS += -Wno-missing-field-initializers
include $(NVIDIA_STATIC_LIBRARY)
