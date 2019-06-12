#
# Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#
#TODO: This makefile needs refactoring, putting this tag to track
#_BROKEN_MAKEFILE

_avp_local_path := $(call my-dir)

# We need to generate ARMv4T code, not Thumb code and Thumb is the default.
# Some expressions and instructions we use are not compatible with Thumb.
_avp_local_arm_mode := arm

_avp_local_cflags := -DNV_IS_AVP=1
_avp_local_cflags += -DNVAOS_SHELL=1

ifeq ($(HOST_OS),darwin)
_avp_local_cflags += -DNVML_BYPASS_PRINT=1
endif

ifneq ($(filter bonaire_sim ,$(TARGET_PRODUCT)),)
_avp_local_cflags += -DBUILD_FOR_COSIM
endif

_avp_local_c_includes := $(TEGRA_TOP)/core/utils/nvos/include
_avp_local_c_includes += $(TEGRA_TOP)/core/utils/nvos
_avp_local_c_includes += $(TEGRA_TOP)/core/utils/nvos/aos
_avp_local_c_includes += $(TEGRA_TOP)/core/drivers/include
_avp_local_c_includes += $(TEGRA_TOP)/core/drivers/nvrm/nvrmkernel/core
_avp_local_c_includes += $(TEGRA_TOP)/core-private/include
_avp_local_c_includes += $(TEGRA_TOP)/hwinc-private
_avp_local_c_includes += $(TEGRA_TOP)/hwinc-private/$(TARGET_TEGRA_FAMILY)
_avp_local_c_includes += $(TEGRA_TOP)/hwinc/$(TARGET_TEGRA_FAMILY)
_avp_local_c_includes += $(TOP)/bionic/libc/arch-arm/include
_avp_local_c_includes += $(TOP)/bionic/libc/include
_avp_local_c_includes += $(TOP)/bionic/libc/kernel/common
_avp_local_c_includes += $(TOP)/bionic/libc/kernel/arch-arm

# Do not change LOCAL_PATH till we include NVIDIA_DEFAULTS, NVIDIA_DEFAULTS
# depends on correct value of LOCAL_PATH
LOCAL_PATH := $(_avp_local_path)

include $(NVIDIA_DEFAULTS)

# We are not allowed to use ../ in LOCAL_SRC_FILES
LOCAL_PATH := $(_avp_local_path)/..

LOCAL_MODULE := libavpmain_avp_aos

LOCAL_ARM_MODE   := $(_avp_local_arm_mode)
LOCAL_C_INCLUDES += $(_avp_local_c_includes)
LOCAL_CFLAGS     := $(_avp_local_cflags)
LOCAL_CFLAGS     += $(TEGRA_CFLAGS)

LOCAL_SRC_FILES += aos_print.c
LOCAL_SRC_FILES += aos_process_args.c
LOCAL_SRC_FILES += aos_profile.c
LOCAL_SRC_FILES += aos_semi_rvice.c
LOCAL_SRC_FILES += aos_semi_uart.c
LOCAL_SRC_FILES += dlmalloc.c
LOCAL_SRC_FILES += nvos_aos.c
LOCAL_SRC_FILES += nvos_aos_core.c
LOCAL_SRC_FILES += nvos_aos_libgcc.c
LOCAL_SRC_FILES += nvos_aos_semi.c
LOCAL_SRC_FILES += nvap/aos_avp.c
LOCAL_SRC_FILES += nvap/aos_avp_cache_t30.c
LOCAL_SRC_FILES += nvap/aos_avp_t30.c
LOCAL_SRC_FILES += nvap/avp_override.S
LOCAL_SRC_FILES += nvap/init_avp.c
LOCAL_SRC_FILES += nvap/nvos_aos_gcc_avp.c
LOCAL_SRC_FILES += nvap/nvos_aos_libc.c
LOCAL_SRC_FILES += nvap/aos_avp_board_info.c
LOCAL_SRC_FILES += nvap/aos_avp_pmu.c

include $(NVIDIA_STATIC_AVP_LIBRARY)

# Do not change LOCAL_PATH till we include NVIDIA_DEFAULTS, NVIDIA_DEFAULTS
# depends on correct value of LOCAL_PATH
LOCAL_PATH := $(_avp_local_path)

include $(NVIDIA_DEFAULTS)

# We are not allowed to use ../ in LOCAL_SRC_FILES
LOCAL_PATH := $(_avp_local_path)/../..

LOCAL_MODULE := libavpmain_avp_nvos

LOCAL_ARM_MODE   := $(_avp_local_arm_mode)
LOCAL_C_INCLUDES += $(_avp_local_c_includes)
LOCAL_CFLAGS     := $(_avp_local_cflags)
LOCAL_CFLAGS     += $(TEGRA_CFLAGS)

LOCAL_SRC_FILES += nvos_alloc.c
LOCAL_SRC_FILES += nvos_debug.c
LOCAL_SRC_FILES += nvos_file.c
LOCAL_SRC_FILES += nvos_pointer_hash.c
LOCAL_SRC_FILES += nvos_thread_no_coop.c
LOCAL_SRC_FILES += nvos_trace.c

include $(NVIDIA_STATIC_AVP_LIBRARY)

# Do not change LOCAL_PATH till we include NVIDIA_DEFAULTS, NVIDIA_DEFAULTS
# depends on correct value of LOCAL_PATH
LOCAL_PATH := $(_avp_local_path)

include $(NVIDIA_DEFAULTS)

# We are not allowed to use ../ in LOCAL_SRC_FILES
LOCAL_PATH := $(_avp_local_path)/../../../nvosutils

LOCAL_MODULE := libavpmain_avp_nvosutils

LOCAL_ARM_MODE   := $(_avp_local_arm_mode)
LOCAL_C_INCLUDES += $(_avp_local_c_includes)
LOCAL_CFLAGS     := $(_avp_local_cflags)
LOCAL_CFLAGS     += $(TEGRA_CFLAGS)

LOCAL_SRC_FILES += nvustring.c
LOCAL_SRC_FILES += nvuhash.c

include $(NVIDIA_STATIC_AVP_LIBRARY)

LOCAL_PATH := $(_avp_local_path)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libavpmain_avp

LOCAL_ARM_MODE   := $(_avp_local_arm_mode)
LOCAL_C_INCLUDES += $(_avp_local_c_includes)
LOCAL_CFLAGS     := $(_avp_local_cflags)
LOCAL_CFLAGS     += $(TEGRA_CFLAGS)

LOCAL_SRC_FILES += aos_avpdebugsemi_stub.c
LOCAL_WHOLE_STATIC_LIBRARIES += libavpmain_avp_aos
LOCAL_WHOLE_STATIC_LIBRARIES += libavpmain_avp_nvos
LOCAL_WHOLE_STATIC_LIBRARIES += libavpmain_avp_nvosutils

include $(NVIDIA_STATIC_AVP_LIBRARY)

# variable cleanup
_avp_local_arm_mode   :=
_avp_local_c_includes :=
_avp_local_cflags     :=
_avp_local_path       :=
