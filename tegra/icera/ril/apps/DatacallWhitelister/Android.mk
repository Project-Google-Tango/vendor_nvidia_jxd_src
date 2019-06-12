
LOCAL_PATH:= $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-java-files-under, src) \

LOCAL_PACKAGE_NAME := DatacallWhitelister

LOCAL_PROGUARD_ENABLED := disabled

include $(NVIDIA_PACKAGE)
