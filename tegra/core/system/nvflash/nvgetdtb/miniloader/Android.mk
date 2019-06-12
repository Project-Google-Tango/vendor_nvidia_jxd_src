# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

_avp_local_path := $(call my-dir)

_avp_local_cflags := -march=armv4t
_avp_local_cflags += -Os
_avp_local_cflags += -fno-unwind-tables
_avp_local_cflags += -fomit-frame-pointer
_avp_local_cflags += -ffunction-sections
_avp_local_cflags += -fdata-sections
ifeq ($(AVP_EXTERNAL_TOOLCHAIN),)
  _avp_local_cflags += -marm
else
  _avp_local_cflags += -mthumb-interwork
  _avp_local_cflags += -mthumb
endif

_avp_local_c_includes += $(TARGET_OUT_HEADERS)
_avp_local_c_includes += $(TEGRA_TOP)/core/utils/nvos/aos/nvap
_avp_local_c_includes += $(TEGRA_TOP)/microboot/hwinc/$(TARGET_TEGRA_FAMILY)
_avp_local_c_includes += $(TOP)/bionic/libc/arch-arm/include
_avp_local_c_includes += $(TOP)/bionic/libc/include
_avp_local_c_includes += $(TOP)/bionic/libc/kernel/common
_avp_local_c_includes += $(TOP)/bionic/libc/kernel/arch-arm
ifneq ($(filter t30, $(TARGET_TEGRA_VERSION)),)
_avp_local_c_includes += $(TEGRA_TOP)/core/utils/aes_keysched_lock
endif
# Look for string.h in bionic libc include directory, in case we
# are using an Android prebuilt tool chain.  string.h recursively
# includes malloc.h, which would cause a declaration conflict with NvOS,
# so add a define in order to avoid recursive inclusion.
ifeq ($(AVP_EXTERNAL_TOOLCHAIN),)
  _avp_local_c_includes += $(TOP)/bionic/libc/include
  _avp_local_cflags += -D_MALLOC_H_
endif

# define miniloader common library to avoid ../ parent paths
LOCAL_PATH := $(_avp_local_path)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := nvgetdtb_miniloader_$(TARGET_TEGRA_FAMILY)


LOCAL_C_INCLUDES += $(_avp_local_c_includes)
LOCAL_CFLAGS     := $(_avp_local_cflags)

LOCAL_NO_DEFAULT_COMPILER_FLAGS := true
LOCAL_MODULE_CLASS := EXECUTABLES

LOCAL_SRC_FILES :=
LOCAL_SRC_FILES += nvgetdtb_platform.c
LOCAL_SRC_FILES += nvgetdtb_server.c

LOCAL_STATIC_LIBRARIES += libnv3p_avp
LOCAL_STATIC_LIBRARIES += libavpmain_avp
ifneq ($(HOST_OS),darwin)
LOCAL_STATIC_LIBRARIES += libnvodm_query_avp
endif
LOCAL_STATIC_LIBRARIES += libnvddk_i2c
ifeq ($(NV_TARGET_BOOTLOADER_PINMUX),kernel)
LOCAL_STATIC_LIBRARIES += libnvpinmux_avp
endif
ifneq ($(filter t30, $(TARGET_TEGRA_VERSION)),)
LOCAL_STATIC_LIBRARIES += libnvseaes_keysched_lock_avp
endif
# Potentially override the default compiler.  Froyo arm-eabi-4.4.0 toolchain
# does not produce valid code for ARMv4T ARM/Thumb interworking.
ifneq ($(AVP_EXTERNAL_TOOLCHAIN),)
  LOCAL_CC := $(AVP_EXTERNAL_TOOLCHAIN)gcc
endif

LOCAL_NVIDIA_LINK_SCRIPT_PATH := $(LOCAL_PATH)
MAP_FILE := $(call local-intermediates-dir)/$(LOCAL_MODULE).map

include $(NVIDIA_STATIC_AVP_EXECUTABLE)

# Preserve value of LOCAL_BUILT_MODULE path for generate header from it
_avp_local_built_module := $(LOCAL_BUILT_MODULE)

# miniloader header
LOCAL_PATH := $(_avp_local_path)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := nvgetdtb_miniloader.h

LOCAL_SRC_FILES := $(_avp_local_built_module)

include $(NVIDIA_GENERATED_HEADER)

# variable cleanup
_avp_local_built_module :=
_avp_local_c_includes   :=
_avp_local_cflags       :=
_avp_local_path         :=
