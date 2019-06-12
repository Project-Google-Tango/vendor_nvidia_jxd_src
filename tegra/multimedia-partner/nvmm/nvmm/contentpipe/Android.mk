LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)
include $(LOCAL_PATH)/../Android.common.mk

LOCAL_MODULE := libnvmm_contentpipe

LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/include/openmax/il

LOCAL_SRC_FILES += nvmm_contentpipe.c
LOCAL_SRC_FILES += nvlocalfilecontentpipe.c
LOCAL_SRC_FILES += nvmm_customprotocol.c
LOCAL_SRC_FILES += nvmm_protocol_file.c
LOCAL_SRC_FILES += nvmm_protocol_http.c
LOCAL_SRC_FILES += nvmm_protocol_rtsp.c
LOCAL_SRC_FILES += rtsp.c
LOCAL_SRC_FILES += rtp.c
LOCAL_SRC_FILES += rtp_audio.c
LOCAL_SRC_FILES += rtp_video.c
LOCAL_SRC_FILES += rtp_latm.c
LOCAL_SRC_FILES += rtp_video_h264.c

LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_SHARED_LIBRARIES += libnvrm
LOCAL_SHARED_LIBRARIES += libnvrm_graphics
LOCAL_SHARED_LIBRARIES += libnvmm_utils

#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1
include $(NVIDIA_SHARED_LIBRARY)
include $(call all-makefiles-under,$(LOCAL_PATH))
