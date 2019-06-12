#
# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH := $(call my-dir)

_local_cflags := -DNVODM_BOARD_IS_FPGA=0
_local_cflags += -DNVODM_ENABLE_SIMULATION_CODE=1
_local_cflags += -DNVTBOOT_EXISTS

_local_includes := $(TEGRA_TOP)/core/system/nvflash/lib
_local_includes += $(TEGRA_TOP)/core/drivers/nvboot/include
_local_includes += $(TEGRA_TOP)/core/drivers/nvboot/t30/include
_local_includes += $(TEGRA_TOP)/core/drivers/nvboot/t114/include
_local_includes += $(TEGRA_TOP)/core/drivers/nvboot/t124/include
_local_includes += $(TEGRA_TOP)/core/system/nvdioconverter
_local_includes += $(TEGRA_TOP)/core/system/nvbuildbct
_local_includes += $(TEGRA_TOP)/core/utils/nvos/aos/nvap
_local_includes += $(TARGET_OUT_HEADERS)

_local_src_files := nvflash_app.c
_local_src_files += nvflash_util.c
_local_src_files += nvflash_usage.c
_local_src_files += nvflash_util_t30.c
_local_src_files += nvflash_util_t11x.c
_local_src_files += nvflash_util_t12x.c
_local_src_files += nvflash_hostblockdev.c
_local_src_files += nvflash_app_version.c
_local_src_files += nvflash_fuse_bypass.c
ifeq ($(NV_EMBEDDED_BUILD),1)
_local_src_files += nvimager.c
endif

_local_static_libraries := libnvapputil
_local_static_libraries += libnv3p
_local_static_libraries += libnv3pserverhost
_local_static_libraries += libnvddk_fuse_read_host
_local_static_libraries += libnvbcthost
_local_static_libraries += libnvpartmgrhost
_local_static_libraries += libnvbootupdatehost
_local_static_libraries += libnvsystem_utilshost
_local_static_libraries += libnvflash
_local_static_libraries += libnvdioconverter
_local_static_libraries += libnvtestresults
_local_static_libraries += libnvboothost
_local_static_libraries += libnvusbhost
_local_static_libraries += libnvos
_local_static_libraries += libnvaes_ref
_local_static_libraries += libnvswcrypto
_local_static_libraries += libnvbuildbct
ifeq ($(NV_EMBEDDED_BUILD),1)
_local_static_libraries += libnvskuhost
endif

##########################################################################
#                        Makefile for nvflash                            #
##########################################################################
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := nvflash

LOCAL_CFLAGS += $(_local_cflags)

ifeq ($(TARGET_USE_NVDUMPER), true)
LOCAL_CFLAGS += -DENABLE_NVDUMPER=1
else
LOCAL_CFLAGS += -DENABLE_NVDUMPER=0
endif

LOCAL_C_INCLUDES += $(_local_includes)

LOCAL_SRC_FILES += $(_local_src_files)

LOCAL_STATIC_LIBRARIES += $(_local_static_libraries)

LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_HEADERS)/nvflash_miniloader_t30.h
LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_HEADERS)/nvflash_miniloader_t11x.h
LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_HEADERS)/nvflash_miniloader_t12x.h
LOCAL_ADDITIONAL_DEPENDENCIES += $(TARGET_OUT_HEADERS)/nvtboot_t124.h

LOCAL_LDLIBS += -lpthread -ldl

ifeq ($(HOST_OS),darwin)
LOCAL_LDLIBS += -lpthread -framework CoreFoundation -framework IOKit \
	-framework Carbon
endif

include $(NVIDIA_HOST_EXECUTABLE)

##########################################################################
#                        Makefile for nvgetdtb                           #
##########################################################################
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := nvgetdtb

LOCAL_CFLAGS += $(_local_cflags)
LOCAL_CFLAGS += -DNVHOST_QUERY_DTB

ifeq ($(TARGET_USE_NVDUMPER), true)
LOCAL_CFLAGS += -DENABLE_NVDUMPER=1
else
LOCAL_CFLAGS += -DENABLE_NVDUMPER=0
endif

LOCAL_C_INCLUDES += $(_local_includes)

LOCAL_SRC_FILES += $(_local_src_files)

LOCAL_STATIC_LIBRARIES += $(_local_static_libraries)
LOCAL_STATIC_LIBRARIES += libnvgetdtb

LOCAL_ADDITIONAL_DEPENDENCIES := $(TARGET_OUT_HEADERS)/nvgetdtb_miniloader.h

LOCAL_LDLIBS += -lpthread -ldl

ifeq ($(HOST_OS),darwin)
LOCAL_LDLIBS += -lpthread -framework CoreFoundation -framework IOKit \
	-framework Carbon
endif

include $(NVIDIA_HOST_EXECUTABLE)

#clean variables
_local_cflags :=
_local_includes :=
_local_src_files :=
_local_static_libraries :=
_local_additional_dependencies :=
