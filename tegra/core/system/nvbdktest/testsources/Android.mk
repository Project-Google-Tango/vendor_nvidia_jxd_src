# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvbdktest_testsources
LOCAL_NVIDIA_NO_COVERAGE := true

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../framework
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../framework/error_handler
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../framework/registerer
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../drivers/nvavp/uart
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../utils/nvos/aos
LOCAL_C_INCLUDES += $(LOCAL_PATH)/pwm/$(TARGET_TEGRA_FAMILY)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/pcb

LOCAL_SRC_FILES += se/se_test.c
LOCAL_SRC_FILES += dsi/nvddk_dsi_block_driver_test.c
LOCAL_SRC_FILES += uart/nvddk_avp_uart_driver_test.c
LOCAL_SRC_FILES += i2c/nvddk_i2c_driver_test_eeprom.c
LOCAL_SRC_FILES += usb/nvddk_usb_block_driver_test.c
LOCAL_SRC_FILES += sd/nvddk_sd_block_driver_test.c
LOCAL_SRC_FILES += pmu/nvbdk_pmu_test.c
LOCAL_SRC_FILES += pwm/pwm_test.c
LOCAL_SRC_FILES += fuse/nvddk_fuse_test.c
LOCAL_SRC_FILES += usbdevice/nvddk_usbf_test.c
ifneq ($(filter  t148,$(TARGET_TEGRA_VERSION)),)
LOCAL_SRC_FILES += bif/nvddk_mipibif_test.c
else
LOCAL_SRC_FILES += bif/nvddk_mipibif_test_stub.c
endif

LOCAL_SRC_FILES += pcb/nvbdk_init_pcb_test.c

ifneq ($(filter $(TARGET_DEVICE),macallan tegratab ardbeg loki),)
LOCAL_SRC_FILES += pcb/board/nvbdk_pcb_interface_$(TARGET_DEVICE).c
else
LOCAL_SRC_FILES += pcb/board/nvbdk_pcb_interface_default.c
endif
LOCAL_SRC_FILES += pcb/nvbdk_api_osc.c
LOCAL_SRC_FILES += pcb/nvbdk_api_i2c.c
LOCAL_SRC_FILES += pcb/nvbdk_api_emmc.c
LOCAL_SRC_FILES += pcb/nvbdk_api_wifi.c
LOCAL_SRC_FILES += pcb/nvbdk_api_kbd.c

LOCAL_CFLAGS += -DNVODM_BOARD_IS_FPGA=0

LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
include $(NVIDIA_STATIC_LIBRARY)
