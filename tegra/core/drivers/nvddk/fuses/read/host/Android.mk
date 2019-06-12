LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvddk_fuse_read_host

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers/nvddk/fuses/read
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvflash/app

LOCAL_SRC_FILES += nvddk_fuse_read.c

LOCAL_CFLAGS += -DNVODM_BOARD_IS_SIMULATION=1
LOCAL_CFLAGS += -DNVODM_BOARD_IS_FPGA=0
LOCAL_CFLAGS += -Wno-missing-field-initializers
LOCAL_CFLAGS += -fno-exceptions
include $(NVIDIA_HOST_STATIC_LIBRARY)
