#
# Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.

LOCAL_PATH := $(call my-dir)
BOOTLOADER_PATHS :=
ifeq ($(BOARD_BUILD_BOOTLOADER),true)
BOOTLOADER_PATHS := fastboot
endif

_nv_core_system_modules :=
_nv_core_tegra_versions := t30 t114 t124 t148

ifneq ($(filter $(_nv_core_tegra_versions),$(TARGET_TEGRA_VERSION)),)
    _nv_core_system_modules += fastboot
    _nv_core_system_modules += nv3p
    _nv_core_system_modules += nv3pserver
    _nv_core_system_modules += nvbct
    _nv_core_system_modules += nvpartmgr
    _nv_core_system_modules += nvstormgr
    _nv_core_system_modules += nvaboot
    _nv_core_system_modules += nvbootupdate
    _nv_core_system_modules += nvcrypto
    _nv_core_system_modules += utils
    _nv_core_system_modules += nvfsmgr
    _nv_core_system_modules += nvfs
    _nv_core_system_modules += nvdioconverter
    _nv_core_system_modules += nvflash
    _nv_core_system_modules += nvbuildbct
    _nv_core_system_modules += nvsku
    _nv_core_system_modules += nvbdktest
    _nv_core_system_modules += nvsecuretool
ifeq ($(TARGET_USE_NCT),true)
    _nv_core_system_modules += nvnct
endif
endif

ifneq ($strip($(_nv_core_system_modules)),)
include $(addprefix $(LOCAL_PATH)/, $(addsuffix /Android.mk, $(_nv_core_system_modules)))
endif

_nv_core_tegra_versions :=
_nv_core_system_modules :=
