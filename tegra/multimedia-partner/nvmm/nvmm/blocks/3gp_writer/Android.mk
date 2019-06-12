LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnv3gpwriter

LOCAL_CFLAGS += -D__OMX_EXPORTS
LOCAL_CFLAGS += -DOMXVERSION=2
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/nvmm/nvmm/include

ifeq ($(NV_LOGGER_ENABLED),1)
LOCAL_CFLAGS += -DNV_LOGGER_ENABLED=1
endif
LOCAL_SRC_FILES += nvmm_3gpwriterblock.c
LOCAL_SRC_FILES += nv_3gp_writer.c
LOCAL_SRC_FILES += nv3gpwritervideo.c
LOCAL_SRC_FILES += nv3gpwriteraudio.c

include $(NVIDIA_STATIC_LIBRARY)
