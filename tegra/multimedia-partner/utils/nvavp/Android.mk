LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvavp

ifneq ($(filter $(TARGET_PRODUCT),dolak_sim bonaire_sim),)
LOCAL_C_INCLUDES += $(NVLIB_COMMON_INCLUDES)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/utils/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia/tvmr/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia/tvmr/tvmr/class
endif

ifeq ($(filter $(TARGET_PRODUCT),dolak_sim bonaire_sim),)
LOCAL_SRC_FILES += nvavp.c
else
LOCAL_SRC_FILES += nvavp_sim.c
endif

ifneq ($(filter $(TARGET_PRODUCT),dolak_sim bonaire_sim),)
LOCAL_SHARED_LIBRARIES := \
	libnvos \
	libnvrm \
	libnvtvmr_ucode
else
LOCAL_SHARED_LIBRARIES := \
	libnvos \
	libnvrm
endif
include $(NVIDIA_SHARED_LIBRARY)
