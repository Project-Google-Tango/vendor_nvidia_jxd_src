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
LOCAL_MODULE_TAGS := nvidia_tests
LOCAL_C_INCLUDES:= $(LOCAL_PATH) \
	$(TOP)/3rdparty/ote/lib/include \
	$(TEGRA_TOP)/core/include
LOCAL_SHARED_LIBRARIES := libnvos
LOCAL_SRC_FILES:= testapp.c
LOCAL_STATIC_LIBRARIES:= \
		libtlk_client \
		libtlk_common
LOCAL_MODULE:= testapp
include $(NVIDIA_EXECUTABLE)

else

LOCAL_PATH:= $(call my-dir)
include $(NVIDIA_DEFAULTS)
LOCAL_MODULE_TAGS := nvidia_tests
LOCAL_C_INCLUDES := $(LOCAL_PATH) \
		$(TEGRA_TOP)/core/include
LOCAL_SHARED_LIBRARIES := libnvos
LOCAL_SRC_FILES:= testapp_stub.c
LOCAL_MODULE:= testapp
include $(NVIDIA_EXECUTABLE)

endif # SECURE_OS_BUILD == tlk
