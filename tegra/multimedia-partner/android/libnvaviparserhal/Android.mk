ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)
include $(TEGRA_TOP)/multimedia-partner/nvmm/nvmm/Android.common.mk

LOCAL_CFLAGS += $(LCDEFS)
ifeq ($(NV_LOGGER_ENABLED),1)
LOCAL_CFLAGS += -DNV_LOGGER_ENABLED=1
endif
ifeq ($(OFFSET_DISABLED),1)
LOCAL_CFLAGS += -DOFFSET_DISABLED=1
endif

LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/nvmm/nvmm/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/codecs/avi
LOCAL_C_INCLUDES += $(TOP)/frameworks/av/include
LOCAL_C_INCLUDES += $(TOP)/frameworks/av/media/libstagefright/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/include/openmax/il
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/android/libnvparserhalutils

LOCAL_MODULE := libnvaviparserhal

LOCAL_NVIDIA_EXPORTS := libnvaviparserhal.export

LOCAL_SRC_FILES += nvaviparserhal.cpp

LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libnvmm_utils
LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_SHARED_LIBRARIES += libnvrm
LOCAL_SHARED_LIBRARIES += libnvmm
LOCAL_SHARED_LIBRARIES += libstagefright
LOCAL_SHARED_LIBRARIES += libstagefright_foundation
LOCAL_SHARED_LIBRARIES += libdl

LOCAL_STATIC_LIBRARIES += libnvparserhalutils

LOCAL_CFLAGS += -Werror

LOCAL_PRELINK_MODULE := false

include $(NVIDIA_SHARED_LIBRARY)
endif
