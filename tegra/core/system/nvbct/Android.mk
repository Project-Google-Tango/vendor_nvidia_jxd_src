#
# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

_avp_local_cflags += -DNVODM_BOARD_IS_SIMULATION=0

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvbct

LOCAL_CFLAGS += $(_avp_local_cflags)
LOCAL_CFLAGS += -DENABLE_T30
LOCAL_CFLAGS += -DENABLE_T114
LOCAL_CFLAGS += -DENABLE_T148
LOCAL_CFLAGS += -DENABLE_T124
ifeq ($(TARGET_USE_NVDUMPER),true)
LOCAL_CFLAGS += -DENABLE_NVDUMPER=1
else
LOCAL_CFLAGS += -DENABLE_NVDUMPER=0
endif

LOCAL_SRC_FILES += nvbct_private_t30.c
LOCAL_SRC_FILES += nvbit_private_t30.c
LOCAL_SRC_FILES += nvbct.c
LOCAL_SRC_FILES += nvbit.c
LOCAL_SRC_FILES += nvbct_private_t11x.c
LOCAL_SRC_FILES += nvbit_private_t11x.c
LOCAL_SRC_FILES += nvbct_private_t12x.c
LOCAL_SRC_FILES += nvbit_private_t12x.c
LOCAL_SRC_FILES += nvbct_private_t14x.c
LOCAL_SRC_FILES += nvbit_private_t14x.c

LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
include $(NVIDIA_STATIC_LIBRARY)


include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnvbct_avp
LOCAL_CFLAGS += $(_avp_local_cflags)
LOCAL_CFLAGS += -Os -ggdb0
LOCAL_CFLAGS += -DENABLE_T30
LOCAL_CFLAGS += -DENABLE_T114
LOCAL_CFLAGS += -DENABLE_T148
LOCAL_CFLAGS += -DENABLE_T124
ifeq ($(TARGET_USE_NVDUMPER),true)
LOCAL_CFLAGS += -DENABLE_NVDUMPER=1
else
LOCAL_CFLAGS += -DENABLE_NVDUMPER=0
endif
LOCAL_SRC_FILES += nvbct_private_t30.c
LOCAL_SRC_FILES += nvbit_private_t30.c
LOCAL_SRC_FILES += nvbct.c
LOCAL_SRC_FILES += nvbit.c
LOCAL_SRC_FILES += nvbct_private_t11x.c
LOCAL_SRC_FILES += nvbit_private_t11x.c
LOCAL_SRC_FILES += nvbct_private_t12x.c
LOCAL_SRC_FILES += nvbit_private_t12x.c
LOCAL_SRC_FILES += nvbct_private_t14x.c
LOCAL_SRC_FILES += nvbit_private_t14x.c

include $(NVIDIA_STATIC_AVP_LIBRARY)


include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnvbct_avp_t11x

LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc/t11x
LOCAL_CFLAGS += $(_avp_local_cflags)
LOCAL_CFLAGS += -Os -ggdb0
LOCAL_CFLAGS += -DENABLE_T114
LOCAL_SRC_FILES += nvbct_private_t11x.c
LOCAL_SRC_FILES += nvbit_private_t11x.c
LOCAL_SRC_FILES += nvbct.c
LOCAL_SRC_FILES += nvbit.c

include $(NVIDIA_STATIC_AVP_LIBRARY)


include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnvbct_avp_t14x

LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc/t14x
LOCAL_CFLAGS += $(_avp_local_cflags)
LOCAL_CFLAGS += -Os -ggdb0
LOCAL_CFLAGS += -DENABLE_T148
LOCAL_SRC_FILES += nvbct_private_t14x.c
LOCAL_SRC_FILES += nvbit_private_t14x.c
LOCAL_SRC_FILES += nvbct.c
LOCAL_SRC_FILES += nvbit.c

include $(NVIDIA_STATIC_AVP_LIBRARY)


include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnvbct_avp_t12x

LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc/t12x
LOCAL_CFLAGS += $(_avp_local_cflags)
LOCAL_CFLAGS += -Os -ggdb0
LOCAL_CFLAGS += -DENABLE_T124
ifeq ($(TARGET_USE_NVDUMPER),true)
LOCAL_CFLAGS += -DENABLE_NVDUMPER=1
else
LOCAL_CFLAGS += -DENABLE_NVDUMPER=0
endif
LOCAL_SRC_FILES += nvbct_private_t12x.c
LOCAL_SRC_FILES += nvbit_private_t12x.c
LOCAL_SRC_FILES += nvbct.c
LOCAL_SRC_FILES += nvbit.c

include $(NVIDIA_STATIC_AVP_LIBRARY)


include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnvbcthost

LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)
LOCAL_CFLAGS += -DNVODM_BOARD_IS_SIMULATION=1
LOCAL_CFLAGS += -DENABLE_T30
LOCAL_CFLAGS += -DENABLE_T114
LOCAL_CFLAGS += -DENABLE_T148
LOCAL_CFLAGS += -DENABLE_T124
ifeq ($(TARGET_USE_NVDUMPER),true)
LOCAL_CFLAGS += -DENABLE_NVDUMPER=1
else
LOCAL_CFLAGS += -DENABLE_NVDUMPER=0
endif
LOCAL_SRC_FILES += nvbct_private_t30.c
LOCAL_SRC_FILES += nvbit_private_t30.c
LOCAL_SRC_FILES += nvbct.c
LOCAL_SRC_FILES += nvbit.c
LOCAL_SRC_FILES += nvbct_private_t11x.c
LOCAL_SRC_FILES += nvbit_private_t11x.c
LOCAL_SRC_FILES += nvbct_private_t12x.c
LOCAL_SRC_FILES += nvbit_private_t12x.c
LOCAL_SRC_FILES += nvbct_private_t14x.c
LOCAL_SRC_FILES += nvbit_private_t14x.c
include $(NVIDIA_HOST_STATIC_LIBRARY)

_avp_local_cflags :=
