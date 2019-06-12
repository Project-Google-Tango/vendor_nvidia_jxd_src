#
# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#
LOCAL_PATH := $(call my-dir)

LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvpinmux
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/hwinc/t12x
ifeq ($(wildcard vendor/nvidia/tegra/core-private),vendor/nvidia/tegra/core-private)
#ifneq ($(TEGRA_TOP)/core,vendor/nvidia/proprietary_src/core)
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/bootloader/nvbootloader/odm-partner/template/odm_kit/adaptations
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/bootloader/nvbootloader/odm-partner/template
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/bootloader/nvbootloader/odm-partner/template/odm_kit/adaptations/pmu
else
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/odm/template/odm_kit/adaptations
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/odm/template
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/odm/template/odm_kit/adaptations/pmu
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/odm/template/odm_kit/query/include
endif

LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/camera-partner/imager/include
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/bootloader/nvbootloader/odm-partner/template/odm_kit/query/include
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/core/utils/nvos/aos/nvap

LOCAL_COMMON_SRC_FILES := nvodm_query.c
LOCAL_COMMON_SRC_FILES += nvodm_query_discovery.c
LOCAL_COMMON_SRC_FILES += nvodm_query_nand.c
LOCAL_COMMON_SRC_FILES += nvodm_query_gpio.c
ifneq ($(NV_TARGET_BOOTLOADER_PINMUX),kernel)
LOCAL_COMMON_SRC_FILES += nvodm_query_pinmux.c
else
LOCAL_COMMON_SRC_FILES += nvodm_pinmux_init.c
endif
LOCAL_COMMON_SRC_FILES += nvodm_query_kbc.c
LOCAL_COMMON_SRC_FILES += nvodm_platform_init.c
LOCAL_COMMON_SRC_FILES += secure/nvodm_query_secure.c
LOCAL_COMMON_CFLAGS := -DLPM_BATTERY_CHARGING=1
LOCAL_COMMON_CFLAGS += -DREAD_BATTERY_SOC=1
ifeq ($(NV_TARGET_BOOTLOADER_PINMUX),kernel)
LOCAL_COMMON_CFLAGS += -DSET_KERNEL_PINMUX
endif

include $(NVIDIA_DEFAULTS)


LOCAL_MODULE := libnvodm_query
# for some reason nvodm_query needs to be arm, as it is called
# very early from the bootloader
LOCAL_ARM_MODE := arm

LOCAL_CFLAGS += -DFPGA_BOARD
LOCAL_CFLAGS += -DAVP_PINMUX=0
LOCAL_CFLAGS += -DNOT_SHARED_QUERY_LIBRARY=0
LOCAL_CFLAGS += $(LOCAL_COMMON_CFLAGS)

LOCAL_C_INCLUDES += $(LOCAL_COMMON_C_INCLUDES)
LOCAL_SRC_FILES += $(LOCAL_COMMON_SRC_FILES)

LOCAL_SHARED_LIBRARIES += libnvrm
LOCAL_SHARED_LIBRARIES += libnvos
ifeq ($(NV_TARGET_BOOTLOADER_PINMUX),kernel)
LOCAL_STATIC_LIBRARIES += libnvpinmux
endif
LOCAL_STATIC_LIBRARIES += libnvodm_services
LOCAL_STATIC_LIBRARIES += libnvbct


LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1
include $(NVIDIA_STATIC_AND_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvodm_query_static_avp
# For some reason nvodm_query needs to be arm, as it is called
# very early from the bootloader.
LOCAL_ARM_MODE := arm

LOCAL_C_INCLUDES += $(LOCAL_COMMON_C_INCLUDES)
LOCAL_SRC_FILES += $(LOCAL_COMMON_SRC_FILES)

ifeq ($(BOOT_MINIMAL_BL),1)
LOCAL_CFLAGS += -DBOOT_MINIMAL_BL
endif
LOCAL_CFLAGS += -Os -ggdb0
LOCAL_CFLAGS += -DFPGA_BOARD
LOCAL_CFLAGS += -DAVP_PINMUX=0
LOCAL_CFLAGS += -DNOT_SHARED_QUERY_LIBRARY=1
LOCAL_CFLAGS += $(LOCAL_COMMON_CFLAGS)

include $(NVIDIA_STATIC_AVP_LIBRARY)


include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvodm_query_avp
# For some reason nvodm_query needs to be arm, as it is called
# very early from the bootloader.
LOCAL_ARM_MODE := arm

LOCAL_C_INCLUDES += $(LOCAL_COMMON_C_INCLUDES)
LOCAL_SRC_FILES += $(LOCAL_COMMON_SRC_FILES)

LOCAL_CFLAGS += -Os -ggdb0
LOCAL_CFLAGS += -DAVP_PINMUX=1
LOCAL_CFLAGS += -DNOT_SHARED_QUERY_LIBRARY=1
LOCAL_CFLAGS += $(LOCAL_COMMON_CFLAGS)

include $(NVIDIA_STATIC_AVP_LIBRARY)

