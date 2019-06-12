LOCAL_PATH:= $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_SRC_FILES:= \
    NvCpuService.cpp \
    TimeoutPoker.cpp \
    Config.cpp \
    NvParseConfig.cpp

LOCAL_CFLAGS += -DLOG_TAG=\"NvCpuService\"

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils \
	libbinder \
	libnvcpud_client

LOCAL_C_INCLUDES += \
       $(call include-path-for, ../../include)

LOCAL_MODULE:= libnvcpud
LOCAL_MODULE_TAGS:= optional

include $(NVIDIA_SHARED_LIBRARY)
