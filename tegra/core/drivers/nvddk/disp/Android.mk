# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvddk_disp

ifneq ($(filter t124,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DNV_SUPPORTS_GANGED_MODE
LOCAL_CFLAGS += -DNV_MIPI_PAD_CTRL_EXISTS
endif

ifneq ($(filter t114,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DNV_SUPPORTS_GANGED_MODE
endif

ifneq ($(filter t30,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DNV_MIPI_PAD_CTRL_EXISTS
endif

# t30 has DSI calibration registers in vi
ifneq ($(filter t30,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DNV_MIPI_CSI_CIL
endif

ifeq ($(filter t30,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DNV_SUPPORTS_MULTIPLE_PADS
endif

ifneq ($(filter t30,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DT30_CHIP
endif

LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc/$(TARGET_TEGRA_FAMILY)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera/core/hwinc/$(TARGET_TEGRA_FAMILY)

LOCAL_SRC_FILES += nvddk_disp.c
LOCAL_SRC_FILES += nvddk_disp_hw.c
LOCAL_SRC_FILES += nvddk_disp_edid.c
LOCAL_SRC_FILES += dc_hal.c
LOCAL_SRC_FILES += dc_crt_hal.c
LOCAL_SRC_FILES += dc_dsi_hal.c
LOCAL_SRC_FILES += dc_hdmi_hal.c
LOCAL_SRC_FILES += dc_sd3.c

LOCAL_SHARED_LIBRARIES += libnvodm_query

LOCAL_CFLAGS += -Wno-error=missing-field-initializers -Wno-error=enum-compare
LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1

include $(NVIDIA_STATIC_LIBRARY)
