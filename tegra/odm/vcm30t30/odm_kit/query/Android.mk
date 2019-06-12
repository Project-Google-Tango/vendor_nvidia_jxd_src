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

LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/core/drivers/hwinc/t30

ifeq ($(wildcard vendor/nvidia/tegra/core-private),vendor/nvidia/tegra/core-private)
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/bootloader/nvbootloader/odm-partner/template/odm_kit/adaptations
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/bootloader/nvbootloader/odm-partner/template
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/bootloader/nvbootloader/odm-partner/vcm30t30/odm_kit/adaptations/pmu
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/bootloader/nvbootloader/odm-partner/template/odm_kit/query
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/bootloader/nvbootloader/odm-partner/template/odm_kit/query/include
else
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/odm/template/odm_kit/adaptations
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/odm/template
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/odm/vcm30t30/odm_kit/adaptations/pmu
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/odm/template/odm_kit/query
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/odm/template/odm_kit/query/include
endif

LOCAL_COMMON_CFLAGS := -DFPGA_BOARD
LOCAL_COMMON_SRC_FILES := nvodm_query.c
LOCAL_COMMON_SRC_FILES += nvodm_query_discovery.c
LOCAL_COMMON_SRC_FILES += nvodm_query_nand.c
LOCAL_COMMON_SRC_FILES += nvodm_query_gpio.c
LOCAL_COMMON_SRC_FILES += nvodm_query_pinmux.c
LOCAL_COMMON_SRC_FILES += nvodm_query_kbc.c
LOCAL_COMMON_SRC_FILES += nvodm_platform_init.c
LOCAL_COMMON_SRC_FILES += secure/nvodm_query_secure.c


include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvodm_query
# for some reason nvodm_query needs to be arm, as it is called
# very early from the bootloader
LOCAL_ARM_MODE := arm

LOCAL_C_INCLUDES += $(LOCAL_COMMON_C_INCLUDES)
LOCAL_SRC_FILES += $(LOCAL_COMMON_SRC_FILES)
LOCAL_CFLAGS += $(LOCAL_COMMON_CFLAGS)

LOCAL_SHARED_LIBRARIES += libnvrm
LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_STATIC_LIBRARIES += libnvodm_services

LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1
include $(NVIDIA_STATIC_AND_SHARED_LIBRARY)


include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvodm_query_avp
# For some reason nvodm_query needs to be arm, as it is called
# very early from the bootloader.
LOCAL_ARM_MODE := arm

LOCAL_C_INCLUDES += $(LOCAL_COMMON_C_INCLUDES)
LOCAL_SRC_FILES += $(LOCAL_COMMON_SRC_FILES)
LOCAL_CFLAGS += $(LOCAL_COMMON_CFLAGS)
LOCAL_CFLAGS += -Os -ggdb0
include $(NVIDIA_STATIC_AVP_LIBRARY)
