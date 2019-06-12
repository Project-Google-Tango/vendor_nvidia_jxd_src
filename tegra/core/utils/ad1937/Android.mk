LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := ad1937

LOCAL_SRC_FILES += main.c
LOCAL_SRC_FILES += ad1937.c
LOCAL_SRC_FILES += board.c
LOCAL_SRC_FILES += i2c-util.c
LOCAL_SHARED_LIBRARIES += libnvrm
LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_CFLAGS += -DANDROID

# Due to bug in bionic lib, https://review.source.android.com/#change,21736
LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef
include $(NVIDIA_EXECUTABLE)
