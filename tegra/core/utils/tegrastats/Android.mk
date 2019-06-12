LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := tegrastats

LOCAL_SRC_FILES += main.c
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libnvrm_graphics

LOCAL_CFLAGS += -DTEGRASTATS_ATRACE=1

# Due to bug in bionic lib, https://review.source.android.com/#change,21736
LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
include $(NVIDIA_EXECUTABLE)
