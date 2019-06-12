#
# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#
LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvtestio
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include

LOCAL_SRC_FILES += tio_vfs.c
LOCAL_SRC_FILES += tio_remote.c
LOCAL_SRC_FILES += tio_linux.c
LOCAL_SRC_FILES += tio_linux_poll.c
LOCAL_SRC_FILES += tio_tcp.c
LOCAL_SRC_FILES += tio_null_usb.c
LOCAL_SRC_FILES += tio_host.c
LOCAL_SRC_FILES += tio_host_stdio.c
LOCAL_SRC_FILES += tio_listen.c
LOCAL_SRC_FILES += tio_target.c
LOCAL_SRC_FILES += tio_reliable.c
LOCAL_SRC_FILES += tio_gdbt.c
LOCAL_SRC_FILES += tio_gdbt_host.c
LOCAL_SRC_FILES += tio_gdbt_target.c
LOCAL_SRC_FILES += tio_shmoo.c
LOCAL_SRC_FILES += tio_shmoo_norm.c
LOCAL_SRC_FILES += tio_nvos.c
LOCAL_SRC_FILES += tio_null_file_cache.c

LOCAL_SHARED_LIBRARIES += libnvapputil
LOCAL_SHARED_LIBRARIES += libnvos

LOCAL_CFLAGS += -Wno-missing-field-initializers
LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvtestio
LOCAL_MODULE_TAGS := eng
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvboot
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include

LOCAL_SRC_FILES += tio_vfs.c
LOCAL_SRC_FILES += tio_remote.c
LOCAL_SRC_FILES += tio_linux.c
LOCAL_SRC_FILES += tio_linux_poll.c
LOCAL_SRC_FILES += tio_tcp.c
LOCAL_SRC_FILES += tio_linux_usb.c
LOCAL_SRC_FILES += tio_host.c
LOCAL_SRC_FILES += tio_host_stdio.c
LOCAL_SRC_FILES += tio_listen.c
LOCAL_SRC_FILES += tio_target.c
LOCAL_SRC_FILES += tio_reliable.c
LOCAL_SRC_FILES += tio_gdbt.c
LOCAL_SRC_FILES += tio_gdbt_host.c
LOCAL_SRC_FILES += tio_gdbt_target.c
LOCAL_SRC_FILES += tio_shmoo.c
LOCAL_SRC_FILES += tio_shmoo_norm.c
LOCAL_SRC_FILES += tio_nvos.c
LOCAL_SRC_FILES += tio_null_file_cache.c

include $(NVIDIA_HOST_STATIC_LIBRARY)
