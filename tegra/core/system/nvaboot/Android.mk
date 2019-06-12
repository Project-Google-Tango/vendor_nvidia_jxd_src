# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvaboot
LOCAL_NVIDIA_NO_COVERAGE := true

ifneq ($(filter tf y,$(SECURE_OS_BUILD)),)
ifeq ($(TARGET_TEGRA_VERSION),t114)
LOCAL_C_INCLUDES += $(TOP)/3rdparty/trustedlogic/sdk/include/tegra4
endif
ifeq ($(TARGET_TEGRA_VERSION),t30)
LOCAL_C_INCLUDES += $(TOP)/3rdparty/trustedlogic/sdk/include/tegra3
endif
endif

ifeq ($(TF_INCLUDE_WITH_SERVICES),y)
LOCAL_C_INCLUDES += $(TOP)/3rdparty/trustedlogic/donotdistribute/include/$(TARGET_PRODUCT)
endif

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nv3p
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvrm/nvrmkernel/core/common
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/utils/nvos/aos

LOCAL_CFLAGS += -DCOMPILE_ARCH_V7=1

ifeq ($(filter $(TARGET_BOARD_PLATFORM_TYPE),fpga simulation),)
LOCAL_CFLAGS += -DLPM_BATTERY_CHARGING=1
endif

ifneq ($(filter t30,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DHAS_PKC_BOOT_SUPPORT=0
else
LOCAL_CFLAGS += -DHAS_PKC_BOOT_SUPPORT=1
endif

ifneq ($(filter t30 t114,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DBSEAV_USED=1
else
LOCAL_CFLAGS += -DBSEAV_USED=0
endif

ifneq ($(filter t124,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DNVABOOT_T124
endif

ifeq ($(BOOT_MINIMAL_BL),1)
LOCAL_CFLAGS += -DBOOT_MINIMAL_BL
LOCAL_CFLAGS += -DXUSB_EXISTS=0
LOCAL_CFLAGS += -DTSEC_EXISTS=0
LOCAL_CFLAGS += -DVPR_EXISTS=0
else
ifneq ($(filter t114 t124,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DTSEC_EXISTS=1
LOCAL_CFLAGS += -DVPR_EXISTS=1
LOCAL_CFLAGS += -DXUSB_EXISTS=1
endif
endif

ifneq ($(filter t124,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DENABLE_NVTBOOT=1
endif

ifneq ($(filter t114,$(TARGET_TEGRA_VERSION)),)
LOCAL_ASFLAGS += -D__ARM_ERRATA_784420__=1
endif

#Following flag is applicable for all Cortex-A9 based SoCs
ifneq ($(filter t30,$(TARGET_TEGRA_VERSION)),)
LOCAL_ASFLAGS += -D__ARM_ERRATA_794073__=1
endif

ifeq ($(TARGET_USE_NVDUMPER),true)
LOCAL_CFLAGS += -DENABLE_NVDUMPER=1
else
LOCAL_CFLAGS += -DENABLE_NVDUMPER=0
endif

ifeq ($(TARGET_USE_NCT),true)
LOCAL_CFLAGS += -DNV_USE_NCT
endif

LOCAL_SRC_FILES += nvaboot.c
LOCAL_SRC_FILES += nvaboot_cluster_switch.S
LOCAL_SRC_FILES += nvaboot_rawfs.c
LOCAL_SRC_FILES += nvaboot_bootfs.c
LOCAL_SRC_FILES += nvaboot_blockdev_nice.c
LOCAL_SRC_FILES += nvaboot_usbf.c
ifneq ($(filter tlk tf y,$(SECURE_OS_BUILD)),)
ifeq ($(TARGET_TEGRA_VERSION), t114)
LOCAL_SRC_FILES += nvaboot_tf.c
endif
endif
LOCAL_SRC_FILES += nvaboot_warmboot_sign.c
LOCAL_SRC_FILES += nvaboot_sanitize_keys.c
LOCAL_SRC_FILES += nvaboot_sanitize.S
LOCAL_SRC_FILES += nvaboot_warmboot_avp_$(TARGET_TEGRA_VERSION).S
LOCAL_SRC_FILES += nvaboot_$(TARGET_TEGRA_FAMILY).c

#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1
include $(NVIDIA_STATIC_LIBRARY)
