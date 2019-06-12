LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_CFLAGS += -DNVMMLITE_BUILT_DYNAMIC=1

LOCAL_MODULE := libnvmmlite

LOCAL_SRC_FILES += nvmmlite.c

LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_SHARED_LIBRARIES += libnvmmlite_utils
LOCAL_SHARED_LIBRARIES += libnvmm_utils

include $(NVIDIA_SHARED_LIBRARY)
-include $(addprefix $(LOCAL_PATH)/, $(addsuffix /Android.mk, utils))

