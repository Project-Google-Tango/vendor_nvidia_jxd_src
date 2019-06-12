LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvtestresults
LOCAL_SRC_FILES += nvresults.c
LOCAL_SHARED_LIBRARIES += libnvtestio
LOCAL_SHARED_LIBRARIES += libnvos

include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvtestresults
LOCAL_MODULE_TAGS := eng
LOCAL_SRC_FILES += nvresults.c

include $(NVIDIA_HOST_STATIC_LIBRARY)
