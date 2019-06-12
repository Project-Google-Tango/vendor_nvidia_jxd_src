LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvodm_hdmi

LOCAL_SRC_FILES += nvodm_hdmi.c
LOCAL_STATIC_LIBRARIES += libnvodm_services
LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_SHARED_LIBRARIES += libnvodm_query
LOCAL_SHARED_LIBRARIES += libnvrm

LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
LOCAL_CFLAGS += -Wno-missing-field-initializers
LOCAL_CFLAGS += -Wno-missing-field-initializers
include $(NVIDIA_STATIC_AND_SHARED_LIBRARY)
