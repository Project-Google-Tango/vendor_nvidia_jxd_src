LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := hdcp_test
LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include
LOCAL_SRC_FILES += nvhdcp_test.c
LOCAL_STATIC_LIBRARIES += libhdcp_up

include $(NVIDIA_EXECUTABLE)

ifeq ($(BOARD_ENABLE_SECURE_HDCP),1)
    include $(NVIDIA_DEFAULTS)

    LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include
    LOCAL_MODULE := secure_hdcp_test
    LOCAL_MODULE_TAGS := nvidia_tests

    LOCAL_SRC_FILES += nvhdcp_test.c
ifeq ($(SECURE_OS_BUILD),tlk)
    LOCAL_SHARED_LIBRARIES += libtlk_secure_hdcp_up
else
    LOCAL_STATIC_LIBRARIES += libsecure_hdcp_up
    LOCAL_STATIC_LIBRARIES += libtee_client_api_driver
    LOCAL_SHARED_LIBRARIES := libnvrm_graphics
endif
ifneq ($(TARGET_TEGRA_VERSION),t30)
    LOCAL_SHARED_LIBRARIES += libtsechdcp
endif
    LOCAL_SHARED_LIBRARIES += liblog
    LOCAL_SHARED_LIBRARIES += libcutils


    include $(NVIDIA_EXECUTABLE)
endif
