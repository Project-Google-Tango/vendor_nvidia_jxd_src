LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_SRC_FILES :=
LOCAL_SRC_FILES += EglStreamTexture.cpp
LOCAL_SRC_FILES += EglStreamTextureWrapper.cpp
LOCAL_SRC_FILES += EGLStreamConsumer.cpp
LOCAL_SRC_FILES += nvimagescaler.cpp

LOCAL_SHARED_LIBRARIES := \
        libEGL \
        libcutils \
        libutils  \
        libgui   \
        libbinder  \
        libdl \
        libhardware \
        libnvrm \
        libnvrm_graphics \
        libnvos \
        libnvmm_utils \
        libnvddk_2d_v2

LOCAL_C_INCLUDES += $(NV_GPUDRV_SOURCE)/drivers/khronos/interface/partner
LOCAL_C_INCLUDES += $(NV_GPUDRV_SOURCE)/drivers/khronos/egl/interface/partner
LOCAL_C_INCLUDES += $(TOP)/vendor/nvidia/tegra/graphics-partner/android/include
LOCAL_C_INCLUDES += $(TOP)/vendor/nvidia/tegra/graphics/2d/include
LOCAL_C_INCLUDES += $(TOP)/vendor/nvidia/tegra/multimedia-partner/openmax/il/nvmm/common
LOCAL_C_INCLUDES += $(TOP)/vendor/nvidia/tegra/multimedia-partner/openmax/include/openmax/il
LOCAL_C_INCLUDES += $(TOP)/vendor/nvidia/tegra/core/include
LOCAL_C_INCLUDES += $(TOP)/vendor/nvidia/tegra/multimedia-partner/openmax/include/openmax/al

LOCAL_MODULE := libeglstreamtexture

LOCAL_PRELINK_MODULE := false

#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 0
include $(NVIDIA_SHARED_LIBRARY)

include $(CLEAR_VARS)
