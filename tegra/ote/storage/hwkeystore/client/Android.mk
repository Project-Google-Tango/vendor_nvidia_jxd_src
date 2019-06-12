#
# Copyright (c) 2013 NVIDIA CORPORATION.  All Rights Reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

ifeq ($(SECURE_OS_BUILD),tlk)

LOCAL_PATH := $(call my-dir)

ifneq ($(TARGET_SIMULATOR),true)

include $(NVIDIA_DEFAULTS)

# HAL module implemenation, not prelinked and stored in
# hw/<COPYPIX_HARDWARE_MODULE_ID>.<ro.board.platform>.so

LOCAL_MODULE := keystore.tegra

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_SRC_FILES := hw_keystore.c

LOCAL_C_INCLUDES := \
	system/security/keystore \
	external/openssl/include \
	3rdparty/ote/lib/include

LOCAL_SHARED_LIBRARIES := liblog libcrypto

LOCAL_STATIC_LIBRARIES := libtlk_client libtlk_common

LOCAL_MODULE_TAGS := optional

LOCAL_NVIDIA_WARNINGS_AS_ERRORS := 1

include $(NVIDIA_SHARED_LIBRARY)

endif # !TARGET_SIMULATOR

endif # tlk
