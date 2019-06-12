LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)
LOCAL_NVIDIA_NO_COVERAGE := true

LOCAL_MODULE := libnvodm_misc

LOCAL_SRC_FILES += misc_hal.c
LOCAL_SRC_FILES += nvodm_kbc.c

LOCAL_SHARED_LIBRARIES += libnvrm
LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_SHARED_LIBRARIES += libnvodm_query
LOCAL_STATIC_LIBRARIES += libnvodm_services

LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1
include $(NVIDIA_STATIC_AND_SHARED_LIBRARY)
