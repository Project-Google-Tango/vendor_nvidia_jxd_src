LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := omxplayer2
LOCAL_MODULE_TAGS := nvidia_tests

LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/include/openmax/il
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/include/openmax/ilclient
LOCAL_C_INCLUDES += $(NV_GPUDRV_SOURCE)/drivers/khronos/interface/partner
LOCAL_C_INCLUDES += $(NV_GPUDRV_SOURCE)/drivers/khronos/egl/interface/partner

LOCAL_SRC_FILES += omxplayer2.c

LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_SHARED_LIBRARIES += libnvomxilclient
LOCAL_SHARED_LIBRARIES += libnvwinsys
LOCAL_SHARED_LIBRARIES += libEGL
LOCAL_SHARED_LIBRARIES += libGLESv2

LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/README.full

include $(NVIDIA_EXECUTABLE)
