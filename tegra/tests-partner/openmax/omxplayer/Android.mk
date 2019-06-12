# We're not allowed to use ../ in LOCAL_SRC_FILES
LOCAL_PATH := $(call my-dir)/..

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := omxplayer
LOCAL_MODULE_TAGS := nvidia_tests

LOCAL_C_INCLUDES += $(TEGRA_TOP)/tests-partner/openmax/common
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/include/openmax/il
LOCAL_C_INCLUDES += $(NV_GPUDRV_SOURCE)/drivers/khronos/interface/partner
LOCAL_C_INCLUDES += $(NV_GPUDRV_SOURCE)/drivers/khronos/egl/interface/partner

LOCAL_CFLAGS += -D__OMX_EXPORTS
LOCAL_CFLAGS += -DOMXVERSION=2
LOCAL_CFLAGS += -DUSE_ANDROID_WINSYS=1

LOCAL_SRC_FILES += omxplayer/omxplayer.c
LOCAL_SRC_FILES += omxplayer/main_omxplayer.c
LOCAL_SRC_FILES += omxplayer/samplecomponent.c
LOCAL_SRC_FILES += omxplayer/sampleprotocol.c
LOCAL_SRC_FILES += omxplayer/nvhttppost.c
LOCAL_SRC_FILES += common/samplebase.c
LOCAL_SRC_FILES += common/omxtestlib.c
LOCAL_SRC_FILES += common/samplewindow_winsys.c

LOCAL_STATIC_LIBRARIES += libnvfxmath
LOCAL_STATIC_LIBRARIES += libnvappmain
LOCAL_STATIC_LIBRARIES += libnvtestmain

LOCAL_SHARED_LIBRARIES += libnvtestio
LOCAL_SHARED_LIBRARIES += libnvtestresults

LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_SHARED_LIBRARIES += libnvmm_utils
LOCAL_SHARED_LIBRARIES += libnvomx
LOCAL_SHARED_LIBRARIES += libnvwinsys

#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1

include $(NVIDIA_SHARED_LIBRARY)
