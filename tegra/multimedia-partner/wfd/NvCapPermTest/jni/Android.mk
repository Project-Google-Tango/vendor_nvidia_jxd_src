# Copyright (c) 2012, NVIDIA CORPORATION.  All Rights Reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property and
# proprietary rights in and to this software and related documentation.  Any
# use, reproduction, disclosure or distribution of this software and related
# documentation without an express license agreement from NVIDIA Corporation
# is strictly prohibited.
#
LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

# All of the source files that we will compile.
LOCAL_SRC_FILES:= \
    nvcap_perm_test_jni.cpp

# All of the shared libraries we link against.
LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libnvcap  \
    libutils

# Also need the JNI headers.
LOCAL_C_INCLUDES +=  \
    $(JNI_H_INCLUDE) \
    $(TEGRA_TOP)/core/include \

LOCAL_MODULE := libnvcappermtest_jni

include $(NVIDIA_SHARED_LIBRARY)

