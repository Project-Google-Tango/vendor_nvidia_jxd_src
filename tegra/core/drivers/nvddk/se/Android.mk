LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvddk_se

LOCAL_NVIDIA_NO_COVERAGE := true
LOCAL_CFLAGS += -DNV_IS_AVP=0

LOCAL_SRC_FILES += nvddk_se_blockdev.c
LOCAL_SRC_FILES += nvddk_se_common_core.c
LOCAL_SRC_FILES += nvddk_se_common_hw.c
LOCAL_SRC_FILES += nvddk_se_rsa.c
LOCAL_SRC_FILES += t1xx_nvddk_se_core.c
LOCAL_SRC_FILES += t1xx_nvddk_se_hw.c

ifneq ($(filter t30,$(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DT30_CHIP=1
else
LOCAL_CFLAGS += -DT30_CHIP=0
endif
ifeq ($(TARGET_USE_NVDUMPER),true)
LOCAL_CFLAGS += -DENABLE_NVDUMPER=1
else
LOCAL_CFLAGS += -DENABLE_NVDUMPER=0
endif

include $(NVIDIA_STATIC_LIBRARY)
