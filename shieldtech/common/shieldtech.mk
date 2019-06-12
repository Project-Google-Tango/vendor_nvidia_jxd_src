# NVIDIA Tegra4 "Dalmore" development system
#
# Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.


BOARD_USES_SHIELDTECH := true

TARGET_GLOBAL_CPPFLAGS += -DSHIELDTECH
TARGET_GLOBAL_CFLAGS += -DSHIELDTECH

# Add support for Controller menu
PRODUCT_COPY_FILES += \
    vendor/nvidia/shieldtech/common/etc/com.nvidia.shieldtech.xml:system/etc/permissions/com.nvidia.shieldtech.xml


# RSMouse
ifneq ($(SHIELDTECH_FEATURE_RSMOUSE),false)
BOARD_USES_SHIELDTECH_RSMOUSE := true
TARGET_GLOBAL_CPPFLAGS += -DSHIELDTECH_RSMOUSE
TARGET_GLOBAL_CFLAGS += -DSHIELDTECH_RSMOUSE
endif


# Controller-based Keyboard
ifneq ($(SHIELDTECH_FEATURE_KEYBOARD),false)
PRODUCT_PACKAGES += \
  NVLatinIME \
  libjni_nvlatinime
endif

# Full-screen Mode
ifneq ($(SHIELDTECH_FEATURE_FULLSCREEN),false)
endif

# Console Mode
TARGET_GLOBAL_CPPFLAGS += -DSHIELDTECH_CONSOLEMODE
TARGET_GLOBAL_CFLAGS += -DSHIELDTECH_CONSOLEMODE
DEVICE_PACKAGE_OVERLAYS += vendor/nvidia/shieldtech/common/feature_overlays/console_mode
PRODUCT_PACKAGES += \
  ConsoleUI


# Blake controller
PRODUCT_PACKAGES += \
  BlakeController \
  blake \
  lota


# Gallery
PRODUCT_PACKAGES += \
  NVGallery \
  libnvjni_eglfence \
  libnvjni_filtershow_filters \
  libnvjni_mosaic

# Generic ShieldTech Features
DEVICE_PACKAGE_OVERLAYS += vendor/nvidia/shieldtech/common/overlay

