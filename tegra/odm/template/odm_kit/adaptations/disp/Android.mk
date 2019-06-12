#
# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#
LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvodm_disp

LOCAL_C_INCLUDES += $(TEGRA_TOP)/bootloader/nvbootloader/odm-partner/template/odm_kit/adaptations/disp
LOCAL_SRC_FILES += display_hal.c
LOCAL_SRC_FILES += panel_null.c
LOCAL_SRC_FILES += amoled_amaf002.c
LOCAL_SRC_FILES += panel_sharp_wvga.c
LOCAL_SRC_FILES += bl_pcf50626.c
LOCAL_SRC_FILES += bl_ap20.c
LOCAL_SRC_FILES += panel_samsungsdi.c
LOCAL_SRC_FILES += panel_tpo_wvga.c
LOCAL_SRC_FILES += panel_sharp_dsi.c
LOCAL_SRC_FILES += panel_lvds_wsga.c
LOCAL_SRC_FILES += panel_sharp_wvgap.c
LOCAL_SRC_FILES += panel_hdmi.c
LOCAL_STATIC_LIBRARIES += libnvodm_services
LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_SHARED_LIBRARIES += libnvodm_query
LOCAL_SHARED_LIBRARIES += libnvrm

#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
LOCAL_CFLAGS += -Wno-missing-field-initializers
include $(NVIDIA_STATIC_LIBRARY)
