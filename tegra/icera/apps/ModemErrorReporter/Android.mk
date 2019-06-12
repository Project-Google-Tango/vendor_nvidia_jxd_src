LOCAL_PATH:= $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-java-files-under, src) \

LOCAL_PACKAGE_NAME := IceraDebugAgent

LOCAL_CERTIFICATE := platform

LOCAL_STATIC_JAVA_LIBRARIES := android-support-v4

#LOCAL_PROGUARD_ENABLED := disabled
#LOCAL_PROGUARD_FLAG_FILES := proguard.flags
 
include $(NVIDIA_PACKAGE)

#include $(CLEAR_VARS)

#LOCAL_PREBUILT_STATIC_JAVA_LIBRARIES := android-support-v4-4.1.2:libs/android-support-v4.jar

#include $(BUILD_MULTI_PREBUILT)
