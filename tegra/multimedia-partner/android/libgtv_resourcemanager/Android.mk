ifeq ($(BUILD_GOOGLETV),true)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(TARGET_BUILD_TYPE),debug)
LOCAL_CFLAGS += -DDEBUG
endif

LOCAL_MODULE := libnv_resourcemanager
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
    ResourceManager.cpp

LOCAL_C_INCLUDES := \
    frameworks/base/include \
    frameworks/base/include/utils \
    vendor/tv/frameworks/base/include \
    vendor/tv/frameworks/av/libs/resource_manager

LOCAL_SHARED_LIBRARIES := \
    libbinder \
    libcutils \
    librmclient \
    librmservice \
    libstagefright_foundation \
    libutils \

include $(BUILD_SHARED_LIBRARY)
endif

