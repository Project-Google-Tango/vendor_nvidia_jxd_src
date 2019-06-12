LOCAL_PATH := $(call my-dir)

include $(call all-subdir-makefiles)

include $(CLEAR_VARS)
LOCAL_MODULE := python_source_files
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := $(local_python_modules)
include $(BUILD_PHONY_PACKAGE)
