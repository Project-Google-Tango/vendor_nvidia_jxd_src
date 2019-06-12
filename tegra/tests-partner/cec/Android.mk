LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=\
cectest.c

LOCAL_CFLAGS := -O2 -g
LOCAL_CFLAGS += -DHAVE_CONFIG_H -D_U_="__attribute__((unused))"


LOCAL_MODULE_PATH := $(TARGET_OUT_EXECUTABLES)

LOCAL_MODULE_TAGS := nvidia-tests

LOCAL_MODULE := cectest

include $(BUILD_EXECUTABLE)
