LOCAL_PATH:= $(call my-dir)

#
# nvaudio_test Test App
#

include $(NVIDIA_DEFAULTS)

LOCAL_CFLAGS += -DLOG_TAG=\"NvAudioTest\"

LOCAL_MODULE := nvaudio_test

LOCAL_SHARED_LIBRARIES += libutils
LOCAL_SHARED_LIBRARIES += libbinder
LOCAL_SHARED_LIBRARIES += libmedia
LOCAL_SHARED_LIBRARIES += libtinyalsa
LOCAL_SHARED_LIBRARIES += liblog

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/android/libaudio/
LOCAL_C_INCLUDES += external/tinyalsa/include

LOCAL_SRC_FILES:=        \
 nvaudio_test.cpp

include $(NVIDIA_EXECUTABLE)
