# Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property and
# proprietary rights in and to this software and related documentation.  Any
# use, reproduction, disclosure or distribution of this software and related
# documentation without an express license agreement from NVIDIA Corporation
# is strictly prohibited.
#
LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_SRC_FILES := $(call all-java-files-under, src)

LOCAL_PACKAGE_NAME := NvCapTest
LOCAL_CERTIFICATE := platform
LOCAL_SDK_VERSION := current

LOCAL_JNI_SHARED_LIBRARIES := libnvcaptest_jni

include $(NVIDIA_PACKAGE)

include $(call all-makefiles-under,$(LOCAL_PATH))
