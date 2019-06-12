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
include $(NVIDIA_DEFAULTS)

LOCAL_NVIDIA_NULL_COVERAGE := true

LOCAL_MODULE := nvbdktestbl
# place bootloader at root of output hierarchy
LOCAL_MODULE_PATH := $(PRODUCT_OUT)

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nv3p
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvbootupdate
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvrm/nvrmkernel/core/common
LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvaboot
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/utils/nvos/aos
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvbdktest/server
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvbdktest/framework
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvbdktest/testsources
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvddk/fuses/read

LOCAL_CFLAGS += -DNV_AOS_ENTRY_POINT=0x80108000
LOCAL_CFLAGS += -DNV_AOS_LOAD_ADDRESS=0x80108000

ifeq ($(TARGET_PRODUCT),dalmore)
LOCAL_CFLAGS += -DHACKS_DALMORE_ENABLE=1
else
LOCAL_CFLAGS += -DHACKS_DALMORE_ENABLE=0
endif
ifneq ($(filter  t114,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DBL_DISP_CONTROLLER=1
LOCAL_CFLAGS += -DTSEC_EXISTS=1
LOCAL_CFLAGS += -DVPR_EXISTS=1
LOCAL_CFLAGS += -DXUSB_EXISTS=1
endif

ifeq ($(BOOT_MINIMAL_BL),1)
LOCAL_CFLAGS += -DBOOT_MINIMAL_BL
endif

LOCAL_SRC_FILES += nvbdktest.c

LOCAL_CFLAGS += -std=c9x

LOCAL_STATIC_LIBRARIES += libnvaboot
LOCAL_STATIC_LIBRARIES += libnv3p
LOCAL_STATIC_LIBRARIES += libnv3pserver
LOCAL_STATIC_LIBRARIES += libnvpartmgr
LOCAL_STATIC_LIBRARIES += libnvstormgr
LOCAL_STATIC_LIBRARIES += libnvbootupdate
LOCAL_STATIC_LIBRARIES += libnvcrypto
ifeq ($(BOOT_MINIMAL_BL),1)
LOCAL_STATIC_LIBRARIES += libnvaes_ref
endif
LOCAL_STATIC_LIBRARIES += libnvsystem_utils
LOCAL_STATIC_LIBRARIES += libnvfsmgr
LOCAL_STATIC_LIBRARIES += libnvfs
LOCAL_STATIC_LIBRARIES += libnvddk_aes
LOCAL_STATIC_LIBRARIES += libnvddk_blockdevmgr
LOCAL_STATIC_LIBRARIES += libnvddk_disp
LOCAL_STATIC_LIBRARIES += libnvddk_i2s
LOCAL_STATIC_LIBRARIES += libnvddk_keyboard
LOCAL_STATIC_LIBRARIES += libnvddk_sdio
LOCAL_STATIC_LIBRARIES += libnvddk_snor
LOCAL_STATIC_LIBRARIES += libnvddk_se
LOCAL_STATIC_LIBRARIES += libnvddk_spif
LOCAL_STATIC_LIBRARIES += libnvrm_impl
LOCAL_STATIC_LIBRARIES += libnvrm_channel_impl
LOCAL_STATIC_LIBRARIES += libnvodm_keyboard
ifneq ($(filter t124,$(TARGET_TEGRA_VERSION)),)
LOCAL_STATIC_LIBRARIES += libnvddk_usbphy
endif
ifeq ($(filter $(TARGET_BOARD_PLATFORM_TYPE),fpga simulation),)
# HACK:Removing charging temporarily for Dalmore. This needs to
# removed when functionality is added.
ifneq ($(IS_PLATFORM_DALMORE),true)
LOCAL_CFLAGS += -DLPM_BATTERY_CHARGING=1
LOCAL_STATIC_LIBRARIES += libnvodm_charging
endif
ifneq ($(TARGET_PRODUCT),p1852)
LOCAL_CFLAGS += -DLPM_BATTERY_CHARGING=1
LOCAL_STATIC_LIBRARIES += libnvodm_charging
endif
ifeq ($(NV_EMBEDDED_BUILD),1)
LOCAL_STATIC_LIBRARIES += libnvsku
LOCAL_STATIC_LIBRARIES += libnvodm_avp
endif
endif
LOCAL_STATIC_LIBRARIES += libnvodm_fuelgaugefwupgrade
LOCAL_STATIC_LIBRARIES += libnvodm_disp
LOCAL_STATIC_LIBRARIES += libnvodm_query_static_avp
LOCAL_STATIC_LIBRARIES += libnvodm_audiocodec
LOCAL_STATIC_LIBRARIES += libnvodm_tmon
LOCAL_STATIC_LIBRARIES += libnvodm_pmu
LOCAL_STATIC_LIBRARIES += libnvodm_misc_static
LOCAL_STATIC_LIBRARIES += libnvodm_gpio_ext
LOCAL_STATIC_LIBRARIES += libnvfxmath
LOCAL_STATIC_LIBRARIES += libnvextfsmgr
LOCAL_STATIC_LIBRARIES += libnvextfs
LOCAL_STATIC_LIBRARIES += libnvodm_services_os_avp
LOCAL_STATIC_LIBRARIES += libnvgpio_avp
LOCAL_STATIC_LIBRARIES += libnvos_aos
LOCAL_STATIC_LIBRARIES += libnvos_aos_avp
LOCAL_STATIC_LIBRARIES += libnvappmain_aos
LOCAL_STATIC_LIBRARIES += libnvbct
LOCAL_STATIC_LIBRARIES += libnvintr
LOCAL_STATIC_LIBRARIES += libnvodm_system_update
LOCAL_STATIC_LIBRARIES += libnvrsa
LOCAL_STATIC_LIBRARIES += libnvddk_fuse_read_avp
LOCAL_STATIC_LIBRARIES += libnvddk_fuse_write_avp
ifneq ($(filter t114,$(TARGET_TEGRA_VERSION)),)
LOCAL_STATIC_LIBRARIES += libtsec_otf_keygen
LOCAL_STATIC_LIBRARIES += libxusb_fw_load
LOCAL_STATIC_LIBRARIES += libnvddk_xusb
endif

ifeq ($(NV_TARGET_BOOTLOADER_PINMUX),kernel)
LOCAL_STATIC_LIBRARIES += libnvpinmux_avp
endif

LOCAL_STATIC_LIBRARIES += libnvavpuart
LOCAL_STATIC_LIBRARIES += libnvbdktest_server
LOCAL_STATIC_LIBRARIES += libnvbdktest_framework
LOCAL_STATIC_LIBRARIES += libnvbdktest_testsources
LOCAL_STATIC_LIBRARIES += libnvddk_fuse_bypass
LOCAL_STATIC_LIBRARIES += libnvavpsdmmc

ifeq ($(filter t30,$(TARGET_TEGRA_VERSION)),)
LOCAL_STATIC_LIBRARIES += libnvddk_spi
endif

ifneq ($(filter t124,$(TARGET_TEGRA_VERSION)),)
LOCAL_STATIC_LIBRARIES += libnvddk_sata
LOCAL_CFLAGS += -DSATA_EXISTS=1
endif

ifneq ($(filter t148,$(TARGET_TEGRA_VERSION)),)
LOCAL_STATIC_LIBRARIES += libnvddk_mipibif
endif
LOCAL_STATIC_LIBRARIES += libnvddk_i2c

ifeq ($(TARGET_USE_NCT),true)
LOCAL_STATIC_LIBRARIES += libnvnct
endif

LOCAL_NVIDIA_LINK_SCRIPT := $(TEGRA_TOP)/core/utils/nvos/aos/nvap/t30_cpu.x

include $(NVIDIA_STATIC_EXECUTABLE)

# Include sub dir makefiles
include $(addprefix $(LOCAL_PATH)/, $(addsuffix /Android.mk, \
	framework \
	server \
	testsources \
))
