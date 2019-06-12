# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

# Grab name of the makefile to depend on it
ifneq ($(PREV_LOCAL_PATH),$(LOCAL_PATH))
NVIDIA_MAKEFILE := $(lastword $(filter-out $(lastword $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))
PREV_LOCAL_PATH := $(LOCAL_PATH)
endif
include $(CLEAR_VARS)

# Build variables common to all nvidia modules

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc
LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc/$(TARGET_TEGRA_FAMILY)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera/core/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia/codecs/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia/tvmr/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/utils/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/nvmm/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera-partner/imager/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera/core/hwinc/$(TARGET_TEGRA_FAMILY)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera/core/hwinc
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera/core/camera
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core-private/include

LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia/codecs/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia/tvmr/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/utils/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/nvmm/include
ifneq (,$(findstring core-private,$(LOCAL_PATH)))
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core-private/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc-private
LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc-private/$(TARGET_TEGRA_FAMILY)
endif

ifneq (,$(findstring tests,$(LOCAL_PATH)))
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core-private/include
endif

-include $(NVIDIA_UBM_DEFAULTS)

TEGRA_CFLAGS :=

# Following line has been added to prevent redefinition of NV_DEBUG
LOCAL_CFLAGS += -UNV_DEBUG
# NOTE: this conditional needs to be kept in sync with the one in base.mk!
ifeq ($(TARGET_BUILD_TYPE),debug)
LOCAL_CFLAGS += -DNV_DEBUG=1
# TODO: fix source that relies on these
LOCAL_CFLAGS += -DDEBUG
LOCAL_CFLAGS += -D_DEBUG
# disable all optimizations and enable gdb debugging extensions
LOCAL_CFLAGS += -O0 -ggdb
else
LOCAL_CFLAGS += -DNV_DEBUG=0
endif

LOCAL_CFLAGS += -DNV_IS_AVP=0
LOCAL_CFLAGS += -DNV_BUILD_STUBS=1
TEGRA_CFLAGS += -DCONFIG_PLLP_BASE_AS_408MHZ=1


# Define Trusted Foundations
ifneq ($(filter tf y,$(SECURE_OS_BUILD)),)
ifeq ($(TARGET_TEGRA_VERSION), t114)
TEGRA_CFLAGS += -DCONFIG_TRUSTED_FOUNDATIONS
else
TEGRA_CFLAGS += -DCONFIG_NONTZ_BL
endif
ifeq (,$(findstring tf.enable=y,$(ADDITIONAL_BUILD_PROPERTIES)))
ADDITIONAL_BUILD_PROPERTIES += tf.enable=y
endif
endif
ifeq ($(SECURE_OS_BUILD),tlk)
TEGRA_CFLAGS += -DCONFIG_NONTZ_BL
endif

ifeq ($(NV_EMBEDDED_BUILD),1)
TEGRA_CFLAGS += -DNV_EMBEDDED_BUILD
endif

ifdef PLATFORM_IS_JELLYBEAN
LOCAL_CFLAGS += -DPLATFORM_IS_JELLYBEAN=1
endif
ifdef PLATFORM_IS_JELLYBEAN_MR1
LOCAL_CFLAGS += -DPLATFORM_IS_JELLYBEAN_MR1=1
endif
ifdef PLATFORM_IS_JELLYBEAN_MR2
LOCAL_CFLAGS += -DPLATFORM_IS_JELLYBEAN_MR2=1
endif
ifdef PLATFORM_IS_KITKAT
LOCAL_CFLAGS += -DPLATFORM_IS_KITKAT=1
endif

ifdef PLATFORM_IS_GTV_HC
LOCAL_CFLAGS += -DPLATFORM_IS_GTV_HC=1
endif

#########################################################
#                  T30 Macros
#########################################################
ifneq ($(filter tf y,$(SECURE_OS_BUILD)),)
# Disallow all profiling and debug on both Android and Secure OS
TEGRA_CFLAGS += -DS_PROF_OFF_DBG_OFF_NS_PROF_OFF_DBG_OFF=0
# Only profiling on Android
TEGRA_CFLAGS += -DS_PROF_OFF_DBG_OFF_NS_PROF_ON_DBG_OFF=0
# Full profiling and debug on Android
TEGRA_CFLAGS += -DS_PROF_OFF_DBG_OFF_NS_PROF_ON_DBG_ON=1
# Full profiling and debug on both Android and Secure OS
TEGRA_CFLAGS += -DS_PROF_ON_DBG_ON_NS_PROF_ON_DBG_ON=0
else ifeq ($(SECURE_OS_BUILD),tlk)
# Disallow all profiling and debug on both Android and Secure OS
TEGRA_CFLAGS += -DS_PROF_OFF_DBG_OFF_NS_PROF_OFF_DBG_OFF=0
# Only profiling on Android
TEGRA_CFLAGS += -DS_PROF_OFF_DBG_OFF_NS_PROF_ON_DBG_OFF=0
# Full profiling and debug on Android
TEGRA_CFLAGS += -DS_PROF_OFF_DBG_OFF_NS_PROF_ON_DBG_ON=1
# Full profiling and debug on both Android and Secure OS
TEGRA_CFLAGS += -DS_PROF_ON_DBG_ON_NS_PROF_ON_DBG_ON=0
else
# Only profiling on Android
TEGRA_CFLAGS += -DNS_PROF_ON_DBG_OFF=0
# Full profiling and debug on Android
TEGRA_CFLAGS += -DNS_PROF_ON_DBG_ON=1
endif

#########################################################
#                  T114 Macros
#########################################################
ifneq ($(filter tf y,$(SECURE_OS_BUILD)),)
# Secure Profiling (ARM - Secure Priviledged Non-Invasive Debug)
TEGRA_CFLAGS += -DSECURE_PROF=0
# Secure Debugging (ARM - Secure Priviledged Invasive Debug Enable)
TEGRA_CFLAGS += -DSECURE_DEBUG=0
else ifeq ($(SECURE_OS_BUILD),tlk)
# Secure Profiling (ARM - Secure Priviledged Non-Invasive Debug)
TEGRA_CFLAGS += -DSECURE_PROF=0
# Secure Debugging (ARM - Secure Priviledged Invasive Debug Enable)
TEGRA_CFLAGS += -DSECURE_DEBUG=0
endif
# Non-Secure Profiling (ARM - Non-Invasive Debug Enable)
TEGRA_CFLAGS += -DNON_SECURE_PROF=0


LOCAL_CFLAGS += $(TEGRA_CFLAGS)
LOCAL_ASFLAGS += $(TEGRA_CFLAGS)

ifneq (,$(findstring _sim, $(TARGET_PRODUCT)))
LOCAL_CFLAGS += -DBUILD_FOR_COSIM
endif

LOCAL_MODULE_TAGS := optional

# clear nvidia local variables to defaults
NVIDIA_CLEARED := true
LOCAL_NVIDIA_CGOPTS :=
LOCAL_NVIDIA_CGFRAGOPTS :=
LOCAL_NVIDIA_CGVERTOPTS :=
LOCAL_NVIDIA_INTERMEDIATES_DIR :=
LOCAL_NVIDIA_SHADERS :=
LOCAL_NVIDIA_GEN_SHADERS :=
LOCAL_NVIDIA_PKG :=
LOCAL_NVIDIA_EXPORTS :=
LOCAL_NVIDIA_NO_COVERAGE :=
LOCAL_NVIDIA_NULL_COVERAGE :=
LOCAL_NVIDIA_NO_EXTRA_WARNINGS :=
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS :=
LOCAL_NVIDIA_RM_WARNING_FLAGS :=
LOCAL_NVIDIA_NVMAKE_ARGS :=
LOCAL_NVIDIA_NVMAKE_BUILD_DIR :=
LOCAL_NVIDIA_NVMAKE_OVERRIDE_BUILD_TYPE :=
LOCAL_NVIDIA_NVMAKE_OVERRIDE_MODULE_NAME :=
LOCAL_NVIDIA_NVMAKE_OVERRIDE_TOP :=
LOCAL_NVIDIA_NVMAKE_OVERRIDE_TARGET_ABI :=
LOCAL_NVIDIA_NVMAKE_OVERRIDE_TARGET_OS :=
LOCAL_NVIDIA_NVMAKE_OVERRIDE_MODULE_PRIVATE_PATH :=
LOCAL_NVIDIA_NVMAKE_TREE :=
LOCAL_NVIDIA_LINK_SCRIPT_PATH :=
LOCAL_NVIDIA_LINK_SCRIPT :=
LOCAL_NVIDIA_OBJCOPY_FLAGS :=
LOCAL_NVIDIA_OVERRIDE_HOST_DEBUG_FLAGS :=
LOCAL_NVIDIA_RAW_EXECUTABLE_LDFLAGS :=
LOCAL_NVIDIA_USE_PUBLIC_KEY_HEADER :=

# FIXME: GTV's toolchain generates a lot of warnings for now
ifdef PLATFORM_IS_GTV_HC
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1
endif
