ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_CFLAGS += $(LCDEFS)
LOCAL_CFLAGS += -DGL_GLEXT_PROTOTYPES -DEGL_EGLEXT_PROTOTYPES -DTOSHIBA -DNO_VBLT_ENABLE_SCALING
ifeq ($(NV_LOGGER_ENABLED),1)
LOCAL_CFLAGS += -DNV_LOGGER_ENABLED=1
endif


LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/drivers
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/il
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/il/common
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/include/openmax/il
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/il/components
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/il/components/common
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera/utils/camerautil
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/il/nvmm
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/il/nvmm/common
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/il/nvmm/components
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/il/nvmm/components/common
LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics-partner/android/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics/android/include
LOCAL_C_INCLUDES += $(NV_GPUDRV_SOURCE)/drivers/khronos/egl/interface/partner
LOCAL_C_INCLUDES += $(NV_GPUDRV_SOURCE)/drivers/khronos/interface/partner
LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics/2d/include
LOCAL_C_INCLUDES += $(TOP)/frameworks
LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics/opengles/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics-partner/android/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics/egl/include/nvidia


LOCAL_MODULE := libnvvpp

LOCAL_SRC_FILES += nvxlitevpp.c
LOCAL_SRC_FILES += nvxlitevppcpu.c
LOCAL_SRC_FILES += nvxlitevppegl.c
LOCAL_SRC_FILES += vblt.cpp

LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libnvmm_utils
LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_SHARED_LIBRARIES += libdl
LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_SHARED_LIBRARIES += libnvrm
LOCAL_SHARED_LIBRARIES += libEGL
LOCAL_SHARED_LIBRARIES += libGLESv2
LOCAL_SHARED_LIBRARIES += libnvrm_graphics
LOCAL_SHARED_LIBRARIES += libtsechdcp
LOCAL_SHARED_LIBRARIES += libstagefright_foundation



LOCAL_CFLAGS += -Werror

LOCAL_PRELINK_MODULE := false

include $(NVIDIA_SHARED_LIBRARY)


include $(CLEAR_VARS)

LOCAL_MODULE := InvertY.fs
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/firmware
LOCAL_SRC_FILES := InvertY.fs
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := InvertUV.fs
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/firmware
LOCAL_SRC_FILES := InvertUV.fs
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := InvertY.vs
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/firmware
LOCAL_SRC_FILES := InvertY.vs
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := InvertUV.vs
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)/firmware
LOCAL_SRC_FILES := InvertUV.vs
include $(BUILD_PREBUILT)
endif
