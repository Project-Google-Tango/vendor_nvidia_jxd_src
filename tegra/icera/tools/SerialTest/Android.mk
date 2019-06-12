ifndef PLATFORM_IS_JELLYBEAN

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-java-files-under, src) \

LOCAL_PACKAGE_NAME := SerialTest

include $(NVIDIA_PACKAGE)
endif
