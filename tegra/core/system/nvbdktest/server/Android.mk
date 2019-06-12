LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvbdktest_server
LOCAL_NVIDIA_NO_COVERAGE := true

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../framework
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../framework/runner
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../framework/registerer
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../framework/error_handler

LOCAL_CFLAGS += -DNVODM_BOARD_IS_FPGA=0

LOCAL_SRC_FILES += nvbdktest_server.c

LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
include $(NVIDIA_STATIC_LIBRARY)
