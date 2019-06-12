#
# Copyright (c) 2011-2013, NVIDIA Corporation.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#

_fuses_local_path := $(call my-dir)

_fuses_local_cflags := -DNVODM_BOARD_IS_SIMULATION=0
_fuses_local_cflags += -Wno-missing-field-initializers
_fuses_local_cflags += -fno-exceptions
_fuses_local_cflags += -Os -ggdb0

_fuses_local_c_includes := $(TEGRA_TOP)/core/include
_fuses_local_c_includes += $(TEGRA_TOP)/hwinc
_fuses_local_c_includes += $(_fuses_local_path)/common
_fuses_local_c_includes += $(_fuses_local_path)/read

LOCAL_PATH := $(_fuses_local_path)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvddk_fuse_read_avp

LOCAL_C_INCLUDES += $(_fuses_local_c_includes)
LOCAL_C_INCLUDES += $(_fuses_local_path)/private/t30
LOCAL_C_INCLUDES += $(_fuses_local_path)/private/t11x
LOCAL_C_INCLUDES += $(_fuses_local_path)/private/t14x
LOCAL_C_INCLUDES += $(_fuses_local_path)/private/t12x

LOCAL_CFLAGS     := $(_fuses_local_cflags)
LOCAL_CFLAGS     += -DENABLE_T30
LOCAL_CFLAGS     += -DENABLE_T114
LOCAL_CFLAGS     += -DENABLE_T148
LOCAL_CFLAGS     += -DENABLE_T124

ifeq ($(BOOT_MINIMAL_BL),1)
LOCAL_CFLAGS += -DBOOT_MINIMAL_BL
endif

LOCAL_SRC_FILES += private/t30/nvddk_fuse_read_priv_t30.c
LOCAL_SRC_FILES += private/t11x/nvddk_fuse_read_priv_t11x.c
LOCAL_SRC_FILES += private/t14x/nvddk_fuse_read_priv_t14x.c
LOCAL_SRC_FILES += private/t12x/nvddk_fuse_read_priv_t12x.c
LOCAL_SRC_FILES += read/nvddk_fuse_read.c
LOCAL_SRC_FILES += common/nvddk_fuse_common.c

include $(NVIDIA_STATIC_AVP_LIBRARY)


include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnvddk_fuse_read_avp_t11x

LOCAL_C_INCLUDES := $(_fuses_local_c_includes)
LOCAL_C_INCLUDES += $(_fuses_local_path)/private/t11x
LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc/t11x

LOCAL_CFLAGS     := $(_fuses_local_cflags)
LOCAL_CFLAGS     += -DENABLE_T114

LOCAL_SRC_FILES := private/t11x/nvddk_fuse_read_priv_t11x.c
LOCAL_SRC_FILES += read/nvddk_fuse_read.c
LOCAL_SRC_FILES += common/nvddk_fuse_common.c

include $(NVIDIA_STATIC_AVP_LIBRARY)


include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnvddk_fuse_read_avp_t14x

LOCAL_C_INCLUDES := $(_fuses_local_c_includes)
LOCAL_C_INCLUDES += $(_fuses_local_path)/private/t14x
LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc/t14x

LOCAL_CFLAGS     := $(_fuses_local_cflags)
LOCAL_CFLAGS     += -DENABLE_T148

LOCAL_SRC_FILES := private/t14x/nvddk_fuse_read_priv_t14x.c
LOCAL_SRC_FILES += read/nvddk_fuse_read.c
LOCAL_SRC_FILES += common/nvddk_fuse_common.c

include $(NVIDIA_STATIC_AVP_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnvddk_fuse_read_avp_t12x

LOCAL_C_INCLUDES := $(_fuses_local_c_includes)
LOCAL_C_INCLUDES += $(_fuses_local_path)/private/t12x
LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc/t12x

LOCAL_CFLAGS     := $(_fuses_local_cflags)
LOCAL_CFLAGS     += -DENABLE_T124

ifeq ($(BOOT_MINIMAL_BL),1)
LOCAL_CFLAGS += -DBOOT_MINIMAL_BL
endif

LOCAL_SRC_FILES := private/t12x/nvddk_fuse_read_priv_t12x.c
LOCAL_SRC_FILES += read/nvddk_fuse_read.c
LOCAL_SRC_FILES += common/nvddk_fuse_common.c

include $(NVIDIA_STATIC_AVP_LIBRARY)

# Write
LOCAL_PATH := $(_fuses_local_path)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvddk_fuse_write_avp

LOCAL_C_INCLUDES += $(_fuses_local_c_includes)
LOCAL_C_INCLUDES += $(_fuses_local_path)/write
LOCAL_C_INCLUDES += $(_fuses_local_path)/private/$(TARGET_TEGRA_FAMILY)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nv3p
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot/$(TARGET_TEGRA_VERSION)

LOCAL_CFLAGS     := $(_fuses_local_cflags)

LOCAL_SRC_FILES := write/nvddk_fuse_write.c
LOCAL_SRC_FILES += private/$(TARGET_TEGRA_FAMILY)/nvddk_fuse_write_priv.c

include $(NVIDIA_STATIC_AVP_LIBRARY)


# variable cleanup
_fuses_local_path :=
_fuses_local_cflags :=
_fuses_local_c_includes :=

# Include host specific library make file
include $(addprefix $(LOCAL_PATH)/, $(addsuffix /Android.mk, \
     read/host \
))
