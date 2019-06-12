LOCAL_PATH := $(call my-dir)

ifeq ($(NV_EMBEDDED_BUILD),1)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvsku

LOCAL_CFLAGS += -DNVODM_BOARD_IS_SIMULATION=0
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvbct

LOCAL_SRC_FILES += nvsku_boot_ap20.c
LOCAL_SRC_FILES += nvsku_burn_ap20.c
LOCAL_SRC_FILES += nvsku_boot_t30.c
LOCAL_SRC_FILES += nvsku_burn_t30.c

LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
include $(NVIDIA_STATIC_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnvsku_avp
LOCAL_CFLAGS += -Os -ggdb0
LOCAL_CFLAGS += -DNVODM_BOARD_IS_SIMULATION=0

LOCAL_SRC_FILES += nvsku_boot_ap20.c
LOCAL_SRC_FILES += nvsku_burn_ap20.c
LOCAL_SRC_FILES += nvsku_boot_t30.c
LOCAL_SRC_FILES += nvsku_burn_t30.c
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvbct

include $(NVIDIA_STATIC_AVP_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnvskuhost

LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvbct
LOCAL_CFLAGS += -DNVODM_BOARD_IS_SIMULATION=1

LOCAL_SRC_FILES += nvsku_boot_ap20.c
LOCAL_SRC_FILES += nvsku_burn_ap20.c
LOCAL_SRC_FILES += nvsku_boot_t30.c
LOCAL_SRC_FILES += nvsku_burn_t30.c
include $(NVIDIA_HOST_STATIC_LIBRARY)
endif
