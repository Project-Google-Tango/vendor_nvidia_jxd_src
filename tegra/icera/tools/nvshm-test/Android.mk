LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

# Please fix the warnings and remove this line ASAP
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= nvshmtest
LOCAL_SRC_FILES += \
       nvshmtest.c
include $(NVIDIA_EXECUTABLE)
