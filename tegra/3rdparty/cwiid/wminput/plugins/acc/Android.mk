LOCAL_PATH:= $(call my-dir)
include $(NVIDIA_DEFAULTS)
LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1

LOCAL_SRC_FILES:= \
        acc.c

LOCAL_SHARED_LIBRARIES := \
        libm  \
        libcwiid

LOCAL_C_INCLUDES:= \
        $(LOCAL_PATH)/../../../wminput \
        $(LOCAL_PATH)/../../../libcwiid \
        $(LOCAL_PATH)/../../../common/include/ \
        external/bluetooth/bluez/lib \
        external/bluetooth/bluez/src

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= acc
include $(NVIDIA_SHARED_LIBRARY)
