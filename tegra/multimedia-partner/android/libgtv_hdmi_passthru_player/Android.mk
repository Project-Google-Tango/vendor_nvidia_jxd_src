# Copyright 2010 Google Inc.

ifeq ($(BUILD_GOOGLETV),true)

LOCAL_PATH:= $(call my-dir)
#
# libgtv_abstract_hdmi_passthru_player
#
# Abstract implementation of HDMI passthrough player.
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    nvhdmi_passthru_player.cpp

LOCAL_C_INCLUDES := \
    frameworks/base/include \
    frameworks/av/include/media/stagefright/foundation \
    vendor/tv/frameworks/base/include \
    vendor/tv/frameworks/av/include \
    vendor/tv/frameworks/av/libs/mooplayer \
    vendor/tv/frameworks/av/include/media/openmax_1.2 \
    vendor/tv/frameworks/av/libs/resource_manager \
    $(call include-path-for, graphics)

LOCAL_MODULE := libgtv_hdmi_passthru_player
LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libbinder \
    libmedia \
    libui \
    libcutils \
    libbinder \
    libgui \
    libmedia \
    libskia \
    libstagefright \
    libstagefright_foundation \
    libgtv_abstract_hdmi_passthru_player \
    libgtvmediaplayer \
    libmooplayer

include external/stlport/libstlport.mk
include $(BUILD_SHARED_LIBRARY)

endif
