LOCAL_PATH:= $(call my-dir)
include $(NVIDIA_DEFAULTS)
LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1

LOCAL_SRC_FILES:= \
          bluetooth.c command.c connect.c interface.c process.c state.c \
          thread.c util.c


LOCAL_SHARED_LIBRARIES := \
        libbluetooth \
        libglib


LOCAL_C_INCLUDES:= \
        external/bluetooth/bluez/lib \
        external/bluetooth/bluez/src

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= libcwiid
include $(NVIDIA_SHARED_LIBRARY)
