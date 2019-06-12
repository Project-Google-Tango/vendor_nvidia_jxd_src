LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvmm

LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/nvmm/nvmm/include
LOCAL_SRC_FILES += nvmm.c

LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_SHARED_LIBRARIES += libnvrm
LOCAL_SHARED_LIBRARIES += libnvrm_graphics
LOCAL_SHARED_LIBRARIES += libnvmm_utils
LOCAL_STATIC_LIBRARIES += libnvmmtransport

include $(NVIDIA_SHARED_LIBRARY)
-include $(addprefix $(LOCAL_PATH)/, $(addsuffix /Android.mk, blocks utils contentpipe transport))

