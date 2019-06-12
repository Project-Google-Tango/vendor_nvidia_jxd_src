LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvaes_ref
LOCAL_SRC_FILES += aes_ref.c

include $(NVIDIA_HOST_STATIC_LIBRARY)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvaes_ref
LOCAL_CFLAGS += -Os -ggdb0
LOCAL_SRC_FILES += aes_ref.c

LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
include $(NVIDIA_STATIC_LIBRARY)


