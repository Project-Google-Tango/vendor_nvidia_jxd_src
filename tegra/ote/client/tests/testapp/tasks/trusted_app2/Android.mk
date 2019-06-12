#
# Copyright (c) 2012-2014 NVIDIA CORPORATION.  All Rights Reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

ifeq (tlk,$(SECURE_OS_BUILD))

LOCAL_PATH:= $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES := $(LOCAL_PATH) \
	$(TOP)/3rdparty/ote/lib/include
LOCAL_SRC_FILES := trusted_app.c manifest.c

LOCAL_MODULE := trusted_app2
LOCAL_STATIC_LIBRARIES:= \
	libtlk_common \
	libtlk_nv_hw_ext \
	libtlk_service \
	libc

LOCAL_LDFLAGS := -T $(TOP)/3rdparty/ote/lib/task_linker_script.ld

LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_UNINSTALLABLE_MODULE := true
include $(NVIDIA_EXECUTABLE)

endif # SECURE_OS_BUILD == tlk
