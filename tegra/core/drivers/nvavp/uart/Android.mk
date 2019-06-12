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

LOCAL_MODULE := libnvavpuart
LOCAL_ARM_MODE := arm
LOCAL_CFLAGS += -Os -ggdb0
ifeq ($(AVP_EXTERNAL_TOOLCHAIN),)
  LOCAL_C_INCLUDES += $(TOP)/bionic/libc/include
  LOCAL_CFLAGS += -D_MALLOC_H_
endif

ifeq ($(BOOT_MINIMAL_BL),1)
LOCAL_CFLAGS += -DBOOT_MINIMAL_BL
endif

LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc/$(TARGET_TEGRA_FAMILY)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvavp/uart
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvavp/uart/$(TARGET_TEGRA_VERSION)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot/$(TARGET_TEGRA_VERSION)/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/utils/nvos/aos

USE_UART_STUB := false

ifeq ($(NV_EMBEDDED_BUILD), 1)
USE_UART_STUB := true
endif


ifeq ($(TARGET_PRODUCT),bonaire_sim)
USE_UART_STUB := true
endif
ifeq ($(USE_UART_STUB),true)
LOCAL_SRC_FILES := avp_uart_stub.c
else
LOCAL_SRC_FILES := avp_uart.c
LOCAL_SRC_FILES += avp_vsnprintf.c
endif

include $(NVIDIA_STATIC_AVP_LIBRARY)
