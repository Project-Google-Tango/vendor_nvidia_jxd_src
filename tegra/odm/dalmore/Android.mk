#
# Copyright (c) 2012-2013 NVIDIA Corporation. All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto. Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#

ifneq (,$(filter $(TARGET_DEVICE),dalmore))
LOCAL_PATH := $(call my-dir)
TEMPLATE_PATH := $(LOCAL_PATH)/../template

include $(addsuffix /Android.mk,\
        $(LOCAL_PATH)/odm_kit/query \
        $(LOCAL_PATH)/odm_kit/adaptations/disp \
        $(TEMPLATE_PATH)/odm_kit/adaptations/dtvtuner \
        $(TEMPLATE_PATH)/odm_kit/adaptations/hdmi \
        $(TEMPLATE_PATH)/odm_kit/adaptations/pmu \
        $(LOCAL_PATH)/odm_kit/adaptations/misc \
        $(TEMPLATE_PATH)/odm_kit/platform/touch \
        $(TEMPLATE_PATH)/odm_kit/platform/fuelgaugefwupgrade \
        $(LOCAL_PATH)/odm_kit/platform/keyboard \
        $(TEMPLATE_PATH)/odm_kit/adaptations/audiocodec \
        $(TEMPLATE_PATH)/odm_kit/adaptations/tmon \
        $(TEMPLATE_PATH)/odm_kit/adaptations/charging \
        $(TEMPLATE_PATH)/odm_kit/adaptations/gpio_ext \
        $(TEMPLATE_PATH)/odm_kit/platform/ota \
        $(TEMPLATE_PATH)/os/linux/system/nvextfsmgr \
        $(TEMPLATE_PATH)/os/linux/system/extfs \
        $(LOCAL_PATH)/nvflash/ \
)
endif
