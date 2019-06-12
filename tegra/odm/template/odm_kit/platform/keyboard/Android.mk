LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvodm_keyboard
LOCAL_NVIDIA_NO_COVERAGE := true
LOCAL_MODULE_TAGS := nvidia_tests

LOCAL_SRC_FILES += nvodm_keyboard_stub.c

#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
LOCAL_CFLAGS += -Wno-missing-field-initializers
include $(NVIDIA_STATIC_LIBRARY)
