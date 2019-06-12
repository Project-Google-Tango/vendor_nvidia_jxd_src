LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnvodm_charging

LOCAL_C_INCLUDES += $(TEGRA_TOP)/bootloader/nvbootloader/odm-partner/template/odm_kit/adaptations/charging
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/system/nvaboot

LOCAL_CFLAGS += -DLPM_BATTERY_CHARGING=1

ifeq ($(TARGET_DEVICE),loki)
LOCAL_CFLAGS += -DREAD_BATTERY_SOC=1
endif
ifeq ($(TARGET_DEVICE),ardbeg)
LOCAL_CFLAGS += -DREAD_BATTERY_SOC=1
endif
LOCAL_SRC_FILES := battery-basic.c
LOCAL_SRC_FILES += charging.c
LOCAL_SRC_FILES += charger-usb.c

include $(NVIDIA_STATIC_LIBRARY)
