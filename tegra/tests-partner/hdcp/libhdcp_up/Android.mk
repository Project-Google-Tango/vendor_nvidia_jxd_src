LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libhdcp_up
LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include/

LOCAL_SRC_FILES += sha1.c
LOCAL_SRC_FILES += hdcp_up.c

include $(NVIDIA_STATIC_LIBRARY)
