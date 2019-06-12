LOCAL_PATH := $(call my-dir)

LOCAL_COMMON_SRC_FILES := tmon_hal.c
LOCAL_COMMON_SRC_FILES += adt7461/nvodm_tmon_adt7461.c

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnvodm_tmon
LOCAL_NVIDIA_NO_COVERAGE := true
LOCAL_SRC_FILES := $(LOCAL_COMMON_SRC_FILES)
#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
LOCAL_CFLAGS += -Wno-missing-field-initializers
include $(NVIDIA_STATIC_LIBRARY)


include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnvodm_tmon_avp
LOCAL_CFLAGS += -Os -ggdb0
LOCAL_SRC_FILES := $(LOCAL_COMMON_SRC_FILES)
include $(NVIDIA_STATIC_AVP_LIBRARY)
