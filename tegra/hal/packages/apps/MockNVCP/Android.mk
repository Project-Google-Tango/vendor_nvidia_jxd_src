# As this depends on com.nvidia.display, only build it if that will be built
ifneq (,$(filter $(BOARD_INCLUDES_TEGRA_JNI),display))
LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_SRC_FILES := $(call all-subdir-java-files)

LOCAL_PACKAGE_NAME := MockNVCP

LOCAL_MODULE_TAGS := optional

# link against the NVIDIA display library
LOCAL_JAVA_LIBRARIES := com.nvidia.display

include $(NVIDIA_PACKAGE)
endif
