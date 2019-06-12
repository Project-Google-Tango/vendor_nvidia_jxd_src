#
# Copyright (c) 2011, NVIDIA CORPORATION.  All rights reserved.
#

ifneq ($(strip $(BOARD_USES_GENERIC_AUDIO)),true)
ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_CFLAGS += -D_POSIX_SOURCE -Wno-multichar

ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
# Now build the libaudioservice
include $(NVIDIA_DEFAULTS)

LOCAL_ARM_MODE := arm
LOCAL_CFLAGS += -D_POSIX_SOURCE

LOCAL_MODULE := libaudioservice

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils \
	libbinder \
	libc

LOCAL_C_INCLUDES += $(TEGRA_INCLUDE)
LOCAL_C_INCLUDES += external/alsa-lib/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/include

LOCAL_SRC_FILES := \
	invaudioalsaservice.cpp

include $(NVIDIA_SHARED_LIBRARY)
endif

# Now build the libaudio
include $(NVIDIA_DEFAULTS)

LOCAL_ARM_MODE := arm
LOCAL_CFLAGS += -D_POSIX_SOURCE

LOCAL_MODULE := audio.primary.$(TARGET_BOARD_PLATFORM)

LOCAL_SHARED_LIBRARIES := \
	libasound \
	libcutils \
	libutils \
	libmedia \
	libhardware \
	libhardware_legacy \
	libbinder \
	libc

ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
LOCAL_SHARED_LIBRARIES += \
	libaudioservice
endif

LOCAL_STATIC_LIBRARIES += tegra_alsa.tegra

LOCAL_WHOLE_STATIC_LIBRARIES += libaudiohw_legacy \
	libmedia_helper

LOCAL_C_INCLUDES += $(TEGRA_INCLUDE)
LOCAL_C_INCLUDES += external/alsa-lib/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/include

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_CFLAGS += -DTEGRA_TEST_LIBAUDIO

LOCAL_SRC_FILES := \
	nvaudioalsaentry.cpp \
	nvaudioalsadevice.cpp \
	nvaudioalsastreamout.cpp \
	nvaudioalsastreamin.cpp \
	nvaudioalsamixer.cpp \
	nvaudioalsacontrol.cpp \
	nvaudioalsarenderer.cpp \

ifeq ($(NV_ANDROID_FRAMEWORK_ENHANCEMENTS),TRUE)
LOCAL_SRC_FILES += \
	nvaudioalsawfd.cpp \
	nvaudioalsaservice.cpp
endif

#ifeq ($(BOARD_HAVE_BLUETOOTH),true)
#	LOCAL_CFLAGS += -DWITH_BLUETOOTH -DWITH_A2DP
#	LOCAL_SHARED_LIBRARIES += liba2dp
#	LOCAL_C_INCLUDES += $(call include-path-for, bluez)
#endif

#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
include $(NVIDIA_SHARED_LIBRARY)

#
# Build libaudiopolicy
#
include $(NVIDIA_DEFAULTS)

LOCAL_CFLAGS += -D_POSIX_SOURCE

ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  LOCAL_CFLAGS += -DWITH_A2DP
endif

LOCAL_SRC_FILES := nvaudioalsapolicymanager.cpp

LOCAL_MODULE := audio_policy.$(TARGET_BOARD_PLATFORM)

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_WHOLE_STATIC_LIBRARIES += libaudiopolicy_legacy \
	libmedia_helper

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils \
	libmedia

#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
include $(NVIDIA_SHARED_LIBRARY)

endif
endif
