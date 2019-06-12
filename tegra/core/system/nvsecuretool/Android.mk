###############################################################################
#
# Copyright (c) 2012-2014, NVIDIA CORPORATION.  All Rights Reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#
###############################################################################

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := nvsecuretool

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvflash/lib
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvflash/app
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvbuildbct
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core-private/utils/nvelfutil/include
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)

LOCAL_CFLAGS += -DNVTBOOT_EXISTS

LOCAL_SRC_FILES += nvsecuretool.c
LOCAL_SRC_FILES += nv_rsa_core.c
LOCAL_SRC_FILES += nv_bigintmod.c
LOCAL_SRC_FILES += nv_sha256.c
LOCAL_SRC_FILES += nvsecure_bctupdate.c
LOCAL_SRC_FILES += nvsecuretool_t30.c
LOCAL_SRC_FILES += nvsecuretool_t1xx.c
LOCAL_SRC_FILES += nvsecuretool_pkc.c
LOCAL_SRC_FILES += nvsecuretool_version.c
LOCAL_SRC_FILES += nvsecuretool_t12x.c
LOCAL_SRC_FILES += nvtest.c
LOCAL_SRC_FILES += nv_asn_parser.c

LOCAL_STATIC_LIBRARIES += libnvapputil
LOCAL_STATIC_LIBRARIES += libnvos
LOCAL_STATIC_LIBRARIES += libnvboothost
LOCAL_STATIC_LIBRARIES += libnvaes_ref
LOCAL_STATIC_LIBRARIES += libnvbcthost
LOCAL_STATIC_LIBRARIES += libnvflash
LOCAL_STATIC_LIBRARIES += libnvbuildbct
LOCAL_STATIC_LIBRARIES += libnvelfutil

LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_HEADERS)/nvflash_miniloader_t30.h
LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_HEADERS)/nvflash_miniloader_t11x.h
LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_HEADERS)/nvtboot_t124.h

LOCAL_LDLIBS += -lpthread -ldl

include $(NVIDIA_HOST_EXECUTABLE)
