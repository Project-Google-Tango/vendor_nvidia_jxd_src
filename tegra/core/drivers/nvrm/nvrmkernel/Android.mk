#
# Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#
LOCAL_PATH := $(call my-dir)

# stub

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvrm

LOCAL_CFLAGS += -DNV_IS_DYNAMIC=1
LOCAL_CFLAGS += -DNVRM_TRANSPORT_IN_KERNEL=1

ifeq ($(TARGET_TEGRA_FAMILY), t11x)
LOCAL_CFLAGS += -DTARGET_SOC_T11X
endif
ifeq ($(TARGET_TEGRA_FAMILY), t12x)
LOCAL_CFLAGS += -DTARGET_SOC_T12X
endif

LOCAL_C_INCLUDES += $(LOCAL_PATH)/core/common
LOCAL_C_INCLUDES += external/valgrind/main/include
LOCAL_C_INCLUDES += external/valgrind/main/memcheck

LOCAL_SRC_FILES += core/common/nvrm_avp_service.c
LOCAL_SRC_FILES += core/common/nvrm_avp_cpu_rpc.c
LOCAL_SRC_FILES += core/common/nvrm_chip.c
LOCAL_SRC_FILES += core/common/nvrm_hwmap.c
LOCAL_SRC_FILES += core/common/nvrm_surface.c
LOCAL_SRC_FILES += core/common/nvrm_surface_debug.c
LOCAL_SRC_FILES += core/common/nvrm_stub_helper.c
LOCAL_SRC_FILES += core/common/nvrm_stub_helper_linux.c
LOCAL_SRC_FILES += core/common/nvrm_stub_helper_linux_nvmap.c
LOCAL_SRC_FILES += core/common/nvrm_stub_helper_linux_ion.c
LOCAL_SRC_FILES += core/common/nvrm_moduleloader_linux.c
LOCAL_SRC_FILES += core/common/nvrpc_helper.c
LOCAL_SRC_FILES += null_stubs/nvrm_analog_stub.c
LOCAL_SRC_FILES += null_stubs/nvrm_diag_stub.c
LOCAL_SRC_FILES += null_stubs/nvrm_gpio_stub.c
LOCAL_SRC_FILES += null_stubs/nvrm_i2c_stub.c
LOCAL_SRC_FILES += null_stubs/nvrm_init_stub.c
LOCAL_SRC_FILES += null_stubs/nvrm_keylist_stub.c
LOCAL_SRC_FILES += null_stubs/nvrm_module_stub.c
LOCAL_SRC_FILES += null_stubs/nvrm_owr_stub.c
LOCAL_SRC_FILES += null_stubs/nvrm_pinmux_stub.c
LOCAL_SRC_FILES += null_stubs/nvrm_pmu_stub.c
LOCAL_SRC_FILES += null_stubs/nvrm_power_stub.c
LOCAL_SRC_FILES += null_stubs/nvrm_pwm_stub.c
LOCAL_SRC_FILES += null_stubs/nvrm_spi_stub.c

LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_STATIC_LIBRARIES += libmd5

LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
LOCAL_CFLAGS += -Wno-missing-field-initializers
include $(NVIDIA_SHARED_LIBRARY)

# implementation static library for fastboot

include $(NVIDIA_DEFAULTS)
LOCAL_NVIDIA_NO_COVERAGE := true

LOCAL_MODULE := libnvrm_impl

LOCAL_CFLAGS += -Wno-error=missing-prototypes
LOCAL_CFLAGS += -Wno-error=implicit-function-declaration
LOCAL_CFLAGS += -DNV_IS_DYNAMIC=0
LOCAL_CFLAGS += -DNVRM_TRANSPORT_IN_KERNEL=1

ifeq ($(TARGET_TEGRA_FAMILY), t30)
LOCAL_CFLAGS += -DCHIP_T30
endif

ifeq ($(BOOT_MINIMAL_BL),1)
LOCAL_CFLAGS += -DBOOT_MINIMAL_BL
endif

# a bit yucky, have to force this
LOCAL_CFLAGS += -DNVOS_IS_LINUX=0
LOCAL_CFLAGS += -DNVOS_USE_WRITECOMBINE=1

LOCAL_C_INCLUDES += $(LOCAL_PATH)/core
LOCAL_C_INCLUDES += $(LOCAL_PATH)/core/common
LOCAL_C_INCLUDES += $(LOCAL_PATH)/io
LOCAL_C_INCLUDES += $(LOCAL_PATH)/io/common
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../graphics/common
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../graphics
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/utils/nvos/aos/nvap

