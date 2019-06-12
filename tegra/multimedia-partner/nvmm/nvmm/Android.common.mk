NVMM_PATH := $(call my-dir)

NVIDIA_MAKEFILE += $(NVMM_PATH)/Android.common.mk
NVIDIA_MAKEFILE += $(NVMM_PATH)/Makefile.nvmm_enable_defs
NVIDIA_MAKEFILE += $(NVMM_PATH)/Makefile.nvmm_locale_defs

TARGET_OS := linux
LCDEFS :=
include $(NVMM_PATH)/Makefile.nvmm_enable_defs
include $(NVMM_PATH)/Makefile.nvmm_locale_defs
LOCAL_CFLAGS += $(LCDEFS)
ifeq ($(NV_LOGGER_ENABLED),1)
LOCAL_CFLAGS += -DNV_LOGGER_ENABLED=1
endif
ifeq ($(OFFSET_DISABLED),1)
LOCAL_CFLAGS += -DOFFSET_DISABLED=1
endif
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/nvmm/nvmm/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia/codecs/include
# TODO Bug 891531 - Do we need these here for all of nvmm, or can we narrow it down?
LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics/2d/include
