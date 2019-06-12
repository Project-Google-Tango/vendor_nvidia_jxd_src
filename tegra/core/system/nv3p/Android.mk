#
# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

_avp_local_src_files := nv3p.c
_avp_local_src_files += nv3p_transport_device.c
_avp_local_src_files += nv3p_transport_usb_descriptors.c
_avp_local_src_files += nvml_usbf_t1xx.c
_avp_local_src_files += nvboot_misc_t1xx.c

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnv3p
LOCAL_NVIDIA_NO_COVERAGE := true

LOCAL_CFLAGS += -DNVODM_BOARD_IS_FPGA=0
LOCAL_CFLAGS += -DENABLE_T30
LOCAL_CFLAGS += -DENABLE_T1XX
ifneq ($(filter  t148,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DENABLE_T148
endif
ifneq ($(filter  t30,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DENABLE_TXX
endif
ifneq ($(filter  t124,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DENABLE_T124
endif
ifeq ($(TARGET_USE_NVDUMPER),true)
LOCAL_CFLAGS += -DENABLE_NVDUMPER=1
else
LOCAL_CFLAGS += -DENABLE_NVDUMPER=0
endif

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot/$(TARGET_TEGRA_VERSION)

LOCAL_SRC_FILES += $(_avp_local_src_files)
LOCAL_SRC_FILES += nvml_usbf_t30.c
LOCAL_SRC_FILES += nvboot_misc_t30.c

LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
include $(NVIDIA_STATIC_LIBRARY)


_avp_local_cflags := -Os -ggdb0
_avp_local_cflags += -DNVBOOT_SI_ONLY=1
_avp_local_cflags += -DNVBOOT_TARGET_FPGA=0
_avp_local_cflags += -DNVBOOT_TARGET_RTL=0

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnv3p_avp

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot/$(TARGET_TEGRA_VERSION)

# Look for string.h in bionic libc include directory, in case we
# are using an Android prebuilt tool chain.  string.h recursively
# includes malloc.h, which would cause a declaration conflict with NvOS,
# so add a define in order to avoid recursive inclusion.
ifeq ($(AVP_EXTERNAL_TOOLCHAIN),)
  LOCAL_C_INCLUDES += $(TOP)/bionic/libc/include
  LOCAL_CFLAGS += -D_MALLOC_H_
endif
LOCAL_CFLAGS += -DENABLE_T30
LOCAL_CFLAGS += -DENABLE_T1XX
ifneq ($(filter  t148,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DENABLE_T148
endif
ifneq ($(filter  t30,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DENABLE_TXX
endif
ifneq ($(filter  t124,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DENABLE_T124
endif
LOCAL_CFLAGS += $(_avp_local_cflags)
LOCAL_CFLAGS += -DNV_IS_AVP=1
ifeq ($(TARGET_USE_NVDUMPER),true)
LOCAL_CFLAGS += -DENABLE_NVDUMPER=1
else
LOCAL_CFLAGS += -DENABLE_NVDUMPER=0
endif

LOCAL_SRC_FILES += $(_avp_local_src_files)
LOCAL_SRC_FILES += nvml_usbf_t30.c
LOCAL_SRC_FILES += nvboot_misc_t30.c

include $(NVIDIA_STATIC_AVP_LIBRARY)


include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnv3p_avp_t11x

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot/t114

ifeq ($(AVP_EXTERNAL_TOOLCHAIN),)
  LOCAL_C_INCLUDES += $(TOP)/bionic/libc/include
  LOCAL_CFLAGS += -D_MALLOC_H_
endif
LOCAL_CFLAGS += -DENABLE_T1XX
LOCAL_CFLAGS += $(_avp_local_cflags)
ifeq ($(TARGET_USE_NVDUMPER),true)
LOCAL_CFLAGS += -DENABLE_NVDUMPER=1
else
LOCAL_CFLAGS += -DENABLE_NVDUMPER=0
endif
LOCAL_SRC_FILES += $(_avp_local_src_files)

include $(NVIDIA_STATIC_AVP_LIBRARY)


include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnv3p_avp_t14x

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot/t148

ifeq ($(AVP_EXTERNAL_TOOLCHAIN),)
  LOCAL_C_INCLUDES += $(TOP)/bionic/libc/include
  LOCAL_CFLAGS += -D_MALLOC_H_
endif
LOCAL_CFLAGS += -DENABLE_T1XX
LOCAL_CFLAGS += -DENABLE_T148
LOCAL_CFLAGS += $(_avp_local_cflags)

LOCAL_SRC_FILES += $(_avp_local_src_files)

include $(NVIDIA_STATIC_AVP_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnv3p_avp_t12x

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot/t124

ifeq ($(AVP_EXTERNAL_TOOLCHAIN),)
  LOCAL_C_INCLUDES += $(TOP)/bionic/libc/include
  LOCAL_CFLAGS += -D_MALLOC_H_
endif
LOCAL_CFLAGS += -DENABLE_T1XX
LOCAL_CFLAGS += -DENABLE_T124
LOCAL_CFLAGS += $(_avp_local_cflags)
ifeq ($(TARGET_USE_NVDUMPER),true)
LOCAL_CFLAGS += -DENABLE_NVDUMPER=1
else
LOCAL_CFLAGS += -DENABLE_NVDUMPER=0
endif
LOCAL_SRC_FILES += $(_avp_local_src_files)

include $(NVIDIA_STATIC_AVP_LIBRARY)

#variable cleanup
_avp_local_cflags :=
_avp_local_src_files :=

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnv3p

LOCAL_CFLAGS += -DNVODM_BOARD_IS_FPGA=0
ifeq ($(TARGET_USE_NVDUMPER),true)
LOCAL_CFLAGS += -DENABLE_NVDUMPER=1
else
LOCAL_CFLAGS += -DENABLE_NVDUMPER=0
endif
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot/$(TARGET_TEGRA_VERSION)

LOCAL_SRC_FILES += nv3p.c
LOCAL_SRC_FILES += nv3p_transport_usb_host.c
LOCAL_SRC_FILES += nv3p_transport_sema_host.c
LOCAL_SRC_FILES += nv3p_transport_host.c

include $(NVIDIA_HOST_STATIC_LIBRARY)
