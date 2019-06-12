#
# Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#
LOCAL_PATH := $(call my-dir)

LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/customers/nvidia-partner/template/odm_kit/adaptations/pmu
LOCAL_COMMON_C_INCLUDES += $(TEGRA_TOP)/core/system/nvnct

LOCAL_COMMON_SRC_FILES :=  \
        pmu_hal.c \
        pmu_i2c/nvodm_pmu_i2c.c

LOCAL_SRC_FILES_PLUTO := \
        boards/pluto/nvodm_pmu_pluto_rail.c \
        boards/pluto/nvodm_pmu_pluto_battery.c \
        boards/pluto/nvodm_pmu_pluto_dummy.c \
        boards/pluto/odm_pmu_pluto_apgpio_rail.c \
        boards/pluto/odm_pmu_pluto_tps65914_rail.c \
        apgpio/nvodm_pmu_apgpio.c \
        tps65914/nvodm_pmu_tps65914_pmu_driver.c \
        max77665a/nvodm_pmu_max77665a_pmu_driver.c \
        max77665a/nvodm_pmu_max17047_pmu_driver.c

LOCAL_SRC_FILES_DALMORE := \
        boards/dalmore/nvodm_pmu_dalmore_dummy.c \
        boards/dalmore/nvodm_pmu_dalmore_rail.c \
        boards/dalmore/odm_pmu_dalmore_apgpio_rail.c \
        boards/dalmore/odm_pmu_dalmore_max77663_rail.c \
        boards/dalmore/odm_pmu_dalmore_tps51632_rail.c \
        boards/dalmore/odm_pmu_dalmore_tps65090_rail.c \
        boards/dalmore/odm_pmu_dalmore_tps65914_rail.c \
        apgpio/nvodm_pmu_apgpio.c \
        tps65090/nvodm_pmu_tps65090_pmu_driver.c \
        max77663/nvodm_pmu_max77663_pmu_driver.c \
        tps51632/nvodm_pmu_tps51632_pmu_driver.c \
        tps65914/nvodm_pmu_tps65914_pmu_driver.c

LOCAL_SRC_FILES_E1859_T114 := \
        boards/e1859_t114/nvodm_pmu_e1859_t114_dummy.c \
        boards/e1859_t114/nvodm_pmu_e1859_t114_rail.c \
        boards/e1859_t114/odm_pmu_e1859_t114_apgpio_rail.c \
        boards/e1859_t114/odm_pmu_e1859_t114_max77663_rail.c \
        boards/e1859_t114/odm_pmu_e1859_t114_tps51632_rail.c \
        boards/e1859_t114/odm_pmu_e1859_t114_tps65090_rail.c \
        boards/e1859_t114/odm_pmu_e1859_t114_tps65914_rail.c \
        apgpio/nvodm_pmu_apgpio.c \
        tps65090/nvodm_pmu_tps65090_pmu_driver.c \
        max77663/nvodm_pmu_max77663_pmu_driver.c \
        tps51632/nvodm_pmu_tps51632_pmu_driver.c \
        tps65914/nvodm_pmu_tps65914_pmu_driver.c

LOCAL_SRC_FILES_E1852 :=  \
        boards/p1852/nvodm_pmu_p1852_rail.c \
        boards/p1852/odm_pmu_p1852_tps6591x_rail.c \
        apgpio/nvodm_pmu_apgpio.c \
        tps6591x/nvodm_pmu_tps6591x_pmu_driver.c

LOCAL_SRC_FILES_E1853 :=  \
        boards/e1853/nvodm_pmu_e1853_rail.c \
        boards/e1853/odm_pmu_e1853_tps6591x_rail.c \
        apgpio/nvodm_pmu_apgpio.c \
        tps6591x/nvodm_pmu_tps6591x_pmu_driver.c

LOCAL_SRC_FILES_VCM30T30 :=  \
        boards/vcm30t30/nvodm_pmu_vcm30t30_rail.c \
        boards/vcm30t30/odm_pmu_vcm30t30_tps6591x_rail.c \
        apgpio/nvodm_pmu_apgpio.c \
        tps6591x/nvodm_pmu_tps6591x_pmu_driver.c

