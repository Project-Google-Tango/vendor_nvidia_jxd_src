LOCAL_PATH:= $(call my-dir)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE_TAGS := nvidia_tests
LOCAL_SRC_FILES := $(call all-java-files-under, src)
LOCAL_PACKAGE_NAME := PresentationApiSample
LOCAL_SDK_VERSION := current
include $(BUILD_PACKAGE)
