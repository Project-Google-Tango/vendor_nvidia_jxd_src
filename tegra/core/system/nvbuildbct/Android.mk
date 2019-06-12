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

LOCAL_MODULE := libnvbuildbct

LOCAL_MODULE_TAGS := eng

LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc

LOCAL_CFLAGS += -DNVODM_BOARD_IS_FPGA=0
LOCAL_CFLAGS += -DT114_BUILDBCT_NAND_SUPPORT=1

LOCAL_SRC_FILES += t30/t30_parse.c
LOCAL_SRC_FILES += t30/t30_set.c
LOCAL_SRC_FILES += t30/t30_data_layout.c
LOCAL_SRC_FILES += t30/t30_buildbctinterface.c

LOCAL_SRC_FILES += t11x/t11x_parse.c
LOCAL_SRC_FILES += t11x/t11x_set.c
LOCAL_SRC_FILES += t11x/t11x_data_layout.c
LOCAL_SRC_FILES += t11x/t11x_buildbctinterface.c

LOCAL_SRC_FILES += t12x/t12x_parse.c
LOCAL_SRC_FILES += t12x/t12x_set.c
LOCAL_SRC_FILES += t12x/t12x_data_layout.c
LOCAL_SRC_FILES += t12x/t12x_buildbctinterface.c

LOCAL_SRC_FILES += nvbuildbct_util.c
LOCAL_SRC_FILES += nvbuildbct.c

LOCAL_LDLIBS += -lpthread -ldl

include $(NVIDIA_HOST_STATIC_LIBRARY)