LOCAL_SRC_FILES_VCM30T114 := \
        boards/vcm30t114/nvodm_pmu_vcm30t114_dummy.c \
        boards/vcm30t114/nvodm_pmu_vcm30t114_rail.c \
        boards/vcm30t114/odm_pmu_vcm30t114_apgpio_rail.c \
        boards/vcm30t114/odm_pmu_vcm30t114_max77663_rail.c \
        boards/vcm30t114/odm_pmu_vcm30t114_tps51632_rail.c \
        boards/vcm30t114/odm_pmu_vcm30t114_tps65090_rail.c \
        boards/vcm30t114/odm_pmu_vcm30t114_tps65914_rail.c \
        apgpio/nvodm_pmu_apgpio.c \
        tps65090/nvodm_pmu_tps65090_pmu_driver.c \
        max77663/nvodm_pmu_max77663_pmu_driver.c \
        tps51632/nvodm_pmu_tps51632_pmu_driver.c \
        tps65914/nvodm_pmu_tps65914_pmu_driver.c

LOCAL_SRC_FILES_M2601 :=  \
        boards/m2601/nvodm_pmu_m2601_rail.c \
        boards/m2601/odm_pmu_m2601_tps6591x_rail.c \
        apgpio/nvodm_pmu_apgpio.c \
        tps6591x/nvodm_pmu_tps6591x_pmu_driver.c

LOCAL_SRC_FILES_ARDBEG := \
        boards/ardbeg/nvodm_pmu_ardbeg_rail.c \
        boards/ardbeg/nvodm_pmu_ardbeg_dummy.c \
        boards/ardbeg/nvodm_pmu_ardbeg_battery.c \
        boards/ardbeg/odm_pmu_ardbeg_apgpio_rail.c \
        boards/ardbeg/odm_pmu_ardbeg_tps80036_rail.c \
        boards/ardbeg/odm_pmu_ardbeg_tps51632_rail.c \
        boards/ardbeg/odm_pmu_ardbeg_as3722_rail.c \
        boards/ardbeg/odm_pmu_ardbeg_tps65914_rail.c \
        apgpio/nvodm_pmu_apgpio.c \
        tps80036/nvodm_pmu_tps80036_pmu_driver.c \
        tps51632/nvodm_pmu_tps51632_pmu_driver.c \
        as3722/nvodm_pmu_as3722_pmu_driver.c \
        tps65914/nvodm_pmu_tps65914_pmu_driver.c \
        max17048/nvodm_pmu_max17048_fuel_gauge_driver.c \
        bq2419x/nvodm_pmu_bq2419x_batterycharger_driver.c \
        bq24z45/nvodm_pmu_bq24z45_fuel_gauge_driver.c \
        bq2477x/nvodm_pmu_bq2477x_batterycharger_driver.c \
        cw2015/nvodm_pmu_cw2015_fuel_gauge_driver.c \
        lc709203f/nvodm_pmu_lc709203f_fuel_gauge_driver.c

LOCAL_SRC_FILES_LOKI := \
        boards/loki/nvodm_pmu_loki_rail.c \
        boards/loki/nvodm_pmu_loki_battery.c \
        boards/loki/nvodm_pmu_loki_dummy.c \
        boards/loki/odm_pmu_loki_apgpio_rail.c \
        boards/loki/odm_pmu_loki_tps65914_rail.c \
        apgpio/nvodm_pmu_apgpio.c \
        tps65914/nvodm_pmu_tps65914_pmu_driver.c \
        bq2419x/nvodm_pmu_bq2419x_batterycharger_driver.c \
        bq27441/nvodm_pmu_bq27441_fuel_gauge_driver.c \
        lc709203f/nvodm_pmu_lc709203f_fuel_gauge_driver.c

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnvodm_pmu
LOCAL_NVIDIA_NO_COVERAGE := true
LOCAL_C_INCLUDES += $(LOCAL_COMMON_C_INCLUDES)
LOCAL_SRC_FILES +=  $(LOCAL_COMMON_SRC_FILES)

ifeq ($(TARGET_USE_NCT),true)
LOCAL_CFLAGS += -DNV_USE_NCT
endif

