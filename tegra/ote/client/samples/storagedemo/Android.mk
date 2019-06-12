#
# Copyright (c) 2013-2014 NVIDIA CORPORATION.  All Rights Reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

ifeq (tlk,$(SECURE_OS_BUILD))

LOCAL_PATH:= $(call my-dir)

## Make internal task
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES := $(LOCAL_PATH) \
		    $(TOP)/3rdparty/ote/lib/include
LOCAL_SRC_FILES := storagedemo.c manifest.c
LOCAL_MODULE := tlkstoragedemo_task
LOCAL_STATIC_LIBRARIES:= \
	libtlk_service \
	libtlk_common \
	libc

LOCAL_LDFLAGS := -T $(TOP)/3rdparty/ote/lib/task_linker_script.ld

LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_UNINSTALLABLE_MODULE := true
include $(NVIDIA_EXECUTABLE)

# Make client executable
include $(NVIDIA_DEFAULTS)
LOCAL_MODULE_TAGS := nvidia_tests
LOCAL_C_INCLUDES := $(LOCAL_PATH) \
		$(TOP)/3rdparty/ote/lib/include \
		$(TEGRA_TOP)/core/include
LOCAL_SRC_FILES := client_storagedemo.c
LOCAL_STATIC_LIBRARIES := \
	libtlk_common \
	libtlk_client
LOCAL_SHARED_LIBRARIES := libnvos
LOCAL_MODULE := tlkstoragedemo_client

include $(NVIDIA_EXECUTABLE)

else

LOCAL_PATH:= $(call my-dir)
include $(NVIDIA_DEFAULTS)
LOCAL_MODULE_TAGS := nvidia_tests
LOCAL_C_INCLUDES := $(LOCAL_PATH) \
		$(TEGRA_TOP)/core/include
LOCAL_SHARED_LIBRARIES := libnvos
LOCAL_SRC_FILES:= client_storagedemo_stub.c
LOCAL_MODULE := tlkstoragedemo_client
include $(NVIDIA_EXECUTABLE)

endif # SECURE_OS_BUILD == tlk
