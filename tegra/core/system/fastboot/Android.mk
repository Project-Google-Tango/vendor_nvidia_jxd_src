#
# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#

ifeq ($(BOARD_BUILD_BOOTLOADER),true)

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_NVIDIA_NULL_COVERAGE := true

LOCAL_MODULE := bootloader
# place bootloader at root of output hierarchy
LOCAL_MODULE_PATH := $(PRODUCT_OUT)

# local-intermediates-dir macro requires this to be defined
LOCAL_MODULE_CLASS := EXECUTABLES

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nv3p
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nv3pserver
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvbootupdate
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvddk/disp
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvrm/nvrmkernel/core/common
LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvaboot
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/utils/nvos/aos
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/fastboot/libfdt
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvddk/fuses/read
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvddk/se
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/utils/nvos/aos/nvap
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvavpgpio
ifeq ($(TARGET_USE_NCT),true)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvnct
endif

LOCAL_CFLAGS += -DBUILD_NUMBER=\"$(BUILD_NUMBER)\"

LOCAL_CFLAGS += -DNV_AOS_ENTRY_POINT=0x80108000
LOCAL_CFLAGS += -DNV_AOS_LOAD_ADDRESS=0x80108000
LOCAL_CFLAGS += -DLP0_SUPPORTED

ifneq ($(filter t124,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DENABLE_NVTBOOT=1
endif

ifeq ($(BOOT_MINIMAL_BL),1)
LOCAL_CFLAGS += -DBOOT_MINIMAL_BL
LOCAL_CFLAGS += -DTSEC_EXISTS=0
LOCAL_CFLAGS += -DVPR_EXISTS=0
else
ifneq ($(filter  t114 t148,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DBL_DISP_CONTROLLER=0
LOCAL_CFLAGS += -DTSEC_EXISTS=1
LOCAL_CFLAGS += -DVPR_EXISTS=1
endif
ifneq ($(filter  t124,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DTSEC_EXISTS=1
LOCAL_CFLAGS += -DVPR_EXISTS=1
LOCAL_CFLAGS += -DSATA_EXISTS=1
endif

ifneq ($(filter dalmore pluto ceres,$(TARGET_DEVICE)),)
LOCAL_CFLAGS += -DDISP_FONT_SCALE=2
endif

endif
ifneq ($(filter t12x,$(TARGET_TEGRA_FAMILY)),)
LOCAL_CFLAGS += -DTEGRA_12x_SOC
endif
ifneq ($(filter t14x,$(TARGET_TEGRA_FAMILY)),)
LOCAL_CFLAGS += -DTEGRA_14x_SOC
endif
ifneq ($(filter t11x,$(TARGET_TEGRA_FAMILY)),)
LOCAL_CFLAGS += -DTEGRA_11x_SOC
endif
ifneq ($(filter t30,$(TARGET_TEGRA_FAMILY)),)
LOCAL_CFLAGS += -DTEGRA_3x_SOC
endif

ifeq ($(NV_MOBILE_DGPU),1)
LOCAL_CFLAGS += -DNV_MOBILE_DGPU=1
else
LOCAL_CFLAGS += -DNV_MOBILE_DGPU=0
endif

ifeq ($(TARGET_USE_NCT),true)
LOCAL_CFLAGS += -DNV_USE_NCT
endif

LOCAL_ASFLAGS += $(LOCAL_CFLAGS)

LOCAL_SRC_FILES += libfdt/fdt.c
LOCAL_SRC_FILES += libfdt/fdt_ro.c
LOCAL_SRC_FILES += libfdt/fdt_rw.c
LOCAL_SRC_FILES += libfdt/fdt_strerror.c
LOCAL_SRC_FILES += libfdt/fdt_sw.c
LOCAL_SRC_FILES += libfdt/fdt_wip.c
LOCAL_SRC_FILES += dump.c
LOCAL_SRC_FILES += main.c
LOCAL_SRC_FILES += prettyprint.c
LOCAL_SRC_FILES += gui.c
LOCAL_SRC_FILES += linux_cmdline.c
LOCAL_SRC_FILES += usb.c
LOCAL_SRC_FILES += jingle.c
LOCAL_SRC_FILES += recovery_utils.c
LOCAL_SRC_FILES += nvbllp0/nvbl_lp0.c
LOCAL_SRC_FILES += nvbllp0/sleep.S

ifeq ($(BUILD_NV_CRASHCOUNTER),true)
LOCAL_SRC_FILES += crash_counter.c
LOCAL_CFLAGS += -DNV_USE_CRASH_COUNTER
endif

LOCAL_CFLAGS += -std=c9x
ifeq ($(TARGET_USE_NVDUMPER),true)
LOCAL_CFLAGS += -DENABLE_NVDUMPER=1
else
LOCAL_CFLAGS += -DENABLE_NVDUMPER=0
endif

CHIP_LIST := t148 t124

ifneq ($(filter $(CHIP_LIST),$(TARGET_TEGRA_VERSION)),)
LOCAL_STATIC_LIBRARIES += libnvboot_warmboot_avp
LOCAL_STATIC_LIBRARIES += libnvboot_util_avp
endif
ifneq ($(filter t148,$(TARGET_TEGRA_VERSION)),)
LOCAL_STATIC_LIBRARIES += libnvddk_mipibif
endif
ifneq ($(filter t124,$(TARGET_TEGRA_VERSION)),)
LOCAL_STATIC_LIBRARIES += libnvddk_usbphy
endif
LOCAL_STATIC_LIBRARIES += libnvaboot
LOCAL_STATIC_LIBRARIES += libnvddk_fuse_bypass
LOCAL_STATIC_LIBRARIES += libnv3pserver
LOCAL_STATIC_LIBRARIES += libnv3p
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
LOCAL_STATIC_LIBRARIES += libnvchip

ifneq ($(BOOT_MINIMAL_BL),1)
ifeq ($(filter $(TARGET_BOARD_PLATFORM_TYPE),fpga simulation),)

ifneq ($(filter  enterprise ceres pluto loki ardbeg, $(TARGET_DEVICE)),)
LOCAL_CFLAGS += -DLPM_BATTERY_CHARGING=1
LOCAL_STATIC_LIBRARIES += libnvodm_charging
endif

ifneq ($(filter  ardbeg loki, $(TARGET_PRODUCT)),)
LOCAL_CFLAGS += -DREAD_BATTERY_SOC=1
endif

ifeq ($(NV_EMBEDDED_BUILD),1)
LOCAL_STATIC_LIBRARIES += libnvsku
LOCAL_STATIC_LIBRARIES += libnvodm_avp
endif
endif
endif

ifeq ($(TARGET_PRODUCT),thor)
LOCAL_CFLAGS += -DHAVE_LED
LOCAL_STATIC_LIBRARIES += libnvodm_led
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
LOCAL_STATIC_LIBRARIES += libnvddk_issp
LOCAL_STATIC_LIBRARIES += libnvddk_i2c

ifeq ($(filter t30,$(TARGET_TEGRA_VERSION)),)
LOCAL_STATIC_LIBRARIES += libnvddk_spi
endif
LOCAL_STATIC_LIBRARIES += libnvddk_fuse_read_avp
LOCAL_STATIC_LIBRARIES += libnvddk_fuse_write_avp
ifneq ($(filter t114,$(TARGET_TEGRA_VERSION)),)
LOCAL_STATIC_LIBRARIES += libtsec_otf_keygen
LOCAL_STATIC_LIBRARIES += libxusb_fw_load
LOCAL_STATIC_LIBRARIES += libnvddk_xusb
endif
ifneq ($(filter t124 t148,$(TARGET_TEGRA_VERSION)),)
LOCAL_STATIC_LIBRARIES += libtsec_otf_keygen
endif

ifneq ($(filter t124,$(TARGET_TEGRA_VERSION)),)
LOCAL_STATIC_LIBRARIES += libxusb_fw_load
LOCAL_STATIC_LIBRARIES += libnvddk_sata
endif

ifeq ($(NV_TARGET_BOOTLOADER_PINMUX),kernel)
LOCAL_STATIC_LIBRARIES += libnvpinmux_avp
endif

LOCAL_STATIC_LIBRARIES += libnvavpuart
LOCAL_STATIC_LIBRARIES += libnvavpsdmmc

ifeq ($(TARGET_USE_NCT),true)
LOCAL_STATIC_LIBRARIES += libnvnct
endif
LOCAL_NVIDIA_LINK_SCRIPT := $(TEGRA_TOP)/core/utils/nvos/aos/nvap/t30_cpu.x

LOCAL_NVIDIA_RAW_EXECUTABLE_LDFLAGS := -Map $(call local-intermediates-dir)/$(LOCAL_MODULE).map

include $(NVIDIA_STATIC_EXECUTABLE)
endif
