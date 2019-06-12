ifeq ($(BUILD_GOOGLETV),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
include $(NVIDIA_DEFAULTS)
LOCAL_C_INCLUDES += bootable/recovery
LOCAL_C_INCLUDES += vendor/tv/frameworks/libs/flash_ts
LOCAL_C_INCLUDES += $(TARGET_C_INCLUDES)
LOCAL_SRC_FILES := bootloader.c crash_counter.c
LOCAL_LDLIBS := -llog
# should match TARGET_RECOVERY_LIBS in BoardConfig.mk
LOCAL_MODULE := librecovery_nv
LOCAL_MODULE_TAGS := optional
include $(BUILD_STATIC_LIBRARY)

ifeq (0,1)
# Recovery ui lib
include $(CLEAR_VARS)
LOCAL_C_INCLUDES += bootable/recovery
LOCAL_SRC_FILES := recovery_ui.c
# should match TARGET_RECOVERY_UI_LIB in BoardConfig.mk
LOCAL_MODULE := librecovery_ui_generic
LOCAL_MODULE_TAGS := optional
include $(BUILD_STATIC_LIBRARY)
endif

endif
