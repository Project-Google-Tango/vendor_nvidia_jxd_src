LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvbdktest_framework
LOCAL_NVIDIA_NO_COVERAGE := true

LOCAL_C_INCLUDES += $(LOCAL_PATH)/error_handler
LOCAL_C_INCLUDES += $(LOCAL_PATH)/registerer
LOCAL_C_INCLUDES += $(LOCAL_PATH)/runner
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../testsources

LOCAL_SRC_FILES += error_handler/error_handler.c
LOCAL_SRC_FILES += registerer/registerer.c
LOCAL_SRC_FILES += runner/runner.c

LOCAL_CFLAGS += -DNVODM_BOARD_IS_FPGA=0
ifneq ($(filter t124, $(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DNVBDKTEST_T124
endif

LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
include $(NVIDIA_STATIC_LIBRARY)
