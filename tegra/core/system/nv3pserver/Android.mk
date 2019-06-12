#
# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnv3pserver
LOCAL_NVIDIA_NO_COVERAGE := true

ifneq ($(filter t30, $(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DNV_AOS_ENTRY_POINT=0x80108000
LOCAL_CFLAGS += -DNV_MICROBOOT_ENTRY_POINT=0x4000a000
LOCAL_CFLAGS += -DNV_RNG_SUPPORT=0
LOCAL_CFLAGS += -DENABLE_TXX
else
LOCAL_CFLAGS += -DNV_AOS_ENTRY_POINT=0x80108000
LOCAL_CFLAGS += -DNV_MICROBOOT_ENTRY_POINT=0x4000e000
LOCAL_CFLAGS += -DNV_RNG_SUPPORT=1
endif

LOCAL_CFLAGS += -DENABLE_THREADED_DOWNLOAD=1
ifeq ($(filter t30 t114 t148, $(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DPMC_BASE=0x7000E400
endif

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/fastboot
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot/$(TARGET_TEGRA_VERSION)/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot/$(TARGET_TEGRA_VERSION)/include/$(TARGET_TEGRA_VERSION)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/utils/nvos/aos/nvap

ifeq ($(TARGET_USE_NCT),true)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvnct
LOCAL_CFLAGS += -DNV_USE_NCT
endif
LOCAL_CFLAGS += -DNVODM_BOARD_IS_FPGA=0
#LOCAL_CFLAGS += -DNV_IS_AOS=1
LOCAL_CFLAGS += -DNVODM_BOARD_IS_SIMULATION=0
ifeq ($(NV_EMBEDDED_BUILD),1)
LOCAL_CFLAGS += -DNV_EMBEDDED_BUILD
endif

LOCAL_SRC_FILES += nv3p_server.c
LOCAL_SRC_FILES += nv3p_server_utils_t30.c
LOCAL_SRC_FILES += nv3p_server_utils_t1xx.c
LOCAL_SRC_FILES += nv3p_server_utils.c
LOCAL_SRC_FILES += nv3p_secureflow.c

LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
include $(NVIDIA_STATIC_LIBRARY)


include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnv3pserverhost

ifneq ($(filter t30, $(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DNV_AOS_ENTRY_POINT=0x80108000
LOCAL_CFLAGS += -DNV_MICROBOOT_ENTRY_POINT=0x4000a000
LOCAL_CFLAGS += -DNV_RNG_SUPPORT=0
LOCAL_CFLAGS += -DENABLE_TXX
LOCAL_CFLAGS += -DCONFIG_ARCH_TEGRA_3x_SOC
else
LOCAL_CFLAGS += -DNV_AOS_ENTRY_POINT=0x80108000
LOCAL_CFLAGS += -DNV_MICROBOOT_ENTRY_POINT=0x4000e000
LOCAL_CFLAGS += -DNV_RNG_SUPPORT=1
endif

LOCAL_CFLAGS += -DNVODM_BOARD_IS_FPGA=0
LOCAL_CFLAGS += -DNVODM_BOARD_IS_SIMULATION=1
ifeq ($(NV_EMBEDDED_BUILD),1)
LOCAL_CFLAGS += -DNV_EMBEDDED_BUILD
endif

LOCAL_CFLAGS += -DENABLE_THREADED_DOWNLOAD=0

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/fastboot
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot/$(TARGET_TEGRA_VERSION)/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot/$(TARGET_TEGRA_VERSION)/include/$(TARGET_TEGRA_VERSION)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/utils/nvos/aos/nvap
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)
ifeq ($(TARGET_USE_NCT),true)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvnct
LOCAL_CFLAGS += -DNV_USE_NCT
endif

LOCAL_SRC_FILES += nv3p_server.c
LOCAL_SRC_FILES += nv3p_server_utils_t30.c
LOCAL_SRC_FILES += nv3p_server_utils_t1xx.c
LOCAL_SRC_FILES += nv3p_server_utils.c
LOCAL_SRC_FILES += nv3p_secureflow.c

include $(NVIDIA_HOST_STATIC_LIBRARY)
