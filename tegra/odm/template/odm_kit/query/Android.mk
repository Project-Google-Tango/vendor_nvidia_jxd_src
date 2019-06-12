#
# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#
LOCAL_PATH := $(call my-dir)
ifneq (,$(filter $(TARGET_PRODUCT),harmony ventana))

LOCAL_COMMON_SRC_FILES := nvodm_query.c
LOCAL_COMMON_SRC_FILES += nvodm_query_discovery.c
LOCAL_COMMON_SRC_FILES += nvodm_query_nand.c
LOCAL_COMMON_SRC_FILES += nvodm_query_gpio.c
LOCAL_COMMON_SRC_FILES += nvodm_query_pinmux.c
LOCAL_COMMON_SRC_FILES += nvodm_query_kbc.c


include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvodm_query
# for some reason nvodm_query needs to be arm, as it is called
# very early from the bootloader
LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES += $(LOCAL_COMMON_SRC_FILES)

LOCAL_SHARED_LIBRARIES += libnvrm
LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_STATIC_LIBRARIES += libnvodm_services

LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
LOCAL_CFLAGS += -Wno-missing-field-initializers
LOCAL_CFLAGS += -Wno-missing-field-initializers

include $(NVIDIA_STATIC_AND_SHARED_LIBRARY)


include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvodm_query_static_avp
# for some reason nvodm_query needs to be arm, as it is called
# very early from the bootloader
LOCAL_ARM_MODE := arm

LOCAL_CFLAGS += -Os -ggdb0
LOCAL_CFLAGS += -DFPGA_BOARD
LOCAL_CFLAGS += -DAVP_PINMUX=0

LOCAL_C_INCLUDES += $(LOCAL_COMMON_C_INCLUDES)
LOCAL_SRC_FILES += $(LOCAL_COMMON_SRC_FILES)

LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
LOCAL_CFLAGS += -Wno-missing-field-initializers
LOCAL_CFLAGS += -Wno-missing-field-initializers
include $(NVIDIA_STATIC_AVP_LIBRARY)


include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvodm_query_avp
# For some reason nvodm_query needs to be arm, as it is called
# very early from the bootloader.
LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES += $(LOCAL_COMMON_SRC_FILES)
LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
LOCAL_CFLAGS += -Wno-missing-field-initializers
LOCAL_CFLAGS += -Wno-missing-field-initializers
LOCAL_CFLAGS += -Os -ggdb0
include $(NVIDIA_STATIC_AVP_LIBRARY)

endif
