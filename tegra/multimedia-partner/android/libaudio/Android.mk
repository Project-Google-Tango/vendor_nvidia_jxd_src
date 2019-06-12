#
# Copyright (c) 2011-2012, NVIDIA CORPORATION.  All rights reserved.
#

USE_NEW_LIBAUDIO = 1
ifneq ($(USE_NEW_LIBAUDIO), 1)
LOCAL_PATH := $(call my-dir)
ifneq ($(BOARD_VENDOR_USE_NV_AUDIO),false)
ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)
    include $(LOCAL_PATH)/libaudioalsa/Android.mk
endif
endif

else

ifneq ($(strip $(BOARD_USES_GENERIC_AUDIO)),true)
ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

#LOCAL_CFLAGS += -D_POSIX_SOURCE -Wno-multichar

# Now build the libnvaudioservice
include $(NVIDIA_DEFAULTS)

LOCAL_ARM_MODE := arm
LOCAL_CFLAGS += -D_POSIX_SOURCE

LOCAL_MODULE := libnvaudioservice

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libbinder \
    libc

LOCAL_C_INCLUDES += $(TEGRA_INCLUDE)

LOCAL_SRC_FILES := \
    invaudioservice.cpp

include $(NVIDIA_SHARED_LIBRARY)

# Now build the libaudio
include $(NVIDIA_DEFAULTS)

LOCAL_ARM_MODE := arm
#LOCAL_CFLAGS += -DAUDIO_PLAYING_SETPROP
#LOCAL_CFLAGS += -D_POSIX_SOURCE

LOCAL_MODULE := audio.primary.tegra

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libtinyalsa \
    libaudioutils \
    libdl \
    libexpat \
    libbinder \
    libmedia \
    libnvaudioservice \
    libaudioavp \
    libnbaio \
    libnvos

LOCAL_SHARED_LIBRARIES += libnvcapaudioservice

ifeq ($(BOARD_SUPPORT_NVOICE),true)
    LOCAL_SHARED_LIBRARIES += libnvoice
    LOCAL_CFLAGS += -DNVOICE
endif

ifeq ($(BOARD_USES_TFA),true)
    LOCAL_CFLAGS += -DTFA
endif

ifeq ($(BOARD_SUPPORT_NVAUDIOFX),true)
	LOCAL_SHARED_LIBRARIES += libnvaudiofx
	LOCAL_CFLAGS += -DNVAUDIOFX -O3 -fstrict-aliasing -fprefetch-loop-arrays -funsafe-math-optimizations
endif

LOCAL_C_INCLUDES += \
    $(TEGRA_INCLUDE) \
    external/tinyalsa/include \
    external/expat/lib \
    frameworks/av/include \
    frameworks/native/include \
    system/media/audio_utils/include \
    system/media/audio_effects/include

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_CFLAGS += -DTEGRA_TEST_LIBAUDIO
ifeq ($(BOARD_SUPPORT_AUDIO_TUNE), true)
	LOCAL_CFLAGS += -DAUDIO_TUNE
endif

LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1

LOCAL_SRC_FILES := \
    nvaudio_list.c \
    nvaudio_hw.c \
    nvaudio_submix.cpp \
    nvaudio_service.cpp
ifeq ($(BOARD_SUPPORT_AUDIO_TUNE), true)
	LOCAL_SRC_FILES += nvaudio_tune.c
endif

include $(NVIDIA_SHARED_LIBRARY)

#
# Build libaudiopolicy
#
include $(NVIDIA_DEFAULTS)

#LOCAL_CFLAGS += -D_POSIX_SOURCE

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  LOCAL_CFLAGS += -DWITH_A2DP
endif

ifeq ($(BOARD_SUPPORT_NVAUDIOFX),true)
        LOCAL_CFLAGS += -DNVAUDIOFX
endif

LOCAL_SRC_FILES := nvaudiopolicymanager.cpp

LOCAL_MODULE := audio_policy.tegra

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_WHOLE_STATIC_LIBRARIES += libaudiopolicy_legacy \
    libmedia_helper

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    libmedia \
    libbinder \
    libnvaudioservice

#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
LOCAL_CFLAGS += -Wno-error=sign-compare
include $(NVIDIA_SHARED_LIBRARY)

endif
endif
endif