LOCAL_SRC_FILES += core/common/nvrm_pinmux.c
LOCAL_SRC_FILES += core/common/nvrm_heap_simple.c
LOCAL_SRC_FILES += core/common/nvrm_memmgr.c
LOCAL_SRC_FILES += core/common/nvrm_heap_carveout.c
LOCAL_SRC_FILES += core/common/nvrm_heap_secure.c
LOCAL_SRC_FILES += core/common/nvrm_heap_iram.c
LOCAL_SRC_FILES += core/common/nvrm_keylist.c
LOCAL_SRC_FILES += core/common/nvrm_configuration.c
LOCAL_SRC_FILES += core/common/nvrm_pmu.c
LOCAL_SRC_FILES += core/common/nvrm_module.c
LOCAL_SRC_FILES += core/common/nvrm_hwintf.c
LOCAL_SRC_FILES += core/common/nvrm_chip.c
LOCAL_SRC_FILES += core/common/nvrm_clocks_limits.c
LOCAL_SRC_FILES += core/common/nvrm_hwmap.c
LOCAL_SRC_FILES += core/common/nvrm_power.c
LOCAL_SRC_FILES += core/common/nvrm_power_dfs.c
LOCAL_SRC_FILES += core/common/nvrm_rmctrace.c
LOCAL_SRC_FILES += core/common/nvrm_relocation_table.c
LOCAL_SRC_FILES += core/common/nvrm_surface.c
LOCAL_SRC_FILES += core/common/nvrm_surface_debug.c
LOCAL_SRC_FILES += core/common/nvrm_init.c
LOCAL_SRC_FILES += io/common/nvrm_i2c.c
LOCAL_SRC_FILES += io/common/nvrm_gpioi2c.c
LOCAL_SRC_FILES += io/common/nvrm_owr.c
LOCAL_SRC_FILES += io/ap15/ap15rm_dma_hw_private.c
LOCAL_SRC_FILES += io/ap15/rm_common_slink_hw_private.c
LOCAL_SRC_FILES += io/ap15/ap15rm_pwm.c
LOCAL_SRC_FILES += core/ap15/ap15rm_clock_config.c
LOCAL_SRC_FILES += io/ap15/ap15rm_analog.c
LOCAL_SRC_FILES += core/ap15/nvrm_clocks.c
LOCAL_SRC_FILES += io/ap15/rm_dma_hw_private.c
LOCAL_SRC_FILES += core/ap15/ap15rm_power.c
LOCAL_SRC_FILES += core/ap15/ap15rm_power_dfs.c
LOCAL_SRC_FILES += core/ap15/ap15rm_power_utils.c
LOCAL_SRC_FILES += core/ap15/ap15rm_clock_misc.c
LOCAL_SRC_FILES += core/ap15/nvrm_diag.c
LOCAL_SRC_FILES += core/ap15/ap15rm_init.c
LOCAL_SRC_FILES += core/ap15/ap15rm_interrupt.c
LOCAL_SRC_FILES += io/ap15/ap15rm_gpio_vi.c
LOCAL_SRC_FILES += io/ap15/nvrm_dma.c
LOCAL_SRC_FILES += io/ap15/nvrm_gpio.c
LOCAL_SRC_FILES += io/ap15/nvrm_gpio_private.c
LOCAL_SRC_FILES += io/ap15/nvrm_gpio_stub_helper.c
LOCAL_SRC_FILES += io/ap15/ap15rm_dma_intr.c
LOCAL_SRC_FILES += io/ap15/rm_spi_hw_private.c
LOCAL_SRC_FILES += io/ap15/rm_spi_slink.c
LOCAL_SRC_FILES += core/ap20/ap20rm_power_dfs.c
LOCAL_SRC_FILES += io/ap20/ap20rm_i2c.c
LOCAL_SRC_FILES += io/ap20/ap20rm_slink_hw_private.c
LOCAL_SRC_FILES += io/t30/t30rm_owr.c
LOCAL_SRC_FILES += io/t30/t30rm_dma_hw_private.c
LOCAL_SRC_FILES += io/t30/t30rm_slink_hw_private.c
LOCAL_SRC_FILES += io/t30/t30rm_i2c_slave.c
LOCAL_SRC_FILES += core/common/nvrm_memctrl.c
LOCAL_SRC_FILES += core/ap15/ap15rm_interrupt_generic.c
LOCAL_SRC_FILES += core/t30/t30rm_smmu.c
LOCAL_SRC_FILES += core/t30/t30rm_smmu_clients.c

#FIX IT BUG 920412:DSI is bondout on 7.2
LOCAL_CFLAGS += -DUPDATE_DSI_BONDOUT
LOCAL_SRC_FILES += core/common/nvrm_transport.c
LOCAL_SRC_FILES += core/ap15/ap15rm_xpc_hw_private.c
LOCAL_SRC_FILES += core/ap15/ap15rm_xpc.c

LOCAL_SRC_FILES += core/$(TARGET_TEGRA_FAMILY)/$(TARGET_TEGRA_FAMILY)rm_reloctable.c
LOCAL_SRC_FILES += core/$(TARGET_TEGRA_FAMILY)/$(TARGET_TEGRA_FAMILY)rm_fuse.c
LOCAL_SRC_FILES += core/$(TARGET_TEGRA_FAMILY)/$(TARGET_TEGRA_FAMILY)rm_pinmux_tables.c
LOCAL_SRC_FILES += core/$(TARGET_TEGRA_FAMILY)/$(TARGET_TEGRA_FAMILY)rm_clocks.c
LOCAL_SRC_FILES += core/$(TARGET_TEGRA_FAMILY)/$(TARGET_TEGRA_FAMILY)rm_clocks_info.c

LOCAL_WHOLE_STATIC_LIBRARIES += libnvrm_limits

LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
LOCAL_CFLAGS += -Wno-missing-field-initializers
LOCAL_CFLAGS += -Wno-error=enum-compare

ifeq ($(TARGET_BOARD),simulation)
  LOCAL_CFLAGS += -DPLATFORM_SIMULATION=1
else
  LOCAL_CFLAGS += -DPLATFORM_SIMULATION=0
endif

ifeq ($(TARGET_BOARD_PLATFORM_TYPE),fpga)
  LOCAL_CFLAGS += -DPLATFORM_EMULATION=1
else
  LOCAL_CFLAGS += -DPLATFORM_EMULATION=0
endif

include $(NVIDIA_STATIC_LIBRARY)