ifeq ($(TARGET_DEVICE),dalmore)
LOCAL_SRC_FILES += $(LOCAL_SRC_FILES_DALMORE)
LOCAL_CFLAGS += -DNV_TARGET_DALMORE
LOCAL_CFLAGS += -DLPM_BATTERY_CHARGING
LOCAL_STATIC_LIBRARIES += libnvodm_charging
endif
ifeq ($(TARGET_DEVICE),e1859_t114)
LOCAL_SRC_FILES += $(LOCAL_SRC_FILES_E1859_T114)
LOCAL_CFLAGS += -DNV_TARGET_E1859_T114
LOCAL_CFLAGS += -DLPM_BATTERY_CHARGING
LOCAL_STATIC_LIBRARIES += libnvodm_charging
endif
ifeq ($(TARGET_DEVICE),pluto)
LOCAL_SRC_FILES += $(LOCAL_SRC_FILES_PLUTO)
LOCAL_CFLAGS += -DNV_TARGET_PLUTO
endif
ifeq ($(TARGET_DEVICE),p1852)
LOCAL_SRC_FILES += $(LOCAL_SRC_FILES_E1852)
LOCAL_CFLAGS += -DNV_TARGET_P1852
endif
ifeq ($(TARGET_DEVICE),e1853)
LOCAL_SRC_FILES += $(LOCAL_SRC_FILES_E1853)
LOCAL_CFLAGS += -DNV_TARGET_E1853
endif
ifeq ($(TARGET_DEVICE),vcm30t30)
LOCAL_SRC_FILES += $(LOCAL_SRC_FILES_VCM30T30)
LOCAL_CFLAGS += -DNV_TARGET_VCM30T30
endif
ifeq ($(TARGET_DEVICE),vcm30t114)
LOCAL_SRC_FILES += $(LOCAL_SRC_FILES_VCM30T114)
LOCAL_CFLAGS += -DNV_TARGET_VCM30T114
endif
ifeq ($(TARGET_DEVICE),m2601)
LOCAL_SRC_FILES += $(LOCAL_SRC_FILES_M2601)
LOCAL_CFLAGS += -DNV_TARGET_M2601
endif
ifeq ($(TARGET_DEVICE),ardbeg)
LOCAL_SRC_FILES += $(LOCAL_SRC_FILES_ARDBEG)
LOCAL_CFLAGS += -DNV_TARGET_ARDBEG
LOCAL_CFLAGS += -DLPM_BATTERY_CHARGING
LOCAL_CFLAGS += -DREAD_BATTERY_SOC
LOCAL_STATIC_LIBRARIES += libnvodm_charging
endif
ifeq ($(TARGET_DEVICE),loki)
LOCAL_SRC_FILES += $(LOCAL_SRC_FILES_LOKI)
LOCAL_CFLAGS += -DNV_TARGET_LOKI
LOCAL_CFLAGS += -DLPM_BATTERY_CHARGING
LOCAL_CFLAGS += -DREAD_BATTERY_SOC
LOCAL_STATIC_LIBRARIES += libnvodm_charging
endif
#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
LOCAL_CFLAGS += -Wno-missing-field-initializers
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1
include $(NVIDIA_STATIC_LIBRARY)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := libnvodm_pmu_avp
LOCAL_CFLAGS += -Os -ggdb0
LOCAL_C_INCLUDES += $(LOCAL_C_COMMON_INCLUDES)
LOCAL_SRC_FILES +=  $(LOCAL_COMMON_SRC_FILES)

ifeq ($(TARGET_DEVICE),dalmore)
LOCAL_SRC_FILES += $(LOCAL_SRC_FILES_DALMORE)
LOCAL_CFLAGS += -DNV_TARGET_DALMORE
LOCAL_CFLAGS += -DLPM_BATTERY_CHARGING
LOCAL_STATIC_LIBRARIES += libnvodm_charging
endif
ifeq ($(TARGET_DEVICE),e1859_t114)
LOCAL_SRC_FILES += $(LOCAL_SRC_FILES_E1859_T114)
LOCAL_CFLAGS += -DNV_TARGET_E1859_T114
LOCAL_CFLAGS += -DLPM_BATTERY_CHARGING
LOCAL_STATIC_LIBRARIES += libnvodm_charging
endif
ifeq ($(TARGET_DEVICE),vcm30t114)
LOCAL_SRC_FILES += $(LOCAL_SRC_FILES_VCM30T114)
LOCAL_CFLAGS += -DNV_TARGET_VCM30T114
endif
ifeq ($(TARGET_DEVICE),pluto)
LOCAL_SRC_FILES += $(LOCAL_SRC_FILES_PLUTO)
LOCAL_CFLAGS += -DNV_TARGET_PLUTO
endif
ifeq ($(TARGET_DEVICE),ardbeg)
LOCAL_SRC_FILES += $(LOCAL_SRC_FILES_ARDBEG)
LOCAL_CFLAGS += -DNV_TARGET_ARDBEG
LOCAL_CFLAGS += -DLPM_BATTERY_CHARGING
LOCAL_STATIC_LIBRARIES += libnvodm_charging
endif
ifeq ($(TARGET_DEVICE),loki)
LOCAL_SRC_FILES += $(LOCAL_SRC_FILES_LOKI)
LOCAL_CFLAGS += -DNV_TARGET_LOKI
LOCAL_CFLAGS += -DLPM_BATTERY_CHARGING
LOCAL_STATIC_LIBRARIES += libnvodm_charging
endif
include $(NVIDIA_STATIC_AVP_LIBRARY)
